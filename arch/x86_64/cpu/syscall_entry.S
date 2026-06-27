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
 *   Offset 0: kernel_rsp (kernel stack pointer)
 *   Offset 8: user_rsp (saved user stack pointer)
 */

.globl syscall_entry
.type syscall_entry, @function
syscall_entry:
    /*
     * Step 1: Swap GS base
     * User mode: GS base = user GS base (if any)
     * Kernel mode: GS base = per-CPU data structure
     */
    swapgs

    /*
     * Step 2: Save user RSP and switch to kernel stack
     * At this point, RSP still points to user stack!
     */
    movq    %rsp, %gs:8       /* Save user RSP to per-CPU (offset 8) */
    movq    %gs:0, %rsp       /* Load kernel RSP from per-CPU (offset 0) */

    /*
     * Step 3: Save registers on kernel stack
     * Save user state for syscall_handler and later restoration
     */
    pushq   %rax                /* System call number */
    pushq   %rcx                /* User RIP (saved by SYSCALL) */
    pushq   %rdx                /* Arg3 */
    pushq   %rsi                /* Arg2 */
    pushq   %rdi                /* Arg1 */
    pushq   %r8                 /* Arg5 */
    pushq   %r9                 /* Arg6 */
    pushq   %r10                /* Arg4 (SYSCALL uses R10 instead of RCX) */
    pushq   %r11                /* User RFLAGS (saved by SYSCALL) */
    pushq   %rbx                /* Preserve */
    pushq   %rbp                /* Preserve */
    pushq   %r12                /* Preserve */
    pushq   %r13                /* Preserve */
    pushq   %r14                /* Preserve */
    pushq   %r15                /* Preserve */

    /*
     * Step 4: Set up arguments for syscall_handler
     * System V ABI: RDI=num, RSI=arg1, RDX=arg2, RCX=arg3
     */
    movq    15*8(%rsp), %rdi    /* Syscall number from stack */
    movq    11*8(%rsp), %rsi    /* Original RDI (arg1) */
    movq    10*8(%rsp), %rdx    /* Original RSI (arg2) */
    movq    9*8(%rsp), %rcx     /* Original RDX (arg3) */

    /*
     * Step 5: Call C syscall handler
     */
    call    syscall_handler

    /*
     * Step 6: Save return value (will be restored to RAX later)
     */
    movq    %rax, %rbx          /* Save return value */

    /*
     * Step 7: Restore registers from kernel stack
     */
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbp
    popq    %rbx                /* This was pushed RBX, not return value */
    popq    %r11                /* User RFLAGS */
    popq    %r10                /* Arg4 */
    popq    %r9                 /* Arg6 */
    popq    %r8                 /* Arg5 */
    popq    %rdi                /* Arg1 */
    popq    %rsi                /* Arg2 */
    popq    %rdx                /* Arg3 */
    popq    %rcx                /* User RIP */
    popq    %rax                /* Original RAX (syscall number) */

    /*
     * Step 8: Restore user RSP
     */
    movq    %gs:8, %rsp       /* Restore user RSP from per-CPU (offset 8) */

    /*
     * Step 9: Swap GS base back to user mode
     */
    swapgs

    /*
     * Step 10: Restore return value and execute SYSRET
     * RAX = return value, RCX = user RIP, R11 = user RFLAGS
     */
    movq    %rbx, %rax          /* Return value */
    sysretq

.size syscall_entry, .-syscall_entry
