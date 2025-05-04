/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	Serial port MS-DOS specifics
*/

#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "spc-os.h"
#include "qlio.h"
#include "ser-os.h"
#include "svasync.h"

static void set_ser_baud(int ch, int baud);

#define MAXSER	2
static	int	ser_open[MAXSER];
static	int	sdev[MAXSER];
static	int	baudrate[MAXSER];

#ifndef O_NONBLOCK
#define O_NONBLOCK O_NDELAY
#endif

#define SERIALDEBUG 1 /* 0, 1, 2 3 */
#define MODEMTEST   0 /* 0 or 1 */


void serial_dtr_on (void);
void serial_dtr_off (void);

void serial_flush_buffer (void);

int serial_readstatus (int);
int serial_writestatus (int, int);

int SERDATR (void);

int  SERDATS (void);
void  SERDAT (int w);

static char outbuf[1024];
static int inptr, inlast, outlast;

int waitqueue=0,
    carrier=0,
    dsr=0,
    dtr=0,
    isbaeh=0,
    doreadser=0,
    serstat=-1;


int serper=0,serdat;

void init_serial(void)
{
int i;
	for(i=0;i<MAXSER;i++) {
		ser_open[i]=0;
		sdev[i]=-1;
		baudrate[i]=0;
	}
}

void exit_serial(void)
{
int i;
	for(i=0;i<MAXSER;i++) {
		close_serial(i);
	}
}


#define COM_SETTINGS    (BITS_8 | STOP_1 | NO_PARITY)

// open a SER# device. SER1: channel 0
void open_serial(int ch, int baudrate, int parity)
{
int com;

	if (ser_open[ch] == 1) return;
	if (ser_open[1-ch] == 1) {
		fpr("Error: Only one serial port can be opened\n");
		return;
	}

	if (strncmp(sername[ch],"com",3)!=0) {
		fpr("Error: Invalid com port: %s\n",sername[ch]);
		return;
	}
	com=sername[ch][3]-'1';

	SVAsyncInit(com);

	SVAsyncFifoInit();

	SVAsyncSet(baudrate, COM_SETTINGS);
	SVAsyncHand(DTR | RTS);


	ser_open[ch]=1;
fpr("SOPEN %d\n",ch);

}

void close_serial(int ch)
{
	if (ser_open[ch] == 0) return;
	SVAsyncStop();
	ser_open[ch]=0;

}

void send_serial_char(int ch, int c)
{
int num;
static char tb[1024];

//fpr("TX-0 %d; ",ch);
	if (!ser_open[ch]) return;
	c&=0x7f;
	SVAsyncOut(c);


//fpr("TX-1: %c\n",c);
}

#define MAXRBUF 1024

// called at 50Hz, simulated thread
void ser_rcv_thread(void)
{
int ch,num,c;
static char rb[MAXRBUF];
static max=0;

	for (ch=0;ch<MAXSER;ch++) {
		if (!ser_open[ch]) continue;
		num=0;
		while ( (c=SVAsyncIn()) >= 0 ) {
			rb[num]=c;
			num++;
			if (num>=MAXRBUF) break;
		}

		if (num<0) {
			fpr("Error rf %d: %d\n",ch,num);
		} else {
			if (num>0) {
				ser_rcv_enqueue(ch,num,rb);
				if (num>max) {
					max=num;
					fpr("RXM: %d\n",max);
				}
				rb[num]=0;
if(0)				fpr("RX: %s\n",rb);
			}
		}
	}
}


/* Not (fully) implemented yet:
 *
 *  -  Something's wrong with the Interrupts.
 *     (NComm works, TERM does not. TERM switches to a
 *     blind mode after a connect and wait's for the end
 *     of an asynchronous read before switching blind
 *     mode off again. It never gets there on UAE :-< )
 *
 *  -  RTS/CTS handshake, this is not really neccessary,
 *     because you can use RTS/CTS "outside" without
 *     passing it through to the emulated Amiga
 *
 *  -  ADCON-Register ($9e write, $10 read) Bit 11 (UARTBRK)
 *     (see "Amiga Intern", pg 246)
 */

void SERDAT (int w)
{
    unsigned char z;

    z = (unsigned char)(w&0xff);

	outbuf[outlast++] = z;
	if (outlast == sizeof outbuf)
	    serial_flush_buffer();

#if SERIALDEBUG > 2
    fpr("SERDAT: wrote 0x%04x\n", w);
#endif

//    serdat|=0x2000; /* Set TBE in the SERDATR ... */
//    intreq|=1;      /* ... and in INTREQ register */
    return;
}

int SERDATR (void)
{

#if SERIALDEBUG > 2
    fpr("SERDATR: read 0x%04x\n", serdat);
#endif
    waitqueue = 0;
    return serdat;
}

void serial_dtr_on(void)
{
#if SERIALDEBUG > 0
	fpr("DTR on.\n");
#endif
	dtr=1;
	open_serial(0,0,0);
}

void serial_dtr_off(void)
{
#if SERIALDEBUG > 0
	fpr("DTR off.\n");
#endif
	dtr=0;
	close_serial(0);
}


void serial_flush_buffer(void)
{
}

int serial_readstatus(int ch)
{
    int status = 0;

#ifdef POSIX_SERIAL
    ioctl (sdev[ch], TIOCMGET, &status);

    if (status & TIOCM_CAR) {
	if (!carrier) {
//	    ciabpra |= 0x20; /* Push up Carrier Detect line */
	    carrier = 1;
#if SERIALDEBUG > 0
	    fpr("Carrier detect.\n");
#endif
	}
    } else {
	if (carrier) {
//	    ciabpra &= ~0x20;
	    carrier = 0;
#if SERIALDEBUG > 0
	    fpr("Carrier lost.\n");
#endif
	}
    }

    if (status & TIOCM_DSR) {
	if (!dsr) {
//	    ciabpra |= 0x08; /* DSR ON */
	    dsr = 1;
	}
    } else {
	if (dsr) {
//	    ciabpra &= ~0x08;
	    dsr = 0;
	}
    }
#endif
    return status;
}

int serial_writestatus (int old, int nw)
{
    if ((old & 0x80) == 0x80 && (nw & 0x80) == 0x00)
	serial_dtr_on();
    if ((old & 0x80) == 0x00 && (nw & 0x80) == 0x80)
	serial_dtr_off();

    if ((old & 0x40) != (nw & 0x40))
	fpr("RTS %s.\n", ((nw & 0x40) == 0x40) ? "set" : "cleared");

    if ((old & 0x10) != (nw & 0x10))
	fpr("CTS %s.\n", ((nw & 0x10) == 0x10) ? "set" : "cleared");

    return nw; /* This value could also be changed here */
}

