/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	QL input and output
*/

#include <sys/timeb.h>
#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "qlmem.h"
#include "exe68k.h"
#include "keybuf.h"
#include "spc-os.h"
#include "qldisk.h"
#include "qlio.h"
#include "ser-os.h"

//#include "winmain.h"

#define QDOSTIME (9*365+2)*86400	/* adjust 9 years and 2 days */

/* xternal? */
U8 qliord_b(A32 a);
U32 qliord_l(A32 a);
void qliowr_b(A32 a, U8 d);
void QL_exit(void);

/* internal */
static void do_50Hz(void);
static void do_1Hz(void);
static void do_mdv_motor(void);
static void do_mouse_xm(void);

/* internal */
static void wr8049(A32 addr, U8 data);
static void exec_IPCcmd(int cmd);
static void wrvidcntl(U8 d);
static void wrmdvdata(U8 x);
static void wrmdvcntl(U8 x);
static void wrserdata(U8 x);
static void wrZX8302(U8 x);
static int decode_key(int key);
static void QLwrdisk(void);
static void QLrddisk(void);
static void mdv_next_sect(void);
static void init_mdvs(void);
static int dt_event(int dt_type);
static void mdv_select(int drive);
static U8 rdmouse(A32 a);
static void wrmouse(A32 a, U8 d);
static void init_events(void);
static void eval_next_event(void);
static int sound_process(int);
static void do_tx(void);
static int get_next_ser(void);
static void sclck(int,int);
static void ser_rcv_init(void);
static int ser_rcv_dequeue(int ch);
static int ser_rcv_size(int ch);

static int IPC020=0;	/* current return value for 18020 */
static int IPCreturn;	/* internal 8049 value */
static int IPCcnt;	/* send bit counter */
static int IPCwfc=1;	/* wait for command */
U8	REG18021=0;	/* interrupt control/status register 18021 */
static int ser12oc=0;	/* ser1,2 open: bit0: SER1, bit1: SER2 */
static int IPCbeeping=0;	/* BEEP is sounding */
int	BEEPpars[16];		/* IPC beep parameters */

#define	SERBUFLEN 23
//U8	IPCserbuf[2][SERBUFLEN];/* serial receive buffers from 8049 */
static	int	IPCsercnt=0;		/* number of bytes left */
//static	int	IPCsersptr=0;		/* send to CPU pointer */
static	int	IPCchan=0;	/* 0: SER1, 1: SER2 */
#define	SER_RCV_LEN 2048
static	U8	ser_rcv_buf[2][SER_RCV_LEN];
static	int	ser_rcv_1st[2];
static	int	ser_rcv_fill[2];

static	U8 	Mdirreg=0;	/* mouse direction */
static	U8	Mbutreg=0;	/* mouse buttons */
static	int	Mcount=0;	/* mouse interrupt repeat counter */
static	int	Mavail=0;	/* is the mouse detected, required and available? */
				/* signal when mouse ints should occur */

U32	e50,emdv,emouse,esound,etx;	/* events */

static	int	ZXmode;		/* ZX8302 mode; sertx,net,mdv: bit4,5 of 18002 */
static	int	ZXbaud;		/* ZX8302 sertx baudrate */
static	int	REG18020tx;	/* ZX8302 sertx read register */
static	U32	qlclkoff;	/* QL hardware clock offset */

#define NOSECTS 255
#define SECTLEN (14+14+512+26+120)
#define NUMOFDRIVES 8

typedef struct {
	char name[256];			/* file name */
	int present;			/* is it in? */
	int sector;			/* current sector */
	int wrprot;			/* write protected */
	U8 data[NOSECTS*SECTLEN];	/* mdv drive data */
} mdvt;

mdvt mdrive[NUMOFDRIVES];

U8 *mdv;				/* mdv drive data */
char mdvname[256];			/* mdv file name */
int mdvnum;				/* current mdv */
int mdvpresent;				/* mdv in drive? */
int mdvwp;				/* mdv write protected */
int mdvixb;				/* mdv byte index */
int mdvixs;				/* mdv sector index */
int mdvwrite;				/* mdv r/w */
int mdvmotor;				/* mdv motor running */
int mdvghstate;				/* mdv g/h/g/s progress indicator */
int mdvdoub2;				/* 2 2 mdv command */
int mdvwra;				/* mdv write actions */


/* don't split this into 4 byte reads, it may get interrupted... */
/* 980516: get the local time via GMT offset */
U32 qliord_l(A32 a)
{
struct timeb tb;

	ftime(&tb);
	if (a==0x18000) return (U32) (tb.time-60*tb.timezone+QDOSTIME+qlclkoff);
//	if (a==0x18000) return (U32) (time(0)+QDOSTIME+qlclkoff);

	fpr("RL%x %06x",a,m68k_getpc());
	return -1;
}

