/*
 * gettimeofday function for Windows
 *
 * https://stackoverflow.com/questions/10905892/equivalent-of-gettimeofday-for-windows
 *
 */
#include "gettimeofday.h"
#include <limits.h>
#include <time.h>

#if defined(__BORLANDC__)
# if (__BORLANDC__ > 0x0551)
#  include <emmintrin.h>
# else
void _mm_pause(void)
{
	asm { nop };
}
# endif
#define EPOCH_OFFSET_US 11644473600000000ui64
#define _1000000ULL 1000000ui64
#else
#include <immintrin.h>
#define EPOCH_OFFSET_US 11644473600000000ULL
#define _1000000ULL 1000000ULL
#endif

#if defined(__BORLANDC__)
# if (__BORLANDC__ > 0x0551)
#  define PRECISE_TIME 1
# endif
#elif (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
# define PRECISE_TIME 1
#endif

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    if (tv) {
        FILETIME               filetime; /* 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 00:00 UTC */
        ULARGE_INTEGER         x;
        ULONGLONG              usec;
        static const ULONGLONG epoch_offset_us = EPOCH_OFFSET_US; /* microseconds betweeen Jan 1,1601 and Jan 1,1970 */

#if defined(PRECISE_TIME)
        GetSystemTimePreciseAsFileTime(&filetime);
#else
        GetSystemTimeAsFileTime(&filetime);
#endif
        x.LowPart =  filetime.dwLowDateTime;
        x.HighPart = filetime.dwHighDateTime;
        usec = x.QuadPart / 10  -  epoch_offset_us;
        tv->tv_sec  = (long)(usec / _1000000ULL);
        tv->tv_usec = (long)(usec % _1000000ULL);
    }
    if (tz) {
        TIME_ZONE_INFORMATION timezone;
        GetTimeZoneInformation(&timezone);
        tz->tz_minuteswest = timezone.Bias;
        tz->tz_dsttime = 0;
    }
    return 0;
}


void sleep(int seconds)
{
  Sleep (1000 * seconds);
}

static double Freq;

void GetCounter(__int64* start)
{
	LARGE_INTEGER li;
	static int init = 1;
	
	if (init) {
    	LARGE_INTEGER perfFreq;
    	QueryPerformanceFrequency(&perfFreq);
    	Freq = perfFreq.QuadPart;
    	init = 0;
  	}

	QueryPerformanceCounter(&li);
	*start = li.QuadPart;
}

double Elapsed(__int64* start)
{
	double ret;
	LARGE_INTEGER li;
	
	QueryPerformanceCounter(&li);
	ret = (double)(li.QuadPart - *start) * 1000.0 * 1000.0 / Freq;
	*start = li.QuadPart;
	return ret;
}

double usleep(int useconds)
{
  static int init = 1;
  LARGE_INTEGER start, now;
  int counter = 0;
  double lapsed;

  useconds *= 2;
  if (init) {
    LARGE_INTEGER perfFreq;
    QueryPerformanceFrequency(&perfFreq);
    Freq = perfFreq.QuadPart;
    init = 0;
  }

  QueryPerformanceCounter(&start);
  do {
    if (0 == (++counter & 31))
	    _mm_pause();
    QueryPerformanceCounter((LARGE_INTEGER*) &now);
    lapsed = (now.QuadPart - start.QuadPart) * 1000.0 * 1000.0 / Freq;
  } while (lapsed < useconds);
  return lapsed;
}
