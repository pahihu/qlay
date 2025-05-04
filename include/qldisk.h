/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	qldisk defines and functions
*/

extern void init_qldisk(void);
extern void exit_qldisk(void);
extern void wrnfa(A32,U8);
extern U8 rdnfa(A32);
extern int win_avail(void);

extern U8 rdserpar(A32);
extern void wrserpar(A32,U8);
