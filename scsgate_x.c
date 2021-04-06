/* --------------------------------------------------------------------------------------------
 * UART test utility - la GPIO seriale va abilitata in raspi-config 
 *      - INTERFACING OPTIONS - P6 SERIAL : login shell NO  
 *      -  serial port hardware enabled YES
 * verifica 	ls /dev/serial*
 * -------------------------------------------------------------------------------------------*/
#define PROGNAME "SCSGATE_X "
#define VERSION  "1.62"
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

#include <vector>
#include "scs_mqtt.h"
#include "scs_hue.h"

using namespace std;
// =============================================================================================
		time_t	rawtime;
struct	tm *	timeinfo;
int		timeToexec = 0;
char	immediatePicUpdate = 0;
char	verbose = 0;	// 1=verbose     2=verbose+      3=debug
// =============================================================================================
char    huegate = 0;		// simulazione hue gate per alexa
char    mqttgate = 0;		// connessione in/out a broker mqtt
char    huemqtt_direct = 0;	// 1: ponte diretto hue -> mqtt (stati)     2: ponte hue -> mqtt (comandi)
char    uartgate = 1;		// connessione in/out serial0 (uart)
char	mqttbroker[24] = {0};
char	user[24] = {0};
char	password[24] = {0};
// =============================================================================================
struct termios tios_bak;
struct termios tios;
int	   fduart = 0;
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

char busdevHue [256] = {0};

extern std::vector<hue_device_t> _devices;
std::vector<hue_scs_queue> _schedule;
std::vector<bus_scs_queue> _schedule_b;
// =============================================================================================
enum _TCP_SM
{
    TCP_FREE = 0,
    TCP_SOCKET_OK,
    TCP_BIND_OK,
    TCP_LISTENING,
    TCP_CONNECTING,
    TCP_CONNECTED,
    TCP_RECEIVED
} sm_tcp = TCP_FREE;
// ===================================================================================
    int server_file_descriptor, new_connection;
    long tcpread;
    struct sockaddr_in server_address, client_address;
    socklen_t server_len, client_len;
    int opt = 1;
    char tcpBuffer[1024] = {0};
	char tcpuart = 0;
