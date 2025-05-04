/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998

	MC68000 emulation generator
	Copyright 1995 Bernd Schmidt
*/

/*
This is a fairly stupid program that generates a lot of case labels that
can be #included in a switch statement.
As an alternative, it can generate functions that handle specific
MC68000 instructions, plus a prototype header file and a function pointer
array to look up the function for an opcode.
Error checking is bad, an illegal table68k file will cause the program to
call abort().
The generated code is sometimes sub-optimal, an optimizing compiler should
take care of this.
*/

#include <ctype.h>

#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "readcpu.h"

#define BOOL_TYPE "int"

long int counts[65536];

static int isspecific(int opcode)
{
    return counts[opcode]>5;
}

static void read_counts(void)
{
    FILE *file;
    unsigned long opcode,count, total;
    int trapcount=0;
    int trap=0;
    memset(counts, 0, sizeof counts);

    file=fopen("insncount","r");
    if(file)
    {
	fscanf(file,"Total: %d",&total);
	while(fscanf(file,"%x: %d\n",&opcode,&count)==2)
	{
	    counts[opcode]=10000.0*count/total;
	    if(isspecific(opcode))
	    {
		trapcount+=count;
		trap++;
	    }
	}
#if 0
	fprintf(stderr,"trap %d function: %f%\n",trap,100.0*trapcount/total);
#endif
    }
}


static int n_braces = 0;

static void start_brace(void)
{
    n_braces++;
    printf("{");
}

static void close_brace(void)
{
    assert (n_braces > 0);
    n_braces--;
    printf("}");
}

static void finish_braces(void)
{
    while (n_braces > 0)
	close_brace();
}

static void pop_braces(int to)
{
    while (n_braces > to)
	close_brace();
}

static void genamode(amodes mode, char *reg, wordsizes size, char *name, int getv, int movem)
{
    start_brace ();
    switch(mode) {
     case Dreg:
	if (movem)
	    abort();
	if (getv)
	    switch(size) {
	     case sz_byte:
		printf("\tS8 %s = regs.d[%s];\n", name, reg);
		break;
	     case sz_word:
		printf("\tS16 %s = regs.d[%s];\n", name, reg);
		break;
	     case sz_long:
		printf("\tS32 %s = regs.d[%s];\n", name, reg);
		break;
	     default: abort();
	    }
	break;
     case Areg:
	if (movem)
	    abort();
	if (getv)
	    switch(size) {
	     case sz_word:
		printf("\tS16 %s = regs.a[%s];\n", name, reg);
		break;
	     case sz_long:
		printf("\tS32 %s = regs.a[%s];\n", name, reg);
		break;
	     default: abort();
	    }
	break;
     case Aind:
	printf("\tA32 %sa = regs.a[%s];\n", name, reg);
	if (getv)
	    switch(size) {
	     case sz_byte:
		printf("\tS8 %s = get_byte(%sa);\n", name, name);
		break;
	     case sz_word:
		printf("\tS16 %s = get_word(%sa);\n", name, name);
		break;
	     case sz_long:
		printf("\tS32 %s = get_long(%sa);\n", name, name);
		break;
	     default: abort();
	    }
	break;
     case Aipi:
	printf("\tA32 %sa = regs.a[%s];\n", name, reg);
	switch(size) {
	 case sz_byte:
	    if (getv) printf("\tS8 %s = get_byte(%sa);\n", name, name);
	    if (!movem) {
		start_brace();
		printf("\tregs.a[%s] += areg_byteinc[%s];\n", reg, reg);
	    }
	    break;
	 case sz_word:
	    if (getv) printf("\tS16 %s = get_word(%sa);\n", name, name);
	    if (!movem) {
		start_brace();
		printf("\tregs.a[%s] += 2;\n", reg);
	    }
	    break;
	 case sz_long:
	    if (getv) printf("\tS32 %s = get_long(%sa);\n", name, name);
	    if (!movem) {
		start_brace();
		printf("\tregs.a[%s] += 4;\n", reg);
	    }
	    break;
	 default: abort();
	}
	break;
     case Apdi:
	switch(size) {
	 case sz_byte:
	    if (!movem) printf("\tregs.a[%s] -= areg_byteinc[%s];\n", reg, reg);
	    start_brace();
	    printf("\tA32 %sa = regs.a[%s];\n", name, reg);
	    if (getv) printf("\tS8 %s = get_byte(%sa);\n", name, name);
	    break;
	 case sz_word:
	    if (!movem) printf("\tregs.a[%s] -= 2;\n", reg);
	    start_brace();
	    printf("\tA32 %sa = regs.a[%s];\n", name, reg);
	    if (getv) printf("\tS16 %s = get_word(%sa);\n", name, name);
	    break;
	 case sz_long:
	    if (!movem) printf("\tregs.a[%s] -= 4;\n", reg);
	    start_brace();
	    printf("\tA32 %sa = regs.a[%s];\n", name, reg);
	    if (getv) printf("\tS32 %s = get_long(%sa);\n", name, name);
	    break;
	 default: abort();
	}
	break;
     case Ad16:
	printf("\tA32 %sa = regs.a[%s] + (S32)(S16)nextiword();\n", name, reg);
	if (getv)
	    switch(size) {
	     case sz_byte:
		printf("\tS8 %s = get_byte(%sa);\n", name, name);
		break;
	     case sz_word:
		printf("\tS16 %s = get_word(%sa);\n", name, name);
		break;
	     case sz_long:
		printf("\tS32 %s = get_long(%sa);\n", name, name);
		break;
	     default: abort();
	    }
	break;
     case Ad8r:
	printf("\tA32 %sa = regs.a[%s];\n", name, reg);
#if 0
	printf("\tU16 %sdp = nextiword();\n", name);
	printf("\t%sa += (S32)(S8)(%sdp & 0xFF);\n", name, name);
	start_brace();
	printf("\tU32 %sdpr = %sdp & 0x8000 ? regs.a[(%sdp & 0x7000) >> 12] : regs.d[(%sdp & 0x7000) >> 12];\n", name, name, name, name);
	printf("\tif (!(%sdp & 0x800)) %sdpr = (S32)(S16)%sdpr;\n", name, name, name);
	printf("\t%sa += %sdpr;\n", name, name);
#endif
	printf("\t%sa = get_disp_ea (%sa, nextiword());\n", name, name);
	if (getv) {
	    start_brace();
	    switch(size) {
	     case sz_byte:
		printf("\tS8 %s = get_byte(%sa);\n", name, name);
		break;
	     case sz_word:
		printf("\tS16 %s = get_word(%sa);\n", name, name);
		break;
	     case sz_long:
		printf("\tS32 %s = get_long(%sa);\n", name, name);
		break;
	     default: abort();
	    }
	}
	break;
     case PC16:
	printf("\tA32 %sa = m68k_getpc();\n", name);
	printf("\t%sa += (S32)(S16)nextiword();\n", name);
	if (getv) {
	    start_brace();
	    switch(size) {
	     case sz_byte:
		printf("\tS8 %s = get_byte(%sa);\n", name, name);
		break;
	     case sz_word:
		printf("\tS16 %s = get_word(%sa);\n", name, name);
		break;
	     case sz_long:
		printf("\tS32 %s = get_long(%sa);\n", name, name);
		break;
	     default: abort();
	    }
	}
	break;
     case PC8r:
	printf("\tA32 %sa = m68k_getpc();\n", name);
#if 0
	printf("\tU16 %sdp = nextiword();\n", name);
	printf("\t%sa += (S32)(S8)(%sdp & 0xFF);\n", name, name);
	start_brace();
	printf("\tU32 %sdpr = %sdp & 0x8000 ? regs.a[(%sdp & 0x7000) >> 12] : regs.d[(%sdp & 0x7000) >> 12];\n", name, name, name, name);
	printf("\tif (!(%sdp & 0x800)) %sdpr = (S32)(S16)%sdpr;\n", name, name, name);
	printf("\t%sa += %sdpr;\n", name, name);
#endif
	printf("\t%sa = get_disp_ea (%sa, nextiword());\n", name, name);
	if (getv) {
	    start_brace();
	    switch(size) {
	     case sz_byte:
		printf("\tS8 %s = get_byte(%sa);\n", name, name);
		break;
	     case sz_word:
		printf("\tS16 %s = get_word(%sa);\n", name, name);
		break;
	     case sz_long:
		printf("\tS32 %s = get_long(%sa);\n", name, name);
		break;
	     default: abort();
	    }
	}
	break;
     case absw:
	printf("\tA32 %sa = (S32)(S16)nextiword();\n", name);
	if (getv)
	    switch(size) {
	     case sz_byte:
		printf("\tS8 %s = get_byte(%sa);\n", name, name);
		break;
	     case sz_word:
		printf("\tS16 %s = get_word(%sa);\n", name, name);
		break;
	     case sz_long:
		printf("\tS32 %s = get_long(%sa);\n", name, name);
		break;
	     default: abort();
	    }
	break;
     case absl:
	printf("\tA32 %sa = nextilong();\n", name);
	if (getv)
	    switch(size) {
	     case sz_byte:
		printf("\tS8 %s = get_byte(%sa);\n", name, name);
		break;
	     case sz_word:
		printf("\tS16 %s = get_word(%sa);\n", name, name);
		break;
	     case sz_long:
		printf("\tS32 %s = get_long(%sa);\n", name, name);
		break;
	     default: abort();
	    }
	break;
     case imm:
	if (getv)
	    switch(size) {
	     case sz_byte:
		printf("\tS8 %s = nextiword();\n", name);
		break;
	     case sz_word:
		printf("\tS16 %s = nextiword();\n", name);
		break;
	     case sz_long:
		printf("\tS32 %s = nextilong();\n", name);
		break;
	     default: abort();
	    }
	break;
     case imm0:
	if (!getv) abort();
	printf("\tS8 %s = nextiword();\n", name);
	break;
     case imm1:
	if (!getv) abort();
	printf("\tS16 %s = nextiword();\n", name);
	break;
     case imm2:
	if (!getv) abort();
        printf("\tS32 %s = nextilong();\n", name);
	break;
     case immi:
	if (!getv) abort();
	printf("\tU32 %s = %s;\n", name, reg);
	break;
     default:
	abort();
    }
}

