/*
 * Ethernet driver for the Atmel AT91RM9200 (Thunder)
 *
 *  Copyright (C) SAN People (Pty) Ltd
 *
 * Based on an earlier Atmel EMAC macrocell driver by Atmel and Lineo Inc.
 * Initial version by Rick Bronson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef AT91_ETHERNET
#define AT91_ETHERNET


#define MII_DM9161_ID		0x0181b880
#define MII_DM9161A_ID		0x0181b8a0
#define MII_DSCR_REG		16
#define MII_DSCSR_REG		17
#define MII_DSINTR_REG		21

#define MII_LXT971A_ID		0x001378E0
#define MII_ISINTE_REG		18
#define MII_ISINTS_REG		19
#define MII_LEDCTRL_REG		20

#define MII_RTL8201_ID		0x00008200

#define MII_BCM5221_ID		0x004061e0
#define MII_BCMINTR_REG		26

#define MII_DP83847_ID		0x20005c30

#define MII_DP83848_ID		0x20005c90
#define MII_DPPHYSTS_REG	16
#define MII_DPMICR_REG		17
#define MII_DPMISR_REG		18

#define MII_AC101L_ID		0x00225520

#define MII_KS8721_ID		0x00221610

#define MII_T78Q21x3_ID		0x000e7230
#define MII_T78Q21INT_REG	17

#define MII_LAN83C185_ID	0x0007C0A0


#define MAX_RBUFF_SZ	0x600		
#define MAX_RX_DESCR	9		

#define EMAC_DESC_DONE	0x00000001	
#define EMAC_DESC_WRAP	0x00000002	

#define EMAC_BROADCAST	0x80000000	
#define EMAC_MULTICAST	0x40000000	
#define EMAC_UNICAST	0x20000000	

struct rbf_t
{
	unsigned int addr;
	unsigned long size;
};

struct recv_desc_bufs
{
	struct rbf_t descriptors[MAX_RX_DESCR];		
	char recv_buf[MAX_RX_DESCR][MAX_RBUFF_SZ];	
};

struct at91_private
{
	struct mii_if_info mii;			
	struct macb_platform_data board_data;	
	struct clk *ether_clk;			

	
	unsigned long phy_type;			
	spinlock_t lock;			
	short phy_media;			
	unsigned short phy_address;		
	struct timer_list check_timer;		

	
	struct sk_buff *skb;			
	dma_addr_t skb_physaddr;		
	int skb_length;				

	
	int rxBuffIndex;			
	struct recv_desc_bufs *dlist;		
	struct recv_desc_bufs *dlist_phys;	
};

#endif
