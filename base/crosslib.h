
/*
 * Copyright (C) lcinx
 * lcinx@163.com 
*/

#ifndef _H_CROSSLIB_MY_H_
#define _H_CROSSLIB_MY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_config.h"

int64 high_millisecond_();
int64 high_microsecond_();

/* get current millisecond time */
#define get_millisecond() high_millisecond_()

/* get current microsecond time */
#define get_microsecond() high_microsecond_()

/* sleep some millisecond */
void delay_delay(unsigned long millisecond);

#ifdef __cplusplus
}
#endif
#endif

