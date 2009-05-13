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

#include <termios.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/un.h>

#include "log.h"
#include "sig_handler.h"
#include "options.h"

#include "command_queue.h"
#include "client_list.h"

#include "daemon.h"
#include "sysexec.h"

int init_command_socket(const char* path)
{
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(fd < 0) {
    log_printf(ERROR, "unable to open socket: %s", strerror(errno));
    return -1;
  }

  struct sockaddr_un local;
  local.sun_family = AF_UNIX;
  strcpy(local.sun_path, path); // TODO: strlen ???
  unlink(local.sun_path);
  int len = SUN_LEN(&local);
  int ret = bind(fd, (struct sockaddr*)&local, len);
  if(ret) {
    log_printf(ERROR, "unable to bind to '%s': %s", local.sun_path, strerror(errno));
    return -1;
  }
  
  ret = listen(fd, 4);
  if(ret) {
    log_printf(ERROR, "unable to listen on command socket: %s", local.sun_path, strerror(errno));
    return -1;
  }

  log_printf(INFO, "now listening on %s for incoming commands", path);
  
  return fd;
}

int send_command(int door_fd, cmd_t* cmd)
{
  if(!cmd)
    return -1;
  
  char c;
  switch(cmd->cmd) {
  case OPEN: c = 'o'; break;
  case CLOSE: c = 'c'; break;
  case TOGGLE: c = 't'; break;
  case STATUS: c = 's'; break;
  case RESET: c = 'r'; break;
  case LOG: return 0;
  }
  
  int ret;
  do {
    ret = write(door_fd, &c, 1);
  } while(!ret || (ret == -1 && errno == EINTR));

  if(ret > 0) {
    cmd->sent = 1;
    return 0;
  }

  return ret;
}

int send_response(int fd, const char* response)
{
  if(!response)
    return -1;

  int len = strlen(response);
  int offset = 0;
  int ret;
  for(;;) {
    ret = write(fd, &response[offset], strlen(response));
    if(ret < 0) {
      if(errno != EINTR)
        return ret;

      ret = 0;
    }

    offset += ret;
    if(offset+1 >= len)
      break;
  }
  do {
    ret = write(fd, "\n", 1);
  } while(!ret || (ret == -1 && errno == EINTR));

  if(ret > 0)
    return 0;

  return ret;
}

int handle_command(const char* cmd, int fd, cmd_t** cmd_q, client_t* client_lst)
{
  if(!cmd_q || !cmd)
    return -1;
  
  cmd_id_t cmd_id;
  if(!strncmp(cmd, "open", 4))
    cmd_id = OPEN;
  else if(!strncmp(cmd, "close", 5))
    cmd_id = CLOSE;
  else if(!strncmp(cmd, "toggle", 6))
    cmd_id = TOGGLE;
  else if(!strncmp(cmd, "reset", 5))
    cmd_id = RESET;
  else if(!strncmp(cmd, "status", 6))
    cmd_id = STATUS;
  else if(!strncmp(cmd, "log", 3))
    cmd_id = LOG;
  else if(!strncmp(cmd, "listen", 6)) {
    client_t* listener = client_find(client_lst, fd);
    if(listener) {
      log_printf(DEBUG, "adding status listener %d", fd);
      listener->status_listener = 1;
    }
    else       
      log_printf(ERROR, "unable to add status listener %d", fd);

    return 0;
  }
  else {
    log_printf(WARNING, "unknown command '%s'", cmd);
    return 1;
  }
  char* param = strchr(cmd, ' ');
  if(param) 
    param++;

  switch(cmd_id) {
  case OPEN:
  case CLOSE:
  case TOGGLE:
  case STATUS:
  case RESET: {
    int ret = cmd_push(cmd_q, fd, cmd_id, param);
    if(ret)
      return ret;

    log_printf(NOTICE, "command: %s", cmd); 
    break;
  }
  case LOG: {
    if(param && param[0])
      log_printf(NOTICE, "ext msg: %s", param); 
    else
      log_printf(DEBUG, "ignoring empty ext log message");
    break;
  }
  }
  
  return 0;
}

