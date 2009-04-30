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

#ifndef _DATATYPES_H_
#define _DATATYPES_H_

#include <stdint.h>
#include <arpa/inet.h>

typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;
/* typedef int8_t int8_t; */
/* typedef int16_t int16_t; */
/* typedef int32_t int32_t; */
/* typedef int64_t int64_t; */

typedef u_int32_t window_size_t;

typedef u_int32_t seq_nr_t;
#define SEQ_NR_T_NTOH(a) ntohl(a)
#define SEQ_NR_T_HTON(a) htonl(a)
#define SEQ_NR_MAX UINT32_MAX

typedef u_int16_t sender_id_t;
#define SENDER_ID_T_NTOH(a) ntohs(a)
#define SENDER_ID_T_HTON(a) htons(a)

typedef u_int16_t payload_type_t;
#define PAYLOAD_TYPE_T_NTOH(a) ntohs(a)
#define PAYLOAD_TYPE_T_HTON(a) htons(a)

typedef u_int16_t mux_t;
#define MUX_T_NTOH(a) ntohs(a)
#define MUX_T_HTON(a) htons(a)

typedef u_int32_t satp_prf_label_t;
#define SATP_PRF_LABEL_T_NTOH(a) ntohl(a)
#define SATP_PRF_LABEL_T_HTON(a) htonl(a)

struct buffer_struct {
  u_int32_t length_;
  u_int8_t* buf_;
};
typedef struct buffer_struct buffer_t;

#endif
