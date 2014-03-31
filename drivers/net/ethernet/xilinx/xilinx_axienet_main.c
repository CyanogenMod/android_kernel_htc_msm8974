/*
 * Xilinx Axi Ethernet device driver
 *
 * Copyright (c) 2008 Nissin Systems Co., Ltd.,  Yoshio Kashiwagi
 * Copyright (c) 2005-2008 DLA Systems,  David H. Lynch Jr. <dhlii@dlasys.net>
 * Copyright (c) 2008-2009 Secret Lab Technologies Ltd.
 * Copyright (c) 2010 - 2011 Michal Simek <monstr@monstr.eu>
 * Copyright (c) 2010 - 2011 PetaLogix
 * Copyright (c) 2010 - 2012 Xilinx, Inc. All rights reserved.
 *
 * This is a driver for the Xilinx Axi Ethernet which is used in the Virtex6
 * and Spartan6.
 *
 * TODO:
 *  - Add Axi Fifo support.
 *  - Factor out Axi DMA code into separate driver.
 *  - Test and fix basic multicast filtering.
 *  - Add support for extended multicast filtering.
 *  - Test basic VLAN support.
 *  - Add support for extended VLAN support.
 */

#include <linux/delay.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of_mdio.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/phy.h>
#include <linux/mii.h>
#include <linux/ethtool.h>

#include "xilinx_axienet.h"

#define TX_BD_NUM		64
#define RX_BD_NUM		128

#define DRIVER_NAME		"xaxienet"
#define DRIVER_DESCRIPTION	"Xilinx Axi Ethernet driver"
#define DRIVER_VERSION		"1.00a"

#define AXIENET_REGS_N		32

static struct of_device_id axienet_of_match[] __devinitdata = {
	{ .compatible = "xlnx,axi-ethernet-1.00.a", },
	{ .compatible = "xlnx,axi-ethernet-1.01.a", },
	{ .compatible = "xlnx,axi-ethernet-2.01.a", },
	{},
};

MODULE_DEVICE_TABLE(of, axienet_of_match);

static struct axienet_option axienet_options[] = {
	
	{
		.opt = XAE_OPTION_JUMBO,
		.reg = XAE_TC_OFFSET,
		.m_or = XAE_TC_JUM_MASK,
	}, {
		.opt = XAE_OPTION_JUMBO,
		.reg = XAE_RCW1_OFFSET,
		.m_or = XAE_RCW1_JUM_MASK,
	}, { 
		.opt = XAE_OPTION_VLAN,
		.reg = XAE_TC_OFFSET,
		.m_or = XAE_TC_VLAN_MASK,
	}, {
		.opt = XAE_OPTION_VLAN,
		.reg = XAE_RCW1_OFFSET,
		.m_or = XAE_RCW1_VLAN_MASK,
	}, { 
		.opt = XAE_OPTION_FCS_STRIP,
		.reg = XAE_RCW1_OFFSET,
		.m_or = XAE_RCW1_FCS_MASK,
	}, { 
		.opt = XAE_OPTION_FCS_INSERT,
		.reg = XAE_TC_OFFSET,
		.m_or = XAE_TC_FCS_MASK,
	}, { 
		.opt = XAE_OPTION_LENTYPE_ERR,
		.reg = XAE_RCW1_OFFSET,
		.m_or = XAE_RCW1_LT_DIS_MASK,
	}, { 
		.opt = XAE_OPTION_FLOW_CONTROL,
		.reg = XAE_FCC_OFFSET,
		.m_or = XAE_FCC_FCRX_MASK,
	}, { 
		.opt = XAE_OPTION_FLOW_CONTROL,
		.reg = XAE_FCC_OFFSET,
		.m_or = XAE_FCC_FCTX_MASK,
	}, { 
		.opt = XAE_OPTION_PROMISC,
		.reg = XAE_FMI_OFFSET,
		.m_or = XAE_FMI_PM_MASK,
	}, { 
		.opt = XAE_OPTION_TXEN,
		.reg = XAE_TC_OFFSET,
		.m_or = XAE_TC_TX_MASK,
	}, { 
		.opt = XAE_OPTION_RXEN,
		.reg = XAE_RCW1_OFFSET,
		.m_or = XAE_RCW1_RX_MASK,
	},
	{}
};

static inline u32 axienet_dma_in32(struct axienet_local *lp, off_t reg)
{
	return in_be32(lp->dma_regs + reg);
}

/**
 * axienet_dma_out32 - Memory mapped Axi DMA register write.
 * @lp:		Pointer to axienet local structure
 * @reg:	Address offset from the base address of the Axi DMA core
 * @value:	Value to be written into the Axi DMA register
 *
 * This function writes the desired value into the corresponding Axi DMA
 * register.
 */
static inline void axienet_dma_out32(struct axienet_local *lp,
				     off_t reg, u32 value)
{
	out_be32((lp->dma_regs + reg), value);
}

static void axienet_dma_bd_release(struct net_device *ndev)
{
	int i;
	struct axienet_local *lp = netdev_priv(ndev);

	for (i = 0; i < RX_BD_NUM; i++) {
		dma_unmap_single(ndev->dev.parent, lp->rx_bd_v[i].phys,
				 lp->max_frm_size, DMA_FROM_DEVICE);
		dev_kfree_skb((struct sk_buff *)
			      (lp->rx_bd_v[i].sw_id_offset));
	}

	if (lp->rx_bd_v) {
		dma_free_coherent(ndev->dev.parent,
				  sizeof(*lp->rx_bd_v) * RX_BD_NUM,
				  lp->rx_bd_v,
				  lp->rx_bd_p);
	}
	if (lp->tx_bd_v) {
		dma_free_coherent(ndev->dev.parent,
				  sizeof(*lp->tx_bd_v) * TX_BD_NUM,
				  lp->tx_bd_v,
				  lp->tx_bd_p);
	}
}