int process_cmd(int fd, cmd_t **cmd_q, client_t* client_lst)
{
  log_printf(DEBUG, "processing command from %d", fd);

  static char buffer[100];
  int ret = 0;
  do { // TODO: replace this whith a actually working readline
    memset(buffer, 0, 100);
    ret = recv(fd, buffer, sizeof(buffer), 0);
    if(!ret)
      return 1;
    if(ret < 0)
      return ret;

    char* saveptr;
    char* tok = strtok_r(buffer, "\n\r", &saveptr);
    do {
      if(!tok)
        continue;

      ret = handle_command(tok, fd, cmd_q, client_lst);
      if(ret < 0)
        return ret;
    } while(tok = strtok_r(NULL, "\n\r", &saveptr));
  } while (ret == -1 && errno == EINTR);
    
  return 0;
}

int process_door(int door_fd, cmd_t **cmd_q, client_t* client_lst)
{
  log_printf(DEBUG, "processing data from door (fd=%d)", door_fd);

  static char buffer[100];
  int ret = 0;
  do { // TODO: replace this whith a actually working readline
    memset(buffer, 0, 100);
    ret = read(door_fd, buffer, sizeof(buffer));
    if(!ret) 
      return 2;
    if(ret < 0)
      return ret;

    char* saveptr;
    char* tok = strtok_r(buffer, "\n\r", &saveptr);
    do {
      if(!tok)
        continue;

      log_printf(NOTICE, "door-firmware: %s", tok);

      int cmd_fd = -1;
      if(cmd_q && (*cmd_q)) {
        cmd_fd = (*cmd_q)->fd;
        send_response(cmd_fd, tok);
      }

      if(!strncmp(tok, "Status:", 7)) {
        client_t* client;
        for(client = client_lst; client; client = client->next)
          if(client->status_listener && client->fd != cmd_fd)
            send_response(client->fd, tok);
      }

      cmd_pop(cmd_q);
    } while(tok = strtok_r(NULL, "\n\r", &saveptr));
  } while (ret == -1 && errno == EINTR);

  return 0;
}


int main_loop(int door_fd, int cmd_listen_fd)
{
  log_printf(INFO, "entering main loop");

  fd_set readfds, tmpfds;
  FD_ZERO(&readfds);
  FD_SET(door_fd, &readfds);
  FD_SET(cmd_listen_fd, &readfds);
  int max_fd = door_fd > cmd_listen_fd ? door_fd : cmd_listen_fd;
  cmd_t* cmd_q = NULL;
  client_t* client_lst = NULL;

  int sig_fd = signal_init();
  if(sig_fd < 0)
    return -1;
  FD_SET(sig_fd, &readfds);
  max_fd = (max_fd < sig_fd) ? sig_fd : max_fd;

  int return_value = 0;
  while(!return_value) {
    memcpy(&tmpfds, &readfds, sizeof(tmpfds));

    int ret = select(max_fd+1, &tmpfds, NULL, NULL, NULL);
    if(ret == -1 && errno != EINTR) {
      log_printf(ERROR, "select returned with error: %s", strerror(errno));
      return_value = -1;
      break;
    }
    if(!ret || ret == -1)
      continue;

    if(signal_handle()) {
      return_value = 1;
      break;
    }

    if(FD_ISSET(door_fd, &tmpfds)) {
      return_value = process_door(door_fd, &cmd_q, client_lst);
      if(return_value)
        break;
    }

    if(FD_ISSET(cmd_listen_fd, &tmpfds)) {
      int new_fd = accept(cmd_listen_fd, NULL, NULL);
      if(new_fd < 0) {
        log_printf(ERROR, "accept returned with error: %s", strerror(errno));
        return_value = -1;
        break;
      }  
      log_printf(DEBUG, "new command connection (fd=%d)", new_fd);
      FD_SET(new_fd, &readfds);
      max_fd = (max_fd < new_fd) ? new_fd : max_fd;
      client_add(&client_lst, new_fd);
    }

    client_t* lst = client_lst;
    while(lst) {
      if(FD_ISSET(lst->fd, &tmpfds)) {
        return_value = process_cmd(lst->fd, &cmd_q, client_lst); 
        if(return_value == 1) {
          log_printf(DEBUG, "removing closed command connection (fd=%d)", lst->fd);
          client_t* deletee = lst;
          lst = lst->next;
          FD_CLR(deletee->fd, &readfds);
          client_remove(&client_lst, deletee->fd);
          return_value = 0;
          continue;
        }
        if(return_value)
          break;

      }
      lst = lst->next;
    }

    if(cmd_q && !cmd_q->sent)
      send_command(door_fd, cmd_q);
  }

  cmd_clear(&cmd_q);
  client_clear(&client_lst);
  signal_stop();
  return return_value;
}

