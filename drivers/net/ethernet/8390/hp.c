/*
	Written 1993-94 by Donald Becker.

	Copyright 1993 United States Government as represented by the
	Director, National Security Agency.

	This software may be used and distributed according to the terms
	of the GNU General Public License, incorporated herein by reference.

	The author may be reached as becker@scyld.com, or C/O
	Scyld Computing Corporation
	410 Severn Ave., Suite 210
	Annapolis MD 21403

	This is a driver for the HP PC-LAN adaptors.

	Sources:
	  The Crynwr packet driver.
*/

static const char version[] =
	"hp.c:v1.10 9/23/94 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <asm/io.h>

#include "8390.h"

#define DRV_NAME "hp"

static unsigned int hppclan_portlist[] __initdata =
{ 0x300, 0x320, 0x340, 0x280, 0x2C0, 0x200, 0x240, 0};

#define HP_IO_EXTENT	32

#define HP_DATAPORT		0x0c	
#define HP_ID			0x07
#define HP_CONFIGURE	0x08	
#define	 HP_RUN			0x01	
#define	 HP_IRQ			0x0E	
#define	 HP_DATAON		0x10	
#define NIC_OFFSET		0x10	

#define HP_START_PG		0x00	
#define HP_8BSTOP_PG	0x80	
#define HP_16BSTOP_PG	0xFF	

static int hp_probe1(struct net_device *dev, int ioaddr);

static void hp_reset_8390(struct net_device *dev);
static void hp_get_8390_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr,
					int ring_page);
static void hp_block_input(struct net_device *dev, int count,
					struct sk_buff *skb , int ring_offset);
static void hp_block_output(struct net_device *dev, int count,
							const unsigned char *buf, int start_page);

static void hp_init_card(struct net_device *dev);

static char irqmap[16] __initdata= { 0, 0, 4, 6, 8,10, 0,14, 0, 4, 2,12,0,0,0,0};



static int __init do_hp_probe(struct net_device *dev)
{
	int i;
	int base_addr = dev->base_addr;
	int irq = dev->irq;

	if (base_addr > 0x1ff)		
		return hp_probe1(dev, base_addr);
	else if (base_addr != 0)	
		return -ENXIO;

	for (i = 0; hppclan_portlist[i]; i++) {
		if (hp_probe1(dev, hppclan_portlist[i]) == 0)
			return 0;
		dev->irq = irq;
	}

	return -ENODEV;
}

#ifndef MODULE
struct net_device * __init hp_probe(int unit)
{
	struct net_device *dev = alloc_eip_netdev();
	int err;

	if (!dev)
		return ERR_PTR(-ENOMEM);

	sprintf(dev->name, "eth%d", unit);
	netdev_boot_setup_check(dev);

	err = do_hp_probe(dev);
	if (err)
		goto out;
	return dev;
out:
	free_netdev(dev);
	return ERR_PTR(err);
}
#endif

static int __init hp_probe1(struct net_device *dev, int ioaddr)
{
	int i, retval, board_id, wordmode;
	const char *name;
	static unsigned version_printed;

	if (!request_region(ioaddr, HP_IO_EXTENT, DRV_NAME))
		return -EBUSY;

	
	if (inb(ioaddr) != 0x08
		|| inb(ioaddr+1) != 0x00
		|| inb(ioaddr+2) != 0x09
		|| inb(ioaddr+14) == 0x57) {
		retval = -ENODEV;
		goto out;
	}

	if ((board_id = inb(ioaddr + HP_ID)) & 0x80) {
		name = "HP27247";
		wordmode = 1;
	} else {
		name = "HP27250";
		wordmode = 0;
	}

	if (ei_debug  &&  version_printed++ == 0)
		printk(version);

	printk("%s: %s (ID %02x) at %#3x,", dev->name, name, board_id, ioaddr);

	for(i = 0; i < ETH_ALEN; i++)
		dev->dev_addr[i] = inb(ioaddr + i);

	printk(" %pM", dev->dev_addr);

	
	if (dev->irq < 2) {
		static const int irq_16list[] = { 11, 10, 5, 3, 4, 7, 9, 0};
		static const int irq_8list[] = { 7, 5, 3, 4, 9, 0};
		const int *irqp = wordmode ? irq_16list : irq_8list;
		do {
			int irq = *irqp;
			if (request_irq (irq, NULL, 0, "bogus", NULL) != -EBUSY) {
				unsigned long cookie = probe_irq_on();
				
				outb_p(irqmap[irq] | HP_RUN, ioaddr + HP_CONFIGURE);
				outb_p( 0x00 | HP_RUN, ioaddr + HP_CONFIGURE);
				if (irq == probe_irq_off(cookie)		 
					&& request_irq (irq, eip_interrupt, 0, DRV_NAME, dev) == 0) {
					printk(" selecting IRQ %d.\n", irq);
					dev->irq = *irqp;
					break;
				}
			}
		} while (*++irqp);
		if (*irqp == 0) {
			printk(" no free IRQ lines.\n");
			retval = -EBUSY;
			goto out;
		}
	} else {
		if (dev->irq == 2)
			dev->irq = 9;
		if ((retval = request_irq(dev->irq, eip_interrupt, 0, DRV_NAME, dev))) {
			printk (" unable to get IRQ %d.\n", dev->irq);
			goto out;
		}
	}

	
	dev->base_addr = ioaddr + NIC_OFFSET;
	dev->netdev_ops = &eip_netdev_ops;

	ei_status.name = name;
	ei_status.word16 = wordmode;
	ei_status.tx_start_page = HP_START_PG;
	ei_status.rx_start_page = HP_START_PG + TX_PAGES;
	ei_status.stop_page = wordmode ? HP_16BSTOP_PG : HP_8BSTOP_PG;

	ei_status.reset_8390 = hp_reset_8390;
	ei_status.get_8390_hdr = hp_get_8390_hdr;
	ei_status.block_input = hp_block_input;
	ei_status.block_output = hp_block_output;
	hp_init_card(dev);

	retval = register_netdev(dev);
	if (retval)
		goto out1;
	return 0;
out1:
	free_irq(dev->irq, dev);
out:
	release_region(ioaddr, HP_IO_EXTENT);
	return retval;
}

