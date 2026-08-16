/**
 * E-comOS Kernel - The Kernel of E-comOS Operating System
 * Copyright (C) 2025,2026 Saladin5101
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along this program.  If not, see <https://www.gnu.org/licenses />.
 */
#include <kernel/sched.h>
#include <kernel/mm.h>
#include <kernel/internal/types.h>
#include <kernel/printkit/print.h>
#include <klibc/string.h>

static Thread   threads[MAX_THREADS];
static uint32_t current_thread = 0;
static uint32_t next_thread_id  = 1;
static int      scheduler_initialized = 0;

extern void cpu_switch_to(uintptr_t *old_sp, uintptr_t new_sp);

static void thread_exit(void) {
    Thread *cur = sched_get_current_thread();
    if (!cur) {
        while (1) asm volatile("hlt");
    }
    cur->state = THREAD_TERMINATED;
    cur->block_reason = BLOCK_REASON_NONE;

    while (cur->ipc_count > 0) {
        ipc_message_t *msg = cur->ipc_queue[cur->ipc_head];
        if (msg) mm_free_page(msg);
        cur->ipc_head = (cur->ipc_head + 1) % IPC_MAX_QUEUE_SIZE;
        cur->ipc_count--;
    }
    sched_yield();
    while (1) asm volatile("hlt");
}

void sched_init(void) {
    memset(threads, 0, sizeof(threads));
    for (int i=0;i < MAX_THREADS; i++) {
        threads[i].state = THREAD_TERMINATED;
    }

    // Set up the kernel thread
    threads[0].id = next_thread_id++;
    threads[0].state = THREAD_RUNNING;
    threads[0].gs_base = 0;

    uintptr_t boot_rsp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(boot_rsp));
    threads[0].stack_ptr = boot_rsp;
    current_thread = 0;
    scheduler_initialized = 1;
}

int sched_create_thread(void (*entry_point)(void)) {
    if (!entry_point)
        return -1;

    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state == THREAD_TERMINATED) {
            void *stack = mm_alloc_page();
            if (!stack) {
                print_str("[SCHED] mm_alloc_page failed\n", 0x0F);
                return -1;
            }

            uintptr_t stack_top = (uintptr_t)stack + PAGE_SIZE;
            stack_top &= ~0xFULL;  // 16‑byte alignment

            // Push exit trampoline (will be popped when entry_point returns)
            stack_top -= sizeof(uint64_t);
            *(uint64_t *)stack_top = (uint64_t)(uintptr_t)thread_exit;

            // Push entry point (ret will pop this first)
            stack_top -= sizeof(uint64_t);
            *(uint64_t *)stack_top = (uint64_t)(uintptr_t)entry_point;

            // Now stack_top points to entry_point.
            // cpu_switch_to will pop 6 registers before ret, so we need 6 zero slots
            // below entry_point. The stack pointer passed to cpu_switch_to (new_sp)
            // must point to the first zero slot.
            uintptr_t new_sp = stack_top - 6 * sizeof(uint64_t);
            for (int r = 0; r < 6; r++)
                ((uint64_t *)(new_sp + r * sizeof(uint64_t)))[0] = 0;

            threads[i].stack_ptr = new_sp;
            threads[i].id = next_thread_id++;
            threads[i].priority = 1;
            threads[i].gs_base = 0;
            threads[i].ipc_head = 0;
            threads[i].ipc_tail = 0;
            threads[i].ipc_count = 0;
            for (int j = 0; j < IPC_MAX_QUEUE_SIZE; j++)
                threads[i].ipc_queue[j] = 0;

            threads[i].state = THREAD_READY;
            return (int)threads[i].id;
        }
    }
    print_str("[SCHED] Max thread capacity reached\n", 0x0F);
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
