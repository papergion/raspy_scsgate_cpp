/*
 *  SCSFIRMWARE - download nuovo firmware su pic - input file .bin
 */
#define PROGNAME "SCS_FIRMWARE "
#define VERSION  "1.20"

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <termios.h>

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

// ===================================================================================
struct termios tios_bak;
struct termios tios;
int	   fduart = 0;
char   force = 0;
// =============================================================================================
  typedef union _WORD_VAL
  {
    unsigned int  Val;
    char v[2];
    struct
    {
        char LB;
        char HB;
    } byte;
  } WORD_VAL;
// ===================================================================================
enum _PICPROG_SM
{
    PICPROG_FREE = 0,
    PICPROG_QUERY_FIRMWARE,
    PICPROG_QUERY_WAIT,
    PICPROG_QUERY_OK,
    PICPROG_QUERY_KO, 
    PICPROG_START,
    PICPROG_REQUEST_WAIT,
    PICPROG_REQUEST_OK,
    PICPROG_FLASH_BLOCK_START,
    PICPROG_FLASH_BLOCK,
    PICPROG_FLASH_WAIT,
    PICPROG_FLASH_SYNCH,
    PICPROG_FLASH_END,
    PICPROG_ERROR
} sm_picprog = PICPROG_FREE;
// =============================================================================================
char	sbyte;
char    rx_prefix;
char    rx_buffer[255];
int     rx_len;
int     rx_max = 250;
char    rx_internal;

