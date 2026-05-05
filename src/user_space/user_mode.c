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

    /* 3. Map user stack */
    {
        uintptr_t stack_pa = (uintptr_t)mm_alloc_page();
        if (!stack_pa) kernel_panic("OOM: stack page");
        
        uint64_t va = 0xF00000000ULL;  /* Match GDB stack_pointer */
        uint64_t pml4_idx = (va >> 39) & 0x1FF;
        uint64_t pdpt_idx = (va >> 30) & 0x1FF;
        uint64_t pd_idx = (va >> 21) & 0x1FF;
        uint64_t pt_idx = (va >> 12) & 0x1FF;
        
        // Allocate PDPT if needed
        if (!(pml4[pml4_idx] & PTE_PRESENT)) {
            uintptr_t pdpt_pa = (uintptr_t)mm_alloc_page();
            if (!pdpt_pa) kernel_panic("OOM: pdpt page");
            pml4[pml4_idx] = pdpt_pa | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        }
        uint64_t *user_pdpt = (uint64_t*)(pml4[pml4_idx] & ~0xFFFULL);
        
        // Allocate PD if needed
        if (!(user_pdpt[pdpt_idx] & PTE_PRESENT)) {
            uintptr_t pd_pa = (uintptr_t)mm_alloc_page();
            if (!pd_pa) kernel_panic("OOM: pd page");
            user_pdpt[pdpt_idx] = pd_pa | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        }
        uint64_t *user_pd = (uint64_t*)(user_pdpt[pdpt_idx] & ~0xFFFULL);
        
        // Allocate PT if needed
        if (!(user_pd[pd_idx] & PTE_PRESENT)) {
            uintptr_t pt_pa = (uintptr_t)mm_alloc_page();
            if (!pt_pa) kernel_panic("OOM: pt page");
            user_pd[pd_idx] = pt_pa | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        }
        uint64_t *user_pt = (uint64_t*)(user_pd[pd_idx] & ~0xFFFULL);
        
        // Set the page
        user_pt[pt_idx] = stack_pa | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        
        // Invalidate TLB
        __asm__ volatile("invlpg (%0)" : : "r"(va) : "memory");
        
        print_str("  Stack mapped at 0x", 0x0F);
        print_hex(va, 0x0F);
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

    /* 4. Switch to user mode */
    print_str("Entering user mode (this is the last kernel message)...\n", 0x0F);
    __asm__ volatile("cli");
    switch_to_user_mode(INIT_LOAD_ADDR, INIT_STACK_TOP);

    return -1;
}