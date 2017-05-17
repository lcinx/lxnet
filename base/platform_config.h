
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_CROSS_PLATFORM_CONFIG_H_
#define _H_CROSS_PLATFORM_CONFIG_H_


#ifndef _CROSS_PLATFORM_INT_TYPE_CONFIG_

#ifdef _MSC_VER
typedef signed __int8 int8;
typedef signed __int16 int16;
typedef signed __int32 int32;
typedef signed __int64 int64;

typedef unsigned __int8 uint8;
typedef unsigned __int16 uint16;
typedef unsigned __int32 uint32;
typedef unsigned __int64 uint64;
#else
#include <stdint.h>
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
#endif

#define _CROSS_PLATFORM_INT_TYPE_CONFIG_
#endif


#include <stddef.h>

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

#ifdef _WIN32
	#ifdef _MSC_VER
		#define snprintf(buf, bufsize, fmt, ...)	\
			(((buf)[(bufsize) - 1] = '\0'), _snprintf((buf), (size_t)(bufsize) - 1, fmt, ##__VA_ARGS__))

		#define safe_localtime(timep, tm_result)	(localtime_s((tm_result), (timep)), (tm_result))
		#define safe_gmtime(timep, tm_result)		(gmtime_s((tm_result), (timep)), (tm_result))
	#else
		#define safe_localtime(timep, tm_result)	(((void)tm_result), localtime(timep))
		#define safe_gmtime(timep, tm_result)		(((void)tm_result), gmtime(timep))
	#endif

	#define _FORMAT_64D_NUM "%I64d"
	#define _FORMAT_64U_NUM "%I64u"
	#define _FORMAT_64X_NUM "%I64x"


#else
	#if defined(__APPLE__)
		#define _FORMAT_64D_NUM "%lld"
		#define _FORMAT_64U_NUM "%llu"
		#define _FORMAT_64X_NUM "%llx"
	#elif defined(__LP64__) || defined(__x86_64__)
		#define _FORMAT_64D_NUM "%ld"
		#define _FORMAT_64U_NUM "%lu"
		#define _FORMAT_64X_NUM "%lx"
	#elif defined(__i386__)
		#define _FORMAT_64D_NUM "%lld"
		#define _FORMAT_64U_NUM "%llu"
		#define _FORMAT_64X_NUM "%llx"
	#else
		#define _FORMAT_64D_NUM "%lld"
		#define _FORMAT_64U_NUM "%llu"
		#define _FORMAT_64X_NUM "%llx"
	#endif

	#define safe_localtime localtime_r
	#define safe_gmtime gmtime_r
#endif


#endif

