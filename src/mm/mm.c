/*
    E-comOS Kernel - Memory Manager
    Copyright (C) 2025,2026  Saladin5101
    Licensed under AGPL Version 3.

    Physical memory window managed: [PHYS_BASE, PHYS_BASE + PHYS_SIZE)
    = [0x100000, 0x1100000)  (1 MB … 17 MB, 4096 × 4 KB pages)

    Page table layout (64-bit, 4-level, identity-mapped):
      PML4[0] → PDPT[0] → PD[0..3] → PT[0..1023]  (covers 0 … 16 MB)
*/

#include <kernel/boot.h>
#include <kernel/mm.h>
#include <kernel/printkit/print.h>
#include <klibc/string.h>
#include <stdint.h>
#include <user_space/driver_loader.h>
/* Forward declarations */
extern void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags);
extern void isr14(void); /* Page fault handler from isr.s */

/* ------------------------------------------------------------------ */
/* Page Fault Error Codes                                              */
/* ------------------------------------------------------------------ */
#define PF_PRESENT (1u << 0)  /* Page is present */
#define PF_WRITE (1u << 1)    /* Write access */
#define PF_USER (1u << 2)     /* User mode */
#define PF_RESERVED (1u << 3) /* Reserved bit set */
#define PF_INSTR (1u << 4)    /* Instruction fetch */

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

/* Number of pages to reserve for the kernel image.
 * Computed precisely from linker symbols at runtime; this is the
 * upper bound used before mm_init has run (should never be needed). */
#define KERNEL_RESERVED_PAGES_FALLBACK 64u /* 256 KB conservative bound */

/* Kernel heap constants */
#define HEAP_START_VIRT 0x2000000u /* 32 MB - start of kernel heap */
#define HEAP_INITIAL_PAGES 16u     /* Initial 64KB heap */
#define HEAP_BLOCK_SIZE 16u        /* Minimum allocation size */
#define HEAP_ALIGNMENT 16u

/* Kernel heap block header */
typedef struct heap_block {
	size_t size; /* Block size (excluding header) */
	int free;    /* 1 = free, 0 = allocated */
	struct heap_block *next;
	struct heap_block *prev;
} heap_block_t;

/* ------------------------------------------------------------------ */
/* Global allocator state                                              */
/* ------------------------------------------------------------------ */
uint8_t page_bitmap[MAX_PAGES / 8] = {0};
uint32_t next_free_page = 0;
int page_tables_ready = 0;

/* Kernel heap state */
static heap_block_t *heap_free_list = NULL;
static uintptr_t heap_current = 0;
static uintptr_t heap_end = 0;

/* ------------------------------------------------------------------ */
/* 64-bit page table structures (4-level paging, identity map)        */
/* ------------------------------------------------------------------ */
#define PT_ENTRIES 512u /* 64-bit PT has 512 × 8-byte entries        */
#define PD_ENTRIES 512u
#define PDPT_ENTRIES 512u
#define PML4_ENTRIES 512u

/* Each PT covers 512 × 4 KB = 2 MB.  We need 8 PTs for 16 MB. */
#define NUM_PTS 8u

uint64_t pml4[PML4_ENTRIES] __attribute__((section(".paging"), aligned(PAGE_SIZE)));
uint64_t pdpt[PDPT_ENTRIES] __attribute__((section(".paging"), aligned(PAGE_SIZE)));
uint64_t pd[PD_ENTRIES] __attribute__((section(".paging"), aligned(PAGE_SIZE)));
uint64_t pt[NUM_PTS][PT_ENTRIES] __attribute__((section(".paging"), aligned(PAGE_SIZE)));

/* ------------------------------------------------------------------ */
/* Panic helper (no dependency on heap)                               */
/* ------------------------------------------------------------------ */
static void __attribute__((noreturn)) mm_panic(const char *msg) {
	print_str("MM PANIC: ", 0x4F);
	print_str(msg, 0x4F);
	__asm__ volatile("cli");
	while (1)
		__asm__ volatile("hlt");
}

/* ------------------------------------------------------------------ */
/* Bitmap helpers                                                      */
/* ------------------------------------------------------------------ */
static inline void bitmap_set(uint32_t idx) {
	page_bitmap[idx >> 3] |= (uint8_t)(1u << (idx & 7u));
}

static inline void bitmap_clear(uint32_t idx) {
	page_bitmap[idx >> 3] &= (uint8_t)~(1u << (idx & 7u));
}

static inline int bitmap_test(uint32_t idx) {
	return (page_bitmap[idx >> 3] >> (idx & 7u)) & 1u;
}

/* Forward declaration for page fault handler */
int handle_page_fault(uint64_t fault_addr, uint64_t error_code);

/* ------------------------------------------------------------------ */
/* Kernel heap management                                              */
/* ------------------------------------------------------------------ */

