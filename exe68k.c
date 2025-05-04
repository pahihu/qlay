/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	68k CPU executor functions
*/

#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "qlmem.h"
#include "exe68k.h"
#include "readcpu.h"
#include "spc-os.h"
#include "qlio.h"

//static
	void MC68000_dumpstate_f(void);

static char* ccnames[] =
{
	"T ","F ","HI","LS","CC","CS","NE","EQ",
	"VC","VS","PL","MI","GE","LT","GT","LE"
};

static int illegal_instr_cnt=0;

int broken_in;

#ifdef INTEL_FLAG_OPT
union flagu intel_flag_lookup[256];
#endif

U32 cycle_next_event;
U32 cycle;
U32 specialflags;

struct regstruct regs;
union flagu regflags;

#if DO_TRACE
#define TRBLEN (8*65536)
static U8 tr_t[TRBLEN];	/* type */
static A32  tr_a[TRBLEN];	/* addr */
static U32 tr_d[TRBLEN];	/* data */
static U32 tr_c[TRBLEN];	/* cycle */
int tracepos=0;
#endif

int areg_byteinc[] = { 1,1,1,1,1,1,1,2 }; // A7 does word incs
int imm8_table[] = { 8,1,2,3,4,5,6,7 };

cpuop_func *cpufunctbl[65536];

#ifdef COUNT_INSTRS
static unsigned long int instrcount[65536];
static U16 opcodenums[65536];



int compfn(const void *el1, const void *el2)
{
	return instrcount[*(const U16 *)el1] < instrcount[*(const U16 *)el2];
}

void dump_counts(void)
{
FILE *f = fopen("insncount", "w");
unsigned long int total = 0;
int i;

	for(i=0; i < 65536; i++) {
		opcodenums[i] = i;
    		total += instrcount[i];
	}
	qsort(opcodenums, 65536, sizeof(U16), compfn);

	fprintf(f, "Total: %ld\n", total);
	for(i=0; i < 65536; i++) {
		unsigned long int cnt = instrcount[opcodenums[i]];
		if (!cnt)
			break;
		fprintf(f, "%04x: %ld\n", opcodenums[i], cnt);
	}
	fclose(f);
}
#endif

void init_m68k (void)
{
long int opcode;
int i;

#if MEMCNT == 1
	instrcnt=0;
#endif

#if DO_TRACE
	for(i=0;i<TRBLEN;i++) {
	    	tr_t[i]=0;
	    	tr_d[i]=0;
		tr_a[i]=0;
		tr_c[i]=0;
	}
	tracepos=0;
#endif

#ifdef COUNT_INSTRS
	memset(instrcount, 0, sizeof instrcount);
#endif
#ifdef INTEL_FLAG_OPT
	for (i = 0; i < 256; i++) {
		intel_flag_lookup[i].flags.c = !!(i & 1);
		intel_flag_lookup[i].flags.z = !!(i & 64);
		intel_flag_lookup[i].flags.n = !!(i & 128);
		intel_flag_lookup[i].flags.v = 0;
	}
#endif

	epr("%d instructions/msec\n",qlay1msec);
	epr("Preparing M68k emulation ...\n");
	if (read_table68k ()) {
		epr("Out of virtual memory t68k, exiting\n");
		exit(1);
	}

	do_merges ();
	for (opcode = 0; opcode < 65536; opcode++)
		cpufunctbl[opcode] = op_illg;
	for (i = 0; smallcputbl[i].handler != NULL; i++) {
		if (!smallcputbl[i].specific)
			cpufunctbl[smallcputbl[i].opcode] = smallcputbl[i].handler;
	}
	for (opcode = 0; opcode < 65536; opcode++) {
		cpuop_func *f;

		if (table68k[opcode].mnemo == i_ILLG)
			continue;

		if (table68k[opcode].handler != -1) {
			f = cpufunctbl[table68k[opcode].handler];
			if (f == op_illg)
				abort();
			cpufunctbl[opcode] = f;
		}
	}
	for (i = 0; smallcputbl[i].handler != NULL; i++) {
		if (smallcputbl[i].specific)
			cpufunctbl[smallcputbl[i].opcode] = smallcputbl[i].handler;
	}
}

void MakeSR(void)
{
#if 0
	assert((NFLG & 1) == NFLG);
	assert((regs.s & 1) == regs.s);
	assert((regs.x & 1) == regs.x);
	assert((CFLG & 1) == CFLG);
	assert((VFLG & 1) == VFLG);
	assert((ZFLG & 1) == ZFLG);
#endif
	regs.sr = ((regs.t << 15) | (regs.s << 13) | (regs.intmask << 8)
		| (regs.x << 4) | (NFLG << 3) | (ZFLG << 2) | (VFLG << 1)
		|  CFLG);
}

