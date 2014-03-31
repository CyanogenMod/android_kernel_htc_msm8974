/* Based on the former daynaport.c driver, by Alan Cox.  Some code
   taken from or inspired by skeleton.c by Donald Becker, acenic.c by
   Jes Sorensen, and ne2k-pci.c by Donald Becker and Paul Gortmaker.

   This software may be used and distributed according to the terms of
   the GNU Public License, incorporated herein by reference.  */


#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/nubus.h>
#include <linux/in.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/bitops.h>
#include <linux/io.h>

#include <asm/dma.h>
#include <asm/hwtest.h>
#include <asm/macints.h>

static char version[] =
	"v0.4 2001-05-15 David Huggins-Daines <dhd@debian.org> and others\n";

#define EI_SHIFT(x)	(ei_local->reg_offset[x])
#define ei_inb(port)	in_8(port)
#define ei_outb(val, port)	out_8(port, val)
#define ei_inb_p(port)	in_8(port)
#define ei_outb_p(val, port)	out_8(port, val)

#include "lib8390.c"

#define WD_START_PG			0x00	
#define CABLETRON_RX_START_PG		0x00    
#define CABLETRON_RX_STOP_PG		0x30    
#define CABLETRON_TX_START_PG		CABLETRON_RX_STOP_PG
						


#define DAYNA_8390_BASE		0x80000
#define DAYNA_8390_MEM		0x00000

#define CABLETRON_8390_BASE	0x90000
#define CABLETRON_8390_MEM	0x00000

#define INTERLAN_8390_BASE	0xE0000
#define INTERLAN_8390_MEM	0xD0000

enum mac8390_type {
	MAC8390_NONE = -1,
	MAC8390_APPLE,
	MAC8390_ASANTE,
	MAC8390_FARALLON,
	MAC8390_CABLETRON,
	MAC8390_DAYNA,
	MAC8390_INTERLAN,
	MAC8390_KINETICS,
};

static const char *cardname[] = {
	"apple",
	"asante",
	"farallon",
	"cabletron",
	"dayna",
	"interlan",
	"kinetics",
};

static const int word16[] = {
	1, 
	1, 
	1, 
	1, 
	0, 
	1, 
	0, 
};

static const int useresources[] = {
	1, 
	1, 
	1, 
	0, 
	0, 
	0, 
	0, 
};

enum mac8390_access {
	ACCESS_UNKNOWN = 0,
	ACCESS_32,
	ACCESS_16,
};

extern int mac8390_memtest(struct net_device *dev);
static int mac8390_initdev(struct net_device *dev, struct nubus_dev *ndev,
			   enum mac8390_type type);

static int mac8390_open(struct net_device *dev);
static int mac8390_close(struct net_device *dev);
static void mac8390_no_reset(struct net_device *dev);
static void interlan_reset(struct net_device *dev);

static void sane_get_8390_hdr(struct net_device *dev,
			      struct e8390_pkt_hdr *hdr, int ring_page);
static void sane_block_input(struct net_device *dev, int count,
			     struct sk_buff *skb, int ring_offset);
static void sane_block_output(struct net_device *dev, int count,
			      const unsigned char *buf, const int start_page);

static void dayna_memcpy_fromcard(struct net_device *dev, void *to,
				int from, int count);
static void dayna_memcpy_tocard(struct net_device *dev, int to,
			      const void *from, int count);

static void dayna_get_8390_hdr(struct net_device *dev,
			       struct e8390_pkt_hdr *hdr, int ring_page);
static void dayna_block_input(struct net_device *dev, int count,
			      struct sk_buff *skb, int ring_offset);
static void dayna_block_output(struct net_device *dev, int count,
			       const unsigned char *buf, int start_page);

#define memcpy_fromio(a, b, c)	memcpy((a), (void *)(b), (c))
#define memcpy_toio(a, b, c)	memcpy((void *)(a), (b), (c))

#define memcmp_withio(a, b, c)	memcmp((a), (void *)(b), (c))

static void slow_sane_get_8390_hdr(struct net_device *dev,
				   struct e8390_pkt_hdr *hdr, int ring_page);
static void slow_sane_block_input(struct net_device *dev, int count,
				  struct sk_buff *skb, int ring_offset);
