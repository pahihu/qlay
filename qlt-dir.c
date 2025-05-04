/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	QLAY TOOLS DIR
*/

#include "qlayt.h"

static void printhead(U8 *head)
{
int	slen;

/*int i;for(i=0;i<64;i++) printf("%02x ",head[i]);*/
	slen=head[15];
	if (slen>36) slen=36;
	strncpy(qdosname,&head[16],36);
	qdosname[slen]='\0';
	printf("%-36s",qdosname);
	if (head[5]==1) printf("%ld",getlong(&head[6]));
	printf("\n");
}

/* return 0 if 's' does exist and no errors occured */
/* return 1 if 's' was found at position 'rpos' */
static int alreadyexists(FILE *f, char *s, U32 *rpos)
{
char	t[37];
U32	fpos;
int	rv,n,i,fnlen;
U8 	tmp[64];

	fpos=ftell(f);
	fseek(f,0L,0);
	strncpy(t,s,36);
	t[36]='\0';
	for(i=0;i<(int)strlen(t);i++) t[i]=toupper(t[i]);

	rv=0;
	*rpos=0;
	while ((n=fread(tmp,sizeof(U8),64,f))>0) {
		if(n!=64) {
			fprintf(stderr,"Error: corrupt directory file %s, wrong length\n",dirfname);
			exit(1);
		}
		strncpy(qdosname,&tmp[16],36);
		fnlen=tmp[15];
		if (fnlen>36) fnlen=36; /* don't bother with error messages here */
		for(i=0;i<fnlen;i++) qdosname[i]=toupper(qdosname[i]);
if(0)printf("%s %s\n",t,qdosname);
		if (strlen(t)!=strlen(qdosname)) continue;
		if (strncmp(t,qdosname,36)==0) {
			rv=1;
			*rpos=ftell(f)-64;
			break;
		}
	}
	/* reset file pointer */
	fseek(f,fpos,0);
	return rv;
}

/* return -1 if error */
int getxtcc(FILE *f, U32 *d)
{
U8 tmp[8];

	fseek(f,-8L,2);
	fread(tmp,sizeof(U8),8,f);
	if (strncmp(tmp,"XTcc",4)==0) {
		*d=getlong(&tmp[4]);
	} else {
		fprintf(stderr,"Error: could not find XTcc datasize\n");
		return -1;
	}
	return 0;
}

void showxtcc(char *fname)
{
FILE	*f;
U32	datasize;

	if ((f=fopen(fname,"rb")) == NULL) {
		fprintf(stderr,"Cannot open %s\n",fname);
		exit(1);
	}
	if (getxtcc(f,&datasize)!=0) exit(1);
	printf("Datasize = %ld\n",datasize);
}

void showzip(char *fname)
{
FILE	*f;
int	c,n,found;

	if ((f=fopen(fname,"rb")) == NULL) {
		fprintf(stderr,"Cannot open %s\n",fname);
		exit(1);
	}

	found=0;
	while ((c=fgetc(f))!=EOF) {
		if (c!=0x4A) continue;
		if (fgetc(f)!=0xFB) continue;
		if (fgetc(f)!=72) continue;
		if (fgetc(f)!=0) continue;
		if (fgetc(f)!='Q') continue;
		if (fgetc(f)!='D') continue;
		if (fgetc(f)!='O') continue;
		if (fgetc(f)!='S') continue;
		if (fgetc(f)!='0') continue;
		if (fgetc(f)!='2') continue;
		if (fgetc(f)!=0) continue;
		if (fgetc(f)!=0) continue;
		/* found iT */
		n=fread(head,sizeof(U8),64,f);
		if (n!=64) continue;
		found++;
		printhead(head);
	}
	if (found) fprintf(stderr,"Found %d entries\n",found);
	else  fprintf(stderr,"No QDOS markers found\n");
}

/* remove empty entries
fopen&fclose dirfile by name 'dirfname'
*/

static void refreshdir(void)
{
FILE	*qldf;
U32	rpos,wpos;
int	mt,i,n,cleanedup;

	if ((qldf=fopen(dirfname,"rb+")) == NULL) {
		fprintf(stderr,"Cannot open %s\n",dirfname);
		usage();
	}
	cleanedup=0;
	rpos=wpos=0;
	while ((n=fread(head,sizeof(U8),64,qldf))>0) {
		if(n!=64) {
			fprintf(stderr,"Error: corrupt directory file %s, wrong length\n",dirfname);
			exit(1);
		}
		rpos+=64;
		if (wpos!=rpos-64) {
			fseek(qldf,wpos,0);
			fwrite(head,sizeof(U8),64,qldf); /* move iT */
			fseek(qldf,rpos,0);
		}

		/* analyse the content */
		mt=1;
		for(i=0;i<64;i++) if (head[i]!=0) {mt=0;break;}

		if (!mt) wpos+=64; else cleanedup++;
	}
	if (cleanedup) {
		truncate(dirfname,wpos);
		printf("Cleaned up empty space in directory file %s\n",dirfname);
	}
	fclose(qldf);
}

