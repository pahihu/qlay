/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	Win95 interface functions
*/

#define STRICT
// #define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmreg.h>
#include <stdlib.h>
#include <string.h>

#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "qlvers.h"
#include "spc-os.h"
#include "qlio.h"
#include "winmain.h"
#include "cfg-win.h"
#include "debug.h"
#include "snd-win.h"

#define WIN32COL 24

#ifdef __DJGPP__
# define DIB_RGB_COLORS 0	//missing from wingdi.h
# define DIB_PAL_COLORS 1	//..
# define CBM_INIT	0x4L	//.. not needed anymore?
#endif

#define qscr_w 512
#define qscr_h 256
#define tiles 8		// power of 2
#define log2tiles 3
#define MKTILE(x,y)	(((x) << 8) + (y))
#define TILEX(x)	((x) >> 8)
#define TILEY(x)	((x) & 255)

static void init_bmpi(void);
static void init_bmpit(void);
static void make_dirty(void);
static void test_pattern2(void);
static void update_size(void);
static void resize_screen(int);
static void exit_fn(void);
static void frame_sizes(int*,int*);
static void dump_screen_w(void);
static void dump_screen_wt(void);
static void clear_screen(int c);
static int win_alt_key(MSG*);
static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

/* externals */
int mouse_xpos;
int mouse_ypos;
int mouse_button;

/* locals */
static char szAppname[] = "QLAY";
HWND globalhwnd;

LPBITMAPINFO pbmpi;
static char bmpibuf[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)];
static char qscr_buf[qscr_w*qscr_h];

LPBITMAPINFO pbmpit;
static char bmpitbuf[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)];
static char qscrt_buf[tiles*tiles][qscr_w*qscr_h/(tiles*tiles)];
static unsigned char dirty[tiles][tiles];
static int ndirty;
static int dtiles[tiles*tiles];

static int win_w, win_h, wino_w, wino_h;
static HINSTANCE hInstance;
static int init_done;
static int emul_running;
static BOOL hideMouse;

static int i_maybe_bad;	// for atexit()

//def'd in grx-win, last halves don't care??
//int mode4_col[8]={0,249,250,255, 251,252,253,254};
//int mode8_col[16]={0,252,249,253,250,253,254,255 ,250,254,251,255,0,252,249,251};

//windows palette
RGBQUAD wincols16[]={		//blue,green,red,rsvd
	{  0,  0,  0,  0},	//0 black
	{  0,  0,128,  0},	//1 dark red
	{  0,128,  0,  0},	//2 dark green
	{  0,128,128,  0},	//3 dark yellow,brown
	{128,  0,  0,  0},	//4 dark blue
	{128,  0,128,  0},	//5 dark
	{128,128,  0,  0},	//6 dark
	{192,192,192,  0},	//7 lt grey
	{128,128,128,  0},	//248 dark grey
	{  0,  0,255,  0},	//249 red
	{  0,255,  0,  0},	//250 green
	{  0,255,255,  0},	//251 yellow
	{255,  0,  0,  0},	//252 blue
	{255,  0,255,  0},	//253 cyan
	{255,255,  0,  0},	//254 mint
	{255,255,255,  0}	//255 white
};

void init_bmpi(void){
HANDLE	hloc;
int	i;

	pbmpi = (LPBITMAPINFO) bmpibuf;
	pbmpi->bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
	pbmpi->bmiHeader.biWidth=qscr_w;
	pbmpi->bmiHeader.biHeight=qscr_h;
	pbmpi->bmiHeader.biPlanes=1;
	pbmpi->bmiHeader.biBitCount=8;
	pbmpi->bmiHeader.biCompression=BI_RGB;
	pbmpi->bmiHeader.biSizeImage=0;		//h*w implied
	pbmpi->bmiHeader.biXPelsPerMeter=1;
	pbmpi->bmiHeader.biYPelsPerMeter=1;
	pbmpi->bmiHeader.biClrUsed=256;
	pbmpi->bmiHeader.biClrImportant=0;

	// copy the color table 16 times to get 256 entries, mimic windows palette
	for(i=0;i<16;i++) {
		memcpy((pbmpi->bmiColors+4*sizeof(RGBQUAD)*i), wincols16, sizeof(RGBQUAD) * 16);
	}				//pointer to RGBQUAD: steps of 16 bytes!

}

