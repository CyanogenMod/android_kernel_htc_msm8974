/*
	Written 1996 by Russell Nelson, with reference to skeleton.c
	written 1993-1994 by Donald Becker.

	This software may be used and distributed according to the terms
	of the GNU General Public License, incorporated herein by reference.

	The author may be reached at nelson@crynwr.com, Crynwr
	Software, 11 Grant St., Potsdam, NY 13676

  Changelog:

  Mike Cruse        : mcruse@cti-ltd.com
                    : Changes for Linux 2.0 compatibility.
                    : Added dev_id parameter in net_interrupt(),
                    : request_irq() and free_irq(). Just NULL for now.

  Mike Cruse        : Added MOD_INC_USE_COUNT and MOD_DEC_USE_COUNT macros
                    : in net_open() and net_close() so kerneld would know
                    : that the module is in use and wouldn't eject the
                    : driver prematurely.

  Mike Cruse        : Rewrote init_module() and cleanup_module using 8390.c
                    : as an example. Disabled autoprobing in init_module(),
                    : not a good thing to do to other devices while Linux
                    : is running from all accounts.

  Alan Cox          : Removed 1.2 support, added 2.1 extra counters.

  David Huggins-Daines <dhd@debian.org>

  Split this off into mac89x0.c, and gutted it of all parts which are
  not relevant to the existing CS8900 cards on the Macintosh
  (i.e. basically the Daynaport CS and LC cards).  To be precise:

    * Removed all the media-detection stuff, because these cards are
    TP-only.

    * Lobotomized the ISA interrupt bogosity, because these cards use
    a hardwired NuBus interrupt and a magic ISAIRQ value in the card.

    * Basically eliminated everything not relevant to getting the
    cards minimally functioning on the Macintosh.

  I might add that these cards are badly designed even from the Mac
  standpoint, in that Dayna, in their infinite wisdom, used NuBus slot
  I/O space and NuBus interrupts for these cards, but neglected to
  provide anything even remotely resembling a NuBus ROM.  Therefore we
  have to probe for them in a brain-damaged ISA-like fashion.

  Arnaldo Carvalho de Melo <acme@conectiva.com.br> - 11/01/2001
  check kmalloc and release the allocated memory on failure in
  mac89x0_probe and in init_module
  use local_irq_{save,restore}(flags) in net_get_stat, not just
  local_irq_{dis,en}able()
*/

static char *version =
"cs89x0.c:v1.02 11/26/96 Russell Nelson <nelson@crynwr.com>\n";


#ifndef NET_DEBUG
#define NET_DEBUG 0
#endif



#include <linux/module.h>


#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/string.h>
#include <linux/nubus.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/gfp.h>

#include <asm/io.h>
#include <asm/hwtest.h>
#include <asm/macints.h>

#include "cs89x0.h"

static unsigned int net_debug = NET_DEBUG;

struct net_local {
	int chip_type;		
	char chip_revision;	
	int send_cmd;		
	int rx_mode;
	int curr_rx_cfg;
        int send_underrun;      
	struct sk_buff *skb;
};


#if 0
extern void reset_chip(struct net_device *dev);
#endif
static int net_open(struct net_device *dev);
static int net_send_packet(struct sk_buff *skb, struct net_device *dev);
static irqreturn_t net_interrupt(int irq, void *dev_id);
static void set_multicast_list(struct net_device *dev);
static void net_rx(struct net_device *dev);
static int net_close(struct net_device *dev);
static struct net_device_stats *net_get_stats(struct net_device *dev);
static int set_mac_address(struct net_device *dev, void *addr);


#define tx_done(dev) 1

static inline int
readreg_io(struct net_device *dev, int portno)
{
	nubus_writew(swab16(portno), dev->base_addr + ADD_PORT);
	return swab16(nubus_readw(dev->base_addr + DATA_PORT));
}

static inline void
writereg_io(struct net_device *dev, int portno, int value)
{
	nubus_writew(swab16(portno), dev->base_addr + ADD_PORT);
	nubus_writew(swab16(value), dev->base_addr + DATA_PORT);
}

static inline int
readreg(struct net_device *dev, int portno)
{
	return swab16(nubus_readw(dev->mem_start + portno));
}

static inline void
writereg(struct net_device *dev, int portno, int value)
{
	nubus_writew(swab16(value), dev->mem_start + portno);
}

