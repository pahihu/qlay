// https://stackoverflow.com/questions/5814869/playing-an-arbitrary-sound-on-windows
// https://github.com/Planet-Source-Code/david-overton-playing-audio-in-windows-using-waveout-interface__3-4422

#define _USE_MATH_DEFINES 1
#include <math.h>
#include <stdio.h>

#define WIN32_BIG_FAT 1
#include <windows.h>
#include <mmreg.h>
#include "gettimeofday.h"
#include <limits.h>
#include "snd-win.h"

// #include <complex>

// #pragma comment(lib, "Winmm.lib")

// Triangle wave generator
static float triangle(float timeInSeconds, unsigned short channel, void *context)
{
    const float frequency = *(const float *)context;
    const float angle = (float)(frequency * 2 * M_PI * timeInSeconds);
    switch (channel)
    {
        case  0: return (float)asin(sin(angle + 0 * M_PI / 2));
        default: return (float)asin(sin(angle + 1 * M_PI / 2));
    }
}

// Pure tone generator
static float pure(float timeInSeconds, float frequency)
{
    const float angle = (float)(frequency * 2 * M_PI * timeInSeconds);
	return (float)sin(angle + 0 * M_PI / 2);
	// default: return (float)sin(angle + 1 * M_PI / 2);
}

#define SAMPLES_PER_SECOND	8000
#define NCHANNELS			1
#define MAX_SECONDS			3
#define MAX_WAVEHDRS		16

static int snd_flags;
static float *waveBuffer;
static HWAVEOUT hWavOut;
static WAVEHDR waveHdrs[MAX_WAVEHDRS];
static int currentHdr;

void CALLBACK MyWaveOutProc(
	HWAVEOUT hwo,
	UINT      uMsg,
	DWORD_PTR dwInstance,
	DWORD_PTR dwParam1,
	DWORD_PTR dwParam2)
{
	switch (uMsg) {
	case WOM_OPEN:
		snd_flags |= SND_OPEN;
		break;
	case WOM_DONE:
		snd_flags |= SND_DONE;
		break;
	case WOM_CLOSE:
		snd_flags |= SND_CLOSE;
		break;
	}

}

void snd_set_flags(int flags)
{
	snd_flags |= flags;
}

int snd_open(void *hwnd)
{
    MMRESULT mmresult = MMSYSERR_NOERROR;
	WAVEFORMATEX waveFormat;
	UINT nDevs;
	float *buf;
	int i;
	
	nDevs=waveOutGetNumDevs();
	// fpr("snd_open: nDevs=%d\n",nDevs);

	waveBuffer = (float*)malloc(MAX_WAVEHDRS *
		MAX_SECONDS * NCHANNELS * SAMPLES_PER_SECOND * sizeof(float));

    memset(&waveFormat, 0, sizeof(waveFormat));
    waveFormat.cbSize = 0;
    waveFormat.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    waveFormat.nChannels = NCHANNELS;
    waveFormat.nSamplesPerSec = SAMPLES_PER_SECOND;
    waveFormat.wBitsPerSample = CHAR_BIT * sizeof(waveBuffer[0]);
    waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / CHAR_BIT;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;

    if (NULL == hwnd) {
	    // fpr("snd_open: hwnd is NULL\n");
	    return -1;
    }
    mmresult = waveOutOpen(&hWavOut, WAVE_MAPPER,
        &waveFormat, (DWORD_PTR)hwnd, NULL, CALLBACK_WINDOW);
	// fpr("snd_open: result=%d\n",mmresult);
	
	buf = waveBuffer;
	for (i = 0; i < MAX_WAVEHDRS; i++) {
		waveHdrs[i].lpData = (LPSTR)buf;
		buf += MAX_SECONDS*SAMPLES_PER_SECOND;
	}
	currentHdr=0;
	return mmresult;
}

int snd_play(float nFreq, float nMillis)
{
    MMRESULT mmresult = MMSYSERR_NOERROR;
    float *buf = (float*)waveHdrs[currentHdr].lpData;
    float nSeconds = nMillis / 1000.0;
    size_t i;
	// fpr("snd_play: currentHdr=%d flags=%d nSeconds=%g\n", currentHdr, snd_flags, nSeconds);

    const size_t nBuffer = (size_t)(nSeconds * SAMPLES_PER_SECOND);
	for (i = 0; i < nBuffer; i++)
		buf[i] = pure((i) * nSeconds / nBuffer, nFreq);

	if (0==(snd_flags & SND_OPEN)) {
		// fpr("snd_play: not open\n");
		return -1;
	}
	if (waveHdrs[currentHdr].dwFlags & WHDR_PREPARED) {
		do {
			mmresult = waveOutUnprepareHeader(hWavOut,&waveHdrs[currentHdr],sizeof(waveHdrs[0]));
		} while(WAVERR_STILLPLAYING == mmresult);
        /*while (0 == (waveHdr.dwFlags & WHDR_DONE))
        	usleep(10);
		mmresult = waveOutUnprepareHeader(hWavOut,&waveHdr,sizeof(waveHdr));*/
		// fpr("snd_play: UnprepareHeader result=%d\n",mmresult);
	}

    // memset(&waveHdr, 0, sizeof(waveHdr));
    waveHdrs[currentHdr].dwBufferLength = (ULONG)(nBuffer * sizeof(waveBuffer[0]));
    // waveHdr.lpData = (LPSTR)&waveBuffer[0];
    mmresult = waveOutPrepareHeader(hWavOut, &waveHdrs[currentHdr], sizeof(waveHdrs[0]));
    if (mmresult == MMSYSERR_NOERROR) {
        snd_flags &= ~SND_DONE;
        mmresult = waveOutWrite(hWavOut, &waveHdrs[currentHdr], sizeof(waveHdrs[0]));
        // fpr("snd_play: Write result=%d\n",mmresult);
        currentHdr++;
        if (currentHdr == MAX_WAVEHDRS)
        	currentHdr = 0;
	} else {
		// fpr("snd_play: PrepareHeader result=%d\n",mmresult)
		;
	}
	return mmresult;
}

int snd_stop(void)
{
	/* TBD */
	return 0;
}

int snd_close(void)
{
	MMRESULT mmresult;
	// fpr("snd_close: flags=%d\n",snd_flags);
	mmresult=waveOutClose(hWavOut);
	// fpr("snd_close: Close result=%d\n",mmresult);
	return mmresult;
}