WORD_VAL prog_address;
int     prog_error;
int     prog_retry = 0;
#define PICBUF 64
char    prog_file_data[PICBUF];
FILE   *picFw;
char	filename[64];
char	ValidResponse;
int		fwTimeout;
int		fwRetry;
// ===================================================================================
char   verbose = 0;	
char   prog_mode = 3;	// programmazione di prova
// ===================================================================================
void rxBufferLoad(int tries);
char aConvert(char * aData);
static void print_usage(const char *prog);
static char parse_opts(int argc, char *argv[]);
void mSleep(int millisec);
void uSleep(int microsec);
void MsgPrepareFirmware(char len, char * buffer, char log);
char PicProg(void);
// ===================================================================================
char aConvert(char * aData)
{
char *ptr;
long ret;
    ret = strtoul(aData, &ptr, 16);
    return (char) ret;
}
// ===================================================================================
static void print_usage(const char *prog)
{
	printf("Usage: %s [-fuv]\n", prog);
	puts("  -f --file     firmware file name (bin)\n"
		 "  -u --update   true firmware update\n");
	exit(1);
}
// ===================================================================================
static char parse_opts(int argc, char *argv[])
{
	if ((argc < 2) || (argc > 4))
	{
		print_usage(PROGNAME);
		return 3;
	}

	while (1) {
		static const struct option lopts[] = {
			{ "file",      1, 0, 'f' },
			{ "update",    2, 0, 'u' },
			{ "verbose",   0, 0, 'v' },
			{ NULL, 0, 0, 0 },
		};
		int c;

		c = getopt_long(argc, argv, "D:f:u::v ", lopts, NULL);

		if (c == -1)
			break;

		switch (c) {
		case 'f':
			strcpy (filename,optarg);
            printf("filename: %s\n", filename); 
			break;
		case 'u':
            printf("true UPDATE\n"); 
			prog_mode = 2;	// programmazione vera
			if (optarg) 
				force=1;
			break;
		case 'v':
            printf("Verbose\n"); 
			verbose = 1;	// programmazione vera
			break;
		case ':':
			printf("Option -%c requires a value.\n", optopt);
			return 1;		

		case '?':
			if ((optopt == 'D') || (optopt == 'f'))
				fprintf (stderr, "Option -%c requires an argument.\n", optopt);
			else 
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
//	options.c_cflag = B115200 | CS8 | CLOCAL | CREAD;		//<Set baud rate
	options.c_cflag = B115200 | CS8 | CLOCAL | CREAD | CSTOPB;		//<Set baud rate - 2 bits stop?
	options.c_iflag = IGNPAR;
	options.c_oflag = 0;
	options.c_lflag = 0;
	tcflush(fduart, TCIFLUSH);
	tcsetattr(fduart, TCSANOW, &options);
}
// ===================================================================================
int setFirst(void)
{
  char requestBuffer[24];
  int requestLen = 0;
  int n;

  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 0x15; // evita memo in eeprom (in 0x17)

  requestBuffer[requestLen++] = '§';
  requestBuffer[requestLen++] = 'Q'; 
  requestBuffer[requestLen++] = 'Q'; 

  n = write(fduart,requestBuffer,requestLen);			// scrittura su scsgate

  rx_max = 16;
  rx_len = 0;
  rxBufferLoad(100);
  if ((memcmp(rx_buffer,"SCS ",3) == 0) || (force))
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
void  LogBufferTx(char * requestBuffer, int requestLen)
{
	fprintf(stderr,"tx-> ");
	for (int r=0; r<requestLen; r++)
	{
		fprintf(stderr,"%02x ", *requestBuffer++);
	}
	fprintf(stderr,"\n");
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
			if (verbose)
			{
				if (rx_len == 0) fprintf(stderr,"rx<- ");
				fprintf(stderr,"%02x ", sbyte);	// scrittura a video
			}
			rx_buffer[rx_len++] = sbyte;
			loop = 0;
		}
		else
			loop++;
		uSleep(90);
    }
    rx_buffer[rx_len] = 0;        // aggiunge 0x00
	if ((verbose) && (rx_len)) fprintf(stderr," \n");	// scrittura a video
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
int main(int argc, char *argv[])
{
	int loopWait = 0;

//	printf("%s" PROGNAME VERSION "\n%s", RED,NRM);
	printf(CLR WHT BOLD UNDER PROGNAME BOLD VERSION NRM "\n");
	printf("===================================================\n");

	if (parse_opts(argc, argv))
		return 0;

	UART_start();

	picFw = fopen(filename, "rb");
	if (picFw)
	{
		fclose(picFw);
		picFw = NULL;
	}
	else
	{
		//----- FILE NOT FOUND -----
		printf("File not found\n");
		return 0;
	}

	int c = setFirst();
	if (c < 0) 
	{
		perror("Serial0 write failed - ");
		return -1;
	}
	else
		if (verbose) fprintf(stderr,"Serial0 initialized - OK\n");

	mSleep(2);					// pausa
	rx_len = 0;
	rxBufferLoad(10);	// discard uart input
	rx_len = 0;
//--------------------------------------------------------------------------------
    sm_picprog = PICPROG_QUERY_FIRMWARE;
	while (PicProg())
	{
		mSleep(10);
		rxBufferLoad(10);	// discard uart input
        if (rx_len > 0)
        {
			ValidResponse = 1;
		}
	}
//--------------------------------------------------------------------------------

	if (prog_mode == 2)
      printf("PIC flash REAL update\n");
	else
      printf("PIC flash DUMMY update\n");
	printf("confirm [Y/N]?");
	char r=getchar();
	printf("\n===================================================\n");
	if ((r!='y') && (r!='Y'))
		return 0;

	loopWait = 0;
	sm_picprog = PICPROG_START;
	while (PicProg())
	{
		loopWait++;
		if (loopWait > 1000)  loopWait = 0;  // 1 sec
		mSleep(1);
        rx_len = 0;
		rxBufferLoad(10);	// load uart input
        if (rx_len > 0)
        {
			ValidResponse = 1;
		}
	}
	printf("\nwait... \n");

	for (c=0; c<1000; c++)
	{
		mSleep(4);	// pausa 4 secondi
	}
	printf("wait... \n");
	rx_len = 0;
	rxBufferLoad(10);	// discard uart input
	rx_len = 0;
//--------------------------------------------------------------------------------
    sm_picprog = PICPROG_QUERY_FIRMWARE;
	while (PicProg())
	{
		mSleep(10);
		rxBufferLoad(10);	// discard uart input
        if (rx_len > 0)
        {
			ValidResponse = 1;
		}
	}
//--------------------------------------------------------------------------------

	close(fduart);
	return 0;
}
// =====================================================================================================
void MsgPrepareFirmware(char len, char * buffer, char log)
{
	write(fduart,buffer,(int)len);			// scrittura su scsgate
	if (log)
	{
	  LogBufferTx(buffer,len);
	}
	mSleep(1);
}
// =====================================================================================================
void  MsgPrepareQuery(char log)	// cmd_sys 0x10
{
	char requestBuffer[6];
	int requestLen = 0;
    requestBuffer[requestLen++] = '@';
    requestBuffer[requestLen++] = '@';
    requestBuffer[requestLen++] = 'Q';
    requestBuffer[requestLen++] = 'Q';
//    requestBuffer[requestLen++] = 0x11; // prg request
	write(fduart,requestBuffer,requestLen);			// scrittura su scsgate

	if (log)
	{
	  LogBufferTx(requestBuffer,requestLen);
	}
}

// =====================================================================================================
// PicProg
// =====================================================================================================
char PicProg(void)
{
   int s, l, progptr;
   char sBuf[16];
   WORD_VAL block_check;

   char crcdep;
   char crcseq;
   int  crcChk;
   
//======================================================================================================
   switch (sm_picprog)
   {
    case PICPROG_FREE:
	  return 0;
	  break;
//------------------------------------------------------
    case PICPROG_QUERY_FIRMWARE:
   	  MsgPrepareQuery(verbose);	// 00
	  fwTimeout = 3000; // x 1mSec = 3 sec
      sm_picprog = PICPROG_QUERY_WAIT;
	  ValidResponse = 0;
	  break;
//------------------------------------------------------
    case PICPROG_QUERY_WAIT: 
// attesa risposta
	  if (ValidResponse == 1)
	  {
		  ValidResponse = 0;
		  printf("\n========> current version is %.*s  \n\n", rx_len, rx_buffer);
	 	  sm_picprog = PICPROG_QUERY_OK;
      }
	  else
	  {
		  // gestire timeout ...
		  fwTimeout--;
		  if (fwTimeout == 0)
		  {
			  printf("\n========> no current version answer\n\n");
		 	  sm_picprog = PICPROG_QUERY_KO;
		  }
	  }
	  break;
//------------------------------------------------------
    case PICPROG_QUERY_OK: 
	  return 0;
	  break;
//------------------------------------------------------
    case PICPROG_QUERY_KO: 
	  return 0;
	  break;
//------------------------------------------------------
    case PICPROG_START:
	  picFw = fopen(filename, "rb");
	  if (picFw)
	  {
		printf("start programming\n");
	  }
	  else
	  {
	 	  sm_picprog = PICPROG_ERROR;
		  printf("\nfile not found...\n");
		  return 0;
	  }

//	  setbuf(stdout, NULL);
//	  setvbuf(stdout, NULL, _IONBF, 0);

      prog_error = 0;
      prog_retry = 0;
      prog_address.Val = 0;

	  sBuf[0] = 0x11;		// flash request
   	  MsgPrepareFirmware(1, sBuf, verbose);	// 00

	  fwTimeout = 3000; // x 1mSec = 3 sec
      sm_picprog = PICPROG_REQUEST_WAIT;
	  ValidResponse = 0;
	  break;

//------------------------------------------------------
    case PICPROG_REQUEST_WAIT: 
// attesa risposta
	  if (ValidResponse == 1)
	  {
		  ValidResponse = 0;
		  if ((rx_buffer[0] == 0x04) && (rx_buffer[1] == 0x10) && (rx_buffer[2] == 0x07)) 
	           sm_picprog = PICPROG_REQUEST_OK; 
		  else
		  {
		 	  sm_picprog = PICPROG_ERROR;
			  printf("\nPIC answer error at initial CMD_FIRMWARE \n");
		  }
      }
	  else
	  {
		  // gestire timeout ...
		  fwTimeout--;
		  if (fwTimeout == 0)
		  {
		 	  sm_picprog = PICPROG_ERROR;
			  printf("\nTIMEOUT at initial CMD_FIRMWARE \n");
		  }
	  }
	  break;

//------------------------------------------------------
    case PICPROG_REQUEST_OK:
	  l = fread(&prog_file_data[0], sizeof(unsigned char), PICBUF, picFw);
      sm_picprog = PICPROG_FLASH_BLOCK_START;
      while (l < PICBUF)
      {
         prog_file_data[l++] = 0xFF;
      }

//------------------------------------------------------
    case PICPROG_FLASH_BLOCK_START:
//    vb6:  WriteBuf (Chr(Len(sWrite) + 2) + Chr(10) + Chr(7) + sWrite)

// compute checksum      
      block_check.Val = 0;
      crcseq = 0;
      crcChk = 0;
      for (s=0; s < PICBUF; s++)
      {
         block_check.Val += prog_file_data[s];
         crcdep = prog_file_data[s];
         crcseq++;
         if (crcseq > 7) crcseq = 0;
         /*
         shiftloop = crcseq;
         while (shiftloop)
         {
           crcdep = ((crcdep & 0x80)?0x01:0x00) | (crcdep << 1);
           shiftloop--;
         }
         */
         if (crcseq) crcdep = ((crcdep<<crcseq) | (crcdep>>(8-crcseq)));

         crcChk += crcdep;
      }
//	  printf("\ncrc %i ",block_check.Val);

      if ((prog_address.byte.HB == 0xF0) || (block_check.Val == (int)(PICBUF * 255)))
      {
		 printf("\nend of firmware \n");
         sm_picprog = PICPROG_FLASH_END;
         break;
      }
//      block_check.Val = crcChk;  // no - puro checksum

      // block 1: firmware block address
      //   06 10 07 01 00 00 40 

      sBuf[0] = 6;     // data length
      sBuf[1] = 0x10;  // from
      sBuf[2] = 0x07;  // format
      sBuf[3] = 0x01;  // command
      
      sBuf[4] = prog_address.byte.LB;  // address LB
      sBuf[5] = prog_address.byte.HB;  // address HB
      sBuf[6] = 64;    // data block length (command 0x40)

	  if (!verbose) fprintf(stderr,".");
	  MsgPrepareFirmware(7, &sBuf[0],verbose);	// 01000040 (01aaaaLL)
      sm_picprog = PICPROG_FLASH_BLOCK; 
//	  break;  // altrimenti perde il checksum

//------------------------------------------------------
//  case PICPROG_FLASH_BLOCK:
//    vb6:  WriteBuf (Chr(Len(sWrite) + 2) + Chr(10) + Chr(7) + sWrite)
      
// firmware block data (64 bytes)

	  // block 2-3-4-5-6-7-8-9: firmware block data
      //   10 10 07 xx xx xx xx xx xx xx xx 

	  progptr = 0;
      for (s=0; s < 8; s++)
	  {
		sBuf[0] = 10;     // data length
		sBuf[1] = 0x10;  // from
        sBuf[2] = 0x07;  // format
	    MsgPrepareFirmware(3, sBuf, verbose);

	    MsgPrepareFirmware(8, &prog_file_data[progptr],verbose);	// data
		progptr += 8;
      }

// firmware block trailer
      // block 10: firmware block end
      //   05 10 07 02 ck ck
      sBuf[0] = 5;     // data length
      sBuf[1] = 0x10;  // from
      sBuf[2] = 0x07;  // format
      sBuf[3] = prog_mode;  // command 2: true write    3: test
      
      sBuf[4] = block_check.byte.LB;
      sBuf[5] = block_check.byte.HB;

	  MsgPrepareFirmware(6, &sBuf[0],verbose);	// 02DA12 (02cccc)
      sm_picprog = PICPROG_FLASH_WAIT;
	  fwTimeout = 300; // x 1mSec = 0,3 sec
	  fwRetry = 0;
      break;


    case PICPROG_FLASH_WAIT:
// attesa asincrona 
	  if (ValidResponse == 1)
	  {
		ValidResponse = 0;
		if (rx_buffer[0] == 8)
		{
             //   08 10 07 02 00 F1 1D F1 1D  (l=8, destin=10, fmt=07, req=02 write - 03=test )
             //               ^^ d(1)=00 ok   01 protect   >0xF0 error
			if ((rx_buffer[4] == 0) || (rx_buffer[4] == 1) || (prog_mode == 3)) // 0=ok   1=protetto  FF=checksum  FE=errore
			{
			  fprintf(stderr,"k");
              sm_picprog = PICPROG_REQUEST_OK;
              prog_address.Val += PICBUF;
			}
				
			else
 			if (rx_buffer[1] == 0xFF) // r - checksum error - retry
			{
				  fprintf(stderr," r ");
	              sm_picprog = PICPROG_FLASH_BLOCK_START;
			}
			else
			{
              sm_picprog = PICPROG_ERROR;
              printf("\nPIC answer error at BLOCK WRITE\n");
			}
		}
		else
		{
            sm_picprog = PICPROG_ERROR;
            printf("\nPIC answer error at BLOCK WRITE\n");
        }
      }
	  else
	  {
		  // gestire timeout ...
		  fwTimeout--;
		  if (fwTimeout == 0)
		  {
			  if (fwRetry < 20)
			  {
				  fwRetry++;
				  fwTimeout = 300;
				  //   03 10 07 00
				  sBuf[0] = 3;     // data length
				  sBuf[1] = 0x10;  // from
				  sBuf[2] = 0x07;  // format
				  sBuf[3] = 0;     // query
				  MsgPrepareFirmware(6, &sBuf[0],verbose);	// 02DA12 (02cccc)

				  fprintf(stderr," R ");
				  sm_picprog = PICPROG_FLASH_SYNCH;
			  }
			  else
			  {
				  sm_picprog = PICPROG_ERROR;
				  printf("\nTIMEOUT at BLOCK WRITE command \n");
			  }
		  }
	  }
	  break;


    case PICPROG_FLASH_SYNCH:
// attesa asincrona 
	  if (ValidResponse == 1)
	  {
		ValidResponse = 0;
		fprintf(stderr," r ");
	    sm_picprog = PICPROG_FLASH_BLOCK_START;
      }
	  else
	  {
		  // gestire timeout ...
		  fwTimeout--;
		  if (fwTimeout == 0)
		  {
			  if (fwRetry < 20)
			  {
				  fwRetry++;
				  fwTimeout = 300;
				  //   03 10 07 00
				  sBuf[0] = 3;     // data length
				  sBuf[1] = 0x10;  // from
				  sBuf[2] = 0x07;  // format
				  sBuf[3] = 0;     // query
				  MsgPrepareFirmware(6, &sBuf[0],verbose);	// 02DA12 (02cccc)
				  fprintf(stderr," R ");
			  }
			  else
			  {
				  sm_picprog = PICPROG_ERROR;
				  printf("\nTIMEOUT at BLOCK WRITE command \n");
			  }
		  }
	  }
	  break;

    case PICPROG_ERROR:
		printf ("\nABNORMAL END - last addr: %02X%02X\n", prog_address.byte.HB, prog_address.byte.LB);
		sm_picprog = PICPROG_FREE;
		fclose(picFw);
		picFw = NULL;
		return 0;
	    break;

	  
    case PICPROG_FLASH_END:
      //   03 10 07 80
      sBuf[0] = 3;     // data length
      sBuf[1] = 0x10;  // from
      sBuf[2] = 0x07;  // format
      sBuf[3] = 0x80;  // command
	  MsgPrepareFirmware(5, &sBuf[0],verbose);	// 80
      sm_picprog = PICPROG_FREE;
	  fclose(picFw);
	  picFw = NULL;
	  if (prog_mode == 2)
	      printf("\nPIC flash update OK\n");
	  else
	      printf("\ndummy PIC flash ok\n");
	  return 0;
	  break;
	  
    default:
	  break;
   }
   return 1;
}
