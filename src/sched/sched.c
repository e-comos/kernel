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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along this program.	If not, see <https://www.gnu.org/licenses />.
 */
#include <kernel/internal/types.h>
#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/percpu.h>
#include <kernel/printkit/print.h>
#include <kernel/sched.h>
#include <klibc/string.h>
#include <user_space/user_mode.h>

static Thread threads[MAX_THREADS];
static uint32_t current_thread = 0;
static uint32_t next_thread_id = 1;
static int scheduler_initialized = 0;

extern void cpu_switch_to(uintptr_t* old_sp, uintptr_t new_sp);

static void thread_exit(void);

static void user_thread_start(void) {
	Thread* thread = sched_get_current_thread();
	if (!thread)
		while (1)
			asm volatile("hlt");
	switch_to_user_mode(thread->user_entry, thread->user_rsp);
}

static int create_thread_context(void (*entry_point)(void), uint32_t stack_pages,
								 uintptr_t user_entry, uintptr_t user_rsp) {
	for (int i = 0; i < MAX_THREADS; i++) {
		if (threads[i].state != THREAD_TERMINATED)
			continue;

		void* stack = mm_alloc_pages(stack_pages);
		if (!stack) {
			print_str("[SCHED] mm_alloc_pages failed\n", 0x0F);
			return -1;
		}

		Thread* thread = &threads[i];
		uintptr_t stack_top = (uintptr_t)stack + (uintptr_t)stack_pages * PAGE_SIZE;
		stack_top &= ~0xFULL;
		thread->syscall_stack_top = stack_top;

		stack_top -= sizeof(uint64_t);
		*(uint64_t*)stack_top = (uint64_t)(uintptr_t)thread_exit;
		stack_top -= sizeof(uint64_t);
		*(uint64_t*)stack_top = (uint64_t)(uintptr_t)entry_point;

		uintptr_t new_sp = stack_top - 6 * sizeof(uint64_t);
		for (int r = 0; r < 6; r++)
			((uint64_t*)(new_sp + r * sizeof(uint64_t)))[0] = 0;

		thread->stack_ptr = new_sp;
		thread->id = next_thread_id++;
		thread->priority = 1;
		thread->gs_base = 0;
		thread->user_entry = user_entry;
		thread->user_rsp = user_rsp;
		thread->syscall_active = 0;
		thread->ipc_head = 0;
		thread->ipc_tail = 0;
		thread->ipc_count = 0;
		for (int j = 0; j < IPC_MAX_QUEUE_SIZE; j++)
			thread->ipc_queue[j] = 0;

		thread->state = THREAD_READY;
		return (int)thread->id;
	}
	print_str("[SCHED] Max thread capacity reached\n", 0x0F);
	return -1;
}

static void thread_exit(void) {
	Thread* cur = sched_get_current_thread();
	if (!cur) {
		while (1)
			asm volatile("hlt");
	}
	cur->state = THREAD_TERMINATED;
	cur->block_reason = BLOCK_REASON_NONE;

	while (cur->ipc_count > 0) {
		ipc_message_t* msg = cur->ipc_queue[cur->ipc_head];
		if (msg)
			ipc_message_free(msg);
		cur->ipc_head = (cur->ipc_head + 1) % IPC_MAX_QUEUE_SIZE;
		cur->ipc_count--;
	}
	sched_yield();
	while (1)
		asm volatile("hlt");
}

void sched_init(void) {
	memset(threads, 0, sizeof(threads));
	for (int i = 0; i < MAX_THREADS; i++) {
		threads[i].state = THREAD_TERMINATED;
	}

	// Set up the kernel thread
	threads[0].id = next_thread_id++;
	threads[0].state = THREAD_RUNNING;
	threads[0].gs_base = 0;
	threads[0].syscall_stack_top = percpu_get()->kernel_rsp;

	uintptr_t boot_rsp;
	__asm__ volatile("mov %%rsp, %0" : "=r"(boot_rsp));
	threads[0].stack_ptr = boot_rsp;
	current_thread = 0;
	scheduler_initialized = 1;
}

int sched_create_thread(void (*entry_point)(void)) {
	if (!entry_point)
		return -1;
	return create_thread_context(entry_point, 1, 0, 0);
}

int sched_create_user_thread(uintptr_t entry_point, uintptr_t user_rsp) {
	if (!entry_point || !user_rsp || (user_rsp & 0xFULL))
		return -1;
	return create_thread_context(user_thread_start, 2, entry_point, user_rsp);
}

/* ------------------------------------------------------------------ */
/* sched_kill_current - terminate the calling thread
 *
 * Marks the current thread TERMINATED and switches away.  If no other
 * thread is runnable this returns to the caller, which is expected to
 * panic: there is nothing left to schedule.
 * ------------------------------------------------------------------ */
void sched_kill_current(void) {
	Thread* cur = sched_get_current_thread();
	if (cur) {
		cur->state = THREAD_TERMINATED;
		cur->block_reason = BLOCK_REASON_NONE;
	}
	sched_schedule();
	/* Nothing else to run -- caller decides. */
}

/* Called when an IPC queue gains a free slot. */
void sched_wake_blocked_senders(void) {
	for (int i = 0; i < MAX_THREADS; i++) {
		if (threads[i].state == THREAD_BLOCKED &&
			threads[i].block_reason == BLOCK_REASON_IPC_SEND) {
			threads[i].state = THREAD_READY;
			threads[i].block_reason = BLOCK_REASON_NONE;
		}
	}
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

	if (found == current_thread) {
		/* Nothing else is runnable.  If the caller has blocked itself we
		 * must really idle here: returning would leave a "blocked" thread
		 * spinning in the kernel and no one else would ever run.  A
		 * "blocked + spinning" thread was exactly what kept the keyboard
		 * driver from being woken by IRQ1. */
		if (threads[current_thread].state != THREAD_RUNNING) {
			while (threads[current_thread].state != THREAD_READY)
				__asm__ volatile("sti; hlt");
			threads[current_thread].state = THREAD_RUNNING;
		}
		return;
	}

	uint32_t prev = current_thread;
	current_thread = found;
	percpu_t* percpu = percpu_get();
	Thread* previous_thread = &threads[prev];
	Thread* next_thread = &threads[current_thread];

	if (percpu->syscall_active) {
		previous_thread->user_rsp = percpu->user_rsp;
		previous_thread->syscall_active = 1;
	} else {
		previous_thread->syscall_active = 0;
	}

	if (threads[prev].state == THREAD_RUNNING) {
		threads[prev].state = THREAD_READY;
	}
	next_thread->state = THREAD_RUNNING;
	percpu->kernel_rsp = next_thread->syscall_stack_top;
	percpu->user_rsp = next_thread->user_rsp;
	percpu->syscall_active = next_thread->syscall_active;

	cpu_switch_to(&previous_thread->stack_ptr, next_thread->stack_ptr);
}

Thread* sched_get_thread_by_pid(uint32_t pid) {
	for (int i = 0; i < MAX_THREADS; i++)
		if (threads[i].id == pid)
			return &threads[i];
	return 0;
}

uint32_t sched_get_current_pid(void) {
	return threads[current_thread].id;
}

Thread* sched_get_current_thread(void) {
	return &threads[current_thread];
}
