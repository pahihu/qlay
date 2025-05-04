/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	QLAY TOOLS QXL
*/

#include "qlayt.h"

int	nos;			/* num of sectors in QXL image file */
int	*smap,*stype,*fnum;

/* don't want these recursively? */
int	filenum=0;

/* process the qxl image file. In: 's'=start sector, 'ec'=end of directory */
/* recursively called! */
/* return 0 if A-OK */
static int do_dir(FILE *infile, int s, int ec)
{
int i,lf,sidx,n,sectnum,dirtype,nlen,si,cs,cflen,cslen,nsect;
U32 dirlen;
FILE	*of;
char	fname[37];
U8	header[64];
int	headoff;

	s=s*SECTLENQXL;
	fname[36]='\0';
	lf=0;
	sidx=0x40;
	while (!lf) { /* not last file */
		/* (re)read the sector */
		fseek(infile,s,0);
		n=fread(sector,sizeof(char),SECTLENQXL,infile);
		if(n!=SECTLENQXL){
			printf("error: cannot read sector at %x\n",s);
			return(1);
		}
		sectnum=getword(&sector[sidx+0x3a]);
		dirtype=sector[sidx+5];
		stype[sectnum]=dirtype;
		dirlen=getlong(&sector[sidx]);
if(dbg)		printf("DL %6lx ",dirlen);
if(dbg)		printf("SN %4x ",sectnum);
if(dbg)		printf("DT %2x ",dirtype);
		/* extra check needed on strings!... */
if(dbg)		printf("FN %s\n",&sector[sidx+0x10]);
		if ((dirtype<2)&&(dirlen>0)) {
			filenum++;
if(dbg)			printf("FN %d ",filenum);
			nsect=(dirlen+0x7ff)>>11;
if(dbg)			printf("NS %d \n",nsect);

			/* copy file header from directory */
			for (i=0;i<64;i++) header[i]=sector[sidx+i];

			/* now write the file */
			nlen=getword(&sector[sidx+0xe]);
/*082d: use nlen!*/
			if(nlen>36)nlen=36;
			strncpy(fname,&sector[sidx+0x10],nlen);
			fname[nlen]='\0'; /* just make sure... req. for DJ?? */
if(dbg)			printf("\nWriting %s\n",fname);
			if ((of=fopen(fname,"wb")) == NULL) {
/*0.82, taken from mdv2fil.
Should make a general use function, also for 8.3 support ...
try file_### instead */
				printf("Cannot open '%s', trying different name --> ",fname);
				sprintf(fname,"file_%03d",filenum);
				printf("%s\n",fname);
				if ((of=fopen(fname,"wb")) == NULL) {
					printf("Cannot open '%s'\n",fname);
					return(1);
				}
			}
			cflen=dirlen;
			cs=sectnum;
			for(si=0;si<nsect;si++) {
				if (cs==0) printf("ERRORcs=0");
				fseek(infile,cs*SECTLENQXL,0);
				n=fread(sector,sizeof(char),SECTLENQXL,infile);
				cslen=SECTLENQXL;
				if(cflen<cslen)cslen=cflen;
/*082d: chop it */
				if(si==0) {
					cslen-=64;headoff=64;
					n=fwrite(header,sizeof(U8),64,qxldf);
					if(n!=64) {
						printf("Cannot write directory file %s\n",dirfname);
						exit(1);
					}
				} else headoff=0;

				/* first file sector, overwrite with dir header */
/*082d, no... we don't		if(si==0) {
					for(i=0;i<64;i++) sector[i]=header[i];
				}
*/
				n=fwrite(&sector[headoff],sizeof(char),cslen,of);
				if(n!=cslen){
					printf("Cannot write %s, %d != %d\n",fname,n,cslen);
					fclose(of);
					return(1);
				}
				cflen-=SECTLENQXL;

				fnum[cs]=filenum;
if(dbg)				printf("%04x,%04x ",cs,smap[cs]);
				cs=smap[cs];
			}
			fclose(of);
if(dbg)			printf("\n");
		}
		/* recurse down the dir tree */
		if((dirtype==0xff)&&(dirlen>0x40)) {
			n=do_dir(infile,sectnum,dirlen);
			if (n) {printf("error, exiting\n"); fclose(infile); exit(1);}
		}

		sidx+=0x40;
		if (sidx>=ec) lf=1;
	}
	return(0);
}

void enqxl(char *fn)
{
;
}

static void getqxlheader(FILE *f, int *firstsect, int *firstdirlen)
{
	fread(head,sizeof(U8),64,f);
	if (strncmp(head,"QLWA",4)) {
		fprintf(stderr,"Not a QLWA image file, exiting\n");
		exit(1);
	}
	nos=getword(&head[0x2a]);
	fprintf(stderr,"QLWA: %d sectors\n",nos);
	*firstsect=getword(&head[0x34]);
	*firstdirlen=getword(&head[0x38]);
}

void deqxl(char *fn)
{
int i,sn,firsts,dlen;
int n;

FILE	*infile;

	if ((infile=fopen(fn,"rb")) == NULL) {
		printf("%s: cannot open %s\n",PROGNAME,fn);
		exit(1);
	}

	if ((qxldf=fopen(dirfname,"rb")) != NULL) {
		printf("%s: %s already exists, exiting\n",PROGNAME,dirfname);
		fclose(qxldf);
		exit(1);
	}
	fclose(qxldf);

	if ((qxldf=fopen(dirfname,"wb+")) == NULL) {
		fprintf(stderr,"%s: cannot open %s for write\n",PROGNAME,dirfname);
		exit(1);
	}

	for(i=0;i<nos;i++) {
		stype[i]=0x55;
		fnum[i]=0;
	}
	getqxlheader(infile,&firsts,&dlen);

	smap=malloc(sizeof(int)*nos);
	if (!smap) {fprintf(stderr,"Out of memory - exiting\n");exit(1);}
	stype=malloc(sizeof(int)*nos);
	if (!stype) {fprintf(stderr,"Out of memory - exiting\n");exit(1);}
	fnum=malloc(sizeof(int)*nos);
	if (!fnum) {fprintf(stderr,"Out of memory - exiting\n");exit(1);}

	for(i=0;i<nos;i++) {
		sn=fgetc(infile)*256;
		sn+=fgetc(infile);
		smap[i]=sn;
	}

	n=do_dir(infile,firsts,dlen);
	if (n) {printf("error, exiting\n"); fclose(infile); exit(1);}

if(dbg)	for(i=0;i<nos;i++) {
		printf("%07x ",i*SECTLENQXL);
		printf("I %04x S %04x T %04x N %d ",i,smap[i],stype[i],fnum[i]);
		if((fnum[i]==0)&&(stype[i]!=0xff)) printf("EMPTY");
		printf("\n");
	}

	fclose(infile);
}

