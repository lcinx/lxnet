
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 *
 * this define from linux source.
*/

#ifndef _H_ALWAY_INLINE_H_
#define _H_ALWAY_INLINE_H_

#ifndef __cplusplus

#ifdef __GNUC__
	#include <stdbool.h>
	#define inline __inline __attribute__((always_inline))
#elif defined(_MSC_VER) 
	#define inline __forceinline
	#define bool char
	#define true 1
	#define false 0
#endif	/* __GNUC__ */

#endif	/* __cplusplus */

#endif

