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

.set CR0_PG,         0x80000000 /* CR0: Paging enable bit (bit 31) */
.set CR4_PAE,        0x20       /* CR4: PAE enable bit (bit 5) */
.set EFER_MSR,       0xC0000080 /* EFER Model Specific Register number */
.set EFER_LME,       0x100      /* EFER: Long Mode Enable (bit 8) */

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
    /* Set up identity page tables */
    call    setup_page_tables

    /* Enable PAE, paging, and long mode */
    call    enable_paging

    /* Load the 64-bit Global Descriptor Table */
    lgdt    gdt64_pointer

    /* Far jump to 64-bit code segment (0x08 is code selector in GDT) */
    ljmp    $0x08, $long_mode_jump

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
    /* Clear page table memory (PML4, PDP, PD) */
    movl    $page_table_l4, %edi
    movl    $(4096 * 3 / 4), %ecx
    xorl    %eax, %eax
    rep; stosl

    /* Set up PML4 (Page Map Level 4) entry 0 to point to PDP */
    movl    $page_table_l3, %eax
    orl     $(PAGE_PRESENT | PAGE_WRITE), %eax
    movl    %eax, page_table_l4

    /* Set up PDP (Page Directory Pointer) entry 0 to point to PD */
    movl    $page_table_l2, %eax
    orl     $(PAGE_PRESENT | PAGE_WRITE), %eax
    movl    %eax, page_table_l3

    /* Set up PD (Page Directory) with 2MB huge pages, mapping first 1GB */
    movl    $(PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE), %eax
    movl    $page_table_l2, %edi
    movl    $512, %ecx

1:
    movl    %eax, (%edi)
    addl    $0x200000, %eax
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
    movl    $page_table_l4, %eax
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

    /* Set up 64-bit stack */
    movq    $0x70000, %rsp

    /* Set up IDT pointer (must be done before any interrupts) */
    movq    $idt64_pointer, %rax
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