static void slow_sane_block_output(struct net_device *dev, int count,
				   const unsigned char *buf, int start_page);
static void word_memcpy_tocard(unsigned long tp, const void *fp, int count);
static void word_memcpy_fromcard(void *tp, unsigned long fp, int count);

static enum mac8390_type __init mac8390_ident(struct nubus_dev *dev)
{
	switch (dev->dr_sw) {
	case NUBUS_DRSW_3COM:
		switch (dev->dr_hw) {
		case NUBUS_DRHW_APPLE_SONIC_NB:
		case NUBUS_DRHW_APPLE_SONIC_LC:
		case NUBUS_DRHW_SONNET:
			return MAC8390_NONE;
			break;
		default:
			return MAC8390_APPLE;
			break;
		}
		break;

	case NUBUS_DRSW_APPLE:
		switch (dev->dr_hw) {
		case NUBUS_DRHW_ASANTE_LC:
			return MAC8390_NONE;
			break;
		case NUBUS_DRHW_CABLETRON:
			return MAC8390_CABLETRON;
			break;
		default:
			return MAC8390_APPLE;
			break;
		}
		break;

	case NUBUS_DRSW_ASANTE:
		return MAC8390_ASANTE;
		break;

	case NUBUS_DRSW_TECHWORKS:
	case NUBUS_DRSW_DAYNA2:
	case NUBUS_DRSW_DAYNA_LC:
		if (dev->dr_hw == NUBUS_DRHW_CABLETRON)
			return MAC8390_CABLETRON;
		else
			return MAC8390_APPLE;
		break;

	case NUBUS_DRSW_FARALLON:
		return MAC8390_FARALLON;
		break;

	case NUBUS_DRSW_KINETICS:
		switch (dev->dr_hw) {
		case NUBUS_DRHW_INTERLAN:
			return MAC8390_INTERLAN;
			break;
		default:
			return MAC8390_KINETICS;
			break;
		}
		break;

	case NUBUS_DRSW_DAYNA:
		if (dev->dr_hw == NUBUS_DRHW_SMC9194 ||
		    dev->dr_hw == NUBUS_DRHW_INTERLAN)
			return MAC8390_NONE;
		else
			return MAC8390_DAYNA;
		break;
	}
	return MAC8390_NONE;
}

static enum mac8390_access __init mac8390_testio(volatile unsigned long membase)
{
	unsigned long outdata = 0xA5A0B5B0;
	unsigned long indata =  0x00000000;
	
	memcpy_toio(membase, &outdata, 4);
	
	if (memcmp_withio(&outdata, membase, 4) == 0)
		return ACCESS_32;
	
	word_memcpy_tocard(membase, &outdata, 4);
	
	word_memcpy_fromcard(&indata, membase, 4);
	if (outdata == indata)
		return ACCESS_16;
	return ACCESS_UNKNOWN;
}

static int __init mac8390_memsize(unsigned long membase)
{
	unsigned long flags;
	int i, j;

	local_irq_save(flags);
	
	for (i = 0; i < 8; i++) {
		volatile unsigned short *m = (unsigned short *)(membase + (i * 0x1000));

		if (hwreg_present(m) == 0)
			break;

		
		*m = 0xA5A0 | i;
		
		if (*m != (0xA5A0 | i))
			break;

		
		for (j = 0; j < i; j++) {
			volatile unsigned short *p = (unsigned short *)(membase + (j * 0x1000));
			if (*p != (0xA5A0 | j))
				break;
		}
	}
	local_irq_restore(flags);
	return i * 0x1000;
}

static bool __init mac8390_init(struct net_device *dev, struct nubus_dev *ndev,
				enum mac8390_type cardtype)
{
	struct nubus_dir dir;
	struct nubus_dirent ent;
	int offset;
	volatile unsigned short *i;

	printk_once(KERN_INFO pr_fmt("%s"), version);

	dev->irq = SLOT2IRQ(ndev->board->slot);
	
	dev->base_addr = (ndev->board->slot_addr |
			  ((ndev->board->slot & 0xf) << 20));


	if (nubus_get_func_dir(ndev, &dir) == -1) {
		pr_err("%s: Unable to get Nubus functional directory for slot %X!\n",
		       dev->name, ndev->board->slot);
		return false;
	}

	
	if (nubus_find_rsrc(&dir, NUBUS_RESID_MAC_ADDRESS, &ent) == -1) {
		pr_info("%s: Couldn't get MAC address!\n", dev->name);
		return false;
	}

