#include <user_space/driver_loader.h>
#include <kernel/mm.h>
#include <kernel/printkit/print.h>
#include <kernel/debug.h>
#include <stdint.h>

extern uint32_t multiboot2_info_phys;

int load_driver_from_multiboot(void* mb_info_addr) {
    if (!mb_info_addr || (uintptr_t)mb_info_addr == 0) {
        mb_info_addr = (void*)(uintptr_t)multiboot2_info_phys;
    }

    if (!mb_info_addr || (uintptr_t)mb_info_addr == 0) {
        print_str("Error: Multiboot info address is NULL\n", 0x0F);
        return -1;
    }

    uint32_t addr = (uint32_t)(uintptr_t)mb_info_addr;
    
    uint32_t total_size = *(uint32_t *)(uintptr_t)addr;
    /* Limit the size check to the 4KB (4096 bytes) buffer max we created */
    if (total_size < 16 || total_size > 4096) {
        print_str("Error: Invalid Multiboot2 total size\n", 0x0F);
        return -1;
    }

    struct multiboot_tag *tag = (struct multiboot_tag *)(uintptr_t)(addr + 8);
    struct multiboot_tag *end = (struct multiboot_tag *)(uintptr_t)(addr + total_size);

    print_str("Scanning Multiboot2 modules for driver...\n", 0x0F);

    while (tag < end && tag->type != 0) {
        if (tag->size == 0) {
            print_str("Error: Encountered Multiboot tag with zero size\n", 0x0F);
            return -1;
        }

        if (tag->type == 3) { // Type 3 = Module tag
            struct multiboot_tag_module *mod = (struct multiboot_tag_module *)tag;
            
            uint32_t pstart = mod->mod_start;
            uint32_t pend = mod->mod_end;
            uint32_t size = pend - pstart;

            if (size == 0 || size > 0x100000) {
                print_str("Error: Invalid driver module size\n", 0x0F);
                return -1;
            }

            print_str("Found driver module. Physical: 0x", 0x0F);
            print_hex(pstart, 0x0F);
            print_str(" Size: ", 0x0F);
            print_num(size, 0x0F);
            print_str(" bytes\n", 0x0F);

            uint32_t pages = (uint32_t)((size + PAGE_SIZE - 1) / PAGE_SIZE);
            for (uint32_t p = 0; p < pages; p++) {
                void *pa = mm_alloc_page();
                if (!pa) {
                    kernel_panic("OOM: driver page allocation failed");
                }
                if (mm_map_page(DRIVER_LOAD_ADDR + (p * PAGE_SIZE), (uintptr_t)pa, MM_FLAG_USER_RX) != 0) {
                    kernel_panic("Failed to map driver page");
                }
            }

            uint8_t *src = (uint8_t *)(uintptr_t)pstart;
            uint8_t *dst = (uint8_t *)DRIVER_LOAD_ADDR;
            for (uint32_t i = 0; i < size; i++) {
                dst[i] = src[i];
            }

            if (mm_map_page(0xB80000ULL, 0xB80000ULL, MM_FLAG_USER_RW) != 0) {
                kernel_panic("Failed to map VGA buffer for driver");
            }
            print_str("  VGA buffer (0xB80000) mapped for user-space driver\n", 0x0F);

            print_str("drivers.bin successfully loaded to virtual address 0x700000\n", 0x0F);
            return 0;
        }

        tag = (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7));
    }

    print_str("Warning: No driver module found in Multiboot2 tags\n", 0x0F);
    return -1;
}

/* 
 * Reads components like init-service or ebts directly from their designated 
 * physical memory addresses rather than relying on a GRUB module.
 */
int load_service_from_memory(uintptr_t phys_addr, uint32_t size, uintptr_t target_vaddr) {
    if (size == 0 || size > 0x200000) { 
        print_str("Error: Invalid service size\n", 0x0F);
        return -1;
    }

    print_str("Loading service from raw memory at 0x", 0x0F);
    print_hex((uint32_t)phys_addr, 0x0F);
    print_str("\n", 0x0F);

    uint32_t pages = (uint32_t)((size + PAGE_SIZE - 1) / PAGE_SIZE);
    for (uint32_t p = 0; p < pages; p++) {
        void *pa = mm_alloc_page();
        if (!pa) {
            kernel_panic("OOM: service page allocation failed");
        }
        if (mm_map_page(target_vaddr + (p * PAGE_SIZE), (uintptr_t)pa, MM_FLAG_USER_RX) != 0) {
            kernel_panic("Failed to map service page");
        }
    }

    uint8_t *src = (uint8_t *)phys_addr;
    uint8_t *dst = (uint8_t *)target_vaddr;
    for (uint32_t i = 0; i < size; i++) {
        dst[i] = src[i];
    }

    print_str("Service successfully loaded directly from memory\n", 0x0F);
    return 0;
}
