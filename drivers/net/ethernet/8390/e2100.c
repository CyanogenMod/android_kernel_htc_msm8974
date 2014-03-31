/*
	Written 1993-1994 by Donald Becker.

	Copyright 1994 by Donald Becker.
	Copyright 1993 United States Government as represented by the
	Director, National Security Agency.  This software may be used and
	distributed according to the terms of the GNU General Public License,
	incorporated herein by reference.

	This is a driver for the Cabletron E2100 series ethercards.

	The Author may be reached as becker@scyld.com, or C/O
	Scyld Computing Corporation
	410 Severn Ave., Suite 210
	Annapolis MD 21403

	The E2100 series ethercard is a fairly generic shared memory 8390
	implementation.  The only unusual aspect is the way the shared memory
	registers are set: first you do an inb() in what is normally the
	station address region, and the low three bits of next outb() *address*
	is used	as the write value for that register.  Either someone wasn't
	too used to dem bit en bites, or they were trying to obfuscate the
	programming interface.

	There is an additional complication when setting the window on the packet
	buffer.  You must first do a read into the packet buffer region with the
	low 8 address bits the address setting the page for the start of the packet
	buffer window, and then do the above operation.  See mem_on() for details.

	One bug on the chip is that even a hard reset won't disable the memory
	window, usually resulting in a hung machine if mem_off() isn't called.
	If this happens, you must power down the machine for about 30 seconds.
*/

static const char version[] =
	"e2100.c:v1.01 7/21/94 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <asm/io.h>

#include "8390.h"

#define DRV_NAME "e2100"

static int e21_probe_list[] = {0x300, 0x280, 0x380, 0x220, 0};

#define E21_NIC_OFFSET  0		
#define E21_ASIC		0x10
#define E21_MEM_ENABLE	0x10
#define  E21_MEM_ON		0x05	
#define  E21_MEM_ON_8	0x07	
#define E21_MEM_BASE	0x11
#define E21_IRQ_LOW		0x12	
#define E21_IRQ_HIGH	0x14	
#define E21_MEDIA		0x14	
#define  E21_ALT_IFPORT 0x02	
#define  E21_BIG_MEM	0x04	
#define E21_SAPROM		0x10	
#define E21_IO_EXTENT	 0x20

static inline void mem_on(short port, volatile char __iomem *mem_base,
						  unsigned char start_page )
{
	readb(mem_base+start_page);
	inb(port + E21_MEM_ENABLE);
	outb(E21_MEM_ON, port + E21_MEM_ENABLE + E21_MEM_ON);
}

static inline void mem_off(short port)
{
	inb(port + E21_MEM_ENABLE);
	outb(0x00, port + E21_MEM_ENABLE);
}

#define E21_RX_START_PG		0x00	
#define E21_RX_STOP_PG		0x30	
#define E21_BIG_RX_STOP_PG	0xF0	
#define E21_TX_START_PG		E21_RX_STOP_PG	

static int e21_probe1(struct net_device *dev, int ioaddr);

static int e21_open(struct net_device *dev);
static void e21_reset_8390(struct net_device *dev);
static void e21_block_input(struct net_device *dev, int count,
						   struct sk_buff *skb, int ring_offset);
static void e21_block_output(struct net_device *dev, int count,
							 const unsigned char *buf, int start_page);
static void e21_get_8390_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr,
							int ring_page);
static int e21_open(struct net_device *dev);
static int e21_close(struct net_device *dev);



static int  __init do_e2100_probe(struct net_device *dev)
{
	int *port;
	int base_addr = dev->base_addr;
	int irq = dev->irq;

	if (base_addr > 0x1ff)		
		return e21_probe1(dev, base_addr);
	else if (base_addr != 0)	
		return -ENXIO;

	for (port = e21_probe_list; *port; port++) {
		dev->irq = irq;
		if (e21_probe1(dev, *port) == 0)
			return 0;
	}

	return -ENODEV;
}

#ifndef MODULE
struct net_device * __init e2100_probe(int unit)
{
	struct net_device *dev = alloc_ei_netdev();
	int err;

	if (!dev)
		return ERR_PTR(-ENOMEM);

	sprintf(dev->name, "eth%d", unit);
	netdev_boot_setup_check(dev);

	err = do_e2100_probe(dev);
	if (err)
		goto out;
	return dev;
out:
	free_netdev(dev);
	return ERR_PTR(err);
}
#endif

static const struct net_device_ops e21_netdev_ops = {
	.ndo_open		= e21_open,
	.ndo_stop		= e21_close,

	.ndo_start_xmit		= ei_start_xmit,
	.ndo_tx_timeout		= ei_tx_timeout,
	.ndo_get_stats		= ei_get_stats,
	.ndo_set_rx_mode	= ei_set_multicast_list,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_change_mtu		= eth_change_mtu,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller 	= ei_poll,
#endif
};

