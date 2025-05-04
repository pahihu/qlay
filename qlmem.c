/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	QL memory
*/

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "qlmem.h"
#include "roms.h"
#include "spc-os.h"
#include "qlio.h"
#include "exe68k.h"

/* internal*/
static	U32	ramtop=0;
static	U32	ramtop_p2;
static	U16	*rammemory;

#if MEMCNT == 1
static	U32 memcntr[40];
static	U32 niw,nil;
U32 instrcnt;
#define	MCT(m)	memcntr[(m)]++
#define MCTI	{int m;for(m=0;m<40;m++)memcntr[m]=0; nil=niw=0;}
#define	MCTD	\
{ \
	static U32 pc=0, pi=0; \
	FILE *f=fopen("mem.cnt","a");int m,t;U32 s=0; \
	fpr("MCTD "); \
	for(t=0;t<5;t++) { \
		fprintf(f,"\n%d\t",t); \
		for(m=0;m<8;m++){fprintf(f,"%d\t",memcntr[8*t+m]);s+=memcntr[8*t+m];} \
	} \
	fprintf(f,"\ns: %d\ti: %d\tc: %d\t",s,instrcnt-pi,cycle-pc); \
	fprintf(f,"niw: %d\tnil: %d\n",niw,nil); \
	pc=cycle; pi=instrcnt; \
	fclose(f); \
}
#else
#define MCT(m) ;
#define MCTI ;
#define MCTD ;
#endif

int addresserror;
addrbank membanks[65536];

/* A dummy bank that contains 'ff' */

static U32 dummy_lget(A32) REGPARAM;
static U16 dummy_wget(A32) REGPARAM;
static U8 dummy_bget(A32) REGPARAM;
static void  dummy_lput(A32, U32) REGPARAM;
static void  dummy_wput(A32, U16) REGPARAM;
static void  dummy_bput(A32, U8) REGPARAM;
static int   dummy_check(A32 addr, U32 size) REGPARAM;
static U16 *dummy_xlate(A32 addr) REGPARAM;

U32 dummy_lget(A32 addr)
{
MCT(0);
if(0)if(addr>0xfffff)fpr("DLG %x\n",addr);
    return 0xffffffff;
}

U16 dummy_wget(A32 addr)
{
MCT(1);
if(0)if(addr>0xfffff)fpr("DWG %x\n",addr);
    return 0xffff;
}

U8 dummy_bget(A32 addr)
{
MCT(2);
if(0)if(addr>0xfffff)fpr("DBG %x\n",addr);
    return 0xff;
}

void dummy_lput(A32 addr, U32 l)
{
MCT(4);
if(0)if(addr>0xfffff)fpr("DLP %x\n",addr);
}

void dummy_wput(A32 addr, U16 w)
{
MCT(5);
if(0)if(addr>0xfffff)fpr("DWP %x\n",addr);
}

void dummy_bput(A32 addr, U8 b)
{
MCT(6);
if(0)if(addr>0xfffff)fpr("DBP %x\n",addr);
}

int dummy_check(A32 addr, U32 size)
{
MCT(3);
    return 0;
}

U16 *dummy_xlate(A32 addr)
{
MCT(7);
	fpr("DXL %x, %x\n",addr,m68k_getpc());	// after this we'll have a sigsegv
	exit(1);
	return NULL;
}

#if DO_TRACE
#define TIAC(t,a,d) store_trace(t,a,d);
#else
#define TIAC(t,a,d) ;
#endif

/* RAM memory */

/*082f malloc's */
/*U16 rammemory[rammem_mask/2];*/

static U32 rammem_lget(A32) REGPARAM;
static U16 rammem_wget(A32) REGPARAM;
static U8 rammem_bget(A32) REGPARAM;
static void  rammem_lput(A32, U32) REGPARAM;
static void  rammem_wput(A32, U16) REGPARAM;
static void  rammem_bput(A32, U8) REGPARAM;
static int   rammem_check(A32 addr, U32 size) REGPARAM;
static U16 *rammem_xlate(A32 addr) REGPARAM;

