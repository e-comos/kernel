#ifndef DRIVER_LOADER_H
#define DRIVER_LOADER_H

#include <stdint.h>

#define DRIVER_LOAD_ADDR 0x700000ULL

struct multiboot_tag {
	uint32_t type;
	uint32_t size;
};

struct multiboot_tag_module {
	uint32_t type;
	uint32_t size;
	uint32_t mod_start;
	uint32_t mod_end;
	char cmdline[];
};

int load_driver_from_multiboot(void *mb_info_addr);
int load_service_from_memory(uintptr_t phys_addr, uint32_t size, uintptr_t target_vaddr);
#endif
