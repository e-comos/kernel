/**
 * E-comOS Kernel - A Microkernel of E-comOS Operating System.
 * Copyright (C) 2026 Saladin5101 
 * This file is a part of E-comOS Kernel
 * This program is a free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses>
**/

.section .text

.macro SAVE_REGS
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
.endm

.macro RESTORE_REGS
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
.endm

# Macro for IRQs/Exceptions that DO NOT push an error code by default
.macro IRQ_NO_ERR num, vec
.global irq\num
irq\num:
    cli
    pushq $0                # Dummy error code
    pushq $\vec             # Vector number
    jmp irq_common_stub
.endm

# Generate IRQ stubs 0 through 15 (Vectors 32 to 47)
IRQ_NO_ERR 0,  32
IRQ_NO_ERR 1,  33
IRQ_NO_ERR 2,  34
IRQ_NO_ERR 3,  35
IRQ_NO_ERR 4,  36
IRQ_NO_ERR 5,  37
IRQ_NO_ERR 6,  38
IRQ_NO_ERR 7,  39
IRQ_NO_ERR 8,  40
IRQ_NO_ERR 9,  41
IRQ_NO_ERR 10, 42
IRQ_NO_ERR 11, 43
IRQ_NO_ERR 12, 44
IRQ_NO_ERR 13, 45
IRQ_NO_ERR 14, 46
IRQ_NO_ERR 15, 47

.global irq_common_stub
.extern irq_handler_asm_shim

irq_common_stub:
    SAVE_REGS

    # FIXED: CS is located at 0x90(%rsp) after SAVE_REGS, not 0x98.
    # 0x98(%rsp) is RFLAGS, which has bit 1 hardwired to 1.
    # Checking 0x98 was causing the CPU to mistake kernel interrupts for user interrupts.
    movq 0x90(%rsp), %rax
    testq $3, %rax
    jz do_kernel_irq
    jnz do_user_irq

do_kernel_irq:
    movq %rsp, %r12         # Save kernel stack pointer

    # Set up kernel data segment registers
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    # Pass pointer to registers_t struct (%rsp) as first argument (%rdi)
    movq %rsp, %rdi
    call irq_handler_asm_shim

    movq %r12, %rsp         # Restore stack pointer
    
    # Reload kernel data segments
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    RESTORE_REGS
    addq $16, %rsp          # Clean up vector number and error code
    iretq

do_user_irq:
    movq %rsp, %r12         # Save user transition stack pointer

    # Set up kernel data segment registers
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    # Pass pointer to registers_t struct (%rsp) as first argument (%rdi)
    movq %rsp, %rdi
    call irq_handler_asm_shim

    movq %r12, %rsp         # Restore stack pointer

    # PREVENTATIVE FIX: 
    # Before dropping back to Ring 3, you must restore the segment registers 
    # to your User Data selector (typically 0x23). If you leave them at 0x10, 
    # the Ring 3 user program will trigger a GP fault the moment it tries to read/write memory.
    #
    # Uncomment the lines below and change 0x23 to your GDT's User Data Selector:
   	movw $0x23, %ax
  	movw %ax, %ds
  	movw %ax, %es
   	movw %ax, %fs
  	movw %ax, %gs

    RESTORE_REGS
    addq $16, %rsp          # Clean up vector number and error code
    iretq
