/* ===================================================================
 * kernel/boot/long_mode_switch.s
 *
 * Long Mode Switch Routine (GAS/AT&T Syntax)
 * Switches from 32-bit protected mode to 64-bit long mode.
 * =================================================================== */

.code32                         /* 32-bit protected mode code */

/* ==================== Exported Symbols ==================== */
.global switch_to_long_mode
.global check_long_mode
.global setup_page_tables
.global enable_paging

/* ==================== Imported Symbols ==================== */
.extern kernel_main

/* ==================== Constants ==================== */
.set PAGE_PRESENT,   0x01       /* Page present in memory */
.set PAGE_WRITE,     0x02       /* Page is writable */
.set PAGE_HUGE,      0x80       /* 2MB page (huge page) */
.set KERNEL_HIGH_BASE, 0xffff800000000000
.set KERNEL_KERNEL_VIRT, 0xffff800000100000

.set CR0_PG,         0x80000000 /* CR0: Paging enable bit (bit 31) */
.set CR4_PAE,        0x20       /* CR4: PAE enable bit (bit 5) */
.set EFER_MSR,       0xC0000080 /* EFER Model Specific Register number */
.set EFER_LME,       0x100      /* EFER: Long Mode Enable (bit 8) */

/* Use physical addresses for the early 32-bit setup phase. These are not
 * high-half virtual addresses; they are valid runtime physical locations before
 * the high-half mapping is enabled. */
.set PAGE_TABLE_L4_PHYS, 0x200000
.set PAGE_TABLE_L3_PHYS, 0x201000
.set PAGE_TABLE_L2_PHYS, 0x202000
.set PAGE_TABLE_L1_PHYS, 0x203000
.set SAVED_MULTIBOOT_INFO_PHYS, 0x300000

/* ==================== Switch Function ==================== */
switch_to_long_mode:
    /* Save callee-saved registers (32-bit calling convention) */
    pushl   %ebx
    pushl   %ecx
    pushl   %edx

    /* Check if CPU supports long mode */
    call    check_long_mode
    testl   %eax, %eax
    jz      .no_long_mode_error

    movl    %ebx, %esi
    /* Set up low-address identity mapping and high-half kernel mapping */
    call    setup_page_tables

    /* Enable PAE, paging, and long mode */
    call    enable_paging

    /* Mask PIC to prevent IRQs during mode switch */
    movb    $0xff, %al
    outb    %al, $0x21
    outb    %al, $0xa1

    /* Build a temporary GDT in low identity-mapped physical memory so that
     * LGDT can be executed safely in 32-bit protected mode without relying on
     * high-half linked virtual symbols. We place the GDT at PAGE_TABLE_L4_PHYS + 0x800.
     */
    movl    $PAGE_TABLE_L4_PHYS, %edi
    addl    $0x800, %edi
    /* Null descriptor */
    movl    $0x0, (%edi)
    movl    $0x0, 4(%edi)
    /* Code descriptor (low dword then high dword) */
    movl    $0x0, 8(%edi)
    movl    $0x00209A00, 12(%edi)
    /* Data descriptor */
    movl    $0x0, 16(%edi)
    movl    $0x00009200, 20(%edi)

    /* Build GDTR (2-byte limit, 4-byte base) at PAGE_TABLE_L4_PHYS + 0x820.
     * Use the same addressing style already used in this file for memory
     * constants so operands are identity-accessible in 32-bit mode.
     */
    movw    $0x17, PAGE_TABLE_L4_PHYS + 0x820
    movl    $PAGE_TABLE_L4_PHYS + 0x800, PAGE_TABLE_L4_PHYS + 0x822

    /* Load the GDTR from identity-mapped low memory */
    lgdt    PAGE_TABLE_L4_PHYS + 0x820

    /* Jump to the high-half 64-bit entry point after paging and long mode are enabled */
    .byte   0x48, 0xEA
    .quad   long_mode_jump
    .word   0x08

    /* Should not reach here */
    jmp     .hang

.no_long_mode_error:
    /* Display error: "NO LM" at top-left of screen (VGA text buffer) */
    movl    $0x4f204f4e, 0xb8000  /* Red 'N', Red 'O' */
    movl    $0x4f204f4c, 0xb8004  /* Red 'L', Red 'M' */

.hang:
    cli
1:  hlt
    jmp     1b

/* ==================== Check Long Mode Support ==================== */
check_long_mode:
    /* Returns: eax = 1 (supported), 0 (not supported) */

    /* Check if CPUID is available */
    pushfl
    popl    %eax
    movl    %eax, %ecx
    xorl    $0x200000, %eax
    pushl   %eax
    popfl
    pushfl
    popl    %eax
    pushl   %ecx
    popfl
    xorl    %ecx, %eax
    andl    $0x200000, %eax
    jz      .no_cpuid

    /* Check for extended processor info */
    movl    $0x80000000, %eax
    cpuid
    cmpl    $0x80000001, %eax
    jb      .no_long_mode

    /* Check the long mode (LM) bit in extended feature flags */
    movl    $0x80000001, %eax
    cpuid
    testl   $(1 << 29), %edx
    jz      .no_long_mode

    movl    $1, %eax
    ret

.no_cpuid:
.no_long_mode:
    xorl    %eax, %eax
    ret

