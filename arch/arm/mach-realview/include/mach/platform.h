/*
 * arch/arm/mach-realview/include/mach/platform.h
 *
 * Copyright (c) ARM Limited 2003.  All rights reserved.
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

#ifndef __ASM_ARCH_PLATFORM_H
#define __ASM_ARCH_PLATFORM_H

#define REALVIEW_BOOT_ROM_LO          0x30000000		
#define REALVIEW_BOOT_ROM_HI          0x30000000
#define REALVIEW_BOOT_ROM_BASE        REALVIEW_BOOT_ROM_HI	 
#define REALVIEW_BOOT_ROM_SIZE        SZ_64M

#define REALVIEW_SSRAM_BASE           
#define REALVIEW_SSRAM_SIZE           SZ_2M

#define REALVIEW_SDRAM_BASE           0x00000000



#define REALVIEW_SYS_ID_OFFSET               0x00
#define REALVIEW_SYS_SW_OFFSET               0x04
#define REALVIEW_SYS_LED_OFFSET              0x08
#define REALVIEW_SYS_OSC0_OFFSET             0x0C

#define REALVIEW_SYS_OSC1_OFFSET             0x10
#define REALVIEW_SYS_OSC2_OFFSET             0x14
#define REALVIEW_SYS_OSC3_OFFSET             0x18
#define REALVIEW_SYS_OSC4_OFFSET             0x1C	

#define REALVIEW_SYS_LOCK_OFFSET             0x20
#define REALVIEW_SYS_100HZ_OFFSET            0x24
#define REALVIEW_SYS_CFGDATA1_OFFSET         0x28
#define REALVIEW_SYS_CFGDATA2_OFFSET         0x2C
#define REALVIEW_SYS_FLAGS_OFFSET            0x30
#define REALVIEW_SYS_FLAGSSET_OFFSET         0x30
#define REALVIEW_SYS_FLAGSCLR_OFFSET         0x34
#define REALVIEW_SYS_NVFLAGS_OFFSET          0x38
#define REALVIEW_SYS_NVFLAGSSET_OFFSET       0x38
#define REALVIEW_SYS_NVFLAGSCLR_OFFSET       0x3C
#define REALVIEW_SYS_RESETCTL_OFFSET         0x40
#define REALVIEW_SYS_PCICTL_OFFSET           0x44
#define REALVIEW_SYS_MCI_OFFSET              0x48
#define REALVIEW_SYS_FLASH_OFFSET            0x4C
#define REALVIEW_SYS_CLCD_OFFSET             0x50
#define REALVIEW_SYS_CLCDSER_OFFSET          0x54
#define REALVIEW_SYS_BOOTCS_OFFSET           0x58
#define REALVIEW_SYS_24MHz_OFFSET            0x5C
#define REALVIEW_SYS_MISC_OFFSET             0x60
#define REALVIEW_SYS_IOSEL_OFFSET            0x70
#define REALVIEW_SYS_PROCID_OFFSET           0x84
#define REALVIEW_SYS_TEST_OSC0_OFFSET        0xC0
#define REALVIEW_SYS_TEST_OSC1_OFFSET        0xC4
#define REALVIEW_SYS_TEST_OSC2_OFFSET        0xC8
#define REALVIEW_SYS_TEST_OSC3_OFFSET        0xCC
#define REALVIEW_SYS_TEST_OSC4_OFFSET        0xD0

#define REALVIEW_SYS_BASE                    0x10000000
#define REALVIEW_SYS_ID                      (REALVIEW_SYS_BASE + REALVIEW_SYS_ID_OFFSET)
#define REALVIEW_SYS_SW                      (REALVIEW_SYS_BASE + REALVIEW_SYS_SW_OFFSET)
#define REALVIEW_SYS_LED                     (REALVIEW_SYS_BASE + REALVIEW_SYS_LED_OFFSET)
#define REALVIEW_SYS_OSC0                    (REALVIEW_SYS_BASE + REALVIEW_SYS_OSC0_OFFSET)
#define REALVIEW_SYS_OSC1                    (REALVIEW_SYS_BASE + REALVIEW_SYS_OSC1_OFFSET)

#define REALVIEW_SYS_LOCK                    (REALVIEW_SYS_BASE + REALVIEW_SYS_LOCK_OFFSET)
#define REALVIEW_SYS_100HZ                   (REALVIEW_SYS_BASE + REALVIEW_SYS_100HZ_OFFSET)
#define REALVIEW_SYS_CFGDATA1                (REALVIEW_SYS_BASE + REALVIEW_SYS_CFGDATA1_OFFSET)
#define REALVIEW_SYS_CFGDATA2                (REALVIEW_SYS_BASE + REALVIEW_SYS_CFGDATA2_OFFSET)
#define REALVIEW_SYS_FLAGS                   (REALVIEW_SYS_BASE + REALVIEW_SYS_FLAGS_OFFSET)
#define REALVIEW_SYS_FLAGSSET                (REALVIEW_SYS_BASE + REALVIEW_SYS_FLAGSSET_OFFSET)
#define REALVIEW_SYS_FLAGSCLR                (REALVIEW_SYS_BASE + REALVIEW_SYS_FLAGSCLR_OFFSET)
#define REALVIEW_SYS_NVFLAGS                 (REALVIEW_SYS_BASE + REALVIEW_SYS_NVFLAGS_OFFSET)
#define REALVIEW_SYS_NVFLAGSSET              (REALVIEW_SYS_BASE + REALVIEW_SYS_NVFLAGSSET_OFFSET)
#define REALVIEW_SYS_NVFLAGSCLR              (REALVIEW_SYS_BASE + REALVIEW_SYS_NVFLAGSCLR_OFFSET)
#define REALVIEW_SYS_RESETCTL                (REALVIEW_SYS_BASE + REALVIEW_SYS_RESETCTL_OFFSET)
#define REALVIEW_SYS_PCICTL                  (REALVIEW_SYS_BASE + REALVIEW_SYS_PCICTL_OFFSET)
#define REALVIEW_SYS_MCI                     (REALVIEW_SYS_BASE + REALVIEW_SYS_MCI_OFFSET)
#define REALVIEW_SYS_FLASH                   (REALVIEW_SYS_BASE + REALVIEW_SYS_FLASH_OFFSET)
#define REALVIEW_SYS_CLCD                    (REALVIEW_SYS_BASE + REALVIEW_SYS_CLCD_OFFSET)
#define REALVIEW_SYS_CLCDSER                 (REALVIEW_SYS_BASE + REALVIEW_SYS_CLCDSER_OFFSET)
#define REALVIEW_SYS_BOOTCS                  (REALVIEW_SYS_BASE + REALVIEW_SYS_BOOTCS_OFFSET)
#define REALVIEW_SYS_24MHz                   (REALVIEW_SYS_BASE + REALVIEW_SYS_24MHz_OFFSET)
#define REALVIEW_SYS_MISC                    (REALVIEW_SYS_BASE + REALVIEW_SYS_MISC_OFFSET)
#define REALVIEW_SYS_IOSEL                   (REALVIEW_SYS_BASE + REALVIEW_SYS_IOSEL_OFFSET)
#define REALVIEW_SYS_PROCID                  (REALVIEW_SYS_BASE + REALVIEW_SYS_PROCID_OFFSET)
#define REALVIEW_SYS_TEST_OSC0               (REALVIEW_SYS_BASE + REALVIEW_SYS_TEST_OSC0_OFFSET)
#define REALVIEW_SYS_TEST_OSC1               (REALVIEW_SYS_BASE + REALVIEW_SYS_TEST_OSC1_OFFSET)
#define REALVIEW_SYS_TEST_OSC2               (REALVIEW_SYS_BASE + REALVIEW_SYS_TEST_OSC2_OFFSET)
#define REALVIEW_SYS_TEST_OSC3               (REALVIEW_SYS_BASE + REALVIEW_SYS_TEST_OSC3_OFFSET)
#define REALVIEW_SYS_TEST_OSC4               (REALVIEW_SYS_BASE + REALVIEW_SYS_TEST_OSC4_OFFSET)



#define REALVIEW_SYS_LOCK_LOCKED    (1 << 16)
#define REALVIEW_SYS_LOCK_VAL	0xA05F	       

#define REALVIEW_FLASHPROG_FLVPPEN	(1 << 0)	

#define REALVIEW_INTREG_WPROT        0x00    
#define REALVIEW_INTREG_RI0          0x01    
#define REALVIEW_INTREG_CARDIN       0x08    
                                                
#define REALVIEW_INTREG_RI1          0x02    
#define REALVIEW_INTREG_CARDINSERT   0x03    

#define REALVIEW_SCTL_BASE            0x10001000	
#define REALVIEW_I2C_BASE             0x10002000	
#define REALVIEW_AACI_BASE            0x10004000	
#define REALVIEW_MMCI0_BASE           0x10005000	
#define REALVIEW_KMI0_BASE            0x10006000	
#define REALVIEW_KMI1_BASE            0x10007000	
#define REALVIEW_CHAR_LCD_BASE        0x10008000	
#define REALVIEW_SCI_BASE             0x1000E000	
#define REALVIEW_GPIO1_BASE           0x10014000	
#define REALVIEW_GPIO2_BASE           0x10015000	
#define REALVIEW_DMC_BASE             0x10018000	
#define REALVIEW_DMAC_BASE            0x10030000	

#define REALVIEW_PCI_BASE             0x41000000	
#define REALVIEW_PCI_CFG_BASE	      0x42000000
#define REALVIEW_PCI_MEM_BASE0        0x44000000
#define REALVIEW_PCI_MEM_BASE1        0x50000000
#define REALVIEW_PCI_MEM_BASE2        0x60000000
#define REALVIEW_PCI_BASE_SIZE	       0x01000000
#define REALVIEW_PCI_CFG_BASE_SIZE    0x02000000
#define REALVIEW_PCI_MEM_BASE0_SIZE   0x0c000000	
#define REALVIEW_PCI_MEM_BASE1_SIZE   0x10000000	
#define REALVIEW_PCI_MEM_BASE2_SIZE   0x10000000	

#define REALVIEW_SDRAM67_BASE         0x70000000	
#define REALVIEW_LT_BASE              0x80000000	

#define REALVIEW_CF_BASE		0x18000000	
#define REALVIEW_CF_MEM_BASE		0x18003000	

#define REALVIEW_DOC_BASE             0x2C000000
#define REALVIEW_DOC_SIZE             (16 << 20)
#define REALVIEW_DOC_PAGE_SIZE        512
#define REALVIEW_DOC_TOTAL_PAGES     (DOC_SIZE / PAGE_SIZE)

#define ERASE_UNIT_PAGES    32
#define START_PAGE          0x80

#define REALVIEW_SYS_LED0             (1 << 0)
#define REALVIEW_SYS_LED1             (1 << 1)
#define REALVIEW_SYS_LED2             (1 << 2)
#define REALVIEW_SYS_LED3             (1 << 3)
#define REALVIEW_SYS_LED4             (1 << 4)
#define REALVIEW_SYS_LED5             (1 << 5)
#define REALVIEW_SYS_LED6             (1 << 6)
#define REALVIEW_SYS_LED7             (1 << 7)

#define ALL_LEDS                  0xFF

#define LED_BANK                  REALVIEW_SYS_LED

#define REALVIEW_IDFIELD_OFFSET	0x0	
#define REALVIEW_FLASHPROG_OFFSET	0x4	
#define REALVIEW_INTREG_OFFSET		0x8	
#define REALVIEW_DECODE_OFFSET		0xC	

#define REALVIEW_REFCLK	0
#define REALVIEW_TIMCLK	1

#define REALVIEW_TIMER1_EnSel	15
#define REALVIEW_TIMER2_EnSel	17
#define REALVIEW_TIMER3_EnSel	19
#define REALVIEW_TIMER4_EnSel	21


#define REALVIEW_CSR_BASE             0x10000000
#define REALVIEW_CSR_SIZE             0x10000000

#endif	
