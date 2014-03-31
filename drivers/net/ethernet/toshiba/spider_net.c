/*
 * Network device driver for Cell Processor-Based Blade and Celleb platform
 *
 * (C) Copyright IBM Corp. 2005
 * (C) Copyright 2006 TOSHIBA CORPORATION
 *
 * Authors : Utz Bacher <utz.bacher@de.ibm.com>
 *           Jens Osterkamp <Jens.Osterkamp@de.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/compiler.h>
#include <linux/crc32.h>
#include <linux/delay.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/firmware.h>
#include <linux/if_vlan.h>
#include <linux/in.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/gfp.h>
#include <linux/ioport.h>
#include <linux/ip.h>
#include <linux/kernel.h>
#include <linux/mii.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/skbuff.h>
#include <linux/tcp.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/bitops.h>
#include <asm/pci-bridge.h>
#include <net/checksum.h>

#include "spider_net.h"

MODULE_AUTHOR("Utz Bacher <utz.bacher@de.ibm.com> and Jens Osterkamp " \
	      "<Jens.Osterkamp@de.ibm.com>");
MODULE_DESCRIPTION("Spider Southbridge Gigabit Ethernet driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(VERSION);
MODULE_FIRMWARE(SPIDER_NET_FIRMWARE_NAME);

static int rx_descriptors = SPIDER_NET_RX_DESCRIPTORS_DEFAULT;
static int tx_descriptors = SPIDER_NET_TX_DESCRIPTORS_DEFAULT;

module_param(rx_descriptors, int, 0444);
module_param(tx_descriptors, int, 0444);

MODULE_PARM_DESC(rx_descriptors, "number of descriptors used " \
		 "in rx chains");
MODULE_PARM_DESC(tx_descriptors, "number of descriptors used " \
		 "in tx chain");

char spider_net_driver_name[] = "spidernet";

static DEFINE_PCI_DEVICE_TABLE(spider_net_pci_tbl) = {
	{ PCI_VENDOR_ID_TOSHIBA_2, PCI_DEVICE_ID_TOSHIBA_SPIDER_NET,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, spider_net_pci_tbl);

static inline u32
spider_net_read_reg(struct spider_net_card *card, u32 reg)
{
	return in_be32(card->regs + reg);
}

static inline void
spider_net_write_reg(struct spider_net_card *card, u32 reg, u32 value)
{
	out_be32(card->regs + reg, value);
}

/** spider_net_write_phy - write to phy register
 * @netdev: adapter to be written to
 * @mii_id: id of MII
 * @reg: PHY register
 * @val: value to be written to phy register
 *
 * spider_net_write_phy_register writes to an arbitrary PHY
 * register via the spider GPCWOPCMD register. We assume the queue does
 * not run full (not more than 15 commands outstanding).
 **/
static void
spider_net_write_phy(struct net_device *netdev, int mii_id,
		     int reg, int val)
{
	struct spider_net_card *card = netdev_priv(netdev);
	u32 writevalue;

	writevalue = ((u32)mii_id << 21) |
		((u32)reg << 16) | ((u32)val);

	spider_net_write_reg(card, SPIDER_NET_GPCWOPCMD, writevalue);
}

static int
spider_net_read_phy(struct net_device *netdev, int mii_id, int reg)
{
	struct spider_net_card *card = netdev_priv(netdev);
	u32 readvalue;

	readvalue = ((u32)mii_id << 21) | ((u32)reg << 16);
	spider_net_write_reg(card, SPIDER_NET_GPCROPCMD, readvalue);

	do {
		readvalue = spider_net_read_reg(card, SPIDER_NET_GPCROPCMD);
	} while (readvalue & SPIDER_NET_GPREXEC);

	readvalue &= SPIDER_NET_GPRDAT_MASK;

	return readvalue;
}

static void
spider_net_setup_aneg(struct spider_net_card *card)
{
	struct mii_phy *phy = &card->phy;
	u32 advertise = 0;
	u16 bmsr, estat;

	bmsr  = spider_net_read_phy(card->netdev, phy->mii_id, MII_BMSR);
	estat = spider_net_read_phy(card->netdev, phy->mii_id, MII_ESTATUS);

	if (bmsr & BMSR_10HALF)
		advertise |= ADVERTISED_10baseT_Half;
	if (bmsr & BMSR_10FULL)
		advertise |= ADVERTISED_10baseT_Full;
	if (bmsr & BMSR_100HALF)
		advertise |= ADVERTISED_100baseT_Half;
	if (bmsr & BMSR_100FULL)
		advertise |= ADVERTISED_100baseT_Full;

	if ((bmsr & BMSR_ESTATEN) && (estat & ESTATUS_1000_TFULL))
		advertise |= SUPPORTED_1000baseT_Full;
	if ((bmsr & BMSR_ESTATEN) && (estat & ESTATUS_1000_THALF))
		advertise |= SUPPORTED_1000baseT_Half;

	sungem_phy_probe(phy, phy->mii_id);
	phy->def->ops->setup_aneg(phy, advertise);

}

static void
spider_net_rx_irq_off(struct spider_net_card *card)
{
	u32 regvalue;

	regvalue = SPIDER_NET_INT0_MASK_VALUE & (~SPIDER_NET_RXINT);
	spider_net_write_reg(card, SPIDER_NET_GHIINT0MSK, regvalue);
}

static void
spider_net_rx_irq_on(struct spider_net_card *card)
{
	u32 regvalue;

	regvalue = SPIDER_NET_INT0_MASK_VALUE | SPIDER_NET_RXINT;
	spider_net_write_reg(card, SPIDER_NET_GHIINT0MSK, regvalue);
}

static void
spider_net_set_promisc(struct spider_net_card *card)
{
	u32 macu, macl;
	struct net_device *netdev = card->netdev;

	if (netdev->flags & IFF_PROMISC) {
		
		spider_net_write_reg(card, SPIDER_NET_GMRUAFILnR, 0);
		spider_net_write_reg(card, SPIDER_NET_GMRUAFILnR + 0x04, 0);
		spider_net_write_reg(card, SPIDER_NET_GMRUA0FIL15R,
				     SPIDER_NET_PROMISC_VALUE);
	} else {
		macu = netdev->dev_addr[0];
		macu <<= 8;
		macu |= netdev->dev_addr[1];
		memcpy(&macl, &netdev->dev_addr[2], sizeof(macl));

		macu |= SPIDER_NET_UA_DESCR_VALUE;
		spider_net_write_reg(card, SPIDER_NET_GMRUAFILnR, macu);
		spider_net_write_reg(card, SPIDER_NET_GMRUAFILnR + 0x04, macl);
		spider_net_write_reg(card, SPIDER_NET_GMRUA0FIL15R,
				     SPIDER_NET_NONPROMISC_VALUE);
	}
}

static int
spider_net_get_mac_address(struct net_device *netdev)
{
	struct spider_net_card *card = netdev_priv(netdev);
	u32 macl, macu;

	macl = spider_net_read_reg(card, SPIDER_NET_GMACUNIMACL);
	macu = spider_net_read_reg(card, SPIDER_NET_GMACUNIMACU);

	netdev->dev_addr[0] = (macu >> 24) & 0xff;
	netdev->dev_addr[1] = (macu >> 16) & 0xff;
	netdev->dev_addr[2] = (macu >> 8) & 0xff;
	netdev->dev_addr[3] = macu & 0xff;
	netdev->dev_addr[4] = (macl >> 8) & 0xff;
	netdev->dev_addr[5] = macl & 0xff;

	if (!is_valid_ether_addr(&netdev->dev_addr[0]))
		return -EINVAL;

	return 0;
}

static inline int
spider_net_get_descr_status(struct spider_net_hw_descr *hwdescr)
{
	return hwdescr->dmac_cmd_status & SPIDER_NET_DESCR_IND_PROC_MASK;
}

static void
spider_net_free_chain(struct spider_net_card *card,
		      struct spider_net_descr_chain *chain)
{
	struct spider_net_descr *descr;

	descr = chain->ring;
	do {
		descr->bus_addr = 0;
		descr->hwdescr->next_descr_addr = 0;
		descr = descr->next;
	} while (descr != chain->ring);

	dma_free_coherent(&card->pdev->dev, chain->num_desc,
	    chain->hwring, chain->dma_addr);
}

