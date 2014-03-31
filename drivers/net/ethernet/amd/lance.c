/*
	Written/copyright 1993-1998 by Donald Becker.

	Copyright 1993 United States Government as represented by the
	Director, National Security Agency.
	This software may be used and distributed according to the terms
	of the GNU General Public License, incorporated herein by reference.

	This driver is for the Allied Telesis AT1500 and HP J2405A, and should work
	with most other LANCE-based bus-master (NE2100/NE2500) ethercards.

	The author may be reached as becker@scyld.com, or C/O
	Scyld Computing Corporation
	410 Severn Ave., Suite 210
	Annapolis MD 21403

	Andrey V. Savochkin:
	- alignment problem with 1.3.* kernel and some minor changes.
	Thomas Bogendoerfer (tsbogend@bigbug.franken.de):
	- added support for Linux/Alpha, but removed most of it, because
        it worked only for the PCI chip.
      - added hook for the 32bit lance driver
      - added PCnetPCI II (79C970A) to chip table
	Paul Gortmaker (gpg109@rsphy1.anu.edu.au):
	- hopefully fix above so Linux/Alpha can use ISA cards too.
    8/20/96 Fixed 7990 autoIRQ failure and reversed unneeded alignment -djb
    v1.12 10/27/97 Module support -djb
    v1.14  2/3/98 Module support modified, made PCI support optional -djb
    v1.15 5/27/99 Fixed bug in the cleanup_module(). dev->priv was freed
                  before unregister_netdev() which caused NULL pointer
                  reference later in the chain (in rtnetlink_fill_ifinfo())
                  -- Mika Kuoppala <miku@iki.fi>

    Forward ported v1.14 to 2.1.129, merged the PCI and misc changes from
    the 2.1 version of the old driver - Alan Cox

    Get rid of check_region, check kmalloc return in lance_probe1
    Arnaldo Carvalho de Melo <acme@conectiva.com.br> - 11/01/2001

	Reworked detection, added support for Racal InterLan EtherBlaster cards
	Vesselin Kostadinov <vesok at yahoo dot com > - 22/4/2004
*/

static const char version[] = "lance.c:v1.16 2006/11/09 dplatt@3do.com, becker@cesdis.gsfc.nasa.gov\n";

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/mm.h>
#include <linux/bitops.h>

#include <asm/io.h>
#include <asm/dma.h>

static unsigned int lance_portlist[] __initdata = { 0x300, 0x320, 0x340, 0x360, 0};
static int lance_probe1(struct net_device *dev, int ioaddr, int irq, int options);
static int __init do_lance_probe(struct net_device *dev);


static struct card {
	char id_offset14;
	char id_offset15;
} cards[] = {
	{	
		.id_offset14 = 0x57,
		.id_offset15 = 0x57,
	},
	{	
		.id_offset14 = 0x52,
		.id_offset15 = 0x44,
	},
	{	
		.id_offset14 = 0x52,
		.id_offset15 = 0x49,
	},
};
#define NUM_CARDS 3

#ifdef LANCE_DEBUG
static int lance_debug = LANCE_DEBUG;
#else
static int lance_debug = 1;
#endif


#ifndef LANCE_LOG_TX_BUFFERS
#define LANCE_LOG_TX_BUFFERS 4
#define LANCE_LOG_RX_BUFFERS 4
#endif

#define TX_RING_SIZE			(1 << (LANCE_LOG_TX_BUFFERS))
#define TX_RING_MOD_MASK		(TX_RING_SIZE - 1)
#define TX_RING_LEN_BITS		((LANCE_LOG_TX_BUFFERS) << 29)

#define RX_RING_SIZE			(1 << (LANCE_LOG_RX_BUFFERS))
#define RX_RING_MOD_MASK		(RX_RING_SIZE - 1)
#define RX_RING_LEN_BITS		((LANCE_LOG_RX_BUFFERS) << 29)

#define PKT_BUF_SZ		1544

#define LANCE_DATA 0x10
#define LANCE_ADDR 0x12
#define LANCE_RESET 0x14
#define LANCE_BUS_IF 0x16
#define LANCE_TOTAL_SIZE 0x18

#define TX_TIMEOUT	(HZ/5)

struct lance_rx_head {
	s32 base;
	s16 buf_length;			
	s16 msg_length;			
};

struct lance_tx_head {
	s32 base;
	s16 length;				
	s16 misc;
};

struct lance_init_block {
	u16 mode;		
	u8  phys_addr[6]; 
	u32 filter[2];			
	
	u32  rx_ring;			
	u32  tx_ring;
};

struct lance_private {
	
	struct lance_rx_head rx_ring[RX_RING_SIZE];
	struct lance_tx_head tx_ring[TX_RING_SIZE];
	struct lance_init_block	init_block;
	const char *name;
	
	struct sk_buff* tx_skbuff[TX_RING_SIZE];
	
	struct sk_buff* rx_skbuff[RX_RING_SIZE];
	unsigned long rx_buffs;		
	
