/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	QLAY MS-DOS specifics
*/

#include "sysconfig.h"
#include "sysdeps.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <grx20.h>
#include <pc.h>
#include <go32.h>
#include <dpmi.h>
#include <sys/nearptr.h>
#include <sys/exceptn.h>
#include <conio.h>
#include <sys/farptr.h>

#include "options.h"
#include "qlmem.h"
#include "exe68k.h"
#include "keybuf.h"
#include "spc-os.h"
#include "qlvers.h"
#include "qlio.h"
#include "ser-os.h"

static void redraw_screen(void);
static void plot_6(int x, int y, int c);
static void put8dots(int target_x, int target_y, U16 w);
static void put8dots_asp(int target_x, int target_y, U16 w);
static void put8dots_asp4(int target_x, int target_y, U16 w);
static void put8dots_f(int target_x, int target_y, U16 w);
static void put8dots_f4(int target_x, int target_y, U16 w);
static void put8dots_f4s(int target_x, int target_y, U16 w);
static void set_palette(int c, int r, int g, int b);
static void set_palette_table(int Color, int Red, int Green, int Blue);
static void load_palette(void);
static void my_kbd_handler(void);
static void my_mouse_handler(_go32_dpmi_registers *mouse_regs);

static int scrmode=0;	/* 18063 screen mode register */
static int cas_key;
unsigned char palette_data[768];
_go32_dpmi_seginfo palette_mem;
static FILE *logfile;

extern struct _GR_driverInfo _GrDriverInfo;
extern struct _GR_contextInfo _GrContextInfo;

#define BANKPOS(offs)   ((unsigned short)(offs))
#define BANKNUM(offs)   (((unsigned short *)(&(offs)))[1])
#define BANKLFT(offs)   (0x10000 - BANKPOS(offs))

/* Command Flags */
#define KEY_EVENTRELEASE 0
#define KEY_EVENTPRESS 1

#define ResetCPU    0x001
#define EjectDisk0  0x002
#define EjectDisk1  0x004
#define EjectDisk2  0x008
#define EjectDisk3  0x010
#define ChangeDisk0 0x020
#define ChangeDisk1 0x040
#define ChangeDisk2 0x080
#define ChangeDisk3 0x100


static int vsize;
static int linebytes;
static int screenwidth;
static int screenheight;
static int screencolors;
static int colors256;
static int CommandFlags = 0;


#define h_offset ((640-512)/2)
#define v_offset ((480-256)/4)

int no_screen_refresh=0;

void do_scrn(A32 a, U16 w)
{
int target_y, target_x;

	if (no_screen_refresh) return;
	if (scrmode&0x80) {
		if (a<0x028000) return;
	} else {
		if (a>=0x028000) return;
	}
	if (use_debugger) return; /* skip if debug */
	a=a&0x7fff;
        target_y=a/0x80+v_offset;
        target_x=((a&0x7f)<<2)+h_offset;
	if (screen_res>=6) {
		if (colors256) put8dots_asp(target_x-h_offset,target_y-v_offset,w);
		else put8dots_asp4(target_x-h_offset,target_y-v_offset,w);
	} else {
		if (opt_new_gfx) {
			if (colors256) put8dots_f(target_x,target_y,w);
			else put8dots_f4(target_x,target_y,w);
		} else {
			/* compatibility, slow mode */
			if (colors256) put8dots(target_x,target_y,w);
			else put8dots_f4s(target_x,target_y,w);
		}
	}
}

