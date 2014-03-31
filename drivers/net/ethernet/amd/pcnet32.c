/*
 *	Copyright 1996-1999 Thomas Bogendoerfer
 *
 *	Derived from the lance driver written 1993,1994,1995 by Donald Becker.
 *
 *	Copyright 1993 United States Government as represented by the
 *	Director, National Security Agency.
 *
 *	This software may be used and distributed according to the terms
 *	of the GNU General Public License, incorporated herein by reference.
 *
 *	This driver is for PCnet32 and PCnetPCI based ethercards
 */
/**************************************************************************
 *  23 Oct, 2000.
 *  Fixed a few bugs, related to running the controller in 32bit mode.
 *
 *  Carsten Langgaard, carstenl@mips.com
 *  Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 *
 *************************************************************************/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define DRV_NAME	"pcnet32"
#define DRV_VERSION	"1.35"
#define DRV_RELDATE	"21.Apr.2008"
#define PFX		DRV_NAME ": "

static const char *const version =
    DRV_NAME ".c:v" DRV_VERSION " " DRV_RELDATE " tsbogend@alpha.franken.de\n";

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/crc32.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/moduleparam.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#include <asm/dma.h>
#include <asm/irq.h>

static DEFINE_PCI_DEVICE_TABLE(pcnet32_pci_tbl) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_LANCE_HOME), },
	{ PCI_DEVICE(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_LANCE), },

	{ PCI_DEVICE(PCI_VENDOR_ID_TRIDENT, PCI_DEVICE_ID_AMD_LANCE),
	  .class = (PCI_CLASS_NETWORK_ETHERNET << 8), .class_mask = 0xffff00, },

	{ }	
};

MODULE_DEVICE_TABLE(pci, pcnet32_pci_tbl);

static int cards_found;

static unsigned int pcnet32_portlist[] =
    { 0x300, 0x320, 0x340, 0x360, 0 };

static int pcnet32_debug;
static int tx_start = 1;	
static int pcnet32vlb;		

static struct net_device *pcnet32_dev;

static int max_interrupt_work = 2;
static int rx_copybreak = 200;

#define PCNET32_PORT_AUI      0x00
#define PCNET32_PORT_10BT     0x01
#define PCNET32_PORT_GPSI     0x02
#define PCNET32_PORT_MII      0x03

#define PCNET32_PORT_PORTSEL  0x03
#define PCNET32_PORT_ASEL     0x04
#define PCNET32_PORT_100      0x40
#define PCNET32_PORT_FD	      0x80

#define PCNET32_DMA_MASK 0xffffffff

#define PCNET32_WATCHDOG_TIMEOUT (jiffies + (2 * HZ))
#define PCNET32_BLINK_TIMEOUT	(jiffies + (HZ/4))

static const unsigned char options_mapping[] = {
	PCNET32_PORT_ASEL,			
	PCNET32_PORT_AUI,			
	PCNET32_PORT_AUI,			
	PCNET32_PORT_ASEL,			
	PCNET32_PORT_10BT | PCNET32_PORT_FD,	
	PCNET32_PORT_ASEL,			
	PCNET32_PORT_ASEL,			
	PCNET32_PORT_ASEL,			
	PCNET32_PORT_ASEL,			
	PCNET32_PORT_MII,			
	PCNET32_PORT_MII | PCNET32_PORT_FD,	
	PCNET32_PORT_MII,			
	PCNET32_PORT_10BT,			
	PCNET32_PORT_MII | PCNET32_PORT_100,	
						
	PCNET32_PORT_MII | PCNET32_PORT_100 | PCNET32_PORT_FD,
	PCNET32_PORT_ASEL			
};

static const char pcnet32_gstrings_test[][ETH_GSTRING_LEN] = {
	"Loopback test  (offline)"
};

#define PCNET32_TEST_LEN	ARRAY_SIZE(pcnet32_gstrings_test)

#define PCNET32_NUM_REGS 136

#define MAX_UNITS 8		
static int options[MAX_UNITS];
static int full_duplex[MAX_UNITS];
static int homepna[MAX_UNITS];


#ifndef PCNET32_LOG_TX_BUFFERS
#define PCNET32_LOG_TX_BUFFERS		4
#define PCNET32_LOG_RX_BUFFERS		5
#define PCNET32_LOG_MAX_TX_BUFFERS	9	
#define PCNET32_LOG_MAX_RX_BUFFERS	9
#endif

#define TX_RING_SIZE		(1 << (PCNET32_LOG_TX_BUFFERS))
#define TX_MAX_RING_SIZE	(1 << (PCNET32_LOG_MAX_TX_BUFFERS))

#define RX_RING_SIZE		(1 << (PCNET32_LOG_RX_BUFFERS))
#define RX_MAX_RING_SIZE	(1 << (PCNET32_LOG_MAX_RX_BUFFERS))

#define PKT_BUF_SKB		1544
#define PKT_BUF_SIZE		(PKT_BUF_SKB - NET_IP_ALIGN)
#define NEG_BUF_SIZE		(NET_IP_ALIGN - PKT_BUF_SKB)

#define PCNET32_WIO_RDP		0x10
#define PCNET32_WIO_RAP		0x12
#define PCNET32_WIO_RESET	0x14
#define PCNET32_WIO_BDP		0x16

#define PCNET32_DWIO_RDP	0x10
#define PCNET32_DWIO_RAP	0x14
#define PCNET32_DWIO_RESET	0x18
#define PCNET32_DWIO_BDP	0x1C

#define PCNET32_TOTAL_SIZE	0x20

#define CSR0		0
#define CSR0_INIT	0x1
#define CSR0_START	0x2
#define CSR0_STOP	0x4
#define CSR0_TXPOLL	0x8
#define CSR0_INTEN	0x40
#define CSR0_IDON	0x0100
#define CSR0_NORMAL	(CSR0_START | CSR0_INTEN)
#define PCNET32_INIT_LOW	1
#define PCNET32_INIT_HIGH	2
#define CSR3		3
#define CSR4		4
#define CSR5		5
#define CSR5_SUSPEND	0x0001
#define CSR15		15
#define PCNET32_MC_FILTER	8

#define PCNET32_79C970A	0x2621

struct pcnet32_rx_head {
	__le32	base;
	__le16	buf_length;	
	__le16	status;
	__le32	msg_length;
	__le32	reserved;
};

struct pcnet32_tx_head {
	__le32	base;
	__le16	length;		
	__le16	status;
	__le32	misc;
	__le32	reserved;
};

struct pcnet32_init_block {
	__le16	mode;
	__le16	tlen_rlen;
	u8	phys_addr[6];
	__le16	reserved;
	__le32	filter[2];
	
	__le32	rx_ring;
	__le32	tx_ring;
};

struct pcnet32_access {
	u16	(*read_csr) (unsigned long, int);
	void	(*write_csr) (unsigned long, int, u16);
	u16	(*read_bcr) (unsigned long, int);
	void	(*write_bcr) (unsigned long, int, u16);
	u16	(*read_rap) (unsigned long);
	void	(*write_rap) (unsigned long, u16);
	void	(*reset) (unsigned long);
};

struct pcnet32_private {
	struct pcnet32_init_block *init_block;
	
	struct pcnet32_rx_head	*rx_ring;
	struct pcnet32_tx_head	*tx_ring;
	dma_addr_t		init_dma_addr;
	struct pci_dev		*pci_dev;
	const char		*name;
	
	struct sk_buff		**tx_skbuff;
	struct sk_buff		**rx_skbuff;
	dma_addr_t		*tx_dma_addr;
	dma_addr_t		*rx_dma_addr;
	const struct pcnet32_access *a;
	spinlock_t		lock;		
	unsigned int		cur_rx, cur_tx;	
	unsigned int		rx_ring_size;	
	unsigned int		tx_ring_size;	
	unsigned int		rx_mod_mask;	
	unsigned int		tx_mod_mask;	
	unsigned short		rx_len_bits;
	unsigned short		tx_len_bits;
	dma_addr_t		rx_ring_dma_addr;
	dma_addr_t		tx_ring_dma_addr;
	unsigned int		dirty_rx,	
				dirty_tx;

	struct net_device	*dev;
	struct napi_struct	napi;
	char			tx_full;
	char			phycount;	
	int			options;
	unsigned int		shared_irq:1,	
				dxsuflo:1,   
				mii:1;		
	struct net_device	*next;
	struct mii_if_info	mii_if;
	struct timer_list	watchdog_timer;
	u32			msg_enable;	

	
	u32			phymask;
	unsigned short		chip_version;	

	
	u16 			save_regs[4];
};

static int pcnet32_probe_pci(struct pci_dev *, const struct pci_device_id *);
static int pcnet32_probe1(unsigned long, int, struct pci_dev *);
static int pcnet32_open(struct net_device *);
static int pcnet32_init_ring(struct net_device *);
static netdev_tx_t pcnet32_start_xmit(struct sk_buff *,
				      struct net_device *);
static void pcnet32_tx_timeout(struct net_device *dev);
static irqreturn_t pcnet32_interrupt(int, void *);
static int pcnet32_close(struct net_device *);
static struct net_device_stats *pcnet32_get_stats(struct net_device *);
static void pcnet32_load_multicast(struct net_device *dev);
static void pcnet32_set_multicast_list(struct net_device *);
static int pcnet32_ioctl(struct net_device *, struct ifreq *, int);
static void pcnet32_watchdog(struct net_device *);
static int mdio_read(struct net_device *dev, int phy_id, int reg_num);
static void mdio_write(struct net_device *dev, int phy_id, int reg_num,
		       int val);
static void pcnet32_restart(struct net_device *dev, unsigned int csr0_bits);
static void pcnet32_ethtool_test(struct net_device *dev,
				 struct ethtool_test *eth_test, u64 * data);
static int pcnet32_loopback_test(struct net_device *dev, uint64_t * data1);
static int pcnet32_get_regs_len(struct net_device *dev);
static void pcnet32_get_regs(struct net_device *dev, struct ethtool_regs *regs,
			     void *ptr);
static void pcnet32_purge_tx_ring(struct net_device *dev);
static int pcnet32_alloc_ring(struct net_device *dev, const char *name);
static void pcnet32_free_ring(struct net_device *dev);
static void pcnet32_check_media(struct net_device *dev, int verbose);

static u16 pcnet32_wio_read_csr(unsigned long addr, int index)
{
	outw(index, addr + PCNET32_WIO_RAP);
	return inw(addr + PCNET32_WIO_RDP);
}

static void pcnet32_wio_write_csr(unsigned long addr, int index, u16 val)
{
	outw(index, addr + PCNET32_WIO_RAP);
	outw(val, addr + PCNET32_WIO_RDP);
}

static u16 pcnet32_wio_read_bcr(unsigned long addr, int index)
{
	outw(index, addr + PCNET32_WIO_RAP);
	return inw(addr + PCNET32_WIO_BDP);
}

static void pcnet32_wio_write_bcr(unsigned long addr, int index, u16 val)
{
	outw(index, addr + PCNET32_WIO_RAP);
	outw(val, addr + PCNET32_WIO_BDP);
}

static u16 pcnet32_wio_read_rap(unsigned long addr)
{
	return inw(addr + PCNET32_WIO_RAP);
}

static void pcnet32_wio_write_rap(unsigned long addr, u16 val)
{
	outw(val, addr + PCNET32_WIO_RAP);
}

static void pcnet32_wio_reset(unsigned long addr)
{
	inw(addr + PCNET32_WIO_RESET);
}

static int pcnet32_wio_check(unsigned long addr)
{
	outw(88, addr + PCNET32_WIO_RAP);
	return inw(addr + PCNET32_WIO_RAP) == 88;
}

