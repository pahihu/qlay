/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	QLAY TOOLS FLP, modified from ql-tools
*/

/*
QLTOOLS

Read a QL floppy disk

(c)1992 by Giuseppe Zanetti

Giuseppe Zanetti
via Vergani, 11 - 35031 Abano Terme (Padova) ITALY
e-mail: beppe@sabrina.dei.unipd.it

Valenti Omar
via Statale,127 - 42013 Casalgrande (REGGIO EMILIA) ITALY
e-mail: sinf7@imovx2.unimo.it
	sinf7@imoax1.unimo.it
	sinf@c220.unimo.it

somewhat hacked by Richard Zidlicky, added formatting, -dl, -x option
rdzidlic@cip.informatik.uni-erlangen.de

*/

#define VERSION     "0.86e,  May 15 1998"

/* Maximum allocation block (normally 3) */

#define MAXALB          6

/* Maximum number of sectors (norm. 1440) */

#define MAXSECT         2880


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>        /* lseek read write */
#include <fcntl.h>

#ifdef THINK_C
#include "console.h"
#include <string.h>
#else
#include <string.h>
#endif

#ifdef MSDOS
#include <bios.h>
#include <dos.h>    /* for delay    */
#include <io.h>     /* for setmode  */
#define RESET   0
#define LAST    1
#define READ    2
#define WRITE   3
#define VERIFY  4
#define FORMAT  5
#endif

/* this include is neccessary since stdio.h in SUN 4.0 do not define it */

#ifndef SEEK_SET
#include <unistd.h>
#endif

int flp_main(char*,char*);

typedef unsigned long U32;
typedef unsigned short U16;
typedef unsigned char U8;
typedef long S32;

int fd;
char nfile[12];
static U8 b[512*MAXALB];          /* general purpose buffer */
static U8 b0[512*MAXALB];         /* block0 and FAT */
static signed char *ltp;                     /* ltp table */
static signed char *ptl;
static U32 bleod;                   /* directory block offset */
static U32 byeod;                   /*           bytes        */
static U8 *pdir;                  /* memory image of directory */
static U8 *__base__;

static S32 err;

#ifdef MSDOS
static U32 drive;
#endif

#ifdef TABLE
static U32 convtable[MAXSECT];

#if (defined(QDOS) || defined(MSDOS))
static U8 conv_side[MAXSECT];
static U8 conv_sector[MAXSECT];
static U16 conv_track[MAXSECT];
#endif
#endif

/* globals */
static U32 gsides,gtracks,gsectors,goffset,allocblock,gclusters,gspcyl,gssize;
static int ql5a;                  /* flag */
static int block_dir[20];
static int block_max=0;
static S32 lac=0;                /* last allocated cluster */

U32 wrd(U8 *wp);
void write_cluster(char *p,int num);
U32 lwrd(U8 *p);
int read_cluster(char *p,int num);
void wr_wrd(U8 *wp,U32 r);
void f_usage(char *error);
void make_convtable(int verbose);
void wr_lwrd(U8 *p,U32 r);

void free_cluster(long);
int alloc_new_cluster(int ,int);
void format(char * frmt,char *argfname);
void dir_write_back(void);
void set_header(U32 ni,char *name);
void read_b0fat(int);
void write_b0fat(void);
void read_ltp(void);
void read_fat(void);
void read_dir(void);
void print_map(void);

#ifdef QDOS
#define SECT0 1
#else
#define SECT0 0
#endif

/* logical to physical translation macros */

#ifdef TABLE
#ifdef MSDOS
#define LTP_TRACK(_sect_)   (conv_track[_sect_])
#define LTP_SIDE(_sect_)    (conv_side[_sect_])
#define LTP_SECT(_sect_)    (conv_sector[_sect_]+1)
#else
#ifdef QDOS
#define LTP(_sect_) \
    (conv_side[_sect_]*256+conv_track[_sect_]*65536+conv_sector[_sect_]+1)
#else
#define LTP(_sect_)  \
    (gssize*convtable[_sect_])         /* unix /dev/rfd0 style */
#endif
#endif

#else /* no TABLE */
#define LTP_TRACK(_sect_)   ((_sect_)/gspcyl)
#define LTP_SIDE(_sect_)    (0>ltp[(_sect_)%gspcyl])
#define LTP_SCT(_sect_) \
     (((0x7f&ltp[(_sect_)%gspcyl])+goffset*LTP_TRACK(_sect_)) % gsectors)
#define LTP_SECT(_sect_)    (LTP_SCT(_sect_)+1)

#ifdef QDOS
#define LTP(_sect_)  ((LTP_SIDE(_sect_)<<8)+(LTP_TRACK(_sect_)<<16)+ \
                      LTP_SCT(_sect_)+1)