/* 980516: set clock 18000, 18001 */
void sclck(int clr, int d)
{
static U32 qlclck;
struct timeb tb;

	if (clr) {
		if (d==0) {
			qlclck=0;
		}
	} else {
		switch(d) {
			case 0xef: qlclck+=0x01000000;break;
			case 0xf7: qlclck+=0x00010000;break;
			case 0xfb: qlclck+=0x00000100;break;
			case 0xfd: qlclck+=0x00000001;break;
			default: fpr("WB18001 %02x ",d);
		}
	}
	ftime(&tb);
	qlclkoff=qlclck-(U32) (tb.time-60*tb.timezone+QDOSTIME);
}

/* process QL io read */
U8 qliord_b(A32 a)
{
int t;
static int pulse=0;
static int bcnt=0;
int p=0,q=0; /*debug*/

/* 0.82c: 18400-18800 is not for nfa! */
	if ( (a>=0x18100) && (a<0x18400) ) return rdnfa(a);
	if ( (a>=0x180c0) && (a<0x18100) ) return rdserpar(a);
	if ((a&0xffffff00)==0x1bf00) return rdmouse(a);
	if (a==0x18021) {
		if(q)fpr("R021:%x:%02x ",m68k_getpc(),REG18021);
		return REG18021;
	}
	if (a==0x18020) {
		if (IPC020!=0) {
			t=IPC020;
			IPC020>>=8;
			if(IPC020==0xa5)IPC020=0; /* clear end marker */
			return (U8)t&0xff;
		}
		{ int t;t=dt_event(0); if (t<7) return(0); }/*970730*/
/*082g*/
		if ((ZXmode&0x10)==0) { /* TXSER mode */
if(0)			fpr("R020tx%d ",REG18020tx);
			return REG18020tx;
		}
		if(mdvmotor==0) { /* no motor running, return NOP */
			return(0x0);
		} else { /* return MDV responses */
			if (!mdvpresent) return(0);
			if (mdvwrite==0) {
			  	if ((mdvghstate==0)||(mdvghstate==2)) {
						/* find GAP?? */
if(p)						fpr("FG%d:%x ",mdvghstate,m68k_getpc());
if(p)						fpr("P%x ",pulse);
						if(pulse==0) {
							if (mdvghstate==0) mdv_next_sect();
/*							fpr("HR%02x ",mdvixs);*/
							pulse++; return(0x8); /*gap*/
						}
						if(pulse<0x18) {pulse++; return(0x0); }
						pulse=0;
						mdvghstate++;
						if (mdvghstate==1) { /* skip preamble */
							mdvixb=12;
						} else mdvixb=40;
						bcnt=0;
						return(0x00);
				}
				if(mdvghstate==1) { /* read header */
					bcnt++;
if(p)					fpr("GB%d:%x ",mdvghstate,m68k_getpc());
if(p)					fpr("BC%03x ",bcnt);
					if (bcnt<0x10) return(0x04); /* read buf read */
					mdvghstate=2;
					bcnt=0;
					return(0x04); /* read buf ready */
				}
				if(mdvghstate==3) { /* read sector */
					bcnt++;
if(p)					fpr("GB%d:%x ",mdvghstate,m68k_getpc());
if(p)					fpr("BC%03x ",bcnt);
					if (bcnt<0x264) return(0x04);
					mdvghstate=0;
					bcnt=0;
					return(0x04);
				}
			}
		}
	}
	if ((a==0x18022)||(a==0x18023)) {
if(p)fpr("*D*%d %02x %03x ",mdvghstate,mdvixs,mdvixb);
		if ((mdvixb==0x2c)&&(mdvdoub2==1)&&(dt_event(1)<125)) {/*970730*/
if(0)fpr("< ");
			mdvixb+=8; /* skip preamb */
			mdvdoub2=0;
		}
		mdvixb=mdvixb%SECTLEN; /* justincase*/
/*		fpr("B%03x %02x ",mdvixb,mdv[mdvixs*SECTLEN+mdvixb]);*/
		mdvixb++;
		return mdv[mdvixs*SECTLEN+mdvixb-1];
	}
	return 0; /* jodo-bu breaks????? */
	/* 0.82c are you still there? */
	return ( qliord_l(0x18000) >> (8*(3-(a&3))) ) & 0xff;
}

/* process QL io write */
void qliowr_b(A32 a, U8 d)
{
int p=0;/*debug*/
	if (a==0x18000) {if(p)fpr("W18000:%02x ",d);sclck(1,d);}
	if (a==0x18001) {if(p)fpr("W18001:%02x ",d);sclck(0,d);}
	if (a==0x18002) wrZX8302(d);
	if (a==0x18003) wr8049(a,d);
	if (a==0x18020) {if(p)fpr("W020:%x:%02x \n",m68k_getpc(),d);wrmdvcntl(d);}
	if (a==0x18021) {
		if(p)fpr("W021:%x:%02x ",m68k_getpc(),d);
		REG18021&=~d;				/* clear interrupts */
return;
		REG18021=(d&0xe0) | (REG18021&0x1f);	/* copy upper 3 bits */
		if ((d&0x20)==0) REG18021&=0xfe;	/* clear interrupt */
	}
	if (a==0x18022) {
		if(p)fpr("W022:%02x ",d);
		if ((ZXmode&0x18)==0x10) wrmdvdata(d);
		else wrserdata(d);
	}
	if (a==0x18023) {if(p)fpr("W023:%02x ",d);}
	if (a==0x18063) wrvidcntl(d);
	if ((a&0xffffff00)==0x1bf00) return wrmouse(a,d);

	/* 98432 */
	if (a==0x18080) {
		if (d==0x55) dump_memory();
	}
	if ( (a>=0x180c0) && (a<0x18100) ) wrserpar(a,d);
	if (a>=0x18100) wrnfa(a,d);
}