/* Expand kernel heap by allocating more pages */
static int heap_expand(size_t size) {
	size_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;

	/* Try to allocate contiguous pages */
	void *new_pages = mm_alloc_pages(pages_needed);
	if (!new_pages) {
		/* Try allocating individual pages */
		for (size_t i = 0; i < pages_needed; i++) {
			void *page = mm_alloc_page();
			if (!page) {
				/* Free previously allocated pages */
				for (size_t j = 0; j < i; j++) {
					mm_free_page((void *)((uintptr_t)new_pages + j * PAGE_SIZE));
				}
				return 0;
			}

			/* Map the page into kernel heap space */
			if (mm_map_page(HEAP_START_VIRT + (heap_end - HEAP_START_VIRT) + i * PAGE_SIZE,
			                (uint32_t)(uintptr_t)page,
			                MM_FLAG_KERNEL_RW) != 0) {
				mm_free_page(page);
				for (size_t j = 0; j < i; j++) {
					mm_free_page((void *)((uintptr_t)new_pages + j * PAGE_SIZE));
				}
				return 0;
			}
		}
	} else {
		/* Map contiguous pages */
		for (size_t i = 0; i < pages_needed; i++) {
			if (mm_map_page(HEAP_START_VIRT + (heap_end - HEAP_START_VIRT) + i * PAGE_SIZE,
			                (uint32_t)(uintptr_t)new_pages + i * PAGE_SIZE,
			                MM_FLAG_KERNEL_RW) != 0) {
				/* Cleanup on failure */
				for (size_t j = 0; j < i; j++) {
					mm_unmap_page(HEAP_START_VIRT + (heap_end - HEAP_START_VIRT) + j * PAGE_SIZE);
				}
				mm_free_pages(new_pages, pages_needed);
				return 0;
			}
		}
	}

	heap_end += pages_needed * PAGE_SIZE;
	return 1;
}

/* Find best fit free block in heap */
static heap_block_t *heap_find_best_fit(size_t size) {
	heap_block_t *best = NULL;
	heap_block_t *current = heap_free_list;

	while (current) {
		if (current->free && current->size >= size) {
			if (!best || current->size < best->size) {
				best = current;
			}
		}
		current = current->next;
	}

	return best;
}

/* Split a heap block if it's too large */
static void heap_split_block(heap_block_t *block, size_t size) {
	if (block->size <= size + sizeof(heap_block_t) + HEAP_BLOCK_SIZE) {
		return;
	}

	heap_block_t *new_block = (heap_block_t *)((uintptr_t)block + sizeof(heap_block_t) + size);
	new_block->size = block->size - size - sizeof(heap_block_t);
	new_block->free = 1;
	new_block->prev = block;
	new_block->next = block->next;

	if (block->next) {
		block->next->prev = new_block;
	}

	block->size = size;
	block->next = new_block;
}

/* Merge adjacent free heap blocks */
static void heap_merge_blocks(void) {
	heap_block_t *current = heap_free_list;

	while (current && current->next) {
		uintptr_t current_end = (uintptr_t)current + sizeof(heap_block_t) + current->size;
		uintptr_t next_start = (uintptr_t)current->next;

		if (current_end == next_start && current->free && current->next->free) {
			/* Merge current with next */
			current->size += sizeof(heap_block_t) + current->next->size;
			current->next = current->next->next;

			if (current->next) {
				current->next->prev = current;
			}
		} else {
			current = current->next;
		}
	}
}

/* ------------------------------------------------------------------ */
/* 64-bit identity page table setup                                   */
/* ------------------------------------------------------------------ */
static void build_page_tables(void) {
	/* Clear page tables */
	for (uint32_t i = 0; i < PML4_ENTRIES; i++)
		pml4[i] = 0;
	for (uint32_t i = 0; i < PDPT_ENTRIES; i++)
		pdpt[i] = 0;
	for (uint32_t i = 0; i < PD_ENTRIES; i++)
		pd[i] = 0;
	for (uint32_t i = 0; i < NUM_PTS; i++) {
		for (uint32_t j = 0; j < PT_ENTRIES; j++) {
			pt[i][j] = 0;
		}
	}

	/* PML4[0] → pdpt */
	pml4[0] = (uint64_t)(uintptr_t)pdpt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;

	/* PDPT[0] → pd */
	pdpt[0] = (uint64_t)(uintptr_t)pd | PTE_PRESENT | PTE_WRITABLE | PTE_USER;

	/* PD[i] → pt[i]  (each covers 2 MB) */
	/* Initialize all PD entries: PD[0..7] point to PTs, others are 0 */
	for (uint32_t i = 0; i < NUM_PTS; i++) {
		pd[i] = (uint64_t)(uintptr_t)pt[i] | PTE_PRESENT | PTE_WRITABLE | PTE_USER;

		/* Fill PT: identity-map 512 × 4 KB pages (0-16MB) */
		for (uint32_t j = 0; j < PT_ENTRIES; j++) {
			uint64_t phys = (uint64_t)i * PT_ENTRIES * PAGE_SIZE + (uint64_t)j * PAGE_SIZE;
			uint64_t flags = PTE_PRESENT | PTE_WRITABLE | PTE_USER;

			/* Make kernel pages global */
			if (phys >= 0x200000) {
				flags |= PTE_GLOBAL;
			}

			/* MODIFIED: Do not map user space so demand paging triggers! */
			if (phys >= 0x400000) {
				flags &= ~PTE_PRESENT;
			}

			pt[i][j] = phys | flags;
		}
	}

	page_tables_ready = 1;
}

