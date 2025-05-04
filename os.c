/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	OS specific functions
*/

#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "qlmem.h"
#include "os.h"

#ifdef HAVE_LINUX_JOYSTICK_H

static int js0;
static int joystickpresent = 0;

struct JS_DATA_TYPE jscal;

void read_joystick(U16 *dir, int *button)
{
    static int minx = INT_MAX, maxx = INT_MIN,
               miny = INT_MAX, maxy = INT_MIN;
    int left = 0, right = 0, top = 0, bot = 0;
    struct JS_DATA_TYPE buffer;
    int len;

    *dir = 0;
    *button = 0;
    if (!joystickpresent)
    	return;

    len = read(js0, &buffer, sizeof(buffer));
    if (len != sizeof(buffer))
    	return;

    if (buffer.x < minx) minx = buffer.x;
    if (buffer.y < miny) miny = buffer.y;
    if (buffer.x > maxx) maxx = buffer.x;
    if (buffer.y > maxy) maxy = buffer.y;

    if (buffer.x < (minx + (maxx-minx)/3))
    	left = 1;
    else if (buffer.x > (minx + 2*(maxx-minx)/3))
    	right = 1;

    if (buffer.y < (miny + (maxy-miny)/3))
    	top = 1;
    else if (buffer.y > (miny + 2*(maxy-miny)/3))
    	bot = 1;

    if (left) top = !top;
    if (right) bot = !bot;
    *dir = bot | (right << 1) | (top << 8) | (left << 9);
    *button = (buffer.buttons & 3) != 0;
}

void init_joystick(void)
{
    js0 = open("/dev/js0", O_RDONLY);
    if (js0 < 0)
    	return;
    joystickpresent = 1;
}

void close_joystick(void)
{
    if (joystickpresent)
	close(js0);
}

#elif defined(__DOS__) && !defined(_WIN32)

static int joystickpresent = 0;

void read_joystick(U16 *dir, int *button)
{
    static int minx = INT_MAX, maxx = INT_MIN,
               miny = INT_MAX, maxy = INT_MIN;
    int left = 0, right = 0, top = 0, bot = 0;
    char JoyPort;
    int laps, JoyX, JoyY;

    *dir = 0;
    *button = 0;
    if (!joystickpresent)
	return;

    JoyX = 0;
    JoyY = 0;
    laps = 0;
    __asm__ __volatile__("cli");
    outportb(0x201, 0xff);
    do {
	JoyPort = inportb(0x201);
	JoyX = JoyX + (JoyPort & 1);
	JoyY = JoyY + ((JoyPort & 2) >> 1);
	laps++;
    } while(((JoyPort & 3) != 0) && (laps != 65535));
    __asm__ __volatile__("sti");

    if (JoyX < minx) minx = JoyX;
    if (JoyY < miny) miny = JoyY;
    if (JoyX > maxx) maxx = JoyX;
    if (JoyY > maxy) maxy = JoyY;

    if (JoyX < (minx + (maxx-minx)/3))
	left = 1;
    else if (JoyX > (minx + 2*(maxx-minx)/3))
	right = 1;

    if (JoyY < (miny + (maxy-miny)/3))
	top = 1;
    else if (JoyY > (miny + 2*(maxy-miny)/3))
	bot = 1;

    if (left) top = !top;
    if (right) bot = !bot;
    *dir = bot | (right << 1) | (top << 8) | (left << 9);
    *button = ((~JoyPort) & 48) != 0;
}

void init_joystick(void)
{
    int laps = 0;
    char JoyPort;
    __asm__ __volatile__("cli");
    outportb(0x201, 0xff);
    do {
	JoyPort = inportb(0x201);
	laps++;
    } while(((JoyPort & 3) != 0) && (laps != 65535));
    __asm__ __volatile__("sti");
    if (laps != 65535)
	joystickpresent = 1;
}

void close_joystick(void)
{
}

