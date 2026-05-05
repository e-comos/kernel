/*
    E-comOS Kernel - Boot Parameters
    Copyright (C) 2025,2026  Saladin5101

    Precondition:  filled by the UEFI bootloader before jumping to _start.
    Postcondition: read-only after kernel_main receives it.
*/

#ifndef KERNEL_BOOT_H
#define KERNEL_BOOT_H

#include <stdint.h>
#include <kernel/internal/types.h>
/* UEFI memory types (UEFI spec 2.x Table 7-6) */
#define EFI_RESERVED_MEMORY_TYPE   0
#define EFI_LOADER_CODE            1
#define EFI_LOADER_DATA            2
#define EFI_BOOT_SERVICES_CODE     3
#define EFI_BOOT_SERVICES_DATA     4
#define EFI_RUNTIME_SERVICES_CODE  5
#define EFI_RUNTIME_SERVICES_DATA  6
#define EFI_CONVENTIONAL_MEMORY    7
#define EFI_UNUSABLE_MEMORY        8
#define EFI_ACPI_RECLAIM_MEMORY    9
#define EFI_ACPI_MEMORY_NVS        10
#define EFI_MEMORY_MAPPED_IO       11
#define EFI_MEMORY_MAPPED_IO_PORT  12
#define EFI_PAL_CODE               13

/*
 * EFI_MEMORY_DESCRIPTOR — layout matches UEFI spec.
 * desc_size from get_memory_map() may be larger than sizeof this struct;
 * always use boot_params->memory_descriptor_size to stride the array.
 */
typedef struct {
    uint32_t type;
    uint32_t _pad;           /* UEFI spec: 4-byte pad before physicalStart */
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;  /* 4 KB EFI pages */
    uint64_t attribute;
} __attribute__((packed)) efi_memory_descriptor;

/* Passed from bootloader to kernelMain via rdi (System V AMD64 ABI) */
typedef struct {
    u64 signature;                       // Magic number for validation
    u32 version;                         // Structure version
    u32 size;                            // Size of this structure

    u64 memory_map;
    u64 memory_map_size;
    u64 memory_map_key;
    u64 memory_map_descriptor_size;
    u64 memory_map_descriptor_version;

    u64 frame_buffer;                  // Address of framebuffer
    u32 frame_buffer_width;
    u32 frame_buffer_height;
    u32 frame_buffer_pitch;
    u32 frame_buffer_bpp;

    void* acpi_rsdt;
    void* smbios_table;
    u32 daemon_process_id;
    u64 shared_header_phys;
    u64 kernel_base;
    u64 kernel_size;
    u64 kernel_entry;

    char commandLine[256];             // Optional command line for kernel

    void* runtime_service;
    u64 rt_service_phys;
    u64 shared_buffer;
    uint64_t shared_buffer_size;
}boot_params;
/*
 * Linker-provided symbols marking the kernel image boundaries.
 * Used by mm_init to precisely reserve kernel pages.
 * Declared as arrays so taking their address gives the symbol value
 * without an extra indirection.
 */
extern uint8_t _kernelStart[];
extern uint8_t _kernelEnd[];

typedef struct {
    uint64_t ebts_src;
    uint64_t ebts_size;
    uint32_t flags;
    uint32_t _pad;
} boot_info_t;

#define BOOT_INFO_ADDR  0x600000ULL
#define INIT_LOAD_ADDR  0x400000ULL
#define EBTS_LOAD_ADDR  0x500000ULL
#define INIT_STACK_TOP  0x7FFFF0ULL

#endif