#else
#define LTP(_sect_)   (gssize*(LTP_TRACK(_sect_)*gspcyl+ \
                       LTP_SIDE(_sect_)*gsectors+LTP_SCT(_sect_)))
#endif
#endif /* TABLE */


/* some defines to hide FAT bit artistics, heavy use */
/* of global variables RZ */

#define FAT_FILE(__cluster) (__base__=b0+0x4c+20+(__cluster)*3,\
			  (U32)((*__base__) <<4)+ \
                          (U32)((*(__base__+1)>>4)&15))


#define FAT_CL(__cluster)  (__base__=b0+0x4C+20+(__cluster)*3,\
			  (*(__base__+1) &15)*256+ *(__base__+2))



#define SET_FAT_FILE(__cluster,__file)\
    (__base__=b0+0x4c+20+(__cluster)*3,\
     *(__base__)=(__file)>>4,\
     *(__base__+1)=(((__file)&15)<<4)+(*(__base__+1)&15))


#define SET_FAT_CL(__cluster,__clnum)\
    (__base__=b0+0x4C+20+(__cluster)*3,\
     *(__base__+1)=((__clnum)>>8)+(*(__base__+1)&(~15)),\
     *(__base__+2)=(__clnum)&255)


/* some defines for block0 info */
#define	 SET_TRACKS(x) wr_wrd(b0+30,x)
#define	 SET_SPT(x)    wr_wrd(b0+26,x)
#define  SET_SPCYL(x)  wr_wrd(b0+28,x)
#define  SET_SOFF(x)   wr_wrd(b0+38,x)
#define  SET_DIRBL(x)  wr_wrd(b0+34,x)
#define  SET_DIROF(x)  wr_wrd(b0+36,x)
#define  SET_FREE(x)   wr_wrd(b0+20,x)
#define  SET_TOTAL(x)  wr_wrd(b0+24,x)
#define	 SET_GOOD(x)   wr_wrd(b0+22,x)
#define  SET_SPCL(x)   wr_wrd(b0+32,x)


#define	 GET_TRACKS() wrd(b0+30)
#define	 GET_SPT()    wrd(b0+26)
#define  GET_SPCYL()  wrd(b0+28)
#define  GET_SOFF()   wrd(b0+38)
#define  GET_DIRBL()  wrd(b0+34)
#define  GET_DIROF()  wrd(b0+36)
#define  GET_FREE()   wrd(b0+20)
#define  GET_TOTAL()  wrd(b0+24)
#define	 GET_GOOD()   wrd(b0+22)
#define  GET_SPCL()   wrd(b0+32)

#define min(a,b) (a<b ? a : b)

/* byte order conversion */
U32 wrd(U8 *wp)
{
    U32 r;

    r= (((U32) *(wp))<<8) + (U32) *(wp+1);

    return r;
}

void  wr_wrd(U8 *wp, U32 r)
{

    *(wp)=r>>8;
    *(wp+1)=r & 255;

}
void wr_lwrd(U8 *p, U32 r)
{
    *(p+3)=r&255; r=r>>8;
    *(p+2)=r&255; r=r>>8;
    *(p+1)=r&255;
    *p = r>>8;
}


U32 lwrd(U8 *p)
{

    return  (((((*p<<8)+*(p+1))<<8)+*(p+2))<<8)+ *(p+3)  ;
}


static void cat_file(U32 fnum, char *fname)
{
    U32 flen,file,blk;
    U32 i,ii,s,start,end;
    char buffer[512*MAXALB],*entry;
    int ok,del;
    FILE *f;

    f=fopen(fname,"wb");
    if (f==NULL) {
	    fprintf(stderr,"Cannot open file %s\n",fname);
	    exit(1);
    }

    del=0;
    if ((fnum<<6)>= byeod+bleod*gssize)
	{fprintf(stderr,"file number out of bounds\n");
	exit(1);}

    entry=pdir+(fnum<<6);
    flen=lwrd(entry);               /* with the header */
    if(flen+wrd(entry+14)==0)
	{ fprintf(stderr,"warning: file %ld appears to be deleted, trying to recover it\n",fnum);
        fnum=(fnum&0xf)+0xfd0;
	del=1;
	}

    s=0;
    do{
	ok=0;
       	for (i=0;i< gclusters;i++){
	    file = FAT_FILE(i);
	    blk= FAT_CL(i);
	    if (file==fnum && blk==s){
		if (blk == s){
		    read_cluster(buffer,i);
		    if (s==0)
			if (flen==0) flen=lwrd(buffer);
#if 0   /* sandy controler doesn't maintain the header copy */
			else if(flen!=lwrd(buffer))
			    fprintf(stderr,"read file: incomplete header copy ?\n");
#endif
		    if (s==0) start=64;
		    else start=0;
		    end=gssize*allocblock;
		    if (s==(flen/(gssize*allocblock)))
			end=flen % (gssize*allocblock);
		    err=fwrite(buffer+start,1,end-start,f);
		    if(err<0)
			perror("output file: fwrite(): ");
		    ok=1;
		}
		if (ok) break;
	    }}

	if (!ok)
	    {
		fprintf(stderr,"\n\nCluster #%i of file #%li not found\n\n",s,fnum);
		if (s==0 && del) {
		    fprintf(stderr,"sorry, not recoverable, block #0 missing\n\n");
		    exit(1);
		}
		err=lseek(1,gssize*allocblock,SEEK_CUR);  /* leave hole */
		if (err<0)                             /* non seekable */
		    for(ii=0;ii<allocblock*gssize/16;i++)
			fwrite("################",1,16,f);
	    }
	s++;
    } while(s<=flen/(gssize*allocblock));
    fclose(f);
}