static int __init e21_probe1(struct net_device *dev, int ioaddr)
{
	int i, status, retval;
	unsigned char *station_addr = dev->dev_addr;
	static unsigned version_printed;

	if (!request_region(ioaddr, E21_IO_EXTENT, DRV_NAME))
		return -EBUSY;

	
	if (inb(ioaddr + E21_SAPROM + 0) != 0x00 ||
	    inb(ioaddr + E21_SAPROM + 1) != 0x00 ||
	    inb(ioaddr + E21_SAPROM + 2) != 0x1d) {
		retval = -ENODEV;
		goto out;
	}

	
	outb(E8390_NODMA + E8390_STOP, ioaddr);
	udelay(1);	
	status = inb(ioaddr);
	if (status != 0x21 && status != 0x23) {
		retval = -ENODEV;
		goto out;
	}

	
	for (i = 0; i < 6; i++)
		station_addr[i] = inb(ioaddr + E21_SAPROM + i);

	inb(ioaddr + E21_MEDIA); 		
	outb(0, ioaddr + E21_ASIC); 	

	if (ei_debug  &&  version_printed++ == 0)
		printk(version);

	for (i = 0; i < 6; i++)
		printk(" %02X", station_addr[i]);

	if (dev->irq < 2) {
		static const int irqlist[] = {15, 11, 10, 12, 5, 9, 3, 4};
		for (i = 0; i < ARRAY_SIZE(irqlist); i++)
			if (request_irq (irqlist[i], NULL, 0, "bogus", NULL) != -EBUSY) {
				dev->irq = irqlist[i];
				break;
			}
		if (i >= ARRAY_SIZE(irqlist)) {
			printk(" unable to get IRQ %d.\n", dev->irq);
			retval = -EAGAIN;
			goto out;
		}
	} else if (dev->irq == 2)	
		dev->irq = 9;

	
	dev->base_addr = ioaddr;

	ei_status.name = "E2100";
	ei_status.word16 = 1;
	ei_status.tx_start_page = E21_TX_START_PG;
	ei_status.rx_start_page = E21_RX_START_PG;
	ei_status.stop_page = E21_RX_STOP_PG;
	ei_status.saved_irq = dev->irq;

	if (dev->mem_end & 15)
		dev->if_port = dev->mem_end & 7;
	else {
		dev->if_port = 0;
		inb(ioaddr + E21_MEDIA); 	
		for(i = 0; i < 6; i++)
			if (station_addr[i] != inb(ioaddr + E21_SAPROM + 8 + i)) {
				dev->if_port = 1;
				break;
			}
	}

	if (dev->mem_start == 0)
		dev->mem_start = 0xd0000;

	ei_status.mem = ioremap(dev->mem_start, 2*1024);
	if (!ei_status.mem) {
		printk("unable to remap memory\n");
		retval = -EAGAIN;
		goto out;
	}

#ifdef notdef
	ei_status.rmem_start = dev->mem_start + TX_PAGES*256;
	dev->mem_end = ei_status.rmem_end = dev->mem_start + 2*1024;
#endif

	printk(", IRQ %d, %s media, memory @ %#lx.\n", dev->irq,
		   dev->if_port ? "secondary" : "primary", dev->mem_start);

	ei_status.reset_8390 = &e21_reset_8390;
	ei_status.block_input = &e21_block_input;
	ei_status.block_output = &e21_block_output;
	ei_status.get_8390_hdr = &e21_get_8390_hdr;

	dev->netdev_ops = &e21_netdev_ops;
	NS8390_init(dev, 0);

	retval = register_netdev(dev);
	if (retval)
		goto out;
	return 0;
out:
	release_region(ioaddr, E21_IO_EXTENT);
	return retval;
}

static int
e21_open(struct net_device *dev)
{
	short ioaddr = dev->base_addr;
	int retval;

	if ((retval = request_irq(dev->irq, ei_interrupt, 0, dev->name, dev)))
		return retval;

	
	inb(ioaddr + E21_IRQ_LOW);
	outb(0, ioaddr + E21_ASIC + (dev->irq & 7));
	inb(ioaddr + E21_IRQ_HIGH); 			
	outb(0, ioaddr + E21_ASIC + (dev->irq > 7 ? 1:0)
		   + (dev->if_port ? E21_ALT_IFPORT : 0));
	inb(ioaddr + E21_MEM_BASE);
	outb(0, ioaddr + E21_ASIC + ((dev->mem_start >> 17) & 7));

	ei_open(dev);
	return 0;
}