/* ------------------------------------------------------------------ */
/* mm_init                                                            */
/* ------------------------------------------------------------------ */
memory_status mm_init(boot_params *boot_params) {
	/* Step 1: mark everything allocated (deny-by-default) */
	for (uint32_t i = 0; i < MAX_PAGES / 8u; i++)
		page_bitmap[i] = 0xFFu;
	next_free_page = MAX_PAGES; /* sentinel: no free pages yet */

	/* Step 2: determine kernel image extent from linker symbols */
	extern uint8_t _kernel_start[], _kernel_end[];
	uint64_t kern_start = (uint64_t)(uintptr_t)_kernel_start;
	uint64_t kern_end = (uint64_t)(uintptr_t)_kernel_end;

	/* Step 3: parse UEFI memory map or use fallback */
	if (!boot_params || !boot_params->memory_map || boot_params->memory_map_size == 0 || boot_params->memory_map_descriptor_size < sizeof(efi_memory_descriptor)) {
		/* Fallback: assume the entire managed window is conventional RAM
		 * except the kernel image pages. */
		for (uint32_t i = 0; i < MAX_PAGES; i++)
			bitmap_clear(i);

		/* Re-mark kernel pages as used */
		if (kern_start >= PHYS_BASE && kern_end > kern_start) {
			uint32_t k_first = (uint32_t)((kern_start - PHYS_BASE) / PAGE_SIZE);
			uint32_t k_last = (uint32_t)((kern_end - PHYS_BASE + PAGE_SIZE - 1u) / PAGE_SIZE);
			if (k_last > MAX_PAGES)
				k_last = MAX_PAGES;
			for (uint32_t i = k_first; i < k_last; i++)
				bitmap_set(i);
		} else {
			/* Linker symbols unavailable — use conservative bound */
			for (uint32_t i = 0; i < KERNEL_RESERVED_PAGES_FALLBACK; i++)
				bitmap_set(i);
		}

		print_str("MM: fallback map (no UEFI params)\n", 0x0E);
		goto find_first;
	}

	/* Step 4: walk UEFI memory map */
	{
		const uint8_t *base = (const uint8_t *)boot_params->memory_map;
		uint64_t stride = boot_params->memory_map_descriptor_size;
		uint64_t num_descs = boot_params->memory_map_size / stride;

		for (uint64_t d = 0; d < num_descs; d++) {
			const efi_memory_descriptor *desc =
			    (const efi_memory_descriptor *)(base + d * stride);

			if (desc->type != EFI_CONVENTIONAL_MEMORY)
				continue;

			uint64_t region_start = desc->physical_start;
			uint64_t region_pages = desc->number_of_pages;

			/* Overflow guard */
			if (region_pages > (PHYS_SIZE / PAGE_SIZE))
				region_pages = PHYS_SIZE / PAGE_SIZE;

			for (uint64_t p = 0; p < region_pages; p++) {
				uint64_t phys = region_start + p * (uint64_t)PAGE_SIZE;

				if (phys < PHYS_BASE)
					continue;
				if (phys >= PHYS_BASE + PHYS_SIZE)
					break;

				uint32_t idx = (uint32_t)((phys - PHYS_BASE) / PAGE_SIZE);
				bitmap_clear(idx);
			}
		}
	}

	/* Step 5: re-mark kernel image pages as used */
	if (kern_start >= PHYS_BASE && kern_end > kern_start) {
		uint32_t k_first = (uint32_t)((kern_start - PHYS_BASE) / PAGE_SIZE);
		uint32_t k_last = (uint32_t)((kern_end - PHYS_BASE + PAGE_SIZE - 1u) / PAGE_SIZE);
		if (k_last > MAX_PAGES)
			k_last = MAX_PAGES;
		for (uint32_t i = k_first; i < k_last; i++)
			bitmap_set(i);
	} else {
		for (uint32_t i = 0; i < KERNEL_RESERVED_PAGES_FALLBACK; i++)
			bitmap_set(i);
	}

find_first:
	/* Step 6: find first free page */
	/* NEW FIX: Forcefully protect the first 2MB of Physical RAM.
	 * This safeguards the kernel, the UEFI/Multiboot info structures,
	 * and any modules GRUB loaded immediately after the kernel.
	 */
	for (uint32_t i = 0; i < 512; i++) {
		bitmap_set(i);
	}
	next_free_page = MAX_PAGES;

	for (uint32_t i = 0; i < MAX_PAGES; i++) {
		if (!bitmap_test(i)) {
			next_free_page = i;
			break;
		}
	}

	/* Step 7: build page tables */
	build_page_tables();

	/* Step 8: initialize kernel heap */
	heap_current = HEAP_START_VIRT;
	heap_end = HEAP_START_VIRT;

	/* Allocate initial heap pages */
	for (uint32_t i = 0; i < HEAP_INITIAL_PAGES; i++) {
		void *page = mm_alloc_page();
		if (!page) {
			print_str("MM: failed to allocate initial heap page ", 0x0C);
			print_num(i, 0x0C);
			print_str("\n", 0x0C);
			break;
		}

		/* Map the page into kernel heap space */
		if (mm_map_page(HEAP_START_VIRT + i * PAGE_SIZE,
		                (uintptr_t)page,
		                MM_FLAG_KERNEL_RW) != 0) {
			mm_free_page(page);
			print_str("MM: failed to map heap page ", 0x0C);
			print_num(i, 0x0C);
			print_str("\n", 0x0C);
			break;
		}

		heap_end += PAGE_SIZE;
	}

	/* Initialize heap free list */
	if (heap_end > heap_current) {
		heap_free_list = (heap_block_t *)heap_current;
		heap_free_list->size = heap_end - heap_current - sizeof(heap_block_t);
		heap_free_list->free = 1;
		heap_free_list->next = NULL;
		heap_free_list->prev = NULL;
	}

	/* Step 9: report */
	uint32_t free_count = 0;
	for (uint32_t i = 0; i < MAX_PAGES; i++)
		if (!bitmap_test(i))
			free_count++;

	print_str("MM: free pages: ", 0x0A);
	print_num(free_count, 0x0A);
	print_str(" / ", 0x0A);
	print_num(MAX_PAGES, 0x0A);
	print_str("  first free: ", 0x0A);
	print_num(next_free_page, 0x0A);
	print_str("  heap: 0x", 0x0A);
	print_hex(HEAP_START_VIRT, 0x0A);
	print_str("-0x", 0x0A);
	print_hex(heap_end, 0x0A);
	print_str("\n", 0x0A);

	if (free_count == 0)
		return MEMORY_ERROR_NOMEM;

	/* Register page fault handler in IDT */
	print_str("MM: Registering page fault handler...\n", 0x0A);
	extern void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags);
	/* Use the page fault handler from isr.c (isr14 -> isr_handler) */
	uint64_t handler_addr = (uint64_t)(uintptr_t)isr14;
	idt_set_gate(14, handler_addr, 0x08, 0x8E); /* Gate 14 = Page Fault */
	print_str("  Page fault handler registered at 0x", 0x0A);
	print_hex(handler_addr, 0x0A);
	print_str("\n", 0x0A);

	/* Pre-map BOOT_INFO_ADDR (0x600000) for early use */
	print_str("MM: Pre-mapping BOOT_INFO_ADDR at 0x600000...\n", 0x0A);
	void *boot_info_page = mm_alloc_page();
	if (!boot_info_page) {
		print_str("MM: Failed to allocate boot_info page\n", 0x0C);
		return MEMORY_ERROR_NOMEM;
	}
	/* Zero the page */
	uint8_t *p = (uint8_t *)boot_info_page;
	for (uint32_t i = 0; i < PAGE_SIZE; i++)
		p[i] = 0;

	if (mm_map_page(BOOT_INFO_ADDR, (uintptr_t)boot_info_page, MM_FLAG_USER) != 0) {
		print_str("MM: Failed to map boot_info page at 0x600000\n", 0x0C);
		return MEMORY_ERROR_NOMEM;
	}
	print_str("MM: Boot info page mapped at 0x600000\n", 0x0A);

	return MEMORY_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* mm_alloc_page                                                      */