static int
spider_net_init_chain(struct spider_net_card *card,
		       struct spider_net_descr_chain *chain)
{
	int i;
	struct spider_net_descr *descr;
	struct spider_net_hw_descr *hwdescr;
	dma_addr_t buf;
	size_t alloc_size;

	alloc_size = chain->num_desc * sizeof(struct spider_net_hw_descr);

	chain->hwring = dma_alloc_coherent(&card->pdev->dev, alloc_size,
		&chain->dma_addr, GFP_KERNEL);

	if (!chain->hwring)
		return -ENOMEM;

	memset(chain->ring, 0, chain->num_desc * sizeof(struct spider_net_descr));

	
	descr = chain->ring;
	hwdescr = chain->hwring;
	buf = chain->dma_addr;
	for (i=0; i < chain->num_desc; i++, descr++, hwdescr++) {
		hwdescr->dmac_cmd_status = SPIDER_NET_DESCR_NOT_IN_USE;
		hwdescr->next_descr_addr = 0;

		descr->hwdescr = hwdescr;
		descr->bus_addr = buf;
		descr->next = descr + 1;
		descr->prev = descr - 1;

		buf += sizeof(struct spider_net_hw_descr);
	}
	
	(descr-1)->next = chain->ring;
	chain->ring->prev = descr-1;

	spin_lock_init(&chain->lock);
	chain->head = chain->ring;
	chain->tail = chain->ring;
	return 0;
}

static void
spider_net_free_rx_chain_contents(struct spider_net_card *card)
{
	struct spider_net_descr *descr;

	descr = card->rx_chain.head;
	do {
		if (descr->skb) {
			pci_unmap_single(card->pdev, descr->hwdescr->buf_addr,
					 SPIDER_NET_MAX_FRAME,
					 PCI_DMA_BIDIRECTIONAL);
			dev_kfree_skb(descr->skb);
			descr->skb = NULL;
		}
		descr = descr->next;
	} while (descr != card->rx_chain.head);
}

static int
spider_net_prepare_rx_descr(struct spider_net_card *card,
			    struct spider_net_descr *descr)
{
	struct spider_net_hw_descr *hwdescr = descr->hwdescr;
	dma_addr_t buf;
	int offset;
	int bufsize;

	
	bufsize = (SPIDER_NET_MAX_FRAME + SPIDER_NET_RXBUF_ALIGN - 1) &
		(~(SPIDER_NET_RXBUF_ALIGN - 1));

	
	descr->skb = netdev_alloc_skb(card->netdev,
				      bufsize + SPIDER_NET_RXBUF_ALIGN - 1);
	if (!descr->skb) {
		if (netif_msg_rx_err(card) && net_ratelimit())
			dev_err(&card->netdev->dev,
			        "Not enough memory to allocate rx buffer\n");
		card->spider_stats.alloc_rx_skb_error++;
		return -ENOMEM;
	}
	hwdescr->buf_size = bufsize;
	hwdescr->result_size = 0;
	hwdescr->valid_size = 0;
	hwdescr->data_status = 0;
	hwdescr->data_error = 0;

	offset = ((unsigned long)descr->skb->data) &
		(SPIDER_NET_RXBUF_ALIGN - 1);
	if (offset)
		skb_reserve(descr->skb, SPIDER_NET_RXBUF_ALIGN - offset);
	
	buf = pci_map_single(card->pdev, descr->skb->data,
			SPIDER_NET_MAX_FRAME, PCI_DMA_FROMDEVICE);
	if (pci_dma_mapping_error(card->pdev, buf)) {
		dev_kfree_skb_any(descr->skb);
		descr->skb = NULL;
		if (netif_msg_rx_err(card) && net_ratelimit())
			dev_err(&card->netdev->dev, "Could not iommu-map rx buffer\n");
		card->spider_stats.rx_iommu_map_error++;
		hwdescr->dmac_cmd_status = SPIDER_NET_DESCR_NOT_IN_USE;
	} else {
		hwdescr->buf_addr = buf;
		wmb();
		hwdescr->dmac_cmd_status = SPIDER_NET_DESCR_CARDOWNED |
					 SPIDER_NET_DMAC_NOINTR_COMPLETE;
	}

	return 0;
}

static inline void
spider_net_enable_rxchtails(struct spider_net_card *card)
{
	
	spider_net_write_reg(card, SPIDER_NET_GDADCHA ,
			     card->rx_chain.tail->bus_addr);
}

static inline void
spider_net_enable_rxdmac(struct spider_net_card *card)
{
	wmb();
	spider_net_write_reg(card, SPIDER_NET_GDADMACCNTR,
			     SPIDER_NET_DMA_RX_VALUE);
}

static inline void
spider_net_disable_rxdmac(struct spider_net_card *card)
{
	spider_net_write_reg(card, SPIDER_NET_GDADMACCNTR,
			     SPIDER_NET_DMA_RX_FEND_VALUE);
}

static void
spider_net_refill_rx_chain(struct spider_net_card *card)
{
	struct spider_net_descr_chain *chain = &card->rx_chain;
	unsigned long flags;

	if (!spin_trylock_irqsave(&chain->lock, flags))
		return;

	while (spider_net_get_descr_status(chain->head->hwdescr) ==
			SPIDER_NET_DESCR_NOT_IN_USE) {
		if (spider_net_prepare_rx_descr(card, chain->head))
			break;
		chain->head = chain->head->next;
	}

	spin_unlock_irqrestore(&chain->lock, flags);
}

static int
spider_net_alloc_rx_skbs(struct spider_net_card *card)
{
	struct spider_net_descr_chain *chain = &card->rx_chain;
	struct spider_net_descr *start = chain->tail;
	struct spider_net_descr *descr = start;

	
	do {
		descr->prev->hwdescr->next_descr_addr = descr->bus_addr;
		descr = descr->next;
	} while (descr != start);

	if (spider_net_prepare_rx_descr(card, chain->head))
		goto error;
	else
		chain->head = chain->head->next;

	spider_net_refill_rx_chain(card);
	spider_net_enable_rxdmac(card);
	return 0;

error:
	spider_net_free_rx_chain_contents(card);
	return -ENOMEM;
}

static u8
spider_net_get_multicast_hash(struct net_device *netdev, __u8 *addr)
{
	u32 crc;
	u8 hash;
	char addr_for_crc[ETH_ALEN] = { 0, };
	int i, bit;

	for (i = 0; i < ETH_ALEN * 8; i++) {
		bit = (addr[i / 8] >> (i % 8)) & 1;
		addr_for_crc[ETH_ALEN - 1 - i / 8] += bit << (7 - (i % 8));
	}

	crc = crc32_be(~0, addr_for_crc, netdev->addr_len);

	hash = (crc >> 27);
	hash <<= 3;
	hash |= crc & 7;
	hash &= 0xff;

	return hash;
}

static void
spider_net_set_multi(struct net_device *netdev)
{
	struct netdev_hw_addr *ha;
	u8 hash;
	int i;
	u32 reg;
	struct spider_net_card *card = netdev_priv(netdev);
	unsigned long bitmask[SPIDER_NET_MULTICAST_HASHES / BITS_PER_LONG] =
		{0, };

	spider_net_set_promisc(card);

	if (netdev->flags & IFF_ALLMULTI) {
		for (i = 0; i < SPIDER_NET_MULTICAST_HASHES; i++) {
			set_bit(i, bitmask);
		}
		goto write_hash;
	}

	set_bit(0xfd, bitmask);

	netdev_for_each_mc_addr(ha, netdev) {
		hash = spider_net_get_multicast_hash(netdev, ha->addr);
		set_bit(hash, bitmask);
	}

write_hash:
	for (i = 0; i < SPIDER_NET_MULTICAST_HASHES / 4; i++) {
		reg = 0;
		if (test_bit(i * 4, bitmask))
			reg += 0x08;
		reg <<= 8;
		if (test_bit(i * 4 + 1, bitmask))
			reg += 0x08;
		reg <<= 8;
		if (test_bit(i * 4 + 2, bitmask))
			reg += 0x08;
		reg <<= 8;
		if (test_bit(i * 4 + 3, bitmask))
			reg += 0x08;

		spider_net_write_reg(card, SPIDER_NET_GMRMHFILnR + i * 4, reg);
	}
}

static int
spider_net_prepare_tx_descr(struct spider_net_card *card,
			    struct sk_buff *skb)
{
	struct spider_net_descr_chain *chain = &card->tx_chain;
	struct spider_net_descr *descr;
	struct spider_net_hw_descr *hwdescr;
	dma_addr_t buf;
	unsigned long flags;