void put8dots_f(int target_x, int target_y, U16 w)
{
int i;
long colbit;

static char colbuf[8];
static char *addr, *ptr;
long offs;
int page, left;

	offs = target_y * _GrContextInfo.current.gc_lineoffset + target_x;
	ptr = &_GrContextInfo.current.gc_baseaddr[0][BANKPOS(offs)];
	page = BANKNUM(offs);
	left = BANKLFT(offs);
	if (page != _GrDriverInfo.curbank) {
		_GrDriverInfo.curbank = page;
		(*_GrDriverInfo.setbank)(page);
	}
	if (left < 8) fpr("VidError%d<8 ",left);

	addr=colbuf;
	for(i=0;i<8;i+=2) {
		colbit=((w&0xc000)>>12)|((w&0x00c0)>>6);
		*addr++=colbit;
		*addr++=colbit+16;
		w<<=2;
	}
/*	addr=colbuf;*/
	asm ("
		push %%es
		cld
		movw %0, %%es
		shrl $2, %%ecx
		rep
		movsl
		pop %%es
		"
		:
		: "g" (_GrContextInfo.current.gc_selector),
		  "S" (colbuf), "D" (ptr), "c" (8)
		: "%esi", "%edi", "%ecx", "cc"
	);
}

void put8dots_f4(int target_x, int target_y, U16 w)
{
int i;
long colbit;
U8 c;
#define DRVINFO		(&_GrDriverInfo)
#define CXTINFO		(&_GrContextInfo)
#define CLRINFO		(&_GrColorInfo)
#define MOUINFO		(&_GrMouseInfo)

#define CURC		(&(CXTINFO->current))
#define SCRN		(&(CXTINFO->screen))
#define FDRV		(&(DRVINFO->fdriver))
#define SDRV		(&(DRVINFO->sdriver))
#define VDRV		( (DRVINFO->vdriver))
#define setup_far_selector(S) ({			\
    __asm__ volatile(					\
	"movw %0,%%fs"					\
	: /* no outputs */				\
	: "r" ((unsigned short)(S))			\
    );							\
})

	char/*U8*/ far *ptr = &CURC->gc_baseaddr[0][(target_y * SCRN->gc_lineoffset) + (target_x >> 3)];
	if ((scrmode&8)==0) {/* mode 4 */

		outportw(0x3c4, 0x102);
		_farpokeb(CURC->gc_selector,(U32)ptr, w);
		outportw(0x3c4, 0x202);
		_farpokeb(CURC->gc_selector,(U32)ptr, w>>8);
		outportw(0x3c4, 0x402);
		_farpokeb(CURC->gc_selector,(U32)ptr, 0);
		outportw(0x3c4, 0x802);
		_farpokeb(CURC->gc_selector,(U32)ptr, 0);

	} else { /* mode 8 */
		U8 t;
		outportw(0x3c4, 0x102);
		t=w&0x0055;
		_farpokeb(CURC->gc_selector,(U32)ptr, t|(t<<1));
		outportw(0x3c4, 0x202);
		t=w&0x00aa;
		_farpokeb(CURC->gc_selector,(U32)ptr, t|(t>>1));
		outportw(0x3c4, 0x402);
		_farpokeb(CURC->gc_selector,(U32)ptr, 0);
		outportw(0x3c4, 0x802);
		t=(w>>8)&0x00aa;
		_farpokeb(CURC->gc_selector,(U32)ptr, t|(t>>1));
	}
}

/* slow and save, 4 bit color */
void put8dots_f4s(int target_x, int target_y, U16 w)
{
int i;
long colbit;

	if ((scrmode&8)==0) {/* mode 4 */
		for(i=target_x;i<target_x+8;) {
			colbit=((w&0x8000)>>14)|((w&0x0080)>>7);
			GrPlotNC(i++,target_y,colbit);
			w<<=1;
		}
	} else {
		for(i=target_x;i<target_x+8;) {
			colbit=((w&0xc000)>>12)|((w&0x00c0)>>6);
			GrPlotNC(i++,target_y,colbit);
			GrPlotNC(i++,target_y,colbit);
			w<<=2;
		}
	}
}

void put8dots(int target_x, int target_y, U16 w)
{
int i;
long colbit;

	for(i=target_x;i<target_x+8;) {
		colbit=((w&0xc000)>>12)|((w&0x00c0)>>6);
		GrPlotNC(i++,target_y,colbit);
		GrPlotNC(i++,target_y,colbit+16);
		w<<=2;
	}
}

