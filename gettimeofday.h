#ifndef _GETTIMEOFDAY_H
#define _GETTIMEOFDAY_H

#if !defined(WIN32_BIG_FAT)
#define WIN32_LEAN_AND_MEAN	1
#include <windows.h>
#include <winsock2.h>
#endif

#if defined(__MINGW32__)
#include <time.h>
#else
struct timezone
{
        int  tz_minuteswest; /* minutes W of Greenwich */
        int  tz_dsttime;     /* type of dst correction */
};
#endif

int gettimeofday (struct timeval *tv, struct timezone *tz);
void sleep (int seconds);
double usleep (int useconds);
void GetCounter(__int64* start);
double Elapsed(__int64* start);

#endif
