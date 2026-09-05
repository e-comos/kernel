/*
	E-comOS Kernel - Per-CPU data structures
	Copyright (C) 2025,2026  Saladin5101

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Affero General Public License as published
	by the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
*/

#ifndef KERNEL_PERCPU_H
#define KERNEL_PERCPU_H

#include <stdint.h>

/* Per-CPU data structure - accessed via GS base in kernel mode */
typedef struct {
	uint64_t kernel_rsp; /* Kernel stack pointer for syscall entry */
	uint64_t user_rsp;	 /* Saved user stack pointer */
} percpu_t;

/* Initialize per-CPU data for the current CPU */
void percpu_init(void);

/* Get the address of per-CPU data structure */
percpu_t* percpu_get(void);

#endif