/* correct aspect ratio on 1024x768, 256 color screen */
void put8dots_asp(int target_x, int target_y, U16 w)
{
int i,colbit;

	for(i=target_x;i<target_x+8;) {
		colbit=((w&0xc000)>>12)|((w&0x00c0)>>6);
		plot_6(i++,target_y,colbit);
		plot_6(i++,target_y,colbit+16);
		w<<=2;
	}
}

/* correct aspect ratio on 1024x768, 16 color screen */
void put8dots_asp4(int target_x, int target_y, U16 w)
{
int i,colbit;

	if ((scrmode&8)==0) {/* mode 4 */
		for(i=target_x;i<target_x+8;) {
			colbit=((w&0x8000)>>14)|((w&0x0080)>>7);
			plot_6(i++,target_y,colbit);
			w<<=1;
		}
	} else {
		for(i=target_x;i<target_x+8;) {
			colbit=((w&0xc000)>>12)|((w&0x00c0)>>6);
			plot_6(i++,target_y,colbit);
			plot_6(i++,target_y,colbit);
			w<<=2;
		}
	}
}

void plot_6(int x, int y, int c)
{
int x2=x*2;
int y3=y*3;

	GrFilledBoxNC(x2,y3,x2+1,y3+2,c);
	return;
	GrPlot(x2,y3,c);
	GrPlot(x2+1,y3,c);
	GrPlot(x2,y3+1,c);
	GrPlot(x2+1,y3+1,c);
	GrPlot(x2,y3+2,c);
	GrPlot(x2+1,y3+2,c);
}

/* draw a LED on pos x,y, color c; size=0 (bar) or 1 (full) */
void draw_LED(int x, int y, long c, int size)
{
int h,v;

if(screen_res==1)return; /* VGA16testing*/
	if(screen_res>5)return;
	if(colors256) {
		if (c==2) c=32;	/* red */
		if (c==6) c=33; /* yellow */
	}

	if (size)
		for(v=0;v<4;v++) for(h=0;h<6;h++) GrPlot(x+h+h_offset,y+v+v_offset,c);
	else
		for(v=3;v<4;v++) for(h=0;h<6;h++) GrPlot(x+h+h_offset,y+v+v_offset,c);
}

void set_palette(int c, int r, int g, int b) {
_go32_dpmi_registers regs;

	if (!colors256) {
		if (c>15) return;
		regs.h.ah = 0x10;
		regs.h.al = 0x00;
		regs.h.bl = c; /* col: 0..15 */
		regs.h.bh = c; /* pal: 0..63 */
		regs.x.ss = regs.x.sp = regs.x.flags = 0;
		_go32_dpmi_simulate_int(0x10, &regs);

	}
	regs.h.ah = 0x10;
	regs.h.al = 0x10;
	regs.x.bx = c;
	regs.h.dh = r;
	regs.h.ch = g;
	regs.h.cl = b;
	regs.x.ss = regs.x.sp = regs.x.flags = 0;
	_go32_dpmi_simulate_int(0x10, &regs);
}

void set_palette_table(int c, int r, int g, int b) {
	palette_data[c * 3 + 0] = r;
	palette_data[c * 3 + 1] = g;
	palette_data[c * 3 + 2] = b;
}

void load_palette(void) {
    _go32_dpmi_registers regs;
    int i;

    dosmemput(palette_data, 768, palette_mem.rm_segment*16);
    if (!colors256) {
	for(i=0; i<16; i++) {
	    regs.h.ah = 0x10;
	    regs.h.al = 0x00;
	    regs.h.bh = i;
	    regs.h.bl = i;
	    regs.x.ss = regs.x.sp = regs.x.flags = 0;
	    _go32_dpmi_simulate_int(0x10, &regs);
	}
    }
    regs.h.ah = 0x10;
    regs.h.al = 0x12;
    regs.x.bx = 0;
    if (!colors256)
	regs.x.cx = 16;
    else
	regs.x.cx = 256;
    regs.x.es = palette_mem.rm_segment;
    regs.x.dx = 0x0000;
    regs.x.ss = regs.x.sp = regs.x.flags = 0;
    _go32_dpmi_simulate_int(0x10, &regs);
}

