#ifndef USER_SPACE_USER_MODE_H
#define USER_SPACE_USER_MODE_H

#include <stdint.h>

/* Load and switch to init-service in user mode */
int load_init_service_to_user_mode(void);

/* Switch from kernel mode to user mode (never returns) */
void __attribute__((noreturn)) switch_to_user_mode(uintptr_t entry_point, uintptr_t stack_pointer);

#endif /* USER_SPACE_USER_MODE_H */
