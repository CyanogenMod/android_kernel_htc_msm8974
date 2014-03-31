/* sis900.c: A SiS 900/7016 PCI Fast Ethernet driver for Linux.
   Copyright 1999 Silicon Integrated System Corporation
   Revision:	1.08.10 Apr. 2 2006

   Modified from the driver which is originally written by Donald Becker.

   This software may be used and distributed according to the terms
   of the GNU General Public License (GPL), incorporated herein by reference.
   Drivers based on this skeleton fall under the GPL and must retain
   the authorship (implicit copyright) notice.

   References:
   SiS 7016 Fast Ethernet PCI Bus 10/100 Mbps LAN Controller with OnNow Support,
   preliminary Rev. 1.0 Jan. 14, 1998
   SiS 900 Fast Ethernet PCI Bus 10/100 Mbps LAN Single Chip with OnNow Support,
   preliminary Rev. 1.0 Nov. 10, 1998
   SiS 7014 Single Chip 100BASE-TX/10BASE-T Physical Layer Solution,
   preliminary Rev. 1.0 Jan. 18, 1998

   Rev 1.08.10 Apr.  2 2006 Daniele Venzano add vlan (jumbo packets) support
   Rev 1.08.09 Sep. 19 2005 Daniele Venzano add Wake on LAN support
   Rev 1.08.08 Jan. 22 2005 Daniele Venzano use netif_msg for debugging messages
   Rev 1.08.07 Nov.  2 2003 Daniele Venzano <venza@brownhat.org> add suspend/resume support
   Rev 1.08.06 Sep. 24 2002 Mufasa Yang bug fix for Tx timeout & add SiS963 support
   Rev 1.08.05 Jun.  6 2002 Mufasa Yang bug fix for read_eeprom & Tx descriptor over-boundary
   Rev 1.08.04 Apr. 25 2002 Mufasa Yang <mufasa@sis.com.tw> added SiS962 support
   Rev 1.08.03 Feb.  1 2002 Matt Domsch <Matt_Domsch@dell.com> update to use library crc32 function
   Rev 1.08.02 Nov. 30 2001 Hui-Fen Hsu workaround for EDB & bug fix for dhcp problem
   Rev 1.08.01 Aug. 25 2001 Hui-Fen Hsu update for 630ET & workaround for ICS1893 PHY
   Rev 1.08.00 Jun. 11 2001 Hui-Fen Hsu workaround for RTL8201 PHY and some bug fix
   Rev 1.07.11 Apr.  2 2001 Hui-Fen Hsu updates PCI drivers to use the new pci_set_dma_mask for kernel 2.4.3
   Rev 1.07.10 Mar.  1 2001 Hui-Fen Hsu <hfhsu@sis.com.tw> some bug fix & 635M/B support
   Rev 1.07.09 Feb.  9 2001 Dave Jones <davej@suse.de> PCI enable cleanup
   Rev 1.07.08 Jan.  8 2001 Lei-Chun Chang added RTL8201 PHY support
   Rev 1.07.07 Nov. 29 2000 Lei-Chun Chang added kernel-doc extractable documentation and 630 workaround fix
   Rev 1.07.06 Nov.  7 2000 Jeff Garzik <jgarzik@pobox.com> some bug fix and cleaning
   Rev 1.07.05 Nov.  6 2000 metapirat<metapirat@gmx.de> contribute media type select by ifconfig
   Rev 1.07.04 Sep.  6 2000 Lei-Chun Chang added ICS1893 PHY support
   Rev 1.07.03 Aug. 24 2000 Lei-Chun Chang (lcchang@sis.com.tw) modified 630E equalizer workaround rule
   Rev 1.07.01 Aug. 08 2000 Ollie Lho minor update for SiS 630E and SiS 630E A1
   Rev 1.07    Mar. 07 2000 Ollie Lho bug fix in Rx buffer ring
   Rev 1.06.04 Feb. 11 2000 Jeff Garzik <jgarzik@pobox.com> softnet and init for kernel 2.4
   Rev 1.06.03 Dec. 23 1999 Ollie Lho Third release
   Rev 1.06.02 Nov. 23 1999 Ollie Lho bug in mac probing fixed
   Rev 1.06.01 Nov. 16 1999 Ollie Lho CRC calculation provide by Joseph Zbiciak (im14u2c@primenet.com)
   Rev 1.06 Nov. 4 1999 Ollie Lho (ollie@sis.com.tw) Second release
   Rev 1.05.05 Oct. 29 1999 Ollie Lho (ollie@sis.com.tw) Single buffer Tx/Rx
   Chin-Shan Li (lcs@sis.com.tw) Added AMD Am79c901 HomePNA PHY support
   Rev 1.05 Aug. 7 1999 Jim Huang (cmhuang@sis.com.tw) Initial release
*/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/init.h>
#include <linux/mii.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/crc32.h>
#include <linux/bitops.h>
#include <linux/dma-mapping.h>

#include <asm/processor.h>      
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>	

#include "sis900.h"

#define SIS900_MODULE_NAME "sis900"
#define SIS900_DRV_VERSION "v1.08.10 Apr. 2 2006"

static const char version[] __devinitconst =
	KERN_INFO "sis900.c: " SIS900_DRV_VERSION "\n";

static int max_interrupt_work = 40;
static int multicast_filter_limit = 128;

static int sis900_debug = -1; 

#define SIS900_DEF_MSG \
	(NETIF_MSG_DRV		| \
	 NETIF_MSG_LINK		| \
	 NETIF_MSG_RX_ERR	| \
	 NETIF_MSG_TX_ERR)

#define TX_TIMEOUT  (4*HZ)

enum {
	SIS_900 = 0,
	SIS_7016
};
static const char * card_names[] = {
	"SiS 900 PCI Fast Ethernet",
	"SiS 7016 PCI Fast Ethernet"
};
static DEFINE_PCI_DEVICE_TABLE(sis900_pci_tbl) = {
	{PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_900,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, SIS_900},
	{PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_7016,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, SIS_7016},
	{0,}
};
MODULE_DEVICE_TABLE (pci, sis900_pci_tbl);

static void sis900_read_mode(struct net_device *net_dev, int *speed, int *duplex);

static const struct mii_chip_info {
	const char * name;
	u16 phy_id0;
	u16 phy_id1;
	u8  phy_types;
#define	HOME 	0x0001
#define LAN	0x0002
#define MIX	0x0003
#define UNKNOWN	0x0
} mii_chip_table[] = {
	{ "SiS 900 Internal MII PHY", 		0x001d, 0x8000, LAN },
	{ "SiS 7014 Physical Layer Solution", 	0x0016, 0xf830, LAN },
	{ "SiS 900 on Foxconn 661 7MI",         0x0143, 0xBC70, LAN },
	{ "Altimata AC101LF PHY",               0x0022, 0x5520, LAN },
	{ "ADM 7001 LAN PHY",			0x002e, 0xcc60, LAN },
	{ "AMD 79C901 10BASE-T PHY",  		0x0000, 0x6B70, LAN },
	{ "AMD 79C901 HomePNA PHY",		0x0000, 0x6B90, HOME},
	{ "ICS LAN PHY",			0x0015, 0xF440, LAN },
	{ "ICS LAN PHY",			0x0143, 0xBC70, LAN },
	{ "NS 83851 PHY",			0x2000, 0x5C20, MIX },
	{ "NS 83847 PHY",                       0x2000, 0x5C30, MIX },
	{ "Realtek RTL8201 PHY",		0x0000, 0x8200, LAN },
	{ "VIA 6103 PHY",			0x0101, 0x8f20, LAN },
	{NULL,},
};

struct mii_phy {
	struct mii_phy * next;
	int phy_addr;
	u16 phy_id0;
	u16 phy_id1;
	u16 status;
	u8  phy_types;
};

typedef struct _BufferDesc {
	u32 link;
	u32 cmdsts;
	u32 bufptr;
} BufferDesc;

struct sis900_private {
	struct pci_dev * pci_dev;

	spinlock_t lock;

	struct mii_phy * mii;
	struct mii_phy * first_mii; 
	unsigned int cur_phy;
	struct mii_if_info mii_info;

	struct timer_list timer; 
	u8 autong_complete; 

	u32 msg_enable;

	unsigned int cur_rx, dirty_rx; 
	unsigned int cur_tx, dirty_tx;

	
	struct sk_buff *tx_skbuff[NUM_TX_DESC];
	struct sk_buff *rx_skbuff[NUM_RX_DESC];
	BufferDesc *tx_ring;
	BufferDesc *rx_ring;

	dma_addr_t tx_ring_dma;
	dma_addr_t rx_ring_dma;

	unsigned int tx_full; 
	u8 host_bridge_rev;
	u8 chipset_rev;
};

MODULE_AUTHOR("Jim Huang <cmhuang@sis.com.tw>, Ollie Lho <ollie@sis.com.tw>");
MODULE_DESCRIPTION("SiS 900 PCI Fast Ethernet driver");
MODULE_LICENSE("GPL");

