/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	Keyboard buffer
*/

#include "sysconfig.h"
#include "sysdeps.h"
#include <assert.h>
#include "options.h"
#include "keybuf.h"
#include "qlkeys.h"
#include "pckeys.h"
#include "os.h"
#include "spc-os.h"

static int translate_keycode(int);

#define KBL 256
static int keybuf[KBL];

static int kpb_first, kpb_last;

int fakestate[5] = { 0, 0, 0, 0, 0 };

void getjoystate(U16 *st, int *button)
{
	if (fake_joystick) {
		int top = fakestate[0];
		int bot = fakestate[3];
		if (fakestate[1]) top = !top;
		if (fakestate[2]) bot = !bot;
		*st = bot | (fakestate[2] << 1) | (top << 8) | (fakestate[1] << 9);
		*button = fakestate[4];
	} else
		read_joystick(st, button);
}

void init_keybuf (void)
{
	kpb_first = kpb_last = 0;
}

int keys_available (void)
{
	return kpb_first != kpb_last;
}

int get_next_key (void)
{
int key;

	assert (kpb_first != kpb_last);

	key = keybuf[kpb_last];
	if (++kpb_last == KBL) kpb_last = 0;
	key &= 0x073f;
	return key;
}

void record_key (int kc)
{
int	kpb_next;

	kc = translate_keycode(kc);
	if (kc == -1) return;

	kpb_next = kpb_first + 1;
	if (kpb_next == KBL)
		kpb_next = 0;
	if (kpb_next == kpb_last) {
		fpr("Keyboard buffer overrun.\n");
		return;
	}
	if (fake_joystick) {
		switch (kc >> 1) {
/*
		case AK_NP8: fakestate[0] = !(kc & 1); return;
		case AK_NP4: fakestate[1] = !(kc & 1); return;
		case AK_NP6: fakestate[2] = !(kc & 1); return;
		case AK_NP2: fakestate[3] = !(kc & 1); return;
		case AK_NP0: case AK_NP5: fakestate[4] = !(kc & 1); return;
*/
		}
	}
	keybuf[kpb_first] = kc;
	kpb_first = kpb_next;
}

unsigned char escape_keys[128] = {
  0,			0,			0,		0,
  0, 			0,			0,		0,
  0, 			0,			0,		0,
  0, 			0,			0,		0,
/*10*/
  0, 			0,			0,		0,
  0, 			0,			0,		0,
  0,			0,			0,		0,
  SCODE_KEYPADENTER,	SCODE_RCONTROL,		0,		0,
/*20*/
  0, 			0,			0,		0,
  0, 			0,			0,		0,
  0, 			0,			0,		0,
  0, 			0,			0,		0,
/*30*/
  0, 			0,			0,		0,
  0, 			SCODE_KEYPADDIV,			0,		0,
  SCODE_RALT, 		0,			0,		0,
  0, 			0,			0,		0,
/*40*/
  0, 			0,			0,		0,
  0,			SCODE_NUMLOCK,		0,		SCODE_HOME,
  SCODE_CBLOCKUP,	SCODE_PGUP,		0,		SCODE_CBLOCKLEFT,
  0,			SCODE_CBLOCKRIGHT,	0,		SCODE_END,
/*50*/
  SCODE_CBLOCKDOWN,	SCODE_PGDN,		SCODE_INSERT,	SCODE_DELETE,
  0,			0,			0,		0,
  0,			0,			0,		SCODE_LWIN95,
  SCODE_RWIN95,		SCODE_MWIN95,		0,		0,
/*60*/
  0,			0,			0,		0,
  0,			0,			0,		0,
  0,			0,			0,		0,
  0,			0,			0,		0,
/*70*/
  0,			0,			0,		0,
  0,			0,			0,		0,
  0,			0,			0,		0,
  0,			0,			0,		0
};