static void del_file(U32 fnum)
{
    long int flen,blk;
    int freed,blk0;
    U8 *entry;
    U32 i,file;


#ifdef MSDOS
    setmode(fileno(stdout),O_BINARY);
#endif

    freed=0; blk0=-1;
    entry=pdir+(fnum<<6);

    if ((fnum<<6) >= bleod*gssize+byeod)
       {fprintf(stderr,"file number out of bounds\n");exit(1);}

    flen=lwrd(entry);    /* with the header */
    if (flen==0)
       {fprintf(stderr,"file already deleted?\n"); exit(1);}

    for (i=1;i< gclusters;i++)
	{
	    file = FAT_FILE(i);
	    if (file == fnum)
		{
         if( FAT_CL(i)==0 ) blk0=i;
		    free_cluster(i);
		    freed++;
		}


#ifdef MSDOS
	    setmode(fileno(stdout),O_BINARY);
#endif

	}

    blk=GET_FREE();
    SET_FREE(freed*allocblock+blk);

    if (blk0>0)
      {  read_cluster(b,blk0);
         memcpy(b,entry,64);
         write_cluster(b,blk0);
      }
    else fprintf(stderr,"block 0 of file missing??\n");

    wr_wrd(entry+14,0);        /* lunghezza del nome del file */
    wr_lwrd(entry,0);          /* lunghezza del file */
    *(entry+5)=0;

    write_b0fat();             /* write_cluster(b0,0); */
    dir_write_back();

    exit(0);
}


void f_usage(char *error)
{
    fprintf(stderr,"Error %s\n",error);

    exit(1);

#ifdef MSDOS
    fprintf(stderr,"Usage: qltools [a:|b:] -[options] [filename]\n");
#else
#ifdef QDOS
    fprintf(stderr,"Usage: qltools drive -[options] [filename]\n");
#endif
#endif

    fprintf(stderr,"Options:\n");
    fprintf(stderr,"-d         list directory\n");
    fprintf(stderr,"-i         list info\n");
    fprintf(stderr,"-m         list disk map\n");
    fprintf(stderr,"-c         list conversion table of sectors\n");
    fprintf(stderr,"-w <name>  write <name> file to disk\n");
    fprintf(stderr,"-nFN       copy file number FN to stdout, handles\n");
    fprintf(stderr,"               deleted files where possible\n");

#ifdef MSDOS
    fprintf(stderr,"QLTOOLS for MS-DOS (version %s)\n",VERSION);
#else
#ifdef QDOS
    fprintf(stderr,"drive is in the form flp1_*D2d \n");
    fprintf(stderr,"QLTOOLS for QDOS (version: %s)\n",VERSION);
#else
    fprintf(stderr,"diskimage is either a file with the image of a QL format disk\n");
    fprintf(stderr,"or a Unix device with a QL disk inserted in it (/dev/fd...)\n\n");
    fprintf(stderr,"QLTOOLS for UNIX (version: %s)\n",VERSION);
#endif
#endif

    fprintf(stderr,"Giuseppe Zanetti  ");
    fprintf(stderr,"via Vergani, 11 - 35031 Abano Terme (Padova) ITALY\n");
    fprintf(stderr,"e-mail: beppe@sabrina.dei.unipd.it\n");
    fprintf(stderr,"Version 2.01:Valenti Omar, Via Statale 127, Casalgrande (RE)\n");
    fprintf(stderr,"e-mail: sinf7@imovx2.unimo.it   sinf7@imoax1.unimo.it\n");
    fprintf(stderr,"Version 2.02:Richard Zidlicky, rdzidlic@cip.informatik.uni-erlangen.de\n");
	exit(1);
}