	char (*tx_bounce_buffs)[PKT_BUF_SZ];
	int cur_rx, cur_tx;			
	int dirty_rx, dirty_tx;		
	int dma;
	unsigned char chip_version;	
	spinlock_t devlock;
};

#define LANCE_MUST_PAD          0x00000001
#define LANCE_ENABLE_AUTOSELECT 0x00000002
#define LANCE_MUST_REINIT_RING  0x00000004
#define LANCE_MUST_UNRESET      0x00000008
#define LANCE_HAS_MISSED_FRAME  0x00000010

static struct lance_chip_type {
	int id_number;
	const char *name;
	int flags;
} chip_table[] = {
	{0x0000, "LANCE 7990",				
		LANCE_MUST_PAD + LANCE_MUST_UNRESET},
	{0x0003, "PCnet/ISA 79C960",		
		LANCE_ENABLE_AUTOSELECT + LANCE_MUST_REINIT_RING +
			LANCE_HAS_MISSED_FRAME},
	{0x2260, "PCnet/ISA+ 79C961",		
		LANCE_ENABLE_AUTOSELECT + LANCE_MUST_REINIT_RING +
			LANCE_HAS_MISSED_FRAME},
	{0x2420, "PCnet/PCI 79C970",		
		LANCE_ENABLE_AUTOSELECT + LANCE_MUST_REINIT_RING +
			LANCE_HAS_MISSED_FRAME},
	{0x2430, "PCnet32",					
		LANCE_ENABLE_AUTOSELECT + LANCE_MUST_REINIT_RING +
			LANCE_HAS_MISSED_FRAME},
        {0x2621, "PCnet/PCI-II 79C970A",        
                LANCE_ENABLE_AUTOSELECT + LANCE_MUST_REINIT_RING +
                        LANCE_HAS_MISSED_FRAME},
	{0x0, 	 "PCnet (unknown)",
		LANCE_ENABLE_AUTOSELECT + LANCE_MUST_REINIT_RING +
			LANCE_HAS_MISSED_FRAME},
};

enum {OLD_LANCE = 0, PCNET_ISA=1, PCNET_ISAP=2, PCNET_PCI=3, PCNET_VLB=4, PCNET_PCI_II=5, LANCE_UNKNOWN=6};


static unsigned char lance_need_isa_bounce_buffers = 1;

static int lance_open(struct net_device *dev);
static void lance_init_ring(struct net_device *dev, gfp_t mode);
static netdev_tx_t lance_start_xmit(struct sk_buff *skb,
				    struct net_device *dev);
static int lance_rx(struct net_device *dev);
static irqreturn_t lance_interrupt(int irq, void *dev_id);
static int lance_close(struct net_device *dev);
static struct net_device_stats *lance_get_stats(struct net_device *dev);
static void set_multicast_list(struct net_device *dev);
static void lance_tx_timeout (struct net_device *dev);



#ifdef MODULE
#define MAX_CARDS		8	

static struct net_device *dev_lance[MAX_CARDS];
static int io[MAX_CARDS];
static int dma[MAX_CARDS];
static int irq[MAX_CARDS];

module_param_array(io, int, NULL, 0);
module_param_array(dma, int, NULL, 0);
module_param_array(irq, int, NULL, 0);
module_param(lance_debug, int, 0);
MODULE_PARM_DESC(io, "LANCE/PCnet I/O base address(es),required");
MODULE_PARM_DESC(dma, "LANCE/PCnet ISA DMA channel (ignored for some devices)");
MODULE_PARM_DESC(irq, "LANCE/PCnet IRQ number (ignored for some devices)");
MODULE_PARM_DESC(lance_debug, "LANCE/PCnet debug level (0-7)");

int __init init_module(void)
{
	struct net_device *dev;
	int this_dev, found = 0;

	for (this_dev = 0; this_dev < MAX_CARDS; this_dev++) {
		if (io[this_dev] == 0)  {
			if (this_dev != 0) 
				break;
			printk(KERN_NOTICE "lance.c: Module autoprobing not allowed. Append \"io=0xNNN\" value(s).\n");
			return -EPERM;
		}
		dev = alloc_etherdev(0);
		if (!dev)
			break;
		dev->irq = irq[this_dev];
		dev->base_addr = io[this_dev];
		dev->dma = dma[this_dev];
		if (do_lance_probe(dev) == 0) {
			dev_lance[found++] = dev;
			continue;
		}
		free_netdev(dev);
		break;
	}
	if (found != 0)
		return 0;
	return -ENXIO;
}

static void cleanup_card(struct net_device *dev)
{
	struct lance_private *lp = dev->ml_priv;
	if (dev->dma != 4)
		free_dma(dev->dma);
	release_region(dev->base_addr, LANCE_TOTAL_SIZE);
	kfree(lp->tx_bounce_buffs);
	kfree((void*)lp->rx_buffs);
	kfree(lp);
}

void __exit cleanup_module(void)
{
	int this_dev;

	for (this_dev = 0; this_dev < MAX_CARDS; this_dev++) {
		struct net_device *dev = dev_lance[this_dev];
		if (dev) {
			unregister_netdev(dev);
			cleanup_card(dev);
			free_netdev(dev);
		}
	}
}
#endif 
MODULE_LICENSE("GPL");


