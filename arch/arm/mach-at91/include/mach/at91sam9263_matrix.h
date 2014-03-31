/*
 * arch/arm/mach-at91/include/mach/at91sam9263_matrix.h
 *
 *  Copyright (C) 2006 Atmel Corporation.
 *
 * Memory Controllers (MATRIX, EBI) - System peripherals registers.
 * Based on AT91SAM9263 datasheet revision B (Preliminary).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef AT91SAM9263_MATRIX_H
#define AT91SAM9263_MATRIX_H

#define AT91_MATRIX_MCFG0	0x00			
#define AT91_MATRIX_MCFG1	0x04			
#define AT91_MATRIX_MCFG2	0x08			
#define AT91_MATRIX_MCFG3	0x0C			
#define AT91_MATRIX_MCFG4	0x10			
#define AT91_MATRIX_MCFG5	0x14			
#define AT91_MATRIX_MCFG6	0x18			
#define AT91_MATRIX_MCFG7	0x1C			
#define AT91_MATRIX_MCFG8	0x20			
#define		AT91_MATRIX_ULBT	(7 << 0)	
#define			AT91_MATRIX_ULBT_INFINITE	(0 << 0)
#define			AT91_MATRIX_ULBT_SINGLE		(1 << 0)
#define			AT91_MATRIX_ULBT_FOUR		(2 << 0)
#define			AT91_MATRIX_ULBT_EIGHT		(3 << 0)
#define			AT91_MATRIX_ULBT_SIXTEEN	(4 << 0)

#define AT91_MATRIX_SCFG0	0x40			
#define AT91_MATRIX_SCFG1	0x44			
#define AT91_MATRIX_SCFG2	0x48			
#define AT91_MATRIX_SCFG3	0x4C			
#define AT91_MATRIX_SCFG4	0x50			
#define AT91_MATRIX_SCFG5	0x54			
#define AT91_MATRIX_SCFG6	0x58			
#define AT91_MATRIX_SCFG7	0x5C			
#define		AT91_MATRIX_SLOT_CYCLE		(0xff << 0)	
#define		AT91_MATRIX_DEFMSTR_TYPE	(3    << 16)	
#define			AT91_MATRIX_DEFMSTR_TYPE_NONE	(0 << 16)
#define			AT91_MATRIX_DEFMSTR_TYPE_LAST	(1 << 16)
#define			AT91_MATRIX_DEFMSTR_TYPE_FIXED	(2 << 16)
#define		AT91_MATRIX_FIXED_DEFMSTR	(0xf  << 18)	
#define		AT91_MATRIX_ARBT		(3    << 24)	
#define			AT91_MATRIX_ARBT_ROUND_ROBIN	(0 << 24)
#define			AT91_MATRIX_ARBT_FIXED_PRIORITY	(1 << 24)

#define AT91_MATRIX_PRAS0	0x80			
#define AT91_MATRIX_PRBS0	0x84			
#define AT91_MATRIX_PRAS1	0x88			
#define AT91_MATRIX_PRBS1	0x8C			
#define AT91_MATRIX_PRAS2	0x90			
#define AT91_MATRIX_PRBS2	0x94			
#define AT91_MATRIX_PRAS3	0x98			
#define AT91_MATRIX_PRBS3	0x9C			
#define AT91_MATRIX_PRAS4	0xA0			
#define AT91_MATRIX_PRBS4	0xA4			
#define AT91_MATRIX_PRAS5	0xA8			
#define AT91_MATRIX_PRBS5	0xAC			
#define AT91_MATRIX_PRAS6	0xB0			
#define AT91_MATRIX_PRBS6	0xB4			
#define AT91_MATRIX_PRAS7	0xB8			
#define AT91_MATRIX_PRBS7	0xBC			
#define		AT91_MATRIX_M0PR		(3 << 0)	
#define		AT91_MATRIX_M1PR		(3 << 4)	
#define		AT91_MATRIX_M2PR		(3 << 8)	
#define		AT91_MATRIX_M3PR		(3 << 12)	
#define		AT91_MATRIX_M4PR		(3 << 16)	
#define		AT91_MATRIX_M5PR		(3 << 20)	
#define		AT91_MATRIX_M6PR		(3 << 24)	
#define		AT91_MATRIX_M7PR		(3 << 28)	
#define		AT91_MATRIX_M8PR		(3 << 0)	

#define AT91_MATRIX_MRCR	0x100			
#define		AT91_MATRIX_RCB0		(1 << 0)	
#define		AT91_MATRIX_RCB1		(1 << 1)	
#define		AT91_MATRIX_RCB2		(1 << 2)
#define		AT91_MATRIX_RCB3		(1 << 3)
#define		AT91_MATRIX_RCB4		(1 << 4)
#define		AT91_MATRIX_RCB5		(1 << 5)
#define		AT91_MATRIX_RCB6		(1 << 6)
#define		AT91_MATRIX_RCB7		(1 << 7)
#define		AT91_MATRIX_RCB8		(1 << 8)

#define AT91_MATRIX_TCMR	0x114			
#define		AT91_MATRIX_ITCM_SIZE		(0xf << 0)	
#define			AT91_MATRIX_ITCM_0		(0 << 0)
#define			AT91_MATRIX_ITCM_16		(5 << 0)
#define			AT91_MATRIX_ITCM_32		(6 << 0)
#define		AT91_MATRIX_DTCM_SIZE		(0xf << 4)	
#define			AT91_MATRIX_DTCM_0		(0 << 4)
#define			AT91_MATRIX_DTCM_16		(5 << 4)
#define			AT91_MATRIX_DTCM_32		(6 << 4)

#define AT91_MATRIX_EBI0CSA	0x120			
#define		AT91_MATRIX_EBI0_CS1A		(1 << 1)	
#define			AT91_MATRIX_EBI0_CS1A_SMC		(0 << 1)
#define			AT91_MATRIX_EBI0_CS1A_SDRAMC		(1 << 1)
#define		AT91_MATRIX_EBI0_CS3A		(1 << 3)	
#define			AT91_MATRIX_EBI0_CS3A_SMC		(0 << 3)
#define			AT91_MATRIX_EBI0_CS3A_SMC_SMARTMEDIA	(1 << 3)
#define		AT91_MATRIX_EBI0_CS4A		(1 << 4)	
#define			AT91_MATRIX_EBI0_CS4A_SMC		(0 << 4)
#define			AT91_MATRIX_EBI0_CS4A_SMC_CF1		(1 << 4)
#define		AT91_MATRIX_EBI0_CS5A		(1 << 5)	
#define			AT91_MATRIX_EBI0_CS5A_SMC		(0 << 5)
#define			AT91_MATRIX_EBI0_CS5A_SMC_CF2		(1 << 5)
#define		AT91_MATRIX_EBI0_DBPUC		(1 << 8)	
#define		AT91_MATRIX_EBI0_VDDIOMSEL	(1 << 16)	
#define			AT91_MATRIX_EBI0_VDDIOMSEL_1_8V		(0 << 16)
#define			AT91_MATRIX_EBI0_VDDIOMSEL_3_3V		(1 << 16)

#define AT91_MATRIX_EBI1CSA	0x124			
#define		AT91_MATRIX_EBI1_CS1A		(1 << 1)	
#define			AT91_MATRIX_EBI1_CS1A_SMC		(0 << 1)
#define			AT91_MATRIX_EBI1_CS1A_SDRAMC		(1 << 1)
#define		AT91_MATRIX_EBI1_CS2A		(1 << 3)	
#define			AT91_MATRIX_EBI1_CS2A_SMC		(0 << 3)
#define			AT91_MATRIX_EBI1_CS2A_SMC_SMARTMEDIA	(1 << 3)
#define		AT91_MATRIX_EBI1_DBPUC		(1 << 8)	
#define		AT91_MATRIX_EBI1_VDDIOMSEL	(1 << 16)	
#define			AT91_MATRIX_EBI1_VDDIOMSEL_1_8V		(0 << 16)
#define			AT91_MATRIX_EBI1_VDDIOMSEL_3_3V		(1 << 16)

#endif
