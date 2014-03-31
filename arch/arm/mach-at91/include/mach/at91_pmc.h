/*
 * arch/arm/mach-at91/include/mach/at91_pmc.h
 *
 * Copyright (C) 2005 Ivan Kokshaysky
 * Copyright (C) SAN People
 *
 * Power Management Controller (PMC) - System peripherals registers.
 * Based on AT91RM9200 datasheet revision E.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef AT91_PMC_H
#define AT91_PMC_H

#ifndef __ASSEMBLY__
extern void __iomem *at91_pmc_base;

#define at91_pmc_read(field) \
	__raw_readl(at91_pmc_base + field)

#define at91_pmc_write(field, value) \
	__raw_writel(value, at91_pmc_base + field)
#else
.extern at91_pmc_base
#endif

#define	AT91_PMC_SCER		0x00			
#define	AT91_PMC_SCDR		0x04			

#define	AT91_PMC_SCSR		0x08			
#define		AT91_PMC_PCK		(1 <<  0)		
#define		AT91RM9200_PMC_UDP	(1 <<  1)		
#define		AT91RM9200_PMC_MCKUDP	(1 <<  2)		
#define		AT91RM9200_PMC_UHP	(1 <<  4)		
#define		AT91SAM926x_PMC_UHP	(1 <<  6)		
#define		AT91SAM926x_PMC_UDP	(1 <<  7)		
#define		AT91_PMC_PCK0		(1 <<  8)		
#define		AT91_PMC_PCK1		(1 <<  9)		
#define		AT91_PMC_PCK2		(1 << 10)		
#define		AT91_PMC_PCK3		(1 << 11)		
#define		AT91_PMC_PCK4		(1 << 12)		
#define		AT91_PMC_HCK0		(1 << 16)		
#define		AT91_PMC_HCK1		(1 << 17)		

#define	AT91_PMC_PCER		0x10			
#define	AT91_PMC_PCDR		0x14			
#define	AT91_PMC_PCSR		0x18			

#define	AT91_CKGR_UCKR		0x1C			
#define		AT91_PMC_UPLLEN		(1   << 16)		
#define		AT91_PMC_UPLLCOUNT	(0xf << 20)		
#define		AT91_PMC_BIASEN		(1   << 24)		
#define		AT91_PMC_BIASCOUNT	(0xf << 28)		

#define	AT91_CKGR_MOR		0x20			
#define		AT91_PMC_MOSCEN		(1    <<  0)		
#define		AT91_PMC_OSCBYPASS	(1    <<  1)		
#define		AT91_PMC_MOSCRCEN	(1    <<  3)		
#define		AT91_PMC_OSCOUNT	(0xff <<  8)		
#define		AT91_PMC_KEY		(0x37 << 16)		
#define		AT91_PMC_MOSCSEL	(1    << 24)		
#define		AT91_PMC_CFDEN		(1    << 25)		

#define	AT91_CKGR_MCFR		0x24			
#define		AT91_PMC_MAINF		(0xffff <<  0)		
#define		AT91_PMC_MAINRDY	(1	<< 16)		

#define	AT91_CKGR_PLLAR		0x28			
#define	AT91_CKGR_PLLBR		0x2c			
#define		AT91_PMC_DIV		(0xff  <<  0)		
#define		AT91_PMC_PLLCOUNT	(0x3f  <<  8)		
#define		AT91_PMC_OUT		(3     << 14)		
#define		AT91_PMC_MUL		(0x7ff << 16)		
#define		AT91_PMC_USBDIV		(3     << 28)		
#define			AT91_PMC_USBDIV_1		(0 << 28)
#define			AT91_PMC_USBDIV_2		(1 << 28)
#define			AT91_PMC_USBDIV_4		(2 << 28)
#define		AT91_PMC_USB96M		(1     << 28)		

#define	AT91_PMC_MCKR		0x30			
#define		AT91_PMC_CSS		(3 <<  0)		
#define			AT91_PMC_CSS_SLOW		(0 << 0)
#define			AT91_PMC_CSS_MAIN		(1 << 0)
#define			AT91_PMC_CSS_PLLA		(2 << 0)
#define			AT91_PMC_CSS_PLLB		(3 << 0)
#define			AT91_PMC_CSS_UPLL		(3 << 0)	
#define		PMC_PRES_OFFSET		2
#define		AT91_PMC_PRES		(7 <<  PMC_PRES_OFFSET)		
#define			AT91_PMC_PRES_1			(0 << PMC_PRES_OFFSET)
#define			AT91_PMC_PRES_2			(1 << PMC_PRES_OFFSET)
#define			AT91_PMC_PRES_4			(2 << PMC_PRES_OFFSET)
#define			AT91_PMC_PRES_8			(3 << PMC_PRES_OFFSET)
#define			AT91_PMC_PRES_16		(4 << PMC_PRES_OFFSET)
#define			AT91_PMC_PRES_32		(5 << PMC_PRES_OFFSET)
#define			AT91_PMC_PRES_64		(6 << PMC_PRES_OFFSET)
#define		PMC_ALT_PRES_OFFSET	4
#define		AT91_PMC_ALT_PRES	(7 <<  PMC_ALT_PRES_OFFSET)		
#define			AT91_PMC_ALT_PRES_1		(0 << PMC_ALT_PRES_OFFSET)
#define			AT91_PMC_ALT_PRES_2		(1 << PMC_ALT_PRES_OFFSET)
#define			AT91_PMC_ALT_PRES_4		(2 << PMC_ALT_PRES_OFFSET)
#define			AT91_PMC_ALT_PRES_8		(3 << PMC_ALT_PRES_OFFSET)
#define			AT91_PMC_ALT_PRES_16		(4 << PMC_ALT_PRES_OFFSET)
#define			AT91_PMC_ALT_PRES_32		(5 << PMC_ALT_PRES_OFFSET)
#define			AT91_PMC_ALT_PRES_64		(6 << PMC_ALT_PRES_OFFSET)
#define		AT91_PMC_MDIV		(3 <<  8)		
#define			AT91RM9200_PMC_MDIV_1		(0 << 8)	
#define			AT91RM9200_PMC_MDIV_2		(1 << 8)
#define			AT91RM9200_PMC_MDIV_3		(2 << 8)
#define			AT91RM9200_PMC_MDIV_4		(3 << 8)
#define			AT91SAM9_PMC_MDIV_1		(0 << 8)	
#define			AT91SAM9_PMC_MDIV_2		(1 << 8)
#define			AT91SAM9_PMC_MDIV_4		(2 << 8)
#define			AT91SAM9_PMC_MDIV_6		(3 << 8)	
#define			AT91SAM9_PMC_MDIV_3		(3 << 8)	
#define		AT91_PMC_PDIV		(1 << 12)		
#define			AT91_PMC_PDIV_1			(0 << 12)
#define			AT91_PMC_PDIV_2			(1 << 12)
#define		AT91_PMC_PLLADIV2	(1 << 12)		
#define			AT91_PMC_PLLADIV2_OFF		(0 << 12)
#define			AT91_PMC_PLLADIV2_ON		(1 << 12)

#define	AT91_PMC_USB		0x38			
#define		AT91_PMC_USBS		(0x1 <<  0)		
#define			AT91_PMC_USBS_PLLA		(0 << 0)
#define			AT91_PMC_USBS_UPLL		(1 << 0)
#define		AT91_PMC_OHCIUSBDIV	(0xF <<  8)		

#define	AT91_PMC_SMD		0x3c			
#define		AT91_PMC_SMDS		(0x1  <<  0)		
#define		AT91_PMC_SMD_DIV	(0x1f <<  8)		
#define		AT91_PMC_SMDDIV(n)	(((n) <<  8) & AT91_PMC_SMD_DIV)

#define	AT91_PMC_PCKR(n)	(0x40 + ((n) * 4))	
#define		AT91_PMC_ALT_PCKR_CSS	(0x7 <<  0)		
#define			AT91_PMC_CSS_MASTER		(4 << 0)	
#define		AT91_PMC_CSSMCK		(0x1 <<  8)		
#define			AT91_PMC_CSSMCK_CSS		(0 << 8)
#define			AT91_PMC_CSSMCK_MCK		(1 << 8)

#define	AT91_PMC_IER		0x60			
#define	AT91_PMC_IDR		0x64			
#define	AT91_PMC_SR		0x68			
#define		AT91_PMC_MOSCS		(1 <<  0)		
#define		AT91_PMC_LOCKA		(1 <<  1)		
#define		AT91_PMC_LOCKB		(1 <<  2)		
#define		AT91_PMC_MCKRDY		(1 <<  3)		
#define		AT91_PMC_LOCKU		(1 <<  6)		
#define		AT91_PMC_PCK0RDY	(1 <<  8)		
#define		AT91_PMC_PCK1RDY	(1 <<  9)		
#define		AT91_PMC_PCK2RDY	(1 << 10)		
#define		AT91_PMC_PCK3RDY	(1 << 11)		
#define		AT91_PMC_MOSCSELS	(1 << 16)		
#define		AT91_PMC_MOSCRCS	(1 << 17)		
#define		AT91_PMC_CFDEV		(1 << 18)		
#define	AT91_PMC_IMR		0x6c			

#define AT91_PMC_PROT		0xe4			
#define		AT91_PMC_WPEN		(0x1  <<  0)		
#define		AT91_PMC_WPKEY		(0xffffff << 8)		
#define		AT91_PMC_PROTKEY	(0x504d43 << 8)		

#define AT91_PMC_WPSR		0xe8			
#define		AT91_PMC_WPVS		(0x1  <<  0)		
#define		AT91_PMC_WPVSRC		(0xffff  <<  8)		

#define AT91_PMC_PCR		0x10c			
#define		AT91_PMC_PCR_PID	(0x3f  <<  0)		
#define		AT91_PMC_PCR_CMD	(0x1  <<  12)		
#define		AT91_PMC_PCR_DIV	(0x3  <<  16)		
#define		AT91_PMC_PCRDIV(n)	(((n) <<  16) & AT91_PMC_PCR_DIV)
#define		AT91_PMC_PCR_EN		(0x1  <<  28)		

#endif
