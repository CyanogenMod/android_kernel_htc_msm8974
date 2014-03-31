/*
 * arch/arm/mach-at91/include/mach/at91_rtc.h
 *
 * Copyright (C) 2005 Ivan Kokshaysky
 * Copyright (C) SAN People
 *
 * Real Time Clock (RTC) - System peripheral registers.
 * Based on AT91RM9200 datasheet revision E.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef AT91_RTC_H
#define AT91_RTC_H

#define	AT91_RTC_CR		0x00			
#define		AT91_RTC_UPDTIM		(1 <<  0)		
#define		AT91_RTC_UPDCAL		(1 <<  1)		
#define		AT91_RTC_TIMEVSEL	(3 <<  8)		
#define			AT91_RTC_TIMEVSEL_MINUTE	(0 << 8)
#define			AT91_RTC_TIMEVSEL_HOUR		(1 << 8)
#define			AT91_RTC_TIMEVSEL_DAY24		(2 << 8)
#define			AT91_RTC_TIMEVSEL_DAY12		(3 << 8)
#define		AT91_RTC_CALEVSEL	(3 << 16)		
#define			AT91_RTC_CALEVSEL_WEEK		(0 << 16)
#define			AT91_RTC_CALEVSEL_MONTH		(1 << 16)
#define			AT91_RTC_CALEVSEL_YEAR		(2 << 16)

#define	AT91_RTC_MR		0x04			
#define			AT91_RTC_HRMOD		(1 <<  0)		

#define	AT91_RTC_TIMR		0x08			
#define		AT91_RTC_SEC		(0x7f <<  0)		
#define		AT91_RTC_MIN		(0x7f <<  8)		
#define		AT91_RTC_HOUR		(0x3f << 16)		
#define		AT91_RTC_AMPM		(1    << 22)		

#define	AT91_RTC_CALR		0x0c			
#define		AT91_RTC_CENT		(0x7f <<  0)		
#define		AT91_RTC_YEAR		(0xff <<  8)		
#define		AT91_RTC_MONTH		(0x1f << 16)		
#define		AT91_RTC_DAY		(7    << 21)		
#define		AT91_RTC_DATE		(0x3f << 24)		

#define	AT91_RTC_TIMALR		0x10			
#define		AT91_RTC_SECEN		(1 <<  7)		
#define		AT91_RTC_MINEN		(1 << 15)		
#define		AT91_RTC_HOUREN		(1 << 23)		

#define	AT91_RTC_CALALR		0x14			
#define		AT91_RTC_MTHEN		(1 << 23)		
#define		AT91_RTC_DATEEN		(1 << 31)		

#define	AT91_RTC_SR		0x18			
#define		AT91_RTC_ACKUPD		(1 <<  0)		
#define		AT91_RTC_ALARM		(1 <<  1)		
#define		AT91_RTC_SECEV		(1 <<  2)		
#define		AT91_RTC_TIMEV		(1 <<  3)		
#define		AT91_RTC_CALEV		(1 <<  4)		

#define	AT91_RTC_SCCR		0x1c			
#define	AT91_RTC_IER		0x20			
#define	AT91_RTC_IDR		0x24			
#define	AT91_RTC_IMR		0x28			

#define	AT91_RTC_VER		0x2c			
#define		AT91_RTC_NVTIM		(1 <<  0)		
#define		AT91_RTC_NVCAL		(1 <<  1)		
#define		AT91_RTC_NVTIMALR	(1 <<  2)		
#define		AT91_RTC_NVCALALR	(1 <<  3)		

#endif
