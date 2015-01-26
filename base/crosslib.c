
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#include "crosslib.h"

#ifdef WIN32
#include <windows.h>

struct _tp_integer {
	int isinit;
	LARGE_INTEGER liCounter;
};
static struct _tp_integer s_larget_int = {0};
static __inline int64 __GetSecondCount__() {
	if (0 == s_larget_int.isinit) {
		QueryPerformanceFrequency(&s_larget_int.liCounter);
		s_larget_int.isinit = 1;
	}
	return s_larget_int.liCounter.QuadPart;
}
/*
 * get current millisecond time
 */
int64 high_millisecond_() {
	double tmp;
	LARGE_INTEGER liCurrent;
	QueryPerformanceCounter(&liCurrent);
	tmp = (double)liCurrent.QuadPart / (double)__GetSecondCount__();
	return (int64)(tmp * 1000);
}

/* get current microsecond time */
int64 high_microsecond_() {
	double tmp;
	LARGE_INTEGER liCurrent;
	QueryPerformanceCounter(&liCurrent);
	tmp = (double)liCurrent.QuadPart / (double)__GetSecondCount__();
	return (int64)(tmp * 1000000);
}

void delay_delay(unsigned long millisecond) {
	Sleep(millisecond);
}

#else

#include <sys/time.h>
#include <unistd.h>

/* get current millisecond time */
int64 high_millisecond_() {
	struct timeval tv;
	int64 res;
	gettimeofday(&tv, NULL);
	res = (int64)(tv.tv_sec * (int64)1000) + (int64)(tv.tv_usec/(int64)1000);
	return res;
}

/* get current microsecond time */
int64 high_microsecond_() {
	struct timeval tv;
	int64 res;
	gettimeofday(&tv, NULL);
	res = (int64)(tv.tv_sec * (int64)1000000) + (int64)tv.tv_usec;
	return res;
}

void delay_delay(unsigned long millisecond) {
	usleep(millisecond*1000);
}

#endif



