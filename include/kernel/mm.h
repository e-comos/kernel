/*
    E-comOS Kernel - Memory Manager Interface
    Copyright (C) 2025,2026  Saladin5101

    Invariant: page_bitmap bit i == 1  ↔  physical page i is allocated.
    Invariant: next_free_page ≤ MAX_PAGES at all times.
*/

#ifndef KERNEL_MM_H
#define KERNEL_MM_H

#include <stdint.h>
#include <stddef.h>
#include <kernel/boot.h>

#define PAGE_SIZE       4096u
#define MAX_PHYS_PAGES  4096u   /* manages 16 MB: [0x100000, 0x1100000) */
#define MAX_PAGES       MAX_PHYS_PAGES
#define KERNEL_BASE     0x100000u

/* Physical memory window managed */
#define PHYS_BASE      0x100000ULL
#define PHYS_SIZE      (MAX_PAGES * (uint64_t)PAGE_SIZE)  /* 16 MB */

/* x86 page-table entry flags */
#define PTE_PRESENT  (1u << 0)
#define PTE_WRITABLE (1u << 1)
#define PTE_USER     (1u << 2)
#define PTE_PWT      (1u << 3)  /* Page Write Through */
#define PTE_PCD      (1u << 4)  /* Page Cache Disable */
#define PTE_ACCESSED (1u << 5)
#define PTE_DIRTY    (1u << 6)
#define PTE_PAT      (1u << 7)  /* Page Attribute Table */
#define PTE_GLOBAL   (1u << 8)

/* Higher-level mapping flags (translated to PTE flags by mmMapPage) */
#define MM_FLAG_READ    (1u << 0)
#define MM_FLAG_WRITE   (1u << 1)
#define MM_FLAG_EXEC    (1u << 2)
#define MM_FLAG_USER    (1u << 3)
#define MM_FLAG_DEVICE  (1u << 4)  /* Non-cacheable */
#define MM_FLAG_CACHED  (1u << 5)  /* Cacheable */
#define MM_FLAG_GLOBAL  (1u << 6)  /* Global page */

#define MM_FLAG_KERNEL_RW (MM_FLAG_READ | MM_FLAG_WRITE)
#define MM_FLAG_USER_RO   (MM_FLAG_READ | MM_FLAG_USER)
#define MM_FLAG_USER_RW   (MM_FLAG_READ | MM_FLAG_WRITE | MM_FLAG_USER)
#define MM_FLAG_KERNEL_RX (MM_FLAG_READ | MM_FLAG_EXEC)
#define MM_FLAG_USER_RX   (MM_FLAG_READ | MM_FLAG_EXEC | MM_FLAG_USER)

/* Page table structures */
#define PT_ENTRIES  512u
#define PD_ENTRIES  512u
#define PDPT_ENTRIES 512u
#define PML4_ENTRIES 512u

/* Extern declarations for page tables */
extern uint64_t pml4[PML4_ENTRIES];
extern uint64_t pdpt[PDPT_ENTRIES];
extern uint64_t pd[PD_ENTRIES];
extern uint64_t pt[8][PT_ENTRIES];

/* Memory allocation flags */
#define KMALLOC_NORMAL  0x00
#define KMALLOC_ZEROED  0x01  /* Clear memory to zeros */
#define KMALLOC_ALIGNED 0x02  /* Return aligned memory (16-byte) */

typedef enum {
    MEMORY_SUCCESS              =  0,
    MEMORY_ERROR_INVALID_PARAMS = -1,
    MEMORY_ERROR_NOMEM          = -2,
    MEMORY_ERROR_BUSY           = -3,
    MEMORY_ERROR_NOT_ALIGNED    = -4,
    MEMORY_ERROR_OUT_OF_RANGE   = -5
} memory_status;

/*
 * mm_init — initialise the physical page allocator.
 *
 * Precondition:  called exactly once, before any mm_alloc_page call.
 * Precondition:  interrupts are disabled.
 * Postcondition: page_bitmap reflects all usable physical pages;
 *                kernel image pages are marked allocated.
 * Postcondition: next_free_page points to the first free page index,
 *                or equals MAX_PAGES if no free pages exist.
 *
 * boot_params may be NULL; in that case a conservative fallback is used.
 */
memory_status mm_init(boot_params *boot_params);

/*
 * mm_alloc_page — allocate one physical page (4 KB).
 * Returns physical address, or NULL if OOM.
 * NOT interrupt-safe; caller must disable interrupts if needed.
 */
void *mm_alloc_page(void);

/* mm_free_page — release a page previously returned by mm_alloc_page. */
void mm_free_page(void *page);

/*
 * mm_alloc_pages — allocate multiple contiguous physical pages.
 * Returns physical address, or NULL if OOM.
 */
void *mm_alloc_pages(uint32_t count);

/* mm_free_pages — release contiguous pages. */
void mm_free_pages(void *pages, uint32_t count);

/*
 * mm_map_page — insert a vaddr→paddr mapping into the current page tables.
 * flags: combination of MM_FLAG_* constants.
 */
int mm_map_page(uint64_t vaddr, uint32_t paddr, uint32_t flags);
int mm_unmap_page(uint64_t vaddr);

/*
 * mm_enable_paging — load CR3 and set CR0.PG.
 * Precondition: page tables built by build_page_tables() (called from mm_init).
 * Panics if page tables are not ready.
 */
void mm_enable_paging(void);

/*
 * mm_phys_to_virt — convert physical address to kernel virtual address.
 * In identity-mapped setup, this is a simple cast.
 */
static inline void *mm_phys_to_virt(uintptr_t phys) {
    return (void *)(phys);
}

/*
 * mm_virt_to_phys — convert kernel virtual address to physical address.
 * In identity-mapped setup, this is a simple cast.
 */
static inline uintptr_t mm_virt_to_phys(void *virt) {
    return (uintptr_t)virt;
}

/*
 * Kernel heap allocator (built on top of page allocator)
 */
void *kmalloc(size_t size, uint32_t flags);
void kfree(void *ptr);
void *kcalloc(size_t num, size_t size);
void *krealloc(void *ptr, size_t size);

/* Memory statistics */
uint32_t mm_get_free_pages(void);
uint32_t mm_get_total_pages(void);
uint32_t mm_get_used_pages(void);

/* Debug functions */
void mm_dump_bitmap(uint32_t start, uint32_t count);
void mm_dump_stats(void);

extern uint8_t  page_bitmap[MAX_PAGES / 8];
extern uint32_t next_free_page;
extern int page_tables_ready;

#endif /* KERNEL_MM_H */
