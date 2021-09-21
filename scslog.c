/* ---------------------------------------------------------------------------
 * UART test utility - la GPIO seriale va abilitata in raspi-config 
 *      - INTERFACING OPTIONS - P6 SERIAL : login shell NO  
 *      -  serial port hardware enabled YES
 * verifica 	ls /dev/serial*
 * ---------------------------------------------------------------------------*/
#define PROGNAME "SCSLOG "
#define VERSION  "1.04"

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
#include <signal.h>

#define CLR  "\x1B[2J"
//#define CLR  "\033[H\033[2J"
#define NRM  "\x1B[0m"
#define RED  "\x1B[31m"
#define GRN  "\x1B[32m"
#define YEL  "\x1B[33m"
#define BLU  "\x1B[34m"
#define MAG  "\x1B[35m"
#define CYN  "\x1B[36m"
#define WHT  "\x1B[37m"

#define BOLD  "\x1B[1m"
#define UNDER "\x1B[4m"
#define BLINK "\x1B[5m"
#define REVER "\x1B[7m"
#define HIDD  "\x1B[8m"

// =============================================================================================
		time_t	rawtime;
struct	tm *	timeinfo;
char	verbose = 0;
// =============================================================================================
  typedef union _INT_VAL
  {
    int  Val;
//    uint32_t  Val;
    char v[4];
    struct
    {
        char LB;
        char HB;
        char UB;
        char XB;
    } byte;
  } INT_VAL;
// =============================================================================================
  typedef union _WORD_VAL
  {
    uint16_t  Val;
    char v[2];
    struct
    {
        char LB;
        char HB;
    } byte;
  } WORD_VAL;
// =============================================================================================
#define MAXDEV 2560
// =============================================================================================
char busdevType[MAXDEV] = {0};
// =============================================================================================
int		fduart = -1;
struct termios tios_bak;
struct termios tios;
int    keepRunning = 1;
// =============================================================================================
FILE   *fConfig;
char	filename[64];
int  timeToClose = 0;
char    rx_buffer[256];
int     rx_len;
int     rx_max = 250;
char	my_busid = 0;
char	extended = 0;
// =============================================================================================
void rxBufferLoad(int tries);
void mSleep(int millisec);
char axTOchar(char * aData);
// =============================================================================================
char axTOchar(char * aData)
{
char *ptr;
long ret;
    ret = strtoul(aData, &ptr, 16);
    return (char) ret;
}
// =============================================================================================






// =============================================================================================
void intHandler(int sig) {
	(void) sig;
    keepRunning = 0;
//	printf("\nCaught signal %d\n", sig); 
	printf("\nINTERRUPTED\n");
//	sprintf(rxbuffer,"\nCaught signal %d\n", sig); 
}
// ===================================================================================
//void intHandlerB(int sig) {
//	printf("\nCaught signal %d\n", sig); 
//	sprintf(rxbuffer,"\nCaught signal %d\n", sig); 
//}
// ===================================================================================



// ===================================================================================
void mSleep(int millisec) {
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = millisec * 1000000L;
    nanosleep(&req, (struct timespec *)NULL);
}
// ===================================================================================
void uSleep(int microsec) {
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = microsec * 1000L;
    nanosleep(&req, (struct timespec *)NULL);
}
// ===================================================================================

