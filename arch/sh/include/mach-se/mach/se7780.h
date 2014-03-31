#ifndef __ASM_SH_SE7780_H
#define __ASM_SH_SE7780_H

/*
 * linux/include/asm-sh/se7780.h
 *
 * Copyright (C) 2006,2007  Nobuhiro Iwamatsu
 *
 * Hitachi UL SolutionEngine 7780 Support.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <asm/addrspace.h>

#define SE_AREA0_WIDTH	4		
#define PA_ROM		0xa0000000	
#define PA_ROM_SIZE	0x00400000	
#define PA_FROM		0xa1000000	
#define PA_FROM_SIZE	0x01000000	
#define PA_EXT1		0xa4000000
#define PA_EXT1_SIZE	0x04000000
#define PA_SM501	PA_EXT1		
#define PA_SM501_SIZE	PA_EXT1_SIZE	
#define PA_SDRAM	0xa8000000	
#define PA_SDRAM_SIZE	0x08000000

#define PA_EXT4		0xb0000000
#define PA_EXT4_SIZE	0x04000000
#define PA_EXT_FLASH	PA_EXT4		

#define PA_PERIPHERAL	PA_AREA6_IO	

#define PA_LAN		(PA_PERIPHERAL + 0)		
#define PA_LED_DISP	(PA_PERIPHERAL + 0x02000000)	
#define DISP_CHAR_RAM	(7 << 3)
#define DISP_SEL0_ADDR	(DISP_CHAR_RAM + 0)
#define DISP_SEL1_ADDR	(DISP_CHAR_RAM + 1)
#define DISP_SEL2_ADDR	(DISP_CHAR_RAM + 2)
#define DISP_SEL3_ADDR	(DISP_CHAR_RAM + 3)
#define DISP_SEL4_ADDR	(DISP_CHAR_RAM + 4)
#define DISP_SEL5_ADDR	(DISP_CHAR_RAM + 5)
#define DISP_SEL6_ADDR	(DISP_CHAR_RAM + 6)
#define DISP_SEL7_ADDR	(DISP_CHAR_RAM + 7)

#define DISP_UDC_RAM	(5 << 3)
#define PA_FPGA		(PA_PERIPHERAL + 0x03000000) 

#define FPGA_SFTRST		(PA_FPGA + 0)	
#define FPGA_INTMSK1		(PA_FPGA + 2)	
#define FPGA_INTMSK2		(PA_FPGA + 4)	
#define FPGA_INTSEL1		(PA_FPGA + 6)	
#define FPGA_INTSEL2		(PA_FPGA + 8)	
#define FPGA_INTSEL3		(PA_FPGA + 10)	
#define FPGA_PCI_INTSEL1	(PA_FPGA + 12)	
#define FPGA_PCI_INTSEL2	(PA_FPGA + 14)	
#define FPGA_INTSET		(PA_FPGA + 16)	
#define FPGA_INTSTS1		(PA_FPGA + 18)	
#define FPGA_INTSTS2		(PA_FPGA + 20)	
#define FPGA_REQSEL		(PA_FPGA + 22)	
#define FPGA_DBG_LED		(PA_FPGA + 32)	
#define PA_LED			FPGA_DBG_LED
#define FPGA_IVDRID		(PA_FPGA + 36)	
#define FPGA_IVDRPW		(PA_FPGA + 38)	
#define FPGA_MMCID		(PA_FPGA + 40)	

#define IRQPOS_SMC91CX          (0 * 4)
#define IRQPOS_SM501            (1 * 4)
#define IRQPOS_EXTINT1          (0 * 4)
#define IRQPOS_EXTINT2          (1 * 4)
#define IRQPOS_EXTINT3          (2 * 4)
#define IRQPOS_EXTINT4          (3 * 4)
#define IRQPOS_PCCPW            (0 * 4)

#define IRQ_IDE0                67 

#define SMC_IRQ                 8

#define SM501_IRQ               0

#define IRQPIN_EXTINT1          0 
#define IRQPIN_EXTINT2          1 
#define IRQPIN_EXTINT3          2 
#define IRQPIN_SMC91CX          3 
#define IRQPIN_EXTINT4          4 
#define IRQPIN_PCC0             5 
#define IRQPIN_PCC2             6 
#define IRQPIN_SM501            7 
#define IRQPIN_PCCPW            7 

void init_se7780_IRQ(void);

#define __IO_PREFIX		se7780
#include <asm/io_generic.h>

#endif  
