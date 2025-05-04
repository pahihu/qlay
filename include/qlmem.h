/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	QL memory defines and functions
*/

#define rammem_start	0x000000
#define rommem_start	0x000000
#define qliomem_start	0x000000
#define scrmem_start	0x000000

/* MASKS MUST BE POWER OF TWO */

/*082f 16M */

#define ALOTOFMEM

#ifdef ALOTOFMEM
#define rammem_mask	0x800000	/* 8M */
#else
#define rammem_mask	0x400000	/* 4M */
#endif

#define rommem_mask	0x100000	/* 1M */
#define qliomem_mask	0x020000
#define scrmem_mask	0x040000	/* both screens */

typedef U32 (*lget_func)(A32) REGPARAM;
typedef U16 (*wget_func)(A32) REGPARAM;
typedef U8 (*bget_func)(A32) REGPARAM;
typedef void (*lput_func)(A32,U32) REGPARAM;
typedef void (*wput_func)(A32,U16) REGPARAM;
typedef void (*bput_func)(A32,U8) REGPARAM;
typedef U16 *(*xlate_func)(A32) REGPARAM;
typedef int (*check_func)(A32, U32) REGPARAM;

typedef struct {
    lget_func lget;
    wget_func wget;
    bget_func bget;
    lput_func lput;
    wput_func wput;
    bput_func bput;
    xlate_func xlateaddr;
    check_func check;
} addrbank;

extern addrbank rammem_bank;
extern addrbank rommem_bank;
extern addrbank scrmem_bank;

extern addrbank membanks[65536];

static __inline__ unsigned int bankindex(A32 a)
{
/*    return a>>16; */
    return (a>>14)&0x0000ffff;
}

static __inline__ U32 longget(A32 addr)
{
    return membanks[bankindex(addr)].lget(addr);
}

static __inline__ U16 wordget(A32 addr)
{
    return membanks[bankindex(addr)].wget(addr);
}

static __inline__ U8 byteget(A32 addr)
{
    return membanks[bankindex(addr)].bget(addr);
}

static __inline__ void longput(A32 addr, U32 l)
{
    membanks[bankindex(addr)].lput(addr, l);
}

static __inline__ void wordput(A32 addr, U16 w)
{
    membanks[bankindex(addr)].wput(addr, w);
}

static __inline__ void byteput(A32 addr, U8 b)
{
    membanks[bankindex(addr)].bput(addr, b);
}

static __inline__ int check_addr(A32 a)
{
#if EMUL_EXCEPTION_3 == 1
    return (a & 1) == 0;
#else
    return 1;
#endif
}

extern int addresserror;

extern void init_memory(void);
extern void map_banks(addrbank bank, int first, int count);

static __inline__ U32 get_long(A32 addr)
{
    if (check_addr(addr))
	return longget(addr);
    addresserror = addr;
    return 0;
}

static __inline__ U16 get_word(A32 addr)
{
    if (check_addr(addr))
	return wordget(addr);
    addresserror = addr;
    return 0;
}

static __inline__ U8 get_byte(A32 addr)
{
    return byteget(addr);
}

static __inline__ void put_long(A32 addr, U32 l)
{
    if (!check_addr(addr))
	addresserror = addr;
    longput(addr, l);
}

static __inline__ void put_word(A32 addr, U16 w)
{
    if (!check_addr(addr))
	addresserror = addr;
    wordput(addr, w);
}

static __inline__ void put_byte(A32 addr, U8 b)
{
    byteput(addr, b);
}

static __inline__ U16 *get_real_address(A32 addr)
{
    if (!check_addr(addr))
	addresserror = addr;
    return membanks[bankindex(addr)].xlateaddr(addr);
}

static __inline__ int valid_address(A32 addr, U32 size)	// not used
{
    if (!check_addr(addr))
	addresserror = addr;
    return membanks[bankindex(addr)].check(addr, size);
}

/* array bounds are checked to guard against pointers out of bounds: */

/* no checking at all */
#if MEMCHK_PRINT == 0
#define membound_check(start,mask,addr) ;
#endif

/* just mask violations */
#if MEMCHK_PRINT == 1
#define membound_check(start,mask,addr) addr -= start & (mask-1); addr &= mask-1;
#endif

/* violations are printed */
#if MEMCHK_PRINT == 2
#define membound_check(start,mask,addr) if ( (addr&(mask-1)) != addr ) { \
	fpr("AdrMsk %x %x %x\n",addr,mask,m68k_getpc()); addr&=mask-1; \
	fpr("AdrMskM %x\n",addr);};
#endif

/* undefined behavior when long or word routines are called with odd address */
/* use EMUL_EXCEPTION_3 to check against violations */
#define mem_lget(start,mask,memory,addr) { \
	membound_check(start,mask,addr); \
	{ U32 t = *(U32*) ((U8*)(memory) + addr); \
	return ((t>>16)&0xffff) | ((t<<16)&0xffff0000); } }

#define mem_wget(start,mask,memory,addr) { \
	membound_check(start,mask,addr); \
	{ U16 *p = (U16 *) ((U8 *) (memory) + addr); return *p; } }

#define mem_bget(start,mask,memory,addr) { \
	membound_check(start,mask,addr); \
	{ U8 *p = (U8 *) (memory) + (addr^1); return *p; } }

#define mem_lput(start,mask,memory,addr,l) { \
	membound_check(start,mask,addr); \
	{ U32 *p = (U32*) ((U8*)(memory) + addr); \
	*p = ((l>>16)&0xffff) | ((l<<16)&0xffff0000); } }

#define mem_wput(start,mask,memory,addr,w) { \
	membound_check(start,mask,addr); \
	{ U16 *p = (U16 *) ((U8 *) (memory) + addr); *p =w; } }

#define mem_bput(start,mask,memory,addr,b) { \
	membound_check(start,mask,addr); \
	{ U8 *p = (U8 *) (memory) + (addr^1); *p = b; } }

/*
A short note on memory banks:

MC68000 data is stored in words and optimized for word access=instruction size.
Due to Intel's little endian vs. Motorola's big endian a choice has to be made:
QLAY uses Intel little endian words in memory.

Intel pointers:
L 0               4               8
W 0       2       4       6       8
B 0   1   2   3   4   5   6   7   8
+---+---+---+---+---+---+---+---+--
| 1 | 0 | 3 | 2 | 5 | 4 | 7 | 6 | ..  Motorola's bytes
+---+---+---+---+---+---+---+---+--

For Intel to access a:
 byte at address x, access at (x^1).B
 word at address x, access at x.W
 long at address x, access at x.L, and swap the two resulting words

*/

extern void flush_vidbuf(int);
extern void dump_memory(void);
extern U32 instrcnt;
