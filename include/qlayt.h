/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	QLAY TOOLS include and define
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <sys/time.h>
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <ctype.h>

#define PROGNAME "qlayt"

#define QDOSTIME ((9*365+2)*86400) /* 0.82c forget about GMT&DST offsets */

typedef unsigned long U32;
typedef unsigned short U16;
typedef unsigned char U8;

#define	LINESIZE 256
#define	QDOSSIZE 37
#define	DOSSIZE 14
#define	SECTLENQXL	0x800	/* sector length in QXL */
				/* shared usage among options */

extern	char	ifname[LINESIZE],lstline[LINESIZE],qdosname[QDOSSIZE],dosname[DOSSIZE];
extern	char	lstline2[LINESIZE];
extern	U8 	head[64];
extern	U8	sector[SECTLENQXL];
extern	int	dbg;
extern	FILE	*qxldf;
extern	int	randmdv;
extern	char	lstfname[256];
extern	char	addfname[256];
extern	char	outfname[256];
extern	char	dirfname[256];

extern	void	putlong(U8*,U32);
extern	void	putword(U8*,U16);
extern	U32	getlong(U8*);
extern	U16	getword(U8*);
extern	void	usage(void);
extern	int	getxtcc(FILE*,U32*);

extern	void	listqld(int);
extern	void	createdir(char*,int);
extern	void	insertfile(int,U32,char*);
extern	void	removefile(char*,int);
extern	void	showxtcc(char*);
extern	void	ser2mdv(char*);
extern	void	mdv2fil(char*,int);
extern	void	fil2mdv(char*,char*);
extern	void	deqxl(char*);
extern	void	enqxl(char*);
extern	void	showzip(char*);
extern	void	updatef(char*);
extern	void	updatea(void);
extern	void	dos2qdos(char*,char*,int,U32);
extern	void	qdos2dos(char*,char*);
extern	void	flp_main(char*,char*);

#define	LIST		1
#define	CREATE		2
#define	APPEND		3
#define	INSERT		4
#define REMOVE		5
#define SHOWZIP		6
#define SHOWXTCC	7
#define FIL2MDV		8
#define MDV2FIL		9
#define SER2MDV		10
#define QLNET		11
#define DEQXL		12
#define ENQXL		13
#define UPDATEF		14
#define UPDATEA		15
#define LISTMDV		16
#define DOS2QDOS	17
#define QDOS2DOS	18
#define LISTFLP		19
#define DUMPFLP		20
