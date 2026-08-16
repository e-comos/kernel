/*
    E-comOS Kernel - Service Registry
    Copyright (C) 2025,2026  Saladin5101
*/

#ifndef KERNEL_SERVICE_H
#define KERNEL_SERVICE_H

#include <stdint.h>

typedef service_id;

#define SERVICE_VGA_DISPLAY 1
#define SERVICE_KEYBOARD_INPUT 2
#define SERVICE_FILE_SYSTEM 3
#define SERVICE_NETWORK 4
#define SERVICE_MEMORY_MANAGER 5

typedef struct {
	service_id id;
	uint32_t provider_pid;
	char name[32];
	uint32_t capabilities;
} Service;

int service_register(service_id id, uint32_t provider_pid, const char *name);
int service_unregister(service_id id);
uint32_t service_lookup(service_id id);
int service_call(service_id id, void *request, void *response);

#endif
