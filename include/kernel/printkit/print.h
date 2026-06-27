/*
 * print.h - E-comOS Kernel Print Utility (Extended with paging and keyboard)
 *
 * Copyright (C) 2025,2026  Saladin5101
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef KERNEL_PRINT_H
#define KERNEL_PRINT_H

#include <stdint.h>

/* Color constants */
#define COLOR_BLACK         0x0
#define COLOR_BLUE          0x1
#define COLOR_GREEN         0x2
#define COLOR_CYAN          0x3
#define COLOR_RED           0x4
#define COLOR_MAGENTA       0x5
#define COLOR_BROWN         0x6
#define COLOR_LIGHT_GRAY    0x7
#define COLOR_DARK_GRAY     0x8
#define COLOR_LIGHT_BLUE    0x9
#define COLOR_LIGHT_GREEN   0xA
#define COLOR_LIGHT_CYAN    0xB
#define COLOR_LIGHT_RED     0xC
#define COLOR_LIGHT_MAGENTA 0xD
#define COLOR_YELLOW        0xE
#define COLOR_WHITE         0xF

#define VGA_COLOR(fg, bg) ((bg << 4) | fg)

/* Basic VGA output functions (unchanged) */
void clear_screen(uint8_t color);
void print_char(char c, uint8_t color);
void print_str(const char *str, uint8_t color);
void print_num(uint32_t num, uint8_t color);
void print_hex(uint32_t num, uint8_t color);

/* Extended 64-bit printing (new) */
void print_hex64(uint64_t value, uint8_t color);
void print_num64(uint64_t value, uint8_t color);

/* Paged output functions (pause every 24 lines, wait for key) */
void paged_print_str(const char *str, uint8_t color);
void paged_print_hex64(uint64_t value, uint8_t color);
void paged_print_num64(uint64_t value, uint8_t color);
void reset_paging_counter(void);

/* Keyboard polling (returns ' ' or 'q', others 0) */
char poll_keyboard(void);

#endif