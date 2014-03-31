/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2013, The Linux Foundation. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * The MSM peripherals are spread all over across 768MB of physical
 * space, which makes just having a simple IO_ADDRESS macro to slide
 * them into the right virtual location rough.  Instead, we will
 * provide a master phys->virt mapping for peripherals here.
 *
 */

#ifndef __ASM_ARCH_MSM_IOMAP_H
#define __ASM_ARCH_MSM_IOMAP_H

#include <asm/sizes.h>


#define MSM_DEBUG_UART_SIZE	SZ_4K

#if defined(CONFIG_DEBUG_MSM_UART1) || defined(CONFIG_DEBUG_MSM_UART2) \
				|| defined(CONFIG_DEBUG_MSM_UART3)
#define MSM_DEBUG_UART_BASE	0xFC000000
#define MSM_DEBUG_UART_PHYS	CONFIG_MSM_DEBUG_UART_PHYS
#endif

#define MSM8625_WARM_BOOT_PHYS  0x0FD00000

#if defined(CONFIG_ARCH_QSD8X50)
#include "msm_iomap-8x50.h"
#elif defined(CONFIG_ARCH_MSM8X60)
#include "msm_iomap-8x60.h"
#elif defined(CONFIG_ARCH_FSM9XXX)
#include "msm_iomap-fsm9xxx.h"
#else


#define MSM_TMR_BASE		IOMEM(0xFA000000)	
#define MSM_TMR0_BASE		IOMEM(0xFA001000)	
#define MSM_QGIC_DIST_BASE	IOMEM(0xFA002000)	
#define MSM_QGIC_CPU_BASE	IOMEM(0xFA003000)	
#define MSM_TCSR_BASE		IOMEM(0xFA004000)	
#define MSM_APCS_GCC_BASE	IOMEM(0xFA006000)	
#define MSM_SAW_L2_BASE		IOMEM(0xFA007000)	
#define MSM_SAW0_BASE		IOMEM(0xFA008000)	
#define MSM_SAW1_BASE		IOMEM(0xFA009000)	

#ifndef CONFIG_ARCH_MSM9625
#define MSM_IMEM_BASE		IOMEM(0xFA00A000)	
#endif

#define MSM_ACC0_BASE		IOMEM(0xFA00B000)	
#define MSM_ACC1_BASE		IOMEM(0xFA00C000)	
#define MSM_ACC2_BASE		IOMEM(0xFA00D000)	
#define MSM_ACC3_BASE		IOMEM(0xFA00E000)	
#define MSM_CLK_CTL_BASE	IOMEM(0xFA010000)	
#define MSM_MMSS_CLK_CTL_BASE	IOMEM(0xFA014000)	
#define MSM_LPASS_CLK_CTL_BASE	IOMEM(0xFA015000)	
#define MSM_HFPLL_BASE		IOMEM(0xFA016000)	
#define MSM_TLMM_BASE		IOMEM(0xFA017000)	
#define MSM_VIC_BASE		IOMEM(0xFA100000)	
#define MSM_CSR_BASE		IOMEM(0xFA101000)	
#define MSM_GPIO1_BASE		IOMEM(0xFA102000)	
#define MSM_GPIO2_BASE		IOMEM(0xFA103000)	
#define MSM_SCU_BASE		IOMEM(0xFA104000)	
#define MSM_CFG_CTL_BASE	IOMEM(0xFA105000)	
#define MSM_CLK_CTL_SH2_BASE	IOMEM(0xFA106000)	
#define MSM_MPM2_PSHOLD_BASE	IOMEM(0xFA107000)	
#define MSM_SHARED_RAM_BASE	IOMEM(0xFA400000)	
#define MSM_SIC_NON_SECURE_BASE	IOMEM(0xFA600000)	
#define MSM_QFPROM_BASE		IOMEM(0xFA700000)	
#define MSM_L2CC_BASE		IOMEM(0xFA701000)	
#define MSM_APCS_GLB_BASE	IOMEM(0xFA702000)	
#define MSM_SAW2_BASE		IOMEM(0xFA703000)	
#define MSM_SAW3_BASE		IOMEM(0xFA704000)	
#define MSM_HDMI_BASE		IOMEM(0xFA800000)	
#define MSM_RPM_BASE		IOMEM(0xFA801000)	
#define MSM_RPM_MPM_BASE	IOMEM(0xFA802000)	
#define MSM_AD5_BASE		IOMEM(0xFA900000)	
#ifdef CONFIG_ARCH_MSM9625
#define MSM_IMEM_BASE		IOMEM(0xFB601800)	
#endif

#define MSM_STRONGLY_ORDERED_PAGE	0xFA0F0000
#define MSM8625_CPU_PHYS		0x0FE00000


#if defined(CONFIG_ARCH_MSM9615) || defined(CONFIG_ARCH_MSM7X27) \
	|| defined(CONFIG_ARCH_MSM7X30) || defined(CONFIG_ARCH_MSM9625) \
	|| defined(CONFIG_ARCH_MSM8610) || defined(CONFIG_ARCH_MSM8226) \
	|| defined(CONFIG_ARCH_MSMKRYPTON)
#define MSM_SHARED_RAM_SIZE	SZ_1M
#else
#define MSM_SHARED_RAM_SIZE	SZ_2M
#endif

#include "msm_iomap-7xxx.h"
#include "msm_iomap-7x30.h"
#include "msm_iomap-8625.h"
#include "msm_iomap-8960.h"
#include "msm_iomap-8930.h"
#include "msm_iomap-8064.h"
#include "msm_iomap-9615.h"
#include "msm_iomap-8974.h"
#include "msm_iomap-8084.h"
#include "msm_iomap-9625.h"
#include "msm_iomap-8092.h"
#include "msm_iomap-8226.h"
#include "msm_iomap-8610.h"
#include "msm_iomap-krypton.h"
#include "msm_iomap-fsm9900.h"
#include "msm_iomap-samarium.h"

#endif

#endif