	buf = pci_map_single(card->pdev, skb->data, skb->len, PCI_DMA_TODEVICE);
	if (pci_dma_mapping_error(card->pdev, buf)) {
		if (netif_msg_tx_err(card) && net_ratelimit())
			dev_err(&card->netdev->dev, "could not iommu-map packet (%p, %i). "
				  "Dropping packet\n", skb->data, skb->len);
		card->spider_stats.tx_iommu_map_error++;
		return -ENOMEM;
	}

	spin_lock_irqsave(&chain->lock, flags);
	descr = card->tx_chain.head;
	if (descr->next == chain->tail->prev) {
		spin_unlock_irqrestore(&chain->lock, flags);
		pci_unmap_single(card->pdev, buf, skb->len, PCI_DMA_TODEVICE);
		return -ENOMEM;
	}
	hwdescr = descr->hwdescr;
	chain->head = descr->next;

	descr->skb = skb;
	hwdescr->buf_addr = buf;
	hwdescr->buf_size = skb->len;
	hwdescr->next_descr_addr = 0;
	hwdescr->data_status = 0;

	hwdescr->dmac_cmd_status =
			SPIDER_NET_DESCR_CARDOWNED | SPIDER_NET_DMAC_TXFRMTL;
	spin_unlock_irqrestore(&chain->lock, flags);

	if (skb->ip_summed == CHECKSUM_PARTIAL)
		switch (ip_hdr(skb)->protocol) {
		case IPPROTO_TCP:
			hwdescr->dmac_cmd_status |= SPIDER_NET_DMAC_TCP;
			break;
		case IPPROTO_UDP:
			hwdescr->dmac_cmd_status |= SPIDER_NET_DMAC_UDP;
			break;
		}

	
	wmb();
	descr->prev->hwdescr->next_descr_addr = descr->bus_addr;

	card->netdev->trans_start = jiffies; 
	return 0;
}

static int
spider_net_set_low_watermark(struct spider_net_card *card)
{
	struct spider_net_descr *descr = card->tx_chain.tail;
	struct spider_net_hw_descr *hwdescr;
	unsigned long flags;
	int status;
	int cnt=0;
	int i;

	while (descr != card->tx_chain.head) {
		status = descr->hwdescr->dmac_cmd_status & SPIDER_NET_DESCR_NOT_IN_USE;
		if (status == SPIDER_NET_DESCR_NOT_IN_USE)
			break;
		descr = descr->next;
		cnt++;
	}

	
	if (cnt < card->tx_chain.num_desc/4)
		return cnt;

	
	descr = card->tx_chain.tail;
	cnt = (cnt*3)/4;
	for (i=0;i<cnt; i++)
		descr = descr->next;

	
	spin_lock_irqsave(&card->tx_chain.lock, flags);
	descr->hwdescr->dmac_cmd_status |= SPIDER_NET_DESCR_TXDESFLG;
	if (card->low_watermark && card->low_watermark != descr) {
		hwdescr = card->low_watermark->hwdescr;
		hwdescr->dmac_cmd_status =
		     hwdescr->dmac_cmd_status & ~SPIDER_NET_DESCR_TXDESFLG;
	}
	card->low_watermark = descr;
	spin_unlock_irqrestore(&card->tx_chain.lock, flags);
	return cnt;
}

static int
spider_net_release_tx_chain(struct spider_net_card *card, int brutal)
{
	struct net_device *dev = card->netdev;
	struct spider_net_descr_chain *chain = &card->tx_chain;
	struct spider_net_descr *descr;
	struct spider_net_hw_descr *hwdescr;
	struct sk_buff *skb;
	u32 buf_addr;
	unsigned long flags;
	int status;

	while (1) {
		spin_lock_irqsave(&chain->lock, flags);
		if (chain->tail == chain->head) {
			spin_unlock_irqrestore(&chain->lock, flags);
			return 0;
		}
		descr = chain->tail;
		hwdescr = descr->hwdescr;

		status = spider_net_get_descr_status(hwdescr);
		switch (status) {
		case SPIDER_NET_DESCR_COMPLETE:
			dev->stats.tx_packets++;
			dev->stats.tx_bytes += descr->skb->len;
			break;

		case SPIDER_NET_DESCR_CARDOWNED:
			if (!brutal) {
				spin_unlock_irqrestore(&chain->lock, flags);
				return 1;
			}


		case SPIDER_NET_DESCR_RESPONSE_ERROR:
		case SPIDER_NET_DESCR_PROTECTION_ERROR:
		case SPIDER_NET_DESCR_FORCE_END:
			if (netif_msg_tx_err(card))
				dev_err(&card->netdev->dev, "forcing end of tx descriptor "
				       "with status x%02x\n", status);
			dev->stats.tx_errors++;
			break;

		default:
			dev->stats.tx_dropped++;
			if (!brutal) {
				spin_unlock_irqrestore(&chain->lock, flags);
				return 1;
			}
		}

		chain->tail = descr->next;
		hwdescr->dmac_cmd_status |= SPIDER_NET_DESCR_NOT_IN_USE;
		skb = descr->skb;
		descr->skb = NULL;
		buf_addr = hwdescr->buf_addr;
		spin_unlock_irqrestore(&chain->lock, flags);

		
		if (skb) {
			pci_unmap_single(card->pdev, buf_addr, skb->len,
					PCI_DMA_TODEVICE);
			dev_kfree_skb(skb);
		}
	}
	return 0;
}

static inline void
spider_net_kick_tx_dma(struct spider_net_card *card)
{
	struct spider_net_descr *descr;

	if (spider_net_read_reg(card, SPIDER_NET_GDTDMACCNTR) &
			SPIDER_NET_TX_DMA_EN)
		goto out;

	descr = card->tx_chain.tail;
	for (;;) {
		if (spider_net_get_descr_status(descr->hwdescr) ==
				SPIDER_NET_DESCR_CARDOWNED) {
			spider_net_write_reg(card, SPIDER_NET_GDTDCHA,
					descr->bus_addr);
			spider_net_write_reg(card, SPIDER_NET_GDTDMACCNTR,
					SPIDER_NET_DMA_TX_VALUE);
			break;
		}
		if (descr == card->tx_chain.head)
			break;
		descr = descr->next;
	}

out:
	mod_timer(&card->tx_timer, jiffies + SPIDER_NET_TX_TIMER);
}

static int
spider_net_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	int cnt;
	struct spider_net_card *card = netdev_priv(netdev);

	spider_net_release_tx_chain(card, 0);

	if (spider_net_prepare_tx_descr(card, skb) != 0) {
		netdev->stats.tx_dropped++;
		netif_stop_queue(netdev);
		return NETDEV_TX_BUSY;
	}

	cnt = spider_net_set_low_watermark(card);
	if (cnt < 5)
		spider_net_kick_tx_dma(card);
	return NETDEV_TX_OK;
}

static void
spider_net_cleanup_tx_ring(struct spider_net_card *card)
{
	if ((spider_net_release_tx_chain(card, 0) != 0) &&
	    (card->netdev->flags & IFF_UP)) {
		spider_net_kick_tx_dma(card);
		netif_wake_queue(card->netdev);
	}
}

static int
spider_net_do_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	switch (cmd) {
	default:
		return -EOPNOTSUPP;
	}
}

static void
spider_net_pass_skb_up(struct spider_net_descr *descr,
		       struct spider_net_card *card)
{
	struct spider_net_hw_descr *hwdescr = descr->hwdescr;
	struct sk_buff *skb = descr->skb;
	struct net_device *netdev = card->netdev;
	u32 data_status = hwdescr->data_status;
	u32 data_error = hwdescr->data_error;

	skb_put(skb, hwdescr->valid_size);

#define SPIDER_MISALIGN		2
	skb_pull(skb, SPIDER_MISALIGN);
	skb->protocol = eth_type_trans(skb, netdev);

	
	skb_checksum_none_assert(skb);
	if (netdev->features & NETIF_F_RXCSUM) {
		if ( ( (data_status & SPIDER_NET_DATA_STATUS_CKSUM_MASK) ==
		       SPIDER_NET_DATA_STATUS_CKSUM_MASK) &&
		     !(data_error & SPIDER_NET_DATA_ERR_CKSUM_MASK))
			skb->ip_summed = CHECKSUM_UNNECESSARY;
	}