/* control mode8 FLASH, called at 50Hz interrupt */
void flash_colors()
{
static flash=0;
int i;
#define FLASH_DELAY 12


/* needs to be updated 19970627! */
   	if (scrmode==0) return;
	flash++;
	if (flash>2*FLASH_DELAY) flash=0;
	for (i=8;i<16;i++) {
		if (flash>FLASH_DELAY)
		  set_palette_table(i,i&2?255:0,i&4?255:0,i&1?255:0);
		else
		  set_palette_table(i,0,0,0);
	}
	load_palette();
}

static void init_colors(void)
{
int i;

	if(!colors256) { /* w.h.16*/
		if ((scrmode&8)==0) {/* mode 4 */
			for(i=0;i<16;i++) {
				set_palette_table(i,i&1?255:0,i&2?255:0,((i&3)==3)?255:0);/* r, g, b */
			}
		} else { /* mode 8 */
			for(i=0;i<16;i++) {
				set_palette_table(i,i&2?255:0,i&8?255:0,i&1?255:0);/* r, g, b */
			}
		}
		load_palette();
		flush_vidbuf(scrmode);
		return;
	} else { /* 256 colors continue here */

		if ((scrmode&8)==0) {/* mode 4 */
			for (i=0;i<16;i++) {
				set_palette_table(i,i&2?255:0,i&8?255:0,((i&0xa)==0xa)?255:0); /* r, g, b */
			}
			for (i=16;i<32;i++) {
				set_palette_table(i,i&1?255:0,i&4?255:0,((i&5)==5)?255:0); /* r, g, b */
			}
		} else {
			for (i=0;i<32;i++) {
				set_palette_table(i,i&2?255:0,i&8?255:0,i&1?255:0);/* r, g, b */
			}
		}
		set_palette_table(32,255,0,0);		/* red LED */
		set_palette_table(33,255,255,0);	/* yellow LED */
	}
	load_palette();
}

void do_scrmode(int mode)
{
int i;

	if ((scrmode&0x80)!=(mode&0x80)) {scrmode=mode;flush_vidbuf(scrmode);}
	scrmode=mode;
	if (mode&0x2) {	/* screen blank */
		for (i=0;i<32;i++) set_palette_table(i,0,0,0);
	} else {
		init_colors();
	}
	load_palette();
}

int buttonstate[3] = { 0, 0, 0 };
int lastmx, lastmy;
int newmousecounters = 0;

#define KEYSBUF 0x800
static int keystate[KEYSBUF];
static int keyspressed;

static int ledsstate;