module_param(multicast_filter_limit, int, 0444);
module_param(max_interrupt_work, int, 0444);
module_param(sis900_debug, int, 0444);
MODULE_PARM_DESC(multicast_filter_limit, "SiS 900/7016 maximum number of filtered multicast addresses");
MODULE_PARM_DESC(max_interrupt_work, "SiS 900/7016 maximum events handled per interrupt");
MODULE_PARM_DESC(sis900_debug, "SiS 900/7016 bitmapped debugging message level");

#ifdef CONFIG_NET_POLL_CONTROLLER
static void sis900_poll(struct net_device *dev);
#endif
static int sis900_open(struct net_device *net_dev);
static int sis900_mii_probe (struct net_device * net_dev);
static void sis900_init_rxfilter (struct net_device * net_dev);
static u16 read_eeprom(long ioaddr, int location);
static int mdio_read(struct net_device *net_dev, int phy_id, int location);
static void mdio_write(struct net_device *net_dev, int phy_id, int location, int val);
static void sis900_timer(unsigned long data);
static void sis900_check_mode (struct net_device *net_dev, struct mii_phy *mii_phy);
static void sis900_tx_timeout(struct net_device *net_dev);
static void sis900_init_tx_ring(struct net_device *net_dev);
static void sis900_init_rx_ring(struct net_device *net_dev);
static netdev_tx_t sis900_start_xmit(struct sk_buff *skb,
				     struct net_device *net_dev);
static int sis900_rx(struct net_device *net_dev);
static void sis900_finish_xmit (struct net_device *net_dev);
static irqreturn_t sis900_interrupt(int irq, void *dev_instance);
static int sis900_close(struct net_device *net_dev);
static int mii_ioctl(struct net_device *net_dev, struct ifreq *rq, int cmd);
static u16 sis900_mcast_bitnr(u8 *addr, u8 revision);
static void set_rx_mode(struct net_device *net_dev);
static void sis900_reset(struct net_device *net_dev);
static void sis630_set_eq(struct net_device *net_dev, u8 revision);
static int sis900_set_config(struct net_device *dev, struct ifmap *map);
static u16 sis900_default_phy(struct net_device * net_dev);
static void sis900_set_capability( struct net_device *net_dev ,struct mii_phy *phy);
static u16 sis900_reset_phy(struct net_device *net_dev, int phy_addr);
static void sis900_auto_negotiate(struct net_device *net_dev, int phy_addr);
static void sis900_set_mode (long ioaddr, int speed, int duplex);
static const struct ethtool_ops sis900_ethtool_ops;


static int __devinit sis900_get_mac_addr(struct pci_dev * pci_dev, struct net_device *net_dev)
{
	long ioaddr = pci_resource_start(pci_dev, 0);
	u16 signature;
	int i;

	
	signature = (u16) read_eeprom(ioaddr, EEPROMSignature);
	if (signature == 0xffff || signature == 0x0000) {
		printk (KERN_WARNING "%s: Error EERPOM read %x\n",
			pci_name(pci_dev), signature);
		return 0;
	}

	
	for (i = 0; i < 3; i++)
	        ((u16 *)(net_dev->dev_addr))[i] = read_eeprom(ioaddr, i+EEPROMMACAddr);

	
	memcpy(net_dev->perm_addr, net_dev->dev_addr, ETH_ALEN);

	return 1;
}


static int __devinit sis630e_get_mac_addr(struct pci_dev * pci_dev,
					struct net_device *net_dev)
{
	struct pci_dev *isa_bridge = NULL;
	u8 reg;
	int i;

	isa_bridge = pci_get_device(PCI_VENDOR_ID_SI, 0x0008, isa_bridge);
	if (!isa_bridge)
		isa_bridge = pci_get_device(PCI_VENDOR_ID_SI, 0x0018, isa_bridge);
	if (!isa_bridge) {
		printk(KERN_WARNING "%s: Can not find ISA bridge\n",
		       pci_name(pci_dev));
		return 0;
	}
	pci_read_config_byte(isa_bridge, 0x48, &reg);
	pci_write_config_byte(isa_bridge, 0x48, reg | 0x40);

	for (i = 0; i < 6; i++) {
		outb(0x09 + i, 0x70);
		((u8 *)(net_dev->dev_addr))[i] = inb(0x71);
	}

	
	memcpy(net_dev->perm_addr, net_dev->dev_addr, ETH_ALEN);

	pci_write_config_byte(isa_bridge, 0x48, reg & ~0x40);
	pci_dev_put(isa_bridge);

	return 1;
}



static int __devinit sis635_get_mac_addr(struct pci_dev * pci_dev,
					struct net_device *net_dev)
{
	long ioaddr = net_dev->base_addr;
	u32 rfcrSave;
	u32 i;

	rfcrSave = inl(rfcr + ioaddr);

	outl(rfcrSave | RELOAD, ioaddr + cr);
	outl(0, ioaddr + cr);

	
	outl(rfcrSave & ~RFEN, rfcr + ioaddr);

	
	for (i = 0 ; i < 3 ; i++) {
		outl((i << RFADDR_shift), ioaddr + rfcr);
		*( ((u16 *)net_dev->dev_addr) + i) = inw(ioaddr + rfdr);
	}

	
	memcpy(net_dev->perm_addr, net_dev->dev_addr, ETH_ALEN);

	
	outl(rfcrSave | RFEN, rfcr + ioaddr);

	return 1;
}


static int __devinit sis96x_get_mac_addr(struct pci_dev * pci_dev,
					struct net_device *net_dev)
{
	long ioaddr = net_dev->base_addr;
	long ee_addr = ioaddr + mear;
	u32 waittime = 0;
	int i;

	outl(EEREQ, ee_addr);
	while(waittime < 2000) {
		if(inl(ee_addr) & EEGNT) {

			
			for (i = 0; i < 3; i++)
			        ((u16 *)(net_dev->dev_addr))[i] = read_eeprom(ioaddr, i+EEPROMMACAddr);

			
			memcpy(net_dev->perm_addr, net_dev->dev_addr, ETH_ALEN);

			outl(EEDONE, ee_addr);
			return 1;
		} else {
			udelay(1);
			waittime ++;
		}
	}
	outl(EEDONE, ee_addr);
	return 0;
}

static const struct net_device_ops sis900_netdev_ops = {
	.ndo_open		 = sis900_open,
	.ndo_stop		= sis900_close,
	.ndo_start_xmit		= sis900_start_xmit,
	.ndo_set_config		= sis900_set_config,
	.ndo_set_rx_mode	= set_rx_mode,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_do_ioctl		= mii_ioctl,
	.ndo_tx_timeout		= sis900_tx_timeout,
#ifdef CONFIG_NET_POLL_CONTROLLER
        .ndo_poll_controller	= sis900_poll,
#endif
};


