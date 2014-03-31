/*
 *	de620.c $Revision: 1.40 $ BETA
 *
 *
 *	Linux driver for the D-Link DE-620 Ethernet pocket adapter.
 *
 *	Portions (C) Copyright 1993, 1994 by Bjorn Ekwall <bj0rn@blox.se>
 *
 *	Based on adapter information gathered from DOS packetdriver
 *	sources from D-Link Inc:  (Special thanks to Henry Ngai of D-Link.)
 *		Portions (C) Copyright D-Link SYSTEM Inc. 1991, 1992
 *		Copyright, 1988, Russell Nelson, Crynwr Software
 *
 *	Adapted to the sample network driver core for linux,
 *	written by: Donald Becker <becker@super.org>
 *		(Now at <becker@scyld.com>)
 *
 *	Valuable assistance from:
 *		J. Joshua Kopper <kopper@rtsg.mot.com>
 *		Olav Kvittem <Olav.Kvittem@uninett.no>
 *		Germano Caronni <caronni@nessie.cs.id.ethz.ch>
 *		Jeremy Fitzhardinge <jeremy@suite.sw.oz.au>
 *
 *****************************************************************************/
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
 *****************************************************************************/
static const char version[] =
	"de620.c: $Revision: 1.40 $,  Bjorn Ekwall <bj0rn@blox.se>\n";


#define DE620_CLONE 0


#ifndef READ_DELAY
#define READ_DELAY 100	
#endif

#ifndef WRITE_DELAY
#define WRITE_DELAY 100	
#endif


#ifdef LOWSPEED
#endif

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
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <asm/io.h>

#include "de620.h"

typedef unsigned char byte;

#ifndef DE620_IO 
#define DE620_IO 0x378
#endif

#ifndef DE620_IRQ 
#define DE620_IRQ	7
#endif

#define DATA_PORT	(dev->base_addr)
#define STATUS_PORT	(dev->base_addr + 1)
#define COMMAND_PORT	(dev->base_addr + 2)

#define RUNT 60		
#define GIANT 1514	

static int bnc;
static int utp;
static int io  = DE620_IO;
static int irq = DE620_IRQ;
static int clone = DE620_CLONE;

static spinlock_t de620_lock;

module_param(bnc, int, 0);
module_param(utp, int, 0);
module_param(io, int, 0);
module_param(irq, int, 0);
module_param(clone, int, 0);
MODULE_PARM_DESC(bnc, "DE-620 set BNC medium (0-1)");
MODULE_PARM_DESC(utp, "DE-620 set UTP medium (0-1)");
MODULE_PARM_DESC(io, "DE-620 I/O base address,required");
MODULE_PARM_DESC(irq, "DE-620 IRQ number,required");
MODULE_PARM_DESC(clone, "Check also for non-D-Link DE-620 clones (0-1)");



static int	de620_open(struct net_device *);
static int	de620_close(struct net_device *);
static void	de620_set_multicast_list(struct net_device *);
static int	de620_start_xmit(struct sk_buff *, struct net_device *);

static irqreturn_t de620_interrupt(int, void *);
static int	de620_rx_intr(struct net_device *);

static int	adapter_init(struct net_device *);
static int	read_eeprom(struct net_device *);


#define SCR_DEF NIBBLEMODE |INTON | SLEEP | AUTOTX
#define	TCR_DEF RXPB			
#define DE620_RX_START_PAGE 12		
#define DEF_NIC_CMD IRQEN | ICEN | DS1

static volatile byte	NIC_Cmd;
static volatile byte	next_rx_page;
static byte		first_rx_page;
static byte		last_rx_page;
static byte		EIPRegister;

static struct nic {
	byte	NodeID[6];
	byte	RAM_Size;
	byte	Model;
	byte	Media;
	byte	SCR;
} nic_data;

