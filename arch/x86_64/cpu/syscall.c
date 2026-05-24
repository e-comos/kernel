/*
    E-comOS Kernel - SYSCALL/SYSRET mechanism support
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

/* Model Specific Register (MSR) addresses for SYSCALL/SYSRET */
#define MSR_EFER    0xC0000080  /* Extended Feature Enable Register */
#define MSR_STAR    0xC0000081  /* SYSCALL Target Address */
#define MSR_LSTAR   0xC0000082  /* Long Mode SYSCALL Target */
#define MSR_SFMASK  0xC0000084  /* SYSCALL Flags Mask */

/* EFER bits */
#define EFER_SCE    (1 << 0)    /* System Call Extensions (SYSCALL/SYSRET) */

/* GDT selectors for SYSCALL/SYSRET (based on our GDT layout)
 * Index 1 (0x08): Kernel code segment
 * Index 2 (0x10): Kernel data segment  
 * Index 3 (0x18): User code segment (with RPL=3 becomes 0x1B)
 * Index 4 (0x20): User data segment (with RPL=3 becomes 0x23)
 */
#define KERNEL_CS   0x08        /* Kernel code segment (ring 0) */
#define USER_CS     0x18        /* User code segment (ring 3 when RPL=3) */
#define USER_DS     0x20        /* User data segment (ring 3 when RPL=3) */

/* Function prototype for the assembly syscall entry point */
extern void syscall_entry(void);

/**
 * write_msr - Write a 64-bit value to a Model Specific Register
 * @msr: MSR address to write to
 * @value: 64-bit value to write
 */
static void write_msr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)(value & 0xFFFFFFFFu);
    uint32_t high = (uint32_t)((value >> 32) & 0xFFFFFFFFu);
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high) : "memory");
}

/**
 * read_msr - Read a 64-bit value from a Model Specific Register
 * @msr: MSR address to read from
 * @return: 64-bit value read from MSR
 */
static uint64_t read_msr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

/**
 * enable_syscall_mechanism - Enable SYSCALL/SYSRET instruction support
 * 
 * Configures the necessary MSRs to allow user programs to use the SYSCALL
 * instruction for entering kernel mode and SYSRET for returning to user mode.
 * This is required because some user programs (like init.bin) expect this
 * standard x86-64 mechanism to be available.
 * 
 * The function sets up:
 * - EFER.SCE to enable SYSCALL/SYSRET extensions
 * - STAR register to define target segments for SYSCALL
 * - LSTAR register to point to our syscall entry handler
 * - SFMASK to control which RFLAGS bits are cleared during SYSCALL
 */
void enable_syscall_mechanism(void) {
    print_str("SYSCALL: Initializing SYSCALL/SYSRET mechanism...\n", 0x0A);
    
    /* Step 1: Enable SYSCALL/SYSRET extensions in EFER */
    uint64_t efer_value = read_msr(MSR_EFER);
    efer_value |= EFER_SCE;  /* Set System Call Extensions bit */
    write_msr(MSR_EFER, efer_value);
    
    print_str("  EFER.SCE enabled (EFER=0x", 0x0A);
    print_hex((uint32_t)efer_value, 0x0A);
    print_hex((uint32_t)(efer_value >> 32), 0x0A);
    print_str(")\n", 0x0A);
    
    /* Step 2: Configure STAR register
     * Bits 63:48: Selector for user code segment (with RPL forced to 3)
     * Bits 47:32: Selector for kernel code segment (with RPL forced to 0)
     * Our layout: USER_CS=0x18, KERNEL_CS=0x08
     * So STAR = (USER_CS << 16) | (KERNEL_CS << 32) = (0x18 << 16) | (0x08 << 32)
     */
    uint64_t star_value = ((uint64_t)USER_CS << 16) | ((uint64_t)KERNEL_CS << 32);
    write_msr(MSR_STAR, star_value);
    
    print_str("  STAR configured (0x", 0x0A);
    print_hex((uint32_t)star_value, 0x0A);
    print_hex((uint32_t)(star_value >> 32), 0x0A);
    print_str(")\n", 0x0A);
    
    /* Step 3: Configure LSTAR register to point to our syscall entry handler
     * LSTAR holds the linear address for the SYSCALL target in long mode
     */
    write_msr(MSR_LSTAR, (uint64_t)(uintptr_t)syscall_entry);
    
    print_str("  LSTAR set to syscall_entry (0x", 0x0A);
    print_hex((uint32_t)(uintptr_t)syscall_entry, 0x0A);
    print_hex((uint32_t)((uintptr_t)syscall_entry >> 32), 0x0A);
    print_str(")\n", 0x0A);
    
    /* Step 4: Configure SFMASK to clear IF flag (bit 9) and TF flag (bit 8) on SYSCALL
     * This ensures interrupts are disabled when entering kernel mode
     */
    write_msr(MSR_SFMASK, (1u << 9) | (1u << 8));  /* Clear IF and TF flags */
    
    print_str("  SFMASK set to disable IF+TF on SYSCALL (0x", 0x0A);
    print_hex(((1u << 9) | (1u << 8)), 0x0A);
    print_str(")\n", 0x0A);
    
    print_str("SYSCALL: SYSCALL/SYSRET mechanism initialized successfully\n", 0x0A);
}