/* ------------------------------------------------------------------ */
void *mm_alloc_page(void) {
	for (uint32_t i = next_free_page; i < MAX_PAGES; i++) {
		if (!bitmap_test(i)) {
			bitmap_set(i);
			next_free_page = i + 1u;
			return (void *)(uintptr_t)(PHYS_BASE + (uint64_t)i * PAGE_SIZE);
		}
	}
	return NULL;
}

/* ------------------------------------------------------------------ */
/* mm_free_page                                                       */
/* ------------------------------------------------------------------ */
void mm_free_page(void *page) {
	uint64_t addr = (uint64_t)(uintptr_t)page;
	if (addr < PHYS_BASE || addr >= PHYS_BASE + PHYS_SIZE)
		return;
	if (addr & (PAGE_SIZE - 1u))
		return; /* not page-aligned — refuse silently */
	uint32_t idx = (uint32_t)((addr - PHYS_BASE) / PAGE_SIZE);
	bitmap_clear(idx);
	if (idx < next_free_page)
		next_free_page = idx;
}

/* ------------------------------------------------------------------ */
/* mm_alloc_pages                                                      */
/* ------------------------------------------------------------------ */
void *mm_alloc_pages(uint32_t count) {
	if (count == 0)
		return NULL;
	if (count == 1)
		return mm_alloc_page();

	/* Find contiguous free pages */
	for (uint32_t i = 0; i <= MAX_PAGES - count; i++) {
		uint32_t j;
		for (j = 0; j < count; j++) {
			if (bitmap_test(i + j))
				break;
		}
		if (j == count) {
			/* Found contiguous free pages */
			for (j = 0; j < count; j++) {
				bitmap_set(i + j);
			}
			if (i < next_free_page) {
				next_free_page = i + count;
			}
			return (void *)(uintptr_t)(PHYS_BASE + (uint64_t)i * PAGE_SIZE);
		}
	}
	return NULL; /* No contiguous block found */
}

/* ------------------------------------------------------------------ */
/* mm_free_pages                                                       */
/* ------------------------------------------------------------------ */
void mm_free_pages(void *pages, uint32_t count) {
	uint64_t addr = (uint64_t)(uintptr_t)pages;
	if (addr < PHYS_BASE || addr >= PHYS_BASE + PHYS_SIZE)
		return;
	if (addr & (PAGE_SIZE - 1u))
		return;

	uint32_t first = (uint32_t)((addr - PHYS_BASE) / PAGE_SIZE);
	for (uint32_t i = 0; i < count; i++) {
		bitmap_clear(first + i);
	}

	if (first < next_free_page) {
		next_free_page = first;
	}
}