static int __init do_lance_probe(struct net_device *dev)
{
	unsigned int *port;
	int result;

	if (high_memory <= phys_to_virt(16*1024*1024))
		lance_need_isa_bounce_buffers = 0;

	for (port = lance_portlist; *port; port++) {
		int ioaddr = *port;
		struct resource *r = request_region(ioaddr, LANCE_TOTAL_SIZE,
							"lance-probe");

		if (r) {
			
			char offset14 = inb(ioaddr + 14);
			int card;
			for (card = 0; card < NUM_CARDS; ++card)
				if (cards[card].id_offset14 == offset14)
					break;
			if (card < NUM_CARDS) {
				char offset15 = inb(ioaddr + 15);
				for (card = 0; card < NUM_CARDS; ++card)
					if ((cards[card].id_offset14 == offset14) &&
						(cards[card].id_offset15 == offset15))
						break;
			}
			if (card < NUM_CARDS) { 
				result = lance_probe1(dev, ioaddr, 0, 0);
				if (!result) {
					struct lance_private *lp = dev->ml_priv;
					int ver = lp->chip_version;

					r->name = chip_table[ver].name;
					return 0;
				}
			}
			release_region(ioaddr, LANCE_TOTAL_SIZE);
		}
	}
	return -ENODEV;
}

#ifndef MODULE
struct net_device * __init lance_probe(int unit)
{
	struct net_device *dev = alloc_etherdev(0);
	int err;

	if (!dev)
		return ERR_PTR(-ENODEV);

	sprintf(dev->name, "eth%d", unit);
	netdev_boot_setup_check(dev);

	err = do_lance_probe(dev);
	if (err)
		goto out;
	return dev;
out:
	free_netdev(dev);
	return ERR_PTR(err);
}
#endif

static const struct net_device_ops lance_netdev_ops = {
	.ndo_open 		= lance_open,
	.ndo_start_xmit		= lance_start_xmit,
	.ndo_stop		= lance_close,
	.ndo_get_stats		= lance_get_stats,
	.ndo_set_rx_mode	= set_multicast_list,
	.ndo_tx_timeout		= lance_tx_timeout,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};