static const struct pcnet32_access pcnet32_wio = {
	.read_csr = pcnet32_wio_read_csr,
	.write_csr = pcnet32_wio_write_csr,
	.read_bcr = pcnet32_wio_read_bcr,
	.write_bcr = pcnet32_wio_write_bcr,
	.read_rap = pcnet32_wio_read_rap,
	.write_rap = pcnet32_wio_write_rap,
	.reset = pcnet32_wio_reset
};

static u16 pcnet32_dwio_read_csr(unsigned long addr, int index)
{
	outl(index, addr + PCNET32_DWIO_RAP);
	return inl(addr + PCNET32_DWIO_RDP) & 0xffff;
}

static void pcnet32_dwio_write_csr(unsigned long addr, int index, u16 val)
{
	outl(index, addr + PCNET32_DWIO_RAP);
	outl(val, addr + PCNET32_DWIO_RDP);
}

static u16 pcnet32_dwio_read_bcr(unsigned long addr, int index)
{
	outl(index, addr + PCNET32_DWIO_RAP);
	return inl(addr + PCNET32_DWIO_BDP) & 0xffff;
}

static void pcnet32_dwio_write_bcr(unsigned long addr, int index, u16 val)
{
	outl(index, addr + PCNET32_DWIO_RAP);
	outl(val, addr + PCNET32_DWIO_BDP);
}

static u16 pcnet32_dwio_read_rap(unsigned long addr)
{
	return inl(addr + PCNET32_DWIO_RAP) & 0xffff;
}

static void pcnet32_dwio_write_rap(unsigned long addr, u16 val)
{
	outl(val, addr + PCNET32_DWIO_RAP);
}

static void pcnet32_dwio_reset(unsigned long addr)
{
	inl(addr + PCNET32_DWIO_RESET);
}

static int pcnet32_dwio_check(unsigned long addr)
{
	outl(88, addr + PCNET32_DWIO_RAP);
	return (inl(addr + PCNET32_DWIO_RAP) & 0xffff) == 88;
}

static const struct pcnet32_access pcnet32_dwio = {
	.read_csr = pcnet32_dwio_read_csr,
	.write_csr = pcnet32_dwio_write_csr,
	.read_bcr = pcnet32_dwio_read_bcr,
	.write_bcr = pcnet32_dwio_write_bcr,
	.read_rap = pcnet32_dwio_read_rap,
	.write_rap = pcnet32_dwio_write_rap,
	.reset = pcnet32_dwio_reset
};

static void pcnet32_netif_stop(struct net_device *dev)
{
	struct pcnet32_private *lp = netdev_priv(dev);

	dev->trans_start = jiffies; 
	napi_disable(&lp->napi);
	netif_tx_disable(dev);
}

static void pcnet32_netif_start(struct net_device *dev)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	ulong ioaddr = dev->base_addr;
	u16 val;

	netif_wake_queue(dev);
	val = lp->a->read_csr(ioaddr, CSR3);
	val &= 0x00ff;
	lp->a->write_csr(ioaddr, CSR3, val);
	napi_enable(&lp->napi);
}

static void pcnet32_realloc_tx_ring(struct net_device *dev,
				    struct pcnet32_private *lp,
				    unsigned int size)
{
	dma_addr_t new_ring_dma_addr;
	dma_addr_t *new_dma_addr_list;
	struct pcnet32_tx_head *new_tx_ring;
	struct sk_buff **new_skb_list;

	pcnet32_purge_tx_ring(dev);

	new_tx_ring = pci_alloc_consistent(lp->pci_dev,
					   sizeof(struct pcnet32_tx_head) *
					   (1 << size),
					   &new_ring_dma_addr);
	if (new_tx_ring == NULL) {
		netif_err(lp, drv, dev, "Consistent memory allocation failed\n");
		return;
	}
	memset(new_tx_ring, 0, sizeof(struct pcnet32_tx_head) * (1 << size));

	new_dma_addr_list = kcalloc((1 << size), sizeof(dma_addr_t),
				GFP_ATOMIC);
	if (!new_dma_addr_list) {
		netif_err(lp, drv, dev, "Memory allocation failed\n");
		goto free_new_tx_ring;
	}

	new_skb_list = kcalloc((1 << size), sizeof(struct sk_buff *),
				GFP_ATOMIC);
	if (!new_skb_list) {
		netif_err(lp, drv, dev, "Memory allocation failed\n");
		goto free_new_lists;
	}

	kfree(lp->tx_skbuff);
	kfree(lp->tx_dma_addr);
	pci_free_consistent(lp->pci_dev,
			    sizeof(struct pcnet32_tx_head) *
			    lp->tx_ring_size, lp->tx_ring,
			    lp->tx_ring_dma_addr);

	lp->tx_ring_size = (1 << size);
	lp->tx_mod_mask = lp->tx_ring_size - 1;
	lp->tx_len_bits = (size << 12);
	lp->tx_ring = new_tx_ring;
	lp->tx_ring_dma_addr = new_ring_dma_addr;
	lp->tx_dma_addr = new_dma_addr_list;
	lp->tx_skbuff = new_skb_list;
	return;

free_new_lists:
	kfree(new_dma_addr_list);
free_new_tx_ring:
	pci_free_consistent(lp->pci_dev,
			    sizeof(struct pcnet32_tx_head) *
			    (1 << size),
			    new_tx_ring,
			    new_ring_dma_addr);
}

static void pcnet32_realloc_rx_ring(struct net_device *dev,
				    struct pcnet32_private *lp,
				    unsigned int size)
{
	dma_addr_t new_ring_dma_addr;
	dma_addr_t *new_dma_addr_list;
	struct pcnet32_rx_head *new_rx_ring;
	struct sk_buff **new_skb_list;
	int new, overlap;

	new_rx_ring = pci_alloc_consistent(lp->pci_dev,
					   sizeof(struct pcnet32_rx_head) *
					   (1 << size),
					   &new_ring_dma_addr);
	if (new_rx_ring == NULL) {
		netif_err(lp, drv, dev, "Consistent memory allocation failed\n");
		return;
	}
	memset(new_rx_ring, 0, sizeof(struct pcnet32_rx_head) * (1 << size));

	new_dma_addr_list = kcalloc((1 << size), sizeof(dma_addr_t),
				GFP_ATOMIC);
	if (!new_dma_addr_list) {
		netif_err(lp, drv, dev, "Memory allocation failed\n");
		goto free_new_rx_ring;
	}

	new_skb_list = kcalloc((1 << size), sizeof(struct sk_buff *),
				GFP_ATOMIC);
	if (!new_skb_list) {
		netif_err(lp, drv, dev, "Memory allocation failed\n");
		goto free_new_lists;
	}

	
	overlap = min(size, lp->rx_ring_size);
	for (new = 0; new < overlap; new++) {
		new_rx_ring[new] = lp->rx_ring[new];
		new_dma_addr_list[new] = lp->rx_dma_addr[new];
		new_skb_list[new] = lp->rx_skbuff[new];
	}
	
	for (; new < size; new++) {
		struct sk_buff *rx_skbuff;
		new_skb_list[new] = netdev_alloc_skb(dev, PKT_BUF_SKB);
		rx_skbuff = new_skb_list[new];
		if (!rx_skbuff) {
			
			netif_err(lp, drv, dev, "%s netdev_alloc_skb failed\n",
				  __func__);
			goto free_all_new;
		}
		skb_reserve(rx_skbuff, NET_IP_ALIGN);

		new_dma_addr_list[new] =
			    pci_map_single(lp->pci_dev, rx_skbuff->data,
					   PKT_BUF_SIZE, PCI_DMA_FROMDEVICE);
		new_rx_ring[new].base = cpu_to_le32(new_dma_addr_list[new]);
		new_rx_ring[new].buf_length = cpu_to_le16(NEG_BUF_SIZE);
		new_rx_ring[new].status = cpu_to_le16(0x8000);
	}
	
	for (; new < lp->rx_ring_size; new++) {
		if (lp->rx_skbuff[new]) {
			pci_unmap_single(lp->pci_dev, lp->rx_dma_addr[new],
					 PKT_BUF_SIZE, PCI_DMA_FROMDEVICE);
			dev_kfree_skb(lp->rx_skbuff[new]);
		}
	}

	kfree(lp->rx_skbuff);
	kfree(lp->rx_dma_addr);
	pci_free_consistent(lp->pci_dev,
			    sizeof(struct pcnet32_rx_head) *
			    lp->rx_ring_size, lp->rx_ring,
			    lp->rx_ring_dma_addr);

	lp->rx_ring_size = (1 << size);
	lp->rx_mod_mask = lp->rx_ring_size - 1;
	lp->rx_len_bits = (size << 4);
	lp->rx_ring = new_rx_ring;
	lp->rx_ring_dma_addr = new_ring_dma_addr;
	lp->rx_dma_addr = new_dma_addr_list;
	lp->rx_skbuff = new_skb_list;
	return;

free_all_new:
	while (--new >= lp->rx_ring_size) {
		if (new_skb_list[new]) {
			pci_unmap_single(lp->pci_dev, new_dma_addr_list[new],
					 PKT_BUF_SIZE, PCI_DMA_FROMDEVICE);
			dev_kfree_skb(new_skb_list[new]);
		}
	}
	kfree(new_skb_list);
free_new_lists:
	kfree(new_dma_addr_list);
free_new_rx_ring:
	pci_free_consistent(lp->pci_dev,
			    sizeof(struct pcnet32_rx_head) *
			    (1 << size),
			    new_rx_ring,
			    new_ring_dma_addr);
}

static void pcnet32_purge_rx_ring(struct net_device *dev)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	int i;

	
	for (i = 0; i < lp->rx_ring_size; i++) {
		lp->rx_ring[i].status = 0;	
		wmb();		
		if (lp->rx_skbuff[i]) {
			pci_unmap_single(lp->pci_dev, lp->rx_dma_addr[i],
					 PKT_BUF_SIZE, PCI_DMA_FROMDEVICE);
			dev_kfree_skb_any(lp->rx_skbuff[i]);
		}
		lp->rx_skbuff[i] = NULL;
		lp->rx_dma_addr[i] = 0;
	}
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void pcnet32_poll_controller(struct net_device *dev)
{
	disable_irq(dev->irq);
	pcnet32_interrupt(0, dev);
	enable_irq(dev->irq);
}
#endif

static int pcnet32_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	unsigned long flags;
	int r = -EOPNOTSUPP;

	if (lp->mii) {
		spin_lock_irqsave(&lp->lock, flags);
		mii_ethtool_gset(&lp->mii_if, cmd);
		spin_unlock_irqrestore(&lp->lock, flags);
		r = 0;
	}
	return r;
}

static int pcnet32_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	unsigned long flags;
	int r = -EOPNOTSUPP;

	if (lp->mii) {
		spin_lock_irqsave(&lp->lock, flags);
		r = mii_ethtool_sset(&lp->mii_if, cmd);
		spin_unlock_irqrestore(&lp->lock, flags);
	}
	return r;
}

static void pcnet32_get_drvinfo(struct net_device *dev,
				struct ethtool_drvinfo *info)
{
	struct pcnet32_private *lp = netdev_priv(dev);

	strlcpy(info->driver, DRV_NAME, sizeof(info->driver));
	strlcpy(info->version, DRV_VERSION, sizeof(info->version));
	if (lp->pci_dev)
		strlcpy(info->bus_info, pci_name(lp->pci_dev),
			sizeof(info->bus_info));
	else
		snprintf(info->bus_info, sizeof(info->bus_info),
			"VLB 0x%lx", dev->base_addr);
}

