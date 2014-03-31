static const char version[] = "de600.c: $Revision: 1.41-2.5 $,  Bjorn Ekwall (bj0rn@blox.se)\n";
/*
 *	de600.c
 *
 *	Linux driver for the D-Link DE-600 Ethernet pocket adapter.
 *
 *	Portions (C) Copyright 1993, 1994 by Bjorn Ekwall
 *	The Author may be reached as bj0rn@blox.se
 *
 *	Based on adapter information gathered from DE600.ASM by D-Link Inc.,
 *	as included on disk C in the v.2.11 of PC/TCP from FTP Software.
 *	For DE600.asm:
 *		Portions (C) Copyright 1990 D-Link, Inc.
 *		Copyright, 1988-1992, Russell Nelson, Crynwr Software
 *
 *	Adapted to the sample network driver core for linux,
 *	written by: Donald Becker <becker@super.org>
 *		(Now at <becker@scyld.com>)
 *
 **************************************************************/
/*
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2, or (at your option)
 *	any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 **************************************************************/

#define DE600_SLOW_DOWN	udelay(delay_time)

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <asm/io.h>

#include "de600.h"

static bool check_lost = true;
module_param(check_lost, bool, 0);
MODULE_PARM_DESC(check_lost, "If set then check for unplugged de600");

static unsigned int delay_time = 10;
module_param(delay_time, int, 0);
MODULE_PARM_DESC(delay_time, "DE-600 deley on I/O in microseconds");



static volatile int		rx_page;

#define TX_PAGES 2
static volatile int		tx_fifo[TX_PAGES];
static volatile int		tx_fifo_in;
static volatile int		tx_fifo_out;
static volatile int		free_tx_pages = TX_PAGES;
static int			was_down;
static DEFINE_SPINLOCK(de600_lock);

static inline u8 de600_read_status(struct net_device *dev)
{
	u8 status;

	outb_p(STATUS, DATA_PORT);
	status = inb(STATUS_PORT);
	outb_p(NULL_COMMAND | HI_NIBBLE, DATA_PORT);

	return status;
}

static inline u8 de600_read_byte(unsigned char type, struct net_device *dev)
{
	
	u8 lo;
	outb_p((type), DATA_PORT);
	lo = ((unsigned char)inb(STATUS_PORT)) >> 4;
	outb_p((type) | HI_NIBBLE, DATA_PORT);
	return ((unsigned char)inb(STATUS_PORT) & (unsigned char)0xf0) | lo;
}


static int de600_open(struct net_device *dev)
{
	unsigned long flags;
	int ret = request_irq(DE600_IRQ, de600_interrupt, 0, dev->name, dev);
	if (ret) {
		printk(KERN_ERR "%s: unable to get IRQ %d\n", dev->name, DE600_IRQ);
		return ret;
	}
	spin_lock_irqsave(&de600_lock, flags);
	ret = adapter_init(dev);
	spin_unlock_irqrestore(&de600_lock, flags);
	return ret;
}


static int de600_close(struct net_device *dev)
{
	select_nic();
	rx_page = 0;
	de600_put_command(RESET);
	de600_put_command(STOP_RESET);
	de600_put_command(0);
	select_prn();
	free_irq(DE600_IRQ, dev);
	return 0;
}

static inline void trigger_interrupt(struct net_device *dev)
{
	de600_put_command(FLIP_IRQ);
	select_prn();
	DE600_SLOW_DOWN;
	select_nic();
	de600_put_command(0);
}


