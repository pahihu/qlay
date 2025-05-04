/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	QLAY Win95 specifics
*/

#include "sysconfig.h"
#include "sysdeps.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
//#include <pc.h>
#include "gettimeofday.h"

/*
#include <dpmi.h>
#include <sys/nearptr.h>
#include <sys/exceptn.h>
#include <conio.h>
#include <sys/farptr.h>
*/

#include "options.h"
#include "qlmem.h"
#include "exe68k.h"
#include "keybuf.h"
#include "spc-os.h"
//#include "pckeys.h"
//#include "qlkeys.h"
#include "qlvers.h"
#include "qlio.h"
#include "winmain.h"

static void redraw_screen(void);
static void plot_6(int x, int y, int c);
static void put8dots(int target_x, int target_y, U16 w);
static void put8dots_asp(int target_x, int target_y, U16 w);
static void put8dots_asp4(int target_x, int target_y, U16 w);
static void put8dots_f(int target_x, int target_y, U16 w);
static void put8dots_f4(int target_x, int target_y, U16 w);
static void put8dots_f4f(int target_x, int target_y, U16 w);
static void put8dots_f4s(int target_x, int target_y, U16 w);
static void set_palette(int c, int r, int g, int b);
static void set_palette_table(int Color, int Red, int Green, int Blue);
static void load_palette(void);
void my_kbd_handler(int); /*win32!*/
//088 void my_mouse_handler(_go32_dpmi_registers *mouse_regs);

static int scrmode=0;	/* 18063 screen mode register */
static int cas_key;
unsigned char palette_data[768];
//088 _go32_dpmi_seginfo palette_mem;
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

	target_y=a/0x80;
	target_x=(a&0x7f)<<2;
//	put8dots_f4s(target_x,target_y,w);
	put8dots_f4f(target_x,target_y,w);
	return;

//
#if 0
        target_y=a/0x80+v_offset;
        target_x=((a&0x7f)<<2)+h_offset;
	if (screen_res>=6) {
		if (colors256) put8dots_asp(target_x-h_offset,target_y-v_offset,w);
		else put8dots_asp4(target_x-h_offset,target_y-v_offset,w);
	} else {
		if (opt_new_gfx) {
			if (colors256) put8dots_f(target_x,target_y,w);
			else put8dots_f4(target_x,target_y,w); /* ? */
		} else {
			/* compatibility, slow mode */
			if (colors256) put8dots(target_x,target_y,w);
			else put8dots_f4s(target_x,target_y,w);
		}
	}
#endif
}



int mode4_col[8]={0,249,250,255, 251,252,253,254};
//int mode8_col[16]={0,252,249,251,250,253,254,255 ,250,253,254,255,0,252,249,251};
int mode8_col[16]={0,252,249,253,250,253,254,255 ,250,254,251,255,0,252,249,251};

/* slow and save, 4 bit color */
void put8dots_f4s(int target_x, int target_y, U16 w)
{
int i;
long colbit;

	if ((scrmode&8)==0) {/* mode 4 */
		for(i=target_x;i<target_x+8;) {
			colbit=((w&0x8000)>>14)|((w&0x0080)>>7);
			win32_plotp(i++,target_y,mode4_col[colbit]);
			w<<=1;
		}
	} else {
		for(i=target_x;i<target_x+8;) {
			colbit=((w&0xc000)>>12)|((w&0x00c0)>>6);
			win32_plotp(i++,target_y,mode8_col[colbit]);
			win32_plotp(i++,target_y,mode8_col[colbit]);
			w<<=2;
		}
	}
}

/* slow and save, 4 bit color */
void put8dots_f4f(int target_x, int target_y, U16 w)
{
int i;
long colbit;
static U8 colbuf[8];
U8 *addr;

	addr=colbuf;
	if ((scrmode&8)==0) {/* mode 4 */
		for(i=0;i<8;i++) {
			colbit=((w&0x8000)>>14)|((w&0x0080)>>7);
			*addr++=mode4_col[colbit];
			w<<=1;
		}
	} else {
		for(i=0;i<8;i+=2) {
			colbit=((w&0xc000)>>12)|((w&0x00c0)>>6);
			*addr++=mode8_col[colbit];
			*addr++=mode8_col[colbit];
			w<<=2;
		}
	}
	win32_plotp8(target_x,target_y,colbuf);
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

//	GrFilledBoxNC(x2,y3,x2+1,y3+2,c);
	return;
//	GrPlot(x2,y3,c);
//	GrPlot(x2+1,y3,c);
//	GrPlot(x2,y3+1,c);
//	GrPlot(x2+1,y3+1,c);
//	GrPlot(x2,y3+2,c);
//	GrPlot(x2+1,y3+2,c);
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

//	if (size)
//		for(v=0;v<4;v++) for(h=0;h<6;h++) GrPlot(x+h+h_offset,y+v+v_offset,c);
//	else
//		for(v=3;v<4;v++) for(h=0;h<6;h++) GrPlot(x+h+h_offset,y+v+v_offset,c);
}

void set_palette(int c, int r, int g, int b) {
}

void set_palette_table(int c, int r, int g, int b) {
}

void load_palette(void) {
}

