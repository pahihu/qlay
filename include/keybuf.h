/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	Keyboard buffer defines and functions
*/

extern int get_next_key (void);
extern int keys_available (void);
extern void record_key (int);
extern void init_keybuf (void);
extern void getjoystate(U16*, int*);
extern int pcsc_ql(int);

extern unsigned char escape_keys[];
