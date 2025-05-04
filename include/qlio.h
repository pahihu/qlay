/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	IO defines and functions
*/

extern void init_QL(void);
extern U8 qliord_b(A32 a);
extern U32 qliord_l(A32 a);
extern void qliowr_b(A32 a, U8 d);
extern int irq_level(void);
extern void do_next_event(void);
extern void update_LED(void);
extern void do_keyboard(void);
extern void ser_rcv_enqueue(int ch, int len, U8 *b);
