/*
 * switch_to_user_mode.S - Switch from kernel mode to user mode
 * 
 * This file implements the user mode switch using GAS assembler syntax.
 * It replaces the inline assembly in user_mode.c for better maintainability
 * and debugging.
 */

.text
.global asm_switch_to_user_mode
.type asm_switch_to_user_mode, @function

/*
 * void asm_switch_to_user_mode(uintptr_t entry_point, uintptr_t stack_pointer) __attribute__((noreturn));
 * 
 * Switches to user mode using iretq instruction.
 * 
 * Parameters:
 *   RDI: entry_point (user RIP)
 *   RSI: stack_pointer (user RSP)
 * 
 * This function never returns.
 */
asm_switch_to_user_mode:
    /* Save kernel stack pointer for error recovery */
    push %rbx
    mov %rsp, %rbx

    /* Align user stack to 16 bytes */
    and $~0xF, %rsi
    mov %rsi, %rsp

    /* Load user data segment selectors into segment registers */
    /* User data segment selector: 0x23 (index 4, DPL=3) */
    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    /* SS will be set by iretq from stack */

    /* Build the iretq stack frame */
    /* Push user SS (stack segment) */
    pushq $0x23          /* User data segment selector: 0x23 (index 4, DPL=3) */
    /* Push user RSP */
    pushq %rsi
    /* Push user RFLAGS */
    pushfq
    /* Enable interrupts in user mode (set IF flag) */
    orq $0x200, (%rsp)
    /* Push user CS (code segment) */
    /* User code segment selector: 0x1B (index 3, DPL=3) */
    pushq $0x1B
    /* Push user RIP */
    pushq %rdi

    /* Debug label: before iretq */
asm_switch_to_user_mode_iretq:
    /* Switch to user mode */
    iretq

    /* If we reach here, iretq failed - this should never happen */
    /* Restore kernel stack */
    mov %rbx, %rsp
    pop %rbx

    /* Infinite loop with halt */
asm_switch_to_user_mode_error:
    hlt
    jmp asm_switch_to_user_mode_error

.size asm_switch_to_user_mode, . - asm_switch_to_user_mode