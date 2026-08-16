/*
    E-com_os Kernel - Interrupt Descriptor Table (64-bit)
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

#include <kernel/arch/interrupts.h>
#include <stdint.h>

#define IDT_ENTRIES 256

typedef struct {
	uint16_t offset_low;
	uint16_t selector;
	uint8_t ist;
	uint8_t flags;
	uint16_t offset_mid;
	uint32_t offset_high;
	uint32_t reserved;
} __attribute__((packed)) idt_entry;

typedef struct {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed)) idt_ptr;

static idt_entry idt[IDT_ENTRIES];
static idt_ptr idt_pointer;

/* Public stub kept for ABI compatibility — real work done by setGate64 */
void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
	idt[num].offset_low = base & 0xFFFF;
	idt[num].selector = sel;
	idt[num].ist = 0;
	idt[num].flags = flags;
	idt[num].offset_mid = (base >> 16) & 0xFFFF;
	idt[num].offset_high = (base >> 32) & 0xFFFFFFFF;
	idt[num].reserved = 0;
}

/* Set gate with IST (Interrupt Stack Table) support */
static void idt_set_gate_ist(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags, uint8_t ist) {
	idt[num].offset_low = base & 0xFFFF;
	idt[num].selector = sel;
	idt[num].ist = ist & 0x07; /* IST 1-7, 0 = no IST */
	idt[num].flags = flags;
	idt[num].offset_mid = (base >> 16) & 0xFFFF;
	idt[num].offset_high = (base >> 32) & 0xFFFFFFFF;
	idt[num].reserved = 0;
}

#define GATE(n, fn) idt_set_gate((n), (uint64_t)(uintptr_t)(fn), 0x08, 0x8E)
#define GATE_IST(n, fn) idt_set_gate_ist((n), (uint64_t)(uintptr_t)(fn), 0x08, 0x8E, 1)
#define UGATE(n, fn) idt_set_gate((n), (uint64_t)(uintptr_t)(fn), 0x08, 0xEE)

void idt_init(void) {
	for (int i = 0; i < IDT_ENTRIES; i++) {
		idt[i].offset_low = 0;
		idt[i].selector = 0;
		idt[i].ist = 0;
		idt[i].flags = 0;
		idt[i].offset_mid = 0;
		idt[i].offset_high = 0;
		idt[i].reserved = 0;
	}

	GATE(0, isr0);
	GATE(1, isr1);
	GATE(2, isr2);
	GATE(3, isr3);
	GATE(4, isr4);
	GATE(5, isr5);
	GATE(6, isr6);
	GATE(7, isr7);
	GATE_IST(8, isr8);
	GATE(9, isr9);
	GATE(10, isr10);
	GATE(11, isr11);
	GATE_IST(12, isr12);
	GATE_IST(13, isr13);
	GATE_IST(14, isr14);
	GATE(15, isr15);
	GATE(16, isr16);
	GATE(17, isr17);
	GATE(18, isr18);
	GATE(19, isr19);
	GATE(20, isr20);
	GATE(21, isr21);
	GATE(22, isr22);
	GATE(23, isr23);
	GATE(24, isr24);
	GATE(25, isr25);
	GATE(26, isr26);
	GATE(27, isr27);
	GATE(28, isr28);
	GATE(29, isr29);
	GATE(30, isr30);
	GATE(31, isr31);

	/* All IRQs use IST1 for safe interrupt handling */
	GATE_IST(32, irq0);
	GATE_IST(33, irq1);
	GATE_IST(34, irq2);
	GATE_IST(35, irq3);
	GATE_IST(36, irq4);
	GATE_IST(37, irq5);
	GATE_IST(38, irq6);
	GATE_IST(39, irq7);
	GATE_IST(40, irq8);
	GATE_IST(41, irq9);
	GATE_IST(42, irq10);
	GATE_IST(43, irq11);
	GATE_IST(44, irq12);
	GATE_IST(45, irq13);
	GATE_IST(46, irq14);
	GATE_IST(47, irq15);

	UGATE(128, isr128);

	idt_pointer.limit = sizeof(idt) - 1;
	idt_pointer.base = (uint64_t)(uintptr_t)&idt;
	__asm__ volatile("lidt %0" : : "m"(idt_pointer));
}
