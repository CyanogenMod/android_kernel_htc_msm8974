/* include/asm-sh/dreamcast/sysasic.h
 *
 * Definitions for the Dreamcast System ASIC and related peripherals.
 *
 * Copyright (c) 2001 M. R. Brown <mrbrown@linuxdc.org>
 * Copyright (C) 2003 Paul Mundt <lethal@linux-sh.org>
 *
 * This file is part of the LinuxDC project (www.linuxdc.org)
 *
 * Released under the terms of the GNU GPL v2.0.
 *
 */
#ifndef __ASM_SH_DREAMCAST_SYSASIC_H
#define __ASM_SH_DREAMCAST_SYSASIC_H

#include <asm/irq.h>


#define HW_EVENT_IRQ_BASE  48

#define HW_EVENT_VSYNC     (HW_EVENT_IRQ_BASE +  5) 
#define HW_EVENT_MAPLE_DMA (HW_EVENT_IRQ_BASE + 12) 
#define HW_EVENT_GDROM_DMA (HW_EVENT_IRQ_BASE + 14) 
#define HW_EVENT_G2_DMA    (HW_EVENT_IRQ_BASE + 15) 
#define HW_EVENT_PVR2_DMA  (HW_EVENT_IRQ_BASE + 19) 

#define HW_EVENT_GDROM_CMD (HW_EVENT_IRQ_BASE + 32) 
#define HW_EVENT_AICA_SYS  (HW_EVENT_IRQ_BASE + 33) 
#define HW_EVENT_EXTERNAL  (HW_EVENT_IRQ_BASE + 35) 

#define HW_EVENT_IRQ_MAX (HW_EVENT_IRQ_BASE + 95)

extern int systemasic_irq_demux(int);
extern void systemasic_irq_init(void);
extern void aica_time_init(void);

#endif 

