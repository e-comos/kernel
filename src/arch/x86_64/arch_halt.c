/**
 * E-com_os Kernel - A Microkernel for E-com_os
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
#ifndef ARCH_HALT_H
#define ARCH_HALT_H
#include <kernel/arch/universal.h>
void arch_halt(void) {
	// Disable interrupts and halt the CPU
	__asm__ volatile("hlt");
}
#endif /* ARCH_HALT_H */