/* ------------------------------------------------------------------ */
/* mm_map_page                                                         */
/* ------------------------------------------------------------------ */
int mm_map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags) {
	print_str("mm_map_page called: vaddr=0x", 0x0E);
	print_hex(vaddr, 0x0E);
	print_str("\n", 0x0E);

	if (!page_tables_ready)
		return -1;

	/* Validate addresses */
	if (vaddr & (PAGE_SIZE - 1)) {
		print_str("MM: vaddr not aligned\n", 0x0C);
		return -1;
	}
	if (paddr & (PAGE_SIZE - 1)) {
		print_str("MM: paddr not aligned\n", 0x0C);
		return -1;
	}
	if (vaddr >= PHYS_BASE + PHYS_SIZE) {
		print_str("MM: vaddr out of range\n", 0x0C);
		return -1;
	}
	if (paddr >= PHYS_BASE + PHYS_SIZE) {
		print_str("MM: paddr out of range\n", 0x0C);
		return -1;
	}

	uint64_t pml4_idx = (vaddr >> 39) & 0x1FF;
	uint64_t pdp_idx = (vaddr >> 30) & 0x1FF;
	uint64_t pd_idx = (vaddr >> 21) & 0x1FF;
	uint64_t pt_idx = (vaddr >> 12) & 0x1FF;

	uint64_t *cur = pml4;

	if (!(cur[pml4_idx] & PTE_PRESENT)) {
		void *new_table = mm_alloc_page();
		if (!new_table) {
			print_str("MM: failed to allocate PML4 entry table\n", 0x0C);
			return -1;
		}
		/* Zero the new table */
		uint64_t *tbl = (uint64_t *)(uintptr_t)new_table;
		for (uint32_t i = 0; i < PT_ENTRIES; i++)
			tbl[i] = 0;
		cur[pml4_idx] = (uint64_t)(uintptr_t)new_table | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
		print_str("MM: allocated PML4 entry table at 0x", 0x0A);
		print_hex((uintptr_t)new_table, 0x0A);
		print_str("\n", 0x0A);
	}
	uint64_t *pdp = (uint64_t *)(uintptr_t)(cur[pml4_idx] & ~0xFFFULL);

	if (!(pdp[pdp_idx] & PTE_PRESENT)) {
		void *new_table = mm_alloc_page();
		if (!new_table) {
			print_str("MM: failed to allocate PDPT entry table\n", 0x0C);
			return -1;
		}
		/* Zero the new table */
		uint64_t *tbl = (uint64_t *)(uintptr_t)new_table;
		for (uint32_t i = 0; i < PT_ENTRIES; i++)
			tbl[i] = 0;
		pdp[pdp_idx] = (uint64_t)(uintptr_t)new_table | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
		print_str("MM: allocated PDPT entry table at 0x", 0x0A);
		print_hex((uintptr_t)new_table, 0x0A);
		print_str("\n", 0x0A);
	}
	uint64_t *pd = (uint64_t *)(uintptr_t)(pdp[pdp_idx] & ~0xFFFULL);

	if (!(pd[pd_idx] & PTE_PRESENT)) {
		void *new_table = mm_alloc_page();
		if (!new_table) {
			print_str("MM: failed to allocate PD entry table\n", 0x0C);
			return -1;
		}
		/* Zero the new table */
		uint64_t *tbl = (uint64_t *)(uintptr_t)new_table;
		for (uint32_t i = 0; i < PT_ENTRIES; i++)
			tbl[i] = 0;
		pd[pd_idx] = (uint64_t)(uintptr_t)new_table | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
		print_str("MM: allocated PD entry table at 0x", 0x0A);
		print_hex((uintptr_t)new_table, 0x0A);
		print_str("\n", 0x0A);
	}
	uint64_t *pt = (uint64_t *)(uintptr_t)(pd[pd_idx] & ~0xFFFULL);

	/* Check if entry already exists */
	if (pt[pt_idx] & PTE_PRESENT) {
		print_str("MM: entry already present, overwriting\n", 0x0E);
	}

	uint64_t entry = (paddr & ~0xFFFULL) | PTE_PRESENT;
	if (flags & MM_FLAG_WRITE)
		entry |= PTE_WRITABLE;
	if (flags & MM_FLAG_USER)
		entry |= PTE_USER;
	if (flags & MM_FLAG_PCD)
		entry |= PTE_PCD;
	if (flags & MM_FLAG_GLOBAL)
		entry |= PTE_GLOBAL;

	/* Handle NX (No-Execute) bit - bit 63 in page table entry */
	if (!(flags & MM_FLAG_NX)) {
		/* If NX flag is NOT set, allow execution (clear bit 63) */
		entry &= ~(1ULL << 63);
	} else {
		/* If NX flag IS set, disable execution (set bit 63) */
		entry |= (1ULL << 63);
	}

	pt[pt_idx] = entry;
	__asm__ volatile("invlpg (%0)" : : "r"((uintptr_t)vaddr) : "memory");

	print_str("MM: mapped 0x", 0x0A);
	print_hex(vaddr, 0x0A);
	print_str(" -> 0x", 0x0A);
	print_hex(paddr, 0x0A);
	print_str(" (entry=0x", 0x0A);
	print_hex(entry, 0x0A);
	print_str(")\n", 0x0A);

	return 0;
}