static int __devinit sis900_probe(struct pci_dev *pci_dev,
				const struct pci_device_id *pci_id)
{
	struct sis900_private *sis_priv;
	struct net_device *net_dev;
	struct pci_dev *dev;
	dma_addr_t ring_dma;
	void *ring_space;
	long ioaddr;
	int i, ret;
	const char *card_name = card_names[pci_id->driver_data];
	const char *dev_name = pci_name(pci_dev);

#ifndef MODULE
	static int printed_version;
	if (!printed_version++)
		printk(version);
#endif

	
	ret = pci_enable_device(pci_dev);
	if(ret) return ret;

	i = pci_set_dma_mask(pci_dev, DMA_BIT_MASK(32));
	if(i){
		printk(KERN_ERR "sis900.c: architecture does not support "
			"32bit PCI busmaster DMA\n");
		return i;
	}

	pci_set_master(pci_dev);

	net_dev = alloc_etherdev(sizeof(struct sis900_private));
	if (!net_dev)
		return -ENOMEM;
	SET_NETDEV_DEV(net_dev, &pci_dev->dev);

	
	ioaddr = pci_resource_start(pci_dev, 0);
	ret = pci_request_regions(pci_dev, "sis900");
	if (ret)
		goto err_out;

	sis_priv = netdev_priv(net_dev);
	net_dev->base_addr = ioaddr;
	net_dev->irq = pci_dev->irq;
	sis_priv->pci_dev = pci_dev;
	spin_lock_init(&sis_priv->lock);

	pci_set_drvdata(pci_dev, net_dev);

	ring_space = pci_alloc_consistent(pci_dev, TX_TOTAL_SIZE, &ring_dma);
	if (!ring_space) {
		ret = -ENOMEM;
		goto err_out_cleardev;
	}
	sis_priv->tx_ring = ring_space;
	sis_priv->tx_ring_dma = ring_dma;

	ring_space = pci_alloc_consistent(pci_dev, RX_TOTAL_SIZE, &ring_dma);
	if (!ring_space) {
		ret = -ENOMEM;
		goto err_unmap_tx;
	}
	sis_priv->rx_ring = ring_space;
	sis_priv->rx_ring_dma = ring_dma;

	
	net_dev->netdev_ops = &sis900_netdev_ops;
	net_dev->watchdog_timeo = TX_TIMEOUT;
	net_dev->ethtool_ops = &sis900_ethtool_ops;

	if (sis900_debug > 0)
		sis_priv->msg_enable = sis900_debug;
	else
		sis_priv->msg_enable = SIS900_DEF_MSG;

	sis_priv->mii_info.dev = net_dev;
	sis_priv->mii_info.mdio_read = mdio_read;
	sis_priv->mii_info.mdio_write = mdio_write;
	sis_priv->mii_info.phy_id_mask = 0x1f;
	sis_priv->mii_info.reg_num_mask = 0x1f;

	
	sis_priv->chipset_rev = pci_dev->revision;
	if(netif_msg_probe(sis_priv))
		printk(KERN_DEBUG "%s: detected revision %2.2x, "
				"trying to get MAC address...\n",
				dev_name, sis_priv->chipset_rev);

	ret = 0;
	if (sis_priv->chipset_rev == SIS630E_900_REV)
		ret = sis630e_get_mac_addr(pci_dev, net_dev);
	else if ((sis_priv->chipset_rev > 0x81) && (sis_priv->chipset_rev <= 0x90) )
		ret = sis635_get_mac_addr(pci_dev, net_dev);
	else if (sis_priv->chipset_rev == SIS96x_900_REV)
		ret = sis96x_get_mac_addr(pci_dev, net_dev);
	else
		ret = sis900_get_mac_addr(pci_dev, net_dev);

	if (!ret || !is_valid_ether_addr(net_dev->dev_addr)) {
		eth_hw_addr_random(net_dev);
		printk(KERN_WARNING "%s: Unreadable or invalid MAC address,"
				"using random generated one\n", dev_name);
	}

	
	if (sis_priv->chipset_rev == SIS630ET_900_REV)
		outl(ACCESSMODE | inl(ioaddr + cr), ioaddr + cr);

	
	if (sis900_mii_probe(net_dev) == 0) {
		printk(KERN_WARNING "%s: Error probing MII device.\n",
		       dev_name);
		ret = -ENODEV;
		goto err_unmap_rx;
	}

	
	dev = pci_get_device(PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_630, NULL);
	if (dev) {
		sis_priv->host_bridge_rev = dev->revision;
		pci_dev_put(dev);
	}

	ret = register_netdev(net_dev);
	if (ret)
		goto err_unmap_rx;

	
	printk(KERN_INFO "%s: %s at %#lx, IRQ %d, %pM\n",
	       net_dev->name, card_name, ioaddr, net_dev->irq,
	       net_dev->dev_addr);

	
	ret = (inl(net_dev->base_addr + CFGPMC) & PMESP) >> 27;
	if (netif_msg_probe(sis_priv) && (ret & PME_D3C) == 0)
		printk(KERN_INFO "%s: Wake on LAN only available from suspend to RAM.", net_dev->name);

	return 0;

 err_unmap_rx:
	pci_free_consistent(pci_dev, RX_TOTAL_SIZE, sis_priv->rx_ring,
		sis_priv->rx_ring_dma);
 err_unmap_tx:
	pci_free_consistent(pci_dev, TX_TOTAL_SIZE, sis_priv->tx_ring,
		sis_priv->tx_ring_dma);
 err_out_cleardev:
 	pci_set_drvdata(pci_dev, NULL);
	pci_release_regions(pci_dev);
 err_out:
	free_netdev(net_dev);
	return ret;
}


static int __devinit sis900_mii_probe(struct net_device * net_dev)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	const char *dev_name = pci_name(sis_priv->pci_dev);
	u16 poll_bit = MII_STAT_LINK, status = 0;
	unsigned long timeout = jiffies + 5 * HZ;
	int phy_addr;

	sis_priv->mii = NULL;

	
	for (phy_addr = 0; phy_addr < 32; phy_addr++) {
		struct mii_phy * mii_phy = NULL;
		u16 mii_status;
		int i;

		mii_phy = NULL;
		for(i = 0; i < 2; i++)
			mii_status = mdio_read(net_dev, phy_addr, MII_STATUS);

		if (mii_status == 0xffff || mii_status == 0x0000) {
			if (netif_msg_probe(sis_priv))
				printk(KERN_DEBUG "%s: MII at address %d"
						" not accessible\n",
						dev_name, phy_addr);
			continue;
		}

		if ((mii_phy = kmalloc(sizeof(struct mii_phy), GFP_KERNEL)) == NULL) {
			mii_phy = sis_priv->first_mii;
			while (mii_phy) {
				struct mii_phy *phy;
				phy = mii_phy;
				mii_phy = mii_phy->next;
				kfree(phy);
			}
			return 0;
		}

		mii_phy->phy_id0 = mdio_read(net_dev, phy_addr, MII_PHY_ID0);
		mii_phy->phy_id1 = mdio_read(net_dev, phy_addr, MII_PHY_ID1);
		mii_phy->phy_addr = phy_addr;
		mii_phy->status = mii_status;
		mii_phy->next = sis_priv->mii;
		sis_priv->mii = mii_phy;
		sis_priv->first_mii = mii_phy;

		for (i = 0; mii_chip_table[i].phy_id1; i++)
			if ((mii_phy->phy_id0 == mii_chip_table[i].phy_id0 ) &&
			    ((mii_phy->phy_id1 & 0xFFF0) == mii_chip_table[i].phy_id1)){
				mii_phy->phy_types = mii_chip_table[i].phy_types;
				if (mii_chip_table[i].phy_types == MIX)
					mii_phy->phy_types =
					    (mii_status & (MII_STAT_CAN_TX_FDX | MII_STAT_CAN_TX)) ? LAN : HOME;
				printk(KERN_INFO "%s: %s transceiver found "
							"at address %d.\n",
							dev_name,
							mii_chip_table[i].name,
							phy_addr);
				break;
			}

		if( !mii_chip_table[i].phy_id1 ) {
			printk(KERN_INFO "%s: Unknown PHY transceiver found at address %d.\n",
			       dev_name, phy_addr);
			mii_phy->phy_types = UNKNOWN;
		}
	}

	if (sis_priv->mii == NULL) {
		printk(KERN_INFO "%s: No MII transceivers found!\n", dev_name);
		return 0;
	}

	
	sis_priv->mii = NULL;
	sis900_default_phy( net_dev );

	
        if ((sis_priv->mii->phy_id0 == 0x001D) &&
	    ((sis_priv->mii->phy_id1&0xFFF0) == 0x8000))
        	status = sis900_reset_phy(net_dev, sis_priv->cur_phy);

        
        if ((sis_priv->mii->phy_id0 == 0x0015) &&
            ((sis_priv->mii->phy_id1&0xFFF0) == 0xF440))
            	mdio_write(net_dev, sis_priv->cur_phy, 0x0018, 0xD200);

	if(status & MII_STAT_LINK){
		while (poll_bit) {
			yield();

			poll_bit ^= (mdio_read(net_dev, sis_priv->cur_phy, MII_STATUS) & poll_bit);
			if (time_after_eq(jiffies, timeout)) {
				printk(KERN_WARNING "%s: reset phy and link down now\n",
				       dev_name);
				return -ETIME;
			}
		}
	}

	if (sis_priv->chipset_rev == SIS630E_900_REV) {
		
		mdio_write(net_dev, sis_priv->cur_phy, MII_ANADV, 0x05e1);
		mdio_write(net_dev, sis_priv->cur_phy, MII_CONFIG1, 0x22);
		mdio_write(net_dev, sis_priv->cur_phy, MII_CONFIG2, 0xff00);
		mdio_write(net_dev, sis_priv->cur_phy, MII_MASK, 0xffc0);
		
	}

	if (sis_priv->mii->status & MII_STAT_LINK)
		netif_carrier_on(net_dev);
	else
		netif_carrier_off(net_dev);

	return 1;
}


static u16 sis900_default_phy(struct net_device * net_dev)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
 	struct mii_phy *phy = NULL, *phy_home = NULL,
		*default_phy = NULL, *phy_lan = NULL;
	u16 status;

        for (phy=sis_priv->first_mii; phy; phy=phy->next) {
		status = mdio_read(net_dev, phy->phy_addr, MII_STATUS);
		status = mdio_read(net_dev, phy->phy_addr, MII_STATUS);

		
		 if ((status & MII_STAT_LINK) && !default_phy &&
					(phy->phy_types != UNKNOWN))
		 	default_phy = phy;
		 else {
			status = mdio_read(net_dev, phy->phy_addr, MII_CONTROL);
			mdio_write(net_dev, phy->phy_addr, MII_CONTROL,
				status | MII_CNTL_AUTO | MII_CNTL_ISOLATE);
			if (phy->phy_types == HOME)
				phy_home = phy;
			else if(phy->phy_types == LAN)
				phy_lan = phy;
		 }
	}

	if (!default_phy && phy_home)
		default_phy = phy_home;
	else if (!default_phy && phy_lan)
		default_phy = phy_lan;
	else if (!default_phy)
		default_phy = sis_priv->first_mii;

	if (sis_priv->mii != default_phy) {
		sis_priv->mii = default_phy;
		sis_priv->cur_phy = default_phy->phy_addr;
		printk(KERN_INFO "%s: Using transceiver found at address %d as default\n",
		       pci_name(sis_priv->pci_dev), sis_priv->cur_phy);
	}

	sis_priv->mii_info.phy_id = sis_priv->cur_phy;

	status = mdio_read(net_dev, sis_priv->cur_phy, MII_CONTROL);
	status &= (~MII_CNTL_ISOLATE);

	mdio_write(net_dev, sis_priv->cur_phy, MII_CONTROL, status);
	status = mdio_read(net_dev, sis_priv->cur_phy, MII_STATUS);
	status = mdio_read(net_dev, sis_priv->cur_phy, MII_STATUS);

	return status;
}



static void sis900_set_capability(struct net_device *net_dev, struct mii_phy *phy)
{
	u16 cap;
	u16 status;

	status = mdio_read(net_dev, phy->phy_addr, MII_STATUS);
	status = mdio_read(net_dev, phy->phy_addr, MII_STATUS);

	cap = MII_NWAY_CSMA_CD |
		((phy->status & MII_STAT_CAN_TX_FDX)? MII_NWAY_TX_FDX:0) |
		((phy->status & MII_STAT_CAN_TX)    ? MII_NWAY_TX:0) |
		((phy->status & MII_STAT_CAN_T_FDX) ? MII_NWAY_T_FDX:0)|
		((phy->status & MII_STAT_CAN_T)     ? MII_NWAY_T:0);

	mdio_write(net_dev, phy->phy_addr, MII_ANADV, cap);
}


#define eeprom_delay()  inl(ee_addr)


static u16 __devinit read_eeprom(long ioaddr, int location)
{
	int i;
	u16 retval = 0;
	long ee_addr = ioaddr + mear;
	u32 read_cmd = location | EEread;

	outl(0, ee_addr);
	eeprom_delay();
	outl(EECS, ee_addr);
	eeprom_delay();

	
	for (i = 8; i >= 0; i--) {
		u32 dataval = (read_cmd & (1 << i)) ? EEDI | EECS : EECS;
		outl(dataval, ee_addr);
		eeprom_delay();
		outl(dataval | EECLK, ee_addr);
		eeprom_delay();
	}
	outl(EECS, ee_addr);
	eeprom_delay();

	
	for (i = 16; i > 0; i--) {
		outl(EECS, ee_addr);
		eeprom_delay();
		outl(EECS | EECLK, ee_addr);
		eeprom_delay();
		retval = (retval << 1) | ((inl(ee_addr) & EEDO) ? 1 : 0);
		eeprom_delay();
	}

	
	outl(0, ee_addr);
	eeprom_delay();

	return retval;
}

#define mdio_delay()    inl(mdio_addr)

static void mdio_idle(long mdio_addr)
{
	outl(MDIO | MDDIR, mdio_addr);
	mdio_delay();
	outl(MDIO | MDDIR | MDC, mdio_addr);
}

static void mdio_reset(long mdio_addr)
{
	int i;

	for (i = 31; i >= 0; i--) {
		outl(MDDIR | MDIO, mdio_addr);
		mdio_delay();
		outl(MDDIR | MDIO | MDC, mdio_addr);
		mdio_delay();
	}
}


static int mdio_read(struct net_device *net_dev, int phy_id, int location)
{
	long mdio_addr = net_dev->base_addr + mear;
	int mii_cmd = MIIread|(phy_id<<MIIpmdShift)|(location<<MIIregShift);
	u16 retval = 0;
	int i;

	mdio_reset(mdio_addr);
	mdio_idle(mdio_addr);

	for (i = 15; i >= 0; i--) {
		int dataval = (mii_cmd & (1 << i)) ? MDDIR | MDIO : MDDIR;
		outl(dataval, mdio_addr);
		mdio_delay();
		outl(dataval | MDC, mdio_addr);
		mdio_delay();
	}

	
	for (i = 16; i > 0; i--) {
		outl(0, mdio_addr);
		mdio_delay();
		retval = (retval << 1) | ((inl(mdio_addr) & MDIO) ? 1 : 0);
		outl(MDC, mdio_addr);
		mdio_delay();
	}
	outl(0x00, mdio_addr);

	return retval;
}


static void mdio_write(struct net_device *net_dev, int phy_id, int location,
			int value)
{
	long mdio_addr = net_dev->base_addr + mear;
	int mii_cmd = MIIwrite|(phy_id<<MIIpmdShift)|(location<<MIIregShift);
	int i;

	mdio_reset(mdio_addr);
	mdio_idle(mdio_addr);

	
	for (i = 15; i >= 0; i--) {
		int dataval = (mii_cmd & (1 << i)) ? MDDIR | MDIO : MDDIR;
		outb(dataval, mdio_addr);
		mdio_delay();
		outb(dataval | MDC, mdio_addr);
		mdio_delay();
	}
	mdio_delay();

	
	for (i = 15; i >= 0; i--) {
		int dataval = (value & (1 << i)) ? MDDIR | MDIO : MDDIR;
		outl(dataval, mdio_addr);
		mdio_delay();
		outl(dataval | MDC, mdio_addr);
		mdio_delay();
	}
	mdio_delay();

	
	for (i = 2; i > 0; i--) {
		outb(0, mdio_addr);
		mdio_delay();
		outb(MDC, mdio_addr);
		mdio_delay();
	}
	outl(0x00, mdio_addr);
}



static u16 sis900_reset_phy(struct net_device *net_dev, int phy_addr)
{
	int i;
	u16 status;

	for (i = 0; i < 2; i++)
		status = mdio_read(net_dev, phy_addr, MII_STATUS);

	mdio_write( net_dev, phy_addr, MII_CONTROL, MII_CNTL_RESET );

	return status;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void sis900_poll(struct net_device *dev)
{
	disable_irq(dev->irq);
	sis900_interrupt(dev->irq, dev);
	enable_irq(dev->irq);
}
#endif


static int
sis900_open(struct net_device *net_dev)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	long ioaddr = net_dev->base_addr;
	int ret;

	
	sis900_reset(net_dev);

	
	sis630_set_eq(net_dev, sis_priv->chipset_rev);

	ret = request_irq(net_dev->irq, sis900_interrupt, IRQF_SHARED,
						net_dev->name, net_dev);
	if (ret)
		return ret;

	sis900_init_rxfilter(net_dev);

	sis900_init_tx_ring(net_dev);
	sis900_init_rx_ring(net_dev);

	set_rx_mode(net_dev);

	netif_start_queue(net_dev);

	
	sis900_set_mode(ioaddr, HW_SPEED_10_MBPS, FDX_CAPABLE_HALF_SELECTED);

	
	outl((RxSOVR|RxORN|RxERR|RxOK|TxURN|TxERR|TxIDLE), ioaddr + imr);
	outl(RxENA | inl(ioaddr + cr), ioaddr + cr);
	outl(IE, ioaddr + ier);

	sis900_check_mode(net_dev, sis_priv->mii);

	init_timer(&sis_priv->timer);
	sis_priv->timer.expires = jiffies + HZ;
	sis_priv->timer.data = (unsigned long)net_dev;
	sis_priv->timer.function = sis900_timer;
	add_timer(&sis_priv->timer);

	return 0;
}


static void
sis900_init_rxfilter (struct net_device * net_dev)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	long ioaddr = net_dev->base_addr;
	u32 rfcrSave;
	u32 i;

	rfcrSave = inl(rfcr + ioaddr);

	
	outl(rfcrSave & ~RFEN, rfcr + ioaddr);

	
	for (i = 0 ; i < 3 ; i++) {
		u32 w;

		w = (u32) *((u16 *)(net_dev->dev_addr)+i);
		outl((i << RFADDR_shift), ioaddr + rfcr);
		outl(w, ioaddr + rfdr);

		if (netif_msg_hw(sis_priv)) {
			printk(KERN_DEBUG "%s: Receive Filter Addrss[%d]=%x\n",
			       net_dev->name, i, inl(ioaddr + rfdr));
		}
	}

	
	outl(rfcrSave | RFEN, rfcr + ioaddr);
}


static void
sis900_init_tx_ring(struct net_device *net_dev)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	long ioaddr = net_dev->base_addr;
	int i;

	sis_priv->tx_full = 0;
	sis_priv->dirty_tx = sis_priv->cur_tx = 0;

	for (i = 0; i < NUM_TX_DESC; i++) {
		sis_priv->tx_skbuff[i] = NULL;

		sis_priv->tx_ring[i].link = sis_priv->tx_ring_dma +
			((i+1)%NUM_TX_DESC)*sizeof(BufferDesc);
		sis_priv->tx_ring[i].cmdsts = 0;
		sis_priv->tx_ring[i].bufptr = 0;
	}

	
	outl(sis_priv->tx_ring_dma, ioaddr + txdp);
	if (netif_msg_hw(sis_priv))
		printk(KERN_DEBUG "%s: TX descriptor register loaded with: %8.8x\n",
		       net_dev->name, inl(ioaddr + txdp));
}