U32 rammem_lget(A32 addr)
{
MCT(8);
if(0)fpr("ALG%x ",addr);

#if DO_TRACE
	if ( (addr&(rammem_mask-1)) != addr )
		fpr("AM %x %x %x %ld ",addr,rammem_mask,m68k_getpc(),cycle);
	addr -= rammem_start & (rammem_mask-1); addr &= rammem_mask-1;
	{ U32 t = *(U32*) ((U8*)(rammemory) + addr);
	  U32 r = ((t>>16)&0xffff) | ((t<<16)&0xffff0000);
	  store_trace(8,addr,r);
	  return r;
	}
#else
	mem_lget(rammem_start,rammem_mask,rammemory,addr);
#endif
}

U16 rammem_wget(A32 addr)
{
MCT(9);
if(0)fpr("AWG%x ",addr);

#if DO_TRACE
	if ( (addr&(rammem_mask-1)) != addr )
		fpr("AM %x %x %x %ld ",addr,rammem_mask,m68k_getpc(),cycle);
	addr -= rammem_start & (rammem_mask-1); addr &= rammem_mask-1;
	{ U16 *p = (U16 *) ((U8 *) (rammemory) + addr);
	  store_trace(9,addr,*p);
	  return *p;
	}
#else
	mem_wget(rammem_start,rammem_mask,rammemory,addr);
#endif
}

U8 rammem_bget(A32 addr)
{
MCT(10);
if(0)fpr("ABG%x ",addr);

#if DO_TRACE
	if ( (addr&(rammem_mask-1)) != addr )
		fpr("AM %x %x %x %ld ",addr,rammem_mask,m68k_getpc(),cycle);
	addr -= rammem_start & (rammem_mask-1); addr &= rammem_mask-1;
	{ U8 *p = (U8 *) (rammemory) + (addr^1);
	  store_trace(10,addr,*p);
	  return *p;
	}
#else
	mem_bget(rammem_start,rammem_mask,rammemory,addr);
#endif
}

void rammem_lput(A32 addr, U32 l)
{
MCT(12);

#if DO_TRACE
	if ( (addr&(rammem_mask-1)) != addr )
		fpr("AM %x %x %x %ld ",addr,rammem_mask,m68k_getpc(),cycle);
	store_trace(4,addr,l);
#endif
	mem_lput(rammem_start,rammem_mask,rammemory,addr,l);
}

void rammem_wput(A32 addr, U16 w)
{
MCT(13);

#if DO_TRACE
	if ( (addr&(rammem_mask-1)) != addr )
		fpr("AM %x %x %x %ld ",addr,rammem_mask,m68k_getpc(),cycle);
	store_trace(5,addr,(U32)w);
#endif
	mem_wput(rammem_start,rammem_mask,rammemory,addr,w);
}

void rammem_bput(A32 addr, U8 b)
{
MCT(14);

#if DO_TRACE
	if ( (addr&(rammem_mask-1)) != addr )
		fpr("AM %x %x %x %ld ",addr,rammem_mask,m68k_getpc(),cycle);
	store_trace(6,addr,(U32)b);
#endif
	mem_bput(rammem_start,rammem_mask,rammemory,addr,b);
}

int rammem_check(A32 addr, U32 size)
{
MCT(11);
    addr -= rammem_start & (rammem_mask-1);
    addr &= rammem_mask-1;
    return (addr + size) < rammem_mask;
}

U16 *rammem_xlate(A32 addr)
{
MCT(15);
    addr -= rammem_start & (rammem_mask-1);
    addr &= rammem_mask-1;
    return rammemory + (addr >> 1);
}

/* Screen memory */

/*static U16 scrmemory[scrmem_mask/2];*/

static U32 scrmem_lget(A32) REGPARAM;
static U16 scrmem_wget(A32) REGPARAM;
static U8 scrmem_bget(A32) REGPARAM;
static void  scrmem_lput(A32, U32) REGPARAM;
static void  scrmem_wput(A32, U16) REGPARAM;
static void  scrmem_bput(A32, U8) REGPARAM;
static int  scrmem_check(A32 addr, U32 size) REGPARAM;
static U16 *scrmem_xlate(A32 addr) REGPARAM;

U32 scrmem_lget(A32 addr)
{
MCT(16);

	mem_lget(rammem_start,rammem_mask,rammemory,addr);
}