/* control mode8 FLASH, called at 50Hz interrupt */
void flash_colors()
{
static int flash=0;
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

	return;
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
//win32?!	for (i=0;i<32;i++) set_palette_table(i,0,0,0);
	} else {
		flush_vidbuf(mode);	// needed for mode4 mode8 980513
	}
}

int buttonstate[3] = { 0, 0, 0 };
int lastmx, lastmy;
int newmousecounters = 0;

#define KEYSBUF 0x800
static int keystate[KEYSBUF];
static int keyspressed;

static int ledsstate;

void my_kbd_handler(int wc)
{
static int is_escape = 0;
int scancode, newstate, akey;

/*
	scancode = inportb(0x60);
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
*/
	scancode = wc & 0x7f;
	if (wc & 0x100 ) {
		scancode = escape_keys[scancode];
	}
	newstate = 1;
	wc&=0x8000;
	if (wc & 0x8000) newstate=0;

//fpr("SC %02x, NS %02x -> ",scancode,newstate);

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
			cas_key=0x03;
			specialflags |= SPCFLAG_CAS_KEY;
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

void handle_reset()
{
	init_QL();
	MC68000_reset();
}

void handle_cas_key()
{
	if(cas_key==0x03) { /*csa X*/
		handle_exit();
		return;
	}

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
		return;
	}
}

void win_stop_emulation(void)
{
	specialflags |= SPCFLAG_BRK;
	return;
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
static int c50=0;
	c50++;
	// if (c50 == 4) { c50=0; repaint_screen(); }	/* 12.5 times a second */
	if (c50 == 2) { c50=0; repaint_screen(); }	/* 25 times a second */
	q_check_message();	/* win32 callback */
}

void get_mouse_state(int *x, int *y, int *b)
{
static	int px=0,py=0;	//previous position
static	int wait10=0;

// this f is called every 2 msec
// simulate dos, so that only every 20 msec (50Hz) a new position is notified

	if (wait10==0) {
		*x=mouse_xpos-px;
		*y=mouse_ypos-py;
		px=mouse_xpos;
		py=mouse_ypos;
		wait10=9;
	} else {
		*x=0;
		*y=0;
		wait10--;
	}
	*b=mouse_button;
}

//089 _go32_dpmi_seginfo old_kbd_handler, new_kbd_handler, mouse_handler;
//089 _go32_dpmi_registers mouse_callback_regs;

int init_graphics(void)
{
    int i;
    int found_mode = 0;
    int gw, gh, gc, gbpp;


    fpr("Build: %s\n",qlayversion());
    fpr("Options: d%d o%d f%d %sw%d m%d\n",
    	screen_res,opt_new_gfx,qlay1msec,opt_throttle ? "t " : "",opt_busy_wait,ram_size);

    cas_key=0;

/*
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
*/
    fpr("Installed keyboard driver.\n");
/*
    if (opt_use_mouse) {
	    regs.x.ax=0;
	    regs.x.ss=regs.x.sp=regs.x.flags=0;
	    _go32_dpmi_simulate_int(0x33, &regs);
	    if (regs.x.ax==0) {
		fpr("Mouse driver not present, skipping.\n");
		opt_use_mouse=0;
	    }
    }
*/

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
/*
    palette_mem.size = 48;
    if (_go32_dpmi_allocate_dos_memory(&palette_mem)) {
	fpr("Can't allocate real mode memory for palette data.\n");
	return(0);
    }
*/

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

/*
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
*/
//    fpr("Installing graphics driver: W %d, H %d, C %d.\n",gw,gh,gc);
/*
    if (GrSetMode(GR_width_height_color_graphics, gw, gh, gc) == 0) {
	_GrDriverInfo.vdriver = NULL;
	fpr("Graphics initialization error.\n");
	return 0;
    }

    if (screen_res==1) set_vga16();
*/
    screenwidth  = gw;
    screenheight = gh;
    screencolors = gc;
/*
    init_colors();
*/
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
/*
	GrSetMode(GR_width_height_color_graphics, screenwidth, screenheight, screencolors);
	if (screen_res==1) set_vga16();
	init_colors();
	flush_vidbuf(scrmode);
*/
}

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include "snd-win.h"
#endif

void start_speaker()
{
/* nop */
	snd_open(globalhwnd);
}

void do_speaker(int pitch,int gradx)
{
	static int toggle=0;
	static int ppitch=0,pgradx=0;
	float freq, durationMS;

	toggle^=0x02;
#if defined(_WIN32)
	// fpr("pitch=%d gradx=%d\n",pitch,gradx);
	if (ppitch==pitch&&pgradx==gradx)
		return;
	ppitch=pitch; pgradx=gradx;
	freq = 11447.0/(10.6+pitch); durationMS = (72.0 * gradx / 1000.0);
	if (durationMS > 1) {
		// fpr("do_beep\n");
		snd_play(freq, durationMS);
	}
#else
	outportb(0x61,(inportb(0x61)&0xfd)|toggle); /* cli/sti needed? */
#endif
}

/* kill it in a nice but quiet way ... :=< */
void stop_speaker()
{
#if defined(_WIN32)
	snd_stop();
#else
	outportb(0x61,inportb(0x61)&0xfd); /* cli/sti needed? */
#endif
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
	write_box(ln);
	return res;
}