/* ------------------------------------------------------------------ */
/* mm_unmap_page                                                       */
/* ------------------------------------------------------------------ */
int mm_unmap_page(uint64_t vaddr) {
	if (!page_tables_ready)
		return -1;

	uint64_t pml4_idx = (vaddr >> 39) & 0x1FF;
	uint64_t pdp_idx = (vaddr >> 30) & 0x1FF;
	uint64_t pd_idx = (vaddr >> 21) & 0x1FF;
	uint64_t pt_idx = (vaddr >> 12) & 0x1FF;

	uint64_t *pdp = (uint64_t *)(uintptr_t)(pml4[pml4_idx] & ~0xFFFULL);
	if (!(pml4[pml4_idx] & PTE_PRESENT))
		return -1;

	uint64_t *pd = (uint64_t *)(uintptr_t)(pdp[pdp_idx] & ~0xFFFULL);
	if (!(pdp[pdp_idx] & PTE_PRESENT))
		return -1;

	uint64_t *pt = (uint64_t *)(uintptr_t)(pd[pd_idx] & ~0xFFFULL);
	if (!(pd[pd_idx] & PTE_PRESENT))
		return -1;

	pt[pt_idx] = 0;
	__asm__ volatile("invlpg (%0)" : : "r"((uintptr_t)vaddr) : "memory");
	return 0;
}

/* ------------------------------------------------------------------ */
/* mm_enable_paging                                                    */
/* ------------------------------------------------------------------ */
void mm_enable_paging(void) {
	if (!page_tables_ready)
		mm_panic("mm_enable_paging called before page tables are built");

	/* Write PML4 to CR3 */
	__asm__ volatile("movq %0, %%cr3" : : "r"((uintptr_t)pml4) : "memory");

	/* Enable PAE (should already be enabled in long mode setup) */
	uint64_t cr4;
	__asm__ volatile("movq %%cr4, %0" : "=r"(cr4));
	cr4 |= (1 << 5); /* Set PAE bit */
	__asm__ volatile("movq %0, %%cr4" : : "r"(cr4));

	/* Enable paging */
	uint64_t cr0;
	__asm__ volatile("movq %%cr0, %0" : "=r"(cr0));
	cr0 |= (1 << 31); /* Set PG bit */
	__asm__ volatile("movq %0, %%cr0" : : "r"(cr0));

	/* Invalidate TLB */
	__asm__ volatile("movq %%cr3, %%rax\n"
	                 "movq %%rax, %%cr3" : : : "rax");

	print_str("MM: paging enabled\n", 0x0A);

	/* Add debug information after enabling paging */
	print_str("MM: paging enabled, CR3=0x", 0x0A);
	print_hex((uintptr_t)pml4, 0x0A);
	print_str("\n", 0x0A);

	/* Check user program page mapping */
	print_str("MM: Checking user program page at 0x400000\n", 0x0E);
	uint64_t entry = pt[2][0]; /* Page table entry for 0x400000 */
	print_str("  Page table entry: 0x", 0x0E);
	print_hex(entry, 0x0E);
	print_str("\n  Present: ", 0x0E);
	print_str((entry & PTE_PRESENT) ? "Yes" : "No", 0x0E);
	print_str(", Writable: ", 0x0E);
	print_str((entry & PTE_WRITABLE) ? "Yes" : "No", 0x0E);
	print_str(", User: ", 0x0E);
	print_str((entry & PTE_USER) ? "Yes" : "No", 0x0E);
	print_str("\n", 0x0E);
}

/* ------------------------------------------------------------------ */
/* Kernel heap allocator (kmalloc/kfree)                               */
/* ------------------------------------------------------------------ */

void *kmalloc(size_t size, uint32_t flags) {
	if (size == 0)
		return NULL;

	/* Align size to HEAP_BLOCK_SIZE */
	size = (size + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1);
	if (size < HEAP_BLOCK_SIZE) {
		size = HEAP_BLOCK_SIZE;
	}

	/* Add header size */
	size_t total_size = size + sizeof(heap_block_t);

	/* Find best fit block */
	heap_block_t *block = heap_find_best_fit(size);

	/* If no suitable block found, expand heap */
	if (!block) {
		if (!heap_expand(total_size)) {
			return NULL;
		}
		/* Try again after expansion */
		block = heap_find_best_fit(size);
		if (!block) {
			return NULL;
		}
	}

	/* Remove from free list */
	if (block->prev) {
		block->prev->next = block->next;
	} else {
		heap_free_list = block->next;
	}

	if (block->next) {
		block->next->prev = block->prev;
	}

	/* Split block if it's too large */
	heap_split_block(block, size);

	/* Mark as allocated */
	block->free = 0;

	/* Zero memory if requested */
	if (flags & KMALLOC_ZEROED) {
		uint8_t *ptr = (uint8_t *)(block + 1);
		for (size_t i = 0; i < size; i++) {
			ptr[i] = 0;
		}
	}

	/* Return pointer to data area */
	return (void *)(block + 1);
}

void kfree(void *ptr) {
	if (!ptr)
		return;

	/* Get block header */
	heap_block_t *block = (heap_block_t *)ptr - 1;

	/* Validate pointer */
	uintptr_t addr = (uintptr_t)block;
	if (addr < HEAP_START_VIRT || addr >= heap_end) {
		print_str("kfree: invalid pointer 0x", 0x0C);
		print_hex(addr, 0x0C);
		print_str("\n", 0x0C);
		return;
	}

	if (block->free) {
		print_str("kfree: double free at 0x", 0x0C);
		print_hex(addr, 0x0C);
		print_str("\n", 0x0C);
		return;
	}

	/* Mark as free */
	block->free = 1;

	/* Add to free list (sorted by address) */
	heap_block_t *current = heap_free_list;
	heap_block_t *prev = NULL;

	while (current && (uintptr_t)current < (uintptr_t)block) {
		prev = current;
		current = current->next;
	}

	block->prev = prev;
	block->next = current;

	if (prev) {
		prev->next = block;
	} else {
		heap_free_list = block;
	}

	if (current) {
		current->prev = block;
	}

	/* Merge with adjacent free blocks */
	heap_merge_blocks();
}

