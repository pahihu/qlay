/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	OS specific defines and functions
*/

extern void do_scrn(A32 a, U16 l);
extern void do_scrmode(int mode);
extern void start_speaker(void);
extern void do_speaker(void);
extern void stop_speaker(void);
extern void do_mouse(int lastmx, int lastmy, int button);
extern void get_mouse_state(int *x, int *y, int *b);
extern int k_press(void);
extern int get_keyrow(int row);
extern void draw_LED(int x, int y, long color, int size);
extern void flash_colors(void);
extern void handle_cas_key(void);
extern int init_graphics(void);
extern void graphics_leave(void);
extern void handle_events_50Hz(void);
extern int open_log(void);
extern int close_log(void);
extern int fpr(char* fmt, ...);
extern int epr(char* fmt, ...);

#ifdef __QLWIN32__
extern void my_kbd_handler(int);
extern void win_stop_emulation(void);
extern void handle_reset(void);
#endif