static void
sis900_init_rx_ring(struct net_device *net_dev)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	long ioaddr = net_dev->base_addr;
	int i;

	sis_priv->cur_rx = 0;
	sis_priv->dirty_rx = 0;

	
	for (i = 0; i < NUM_RX_DESC; i++) {
		sis_priv->rx_skbuff[i] = NULL;

		sis_priv->rx_ring[i].link = sis_priv->rx_ring_dma +
			((i+1)%NUM_RX_DESC)*sizeof(BufferDesc);
		sis_priv->rx_ring[i].cmdsts = 0;
		sis_priv->rx_ring[i].bufptr = 0;
	}

	
	for (i = 0; i < NUM_RX_DESC; i++) {
		struct sk_buff *skb;

		if ((skb = netdev_alloc_skb(net_dev, RX_BUF_SIZE)) == NULL) {
			break;
		}
		sis_priv->rx_skbuff[i] = skb;
		sis_priv->rx_ring[i].cmdsts = RX_BUF_SIZE;
                sis_priv->rx_ring[i].bufptr = pci_map_single(sis_priv->pci_dev,
                        skb->data, RX_BUF_SIZE, PCI_DMA_FROMDEVICE);
	}
	sis_priv->dirty_rx = (unsigned int) (i - NUM_RX_DESC);

	
	outl(sis_priv->rx_ring_dma, ioaddr + rxdp);
	if (netif_msg_hw(sis_priv))
		printk(KERN_DEBUG "%s: RX descriptor register loaded with: %8.8x\n",
		       net_dev->name, inl(ioaddr + rxdp));
}


static void sis630_set_eq(struct net_device *net_dev, u8 revision)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	u16 reg14h, eq_value=0, max_value=0, min_value=0;
	int i, maxcount=10;

	if ( !(revision == SIS630E_900_REV || revision == SIS630EA1_900_REV ||
	       revision == SIS630A_900_REV || revision ==  SIS630ET_900_REV) )
		return;

	if (netif_carrier_ok(net_dev)) {
		reg14h = mdio_read(net_dev, sis_priv->cur_phy, MII_RESV);
		mdio_write(net_dev, sis_priv->cur_phy, MII_RESV,
					(0x2200 | reg14h) & 0xBFFF);
		for (i=0; i < maxcount; i++) {
			eq_value = (0x00F8 & mdio_read(net_dev,
					sis_priv->cur_phy, MII_RESV)) >> 3;
			if (i == 0)
				max_value=min_value=eq_value;
			max_value = (eq_value > max_value) ?
						eq_value : max_value;
			min_value = (eq_value < min_value) ?
						eq_value : min_value;
		}
		
		if (revision == SIS630E_900_REV || revision == SIS630EA1_900_REV ||
		    revision == SIS630ET_900_REV) {
			if (max_value < 5)
				eq_value = max_value;
			else if (max_value >= 5 && max_value < 15)
				eq_value = (max_value == min_value) ?
						max_value+2 : max_value+1;
			else if (max_value >= 15)
				eq_value=(max_value == min_value) ?
						max_value+6 : max_value+5;
		}
		
		if (revision == SIS630A_900_REV &&
		    (sis_priv->host_bridge_rev == SIS630B0 ||
		     sis_priv->host_bridge_rev == SIS630B1)) {
			if (max_value == 0)
				eq_value = 3;
			else
				eq_value = (max_value + min_value + 1)/2;
		}
		
		reg14h = mdio_read(net_dev, sis_priv->cur_phy, MII_RESV);
		reg14h = (reg14h & 0xFF07) | ((eq_value << 3) & 0x00F8);
		reg14h = (reg14h | 0x6000) & 0xFDFF;
		mdio_write(net_dev, sis_priv->cur_phy, MII_RESV, reg14h);
	} else {
		reg14h = mdio_read(net_dev, sis_priv->cur_phy, MII_RESV);
		if (revision == SIS630A_900_REV &&
		    (sis_priv->host_bridge_rev == SIS630B0 ||
		     sis_priv->host_bridge_rev == SIS630B1))
			mdio_write(net_dev, sis_priv->cur_phy, MII_RESV,
						(reg14h | 0x2200) & 0xBFFF);
		else
			mdio_write(net_dev, sis_priv->cur_phy, MII_RESV,
						(reg14h | 0x2000) & 0xBFFF);
	}
}


static void sis900_timer(unsigned long data)
{
	struct net_device *net_dev = (struct net_device *)data;
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	struct mii_phy *mii_phy = sis_priv->mii;
	static const int next_tick = 5*HZ;
	u16 status;

	if (!sis_priv->autong_complete){
		int uninitialized_var(speed), duplex = 0;

		sis900_read_mode(net_dev, &speed, &duplex);
		if (duplex){
			sis900_set_mode(net_dev->base_addr, speed, duplex);
			sis630_set_eq(net_dev, sis_priv->chipset_rev);
			netif_start_queue(net_dev);
		}

		sis_priv->timer.expires = jiffies + HZ;
		add_timer(&sis_priv->timer);
		return;
	}

	status = mdio_read(net_dev, sis_priv->cur_phy, MII_STATUS);
	status = mdio_read(net_dev, sis_priv->cur_phy, MII_STATUS);

	
	if (!netif_carrier_ok(net_dev)) {
	LookForLink:
		
		status = sis900_default_phy(net_dev);
		mii_phy = sis_priv->mii;

		if (status & MII_STAT_LINK){
			sis900_check_mode(net_dev, mii_phy);
			netif_carrier_on(net_dev);
		}
	} else {
	
                if (!(status & MII_STAT_LINK)){
                	netif_carrier_off(net_dev);
			if(netif_msg_link(sis_priv))
                		printk(KERN_INFO "%s: Media Link Off\n", net_dev->name);

                	
                	if ((mii_phy->phy_id0 == 0x001D) &&
			    ((mii_phy->phy_id1 & 0xFFF0) == 0x8000))
               			sis900_reset_phy(net_dev,  sis_priv->cur_phy);

			sis630_set_eq(net_dev, sis_priv->chipset_rev);

                	goto LookForLink;
                }
	}

	sis_priv->timer.expires = jiffies + next_tick;
	add_timer(&sis_priv->timer);
}


static void sis900_check_mode(struct net_device *net_dev, struct mii_phy *mii_phy)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	long ioaddr = net_dev->base_addr;
	int speed, duplex;

	if (mii_phy->phy_types == LAN) {
		outl(~EXD & inl(ioaddr + cfg), ioaddr + cfg);
		sis900_set_capability(net_dev , mii_phy);
		sis900_auto_negotiate(net_dev, sis_priv->cur_phy);
	} else {
		outl(EXD | inl(ioaddr + cfg), ioaddr + cfg);
		speed = HW_SPEED_HOME;
		duplex = FDX_CAPABLE_HALF_SELECTED;
		sis900_set_mode(ioaddr, speed, duplex);
		sis_priv->autong_complete = 1;
	}
}


static void sis900_set_mode (long ioaddr, int speed, int duplex)
{
	u32 tx_flags = 0, rx_flags = 0;

	if (inl(ioaddr + cfg) & EDB_MASTER_EN) {
		tx_flags = TxATP | (DMA_BURST_64 << TxMXDMA_shift) |
					(TX_FILL_THRESH << TxFILLT_shift);
		rx_flags = DMA_BURST_64 << RxMXDMA_shift;
	} else {
		tx_flags = TxATP | (DMA_BURST_512 << TxMXDMA_shift) |
					(TX_FILL_THRESH << TxFILLT_shift);
		rx_flags = DMA_BURST_512 << RxMXDMA_shift;
	}

	if (speed == HW_SPEED_HOME || speed == HW_SPEED_10_MBPS) {
		rx_flags |= (RxDRNT_10 << RxDRNT_shift);
		tx_flags |= (TxDRNT_10 << TxDRNT_shift);
	} else {
		rx_flags |= (RxDRNT_100 << RxDRNT_shift);
		tx_flags |= (TxDRNT_100 << TxDRNT_shift);
	}

	if (duplex == FDX_CAPABLE_FULL_SELECTED) {
		tx_flags |= (TxCSI | TxHBI);
		rx_flags |= RxATX;
	}

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
	
	rx_flags |= RxAJAB;
#endif

	outl (tx_flags, ioaddr + txcfg);
	outl (rx_flags, ioaddr + rxcfg);
}


static void sis900_auto_negotiate(struct net_device *net_dev, int phy_addr)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	int i = 0;
	u32 status;

	for (i = 0; i < 2; i++)
		status = mdio_read(net_dev, phy_addr, MII_STATUS);

	if (!(status & MII_STAT_LINK)){
		if(netif_msg_link(sis_priv))
			printk(KERN_INFO "%s: Media Link Off\n", net_dev->name);
		sis_priv->autong_complete = 1;
		netif_carrier_off(net_dev);
		return;
	}

	
	mdio_write(net_dev, phy_addr, MII_CONTROL,
		   MII_CNTL_AUTO | MII_CNTL_RST_AUTO);
	sis_priv->autong_complete = 0;
}



