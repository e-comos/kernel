/*
    E-comOS Kernel - Syscall handler
    Copyright (C) 2025,2026  Saladin5101

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <kernel/syscall.h>
#include <kernel/ipc.h>
#include <kernel/sched.h>
#include <kernel/mm.h>
#include <kernel/time.h>
#include <stdint.h>

#define MAX_IRQ_WAITERS 16
#define MAX_IRQS        16

typedef struct {
    uint32_t pid;
    uint8_t  irq_number;
    uint8_t  flags;
    uint32_t timeout_ms;
    uint64_t start_time;
    uint8_t  is_active;
} irq_waiter;

static irq_waiter     irq_waiters[MAX_IRQ_WAITERS];
static uint32_t      num_waiters = 0;
static volatile uint32_t irq_occurred[MAX_IRQS];
static volatile uint32_t irq_occurrence_count[MAX_IRQS];

void syscall_irq_init(void) {
    for (int i = 0; i < MAX_IRQ_WAITERS; i++) {
        irq_waiters[i].pid      = 0;
        irq_waiters[i].is_active = 0;
    }
    for (int i = 0; i < MAX_IRQS; i++) {
        irq_occurred[i]         = 0;
        irq_occurrence_count[i]  = 0;
    }
    num_waiters = 0;
}

static int find_irq_waiter(uint32_t pid, uint8_t irq_num) {
    for (uint32_t i = 0; i < num_waiters; i++) {
        if (irq_waiters[i].is_active &&
            irq_waiters[i].pid == pid &&
            irq_waiters[i].irq_number == irq_num)
            return (int)i;
    }
    return -1;
}

static int add_irq_waiter(uint32_t pid, uint8_t irq_num,
                        uint8_t flags, uint32_t timeout_ms) {
    if (find_irq_waiter(pid, irq_num) >= 0)
        return -2;
    for (uint32_t i = 0; i < MAX_IRQ_WAITERS; i++) {
        if (!irq_waiters[i].is_active) {
            irq_waiters[i].pid       = pid;
            irq_waiters[i].irq_number = irq_num;
            irq_waiters[i].flags     = flags;
            irq_waiters[i].timeout_ms = timeout_ms;
            irq_waiters[i].start_time = time_get_current_ms();
            irq_waiters[i].is_active  = 1;
            if (i >= num_waiters)
                num_waiters = i + 1;
            return 0;
        }
    }
    return -1;
}

static void remove_irq_waiter(uint32_t pid, uint8_t irq_num) {
    int idx = find_irq_waiter(pid, irq_num);
    if (idx >= 0)
        irq_waiters[idx].is_active = 0;
}

void syscall_irq_notify(uint8_t irq_num) {
    if (irq_num >= MAX_IRQS)
        return;
    irq_occurred[irq_num] = 1;
    irq_occurrence_count[irq_num]++;
    for (uint32_t i = 0; i < num_waiters; i++) {
        if (!irq_waiters[i].is_active) continue;
        if (irq_waiters[i].irq_number != irq_num) continue;
        irq_waiters[i].is_active = 0;
        Thread *t = sched_get_thread_by_pid(irq_waiters[i].pid);
        if (t && t->state == THREAD_BLOCKED) {
            t->state       = THREAD_READY;
            t->block_reason = BLOCK_REASON_NONE;
        }
    }
}

void syscall_irq_check_timeouts(void) {
    uint64_t now = time_get_current_ms();
    for (uint32_t i = 0; i < num_waiters; i++) {
        if (!irq_waiters[i].is_active) continue;
        if (irq_waiters[i].timeout_ms == 0) continue;
        uint64_t elapsed = now - irq_waiters[i].start_time;
        if (elapsed < irq_waiters[i].timeout_ms) continue;
        uint32_t pid = irq_waiters[i].pid;
        irq_waiters[i].is_active = 0;
        Thread *t = sched_get_thread_by_pid(pid);
        if (t && t->state == THREAD_BLOCKED) {
            t->state       = THREAD_READY;
            t->block_reason = BLOCK_REASON_NONE;
            t->last_error   = ERR_TIMEOUT;
        }
    }
}

static long irq_wait_syscall(uint8_t irq_num, uint8_t flags, uint32_t timeout_ms) {
    if (irq_num >= MAX_IRQS) return -1;
    uint32_t pid = sched_get_current_pid();
    if (pid == 0) return -4;
    if (irq_occurred[irq_num]) {
        if (flags & IRQ_WAIT_CLEAR)
            irq_occurred[irq_num] = 0;
        return 0;
    }
    if (flags & IRQ_WAIT_NOWAIT) return -2;
    int rc = add_irq_waiter(pid, irq_num, flags, timeout_ms);
    if (rc < 0) return rc;
    Thread *t = sched_get_current_thread();
    if (!t) {
        remove_irq_waiter(pid, irq_num);
        return -4;
    }
    t->state                = THREAD_BLOCKED;
    t->block_reason          = BLOCK_REASON_IRQ_WAIT;
    t->block_data.irq_num     = irq_num;
    while (!irq_occurred[irq_num]) {
        syscall_irq_check_timeouts();
        if (find_irq_waiter(pid, irq_num) < 0)
            break;
        sched_yield();
    }
    if (irq_occurred[irq_num] && (flags & IRQ_WAIT_CLEAR))
        irq_occurred[irq_num] = 0;
    remove_irq_waiter(pid, irq_num);
    if (t->last_error == ERR_TIMEOUT) {
        t->last_error = 0;
        return -3;
    }
    return 0;
}

long syscall_handler(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    switch (num) {
    case SYS_IPC_SEND:
        return ipc_send((thread_id)arg1, (ipc_message_t *)(uintptr_t)arg2);
    case SYS_IPC_RECEIVE:
        return ipc_receive((ipc_message_t *)(uintptr_t)arg1);
    case SYS_THREAD_YIELD:
        sched_yield();
        return 0;
    case SYS_ADDRESS_MAP:
        return mm_map_page(arg1, arg2, arg3);
    case SYS_IRQ_WAIT:
        return irq_wait_syscall((uint8_t)arg1, (uint8_t)arg2, arg3);
    case SYS_IRQ_GET_COUNT:
        if (arg1 >= MAX_IRQS) return -1;
        return (long)irq_occurrence_count[arg1];
    case SYS_IRQ_RESET_COUNT:
        if (arg1 >= MAX_IRQS) return -1;
        { uint32_t old = irq_occurrence_count[arg1]; irq_occurrence_count[arg1] = 0; return old; }
    case SYS_THREAD_CREATE:
        return sched_create_thread((void (*)(void))(uintptr_t)arg1);
    case SYS_MM_ALLOC_PAGES:
        return (long)(uintptr_t)mm_alloc_pages(arg1);
    case SYS_MM_FREE_PAGES:
        mm_free_pages((void *)(uintptr_t)arg1, arg2);
        return 0;
    case SYS_GET_MONOTIME:
        return (long)time_get_current_ms();
    case SYS_GETRANDOM: {
        /* Simple LCG seeded from monotonic time */
        uint8_t *buf = (uint8_t *)(uintptr_t)arg1;
        uint32_t len = arg2;
        if (!buf) return -1;
        static uint64_t rng_state = 0;
        if (!rng_state) rng_state = time_get_current_ms() ^ 0xDEADBEEFULL;
        for (uint32_t i = 0; i < len; i++) {
            rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
            buf[i] = (uint8_t)(rng_state >> 33);
        }
        return 0;
    }
    case SYS_INFO_SYS: {
        struct { char sysname[32]; char release[32]; char machine[32]; } *u =
            (void *)(uintptr_t)arg1;
        if (!u) return -1;
        const char *sn = "E-comOS", *rel = "0.1", *mach = "x86_64";
        for (int i = 0; sn[i]; i++)   u->sysname[i]  = sn[i];
        for (int i = 0; rel[i]; i++)  u->release[i]  = rel[i];
        for (int i = 0; mach[i]; i++) u->machine[i]  = mach[i];
        return 0;
    }
    /* SYS_SHM_OPEN, SYS_SHM_UNLINK, SYS_GETRLIMIT, SYS_SETRLIMIT,
       SYS_SLEEP_PRO, SYS_TIMER_CREATE, SYS_SETTIMEOFDAY, SYS_HLUD_REQUEST
       are delegated to user-space servers per SLFD Appendix D. */
    default:
        return -38; /* -ENOSYS */
    }
}