static int __init lance_probe1(struct net_device *dev, int ioaddr, int irq, int options)
{
	struct lance_private *lp;
	unsigned long dma_channels;	
	int i, reset_val, lance_version;
	const char *chipname;
	
	unsigned char hpJ2405A = 0;	
	int hp_builtin = 0;		
	static int did_version;		
	unsigned long flags;
	int err = -ENOMEM;
	void __iomem *bios;

	bios = ioremap(0xf00f0, 0x14);
	if (!bios)
		return -ENOMEM;
	if (readw(bios + 0x12) == 0x5048)  {
		static const short ioaddr_table[] = { 0x300, 0x320, 0x340, 0x360};
		int hp_port = (readl(bios + 1) & 1)  ? 0x499 : 0x99;
		
		if ((inb(hp_port) & 0xc0) == 0x80 &&
		    ioaddr_table[inb(hp_port) & 3] == ioaddr)
			hp_builtin = hp_port;
	}
	iounmap(bios);
	
	hpJ2405A = (inb(ioaddr) == 0x08 && inb(ioaddr+1) == 0x00 &&
		    inb(ioaddr+2) == 0x09);

	
	reset_val = inw(ioaddr+LANCE_RESET); 

	if (!hpJ2405A)
		outw(reset_val, ioaddr+LANCE_RESET);

	outw(0x0000, ioaddr+LANCE_ADDR); 
	if (inw(ioaddr+LANCE_DATA) != 0x0004)
		return -ENODEV;

	
	outw(88, ioaddr+LANCE_ADDR);
	if (inw(ioaddr+LANCE_ADDR) != 88) {
		lance_version = 0;
	} else {			
		int chip_version = inw(ioaddr+LANCE_DATA);
		outw(89, ioaddr+LANCE_ADDR);
		chip_version |= inw(ioaddr+LANCE_DATA) << 16;
		if (lance_debug > 2)
			printk("  LANCE chip version is %#x.\n", chip_version);
		if ((chip_version & 0xfff) != 0x003)
			return -ENODEV;
		chip_version = (chip_version >> 12) & 0xffff;
		for (lance_version = 1; chip_table[lance_version].id_number; lance_version++) {
			if (chip_table[lance_version].id_number == chip_version)
				break;
		}
	}

	chipname = chip_table[lance_version].name;
	printk("%s: %s at %#3x, ", dev->name, chipname, ioaddr);

	for (i = 0; i < 6; i++)
		dev->dev_addr[i] = inb(ioaddr + i);
	printk("%pM", dev->dev_addr);

	dev->base_addr = ioaddr;
	

	lp = kzalloc(sizeof(*lp), GFP_DMA | GFP_KERNEL);
	if(lp==NULL)
		return -ENODEV;
	if (lance_debug > 6) printk(" (#0x%05lx)", (unsigned long)lp);
	dev->ml_priv = lp;
	lp->name = chipname;
	lp->rx_buffs = (unsigned long)kmalloc(PKT_BUF_SZ*RX_RING_SIZE,
						  GFP_DMA | GFP_KERNEL);
	if (!lp->rx_buffs)
		goto out_lp;
	if (lance_need_isa_bounce_buffers) {
		lp->tx_bounce_buffs = kmalloc(PKT_BUF_SZ*TX_RING_SIZE,
						  GFP_DMA | GFP_KERNEL);
		if (!lp->tx_bounce_buffs)
			goto out_rx;
	} else
		lp->tx_bounce_buffs = NULL;

	lp->chip_version = lance_version;
	spin_lock_init(&lp->devlock);

	lp->init_block.mode = 0x0003;		
	for (i = 0; i < 6; i++)
		lp->init_block.phys_addr[i] = dev->dev_addr[i];
	lp->init_block.filter[0] = 0x00000000;
	lp->init_block.filter[1] = 0x00000000;
	lp->init_block.rx_ring = ((u32)isa_virt_to_bus(lp->rx_ring) & 0xffffff) | RX_RING_LEN_BITS;
	lp->init_block.tx_ring = ((u32)isa_virt_to_bus(lp->tx_ring) & 0xffffff) | TX_RING_LEN_BITS;

	outw(0x0001, ioaddr+LANCE_ADDR);
	inw(ioaddr+LANCE_ADDR);
	outw((short) (u32) isa_virt_to_bus(&lp->init_block), ioaddr+LANCE_DATA);
	outw(0x0002, ioaddr+LANCE_ADDR);
	inw(ioaddr+LANCE_ADDR);
	outw(((u32)isa_virt_to_bus(&lp->init_block)) >> 16, ioaddr+LANCE_DATA);
	outw(0x0000, ioaddr+LANCE_ADDR);
	inw(ioaddr+LANCE_ADDR);

	if (irq) {					
		dev->dma = 4;			
		dev->irq = irq;
	} else if (hp_builtin) {
		static const char dma_tbl[4] = {3, 5, 6, 0};
		static const char irq_tbl[4] = {3, 4, 5, 9};
		unsigned char port_val = inb(hp_builtin);
		dev->dma = dma_tbl[(port_val >> 4) & 3];
		dev->irq = irq_tbl[(port_val >> 2) & 3];
		printk(" HP Vectra IRQ %d DMA %d.\n", dev->irq, dev->dma);
	} else if (hpJ2405A) {
		static const char dma_tbl[4] = {3, 5, 6, 7};
		static const char irq_tbl[8] = {3, 4, 5, 9, 10, 11, 12, 15};
		short reset_val = inw(ioaddr+LANCE_RESET);
		dev->dma = dma_tbl[(reset_val >> 2) & 3];
		dev->irq = irq_tbl[(reset_val >> 4) & 7];
		printk(" HP J2405A IRQ %d DMA %d.\n", dev->irq, dev->dma);
	} else if (lance_version == PCNET_ISAP) {		
		short bus_info;
		outw(8, ioaddr+LANCE_ADDR);
		bus_info = inw(ioaddr+LANCE_BUS_IF);
		dev->dma = bus_info & 0x07;
		dev->irq = (bus_info >> 4) & 0x0F;
	} else {
		
		if (dev->mem_start & 0x07)
			dev->dma = dev->mem_start & 0x07;
	}

	if (dev->dma == 0) {
		dma_channels = ((inb(DMA1_STAT_REG) >> 4) & 0x0f) |
			(inb(DMA2_STAT_REG) & 0xf0);
	}
	err = -ENODEV;
	if (dev->irq >= 2)
		printk(" assigned IRQ %d", dev->irq);
	else if (lance_version != 0)  {	
		unsigned long irq_mask;

		irq_mask = probe_irq_on();

		
		outw(0x0041, ioaddr+LANCE_DATA);

		mdelay(20);
		dev->irq = probe_irq_off(irq_mask);
		if (dev->irq)
			printk(", probed IRQ %d", dev->irq);
		else {
			printk(", failed to detect IRQ line.\n");
			goto out_tx;
		}

		if (inw(ioaddr+LANCE_DATA) & 0x0100)
			dev->dma = 4;
	}

	if (dev->dma == 4) {
		printk(", no DMA needed.\n");
	} else if (dev->dma) {
		if (request_dma(dev->dma, chipname)) {
			printk("DMA %d allocation failed.\n", dev->dma);
			goto out_tx;
		} else
			printk(", assigned DMA %d.\n", dev->dma);
	} else {			
		for (i = 0; i < 4; i++) {
			static const char dmas[] = { 5, 6, 7, 3 };
			int dma = dmas[i];
			int boguscnt;

			if (test_bit(dma, &dma_channels))
				continue;
			outw(0x7f04, ioaddr+LANCE_DATA); 
			if (request_dma(dma, chipname))
				continue;

			flags=claim_dma_lock();
			set_dma_mode(dma, DMA_MODE_CASCADE);
			enable_dma(dma);
			release_dma_lock(flags);

			
			outw(0x0001, ioaddr+LANCE_DATA);
			for (boguscnt = 100; boguscnt > 0; --boguscnt)
				if (inw(ioaddr+LANCE_DATA) & 0x0900)
					break;
			if (inw(ioaddr+LANCE_DATA) & 0x0100) {
				dev->dma = dma;
				printk(", DMA %d.\n", dev->dma);
				break;
			} else {
				flags=claim_dma_lock();
				disable_dma(dma);
				release_dma_lock(flags);
				free_dma(dma);
			}
		}
		if (i == 4) {			
			printk("DMA detection failed.\n");
			goto out_tx;
		}
	}

	if (lance_version == 0 && dev->irq == 0) {
		
		
		unsigned long irq_mask;

		irq_mask = probe_irq_on();
		outw(0x0041, ioaddr+LANCE_DATA);

		mdelay(40);
		dev->irq = probe_irq_off(irq_mask);
		if (dev->irq == 0) {
			printk("  Failed to detect the 7990 IRQ line.\n");
			goto out_dma;
		}
		printk("  Auto-IRQ detected IRQ%d.\n", dev->irq);
	}

	if (chip_table[lp->chip_version].flags & LANCE_ENABLE_AUTOSELECT) {
		outw(0x0002, ioaddr+LANCE_ADDR);
		
		outw(inw(ioaddr+LANCE_BUS_IF) | 0x0002, ioaddr+LANCE_BUS_IF);
	}

	if (lance_debug > 0  &&  did_version++ == 0)
		printk(version);

	
	dev->netdev_ops = &lance_netdev_ops;
	dev->watchdog_timeo = TX_TIMEOUT;

	err = register_netdev(dev);
	if (err)
		goto out_dma;
	return 0;
out_dma:
	if (dev->dma != 4)
		free_dma(dev->dma);
out_tx:
	kfree(lp->tx_bounce_buffs);
out_rx:
	kfree((void*)lp->rx_buffs);
out_lp:
	kfree(lp);
	return err;
}


