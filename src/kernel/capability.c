/*
    E-comOS Kernel - Capability System
    Copyright (C) 2025,2026  Saladin5101
*/

#include <kernel/capability.h>

int cap_grant(uint32_t target_pid, Capability cap) {
	(void)target_pid;
	(void)cap;
	/* Capability transfer between processes — requires IPC integration.
	   Placeholder until object manager service is implemented. */
	return 0;
}

int cap_revoke(Capability cap) {
	(void)cap;
	return 0;
}

int cap_check(Capability cap, uint32_t required_rights) {
	return (cap.rights & required_rights) == required_rights ? 0 : -1;
}