// ===================================================================================
static void print_usage(const char *prog)	// NOT USED
{
	printf("Usage: %s [-v -ixx]\n", prog);
	puts("  -v   --verbose \n");
	puts("  -ixx --mybusid (xx) \n");
	exit(1);
}
// ===================================================================================
static char parse_opts(int argc, char *argv[])	// NOT USED
{
	if ((argc < 1) || (argc > 3))
	{
		print_usage(PROGNAME);
		return 3;
	}

	while (1) {
		static const struct option lopts[] = {
//------------longname---optarg---short--      0=no optarg    1=optarg obbligatorio     2=optarg facoltativo
			{ "verbose",    0, 0, 'v' },
			{ "mybusid",    1, 0, 'i' },
			{ "help",		0, 0, '?' },
			{ NULL, 0, 0, 0 },
		};
		int c;
		c = getopt_long(argc, argv, "v i:h ", lopts, NULL);
		if (c == -1)
			return 0;

		switch (c) {
		case 'h':
			print_usage(PROGNAME);
			break;
		case 'v':
			verbose=1;
			printf("Verbose\n");
			break;
		case 'i':
			if (optarg) 
				my_busid=axTOchar(optarg);
			printf("my bus id: 0x%X-\n",my_busid);
//			i2cbase <<= 4; // da LB a HB
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


// ===================================================================================
int setFirst(void)
{
  char requestBuffer[24];
  int requestLen = 0;
  int n;

  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 0x15; // evita memo in eeprom (in 0x17)

  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 'o'; 

  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 0xF1; // set led lamps std-freq (client mode)

  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 'l'; // (in 0x17)

  requestBuffer[requestLen++] = '@'; 
  requestBuffer[requestLen++] = 'M'; // (in 0x17)
  requestBuffer[requestLen++] = 'X'; // (in 0x17)
  n = write(fduart,requestBuffer,requestLen);			// scrittura su scsgate

  mSleep(50);
  rx_len = 0;
  rxBufferLoad(100);

  requestLen = 0;
  requestBuffer[requestLen++] = '@';
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
void writeFile(void)
{
	strcpy(filename,"discovered");
	fConfig = fopen(filename, "wb");
	if (!fConfig)
	{
	  printf("\nfile open error...");
	  exit(EXIT_FAILURE);
	}
	int i;

	fprintf(fConfig,"{\"coverpct\":\"false\",\"devclear\":\"true\"}\n"); 

	for (i=0;i<MAXDEV;i++)
	{
		switch(busdevType[i])
		{
				// 0x01:switch       0x03:dimmer   
				// 0x08:tapparella   0x09:tapparella pct   
				// 0x0B generic		 0x0C generic 
				// 0x0E:allarme		 0x0F termostato
				// 0x30-0x37:rele i2c
				// 0x40-0x47:pulsanti i2c
		case 1:
			fprintf(fConfig,"{\"device\":\"%02X\",\"type\":\"1\",\"descr\":\"switch %02x\"}\n",i,i); 
			break;
		case 3:
			fprintf(fConfig,"{\"device\":\"%02X\",\"type\":\"3\",\"descr\":\"dimmer %02x\"}\n",i,i); 
			break;
		case 8:
			fprintf(fConfig,"{\"device\":\"%02X\",\"type\":\"8\",\"maxp\":\"\",\"descr\":\"tapparella %02x\"}\n",i,i); 
			break;
		case 15:
			fprintf(fConfig,"{\"device\":\"%02X\",\"type\":\"15\",\"maxp\":\"\",\"descr\":\"termostato %02x\"}\n",i,i); 
			break;
		}
	}
	fclose(fConfig);
	fConfig = NULL;
}
// ===================================================================================
void writeFile2(void)
{
	strcpy(filename,"discovered");
	fConfig = fopen(filename, "wb");
	if (!fConfig)
	{
	  printf("\nfile open error...");
	  exit(EXIT_FAILURE);
	}
	INT_VAL  busix;
	busix.Val = 0;
	INT_VAL i;

	fprintf(fConfig,"{\"coverpct\":\"false\",\"devclear\":\"true\",\"extended\":\"true\",\"busid\":\"%02X\"}\n",my_busid); 

	for (i.Val=0;i.Val<MAXDEV;i.Val++)
	{
		busix.byte.LB = i.byte.LB;
		busix.byte.HB = i.byte.HB;
		switch(busdevType[i.Val])
		{
				// 0x01:switch       0x03:dimmer   
				// 0x08:tapparella   0x09:tapparella pct   
				// 0x0B generic		 0x0C generic 
				// 0x0E:allarme		 0x0F termostato
				// 0x30-0x37:rele i2c
				// 0x40-0x47:pulsanti i2c
		case 1:
			fprintf(fConfig,"{\"device\":\"%04X\",\"type\":\"1\",\"descr\":\"switch %04x\"}\n",busix.Val,busix.Val); 
			break;
		case 3:
			fprintf(fConfig,"{\"device\":\"%04X\",\"type\":\"3\",\"descr\":\"dimmer %04x\"}\n",busix.Val,busix.Val); 
			break;
		case 8:
			fprintf(fConfig,"{\"device\":\"%04X\",\"type\":\"8\",\"maxp\":\"\",\"descr\":\"tapparella %04x\"}\n",busix.Val,busix.Val); 
			break;
		case 15:
			fprintf(fConfig,"{\"device\":\"%04X\",\"type\":\"15\",\"maxp\":\"\",\"descr\":\"termostato %04x\"}\n",busix.Val,busix.Val); 
			break;
		}
	}
	fclose(fConfig);
	fConfig = NULL;
}
// ===================================================================================
void rxBufferLoad(int tries)
{
	int r;
	int loop = 0;
	char sbyte;
	rx_len = 0;
    while ((rx_len < rx_max) && (loop < tries))
    {
		r = -1;
		r = read(fduart, &sbyte, 1);
	    if ((r > 0) && (rx_len < rx_max))
		{
			rx_buffer[rx_len++] = sbyte;
			loop = 0;
		}
		else
			loop++;
		uSleep(90);
    }
    rx_buffer[rx_len] = 0;        // aggiunge 0x00
}
// ===================================================================================


// ===================================================================================
void niceEnd(void)
{
	printf("nice end\n");
}
// ===================================================================================
int main(int argc, char *argv[])
{
	printf(PROGNAME "\n");

	if (parse_opts(argc, argv))
		return 0;

	printf("UART_Initialization\n");
	
	fduart = open("/dev/serial0", O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
	if (fduart == -1) 
	{
		perror("open_port: Unable to open /dev/serial0 - ");
		return(-1);
	}

	struct termios options;
	tcgetattr(fduart, &options);
//	cfsetispeed(&options, B115200);
//	cfsetospeed(&options, B115200);

	options.c_cflag = B115200 | CS8 | CLOCAL | CREAD | CSTOPB;		//<Set baud rate - 2 bits stop?
	options.c_iflag = IGNPAR;
	options.c_oflag = 0;
	options.c_lflag = 0;
	tcflush(fduart, TCIFLUSH);

	tcsetattr(fduart, TCSANOW, &options);

	printf("initialized - OK\n");

	// First write to the port
	int n = setFirst();
 	if (n < 0) 
	{
		perror("Write failed - ");
		return -1;
	}
	else
		printf("write - OK\n");

	for (int i=0; i<MAXDEV; i++) 
	{
		busdevType[i] = 0;
	}

	mSleep(10);
	rxBufferLoad(10);	// discard uart input
	int i;

	
	signal(SIGINT, intHandler);
	/*
	signal(1, intHandlerB);

	signal(3, intHandlerB);
	signal(4, intHandlerB);
	signal(5, intHandlerB);
	signal(6, intHandlerB);
	signal(7, intHandlerB);
	signal(8, intHandlerB);
	*/
//	atexit(niceEnd);
	// ====================================================================================
	while (keepRunning)
	{
		mSleep(1);
		rxBufferLoad(3);	// discard uart input
		if (rx_len > 0) 
		{
			if (verbose)
			{
				fprintf(stderr,"-rx-> ");	// scrittura a video
				for (i=0; i<rx_len; i++)
				{
					fprintf(stderr,"%02X ",rx_buffer[i]);
				}
				fprintf(stderr,"\n");	// scrittura a video
			}

			char busid;
			char busrequest = 0xFF;
			char buscommand = 0;
			char busdevice = 0;
			char busdestination = 0;
			char devtype = 0;
			INT_VAL  busix;
			busix.Val = 0;

			if ((rx_len > 6) && (rx_buffer[0] == 7) && (rx_buffer[1] == 0xA8))  
			{
// 07 A8 B8 31 12 01 9A A3
				busid = my_busid;
				busdestination = rx_buffer[2];
				busrequest = rx_buffer[4];
				buscommand = rx_buffer[5];
				if (rx_buffer[2] < 0xb0)
					busdevice = rx_buffer[2];
				else
					busdevice = rx_buffer[3];
				if (extended == 1)
					extended = 0;
			}
			else
			if ((rx_len > 10) && (rx_buffer[0] == 0x0B) && (rx_buffer[1] == 0xA8) && (rx_buffer[2] == 0xEC))  
			{
// 0B A8 EC 01 00 00 39 00 12 00 C6 A3
// 0B A8 EC FF 01 F0 B8 39 12 00 71 A3
				busdestination = rx_buffer[6];
				if (rx_buffer[3] == 0xFF)
					busid = rx_buffer[4];
				else
					busid = rx_buffer[3];

				busrequest = rx_buffer[8];
				buscommand = rx_buffer[9];
				if (rx_buffer[6] < 0xb0)
					busdevice = rx_buffer[6];
				else
					busdevice = rx_buffer[7];
				if (extended == 0)
					extended = 1;
			}


			if (busrequest != 0xFF) 
			{
				busix.byte.LB = busdevice;
				if (busid > 10)  busid = 0;
				busix.byte.HB = busid;

				const char * type_descri[] =
				{
					"nn",
					"switch",	// 1
					"nn",
					"dimmer",	// 3
					"nn",
					"nn",
					"nn",
					"nn",
					"tapparella",	// 8
					"nn",
					"nn",
					"nn",
					"nn",
					"nn",
					"allarme",
					"termostato",
					"nn"
				};

				// 1:switch       3:dimmer   
				// 8:tapparella   9:tapparella pct   
				// 0B generic    0C generic 
				// 0E:allarme	 0F termostato


				if ((busrequest == 0x30) && (busdestination == 0xB4))  // <-termostato----------------------------
				{
					devtype = 0x0F;
					busdevice = rx_buffer[3];
				}
				else
				if ((busrequest == 0x12) && (busdestination != 0xB1))
				{
					if (extended == 1)
						extended = 2;

					if ((buscommand == 0x08) || (buscommand == 0x09) || (buscommand == 0x0A))	// tapparella
						devtype = 8;
					else
					if ((buscommand == 0) || (buscommand == 1))		// switch/luce
						devtype = 1;
					else
					if (((buscommand == 0x43) || (buscommand == 0x44) || (buscommand == 0x49) || (buscommand == 0x4E))  && (rx_buffer[2] == 0xB4))	// allarme
					{
						devtype = 0x0E;
						busdevice = rx_buffer[3];
					}
					else
					if ((buscommand == 3) || (buscommand == 4))		// dimmer
						devtype = 3;
					else
						if ((buscommand & 0x0F) == 0x0D)
						devtype = 3;
				}

				if ((devtype) && (busdevType[busix.Val] == 0))
				{
					if (extended == 1)
						extended = 2;
					busdevType[busix.Val] = devtype;
					if (extended == 2)
						printf(GRN BOLD "NUOVO -> [%X][%02X] - tipo %02u -> %s " NRM "\n",busid, busdevice, devtype, type_descri[(int)devtype]);
					else
						printf(GRN BOLD "NUOVO -> [%02X] - tipo %02u -> %s " NRM "\n",busdevice, devtype, type_descri[(int)devtype]);
				}
				else
				if ((devtype) && (busdevType[busix.Val] == 1) && (devtype == 3))
				{
					busdevType[busix.Val] = devtype;
					if (extended == 2)
						printf(YEL BOLD "VARIATO -> [%X][%02X] - tipo %02u -> %s " NRM "\n",busid, busdevice, devtype, type_descri[(int)devtype]);
					else
						printf(YEL BOLD "VARIATO -> [%02X] - tipo %02u -> %s " NRM "\n",busdevice, devtype, type_descri[(int)devtype]);
				}
				else
				if ((devtype) && (busdevType[busix.Val] != devtype))
				{
					busdevType[busix.Val] = devtype;
					if (extended == 2)
						printf(RED BOLD "INCOERENTE -> [%X][%02X] - tipo %02u -> %s " NRM "\n",busid, busdevice, devtype, type_descri[(int)devtype]);
					else
						printf(RED BOLD "INCOERENTE -> [%02X] - tipo %02u -> %s " NRM "\n",busdevice, devtype, type_descri[(int)devtype]);
				}

			}

		}
	}

  // Don't forget to clean up
	close(fduart);
	if (extended == 2)
		writeFile2();
	else
		writeFile();
	printf("end\n");
	return 0;
}
// ===================================================================================
