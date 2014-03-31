/*
 * arch/arm/mach-at91/include/mach/at91_twi.h
 *
 * Copyright (C) 2005 Ivan Kokshaysky
 * Copyright (C) SAN People
 *
 * Two-wire Interface (TWI) registers.
 * Based on AT91RM9200 datasheet revision E.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef AT91_TWI_H
#define AT91_TWI_H

#define	AT91_TWI_CR		0x00		
#define		AT91_TWI_START		(1 <<  0)	
#define		AT91_TWI_STOP		(1 <<  1)	
#define		AT91_TWI_MSEN		(1 <<  2)	
#define		AT91_TWI_MSDIS		(1 <<  3)	
#define		AT91_TWI_SVEN		(1 <<  4)	
#define		AT91_TWI_SVDIS		(1 <<  5)	
#define		AT91_TWI_SWRST		(1 <<  7)	

#define	AT91_TWI_MMR		0x04		
#define		AT91_TWI_IADRSZ		(3    <<  8)	
#define			AT91_TWI_IADRSZ_NO		(0 << 8)
#define			AT91_TWI_IADRSZ_1		(1 << 8)
#define			AT91_TWI_IADRSZ_2		(2 << 8)
#define			AT91_TWI_IADRSZ_3		(3 << 8)
#define		AT91_TWI_MREAD		(1    << 12)	
#define		AT91_TWI_DADR		(0x7f << 16)	

#define	AT91_TWI_SMR		0x08		
#define		AT91_TWI_SADR		(0x7f << 16)	

#define	AT91_TWI_IADR		0x0c		

#define	AT91_TWI_CWGR		0x10		
#define		AT91_TWI_CLDIV		(0xff <<  0)	
#define		AT91_TWI_CHDIV		(0xff <<  8)	
#define		AT91_TWI_CKDIV		(7    << 16)	

#define	AT91_TWI_SR		0x20		
#define		AT91_TWI_TXCOMP		(1 <<  0)	
#define		AT91_TWI_RXRDY		(1 <<  1)	
#define		AT91_TWI_TXRDY		(1 <<  2)	
#define		AT91_TWI_SVREAD		(1 <<  3)	
#define		AT91_TWI_SVACC		(1 <<  4)	
#define		AT91_TWI_GACC		(1 <<  5)	
#define		AT91_TWI_OVRE		(1 <<  6)	
#define		AT91_TWI_UNRE		(1 <<  7)	
#define		AT91_TWI_NACK		(1 <<  8)	
#define		AT91_TWI_ARBLST		(1 <<  9)	
#define		AT91_TWI_SCLWS		(1 << 10)	
#define		AT91_TWI_EOSACC		(1 << 11)	

#define	AT91_TWI_IER		0x24		
#define	AT91_TWI_IDR		0x28		
#define	AT91_TWI_IMR		0x2c		
#define	AT91_TWI_RHR		0x30		
#define	AT91_TWI_THR		0x34		

#endif

