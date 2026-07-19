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
    cli

    /* Align user RSP to 16-byte boundary (ABI requirement) */
    andq    $~0xF, %rsi

    /* Switch data segment registers to user selectors (0x1B, RPL=3) */
    movw    $0x1B, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs

    /*
     * Build iretq frame on the current (kernel) stack.
     * iretq pops in this order: RIP, CS, RFLAGS, RSP, SS.
     * Push in reverse order so the top of stack is RIP.
     *
     * RFLAGS value: 0x3202
     *   bit  1 (Reserved=1) : always 1
     *   bit  9 (IF=1)       : enable interrupts in user mode
     *   bit 12-13 (IOPL=3)  : I/O privilege level 3 (user can do port I/O)
     */

    /* 5. SS */
    pushq   $0x1B

    /* 4. RSP */
    pushq   %rsi

    /* 3. RFLAGS: IF=1, IOPL=3, Reserved=1 */
    pushq   $0x3202

    /* 2. CS */
    pushq   $0x23

    /* 1. RIP */
    pushq   %rdi

    /* Atomically restore RIP/CS/RFLAGS/RSP/SS and switch to ring 3 */
    iretq

    /* Never reached – just in case, halt forever */
.halt_loop:
    cli
    hlt
    jmp .halt_loop

.size asm_switch_to_user_mode, . - asm_switch_to_user_mode
