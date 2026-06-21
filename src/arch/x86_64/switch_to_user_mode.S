/*
 * asm_switch_to_user_mode.S - Transition from kernel mode to user mode
 *
 * This function constructs an interrupt return frame on the kernel stack
 * and executes the `iretq` instruction to atomically switch to user mode
 * (ring 3). It never returns.
 *
 * Calling Convention (System V AMD64):
 *   RDI = user RIP (entry point, e.g., 0x400000)
 *   RSI = user RSP (stack pointer, must be user-mapped and 16-byte aligned)
 *
 * Stack Layout before iretq (built in reverse order of popping):
 *   RSP+0  : RIP  (user entry point)
 *   RSP+8  : CS   (user code segment selector, 0x1B)
 *   RSP+16 : RFLAGS (IF=0, TF=0, RF=0 — interrupts disabled in user mode)
 *   RSP+24 : RSP  (user stack pointer)
 *   RSP+32 : SS   (user stack segment selector, 0x23)
 */

.section .text
.global asm_switch_to_user_mode
.type asm_switch_to_user_mode, @function

asm_switch_to_user_mode:
    /* Disable interrupts during the critical switch */
    cli

    /* Save kernel stack pointer for potential debugging (not used further) */
    pushq %rbx
    movq %rsp, %rbx

    /* Align user stack pointer to 16-byte boundary (ABI requirement) */
    andq $~0xF, %rsi

    /* Load user data segment selectors into data registers */
    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    /* Build the iretq frame (push in reverse order of popping) */

    /* 1. SS: user stack segment selector (RPL=3) */
    pushq $0x23

    /* 2. RSP: aligned user stack pointer */
    pushq %rsi

    /* 3. RFLAGS: current flags with IF=0, TF=0, RF=0 */
    pushfq
    andq  $~(0x200 | 0x100 | 0x10000), (%rsp) /* Clear IF (bit 9), TF (bit 8), RF (bit 16) */

    /* 4. CS: user code segment selector (RPL=3) */
    pushq $0x1B

    /* 5. RIP: user entry point */
    pushq %rdi

    /* Atomic switch to user mode */
    iretq

    /* Never reached – just in case, halt forever */
.halt_loop:
    cli
    hlt
    jmp .halt_loop

.size asm_switch_to_user_mode, . - asm_switch_to_user_mode