static u32 pcnet32_get_link(struct net_device *dev)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	unsigned long flags;
	int r;

	spin_lock_irqsave(&lp->lock, flags);
	if (lp->mii) {
		r = mii_link_ok(&lp->mii_if);
	} else if (lp->chip_version >= PCNET32_79C970A) {
		ulong ioaddr = dev->base_addr;	
		r = (lp->a->read_bcr(ioaddr, 4) != 0xc0);
	} else {	
		r = 1;
	}
	spin_unlock_irqrestore(&lp->lock, flags);

	return r;
}

static u32 pcnet32_get_msglevel(struct net_device *dev)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	return lp->msg_enable;
}

static void pcnet32_set_msglevel(struct net_device *dev, u32 value)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	lp->msg_enable = value;
}

static int pcnet32_nway_reset(struct net_device *dev)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	unsigned long flags;
	int r = -EOPNOTSUPP;

	if (lp->mii) {
		spin_lock_irqsave(&lp->lock, flags);
		r = mii_nway_restart(&lp->mii_if);
		spin_unlock_irqrestore(&lp->lock, flags);
	}
	return r;
}

static void pcnet32_get_ringparam(struct net_device *dev,
				  struct ethtool_ringparam *ering)
{
	struct pcnet32_private *lp = netdev_priv(dev);

	ering->tx_max_pending = TX_MAX_RING_SIZE;
	ering->tx_pending = lp->tx_ring_size;
	ering->rx_max_pending = RX_MAX_RING_SIZE;
	ering->rx_pending = lp->rx_ring_size;
}

static int pcnet32_set_ringparam(struct net_device *dev,
				 struct ethtool_ringparam *ering)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	unsigned long flags;
	unsigned int size;
	ulong ioaddr = dev->base_addr;
	int i;

	if (ering->rx_mini_pending || ering->rx_jumbo_pending)
		return -EINVAL;

	if (netif_running(dev))
		pcnet32_netif_stop(dev);

	spin_lock_irqsave(&lp->lock, flags);
	lp->a->write_csr(ioaddr, CSR0, CSR0_STOP);	

	size = min(ering->tx_pending, (unsigned int)TX_MAX_RING_SIZE);

	for (i = 2; i <= PCNET32_LOG_MAX_TX_BUFFERS; i++) {
		if (size <= (1 << i))
			break;
	}
	if ((1 << i) != lp->tx_ring_size)
		pcnet32_realloc_tx_ring(dev, lp, i);

	size = min(ering->rx_pending, (unsigned int)RX_MAX_RING_SIZE);
	for (i = 2; i <= PCNET32_LOG_MAX_RX_BUFFERS; i++) {
		if (size <= (1 << i))
			break;
	}
	if ((1 << i) != lp->rx_ring_size)
		pcnet32_realloc_rx_ring(dev, lp, i);

	lp->napi.weight = lp->rx_ring_size / 2;

	if (netif_running(dev)) {
		pcnet32_netif_start(dev);
		pcnet32_restart(dev, CSR0_NORMAL);
	}

	spin_unlock_irqrestore(&lp->lock, flags);

	netif_info(lp, drv, dev, "Ring Param Settings: RX: %d, TX: %d\n",
		   lp->rx_ring_size, lp->tx_ring_size);

	return 0;
}

static void pcnet32_get_strings(struct net_device *dev, u32 stringset,
				u8 *data)
{
	memcpy(data, pcnet32_gstrings_test, sizeof(pcnet32_gstrings_test));
}

static int pcnet32_get_sset_count(struct net_device *dev, int sset)
{
	switch (sset) {
	case ETH_SS_TEST:
		return PCNET32_TEST_LEN;
	default:
		return -EOPNOTSUPP;
	}
}

static void pcnet32_ethtool_test(struct net_device *dev,
				 struct ethtool_test *test, u64 * data)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	int rc;

	if (test->flags == ETH_TEST_FL_OFFLINE) {
		rc = pcnet32_loopback_test(dev, data);
		if (rc) {
			netif_printk(lp, hw, KERN_DEBUG, dev,
				     "Loopback test failed\n");
			test->flags |= ETH_TEST_FL_FAILED;
		} else
			netif_printk(lp, hw, KERN_DEBUG, dev,
				     "Loopback test passed\n");
	} else
		netif_printk(lp, hw, KERN_DEBUG, dev,
			     "No tests to run (specify 'Offline' on ethtool)\n");
}				

static int pcnet32_loopback_test(struct net_device *dev, uint64_t * data1)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	const struct pcnet32_access *a = lp->a;	
	ulong ioaddr = dev->base_addr;	
	struct sk_buff *skb;	
	int x, i;		
	int numbuffs = 4;	
	u16 status = 0x8300;	
	__le16 teststatus;	
	int rc;			
	int size;		
	unsigned char *packet;	
	static const int data_len = 60;	
	unsigned long flags;
	unsigned long ticks;

	rc = 1;			

	if (netif_running(dev))
		pcnet32_netif_stop(dev);

	spin_lock_irqsave(&lp->lock, flags);
	lp->a->write_csr(ioaddr, CSR0, CSR0_STOP);	

	numbuffs = min(numbuffs, (int)min(lp->rx_ring_size, lp->tx_ring_size));

	
	lp->a->reset(ioaddr);
	lp->a->write_csr(ioaddr, CSR4, 0x0915);	

	
	lp->a->write_bcr(ioaddr, 20, 2);

	
	pcnet32_restart(dev, 0x0000);

	lp->a->write_csr(ioaddr, CSR0, CSR0_STOP);	

	
	size = data_len + 15;
	for (x = 0; x < numbuffs; x++) {
		skb = netdev_alloc_skb(dev, size);
		if (!skb) {
			netif_printk(lp, hw, KERN_DEBUG, dev,
				     "Cannot allocate skb at line: %d!\n",
				     __LINE__);
			goto clean_up;
		}
		packet = skb->data;
		skb_put(skb, size);	
		lp->tx_skbuff[x] = skb;
		lp->tx_ring[x].length = cpu_to_le16(-skb->len);
		lp->tx_ring[x].misc = 0;

		
		for (i = 0; i < 6; i++)
			*packet++ = dev->dev_addr[i];
		for (i = 0; i < 6; i++)
			*packet++ = dev->dev_addr[i];
		
		*packet++ = 0x08;
		*packet++ = 0x06;
		
		*packet++ = x;
		
		for (i = 0; i < data_len; i++)
			*packet++ = i;

		lp->tx_dma_addr[x] =
			pci_map_single(lp->pci_dev, skb->data, skb->len,
				       PCI_DMA_TODEVICE);
		lp->tx_ring[x].base = cpu_to_le32(lp->tx_dma_addr[x]);
		wmb();	
		lp->tx_ring[x].status = cpu_to_le16(status);
	}

	x = a->read_bcr(ioaddr, 32);	
	a->write_bcr(ioaddr, 32, x | 0x0002);

	
	x = a->read_csr(ioaddr, CSR15) & 0xfffc;
	lp->a->write_csr(ioaddr, CSR15, x | 0x0044);

	teststatus = cpu_to_le16(0x8000);
	lp->a->write_csr(ioaddr, CSR0, CSR0_START);	

	
	for (x = 0; x < numbuffs; x++) {
		ticks = 0;
		rmb();
		while ((lp->rx_ring[x].status & teststatus) && (ticks < 200)) {
			spin_unlock_irqrestore(&lp->lock, flags);
			msleep(1);
			spin_lock_irqsave(&lp->lock, flags);
			rmb();
			ticks++;
		}
		if (ticks == 200) {
			netif_err(lp, hw, dev, "Desc %d failed to reset!\n", x);
			break;
		}
	}

	lp->a->write_csr(ioaddr, CSR0, CSR0_STOP);	
	wmb();
	if (netif_msg_hw(lp) && netif_msg_pktdata(lp)) {
		netdev_printk(KERN_DEBUG, dev, "RX loopback packets:\n");

		for (x = 0; x < numbuffs; x++) {
			netdev_printk(KERN_DEBUG, dev, "Packet %d: ", x);
			skb = lp->rx_skbuff[x];
			for (i = 0; i < size; i++)
				pr_cont(" %02x", *(skb->data + i));
			pr_cont("\n");
		}
	}

	x = 0;
	rc = 0;
	while (x < numbuffs && !rc) {
		skb = lp->rx_skbuff[x];
		packet = lp->tx_skbuff[x]->data;
		for (i = 0; i < size; i++) {
			if (*(skb->data + i) != packet[i]) {
				netif_printk(lp, hw, KERN_DEBUG, dev,
					     "Error in compare! %2x - %02x %02x\n",
					     i, *(skb->data + i), packet[i]);
				rc = 1;
				break;
			}
		}
		x++;
	}

clean_up:
	*data1 = rc;
	pcnet32_purge_tx_ring(dev);

	x = a->read_csr(ioaddr, CSR15);
	a->write_csr(ioaddr, CSR15, (x & ~0x0044));	

	x = a->read_bcr(ioaddr, 32);	
	a->write_bcr(ioaddr, 32, (x & ~0x0002));

	if (netif_running(dev)) {
		pcnet32_netif_start(dev);
		pcnet32_restart(dev, CSR0_NORMAL);
	} else {
		pcnet32_purge_rx_ring(dev);
		lp->a->write_bcr(ioaddr, 20, 4);	
	}
	spin_unlock_irqrestore(&lp->lock, flags);

	return rc;
}				

static int pcnet32_set_phys_id(struct net_device *dev,
			       enum ethtool_phys_id_state state)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	const struct pcnet32_access *a = lp->a;
	ulong ioaddr = dev->base_addr;
	unsigned long flags;
	int i;

	switch (state) {
	case ETHTOOL_ID_ACTIVE:
		
		spin_lock_irqsave(&lp->lock, flags);
		for (i = 4; i < 8; i++)
			lp->save_regs[i - 4] = a->read_bcr(ioaddr, i);
		spin_unlock_irqrestore(&lp->lock, flags);
		return 2;	

	case ETHTOOL_ID_ON:
	case ETHTOOL_ID_OFF:
		
		spin_lock_irqsave(&lp->lock, flags);
		for (i = 4; i < 8; i++)
			a->write_bcr(ioaddr, i, a->read_bcr(ioaddr, i) ^ 0x4000);
		spin_unlock_irqrestore(&lp->lock, flags);
		break;

	case ETHTOOL_ID_INACTIVE:
		
		spin_lock_irqsave(&lp->lock, flags);
		for (i = 4; i < 8; i++)
			a->write_bcr(ioaddr, i, lp->save_regs[i - 4]);
		spin_unlock_irqrestore(&lp->lock, flags);
	}
	return 0;
}

static int pcnet32_suspend(struct net_device *dev, unsigned long *flags,
		int can_sleep)
{
	int csr5;
	struct pcnet32_private *lp = netdev_priv(dev);
	const struct pcnet32_access *a = lp->a;
	ulong ioaddr = dev->base_addr;
	int ticks;

	
	if (lp->chip_version < PCNET32_79C970A)
		return 0;

	
	csr5 = a->read_csr(ioaddr, CSR5);
	a->write_csr(ioaddr, CSR5, csr5 | CSR5_SUSPEND);

	
	ticks = 0;
	while (!(a->read_csr(ioaddr, CSR5) & CSR5_SUSPEND)) {
		spin_unlock_irqrestore(&lp->lock, *flags);
		if (can_sleep)
			msleep(1);
		else
			mdelay(1);
		spin_lock_irqsave(&lp->lock, *flags);
		ticks++;
		if (ticks > 200) {
			netif_printk(lp, hw, KERN_DEBUG, dev,
				     "Error getting into suspend!\n");
			return 0;
		}
	}
	return 1;
}


