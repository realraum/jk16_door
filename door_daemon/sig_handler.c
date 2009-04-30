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

#include "datatypes.h"

#include "log.h"
#include <signal.h>
#include "sig_handler.h"

volatile sig_atomic_t signal_exit = 0;

void signal_init()
{
  signal(SIGINT, handle_signal);
  signal(SIGQUIT, handle_signal);
  signal(SIGTERM, handle_signal);
  signal(SIGHUP, handle_signal);
  signal(SIGUSR1, handle_signal);
  signal(SIGUSR2, handle_signal);
}

void handle_signal(int sig)
{
  switch(sig) {
  case SIGINT: log_printf(NOTICE, "SIG-Int caught, exitting"); signal_exit = 1; break;
  case SIGQUIT: log_printf(NOTICE, "SIG-Quit caught, exitting"); signal_exit = 1; break;
  case SIGTERM: log_printf(NOTICE, "SIG-Term caught, exitting"); signal_exit = 1; break;
  case SIGHUP: log_printf(NOTICE, "SIG-Hup caught"); break;
  case SIGUSR1: log_printf(NOTICE, "SIG-Usr1 caught"); break;
  case SIGUSR2: log_printf(NOTICE, "SIG-Usr2 caught"); break;
  default: log_printf(NOTICE, "Signal %d caught, ignoring", sig); break;
  }
}