static void wrmdvdata(U8 x)
{
	if (!mdvpresent) return; /* is this necessary ? */
	if (mdvwp) return;
/*	if(mdvixb<0x40)fpr("M%02x%02x ",mdvixs,mdvixb);*/
	mdv[mdvixs*SECTLEN+mdvixb]=x;
	mdvwra++;
	mdvixb=(mdvixb+1)%SECTLEN;
}

/*
MDV control
at reset; and after data transfer of any mdv
	(02 00) 8 times			stop all motors
start an mdvN:
	(03 01) (02 00) N-1 times	start motorN
format (erase):
	0a
	(0e 0e) 0a per header and per sector
format (verify):
	02
	(02 02) after finding pulses per header/sector
format (write directory):
	0a (0e 0e) 02 write sector
format (find sector 0):
	(02 02) after pulses
format (write directory):
	0a (0e 0e) 02 write sector
	(02 00) 8 times 		stop motor

read sector:
	(02 02) after pulses
	get 4 bytes
	(02 02) indication to skip PLL sequence: 6*00,2*ff
*/

static void wrmdvcntl(U8 x)
{
static int mdvcnt=0;
static int mdvcntls=0;
static int currdrive=0;
static int mcnt=0;
static int pcntl=-1;
int p=0;/*debug*/
int tmp;

/*process mdv state*/
if(p)	fpr("*M* ");
	tmp=dt_event(1);
	if (tmp>25) { /*970730*/
//	if (tmp>120) { /*zkul980620*/
if(0)		fpr("p%d x%d; %d>25 %ld\n",pcntl,x,tmp,cycle);
		mdvcnt=0;mcnt=0; mdvdoub2=0; pcntl=-1;
	}

	if ((pcntl==2)&&(x==2)) {mdvdoub2=1;}
	if ((pcntl==2)&&(x==0)) {
/*zkul980620*/
//	if ((pcntl==2)&&((x==0)||(x==1))) {
		if (mdvwra) {    /* stopping motor: first write to OS disk now */
			mdvwra=0;
			QLwrdisk();
		}
		if(mcnt){currdrive++;mdv_select(currdrive);}
		mdvcnt++;
/*		fpr("T%d ",cycle);*/
	}
	if ((pcntl==3)&&(x==1)) {mcnt=1;currdrive=1;mdv_select(currdrive);}

	if (mdvcnt==8) { /* stopped mdv motor */
		currdrive=0;mdv_select(currdrive);
	}
	pcntl=x;
/*end process state*/

	switch(x)
	{
	  case 0x00:	/* motor control */
		mdvghstate=0;
		break;
	  case 0x01:	/* motor ?? */
		break;
	  case 0x02:	/* motor control */
		mdvwrite=0;
		break;
	  case 0x03:	/* motor on */

		break;
	  case 0x0a:	/* erase off? */
		if (mdvixb>0x40) { /* somewhere after the sector header */
			mdv_next_sect();
			mdvixb=0;
		}
/*		fpr("H/S%02x ",mdvixs);*/
		break;
	  case 0x0e:  /* erase on? */
		mdvwrite=1;
		break;
	}
}

/* in: drive; 0: no drive, 1..8: select MDVdrive */
static void mdv_select(int drive)
{
	mdvnum=drive;
 	mdvixb=0;
	mdvwrite=0;
	mdvghstate=0;
	mdvdoub2=0;
	mdvwra=0;
	if (drive==0) {
		mdvname[0]='\0';
		mdvpresent=0;
 		mdvixs=0;
		mdvwp=1; /* just in case */
		mdvmotor=0;
	} else {
		drive--; /* structure is one less */
		strcpy(mdvname,mdrive[drive].name);
		mdvpresent=mdrive[drive].present;
		mdvixs=mdrive[drive].sector;
		mdvwp=mdrive[drive].wrprot;
		mdv=mdrive[drive].data;
		mdvmotor=1;
	}
	update_LED();
/*{int i; for(i=0;i<8;i++) fpr("NAME %s ",mdrive[i].name);}*/

}

void update_LED()
{
int i,s;


	draw_LED(10,266,6,1);	/* power on: yellow */
	for(i=0;i<8;i++) {
		if ((i+1)==mdvnum) draw_LED(384+i*16,266,2,1); /* full red */
		else {
			draw_LED(384+i*16,266,0,1);	/* full black */
			if (mdrive[i].present) draw_LED(384+i*16,266,2,0); /* red bar */
		}
	}
}

