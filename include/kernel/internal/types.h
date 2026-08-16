/*
 * E-com_os Microkernel - Internal type definitions
 */

#ifndef KERNEL_INTERNAL_TYPES_H
#define KERNEL_INTERNAL_TYPES_H

#include <stddef.h>
#include <stdint.h>

// Kernel object IDs
typedef uint32_t threadId_t;
typedef uint32_t processId_t;
typedef uint32_t capability_t;

// Error codes
#define KERNEL_OK 0
#define KERNEL_ERROR -1
#define KERNEL_INVALID_ARG -2
#define KERNEL_NO_MEMORY -3
#define KERNEL_NO_PERM -4

// Kernel limits
#define MAX_THREADS 256
#define MAX_PROCESSES 64
#define MAX_CAPABILITIES 1024

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long s64;

#endif