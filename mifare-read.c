/* mifare-tool - a small command-line tool for librfid mifare testing
 *
 * (C) 2006 by Harald Welte <laforge@gnumonks.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <librfid/rfid.h>
#include <librfid/rfid_scan.h>
#include <librfid/rfid_reader.h>
#include <librfid/rfid_layer2.h>

struct rfid_reader_handle *rh = NULL;
struct rfid_layer2_handle *l2h = NULL;

void sigHandler(int sig)
{
  printf("sig handler called\n");

/*   rfid_layer2_close(l2h); */
/*   rfid_layer2_fini(l2h); */

  rfid_reader_close(rh);
  exit(0);
}

int l2_init(int layer2)
{
  int rc;

  l2h = rfid_layer2_init(rh, layer2);
  if (!l2h) {
    fprintf(stderr, "error during layer2(%d)_init (0=14a,1=14b,3=15)\n",layer2);
    return -1;
  }

  rc = rfid_layer2_open(l2h);
  if (rc < 0) {
    fprintf(stderr, "error during layer2_open\n");
    return rc;
  }

  return 0;
}

int main(int argc, char **argv)
{
  unsigned int uid, uid_len;

  rfid_init();
  rh = rfid_reader_open(NULL, RFID_READER_OPENPCD);
  if (!rh) {
    fprintf(stderr, "No OpenPCD found\n");
    exit(1);
  }

  (void) signal(SIGHUP, sigHandler);
  (void) signal(SIGINT, sigHandler);
  (void) signal(SIGTERM, sigHandler);

  while(l2_init(RFID_LAYER2_ISO14443A) < 0);
  
  uid_len = sizeof(uid);
  uid = 0;
  if(rfid_layer2_getopt(l2h, RFID_OPT_LAYER2_UID, &uid, &uid_len) >= 0)
    printf("UID=%08X\n",uid);
  fflush(stdout);

/*   rfid_layer2_close(l2h); */
/*   rfid_layer2_fini(l2h); */

  rfid_reader_close(rh);
  exit(0);
}