	nubus_get_rsrc_mem(dev->dev_addr, &ent, 6);

	if (useresources[cardtype] == 1) {
		nubus_rewinddir(&dir);
		if (nubus_find_rsrc(&dir, NUBUS_RESID_MINOR_BASEOS,
				    &ent) == -1) {
			pr_err("%s: Memory offset resource for slot %X not found!\n",
			       dev->name, ndev->board->slot);
			return false;
		}
		nubus_get_rsrc_mem(&offset, &ent, 4);
		dev->mem_start = dev->base_addr + offset;
		
		dev->base_addr = dev->mem_start + 0x10000;
		nubus_rewinddir(&dir);
		if (nubus_find_rsrc(&dir, NUBUS_RESID_MINOR_LENGTH,
				    &ent) == -1) {
			pr_info("%s: Memory length resource for slot %X not found, probing\n",
				dev->name, ndev->board->slot);
			offset = mac8390_memsize(dev->mem_start);
		} else {
			nubus_get_rsrc_mem(&offset, &ent, 4);
		}
		dev->mem_end = dev->mem_start + offset;
	} else {
		switch (cardtype) {
		case MAC8390_KINETICS:
		case MAC8390_DAYNA: 
			dev->base_addr = (int)(ndev->board->slot_addr +
					       DAYNA_8390_BASE);
			dev->mem_start = (int)(ndev->board->slot_addr +
					       DAYNA_8390_MEM);
			dev->mem_end = dev->mem_start +
				       mac8390_memsize(dev->mem_start);
			break;
		case MAC8390_INTERLAN:
			dev->base_addr = (int)(ndev->board->slot_addr +
					       INTERLAN_8390_BASE);
			dev->mem_start = (int)(ndev->board->slot_addr +
					       INTERLAN_8390_MEM);
			dev->mem_end = dev->mem_start +
				       mac8390_memsize(dev->mem_start);
			break;
		case MAC8390_CABLETRON:
			dev->base_addr = (int)(ndev->board->slot_addr +
					       CABLETRON_8390_BASE);
			dev->mem_start = (int)(ndev->board->slot_addr +
					       CABLETRON_8390_MEM);
			/* The base address is unreadable if 0x00
			 * has been written to the command register
			 * Reset the chip by writing E8390_NODMA +
			 *   E8390_PAGE0 + E8390_STOP just to be
			 *   sure
			 */
			i = (void *)dev->base_addr;
			*i = 0x21;
			dev->mem_end = dev->mem_start +
				       mac8390_memsize(dev->mem_start);
			break;

		default:
			pr_err("Card type %s is unsupported, sorry\n",
			       ndev->board->name);
			return false;
		}
	}

	return true;
}

struct net_device * __init mac8390_probe(int unit)
{
	struct net_device *dev;
	struct nubus_dev *ndev = NULL;
	int err = -ENODEV;

	static unsigned int slots;

	enum mac8390_type cardtype;

	

	if (!MACH_IS_MAC)
		return ERR_PTR(-ENODEV);

	dev = ____alloc_ei_netdev(0);
	if (!dev)
		return ERR_PTR(-ENOMEM);

	if (unit >= 0)
		sprintf(dev->name, "eth%d", unit);

	while ((ndev = nubus_find_type(NUBUS_CAT_NETWORK, NUBUS_TYPE_ETHERNET,
				       ndev))) {
		
		if (slots & (1 << ndev->board->slot))
			continue;
		slots |= 1 << ndev->board->slot;

		cardtype = mac8390_ident(ndev);
		if (cardtype == MAC8390_NONE)
			continue;

		if (!mac8390_init(dev, ndev, cardtype))
			continue;

		
		if (!mac8390_initdev(dev, ndev, cardtype))
			break;
	}

	if (!ndev)
		goto out;
	err = register_netdev(dev);
	if (err)
		goto out;
	return dev;

out:
	free_netdev(dev);
	return ERR_PTR(err);
}

#ifdef MODULE
MODULE_AUTHOR("David Huggins-Daines <dhd@debian.org> and others");
MODULE_DESCRIPTION("Macintosh NS8390-based Nubus Ethernet driver");
MODULE_LICENSE("GPL");

