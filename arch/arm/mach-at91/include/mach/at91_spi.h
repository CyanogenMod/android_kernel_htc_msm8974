/*
 * arch/arm/mach-at91/include/mach/at91_spi.h
 *
 * Copyright (C) 2005 Ivan Kokshaysky
 * Copyright (C) SAN People
 *
 * Serial Peripheral Interface (SPI) registers.
 * Based on AT91RM9200 datasheet revision E.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef AT91_SPI_H
#define AT91_SPI_H

#define AT91_SPI_CR			0x00		
#define		AT91_SPI_SPIEN		(1 <<  0)		
#define		AT91_SPI_SPIDIS		(1 <<  1)		
#define		AT91_SPI_SWRST		(1 <<  7)		
#define		AT91_SPI_LASTXFER	(1 << 24)		

#define AT91_SPI_MR			0x04		
#define		AT91_SPI_MSTR		(1    <<  0)		
#define		AT91_SPI_PS		(1    <<  1)		
#define			AT91_SPI_PS_FIXED	(0 << 1)
#define			AT91_SPI_PS_VARIABLE	(1 << 1)
#define		AT91_SPI_PCSDEC		(1    <<  2)		
#define		AT91_SPI_DIV32		(1    <<  3)		
#define		AT91_SPI_MODFDIS	(1    <<  4)		
#define		AT91_SPI_LLB		(1    <<  7)		
#define		AT91_SPI_PCS		(0xf  << 16)		
#define		AT91_SPI_DLYBCS		(0xff << 24)		

#define AT91_SPI_RDR		0x08			
#define		AT91_SPI_RD		(0xffff <<  0)		
#define		AT91_SPI_PCS		(0xf	<< 16)		

#define AT91_SPI_TDR		0x0c			
#define		AT91_SPI_TD		(0xffff <<  0)		
#define		AT91_SPI_PCS		(0xf	<< 16)		
#define		AT91_SPI_LASTXFER	(1	<< 24)		

#define AT91_SPI_SR		0x10			
#define		AT91_SPI_RDRF		(1 <<  0)		
#define		AT91_SPI_TDRE		(1 <<  1)		
#define		AT91_SPI_MODF		(1 <<  2)		
#define		AT91_SPI_OVRES		(1 <<  3)		
#define		AT91_SPI_ENDRX		(1 <<  4)		
#define		AT91_SPI_ENDTX		(1 <<  5)		
#define		AT91_SPI_RXBUFF		(1 <<  6)		
#define		AT91_SPI_TXBUFE		(1 <<  7)		
#define		AT91_SPI_NSSR		(1 <<  8)		
#define		AT91_SPI_TXEMPTY	(1 <<  9)		
#define		AT91_SPI_SPIENS		(1 << 16)		

#define AT91_SPI_IER		0x14			
#define AT91_SPI_IDR		0x18			
#define AT91_SPI_IMR		0x1c			

#define AT91_SPI_CSR(n)		(0x30 + ((n) * 4))	
#define		AT91_SPI_CPOL		(1    <<  0)		
#define		AT91_SPI_NCPHA		(1    <<  1)		
#define		AT91_SPI_CSAAT		(1    <<  3)		
#define		AT91_SPI_BITS		(0xf  <<  4)		
#define			AT91_SPI_BITS_8		(0 << 4)
#define			AT91_SPI_BITS_9		(1 << 4)
#define			AT91_SPI_BITS_10	(2 << 4)
#define			AT91_SPI_BITS_11	(3 << 4)
#define			AT91_SPI_BITS_12	(4 << 4)
#define			AT91_SPI_BITS_13	(5 << 4)
#define			AT91_SPI_BITS_14	(6 << 4)
#define			AT91_SPI_BITS_15	(7 << 4)
#define			AT91_SPI_BITS_16	(8 << 4)
#define		AT91_SPI_SCBR		(0xff <<  8)		
#define		AT91_SPI_DLYBS		(0xff << 16)		
#define		AT91_SPI_DLYBCT		(0xff << 24)		

#endif
