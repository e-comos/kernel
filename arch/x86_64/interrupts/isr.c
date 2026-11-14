/* isr.c - Interrupt Service Routines for E-comOS Kernel
 *
 * Self-contained: no external headers beyond <stdint.h>.
 * No paging, no keyboard wait.	 Dumps all info then halts.
 *
 * Color scheme (VGA text-mode attribute bytes):
 *	 0x0A  green   - general-purpose registers, CS decode
 *	 0x0E  yellow  - segment and control registers
 *	 0x0F  white   - stack trace
 *	 0x0C  red	   - exception title, fatal halt message
 */

#include <stdint.h>

/* Verbose register dump switch.  The compact one-line summary below is
 * always printed; the 100-line dump is only compiled when this is 1. */
#ifndef EXC_VERBOSE_DUMP
#define EXC_VERBOSE_DUMP 0
#endif

/* Scheduler / panic entry points (declared here to keep this file
 * self-contained, matching the print_str declarations below). */
uint32_t sched_get_current_pid(void);
void sched_kill_current(void);
void kernel_panic(const char* msg);

/* ---------------------------------------------------------------------------
 * External print functions (implemented in src/printkit/print.c)
 * ---------------------------------------------------------------------------
 */
void print_str(const char* str, uint8_t color);
void print_hex64(uint64_t value, uint8_t color);
void print_num64(uint64_t value, uint8_t color);

/* ---------------------------------------------------------------------------
 * registers_t
 *
 * Field order MUST match the push sequence in isr.s SAVE_REGS + stub pushes.
 *
 * SAVE_REGS pushes (first push = highest address, last push -> RSP):
 *	 rax rbx rcx rdx rsi rdi rbp r8 r9 r10 r11 r12 r13 r14 r15
 * So from RSP upward:
 *	 r15 r14 r13 r12 r11 r10 r9 r8 rbp rdi rsi rdx rcx rbx rax
 * Then stub pushes:
 *	 int_no	 err_code
 * Then CPU-pushed frame:
 *	 rip  cs  rflags  rsp  ss
 * ---------------------------------------------------------------------------
 */
typedef struct {
	uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
	uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
	uint64_t int_no, err_code;
	uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) registers_t;

/* ---------------------------------------------------------------------------
 * Exception names (Intel SDM Vol.3, Table 6-1)
 * ---------------------------------------------------------------------------
 */
static const char* exc_names[32] = {
	"Divide Error",					  /*  0	 #DE */
	"Debug",						  /*  1	 #DB */
	"Non-Maskable Interrupt",		  /*  2	 NMI */
	"Breakpoint",					  /*  3	 #BP */
	"Overflow",						  /*  4	 #OF */
	"Bound Range Exceeded",			  /*  5	 #BR */
	"Invalid Opcode",				  /*  6	 #UD */
	"Device Not Available",			  /*  7	 #NM */
	"Double Fault",					  /*  8	 #DF */
	"Coprocessor Segment Overrun",	  /*  9		  */
	"Invalid TSS",					  /* 10	 #TS */
	"Segment Not Present",			  /* 11	 #NP */
	"Stack-Segment Fault",			  /* 12	 #SS */
	"General Protection Fault",		  /* 13	 #GP */
	"Page Fault",					  /* 14	 #PF */
	"Reserved",						  /* 15		  */
	"x87 Floating-Point Exception",	  /* 16	 #MF */
	"Alignment Check",				  /* 17	 #AC */
	"Machine Check",				  /* 18	 #MC */
	"SIMD Floating-Point Exception",  /* 19	 #XM */
	"Virtualization Exception",		  /* 20	 #VE */
	"Control-Protection Exception",	  /* 21	 #CP */
	"Reserved",						  /* 22		  */
	"Reserved",						  /* 23		  */
	"Reserved",						  /* 24		  */
	"Reserved",						  /* 25		  */
	"Reserved",						  /* 26		  */
	"Reserved",						  /* 27		  */
	"Hypervisor Injection Exception", /* 28	 #HV */
	"VMM Communication Exception",	  /* 29	 #VC */
	"Security Exception",			  /* 30	 #SX */
	"Reserved",						  /* 31		  */
};

