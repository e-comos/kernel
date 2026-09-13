/* ===================================================================
 * kernel/boot/multiboot2_start.s
 *
 * Multiboot2-compliant kernel entry point (GAS/AT&T syntax).
 * This is the first code executed when GRUB loads the kernel.
 * It runs in 32-bit protected mode, validates the bootloader,
 * saves the Multiboot2 information structure securely, sets up a minimal
 * environment, and then switches to 64-bit long mode.
 *
 * GRUB will:
 *   1. Load this file (with the Multiboot2 header) at the load address
 *   2. Set EAX to the magic number 0x36d76289
 *   3. Set EBX to the physical address of the Multiboot2 info structure
 *   4. Jump to the entry point (_start)
 *
 * Compile with: gcc -m32 -c -o multiboot2_start.o multiboot2_start.s
 * =================================================================== */

.code32                         /* GRUB starts us in 32-bit protected mode */

/* ===================================== ==================== */
.global multiboot2_header_start
.global multiboot2_header_end
.global _start
.global multiboot2_info_phys
.global saved_multiboot_info

/* ==================== Constants ==================== */
.set KERNEL_VIRTUAL_BASE, 0xffff800000000000
.set KERNEL_LOAD_ADDR, 0x100000
.set SCRATCH_AREA_PHYS, 0x300000
.set MULTIBOOT_HEADER_PHYS, KERNEL_LOAD_ADDR
.set MULTIBOOT_ENTRY_PHYS, KERNEL_LOAD_ADDR + (multiboot2_header_end - multiboot2_header_start)
.set SAVED_MULTIBOOT_INFO_PHYS, SCRATCH_AREA_PHYS
.set MULTIBOOT_INFO_PHYS, SCRATCH_AREA_PHYS + 0x1000

/* ==================== Multiboot2 Header ==================== */
/* The Multiboot2 header must be in the first 32KB of the kernel file
 * and must be 64-bit aligned. GRUB will scan for this header. */

.section .multiboot2, "a"      /* "a" for allocatable, will be loaded */
.align 8                       /* Multiboot2 header must be 8-byte aligned */

multiboot2_header_start:
    /* Magic number, architecture, header length, checksum */
    .long 0xe85250d6           /* Multiboot2 magic number */
    .long 0                    /* Architecture: 0 = i386 (32-bit protected mode) */
    .long multiboot2_header_end - multiboot2_header_start  /* Header length */
    /* Checksum: -(magic + arch + header_length) mod 2^32 */
    .long -(0xe85250d6 + 0 + (multiboot2_header_end - multiboot2_header_start))

    /* Address tag (required) */
    .word 2                    /* Type: Address tag */
    .word 0                    /* Flags */
    .long 24                   /* Size: 24 bytes */
    .long MULTIBOOT_HEADER_PHYS /* Header address (where GRUB loaded it) */
    .long 0x100000             /* Load address: kernel should be loaded at 1MB */
    .long 0                    /* Load end address: 0 = whole file */
    .long 0                    /* BSS end address: 0 = no BSS segment */

    /* Entry address tag */
    .word 3                    /* Type: Entry address */
    .word 0                    /* Flags */
    .long 12                   /* Size: 12 bytes */
    .long KERNEL_LOAD_ADDR + (multiboot2_header_end - multiboot2_header_start) /* Entry point: _start after the header */
    
    /* End tag */
    .align 8
    .word 0                    /* Type: End */
    .word 0                    /* Flags */
    .long 8                    /* Size: 8 bytes */
	
    /* End tag (required) */
    .word 0                    /* Type: End tag */
    .word 0                    /* Flags */
    .long 8                    /* Size: 8 bytes */

multiboot2_header_end:

/* ==================== Kernel Entry Point ==================== */
.section .text
.type _start, @function

_start:
    /* Immediately disable interrupts */
    cli

    /* ------------------------------------------------------------
     * 1. Validate that we were loaded by a Multiboot2-compliant bootloader
     *    GRUB sets EAX to 0x36d76289 on entry
     * ------------------------------------------------------------ */
    cmpl $0x36d76289, %eax
    jne .not_multiboot2

    /* ------------------------------------------------------------
     * 2. Save the Multiboot2 information structure securely
     *    EBX contains the physical address of the Multiboot2 info structure.
     *    We immediately copy it to a safe buffer before BSS or page tables
     *    overwrite the memory area GRUB placed it in.
     * ------------------------------------------------------------ */
    movl (%ebx), %ecx                /* Get total structure size */
    cmpl $4096, %ecx                 /* Ensure it fits in our 4KB buffer */
    jg .hang                         /* Hang if it exceeds buffer limits */

    movl %ebx, %esi                  /* Source: physical address from GRUB */
    movl $SAVED_MULTIBOOT_INFO_PHYS, %edi /* Destination: safe kernel buffer */
    cld
    rep movsb                        /* Copy %ecx bytes safely */

    /* Do not touch the high-half .bss variable before paging is active.
     * The copied Multiboot2 pointer remains in the low scratch region until the
     * 64-bit runtime has mapped the high-half data area.
     */

    /* ------------------------------------------------------------
     * 3. Set up a minimal stack for 32-bit mode
     *    The stack grows downward. We'll use a small 16KB stack
     *    located at 0x70000 for the transition phase.
     * ------------------------------------------------------------ */
    movl $0x70000, %esp
    movl %esp, %ebp

    /* ------------------------------------------------------------
     * 4. Clear the direction flag (for string operations)
     *    Ensures string instructions (like rep movsb) increment pointers
     * ------------------------------------------------------------ */
    cld

    /* ------------------------------------------------------------
     * 5. Verify and display a simple boot message on screen
     *    This is a minimal sanity check that we're running
     * ------------------------------------------------------------ */
    movl $0xb8000, %edi        /* VGA text buffer address */
    movl $0x2f4b2f4d, (%edi)   /* Display "MK" (Multiboot2 Kernel) in green */
    addl $4, %edi
    movl $0x2f322f42, (%edi)   /* Display "B2" in green */

    /* ------------------------------------------------------------
     * 6. Call the 32-to-64-bit mode switcher
     *    This routine (in long_mode_switch.s) will:
     *    a. Check CPU support for long mode
     *    b. Set up low-address and high-half page tables
     *    c. Enable PAE, paging, and long mode
     *    d. Load a 64-bit GDT
     *    e. Far-jump to 64-bit code
     * ------------------------------------------------------------ */
    call switch_to_long_mode

    /* ------------------------------------------------------------
     * The call above should never return. If it does, something
     * went wrong in the mode switch.
     * ------------------------------------------------------------ */
    jmp .hang

/* ==================== Error Handlers ==================== */
.not_multiboot2:
    /* Display "!MB2" in red at top-left corner */
    movl $0xb8000, %edi
    movl $0x4f214f4d, (%edi)   /* "!M" in red */
    addl $4, %edi
    movl $0x4f424f32, (%edi)   /* "B2" in red */
    /* Fall through to halt */
    jmp .hang

.hang:
    cli
1:  hlt
    jmp 1b

/* ==================== Data Section ==================== */
.section .bss
.align 8
/* Reserve 4 bytes to store the 32-bit physical address of the
 * Multiboot2 information structure passed by GRUB in EBX.
 * The 64-bit switcher will read this and pass it to the 64-bit kernel. */
multiboot2_info_phys:
    .long 0

/* Reserve 4KB of safe space to hold the copied Multiboot2 info */
saved_multiboot_info:
    .space 4096

/* ==================== Stack Space ==================== */
/* Small stack for the 32-bit transition phase (4KB) */
.align 16
    .space 4096
stack_top:
