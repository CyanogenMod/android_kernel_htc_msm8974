/* MN10300 on-board serial port module registers
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */

#ifndef _ASM_SERIAL_REGS_H
#define _ASM_SERIAL_REGS_H

#include <asm/cpu-regs.h>
#include <asm/intctl-regs.h>

#ifdef __KERNEL__

#define	SC0CTR			__SYSREG(0xd4002000, u16)	
#define	SC01CTR_CK		0x0007	
#define	SC01CTR_CK_IOCLK_8	0x0001	
#define	SC01CTR_CK_IOCLK_32	0x0002	
#define	SC01CTR_CK_EXTERN_8	0x0006	
#define	SC01CTR_CK_EXTERN	0x0007	
#if defined(CONFIG_AM33_2) || defined(CONFIG_AM33_3)
#define	SC0CTR_CK_TM8UFLOW_8	0x0000	
#define	SC0CTR_CK_TM2UFLOW_2	0x0003	
#define	SC0CTR_CK_TM0UFLOW_8	0x0004	
#define	SC0CTR_CK_TM2UFLOW_8	0x0005	
#define	SC1CTR_CK_TM9UFLOW_8	0x0000	
#define	SC1CTR_CK_TM3UFLOW_2	0x0003	
#define	SC1CTR_CK_TM1UFLOW_8	0x0004	
#define	SC1CTR_CK_TM3UFLOW_8	0x0005	
#else  
#define	SC0CTR_CK_TM8UFLOW_8	0x0000	
#define	SC0CTR_CK_TM0UFLOW_8	0x0004	
#define	SC0CTR_CK_TM2UFLOW_8	0x0005	
#define	SC1CTR_CK_TM12UFLOW_8	0x0000	
#endif 
#define	SC01CTR_STB		0x0008	
#define	SC01CTR_STB_1BIT	0x0000	
#define	SC01CTR_STB_2BIT	0x0008	
#define	SC01CTR_PB		0x0070	
#define	SC01CTR_PB_NONE		0x0000	
#define	SC01CTR_PB_FIXED0	0x0040	
#define	SC01CTR_PB_FIXED1	0x0050	
#define	SC01CTR_PB_EVEN		0x0060	
#define	SC01CTR_PB_ODD		0x0070	
#define	SC01CTR_CLN		0x0080	
#define	SC01CTR_CLN_7BIT	0x0000	
#define	SC01CTR_CLN_8BIT	0x0080	
#define	SC01CTR_TOE		0x0100	
#define	SC01CTR_OD		0x0200	
#define	SC01CTR_OD_LSBFIRST	0x0000	
#define	SC01CTR_OD_MSBFIRST	0x0200	
#define	SC01CTR_MD		0x0c00	
#define SC01CTR_MD_STST_SYNC	0x0000	
#define SC01CTR_MD_CLOCK_SYNC1	0x0400	
#define SC01CTR_MD_I2C		0x0800	
#define SC01CTR_MD_CLOCK_SYNC2	0x0c00	
#define	SC01CTR_IIC		0x1000	
#define	SC01CTR_BKE		0x2000	
#define	SC01CTR_RXE		0x4000	
#define	SC01CTR_TXE		0x8000	

#define	SC0ICR			__SYSREG(0xd4002004, u8)	
#define SC01ICR_DMD		0x80	
#define SC01ICR_TD		0x20	
#define SC01ICR_TI		0x10	
#define SC01ICR_RES		0x04	
#define SC01ICR_RI		0x01	

#define	SC0TXB			__SYSREG(0xd4002008, u8)	
#define	SC0RXB			__SYSREG(0xd4002009, u8)	

#define	SC0STR			__SYSREG(0xd400200c, u16)	
#define SC01STR_OEF		0x0001	
#define SC01STR_PEF		0x0002	
#define SC01STR_FEF		0x0004	
#define SC01STR_RBF		0x0010	
#define SC01STR_TBF		0x0020	
#define SC01STR_RXF		0x0040	
#define SC01STR_TXF		0x0080	
#define SC01STR_STF		0x0100	
#define SC01STR_SPF		0x0200	

#define SC0RXIRQ		20	
#define SC0TXIRQ		21	

#define	SC0RXICR		GxICR(SC0RXIRQ)	
#define	SC0TXICR		GxICR(SC0TXIRQ)	