static void mdv_next_sect()
{
	mdvixs=(mdvixs+1)%NOSECTS;
}

/*18002*/
static void wrZX8302(U8 d)
{

if(0)	fpr("WZX2:%02x:%x ",d,m68k_getpc());

	ZXmode=d&0x18;
if(0)	switch(ZXmode) {
		case 0x0:  fpr("ZXSER1 ");break;
		case 0x8:  fpr("ZXSER2 ");break;
		case 0x10: fpr("ZXMDV ");break;
		case 0x18: fpr("ZXNET ");break;
	}
	switch(d&7){
		case 7: ZXbaud=75; break;
		case 6: ZXbaud=300; break;
		case 5: ZXbaud=600; break;
		case 4: ZXbaud=1200; break;
		case 3: ZXbaud=2400; break;
		case 2: ZXbaud=4800; break;
		case 1: ZXbaud=9600; break;
		case 0: ZXbaud=19200; break;
	}
}

static void wrserdata(U8 d)
{
int ch;
int p=0;

	ch=0;
	if (ZXmode==0x8) ch=1;
	send_serial_char(ch,d);
	if(p)fpr("ST%02x(%c) ",d,d);
	if(p)fpr("ST%ld ",cycle);
	REG18020tx|=0x02;	/* set busy */
	etx=cycle+10000*qlay1msec/ZXbaud; /* clear busy after 10 bits transmitted */
	eval_next_event();
	if(p)fpr("ETX%ld ",etx);
}

static void wrvidcntl(U8 d)
{
/*
18063 is write only (Quasar p.618)
bit 1: 0: screen on, 1: screen off
bit 3: 0: mode 512, 1: mode 256
bit 7: 0: base=0x20000, 1: base=0x28000
*/
/*fpr("VM %02x ",d);*/
	do_scrmode(d);
}

/*
process 8049 commands (p92)
WR 18003: 8049  commands
0:
1: get interrupt status
2: open ser1
3: open ser2
4: close ser1/ser2
5:
6: serial1 receive
7: serial2 receive
8: read keyboard
9: keyrow
a: set sound
b: kill sound
c:
d: set serial baudrate
e: get response
f: test

write nibble to 8049:
'dbca' in 4 writes to 18003: 000011d0, 000011c0, 000011b0, 000011a0.
each write is acknowledged by 8049 with a '0' in bit 6 of 18020.

response from 8049:
repeat for any number of bits to be received (MSB first)
970718
{ write 00001101 to 18003; wait for '0' in bit6 18020; read bit7 of 18020 }

*/

static void wr8049(A32 addr, U8 data)
{
static int IPCrcvd=1;	/* bit marker */
int IPCcmd;

	dt_event(1);	/* mark this here, check delay when reading 18020 */
	if (IPCwfc) {
		if ((data&0x0c)==0x0c) {
			IPCrcvd<<=1;
			IPCrcvd|=(data==0x0c)?0:1;
			if ((IPCrcvd&0x10)==0x10) {
				IPCcmd=IPCrcvd&0x0f;
				IPCrcvd=1;
				IPCwfc=0;
/*printf("C%x ",IPCcmd);*/
				exec_IPCcmd(IPCcmd);
			}
		}
	} else {
		/* expect 0x0e */
		if (data!=0x0e) {
			fpr("ERRORIPC?:%x %x ",m68k_getpc(),data);
		}
/*970718*/	IPC020=0;
		IPCcnt--;
		if(IPCreturn&(1<<IPCcnt)) IPC020|=0x80 ;
		IPC020<<=8;	/* will be read as byte twice, sender will shift back */
		/* lower 16 bit have real value now, put in an end indicator */
		IPC020|=0xa50000;

		if (IPCcnt==0) {/* this command session done */
			if (IPCsercnt) {
				/* serial receive buffer transfer in progress */
				/* prepare next byte */
				IPCreturn=ser_rcv_dequeue(IPCchan);
//				fpr("R: %03x\n",IPCreturn);
				IPCsercnt--;
				IPCcnt=8;
			} else {
				IPCwfc=1; /* wait for next command */
			}
		}
	}
}

static void ser_rcv_init(void)
{
int 	i;
	for(i=0;i<2;i++) {
		ser_rcv_1st[i] = 0;
		ser_rcv_fill[i] = 0;
	}
}

static int ser_rcv_dequeue(int ch)
{
int	next;
//fpr("deq-0 f %d, 1 %d\n",ser_rcv_fill[ch],ser_rcv_1st[ch]);

	if (ser_rcv_fill[ch]==0) {
		fpr("RCV buffer empty\n");
		return '.';
	}
	next=ser_rcv_1st[ch]-ser_rcv_fill[ch];
	if (next < 0) next = next + SER_RCV_LEN;
	ser_rcv_fill[ch]--;
//fpr("deq-1 f %d, 1 %d\n",ser_rcv_fill[ch],ser_rcv_1st[ch]);
	return ser_rcv_buf[ch][next];
}