void MakeFromSR(void)
{
int olds = regs.s;

	regs.t = (regs.sr >> 15) & 1;
	regs.s = (regs.sr >> 13) & 1;
	regs.intmask = (regs.sr >> 8) & 7;
	regs.x = (regs.sr >> 4) & 1;
	NFLG = (regs.sr >> 3) & 1;
	ZFLG = (regs.sr >> 2) & 1;
	VFLG = (regs.sr >> 1) & 1;
	CFLG = regs.sr & 1;
	if (olds != regs.s) {
		A32 temp = regs.usp;
		regs.usp = regs.a[7];
		regs.a[7] = temp;
	}
	if (regs.t)
		specialflags |= SPCFLAG_TRACE;
	else
		specialflags &= ~(SPCFLAG_TRACE | SPCFLAG_DOTRACE);
}

void Exception(int nr, A32 oldpc)
{
	MakeSR();
	if (!regs.s) {
		A32 temp = regs.usp;
		regs.usp = regs.a[7];
		regs.a[7] = temp;
		regs.s = 1;
	}
	/* bus- or address error */
	if ((nr==2)||(nr==3)) {
		regs.a[7] -= 4;
		put_long (regs.a[7], m68k_getpc() ); /*regs.oldpc);*/
		regs.a[7] -= 2;
		put_word (regs.a[7], regs.sr);
		regs.a[7] -=2;
		put_word (regs.a[7], regs.opcode); /* instruction register */
		regs.a[7] -=4;
		put_long (regs.a[7], addresserror); /* current cycle address */
		regs.a[7] -=2;
		put_word (regs.a[7], 0); /* access type */
		fpr("ABerr: PC%08x SR%04x I%04x A%08x AC%04x %ld\n",
		m68k_getpc(),regs.sr,regs.opcode,addresserror,0,cycle);
#if DO_TRACE
if(0)		if (iatrace) {
			traceia_dump();
			dump_memory();
			MC68000_dumpstate_f();
			specialflags |= SPCFLAG_BRK; /* break now! */
		}
#endif
	} else {
		regs.a[7] -= 4;
		if(oldpc>0) {
			put_long (regs.a[7], oldpc);
		} else {
			put_long (regs.a[7], m68k_getpc ());
		}
		regs.a[7] -= 2;
		put_word (regs.a[7], regs.sr);
	}
	m68k_setpc(get_long(regs.vbr + 4*nr));
	regs.t = 0;
	/* priv, trace, A-line, F-line + bus, addr, ilgl: do not take trace exception */
	if ( ((nr>=8)&&(nr<=11)) || ((nr>=2)&&(nr<=4)) ) {
		specialflags &= ~(SPCFLAG_TRACE | SPCFLAG_DOTRACE);
	}
}

static void Interrupt(int nr)
{
	assert(nr < 8 && nr >= 0);
	Exception(nr+24,0);
	regs.intmask = nr;
}

/* RESET instruction */
void m68k_reset()
{
/* should MC8000_reset() be called here? */
}

void MC68000_reset(void)
{
	regs.a[7] = get_long(0x0);
	m68k_setpc(get_long(0x4));
	regs.s = 1;
	regs.stopped = 0;
	regs.t = 0;
	specialflags = 0;
	regs.intmask = 7;
	regs.vbr = regs.sfc = regs.dfc = 0;
	if (opt_busy_wait) specialflags |= SPCFLAG_BUSY_WAIT;
}

void op_illg(U32 opcode)
{
	regs.pc_p--;

	if ((opcode & 0xF000) == 0xF000) {
		Exception(0xB,0);
		return;
	}
	if ((opcode & 0xF000) == 0xA000) {
	    	Exception(0xA,0);
		return;
	}
	fpr("Illegal instruction: %04x, %08x, %ld", opcode,m68k_getpc(),cycle);

#if DO_TRACE
	if (iatrace) traceia_dump();
#endif
	if(0)	dump_memory();
	MC68000_dumpstate_f();

	/* end prgm nicely */
	if (++illegal_instr_cnt>1000) specialflags |= SPCFLAG_BRK;
	if(iatrace) specialflags |= SPCFLAG_BRK; /* break now! */

	Exception(4,0);
}

