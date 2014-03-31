/*
    Written 1992-94 by Donald Becker.

    Copyright 1993 United States Government as represented by the
    Director, National Security Agency.  This software may be used and
    distributed according to the terms of the GNU General Public License,
    incorporated herein by reference.

    The author may be reached as becker@scyld.com, or C/O
	Scyld Computing Corporation
	410 Severn Ave., Suite 210
	Annapolis MD 21403


    This driver should work with the 3c503 and 3c503/16.  It should be used
    in shared memory mode for best performance, although it may also work
    in programmed-I/O mode.

    Sources:
    EtherLink II Technical Reference Manual,
    EtherLink II/16 Technical Reference Manual Supplement,
    3Com Corporation, 5400 Bayfront Plaza, Santa Clara CA 95052-8145

    The Crynwr 3c503 packet driver.

    Changelog:

    Paul Gortmaker	: add support for the 2nd 8kB of RAM on 16 bit cards.
    Paul Gortmaker	: multiple card support for module users.
    rjohnson@analogic.com : Fix up PIO interface for efficient operation.
    Jeff Garzik		: ethtool support

*/

#define DRV_NAME	"3c503"
#define DRV_VERSION	"1.10a"
#define DRV_RELDATE	"11/17/2001"


static const char version[] =
    DRV_NAME ".c:v" DRV_VERSION " " DRV_RELDATE "  Donald Becker (becker@scyld.com)\n";

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ethtool.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/byteorder.h>

#include "8390.h"
#include "3c503.h"
#define WRD_COUNT 4

static int el2_pio_probe(struct net_device *dev);
static int el2_probe1(struct net_device *dev, int ioaddr);

static unsigned int netcard_portlist[] __initdata =
	{ 0x300,0x310,0x330,0x350,0x250,0x280,0x2a0,0x2e0,0};

#define EL2_IO_EXTENT	16

static int el2_open(struct net_device *dev);
static int el2_close(struct net_device *dev);
static void el2_reset_8390(struct net_device *dev);
static void el2_init_card(struct net_device *dev);
static void el2_block_output(struct net_device *dev, int count,
			     const unsigned char *buf, int start_page);
static void el2_block_input(struct net_device *dev, int count, struct sk_buff *skb,
			   int ring_offset);
static void el2_get_8390_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr,
			 int ring_page);
static const struct ethtool_ops netdev_ethtool_ops;


static int __init do_el2_probe(struct net_device *dev)
{
    int *addr, addrs[] = { 0xddffe, 0xd9ffe, 0xcdffe, 0xc9ffe, 0};
    int base_addr = dev->base_addr;
    int irq = dev->irq;

    if (base_addr > 0x1ff)	
	return el2_probe1(dev, base_addr);
    else if (base_addr != 0)		
	return -ENXIO;

    for (addr = addrs; *addr; addr++) {
	void __iomem *p = ioremap(*addr, 1);
	unsigned base_bits;
	int i;

	if (!p)
		continue;
	base_bits = readb(p);
	iounmap(p);
	i = ffs(base_bits) - 1;
	if (i == -1 || base_bits != (1 << i))
	    continue;
	if (el2_probe1(dev, netcard_portlist[i]) == 0)
	    return 0;
	dev->irq = irq;
    }
#if ! defined(no_probe_nonshared_memory)
    return el2_pio_probe(dev);
#else
    return -ENODEV;
#endif
}

static int __init
el2_pio_probe(struct net_device *dev)
{
    int i;
    int base_addr = dev->base_addr;
    int irq = dev->irq;

    if (base_addr > 0x1ff)	
	return el2_probe1(dev, base_addr);
    else if (base_addr != 0)	
	return -ENXIO;

    for (i = 0; netcard_portlist[i]; i++) {
	if (el2_probe1(dev, netcard_portlist[i]) == 0)
	    return 0;
	dev->irq = irq;
    }

    return -ENODEV;
}

#ifndef MODULE
struct net_device * __init el2_probe(int unit)
{
	struct net_device *dev = alloc_eip_netdev();
	int err;

	if (!dev)
		return ERR_PTR(-ENOMEM);

	sprintf(dev->name, "eth%d", unit);
	netdev_boot_setup_check(dev);

	err = do_el2_probe(dev);
	if (err)
		goto out;
	return dev;
out:
	free_netdev(dev);
	return ERR_PTR(err);
}
#endif

static const struct net_device_ops el2_netdev_ops = {
	.ndo_open		= el2_open,
	.ndo_stop		= el2_close,

	.ndo_start_xmit		= eip_start_xmit,
	.ndo_tx_timeout		= eip_tx_timeout,
	.ndo_get_stats		= eip_get_stats,
	.ndo_set_rx_mode	= eip_set_multicast_list,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_change_mtu		= eth_change_mtu,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller 	= eip_poll,
#endif
};