U16 scrmem_wget(A32 addr)
{
MCT(17);

	mem_wget(rammem_start,rammem_mask,rammemory,addr);
}

U8 scrmem_bget(A32 addr)
{
MCT(18);

	mem_bget(rammem_start,rammem_mask,rammemory,addr);
}

void scrmem_lput(A32 addr, U32 l)
{
MCT(20);

	mem_lput(rammem_start,rammem_mask,rammemory,addr,l);
	do_scrn(addr,l>>16);
	do_scrn(addr+2,l&0xffff);

/*
    addr -= scrmem_start & (scrmem_mask-1);
    addr &= scrmem_mask-1;
if(0)fpr("%06x ",addr);
    scrmemory[addr >> 1] = l >> 16;
    scrmemory[(addr >> 1)+1] = (U16)l;
*/
}

void scrmem_wput(A32 addr, U16 w)
{
MCT(21);
	mem_wput(rammem_start,rammem_mask,rammemory,addr,w);
    	do_scrn(addr,w);
/*
    addr -= scrmem_start & (scrmem_mask-1);
    addr &= scrmem_mask-1;
if(0)fpr("%06x ",addr);
    scrmemory[addr >> 1] = w;
*/
}

void scrmem_bput(A32 addr, U8 b)
{
MCT(22);
	mem_bput(rammem_start,rammem_mask,rammemory,addr,b);
	addr&=0xfffffffe;
	{ U16 p = * (U16 *) ((U8 *) (rammemory) + addr);
	do_scrn(addr+rammem_start,p);
	}

/*
    	do_scrn((addr&0xfffffffe)+rammem_start,rammemory[addr >> 1]);
*/
}

int scrmem_check(A32 addr, U32 size)
{
MCT(19);
    addr -= scrmem_start & (scrmem_mask-1);
    addr &= scrmem_mask-1;
    return (addr + size) < scrmem_mask;
}

U16 *scrmem_xlate(A32 addr)
{
MCT(23);
    addr -= rammem_start & (rammem_mask-1);
    addr &= rammem_mask-1;
    return rammemory + (addr >> 1);
}

/* Memory mapped I/O */

/*static U16 qliomemory[qliomem_mask/2];*/

static U32 qliomem_lget(A32) REGPARAM;
static U16 qliomem_wget(A32) REGPARAM;
static U8 qliomem_bget(A32) REGPARAM;
static void  qliomem_lput(A32, U32) REGPARAM;
static void  qliomem_wput(A32, U16) REGPARAM;
static void  qliomem_bput(A32, U8) REGPARAM;
static int  qliomem_check(A32 addr, U32 size) REGPARAM;
static U16 *qliomem_xlate(A32 addr) REGPARAM;

U32 qliomem_lget(A32 addr)
{
MCT(24);
	return qliord_l(addr);
}

U16 qliomem_wget(A32 addr)
{
U16 tmp;
MCT(25);

	tmp=qliord_b(addr);  /* make sure we have the right execution order */
	return tmp*256+qliord_b(addr+1);
}

U8 qliomem_bget(A32 addr)
{
MCT(26);

	if ((addr<0x18100)||(addr>=0x18400)) return qliord_b(addr);

/* a piece of memory needed for nfa: [0x18100..0x18400) */
/* this should be moved over there ... */

	mem_bget(rammem_start,rammem_mask,rammemory,addr);
/*
    addr -= qliomem_start & (qliomem_mask-1);
    addr &= qliomem_mask-1;
    if (addr & 1)
	return qliomemory[addr >> 1];
    else
	return qliomemory[addr >> 1] >> 8;
*/
}

void qliomem_lput(A32 addr, U32 l)
{
MCT(28);

	mem_lput(rammem_start,rammem_mask,rammemory,addr,l); /* for ? */
}

void qliomem_wput(A32 addr, U16 w)
{
MCT(29);

	qliowr_b(addr,(w>>8)&0xff);
	qliowr_b(addr+1,w&0xff);
	mem_wput(rammem_start,rammem_mask,rammemory,addr,w); /* for nfa :-< */
}