void init_bmpit(void){
HANDLE	hloc;
int	i;

	pbmpit = (LPBITMAPINFO) bmpibuf;
	pbmpit->bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
	pbmpit->bmiHeader.biWidth=qscr_w/tiles;
	pbmpit->bmiHeader.biHeight=qscr_h/tiles;
	pbmpit->bmiHeader.biPlanes=1;
	pbmpit->bmiHeader.biBitCount=8;
	pbmpit->bmiHeader.biCompression=BI_RGB;
	pbmpit->bmiHeader.biSizeImage=0;		//h*w implied
	pbmpit->bmiHeader.biXPelsPerMeter=1;
	pbmpit->bmiHeader.biYPelsPerMeter=1;
	pbmpit->bmiHeader.biClrUsed=256;
	pbmpit->bmiHeader.biClrImportant=0;

	// copy the color table 16 times to get 256 entries, mimic windows palette
	for(i=0;i<16;i++) {
		memcpy((pbmpit->bmiColors+4*sizeof(RGBQUAD)*i), wincols16, sizeof(RGBQUAD) * 16);
	}				//pointer to RGBQUAD: steps of 16 bytes!

}

void make_dirty(void)
{
int x,y;

	ndirty=0;
	for(x=0;x<tiles;x++)
		for(y=0;y<tiles;y++) {
			dirty[x][y]=1;
			dtiles[ndirty++]=MKTILE(x,y);
		}
}

/* input 0: black, else white */
void clear_screen(int c)
{
HANDLE	hOldBrush;
POINT	p;
HDC	hDC;

	hDC = GetDC(globalhwnd);
	if (c) hOldBrush = SelectObject(hDC,CreateSolidBrush(RGB(255,255,255)));
	else hOldBrush = SelectObject(hDC,CreateSolidBrush(RGB(0,0,0)));
	PatBlt(hDC, 0, 0, win_w, win_h, PATCOPY);
	SelectObject(hDC, hOldBrush);
	ReleaseDC(globalhwnd, hDC);
}

void test_pattern2(void)
{
int i,j;

	for (i=0;i<qscr_h;i++) {
		for (j=0;j<qscr_w;j++) {
			if (i<128) qscr_buf[i*qscr_w+j]=i/16;
			else qscr_buf[i*qscr_w+j]=240+i/16;
//			qscr_buf[i*qscr_w+j]=i;
		}
	}
}

void frame_sizes(int *x, int *y){

	*x=0;
//	*x+=2*GetSystemMetrics(SM_CXBORDER)
	*x+=2*GetSystemMetrics(SM_CXFRAME);
	*y=0;
//	*y+=2*GetSystemMetrics(SM_CYBORDER);
	*y+=2*GetSystemMetrics(SM_CYFRAME);
	*y+=GetSystemMetrics(SM_CYCAPTION);
	*y+=GetSystemMetrics(SM_CYMENU);
	if (0)	fpr("o: x %d, y %d\n",*x,*y);
}

void update_size(void){
RECT	dim;
int i,x,y;

	if (0) {
		fpr("-- update_size ---\n");
		i=GetSystemMetrics(SM_CXBORDER);
		fpr("SM_CXBORDER %d\n",i);
		i=GetSystemMetrics(SM_CYBORDER);
		fpr("SM_CYBORDER %d\n",i);
		i=GetSystemMetrics(SM_CYCAPTION);
		fpr("SM_CYCAPTION %d\n",i);
		i=GetSystemMetrics(SM_CXFRAME);
		fpr("SM_CXFRAME %d\n",i);
		i=GetSystemMetrics(SM_CYFRAME);
		fpr("SM_CYFRAME %d\n",i);
		i=GetSystemMetrics(SM_CXFULLSCREEN);
		fpr("SM_CXFULLSCREEN %d\n",i);
		i=GetSystemMetrics(SM_CYFULLSCREEN);
		fpr("SM_CYFULLSCREEN %d\n",i);
		i=GetSystemMetrics(SM_CYMENU);
		fpr("SM_CYMENU %d\n",i);
		i=GetSystemMetrics(SM_CXMIN);
		fpr("SM_CXMIN %d\n",i);
		i=GetSystemMetrics(SM_CYMIN);
		fpr("SM_CYMIN %d\n",i);
		i=GetSystemMetrics(SM_CXMINTRACK);
		fpr("SM_CXMINTRACK %d\n",i);
		i=GetSystemMetrics(SM_CYMINTRACK);
		fpr("SM_CYMINTRACK %d\n",i);
		i=GetSystemMetrics(SM_CXSCREEN);
		fpr("SM_CXSCREEN %d\n",i);
		i=GetSystemMetrics(SM_CYSCREEN);
		fpr("SM_CYSCREEN %d\n",i);
	}

	frame_sizes(&x,&y);
	wino_w=x;
	wino_h=y;

	GetWindowRect(globalhwnd, &dim);
	win_w=dim.right-dim.left-wino_w;
	win_h=dim.bottom-dim.top-wino_h;
	MoveWindow(globalhwnd, dim.left, dim.top, win_w+wino_w, win_h+wino_h, TRUE);
	make_dirty();
}