// called from ser rcv thread
void ser_rcv_enqueue(int ch, int len, U8 *b)
{
int	next,i;

	if (len>SER_RCV_LEN-ser_rcv_fill[ch]) {
		// fpr is NOT fully reentrant
		fpr("RCV buffer overflow; dropping chars\n");
		len=SER_RCV_LEN-ser_rcv_fill[ch];
	}
	ser_rcv_fill[ch]+=len;

	next = ser_rcv_1st[ch];
	for (i=0;i<len;i++) {
		ser_rcv_buf[ch][next] = b[i];
		next++;
		if (next >= SER_RCV_LEN) next = 0;
	}
	ser_rcv_1st[ch] = next;
}

static int ser_rcv_size(int ch)
{
	return ser_rcv_fill[ch];
}

char *KEYS="xv/n,826qe0tu9wi	r-yol3m1apdj[[ksf-g;]z.cb#m`\n00e0\\ 000500047";
static int decode_key(int key)
{
int rk;
char *sp;
/*fpr("%04x ",key);*/
/*	sp=strchr(KEYS,key);
	if(sp!=NULL) rk=(int)sp-(int)&KEYS+3; else rk=key;
*/
	rk=key;
	rk|=0x1000; /* 1 key */
	return rk;
}

static void exec_IPCcmd(int cmd)
{
static int IPCpcmd=0x10; /* previous */
static int IPCbaud=0;

	if (IPCpcmd==0x0d) {/*baudr*/
if(0)		fpr("BRC:%d ",cmd);
		switch(cmd){
		case 7: IPCbaud=75; break;
		case 6: IPCbaud=300; break;
		case 5: IPCbaud=600; break;
		case 4: IPCbaud=1200; break;
		case 3: IPCbaud=2400; break;
		case 2: IPCbaud=4800; break;
		case 1: IPCbaud=9600; break;
		case 0: IPCbaud=19200; break;
		}
if(0)		fpr("BR:%d ",IPCbaud);
		IPCwfc=1;
		cmd=0x10;
	}

	if (IPCpcmd==0x09) { /*keyrow*/
		int row;
		row=cmd;
		IPCreturn=get_keyrow(row);
		IPCcnt=8;
		cmd=0x10;
	}

	if (IPCpcmd==0x0a) { /*sound*/
		static int params=0;
		BEEPpars[params]=cmd;
if(0)		fpr("B%d:%x ",params,cmd);
		params++;
		if (params>15) {
			IPCbeeping=1;
			IPCpcmd=0x10;
			params=0;
			sound_process(1);
		}
		IPCwfc=1;
		return;
	}

/*082d ipc cmd C has 1 nibble parameter, no reply */
	if (IPCpcmd==0x0c) {
		fpr("IPCc'C'par%d ",cmd);
		IPCwfc=1;
		IPCpcmd=0x10;
		return;
	}

	if (IPCpcmd==0x0f) { /*test*/
		static int params=0;
		static int testval=0;
		params++;
		fpr("TP%d:%x ",params,cmd);
		testval=16*testval+cmd;
		if (params>1) {
			fpr("RTV%02x ",testval);
			IPCpcmd=0x10;
			params=0;
			IPCreturn=testval;
			testval=0; /* for next time 'round */
			IPCcnt=8;
			cmd=0x10;
		} else {
			IPCwfc=1;
			return;
		}
	}

	switch(cmd) {
		case 0: /* init */
			fpr("IPC%02x ",cmd);
			break;
		case 1: /* get interrupt status */
/*fpr("GI ");*/
			IPCreturn=0;
			if (keys_available()||k_press()) IPCreturn|=0x01;
			if (use_debugger||fakeF1) IPCreturn|=0x01; /* fake kbd interrupt */
			if (IPCbeeping) IPCreturn|=0x02;
			if (ser12oc&0x01) { /* SER1 */
				if (ser_rcv_size(0)>0) {
					IPCreturn|=0x10;
				}
			}
			if (ser12oc&0x02) { /* SER2 */
				if (ser_rcv_size(1)>0) {
					IPCreturn|=0x20;
				}
			}
			IPCcnt=8;
			break;
		case 2:	/* SER1 open */
		case 3: /* SER2 open */
		case 4: /* SER1 close */
		case 5: /* SER2 close */
			if (cmd==2) {
				ser12oc|=1;
				fpr("Open Ser 1, ZXb: %d\n",ZXbaud);
				open_serial(0,ZXbaud,0);
			}
			if (cmd==3) {
				ser12oc|=2;
				fpr("Open Ser 2, ZXb: %d\n",ZXbaud);
				open_serial(1,ZXbaud,0);
			}
			if (cmd==4) {
				ser12oc&=0xfe;
				fpr("Close Ser 1\n");
				close_serial(0);
			}
			if (cmd==5) {
				ser12oc&=0xfd;
				fpr("Close Ser 2\n");
				close_serial(1);
			}
			fpr("IPC %02x, SER12OC %02x ",cmd,ser12oc);
			IPCwfc=1;
			break;
		case 6: /* SER1 rcv */
		case 7:	/* SER2 rcv */
			/* only called when 8049 sent a SER RCV interrupt */
if(0)			fpr("IPCrcv%02x ",cmd);
			IPCchan=cmd&0x01;
//			IPCsersptr=0;
			IPCsercnt=ser_rcv_size(IPCchan);
			if (IPCsercnt>20) IPCsercnt=20;
			IPCreturn=IPCsercnt;
			IPCcnt=8;
			break;
		case 8: /* read keyboard */
		  { int key;static int lastkey=0;
/*fpr("C8 ");*/
			IPCreturn=0;
			IPCcnt=4;
			if (keys_available()) {/* just double check */
				key=get_next_key();
				lastkey=key;
/*fpr("Dec %x ",key);*/
				IPCreturn=decode_key(key);
				IPCcnt=16;
			} else {
				if (k_press()) {/* still pressed: autorepeat */
/*fpr("Dec KP ");*/
					IPCreturn=0x8; /* just the repeat bit */
					IPCcnt=4;
					/*970705: not good!*/
/*					IPCreturn=decode_key(lastkey);
					IPCreturn|=0x8000;
					IPCcnt=16;
*/
				}
			}
			if(use_debugger||fakeF1) {
				IPCreturn=0x1039; /* 1 char, no spec.key, F1=39, F2=3b */
				IPCcnt=16;
				fakeF1--;
			}
			break;
		  }
		case 0x9: /* keyrow */
/*			fpr("KEYR ");*/
			IPCwfc=1;
			break;
		case 0xa: /* set sound */
if(0)			fpr("BEEP ");
			IPCwfc=1;
			break;
		case 0xb: /* kill sound */
if(0)			fpr("KILLBEEP ");
			IPCbeeping=0;
			IPCwfc=1;
			break;
		case 0xc: /* test interrupt 2?? */
			fpr("IPCcINT ");
			IPCwfc=1;
			break;
		case 0xd: /* set baud rate */
/*			fpr("BRATE ");*/
			IPCwfc=1;
			break;
		case 0xe: /* get random, return 16 bits */
			IPCreturn=0xabcd; /* can do better.. */
			IPCcnt=16;
			fpr("IPCrnd %04x ",IPCreturn);
			IPCwfc=1;
			break;
		case 0xf: /* test, return received byte */
			fpr("IPCtest ");
			IPCwfc=1;
			break;
		case 0x10: /* fake: no more parameters */
			break;
		default:
			fpr("ERRORunexpIPCcmd:%x ",cmd);
			IPCreturn=0;
			IPCcnt=4;
			break;
	}
	IPCpcmd=cmd;
}