void *kcalloc(size_t num, size_t size) {
	size_t total = num * size;
	if (total == 0)
		return NULL;

	/* Check for overflow */
	if (size != 0 && total / size != num) {
		return NULL;
	}

	void *ptr = kmalloc(total, KMALLOC_ZEROED);
	return ptr;
}

void *krealloc(void *ptr, size_t size) {
	if (!ptr) {
		return kmalloc(size, 0);
	}

	if (size == 0) {
		kfree(ptr);
		return NULL;
	}

	heap_block_t *block = (heap_block_t *)ptr - 1;
	if (block->size >= size) {
		/* Block is already large enough */
		return ptr;
	}

	/* Allocate new block */
	void *new_ptr = kmalloc(size, 0);
	if (!new_ptr) {
		return NULL;
	}

	/* Copy data */
	uint8_t *src = (uint8_t *)ptr;
	uint8_t *dst = (uint8_t *)new_ptr;
	for (size_t i = 0; i < block->size && i < size; i++) {
		dst[i] = src[i];
	}

	/* Free old block */
	kfree(ptr);

	return new_ptr;
}

/* ------------------------------------------------------------------ */
/* Memory statistics                                                   */
/* ------------------------------------------------------------------ */
uint32_t mm_get_free_pages(void) {
	uint32_t count = 0;
	for (uint32_t i = 0; i < MAX_PAGES; i++) {
		if (!bitmap_test(i))
			count++;
	}
	return count;
}

uint32_t mm_get_total_pages(void) {
	return MAX_PAGES;
}

uint32_t mm_get_used_pages(void) {
	return MAX_PAGES - mm_get_free_pages();
}

/* ------------------------------------------------------------------ */
/* Debug functions                                                     */
/* ------------------------------------------------------------------ */
void mm_dump_bitmap(uint32_t start, uint32_t count) {
	if (start >= MAX_PAGES)
		return;
	if (start + count > MAX_PAGES)
		count = MAX_PAGES - start;

	print_str("Page bitmap [", 0x0E);
	print_num(start, 0x0E);
	print_str("-", 0x0E);
	print_num(start + count - 1, 0x0E);
	print_str("]: ", 0x0E);

	for (uint32_t i = 0; i < count; i++) {
		if (i > 0 && i % 64 == 0) {
			print_str("\n", 0x0E);
		}
		print_char(bitmap_test(start + i) ? 'X' : '.', 0x0E);
	}
	print_str("\n", 0x0E);
}

void mm_dump_stats(void) {
	uint32_t free_pages = mm_get_free_pages();
	uint32_t used_pages = mm_get_used_pages();
	uint32_t total_pages = mm_get_total_pages();

	print_str("Memory Statistics:\n", 0x0E);
	print_str("  Total pages:  ", 0x0E);
	print_num(total_pages, 0x0E);
	print_str(" (", 0x0E);
	print_num(total_pages * 4, 0x0E);
	print_str(" KB)\n", 0x0E);

	print_str("  Used pages:   ", 0x0E);
	print_num(used_pages, 0x0E);
	print_str(" (", 0x0E);
	print_num(used_pages * 4, 0x0E);
	print_str(" KB)\n", 0x0E);

	print_str("  Free pages:   ", 0x0E);
	print_num(free_pages, 0x0E);
	print_str(" (", 0x0E);
	print_num(free_pages * 4, 0x0E);
	print_str(" KB)\n", 0x0E);

	print_str("  Heap:         0x", 0x0E);
	print_hex(HEAP_START_VIRT, 0x0E);
	print_str(" - 0x", 0x0E);
	print_hex(heap_end, 0x0E);
	print_str(" (", 0x0E);
	print_num((heap_end - HEAP_START_VIRT) / 1024, 0x0E);
	print_str(" KB)\n", 0x0E);
}

/* ------------------------------------------------------------------ */
/* Check page mapping status (debug)                                   */
/* ------------------------------------------------------------------ */
void mm_check_page(uint64_t vaddr) {
	uint64_t pml4_idx = (vaddr >> 39) & 0x1FF;
	uint64_t pdp_idx = (vaddr >> 30) & 0x1FF;
	uint64_t pd_idx = (vaddr >> 21) & 0x1FF;
	uint64_t pt_idx = (vaddr >> 12) & 0x1FF;

	print_str("MM: Page 0x", 0x0E);
	print_hex(vaddr, 0x0E);
	print_str(" (PML4[", 0x0E);
	print_num(pml4_idx, 0x0E);
	print_str("] PDPT[", 0x0E);
	print_num(pdp_idx, 0x0E);
	print_str("] PD[", 0x0E);
	print_num(pd_idx, 0x0E);
	print_str("] PT[", 0x0E);
	print_num(pt_idx, 0x0E);
	print_str("]): ", 0x0E);

	/* Check PML4 */
	if (!(pml4[pml4_idx] & PTE_PRESENT)) {
		print_str("[NOT PRESENT] PML4 entry\n", 0x0C);
		return;
	}
	uint64_t *pdp = (uint64_t *)(uintptr_t)(pml4[pml4_idx] & ~0xFFFULL);

	/* Check PDPT */
	if (!(pdp[pdp_idx] & PTE_PRESENT)) {
		print_str("[NOT PRESENT] PDPT entry\n", 0x0C);
		return;
	}
	uint64_t *pd = (uint64_t *)(uintptr_t)(pdp[pdp_idx] & ~0xFFFULL);

	/* Check PD */
	if (!(pd[pd_idx] & PTE_PRESENT)) {
		print_str("[NOT PRESENT] PD entry\n", 0x0C);
		return;
	}
	uint64_t *pt = (uint64_t *)(uintptr_t)(pd[pd_idx] & ~0xFFFULL);

	/* Check PT */
	uint64_t entry = pt[pt_idx];

	if (entry & PTE_PRESENT) {
		print_str("[P", 0x0A);
		if (entry & PTE_WRITABLE)
			print_str("W", 0x0A);
		if (entry & PTE_USER)
			print_str("U", 0x0A);
		if (entry & PTE_GLOBAL)
			print_str("G", 0x0A);
		print_str("] Phys=0x", 0x0A);
		print_hex64((entry & ~0xFFFULL), 0x0A);
	} else {
		print_str("[NOT PRESENT] PT entry\n", 0x0C);
	}
}

