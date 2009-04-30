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

#include "options.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "log.h"

#ifndef NO_CRYPT
#include "auth_algo.h"
#endif

#define PARSE_BOOL_PARAM(SHORT, LONG, VALUE)             \
    else if(!strcmp(str,SHORT) || !strcmp(str,LONG))     \
      VALUE = 1;

#define PARSE_INVERSE_BOOL_PARAM(SHORT, LONG, VALUE)     \
    else if(!strcmp(str,SHORT) || !strcmp(str,LONG))     \
      VALUE = 0;

#define PARSE_INT_PARAM(SHORT, LONG, VALUE)              \
    else if(!strcmp(str,SHORT) || !strcmp(str,LONG))     \
    {                                                    \
      if(argc < 1)                                       \
        return i;                                        \
      VALUE = atoi(argv[i+1]);                           \
      argc--;                                            \
      i++;                                               \
    }

#define PARSE_STRING_PARAM(SHORT, LONG, VALUE)           \
    else if(!strcmp(str,SHORT) || !strcmp(str,LONG))     \
    {                                                    \
      if(argc < 1 || argv[i+1][0] == '-')                \
        return i;                                        \
      if(VALUE) free(VALUE);                             \
      VALUE = strdup(argv[i+1]);                         \
      if(!VALUE)                                         \
        return -2;                                       \
      argc--;                                            \
      i++;                                               \
    }

#define PARSE_STRING_PARAM_SEC(SHORT, LONG, VALUE)       \
    else if(!strcmp(str,SHORT) || !strcmp(str,LONG))     \
    {                                                    \
      if(argc < 1 || argv[i+1][0] == '-')                \
        return i;                                        \
      if(VALUE) free(VALUE);                             \
      VALUE = strdup(argv[i+1]);                         \
      if(!VALUE)                                         \
        return -2;                                       \
      size_t j;                                          \
      for(j=0; j < strlen(argv[i+1]); ++j)               \
        argv[i+1][j] = '#';                              \
      argc--;                                            \
      i++;                                               \
    }

#define PARSE_IFCONFIG_PARAM(SHORT, LONG, VALUE)         \
    else if(!strcmp(str,SHORT) || !strcmp(str,LONG))     \
    {                                                    \
      if(argc < 1 || argv[i+1][0] == '-')                \
        return i;                                        \
      int ret;                                           \
      ret = options_parse_ifconfig(argv[i+1], &VALUE);   \
      if(ret > 0)                                        \
        return i+1;                                      \
      if(ret < 0)                                        \
        return ret;                                      \
      argc--;                                            \
      i++;                                               \
    }

#define PARSE_HEXSTRING_PARAM_SEC(SHORT, LONG, VALUE)    \
    else if(!strcmp(str,SHORT) || !strcmp(str,LONG))     \
    {                                                    \
      if(argc < 1 || argv[i+1][0] == '-')                \
        return i;                                        \
      int ret;                                           \
      ret = options_parse_hex_string(argv[i+1], &VALUE); \
      if(ret > 0)                                        \
        return i+1;                                      \
      else if(ret < 0)                                   \
        return ret;                                      \
      size_t j;                                          \
      for(j=0; j < strlen(argv[i+1]); ++j)               \
        argv[i+1][j] = '#';                              \
      argc--;                                            \
      i++;                                               \
    }

#define PARSE_STRING_LIST(SHORT, LONG, LIST)             \
    else if(!strcmp(str,SHORT) || !strcmp(str,LONG))     \
    {                                                    \
      if(argc < 1 || argv[i+1][0] == '-')                \
        return i;                                        \
      int ret = string_list_add(&LIST, argv[i+1]);       \
      if(ret == -2)                                      \
        return ret;                                      \
      else if(ret)                                       \
        return i+1;                                      \
      argc--;                                            \
      i++;                                               \
    }

int options_parse_hex_string(const char* hex, buffer_t* buffer)
{
  if(!hex || !buffer)
    return -1;

  u_int32_t hex_len = strlen(hex);
  if(hex_len%2)
    return 1;

  if(buffer->buf_) 
    free(buffer->buf_);
  
  buffer->length_ = hex_len/2;
  buffer->buf_ = malloc(buffer->length_);
  if(!buffer->buf_) {
    buffer->length_ = 0;
    return -2;
  }

  const char* ptr = hex;
  int i;
  for(i=0;i<buffer->length_;++i) {
    u_int32_t tmp;
    sscanf(ptr, "%2X", &tmp);
    buffer->buf_[i] = (u_int8_t)tmp;
    ptr += 2;
  }

  return 0;
}

