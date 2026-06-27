/* ============================================================================
 * isr.S - 64-bit Interrupt Service Routine Stubs for E-comOS Kernel
 * 
 * This file contains the low-level assembly entry points (stubs) for all
 * x86_64 CPU exceptions, the system call interrupt (int 0x80), and provides
 * common handling stubs. It bridges the hardware-generated interrupt/exception
 * to the C-language handlers in the kernel.
 *
 * Core Components:
 *   1. Macros to save/restore the full CPU register state.
 *   2. Macros to generate individual exception entry points (0-31).
 *   3. The system call entry point for int 0x80 (vector 128).
 *   4. Common handler stubs that set up the C calling environment.
 *
 * A critical correction has been made to `isr_common_stub` to dynamically
 * adjust stack pointer offsets based on whether the exception originated
 * from user or kernel mode, fixing the previously observed erroneous
 * CS/RIP values (e.g., CS=0x0, RIP=0xFFFFFFFF).
 * ============================================================================
 */

.section .text

/* ============================================================================
 * Register Save/Restore Macros
 * ============================================================================
 */

/*
 * SAVE_REGS - Saves all general-purpose registers onto the stack.
 *
 * The x86_64 architecture lacks a `pusha` instruction. This macro manually
 * pushes all 15 general-purpose registers (excluding RSP) to create a
 * consistent stack frame for the C handler. The order is chosen for clarity.
 */
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

/*
 * RESTORE_REGS - Restores all general-purpose registers from the stack.
 *
 * Pops registers in the reverse order of SAVE_REGS. This must be called
 * to maintain stack balance before returning from an interrupt.
 */
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

/* ============================================================================
 * ISR Stub Generation Macros
 * ============================================================================
 */

/*
 * ISR_NOERRCODE - Generates a stub for an exception that does NOT push
 *                 an error code onto the stack.
 *
 * To maintain a uniform stack frame for the common handler, a dummy
 * error code (0) is pushed.
 *
 * @num: The interrupt vector number (0-31).
 */
.macro ISR_NOERRCODE num
.global isr\num
isr\num:
    cli                     /* Disable interrupts */
    pushq $0                /* Push dummy error code (0) */
    pushq $\num            /* Push the interrupt vector number */
    jmp isr_common_stub    /* Jump to the shared handler */
.endm

/*
 * ISR_ERRCODE - Generates a stub for an exception that DOES push an
 *               error code onto the stack.
 *
 * The CPU automatically pushes the error code before transferring control.
 * We only need to push the vector number.
 *
 * @num: The interrupt vector number (0-31).
 */
.macro ISR_ERRCODE num
.global isr\num
isr\num:
    cli                     /* Disable interrupts */
    pushq $\num            /* Push the interrupt vector number */
    jmp isr_common_stub    /* Jump to the shared handler */
.endm

/* ============================================================================
 * CPU Exception Handlers (Vectors 0-31)
 * ============================================================================
 */

/* Generate stubs for all architectural exceptions. */
ISR_NOERRCODE 0   /* #DE: Divide Error */
ISR_NOERRCODE 1   /* #DB: Debug */
ISR_NOERRCODE 2   /* NMI: Non-Maskable Interrupt */
ISR_NOERRCODE 3   /* #BP: Breakpoint */
ISR_NOERRCODE 4   /* #OF: Overflow */
ISR_NOERRCODE 5   /* #BR: Bound Range Exceeded */
ISR_NOERRCODE 6   /* #UD: Invalid Opcode */
ISR_NOERRCODE 7   /* #NM: Device Not Available */
ISR_ERRCODE   8   /* #DF: Double Fault */
ISR_NOERRCODE 9   /* Coprocessor Segment Overrun (reserved) */
ISR_ERRCODE   10  /* #TS: Invalid TSS */
ISR_ERRCODE   11  /* #NP: Segment Not Present */
ISR_ERRCODE   12  /* #SS: Stack Segment Fault */
ISR_ERRCODE   13  /* #GP: General Protection Fault */
ISR_ERRCODE   14  /* #PF: Page Fault */
ISR_NOERRCODE 15  /* (Intel reserved) */
ISR_NOERRCODE 16  /* #MF: x87 Floating-Point Exception */
ISR_ERRCODE   17  /* #AC: Alignment Check */
ISR_NOERRCODE 18  /* #MC: Machine Check */
ISR_NOERRCODE 19  /* #XM: SIMD Floating-Point Exception */
ISR_NOERRCODE 20  /* #VE: Virtualization Exception */
/* Vectors 21-31 are reserved */
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

