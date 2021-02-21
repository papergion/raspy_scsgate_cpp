/* ---------------------------------------------------------------------------
 * UART test utility - la GPIO seriale va abilitata in raspi-config 
 *      - INTERFACING OPTIONS - P6 SERIAL : login shell NO  
 *      -  serial port hardware enabled YES
 * verifica 	ls /dev/serial*
 * ---------------------------------------------------------------------------*/
#define PROGNAME "SCSTCP "
#define VERSION  "1.10"
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

#include <dirent.h>
#include <sys/stat.h>
#include <time.h>


#define PORT 5045
#define MAX_QUEUE 1
#define NAME "SCSserver"

int tcpport = PORT;
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
char	firstTime = 0;
// =============================================================================================
struct termios tios_bak;
struct termios tios;
int	   fduart;
// =============================================================================================
FILE   *fConfig;
char	filename[64];
int		timeToClose = 0;
char	httpcallback[128] = {0};
char	httpaddress[18] = {0};
char	httpport[8] = {0};
struct	in_addr httpaddr;
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
int  aTOint(char * aData);
void J1939_Home_Rollcall();
static char parse_opts(int argc, char *argv[]);
void mSleep(int millisec);
void rxBufferLoad(int tries);
int	 waitReceive(char w);
// =============================================================================================
char devType[256] = {0};
// =============================================================================================
enum _TCP_SM
{
    TCP_LISTENING,
    TCP_CONNECTED,
    TCP_RECEIVED
} sm_tcp;
// ===================================================================================
    int server_file_descriptor, new_connection;
    long	tcpread;
    struct	sockaddr_in server_address, client_address, callback_server;
    socklen_t server_len, client_len;
    int		opt = 1;
    char	tcpBuffer[1024] = {0};
	char	tcpuart = 0;
	in_addr	last_address;
	char 	last_response = 0;
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
int aTOint(char * aData)
{
char *ptr;
long ret;
    ret = strtoul(aData, &ptr, 10);
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
	printf("Usage: %s [-upv]\n", prog);
	puts("  -u --picupdate  immediate update pic eeprom \n"
		 "  -p --port   \n"
		 "  -v --verbose   \n");
	exit(1);
}
// ===================================================================================
static char parse_opts(int argc, char *argv[])	// NOT USED
{
	if ((argc < 1) || (argc > 4))
	{
		print_usage(PROGNAME);
		return 3;
	}

	while (1) {
		static const struct option lopts[] = {
			{ "picupdate",  0, 0, 'u' },
			{ "port",       1, 0, 'p' },
			{ "verbose",    0, 0, 'v' },
			{ "help",		0, 0, '?' },
			{ NULL, 0, 0, 0 },
		};
		int c;

		c = getopt_long(argc, argv, "u p:v h", lopts, NULL);
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
		case 'p':
			if (optarg) 
				tcpport=aTOint(optarg);
			printf("Port %d\n",tcpport);
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
void TCP_start(void)
{
    if ((server_file_descriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1)
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
//  server_address.sin_port = htons(PORT);
    server_address.sin_port = htons(tcpport);

    server_len = sizeof(server_address);

    if (bind(server_file_descriptor, (struct sockaddr *)&server_address, server_len))
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_file_descriptor, MAX_QUEUE))
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
	else
		printf("tpc listening\n");
    sm_tcp = TCP_LISTENING;
}
// ===================================================================================
void UART_start(void)
{
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
	options.c_cflag = B115200 | CS8 | CLOCAL | CREAD;		//<Set baud rate
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
int	waitReceive(char w)
{
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
void rxBufferLoad(int tries)
{
	int r;
	int loop = 0;

    while ((rx_len < rx_max) && (loop < tries))
    {
		r = -1;
		r = read(fduart, &sbyte, 1);
	    if ((r > 0) && (rx_len < rx_max))
		{
			if (verbose) fprintf(stderr," %02x", sbyte);	// scrittura a video
			rx_buffer[rx_len++] = sbyte;
			loop = 0;
		}
		else
			loop++;
		uSleep(90);
    }
    rx_buffer[rx_len] = 0;        // aggiunge 0x00
	if ((verbose) && (rx_len)) fprintf(stderr,"(%d)\n",rx_len);	// scrittura a video
}

// ===================================================================================
void bufferPicLoad(char * decBuffer, char fileWrite)  // fileWrite 1=write config file
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

		maxp.Val = aTOint(smaxpos);

		char requestBuffer[16];
		int  requestLen = 0;
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

//		mSleep(10);					// pausa
//		rx_len = 0;
//		rxBufferLoad(10);	// discard uart input
	  }
//				  else
//					maxp.Val = 0;

	  if (fileWrite)
	  {
		fprintf(fConfig,"%s\n",&decBuffer[11]); 
		timeToClose = 1000; // chiude il file dopo 2 secondi
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
		write(fduart,"§U9",3);			// scrittura su scsgate

		if (waitReceive('k') == 0)
			printf("  -->PIC communication ERROR...\n");

//		mSleep(10);					// pausa
//		rx_len = 0;
//		rxBufferLoad(30);	// discard uart input
	  }  // cover == "false"

	  if (fileWrite)
	  {
		  fprintf(fConfig,"%s\n",&decBuffer[11]); 
		  timeToClose = 1000; // chiude il file dopo 2 secondi
	  }
  }
}	

