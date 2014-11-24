
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifndef _H_T_CONFIGTYPE_T_H_
#define _H_T_CONFIGTYPE_T_H_

#ifdef _MSC_VER
typedef signed __int64 int64;
typedef signed __int32 int32;
typedef signed __int16 int16;
typedef signed __int8 int8;

typedef unsigned __int64 uint64;
typedef unsigned __int32 uint32;
typedef unsigned __int16 uint16;
typedef unsigned __int8 uint8;
#else
#include <stdint.h>
typedef int64_t int64;
typedef int32_t int32;
typedef int16_t int16;
typedef int8_t int8;
typedef uint64_t uint64;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;
#endif


#endif