static void
hp_reset_8390(struct net_device *dev)
{
	int hp_base = dev->base_addr - NIC_OFFSET;
	int saved_config = inb_p(hp_base + HP_CONFIGURE);

	if (ei_debug > 1) printk("resetting the 8390 time=%ld...", jiffies);
	outb_p(0x00, hp_base + HP_CONFIGURE);
	ei_status.txing = 0;
	
	udelay(5);

	outb_p(saved_config, hp_base + HP_CONFIGURE);
	udelay(5);

	if ((inb_p(hp_base+NIC_OFFSET+EN0_ISR) & ENISR_RESET) == 0)
		printk("%s: hp_reset_8390() did not complete.\n", dev->name);

	if (ei_debug > 1) printk("8390 reset done (%ld).", jiffies);
}

static void
hp_get_8390_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr, int ring_page)
{
	int nic_base = dev->base_addr;
	int saved_config = inb_p(nic_base - NIC_OFFSET + HP_CONFIGURE);

	outb_p(saved_config | HP_DATAON, nic_base - NIC_OFFSET + HP_CONFIGURE);
	outb_p(E8390_NODMA+E8390_PAGE0+E8390_START, nic_base);
	outb_p(sizeof(struct e8390_pkt_hdr), nic_base + EN0_RCNTLO);
	outb_p(0, nic_base + EN0_RCNTHI);
	outb_p(0, nic_base + EN0_RSARLO);	
	outb_p(ring_page, nic_base + EN0_RSARHI);
	outb_p(E8390_RREAD+E8390_START, nic_base);

	if (ei_status.word16)
	  insw(nic_base - NIC_OFFSET + HP_DATAPORT, hdr, sizeof(struct e8390_pkt_hdr)>>1);
	else
	  insb(nic_base - NIC_OFFSET + HP_DATAPORT, hdr, sizeof(struct e8390_pkt_hdr));

	outb_p(saved_config & (~HP_DATAON), nic_base - NIC_OFFSET + HP_CONFIGURE);
}


static void
hp_block_input(struct net_device *dev, int count, struct sk_buff *skb, int ring_offset)
{
	int nic_base = dev->base_addr;
	int saved_config = inb_p(nic_base - NIC_OFFSET + HP_CONFIGURE);
	int xfer_count = count;
	char *buf = skb->data;

	outb_p(saved_config | HP_DATAON, nic_base - NIC_OFFSET + HP_CONFIGURE);
	outb_p(E8390_NODMA+E8390_PAGE0+E8390_START, nic_base);
	outb_p(count & 0xff, nic_base + EN0_RCNTLO);
	outb_p(count >> 8, nic_base + EN0_RCNTHI);
	outb_p(ring_offset & 0xff, nic_base + EN0_RSARLO);
	outb_p(ring_offset >> 8, nic_base + EN0_RSARHI);
	outb_p(E8390_RREAD+E8390_START, nic_base);
	if (ei_status.word16) {
	  insw(nic_base - NIC_OFFSET + HP_DATAPORT,buf,count>>1);
	  if (count & 0x01)
		buf[count-1] = inb(nic_base - NIC_OFFSET + HP_DATAPORT), xfer_count++;
	} else {
		insb(nic_base - NIC_OFFSET + HP_DATAPORT, buf, count);
	}
	
	if (ei_debug > 0) {			
	  int high = inb_p(nic_base + EN0_RSARHI);
	  int low = inb_p(nic_base + EN0_RSARLO);
	  int addr = (high << 8) + low;
	  
	  if (((ring_offset + xfer_count) & 0xff) != (addr & 0xff))
		printk("%s: RX transfer address mismatch, %#4.4x vs. %#4.4x (actual).\n",
			   dev->name, ring_offset + xfer_count, addr);
	}
	outb_p(saved_config & (~HP_DATAON), nic_base - NIC_OFFSET + HP_CONFIGURE);
}

