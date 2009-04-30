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

#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#include "string_list.h"

struct ifconfig_param_struct {
  char* net_addr_;
  u_int16_t prefix_length_;
};
typedef struct ifconfig_param_struct ifconfig_param_t;

enum resolv_addr_type_enum { ANY, IPV4_ONLY, IPV6_ONLY };
typedef enum resolv_addr_type_enum resolv_addr_type_t;

enum role_enum  { ROLE_LEFT, ROLE_RIGHT };
typedef enum role_enum role_t;

struct options_struct {
  char* progname_;
  int daemonize_;
  char* username_;
  char* groupname_;
  char* chroot_dir_;
  char* pid_file_;
  string_list_t log_targets_;
  char* local_addr_;
  char* local_port_;
  sender_id_t sender_id_;
  char* remote_addr_;
  char* remote_port_;
  resolv_addr_type_t resolv_addr_type_;
  char* dev_name_;
  char* dev_type_;
  ifconfig_param_t ifconfig_param_;
  char* post_up_script_;
  mux_t mux_;
  window_size_t seq_window_size_;
  char* cipher_;
#ifndef NO_CRYPT
  char* kd_prf_;
  char* auth_algo_;
  char* passphrase_;
  role_t role_;
#endif
  u_int32_t auth_tag_length_;
  buffer_t key_;
  buffer_t salt_;
};
typedef struct options_struct options_t;

int options_parse_hex_string(const char* hex, buffer_t* buffer);
int options_parse_ifconfig(const char* arg, ifconfig_param_t* ifcfg);

int options_parse(options_t* opt, int argc, char* argv[]);
void options_parse_post(options_t* opt);
void options_default(options_t* opt);
void options_clear(options_t* opt);
void options_print_usage();
void options_print(options_t* opt);

#endif
