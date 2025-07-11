/***************************************************************************
 * QLTOOLS
 *
 * Read a QL floppy disk
 *
 * (c)1992 by Giuseppe Zanetti
 *
 * Giuseppe Zanetti
 * via Vergani, 11 - 35031 Abano Terme (Padova) ITALY
 * e-mail: beppe@sabrina.dei.unipd.it
 *
 * Valenti Omar
 * via Statale,127 - 42013 Casalgrande (REGGIO EMILIA) ITALY
 * e-mail: sinf7@imovx2.unimo.it
 * sinf7@imoax1.unimo.it
 * sinf@c220.unimo.it
 *
 * somewhat hacked by Richard Zidlicky, added formatting, -dl, -x option
 * rdzidlic@cip.informatik.uni-erlangen.de
 *
 * Seriously major rewrite (c) Jonathan R Hudson to support level 2
 * sub-directories, fix some bugs and make the code IMHO :-) rather more
 * maintainable. Split system specific code into individual directories and
 * added code for NT and OS2.
 *
 * $Log: qltools.c,v $
 * Revision 2.11  1996/07/14  11:57:07  jrh
 * Tidied up code
 *
 * Revision 2.10  1996/07/14  10:28:58  jrh
 * added extra info messages
 *
 * Revision 2.9  1995/11/29  18:34:53  root
 * Changed output routine to correctly decide if an XTcc block was
 * required.
 *
 * Revision 2.8  1995/10/22  14:11:36  root
 * Added VMS support
 *
 * Revision 2.7  1995/10/22  14:02:42  root
 * Added zeroing of some sectors
 *
 * Revision 2.6  1995/09/21  17:01:03  root
 * Release version 2.4
 *
 * Revision 2.5  1995/09/18  13:13:59  root
 * Full support for Level 2 directories
 *
 * Revision 2.4  1995/09/16  18:56:48  root
 * Placeholder, system specific code in
 *
 * Revision 2.3  1995/09/09  16:38:08  root
 * Added data structures for disk objects (directories, block 0 etc)
 * Did I fix a few more bugs too ?
 *
 * Revision 2.2  1995/09/06  12:13:17  root
 * Fixed null version record in DOS, and some other bugs
 *
 * Revision 2.1  1995/09/06  12:11:26  root
 * Initial jh version, fixed lots of bugs
 *
 ****************************************************************************/

static char rcsid[] = "$Id: qltools.c,v 2.11 1996/07/14 11:57:07 jrh Exp $";

#if defined (__OS2__) || defined (__NT__) || defined  (__MSDOS__)
# define DOS_LIKE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include "qltools.h"

/* -------------------------- globals ----------------------------------- */
int gsides, gtracks, gsectors, goffset, allocblock, gclusters, gspcyl;
BLOCK0 *b0;

/* -------------------------- 'local' globals --------------------------- */
static HANDLE fd;
static SDL *SList = NULL;	/* sub-directory list */
static unsigned int bleod;	/* directory block offset */
static unsigned int byeod;	/*           bytes        */
static QLDIR *pdir;		/* memory image of directory */
static long err;
static short tranql = 1;
static short list = 0;
static int ql5a;		/* flag */
static int block_dir[64];
static int block_max = 0;
static long lac = 0;		/* last allocated cluster */
static time_t timeadjust;
static char OWopt = 0;

/* -------------------------- Prototypes ------------------------------- */
void write_cluster (void *, int);
int read_cluster (void *, int);
void make_convtable (int);
int print_entry (QLDIR *, int, void *);
void free_cluster (long);
int alloc_new_cluster (int, int, short);
void format (char *, char *);
void dir_write_back (QLDIR *, SDL *, short *);
void set_header (int, long, QLDIR *, SDL *);
void read_b0fat (int);
void write_b0fat (void);
void read_fat (void);
void read_dir (void);
void print_map (void);
int RecurseDir (int, long, void *, int (*func) (QLDIR *, int, void *));
int find_free_cluster (void);

void *xmalloc (long alot)
{
    void *p = calloc (alot, 1);

    if (p == NULL)
    {
	perror ("! :");
	exit (0);
    }
    return p;
}


/* --------------------- byte order conversion --------------------------- */

ushort inline swapword (ushort val)
{
    return (ushort) (val << 8) + (val >> 8);
}

ulong inline swaplong (ulong val)
{
    return (ulong) (((ulong) swapword (val & 0xFFFF) << 16) |
		    (ulong) swapword (val >> 16));
}

static inline int maxdir (void)
{
    return (byeod >> 6) + (bleod * DIRSBLK);
}

ushort inline fat_file (ushort cluster)
{
    unchar *base = b0->map + cluster * 3;

    return (ushort) (*base << 4) + (ushort) ((*(base + 1) >> 4) & 15);
}

ushort inline fat_cl (ushort cluster)
{
    unchar *base = b0->map + cluster * 3;

    return (*(base + 1) & 15) * 256 + *(base + 2);
}

void inline set_fat_file (ushort cluster, ushort file)
{
    unchar *base = b0->map + cluster * 3;

    *base = file >> 4;
    *(base + 1) = ((file & 15) << 4) | (*(base + 1) & 15);
}

void inline set_fat_cl (ushort cluster, ushort clnum)
{
    unchar *base = b0->map + cluster * 3;

    *(base + 1) = (clnum >> 8) | (*(base + 1) & (~15));
    *(base + 2) = clnum & 255;
}


ushort inline FileXDir(ushort fnum)
{
    ushort fno = 0;
    if(fnum)
    {
	fno = swapword(fnum) - 1;
    }
    return fno;
}

inline QLDIR *GetEntry (int fn)
{
    QLDIR *entry;

    if (fn & 0x800)
    {
	entry = NULL;
    }
    else
    {
	entry = pdir + fn;
    }
    return entry;
}

short FindCluster (ushort fnum, ushort blkno)
{
    ushort file, blk;
    short i;

    for (i = 0; i < gclusters; i++)
    {
	file = fat_file (i);
	blk = fat_cl (i);
	if (file == fnum && blk == blkno)
	{
	    break;
	}
    }
    return (i == gclusters) ? -1 : i;
}