void my_kbd_handler(void)
{
static int is_escape = 0;
int scancode, newstate, akey;

	scancode = inportb(0x60);
/*fpr("S1: %02x ",scancode);*/
	if (scancode == 0xe0) {
		is_escape = 1;
		outportb(0x20, 0x20);
		return;
	}
	newstate = !(scancode & 0x80);
	scancode = scancode & 0x7f;
	if (is_escape) {
		scancode = escape_keys[scancode];
		is_escape = 0;
	}
	outportb(0x20, 0x20);

/*fpr("%02x %02x ",scancode,newstate);*/

	akey = pcsc_ql(scancode);

	if (akey == -1)	return;

	if (akey <= 0x800) {
		if (keystate[akey] == newstate) return;
		keystate[akey] = newstate;	/* clr QL_KP */
		if (akey&0x7f) {/* not just shift,ctl,alt or QL_KP */
			if (newstate) keyspressed++; else keyspressed--;
		}
/*		fpr("KP%d ",keyspressed);*/
    }

	/* update shift,ctl,alt keys in keystate[] for keyrow */
	keystate[2]=keystate[0x100]|keystate[0x180];	/* KP */
	keystate[1]=keystate[0x200];
	keystate[0]=keystate[0x400];

	if((newstate==KEY_EVENTPRESS)&&(keystate[0]&&keystate[1]&&keystate[2])) {
		if(keystate[0x1e]) { /*csa D*/
			cas_key=0x1e;
			specialflags |= SPCFLAG_CAS_KEY;
			return;
		}
		if(keystate[0x03]) { /*csa X*/
			specialflags |= SPCFLAG_BRK;
			return;
		}
		if(keystate[0x18]) { /*csa L*/
			cas_key=0x18;
			specialflags |= SPCFLAG_CAS_KEY;
			return;
		}
		if(keystate[0x14]) { /*csa R*/
			cas_key=0x14;
			specialflags |= SPCFLAG_CAS_KEY;
			return;
		}
		if(keystate[0x2c]) { /*csa B*/
			cas_key=0x2c;
			specialflags |= SPCFLAG_CAS_KEY;
			return;
		}
		if(keystate[0x24]) { /*csa F*/
			cas_key=0x24;
			specialflags |= SPCFLAG_CAS_KEY;
			return;
		}
	}

    if (newstate == KEY_EVENTPRESS) {
		do_keyboard();	/* generate interrupt */

//if (akey==0x29) trigger_trduco(); /* 'z' */

		if (akey<0x80) {
			if (keystate[0x180]==1) akey+=0x180;
			if (keystate[0x100]==1) akey+=0x100;
			if (keystate[0x200]==1) akey+=0x200;
			if (keystate[0x400]==1) akey+=0x400;
			record_key(akey);
		} else {
			if ((akey&0x780)!=akey) record_key(akey); /* eg: BS, DEL */
		}
	}

/* generate interrupt level 7? reset? */
/*
    if (keystate[AK_CTRL] && keystate[AK_LAMI] && keystate[AK_RAMI])
	CommandFlags |= ResetCPU;
    }
*/
}

void handle_cas_key()
{

	if(cas_key==0x1e) { /*csa D*/
		dump_memory();
		return;
	}
	if(cas_key==0x18) { /*csa L*/
		redraw_screen();
		update_LED();
		return;
	}
	if(cas_key==0x14) { /*csa R*/
		init_QL();
		MC68000_reset();
		return;
	}
	if(cas_key==0x2c) { /*csa B*/
		no_screen_refresh=1-no_screen_refresh;
		fpr("NSR%d ",no_screen_refresh);
		if (!no_screen_refresh) {
			flush_vidbuf(scrmode);
			update_LED();
		}
		return;
	}
	if(cas_key==0x24) { /*csa F*/
//		fflush(logfile);
//		freopen(LOG_FILE, "a", logfile);
		fpr("Pressed cas f\n");
		return;
	}
}

int get_keyrow(int row)
{
int b,rv;

	row&=7;
	row=7-row;
	row<<=3;
	rv=0;
	for(b=7;b>-1;b--) {
		rv<<=1;
		if(keystate[row+b]) rv++;
	}
	return rv;
}

int k_press()
{
	return(keyspressed);
}

void handle_events_50Hz(void)
{
	ser_rcv_thread();
}

void my_mouse_handler(_go32_dpmi_registers *mouse_regs)
{
    lastmx = (short)mouse_regs->x.si;
    lastmy = (short)mouse_regs->x.di;
    buttonstate[0] = mouse_regs->x.bx & 1;
    buttonstate[1] = mouse_regs->x.bx & 4;
    buttonstate[2] = mouse_regs->x.bx & 2;

	do_mouse(lastmx,lastmy,mouse_regs->x.bx);
}

static void set_vga16(void)
{
_go32_dpmi_registers regs;

	if (!opt_new_gfx) return;
	regs.h.ah=0x00;
	regs.h.al=0x10;
	regs.x.ss=regs.x.sp=regs.x.flags=0;
	_go32_dpmi_simulate_int(0x10, &regs);
}

