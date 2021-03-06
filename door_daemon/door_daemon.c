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

int init_command_socket(const char* path)
{
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(fd < 0) {
    log_printf(ERROR, "unable to open socket: %s", strerror(errno));
    return -1;
  }

  struct sockaddr_un local;
  local.sun_family = AF_UNIX;
  if(sizeof(local.sun_path) <= strlen(path)) {
    log_printf(ERROR, "socket path is to long (max %d)", sizeof(local.sun_path)-1);
    return -1;
  }
  strcpy(local.sun_path, path);
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
    cmd_sent(cmd);
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
    ret = write(fd, &response[offset], len - offset);
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

int process_cmd(const char* cmd, int fd, cmd_t **cmd_q, client_t* client_lst)
{
  log_printf(DEBUG, "processing command from %d", fd);

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
    cmd_id = LISTEN;
  }
  else {
    log_printf(WARNING, "unknown command '%s'", cmd);
    return 0;
  }
  char* param = strchr(cmd, ' ');
  if(param) 
    param++;

  if(cmd_id == OPEN || cmd_id == CLOSE || cmd_id == TOGGLE) {
    char* resp;
    asprintf(&resp, "Request: %s", cmd);
    if(resp) {
      char* linefeed = strchr(resp, '\n');
      if(linefeed) linefeed[0] = 0;
      client_t* client;
      int listener_cnt = 0;
      for(client = client_lst; client; client = client->next)
        if(client->request_listener && client->fd != fd) {
          send_response(client->fd, resp);
          listener_cnt++;
        }
      free(resp);
      log_printf(DEBUG, "sent request to %d additional listeners", listener_cnt);
    }
// else silently ignore memory alloc error
  }

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
  case LISTEN: {
    client_t* listener = client_find(client_lst, fd);
    if(listener) {
      if(!param) {
        listener->status_listener = 1;
        listener->error_listener = 1;      
        listener->request_listener = 1;
      }
      else {
        if(!strncmp(param, "status", 6))
          listener->status_listener = 1;
        else if(!strncmp(param, "error", 5))
          listener->error_listener = 1;      
        else if(!strncmp(param, "request", 7))
          listener->request_listener = 1;
        else {
          log_printf(DEBUG, "unkown listener type '%s'", param);
          break;
        }
      }
      log_printf(DEBUG, "listener %d requests %s messages", fd, param ? param:"all");
    }
    else {
      log_printf(ERROR, "unable to add listener %d", fd);
    }
    break;
  }
  }
  
  return 0;
}

int nonblock_recvline(read_buffer_t* buffer, int fd, cmd_t** cmd_q, client_t* client_lst)
{
  int ret = 0;
  for(;;) {
    ret = recv(fd, &buffer->buf[buffer->offset], 1, 0);
    if(!ret)
      return 2;
    if(ret == -1 && errno == EAGAIN)
      return 0;
    else if(ret < 0)
      break;

    if(buffer->buf[buffer->offset] == '\n') {
      buffer->buf[buffer->offset] = 0;
      ret = process_cmd(buffer->buf, fd, cmd_q, client_lst);
      buffer->offset = 0;
      break;
    }

    buffer->offset++;
    if(buffer->offset >= sizeof(buffer->buf)) {
      log_printf(DEBUG, "string too long (fd=%d)", fd);
      buffer->offset = 0;
      return 0;
    }
  }

  return ret;
}