void cat_file (long fnum, QLDIR * entry)
{
    long flen;
    int i, ii, s, start, end;
    long qldata = 0;

#ifdef DOS_LIKE
    setmode (fileno (stdout), O_BINARY);
#endif

    if(entry->d_type == 255)
    {
	write(1, QLDIRSTRING, 16);
    }
    else
    {
	flen = swaplong (entry->d_length);	/* with the header */
	if (entry->d_type == 1)
	{
	    qldata = entry->d_datalen;
	}

	if (flen + swapword (entry->d_szname) == 0)
	{
	    fputs ("warning: file appears to be deleted\n", stderr);
	}
	else
	{
	    char *buffer = xmalloc (512 * allocblock);
	    long lblk = flen / (GSSIZE * allocblock);
	    long xblk = 0,xbyt = 0;
	    short needx = 1;

	    if(qldata)
	    {
		xblk = (flen - 8) / (GSSIZE * allocblock);
		xbyt = (flen - 8) % (GSSIZE * allocblock);
	    }

	    for (s = 0; s <= lblk; s++)
	    {
		if ((i = FindCluster (fnum, s)) != -1)
		{
		    read_cluster (buffer, i);
		    if (s == 0)
			start = 64;
		    else
			start = 0;
		    end = GSSIZE * allocblock;
		    if (s == lblk)
			end = flen % (GSSIZE * allocblock);
		    err = write (1, buffer + start, end - start);
		    if (err < 0)
			perror ("output file: write(): ");
		    if(qldata && s == xblk)
		    {
			needx = memcmp(buffer+start+xbyt, "XTcc", 4);
		    }
		}
		else
		{
		    fprintf (stderr, "** Cluster #%d of %.*s not found **\n",
			     s, entry->d_szname, entry->d_name);
		    err = lseek (1, GSSIZE * allocblock, SEEK_CUR);
		    /* leave hole */
		    if (err < 0)	/* non seekable */
			for (ii = 0; ii < allocblock * GSSIZE; ii++)
			    fputc ('#', stdout);
		}
	    }
	    free (buffer);

	    if (qldata && needx)
	    {
		static struct
		{
		    union
		    {
			char xtcc[4];
			long x;
		    } x;
		    long dlen;
		} xtcc =  {{"XTcc"}, 0};
		xtcc.dlen = qldata;
		write(1, &xtcc, 8);
	    }
	}
    }
}

void dump_file (long fnum, QLDIR * entry)
{
    long flen;
    int i, ii, s, start, end;
    long qldata = 0;

    short j,k;
    char fname[37];
    FILE *f;

    j = k = swapword (entry->d_szname);
    sprintf (fname,"%-*.*s\0", k, j, entry->d_name);

    f=fopen(fname,"wb");
    if (f==NULL) {
	    fprintf(stderr,"Cannot open file %s for writing\n",fname);
	    exit(1);
    }

    if(entry->d_type == 255)
    {
	fwrite(QLDIRSTRING, 1, 16, f);
    }
    else
    {
	flen = swaplong (entry->d_length);	/* with the header */
	if (entry->d_type == 1)
	{
	    qldata = entry->d_datalen;
	}

	if (flen + swapword (entry->d_szname) == 0)
	{
	    fputs ("warning: file appears to be deleted\n", stderr);
	}
	else
	{
	    char *buffer = xmalloc (512 * allocblock);
	    long lblk = flen / (GSSIZE * allocblock);
	    long xblk = 0,xbyt = 0;
	    short needx = 1;

	    if(qldata)
	    {
		xblk = (flen - 8) / (GSSIZE * allocblock);
		xbyt = (flen - 8) % (GSSIZE * allocblock);
	    }

	    for (s = 0; s <= lblk; s++)
	    {
		if ((i = FindCluster (fnum, s)) != -1)
		{
		    read_cluster (buffer, i);
		    if (s == 0)
			start = 64;
		    else
			start = 0;
		    end = GSSIZE * allocblock;
		    if (s == lblk)
			end = flen % (GSSIZE * allocblock);
		    err = fwrite (buffer + start, 1, end - start, f);
		    if (err < 0)
			perror ("output file: write(): ");
		    if(qldata && s == xblk)
		    {
			needx = memcmp(buffer+start+xbyt, "XTcc", 4);
		    }
		}
		else
		{
		    fprintf (stderr, "** Cluster #%d of %.*s not found **\n",
			     s, entry->d_szname, entry->d_name);
		    err = lseek (1, GSSIZE * allocblock, SEEK_CUR);
		    /* leave hole */
		    if (err < 0)	/* non seekable */
			for (ii = 0; ii < allocblock * GSSIZE; ii++)
			    fputc ('#', f);
		}
	    }
	    free (buffer);

	    if (qldata && needx)
	    {
		static struct
		{
		    union
		    {
			char xtcc[4];
			long x;
		    } x;
		    long dlen;
		} xtcc =  {{"XTcc"}, 0};
		xtcc.dlen = qldata;
		fwrite(&xtcc, 1, 8, f);
	    }
	}
    }
    fclose(f);
}

void UpdateSubEntry (QLDIR * entry, SDL * sdl, short *off)
{

    int s, start, end;
    int i, j;
    int rval = 0;
    long flen = sdl->flen;
    short fnum = sdl->fileno;
    unchar *buffer = xmalloc (GSSIZE * allocblock);

    if (off)
    {
	s = (*off) / (GSSIZE * allocblock);
	j = (*off) % (GSSIZE * allocblock);
	if ((i = FindCluster (fnum, s)) != -1)
	{
	    if (flen != 128 && j != 64)
	    {
		read_cluster (buffer, i);
	    }
	    memcpy ((buffer + j), entry, 64);
	    write_cluster (buffer, i);
	}
    }
    else
    {
	for (s = 0; !rval && s <= flen / (GSSIZE * allocblock); s++)
	{
	    i = FindCluster (fnum, s);
	    if (i != -1)
	    {
		read_cluster (buffer, i);
		if (s == 0)
		    start = 64;
		else
		    start = 0;
		end = GSSIZE * allocblock;

		if (s == (flen / (GSSIZE * allocblock)))
		    end = (flen % (GSSIZE * allocblock));
		else
		    end = GSSIZE * allocblock;
		for (j = start; j <= end; j += 64)
		{
		    QLDIR *ent = (QLDIR *) (buffer + j);

		    if (ent->d_fileno == entry->d_fileno)
		    {
			if (entry->d_szname == 0 && entry->d_length == 0)
			{
			    memset (ent, '\0', 64);
			}
			else
			{
			    memcpy (ent, entry, 64);
			}
			write_cluster (buffer, i);
			rval = 1;
			break;
		    }
		}
	    }
	}
    }
    free (buffer);
}

void RemoveList (SDL * sdl)
{
    SDL *prev = NULL, *sl;

    for (sl = SList; sl; prev = sl, sl = sl->next)
    {
	if (sdl == sl)
	{
	    if (prev == NULL)
	    {
		SList = sl->next;
	    }
	    else
	    {
		prev->next = sl->next;
	    }
	    free (sl);
	    sl = NULL;
	    break;
	}
    }
}


int CountDir (QLDIR * e, int fnum, COUNT_S *s)
{
    long len;

    if (((len = e->d_length) != 0) &&  (e->d_szname != 0))
    {
	s->nfiles++;
    }

    if (s->rflag == 0)
    {
	s->indir++;
	if (len == 0 && s->freed == 0)
	{
	    s->freed = s->indir;
	}
    }
    else if (e->d_type == 255)
    {
	RecurseDir (fnum, swaplong (e->d_length), s,
		    (int (*) (QLDIR *, int, void *)) CountDir);
    }
    return 0;
}

