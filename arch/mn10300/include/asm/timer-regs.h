/* AM33v2 on-board timer module registers
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */

#ifndef _ASM_TIMER_REGS_H
#define _ASM_TIMER_REGS_H

#include <asm/cpu-regs.h>
#include <asm/intctl-regs.h>

#ifdef __KERNEL__

#define	TMPSCNT			__SYSREG(0xd4003071, u8) 
#define	TMPSCNT_ENABLE		0x80	
#define	TMPSCNT_DISABLE		0x00	

#define	TM0MD			__SYSREG(0xd4003000, u8) 
#define	TM0MD_SRC		0x07	
#define	TM0MD_SRC_IOCLK		0x00	
#define	TM0MD_SRC_IOCLK_8	0x01	
#define	TM0MD_SRC_IOCLK_32	0x02	
#define	TM0MD_SRC_TM1UFLOW	0x05	
#define	TM0MD_SRC_TM2UFLOW	0x06	
#if	defined(CONFIG_AM33_2)
#define	TM0MD_SRC_TM2IO		0x03	
#define	TM0MD_SRC_TM0IO		0x07	
#endif 
#define	TM0MD_INIT_COUNTER	0x40	
#define	TM0MD_COUNT_ENABLE	0x80	

#define	TM1MD			__SYSREG(0xd4003001, u8) 
#define	TM1MD_SRC		0x07	
#define	TM1MD_SRC_IOCLK		0x00	
#define	TM1MD_SRC_IOCLK_8	0x01	
#define	TM1MD_SRC_IOCLK_32	0x02	
#define	TM1MD_SRC_TM0CASCADE	0x03	
#define	TM1MD_SRC_TM0UFLOW	0x04	
#define	TM1MD_SRC_TM2UFLOW	0x06	
#if defined(CONFIG_AM33_2)
#define	TM1MD_SRC_TM1IO		0x07	
#endif	
#define	TM1MD_INIT_COUNTER	0x40	
#define	TM1MD_COUNT_ENABLE	0x80	

#define	TM2MD			__SYSREG(0xd4003002, u8) 
#define	TM2MD_SRC		0x07	
#define	TM2MD_SRC_IOCLK		0x00	
#define	TM2MD_SRC_IOCLK_8	0x01	
#define	TM2MD_SRC_IOCLK_32	0x02	
#define	TM2MD_SRC_TM1CASCADE	0x03	
#define	TM2MD_SRC_TM0UFLOW	0x04	
#define	TM2MD_SRC_TM1UFLOW	0x05	
#if defined(CONFIG_AM33_2)
#define	TM2MD_SRC_TM2IO		0x07	
#endif	
#define	TM2MD_INIT_COUNTER	0x40	
#define	TM2MD_COUNT_ENABLE	0x80	

#define	TM3MD			__SYSREG(0xd4003003, u8) 
#define	TM3MD_SRC		0x07	
#define	TM3MD_SRC_IOCLK		0x00	
#define	TM3MD_SRC_IOCLK_8	0x01	
#define	TM3MD_SRC_IOCLK_32	0x02	
#define	TM3MD_SRC_TM2CASCADE	0x03	
#define	TM3MD_SRC_TM0UFLOW	0x04	
#define	TM3MD_SRC_TM1UFLOW	0x05	
#define	TM3MD_SRC_TM2UFLOW	0x06	
#if defined(CONFIG_AM33_2)
#define	TM3MD_SRC_TM3IO		0x07	
#endif	
#define	TM3MD_INIT_COUNTER	0x40	
#define	TM3MD_COUNT_ENABLE	0x80	

#define	TM01MD			__SYSREG(0xd4003000, u16)  

#define	TM0BR			__SYSREG(0xd4003010, u8)   
#define	TM1BR			__SYSREG(0xd4003011, u8)   
#define	TM2BR			__SYSREG(0xd4003012, u8)   
#define	TM3BR			__SYSREG(0xd4003013, u8)   
#define	TM01BR			__SYSREG(0xd4003010, u16)  

#define	TM0BC			__SYSREGC(0xd4003020, u8)  
#define	TM1BC			__SYSREGC(0xd4003021, u8)  
#define	TM2BC			__SYSREGC(0xd4003022, u8)  
#define	TM3BC			__SYSREGC(0xd4003023, u8)  
#define	TM01BC			__SYSREGC(0xd4003020, u16) 

