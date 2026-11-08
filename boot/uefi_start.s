/*
 * E-comOS - Kernel entry point (64-bit UEFI)
 *
 * The UEFI firmware calls this image as a standard EFI application.
 * By the time we reach _start the CPU is already in 64-bit long mode.
 *
 * Calling convention (System V AMD64 ABI):
 *	 rdi = first argument = BootParams* (set by the bootloader before jmp)
 *
 * We preserve rdi across the register clear so kernelMain receives it.
 */

.text
.global _start
.extern kernelMain

_start:
	/* Save BootParams* before we clobber registers */
	movq %rdi, %r15

	/* Set up a known-good kernel stack */
	movq $0x70000, %rsp

	/* Clear direction flag */
	cld

	/* Zero all GP registers except r15 (holds BootParams*) */
	xorq %rax, %rax
	xorq %rbx, %rbx
	xorq %rcx, %rcx
	xorq %rdx, %rdx
	xorq %rsi, %rsi
	xorq %rbp, %rbp
	xorq %r8,  %r8
	xorq %r9,  %r9
	xorq %r10, %r10
	xorq %r11, %r11
	xorq %r12, %r12
	xorq %r13, %r13
	xorq %r14, %r14

	/* Restore BootParams* into rdi (first argument) */
	movq %r15, %rdi
	xorq %r15, %r15

	/* Call kernelMain(BootParams *bootParams) */
	call kernelMain

	/* kernelMain must never return; halt if it does */
.hang:
	cli
	hlt
	jmp .hang