/* ==================== Set Up Page Tables ==================== */
setup_page_tables:
    /* Clear the four page-table pages in one pass.
     * Each page is 4096 bytes and stosl writes 4 bytes per iteration,
     * so 4096 iterations clears exactly 4 * 4096 bytes.
     */
    movl    $PAGE_TABLE_L4_PHYS, %edi
    movl    $4096, %ecx
    xorl    %eax, %eax
    rep; stosl

    /* Set up PML4[0] -> PDPT (low, identity alias) */
    movl    $PAGE_TABLE_L3_PHYS, %eax
    orl     $(PAGE_PRESENT + PAGE_WRITE), %eax
    /* Write full 8-byte PML4 entry (low dword then high dword) */
    movl    %eax, PAGE_TABLE_L4_PHYS
    movl    $0x0, PAGE_TABLE_L4_PHYS + 4

    /* Set up PML4[256] -> PDPT (high-half kernel mapping) */
    movl    %eax, PAGE_TABLE_L4_PHYS + 256*8
    movl    $0x0, PAGE_TABLE_L4_PHYS + 256*8 + 4

    /* Set up PDPT[0] -> PD */
    movl    $PAGE_TABLE_L2_PHYS, %eax
    orl     $(PAGE_PRESENT + PAGE_WRITE), %eax
    movl    %eax, PAGE_TABLE_L3_PHYS
    movl    $0x0, PAGE_TABLE_L3_PHYS + 4

    /* Set up PD[0] -> PT */
    movl    $PAGE_TABLE_L1_PHYS, %eax
    orl     $(PAGE_PRESENT + PAGE_WRITE), %eax
    movl    %eax, PAGE_TABLE_L2_PHYS
    movl    $0x0, PAGE_TABLE_L2_PHYS + 4

    /* Fill PT entries 256..511 with the low identity mapping for
     * virtual 0x100000..0x1fffff and the matching high-half alias.
     * This gives: low vaddr == phys at 1MB and high vaddr = KERNEL_BASE + phys.
     * 64-bit PTEs must be written as 8-byte values even though the code runs
     * in 32-bit mode during the early page-table setup phase.
     */
    movl    $PAGE_TABLE_L1_PHYS, %edi
    /* Start writing at PT index 256 (offset 256*8 = 2048) */
    addl    $2048, %edi
    movl    $0x100000, %eax
    movl    $256, %ecx
1:
    movl    %eax, %ebx
    xorl    %edx, %edx
    orl     $(PAGE_PRESENT + PAGE_WRITE), %ebx
    movl    %ebx, (%edi)
    movl    %edx, 4(%edi)
    addl    $0x1000, %eax
    addl    $8, %edi
    loop    1b

    ret
/* ==================== Enable Paging ==================== */
enable_paging:
    /* Enable PAE (Physical Address Extension) in CR4 */
    movl    %cr4, %eax
    orl     $CR4_PAE, %eax
    movl    %eax, %cr4

    /* Load CR3 with the physical address of the PML4 */
    movl    $PAGE_TABLE_L4_PHYS, %eax
    movl    %eax, %cr3

    /* Enable Long Mode by setting the EFER.LME bit */
    movl    $EFER_MSR, %ecx
    rdmsr
    orl     $EFER_LME, %eax
    wrmsr

    /* Enable paging by setting CR0.PG bit */
    movl    %cr0, %eax
    orl     $CR0_PG, %eax
    movl    %eax, %cr0
    ret

/* ==================== 64-bit Code Section ==================== */
.code64
long_mode_jump:
    movq    %rsi, %rdi
    /* Reload data segment selectors for 64-bit mode */
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    movw    %ax, %ss

    /* Set up a valid 64-bit kernel stack above the loaded image.
     * The stack must live inside the mapped high-half window; placing it at
     * KERNEL_HIGH_BASE + 0x90000 would not be backed by the early page tables
     * because the kernel image itself begins at KERNEL_HIGH_BASE + 0x100000.
     */
    movq    $KERNEL_HIGH_BASE + 0x100000 + 0x90000, %rsp

    /* Set the runtime Multiboot2 pointer now that the high-half .bss is valid */
    movl    $SAVED_MULTIBOOT_INFO_PHYS, multiboot2_info_phys(%rip)

    /* Set up IDT pointer (must be done before any interrupts) */
    leaq    idt64_pointer(%rip), %rax
    lidt    (%rax)

    /* Call the 64-bit C kernel entry point */
    call    kernel_main

    /* Should not return */
    cli
1:  hlt
    jmp     1b

/* ==================== Page Tables (uninitialized) ==================== */
.section .bss
.align 4096
page_table_l4:
    .skip 4096
page_table_l3:
    .skip 4096
page_table_l2:
    .skip 4096
page_table_l1:
	.skip 4096

/* ==================== Simple IDT for 64-bit mode ==================== */
/* Add minimal IDT to handle exceptions */
.section .rodata
.align 8
idt64:
    .rept 256
    .word 0          /* Offset 0-15 */
    .word 0x08       /* Segment selector */
    .byte 0          /* IST */
    .byte 0x8E       /* Type: 64-bit interrupt gate */
    .word 0          /* Offset 16-31 */
    .long 0          /* Offset 32-63 */
    .long 0          /* Reserved */
    .endr
idt64_pointer:
    .word . - idt64 - 1
    .quad idt64

/* ==================== Read-only Data (GDT) ==================== */
.section .rodata
.align 8
/* Corrected GDT entries */
gdt64:
    .quad 0x0000000000000000     /* Null descriptor */
    .quad 0x00209A0000000000     /* Code segment: 64-bit, present, executable, read */
    .quad 0x0000920000000000     /* Data segment: 64-bit, present, writable */

gdt64_pointer:
    .word   . - gdt64 - 1        /* GDT limit */
    .quad   gdt64                /* GDT base address */