#define de620_tx_buffs(dd) (inb(STATUS_PORT) & (TXBF0 | TXBF1))
#define de620_flip_ds(dd) NIC_Cmd ^= DS0 | DS1; outb(NIC_Cmd, COMMAND_PORT);

#ifdef COUNT_LOOPS
static int tot_cnt;
#endif
static inline byte
de620_ready(struct net_device *dev)
{
	byte value;
	register short int cnt = 0;

	while ((((value = inb(STATUS_PORT)) & READY) == 0) && (cnt <= 1000))
		++cnt;

#ifdef COUNT_LOOPS
	tot_cnt += cnt;
#endif
	return value & 0xf0; 
}

static inline void
de620_send_command(struct net_device *dev, byte cmd)
{
	de620_ready(dev);
	if (cmd == W_DUMMY)
		outb(NIC_Cmd, COMMAND_PORT);

	outb(cmd, DATA_PORT);

	outb(NIC_Cmd ^ CS0, COMMAND_PORT);
	de620_ready(dev);
	outb(NIC_Cmd, COMMAND_PORT);
}

static inline void
de620_put_byte(struct net_device *dev, byte value)
{
	
	de620_ready(dev);
	outb(value, DATA_PORT);
	de620_flip_ds(dev);
}

static inline byte
de620_read_byte(struct net_device *dev)
{
	byte value;

	
	value = de620_ready(dev); 
	de620_flip_ds(dev);
	value |= de620_ready(dev) >> 4; 
	return value;
}

static inline void
de620_write_block(struct net_device *dev, byte *buffer, int count, int pad)
{
#ifndef LOWSPEED
	byte uflip = NIC_Cmd ^ (DS0 | DS1);
	byte dflip = NIC_Cmd;
#else 
#ifdef COUNT_LOOPS
	int bytes = count;
#endif 
#endif 

#ifdef LOWSPEED
#ifdef COUNT_LOOPS
	tot_cnt = 0;
#endif 
	
	for ( ; count > 0; --count, ++buffer) {
		de620_put_byte(dev,*buffer);
	}
	for ( count = pad ; count > 0; --count, ++buffer) {
		de620_put_byte(dev, 0);
	}
	de620_send_command(dev,W_DUMMY);
#ifdef COUNT_LOOPS
	
	printk("WRITE(%d)\n", tot_cnt/((bytes?bytes:1)));
#endif 
#else 
	for ( ; count > 0; count -=2) {
		outb(*buffer++, DATA_PORT);
		outb(uflip, COMMAND_PORT);
		outb(*buffer++, DATA_PORT);
		outb(dflip, COMMAND_PORT);
	}
	de620_send_command(dev,W_DUMMY);
#endif 
}

static inline void
de620_read_block(struct net_device *dev, byte *data, int count)
{
#ifndef LOWSPEED
	byte value;
	byte uflip = NIC_Cmd ^ (DS0 | DS1);
	byte dflip = NIC_Cmd;
#else 
#ifdef COUNT_LOOPS
	int bytes = count;

	tot_cnt = 0;
#endif 
#endif 

#ifdef LOWSPEED
	
	while (count-- > 0) {
		*data++ = de620_read_byte(dev);
		de620_flip_ds(dev);
	}
#ifdef COUNT_LOOPS
	
	printk("READ(%d)\n", tot_cnt/(2*(bytes?bytes:1)));
#endif 
#else 
	while (count-- > 0) {
		value = inb(STATUS_PORT) & 0xf0; 
		outb(uflip, COMMAND_PORT);
		*data++ = value | inb(STATUS_PORT) >> 4; 
		outb(dflip , COMMAND_PORT);
	}
#endif 
}

static inline void
de620_set_delay(struct net_device *dev)
{
	de620_ready(dev);
	outb(W_DFR, DATA_PORT);
	outb(NIC_Cmd ^ CS0, COMMAND_PORT);

	de620_ready(dev);
#ifdef LOWSPEED
	outb(WRITE_DELAY, DATA_PORT);
#else
	outb(0, DATA_PORT);
#endif
	de620_flip_ds(dev);

	de620_ready(dev);
#ifdef LOWSPEED
	outb(READ_DELAY, DATA_PORT);
#else
	outb(0, DATA_PORT);
#endif
	de620_flip_ds(dev);
}