#define TM0IRQ			2	
#define TM1IRQ			3	
#define TM2IRQ			4	
#define TM3IRQ			5	

#define	TM0ICR			GxICR(TM0IRQ)	
#define	TM1ICR			GxICR(TM1IRQ)	
#define	TM2ICR			GxICR(TM2IRQ)	
#define	TM3ICR			GxICR(TM3IRQ)	

#define	TM4MD			__SYSREG(0xd4003080, u8)   
#define	TM4MD_SRC		0x07	
#define	TM4MD_SRC_IOCLK		0x00	
#define	TM4MD_SRC_IOCLK_8	0x01	
#define	TM4MD_SRC_IOCLK_32	0x02	
#define	TM4MD_SRC_TM0UFLOW	0x04	
#define	TM4MD_SRC_TM1UFLOW	0x05	
#define	TM4MD_SRC_TM2UFLOW	0x06	
#if defined(CONFIG_AM33_2)
#define	TM4MD_SRC_TM4IO		0x07	
#endif	
#define	TM4MD_INIT_COUNTER	0x40	
#define	TM4MD_COUNT_ENABLE	0x80	

#define	TM5MD			__SYSREG(0xd4003082, u8)   
#define	TM5MD_SRC		0x07	
#define	TM5MD_SRC_IOCLK		0x00	
#define	TM5MD_SRC_IOCLK_8	0x01	
#define	TM5MD_SRC_IOCLK_32	0x02	
#define	TM5MD_SRC_TM4CASCADE	0x03	
#define	TM5MD_SRC_TM0UFLOW	0x04	
#define	TM5MD_SRC_TM1UFLOW	0x05	
#define	TM5MD_SRC_TM2UFLOW	0x06	
#if defined(CONFIG_AM33_2)
#define	TM5MD_SRC_TM5IO		0x07	
#else	
#define	TM5MD_SRC_TM7UFLOW	0x07	
#endif	
#define	TM5MD_INIT_COUNTER	0x40	
#define	TM5MD_COUNT_ENABLE	0x80	

#define	TM7MD			__SYSREG(0xd4003086, u8)   
#define	TM7MD_SRC		0x07	
#define	TM7MD_SRC_IOCLK		0x00	
#define	TM7MD_SRC_IOCLK_8	0x01	
#define	TM7MD_SRC_IOCLK_32	0x02	
#define	TM7MD_SRC_TM0UFLOW	0x04	
#define	TM7MD_SRC_TM1UFLOW	0x05	
#define	TM7MD_SRC_TM2UFLOW	0x06	
#if defined(CONFIG_AM33_2)
#define	TM7MD_SRC_TM7IO		0x07	
#endif	
#define	TM7MD_INIT_COUNTER	0x40	
#define	TM7MD_COUNT_ENABLE	0x80	

#define	TM8MD			__SYSREG(0xd4003088, u8)   
#define	TM8MD_SRC		0x07	
#define	TM8MD_SRC_IOCLK		0x00	
#define	TM8MD_SRC_IOCLK_8	0x01	
#define	TM8MD_SRC_IOCLK_32	0x02	
#define	TM8MD_SRC_TM7CASCADE	0x03	
#define	TM8MD_SRC_TM0UFLOW	0x04	
#define	TM8MD_SRC_TM1UFLOW	0x05	
#define	TM8MD_SRC_TM2UFLOW	0x06	
#if defined(CONFIG_AM33_2)
#define	TM8MD_SRC_TM8IO		0x07	
#else	
#define	TM8MD_SRC_TM7UFLOW	0x07	
#endif	
#define	TM8MD_INIT_COUNTER	0x40	
#define	TM8MD_COUNT_ENABLE	0x80	

#define	TM9MD			__SYSREG(0xd400308a, u8)   
#define	TM9MD_SRC		0x07	
#define	TM9MD_SRC_IOCLK		0x00	
#define	TM9MD_SRC_IOCLK_8	0x01	
#define	TM9MD_SRC_IOCLK_32	0x02	
#define	TM9MD_SRC_TM8CASCADE	0x03	
#define	TM9MD_SRC_TM0UFLOW	0x04	
#define	TM9MD_SRC_TM1UFLOW	0x05	
#define	TM9MD_SRC_TM2UFLOW	0x06	
#if defined(CONFIG_AM33_2)
#define	TM9MD_SRC_TM9IO		0x07	
#else	
#define	TM9MD_SRC_TM7UFLOW	0x07	
#endif	
#define	TM9MD_INIT_COUNTER	0x40	
#define	TM9MD_COUNT_ENABLE	0x80	