static void
hp_block_output(struct net_device *dev, int count,
				const unsigned char *buf, int start_page)
{
	int nic_base = dev->base_addr;
	int saved_config = inb_p(nic_base - NIC_OFFSET + HP_CONFIGURE);

	outb_p(saved_config | HP_DATAON, nic_base - NIC_OFFSET + HP_CONFIGURE);
	if (ei_status.word16 && (count & 0x01))
	  count++;
	
	outb_p(E8390_PAGE0+E8390_START+E8390_NODMA, nic_base);

#ifdef NE8390_RW_BUGFIX
	outb_p(0x42, nic_base + EN0_RCNTLO);
	outb_p(0,	nic_base + EN0_RCNTHI);
	outb_p(0xff, nic_base + EN0_RSARLO);
	outb_p(0x00, nic_base + EN0_RSARHI);
#define NE_CMD	 	0x00
	outb_p(E8390_RREAD+E8390_START, nic_base + NE_CMD);
	
	inb_p(0x61);
	inb_p(0x61);
#endif

	outb_p(count & 0xff, nic_base + EN0_RCNTLO);
	outb_p(count >> 8,	 nic_base + EN0_RCNTHI);
	outb_p(0x00, nic_base + EN0_RSARLO);
	outb_p(start_page, nic_base + EN0_RSARHI);

	outb_p(E8390_RWRITE+E8390_START, nic_base);
	if (ei_status.word16) {
		
		outsw(nic_base - NIC_OFFSET + HP_DATAPORT, buf, count>>1);
	} else {
		outsb(nic_base - NIC_OFFSET + HP_DATAPORT, buf, count);
	}

	

	
	if (ei_debug > 0) {			
	  int high = inb_p(nic_base + EN0_RSARHI);
	  int low  = inb_p(nic_base + EN0_RSARLO);
	  int addr = (high << 8) + low;
	  if ((start_page << 8) + count != addr)
		printk("%s: TX Transfer address mismatch, %#4.4x vs. %#4.4x.\n",
			   dev->name, (start_page << 8) + count, addr);
	}
	outb_p(saved_config & (~HP_DATAON), nic_base - NIC_OFFSET + HP_CONFIGURE);
}

static void __init
hp_init_card(struct net_device *dev)
{
	int irq = dev->irq;
	NS8390p_init(dev, 0);
	outb_p(irqmap[irq&0x0f] | HP_RUN,
		   dev->base_addr - NIC_OFFSET + HP_CONFIGURE);
}

#ifdef MODULE
#define MAX_HP_CARDS	4	
static struct net_device *dev_hp[MAX_HP_CARDS];
static int io[MAX_HP_CARDS];
static int irq[MAX_HP_CARDS];

module_param_array(io, int, NULL, 0);
module_param_array(irq, int, NULL, 0);
MODULE_PARM_DESC(io, "I/O base address(es)");
MODULE_PARM_DESC(irq, "IRQ number(s) (assigned)");
MODULE_DESCRIPTION("HP PC-LAN ISA ethernet driver");
MODULE_LICENSE("GPL");

int __init
init_module(void)
{
	struct net_device *dev;
	int this_dev, found = 0;

	for (this_dev = 0; this_dev < MAX_HP_CARDS; this_dev++) {
		if (io[this_dev] == 0)  {
			if (this_dev != 0) break; 
			printk(KERN_NOTICE "hp.c: Presently autoprobing (not recommended) for a single card.\n");
		}
		dev = alloc_eip_netdev();
		if (!dev)
			break;
		dev->irq = irq[this_dev];
		dev->base_addr = io[this_dev];
		if (do_hp_probe(dev) == 0) {
			dev_hp[found++] = dev;
			continue;
		}
		free_netdev(dev);
		printk(KERN_WARNING "hp.c: No HP card found (i/o = 0x%x).\n", io[this_dev]);
		break;
	}
	if (found)
		return 0;
	return -ENXIO;
}

static void cleanup_card(struct net_device *dev)
{
	free_irq(dev->irq, dev);
	release_region(dev->base_addr - NIC_OFFSET, HP_IO_EXTENT);
}

void __exit
cleanup_module(void)
{
	int this_dev;

	for (this_dev = 0; this_dev < MAX_HP_CARDS; this_dev++) {
		struct net_device *dev = dev_hp[this_dev];
		if (dev) {
			unregister_netdev(dev);
			cleanup_card(dev);
			free_netdev(dev);
		}
	}
}
#endif 