static int axienet_dma_bd_init(struct net_device *ndev)
{
	u32 cr;
	int i;
	struct sk_buff *skb;
	struct axienet_local *lp = netdev_priv(ndev);

	
	lp->tx_bd_ci = 0;
	lp->tx_bd_tail = 0;
	lp->rx_bd_ci = 0;

	lp->tx_bd_v = dma_alloc_coherent(ndev->dev.parent,
					 sizeof(*lp->tx_bd_v) * TX_BD_NUM,
					 &lp->tx_bd_p,
					 GFP_KERNEL);
	if (!lp->tx_bd_v) {
		dev_err(&ndev->dev, "unable to allocate DMA Tx buffer "
			"descriptors");
		goto out;
	}

	lp->rx_bd_v = dma_alloc_coherent(ndev->dev.parent,
					 sizeof(*lp->rx_bd_v) * RX_BD_NUM,
					 &lp->rx_bd_p,
					 GFP_KERNEL);
	if (!lp->rx_bd_v) {
		dev_err(&ndev->dev, "unable to allocate DMA Rx buffer "
			"descriptors");
		goto out;
	}

	memset(lp->tx_bd_v, 0, sizeof(*lp->tx_bd_v) * TX_BD_NUM);
	for (i = 0; i < TX_BD_NUM; i++) {
		lp->tx_bd_v[i].next = lp->tx_bd_p +
				      sizeof(*lp->tx_bd_v) *
				      ((i + 1) % TX_BD_NUM);
	}

	memset(lp->rx_bd_v, 0, sizeof(*lp->rx_bd_v) * RX_BD_NUM);
	for (i = 0; i < RX_BD_NUM; i++) {
		lp->rx_bd_v[i].next = lp->rx_bd_p +
				      sizeof(*lp->rx_bd_v) *
				      ((i + 1) % RX_BD_NUM);

		skb = netdev_alloc_skb_ip_align(ndev, lp->max_frm_size);
		if (!skb) {
			dev_err(&ndev->dev, "alloc_skb error %d\n", i);
			goto out;
		}

		lp->rx_bd_v[i].sw_id_offset = (u32) skb;
		lp->rx_bd_v[i].phys = dma_map_single(ndev->dev.parent,
						     skb->data,
						     lp->max_frm_size,
						     DMA_FROM_DEVICE);
		lp->rx_bd_v[i].cntrl = lp->max_frm_size;
	}

	
	cr = axienet_dma_in32(lp, XAXIDMA_RX_CR_OFFSET);
	
	cr = ((cr & ~XAXIDMA_COALESCE_MASK) |
	      ((lp->coalesce_count_rx) << XAXIDMA_COALESCE_SHIFT));
	
	cr = ((cr & ~XAXIDMA_DELAY_MASK) |
	      (XAXIDMA_DFT_RX_WAITBOUND << XAXIDMA_DELAY_SHIFT));
	
	cr |= XAXIDMA_IRQ_ALL_MASK;
	
	axienet_dma_out32(lp, XAXIDMA_RX_CR_OFFSET, cr);

	
	cr = axienet_dma_in32(lp, XAXIDMA_TX_CR_OFFSET);
	
	cr = (((cr & ~XAXIDMA_COALESCE_MASK)) |
	      ((lp->coalesce_count_tx) << XAXIDMA_COALESCE_SHIFT));
	
	cr = (((cr & ~XAXIDMA_DELAY_MASK)) |
	      (XAXIDMA_DFT_TX_WAITBOUND << XAXIDMA_DELAY_SHIFT));
	
	cr |= XAXIDMA_IRQ_ALL_MASK;
	
	axienet_dma_out32(lp, XAXIDMA_TX_CR_OFFSET, cr);

	axienet_dma_out32(lp, XAXIDMA_RX_CDESC_OFFSET, lp->rx_bd_p);
	cr = axienet_dma_in32(lp, XAXIDMA_RX_CR_OFFSET);
	axienet_dma_out32(lp, XAXIDMA_RX_CR_OFFSET,
			  cr | XAXIDMA_CR_RUNSTOP_MASK);
	axienet_dma_out32(lp, XAXIDMA_RX_TDESC_OFFSET, lp->rx_bd_p +
			  (sizeof(*lp->rx_bd_v) * (RX_BD_NUM - 1)));

	axienet_dma_out32(lp, XAXIDMA_TX_CDESC_OFFSET, lp->tx_bd_p);
	cr = axienet_dma_in32(lp, XAXIDMA_TX_CR_OFFSET);
	axienet_dma_out32(lp, XAXIDMA_TX_CR_OFFSET,
			  cr | XAXIDMA_CR_RUNSTOP_MASK);

	return 0;
out:
	axienet_dma_bd_release(ndev);
	return -ENOMEM;
}

/**
 * axienet_set_mac_address - Write the MAC address
 * @ndev:	Pointer to the net_device structure
 * @address:	6 byte Address to be written as MAC address
 *
 * This function is called to initialize the MAC address of the Axi Ethernet
 * core. It writes to the UAW0 and UAW1 registers of the core.
 */
static void axienet_set_mac_address(struct net_device *ndev, void *address)
{
	struct axienet_local *lp = netdev_priv(ndev);

	if (address)
		memcpy(ndev->dev_addr, address, ETH_ALEN);
	if (!is_valid_ether_addr(ndev->dev_addr))
		random_ether_addr(ndev->dev_addr);

	
	axienet_iow(lp, XAE_UAW0_OFFSET,
		    (ndev->dev_addr[0]) |
		    (ndev->dev_addr[1] << 8) |
		    (ndev->dev_addr[2] << 16) |
		    (ndev->dev_addr[3] << 24));
	axienet_iow(lp, XAE_UAW1_OFFSET,
		    (((axienet_ior(lp, XAE_UAW1_OFFSET)) &
		      ~XAE_UAW1_UNICASTADDR_MASK) |
		     (ndev->dev_addr[4] |
		     (ndev->dev_addr[5] << 8))));
}

/**
 * netdev_set_mac_address - Write the MAC address (from outside the driver)
 * @ndev:	Pointer to the net_device structure
 * @p:		6 byte Address to be written as MAC address
 *
 * returns: 0 for all conditions. Presently, there is no failure case.
 *
 * This function is called to initialize the MAC address of the Axi Ethernet
 * core. It calls the core specific axienet_set_mac_address. This is the
 * function that goes into net_device_ops structure entry ndo_set_mac_address.
 */
static int netdev_set_mac_address(struct net_device *ndev, void *p)
{
	struct sockaddr *addr = p;
	axienet_set_mac_address(ndev, addr->sa_data);
	return 0;
}

static void axienet_set_multicast_list(struct net_device *ndev)
{
	int i;
	u32 reg, af0reg, af1reg;
	struct axienet_local *lp = netdev_priv(ndev);

	if (ndev->flags & (IFF_ALLMULTI | IFF_PROMISC) ||
	    netdev_mc_count(ndev) > XAE_MULTICAST_CAM_TABLE_NUM) {
		ndev->flags |= IFF_PROMISC;
		reg = axienet_ior(lp, XAE_FMI_OFFSET);
		reg |= XAE_FMI_PM_MASK;
		axienet_iow(lp, XAE_FMI_OFFSET, reg);
		dev_info(&ndev->dev, "Promiscuous mode enabled.\n");
	} else if (!netdev_mc_empty(ndev)) {
		struct netdev_hw_addr *ha;

		i = 0;
		netdev_for_each_mc_addr(ha, ndev) {
			if (i >= XAE_MULTICAST_CAM_TABLE_NUM)
				break;

			af0reg = (ha->addr[0]);
			af0reg |= (ha->addr[1] << 8);
			af0reg |= (ha->addr[2] << 16);
			af0reg |= (ha->addr[3] << 24);

			af1reg = (ha->addr[4]);
			af1reg |= (ha->addr[5] << 8);

			reg = axienet_ior(lp, XAE_FMI_OFFSET) & 0xFFFFFF00;
			reg |= i;

			axienet_iow(lp, XAE_FMI_OFFSET, reg);
			axienet_iow(lp, XAE_AF0_OFFSET, af0reg);
			axienet_iow(lp, XAE_AF1_OFFSET, af1reg);
			i++;
		}
	} else {
		reg = axienet_ior(lp, XAE_FMI_OFFSET);
		reg &= ~XAE_FMI_PM_MASK;

		axienet_iow(lp, XAE_FMI_OFFSET, reg);

		for (i = 0; i < XAE_MULTICAST_CAM_TABLE_NUM; i++) {
			reg = axienet_ior(lp, XAE_FMI_OFFSET) & 0xFFFFFF00;
			reg |= i;

			axienet_iow(lp, XAE_FMI_OFFSET, reg);
			axienet_iow(lp, XAE_AF0_OFFSET, 0);
			axienet_iow(lp, XAE_AF1_OFFSET, 0);
		}

		dev_info(&ndev->dev, "Promiscuous mode disabled.\n");
	}
}

