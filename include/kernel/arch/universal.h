/*
 * E-com_os Microkernel - universal architecture definitions
 */

#ifndef KERNEL_ARCH_UNIVERSAL_H
#define KERNEL_ARCH_UNIVERSAL_H

#include <stdint.h>

// CPU registers structure
struct cpu_context {
	uint32_t eax, ebx, ecx, edx;
	uint32_t esi, edi, esp, ebp;
	uint32_t eip, eflags;
};

// Interrupt handling
#define IRQ_TIMER 0
#define IRQ_KEYBOARD 1
#define IRQ_SYSCALL 0x80

// Architecture-specific functions
void arch_enable_interrupts(void);
void arch_disable_interrupts(void);
void arch_halt(void);
void arch_context_switch(struct cpu_context *old, struct cpu_context *new);
// System call entry
void syscall_entry(void);

#endif