// =============================================================================================
char	sbyte;
char    rx_prefix;
char    rx_buffer[255];
int     rx_len;
int     rx_max = 250;
char    rx_internal;
// ===================================================================================
char axTOchar(char * aData);
char aTOchar(char * aData);
int	 aTOint(char * aData);
int  setFirst(void);
static void print_usage(const char *prog);
static char parse_opts(int argc, char *argv[]);
void uSleep(int microsec);
void mSleep(int millisec);
void initDevice (void);
void UART_start(void);
int  tcpJarg(char * mybuffer, const char * argument, char * value);
void rxBufferLoad(int tries);
int	 waitReceive(char w);
void bufferPicLoad(char * decBuffer);
void BufferMemo(char * decBuffer, char hueaction);
void mqtt_dequeueExec(void);
void hue_dequeueExec(void);
// ===================================================================================
#ifdef KEYBOARD
void initkeyboard(void){
    tcgetattr(0,&tios_bak);
    tios=tios_bak;

    tios.c_lflag&=~ICANON;
    tios.c_lflag&=~ECHO;
    tios.c_cc[VMIN]=0;	// Read one char at a time
    tios.c_cc[VTIME]=0; // polling

    tcsetattr(0,TCSAFLUSH,&tios);
//	cfmakeraw(&tios); /// <------------
}
// =============================================================================================
void endkeyboard(void){
    tcsetattr(0,TCSAFLUSH,&tios_bak);
}
#endif
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
int getinWait( void )
{
	int ch;
//	ch = getch();
	ch = fgetc(stdin);
	return ch;
}
// ===================================================================================
int getinNowait( void )
{
	int n = 0;
	read(fileno(stdin), &n, 1);
	return n;
}
// ===================================================================================
int setFirst(void)
{
  if (!uartgate) return 0;
  char requestBuffer[24];
  int requestLen = 0;
  int n;

  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 0x15; // evita memo in eeprom (in 0x17)

  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 0xF1; // set led lamps std-freq (client mode)

  requestBuffer[requestLen++] = '@'; 
  requestBuffer[requestLen++] = 'M'; // (in 0x17)
  requestBuffer[requestLen++] = 'X'; // (in 0x17)

  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 'Y'; // (in 0x17)
  requestBuffer[requestLen++] = '1'; // (in 0x17)

  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 'F'; // (in 0x17)
  requestBuffer[requestLen++] = '3';

//  requestBuffer[requestLen++] = '@';
//  requestBuffer[requestLen++] = 'U'; // gestione tapparelle senza percentuale
//  requestBuffer[requestLen++] = '9';

  requestBuffer[requestLen++] = '§';
  requestBuffer[requestLen++] = 'l'; // (in 0x17)

  n = write(fduart,requestBuffer,requestLen);			// scrittura su scsgate

  mSleep(50);
  rx_len = 0;
  rxBufferLoad(100);

  requestLen = 0;
  requestBuffer[requestLen++] = '§';
  requestBuffer[requestLen++] = 'Q'; 
  requestBuffer[requestLen++] = 'Q'; 
  n = write(fduart,requestBuffer,requestLen);			// scrittura su scsgate

  rx_max = 16;
  rx_len = 0;
  rxBufferLoad(100);
  if (memcmp(rx_buffer,"SCS ",3) == 0)
  {
	  printf("===============> %s <================\n",rx_buffer);
  }
  else
  {
    printf("%s\n",rx_buffer);
	printf("SCSGATE connection failed!\n");
	return -1;
  }
  rx_len = 0;
  rx_max = 250;
  return n;
}
// ===================================================================================
static void print_usage(const char *prog)	// NOT USED
{
	printf("Usage: %s [-uvHBDN]\n", prog);
	puts("  -u --picupdate  immediate update pic eeprom \n"
		 "  -v --verbose 1/2/3  \n"
		 "  -H --huegate interface(alexa)\n"
		 "  -B --broker address  broker name/address:port (default localhost)\n"
		 "  -U --broker username\n"
		 "  -P --broker password\n"
		 "  -D --direct connection hue->mqtt  1:state  2:command\n"
//		 "  -N --nouart no serial connection\n"		// comando privato
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
			{ "picupdate",  0, 0, 'u' },
			{ "verbose",    2, 0, 'v' },
			{ "huegate",    0, 0, 'H' },
			{ "broker",     2, 0, 'B' },
			{ "user",       2, 0, 'U' },
			{ "password",   2, 0, 'P' },
			{ "direct",     2, 0, 'D' },
			{ "nouart",     0, 0, 'N' },
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
		case 'u':
			immediatePicUpdate = 1;
			printf("Immediate pic update\n");
			break;
		case 'H':
			printf("HUE interface on\n");
			huegate = 1;
			break;
		case 'D':
			printf("direct connection HUE->MQTT\n");
			// 1: ponte diretto hue -> mqtt (stati)     2: ponte hue -> mqtt (comandi)
			if (optarg) 
				huemqtt_direct=aTOchar(optarg);
			if ((huemqtt_direct == 0) || (huemqtt_direct > 2)) huemqtt_direct = 1;		// simulazione hue gate per alexa
			break;
		case 'N':
			printf("no uart serial connection\n");
			uartgate = 0;	// senza connessione uart
			break;
		case 'B':
			if (optarg) 
				strcpy(mqttbroker, optarg);
			else
				strcpy(mqttbroker,"localhost:1883");

			mqttgate = 1;		// simulazione hue gate per alexa
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
void initDevice (void)
{
  // Free the name for each device
  for (auto& device : _devices) {
    free(device.name);
  }
  // Delete devices
  _devices.clear();

  // Delete schedule
  _schedule.clear();
  _schedule_b.clear();
}
// ===================================================================================
void UART_start(void)
{
	if (!uartgate) return;

	printf("UART_Initialization\n");
	fduart = -1;
	
	fduart = open("/dev/serial0", O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
	if (fduart == -1) 
	{
		perror("open_port: Unable to open /dev/serial0 - ");
        exit(EXIT_FAILURE);
	}

	struct termios options;
	tcgetattr(fduart, &options);
//	cfsetispeed(&options, B115200);
//	cfsetospeed(&options, B115200);
//	options.c_cflag = B115200 | CS8 | CLOCAL | CREAD;		//<Set baud rate
	options.c_cflag = B115200 | CS8 | CLOCAL | CREAD | CSTOPB;		//<Set baud rate - 2 bits stop?
	options.c_iflag = IGNPAR;
	options.c_oflag = 0;
	options.c_lflag = 0;
	tcflush(fduart, TCIFLUSH);
	tcsetattr(fduart, TCSANOW, &options);
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
void rxBufferLoad(int tries)
{
	if (!uartgate)  return;

	int r;
	int loop = 0;

    while ((rx_len < rx_max) && (loop < tries))
    {
		r = -1;
		r = read(fduart, &sbyte, 1);
	    if ((r > 0) && (rx_len < rx_max))
		{
			if (verbose) fprintf(stderr,"%2x ", sbyte);	// scrittura a video
			rx_buffer[rx_len++] = sbyte;
			loop = 0;
		}
		else
			loop++;
		uSleep(90);
    }
    rx_buffer[rx_len] = 0;        // aggiunge 0x00
	if ((verbose) && (rx_len)) fprintf(stderr," - ");	// scrittura a video
}
// ===================================================================================
int	waitReceive(char w)
{
	if (!uartgate) return 0;

	int r;
	int loop = 0;

    while (loop < 10000) // 1 sec
    {
		r = -1;
		r = read(fduart, &sbyte, 1);
	    if ((r > 0) && (sbyte == w))
		{
			return 1;
		}
		else
			loop++;
		uSleep(100);
    }
//			if (verbose) fprintf(stderr,"%2x ", sbyte);	// scrittura a video
	return 0;
}
// ===================================================================================
void bufferPicLoad(char * decBuffer)
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
	if ((device) && (uartgate)) 
	{
	  tcpJarg(decBuffer,"\"descr\"",alexadescr);
	  tcpJarg(decBuffer,"\"type\"",stype);
	  devtype = aTOchar(stype);
//	  devType[(int)device] = devtype;

	  if (devtype == 9)			// w6 - aggiorna tapparelle pct su pic
	  {
		char smaxpos[8];
		tcpJarg(decBuffer,"\"maxp\"",smaxpos);

		WORD_VAL maxp;
		maxp.Val = aTOint(smaxpos);

		rx_len = 0;
		rxBufferLoad(10);	// discard uart input

		char requestBuffer[16];
		int requestLen = 0;
		requestBuffer[requestLen++] = '§';
		requestBuffer[requestLen++] = 'U';
		requestBuffer[requestLen++] = '8';
		requestBuffer[requestLen++] = device;     // device id
		requestBuffer[requestLen++] = devtype;    // device type
		requestBuffer[requestLen++] = maxp.byte.HB;    // max position H
		requestBuffer[requestLen++] = maxp.byte.LB;    // max position L
		write(fduart,requestBuffer,requestLen);			// scrittura su scsgate

		if (waitReceive('k') == 0)
			printf("  -->PIC communication ERROR...\n");
	  }
//				  else
//					maxp.Val = 0;

	} // deviceX > 0
  }  // busid != ""
  else
  {
	  char cover[8];
	  tcpJarg(decBuffer,"\"coverpct\"",cover);
	  if ((strcmp(cover,"false") == 0) && (uartgate)) 
	  {
		rx_len = 0;
		rxBufferLoad(10);	// discard uart input
		write(fduart,"§U9",3);			// scrittura su scsgate
		if (waitReceive('k') == 0)
			printf("  -->PIC communication ERROR...\n");
	  }  // cover == "false"
  }
}	
// ===================================================================================
void BufferMemo(char * decBuffer, char hueaction)  //  hueaction 1=add device
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

	  printf("device %2X tipo %02u - %s \n",device,devtype,alexadescr);

	  if ((hueaction) && (devtype < 11))		// w6 - alexa non ha bisogno dei types 0x0B 0x0C (11-12) 0x0E 0x0F 0x12 0x13 (18 19)
	  {
          if (devtype == 9)
			busdevHue[(int)device] = addDevice(alexadescr, 1,device);
          else
			busdevHue[(int)device] = addDevice(alexadescr, 128,device);
	  }
	} // deviceX > 0
  }  // busid != ""
}	
// ===================================================================================
void mqtt_dequeueExec( void)
{
	int  m = 0; // _schedule.begin();
	char busid = _schedule_b[m].busid;
	char bustype = busdevType[(int)busid];
	if (bustype == 0) 
	{
		bustype = _schedule_b[m].bustype;
		busdevType[(int)busid] = bustype;
	}
//	int  hueid = (int) _schedule_b[m].hueid;
	char command = _schedule_b[m].buscommand;
	char value = _schedule_b[m].busvalue;
	char request = _schedule_b[m].busrequest;
	char from = _schedule_b[m].busfrom;
	_schedule_b.erase(_schedule_b.begin());

//	printf("dequeued - id:%02x  command:%02x  value:%d  bus:%02x  type:%02x  des: %s\n", hueid, huecommand, pctvalue, busid, bustype, _devices[hueid].name);
//	// comandi  01 accendi       02 spegni     04 alza / aumenta       05 abbassa 

	char requestBuffer[24];
	int requestLen = 0;
	if ((command == 0xFF) && (busid != 0) && (bustype == 9))
	{
	  requestBuffer[requestLen++] = '§';
	  requestBuffer[requestLen++] = 'u';    // 0x79 (@y: invia a pic da MQTT cmd tapparelle % da inviare sul bus)
	  requestBuffer[requestLen++] = busid; // to   device
	  requestBuffer[requestLen++] = value;	// %
	}
	if ((command != 0xFF) && (busid != 0) && (bustype != 0))
	{
	//    if (devtype != 11)	// <====================not GENERIC=======================
		{
		  requestBuffer[requestLen++] = '§';
		  requestBuffer[requestLen++] = 'y';    // 0x79 (@y: invia a pic da MQTT cmd standard da inviare sul bus)
		  requestBuffer[requestLen++] = busid; // to   device
		  requestBuffer[requestLen++] = from;   // from device
		  requestBuffer[requestLen++] = request;   // type:command
		  requestBuffer[requestLen++] = command;// command
		}
	}
	if (requestLen > 0)
	{
		if (uartgate) write(fduart,requestBuffer,requestLen);			// scrittura su scsgate
		printf("SCS cmd from mqtt: %c %c ",requestBuffer[0],requestBuffer[1]);
		for (int i=2;i<requestLen;i++)
		{
			printf("%02X ",requestBuffer[i]);
		}
		printf("\n");
	}
}
// ===================================================================================
void hue_dequeueExec( void)
{
	int  m = 0; // _schedule.begin();
	int  hueid = (int) _schedule[m].hueid;
	char huecommand = _schedule[m].huecommand;
	char pctvalue = _schedule[m].pctvalue;
	char huevalue = _schedule[m].huevalue;
	char busid = _devices[hueid].busaddress;
	char bustype = busdevType[(int)busid];
	_schedule.erase(_schedule.begin());

	printf("dequeued - id:%02x  command:%02x  value:%d  bus:%02x  type:%02x  des: %s\n", hueid, huecommand, pctvalue, busid, bustype, _devices[hueid].name);
	// comandi  01 accendi       02 spegni     04 alza / aumenta       05 abbassa 

	char requestBuffer[16];
	int requestLen = 0;

	char command;
	char buscommand;
	int  pct = 0;
	char stato = _devices[hueid].state;
	switch (bustype)
	{
	// --------------------------------------- SWITCHES -------------------------------------------
	case 1:
	  if (huecommand == 1) // accendi <----------------
	  {
		command = 0;
		setState(hueid, stato | 1, 128);
	  }
	  else if (huecommand == 2) // spegni  <----------------
	  {
		command = 1;
		setState(hueid, stato & 0xFE, 128);
	  }
	  else
		break;
	    
	// comando §y<destaddress><source><type><command>
		requestBuffer[requestLen++] = '§';
		requestBuffer[requestLen++] = 'y';
		requestBuffer[requestLen++] = busid;   // to device id
		requestBuffer[requestLen++] = 0x00;    // from device id
		requestBuffer[requestLen++] = 0x12;    // command
		requestBuffer[requestLen++] = command; // command char
	  break;


	case 3:
	  // --------------------------------------- LIGHTS DIMM ------------------------------------------
	  if (huecommand == 1) // accendi  <----------------
	  {
		command = 0;
		buscommand = 0;
		setState(hueid, stato | 1, 0);
	  }
	  else if (huecommand == 2) // spegni <----------------
	  {
		command = 1;
		buscommand = 1;
		setState(hueid, stato & 0xFE, 0);
	  }
	  else if ((huecommand == 3) || (huecommand == 4) || (huecommand == 5)) // cambia il valore  <----------------
	  {
		setState(hueid, 1, huevalue);

		// trasformare da % a 1D-9D <--------------------------------------------------------------
		pct = pctvalue;    // percentuale da 0 a 100
//		pct *= 100;                  // da 0 a 25500
//		pct /= 255;                  // da 0 a 100
		pct += 5;                    // arrotondamento
		pct /= 10;                   // 0-10
		if (pct > 9) pct = 9;
		if (pct == 0) pct = 1;	   // 1-9
		pct *= 16;                   // hex high nibble
		pct += 0x0D;                 // hex low  nibble
		buscommand = (unsigned char) pct;
		command = 0x8F;
	  }
	  else
		break;

	// comando §y<destaddress><source><type><command>
		requestBuffer[requestLen++] = '§';
		requestBuffer[requestLen++] = 'y';
		requestBuffer[requestLen++] = busid;   // to device id
		requestBuffer[requestLen++] = 0x00;    // from device id
		requestBuffer[requestLen++] = 0x12;    // command
		requestBuffer[requestLen++] = buscommand; // command char
	  break;


	case 4:
	  break;

	case 8:
	//              case 18:
	  // --------------------------------------- COVER ---------------------------------------------------
	  if ((huecommand == 2) || (huecommand == 1)) // spegni / ferma  <--oppure accendi------
	  {
		command = 0x0A;
		setState(hueid, stato | 0xC1, huevalue); // 0xc1: dara' errore ma almeno evita il blocco
	  }
	  else if (huecommand == 4) // alza  <----------------
	  {
		command = 0x08;
		setState(hueid, stato | 0xC0, huevalue); // 0xc0: dopo aver inviato lo stato setta value a 128
	  }
	  else if (huecommand == 5) // abbassa  <----------------
	  {
		command = 0x09;
		setState(hueid, stato | 0xC0, huevalue); // 0xc0: dopo aver inviato lo stato setta value a 128
	  }
	  else
		break;

	// comando §y<destaddress><source><type><command>
		requestBuffer[requestLen++] = '§';
		requestBuffer[requestLen++] = 'y';
		requestBuffer[requestLen++] = busid;   // to device id
		requestBuffer[requestLen++] = 0x00;    // from device id
		requestBuffer[requestLen++] = 0x12;    // command
		requestBuffer[requestLen++] = command; // command char
	  break;

	case 9:
	//              case 19:
	  // --------------------------------------- COVERPCT alexa------------------------------------------------
//	  setState(hueid, 1, huevalue);  // coverpct - lo stato deve sempre essere ON
	  pct = pctvalue;
//	  pct *= 100;
//	  pct /= 255;

	  if (huecommand == 2) // spegni / ferma  <----------------
	  {
		command = 0x80;
		pct = 0;
//		if (pctvalue < 5)
//		  setState(hueid, 0, huevalue);
	  }
	  else if (huecommand == 1) // accendi (SU) <----------
	  {
		command = 0x81;
	  }
	  else if (huecommand == 4) // alza  <----------------
	  {
		command = 0x84;
	  }
	  else if (huecommand == 5) // abbassa  <----------------
	  {
		command = 0x85;
	  }
	  else // (huecommand == 3) // non cambia  <----------------
	  {
		command = 0x89;
		break;
	  }

	  requestBuffer[requestLen++] = '§';
	  requestBuffer[requestLen++] = 'u';
	  requestBuffer[requestLen++] = busid;   // to device id
	  requestBuffer[requestLen++] = (char) pct; // command char
	  break;
	} // end switch

	if (requestLen > 0)
	{
		if (uartgate) write(fduart,requestBuffer,requestLen);			// scrittura su scsgate
		printf("SCS cmd from HUE: %c %c ",requestBuffer[0],requestBuffer[1]);
		for (int i=2;i<requestLen;i++)
		{
			printf("%02X ",requestBuffer[i]);
		}
		printf("\n");



		if	(huemqtt_direct)
		{// ponte diretto hue -> mqtt
			bus_scs_queue busdata;
			busdata.busid = busid;
			busdata.bustype = bustype;
			busdata.busvalue = (char) pct; 
			busdata.busfrom = 0;
			busdata.busrequest = 0x12;
			busdata.buscommand = command;
//			printf("mqttr %02x t:%02x c:%02x v:%d\n",busid,bustype,command,(char)pct);
			if	(huemqtt_direct == 1)
				MQTTrequest(&busdata);
			else
			if	(huemqtt_direct == 2)
				MQTTcommand(&busdata);
		}

	}
}
// ===================================================================================