/* return how many cycles past previous dt_event() call */
/* dt_type=0 will read only since last update; 1 will update time */
static int dt_event(int dt_type)
{
static U32 prevevent=0x80000000;
U32 rv;
				/* this is wrong?? see QABS 970801 */
	if (cycle<prevevent) rv=prevevent-cycle; else rv=cycle-prevevent;
/*fpr("DT%d ",rv);*/
	if (dt_type) prevevent=cycle;
	return rv;
}

void init_QL()
{
	init_mdvs();
	mdv_select(0);
	init_events();
	ZXmode=0;
	ZXbaud=9600;
	REG18020tx=0;
	ser_rcv_init();
	start_speaker();
	qlclkoff=0;
	cycle=0;
	cycle_next_event=0;
}

void QL_exit()
{
}

static void init_mdvs()
{
char *p;
int i,nod;

	for(i=0;i<NUMOFDRIVES;i++){
		mdrive[i].name[0]='\0';
		mdrive[i].present=0;
		mdrive[i].sector=0;
		mdrive[i].wrprot=0;

		if(strlen(mdvfn[i])>0) {
			if(*mdvfn[i]=='R') {				 /* R as first filename letter -> WP */
				if(strlen(mdvfn[i])==1) continue;
				mdrive[i].wrprot=1;
				p=mdvfn[i];
				while(*(p+1)) {*p=*(p+1);p++;} /* chop R */
				*p='\0';
			}
			strcpy(mdrive[i].name,mdvfn[i]);
			mdv=mdrive[i].data;
			mdvpresent=1;
			strcpy(mdvname,mdvfn[i]);
			QLrddisk();
			mdrive[i].present=mdvpresent;
		}
	}
	nod=0;
	for(i=0;i<NUMOFDRIVES;i++)
		if (mdrive[i].present==1)nod++;
	if (nod==0) fpr("No micro drives found\n");
}

/* write diskimage in mdv[] to a file */
static void QLwrdisk()
{
FILE	*qldisk;
int	addr;

//fpr("Try write %s ",mdvname);
	if (mdvwp) return; /* don't write to OS */
	qldisk=fopen(mdvname,"wb");
	for(addr=0;addr<NOSECTS*SECTLEN;addr++) {
		putc(mdv[addr],qldisk);
	}
	fclose(qldisk);
//fpr("Wrote %s ",mdvname);
}

