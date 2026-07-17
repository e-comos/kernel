/*
    E-comOS Kernel - SYSCALL entry point
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

.section .text

/*
 * syscall_entry - Entry point for SYSCALL instruction
 *
 * SYSCALL instruction behavior:
 * - Saves user RIP to RCX register
 * - Saves user RFLAGS to R11 register
 * - Jumps to address in MSR_LSTAR register
 * - Loads kernel CS from STAR[47:32] into CS (with RPL forced to 0)
 * - Clears IF and TF flags in RFLAGS (due to SFMASK configuration)
 * - Does NOT change RSP (still points to user stack!)
 *
 * Per-CPU data structure (accessed via GS base):
 * Offset 0: kernel_rsp (kernel stack pointer)
 * Offset 8: user_rsp (saved user stack pointer)
 */

.globl syscall_entry
.type syscall_entry, @function
syscall_entry:
    /* Step 1: Swap GS base to access per-CPU data */
    swapgs

    /* Step 2: Save user RSP and switch to kernel stack */
    movq    %rsp, %gs:8       /* Save user RSP to per-CPU (offset 8) */
    movq    %gs:0, %rsp       /* Load kernel RSP from per-CPU (offset 0) */

    /* Step 3: Save registers on kernel stack
     * After all pushes, stack layout from RSP:
     * 0   = %r15 (pushed last)
     * 8   = %r14
     * 16  = %r13
     * 24  = %r12
     * 32  = %rbp
     * 40  = %rbx
     * 48  = %r11 (user RFLAGS)
     * 56  = %r10 (arg4)
     * 64  = %r9
     * 72  = %r8
     * 80  = %rdi (arg1)
     * 88  = %rsi (arg2)
     * 96  = %rdx (arg3)
     * 104 = %rcx (user RIP)
     * 112 = %rax (syscall number, pushed first)
     */
    pushq   %rax                /* syscall number */
    pushq   %rcx                /* user RIP */
    pushq   %rdx                /* arg3 */
    pushq   %rsi                /* arg2 */
    pushq   %rdi                /* arg1 */
    pushq   %r8
    pushq   %r9
    pushq   %r10                /* arg4 */
    pushq   %r11                /* user RFLAGS */
    pushq   %rbx
    pushq   %rbp
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15

    /* Step 4: Set up arguments for syscall_handler
     * System V ABI: RDI=num, RSI=arg1, RDX=arg2, RCX=arg3
     */
    movq    112(%rsp), %rdi    /* Syscall number (original %rax) */
    movq    80(%rsp), %rsi     /* Original %rdi (arg1) */
    movq    88(%rsp), %rdx     /* Original %rsi (arg2) */
    movq    96(%rsp), %rcx     /* Original %rdx (arg3) */

    /* Step 5: Call C syscall handler */
    call    syscall_handler

    /* Step 6: Save return value by overwriting saved %rax on stack */
    movq    %rax, 112(%rsp)

    /* Step 7: Restore registers from kernel stack in exact reverse push order */
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbp
    popq    %rbx
    popq    %r11                /* Matches pushq %r11 */
    popq    %r10                /* Matches pushq %r10 */
    popq    %r9                 /* Matches pushq %r9 */
    popq    %r8                 /* Matches pushq %r8 */
    popq    %rdi                /* Matches pushq %rdi */
    popq    %rsi                /* Matches pushq %rsi */
    popq    %rdx                /* Matches pushq %rdx */
    popq    %rcx                /* Matches pushq %rcx (user RIP) */
    popq    %rax                /* Matches pushq %rax (return value) */

    /* Step 8: Restore user RSP */
    movq    %gs:8, %rsp         /* Restore user RSP from per-CPU */

    /* Step 9: Swap GS base back to user mode */
    swapgs

    /* Step 10: Execute SYSRET */
    sysretq

.size syscall_entry, .-syscall_entry