void get_mouse_state(int *x, int *y, int *b)
{
_go32_dpmi_registers regs;

/*	memset(&regs,0,sizeof(regs));*/
	regs.x.ax=0x03;
	regs.x.ss=regs.x.sp=regs.x.flags=0;
	_go32_dpmi_simulate_int(0x33, &regs);
	*b = (short)regs.x.bx;

	regs.x.ax=0x0b;
	regs.x.ss=regs.x.sp=regs.x.flags=0;
	_go32_dpmi_simulate_int(0x33, &regs);
	*x = (short)regs.x.cx;
	*y = (short)regs.x.dx;
}

_go32_dpmi_seginfo old_kbd_handler, new_kbd_handler, mouse_handler;
_go32_dpmi_registers mouse_callback_regs;

int init_graphics(void)
{
    int i;
    GrFrameMode fm;
    const GrVideoMode *mp;
    int found_mode = 0;
    int gw, gh, gc, gbpp;
    _go32_dpmi_registers regs;

    fpr("Build: %s\n",qlayversion());
    fpr("Options: d%d o%d f%d w%d m%d\n",
    	screen_res,opt_new_gfx,qlay1msec,opt_busy_wait,ram_size);

    cas_key=0;

    _go32_want_ctrl_break(1);
    __djgpp_set_ctrl_c(0);

    _go32_dpmi_get_protected_mode_interrupt_vector(9, &old_kbd_handler);
    new_kbd_handler.pm_offset = (int)my_kbd_handler;
    if (_go32_dpmi_allocate_iret_wrapper(&new_kbd_handler) != 0) {
	fpr("Can't allocate keyboard iret_wrapper.\n");
	return 0;
    }
    if (_go32_dpmi_set_protected_mode_interrupt_vector(9, &new_kbd_handler) != 0) {
	fpr("Can't set protected mode interrupt vector.\n");
	return 0;
    }
    fpr("Installed keyboard driver.\n");

    if (opt_use_mouse) {
	    regs.x.ax=0;
	    regs.x.ss=regs.x.sp=regs.x.flags=0;
	    _go32_dpmi_simulate_int(0x33, &regs);
	    if (regs.x.ax==0) {
		fpr("Mouse driver not present, skipping.\n");
		opt_use_mouse=0;
	    }
    }
/*
	if(opt_use_mouse)....
    mouse_handler.pm_offset = (int)my_mouse_handler;
    if (_go32_dpmi_allocate_real_mode_callback_retf(&mouse_handler, &mouse_callback_regs) != 0) {
	fpr("Can't allocate mouse callback_retf.\n");
	return 0;
    }
    regs.x.ax=0xc;
    regs.x.cx=0x7f;
    regs.x.es=mouse_handler.rm_segment;
    regs.x.dx=mouse_handler.rm_offset;
    regs.x.ss=regs.x.sp=regs.x.flags=0;
    _go32_dpmi_simulate_int(0x33, &regs);
*/

	if (opt_use_mouse) fpr("Installed mouse driver.\n");

    /* Allocate Palette Memory */
    palette_mem.size = 48;
    if (_go32_dpmi_allocate_dos_memory(&palette_mem)) {
	fpr("Can't allocate real mode memory for palette data.\n");
	return(0);
    }

    switch (screen_res) {
     case 1: gw=640;  gh=350; gc=16;  gbpp=4; break;
     case 2: gw=640;  gh=400; gc=256; gbpp=8; break;
     case 3: gw=640;  gh=480; gc=256; gbpp=8; break;
     case 4: gw=800;  gh=600; gc=256; gbpp=8; break;
     case 5: gw=800;  gh=600; gc=16;  gbpp=4; break;
     case 6: gw=1024; gh=768; gc=256; gbpp=8; break;
     case 7: gw=1024; gh=768; gc=16;  gbpp=4; break;

     default:
	fpr("Invalid screen mode.\n");
	return 0;
    }
    colors256=1;
    if (gc==16) colors256=0;
    if (screen_res>4) opt_new_gfx=0; /* 4 seems to work */


    GrSetDriver(NULL);
    if (GrCurrentVideoDriver() == NULL) {
	_GrDriverInfo.vdriver = NULL;
	fpr("Graphics adapter not supported.\n");
	return 0;
    }
    for (fm = GR_frameHERC1; (fm <= GR_frameSVGA32H) && (!found_mode); fm++) {
	for (mp = GrFirstVideoMode(fm); mp && (!found_mode); mp = GrNextVideoMode(mp)) {
	    if (gw==mp->width && gh==mp->height && gbpp==mp->bpp)
		found_mode = 1;
	}
    }
    if (!found_mode) {
	_GrDriverInfo.vdriver = NULL;
	fpr("Sorry, this combination of color and video mode is not available.\n");
	return 0;
    }

    fpr("Installing graphics driver: W %d, H %d, C %d.\n",gw,gh,gc);

    if (GrSetMode(GR_width_height_color_graphics, gw, gh, gc) == 0) {
	_GrDriverInfo.vdriver = NULL;
	fpr("Graphics initialization error.\n");
	return 0;
    }

    if (screen_res==1) set_vga16();

    screenwidth  = gw;
    screenheight = gh;
    screencolors = gc;
    init_colors();

    buttonstate[0] = buttonstate[1] = buttonstate[2] = 0;
    for(i = 0; i < KEYSBUF; i++)
		keystate[i] = 0;
	keyspressed=0;

    lastmx = lastmy = 0;
    newmousecounters = 0;

    fpr("Installed graphics driver.\n");

    return 1;
}