/* read diskimage from file to mdv[] */
static void QLrddisk()
{
FILE	*qldisk;
int	i;

/*fpr("Try read %s ",mdvname);*/
	qldisk=fopen(mdvname,"rb");
	if (qldisk==NULL) {
		mdvpresent=0;
		fpr("Cannot open %s\n",mdvname);
		return;
	}
	i = fread(mdv, 1, NOSECTS*SECTLEN, qldisk);
	fclose(qldisk);
/*fpr("Read %s ",mdvname);*/
}

void init_events()
{
	e50=emdv=emouse=esound=etx=0;
}

void eval_next_event()
{
/*
U32	d50,dmdv,dmouse,dsound,dtx;
#define QMIN(a,b) ( (a)<(b) ? (a) : (b) )
#define QABSD(a,b) ( (a)-(b) )

	d50=QABSD(e50,cycle);
	dmdv=QABSD(emdv,cycle);
	dmouse=QABSD(emouse,cycle);
	dsound=QABSD(esound,cycle);
	dtx=QABSD(etx,cycle);
	cycle_next_event=cycle+QMIN(QMIN(QMIN(dsound,dtx),d50),QMIN(dmouse,dmdv));
*/
U32	dc,dmin;
	dmin=e50-cycle;
	dc=emdv-cycle;
	if (dc<dmin) dmin=dc;
	dc=emouse-cycle;
	if (dc<dmin) dmin=dc;
	dc=esound-cycle;
	if (dc<dmin) dmin=dc;
	dc=etx-cycle;
	if (dc<dmin) dmin=dc;
	cycle_next_event=cycle+dmin;
}

/* a simple brute force implementation that works just fine ... */
void do_next_event()
{
int	e;

	e=0;
	if (cycle==e50) { /* 50Hz interrupt, 20 msec */
		do_50Hz();
/*		do_1Hz();*/
		e50=cycle+20*qlay1msec;
		e++;
		if(0)fpr("I5 ");
	}
	if (cycle==emdv) { /* MDV gap interrupt, 31 msec */
		if (regs.intmask < 2 ) {
			do_mdv_motor();
		}
		emdv=cycle+31*qlay1msec;
		e++;
		if(0)fpr("IM ");
	}
	if (cycle==emouse) { /* mouse interrupt, x msec */
		do_mouse_xm();
		emouse=cycle+2*qlay1msec;
		e++;
		if(0)fpr("IMo ");
	}
	if (cycle==esound) {
		esound=cycle+sound_process(0);
		e++;
		if(0)fpr("IS ");
	}
	if (cycle==etx) {
		do_tx();
		etx=cycle-1; /* nothing more */
		e++;
		if(0)fpr("ITX ");
	}

	/* decide who's next */
	eval_next_event();

	if (!e) {
		fpr("\nError NoE c: %d ",cycle);
		fpr("e: %d %d %d %d %d\n",e50,emdv,emouse,esound,etx);
	}
}

/* if 'initbeep' first init BEEP parameters */
/* return number of ticks till next process; -1 for stop process */
static int sound_process(int initbeep)
{
/* these are the SuperBasic equivalent values */
static int dur,pitch1,pitch2,gradx,grady,wrap,random,fuzzy;
static int dur_decr,cpitch,cpstep,cpdelay,us72;
int rv;

	if (!IPCbeeping) return -1;
	if (initbeep) {
		pitch1=BEEPpars[0]*16+BEEPpars[1];if(pitch1==0)pitch1=256;
		pitch2=BEEPpars[2]*16+BEEPpars[3];if(pitch2==0)pitch2=256;
		gradx=((BEEPpars[6]*16+BEEPpars[7])*16+BEEPpars[4])*16+BEEPpars[5];
		dur=((BEEPpars[10]*16+BEEPpars[11])*16+BEEPpars[8])*16+BEEPpars[9];
		if (dur==0) dur_decr=0; else dur_decr=1;
		grady=BEEPpars[12];
		if(grady>7)grady=16-grady;
		wrap=BEEPpars[13];
		random=BEEPpars[14];
		fuzzy=BEEPpars[15];
if(0)		fpr("\nBP: D:%d P1:%d P2:%d GX:%d GY:%d W:%d R:%d F:%d \n",\
			dur,pitch1-1,pitch2-1,gradx,grady,wrap,random,fuzzy);
		/* prepare ourselves */
		us72=qlay1msec/13;	/* 72 microseconds */
		cpdelay=gradx;
		cpstep=grady;
		if (cpstep>0) if (pitch1>pitch2) {int a=pitch2;pitch2=pitch1;pitch1=a;}
		if (cpstep<0) if (pitch2>pitch1) {int a=pitch2;pitch2=pitch1;pitch1=a;}
		/* assert pitch steps from pitch1 towards pitch2 */
		cpitch=pitch1;
/*		cpitch=pitch2;
		if (cpstep<0) cpitch=pitch1;
		if (pitch1>pitch2) if (cpstep<0) cpstep-=cpstep;
		if (pitch1<pitch2) if (cpstep>0) cpstep-=cpstep;
*/
		/* get me started */
		esound=cycle+1; /* next cycle we will start */
		eval_next_event();
		return 1; /* return value is actually don't care now */
	}
	do_speaker();	/* blip */
	if (dur_decr) {
		if (dur<=0) {
			IPCbeeping=0;
			stop_speaker();
			return -1;
		}
	}
	rv=cpitch*us72;
	dur-=cpitch;
	cpdelay-=cpitch;
	if (cpdelay<=0) { cpdelay=gradx; cpitch+=cpstep; }
	if (cpstep>0) {
		if (cpitch>pitch2) {
			cpitch=pitch2;
			cpstep=-cpstep;
			{int a=pitch2;pitch2=pitch1;pitch1=a;}
		}
	} else {
	    if (cpstep<0) {
		if (cpitch<pitch2) {
			cpitch=pitch2;
			cpstep=-cpstep;
			{int a=pitch2;pitch2=pitch1;pitch1=a;}
		}
	    }
	}
	if (rv==0) {
		fpr("ErrorSND ");
		rv=us72; /* we'll be back soon... */
	}
	return rv;
}