int process_door(read_buffer_t* buffer, int door_fd, cmd_t **cmd_q, client_t* client_lst)
{
  int ret = 0;
  struct timeval tv;
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(door_fd, &fds);

  for(;;) {
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    ret = select(door_fd+1, &fds, NULL, NULL, &tv);
    if(!ret)
      return 0;
    else if(ret < 0)
      return ret;

    ret = read(door_fd, &buffer->buf[buffer->offset], 1);
    if(!ret)
      return 2;
    if(ret == -1 && errno == EAGAIN)
      return 0;
    else if(ret < 0)
      break;

    if(buffer->buf[buffer->offset] == '\n') {
      buffer->buf[buffer->offset] = 0;

      if(buffer->offset > 0 && buffer->buf[buffer->offset-1] == '\r')
        buffer->buf[buffer->offset-1] = 0;

      log_printf(NOTICE, "door-firmware: %s", buffer->buf);      

      int cmd_fd = -1;
      if(cmd_q && (*cmd_q)) {
        cmd_fd = (*cmd_q)->fd;
        send_response(cmd_fd, buffer->buf);
      }
      
      if(!strncmp(buffer->buf, "Status:", 7)) {
        client_t* client;
        int listener_cnt = 0;
        for(client = client_lst; client; client = client->next)
          if(client->status_listener && client->fd != cmd_fd) {
            send_response(client->fd, buffer->buf);
            listener_cnt++;
          }
        log_printf(DEBUG, "sent status to %d additional listeners", listener_cnt);
      }

      if(!strncmp(buffer->buf, "Error:", 6)) {
        client_t* client;
        int listener_cnt = 0;
        for(client = client_lst; client; client = client->next)
          if(client->error_listener && client->fd != cmd_fd) {
            send_response(client->fd, buffer->buf);
            listener_cnt++;
          }
        log_printf(DEBUG, "sent error to %d additional listeners", listener_cnt);
      }
      
      cmd_pop(cmd_q);
      buffer->offset = 0;
      return 0;
    }

    buffer->offset++;
    if(buffer->offset >= sizeof(buffer->buf)) {
      log_printf(DEBUG, "string too long (fd=%d)", door_fd);
      buffer->offset = 0;
      return 0;
    }
  }

  return ret;
}

int main_loop(int door_fd, int cmd_listen_fd)
{
  log_printf(NOTICE, "entering main loop");

  fd_set readfds, tmpfds;
  FD_ZERO(&readfds);
  FD_SET(door_fd, &readfds);
  FD_SET(cmd_listen_fd, &readfds);
  int max_fd = door_fd > cmd_listen_fd ? door_fd : cmd_listen_fd;
  cmd_t* cmd_q = NULL;
  client_t* client_lst = NULL;

  read_buffer_t door_buffer;
  door_buffer.offset = 0;

  int sig_fd = signal_init();
  if(sig_fd < 0)
    return -1;
  FD_SET(sig_fd, &readfds);
  max_fd = (max_fd < sig_fd) ? sig_fd : max_fd;

  struct timeval timeout;
  int return_value = 0;
  while(!return_value) {
    memcpy(&tmpfds, &readfds, sizeof(tmpfds));

    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;
    int ret = select(max_fd+1, &tmpfds, NULL, NULL, &timeout);
    if(ret == -1 && errno != EINTR) {
      log_printf(ERROR, "select returned with error: %s", strerror(errno));
      return_value = -1;
      break;
    }
    if(ret == -1)
      continue;
    if(!ret) {
      if(cmd_q && cmd_has_expired(*cmd_q)) {
        log_printf(ERROR, "last command expired");
        cmd_pop(&cmd_q);
      }
      else
        continue;
    }

    if(FD_ISSET(sig_fd, &tmpfds)) {
      if(signal_handle()) {
        return_value = 1;
        break;
      }
    }
   
    if(FD_ISSET(door_fd, &tmpfds)) {
      return_value = process_door(&door_buffer, door_fd, &cmd_q, client_lst);
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
      fcntl(new_fd, F_SETFL, O_NONBLOCK);
      client_add(&client_lst, new_fd);
    }

    client_t* lst = client_lst;
    while(lst) {
      if(FD_ISSET(lst->fd, &tmpfds)) {
        return_value = nonblock_recvline(&(lst->buffer), lst->fd, &cmd_q, client_lst);
        if(return_value == 2) {
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
      if(lst)
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

  int cmd_listen_fd = init_command_socket(opt.command_sock_);
  if(cmd_listen_fd < 0) {
    options_clear(&opt);
    log_close();
    exit(-1);
  }
  
  int door_fd = 0;
  for(;;) {
    door_fd = open(opt.door_dev_, O_RDWR | O_NOCTTY);
    if(door_fd < 0)
      ret = 2;
    else {
      ret = setup_tty(door_fd);
      if(ret)
        ret = 2;
      else
        ret = main_loop(door_fd, cmd_listen_fd);
    }

    if(ret == 2) {
      log_printf(ERROR, "%s error, trying to reopen in 5 seconds..", opt.door_dev_);
      if(door_fd > 0)
        close(door_fd);
      sleep(5);
    }
    else
      break;
  }

  close(cmd_listen_fd);
  if(door_fd > 0)
    close(door_fd);

  if(!ret)
    log_printf(NOTICE, "normal shutdown");
  else if(ret < 0)
    log_printf(NOTICE, "shutdown after error");
  else
    log_printf(NOTICE, "shutdown after signal");

  options_clear(&opt);
  log_close();

  return ret;
}
