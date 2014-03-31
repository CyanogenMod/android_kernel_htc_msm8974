/*
 * platform/hardware.h
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 Tensilica Inc.
 */


#ifndef _XTENSA_XT2000_HARDWARE_H
#define _XTENSA_XT2000_HARDWARE_H

#include <variant/core.h>


#define PLATFORM_DEFAULT_MEM_START 0x00000000
#define PLATFORM_DEFAULT_MEM_SIZE 0x08000000

#define PLATFORM_NR_IRQS 3

#define SONIC83934_INTNUM	XCHAL_EXTINT3_NUM
#define SONIC83934_ADDR		IOADDR(0x0d030000)



#define IRQ_PCI_A		(XCHAL_NUM_INTERRUPTS + 0)
#define IRQ_PCI_B		(XCHAL_NUM_INTERRUPTS + 1)
#define IRQ_PCI_C		(XCHAL_NUM_INTERRUPTS + 2)


#define XT2000_LED_ADDR		IOADDR(0x0d040000)

#endif 
