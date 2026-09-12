/*
	E-comOS Kernel - Global Descriptor Table + TSS (64-bit)
	Copyright (C) 2025,2026  Saladin5101

	GDT layout:
	  0x00  null
	  0x08  kernel code  (ring 0, 64-bit)
	  0x10  kernel data  (ring 0)
	  0x18  user   code  (ring 3, 64-bit)   selector 0x23 (|3)
	  0x20  user   data  (ring 3)            selector 0x1B (|3)
	  0x28  TSS low  (16 bytes, two GDT slots)
	  0x30  TSS high

	64-bit TSS (Intel SDM Vol.3 §7.7):
	  rsp0 at offset +4 (used on ring-3 → ring-0 transition)
*/

#include "../internal/gdt.h" /* Use the user's unmodified header */
#include <stdint.h>

#define GDT_USER_DATA_INDEX 3
#define GDT_USER_CODE_INDEX 4
#define USER_CS_SELECTOR ((GDT_USER_CODE_INDEX << 3) | 0x03) // 0x23
#define USER_DS_SELECTOR ((GDT_USER_DATA_INDEX << 3) | 0x03) // 0x1B

/* ------------------------------------------------------------------ */
/* GDTR (10 bytes for 64-bit mode)                                    */
/* ------------------------------------------------------------------ */
typedef struct {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed)) gdt_ptr64;

/* ------------------------------------------------------------------ */
/* 64-bit TSS descriptor (16 bytes = two GDT slots)                  */
/* ------------------------------------------------------------------ */
typedef struct {
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_middle;
	uint8_t access; /* 0x89 = present, ring-0, available 64-bit TSS */
	uint8_t granularity;
	uint8_t base_high;
	uint32_t base_upper;
	uint32_t reserved;
} __attribute__((packed)) tss_descriptor;

/* ------------------------------------------------------------------ */
/* 64-bit TSS body (Intel SDM Vol.3 §7.7, Table 7-11)               */
/* ------------------------------------------------------------------ */
typedef struct {
	uint32_t reserved0;
	uint64_t rsp0; /* kernel stack for ring-3 → ring-0            */
	uint64_t rsp1;
	uint64_t rsp2;
	uint64_t reserved1;
	uint64_t ist[7]; /* interrupt stack table (IST1..IST7)          */
	uint64_t reserved2;
	uint16_t reserved3;
	uint16_t iomap_base; /* offset to I/O permission bitmap             */
} __attribute__((packed)) Tss64;

/* ------------------------------------------------------------------ */
/* Static storage                                                     */
/* ------------------------------------------------------------------ */
/* 7 entries total:
 * - Indices 0-4: Standard code and data segments
 * - Indices 5-6: Map directly to the 16-byte TSS descriptor
 */
static struct gdt_entry gdt[7] __attribute__((aligned(8)));
static gdt_ptr64 gdtp;
static Tss64 tss;

static uint8_t kernel_stack[65536] __attribute__((aligned(16)));
static uint8_t interrupt_stack[32768] __attribute__((aligned(16)));

/* ------------------------------------------------------------------ */
/* Public Set Gate Function (conforms to gdt.h)                       */
/* ------------------------------------------------------------------ */
void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access,
				  uint8_t gran) {
	if (num < 0 || num >= 7)
		return;
	gdt[num].base_low = (uint16_t)(base & 0xFFFFu);
	gdt[num].base_middle = (uint8_t)((base >> 16) & 0xFFu);
	gdt[num].base_high = (uint8_t)((base >> 24) & 0xFFu);
	gdt[num].limit_low = (uint16_t)(limit & 0xFFFFu);
	gdt[num].granularity = (uint8_t)(((limit >> 16) & 0x0Fu) | (gran & 0xF0u));
	gdt[num].access = access;
}

/* ------------------------------------------------------------------ */
/* GDT Initialization                                                 */
/* ------------------------------------------------------------------ */
void gdt_init(void) {
	/* Null descriptor */
	gdt_set_gate(0, 0, 0, 0x00u, 0x00u);
	/* Kernel code: 64-bit, ring 0 (L=1 in granularity byte) */
	gdt_set_gate(1, 0, 0xFFFFFu, 0x9Au, 0xA0u); /* 0xA0 = G=1, L=1 (64-bit) */
	/* Kernel data: ring 0 */
	gdt_set_gate(2, 0, 0xFFFFFu, 0x92u, 0xC0u);
	/* User data: ring 3 (Index 3) */
	/* Access: 0xF2 (Data), Granularity: 0xC0 */
	gdt_set_gate(3, 0, 0xFFFFFFFFu, 0xF2u, 0xC0u);
	/* User code: ring 3 (Index 4) */
	/* Access: 0xFA (Code), Granularity: 0xA0 (Long Mode) */
	gdt_set_gate(4, 0, 0xFFFFFFFFu, 0xFAu, 0xA0u);

	/* TSS Setup (Maps directly onto gdt[5] and gdt[6]) */
	uint64_t tss_base = (uint64_t)(uintptr_t)&tss;
	uint32_t tss_limit = (uint32_t)(sizeof(Tss64) - 1u);

	tss_descriptor* td = (tss_descriptor*)&gdt[5];
	td->limit_low = (uint16_t)(tss_limit & 0xFFFFu);
	td->base_low = (uint16_t)(tss_base & 0xFFFFu);
	td->base_middle = (uint8_t)((tss_base >> 16) & 0xFFu);
	td->access = 0x89u; /* present, DPL=0, available 64-bit TSS */
	td->granularity = (uint8_t)((tss_limit >> 16) & 0x0Fu);
	td->base_high = (uint8_t)((tss_base >> 24) & 0xFFu);
	td->base_upper = (uint32_t)(tss_base >> 32);
	td->reserved = 0;

	/* Manually zero the TSS to avoid garbage-memory traps */
	for (uint32_t i = 0; i < sizeof(Tss64); i++) {
		((uint8_t*)&tss)[i] = 0;
	}

	tss.rsp0 = (uint64_t)(uintptr_t)(kernel_stack + sizeof(kernel_stack));
	tss.ist[0] =
		(uint64_t)(uintptr_t)(interrupt_stack + sizeof(interrupt_stack));
	tss.iomap_base = (uint16_t)sizeof(Tss64);

	/* Load the GDTR base pointing to our clean, static global array */
	gdtp.limit = (uint16_t)(sizeof(gdt) - 1u);
	gdtp.base = (uint64_t)(uintptr_t)gdt;

	__asm__ volatile(
		"lgdt %0\n"
		/* Far return to reload CS with kernel code selector 0x08 */
		"pushq $0x08\n"
		"leaq  1f(%%rip), %%rax\n"
		"pushq %%rax\n"
		"lretq\n"
		"1:\n"
		"movw $0x10, %%ax\n" /* kernel data selector */
		"movw %%ax, %%ds\n"
		"movw %%ax, %%es\n"
		"movw %%ax, %%ss\n"
		"xorw %%ax, %%ax\n" /* FS/GS = null in 64-bit mode */
		"movw %%ax, %%fs\n"
		"movw %%ax, %%gs\n"
		:
		: "m"(gdtp)
		: "rax", "memory");

	/* Load TSS selector (Index 5 = 5 * 8 = 40 = 0x28) */
	__asm__ volatile("ltr %%ax" : : "a"((uint16_t)0x28u));
}

/* Update kernel stack pointer in TSS (called on each context switch) */
void tss_set_kernel_stack(uint64_t rsp0) {
	tss.rsp0 = rsp0;
}
