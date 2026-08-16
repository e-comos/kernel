/*
 * E-comOS Kernel - Driver Loader (Multiboot2 Module Parser)
 * Copyright (C) 2025,2026  Saladin5101
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <user_space/driver_loader.h>
#include <kernel/debug.h>
#include <kernel/printkit/print.h>
#include <kernel/mm.h>

/* Physical address of the Multiboot2 info structure (set by boot assembly) */
extern uint32_t multiboot2_info_phys;

/* Global variables holding the driver module's physical address and size */
uint64_t driver_phys_addr = 0;
uint64_t driver_size = 0;


/*
 * load_driver_from_multiboot - Scan Multiboot2 tags for a driver module.
 *
 * Searches for a module tag (type 3). If found, it stores the physical
 * address and size in the global variables driver_phys_addr and driver_size.
 * This allows the user-mode initialization code to later write it to
 * the shared boot_info structure.
 *
 * Returns: 0 on success, -1 on error or if no driver module is found.
 */
int load_driver_from_multiboot(void *mb_info_addr) {
    /* Fallback to the fixed physical address if none given */
    if (!mb_info_addr || (uintptr_t)mb_info_addr == 0) {
        mb_info_addr = (void *)(uintptr_t)multiboot2_info_phys;
    }

    if (!mb_info_addr || (uintptr_t)mb_info_addr == 0) {
        print_str("Error: Multiboot info address is NULL\n", 0x0F);
        return -1;
    }

    uint32_t addr = (uint32_t)(uintptr_t)mb_info_addr;
    uint32_t total_size = *(uint32_t *)(uintptr_t)addr;

    /* Basic sanity check on the info structure */
    if (total_size < 16 || total_size > 4096) {
        print_str("Error: Invalid Multiboot2 total size\n", 0x0F);
        return -1;
    }

    /* Tags start at offset 8 */
    struct multiboot_tag *tag = (struct multiboot_tag *)(uintptr_t)(addr + 8);
    struct multiboot_tag *end = (struct multiboot_tag *)(uintptr_t)(addr + total_size);

    print_str("Scanning Multiboot2 modules for driver...\n", 0x0F);

    while (tag < end && tag->type != 0) {
        if (tag->size == 0) {
            print_str("Error: Encountered Multiboot tag with zero size\n", 0x0F);
            return -1;
        }

        /* Tag type 3 = Module */
        if (tag->type == 3) {
            struct multiboot_tag_module *mod = (struct multiboot_tag_module *)tag;
            uint32_t pstart = mod->mod_start;
            uint32_t pend   = mod->mod_end;
            uint32_t size   = pend - pstart;

            if (size == 0 || size > 0x100000) {
                print_str("Error: Invalid driver module size\n", 0x0F);
                return -1;
            }

            print_str("Found driver module. Physical: 0x", 0x0F);
            print_hex(pstart, 0x0F);
            print_str(" Size: ", 0x0F);
            print_num(size, 0x0F);
            print_str(" bytes\n", 0x0F);

            /* Store in global variables for later use */
            driver_phys_addr = (uint64_t)pstart;
            driver_size      = (uint64_t)size;

            return 0;
        }

        /* Advance to the next tag (8‑byte aligned) */
        tag = (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7));
    }

    print_str("Warning: No driver module found in Multiboot2 tags\n", 0x0F);
    driver_phys_addr = 0;
    driver_size = 0;
    return -1;
}