/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	68k executor defines and functions
*/

extern void MakeSR(void);
extern void MakeFromSR(void);
extern void Exception(int,A32);
extern void m68k_reset(void);
extern void m68k_move2c(int, U32 *);
extern void m68k_movec2(int, U32 *);
extern void m68k_divl (U16, U32, U16);
extern void m68k_mull (U16, U32, U16);
extern void init_m68k (void);
extern void MC68000_step(void);
extern void MC68000_run(void);
extern void MC68000_skip(A32);
extern void MC68000_dumpstate(A32 *);
extern void MC68000_disasm(A32,A32 *,int);
extern void MC68000_reset(void);
extern void start_PC_dump(void);
extern void trigger_trduco(void);
extern void store_trace(U8,A32,U32);
extern void traceia_dump(void);
extern void dump_counts(void);

extern int areg_byteinc[];
extern int imm8_table[];
extern int broken_in;

typedef void cpuop_func(U32) REGPARAM;

struct cputbl {
	cpuop_func *handler;
	int specific;
	U16 opcode;
};

extern struct cputbl smallcputbl[];

extern cpuop_func *cpufunctbl[65536];
extern void op_illg(U32) REGPARAM;

typedef char flagtype;

union flagu {
	struct {
		/* v must be at the start so that the x86 seto instruction
		 * changes the V flag. C must follow after V. */
		char v;
		char c;
		char n;
		char z;
	} flags;
	U32 longflags;
};

extern struct regstruct
{
	U32 d[8];
	A32  a[8],usp;
	U16 sr;
	flagtype t;
	flagtype s;
	flagtype x;
	flagtype stopped;
	int intmask;
	U32 pc;
	U16 *pc_p;
	U16 *pc_oldp;

	U32 vbr,sfc,dfc;
	U32 oldpc;
	U16 opcode;
} regs;

#ifdef INTEL_FLAG_OPT
extern union flagu intel_flag_lookup[256] __asm__ ("intel_flag_lookup");
extern union flagu regflags __asm__ ("regflags");
#else
extern union flagu regflags;
#endif

#define ZFLG (regflags.flags.z)
#define NFLG (regflags.flags.n)
#define CFLG (regflags.flags.c)
#define VFLG (regflags.flags.v)

extern U32 cycle, cycle_next_event, specialflags;

#define SPCFLAG_CAS_KEY		0x001
#define SPCFLAG_STOP		0x002
#define SPCFLAG_INT		0x004
#define SPCFLAG_DOINT		0x008
#define SPCFLAG_BRK		0x010
#define SPCFLAG_BUSY_WAIT	0x020
#define SPCFLAG_TRACE		0x040
#define SPCFLAG_DOTRACE		0x080

extern void MC68000_oldstep(U16 opcode);

static __inline__ U16 nextiword(void)
{
	U16 r = *regs.pc_p++;
	return r;
}

static __inline__ U32 nextilong(void)
{
	U32 t = *(U32*) ((U8*)(regs.pc_p));
	t= ((t>>16)&0xffff) | ((t<<16)&0xffff0000);
	regs.pc_p+=2; /* 2 words = 1 long */
	return t;
}

static __inline__ void m68k_setpc(A32 newpc)
{
	regs.pc = newpc;
	regs.pc_p = regs.pc_oldp = get_real_address(newpc);
}

static __inline__ A32 m68k_getpc(void)
{
	return regs.pc + ((char *)regs.pc_p - (char *)regs.pc_oldp);
}

static __inline__ void m68k_setstopped(int stop)
{
	regs.stopped = stop;
	if (stop)
		specialflags |= SPCFLAG_STOP;
}

static __inline__ int cctrue(const int cc)
{
	switch(cc){
		case 0: return 1;                       /* T */
		case 1: return 0;                       /* F */
		case 2: return !CFLG && !ZFLG;          /* HI */
		case 3: return CFLG || ZFLG;            /* LS */
		case 4: return !CFLG;                   /* CC */
		case 5: return CFLG;                    /* CS */
		case 6: return !ZFLG;                   /* NE */
		case 7: return ZFLG;                    /* EQ */
		case 8: return !VFLG;                   /* VC */
		case 9: return VFLG;                    /* VS */
		case 10:return !NFLG;                   /* PL */
		case 11:return NFLG;                    /* MI */
		case 12:return NFLG == VFLG;            /* GE */
		case 13:return NFLG != VFLG;            /* LT */
		case 14:return !ZFLG && (NFLG == VFLG); /* GT */
		case 15:return ZFLG || (NFLG != VFLG);  /* LE */
	}
	abort();
	return 0;
}

static __inline__ U32 get_disp_ea (U32 base, U16 dp)
{
int reg = (dp >> 12) & 7;
S32 regd;

	if (dp & 0x8000)
		regd = regs.a[reg];
	else
		regd = regs.d[reg];
	if (!(dp & 0x800))
		regd = (S32)(S16)regd;
	return base + (S8)(dp) + regd;
}
