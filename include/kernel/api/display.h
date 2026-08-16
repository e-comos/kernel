/*
 * E-com_os Microkernel - Display service API
 * Interface for userspace display drivers
 */

#ifndef KERNEL_API_DISPLAY_H
#define KERNEL_API_DISPLAY_H

#include <stdint.h>

// Display service capabilities
#define DISPLAY_CAP_TEXT_MODE (1 << 0)
#define DISPLAY_CAP_GRAPHICS (1 << 1)
#define DISPLAY_CAP_COLOR (1 << 2)

// System calls for display service
typedef enum {
	SYS_DISPLAY_MAP_FRAMEBUFFER = 0x100,
	SYS_DISPLAY_REGISTER_SERVICE,
	SYS_DISPLAY_UNREGISTER_SERVICE
} display_syscall_t;

// Display service registration
struct display_service_info {
	uint32_t capabilities;
	uint32_t width;
	uint32_t height;
	uint32_t bpp;
};

#endif