static void pcnet32_rx_entry(struct net_device *dev,
			     struct pcnet32_private *lp,
			     struct pcnet32_rx_head *rxp,
			     int entry)
{
	int status = (short)le16_to_cpu(rxp->status) >> 8;
	int rx_in_place = 0;
	struct sk_buff *skb;
	short pkt_len;

	if (status != 0x03) {	
		if (status & 0x01)	
			dev->stats.rx_errors++;	
		if (status & 0x20)
			dev->stats.rx_frame_errors++;
		if (status & 0x10)
			dev->stats.rx_over_errors++;
		if (status & 0x08)
			dev->stats.rx_crc_errors++;
		if (status & 0x04)
			dev->stats.rx_fifo_errors++;
		return;
	}

	pkt_len = (le32_to_cpu(rxp->msg_length) & 0xfff) - 4;

	
	if (unlikely(pkt_len > PKT_BUF_SIZE)) {
		netif_err(lp, drv, dev, "Impossible packet size %d!\n",
			  pkt_len);
		dev->stats.rx_errors++;
		return;
	}
	if (pkt_len < 60) {
		netif_err(lp, rx_err, dev, "Runt packet!\n");
		dev->stats.rx_errors++;
		return;
	}

	if (pkt_len > rx_copybreak) {
		struct sk_buff *newskb;

		newskb = netdev_alloc_skb(dev, PKT_BUF_SKB);
		if (newskb) {
			skb_reserve(newskb, NET_IP_ALIGN);
			skb = lp->rx_skbuff[entry];
			pci_unmap_single(lp->pci_dev,
					 lp->rx_dma_addr[entry],
					 PKT_BUF_SIZE,
					 PCI_DMA_FROMDEVICE);
			skb_put(skb, pkt_len);
			lp->rx_skbuff[entry] = newskb;
			lp->rx_dma_addr[entry] =
					    pci_map_single(lp->pci_dev,
							   newskb->data,
							   PKT_BUF_SIZE,
							   PCI_DMA_FROMDEVICE);
			rxp->base = cpu_to_le32(lp->rx_dma_addr[entry]);
			rx_in_place = 1;
		} else
			skb = NULL;
	} else
		skb = netdev_alloc_skb(dev, pkt_len + NET_IP_ALIGN);

	if (skb == NULL) {
		netif_err(lp, drv, dev, "Memory squeeze, dropping packet\n");
		dev->stats.rx_dropped++;
		return;
	}
	if (!rx_in_place) {
		skb_reserve(skb, NET_IP_ALIGN);
		skb_put(skb, pkt_len);	
		pci_dma_sync_single_for_cpu(lp->pci_dev,
					    lp->rx_dma_addr[entry],
					    pkt_len,
					    PCI_DMA_FROMDEVICE);
		skb_copy_to_linear_data(skb,
				 (unsigned char *)(lp->rx_skbuff[entry]->data),
				 pkt_len);
		pci_dma_sync_single_for_device(lp->pci_dev,
					       lp->rx_dma_addr[entry],
					       pkt_len,
					       PCI_DMA_FROMDEVICE);
	}
	dev->stats.rx_bytes += skb->len;
	skb->protocol = eth_type_trans(skb, dev);
	netif_receive_skb(skb);
	dev->stats.rx_packets++;
}

static int pcnet32_rx(struct net_device *dev, int budget)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	int entry = lp->cur_rx & lp->rx_mod_mask;
	struct pcnet32_rx_head *rxp = &lp->rx_ring[entry];
	int npackets = 0;

	
	while (npackets < budget && (short)le16_to_cpu(rxp->status) >= 0) {
		pcnet32_rx_entry(dev, lp, rxp, entry);
		npackets += 1;
		rxp->buf_length = cpu_to_le16(NEG_BUF_SIZE);
		wmb();	
		rxp->status = cpu_to_le16(0x8000);
		entry = (++lp->cur_rx) & lp->rx_mod_mask;
		rxp = &lp->rx_ring[entry];
	}

	return npackets;
}

static int pcnet32_tx(struct net_device *dev)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	unsigned int dirty_tx = lp->dirty_tx;
	int delta;
	int must_restart = 0;

	while (dirty_tx != lp->cur_tx) {
		int entry = dirty_tx & lp->tx_mod_mask;
		int status = (short)le16_to_cpu(lp->tx_ring[entry].status);

		if (status < 0)
			break;	

		lp->tx_ring[entry].base = 0;

		if (status & 0x4000) {
			
			int err_status = le32_to_cpu(lp->tx_ring[entry].misc);
			dev->stats.tx_errors++;
			netif_err(lp, tx_err, dev,
				  "Tx error status=%04x err_status=%08x\n",
				  status, err_status);
			if (err_status & 0x04000000)
				dev->stats.tx_aborted_errors++;
			if (err_status & 0x08000000)
				dev->stats.tx_carrier_errors++;
			if (err_status & 0x10000000)
				dev->stats.tx_window_errors++;
#ifndef DO_DXSUFLO
			if (err_status & 0x40000000) {
				dev->stats.tx_fifo_errors++;
				
				
				netif_err(lp, tx_err, dev, "Tx FIFO error!\n");
				must_restart = 1;
			}
#else
			if (err_status & 0x40000000) {
				dev->stats.tx_fifo_errors++;
				if (!lp->dxsuflo) {	
					
					
					netif_err(lp, tx_err, dev, "Tx FIFO error!\n");
					must_restart = 1;
				}
			}
#endif
		} else {
			if (status & 0x1800)
				dev->stats.collisions++;
			dev->stats.tx_packets++;
		}

		
		if (lp->tx_skbuff[entry]) {
			pci_unmap_single(lp->pci_dev,
					 lp->tx_dma_addr[entry],
					 lp->tx_skbuff[entry]->
					 len, PCI_DMA_TODEVICE);
			dev_kfree_skb_any(lp->tx_skbuff[entry]);
			lp->tx_skbuff[entry] = NULL;
			lp->tx_dma_addr[entry] = 0;
		}
		dirty_tx++;
	}

	delta = (lp->cur_tx - dirty_tx) & (lp->tx_mod_mask + lp->tx_ring_size);
	if (delta > lp->tx_ring_size) {
		netif_err(lp, drv, dev, "out-of-sync dirty pointer, %d vs. %d, full=%d\n",
			  dirty_tx, lp->cur_tx, lp->tx_full);
		dirty_tx += lp->tx_ring_size;
		delta -= lp->tx_ring_size;
	}

	if (lp->tx_full &&
	    netif_queue_stopped(dev) &&
	    delta < lp->tx_ring_size - 2) {
		
		lp->tx_full = 0;
		netif_wake_queue(dev);
	}
	lp->dirty_tx = dirty_tx;

	return must_restart;
}

static int pcnet32_poll(struct napi_struct *napi, int budget)
{
	struct pcnet32_private *lp = container_of(napi, struct pcnet32_private, napi);
	struct net_device *dev = lp->dev;
	unsigned long ioaddr = dev->base_addr;
	unsigned long flags;
	int work_done;
	u16 val;

	work_done = pcnet32_rx(dev, budget);

	spin_lock_irqsave(&lp->lock, flags);
	if (pcnet32_tx(dev)) {
		
		lp->a->reset(ioaddr);
		lp->a->write_csr(ioaddr, CSR4, 0x0915);	
		pcnet32_restart(dev, CSR0_START);
		netif_wake_queue(dev);
	}
	spin_unlock_irqrestore(&lp->lock, flags);

	if (work_done < budget) {
		spin_lock_irqsave(&lp->lock, flags);

		__napi_complete(napi);

		
		val = lp->a->read_csr(ioaddr, CSR3);
		val &= 0x00ff;
		lp->a->write_csr(ioaddr, CSR3, val);

		
		lp->a->write_csr(ioaddr, CSR0, CSR0_INTEN);

		spin_unlock_irqrestore(&lp->lock, flags);
	}
	return work_done;
}

#define PCNET32_REGS_PER_PHY	32
#define PCNET32_MAX_PHYS	32
static int pcnet32_get_regs_len(struct net_device *dev)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	int j = lp->phycount * PCNET32_REGS_PER_PHY;

	return (PCNET32_NUM_REGS + j) * sizeof(u16);
}

static void pcnet32_get_regs(struct net_device *dev, struct ethtool_regs *regs,
			     void *ptr)
{
	int i, csr0;
	u16 *buff = ptr;
	struct pcnet32_private *lp = netdev_priv(dev);
	const struct pcnet32_access *a = lp->a;
	ulong ioaddr = dev->base_addr;
	unsigned long flags;

	spin_lock_irqsave(&lp->lock, flags);

	csr0 = a->read_csr(ioaddr, CSR0);
	if (!(csr0 & CSR0_STOP))	
		pcnet32_suspend(dev, &flags, 1);

	
	for (i = 0; i < 16; i += 2)
		*buff++ = inw(ioaddr + i);

	
	for (i = 0; i < 90; i++)
		*buff++ = a->read_csr(ioaddr, i);

	*buff++ = a->read_csr(ioaddr, 112);
	*buff++ = a->read_csr(ioaddr, 114);

	
	for (i = 0; i < 30; i++)
		*buff++ = a->read_bcr(ioaddr, i);

	*buff++ = 0;		

	for (i = 31; i < 36; i++)
		*buff++ = a->read_bcr(ioaddr, i);

	
	if (lp->mii) {
		int j;
		for (j = 0; j < PCNET32_MAX_PHYS; j++) {
			if (lp->phymask & (1 << j)) {
				for (i = 0; i < PCNET32_REGS_PER_PHY; i++) {
					lp->a->write_bcr(ioaddr, 33,
							(j << 5) | i);
					*buff++ = lp->a->read_bcr(ioaddr, 34);
				}
			}
		}
	}

	if (!(csr0 & CSR0_STOP)) {	
		int csr5;

		
		csr5 = a->read_csr(ioaddr, CSR5);
		a->write_csr(ioaddr, CSR5, csr5 & (~CSR5_SUSPEND));
	}

	spin_unlock_irqrestore(&lp->lock, flags);
}

static const struct ethtool_ops pcnet32_ethtool_ops = {
	.get_settings		= pcnet32_get_settings,
	.set_settings		= pcnet32_set_settings,
	.get_drvinfo		= pcnet32_get_drvinfo,
	.get_msglevel		= pcnet32_get_msglevel,
	.set_msglevel		= pcnet32_set_msglevel,
	.nway_reset		= pcnet32_nway_reset,
	.get_link		= pcnet32_get_link,
	.get_ringparam		= pcnet32_get_ringparam,
	.set_ringparam		= pcnet32_set_ringparam,
	.get_strings		= pcnet32_get_strings,
	.self_test		= pcnet32_ethtool_test,
	.set_phys_id		= pcnet32_set_phys_id,
	.get_regs_len		= pcnet32_get_regs_len,
	.get_regs		= pcnet32_get_regs,
	.get_sset_count		= pcnet32_get_sset_count,
};


static void __devinit pcnet32_probe_vlbus(unsigned int *pcnet32_portlist)
{
	unsigned int *port, ioaddr;

	
	for (port = pcnet32_portlist; (ioaddr = *port); port++) {
		if (request_region
		    (ioaddr, PCNET32_TOTAL_SIZE, "pcnet32_probe_vlbus")) {
			
			if ((inb(ioaddr + 14) == 0x57) &&
			    (inb(ioaddr + 15) == 0x57)) {
				pcnet32_probe1(ioaddr, 0, NULL);
			} else {
				release_region(ioaddr, PCNET32_TOTAL_SIZE);
			}
		}
	}
}

