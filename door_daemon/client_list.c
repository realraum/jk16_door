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

#include <stdlib.h>

#include "client_list.h"
#include "datatypes.h"

client_t* client_get_last(client_t* first)
{
  if(!first) 
    return NULL;

  while(first->next) {
    first = first->next;
  }

  return first;
}

int client_add(client_t** first, int fd)
{
  if(!first)
    return -1;

  client_t* new_client = malloc(sizeof(client_t));
  if(!new_client)
    return -2;

  new_client->fd = fd;
  new_client->status_listener = 0;
  new_client->next = NULL;

  if(!(*first)) {
    *first = new_client;
    return 0;
  }
    
  client_get_last(*first)->next = new_client;

  return 0;
}

void client_remove(client_t** first, int fd)
{
  if(!first || !(*first)) 
    return;

  client_t* deletee = *first;
  if((*first)->fd == fd) {
    *first = (*first)->next;
    close(deletee->fd);
    free(deletee);
    return;
  }

  client_t* prev = deletee;
  deletee = deletee->next;
  while(deletee) {
    if(deletee->fd == fd) {
      prev->next = deletee->next;
      close(deletee->fd);
      free(deletee);
      return;
    }
    prev = deletee;
    deletee = deletee->next;
  }
}

client_t* client_find(client_t* first, int fd)
{
  if(!first)
    return NULL;
  
  while(first) {
    if(first->fd == fd)
      return first;

    first = first->next;
  }
  return NULL;
}

void client_clear(client_t** first)
{
  if(!first || !(*first)) 
    return;

  while(*first) {
    client_t* deletee = *first;
    *first = (*first)->next;
    close(deletee->fd);
    free(deletee);
  }
}
