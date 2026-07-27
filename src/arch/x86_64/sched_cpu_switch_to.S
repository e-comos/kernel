.global cpu_switch_to
.text

# void cpu_switch_to(uintptr_t *old_sp, uintptr_t new_sp);
# %rdi = old_sp pointer
# %rsi = new_sp value
cpu_switch_to:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # Store current stack pointer into *old_sp (AT&T: source, destination)
    movq %rsp, (%rdi)

    # Load new stack pointer from new_sp
    movq %rsi, %rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret

.section .note.GNU-stack,"",@progbits
