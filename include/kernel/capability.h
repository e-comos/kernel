/*
    E-comOS Kernel - Capability System
    Copyright (C) 2025,2026  Saladin5101
*/

#ifndef KERNEL_CAPABILITY_H
#define KERNEL_CAPABILITY_H

#include <stdint.h>

#define CAP_TYPE_MEMORY 1
#define CAP_TYPE_IRQ 2
#define CAP_TYPE_IPC 3
#define CAP_TYPE_IO_PORT 4

#define CAP_RIGHT_READ (1 << 0)
#define CAP_RIGHT_WRITE (1 << 1)
#define CAP_RIGHT_EXECUTE (1 << 2)
#define CAP_RIGHT_GRANT (1 << 3)

typedef struct {
	uint32_t type;
	uint32_t rights;
	uint32_t object_id;
	uint32_t base_addr;
	uint32_t size;
} Capability;

int cap_grant(uint32_t target_pid, Capability cap);
int cap_revoke(Capability cap);
int cap_check(Capability cap, uint32_t required_rights);

#endif
