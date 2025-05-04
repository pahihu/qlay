/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	Command line options and global defines, main()
*/

typedef enum {
	KEYB_US,
	KEYB_UK,
	KEYB_GE,
	KEYB_FR,
	KEYB_IT,
	KEYB_SE
} T_KEYB;

extern int produce_sound;
extern U32 qlay1msec;
extern int use_debugger;
extern int opt_use_mouse;
extern int opt_use_altkey;
extern int ram_size;
extern int fake_joystick;
extern int fakeF1;
extern int iatrace;
extern T_KEYB opt_keyb;
extern int screen_res;
extern int opt_busy_wait;
extern int opt_new_gfx;

#define LOG_FILE "qlay.log"
extern char romfile[];
extern char prtname[];
extern char sername[][256];
extern char opt_cartfn[][256];
extern U32 opt_cartaddr[];
extern char mdvfn[][256];
extern char winfn[][256];
extern int winpresent[];

extern int ql_main1(int,char **);
extern int ql_main2(void);
extern int ql_exit(void);
extern void write_options(void);

#ifdef __mac__
/* Apparently, no memcpy :-/ */
static __inline__ void *memcpy(void *to, void *from, int size)
{
	BlockMove(from, to, size);
}
#endif

/* strdup() may be non-portable if you have a weird system */
static char *my_strdup(const char*s)
{
	/* The casts to char * are there to shut up the compiler on HPUX */
	char *x = (char*)malloc(strlen((char *)s) + 1);
	strcpy(x, (char *)s);
	return x;
}

#undef REGPARAM
#define fast_memcmp memcmp
#define memcmpy generic_memcmpy

static __inline__ int generic_memcmpy(void *foo, const void *bar, int len)
{
	int res = memcmp(foo, bar, len);
	if (res)
		memcpy(foo, bar, len);
	return res;
}

#if defined(__GNUC_MINOR__)

#ifdef __i386__

#define INTEL_FLAG_OPT

#undef fast_memcmp
#undef memcmpy

static __inline__ int fast_memcmp(const void *foo, const void *bar, int len)
{
int differs, baz;

	__asm__ __volatile__ ("subl $4, %2\n"
		"jc  2f\n"
		"1:\n"
		"movl (%0),%%ebx\n"
		"cmpl (%1),%%ebx\n"
		"jne 5f\n"
		"addl $4, %0\n"
		"addl $4, %1\n"
		"subl $4, %2\n"
		"jnc  1b\n"
		"2:\n"
		"addl $4, %2\n"
		"jz 4f\n"
		"3:\n"
		"movb (%0),%%bl\n"
		"cmpb (%1),%%bl\n"
		"jne 5f\n"
		"incl %0\n"
		"incl %1\n"
		"decl %2\n"
		"jnz 3b\n"
		"4:\n"
		"movl $0, %3\n"
		"jmp 6f\n"
		"5:\n"
		"movl $1, %3\n"
		"6:\n"
		: "=&r" (foo), "=&r" (bar), "=&rm" (len), "=rm" (differs),
		  "=&b" (baz)
		: "0" (foo), "1" (bar), "2" (len), "3" (baz) : "cc");
	return differs;
}