static int
lance_open(struct net_device *dev)
{
	struct lance_private *lp = dev->ml_priv;
	int ioaddr = dev->base_addr;
	int i;

	if (dev->irq == 0 ||
		request_irq(dev->irq, lance_interrupt, 0, lp->name, dev)) {
		return -EAGAIN;
	}


	
	inw(ioaddr+LANCE_RESET);

	
	if (dev->dma != 4) {
		unsigned long flags=claim_dma_lock();
		enable_dma(dev->dma);
		set_dma_mode(dev->dma, DMA_MODE_CASCADE);
		release_dma_lock(flags);
	}

	
	if (chip_table[lp->chip_version].flags & LANCE_MUST_UNRESET)
		outw(0, ioaddr+LANCE_RESET);

	if (chip_table[lp->chip_version].flags & LANCE_ENABLE_AUTOSELECT) {
		
		outw(0x0002, ioaddr+LANCE_ADDR);
		
		outw(inw(ioaddr+LANCE_BUS_IF) | 0x0002, ioaddr+LANCE_BUS_IF);
 	}

	if (lance_debug > 1)
		printk("%s: lance_open() irq %d dma %d tx/rx rings %#x/%#x init %#x.\n",
			   dev->name, dev->irq, dev->dma,
		           (u32) isa_virt_to_bus(lp->tx_ring),
		           (u32) isa_virt_to_bus(lp->rx_ring),
			   (u32) isa_virt_to_bus(&lp->init_block));

	lance_init_ring(dev, GFP_KERNEL);
	
	outw(0x0001, ioaddr+LANCE_ADDR);
	outw((short) (u32) isa_virt_to_bus(&lp->init_block), ioaddr+LANCE_DATA);
	outw(0x0002, ioaddr+LANCE_ADDR);
	outw(((u32)isa_virt_to_bus(&lp->init_block)) >> 16, ioaddr+LANCE_DATA);

	outw(0x0004, ioaddr+LANCE_ADDR);
	outw(0x0915, ioaddr+LANCE_DATA);

	outw(0x0000, ioaddr+LANCE_ADDR);
	outw(0x0001, ioaddr+LANCE_DATA);

	netif_start_queue (dev);

	i = 0;
	while (i++ < 100)
		if (inw(ioaddr+LANCE_DATA) & 0x0100)
			break;
 	outw(0x0042, ioaddr+LANCE_DATA);

	if (lance_debug > 2)
		printk("%s: LANCE open after %d ticks, init block %#x csr0 %4.4x.\n",
			   dev->name, i, (u32) isa_virt_to_bus(&lp->init_block), inw(ioaddr+LANCE_DATA));

	return 0;					
}