void listqld(int verbose)
{
FILE	*qldf;
int	number,slen,nof;

	if ((qldf=fopen(dirfname,"rb")) == NULL) {
		fprintf(stderr,"Cannot open %s\n\n",dirfname);
		usage();
	}
	nof=0;
	while ((number=fread(head,sizeof(U8),64,qldf))>0) {
		if(number!=64) {
			fprintf(stderr,"Error: corrupt directory file %s, wrong length\n",dirfname);
			exit(1);
		}
		slen=head[15];
		if (slen>36) slen=36;
		strncpy(qdosname,&head[16],36);
		qdosname[slen]='\0';
		printf("%-36s",qdosname);
		if (head[5]==1) printf("%ld",getlong(&head[6]));
		printf("\n");
		nof++;
	}
	fprintf(stderr,"Found %d files in directory %s\n",nof,dirfname);
	exit(0);
}

/*
write a header for filename 'ifname' to file 'f' with 'datasize' if 'dz'
write at current file position
check whether ifname exists, check whether ifname is not already in 'f'
take a datasize when dz==1, find XTcc when dz==2
return -1 if error (error message is printed here)
*/
static int writeheader(FILE *f, int dz, U32 datasize, char *ifname)
{
int	i,fnlen;
struct stat s;
U32	ftime,dummy;
FILE	*infile;

	/* is 'ifname' readable */
	if ((infile=fopen(ifname,"rb")) == NULL) {
		fprintf(stderr,"%s: cannot open %s, skipping\n",PROGNAME,ifname);
		return -1;
	}

	/* prepare the header */
	for(i=0;i<64;i++) head[i]=0;

	/* get file length */
	fseek(infile,0L,2);
	putlong(head,ftell(infile)+64);	/* +64 ! */
/*	fseek(infile,0L,0);*/

	fnlen=strlen(ifname);
	if (fnlen>36) {
		fprintf(stderr,"Error, filename too long %s, %d\n", ifname,fnlen);
		return -1;
	}
	putword(&head[14],fnlen);
	strncpy(&head[16],ifname,36);

	if (alreadyexists(f,ifname,&dummy)) {
		fprintf(stderr,"Error: file %s is already in directory\n",ifname);
		return -1;
	}

	if (dz==2) if (getxtcc(infile,&datasize)!=0) return -1;
	if (dz) {
		putlong(&head[6],datasize);
		head[5]=1;
	}

	/* find the file's modification time */
	if(stat(ifname,&s)!=0) {
		fprintf(stderr,"error: cannot stat %s, skipping\n",ifname);
		fclose(infile);
		return -1;
	}
	if(0)printf("TIME %x ",s.st_mtime);
	ftime=s.st_mtime+QDOSTIME;

	putlong(&head[0x34],ftime);

	printf("%-36s",ifname);
	if (dz) printf("%ld ",getlong(&head[6]));
	printf("\n");

	/* write out to file */
	if (fwrite(head,sizeof(U8),64,f)!=64) {
		fprintf(stderr,"Error: file %s could not be written\n",dirfname);
		return -1;
	}
	fclose(infile);
	return 0;
}

void insertfile(int dz, U32 datasize, char *addfname)
{
FILE	*qldf;

	/* simply append at end */
	if ((qldf=fopen(dirfname,"ab+")) == NULL) {
		fprintf(stderr,"Cannot open %s\n\n",dirfname);
		usage();
	}
	fseek(qldf,0L,2);
	if ( (ftell(qldf)&0x3f) != 0 ) {
		fprintf(stderr,"Error: corrupt directory file %s, wrong length\n",dirfname);
		exit(1);
	}

	if (writeheader(qldf,dz,datasize,addfname)==-1) {
		fclose(qldf);
		exit(1);
	}

	fprintf(stderr,"Appended file %s\n",addfname);
	exit(0);
}

/* if 'clean' the directory will be cleaned and refreshed */
void removefile(char *fname, int clean)
{
FILE	*qldf;
U32	fpos;
int	i;

	if ((qldf=fopen(dirfname,"rb+")) == NULL) {
		fprintf(stderr,"Cannot open directory file %s\n\n",dirfname);
		exit(1);
	}
	if (alreadyexists(qldf,fname,&fpos) == 0) {
		fprintf(stderr,"File %s not found\n",fname);
		exit(1);
	}
	fseek(qldf,fpos,0);
	for(i=0;i<64;i++) head[i]=0;
	fwrite(head,sizeof(U8),64,qldf);

	if (clean) printf("Removed file %s\n",fname); /* silent for update */
	fclose(qldf);

	if (clean) refreshdir();
}

