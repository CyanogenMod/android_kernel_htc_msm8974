/*
    Copyright (c) 2001, 2002 by D-Link Corporation
    Written by Edward Peng.<edward_peng@dlink.com.tw>
    Created 03-May-2001, base on Linux' sundance.c.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#define DRV_NAME	"DL2000/TC902x-based linux driver"
#define DRV_VERSION	"v1.19"
#define DRV_RELDATE	"2007/08/12"
#include "dl2k.h"
#include <linux/dma-mapping.h>

static char version[] __devinitdata =
      KERN_INFO DRV_NAME " " DRV_VERSION " " DRV_RELDATE "\n";
#define MAX_UNITS 8
static int mtu[MAX_UNITS];
static int vlan[MAX_UNITS];
static int jumbo[MAX_UNITS];
static char *media[MAX_UNITS];
static int tx_flow=-1;
static int rx_flow=-1;
static int copy_thresh;
static int rx_coalesce=10;	
static int rx_timeout=200;	
static int tx_coalesce=16;	


MODULE_AUTHOR ("Edward Peng");
MODULE_DESCRIPTION ("D-Link DL2000-based Gigabit Ethernet Adapter");
MODULE_LICENSE("GPL");
module_param_array(mtu, int, NULL, 0);
module_param_array(media, charp, NULL, 0);
module_param_array(vlan, int, NULL, 0);
module_param_array(jumbo, int, NULL, 0);
module_param(tx_flow, int, 0);
module_param(rx_flow, int, 0);
module_param(copy_thresh, int, 0);
module_param(rx_coalesce, int, 0);	
module_param(rx_timeout, int, 0);	
module_param(tx_coalesce, int, 0); 


#define DEFAULT_INTR (RxDMAComplete | HostError | IntRequested | TxDMAComplete| \
       UpdateStats | LinkEvent)
#define EnableInt() \
writew(DEFAULT_INTR, ioaddr + IntEnable)

static const int max_intrloop = 50;
static const int multicast_filter_limit = 0x40;

static int rio_open (struct net_device *dev);
static void rio_timer (unsigned long data);
static void rio_tx_timeout (struct net_device *dev);
static void alloc_list (struct net_device *dev);
static netdev_tx_t start_xmit (struct sk_buff *skb, struct net_device *dev);
static irqreturn_t rio_interrupt (int irq, void *dev_instance);
static void rio_free_tx (struct net_device *dev, int irq);
static void tx_error (struct net_device *dev, int tx_status);
static int receive_packet (struct net_device *dev);
static void rio_error (struct net_device *dev, int int_status);
static int change_mtu (struct net_device *dev, int new_mtu);
static void set_multicast (struct net_device *dev);
static struct net_device_stats *get_stats (struct net_device *dev);
static int clear_stats (struct net_device *dev);
static int rio_ioctl (struct net_device *dev, struct ifreq *rq, int cmd);
static int rio_close (struct net_device *dev);
static int find_miiphy (struct net_device *dev);
static int parse_eeprom (struct net_device *dev);
static int read_eeprom (long ioaddr, int eep_addr);
static int mii_wait_link (struct net_device *dev, int wait);
static int mii_set_media (struct net_device *dev);
static int mii_get_media (struct net_device *dev);
static int mii_set_media_pcs (struct net_device *dev);
static int mii_get_media_pcs (struct net_device *dev);
static int mii_read (struct net_device *dev, int phy_addr, int reg_num);
static int mii_write (struct net_device *dev, int phy_addr, int reg_num,
		      u16 data);

static const struct ethtool_ops ethtool_ops;

static const struct net_device_ops netdev_ops = {
	.ndo_open		= rio_open,
	.ndo_start_xmit	= start_xmit,
	.ndo_stop		= rio_close,
	.ndo_get_stats		= get_stats,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_set_rx_mode	= set_multicast,
	.ndo_do_ioctl		= rio_ioctl,
	.ndo_tx_timeout		= rio_tx_timeout,
	.ndo_change_mtu		= change_mtu,
};

static int __devinit
rio_probe1 (struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct net_device *dev;
	struct netdev_private *np;
	static int card_idx;
	int chip_idx = ent->driver_data;
	int err, irq;
	long ioaddr;
	static int version_printed;
	void *ring_space;
	dma_addr_t ring_dma;

	if (!version_printed++)
		printk ("%s", version);

	err = pci_enable_device (pdev);
	if (err)
		return err;

	irq = pdev->irq;
	err = pci_request_regions (pdev, "dl2k");
	if (err)
		goto err_out_disable;

	pci_set_master (pdev);
	dev = alloc_etherdev (sizeof (*np));
	if (!dev) {
		err = -ENOMEM;
		goto err_out_res;
	}
	SET_NETDEV_DEV(dev, &pdev->dev);

#ifdef MEM_MAPPING
	ioaddr = pci_resource_start (pdev, 1);
	ioaddr = (long) ioremap (ioaddr, RIO_IO_SIZE);
	if (!ioaddr) {
		err = -ENOMEM;
		goto err_out_dev;
	}
#else
	ioaddr = pci_resource_start (pdev, 0);
#endif
	dev->base_addr = ioaddr;
	dev->irq = irq;
	np = netdev_priv(dev);
	np->chip_id = chip_idx;
	np->pdev = pdev;
	spin_lock_init (&np->tx_lock);
	spin_lock_init (&np->rx_lock);

	
	np->an_enable = 1;
	np->tx_coalesce = 1;
	if (card_idx < MAX_UNITS) {
		if (media[card_idx] != NULL) {
			np->an_enable = 0;
			if (strcmp (media[card_idx], "auto") == 0 ||
			    strcmp (media[card_idx], "autosense") == 0 ||
			    strcmp (media[card_idx], "0") == 0 ) {
				np->an_enable = 2;
			} else if (strcmp (media[card_idx], "100mbps_fd") == 0 ||
			    strcmp (media[card_idx], "4") == 0) {
				np->speed = 100;
				np->full_duplex = 1;
			} else if (strcmp (media[card_idx], "100mbps_hd") == 0 ||
				   strcmp (media[card_idx], "3") == 0) {
				np->speed = 100;
				np->full_duplex = 0;
			} else if (strcmp (media[card_idx], "10mbps_fd") == 0 ||
				   strcmp (media[card_idx], "2") == 0) {
				np->speed = 10;
				np->full_duplex = 1;
			} else if (strcmp (media[card_idx], "10mbps_hd") == 0 ||
				   strcmp (media[card_idx], "1") == 0) {
				np->speed = 10;
				np->full_duplex = 0;
			} else if (strcmp (media[card_idx], "1000mbps_fd") == 0 ||
				 strcmp (media[card_idx], "6") == 0) {
				np->speed=1000;
				np->full_duplex=1;
			} else if (strcmp (media[card_idx], "1000mbps_hd") == 0 ||
				 strcmp (media[card_idx], "5") == 0) {
				np->speed = 1000;
				np->full_duplex = 0;
			} else {
				np->an_enable = 1;
			}
		}
		if (jumbo[card_idx] != 0) {
			np->jumbo = 1;
			dev->mtu = MAX_JUMBO;
		} else {
			np->jumbo = 0;
			if (mtu[card_idx] > 0 && mtu[card_idx] < PACKET_SIZE)
				dev->mtu = mtu[card_idx];
		}
		np->vlan = (vlan[card_idx] > 0 && vlan[card_idx] < 4096) ?
		    vlan[card_idx] : 0;
		if (rx_coalesce > 0 && rx_timeout > 0) {
			np->rx_coalesce = rx_coalesce;
			np->rx_timeout = rx_timeout;
			np->coalesce = 1;
		}
		np->tx_flow = (tx_flow == 0) ? 0 : 1;
		np->rx_flow = (rx_flow == 0) ? 0 : 1;

		if (tx_coalesce < 1)
			tx_coalesce = 1;
		else if (tx_coalesce > TX_RING_SIZE-1)
			tx_coalesce = TX_RING_SIZE - 1;
	}
	dev->netdev_ops = &netdev_ops;
	dev->watchdog_timeo = TX_TIMEOUT;
	SET_ETHTOOL_OPS(dev, &ethtool_ops);
#if 0
	dev->features = NETIF_F_IP_CSUM;
#endif
	pci_set_drvdata (pdev, dev);

	ring_space = pci_alloc_consistent (pdev, TX_TOTAL_SIZE, &ring_dma);
	if (!ring_space)
		goto err_out_iounmap;
	np->tx_ring = ring_space;
	np->tx_ring_dma = ring_dma;

	ring_space = pci_alloc_consistent (pdev, RX_TOTAL_SIZE, &ring_dma);
	if (!ring_space)
		goto err_out_unmap_tx;
	np->rx_ring = ring_space;
	np->rx_ring_dma = ring_dma;

	
	parse_eeprom (dev);

	
	err = find_miiphy (dev);
	if (err)
		goto err_out_unmap_rx;

	
	np->phy_media = (readw(ioaddr + ASICCtrl) & PhyMedia) ? 1 : 0;
	np->link_status = 0;
	
	if (np->phy_media) {
		
	 	if (np->an_enable == 2) {
			np->an_enable = 1;
		}
		mii_set_media_pcs (dev);
	} else {
		if (np->speed == 1000)
			np->an_enable = 1;
		mii_set_media (dev);
	}

	err = register_netdev (dev);
	if (err)
		goto err_out_unmap_rx;

	card_idx++;

	printk (KERN_INFO "%s: %s, %pM, IRQ %d\n",
		dev->name, np->name, dev->dev_addr, irq);
	if (tx_coalesce > 1)
		printk(KERN_INFO "tx_coalesce:\t%d packets\n",
				tx_coalesce);
	if (np->coalesce)
		printk(KERN_INFO
		       "rx_coalesce:\t%d packets\n"
		       "rx_timeout: \t%d ns\n",
				np->rx_coalesce, np->rx_timeout*640);
	if (np->vlan)
		printk(KERN_INFO "vlan(id):\t%d\n", np->vlan);
	return 0;

      err_out_unmap_rx:
	pci_free_consistent (pdev, RX_TOTAL_SIZE, np->rx_ring, np->rx_ring_dma);
      err_out_unmap_tx:
	pci_free_consistent (pdev, TX_TOTAL_SIZE, np->tx_ring, np->tx_ring_dma);
      err_out_iounmap:
#ifdef MEM_MAPPING
	iounmap ((void *) ioaddr);

      err_out_dev:
#endif
	free_netdev (dev);

      err_out_res:
	pci_release_regions (pdev);

      err_out_disable:
	pci_disable_device (pdev);
	return err;
}

static int
find_miiphy (struct net_device *dev)
{
	int i, phy_found = 0;
	struct netdev_private *np;
	long ioaddr;
	np = netdev_priv(dev);
	ioaddr = dev->base_addr;
	np->phy_addr = 1;

	for (i = 31; i >= 0; i--) {
		int mii_status = mii_read (dev, i, 1);
		if (mii_status != 0xffff && mii_status != 0x0000) {
			np->phy_addr = i;
			phy_found++;
		}
	}
	if (!phy_found) {
		printk (KERN_ERR "%s: No MII PHY found!\n", dev->name);
		return -ENODEV;
	}
	return 0;
}

static int
parse_eeprom (struct net_device *dev)
{
	int i, j;
	long ioaddr = dev->base_addr;
	u8 sromdata[256];
	u8 *psib;
	u32 crc;
	PSROM_t psrom = (PSROM_t) sromdata;
	struct netdev_private *np = netdev_priv(dev);

	int cid, next;

#ifdef	MEM_MAPPING
	ioaddr = pci_resource_start (np->pdev, 0);
#endif
	
	for (i = 0; i < 128; i++) {
		((__le16 *) sromdata)[i] = cpu_to_le16(read_eeprom (ioaddr, i));
	}
#ifdef	MEM_MAPPING
	ioaddr = dev->base_addr;
#endif
	if (np->pdev->vendor == PCI_VENDOR_ID_DLINK) {	
		
		crc = ~ether_crc_le (256 - 4, sromdata);
		if (psrom->crc != cpu_to_le32(crc)) {
			printk (KERN_ERR "%s: EEPROM data CRC error.\n",
					dev->name);
			return -1;
		}
	}

	
	for (i = 0; i < 6; i++)
		dev->dev_addr[i] = psrom->mac_addr[i];

	if (np->pdev->vendor != PCI_VENDOR_ID_DLINK) {
		return 0;
	}

	
	i = 0x30;
	psib = (u8 *) sromdata;
	do {
		cid = psib[i++];
		next = psib[i++];
		if ((cid == 0 && next == 0) || (cid == 0xff && next == 0xff)) {
			printk (KERN_ERR "Cell data error\n");
			return -1;
		}
		switch (cid) {
		case 0:	
			break;
		case 1:	
			return 0;
		case 2:	
			np->duplex_polarity = psib[i];
			writeb (readb (ioaddr + PhyCtrl) | psib[i],
				ioaddr + PhyCtrl);
			break;
		case 3:	
			np->wake_polarity = psib[i];
			break;
		case 9:	
			j = (next - i > 255) ? 255 : next - i;
			memcpy (np->name, &(psib[i]), j);
			break;
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:	
			break;
		default:	
			return -1;
		}
		i = next;
	} while (1);

	return 0;
}

static int
rio_open (struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	long ioaddr = dev->base_addr;
	int i;
	u16 macctrl;

	i = request_irq (dev->irq, rio_interrupt, IRQF_SHARED, dev->name, dev);
	if (i)
		return i;

	
	writew (GlobalReset | DMAReset | FIFOReset | NetworkReset | HostReset,
		ioaddr + ASICCtrl + 2);
	mdelay(10);

	
	writel (readl (ioaddr + DebugCtrl) | 0x0230, ioaddr + DebugCtrl);

	
	if (np->jumbo != 0)
		writew (MAX_JUMBO+14, ioaddr + MaxFrameSize);

	alloc_list (dev);

	
	for (i = 0; i < 6; i++)
		writeb (dev->dev_addr[i], ioaddr + StationAddr0 + i);

	set_multicast (dev);
	if (np->coalesce) {
		writel (np->rx_coalesce | np->rx_timeout << 16,
			ioaddr + RxDMAIntCtrl);
	}
	
	writeb (0x20, ioaddr + RxDMAPollPeriod);
	writeb (0xff, ioaddr + TxDMAPollPeriod);
	writeb (0x30, ioaddr + RxDMABurstThresh);
	writeb (0x30, ioaddr + RxDMAUrgentThresh);
	writel (0x0007ffff, ioaddr + RmonStatMask);
	
	clear_stats (dev);

	
	if (np->vlan) {
		
		writel (readl(ioaddr + RxDMAIntCtrl) | 0x7 << 10,
			ioaddr + RxDMAIntCtrl);
		
		writew (np->vlan, ioaddr + VLANId);
		
		writel (0x8100 << 16 | np->vlan, ioaddr + VLANTag);
		writel (readl (ioaddr + MACCtrl) | AutoVLANuntagging,
			ioaddr + MACCtrl);
	}

	init_timer (&np->timer);
	np->timer.expires = jiffies + 1*HZ;
	np->timer.data = (unsigned long) dev;
	np->timer.function = rio_timer;
	add_timer (&np->timer);

	
	writel (readl (ioaddr + MACCtrl) | StatsEnable | RxEnable | TxEnable,
			ioaddr + MACCtrl);

	macctrl = 0;
	macctrl |= (np->vlan) ? AutoVLANuntagging : 0;
	macctrl |= (np->full_duplex) ? DuplexSelect : 0;
	macctrl |= (np->tx_flow) ? TxFlowControlEnable : 0;
	macctrl |= (np->rx_flow) ? RxFlowControlEnable : 0;
	writew(macctrl,	ioaddr + MACCtrl);

	netif_start_queue (dev);

	
	EnableInt ();
	return 0;
}

static void
rio_timer (unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	struct netdev_private *np = netdev_priv(dev);
	unsigned int entry;
	int next_tick = 1*HZ;
	unsigned long flags;

	spin_lock_irqsave(&np->rx_lock, flags);
	
	if (np->cur_rx - np->old_rx >= RX_RING_SIZE) {
		printk(KERN_INFO "Try to recover rx ring exhausted...\n");
		
		for (; np->cur_rx - np->old_rx > 0; np->old_rx++) {
			struct sk_buff *skb;
			entry = np->old_rx % RX_RING_SIZE;
			
			if (np->rx_skbuff[entry] == NULL) {
				skb = netdev_alloc_skb_ip_align(dev,
								np->rx_buf_sz);
				if (skb == NULL) {
					np->rx_ring[entry].fraginfo = 0;
					printk (KERN_INFO
						"%s: Still unable to re-allocate Rx skbuff.#%d\n",
						dev->name, entry);
					break;
				}
				np->rx_skbuff[entry] = skb;
				np->rx_ring[entry].fraginfo =
				    cpu_to_le64 (pci_map_single
					 (np->pdev, skb->data, np->rx_buf_sz,
					  PCI_DMA_FROMDEVICE));
			}
			np->rx_ring[entry].fraginfo |=
			    cpu_to_le64((u64)np->rx_buf_sz << 48);
			np->rx_ring[entry].status = 0;
		} 
	} 
	spin_unlock_irqrestore (&np->rx_lock, flags);
	np->timer.expires = jiffies + next_tick;
	add_timer(&np->timer);
}

static void
rio_tx_timeout (struct net_device *dev)
{
	long ioaddr = dev->base_addr;

	printk (KERN_INFO "%s: Tx timed out (%4.4x), is buffer full?\n",
		dev->name, readl (ioaddr + TxStatus));
	rio_free_tx(dev, 0);
	dev->if_port = 0;
	dev->trans_start = jiffies; 
}

 
static void
alloc_list (struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	int i;

	np->cur_rx = np->cur_tx = 0;
	np->old_rx = np->old_tx = 0;
	np->rx_buf_sz = (dev->mtu <= 1500 ? PACKET_SIZE : dev->mtu + 32);

	
	for (i = 0; i < TX_RING_SIZE; i++) {
		np->tx_skbuff[i] = NULL;
		np->tx_ring[i].status = cpu_to_le64 (TFDDone);
		np->tx_ring[i].next_desc = cpu_to_le64 (np->tx_ring_dma +
					      ((i+1)%TX_RING_SIZE) *
					      sizeof (struct netdev_desc));
	}

	
	for (i = 0; i < RX_RING_SIZE; i++) {
		np->rx_ring[i].next_desc = cpu_to_le64 (np->rx_ring_dma +
						((i + 1) % RX_RING_SIZE) *
						sizeof (struct netdev_desc));
		np->rx_ring[i].status = 0;
		np->rx_ring[i].fraginfo = 0;
		np->rx_skbuff[i] = NULL;
	}

	
	for (i = 0; i < RX_RING_SIZE; i++) {
		
		struct sk_buff *skb;

		skb = netdev_alloc_skb_ip_align(dev, np->rx_buf_sz);
		np->rx_skbuff[i] = skb;
		if (skb == NULL) {
			printk (KERN_ERR
				"%s: alloc_list: allocate Rx buffer error! ",
				dev->name);
			break;
		}
		
		np->rx_ring[i].fraginfo =
		    cpu_to_le64 ( pci_map_single (
			 	  np->pdev, skb->data, np->rx_buf_sz,
				  PCI_DMA_FROMDEVICE));
		np->rx_ring[i].fraginfo |= cpu_to_le64((u64)np->rx_buf_sz << 48);
	}

	
	writel (np->rx_ring_dma, dev->base_addr + RFDListPtr0);
	writel (0, dev->base_addr + RFDListPtr1);
}

static netdev_tx_t
start_xmit (struct sk_buff *skb, struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	struct netdev_desc *txdesc;
	unsigned entry;
	u32 ioaddr;
	u64 tfc_vlan_tag = 0;

	if (np->link_status == 0) {	
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}
	ioaddr = dev->base_addr;
	entry = np->cur_tx % TX_RING_SIZE;
	np->tx_skbuff[entry] = skb;
	txdesc = &np->tx_ring[entry];

#if 0
	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		txdesc->status |=
		    cpu_to_le64 (TCPChecksumEnable | UDPChecksumEnable |
				 IPChecksumEnable);
	}
#endif
	if (np->vlan) {
		tfc_vlan_tag = VLANTagInsert |
		    ((u64)np->vlan << 32) |
		    ((u64)skb->priority << 45);
	}
	txdesc->fraginfo = cpu_to_le64 (pci_map_single (np->pdev, skb->data,
							skb->len,
							PCI_DMA_TODEVICE));
	txdesc->fraginfo |= cpu_to_le64((u64)skb->len << 48);

	if (entry % np->tx_coalesce == 0 || np->speed == 10)
		txdesc->status = cpu_to_le64 (entry | tfc_vlan_tag |
					      WordAlignDisable |
					      TxDMAIndicate |
					      (1 << FragCountShift));
	else
		txdesc->status = cpu_to_le64 (entry | tfc_vlan_tag |
					      WordAlignDisable |
					      (1 << FragCountShift));

	
	writel (readl (ioaddr + DMACtrl) | 0x00001000, ioaddr + DMACtrl);
	
	writel(10000, ioaddr + CountDown);
	np->cur_tx = (np->cur_tx + 1) % TX_RING_SIZE;
	if ((np->cur_tx - np->old_tx + TX_RING_SIZE) % TX_RING_SIZE
			< TX_QUEUE_LEN - 1 && np->speed != 10) {
		
	} else if (!netif_queue_stopped(dev)) {
		netif_stop_queue (dev);
	}

	
	if (readl (dev->base_addr + TFDListPtr0) == 0) {
		writel (np->tx_ring_dma + entry * sizeof (struct netdev_desc),
			dev->base_addr + TFDListPtr0);
		writel (0, dev->base_addr + TFDListPtr1);
	}

	return NETDEV_TX_OK;
}

static irqreturn_t
rio_interrupt (int irq, void *dev_instance)
{
	struct net_device *dev = dev_instance;
	struct netdev_private *np;
	unsigned int_status;
	long ioaddr;
	int cnt = max_intrloop;
	int handled = 0;

	ioaddr = dev->base_addr;
	np = netdev_priv(dev);
	while (1) {
		int_status = readw (ioaddr + IntStatus);
		writew (int_status, ioaddr + IntStatus);
		int_status &= DEFAULT_INTR;
		if (int_status == 0 || --cnt < 0)
			break;
		handled = 1;
		
		if (int_status & RxDMAComplete)
			receive_packet (dev);
		
		if ((int_status & (TxDMAComplete|IntRequested))) {
			int tx_status;
			tx_status = readl (ioaddr + TxStatus);
			if (tx_status & 0x01)
				tx_error (dev, tx_status);
			
			rio_free_tx (dev, 1);
		}

		
		if (int_status &
		    (HostError | LinkEvent | UpdateStats))
			rio_error (dev, int_status);
	}
	if (np->cur_tx != np->old_tx)
		writel (100, ioaddr + CountDown);
	return IRQ_RETVAL(handled);
}

static inline dma_addr_t desc_to_dma(struct netdev_desc *desc)
{
	return le64_to_cpu(desc->fraginfo) & DMA_BIT_MASK(48);
}

static void
rio_free_tx (struct net_device *dev, int irq)
{
	struct netdev_private *np = netdev_priv(dev);
	int entry = np->old_tx % TX_RING_SIZE;
	int tx_use = 0;
	unsigned long flag = 0;

	if (irq)
		spin_lock(&np->tx_lock);
	else
		spin_lock_irqsave(&np->tx_lock, flag);

	
	while (entry != np->cur_tx) {
		struct sk_buff *skb;

		if (!(np->tx_ring[entry].status & cpu_to_le64(TFDDone)))
			break;
		skb = np->tx_skbuff[entry];
		pci_unmap_single (np->pdev,
				  desc_to_dma(&np->tx_ring[entry]),
				  skb->len, PCI_DMA_TODEVICE);
		if (irq)
			dev_kfree_skb_irq (skb);
		else
			dev_kfree_skb (skb);

		np->tx_skbuff[entry] = NULL;
		entry = (entry + 1) % TX_RING_SIZE;
		tx_use++;
	}
	if (irq)
		spin_unlock(&np->tx_lock);
	else
		spin_unlock_irqrestore(&np->tx_lock, flag);
	np->old_tx = entry;


	if (netif_queue_stopped(dev) &&
	    ((np->cur_tx - np->old_tx + TX_RING_SIZE) % TX_RING_SIZE
	    < TX_QUEUE_LEN - 1 || np->speed == 10)) {
		netif_wake_queue (dev);
	}
}

static void
tx_error (struct net_device *dev, int tx_status)
{
	struct netdev_private *np;
	long ioaddr = dev->base_addr;
	int frame_id;
	int i;

	np = netdev_priv(dev);

	frame_id = (tx_status & 0xffff0000);
	printk (KERN_ERR "%s: Transmit error, TxStatus %4.4x, FrameId %d.\n",
		dev->name, tx_status, frame_id);
	np->stats.tx_errors++;
	
	if (tx_status & 0x10) {
		np->stats.tx_fifo_errors++;
		writew (readw (ioaddr + TxStartThresh) + 0x10,
			ioaddr + TxStartThresh);
		
		writew (TxReset | DMAReset | FIFOReset | NetworkReset,
			ioaddr + ASICCtrl + 2);
		
		for (i = 50; i > 0; i--) {
			if ((readw (ioaddr + ASICCtrl + 2) & ResetBusy) == 0)
				break;
			mdelay (1);
		}
		rio_free_tx (dev, 1);
		
		writel (np->tx_ring_dma +
			np->old_tx * sizeof (struct netdev_desc),
			dev->base_addr + TFDListPtr0);
		writel (0, dev->base_addr + TFDListPtr1);

		
	}
	
	if (tx_status & 0x04) {
		np->stats.tx_fifo_errors++;
		
		writew (TxReset | FIFOReset, ioaddr + ASICCtrl + 2);
		
		for (i = 50; i > 0; i--) {
			if ((readw (ioaddr + ASICCtrl + 2) & ResetBusy) == 0)
				break;
			mdelay (1);
		}
		
	}
	
#ifdef ETHER_STATS
	if (tx_status & 0x08)
		np->stats.collisions16++;
#else
	if (tx_status & 0x08)
		np->stats.collisions++;
#endif
	
	writel (readw (dev->base_addr + MACCtrl) | TxEnable, ioaddr + MACCtrl);
}

static int
receive_packet (struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	int entry = np->cur_rx % RX_RING_SIZE;
	int cnt = 30;

	
	while (1) {
		struct netdev_desc *desc = &np->rx_ring[entry];
		int pkt_len;
		u64 frame_status;

		if (!(desc->status & cpu_to_le64(RFDDone)) ||
		    !(desc->status & cpu_to_le64(FrameStart)) ||
		    !(desc->status & cpu_to_le64(FrameEnd)))
			break;

		
		frame_status = le64_to_cpu(desc->status);
		pkt_len = frame_status & 0xffff;
		if (--cnt < 0)
			break;
		
		if (frame_status & RFS_Errors) {
			np->stats.rx_errors++;
			if (frame_status & (RxRuntFrame | RxLengthError))
				np->stats.rx_length_errors++;
			if (frame_status & RxFCSError)
				np->stats.rx_crc_errors++;
			if (frame_status & RxAlignmentError && np->speed != 1000)
				np->stats.rx_frame_errors++;
			if (frame_status & RxFIFOOverrun)
	 			np->stats.rx_fifo_errors++;
		} else {
			struct sk_buff *skb;

			
			if (pkt_len > copy_thresh) {
				pci_unmap_single (np->pdev,
						  desc_to_dma(desc),
						  np->rx_buf_sz,
						  PCI_DMA_FROMDEVICE);
				skb_put (skb = np->rx_skbuff[entry], pkt_len);
				np->rx_skbuff[entry] = NULL;
			} else if ((skb = netdev_alloc_skb_ip_align(dev, pkt_len))) {
				pci_dma_sync_single_for_cpu(np->pdev,
							    desc_to_dma(desc),
							    np->rx_buf_sz,
							    PCI_DMA_FROMDEVICE);
				skb_copy_to_linear_data (skb,
						  np->rx_skbuff[entry]->data,
						  pkt_len);
				skb_put (skb, pkt_len);
				pci_dma_sync_single_for_device(np->pdev,
							       desc_to_dma(desc),
							       np->rx_buf_sz,
							       PCI_DMA_FROMDEVICE);
			}
			skb->protocol = eth_type_trans (skb, dev);
#if 0
			
			if (np->pdev->pci_rev_id >= 0x0c &&
				!(frame_status & (TCPError | UDPError | IPError))) {
				skb->ip_summed = CHECKSUM_UNNECESSARY;
			}
#endif
			netif_rx (skb);
		}
		entry = (entry + 1) % RX_RING_SIZE;
	}
	spin_lock(&np->rx_lock);
	np->cur_rx = entry;
	
	entry = np->old_rx;
	while (entry != np->cur_rx) {
		struct sk_buff *skb;
		
		if (np->rx_skbuff[entry] == NULL) {
			skb = netdev_alloc_skb_ip_align(dev, np->rx_buf_sz);
			if (skb == NULL) {
				np->rx_ring[entry].fraginfo = 0;
				printk (KERN_INFO
					"%s: receive_packet: "
					"Unable to re-allocate Rx skbuff.#%d\n",
					dev->name, entry);
				break;
			}
			np->rx_skbuff[entry] = skb;
			np->rx_ring[entry].fraginfo =
			    cpu_to_le64 (pci_map_single
					 (np->pdev, skb->data, np->rx_buf_sz,
					  PCI_DMA_FROMDEVICE));
		}
		np->rx_ring[entry].fraginfo |=
		    cpu_to_le64((u64)np->rx_buf_sz << 48);
		np->rx_ring[entry].status = 0;
		entry = (entry + 1) % RX_RING_SIZE;
	}
	np->old_rx = entry;
	spin_unlock(&np->rx_lock);
	return 0;
}

static void
rio_error (struct net_device *dev, int int_status)
{
	long ioaddr = dev->base_addr;
	struct netdev_private *np = netdev_priv(dev);
	u16 macctrl;

	
	if (int_status & LinkEvent) {
		if (mii_wait_link (dev, 10) == 0) {
			printk (KERN_INFO "%s: Link up\n", dev->name);
			if (np->phy_media)
				mii_get_media_pcs (dev);
			else
				mii_get_media (dev);
			if (np->speed == 1000)
				np->tx_coalesce = tx_coalesce;
			else
				np->tx_coalesce = 1;
			macctrl = 0;
			macctrl |= (np->vlan) ? AutoVLANuntagging : 0;
			macctrl |= (np->full_duplex) ? DuplexSelect : 0;
			macctrl |= (np->tx_flow) ?
				TxFlowControlEnable : 0;
			macctrl |= (np->rx_flow) ?
				RxFlowControlEnable : 0;
			writew(macctrl,	ioaddr + MACCtrl);
			np->link_status = 1;
			netif_carrier_on(dev);
		} else {
			printk (KERN_INFO "%s: Link off\n", dev->name);
			np->link_status = 0;
			netif_carrier_off(dev);
		}
	}

	
	if (int_status & UpdateStats) {
		get_stats (dev);
	}

	if (int_status & HostError) {
		printk (KERN_ERR "%s: HostError! IntStatus %4.4x.\n",
			dev->name, int_status);
		writew (GlobalReset | HostReset, ioaddr + ASICCtrl + 2);
		mdelay (500);
	}
}

static struct net_device_stats *
get_stats (struct net_device *dev)
{
	long ioaddr = dev->base_addr;
	struct netdev_private *np = netdev_priv(dev);
#ifdef MEM_MAPPING
	int i;
#endif
	unsigned int stat_reg;


	np->stats.rx_packets += readl (ioaddr + FramesRcvOk);
	np->stats.tx_packets += readl (ioaddr + FramesXmtOk);
	np->stats.rx_bytes += readl (ioaddr + OctetRcvOk);
	np->stats.tx_bytes += readl (ioaddr + OctetXmtOk);

	np->stats.multicast = readl (ioaddr + McstFramesRcvdOk);
	np->stats.collisions += readl (ioaddr + SingleColFrames)
			     +  readl (ioaddr + MultiColFrames);

	
	stat_reg = readw (ioaddr + FramesAbortXSColls);
	np->stats.tx_aborted_errors += stat_reg;
	np->stats.tx_errors += stat_reg;

	stat_reg = readw (ioaddr + CarrierSenseErrors);
	np->stats.tx_carrier_errors += stat_reg;
	np->stats.tx_errors += stat_reg;

	
	readl (ioaddr + McstOctetXmtOk);
	readw (ioaddr + BcstFramesXmtdOk);
	readl (ioaddr + McstFramesXmtdOk);
	readw (ioaddr + BcstFramesRcvdOk);
	readw (ioaddr + MacControlFramesRcvd);
	readw (ioaddr + FrameTooLongErrors);
	readw (ioaddr + InRangeLengthErrors);
	readw (ioaddr + FramesCheckSeqErrors);
	readw (ioaddr + FramesLostRxErrors);
	readl (ioaddr + McstOctetXmtOk);
	readl (ioaddr + BcstOctetXmtOk);
	readl (ioaddr + McstFramesXmtdOk);
	readl (ioaddr + FramesWDeferredXmt);
	readl (ioaddr + LateCollisions);
	readw (ioaddr + BcstFramesXmtdOk);
	readw (ioaddr + MacControlFramesXmtd);
	readw (ioaddr + FramesWEXDeferal);

#ifdef MEM_MAPPING
	for (i = 0x100; i <= 0x150; i += 4)
		readl (ioaddr + i);
#endif
	readw (ioaddr + TxJumboFrames);
	readw (ioaddr + RxJumboFrames);
	readw (ioaddr + TCPCheckSumErrors);
	readw (ioaddr + UDPCheckSumErrors);
	readw (ioaddr + IPCheckSumErrors);
	return &np->stats;
}

static int
clear_stats (struct net_device *dev)
{
	long ioaddr = dev->base_addr;
#ifdef MEM_MAPPING
	int i;
#endif

	readl (ioaddr + FramesRcvOk);
	readl (ioaddr + FramesXmtOk);
	readl (ioaddr + OctetRcvOk);
	readl (ioaddr + OctetXmtOk);

	readl (ioaddr + McstFramesRcvdOk);
	readl (ioaddr + SingleColFrames);
	readl (ioaddr + MultiColFrames);
	readl (ioaddr + LateCollisions);
	
	readw (ioaddr + FrameTooLongErrors);
	readw (ioaddr + InRangeLengthErrors);
	readw (ioaddr + FramesCheckSeqErrors);
	readw (ioaddr + FramesLostRxErrors);

	
	readw (ioaddr + FramesAbortXSColls);
	readw (ioaddr + CarrierSenseErrors);

	
	readl (ioaddr + McstOctetXmtOk);
	readw (ioaddr + BcstFramesXmtdOk);
	readl (ioaddr + McstFramesXmtdOk);
	readw (ioaddr + BcstFramesRcvdOk);
	readw (ioaddr + MacControlFramesRcvd);
	readl (ioaddr + McstOctetXmtOk);
	readl (ioaddr + BcstOctetXmtOk);
	readl (ioaddr + McstFramesXmtdOk);
	readl (ioaddr + FramesWDeferredXmt);
	readw (ioaddr + BcstFramesXmtdOk);
	readw (ioaddr + MacControlFramesXmtd);
	readw (ioaddr + FramesWEXDeferal);
#ifdef MEM_MAPPING
	for (i = 0x100; i <= 0x150; i += 4)
		readl (ioaddr + i);
#endif
	readw (ioaddr + TxJumboFrames);
	readw (ioaddr + RxJumboFrames);
	readw (ioaddr + TCPCheckSumErrors);
	readw (ioaddr + UDPCheckSumErrors);
	readw (ioaddr + IPCheckSumErrors);
	return 0;
}


static int
change_mtu (struct net_device *dev, int new_mtu)
{
	struct netdev_private *np = netdev_priv(dev);
	int max = (np->jumbo) ? MAX_JUMBO : 1536;

	if ((new_mtu < 68) || (new_mtu > max)) {
		return -EINVAL;
	}

	dev->mtu = new_mtu;

	return 0;
}

static void
set_multicast (struct net_device *dev)
{
	long ioaddr = dev->base_addr;
	u32 hash_table[2];
	u16 rx_mode = 0;
	struct netdev_private *np = netdev_priv(dev);

	hash_table[0] = hash_table[1] = 0;
	
	hash_table[1] |= 0x02000000;
	if (dev->flags & IFF_PROMISC) {
		
		rx_mode = ReceiveAllFrames;
	} else if ((dev->flags & IFF_ALLMULTI) ||
			(netdev_mc_count(dev) > multicast_filter_limit)) {
		
		rx_mode = ReceiveBroadcast | ReceiveMulticast | ReceiveUnicast;
	} else if (!netdev_mc_empty(dev)) {
		struct netdev_hw_addr *ha;
		rx_mode =
		    ReceiveBroadcast | ReceiveMulticastHash | ReceiveUnicast;
		netdev_for_each_mc_addr(ha, dev) {
			int bit, index = 0;
			int crc = ether_crc_le(ETH_ALEN, ha->addr);
			for (bit = 0; bit < 6; bit++)
				if (crc & (1 << (31 - bit)))
					index |= (1 << bit);
			hash_table[index / 32] |= (1 << (index % 32));
		}
	} else {
		rx_mode = ReceiveBroadcast | ReceiveUnicast;
	}
	if (np->vlan) {
		
		rx_mode |= ReceiveVLANMatch;
	}

	writel (hash_table[0], ioaddr + HashTable0);
	writel (hash_table[1], ioaddr + HashTable1);
	writew (rx_mode, ioaddr + ReceiveMode);
}

static void rio_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	struct netdev_private *np = netdev_priv(dev);
	strcpy(info->driver, "dl2k");
	strcpy(info->version, DRV_VERSION);
	strcpy(info->bus_info, pci_name(np->pdev));
}

static int rio_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct netdev_private *np = netdev_priv(dev);
	if (np->phy_media) {
		
		cmd->supported = SUPPORTED_Autoneg | SUPPORTED_FIBRE;
		cmd->advertising= ADVERTISED_Autoneg | ADVERTISED_FIBRE;
		cmd->port = PORT_FIBRE;
		cmd->transceiver = XCVR_INTERNAL;
	} else {
		
		cmd->supported = SUPPORTED_10baseT_Half |
			SUPPORTED_10baseT_Full | SUPPORTED_100baseT_Half
			| SUPPORTED_100baseT_Full | SUPPORTED_1000baseT_Full |
			SUPPORTED_Autoneg | SUPPORTED_MII;
		cmd->advertising = ADVERTISED_10baseT_Half |
			ADVERTISED_10baseT_Full | ADVERTISED_100baseT_Half |
			ADVERTISED_100baseT_Full | ADVERTISED_1000baseT_Full|
			ADVERTISED_Autoneg | ADVERTISED_MII;
		cmd->port = PORT_MII;
		cmd->transceiver = XCVR_INTERNAL;
	}
	if ( np->link_status ) {
		ethtool_cmd_speed_set(cmd, np->speed);
		cmd->duplex = np->full_duplex ? DUPLEX_FULL : DUPLEX_HALF;
	} else {
		ethtool_cmd_speed_set(cmd, -1);
		cmd->duplex = -1;
	}
	if ( np->an_enable)
		cmd->autoneg = AUTONEG_ENABLE;
	else
		cmd->autoneg = AUTONEG_DISABLE;

	cmd->phy_address = np->phy_addr;
	return 0;
}

static int rio_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct netdev_private *np = netdev_priv(dev);
	netif_carrier_off(dev);
	if (cmd->autoneg == AUTONEG_ENABLE) {
		if (np->an_enable)
			return 0;
		else {
			np->an_enable = 1;
			mii_set_media(dev);
			return 0;
		}
	} else {
		np->an_enable = 0;
		if (np->speed == 1000) {
			ethtool_cmd_speed_set(cmd, SPEED_100);
			cmd->duplex = DUPLEX_FULL;
			printk("Warning!! Can't disable Auto negotiation in 1000Mbps, change to Manual 100Mbps, Full duplex.\n");
		}
		switch (ethtool_cmd_speed(cmd)) {
		case SPEED_10:
			np->speed = 10;
			np->full_duplex = (cmd->duplex == DUPLEX_FULL);
			break;
		case SPEED_100:
			np->speed = 100;
			np->full_duplex = (cmd->duplex == DUPLEX_FULL);
			break;
		case SPEED_1000: 
		default:
			return -EINVAL;
		}
		mii_set_media(dev);
	}
	return 0;
}

static u32 rio_get_link(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	return np->link_status;
}

static const struct ethtool_ops ethtool_ops = {
	.get_drvinfo = rio_get_drvinfo,
	.get_settings = rio_get_settings,
	.set_settings = rio_set_settings,
	.get_link = rio_get_link,
};

static int
rio_ioctl (struct net_device *dev, struct ifreq *rq, int cmd)
{
	int phy_addr;
	struct netdev_private *np = netdev_priv(dev);
	struct mii_ioctl_data *miidata = if_mii(rq);

	phy_addr = np->phy_addr;
	switch (cmd) {
	case SIOCGMIIPHY:
		miidata->phy_id = phy_addr;
		break;
	case SIOCGMIIREG:
		miidata->val_out = mii_read (dev, phy_addr, miidata->reg_num);
		break;
	case SIOCSMIIREG:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		mii_write (dev, phy_addr, miidata->reg_num, miidata->val_in);
		break;
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

#define EEP_READ 0x0200
#define EEP_BUSY 0x8000
static int
read_eeprom (long ioaddr, int eep_addr)
{
	int i = 1000;
	outw (EEP_READ | (eep_addr & 0xff), ioaddr + EepromCtrl);
	while (i-- > 0) {
		if (!(inw (ioaddr + EepromCtrl) & EEP_BUSY)) {
			return inw (ioaddr + EepromData);
		}
	}
	return 0;
}

enum phy_ctrl_bits {
	MII_READ = 0x00, MII_CLK = 0x01, MII_DATA1 = 0x02, MII_WRITE = 0x04,
	MII_DUPLEX = 0x08,
};

#define mii_delay() readb(ioaddr)
static void
mii_sendbit (struct net_device *dev, u32 data)
{
	long ioaddr = dev->base_addr + PhyCtrl;
	data = (data) ? MII_DATA1 : 0;
	data |= MII_WRITE;
	data |= (readb (ioaddr) & 0xf8) | MII_WRITE;
	writeb (data, ioaddr);
	mii_delay ();
	writeb (data | MII_CLK, ioaddr);
	mii_delay ();
}

static int
mii_getbit (struct net_device *dev)
{
	long ioaddr = dev->base_addr + PhyCtrl;
	u8 data;

	data = (readb (ioaddr) & 0xf8) | MII_READ;
	writeb (data, ioaddr);
	mii_delay ();
	writeb (data | MII_CLK, ioaddr);
	mii_delay ();
	return ((readb (ioaddr) >> 1) & 1);
}

static void
mii_send_bits (struct net_device *dev, u32 data, int len)
{
	int i;
	for (i = len - 1; i >= 0; i--) {
		mii_sendbit (dev, data & (1 << i));
	}
}

static int
mii_read (struct net_device *dev, int phy_addr, int reg_num)
{
	u32 cmd;
	int i;
	u32 retval = 0;

	
	mii_send_bits (dev, 0xffffffff, 32);
	
	
	cmd = (0x06 << 10 | phy_addr << 5 | reg_num);
	mii_send_bits (dev, cmd, 14);
	
	if (mii_getbit (dev))
		goto err_out;
	
	for (i = 0; i < 16; i++) {
		retval |= mii_getbit (dev);
		retval <<= 1;
	}
	
	mii_getbit (dev);
	return (retval >> 1) & 0xffff;

      err_out:
	return 0;
}
static int
mii_write (struct net_device *dev, int phy_addr, int reg_num, u16 data)
{
	u32 cmd;

	
	mii_send_bits (dev, 0xffffffff, 32);
	
	
	cmd = (0x5002 << 16) | (phy_addr << 23) | (reg_num << 18) | data;
	mii_send_bits (dev, cmd, 32);
	
	mii_getbit (dev);
	return 0;
}
static int
mii_wait_link (struct net_device *dev, int wait)
{
	__u16 bmsr;
	int phy_addr;
	struct netdev_private *np;

	np = netdev_priv(dev);
	phy_addr = np->phy_addr;

	do {
		bmsr = mii_read (dev, phy_addr, MII_BMSR);
		if (bmsr & BMSR_LSTATUS)
			return 0;
		mdelay (1);
	} while (--wait > 0);
	return -1;
}
static int
mii_get_media (struct net_device *dev)
{
	__u16 negotiate;
	__u16 bmsr;
	__u16 mscr;
	__u16 mssr;
	int phy_addr;
	struct netdev_private *np;

	np = netdev_priv(dev);
	phy_addr = np->phy_addr;

	bmsr = mii_read (dev, phy_addr, MII_BMSR);
	if (np->an_enable) {
		if (!(bmsr & BMSR_ANEGCOMPLETE)) {
			
			return -1;
		}
		negotiate = mii_read (dev, phy_addr, MII_ADVERTISE) &
			mii_read (dev, phy_addr, MII_LPA);
		mscr = mii_read (dev, phy_addr, MII_CTRL1000);
		mssr = mii_read (dev, phy_addr, MII_STAT1000);
		if (mscr & ADVERTISE_1000FULL && mssr & LPA_1000FULL) {
			np->speed = 1000;
			np->full_duplex = 1;
			printk (KERN_INFO "Auto 1000 Mbps, Full duplex\n");
		} else if (mscr & ADVERTISE_1000HALF && mssr & LPA_1000HALF) {
			np->speed = 1000;
			np->full_duplex = 0;
			printk (KERN_INFO "Auto 1000 Mbps, Half duplex\n");
		} else if (negotiate & ADVERTISE_100FULL) {
			np->speed = 100;
			np->full_duplex = 1;
			printk (KERN_INFO "Auto 100 Mbps, Full duplex\n");
		} else if (negotiate & ADVERTISE_100HALF) {
			np->speed = 100;
			np->full_duplex = 0;
			printk (KERN_INFO "Auto 100 Mbps, Half duplex\n");
		} else if (negotiate & ADVERTISE_10FULL) {
			np->speed = 10;
			np->full_duplex = 1;
			printk (KERN_INFO "Auto 10 Mbps, Full duplex\n");
		} else if (negotiate & ADVERTISE_10HALF) {
			np->speed = 10;
			np->full_duplex = 0;
			printk (KERN_INFO "Auto 10 Mbps, Half duplex\n");
		}
		if (negotiate & ADVERTISE_PAUSE_CAP) {
			np->tx_flow &= 1;
			np->rx_flow &= 1;
		} else if (negotiate & ADVERTISE_PAUSE_ASYM) {
			np->tx_flow = 0;
			np->rx_flow &= 1;
		}
		
	} else {
		__u16 bmcr = mii_read (dev, phy_addr, MII_BMCR);
		switch (bmcr & (BMCR_SPEED100 | BMCR_SPEED1000)) {
		case BMCR_SPEED1000:
			printk (KERN_INFO "Operating at 1000 Mbps, ");
			break;
		case BMCR_SPEED100:
			printk (KERN_INFO "Operating at 100 Mbps, ");
			break;
		case 0:
			printk (KERN_INFO "Operating at 10 Mbps, ");
		}
		if (bmcr & BMCR_FULLDPLX) {
			printk (KERN_CONT "Full duplex\n");
		} else {
			printk (KERN_CONT "Half duplex\n");
		}
	}
	if (np->tx_flow)
		printk(KERN_INFO "Enable Tx Flow Control\n");
	else
		printk(KERN_INFO "Disable Tx Flow Control\n");
	if (np->rx_flow)
		printk(KERN_INFO "Enable Rx Flow Control\n");
	else
		printk(KERN_INFO "Disable Rx Flow Control\n");

	return 0;
}

static int
mii_set_media (struct net_device *dev)
{
	__u16 pscr;
	__u16 bmcr;
	__u16 bmsr;
	__u16 anar;
	int phy_addr;
	struct netdev_private *np;
	np = netdev_priv(dev);
	phy_addr = np->phy_addr;

	
	if (np->an_enable) {
		
		bmsr = mii_read (dev, phy_addr, MII_BMSR);
		anar = mii_read (dev, phy_addr, MII_ADVERTISE) &
			~(ADVERTISE_100FULL | ADVERTISE_10FULL |
			  ADVERTISE_100HALF | ADVERTISE_10HALF |
			  ADVERTISE_100BASE4);
		if (bmsr & BMSR_100FULL)
			anar |= ADVERTISE_100FULL;
		if (bmsr & BMSR_100HALF)
			anar |= ADVERTISE_100HALF;
		if (bmsr & BMSR_100BASE4)
			anar |= ADVERTISE_100BASE4;
		if (bmsr & BMSR_10FULL)
			anar |= ADVERTISE_10FULL;
		if (bmsr & BMSR_10HALF)
			anar |= ADVERTISE_10HALF;
		anar |= ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM;
		mii_write (dev, phy_addr, MII_ADVERTISE, anar);

		
		pscr = mii_read (dev, phy_addr, MII_PHY_SCR);
		pscr |= 3 << 5;	
		mii_write (dev, phy_addr, MII_PHY_SCR, pscr);

		
		mii_write (dev, phy_addr, MII_BMCR, BMCR_RESET);
		bmcr = BMCR_ANENABLE | BMCR_ANRESTART | BMCR_RESET;
		mii_write (dev, phy_addr, MII_BMCR, bmcr);
		mdelay(1);
	} else {
		
		
		pscr = mii_read (dev, phy_addr, MII_PHY_SCR);
		pscr &= ~(3 << 5);
		mii_write (dev, phy_addr, MII_PHY_SCR, pscr);

		
		bmcr = mii_read (dev, phy_addr, MII_BMCR);
		bmcr |= BMCR_RESET;
		mii_write (dev, phy_addr, MII_BMCR, bmcr);

		
		bmcr = 0x1940;	
		mii_write (dev, phy_addr, MII_BMCR, bmcr);
		mdelay (100);	

		
		mii_write (dev, phy_addr, MII_ADVERTISE, 0);

		
		bmcr = BMCR_PDOWN;
		if (np->speed == 100) {
			bmcr |= BMCR_SPEED100;
			printk (KERN_INFO "Manual 100 Mbps, ");
		} else if (np->speed == 10) {
			printk (KERN_INFO "Manual 10 Mbps, ");
		}
		if (np->full_duplex) {
			bmcr |= BMCR_FULLDPLX;
			printk (KERN_CONT "Full duplex\n");
		} else {
			printk (KERN_CONT "Half duplex\n");
		}
#if 0
		
		mscr = mii_read (dev, phy_addr, MII_CTRL1000);
		mscr |= MII_MSCR_CFG_ENABLE;
		mscr &= ~MII_MSCR_CFG_VALUE = 0;
#endif
		mii_write (dev, phy_addr, MII_BMCR, bmcr);
		mdelay(10);
	}
	return 0;
}

static int
mii_get_media_pcs (struct net_device *dev)
{
	__u16 negotiate;
	__u16 bmsr;
	int phy_addr;
	struct netdev_private *np;

	np = netdev_priv(dev);
	phy_addr = np->phy_addr;

	bmsr = mii_read (dev, phy_addr, PCS_BMSR);
	if (np->an_enable) {
		if (!(bmsr & BMSR_ANEGCOMPLETE)) {
			
			return -1;
		}
		negotiate = mii_read (dev, phy_addr, PCS_ANAR) &
			mii_read (dev, phy_addr, PCS_ANLPAR);
		np->speed = 1000;
		if (negotiate & PCS_ANAR_FULL_DUPLEX) {
			printk (KERN_INFO "Auto 1000 Mbps, Full duplex\n");
			np->full_duplex = 1;
		} else {
			printk (KERN_INFO "Auto 1000 Mbps, half duplex\n");
			np->full_duplex = 0;
		}
		if (negotiate & PCS_ANAR_PAUSE) {
			np->tx_flow &= 1;
			np->rx_flow &= 1;
		} else if (negotiate & PCS_ANAR_ASYMMETRIC) {
			np->tx_flow = 0;
			np->rx_flow &= 1;
		}
		
	} else {
		__u16 bmcr = mii_read (dev, phy_addr, PCS_BMCR);
		printk (KERN_INFO "Operating at 1000 Mbps, ");
		if (bmcr & BMCR_FULLDPLX) {
			printk (KERN_CONT "Full duplex\n");
		} else {
			printk (KERN_CONT "Half duplex\n");
		}
	}
	if (np->tx_flow)
		printk(KERN_INFO "Enable Tx Flow Control\n");
	else
		printk(KERN_INFO "Disable Tx Flow Control\n");
	if (np->rx_flow)
		printk(KERN_INFO "Enable Rx Flow Control\n");
	else
		printk(KERN_INFO "Disable Rx Flow Control\n");

	return 0;
}

static int
mii_set_media_pcs (struct net_device *dev)
{
	__u16 bmcr;
	__u16 esr;
	__u16 anar;
	int phy_addr;
	struct netdev_private *np;
	np = netdev_priv(dev);
	phy_addr = np->phy_addr;

	
	if (np->an_enable) {
		
		esr = mii_read (dev, phy_addr, PCS_ESR);
		anar = mii_read (dev, phy_addr, MII_ADVERTISE) &
			~PCS_ANAR_HALF_DUPLEX &
			~PCS_ANAR_FULL_DUPLEX;
		if (esr & (MII_ESR_1000BT_HD | MII_ESR_1000BX_HD))
			anar |= PCS_ANAR_HALF_DUPLEX;
		if (esr & (MII_ESR_1000BT_FD | MII_ESR_1000BX_FD))
			anar |= PCS_ANAR_FULL_DUPLEX;
		anar |= PCS_ANAR_PAUSE | PCS_ANAR_ASYMMETRIC;
		mii_write (dev, phy_addr, MII_ADVERTISE, anar);

		
		mii_write (dev, phy_addr, MII_BMCR, BMCR_RESET);
		bmcr = BMCR_ANENABLE | BMCR_ANRESTART | BMCR_RESET;
		mii_write (dev, phy_addr, MII_BMCR, bmcr);
		mdelay(1);
	} else {
		
		
		bmcr = BMCR_RESET;
		mii_write (dev, phy_addr, MII_BMCR, bmcr);
		mdelay(10);
		if (np->full_duplex) {
			bmcr = BMCR_FULLDPLX;
			printk (KERN_INFO "Manual full duplex\n");
		} else {
			bmcr = 0;
			printk (KERN_INFO "Manual half duplex\n");
		}
		mii_write (dev, phy_addr, MII_BMCR, bmcr);
		mdelay(10);

		
		mii_write (dev, phy_addr, MII_ADVERTISE, 0);
	}
	return 0;
}


static int
rio_close (struct net_device *dev)
{
	long ioaddr = dev->base_addr;
	struct netdev_private *np = netdev_priv(dev);
	struct sk_buff *skb;
	int i;

	netif_stop_queue (dev);

	
	writew (0, ioaddr + IntEnable);

	
	writel (TxDisable | RxDisable | StatsDisable, ioaddr + MACCtrl);

	free_irq (dev->irq, dev);
	del_timer_sync (&np->timer);

	
	for (i = 0; i < RX_RING_SIZE; i++) {
		skb = np->rx_skbuff[i];
		if (skb) {
			pci_unmap_single(np->pdev,
					 desc_to_dma(&np->rx_ring[i]),
					 skb->len, PCI_DMA_FROMDEVICE);
			dev_kfree_skb (skb);
			np->rx_skbuff[i] = NULL;
		}
		np->rx_ring[i].status = 0;
		np->rx_ring[i].fraginfo = 0;
	}
	for (i = 0; i < TX_RING_SIZE; i++) {
		skb = np->tx_skbuff[i];
		if (skb) {
			pci_unmap_single(np->pdev,
					 desc_to_dma(&np->tx_ring[i]),
					 skb->len, PCI_DMA_TODEVICE);
			dev_kfree_skb (skb);
			np->tx_skbuff[i] = NULL;
		}
	}

	return 0;
}

static void __devexit
rio_remove1 (struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata (pdev);

	if (dev) {
		struct netdev_private *np = netdev_priv(dev);

		unregister_netdev (dev);
		pci_free_consistent (pdev, RX_TOTAL_SIZE, np->rx_ring,
				     np->rx_ring_dma);
		pci_free_consistent (pdev, TX_TOTAL_SIZE, np->tx_ring,
				     np->tx_ring_dma);
#ifdef MEM_MAPPING
		iounmap ((char *) (dev->base_addr));
#endif
		free_netdev (dev);
		pci_release_regions (pdev);
		pci_disable_device (pdev);
	}
	pci_set_drvdata (pdev, NULL);
}

static struct pci_driver rio_driver = {
	.name		= "dl2k",
	.id_table	= rio_pci_tbl,
	.probe		= rio_probe1,
	.remove		= __devexit_p(rio_remove1),
};

static int __init
rio_init (void)
{
	return pci_register_driver(&rio_driver);
}

static void __exit
rio_exit (void)
{
	pci_unregister_driver (&rio_driver);
}

module_init (rio_init);
module_exit (rio_exit);


