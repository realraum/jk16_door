/*
 *  uAnytun
 *
 *  uAnytun is a tiny implementation of SATP. Unlike Anytun which is a full
 *  featured implementation uAnytun has no support for multiple connections
 *  or synchronisation. It is a small single threaded implementation intended
 *  to act as a client on small platforms.
 *  The secure anycast tunneling protocol (satp) defines a protocol used
 *  for communication between any combination of unicast and anycast
 *  tunnel endpoints.  It has less protocol overhead than IPSec in Tunnel
 *  mode and allows tunneling of every ETHER TYPE protocol (e.g.
 *  ethernet, ip, arp ...). satp directly includes cryptography and
 *  message authentication based on the methodes used by SRTP.  It is
 *  intended to deliver a generic, scaleable and secure solution for
 *  tunneling and relaying of packets of any protocol.
 *  
 *
 *  Copyright (C) 2007-2008 Christian Pointner <equinox@anytun.org>
 *
 *  This file is part of uAnytun.
 *
 *  uAnytun is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 *
 *  uAnytun is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with uAnytun. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SYSEXEC_H_
#define _SYSEXEC_H_

int exec_script(const char* script, const char* ifname)
{
  if(!script || !ifname)
    return -1;

  pid_t pid;
  pid = fork();
  if(!pid) {
    int fd;
    for (fd=getdtablesize();fd>=0;--fd) // close all file descriptors
      close(fd);

    fd = open("/dev/null",O_RDWR);        // stdin
    if(fd == -1)
      log_printf(WARNING,  "can't open stdin");
    else {
      if(dup(fd) == -1)   // stdout
        log_printf(WARNING,  "can't open stdout");
      if(dup(fd) == -1)   // stderr
        log_printf(WARNING,  "can't open stderr");
    }
    execl("/bin/sh", "/bin/sh", script, ifname, NULL);
        // if execl return, an error occurred
    log_printf(ERROR, "error on executing script: %s", strerror(errno));
    return -1;
  }
  int status = 0;
  waitpid(pid, &status, 0);
  if(WIFEXITED(status))
    log_printf(NOTICE, "script '%s' returned %d", script, WEXITSTATUS(status));  
  else if(WIFSIGNALED(status))
    log_printf(NOTICE, "script '%s' terminated after signal %d", script, WTERMSIG(status));
  else
    log_printf(ERROR, "executing script: unkown error");

  return status;

}

#endif