int options_parse_ifconfig(const char* arg, ifconfig_param_t* ifcfg)
{
  char* str = strdup(arg);
  if(!str)
    return -2;

  char* ptr = str;
  for(;*ptr;++ptr) {
    if(*ptr == '/') {
      *ptr = 0;
      ptr++;
      if(!(*ptr)) {
        free(str);
        return 1;
      }
      
      ifcfg->prefix_length_ = atoi(ptr);
      ifcfg->net_addr_ = strdup(str);
      free(str);

      if(!ifcfg->net_addr_)
        return -2;

      return 0;
    }
    if(!isdigit(*ptr) && *ptr != '.') {
      free(str);
      return 1;
    }
  }

  free(str);
  return 1;
}


int options_parse(options_t* opt, int argc, char* argv[])
{
  if(!opt)
    return -1;

  options_default(opt);

  if(opt->progname_)
    free(opt->progname_);
  opt->progname_ = strdup(argv[0]);
  if(!opt->progname_)
    return -2;

  argc--;

  char* role = NULL;
  int i, ipv4_only = 0, ipv6_only = 0;
  for(i=1; argc > 0; ++i)
  {
    char* str = argv[i];
    argc--;

    if(!strcmp(str,"-h") || !strcmp(str,"--help"))
      return -1;
    PARSE_INVERSE_BOOL_PARAM("-D","--nodaemonize", opt->daemonize_)
    PARSE_STRING_PARAM("-u","--username", opt->username_)
    PARSE_STRING_PARAM("-g","--groupname", opt->groupname_)
    PARSE_STRING_PARAM("-C","--chroot", opt->chroot_dir_)
    PARSE_STRING_PARAM("-P","--write-pid", opt->pid_file_)
    PARSE_STRING_PARAM("-i","--interface", opt->local_addr_)
    PARSE_STRING_PARAM("-p","--port", opt->local_port_)
    PARSE_INT_PARAM("-s","--sender-id", opt->sender_id_)
    PARSE_STRING_LIST("-L","--log", opt->log_targets_)
    PARSE_STRING_PARAM("-r","--remote-host", opt->remote_addr_)
    PARSE_STRING_PARAM("-o","--remote-port", opt->remote_port_)
    PARSE_BOOL_PARAM("-4","--ipv4-only", ipv4_only)
    PARSE_BOOL_PARAM("-6","--ipv6-only", ipv6_only)
    PARSE_STRING_PARAM("-d","--dev", opt->dev_name_)
    PARSE_STRING_PARAM("-t","--type", opt->dev_type_)
    PARSE_IFCONFIG_PARAM("-n","--ifconfig", opt->ifconfig_param_)
    PARSE_STRING_PARAM("-x","--post-up-script", opt->post_up_script_)
    PARSE_INT_PARAM("-m","--mux", opt->mux_)
    PARSE_INT_PARAM("-w","--window-size", opt->seq_window_size_)
#ifndef NO_CRYPT
    PARSE_STRING_PARAM("-k","--kd-prf", opt->kd_prf_)
#ifndef NO_PASSPHRASE
    PARSE_STRING_PARAM_SEC("-E","--passphrase", opt->passphrase_)
#endif
    PARSE_STRING_PARAM("-e","--role", role)
    PARSE_HEXSTRING_PARAM_SEC("-K","--key", opt->key_)
    PARSE_HEXSTRING_PARAM_SEC("-A","--salt", opt->salt_)
    PARSE_STRING_PARAM("-c","--cipher", opt->cipher_)
    PARSE_STRING_PARAM("-a","--auth-algo", opt->auth_algo_)
    PARSE_INT_PARAM("-b","--auth-tag-length", opt->auth_tag_length_)
#endif
    else 
      return i;
  }
  if(ipv4_only && ipv6_only)
    return -3;
  if(ipv4_only)
    opt->resolv_addr_type_ = IPV4_ONLY;
  if(ipv6_only)
    opt->resolv_addr_type_ = IPV6_ONLY;

  if(role) {
    if(!strcmp(role, "alice") || !strcmp(role, "server") || !strcmp(role, "left"))
      opt->role_ = ROLE_LEFT;
    else if(!strcmp(role, "bob") || !strcmp(role, "client") || !strcmp(role, "right"))
      opt->role_ = ROLE_RIGHT;
    else {
      free(role);
      return -4;
    }
    free(role);
  }
  return 0;
}

