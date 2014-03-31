/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * ip22.h: Definitions for SGI IP22 machines
 *
 * Copyright (C) 1996 David S. Miller
 * Copyright (C) 1997, 1998, 1999, 2000 Ralf Baechle
 */

#ifndef _SGI_IP22_H
#define _SGI_IP22_H


#include <irq.h>
#include <asm/sgi/ioc.h>

#define SGINT_EISA	0	
#define SGINT_CPU	MIPS_CPU_IRQ_BASE	
#define SGINT_LOCAL0	(SGINT_CPU+8)	
#define SGINT_LOCAL1	(SGINT_CPU+16)	
#define SGINT_LOCAL2	(SGINT_CPU+24)	
#define SGINT_LOCAL3	(SGINT_CPU+32)	
#define SGINT_END	(SGINT_CPU+40)	


#define SGI_SOFT_0_IRQ	SGINT_CPU + 0
#define SGI_SOFT_1_IRQ	SGINT_CPU + 1
#define SGI_LOCAL_0_IRQ	SGINT_CPU + 2
#define SGI_LOCAL_1_IRQ	SGINT_CPU + 3
#define SGI_8254_0_IRQ	SGINT_CPU + 4
#define SGI_8254_1_IRQ	SGINT_CPU + 5
#define SGI_BUSERR_IRQ	SGINT_CPU + 6
#define SGI_TIMER_IRQ	SGINT_CPU + 7

#define SGI_FIFO_IRQ	SGINT_LOCAL0 + 0	
#define SGI_GIO_0_IRQ	SGI_FIFO_IRQ		
#define SGI_WD93_0_IRQ	SGINT_LOCAL0 + 1	
#define SGI_WD93_1_IRQ	SGINT_LOCAL0 + 2	
#define SGI_ENET_IRQ	SGINT_LOCAL0 + 3	
#define SGI_MCDMA_IRQ	SGINT_LOCAL0 + 4	
#define SGI_PARPORT_IRQ	SGINT_LOCAL0 + 5	
#define SGI_GIO_1_IRQ	SGINT_LOCAL0 + 6	
#define SGI_MAP_0_IRQ	SGINT_LOCAL0 + 7	

#define SGI_GPL0_IRQ	SGINT_LOCAL1 + 0	
#define SGI_PANEL_IRQ	SGINT_LOCAL1 + 1	
#define SGI_GPL2_IRQ	SGINT_LOCAL1 + 2	
#define SGI_MAP_1_IRQ	SGINT_LOCAL1 + 3	
#define SGI_HPCDMA_IRQ	SGINT_LOCAL1 + 4	
#define SGI_ACFAIL_IRQ	SGINT_LOCAL1 + 5	
#define SGI_VINO_IRQ	SGINT_LOCAL1 + 6	
#define SGI_GIO_2_IRQ	SGINT_LOCAL1 + 7	

#define SGI_VERT_IRQ	SGINT_LOCAL2 + 0	
#define SGI_EISA_IRQ	SGINT_LOCAL2 + 3	
#define SGI_KEYBD_IRQ	SGINT_LOCAL2 + 4	
#define SGI_SERIAL_IRQ	SGINT_LOCAL2 + 5	

#define ip22_is_fullhouse()	(sgioc->sysid & SGIOC_SYSID_FULLHOUSE)

extern unsigned short ip22_eeprom_read(unsigned int *ctrl, int reg);
extern unsigned short ip22_nvram_read(int reg);

#endif