static void print_info(void)
{
    U32 i;

    printf("Disk ID          : ");

    for (i=0;i<4;i++)
	{
	    printf("%c",b0[i]);
	}
    printf("\nDisk Label       : ");

    for (i=0;i<10;i++)
	{
	    printf("%c",b0[4+i]);
	}

    printf("\nsectors per track: %i\n",gsectors);
    printf("sectors per cyl. : %i\n",gspcyl);
    printf("number of cylind.: %i\n",gtracks);
    printf("allocation block : %i\n",allocblock);
    printf("sector offset/cyl: %i\n",goffset);

    printf("free sectors     : %li\n",GET_FREE());
    printf("good sectors     : %li\n",GET_GOOD());
    printf("total sectors    : %li\n",GET_TOTAL());

    printf("directory is     : %lu sectors and %lu bytes\n",bleod,byeod);

    printf("\nlogical-to-physical sector mapping table:\n\n");
    for (i=0;i<gspcyl;i++) printf("%x ",(U8)ltp[i]);
    printf("\n");

    if(ql5a)
     {
       printf("\nphysical-to-logical sector mapping table:\n\n");
       for (i=0;i<gspcyl;i++) printf("%x ",(U8)ptl[i]);
     }
    printf("\n");
}

static void print_entry(U8 *entry, int fnum, int del)
{
    U8 c;
    U32 i;

    if (entry==NULL) return;

    if (lwrd(entry)+wrd(entry+14)==0) return;

	printf("%3i: ",fnum);

    printf(del ? "deleted  ":"         ");

    for (i=0;i<min(wrd(entry+14),36);i++)
	{
	    c=*(entry+16+i);
	    printf("%c",(c));
	}

	{
	    for(i=0;i<38-wrd(entry+14);i++) printf(" ");

	    switch(*(entry+5))
		{
		case 0: printf("   \t\t");     break;
		case 1: printf("E %8ld\t",lwrd(entry+6)); break;
		case 2: printf("Rel  \t\t"); break;
		case 3: printf("Thor dir \t"); break;
		case 4: printf("CST  dir \t");break;
		case 255:printf("->  \t\t"); break;
		default:printf("(type %3d)",*(entry+5)); break;
		}

	    printf("    %ld",lwrd(entry) - 64);
	}
}

static void print_dir(void)
{
    int i,del;
    U8 *entry;
    U32 d;


	{
	    for (i=0;i<10;i++)
		{
		    printf("%c",b0[4+i]);
		}
	    printf("\n");

	    printf("%li/%li sectors.\n\n",GET_FREE(),GET_GOOD());
	}

    for (d=64;d <gssize*bleod+byeod;d+=64)
	{
	    entry=NULL;del=0;
	    if(lwrd(pdir+d)+wrd(pdir+d+14)!=0) {
	         entry=pdir+d; del=0;
	    }
	    if (entry == NULL) continue;

	    print_entry(entry,d>>6,del);
	    printf("\n");
	}
}

static void dump_entry(U8 *entry, int fnum, int del)
{
    U32 i;
    char fname[37];

    if (entry==NULL) return;

    if (lwrd(entry)+wrd(entry+14)==0) return;

	printf("%3i: ",fnum);

    printf(del ? "deleted  ":"         ");

    for (i=0;i<min(wrd(entry+14),36);i++)
	{
	    fname[i]=*(entry+16+i);
	    printf("%c",fname[i]);
	}
     fname[i]='\0';

	{
	    for(i=0;i<38-wrd(entry+14);i++) printf(" ");

	    switch(*(entry+5))
		{
		case 0: printf("   \t\t");     break;
		case 1: printf("E %8ld\t",lwrd(entry+6)); break;
		case 2: printf("Rel  \t\t"); break;
		case 3: printf("Thor dir \t"); break;
		case 4: printf("CST  dir \t");break;
		case 255:printf("->  \t\t"); break;
		default:printf("(type %3d)",*(entry+5)); break;
		}

	    printf("    %ld",lwrd(entry) - 64);
	}
	cat_file(fnum,fname);
}

static void dump_files(void)
{
    int del;
    U8 *entry;
    U32 d;

    printf("Dumping files.\n");
    for (d=64;d <gssize*bleod+byeod;d+=64)
	{
	    entry=NULL;del=0;
	    if(lwrd(pdir+d)+wrd(pdir+d+14)!=0) {
	         entry=pdir+d; del=0;
	    }
	    if (entry == NULL) continue;

	    dump_entry(entry,d>>6,del);
	    printf("\n");
	    fflush(stdout);
	}
}