static inline void
de620_set_register(struct net_device *dev, byte reg, byte value)
{
	de620_ready(dev);
	outb(reg, DATA_PORT);
	outb(NIC_Cmd ^ CS0, COMMAND_PORT);

	de620_put_byte(dev, value);
}

static inline byte
de620_get_register(struct net_device *dev, byte reg)
{
	byte value;

	de620_send_command(dev,reg);
	value = de620_read_byte(dev);
	de620_send_command(dev,W_DUMMY);

	return value;
}

static int de620_open(struct net_device *dev)
{
	int ret = request_irq(dev->irq, de620_interrupt, 0, dev->name, dev);
	if (ret) {
		printk (KERN_ERR "%s: unable to get IRQ %d\n", dev->name, dev->irq);
		return ret;
	}

	if (adapter_init(dev)) {
		ret = -EIO;
		goto out_free_irq;
	}

	netif_start_queue(dev);
	return 0;

out_free_irq:
	free_irq(dev->irq, dev);
	return ret;
}


static int de620_close(struct net_device *dev)
{
	netif_stop_queue(dev);
	
	de620_set_register(dev, W_TCR, RXOFF);
	free_irq(dev->irq, dev);
	return 0;
}


static void de620_set_multicast_list(struct net_device *dev)
{
	if (!netdev_mc_empty(dev) || dev->flags&(IFF_ALLMULTI|IFF_PROMISC))
	{ 
		de620_set_register(dev, W_TCR, (TCR_DEF & ~RXPBM) | RXALL);
	}
	else
	{ 
		de620_set_register(dev, W_TCR, TCR_DEF);
	}
}


static void de620_timeout(struct net_device *dev)
{
	printk(KERN_WARNING "%s: transmit timed out, %s?\n", dev->name, "network cable problem");
	
	if (!adapter_init(dev)) 
		netif_wake_queue(dev);
}

static int de620_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	unsigned long flags;
	int len;
	byte *buffer = skb->data;
	byte using_txbuf;

	using_txbuf = de620_tx_buffs(dev); 

	netif_stop_queue(dev);


	if ((len = skb->len) < RUNT)
		len = RUNT;
	if (len & 1) 
		++len;

	

	spin_lock_irqsave(&de620_lock, flags);
	pr_debug("de620_start_xmit: len=%d, bufs 0x%02x\n",
		(int)skb->len, using_txbuf);

	
	switch (using_txbuf) {
	default: 
	case TXBF1: 
		de620_send_command(dev,W_CR | RW0);
		using_txbuf |= TXBF0;
		break;

	case TXBF0: 
		de620_send_command(dev,W_CR | RW1);
		using_txbuf |= TXBF1;
		break;

	case (TXBF0 | TXBF1): 
		printk(KERN_WARNING "%s: No tx-buffer available!\n", dev->name);
		spin_unlock_irqrestore(&de620_lock, flags);
		return NETDEV_TX_BUSY;
	}
	de620_write_block(dev, buffer, skb->len, len-skb->len);

	if(!(using_txbuf == (TXBF0 | TXBF1)))
		netif_wake_queue(dev);

	dev->stats.tx_packets++;
	spin_unlock_irqrestore(&de620_lock, flags);
	dev_kfree_skb (skb);
	return NETDEV_TX_OK;
}

