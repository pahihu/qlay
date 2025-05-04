/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	QL disk access functions
*/

#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "qlmem.h"
#include "spc-os.h"
#include "qldisk.h"

#if defined(__CYGWIN32__)
#define HAVE_FTRUNCATE 1
#elif defined(__BORLANDC__) || defined(_MSC_VER)
#include <io.h>
#define HAVE_FTRUNCATE 1
# if defined(__BORLANDC__)
#  define ftruncate chsize
# elif defined(_MSC_VER)
#  define ftruncate _chsize_s
# endif
#endif

#define PD 0

#if PD == 1
#include "exe68k.h"	//cycle temp
#endif

/* internal */
static int wrnfafile(int drivenr, int filenum, int sectnum, int bytenum, int bytecnt);
static int rdnfafile(int drivenr, int filenum, int sectnum, int bytenum, int bytecnt);
static int gdnfa(int drivenr, int filenum, int sectnum, int bytenum, int bytecnt);
static int truncnfa(int drivenr, int filenum, int sectnum, int bytenum, int bytecnt);
static int rennfa(int drivenr, int filenum, int sectnum, int bytenum, int bytecnt);
static int drvcfgnfa(int drivenr);
static int store_dir(int drivenr, int offset, int bytecnt);
static int get_dir(int drivenr, int offset, int bytecnt);
static int fnum2fname(int drivenr, int fnum, char *fname);
static int fnum2dirname(int drivenr, char *fname);

static void print_byte(U8 val);
static void print_open(void);
static void print_close(void);

#define	END_CMD	0x00
#define	RD_CMD	0x81
#define	WR_CMD	0x82
#define	DEL_CMD	0x84
#define	GD_CMD	0x88
#define	TRC_CMD	0x90
#define	REN_CMD	0xa0
#define	DCF_CMD	0xc0
#define	RST_CMD	0xff

#define	CNTLREG	0x18100
#define	DRIVENR	0x18101
#define	FILENUM	0x18102
#define	SECTNUM	0x18104
#define	BYTENUM 0x18106
#define	BYTECNT 0x18108
#define	SECTOR	0x18200

#define	OFFSET	sectnum*512+bytenum
#define CHOPHEAD()
/*{ int offset=sectnum*512+bytenum-64; sectnum=offset/512; bytenum=offset%512; \
if (offset<0) {sectnum=0;bytenum=0;bytecnt-=64;} \
}
*/

#define CHKBNUMCNT \
{	if(bytenum+bytecnt>512) { \
		fnum2fname(drivenr,filenum,filename); /* 0.82c */ \
		fpr("\nN%02x: D%d, F%d, S%d, B%d, C%d ",data,drivenr,filenum,sectnum,bytenum,bytecnt); \
		fpr("Error B+B %d:<%s> ",bytenum+bytecnt,filename); \
	} \
}

#define MAXDRIVE	8
#define MAXDLEN		(512*20)
#define MAXFNLEN	(255+37)	/* drive + directory + file */

static	U8	dir[MAXDRIVE][MAXDLEN];
static	U8	sector[512];
static	FILE	*nfa;
static	char	filename[MAXFNLEN];

static	int	p=0;	/* print debug */

static FILE *prttmp;
static int prtopen;
static int seropen;

void init_qldisk(void)
{
int	i,q;

	/* chop 'qlay.dir' from option string, leave '\' separator */
	for(i=0;i<8;i++) {
		q=strlen(winfn[i]);
		if (q>7) {
			winfn[i][q-8]='\0';
		}
//		fpr("%d: %s\n",i,winfn[i]);
	}
	prtopen = 0;
	seropen = 0;
}

void exit_qldisk(void)
{
	if (prtopen==1) print_close();
	prtopen = 0;
}

/* store part of directory in 'sector' into 'dir' */
/* return QDOS error */
int store_dir(int drivenr, int offset, int bytecnt)
{
int	i;

if(0)	fpr("store_dir %d %d %d\n",drivenr,offset,bytecnt);
	for(i=0;i<bytecnt;i++) {
		if (offset+i>=MAXDLEN) return -11; /*DF*/
		dir[drivenr-1][offset+i]=sector[i];
if(0)		fpr("S%d %02x ",offset+i,sector[i]);
	}
	return 0; /*OK*/
}

/* get part of directory 'dir' into 'sector' */
/* return QDOS error */
int get_dir(int drivenr, int offset, int bytecnt)
{
int	i;

if(0)	fpr("get_dir %d %d %d\n",drivenr,offset,bytecnt);
	for(i=0;i<bytecnt;i++) {
		if (offset+i>=MAXDLEN) return -7; /*NF*/
		sector[i]=dir[drivenr-1][offset+i];
if(0)		fpr("G%d %02x ",offset+i,sector[i]);
	}
	return 0; /*OK*/
}