static void sis900_read_mode(struct net_device *net_dev, int *speed, int *duplex)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	struct mii_phy *phy = sis_priv->mii;
	int phy_addr = sis_priv->cur_phy;
	u32 status;
	u16 autoadv, autorec;
	int i;

	for (i = 0; i < 2; i++)
		status = mdio_read(net_dev, phy_addr, MII_STATUS);

	if (!(status & MII_STAT_LINK))
		return;

	
	autoadv = mdio_read(net_dev, phy_addr, MII_ANADV);
	autorec = mdio_read(net_dev, phy_addr, MII_ANLPAR);
	status = autoadv & autorec;

	*speed = HW_SPEED_10_MBPS;
	*duplex = FDX_CAPABLE_HALF_SELECTED;

	if (status & (MII_NWAY_TX | MII_NWAY_TX_FDX))
		*speed = HW_SPEED_100_MBPS;
	if (status & ( MII_NWAY_TX_FDX | MII_NWAY_T_FDX))
		*duplex = FDX_CAPABLE_FULL_SELECTED;

	sis_priv->autong_complete = 1;

	
	if ((phy->phy_id0 == 0x0000) && ((phy->phy_id1 & 0xFFF0) == 0x8200)) {
		if (mdio_read(net_dev, phy_addr, MII_CONTROL) & MII_CNTL_FDX)
			*duplex = FDX_CAPABLE_FULL_SELECTED;
		if (mdio_read(net_dev, phy_addr, 0x0019) & 0x01)
			*speed = HW_SPEED_100_MBPS;
	}

	if(netif_msg_link(sis_priv))
		printk(KERN_INFO "%s: Media Link On %s %s-duplex\n",
	       				net_dev->name,
	       				*speed == HW_SPEED_100_MBPS ?
	       					"100mbps" : "10mbps",
	       				*duplex == FDX_CAPABLE_FULL_SELECTED ?
	       					"full" : "half");
}


static void sis900_tx_timeout(struct net_device *net_dev)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	long ioaddr = net_dev->base_addr;
	unsigned long flags;
	int i;

	if(netif_msg_tx_err(sis_priv))
		printk(KERN_INFO "%s: Transmit timeout, status %8.8x %8.8x\n",
	       		net_dev->name, inl(ioaddr + cr), inl(ioaddr + isr));

	
	outl(0x0000, ioaddr + imr);

	
	spin_lock_irqsave(&sis_priv->lock, flags);

	
	sis_priv->dirty_tx = sis_priv->cur_tx = 0;
	for (i = 0; i < NUM_TX_DESC; i++) {
		struct sk_buff *skb = sis_priv->tx_skbuff[i];

		if (skb) {
			pci_unmap_single(sis_priv->pci_dev,
				sis_priv->tx_ring[i].bufptr, skb->len,
				PCI_DMA_TODEVICE);
			dev_kfree_skb_irq(skb);
			sis_priv->tx_skbuff[i] = NULL;
			sis_priv->tx_ring[i].cmdsts = 0;
			sis_priv->tx_ring[i].bufptr = 0;
			net_dev->stats.tx_dropped++;
		}
	}
	sis_priv->tx_full = 0;
	netif_wake_queue(net_dev);

	spin_unlock_irqrestore(&sis_priv->lock, flags);

	net_dev->trans_start = jiffies; 

	
	outl(sis_priv->tx_ring_dma, ioaddr + txdp);

	
	outl((RxSOVR|RxORN|RxERR|RxOK|TxURN|TxERR|TxIDLE), ioaddr + imr);
}


static netdev_tx_t
sis900_start_xmit(struct sk_buff *skb, struct net_device *net_dev)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	long ioaddr = net_dev->base_addr;
	unsigned int  entry;
	unsigned long flags;
	unsigned int  index_cur_tx, index_dirty_tx;
	unsigned int  count_dirty_tx;

	
	if(!sis_priv->autong_complete){
		netif_stop_queue(net_dev);
		return NETDEV_TX_BUSY;
	}

	spin_lock_irqsave(&sis_priv->lock, flags);

	
	entry = sis_priv->cur_tx % NUM_TX_DESC;
	sis_priv->tx_skbuff[entry] = skb;

	
	sis_priv->tx_ring[entry].bufptr = pci_map_single(sis_priv->pci_dev,
		skb->data, skb->len, PCI_DMA_TODEVICE);
	sis_priv->tx_ring[entry].cmdsts = (OWN | skb->len);
	outl(TxENA | inl(ioaddr + cr), ioaddr + cr);

	sis_priv->cur_tx ++;
	index_cur_tx = sis_priv->cur_tx;
	index_dirty_tx = sis_priv->dirty_tx;

	for (count_dirty_tx = 0; index_cur_tx != index_dirty_tx; index_dirty_tx++)
		count_dirty_tx ++;

	if (index_cur_tx == index_dirty_tx) {
		
		sis_priv->tx_full = 1;
		netif_stop_queue(net_dev);
	} else if (count_dirty_tx < NUM_TX_DESC) {
		
		netif_start_queue(net_dev);
	} else {
		
		sis_priv->tx_full = 1;
		netif_stop_queue(net_dev);
	}

	spin_unlock_irqrestore(&sis_priv->lock, flags);

	if (netif_msg_tx_queued(sis_priv))
		printk(KERN_DEBUG "%s: Queued Tx packet at %p size %d "
		       "to slot %d.\n",
		       net_dev->name, skb->data, (int)skb->len, entry);

	return NETDEV_TX_OK;
}


static irqreturn_t sis900_interrupt(int irq, void *dev_instance)
{
	struct net_device *net_dev = dev_instance;
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	int boguscnt = max_interrupt_work;
	long ioaddr = net_dev->base_addr;
	u32 status;
	unsigned int handled = 0;

	spin_lock (&sis_priv->lock);

	do {
		status = inl(ioaddr + isr);

		if ((status & (HIBERR|TxURN|TxERR|TxIDLE|RxORN|RxERR|RxOK)) == 0)
			
			break;
		handled = 1;

		
		if (status & (RxORN | RxERR | RxOK))
			
			sis900_rx(net_dev);

		if (status & (TxURN | TxERR | TxIDLE))
			
			sis900_finish_xmit(net_dev);

		
		if (status & HIBERR) {
			if(netif_msg_intr(sis_priv))
				printk(KERN_INFO "%s: Abnormal interrupt, "
					"status %#8.8x.\n", net_dev->name, status);
			break;
		}
		if (--boguscnt < 0) {
			if(netif_msg_intr(sis_priv))
				printk(KERN_INFO "%s: Too much work at interrupt, "
					"interrupt status = %#8.8x.\n",
					net_dev->name, status);
			break;
		}
	} while (1);

	if(netif_msg_intr(sis_priv))
		printk(KERN_DEBUG "%s: exiting interrupt, "
		       "interrupt status = 0x%#8.8x.\n",
		       net_dev->name, inl(ioaddr + isr));

	spin_unlock (&sis_priv->lock);
	return IRQ_RETVAL(handled);
}


static int sis900_rx(struct net_device *net_dev)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	long ioaddr = net_dev->base_addr;
	unsigned int entry = sis_priv->cur_rx % NUM_RX_DESC;
	u32 rx_status = sis_priv->rx_ring[entry].cmdsts;
	int rx_work_limit;

	if (netif_msg_rx_status(sis_priv))
		printk(KERN_DEBUG "sis900_rx, cur_rx:%4.4d, dirty_rx:%4.4d "
		       "status:0x%8.8x\n",
		       sis_priv->cur_rx, sis_priv->dirty_rx, rx_status);
	rx_work_limit = sis_priv->dirty_rx + NUM_RX_DESC - sis_priv->cur_rx;

	while (rx_status & OWN) {
		unsigned int rx_size;
		unsigned int data_size;

		if (--rx_work_limit < 0)
			break;

		data_size = rx_status & DSIZE;
		rx_size = data_size - CRC_SIZE;

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
		
		if ((rx_status & TOOLONG) && data_size <= MAX_FRAME_SIZE)
			rx_status &= (~ ((unsigned int)TOOLONG));
#endif

		if (rx_status & (ABORT|OVERRUN|TOOLONG|RUNT|RXISERR|CRCERR|FAERR)) {
			
			if (netif_msg_rx_err(sis_priv))
				printk(KERN_DEBUG "%s: Corrupted packet "
				       "received, buffer status = 0x%8.8x/%d.\n",
				       net_dev->name, rx_status, data_size);
			net_dev->stats.rx_errors++;
			if (rx_status & OVERRUN)
				net_dev->stats.rx_over_errors++;
			if (rx_status & (TOOLONG|RUNT))
				net_dev->stats.rx_length_errors++;
			if (rx_status & (RXISERR | FAERR))
				net_dev->stats.rx_frame_errors++;
			if (rx_status & CRCERR)
				net_dev->stats.rx_crc_errors++;
			
			sis_priv->rx_ring[entry].cmdsts = RX_BUF_SIZE;
		} else {
			struct sk_buff * skb;
			struct sk_buff * rx_skb;

			pci_unmap_single(sis_priv->pci_dev,
				sis_priv->rx_ring[entry].bufptr, RX_BUF_SIZE,
				PCI_DMA_FROMDEVICE);

			if ((skb = netdev_alloc_skb(net_dev, RX_BUF_SIZE)) == NULL) {
				skb = sis_priv->rx_skbuff[entry];
				net_dev->stats.rx_dropped++;
				goto refill_rx_ring;
			}

			if (sis_priv->rx_skbuff[entry] == NULL) {
				if (netif_msg_rx_err(sis_priv))
					printk(KERN_WARNING "%s: NULL pointer "
					      "encountered in Rx ring\n"
					      "cur_rx:%4.4d, dirty_rx:%4.4d\n",
					      net_dev->name, sis_priv->cur_rx,
					      sis_priv->dirty_rx);
				dev_kfree_skb(skb);
				break;
			}

			
			rx_skb = sis_priv->rx_skbuff[entry];
			skb_put(rx_skb, rx_size);
			rx_skb->protocol = eth_type_trans(rx_skb, net_dev);
			netif_rx(rx_skb);

			
			if ((rx_status & BCAST) == MCAST)
				net_dev->stats.multicast++;
			net_dev->stats.rx_bytes += rx_size;
			net_dev->stats.rx_packets++;
			sis_priv->dirty_rx++;
refill_rx_ring:
			sis_priv->rx_skbuff[entry] = skb;
			sis_priv->rx_ring[entry].cmdsts = RX_BUF_SIZE;
                	sis_priv->rx_ring[entry].bufptr =
				pci_map_single(sis_priv->pci_dev, skb->data,
					RX_BUF_SIZE, PCI_DMA_FROMDEVICE);
		}
		sis_priv->cur_rx++;
		entry = sis_priv->cur_rx % NUM_RX_DESC;
		rx_status = sis_priv->rx_ring[entry].cmdsts;
	} 

	for (; sis_priv->cur_rx != sis_priv->dirty_rx; sis_priv->dirty_rx++) {
		struct sk_buff *skb;

		entry = sis_priv->dirty_rx % NUM_RX_DESC;

		if (sis_priv->rx_skbuff[entry] == NULL) {
			if ((skb = netdev_alloc_skb(net_dev, RX_BUF_SIZE)) == NULL) {
				if (netif_msg_rx_err(sis_priv))
					printk(KERN_INFO "%s: Memory squeeze, "
						"deferring packet.\n",
						net_dev->name);
				net_dev->stats.rx_dropped++;
				break;
			}
			sis_priv->rx_skbuff[entry] = skb;
			sis_priv->rx_ring[entry].cmdsts = RX_BUF_SIZE;
                	sis_priv->rx_ring[entry].bufptr =
				pci_map_single(sis_priv->pci_dev, skb->data,
					RX_BUF_SIZE, PCI_DMA_FROMDEVICE);
		}
	}
	
	outl(RxENA | inl(ioaddr + cr), ioaddr + cr );

	return 0;
}