void qliomem_bput(A32 addr, U8 b)
{
MCT(30);

	qliowr_b(addr,b);
	mem_bput(rammem_start,rammem_mask,rammemory,addr,b); /* for nfa :-< */
}

int qliomem_check(A32 addr, U32 size)
{
MCT(25);
    addr -= qliomem_start & (qliomem_mask-1);
    addr &= qliomem_mask-1;
if(0)fpr("%06x ",addr);
    return (addr + size) < qliomem_mask;
}

U16 *qliomem_xlate(A32 addr)
{
MCT(31);
    addr -= rammem_start & (rammem_mask-1);
    addr &= rammem_mask-1;
if(1)fpr("Do you want to exec in IO area? %x ",addr);
    return rammemory + (addr >> 1);
}

/* ROMs */

/*static U16 rommemory[rommem_mask/2];*/

static U32 rommem_lget(A32) REGPARAM;
static U16 rommem_wget(A32) REGPARAM;
static U8 rommem_bget(A32) REGPARAM;
static void  rommem_lput(A32, U32) REGPARAM;
static void  rommem_wput(A32, U16) REGPARAM;
static void  rommem_bput(A32, U8) REGPARAM;
static int  rommem_check(A32 addr, U32 size) REGPARAM;
static U16 *rommem_xlate(A32 addr) REGPARAM;

U32 rommem_lget(A32 addr)
{
MCT(32);

#if DO_TRACE
	if ( (addr&(rammem_mask-1)) != addr )
		fpr("AM %x %x %x %ld ",addr,rammem_mask,m68k_getpc(),cycle);
	addr -= rammem_start & (rammem_mask-1); addr &= rammem_mask-1;
	{ U32 t = *(U32*) ((U8*)(rammemory) + addr);
	  U32 r = ((t>>16)&0xffff) | ((t<<16)&0xffff0000);
	  store_trace(8,addr,r);
	  return r;
	}
#else
	mem_lget(rammem_start,rammem_mask,rammemory,addr);
#endif
}

U16 rommem_wget(A32 addr)
{
MCT(33);

#if DO_TRACE
	if ( (addr&(rammem_mask-1)) != addr )
		fpr("AM %x %x %x %ld ",addr,rammem_mask,m68k_getpc(),cycle);
	addr -= rammem_start & (rammem_mask-1); addr &= rammem_mask-1;
	{ U16 *p = (U16 *) ((U8 *) (rammemory) + addr);
	  store_trace(9,addr,*p);
	  return *p;
	}
#else
	mem_wget(rammem_start,rammem_mask,rammemory,addr);
#endif
}

U8 rommem_bget(A32 addr)
{
MCT(34);

#if DO_TRACE
	if ( (addr&(rammem_mask-1)) != addr )
		fpr("AM %x %x %x %ld ",addr,rammem_mask,m68k_getpc(),cycle);
	addr -= rammem_start & (rammem_mask-1); addr &= rammem_mask-1;
	{ U8 *p = (U8 *) (rammemory) + (addr^1);
	  store_trace(10,addr,*p);
	  return *p;
	}
#else
	mem_bget(rammem_start,rammem_mask,rammemory,addr);
#endif
}

void rommem_lput(A32 addr, U32 l)
{
MCT(36);
#if DO_TRACE
	if ( (addr&(rammem_mask-1)) != addr )
		fpr("AM %x %x %x %ld ",addr,rammem_mask,m68k_getpc(),cycle);
	store_trace(4,addr,l);

	fpr("WL%x %lx %lx ",addr,m68k_getpc(),cycle);
//	trigger_trduco();
#else
	if(addr != 0xc0000) /* JS&JM roms do this */
		fpr("WL%x %x",addr,m68k_getpc());
#endif
}

void rommem_wput(A32 addr, U16 w)
{
MCT(37);
	fpr("WW%x %x\n",addr,m68k_getpc());
}

void rommem_bput(A32 addr, U8 b)
{
MCT(38);
	fpr("WB%x %x\n",addr,m68k_getpc());
}

int rommem_check(A32 addr, U32 size)
{
MCT(35);
    addr -= rommem_start & (rommem_mask-1);
    addr &= rommem_mask-1;
    return (addr + size) < rommem_mask;
}

