/* ---------------------------------------------------------------------------
 * UART test utility - la GPIO seriale va abilitata in raspi-config 
 *      - INTERFACING OPTIONS - P6 SERIAL : login shell NO  
 *      -  serial port hardware enabled YES
 * verifica 	ls /dev/serial*
 * ---------------------------------------------------------------------------*/
#define PROGNAME "SCSMONITOR "
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
char	pduFormat = 0;
char	deviceFrom = 0;
char	my_busid = 0;
char	extended = 0;
// =============================================================================================
struct termios tios_bak;
struct termios tios;
// =============================================================================================
void msleep(int millisec);
// =============================================================================================
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
	printf("Usage: %s [-ixx]\n", prog);
	puts("  -ixx --mybusid (xx) \n"
		 );
	exit(1);
}
// ===================================================================================
static char parse_opts(int argc, char *argv[])	// NOT USED
{
	if ((argc < 1) || (argc > 2))
	{
		print_usage(PROGNAME);
		return 3;
	}

	while (1) {
		static const struct option lopts[] = {
//------------longname---optarg---short--      0=no optarg    1=optarg obbligatorio     2=optarg facoltativo
			{ "mybusid",    1, 0, 'i' },
			{ "help",		0, 0, '?' },
			{ NULL, 0, 0, 0 },
		};
		int c;
		c = getopt_long(argc, argv, "i:h ", lopts, NULL);
		if (c == -1)
			return 0;

		switch (c) {
		case 'h':
			print_usage(PROGNAME);
			break;
		case 'i':
			if (optarg) 
				my_busid=axTOchar(optarg);
			printf("my bus id: 0x%X\n",my_busid);
			extended = 1;
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
void msleep(int millisec) {
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = millisec * 1000000L;
    nanosleep(&req, (struct timespec *)NULL);
}
// ===================================================================================


// ===================================================================================
int main(int argc, char *argv[])
{
	initkeyboard();
	atexit(endkeyboard);
	
	printf(PROGNAME VERSION "\n");
	if (parse_opts(argc, argv))
		return 0;

	printf("UART_Initialization\n");
	int fd = -1;
	
	fd = open("/dev/serial0", O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
	if (fd == -1) 
	{
		perror("open_port: Unable to open /dev/serial0 - ");
		return(-1);
	}

	struct termios options;
	tcgetattr(fd, &options);
//	cfsetispeed(&options, B115200);
//	cfsetospeed(&options, B115200);

	options.c_cflag = B115200 | CS8 | CLOCAL | CREAD;		//<Set baud rate
	options.c_iflag = IGNPAR;
	options.c_oflag = 0;
	options.c_lflag = 0;
	tcflush(fd, TCIFLUSH);

	tcsetattr(fd, TCSANOW, &options);

	printf("\ninitialized - OK\n");

	int  c = 0x15;

	// First write to the port
	int n = write(fd,"@",1);	 
	if (n < 0) 
	{
		perror("Write failed - ");
		return -1;
	}
	else
		printf("\nwrite - OK\n");

	n = write(fd,&c,1);			// per evitare memo setup in eeprom
	n = write(fd,"@MA@o@l",7);	// modalita ascii, senza abbreviazioni, log attivo 
	msleep(10);					// pausa

	if (extended == 1)
	{
		n = write(fd,"@O1",3);		// option (ASCII)
		c = my_busid+'0';			// 
		n = write(fd,&c,1);			// busid (ASCII)
	}
	else
	{
		n = write(fd,"@O0",3);		// option (ASCII)
		c = '0';			// 
		n = write(fd,&c,1);			// busid (ASCII)
	}

	n = write(fd,"@h",2);		// help
	char cb;
	while (1)
	{
		msleep(1);
		n = read(fd, &cb, 1);			// lettura da scsgate
		if (n > 0) 
		    fprintf(stderr,"%c", cb);	// scrittura a video

		c = getinNowait();				// lettura tastiera
		if (c)
		{
			fprintf(stderr, "%c", (char) c);// echo a video
			n = write(fd,&c,1);			// scrittura su scsgate
			if (n < 0) 
			{
				perror("Write failed - ");
				return -1;
			}
		}
	}

  // Don't forget to clean up
	close(fd);
	endkeyboard();
	return 0;
}
// ===================================================================================