/* ---------------------------------------------------------------------------
 * Inline CR readers
 * ---------------------------------------------------------------------------
 */
static inline uint64_t read_cr0(void) {
	uint64_t v;
	__asm__ volatile("movq %%cr0, %0" : "=r"(v));
	return v;
}
static inline uint64_t read_cr2(void) {
	uint64_t v;
	__asm__ volatile("movq %%cr2, %0" : "=r"(v));
	return v;
}
static inline uint64_t read_cr3(void) {
	uint64_t v;
	__asm__ volatile("movq %%cr3, %0" : "=r"(v));
	return v;
}
static inline uint64_t read_cr4(void) {
	uint64_t v;
	__asm__ volatile("movq %%cr4, %0" : "=r"(v));
	return v;
}

/* ---------------------------------------------------------------------------
 * Small formatting helpers
 * ---------------------------------------------------------------------------
 */
static void pr_reg(const char* name, uint64_t val, uint8_t c) {
	print_str(name, c);
	print_str(": 0x", c);
	print_hex64(val, c);
}

/* ---------------------------------------------------------------------------
 * decode_cs - print GDT selector fields and privilege level
 * ---------------------------------------------------------------------------
 */
static void decode_cs(uint64_t cs) {
	uint8_t c = 0x0A;
	print_str("CS Decode:\n", c);
	print_str("	 Raw CS: 0x", c);
	print_hex64(cs, c);
	print_str("\n  Index: ", c);
	print_num64((cs >> 3) & 0x1FFF, c);
	print_str("	 TI: ", c);
	print_str((cs & 0x04) ? "LDT" : "GDT", c);
	print_str("	 RPL: ", c);
	switch (cs) {
	case 0x08:
		print_str("Ring 0 (Kernel)", c);
		break;
	case 0x1B:
		print_str("Ring 3 (User)", c);
		break;
	default:
		print_str("Unknown selector", c);
		break;
	}
	print_str("\n", c);
}

/* ---------------------------------------------------------------------------
 * isr_handler - called from isr_common_stub
 *
 *	 rdi = registers_t*	 (RSP after SAVE_REGS)
 *
 * For fatal exceptions this function never returns (halts in place).
 * For #BP (3), #OF (4), and #PF (14) it returns so execution can resume.
 * ---------------------------------------------------------------------------
 */
/* ---------------------------------------------------------------------------
 * Tiny string builder used for the panic message.
 *
 * Assembled by hand rather than with snprintf() so the panic path has no
 * dependency on the printf implementation (it used to be limited to
 * %d %s %c %x, and klibc's stdarg walked the stack by hand).
 * ---------------------------------------------------------------------------
 */
static int fmt_putc(char* buf, int cap, int pos, char c) {
	if (pos < cap - 1)
		buf[pos] = c;
	return pos + 1;
}

static int fmt_str(char* buf, int cap, int pos, const char* s) {
	while (*s)
		pos = fmt_putc(buf, cap, pos, *s++);
	return pos;
}

static int fmt_hex64(char* buf, int cap, int pos, uint64_t v) {
	char t[16];
	int i = 0;
	do {
		t[i++] = "0123456789abcdef"[v & 0xF];
		v >>= 4;
	} while (v);
	pos = fmt_str(buf, cap, pos, "0x");
	while (i)
		pos = fmt_putc(buf, cap, pos, t[--i]);
	return pos;
}

static int fmt_dec(char* buf, int cap, int pos, uint32_t v) {
	char t[10];
	int i = 0;
	do {
		t[i++] = '0' + (v % 10);
		v /= 10;
	} while (v);
	while (i)
		pos = fmt_putc(buf, cap, pos, t[--i]);
	return pos;
}

