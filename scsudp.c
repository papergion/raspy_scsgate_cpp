/* ---------------------------------------------------------------------------
 * UART test utility - la GPIO seriale va abilitata in raspi-config 
 *      - INTERFACING OPTIONS - P6 SERIAL : login shell NO  
 *      -  serial port hardware enabled YES
 * verifica 	ls /dev/serial*
 * ---------------------------------------------------------------------------*/
#define PROGNAME "SCSUDP "
#define VERSION  "1.00"
//#define KEYBOARD

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
//#include <udpserver.h>

#include <dirent.h>
#include <sys/stat.h>
#include <time.h>


#define PORT 52056
#define MAX_QUEUE 1
#define NAME "SCSserver"

// =============================================================================================
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
char	immediatePicUpdate = 0;
char	verbose = 0;
// =============================================================================================
struct termios tios_bak;
struct termios tios;
int	   fdUart;
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
char aConvert(char * aData);
int  iConvert(char * aData);
static char parse_opts(int argc, char *argv[]);
void mSleep(int millisec);
// =============================================================================================
char devType[256] = {0};
// =============================================================================================
enum _UDP_SM
{
    UDP_LISTENING,
    UDP_CONNECTED,
    UDP_RECEIVED
} sm_udp;
// ===================================================================================
    int server_file_descriptor, new_connection;
    long udpread;
    struct sockaddr_in server_address, client_address;
    socklen_t server_len, client_len;
    int opt = 1;
    char udpBuffer[1024] = {0};
	char udpuart = 0;
// =============================================================================================
char	sbyte;
char    rx_prefix;
char    rx_buffer[255];
int     rx_len;
int     rx_max = 250;
char    rx_internal;
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
char aConvert(char * aData)
{
char *ptr;
long ret;
    ret = strtoul(aData, &ptr, 16);
    return (char) ret;
}
// =============================================================================================
int iConvert(char * aData)
{
char *ptr;
long ret;
    ret = strtoul(aData, &ptr, 16);
    return (int) ret;
}
// =============================================================================================
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






