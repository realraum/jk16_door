/*
 *  door_daemon
 *
 *  Copyright (C) 2009 Christian Pointner <equinox@spreadspace.org>
 *
 *  This file is part of door_daemon.
 *
 *  door_daemon is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  door_daemon is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with door_daemon. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#include "string_list.h"

struct options_struct {
  char* progname_;
  int daemonize_;
  char* username_;
  char* groupname_;
  char* chroot_dir_;
  char* pid_file_;
  string_list_t log_targets_;

  char* door_dev_;
  char* command_sock_;
};
typedef struct options_struct options_t;

int options_parse_hex_string(const char* hex, buffer_t* buffer);

int options_parse(options_t* opt, int argc, char* argv[]);
void options_parse_post(options_t* opt);
void options_default(options_t* opt);
void options_clear(options_t* opt);
void options_print_usage();
void options_print(options_t* opt);

#endif
