/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	QLAY TOOLS main
*/

/*
v0.82c: 970802
v0.82d: 970802 with mdv support
v0.82e:	970803 update, QDOS02 zip
v0.83a:	970903 chop 64 byte header from file
rompatch..
v0.90	990129 random number from command line
*/

#include "qlayt.h"
#include "qlvers.h"

/* globals, declared here */
char	ifname[LINESIZE],lstline[LINESIZE],qdosname[QDOSSIZE],dosname[DOSSIZE];
char	lstline2[LINESIZE];
U8 	head[64];
U8	sector[SECTLENQXL];
int	dbg;
int	randmdv;
FILE	*qxldf;
char	lstfname[256]="";
char	addfname[256]="";
char	outfname[256]="qlay.mdv";
char	dirfname[256]="qlay.dir";

/* arrrgh.. little endian */
void putlong(U8 *p, U32 v)
{
	p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;
}

void putword(U8 *p, U16 v)
{
	p[0]=v>>8;p[1]=v;
}

U32 getlong(U8 *p)
{
	return (p[0]<<24)+(p[1]<<16)+(p[2]<<8)+p[3];
}

U16 getword(U8 *p)
{
	return (p[0]<<8)+p[1];
}

void usage(void)
{
	fprintf(stderr,"Usage  %s [options]\n",PROGNAME);
	fprintf(stderr,"   -i file     insert a file to the directory\n");
	fprintf(stderr,"   -d datasize use 'datasize' when adding file to directory (use with -i)\n");
	fprintf(stderr,"   -X          use XTcc datasize when adding file to directory (use with -i)\n");
	fprintf(stderr,"   -l          list the files in the directory\n");
	fprintf(stderr,"   -q file     directory is in 'file'; default 'qlay.dir'\n");
	fprintf(stderr,"   -c file     create new qlay directory, filenames are in 'file'\n");
	fprintf(stderr,"   -a file     append files to the directory, filenames are in 'file'\n");
	fprintf(stderr,"   -r file     remove a file from the directory\n");
	fprintf(stderr,"   -u file     update a file in the directory (needed e.g. after a DOS edit)\n");
	fprintf(stderr,"   -x file     show XTcc datasize in file, no directory update\n");
	fprintf(stderr,"   -z file     show datasizes in InfoZip file, no directory update\n");
	fprintf(stderr,"   -L file     list contents of mdv-'file'\n");
	fprintf(stderr,"   -W file     write all files in mdv-'file' to current directory\n");
	fprintf(stderr,"   -C file     create MDV from files in list 'file'; output default: 'qlay.mdv'\n");
	fprintf(stderr,"   -o file     set output file name for create MDV, in stead of 'qlay.mdv'\n");
	fprintf(stderr,"   -R number   set random number (hexadecimal) in MDV header (use with -C)\n");
	fprintf(stderr,"   -E file     extract files from a QXL image file\n");
	fprintf(stderr,"   -m file1 file2 convert qdos to dos file\n");
	fprintf(stderr,"   -n file1 file2 convert dos file to qdos; use -d datasize if needed\n");
	fprintf(stderr,"qlayt is a file conversion support program for QLAY: QL emulator.\n");
	fprintf(stderr,"%s is freeware (C) Jan Venema, ",PROGNAME);
	fprintf(stderr,"Version %s\n",qlayversion());
	exit(1);
}

