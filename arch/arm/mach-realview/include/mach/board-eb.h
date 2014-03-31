/*
 * arch/arm/mach-realview/include/mach/board-eb.h
 *
 * Copyright (C) 2007 ARM Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifndef __ASM_ARCH_BOARD_EB_H
#define __ASM_ARCH_BOARD_EB_H

#include <mach/platform.h>

#define REALVIEW_EB_UART0_BASE		0x10009000	
#define REALVIEW_EB_UART1_BASE		0x1000A000	
#define REALVIEW_EB_UART2_BASE		0x1000B000	
#define REALVIEW_EB_UART3_BASE		0x1000C000	
#define REALVIEW_EB_SSP_BASE		0x1000D000	
#define REALVIEW_EB_WATCHDOG_BASE	0x10010000	
#define REALVIEW_EB_TIMER0_1_BASE	0x10011000	
#define REALVIEW_EB_TIMER2_3_BASE	0x10012000	
#define REALVIEW_EB_GPIO0_BASE		0x10013000	
#define REALVIEW_EB_RTC_BASE		0x10017000	
#define REALVIEW_EB_CLCD_BASE		0x10020000	
#define REALVIEW_EB_GIC_CPU_BASE	0x10040000	
#define REALVIEW_EB_GIC_DIST_BASE	0x10041000	
#define REALVIEW_EB_SMC_BASE		0x10080000	

#define REALVIEW_EB_FLASH_BASE		0x40000000
#define REALVIEW_EB_FLASH_SIZE		SZ_64M
#define REALVIEW_EB_ETH_BASE		0x4E000000	
#define REALVIEW_EB_USB_BASE		0x4F000000	

#ifdef CONFIG_REALVIEW_EB_ARM11MP_REVB
#define REALVIEW_EB11MP_PRIV_MEM_BASE	0x1F000000
#define REALVIEW_EB11MP_L220_BASE	0x10102000	
#define REALVIEW_EB11MP_SYS_PLD_CTRL1	0xD8		
#else
#define REALVIEW_EB11MP_PRIV_MEM_BASE	0x1F000000
#define REALVIEW_EB11MP_L220_BASE	0x1F002000	
#define REALVIEW_EB11MP_SYS_PLD_CTRL1	0x74		
#endif

#define REALVIEW_EB11MP_PRIV_MEM_SIZE	SZ_8K
#define REALVIEW_EB11MP_PRIV_MEM_OFF(x)	(REALVIEW_EB11MP_PRIV_MEM_BASE + (x))

#define REALVIEW_EB11MP_SCU_BASE	REALVIEW_EB11MP_PRIV_MEM_OFF(0)		
#define REALVIEW_EB11MP_GIC_CPU_BASE	REALVIEW_EB11MP_PRIV_MEM_OFF(0x0100)	
#define REALVIEW_EB11MP_TWD_BASE	REALVIEW_EB11MP_PRIV_MEM_OFF(0x0600)
#define REALVIEW_EB11MP_GIC_DIST_BASE	REALVIEW_EB11MP_PRIV_MEM_OFF(0x1000)	

#define REALVIEW_EB_PROC_MASK		0xFF000000
#define REALVIEW_EB_PROC_ARM7TDMI	0x00000000
#define REALVIEW_EB_PROC_ARM9		0x02000000
#define REALVIEW_EB_PROC_ARM11		0x04000000
#define REALVIEW_EB_PROC_ARM11MP	0x06000000
#define REALVIEW_EB_PROC_A9MP		0x0C000000

#define check_eb_proc(proc_type)						\
	((readl(__io_address(REALVIEW_SYS_PROCID)) & REALVIEW_EB_PROC_MASK)	\
	 == proc_type)

#ifdef CONFIG_REALVIEW_EB_ARM11MP
#define core_tile_eb11mp()	check_eb_proc(REALVIEW_EB_PROC_ARM11MP)
#else
#define core_tile_eb11mp()	0
#endif

#ifdef CONFIG_REALVIEW_EB_A9MP
#define core_tile_a9mp()	check_eb_proc(REALVIEW_EB_PROC_A9MP)
#else
#define core_tile_a9mp()	0
#endif

#define machine_is_realview_eb_mp() \
	(machine_is_realview_eb() && (core_tile_eb11mp() || core_tile_a9mp()))

#endif	