int fnum2fname(int drivenr, int fnum, char *fname)
{
	fname[MAXFNLEN-1]='\0';
	if (fnum==0) {
		strcpy(fname,winfn[drivenr-1]);
		strncat(fname,"qlay.dir",36);
		return 0;
	}
	/* first file (1) stored at pos '0' */
	fnum--;
	if (fnum*64<MAXDLEN) {
		strcpy(fname,winfn[drivenr-1]);
		strncat(fname,(const char*)&dir[drivenr-1][fnum*64+16],36);
if(0)		fpr("FN %s ",fname);
		return 0;
	}
	return -1;
}

/* get directory prefix, for RENAME */
int fnum2dirname(int drivenr, char *fname)
{
	strcpy(fname,winfn[drivenr-1]);
	return 0;
}

static int win_drives=0;

int win_avail()
{
	return win_drives;
}

void wrnfa(A32 addr, U8 data)
{
int drivenr,filenum,sectnum,bytenum,bytecnt,r;

	if (addr==0x18100) {
		draw_LED(384-16,266,6,1); /* full yellow */
		drivenr=get_byte(0x18101);
		filenum=get_byte(0x18102)*256+get_byte(0x18103);
		sectnum=get_byte(0x18104)*256+get_byte(0x18105);
		bytenum=get_byte(0x18106)*256+get_byte(0x18107);
		bytecnt=get_byte(0x18108)*256+get_byte(0x18109);

		win_drives|=(1<<(drivenr-1));

#if PD == 1
		fpr("CMD %02x, DR%d, F%d, S%d, B%d, C%d %ld\n",
		data,drivenr,filenum,sectnum,bytenum,bytecnt,cycle);
#endif
		switch(data) {
		case END_CMD:
			r=0;
			break;
		case RD_CMD:
			CHKBNUMCNT;
			r=rdnfafile(drivenr,filenum,sectnum,bytenum,bytecnt);
			break;
		case WR_CMD:
			CHKBNUMCNT;
			r=wrnfafile(drivenr,filenum,sectnum,bytenum,bytecnt);
			break;
		case DEL_CMD:
			fnum2fname(drivenr,filenum,filename);
			/* more checks needed... */
			r=remove(filename);
			break;
		case GD_CMD:
			filenum=0;
			bytenum=0;
			bytecnt=512;
			r=gdnfa(drivenr,filenum,sectnum,bytenum,bytecnt);
			break;
		case TRC_CMD:
			r=truncnfa(drivenr,filenum,sectnum,bytenum,bytecnt);
			break;
		case REN_CMD:
			r=rennfa(drivenr,filenum,sectnum,bytenum,bytecnt);
			break;
		case DCF_CMD:
			r=drvcfgnfa(drivenr);
			break;
		case RST_CMD:
			r=0;
			break;
		default:
			fpr("NFA: illegal command: %d\n",data);
			r=-19;	/*NIyet*/
		}
#if PD == 1
		fpr("RV %d\n",r);
#endif
		put_byte(0x1810a,(r>>8)&0xff);put_byte(0x1810b,r&0xff);
		draw_LED(384-16,266,0,1); /* full black */
		draw_LED(384-16,266,6,0); /* yellow bar */
	}
}

U8 rdnfa(A32 addr)
{
U8 data;

	data=get_byte(addr);
if(0)	fpr("NFAR%x,%02x ",addr,data);
	return data;
}

int drvcfgnfa(int drivenr)
{
	if (drivenr==1) return 1;	/* win1_ always there */
	if (strlen(winfn[drivenr-1])) return 1; else return 0;
//	if (winpresent[drivenr-1]) return 1; else return 0;
}

