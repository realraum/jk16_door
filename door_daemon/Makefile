##
##  door_daemon
##
##  Copyright (C) 2009 Christian Pointner <equinox@spreadspace.org>
##
##  This file is part of door_daemon.
##
##  door_daemon is free software: you can redistribute it and/or modify
##  it under the terms of the GNU General Public License version 3 as
##  published by the Free Software Foundation.
##
##  door_daemon is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##  GNU General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with door_daemon. If not, see <http://www.gnu.org/licenses/>.
##

ifneq ($(MAKECMDGOALS),distclean)
include include.mk
endif

EXECUTABLE := door_daemon

OBJ := log.o \
      sig_handler.o \
      door_daemon.o


SRC := $(OBJ:%.o=%.c)

.PHONY: clean distclean

all: $(EXECUTABLE)

%.d: %.c
	@set -e; rm -f $@; \
	 $(CC) -MM $(CFLAGS) $< > $@.$$$$; \
   sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
   rm -f $@.$$$$; echo '(re)building $@'

ifneq ($(MAKECMDGOALS),distclean)
-include $(SRC:%.c=%.d)
endif

door_daemon: $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<


distclean: clean
	find . -name *.o -exec rm -f {} \;
	find . -name "*.\~*" -exec rm -rf {} \;
	rm -f include.mk
	rm -f tun.c

clean:
	rm -f *.o
	rm -f *.d
	rm -f *.d.*
	rm -f $(EXECUTABLE)

