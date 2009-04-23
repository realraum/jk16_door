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
#include <errno.h>
#include <signal.h>

#include <librfid/rfid.h>
#include <librfid/rfid_scan.h>
#include <librfid/rfid_reader.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_protocol.h>

#include <librfid/rfid_protocol_mifare_classic.h>
#include <librfid/rfid_protocol_mifare_ul.h>

//#include <librfid/rfid_access_mifare_classic.h>

struct rfid_reader_handle *rh = NULL;
struct rfid_layer2_handle *l2h = NULL;
struct rfid_protocol_handle *ph = NULL;


void sigHandler(int sig)
{
  printf("sig handler called\n");

/*   rfid_protocol_close(ph); */
/*   rfid_protocol_fini(ph); */

  rfid_layer2_close(l2h);
  rfid_layer2_fini(l2h);

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

int l3_init(int protocol)
{
	ph = rfid_protocol_init(l2h, protocol);
	if (!ph) {
		fprintf(stderr, "error during protocol_init\n");
		return -1;
	}
	if (rfid_protocol_open(ph) < 0) {
		fprintf(stderr, "error during protocol_open\n");
		return -1;
	}

	return 0;
}

static int mifare_cl_auth(unsigned char *key, int page)
{
	int rc;

	rc = mfcl_set_key(ph, key);
	if (rc < 0) {
		fprintf(stderr, "key format error\n");
		return rc;
	}
	rc = mfcl_auth(ph, RFID_CMD_MIFARE_AUTH1A, page);
	if (rc < 0) {
		fprintf(stderr, "mifare auth error\n");
		return rc;
	} else 
//		printf("mifare auth succeeded!\n");

	return 0;
}

static void mifare_l3(void)
{
	while (l2_init(RFID_LAYER2_ISO14443A) < 0) ;

//	printf("ISO14443-3A anticollision succeeded\n");
 
//	while (l3_init(RFID_PROTOCOL_MIFARE_CLASSIC) < 0) ;

//	printf("Mifare card available\n");
}

void print_hex(const unsigned char* buffer, int length)
{
  int i;

  if(length <= 0) return;
  for(i=0; i<length; ++i) {
    printf("0x%02X ", buffer[i]);
    if(i && !((i+1)%8)) printf(" ");
    if(i && !((i+1)%16)) printf("\n");
  }
}

int main(int argc, char **argv)
{
	int len, rc, c, option_index = 0;
	unsigned int page,uid,uid_len;
	//char key[MIFARE_CL_KEY_LEN];
	//char buf[MIFARE_CL_PAGE_SIZE];

  (void) signal(SIGHUP, sigHandler);
  (void) signal(SIGINT, sigHandler);
  (void) signal(SIGTERM, sigHandler);

/* init */

	//memcpy(key, MIFARE_CL_KEYA_DEFAULT_INFINEON, MIFARE_CL_KEY_LEN);

//	printf("initializing librfid\n");
	rfid_init();

	rh = rfid_reader_open(NULL, RFID_READER_OPENPCD);
	if (!rh) {
		fprintf(stderr, "No OpenPCD found\n");
		exit(1);
	}

/* reading serial */

  page = 0; //page 0

  //len = MIFARE_CL_PAGE_SIZE;
  mifare_l3();
/*   if (mifare_cl_auth(key, page) < 0) */
/*     exit(1); */
  
  uid_len=sizeof(uid);
  uid=0;
  if(rfid_layer2_getopt(l2h,RFID_OPT_LAYER2_UID,&uid,&uid_len)>=0)
    printf("UID=%08X\n",uid);
  fflush(stdout);

//  rfid_layer2_close(l2h);
//  rfid_layer2_fini(l2h);

	rfid_reader_close(rh);
	exit(0);
}

