/*
 * arch/arm/mach-ks8695/include/mach/regs-uart.h
 *
 * Copyright (C) 2006 Ben Dooks <ben@simtec.co.uk>
 * Copyright (C) 2006 Simtec Electronics
 *
 * KS8695 - UART register and bit definitions.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef KS8695_UART_H
#define KS8695_UART_H

#define KS8695_UART_OFFSET	(0xF0000 + 0xE000)
#define KS8695_UART_VA		(KS8695_IO_VA + KS8695_UART_OFFSET)
#define KS8695_UART_PA		(KS8695_IO_PA + KS8695_UART_OFFSET)


#define KS8695_URRB	(0x00)		
#define KS8695_URTH	(0x04)		
#define KS8695_URFC	(0x08)		
#define KS8695_URLC	(0x0C)		
#define KS8695_URMC	(0x10)		
#define KS8695_URLS	(0x14)		
#define KS8695_URMS	(0x18)		
#define KS8695_URBD	(0x1C)		
#define KS8695_USR	(0x20)		


#define URFC_URFRT	(3 << 6)	
#define		URFC_URFRT_1	(0 << 6)
#define		URFC_URFRT_4	(1 << 6)
#define		URFC_URFRT_8	(2 << 6)
#define		URFC_URFRT_14	(3 << 6)
#define URFC_URTFR	(1 << 2)	
#define URFC_URRFR	(1 << 1)	
#define URFC_URFE	(1 << 0)	

#define URLC_URSBC	(1 << 6)	
#define URLC_PARITY	(7 << 3)	
#define		URPE_NONE	(0 << 3)
#define		URPE_ODD	(1 << 3)
#define		URPE_EVEN	(3 << 3)
#define		URPE_MARK	(5 << 3)
#define		URPE_SPACE	(7 << 3)
#define URLC_URSB	(1 << 2)	
#define URLC_URCL	(3 << 0)	
#define		URCL_5		(0 << 0)
#define		URCL_6		(1 << 0)
#define		URCL_7		(2 << 0)
#define		URCL_8		(3 << 0)

#define URMC_URLB	(1 << 4)	
#define URMC_UROUT2	(1 << 3)	
#define URMC_UROUT1	(1 << 2)	
#define URMC_URRTS	(1 << 1)	
#define URMC_URDTR	(1 << 0)	

#define URLS_URRFE	(1 << 7)	
#define URLS_URTE	(1 << 6)	
#define URLS_URTHRE	(1 << 5)	
#define URLS_URBI	(1 << 4)	
#define URLS_URFE	(1 << 3)	
#define URLS_URPE	(1 << 2)	
#define URLS_URROE	(1 << 1)	
#define URLS_URDR	(1 << 0)	

#define URMS_URDCD	(1 << 7)	
#define URMS_URRI	(1 << 6)	
#define URMS_URDSR	(1 << 5)	
#define URMS_URCTS	(1 << 4)	
#define URMS_URDDCD	(1 << 3)	
#define URMS_URTERI	(1 << 2)	
#define URMS_URDDST	(1 << 1)	
#define URMS_URDCTS	(1 << 0)	

#define USR_UTI		(1 << 0)	


#endif
