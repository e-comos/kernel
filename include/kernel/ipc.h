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

#ifndef KERNEL_IPC_H
#define KERNEL_IPC_H

#include <stdint.h>
#include <kernel/internal/types.h>

#define IPC_MAX_DATA_SIZE   4096
#define IPC_MAX_QUEUE_SIZE  16

#define ECLIB_OK                    0
#define ECLIB_IPC_TIMEOUT          -1
#define ECLIB_IPC_SERVICE_UNAVAIL  -2
#define ECLIB_IPC_PERM_DENIED      -3
#define ECLIB_IPC_BUFFER_OVERFLOW  -4

typedef enum {
    IPC_MSG_REQUEST = 1,
    IPC_MSG_RESPONSE = 2,
    IPC_MSG_NOTIFICATION = 3,
} ipc_msg_type_t;

typedef struct ipc_message {
    uint32_t type;
    uint32_t source;
    uint32_t target;
    uint32_t timestamp;
    uint32_t size;
    uint32_t sequence;
    uint8_t  data[IPC_MAX_DATA_SIZE];
} ipc_message_t;

typedef uint32_t thread_id;

/* Low-level kernel IPC */
int ipc_send(thread_id target, ipc_message_t *msg);
int ipc_receive(ipc_message_t *msg);

/* IPC subsystem initialization */
void ipc_init(void);

/* Higher-level helpers */
int ipc_receive_msg(ipc_message_t *msg, int timeout_ms);
int ipc_send_msg(uint32_t type, uint32_t flags, uint32_t receiver_pid,
               uint32_t data_len, const void *data);

#endif