static int __devinit
pcnet32_probe_pci(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	unsigned long ioaddr;
	int err;

	err = pci_enable_device(pdev);
	if (err < 0) {
		if (pcnet32_debug & NETIF_MSG_PROBE)
			pr_err("failed to enable device -- err=%d\n", err);
		return err;
	}
	pci_set_master(pdev);

	ioaddr = pci_resource_start(pdev, 0);
	if (!ioaddr) {
		if (pcnet32_debug & NETIF_MSG_PROBE)
			pr_err("card has no PCI IO resources, aborting\n");
		return -ENODEV;
	}

	if (!pci_dma_supported(pdev, PCNET32_DMA_MASK)) {
		if (pcnet32_debug & NETIF_MSG_PROBE)
			pr_err("architecture does not support 32bit PCI busmaster DMA\n");
		return -ENODEV;
	}
	if (!request_region(ioaddr, PCNET32_TOTAL_SIZE, "pcnet32_probe_pci")) {
		if (pcnet32_debug & NETIF_MSG_PROBE)
			pr_err("io address range already allocated\n");
		return -EBUSY;
	}

	err = pcnet32_probe1(ioaddr, 1, pdev);
	if (err < 0)
		pci_disable_device(pdev);

	return err;
}

static const struct net_device_ops pcnet32_netdev_ops = {
	.ndo_open		= pcnet32_open,
	.ndo_stop 		= pcnet32_close,
	.ndo_start_xmit		= pcnet32_start_xmit,
	.ndo_tx_timeout		= pcnet32_tx_timeout,
	.ndo_get_stats		= pcnet32_get_stats,
	.ndo_set_rx_mode	= pcnet32_set_multicast_list,
	.ndo_do_ioctl		= pcnet32_ioctl,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= pcnet32_poll_controller,
#endif
};

static int __devinit
pcnet32_probe1(unsigned long ioaddr, int shared, struct pci_dev *pdev)
{
	struct pcnet32_private *lp;
	int i, media;
	int fdx, mii, fset, dxsuflo;
	int chip_version;
	char *chipname;
	struct net_device *dev;
	const struct pcnet32_access *a = NULL;
	u8 promaddr[6];
	int ret = -ENODEV;

	
	pcnet32_wio_reset(ioaddr);

	
	if (pcnet32_wio_read_csr(ioaddr, 0) == 4 && pcnet32_wio_check(ioaddr)) {
		a = &pcnet32_wio;
	} else {
		pcnet32_dwio_reset(ioaddr);
		if (pcnet32_dwio_read_csr(ioaddr, 0) == 4 &&
		    pcnet32_dwio_check(ioaddr)) {
			a = &pcnet32_dwio;
		} else {
			if (pcnet32_debug & NETIF_MSG_PROBE)
				pr_err("No access methods\n");
			goto err_release_region;
		}
	}

	chip_version =
	    a->read_csr(ioaddr, 88) | (a->read_csr(ioaddr, 89) << 16);
	if ((pcnet32_debug & NETIF_MSG_PROBE) && (pcnet32_debug & NETIF_MSG_HW))
		pr_info("  PCnet chip version is %#x\n", chip_version);
	if ((chip_version & 0xfff) != 0x003) {
		if (pcnet32_debug & NETIF_MSG_PROBE)
			pr_info("Unsupported chip version\n");
		goto err_release_region;
	}

	
	fdx = mii = fset = dxsuflo = 0;
	chip_version = (chip_version >> 12) & 0xffff;

	switch (chip_version) {
	case 0x2420:
		chipname = "PCnet/PCI 79C970";	
		break;
	case 0x2430:
		if (shared)
			chipname = "PCnet/PCI 79C970";	
		else
			chipname = "PCnet/32 79C965";	
		break;
	case 0x2621:
		chipname = "PCnet/PCI II 79C970A";	
		fdx = 1;
		break;
	case 0x2623:
		chipname = "PCnet/FAST 79C971";	
		fdx = 1;
		mii = 1;
		fset = 1;
		break;
	case 0x2624:
		chipname = "PCnet/FAST+ 79C972";	
		fdx = 1;
		mii = 1;
		fset = 1;
		break;
	case 0x2625:
		chipname = "PCnet/FAST III 79C973";	
		fdx = 1;
		mii = 1;
		break;
	case 0x2626:
		chipname = "PCnet/Home 79C978";	
		fdx = 1;
		media = a->read_bcr(ioaddr, 49);
		media &= ~3;	
		if (cards_found < MAX_UNITS && homepna[cards_found])
			media |= 1;	
		if (pcnet32_debug & NETIF_MSG_PROBE)
			printk(KERN_DEBUG PFX "media set to %sMbit mode\n",
			       (media & 1) ? "1" : "10");
		a->write_bcr(ioaddr, 49, media);
		break;
	case 0x2627:
		chipname = "PCnet/FAST III 79C975";	
		fdx = 1;
		mii = 1;
		break;
	case 0x2628:
		chipname = "PCnet/PRO 79C976";
		fdx = 1;
		mii = 1;
		break;
	default:
		if (pcnet32_debug & NETIF_MSG_PROBE)
			pr_info("PCnet version %#x, no PCnet32 chip\n",
				chip_version);
		goto err_release_region;
	}


	if (fset) {
		a->write_bcr(ioaddr, 18, (a->read_bcr(ioaddr, 18) | 0x0860));
		a->write_csr(ioaddr, 80,
			     (a->read_csr(ioaddr, 80) & 0x0C00) | 0x0c00);
		dxsuflo = 1;
	}

	dev = alloc_etherdev(sizeof(*lp));
	if (!dev) {
		ret = -ENOMEM;
		goto err_release_region;
	}

	if (pdev)
		SET_NETDEV_DEV(dev, &pdev->dev);

	if (pcnet32_debug & NETIF_MSG_PROBE)
		pr_info("%s at %#3lx,", chipname, ioaddr);

	for (i = 0; i < 3; i++) {
		unsigned int val;
		val = a->read_csr(ioaddr, i + 12) & 0x0ffff;
		
		dev->dev_addr[2 * i] = val & 0x0ff;
		dev->dev_addr[2 * i + 1] = (val >> 8) & 0x0ff;
	}

	
	for (i = 0; i < 6; i++)
		promaddr[i] = inb(ioaddr + i);

	if (memcmp(promaddr, dev->dev_addr, 6) ||
	    !is_valid_ether_addr(dev->dev_addr)) {
		if (is_valid_ether_addr(promaddr)) {
			if (pcnet32_debug & NETIF_MSG_PROBE) {
				pr_cont(" warning: CSR address invalid,\n");
				pr_info("    using instead PROM address of");
			}
			memcpy(dev->dev_addr, promaddr, 6);
		}
	}
	memcpy(dev->perm_addr, dev->dev_addr, dev->addr_len);

	
	if (!is_valid_ether_addr(dev->perm_addr))
		memset(dev->dev_addr, 0, ETH_ALEN);

	if (pcnet32_debug & NETIF_MSG_PROBE) {
		pr_cont(" %pM", dev->dev_addr);

		
		if (((chip_version + 1) & 0xfffe) == 0x2624) {
			i = a->read_csr(ioaddr, 80) & 0x0C00;	
			pr_info("    tx_start_pt(0x%04x):", i);
			switch (i >> 10) {
			case 0:
				pr_cont("  20 bytes,");
				break;
			case 1:
				pr_cont("  64 bytes,");
				break;
			case 2:
				pr_cont(" 128 bytes,");
				break;
			case 3:
				pr_cont("~220 bytes,");
				break;
			}
			i = a->read_bcr(ioaddr, 18);	
			pr_cont(" BCR18(%x):", i & 0xffff);
			if (i & (1 << 5))
				pr_cont("BurstWrEn ");
			if (i & (1 << 6))
				pr_cont("BurstRdEn ");
			if (i & (1 << 7))
				pr_cont("DWordIO ");
			if (i & (1 << 11))
				pr_cont("NoUFlow ");
			i = a->read_bcr(ioaddr, 25);
			pr_info("    SRAMSIZE=0x%04x,", i << 8);
			i = a->read_bcr(ioaddr, 26);
			pr_cont(" SRAM_BND=0x%04x,", i << 8);
			i = a->read_bcr(ioaddr, 27);
			if (i & (1 << 14))
				pr_cont("LowLatRx");
		}
	}

	dev->base_addr = ioaddr;
	lp = netdev_priv(dev);
	
	lp->init_block = pci_alloc_consistent(pdev, sizeof(*lp->init_block),
					      &lp->init_dma_addr);
	if (!lp->init_block) {
		if (pcnet32_debug & NETIF_MSG_PROBE)
			pr_err("Consistent memory allocation failed\n");
		ret = -ENOMEM;
		goto err_free_netdev;
	}
	lp->pci_dev = pdev;

	lp->dev = dev;

	spin_lock_init(&lp->lock);

	lp->name = chipname;
	lp->shared_irq = shared;
	lp->tx_ring_size = TX_RING_SIZE;	
	lp->rx_ring_size = RX_RING_SIZE;	
	lp->tx_mod_mask = lp->tx_ring_size - 1;
	lp->rx_mod_mask = lp->rx_ring_size - 1;
	lp->tx_len_bits = (PCNET32_LOG_TX_BUFFERS << 12);
	lp->rx_len_bits = (PCNET32_LOG_RX_BUFFERS << 4);
	lp->mii_if.full_duplex = fdx;
	lp->mii_if.phy_id_mask = 0x1f;
	lp->mii_if.reg_num_mask = 0x1f;
	lp->dxsuflo = dxsuflo;
	lp->mii = mii;
	lp->chip_version = chip_version;
	lp->msg_enable = pcnet32_debug;
	if ((cards_found >= MAX_UNITS) ||
	    (options[cards_found] >= sizeof(options_mapping)))
		lp->options = PCNET32_PORT_ASEL;
	else
		lp->options = options_mapping[options[cards_found]];
	lp->mii_if.dev = dev;
	lp->mii_if.mdio_read = mdio_read;
	lp->mii_if.mdio_write = mdio_write;

	
	lp->napi.weight = lp->rx_ring_size / 2;

	netif_napi_add(dev, &lp->napi, pcnet32_poll, lp->rx_ring_size / 2);

	if (fdx && !(lp->options & PCNET32_PORT_ASEL) &&
	    ((cards_found >= MAX_UNITS) || full_duplex[cards_found]))
		lp->options |= PCNET32_PORT_FD;

	lp->a = a;

	
	if (pcnet32_alloc_ring(dev, pci_name(lp->pci_dev))) {
		ret = -ENOMEM;
		goto err_free_ring;
	}
	
	if (dev->dev_addr[0] == 0x00 && dev->dev_addr[1] == 0xe0 &&
	    dev->dev_addr[2] == 0x75)
		lp->options = PCNET32_PORT_FD | PCNET32_PORT_GPSI;

	lp->init_block->mode = cpu_to_le16(0x0003);	
	lp->init_block->tlen_rlen =
	    cpu_to_le16(lp->tx_len_bits | lp->rx_len_bits);
	for (i = 0; i < 6; i++)
		lp->init_block->phys_addr[i] = dev->dev_addr[i];
	lp->init_block->filter[0] = 0x00000000;
	lp->init_block->filter[1] = 0x00000000;
	lp->init_block->rx_ring = cpu_to_le32(lp->rx_ring_dma_addr);
	lp->init_block->tx_ring = cpu_to_le32(lp->tx_ring_dma_addr);

	
	a->write_bcr(ioaddr, 20, 2);

	a->write_csr(ioaddr, 1, (lp->init_dma_addr & 0xffff));
	a->write_csr(ioaddr, 2, (lp->init_dma_addr >> 16));

	if (pdev) {		
		dev->irq = pdev->irq;
		if (pcnet32_debug & NETIF_MSG_PROBE)
			pr_cont(" assigned IRQ %d\n", dev->irq);
	} else {
		unsigned long irq_mask = probe_irq_on();

		
		a->write_csr(ioaddr, CSR0, CSR0_INTEN | CSR0_INIT);
		mdelay(1);

		dev->irq = probe_irq_off(irq_mask);
		if (!dev->irq) {
			if (pcnet32_debug & NETIF_MSG_PROBE)
				pr_cont(", failed to detect IRQ line\n");
			ret = -ENODEV;
			goto err_free_ring;
		}
		if (pcnet32_debug & NETIF_MSG_PROBE)
			pr_cont(", probed IRQ %d\n", dev->irq);
	}

	
	if (lp->mii) {
		

		lp->mii_if.phy_id = ((lp->a->read_bcr(ioaddr, 33)) >> 5) & 0x1f;
		
		for (i = 0; i < PCNET32_MAX_PHYS; i++) {
			unsigned short id1, id2;

			id1 = mdio_read(dev, i, MII_PHYSID1);
			if (id1 == 0xffff)
				continue;
			id2 = mdio_read(dev, i, MII_PHYSID2);
			if (id2 == 0xffff)
				continue;
			if (i == 31 && ((chip_version + 1) & 0xfffe) == 0x2624)
				continue;	
			lp->phycount++;
			lp->phymask |= (1 << i);
			lp->mii_if.phy_id = i;
			if (pcnet32_debug & NETIF_MSG_PROBE)
				pr_info("Found PHY %04x:%04x at address %d\n",
					id1, id2, i);
		}
		lp->a->write_bcr(ioaddr, 33, (lp->mii_if.phy_id) << 5);
		if (lp->phycount > 1)
			lp->options |= PCNET32_PORT_MII;
	}

	init_timer(&lp->watchdog_timer);
	lp->watchdog_timer.data = (unsigned long)dev;
	lp->watchdog_timer.function = (void *)&pcnet32_watchdog;

	
	dev->netdev_ops = &pcnet32_netdev_ops;
	dev->ethtool_ops = &pcnet32_ethtool_ops;
	dev->watchdog_timeo = (5 * HZ);

	
	if (register_netdev(dev))
		goto err_free_ring;

	if (pdev) {
		pci_set_drvdata(pdev, dev);
	} else {
		lp->next = pcnet32_dev;
		pcnet32_dev = dev;
	}

	if (pcnet32_debug & NETIF_MSG_PROBE)
		pr_info("%s: registered as %s\n", dev->name, lp->name);
	cards_found++;

	
	a->write_bcr(ioaddr, 2, a->read_bcr(ioaddr, 2) | 0x1000);

	return 0;

err_free_ring:
	pcnet32_free_ring(dev);
	pci_free_consistent(lp->pci_dev, sizeof(*lp->init_block),
			    lp->init_block, lp->init_dma_addr);
err_free_netdev:
	free_netdev(dev);
err_release_region:
	release_region(ioaddr, PCNET32_TOTAL_SIZE);
	return ret;
}