void make_convtable(int verbose)
{
    int i,si,tr,se,uxs,sectors;

    if (verbose)
	{
	    printf("\nCONVERSION TABLE\n\n");
	    printf("logic\ttrack\tside\tsector\tunix_dev\n\n");
	}

    sectors=gclusters*allocblock;
#ifdef TABLE
    if (sectors > MAXSECT)
         {printf("too many %d sectors, increase MAXSECT\n",sectors);
          exit(1);}
#else
    if(verbose)
#endif
    for(i=0; i<sectors; i++)
	{
#ifdef TABLE
	    tr=i / gspcyl;
	    ls=i % gspcyl;
	    ps = ltp[ls];
	    si=(ps & 0x80) != 0;
	    ps &= 0x7F;
	    ps += goffset*tr;
	    se = ps % gsectors;
	    uxs = tr*gspcyl+gsectors*si+se;
	    convtable[i] = uxs;
#if (defined(QDOS) || defined(MSDOS))
	    conv_side[i]=si;
	    conv_sector[i]=se;
	    conv_track[i]=tr;
#endif
#else /* not TABLE */
     tr=LTP_TRACK(i); si=LTP_SIDE(i);
     se=LTP_SCT(i);
	    uxs = tr*gspcyl+gsectors*si+se;
#endif /* TABLE */

	    if (verbose)
		{
		    printf("%i\t%i\t%i\t%i\t%i\n",i,tr,si,se,uxs);
		}
	}
}

int read_cluster(char *p, int num)
{
    int sect;
    U32 i;
    int r=0;

    for(i=0;i<allocblock;i++){
       sect=num*allocblock+i;

#ifdef MSDOS
    /* biosdisk(RESET, drive , 0, 0, 0, 0, p+gssize*i); */
    biosdisk(READ, drive ,LTP_SIDE(sect),LTP_TRACK(sect),LTP_SECT(sect),1,p+gssize*i);
#else
    err=lseek(fd,LTP(sect),SEEK_SET);
    if (err<0) perror("read_cluster : lseek():");
    r += err=read(fd,p+gssize*i,gssize);
    if (err<0)
	perror("read block: read(): ");
#endif

    }
    return r;
}

void write_cluster(char *p, int num)
{
    int sect;
    U32 i;

    for(i=0;i<allocblock;i++){
       sect=num*allocblock+i;

#ifdef MSDOS
    /* biosdisk(RESET, drive , 0, 0, 0, 0, p+gssize*i); */
    biosdisk(WRITE, drive ,LTP_SIDE(sect),LTP_TRACK(sect),LTP_SECT(sect),1,p+gssize*i);
#else
    err=lseek(fd,LTP(sect),SEEK_SET);
    if (err<0)
	perror("write cluster: lseek():");
    r += err= write(fd,p+gssize*i,gssize);
    if (r<0)
	perror("write cluster: write():");

#endif

    }

}

void read_b0fat(int argconv)
    {

    int status;

    gssize=512;

#ifdef MSDOS

    /* reset the disk */

    status = biosdisk(READ, drive, 0, 10, 1, 1, b0);
    if (status != 0) fprintf(stderr,"Disk not ready (continuing...)\n");

    /* door change signal ? */

    if (status == 0x06) status = biosdisk(READ, drive, 0, 0, 1, 1, b0);

    status = biosdisk(RESET, drive , 0, 0, 0, 0, b0);

    if (status != 0) fprintf(stderr,"Disk not ready (continuing...)\n");

    /* read block 0 */

    biosdisk(READ, drive ,0,0,1,1,b0);
#else

    err=lseek(fd,SECT0,SEEK_SET);
    if (err<0)
       perror("read_cluster0: lseek():");

    err=read(fd,b0,gssize);
    if(err<0)
	perror("read_cluster0: read():");
#endif

    /* is this a QL disk ? */

    if ((*(b0) != 'Q') || (*(b0+1) != 'L'))
	{
	    fprintf(stderr,"\nNot a QL disk !!!\n\n");
	    exit(1);
	}
    if ((*(short*)(b0+2)) == 0x3541)   /* == QL5A ? */
         ql5a=1;
    else ql5a=0;

    gtracks=GET_TRACKS();
    gsectors=GET_SPT();
    gspcyl=GET_SPCYL();
    gsides=gspcyl/gsectors;
    goffset=GET_SOFF();
    bleod=GET_DIRBL();
    byeod=GET_DIROF();
    allocblock=GET_SPCL();
    gclusters=gtracks*gspcyl/allocblock;

    /* read ltp and ptl tables */

    read_ltp();

    make_convtable(argconv);

    read_fat();

}

void write_b0fat(void)
{
  int i;

  for(i=0; FAT_FILE(i)==0xf80; i++)
    write_cluster(b0+i*allocblock*gssize,i);
}


void read_fat(void)
{
   int i;

   for(i=0; FAT_FILE(i)==0xf80; i++)
       read_cluster(b0+i*allocblock*gssize,i);
}

void read_ltp(void)
{
    U32 i;

    ltp=(char *)malloc(gspcyl);
    if (ql5a)
       ptl=(char *)malloc(gspcyl);

    for (i=0;i<gspcyl;i++)
	{
	    ltp[i] = *(b0+40+i);
         if(ql5a)
	    ptl[i] = *(b0+58+i);
	}
}

