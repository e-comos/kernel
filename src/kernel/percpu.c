/*
    E-comOS Kernel - Per-CPU data structures implementation
    Copyright (C) 2025,2026  Saladin5101
*/

#include <kernel/percpu.h>
#include <stdint.h>

/* Per-CPU data for the bootstrap processor */
static percpu_t bsp_percpu __attribute__((aligned(16)));

void percpu_init(void) {
    bsp_percpu.kernel_rsp = 0;
    bsp_percpu.user_rsp = 0;
    
    /*
     * For SYSCALL/SYSRET mechanism:
     * - When in user mode, GS base = user GS base (or 0)
     * - KernelGSbase MSR holds the kernel GS base
     * - swapgs exchanges GS base and KernelGSbase
     *
     * We need to set KernelGSbase MSR (0xC0000102) to our per-CPU data.
     */
#define MSR_KERNEL_GS_BASE 0xC0000102
    
    /* Write kernel GS base - this is what swapgs will load in kernel mode */
    uint32_t low = (uint32_t)((uint64_t)(uintptr_t)&bsp_percpu & 0xFFFFFFFFu);
    uint32_t high = (uint32_t)(((uint64_t)(uintptr_t)&bsp_percpu >> 32) & 0xFFFFFFFFu);
    __asm__ volatile("wrmsr" : : "c"(MSR_KERNEL_GS_BASE), "a"(low), "d"(high));
}

percpu_t *percpu_get(void) {
    return &bsp_percpu;
}
