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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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
/*
 * C99 <stdarg.h> for the kernel.
 *
 * The previous implementation walked a char* over the stack:
 *     typedef char* va_list;
 *     va_arg(ap, t) -> ((ap) += sizeof(t), *(t*)(ap - sizeof(t)))
 * That is only valid if every variadic argument sits contiguously right
 * above the last named parameter.  For the SysV x86_64 ABI arguments are
 * passed in registers and spilled into a register save area, so that
 * assumption is wrong and %d/%x conversions read garbage.
 *
 * Use the compiler's own implementation instead; it is provided by GCC
 * and needs no libc.
 */
typedef __builtin_va_list va_list;

#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)
#define va_copy(dest, src) __builtin_va_copy(dest, src)

#endif /* STDARG_H */