	if (data_status & SPIDER_NET_VLAN_PACKET) {
		
	}

	
	netdev->stats.rx_packets++;
	netdev->stats.rx_bytes += skb->len;

	
	netif_receive_skb(skb);
}

static void show_rx_chain(struct spider_net_card *card)
{
	struct spider_net_descr_chain *chain = &card->rx_chain;
	struct spider_net_descr *start= chain->tail;
	struct spider_net_descr *descr= start;
	struct spider_net_hw_descr *hwd = start->hwdescr;
	struct device *dev = &card->netdev->dev;
	u32 curr_desc, next_desc;
	int status;

	int tot = 0;
	int cnt = 0;
	int off = start - chain->ring;
	int cstat = hwd->dmac_cmd_status;

	dev_info(dev, "Total number of descrs=%d\n",
		chain->num_desc);
	dev_info(dev, "Chain tail located at descr=%d, status=0x%x\n",
		off, cstat);

	curr_desc = spider_net_read_reg(card, SPIDER_NET_GDACTDPA);
	next_desc = spider_net_read_reg(card, SPIDER_NET_GDACNEXTDA);

	status = cstat;
	do
	{
		hwd = descr->hwdescr;
		off = descr - chain->ring;
		status = hwd->dmac_cmd_status;

		if (descr == chain->head)
			dev_info(dev, "Chain head is at %d, head status=0x%x\n",
			         off, status);

		if (curr_desc == descr->bus_addr)
			dev_info(dev, "HW curr desc (GDACTDPA) is at %d, status=0x%x\n",
			         off, status);

		if (next_desc == descr->bus_addr)
			dev_info(dev, "HW next desc (GDACNEXTDA) is at %d, status=0x%x\n",
			         off, status);

		if (hwd->next_descr_addr == 0)
			dev_info(dev, "chain is cut at %d\n", off);

		if (cstat != status) {
			int from = (chain->num_desc + off - cnt) % chain->num_desc;
			int to = (chain->num_desc + off - 1) % chain->num_desc;
			dev_info(dev, "Have %d (from %d to %d) descrs "
			         "with stat=0x%08x\n", cnt, from, to, cstat);
			cstat = status;
			cnt = 0;
		}

		cnt ++;
		tot ++;
		descr = descr->next;
	} while (descr != start);

	dev_info(dev, "Last %d descrs with stat=0x%08x "
	         "for a total of %d descrs\n", cnt, cstat, tot);

#ifdef DEBUG
	
	descr = start;
	do
	{
		struct spider_net_hw_descr *hwd = descr->hwdescr;
		status = spider_net_get_descr_status(hwd);
		cnt = descr - chain->ring;
		dev_info(dev, "Descr %d stat=0x%08x skb=%p\n",
		         cnt, status, descr->skb);
		dev_info(dev, "bus addr=%08x buf addr=%08x sz=%d\n",
		         descr->bus_addr, hwd->buf_addr, hwd->buf_size);
		dev_info(dev, "next=%08x result sz=%d valid sz=%d\n",
		         hwd->next_descr_addr, hwd->result_size,
		         hwd->valid_size);
		dev_info(dev, "dmac=%08x data stat=%08x data err=%08x\n",
		         hwd->dmac_cmd_status, hwd->data_status,
		         hwd->data_error);
		dev_info(dev, "\n");

		descr = descr->next;
	} while (descr != start);
#endif

}

static void spider_net_resync_head_ptr(struct spider_net_card *card)
{
	unsigned long flags;
	struct spider_net_descr_chain *chain = &card->rx_chain;
	struct spider_net_descr *descr;
	int i, status;

	
	descr = chain->head;
	status = spider_net_get_descr_status(descr->hwdescr);

	if (status == SPIDER_NET_DESCR_NOT_IN_USE)
		return;

	spin_lock_irqsave(&chain->lock, flags);

	descr = chain->head;
	status = spider_net_get_descr_status(descr->hwdescr);
	for (i=0; i<chain->num_desc; i++) {
		if (status != SPIDER_NET_DESCR_CARDOWNED) break;
		descr = descr->next;
		status = spider_net_get_descr_status(descr->hwdescr);
	}
	chain->head = descr;

	spin_unlock_irqrestore(&chain->lock, flags);
}

static int spider_net_resync_tail_ptr(struct spider_net_card *card)
{
	struct spider_net_descr_chain *chain = &card->rx_chain;
	struct spider_net_descr *descr;
	int i, status;

	
	descr = chain->tail;
	status = spider_net_get_descr_status(descr->hwdescr);

	for (i=0; i<chain->num_desc; i++) {
		if ((status != SPIDER_NET_DESCR_CARDOWNED) &&
		    (status != SPIDER_NET_DESCR_NOT_IN_USE)) break;
		descr = descr->next;
		status = spider_net_get_descr_status(descr->hwdescr);
	}
	chain->tail = descr;

	if ((i == chain->num_desc) || (i == 0))
		return 1;
	return 0;
}

static int
spider_net_decode_one_descr(struct spider_net_card *card)
{
	struct net_device *dev = card->netdev;
	struct spider_net_descr_chain *chain = &card->rx_chain;
	struct spider_net_descr *descr = chain->tail;
	struct spider_net_hw_descr *hwdescr = descr->hwdescr;
	u32 hw_buf_addr;
	int status;

	status = spider_net_get_descr_status(hwdescr);

	
	if ((status == SPIDER_NET_DESCR_CARDOWNED) ||
	    (status == SPIDER_NET_DESCR_NOT_IN_USE))
		return 0;

	
	chain->tail = descr->next;

	
	hw_buf_addr = hwdescr->buf_addr;
	hwdescr->buf_addr = 0xffffffff;
	pci_unmap_single(card->pdev, hw_buf_addr,
			SPIDER_NET_MAX_FRAME, PCI_DMA_FROMDEVICE);

	if ( (status == SPIDER_NET_DESCR_RESPONSE_ERROR) ||
	     (status == SPIDER_NET_DESCR_PROTECTION_ERROR) ||
	     (status == SPIDER_NET_DESCR_FORCE_END) ) {
		if (netif_msg_rx_err(card))
			dev_err(&dev->dev,
			       "dropping RX descriptor with state %d\n", status);
		dev->stats.rx_dropped++;
		goto bad_desc;
	}

	if ( (status != SPIDER_NET_DESCR_COMPLETE) &&
	     (status != SPIDER_NET_DESCR_FRAME_END) ) {
		if (netif_msg_rx_err(card))
			dev_err(&card->netdev->dev,
			       "RX descriptor with unknown state %d\n", status);
		card->spider_stats.rx_desc_unk_state++;
		goto bad_desc;
	}

	
	if (hwdescr->data_error & SPIDER_NET_DESTROY_RX_FLAGS) {
		if (netif_msg_rx_err(card))
			dev_err(&card->netdev->dev,
			       "error in received descriptor found, "
			       "data_status=x%08x, data_error=x%08x\n",
			       hwdescr->data_status, hwdescr->data_error);
		goto bad_desc;
	}

	if (hwdescr->dmac_cmd_status & SPIDER_NET_DESCR_BAD_STATUS) {
		dev_err(&card->netdev->dev, "bad status, cmd_status=x%08x\n",
			       hwdescr->dmac_cmd_status);
		pr_err("buf_addr=x%08x\n", hw_buf_addr);
		pr_err("buf_size=x%08x\n", hwdescr->buf_size);
		pr_err("next_descr_addr=x%08x\n", hwdescr->next_descr_addr);
		pr_err("result_size=x%08x\n", hwdescr->result_size);
		pr_err("valid_size=x%08x\n", hwdescr->valid_size);
		pr_err("data_status=x%08x\n", hwdescr->data_status);
		pr_err("data_error=x%08x\n", hwdescr->data_error);
		pr_err("which=%ld\n", descr - card->rx_chain.ring);

		card->spider_stats.rx_desc_error++;
		goto bad_desc;
	}

	
	spider_net_pass_skb_up(descr, card);
	descr->skb = NULL;
	hwdescr->dmac_cmd_status = SPIDER_NET_DESCR_NOT_IN_USE;
	return 1;

bad_desc:
	if (netif_msg_rx_err(card))
		show_rx_chain(card);
	dev_kfree_skb_irq(descr->skb);
	descr->skb = NULL;
	hwdescr->dmac_cmd_status = SPIDER_NET_DESCR_NOT_IN_USE;
	return 0;
}