void resize_screen(int sz){
RECT	dim;

	switch(sz) {
		case 1:	 win_w=512;  win_h=256;  break;	//2:1 no jags
		case 2:	 win_w=512;  win_h=341;  break;	//3:2 geo
		case 3:	 win_w=768;  win_h=512;  break; //3:2 geo
		case 4:	 win_w=1024; win_h=683;  break; //3:2 geo
		case 5:	 win_w=1024; win_h=768;  break; //4:3 no jags
		default: win_w=512;  win_h=256;
	}
	GetWindowRect(globalhwnd, &dim);
	MoveWindow(globalhwnd, dim.left, dim.top, win_w+wino_w, win_h+wino_h, TRUE);
	make_dirty();
}

void win32_plotp(int x, int y, int c)
{
#if WIN32COL == 8
	qscr_buf[x+y*qscr_w]=c;
#else
	qscr_buf[x+((qscr_h-1)-y)*qscr_w]=c;
#endif
}

/* eight at a time */
void win32_plotp8x(int x, int y, U8 *p)
{
int i,tx,ty;
U8 *d;

#if WIN32COL == 8
	d=&qscr_buf[x+y*qscr_w];
#else
//	d=&qscr_buf[x+((qscr_h-1)-y)*qscr_w];
	y=(qscr_h-1)-y;
	tx=x>>7;	// make tiles dependent!!!!
	ty=y>>6;
	d=(U8*)&qscrt_buf[tx+tiles*ty][ (x & 0x7f)+(y & 0x3f)*qscr_w/(tiles) ];
//	fpr("p %03x %03x  %02x %02x  %02x %02x\n",
//	 x,y,tx,ty,(x & 0x7f),(y & 0x3f)*qscr_w/(tiles) );
#endif
	dirty[tx][ty]=1;
	for(i=0;i<8;i++) *d++=*p++;
}

#define XSHIFT	(9-log2tiles)
#define YSHIFT	(8-log2tiles)
#define XMASK	((1 << XSHIFT)-1)
#define YMASK	((1 << YSHIFT)-1)

void win32_plotp8(int x, int y, U8 *p)
{
int i,tx,ty;
U8 *d;

#if WIN32COL == 8
	d=&qscr_buf[x+y*qscr_w];
#else
//	d=&qscr_buf[x+((qscr_h-1)-y)*qscr_w];
	y=(qscr_h-1)-y;
	tx=x>>XSHIFT;	// make tiles dependent!!!!
	ty=y>>YSHIFT;
	d=(U8*)&qscrt_buf[tx+tiles*ty][ (x & XMASK)+(y & YMASK)*qscr_w/(tiles) ];
//	fpr("p %03x %03x  %02x %02x  %02x %02x\n",
//	 x,y,tx,ty,(x & 0x7f),(y & 0x3f)*qscr_w/(tiles) );
#endif
	if (0 == dirty[tx][ty]) {
		dirty[tx][ty]=1;
		dtiles[ndirty++]=MKTILE(tx,ty);
	}
	for(i=0;i<8;i++) *d++=*p++;
}

void dump_screen_w(void)
{
HDC hdc,hdcMemory;
HBITMAP hbmp,hbmpOld;
int i;
BITMAP bm;

	hdc = GetDC(globalhwnd);
#if WIN32COL == 8
	hbmp = CreateBitmap(qscr_w, qscr_h, 1, 8, qscr_buf);

	hdcMemory = CreateCompatibleDC(hdc);

	hbmpOld = SelectObject(hdcMemory, hbmp);

	BitBlt(hdc, 0, 0, qscr_w, qscr_h, hdcMemory, 0, 0, SRCCOPY);
//	StretchBlt(hdc, 0, 0, qscr_w, qscr_h, hdcMemory, 0, 0, qscr_w, qscr_h, SRCCOPY);
	SelectObject(hdcMemory, hbmpOld);

	DeleteDC(hdcMemory);

	DeleteObject(hbmp);
#else
	StretchDIBits(hdc, 0, 0, win_w, win_h,
	  0, 0, qscr_w, qscr_h, qscr_buf, pbmpi, DIB_RGB_COLORS, SRCCOPY);
#endif
//0.86a
	ReleaseDC(globalhwnd, hdc);
}

