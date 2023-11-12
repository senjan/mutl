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

#ifndef mutl_h__
#define	mutl_h__ 

typedef void (*thread_func_t)(void *);

typedef struct mutl_mutex {
	int owner;
	int nwaiters;
} mutl_mutex_t;

extern void mutl_init(void);
extern int mutl_new_thread(thread_func_t func, void *arg);
void mutl_exit(int code);
void mutl_yield(void);
int mutl_nthreads(void);

mutl_mutex_t *mutl_mutex_init(void);
void mutl_mutex_fini(mutl_mutex_t *mtx);
void mutl_mutex_enter(mutl_mutex_t *mtx);
void mutl_mutex_exit(mutl_mutex_t *mtx);

#endif /* mutl_h__ */