static struct net_device *dev_mac8390[15];
int init_module(void)
{
	int i;
	for (i = 0; i < 15; i++) {
		struct net_device *dev = mac8390_probe(-1);
		if (IS_ERR(dev))
			break;
		dev_mac890[i] = dev;
	}
	if (!i) {
		pr_notice("No useable cards found, driver NOT installed.\n");
		return -ENODEV;
	}
	return 0;
}

void cleanup_module(void)
{
	int i;
	for (i = 0; i < 15; i++) {
		struct net_device *dev = dev_mac890[i];
		if (dev) {
			unregister_netdev(dev);
			free_netdev(dev);
		}
	}
}

#endif 

static const struct net_device_ops mac8390_netdev_ops = {
	.ndo_open 		= mac8390_open,
	.ndo_stop		= mac8390_close,
	.ndo_start_xmit		= __ei_start_xmit,
	.ndo_tx_timeout		= __ei_tx_timeout,
	.ndo_get_stats		= __ei_get_stats,
	.ndo_set_rx_mode	= __ei_set_multicast_list,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_change_mtu		= eth_change_mtu,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= __ei_poll,
#endif
};

static int __init mac8390_initdev(struct net_device *dev,
				  struct nubus_dev *ndev,
				  enum mac8390_type type)
{
	static u32 fwrd4_offsets[16] = {
		0,      4,      8,      12,
		16,     20,     24,     28,
		32,     36,     40,     44,
		48,     52,     56,     60
	};
	static u32 back4_offsets[16] = {
		60,     56,     52,     48,
		44,     40,     36,     32,
		28,     24,     20,     16,
		12,     8,      4,      0
	};
	static u32 fwrd2_offsets[16] = {
		0,      2,      4,      6,
		8,     10,     12,     14,
		16,    18,     20,     22,
		24,    26,     28,     30
	};

	int access_bitmode = 0;

	
	dev->netdev_ops = &mac8390_netdev_ops;

	
	ei_status.name = cardname[type];
	ei_status.word16 = word16[type];

	
	if (type == MAC8390_CABLETRON) {
		ei_status.tx_start_page = CABLETRON_TX_START_PG;
		ei_status.rx_start_page = CABLETRON_RX_START_PG;
		ei_status.stop_page = CABLETRON_RX_STOP_PG;
		ei_status.rmem_start = dev->mem_start;
		ei_status.rmem_end = dev->mem_start + CABLETRON_RX_STOP_PG*256;
	} else {
		ei_status.tx_start_page = WD_START_PG;
		ei_status.rx_start_page = WD_START_PG + TX_PAGES;
		ei_status.stop_page = (dev->mem_end - dev->mem_start)/256;
		ei_status.rmem_start = dev->mem_start + TX_PAGES*256;
		ei_status.rmem_end = dev->mem_end;
	}

	
	switch (type) {
	case MAC8390_FARALLON:
	case MAC8390_APPLE:
		switch (mac8390_testio(dev->mem_start)) {
		case ACCESS_UNKNOWN:
			pr_err("Don't know how to access card memory!\n");
			return -ENODEV;
			break;

		case ACCESS_16:
			
			ei_status.reset_8390 = mac8390_no_reset;
			ei_status.block_input = slow_sane_block_input;
			ei_status.block_output = slow_sane_block_output;
			ei_status.get_8390_hdr = slow_sane_get_8390_hdr;
			ei_status.reg_offset = back4_offsets;
			break;

		case ACCESS_32:
			
			ei_status.reset_8390 = mac8390_no_reset;
			ei_status.block_input = sane_block_input;
			ei_status.block_output = sane_block_output;
			ei_status.get_8390_hdr = sane_get_8390_hdr;
			ei_status.reg_offset = back4_offsets;
			access_bitmode = 1;
			break;
		}
		break;

	case MAC8390_ASANTE:
		ei_status.reset_8390 = mac8390_no_reset;
		ei_status.block_input = slow_sane_block_input;
		ei_status.block_output = slow_sane_block_output;
		ei_status.get_8390_hdr = slow_sane_get_8390_hdr;
		ei_status.reg_offset = back4_offsets;
		break;

	case MAC8390_CABLETRON:
		
		ei_status.reset_8390 = mac8390_no_reset;
		ei_status.block_input = slow_sane_block_input;
		ei_status.block_output = slow_sane_block_output;
		ei_status.get_8390_hdr = slow_sane_get_8390_hdr;
		ei_status.reg_offset = fwrd2_offsets;
		break;

	case MAC8390_DAYNA:
	case MAC8390_KINETICS:
		
		
		ei_status.reset_8390 = mac8390_no_reset;
		ei_status.block_input = dayna_block_input;
		ei_status.block_output = dayna_block_output;
		ei_status.get_8390_hdr = dayna_get_8390_hdr;
		ei_status.reg_offset = fwrd4_offsets;
		break;

	case MAC8390_INTERLAN:
		
		ei_status.reset_8390 = interlan_reset;
		ei_status.block_input = slow_sane_block_input;
		ei_status.block_output = slow_sane_block_output;
		ei_status.get_8390_hdr = slow_sane_get_8390_hdr;
		ei_status.reg_offset = fwrd4_offsets;
		break;

	default:
		pr_err("Card type %s is unsupported, sorry\n",
		       ndev->board->name);
		return -ENODEV;
	}