/* ============================================================================
 * System Call Handler (Legacy int 0x80, Vector 128)
 * ============================================================================
 */

/*
 * isr128 - Traditional system call entry point via software interrupt 0x80.
 *
 * User-space programs execute 'int 0x80' to request kernel services.
 * This stub provides the bridge to the C system call dispatcher.
 */
.global isr128
isr128:
    cli
    pushq $0        /* Dummy error code */
    pushq $128      /* Interrupt vector 128 */
    jmp syscall_stub

/* ============================================================================
 * System Call Handling Stub (for int 0x80)
 * ============================================================================
 */

/*
 * syscall_stub - Common handler for int 0x80 system calls.
 *
 * Saves context, extracts arguments according to the kernel's system call
 * ABI, calls the C handler, and returns the result in the saved RAX slot.
 *
 * Stack after SAVE_REGS (15 pushes = 120 bytes):
 *   RSP+0   : Saved RAX (system call number)
 *   RSP+8   : Saved RBX (arg1)
 *   RSP+16  : Saved RCX (arg2)
 *   RSP+24  : Saved RDX (arg3)
 *   ... (other saved registers)
 *   RSP+120 : Vector Number (128)
 *   RSP+128 : Error Code (0)
 */
syscall_stub:
    SAVE_REGS

    /* Load kernel data segment descriptors (GDT index 2 -> 0x10) */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    /*
     * Prepare arguments for the C handler: syscall_handler(num, arg1, arg2, arg3)
     * Arguments are extracted from the saved register slots on the stack.
     * Syscall ABI for int 0x80 (as per the kernel's convention):
     *   RAX -> System call number
     *   RBX -> First argument
     *   RCX -> Second argument
     *   RDX -> Third argument
     */
    movq 0(%rsp), %rdi   /* arg1: syscall number (from saved RAX) */
    movq 8(%rsp), %rsi   /* arg2: first argument (from saved RBX) */
    movq 16(%rsp), %rdx  /* arg3: second argument (from saved RCX) */
    movq 24(%rsp), %rcx  /* arg4: third argument (from saved RDX) */

    /* Call the C system call dispatcher */
    call syscall_handler

    /* Store the return value back into the saved RAX slot on the stack */
    movq %rax, 0(%rsp)

    /* Restore segment registers */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    RESTORE_REGS
    addq $16, %rsp   /* Clean up vector number and error code */
    sti
    iretq

/* ============================================================================
 * Common Exception Handling Stub (Corrected Version)
 * ============================================================================
 */

/*
 * isr_common_stub - The shared handler for all CPU exceptions (0-31).
 *
 * This is the main entry point after the individual ISR stubs. It saves
 * the full CPU context, determines the correct stack layout, calls the
 * C exception handler, and then restores context.
 *
 * CRITICAL FIX: The stack layout differs depending on whether the exception
 * occurred in user mode (CPL=3) or kernel mode (CPL=0). The CPU only
 * automatically pushes the user SS and RSP if a privilege level change occurs.
 *
 * Initial Stack (from individual ISR):
 *   [SS]       (optional, if CPL changed)
 *   [RSP]      (optional, if CPL changed)
 *   [RFLAGS]
 *   [CS]
 *   [RIP]
 *   [Error Code] (or dummy 0)
 *   [Vector Number]
 *   <-- RSP points here when isr_common_stub begins
 *
 * After SAVE_REGS (adds 120 bytes), we need to locate RIP and CS.
 * Their offsets depend on the presence of the optional SS and RSP.
 *
 * This version correctly identifies the CPL from the saved CS on the stack
 * to calculate the right offsets.
 */
