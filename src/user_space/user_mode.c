/*
	User mode management
	Handles transitions between kernel mode and user mode.
*/

#include <kernel/arch/interrupts.h>
#include <kernel/boot.h>
#include <kernel/debug.h>
#include <kernel/mm.h>
#include <kernel/printkit/print.h>
#include <stdint.h>
#include <user_space/user_mode.h>

/* GDT segment selectors for user mode */
#define USER_CODE_SELECTOR 0x23 /* index 3, DPL=3, 64-bit code */
#define USER_DATA_SELECTOR 0x1B /* index 4, DPL=3, data         */

extern uint64_t driver_phys_addr;
extern uint64_t driver_size;

extern uint8_t _binary_payload_init_bin_start[];
extern uint8_t _binary_payload_init_bin_end[];
extern uint8_t _binary_payload_ebts_bin_start[];
extern uint8_t _binary_payload_ebts_bin_end[];

/* --------------------------------------------------------------- */
/* create_user_pagetable                                           */
/* --------------------------------------------------------------- */
uintptr_t create_user_pagetable(void) {
	// Allocate new P4 table for user
	uintptr_t user_p4 = (uintptr_t)mm_alloc_page();
	if (!user_p4)
		kernel_panic("Failed to allocate user P4");

	// Copy all kernel P4 entries to user P4
	uint64_t* kernel_p4 = pml4; // Use the actual kernel PML4 array
	uint64_t* new_p4 = (uint64_t*)user_p4;
	for (int i = 0; i < 512; i++) {
		new_p4[i] = kernel_p4[i];
	}

	// User mappings are already installed in kernel page table
	return user_p4;
}

/* --------------------------------------------------------------- */
/* switch_to_user_mode                                             */
/* --------------------------------------------------------------- */
void __attribute__((noreturn)) switch_to_user_mode(uintptr_t entry_point,
												   uintptr_t stack_pointer) {
	uint64_t user_rip = entry_point;
	uint64_t user_rsp = stack_pointer;

	// Ensure 16-byte stack alignment
	if ((user_rsp & 0xF) != 0) {
		user_rsp &= ~0xFULL; // Align to 16-byte boundary
	}

	print_str("Switching to user mode...\n", 0x0F);
	print_str("  RIP: 0x", 0x0F);
	print_hex(user_rip, 0x0F);
	print_str("\n  RSP: 0x", 0x0F);
	print_hex(user_rsp, 0x0F);
	print_str(" (aligned: ", 0x0F);
	print_str((user_rsp & 0xF) == 0 ? "yes" : "no", 0x0F);
	print_str(")\n", 0x0F);

	print_str("  CS: 0x1B (index 3, DPL=3)\n", 0x0F);
	print_str("  SS: 0x23 (index 4, DPL=3)\n", 0x0F);

	// Kernel page tables already have PTE_USER on all entries;
	// no need to create a separate user CR3.

	// Call the assembly function
	extern void asm_switch_to_user_mode(uintptr_t, uintptr_t)
		__attribute__((noreturn));
	asm_switch_to_user_mode(user_rip, user_rsp);

	__builtin_unreachable();
}