#else
void read_joystick(U16 *dir, int *button)
{
    *dir = 0;
    *button = 0;
}

void init_joystick(void)
{
}

void close_joystick(void)
{
}

#endif

A32 audlc[4], audpt[4];
U16 audvol[4], audper[4], audlen[4], audwlen[4];
int audwper[4];
U16 auddat[4];
int audsnum[4];

/* Audio states. This is not an exact representation of the Audio State Machine
 * (HRM p. 166), but a simplification. To be honest, I don't completely
 * understand that picture yet.
 */
int audst[4];

int sound_table[256][64];

static void init_sound_table16(void)
{
    int i,j;

    for (i = 0; i < 256; i++)
	for (j = 0; j < 64; j++)
	    sound_table[i][j] = j * (S8)i;
}

static void init_sound_table8(void)
{
    int i,j;

    for (i = 0; i < 256; i++)
	for (j = 0; j < 64; j++)
	    sound_table[i][j] = (j * (S8)i) / 256;
}

#ifdef LINUX_SOUND

#include <sys/ioctl.h>
#include <sys/soundcard.h>

static const int n_frames = 10;

/* The buffer is too large... */
static U16 buffer[44100], *bufpt;
static U8 snddata[4];

static int frames = 0;
static int smplcnt = 0;
static int dsprate, dspbits, dspomit = 0;
static int sndbufsize;

static int sfd;
static int have_sound;

int init_sound (void)
{
    int tmp;
    int rate;

    unsigned long formats;

    sfd = open ("/dev/dsp", O_WRONLY);
    have_sound = !(sfd < 0);
    if (!have_sound) {
	return 0;
    }

    ioctl (sfd, SNDCTL_DSP_GETFMTS, &formats);

    tmp = 0x0004000D;
    ioctl (sfd, SNDCTL_DSP_SETFRAGMENT, &tmp);
    ioctl (sfd, SNDCTL_DSP_GETBLKSIZE, &sndbufsize);
#ifndef LINUX_SOUND_SLOW_MACHINE
    dspbits = 16;
    ioctl(sfd, SNDCTL_DSP_SAMPLESIZE, &dspbits);
    ioctl(sfd, SOUND_PCM_READ_BITS, &dspbits);
    if (dspbits != 16)
#endif
    {
	dspbits = 8;
	ioctl(sfd, SNDCTL_DSP_SAMPLESIZE, &dspbits);
	ioctl(sfd, SOUND_PCM_READ_BITS, &dspbits);
	if (dspbits != 8)
	    return 0;
    }

    tmp = 0;
    ioctl(sfd, SNDCTL_DSP_STEREO, &tmp);

#ifndef LINUX_SOUND_SLOW_MACHINE
    dsprate = 0;
    rate = 44100;
    ioctl(sfd, SNDCTL_DSP_SPEED, &rate);
    ioctl(sfd, SOUND_PCM_READ_RATE, &rate);
    if (rate < 43000 || rate > 45000)
#endif
    {
	dsprate = 1;
	rate = 22050;
	ioctl(sfd, SNDCTL_DSP_SPEED, &rate);
	ioctl(sfd, SOUND_PCM_READ_RATE, &rate);
	/* Some soundcards return 22222 here. */
	if (rate < 21000 || rate > 23000)
	    return 0;
    }
    if (dspbits == 16) {
	/* Will this break horribly on Linux/Alpha? Possible... */
	if (!(formats & AFMT_S16_LE))
	    return 0;
	init_sound_table16 ();
    } else {
	if (!(formats & AFMT_U8))
	    return 0;
	init_sound_table8 ();
    }
    printf ("Sound driver found and configured for %d bits at %d Hz, buffer is %d bytes\n",
	    dspbits, rate, sndbufsize);
    audst[0] = audst[1] = audst[2] = audst[3] = 0;
    bufpt = buffer;
    frames = n_frames;
    smplcnt = 0;
    return 1;
}

