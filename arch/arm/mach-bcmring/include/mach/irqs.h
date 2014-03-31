/*
 *  Copyright (C) 2007 Broadcom
 *  Copyright (C) 1999 ARM Limited
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
 */

#if !defined(ARCH_BCMRING_IRQS_H)
#define ARCH_BCMRING_IRQS_H

#define IRQ_INTC0_START     0
#define IRQ_DMA0C0          0	
#define IRQ_DMA0C1          1	
#define IRQ_DMA0C2          2	
#define IRQ_DMA0C3          3	
#define IRQ_DMA0C4          4	
#define IRQ_DMA0C5          5	
#define IRQ_DMA0C6          6	
#define IRQ_DMA0C7          7	
#define IRQ_DMA1C0          8	
#define IRQ_DMA1C1          9	
#define IRQ_DMA1C2         10	
#define IRQ_DMA1C3         11	
#define IRQ_DMA1C4         12	
#define IRQ_DMA1C5         13	
#define IRQ_DMA1C6         14	
#define IRQ_DMA1C7         15	
#define IRQ_VPM            16	
#define IRQ_USBHD2         17	
#define IRQ_USBH1          18	
#define IRQ_USBD           19	
#define IRQ_SDIOH0         20	
#define IRQ_SDIOH1         21	
#define IRQ_TIMER0         22	
#define IRQ_TIMER1         23	
#define IRQ_TIMER2         24	
#define IRQ_TIMER3         25	
#define IRQ_SPIH           26	
#define IRQ_ESW            27	
#define IRQ_APM            28	
#define IRQ_GE             29	
#define IRQ_CLCD           30	
#define IRQ_PIF            31	
#define IRQ_INTC0_END      31

#define IRQ_INTC1_START    32
#define IRQ_GPIO0          32	
#define IRQ_GPIO1          33	
#define IRQ_I2S0           34	
#define IRQ_I2S1           35	
#define IRQ_I2CH           36	
#define IRQ_I2CS           37	
#define IRQ_SPIS           38	
#define IRQ_GPHY           39	
#define IRQ_FLASHC         40	
#define IRQ_COMMTX         41	
#define IRQ_COMMRX         42	
#define IRQ_PMUIRQ         43	
#define IRQ_UARTB          44	
#define IRQ_WATCHDOG       45	
#define IRQ_UARTA          46	
#define IRQ_TSC            47	
#define IRQ_KEYC           48	
#define IRQ_DMPU           49	
#define IRQ_VMPU           50	
#define IRQ_FMPU           51	
#define IRQ_RNG            52	
#define IRQ_RTC0           53	
#define IRQ_RTC1           54	
#define IRQ_SPUM           55	
#define IRQ_VDEC           56	
#define IRQ_RTC2           57	
#define IRQ_DDRP           58	
#define IRQ_INTC1_END      58

#define IRQ_SINTC_START    59
#define IRQ_SEC_WATCHDOG   59	
#define IRQ_SEC_UARTA      60	
#define IRQ_SEC_TSC        61	
#define IRQ_SEC_KEYC       62	
#define IRQ_SEC_DMPU       63	
#define IRQ_SEC_VMPU       64	
#define IRQ_SEC_FMPU       65	
#define IRQ_SEC_RNG        66	
#define IRQ_SEC_RTC0       67	
#define IRQ_SEC_RTC1       68	
#define IRQ_SEC_SPUM       69	
#define IRQ_SEC_TIMER0     70	
#define IRQ_SEC_TIMER1     71	
#define IRQ_SEC_TIMER2     72	
#define IRQ_SEC_TIMER3     73	
#define IRQ_SEC_RTC2       74	

#define IRQ_SINTC_END      74


#define IRQ_GPIO_0                  100

#define NUM_INTERNAL_IRQS          (IRQ_SINTC_END+1)

#define NUM_GPIO_IRQS               62

#define NR_IRQS                     (IRQ_GPIO_0 + NUM_GPIO_IRQS)

#define IRQ_UNKNOWN                 -1

#define IRQ_INTC0_VALID_MASK        0xffffffff
#define IRQ_INTC1_VALID_MASK        0x07ffffff
#define IRQ_SINTC_VALID_MASK        0x0000ffff

#endif 