void options_parse_post(options_t* opt)
{
  if(!opt)
    return;

#ifdef NO_V4MAPPED
  if(opt->resolv_addr_type_ == ANY) {
    opt->resolv_addr_type_ = IPV4_ONLY;
    log_printf(WARNING, "No support for V4-mapped Adresses on this platform, defaulting to only use IPv4 addresses");
  }
#endif

#ifndef NO_CRYPT
  if(!strcmp(opt->cipher_, "null") && !strcmp(opt->auth_algo_, "null") && 
     strcmp(opt->kd_prf_, "null")) {
    if(opt->kd_prf_)
      free(opt->kd_prf_);
    opt->kd_prf_ = strdup("null");
  }
  if((strcmp(opt->cipher_, "null") || strcmp(opt->auth_algo_, "null")) && 
     !strcmp(opt->kd_prf_, "null")) {
    log_printf(WARNING, "using NULL key derivation with encryption and or authentication enabled!");
  }

  u_int32_t tag_len_max = auth_algo_get_max_length(opt->auth_algo_);
  if(!tag_len_max) opt->auth_tag_length_ = 0;
  else if(tag_len_max < opt->auth_tag_length_) {
    log_printf(WARNING, "%s auth algo can't generate tags of length %d, using maximum tag length(%d)", opt->auth_algo_, opt->auth_tag_length_, tag_len_max);
    opt->auth_tag_length_ = tag_len_max;
  }
#endif

  if(!(opt->dev_name_) && !(opt->dev_type_))
    opt->dev_type_ = strdup("tun");
}

void options_default(options_t* opt)
{
  if(!opt)
    return;

  opt->progname_ = strdup("uanytun");
  opt->daemonize_ = 1;
  opt->username_ = NULL;
  opt->groupname_ = NULL;
  opt->chroot_dir_ = NULL;
  opt->pid_file_ = NULL;
  string_list_init(&opt->log_targets_);
  opt->local_addr_ = NULL;
  opt->local_port_ = strdup("4444");
  opt->sender_id_ = 0;
  opt->remote_addr_ = NULL;
  opt->remote_port_ = strdup("4444");
  opt->resolv_addr_type_ = ANY;
  opt->dev_name_ = NULL;
  opt->dev_type_ = NULL;
  opt->ifconfig_param_.net_addr_ = NULL;
  opt->ifconfig_param_.prefix_length_ = 0;
  opt->post_up_script_ = NULL;
  opt->mux_ = 0;
  opt->seq_window_size_ = 0;
#ifndef NO_CRYPT
  opt->kd_prf_ = strdup("aes-ctr");
  opt->passphrase_ = NULL;
  opt->role_ = ROLE_LEFT;
  opt->cipher_ = strdup("aes-ctr");
  opt->auth_algo_ = strdup("sha1");
  opt->auth_tag_length_ = 10;
#else
  opt->cipher_ = strdup("null");
  opt->auth_tag_length_ = 0;
#endif
  opt->key_.buf_ = NULL;
  opt->key_.length_ = 0;
  opt->salt_.buf_ = NULL;
  opt->salt_.length_ = 0;
}

void options_clear(options_t* opt)
{
  if(!opt)
    return;

  if(opt->progname_)
    free(opt->progname_);
  if(opt->username_)
    free(opt->username_);
  if(opt->groupname_)
    free(opt->groupname_);
  if(opt->chroot_dir_)
    free(opt->chroot_dir_);
  if(opt->pid_file_)
    free(opt->pid_file_);
  string_list_clear(&opt->log_targets_);
  if(opt->local_addr_)
    free(opt->local_addr_);
  if(opt->local_port_)
    free(opt->local_port_);
  if(opt->remote_addr_)
    free(opt->remote_addr_);
  if(opt->remote_port_)
    free(opt->remote_port_);
  if(opt->dev_name_)
    free(opt->dev_name_);
  if(opt->dev_type_)
    free(opt->dev_type_);
  if(opt->ifconfig_param_.net_addr_)
    free(opt->ifconfig_param_.net_addr_);
  if(opt->post_up_script_)
    free(opt->post_up_script_);
  if(opt->cipher_)
    free(opt->cipher_);
#ifndef NO_CRYPT
  if(opt->auth_algo_)
    free(opt->auth_algo_);
  if(opt->kd_prf_)
    free(opt->kd_prf_);
  if(opt->passphrase_)
    free(opt->passphrase_);
#endif
  if(opt->key_.buf_)
    free(opt->key_.buf_);
  if(opt->salt_.buf_)
    free(opt->salt_.buf_);
}

