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

#include "datatypes.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/un.h>

#include "log.h"
#include "sig_handler.h"
#include "options.h"

#include "daemon.h"
#include "sysexec.h"

int process_cmd(int fd)
{
  log_printf(INFO, "processing command from %d", fd);

  char* buffer[100];
  int ret = 0;
  do {
    ret = recv(fd, buffer, sizeof(buffer), 0);
    if(!ret) return 1;
  } while (ret == -1 && errno == EINTR);
    
  return 0;
}

int process_ttyusb(int ttyusb_fd)
{
  log_printf(INFO, "processing data from ttyusb (fd=%d)", ttyusb_fd);

  char* buffer[100];
  int ret = 0;
  do {
    ret = read(ttyusb_fd, buffer, sizeof(buffer));
    if(!ret) return -1;
  } while (ret == -1 && errno == EINTR);

  return 0;
}


int main_loop(int ttyusb_fd, int cmd_listen_fd)
{
  log_printf(INFO, "entering main loop");

  fd_set readfds, tmpfds;
  FD_ZERO(&readfds);
  FD_SET(ttyusb_fd, &readfds);
  FD_SET(cmd_listen_fd, &readfds);
  int max_fd = ttyusb_fd > cmd_listen_fd ? ttyusb_fd : cmd_listen_fd;

  signal_init();
  int return_value = 0;
  while(!return_value) {
    memcpy(&tmpfds, &readfds, sizeof(tmpfds));

    int ret = select(max_fd+1, &tmpfds, NULL, NULL, NULL);
    if(ret == -1 && errno != EINTR) {
      log_printf(ERROR, "select returned with error: %s", strerror(errno));
      return_value = -1;
      break;
    }
    if(!ret)
      continue;

    if(signal_exit) {
      return_value = 1;
      break;
    }

    if(FD_ISSET(ttyusb_fd, &tmpfds)) {
      return_value = process_ttyusb(ttyusb_fd);
      if(return_value)
        break;

      FD_CLR(ttyusb_fd, &tmpfds);
    }

    if(FD_ISSET(cmd_listen_fd, &tmpfds)) {
      int new_fd = accept(cmd_listen_fd, NULL, NULL);
      if(new_fd < 0) {
        log_printf(ERROR, "accept returned with error: %s", strerror(errno));
        return_value = -1;
        break;
      }  
      log_printf(INFO, "new command connection (fd=%d)", new_fd);
      FD_SET(new_fd, &readfds);
      max_fd = (max_fd < new_fd) ? new_fd : max_fd;
      FD_CLR(cmd_listen_fd, &tmpfds);
    }

    int fd;
    for(fd = 0; fd <= max_fd; fd++) {
      if(FD_ISSET(fd, &tmpfds)) {
        return_value = process_cmd(fd); 
        if(return_value == 1) {
          close(fd);
          FD_CLR(fd, &readfds);
          return_value = 0;
        }
        if(return_value)
          break;
      }
    }
  }

  return return_value;
}

int main(int argc, char* argv[])
{
  log_init();

  options_t opt;
  int ret = options_parse(&opt, argc, argv);
  if(ret) {
    if(ret > 0) {
      fprintf(stderr, "syntax error near: %s\n\n", argv[ret]);
    }
    if(ret == -2) {
      fprintf(stderr, "memory error on options_parse, exitting\n");
    }

    if(ret != -2)
      options_print_usage();

    options_clear(&opt);
    log_close();
    exit(ret);
  }
  string_list_element_t* tmp = opt.log_targets_.first_;
  if(!tmp) {
    log_add_target("syslog:3,door_daemon,daemon");
  }
  else {
    while(tmp) {
      ret = log_add_target(tmp->string_);
      if(ret) {
        switch(ret) {
        case -2: fprintf(stderr, "memory error on log_add_target, exitting\n"); break;
        case -3: fprintf(stderr, "unknown log target: '%s', exitting\n", tmp->string_); break;
        case -4: fprintf(stderr, "this log target is only allowed once: '%s', exitting\n", tmp->string_); break;
        default: fprintf(stderr, "syntax error near: '%s', exitting\n", tmp->string_); break;
        }
        
        options_clear(&opt);
        log_close();
        exit(ret);
      }
      tmp = tmp->next_;
    }
  }
  log_printf(NOTICE, "just started...");
  options_parse_post(&opt);

  priv_info_t priv;
  if(opt.username_)
    if(priv_init(&priv, opt.username_, opt.groupname_)) {
      options_clear(&opt);
      log_close();
      exit(-1);
    }

  int ttyusb_fd = open(opt.ttyusb_dev_, O_RDWR);
  if(ttyusb_fd < 0) {
    log_printf(ERROR, "unable to open %s: %s", opt.ttyusb_dev_, strerror(errno));
    options_clear(&opt);
    log_close();
    exit(-1);
  }

  int cmd_listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un local;
  int len;
  if(cmd_listen_fd < 0) {
    log_printf(ERROR, "unable to open socket: %s", strerror(errno));
    close(ttyusb_fd);
    options_clear(&opt);
    log_close();
    exit(-1);
  }
  local.sun_family = AF_UNIX;
  strcpy(local.sun_path, opt.command_sock_); // TODO: strlen ???
  unlink(local.sun_path);
  len = SUN_LEN(&local);
  ret = bind(cmd_listen_fd, (struct sockaddr*)&local, len);
  if(ret) {
    log_printf(ERROR, "unable to bind to '%s': %s", local.sun_path, strerror(errno));
    close(cmd_listen_fd);
    close(ttyusb_fd);
    options_clear(&opt);
    log_close();
    exit(-1);
  }

  ret = listen(cmd_listen_fd, 4);
  if(ret) {
    log_printf(ERROR, "unable to listen on command socket: %s", local.sun_path, strerror(errno));
    close(cmd_listen_fd);
    close(ttyusb_fd);
    options_clear(&opt);
    log_close();
    exit(-1);
  }
  
  FILE* pid_file = NULL;
  if(opt.pid_file_) {
    pid_file = fopen(opt.pid_file_, "w");
    if(!pid_file) {
      log_printf(WARNING, "unable to open pid file: %s", strerror(errno));
    }
  }

  if(opt.chroot_dir_)
    if(do_chroot(opt.chroot_dir_)) {
      options_clear(&opt);
      log_close();
      exit(-1);
    }
  if(opt.username_)
    if(priv_drop(&priv)) {
      options_clear(&opt);
      log_close();
      exit(-1);
    }

  if(opt.daemonize_) {
    pid_t oldpid = getpid();
    daemonize();
    log_printf(INFO, "running in background now (old pid: %d)", oldpid);
  }

  if(pid_file) {
    pid_t pid = getpid();
    fprintf(pid_file, "%d", pid);
    fclose(pid_file);
  }

  ret = main_loop(ttyusb_fd, cmd_listen_fd);

  close(cmd_listen_fd);
  close(ttyusb_fd);
  options_clear(&opt);

  if(!ret)
    log_printf(NOTICE, "normal shutdown");
  else if(ret < 0)
    log_printf(NOTICE, "shutdown after error");
  else
    log_printf(NOTICE, "shutdown after signal");

  log_close();

  return ret;
}