static void axienet_setoptions(struct net_device *ndev, u32 options)
{
	int reg;
	struct axienet_local *lp = netdev_priv(ndev);
	struct axienet_option *tp = &axienet_options[0];

	while (tp->opt) {
		reg = ((axienet_ior(lp, tp->reg)) & ~(tp->m_or));
		if (options & tp->opt)
			reg |= tp->m_or;
		axienet_iow(lp, tp->reg, reg);
		tp++;
	}

	lp->options |= options;
}

static void __axienet_device_reset(struct axienet_local *lp,
				   struct device *dev, off_t offset)
{
	u32 timeout;
	axienet_dma_out32(lp, offset, XAXIDMA_CR_RESET_MASK);
	timeout = DELAY_OF_ONE_MILLISEC;
	while (axienet_dma_in32(lp, offset) & XAXIDMA_CR_RESET_MASK) {
		udelay(1);
		if (--timeout == 0) {
			dev_err(dev, "axienet_device_reset DMA "
				"reset timeout!\n");
			break;
		}
	}
}

static void axienet_device_reset(struct net_device *ndev)
{
	u32 axienet_status;
	struct axienet_local *lp = netdev_priv(ndev);

	__axienet_device_reset(lp, &ndev->dev, XAXIDMA_TX_CR_OFFSET);
	__axienet_device_reset(lp, &ndev->dev, XAXIDMA_RX_CR_OFFSET);

	lp->max_frm_size = XAE_MAX_VLAN_FRAME_SIZE;
	lp->options &= (~XAE_OPTION_JUMBO);

	if ((ndev->mtu > XAE_MTU) &&
	    (ndev->mtu <= XAE_JUMBO_MTU) &&
	    (lp->jumbo_support)) {
		lp->max_frm_size = ndev->mtu + XAE_HDR_VLAN_SIZE +
				   XAE_TRL_SIZE;
		lp->options |= XAE_OPTION_JUMBO;
	}

	if (axienet_dma_bd_init(ndev)) {
		dev_err(&ndev->dev, "axienet_device_reset descriptor "
			"allocation failed\n");
	}

	axienet_status = axienet_ior(lp, XAE_RCW1_OFFSET);
	axienet_status &= ~XAE_RCW1_RX_MASK;
	axienet_iow(lp, XAE_RCW1_OFFSET, axienet_status);

	axienet_status = axienet_ior(lp, XAE_IP_OFFSET);
	if (axienet_status & XAE_INT_RXRJECT_MASK)
		axienet_iow(lp, XAE_IS_OFFSET, XAE_INT_RXRJECT_MASK);

	axienet_iow(lp, XAE_FCC_OFFSET, XAE_FCC_FCRX_MASK);

	axienet_setoptions(ndev, lp->options &
			   ~(XAE_OPTION_TXEN | XAE_OPTION_RXEN));
	axienet_set_mac_address(ndev, NULL);
	axienet_set_multicast_list(ndev);
	axienet_setoptions(ndev, lp->options);

	ndev->trans_start = jiffies;
}

static void axienet_adjust_link(struct net_device *ndev)
{
	u32 emmc_reg;
	u32 link_state;
	u32 setspeed = 1;
	struct axienet_local *lp = netdev_priv(ndev);
	struct phy_device *phy = lp->phy_dev;

	link_state = phy->speed | (phy->duplex << 1) | phy->link;
	if (lp->last_link != link_state) {
		if ((phy->speed == SPEED_10) || (phy->speed == SPEED_100)) {
			if (lp->phy_type == XAE_PHY_TYPE_1000BASE_X)
				setspeed = 0;
		} else {
			if ((phy->speed == SPEED_1000) &&
			    (lp->phy_type == XAE_PHY_TYPE_MII))
				setspeed = 0;
		}

		if (setspeed == 1) {
			emmc_reg = axienet_ior(lp, XAE_EMMC_OFFSET);
			emmc_reg &= ~XAE_EMMC_LINKSPEED_MASK;

			switch (phy->speed) {
			case SPEED_1000:
				emmc_reg |= XAE_EMMC_LINKSPD_1000;
				break;
			case SPEED_100:
				emmc_reg |= XAE_EMMC_LINKSPD_100;
				break;
			case SPEED_10:
				emmc_reg |= XAE_EMMC_LINKSPD_10;
				break;
			default:
				dev_err(&ndev->dev, "Speed other than 10, 100 "
					"or 1Gbps is not supported\n");
				break;
			}

			axienet_iow(lp, XAE_EMMC_OFFSET, emmc_reg);
			lp->last_link = link_state;
			phy_print_status(phy);
		} else {
			dev_err(&ndev->dev, "Error setting Axi Ethernet "
				"mac speed\n");
		}
	}
}

static void axienet_start_xmit_done(struct net_device *ndev)
{
	u32 size = 0;
	u32 packets = 0;
	struct axienet_local *lp = netdev_priv(ndev);
	struct axidma_bd *cur_p;
	unsigned int status = 0;

	cur_p = &lp->tx_bd_v[lp->tx_bd_ci];
	status = cur_p->status;
	while (status & XAXIDMA_BD_STS_COMPLETE_MASK) {
		dma_unmap_single(ndev->dev.parent, cur_p->phys,
				(cur_p->cntrl & XAXIDMA_BD_CTRL_LENGTH_MASK),
				DMA_TO_DEVICE);
		if (cur_p->app4)
			dev_kfree_skb_irq((struct sk_buff *)cur_p->app4);
		
		cur_p->app0 = 0;
		cur_p->app1 = 0;
		cur_p->app2 = 0;
		cur_p->app4 = 0;
		cur_p->status = 0;

		size += status & XAXIDMA_BD_STS_ACTUAL_LEN_MASK;
		packets++;

		lp->tx_bd_ci = ++lp->tx_bd_ci % TX_BD_NUM;
		cur_p = &lp->tx_bd_v[lp->tx_bd_ci];
		status = cur_p->status;
	}

	ndev->stats.tx_packets += packets;
	ndev->stats.tx_bytes += size;
	netif_wake_queue(ndev);
}