static const struct net_device_ops mac89x0_netdev_ops = {
	.ndo_open		= net_open,
	.ndo_stop		= net_close,
	.ndo_start_xmit		= net_send_packet,
	.ndo_get_stats		= net_get_stats,
	.ndo_set_rx_mode	= set_multicast_list,
	.ndo_set_mac_address	= set_mac_address,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_change_mtu		= eth_change_mtu,
};

struct net_device * __init mac89x0_probe(int unit)
{
	struct net_device *dev;
	static int once_is_enough;
	struct net_local *lp;
	static unsigned version_printed;
	int i, slot;
	unsigned rev_type = 0;
	unsigned long ioaddr;
	unsigned short sig;
	int err = -ENODEV;

	if (!MACH_IS_MAC)
		return ERR_PTR(-ENODEV);

	dev = alloc_etherdev(sizeof(struct net_local));
	if (!dev)
		return ERR_PTR(-ENOMEM);

	if (unit >= 0) {
		sprintf(dev->name, "eth%d", unit);
		netdev_boot_setup_check(dev);
	}

	if (once_is_enough)
		goto out;
	once_is_enough = 1;

	
	slot = 0xE;
	
	if (nubus_find_slot(slot, NULL) != NULL)
		goto out;

	ioaddr = (unsigned long)
		nubus_slot_addr(slot) | (((slot&0xf) << 20) + DEFAULTIOBASE);
	{
		unsigned long flags;
		int card_present;

		local_irq_save(flags);
		card_present = (hwreg_present((void*) ioaddr+4) &&
				hwreg_present((void*) ioaddr + DATA_PORT));
		local_irq_restore(flags);

		if (!card_present)
			goto out;
	}

	nubus_writew(0, ioaddr + ADD_PORT);
	sig = nubus_readw(ioaddr + DATA_PORT);
	if (sig != swab16(CHIP_EISA_ID_SIG))
		goto out;

	
	lp = netdev_priv(dev);

	
	dev->base_addr = ioaddr;
	dev->mem_start = (unsigned long)
		nubus_slot_addr(slot) | (((slot&0xf) << 20) + MMIOBASE);
	dev->mem_end = dev->mem_start + 0x1000;

	
	writereg_io(dev, PP_BusCTL, MEMORY_ON);

	
	rev_type = readreg(dev, PRODUCT_ID_ADD);
	lp->chip_type = rev_type &~ REVISON_BITS;
	lp->chip_revision = ((rev_type & REVISON_BITS) >> 8) + 'A';

	lp->send_cmd = TX_AFTER_381;
	if (lp->chip_type == CS8900 && lp->chip_revision >= 'F')
		lp->send_cmd = TX_NOW;
	if (lp->chip_type != CS8900 && lp->chip_revision >= 'C')
		lp->send_cmd = TX_NOW;

	if (net_debug && version_printed++ == 0)
		printk(version);

	printk(KERN_INFO "%s: cs89%c0%s rev %c found at %#8lx",
	       dev->name,
	       lp->chip_type==CS8900?'0':'2',
	       lp->chip_type==CS8920M?"M":"",
	       lp->chip_revision,
	       dev->base_addr);

	
	if ((readreg(dev, PP_SelfST) & (EEPROM_PRESENT | EEPROM_OK)) == 0) {
		printk("\nmac89x0: No EEPROM, giving up now.\n");
		goto out1;
        } else {
                for (i = 0; i < ETH_ALEN; i += 2) {
			
			unsigned short s = readreg(dev, PP_IA + i);
                        dev->dev_addr[i] = s >> 8;
                        dev->dev_addr[i+1] = s & 0xff;
                }
        }

	dev->irq = SLOT2IRQ(slot);

	

	printk(" IRQ %d ADDR %pM\n", dev->irq, dev->dev_addr);

	dev->netdev_ops		= &mac89x0_netdev_ops;

	err = register_netdev(dev);
	if (err)
		goto out1;
	return NULL;
out1:
	nubus_writew(0, dev->base_addr + ADD_PORT);
out:
	free_netdev(dev);
	return ERR_PTR(err);
}

#if 0
void __init reset_chip(struct net_device *dev)
{
	int reset_start_time;

	writereg(dev, PP_SelfCTL, readreg(dev, PP_SelfCTL) | POWER_ON_RESET);

	
	msleep_interruptible(30);

	
	reset_start_time = jiffies;
	while( (readreg(dev, PP_SelfST) & INIT_DONE) == 0 && jiffies - reset_start_time < 2)
		;
}
#endif