static void
lance_purge_ring(struct net_device *dev)
{
	struct lance_private *lp = dev->ml_priv;
	int i;

	
	for (i = 0; i < RX_RING_SIZE; i++) {
		struct sk_buff *skb = lp->rx_skbuff[i];
		lp->rx_skbuff[i] = NULL;
		lp->rx_ring[i].base = 0;		
		if (skb)
			dev_kfree_skb_any(skb);
	}
	for (i = 0; i < TX_RING_SIZE; i++) {
		if (lp->tx_skbuff[i]) {
			dev_kfree_skb_any(lp->tx_skbuff[i]);
			lp->tx_skbuff[i] = NULL;
		}
	}
}


static void
lance_init_ring(struct net_device *dev, gfp_t gfp)
{
	struct lance_private *lp = dev->ml_priv;
	int i;

	lp->cur_rx = lp->cur_tx = 0;
	lp->dirty_rx = lp->dirty_tx = 0;

	for (i = 0; i < RX_RING_SIZE; i++) {
		struct sk_buff *skb;
		void *rx_buff;

		skb = alloc_skb(PKT_BUF_SZ, GFP_DMA | gfp);
		lp->rx_skbuff[i] = skb;
		if (skb) {
			skb->dev = dev;
			rx_buff = skb->data;
		} else
			rx_buff = kmalloc(PKT_BUF_SZ, GFP_DMA | gfp);
		if (rx_buff == NULL)
			lp->rx_ring[i].base = 0;
		else
			lp->rx_ring[i].base = (u32)isa_virt_to_bus(rx_buff) | 0x80000000;
		lp->rx_ring[i].buf_length = -PKT_BUF_SZ;
	}
	for (i = 0; i < TX_RING_SIZE; i++) {
		lp->tx_skbuff[i] = NULL;
		lp->tx_ring[i].base = 0;
	}

	lp->init_block.mode = 0x0000;
	for (i = 0; i < 6; i++)
		lp->init_block.phys_addr[i] = dev->dev_addr[i];
	lp->init_block.filter[0] = 0x00000000;
	lp->init_block.filter[1] = 0x00000000;
	lp->init_block.rx_ring = ((u32)isa_virt_to_bus(lp->rx_ring) & 0xffffff) | RX_RING_LEN_BITS;
	lp->init_block.tx_ring = ((u32)isa_virt_to_bus(lp->tx_ring) & 0xffffff) | TX_RING_LEN_BITS;
}

static void
lance_restart(struct net_device *dev, unsigned int csr0_bits, int must_reinit)
{
	struct lance_private *lp = dev->ml_priv;

	if (must_reinit ||
		(chip_table[lp->chip_version].flags & LANCE_MUST_REINIT_RING)) {
		lance_purge_ring(dev);
		lance_init_ring(dev, GFP_ATOMIC);
	}
	outw(0x0000,    dev->base_addr + LANCE_ADDR);
	outw(csr0_bits, dev->base_addr + LANCE_DATA);
}


static void lance_tx_timeout (struct net_device *dev)
{
	struct lance_private *lp = (struct lance_private *) dev->ml_priv;
	int ioaddr = dev->base_addr;

	outw (0, ioaddr + LANCE_ADDR);
	printk ("%s: transmit timed out, status %4.4x, resetting.\n",
		dev->name, inw (ioaddr + LANCE_DATA));
	outw (0x0004, ioaddr + LANCE_DATA);
	dev->stats.tx_errors++;
#ifndef final_version
	if (lance_debug > 3) {
		int i;
		printk (" Ring data dump: dirty_tx %d cur_tx %d%s cur_rx %d.",
		  lp->dirty_tx, lp->cur_tx, netif_queue_stopped(dev) ? " (full)" : "",
			lp->cur_rx);
		for (i = 0; i < RX_RING_SIZE; i++)
			printk ("%s %08x %04x %04x", i & 0x3 ? "" : "\n ",
			 lp->rx_ring[i].base, -lp->rx_ring[i].buf_length,
				lp->rx_ring[i].msg_length);
		for (i = 0; i < TX_RING_SIZE; i++)
			printk ("%s %08x %04x %04x", i & 0x3 ? "" : "\n ",
			     lp->tx_ring[i].base, -lp->tx_ring[i].length,
				lp->tx_ring[i].misc);
		printk ("\n");
	}
#endif
	lance_restart (dev, 0x0043, 1);

	dev->trans_start = jiffies; 
	netif_wake_queue (dev);
}


static netdev_tx_t lance_start_xmit(struct sk_buff *skb,
				    struct net_device *dev)
{
	struct lance_private *lp = dev->ml_priv;
	int ioaddr = dev->base_addr;
	int entry;
	unsigned long flags;

	spin_lock_irqsave(&lp->devlock, flags);

	if (lance_debug > 3) {
		outw(0x0000, ioaddr+LANCE_ADDR);
		printk("%s: lance_start_xmit() called, csr0 %4.4x.\n", dev->name,
			   inw(ioaddr+LANCE_DATA));
		outw(0x0000, ioaddr+LANCE_DATA);
	}

	

	
	entry = lp->cur_tx & TX_RING_MOD_MASK;


	
	if (chip_table[lp->chip_version].flags & LANCE_MUST_PAD) {
		if (skb->len < ETH_ZLEN) {
			if (skb_padto(skb, ETH_ZLEN))
				goto out;
			lp->tx_ring[entry].length = -ETH_ZLEN;
		}
		else
			lp->tx_ring[entry].length = -skb->len;
	} else
		lp->tx_ring[entry].length = -skb->len;

