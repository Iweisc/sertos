#ifndef TYPES_H
#define TYPES_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;

typedef uint32_t size_t;
typedef int32_t ssize_t;

#ifndef __cplusplus
typedef uint8_t bool_t;
#define bool bool_t
#define true 1
#define false 0
#endif

#define NULL ((void*)0)

#define PACKED __attribute__((packed))

#endif
