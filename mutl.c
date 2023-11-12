/*
 * My Userland Thread Library
 * https://github.com/senjan/mutl
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <string.h>
#if defined __NetBSD__
#include <sys/time.h>
#else
#include <alloca.h>
#endif
#include <unistd.h>

#if defined __APPLE__ || defined __osf__
#include <sys/time.h>
#endif
#include <assert.h>

#include "mutl_impl.h"

#define	VERBOSE_DEBUG	0

static mutl_thread_t threads[MAX_NTHREADS];
static int cur_thread = 0;
static int nthreads = 0;
static sigset_t interrupt;

void timer_handler(int signum);

void
disable_int(void)
{
	sigprocmask(SIG_BLOCK, &interrupt, NULL);
}

void
enable_int(void)
{
	sigprocmask(SIG_UNBLOCK, &interrupt, NULL);
}

mutl_mutex_t *
mutl_mutex_init(void)
{
	mutl_mutex_t *m = malloc(sizeof (mutl_mutex_t));
	m->owner = -1;
	return (m);
}

void
mutl_mutex_fini(mutl_mutex_t *mtx)
{
	assert(mtx->owner == -1);
	free(mtx);
}

void
mutl_mutex_exit(mutl_mutex_t *mtx)
{
	disable_int();
	assert(mtx->owner == cur_thread);
	mtx->owner = -1;
	enable_int();
}

void
mutl_init(void)
{
	struct sigaction sa = { 0 };
	struct itimerval timer;
	int i;

	if (nthreads > 0) {
		/* Already initialized */
		return;
	}
	nthreads = 1;

	for (i = 0; i < MAX_NTHREADS; i++) {
		threads[i].state = DONE;
		threads[i].arg = threads[i].func = NULL;
		threads[i].exit_code = -1;
		threads[i].waiting = NULL;
	}

	/* Thread #0 is the main thread which is currently running */
	threads[0].state = RUNNING;

	/* ITIMER_VIRTUAL delivers SIGVTALRM */
	sigemptyset(&interrupt);
	sigaddset(&interrupt, SIGVTALRM);

	/*
	 * Set timer which will call the scheduler periodically.
	 * We actually never leave the signal handler so we must not defer
	 * the signal.
	 */
	sa.sa_flags = SA_NODEFER;
	sa.sa_handler = &timer_handler;
	sigaction(SIGVTALRM, &sa, NULL);

	timer.it_value.tv_sec = SCHEDQ_S;
	timer.it_value.tv_usec = SCHEDQ_MS * 1000;
	timer.it_interval.tv_sec = SCHEDQ_S;
	timer.it_interval.tv_usec = SCHEDQ_MS * 1000;

	setitimer(ITIMER_VIRTUAL, &timer, NULL);
}

static void
start_thread(int ti)
{
	char *reserv = alloca(STACK_SIZE * ti);
	char next = 'A';	/* stack starts after this mark */

#if VERBOSE_DEBUG
	printf("%s: %d, stack %p\n", __func__, ti, &next);
#endif
	cur_thread = ti;
	threads[ti].state = RUNNING;
	(threads[ti].func)(threads[ti].arg);
}

#if VERBOSE_DEBUG
static char
thread_status(thread_state_t s)
{
	switch (s) {
	case NEW:
		return ('N');
	case READY:
		return ('r');
	case RUNNING:
		return ('R');
	case DONE:
		return ('D');
	case WAITING:
		return ('W');
	}
	return ('-');
}
#endif

static void
scheduler(sched_reason_t reason)
{
	int ti, i;
	int ret = 0;
#if VERBOSE_DEBUG
	printf("%s: cur=%d, reason=%d: ", __func__, cur_thread, reason);
#endif
	disable_int();

	if (reason == EXIT) {
		assert(threads[cur_thread].state == RUNNING);

		threads[cur_thread].state = DONE;
		nthreads--;
#if VERBOSE_DEBUG
		printf("%s: thread %d exited: %d\n", __func__, cur_thread,
		    threads[cur_thread].exit_code);
#endif
	} else {
		if (reason == WAIT) {
			assert(threads[cur_thread].waiting != NULL);
			threads[cur_thread].state = WAITING;
		} else {
			threads[cur_thread].state = READY;
		}
		enable_int();
		ret = setjmp(threads[cur_thread].context);
	}
	if (ret != 0) {	/* resuming */
		enable_int();
		return;
	}

	ti = cur_thread + 1;
	for (i = 0; i < MAX_NTHREADS; i++, ti++) {
		ti %= MAX_NTHREADS;
		mutl_thread_t *ct = &threads[ti];

#if VERBOSE_DEBUG
		putchar(thread_status(ct->state));
#endif
		if (ct->state == NEW) {
			/* Start new thread */
			start_thread(ti);
			threads[ti].state = DONE;	/* thread ended */
			continue;
		}
		if (ct->state == READY) {
			/* Resume thread */
			cur_thread = ti;
#if VERBOSE_DEBUG
			putchar('\n');
#endif
			threads[ti].state = RUNNING;
			enable_int();
			longjmp(threads[ti].context, 1);
			assert(0);
		}
		if (ct->state == WAITING) {
			/* Resume thread which used to wait */
			assert(ct->waiting != NULL);
			if (ct->waiting->owner == -1) {
#if VERBOSE_DEBUG
				printf("resume %d for mtx %p\n", ti,
				    ct->waiting);
#endif
				ct->waiting = NULL;
				cur_thread = ti;
				ct->state = RUNNING;
				enable_int();
				longjmp(threads[ti].context, 1);
				assert(0);
			}
		}
	}
	enable_int();
}

void
timer_handler(int signum)
{
	fflush(stdout);
	scheduler(QELAPSED);
}

void
mutl_mutex_enter(mutl_mutex_t *mtx)
{
	disable_int();
	while (1) {
		if (mtx->owner == -1) {
#if VERBOSE_DEBUG
			printf("%s: thread %d got mtx %p\n", __func__,
			    cur_thread, mtx);
#endif
			mtx->owner = cur_thread;
			assert(mtx->owner == cur_thread);
			enable_int();
			break;
		}
		assert(threads[cur_thread].waiting == NULL);
		threads[cur_thread].waiting = mtx;
		enable_int();
		scheduler(WAIT);
	}
}

int
mutl_new_thread(thread_func_t func, void *arg)
{
	int i;

	if (nthreads == 0)
		return (-2);	/* library not initilized */

	for (i = 0; i < MAX_NTHREADS; i++) {
		if (threads[i].state != DONE)
			continue;
		nthreads++;
#if VERBOSE_DEBUG
		printf("new thread %d: %p - %p\n", i, func, arg);
#endif
		threads[i].func = func;
		threads[i].arg = arg;
		threads[i].exit_code = -1;
		threads[i].state = NEW;
		return (i);
	}

	/* Too many threads, no slot available */
	return (-1);
}

void
mutl_exit(int code)
{
	threads[cur_thread].exit_code = code;
	scheduler(EXIT);

	assert(0);
}

void
mutl_yield(void)
{
	scheduler(YIELD);
}

int
mutl_nthreads(void)
{
	assert(nthreads >= 0);
	assert(nthreads <= MAX_NTHREADS);
	return (nthreads);
}