void del_file (long fnum, QLDIR * entry, SDL * sdl)
{
    long int flen, file;
    int i, freed, blk0;
    short rdir = 0;
    COUNT_S nf;

    nf.nfiles = 0;
    nf.rflag = 1;

    if (entry->d_type == 255)
    {
	RecurseDir (fnum, swaplong (entry->d_length), &nf,
		    (int (*) (QLDIR *, int, void *)) CountDir);
	if (nf.nfiles)
	{
	    fprintf (stderr, "Directory must be empty to delete (%d)\n",
		     nf.nfiles);
	    exit (0);
	}
	else
	{
	    rdir = 1;
	}
    }

    freed = 0;
    blk0 = -1;

    flen = swaplong (entry->d_length);
    if (flen == 0)
    {
	fprintf (stderr, "file already deleted?\n");
	exit (1);
    }

    for (i = 1; i < gclusters; i++)
    {
	file = fat_file (i);
	if (file == fnum)
	{
	    if (fat_cl (i) == 0)
		blk0 = i;
	    free_cluster (i);
	    freed++;
	}
    }

    b0->q5a_free = swapword (freed * allocblock + swapword (b0->q5a_free));

    entry->d_szname = 0;
    entry->d_length = 0;
    entry->d_type = 0;

    if (blk0 > 0)
    {
	unchar *b = xmalloc (512 * allocblock);

	read_cluster (b, blk0);
	memcpy (b, entry, 64);
	write_cluster (b, blk0);
	free (b);
    }
    else
	fprintf (stderr, "block 0 of file missing??\n");

    write_b0fat ();		/* write_cluster(b0,0); */
    dir_write_back (entry, sdl, NULL);
    if (rdir)
    {
	RemoveList (sdl);
    }
}

void usage (char *error)
{
    static char *options[] =
    {
    "Usage: qltools dev -[options] [filename]\n",
    "options:",
    "    -q         dump files in qlay(w) format and update qlay.dir",
    "    -d         list directory          -s         list short directory",
    "    -i         list info               -m         list disk map",
    "    -c         list conversion table   -l         list files on write",
    "    -w <files> write files (query)     -W <files> (over)write files",
    "    -r <name>  remove file <name>      -n <file>  copy <file> to stdout",
    "    -uN        ASCII dump cluster N    -UN        binary dump",
    "    -M <name>  Make level 2 directory <name>\n",
    "    -x <name> <size> make <name> executable with dataspace <size>",
    "    -fxx <name> format as xx=hd|dd disk with label <name>\n",
    "  QLTOOLS for " OSNAME " (version " VERSION ")",
    "  dev is either a file with the image of a QL format disk",
    "  or a floppy drive with a SMS/QDOS disk inserted in it (e.g. " FDNAME ")\n",
    "  by Giuseppe Zanetti,Valenti Omar,Richard Zidlicky & Jonathan Hudson",
    NULL};
    char **h;

    fprintf (stderr, "error : %s\n", error);
    for( h = options; *h; h++)
    {
	fputs(*h, stderr);
	fputc('\n', stderr);
    }
    exit (1);
}

void print_info (void)
{
    short i;

    printf ("Disk ID          : %.4s\n", b0->q5a_id);
    printf ("Disk Label       : %.10s\n", b0->q5a_mnam);
    printf ("sectors per track: %i\n", gsectors);
    printf ("sectors per cyl. : %i\n", gspcyl);
    printf ("number of cylind.: %i\n", gtracks);
    printf ("allocation block : %i\n", allocblock);
    printf ("sector offset/cyl: %i\n", goffset);
    printf ("random           : %04x\n", swapword (b0->q5a_rand));
    printf ("Updates          : %ld\n", swaplong (b0->q5a_mupd));
    printf ("free sectors     : %i\n", swapword (b0->q5a_free));
    printf ("good sectors     : %i\n", swapword (b0->q5a_good));
    printf ("total sectors    : %i\n", swapword (b0->q5a_totl));

    printf ("directory is     : %u sectors and %u bytes\n", bleod, byeod);

    printf ("\nlogical-to-physical sector mapping table:\n\n");
    for (i = 0; i < gspcyl; i++)
	printf ("%x ", b0->q5a_lgph[i]);
    putc ('\n', stdout);

    if (ql5a)
    {
	printf ("\nphysical-to-logical sector mapping table:\n\n");
	for (i = 0; i < gspcyl; i++)
	    printf ("%x ", b0->q5a_phlg[i]);
    }
    putc ('\n', stdout);
}

int RecurseDir (int fnum, long flen, void *parm, int (*func) (QLDIR *, int, void *))
{
    int s, start, end;
    int i, j;
    int rval = 0;

    if (flen > 64)
    {
	unchar *buffer = xmalloc (GSSIZE * allocblock);

	for (s = 0; s <= flen / (GSSIZE * allocblock); s++)
	{

	    i = FindCluster (fnum, s);
	    if (i != -1)
	    {
		read_cluster (buffer, i);
		if (s == 0)
		    start = 64;
		else
		    start = 0;
		end = GSSIZE * allocblock;

		if (s == (flen / (GSSIZE * allocblock)))
		    end = (flen % (GSSIZE * allocblock));
		else
		    end = GSSIZE * allocblock;
		for (j = start; j <= end; j += 64)
		{
		    QLDIR *ent = (QLDIR *) (buffer + j);
		    int fno = FileXDir(ent->d_fileno);
		    if ((rval = (func) (ent, fno, parm)) != 0)
		    {
			break;
		    }
		}
	    }
	}
	free (buffer);
    }
    else
    {
	rval = 1;
    }
    return rval;
}

int print_entry (QLDIR * entry, int fnum, void *flag)
{
    short j,k;
    long flen;

    if (entry == NULL)
	return 0;

    flen = swaplong (entry->d_length);

    if (flen + swapword (entry->d_szname) == 0)
	return 0;

    j = k = swapword (entry->d_szname);
    if (*((short *) flag) == 0)
    {
	k = 36;
    }
    printf ("%-*.*s", k, j, entry->d_name);

    if (entry->d_type == 255)
    {
	if (*((short *) flag))
	{
	    putc ('\n', stdout);
	}
	else
	{
	    if (*((short *) flag) == 0)
	    {
		printf ("(dir) %ld\n", flen - 64l);
	    }
	}
	if(*((short *)flag) != 2)
	{
	    RecurseDir (fnum, flen, flag, print_entry);
	}

    }
    else if (*((short *) flag) == 0)
    {
	switch (entry->d_type)
	{
	    case 0:
		fputs (" ", stdout);
		break;
	    case 1:
		fputs ("E", stdout);
		break;
	    case 2:
		fputs ("r", stdout);
		break;
	    default:
		printf ("%3d", entry->d_type);
		break;
	}
	printf (" %7ld", (long) (flen - 64L));
	{
	    struct tm *tm;
	    time_t t = swaplong (entry->d_update) - TIME_DIFF - timeadjust;

	    tm = localtime (&t);
	    printf (" %02d/%02d/%02d %02d:%02d:%02d v%-5u",
		    tm->tm_mday, tm->tm_mon + 1, tm->tm_year,
		    tm->tm_hour, tm->tm_min, tm->tm_sec,
		    swapword (entry->d_version));
	}
	if (entry->d_type == 1 && entry->d_datalen)
	{
	    printf (" (%ld)", swaplong (entry->d_datalen));
	}
	putc ('\n', stdout);
    }
    else
    {
	putc ('\n', stdout);
    }
    return 0;
}

