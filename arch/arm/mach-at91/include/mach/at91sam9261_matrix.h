/*
 * arch/arm/mach-at91/include/mach/at91sam9261_matrix.h
 *
 *  Copyright (C) 2007 Atmel Corporation.
 *
 * Memory Controllers (MATRIX, EBI) - System peripherals registers.
 * Based on AT91SAM9261 datasheet revision D.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef AT91SAM9261_MATRIX_H
#define AT91SAM9261_MATRIX_H

#define AT91_MATRIX_MCFG	0x00			
#define		AT91_MATRIX_RCB0	(1 << 0)		
#define		AT91_MATRIX_RCB1	(1 << 1)		

#define AT91_MATRIX_SCFG0	0x04			
#define AT91_MATRIX_SCFG1	0x08			
#define AT91_MATRIX_SCFG2	0x0C			
#define AT91_MATRIX_SCFG3	0x10			
#define AT91_MATRIX_SCFG4	0x14			
#define		AT91_MATRIX_SLOT_CYCLE		(0xff << 0)	
#define		AT91_MATRIX_DEFMSTR_TYPE	(3    << 16)	
#define			AT91_MATRIX_DEFMSTR_TYPE_NONE	(0 << 16)
#define			AT91_MATRIX_DEFMSTR_TYPE_LAST	(1 << 16)
#define			AT91_MATRIX_DEFMSTR_TYPE_FIXED	(2 << 16)
#define		AT91_MATRIX_FIXED_DEFMSTR	(7    << 18)	

#define AT91_MATRIX_TCR		0x24			
#define		AT91_MATRIX_ITCM_SIZE		(0xf << 0)	
#define			AT91_MATRIX_ITCM_0		(0 << 0)
#define			AT91_MATRIX_ITCM_16		(5 << 0)
#define			AT91_MATRIX_ITCM_32		(6 << 0)
#define			AT91_MATRIX_ITCM_64		(7 << 0)
#define		AT91_MATRIX_DTCM_SIZE		(0xf << 4)	
#define			AT91_MATRIX_DTCM_0		(0 << 4)
#define			AT91_MATRIX_DTCM_16		(5 << 4)
#define			AT91_MATRIX_DTCM_32		(6 << 4)
#define			AT91_MATRIX_DTCM_64		(7 << 4)

#define AT91_MATRIX_EBICSA	0x30			
#define		AT91_MATRIX_CS1A		(1 << 1)	
#define			AT91_MATRIX_CS1A_SMC		(0 << 1)
#define			AT91_MATRIX_CS1A_SDRAMC		(1 << 1)
#define		AT91_MATRIX_CS3A		(1 << 3)	
#define			AT91_MATRIX_CS3A_SMC		(0 << 3)
#define			AT91_MATRIX_CS3A_SMC_SMARTMEDIA	(1 << 3)
#define		AT91_MATRIX_CS4A		(1 << 4)	
#define			AT91_MATRIX_CS4A_SMC		(0 << 4)
#define			AT91_MATRIX_CS4A_SMC_CF1	(1 << 4)
#define		AT91_MATRIX_CS5A		(1 << 5)	
#define			AT91_MATRIX_CS5A_SMC		(0 << 5)
#define			AT91_MATRIX_CS5A_SMC_CF2	(1 << 5)
#define		AT91_MATRIX_DBPUC		(1 << 8)	

#define AT91_MATRIX_USBPUCR	0x34			
#define		AT91_MATRIX_USBPUCR_PUON	(1 << 30)	

#endif