void dump_screen_wtx(void)
{
HDC hdc,hdcMemory;
HBITMAP hbmp,hbmpOld;
int i,tw,th,thr;
BITMAP bm;

	hdc = GetDC(globalhwnd);

	for (th=0;th<tiles;th++) {
		thr=3-th;
		for (tw=0;tw<tiles;tw++) {
			if (dirty[tw][thr]) {
				StretchDIBits(hdc, tw*win_w/tiles, th*win_h/tiles, (win_w+2)/tiles, (win_h+2)/tiles,
				  0, 0, qscr_w/tiles, qscr_h/tiles, qscrt_buf[tw+thr*4], pbmpit, DIB_RGB_COLORS, SRCCOPY);
				dirty[tw][thr]=0;
			}
		}
	}
	ndirty=0;
	ReleaseDC(globalhwnd, hdc);
}

void dump_screen_wt(void)
{
HDC hdc,hdcMemory;
HBITMAP hbmp,hbmpOld;
int i,tw,th,thr;
BITMAP bm;

	if (0 == ndirty)
		return;
	hdc = GetDC(globalhwnd);

#if 1
	for (th=0;th<tiles;th++) {
		thr=(tiles-1)-th;
		for (tw=0;tw<tiles;tw++) {
			if (dirty[tw][thr]) {
				StretchDIBits(hdc, tw*win_w/tiles, th*win_h/tiles, (win_w+2)/tiles, (win_h+2)/tiles,
				  0, 0, qscr_w/tiles, qscr_h/tiles, qscrt_buf[tw+thr*tiles], pbmpit, DIB_RGB_COLORS, SRCCOPY);
				dirty[tw][thr]=0;
			}
		}
	}
#else
	for (i = 0; i < ndirty; i++) {
		th=dtiles[i]&255; thr=(tiles-1)-th;
		tw=dtiles[i]>>8;
		StretchDIBits(hdc, tw*win_w/tiles, th*win_h/tiles, (win_w+2)/tiles, (win_h+2)/tiles,
		  0, 0, qscr_w/tiles, qscr_h/tiles, qscrt_buf[tw+thr*tiles], pbmpit, DIB_RGB_COLORS, SRCCOPY);
		dirty[tw][thr]=0;
	}
#endif
	ndirty = 0;
	ReleaseDC(globalhwnd, hdc);
}

void repaint_screen(void)
{
//	dump_screen_w();
	dump_screen_wt();
}