/* ------------------------------------------------------------------ */
/* Dynamic page fault handler                                          */
/* ------------------------------------------------------------------ */
/*
 * handle_page_fault - Handle a page fault by dynamically allocating a page
 *
 * This function is called from isr_handler AFTER the interrupt stub has
 * saved all registers and switched to a known-good kernel stack.
 *
 * Parameters:
 *   fault_addr - Virtual address that caused the fault (from CR2)
 *   error_code - Page fault error code (P, W/R, U/S, RSV, I/D bits)
 *
 * Return:
 *   1 if page fault was handled (map a new page)
 *   0 if unhandled (should halt)
 *
 * IMPORTANT: This function must NOT be called directly from an ISR stub!
 * The ISR stub must save all registers first, then call isr_handler,
 * which then calls this function safely.
 */
int handle_page_fault(uint64_t fault_addr, uint64_t error_code) {
	/* Calculate the 4KB aligned page boundary */
	uint64_t page_start = fault_addr & ~0xFFFULL;

	/* Verify that the faulting address is within the valid user-space region */
	if (fault_addr >= 0x400000 && fault_addr < 0x800000) {

		/* Allocate a fresh physical page */
		void *page = mm_alloc_page();
		if (!page) {
			print_str("MM: Page fault out of memory\n", 0x0C);
			return 0; /* Return 0 for failure */
		}

		/* Zero-initialize the page by default
		 * (This is crucial because any space beyond the binary file's end
		 * inside the page acts as the zeroed .bss section) */
		uint8_t *p = (uint8_t *)page;
		for (uint32_t i = 0; i < PAGE_SIZE; i++) {
			p[i] = 0;
		}

		extern uint8_t _binary_payload_init_bin_start[];
		extern uint8_t _binary_payload_init_bin_end[];
		extern uint8_t _binary_payload_ebts_bin_start[];
		extern uint8_t _binary_payload_ebts_bin_end[];

		uint64_t init_size = (uint64_t)(_binary_payload_init_bin_end - _binary_payload_init_bin_start);
		uint64_t ebts_size = (uint64_t)(_binary_payload_ebts_bin_end - _binary_payload_ebts_bin_start);

		/* Case 1: Check if fault falls anywhere inside init.bin's memory range */
		if (page_start >= 0x400000 && page_start < (0x400000 + init_size)) {
			uint64_t offset = page_start - 0x400000;
			uint64_t remaining_bytes = init_size - offset;
			uint64_t copy_size = (remaining_bytes < PAGE_SIZE) ? remaining_bytes : PAGE_SIZE;

			for (uint64_t i = 0; i < copy_size; i++) {
				p[i] = _binary_payload_init_bin_start[offset + i];
			}
		}
		/* Case 2: Check if fault falls anywhere inside ebts.bin's memory range */
		else if (page_start >= 0x500000 && page_start < (0x500000 + ebts_size)) {
			uint64_t offset = page_start - 0x500000;
			uint64_t remaining_bytes = ebts_size - offset;
			uint64_t copy_size = (remaining_bytes < PAGE_SIZE) ? remaining_bytes : PAGE_SIZE;

			for (uint64_t i = 0; i < copy_size; i++) {
				p[i] = _binary_payload_ebts_bin_start[offset + i];
			}
		}
		/* Case 3: If it falls in user memory but outside the binary sizes
		 * (like the heap or stack), the page simply remains zeroed. */

		/* Map the virtual page to the physical frame */
		if (mm_map_page(page_start, (uintptr_t)page, MM_FLAG_USER | MM_FLAG_WRITE) != 0) {
			mm_free_page(page);
			print_str("MM: Failed to map page fault address 0x", 0x0C);
			print_hex(page_start, 0x0C);
			print_str("\n", 0x0C);
			return 0; /* Return 0 for failure */
		}

		/* Invalidate the TLB cache for this virtual address */
		__asm__ volatile("invlpg (%0)" : : "r"(page_start) : "memory");

		return 1; /* Return 1 (non-zero) for successful resolution */
	}

	/* Unhandled or out-of-bounds fault */
	print_str("MM: Unhandled page fault at virtual address 0x", 0x0C);
	print_hex(fault_addr, 0x0C);
	print_str(" (error code: ", 0x0C);
	print_num(error_code, 0x0C);
	print_str(")\n", 0x0C);

	return 0; /* Return 0 for failure */
}