#define	TM10MD			__SYSREG(0xd400308c, u8)   
#define	TM10MD_SRC		0x07	
#define	TM10MD_SRC_IOCLK	0x00	
#define	TM10MD_SRC_IOCLK_8	0x01	
#define	TM10MD_SRC_IOCLK_32	0x02	
#define	TM10MD_SRC_TM9CASCADE	0x03	
#define	TM10MD_SRC_TM0UFLOW	0x04	
#define	TM10MD_SRC_TM1UFLOW	0x05	
#define	TM10MD_SRC_TM2UFLOW	0x06	
#if defined(CONFIG_AM33_2)
#define	TM10MD_SRC_TM10IO	0x07	
#else	
#define	TM10MD_SRC_TM7UFLOW	0x07	
#endif	
#define	TM10MD_INIT_COUNTER	0x40	
#define	TM10MD_COUNT_ENABLE	0x80	

#define	TM11MD			__SYSREG(0xd400308e, u8)   
#define	TM11MD_SRC		0x07	
#define	TM11MD_SRC_IOCLK	0x00	
#define	TM11MD_SRC_IOCLK_8	0x01	
#define	TM11MD_SRC_IOCLK_32	0x02	
#define	TM11MD_SRC_TM0UFLOW	0x04	
#define	TM11MD_SRC_TM1UFLOW	0x05	
#define	TM11MD_SRC_TM2UFLOW	0x06	
#if defined(CONFIG_AM33_2)
#define	TM11MD_SRC_TM11IO	0x07	
#else	
#define	TM11MD_SRC_TM7UFLOW	0x07	
#endif	
#define	TM11MD_INIT_COUNTER	0x40	
#define	TM11MD_COUNT_ENABLE	0x80	

#if defined(CONFIG_AM34_2)
#define	TM12MD			__SYSREG(0xd4003180, u8)   
#define	TM12MD_SRC		0x07	
#define	TM12MD_SRC_IOCLK	0x00	
#define	TM12MD_SRC_IOCLK_8	0x01	
#define	TM12MD_SRC_IOCLK_32	0x02	
#define	TM12MD_SRC_TM0UFLOW	0x04	
#define	TM12MD_SRC_TM1UFLOW	0x05	
#define	TM12MD_SRC_TM2UFLOW	0x06	
#define	TM12MD_SRC_TM7UFLOW	0x07	
#define	TM12MD_INIT_COUNTER	0x40	
#define	TM12MD_COUNT_ENABLE	0x80	

#define	TM13MD			__SYSREG(0xd4003182, u8)   
#define	TM13MD_SRC		0x07	
#define	TM13MD_SRC_IOCLK	0x00	
#define	TM13MD_SRC_IOCLK_8	0x01	
#define	TM13MD_SRC_IOCLK_32	0x02	
#define	TM13MD_SRC_TM12CASCADE	0x03	
#define	TM13MD_SRC_TM0UFLOW	0x04	
#define	TM13MD_SRC_TM1UFLOW	0x05	
#define	TM13MD_SRC_TM2UFLOW	0x06	
#define	TM13MD_SRC_TM7UFLOW	0x07	
#define	TM13MD_INIT_COUNTER	0x40	
#define	TM13MD_COUNT_ENABLE	0x80	

#define	TM14MD			__SYSREG(0xd4003184, u8)   
#define	TM14MD_SRC		0x07	
#define	TM14MD_SRC_IOCLK	0x00	
#define	TM14MD_SRC_IOCLK_8	0x01	
#define	TM14MD_SRC_IOCLK_32	0x02	
#define	TM14MD_SRC_TM13CASCADE	0x03	
#define	TM14MD_SRC_TM0UFLOW	0x04	
#define	TM14MD_SRC_TM1UFLOW	0x05	
#define	TM14MD_SRC_TM2UFLOW	0x06	
#define	TM14MD_SRC_TM7UFLOW	0x07	
#define	TM14MD_INIT_COUNTER	0x40	
#define	TM14MD_COUNT_ENABLE	0x80	

