/*
    E-comOS Kernel - Address Space Management
    Copyright (C) 2025,2026  Saladin5101
*/

#ifndef KERNEL_ADDRESS_SPACE_H
#define KERNEL_ADDRESS_SPACE_H

#include <stdint.h>

typedef address_space;

address_space as_create(void);
int as_destroy(address_space as);
int as_map(address_space as, uint32_t vaddr, uint32_t paddr,
           uint32_t size, uint32_t flags);
int as_unmap(address_space as, uint32_t vaddr, uint32_t size);

#endif