void handle_exit(void)
{
	SendMessage(globalhwnd, WM_CLOSE, 0, 0L);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
HDC	hdc;
PAINTSTRUCT ps;
int	i, j, x, y;
int	reply;
static	HMENU hMenu;
static	HMENU hMenu_german;

	switch (message) {
	case MM_WOM_OPEN:
		snd_set_flags(SND_OPEN);
		break;
	case MM_WOM_DONE:
		snd_set_flags(SND_DONE);
		break;
	case MM_WOM_CLOSE:
		snd_set_flags(SND_CLOSE);
		break;
	case WM_CREATE:
		hMenu = GetMenu(hwnd);
		hMenu_german = LoadMenu(hInstance, "MENU_GERMAN");
		CheckMenuItem(hMenu, IDM_ENGLISH, MF_CHECKED);

		mouse_xpos=256;
		mouse_ypos=128;
		mouse_button=0;

		{char t[80];
			sprintf(t,"QLAYW - %s",qlayversion2());
			SetWindowText(hwnd, t);
		}

		return 0;

	case WM_PAINT:
		hdc = BeginPaint(hwnd, &ps);
		make_dirty();
		repaint_screen();
		EndPaint(hwnd, &ps);
		return 0;

	case WM_SIZE:
		update_size();
		return 0;

	case WM_CLOSE:
	case WM_DESTROY:
		reply=MessageBox(hwnd, "Are you sure?", "Exit", MB_YESNO|MB_ICONEXCLAMATION);
		if (reply==IDNO) return 0;
		win_stop_emulation();
		emul_running=0;
		PostQuitMessage(0);
		return 0;

	case WM_KEYDOWN:
		if (wParam == VK_F12) {
		      hideMouse = !hideMouse;
		      ShowCursor(hideMouse);
//		      if (hideMouse)
//			      fpr("SC= %d\n",SetCapture(hwnd));
//		      else
//		      	 	ReleaseCapture();
		      {char t[80];
		      	if (!hideMouse)
					sprintf(t,"QLAYW - %s [Press F12 to show mouse]",qlayversion2());
		      	else
					sprintf(t,"QLAYW - %s",qlayversion2());
		      	SetWindowText(hwnd, t);
		      }
		}
		//fall through
	case WM_KEYUP:
		if (!emul_running) return 0;
if(0)		fpr("U/D: %03x %03d %03x %08lx %03d\n",
			message,wParam,wParam,lParam,(lParam >> 16) & 0xff);
		my_kbd_handler(lParam >> 16);
		return 0;

	case WM_MOUSEMOVE:
//	case WM_NCMOUSEMOVE:
	case WM_RBUTTONDOWN:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_LBUTTONUP:
		mouse_xpos=LOWORD(lParam);
		mouse_ypos=HIWORD(lParam);
if(0)		{char t[80];
			sprintf(t,"MM: %5d %5d %4x",mouse_xpos,mouse_ypos,wParam);
			SetWindowText(hwnd, t);
		}
		mouse_xpos=(mouse_xpos*(512+128))/win_w;	// stay a bit ahead
		mouse_ypos=(mouse_ypos*(256+64))/win_h;
		mouse_button=wParam & 0x03;
//		fpr("MM: %d %d %d\n",mouse_xpos,mouse_ypos,mouse_button);
		return 0;

	case WM_COMMAND:

		switch (LOWORD(wParam)) {

		case IDM_INIT:
			if (emul_running) {
				MessageBox(hwnd, "Emulation runs", "Error", MB_OK|MB_ICONEXCLAMATION);
				return 0;
			}
			if (init_done) return 0;
			// see \rsxntdj\source\rsxnt\winmain.c
//			{extern int _argc; extern char ** _argv;
//				ql_main(_argc,_argv);
//			}
			init_done=1;
			return 0;

		case IDM_GO: /* go! */
			if (!init_done) {
				MessageBox(hwnd, "Init first", "Error", MB_OK|MB_ICONEXCLAMATION);
				return 0;
			}
			if (emul_running) return 0;
			emul_running=1;
			resize_screen(screen_res);
			debug();
			return 0;

		case IDM_END:
			SendMessage(hwnd, WM_CLOSE, 0, 0L);
			return 0;

		case IDM_SIZE1: resize_screen(1); return 0;

		case IDM_SIZE2: resize_screen(2); return 0;

		case IDM_SIZE3:	resize_screen(3); return 0;

		case IDM_SIZE4:	resize_screen(4); return 0;

		case IDM_SIZE5:	resize_screen(5); return 0;

		case IDM_TEST1:
			clear_screen(1);
			return 0;

		case IDM_TEST2:
			test_pattern2();
			dump_screen_w();
			return 0;

		case IDM_TEST3:
			{ int *p,a;p=(int*)-1;a=*p;*p=a+1;} //create a segment violation
			return 0;

		case IDM_HELP:
			{FILE *f;
			f=fopen("qlay.hlp","r");
			if (f!=NULL) {
				fclose(f);
				WinHelp( hwnd, "qlay.hlp", HELP_INDEX, 0L );
			} else {
				MessageBox(hwnd,
				 "Quit with <File> <Exit>\n"
				 "Error messages are in 'qlay.log'\n"
				 , "Help", MB_OK);
			}
			return 0;
			}
		case IDM_ABOUT:
			{char chbuf[180];
		 	sprintf(chbuf,"QLAY\n%s\n(c) Jan Venema",qlayversion());
			MessageBox(hwnd, chbuf, "About", MB_OK|MB_ICONINFORMATION);
			return 0;
			}
		case IDM_CONFIG:
			{char chbuf[80];int rv;
			hdc = GetDC(hwnd);
			rv=GetDeviceCaps(hdc, RASTERCAPS);
		 	fpr("Raster Caps: %08x ",rv);
			rv=GetDeviceCaps(hdc, BITSPIXEL);
		 	fpr("Bits/pixel: %08x ",rv);
			rv=GetDeviceCaps(hdc, NUMCOLORS);
		 	fpr("Num Colors: %08x ",rv);
			rv=GetDeviceCaps(hdc, PLANES);
		 	fpr("Planes: %08x ",rv);
			ReleaseDC(hwnd, hdc);
//			MessageBox(hwnd, chbuf, "Config", MB_OK);
			return 0;
		}

		case IDM_ENGLISH:
			SetMenu(hwnd, hMenu);
			CheckMenuItem(hMenu, IDM_ENGLISH, MF_CHECKED);
			CheckMenuItem(hMenu, IDM_GERMAN, MF_UNCHECKED);
			return 0;

		case IDM_GERMAN:
			SetMenu(hwnd, hMenu_german);
			CheckMenuItem(hMenu_german, IDM_ENGLISH, MF_UNCHECKED);
			CheckMenuItem(hMenu_german, IDM_GERMAN, MF_CHECKED);
			return 0;
		case IDM_DLG_CFG:
			wincfg_main(hInstance);
			return 0;
		case IDM_RESET:
			handle_reset();
//			MessageBox(hwnd, "Not yet implemented", "QLAYW", MB_OK);
			return 0;
		}
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}

void exit_fn(void)
{
	if (i_maybe_bad) MessageBox(globalhwnd, "Exiting.\n"
		"Error messages are in 'qlay.log'\n", "QLAY", MB_OK);
}

void write_box(char *s)
{
static int startposy=0;
HDC hdc;
unsigned int	i;

	for (i=0;i<strlen(s);i++) if (s[i]=='\n') s[i]=' ';

	hdc = GetDC(globalhwnd);
	if (startposy<240) {
		TextOut(hdc,0,startposy,s,strlen(s));
		startposy+=17;
	} else {
		MessageBox(globalhwnd, "Screen full", "QLAY", MB_OK);
		clear_screen(0);
		startposy=0;
		TextOut(hdc,0,startposy,s,strlen(s));
	}
	ReleaseDC(globalhwnd, hdc);

//	MessageBox(globalhwnd, s, "QLAY", MB_OK);
}

// see \rsxntdj\source\rsxnt\winmain.c
#ifdef __DJGPP__
# define argv _argv
# define argc _argc
extern int argc;
extern char ** argv;
#else
int argc = 0;
char *argv[2] = { "", "" };
#endif

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nCmdShow)
{
HWND	hwnd;
MSG	msg;
WNDCLASS wndclass;

	i_maybe_bad=1;
	atexit(exit_fn);

	open_log();

	init_done=0;
	emul_running=0;
	hideMouse=TRUE;
//	ShowCursor(FALSE);

	hInstance = hInst;

	if (!hPrevInstance) {
		wndclass.style = CS_HREDRAW | CS_VREDRAW | CS_BYTEALIGNCLIENT;
		wndclass.lpfnWndProc = WndProc;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = 0;
		wndclass.hInstance = hInstance;
		wndclass.hIcon = LoadIcon(hInstance, szAppname);
		wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
		wndclass.hbrBackground = GetStockObject(GRAY_BRUSH);
		wndclass.lpszMenuName = szAppname;
		wndclass.lpszClassName = szAppname;
		RegisterClass(&wndclass);
	}

	win_w=512;
	win_h=256;
	wino_w=8;
	wino_h=46;
	frame_sizes(&wino_w,&wino_h);
	hwnd = CreateWindow(szAppname, "QLAY", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, win_w+wino_w, win_h+wino_h,
		NULL, NULL, hInstance, NULL);

	globalhwnd = hwnd;

	init_bmpi();
	init_bmpit();
	make_dirty();

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	if (strlen(lpszCmdParam)) {
		argc=1;
		argv[1]=lpszCmdParam;
	}
	ql_main1(argc,argv);	/* get options */
	wincfg_main(hInstance);

	ql_main2();
	init_done=1;
	resize_screen(screen_res);
	emul_running=1;
	debug();

	// void, but doesn't hurt either
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	ql_exit();
	close_log();
	i_maybe_bad=0;
	return msg.wParam;
}

/* filter all ALT key presses and send to QLAY kbd handler */
int win_alt_key(MSG *m)
{

	if ((m->message==WM_SYSKEYDOWN)||(m->message==WM_SYSKEYUP)) {
/*
		{int kc;

		if (!emul_running) return 0;
		kc=(m->lParam >> 16) & 0xff;
if(0)		fpr("wak: %03x %03d %03x %08lx %03d\n",m->message,
			m->wParam,m->wParam,m->lParam,kc);
    		}
*/
		my_kbd_handler(m->lParam >> 16);
		return 1;
	}
	return 0;
}

/* check for pending messages for me */
void q_check_message(void)
{
MSG	msg;

	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (opt_use_altkey) if (win_alt_key(&msg)) return;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}
