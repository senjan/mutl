#
# My Userland Thread Library
# https://github.com/senjan/mutl
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

CC=gcc
CFLAGS=-Wall -g3 -std=gnu99

all:	libmutl.so dining_philosophers

libmutl.so:	mutl.o
	$(CC) -shared -o $@ $?

mutl.o:	mutl.c mutl_impl.h
	$(CC) -c -fpic $(CFLAGS) mutl.c

dining_philosophers: dining_philosophers.c
	$(CC) -o $@ $(CFLAGS) dining_philosophers.c -lmutl -lm -L.

clean:
	rm -f *.o libmutl.so dining_philosophers