	__NS8390_init(dev, 0);

	
	pr_info("%s: %s in slot %X (type %s)\n",
		dev->name, ndev->board->name, ndev->board->slot,
		cardname[type]);
	pr_info("MAC %pM IRQ %d, %d KB shared memory at %#lx, %d-bit access.\n",
		dev->dev_addr, dev->irq,
		(unsigned int)(dev->mem_end - dev->mem_start) >> 10,
		dev->mem_start, access_bitmode ? 32 : 16);
	return 0;
}

static int mac8390_open(struct net_device *dev)
{
	int err;

	__ei_open(dev);
	err = request_irq(dev->irq, __ei_interrupt, 0, "8390 Ethernet", dev);
	if (err)
		pr_err("%s: unable to get IRQ %d\n", dev->name, dev->irq);
	return err;
}

static int mac8390_close(struct net_device *dev)
{
	free_irq(dev->irq, dev);
	__ei_close(dev);
	return 0;
}

static void mac8390_no_reset(struct net_device *dev)
{
	ei_status.txing = 0;
	if (ei_debug > 1)
		pr_info("reset not supported\n");
}

static void interlan_reset(struct net_device *dev)
{
	unsigned char *target = nubus_slot_addr(IRQ2SLOT(dev->irq));
	if (ei_debug > 1)
		pr_info("Need to reset the NS8390 t=%lu...", jiffies);
	ei_status.txing = 0;
	target[0xC0000] = 0;
	if (ei_debug > 1)
		pr_cont("reset complete\n");
}

static void dayna_memcpy_fromcard(struct net_device *dev, void *to, int from,
				  int count)
{
	volatile unsigned char *ptr;
	unsigned char *target = to;
	from <<= 1;	
	ptr = (unsigned char *)(dev->mem_start+from);
	
	if (from & 2) {
		*target++ = ptr[-1];
		ptr += 2;
		count--;
	}
	while (count >= 2) {
		*(unsigned short *)target = *(unsigned short volatile *)ptr;
		ptr += 4;			
		target += 2;
		count -= 2;
	}
	
	if (count)
		*target = *ptr;
}

static void dayna_memcpy_tocard(struct net_device *dev, int to,
				const void *from, int count)
{
	volatile unsigned short *ptr;
	const unsigned char *src = from;
	to <<= 1;	
	ptr = (unsigned short *)(dev->mem_start+to);
	
	if (to & 2) {		
		ptr[-1] = (ptr[-1]&0xFF00)|*src++;
		ptr++;
		count--;
	}
	while (count >= 2) {
		*ptr++ = *(unsigned short *)src;	
		ptr++;			
		src += 2;
		count -= 2;
	}
	
	if (count) {
		
		*ptr = (*ptr & 0x00FF) | (*src << 8);
	}
}

static void sane_get_8390_hdr(struct net_device *dev,
			      struct e8390_pkt_hdr *hdr, int ring_page)
{
	unsigned long hdr_start = (ring_page - WD_START_PG)<<8;
	memcpy_fromio(hdr, dev->mem_start + hdr_start, 4);
	
	hdr->count = swab16(hdr->count);
}