int setup_tty(int fd)
{
  struct termios tmio;
  
  int ret = tcgetattr(fd, &tmio);
  if(ret) {
    log_printf(ERROR, "Error on tcgetattr(): %s", strerror(errno));
    return ret;
  }

  ret = cfsetospeed(&tmio, B9600);
  if(ret) {
    log_printf(ERROR, "Error on cfsetospeed(): %s", strerror(errno));
    return ret;
  }

  ret = cfsetispeed(&tmio, B9600);
  if(ret) {
    log_printf(ERROR, "Error on cfsetispeed(): %s", strerror(errno));
    return ret;
  }

  tmio.c_lflag &= ~ECHO;

  ret = tcsetattr(fd, TCSANOW, &tmio);
  if(ret) {
    log_printf(ERROR, "Error on tcsetattr(): %s", strerror(errno));
    return ret;
  }
  
  ret = tcflush(fd, TCIFLUSH);
  if(ret) {
    log_printf(ERROR, "Error on tcflush(): %s", strerror(errno));
    return ret;
  }

  fd_set fds;
  struct timeval tv;
  FD_ZERO(&fds);
  FD_SET(fd, &fds);
  tv.tv_sec = 0;
  tv.tv_usec = 50000;
  for(;;) {
    ret = select(fd+1, &fds, NULL, NULL, &tv);
    if(ret > 0) {
      char buffer[100];
      ret = read(fd, buffer, sizeof(buffer));
    }
    else
      break;
  }

  return 0;
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
      fprintf(stderr, "memory error on options_parse, exiting\n");
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

  int door_fd = open(opt.door_dev_, O_RDWR | O_NOCTTY);
  if(door_fd < 0) {
    log_printf(ERROR, "unable to open %s: %s", opt.door_dev_, strerror(errno));
    options_clear(&opt);
    log_close();
    exit(-1);
  }
  ret = setup_tty(door_fd);
  if(ret) {
    close(door_fd);
    options_clear(&opt);
    log_close();
    exit(-1);
  }
  

  int cmd_listen_fd = init_command_socket(opt.command_sock_);
  if(cmd_listen_fd < 0) {
    close(door_fd);
    options_clear(&opt);
    log_close();
    exit(-1);
  }

  ret = main_loop(door_fd, cmd_listen_fd);

  close(cmd_listen_fd);
  close(door_fd);

  if(!ret)
    log_printf(NOTICE, "normal shutdown");
  else if(ret < 0)
    log_printf(NOTICE, "shutdown after error");
  else if(ret == 2)
    log_printf(ERROR, "shutdown after %s read error", opt.door_dev_);
  else
    log_printf(NOTICE, "shutdown after signal");

  options_clear(&opt);
  log_close();

  return ret;
}
