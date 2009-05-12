/*
 *  door_daemon
 *
 *  Copyright (C) 2009 Christian Pointner <equinox@spreadspace.org>
 *
 *  This file is part of door_daemon.
 *
 *  door_daemon is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 *
 *  door_daemon is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with door_daemon. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _CLIENT_LIST_H_
#define _CLIENT_LIST_H_

struct client_struct {
  int fd;
  int status_listener;
  struct client_struct* next;
};
typedef struct client_struct client_t;

int client_add(client_t** first, int fd);
void client_remove(client_t** first, int fd);
client_t* client_find(client_t* first, int fd);
void client_clear(client_t** first);

#endif
