/*
	E-comOS Kernel - Debug Utilities
	Copyright (C) 2025,2026  Saladin5101
*/

#ifndef KERNEL_DEBUG_H
#define KERNEL_DEBUG_H

void early_debug_init(void);
void early_debug_puts(const char* str);
void kernel_panic(const char* msg);
void kernel_log(const char* msg);

#endif