static int __init
el2_probe1(struct net_device *dev, int ioaddr)
{
    int i, iobase_reg, membase_reg, saved_406, wordlength, retval;
    static unsigned version_printed;
    unsigned long vendor_id;

    if (!request_region(ioaddr, EL2_IO_EXTENT, DRV_NAME))
	return -EBUSY;

    if (!request_region(ioaddr + 0x400, 8, DRV_NAME)) {
	retval = -EBUSY;
	goto out;
    }

    
    if (inb(ioaddr + 0x408) == 0xff) {
    	mdelay(1);
	retval = -ENODEV;
	goto out1;
    }

    iobase_reg = inb(ioaddr+0x403);
    membase_reg = inb(ioaddr+0x404);
    
    if ((iobase_reg  & (iobase_reg - 1)) ||
	(membase_reg & (membase_reg - 1))) {
	retval = -ENODEV;
	goto out1;
    }
    saved_406 = inb_p(ioaddr + 0x406);
    outb_p(ECNTRL_RESET|ECNTRL_THIN, ioaddr + 0x406); 
    outb_p(ECNTRL_THIN, ioaddr + 0x406);
    outb(ECNTRL_SAPROM|ECNTRL_THIN, ioaddr + 0x406);
    vendor_id = inb(ioaddr)*0x10000 + inb(ioaddr + 1)*0x100 + inb(ioaddr + 2);
    if ((vendor_id != OLD_3COM_ID) && (vendor_id != NEW_3COM_ID)) {
	
	outb(saved_406, ioaddr + 0x406);
	retval = -ENODEV;
	goto out1;
    }

    if (ei_debug  &&  version_printed++ == 0)
	pr_debug("%s", version);

    dev->base_addr = ioaddr;

    pr_info("%s: 3c503 at i/o base %#3x, node ", dev->name, ioaddr);

    
    for (i = 0; i < 6; i++)
	dev->dev_addr[i] = inb(ioaddr + i);
    pr_cont("%pM", dev->dev_addr);

    
    outb(ECNTRL_THIN, ioaddr + 0x406);

    
    outb_p(E8390_PAGE0, ioaddr + E8390_CMD);
    outb_p(0, ioaddr + EN0_DCFG);
    outb_p(E8390_PAGE2, ioaddr + E8390_CMD);
    wordlength = inb_p(ioaddr + EN0_DCFG) & ENDCFG_WTS;
    outb_p(E8390_PAGE0, ioaddr + E8390_CMD);

    
    if (ei_debug > 2)
	pr_cont(" memory jumpers %2.2x ", membase_reg);
    outb(EGACFR_NORM, ioaddr + 0x405);	

#if defined(EI8390_THICK) || defined(EL2_AUI)
    ei_status.interface_num = 1;
#else
    ei_status.interface_num = dev->mem_end & 0xf;
#endif
    pr_cont(", using %sternal xcvr.\n", ei_status.interface_num == 0 ? "in" : "ex");

    if ((membase_reg & 0xf0) == 0) {
	dev->mem_start = 0;
	ei_status.name = "3c503-PIO";
	ei_status.mem = NULL;
    } else {
	dev->mem_start = ((membase_reg & 0xc0) ? 0xD8000 : 0xC8000) +
	    ((membase_reg & 0xA0) ? 0x4000 : 0);
#define EL2_MEMSIZE (EL2_MB1_STOP_PG - EL2_MB1_START_PG)*256
	ei_status.mem = ioremap(dev->mem_start, EL2_MEMSIZE);

#ifdef EL2MEMTEST
	{			
	    void __iomem *mem_base = ei_status.mem;
	    unsigned int test_val = 0xbbadf00d;
	    writel(0xba5eba5e, mem_base);
	    for (i = sizeof(test_val); i < EL2_MEMSIZE; i+=sizeof(test_val)) {
		writel(test_val, mem_base + i);
		if (readl(mem_base) != 0xba5eba5e ||
		    readl(mem_base + i) != test_val) {
		    pr_warning("3c503: memory failure or memory address conflict.\n");
		    dev->mem_start = 0;
		    ei_status.name = "3c503-PIO";
		    iounmap(mem_base);
		    ei_status.mem = NULL;
		    break;
		}
		test_val += 0x55555555;
		writel(0, mem_base + i);
	    }
	}
#endif  

	if (dev->mem_start)
		dev->mem_end = dev->mem_start + EL2_MEMSIZE;

	if (wordlength) {	
		ei_status.priv = 0;
		ei_status.name = "3c503/16";
	} else {
		ei_status.priv = TX_PAGES * 256;
		ei_status.name = "3c503";
	}
    }


    if (wordlength) {
	ei_status.tx_start_page = EL2_MB0_START_PG;
	ei_status.rx_start_page = EL2_MB1_START_PG;
    } else {
	ei_status.tx_start_page = EL2_MB1_START_PG;
	ei_status.rx_start_page = EL2_MB1_START_PG + TX_PAGES;
    }

    
    ei_status.stop_page = EL2_MB1_STOP_PG;
    ei_status.word16 = wordlength;
    ei_status.reset_8390 = el2_reset_8390;
    ei_status.get_8390_hdr = el2_get_8390_hdr;
    ei_status.block_input = el2_block_input;
    ei_status.block_output = el2_block_output;

    if (dev->irq == 2)
	dev->irq = 9;
    else if (dev->irq > 5 && dev->irq != 9) {
	pr_warning("3c503: configured interrupt %d invalid, will use autoIRQ.\n",
	       dev->irq);
	dev->irq = 0;
    }

    ei_status.saved_irq = dev->irq;

    dev->netdev_ops = &el2_netdev_ops;
    dev->ethtool_ops = &netdev_ethtool_ops;

    retval = register_netdev(dev);
    if (retval)
	goto out1;

    if (dev->mem_start)
	pr_info("%s: %s - %dkB RAM, 8kB shared mem window at %#6lx-%#6lx.\n",
		dev->name, ei_status.name, (wordlength+1)<<3,
		dev->mem_start, dev->mem_end-1);

    else
    {
	ei_status.tx_start_page = EL2_MB1_START_PG;
	ei_status.rx_start_page = EL2_MB1_START_PG + TX_PAGES;
	pr_info("%s: %s, %dkB RAM, using programmed I/O (REJUMPER for SHARED MEMORY).\n",
	       dev->name, ei_status.name, (wordlength+1)<<3);
    }
    release_region(ioaddr + 0x400, 8);
    return 0;
out1:
    release_region(ioaddr + 0x400, 8);
out:
    release_region(ioaddr, EL2_IO_EXTENT);
    return retval;
}

