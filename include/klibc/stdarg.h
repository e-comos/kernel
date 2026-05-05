/**
 * ECLib - E-comOS C Library
 * Copyright (C) 2026 Saladin5101
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */
/* By C99 , Variable arguments <stdarg.h> */
#ifndef STDARG_H
#define STDARG_H
// C99 Standard 7.15 Variable arguments <stdarg.h> Implementation
// For E-comOS Version 0.0.1
#ifdef __arm__
typedef __builtin_va_list va_list;
#elif defined(__x86_64__)
typedef char* va_list;
#else
typedef __builtin_va_list va_list;
#endif

#define _VA_ALIGNMENT sizeof(int)
#define _VA_ROUND_UP(n) (((n) + _VA_ALIGNMENT - 1) & ~(_VA_ALIGNMENT - 1))

#define va_start(ap, last) ((ap) = (va_list)&(last) + _VA_ROUND_UP(sizeof(last)))
#define va_arg(ap, type) (*(type*)((ap) += _VA_ROUND_UP(sizeof(type)), \
                                   (ap) - _VA_ROUND_UP(sizeof(type))))
#define va_end(ap) ((ap) = (va_list)0)
#define va_copy(dest, src) ((dest) = (src))
#endif /* STDARG_H */