static irqreturn_t
de620_interrupt(int irq_in, void *dev_id)
{
	struct net_device *dev = dev_id;
	byte irq_status;
	int bogus_count = 0;
	int again = 0;

	spin_lock(&de620_lock);

	
	irq_status = de620_get_register(dev, R_STS);

	pr_debug("de620_interrupt (%2.2X)\n", irq_status);

	if (irq_status & RXGOOD) {
		do {
			again = de620_rx_intr(dev);
			pr_debug("again=%d\n", again);
		}
		while (again && (++bogus_count < 100));
	}

	if(de620_tx_buffs(dev) != (TXBF0 | TXBF1))
		netif_wake_queue(dev);

	spin_unlock(&de620_lock);
	return IRQ_HANDLED;
}

static int de620_rx_intr(struct net_device *dev)
{
	struct header_buf {
		byte		status;
		byte		Rx_NextPage;
		unsigned short	Rx_ByteCount;
	} header_buf;
	struct sk_buff *skb;
	int size;
	byte *buffer;
	byte pagelink;
	byte curr_page;

	pr_debug("de620_rx_intr: next_rx_page = %d\n", next_rx_page);

	
	de620_send_command(dev, W_CR | RRN);
	de620_set_register(dev, W_RSA1, next_rx_page);
	de620_set_register(dev, W_RSA0, 0);

	
	de620_read_block(dev, (byte *)&header_buf, sizeof(struct header_buf));
	pr_debug("page status=0x%02x, nextpage=%d, packetsize=%d\n",
		header_buf.status, header_buf.Rx_NextPage,
		header_buf.Rx_ByteCount);

	
	pagelink = header_buf.Rx_NextPage;
	if ((pagelink < first_rx_page) || (last_rx_page < pagelink)) {
		
		printk(KERN_WARNING "%s: Ring overrun? Restoring...\n", dev->name);
		
		adapter_init(dev);
		netif_wake_queue(dev);
		dev->stats.rx_over_errors++;
		return 0;
	}

	
	
	pagelink = next_rx_page +
		((header_buf.Rx_ByteCount + (4 - 1 + 0x100)) >> 8);

	
	if (pagelink > last_rx_page)
		pagelink -= (last_rx_page - first_rx_page + 1);

	
	if (pagelink != header_buf.Rx_NextPage) {
		
		printk(KERN_WARNING "%s: Page link out of sync! Restoring...\n", dev->name);
		next_rx_page = header_buf.Rx_NextPage; 
		de620_send_command(dev, W_DUMMY);
		de620_set_register(dev, W_NPRF, next_rx_page);
		dev->stats.rx_over_errors++;
		return 0;
	}
	next_rx_page = pagelink;

	size = header_buf.Rx_ByteCount - 4;
	if ((size < RUNT) || (GIANT < size)) {
		printk(KERN_WARNING "%s: Illegal packet size: %d!\n", dev->name, size);
	}
	else { 
		skb = netdev_alloc_skb(dev, size + 2);
		if (skb == NULL) { 
			printk(KERN_WARNING "%s: Couldn't allocate a sk_buff of size %d.\n", dev->name, size);
			dev->stats.rx_dropped++;
		}
		else { 
			skb_reserve(skb,2);	
			
			buffer = skb_put(skb,size);
			
			de620_read_block(dev, buffer, size);
			pr_debug("Read %d bytes\n", size);
			skb->protocol=eth_type_trans(skb,dev);
			netif_rx(skb); 
			
			dev->stats.rx_packets++;
			dev->stats.rx_bytes += size;
		}
	}

	
	
	curr_page = de620_get_register(dev, R_CPR);
	de620_set_register(dev, W_NPRF, next_rx_page);
	pr_debug("next_rx_page=%d CPR=%d\n", next_rx_page, curr_page);

	return next_rx_page != curr_page; 
}