static irqreturn_t el2_probe_interrupt(int irq, void *seen)
{
	*(bool *)seen = true;
	return IRQ_HANDLED;
}

static int
el2_open(struct net_device *dev)
{
    int retval;

    if (dev->irq < 2) {
	static const int irqlist[] = {5, 9, 3, 4, 0};
	const int *irqp = irqlist;

	outb(EGACFR_NORM, E33G_GACFR);	
	do {
		bool seen;

		retval = request_irq(*irqp, el2_probe_interrupt, 0,
				     dev->name, &seen);
		if (retval == -EBUSY)
			continue;
		if (retval < 0)
			goto err_disable;

		
		seen = false;
		smp_wmb();
		outb_p(0x04 << ((*irqp == 9) ? 2 : *irqp), E33G_IDCFR);
		outb_p(0x00, E33G_IDCFR);
		msleep(1);
		free_irq(*irqp, &seen);
		if (!seen)
			continue;

		retval = request_irq(dev->irq = *irqp, eip_interrupt, 0,
				     dev->name, dev);
		if (retval == -EBUSY)
			continue;
		if (retval < 0)
			goto err_disable;
		break;
	} while (*++irqp);

	if (*irqp == 0) {
	err_disable:
	    outb(EGACFR_IRQOFF, E33G_GACFR);	
	    return -EAGAIN;
	}
    } else {
	if ((retval = request_irq(dev->irq, eip_interrupt, 0, dev->name, dev))) {
	    return retval;
	}
    }

    el2_init_card(dev);
    eip_open(dev);
    return 0;
}

static int
el2_close(struct net_device *dev)
{
    free_irq(dev->irq, dev);
    dev->irq = ei_status.saved_irq;
    outb(EGACFR_IRQOFF, E33G_GACFR);	

    eip_close(dev);
    return 0;
}

static void
el2_reset_8390(struct net_device *dev)
{
    if (ei_debug > 1) {
	pr_debug("%s: Resetting the 3c503 board...", dev->name);
	pr_cont(" %#lx=%#02x %#lx=%#02x %#lx=%#02x...", E33G_IDCFR, inb(E33G_IDCFR),
	       E33G_CNTRL, inb(E33G_CNTRL), E33G_GACFR, inb(E33G_GACFR));
    }
    outb_p(ECNTRL_RESET|ECNTRL_THIN, E33G_CNTRL);
    ei_status.txing = 0;
    outb_p(ei_status.interface_num==0 ? ECNTRL_THIN : ECNTRL_AUI, E33G_CNTRL);
    el2_init_card(dev);
    if (ei_debug > 1)
	pr_cont("done\n");
}