static U32 match_file(char *fname)
{
    int match;
    long int r=0L;
    char c;
    U32 d,len,i;

    len=strlen(fname);

    for (d=64;d <gssize*bleod+byeod;d+=64)
	{
	    match=1;

	    if (wrd(pdir+d+14) == len)
		{
		    for (i=0;i<len;i++)
			{
			    c=*(pdir+d+16+i);

			    if (c != fname[i]) match=0;
			}

		    if (match)
			{
			    r=d>>6;
			    break;
			}
		}
	}

    return(r);
}

static void writefile(void)
{
    char dati_file[1536];
    int i,filenew;
    int fl,hole;
    U32 y,offset;
    U8 *entry;

    U32 free_sect;

    int block=0;
    int end_dati;


    if (match_file(nfile))
	{
	    fprintf(stderr,"file %s already exists, delete by hand\n",nfile);
	    exit(2);
	}


#ifdef MSDOS
    if ((fl=open(nfile,O_RDONLY | O_BINARY)) <0)
	{
	    printf("File %s not open",nfile);
	    perror(NULL);
	    exit(1);
    }
    y=filelength(fl)+64;    /* Add the 64 byte of QL Header */
#else
    if ((fl=open(nfile,O_RDONLY))==-1)
	{
	    perror("write file: could not open input file ");
	    exit(1);
	}
    y=lseek(fl,0,SEEK_END)+64;
    if (y<0)
	perror("write file: lseek():");
#endif

    err=lseek(fl,0,SEEK_SET);   /* reposition to zero!!! */
    if (err<0)
       perror("write file: lseek() on input file : ");


    free_sect=GET_FREE();

    /* controlla se il file Š troppo grande */
    if (y>free_sect*gssize)
	{
	    printf("File %s too large",nfile);
	    exit(1);
    }


    offset=64;
    while ((lwrd(pdir+offset+0)+wrd(pdir+offset+14)>0) && (offset<gssize*bleod+byeod)) offset+=64;
    if (offset>=gssize*bleod+byeod)
	{ hole=0;
	  offset=gssize*bleod+byeod;
	/* printf("append to directory\n"); */
	}
    else {
	hole=1;
	/* printf("found place in directory\n"); */
       }

    /* fprintf(stderr,"Bye %i , Ble %i\n",byeod,bleod);  */

    if ((byeod==gssize) && ((bleod%allocblock)==2) && !hole)
	{
	    i=alloc_new_cluster(0,block_max);
	    if(i<0)
		{
		    fprintf(stderr,"write file: no free cluster\n");
		    exit(1);
		}
	    byeod=0;
	    bleod+=1;
	    block++;
	    block_dir[block_max]=i;
	    block_max++;
	}

/*     printf("offset : %d\n",offset); */

    entry=pdir+offset;

    wr_lwrd(entry,y);             /* lunghezza del file */
    *(entry+5)=0;                 /* tipo del file */
    wr_wrd(entry+14,min(strlen(nfile),36));     /* lunghezza del nome del file */
    strncpy(pdir+offset+16,nfile,36);   /* file name */

    /* copio il file */

    if (y<1536) end_dati=y;
    else end_dati=1536;
    read(fl,dati_file+64,end_dati-64);  /* copio i dati anche nell'header del file */

    memcpy(dati_file,entry,64);     /* second copy of header */

    /* scrittura del file */

    filenew=(offset+1)>>6;
    while ( y>0 )
	{
	    i=alloc_new_cluster(filenew,block);
	    if (i<0){
		fprintf(stderr,"filewrite failed : no free cluster\n");
		exit(1);
	        }
	    block++;
	    write_cluster(dati_file,i);
	    y-=allocblock*gssize;

	    err=read(fl,dati_file,allocblock*gssize);
	    if (err<0)
		perror("write file: read on input file:");
	}
    close(fl);

    SET_FREE(free_sect-block*allocblock); /* scrive i byte rimasti liberi;*/

    if (!hole) byeod+=64;
    if (byeod>gssize) {
	byeod=byeod%gssize;
	bleod+=1;
    }

    SET_DIRBL(bleod);
    SET_DIROF(byeod);

    write_b0fat();              /* write_cluster(b0,0); */
    dir_write_back();

    exit(0);
}