#define	SC1CTR			__SYSREG(0xd4002010, u16)	
#define	SC1ICR			__SYSREG(0xd4002014, u8)	
#define	SC1TXB			__SYSREG(0xd4002018, u8)	
#define	SC1RXB			__SYSREG(0xd4002019, u8)	
#define	SC1STR			__SYSREG(0xd400201c, u16)	

#define SC1RXIRQ		22	
#define SC1TXIRQ		23	

#define	SC1RXICR		GxICR(SC1RXIRQ)	
#define	SC1TXICR		GxICR(SC1TXIRQ)	

#define	SC2CTR			__SYSREG(0xd4002020, u16)	
#ifdef CONFIG_AM33_2
#define	SC2CTR_CK		0x0003	
#define	SC2CTR_CK_TM10UFLOW	0x0000	
#define	SC2CTR_CK_TM2UFLOW	0x0001	
#define	SC2CTR_CK_EXTERN	0x0002	
#define	SC2CTR_CK_TM3UFLOW	0x0003	
#else  
#define	SC2CTR_CK		0x0007	
#define	SC2CTR_CK_TM9UFLOW_8	0x0000	
#define	SC2CTR_CK_IOCLK_8	0x0001	
#define	SC2CTR_CK_IOCLK_32	0x0002	
#define	SC2CTR_CK_TM3UFLOW_2	0x0003	
#define	SC2CTR_CK_TM1UFLOW_8	0x0004	
#define	SC2CTR_CK_TM3UFLOW_8	0x0005	
#define	SC2CTR_CK_EXTERN_8	0x0006	
#define	SC2CTR_CK_EXTERN	0x0007	
#endif 
#define	SC2CTR_STB		0x0008	
#define	SC2CTR_STB_1BIT		0x0000	
#define	SC2CTR_STB_2BIT		0x0008	
#define	SC2CTR_PB		0x0070	
#define	SC2CTR_PB_NONE		0x0000	
#define	SC2CTR_PB_FIXED0	0x0040	
#define	SC2CTR_PB_FIXED1	0x0050	
#define	SC2CTR_PB_EVEN		0x0060	
#define	SC2CTR_PB_ODD		0x0070	
#define	SC2CTR_CLN		0x0080	
#define	SC2CTR_CLN_7BIT		0x0000	
#define	SC2CTR_CLN_8BIT		0x0080	
#define	SC2CTR_TWE		0x0100	
#define	SC2CTR_OD		0x0200	
#define	SC2CTR_OD_LSBFIRST	0x0000	
#define	SC2CTR_OD_MSBFIRST	0x0200	
#define	SC2CTR_TWS		0x1000	
#define	SC2CTR_TWS_XCTS_HIGH	0x0000	
#define	SC2CTR_TWS_XCTS_LOW	0x1000	
#define	SC2CTR_BKE		0x2000	
#define	SC2CTR_RXE		0x4000	
#define	SC2CTR_TXE		0x8000	

#define	SC2ICR			__SYSREG(0xd4002024, u8)	
#define SC2ICR_TD		0x20	
#define SC2ICR_TI		0x10	
#define SC2ICR_RES		0x04	
#define SC2ICR_RI		0x01	

#define	SC2TXB			__SYSREG(0xd4002028, u8)	
#define	SC2RXB			__SYSREG(0xd4002029, u8)	

#ifdef CONFIG_AM33_2
#define	SC2STR			__SYSREG(0xd400202c, u8)	
#else  
#define	SC2STR			__SYSREG(0xd400202c, u16)	
#endif 
#define SC2STR_OEF		0x0001	
#define SC2STR_PEF		0x0002	
#define SC2STR_FEF		0x0004	
#define SC2STR_CTS		0x0008	
#define SC2STR_RBF		0x0010	
#define SC2STR_TBF		0x0020	
#define SC2STR_RXF		0x0040	
#define SC2STR_TXF		0x0080	

#ifdef CONFIG_AM33_2
#define	SC2TIM			__SYSREG(0xd400202d, u8)	
#endif

#ifdef CONFIG_AM33_2
#define SC2RXIRQ		24	
#define SC2TXIRQ		25	
#else  
#define SC2RXIRQ		68	
#define SC2TXIRQ		69	
#endif 

#define	SC2RXICR		GxICR(SC2RXIRQ)	
#define	SC2TXICR		GxICR(SC2TXIRQ)	


#endif 

#endif 