int wrnfafile(int drivenr, int filenum, int sectnum, int bytenum, int bytecnt)
{
long	offset;
int	pos,i;

if(p)	fpr("\nWS: D%d, F%d, S%d, B%d, C%d ",drivenr,filenum,sectnum,bytenum,bytecnt);
	offset=OFFSET-64;
	if (offset<0) {
		if (bytecnt<=64) {
			return bytecnt;
		} else {
			bytecnt=bytecnt+offset; /* sic! */
			offset=0;
		}
	}
	fnum2fname(drivenr,filenum,filename);
	nfa=fopen(filename,"rb+");
	if (nfa==NULL) {
		nfa=fopen(filename,"wb+");
		if(nfa==NULL) {
			fpr("WR: cannot create file %s\n",filename);
			return -7; /*NF*/
		}
	}
	fseek(nfa,0,0);
	pos=fseek(nfa,offset,1);
	if (pos!=0) { /*seek failed*/
		if(filenum==0) {
			fseek(nfa,0,0);
			for(i=0;i<offset;i++) putc(0,nfa);
if(p)			fpr("Wrote header: %d bytes to %d\n",offset,filenum);
		} else {
if(p)			fpr("WR: cannot fseek %d,%d\n",filenum,offset);
			fclose(nfa);
			return -10; /*EOF*/
		}
	}
	for(i=bytenum;i<bytenum+bytecnt;i++) sector[i-bytenum]=get_byte(i+0x18200);
/* eqv:	for(i=0;i<bytecnt;i++) sector[i]=get_byte(i+bytenum+0x18200);*/

	i = fwrite(sector, 1, bytecnt, nfa);
	if(i!=bytecnt){
		fpr("WR: cannot write %d bytes to %s\n",bytecnt,filename);
		fclose(nfa);
		return -7; /*NF*/
	}
if(p)	fpr("Wrote %d bytes at %d to %s\n",bytecnt,offset,filename);
	fclose(nfa);
	if (filenum==0) {
		if (store_dir(drivenr,offset,bytecnt)<0) return -11; /* DF */
	}
	return 0; /*OK*/
}

int rdnfafile(int drivenr, int filenum, int sectnum, int bytenum, int bytecnt)
{
long	offset;
int	pos,i;

if(p)	fpr("\nRS: D%d, F%d, S%d, B%d, C%d ",drivenr,filenum,sectnum,bytenum,bytecnt);
	offset=OFFSET-64;
	if (offset<0) {
		if (bytecnt<=64) {
			return bytecnt;
		} else {
			bytecnt=bytecnt+offset; /* sic! */
			offset=0;
		}
	}

if(0)	if (filenum==0) {
		if (get_dir(drivenr,offset,bytecnt)<0) return -7; /*NF*/
		for(i=0;i<bytecnt;i++) put_byte(i+0x18200+bytenum,sector[i]);
if(p)		fpr("Read %d bytes at %d from dir\n",bytecnt,offset);
		return 0;
	}

	fnum2fname(drivenr,filenum,filename);
	nfa=fopen(filename,"rb");
	if (nfa==NULL) {
		fpr("RD: cannot open file %s\n",filename);
		return -7; /*NF*/
	}
	fseek(nfa,0,0);
	pos=fseek(nfa,offset,0);
	if (pos!=0) {/*seekfailed*/
if(p)		fpr("RD: cannot fseek file %s: %d %d\n",filename,pos,offset);
		fclose(nfa);
		return -10; /*EOF*/
	}
	i = fread(sector, 1, bytecnt, nfa);
	if(i!=bytecnt){
if(p)		fpr("RD: cannot read %d bytes from file %s\n",bytecnt,filename);
/*		fclose(nfa);*/
/*		return -7 */ /*NF*/;
		bytecnt=i;
if(p)		fpr("Instead: %d\n",i);
	}
	for(i=bytenum;i<bytenum+bytecnt;i++) put_byte(i+0x18200,sector[i-bytenum]);
if(p)	fpr("Read %d bytes at %d from file %s\n",bytecnt,offset,filename);
	fclose(nfa);
	return 0; /*OK*/
}

int gdnfa(int drivenr, int filenum, int sectnum, int bytenum, int bytecnt)
{
long	offset;
int	pos,i,rv;
int	s0offset;

if(0)	fpr("\nGD: D%d, F%d, S%d, B%d, C%d ",drivenr,filenum,sectnum,bytenum,bytecnt);
	offset=OFFSET-64;
	s0offset=0;
	if (offset<0) {
		if (bytecnt<=64) {
			return bytecnt;
		} else {
			s0offset=-offset;	/* compensate for return */
			bytecnt=bytecnt-s0offset;
			offset=0;
		}
	}

	fnum2fname(drivenr,filenum,filename);
	nfa=fopen(filename,"rb");
	if (nfa==NULL) {
		fpr("GD: cannot open file %s\n",filename);
		return -7; /*NF*/
	}
/*	fseek(nfa,0L,0);*/
	pos=fseek(nfa,offset,0);
	if (ftell(nfa)!=offset) {/*seekfailed*/
if(p)		fpr("GD: cannot fseek file %s: %d %d\n",filename,pos,offset);
		fclose(nfa);
		return 0;	/* this occurs when dir file len%512=0 */
	}
	rv = fread(sector, 1, bytecnt, nfa);

	for(i=bytenum;i<bytenum+rv;i++) put_byte(i+0x18200,sector[i-bytenum]);
if(0)	fpr("Read %d bytes at %d from file %s\n",rv,offset,filename);
	fclose(nfa);
	if (store_dir(drivenr,offset,rv)<0) return -11; /* DF */
	return rv+s0offset; /*OK*/
}

