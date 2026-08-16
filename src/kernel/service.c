/*
    E-comOS Kernel - Service Registry
    Copyright (C) 2025,2026  Saladin5101
*/

#include <kernel/service.h>

#define MAX_SERVICES 32

static Service service_table[MAX_SERVICES];
static int service_count = 0;

int service_register(service_id id, uint32_t provider_pid, const char *name) {
	if (service_count >= MAX_SERVICES)
		return -1;
	for (int i = 0; i < service_count; i++) {
		if (service_table[i].id == id)
			return -2; /* already registered */
	}
	service_table[service_count].id = id;
	service_table[service_count].provider_pid = provider_pid;
	service_table[service_count].capabilities = 0;
	/* copy name */
	int j = 0;
	while (name[j] && j < 31) {
		service_table[service_count].name[j] = name[j];
		j++;
	}
	serviceTable[serviceCount].name[j] = '\0';
	service_count++;
	return 0;
}

int service_unregister(service_id id) {
	for (int i = 0; i < service_count; i++) {
		if (service_table[i].id == id) {
			service_table[i] = service_table[--service_count];
			return 0;
		}
	}
	return -1;
}

uint32_t service_lookup(service_id id) {
	for (int i = 0; i < service_count; i++) {
		if (service_table[i].id == id)
			return service_table[i].provider_pid;
	}
	return 0;
}

int service_call(service_id id, void *request, void *response) {
	(void)id;
	(void)request;
	(void)response;
	/* Full implementation requires IPC to the provider process.
	   Userspace services use SYS_IPC_SEND/RECEIVE directly. */
	return -1;
}
