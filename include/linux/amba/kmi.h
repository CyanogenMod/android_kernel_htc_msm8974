/*
 *  linux/include/asm-arm/hardware/amba_kmi.h
 *
 *  Internal header file for AMBA KMI ports
 *
 *  Copyright (C) 2000 Deep Blue Solutions Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * ---------------------------------------------------------------------------
 *  From ARM PrimeCell(tm) PS2 Keyboard/Mouse Interface (PL050) Technical
 *  Reference Manual - ARM DDI 0143B - see http://www.arm.com/
 * ---------------------------------------------------------------------------
 */
#ifndef ASM_ARM_HARDWARE_AMBA_KMI_H
#define ASM_ARM_HARDWARE_AMBA_KMI_H

#define KMICR		(KMI_BASE + 0x00)
#define KMICR_TYPE		(1 << 5)
#define KMICR_RXINTREN		(1 << 4)
#define KMICR_TXINTREN		(1 << 3)
#define KMICR_EN		(1 << 2)
#define KMICR_FD		(1 << 1)
#define KMICR_FC		(1 << 0)

#define KMISTAT		(KMI_BASE + 0x04)
#define KMISTAT_TXEMPTY		(1 << 6)
#define KMISTAT_TXBUSY		(1 << 5)
#define KMISTAT_RXFULL		(1 << 4)
#define KMISTAT_RXBUSY		(1 << 3)
#define KMISTAT_RXPARITY	(1 << 2)
#define KMISTAT_IC		(1 << 1)
#define KMISTAT_ID		(1 << 0)

#define KMIDATA		(KMI_BASE + 0x08)

#define KMICLKDIV	(KMI_BASE + 0x0c)

#define KMIIR		(KMI_BASE + 0x10)
#define KMIIR_TXINTR		(1 << 1)
#define KMIIR_RXINTR		(1 << 0)

#define KMI_SIZE	(0x100)

#endif