static int spider_net_poll(struct napi_struct *napi, int budget)
{
	struct spider_net_card *card = container_of(napi, struct spider_net_card, napi);
	int packets_done = 0;

	while (packets_done < budget) {
		if (!spider_net_decode_one_descr(card))
			break;

		packets_done++;
	}

	if ((packets_done == 0) && (card->num_rx_ints != 0)) {
		if (!spider_net_resync_tail_ptr(card))
			packets_done = budget;
		spider_net_resync_head_ptr(card);
	}
	card->num_rx_ints = 0;

	spider_net_refill_rx_chain(card);
	spider_net_enable_rxdmac(card);

	spider_net_cleanup_tx_ring(card);

	
	
	if (packets_done < budget) {
		napi_complete(napi);
		spider_net_rx_irq_on(card);
		card->ignore_rx_ramfull = 0;
	}

	return packets_done;
}

static int
spider_net_change_mtu(struct net_device *netdev, int new_mtu)
{
	if ( (new_mtu < SPIDER_NET_MIN_MTU ) ||
		(new_mtu > SPIDER_NET_MAX_MTU) )
		return -EINVAL;
	netdev->mtu = new_mtu;
	return 0;
}

static int
spider_net_set_mac(struct net_device *netdev, void *p)
{
	struct spider_net_card *card = netdev_priv(netdev);
	u32 macl, macu, regvalue;
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	
	regvalue = spider_net_read_reg(card, SPIDER_NET_GMACOPEMD);
	regvalue &= ~((1 << 5) | (1 << 6));
	spider_net_write_reg(card, SPIDER_NET_GMACOPEMD, regvalue);

	
	macu = (addr->sa_data[0]<<24) + (addr->sa_data[1]<<16) +
		(addr->sa_data[2]<<8) + (addr->sa_data[3]);
	macl = (addr->sa_data[4]<<8) + (addr->sa_data[5]);
	spider_net_write_reg(card, SPIDER_NET_GMACUNIMACU, macu);
	spider_net_write_reg(card, SPIDER_NET_GMACUNIMACL, macl);

	
	regvalue = spider_net_read_reg(card, SPIDER_NET_GMACOPEMD);
	regvalue |= ((1 << 5) | (1 << 6));
	spider_net_write_reg(card, SPIDER_NET_GMACOPEMD, regvalue);

	spider_net_set_promisc(card);

	
	if (spider_net_get_mac_address(netdev))
		return -EADDRNOTAVAIL;
	if (memcmp(netdev->dev_addr,addr->sa_data,netdev->addr_len))
		return -EADDRNOTAVAIL;

	return 0;
}

static void
spider_net_link_reset(struct net_device *netdev)
{

	struct spider_net_card *card = netdev_priv(netdev);

	del_timer_sync(&card->aneg_timer);

	
	spider_net_write_reg(card, SPIDER_NET_GMACST,
			     spider_net_read_reg(card, SPIDER_NET_GMACST));
	spider_net_write_reg(card, SPIDER_NET_GMACINTEN, 0);

	
	card->aneg_count = 0;
	card->medium = BCM54XX_COPPER;
	spider_net_setup_aneg(card);
	mod_timer(&card->aneg_timer, jiffies + SPIDER_NET_ANEG_TIMER);

}

static void
spider_net_handle_error_irq(struct spider_net_card *card, u32 status_reg,
			    u32 error_reg1, u32 error_reg2)
{
	u32 i;
	int show_error = 1;

	
	if (status_reg)
		for (i = 0; i < 32; i++)
			if (status_reg & (1<<i))
				switch (i)
	{

	case SPIDER_NET_GIPSINT:
		show_error = 0;
		break;

	case SPIDER_NET_GPWOPCMPINT:
		
		show_error = 0;
		break;
	case SPIDER_NET_GPROPCMPINT:
		
		show_error = 0;
		break;
	case SPIDER_NET_GPWFFINT:
		
		if (netif_msg_intr(card))
			dev_err(&card->netdev->dev, "PHY write queue full\n");
		show_error = 0;
		break;

	
	
	

	case SPIDER_NET_GDTDEN0INT:
		
		show_error = 0;
		break;

	case SPIDER_NET_GDDDEN0INT: 
	case SPIDER_NET_GDCDEN0INT: 
	case SPIDER_NET_GDBDEN0INT: 
	case SPIDER_NET_GDADEN0INT:
		
		show_error = 0;
		break;

	
	case SPIDER_NET_GDDFDCINT:
	case SPIDER_NET_GDCFDCINT:
	case SPIDER_NET_GDBFDCINT:
	case SPIDER_NET_GDAFDCINT:
	
	
	
	
	
		show_error = 0;
		break;

	
	case SPIDER_NET_GDTFDCINT:
		show_error = 0;
		break;
	case SPIDER_NET_GTTEDINT:
		show_error = 0;
		break;
	case SPIDER_NET_GDTDCEINT:
		show_error = 0;
		break;

	
	
	}

	
	if (error_reg1)
		for (i = 0; i < 32; i++)
			if (error_reg1 & (1<<i))
				switch (i)
	{
	case SPIDER_NET_GTMFLLINT:
		show_error = 0;
		break;
	case SPIDER_NET_GRFDFLLINT: 
	case SPIDER_NET_GRFCFLLINT: 
	case SPIDER_NET_GRFBFLLINT: 
	case SPIDER_NET_GRFAFLLINT: 
	case SPIDER_NET_GRMFLLINT:
		
		if (card->ignore_rx_ramfull == 0) {
			card->ignore_rx_ramfull = 1;
			spider_net_resync_head_ptr(card);
			spider_net_refill_rx_chain(card);
			spider_net_enable_rxdmac(card);
			card->num_rx_ints ++;
			napi_schedule(&card->napi);
		}
		show_error = 0;
		break;

	
	case SPIDER_NET_GDTINVDINT:
		
		show_error = 0;
		break;

	
	case SPIDER_NET_GDDDCEINT: 
	case SPIDER_NET_GDCDCEINT: 
	case SPIDER_NET_GDBDCEINT: 
	case SPIDER_NET_GDADCEINT:
		spider_net_resync_head_ptr(card);
		spider_net_refill_rx_chain(card);
		spider_net_enable_rxdmac(card);
		card->num_rx_ints ++;
		napi_schedule(&card->napi);
		show_error = 0;
		break;

	
	case SPIDER_NET_GDDINVDINT: 
	case SPIDER_NET_GDCINVDINT: 
	case SPIDER_NET_GDBINVDINT: 
	case SPIDER_NET_GDAINVDINT:
		
		spider_net_resync_head_ptr(card);
		spider_net_refill_rx_chain(card);
		spider_net_enable_rxdmac(card);
		card->num_rx_ints ++;
		napi_schedule(&card->napi);
		show_error = 0;
		break;

	
	
	
	
	
	
	
	
	
	
	
	default:
		show_error = 1;
		break;
	}

	
	if (error_reg2)
		for (i = 0; i < 32; i++)
			if (error_reg2 & (1<<i))
				switch (i)
	{
		default:
			break;
	}

	if ((show_error) && (netif_msg_intr(card)) && net_ratelimit())
		dev_err(&card->netdev->dev, "Error interrupt, GHIINT0STS = 0x%08x, "
		       "GHIINT1STS = 0x%08x, GHIINT2STS = 0x%08x\n",
		       status_reg, error_reg1, error_reg2);

	
	spider_net_write_reg(card, SPIDER_NET_GHIINT1STS, error_reg1);
	spider_net_write_reg(card, SPIDER_NET_GHIINT2STS, error_reg2);
}

