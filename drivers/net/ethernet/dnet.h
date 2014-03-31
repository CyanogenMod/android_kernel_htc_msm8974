/*
 * Dave DNET Ethernet Controller driver
 *
 * Copyright (C) 2008 Dave S.r.l. <www.dave.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _DNET_H
#define _DNET_H

#define DRV_NAME		"dnet"
#define DRV_VERSION		"0.9.1"
#define PFX				DRV_NAME ": "

#define dnet_writel(port, value, reg)	\
	writel((value), (port)->regs + DNET_##reg)
#define dnet_readl(port, reg)	readl((port)->regs + DNET_##reg)

#define DNET_RX_LEN_FIFO		0x000	
#define DNET_RX_DATA_FIFO		0x004	
#define DNET_TX_LEN_FIFO		0x008	
#define DNET_TX_DATA_FIFO		0x00C	

#define DNET_VERCAPS			0x100	
#define DNET_INTR_SRC			0x104	
#define DNET_INTR_ENB			0x108	
#define DNET_RX_STATUS			0x10C	
#define DNET_TX_STATUS			0x110	
#define DNET_RX_FRAMES_CNT		0x114	
#define DNET_TX_FRAMES_CNT		0x118	
#define DNET_RX_FIFO_TH			0x11C	
#define DNET_TX_FIFO_TH			0x120	
#define DNET_SYS_CTL			0x124	
#define DNET_PAUSE_TMR			0x128	
#define DNET_RX_FIFO_WCNT		0x12C	
#define DNET_TX_FIFO_WCNT		0x130	

#define DNET_MACREG_DATA		0x200	
#define DNET_MACREG_ADDR		0x204	

#define DNET_RX_PKT_IGNR_CNT		0x300
#define DNET_RX_LEN_CHK_ERR_CNT		0x304
#define DNET_RX_LNG_FRM_CNT		0x308
#define DNET_RX_SHRT_FRM_CNT		0x30C
#define DNET_RX_IPG_VIOL_CNT		0x310
#define DNET_RX_CRC_ERR_CNT		0x314
#define DNET_RX_OK_PKT_CNT		0x318
#define DNET_RX_CTL_FRM_CNT		0x31C
#define DNET_RX_PAUSE_FRM_CNT		0x320
#define DNET_RX_MULTICAST_CNT		0x324
#define DNET_RX_BROADCAST_CNT		0x328
#define DNET_RX_VLAN_TAG_CNT		0x32C
#define DNET_RX_PRE_SHRINK_CNT		0x330
#define DNET_RX_DRIB_NIB_CNT		0x334
#define DNET_RX_UNSUP_OPCD_CNT		0x338
#define DNET_RX_BYTE_CNT		0x33C

#define DNET_TX_UNICAST_CNT		0x400
#define DNET_TX_PAUSE_FRM_CNT		0x404
#define DNET_TX_MULTICAST_CNT		0x408
#define DNET_TX_BRDCAST_CNT		0x40C
#define DNET_TX_VLAN_TAG_CNT		0x410
#define DNET_TX_BAD_FCS_CNT		0x414
#define DNET_TX_JUMBO_CNT		0x418
#define DNET_TX_BYTE_CNT		0x41C

#define DNET_INTERNAL_MODE_REG		0x0
#define DNET_INTERNAL_RXTX_CONTROL_REG	0x2
#define DNET_INTERNAL_MAX_PKT_SIZE_REG	0x4
#define DNET_INTERNAL_IGP_REG		0x8
#define DNET_INTERNAL_MAC_ADDR_0_REG	0xa
#define DNET_INTERNAL_MAC_ADDR_1_REG	0xc
#define DNET_INTERNAL_MAC_ADDR_2_REG	0xe
#define DNET_INTERNAL_TX_RX_STS_REG	0x12
#define DNET_INTERNAL_GMII_MNG_CTL_REG	0x14
#define DNET_INTERNAL_GMII_MNG_DAT_REG	0x16

#define DNET_INTERNAL_GMII_MNG_CMD_FIN	(1 << 14)

#define DNET_INTERNAL_WRITE		(1 << 31)


#define DNET_INTERNAL_MODE_GBITEN			(1 << 0)
#define DNET_INTERNAL_MODE_FCEN				(1 << 1)
#define DNET_INTERNAL_MODE_RXEN				(1 << 2)
#define DNET_INTERNAL_MODE_TXEN				(1 << 3)

#define DNET_INTERNAL_RXTX_CONTROL_RXSHORTFRAME		(1 << 8)
#define DNET_INTERNAL_RXTX_CONTROL_RXBROADCAST		(1 << 7)
#define DNET_INTERNAL_RXTX_CONTROL_RXMULTICAST		(1 << 4)
#define DNET_INTERNAL_RXTX_CONTROL_RXPAUSE		(1 << 3)
#define DNET_INTERNAL_RXTX_CONTROL_DISTXFCS		(1 << 2)
#define DNET_INTERNAL_RXTX_CONTROL_DISCFXFCS		(1 << 1)
#define DNET_INTERNAL_RXTX_CONTROL_ENPROMISC		(1 << 0)
#define DNET_INTERNAL_RXTX_CONTROL_DROPCONTROL		(1 << 6)
#define DNET_INTERNAL_RXTX_CONTROL_ENABLEHALFDUP	(1 << 5)

#define DNET_SYS_CTL_IGNORENEXTPKT			(1 << 0)
#define DNET_SYS_CTL_SENDPAUSE				(1 << 2)
#define DNET_SYS_CTL_RXFIFOFLUSH			(1 << 3)
#define DNET_SYS_CTL_TXFIFOFLUSH			(1 << 4)

#define DNET_TX_STATUS_FIFO_ALMOST_EMPTY		(1 << 2)
#define DNET_TX_STATUS_FIFO_ALMOST_FULL			(1 << 1)

#define DNET_INTR_SRC_TX_PKTSENT			(1 << 0)
#define DNET_INTR_SRC_TX_FIFOAF				(1 << 1)
#define DNET_INTR_SRC_TX_FIFOAE				(1 << 2)
#define DNET_INTR_SRC_TX_DISCFRM			(1 << 3)
#define DNET_INTR_SRC_TX_FIFOFULL			(1 << 4)
#define DNET_INTR_SRC_RX_CMDFIFOAF			(1 << 8)
#define DNET_INTR_SRC_RX_CMDFIFOFF			(1 << 9)
#define DNET_INTR_SRC_RX_DATAFIFOFF			(1 << 10)
#define DNET_INTR_SRC_TX_SUMMARY			(1 << 16)
#define DNET_INTR_SRC_RX_SUMMARY			(1 << 17)
#define DNET_INTR_SRC_PHY				(1 << 19)

#define DNET_INTR_ENB_TX_PKTSENT			(1 << 0)
#define DNET_INTR_ENB_TX_FIFOAF				(1 << 1)
#define DNET_INTR_ENB_TX_FIFOAE				(1 << 2)
#define DNET_INTR_ENB_TX_DISCFRM			(1 << 3)
#define DNET_INTR_ENB_TX_FIFOFULL			(1 << 4)
#define DNET_INTR_ENB_RX_PKTRDY				(1 << 8)
#define DNET_INTR_ENB_RX_FIFOAF				(1 << 9)
#define DNET_INTR_ENB_RX_FIFOERR			(1 << 10)
#define DNET_INTR_ENB_RX_ERROR				(1 << 11)
#define DNET_INTR_ENB_RX_FIFOFULL			(1 << 12)
#define DNET_INTR_ENB_RX_FIFOAE				(1 << 13)
#define DNET_INTR_ENB_TX_SUMMARY			(1 << 16)
#define DNET_INTR_ENB_RX_SUMMARY			(1 << 17)
#define DNET_INTR_ENB_GLOBAL_ENABLE			(1 << 18)

#define DNET_CFG_TX_FIFO_FULL_THRES	25
#define DNET_CFG_RX_FIFO_FULL_THRES	20


#define DNET_HAS_MDIO		(1 << 0)
#define DNET_HAS_IRQ		(1 << 1)
#define DNET_HAS_GIGABIT	(1 << 2)
#define DNET_HAS_DMA		(1 << 3)

#define DNET_HAS_MII		(1 << 4) 
#define DNET_HAS_RMII		(1 << 5) 

#define DNET_CAPS_MASK		0xFFFF

#define DNET_FIFO_SIZE		1024 
#define DNET_FIFO_TX_DATA_AF_TH	(DNET_FIFO_SIZE - 384) 
#define DNET_FIFO_TX_DATA_AE_TH	384

#define DNET_FIFO_RX_CMD_AF_TH	(1 << 16) 

struct dnet_stats {
	u32 rx_pkt_ignr;
	u32 rx_len_chk_err;
	u32 rx_lng_frm;
	u32 rx_shrt_frm;
	u32 rx_ipg_viol;
	u32 rx_crc_err;
	u32 rx_ok_pkt;
	u32 rx_ctl_frm;
	u32 rx_pause_frm;
	u32 rx_multicast;
	u32 rx_broadcast;
	u32 rx_vlan_tag;
	u32 rx_pre_shrink;
	u32 rx_drib_nib;
	u32 rx_unsup_opcd;
	u32 rx_byte;
	u32 tx_unicast;
	u32 tx_pause_frm;
	u32 tx_multicast;
	u32 tx_brdcast;
	u32 tx_vlan_tag;
	u32 tx_bad_fcs;
	u32 tx_jumbo;
	u32 tx_byte;
};

struct dnet {
	void __iomem			*regs;
	spinlock_t			lock;
	struct platform_device		*pdev;
	struct net_device		*dev;
	struct dnet_stats		hw_stats;
	unsigned int			capabilities; 
	struct napi_struct		napi;

	
	struct mii_bus			*mii_bus;
	struct phy_device		*phy_dev;
	unsigned int			link;
	unsigned int			speed;
	unsigned int			duplex;
};

#endif 
