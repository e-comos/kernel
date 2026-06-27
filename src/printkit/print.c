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

#include <kernel/printkit/print.h>

#define VGA_MEMORY ((volatile uint16_t *)0xB8000)

static int cursor_x = 0;
static int cursor_y = 0;

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
    char  buf[12];
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