/* spend some time reading through boot rom area... */
static void handle_busy_wait(void)
{
int	i;
static	A32 ap=0;

	for(i=0;i<opt_busy_wait;i++) {
		ap += 4;
		if (ap>0xb000) ap=0;
		get_long(ap);
	}
}

#if DO_TRACE
void traceia_dump()
{
FILE	*f;
int 	i,t,v;
char	c;

if(0)	return;

	fpr("traceia_dump ");
//	f=fopen("e:\\trace.ia","a"); /*TXT*/
//	fprintf(f,"CYCLE: %ld\n",cycle);
//	fprintf(f,"----------------------\n");
	f=fopen("trace.ia","w"); /*TXT*/
	for (i=0;i<TRBLEN;i++) {
		t=tracepos+i;
		if (t>=TRBLEN)t-=TRBLEN;
		if (t==0) fprintf(f,"*\n");
		v=tr_t[t];
		if (v==0) continue;
		if (v==1) fprintf(f,"\n%06ld c:%09ld ",t,tr_c[t]);
		switch (v) {
			case 0: fprintf(f,"-  "); break;
			case 1: fprintf(f,"I  "); break;
			case 4: fprintf(f,"WL "); break;
			case 5: fprintf(f,"WW "); break;
			case 6: fprintf(f,"WB "); break;
			case 8: fprintf(f,"RL "); break;
			case 9: fprintf(f,"RW "); break;
			case 10:fprintf(f,"RB "); break;
			default:fprintf(f,"?? "); break;
		}
		c=':';
		if (v!=1) c='=';
/* temp stuff for loaded pgms */
if(0)		{U32 ta=tr_a[t];
			if ((v==1)&&(ta > 0x28000)) {
				ta-=(0x9ee7e - 0x10486); // xchange
//				ta-=0xb31d8; // archive
				c='>';
			}
			tr_a[t]=ta;
		}
		fprintf(f,"a%c%08x d:%08x ",c,tr_a[t],tr_d[t]);
	}
	fprintf(f,"\n");
	fclose(f);
}
#endif

int PC_trace=0;

void start_PC_dump()
{
	PC_trace=1;
}

/* 085a */
#if DO_TRACE
static int trduco=-1;
void trigger_trduco()
{
	if (trduco!=-1) {
		fpr("trduco already triggered, ignored\n");
		return;
	}
	trduco=TRBLEN/2;	// halfway
	trduco=10;		// stop after next 10 cycles
//	trduco=TRBLEN-TRBLEN/256; 	// store as much as possible after trigger
	fpr("trduco triggered, cycle %ld\n",cycle);
}

void store_trace (U8 t, A32 addr, U32 data)
{
A32 ciaddr;
static int store_it=0;

	ciaddr=m68k_getpc()-2;
//	if ((ciaddr>=0xc0e4)&&(ciaddr<=0xc17a)) MC68000_dumpstate_f();
//	if (PC_trace&&regs.intmask>0) fpr("%04x.%d ",ciaddr,regs.intmask);
//	if (ciaddr==0x2b260) trigger_trduco(); // zkul start

	if (!iatrace) return;
if(0)	if((addr>=0xbda90)&&(addr<0xbdaa0))fpr("t%d i%08x a%08x d%08x cy%ld\n",
		t,ciaddr,addr,data,cycle);
if(0)	if(addr==0x6e854)fpr("t%d i%08x a%08x d%08x cy%ld\n",
		t,ciaddr,addr,data,cycle);

	if ((!store_it)&&(t==1)) store_it=1;
	if (store_it) {
		/* instruction in RAM? */
		if ((t==1)&&(addr<0x28000)) {
			store_it=0;
		}
	}
	tr_t[tracepos]=t;
	tr_a[tracepos]=addr;
	tr_d[tracepos]=data;
	tr_c[tracepos]=cycle;
	if (store_it) tracepos++;

	if (trduco>=0) {
		if (trduco==0) {
			trduco--;
			fpr("trduco 0, cycle %ld\n",cycle);
			traceia_dump();
			dump_memory();	/* calls store_trace recursively */
			specialflags |= SPCFLAG_BRK;
		} else {
			if (store_it) trduco--;
		}
	}

	if (tracepos>TRBLEN) tracepos=0;
	/* assert: tracepos always indicates next available spot */
}
#endif

#define do_silly_hw() if(cycle==cycle_next_event)do_next_event();cycle++;