static int
net_open(struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);
	int i;

	
	writereg(dev, PP_BusCTL, readreg(dev, PP_BusCTL) & ~ENABLE_IRQ);

	
	if (request_irq(dev->irq, net_interrupt, 0, "cs89x0", dev))
		return -EAGAIN;

	
	if (lp->chip_type == CS8900)
		writereg(dev, PP_CS8900_ISAINT, 0);
	else
		writereg(dev, PP_CS8920_ISAINT, 0);

	
	for (i=0; i < ETH_ALEN/2; i++)
		writereg(dev, PP_IA+i*2, dev->dev_addr[i*2] | (dev->dev_addr[i*2+1] << 8));

	
	writereg(dev, PP_LineCTL, readreg(dev, PP_LineCTL) | SERIAL_RX_ON | SERIAL_TX_ON);

	
	lp->rx_mode = 0;
	writereg(dev, PP_RxCTL, DEF_RX_ACCEPT);

	lp->curr_rx_cfg = RX_OK_ENBL | RX_CRC_ERROR_ENBL;

	writereg(dev, PP_RxCFG, lp->curr_rx_cfg);

	writereg(dev, PP_TxCFG, TX_LOST_CRS_ENBL | TX_SQE_ERROR_ENBL | TX_OK_ENBL |
	       TX_LATE_COL_ENBL | TX_JBR_ENBL | TX_ANY_COL_ENBL | TX_16_COL_ENBL);

	writereg(dev, PP_BufCFG, READY_FOR_TX_ENBL | RX_MISS_COUNT_OVRFLOW_ENBL |
		 TX_COL_COUNT_OVRFLOW_ENBL | TX_UNDERRUN_ENBL);

	
	writereg(dev, PP_BusCTL, readreg(dev, PP_BusCTL) | ENABLE_IRQ);
	netif_start_queue(dev);
	return 0;
}

static int
net_send_packet(struct sk_buff *skb, struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);
	unsigned long flags;

	if (net_debug > 3)
		printk("%s: sent %d byte packet of type %x\n",
		       dev->name, skb->len,
		       (skb->data[ETH_ALEN+ETH_ALEN] << 8)
		       | skb->data[ETH_ALEN+ETH_ALEN+1]);

	local_irq_save(flags);
	netif_stop_queue(dev);

	
	writereg(dev, PP_TxCMD, lp->send_cmd);
	writereg(dev, PP_TxLength, skb->len);

	
	if ((readreg(dev, PP_BusST) & READY_FOR_TX_NOW) == 0) {
		local_irq_restore(flags);
		return NETDEV_TX_BUSY;
	}

	
	skb_copy_from_linear_data(skb, (void *)(dev->mem_start + PP_TxFrame),
				  skb->len+1);

	local_irq_restore(flags);
	dev_kfree_skb (skb);

	return NETDEV_TX_OK;
}

static irqreturn_t net_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct net_local *lp;
	int ioaddr, status;

	if (dev == NULL) {
		printk ("net_interrupt(): irq %d for unknown device.\n", irq);
		return IRQ_NONE;
	}

	ioaddr = dev->base_addr;
	lp = netdev_priv(dev);

	while ((status = swab16(nubus_readw(dev->base_addr + ISQ_PORT)))) {
		if (net_debug > 4)printk("%s: event=%04x\n", dev->name, status);
		switch(status & ISQ_EVENT_MASK) {
		case ISQ_RECEIVER_EVENT:
			
			net_rx(dev);
			break;
		case ISQ_TRANSMITTER_EVENT:
			dev->stats.tx_packets++;
			netif_wake_queue(dev);
			if ((status & TX_OK) == 0)
				dev->stats.tx_errors++;
			if (status & TX_LOST_CRS)
				dev->stats.tx_carrier_errors++;
			if (status & TX_SQE_ERROR)
				dev->stats.tx_heartbeat_errors++;
			if (status & TX_LATE_COL)
				dev->stats.tx_window_errors++;
			if (status & TX_16_COL)
				dev->stats.tx_aborted_errors++;
			break;
		case ISQ_BUFFER_EVENT:
			if (status & READY_FOR_TX) {
				netif_wake_queue(dev);
			}
			if (status & TX_UNDERRUN) {
				if (net_debug > 0) printk("%s: transmit underrun\n", dev->name);
                                lp->send_underrun++;
                                if (lp->send_underrun == 3) lp->send_cmd = TX_AFTER_381;
                                else if (lp->send_underrun == 6) lp->send_cmd = TX_AFTER_ALL;
                        }
			break;
		case ISQ_RX_MISS_EVENT:
			dev->stats.rx_missed_errors += (status >> 6);
			break;
		case ISQ_TX_COL_EVENT:
			dev->stats.collisions += (status >> 6);
			break;
		}
	}
	return IRQ_HANDLED;
}