static inline int axienet_check_tx_bd_space(struct axienet_local *lp,
					    int num_frag)
{
	struct axidma_bd *cur_p;
	cur_p = &lp->tx_bd_v[(lp->tx_bd_tail + num_frag) % TX_BD_NUM];
	if (cur_p->status & XAXIDMA_BD_STS_ALL_MASK)
		return NETDEV_TX_BUSY;
	return 0;
}

static int axienet_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	u32 ii;
	u32 num_frag;
	u32 csum_start_off;
	u32 csum_index_off;
	skb_frag_t *frag;
	dma_addr_t tail_p;
	struct axienet_local *lp = netdev_priv(ndev);
	struct axidma_bd *cur_p;

	num_frag = skb_shinfo(skb)->nr_frags;
	cur_p = &lp->tx_bd_v[lp->tx_bd_tail];

	if (axienet_check_tx_bd_space(lp, num_frag)) {
		if (!netif_queue_stopped(ndev))
			netif_stop_queue(ndev);
		return NETDEV_TX_BUSY;
	}

	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		if (lp->features & XAE_FEATURE_FULL_TX_CSUM) {
			
			cur_p->app0 |= 2;
		} else if (lp->features & XAE_FEATURE_PARTIAL_RX_CSUM) {
			csum_start_off = skb_transport_offset(skb);
			csum_index_off = csum_start_off + skb->csum_offset;
			
			cur_p->app0 |= 1;
			cur_p->app1 = (csum_start_off << 16) | csum_index_off;
		}
	} else if (skb->ip_summed == CHECKSUM_UNNECESSARY) {
		cur_p->app0 |= 2; 
	}

	cur_p->cntrl = skb_headlen(skb) | XAXIDMA_BD_CTRL_TXSOF_MASK;
	cur_p->phys = dma_map_single(ndev->dev.parent, skb->data,
				     skb_headlen(skb), DMA_TO_DEVICE);

	for (ii = 0; ii < num_frag; ii++) {
		lp->tx_bd_tail = ++lp->tx_bd_tail % TX_BD_NUM;
		cur_p = &lp->tx_bd_v[lp->tx_bd_tail];
		frag = &skb_shinfo(skb)->frags[ii];
		cur_p->phys = dma_map_single(ndev->dev.parent,
					     skb_frag_address(frag),
					     skb_frag_size(frag),
					     DMA_TO_DEVICE);
		cur_p->cntrl = skb_frag_size(frag);
	}

	cur_p->cntrl |= XAXIDMA_BD_CTRL_TXEOF_MASK;
	cur_p->app4 = (unsigned long)skb;

	tail_p = lp->tx_bd_p + sizeof(*lp->tx_bd_v) * lp->tx_bd_tail;
	
	axienet_dma_out32(lp, XAXIDMA_TX_TDESC_OFFSET, tail_p);
	lp->tx_bd_tail = ++lp->tx_bd_tail % TX_BD_NUM;

	return NETDEV_TX_OK;
}

static void axienet_recv(struct net_device *ndev)
{
	u32 length;
	u32 csumstatus;
	u32 size = 0;
	u32 packets = 0;
	dma_addr_t tail_p;
	struct axienet_local *lp = netdev_priv(ndev);
	struct sk_buff *skb, *new_skb;
	struct axidma_bd *cur_p;

	tail_p = lp->rx_bd_p + sizeof(*lp->rx_bd_v) * lp->rx_bd_ci;
	cur_p = &lp->rx_bd_v[lp->rx_bd_ci];

	while ((cur_p->status & XAXIDMA_BD_STS_COMPLETE_MASK)) {
		skb = (struct sk_buff *) (cur_p->sw_id_offset);
		length = cur_p->app4 & 0x0000FFFF;

		dma_unmap_single(ndev->dev.parent, cur_p->phys,
				 lp->max_frm_size,
				 DMA_FROM_DEVICE);

		skb_put(skb, length);
		skb->protocol = eth_type_trans(skb, ndev);
		
		skb->ip_summed = CHECKSUM_NONE;

		
		if (lp->features & XAE_FEATURE_FULL_RX_CSUM) {
			csumstatus = (cur_p->app2 &
				      XAE_FULL_CSUM_STATUS_MASK) >> 3;
			if ((csumstatus == XAE_IP_TCP_CSUM_VALIDATED) ||
			    (csumstatus == XAE_IP_UDP_CSUM_VALIDATED)) {
				skb->ip_summed = CHECKSUM_UNNECESSARY;
			}
		} else if ((lp->features & XAE_FEATURE_PARTIAL_RX_CSUM) != 0 &&
			   skb->protocol == __constant_htons(ETH_P_IP) &&
			   skb->len > 64) {
			skb->csum = be32_to_cpu(cur_p->app3 & 0xFFFF);
			skb->ip_summed = CHECKSUM_COMPLETE;
		}

		netif_rx(skb);

		size += length;
		packets++;

		new_skb = netdev_alloc_skb_ip_align(ndev, lp->max_frm_size);
		if (!new_skb) {
			dev_err(&ndev->dev, "no memory for new sk_buff\n");
			return;
		}
		cur_p->phys = dma_map_single(ndev->dev.parent, new_skb->data,
					     lp->max_frm_size,
					     DMA_FROM_DEVICE);
		cur_p->cntrl = lp->max_frm_size;
		cur_p->status = 0;
		cur_p->sw_id_offset = (u32) new_skb;

		lp->rx_bd_ci = ++lp->rx_bd_ci % RX_BD_NUM;
		cur_p = &lp->rx_bd_v[lp->rx_bd_ci];
	}

	ndev->stats.rx_packets += packets;
	ndev->stats.rx_bytes += size;

	axienet_dma_out32(lp, XAXIDMA_RX_TDESC_OFFSET, tail_p);
}

static irqreturn_t axienet_tx_irq(int irq, void *_ndev)
{
	u32 cr;
	unsigned int status;
	struct net_device *ndev = _ndev;
	struct axienet_local *lp = netdev_priv(ndev);

	status = axienet_dma_in32(lp, XAXIDMA_TX_SR_OFFSET);
	if (status & (XAXIDMA_IRQ_IOC_MASK | XAXIDMA_IRQ_DELAY_MASK)) {
		axienet_start_xmit_done(lp->ndev);
		goto out;
	}
	if (!(status & XAXIDMA_IRQ_ALL_MASK))
		dev_err(&ndev->dev, "No interrupts asserted in Tx path");
	if (status & XAXIDMA_IRQ_ERROR_MASK) {
		dev_err(&ndev->dev, "DMA Tx error 0x%x\n", status);
		dev_err(&ndev->dev, "Current BD is at: 0x%x\n",
			(lp->tx_bd_v[lp->tx_bd_ci]).phys);

		cr = axienet_dma_in32(lp, XAXIDMA_TX_CR_OFFSET);
		
		cr &= (~XAXIDMA_IRQ_ALL_MASK);
		
		axienet_dma_out32(lp, XAXIDMA_TX_CR_OFFSET, cr);

		cr = axienet_dma_in32(lp, XAXIDMA_RX_CR_OFFSET);
		
		cr &= (~XAXIDMA_IRQ_ALL_MASK);
		
		axienet_dma_out32(lp, XAXIDMA_RX_CR_OFFSET, cr);

		tasklet_schedule(&lp->dma_err_tasklet);
	}
out:
	axienet_dma_out32(lp, XAXIDMA_TX_SR_OFFSET, status);
	return IRQ_HANDLED;
}