static void sis900_finish_xmit (struct net_device *net_dev)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);

	for (; sis_priv->dirty_tx != sis_priv->cur_tx; sis_priv->dirty_tx++) {
		struct sk_buff *skb;
		unsigned int entry;
		u32 tx_status;

		entry = sis_priv->dirty_tx % NUM_TX_DESC;
		tx_status = sis_priv->tx_ring[entry].cmdsts;

		if (tx_status & OWN) {
			break;
		}

		if (tx_status & (ABORT | UNDERRUN | OWCOLL)) {
			
			if (netif_msg_tx_err(sis_priv))
				printk(KERN_DEBUG "%s: Transmit "
				       "error, Tx status %8.8x.\n",
				       net_dev->name, tx_status);
			net_dev->stats.tx_errors++;
			if (tx_status & UNDERRUN)
				net_dev->stats.tx_fifo_errors++;
			if (tx_status & ABORT)
				net_dev->stats.tx_aborted_errors++;
			if (tx_status & NOCARRIER)
				net_dev->stats.tx_carrier_errors++;
			if (tx_status & OWCOLL)
				net_dev->stats.tx_window_errors++;
		} else {
			
			net_dev->stats.collisions += (tx_status & COLCNT) >> 16;
			net_dev->stats.tx_bytes += tx_status & DSIZE;
			net_dev->stats.tx_packets++;
		}
		
		skb = sis_priv->tx_skbuff[entry];
		pci_unmap_single(sis_priv->pci_dev,
			sis_priv->tx_ring[entry].bufptr, skb->len,
			PCI_DMA_TODEVICE);
		dev_kfree_skb_irq(skb);
		sis_priv->tx_skbuff[entry] = NULL;
		sis_priv->tx_ring[entry].bufptr = 0;
		sis_priv->tx_ring[entry].cmdsts = 0;
	}

	if (sis_priv->tx_full && netif_queue_stopped(net_dev) &&
	    sis_priv->cur_tx - sis_priv->dirty_tx < NUM_TX_DESC - 4) {
		sis_priv->tx_full = 0;
		netif_wake_queue (net_dev);
	}
}


static int sis900_close(struct net_device *net_dev)
{
	long ioaddr = net_dev->base_addr;
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	struct sk_buff *skb;
	int i;

	netif_stop_queue(net_dev);

	
	outl(0x0000, ioaddr + imr);
	outl(0x0000, ioaddr + ier);

	
	outl(RxDIS | TxDIS | inl(ioaddr + cr), ioaddr + cr);

	del_timer(&sis_priv->timer);

	free_irq(net_dev->irq, net_dev);

	
	for (i = 0; i < NUM_RX_DESC; i++) {
		skb = sis_priv->rx_skbuff[i];
		if (skb) {
			pci_unmap_single(sis_priv->pci_dev,
				sis_priv->rx_ring[i].bufptr,
				RX_BUF_SIZE, PCI_DMA_FROMDEVICE);
			dev_kfree_skb(skb);
			sis_priv->rx_skbuff[i] = NULL;
		}
	}
	for (i = 0; i < NUM_TX_DESC; i++) {
		skb = sis_priv->tx_skbuff[i];
		if (skb) {
			pci_unmap_single(sis_priv->pci_dev,
				sis_priv->tx_ring[i].bufptr, skb->len,
				PCI_DMA_TODEVICE);
			dev_kfree_skb(skb);
			sis_priv->tx_skbuff[i] = NULL;
		}
	}

	

	return 0;
}


static void sis900_get_drvinfo(struct net_device *net_dev,
			       struct ethtool_drvinfo *info)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);

	strlcpy(info->driver, SIS900_MODULE_NAME, sizeof(info->driver));
	strlcpy(info->version, SIS900_DRV_VERSION, sizeof(info->version));
	strlcpy(info->bus_info, pci_name(sis_priv->pci_dev),
		sizeof(info->bus_info));
}

static u32 sis900_get_msglevel(struct net_device *net_dev)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	return sis_priv->msg_enable;
}

static void sis900_set_msglevel(struct net_device *net_dev, u32 value)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	sis_priv->msg_enable = value;
}

static u32 sis900_get_link(struct net_device *net_dev)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	return mii_link_ok(&sis_priv->mii_info);
}

static int sis900_get_settings(struct net_device *net_dev,
				struct ethtool_cmd *cmd)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	spin_lock_irq(&sis_priv->lock);
	mii_ethtool_gset(&sis_priv->mii_info, cmd);
	spin_unlock_irq(&sis_priv->lock);
	return 0;
}

static int sis900_set_settings(struct net_device *net_dev,
				struct ethtool_cmd *cmd)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	int rt;
	spin_lock_irq(&sis_priv->lock);
	rt = mii_ethtool_sset(&sis_priv->mii_info, cmd);
	spin_unlock_irq(&sis_priv->lock);
	return rt;
}

static int sis900_nway_reset(struct net_device *net_dev)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	return mii_nway_restart(&sis_priv->mii_info);
}


static int sis900_set_wol(struct net_device *net_dev, struct ethtool_wolinfo *wol)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	long pmctrl_addr = net_dev->base_addr + pmctrl;
	u32 cfgpmcsr = 0, pmctrl_bits = 0;

	if (wol->wolopts == 0) {
		pci_read_config_dword(sis_priv->pci_dev, CFGPMCSR, &cfgpmcsr);
		cfgpmcsr &= ~PME_EN;
		pci_write_config_dword(sis_priv->pci_dev, CFGPMCSR, cfgpmcsr);
		outl(pmctrl_bits, pmctrl_addr);
		if (netif_msg_wol(sis_priv))
			printk(KERN_DEBUG "%s: Wake on LAN disabled\n", net_dev->name);
		return 0;
	}

	if (wol->wolopts & (WAKE_MAGICSECURE | WAKE_UCAST | WAKE_MCAST
				| WAKE_BCAST | WAKE_ARP))
		return -EINVAL;

	if (wol->wolopts & WAKE_MAGIC)
		pmctrl_bits |= MAGICPKT;
	if (wol->wolopts & WAKE_PHY)
		pmctrl_bits |= LINKON;

	outl(pmctrl_bits, pmctrl_addr);

	pci_read_config_dword(sis_priv->pci_dev, CFGPMCSR, &cfgpmcsr);
	cfgpmcsr |= PME_EN;
	pci_write_config_dword(sis_priv->pci_dev, CFGPMCSR, cfgpmcsr);
	if (netif_msg_wol(sis_priv))
		printk(KERN_DEBUG "%s: Wake on LAN enabled\n", net_dev->name);

	return 0;
}

static void sis900_get_wol(struct net_device *net_dev, struct ethtool_wolinfo *wol)
{
	long pmctrl_addr = net_dev->base_addr + pmctrl;
	u32 pmctrl_bits;

	pmctrl_bits = inl(pmctrl_addr);
	if (pmctrl_bits & MAGICPKT)
		wol->wolopts |= WAKE_MAGIC;
	if (pmctrl_bits & LINKON)
		wol->wolopts |= WAKE_PHY;

	wol->supported = (WAKE_PHY | WAKE_MAGIC);
}

