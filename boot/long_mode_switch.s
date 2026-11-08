/* ===================================================================
 * kernel/boot/long_mode_switch.s
 *
 * Long Mode Switch Routine (GAS/AT&T Syntax)
 * Switches from 32-bit protected mode to 64-bit long mode.
 * =================================================================== */

.code32

.global switch_to_long_mode
.global check_long_mode
.global setup_page_tables
.global enable_paging

.extern kernel_main

/* ==================== Constants ==================== */
.set PAGE_PRESENT,	 0x01
.set PAGE_WRITE,	 0x02
.set PAGE_HUGE,		 0x80		/* 2 MB huge page (PDE.PS) */
.set KERNEL_HIGH_BASE, 0xffff800000000000

.set CR0_PG,		 0x80000000
.set CR4_PAE,		 0x20
.set EFER_MSR,		 0xC0000080
.set EFER_LME,		 0x100

/* Page-table pages: physical, used only before/while paging comes on */
.set PAGE_TABLE_L4_PHYS, 0x200000
.set PAGE_TABLE_L3_PHYS, 0x201000
.set PAGE_TABLE_L2_PHYS, 0x202000
.set SAVED_MULTIBOOT_INFO_PHYS, 0x300000

/* Temporary GDT lives inside the identity-mapped 0x200000..0x3FFFFF block,
 * but clear of the table pages (0x200000..0x202FFF). */
.set TMP_GDT_PHYS,		 0x205000
.set TMP_GDTR_PHYS,		 0x205020

/* ==================== Switch Function ==================== */
switch_to_long_mode:
	pushl	%ebx
	pushl	%ecx
	pushl	%edx


	call	check_long_mode
	testl	%eax, %eax
	jz		.no_long_mode_error

	movl	%ebx, %esi
	call	setup_page_tables
	call	enable_paging

	/* Mask PIC */
	movb	$0xff, %al
	outb	%al, $0x21
	outb	%al, $0xa1

	/* Build temporary GDT in identity-mapped low memory.
	 *	 [0x205000 + 0x00] null
	 *	 [0x205000 + 0x08] 64-bit code (sel 0x08)
	 *	 [0x205000 + 0x10] data		   (sel 0x10)			   */
	movl	$TMP_GDT_PHYS, %edi
	movl	$0x0, 0(%edi)
	movl	$0x0, 4(%edi)
	movl	$0x0, 8(%edi)
	movl	$0x00209A00, 12(%edi)	   /* L=1, 64-bit code */
	movl	$0x0, 16(%edi)
	movl	$0x00009200, 20(%edi)

	movw	$0x17, TMP_GDTR_PHYS	   /* limit 23 = 3 descriptors */
	movl	$TMP_GDT_PHYS, TMP_GDTR_PHYS + 2

	lgdt	TMP_GDTR_PHYS

	/* CORRECT far jump: 32-bit mode, 32-bit PHYSICAL offset.
	 * GAS encodes this as EA <off32> <sel16> = EA .. .. .. .. 08 00.
	 * The physical address of long_mode_jump == its link-time high-half
	 * address minus KERNEL_HIGH_BASE. */
	ljmp	$0x08, $(long_mode_jump - KERNEL_HIGH_BASE)

	jmp		.hang

.no_long_mode_error:
	movl	$0x4f204f4e, 0xb8000
	movl	$0x4f204f4c, 0xb8004

.hang:
	cli
1:	hlt
	jmp 1b

/* ==================== Check Long Mode Support ==================== */
check_long_mode:
	pushfl
	popl	%eax
	movl	%eax, %ecx
	xorl	$0x200000, %eax
	pushl	%eax
	popfl
	pushfl
	popl	%eax
	pushl	%ecx
	popfl
	xorl	%ecx, %eax
	andl	$0x200000, %eax
	jz		.no_cpuid

	movl	$0x80000000, %eax
	cpuid
	cmpl	$0x80000001, %eax
	jb		.no_long_mode

	movl	$0x80000001, %eax
	cpuid
	testl	$(1 << 29), %edx
	jz		.no_long_mode

	movl	$1, %eax
	ret

.no_cpuid:
.no_long_mode:
	xorl	%eax, %eax
	ret

