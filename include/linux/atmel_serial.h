/*
 * include/linux/atmel_serial.h
 *
 * Copyright (C) 2005 Ivan Kokshaysky
 * Copyright (C) SAN People
 *
 * USART registers.
 * Based on AT91RM9200 datasheet revision E.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef ATMEL_SERIAL_H
#define ATMEL_SERIAL_H

#define ATMEL_US_CR		0x00			
#define		ATMEL_US_RSTRX		(1 <<  2)		
#define		ATMEL_US_RSTTX		(1 <<  3)		
#define		ATMEL_US_RXEN		(1 <<  4)		
#define		ATMEL_US_RXDIS		(1 <<  5)		
#define		ATMEL_US_TXEN		(1 <<  6)		
#define		ATMEL_US_TXDIS		(1 <<  7)		
#define		ATMEL_US_RSTSTA		(1 <<  8)		
#define		ATMEL_US_STTBRK		(1 <<  9)		
#define		ATMEL_US_STPBRK		(1 << 10)		
#define		ATMEL_US_STTTO		(1 << 11)		
#define		ATMEL_US_SENDA		(1 << 12)		
#define		ATMEL_US_RSTIT		(1 << 13)		
#define		ATMEL_US_RSTNACK	(1 << 14)		
#define		ATMEL_US_RETTO		(1 << 15)		
#define		ATMEL_US_DTREN		(1 << 16)		
#define		ATMEL_US_DTRDIS		(1 << 17)		
#define		ATMEL_US_RTSEN		(1 << 18)		
#define		ATMEL_US_RTSDIS		(1 << 19)		

#define ATMEL_US_MR		0x04			
#define		ATMEL_US_USMODE		(0xf <<  0)		
#define			ATMEL_US_USMODE_NORMAL		0
#define			ATMEL_US_USMODE_RS485		1
#define			ATMEL_US_USMODE_HWHS		2
#define			ATMEL_US_USMODE_MODEM		3
#define			ATMEL_US_USMODE_ISO7816_T0	4
#define			ATMEL_US_USMODE_ISO7816_T1	6
#define			ATMEL_US_USMODE_IRDA		8
#define		ATMEL_US_USCLKS		(3   <<  4)		
#define			ATMEL_US_USCLKS_MCK		(0 <<  4)
#define			ATMEL_US_USCLKS_MCK_DIV8	(1 <<  4)
#define			ATMEL_US_USCLKS_SCK		(3 <<  4)
#define		ATMEL_US_CHRL		(3   <<  6)		
#define			ATMEL_US_CHRL_5			(0 <<  6)
#define			ATMEL_US_CHRL_6			(1 <<  6)
#define			ATMEL_US_CHRL_7			(2 <<  6)
#define			ATMEL_US_CHRL_8			(3 <<  6)
#define		ATMEL_US_SYNC		(1 <<  8)		
#define		ATMEL_US_PAR		(7 <<  9)		
#define			ATMEL_US_PAR_EVEN		(0 <<  9)
#define			ATMEL_US_PAR_ODD		(1 <<  9)
#define			ATMEL_US_PAR_SPACE		(2 <<  9)
#define			ATMEL_US_PAR_MARK		(3 <<  9)
#define			ATMEL_US_PAR_NONE		(4 <<  9)
#define			ATMEL_US_PAR_MULTI_DROP		(6 <<  9)
#define		ATMEL_US_NBSTOP		(3 << 12)		
#define			ATMEL_US_NBSTOP_1		(0 << 12)
#define			ATMEL_US_NBSTOP_1_5		(1 << 12)
#define			ATMEL_US_NBSTOP_2		(2 << 12)
#define		ATMEL_US_CHMODE		(3 << 14)		
#define			ATMEL_US_CHMODE_NORMAL		(0 << 14)
#define			ATMEL_US_CHMODE_ECHO		(1 << 14)
#define			ATMEL_US_CHMODE_LOC_LOOP	(2 << 14)
#define			ATMEL_US_CHMODE_REM_LOOP	(3 << 14)
#define		ATMEL_US_MSBF		(1 << 16)		
#define		ATMEL_US_MODE9		(1 << 17)		
#define		ATMEL_US_CLKO		(1 << 18)		
#define		ATMEL_US_OVER		(1 << 19)		
#define		ATMEL_US_INACK		(1 << 20)		
#define		ATMEL_US_DSNACK		(1 << 21)		
#define		ATMEL_US_MAX_ITER	(7 << 24)		
#define		ATMEL_US_FILTER		(1 << 28)		

#define ATMEL_US_IER		0x08			
#define		ATMEL_US_RXRDY		(1 <<  0)		
#define		ATMEL_US_TXRDY		(1 <<  1)		
#define		ATMEL_US_RXBRK		(1 <<  2)		
#define		ATMEL_US_ENDRX		(1 <<  3)		
#define		ATMEL_US_ENDTX		(1 <<  4)		
#define		ATMEL_US_OVRE		(1 <<  5)		
#define		ATMEL_US_FRAME		(1 <<  6)		
#define		ATMEL_US_PARE		(1 <<  7)		
#define		ATMEL_US_TIMEOUT	(1 <<  8)		
#define		ATMEL_US_TXEMPTY	(1 <<  9)		
#define		ATMEL_US_ITERATION	(1 << 10)		
#define		ATMEL_US_TXBUFE		(1 << 11)		
#define		ATMEL_US_RXBUFF		(1 << 12)		
#define		ATMEL_US_NACK		(1 << 13)		
#define		ATMEL_US_RIIC		(1 << 16)		
#define		ATMEL_US_DSRIC		(1 << 17)		
#define		ATMEL_US_DCDIC		(1 << 18)		
#define		ATMEL_US_CTSIC		(1 << 19)		
#define		ATMEL_US_RI		(1 << 20)		
#define		ATMEL_US_DSR		(1 << 21)		
#define		ATMEL_US_DCD		(1 << 22)		
#define		ATMEL_US_CTS		(1 << 23)		

#define ATMEL_US_IDR		0x0c			
#define ATMEL_US_IMR		0x10			
#define ATMEL_US_CSR		0x14			
#define ATMEL_US_RHR		0x18			
#define ATMEL_US_THR		0x1c			
#define		ATMEL_US_SYNH		(1 << 15)		

#define ATMEL_US_BRGR		0x20			
#define		ATMEL_US_CD		(0xffff << 0)		

#define ATMEL_US_RTOR		0x24			
#define		ATMEL_US_TO		(0xffff << 0)		

#define ATMEL_US_TTGR		0x28			
#define		ATMEL_US_TG		(0xff << 0)		

#define ATMEL_US_FIDI		0x40			
#define ATMEL_US_NER		0x44			
#define ATMEL_US_IF		0x4c			

#endif