	lp->tx_ring[entry].misc = 0x0000;

	dev->stats.tx_bytes += skb->len;

	if ((u32)isa_virt_to_bus(skb->data) + skb->len > 0x01000000) {
		if (lance_debug > 5)
			printk("%s: bouncing a high-memory packet (%#x).\n",
				   dev->name, (u32)isa_virt_to_bus(skb->data));
		skb_copy_from_linear_data(skb, &lp->tx_bounce_buffs[entry], skb->len);
		lp->tx_ring[entry].base =
			((u32)isa_virt_to_bus((lp->tx_bounce_buffs + entry)) & 0xffffff) | 0x83000000;
		dev_kfree_skb(skb);
	} else {
		lp->tx_skbuff[entry] = skb;
		lp->tx_ring[entry].base = ((u32)isa_virt_to_bus(skb->data) & 0xffffff) | 0x83000000;
	}
	lp->cur_tx++;

	
	outw(0x0000, ioaddr+LANCE_ADDR);
	outw(0x0048, ioaddr+LANCE_DATA);

	if ((lp->cur_tx - lp->dirty_tx) >= TX_RING_SIZE)
		netif_stop_queue(dev);

out:
	spin_unlock_irqrestore(&lp->devlock, flags);
	return NETDEV_TX_OK;
}

static irqreturn_t lance_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct lance_private *lp;
	int csr0, ioaddr, boguscnt=10;
	int must_restart;

	ioaddr = dev->base_addr;
	lp = dev->ml_priv;

	spin_lock (&lp->devlock);

	outw(0x00, dev->base_addr + LANCE_ADDR);
	while ((csr0 = inw(dev->base_addr + LANCE_DATA)) & 0x8600 &&
	       --boguscnt >= 0) {
		
		outw(csr0 & ~0x004f, dev->base_addr + LANCE_DATA);

		must_restart = 0;

		if (lance_debug > 5)
			printk("%s: interrupt  csr0=%#2.2x new csr=%#2.2x.\n",
				   dev->name, csr0, inw(dev->base_addr + LANCE_DATA));

		if (csr0 & 0x0400)			
			lance_rx(dev);

		if (csr0 & 0x0200) {		
			int dirty_tx = lp->dirty_tx;

			while (dirty_tx < lp->cur_tx) {
				int entry = dirty_tx & TX_RING_MOD_MASK;
				int status = lp->tx_ring[entry].base;

				if (status < 0)
					break;			

				lp->tx_ring[entry].base = 0;

				if (status & 0x40000000) {
					
					int err_status = lp->tx_ring[entry].misc;
					dev->stats.tx_errors++;
					if (err_status & 0x0400)
						dev->stats.tx_aborted_errors++;
					if (err_status & 0x0800)
						dev->stats.tx_carrier_errors++;
					if (err_status & 0x1000)
						dev->stats.tx_window_errors++;
					if (err_status & 0x4000) {
						
						dev->stats.tx_fifo_errors++;
						
						printk("%s: Tx FIFO error! Status %4.4x.\n",
							   dev->name, csr0);
						
						must_restart = 1;
					}
				} else {
					if (status & 0x18000000)
						dev->stats.collisions++;
					dev->stats.tx_packets++;
				}

				if (lp->tx_skbuff[entry]) {
					dev_kfree_skb_irq(lp->tx_skbuff[entry]);
					lp->tx_skbuff[entry] = NULL;
				}
				dirty_tx++;
			}

#ifndef final_version
			if (lp->cur_tx - dirty_tx >= TX_RING_SIZE) {
				printk("out-of-sync dirty pointer, %d vs. %d, full=%s.\n",
					   dirty_tx, lp->cur_tx,
					   netif_queue_stopped(dev) ? "yes" : "no");
				dirty_tx += TX_RING_SIZE;
			}
#endif

			
			if (netif_queue_stopped(dev) &&
			    dirty_tx > lp->cur_tx - TX_RING_SIZE + 2)
				netif_wake_queue (dev);

			lp->dirty_tx = dirty_tx;
		}

		
		if (csr0 & 0x4000)
			dev->stats.tx_errors++; 
		if (csr0 & 0x1000)
			dev->stats.rx_errors++; 
		if (csr0 & 0x0800) {
			printk("%s: Bus master arbitration failure, status %4.4x.\n",
				   dev->name, csr0);
			
			must_restart = 1;
		}

		if (must_restart) {
			
			outw(0x0000, dev->base_addr + LANCE_ADDR);
			outw(0x0004, dev->base_addr + LANCE_DATA);
			lance_restart(dev, 0x0002, 0);
		}
	}

	
	outw(0x0000, dev->base_addr + LANCE_ADDR);
	outw(0x7940, dev->base_addr + LANCE_DATA);

	if (lance_debug > 4)
		printk("%s: exiting interrupt, csr%d=%#4.4x.\n",
			   dev->name, inw(ioaddr + LANCE_ADDR),
			   inw(dev->base_addr + LANCE_DATA));

	spin_unlock (&lp->devlock);
	return IRQ_HANDLED;
}

