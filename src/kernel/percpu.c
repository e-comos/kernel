/*
    E-comOS Kernel - Per-CPU data structures implementation
    Copyright (C) 2025,2026  Saladin5101
*/

#include <kernel/percpu.h>
#include <stdint.h>

/* Per-CPU data for the bootstrap processor */
static percpu_t bsp_percpu __attribute__((aligned(16)));

/* MSR addresses */
#define MSR_GS_BASE     0xC0000101

static void write_msr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)(value & 0xFFFFFFFFu);
    uint32_t high = (uint32_t)((value >> 32) & 0xFFFFFFFFu);
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

void percpu_init(void) {
    bsp_percpu.kernel_rsp = 0;
    bsp_percpu.user_rsp = 0;
    
    /* Set GS base to point to our per-CPU data structure */
    write_msr(MSR_GS_BASE, (uint64_t)(uintptr_t)&bsp_percpu);
}

percpu_t *percpu_get(void) {
    return &bsp_percpu;
}