void isr_handler(registers_t* regs) {
	/* ---- Page Fault handling (vector 14) --------------------------------- */
	if (regs->int_no == 14) {
		extern int handle_page_fault(uint64_t fault_addr, uint64_t error_code);
		uint64_t fault_addr = read_cr2();

		if (handle_page_fault(fault_addr, regs->err_code)) {
			/* Page fault handled successfully - return to resume execution */
			return;
		}
		/* Page fault not handled - fall through to fatal error */
	}

	/* ---- Compact summary: always printed ------------------------------- */
	const int from_user = (regs->cs & 0x3) == 0x3;
	const uint32_t pid = sched_get_current_pid();
	const char* name =
		(regs->int_no < 32) ? exc_names[regs->int_no] : "Unknown Exception";

	print_str("\n[EXC] ", 0x0C);
	print_str(name, 0x0C);
	print_str("\tpid=", 0x0C);
	print_num64(pid, 0x0C);
	print_str("\terr=0x", 0x0C);
	print_hex64(regs->err_code, 0x0C);
	print_str("\tRIP=0x", 0x0C);
	print_hex64(regs->rip, 0x0C);
	print_str(from_user ? "\t(ring 3)\n" : "\t(ring 0)\n", 0x0C);

	/* ---- Verbose register dump (compile-time switch) -------------------- */
	if (EXC_VERBOSE_DUMP) {
	/* ---- Error Code + RFLAGS header (red) -------------------------------- */
	print_str("Error Code: 0x", 0x0C);
	print_hex64(regs->err_code, 0x0C);
	print_str("	   RFLAGS: 0x", 0x0C);
	print_hex64(regs->rflags, 0x0C);
	print_str("\n", 0x0C);

	/* ---- Enhanced debug dump banner (green) ------------------------------ */
	print_str("=== ENHANCED DEBUG DUMP ===\n", 0x0A);

	pr_reg("RIP", regs->rip, 0x0A);
	print_str("	 ", 0x0A);
	pr_reg("CS", regs->cs, 0x0A);
	print_str("\n", 0x0A);

	/* Only treat the saved RSP/SS as valid if the interrupted context
	 * was in user mode (RPL == 3). If the interrupt originated in
	 * kernel mode, the CPU does not push RSP/SS and those fields in
	 * the saved structure are not valid pointers but leftover memory.
	 */
	int interrupted_in_user = (regs->cs & 0x3) == 0x3;

	pr_reg("RSP", regs->rsp, 0x0A);
	print_str("	 ", 0x0A);
	pr_reg("SS", regs->ss, 0x0A);
	if (!interrupted_in_user) {
		print_str("	 (no user stack - kernel mode interrupt)", 0x0A);
	}
	print_str("\n", 0x0A);

	/* ---- General-purpose registers (green) ------------------------------- */
	print_str("\nGeneral Purpose Registers:\n", 0x0A);

	pr_reg("RAX", regs->rax, 0x0A);
	print_str("	 ", 0x0A);
	pr_reg("RBX", regs->rbx, 0x0A);
	print_str("	 ", 0x0A);
	pr_reg("RCX", regs->rcx, 0x0A);
	print_str("	 ", 0x0A);
	pr_reg("RDX", regs->rdx, 0x0A);
	print_str("\n", 0x0A);

	pr_reg("RSI", regs->rsi, 0x0A);
	print_str("	 ", 0x0A);
	pr_reg("RDI", regs->rdi, 0x0A);
	print_str("	 ", 0x0A);
	pr_reg("RBP", regs->rbp, 0x0A);
	print_str("	 ", 0x0A);
	pr_reg("R8 ", regs->r8, 0x0A);
	print_str("\n", 0x0A);

	pr_reg("R9 ", regs->r9, 0x0A);
	print_str("	 ", 0x0A);
	pr_reg("R10", regs->r10, 0x0A);
	print_str("	 ", 0x0A);
	pr_reg("R11", regs->r11, 0x0A);
	print_str("	 ", 0x0A);
	pr_reg("R12", regs->r12, 0x0A);
	print_str("\n", 0x0A);

	pr_reg("R13", regs->r13, 0x0A);
	print_str("	 ", 0x0A);
	pr_reg("R14", regs->r14, 0x0A);
	print_str("	 ", 0x0A);
	pr_reg("R15", regs->r15, 0x0A);
	print_str("\n", 0x0A);

	/* ---- Segment registers (yellow) -------------------------------------- */
	print_str("\nSegment Registers:\n", 0x0E);

	/* DS/ES/FS/GS are not in registers_t: the stub overwrites them with 0x10
	 * before calling us.  Read the current (kernel) values live. */
	uint64_t ds, es, fs, gs;
	__asm__ volatile("xorq %0,%0; movw %%ds,%%ax" : "=a"(ds));
	__asm__ volatile("xorq %0,%0; movw %%es,%%ax" : "=a"(es));
	__asm__ volatile("xorq %0,%0; movw %%fs,%%ax" : "=a"(fs));
	__asm__ volatile("xorq %0,%0; movw %%gs,%%ax" : "=a"(gs));

	pr_reg("DS", ds, 0x0E);
	print_str("	 ", 0x0E);
	pr_reg("ES", es, 0x0E);
	print_str("	 ", 0x0E);
	pr_reg("FS", fs, 0x0E);
	print_str("	 ", 0x0E);
	pr_reg("GS", gs, 0x0E);
	print_str("\n", 0x0E);

	/* ---- Control registers (yellow) -------------------------------------- */
	print_str("\nControl Registers:\n", 0x0E);

	pr_reg("CR0", read_cr0(), 0x0E);
	print_str("	 ", 0x0E);
	pr_reg("CR2", read_cr2(), 0x0E);
	print_str("\n", 0x0E);
	pr_reg("CR3", read_cr3(), 0x0E);
	print_str("	 ", 0x0E);
	pr_reg("CR4", read_cr4(), 0x0E);
	print_str("\n", 0x0E);

	/* ---- Stack near RSP, first 8 entries (white) ------------------------- */
	print_str("\nStack near RSP (first 8 entries):\n", 0x0F);
	if (interrupted_in_user) {
		/* Validate user stack pointer before dereferencing it. */
		if (regs->rsp >= 0x70000 && regs->rsp < 0x80000) {
			uint64_t* stack = (uint64_t*)(uintptr_t)regs->rsp;
			for (int i = 0; i < 8; i++) {
				print_str("	 [RSP+", 0x0F);
				print_num64((uint64_t)(i * 8), 0x0F);
				print_str("] 0x", 0x0F);
				print_hex64(stack[i], 0x0F);
				print_str("\n", 0x0F);
			}
		} else {
			print_str("	 (invalid user stack pointer, skipped)\n", 0x0F);
		}
	} else {
		print_str("	 (skipped - interrupt came from kernel mode)\n", 0x0F);
	}

	/* ---- CS decode (green) ----------------------------------------------- */
	print_str("\n", 0x0A);
	decode_cs(regs->cs);
	}

	/* ---- Resumable vectors --------------------------------------------- */
	if (regs->int_no == 3 || regs->int_no == 4) {
		return;
	}

	/* ---- Fatal: decide what to do with it ------------------------------ */
	char msg[176];
	int mp = 0;
	const int mcap = (int)sizeof(msg);
	mp = fmt_str(msg, mcap, mp, name);
	mp = fmt_str(msg, mcap, mp, " at RIP=");
	mp = fmt_hex64(msg, mcap, mp, regs->rip);
	mp = fmt_str(msg, mcap, mp, " err=");
	mp = fmt_hex64(msg, mcap, mp, regs->err_code);
	mp = fmt_str(msg, mcap, mp, " pid=");
	mp = fmt_dec(msg, mcap, mp, pid);
	mp = fmt_str(msg, mcap, mp, from_user ? " ring3" : " ring0");
	if (mp > mcap - 1)
		mp = mcap - 1;
	msg[mp] = '\0';

	/* A user thread other than init can simply be killed: the scheduler
	 * has somewhere else to go. */
	if (from_user && pid != 1) {
		sched_kill_current();
		kernel_panic("sched_kill_current returned with no runnable thread");
	}

	/* init (pid 1) is the only user process, so losing it means there is
	 * nothing left to run -- that is a system-level failure, not a
	 * recoverable one.  Kernel faults are fatal as well. */
	kernel_panic(msg);
}