static int
lance_rx(struct net_device *dev)
{
	struct lance_private *lp = dev->ml_priv;
	int entry = lp->cur_rx & RX_RING_MOD_MASK;
	int i;

	
	while (lp->rx_ring[entry].base >= 0) {
		int status = lp->rx_ring[entry].base >> 24;

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
			lp->rx_ring[entry].base &= 0x03ffffff;
		}
		else
		{
			
			short pkt_len = (lp->rx_ring[entry].msg_length & 0xfff)-4;
			struct sk_buff *skb;

			if(pkt_len<60)
			{
				printk("%s: Runt packet!\n",dev->name);
				dev->stats.rx_errors++;
			}
			else
			{
				skb = dev_alloc_skb(pkt_len+2);
				if (skb == NULL)
				{
					printk("%s: Memory squeeze, deferring packet.\n", dev->name);
					for (i=0; i < RX_RING_SIZE; i++)
						if (lp->rx_ring[(entry+i) & RX_RING_MOD_MASK].base < 0)
							break;

					if (i > RX_RING_SIZE -2)
					{
						dev->stats.rx_dropped++;
						lp->rx_ring[entry].base |= 0x80000000;
						lp->cur_rx++;
					}
					break;
				}
				skb_reserve(skb,2);	
				skb_put(skb,pkt_len);	
				skb_copy_to_linear_data(skb,
					(unsigned char *)isa_bus_to_virt((lp->rx_ring[entry].base & 0x00ffffff)),
					pkt_len);
				skb->protocol=eth_type_trans(skb,dev);
				netif_rx(skb);
				dev->stats.rx_packets++;
				dev->stats.rx_bytes += pkt_len;
			}
		}
		lp->rx_ring[entry].buf_length = -PKT_BUF_SZ;
		lp->rx_ring[entry].base |= 0x80000000;
		entry = (++lp->cur_rx) & RX_RING_MOD_MASK;
	}


	return 0;
}

static int
lance_close(struct net_device *dev)
{
	int ioaddr = dev->base_addr;
	struct lance_private *lp = dev->ml_priv;

	netif_stop_queue (dev);

	if (chip_table[lp->chip_version].flags & LANCE_HAS_MISSED_FRAME) {
		outw(112, ioaddr+LANCE_ADDR);
		dev->stats.rx_missed_errors = inw(ioaddr+LANCE_DATA);
	}
	outw(0, ioaddr+LANCE_ADDR);

	if (lance_debug > 1)
		printk("%s: Shutting down ethercard, status was %2.2x.\n",
			   dev->name, inw(ioaddr+LANCE_DATA));

	outw(0x0004, ioaddr+LANCE_DATA);

	if (dev->dma != 4)
	{
		unsigned long flags=claim_dma_lock();
		disable_dma(dev->dma);
		release_dma_lock(flags);
	}
	free_irq(dev->irq, dev);

	lance_purge_ring(dev);

	return 0;
}

static struct net_device_stats *lance_get_stats(struct net_device *dev)
{
	struct lance_private *lp = dev->ml_priv;

	if (chip_table[lp->chip_version].flags & LANCE_HAS_MISSED_FRAME) {
		short ioaddr = dev->base_addr;
		short saved_addr;
		unsigned long flags;

		spin_lock_irqsave(&lp->devlock, flags);
		saved_addr = inw(ioaddr+LANCE_ADDR);
		outw(112, ioaddr+LANCE_ADDR);
		dev->stats.rx_missed_errors = inw(ioaddr+LANCE_DATA);
		outw(saved_addr, ioaddr+LANCE_ADDR);
		spin_unlock_irqrestore(&lp->devlock, flags);
	}

	return &dev->stats;
}


static void set_multicast_list(struct net_device *dev)
{
	short ioaddr = dev->base_addr;

	outw(0, ioaddr+LANCE_ADDR);
	outw(0x0004, ioaddr+LANCE_DATA); 

	if (dev->flags&IFF_PROMISC) {
		outw(15, ioaddr+LANCE_ADDR);
		outw(0x8000, ioaddr+LANCE_DATA); 
	} else {
		short multicast_table[4];
		int i;
		int num_addrs=netdev_mc_count(dev);
		if(dev->flags&IFF_ALLMULTI)
			num_addrs=1;
		
		memset(multicast_table, (num_addrs == 0) ? 0 : -1, sizeof(multicast_table));
		for (i = 0; i < 4; i++) {
			outw(8 + i, ioaddr+LANCE_ADDR);
			outw(multicast_table[i], ioaddr+LANCE_DATA);
		}
		outw(15, ioaddr+LANCE_ADDR);
		outw(0x0000, ioaddr+LANCE_DATA); 
	}

	lance_restart(dev, 0x0142, 0); 

}