void updatef(char *fname)
{
FILE	*qldf;
U32	fpos,datasize;
int	i,dz;

	if ((qldf=fopen(dirfname,"rb+")) == NULL) {
		fprintf(stderr,"Cannot open directory file %s\n\n",dirfname);
		exit(1);
	}
	if (alreadyexists(qldf,fname,&fpos) == 0) {
		fprintf(stderr,"File %s not found\n",fname);
		exit(1);
	}

	/* fileposition is restored by alreadyexists (it was 0), so: update here */
	fseek(qldf,fpos,0);
	fread(head,sizeof(U8),64,qldf);

	/* dz needed for scripting... */
	dz=head[5];
	datasize=getlong(&head[6]); /* don't bother if !=0 here */

	for(i=0;i<64;i++) head[i]=0;
	fseek(qldf,fpos,0);
	fwrite(head,sizeof(U8),64,qldf);

	/* now actually update and check all */
	fseek(qldf,fpos,0);
	writeheader(qldf, dz, datasize, fname);

	fprintf(stderr,"Updated\n");
}

void updatea(void)
{
;
}

void createdir(char *lstfname, int create)
{
int	dz=0,i,ai,no_ascii_file,noif,noof;
FILE	*lstfile,*outfile;
char	*p,*q;
U32	datasize;

	if (create) { /* don't overwrite stuff if it existed */
		if ((outfile=fopen(dirfname,"rb")) != NULL) {
			printf("%s: %s already exists, exiting\n",PROGNAME,dirfname);
			fclose(outfile);
			exit(1);
		}
		fclose(outfile);
		/* now reopen and create it */
		if ((outfile=fopen(dirfname,"wb+")) == NULL) {
			fprintf(stderr,"%s: cannot open %s for write\n",PROGNAME,dirfname);
			exit(1);
		}
	} else { /* append new stuff to end if it existed */
		if ((outfile=fopen(dirfname,"ab+")) == NULL) {
			fprintf(stderr,"%s: cannot open %s for append\n",PROGNAME,dirfname);
			exit(1);
		}
		/* we are at the end, but let's force it */
		fseek(outfile,0L,2);
		if ((ftell(outfile)&0x3f) != 0) {
			fprintf(stderr,"error: file format error in %s\n",dirfname);
			exit(1);
		}
	}

	if ((lstfile=fopen(lstfname,"r")) == NULL) {
		fprintf(stderr,"Cannot open list file %s\n",lstfname);
		exit(1);
	}

	noif=0;
	/* how many lines? */
	while(fgets(ifname,LINESIZE-1,lstfile)!=NULL) {
		if(0)printf("%s",ifname);
		noif++;
		no_ascii_file=0;
		for(i=0;i<(int)strlen(ifname);i++) {
			if (!(isascii(ifname[i]))) {
				no_ascii_file=1;
				break;
			}
		}
		if (no_ascii_file) {
			fprintf(stderr,"Error: %s is not a valid listfile\n",lstfname);
			exit(1);
		}
	}
	fseek(lstfile,0L,0);

	/* now loop through input files */
	ai=0;noof=0;
	while(noif!=0) {
		noif--;
		ai++;
		dz=0;
		fgets(lstline,LINESIZE-1,lstfile);
		strcpy(lstline2,lstline);
		/* get first item */
		p=strtok(lstline2," \t\n");
		if (p==NULL) continue;
		if(0)printf("ST%s* ",p);
		i=sscanf(p,"%s",ifname);
		q=p+strlen(p)+1;
		p=strtok(q," \t\n");
		if (p!=NULL) {
			if(0)printf("DZ%s* ",p);
			i=sscanf(p,"%ld",&datasize);
			if (i==1) {
				dz=1;
			} else {
				fprintf(stderr,"Error: incorrect format datasize: %s\n",p);
				fprintf(stderr,">>%s<<\n",lstline);
				fprintf(stderr,"Skipping\n");
				continue;
			}
		}
		if (writeheader(outfile, dz, datasize, ifname)<0) {
/*082g*/		continue;
			/*exit(1);*/
		}
		noof++;
	}
	if (create) {
		fprintf(stderr,"Created directory file %s with %d files\n",dirfname,noof);
	} else {
		fprintf(stderr,"Appended %d files to directory file %s\n",noof,dirfname);
	}
}

