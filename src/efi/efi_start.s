.section .text
.global _start
.type _start, @function
_start:
	/* Minimal entrypoint for PE/EFI build: call kernel_main and halt if it returns */
	xor %rbp, %rbp
	call kernel_main
.hang:
	hlt
	jmp .hang
.size _start, .-_start
