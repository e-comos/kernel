#ifndef USER_SPACE_USER_MODE_H
#define USER_SPACE_USER_MODE_H

#include <stdint.h>

/* Load and switch to init-service in user mode */
int load_init_service_to_user_mode(void);

/* Switch from kernel mode to user mode (never returns) */
void __attribute__((noreturn)) switch_to_user_mode(uintptr_t entry_point, uintptr_t stack_pointer);

#endif /* USER_SPACE_USER_MODE_H */
       /*
        __    __   ______   _______  _______         ______  _______   ______   ______  _______
       
       |  |  |  | /  ____| |   ____||   _   \       /  ____||   _   \ /  __  \ /      ||   ____|
       |  |  |  | |  |__   |  |__   |  |_)  |      |  |__   |  |_)  ||  |  |  ||  ,----'|  |__
       |  |  |  |  \   __\ |   __|  |      /        \   __\ |   ___/ |  |  |  ||  |     |   __|
       |  `--'  |.___.|  | |  |____ |  |\  \----.____.|  |  |  |     |  `--'  ||  `----.|  |____
        \______/ \____/    |_______|| _| `._____|\______/   | _|      \______/ \______||_______|
       Kernel's code will End after switch_to_user_mode() is called. The kernel will never return to the kernel mode.
       Good morning user space! Of course, if user mode syscall() kernel, OS will return to the kernel mode.
       */