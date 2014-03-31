/*
 * Definitions of consts/structs to drive the Freescale MSCAN.
 *
 * Copyright (C) 2005-2006 Andrey Volkov <avolkov@varma-el.com>,
 *                         Varma Electronics Oy
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the version 2 of the GNU General Public License
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __MSCAN_H__
#define __MSCAN_H__

#include <linux/types.h>

#define MSCAN_RXFRM		0x80
#define MSCAN_RXACT		0x40
#define MSCAN_CSWAI		0x20
#define MSCAN_SYNCH		0x10
#define MSCAN_TIME		0x08
#define MSCAN_WUPE		0x04
#define MSCAN_SLPRQ		0x02
#define MSCAN_INITRQ		0x01

#define MSCAN_CANE		0x80
#define MSCAN_CLKSRC		0x40
#define MSCAN_LOOPB		0x20
#define MSCAN_LISTEN		0x10
#define MSCAN_BORM		0x08
#define MSCAN_WUPM		0x04
#define MSCAN_SLPAK		0x02
#define MSCAN_INITAK		0x01

#ifdef CONFIG_PPC
#define MSCAN_FOR_MPC5XXX
#endif

#ifdef MSCAN_FOR_MPC5XXX
#define MSCAN_CLKSRC_BUS	0
#define MSCAN_CLKSRC_XTAL	MSCAN_CLKSRC
#define MSCAN_CLKSRC_IPS	MSCAN_CLKSRC
#else
#define MSCAN_CLKSRC_BUS	MSCAN_CLKSRC
#define MSCAN_CLKSRC_XTAL	0
#endif

#define MSCAN_WUPIF		0x80
#define MSCAN_CSCIF		0x40
#define MSCAN_RSTAT1		0x20
#define MSCAN_RSTAT0		0x10
#define MSCAN_TSTAT1		0x08
#define MSCAN_TSTAT0		0x04
#define MSCAN_OVRIF		0x02
#define MSCAN_RXF		0x01
#define MSCAN_ERR_IF 		(MSCAN_OVRIF | MSCAN_CSCIF)
#define MSCAN_RSTAT_MSK		(MSCAN_RSTAT1 | MSCAN_RSTAT0)
#define MSCAN_TSTAT_MSK		(MSCAN_TSTAT1 | MSCAN_TSTAT0)
#define MSCAN_STAT_MSK		(MSCAN_RSTAT_MSK | MSCAN_TSTAT_MSK)

#define MSCAN_STATE_BUS_OFF	(MSCAN_RSTAT1 | MSCAN_RSTAT0 | \
				 MSCAN_TSTAT1 | MSCAN_TSTAT0)
#define MSCAN_STATE_TX(canrflg)	(((canrflg)&MSCAN_TSTAT_MSK)>>2)
#define MSCAN_STATE_RX(canrflg)	(((canrflg)&MSCAN_RSTAT_MSK)>>4)
#define MSCAN_STATE_ACTIVE	0
#define MSCAN_STATE_WARNING	1
#define MSCAN_STATE_PASSIVE	2
#define MSCAN_STATE_BUSOFF	3

#define MSCAN_WUPIE		0x80
#define MSCAN_CSCIE		0x40
#define MSCAN_RSTATE1		0x20
#define MSCAN_RSTATE0		0x10
#define MSCAN_TSTATE1		0x08
#define MSCAN_TSTATE0		0x04
#define MSCAN_OVRIE		0x02
#define MSCAN_RXFIE		0x01

#define MSCAN_TXE2		0x04
#define MSCAN_TXE1		0x02
#define MSCAN_TXE0		0x01
#define MSCAN_TXE		(MSCAN_TXE2 | MSCAN_TXE1 | MSCAN_TXE0)

#define MSCAN_TXIE2		0x04
#define MSCAN_TXIE1		0x02
#define MSCAN_TXIE0		0x01
#define MSCAN_TXIE		(MSCAN_TXIE2 | MSCAN_TXIE1 | MSCAN_TXIE0)

#define MSCAN_ABTRQ2		0x04
#define MSCAN_ABTRQ1		0x02
#define MSCAN_ABTRQ0		0x01

#define MSCAN_ABTAK2		0x04
#define MSCAN_ABTAK1		0x02
#define MSCAN_ABTAK0		0x01

#define MSCAN_TX2		0x04
#define MSCAN_TX1		0x02
#define MSCAN_TX0		0x01

#define MSCAN_IDAM1		0x20
#define MSCAN_IDAM0		0x10
#define MSCAN_IDHIT2		0x04
#define MSCAN_IDHIT1		0x02
#define MSCAN_IDHIT0		0x01

#define MSCAN_AF_32BIT		0x00
#define MSCAN_AF_16BIT		MSCAN_IDAM0
#define MSCAN_AF_8BIT		MSCAN_IDAM1
#define MSCAN_AF_CLOSED		(MSCAN_IDAM0|MSCAN_IDAM1)
#define MSCAN_AF_MASK		(~(MSCAN_IDAM0|MSCAN_IDAM1))

#define MSCAN_BOHOLD		0x01

#define MSCAN_SFF_RTR_SHIFT	4
#define MSCAN_EFF_RTR_SHIFT	0
#define MSCAN_EFF_FLAGS		0x18	

#ifdef MSCAN_FOR_MPC5XXX
#define _MSCAN_RESERVED_(n, num) u8 _res##n[num]
#define _MSCAN_RESERVED_DSR_SIZE	2
#else
#define _MSCAN_RESERVED_(n, num)
#define _MSCAN_RESERVED_DSR_SIZE	0
#endif

struct mscan_regs {
	
	u8 canctl0;				
	u8 canctl1;				
	_MSCAN_RESERVED_(1, 2);			
	u8 canbtr0;				
	u8 canbtr1;				
	_MSCAN_RESERVED_(2, 2);			
	u8 canrflg;				
	u8 canrier;				
	_MSCAN_RESERVED_(3, 2);			
	u8 cantflg;				
	u8 cantier;				
	_MSCAN_RESERVED_(4, 2);			
	u8 cantarq;				
	u8 cantaak;				
	_MSCAN_RESERVED_(5, 2);			
	u8 cantbsel;				
	u8 canidac;				
	u8 reserved;				
	_MSCAN_RESERVED_(6, 2);			
	u8 canmisc;				
	_MSCAN_RESERVED_(7, 2);			
	u8 canrxerr;				
	u8 cantxerr;				
	_MSCAN_RESERVED_(8, 2);			
	u16 canidar1_0;				
	_MSCAN_RESERVED_(9, 2);			
	u16 canidar3_2;				
	_MSCAN_RESERVED_(10, 2);		
	u16 canidmr1_0;				
	_MSCAN_RESERVED_(11, 2);		
	u16 canidmr3_2;				
	_MSCAN_RESERVED_(12, 2);		
	u16 canidar5_4;				
	_MSCAN_RESERVED_(13, 2);		
	u16 canidar7_6;				
	_MSCAN_RESERVED_(14, 2);		
	u16 canidmr5_4;				
	_MSCAN_RESERVED_(15, 2);		
	u16 canidmr7_6;				
	_MSCAN_RESERVED_(16, 2);		
	struct {
		u16 idr1_0;			
		_MSCAN_RESERVED_(17, 2);	
		u16 idr3_2;			
		_MSCAN_RESERVED_(18, 2);	
		u16 dsr1_0;			
		_MSCAN_RESERVED_(19, 2);	
		u16 dsr3_2;			
		_MSCAN_RESERVED_(20, 2);	
		u16 dsr5_4;			
		_MSCAN_RESERVED_(21, 2);	
		u16 dsr7_6;			
		_MSCAN_RESERVED_(22, 2);	
		u8 dlr;				
		u8 reserved;			
		_MSCAN_RESERVED_(23, 2);	
		u16 time;			
	} rx;
	_MSCAN_RESERVED_(24, 2);		
	struct {
		u16 idr1_0;			
		_MSCAN_RESERVED_(25, 2);	
		u16 idr3_2;			
		_MSCAN_RESERVED_(26, 2);	
		u16 dsr1_0;			
		_MSCAN_RESERVED_(27, 2);	
		u16 dsr3_2;			
		_MSCAN_RESERVED_(28, 2);	
		u16 dsr5_4;			
		_MSCAN_RESERVED_(29, 2);	
		u16 dsr7_6;			
		_MSCAN_RESERVED_(30, 2);	
		u8 dlr;				
		u8 tbpr;			
		_MSCAN_RESERVED_(31, 2);	
		u16 time;			
	} tx;
	_MSCAN_RESERVED_(32, 2);		
} __packed;

#undef _MSCAN_RESERVED_
#define MSCAN_REGION 	sizeof(struct mscan)

#define MSCAN_NORMAL_MODE	0
#define MSCAN_SLEEP_MODE	MSCAN_SLPRQ
#define MSCAN_INIT_MODE		(MSCAN_INITRQ | MSCAN_SLPRQ)
#define MSCAN_POWEROFF_MODE	(MSCAN_CSWAI | MSCAN_SLPRQ)
#define MSCAN_SET_MODE_RETRIES	255
#define MSCAN_ECHO_SKB_MAX	3
#define MSCAN_RX_INTS_ENABLE	(MSCAN_OVRIE | MSCAN_RXFIE | MSCAN_CSCIE | \
				 MSCAN_RSTATE1 | MSCAN_RSTATE0 | \
				 MSCAN_TSTATE1 | MSCAN_TSTATE0)

enum {
	MSCAN_TYPE_MPC5200,
	MSCAN_TYPE_MPC5121
};

#define BTR0_BRP_MASK		0x3f
#define BTR0_SJW_SHIFT		6
#define BTR0_SJW_MASK		(0x3 << BTR0_SJW_SHIFT)

#define BTR1_TSEG1_MASK 	0xf
#define BTR1_TSEG2_SHIFT	4
#define BTR1_TSEG2_MASK 	(0x7 << BTR1_TSEG2_SHIFT)
#define BTR1_SAM_SHIFT  	7

#define BTR0_SET_BRP(brp)	(((brp) - 1) & BTR0_BRP_MASK)
#define BTR0_SET_SJW(sjw)	((((sjw) - 1) << BTR0_SJW_SHIFT) & \
				 BTR0_SJW_MASK)

#define BTR1_SET_TSEG1(tseg1)	(((tseg1) - 1) &  BTR1_TSEG1_MASK)
#define BTR1_SET_TSEG2(tseg2)	((((tseg2) - 1) << BTR1_TSEG2_SHIFT) & \
				 BTR1_TSEG2_MASK)
#define BTR1_SET_SAM(sam)	((sam) ? 1 << BTR1_SAM_SHIFT : 0)

#define F_RX_PROGRESS	0
#define F_TX_PROGRESS	1
#define F_TX_WAIT_ALL	2

#define TX_QUEUE_SIZE	3

struct tx_queue_entry {
	struct list_head list;
	u8 mask;
	u8 id;
};

struct mscan_priv {
	struct can_priv can;	
	unsigned int type; 	
	long open_time;
	unsigned long flags;
	void __iomem *reg_base;	
	u8 shadow_statflg;
	u8 shadow_canrier;
	u8 cur_pri;
	u8 prev_buf_id;
	u8 tx_active;

	struct list_head tx_head;
	struct tx_queue_entry tx_queue[TX_QUEUE_SIZE];
	struct napi_struct napi;
};

extern struct net_device *alloc_mscandev(void);
extern int register_mscandev(struct net_device *dev, int mscan_clksrc);
extern void unregister_mscandev(struct net_device *dev);

#endif 
