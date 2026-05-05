#include <kernel/ipc.h>
#include <klibc/string.h>

// Internal IPC functions
int ipc_send(thread_id target, ipc_message_t *msg) {
    // Implementation of ipc_send
    // TODO: Implement actual IPC send
    (void)target;
    (void)msg;
    return 0;
}

int ipc_receive(ipc_message_t *msg) {
    // Implementation of ipc_receive
    // TODO: Implement actual IPC receive
    (void)msg;
    return 0;
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
