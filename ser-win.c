/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	Win95 interface functions
*/

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "qlvers.h"
#include "spc-os.h"
#include "qlio.h"
#include "winmain.h"
#include "cfg-win.h"
#include "debug.h"

static DWORD CALLBACK serial_thread ( HANDLE h );

static	int	idCommDev;
static	int	quit_s_t;
HANDLE hComPort;
int	ser1open=0;
int	ser2open=0;

/*
void find_my_addr(void (*f)(void));

void find_my_addr(void (*f)(void))
{
void (*a[2])(void);

	a[0]=f;
	fpr("FA :%08lx\n",f);
	fpr("FA&:%08lx\n",&f);
	fpr("FA*:%08lx\n",*f);
	fpr("AA:%08lx\n",a[0]);
	fpr("AA:%08lx\n",a[1]);
}
*/

// open a SER# device. SER1: channel 0
void open_serial(int channel, int baudrate, int parity)
{
DCB	dcb;
int	err;
HANDLE	hserthr;
DWORD	dummy_id;
COMMTIMEOUTS cto = { 2, 1, 1, 0, 0 };

/*
	fpr("a\n");
	find_my_addr(init_bmpi);
	find_my_addr(someplace);
	fpr("b\n");
*/
	hComPort = CreateFile( sername[channel], GENERIC_READ | GENERIC_WRITE, 0, 0,
                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL /* | FILE_FLAG_OVERLAPPED */, 0);

	if (hComPort == INVALID_HANDLE_VALUE) {
		fpr("Error: Could not open Device %s\n",sername[channel]);
		return;
	}

	SetupComm(hComPort, 1024, 128);

	// raise DTR
	if (!EscapeCommFunction(hComPort, SETDTR)) {
        	fpr("Error EscapeCommFunction SETDTR\n");
	}

	// set timeouts
	if(!SetCommTimeouts(hComPort,&cto)) fpr("SetCommTimeouts failed\n");

	// set DCB
	memset(&dcb,0,sizeof(dcb));
	dcb.DCBlength = sizeof(dcb);
	dcb.BaudRate = baudrate;
	dcb.fBinary = 1;
	dcb.fDtrControl = DTR_CONTROL_ENABLE;

	dcb.fOutxCtsFlow = 1;
	dcb.fRtsControl = DTR_CONTROL_HANDSHAKE;

	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.ByteSize = 8;

	if(!SetCommState(hComPort,&dcb)) fpr("SetCommState failed\n");

	err = SetCommState(hComPort,&dcb);
	if (err < 0) {
	    fpr("Error SetCommState\n");
	    return;
	}

	// create thread
	hserthr = CreateThread(NULL,0,serial_thread,hComPort,0,&dummy_id);
	if( hserthr == INVALID_HANDLE_VALUE ) {
		fpr("CreateThread failed\n");
		return;
	}
	CloseHandle(hserthr);  // don't need this handle

	ser1open=1;
}

void close_serial(int channel)
{
	// lower DTR
	if (!EscapeCommFunction(hComPort, CLRDTR)) {
		fpr("Error EscapeCommFunction CLRDTR\n");
	}
	// Purge reads/writes, input buffer and output buffer
	if (!PurgeComm(hComPort, PURGE_TXABORT | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_RXCLEAR)) {
	        fpr("Error PurgeComm\n");
	}

	quit_s_t=1;	// quit thread
	if(!SetCommMask(hComPort,0)) fpr("error rxc\n");

	CloseHandle(hComPort);
	ser1open=0;
}

void send_serial_char(int c)
{
	if(!ser1open)return;
	c&=0x7f;
//	fpr("TX-0;");
	TransmitCommChar(hComPort, c);
//	fpr("TX-1: %c\n",c);
}

// handle the serial receive process
DWORD CALLBACK serial_thread ( HANDLE h )
{
DWORD	read;
char	rb[512];
OVERLAPPED ov;
DWORD dwCommEvent;
DWORD dwRead;
char  chRead;

fpr("serthr init\n");
	quit_s_t=0;

	if (!SetCommMask(h, EV_RXCHAR)) {
		fpr("error rxc\n");
		quit_s_t=1;
	}

	while(!quit_s_t) {
		if (WaitCommEvent(h, &dwCommEvent, NULL)) {
			read=0;
			if(!ReadFile(h,rb,sizeof(rb),&read,NULL)) {
				fpr("Error rf\n");
				quit_s_t=1;
			}
			if (read>0) {
				rb[read]=0;
//				fpr("RX: %s\n",rb);
				ser_rcv_enqueue(1,read,(unsigned char*)rb);
			}
		} else {
			fpr("error wce\n");
			quit_s_t=1;
		}
	}

	SetCommMask(h,0);
fpr("serthr exit\n");
	return 0;
}