int flp_main(char *opt, char *drv)
{
    int i;

    int argdir,argdump,arginfo,argfnum,argmap,argconv,argread;
    int argshort,argwnum,argformat,argexec,argldir;
    int argdfnum;
    char frmt[3];
    static char argfname[255]="";

    argdir=argdump=arginfo=argfnum=argmap=argconv=argshort=argread=0;
    argwnum=argdfnum=argformat=argldir=argexec=0;
    strcpy(argfname,"");


	    switch(*opt)
		{
		case 'l': argdir=1; break;
		case 'd': argdump=1; break;
		case 'm': argmap=1; break;
		case 'c': argconv=1; break;
//		case 'n': argread=1; argfnum=atol(argv[i]+2); break;
//		case 'w': argwnum=1; strcat(nfile,argv[i+1]); break;
//		case 'r': argdfnum=atol(argv[i]+2); break;
//		case 'f': argformat=1; strncpy(frmt,&argv[i][2],2); frmt[2]=0; break;
//		case 'x': argexec=1;argfnum=atol(argv[i]+2); break;
		default: f_usage("bad option"); break;
		}

#ifdef MSDOS

    switch(*drv)
	{
	case 'a':
	case 'A': drive=0; break;

	case 'b':
	case 'B': drive=1; break;

	default: f_usage("Bad drive: use a: or b:"); break;
	}
#else

    fd=open(argv[1],O_RDWR);

    if (fd<0)
	{perror("could not open image");
	 f_usage("image file not opened");}

#endif

    if (argformat) format(frmt,argfname);

    read_b0fat(argconv);

    /* read the directory map */

    pdir=(U8 *) malloc(gssize*allocblock*(bleod+2));
    read_dir();


    if (argmap) print_map();
    if (arginfo) print_info();

    if (argdir) print_dir();
    if (argdump) dump_files();
    if (argread) cat_file((long int)argfnum,"FILENAME");
    if (argdfnum != 0) del_file((long int)argdfnum);
    if (argwnum) writefile();
    if (argexec) set_header(argfnum,argfname);
    if (strcmp(argfname,"") != 0)
	{
	    argfnum=match_file(argfname);

	    if (argfnum == 0)
		{
		    fprintf(stderr,"file not found\n");
		    exit(2);
		}

	    cat_file((long int)argfnum,"FILENAME2");
	}


#ifdef MSDOS
#else
    close(fd);
#endif

    return (0);
}

void read_dir(void)
{
    int fn,cl;
    U32 i;

    for (i=0;i< gclusters;i++)
	{

	    cl= FAT_CL(i);
	    fn = FAT_FILE(i);

	    if (fn == 0x00)
		{
		    block_dir[block_max]=i;
		    block_max++;
		    read_cluster(pdir+gssize*allocblock*cl,i);
		}
}}

void print_map(void)
{
    int fn,cl,fnum,del;
    U8 *entry;
    U32 i;

    printf("\nblock\tfile\tpos\n\n");

    fnum=0;
    for (i=0;i< gclusters; i++)
	{
	    entry=NULL;
	    del=0;
	    cl= FAT_CL(i);
	    fn = FAT_FILE(i);

	    printf("%d\t%d\t%d\t",i,fn,cl);

	    if ((fn & 0xFF0) == 0xFD0 && (fn & 0xf) != 0xF)
		{
		    printf("erased %2d\t",fn&0xF);
		    del=1;
		    if (cl==0){
			read_cluster(b,i);
			entry=b;
			del=1;
			fnum=fn&0xf;
		    }
		}

	    switch(fn)
		{
		case 0x000: printf("directory"); break;
		case 0xF80: printf("map"); break;
		case 0xFDF: printf("unused"); break;
		case 0xFEF: printf("bad"); break;
		case 0xFFF: printf("not existent"); break;
		default:
		    if(del==0)
			{entry=fn*64+pdir;del=0;fnum=fn;}
		    break;
		}
	    print_entry(entry,fnum,del);
	    printf("\n");
	}
}

void set_header(U32 ni, char *name)
{
    int h;
    U32 ii,i;

    ii=ni*64;

    if (ii>=bleod*gssize+byeod )
       {fprintf(stderr,"filenumber %d out of bounds\n",ni); exit(1);}
    if (lwrd(pdir+ii)+wrd(pdir+ii+14)==0)
       {fprintf(stderr,"file %d deleted ??\n",ni);exit(1);}
    if (!strcmp(name,""))
       f_usage("\n dataspace length expected\n");

    h=atol(name);
    *(pdir+ii+5)=1;
    wr_lwrd(pdir+ii+6,h);

    for(i=1;i<gclusters; i++)
      if (FAT_FILE(i)==ni && FAT_CL(i)==0) break;
 /* if (i==gclusters) error .... */

    read_cluster(b,i);
    *(b+5)=1;
    wr_lwrd(b+6,h);
    write_cluster(b,i);

    dir_write_back();

    exit(0);
}

void dir_write_back(void)
{
    int i;

    for(i=0;i<block_max;i++)
      write_cluster(pdir+gssize*allocblock*i,block_dir[i]);
}

int alloc_new_cluster(int fnum, int iblk)
{
    U32 fflag,i,ok=0;

    for (i=lac+1; i<gclusters;i++)
	{
	    fflag = FAT_FILE(i);
	    if ((fflag>>4)==0xFD)
		{
		    ok=1;
		    SET_FAT_FILE(i,fnum);
		    SET_FAT_CL(i,iblk);
		    lac=i;
		    break;
		}
	}
    return (ok ? i : -1);
}

