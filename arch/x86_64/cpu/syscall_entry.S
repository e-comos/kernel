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

.global syscall_entry
.type syscall_entry, @function

syscall_entry:
    /* 1. Swap GS base to access per-CPU kernel data */
    swapgs

    /* 2. Switch from User Stack to Kernel Stack cleanly 
       (%gs:0x0 holds kernel stack, %gs:0x8 stores saved user RSP) 
       NEVER push onto user stack before this switch! */
    mov %rsp, %gs:0x8
    mov %gs:0x0, %rsp

    /* FIXME: Because the system reached the #GP but we cannot
     * found out who changed MSR, GS base, RSP or other fuck
     * prolems, so we need use this hack.
     * If you can, please fix this problem.
     * For now(in the Aug 7th, 2026) we can know the system failed
     * at shell(ebts) called the first syscall.
     * Because RSP, MSR or GS base was changed. But we cannot found out
     * who change it
     */

    /* 3. Save RAX,RCX and RDX because wrmsr will clobber them */
    push %rax          /* syscall number */
    push %rcx          /* user return address (RIP) */
    push %rdx          /* user arg2 */
    /* 4. Force restore IA32_KERNEL_GS_BASE to &bsp_percpu */
    lea bsp_percpu(%rip), %rax   /* get address of bsp_percpu */
    mov $0xC0000102, %ecx        /* IA32_KERNEL_GS_BASE MSR */
    xor %edx, %edx
    wrmsr

    /* 5. Restore RAX and RCX */
    pop %rdx
    pop %rcx
    pop %rax

    /* 6. Save all user registers onto the KERNEL stack */
    push %rax       /* [rsp + 112] Saved Syscall Number */
    push %rcx       /* [rsp + 104] Saved User RIP (pushed by syscall) */
    push %r11       /* [rsp + 96]  Saved User RFLAGS (pushed by syscall) */
    push %rbx       /* [rsp + 88] */
    push %rbp       /* [rsp + 80] */
    push %r12       /* [rsp + 72] */
    push %r13       /* [rsp + 64] */
    push %r14       /* [rsp + 56] */
    push %r15       /* [rsp + 48] */
    push %rdi       /* [rsp + 40]  User Arg 1 */
    push %rsi       /* [rsp + 32]  User Arg 2 */
    push %rdx       /* [rsp + 24]  User Arg 3 */
    push %r10       /* [rsp + 16]  User Arg 4 */
    push %r8        /* [rsp + 8]   User Arg 5 */
    push %r9        /* [rsp + 0]   User Arg 6 */

    /* 7. Align stack to 16-byte boundary */
    sub $8, %rsp

    /* 8. Shuffle registers to pass arguments to C syscall_handler */
    mov %r8,  %r9
    mov %r10, %r8
    mov %rdx, %rcx
    mov %rsi, %rdx
    mov %rdi, %rsi
    mov %rax, %rdi

    /* 9. Call C Kernel Handler */
    call syscall_handler

    /* 10. Undo 16-byte stack alignment padding */
    add $8, %rsp

    /* 11. Restore user registers in exact reverse order */
    pop %r9
    pop %r8
    pop %r10
    pop %rdx
    pop %rsi
    pop %rdi
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbp
    pop %rbx
    pop %r11
    pop %rcx

    /* Skip the saved RAX slot (8 bytes) on kernel stack */
    add $8, %rsp

    /* 12. Restore User Stack Pointer */
    mov %gs:0x8, %rsp

    /* 13. Swap GS back to User GS Base */
    swapgs

    /* 14. Return to User Space */
    sysretq

.size syscall_entry, .-syscall_entry