static void channel_reload (int c)
{
    audst[c] = 1;
    audpt[c] = audlc[c];
    audwper[c] = 0;
    audwlen[c] = audlen[c];
    audsnum[c] = 1;
}

void do_sound (void)
{
    smplcnt -= 227;
    while (smplcnt < 0) {
	int i;
	smplcnt += 80;
	dspomit ^= dsprate;

	for(i = 0; i < 4; i++) {
	    if (dmaen (1<<i)) {
		if (audst[i] == 0) {
		    /* DMA was turned on for this channel */
		    channel_reload (i);
		    continue;
		}

		if (audwper[i] <= 0) {
		    audwper[i] += audper[i];
		    if (audst[i] == 1) {
			/*  Starting a sample, cause interrupt */
			put_word (0xDFF09C, 0x8000 | (0x80 << i));
			audst[i] = 2;
		    }
		    audsnum[i] ^= 1;
		    if (audsnum[i] == 0) {
			auddat[i] = get_word (audpt[i]);
			audpt[i] += 2;
			audwlen[i]--;
			if (audwlen[i] == 0) {
			    channel_reload (i);
			}
		    }
		}
		if (adkcon & (0x11 << i)) {
		    snddata[i] = 0;
		    audsnum[i] ^= 2;
		    audsnum[i] |= 1;
		    if (i < 3) {
			if (adkcon & (0x11 << i)) {
			    if (audsnum[i] & 2)
				audvol[i+1] = auddat[i];
			    else
				audper[i+1] = auddat[i];
			} else if (adkcon & (1 << i)) {
			    audvol[i+1] = auddat[i];
			} else {
			    audper[i+1] = auddat[i];
			}
		    }
		} else {
		    snddata[i] = audsnum[i] & 1 ? auddat[i] : auddat[i] >> 8;
		}
		audwper[i] -= 80;
	    } else
	    	audst[i] = snddata[i] = 0;
	}
	if (dspbits == 16) {
	    if (dspomit == 0)
		*bufpt++ = (sound_table[snddata[0]][audvol[0]]
			    + sound_table[snddata[1]][audvol[1]]
			    + sound_table[snddata[2]][audvol[2]]
			    + sound_table[snddata[3]][audvol[3]]);
	} else {
	    unsigned char *bp = (unsigned char *)bufpt;
	    if (dspomit == 0)
		*bp++ = (sound_table[snddata[0]][audvol[0]]
			 + sound_table[snddata[1]][audvol[1]]
			 + sound_table[snddata[2]][audvol[2]]
			 + sound_table[snddata[3]][audvol[3]] + 128);
	    bufpt = (U16 *)bp;
	}
	if ((char *)bufpt - (char *)buffer >= sndbufsize) {
	    write(sfd, buffer, sndbufsize);
	    bufpt = buffer;
	}
    }
}

#elif defined(AF_SOUND)

#include <AF/AFlib.h>

/* The buffer is too large... */
static U16 buffer[44100], *bufpt;
static U8 snddata[4];

static int smplcnt = 0;
static int dspbits = 0;
static int sndbufsize;

static AFAudioConn  *aud;
static AC            ac;
static long          count = 0;
static long          aftime;
static int           rate;
static int           freq_divisor;


static int have_sound;