// ===================================================================================
int main(int argc, char *argv[])
{
#ifdef KEYBOARD
	initkeyboard();
	atexit(endkeyboard);
#endif 

	//	printf(CLR WHT BOLD UNDER PROGNAME BOLD VERSION NRM "\n");

	printf(PROGNAME "\n");

	if (parse_opts(argc, argv))
		return 0;
	if ((mqttgate == 0) && (huegate == 0))
	{
		print_usage(argv[0]);
		return -1;
	}

	if ((mqttgate == 0) || (huegate == 0))
		huemqtt_direct = 0;	

	UART_start();

	initDevice ();

	if (huegate) 
	{
		if ((HUE_start(verbose)) < 0)
		{
			printf("HUE start failed\n");
			return -1;
		}
	}

	if (mqttgate) 
	{
		printf("MQTT broker connect: %s  - user: %s  password: %s\n", mqttbroker,user,password);
		if (MQTTconnect(mqttbroker,user,password,verbose))
			printf("MQTT open ok\n");
		else
		{
			printf("MQTT connect failed\n");
			return -1;
		}
	}

	if (verbose) printf("\n");


	// First write to the port
	int c = setFirst();
	if (c < 0) 
	{
		perror("Serial0 write failed - ");
		return -1;
	}
	else
		if (verbose) fprintf(stderr,"Serial0 initialized - OK\n");

	mSleep(20);					// pausa
	rx_len = 0;
	rxBufferLoad(10);	// discard uart input


	char sbyte;
	strcpy(filename,CONFIG_FILE);
	for (int i=0; i<255; i++) 
	{
		busdevType[i] = 0;
		busdevHue [i] = 0xff;
	}

// preload config file ----------------------------------------------------------------------------
	c = 0;
	fConfig = fopen(filename, "rb");
	if (fConfig)
	{
		char line[128];
		while (fgets(line, 128, fConfig))
		{
		  char busid[8];
		  tcpJarg(line,"\"device\"",busid);
		  if (busid[0] != 0)  c++;
		  if (immediatePicUpdate)
		  {
			bufferPicLoad(line);  
		  }
		  BufferMemo(line, 1);  // 1=add device in hue table
		}
		fclose(fConfig);
		fConfig = NULL;
	}

	if (verbose) printf("%d devices loaded from file\n",c);

// =======================================================================================================
	while (1)
	{
		if (timeToClose)
		{
			timeToClose--;
			if (timeToClose == 0)
			{
				if (fConfig)
				{
					fclose(fConfig);
					fConfig = NULL;
				}
			}
		}

		mSleep(1);
		rx_internal = 9;
// ---------------------------------------------------------------------------------------------------------------------------------
		if (uartgate) 
			c = read(fduart, &sbyte, 1);			// lettura da scsgate
		else
			c = 0;
		if (c > 0) 
		{
			if (verbose) fprintf(stderr,"\n%2X ", sbyte);	// scrittura a video
			rx_len = 0;
			rx_buffer[rx_len++] = sbyte;
			rx_prefix = sbyte;
		    if ((rx_prefix > 0xF0) && (rx_prefix < 0xF9))	// 0xf5 y aa bb cc dd
			{
				rx_internal = 1;
				rx_max = (rx_prefix & 0x0F);
				rx_max++;
			}
			else									// rx: F5 y 31 00 12 00 
			{
				rx_internal = 0;
				rx_max = 255;
			}
			rxBufferLoad(3);
		}
// ---------------------------------------------------------------------------------------------------------------------------------

		if ((rx_internal == 1) && (rx_buffer[1] == 'y') && ((rx_buffer[0] == 0xF5) || (rx_buffer[0] == 0xF6)))   
		{ // START pubblicazione stato device        [0xF5] [y] 32 00 12 01
			rx_internal = 9;
			if (verbose) fprintf(stderr," %c msg\n",rx_buffer[1]);	// scrittura a video

			bus_scs_queue _scsrx;
			_scsrx.busrequest = rx_buffer[4];

			if (_scsrx.busrequest == 0x30)  // <-termostato----------------------------
			{
 				if (rx_buffer[2] == 0xB4)
				{
				    _scsrx.busid = rx_buffer[3];	// device
				    _scsrx.busfrom = rx_buffer[2];  // from

					_scsrx.buscommand = 0;
					_scsrx.bustype = busdevType[(int)_scsrx.busid];
					if (_scsrx.bustype == 0) 
					{
						_scsrx.bustype = 0x0F;
						busdevType[(int)_scsrx.busid] = _scsrx.bustype;
					}
					_scsrx.busvalue = rx_buffer[5];
					MQTTrequest(&_scsrx);
				}
			}
			else
			if (_scsrx.busrequest == 0x12)  // <-comando----------------------------
			{
 				if (rx_buffer[2] < 0xB0)
				{
				    _scsrx.busid = rx_buffer[2];  // to
				    _scsrx.busfrom = rx_buffer[3];  // from
				}
				else
 				if (rx_buffer[3] < 0xB0)
				{
				    _scsrx.busid = rx_buffer[3];  // from
				    _scsrx.busfrom = rx_buffer[2];  // to
				}
				else
    // ================================ TRATTAMENTO COMANDI GLOBALI  ===========================================
 				if (rx_buffer[2] == 0xB1)
				{
				    _scsrx.busid = rx_buffer[2];  // to
				    _scsrx.busfrom = rx_buffer[3];  // from
				}
				else
	// ==========================================================================================================
				{
					_scsrx.busid = 0;
				    _scsrx.busfrom = 0;  // to
				}

				_scsrx.buscommand = rx_buffer[5];
				_scsrx.bustype = busdevType[(int)_scsrx.busid];
				_scsrx.busvalue = busdevHue [(int)_scsrx.busid];

//				setState(unsigned char id, char state, unsigned char value) // non indispensabile -

				MQTTrequest(&_scsrx);

			}
			else   // <-rx_buffer[4] != 0x12 
			{		      
    // ================================ TRATTAMENTO COMANDI GENERICI  ===========================================




			}

		}

		if ((rx_internal == 1) && ((rx_buffer[1] == 'u') || (rx_buffer[1] == 'm')) && (rx_buffer[0] == 0xF3)) 
		{ // START pubblicazione stato device        [0xF5] [y] 32 00 12 01
			rx_internal = 9;
			if (verbose) fprintf(stderr," %c msg\n",rx_buffer[1]);	// scrittura a video

			bus_scs_queue _scsrx;

			_scsrx.busid = rx_buffer[2];
			_scsrx.busvalue = rx_buffer[3];
			_scsrx.buscommand = 0x80;
			_scsrx.busfrom = 0;
			_scsrx.busrequest = rx_buffer[1];	// u
			_scsrx.bustype = busdevType[(int)_scsrx.busid];

			char hueid = busdevHue[(int)_scsrx.busid];

			if (hueid != 0xff) 
			{
				int pct = ((_scsrx.busvalue * 255)+50) /100;
				setState(hueid, 0xFF, (char)pct);
			}
			MQTTrequest(&_scsrx);
		}


#ifdef KEYBOARD
		c = getinNowait();				// lettura tastiera
		if (c)
		{
			if (verbose) fprintf(stderr, "%c", (char) c);// echo a video
			if (uartgate)
			{
				n = write(fduart,&c,1);			// scrittura su scsgate
				if (n < 0) 
				{
					perror("Write failed - ");
					return -1;
				}
			}
		}
#endif
		if (timeToexec == 0)
		{
			if (_schedule.size() > 0)
			{
				hue_dequeueExec();
				timeToexec = 100;  // msec to wait until next command execute
			}
			else
			if (_schedule_b.size() > 0)
			{
				mqtt_dequeueExec();
				timeToexec = 100;  // msec to wait until next command execute
			}
		}
		else
			timeToexec--;

		if (mqttgate) 
			publish_dequeue();

		if (mqttgate) 
			MQTTverify();

	}
// =======================================================================================================

		
		// Don't forget to clean up
	if (uartgate) close(fduart);
	if (new_connection)	close(new_connection);

    // Close the server before exiting the program.
	if (huegate) 
	{
		HUE_stop();
	}
	if (mqttgate) 
		MQTTstop();

#ifdef KEYBOARD
	endkeyboard();
#endif
	return 0;
}
// ===================================================================================