void print_dir (short flag)
{
    int d;
    QLDIR *entry;

    if (flag == 0)
    {
	printf ("%.10s\n", b0->q5a_mnam);
	printf ("%i/%i sectors.\n\n",
		swapword (b0->q5a_free), swapword (b0->q5a_good));
    }

    for (d = 1; d < maxdir (); d++)
    {
	entry = pdir + d;
	if (swaplong (entry->d_length) + swapword (entry->d_szname) != 0)
	{
	    print_entry (entry, d, &flag);
	}
    }
}

void qlay_dir(char *p)
{
int	i;
FILE	*qldf;
int	number,slen,nof;

	if ((qldf=fopen("qlay.dir","ab")) == NULL) {
		fprintf(stderr,"Cannot open %s\n\n","qlay.dir");
		exit(1);
	}

	for(i=0;i<64;i++) fputc(*p++,qldf);
	fclose(qldf);
}

int dump_entry (QLDIR * entry, int fnum, void *flag)
{
    short j,k;
    long flen;

    if (entry == NULL)
	return 0;

    flen = swaplong (entry->d_length);

    if (flen + swapword (entry->d_szname) == 0)
	return 0;

    j = k = swapword (entry->d_szname);
    if (*((short *) flag) == 0)
    {
	k = 36;
    }
    printf ("%-*.*s", k, j, entry->d_name);

    if (entry->d_type == 255)
    {
	if (*((short *) flag))
	{
	    putc ('\n', stdout);
	}
	else
	{
	    if (*((short *) flag) == 0)
	    {
		printf ("(dir) %ld\n", flen - 64l);
	    }
	}
	if(*((short *)flag) != 2)
	{
	    RecurseDir (fnum, flen, flag, dump_entry);
	}

    }
    else if (*((short *) flag) == 0)
    {
	switch (entry->d_type)
	{
	    case 0:
		fputs (" ", stdout);
		break;
	    case 1:
		fputs ("E", stdout);
		break;
	    case 2:
		fputs ("r", stdout);
		break;
	    default:
		printf ("%3d", entry->d_type);
		break;
	}
	printf (" %7ld", (long) (flen - 64L));
	{
	    struct tm *tm;
	    time_t t = swaplong (entry->d_update) - TIME_DIFF - timeadjust;

	    tm = localtime (&t);
	    printf (" %02d/%02d/%02d %02d:%02d:%02d v%-5u",
		    tm->tm_mday, tm->tm_mon + 1, tm->tm_year,
		    tm->tm_hour, tm->tm_min, tm->tm_sec,
		    swapword (entry->d_version));
	}
	if (entry->d_type == 1 && entry->d_datalen)
	{
	    printf (" (%ld)", swaplong (entry->d_datalen));
	}
	putc ('\n', stdout);
	fflush(stdout);
	dump_file(fnum,entry);
	qlay_dir((char*)entry);
    }
    else
    {
	putc ('\n', stdout);
    }
    return 0;
}

void dump_dir (short flag)
{
    int d;
    QLDIR *entry;

    if (flag == 0)
    {
	printf ("%.10s\n", b0->q5a_mnam);
	printf ("%i/%i sectors.\n\n",
		swapword (b0->q5a_free), swapword (b0->q5a_good));
    }

    for (d = 1; d < maxdir (); d++)
    {
	entry = pdir + d;
	if (swaplong (entry->d_length) + swapword (entry->d_szname) != 0)
	{
	    dump_entry (entry, d, &flag);
	}
    }
}

void make_convtable (int verbose)
{
    int i, si, tr, se, uxs, sectors;

    if (verbose)
    {
	printf ("\nCONVERSION TABLE\n\n");
	printf ("logic\ttrack\tside\tsector\tunix_dev\n\n");
    }

    sectors = gclusters * allocblock;

    if (verbose)
    {
	for (i = 0; i < sectors; i++)
	{
	    tr = LTP_TRACK (i);
	    si = LTP_SIDE (i);
	    se = LTP_SCT (i);
	    uxs = tr * gspcyl + gsectors * si + se;
	    if (verbose)
	    {
		printf ("%i\t%i\t%i\t%i\t%i\n", i, tr, si, se, uxs);
	    }
	}
    }
}

void dump_cluster (int num, short flag)
{
    int i, sect;
    unsigned char buf[512];

    for (i = 0; i < allocblock; i++)
    {
	short j, k;
	unsigned char *p;
	long fpos=0;

	sect = num * allocblock + i;
	err = ReadQLSector (fd, buf, sect);

	if (err < 0)
	    perror ("dump block: read(): ");
	if (flag == 0)
	{
	    p = buf;
	    for (k = 0; k < 32; k++)
	    {
		printf ("%03lx : ", k * 16 + fpos);

		for (j = 0; j < 16; j++)
		{
		    printf (" %02x", *p++);
		}
		fputc ('\t', stdout);
		p -= 16;

		for (j = 0; j < 16; j++, p++)
		{
		    printf ("%c", isprint (*p) ? *p : '.');
		}
		fputc ('\n', stdout);
	    }
	}
	else
	{
	    write (1, buf, 512);
	}
    }
}

int read_cluster (void *p, int num)
{
    int i, sect;
    int r = 0;

    for (i = 0; i < allocblock; i++)
    {
	sect = num * allocblock + i;
	r = ReadQLSector (fd, (char *) p + i * GSSIZE, sect);
    }
    return r;
}

void write_cluster (void *p, int num)
{
    int i, sect;

    for (i = 0; i < allocblock; i++)
    {
	sect = num * allocblock + i;
	WriteQLSector (fd, (char *) p + i * GSSIZE, sect);
    }
}

void read_b0fat (int argconv)
{
    static const union
    {
	char c[4];
	long l;
    }
    ql5 = {"QL5"};

    err = ReadQLSector (fd, b0, 0);

    if ((*((long *) b0->q5a_id) & 0xffffff) != ql5.l)
    {
	fprintf (stderr, "\nNot an SMS disk %.4s %lx !!!\n",
		 b0->q5a_id, *(long *) b0->q5a_id);
	exit (0);
    }

    ql5a = b0->q5a_id[3] == 'A';

    gtracks = swapword (b0->q5a_trak);
    gsectors = swapword (b0->q5a_strk);
    gspcyl = swapword (b0->q5a_scyl);
    gsides = gspcyl / gsectors;
    goffset = swapword (b0->q5a_soff);
    bleod = swapword (b0->q5a_eodbl);
    byeod = swapword (b0->q5a_eodby);
    allocblock = swapword (b0->q5a_allc);
    gclusters = gtracks * gspcyl / allocblock;

    make_convtable (argconv);
    read_fat ();

}

