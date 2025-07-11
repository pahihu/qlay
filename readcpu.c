/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998

	Read 68000 CPU specs from file "table68k"
	Copyright 1995,1996 Bernd Schmidt
*/

#include "sysconfig.h"
#include "sysdeps.h"
#include <ctype.h>
#include "options.h"
#include "readcpu.h"

struct mnemolookup lookuptab[] = {
    { i_ILLG, "ILLEGAL" },
    { i_OR, "OR" },
    { i_CHK, "CHK" },
    { i_AND, "AND" },
    { i_EOR, "EOR" },
    { i_ORSR, "ORSR" },
    { i_ANDSR, "ANDSR" },
    { i_EORSR, "EORSR" },
    { i_SUB, "SUB" },
    { i_SUBA, "SUBA" },
    { i_SUBX, "SUBX" },
    { i_SBCD, "SBCD" },
    { i_ADD, "ADD" },
    { i_ADDA, "ADDA" },
    { i_ADDX, "ADDX" },
    { i_ABCD, "ABCD" },
    { i_NEG, "NEG" },
    { i_NEGX, "NEGX" },
    { i_NBCD, "NBCD" },
    { i_CLR, "CLR" },
    { i_NOT, "NOT" },
    { i_TST, "TST" },
    { i_BTST, "BTST" },
    { i_BCHG, "BCHG" },
    { i_BCLR, "BCLR" },
    { i_BSET, "BSET" },
    { i_TAS, "TAS" },
    { i_CMP, "CMP" },
    { i_CMPM, "CMPM" },
    { i_CMPA, "CMPA" },
    { i_MVPRM, "MVPRM" },
    { i_MVPMR, "MVPMR" },
    { i_MOVE, "MOVE" },
    { i_MOVEA, "MOVEA" },
    { i_MVSR2, "MVSR2" },
    { i_MV2SR, "MV2SR" },
    { i_SWAP, "SWAP" },
    { i_EXG, "EXG" },
    { i_EXT, "EXT" },
    { i_MVMEL, "MVMEL" },
    { i_MVMLE, "MVMLE" },
    { i_TRAP, "TRAP" },
    { i_MVR2USP, "MVR2USP" },
    { i_MVUSP2R, "MVUSP2R" },
    { i_RESET, "RESET" },
    { i_NOP, "NOP" },
    { i_STOP, "STOP" },
    { i_RTE, "RTE" },
    { i_RTD, "RTD" },
    { i_LINK, "LINK" },
    { i_UNLK, "UNLK" },
    { i_RTS, "RTS" },
    { i_TRAPV, "TRAPV" },
    { i_RTR, "RTR" },
    { i_JSR, "JSR" },
    { i_JMP, "JMP" },
    { i_BSR, "BSR" },
    { i_Bcc, "Bcc" },
    { i_LEA, "LEA" },
    { i_PEA, "PEA" },
    { i_DBcc, "DBcc" },
    { i_Scc, "Scc" },
    { i_DIVU, "DIVU" },
    { i_DIVS, "DIVS" },
    { i_MULU, "MULU" },
    { i_MULS, "MULS" },
    { i_ASR, "ASR" },
    { i_ASL, "ASL" },
    { i_LSR, "LSR" },
    { i_LSL, "LSL" },
    { i_ROL, "ROL" },
    { i_ROR, "ROR" },
    { i_ROXL, "ROXL" },
    { i_ROXR, "ROXR" },
    { i_ASRW, "ASRW" },
    { i_ASLW, "ASLW" },
    { i_LSRW, "LSRW" },
    { i_LSLW, "LSLW" },
    { i_ROLW, "ROLW" },
    { i_RORW, "RORW" },
    { i_ROXLW, "ROXLW" },
    { i_ROXRW, "ROXRW" },

    { i_MOVE2C, "MOVE2C" },
    { i_MOVEC2, "MOVEC2" },
    { i_CAS, "CAS" },
    { i_MULL, "MULL" },
    { i_DIVL, "DIVL" }
};

struct instr *table68k;

