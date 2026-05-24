#include <kernel/ipc.h>
#include <kernel/sched.h>
#include <kernel/syscall.h>
#include <string.h>
#include <kernel/internal/kernel.h>

// IPC message queue
static ipc_message_t ipc_queue[IPC_MAX_QUEUE_SIZE];
static uint32_t ipc_queue_head = 0;
static uint32_t ipc_queue_tail = 0;
static uint32_t ipc_queue_count = 0;
static uint32_t ipc_sequence = 0;

void ipc_init(void) {
    ipc_queue_head = 0;
    ipc_queue_tail = 0;
    ipc_queue_count = 0;
    ipc_sequence = 0;
}

int ipc_send(thread_id target, ipc_message_t *msg) {
    if (!msg) return ECLIB_IPC_PERM_DENIED;

    // Find the target thread
    Thread *target_thread = sched_get_thread_by_pid(target);
    if (!target_thread) return ECLIB_IPC_SERVICE_UNAVAIL;

    // Check if queue is full
    if (ipc_queue_count >= IPC_MAX_QUEUE_SIZE) {
        return ECLIB_IPC_BUFFER_OVERFLOW;
    }

    // Copy message to queue
    ipc_message_t *entry = &ipc_queue[ipc_queue_tail];
    entry->type = msg->type;
    entry->source = msg->source;
    entry->target = msg->target;
    entry->timestamp = msg->timestamp;
    entry->size = msg->size;
    entry->sequence = ++ipc_sequence;

    if (msg->size > 0 && msg->size <= IPC_MAX_DATA_SIZE) {
        memcpy(entry->data, msg->data, msg->size);
    }

    // Update queue
    ipc_queue_tail = (ipc_queue_tail + 1) % IPC_MAX_QUEUE_SIZE;
    ipc_queue_count++;

    // Wake up target thread if it's blocked waiting for IPC
    if (target_thread->state == THREAD_BLOCKED &&
        target_thread->block_reason == BLOCK_REASON_NONE) {
        target_thread->state = THREAD_READY;
    }
    
    return ECLIB_OK;
}

int ipc_receive(ipc_message_t *msg) {
    if (!msg) return ECLIB_IPC_PERM_DENIED;

    // Check if queue is empty
    if (ipc_queue_count == 0) {
        return ECLIB_IPC_TIMEOUT;
    }

    // Copy message from queue
    ipc_message_t *entry = &ipc_queue[ipc_queue_head];
    msg->type = entry->type;
    msg->source = entry->source;
    msg->target = entry->target;
    msg->timestamp = entry->timestamp;
    msg->size = entry->size;
    msg->sequence = entry->sequence;

    if (entry->size > 0 && entry->size <= IPC_MAX_DATA_SIZE) {
        memcpy(msg->data, entry->data, entry->size);
    }

    // Update queue
    ipc_queue_head = (ipc_queue_head + 1) % IPC_MAX_QUEUE_SIZE;
    ipc_queue_count--;

    return ECLIB_OK;
}

int ipc_send_msg(uint32_t type, uint32_t flags, uint32_t receiver_pid,
               uint32_t data_len, const void *data) {
    ipc_message_t msg = {0};
    
    msg.type = type;
    msg.target = receiver_pid;
    msg.size = (uint32_t)data_len;
    
    // Store flags in the message (using timestamp field for now)
    msg.timestamp = (uint32_t)flags;
    
    // Copy data if provided
    if (data && data_len > 0 && data_len <= IPC_MAX_DATA_SIZE) {
        memcpy(msg.data, data, data_len);
    }
    
    // Call the low-level send function
    return ipc_send(receiver_pid, &msg);
}

int ipc_receive_msg(ipc_message_t *msg, int timeout_ms) {
    // Implementation that can timeout
    // For now, just call the blocking version
    (void)timeout_ms;
    return ipc_receive(msg);
}