void free_cluster(long i)
{

    int fn,dfn;

    if (i>0){
	fn=FAT_FILE(i);
	dfn=0xFD0 | (0xf & fn);
	SET_FAT_FILE(i,dfn);}
    else {fprintf(stderr,"freeing cluster 0 ???!!!\n"); exit(1);}
}

void format(char * frmt,char *argfname)
{
    static char ltp_dd[]= {0,3, 6,0x80, 0x83,0x86, 1,4,
	7,0x81, 0x84, 0x87, 2,5, 8,0x82, 0x85,0x88};

    static char ptl_dd[]= {0,6, 0x0c,1, 7,0x0d,
	2,8, 0x0e,3, 9,0x0f, 4,0x0a, 0x10,5, 0x0b, 0x11};

    static char ltp_hd[]= {
      0,       2,    4,    6,    8,  0xa,  0xc,  0xe, 0x10,
      0x80, 0x82, 0x84, 0x86, 0x88, 0x8a, 0x8c, 0x8e, 0x90,
      1,       3,    5,    7,    9,  0xb,  0xd,  0xf, 0x11,
      0x81, 0x83, 0x85, 0x87, 0x89, 0x8b, 0x8d, 0x8f, 0x91
    };

    U32 cls;

/* printf("format : %s\n",frmt); */

    if (!strcmp(frmt,"dd"))        /* 720 K format */
	{
	    memcpy(b0,"QL5A          ",14);
	    ql5a=1;
	    strncpy(b0+4,argfname,(strlen(argfname)<=10 ? strlen(argfname) : 10));
	    memcpy(b0+40,ltp_dd,18);
	    memcpy(b0+58,ptl_dd,18);

	    gsides=2;
	    allocblock=3;
	    SET_TRACKS(80);
	    gtracks=80;
	    SET_SPT(9);           /* b0+26 */
	    SET_SPCYL(18);          /* b0+28 */
	    gspcyl=18;
	    goffset=5;
	    SET_SOFF(5);          /* b0+38 */
	    SET_DIRBL(0);         /* b0+34*/
	    SET_DIROF(64);        /* b0+36 */
	    SET_FREE(1434);       /* b0+20 */
	    gsectors=9;
	    SET_TOTAL(1440);      /* b0+24 */
	    SET_GOOD(1440);       /* b0+22 */
	    SET_SPCL(allocblock);  /* b0+32 */

	    SET_FAT_FILE(0,0xF80);  /* FAT entry for FAT */
	    SET_FAT_CL(0,0);
	    SET_FAT_FILE(1,0);          /*  ...  for directory */
	    SET_FAT_CL(1,0);
	    gclusters=gtracks*gspcyl/allocblock;
	    for(cls=2; cls<gclusters; cls++){   /* init rest of FAT */
	      SET_FAT_FILE(cls,0xFDF);
	      SET_FAT_CL(cls,0xFFF);
	    }
	    read_ltp();
	    make_convtable(0);
	    write_b0fat();              /*  write_cluster(b0,0); */

	    exit(0);
	}
    else if (!strcmp(frmt,"hd"))
      {
      	    memcpy(b0,"QL5B          ",14);
	    ql5a=1;
	    strncpy(b0+4,argfname,(strlen(argfname)<=10 ? strlen(argfname) : 10));
	    memcpy(b0+40,ltp_hd,36);


	    gsides=2;
	    allocblock=3;
	    SET_TRACKS(80);
	    gtracks=80;
	    SET_SPT(18);           /* b0+26 */
	    SET_SPCYL(36);         /* b0+28 */
	    gspcyl=36;
	    goffset=2;
	    SET_SOFF(2);          /* b0+38 */
	    SET_DIRBL(0);         /* b0+34*/
	    SET_DIROF(64);        /* b0+36 */
	    SET_FREE(2871);       /* b0+20 */
	    gsectors=18;
	    SET_TOTAL(2880);      /* b0+24 */
	    SET_GOOD(2880);       /* b0+22 */
	    SET_SPCL(allocblock);  /* b0+32 */

	    SET_FAT_FILE(0,0xF80);  /* FAT entry for FAT */
	    SET_FAT_CL(0,0);
	    SET_FAT_FILE(1,0xf80);
	    SET_FAT_CL(1,1);
	    SET_FAT_FILE(2,0);          /*  ...  for directory */
	    SET_FAT_CL(2,0);
	    gclusters=gtracks*gspcyl/allocblock;
	    for(cls=3; cls<gclusters; cls++){   /* init rest of FAT */
	      SET_FAT_FILE(cls,0xFDF);
	      SET_FAT_CL(cls,0xFFF);
	    }
	    read_ltp();
	    make_convtable(0);
	    write_b0fat();              /*  write_cluster(b0,0); */

	    exit(0);
      }

    exit(1);
}