/* convert a PC scancode to QL key */
int pcsc_ql(int sc)
{
	switch(sc) {
	case SCODE_A:		return QL_A;
	case SCODE_B:		return QL_B;
	case SCODE_C:		return QL_C;
	case SCODE_D:		return QL_D;
	case SCODE_E:		return QL_E;
	case SCODE_F:		return QL_F;
	case SCODE_G:		return QL_G;
	case SCODE_H:		return QL_H;
	case SCODE_I:		return QL_I;
	case SCODE_J:		return QL_J;
	case SCODE_K:		return QL_K;
	case SCODE_L:		return QL_L;
	case SCODE_M:		return QL_M;
	case SCODE_N:		return QL_N;
	case SCODE_O:		return QL_O;
	case SCODE_P:		return QL_P;
	case SCODE_Q:		return QL_Q;
	case SCODE_R:		return QL_R;
	case SCODE_S:		return QL_S;
	case SCODE_T:		return QL_T;
	case SCODE_U:		return QL_U;
	case SCODE_V:		return QL_V;
	case SCODE_W:		return QL_W;
	case SCODE_X:		return QL_X;
	case SCODE_Y:		return QL_Y;
	case SCODE_Z:		return QL_Z;

	case SCODE_0:		return QL_0;
	case SCODE_1:		return QL_1;
	case SCODE_2:		return QL_2;
	case SCODE_3:		return QL_3;
	case SCODE_4:		return QL_4;
	case SCODE_5:		return QL_5;
	case SCODE_6:		return QL_6;
	case SCODE_7:		return QL_7;
	case SCODE_8:		return QL_8;
	case SCODE_9:		return QL_9;

	case SCODE_KEYPAD0:	return QL_KP|QL_0;
	case SCODE_KEYPAD1:	return QL_KP|QL_1;
	case SCODE_KEYPAD2:	return QL_KP|QL_2;
	case SCODE_KEYPAD3:	return QL_KP|QL_3;
	case SCODE_KEYPAD4:	return QL_KP|QL_4;
	case SCODE_KEYPAD5:	return QL_KP|QL_5;
	case SCODE_KEYPAD6:	return QL_KP|QL_6;
	case SCODE_KEYPAD7:	return QL_KP|QL_7;
	case SCODE_KEYPAD8:	return QL_KP|QL_8;
	case SCODE_KEYPAD9:	return QL_KP|QL_9;

	case SCODE_F1:		return QL_F1;
	case SCODE_F2:		return QL_F2;
	case SCODE_F3:		return QL_F3;
	case SCODE_F4:		return QL_F4;
	case SCODE_F5:		return QL_F5;
	case SCODE_F6:		return QL_SHIFT|QL_F1;
	case SCODE_F7:		return QL_SHIFT|QL_F2;
	case SCODE_F8:		return QL_SHIFT|QL_F3;
	case SCODE_F9:		return QL_SHIFT|QL_F4;
	case SCODE_F10:		return QL_SHIFT|QL_F5;
	case SCODE_F11:		return -1;
	case SCODE_F12:		return -1;

	case SCODE_BS:		return QL_CTRL|QL_LEFT;
	case SCODE_LCONTROL:	return QL_CTRL;
	case SCODE_RCONTROL:	return QL_CTRL;
	case SCODE_TAB:		return QL_TAB;
	case SCODE_LALT:	return QL_ALT;
	case SCODE_RALT:	return QL_KP|QL_ALT;
	case SCODE_ENTER:	return QL_ENTER;
	case SCODE_SPACE:	return QL_SPACE;
	case SCODE_LSHIFT:	return QL_SHIFT;
	case SCODE_RSHIFT:	return QL_SHIFT;
	case SCODE_ESCAPE:	return QL_ESCAPE;

	case SCODE_INSERT:	return -1;
	case SCODE_DELETE:	return QL_CTRL|QL_RIGHT;
	case SCODE_CBLOCKUP:	return QL_UP;
	case SCODE_CBLOCKDOWN:	return QL_DOWN;
	case SCODE_CBLOCKLEFT:	return QL_LEFT;
	case SCODE_CBLOCKRIGHT:	return QL_RIGHT;
	case SCODE_CAPSLOCK:	return QL_CAPSLOCK;

	case SCODE_LBRACKET:	return QL_LBRACKET;
	case SCODE_RBRACKET:	return QL_RBRACKET;
	case SCODE_COMMA:	return QL_COMMA;
	case SCODE_PERIOD:	return QL_PERIOD;
	case SCODE_SLASH:	return QL_SLASH;
	case SCODE_SEMICOLON:	return QL_SEMICOLON;
	case SCODE_GRAVE:	return QL_QUOTE;
	case SCODE_MINUS:	return QL_MINUS;
	case SCODE_EQUAL:	return QL_EQUAL;

	case SCODE_KEYPADPLUS:	return QL_KP|QL_SHIFT|QL_EQUAL;
	case SCODE_KEYPADMINUS:	return QL_KP|QL_MINUS;
	case SCODE_KEYPADMUL:	return QL_KP|QL_SHIFT|QL_8;
	case SCODE_KEYPADDIV:	return QL_KP|QL_SLASH;
	case SCODE_KEYPADDOT:	return QL_KP|QL_PERIOD;
	case SCODE_KEYPADENTER:	return QL_KP|QL_ENTER;

	case SCODE_SCROLLLOCK:	return QL_CTRL|QL_F5;

	case SCODE_PGUP:	return QL_ALT|QL_ENTER;
	case SCODE_PGDN:	return QL_ALT|QL_ENTER;

	case SCODE_BACKSLASH:	return QL_BACKSLASH;
	case SCODE_BACKQUOTE:	return QL_POUND;

	case SCODE_BREAK:	return QL_CTRL|QL_SPACE;
	case SCODE_END:		return QL_CTRL|QL_SPACE;
	case SCODE_HOME:	return -1;

	case SCODE_86:		return QL_SC_56;	/* UK,GE */

	/* known keys, not used */
	case SCODE_LWIN95:	return -1;
	case SCODE_RWIN95:	return -1;
	case SCODE_MWIN95:	return -1;
	case SCODE_NUMLOCK:	return -1;

	/* non decoded keys fall through */
	}
	fpr("key code %x\n",sc);
	return -1;
}