//ripetere setfirst quando serve... (se si richiede un cmd TCP o HTTP ma il modo è tornato ascii
//oppure usare opzione per differenziare uso con vb6 

// ===================================================================================
int setFirst(void)
{
  char requestBuffer[24];
  int requestLen = 0;
  int n;

  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 0x15; // evita memo in eeprom (in 0x17)

//  requestBuffer[requestLen++] = '@';
//  requestBuffer[requestLen++] = 'o'; 

  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 0xF1; // set led lamps std-freq (client mode)

  requestBuffer[requestLen++] = '@'; 
  requestBuffer[requestLen++] = 'M'; // (in 0x17)
  requestBuffer[requestLen++] = 'X'; // (in 0x17)

  requestBuffer[requestLen++] = '@'; 
  requestBuffer[requestLen++] = 'Y'; 
  requestBuffer[requestLen++] = '1';

  requestBuffer[requestLen++] = '@'; 
  requestBuffer[requestLen++] = 'F'; 
  requestBuffer[requestLen++] = '3';

  requestBuffer[requestLen++] = '§';
  requestBuffer[requestLen++] = 'l'; // (in 0x17)
  n = write(fduart,requestBuffer,requestLen);			// scrittura su scsgate

  mSleep(10);
  rx_len = 0;
  rxBufferLoad(100);

  rx_len = 0;
  rx_max = 250;
  firstTime = 1;

  return n;
}
// ===================================================================================
int getNextDevice(char * line)
{
	while(1)
	{
		if (fgets(line, 128, fConfig) == NULL)	// scarta la linea 1
			return 0;
		else
		{
			char busid[8];
			tcpJarg(line,"\"device\"",busid);
			if (busid[0] != 0)
				return 1;
		}
	};
}
// ===================================================================================
int HttpResponse(int connection, int retcode, const char * text)
{
	char httpData[1024];
	sprintf(httpData,"HTTP/1.0 %03d OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s\n",retcode, strlen(text)+1,text);
	send(connection, httpData, sizeof(httpData), 0);
	if (verbose) printf("====> reply: %s \n",httpData);
	return 0;
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

	TCP_start();

	if (verbose) printf("initialized - OK\n");

	int c = setFirst();
	if (c < 0) 
	{
		perror("Write failed - ");
		return -1;
	}

	char cb;
	strcpy(filename,"scsconfig");
	for (int i=0; i<256; i++) {devType[i] = 0;}

// preload config file ----------------------------------------------------------------------------
	c = 0;
	fConfig = fopen(filename, "rb");
	if (fConfig)
	{
		char line[128];
		while (fgets(line, 128, fConfig))
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
		  if (httpcallback[0] == 0)
		  {
			  tcpJarg(line,"\"httpcallback\"",httpcallback);
			  if (httpcallback[0])
			  {
				   tcpJarg(line,"\"httpip\"",httpaddress);
				   tcpJarg(line,"\"httpport\"",httpport);
				   if (inet_aton(httpaddress, &httpaddr) == 0) 
					{
						fprintf(stderr, "Invalid callback IP address\n");
						exit(EXIT_FAILURE);
					}
				  if (verbose) printf("callback %s:%s -> %s\n",inet_ntoa(httpaddr),httpport,httpcallback);
			  }
		  }
		  if (immediatePicUpdate)
		  {
			bufferPicLoad(line, 0);  // fileWrite 1=write config file
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
		mSleep(2);
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
		c = read(fduart, &cb, 1);			// lettura da scsgate
		if (c > 0)
		{
			rx_len = 0;
			rx_buffer[rx_len++] = cb;
		    if (verbose) fprintf(stderr,"->[%02x]", cb);	// scrittura a video
			rxBufferLoad(5);
			if ((tcpuart == 1) && (sm_tcp >= TCP_CONNECTED))
			{
//		        firstTime = 0;
				send(new_connection , rx_buffer, rx_len, 0 );
			}
			
			if ((rx_buffer[0] == 0xF5) && (rx_buffer[1] == 'y') && (rx_len == 6))   // solo se stringa=   [0xF5] [y] 32 00 12 01
			{
				if (((last_response == 'y') || (last_response == 'a')) && (httpcallback[0] != 0))
				{
					char hBuffer[128];
					char pszRequest[1024];

					if (httpport[0] == 0)
					  printf(httpport,"%d",80);

				  // intero  [7] A8 32 00 12 01 21 A3
				  // ridotto [0xF5] [y] 32 00 12 01
				  // ----------------1---2--3--4--5--
					sprintf(hBuffer, "&type=%02X&from=%02X&to=%02X&cmd=%02X", rx_buffer[4], rx_buffer[3], rx_buffer[2], rx_buffer[5]);
					sprintf(pszRequest, "GET /%s%s HTTP/1.1\r\nHost: %s:%s\r\nContent-Type: text/plain\r\n\r\n", httpcallback, hBuffer, httpaddress, httpport);

					if (verbose) printf("<<<<CALLBACK>>>>:\n%s",pszRequest);

					callback_server.sin_family = AF_INET;
					callback_server.sin_port = htons(aTOint(httpport));
					callback_server.sin_addr.s_addr = httpaddr.s_addr;
					 /*
					 * Get a stream socket.
					 */
					int s;
					if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
					{
						printf("Socket error()\n");
						exit(3);
					}
					/*
					 * Connect to the server.
					 */
//					if (verbose) printf("<connect>:\n");
					if (connect(s, (struct sockaddr *)&callback_server, sizeof(callback_server)) < 0)
					{
						printf("Connect error()\n");
						exit(4);
					}

//					if (verbose) printf("<send>:\n");
					if (send(s, pszRequest, sizeof(pszRequest), 0) < 0)
					{
						printf("Send error()\n");
						exit(5);
					}
					/*
					 * The server sends back the same message. Receive it into the
					 * buffer.
					 */
//					if (verbose) printf("<recv>:\n");
//					if (recv(s, pszRequest, sizeof(pszRequest), 0) < 0)
//					{
//						printf("Recv error()\n");
//						exit(6);
//					}
//					else
//						printf("\nRETURN: %s\n",pszRequest);

					/*
					 * Close the socket.
					 */
					close(s);



				}
			}

/* logged callback by esp

SCSserver: got connection from 192.168.2.126 on port: 54858
192.168.2.126 send request: 

GET /json.htm?type=command&param=udevices&script=scsgate_json.lua&type=12&from=11&to=B8&cmd=00 HTTP/1.1
Host: 192.168.2.111:504

mylog:

GET /json.htm?type=command&param=udevices&script=scsgate_json.lua&type=12&from=11&to=B8&cmd=00 HTTP/1.1
Host: 127.0.0.1:8080
Content-Type: text/plain


*/
		}

#ifdef KEYBOARD
		c = getinNowait();				// lettura tastiera
		if (c)
		{
			if (verbose) fprintf(stderr, "%c", (char) c);// echo a video
			n = write(fduart,&c,1);			// scrittura su scsgate
			if (n < 0) 
			{
				perror("Write failed - ");
				return -1;
			}
		}
#endif

		switch(sm_tcp)
		{
		case TCP_LISTENING:
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
//					memorizzare client_address.sin_addr; <------------------------ 
				last_address = client_address.sin_addr;

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
				tcpBuffer[128] = 0;  // PER LIMITARE L'ANALISI AI PRIMI 128 BYTES
				if (verbose) printf("%s send request: %s\n", inet_ntoa(client_address.sin_addr), tcpBuffer);
				sm_tcp = TCP_RECEIVED;
			}
			break;


		case TCP_RECEIVED:
		// ------------------------------------------------------------------------------------------------------
			if (memcmp(tcpBuffer, "#request",8) == 0)
		// ------------------------------------------------------------------------------------------------------
			{
			  if (firstTime == 0) setFirst();
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

				write(fduart,requestBuffer,requestLen);		// scrittura su scsgate
			  }
			}
			else
// =============================================================================================



// =============================================================================================
		// ------------------------------------------------------------------------------------------------------
			if (memcmp(tcpBuffer, "#putdevice ",11) == 0)   // upload device
		// ------------------------------------------------------------------------------------------------------
			{
			  bufferPicLoad(tcpBuffer, 1);  // fileWrite 1=write config file
		             
//			  if ((alexaParam == 'y' ) && (devtype < 18))		// w6 - alexa non ha bisogno dei types 18 19
//			  {
//				String edesc = descrOfIx(deviceX);
//				fauxmo.renameDevice(&edesc[0], &alexadescr[0]);
//			  }

			  send(new_connection , "#ok", 3, 0 );
			}	// #putdevice
			else
		// ------------------------------------------------------------------------------------------------------
			if (memcmp(tcpBuffer, "#getdevice",10) == 0)  // download devices
		// ------------------------------------------------------------------------------------------------------
			{
			  char deviceX = 1;
			  char sdevnum[8];
			  tcpJarg(tcpBuffer,"\"devnum\"",sdevnum);
			  if (sdevnum[0] != 0)
			  {
				char line[128] = {0};
				deviceX = aConvert(sdevnum);
				if (deviceX == 1)
				{
					if (fConfig)  fclose(fConfig);
					fConfig = fopen(filename, "rb");
/*
					if (fConfig)
					{
						if (fgets(line, 128, fConfig) == NULL)	// scarta la linea 1
						{
							fclose(fConfig);
							fConfig = NULL;
							send(new_connection , "#eof", 4, 0 );
						}
					}
					else
						send(new_connection , "#eof", 4, 0 );
*/
				}
				if (fConfig)
				{
					if (getNextDevice(line) == 0)
					{
						fclose(fConfig);
						fConfig = NULL;
						send(new_connection , "#eof", 4, 0 );
					}
					else
					{
						if (verbose) printf("-> %s\n",line);
						send(new_connection , line, 80, 0 );
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
			  else
			  if ((strcmp(uart, "no") == 0) && (tcpuart))
			  {
			    if (verbose) printf("- uart tcp NO\n");
				tcpuart = 0;
			  }
			  send(new_connection , "#ok", 3, 0 );
			}
// ------------------------------------------------------------------------------------------------------
			else
		// ------------------------------------------------------------------------------------------------------
			if (memcmp(tcpBuffer, "GET ",4) == 0)
		// ------------------------------------------------------------------------------------------------------
			{
			  char typ = 0;
			  char from = 0;
			  char device = 0;
			  char cmd = 0;
			  char resp = 'n';

			  tcpBuffer[50]=0; // force termination char
			  char* p1 = strstr(tcpBuffer, "/gate"); // cerca l'argomento /gate?
			  if (p1)
			  {
				char* p2 = strstr(tcpBuffer, "type=");
				if (p2)
					typ = aConvert(p2+5);

				char* p3 = strstr(tcpBuffer, "from=");
				if (p3)
					from= aConvert(p3+5);

				char* p4 = strstr(tcpBuffer, "to=");
				if (p4)
					device  = aConvert(p4+3);

				char* p5 = strstr(tcpBuffer, "cmd=");
				if (p5)
					cmd = aConvert(p5+4);

				char* p6 = strstr(tcpBuffer, "resp=");
				if (p6)
					resp = *(p6+5);
				if (verbose) printf("http get, type=%02x,from=%02x,to=%02x,cmd=%02x,resp=%c \n",typ,from,device,cmd,resp);
				last_response = resp;

				if (firstTime == 0) setFirst();

				char requestBuffer[16];
				int  requestLen = 0;
				requestBuffer[requestLen++] = '§';
				requestBuffer[requestLen++] = 'y';   // 0x79 (@y: invia a pic da tcp #request cmd standard da inviare sul bus)

		// comando §y<destaddress><source><type><command>
				requestBuffer[requestLen++] = device;	// to   device
				requestBuffer[requestLen++] = from;		// from device
				requestBuffer[requestLen++] = typ;		// type:command
				requestBuffer[requestLen++] = cmd;		// command
				write(fduart,requestBuffer,requestLen);		// scrittura su scsgate

			    if ((resp == 'y') || (resp == 'i'))
				{
					HttpResponse(new_connection, 200, "{\"status\":\"OK\"}");
				}
			  }
			  else
				HttpResponse(new_connection, 200, "");

			}
// ------------------------------------------------------------------------------------------------------
			else
			{
			  if (tcpuart == 1)
			  {
				c = write(fduart,tcpBuffer,tcpread);			// scrittura su scsgate
		        firstTime = 0;
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
	close(fduart);
	if (new_connection)	close(new_connection);
#ifdef KEYBOARD
	endkeyboard();
#endif
	return 0;
}
// ===================================================================================
