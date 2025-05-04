/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	Win95 menus and functions
*/

#include "qlayw.h"

extern void write_box(char*);
extern void win32_plotp(int,int,int);
extern void win32_plotp8(int,int,U8*);
extern void repaint_screen(void);
extern void q_check_message(void);

extern void open_serial(int,int,int);
extern void close_serial(int);
extern void send_serial_char(int);
extern int rcv_serial_string(char*);

extern int mouse_xpos;
extern int mouse_ypos;
extern int mouse_button;