static int translate_keycode(int k)
{
	switch(opt_keyb) {
	case KEYB_US:
		return k;
	case KEYB_GE:
		if ( (k & 0x3f) == QL_Z ) return (k & 0xfc0) | QL_Y;
		if ( (k & 0x3f) == QL_Y ) return (k & 0xfc0) | QL_Z;
		switch(k) {
		case QL_POUND: return QLSH_6;	/* dead ^*/
		case QLSH_POUND: return QL_CTRL|QL_SHIFT|QL_Z;	/* high 0 */
		case QLSH_2: return QLSH_QUOTE;
		case QLSH_3: return QL_CTRL|QL_SHIFT|QL_V;	/* para sign */
		case QLSH_6: return QLSH_7;
		case QLSH_7: return QL_SLASH;
		case QLSH_8: return QLSH_9;
		case QLSH_9: return QLSH_0;
		case QLSH_0: return QL_EQUAL;
		case QL_MINUS: return QL_CTRL|QL_SHIFT|QL_COMMA; /* beta */
		case QLSH_MINUS: return QLSH_SLASH;
		case QL_EQUAL: return QL_QUOTE;	/* dead ' */
		case QLSH_EQUAL: return QL_CTRL|QL_SHIFT|QL_SLASH; /* dead ` */
		case QL_BACKSLASH: return QLSH_3;
		case QLSH_BACKSLASH: return QL_QUOTE;
		case QL_LBRACKET: return QL_CTRL|QL_QUOTE;	/* u-uml */
		case QLSH_LBRACKET: return QL_CTRL|QL_SHIFT|QL_G;	/* U-uml */
		case QL_RBRACKET: return QLSH_EQUAL;
		case QLSH_RBRACKET: return QLSH_8;
		case QL_SEMICOLON: return QL_CTRL|QL_SHIFT|QL_4; 	/* o-uml */
		case QLSH_SEMICOLON: return QL_CTRL|QL_SHIFT|QL_D;	/* O-uml */
		case QL_QUOTE: return QL_CTRL|QL_ESCAPE;	/* a-uml */
		case QLSH_QUOTE: return QL_CTRL|QL_SHIFT|QL_2;	/* A-uml */
		case QLSH_COMMA: return QL_SEMICOLON;
		case QLSH_PERIOD: return QLSH_SEMICOLON;
		case QL_SLASH: return QL_MINUS;
		case QLSH_SLASH: return QLSH_MINUS;
		case QL_SC_56: return QLSH_COMMA;
		case QL_SHIFT|QL_SC_56: return QLSH_PERIOD;
		case QL_KP|QL_ALT|QL_8: return QL_LBRACKET;
		case QL_KP|QL_ALT|QL_9: return QL_RBRACKET;
		case QL_KP|QL_ALT|QL_7: return QLSH_LBRACKET;
		case QL_KP|QL_ALT|QL_0: return QLSH_RBRACKET;
		case QL_KP|QL_ALT|QL_MINUS: return QL_BACKSLASH;
		case QL_KP|QL_ALT|QL_RBRACKET: return QLSH_POUND;
		case QL_KP|QL_ALT|QL_Q: return QLSH_2;
		}
		return k;
	case KEYB_FR:	/* 120,0x78 */
		if ( (k & 0x3f) == QL_A ) return (k & 0xfc0) | QL_Q;
		if ( (k & 0x3f) == QL_Q ) return (k & 0xfc0) | QL_A;
		if ( (k & 0x3f) == QL_W ) return (k & 0xfc0) | QL_Z;
		if ( (k & 0x3f) == QL_Z ) return (k & 0xfc0) | QL_W;
		if ( (k & 0x3f) == QL_SEMICOLON ) return (k & 0xfc0) | QL_M;
		switch(k) {
		case QL_POUND: return QL_2;			/* high 2 */
		case QLSH_POUND: return QL_POUND;		/* ??? */
		case QL_1: return QLSH_7;
		case QL_2: return QL_CTRL|QL_SHIFT|QL_3;	/* 'e */;
		case QL_3: return QLSH_QUOTE;
		case QL_4: return QL_QUOTE;
		case QL_5: return QLSH_9;
		case QL_6: return QL_CTRL|QL_SHIFT|QL_V;	/* para sign */
		case QL_7: return QL_CTRL|QL_0;			/* `e */
		case QL_8: return QLSH_1;
		case QL_9: return QL_CTRL|QL_SHIFT|QL_9;	/* ,c */
		case QL_0: return QL_CTRL|QL_MINUS;		/* `a */
		case QLSH_1: return QL_1;
		case QLSH_2: return QL_2;
		case QLSH_3: return QL_3;
		case QLSH_4: return QL_4;
		case QLSH_5: return QL_5;
		case QLSH_6: return QL_6;
		case QLSH_7: return QL_7;
		case QLSH_8: return QL_8;
		case QLSH_9: return QL_9;
		case QLSH_0: return QL_0;
		case QL_MINUS: return QLSH_0;
		case QLSH_MINUS: return QL_CTRL|QL_SHIFT|QL_Z;	/* high 0 */
		case QL_EQUAL: return QL_MINUS;
		case QLSH_EQUAL: return QLSH_MINUS;
		case QL_BACKSLASH: return QL_CTRL|QL_SHIFT|QL_P; /* mu */
		case QLSH_BACKSLASH: return QL_POUND;
		case QL_LBRACKET: return -1;			/* dead ^ */
		case QLSH_LBRACKET: return -1;			/* dead trema */
		case QL_RBRACKET: return QLSH_4;
		case QLSH_RBRACKET: return QLSH_8;
		case QL_QUOTE: return QL_CTRL|QL_SHIFT|QL_SEMICOLON;	/* `u */
		case QLSH_QUOTE: return QLSH_5;
		case QL_M: return QL_COMMA;
		case QL_SHIFT|QL_M: return QLSH_SLASH;
		case QL_COMMA: return QL_SEMICOLON;
		case QLSH_COMMA: return QL_PERIOD;
		case QL_PERIOD: return QLSH_SEMICOLON;
		case QLSH_PERIOD: return QL_SLASH;
		case QL_SLASH: return QL_EQUAL;
		case QLSH_SLASH: return QLSH_EQUAL;
		case QL_SC_56: return QLSH_COMMA;
		case QL_SHIFT|QL_SC_56: return QLSH_PERIOD;
		case QL_KP|QL_ALT|QL_1: return QLSH_BACKSLASH;
		case QL_KP|QL_ALT|QL_2: return QLSH_2;
		case QL_KP|QL_ALT|QL_3: return QLSH_3;
		case QL_KP|QL_ALT|QL_6: return QLSH_6;
		case QL_KP|QL_ALT|QL_9: return QLSH_LBRACKET;
		case QL_KP|QL_ALT|QL_0: return QLSH_RBRACKET;
		case QL_KP|QL_ALT|QL_LBRACKET: return QL_LBRACKET;
		case QL_KP|QL_ALT|QL_RBRACKET: return QL_RBRACKET;
		case QL_KP|QL_PERIOD: return QL_COMMA;
		}
		return k;
	case KEYB_IT:	/* 142,0x8e */
		switch(k) {
		case QL_POUND: return QL_BACKSLASH;
		case QLSH_POUND: return QLSH_BACKSLASH;
		case QLSH_2: return QLSH_QUOTE;
		case QLSH_3: return QL_POUND;
		case QLSH_6: return QLSH_7;
		case QLSH_7: return QL_SLASH;
		case QLSH_8: return QLSH_9;
		case QLSH_9: return QLSH_0;
		case QLSH_0: return QL_EQUAL;
		case QL_MINUS: return QL_QUOTE;
		case QLSH_MINUS: return QLSH_SLASH;
		case QL_EQUAL: return QL_CTRL|QL_4;			/* `i */
		case QLSH_EQUAL: return QLSH_6;
		case QL_BACKSLASH: return QL_CTRL|QL_SHIFT|QL_SEMICOLON; /* `u */
		case QLSH_BACKSLASH: return QL_CTRL|QL_SHIFT|QL_V;	/* para sign */
		case QL_LBRACKET: return QL_CTRL|QL_0;			/* `e */
		case QLSH_LBRACKET: return QL_CTRL|QL_SHIFT|QL_3;	/* 'e */
		case QL_RBRACKET: return QLSH_EQUAL;
		case QLSH_RBRACKET: return QLSH_8;
		case QL_SEMICOLON: return QL_CTRL|QL_7;			/* `o */
		case QLSH_SEMICOLON: return QL_CTRL|QL_SHIFT|QL_9;	/* ,c */
		case QL_QUOTE: return QL_CTRL|QL_MINUS;			/* `a */
		case QLSH_QUOTE: return QL_CTRL|QL_SHIFT|QL_Z;		/* high 0 */
		case QLSH_COMMA: return QL_SEMICOLON;
		case QLSH_PERIOD: return QLSH_SEMICOLON;
		case QL_SLASH: return QL_MINUS;
		case QLSH_SLASH: return QLSH_MINUS;
		case QL_SC_56: return QLSH_COMMA;
		case QL_SHIFT|QL_SC_56: return QLSH_PERIOD;
		case QL_KP|QL_ALT|QL_8: return QL_LBRACKET;
		case QL_KP|QL_ALT|QL_9: return QL_RBRACKET;
		case QL_KP|QL_ALT|QL_7: return QLSH_LBRACKET;
		case QL_KP|QL_ALT|QL_0: return QLSH_RBRACKET;
		case QL_KP|QL_ALT|QL_Q: return QLSH_2;
		case QL_KP|QL_ALT|QL_3: return QLSH_3;
		case QL_KP|QL_ALT|QL_RBRACKET: return QLSH_POUND;
//0.90
		/* other code page: 141,0x8d */
		case QL_KP|QL_ALT|QL_LBRACKET: return QL_LBRACKET;
//dup		case QL_KP|QL_ALT|QL_RBRACKET: return QL_RBRACKET;
		case QL_KP|QL_ALT|QL_SEMICOLON: return QLSH_2;
		case QL_KP|QL_ALT|QL_QUOTE: return QLSH_3;

		}
		return k;
	case KEYB_SE:
		return k;
	case KEYB_UK:
		switch(k) {
		case QL_POUND: return -1;	/* ` */
		case QLSH_POUND: return -1;	/* -| */
		case QLSH_2: return QLSH_QUOTE;
		case QLSH_3: return QL_POUND;
		case QL_BACKSLASH: return QLSH_3;
		case QLSH_BACKSLASH: return QLSH_POUND;
		case QLSH_QUOTE: return QLSH_2;
		case QL_SC_56: return QL_BACKSLASH;
		case QL_SHIFT|QL_SC_56: return QLSH_BACKSLASH;
		}
		return k;
	default:
		return k;
	}
}
