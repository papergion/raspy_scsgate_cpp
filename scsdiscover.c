/* --------------------------------------------------------------------------------------------
 * UART test utility - la GPIO seriale va abilitata in raspi-config 
 *      - INTERFACING OPTIONS - P6 SERIAL : login shell NO  
 *      -  serial port hardware enabled YES
 * verifica 	ls /dev/serial*
 * -------------------------------------------------------------------------------------------*/
#define PROGNAME "SCSDISCOVER "
#define VERSION  "1.20"
//#define KEYBOARD

// =============================================================================================
#define CONFIG_FILE "scsconfig"
// =============================================================================================

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <time.h>
#include <errno.h>
#include <termios.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#include <tcpserver.h>
#include <udpserver.h>
#include <iostream>
#include "basesocket.h"
#include <functional>
#include <thread>
#include <ifaddrs.h>
#include <net/if.h>
#include <linux/if_link.h>

#include "scs_mqtt_disc.h"

using namespace std;
// =============================================================================================
char	verbose = 0;	// 1=verbose     2=verbose+      3=debug
// =============================================================================================
char	mqttbroker[24] = {0};
char	user[24] = {0};
char	password[24] = {0};
// =============================================================================================
FILE   *fConfig;
char	filename[64];
int  timeToClose = 0;
// =============================================================================================
  typedef union _WORD_VAL
  {
    int  Val;
    char v[2];
    struct
    {
        char LB;
        char HB;
    } byte;
  } WORD_VAL;
// =============================================================================================
char axTOchar(char * aData);
char aTOchar (char * aData);
int  aTOint  (char * aData);

static char parse_opts(int argc, char *argv[]);
void uSleep(int microsec);
void mSleep(int millisec);
// =============================================================================================
char busdevType[256] = {0};
				// 0x01:switch       0x03:dimmer   
				// 0x08:tapparella   0x09:tapparella pct   
				// 0x0B generic		 0x0C generic 
				// 0x0E:allarme		 0x0F termostato
				// 0x30-0x37:rele i2c
				// 0x40-0x47:pulsanti i2c