void options_print_usage()
{
  printf("USAGE:\n");
  printf("uanytun [-h|--help]                         prints this...\n");
  printf("        [-D|--nodaemonize]                  don't run in background\n");
  printf("        [-u|--username] <username>          change to this user\n");
  printf("        [-g|--groupname] <groupname>        change to this group\n");
  printf("        [-C|--chroot] <path>                chroot to this directory\n");
  printf("        [-P|--write-pid] <path>             write pid to this file\n");
  printf("        [-i|--interface] <ip-address>       local ip address to bind to\n");
  printf("        [-p|--port] <port>                  local port to bind to\n");
  printf("        [-s|--sender-id ] <sender id>       the sender id to use\n");
  printf("        [-L|--log] <target>:<level>[,<param1>[,<param2>..]]\n");
  printf("                                            add a log target, can be invoked several times\n");

  printf("        [-r|--remote-host] <hostname|ip>    remote host\n");
  printf("        [-o|--remote-port] <port>           remote port\n");
  printf("        [-4|--ipv4-only]                    always resolv IPv4 addresses\n");
  printf("        [-6|--ipv6-only]                    always resolv IPv6 addresses\n");
  printf("        [-d|--dev] <name>                   device name\n");
  printf("        [-t|--type] <tun|tap>               device type\n");

  printf("        [-n|--ifconfig] <local>/<prefix>    the local address for the tun/tap device and the used prefix length\n");
  printf("        [-x|--post-up-script] <script>      script gets called after interface is created\n");
  printf("        [-m|--mux] <mux-id>                 the multiplex id to use\n");
  printf("        [-w|--window-size] <window size>    seqence number window size\n");
#ifndef NO_CRYPT
  printf("        [-k|--kd-prf] <kd-prf type>         key derivation pseudo random function\n");
#ifndef NO_PASSPHRASE
  printf("        [-E|--passphrase] <pass phrase>     a passprhase to generate master key and salt from\n");
#endif
  printf("        [-K|--key] <master key>             master key to use for encryption\n");
  printf("        [-A|--salt] <master salt>           master salt to use for encryption\n");
  printf("        [-e|--role] <role>                  left (alice) or right (bob)");
  printf("        [-c|--cipher] <cipher type>         payload encryption algorithm\n");
  printf("        [-a|--auth-algo] <algo type>        message authentication algorithm\n");
  printf("        [-b|--auth-tag-length] <length>     length of the auth tag\n");
#endif
}

void options_print(options_t* opt)
{
  if(!opt)
    return;

  printf("progname: '%s'\n", opt->progname_);
  printf("daemonize: %d\n", opt->daemonize_);
  printf("username: '%s'\n", opt->username_);
  printf("groupname: '%s'\n", opt->groupname_);
  printf("chroot_dir: '%s'\n", opt->chroot_dir_);
  printf("pid_file: '%s'\n", opt->pid_file_);
  printf("log_targets: \n");
  string_list_print(&opt->log_targets_, "  '", "'\n");
  printf("local_addr: '%s'\n", opt->local_addr_);
  printf("local_port: '%s'\n", opt->local_port_);
  printf("sender_id: %d\n", opt->sender_id_);
  printf("remote_addr: '%s'\n", opt->remote_addr_);
  printf("remote_port: '%s'\n", opt->remote_port_);
  printf("resolv_addr_type: ");
  switch(opt->resolv_addr_type_) {
  case ANY: printf("any\n"); break;
  case IPV4_ONLY: printf("ipv4-only\n"); break;
  case IPV6_ONLY: printf("ipv6-only\n"); break;
  default: printf("??\n"); break;
  }
  printf("dev_name: '%s'\n", opt->dev_name_);
  printf("dev_type: '%s'\n", opt->dev_type_);
  printf("ifconfig_net_addr: '%s'\n", opt->ifconfig_param_.net_addr_);
  printf("ifconfig_prefix_length: %d\n", opt->ifconfig_param_.prefix_length_);
  printf("post_up_script: '%s'\n", opt->post_up_script_);
  printf("mux: %d\n", opt->mux_);
  printf("seq_window_size: %d\n", opt->seq_window_size_);
  printf("cipher: '%s'\n", opt->cipher_);
#ifndef NO_CRYPT
  printf("auth_algo: '%s'\n", opt->auth_algo_);
  printf("auth_tag_length: %d\n", opt->auth_tag_length_);
  printf("kd_prf: '%s'\n", opt->kd_prf_);
  printf("passphrase: '%s'\n", opt->passphrase_);
  printf("role: ");
  switch(opt->role_) {
  case ROLE_LEFT: printf("left\n"); break;
  case ROLE_RIGHT: printf("right\n"); break;
  default: printf("??\n"); break;
  }
#endif

  u_int32_t i;
  printf("key_[%d]: '", opt->key_.length_);
  for(i=0; i<opt->key_.length_; ++i) printf("%02X", opt->key_.buf_[i]);
  printf("'\n");

  printf("salt_[%d]: '", opt->salt_.length_);
  for(i=0; i<opt->salt_.length_; ++i) printf("%02X", opt->salt_.buf_[i]);
  printf("'\n");
}