void main(int argc,char **argv)
{
int	opt;
int	cmd=0,ncmd=0,dz=0;
U32	datasize=0; /* kch! */

	dbg=0;
	randmdv=-1;
	ifname[LINESIZE-1]='\0';
	lstline[LINESIZE-1]='\0';
	lstline2[LINESIZE-1]='\0';
	qdosname[QDOSSIZE-1]='\0';
	dosname[DOSSIZE-1]='\0';

	/* process command line options */
	while(((opt = getopt(argc, argv, "a:c:C:d:D:E:f:F:i:lL:m:n:o:q:r:R:S:t:u:UW:x:Xz:")) != EOF))
	switch(opt) {
		case 'i':
			strncpy(addfname,optarg,255);
			cmd=INSERT; ncmd++;
			break;
		case 'c':
			cmd=CREATE; ncmd++;
			strncpy(lstfname,optarg,255);
			break;
		case 'a':
			cmd=APPEND; ncmd++;
			strncpy(lstfname,optarg,255);
			break;
		case 'l':
			cmd=LIST; ncmd++;
			break;
		case 'q':
			strncpy(dirfname,optarg,255);
			break;
		case 'd':
			if (dz) {
				fprintf(stderr,"Error: use either -d or -X option\n");
				usage();
			}
			dz=1;
			datasize=atoi(optarg);
			break;
		case 'X':
			if (dz) {
				fprintf(stderr,"Error: use either -d or -X option\n");
				usage();
			}
			dz=2;
			break;
		case 'x':
			strncpy(addfname,optarg,255);
			cmd=SHOWXTCC; ncmd++;
			break;
		case 'r':
			strncpy(addfname,optarg,255);
			cmd=REMOVE; ncmd++;
			break;
		case 'S':
			strncpy(addfname,optarg,255);
			cmd=SER2MDV; ncmd++;
			break;
		case 'W':
			strncpy(addfname,optarg,255);
			cmd=MDV2FIL; ncmd++;
			break;
		case 'L':
			strncpy(addfname,optarg,255);
			cmd=LISTMDV; ncmd++;
			break;
		case 'C':
			strncpy(lstfname,optarg,255);
			cmd=FIL2MDV; ncmd++;
			break;
		case 'o':
			strncpy(outfname,optarg,255);
			break;
		case 'E':
			strncpy(addfname,optarg,255);
			cmd=DEQXL; ncmd++;
			break;
		case 'F':
			strncpy(addfname,optarg,255);
			cmd=ENQXL; ncmd++;
			break;
		case 'z':
			strncpy(addfname,optarg,255);
			cmd=SHOWZIP; ncmd++;
			break;
		case 'u':
			strncpy(addfname,optarg,255);
			cmd=UPDATEF; ncmd++;
			break;
		case 'U':
			cmd=UPDATEA; ncmd++;
			break;
		case 'm':
			strncpy(addfname,optarg,255);
			strncpy(outfname,argv[optind],255); /* expect no more args */
			cmd=QDOS2DOS; ncmd++;
			break;
		case 'n':
			strncpy(addfname,optarg,255);
			strncpy(outfname,argv[optind],255); /* expect no more args */
			cmd=DOS2QDOS; ncmd++;
			break;
		case 'R':
			sscanf(optarg,"%x",&randmdv);
			break;
		case 'D':
			if (atoi(optarg)==85) dbg=1;
			break;
		case 'f':
			strncpy(addfname,optarg,255);
			cmd=DUMPFLP; ncmd++;
			break;
		case 't':
			strncpy(addfname,optarg,255);
			cmd=LISTFLP; ncmd++;
			break;
		case '?':
 		default:
			usage();
			break;
	}

	if (ncmd>1) {
		fprintf(stderr,"Illegal combination of options\n");
		usage();
	}

	if (dz && (cmd!=INSERT) && (cmd!=DOS2QDOS) ) {
		fprintf(stderr,"Error: -d or -X only valid with -i or -n option\n");
		usage();
	}

	switch(cmd) {
	case LIST:
		listqld(0);
		break;
	case CREATE:
		createdir(lstfname,1);
		break;
	case APPEND:
		createdir(lstfname,0);
		break;
	case INSERT:
		insertfile(dz,datasize,addfname);
		break;
	case REMOVE:
		removefile(addfname,1); /* and clean it up */
		break;
	case SHOWXTCC:
		showxtcc(addfname);
		break;
	case SER2MDV:
		ser2mdv(addfname);
		break;
	case MDV2FIL:
		mdv2fil(addfname,1);
		break;
	case LISTMDV:
		mdv2fil(addfname,0);
		break;
	case FIL2MDV:
		fil2mdv(lstfname,outfname);
		break;
	case DEQXL:
		deqxl(addfname);
		break;
	case ENQXL:
		enqxl(addfname);
		break;
	case SHOWZIP:
		showzip(addfname);
		break;
	case UPDATEF:
		updatef(addfname);
		break;
	case UPDATEA:
		updatea();
		break;
	case DOS2QDOS:
		dos2qdos(addfname,outfname,dz,datasize);
		break;
	case QDOS2DOS:
		qdos2dos(addfname,outfname);
		break;
	case LISTFLP:
		flp_main("l",addfname);
		break;
	case DUMPFLP:
		flp_main("d",addfname);
		break;
	default:
		usage();
	}
	exit(0);
}