static void genastore(char *from, amodes mode, char *reg, wordsizes size, char *to)
{
    switch(mode) {
     case Dreg:
	switch(size) {
	 case sz_byte:
	    printf("\tregs.d[%s] &= ~0xff; regs.d[%s] |= (%s) & 0xff;\n", reg, reg, from);
	    break;
	 case sz_word:
	    printf("\tregs.d[%s] &= ~0xffff; regs.d[%s] |= (%s) & 0xffff;\n", reg, reg, from);
	    break;
	 case sz_long:
	    printf("\tregs.d[%s] = (%s);\n", reg, from);
	    break;
	 default: abort();
	}
	break;
     case Areg:
	switch(size) {
	 case sz_word:
	    printf("\tregs.a[%s] = (S32)(S16)(%s);\n", reg, from);
	    break;
	 case sz_long:
	    printf("\tregs.a[%s] = (%s);\n", reg, from);
	    break;
	 default: abort();
	}
	break;
     case Aind:
     case Aipi:
     case Apdi:
     case Ad16:
     case Ad8r:
     case absw:
     case absl:
	switch(size) {
	 case sz_byte:
	    printf("\tput_byte(%sa,%s);\n", to, from);
	    break;
	 case sz_word:
	    printf("\tput_word(%sa,%s);\n", to, from);
	    break;
	 case sz_long:
	    printf("\tput_long(%sa,%s);\n", to, from);
	    break;
	 default: abort();
	}
	break;
     case PC16:
     case PC8r:
	switch(size) {
	 case sz_byte:
	    printf("\tput_byte(%sa,%s);\n", to, from);
	    break;
	 case sz_word:
	    if (CPU_LEVEL < 2)
		abort();
	    printf("\tput_word(%sa,%s);\n", to, from);
	    break;
	 case sz_long:
	    if (CPU_LEVEL < 2)
		abort();
	    printf("\tput_long(%sa,%s);\n", to, from);
	    break;
	 default: abort();
	}
	break;
     case imm:
     case imm0:
     case imm1:
     case imm2:
     case immi:
	abort();
	break;
     default:
	abort();
    }
}

static void genmovemel(U16 opcode)
{
    char getcode[100];
    int size = table68k[opcode].size == sz_long ? 4 : 2;

    if (table68k[opcode].size == sz_long) {
    	strcpy(getcode, "get_long(srca)");
    } else {
    	strcpy(getcode, "(S32)(S16)get_word(srca)");
    }

    printf("\tU16 mask = nextiword(), bitmask = mask;\n");
    genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 0, 1);
    start_brace();
    printf("\tint i, bitcnt = 0;\n");
    printf("\tfor(i=0;i<16;i++) { bitcnt += bitmask & 1; bitmask >>= 1; }\n");

    printf("\tfor(i=0;i<8;i++) { if (mask & 1) { regs.d[i] = %s; srca += %d; } mask >>= 1; }\n", getcode, size);
    printf("\tfor(i=0;i<8;i++) { if (mask & 1) { regs.a[i] = %s; srca += %d; } mask >>= 1; }\n", getcode, size);

    if (table68k[opcode].smode == Aipi)
    	printf("\tregs.a[srcreg] = srca;\n");
}

static void genmovemle(U16 opcode)
{
    char putcode[100], shiftcode[] = ">>";
    int size = table68k[opcode].size == sz_long ? 4 : 2;
    int mask = 1;
    if (table68k[opcode].size == sz_long) {
    	strcpy(putcode, "put_long(srca,");
    } else {
    	strcpy(putcode, "put_word(srca,");
    }

    printf("\tU16 mask = nextiword(), bitmask = mask;\n");
    genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 0, 1);
    start_brace();
    printf("\tint i, bitcnt = 0;\n");
    printf("\tU32 rd[8], ra[8];\n");
    printf("\tfor(i=0;i<16;i++) { bitcnt += bitmask & 1; bitmask >>= 1; }\n");
    printf("\tfor(i=0;i<8;i++) { rd[i] = regs.d[i]; ra[i] = regs.a[i]; }\n");
    if (table68k[opcode].smode == Apdi) {
	printf("\tsrca -= %d*bitcnt;\n", size);
	printf("\tregs.a[srcreg] = srca;\n");
	strcpy(shiftcode, "<<");
	mask = 0x8000;
    }

    printf("\tfor(i=0;i<8;i++) { if (mask & %d) { %s rd[i]); srca += %d; } mask %s= 1; }\n",
	   mask, putcode, size, shiftcode);
    printf("\tfor(i=0;i<8;i++) { if (mask & %d) { %s ra[i]); srca += %d; } mask %s= 1; }\n",
	   mask, putcode, size, shiftcode);
}

typedef enum {
    flag_logical, flag_add, flag_sub, flag_cmp, flag_addx, flag_subx, flag_zn,
    flag_av, flag_sv
} flagtypes;

static void genflags_normal(flagtypes type, wordsizes size, char *value, char *src, char *dst)
{
    char vstr[100],sstr[100],dstr[100];
    char usstr[100],udstr[100];
    char unsstr[100],undstr[100];

    switch(size) {
     case sz_byte:
	strcpy(vstr, "((S8)(");
	strcpy(usstr, "((U8)(");
	break;
     case sz_word:
	strcpy(vstr, "((S16)(");
	strcpy(usstr, "((U16)(");
	break;
     case sz_long:
	strcpy(vstr, "((S32)(");
	strcpy(usstr, "((U32)(");
	break;
     default:
	abort();
    }
    strcpy(unsstr, usstr);

    strcpy(sstr, vstr);
    strcpy(dstr, vstr);
    strcat(vstr, value); strcat(vstr,"))");
    strcat(dstr, dst); strcat(dstr,"))");
    strcat(sstr, src); strcat(sstr,"))");

    strcpy(udstr, usstr);
    strcat(udstr, dst); strcat(udstr,"))");
    strcat(usstr, src); strcat(usstr,"))");

    strcpy(undstr, unsstr);
    strcat(unsstr, "-");
    strcat(undstr, "~");
    strcat(undstr, dst); strcat(undstr,"))");
    strcat(unsstr, src); strcat(unsstr,"))");

    switch (type) {
     case flag_logical:
     case flag_zn:
     case flag_av:
     case flag_sv:
     case flag_addx:
     case flag_subx:
	break;

     case flag_add:
	start_brace();
	printf("U32 %s = %s + %s;\n", value, dstr, sstr);
	break;
     case flag_sub:
     case flag_cmp:
	start_brace();
	printf("U32 %s = %s - %s;\n", value, dstr, sstr);
	break;
    }


    switch (type) {
     case flag_logical:
     case flag_zn:
	break;

     case flag_add:
     case flag_sub:
     case flag_addx:
     case flag_subx:
     case flag_cmp:
     case flag_av:
     case flag_sv:
	start_brace();
	printf("\t"BOOL_TYPE" flgs = %s < 0;\n", sstr);
	printf("\t"BOOL_TYPE" flgo = %s < 0;\n", dstr);
	printf("\t"BOOL_TYPE" flgn = %s < 0;\n", vstr);
	break;
    }

    switch(type) {
     case flag_logical:
	printf("\tVFLG = CFLG = 0;\n");
	printf("\tZFLG = %s == 0;\n", vstr);
	printf("\tNFLG = %s < 0;\n", vstr);
	break;
     case flag_av:
	printf("\tVFLG = (flgs == flgo) && (flgn != flgo);\n");
	break;
     case flag_sv:
	printf("\tVFLG = (flgs != flgo) && (flgn != flgo);\n");
	break;
     case flag_zn:
	printf("\tif (%s != 0) ZFLG = 0;\n", vstr);
	printf("\tNFLG = %s < 0;\n", vstr);
	break;
     case flag_add:
	printf("\tZFLG = %s == 0;\n", vstr);
	printf("\tVFLG = (flgs == flgo) && (flgn != flgo);\n");
	printf("\tCFLG = regs.x = %s < %s;\n", undstr, usstr);
	printf("\tNFLG = flgn != 0;\n");
	break;
     case flag_sub:
	printf("\tZFLG = %s == 0;\n", vstr);
	printf("\tVFLG = (flgs != flgo) && (flgn != flgo);\n");
	printf("\tCFLG = regs.x = %s > %s;\n", usstr, udstr);
	printf("\tNFLG = flgn != 0;\n");
	break;
     case flag_addx:
	printf("\tVFLG = (flgs && flgo && !flgn) || (!flgs && !flgo && flgn);\n");
	printf("\tregs.x = CFLG = (flgs && flgo) || (!flgn && (flgo || flgs));\n");
	break;
     case flag_subx:
	printf("\tVFLG = (!flgs && flgo && !flgn) || (flgs && !flgo && flgn);\n");
	printf("\tregs.x = CFLG = (flgs && !flgo) || (flgn && (!flgo || flgs));\n");
	break;
     case flag_cmp:
	printf("\tZFLG = %s == 0;\n", vstr);
	printf("\tVFLG = (flgs != flgo) && (flgn != flgo);\n");
	printf("\tCFLG = %s > %s;\n", usstr, udstr);
	printf("\tNFLG = flgn != 0;\n");
	break;
    }
}