int truncnfa(int drivenr, int filenum, int sectnum, int bytenum, int bytecnt)
{
long	offset;
int	pos,i,rv;
int	s0offset;

if(p)	fpr("\nTR: D%d, F%d, S%d, B%d, C%d ",drivenr,filenum,sectnum,bytenum,bytecnt);
	offset=OFFSET-64;
	s0offset=0;
	if (offset<0) {
		if (bytecnt<=64) {
			return bytecnt;
		} else {
			s0offset=-offset;	/* compsensate for return */
			bytecnt=bytecnt-s0offset;
			offset=0;
		}
	}

	fnum2fname(drivenr,filenum,filename);
	nfa=fopen(filename,"rb");
	if (nfa==NULL) {
		fpr("TR: cannot open file %s\n",filename);
		return -7; /*NF*/
	}
/*	fseek(nfa,0L,0);*/
	pos=fseek(nfa,offset,0);
	pos=ftell(nfa);
if(p)	fpr("O%d P%d ",offset,pos);
	if (pos<offset) {/* seekfailed? how do we get this? */
		/* should the file be extended till offset? */
if(p)		fpr("File %s too short: %d %d\n",filename,pos,offset);
		fclose(nfa);
		return 0;	/* this occurs when dir file len%512=0 */
	}
	fseek(nfa,0L,2);
	pos=ftell(nfa);
	if (pos>offset) {
#if defined(HAVE_FTRUNCATE)
/* Apparently truncate not in library B19.1 */
		ftruncate(fileno(nfa),offset);	/* not tested yet */
#else
		truncate(filename,offset);
#endif
if(p)		fpr("Truncated file %s after %d\n",filename,offset);
	}
	fclose(nfa);
	return 0; /*OK*/
}

/* rename file. filename in sector, return QDOS error code */
int rennfa(int drivenr, int filenum, int sectnum, int bytenum, int bytecnt)
{
int	i,rv,fnlen;
char	newname[MAXFNLEN];

	rv=0;
if(p)	fpr("\nRN: D%d, F%d, S%d, B%d, C%d ",drivenr,filenum,sectnum,bytenum,bytecnt);

	fnum2fname(drivenr,filenum,filename);
	nfa=fopen(filename,"rb");
	if (nfa==NULL) {
		fpr("RN: cannot open file %s\n",filename);
		return -7; /*NF*/
	}
	fclose(nfa);
	fnlen=bytecnt;
	if (fnlen>36) fnlen=36; /* silently adjust */
	for(i=0;i<fnlen;i++) sector[i]=get_byte(i+0x18200);
	sector[fnlen]='\0';
	fnum2dirname(drivenr,newname);
	strncat(newname,(const char*)sector,36);
	/* check if already exist */
	nfa=fopen(newname,"rb");
	if (nfa!=NULL) {
if(p)		fpr("RN: already exists: %s\n",newname);
		fclose(nfa);
		return -8; /*AE*/
	}
	rv=rename(filename,newname);
if(1)		fpr("RN: Renamed %s to %s, rv: %d\n",filename,newname,rv);
	if (rv!=0) rv=-1; /*NC*/
	return rv;
}

U8 rdserpar(A32 a)
{

	fpr("RDSP %lx\n",a);
	return 0;
}

void wrserpar(A32 a, U8 d)
{

	fpr("WRSP %lx %x\n",a,d);
	if (a==0x180c0) print_byte(d);
}

static void print_open(void)
{
	fpr("Opening Printer: %s\n",prtname);
	prttmp=fopen(prtname,"wb");
	if (prttmp == NULL) {
		fpr("Printer open failed: %s. Skip print\n",prtname);
		prtopen=-1;
	} else {
		prtopen=1;
	}
}

static void print_close(void)
{
	fclose (prttmp);
	prtopen = 0;
}

static void print_byte(U8 val)
{

	if (prtopen==0) print_open();
	if (prtopen==1) {
#ifndef __DOS__
		fprintf (prttmp,"%c",val);
#else
		fputc (val, prttmp);
		fflush (prttmp);
#endif
		if (val==0x1A) {	/* eof ^Z */
			print_close();
		}
	}
}