static amodes mode_from_str(const char *str)
{
    if (strncmp(str,"Dreg",4) == 0) return Dreg;
    if (strncmp(str,"Areg",4) == 0) return Areg;
    if (strncmp(str,"Aind",4) == 0) return Aind;
    if (strncmp(str,"Apdi",4) == 0) return Apdi;
    if (strncmp(str,"Aipi",4) == 0) return Aipi;
    if (strncmp(str,"Ad16",4) == 0) return Ad16;
    if (strncmp(str,"Ad8r",4) == 0) return Ad8r;
    if (strncmp(str,"absw",4) == 0) return absw;
    if (strncmp(str,"absl",4) == 0) return absl;
    if (strncmp(str,"PC16",4) == 0) return PC16;
    if (strncmp(str,"PC8r",4) == 0) return PC8r;
    if (strncmp(str,"Immd",4) == 0) return imm;
    abort();
}

static amodes mode_from_mr(int mode, int reg)
{
    switch(mode) {
     case 0: return Dreg;
     case 1: return Areg;
     case 2: return Aind;
     case 3: return Aipi;
     case 4: return Apdi;
     case 5: return Ad16;
     case 6: return Ad8r;
     case 7:
	switch(reg) {
	 case 0: return absw;
	 case 1: return absl;
	 case 2: return PC16;
	 case 3: return PC8r;
	 case 4: return imm;
	 case 5:
	 case 6:
	 case 7: return am_illg;
	}
    }
    abort();
}

