/*
 * switch_to_user_mode.S - Switch from kernel mode to user mode (64-bit)
 * 
 * CRITICAL: GDT layout (see gdt.c):
 *   Index 0 (0x00): Null descriptor
 *   Index 1 (0x08): Kernel code (ring 0, 64-bit)  <- selector = 0x08
 *   Index 2 (0x10): Kernel data (ring 0)          <- selector = 0x10
 *   Index 3 (0x18): User code (ring 3, 64-bit)    <- selector = 0x18 | 3 = 0x1B
 *   Index 4 (0x20): User data (ring 3)            <- selector = 0x20 | 3 = 0x23
 *   Index 5 (0x28): TSS descriptor
 *
 * IMPORTANT: In 64-bit mode, iretq stack frame order is:
 *   SS, RSP, RFLAGS, CS, RIP  (pushed in reverse order: RIP, CS, RFLAGS, RSP, SS)
 *   
 * Why Invalid Opcode (#UD) happens:
 *   1. Wrong CS selector (e.g., using 0x1B when GDT entry is not a 64-bit code segment)
 *   2. Trying to execute code at an unmapped address
 *   3. CPU trying to fetch the first instruction after iretq fails validation
 *
 * How this code avoids #UD:
 *   1. Uses correct selectors: CS=0x1B, SS=0x23 (matching GDT entries 3 and 4 with RPL=3)
 *   2. Does NOT switch to user stack before building iretq frame (avoids page fault)
 *   3. Builds the entire iretq frame on kernel stack, then iretq atomically switches
 */

.text
.global asm_switch_to_user_mode
.type asm_switch_to_user_mode, @function

/*
 * void asm_switch_to_user_mode(uintptr_t entry_point, uintptr_t stack_pointer)
 *
 * Parameters:
 *   RDI: entry_point (user RIP, typically 0x400000 for init.bin)
 *   RSI: stack_pointer (user RSP, must be valid and mapped)
 *
 * This function never returns.
 */
/* ============================================================================
 * asm_switch_to_user_mode - Switch from kernel mode to user mode
 *
 * Parameters:
 *   RDI: User entry point (RIP), typically 0x400000 for init.bin
 *   RSI: User stack pointer (RSP), must be valid and mapped
 *
 * This function never returns. It builds an iretq frame and atomically
 * switches to user mode (ring 3).
 *
 * IMPORTANT DEBUG ADDITIONS: Serial output for troubleshooting
 * ============================================================================
 */
asm_switch_to_user_mode:
    /* Debug: Print "S" to indicate start of switch */
    movw $0x3F8, %dx
    movb $'S', %al
    outb %al, %dx
    
    /* Step 1: Disable interrupts during the switch */
    cli

    /* Step 2: Save kernel RSP in RBX (callee-saved) for potential recovery */
    pushq %rbx
    movq %rsp, %rbx

    /* Step 3: Align user RSP to 16-byte boundary (System V ABI requirement) */
    andq $~0xF, %rsi

    /* Debug: Print parameters */
    movw $0x3F8, %dx
    movb $'P', %al        /* 'P' for Parameters */
    outb %al, %dx
    movb %dil, %al        /* Print low byte of RDI (entry point) */
    outb %al, %dx
    movb %sil, %al        /* Print low byte of RSI (stack pointer) */
    outb %al, %dx

    /* Step 4: Load user data segment selector (0x23) into data segment registers */
    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    /* Note: SS will be loaded by iretq, so we don't set it here */

    /* Debug: Print segment values */
    movw $0x3F8, %dx
    movb $'C', %al        /* 'C' for CS value */
    outb %al, %dx
    movb $0x1B, %al       /* Expected CS value */
    outb %al, %dx
    movb $'S', %al        /* 'S' for SS value */
    outb %al, %dx
    movb $0x23, %al       /* Expected SS value */
    outb %al, %dx

    /* Step 5: Build iretq stack frame on KERNEL stack */
    /* iretq pops in this order: RIP, CS, RFLAGS, RSP, SS */
    /* So we push in reverse order: SS, RSP, RFLAGS, CS, RIP */

    /* Push SS (user data segment selector with RPL=3) */
    pushq $0x23

    /* Push RSP (user stack pointer, already aligned) */
    pushq %rsi

    /* Push RFLAGS with interrupts enabled (IF=1) */
    pushfq
    orq $0x200, (%rsp)      /* Set IF bit to enable interrupts in user mode */

    /* Push CS (user code segment selector with RPL=3) */
    pushq $0x1B

    /* Push RIP (user entry point) */
    pushq %rdi

    /* Debug: Verify stack frame before iretq */
    movw $0x3F8, %dx
    movb $'F', %al        /* 'F' for Frame */
    outb %al, %dx
    
    /* Check what's actually on the stack */
    movq 0(%rsp), %rax    /* RIP */
    movb %al, %al
    outb %al, %dx
    movq 8(%rsp), %rax    /* CS */
    movb %al, %al
    outb %al, %dx
    movq 16(%rsp), %rax   /* RFLAGS */
    movb %al, %al
    outb %al, %dx
    movq 24(%rsp), %rax   /* RSP */
    movb %al, %al
    outb %al, %dx
    movq 32(%rsp), %rax   /* SS */
    movb %al, %al
    outb %al, %dx

    /* Debug: Print "G" for Go (about to execute iretq) */
    movw $0x3F8, %dx
    movb $'G', %al
    outb %al, %dx

    /* Step 6: Jump to user mode! */
    iretq

    /* Step 7: Should never reach here */
    cli
.halt_loop:
    hlt
    jmp .halt_loop

.size asm_switch_to_user_mode, . - asm_switch_to_user_mode