void write_b0fat (void)
{
    int i;

    b0->q5a_mupd = swaplong (swaplong (b0->q5a_mupd) + 1);

    for (i = 0; fat_file (i) == 0xf80; i++)
    {
	write_cluster ((unchar *) b0 + i * allocblock * GSSIZE, i);
    }
}


void read_fat (void)
{
    int i;

    for (i = 0; fat_file (i) == 0xf80; i++)
	read_cluster ((unchar *) b0 + i * allocblock * GSSIZE, i);
}

short CheckFileName (QLDIR * ent, char *fname)
{
    short match = 0;
    char c0, c1;
    int i, len;

    len = strlen (fname);
    if (swapword (ent->d_szname) == len)
    {
	match = 1;
	for (i = 0; i < len; i++)
	{
	    c0 = *(ent->d_name + i);
	    c0 = tolower (c0);
	    c1 = tolower (fname[i]);

	    if (c0 != c1)
	    {
		if (!tranql || !(c1 == '.' && c0 == '_'))
		{
		    match = 0;
		    break;
		}
	    }
	}
    }
    return match;
}
#ifndef VMS
# pragma argsused
#endif

/* ARGSUSED */
int FindName (QLDIR * e, int fileno, void *llist)
{
    int res;
    char **mlist = (char **) llist;

    if ((res = CheckFileName (e, mlist[0])) != 0)
    {
	memcpy ((QLDIR *) mlist[1], e, sizeof (QLDIR));
    }
    return res;
}

long int match_file (char *fname, QLDIR ** ent, SDL ** sdl)
{
    static QLDIR sd;
    int d, match = 0;
    long r = 0L;

    if (sdl)
    {
	*sdl = NULL;
    }
    for (d = 1; d < maxdir (); d++)
    {
	if ((match = CheckFileName (pdir + d, fname)) != 0)
	{
	    if (ent)
	    {
		*ent = pdir + d;
	    }
	    r = d;
	    break;
	}
    }

    if (match == 0)
    {
	SDL *sl;

	memset (&sd, 0, 64);
	for (sl = SList; sl; sl = sl->next)
	{
	    if (sl->flen > 64 && strnicmp (fname, sl->name, sl->szname) == 0
		&& strlen(fname) != sl->szname)
	    {
		void *llist[3];

		llist[0] = fname;
		llist[1] = &sd;
		llist[2] = NULL;

		if ((match = RecurseDir (sl->fileno, sl->flen, llist,
					 FindName)) != 0)
		{
		    r = FileXDir (sd.d_fileno);
		    if (ent)
		    {
			*ent = &sd;
		    }
		    if (sdl)
		    {
			*sdl = sl;
		    }
		    break;
		}
	    }
	}
    }
    return (r);
}

char *MakeQLName (char *fn, short *n)
{
    char *p, *q;

    if ((p = strrchr (fn, '/')) != NULL)
    {
	p++;
    }
    else if ((p = strrchr (fn, '\\')) != NULL)
    {
	p++;
    }

    if (p == NULL)
	p = fn;
    q = strdup (p);

    *n = min (strlen (q), 36);
    *(q + (*n)) = 0;

    if (tranql)
    {
	for (p = q; *p; p++)
	{
	    if (*p == '.')
		*p = '_';
	}
    }

    return q;
}

SDL *CheckDirLevel (char *qlnam, short n)
{
    SDL *sl;

    for (sl = SList; sl; sl = sl->next)
    {
	if (n > sl->szname + 1)
	{
	    if (strnicmp (qlnam, sl->name, sl->szname) == 0)
	    {
		if (*(qlnam + sl->szname) == '_')
		{
		    break;
		}
	    }
	}
    }
    return sl;
}

int AllocNewSubDirCluster (long flen, ushort fileno)
{
    short i;
    short seqno;
    unchar *p;

    seqno = flen / (GSSIZE * allocblock);

    if ((i = alloc_new_cluster (fileno, seqno, 0)) != -1)
    {
	p = xmalloc (GSSIZE * allocblock);
	write_cluster (p, i);
    }
    return i;
}

QLDIR *GetNewDirEntry (SDL * sdl, int *filenew, int *nblock, short *diroff)
{
    int i;
    int hole;
    int offset;
    QLDIR *ent;

    if (sdl == NULL)
    {
	offset = 1;
	while ((swaplong ((pdir + offset)->d_length) +
		swapword ((pdir + offset)->d_szname) > 0)
	       && (offset < maxdir ()))
	{
	    offset++;
	}

	if (offset >= maxdir ())
	{
	    hole = 0;
	    offset = maxdir ();
	}
	else
	{
	    hole = 1;
	}

	if ((byeod == 0) && ((bleod % allocblock) == 0) && !hole)
	{
	    i = alloc_new_cluster (0, block_max, 0);
	    if (i < 0)
	    {
		fprintf (stderr, "write file: no free cluster\n");
		exit (1);
	    }
	    *nblock = *nblock + 1;
	    block_dir[block_max] = i;
	    block_max++;
	}

	if (!hole)
	    byeod += 64;

	if (byeod == GSSIZE)
	{
	    byeod = 0;
	    bleod += 1;
	}

	*filenew = offset;
	ent = pdir + offset;
    }
    else
    {
	static QLDIR newent;

	if ((sdl->flen % GSSIZE * allocblock) == 0)
	{
	    if (AllocNewSubDirCluster (sdl->flen, sdl->fileno))
	    {
		*nblock = *nblock + 1;
		*diroff = sdl->flen;
		sdl->flen += 64;
	    }
	}
	else
	{
	    COUNT_S nf;

	    nf.nfiles = nf.rflag = nf.freed = nf.indir = 0;
	    RecurseDir (sdl->fileno, sdl->flen, &nf,
		    (int (*) (QLDIR *, int, void *))CountDir);

	    if ((sdl->flen - 64) == 64 * nf.nfiles)
	    {
		*diroff = sdl->flen;
		sdl->flen += 64;
	    }
	    else
	    {
		*diroff = 64 * nf.freed;
	    }
	}
	i = find_free_cluster ();
	ent = &newent;
	*filenew = (i + 0x800);
    }
    return ent;
}

SDL * AddSlist(QLDIR *entry, int fileno)
{
    SDL *sdl;
    sdl = xmalloc (sizeof (SDL));
    sdl->flen = swaplong (entry->d_length);
    sdl->fileno = fileno;
    sdl->szname = swapword (entry->d_szname);
    memcpy (sdl->name, entry->d_name, sdl->szname);
    sdl->next = SList;
    SList = sdl;

    return sdl;

}