static void
net_rx(struct net_device *dev)
{
	struct sk_buff *skb;
	int status, length;

	status = readreg(dev, PP_RxStatus);
	if ((status & RX_OK) == 0) {
		dev->stats.rx_errors++;
		if (status & RX_RUNT)
				dev->stats.rx_length_errors++;
		if (status & RX_EXTRA_DATA)
				dev->stats.rx_length_errors++;
		if ((status & RX_CRC_ERROR) &&
		    !(status & (RX_EXTRA_DATA|RX_RUNT)))
			
			dev->stats.rx_crc_errors++;
		if (status & RX_DRIBBLE)
				dev->stats.rx_frame_errors++;
		return;
	}

	length = readreg(dev, PP_RxLength);
	
	skb = alloc_skb(length, GFP_ATOMIC);
	if (skb == NULL) {
		printk("%s: Memory squeeze, dropping packet.\n", dev->name);
		dev->stats.rx_dropped++;
		return;
	}
	skb_put(skb, length);

	skb_copy_to_linear_data(skb, (void *)(dev->mem_start + PP_RxFrame),
				length);

	if (net_debug > 3)printk("%s: received %d byte packet of type %x\n",
                                 dev->name, length,
                                 (skb->data[ETH_ALEN+ETH_ALEN] << 8)
				 | skb->data[ETH_ALEN+ETH_ALEN+1]);

        skb->protocol=eth_type_trans(skb,dev);
	netif_rx(skb);
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += length;
}

static int
net_close(struct net_device *dev)
{

	writereg(dev, PP_RxCFG, 0);
	writereg(dev, PP_TxCFG, 0);
	writereg(dev, PP_BufCFG, 0);
	writereg(dev, PP_BusCTL, 0);

	netif_stop_queue(dev);

	free_irq(dev->irq, dev);

	

	return 0;

}

static struct net_device_stats *
net_get_stats(struct net_device *dev)
{
	unsigned long flags;

	local_irq_save(flags);
	
	dev->stats.rx_missed_errors += (readreg(dev, PP_RxMiss) >> 6);
	dev->stats.collisions += (readreg(dev, PP_TxCol) >> 6);
	local_irq_restore(flags);

	return &dev->stats;
}

static void set_multicast_list(struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);

	if(dev->flags&IFF_PROMISC)
	{
		lp->rx_mode = RX_ALL_ACCEPT;
	} else if ((dev->flags & IFF_ALLMULTI) || !netdev_mc_empty(dev)) {
		lp->rx_mode = RX_MULTCAST_ACCEPT;
	}
	else
		lp->rx_mode = 0;

	writereg(dev, PP_RxCTL, DEF_RX_ACCEPT | lp->rx_mode);

	
	writereg(dev, PP_RxCFG, lp->curr_rx_cfg |
	     (lp->rx_mode == RX_ALL_ACCEPT? (RX_CRC_ERROR_ENBL|RX_RUNT_ENBL|RX_EXTRA_DATA_ENBL) : 0));
}


static int set_mac_address(struct net_device *dev, void *addr)
{
	struct sockaddr *saddr = addr;
	int i;

	if (!is_valid_ether_addr(saddr->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(dev->dev_addr, saddr->sa_data, ETH_ALEN);
	printk("%s: Setting MAC address to %pM\n", dev->name, dev->dev_addr);

	
	for (i=0; i < ETH_ALEN/2; i++)
		writereg(dev, PP_IA+i*2, dev->dev_addr[i*2] | (dev->dev_addr[i*2+1] << 8));

	return 0;
}

#ifdef MODULE

static struct net_device *dev_cs89x0;
static int debug;

module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "CS89[02]0 debug level (0-5)");
MODULE_LICENSE("GPL");

int __init
init_module(void)
{
	net_debug = debug;
        dev_cs89x0 = mac89x0_probe(-1);
	if (IS_ERR(dev_cs89x0)) {
                printk(KERN_WARNING "mac89x0.c: No card found\n");
		return PTR_ERR(dev_cs89x0);
	}
	return 0;
}

void
cleanup_module(void)
{
	unregister_netdev(dev_cs89x0);
	nubus_writew(0, dev_cs89x0->base_addr + ADD_PORT);
	free_netdev(dev_cs89x0);
}
#endif 