static void build_insn(int insn)
{
    int find;
    long int opc;
    int i, variants;
    struct instr_def id = defs68k[insn];
    const char *opcstr = id.opcstr;

    for (variants = 0; variants < (1 << id.n_variable); variants++) {
	int bitcnt[lastbit];
	int bitval[lastbit];
	int bitpos[lastbit];
	int i, j;
	U16 opc = id.bits;
	U16 msk, vmsk;
	int pos = 0;
	int mnp = 0;
	int bitno = 0;
	char mnemonic[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	wordsizes sz = sz_unknown;
	int srcgather = 0, dstgather = 0;
	int usesrc = 0, usedst = 0;
	int srctype = 0;
	int srcpos = 0, dstpos = 0;

	amodes srcmode = am_unknown, destmode = am_unknown;
	int srcreg = 0, destreg = 0;

	for(i = 0; i < lastbit; i++)
	    bitcnt[i] = bitval[i] = 0;

	vmsk = 1 << id.n_variable;

	for(i = 0, msk = 0x8000; i < 16; i++, msk >>= 1) {
	    if (!(msk & id.mask)) {
		int currbit = id.bitpos[bitno++];
		int bit_set;
		vmsk >>= 1;
		bit_set = variants & vmsk ? 1 : 0;
		if (bit_set)
		    opc |= msk;
		bitpos[currbit] = 15 - i;
		bitcnt[currbit]++;
		bitval[currbit] <<= 1;
		bitval[currbit] |= bit_set;
	    }
	}

	if (bitval[bitj] == 0) bitval[bitj] = 8;
	/* first check whether this one does not match after all */
	if (bitval[bitz] == 3 || bitval[bitC] == 1)
	    continue;
	if (bitcnt[bitI] && bitval[bitI] == 0)
	    continue;

	/* bitI and bitC get copied to biti and bitc */
	if (bitcnt[bitI]) {
	    bitval[biti] = bitval[bitI]; bitpos[biti] = bitpos[bitI];
	}
	if (bitcnt[bitC])
	    bitval[bitc] = bitval[bitC];

	pos = 0;
	while (opcstr[pos] && !isspace(opcstr[pos])) {
	    if (opcstr[pos] == '.') {
		pos++;
		switch(opcstr[pos]) {

		 case 'B': sz = sz_byte; break;
		 case 'W': sz = sz_word; break;
		 case 'L': sz = sz_long; break;
		 case 'z':
		    switch(bitval[bitz]) {
		     case 0: sz = sz_byte; break;
		     case 1: sz = sz_word; break;
		     case 2: sz = sz_long; break;
		     default: abort();
		    }
		    break;
		 default: abort();
		}
	    } else {
		mnemonic[mnp] = opcstr[pos];
		if(mnemonic[mnp] == 'f') {
		    switch(bitval[bitf]) {
		     case 0: mnemonic[mnp] = 'R'; break;
		     case 1: mnemonic[mnp] = 'L'; break;
		     default: abort();
		    }
		}
		mnp++;
	    }
	    pos++;
	}

	/* now, we have read the mnemonic and the size */
	while (opcstr[pos] && isspace(opcstr[pos]))
	    pos++;

	/* A goto a day keeps the D******a away. */
	if (opcstr[pos] == 0)
	    goto endofline;

	/* parse the source address */
	usesrc = 1;
	switch(opcstr[pos++]) {
	 case 'D':
	    srcmode = Dreg;
	    switch (opcstr[pos++]) {
	     case 'r': srcreg = bitval[bitr]; srcgather = 1; srcpos = bitpos[bitr]; break;
	     case 'R': srcreg = bitval[bitR]; srcgather = 1; srcpos = bitpos[bitR]; break;
	     default: abort();
	    }

	    break;
	 case 'A':
	    srcmode = Areg;
	    switch (opcstr[pos++]) {
	     case 'r': srcreg = bitval[bitr]; srcgather = 1; srcpos = bitpos[bitr]; break;
	     case 'R': srcreg = bitval[bitR]; srcgather = 1; srcpos = bitpos[bitR]; break;
	     default: abort();
	    }
	    switch (opcstr[pos]) {
	     case 'p': srcmode = Apdi; pos++; break;
	     case 'P': srcmode = Aipi; pos++; break;
	    }
	    break;
	 case '#':
	    switch(opcstr[pos++]) {
	     case 'z': srcmode = imm; break;
	     case '0': srcmode = imm0; break;
	     case '1': srcmode = imm1; break;
	     case '2': srcmode = imm2; break;
	     case 'i': srcmode = immi; srcreg = (S32)(S8)bitval[biti];
		if (CPU_EMU_SIZE < 4) {
		    /* Used for branch instructions */
		    srctype = 1;
		    srcgather = 1;
		    srcpos = bitpos[biti];
		}
		break;
	     case 'j': srcmode = immi; srcreg = bitval[bitj];
		if (CPU_EMU_SIZE < 3) {
		    /* 1..8 for ADDQ/SUBQ and rotshi insns */
		    srcgather = 1;
		    srctype = 3;
		    srcpos = bitpos[bitj];
		}
		break;
	     case 'J': srcmode = immi; srcreg = bitval[bitJ];
		if (CPU_EMU_SIZE < 5) {
		    /* 0..15 */
		    srcgather = 1;
		    srctype = 2;
		    srcpos = bitpos[bitJ];
		}
		break;
	     default: abort();
	    }
	    break;
	 case 'd':
	    srcreg = bitval[bitD];
	    srcmode = mode_from_mr(bitval[bitd],bitval[bitD]);
	    if (srcmode == am_illg) continue;
	    if (CPU_EMU_SIZE < 2 &&
		(srcmode == Areg || srcmode == Dreg || srcmode == Aind
		 || srcmode == Ad16 || srcmode == Ad8r || srcmode == Aipi
		 || srcmode == Apdi))
	    {
		srcgather = 1; srcpos = bitpos[bitD];
	    }
	    if (opcstr[pos] == '[') {
		pos++;
		if (opcstr[pos] == '!') {
		    /* exclusion */
		    do {
			pos++;
			if (mode_from_str(opcstr+pos) == srcmode)
			    goto nomatch;
			pos += 4;
		    } while (opcstr[pos] == ',');
		    pos++;
		} else {
		    if (opcstr[pos+4] == '-') {
			/* replacement */
			if (mode_from_str(opcstr+pos) == srcmode)
			    srcmode = mode_from_str(opcstr+pos+5);
			else
			    goto nomatch;
			pos += 10;
		    } else {
			/* normal */
			while(mode_from_str(opcstr+pos) != srcmode) {
			    pos += 4;
			    if (opcstr[pos] == ']')
				goto nomatch;
			    pos++;
			}
			while(opcstr[pos] != ']') pos++;
			pos++;
			break;
		    }
		}
	    }
	    /* Some addressing modes are invalid as destination */
	    if (srcmode == imm || srcmode == PC16 || srcmode == PC8r)
		goto nomatch;
	    break;
	 case 's':
	    srcreg = bitval[bitS];
	    srcmode = mode_from_mr(bitval[bits],bitval[bitS]);

	    if (srcmode == am_illg) continue;
	    if (CPU_EMU_SIZE < 2 &&
		(srcmode == Areg || srcmode == Dreg || srcmode == Aind
		 || srcmode == Ad16 || srcmode == Ad8r || srcmode == Aipi
		 || srcmode == Apdi))
	    {
		srcgather = 1; srcpos = bitpos[bitS];
	    }
	    if (opcstr[pos] == '[') {
		pos++;
		if (opcstr[pos] == '!') {
		    /* exclusion */
		    do {
			pos++;
			if (mode_from_str(opcstr+pos) == srcmode)
			    goto nomatch;
			pos += 4;
		    } while (opcstr[pos] == ',');
		    pos++;
		} else {
		    if (opcstr[pos+4] == '-') {
			/* replacement */
			if (mode_from_str(opcstr+pos) == srcmode)
			    srcmode = mode_from_str(opcstr+pos+5);
			else
			    goto nomatch;
			pos += 10;
		    } else {
			/* normal */
			while(mode_from_str(opcstr+pos) != srcmode) {
			    pos += 4;
			    if (opcstr[pos] == ']')
				goto nomatch;
			    pos++;
			}
			while(opcstr[pos] != ']') pos++;
			pos++;
		    }
		}
	    }
	    break;
	 default: abort();
	}
	/* safety check - might have changed */
	if (srcmode != Areg && srcmode != Dreg && srcmode != Aind
	    && srcmode != Ad16 && srcmode != Ad8r && srcmode != Aipi
	    && srcmode != Apdi && srcmode != immi)
	{
	    srcgather = 0;
	}
	if (srcmode == Areg && sz == sz_byte)
	    goto nomatch;

	if (opcstr[pos] != ',')
	    goto endofline;
	pos++;

	/* parse the destination address */
	usedst = 1;
	switch(opcstr[pos++]) {
	 case 'D':
	    destmode = Dreg;
	    switch (opcstr[pos++]) {
	     case 'r': destreg = bitval[bitr]; dstgather = 1; dstpos = bitpos[bitr]; break;
	     case 'R': destreg = bitval[bitR]; dstgather = 1; dstpos = bitpos[bitR]; break;
	     default: abort();
	    }
	    break;
	 case 'A':
	    destmode = Areg;
	    switch (opcstr[pos++]) {
	     case 'r': destreg = bitval[bitr]; dstgather = 1; dstpos = bitpos[bitr]; break;
	     case 'R': destreg = bitval[bitR]; dstgather = 1; dstpos = bitpos[bitR]; break;
	     default: abort();
	    }
	    switch (opcstr[pos]) {
	     case 'p': destmode = Apdi; pos++; break;
	     case 'P': destmode = Aipi; pos++; break;
	    }
	    break;
	 case '#':
	    switch(opcstr[pos++]) {
	     case 'z': destmode = imm; break;
	     case '0': destmode = imm0; break;
	     case '1': destmode = imm1; break;
	     case '2': destmode = imm2; break;
	     case 'i': destmode = immi; destreg = (S32)(S8)bitval[biti]; break;
	     case 'j': destmode = immi; destreg = bitval[bitj]; break;
	     case 'J': destmode = immi; destreg = bitval[bitJ]; break;
	     default: abort();
	    }
	    break;
	 case 'd':
	    destreg = bitval[bitD];
	    destmode = mode_from_mr(bitval[bitd],bitval[bitD]);
	    if(destmode == am_illg) continue;
	    if (CPU_EMU_SIZE < 1 &&
		(destmode == Areg || destmode == Dreg || destmode == Aind
		 || destmode == Ad16 || destmode == Ad8r || destmode == Aipi
		 || destmode == Apdi))
	    {
		dstgather = 1; dstpos = bitpos[bitD];
	    }

	    if (opcstr[pos] == '[') {
		pos++;
		if (opcstr[pos] == '!') {
		    /* exclusion */
		    do {
			pos++;
			if (mode_from_str(opcstr+pos) == destmode)
			    goto nomatch;
			pos += 4;
		    } while (opcstr[pos] == ',');
		    pos++;
		} else {
		    if (opcstr[pos+4] == '-') {
			/* replacement */
			if (mode_from_str(opcstr+pos) == destmode)
			    destmode = mode_from_str(opcstr+pos+5);
			else
			    goto nomatch;
			pos += 10;
		    } else {
			/* normal */
			while(mode_from_str(opcstr+pos) != destmode) {
			    pos += 4;
			    if (opcstr[pos] == ']')
				goto nomatch;
			    pos++;
			}
			while(opcstr[pos] != ']') pos++;
			pos++;
			break;
		    }
		}
	    }
	    /* Some addressing modes are invalid as destination */
	    if (destmode == imm || destmode == PC16 || destmode == PC8r)
		goto nomatch;
	    break;
	 case 's':
	    destreg = bitval[bitS];
	    destmode = mode_from_mr(bitval[bits],bitval[bitS]);

	    if (destmode == am_illg) continue;
	    if (CPU_EMU_SIZE < 1 &&
		(destmode == Areg || destmode == Dreg || destmode == Aind
		 || destmode == Ad16 || destmode == Ad8r || destmode == Aipi
		 || destmode == Apdi))
	    {
		dstgather = 1; dstpos = bitpos[bitS];
	    }

	    if (opcstr[pos] == '[') {
		pos++;
		if (opcstr[pos] == '!') {
		    /* exclusion */
		    do {
			pos++;
			if (mode_from_str(opcstr+pos) == destmode)
			    goto nomatch;
			pos += 4;
		    } while (opcstr[pos] == ',');
		    pos++;
		} else {
		    if (opcstr[pos+4] == '-') {
			/* replacement */
			if (mode_from_str(opcstr+pos) == destmode)
			    destmode = mode_from_str(opcstr+pos+5);
			else
			    goto nomatch;
			pos += 10;
		    } else {
			/* normal */
			while(mode_from_str(opcstr+pos) != destmode) {
			    pos += 4;
			    if (opcstr[pos] == ']')
				goto nomatch;
			    pos++;
			}
			while(opcstr[pos] != ']') pos++;
			pos++;
		    }
		}
	    }
	    break;
	 default: abort();
	}
	/* safety check - might have changed */
	if (destmode != Areg && destmode != Dreg && destmode != Aind
	    && destmode != Ad16 && destmode != Ad8r && destmode != Aipi
	    && destmode != Apdi)
	{
	    dstgather = 0;
	}

	if (destmode == Areg && sz == sz_byte)
	    goto nomatch;
#if 0
	if (sz == sz_byte && (destmode == Aipi || destmode == Apdi)) {
	    dstgather = 0;
	}
#endif
	endofline:
	/* now, we have a match */
	if (table68k[opc].mnemo != i_ILLG)
	    fprintf(stderr, "Double match: %x: %s\n", opc, opcstr);
	for(find = 0;; find++) {
	    if (strcmp(mnemonic, lookuptab[find].name) == 0) {
		table68k[opc].mnemo = lookuptab[find].mnemo;
		break;
	    }
	    if (strlen(lookuptab[find].name) == 0) abort();
	}
	table68k[opc].cc = bitval[bitc];
	if (table68k[opc].mnemo == i_BTST
	    || table68k[opc].mnemo == i_BSET
	    || table68k[opc].mnemo == i_BCLR
	    || table68k[opc].mnemo == i_BCHG)
	{
	    sz = destmode == Dreg ? sz_long : sz_byte;
	}
	table68k[opc].size = sz;
	table68k[opc].sreg = srcreg;
	table68k[opc].dreg = destreg;
	table68k[opc].smode = srcmode;
	table68k[opc].dmode = destmode;
	table68k[opc].spos = srcgather ? srcpos : -1;
	table68k[opc].dpos = dstgather ? dstpos : -1;
	table68k[opc].suse = usesrc;
	table68k[opc].duse = usedst;
	table68k[opc].stype = srctype;
	table68k[opc].plev = id.plevel;
	nomatch:
	/* FOO! */;
    }
}

/* return 0 if successful */
int read_table68k ()
{
    int i;

    table68k = (struct instr *)malloc (65536 * sizeof (struct instr));
/*082f*/
//    if (!table68k) {fprintf(stderr,"Out of virtual memory t68k, exiting\n");exit(1);}
/*086a*/
    if (!table68k) return 1;
    for(i = 0; i < 65536; i++) {
	table68k[i].mnemo = i_ILLG;
	table68k[i].handler = -1;
    }
    for (i = 0; i < n_defs68k; i++) {
	build_insn (i);
    }
    return 0;
}

static int mismatch;

static void handle_merges(long int opcode)
{
    U16 smsk = 0;
    U16 dmsk;
    int sbitsrc, sbitdst = 0;
    int srcreg, dstreg;

    switch (table68k[opcode].stype) {
     case 0:
	smsk = 7; sbitdst = 8; break;
     case 1:
	smsk = 255; sbitdst = 256; break;
     case 2:
	smsk = 15; sbitdst = 16; break;
     case 3:
	smsk = 7; sbitdst = 8; break;
    }
    smsk <<= table68k[opcode].spos;
    dmsk = 7 << table68k[opcode].dpos;
    sbitsrc = table68k[opcode].spos == -1 ? 0 : 0;
    if (table68k[opcode].spos == -1) sbitdst = 1;
    for (srcreg=sbitsrc; srcreg < sbitdst; srcreg++) {
	for (dstreg=0; dstreg < (table68k[opcode].dpos == -1 ? 1 : 8); dstreg++) {
	    U16 code = opcode;

	    if (table68k[opcode].spos != -1) code &= ~(smsk << table68k[opcode].spos);
	    if (table68k[opcode].dpos != -1) code &= ~(dmsk << table68k[opcode].dpos);

	    if (table68k[opcode].spos != -1) code |= srcreg << table68k[opcode].spos;
	    if (table68k[opcode].dpos != -1) code |= dstreg << table68k[opcode].dpos;

	    /* Check whether this is in fact the same instruction.
	     * The instructions should never differ, except for the
	     * Bcc.(BW) case. */
	    if (table68k[code].mnemo != table68k[opcode].mnemo
		|| table68k[code].size != table68k[opcode].size
		|| table68k[code].suse != table68k[opcode].suse
		|| table68k[code].duse != table68k[opcode].duse)
	    {
		mismatch++; continue;
	    }
	    if (table68k[opcode].suse
		&& (table68k[opcode].spos != table68k[code].spos
		    || table68k[opcode].smode != table68k[code].smode
		    || table68k[opcode].stype != table68k[code].stype))
	    {
		mismatch++; continue;
	    }
	    if (table68k[opcode].duse
		&& (table68k[opcode].dpos != table68k[code].dpos
		    || table68k[opcode].dmode != table68k[code].dmode))
	    {
		mismatch++; continue;
	    }

	    if (code != opcode)
		table68k[code].handler = opcode;
	}
    }
}

void do_merges ()
{
    long int opcode;
    mismatch = 0;

    for (opcode = 0; opcode < 65536; opcode++) {
	if (table68k[opcode].handler != -1 || table68k[opcode].mnemo == i_ILLG)
	    continue;
	handle_merges (opcode);
    }

}

int get_no_mismatches ()
{
    return mismatch;
}