/* --------------------------------------------------------------- */
/* load_init_service_to_user_mode                                  */
/* --------------------------------------------------------------- */
int load_init_service_to_user_mode(void) {
	print_str("Loading init-service to user mode...\n", 0x0F);

	/* 1. Allocate and map boot_info page at 0x600000 */
	void* boot_pa = mm_alloc_page();
	if (!boot_pa)
		kernel_panic("OOM: boot_info page");
	if (mm_map_page(BOOT_INFO_ADDR, (uintptr_t)boot_pa, MM_FLAG_USER_RO) != 0) {
		kernel_panic("Failed to map boot_info page at 0x600000");
	}

	/* 2. Load init.bin to 0x400000 */
	uint8_t* init_src = _binary_payload_init_bin_start;
	uint64_t init_size = (uint64_t)(_binary_payload_init_bin_end -
									_binary_payload_init_bin_start);

	if (init_size == 0 || init_size > 0x100000)
		kernel_panic("init.bin missing or too large");

	uint32_t init_pages = (uint32_t)((init_size + PAGE_SIZE - 1) / PAGE_SIZE);
	for (uint32_t p = 0; p < init_pages; p++) {
		void* pa = mm_alloc_page();
		if (!pa)
			kernel_panic("OOM: init.bin page");
		mm_map_page((INIT_LOAD_ADDR + p * PAGE_SIZE), (uintptr_t)pa,
					MM_FLAG_USER_RX);
	}

	uint8_t* init_dst = (uint8_t*)INIT_LOAD_ADDR;
	for (uint64_t i = 0; i < init_size; i++)
		init_dst[i] = init_src[i];

	print_str("  init.bin: ", 0x0F);
	print_num((uint32_t)init_size, 0x0F);
	print_str(" bytes -> 0x400000\n", 0x0F);

	/* 3. Load ebts.bin to 0x500000 */
	uint8_t* ebts_src = _binary_payload_ebts_bin_start;
	uint64_t ebts_size = (uint64_t)(_binary_payload_ebts_bin_end -
									_binary_payload_ebts_bin_start);

	if (ebts_size > 0 && ebts_size <= 0x100000) {
		uint32_t ebts_pages =
			(uint32_t)((ebts_size + PAGE_SIZE - 1) / PAGE_SIZE);
		for (uint32_t p = 0; p < ebts_pages; p++) {
			void* pa = mm_alloc_page();
			if (!pa)
				kernel_panic("OOM: ebts.bin page");
			mm_map_page((EBTS_LOAD_ADDR + p * PAGE_SIZE), (uintptr_t)pa,
						MM_FLAG_USER_RX);
		}
		uint8_t* ebts_dst = (uint8_t*)EBTS_LOAD_ADDR;
		for (uint64_t i = 0; i < ebts_size; i++)
			ebts_dst[i] = ebts_src[i];
		print_str("  ebts.bin: ", 0x0F);
		print_num((uint32_t)ebts_size, 0x0F);
		print_str(" bytes -> 0x500000\n", 0x0F);
	}

	/* 4. Map user stack (one page at 0x6FF000) */
	{
		uint64_t stack_va = 0x6FF000ULL;
		uintptr_t stack_pa = (uintptr_t)mm_alloc_page();
		if (!stack_pa)
			kernel_panic("OOM: stack page");
		if (mm_map_page(stack_va, (uint32_t)stack_pa, MM_FLAG_USER_RW) != 0) {
			kernel_panic("Failed to map user stack");
		}
		print_str("  Stack mapped at 0x", 0x0F);
		print_hex(stack_va, 0x0F);
		print_str(" -> 0x", 0x0F);
		print_hex(stack_pa, 0x0F);
		print_str("\n", 0x0F);
	}

	/* 5. Write boot_info_t to 0x600000 (fixed offsets) */
	uint64_t* boot = (uint64_t*)BOOT_INFO_ADDR;
	boot[0] = (ebts_size > 0) ? EBTS_LOAD_ADDR : 0; // ebts_src
	boot[1] = ebts_size;							// ebts_size
	boot[2] = driver_phys_addr;						// driver_src  (added)
	boot[3] = driver_size;							// driver_size (added)
	boot[4] = 0;									// flags
	boot[5] = 0;									// _pad

	print_str("  boot_info written: ebts_src=0x", 0x0F);
	print_hex(boot[0], 0x0F);
	print_str(" ebts_size=0x", 0x0F);
	print_hex(boot[1], 0x0F);
	print_str(" driver_src=0x", 0x0F);
	print_hex(boot[2], 0x0F);
	print_str(" driver_size=0x", 0x0F);
	print_hex(boot[3], 0x0F);
	print_str("\n", 0x0F);

	/* 6. Switch to user mode (init-service at 0x400000) */
	print_str("Entering user mode (this is the last kernel message)...\n",
			  0x0F);
	__asm__ volatile("cli");
	switch_to_user_mode(INIT_LOAD_ADDR, 0x80000ULL);

	return -1;
}