int ProcessSubFile(QLDIR *entry, int fileno, FSBLK *fs)
{

    short n = swapword(fs->nde->d_szname);
    short m = swapword(entry->d_szname);

    if((m > n + 1) &&
                strnicmp(entry->d_name, fs->nde->d_name, n) == 0 &&
                *(entry->d_name + n) == '_')
    {
	    ushort dcl, fcl;
	    QLDIR nent;
	    SDL *nsdl;
	    short i;
	    unchar *buf;
	    long cwdlen;

	    fcl = FindCluster(fileno, 0);
	    nent = *entry;

	    /* If its a root entry, this removes it */

	    entry->d_length = 0;
	    entry->d_szname = 0;

	    if((nsdl = CheckDirLevel(entry->d_name, m)) != NULL)
	    {
		/* If a sub-dir, this does */
		UpdateSubEntry(entry, nsdl, 0);
	    }

	    for (i = 0; i < gclusters; i++)
	    {
		if(fat_file (i) == fileno)
		{
		    set_fat_file(i, fcl+0x800);
		}
	    }
	    nent.d_fileno = swapword(fcl+0x801);
	    /*
	     * fs->fnew is the file no (start cluster (+0x800))
	     * of directory
             */

	    /* Now write nent to new-ish directory */

	    buf = xmalloc(GSSIZE * allocblock);
	    cwdlen = swaplong(fs->nde->d_length);
	    dcl = fs->fnew;
	    if((cwdlen % GSSIZE * allocblock) == 0)
	    {
		dcl = alloc_new_cluster(dcl,
					cwdlen / (GSSIZE * allocblock), 0);
		*(fs->nptr) = *(fs->nptr) + 1;
	    }
	    else if(cwdlen > 64)
	    {
		read_cluster(buf, dcl);
	    }
	    memcpy(buf+cwdlen, &nent, 64);
	    write_cluster(buf, dcl);
	    fs->nde->d_length = swaplong(cwdlen + 64);

    }
    else if(entry->d_type == 255)
    {
        RecurseDir(fileno, swaplong(entry->d_length), fs,
		   (int (*) (QLDIR *, int, void *))ProcessSubFile);
    }

    return 0;
}

int MoveSubFiles(FSBLK *fs)
{
    QLDIR *entry;
    short d;

    for (d = 1; d < maxdir (); d++)
    {
	entry = pdir + d;
	if (swaplong (entry->d_length) + swapword (entry->d_szname) != 0)
	{
	    ProcessSubFile (entry, d, fs);
	}
    }
    return 0;
}

void writefile (char *fn, short dflag)
{
    unchar *datbuf;
    int filenew, i, fl;
    QLDIR *entry;
    unsigned long free_sect;
    long y, qdsize = 0;
    int enddat;
    int block = 0, nblock = 0;
    char *qlnam;
    short nlen;
    QLDIR *vers = NULL;
    SDL *sdl = NULL;
    short flag = 0;
    short diroff = 0;
    short nvers = -1;
    time_t t;
    struct stat s;
    short blksiz = GSSIZE * allocblock;

    qlnam = MakeQLName (fn, &nlen);

    if(dflag != 255)
    {
	if(stat(fn, &s) == 0)
	{
	    if(s.st_size == 16)
	    {
		int fd;
		char tbuf[16];

		if((fd = open(fn, O_RDONLY|O_BINARY)) > -1)
		{
		    read(fd, tbuf, 16);
		    if(memcmp(tbuf, QLDIRSTRING,16) == 0)
		    {
			dflag = 255;
		    }
		    close (fd);
		}
	    }
	}
	else
	{
	    fprintf(stderr, "Can't stat file %s\n", fn);
	    exit(0);
	}
    }


    if (list)
    {
	fputs (fn, stderr);
    }

    if ((fl = match_file (qlnam, &vers, &sdl)) != 0)
    {
	if (dflag == 255)
	{
	    fputs ("Exists\n", stderr);
	    exit (0);
	}
	else
	{
	    if (OWopt != 'A')
	    {
		fprintf (stderr, "file %s exists, overwrite [Y/N/A/Q] : ", qlnam);
		do
		{
		    OWopt = getch ();
		    OWopt = toupper (OWopt);
		}
		while (strchr ("\003ANYQ", OWopt) == NULL);
		fprintf (stderr, "%c\n", OWopt);
		if (OWopt == 'N')
		{
		    return;
		}
		else
		{
		    if (OWopt == 'Q' || OWopt == 3)
		    {
			exit (2);
		    }
		}
	    }
	    nvers = swapword (vers->d_version);
	    del_file (fl, vers, sdl);
	}
    }

    if (dflag == 255)
    {
	y = 64;
    }
    else
    {

	if ((fl = open (fn, O_RDONLY | O_BINARY)) == -1)
	{
	    perror ("write file: could not open input file ");
	    exit (1);
	}
	y = s.st_size + 64;
	{
	    long stuff[2];
	    lseek (fl, -8, SEEK_END);
	    read (fl, stuff, 8);
	    if (*stuff == *(long *) "XTcc")
	    {
		qdsize = *(stuff + 1);
	    }
	}

	err = lseek (fl, 0, SEEK_SET);	/* reposition to zero!!! */
	if (err < 0)
	    perror ("write file: lseek() on input file : ");
    }

    free_sect = swapword (b0->q5a_free);
    if (y > free_sect * GSSIZE)
    {
	fprintf (stderr, " file %s too large (%ld %ld)\n", fn, y, free_sect);
	exit (1);
    }

    if (sdl == NULL)
    {
	sdl = CheckDirLevel (qlnam, strlen(qlnam));
    }

    entry = GetNewDirEntry (sdl, &filenew, &nblock, &diroff);
    memset (entry, '\0', 64);

    if (filenew & 0x800)
    {
	flag = filenew - 0x800;
    }
    else
    {
	flag = 0;
    }

    entry->d_length = swaplong (y);
    entry->d_fileno = swapword (filenew + 1);

    if (dflag != 255)
    {
	if (qdsize == 0)
	{
	    entry->d_type = 0;
	    entry->d_datalen = 0;
	}
	else
	{
	    entry->d_type = 1;
	    entry->d_datalen = qdsize;	/* big endian already */
	}
    }

    memcpy (entry->d_name, qlnam, nlen);
    entry->d_szname = swapword (nlen);

    t = time (NULL) + TIME_DIFF + timeadjust;

    entry->d_update = entry->d_backup = swaplong (t);
    nvers++;
    entry->d_version = swapword (nvers);

    if (dflag == 255)
    {
	filenew = alloc_new_cluster (filenew, 0, flag);
	nblock++;
    }
    else
    {
	int nwr = 0;

	datbuf = xmalloc(blksiz);
	enddat = (y < blksiz) ? y : blksiz;
	err = read (fl, datbuf + 64, enddat - 64);

	memcpy (datbuf, entry, 64);
	for (block = 0; y > 0;)
	{
	    i = alloc_new_cluster (filenew, block, flag);
	    flag = 0;

	    if (i < 0)
	    {
		fprintf (stderr, " filewrite failed : no free cluster\n");
		exit (1);
	    }
	    block++;
	    nblock++;

	    if(list)
	    {
		nwr += err;
		fprintf(stderr,"%8d\b\b\b\b\b\b\b\b", nwr); fflush(stderr);
	    }

	    write_cluster (datbuf, i);

	    y -= blksiz;

	    err = read (fl, datbuf, blksiz);
	    if (err < 0)
		perror ("write file: read on input file:");
	}
	close (fl);
	free(datbuf);
    }

    free (qlnam);

    b0->q5a_eodbl = swapword (bleod);
    b0->q5a_eodby = swapword (byeod);

    if (dflag == 255)
    {
	FSBLK fs;
	fs.nde = entry;
	fs.nptr = &nblock;
	fs.fnew = filenew;
	MoveSubFiles (&fs);
	entry->d_type = 255;
    }

    b0->q5a_free = swapword (free_sect - nblock * allocblock);

    write_b0fat ();
    dir_write_back (entry, sdl, &diroff);

    if (sdl)
    {
	SDL *msdl = NULL;
	QLDIR *mentry;
	char dirname[40];

	memcpy (dirname, sdl->name, sdl->szname);
	*(dirname + sdl->szname) = '\0';
	if ((fl = match_file (dirname, &mentry, &msdl)) != 0)
	{
	    mentry->d_length = swaplong (sdl->flen);
	    mentry->d_update = entry->d_update;
	    dir_write_back (mentry, msdl, NULL);
	}
    }

    if(list)
	fputc ('\n', stderr);

    if(dflag == 255)
    {
	AddSlist(entry, FileXDir(entry->d_fileno));
    }
}

