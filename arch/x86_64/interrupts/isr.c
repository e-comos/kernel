/*
    E-com_os Kernel - ISR C handler
    Copyright (C) 2025,2026  Saladin5101

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdint.h>
#include <kernel/printkit/print.h>

/* Exception messages for debugging */
static const char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating Point Exception",
    "Virtualization Exception",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved"
};

/* Page fault handler - implemented in mm.c */
extern int handle_page_fault(uint64_t fault_addr, uint64_t error_code);

/*
 * dump_exception_info - Print exception details for debugging
 *
 * Parameters:
 *   int_no  - Exception vector number (0-31)
 *   err_code - Error code pushed by CPU (or 0 if no error code)
 *   rip     - Instruction pointer that caused the exception
 *   cs      - Code segment selector
 */
static void dump_exception_info(uint64_t int_no, uint64_t err_code, 
                                 uint64_t rip, uint64_t cs) {
    print_str("\n=== EXCEPTION ===\n", 0x4F);
    
    /* Exception name */
    if (int_no < 32) {
        print_str("  ", 0x4F);
        print_str(exception_messages[int_no], 0x4F);
        print_str(" (#", 0x4F);
        print_num((uint32_t)int_no, 0x4F);
        print_str(")\n", 0x4F);
    }
    
    /* Error code */
    print_str("  Error Code: 0x", 0x4F);
    print_hex((uint32_t)err_code, 0x4F);
    print_str("\n", 0x4F);
    
    /* Faulting instruction */
    print_str("  RIP: 0x", 0x4F);
    print_hex(rip, 0x4F);
    print_str("  CS: 0x", 0x4F);
    print_hex((uint32_t)cs, 0x4F);
    print_str("\n", 0x4F);
    
    /* Decode error code for specific exceptions */
    if (int_no == 14) {
        /* Page Fault error code bits */
        print_str("  Page Fault: ", 0x4F);
        if (err_code & 0x01) print_str("P ", 0x0F);  /* Present */
        else                 print_str("NP ", 0x0F); /* Not Present */
        if (err_code & 0x02) print_str("Write ", 0x0F);
        else                 print_str("Read ", 0x0F);
        if (err_code & 0x04) print_str("User ", 0x0F);
        else                 print_str("Kernel ", 0x0F);
        if (err_code & 0x10) print_str("Execute ", 0x0F);
        print_str("\n", 0x4F);
        
        /* Read CR2 for fault address */
        uint64_t fault_addr;
        __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));
        print_str("  Fault Address (CR2): 0x", 0x4F);
        print_hex(fault_addr, 0x4F);
        print_str("\n", 0x4F);
    }
    
    if (int_no == 13) {
        /* General Protection Fault error code */
        if (err_code != 0) {
            print_str("  Segment Selector Index: ", 0x4F);
            print_num((uint32_t)(err_code & 0xFFF8), 0x4F);
            if (err_code & 0x01) print_str(" EXT", 0x0F);
            if (err_code & 0x02) print_str(" IDT", 0x0F);
            if (err_code & 0x04) print_str(" TI", 0x0F);
            print_str("\n", 0x4F);
        } else {
            print_str("  Not segment-related\n", 0x4F);
        }
    }
    
    print_str("==================\n", 0x4F);
}

/*
 * isr_handler - Central interrupt service routine handler
 *
 * CRITICAL: This runs on the interrupt stack with interrupts disabled.
 * DO NOT call functions that may allocate memory or use the heap,
 * as the interrupt context may have limited stack space.
 *
 * Why Invalid Opcode (#UD = int_no 6) happens:
 *   1. Wrong CS selector in iretq frame (must match a 64-bit code segment in GDT)
 *   2. User code at RIP is not valid x86-64 instructions
 *   3. RIP points to unmapped memory (CPU fetches garbage)
 *   4. RIP is not canonical (bits 48-63 not sign-extended)
 *
 * Why General Protection Fault (#GP = int_no 13) happens:
 *   1. Segment limit violation (unlikely in flat 64-bit model)
 *   2. Privilege violation (user accessing kernel memory)
 *   3. Writing to read-only memory
 *   4. Loading invalid selector into segment register
 *
 * Why Page Fault (#PF = int_no 14) happens:
 *   1. Accessing unmapped virtual address
 *   2. Privilege violation (user accessing kernel page without U/S bit)
 *   3. Writing to read-only page
 *   4. Executing from non-executable page (NX bit)
 */
void isr_handler(uint64_t int_no, uint64_t err_code, uint64_t rip, uint64_t cs) {
    /* Handle CPU exceptions (vector 0-31) */
    if (int_no < 32) {
        
        /* Page Fault (#PF) - Try dynamic page allocation first */
        if (int_no == 14) {
            uint64_t fault_addr;
            __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));
            
            /* Try to handle the page fault dynamically */
            if (handle_page_fault(fault_addr, err_code)) {
                /* Page fault handled - return to re-execute instruction */
                return;
            }
            
            /* Unhandled page fault - dump info and halt */
            /* Note: We don't have RIP/CS here, will dump what we can */
            dump_exception_info(int_no, err_code, fault_addr, 0);
            goto halt;
        }
        
        /* Double Fault (#DF) - Cannot recover, minimal output */
        if (int_no == 8) {
            print_str("\n!!! DOUBLE FAULT - SYSTEM HALTED !!!\n", 0x4F);
            goto halt;
        }
        
        /* All other exceptions - dump info and halt */
        /* For now, we pass 0 for RIP/CS since we don't extract them from stack */
        dump_exception_info(int_no, err_code, rip, cs);
        goto halt;
    }
    
    /* Hardware interrupts (IRQ) would be handled here */
    return;

halt:
    __asm__ volatile("cli");
    while (1) {
        __asm__ volatile("hlt");
    }
}
