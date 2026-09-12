/* ============================================================================
 * multiboot2.ld - Linker script for Multiboot2-compatible kernel
 *
 * This linker script defines the memory layout for a kernel that will be
 * loaded by a Multiboot2-compliant bootloader (like GRUB). The kernel
 * is loaded at the physical address 0x100000 and linked at the higher-half
 * virtual base 0xffff800000000000 + 0x100000.
 * ============================================================================ */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

KERNEL_VIRTUAL_BASE = 0xffff800000000000;
KERNEL_LOAD_ADDR = 0x100000;

PHDRS
{
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = KERNEL_VIRTUAL_BASE + KERNEL_LOAD_ADDR;
    _kernel_start = .;

    .multiboot2 : AT(KERNEL_LOAD_ADDR) ALIGN(8)
    {
        KEEP(*(.multiboot2))
    } :text

    . = ALIGN(8);
    .text : AT(LOADADDR(.multiboot2) + SIZEOF(.multiboot2)) ALIGN(8)
    {
        boot/multiboot2_start.o(.text)
        boot/long_mode_switch.o(.text)
        *(.text)
        *(.text.*)
    } :text

    .rodata : AT(LOADADDR(.text) + SIZEOF(.text)) ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    } :rodata

    .data : AT(LOADADDR(.rodata) + SIZEOF(.rodata)) ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    } :data

    .paging (NOLOAD) : ALIGN(4K)
    {
        *(.paging)
    } :data

    .bss (NOLOAD) : ALIGN(4K)
    {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    } :data

    _kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}

PROVIDE(_kernel_phys_start = KERNEL_LOAD_ADDR);
PROVIDE(_kernel_start = KERNEL_VIRTUAL_BASE + KERNEL_LOAD_ADDR);
