/******************************************************************************
 *
 *	(C)Copyright 1998,1999 SysKonnect,
 *	a business unit of Schneider & Koch & Co. Datensysteme GmbH.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/



#include <asm/byteorder.h>

#ifdef __LITTLE_ENDIAN
#define LITTLE_ENDIAN
#else
#define BIG_ENDIAN
#endif



#define USE_CAN_ADDR		

#define MB_OUTSIDE_SMC		



#define SYNC	       		

				

#define ESS			

#define	SMT_PANIC(smc, nr, msg)	printk(KERN_INFO "SMT PANIC: code: %d, msg: %s\n",nr,msg)


#ifdef DEBUG
#define printf(s,args...) printk(KERN_INFO s, ## args)
#endif





#define NUM_RECEIVE_BUFFERS		10

#define NUM_TRANSMIT_BUFFERS		10

#define NUM_SMT_BUF	4

#define HWM_ASYNC_TXD_COUNT	(NUM_TRANSMIT_BUFFERS + NUM_SMT_BUF)

#define HWM_SYNC_TXD_COUNT	HWM_ASYNC_TXD_COUNT


#if (NUM_RECEIVE_BUFFERS > 100)
#define SMT_R1_RXD_COUNT	(1 + 100)
#else
#define SMT_R1_RXD_COUNT	(1 + NUM_RECEIVE_BUFFERS)
#endif

#define SMT_R2_RXD_COUNT	0	




struct s_txd_os {	
	struct sk_buff *skb;
	dma_addr_t dma_addr;
} ;

struct s_rxd_os {	
	struct sk_buff *skb;
	dma_addr_t dma_addr;
} ;



#define AIX_REVERSE(x)		((u32)le32_to_cpu((u32)(x)))
#define MDR_REVERSE(x)		((u32)le32_to_cpu((u32)(x)))
