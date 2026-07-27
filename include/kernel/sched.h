/*
    E-comOS Kernel - A Microkernel for E-comOS
    Copyright (C) 2025,2026  Saladin5101

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef KERNEL_SCHED_H
#define KERNEL_SCHED_H

#include <stdint.h>

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED
} thread_state;

typedef struct thread {
    uint32_t     id;
    thread_state state;
    uintptr_t    stack_ptr;
    uint32_t     priority;
    uint8_t      block_reason;
    int32_t      last_error;
    union {
        uint8_t irq_num;
    } block_data;
} Thread;

void    sched_init(void);
int     sched_create_thread(void (*entry_point)(void));
void    sched_yield(void);
void    sched_schedule(void);
Thread *sched_get_thread_by_pid(uint32_t pid);
Thread *sched_get_current_thread(void);
uint32_t sched_get_current_pid(void);

#endif
