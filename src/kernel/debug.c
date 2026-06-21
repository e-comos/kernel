/*
    E-comOS Kernel - Early Initialization and Debugging
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

#include <kernel/debug.h>
#include <kernel/early_init.h>
#include <kernel/printkit/print.h>
#include <klibc/string.h>
#include <stdint.h>

/* Serial port COM1 for early debug output */
#define COM1 0x3F8

/* Inline functions for port I/O */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*-----------------------------------------------------------------------------
 * Serial Port (COM1) Debugging Functions
 *-----------------------------------------------------------------------------
 */
void early_debug_init(void) {
    outb(COM1 + 1, 0x00); /* Disable all interrupts */
    outb(COM1 + 3, 0x80); /* Enable DLAB (set baud rate divisor) */
    outb(COM1 + 0, 0x03); /* Set divisor to 3 (lo byte) 38400 baud */
    outb(COM1 + 1, 0x00); /*                  (hi byte) */
    outb(COM1 + 3, 0x03); /* 8 bits, no parity, one stop bit */
    outb(COM1 + 2, 0xC7); /* Enable FIFO, clear them, with 14-byte threshold */
    outb(COM1 + 4, 0x0B); /* IRQs enabled, RTS/DSR set */
}

void early_debug_puts(const char *str) {
    for (; *str; str++) {
        /* Wait until the Transmit Holding Register is empty */
        while (!(inb(COM1 + 5) & 0x20))
            ;
        outb(COM1, (uint8_t)*str);
    }
}

void early_debug_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20))
        ;
    outb(COM1, (uint8_t)c);
}

/*-----------------------------------------------------------------------------
 * Kernel Panic and Logging
 *-----------------------------------------------------------------------------
 */
void kernel_panic(const char *msg) {
    /* Disable interrupts immediately */
    __asm__ volatile("cli");

    /* Output to screen (VGA) */
    print_str("\n*** KERNEL PANIC ***\n", 0x4F); /* White on Red */
    print_str("Message: ", 0x4F);
    print_str(msg, 0x4F);
    print_str("\n\n", 0x4F);
    
    /* Try to get current register state */
    uint64_t rbp, rsp, rip;
    __asm__ volatile("mov %%rbp, %0" : "=r"(rbp));
    __asm__ volatile("mov %%rsp, %0" : "=r"(rsp));
    __asm__ volatile("lea 0(%%rip), %0" : "=r"(rip));
    
    char buf[128];
    snprintf(buf, sizeof(buf), "RBP: 0x%016llX\nRSP: 0x%016llX\nRIP: 0x%016llX\n", 
             (unsigned long long)rbp, (unsigned long long)rsp, (unsigned long long)rip);
    print_str(buf, 0x4F);
    
    /* Output to serial port for redundancy */
    early_debug_puts("\n*** KERNEL PANIC ***\n");
    early_debug_puts("Message: ");
    early_debug_puts(msg);
    early_debug_puts("\n\n");
    snprintf(buf, sizeof(buf), "RBP: 0x%016llX\nRSP: 0x%016llX\nRIP: 0x%016llX\n", 
             (unsigned long long)rbp, (unsigned long long)rsp, (unsigned long long)rip);
    early_debug_puts(buf);

    /* Halt the system indefinitely */
    while (1) {
        __asm__ volatile("hlt");
    }
}

void kernel_log(const char *msg) {
    /* Log to screen (light gray on black) */
    print_str("[LOG] ", 0x07);
    print_str(msg, 0x07);

    /* Also log to serial */
    early_debug_puts("[LOG] ");
    early_debug_puts(msg);
}

/*-----------------------------------------------------------------------------
 * SSE/SSE2 Detection and Enable
 * 
 * The user-space program contains SSE instructions (movdqa, etc.).
 * The CPU must have SSE/SSE2 explicitly enabled in CR0 and CR4
 * before any such instruction is executed, otherwise an
 * Invalid Opcode (#UD) exception will be raised.
 *-----------------------------------------------------------------------------
 */
void enable_sse(void) {
    uint32_t eax, ebx, ecx, edx;
    const char *log_prefix = "[CPU/SSE] ";

    /* Check for SSE support using CPUID.01h */
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(1)
                     : );

    early_debug_puts(log_prefix);
    if (!(edx & (1 << 25))) {
        early_debug_puts("SSE not supported. Halting.\n");
        kernel_panic("CPU does not support SSE");
    }
    early_debug_puts("SSE supported. ");

    if (!(edx & (1 << 26))) {
        early_debug_puts("SSE2 not supported. Halting.\n");
        kernel_panic("CPU does not support SSE2");
    }
    early_debug_puts("SSE2 supported.\n");

    /* Enable SSE/SSE2 in CR0 */
    __asm__ volatile(
        "mov %%cr0, %%rax\n\t"
        "and $0xFFFB, %%ax\n\t"   /* Clear CR0.EM (bit 2) - Enable FPU/MMX/SSE */
        "or  $0x2,    %%ax\n\t"   /* Set   CR0.MP (bit 1) - Monitor Coprocessor */
        "mov %%rax, %%cr0"
        :
        :
        : "rax"
    );

    /* Enable SSE/SSE2 in CR4 (OS support for FXSAVE/FXRSTOR and exceptions) */
    __asm__ volatile(
        "mov %%cr4, %%rax\n\t"
        "or  $0x600, %%rax\n\t"   /* Set CR4.OSFXSR (bit 9) and CR4.OSXMMEXCPT (bit 10) */
        "mov %%rax, %%cr4"
        :
        :
        : "rax"
    );

    /* Initialize the MXCSR control/status register to a clean state.
     * 0x1F80 = all exception masks set, rounding mode = round to nearest.
     */
    uint32_t mxcsr_default = 0x1F80;
    __asm__ volatile("ldmxcsr %0" : : "m"(mxcsr_default));

    early_debug_puts(log_prefix);
    early_debug_puts("SSE/SSE2 successfully enabled and MXCSR initialized.\n");
}

/*-----------------------------------------------------------------------------
 * Early Kernel Initialization Entry Point
 * 
 * This function is called from the kernel boot strap code (written in assembly)
 * after the basic CPU mode (long mode) and a temporary stack are set up,
 * but before most kernel subsystems (memory manager, scheduler, etc.) are initialized.
 * 
 * Parameters:
 *   multiboot_magic - The magic number from the bootloader (e.g., GRUB)
 *   multiboot_info  - Physical address of the Multiboot information structure
 * 
 * Returns:
 *   0 on success, or a negative error code on failure (though failure typically
 *   leads to a panic/halt).
 *-----------------------------------------------------------------------------
 */
int early_kernel_init(uint32_t multiboot_magic, uint32_t multiboot_info) {
    /* Suppress unused parameter warnings for now */
    (void)multiboot_magic;
    (void)multiboot_info;

    /* Step 1: Initialize the serial port for debugging.
     * This is the first reliable output channel, usable even if VGA fails.
     */
    early_debug_init();
    early_debug_puts("[INIT] Serial debug port (COM1) initialized.\n");

    /* Step 2: Detect and enable SSE/SSE2 support.
     * This is REQUIRED before any user-space code (which may contain SSE
     * instructions) is executed. Must be done early in kernel life.
     */
    enable_sse();

    /* Step 3: (Placeholder) Future early initialization steps can be added here:
     *   - Parse Multiboot information to locate memory maps, kernel symbols, etc.
     *   - Initialize a basic VGA text mode console.
     *   - Set up a temporary memory map or allocator.
     */

    early_debug_puts("[INIT] Early kernel initialization completed successfully.\n");
    return 0;
}