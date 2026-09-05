/*
 * E-com_os Microkernel - Internal kernel structures
 */

#ifndef KERNEL_INTERNAL_KERNEL_H
#define KERNEL_INTERNAL_KERNEL_H

#include <kernel/internal/types.h>

// Kernel state
struct kernel_state {
	uint32_t boot_time;
	uint32_t uptime;
	uint32_t active_threads;
	uint32_t total_memory;
	uint32_t free_memory;
};
#define READ_BEFORE_FREE_BARRIER() __asm__ volatile("" ::: "memory")

// Global kernel state
extern struct kernel_state g_kernel_state;

// Internal kernel functions
void kernel_panic(const char* message);
void kernel_log(const char* message);

#endif