static __inline__ int memcmpy(void *foo, const void *bar, int len)
{
int differs, baz = 0, uupzuq = 0;

	__asm__ __volatile__ ("subl %1, %2\n"
		"movl $0, %0\n"
		"subl $16, %3\n"
		"jc 7f\n"

		"8:\n"
		"movl (%1),%%ecx\n"
		"movl (%2,%1),%%ebx\n"
		"xorl %%ebx, %%ecx\n"
		"movl %%ebx, (%1)\n"
		"orl %%ecx, %0\n"

		"movl 4(%2,%1),%%ebx\n"
		"movl 4(%1),%%ecx\n"
		"xorl %%ebx, %%ecx\n"
		"movl %%ebx, 4(%1)\n"
		"orl %%ecx, %0\n"

		"movl 8(%2,%1),%%ebx\n"
		"movl 8(%1),%%ecx\n"
		"xorl %%ebx, %%ecx\n"
		"movl %%ebx, 8(%1)\n"
		"orl %%ecx, %0\n"

		"movl 12(%2,%1),%%ebx\n"
		"movl 12(%1),%%ecx\n"
		"xorl %%ebx, %%ecx\n"
		"movl %%ebx, 12(%1)\n"
		"orl %%ecx, %0\n"

		"addl $16, %1\n"
		"subl $16, %3\n"
		"jnc  8b\n"

		"7:\n"
		"addl $16, %3\n"
		"subl $4, %3\n"
		"jc  2f\n"

		"1:\n"
		"movl (%2,%1),%%ebx\n"
		"movl (%1),%%ecx\n"
		"xorl %%ebx, %%ecx\n"
		"movl %%ebx, (%1)\n"
		"orl %%ecx, %0\n"
		"addl $4, %1\n"
		"subl $4, %3\n"
		"jnc  1b\n"

		"2:\n"
		"addl $4, %3\n"
		"jz 6f\n"
		"xorl %%ecx, %%ecx\n"

		"3:\n"
		"movb (%2,%1),%%bl\n"
		"movb (%1),%%cl\n"
		"xorb %%cl,%%bl\n"
		"movb %%bl,(%1)\n"
		"orl %%ecx, %0\n"
		"incl %1\n"
		"decl %3\n"
		"jnz 3b\n"

		"6:\n"
		: "=m" (differs)
		: "r" (foo), "r" (bar), "r" (len), "b" (baz), "c" (uupzuq) : "cc", "memory");

/*
Now tell the compiler that foo, bar and len have been modified
If someone finds a way to express all this cleaner in constraints that
GCC 2.7.2 understands, please FIXME
*/
	__asm__ __volatile__ ("" : "=rm" (foo), "=rm" (bar), "=rm" (len) : :  "ebx", "ecx", "edx", "eax", "esi", "memory");
	return differs;
}
#endif

#if __GNUC__ > 2 || __GNUC_MINOR__ > 6

#if defined(__i386__) && !defined(USE_PROFILING)
#define REGPARAM __attribute__((regparm(3)))
#endif /* __i386__ */

#endif /* GCC version 2.7 or higher */
#endif /* GCC 2 */

#ifndef REGPARAM
#define REGPARAM
#endif

#ifdef __CYGWIN32__
/* Apparently, no portb i/o */
static __inline__ void outportb (unsigned short _port, unsigned char _data)
{
	__asm__ __volatile__ ("outb %1, %0"
		:
		: "d" (_port),
		"a" (_data));
}

static __inline__ unsigned char inportb (unsigned short _port)
{
unsigned char rv;
	__asm__ __volatile__ ("inb %1, %0"
		: "=a" (rv)
		: "d" (_port));
	return rv;
}
#endif

/*
You can specify numbers from 0 to 5 here. It is possible that higher
numbers will make the CPU emulation slightly faster, but if the setting
is too high, you will run out of memory while compiling.
Best to leave this as it is.
 */
#define CPU_EMU_SIZE 0

/*
CPU level: 0 = 68000, 1 = 68010, 2 = sort of a 68020
If configured for 68020, the emulator will be a little slower.
Don't touch this: Only 0 will work for now.
 */
#define CPU_LEVEL 0

/*
Trace all memory accesses.
 */
#define DO_TRACE 0

/*
Count all memory accesses; per size and per r/w
 */
#define MEMCNT 0

/*
Emulate address errors: access word,longword at odd address
 */
#define EMUL_EXCEPTION_3 1

/*
Array bounds are checked to guard against pointers out of bounds
MEMCHK_PRINT 0: no checks; 1 mask violations; 2 mask and print violations
 */
#define MEMCHK_PRINT 1
