/*****************************************************************************
* Copyright 2003 - 2008 Broadcom Corporation.  All rights reserved.
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


#ifndef _INTCHW_REG_H
#define _INTCHW_REG_H

#include <csp/stdint.h>
#include <csp/reg.h>
#include <mach/csp/mm_io.h>


#define INTCHW_NUM_IRQ_PER_INTC   32	
#define INTCHW_NUM_INTC           3

#define INTCHW_INTC0    ((void *)MM_IO_BASE_INTC0)
#define INTCHW_INTC1    ((void *)MM_IO_BASE_INTC1)
#define INTCHW_SINTC    ((void *)MM_IO_BASE_SINTC)

#define INTCHW_INTC0_PIF_BITNUM           31	
#define INTCHW_INTC0_CLCD_BITNUM          30	
#define INTCHW_INTC0_GE_BITNUM            29	
#define INTCHW_INTC0_APM_BITNUM           28	
#define INTCHW_INTC0_ESW_BITNUM           27	
#define INTCHW_INTC0_SPIH_BITNUM          26	
#define INTCHW_INTC0_TIMER3_BITNUM        25	
#define INTCHW_INTC0_TIMER2_BITNUM        24	
#define INTCHW_INTC0_TIMER1_BITNUM        23	
#define INTCHW_INTC0_TIMER0_BITNUM        22	
#define INTCHW_INTC0_SDIOH1_BITNUM        21	
#define INTCHW_INTC0_SDIOH0_BITNUM        20	
#define INTCHW_INTC0_USBD_BITNUM          19	
#define INTCHW_INTC0_USBH1_BITNUM         18	
#define INTCHW_INTC0_USBHD2_BITNUM        17	
#define INTCHW_INTC0_VPM_BITNUM           16	
#define INTCHW_INTC0_DMA1C7_BITNUM        15	
#define INTCHW_INTC0_DMA1C6_BITNUM        14	
#define INTCHW_INTC0_DMA1C5_BITNUM        13	
#define INTCHW_INTC0_DMA1C4_BITNUM        12	
#define INTCHW_INTC0_DMA1C3_BITNUM        11	
#define INTCHW_INTC0_DMA1C2_BITNUM        10	
#define INTCHW_INTC0_DMA1C1_BITNUM         9	
#define INTCHW_INTC0_DMA1C0_BITNUM         8	
#define INTCHW_INTC0_DMA0C7_BITNUM         7	
#define INTCHW_INTC0_DMA0C6_BITNUM         6	
#define INTCHW_INTC0_DMA0C5_BITNUM         5	
#define INTCHW_INTC0_DMA0C4_BITNUM         4	
#define INTCHW_INTC0_DMA0C3_BITNUM         3	
#define INTCHW_INTC0_DMA0C2_BITNUM         2	
#define INTCHW_INTC0_DMA0C1_BITNUM         1	
#define INTCHW_INTC0_DMA0C0_BITNUM         0	

#define INTCHW_INTC0_PIF                  (1<<INTCHW_INTC0_PIF_BITNUM)
#define INTCHW_INTC0_CLCD                 (1<<INTCHW_INTC0_CLCD_BITNUM)
#define INTCHW_INTC0_GE                   (1<<INTCHW_INTC0_GE_BITNUM)
#define INTCHW_INTC0_APM                  (1<<INTCHW_INTC0_APM_BITNUM)
#define INTCHW_INTC0_ESW                  (1<<INTCHW_INTC0_ESW_BITNUM)
#define INTCHW_INTC0_SPIH                 (1<<INTCHW_INTC0_SPIH_BITNUM)
#define INTCHW_INTC0_TIMER3               (1<<INTCHW_INTC0_TIMER3_BITNUM)
#define INTCHW_INTC0_TIMER2               (1<<INTCHW_INTC0_TIMER2_BITNUM)
#define INTCHW_INTC0_TIMER1               (1<<INTCHW_INTC0_TIMER1_BITNUM)
#define INTCHW_INTC0_TIMER0               (1<<INTCHW_INTC0_TIMER0_BITNUM)
#define INTCHW_INTC0_SDIOH1               (1<<INTCHW_INTC0_SDIOH1_BITNUM)
#define INTCHW_INTC0_SDIOH0               (1<<INTCHW_INTC0_SDIOH0_BITNUM)
#define INTCHW_INTC0_USBD                 (1<<INTCHW_INTC0_USBD_BITNUM)
#define INTCHW_INTC0_USBH1                (1<<INTCHW_INTC0_USBH1_BITNUM)
#define INTCHW_INTC0_USBHD2               (1<<INTCHW_INTC0_USBHD2_BITNUM)
#define INTCHW_INTC0_VPM                  (1<<INTCHW_INTC0_VPM_BITNUM)
#define INTCHW_INTC0_DMA1C7               (1<<INTCHW_INTC0_DMA1C7_BITNUM)
#define INTCHW_INTC0_DMA1C6               (1<<INTCHW_INTC0_DMA1C6_BITNUM)
#define INTCHW_INTC0_DMA1C5               (1<<INTCHW_INTC0_DMA1C5_BITNUM)
#define INTCHW_INTC0_DMA1C4               (1<<INTCHW_INTC0_DMA1C4_BITNUM)
#define INTCHW_INTC0_DMA1C3               (1<<INTCHW_INTC0_DMA1C3_BITNUM)
#define INTCHW_INTC0_DMA1C2               (1<<INTCHW_INTC0_DMA1C2_BITNUM)
#define INTCHW_INTC0_DMA1C1               (1<<INTCHW_INTC0_DMA1C1_BITNUM)
#define INTCHW_INTC0_DMA1C0               (1<<INTCHW_INTC0_DMA1C0_BITNUM)
#define INTCHW_INTC0_DMA0C7               (1<<INTCHW_INTC0_DMA0C7_BITNUM)
#define INTCHW_INTC0_DMA0C6               (1<<INTCHW_INTC0_DMA0C6_BITNUM)
#define INTCHW_INTC0_DMA0C5               (1<<INTCHW_INTC0_DMA0C5_BITNUM)
#define INTCHW_INTC0_DMA0C4               (1<<INTCHW_INTC0_DMA0C4_BITNUM)
#define INTCHW_INTC0_DMA0C3               (1<<INTCHW_INTC0_DMA0C3_BITNUM)
#define INTCHW_INTC0_DMA0C2               (1<<INTCHW_INTC0_DMA0C2_BITNUM)
#define INTCHW_INTC0_DMA0C1               (1<<INTCHW_INTC0_DMA0C1_BITNUM)
#define INTCHW_INTC0_DMA0C0               (1<<INTCHW_INTC0_DMA0C0_BITNUM)

#define INTCHW_INTC1_DDRVPMP_BITNUM       27	
#define INTCHW_INTC1_DDRVPMT_BITNUM       26	
#define INTCHW_INTC1_DDRP_BITNUM          26	
#define INTCHW_INTC1_RTC2_BITNUM          25	
#define INTCHW_INTC1_VDEC_BITNUM          24	
#define INTCHW_INTC1_SPUM_BITNUM          23	
#define INTCHW_INTC1_RTC1_BITNUM          22	
#define INTCHW_INTC1_RTC0_BITNUM          21	
#define INTCHW_INTC1_RNG_BITNUM           20	
#define INTCHW_INTC1_FMPU_BITNUM          19	
#define INTCHW_INTC1_VMPU_BITNUM          18	
#define INTCHW_INTC1_DMPU_BITNUM          17	
#define INTCHW_INTC1_KEYC_BITNUM          16	
#define INTCHW_INTC1_TSC_BITNUM           15	
#define INTCHW_INTC1_UART0_BITNUM         14	
#define INTCHW_INTC1_WDOG_BITNUM          13	

#define INTCHW_INTC1_UART1_BITNUM         12	
#define INTCHW_INTC1_PMUIRQ_BITNUM        11	
#define INTCHW_INTC1_COMMRX_BITNUM        10	
#define INTCHW_INTC1_COMMTX_BITNUM         9	
#define INTCHW_INTC1_FLASHC_BITNUM         8	
#define INTCHW_INTC1_GPHY_BITNUM           7	
#define INTCHW_INTC1_SPIS_BITNUM           6	
#define INTCHW_INTC1_I2CS_BITNUM           5	
#define INTCHW_INTC1_I2CH_BITNUM           4	
#define INTCHW_INTC1_I2S1_BITNUM           3	
#define INTCHW_INTC1_I2S0_BITNUM           2	
#define INTCHW_INTC1_GPIO1_BITNUM          1	
#define INTCHW_INTC1_GPIO0_BITNUM          0	

#define INTCHW_INTC1_DDRVPMT              (1<<INTCHW_INTC1_DDRVPMT_BITNUM)
#define INTCHW_INTC1_DDRVPMP              (1<<INTCHW_INTC1_DDRVPMP_BITNUM)
#define INTCHW_INTC1_DDRP                 (1<<INTCHW_INTC1_DDRP_BITNUM)
#define INTCHW_INTC1_VDEC                 (1<<INTCHW_INTC1_VDEC_BITNUM)
#define INTCHW_INTC1_SPUM                 (1<<INTCHW_INTC1_SPUM_BITNUM)
#define INTCHW_INTC1_RTC2                 (1<<INTCHW_INTC1_RTC2_BITNUM)
#define INTCHW_INTC1_RTC1                 (1<<INTCHW_INTC1_RTC1_BITNUM)
#define INTCHW_INTC1_RTC0                 (1<<INTCHW_INTC1_RTC0_BITNUM)
#define INTCHW_INTC1_RNG                  (1<<INTCHW_INTC1_RNG_BITNUM)
#define INTCHW_INTC1_FMPU                 (1<<INTCHW_INTC1_FMPU_BITNUM)
#define INTCHW_INTC1_IMPU                 (1<<INTCHW_INTC1_IMPU_BITNUM)
#define INTCHW_INTC1_DMPU                 (1<<INTCHW_INTC1_DMPU_BITNUM)
#define INTCHW_INTC1_KEYC                 (1<<INTCHW_INTC1_KEYC_BITNUM)
#define INTCHW_INTC1_TSC                  (1<<INTCHW_INTC1_TSC_BITNUM)
#define INTCHW_INTC1_UART0                (1<<INTCHW_INTC1_UART0_BITNUM)
#define INTCHW_INTC1_WDOG                 (1<<INTCHW_INTC1_WDOG_BITNUM)
#define INTCHW_INTC1_UART1                (1<<INTCHW_INTC1_UART1_BITNUM)
#define INTCHW_INTC1_PMUIRQ               (1<<INTCHW_INTC1_PMUIRQ_BITNUM)
#define INTCHW_INTC1_COMMRX               (1<<INTCHW_INTC1_COMMRX_BITNUM)
#define INTCHW_INTC1_COMMTX               (1<<INTCHW_INTC1_COMMTX_BITNUM)
#define INTCHW_INTC1_FLASHC               (1<<INTCHW_INTC1_FLASHC_BITNUM)
#define INTCHW_INTC1_GPHY                 (1<<INTCHW_INTC1_GPHY_BITNUM)
#define INTCHW_INTC1_SPIS                 (1<<INTCHW_INTC1_SPIS_BITNUM)
#define INTCHW_INTC1_I2CS                 (1<<INTCHW_INTC1_I2CS_BITNUM)
#define INTCHW_INTC1_I2CH                 (1<<INTCHW_INTC1_I2CH_BITNUM)
#define INTCHW_INTC1_I2S1                 (1<<INTCHW_INTC1_I2S1_BITNUM)
#define INTCHW_INTC1_I2S0                 (1<<INTCHW_INTC1_I2S0_BITNUM)
#define INTCHW_INTC1_GPIO1                (1<<INTCHW_INTC1_GPIO1_BITNUM)
#define INTCHW_INTC1_GPIO0                (1<<INTCHW_INTC1_GPIO0_BITNUM)

#define INTCHW_SINTC_RTC2_BITNUM          15	
#define INTCHW_SINTC_TIMER3_BITNUM        14	
#define INTCHW_SINTC_TIMER2_BITNUM        13	
#define INTCHW_SINTC_TIMER1_BITNUM        12	
#define INTCHW_SINTC_TIMER0_BITNUM        11	
#define INTCHW_SINTC_SPUM_BITNUM          10	
#define INTCHW_SINTC_RTC1_BITNUM           9	
#define INTCHW_SINTC_RTC0_BITNUM           8	
#define INTCHW_SINTC_RNG_BITNUM            7	
#define INTCHW_SINTC_FMPU_BITNUM           6	
#define INTCHW_SINTC_VMPU_BITNUM           5	
#define INTCHW_SINTC_DMPU_BITNUM           4	
#define INTCHW_SINTC_KEYC_BITNUM           3	
#define INTCHW_SINTC_TSC_BITNUM            2	
#define INTCHW_SINTC_UART0_BITNUM          1	
#define INTCHW_SINTC_WDOG_BITNUM           0	

#define INTCHW_SINTC_TIMER3               (1<<INTCHW_SINTC_TIMER3_BITNUM)
#define INTCHW_SINTC_TIMER2               (1<<INTCHW_SINTC_TIMER2_BITNUM)
#define INTCHW_SINTC_TIMER1               (1<<INTCHW_SINTC_TIMER1_BITNUM)
#define INTCHW_SINTC_TIMER0               (1<<INTCHW_SINTC_TIMER0_BITNUM)
#define INTCHW_SINTC_SPUM                 (1<<INTCHW_SINTC_SPUM_BITNUM)
#define INTCHW_SINTC_RTC2                 (1<<INTCHW_SINTC_RTC2_BITNUM)
#define INTCHW_SINTC_RTC1                 (1<<INTCHW_SINTC_RTC1_BITNUM)
#define INTCHW_SINTC_RTC0                 (1<<INTCHW_SINTC_RTC0_BITNUM)
#define INTCHW_SINTC_RNG                  (1<<INTCHW_SINTC_RNG_BITNUM)
#define INTCHW_SINTC_FMPU                 (1<<INTCHW_SINTC_FMPU_BITNUM)
#define INTCHW_SINTC_IMPU                 (1<<INTCHW_SINTC_IMPU_BITNUM)
#define INTCHW_SINTC_DMPU                 (1<<INTCHW_SINTC_DMPU_BITNUM)
#define INTCHW_SINTC_KEYC                 (1<<INTCHW_SINTC_KEYC_BITNUM)
#define INTCHW_SINTC_TSC                  (1<<INTCHW_SINTC_TSC_BITNUM)
#define INTCHW_SINTC_UART0                (1<<INTCHW_SINTC_UART0_BITNUM)
#define INTCHW_SINTC_WDOG                 (1<<INTCHW_SINTC_WDOG_BITNUM)

#define INTCHW_IRQSTATUS      0x00	
#define INTCHW_FIQSTATUS      0x04	
#define INTCHW_RAWINTR        0x08	
#define INTCHW_INTSELECT      0x0c	
#define INTCHW_INTENABLE      0x10	
#define INTCHW_INTENCLEAR     0x14	
#define INTCHW_SOFTINT        0x18	
#define INTCHW_SOFTINTCLEAR   0x1c	
#define INTCHW_PROTECTION     0x20	
#define INTCHW_SWPRIOMASK     0x24	
#define INTCHW_PRIODAISY      0x28	
#define INTCHW_VECTADDR0      0x100	
#define INTCHW_VECTPRIO0      0x200	
#define INTCHW_ADDRESS        0xf00	
#define INTCHW_PID            0xfe0	
#define INTCHW_PCELLID        0xff0	


static inline void intcHw_irq_disable(void *basep, uint32_t mask)
{
	__REG32(basep + INTCHW_INTENCLEAR) = mask;
}

static inline void intcHw_irq_enable(void *basep, uint32_t mask)
{
	__REG32(basep + INTCHW_INTENABLE) = mask;
}

#endif 