static int pcnet32_alloc_ring(struct net_device *dev, const char *name)
{
	struct pcnet32_private *lp = netdev_priv(dev);

	lp->tx_ring = pci_alloc_consistent(lp->pci_dev,
					   sizeof(struct pcnet32_tx_head) *
					   lp->tx_ring_size,
					   &lp->tx_ring_dma_addr);
	if (lp->tx_ring == NULL) {
		netif_err(lp, drv, dev, "Consistent memory allocation failed\n");
		return -ENOMEM;
	}

	lp->rx_ring = pci_alloc_consistent(lp->pci_dev,
					   sizeof(struct pcnet32_rx_head) *
					   lp->rx_ring_size,
					   &lp->rx_ring_dma_addr);
	if (lp->rx_ring == NULL) {
		netif_err(lp, drv, dev, "Consistent memory allocation failed\n");
		return -ENOMEM;
	}

	lp->tx_dma_addr = kcalloc(lp->tx_ring_size, sizeof(dma_addr_t),
				  GFP_ATOMIC);
	if (!lp->tx_dma_addr) {
		netif_err(lp, drv, dev, "Memory allocation failed\n");
		return -ENOMEM;
	}

	lp->rx_dma_addr = kcalloc(lp->rx_ring_size, sizeof(dma_addr_t),
				  GFP_ATOMIC);
	if (!lp->rx_dma_addr) {
		netif_err(lp, drv, dev, "Memory allocation failed\n");
		return -ENOMEM;
	}

	lp->tx_skbuff = kcalloc(lp->tx_ring_size, sizeof(struct sk_buff *),
				GFP_ATOMIC);
	if (!lp->tx_skbuff) {
		netif_err(lp, drv, dev, "Memory allocation failed\n");
		return -ENOMEM;
	}

	lp->rx_skbuff = kcalloc(lp->rx_ring_size, sizeof(struct sk_buff *),
				GFP_ATOMIC);
	if (!lp->rx_skbuff) {
		netif_err(lp, drv, dev, "Memory allocation failed\n");
		return -ENOMEM;
	}

	return 0;
}

static void pcnet32_free_ring(struct net_device *dev)
{
	struct pcnet32_private *lp = netdev_priv(dev);

	kfree(lp->tx_skbuff);
	lp->tx_skbuff = NULL;

	kfree(lp->rx_skbuff);
	lp->rx_skbuff = NULL;

	kfree(lp->tx_dma_addr);
	lp->tx_dma_addr = NULL;

	kfree(lp->rx_dma_addr);
	lp->rx_dma_addr = NULL;

	if (lp->tx_ring) {
		pci_free_consistent(lp->pci_dev,
				    sizeof(struct pcnet32_tx_head) *
				    lp->tx_ring_size, lp->tx_ring,
				    lp->tx_ring_dma_addr);
		lp->tx_ring = NULL;
	}

	if (lp->rx_ring) {
		pci_free_consistent(lp->pci_dev,
				    sizeof(struct pcnet32_rx_head) *
				    lp->rx_ring_size, lp->rx_ring,
				    lp->rx_ring_dma_addr);
		lp->rx_ring = NULL;
	}
}

static int pcnet32_open(struct net_device *dev)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	struct pci_dev *pdev = lp->pci_dev;
	unsigned long ioaddr = dev->base_addr;
	u16 val;
	int i;
	int rc;
	unsigned long flags;

	if (request_irq(dev->irq, pcnet32_interrupt,
			lp->shared_irq ? IRQF_SHARED : 0, dev->name,
			(void *)dev)) {
		return -EAGAIN;
	}

	spin_lock_irqsave(&lp->lock, flags);
	
	if (!is_valid_ether_addr(dev->dev_addr)) {
		rc = -EINVAL;
		goto err_free_irq;
	}

	
	lp->a->reset(ioaddr);

	
	lp->a->write_bcr(ioaddr, 20, 2);

	netif_printk(lp, ifup, KERN_DEBUG, dev,
		     "%s() irq %d tx/rx rings %#x/%#x init %#x\n",
		     __func__, dev->irq, (u32) (lp->tx_ring_dma_addr),
		     (u32) (lp->rx_ring_dma_addr),
		     (u32) (lp->init_dma_addr));

	
	val = lp->a->read_bcr(ioaddr, 2) & ~2;
	if (lp->options & PCNET32_PORT_ASEL)
		val |= 2;
	lp->a->write_bcr(ioaddr, 2, val);

	
	if (lp->mii_if.full_duplex) {
		val = lp->a->read_bcr(ioaddr, 9) & ~3;
		if (lp->options & PCNET32_PORT_FD) {
			val |= 1;
			if (lp->options == (PCNET32_PORT_FD | PCNET32_PORT_AUI))
				val |= 2;
		} else if (lp->options & PCNET32_PORT_ASEL) {
			
			if (lp->chip_version == 0x2627)
				val |= 3;
		}
		lp->a->write_bcr(ioaddr, 9, val);
	}

	
	val = lp->a->read_csr(ioaddr, 124) & ~0x10;
	if ((lp->options & PCNET32_PORT_PORTSEL) == PCNET32_PORT_GPSI)
		val |= 0x10;
	lp->a->write_csr(ioaddr, 124, val);

	
	if (pdev && pdev->subsystem_vendor == PCI_VENDOR_ID_AT &&
	    (pdev->subsystem_device == PCI_SUBDEVICE_ID_AT_2700FX ||
	     pdev->subsystem_device == PCI_SUBDEVICE_ID_AT_2701FX)) {
		if (lp->options & PCNET32_PORT_ASEL) {
			lp->options = PCNET32_PORT_FD | PCNET32_PORT_100;
			netif_printk(lp, link, KERN_DEBUG, dev,
				     "Setting 100Mb-Full Duplex\n");
		}
	}
	if (lp->phycount < 2) {
		if (lp->mii && !(lp->options & PCNET32_PORT_ASEL)) {
			lp->a->write_bcr(ioaddr, 32,
					lp->a->read_bcr(ioaddr, 32) | 0x0080);
			
			val = lp->a->read_bcr(ioaddr, 32) & ~0xb8;
			if (lp->options & PCNET32_PORT_FD)
				val |= 0x10;
			if (lp->options & PCNET32_PORT_100)
				val |= 0x08;
			lp->a->write_bcr(ioaddr, 32, val);
		} else {
			if (lp->options & PCNET32_PORT_ASEL) {
				lp->a->write_bcr(ioaddr, 32,
						lp->a->read_bcr(ioaddr,
							       32) | 0x0080);
				
				val = lp->a->read_bcr(ioaddr, 32) & ~0x98;
				val |= 0x20;
				lp->a->write_bcr(ioaddr, 32, val);
			}
		}
	} else {
		int first_phy = -1;
		u16 bmcr;
		u32 bcr9;
		struct ethtool_cmd ecmd = { .cmd = ETHTOOL_GSET };

		val = lp->a->read_bcr(ioaddr, 2);
		lp->a->write_bcr(ioaddr, 2, val & ~2);
		val = lp->a->read_bcr(ioaddr, 32);
		lp->a->write_bcr(ioaddr, 32, val & ~(1 << 7));	

		if (!(lp->options & PCNET32_PORT_ASEL)) {
			
			ecmd.port = PORT_MII;
			ecmd.transceiver = XCVR_INTERNAL;
			ecmd.autoneg = AUTONEG_DISABLE;
			ethtool_cmd_speed_set(&ecmd,
					      (lp->options & PCNET32_PORT_100) ?
					      SPEED_100 : SPEED_10);
			bcr9 = lp->a->read_bcr(ioaddr, 9);

			if (lp->options & PCNET32_PORT_FD) {
				ecmd.duplex = DUPLEX_FULL;
				bcr9 |= (1 << 0);
			} else {
				ecmd.duplex = DUPLEX_HALF;
				bcr9 |= ~(1 << 0);
			}
			lp->a->write_bcr(ioaddr, 9, bcr9);
		}

		for (i = 0; i < PCNET32_MAX_PHYS; i++) {
			if (lp->phymask & (1 << i)) {
				
				bmcr = mdio_read(dev, i, MII_BMCR);
				if (first_phy == -1) {
					first_phy = i;
					mdio_write(dev, i, MII_BMCR,
						   bmcr & ~BMCR_ISOLATE);
				} else {
					mdio_write(dev, i, MII_BMCR,
						   bmcr | BMCR_ISOLATE);
				}
				
				lp->mii_if.phy_id = i;
				ecmd.phy_address = i;
				if (lp->options & PCNET32_PORT_ASEL) {
					mii_ethtool_gset(&lp->mii_if, &ecmd);
					ecmd.autoneg = AUTONEG_ENABLE;
				}
				mii_ethtool_sset(&lp->mii_if, &ecmd);
			}
		}
		lp->mii_if.phy_id = first_phy;
		netif_info(lp, link, dev, "Using PHY number %d\n", first_phy);
	}

#ifdef DO_DXSUFLO
	if (lp->dxsuflo) {	
		val = lp->a->read_csr(ioaddr, CSR3);
		val |= 0x40;
		lp->a->write_csr(ioaddr, CSR3, val);
	}