static int de600_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	unsigned long flags;
	int	transmit_from;
	int	len;
	int	tickssofar;
	u8	*buffer = skb->data;
	int	i;

	if (free_tx_pages <= 0) {	
		tickssofar = jiffies - dev_trans_start(dev);
		if (tickssofar < HZ/20)
			return NETDEV_TX_BUSY;
		
		printk(KERN_WARNING "%s: transmit timed out (%d), %s?\n", dev->name, tickssofar, "network cable problem");
		
		spin_lock_irqsave(&de600_lock, flags);
		if (adapter_init(dev)) {
			spin_unlock_irqrestore(&de600_lock, flags);
			return NETDEV_TX_BUSY;
		}
		spin_unlock_irqrestore(&de600_lock, flags);
	}

	
	pr_debug("de600_start_xmit:len=%d, page %d/%d\n", skb->len, tx_fifo_in, free_tx_pages);

	if ((len = skb->len) < RUNT)
		len = RUNT;

	spin_lock_irqsave(&de600_lock, flags);
	select_nic();
	tx_fifo[tx_fifo_in] = transmit_from = tx_page_adr(tx_fifo_in) - len;
	tx_fifo_in = (tx_fifo_in + 1) % TX_PAGES; 

	if(check_lost)
	{
		
		de600_setup_address(NODE_ADDRESS, RW_ADDR);
		de600_read_byte(READ_DATA, dev);
		if (was_down || (de600_read_byte(READ_DATA, dev) != 0xde)) {
			if (adapter_init(dev)) {
				spin_unlock_irqrestore(&de600_lock, flags);
				return NETDEV_TX_BUSY;
			}
		}
	}

	de600_setup_address(transmit_from, RW_ADDR);
	for (i = 0;  i < skb->len ; ++i, ++buffer)
		de600_put_byte(*buffer);
	for (; i < len; ++i)
		de600_put_byte(0);

	if (free_tx_pages-- == TX_PAGES) { 
		dev->trans_start = jiffies;
		netif_start_queue(dev); 
		
		de600_setup_address(transmit_from, TX_ADDR);
		de600_put_command(TX_ENABLE);
	}
	else {
		if (free_tx_pages)
			netif_start_queue(dev);
		else
			netif_stop_queue(dev);
		select_prn();
	}
	spin_unlock_irqrestore(&de600_lock, flags);
	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}


static irqreturn_t de600_interrupt(int irq, void *dev_id)
{
	struct net_device	*dev = dev_id;
	u8		irq_status;
	int		retrig = 0;
	int		boguscount = 0;

	spin_lock(&de600_lock);

	select_nic();
	irq_status = de600_read_status(dev);

	do {
		pr_debug("de600_interrupt (%02X)\n", irq_status);

		if (irq_status & RX_GOOD)
			de600_rx_intr(dev);
		else if (!(irq_status & RX_BUSY))
			de600_put_command(RX_ENABLE);

		
		if (free_tx_pages < TX_PAGES)
			retrig = de600_tx_intr(dev, irq_status);
		else
			retrig = 0;

		irq_status = de600_read_status(dev);
	} while ( (irq_status & RX_GOOD) || ((++boguscount < 100) && retrig) );

	
	select_prn();
	if (retrig)
		trigger_interrupt(dev);
	spin_unlock(&de600_lock);
	return IRQ_HANDLED;
}

static int de600_tx_intr(struct net_device *dev, int irq_status)
{

	
	if (irq_status & TX_BUSY)
		return 1; 

	
	
	if (!(irq_status & TX_FAILED16)) {
		tx_fifo_out = (tx_fifo_out + 1) % TX_PAGES;
		++free_tx_pages;
		dev->stats.tx_packets++;
		netif_wake_queue(dev);
	}

	
	if ((free_tx_pages < TX_PAGES) || (irq_status & TX_FAILED16)) {
		dev->trans_start = jiffies;
		de600_setup_address(tx_fifo[tx_fifo_out], TX_ADDR);
		de600_put_command(TX_ENABLE);
		return 1;
	}
	

	return 0;
}

static void de600_rx_intr(struct net_device *dev)
{
	struct sk_buff	*skb;
	int		i;
	int		read_from;
	int		size;
	unsigned char	*buffer;

	
	size = de600_read_byte(RX_LEN, dev);	
	size += (de600_read_byte(RX_LEN, dev) << 8);	
	size -= 4;	

	
	read_from = rx_page_adr();
	next_rx_page();
	de600_put_command(RX_ENABLE);

	if ((size < 32)  ||  (size > 1535)) {
		printk(KERN_WARNING "%s: Bogus packet size %d.\n", dev->name, size);
		if (size > 10000)
			adapter_init(dev);
		return;
	}

	skb = netdev_alloc_skb(dev, size + 2);
	if (skb == NULL) {
		printk("%s: Couldn't allocate a sk_buff of size %d.\n", dev->name, size);
		return;
	}
	

	skb_reserve(skb,2);	

	
	buffer = skb_put(skb,size);

	
	de600_setup_address(read_from, RW_ADDR);
	for (i = size; i > 0; --i, ++buffer)
		*buffer = de600_read_byte(READ_DATA, dev);

	skb->protocol=eth_type_trans(skb,dev);

	netif_rx(skb);

	
	dev->stats.rx_packets++; 
	dev->stats.rx_bytes += size; 

}