static const struct ethtool_ops sis900_ethtool_ops = {
	.get_drvinfo 	= sis900_get_drvinfo,
	.get_msglevel	= sis900_get_msglevel,
	.set_msglevel	= sis900_set_msglevel,
	.get_link	= sis900_get_link,
	.get_settings	= sis900_get_settings,
	.set_settings	= sis900_set_settings,
	.nway_reset	= sis900_nway_reset,
	.get_wol	= sis900_get_wol,
	.set_wol	= sis900_set_wol
};


static int mii_ioctl(struct net_device *net_dev, struct ifreq *rq, int cmd)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	struct mii_ioctl_data *data = if_mii(rq);

	switch(cmd) {
	case SIOCGMIIPHY:		
		data->phy_id = sis_priv->mii->phy_addr;
		

	case SIOCGMIIREG:		
		data->val_out = mdio_read(net_dev, data->phy_id & 0x1f, data->reg_num & 0x1f);
		return 0;

	case SIOCSMIIREG:		
		mdio_write(net_dev, data->phy_id & 0x1f, data->reg_num & 0x1f, data->val_in);
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}


static int sis900_set_config(struct net_device *dev, struct ifmap *map)
{
	struct sis900_private *sis_priv = netdev_priv(dev);
	struct mii_phy *mii_phy = sis_priv->mii;

	u16 status;

	if ((map->port != (u_char)(-1)) && (map->port != dev->if_port)) {
		switch(map->port){
		case IF_PORT_UNKNOWN: 
			dev->if_port = map->port;
			netif_carrier_off(dev);

			
			status = mdio_read(dev, mii_phy->phy_addr, MII_CONTROL);

			mdio_write(dev, mii_phy->phy_addr,
				   MII_CONTROL, status | MII_CNTL_AUTO | MII_CNTL_RST_AUTO);

			break;

		case IF_PORT_10BASET: 
			dev->if_port = map->port;

			netif_carrier_off(dev);

			
			
			status = mdio_read(dev, mii_phy->phy_addr, MII_CONTROL);

			
			mdio_write(dev, mii_phy->phy_addr,
				   MII_CONTROL, status & ~(MII_CNTL_SPEED |
					MII_CNTL_AUTO));
			break;

		case IF_PORT_100BASET: 
		case IF_PORT_100BASETX: 
			dev->if_port = map->port;

			netif_carrier_off(dev);

			
			
			status = mdio_read(dev, mii_phy->phy_addr, MII_CONTROL);
			mdio_write(dev, mii_phy->phy_addr,
				   MII_CONTROL, (status & ~MII_CNTL_SPEED) |
				   MII_CNTL_SPEED);

			break;

		case IF_PORT_10BASE2: 
		case IF_PORT_AUI: 
		case IF_PORT_100BASEFX: 
                	
			return -EOPNOTSUPP;
			break;

		default:
			return -EINVAL;
		}
	}
	return 0;
}


static inline u16 sis900_mcast_bitnr(u8 *addr, u8 revision)
{

	u32 crc = ether_crc(6, addr);

	
	if ((revision >= SIS635A_900_REV) || (revision == SIS900B_900_REV))
		return (int)(crc >> 24);
	else
		return (int)(crc >> 25);
}


static void set_rx_mode(struct net_device *net_dev)
{
	long ioaddr = net_dev->base_addr;
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	u16 mc_filter[16] = {0};	
	int i, table_entries;
	u32 rx_mode;

	
	if((sis_priv->chipset_rev >= SIS635A_900_REV) ||
			(sis_priv->chipset_rev == SIS900B_900_REV))
		table_entries = 16;
	else
		table_entries = 8;

	if (net_dev->flags & IFF_PROMISC) {
		
		rx_mode = RFPromiscuous;
		for (i = 0; i < table_entries; i++)
			mc_filter[i] = 0xffff;
	} else if ((netdev_mc_count(net_dev) > multicast_filter_limit) ||
		   (net_dev->flags & IFF_ALLMULTI)) {
		
		rx_mode = RFAAB | RFAAM;
		for (i = 0; i < table_entries; i++)
			mc_filter[i] = 0xffff;
	} else {
		struct netdev_hw_addr *ha;
		rx_mode = RFAAB;

		netdev_for_each_mc_addr(ha, net_dev) {
			unsigned int bit_nr;

			bit_nr = sis900_mcast_bitnr(ha->addr,
						    sis_priv->chipset_rev);
			mc_filter[bit_nr >> 4] |= (1 << (bit_nr & 0xf));
		}
	}

	
	for (i = 0; i < table_entries; i++) {
                
		outl((u32)(0x00000004+i) << RFADDR_shift, ioaddr + rfcr);
		outl(mc_filter[i], ioaddr + rfdr);
	}

	outl(RFEN | rx_mode, ioaddr + rfcr);

	if (net_dev->flags & IFF_LOOPBACK) {
		u32 cr_saved;
		
		cr_saved = inl(ioaddr + cr);
		outl(cr_saved | TxDIS | RxDIS, ioaddr + cr);
		
		outl(inl(ioaddr + txcfg) | TxMLB, ioaddr + txcfg);
		outl(inl(ioaddr + rxcfg) | RxATX, ioaddr + rxcfg);
		
		outl(cr_saved, ioaddr + cr);
	}
}


static void sis900_reset(struct net_device *net_dev)
{
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	long ioaddr = net_dev->base_addr;
	int i = 0;
	u32 status = TxRCMP | RxRCMP;

	outl(0, ioaddr + ier);
	outl(0, ioaddr + imr);
	outl(0, ioaddr + rfcr);

	outl(RxRESET | TxRESET | RESET | inl(ioaddr + cr), ioaddr + cr);

	
	while (status && (i++ < 1000)) {
		status ^= (inl(isr + ioaddr) & status);
	}

	if( (sis_priv->chipset_rev >= SIS635A_900_REV) ||
			(sis_priv->chipset_rev == SIS900B_900_REV) )
		outl(PESEL | RND_CNT, ioaddr + cfg);
	else
		outl(PESEL, ioaddr + cfg);
}


static void __devexit sis900_remove(struct pci_dev *pci_dev)
{
	struct net_device *net_dev = pci_get_drvdata(pci_dev);
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	struct mii_phy *phy = NULL;

	while (sis_priv->first_mii) {
		phy = sis_priv->first_mii;
		sis_priv->first_mii = phy->next;
		kfree(phy);
	}

	pci_free_consistent(pci_dev, RX_TOTAL_SIZE, sis_priv->rx_ring,
		sis_priv->rx_ring_dma);
	pci_free_consistent(pci_dev, TX_TOTAL_SIZE, sis_priv->tx_ring,
		sis_priv->tx_ring_dma);
	unregister_netdev(net_dev);
	free_netdev(net_dev);
	pci_release_regions(pci_dev);
	pci_set_drvdata(pci_dev, NULL);
}

#ifdef CONFIG_PM

static int sis900_suspend(struct pci_dev *pci_dev, pm_message_t state)
{
	struct net_device *net_dev = pci_get_drvdata(pci_dev);
	long ioaddr = net_dev->base_addr;

	if(!netif_running(net_dev))
		return 0;

	netif_stop_queue(net_dev);
	netif_device_detach(net_dev);

	
	outl(RxDIS | TxDIS | inl(ioaddr + cr), ioaddr + cr);

	pci_set_power_state(pci_dev, PCI_D3hot);
	pci_save_state(pci_dev);

	return 0;
}

static int sis900_resume(struct pci_dev *pci_dev)
{
	struct net_device *net_dev = pci_get_drvdata(pci_dev);
	struct sis900_private *sis_priv = netdev_priv(net_dev);
	long ioaddr = net_dev->base_addr;

	if(!netif_running(net_dev))
		return 0;
	pci_restore_state(pci_dev);
	pci_set_power_state(pci_dev, PCI_D0);

	sis900_init_rxfilter(net_dev);

	sis900_init_tx_ring(net_dev);
	sis900_init_rx_ring(net_dev);

	set_rx_mode(net_dev);

	netif_device_attach(net_dev);
	netif_start_queue(net_dev);

	
	sis900_set_mode(ioaddr, HW_SPEED_10_MBPS, FDX_CAPABLE_HALF_SELECTED);

	
	outl((RxSOVR|RxORN|RxERR|RxOK|TxURN|TxERR|TxIDLE), ioaddr + imr);
	outl(RxENA | inl(ioaddr + cr), ioaddr + cr);
	outl(IE, ioaddr + ier);

	sis900_check_mode(net_dev, sis_priv->mii);

	return 0;
}
#endif 

static struct pci_driver sis900_pci_driver = {
	.name		= SIS900_MODULE_NAME,
	.id_table	= sis900_pci_tbl,
	.probe		= sis900_probe,
	.remove		= __devexit_p(sis900_remove),
#ifdef CONFIG_PM
	.suspend	= sis900_suspend,
	.resume		= sis900_resume,
#endif 
};

static int __init sis900_init_module(void)
{
#ifdef MODULE
	printk(version);
#endif

	return pci_register_driver(&sis900_pci_driver);
}

static void __exit sis900_cleanup_module(void)
{
	pci_unregister_driver(&sis900_pci_driver);
}

module_init(sis900_init_module);
module_exit(sis900_cleanup_module);