int init_sound (void)
{
    AFSetACAttributes   attributes;
    AFDeviceDescriptor *aDev;
    int                 device;

    aud = AFOpenAudioConn(NULL);
    have_sound = !(aud == NULL);
    if (!have_sound) {
	return 0;
    }

    for(device = 0; device < ANumberOfAudioDevices(aud); device++) {
	aDev = AAudioDeviceDescriptor(aud, device);
	rate = aDev->playSampleFreq;
	sndbufsize = (rate / 8) * 4;
	if(aDev->inputsFromPhone == 0
	   && aDev->outputsToPhone == 0
	   && aDev->playNchannels == 1)
	    break;
    }
    if (device == ANumberOfAudioDevices(aud)) {
	return 0;
    }

    dspbits = 16;
    attributes.type = LIN16;
    ac = AFCreateAC(aud, device, ACEncodingType, &attributes);
    aftime = AFGetTime(ac);

    if (dspbits == 16) {
	init_sound_table16 ();
    } else {
	init_sound_table8 ();
    }

    freq_divisor = 44100 / rate;

    audst[0] = audst[1] = audst[2] = audst[3] = 0;
    bufpt = buffer;
    smplcnt = 0;
    printf ("Sound driver found and configured for %d bits at %d Hz, buffer is %d bytes\n", dspbits, rate, sndbufsize);
    return 1;
}

static void channel_reload (int c)
{
    audst[c] = 1;
    audpt[c] = audlc[c];
    audwper[c] = 0;
    audwlen[c] = audlen[c];
    audsnum[c] = 1;
}

