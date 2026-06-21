/*
    E-comOS Kernel - Print Utility
    Copyright (C) 2025,2026  Saladin5101

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
/*
 * print.c - E-comOS Kernel Print Utility (Extended with paging and keyboard)
 *
 * Copyright (C) 2025,2026  Saladin5101
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "kernel/printkit/print.h"

#define VGA_MEMORY ((volatile uint16_t *)0xB8000)

static int cursor_x = 0;
static int cursor_y = 0;

/* ---------------------------------------------------------------------------
 * Inline I/O helpers (PS/2 keyboard polling)
 * --------------------------------------------------------------------------- */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ---------------------------------------------------------------------------
 * Basic VGA functions (unchanged)
 * --------------------------------------------------------------------------- */
void clear_screen(uint8_t color) {
    (void)color;
    for (int i = 0; i < 80 * 25; i++)
        VGA_MEMORY[i] = 0x0720;
    cursor_x = cursor_y = 0;
}

void print_char(char c, uint8_t color) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else {
        VGA_MEMORY[cursor_y * 80 + cursor_x] = ((uint16_t)color << 8) | (uint8_t)c;
        cursor_x++;
    }
    if (cursor_x >= 80) {
        cursor_x = 0;
        cursor_y++;
    }
    if (cursor_y >= 25) {
        for (int i = 0; i < 80 * 24; i++)
            VGA_MEMORY[i] = VGA_MEMORY[i + 80];
        for (int i = 80 * 24; i < 80 * 25; i++)
            VGA_MEMORY[i] = 0x0F20;
        cursor_y = 24;
    }
}

void print_str(const char *str, uint8_t color) {
    for (int i = 0; str[i] != '\0'; i++)
        print_char(str[i], color);
}

void print_num(uint32_t num, uint8_t color) {
    char buf[12];
    char *ptr = buf + 11;
    *ptr = '\0';
    if (num == 0) {
        print_char('0', color);
        return;
    }
    while (num > 0) {
        *--ptr = '0' + (num % 10);
        num /= 10;
    }
    print_str(ptr, color);
}

void print_hex(uint32_t num, uint8_t color) {
    const char *digits = "0123456789ABCDEF";
    print_str("0x", color);
    for (int i = 7; i >= 0; i--)
        print_char(digits[(num >> (i * 4)) & 0xF], color);
}

/* ---------------------------------------------------------------------------
 * Extended 64-bit printing
 * --------------------------------------------------------------------------- */
void print_hex64(uint64_t value, uint8_t color) {
    const char *digits = "0123456789ABCDEF";
    print_str("0x", color);
    for (int i = 15; i >= 0; i--)
        print_char(digits[(value >> (i * 4)) & 0xF], color);
}

void print_num64(uint64_t value, uint8_t color) {
    char buf[22];
    char *ptr = buf + 21;
    *ptr = '\0';
    if (value == 0) {
        print_char('0', color);
        return;
    }
    while (value > 0) {
        *--ptr = '0' + (value % 10);
        value /= 10;
    }
    print_str(ptr, color);
}

/* ---------------------------------------------------------------------------
 * Paging support
 * --------------------------------------------------------------------------- */
#define LINES_PER_PAGE 24
static int paging_line_count = 0;

void reset_paging_counter(void) {
    paging_line_count = 0;
}

char poll_keyboard(void) {
    /* Wait for a key press */
    while ((inb(0x64) & 0x01) == 0) { /* spin */ }
    uint8_t scancode = inb(0x60);
    switch (scancode) {
        case 0x39: return ' ';   /* Space */
        case 0x10: return 'q';   /* Q key */
        default:   return 0;     /* ignore */
    }
}

static int wait_for_user(void) {
    print_str("--- Press SPACE to continue, 'q' to halt ---\n", 0x07);
    while (1) {
        char c = poll_keyboard();
        if (c == ' ') return 0;
        if (c == 'q') return 1;
    }
}

void paged_print_str(const char *str, uint8_t color) {
    for (const char *p = str; *p != '\0'; p++) {
        if (*p == '\n') {
            paging_line_count++;
            if (paging_line_count >= LINES_PER_PAGE) {
                paging_line_count = 0;
                if (wait_for_user()) {
                    print_str("\n*** HALT requested by user ***\n", 0x04);
                    __asm__ volatile("cli; hlt");
                }
            }
        }
        print_char(*p, color);
    }
}

void paged_print_hex64(uint64_t value, uint8_t color) {
    const char *digits = "0123456789ABCDEF";
    paged_print_str("0x", color);
    for (int i = 15; i >= 0; i--) {
        char c = digits[(value >> (i * 4)) & 0xF];
        paged_print_str(&c, color); /* single char string */
    }
}

void paged_print_num64(uint64_t value, uint8_t color) {
    char buf[22];
    char *ptr = buf + 21;
    *ptr = '\0';
    if (value == 0) {
        paged_print_str("0", color);
        return;
    }
    while (value > 0) {
        *--ptr = '0' + (value % 10);
        value /= 10;
    }
    paged_print_str(ptr, color);
}