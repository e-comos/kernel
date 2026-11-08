/*
 * ============================================================================
 * E-comOS x86_64 - Context Switching Assembly
 * 64-bit version
 * ============================================================================
 */

.text
.global context_switch
.type context_switch, @function

/*
 * void context_switch(struct cpu_context *old_ctx, struct cpu_context *new_ctx)
 *
 * In x86_64 calling convention:
 *	 rdi = first argument (old_ctx)
 *	 rsi = second argument (new_ctx)
 *
 * CPU context structure (64-bit version, 8-byte aligned):
 *	 0x00: r15
 *	 0x08: r14
 *	 0x10: r13
 *	 0x18: r12
 *	 0x20: rbp
 *	 0x28: rbx
 *	 0x30: rip
 *	 0x38: rsp
 *	 (adjust according to your actual struct definition)
 */
context_switch:
	/* Save old context */
	testq %rdi, %rdi
	jz .skip_save		   # if old_ctx is NULL, skip saving
	
	/* Save preserved registers (according to System V AMD64 ABI) */
	movq %r15, 0x00(%rdi)  # save r15
	movq %r14, 0x08(%rdi)  # save r14
	movq %r13, 0x10(%rdi)  # save r13
	movq %r12, 0x18(%rdi)  # save r12
	movq %rbp, 0x20(%rdi)  # save rbp
	movq %rbx, 0x28(%rdi)  # save rbx
	
	/* Save instruction pointer (return address) */
	movq (%rsp), %rax
	movq %rax, 0x30(%rdi)  # save rip
	
	/* Save stack pointer (current RSP + 8 for return address) */
	leaq 8(%rsp), %rax
	movq %rax, 0x38(%rdi)  # save rsp
	
.skip_save:
	/* Load new context */
	movq 0x00(%rsi), %r15  # restore r15
	movq 0x08(%rsi), %r14  # restore r14
	movq 0x10(%rsi), %r13  # restore r13
	movq 0x18(%rsi), %r12  # restore r12
	movq 0x20(%rsi), %rbp  # restore rbp
	movq 0x28(%rsi), %rbx  # restore rbx
	
	/* Restore stack pointer */
	movq 0x38(%rsi), %rsp  # restore rsp
	
	/* Jump to new instruction pointer */
	jmpq *0x30(%rsi)	   # jump to saved rip

.size context_switch, . - context_switch