void redraw_screen()
{
	GrSetMode(GR_width_height_color_graphics, screenwidth, screenheight, screencolors);
	if (screen_res==1) set_vga16();
	init_colors();
	flush_vidbuf(scrmode);
}

void graphics_leave(void)
{
    _go32_dpmi_registers regs;

    if (_GrDriverInfo.vdriver) {
	_GrDriverInfo.vdriver->reset();
	_GrDriverInfo.vdriver = NULL;
    }
/*    freopen("CON", "w", stderr);*/
    _go32_dpmi_set_protected_mode_interrupt_vector(9, &old_kbd_handler);
    _go32_dpmi_free_iret_wrapper(&new_kbd_handler);
/*
	if(opt_use_mouse)....
    regs.x.ax=0xc;
    regs.x.cx=0x0;
    regs.x.ss=regs.x.sp=regs.x.flags=0;
    _go32_dpmi_simulate_int(0x33, &regs);
    _go32_dpmi_free_real_mode_callback(&mouse_handler);
*/

    _go32_dpmi_free_dos_memory(&palette_mem);

}

void start_speaker()
{
/* nop */
}

void do_speaker()
{
static int toggle=0;

	toggle^=0x02;
	outportb(0x61,(inportb(0x61)&0xfd)|toggle); /* cli/sti needed? */
}

/* kill it in a nice but quiet way ... :=< */
void stop_speaker()
{
	outportb(0x61,inportb(0x61)&0xfd); /* cli/sti needed? */
}

int open_log(void)
{
	remove(LOG_FILE);
	logfile = fopen(LOG_FILE, "w");
	return logfile != 0;
}

int close_log(void)
{
	fclose(logfile);
	return 0;
}

/* logfile print */
int fpr(char* fmt, ...)
{
int	res;
va_list l;

	va_start(l, fmt);
	res = vfprintf(logfile, fmt, l);
	fflush(logfile);			// really flush at all times
//	fsync(fileno(logfile));
	freopen(LOG_FILE, "a", logfile);	// enforce it
	va_end(l);
	return res;
}

/* stderr print */
int epr(char* fmt, ...)
{
int	res;
va_list l;
static	char	ln[1024];

	va_start(l, fmt);
	res = vsprintf(ln, fmt, l);
	va_end(l);
	fpr(ln);
	fprintf(stderr,"%s",ln);
	fflush(stderr);				// really flush at all times
	return res;
}