#endif

	lp->init_block->mode =
	    cpu_to_le16((lp->options & PCNET32_PORT_PORTSEL) << 7);
	pcnet32_load_multicast(dev);

	if (pcnet32_init_ring(dev)) {
		rc = -ENOMEM;
		goto err_free_ring;
	}

	napi_enable(&lp->napi);

	
	lp->a->write_csr(ioaddr, 1, (lp->init_dma_addr & 0xffff));
	lp->a->write_csr(ioaddr, 2, (lp->init_dma_addr >> 16));

	lp->a->write_csr(ioaddr, CSR4, 0x0915);	
	lp->a->write_csr(ioaddr, CSR0, CSR0_INIT);

	netif_start_queue(dev);

	if (lp->chip_version >= PCNET32_79C970A) {
		
		pcnet32_check_media(dev, 1);
		mod_timer(&lp->watchdog_timer, PCNET32_WATCHDOG_TIMEOUT);
	}

	i = 0;
	while (i++ < 100)
		if (lp->a->read_csr(ioaddr, CSR0) & CSR0_IDON)
			break;
	lp->a->write_csr(ioaddr, CSR0, CSR0_NORMAL);

	netif_printk(lp, ifup, KERN_DEBUG, dev,
		     "pcnet32 open after %d ticks, init block %#x csr0 %4.4x\n",
		     i,
		     (u32) (lp->init_dma_addr),
		     lp->a->read_csr(ioaddr, CSR0));

	spin_unlock_irqrestore(&lp->lock, flags);

	return 0;		

err_free_ring:
	
	pcnet32_purge_rx_ring(dev);

	lp->a->write_bcr(ioaddr, 20, 4);

err_free_irq:
	spin_unlock_irqrestore(&lp->lock, flags);
	free_irq(dev->irq, dev);
	return rc;
}


static void pcnet32_purge_tx_ring(struct net_device *dev)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	int i;

	for (i = 0; i < lp->tx_ring_size; i++) {
		lp->tx_ring[i].status = 0;	
		wmb();		
		if (lp->tx_skbuff[i]) {
			pci_unmap_single(lp->pci_dev, lp->tx_dma_addr[i],
					 lp->tx_skbuff[i]->len,
					 PCI_DMA_TODEVICE);
			dev_kfree_skb_any(lp->tx_skbuff[i]);
		}
		lp->tx_skbuff[i] = NULL;
		lp->tx_dma_addr[i] = 0;
	}
}

static int pcnet32_init_ring(struct net_device *dev)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	int i;

	lp->tx_full = 0;
	lp->cur_rx = lp->cur_tx = 0;
	lp->dirty_rx = lp->dirty_tx = 0;

	for (i = 0; i < lp->rx_ring_size; i++) {
		struct sk_buff *rx_skbuff = lp->rx_skbuff[i];
		if (rx_skbuff == NULL) {
			lp->rx_skbuff[i] = netdev_alloc_skb(dev, PKT_BUF_SKB);
			rx_skbuff = lp->rx_skbuff[i];
			if (!rx_skbuff) {
				
				netif_err(lp, drv, dev, "%s netdev_alloc_skb failed\n",
					  __func__);
				return -1;
			}
			skb_reserve(rx_skbuff, NET_IP_ALIGN);
		}

		rmb();
		if (lp->rx_dma_addr[i] == 0)
			lp->rx_dma_addr[i] =
			    pci_map_single(lp->pci_dev, rx_skbuff->data,
					   PKT_BUF_SIZE, PCI_DMA_FROMDEVICE);
		lp->rx_ring[i].base = cpu_to_le32(lp->rx_dma_addr[i]);
		lp->rx_ring[i].buf_length = cpu_to_le16(NEG_BUF_SIZE);
		wmb();		
		lp->rx_ring[i].status = cpu_to_le16(0x8000);
	}
	for (i = 0; i < lp->tx_ring_size; i++) {
		lp->tx_ring[i].status = 0;	
		wmb();		
		lp->tx_ring[i].base = 0;
		lp->tx_dma_addr[i] = 0;
	}

	lp->init_block->tlen_rlen =
	    cpu_to_le16(lp->tx_len_bits | lp->rx_len_bits);
	for (i = 0; i < 6; i++)
		lp->init_block->phys_addr[i] = dev->dev_addr[i];
	lp->init_block->rx_ring = cpu_to_le32(lp->rx_ring_dma_addr);
	lp->init_block->tx_ring = cpu_to_le32(lp->tx_ring_dma_addr);
	wmb();			
	return 0;
}

static void pcnet32_restart(struct net_device *dev, unsigned int csr0_bits)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;
	int i;

	
	for (i = 0; i < 100; i++)
		if (lp->a->read_csr(ioaddr, CSR0) & CSR0_STOP)
			break;

	if (i >= 100)
		netif_err(lp, drv, dev, "%s timed out waiting for stop\n",
			  __func__);

	pcnet32_purge_tx_ring(dev);
	if (pcnet32_init_ring(dev))
		return;

	
	lp->a->write_csr(ioaddr, CSR0, CSR0_INIT);
	i = 0;
	while (i++ < 1000)
		if (lp->a->read_csr(ioaddr, CSR0) & CSR0_IDON)
			break;

	lp->a->write_csr(ioaddr, CSR0, csr0_bits);
}

static void pcnet32_tx_timeout(struct net_device *dev)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr, flags;

	spin_lock_irqsave(&lp->lock, flags);
	
	if (pcnet32_debug & NETIF_MSG_DRV)
		pr_err("%s: transmit timed out, status %4.4x, resetting\n",
		       dev->name, lp->a->read_csr(ioaddr, CSR0));
	lp->a->write_csr(ioaddr, CSR0, CSR0_STOP);
	dev->stats.tx_errors++;
	if (netif_msg_tx_err(lp)) {
		int i;
		printk(KERN_DEBUG
		       " Ring data dump: dirty_tx %d cur_tx %d%s cur_rx %d.",
		       lp->dirty_tx, lp->cur_tx, lp->tx_full ? " (full)" : "",
		       lp->cur_rx);
		for (i = 0; i < lp->rx_ring_size; i++)
			printk("%s %08x %04x %08x %04x", i & 1 ? "" : "\n ",
			       le32_to_cpu(lp->rx_ring[i].base),
			       (-le16_to_cpu(lp->rx_ring[i].buf_length)) &
			       0xffff, le32_to_cpu(lp->rx_ring[i].msg_length),
			       le16_to_cpu(lp->rx_ring[i].status));
		for (i = 0; i < lp->tx_ring_size; i++)
			printk("%s %08x %04x %08x %04x", i & 1 ? "" : "\n ",
			       le32_to_cpu(lp->tx_ring[i].base),
			       (-le16_to_cpu(lp->tx_ring[i].length)) & 0xffff,
			       le32_to_cpu(lp->tx_ring[i].misc),
			       le16_to_cpu(lp->tx_ring[i].status));
		printk("\n");
	}
	pcnet32_restart(dev, CSR0_NORMAL);

	dev->trans_start = jiffies; 
	netif_wake_queue(dev);

	spin_unlock_irqrestore(&lp->lock, flags);
}

static netdev_tx_t pcnet32_start_xmit(struct sk_buff *skb,
				      struct net_device *dev)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;
	u16 status;
	int entry;
	unsigned long flags;

	spin_lock_irqsave(&lp->lock, flags);

	netif_printk(lp, tx_queued, KERN_DEBUG, dev,
		     "%s() called, csr0 %4.4x\n",
		     __func__, lp->a->read_csr(ioaddr, CSR0));

	status = 0x8300;

	

	
	entry = lp->cur_tx & lp->tx_mod_mask;


	lp->tx_ring[entry].length = cpu_to_le16(-skb->len);

	lp->tx_ring[entry].misc = 0x00000000;

	lp->tx_skbuff[entry] = skb;
	lp->tx_dma_addr[entry] =
	    pci_map_single(lp->pci_dev, skb->data, skb->len, PCI_DMA_TODEVICE);
	lp->tx_ring[entry].base = cpu_to_le32(lp->tx_dma_addr[entry]);
	wmb();			
	lp->tx_ring[entry].status = cpu_to_le16(status);

	lp->cur_tx++;
	dev->stats.tx_bytes += skb->len;

	
	lp->a->write_csr(ioaddr, CSR0, CSR0_INTEN | CSR0_TXPOLL);

	if (lp->tx_ring[(entry + 1) & lp->tx_mod_mask].base != 0) {
		lp->tx_full = 1;
		netif_stop_queue(dev);
	}
	spin_unlock_irqrestore(&lp->lock, flags);
	return NETDEV_TX_OK;
}

static irqreturn_t
pcnet32_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct pcnet32_private *lp;
	unsigned long ioaddr;
	u16 csr0;
	int boguscnt = max_interrupt_work;

	ioaddr = dev->base_addr;
	lp = netdev_priv(dev);

	spin_lock(&lp->lock);

	csr0 = lp->a->read_csr(ioaddr, CSR0);
	while ((csr0 & 0x8f00) && --boguscnt >= 0) {
		if (csr0 == 0xffff)
			break;	
		
		lp->a->write_csr(ioaddr, CSR0, csr0 & ~0x004f);

		netif_printk(lp, intr, KERN_DEBUG, dev,
			     "interrupt  csr0=%#2.2x new csr=%#2.2x\n",
			     csr0, lp->a->read_csr(ioaddr, CSR0));

		
		if (csr0 & 0x4000)
			dev->stats.tx_errors++;	
		if (csr0 & 0x1000) {
			dev->stats.rx_errors++;	
		}
		if (csr0 & 0x0800) {
			netif_err(lp, drv, dev, "Bus master arbitration failure, status %4.4x\n",
				  csr0);
			
		}
		if (napi_schedule_prep(&lp->napi)) {
			u16 val;
			
			val = lp->a->read_csr(ioaddr, CSR3);
			val |= 0x5f00;
			lp->a->write_csr(ioaddr, CSR3, val);

			__napi_schedule(&lp->napi);
			break;
		}
		csr0 = lp->a->read_csr(ioaddr, CSR0);
	}

	netif_printk(lp, intr, KERN_DEBUG, dev,
		     "exiting interrupt, csr0=%#4.4x\n",
		     lp->a->read_csr(ioaddr, CSR0));

	spin_unlock(&lp->lock);

	return IRQ_HANDLED;
}

static int pcnet32_close(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	struct pcnet32_private *lp = netdev_priv(dev);
	unsigned long flags;

	del_timer_sync(&lp->watchdog_timer);

	netif_stop_queue(dev);
	napi_disable(&lp->napi);

	spin_lock_irqsave(&lp->lock, flags);

	dev->stats.rx_missed_errors = lp->a->read_csr(ioaddr, 112);

	netif_printk(lp, ifdown, KERN_DEBUG, dev,
		     "Shutting down ethercard, status was %2.2x\n",
		     lp->a->read_csr(ioaddr, CSR0));

	
	lp->a->write_csr(ioaddr, CSR0, CSR0_STOP);

	lp->a->write_bcr(ioaddr, 20, 4);

	spin_unlock_irqrestore(&lp->lock, flags);

	free_irq(dev->irq, dev);

	spin_lock_irqsave(&lp->lock, flags);

	pcnet32_purge_rx_ring(dev);
	pcnet32_purge_tx_ring(dev);

	spin_unlock_irqrestore(&lp->lock, flags);

	return 0;
}

static struct net_device_stats *pcnet32_get_stats(struct net_device *dev)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;
	unsigned long flags;

	spin_lock_irqsave(&lp->lock, flags);
	dev->stats.rx_missed_errors = lp->a->read_csr(ioaddr, 112);
	spin_unlock_irqrestore(&lp->lock, flags);

	return &dev->stats;
}