static int adapter_init(struct net_device *dev)
{
	int i;
	static int was_down;

	if ((nic_data.Model == 3) || (nic_data.Model == 0)) { 
		EIPRegister = NCTL0;
		if (nic_data.Media != 1)
			EIPRegister |= NIS0;	
	}
	else if (nic_data.Model == 2) { 
		EIPRegister = NCTL0 | NIS0;
	}

	if (utp)
		EIPRegister = NCTL0 | NIS0;
	if (bnc)
		EIPRegister = NCTL0;

	de620_send_command(dev, W_CR | RNOP | CLEAR);
	de620_send_command(dev, W_CR | RNOP);

	de620_set_register(dev, W_SCR, SCR_DEF);
	
	de620_set_register(dev, W_TCR, RXOFF);

	
	for (i = 0; i < 6; ++i) { 
		de620_set_register(dev, W_PAR0 + i, dev->dev_addr[i]);
	}

	de620_set_register(dev, W_EIP, EIPRegister);

	next_rx_page = first_rx_page = DE620_RX_START_PAGE;
	if (nic_data.RAM_Size)
		last_rx_page = nic_data.RAM_Size - 1;
	else 
		last_rx_page = 255;

	de620_set_register(dev, W_SPR, first_rx_page); 
	de620_set_register(dev, W_EPR, last_rx_page);  
	de620_set_register(dev, W_CPR, first_rx_page);
	de620_send_command(dev, W_NPR | first_rx_page); 
	de620_send_command(dev, W_DUMMY);
	de620_set_delay(dev);

	
	
#define CHECK_MASK (  0 | TXSUC |  T16  |  0  | RXCRC | RXSHORT |  0  |  0  )
#define CHECK_OK   (  0 |   0   |  0    |  0  |   0   |   0     |  0  |  0  )
        
        

	if (((i = de620_get_register(dev, R_STS)) & CHECK_MASK) != CHECK_OK) {
		printk(KERN_ERR "%s: Something has happened to the DE-620!  Please check it"
#ifdef SHUTDOWN_WHEN_LOST
			" and do a new ifconfig"
#endif
			"! (%02x)\n", dev->name, i);
#ifdef SHUTDOWN_WHEN_LOST
		
		dev->flags &= ~IFF_UP;
		de620_close(dev);
#endif
		was_down = 1;
		return 1; 
	}
	if (was_down) {
		printk(KERN_WARNING "%s: Thanks, I feel much better now!\n", dev->name);
		was_down = 0;
	}

	
	de620_set_register(dev, W_TCR, TCR_DEF);

	return 0; 
}

static const struct net_device_ops de620_netdev_ops = {
	.ndo_open 		= de620_open,
	.ndo_stop 		= de620_close,
	.ndo_start_xmit 	= de620_start_xmit,
	.ndo_tx_timeout 	= de620_timeout,
	.ndo_set_rx_mode	= de620_set_multicast_list,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};

struct net_device * __init de620_probe(int unit)
{
	byte checkbyte = 0xa5;
	struct net_device *dev;
	int err = -ENOMEM;
	int i;

	dev = alloc_etherdev(0);
	if (!dev)
		goto out;

	spin_lock_init(&de620_lock);

	dev->base_addr = io;
	dev->irq       = irq;

	
	if (unit >= 0) {
		sprintf(dev->name, "eth%d", unit);
		netdev_boot_setup_check(dev);
	}

	pr_debug("%s", version);

	printk(KERN_INFO "D-Link DE-620 pocket adapter");

	if (!request_region(dev->base_addr, 3, "de620")) {
		printk(" io 0x%3lX, which is busy.\n", dev->base_addr);
		err = -EBUSY;
		goto out1;
	}

	
	NIC_Cmd = DEF_NIC_CMD;
	de620_set_register(dev, W_EIP, EIPRegister);

	
	de620_set_register(dev, W_CPR, checkbyte);
	checkbyte = de620_get_register(dev, R_CPR);

	if ((checkbyte != 0xa5) || (read_eeprom(dev) != 0)) {
		printk(" not identified in the printer port\n");
		err = -ENODEV;
		goto out2;
	}

	
	dev->dev_addr[0] = nic_data.NodeID[0];
	for (i = 1; i < ETH_ALEN; i++) {
		dev->dev_addr[i] = nic_data.NodeID[i];
		dev->broadcast[i] = 0xff;
	}

