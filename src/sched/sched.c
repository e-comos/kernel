/*
    E-comOS Kernel - Scheduler
    Copyright (C) 2025,2026  Saladin5101

    Invariant: threads[current_thread].state == THREAD_RUNNING
               at all times after the first sched_schedule() call.
*/
#include <kernel/sched.h>
#include <kernel/mm.h>
#include <kernel/internal/types.h>
#include <kernel/printkit/print.h>
static Thread   threads[MAX_THREADS];
static uint32_t current_thread = 0;
static uint32_t next_thread_id  = 1;
static int      scheduler_initialized = 0;

extern void cpu_switch_to(uintptr_t *old_sp, uintptr_t new_sp);

void sched_init(void) {
    for (int i = 0; i < MAX_THREADS; i++) {
        threads[i].id = 0;
        threads[i].state = THREAD_TERMINATED;
        threads[i].stack_ptr = 0;
    }
    threads[0].id = next_thread_id++;
    threads[0].state = THREAD_RUNNING;
    current_thread = 0;
    scheduler_initialized = 1;
}

int sched_create_thread(void (*entry_point)(void)) {
    if (!entry_point)
        return -1;

    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state == THREAD_TERMINATED || threads[i].id == 0) {
            
            // 1. Allocate first before touching the slot state
            void *stack = mm_alloc_page();
            if (!stack) {
                print_str("[SCHED] Error: mm_alloc_page failed for thread creation!\n", 0x0F);
				return -1;
            }

            // 2. Only populate fields if allocation succeeded
            uintptr_t stack_top = (uintptr_t)stack + PAGE_SIZE;
            
            stack_top -= sizeof(uint64_t);
            *(uint64_t *)stack_top = (uint64_t)(uintptr_t)entry_point;

            stack_top -= 6 * sizeof(uint64_t);
            for (int r = 0; r < 6; r++) {
                ((uint64_t *)stack_top)[r] = 0;
            }

            threads[i].stack_ptr = stack_top;
            threads[i].id        = next_thread_id++;
            threads[i].priority  = 1;
            threads[i].block_reason = 0;
            threads[i].last_error   = 0;
            
            // 3. Mark READY last so it is never picked up half-baked
            threads[i].state     = THREAD_READY;

            return (int)threads[i].id;
        }
    }
    return -1;
}

void sched_yield(void) {
    if (!scheduler_initialized)
        return;
    sched_schedule();
}

void sched_schedule(void) {
    if (!scheduler_initialized)
        return;

    uint32_t next = (current_thread + 1) % MAX_THREADS;
    uint32_t found = current_thread;

    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[next].state == THREAD_READY && threads[next].id != 0) {
            found = next;
            break;
        }
        next = (next + 1) % MAX_THREADS;
    }

    if (found == current_thread)
        return;

    uint32_t prev = current_thread;
    current_thread = found;

    if (threads[prev].state == THREAD_RUNNING) {
        threads[prev].state = THREAD_READY;
    }
    threads[current_thread].state = THREAD_RUNNING;

    cpu_switch_to(&threads[prev].stack_ptr, threads[current_thread].stack_ptr);
}

Thread *sched_get_thread_by_pid(uint32_t pid) {
    for (int i = 0; i < MAX_THREADS; i++)
        if (threads[i].id == pid)
            return &threads[i];
    return 0;
}

uint32_t sched_get_current_pid(void) {
    return threads[current_thread].id;
}

Thread *sched_get_current_thread(void) {
    return &threads[current_thread];
}