void MC68000_run(void)
{
	for(;;) {
		U16 opcode;

if(0)		{A32 ciaddr;
			ciaddr=m68k_getpc()-2;
			if ((ciaddr>=0xdfa)&&(ciaddr<=0xe40)) MC68000_dumpstate_f();
			if ((ciaddr>=0xc124)&&(ciaddr<=0xc1a2)) MC68000_dumpstate_f();
		}
		opcode = nextiword();

#if DO_TRACE
		if (iatrace) store_trace(1, (A32)(m68k_getpc()-2), opcode);
#endif

#ifdef COUNT_INSTRS
		instrcount[opcode]++;
#endif

#if EMUL_EXCEPTION_3 == 1
		regs.opcode=opcode;
#endif
		(*cpufunctbl[opcode])(opcode);
#if EMUL_EXCEPTION_3 == 1
		if (addresserror) {
			Exception(3,0);
			addresserror = 0;
		}
#endif

		do_silly_hw();
		if (specialflags) {
			if (specialflags & SPCFLAG_DOTRACE) {
				Exception(9,0);
			}
			while (specialflags & SPCFLAG_STOP) {
				/* wait a bit, not so fast! */
				{ int w; for (w=0;w<2;w++) get_long(100*w);
				}
				do_silly_hw();
				if (specialflags & (SPCFLAG_INT | SPCFLAG_DOINT)){
				    int intr = irq_level();
				    specialflags &= ~(SPCFLAG_INT | SPCFLAG_DOINT);
				    if (intr != -1 && intr > regs.intmask) {
					Interrupt(intr);
					regs.stopped = 0;
					specialflags &= ~SPCFLAG_STOP;
				    }
				}
			}
			if (specialflags & SPCFLAG_TRACE) {
				specialflags &= ~SPCFLAG_TRACE;
				specialflags |= SPCFLAG_DOTRACE;
			}
			if (specialflags & SPCFLAG_DOINT) {
				int intr = irq_level();
				if (intr != -1 && intr > regs.intmask) {
				    Interrupt(intr);
				    regs.stopped = 0;
				    specialflags &= ~(SPCFLAG_INT | SPCFLAG_DOINT);
				}
			}
			if (specialflags & SPCFLAG_INT) {
				specialflags &= ~SPCFLAG_INT;
				specialflags |= SPCFLAG_DOINT;
			}
		 	if (specialflags & SPCFLAG_CAS_KEY) {
				handle_cas_key();
				specialflags &= ~SPCFLAG_CAS_KEY;
			}
			if (specialflags & SPCFLAG_BUSY_WAIT) {
				handle_busy_wait();
			}
			if (specialflags & SPCFLAG_BRK) {
				specialflags &= ~SPCFLAG_BRK;
				return;
			}
		}
	}
}

void MC68000_step(void)
{
	specialflags |= SPCFLAG_BRK;
	MC68000_run();
}

void MC68000_skip(A32 nextpc)
{
	broken_in = 0;
	specialflags |= SPCFLAG_BRK;
	do {
		MC68000_step();
	} while (nextpc != m68k_getpc() && !broken_in);
}

static void ShowEA(int reg, amodes mode, wordsizes size)
{
U16 dp;
S8 disp8;
S16 disp16;
int r;
U32 dispreg;
A32 addr;

	switch(mode){
	case Dreg:
		printf("D%d", reg);
		break;
	case Areg:
		printf("A%d", reg);
		break;
	case Aind:
		printf("(A%d)", reg);
		break;
	case Aipi:
		printf("(A%d)+", reg);
		break;
	case Apdi:
		printf("-(A%d)", reg);
		break;
	case Ad16:
		disp16 = nextiword();
		addr = regs.a[reg] + (S16)disp16;
		printf("(A%d,$%04lx) == $%08lx", reg, disp16, (long unsigned int)addr);
		break;
	case Ad8r:
		dp = nextiword();
		disp8 = dp & 0xFF;
		r = (dp & 0x7000) >> 12;
		dispreg = dp & 0x8000 ? regs.a[r] : regs.d[r];
		if (!(dp & 0x800)) dispreg = (S32)(S16)(dispreg);

		addr = regs.a[reg] + disp8 + dispreg;
		printf("(A%d, %c%d.%c, $%02x) == $%08lx", reg,
			dp & 0x8000 ? 'A' : 'D', (int)r, dp & 0x800 ? 'L' : 'W', disp8,
			(long unsigned int)addr);
		break;
	case PC16:
		addr = m68k_getpc();
		disp16 = nextiword();
		addr += (S16)disp16;
		printf("(PC,$%08lx) == $%08lx", disp16, (long unsigned int)addr);
		break;
	case PC8r:
		addr = m68k_getpc();
		dp = nextiword();
		disp8 = dp & 0xFF;
		r = (dp & 0x7000) >> 12;
		dispreg = dp & 0x8000 ? regs.a[r] : regs.d[r];

		if (!(dp & 0x800)) dispreg = (S32)(S16)(dispreg);
		addr += disp8 + dispreg;
		printf("(PC, %c%d.%c, $%02x) == $%08lx", dp & 0x8000 ? 'A' : 'D',
			(int)r, dp & 0x800 ? 'L' : 'W', disp8, (long unsigned int)addr);
		break;
	case absw:
		printf("$%08lx", (S32)(S16)nextiword());
		break;
	case absl:
		printf("$%08lx", nextilong());
		break;
	case imm:
		switch(size){
		case sz_byte:
			printf("#$%02x", nextiword() & 0xff); break;
		case sz_word:
			printf("#$%04x", nextiword()); break;
		case sz_long:
			printf("#$%08lx", nextilong()); break;
		default:
			abort();
		}
		break;
	case imm0:
		printf("#$%02lx", nextiword() & 0xff);
		break;
	case imm1:
		printf("#$%04lx", nextiword());
		break;
	case imm2:
		printf("#$%08lx", nextilong());
		break;
	case immi:
		printf("#$%04lx", reg);
		break;
	default:
		abort();
	}
}