	printk(", Ethernet Address: %pM", dev->dev_addr);

	printk(" (%dk RAM,",
		(nic_data.RAM_Size) ? (nic_data.RAM_Size >> 2) : 64);

	if (nic_data.Media == 1)
		printk(" BNC)\n");
	else
		printk(" UTP)\n");

	dev->netdev_ops = &de620_netdev_ops;
	dev->watchdog_timeo	= HZ*2;

	

	
	pr_debug("\nEEPROM contents:\n"
		"RAM_Size = 0x%02X\n"
		"NodeID = %pM\n"
		"Model = %d\n"
		"Media = %d\n"
		"SCR = 0x%02x\n", nic_data.RAM_Size, nic_data.NodeID,
		nic_data.Model, nic_data.Media, nic_data.SCR);

	err = register_netdev(dev);
	if (err)
		goto out2;
	return dev;

out2:
	release_region(dev->base_addr, 3);
out1:
	free_netdev(dev);
out:
	return ERR_PTR(err);
}

#define sendit(dev,data) de620_set_register(dev, W_EIP, data | EIPRegister);

static unsigned short __init ReadAWord(struct net_device *dev, int from)
{
	unsigned short data;
	int nbits;

	
	
	
	sendit(dev, 0); sendit(dev, 1); sendit(dev, 5); sendit(dev, 4);

	
	for (nbits = 9; nbits > 0; --nbits, from <<= 1) {
		if (from & 0x0100) { 
			
			
			
			sendit(dev, 6); sendit(dev, 7); sendit(dev, 7); sendit(dev, 6);
		}
		else {
			
			
			
			sendit(dev, 4); sendit(dev, 5); sendit(dev, 5); sendit(dev, 4);
		}
	}

	
	for (data = 0, nbits = 16; nbits > 0; --nbits) {
		
		
		
		sendit(dev, 4); sendit(dev, 5); sendit(dev, 5); sendit(dev, 4);
		data = (data << 1) | ((de620_get_register(dev, R_STS) & EEDI) >> 7);
	}
	
	
	
	sendit(dev, 0); sendit(dev, 1); sendit(dev, 1); sendit(dev, 0);

	return data;
}

static int __init read_eeprom(struct net_device *dev)
{
	unsigned short wrd;

	
	wrd = ReadAWord(dev, 0x1aa);	
	if (!clone && (wrd != htons(0x0080))) 
		return -1; 
	nic_data.NodeID[0] = wrd & 0xff;
	nic_data.NodeID[1] = wrd >> 8;

	wrd = ReadAWord(dev, 0x1ab);	
	if (!clone && ((wrd & 0xff) != 0xc8)) 
		return -1; 
	nic_data.NodeID[2] = wrd & 0xff;
	nic_data.NodeID[3] = wrd >> 8;

	wrd = ReadAWord(dev, 0x1ac);	
	nic_data.NodeID[4] = wrd & 0xff;
	nic_data.NodeID[5] = wrd >> 8;

	wrd = ReadAWord(dev, 0x1ad);	
	nic_data.RAM_Size = (wrd >> 8);

	wrd = ReadAWord(dev, 0x1ae);	
	nic_data.Model = (wrd & 0xff);

	wrd = ReadAWord(dev, 0x1af); 
	nic_data.Media = (wrd & 0xff);

	wrd = ReadAWord(dev, 0x1a8); 
	nic_data.SCR = (wrd >> 8);

	return 0; 
}

#ifdef MODULE
static struct net_device *de620_dev;

int __init init_module(void)
{
	de620_dev = de620_probe(-1);
	if (IS_ERR(de620_dev))
		return PTR_ERR(de620_dev);
	return 0;
}

void cleanup_module(void)
{
	unregister_netdev(de620_dev);
	release_region(de620_dev->base_addr, 3);
	free_netdev(de620_dev);
}
#endif 
MODULE_LICENSE("GPL");