// ===================================================================================
static void print_usage(const char *prog)	// NOT USED
{
	printf("Usage: %s [-uv]\n", prog);
	puts("  -u --picupdate  immediate update pic eeprom \n"
		 "  -v --verbose   \n");
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
			{ "picupdate",  0, 0, 'u' },
			{ "verbose",    0, 0, 'v' },
			{ "help",		0, 0, '?' },
			{ NULL, 0, 0, 0 },
		};
		int c;

		c = getopt_long(argc, argv, "u v h", lopts, NULL);

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
		case 'v':
			verbose=1;
			printf("Verbose\n");
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
void UDP_start(void)
{
    if ((server_file_descriptor = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
	fcntl(server_file_descriptor, F_SETFL, O_NONBLOCK); 

    if (setsockopt(server_file_descriptor, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
//  server_address.sin_addr.s_addr = ("127.0.0.1");
    server_address.sin_port = htons(PORT);

    server_len = sizeof(server_address);

    if (bind(server_file_descriptor, (struct sockaddr *)&server_address, server_len))
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
/*
    if (listen(server_file_descriptor, MAX_QUEUE))
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
	else
		printf("tpc listening\n");
*/
	sm_udp = UDP_LISTENING;
}
// ===================================================================================
void UART_start(void)
{
	printf("UART_Initialization\n");
	fdUart = -1;
	
	fdUart = open("/dev/serial0", O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
	if (fdUart == -1) 
	{
		perror("open_port: Unable to open /dev/serial0 - ");
        exit(EXIT_FAILURE);
	}

	struct termios options;
	tcgetattr(fdUart, &options);
//	cfsetispeed(&options, B115200);
//	cfsetospeed(&options, B115200);
	options.c_cflag = B115200 | CS8 | CLOCAL | CREAD;		//<Set baud rate
	options.c_iflag = IGNPAR;
	options.c_oflag = 0;
	options.c_lflag = 0;
	tcflush(fdUart, TCIFLUSH);
	tcsetattr(fdUart, TCSANOW, &options);
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
void rxBufferLoad(void)
{
	int r;
	int loop = 0;

    while ((rx_len < rx_max) && (loop < 3))
    {
		r = -1;
		r = read(fdUart, &sbyte, 1);
	    if ((r > 0) && (rx_len < rx_max))
		{
//			if (verbose) fprintf(stderr,"%2x ", sbyte);	// scrittura a video
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
void BufferDecode(char * decBuffer, char fileWrite)  // fileWrite 1=write config file
{
  char busid[8];
  char device;
  char devtype = 0;
  char alexadescr[32] = {0};
  char stype[8];

  tcpJarg(decBuffer,"\"device\"",busid);
  if (busid[0] != 0)
  {
	device = aConvert(busid);
	if (device)
	{
	  tcpJarg(decBuffer,"\"descr\"",alexadescr);
	  tcpJarg(decBuffer,"\"type\"",stype);
	  devtype = aConvert(stype);
	  devType[(int)device] = devtype;
         
//				  if ((alexaParam == 'y' ) && (devtype < 18))		// w6 - alexa non ha bisogno dei types 18 19
//				  {
//					String edesc = descrOfIx(deviceX);
//					fauxmo.renameDevice(&edesc[0], &alexadescr[0]);
//				  }

	  if (devtype == 9)			// w6 - aggiorna tapparelle pct su pic
	  {
		char smaxpos[8];
		tcpJarg(decBuffer,"\"maxp\"",smaxpos);

		WORD_VAL maxp;

		maxp.Val = iConvert(smaxpos);

		char requestBuffer[16];
		int  requestLen = 0;
		requestBuffer[requestLen++] = '§';
		requestBuffer[requestLen++] = 'U';
		requestBuffer[requestLen++] = '8';
		requestBuffer[requestLen++] = device;     // device id
		requestBuffer[requestLen++] = devtype;    // device type
		requestBuffer[requestLen++] = maxp.byte.HB;    // max position H
		requestBuffer[requestLen++] = maxp.byte.LB;    // max position L
		write(fdUart,requestBuffer,requestLen);			// scrittura su scsgate

//					immediateReceive('k');
		mSleep(10);					// pausa
		rx_len = 0;
		rxBufferLoad();	// discard uart input
	  }
//				  else
//					maxp.Val = 0;

	  if (fileWrite)
	  {
		fprintf(fConfig,"%s\n",&decBuffer[11]); 
		timeToClose = 2000; // chiude il file dopo 2 secondi
	  }
	} // deviceX > 0
  }  // busid != ""
  else
  {
	  if (fileWrite)
	  {
		  char devclear[8];
		  tcpJarg(decBuffer,"\"devclear\"",devclear);
		  if (strcmp(devclear,"true") == 0)
		  {
			  fConfig = fopen(filename, "wb");
			  if (!fConfig)
			  {
				  printf("\nfile open error...");
				  exit(EXIT_FAILURE);
			  }
		  }  // devclear == "true"
	  }
	  char cover[8];
	  tcpJarg(decBuffer,"\"coverpct\"",cover);
	  if (strcmp(cover,"false") == 0)
	  {
		write(fdUart,"§U9",3);			// scrittura su scsgate

//					immediateReceive('k');
		mSleep(10);					// pausa
		rx_len = 0;
		rxBufferLoad();	// discard uart input
	  }  // cover == "false"

	  if (fileWrite)
	  {
		  fprintf(fConfig,"%s\n",&decBuffer[11]); 
		  timeToClose = 2000; // chiude il file dopo 2 secondi
	  }
  }
}	
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

	UART_start();

	UDP_start();

	if (verbose) printf("initialized - OK\n");

	int  c = 0x15;

	// First write to the port
	int n = write(fdUart,"@",1);	 
	if (n < 0) 
	{
		perror("Write failed - ");
		return -1;
	}
//	else
//		printf("\nwrite - OK\n");

	n = write(fdUart,&c,1);			// per evitare memo setup in eeprom
	n = write(fdUart,"@MA@o@l",7);	// modalita ascii, senza abbreviazioni, log attivo 
	mSleep(10);					// pausa
	rx_len = 0;
	rxBufferLoad();
//	n = write(fdUart,"@q",2);		// help

	char cb;
//	int x = 0;
	strcpy(filename,"scsconfig");
	for (int i=0; i<256; i++) {devType[i] = 0;}

// preload config file ----------------------------------------------------------------------------
	c = 0;
	fConfig = fopen(filename, "rb");
	if (fConfig)
	{
		char line[80];
		while (fgets(line, 80, fConfig))
		{
		  char busid[8];
		  char device;
		  char stype[8];
		  tcpJarg(line,"\"device\"",busid);
		  if (busid[0] != 0)
		  {
			device = aConvert(busid);
			if (device)
			{
			  tcpJarg(line,"\"type\"",stype);
			  devType[(int)device] = aConvert(stype);
			  c++;
			}
		  }
		  if (immediatePicUpdate)
		  {
			BufferDecode(line, 0);  // fileWrite 1=write config file
		  }
		}
		fclose(fConfig);
		fConfig = NULL;
	}

	if (verbose) printf("%d devices loaded from file\n",c);

	char device = 0;
	char devtype;

	(void) devtype;

// ----------------------------------------------------------------------------------------------------
	while (1)
	{
		mSleep(1);
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
		n = read(fdUart, &cb, 1);			// lettura da scsgate
		if (n > 0)
		{
			rx_len = 0;
			rx_buffer[rx_len++] = cb;
		    if (verbose) fprintf(stderr,"%c", cb);	// scrittura a video
			rxBufferLoad();
			if ((tcpuart == 1) && (sm_tcp >= TCP_CONNECTED))
			{
				send(new_connection , rx_buffer, rx_len, 0 );
			}
		}

#ifdef KEYBOARD
		c = getinNowait();				// lettura tastiera
		if (c)
		{
			if (verbose) fprintf(stderr, "%c", (char) c);// echo a video
			n = write(fdUart,&c,1);			// scrittura su scsgate
			if (n < 0) 
			{
				perror("Write failed - ");
				return -1;
			}
		}
#endif

		switch(sm_udp)
		{
		case UDP_LISTENING:
		    client_len = sizeof(client_address);
			if (fConfig)
			{
				fclose(fConfig);
				fConfig = NULL;
			}

//			sm_tcp = TCP_CONNECTING;
//		case TCP_CONNECTING:

			if ((new_connection = accept(server_file_descriptor, (struct sockaddr *)&client_address, &client_len)) > 0)
			{
		        if (verbose) printf("\n%s: got connection from %s on port: %d\n", NAME, inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
				sm_tcp = TCP_CONNECTED;
			}
			break;
		case TCP_CONNECTED:
			tcpread = recv(new_connection, tcpBuffer, 1024, MSG_DONTWAIT);
			if (tcpread == 0)
			{
				if (verbose) printf("CLOSED...\n");
				close(new_connection);
				memset(tcpBuffer, 0, sizeof(tcpBuffer));
				new_connection = 0;
				sm_tcp = TCP_LISTENING;
			}
			else
			if (tcpread > 0)
			{
				if (verbose) printf("%s send request: %s\n", inet_ntoa(client_address.sin_addr), tcpBuffer);
				sm_tcp = TCP_RECEIVED;
			}
			break;


		case TCP_RECEIVED:
		// ------------------------------------------------------------------------------------------------------
			if (memcmp(tcpBuffer, "#request",8) == 0)
		// ------------------------------------------------------------------------------------------------------
			{
			  char busid[8];
			  tcpJarg(tcpBuffer,"\"device\"",busid); // bus id
			  device = aConvert(busid);
			  devtype = devType[(int)device];
		      char sreq[8];
			  char scmd[8];
			  tcpJarg(tcpBuffer,"\"request\"",sreq); // on-off-up-down-stop-nn%
			  tcpJarg(tcpBuffer,"\"command\"",scmd); // 0xnn

			  if (scmd[0] != 0)
			  {
				char requestBuffer[16];
				int  requestLen = 0;
				requestBuffer[requestLen++] = '§';
				requestBuffer[requestLen++] = 'y';   // 0x79 (@y: invia a pic da tcp #request cmd standard da inviare sul bus)

		// comando §y<destaddress><source><type><command>
				requestBuffer[requestLen++] = device; // to   device
				requestBuffer[requestLen++] = 0x00;   // from device
				requestBuffer[requestLen++] = 0x12;   // type:command
				requestBuffer[requestLen++] = aConvert(scmd);// command

				write(fdUart,requestBuffer,requestLen);		// scrittura su scsgate
			  }
			}
			else
// =============================================================================================



// =============================================================================================
		// ------------------------------------------------------------------------------------------------------
			if (memcmp(tcpBuffer, "#putdevice ",11) == 0)   // upload device
		// ------------------------------------------------------------------------------------------------------
			{
			  BufferDecode(tcpBuffer, 1);  // fileWrite 1=write config file
		             
//			  if ((alexaParam == 'y' ) && (devtype < 18))		// w6 - alexa non ha bisogno dei types 18 19
//			  {
//				String edesc = descrOfIx(deviceX);
//				fauxmo.renameDevice(&edesc[0], &alexadescr[0]);
//			  }

			  send(new_connection , "#ok", 3, 0 );
			}	// #putdevice
			else
/*
		// ------------------------------------------------------------------------------------------------------
			if (memcmp(tcpBuffer, "#getdevall",10) == 0) // download devices - non usata
		// ------------------------------------------------------------------------------------------------------
			{
			} // received "#getdevall" - non usata
			else
*/
		// ------------------------------------------------------------------------------------------------------
			if (memcmp(tcpBuffer, "#getdevice",10) == 0)  // download devices
		// ------------------------------------------------------------------------------------------------------
			{
			  char deviceX;
			  char device = 0;

			  deviceX = 1;
			  char busid[8];
/*
			  tcpJarg(tcpBuffer,"\"device\"",busid);	// non usato
			  if (busid[0] != 0)
			  {
			    deviceX = 0;
				device = aConvert(busid);
			  }
			  else
			  tcpJarg(tcpBuffer,"\"afterdev\"",busid); // non usato
			  if (busid[0] != 0)
			  {
				deviceX = aConvert(busid);
				deviceX++;
			  }
			  else
*/
			  tcpJarg(tcpBuffer,"\"devnum\"",busid);
			  if (busid[0] != 0)
			  {
				char line[81] = {0};
				deviceX = aConvert(busid);
				if (deviceX == 1)
				{
					if (fConfig)  fclose(fConfig);
					fConfig = fopen(filename, "rb");
					if (fConfig)
					{
						if (fgets(line, 80, fConfig) == NULL)
						{
							fclose(fConfig);
							fConfig = NULL;
							send(new_connection , "#eof", 4, 0 );
						}
					}
					else
						send(new_connection , "#eof", 4, 0 );
				}
				if (fConfig)
				{
//					char len;
					if (fgets(line, 80, fConfig) == NULL) // legge dal file config e ritorna indietro
					{
						fclose(fConfig);
						fConfig = NULL;
						send(new_connection , "#eof", 4, 0 );
					}
					else
					{
						char stype[4];
						if (verbose) printf("-> %s\n",line);
						send(new_connection , line, 80, 0 );

						// tcpJarg(decBuffer,"\"descr\"",alexadescr);
						tcpJarg(line,"\"type\"",stype);
						devType[(int)device] = aConvert(stype);
					}
				}
			  }
	  
			  else
			  {
				send(new_connection , "#eof", 4, 0 );
			  }
			} // received "#getdevice"
			else
		// ------------------------------------------------------------------------------------------------------
			if (memcmp(tcpBuffer, "#setup ",7) == 0)
		// ------------------------------------------------------------------------------------------------------
			{
			  char debug[18];
			  tcpJarg(tcpBuffer,"\"debug\"",debug);
			  if (strcmp(debug,"tcp") == 0)  
			  {
			    if (verbose) printf("- debug tcp\n");
				tcpuart = 2;
			  }
			  else if (strcmp(debug,"no") == 0)  
			  {
			    if (verbose) printf("- debug no\n");
				tcpuart = 0;
			  }

			  char uart[18];
			  tcpJarg(tcpBuffer,"\"uart\"",uart);
			  if ((strcmp(uart, "tcp") == 0) && (tcpuart == 0))
			  {
			    if (verbose) printf("- uart tcp\n");
				tcpuart = 1;
			  }

			  char ecommit[18];
			  tcpJarg(tcpBuffer,"\"commit\"",ecommit);
			  if (strcmp(ecommit,"true") == 0)
			  {
			    if (verbose) printf("- commit\n");
				if (fConfig)
				{
					fclose(fConfig);
					fConfig = NULL;
				}
			  }

			  send(new_connection , "#ok", 3, 0 );
			}
// ------------------------------------------------------------------------------------------------------
		    else
			{
			  if (tcpuart == 1)
			  {
				n = write(fdUart,tcpBuffer,tcpread);			// scrittura su scsgate
		      }
			}					
// ---------------------------------------------------------------------------------------------------------------------------
			memset(tcpBuffer, 0, sizeof(tcpBuffer));
			sm_tcp = TCP_CONNECTED;
			break;
		default:
			break;
		}
	}

  // Don't forget to clean up
	close(fdUart);
	if (new_connection)	close(new_connection);
#ifdef KEYBOARD
	endkeyboard();
#endif
	return 0;
}
// ===================================================================================
