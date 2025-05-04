/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	OS joystick & sound defines and functions
*/

extern void read_joystick(U16 *dir, int *button);
extern void init_joystick(void);
extern void close_joystick(void);

extern A32 audlc[4], audpt[4];
extern U16 audvol[4], audper[4], audlen[4];

extern int init_sound (void);
extern void do_sound (void);