#define	TM15MD			__SYSREG(0xd4003186, u8)   
#define	TM15MD_SRC		0x07	
#define	TM15MD_SRC_IOCLK	0x00	
#define	TM15MD_SRC_IOCLK_8	0x01	
#define	TM15MD_SRC_IOCLK_32	0x02	
#define	TM15MD_SRC_TM0UFLOW	0x04	
#define	TM15MD_SRC_TM1UFLOW	0x05	
#define	TM15MD_SRC_TM2UFLOW	0x06	
#define	TM15MD_SRC_TM7UFLOW	0x07	
#define	TM15MD_INIT_COUNTER	0x40	
#define	TM15MD_COUNT_ENABLE	0x80	
#endif	


#define	TM4BR			__SYSREG(0xd4003090, u16)  
#define	TM5BR			__SYSREG(0xd4003092, u16)  
#define	TM45BR			__SYSREG(0xd4003090, u32)  
#define	TM7BR			__SYSREG(0xd4003096, u16)  
#define	TM8BR			__SYSREG(0xd4003098, u16)  
#define	TM9BR			__SYSREG(0xd400309a, u16)  
#define	TM89BR			__SYSREG(0xd4003098, u32)  
#define	TM10BR			__SYSREG(0xd400309c, u16)  
#define	TM11BR			__SYSREG(0xd400309e, u16)  
#if defined(CONFIG_AM34_2)
#define	TM12BR			__SYSREG(0xd4003190, u16)  
#define	TM13BR			__SYSREG(0xd4003192, u16)  
#define	TM14BR			__SYSREG(0xd4003194, u16)  
#define	TM15BR			__SYSREG(0xd4003196, u16)  
#endif	

#define	TM4BC			__SYSREG(0xd40030a0, u16)  
#define	TM5BC			__SYSREG(0xd40030a2, u16)  
#define	TM45BC			__SYSREG(0xd40030a0, u32)  
#define	TM7BC			__SYSREG(0xd40030a6, u16)  
#define	TM8BC			__SYSREG(0xd40030a8, u16)  
#define	TM9BC			__SYSREG(0xd40030aa, u16)  
#define	TM89BC			__SYSREG(0xd40030a8, u32)  
#define	TM10BC			__SYSREG(0xd40030ac, u16)  
#define	TM11BC			__SYSREG(0xd40030ae, u16)  
#if defined(CONFIG_AM34_2)
#define	TM12BC			__SYSREG(0xd40031a0, u16)  
#define	TM13BC			__SYSREG(0xd40031a2, u16)  
#define	TM14BC			__SYSREG(0xd40031a4, u16)  
#define	TM15BC			__SYSREG(0xd40031a6, u16)  
#endif	

#define TM4IRQ			6	
#define TM5IRQ			7	
#define TM7IRQ			11	
#define TM8IRQ			12	
#define TM9IRQ			13	
#define TM10IRQ			14	
#define TM11IRQ			15	
#if defined(CONFIG_AM34_2)
#define TM12IRQ			64	
#define TM13IRQ			65	
#define TM14IRQ			66	
#define TM15IRQ			67	
#endif	

#define	TM4ICR			GxICR(TM4IRQ)	
#define	TM5ICR			GxICR(TM5IRQ)	
#define	TM7ICR			GxICR(TM7IRQ)	
#define	TM8ICR			GxICR(TM8IRQ)	
#define	TM9ICR			GxICR(TM9IRQ)	
#define	TM10ICR			GxICR(TM10IRQ)	
#define	TM11ICR			GxICR(TM11IRQ)	
#if defined(CONFIG_AM34_2)
#define	TM12ICR			GxICR(TM12IRQ)	
#define	TM13ICR			GxICR(TM13IRQ)	
#define	TM14ICR			GxICR(TM14IRQ)	
#define	TM15ICR			GxICR(TM15IRQ)	
#endif	

#define	TM6MD			__SYSREG(0xd4003084, u16)  
#define	TM6MD_SRC		0x0007	
#define	TM6MD_SRC_IOCLK		0x0000	
#define	TM6MD_SRC_IOCLK_8	0x0001	
#define	TM6MD_SRC_IOCLK_32	0x0002	
#define	TM6MD_SRC_TM0UFLOW	0x0004	
#define	TM6MD_SRC_TM1UFLOW	0x0005	
#define	TM6MD_SRC_TM2UFLOW	0x0006	
#if defined(CONFIG_AM33_2)
	
