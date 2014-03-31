/*****************************************************************************
* Copyright 2004 - 2008 Broadcom Corporation.  All rights reserved.
*
* Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2, available at
* http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a
* license other than the GPL, without Broadcom's express prior written
* consent.
*****************************************************************************/

#ifndef _TMRHW_REG_H
#define _TMRHW_REG_H

#include <mach/csp/mm_io.h>
#include <mach/csp/hw_cfg.h>
#define tmrHw_MODULE_BASE_ADDR          MM_IO_BASE_TMR

#define tmrHw_LOW_FREQUENCY_MHZ         25	
#define tmrHw_LOW_FREQUENCY_HZ          25000000

#if defined(CFG_GLOBAL_CHIP) && (CFG_GLOBAL_CHIP == FPGA11107)
#define tmrHw_HIGH_FREQUENCY_MHZ        150	
#define tmrHw_HIGH_FREQUENCY_HZ         150000000
#else
#define tmrHw_HIGH_FREQUENCY_HZ         HW_CFG_BUS_CLK_HZ
#define tmrHw_HIGH_FREQUENCY_MHZ        (HW_CFG_BUS_CLK_HZ / 1000000)
#endif

#define tmrHw_LOW_RESOLUTION_CLOCK      tmrHw_LOW_FREQUENCY_HZ
#define tmrHw_HIGH_RESOLUTION_CLOCK     tmrHw_HIGH_FREQUENCY_HZ
#define tmrHw_MAX_COUNT                 (0xFFFFFFFF)	
#define tmrHw_TIMER_NUM_COUNT           (4)	

typedef struct {
	uint32_t LoadValue;	
	uint32_t CurrentValue;	
	uint32_t Control;	
	uint32_t InterruptClear;	
	uint32_t RawInterruptStatus;	
	uint32_t InterruptStatus;	
	uint32_t BackgroundLoad;	
	uint32_t padding;	
} tmrHw_REG_t;

#define tmrHw_CONTROL_TIMER_ENABLE            0x00000080
#define tmrHw_CONTROL_PERIODIC                0x00000040
#define tmrHw_CONTROL_INTERRUPT_ENABLE        0x00000020
#define tmrHw_CONTROL_PRESCALE_MASK           0x0000000C
#define tmrHw_CONTROL_PRESCALE_1              0x00000000
#define tmrHw_CONTROL_PRESCALE_16             0x00000004
#define tmrHw_CONTROL_PRESCALE_256            0x00000008
#define tmrHw_CONTROL_32BIT                   0x00000002
#define tmrHw_CONTROL_ONESHOT                 0x00000001
#define tmrHw_CONTROL_FREE_RUNNING            0x00000000

#define tmrHw_CONTROL_MODE_MASK               (tmrHw_CONTROL_PERIODIC | tmrHw_CONTROL_ONESHOT)

#define pTmrHw ((volatile tmrHw_REG_t *)tmrHw_MODULE_BASE_ADDR)

#endif 