static irqreturn_t
spider_net_interrupt(int irq, void *ptr)
{
	struct net_device *netdev = ptr;
	struct spider_net_card *card = netdev_priv(netdev);
	u32 status_reg, error_reg1, error_reg2;

	status_reg = spider_net_read_reg(card, SPIDER_NET_GHIINT0STS);
	error_reg1 = spider_net_read_reg(card, SPIDER_NET_GHIINT1STS);
	error_reg2 = spider_net_read_reg(card, SPIDER_NET_GHIINT2STS);

	if (!(status_reg & SPIDER_NET_INT0_MASK_VALUE) &&
	    !(error_reg1 & SPIDER_NET_INT1_MASK_VALUE) &&
	    !(error_reg2 & SPIDER_NET_INT2_MASK_VALUE))
		return IRQ_NONE;

	if (status_reg & SPIDER_NET_RXINT ) {
		spider_net_rx_irq_off(card);
		napi_schedule(&card->napi);
		card->num_rx_ints ++;
	}
	if (status_reg & SPIDER_NET_TXINT)
		napi_schedule(&card->napi);

	if (status_reg & SPIDER_NET_LINKINT)
		spider_net_link_reset(netdev);

	if (status_reg & SPIDER_NET_ERRINT )
		spider_net_handle_error_irq(card, status_reg,
					    error_reg1, error_reg2);

	
	spider_net_write_reg(card, SPIDER_NET_GHIINT0STS, status_reg);

	return IRQ_HANDLED;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void
spider_net_poll_controller(struct net_device *netdev)
{
	disable_irq(netdev->irq);
	spider_net_interrupt(netdev->irq, netdev);
	enable_irq(netdev->irq);
}
#endif 

static void
spider_net_enable_interrupts(struct spider_net_card *card)
{
	spider_net_write_reg(card, SPIDER_NET_GHIINT0MSK,
			     SPIDER_NET_INT0_MASK_VALUE);
	spider_net_write_reg(card, SPIDER_NET_GHIINT1MSK,
			     SPIDER_NET_INT1_MASK_VALUE);
	spider_net_write_reg(card, SPIDER_NET_GHIINT2MSK,
			     SPIDER_NET_INT2_MASK_VALUE);
}

static void
spider_net_disable_interrupts(struct spider_net_card *card)
{
	spider_net_write_reg(card, SPIDER_NET_GHIINT0MSK, 0);
	spider_net_write_reg(card, SPIDER_NET_GHIINT1MSK, 0);
	spider_net_write_reg(card, SPIDER_NET_GHIINT2MSK, 0);
	spider_net_write_reg(card, SPIDER_NET_GMACINTEN, 0);
}

static void
spider_net_init_card(struct spider_net_card *card)
{
	spider_net_write_reg(card, SPIDER_NET_CKRCTRL,
			     SPIDER_NET_CKRCTRL_STOP_VALUE);

	spider_net_write_reg(card, SPIDER_NET_CKRCTRL,
			     SPIDER_NET_CKRCTRL_RUN_VALUE);

	
	spider_net_write_reg(card, SPIDER_NET_GMACOPEMD,
		spider_net_read_reg(card, SPIDER_NET_GMACOPEMD) | 0x4);

	spider_net_disable_interrupts(card);
}

static void
spider_net_enable_card(struct spider_net_card *card)
{
	int i;
	u32 regs[][2] = {
		{ SPIDER_NET_GRESUMINTNUM, 0 },
		{ SPIDER_NET_GREINTNUM, 0 },

		
		
		{ SPIDER_NET_GFAFRMNUM, SPIDER_NET_GFXFRAMES_VALUE },
		{ SPIDER_NET_GFBFRMNUM, SPIDER_NET_GFXFRAMES_VALUE },
		{ SPIDER_NET_GFCFRMNUM, SPIDER_NET_GFXFRAMES_VALUE },
		{ SPIDER_NET_GFDFRMNUM, SPIDER_NET_GFXFRAMES_VALUE },
		
		{ SPIDER_NET_GFFRMNUM, SPIDER_NET_FRAMENUM_VALUE },

		
		{ SPIDER_NET_GFREECNNUM, 0 },
		{ SPIDER_NET_GONETIMENUM, 0 },
		{ SPIDER_NET_GTOUTFRMNUM, 0 },

		
		{ SPIDER_NET_GRXMDSET, SPIDER_NET_RXMODE_VALUE },
		
		{ SPIDER_NET_GTXMDSET, SPIDER_NET_TXMODE_VALUE },
		
		{ SPIDER_NET_GIPSECINIT, SPIDER_NET_IPSECINIT_VALUE },

		{ SPIDER_NET_GFTRESTRT, SPIDER_NET_RESTART_VALUE },

		{ SPIDER_NET_GMRWOLCTRL, 0 },
		{ SPIDER_NET_GTESTMD, 0x10000000 },
		{ SPIDER_NET_GTTQMSK, 0x00400040 },

		{ SPIDER_NET_GMACINTEN, 0 },

		
		{ SPIDER_NET_GMACAPAUSE, SPIDER_NET_MACAPAUSE_VALUE },
		{ SPIDER_NET_GMACTXPAUSE, SPIDER_NET_TXPAUSE_VALUE },

		{ SPIDER_NET_GMACBSTLMT, SPIDER_NET_BURSTLMT_VALUE },
		{ 0, 0}
	};

	i = 0;
	while (regs[i][0]) {
		spider_net_write_reg(card, regs[i][0], regs[i][1]);
		i++;
	}

	
	for (i = 1; i <= 14; i++) {
		spider_net_write_reg(card,
				     SPIDER_NET_GMRUAFILnR + i * 8,
				     0x00080000);
		spider_net_write_reg(card,
				     SPIDER_NET_GMRUAFILnR + i * 8 + 4,
				     0x00000000);
	}

	spider_net_write_reg(card, SPIDER_NET_GMRUA0FIL15R, 0x08080000);

	spider_net_write_reg(card, SPIDER_NET_ECMODE, SPIDER_NET_ECMODE_VALUE);

	spider_net_enable_rxchtails(card);
	spider_net_enable_rxdmac(card);

	spider_net_write_reg(card, SPIDER_NET_GRXDMAEN, SPIDER_NET_WOL_VALUE);

	spider_net_write_reg(card, SPIDER_NET_GMACLENLMT,
			     SPIDER_NET_LENLMT_VALUE);
	spider_net_write_reg(card, SPIDER_NET_GMACOPEMD,
			     SPIDER_NET_OPMODE_VALUE);

	spider_net_write_reg(card, SPIDER_NET_GDTDMACCNTR,
			     SPIDER_NET_GDTBSTA);
}

static int
spider_net_download_firmware(struct spider_net_card *card,
			     const void *firmware_ptr)
{
	int sequencer, i;
	const u32 *fw_ptr = firmware_ptr;

	
	spider_net_write_reg(card, SPIDER_NET_GSINIT,
			     SPIDER_NET_STOP_SEQ_VALUE);

	for (sequencer = 0; sequencer < SPIDER_NET_FIRMWARE_SEQS;
	     sequencer++) {
		spider_net_write_reg(card,
				     SPIDER_NET_GSnPRGADR + sequencer * 8, 0);
		for (i = 0; i < SPIDER_NET_FIRMWARE_SEQWORDS; i++) {
			spider_net_write_reg(card, SPIDER_NET_GSnPRGDAT +
					     sequencer * 8, *fw_ptr);
			fw_ptr++;
		}
	}

	if (spider_net_read_reg(card, SPIDER_NET_GSINIT))
		return -EIO;

	spider_net_write_reg(card, SPIDER_NET_GSINIT,
			     SPIDER_NET_RUN_SEQ_VALUE);

	return 0;
}

static int
spider_net_init_firmware(struct spider_net_card *card)
{
	struct firmware *firmware = NULL;
	struct device_node *dn;
	const u8 *fw_prop = NULL;
	int err = -ENOENT;
	int fw_size;

	if (request_firmware((const struct firmware **)&firmware,
			     SPIDER_NET_FIRMWARE_NAME, &card->pdev->dev) == 0) {
		if ( (firmware->size != SPIDER_NET_FIRMWARE_LEN) &&
		     netif_msg_probe(card) ) {
			dev_err(&card->netdev->dev,
			       "Incorrect size of spidernet firmware in " \
			       "filesystem. Looking in host firmware...\n");
			goto try_host_fw;
		}
		err = spider_net_download_firmware(card, firmware->data);

		release_firmware(firmware);
		if (err)
			goto try_host_fw;

		goto done;
	}

try_host_fw:
	dn = pci_device_to_OF_node(card->pdev);
	if (!dn)
		goto out_err;

	fw_prop = of_get_property(dn, "firmware", &fw_size);
	if (!fw_prop)
		goto out_err;

	if ( (fw_size != SPIDER_NET_FIRMWARE_LEN) &&
	     netif_msg_probe(card) ) {
		dev_err(&card->netdev->dev,
		       "Incorrect size of spidernet firmware in host firmware\n");
		goto done;
	}

	err = spider_net_download_firmware(card, fw_prop);

done:
	return err;
out_err:
	if (netif_msg_probe(card))
		dev_err(&card->netdev->dev,
		       "Couldn't find spidernet firmware in filesystem " \
		       "or host firmware\n");
	return err;
}

int
spider_net_open(struct net_device *netdev)
{
	struct spider_net_card *card = netdev_priv(netdev);
	int result;

	result = spider_net_init_firmware(card);
	if (result)
		goto init_firmware_failed;

	
	card->aneg_count = 0;
	card->medium = BCM54XX_COPPER;
	spider_net_setup_aneg(card);
	if (card->phy.def->phy_id)
		mod_timer(&card->aneg_timer, jiffies + SPIDER_NET_ANEG_TIMER);

	result = spider_net_init_chain(card, &card->tx_chain);
	if (result)
		goto alloc_tx_failed;
	card->low_watermark = NULL;

	result = spider_net_init_chain(card, &card->rx_chain);
	if (result)
		goto alloc_rx_failed;

	
	if (spider_net_alloc_rx_skbs(card))
		goto alloc_skbs_failed;

	spider_net_set_multi(netdev);

	

	result = -EBUSY;
	if (request_irq(netdev->irq, spider_net_interrupt,
			     IRQF_SHARED, netdev->name, netdev))
		goto register_int_failed;

	spider_net_enable_card(card);

	netif_start_queue(netdev);
	netif_carrier_on(netdev);
	napi_enable(&card->napi);

	spider_net_enable_interrupts(card);

	return 0;

register_int_failed:
	spider_net_free_rx_chain_contents(card);
alloc_skbs_failed:
	spider_net_free_chain(card, &card->rx_chain);
alloc_rx_failed:
	spider_net_free_chain(card, &card->tx_chain);
alloc_tx_failed:
	del_timer_sync(&card->aneg_timer);
init_firmware_failed:
	return result;
}

static void spider_net_link_phy(unsigned long data)
{
	struct spider_net_card *card = (struct spider_net_card *)data;
	struct mii_phy *phy = &card->phy;

	
	if (card->aneg_count > SPIDER_NET_ANEG_TIMEOUT) {

		pr_debug("%s: link is down trying to bring it up\n",
			 card->netdev->name);

		switch (card->medium) {
		case BCM54XX_COPPER:
			
			if (phy->def->ops->enable_fiber)
				phy->def->ops->enable_fiber(phy, 1);
			card->medium = BCM54XX_FIBER;
			break;

		case BCM54XX_FIBER:
			
			if (phy->def->ops->enable_fiber)
				phy->def->ops->enable_fiber(phy, 0);
			card->medium = BCM54XX_UNKNOWN;
			break;

		case BCM54XX_UNKNOWN:
			spider_net_setup_aneg(card);
			card->medium = BCM54XX_COPPER;
			break;
		}

		card->aneg_count = 0;
		mod_timer(&card->aneg_timer, jiffies + SPIDER_NET_ANEG_TIMER);
		return;
	}

	
	if (!(phy->def->ops->poll_link(phy))) {
		card->aneg_count++;
		mod_timer(&card->aneg_timer, jiffies + SPIDER_NET_ANEG_TIMER);
		return;
	}

	
	phy->def->ops->read_link(phy);

	spider_net_write_reg(card, SPIDER_NET_GMACST,
			     spider_net_read_reg(card, SPIDER_NET_GMACST));
	spider_net_write_reg(card, SPIDER_NET_GMACINTEN, 0x4);

	if (phy->speed == 1000)
		spider_net_write_reg(card, SPIDER_NET_GMACMODE, 0x00000001);
	else
		spider_net_write_reg(card, SPIDER_NET_GMACMODE, 0);

	card->aneg_count = 0;

	pr_info("%s: link up, %i Mbps, %s-duplex %sautoneg.\n",
		card->netdev->name, phy->speed,
		phy->duplex == 1 ? "Full" : "Half",
		phy->autoneg == 1 ? "" : "no ");
}

static int
spider_net_setup_phy(struct spider_net_card *card)
{
	struct mii_phy *phy = &card->phy;

	spider_net_write_reg(card, SPIDER_NET_GDTDMASEL,
			     SPIDER_NET_DMASEL_VALUE);
	spider_net_write_reg(card, SPIDER_NET_GPCCTRL,
			     SPIDER_NET_PHY_CTRL_VALUE);

	phy->dev = card->netdev;
	phy->mdio_read = spider_net_read_phy;
	phy->mdio_write = spider_net_write_phy;

	for (phy->mii_id = 1; phy->mii_id <= 31; phy->mii_id++) {
		unsigned short id;
		id = spider_net_read_phy(card->netdev, phy->mii_id, MII_BMSR);
		if (id != 0x0000 && id != 0xffff) {
			if (!sungem_phy_probe(phy, phy->mii_id)) {
				pr_info("Found %s.\n", phy->def->name);
				break;
			}
		}
	}

	return 0;
}

static void
spider_net_workaround_rxramfull(struct spider_net_card *card)
{
	int i, sequencer = 0;

	
	spider_net_write_reg(card, SPIDER_NET_CKRCTRL,
			     SPIDER_NET_CKRCTRL_RUN_VALUE);

	
	for (sequencer = 0; sequencer < SPIDER_NET_FIRMWARE_SEQS;
	     sequencer++) {
		spider_net_write_reg(card, SPIDER_NET_GSnPRGADR +
				     sequencer * 8, 0x0);
		for (i = 0; i < SPIDER_NET_FIRMWARE_SEQWORDS; i++) {
			spider_net_write_reg(card, SPIDER_NET_GSnPRGDAT +
					     sequencer * 8, 0x0);
		}
	}

	
	spider_net_write_reg(card, SPIDER_NET_GSINIT, 0x000000fe);

	
	spider_net_write_reg(card, SPIDER_NET_CKRCTRL,
			     SPIDER_NET_CKRCTRL_STOP_VALUE);
}

int
spider_net_stop(struct net_device *netdev)
{
	struct spider_net_card *card = netdev_priv(netdev);

	napi_disable(&card->napi);
	netif_carrier_off(netdev);
	netif_stop_queue(netdev);
	del_timer_sync(&card->tx_timer);
	del_timer_sync(&card->aneg_timer);

	spider_net_disable_interrupts(card);

	free_irq(netdev->irq, netdev);

	spider_net_write_reg(card, SPIDER_NET_GDTDMACCNTR,
			     SPIDER_NET_DMA_TX_FEND_VALUE);

	
	spider_net_disable_rxdmac(card);

	
	spider_net_release_tx_chain(card, 1);
	spider_net_free_rx_chain_contents(card);

	spider_net_free_chain(card, &card->tx_chain);
	spider_net_free_chain(card, &card->rx_chain);

	return 0;
}

static void
spider_net_tx_timeout_task(struct work_struct *work)
{
	struct spider_net_card *card =
		container_of(work, struct spider_net_card, tx_timeout_task);
	struct net_device *netdev = card->netdev;

	if (!(netdev->flags & IFF_UP))
		goto out;

	netif_device_detach(netdev);
	spider_net_stop(netdev);

	spider_net_workaround_rxramfull(card);
	spider_net_init_card(card);

	if (spider_net_setup_phy(card))
		goto out;

	spider_net_open(netdev);
	spider_net_kick_tx_dma(card);
	netif_device_attach(netdev);

out:
	atomic_dec(&card->tx_timeout_task_counter);
}

static void
spider_net_tx_timeout(struct net_device *netdev)
{
	struct spider_net_card *card;

	card = netdev_priv(netdev);
	atomic_inc(&card->tx_timeout_task_counter);
	if (netdev->flags & IFF_UP)
		schedule_work(&card->tx_timeout_task);
	else
		atomic_dec(&card->tx_timeout_task_counter);
	card->spider_stats.tx_timeouts++;
}

static const struct net_device_ops spider_net_ops = {
	.ndo_open		= spider_net_open,
	.ndo_stop		= spider_net_stop,
	.ndo_start_xmit		= spider_net_xmit,
	.ndo_set_rx_mode	= spider_net_set_multi,
	.ndo_set_mac_address	= spider_net_set_mac,
	.ndo_change_mtu		= spider_net_change_mtu,
	.ndo_do_ioctl		= spider_net_do_ioctl,
	.ndo_tx_timeout		= spider_net_tx_timeout,
	.ndo_validate_addr	= eth_validate_addr,
	
#ifdef CONFIG_NET_POLL_CONTROLLER
	
	.ndo_poll_controller	= spider_net_poll_controller,
#endif 
};

static void
spider_net_setup_netdev_ops(struct net_device *netdev)
{
	netdev->netdev_ops = &spider_net_ops;
	netdev->watchdog_timeo = SPIDER_NET_WATCHDOG_TIMEOUT;
	
	netdev->ethtool_ops = &spider_net_ethtool_ops;
}

static int
spider_net_setup_netdev(struct spider_net_card *card)
{
	int result;
	struct net_device *netdev = card->netdev;
	struct device_node *dn;
	struct sockaddr addr;
	const u8 *mac;

	SET_NETDEV_DEV(netdev, &card->pdev->dev);

	pci_set_drvdata(card->pdev, netdev);

	init_timer(&card->tx_timer);
	card->tx_timer.function =
		(void (*)(unsigned long)) spider_net_cleanup_tx_ring;
	card->tx_timer.data = (unsigned long) card;
	netdev->irq = card->pdev->irq;

	card->aneg_count = 0;
	init_timer(&card->aneg_timer);
	card->aneg_timer.function = spider_net_link_phy;
	card->aneg_timer.data = (unsigned long) card;

	netif_napi_add(netdev, &card->napi,
		       spider_net_poll, SPIDER_NET_NAPI_WEIGHT);

	spider_net_setup_netdev_ops(netdev);

	netdev->hw_features = NETIF_F_RXCSUM | NETIF_F_IP_CSUM;
	if (SPIDER_NET_RX_CSUM_DEFAULT)
		netdev->features |= NETIF_F_RXCSUM;
	netdev->features |= NETIF_F_IP_CSUM | NETIF_F_LLTX;

	netdev->irq = card->pdev->irq;
	card->num_rx_ints = 0;
	card->ignore_rx_ramfull = 0;

	dn = pci_device_to_OF_node(card->pdev);
	if (!dn)
		return -EIO;

	mac = of_get_property(dn, "local-mac-address", NULL);
	if (!mac)
		return -EIO;
	memcpy(addr.sa_data, mac, ETH_ALEN);

	result = spider_net_set_mac(netdev, &addr);
	if ((result) && (netif_msg_probe(card)))
		dev_err(&card->netdev->dev,
		        "Failed to set MAC address: %i\n", result);

	result = register_netdev(netdev);
	if (result) {
		if (netif_msg_probe(card))
			dev_err(&card->netdev->dev,
			        "Couldn't register net_device: %i\n", result);
		return result;
	}

	if (netif_msg_probe(card))
		pr_info("Initialized device %s.\n", netdev->name);

	return 0;
}

static struct spider_net_card *
spider_net_alloc_card(void)
{
	struct net_device *netdev;
	struct spider_net_card *card;
	size_t alloc_size;

	alloc_size = sizeof(struct spider_net_card) +
	   (tx_descriptors + rx_descriptors) * sizeof(struct spider_net_descr);
	netdev = alloc_etherdev(alloc_size);
	if (!netdev)
		return NULL;

	card = netdev_priv(netdev);
	card->netdev = netdev;
	card->msg_enable = SPIDER_NET_DEFAULT_MSG;
	INIT_WORK(&card->tx_timeout_task, spider_net_tx_timeout_task);
	init_waitqueue_head(&card->waitq);
	atomic_set(&card->tx_timeout_task_counter, 0);

	card->rx_chain.num_desc = rx_descriptors;
	card->rx_chain.ring = card->darray;
	card->tx_chain.num_desc = tx_descriptors;
	card->tx_chain.ring = card->darray + rx_descriptors;

	return card;
}

static void
spider_net_undo_pci_setup(struct spider_net_card *card)
{
	iounmap(card->regs);
	pci_release_regions(card->pdev);
}

static struct spider_net_card *
spider_net_setup_pci_dev(struct pci_dev *pdev)
{
	struct spider_net_card *card;
	unsigned long mmio_start, mmio_len;

	if (pci_enable_device(pdev)) {
		dev_err(&pdev->dev, "Couldn't enable PCI device\n");
		return NULL;
	}

	if (!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM)) {
		dev_err(&pdev->dev,
		        "Couldn't find proper PCI device base address.\n");
		goto out_disable_dev;
	}

	if (pci_request_regions(pdev, spider_net_driver_name)) {
		dev_err(&pdev->dev,
		        "Couldn't obtain PCI resources, aborting.\n");
		goto out_disable_dev;
	}

	pci_set_master(pdev);

	card = spider_net_alloc_card();
	if (!card) {
		dev_err(&pdev->dev,
		        "Couldn't allocate net_device structure, aborting.\n");
		goto out_release_regions;
	}
	card->pdev = pdev;

	
	mmio_start = pci_resource_start(pdev, 0);
	mmio_len = pci_resource_len(pdev, 0);

	card->netdev->mem_start = mmio_start;
	card->netdev->mem_end = mmio_start + mmio_len;
	card->regs = ioremap(mmio_start, mmio_len);

	if (!card->regs) {
		dev_err(&pdev->dev,
		        "Couldn't obtain PCI resources, aborting.\n");
		goto out_release_regions;
	}

	return card;

out_release_regions:
	pci_release_regions(pdev);
out_disable_dev:
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
	return NULL;
}

