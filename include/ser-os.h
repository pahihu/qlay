/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	Serial port defines and functions
*/

extern void init_serial(void);
extern void exit_serial(void);
extern void open_serial(int,int,int);
extern void close_serial(int);
extern void send_serial_char(int,int);
extern void ser_rcv_thread(void);
