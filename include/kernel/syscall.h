/*
    E-comOS Kernel - A Microkernel for E-comOS
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

#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include <stdint.h>

#define SYS_IPC_SEND        1
#define SYS_IPC_RECEIVE     2
#define SYS_THREAD_YIELD    3
#define SYS_ADDRESS_MAP     4
#define SYS_IRQ_WAIT        5
#define SYS_IRQ_GET_COUNT   6
#define SYS_IRQ_RESET_COUNT 7

#define BLOCK_REASON_NONE     0
#define BLOCK_REASON_IRQ_WAIT 1

#define IRQ_WAIT_CLEAR  0x01
#define IRQ_WAIT_NOWAIT 0x02

#define ERR_TIMEOUT -3

void syscall_irq_init(void);
void syscall_irq_notify(uint8_t irq_num);
void syscall_irq_check_timeouts(void);
long syscall_handler(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3);

/* SYSCALL/SYSRET mechanism support */
void enable_syscall_mechanism(void);

#endif