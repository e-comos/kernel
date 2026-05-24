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
 *
 * Register state when this handler is entered:
 * - RAX = system call number (from user)
 * - RDI = first argument (from user) 
 * - RSI = second argument (from user)
 * - RDX = third argument (from user)
 * - R10 = fourth argument (from user R8 - SYSCALL uses R10 instead of RCX for 4th param)
 * - R8 = fifth argument (from user R9)
 * - R9 = sixth argument (from user R10)
 * - RCX = return address (user RIP saved by CPU, was originally in user RCX)
 * - R11 = user RFLAGS (saved by CPU, was originally in user R11)
 * - RBX, RBP, R12-R15 = preserved from user context (not touched by SYSCALL)
 *
 * The existing syscall_handler expects: long syscall_handler(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3)
 * This is a standard C function that follows System V ABI calling convention:
 * - RDI = first parameter (num)
 * - RSI = second parameter (arg1) 
 * - RDX = third parameter (arg2)
 * - RCX = fourth parameter (arg3)
 *
 * So we need to transform:
 * - RAX (syscall number) → RDI (first param to syscall_handler)
 * - RDI (first arg from user) → RSI (second param to syscall_handler)
 * - RSI (second arg from user) → RDX (third param to syscall_handler)  
 * - RDX (third arg from user) → RCX (fourth param to syscall_handler)
 */
.globl syscall_entry
.type syscall_entry, @function
syscall_entry:
    /* Save all general-purpose registers that might be modified during the call */
    pushq %rax    /* Save RAX (syscall number) */
    pushq %rcx    /* Save RCX (user return address) */
    pushq %rdx    /* Save RDX (third argument from user) */
    pushq %rsi    /* Save RSI (second argument from user) */
    pushq %rdi    /* Save RDI (first argument from user) */
    pushq %r8     /* Save R8 (fifth argument from user) */
    pushq %r9     /* Save R9 (sixth argument from user) */
    pushq %r10    /* Save R10 (fourth argument from user) */
    pushq %r11    /* Save R11 (user RFLAGS) */
    
    /* TODO: Stack switching - In a complete implementation, we would load the
     * current thread's kernel stack pointer here. For now, we'll assume
     * we're already on a suitable kernel stack, but this is unsafe in a
     * multi-threaded environment.
     * 
     * In a real implementation, this would involve:
     * 1. Reading the current thread/task structure
     * 2. Loading its kernel stack pointer (from TSS or task structure)
     * 3. Switching to that kernel stack
     */
    
    /* Align stack to 16-byte boundary before calling C function.
     * After pushing 8 registers (64 bytes), we subtract 8 to align properly
     * for the function call (which pushes 8 bytes), maintaining 16-byte alignment.
     */
    subq $8, %rsp
    
    /* Prepare arguments for syscall_handler call:
     * syscall_handler(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3)
     * 
     * The x86-64 System V ABI for a function with 4 integer parameters:
     * - RDI = first parameter
     * - RSI = second parameter  
     * - RDX = third parameter
     * - RCX = fourth parameter
     * 
     * We need to map from current state (after saving registers):
     * - 56(%rsp) = original RAX (syscall number)
     * - 48(%rsp) = original RDI (first user argument) 
     * - 40(%rsp) = original RSI (second user argument)
     * - 32(%rsp) = original RDX (third user argument)
     */
    movq 56(%rsp), %rdi    /* First param: syscall number */
    movq 48(%rsp), %rsi    /* Second param: first argument from user */
    movq 40(%rsp), %rdx    /* Third param: second argument from user */
    movq 32(%rsp), %rcx    /* Fourth param: third argument from user */
    
    /* Call the existing syscall_handler function */
    call syscall_handler
    
    /* Restore stack alignment */
    addq $8, %rsp
    
    /* Save the return value from syscall_handler (currently in RAX) */
    pushq %rax
    
    /* Restore all previously saved general-purpose registers */
    popq %r11     /* Restore original R11 (user RFLAGS) */
    popq %r10     /* Restore original R10 */
    popq %r9      /* Restore original R9 */
    popq %r8      /* Restore original R8 */
    popq %rdi     /* Restore original RDI */
    popq %rsi     /* Restore original RSI */
    popq %rdx     /* Restore original RDX */
    popq %rcx     /* Restore original RCX (user return address) */
    popq %rax     /* Restore original RAX (discarded, return value is in current RAX) */
    
    /* SYSRET instruction returns to user mode:
     * - Jumps to address in RCX (our user return address)
     * - Restores RFLAGS from R11 
     * - Uses CS from STAR[63:48] with RPL=3 (user code segment)
     * - RAX contains the return value to user space
     */
    sysretq

.size syscall_entry, .-syscall_entry