/*
 * E-com_os x86_64 - Paging structures
 */

#ifndef ARCH_X86_64_INTERNAL_PAGING_H
#define ARCH_X86_64_INTERNAL_PAGING_H

#include <stdint.h>

// Page directory entry flags
#define PAGE_PRESENT 0x001
#define PAGE_WRITABLE 0x002
#define PAGE_USER 0x004
#define PAGE_ACCESSED 0x020
#define PAGE_DIRTY 0x040

// Page table entry
typedef uint32_t page_table_entry_t;

// Page directory entry
typedef uint32_t page_directory_entry_t;

// Page directory structure
struct page_directory {
	page_directory_entry_t tables[1024];
};

// Paging functions
void paging_init(void);
void paging_enable(void);
int paging_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);

#endif