static void
e21_reset_8390(struct net_device *dev)
{
	short ioaddr = dev->base_addr;

	outb(0x01, ioaddr);
	if (ei_debug > 1) printk("resetting the E2180x3 t=%ld...", jiffies);
	ei_status.txing = 0;

	

	if (ei_debug > 1) printk("reset done\n");
}


static void
e21_get_8390_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr, int ring_page)
{

	short ioaddr = dev->base_addr;
	char __iomem *shared_mem = ei_status.mem;

	mem_on(ioaddr, shared_mem, ring_page);

#ifdef notdef
	
	memcpy_fromio(hdr, shared_mem, sizeof(struct e8390_pkt_hdr));
#else
	((unsigned int*)hdr)[0] = readl(shared_mem);
#endif

	
	mem_off(ioaddr);

}

static void
e21_block_input(struct net_device *dev, int count, struct sk_buff *skb, int ring_offset)
{
	short ioaddr = dev->base_addr;
	char __iomem *shared_mem = ei_status.mem;

	mem_on(ioaddr, shared_mem, (ring_offset>>8));

	memcpy_fromio(skb->data, ei_status.mem + (ring_offset & 0xff), count);

	mem_off(ioaddr);
}

static void
e21_block_output(struct net_device *dev, int count, const unsigned char *buf,
				 int start_page)
{
	short ioaddr = dev->base_addr;
	volatile char __iomem *shared_mem = ei_status.mem;

	readb(shared_mem + start_page);
	mem_on(ioaddr, shared_mem, start_page);

	memcpy_toio(shared_mem, buf, count);
	mem_off(ioaddr);
}

static int
e21_close(struct net_device *dev)
{
	short ioaddr = dev->base_addr;

	if (ei_debug > 1)
		printk("%s: Shutting down ethercard.\n", dev->name);

	free_irq(dev->irq, dev);
	dev->irq = ei_status.saved_irq;

	
	inb(ioaddr + E21_IRQ_LOW);
	outb(0, ioaddr + E21_ASIC);
	inb(ioaddr + E21_IRQ_HIGH); 			
	outb(0, ioaddr + E21_ASIC);

	ei_close(dev);

	mem_off(ioaddr);

	return 0;
}


#ifdef MODULE
#define MAX_E21_CARDS	4	
static struct net_device *dev_e21[MAX_E21_CARDS];
static int io[MAX_E21_CARDS];
static int irq[MAX_E21_CARDS];
static int mem[MAX_E21_CARDS];
static int xcvr[MAX_E21_CARDS];		

module_param_array(io, int, NULL, 0);
module_param_array(irq, int, NULL, 0);
module_param_array(mem, int, NULL, 0);
module_param_array(xcvr, int, NULL, 0);
MODULE_PARM_DESC(io, "I/O base address(es)");
MODULE_PARM_DESC(irq, "IRQ number(s)");
MODULE_PARM_DESC(mem, " memory base address(es)");
MODULE_PARM_DESC(xcvr, "transceiver(s) (0=internal, 1=external)");
MODULE_DESCRIPTION("Cabletron E2100 ISA ethernet driver");
MODULE_LICENSE("GPL");


int __init init_module(void)
{
	struct net_device *dev;
	int this_dev, found = 0;

	for (this_dev = 0; this_dev < MAX_E21_CARDS; this_dev++) {
		if (io[this_dev] == 0)  {
			if (this_dev != 0) break; 
			printk(KERN_NOTICE "e2100.c: Presently autoprobing (not recommended) for a single card.\n");
		}
		dev = alloc_ei_netdev();
		if (!dev)
			break;
		dev->irq = irq[this_dev];
		dev->base_addr = io[this_dev];
		dev->mem_start = mem[this_dev];
		dev->mem_end = xcvr[this_dev];	
		if (do_e2100_probe(dev) == 0) {
			dev_e21[found++] = dev;
			continue;
		}
		free_netdev(dev);
		printk(KERN_WARNING "e2100.c: No E2100 card found (i/o = 0x%x).\n", io[this_dev]);
		break;
	}
	if (found)
		return 0;
	return -ENXIO;
}

static void cleanup_card(struct net_device *dev)
{
	
	iounmap(ei_status.mem);
	release_region(dev->base_addr, E21_IO_EXTENT);
}

void __exit
cleanup_module(void)
{
	int this_dev;

	for (this_dev = 0; this_dev < MAX_E21_CARDS; this_dev++) {
		struct net_device *dev = dev_e21[this_dev];
		if (dev) {
			unregister_netdev(dev);
			cleanup_card(dev);
			free_netdev(dev);
		}
	}
}
#endif 
