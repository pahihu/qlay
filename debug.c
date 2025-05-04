/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	Debugger
*/

#include "sysconfig.h"
#include "sysdeps.h"

#include <ctype.h>
#include <signal.h>

#include "options.h"
#include "debug.h"
#include "qlmem.h"
#include "exe68k.h"

static int quit_program;

#ifdef __cplusplus
static RETSIGTYPE sigbrkhandler(...)
#else
static RETSIGTYPE sigbrkhandler(int foo)
#endif
{
    broken_in = 1;
    specialflags |= SPCFLAG_BRK;
    signal(SIGINT, sigbrkhandler);
}

static void ignore_ws(char **c)
{
    while (**c && isspace(**c)) (*c)++;
}

static U32 readhex(char **c)
{
    U32 val = 0;
    char nc;

    ignore_ws(c);

    while (isxdigit(nc = **c)){
	(*c)++;
	val *= 16;
	nc = toupper(nc);
	if (isdigit(nc)) {
	    val += nc - '0';
	} else {
	    val += nc - 'A' + 10;
	}
    }
    return val;
}

static char next_char(char **c)
{
    ignore_ws(c);
    return *(*c)++;
}

static int more_params(char **c)
{
    ignore_ws(c);
    return (**c) != 0;
}

static void dumpmem(A32 addr, A32 *nxmem, int lines)
{
    broken_in = 0;
    for (;lines-- && !broken_in;){
	int i;
	printf("%08lx ", addr);
	for(i=0; i< 8; i++) {
	    printf("%04x ", get_word(addr)); addr += 2;
	}
	printf("\n");
/* tst
addr-=16;
	printf("%08lx ", addr);
	for(i=0; i< 4; i++) {
	    printf("%08x ", get_long(addr)); addr += 4;
	}
	printf("\n");
*/
    }
    *nxmem = addr;
}

void init_debug(void)
{
	quit_program=0;
}

#define debuggable() (1)

void debug(void)
{
    char input[80],c;
    A32 nextpc,nxdis,nxmem;

    if (debuggable()) {
#if defined(__unix) && !defined(__NeXT__)
	struct sigaction sa;
	sa.sa_handler = sigbrkhandler;
#ifdef SA_RESTART
	sa.sa_flags = SA_RESTART;
#endif
	sigemptyset(&sa.sa_mask);
    	sigaction(SIGINT, &sa, NULL);
#else
	signal(SIGINT, sigbrkhandler);
#endif
    }
    if (!debuggable() || !use_debugger) {
	MC68000_run();
	if (!debuggable()) {
	    return;
	}
	return;
    }

    if (quit_program)
	return;

    printf("debugging...\n");
    MC68000_dumpstate(&nextpc);
    nxdis = nextpc; nxmem = 0;
    for(;;){
	char cmd,*inptr;

	if (quit_program)
	    return;

	printf(">");
	fgets(input, 80, stdin);
	inptr = input;
	cmd = next_char(&inptr);
	switch(cmd){
	 case 'q':
	    quit_program=1;
	    return;
	 case 'c':
	    break;
	 case 'r':
	    MC68000_dumpstate(&nextpc);
	    break;
	 case 'R':
	    MC68000_reset();
	    break;
	 case 'd':
	    {
		U32 daddr;
		int count;

		if (more_params(&inptr))
		    daddr = readhex(&inptr);
		else
		    daddr = nxdis;
		if (more_params(&inptr))
		    count = readhex(&inptr);
		else
		    count = 10;
		MC68000_disasm(daddr, &nxdis, count);
	    }
	    break;
	 case 't':
	    MC68000_step();
	    MC68000_dumpstate(&nextpc);
	    break;
	 case 'z':
	    MC68000_skip(nextpc);
	    MC68000_dumpstate(&nextpc);
	    nxdis = nextpc;
	    break;
	 case 'f':
	    MC68000_skip(readhex(&inptr));
	    MC68000_dumpstate(&nextpc);
	    break;
	 case 'g':
	    if (more_params (&inptr))
		m68k_setpc (readhex (&inptr));
	    MC68000_run();
	    MC68000_dumpstate(&nextpc);
	    break;
	 case 'p':
	    m68k_setpc (readhex (&inptr));
	    MC68000_dumpstate(&nextpc);
	    break;
	 case 'm':
	    {
		U32 maddr; int lines;
		if (more_params(&inptr))
		    maddr = readhex(&inptr);
		else
		    maddr = nxmem;
		if (more_params(&inptr))
		    lines = readhex(&inptr);
		else
		    lines = 4;
		dumpmem(maddr, &nxmem, lines);
	    }
	    break;
	 case 's':
	    {
		U32 maddr; int sbyte;
		if (more_params(&inptr))
		    sbyte = readhex(&inptr);
		else sbyte=0;
		if (more_params(&inptr))
		    maddr = readhex(&inptr);
		else
		    maddr = nxmem;
		put_byte(maddr, sbyte);
		nxmem=maddr+1;
	    }
	    break;
          case 'h':
          case '?':
	    {
             printf ("          HELP for Debugger\n");
             printf ("         -----------------------\n\n");
             printf ("  g: <address>          ");
                printf("Start execution at the current address or <address>.\n");
             printf ("  p: <address>          ");
                printf("Set PC to <address>.\n");
             printf ("  r:                    ");
                printf("Dump state of the CPU\n");
             printf ("  R:                    ");
                printf("Reset the CPU\n");
             printf ("  m <address> <lines>:  ");
                printf ("Memory dump starting at <address>\n");
             printf ("  d <address> <lines>:  ");
                printf ("Disassembly starting at <address>\n");
             printf ("  s <byte> <address>:   ");
                printf ("Set <byte> at <address>\n");
             printf ("  t:                    ");
                printf ("Step one instruction\n");
             printf ("  z:                    ");
                printf ("Step through one instruction - useful for JSR, DBRA etc\n");
             printf ("  f <address>:          ");
                printf ("Step forward until PC == <address>\n");
             printf ("  h,?:                  ");
                printf ("Show this help page\n");
             printf ("  q:                    ");
                printf ("Quit the emulator\n\n");
            }
            break;

	}
    }
}