static const struct net_device_ops de600_netdev_ops = {
	.ndo_open		= de600_open,
	.ndo_stop		= de600_close,
	.ndo_start_xmit		= de600_start_xmit,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};


static struct net_device * __init de600_probe(void)
{
	int	i;
	struct net_device *dev;
	int err;

	dev = alloc_etherdev(0);
	if (!dev)
		return ERR_PTR(-ENOMEM);


	if (!request_region(DE600_IO, 3, "de600")) {
		printk(KERN_WARNING "DE600: port 0x%x busy\n", DE600_IO);
		err = -EBUSY;
		goto out;
	}

	printk(KERN_INFO "%s: D-Link DE-600 pocket adapter", dev->name);
	
	pr_debug("%s", version);

	
	err = -ENODEV;
	rx_page = 0;
	select_nic();
	(void)de600_read_status(dev);
	de600_put_command(RESET);
	de600_put_command(STOP_RESET);
	if (de600_read_status(dev) & 0xf0) {
		printk(": not at I/O %#3x.\n", DATA_PORT);
		goto out1;
	}


	
	de600_setup_address(NODE_ADDRESS, RW_ADDR);
	for (i = 0; i < ETH_ALEN; i++) {
		dev->dev_addr[i] = de600_read_byte(READ_DATA, dev);
		dev->broadcast[i] = 0xff;
	}

	
	if ((dev->dev_addr[1] == 0xde) && (dev->dev_addr[2] == 0x15)) {
		
		dev->dev_addr[0] = 0x00;
		dev->dev_addr[1] = 0x80;
		dev->dev_addr[2] = 0xc8;
		dev->dev_addr[3] &= 0x0f;
		dev->dev_addr[3] |= 0x70;
	} else {
		printk(" not identified in the printer port\n");
		goto out1;
	}

	printk(", Ethernet Address: %pM\n", dev->dev_addr);

	dev->netdev_ops = &de600_netdev_ops;

	dev->flags&=~IFF_MULTICAST;

	select_prn();

	err = register_netdev(dev);
	if (err)
		goto out1;

	return dev;

out1:
	release_region(DE600_IO, 3);
out:
	free_netdev(dev);
	return ERR_PTR(err);
}

static int adapter_init(struct net_device *dev)
{
	int	i;

	select_nic();
	rx_page = 0; 
	de600_put_command(RESET);
	de600_put_command(STOP_RESET);

	
	
	de600_setup_address(NODE_ADDRESS, RW_ADDR);
	de600_read_byte(READ_DATA, dev);
	if ((de600_read_byte(READ_DATA, dev) != 0xde) ||
	    (de600_read_byte(READ_DATA, dev) != 0x15)) {
	
		printk("Something has happened to the DE-600!  Please check it and do a new ifconfig!\n");
		
		dev->flags &= ~IFF_UP;
		de600_close(dev);
		was_down = 1;
		netif_stop_queue(dev); 
		return 1; 
	}

	if (was_down) {
		printk(KERN_INFO "%s: Thanks, I feel much better now!\n", dev->name);
		was_down = 0;
	}

	tx_fifo_in = 0;
	tx_fifo_out = 0;
	free_tx_pages = TX_PAGES;


	
	de600_setup_address(NODE_ADDRESS, RW_ADDR);
	for (i = 0; i < ETH_ALEN; i++)
		de600_put_byte(dev->dev_addr[i]);

	
	rx_page = RX_BP | RX_BASE_PAGE;
	de600_setup_address(MEM_4K, RW_ADDR);
	
	de600_put_command(RX_ENABLE);
	select_prn();

	netif_start_queue(dev);

	return 0; 
}

static struct net_device *de600_dev;

static int __init de600_init(void)
{
	de600_dev = de600_probe();
	if (IS_ERR(de600_dev))
		return PTR_ERR(de600_dev);
	return 0;
}

static void __exit de600_exit(void)
{
	unregister_netdev(de600_dev);
	release_region(DE600_IO, 3);
	free_netdev(de600_dev);
}

module_init(de600_init);
module_exit(de600_exit);

MODULE_LICENSE("GPL");