static int __devinit
spider_net_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int err = -EIO;
	struct spider_net_card *card;

	card = spider_net_setup_pci_dev(pdev);
	if (!card)
		goto out;

	spider_net_workaround_rxramfull(card);
	spider_net_init_card(card);

	err = spider_net_setup_phy(card);
	if (err)
		goto out_undo_pci;

	err = spider_net_setup_netdev(card);
	if (err)
		goto out_undo_pci;

	return 0;

out_undo_pci:
	spider_net_undo_pci_setup(card);
	free_netdev(card->netdev);
out:
	return err;
}

static void __devexit
spider_net_remove(struct pci_dev *pdev)
{
	struct net_device *netdev;
	struct spider_net_card *card;

	netdev = pci_get_drvdata(pdev);
	card = netdev_priv(netdev);

	wait_event(card->waitq,
		   atomic_read(&card->tx_timeout_task_counter) == 0);

	unregister_netdev(netdev);

	
	spider_net_write_reg(card, SPIDER_NET_CKRCTRL,
			     SPIDER_NET_CKRCTRL_STOP_VALUE);
	spider_net_write_reg(card, SPIDER_NET_CKRCTRL,
			     SPIDER_NET_CKRCTRL_RUN_VALUE);

	spider_net_undo_pci_setup(card);
	free_netdev(netdev);
}

