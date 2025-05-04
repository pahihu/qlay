#if !defined(_SND_WIN_H)
#define _SND_WIN_H

#define SND_OPEN	1
#define SND_DONE	2
#define SND_CLOSE	4

extern void snd_set_flags(int flags);
extern int snd_open(void *hwnd);
extern int snd_play(float nFreq, float nMillis);
extern int snd_stop(void);
extern int snd_close(void);

#endif