static void
el2_init_card(struct net_device *dev)
{
    
    outb_p(ei_status.interface_num==0 ? ECNTRL_THIN : ECNTRL_AUI, E33G_CNTRL);

    
    
    outb(ei_status.rx_start_page, E33G_STARTPG);
    outb(ei_status.stop_page,  E33G_STOPPG);

    
    outb(0xff, E33G_VP2);	
    outb(0xff, E33G_VP1);
    outb(0x00, E33G_VP0);
    
    outb_p(0x00,  dev->base_addr + EN0_IMR);
    
    outb(EGACFR_NORM, E33G_GACFR);

    
    outb_p((0x04 << (dev->irq == 9 ? 2 : dev->irq)), E33G_IDCFR);
    outb_p((WRD_COUNT << 1), E33G_DRQCNT);	
    outb_p(0x20, E33G_DMAAH);	
    outb_p(0x00, E33G_DMAAL);
    return;			
}

static void
el2_block_output(struct net_device *dev, int count,
		 const unsigned char *buf, int start_page)
{
    unsigned short int *wrd;
    int boguscount;		
    unsigned short word;	
    void __iomem *base = ei_status.mem;

    if (ei_status.word16)      
	outb(EGACFR_RSEL|EGACFR_TCM, E33G_GACFR);
    else
	outb(EGACFR_NORM, E33G_GACFR);

    if (base) {	
	memcpy_toio(base + ((start_page - ei_status.tx_start_page) << 8),
			buf, count);
	outb(EGACFR_NORM, E33G_GACFR);	
	return;
    }


    word = (unsigned short)start_page;
    outb(word&0xFF, E33G_DMAAH);
    outb(word>>8, E33G_DMAAL);

    outb_p((ei_status.interface_num ? ECNTRL_AUI : ECNTRL_THIN ) | ECNTRL_OUTPUT
	   | ECNTRL_START, E33G_CNTRL);

    wrd = (unsigned short int *) buf;
    count  = (count + 1) >> 1;
    for(;;)
    {
        boguscount = 0x1000;
        while ((inb(E33G_STATUS) & ESTAT_DPRDY) == 0)
        {
            if(!boguscount--)
            {
                pr_notice("%s: FIFO blocked in el2_block_output.\n", dev->name);
                el2_reset_8390(dev);
                goto blocked;
            }
        }
        if(count > WRD_COUNT)
        {
            outsw(E33G_FIFOH, wrd, WRD_COUNT);
            wrd   += WRD_COUNT;
            count -= WRD_COUNT;
        }
        else
        {
            outsw(E33G_FIFOH, wrd, count);
            break;
        }
    }
    blocked:;
    outb_p(ei_status.interface_num==0 ? ECNTRL_THIN : ECNTRL_AUI, E33G_CNTRL);
}

static void
el2_get_8390_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr, int ring_page)
{
    int boguscount;
    void __iomem *base = ei_status.mem;
    unsigned short word;

    if (base) {       
	void __iomem *hdr_start = base + ((ring_page - EL2_MB1_START_PG)<<8);
	memcpy_fromio(hdr, hdr_start, sizeof(struct e8390_pkt_hdr));
	hdr->count = le16_to_cpu(hdr->count);
	return;
    }


    word = (unsigned short)ring_page;
    outb(word&0xFF, E33G_DMAAH);
    outb(word>>8, E33G_DMAAL);

    outb_p((ei_status.interface_num == 0 ? ECNTRL_THIN : ECNTRL_AUI) | ECNTRL_INPUT
	   | ECNTRL_START, E33G_CNTRL);
    boguscount = 0x1000;
    while ((inb(E33G_STATUS) & ESTAT_DPRDY) == 0)
    {
        if(!boguscount--)
        {
            pr_notice("%s: FIFO blocked in el2_get_8390_hdr.\n", dev->name);
            memset(hdr, 0x00, sizeof(struct e8390_pkt_hdr));
            el2_reset_8390(dev);
            goto blocked;
        }
    }
    insw(E33G_FIFOH, hdr, (sizeof(struct e8390_pkt_hdr))>> 1);
    blocked:;
    outb_p(ei_status.interface_num == 0 ? ECNTRL_THIN : ECNTRL_AUI, E33G_CNTRL);
}