static void genflags(flagtypes type, wordsizes size, char *value, char *src, char *dst)
{
#ifdef INTEL_FLAG_OPT

    switch (type) {
     case flag_logical:
     case flag_av:
     case flag_sv:
     case flag_zn:
     case flag_addx:
     case flag_subx:
	break;

     case flag_add:
     case flag_sub:
     case flag_cmp:
	start_brace();
	printf("\tU32 %s;\n", value);
	break;
    }

    switch(type) {
     case flag_av:
     case flag_sv:
     case flag_zn:
     case flag_addx:
     case flag_subx:
	break;

     case flag_logical:
	if (strcmp(value, "0") == 0) {
	    /* GCC doesn't want to load the constant into a register.
	     * intel_flag_lookup[64] is just the zero flag set. */
	    printf("\tregflags = intel_flag_lookup[64];\n");
	} else {
	    switch(size) {
	     case sz_byte:
		printf("\t__asm__(\"testb %%0,%%0; lahf; movzbl %%%%ah,%%%%eax; movl intel_flag_lookup(,%%%%eax,4),%%%%eax; movl %%%%eax,regflags\""
		       ": : \"r\" (%s) : \"%%eax\", \"cc\");", value);
		break;
	     case sz_word:
		printf("\t__asm__(\"testw %%0,%%0; lahf; movzbl %%%%ah,%%%%eax; movl intel_flag_lookup(,%%%%eax,4),%%%%eax; movl %%%%eax,regflags\""
		       ": : \"r\" (%s) : \"%%eax\", \"cc\");", value);
		break;
	     case sz_long:
		printf("\t__asm__(\"testl %%0,%%0; lahf; movzbl %%%%ah,%%%%eax; movl intel_flag_lookup(,%%%%eax,4),%%%%eax; movl %%%%eax,regflags\""
		       ": : \"r\" (%s) : \"%%eax\", \"cc\");", value);

		break;
	     case sz_unknown:
		abort();
	    }
	}
	return;

     case flag_add:
	/* Of course it would be better to let GCC find a proper register to
	 * compute newv in, but it does not seem to like reloading the 8 bit
	 * regs. */
	switch (size) {
	 case sz_byte:
	    printf("\t__asm__(\"addb %%b3,%%b0; lahf; movzbl %%%%ah,%%%%eax;"
		   " movl intel_flag_lookup(,%%%%eax,4),%%%%eax; seto %%%%al; movb %%%%ah,%%1; movl %%%%eax,regflags \""
		   ": \"=&q\" (%s), \"=m\" (regs.x) "
		   ": \"0\" ((S8)(%s)), \"qm\" ((S8)(%s)) : \"cc\", \"%%eax\");",
		   value, src, dst);
	    break;
	 case sz_word:
	    printf("\t__asm__(\"addw %%w3,%%w0; lahf; movzbl %%%%ah,%%%%eax;"
		   " movl intel_flag_lookup(,%%%%eax,4),%%%%eax; seto %%%%al; movb %%%%ah,%%1; movl %%%%eax,regflags \""
		   ": \"=&r\" (%s), \"=m\" (regs.x) "
		   ": \"0\" ((S16)(%s)), \"rmi\" ((S16)(%s)) : \"cc\", \"%%eax\");",
		   value, src, dst);
	    break;
	 case sz_long:
	    printf("\t__asm__(\"addl %%3,%%0; lahf; movzbl %%%%ah,%%%%eax;"
		   " movl intel_flag_lookup(,%%%%eax,4),%%%%eax; seto %%%%al; movb %%%%ah,%%1; movl %%%%eax,regflags \""
		   ": \"=&r\" (%s), \"=m\" (regs.x) "
		   ": \"0\" ((S32)(%s)), \"rmi\" ((S32)(%s)) : \"cc\", \"%%eax\");",
		   value, src, dst);
	    break;
	 case sz_unknown:
	    abort();
	}
	return;

     case flag_sub:
	switch (size) {
	 case sz_byte:
	    printf("\t__asm__(\"subb %%b2,%%b0; lahf; movzbl %%%%ah,%%%%eax;"
		   " movl intel_flag_lookup(,%%%%eax,4),%%%%eax; seto %%%%al; movb %%%%ah,%%1; movl %%%%eax,regflags \""
		   ": \"=&q\" (%s), \"=m\" (regs.x) "
		   ": \"qmi\" ((S8)(%s)), \"0\" ((S8)(%s)) : \"cc\", \"%%eax\");",
		   value, src, dst);
	    break;
	 case sz_word:
	    printf("\t__asm__(\"subw %%w2,%%w0; lahf; movzbl %%%%ah,%%%%eax;"
		   " movl intel_flag_lookup(,%%%%eax,4),%%%%eax; seto %%%%al; movb %%%%ah,%%1; movl %%%%eax,regflags \""
		   ": \"=&r\" (%s), \"=m\" (regs.x) "
		   ": \"rmi\" ((S16)(%s)), \"0\" ((S16)(%s)) : \"cc\", \"%%eax\");",
		   value, src, dst);
	    break;
	 case sz_long:
	    printf("\t__asm__(\"subl %%2,%%0; lahf; movzbl %%%%ah,%%%%eax;"
		   " movl intel_flag_lookup(,%%%%eax,4),%%%%eax; seto %%%%al; movb %%%%ah,%%1; movl %%%%eax,regflags \""
		   ": \"=&r\" (%s), \"=m\" (regs.x) "
		   ": \"rmi\" ((S32)(%s)), \"0\" ((S32)(%s)) : \"cc\", \"%%eax\");",
		   value, src, dst);
	    break;
	 case sz_unknown:
	    abort();
	}
	return;

     case flag_cmp:
	switch (size) {
	 case sz_byte:
	    printf("\t__asm__(\"movb %%3,%%%%cl; subb %%2,%%%%cl; lahf; movzbl %%%%ah,%%%%eax;"
		   " movl intel_flag_lookup(,%%%%eax,4),%%%%eax; seto %%%%al \""
		   ": \"=a\" (regflags.longflags), \"=&c\" (%s) "
		   ": \"rm\" ((S8)(%s)), \"rmi\" ((S8)(%s)) : \"cc\");",
		   value, src, dst);
	    break;
	 case sz_word:
	    printf("\t__asm__(\"movw %%3,%%%%cx; subw %%2,%%%%cx; lahf; movzbl %%%%ah,%%%%eax;"
		   " movl intel_flag_lookup(,%%%%eax,4),%%%%eax; seto %%%%al \""
		   ": \"=a\" (regflags.longflags), \"=&c\" (%s) "
		   ": \"rm\" ((S16)(%s)), \"rmi\" ((S16)(%s)) : \"cc\");",
		   value, src, dst);
	    break;
	 case sz_long:
	    printf("\t__asm__(\"movl %%3,%%%%ecx; subl %%2,%%%%ecx; lahf; movzbl %%%%ah,%%%%eax;"
		   " movl intel_flag_lookup(,%%%%eax,4),%%%%eax; seto %%%%al \""
		   ": \"=a\" (regflags.longflags), \"=&c\" (%s) "
		   ": \"rm\" ((S32)(%s)), \"rmi\" ((S32)(%s)) : \"cc\");",
		   value, src, dst);
	    break;
	 case sz_unknown:
	    abort();
	}
	return;
    }
#endif
    genflags_normal(type, size, value, src, dst);
}
static void gen_opcode(unsigned long int opcode)
{
    start_brace ();
    switch (table68k[opcode].plev) {
     case 0: /* not priviledged */
	break;
     case 1: /* unpriviledged only on 68000 */
	if (CPU_LEVEL == 0)
	    break;
     case 2: /* priviledged */
	printf("if (!regs.s) { regs.pc_p--; Exception(8,0); } else\n");
	start_brace();
	break;
     case 3: /* priviledged if size == word */
	if (table68k[opcode].size == sz_byte)
	    break;
	printf("if (!regs.s) { regs.pc_p--; Exception(8,0); } else\n");
	start_brace();
	break;
    }
    switch(table68k[opcode].mnemo) {
     case i_OR:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 1, 0);
	printf("\tsrc |= dst;\n");
	genflags(flag_logical, table68k[opcode].size, "src", "", "");
	genastore("src", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst");
	break;
     case i_AND:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 1, 0);
	printf("\tsrc &= dst;\n");
	genflags(flag_logical, table68k[opcode].size, "src", "", "");
	genastore("src", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst");
	break;
     case i_EOR:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 1, 0);
	printf("\tsrc ^= dst;\n");
	genflags(flag_logical, table68k[opcode].size, "src", "", "");
	genastore("src", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst");
	break;
     case i_ORSR:
	printf("\tMakeSR();\n");
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	if (table68k[opcode].size == sz_byte) {
	    printf("\tsrc &= 0xFF;\n");
	}
	printf("\tregs.sr |= src;\n");
	printf("\tMakeFromSR();\n");
	break;
     case i_ANDSR:
	printf("\tMakeSR();\n");
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	if (table68k[opcode].size == sz_byte) {
	    printf("\tsrc |= 0xFF00;\n");
	}
	printf("\tregs.sr &= src;\n");
	printf("\tMakeFromSR();\n");
	break;
     case i_EORSR:
	printf("\tMakeSR();\n");
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	if (table68k[opcode].size == sz_byte) {
	    printf("\tsrc &= 0xFF;\n");
	}
	printf("\tregs.sr ^= src;\n");
	printf("\tMakeFromSR();\n");
	break;
     case i_SUB:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 1, 0);
	start_brace ();
	genflags(flag_sub, table68k[opcode].size, "newv", "src", "dst");
	genastore("newv", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst");
	break;
     case i_SUBA:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", sz_long, "dst", 1, 0);
	start_brace ();
	printf("\tU32 newv = dst - src;\n");
	genastore("newv", table68k[opcode].dmode, "dstreg", sz_long, "dst");
	break;
     case i_SUBX:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 1, 0);
	start_brace ();
	printf("\tU32 newv = dst - src - (regs.x ? 1 : 0);\n");
	genflags(flag_subx, table68k[opcode].size, "newv", "src", "dst");
	genflags(flag_zn, table68k[opcode].size, "newv", "", "");
	genastore("newv", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst");
	break;
     case i_SBCD:
	/* Let's hope this works... */
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 1, 0);
	start_brace ();
	printf("\tU16 newv_lo = (dst & 0xF) - (src & 0xF) - regs.x;\n");
	printf("\tU16 newv_hi = (dst & 0xF0) - (src & 0xF0);\n");
	printf("\tU16 newv;\n");
	printf("\tif (newv_lo > 9) { newv_lo-=6; newv_hi-=0x10; }\n");
	printf("\tnewv = newv_hi + (newv_lo & 0xF);");
	printf("\tCFLG = regs.x = (newv_hi & 0x1F0) > 0x90;\n");
	printf("\tif (CFLG) newv -= 0x60;\n");
	genflags(flag_zn, table68k[opcode].size, "newv", "", "");
	genflags(flag_sv, table68k[opcode].size, "newv", "src", "dst");
	genastore("newv", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst");
	break;
     case i_ADD:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 1, 0);
	start_brace ();
	genflags(flag_add, table68k[opcode].size, "newv", "src", "dst");
	genastore("newv", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst");
	break;
     case i_ADDA:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", sz_long, "dst", 1, 0);
	start_brace ();
	printf("\tU32 newv = dst + src;\n");
	genastore("newv", table68k[opcode].dmode, "dstreg", sz_long, "dst");
	break;
     case i_ADDX:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 1, 0);
	start_brace ();
	printf("\tU32 newv = dst + src + (regs.x ? 1 : 0);\n");
	genflags(flag_addx, table68k[opcode].size, "newv", "src", "dst");
	genflags(flag_zn, table68k[opcode].size, "newv", "", "");
	genastore("newv", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst");
	break;
     case i_ABCD:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 1, 0);
	start_brace ();
	printf("\tU16 newv_lo = (src & 0xF) + (dst & 0xF) + regs.x;\n");
	printf("\tU16 newv_hi = (src & 0xF0) + (dst & 0xF0);\n");
	printf("\tU16 newv;\n");
	printf("\tif (newv_lo > 9) { newv_lo +=6; }\n");
	printf("\tnewv = newv_hi + newv_lo;");
	printf("\tCFLG = regs.x = (newv & 0x1F0) > 0x90;\n");
	printf("\tif (CFLG) newv += 0x60;\n");
	genflags(flag_zn, table68k[opcode].size, "newv", "", "");
	genflags(flag_sv, table68k[opcode].size, "newv", "src", "dst");
	genastore("newv", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst");
	break;
     case i_NEG:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	start_brace ();
	genflags(flag_sub, table68k[opcode].size, "dst", "src", "0");
	genastore("dst",table68k[opcode].smode, "srcreg", table68k[opcode].size, "src");
	break;
     case i_NEGX:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	start_brace ();
	printf("\tsrc += regs.x;\n");
	genflags(flag_sub, table68k[opcode].size, "dst", "src", "0");
	genastore("dst",table68k[opcode].smode, "srcreg", table68k[opcode].size, "src");
	break;
     case i_NBCD:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	start_brace ();
	printf("\tU16 newv_lo = - (src & 0xF) - regs.x;\n");
	printf("\tU16 newv_hi = - (src & 0xF0);\n");
	printf("\tU16 newv;\n");
	printf("\tif (newv_lo > 9) { newv_lo-=6; newv_hi-=0x10; }\n");
	printf("\tnewv = newv_hi + (newv_lo & 0xF);");
	printf("\tCFLG = regs.x = (newv_hi & 0x1F0) > 0x90;\n");
	printf("\tif (CFLG) newv -= 0x60;\n");
	printf("\tif (newv != 0) ZFLG = 0;\n");
	genastore("newv", table68k[opcode].smode, "srcreg", table68k[opcode].size, "src");
	break;
     case i_CLR:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 0, 0);
	genflags(flag_logical, table68k[opcode].size, "0", "", "");
	genastore("0",table68k[opcode].smode, "srcreg", table68k[opcode].size, "src");
	break;
     case i_NOT:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	start_brace ();
	printf("\tU32 dst = ~src;\n");
	genflags(flag_logical, table68k[opcode].size, "dst", "", "");
	genastore("dst",table68k[opcode].smode, "srcreg", table68k[opcode].size, "src");
	break;
     case i_TST:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genflags(flag_logical, table68k[opcode].size, "src", "", "");
	break;
     case i_BTST:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 1, 0);
	if (table68k[opcode].size == sz_byte)
	    printf("\tsrc &= 7;\n");
	else
	    printf("\tsrc &= 31;\n");
	printf("\tZFLG = !(dst & (1 << src));\n");
	break;
     case i_BCHG:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 1, 0);
	if (table68k[opcode].size == sz_byte)
	    printf("\tsrc &= 7;\n");
	else
	    printf("\tsrc &= 31;\n");
	printf("\tZFLG = !(dst & (1 << src));\n");
	printf("\tdst ^= (1 << src);\n");
	genastore("dst", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst");
	break;
     case i_BCLR:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 1, 0);
	if (table68k[opcode].size == sz_byte)
	    printf("\tsrc &= 7;\n");
	else
	    printf("\tsrc &= 31;\n");
	printf("\tZFLG = !(dst & (1 << src));\n");
	printf("\tdst &= ~(1 << src);\n");
	genastore("dst", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst");
	break;
     case i_BSET:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 1, 0);
	if (table68k[opcode].size == sz_byte)
	    printf("\tsrc &= 7;\n");
	else
	    printf("\tsrc &= 31;\n");
	printf("\tZFLG = !(dst & (1 << src));\n");
	printf("\tdst |= (1 << src);\n");
	genastore("dst", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst");
	break;
     case i_TAS:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	start_brace();
	printf("\tU32 dst = 0x80 | src;\n");
	genflags(flag_logical, table68k[opcode].size, "src", "", "");
	genastore("dst", table68k[opcode].smode, "srcreg", table68k[opcode].size, "src");
	break;
     case i_CMPM:
     case i_CMP:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 1, 0);
	start_brace ();
	genflags(flag_cmp, table68k[opcode].size, "newv", "src", "dst");
	break;
     case i_CMPA:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", sz_long, "dst", 1, 0);
	start_brace ();
	genflags(flag_cmp, sz_long, "newv", "src", "dst");
	break;
	/* The next two are coded a little unconventional, but they are doing
	 * weird things... */
     case i_MVPRM:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	printf("\tA32 memp = regs.a[dstreg] + (S32)(S16)nextiword();\n");
	if (table68k[opcode].size == sz_word) {
	    printf("\tput_byte(memp, src >> 8); put_byte(memp + 2, src);\n");
	} else {
	    printf("\tput_byte(memp, src >> 24); put_byte(memp + 2, src >> 16);\n");
	    printf("\tput_byte(memp + 4, src >> 8); put_byte(memp + 6, src);\n");
	}
	break;
     case i_MVPMR:
	printf("\tA32 memp = regs.a[srcreg] + (S32)(S16)nextiword();\n");
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 0, 0);
	if (table68k[opcode].size == sz_word) {
	    printf("\tU16 val = (get_byte(memp) << 8) + get_byte(memp + 2);\n");
	} else {
	    printf("\tU32 val = (get_byte(memp) << 24) + (get_byte(memp + 2) << 16)\n");
	    printf("              + (get_byte(memp + 4) << 8) + get_byte(memp + 6);\n");
	}
	genastore("val", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst");
	break;
     case i_MOVE:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 0, 0);
	genflags(flag_logical, table68k[opcode].size, "src", "", "");
	genastore("src", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst");
	break;
     case i_MOVEA:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 0, 0);
	genastore("src", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst");
	break;
     case i_MVSR2:
	genamode(table68k[opcode].smode, "srcreg", sz_word, "src", 0, 0);
	printf("\tMakeSR();\n");
	if (table68k[opcode].size == sz_byte) {
		start_brace();
		printf("\tU16 sr_w;\n");
		printf("\tsr_w = regs.sr & 0x00FF;\n");
		genastore("sr_w", table68k[opcode].smode, "srcreg", sz_word, "src");
	} else {
		genastore("regs.sr", table68k[opcode].smode, "srcreg", sz_word, "src");
	}
	break;
     case i_MV2SR:
	genamode(table68k[opcode].smode, "srcreg", sz_word, "src", 1, 0);
	if (table68k[opcode].size == sz_byte)
	    printf("\tMakeSR();\n\tregs.sr &= 0xFF00;\n\tregs.sr |= src & 0xFF;\n");
	else {
	    printf("\tregs.sr = src;\n");
	}
	printf("\tMakeFromSR();\n");
	break;
     case i_SWAP:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	start_brace ();
	printf("\tU32 dst = ((src >> 16)&0xFFFF) | ((src&0xFFFF)<<16);\n");
	genflags(flag_logical, table68k[opcode].size, "dst", "", "");
	genastore("dst",table68k[opcode].smode, "srcreg", table68k[opcode].size, "src");
	break;
     case i_EXG:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 1, 0);
	genastore("dst",table68k[opcode].smode, "srcreg", table68k[opcode].size, "src");
	genastore("src",table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst");
	break;
     case i_EXT:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	start_brace ();
	switch(table68k[opcode].size) {
	 case sz_word: printf("\tU32 dst = (S32)(S8)src;\n"); break;
	 case sz_long: printf("\tU32 dst = (S32)(S16)src;\n"); break;
	 default: abort();
	}
	genflags(flag_logical, table68k[opcode].size, "dst", "", "");
	genastore("dst",table68k[opcode].smode, "srcreg", table68k[opcode].size, "src");
	break;
     case i_MVMEL:
	genmovemel(opcode);
	break;
     case i_MVMLE:
	genmovemle(opcode);
	break;
     case i_TRAP:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	printf("\tException(src+32,0);\n");
	break;
     case i_MVR2USP:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	printf("\tregs.usp = src;\n");
	break;
     case i_MVUSP2R:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 0, 0);
	genastore("regs.usp", table68k[opcode].smode, "srcreg", table68k[opcode].size, "src");
	break;
     case i_RESET:
	printf("\tm68k_reset();\n");
	break;
     case i_NOP:
	break;
     case i_STOP:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	printf("\tregs.sr = src;\n");
	printf("\tMakeFromSR();\n");
	printf("\tm68k_setstopped(1);\n");
	break;
     case i_RTE:
	genamode(Aipi, "7", sz_word, "sr", 1, 0);
	genamode(Aipi, "7", sz_long, "pc", 1, 0);
	if (CPU_LEVEL > 0) {
	    genamode(Aipi, "7", sz_word, "format", 1, 0);
	}
	printf("\tregs.sr = sr; m68k_setpc(pc);\n");
	if (CPU_LEVEL > 0) {
	    printf("\tif ((format & 0xF000) == 0x8000) regs.a[7] += 50;\n");
	}
	printf("\tMakeFromSR();\n");
	break;
     case i_RTD:
	break;
     case i_LINK:
	genamode(Apdi, "7", sz_long, "old", 0, 0);
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genastore("src", Apdi, "7", sz_long, "old");
	genastore("regs.a[7]", table68k[opcode].smode, "srcreg", table68k[opcode].size, "src");
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "offs", 1, 0);
	printf("\tregs.a[7] += offs;\n");
	break;
     case i_UNLK:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	printf("\tregs.a[7] = src;\n");
	genamode(Aipi, "7", sz_long, "old", 1, 0);
	genastore("old", table68k[opcode].smode, "srcreg", table68k[opcode].size, "src");
	break;
     case i_RTS:
	genamode(Aipi, "7", sz_long, "pc", 1, 0);
	printf("\tm68k_setpc(pc);\n");
	break;
     case i_TRAPV:
	printf("\tif(VFLG) Exception(7,0);\n");
	break;
     case i_RTR:
	printf("\tMakeSR();\n");
	genamode(Aipi, "7", sz_word, "sr", 1, 0);
	genamode(Aipi, "7", sz_long, "pc", 1, 0);
	printf("\tregs.sr &= 0xFF00; sr &= 0xFF;\n");
	printf("\tregs.sr |= sr; m68k_setpc(pc);\n");
	printf("\tMakeFromSR();\n");
	break;
     case i_JSR:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 0, 0);
	genamode(Apdi, "7", sz_long, "sp", 0, 0);
	genastore("m68k_getpc()", Apdi, "7", sz_long, "sp");
	printf("\tm68k_setpc(srca);\n");
	break;
     case i_JMP:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 0, 0);
	printf("\tm68k_setpc(srca);\n");
	break;
     case i_BSR:
	printf("\tchar *oldpcp = (char *)regs.pc_p;\n");
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(Apdi, "7", sz_long, "sp", 0, 0);
	genastore("m68k_getpc()", Apdi, "7", sz_long, "sp");
	printf("\tregs.pc_p = (U16 *)(oldpcp + (S32)src);\n");
	break;
     case i_Bcc:
	printf("\tchar *oldpcp = (char *)regs.pc_p;\n");
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	printf("\tif (cctrue(%d)) regs.pc_p = (U16 *)(oldpcp + (S32)src);\n", table68k[opcode].cc);
	break;
     case i_LEA:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 0, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 0, 0);
	genastore("srca", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst");
	break;
     case i_PEA:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 0, 0);
	genamode(Apdi, "7", sz_long, "dst", 0, 0);
	genastore("srca", Apdi, "7", sz_long, "dst");
	break;
     case i_DBcc:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "offs", 1, 0);
	printf("\tif (!cctrue(%d)) {\n", table68k[opcode].cc);
	printf("\tif (src--) regs.pc_p = (U16 *)((char *)regs.pc_p + (S32)offs - 2);\n");
	genastore("src", table68k[opcode].smode, "srcreg", table68k[opcode].size, "src");
	printf("\t}\n");
	break;
     case i_Scc:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 0, 0);
	start_brace ();
	printf("\tint val = cctrue(%d) ? 0xff : 0;\n", table68k[opcode].cc);
	genastore("val",table68k[opcode].smode, "srcreg", table68k[opcode].size, "src");
	break;
     case i_DIVU:
/*	printf("\tA32 oldpc = m68k_getpc()-2;\n");*/
	genamode(table68k[opcode].smode, "srcreg", sz_word, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", sz_long, "dst", 1, 0);
	printf("\tif(src == 0) Exception(5,0); else {\n");
	printf("\tU32 newv = (U32)dst / (U16)src;\n");
	printf("\tU32 rem = (U32)dst %% (U16)src;\n");
	/* The N flag appears to be set each time there is an overflow.
	 * Weird. */
	printf("\tif (newv > 0xffff) { VFLG = NFLG = 1; } else\n\t{\n");
	genflags(flag_logical, sz_word, "newv", "", "");
	printf("\tnewv = (newv & 0xffff) | ((U32)rem << 16);\n");
	genastore("newv",table68k[opcode].dmode, "dstreg", sz_long, "dst");
	printf("\t}\n");
	printf("\t}\n");
	break;
     case i_DIVS:
/*	printf("\tA32 oldpc = m68k_getpc()-2;\n");*/
	genamode(table68k[opcode].smode, "srcreg", sz_word, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", sz_long, "dst", 1, 0);
	printf("\tif(src == 0) Exception(5,0); else {\n");
	printf("\tS32 newv = (S32)dst / (S16)src;\n");
	printf("\tU16 rem = (S32)dst %% (S16)src;\n");
	printf("\tif ((newv & 0xffff0000) && (newv & 0xffff0000) != 0xffff0000) { VFLG = NFLG = 1; } else\n\t{\n");
	printf("\tif (((S16)rem < 0) != ((S32)dst < 0)) rem = -rem;\n");
	genflags(flag_logical, sz_word, "newv", "", "");
	printf("\tnewv = (newv & 0xffff) | ((U32)rem << 16);\n");
	genastore("newv",table68k[opcode].dmode, "dstreg", sz_long, "dst");
	printf("\t}\n");
	printf("\t}\n");
	break;
     case i_MULU:
	genamode(table68k[opcode].smode, "srcreg", sz_word, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", sz_word, "dst", 1, 0);
	start_brace ();
	printf("\tU32 newv = (U32)(U16)dst * (U32)(U16)src;\n");
	genflags(flag_logical, sz_long, "newv", "", "");
	genastore("newv",table68k[opcode].dmode, "dstreg", sz_long, "dst");
	break;
     case i_MULS:
	genamode(table68k[opcode].smode, "srcreg", sz_word, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", sz_word, "dst", 1, 0);
	start_brace ();
	printf("\tU32 newv = (S32)(S16)dst * (S32)(S16)src;\n");
	genflags(flag_logical, sz_long, "newv", "", "");
	genastore("newv",table68k[opcode].dmode, "dstreg", sz_long, "dst");
	break;
     case i_CHK:
/*	printf("\tA32 oldpc = m68k_getpc()-2;\n");*/
	genamode(table68k[opcode].smode, "srcreg", sz_word, "src", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", sz_word, "dst", 1, 0);
	printf("\tif ((S16)dst < 0) { NFLG=1; Exception(6,0); }\n");
	printf("\telse if ((S16)dst > (S16)src) { NFLG=0; Exception(6,0); }\n");
	break;

     case i_ASR:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "cnt", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "data", 1, 0);
	start_brace ();
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU8 val = data;\n"); break;
	 case sz_word: printf("\tU16 val = data;\n"); break;
	 case sz_long: printf("\tU32 val = data;\n"); break;
	 default: abort();
	}
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU32 cmask = 0x80;\n"); break;
	 case sz_word: printf("\tU32 cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tU32 cmask = 0x80000000;\n"); break;
	 default: abort();
	}
	printf("\tU32 sign = cmask & val;\n");
	printf("\tcnt &= 63;\n");
	printf("\tVFLG = 0;\n");
	printf("\tif (!cnt) { CFLG = 0; } else {");
	printf("\tfor(;cnt;--cnt){\n");
	printf("\tCFLG=regs.x=val&1; val = ((U32)val >> 1) | sign;\n");
	printf("\t}}\n\tNFLG = sign != 0;\n");
	printf("\tZFLG = val == 0;\n");
	genastore("val", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "data");
	break;
     case i_ASL:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "cnt", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "data", 1, 0);
	start_brace ();
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU8 val = data;\n"); break;
	 case sz_word: printf("\tU16 val = data;\n"); break;
	 case sz_long: printf("\tU32 val = data;\n"); break;
	 default: abort();
	}
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU32 cmask = 0x80;\n"); break;
	 case sz_word: printf("\tU32 cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tU32 cmask = 0x80000000;\n"); break;
	 default: abort();
	}
	printf("\tU32 sign = cmask & val;\n");
	printf("\tcnt &= 63;\n");
	printf("\tVFLG = 0;\n");
	printf("\tif (!cnt) { CFLG = 0; } else { ");
	printf("\tfor(;cnt;--cnt){\n");
	printf("\tCFLG=regs.x=(val&cmask)!=0; val <<= 1;\n");
	printf("\tif ((val&cmask)!=sign)VFLG=1;\n");
	printf("\t}}\n\tNFLG = (val&cmask) != 0;\n");
	printf("\tZFLG = val == 0;\n");
	genastore("val", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "data");
	break;
     case i_LSR:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "cnt", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "data", 1, 0);
	start_brace ();
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU8 val = data;\n"); break;
	 case sz_word: printf("\tU16 val = data;\n"); break;
	 case sz_long: printf("\tU32 val = data;\n"); break;
	 default: abort();
	}
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU32 cmask = 0x80;\n"); break;
	 case sz_word: printf("\tU32 cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tU32 cmask = 0x80000000;\n"); break;
	 default: abort();
	}
	printf("\tcnt &= 63;\n");
	printf("\tif (!cnt) { CFLG = 0; } else {");
	printf("\tint carry = 0;\n");
	printf("\tfor(;cnt;--cnt){\n");
	printf("\tcarry=val&1; val >>= 1;\n");
	printf("\t}\n");
	printf("\tCFLG = regs.x = carry!=0;\n}\n");
	printf("\tNFLG = (val & cmask) != 0; ZFLG = val == 0; VFLG = 0;\n");
	genastore("val", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "data");
	break;
     case i_LSL:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "cnt", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "data", 1, 0);
	start_brace ();
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU8 val = data;\n"); break;
	 case sz_word: printf("\tU16 val = data;\n"); break;
	 case sz_long: printf("\tU32 val = data;\n"); break;
	 default: abort();
	}
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU32 cmask = 0x80;\n"); break;
	 case sz_word: printf("\tU32 cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tU32 cmask = 0x80000000;\n"); break;
	 default: abort();
	}
	printf("\tint carry = 0;\n");
	printf("\tcnt &= 63;\n");
	printf("\tif (!cnt) { CFLG = 0; } else {");
	printf("\tfor(;cnt;--cnt){\n");
	printf("\tcarry=val&cmask; val <<= 1;\n");
	printf("\t}\n");
	printf("\tCFLG = regs.x = carry!=0;\n}\n");
	printf("\tNFLG = (val & cmask) != 0; ZFLG = val == 0; VFLG = 0;\n");
	genastore("val", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "data");
	break;
     case i_ROL:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "cnt", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "data", 1, 0);
	start_brace ();
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU8 val = data;\n"); break;
	 case sz_word: printf("\tU16 val = data;\n"); break;
	 case sz_long: printf("\tU32 val = data;\n"); break;
	 default: abort();
	}
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU32 cmask = 0x80;\n"); break;
	 case sz_word: printf("\tU32 cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tU32 cmask = 0x80000000;\n"); break;
	 default: abort();
	}
	printf("\tint carry = 0;\n");
	printf("\tcnt &= 63;\n");
	printf("\tif (!cnt) { CFLG = 0; } else {");
	printf("\tfor(;cnt;--cnt){\n");
	printf("\tcarry=val&cmask; val <<= 1;\n");
	printf("\tif(carry)  val |= 1;\n");
	printf("\t}\n");
	printf("\tCFLG = carry!=0;\n}\n");
	printf("\tNFLG = (val & cmask) != 0; ZFLG = val == 0; VFLG = 0;\n");
	genastore("val", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "data");
	break;
     case i_ROR:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "cnt", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "data", 1, 0);
	start_brace ();
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU8 val = data;\n"); break;
	 case sz_word: printf("\tU16 val = data;\n"); break;
	 case sz_long: printf("\tU32 val = data;\n"); break;
	 default: abort();
	}
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU32 cmask = 0x80;\n"); break;
	 case sz_word: printf("\tU32 cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tU32 cmask = 0x80000000;\n"); break;
	 default: abort();
	}
	printf("\tint carry = 0;\n");
	printf("\tcnt &= 63;\n");
	printf("\tif (!cnt) { CFLG = 0; } else {");
	printf("\tfor(;cnt;--cnt){\n");
	printf("\tcarry=val&1; val = (U32)val >> 1;\n");
	printf("\tif(carry) val |= cmask;\n");
	printf("\t}\n");
	printf("\tCFLG = carry!=0;\n}\n");
	printf("\tNFLG = (val & cmask) != 0; ZFLG = val == 0; VFLG = 0;\n");
	genastore("val", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "data");
	break;
     case i_ROXL:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "cnt", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "data", 1, 0);
	start_brace ();
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU8 val = data;\n"); break;
	 case sz_word: printf("\tU16 val = data;\n"); break;
	 case sz_long: printf("\tU32 val = data;\n"); break;
	 default: abort();
	}
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU32 cmask = 0x80;\n"); break;
	 case sz_word: printf("\tU32 cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tU32 cmask = 0x80000000;\n"); break;
	 default: abort();
	}
	printf("\tint carry = 0;\n");
	printf("\tcnt &= 63;\n");
	printf("\tfor(;cnt;--cnt){\n");
	printf("\tcarry=val&cmask; val <<= 1;\n");
	printf("\tif(regs.x) val |= 1;\n");
	printf("\tregs.x = carry != 0;\n");
	printf("\t}\n");
	printf("\tCFLG = regs.x;\n");
	printf("\tNFLG = (val & cmask) != 0; ZFLG = val == 0; VFLG = 0;\n");
	genastore("val", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "data");
	break;
     case i_ROXR:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "cnt", 1, 0);
	genamode(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "data", 1, 0);
	start_brace ();
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU8 val = data;\n"); break;
	 case sz_word: printf("\tU16 val = data;\n"); break;
	 case sz_long: printf("\tU32 val = data;\n"); break;
	 default: abort();
	}
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU32 cmask = 0x80;\n"); break;
	 case sz_word: printf("\tU32 cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tU32 cmask = 0x80000000;\n"); break;
	 default: abort();
	}
	printf("\tint carry = 0;\n");
	printf("\tcnt &= 63;\n");
	printf("\tfor(;cnt;--cnt){\n");
	printf("\tcarry=val&1; val >>= 1;\n");
	printf("\tif(regs.x) val |= cmask;\n");
	printf("\tregs.x = carry != 0;\n");
	printf("\t}\n");
	printf("\tCFLG = regs.x;\n");
	printf("\tNFLG = (val & cmask) != 0; ZFLG = val == 0; VFLG = 0;\n");
	genastore("val", table68k[opcode].dmode, "dstreg", table68k[opcode].size, "data");
	break;
     case i_ASRW:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "data", 1, 0);
	printf("\tVFLG = 0;\n");
	start_brace ();
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU8 val = data;\n"); break;
	 case sz_word: printf("\tU16 val = data;\n"); break;
	 case sz_long: printf("\tU32 val = data;\n"); break;
	 default: abort();
	}
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU32 cmask = 0x80;\n"); break;
	 case sz_word: printf("\tU32 cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tU32 cmask = 0x80000000;\n"); break;
	 default: abort();
	}
	printf("\tU32 sign = cmask & val;\n");
	printf("\tCFLG=regs.x=val&1; val = (val >> 1) | sign;\n");
	printf("\tNFLG = sign != 0;\n");
	printf("\tZFLG = val == 0;\n");
	genastore("val", table68k[opcode].smode, "srcreg", table68k[opcode].size, "data");
	break;
     case i_ASLW:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "data", 1, 0);
	printf("\tVFLG = 0;\n");
	start_brace ();
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU8 val = data;\n"); break;
	 case sz_word: printf("\tU16 val = data;\n"); break;
	 case sz_long: printf("\tU32 val = data;\n"); break;
	 default: abort();
	}
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU32 cmask = 0x80;\n"); break;
	 case sz_word: printf("\tU32 cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tU32 cmask = 0x80000000;\n"); break;
	 default: abort();
	}
	printf("\tU32 sign = cmask & val;\n");
	printf("\tCFLG=regs.x=(val&cmask)!=0; val <<= 1;\n");
	printf("\tif ((val&cmask)!=sign) VFLG=1;\n");
	printf("\tNFLG = (val&cmask) != 0;\n");
	printf("\tZFLG = val == 0;\n");
	genastore("val", table68k[opcode].smode, "srcreg", table68k[opcode].size, "data");
	break;
     case i_LSRW:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "data", 1, 0);
	start_brace ();
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU8 val = data;\n"); break;
	 case sz_word: printf("\tU16 val = data;\n"); break;
	 case sz_long: printf("\tU32 val = data;\n"); break;
	 default: abort();
	}
	printf("\tint carry = val&1;\n");
	printf("\tcarry=val&1; val >>= 1;\n");
	genflags(flag_logical, table68k[opcode].size, "val", "", "");
	printf("CFLG = regs.x = carry!=0;\n");
	genastore("val", table68k[opcode].smode, "srcreg", table68k[opcode].size, "data");
	break;
     case i_LSLW:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "data", 1, 0);
	start_brace ();
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU8 val = data;\n"); break;
	 case sz_word: printf("\tU16 val = data;\n"); break;
	 case sz_long: printf("\tU32 val = data;\n"); break;
	 default: abort();
	}
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU32 cmask = 0x80;\n"); break;
	 case sz_word: printf("\tU32 cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tU32 cmask = 0x80000000;\n"); break;
	 default: abort();
	}
	printf("\tint carry = val&cmask;\n");
	printf("\tval <<= 1;\n");
	genflags(flag_logical, table68k[opcode].size, "val", "", "");
	printf("CFLG = regs.x = carry!=0;\n");
	genastore("val", table68k[opcode].smode, "srcreg", table68k[opcode].size, "data");
	break;
     case i_ROLW:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "data", 1, 0);
	start_brace ();
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU8 val = data;\n"); break;
	 case sz_word: printf("\tU16 val = data;\n"); break;
	 case sz_long: printf("\tU32 val = data;\n"); break;
	 default: abort();
	}
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU32 cmask = 0x80;\n"); break;
	 case sz_word: printf("\tU32 cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tU32 cmask = 0x80000000;\n"); break;
	 default: abort();
	}
	printf("\tint carry = val&cmask;\n");
	printf("\tval <<= 1;\n");
	printf("\tif(carry)  val |= 1;\n");
	genflags(flag_logical, table68k[opcode].size, "val", "", "");
	printf("CFLG = carry!=0;\n");
	genastore("val", table68k[opcode].smode, "srcreg", table68k[opcode].size, "data");
	break;
     case i_RORW:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "data", 1, 0);
	start_brace ();
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU8 val = data;\n"); break;
	 case sz_word: printf("\tU16 val = data;\n"); break;
	 case sz_long: printf("\tU32 val = data;\n"); break;
	 default: abort();
	}
	printf("\tint carry = val&1;\n");
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU32 cmask = 0x80;\n"); break;
	 case sz_word: printf("\tU32 cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tU32 cmask = 0x80000000;\n"); break;
	 default: abort();
	}
	printf("\tval >>= 1;\n");
	printf("\tif(carry) val |= cmask;\n");
	genflags(flag_logical, table68k[opcode].size, "val", "", "");
	printf("CFLG = carry!=0;\n");
	genastore("val", table68k[opcode].smode, "srcreg", table68k[opcode].size, "data");
	break;
     case i_ROXLW:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "data", 1, 0);
	start_brace ();
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU8 val = data;\n"); break;
	 case sz_word: printf("\tU16 val = data;\n"); break;
	 case sz_long: printf("\tU32 val = data;\n"); break;
	 default: abort();
	}
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU32 cmask = 0x80;\n"); break;
	 case sz_word: printf("\tU32 cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tU32 cmask = 0x80000000;\n"); break;
	 default: abort();
	}
	printf("\tint carry = val&cmask;\n");
	printf("\tval <<= 1;\n");
	printf("\tif(regs.x) val |= 1;\n");
	printf("\tregs.x = carry != 0;\n");
	genflags(flag_logical, table68k[opcode].size, "val", "", "");
	printf("regs.x = CFLG = carry!=0;\n");
	genastore("val", table68k[opcode].smode, "srcreg", table68k[opcode].size, "data");
	break;
     case i_ROXRW:
	genamode(table68k[opcode].smode, "srcreg", table68k[opcode].size, "data", 1, 0);
	start_brace ();
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU8 val = data;\n"); break;
	 case sz_word: printf("\tU16 val = data;\n"); break;
	 case sz_long: printf("\tU32 val = data;\n"); break;
	 default: abort();
	}
	printf("\tint carry = val&1;\n");
	switch(table68k[opcode].size) {
	 case sz_byte: printf("\tU32 cmask = 0x80;\n"); break;
	 case sz_word: printf("\tU32 cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tU32 cmask = 0x80000000;\n"); break;
	 default: abort();
	}
	printf("\tval >>= 1;\n");
	printf("\tif(regs.x) val |= cmask;\n");
	printf("\tregs.x = carry != 0;\n");
	genflags(flag_logical, table68k[opcode].size, "val", "", "");
	printf("regs.x = CFLG = carry!=0;\n");
	genastore("val", table68k[opcode].smode, "srcreg", table68k[opcode].size, "data");
	break;
     case i_MOVEC2:
	genamode (table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	start_brace();
	printf("\tint regno = (src >> 12) & 7;\n");
	printf("\tU32 *regp = src & 0x8000 ? regs.a + regno : regs.d + regno;\n");
	printf("\tm68k_movec2(src & 0xFFF, regp);\n");
	break;
     case i_MOVE2C:
	genamode (table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	start_brace();
	printf("\tint regno = (src >> 12) & 7;\n");
	printf("\tU32 *regp = src & 0x8000 ? regs.a + regno : regs.d + regno;\n");
	printf("\tm68k_move2c(src & 0xFFF, regp);\n");
	break;
     case i_CAS:
	{
	    int old_brace_level;
	    genamode (table68k[opcode].smode, "srcreg", table68k[opcode].size, "src", 1, 0);
	    genamode (table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 1, 0);
	    start_brace();
	    printf("\tint ru = (src >> 6) & 7;\n");
	    printf("\tint rc = src & 7;\n");
	    printf("\tif ((S32)regs.d[rc] == (S32)dst)");
	    old_brace_level = n_braces;
	    start_brace ();
	    genastore("(regs.d[ru])",table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst");
	    pop_braces (old_brace_level);
	    printf("else");
	    start_brace ();
	    printf("regs.d[rc] = dst;\n");
	    pop_braces (old_brace_level);
	}
	break;
     case i_DIVL:
	genamode (table68k[opcode].smode, "srcreg", table68k[opcode].size, "extra", 1, 0);
	genamode (table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 1, 0);
	printf("\tm68k_divl(opcode, dst, extra);\n");
	break;
     case i_MULL:
	genamode (table68k[opcode].smode, "srcreg", table68k[opcode].size, "extra", 1, 0);
	genamode (table68k[opcode].dmode, "dstreg", table68k[opcode].size, "dst", 1, 0);
	printf("\tm68k_mull(opcode, dst, extra);\n");
	break;
     default:
	abort();
	break;
    };
    finish_braces ();
}

static void generate_func(long int from, long int to)
{
    int illg = 0;
    long int opcode;
    U16 smsk;
    U16 dmsk;

    printf("#include \"sysconfig.h\"\n");
    printf("#include \"sysdeps.h\"\n");
    printf("#include \"options.h\"\n");
    printf("#include \"qlmem.h\"\n");
    printf("#include \"exe68k.h\"\n");
    printf("#include \"op68ktbl.h\"\n");
    for(opcode=from; opcode < to; opcode++) {
	if (table68k[opcode].mnemo == i_ILLG) {
	    illg++;
	    continue;
	}

	if (isspecific(opcode)) {
	    printf("void op_%lx_s(U32 opcode)\n{\n", opcode);
	    if (table68k[opcode].suse
		&& table68k[opcode].smode != imm  && table68k[opcode].smode != imm0
		&& table68k[opcode].smode != imm1 && table68k[opcode].smode != imm2
		&& table68k[opcode].smode != absw && table68k[opcode].smode != absl
		&& table68k[opcode].smode != PC8r && table68k[opcode].smode != PC16)
	    {
		printf("\tU32 srcreg = (S32)(S8)%d;\n", (int)table68k[opcode].sreg);
	    }
	    if (table68k[opcode].duse
		/* Yes, the dmode can be imm, in case of LINK or DBcc */
		&& table68k[opcode].dmode != imm  && table68k[opcode].dmode != imm0
		&& table68k[opcode].dmode != imm1 && table68k[opcode].dmode != imm2
		&& table68k[opcode].dmode != absw && table68k[opcode].dmode != absl) {
		printf("\tU32 dstreg = (S32)(S8)%d;\n", (int)table68k[opcode].dreg);
	    }
	    gen_opcode(opcode);
	    printf("}\n");
	}

	if (table68k[opcode].handler != -1)
	    continue;


	switch (table68k[opcode].stype) {
	 case 0:
	    smsk = 7; break;
	 case 1:
	    smsk = 255; break;
	 case 2:
	    smsk = 15; break;
	 case 3:
	    smsk = 7; break;
	 default:
	    abort();
	}
	smsk <<= table68k[opcode].spos;
	dmsk = 7 << table68k[opcode].dpos;

	printf("void op_%lx(U32 opcode)\n{\n", opcode);
	if (table68k[opcode].suse
	    && table68k[opcode].smode != imm  && table68k[opcode].smode != imm0
	    && table68k[opcode].smode != imm1 && table68k[opcode].smode != imm2
	    && table68k[opcode].smode != absw && table68k[opcode].smode != absl
	    && table68k[opcode].smode != PC8r && table68k[opcode].smode != PC16)
	{
	    if (table68k[opcode].spos == -1) {
		printf("\tU32 srcreg = (S32)(S8)%d;\n", (int)table68k[opcode].sreg);
	    } else {
		if (table68k[opcode].stype == 3)
		    printf("\tU32 srcreg = imm8_table[(opcode & %d) >> %d];\n",
			   smsk, (int)table68k[opcode].spos);
		else
		    printf("\tU32 srcreg = (S32)(S8)((opcode & %d) >> %d);\n",
			   smsk, (int)table68k[opcode].spos);
	    }
	}
	if (table68k[opcode].duse
	    /* Yes, the dmode can be imm, in case of LINK or DBcc */
	    && table68k[opcode].dmode != imm  && table68k[opcode].dmode != imm0
	    && table68k[opcode].dmode != imm1 && table68k[opcode].dmode != imm2
	    && table68k[opcode].dmode != absw && table68k[opcode].dmode != absl) {
	    if (table68k[opcode].dpos == -1) {
		printf("\tU32 dstreg = (S32)(S8)%d;\n", (int)table68k[opcode].dreg);
	    } else {
		printf("\tU32 dstreg = (opcode & %d) >> %d;\n", dmsk, (int)table68k[opcode].dpos);
	    }
	}
	gen_opcode(opcode);
        printf("}\n");
    }

    fprintf (stderr, "%d illegals generated.\n", illg);
}

static void generate_table(void)
{
    int illg = 0;
    long int opcode;

    printf("#include \"sysconfig.h\"\n");
    printf("#include \"sysdeps.h\"\n");
    printf("#include \"options.h\"\n");
    printf("#include \"qlmem.h\"\n");
    printf("#include \"exe68k.h\"\n");
    printf("#include \"op68ktbl.h\"\n");

    printf("cpuop_func *cpufunctbl[65536] = {\n");
    for(opcode=0; opcode < 65536; opcode++) {
	if (table68k[opcode].mnemo == i_ILLG) {
	    printf("op_illg");
	    illg++;
	} else if (isspecific(opcode))
	    printf("op_%lx_s", opcode);
	else if (table68k[opcode].handler != -1)
	    printf("op_%lx", table68k[opcode].handler);
	else
	    printf("op_%lx", opcode);

	if (opcode < 65535) printf(",");
	if ((opcode & 7) == 7) printf("\n");
    }
    printf("\n};\n");
    fprintf (stderr, "%d illegals generated.\n", illg);
    if (get_no_mismatches())
	fprintf(stderr, "%d mismatches.\n", get_no_mismatches());
}

static void generate_smalltable(void)
{
    long int opcode;

    printf("#include \"sysconfig.h\"\n");
    printf("#include \"sysdeps.h\"\n");
    printf("#include \"options.h\"\n");
    printf("#include \"qlmem.h\"\n");
    printf("#include \"exe68k.h\"\n");
    printf("#include \"op68ktbl.h\"\n");

    printf("struct cputbl smallcputbl[] = {\n");
    for(opcode=0; opcode < 65536; opcode++) {
	if ((isspecific(opcode) || table68k[opcode].handler == -1)
	    && table68k[opcode].mnemo != i_ILLG)
	{
	    if (isspecific(opcode))
		printf("{ op_%x_s, 1, %d },\n", opcode, opcode);
	    if (table68k[opcode].handler == -1)
		printf("{ op_%x, 0, %d },\n", opcode, opcode);
	}
    }
    printf("{ 0, 0, 0 }};\n");
}

static void generate_header(void)
{
    int illg = 0;
    long int opcode;

    for(opcode=0; opcode < 65536; opcode++) {
	if (table68k[opcode].mnemo == i_ILLG) {
	    illg++;
	    continue;
	}
	if (isspecific(opcode))
	    printf("extern cpuop_func op_%lx_s;\n", opcode);
	if (table68k[opcode].handler != -1)
	    continue;

	printf("extern cpuop_func op_%lx;\n", opcode);
    }

    fprintf (stderr, "%d illegals generated.\n", illg);
    if (get_no_mismatches())
	fprintf(stderr, "%d mismatches.\n", get_no_mismatches());
}

int main(int argc, char **argv)
{
    long int range = -1;
    char mode = 'n';
    int i;

    if (argc == 2)
    	mode = *argv[1];

    if (argc == 3) {
	range = atoi(argv[2]);
	mode = *argv[1];
    }

    read_table68k ();
    read_counts();
    do_merges ();

    switch(mode) {
     case 'f':
    	generate_func(range * 0x1000, (range + 1) * 0x1000);
	break;
     case 'h':
    	generate_header();
	break;
     case 't':
	generate_table();
	break;
     case 's':
	generate_smalltable();
	break;
     default:
	abort();
    }
    free(table68k);
    return 0;
}