#ifndef VMS
#pragma argsused
#endif
/* ARGSUSED */

int AddDirEntry (QLDIR * entry, int fileno, void *flag)
{
    if (entry->d_type == 255)
    {
	SDL *sdl = AddSlist(entry, fileno);
	RecurseDir (fileno, sdl->flen, NULL, AddDirEntry);
    }
    return 0;
}

void BuildSubList (void)
{
    short d;
    QLDIR *entry;
    long flen;

    for (d = 1; d < maxdir (); d++)
    {
	entry = pdir + d;
	flen = swaplong (entry->d_length);

	if (flen + swapword (entry->d_szname) != 0)
	{
	    if (entry->d_type == 255)
	    {
		AddDirEntry (entry, d, NULL);
	    }
	}
    }
}

void read_dir (void)
{
    int i, fn, cl;

    for (i = 0; i < gclusters; i++)
    {
	cl = fat_cl (i);
	fn = fat_file (i);

	if (fn == 0)
	{
	    block_dir[block_max] = i;
	    block_max++;
	    read_cluster ((char *) pdir + GSSIZE * allocblock * cl, i);
	}
    }
    BuildSubList ();
}

void print_map (void)
{
    int i, fn, cl;
    QLDIR *entry;
    short flag;

    printf ("\nblock\tfile\tpos\n\n");

    for (i = 0; i < gclusters; i++)
    {
	cl = fat_cl (i);
	fn = fat_file (i);

	printf ("%d\t%d\t%d\t(%03x, %03x) ", i, fn, cl, fn, cl);

	if ((fn & 0xFF0) == 0xFD0 && (fn & 0xf) != 0xF)
	{
	    printf ("erased %2d\t", fn & 0xF);
	}

	entry = NULL;
	switch (fn)
	{
	    case 0x000:
		printf ("directory");
		break;
	    case 0xF80:
		printf ("map");
		break;
	    case 0xFDF:
		printf ("unused");
		break;
	    case 0xFEF:
		printf ("bad");
		break;
	    case 0xFFF:
		printf ("not existent");
		break;
	    default:
		entry = GetEntry (fn);
		break;
	}
	if (entry)
	{
	    flag = 2;
	    print_entry (entry, fn, &flag);
	}
	else
	{
	    putc ('\n', stdout);
	}
    }
}

void set_header (int ni, long h, QLDIR * entry, SDL * sdl)
{
    int i;
    unchar *b = xmalloc (allocblock * 512);

    if (swaplong (entry->d_length) + swapword (entry->d_szname) == 0)
    {
	fprintf (stderr, "file deleted ??\n");
	exit (1);
    }

    i = FindCluster (ni, 0);

    read_cluster (b, i);
    entry->d_type = ((QLDIR *) b)->d_type = 1;
    entry->d_datalen = ((QLDIR *) b)->d_datalen = swaplong (h);
    write_cluster (b, i);
    free (b);
    dir_write_back (entry, sdl, NULL);
}

void dir_write_back (QLDIR * entry, SDL * sdl, short *off)
{

    if (sdl)
    {
	UpdateSubEntry (entry, sdl, off);
    }
    else
    {
	int i;

	for (i = 0; i < block_max; i++)
	    write_cluster (pdir + DIRSBLK * allocblock * i, block_dir[i]);
    }
}

int find_free_cluster (void)
{
    short fflag, i;

    for (i = lac + 1; i < gclusters; i++)
    {
	fflag = fat_file (i);
	if ((fflag >> 4) == 0xFD)
	{
	    break;
	}
    }
    return (i < gclusters ? i : -1);
}

int alloc_new_cluster (int fnum, int iblk, short flag)
{
    short i;

    if (flag)
    {
	i = flag;
    }
    else
    {
	i = find_free_cluster ();
    }

    if (i != -1)
    {
	set_fat_file (i, fnum);
	set_fat_cl (i, iblk);
	lac = i;
    }
    return i;
}


void free_cluster (long i)
{
    int fn, dfn;

    if (i > 0)
    {
	fn = fat_file (i);
	dfn = 0xFD0 | (0xf & fn);
	set_fat_file (i, dfn);
    }
    else
    {
	fprintf (stderr, "freeing cluster 0 ???!!!\n");
	exit (1);
    }
}