void do_sound (void)
{
    smplcnt -= 227;
    while (smplcnt < 0) {
	int i;
	smplcnt += 80;

	for(i = 0; i < 4; i++) {
	    if (dmaen (1<<i)) {
		if (audst[i] == 0) {
		    /* DMA was turned on for this channel */
		    channel_reload (i);
		    continue;
		}

		if (audwper[i] <= 0) {
		    audwper[i] += audper[i];
		    if (audst[i] == 1) {
			/*  Starting a sample, cause interrupt */
			put_word (0xDFF09C, 0x8000 | (0x80 << i));
			audst[i] = 2;
		    }
		    audsnum[i] ^= 1;
		    if (audsnum[i] == 0) {
			auddat[i] = get_word (audpt[i]);
			audpt[i] += 2;
			audwlen[i]--;
			if (audwlen[i] == 0) {
			    channel_reload (i);
			}
		    }
		}
		if (adkcon & (0x11 << i)) {
		    snddata[i] = 0;
		    audsnum[i] ^= 2;
		    audsnum[i] |= 1;
		    if (i < 3) {
			if (adkcon & (0x11 << i)) {
			    if (audsnum[i] & 2)
				audvol[i+1] = auddat[i];
			    else
				audper[i+1] = auddat[i];
			} else if (adkcon & (1 << i)) {
			    audvol[i+1] = auddat[i];
			} else {
			    audper[i+1] = auddat[i];
			}
		    }
		} else {
		    snddata[i] = audsnum[i] & 1 ? auddat[i] : auddat[i] >> 8;
		}
		audwper[i] -= 80;
	    } else
	    	audst[i] = snddata[i] = 0;
	}
	if ((count++ % frq_divisor) == 0) {
	    long size;
	    *bufpt++ = (sound_table[snddata[0]][audvol[0]]
			+ sound_table[snddata[1]][audvol[1]]
			+ sound_table[snddata[2]][audvol[2]]
			+ sound_table[snddata[3]][audvol[3]];
	    size = (char *)bufpt - (char *)buffer;
	    if (size >= sndbufsize) {
		if (AFGetTime(ac) > aftime)
		    aftime = AFGetTime(ac);
		AFPlaySamples(ac, aftime, size, (unsigned char*) buffer);
		aftime += size / 2;
		bufpt = buffer;
	    }
	}
    }
}

#elif defined(__mac__)

#include <Sound.h>

static SndChannelPtr newChannel;
static ExtSoundHeader theSndBuffer;
static SndCommand theCmd;
static const int n_frames = 10;

/* The buffer is too large... */
static U16 buffer0[44100], buffer1[44100], *bufpt;
static U8 snddata[4];

static int frames = 0;
static int smplcnt = 0;
static long count = 0;
static int have_sound;
static int sndbufsize=44100;
static int nextbuf=0;
static Boolean sFlag=true;

int init_sound (void)
{
    if (SndNewChannel(&newChannel, sampledSynth, initMono, NULL))
	return 0;
    init_sound_table8 ();
    smplcnt = 0;
    bufpt = buffer0;
    return 1;
}

static void channel_reload (int c)
{
    audst[c] = 1;
    audpt[c] = audlc[c];
    audwper[c] = 0;
    audwlen[c] = audlen[c];
    audsnum[c] = 1;
}

void do_sound (void)
{
    unsigned char *bp;

    smplcnt -= 227;
    while (smplcnt < 0) {
	int i;
	smplcnt += 80;

	for(i = 0; i < 4; i++) {
	    if (dmaen (1<<i)) {
		if (audst[i] == 0) {
		    /* DMA was turned on for this channel */
		    channel_reload (i);
		    continue;
		}

		if (audwper[i] <= 0) {
		    audwper[i] += audper[i];
		    if (audst[i] == 1) {
			/*  Starting a sample, cause interrupt */
			put_word (0xDFF09C, 0x8000 | (0x80 << i));
			audst[i] = 2;
		    }
		    audsnum[i] ^= 1;
		    if (audsnum[i] == 0) {
			auddat[i] = get_word (audpt[i]);
			audpt[i] += 2;
			audwlen[i]--;
			if (audwlen[i] == 0) {
			    channel_reload (i);
			}
		    }
		}
		if (adkcon & (0x11 << i)) {
		    snddata[i] = 0;
		    audsnum[i] ^= 2;
		    audsnum[i] |= 1;
		    if (i < 3) {
			if (adkcon & (0x11 << i)) {
			    if (audsnum[i] & 2)
				audvol[i+1] = auddat[i];
			    else
				audper[i+1] = auddat[i];
			} else if (adkcon & (1 << i)) {
			    audvol[i+1] = auddat[i];
			} else {
			    audper[i+1] = auddat[i];
			}
		    }
		} else {
		    snddata[i] = audsnum[i] & 1 ? auddat[i] : auddat[i] >> 8;
		}
		audwper[i] -= 80;
	    } else
	    	audst[i] = snddata[i] = 0;
	}

	bp = (unsigned char *)bufpt;
	*bp++ = (sound_table[snddata[0]][audvol[0]]
		 + sound_table[snddata[1]][audvol[1]]
		 + sound_table[snddata[2]][audvol[2]]
		 + sound_table[snddata[3]][audvol[3]] + 128);
	bufpt = (U16 *)bp;

	if (nextbuf == 0) {
	    if ((char *)bufpt - (char *)buffer0 >= sndbufsize) {
		nextbuf++;
		bufpt = buffer1;

		theSndBuffer.samplePtr = (Ptr)buffer0;
		theSndBuffer.numChannels = 1;
		theSndBuffer.sampleRate = 0xac440000;
		theSndBuffer.encode = extSH;
		theSndBuffer.numFrames = sndbufsize;
		theSndBuffer.sampleSize = 8;
		theCmd.param1 = 0;
		theCmd.param2 = (long)&theSndBuffer;
		theCmd.cmd = bufferCmd;
		SndDoCommand(newChannel, &theCmd, false);
	    }
	} else {
	    if ((char *)bufpt - (char *)buffer1 >= sndbufsize) {
		nextbuf=0;
    		bufpt = buffer0;

    		theSndBuffer.samplePtr = (Ptr)buffer1;
    		theSndBuffer.numChannels = 1;
		theSndBuffer.sampleRate = 0xac440000;
		theSndBuffer.encode = extSH;
		theSndBuffer.numFrames = sndbufsize;
		theSndBuffer.sampleSize = 8;
		theCmd.param1 = 0;
		theCmd.param2 = (long)&theSndBuffer;
		theCmd.cmd = bufferCmd;
		SndDoCommand(newChannel, &theCmd, false);
	    }
	}
    }
}

#else

int init_sound (void)
{
    return 1;
}

void do_sound (void)
{
}

#endif
