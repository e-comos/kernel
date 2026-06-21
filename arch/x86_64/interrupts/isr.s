/* isr.s - 64-bit Interrupt Service Routine Stubs
 *
 * AT&T syntax, assembled with x86_64-elf-as --64.
 *
 * Stack layout AFTER isr_common_stub saves registers (from low addr / RSP):
 *
 *   [RSP+  0]  r15           \
 *   [RSP+  8]  r14            |
 *   [RSP+ 16]  r13            |
 *   [RSP+ 24]  r12            |
 *   [RSP+ 32]  r11            |
 *   [RSP+ 40]  r10            |  SAVE_REGS (15 pushq = 120 bytes)
 *   [RSP+ 48]  r9             |
 *   [RSP+ 56]  r8             |
 *   [RSP+ 64]  rbp            |
 *   [RSP+ 72]  rdi            |
 *   [RSP+ 80]  rsi            |
 *   [RSP+ 88]  rdx            |
 *   [RSP+ 96]  rcx            |
 *   [RSP+104]  rbx            |
 *   [RSP+112]  rax           /
 *   [RSP+120]  int_no        <- pushed by stub
 *   [RSP+128]  err_code      <- pushed by stub OR by CPU
 *   [RSP+136]  rip           \
 *   [RSP+144]  cs             |  pushed automatically by CPU on interrupt
 *   [RSP+152]  rflags         |
 *   [RSP+160]  rsp (user)     |
 *   [RSP+168]  ss            /
 *
 * This layout maps 1-to-1 onto registers_t defined in isr.c.
 *
 * isr_handler C signature:
 *   void isr_handler(registers_t *regs)    -- rdi = regs = RSP after SAVE_REGS
 *
 * syscall_handler C signature:
 *   long syscall_handler(registers_t *regs, uint64_t num,
 *                        uint64_t arg1, uint64_t arg2, uint64_t arg3)
 */

.section .text

.extern isr_handler
.extern syscall_handler

.global isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7
.global isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15
.global isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
.global isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
.global isr128

/* ---------------------------------------------------------------------------
 * SAVE_REGS / RESTORE_REGS
 * Push order (first push = highest address, last push -> RSP):
 *   rax rbx rcx rdx rsi rdi rbp r8 r9 r10 r11 r12 r13 r14 r15
 * registers_t field order (low address / RSP first):
 *   r15 r14 r13 r12 r11 r10 r9 r8 rbp rdi rsi rdx rcx rbx rax
 * --------------------------------------------------------------------------- */
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
    popq  %r15
    popq  %r14
    popq  %r13
    popq  %r12
    popq  %r11
    popq  %r10
    popq  %r9
    popq  %r8
    popq  %rbp
    popq  %rdi
    popq  %rsi
    popq  %rdx
    popq  %rcx
    popq  %rbx
    popq  %rax
.endm

/* ============================================================================
 * Exception entry points  (vectors 0-31)
 *
 * Vectors WITH a CPU-pushed error code (no dummy needed):
 *   8  #DF   Double Fault
 *   10 #TS   Invalid TSS
 *   11 #NP   Segment Not Present
 *   12 #SS   Stack-Segment Fault
 *   13 #GP   General Protection Fault
 *   14 #PF   Page Fault
 *   17 #AC   Alignment Check
 *   21 #CP   Control-Protection Exception
 *   29 #VC   VMM Communication Exception
 *   30 #SX   Security Exception
 *
 * All others push dummy $0 first so that err_code is always present.
 * ============================================================================ */