/* ==================== Set Up Page Tables ====================
 * With PAE + long mode we use 2 MB huge pages (PDE.PS=1).
 * PD[0..7] identity-map physical 0..16 MB; PML4[256] mirrors the same
 * tables for the high half 0xffff800000000000..
 * ==================================================================== */
setup_page_tables:
	/* Clear PML4, PDPT, PD (3 pages = 3*1024 dwords). */
	movl	$PAGE_TABLE_L4_PHYS, %edi
	movl	$3072, %ecx
	xorl	%eax, %eax
	rep; stosl

	/* PML4[0] -> PDPT (low identity) */
	movl	$PAGE_TABLE_L3_PHYS, %eax
	orl		$(PAGE_PRESENT + PAGE_WRITE), %eax
	movl	%eax, PAGE_TABLE_L4_PHYS
	movl	$0x0, PAGE_TABLE_L4_PHYS + 4

	/* PML4[256] -> PDPT (high-half mirror) */
	movl	%eax, PAGE_TABLE_L4_PHYS + 256*8
	movl	$0x0, PAGE_TABLE_L4_PHYS + 256*8 + 4

	/* PDPT[0] -> PD */
	movl	$PAGE_TABLE_L2_PHYS, %eax
	orl		$(PAGE_PRESENT + PAGE_WRITE), %eax
	movl	%eax, PAGE_TABLE_L3_PHYS
	movl	$0x0, PAGE_TABLE_L3_PHYS + 4

	/* PD[0..7] = eight 2 MB huge identity entries, 0..16 MB */
	movl	$PAGE_TABLE_L2_PHYS, %edi
	movl	$0x0, %ebx				   /* physical base of first 2 MB */
	movl	$8, %ecx
1:
	movl	%ebx, %eax
	orl		$(PAGE_PRESENT + PAGE_WRITE + PAGE_HUGE), %eax
	movl	%eax, (%edi)
	movl	$0x0, 4(%edi)
	addl	$0x200000, %ebx			   /* next 2 MB */
	addl	$8, %edi
	loop	1b

	ret

/* ==================== Enable Paging ==================== */
enable_paging:
	movl	%cr4, %eax
	orl		$CR4_PAE, %eax
	movl	%eax, %cr4

	movl	$PAGE_TABLE_L4_PHYS, %eax
	movl	%eax, %cr3

	movl	$EFER_MSR, %ecx
	rdmsr
	orl		$EFER_LME, %eax
	wrmsr

	movl	%cr0, %eax
	orl		$CR0_PG, %eax
	movl	%eax, %cr0
	ret

/* ==================== 64-bit Code ==================== */
.code64

/* We land HERE at its PHYSICAL (low, identity) address. No RIP-relative
 * memory references are allowed below until we've relocated to high half.
 */
long_mode_jump:
	movq	%rsi, %rdi
	movw	$0x10, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss

	/* 64-bit kernel stack, inside the high-half window (phys 0x190000). */
	movq	$KERNEL_HIGH_BASE + 0x100000 + 0x90000, %rsp

	/* Relocate RIP to the high-half virtual address. The immediate is a
	 * link-time constant (no RIP involvement), so this works from low RIP. */
	movabs	$long_mode_jump_high, %rax
	jmp		*%rax

long_mode_jump_high:
	/* Now RIP is high-half; RIP-relative references resolve correctly. */
	movl	$SAVED_MULTIBOOT_INFO_PHYS, multiboot2_info_phys(%rip)

	leaq	idt64_pointer(%rip), %rax
	lidt	(%rax)

	call	kernel_main

	cli
1:	hlt
	jmp 1b

/* ==================== Unused static PT pages (harmless) ==================== */
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

/* ==================== Minimal early IDT (placeholder) ==================== */
.section .rodata
.align 8
idt64:
	.rept 256
	.word 0
	.word 0x08
	.byte 0
	.byte 0x8E
	.word 0
	.long 0
	.long 0
	.endr
idt64_pointer:
	.word . - idt64 - 1
	.quad idt64

/* ==================== Read-only GDT (loaded later by gdt_init) ========= */
.section .rodata
.align 8
gdt64:
	.quad 0x0000000000000000
	.quad 0x00209A0000000000
	.quad 0x0000920000000000

gdt64_pointer:
	.word	. - gdt64 - 1
	.quad	gdt64