static void
el2_block_input(struct net_device *dev, int count, struct sk_buff *skb, int ring_offset)
{
    int boguscount = 0;
    void __iomem *base = ei_status.mem;
    unsigned short int *buf;
    unsigned short word;

    
    if (base) {	
	ring_offset -= (EL2_MB1_START_PG<<8);
	if (ring_offset + count > EL2_MEMSIZE) {
	    
	    int semi_count = EL2_MEMSIZE - ring_offset;
	    memcpy_fromio(skb->data, base + ring_offset, semi_count);
	    count -= semi_count;
	    memcpy_fromio(skb->data + semi_count, base + ei_status.priv, count);
	} else {
		memcpy_fromio(skb->data, base + ring_offset, count);
	}
	return;
    }

    word = (unsigned short) ring_offset;
    outb(word>>8, E33G_DMAAH);
    outb(word&0xFF, E33G_DMAAL);

    outb_p((ei_status.interface_num == 0 ? ECNTRL_THIN : ECNTRL_AUI) | ECNTRL_INPUT
	   | ECNTRL_START, E33G_CNTRL);


    buf =  (unsigned short int *) skb->data;
    count =  (count + 1) >> 1;
    for(;;)
    {
        boguscount = 0x1000;
        while ((inb(E33G_STATUS) & ESTAT_DPRDY) == 0)
        {
            if(!boguscount--)
            {
                pr_notice("%s: FIFO blocked in el2_block_input.\n", dev->name);
                el2_reset_8390(dev);
                goto blocked;
            }
        }
        if(count > WRD_COUNT)
        {
            insw(E33G_FIFOH, buf, WRD_COUNT);
            buf   += WRD_COUNT;
            count -= WRD_COUNT;
        }
        else
        {
            insw(E33G_FIFOH, buf, count);
            break;
        }
    }
    blocked:;
    outb_p(ei_status.interface_num == 0 ? ECNTRL_THIN : ECNTRL_AUI, E33G_CNTRL);
}


static void netdev_get_drvinfo(struct net_device *dev,
			       struct ethtool_drvinfo *info)
{
	strcpy(info->driver, DRV_NAME);
	strcpy(info->version, DRV_VERSION);
	sprintf(info->bus_info, "ISA 0x%lx", dev->base_addr);
}

static const struct ethtool_ops netdev_ethtool_ops = {
	.get_drvinfo		= netdev_get_drvinfo,
};

#ifdef MODULE
#define MAX_EL2_CARDS	4	

static struct net_device *dev_el2[MAX_EL2_CARDS];
static int io[MAX_EL2_CARDS];
static int irq[MAX_EL2_CARDS];
static int xcvr[MAX_EL2_CARDS];	
module_param_array(io, int, NULL, 0);
module_param_array(irq, int, NULL, 0);
module_param_array(xcvr, int, NULL, 0);
MODULE_PARM_DESC(io, "I/O base address(es)");
MODULE_PARM_DESC(irq, "IRQ number(s) (assigned)");
MODULE_PARM_DESC(xcvr, "transceiver(s) (0=internal, 1=external)");
MODULE_DESCRIPTION("3Com ISA EtherLink II, II/16 (3c503, 3c503/16) driver");
MODULE_LICENSE("GPL");

int __init
init_module(void)
{
	struct net_device *dev;
	int this_dev, found = 0;

	for (this_dev = 0; this_dev < MAX_EL2_CARDS; this_dev++) {
		if (io[this_dev] == 0)  {
			if (this_dev != 0) break; 
			pr_notice("3c503.c: Presently autoprobing (not recommended) for a single card.\n");
		}
		dev = alloc_eip_netdev();
		if (!dev)
			break;
		dev->irq = irq[this_dev];
		dev->base_addr = io[this_dev];
		dev->mem_end = xcvr[this_dev];	
		if (do_el2_probe(dev) == 0) {
			dev_el2[found++] = dev;
			continue;
		}
		free_netdev(dev);
		pr_warning("3c503.c: No 3c503 card found (i/o = 0x%x).\n", io[this_dev]);
		break;
	}
	if (found)
		return 0;
	return -ENXIO;
}

static void cleanup_card(struct net_device *dev)
{
	
	release_region(dev->base_addr, EL2_IO_EXTENT);
	if (ei_status.mem)
		iounmap(ei_status.mem);
}

void __exit
cleanup_module(void)
{
	int this_dev;

	for (this_dev = 0; this_dev < MAX_EL2_CARDS; this_dev++) {
		struct net_device *dev = dev_el2[this_dev];
		if (dev) {
			unregister_netdev(dev);
			cleanup_card(dev);
			free_netdev(dev);
		}
	}
}
#endif 