static void sane_block_input(struct net_device *dev, int count,
			     struct sk_buff *skb, int ring_offset)
{
	unsigned long xfer_base = ring_offset - (WD_START_PG<<8);
	unsigned long xfer_start = xfer_base + dev->mem_start;

	if (xfer_start + count > ei_status.rmem_end) {
		
		int semi_count = ei_status.rmem_end - xfer_start;
		memcpy_fromio(skb->data, dev->mem_start + xfer_base,
			      semi_count);
		count -= semi_count;
		memcpy_fromio(skb->data + semi_count, ei_status.rmem_start,
			      count);
	} else {
		memcpy_fromio(skb->data, dev->mem_start + xfer_base, count);
	}
}

static void sane_block_output(struct net_device *dev, int count,
			      const unsigned char *buf, int start_page)
{
	long shmem = (start_page - WD_START_PG)<<8;

	memcpy_toio(dev->mem_start + shmem, buf, count);
}

static void dayna_get_8390_hdr(struct net_device *dev,
			       struct e8390_pkt_hdr *hdr, int ring_page)
{
	unsigned long hdr_start = (ring_page - WD_START_PG)<<8;

	dayna_memcpy_fromcard(dev, hdr, hdr_start, 4);
	
	hdr->count = (hdr->count & 0xFF) << 8 | (hdr->count >> 8);
}

static void dayna_block_input(struct net_device *dev, int count,
			      struct sk_buff *skb, int ring_offset)
{
	unsigned long xfer_base = ring_offset - (WD_START_PG<<8);
	unsigned long xfer_start = xfer_base+dev->mem_start;


	if (xfer_start + count > ei_status.rmem_end) {
		
		int semi_count = ei_status.rmem_end - xfer_start;
		dayna_memcpy_fromcard(dev, skb->data, xfer_base, semi_count);
		count -= semi_count;
		dayna_memcpy_fromcard(dev, skb->data + semi_count,
				      ei_status.rmem_start - dev->mem_start,
				      count);
	} else {
		dayna_memcpy_fromcard(dev, skb->data, xfer_base, count);
	}
}

static void dayna_block_output(struct net_device *dev, int count,
			       const unsigned char *buf,
			       int start_page)
{
	long shmem = (start_page - WD_START_PG)<<8;

	dayna_memcpy_tocard(dev, shmem, buf, count);
}

static void slow_sane_get_8390_hdr(struct net_device *dev,
				   struct e8390_pkt_hdr *hdr,
				   int ring_page)
{
	unsigned long hdr_start = (ring_page - WD_START_PG)<<8;
	word_memcpy_fromcard(hdr, dev->mem_start + hdr_start, 4);
	
	hdr->count = (hdr->count&0xFF)<<8|(hdr->count>>8);
}

static void slow_sane_block_input(struct net_device *dev, int count,
				  struct sk_buff *skb, int ring_offset)
{
	unsigned long xfer_base = ring_offset - (WD_START_PG<<8);
	unsigned long xfer_start = xfer_base+dev->mem_start;

	if (xfer_start + count > ei_status.rmem_end) {
		
		int semi_count = ei_status.rmem_end - xfer_start;
		word_memcpy_fromcard(skb->data, dev->mem_start + xfer_base,
				     semi_count);
		count -= semi_count;
		word_memcpy_fromcard(skb->data + semi_count,
				     ei_status.rmem_start, count);
	} else {
		word_memcpy_fromcard(skb->data, dev->mem_start + xfer_base,
				     count);
	}
}

static void slow_sane_block_output(struct net_device *dev, int count,
				   const unsigned char *buf, int start_page)
{
	long shmem = (start_page - WD_START_PG)<<8;

	word_memcpy_tocard(dev->mem_start + shmem, buf, count);
}

static void word_memcpy_tocard(unsigned long tp, const void *fp, int count)
{
	volatile unsigned short *to = (void *)tp;
	const unsigned short *from = fp;

	count++;
	count /= 2;

	while (count--)
		*to++ = *from++;
}

static void word_memcpy_fromcard(void *tp, unsigned long fp, int count)
{
	unsigned short *to = tp;
	const volatile unsigned short *from = (const void *)fp;

	count++;
	count /= 2;

	while (count--)
		*to++ = *from++;
}