#define	TM6MD_SRC_TM6IOB_SINGLE	0x0007	
#endif	
#define	TM6MD_ONESHOT_ENABLE	0x0040	
#define	TM6MD_CLR_ENABLE	0x0010	
#if	defined(CONFIG_AM33_2)
#define	TM6MD_TRIG_ENABLE	0x0080	
#define TM6MD_PWM		0x3800	
#define TM6MD_PWM_DIS		0x0000	
#define	TM6MD_PWM_10BIT		0x1000	
#define	TM6MD_PWM_11BIT		0x1800	
#define	TM6MD_PWM_12BIT		0x3000	
#define	TM6MD_PWM_14BIT		0x3800	
#endif	

#define	TM6MD_INIT_COUNTER	0x4000	
#define	TM6MD_COUNT_ENABLE	0x8000	

#define	TM6MDA			__SYSREG(0xd40030b4, u8)   
#define	TM6MDA_MODE_CMP_SINGLE	0x00	
#define	TM6MDA_MODE_CMP_DOUBLE	0x40	
#if	defined(CONFIG_AM33_2)
#define TM6MDA_OUT		0x07	
#define	TM6MDA_OUT_SETA_RESETB	0x00	
#define	TM6MDA_OUT_SETA_RESETOV	0x01	
#define	TM6MDA_OUT_SETA		0x02	
#define	TM6MDA_OUT_RESETA	0x03	
#define	TM6MDA_OUT_TOGGLE	0x04	
#define TM6MDA_MODE		0xc0	
#define	TM6MDA_MODE_CAP_S_EDGE	0x80	
#define	TM6MDA_MODE_CAP_D_EDGE	0xc0	
#define TM6MDA_EDGE		0x20	
#define	TM6MDA_EDGE_FALLING	0x00	
#define	TM6MDA_EDGE_RISING	0x20	
#define	TM6MDA_CAPTURE_ENABLE	0x10	
#else	
#define	TM6MDA_MODE		0x40	
#endif	

#define	TM6MDB			__SYSREG(0xd40030b5, u8)   
#define	TM6MDB_MODE_CMP_SINGLE	0x00	
#define	TM6MDB_MODE_CMP_DOUBLE	0x40	
#if defined(CONFIG_AM33_2)
#define TM6MDB_OUT		0x07	
#define	TM6MDB_OUT_SETB_RESETA	0x00	
#define	TM6MDB_OUT_SETB_RESETOV	0x01	
#define	TM6MDB_OUT_RESETB	0x03	
#define	TM6MDB_OUT_TOGGLE	0x04	
#define TM6MDB_MODE		0xc0	
#define	TM6MDB_MODE_CAP_S_EDGE	0x80	
#define	TM6MDB_MODE_CAP_D_EDGE	0xc0	
#define TM6MDB_EDGE		0x20	
#define	TM6MDB_EDGE_FALLING	0x00	
#define	TM6MDB_EDGE_RISING	0x20	
#define	TM6MDB_CAPTURE_ENABLE	0x10	
#else	
#define	TM6MDB_MODE		0x40	
#endif	

#define	TM6CA			__SYSREG(0xd40030c4, u16)   
#define	TM6CB			__SYSREG(0xd40030d4, u16)   
#define	TM6BC			__SYSREG(0xd40030a4, u16)   

#define TM6IRQ			6	
#define TM6AIRQ			9	
#define TM6BIRQ			10	

#define	TM6ICR			GxICR(TM6IRQ)	
#define	TM6AICR			GxICR(TM6AIRQ)	
#define	TM6BICR			GxICR(TM6BIRQ)	

#if defined(CONFIG_AM34_2)
#define	TMTMD			__SYSREG(0xd4004100, u8)	
#define	TMTMD_TMTLDE		0x40	
#define	TMTMD_TMTCNE		0x80	

#define	TMTBR			__SYSREG(0xd4004110, u32)	
#define	TMTBC			__SYSREG(0xd4004120, u32)	

#define	TMSMD			__SYSREG(0xd4004140, u8)	
#define	TMSMD_TMSLDE		0x40		
#define	TMSMD_TMSCNE		0x80		

#define	TMSBR			__SYSREG(0xd4004150, u32)	
#define	TMSBC			__SYSREG(0xd4004160, u32)	

#define TMTIRQ			119		
#define TMSIRQ			120		

#define	TMTICR			GxICR(TMTIRQ)	
#define	TMSICR			GxICR(TMSIRQ)	
#endif	

#endif 

#endif 
