/*
 * E-comOS - Object system (USERSPACE SERVICE)
 * This should NOT be in kernel - move to userspace!
 */

#ifndef USERSPACE_OBJECT_H
#define USERSPACE_OBJECT_H

#include <stdint.h>

typedef uint32_t object_id_t;

// Object types (everything is an object)
#define OBJ_TYPE_THREAD 1
#define OBJ_TYPE_PROCESS 2
#define OBJ_TYPE_MEMORY 3
#define OBJ_TYPE_IPC_CHANNEL 4
#define OBJ_TYPE_IRQ 5
#define OBJ_TYPE_IO_PORT 6

// Object descriptor
struct object {
	object_id_t id;
	uint32_t type;
	uint32_t owner_process;
	uint32_t ref_count;
	void *data;
};

// Object operations (USERSPACE SERVICE)
// These will be implemented as IPC calls to Object Manager Service
// object_create() -> IPC to ObjectManagerService
// object_destroy() -> IPC to ObjectManagerService

#endif