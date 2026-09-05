/*
	E-comOS Kernel - Address Space Management
	Copyright (C) 2025,2026  Saladin5101
*/

#include <kernel/address_space.h>
#include <kernel/mm.h>

#define MAX_ADDRESS_SPACES 16

static uint8_t as_used[MAX_ADDRESS_SPACES] = {0};

address_space as_create(void) {
	for (int i = 1; i < MAX_ADDRESS_SPACES; i++) {
		if (!as_used[i]) {
			as_used[i] = 1;
			return (address_space)i;
		}
	}
	return 0;
}

int as_destroy(address_space as) {
	if (as == 0 || as >= MAX_ADDRESS_SPACES)
		return -1;
	as_used[as] = 0;
	return 0;
}

int as_map(address_space as, uint32_t vaddr, uint32_t paddr, uint32_t size,
		   uint32_t flags) {
	if (as == 0 || as >= MAX_ADDRESS_SPACES || !as_used[as])
		return -1;
	uint32_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	for (uint32_t i = 0; i < pages; i++) {
		int rc =
			mm_map_page(vaddr + i * PAGE_SIZE, paddr + i * PAGE_SIZE, flags);
		if (rc != 0)
			return rc;
	}
	return 0;
}

int as_unmap(address_space as, uint32_t vaddr, uint32_t size) {
	(void)as;
	(void)vaddr;
	(void)size;
	/* mmUnmapPage not yet wired to page tables — placeholder */
	return 0;
}