U16 *rommem_xlate(A32 addr)
{
MCT(39);
    addr -= rammem_start & (rammem_mask-1);
    addr &= rammem_mask-1;
    return rammemory + (addr >> 1);
}

/* Address banks */

addrbank dummy_bank = {
    dummy_lget, dummy_wget, dummy_bget,
    dummy_lput, dummy_wput, dummy_bput,
    dummy_xlate, dummy_check
};

addrbank rammem_bank = {
    rammem_lget, rammem_wget, rammem_bget,
    rammem_lput, rammem_wput, rammem_bput,
    rammem_xlate, rammem_check
};

addrbank qliomem_bank = {
    qliomem_lget, qliomem_wget, qliomem_bget,
    qliomem_lput, qliomem_wput, qliomem_bput,
    qliomem_xlate, qliomem_check
};

addrbank rommem_bank = {
    rommem_lget, rommem_wget, rommem_bget,
    rommem_lput, rommem_wput, rommem_bput,
    rommem_xlate, rommem_check
};

addrbank scrmem_bank = {
    scrmem_lget, scrmem_wget, scrmem_bget,
    scrmem_lput, scrmem_wput, scrmem_bput,
    scrmem_xlate, scrmem_check
};

void flush_vidbuf(int mode)
{
int i;
	if (!(mode&0x80)) for (i=0x20000;i<0x28000;i+=2) do_scrn(i,get_word(i));
	else for (i=0x28000;i<0x30000;i+=2) do_scrn(i,get_word(i));
}

void dump_memory()
{
FILE	*f;
U32	i;

	MCTD;
	MCTI;
	f=fopen("qlay.dmp","wb");
	for (i=0x00000;i<0x18000;i++) fputc(get_byte(i),f);
	for (i=0x18000;i<0x20000;i+=2) {fputc('Q',f);fputc('L',f);}
	for (i=0x20000;i<ramtop;i++) fputc(get_byte(i),f);
	fclose(f);
}

/* load boot and cartridges, return 0 if successfull */
static int load_roms(void)
{
int	currcart;
FILE	*f;
char	cartfn[256];
U32	i,cartaddr,cartend,a,rom_length;

	f = fopen(romfile, "rb");
	if (f == NULL) {
		epr("No BOOT ROM found.\n");
		return(1);
	}

	cartaddr=0;

	fseek(f,0L,2);
	rom_length=ftell(f);
	fseek(f,0L,0);

	cartend=cartaddr+rom_length-1;
	if (cartend>=0x18000) { /* use fake ram too */
		epr("ROM too large > 0x18000\n");
		return(1);
	}

	for (a=cartaddr;a<cartaddr+rom_length;a++) {
		U8 *p = (U8 *) (rammemory) + (a^1); *p = fgetc(f);
	}
	fclose(f);

	epr("%08x-%08x: %s\n",cartaddr,cartend,romfile);
	rom_length=(rom_length+0x3fff)&0xffffc000; /* 4k chunks */
	cartaddr&=0xffffc000;	/* 4k bound start addr */
	map_banks(rommem_bank,cartaddr,rom_length);

	/* now the other cartridges */
	currcart=0;
	while (strlen(opt_cartfn[currcart])) {
		cartaddr=opt_cartaddr[currcart];
		strcpy(cartfn,opt_cartfn[currcart]);
/*
		i=sscanf(opt_cartfn[currcart],"%x@%s",&cartaddr,cartfn);
		if (i!=2) {
			epr("Cartridge option error: %s\n",opt_cartfn[currcart]);
			return(1);
		}
*/
		f = fopen(cartfn, "rb");
		if (f == NULL) {
			epr("Cartridge file not found: %s\n",cartfn);
			return(1);
		}

		fseek(f,0L,2);
		rom_length=ftell(f);
		fseek(f,0L,0);

		cartend=cartaddr+rom_length-1;
		if (cartend>=0x100000) { /* use fake ram too */
			epr("Cartridge memory out of bounds > %x\n",ramtop);
			fclose(f);
			return(1);
		}

		/* check i/o overlap */
		if ( ((cartaddr>=0x18000)&&(cartaddr<0x1c000)) ||
			((cartend>=0x18000)&&(cartend<0x1c000)) ) {
			epr("Cartridge overlaps I/O memory\n");
			fclose(f);
			return(1);
		}
		/* check ram overlap */
		if ( ((cartaddr>=0x20000)&&(cartaddr<0xc0000)) ||
			((cartend>=0x20000)&&(cartend<0xc0000)) ) {
			epr("Cartridge overlaps RAM memory\n");
			fclose(f);
			return(1);
		}

		for (a=cartaddr;a<cartaddr+rom_length;a++) {
			U8 *p = (U8 *) (rammemory) + (a^1); *p = fgetc(f);
		}
		fclose(f);

		epr("%08x-%08x: %s\n",cartaddr,cartend,cartfn);
		rom_length=(rom_length+0x3fff)&0xffffc000; /* 4k chunks */
		cartaddr&=0xffffc000;	/* 4k bound start addr */
		map_banks(rommem_bank,cartaddr,rom_length);
		currcart++;
	}
	return 0;
}