void MC68000_disasm(A32 addr, A32 *nextpc, int cnt)
{
A32 pc = m68k_getpc();

	m68k_setpc(addr);
	for (;cnt--;){
		char instrname[20],*ccpt;
		int opwords;
		U16 opcode;
		U16 special = 0;
		struct mnemolookup *lookup;
		struct instr *dp;
		printf("%08lx: ", m68k_getpc());
		for(opwords = 0; opwords < 5; opwords++){
			printf("%04x ", get_word(m68k_getpc() + opwords*2));
		}

		opcode = nextiword();
		if (cpufunctbl[opcode] == op_illg) {
			opcode = 0x4AFC;
		}
		dp = table68k + opcode;
		for (lookup = lookuptab;lookup->mnemo != dp->mnemo; lookup++)
			;

		strcpy(instrname,lookup->name);
		ccpt = strstr(instrname,"cc");
		if (ccpt != 0) {
			strncpy(ccpt,ccnames[dp->cc],2);
		}
		printf("%s", instrname);
		switch(dp->size){
		case sz_byte:
			printf(".B "); break;
		case sz_word:
			printf(".W "); break;
		case sz_long:
			printf(".L "); break;
		default:
			break;
		}

		if (dp->suse) {
			ShowEA(dp->sreg, dp->smode, dp->size);
		}
		if (dp->suse && dp->duse)
			printf(",");
		if (dp->duse) {
			ShowEA(dp->dreg, dp->dmode, dp->size);
		}
		if (ccpt != 0) {
			if (cctrue(dp->cc))
				printf(" (TRUE)");
			else
				printf(" (FALSE)");
		}
		printf("\n");
	}
	*nextpc = m68k_getpc();
	m68k_setpc(pc);
}

void MC68000_dumpstate(A32 *nextpc)
{
int i;

	for(i = 0; i < 8; i++){
		printf("D%d: %08lx ", i, regs.d[i]);
		if ((i & 3) == 3) printf("\n");
	}
	for(i=0;i<8;i++){
		printf("A%d: %08lx ", i, regs.a[i]);
		if ((i & 3) == 3) printf("\n");
	}
	printf ("T=%d S=%d X=%d N=%d Z=%d V=%d C=%d IMASK=%d\n", regs.t, regs.s,
		regs.x, NFLG, ZFLG, VFLG, CFLG, regs.intmask);
	MC68000_disasm(m68k_getpc(), nextpc, 1);
	printf("next PC: %08lx\n", *nextpc);
}

/* fprintf version, without disassembly */
void MC68000_dumpstate_f()
{
int i;

	fpr("\nMaster Control Program output\n");
	for(i = 0; i < 8; i++){
		fpr("D%d: %08lx ", i, regs.d[i]);
		if ((i & 3) == 3) fpr("\n");
	}
	for(i=0;i<8;i++){
		fpr("A%d: %08lx ", i, regs.a[i]);
		if ((i & 3) == 3) fpr("\n");
	}
	fpr("USP: %08lx, PC: %08lx, CYCLE: %ld\n",regs.usp,m68k_getpc(),cycle);
	fpr("T=%d S=%d X=%d N=%d Z=%d V=%d C=%d IMASK=%d\n", regs.t, regs.s,
		regs.x, NFLG, ZFLG, VFLG, CFLG, regs.intmask);
}