static irqreturn_t axienet_rx_irq(int irq, void *_ndev)
{
	u32 cr;
	unsigned int status;
	struct net_device *ndev = _ndev;
	struct axienet_local *lp = netdev_priv(ndev);

	status = axienet_dma_in32(lp, XAXIDMA_RX_SR_OFFSET);
	if (status & (XAXIDMA_IRQ_IOC_MASK | XAXIDMA_IRQ_DELAY_MASK)) {
		axienet_recv(lp->ndev);
		goto out;
	}
	if (!(status & XAXIDMA_IRQ_ALL_MASK))
		dev_err(&ndev->dev, "No interrupts asserted in Rx path");
	if (status & XAXIDMA_IRQ_ERROR_MASK) {
		dev_err(&ndev->dev, "DMA Rx error 0x%x\n", status);
		dev_err(&ndev->dev, "Current BD is at: 0x%x\n",
			(lp->rx_bd_v[lp->rx_bd_ci]).phys);

		cr = axienet_dma_in32(lp, XAXIDMA_TX_CR_OFFSET);
		
		cr &= (~XAXIDMA_IRQ_ALL_MASK);
		
		axienet_dma_out32(lp, XAXIDMA_TX_CR_OFFSET, cr);

		cr = axienet_dma_in32(lp, XAXIDMA_RX_CR_OFFSET);
		
		cr &= (~XAXIDMA_IRQ_ALL_MASK);
		
		axienet_dma_out32(lp, XAXIDMA_RX_CR_OFFSET, cr);

		tasklet_schedule(&lp->dma_err_tasklet);
	}
out:
	axienet_dma_out32(lp, XAXIDMA_RX_SR_OFFSET, status);
	return IRQ_HANDLED;
}

static int axienet_open(struct net_device *ndev)
{
	int ret, mdio_mcreg;
	struct axienet_local *lp = netdev_priv(ndev);

	dev_dbg(&ndev->dev, "axienet_open()\n");

	mdio_mcreg = axienet_ior(lp, XAE_MDIO_MC_OFFSET);
	ret = axienet_mdio_wait_until_ready(lp);
	if (ret < 0)
		return ret;
	axienet_iow(lp, XAE_MDIO_MC_OFFSET,
		    (mdio_mcreg & (~XAE_MDIO_MC_MDIOEN_MASK)));
	axienet_device_reset(ndev);
	
	axienet_iow(lp, XAE_MDIO_MC_OFFSET, mdio_mcreg);
	ret = axienet_mdio_wait_until_ready(lp);
	if (ret < 0)
		return ret;

	if (lp->phy_node) {
		lp->phy_dev = of_phy_connect(lp->ndev, lp->phy_node,
					     axienet_adjust_link, 0,
					     PHY_INTERFACE_MODE_GMII);
		if (!lp->phy_dev) {
			dev_err(lp->dev, "of_phy_connect() failed\n");
			return -ENODEV;
		}
		phy_start(lp->phy_dev);
	}

	
	ret = request_irq(lp->tx_irq, axienet_tx_irq, 0, ndev->name, ndev);
	if (ret)
		goto err_tx_irq;
	
	ret = request_irq(lp->rx_irq, axienet_rx_irq, 0, ndev->name, ndev);
	if (ret)
		goto err_rx_irq;
	
	tasklet_enable(&lp->dma_err_tasklet);
	return 0;

err_rx_irq:
	free_irq(lp->tx_irq, ndev);
err_tx_irq:
	if (lp->phy_dev)
		phy_disconnect(lp->phy_dev);
	lp->phy_dev = NULL;
	dev_err(lp->dev, "request_irq() failed\n");
	return ret;
}

static int axienet_stop(struct net_device *ndev)
{
	u32 cr;
	struct axienet_local *lp = netdev_priv(ndev);

	dev_dbg(&ndev->dev, "axienet_close()\n");

	cr = axienet_dma_in32(lp, XAXIDMA_RX_CR_OFFSET);
	axienet_dma_out32(lp, XAXIDMA_RX_CR_OFFSET,
			  cr & (~XAXIDMA_CR_RUNSTOP_MASK));
	cr = axienet_dma_in32(lp, XAXIDMA_TX_CR_OFFSET);
	axienet_dma_out32(lp, XAXIDMA_TX_CR_OFFSET,
			  cr & (~XAXIDMA_CR_RUNSTOP_MASK));
	axienet_setoptions(ndev, lp->options &
			   ~(XAE_OPTION_TXEN | XAE_OPTION_RXEN));

	tasklet_disable(&lp->dma_err_tasklet);

	free_irq(lp->tx_irq, ndev);
	free_irq(lp->rx_irq, ndev);

	if (lp->phy_dev)
		phy_disconnect(lp->phy_dev);
	lp->phy_dev = NULL;

	axienet_dma_bd_release(ndev);
	return 0;
}