// =============================================================================================
char axTOchar(char * aData)
{
char *ptr;
long ret;
    ret = strtoul(aData, &ptr, 16);
    return (char) ret;
}
// =============================================================================================
char aTOchar(char * aData)
{
char *ptr;
long ret;
    ret = strtoul(aData, &ptr, 10);
    return (char) ret;
}
// =============================================================================================
int aTOint(char * aData)
{
char *ptr;
long ret;
    ret = strtoul(aData, &ptr, 10);
    return (int) ret;
}
// ===================================================================================
static void print_usage(const char *prog)	// NOT USED
{
	printf("Usage: %s [-vBUP]\n", prog);
	puts("  -v --verbose 1/2/3  \n"
		 "  -B --broker address  broker name/address:port (default localhost)\n"
		 "  -U --broker username\n"
		 "  -P --broker password\n"
		 );
	exit(1);
}
// ===================================================================================
static char parse_opts(int argc, char *argv[])	// NOT USED
{
	if ((argc < 1) || (argc > 9))
	{
		print_usage(PROGNAME);
		return 3;
	}

	while (1) {
		static const struct option lopts[] = {
//------------longname---optarg---short--      0=no optarg    1=optarg obbligatorio     2=optarg facoltativo
			{ "verbose",    2, 0, 'v' },
			{ "broker",     2, 0, 'B' },
			{ "user",       2, 0, 'U' },
			{ "password",   2, 0, 'P' },
			{ "help",		0, 0, '?' },
			{ NULL, 0, 0, 0 },
		};
		int c;
		c = getopt_long(argc, argv, "uv::HB::U:P:DNh", lopts, NULL);
		if (c == -1)
			return 0;

		switch (c) {
		case 'h':
			print_usage(PROGNAME);
			break;
		case 'B':
			if (optarg) 
				strcpy(mqttbroker, optarg);
			else
				strcpy(mqttbroker,"localhost:1883");
			break;
		case 'U':
			if (optarg) 
				strcpy(user, optarg);
			break;
		case 'P':
			if (optarg) 
				strcpy(password, optarg);
			break;
		case 'v':
			if (optarg) 
				verbose=aTOchar(optarg);
			if (verbose == 0) verbose=1;
			if (verbose > 9) verbose=9;

			printf("Verbose %d\n",verbose);
			break;

		case '?':
            fprintf (stderr, "Unknown option `-%c'.\n", optopt);
			return 2;		

		default:
			print_usage(argv[0]);
			break;
		}
	}
	return 0;
}
// ===================================================================================
void uSleep(int microsec) {
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = microsec * 1000L;
    nanosleep(&req, (struct timespec *)NULL);
}
// ===================================================================================
void mSleep(int millisec) {
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = millisec * 1000000L;
    nanosleep(&req, (struct timespec *)NULL);
}
// ===================================================================================
int tcpJarg(char * mybuffer, const char * argument, char * value)
{
  int rc = 0;
  char* p1 = strstr(mybuffer, argument); // cerca l'argomento
  if (p1)
  {
//  char* p2 = strstr(p1, ":");          // cerca successivo :
    char* p2 = strchr(p1, ':');          // cerca successivo :
    if (p2)
    {
//    char* p3 = strstr(p2, "\"");       // cerca successivo "
      char* p3 = strchr(p2, '\"');       // cerca successivo "
      if (p3)
      {
        p3++;
        while ((*p3 != '\"') && (rc < 120))
        {
          *value = *p3;
		  value++;
		  p3++;
		  rc++;
		}
      }
    }
  }
  *value = 0;
  return rc;
}
// ===================================================================================
void BufferPublishDiscover(char * decBuffer) 
{
  char busid[8];
  char device;
  char devtype;
  char alexadescr[32] = {0};
  char stype[8];

  tcpJarg(decBuffer,"\"device\"",busid);
  if (busid[0] != 0)
  {
	device = axTOchar(busid);
	if (device)
	{
	  tcpJarg(decBuffer,"\"descr\"",alexadescr);
	  tcpJarg(decBuffer,"\"type\"",stype);
	  devtype = aTOchar(stype);
	  busdevType[(int)device] = devtype;

	  if (devtype < 18)
	  {
	    printf("\n------------------------------------------------------------------------------\n");
		printf("device %02X tipo %02u - %s \n",device,devtype,alexadescr);
		MQTTpublishDiscover(busid, alexadescr, devtype);
	  }
	} // deviceX > 0
  }  // busid != ""
}	
// ===================================================================================











// ===================================================================================
int main(int argc, char *argv[])
{
	printf(PROGNAME "\n");

	if (parse_opts(argc, argv))
		return 0;

	printf("MQTT broker connect: %s  - user: %s  password: %s\n", mqttbroker,user,password);
	if (MQTTconnect(mqttbroker,user,password,verbose))
		printf("MQTT open ok\n");
	else
	{
		printf("MQTT connect failed\n");
		return -1;
	}

	if (verbose) printf("\n");

	strcpy(filename,CONFIG_FILE);
	for (int i=0; i<255; i++) 
	{
		busdevType[i] = 0;
	}

// load config file ----------------------------------------------------------------------------
	int c = 0;
	fConfig = fopen(filename, "rb");
	if (fConfig)
	{
		char line[128];
		while (fgets(line, 128, fConfig))
		{
		  char busid[8];
		  tcpJarg(line,"\"device\"",busid);
		  if (busid[0] != 0)  c++;
		  BufferPublishDiscover(line);
		}
		fclose(fConfig);
		fConfig = NULL;
	}

	if (verbose) printf("%d devices loaded from file\n",c);

// =======================================================================================================
	MQTTstop();
	return 0;
}
// ===================================================================================