/* load boot and cartridges from internal arrays, return 0 if successfull */
static int load_arrays(U8 *array, U32 start, U32 length, char *romname)
{
U32	a,rom_length;

	for (a=start;a<start+length;a++) {
		U8 *p = (U8 *) (rammemory) + (a^1);
		*p = *(array+a-start);
	}

	epr("%08x-%08x: %s\n",start,start+length-1,romname);
	length=(length+0x3fff)&0xffffc000; /* 4k chunks */
	start&=0xffffc000;	/* 4k bound start addr */
	map_banks(rommem_bank,start,length);

	return 0;
}

void init_memory(void)
{
int i;

	addresserror = 0;
	for(i = 0; i < 65536; i++) membanks[i] = dummy_bank;

	ramtop=0xc0000;
	if (ram_size==0) ramtop=0x40000;
	if (ram_size>0) {
		ramtop=ram_size*0x100000;
	}

/*082f*/
	if (ramtop > rammem_mask) ramtop=rammem_mask;


	/* fake ram till 1Mbyte, for cartridges */
	{ U32 t;
		t=ramtop;
		if (t<0x100000) t=0x100000;
		rammemory = (U16 *)malloc(t);
	}
	if (!rammemory) {
		epr("Out of virtual memory - reduce memory size, exiting\n");
		exit(1);
	}

	if (ramtop<0x100000) { /* don't lie, just present it differently */
		epr("RAMSIZE=%dk\n",ramtop/1024-128);
	} else {
		epr("RAMTOP=%dk\n",ramtop/1024);
	}

	ramtop_p2=rammem_mask;
	for (i=0;i<16;i++) {
		if ( (ramtop_p2&ramtop) == ramtop_p2 ) {
			if (ramtop_p2 == ramtop) break;
			ramtop_p2<<=1; break;
		}
		ramtop_p2>>=1;
	}
	map_banks(rommem_bank, 0x00000, 0x18000);
	map_banks(rammem_bank, 0x20000, ramtop-0x20000);
	map_banks(qliomem_bank,0x18000, 0x04000);
	map_banks(scrmem_bank, 0x20000, 0x08000); /* 1st only!! */

	memset(rammemory, 0, ramtop);
if(1)	if (load_roms()) {
		epr("ROM load error. Exiting\n");
		exit(1);
	}
//	load_arrays(romjs,   0,      ROMJS_LEN,   "JS.ROM");
//	load_arrays(romjsl1, 0,      ROMJSL1_LEN, "JSL1.ROM");
//	load_arrays(romtk212,0x10000,ROMTK212_LEN,"TK.ROM");
//	load_arrays(romnfa,  0x0c000,ROMNFA_LEN,  "NFA.ROM");
	MCTI;
}

void map_banks(addrbank bank, int start, int size)
{
int	bnr,hioffs,mirror;

	/* 16k banks */
	mirror=ramtop_p2/0x4000;
	mirror=512;	/* further debugging needed */
	start/=0x4000;
	size/=0x4000;

	for (hioffs = 0; hioffs < 65536/mirror; hioffs++)
		for (bnr = start; bnr < start+size; bnr++)
			membanks[bnr + hioffs * mirror ] = bank;
}