void format (char *frmt, char *argfname)
{
    static char ltp_dd[] =
    {
	0, 3, 6, 0x80, 0x83, 0x86, 1, 4,
	7, 0x81, 0x84, 0x87, 2, 5, 8, 0x82, 0x85, 0x88
    };

    static char ptl_dd[] =
    {
	0, 6, 0x0c, 1, 7, 0x0d,
	2, 8, 0x0e, 3, 9, 0x0f, 4, 0x0a, 0x10, 5, 0x0b, 0x11
    };

    static char ltp_hd[] =
    {
	0, 2, 4, 6, 8, 0xa, 0xc, 0xe, 0x10,
	0x80, 0x82, 0x84, 0x86, 0x88, 0x8a, 0x8c, 0x8e, 0x90,
	1, 3, 5, 7, 9, 0xb, 0xd, 0xf, 0x11,
	0x81, 0x83, 0x85, 0x87, 0x89, 0x8b, 0x8d, 0x8f, 0x91
    };

    int cls;
    time_t t;

    t = time (NULL);
    b0->q5a_rand = swapword (t & 0xffff);
    if(argfname == NULL)
    {
	argfname = "";
    }

    ZeroSomeSectors(fd, *frmt);

    if (*frmt == 'd')		/* 720 K format */
    {
	ql5a = 1;
	memcpy (b0, "QL5A          ", 14);
	memcpy (b0->q5a_mnam, argfname, (strlen (argfname) <= 10 ?
					 strlen (argfname) : 10));
	memcpy (b0->q5a_lgph, ltp_dd, 18);
	memcpy (b0->q5a_phlg, ptl_dd, 18);

	gsides = 2;
	b0->q5a_trak = swapword (80);
	gtracks = 80;
	b0->q5a_strk = swapword (9);
	gsectors = 9;
	b0->q5a_scyl = swapword (18);
	gspcyl = 18;
	goffset = 5;
	b0->q5a_soff = swapword (5);
	b0->q5a_eodbl = 0;
	b0->q5a_eodby = swapword (64);
	b0->q5a_free = swapword (1434);
	b0->q5a_good = b0->q5a_totl = swapword (1440);
	b0->q5a_allc = swapword (3);
	allocblock = 3;

	set_fat_file (0, 0xF80);	/* FAT entry for FAT */
	set_fat_cl (0, 0);
	set_fat_file (1, 0);	/*  ...  for directory */
	set_fat_cl (1, 0);
	gclusters = gtracks * gspcyl / allocblock;
	for (cls = 2; cls < gclusters; cls++)
	{			/* init rest of FAT */
	    set_fat_file (cls, 0xFDF);
	    set_fat_cl (cls, 0xFFF);
	}
    }
    else if (*frmt == 'h')
    {
	memcpy (b0, "QL5B          ", 14);
	ql5a = 0;

	memcpy (b0->q5a_mnam, argfname,
		(strlen (argfname) <= 10 ? strlen (argfname) : 10));
	memcpy (b0->q5a_lgph, ltp_hd, 36);

	gsides = 2;
	b0->q5a_trak = swapword (80);
	gtracks = 80;
	b0->q5a_strk = swapword (18);
	gsectors = 18;
	b0->q5a_scyl = swapword (36);
	gspcyl = 36;
	goffset = 2;
	b0->q5a_soff = swapword (2);
	b0->q5a_eodbl = 0;
	b0->q5a_eodby = swapword (64);
	b0->q5a_free = swapword (2871);
	b0->q5a_good = b0->q5a_totl = swapword (2880);
	b0->q5a_allc = swapword (3);
	allocblock = 3;

	set_fat_file (0, 0xF80);	/* FAT entry for FAT */
	set_fat_cl (0, 0);
	set_fat_file (1, 0xf80);
	set_fat_cl (1, 1);
	set_fat_file (2, 0);	/*  ...  for directory */
	set_fat_cl (2, 0);

	gclusters = gtracks * gspcyl / allocblock;
	for (cls = 3; cls < gclusters; cls++)
	{			/* init rest of FAT */
	    set_fat_file (cls, 0xFDF);
	    set_fat_cl (cls, 0xFFF);
	}
    }
    make_convtable (0);
    write_b0fat ();
}

int main (int ac, char **av)
{
    int i;
    QLDIR *entry;
    SDL *sdl;
    int mode = O_RDONLY;
    int dofmt = 0;
    long np1 = 0, np2 = 0;

    if (ac < 2)
    {
	usage ("too few parameters");
    }

    for (i = 2; i < ac; i++)
    {
	if ((av[i][0] == '-')
#ifdef DOS_LIKE
	    || (av[i][0] == '/')
#endif
	    )
	{
	    switch (av[i][1])
	    {
		case 'f':
		    dofmt = i;
		case 'x':
		case 'r':
		case 'w':
		case 'W':
		case 'M':
		    mode = O_RDWR;
		    i = ac;
		    break;
	    }
	}
    }

    fd = OpenQLDevice (av[1], mode);

    if ((int) fd < 0)
    {
	perror ("could not open image");
	usage ("image file not opened");
    }

    timeadjust = GetTimeZone ();

    if ((b0 = xmalloc (GSSIZE * MAXALB)) != NULL)
    {
	if (dofmt)
	{
	    format (av[dofmt] + 2, av[dofmt + 1]);
	    CloseQLDevice (fd);
	    exit (0);
	}

	read_b0fat (0);
	pdir = xmalloc (GSSIZE * allocblock * (bleod + 6));
	read_dir ();

	for (i = 2; i < ac; i++)
	{
	    char c;

	    if (av[i][0] == '-')
	    {
		OWopt = 0;
		switch (c = av[i][1])
		{
		    case 'l':
			list = 1;
			break;
		    case 't':
			tranql ^= 1;
			break;
		    case 'U':
		    case 'u':
			if (av[i][2])
			{
			    np1 = atol (av[i] + 2);
			}
			else
			{
			    i++;
			    np1 = atol (av[i]);
			}
			dump_cluster (np1, c == 'U');
			break;
		    case 'd':
			print_dir (0);
			break;
		    case 'q':
			dump_dir (0);
			break;
		    case 's':
			print_dir (1);
			break;
		    case 'i':
			print_info ();
			break;
		    case 'm':
			print_map ();
			break;
		    case 'c':
			make_convtable (1);
			break;
		    case 'n':
			i++;
			np1 = match_file (av[i], &entry, NULL);
			if (np1)
			{
			    cat_file (np1, entry);
			}
			else
			{
			    fprintf (stderr, "Invalid file\n");
			}
			break;
		    case 'W':
			OWopt = 'A';
		    case 'w':
			while (av[i + 1] && *av[i + 1] != '-')
			{
			    i++;
			    writefile (av[i], 0);
			}
			break;
		    case 'r':
			i++;
			np1 = match_file (av[i], &entry, &sdl);
			if (np1)
			{
			    del_file (np1, entry, sdl);
			}
			break;
		    case 'M':
			i++;
			writefile (av[i], 255);
			break;

		    case 'x':
			i++;
			np1 = match_file (av[i], &entry, &sdl);
			if (np1)
			{
			    i++;
			    np2 = strtol (av[i], NULL, 0);
			    if (np2)
			    {
				set_header (np1, np2, entry, sdl);
			    }
			}
			break;
		    default:
			usage ("bad option");
			break;
		}
	    }
	    else
	    {
		np1 = match_file (av[i], &entry, NULL);
		if (np1)
		{
		    cat_file (np1, entry);
		}
	    }
	}
	CloseQLDevice (fd);
    }
    return (0);
/* This is to repvent compiler warning !! */
/* NOTREACHED */
    (void)rcsid;
}