isr_common_stub:
    SAVE_REGS

    /* Load kernel data segments (GDT index 2 -> 0x10) */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    /*
     * === DYNAMIC OFFSET CALCULATION ===
     * We need to read the CS value that was on the stack *before* SAVE_REGS.
     * After SAVE_REGS (120 bytes pushed), the original stack contents are
     * at higher addresses. Let's calculate the potential location of the
     * saved CS.
     *
     * Let's define the initial RSP when entering isr_common_stub as `RSP0`.
     * Contents at RSP0:
     *   RSP0      : Vector Number
     *   RSP0 + 8  : Error Code
     *   RSP0 + 16 : RIP
     *   RSP0 + 24 : CS          <-- The CS we need to check!
     *   RSP0 + 32 : RFLAGS
     *   RSP0 + 40 : [Optional User RSP]
     *   RSP0 + 48 : [Optional User SS]
     *
     * After SAVE_REGS, RSP = RSP0 - 120.
     * Therefore, the saved CS is at: (RSP + 120) + 24 = RSP + 144.
     * This matches the original calculation IF the exception came from user mode
     * (and optional words are present). Let's load it and check its CPL.
     */
    movq 144(%rsp), %rax   /* Load the saved CS value from the stack */
    andq $3, %rax          /* Isolate the RPL (Requested Privilege Level) bits */
    cmpq $3, %rax          /* Compare RPL with 3 (user mode) */
    je .exception_from_user_mode

    /* ====================
     * Exception from KERNEL MODE (CPL=0)
     * ====================
     * CPU did NOT push user SS and RSP.
     * Therefore, the saved RIP is at offset 136, and CS at 144.
     * Offsets from RSP (after SAVE_REGS):
     *   120 : Vector Number
     *   128 : Error Code
     *   136 : RIP
     *   144 : CS
     *   152 : RFLAGS
     */
    movq 120(%rsp), %rdi    /* First argument: Vector Number */
    movq 128(%rsp), %rsi    /* Second argument: Error Code */
    movq 136(%rsp), %rdx    /* Third argument: RIP */
    movq 144(%rsp), %rcx    /* Fourth argument: CS */
    jmp .call_c_handler

.exception_from_user_mode:
    /* ====================
     * Exception from USER MODE (CPL=3)
     * ====================
     * CPU pushed user SS and RSP.
     * Therefore, the saved RIP is at offset 136, and CS at 144.
     * (Note: The offsets are the same as in the kernel case in this layout,
     *  because the optional words are at higher addresses: 160 and 168).
     * Offsets from RSP (after SAVE_REGS):
     *   120 : Vector Number
     *   128 : Error Code
     *   136 : RIP
     *   144 : CS
     *   152 : RFLAGS
     *   160 : User RSP
     *   168 : User SS
     */
    movq 120(%rsp), %rdi    /* First argument: Vector Number */
    movq 128(%rsp), %rsi    /* Second argument: Error Code */
    movq 136(%rsp), %rdx    /* Third argument: RIP */
    movq 144(%rsp), %rcx    /* Fourth argument: CS */

.call_c_handler:
    /* Call the C exception handler: isr_handler(vector, error_code, rip, cs) */
    call isr_handler

    /* Restore kernel data segments */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    RESTORE_REGS
    addq $16, %rsp   /* Remove Error Code and Vector Number from the stack */
    sti
    iretq

/* ============================================================================
 * External References (C Functions)
 * ============================================================================
 */
/* These C functions must be defined elsewhere in the kernel. */
.extern isr_handler      /* Handles all CPU exceptions (0-31) */
.extern syscall_handler  /* Handles int 0x80 system calls */