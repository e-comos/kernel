/*
 * E-com_os x86_64 - Context switching
 */

#ifndef ARCH_X86_64_INTERNAL_CONTEXT_H
#define ARCH_X86_64_INTERNAL_CONTEXT_H

#include <stdint.h>

// CPU context for context switching
struct cpu_context {
	uint32_t eax, ebx, ecx, edx;
	uint32_t esi, edi, esp, ebp;
	uint32_t eip, eflags;
	uint32_t cr3; // Page directory
} __attribute__((packed));

// Context switching functions
void context_switch(struct cpu_context *old_ctx, struct cpu_context *new_ctx);
void context_save(struct cpu_context *ctx);
void context_restore(struct cpu_context *ctx);

#endif