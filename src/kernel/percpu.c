/*
    E-comOS Kernel - Per-CPU data structures implementation
    Copyright (C) 2025,2026  Saladin5101
*/

#include <kernel/percpu.h>
#include <stdint.h>

/* Per-CPU data for the bootstrap processor */
static percpu_t bsp_percpu __attribute__((aligned(16)));

/* Dedicated syscall kernel stack (8KB) */
static uint8_t syscall_kernel_stack[8192] __attribute__((aligned(16)));

void percpu_init(void) {
    /* Initialize kernel stack pointer BEFORE setting GS base */
    bsp_percpu.kernel_rsp = (uint64_t)(uintptr_t)(syscall_kernel_stack + sizeof(syscall_kernel_stack));
    bsp_percpu.user_rsp = 0;
    
    /*
     * Set KernelGSbase MSR (0xC0000102) to point to our per-CPU data.
     * swapgs will exchange GS base with this value.
     */
#define MSR_KERNEL_GS_BASE 0xC0000102
    
    uint32_t low = (uint32_t)((uint64_t)(uintptr_t)&bsp_percpu & 0xFFFFFFFFu);
    uint32_t high = (uint32_t)(((uint64_t)(uintptr_t)&bsp_percpu >> 32) & 0xFFFFFFFFu);
    __asm__ volatile("wrmsr" : : "c"(MSR_KERNEL_GS_BASE), "a"(low), "d"(high));
}

percpu_t *percpu_get(void) {
    return &bsp_percpu;
}
