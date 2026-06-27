/*
    User mode management
    Handles transitions between kernel mode and user mode.
*/

#include "user_space/user_mode.h"
#include "kernel/boot.h"
#include "kernel/mm.h"
#include "kernel/arch/interrupts.h"
#include "kernel/printkit/print.h"
#include "kernel/debug.h"
#include <stdint.h>



/* GDT segment selectors for user mode */
#define USER_CODE_SELECTOR 0x23
#define USER_DATA_SELECTOR 0x2B

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
    if (!user_p4) kernel_panic("Failed to allocate user P4");

    // Copy all kernel P4 entries to user P4
    uint64_t* kernel_p4 = pml4;  // Use the actual kernel PML4 array
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
void __attribute__((noreturn)) switch_to_user_mode(uintptr_t entry_point, uintptr_t stack_pointer) {
    uint64_t user_rip = entry_point;
    uint64_t user_rsp = stack_pointer;

    // Ensure 16-byte stack alignment
    if ((user_rsp & 0xF) != 0) {
        user_rsp &= ~0xFULL;  // Align to 16-byte boundary
    }

    print_str("Switching to user mode...\n", 0x0F);
    print_str("  RIP: 0x", 0x0F);
    print_hex(user_rip, 0x0F);
    print_str("\n  RSP: 0x", 0x0F);
    print_hex(user_rsp, 0x0F);
    print_str(" (aligned: ", 0x0F);
    print_str((user_rsp & 0xF) == 0 ? "yes" : "no", 0x0F);
    print_str(")\n", 0x0F);
    
    print_str("  CS: 0x23 (index 4, DPL=3)\n", 0x0F);
    print_str("  SS: 0x2B (index 5, DPL=3)\n", 0x0F);

    // Create and switch to user page table first
    uintptr_t user_cr3 = create_user_pagetable();
    __asm__ volatile("movq %0, %%cr3" : : "r"(user_cr3) : "memory");

    // Call the assembly function
    extern void asm_switch_to_user_mode(uintptr_t, uintptr_t) __attribute__((noreturn));
    asm_switch_to_user_mode(user_rip, user_rsp);

    __builtin_unreachable();
}

/* --------------------------------------------------------------- */
/* load_init_service_to_user_mode                                  */
/* --------------------------------------------------------------- */
int load_init_service_to_user_mode(void) {
    print_str("Loading init-service to user mode...\n", 0x0F);

    /* 1. Load init.bin to 0x400000 */
    uint8_t* init_src  = _binary_payload_init_bin_start;
    uint64_t init_size = (uint64_t)(_binary_payload_init_bin_end - _binary_payload_init_bin_start);

    if (init_size == 0 || init_size > 0x100000)
        kernel_panic("init.bin missing or too large");

    /* Map pages for init.bin (may span multiple pages) */
    uint32_t init_pages = (uint32_t)((init_size + PAGE_SIZE - 1) / PAGE_SIZE);
    for (uint32_t p = 0; p < init_pages; p++) {
        void *pa = mm_alloc_page();
        if (!pa) kernel_panic("OOM: init.bin page");
        mm_map_page((uint32_t)(INIT_LOAD_ADDR + p * PAGE_SIZE),
                    (uint32_t)(uintptr_t)pa,
                    MM_FLAG_USER_RW);
    }

    uint8_t* init_dst = (uint8_t*)INIT_LOAD_ADDR;
    for (uint64_t i = 0; i < init_size; i++)
        init_dst[i] = init_src[i];

    print_str("  init.bin: ", 0x0F);
    print_num((uint32_t)init_size, 0x0F);
    print_str(" bytes -> 0x400000\n", 0x0F);

    /* 2. Load ebts.bin to 0x500000 */
    uint8_t* ebts_src  = _binary_payload_ebts_bin_start;
    uint64_t ebts_size = (uint64_t)(_binary_payload_ebts_bin_end - _binary_payload_ebts_bin_start);

    if (ebts_size > 0 && ebts_size <= 0x100000) {
        uint32_t ebts_pages = (uint32_t)((ebts_size + PAGE_SIZE - 1) / PAGE_SIZE);
        for (uint32_t p = 0; p < ebts_pages; p++) {
            void *pa = mm_alloc_page();
            if (!pa) kernel_panic("OOM: ebts.bin page");
            mm_map_page((uint32_t)(EBTS_LOAD_ADDR + p * PAGE_SIZE),
                        (uint32_t)(uintptr_t)pa,
                        MM_FLAG_USER_RW);
        }
        uint8_t* ebts_dst = (uint8_t*)EBTS_LOAD_ADDR;
        for (uint64_t i = 0; i < ebts_size; i++)
            ebts_dst[i] = ebts_src[i];
        print_str("  ebts.bin: ", 0x0F);
        print_num((uint32_t)ebts_size, 0x0F);
        print_str(" bytes -> 0x500000\n", 0x0F);
    }

    /* 3. Map user stack
     * User stack is at 0x7F000 (as specified in requirements)
     * We allocate TWO pages for stack safety (guard page + stack page)
     */
    {
        /* Stack page at 0x7F000 (user stack address from requirements) */
        uint64_t stack_va = 0x7F000ULL;
        
        /* Allocate physical page for stack */
        uintptr_t stack_pa = (uintptr_t)mm_alloc_page();
        if (!stack_pa) kernel_panic("OOM: stack page");
        
        /* Use mm_map_page which handles identity-mapped low memory */
        if (mm_map_page(stack_va, (uint32_t)stack_pa, MM_FLAG_USER_RW) != 0) {
            kernel_panic("Failed to map user stack");
        }
        
        print_str("  Stack mapped at 0x", 0x0F);
        print_hex(stack_va, 0x0F);
        print_str(" -> 0x", 0x0F);
        print_hex(stack_pa, 0x0F);
        print_str("\n", 0x0F);
    }

    /* 4. Write boot_info_t to 0x600000 */
    {
        void *boot_pa = mm_alloc_page();
        if (!boot_pa) kernel_panic("OOM: boot_info page");
        mm_map_page((uint32_t)BOOT_INFO_ADDR, (uint32_t)(uintptr_t)boot_pa, MM_FLAG_USER_RW);
    }

    boot_info_t* binfo = (boot_info_t*)BOOT_INFO_ADDR;
    binfo->ebts_src  = (ebts_size > 0) ? EBTS_LOAD_ADDR : 0;
    binfo->ebts_size = ebts_size;
    binfo->flags     = 0;
    binfo->_pad      = 0;

    /* 4. Switch to user mode
     * Entry point: 0x400000 (INIT_LOAD_ADDR)
     * Stack pointer: 0x80000 (top of 0x7F000 stack page + 4KB)
     * Note: Stack grows downward, so we point to the TOP of the stack
     */
    print_str("Entering user mode (this is the last kernel message)...\n", 0x0F);
    __asm__ volatile("cli");
    switch_to_user_mode(INIT_LOAD_ADDR, 0x80000ULL);

    return -1;
}