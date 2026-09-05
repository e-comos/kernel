/* ============================================================================
 * multiboot2.ld - Linker script for Multiboot2-compatible kernel
 * 
 * This linker script defines the memory layout for a kernel that will be
 * loaded by a Multiboot2-compliant bootloader (like GRUB). The kernel
 * is loaded at 1MB (0x100000) physical address.
 * 
 * Key requirements:
 * 1. The Multiboot2 header must be at the very beginning of the file
 * 2. Entry point (_start) should be immediately after the header
 * 3. Text section should follow immediately without large gaps
 * 4. Symbols _kernel_start and _kernel_end are provided for C code
 * ============================================================================ */

/* Output format: 64-bit ELF for x86_64 architecture */
OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

_kernel_phys_start = 0x100000;

SECTIONS
{
		/* Kernel loaded at 1MB (0x100000) by GRUB */
		. = 0xffff800000000000 + _kernel_phys_start;
    
		/* Symbol for kernel start address (used by C code) */
		_kernel_start = .;
    
		PHDRS
		{
			text PT_LOAD FLAGS(5); /* read and exec */
			rodata PT_LOAD FLAGS(4); /* read only */
			data PT_LOAD FLAGS(6); /* readable and writable */
		}
		/* Multiboot2 header must be at the very beginning */
		/* ALIGN(8) ensures 8-byte alignment as required by Multiboot2 spec */
		.multiboot2 : ALIGN(8) {
				KEEP(*(.multiboot2))
		}
    
		/* Align to 8 bytes to ensure proper alignment for text section */
		. = ALIGN(8);
    
		/* 32-bit startup code and kernel text */
		/* NOTE: Changed from ALIGN(4K) to prevent large gaps */
		.text : {
				/* Ensure boot assembly code comes first */
				boot/multiboot2_start.o(.text)
				boot/long_mode_switch.o(.text)
        
				/* Then all other code */
				*(.text)
				*(.text.*)
		}
    
		/* Read-only data section */
		.rodata : ALIGN(4K) {
				*(.rodata)
				*(.rodata.*)
		}

		/* Initialized data section */
		.data : ALIGN(4K) {
				*(.data)
				*(.data.*)
		}
    
		/* Uninitialized data and stack (BSS) */
		.bss : ALIGN(4K) {
				*(.bss)
				*(.bss.*)
				*(COMMON)
		}

		.paging : ALIGN(4K) {
				*(.paging)
		}    

		/* Symbol for kernel end address (used by C code) */
		_kernel_end = .;

		/* Discard debugging and unnecessary sections */
		/DISCARD/ : {
				*(.comment)
				*(.note.*)
		}
    
		/* Verify that _start is at the beginning of .text section */
		ASSERT(_start >= ADDR(.text) && _start < ADDR(.text) + 0x100, 
		"Link order error: _start is not at the beginning of .text section")
}

PROVIDE(_kernel_phys_start = 0x100000);
PROVIDE(_kernel_start = 0xffff800000000000 + 0x100000);
