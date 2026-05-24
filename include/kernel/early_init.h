/*
    E-comOS Kernel - Early Initialization
    Copyright (C) 2025,2026  Saladin5101
*/

#ifndef KERNEL_EARLY_INIT_H
#define KERNEL_EARLY_INIT_H

#include <stdint.h>

int early_kernel_init(uint32_t multiboot_magic, uint32_t multiboot_info);
void enable_sse(void);
#endif
