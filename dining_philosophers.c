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
#include <math.h>
#include <assert.h>
#include <sys/types.h>
#include <time.h>

#include "mutl.h"

#define	DELAY_CNT	8000000
#define	NFORKS		5
#define	NROUNDS		3

mutl_mutex_t *furca[NFORKS];

/* Simple counter of philosophers eating in the same time. */
int eating = 0;

/*
 * Wait for 1 to 6 seconds.
 * We don't use sleep(3C) (or alarm(2) or so) because it may  interfere with
 * the signal used by the mutl library. Also we want to keep our
 * process running on the CPU so other our "threads" can be run.
 */
void
delay(void)
{
	time_t start_time = time(NULL);
	int d = random() % 5 + 1;

	while (time(NULL) - start_time < d) {
		for (int b = 0; b < DELAY_CNT; b++) {
			/* just to burn some cycles ... */
			double x = b * 0.1;
			double y = cos(x);
			if (x == y)
				break;
		}
	}
}

void
philosopher(void *a)
{
	int id = (int)a;
	int left = id - 1;
	int right = (left + 1) % NFORKS;

	for (int c = 0; c < NROUNDS; c++) {
		delay();
		printf("philosopher %d is hungry\n", id);

		mutl_mutex_enter(furca[left]);
		mutl_mutex_enter(furca[right]);

		/*
		 * For simplicity we assume that this is atomic operation (which
		 * is probably not correct).
		 */
		eating++;

		printf("philosopher %d is eating with %d & %d\n", id, left,
		    right);
		delay();
		printf("philosopher %d is done\n", id);

		eating--;
		mutl_mutex_exit(furca[left]);
		mutl_mutex_exit(furca[right]);
	}
	printf("philospher %d end\n", id);

	mutl_exit(0);
}

int
main(int argc, char **argv)
{
	int prev_eating = 0;
	int i;

	srandom(time(NULL));
	for (i = 0; i < NFORKS; i++)
		furca[i] = mutl_mutex_init();

	mutl_init();

	for (i = 0; i < NFORKS; i++) {
		int res = mutl_new_thread(philosopher, (void *)(i + 1));
		assert(res != -1);
	}

	while (mutl_nthreads() > 1) {
		if (prev_eating != eating) {
			prev_eating = eating;
			printf("%d philosophers are eating right now.\n",
			    eating);
		}
		mutl_yield();
	}

	printf("All philosophers are done.\n");

	for (i = 0; i < NFORKS; i++)
		mutl_mutex_fini(furca[i]);

	return (0);
}