static int axienet_change_mtu(struct net_device *ndev, int new_mtu)
{
	struct axienet_local *lp = netdev_priv(ndev);

	if (netif_running(ndev))
		return -EBUSY;
	if (lp->jumbo_support) {
		if ((new_mtu > XAE_JUMBO_MTU) || (new_mtu < 64))
			return -EINVAL;
		ndev->mtu = new_mtu;
	} else {
		if ((new_mtu > XAE_MTU) || (new_mtu < 64))
			return -EINVAL;
		ndev->mtu = new_mtu;
	}

	return 0;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void axienet_poll_controller(struct net_device *ndev)
{
	struct axienet_local *lp = netdev_priv(ndev);
	disable_irq(lp->tx_irq);
	disable_irq(lp->rx_irq);
	axienet_rx_irq(lp->tx_irq, ndev);
	axienet_tx_irq(lp->rx_irq, ndev);
	enable_irq(lp->tx_irq);
	enable_irq(lp->rx_irq);
}
#endif

static const struct net_device_ops axienet_netdev_ops = {
	.ndo_open = axienet_open,
	.ndo_stop = axienet_stop,
	.ndo_start_xmit = axienet_start_xmit,
	.ndo_change_mtu	= axienet_change_mtu,
	.ndo_set_mac_address = netdev_set_mac_address,
	.ndo_validate_addr = eth_validate_addr,
	.ndo_set_rx_mode = axienet_set_multicast_list,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller = axienet_poll_controller,
#endif
};

static int axienet_ethtools_get_settings(struct net_device *ndev,
					 struct ethtool_cmd *ecmd)
{
	struct axienet_local *lp = netdev_priv(ndev);
	struct phy_device *phydev = lp->phy_dev;
	if (!phydev)
		return -ENODEV;
	return phy_ethtool_gset(phydev, ecmd);
}

static int axienet_ethtools_set_settings(struct net_device *ndev,
					 struct ethtool_cmd *ecmd)
{
	struct axienet_local *lp = netdev_priv(ndev);
	struct phy_device *phydev = lp->phy_dev;
	if (!phydev)
		return -ENODEV;
	return phy_ethtool_sset(phydev, ecmd);
}

static void axienet_ethtools_get_drvinfo(struct net_device *ndev,
					 struct ethtool_drvinfo *ed)
{
	memset(ed, 0, sizeof(struct ethtool_drvinfo));
	strcpy(ed->driver, DRIVER_NAME);
	strcpy(ed->version, DRIVER_VERSION);
	ed->regdump_len = sizeof(u32) * AXIENET_REGS_N;
}

static int axienet_ethtools_get_regs_len(struct net_device *ndev)
{
	return sizeof(u32) * AXIENET_REGS_N;
}

static void axienet_ethtools_get_regs(struct net_device *ndev,
				      struct ethtool_regs *regs, void *ret)
{
	u32 *data = (u32 *) ret;
	size_t len = sizeof(u32) * AXIENET_REGS_N;
	struct axienet_local *lp = netdev_priv(ndev);

	regs->version = 0;
	regs->len = len;

	memset(data, 0, len);
	data[0] = axienet_ior(lp, XAE_RAF_OFFSET);
	data[1] = axienet_ior(lp, XAE_TPF_OFFSET);
	data[2] = axienet_ior(lp, XAE_IFGP_OFFSET);
	data[3] = axienet_ior(lp, XAE_IS_OFFSET);
	data[4] = axienet_ior(lp, XAE_IP_OFFSET);
	data[5] = axienet_ior(lp, XAE_IE_OFFSET);
	data[6] = axienet_ior(lp, XAE_TTAG_OFFSET);
	data[7] = axienet_ior(lp, XAE_RTAG_OFFSET);
	data[8] = axienet_ior(lp, XAE_UAWL_OFFSET);
	data[9] = axienet_ior(lp, XAE_UAWU_OFFSET);
	data[10] = axienet_ior(lp, XAE_TPID0_OFFSET);
	data[11] = axienet_ior(lp, XAE_TPID1_OFFSET);
	data[12] = axienet_ior(lp, XAE_PPST_OFFSET);
	data[13] = axienet_ior(lp, XAE_RCW0_OFFSET);
	data[14] = axienet_ior(lp, XAE_RCW1_OFFSET);
	data[15] = axienet_ior(lp, XAE_TC_OFFSET);
	data[16] = axienet_ior(lp, XAE_FCC_OFFSET);
	data[17] = axienet_ior(lp, XAE_EMMC_OFFSET);
	data[18] = axienet_ior(lp, XAE_PHYC_OFFSET);
	data[19] = axienet_ior(lp, XAE_MDIO_MC_OFFSET);
	data[20] = axienet_ior(lp, XAE_MDIO_MCR_OFFSET);
	data[21] = axienet_ior(lp, XAE_MDIO_MWD_OFFSET);
	data[22] = axienet_ior(lp, XAE_MDIO_MRD_OFFSET);
	data[23] = axienet_ior(lp, XAE_MDIO_MIS_OFFSET);
	data[24] = axienet_ior(lp, XAE_MDIO_MIP_OFFSET);
	data[25] = axienet_ior(lp, XAE_MDIO_MIE_OFFSET);
	data[26] = axienet_ior(lp, XAE_MDIO_MIC_OFFSET);
	data[27] = axienet_ior(lp, XAE_UAW0_OFFSET);
	data[28] = axienet_ior(lp, XAE_UAW1_OFFSET);
	data[29] = axienet_ior(lp, XAE_FMI_OFFSET);
	data[30] = axienet_ior(lp, XAE_AF0_OFFSET);
	data[31] = axienet_ior(lp, XAE_AF1_OFFSET);
}

static void
axienet_ethtools_get_pauseparam(struct net_device *ndev,
				struct ethtool_pauseparam *epauseparm)
{
	u32 regval;
	struct axienet_local *lp = netdev_priv(ndev);
	epauseparm->autoneg  = 0;
	regval = axienet_ior(lp, XAE_FCC_OFFSET);
	epauseparm->tx_pause = regval & XAE_FCC_FCTX_MASK;
	epauseparm->rx_pause = regval & XAE_FCC_FCRX_MASK;
}

static int
axienet_ethtools_set_pauseparam(struct net_device *ndev,
				struct ethtool_pauseparam *epauseparm)
{
	u32 regval = 0;
	struct axienet_local *lp = netdev_priv(ndev);

	if (netif_running(ndev)) {
		printk(KERN_ERR	"%s: Please stop netif before applying "
		       "configruation\n", ndev->name);
		return -EFAULT;
	}

	regval = axienet_ior(lp, XAE_FCC_OFFSET);
	if (epauseparm->tx_pause)
		regval |= XAE_FCC_FCTX_MASK;
	else
		regval &= ~XAE_FCC_FCTX_MASK;
	if (epauseparm->rx_pause)
		regval |= XAE_FCC_FCRX_MASK;
	else
		regval &= ~XAE_FCC_FCRX_MASK;
	axienet_iow(lp, XAE_FCC_OFFSET, regval);

	return 0;
}

static int axienet_ethtools_get_coalesce(struct net_device *ndev,
					 struct ethtool_coalesce *ecoalesce)
{
	u32 regval = 0;
	struct axienet_local *lp = netdev_priv(ndev);
	regval = axienet_dma_in32(lp, XAXIDMA_RX_CR_OFFSET);
	ecoalesce->rx_max_coalesced_frames = (regval & XAXIDMA_COALESCE_MASK)
					     >> XAXIDMA_COALESCE_SHIFT;
	regval = axienet_dma_in32(lp, XAXIDMA_TX_CR_OFFSET);
	ecoalesce->tx_max_coalesced_frames = (regval & XAXIDMA_COALESCE_MASK)
					     >> XAXIDMA_COALESCE_SHIFT;
	return 0;
}

static int axienet_ethtools_set_coalesce(struct net_device *ndev,
					 struct ethtool_coalesce *ecoalesce)
{
	struct axienet_local *lp = netdev_priv(ndev);

	if (netif_running(ndev)) {
		printk(KERN_ERR	"%s: Please stop netif before applying "
		       "configruation\n", ndev->name);
		return -EFAULT;
	}

	if ((ecoalesce->rx_coalesce_usecs) ||
	    (ecoalesce->rx_coalesce_usecs_irq) ||
	    (ecoalesce->rx_max_coalesced_frames_irq) ||
	    (ecoalesce->tx_coalesce_usecs) ||
	    (ecoalesce->tx_coalesce_usecs_irq) ||
	    (ecoalesce->tx_max_coalesced_frames_irq) ||
	    (ecoalesce->stats_block_coalesce_usecs) ||
	    (ecoalesce->use_adaptive_rx_coalesce) ||
	    (ecoalesce->use_adaptive_tx_coalesce) ||
	    (ecoalesce->pkt_rate_low) ||
	    (ecoalesce->rx_coalesce_usecs_low) ||
	    (ecoalesce->rx_max_coalesced_frames_low) ||
	    (ecoalesce->tx_coalesce_usecs_low) ||
	    (ecoalesce->tx_max_coalesced_frames_low) ||
	    (ecoalesce->pkt_rate_high) ||
	    (ecoalesce->rx_coalesce_usecs_high) ||
	    (ecoalesce->rx_max_coalesced_frames_high) ||
	    (ecoalesce->tx_coalesce_usecs_high) ||
	    (ecoalesce->tx_max_coalesced_frames_high) ||
	    (ecoalesce->rate_sample_interval))
		return -EOPNOTSUPP;
	if (ecoalesce->rx_max_coalesced_frames)
		lp->coalesce_count_rx = ecoalesce->rx_max_coalesced_frames;
	if (ecoalesce->tx_max_coalesced_frames)
		lp->coalesce_count_tx = ecoalesce->tx_max_coalesced_frames;

	return 0;
}

static struct ethtool_ops axienet_ethtool_ops = {
	.get_settings   = axienet_ethtools_get_settings,
	.set_settings   = axienet_ethtools_set_settings,
	.get_drvinfo    = axienet_ethtools_get_drvinfo,
	.get_regs_len   = axienet_ethtools_get_regs_len,
	.get_regs       = axienet_ethtools_get_regs,
	.get_link       = ethtool_op_get_link,
	.get_pauseparam = axienet_ethtools_get_pauseparam,
	.set_pauseparam = axienet_ethtools_set_pauseparam,
	.get_coalesce   = axienet_ethtools_get_coalesce,
	.set_coalesce   = axienet_ethtools_set_coalesce,
};

static void axienet_dma_err_handler(unsigned long data)
{
	u32 axienet_status;
	u32 cr, i;
	int mdio_mcreg;
	struct axienet_local *lp = (struct axienet_local *) data;
	struct net_device *ndev = lp->ndev;
	struct axidma_bd *cur_p;

	axienet_setoptions(ndev, lp->options &
			   ~(XAE_OPTION_TXEN | XAE_OPTION_RXEN));
	mdio_mcreg = axienet_ior(lp, XAE_MDIO_MC_OFFSET);
	axienet_mdio_wait_until_ready(lp);
	axienet_iow(lp, XAE_MDIO_MC_OFFSET, (mdio_mcreg &
		    ~XAE_MDIO_MC_MDIOEN_MASK));

	__axienet_device_reset(lp, &ndev->dev, XAXIDMA_TX_CR_OFFSET);
	__axienet_device_reset(lp, &ndev->dev, XAXIDMA_RX_CR_OFFSET);

	axienet_iow(lp, XAE_MDIO_MC_OFFSET, mdio_mcreg);
	axienet_mdio_wait_until_ready(lp);

	for (i = 0; i < TX_BD_NUM; i++) {
		cur_p = &lp->tx_bd_v[i];
		if (cur_p->phys)
			dma_unmap_single(ndev->dev.parent, cur_p->phys,
					 (cur_p->cntrl &
					  XAXIDMA_BD_CTRL_LENGTH_MASK),
					 DMA_TO_DEVICE);
		if (cur_p->app4)
			dev_kfree_skb_irq((struct sk_buff *) cur_p->app4);
		cur_p->phys = 0;
		cur_p->cntrl = 0;
		cur_p->status = 0;
		cur_p->app0 = 0;
		cur_p->app1 = 0;
		cur_p->app2 = 0;
		cur_p->app3 = 0;
		cur_p->app4 = 0;
		cur_p->sw_id_offset = 0;
	}

	for (i = 0; i < RX_BD_NUM; i++) {
		cur_p = &lp->rx_bd_v[i];
		cur_p->status = 0;
		cur_p->app0 = 0;
		cur_p->app1 = 0;
		cur_p->app2 = 0;
		cur_p->app3 = 0;
		cur_p->app4 = 0;
	}

	lp->tx_bd_ci = 0;
	lp->tx_bd_tail = 0;
	lp->rx_bd_ci = 0;

	
	cr = axienet_dma_in32(lp, XAXIDMA_RX_CR_OFFSET);
	
	cr = ((cr & ~XAXIDMA_COALESCE_MASK) |
	      (XAXIDMA_DFT_RX_THRESHOLD << XAXIDMA_COALESCE_SHIFT));
	
	cr = ((cr & ~XAXIDMA_DELAY_MASK) |
	      (XAXIDMA_DFT_RX_WAITBOUND << XAXIDMA_DELAY_SHIFT));
	
	cr |= XAXIDMA_IRQ_ALL_MASK;
	
	axienet_dma_out32(lp, XAXIDMA_RX_CR_OFFSET, cr);

	
	cr = axienet_dma_in32(lp, XAXIDMA_TX_CR_OFFSET);
	
	cr = (((cr & ~XAXIDMA_COALESCE_MASK)) |
	      (XAXIDMA_DFT_TX_THRESHOLD << XAXIDMA_COALESCE_SHIFT));
	
	cr = (((cr & ~XAXIDMA_DELAY_MASK)) |
	      (XAXIDMA_DFT_TX_WAITBOUND << XAXIDMA_DELAY_SHIFT));
	
	cr |= XAXIDMA_IRQ_ALL_MASK;
	
	axienet_dma_out32(lp, XAXIDMA_TX_CR_OFFSET, cr);

	axienet_dma_out32(lp, XAXIDMA_RX_CDESC_OFFSET, lp->rx_bd_p);
	cr = axienet_dma_in32(lp, XAXIDMA_RX_CR_OFFSET);
	axienet_dma_out32(lp, XAXIDMA_RX_CR_OFFSET,
			  cr | XAXIDMA_CR_RUNSTOP_MASK);
	axienet_dma_out32(lp, XAXIDMA_RX_TDESC_OFFSET, lp->rx_bd_p +
			  (sizeof(*lp->rx_bd_v) * (RX_BD_NUM - 1)));

	axienet_dma_out32(lp, XAXIDMA_TX_CDESC_OFFSET, lp->tx_bd_p);
	cr = axienet_dma_in32(lp, XAXIDMA_TX_CR_OFFSET);
	axienet_dma_out32(lp, XAXIDMA_TX_CR_OFFSET,
			  cr | XAXIDMA_CR_RUNSTOP_MASK);

	axienet_status = axienet_ior(lp, XAE_RCW1_OFFSET);
	axienet_status &= ~XAE_RCW1_RX_MASK;
	axienet_iow(lp, XAE_RCW1_OFFSET, axienet_status);

	axienet_status = axienet_ior(lp, XAE_IP_OFFSET);
	if (axienet_status & XAE_INT_RXRJECT_MASK)
		axienet_iow(lp, XAE_IS_OFFSET, XAE_INT_RXRJECT_MASK);
	axienet_iow(lp, XAE_FCC_OFFSET, XAE_FCC_FCRX_MASK);

	axienet_setoptions(ndev, lp->options &
			   ~(XAE_OPTION_TXEN | XAE_OPTION_RXEN));
	axienet_set_mac_address(ndev, NULL);
	axienet_set_multicast_list(ndev);
	axienet_setoptions(ndev, lp->options);
}

static int __devinit axienet_of_probe(struct platform_device *op)
{
	__be32 *p;
	int size, ret = 0;
	struct device_node *np;
	struct axienet_local *lp;
	struct net_device *ndev;
	const void *addr;

	ndev = alloc_etherdev(sizeof(*lp));
	if (!ndev)
		return -ENOMEM;

	ether_setup(ndev);
	dev_set_drvdata(&op->dev, ndev);

	SET_NETDEV_DEV(ndev, &op->dev);
	ndev->flags &= ~IFF_MULTICAST;  
	ndev->features = NETIF_F_SG | NETIF_F_FRAGLIST;
	ndev->netdev_ops = &axienet_netdev_ops;
	ndev->ethtool_ops = &axienet_ethtool_ops;

	lp = netdev_priv(ndev);
	lp->ndev = ndev;
	lp->dev = &op->dev;
	lp->options = XAE_OPTION_DEFAULTS;
	
	lp->regs = of_iomap(op->dev.of_node, 0);
	if (!lp->regs) {
		dev_err(&op->dev, "could not map Axi Ethernet regs.\n");
		goto nodev;
	}
	
	lp->features = 0;

	p = (__be32 *) of_get_property(op->dev.of_node, "xlnx,txcsum", NULL);
	if (p) {
		switch (be32_to_cpup(p)) {
		case 1:
			lp->csum_offload_on_tx_path =
				XAE_FEATURE_PARTIAL_TX_CSUM;
			lp->features |= XAE_FEATURE_PARTIAL_TX_CSUM;
			
			ndev->features |= NETIF_F_IP_CSUM;
			break;
		case 2:
			lp->csum_offload_on_tx_path =
				XAE_FEATURE_FULL_TX_CSUM;
			lp->features |= XAE_FEATURE_FULL_TX_CSUM;
			
			ndev->features |= NETIF_F_IP_CSUM;
			break;
		default:
			lp->csum_offload_on_tx_path = XAE_NO_CSUM_OFFLOAD;
		}
	}
	p = (__be32 *) of_get_property(op->dev.of_node, "xlnx,rxcsum", NULL);
	if (p) {
		switch (be32_to_cpup(p)) {
		case 1:
			lp->csum_offload_on_rx_path =
				XAE_FEATURE_PARTIAL_RX_CSUM;
			lp->features |= XAE_FEATURE_PARTIAL_RX_CSUM;
			break;
		case 2:
			lp->csum_offload_on_rx_path =
				XAE_FEATURE_FULL_RX_CSUM;
			lp->features |= XAE_FEATURE_FULL_RX_CSUM;
			break;
		default:
			lp->csum_offload_on_rx_path = XAE_NO_CSUM_OFFLOAD;
		}
	}
	p = (__be32 *) of_get_property(op->dev.of_node, "xlnx,rxmem", NULL);
	if (p) {
		if ((be32_to_cpup(p)) >= 0x4000)
			lp->jumbo_support = 1;
	}
	p = (__be32 *) of_get_property(op->dev.of_node, "xlnx,temac-type",
				       NULL);
	if (p)
		lp->temac_type = be32_to_cpup(p);
	p = (__be32 *) of_get_property(op->dev.of_node, "xlnx,phy-type", NULL);
	if (p)
		lp->phy_type = be32_to_cpup(p);

	
	np = of_parse_phandle(op->dev.of_node, "axistream-connected", 0);
	if (!np) {
		dev_err(&op->dev, "could not find DMA node\n");
		goto err_iounmap;
	}
	lp->dma_regs = of_iomap(np, 0);
	if (lp->dma_regs) {
		dev_dbg(&op->dev, "MEM base: %p\n", lp->dma_regs);
	} else {
		dev_err(&op->dev, "unable to map DMA registers\n");
		of_node_put(np);
	}
	lp->rx_irq = irq_of_parse_and_map(np, 1);
	lp->tx_irq = irq_of_parse_and_map(np, 0);
	of_node_put(np);
	if ((lp->rx_irq == NO_IRQ) || (lp->tx_irq == NO_IRQ)) {
		dev_err(&op->dev, "could not determine irqs\n");
		ret = -ENOMEM;
		goto err_iounmap_2;
	}

	
	addr = of_get_property(op->dev.of_node, "local-mac-address", &size);
	if ((!addr) || (size != 6)) {
		dev_err(&op->dev, "could not find MAC address\n");
		ret = -ENODEV;
		goto err_iounmap_2;
	}
	axienet_set_mac_address(ndev, (void *) addr);

	lp->coalesce_count_rx = XAXIDMA_DFT_RX_THRESHOLD;
	lp->coalesce_count_tx = XAXIDMA_DFT_TX_THRESHOLD;

	lp->phy_node = of_parse_phandle(op->dev.of_node, "phy-handle", 0);
	ret = axienet_mdio_setup(lp, op->dev.of_node);
	if (ret)
		dev_warn(&op->dev, "error registering MDIO bus\n");

	ret = register_netdev(lp->ndev);
	if (ret) {
		dev_err(lp->dev, "register_netdev() error (%i)\n", ret);
		goto err_iounmap_2;
	}

	tasklet_init(&lp->dma_err_tasklet, axienet_dma_err_handler,
		     (unsigned long) lp);
	tasklet_disable(&lp->dma_err_tasklet);

	return 0;

err_iounmap_2:
	if (lp->dma_regs)
		iounmap(lp->dma_regs);
err_iounmap:
	iounmap(lp->regs);
nodev:
	free_netdev(ndev);
	ndev = NULL;
	return ret;
}

static int __devexit axienet_of_remove(struct platform_device *op)
{
	struct net_device *ndev = dev_get_drvdata(&op->dev);
	struct axienet_local *lp = netdev_priv(ndev);

	axienet_mdio_teardown(lp);
	unregister_netdev(ndev);

	if (lp->phy_node)
		of_node_put(lp->phy_node);
	lp->phy_node = NULL;

	dev_set_drvdata(&op->dev, NULL);

	iounmap(lp->regs);
	if (lp->dma_regs)
		iounmap(lp->dma_regs);
	free_netdev(ndev);

	return 0;
}

static struct platform_driver axienet_of_driver = {
	.probe = axienet_of_probe,
	.remove = __devexit_p(axienet_of_remove),
	.driver = {
		 .owner = THIS_MODULE,
		 .name = "xilinx_axienet",
		 .of_match_table = axienet_of_match,
	},
};

module_platform_driver(axienet_of_driver);

MODULE_DESCRIPTION("Xilinx Axi Ethernet driver");
MODULE_AUTHOR("Xilinx");
MODULE_LICENSE("GPL");
