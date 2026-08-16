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
#include <kernel/ipc.h>
#include <kernel/mm.h>
#include <kernel/sched.h>
#include <klibc/string.h>
#include <kernel/internal/kernel.h>

int ipc_send(thread_id target, ipc_message_t *msg) {
	if (!msg)
		return -1;
	Thread *t = sched_get_thread_by_pid(target);
	if (!t)
		return -1;
	if (t->ipc_count >= IPC_MAX_QUEUE_SIZE)
		return -1;

	ipc_message_t *copy = mm_alloc_page();
	if (!copy)
		return -1;
	*copy = *msg;

    // TODO: For now, just make the `source` to be the threads ID
    // But in the future, we have Process Managment System. Please remove this line. 
    copy->source = sched_get_current_thread()->id;
    
	t->ipc_queue[t->ipc_tail] = copy;
	t->ipc_tail = (t->ipc_tail + 1) % IPC_MAX_QUEUE_SIZE;
	t->ipc_count++;

	if (t->state == THREAD_BLOCKED && t->block_reason == BLOCK_REASON_IPC_WAIT) {
		t->state = THREAD_READY;
		t->block_reason = BLOCK_REASON_NONE;
	}
	return 0;
}

int ipc_receive(ipc_message_t *msg) {
	if (!msg)
		return -1;
	Thread *cur = sched_get_current_thread();
	if (!cur)
		return -1;

	while (cur->ipc_count == 0) {
		cur->state = THREAD_BLOCKED;
		cur->block_reason = BLOCK_REASON_IPC_WAIT;
		sched_yield();
	}

	ipc_message_t *entry = cur->ipc_queue[cur->ipc_head];
	*msg = *entry;
	mm_free_page(entry);

	cur->ipc_head = (cur->ipc_head + 1) % IPC_MAX_QUEUE_SIZE;
	cur->ipc_count--;
	return 0;
}
