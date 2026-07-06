# E-comOS - IRQ stubs (64-bit)

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

.macro IRQ num, vec
.global irq\num
irq\num:
    cli
    pushq $0
    pushq $\vec
    jmp irq_common_stub
.endm

IRQ 0,  32
IRQ 1,  33
IRQ 2,  34
IRQ 3,  35
IRQ 4,  36
IRQ 5,  37
IRQ 6,  38
IRQ 7,  39
IRQ 8,  40
IRQ 9,  41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

irq_common_stub:
    SAVE_REGS

    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    /* Pass int_no to handler (after 15*8 saved regs + 16 stub push - 8 call) */
    movq 128(%rsp), %rdi

    call irq_handler_asm_shim

    /* Restore segment selectors (already kernel, but keep consistent) */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    RESTORE_REGS
    /* Check CS.RPL to determine if interrupt came from user or kernel mode */
    /* After RESTORE_REGS, stack has: err_code(8) + int_no(8) + rip(8) + cs(8) + rflags(8) + (rsp)(8) + (ss)(8) */
    /* CS is at RSP+24 */
    movq 24(%rsp), %rax
    testw $3, %ax           /* Check RPL (bits 0-1) */
    jnz 2f                  /* If RPL=3 (user mode), add 56 */
    /* Kernel mode: add 40 (int_no + err_code + rip + cs + rflags) */
    addq $40, %rsp
    jmp 3f
2:
    /* User mode: add 56 (int_no + err_code + rip + cs + rflags + rsp + ss) */
    addq $56, %rsp
3:
    iretq