static U8 rdmouse(A32 a)
{
/*fpr("RM%x ",a);*/
	if (a==0x1bfbc) {
		return Mdirreg;
	}
	if (a==0x1bf9c) {
		if (Mbutreg==1) return 0x10;
		if (Mbutreg==2) return 0x20;
		if (Mbutreg==3) return 0x30;/*untested*/
	}
	if (a==0x1bf9d) {
		if (opt_use_mouse) {
			Mavail=1;	/* add other criteria...*/
			return 1;	/* mouse detection */
		} else {
			return 0;
		}
	}
	if (a==0x1bfbe) {
		/* ext int ackn 292 */
		return 0;
	}
	return 0;
}

static void wrmouse(A32 a, U8 d)
{
	fpr("WM%x %x ",a,d);
}

int irq_level()
{
	if (REG18021&0x1f) return 2; else return -1;
}

static void do_50Hz()
{
	specialflags|=SPCFLAG_INT;
	REG18021|=0x08;
	/*flash_colors();*/
	handle_events_50Hz();	/* os specific */
}

static void do_1Hz(void)
{
static U32 qtime=0,pcycle=0;
static int first=1;
U32	pctime;

	pctime=(U32)time(0);
	if (qtime!=pctime) {
		/* one real second has passed since last call */
		qtime=pctime;
		if (first) {
			first=0;
		} else {
			fpr("%d ",cycle-pcycle);
		}
		pcycle=cycle;
	}
}

static void do_mdv_motor() {
	if (mdvmotor) {
		REG18021|=0x01;
		specialflags|=SPCFLAG_INT;
		dt_event(1); /* set time stamp */
	}
	return;

	/*970708: only interrupt at end of sector, ISR will assume header first */
	if (mdvmotor&&(mdvghstate==0)) { /* add ==0 for other mvds!! */
/*fpr("+M ");*/
		specialflags|=SPCFLAG_INT;
		REG18021|=0x01;
fpr("M%d ",mdvghstate);
	}
}

void do_keyboard() {
/*fpr("+K ");*/
/*970707 removed*/
/*
	specialflags|=SPCFLAG_INT;
	REG18021|=0x02;
*/
}

/* called from dos 50Hz interrupt */
void do_mouse(int x, int y, int b) {
	return;
}

/* called each ? msec simulated */
static void do_mouse_xm()
{
int	x,y,b;
static	int px=0,py=0;
static	int no_delta=1;

#define	sensi 2

	if (!Mavail) return;
	get_mouse_state(&x,&y,&b);

//fpr("MP %d %d %d\n",x,y,b);
	Mbutreg=b;
	Mdirreg=0;
	if ((x==0)&&(y==0)&&no_delta) return;

	/* there was a pointer update, or we still have a delta to process */
	if (x!=0) {
		px+=x;
		no_delta=0;
	}
	if (y!=0) {
		py+=y;
		no_delta=0;
	}

	if (px>0) {
		Mdirreg|=0x14;
		px-=sensi;
		if (px<0) px=0;
	} else {
		if (px<0) {
			Mdirreg|=0x04;Mdirreg&=~0x10;
			px+=sensi;
			if (px>0) px=0;
		}
	}

	if (py>0) {
		Mdirreg|=0x20;Mdirreg&=~0x01;
		py-=sensi;
		if (py<0) py=0;
	} else {
		if (py<0) {
			Mdirreg|=0x21;
			py+=sensi;
			if (py>0) py=0;
		}
	}

	if ((px==0)&&(py==0)) no_delta=1;

	if (Mdirreg) {
/*fpr("MI%x ",Mdirreg);*/
		specialflags|=SPCFLAG_INT;
		REG18021|=0x10;
	}
}

static void do_tx()
{
	REG18020tx&=~0x02;	/* clear tx busy bit: byte is transmitted */
}
