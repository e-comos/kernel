/*
    E-comOS Kernel - Main entry point
    Copyright (C) 2025,2026  Saladin5101

    Precondition:  called from _start with interrupts disabled.
    Precondition:  boot_params is either NULL or a valid UEFI memory map.
    Postcondition: never returns.
*/

#include <stdint.h>
#include <kernel/boot.h>
#include <kernel/arch/interrupts.h>
#include <kernel/mm.h>
#include <kernel/sched.h>
#include <kernel/ipc.h>
#include <kernel/syscall.h>
#include <kernel/printkit/print.h>
#include <kernel/debug.h>

extern void gdt_init(void);
#ifndef ECLIB_OK
#define ECLIB_OK 0
#endif
void kernel_main(void* boot_info) { 
    /* Interrupts are disabled on entry from _start */

    clear_screen(0x1F);
    print_str("E-comOS Microkernel v0.0.1\n", 0x1F);
    print_str("Initializing kernel...\n", 0x1F);

    /* Phase 1: GDT + TSS — must be first; fixes segment registers */
    print_str("GDT + TSS...\n", 0x1F);
    gdt_init();

    /* Phase 2: Memory — must be before any mmAllocPage call */
    print_str("Memory subsystem...\n", 0x1F);
    memory_status mm_status = mm_init(boot_info);
    if (mm_status != MEMORY_SUCCESS) {
        kernel_panic("mmInit failed — no usable memory");
    }
    mm_enable_paging();

    /* Phase 3: Interrupts */
    print_str("Interrupt handling...\n", 0x1F);
    idt_init();
    irq_remap();
    irq_init_timer();

    /* Phase 4: IPC + syscall IRQ subsystem */
    print_str("IPC + syscall...\n", 0x1F);
    syscall_irq_init();

    /* Phase 5: Create init service thread */
    print_str("Creating init service...\n", 0x1F);
    int result = load_init_service_to_user_mode();
    if (result < 0) {
		kernel_panic("Failed to load init-service for system init , kernel abort");
    }

    print_str("init-service is backed from user mode", 0x0F);
    
    print_str("Kernel ready.\n", 0x2F);

    /* Enable interrupts — from this point shared state must be protected */
    __asm__ volatile("sti");

    /* Kernel idle loop */
    while (1) {
        sched_schedule();

        /* Route any pending IPC messages.
         * Disable interrupts around bitmap/queue access to prevent
         * data races with IRQ handlers (F-09). */
        __asm__ volatile("cli");


        syscall_irq_check_timeouts();

        /* Memory pressure: reset allocator scan hint if >80% used */
        static uint32_t mm_counter = 0;
        if (++mm_counter % 100u == 0u) {
            uint32_t used = 0;
            for (uint32_t i = 0; i < MAX_PAGES; i++)
                if (page_bitmap[i >> 3] & (1u << (i & 7u)))
                    used++;
            if (used > (MAX_PAGES * 80u / 100u))
                next_free_page = 0;
        }

        __asm__ volatile("sti");

        /* Halt until next interrupt */
        __asm__ volatile("hlt");
    }
}
