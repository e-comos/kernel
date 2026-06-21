/* isr.c - Interrupt Service Routines for E-comOS Kernel
 *
 * Self-contained: no external headers beyond <stdint.h>.
 * No paging, no keyboard wait.  Dumps all info then halts.
 *
 * Color scheme (VGA text-mode attribute bytes):
 *   0x0A  green   - general-purpose registers, CS decode
 *   0x0E  yellow  - segment and control registers
 *   0x0F  white   - stack trace
 *   0x0C  red     - exception title, fatal halt message
 */

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * External print functions (implemented in src/printkit/print.c)
 * --------------------------------------------------------------------------- */
void print_str(const char *str, uint8_t color);
void print_hex64(uint64_t value, uint8_t color);
void print_num64(uint64_t value, uint8_t color);

/* ---------------------------------------------------------------------------
 * registers_t
 *
 * Field order MUST match the push sequence in isr.s SAVE_REGS + stub pushes.
 *
 * SAVE_REGS pushes (first push = highest address, last push -> RSP):
 *   rax rbx rcx rdx rsi rdi rbp r8 r9 r10 r11 r12 r13 r14 r15
 * So from RSP upward:
 *   r15 r14 r13 r12 r11 r10 r9 r8 rbp rdi rsi rdx rcx rbx rax
 * Then stub pushes:
 *   int_no  err_code
 * Then CPU-pushed frame:
 *   rip  cs  rflags  rsp  ss
 * --------------------------------------------------------------------------- */
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) registers_t;

/* ---------------------------------------------------------------------------
 * Exception names (Intel SDM Vol.3, Table 6-1)
 * --------------------------------------------------------------------------- */
static const char *exc_names[32] = {
    "Divide Error",                    /*  0  #DE */
    "Debug",                           /*  1  #DB */
    "Non-Maskable Interrupt",          /*  2  NMI */
    "Breakpoint",                      /*  3  #BP */
    "Overflow",                        /*  4  #OF */
    "Bound Range Exceeded",            /*  5  #BR */
    "Invalid Opcode",                  /*  6  #UD */
    "Device Not Available",            /*  7  #NM */
    "Double Fault",                    /*  8  #DF */
    "Coprocessor Segment Overrun",     /*  9       */
    "Invalid TSS",                     /* 10  #TS */
    "Segment Not Present",             /* 11  #NP */
    "Stack-Segment Fault",             /* 12  #SS */
    "General Protection Fault",        /* 13  #GP */
    "Page Fault",                      /* 14  #PF */
    "Reserved",                        /* 15       */
    "x87 Floating-Point Exception",    /* 16  #MF */
    "Alignment Check",                 /* 17  #AC */
    "Machine Check",                   /* 18  #MC */
    "SIMD Floating-Point Exception",   /* 19  #XM */
    "Virtualization Exception",        /* 20  #VE */
    "Control-Protection Exception",    /* 21  #CP */
    "Reserved",                        /* 22       */
    "Reserved",                        /* 23       */
    "Reserved",                        /* 24       */
    "Reserved",                        /* 25       */
    "Reserved",                        /* 26       */
    "Reserved",                        /* 27       */
    "Hypervisor Injection Exception",  /* 28  #HV */
    "VMM Communication Exception",     /* 29  #VC */
    "Security Exception",              /* 30  #SX */
    "Reserved",                        /* 31       */
};

/* ---------------------------------------------------------------------------
 * Inline CR readers
 * --------------------------------------------------------------------------- */
static inline uint64_t read_cr0(void)
{
    uint64_t v;
    __asm__ volatile("movq %%cr0, %0" : "=r"(v));
    return v;
}
static inline uint64_t read_cr2(void)
{
    uint64_t v;
    __asm__ volatile("movq %%cr2, %0" : "=r"(v));
    return v;
}
static inline uint64_t read_cr3(void)
{
    uint64_t v;
    __asm__ volatile("movq %%cr3, %0" : "=r"(v));
    return v;
}
static inline uint64_t read_cr4(void)
{
    uint64_t v;
    __asm__ volatile("movq %%cr4, %0" : "=r"(v));
    return v;
}

/* ---------------------------------------------------------------------------
 * Small formatting helpers
 * --------------------------------------------------------------------------- */
static void pr_reg(const char *name, uint64_t val, uint8_t c)
{
    print_str(name, c);
    print_str(": 0x", c);
    print_hex64(val, c);
}

/* ---------------------------------------------------------------------------
 * decode_cs - print GDT selector fields and privilege level
 * --------------------------------------------------------------------------- */
static void decode_cs(uint64_t cs)
{
    uint8_t c = 0x0A;
    print_str("CS Decode:\n", c);
    print_str("  Raw CS: 0x", c);
    print_hex64(cs, c);
    print_str("\n  Index: ", c);
    print_num64((cs >> 3) & 0x1FFF, c);
    print_str("  TI: ", c);
    print_str((cs & 0x04) ? "LDT" : "GDT", c);
    print_str("  RPL: ", c);
    switch (cs) {
    case 0x08: print_str("Ring 0 (Kernel)", c); break;
    case 0x1B: print_str("Ring 3 (User)",   c); break;
    default:   print_str("Unknown selector", c); break;
    }
    print_str("\n", c);
}

/* ---------------------------------------------------------------------------
 * isr_handler - called from isr_common_stub
 *
 *   rdi = registers_t*  (RSP after SAVE_REGS)
 *
 * For fatal exceptions this function never returns (halts in place).
 * For #BP (3) and #OF (4) it returns so execution can resume.
 * --------------------------------------------------------------------------- */