static struct pci_driver spider_net_driver = {
	.name		= spider_net_driver_name,
	.id_table	= spider_net_pci_tbl,
	.probe		= spider_net_probe,
	.remove		= __devexit_p(spider_net_remove)
};

static int __init spider_net_init(void)
{
	printk(KERN_INFO "Spidernet version %s.\n", VERSION);

	if (rx_descriptors < SPIDER_NET_RX_DESCRIPTORS_MIN) {
		rx_descriptors = SPIDER_NET_RX_DESCRIPTORS_MIN;
		pr_info("adjusting rx descriptors to %i.\n", rx_descriptors);
	}
	if (rx_descriptors > SPIDER_NET_RX_DESCRIPTORS_MAX) {
		rx_descriptors = SPIDER_NET_RX_DESCRIPTORS_MAX;
		pr_info("adjusting rx descriptors to %i.\n", rx_descriptors);
	}
	if (tx_descriptors < SPIDER_NET_TX_DESCRIPTORS_MIN) {
		tx_descriptors = SPIDER_NET_TX_DESCRIPTORS_MIN;
		pr_info("adjusting tx descriptors to %i.\n", tx_descriptors);
	}
	if (tx_descriptors > SPIDER_NET_TX_DESCRIPTORS_MAX) {
		tx_descriptors = SPIDER_NET_TX_DESCRIPTORS_MAX;
		pr_info("adjusting tx descriptors to %i.\n", tx_descriptors);
	}

	return pci_register_driver(&spider_net_driver);
}

static void __exit spider_net_cleanup(void)
{
	pci_unregister_driver(&spider_net_driver);
}

module_init(spider_net_init);
module_exit(spider_net_cleanup);