isr0:  cli; pushq $0;  pushq $0;  jmp isr_common_stub
isr1:  cli; pushq $0;  pushq $1;  jmp isr_common_stub
isr2:  cli; pushq $0;  pushq $2;  jmp isr_common_stub
isr3:  cli; pushq $0;  pushq $3;  jmp isr_common_stub
isr4:  cli; pushq $0;  pushq $4;  jmp isr_common_stub
isr5:  cli; pushq $0;  pushq $5;  jmp isr_common_stub
isr6:  cli; pushq $0;  pushq $6;  jmp isr_common_stub
isr7:  cli; pushq $0;  pushq $7;  jmp isr_common_stub
isr8:  cli;            pushq $8;  jmp isr_common_stub   /* #DF - CPU pushes err_code */
isr9:  cli; pushq $0;  pushq $9;  jmp isr_common_stub
isr10: cli;            pushq $10; jmp isr_common_stub   /* #TS */
isr11: cli;            pushq $11; jmp isr_common_stub   /* #NP */
isr12: cli;            pushq $12; jmp isr_common_stub   /* #SS */
isr13: cli;            pushq $13; jmp isr_common_stub   /* #GP */
isr14: cli;            pushq $14; jmp isr_common_stub   /* #PF */
isr15: cli; pushq $0;  pushq $15; jmp isr_common_stub
isr16: cli; pushq $0;  pushq $16; jmp isr_common_stub
isr17: cli;            pushq $17; jmp isr_common_stub   /* #AC */
isr18: cli; pushq $0;  pushq $18; jmp isr_common_stub
isr19: cli; pushq $0;  pushq $19; jmp isr_common_stub
isr20: cli; pushq $0;  pushq $20; jmp isr_common_stub
isr21: cli;            pushq $21; jmp isr_common_stub   /* #CP */
isr22: cli; pushq $0;  pushq $22; jmp isr_common_stub
isr23: cli; pushq $0;  pushq $23; jmp isr_common_stub
isr24: cli; pushq $0;  pushq $24; jmp isr_common_stub
isr25: cli; pushq $0;  pushq $25; jmp isr_common_stub
isr26: cli; pushq $0;  pushq $26; jmp isr_common_stub
isr27: cli; pushq $0;  pushq $27; jmp isr_common_stub
isr28: cli; pushq $0;  pushq $28; jmp isr_common_stub
isr29: cli;            pushq $29; jmp isr_common_stub   /* #VC */
isr30: cli;            pushq $30; jmp isr_common_stub   /* #SX */
isr31: cli; pushq $0;  pushq $31; jmp isr_common_stub

/* System call entry (int 0x80, vector 128) */
isr128: cli; pushq $0; pushq $128; jmp syscall_stub

/* ============================================================================
 * isr_common_stub
 *
 * On arrival the stub-pushed fields are already on the stack:
 *   [RSP] = int_no   [RSP+8] = err_code   then CPU frame above
 *
 * After SAVE_REGS, RSP points at registers_t.
 * We pass RSP as the single argument (rdi) to isr_handler.
 * isr_handler never returns (it halts), but we keep the iretq path
 * for non-fatal vectors (#BP, #OF) that do return 0.
 * ============================================================================ */
isr_common_stub:
    SAVE_REGS

    /* Switch to kernel data segment */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    movq %rsp, %rdi          /* rdi = registers_t* */
    call isr_handler         /* isr_handler(regs) */

    /* Restore caller segments (for non-fatal return) */
    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    RESTORE_REGS
    addq $16, %rsp           /* discard int_no + err_code */
    sti
    iretq

/* ============================================================================
 * syscall_stub  (int 0x80)
 *
 * User-space calling convention:
 *   rax = syscall number
 *   rbx = arg1
 *   rcx = arg2
 *   rdx = arg3
 *
 * C signature:
 *   long syscall_handler(registers_t *regs,
 *                        uint64_t num, uint64_t arg1,
 *                        uint64_t arg2, uint64_t arg3)
 *
 * After SAVE_REGS the saved-register offsets from RSP are:
 *   rax @ [RSP+112]   rbx @ [RSP+104]
 *   rcx @ [RSP+ 96]   rdx @ [RSP+ 88]
 * ============================================================================ */
syscall_stub:
    SAVE_REGS

    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    movq %rsp,       %rdi    /* arg0: registers_t* */
    movq 112(%rsp),  %rsi    /* arg1: num  (saved rax) */
    movq 104(%rsp),  %rdx    /* arg2: arg1 (saved rbx) */
    movq  96(%rsp),  %rcx    /* arg3: arg2 (saved rcx) */
    movq  88(%rsp),  %r8     /* arg4: arg3 (saved rdx) */

    call syscall_handler

    /* Write return value back into saved rax so caller sees it */
    movq %rax, 112(%rsp)

    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    RESTORE_REGS
    addq $16, %rsp
    sti
    iretq