static void pcnet32_load_multicast(struct net_device *dev)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	volatile struct pcnet32_init_block *ib = lp->init_block;
	volatile __le16 *mcast_table = (__le16 *)ib->filter;
	struct netdev_hw_addr *ha;
	unsigned long ioaddr = dev->base_addr;
	int i;
	u32 crc;

	
	if (dev->flags & IFF_ALLMULTI) {
		ib->filter[0] = cpu_to_le32(~0U);
		ib->filter[1] = cpu_to_le32(~0U);
		lp->a->write_csr(ioaddr, PCNET32_MC_FILTER, 0xffff);
		lp->a->write_csr(ioaddr, PCNET32_MC_FILTER+1, 0xffff);
		lp->a->write_csr(ioaddr, PCNET32_MC_FILTER+2, 0xffff);
		lp->a->write_csr(ioaddr, PCNET32_MC_FILTER+3, 0xffff);
		return;
	}
	
	ib->filter[0] = 0;
	ib->filter[1] = 0;

	
	netdev_for_each_mc_addr(ha, dev) {
		crc = ether_crc_le(6, ha->addr);
		crc = crc >> 26;
		mcast_table[crc >> 4] |= cpu_to_le16(1 << (crc & 0xf));
	}
	for (i = 0; i < 4; i++)
		lp->a->write_csr(ioaddr, PCNET32_MC_FILTER + i,
				le16_to_cpu(mcast_table[i]));
}

static void pcnet32_set_multicast_list(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr, flags;
	struct pcnet32_private *lp = netdev_priv(dev);
	int csr15, suspended;

	spin_lock_irqsave(&lp->lock, flags);
	suspended = pcnet32_suspend(dev, &flags, 0);
	csr15 = lp->a->read_csr(ioaddr, CSR15);
	if (dev->flags & IFF_PROMISC) {
		
		netif_info(lp, hw, dev, "Promiscuous mode enabled\n");
		lp->init_block->mode =
		    cpu_to_le16(0x8000 | (lp->options & PCNET32_PORT_PORTSEL) <<
				7);
		lp->a->write_csr(ioaddr, CSR15, csr15 | 0x8000);
	} else {
		lp->init_block->mode =
		    cpu_to_le16((lp->options & PCNET32_PORT_PORTSEL) << 7);
		lp->a->write_csr(ioaddr, CSR15, csr15 & 0x7fff);
		pcnet32_load_multicast(dev);
	}

	if (suspended) {
		int csr5;
		
		csr5 = lp->a->read_csr(ioaddr, CSR5);
		lp->a->write_csr(ioaddr, CSR5, csr5 & (~CSR5_SUSPEND));
	} else {
		lp->a->write_csr(ioaddr, CSR0, CSR0_STOP);
		pcnet32_restart(dev, CSR0_NORMAL);
		netif_wake_queue(dev);
	}

	spin_unlock_irqrestore(&lp->lock, flags);
}

static int mdio_read(struct net_device *dev, int phy_id, int reg_num)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;
	u16 val_out;

	if (!lp->mii)
		return 0;

	lp->a->write_bcr(ioaddr, 33, ((phy_id & 0x1f) << 5) | (reg_num & 0x1f));
	val_out = lp->a->read_bcr(ioaddr, 34);

	return val_out;
}

static void mdio_write(struct net_device *dev, int phy_id, int reg_num, int val)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;

	if (!lp->mii)
		return;

	lp->a->write_bcr(ioaddr, 33, ((phy_id & 0x1f) << 5) | (reg_num & 0x1f));
	lp->a->write_bcr(ioaddr, 34, val);
}

static int pcnet32_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	int rc;
	unsigned long flags;

	
	if (lp->mii) {
		spin_lock_irqsave(&lp->lock, flags);
		rc = generic_mii_ioctl(&lp->mii_if, if_mii(rq), cmd, NULL);
		spin_unlock_irqrestore(&lp->lock, flags);
	} else {
		rc = -EOPNOTSUPP;
	}

	return rc;
}

static int pcnet32_check_otherphy(struct net_device *dev)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	struct mii_if_info mii = lp->mii_if;
	u16 bmcr;
	int i;

	for (i = 0; i < PCNET32_MAX_PHYS; i++) {
		if (i == lp->mii_if.phy_id)
			continue;	
		if (lp->phymask & (1 << i)) {
			mii.phy_id = i;
			if (mii_link_ok(&mii)) {
				
				netif_info(lp, link, dev, "Using PHY number %d\n",
					   i);

				
				bmcr =
				    mdio_read(dev, lp->mii_if.phy_id, MII_BMCR);
				mdio_write(dev, lp->mii_if.phy_id, MII_BMCR,
					   bmcr | BMCR_ISOLATE);

				
				bmcr = mdio_read(dev, i, MII_BMCR);
				mdio_write(dev, i, MII_BMCR,
					   bmcr & ~BMCR_ISOLATE);

				
				lp->mii_if.phy_id = i;
				return 1;
			}
		}
	}
	return 0;
}


static void pcnet32_check_media(struct net_device *dev, int verbose)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	int curr_link;
	int prev_link = netif_carrier_ok(dev) ? 1 : 0;
	u32 bcr9;

	if (lp->mii) {
		curr_link = mii_link_ok(&lp->mii_if);
	} else {
		ulong ioaddr = dev->base_addr;	
		curr_link = (lp->a->read_bcr(ioaddr, 4) != 0xc0);
	}
	if (!curr_link) {
		if (prev_link || verbose) {
			netif_carrier_off(dev);
			netif_info(lp, link, dev, "link down\n");
		}
		if (lp->phycount > 1) {
			curr_link = pcnet32_check_otherphy(dev);
			prev_link = 0;
		}
	} else if (verbose || !prev_link) {
		netif_carrier_on(dev);
		if (lp->mii) {
			if (netif_msg_link(lp)) {
				struct ethtool_cmd ecmd = {
					.cmd = ETHTOOL_GSET };
				mii_ethtool_gset(&lp->mii_if, &ecmd);
				netdev_info(dev, "link up, %uMbps, %s-duplex\n",
					    ethtool_cmd_speed(&ecmd),
					    (ecmd.duplex == DUPLEX_FULL)
					    ? "full" : "half");
			}
			bcr9 = lp->a->read_bcr(dev->base_addr, 9);
			if ((bcr9 & (1 << 0)) != lp->mii_if.full_duplex) {
				if (lp->mii_if.full_duplex)
					bcr9 |= (1 << 0);
				else
					bcr9 &= ~(1 << 0);
				lp->a->write_bcr(dev->base_addr, 9, bcr9);
			}
		} else {
			netif_info(lp, link, dev, "link up\n");
		}
	}
}


static void pcnet32_watchdog(struct net_device *dev)
{
	struct pcnet32_private *lp = netdev_priv(dev);
	unsigned long flags;

	
	spin_lock_irqsave(&lp->lock, flags);
	pcnet32_check_media(dev, 0);
	spin_unlock_irqrestore(&lp->lock, flags);

	mod_timer(&lp->watchdog_timer, round_jiffies(PCNET32_WATCHDOG_TIMEOUT));
}

static int pcnet32_pm_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct net_device *dev = pci_get_drvdata(pdev);

	if (netif_running(dev)) {
		netif_device_detach(dev);
		pcnet32_close(dev);
	}
	pci_save_state(pdev);
	pci_set_power_state(pdev, pci_choose_state(pdev, state));
	return 0;
}

static int pcnet32_pm_resume(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);

	if (netif_running(dev)) {
		pcnet32_open(dev);
		netif_device_attach(dev);
	}
	return 0;
}

static void __devexit pcnet32_remove_one(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);

	if (dev) {
		struct pcnet32_private *lp = netdev_priv(dev);

		unregister_netdev(dev);
		pcnet32_free_ring(dev);
		release_region(dev->base_addr, PCNET32_TOTAL_SIZE);
		pci_free_consistent(lp->pci_dev, sizeof(*lp->init_block),
				    lp->init_block, lp->init_dma_addr);
		free_netdev(dev);
		pci_disable_device(pdev);
		pci_set_drvdata(pdev, NULL);
	}
}

static struct pci_driver pcnet32_driver = {
	.name = DRV_NAME,
	.probe = pcnet32_probe_pci,
	.remove = __devexit_p(pcnet32_remove_one),
	.id_table = pcnet32_pci_tbl,
	.suspend = pcnet32_pm_suspend,
	.resume = pcnet32_pm_resume,
};

static int debug = -1;
static int tx_start_pt = -1;
static int pcnet32_have_pci;

module_param(debug, int, 0);
MODULE_PARM_DESC(debug, DRV_NAME " debug level");
module_param(max_interrupt_work, int, 0);
MODULE_PARM_DESC(max_interrupt_work,
		 DRV_NAME " maximum events handled per interrupt");
module_param(rx_copybreak, int, 0);
MODULE_PARM_DESC(rx_copybreak,
		 DRV_NAME " copy breakpoint for copy-only-tiny-frames");
module_param(tx_start_pt, int, 0);
MODULE_PARM_DESC(tx_start_pt, DRV_NAME " transmit start point (0-3)");
module_param(pcnet32vlb, int, 0);
MODULE_PARM_DESC(pcnet32vlb, DRV_NAME " Vesa local bus (VLB) support (0/1)");
module_param_array(options, int, NULL, 0);
MODULE_PARM_DESC(options, DRV_NAME " initial option setting(s) (0-15)");
module_param_array(full_duplex, int, NULL, 0);
MODULE_PARM_DESC(full_duplex, DRV_NAME " full duplex setting(s) (1)");
module_param_array(homepna, int, NULL, 0);
MODULE_PARM_DESC(homepna,
		 DRV_NAME
		 " mode for 79C978 cards (1 for HomePNA, 0 for Ethernet, default Ethernet");

MODULE_AUTHOR("Thomas Bogendoerfer");
MODULE_DESCRIPTION("Driver for PCnet32 and PCnetPCI based ethercards");
MODULE_LICENSE("GPL");

#define PCNET32_MSG_DEFAULT (NETIF_MSG_DRV | NETIF_MSG_PROBE | NETIF_MSG_LINK)

static int __init pcnet32_init_module(void)
{
	pr_info("%s", version);

	pcnet32_debug = netif_msg_init(debug, PCNET32_MSG_DEFAULT);

	if ((tx_start_pt >= 0) && (tx_start_pt <= 3))
		tx_start = tx_start_pt;

	
	if (!pci_register_driver(&pcnet32_driver))
		pcnet32_have_pci = 1;

	
	if (pcnet32vlb)
		pcnet32_probe_vlbus(pcnet32_portlist);

	if (cards_found && (pcnet32_debug & NETIF_MSG_PROBE))
		pr_info("%d cards_found\n", cards_found);

	return (pcnet32_have_pci + cards_found) ? 0 : -ENODEV;
}

static void __exit pcnet32_cleanup_module(void)
{
	struct net_device *next_dev;

	while (pcnet32_dev) {
		struct pcnet32_private *lp = netdev_priv(pcnet32_dev);
		next_dev = lp->next;
		unregister_netdev(pcnet32_dev);
		pcnet32_free_ring(pcnet32_dev);
		release_region(pcnet32_dev->base_addr, PCNET32_TOTAL_SIZE);
		pci_free_consistent(lp->pci_dev, sizeof(*lp->init_block),
				    lp->init_block, lp->init_dma_addr);
		free_netdev(pcnet32_dev);
		pcnet32_dev = next_dev;
	}

	if (pcnet32_have_pci)
		pci_unregister_driver(&pcnet32_driver);
}

module_init(pcnet32_init_module);
module_exit(pcnet32_cleanup_module);