void isr_handler(registers_t *regs)
{
    /* ---- Error Code + RFLAGS header (red) -------------------------------- */
    print_str("Error Code: 0x", 0x0C);
    print_hex64(regs->err_code, 0x0C);
    print_str("    RFLAGS: 0x", 0x0C);
    print_hex64(regs->rflags, 0x0C);
    print_str("\n", 0x0C);

    /* ---- Enhanced debug dump banner (green) ------------------------------ */
    print_str("=== ENHANCED DEBUG DUMP ===\n", 0x0A);

    pr_reg("RIP", regs->rip, 0x0A);
    print_str("  ", 0x0A);
    pr_reg("CS", regs->cs, 0x0A);
    print_str("\n", 0x0A);

    pr_reg("RSP", regs->rsp, 0x0A);
    print_str("  ", 0x0A);
    pr_reg("SS", regs->ss, 0x0A);
    print_str("\n", 0x0A);

    /* ---- General-purpose registers (green) ------------------------------- */
    print_str("\nGeneral Purpose Registers:\n", 0x0A);

    pr_reg("RAX", regs->rax, 0x0A); print_str("  ", 0x0A);
    pr_reg("RBX", regs->rbx, 0x0A); print_str("  ", 0x0A);
    pr_reg("RCX", regs->rcx, 0x0A); print_str("  ", 0x0A);
    pr_reg("RDX", regs->rdx, 0x0A); print_str("\n", 0x0A);

    pr_reg("RSI", regs->rsi, 0x0A); print_str("  ", 0x0A);
    pr_reg("RDI", regs->rdi, 0x0A); print_str("  ", 0x0A);
    pr_reg("RBP", regs->rbp, 0x0A); print_str("  ", 0x0A);
    pr_reg("R8 ", regs->r8,  0x0A); print_str("\n", 0x0A);

    pr_reg("R9 ", regs->r9,  0x0A); print_str("  ", 0x0A);
    pr_reg("R10", regs->r10, 0x0A); print_str("  ", 0x0A);
    pr_reg("R11", regs->r11, 0x0A); print_str("  ", 0x0A);
    pr_reg("R12", regs->r12, 0x0A); print_str("\n", 0x0A);

    pr_reg("R13", regs->r13, 0x0A); print_str("  ", 0x0A);
    pr_reg("R14", regs->r14, 0x0A); print_str("  ", 0x0A);
    pr_reg("R15", regs->r15, 0x0A); print_str("\n", 0x0A);

    /* ---- Segment registers (yellow) -------------------------------------- */
    print_str("\nSegment Registers:\n", 0x0E);

    /* DS/ES/FS/GS are not in registers_t: the stub overwrites them with 0x10
     * before calling us.  Read the current (kernel) values live. */
    uint64_t ds, es, fs, gs;
    __asm__ volatile("xorq %0,%0; movw %%ds,%%ax" : "=a"(ds));
    __asm__ volatile("xorq %0,%0; movw %%es,%%ax" : "=a"(es));
    __asm__ volatile("xorq %0,%0; movw %%fs,%%ax" : "=a"(fs));
    __asm__ volatile("xorq %0,%0; movw %%gs,%%ax" : "=a"(gs));

    pr_reg("DS", ds, 0x0E); print_str("  ", 0x0E);
    pr_reg("ES", es, 0x0E); print_str("  ", 0x0E);
    pr_reg("FS", fs, 0x0E); print_str("  ", 0x0E);
    pr_reg("GS", gs, 0x0E); print_str("\n", 0x0E);

    /* ---- Control registers (yellow) -------------------------------------- */
    print_str("\nControl Registers:\n", 0x0E);

    pr_reg("CR0", read_cr0(), 0x0E); print_str("  ", 0x0E);
    pr_reg("CR2", read_cr2(), 0x0E); print_str("\n", 0x0E);
    pr_reg("CR3", read_cr3(), 0x0E); print_str("  ", 0x0E);
    pr_reg("CR4", read_cr4(), 0x0E); print_str("\n", 0x0E);

    /* ---- Stack near RSP, first 8 entries (white) ------------------------- */
    print_str("\nStack near RSP (first 8 entries):\n", 0x0F);
    uint64_t *stack = (uint64_t *)(uintptr_t)regs->rsp;
    for (int i = 0; i < 8; i++) {
        print_str("  [RSP+", 0x0F);
        print_num64((uint64_t)(i * 8), 0x0F);
        print_str("] 0x", 0x0F);
        print_hex64(stack[i], 0x0F);
        print_str("\n", 0x0F);
    }

    /* ---- CS decode (green) ----------------------------------------------- */
    print_str("\n", 0x0A);
    decode_cs(regs->cs);

    /* ---- Fatal halt message (red) ---------------------------------------- */
    print_str("\n=======================\n", 0x0C);
    if (regs->int_no < 32) {
        print_str(exc_names[regs->int_no], 0x0C);
    } else {
        print_str("Unknown Exception", 0x0C);
    }
    print_str(". System will halt.\n", 0x0C);
    print_str("=======================\n", 0x0C);

    /* Non-fatal vectors: return so the stub can iretq */
    if (regs->int_no == 3 || regs->int_no == 4) {
        return;
    }

    /* Fatal: halt here */
    while (1) {
        __asm__ volatile("hlt");
    }
}