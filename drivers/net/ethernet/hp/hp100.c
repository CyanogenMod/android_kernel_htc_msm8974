/*
** hp100.c
** HP CASCADE Architecture Driver for 100VG-AnyLan Network Adapters
**
** $Id: hp100.c,v 1.58 2001/09/24 18:03:01 perex Exp perex $
**
** Based on the HP100 driver written by Jaroslav Kysela <perex@jcu.cz>
** Extended for new busmaster capable chipsets by
** Siegfried "Frieder" Loeffler (dg1sek) <floeff@mathematik.uni-stuttgart.de>
**
** Maintained by: Jaroslav Kysela <perex@perex.cz>
**
** This driver has only been tested with
** -- HP J2585B 10/100 Mbit/s PCI Busmaster
** -- HP J2585A 10/100 Mbit/s PCI
** -- HP J2970A 10 Mbit/s PCI Combo 10base-T/BNC
** -- HP J2973A 10 Mbit/s PCI 10base-T
** -- HP J2573  10/100 ISA
** -- Compex ReadyLink ENET100-VG4  10/100 Mbit/s PCI / EISA
** -- Compex FreedomLine 100/VG  10/100 Mbit/s ISA / EISA / PCI
**
** but it should also work with the other CASCADE based adapters.
**
** TODO:
**       -  J2573 seems to hang sometimes when in shared memory mode.
**       -  Mode for Priority TX
**       -  Check PCI registers, performance might be improved?
**       -  To reduce interrupt load in busmaster, one could switch off
**          the interrupts that are used to refill the queues whenever the
**          queues are filled up to more than a certain threshold.
**       -  some updates for EISA version of card
**
**
**   This code is free software; you can redistribute it and/or modify
**   it under the terms of the GNU General Public License as published by
**   the Free Software Foundation; either version 2 of the License, or
**   (at your option) any later version.
**
**   This code is distributed in the hope that it will be useful,
**   but WITHOUT ANY WARRANTY; without even the implied warranty of
**   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**   GNU General Public License for more details.
**
**   You should have received a copy of the GNU General Public License
**   along with this program; if not, write to the Free Software
**   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** 1.57c -> 1.58
**   - used indent to change coding-style
**   - added KTI DP-200 EISA ID
**   - ioremap is also used for low (<1MB) memory (multi-architecture support)
**
** 1.57b -> 1.57c - Arnaldo Carvalho de Melo <acme@conectiva.com.br>
**   - release resources on failure in init_module
**
** 1.57 -> 1.57b - Jean II
**   - fix spinlocks, SMP is now working !
**
** 1.56 -> 1.57
**   - updates for new PCI interface for 2.1 kernels
**
** 1.55 -> 1.56
**   - removed printk in misc. interrupt and update statistics to allow
**     monitoring of card status
**   - timing changes in xmit routines, relogin to 100VG hub added when
**     driver does reset
**   - included fix for Compex FreedomLine PCI adapter
**
** 1.54 -> 1.55
**   - fixed bad initialization in init_module
**   - added Compex FreedomLine adapter
**   - some fixes in card initialization
**
** 1.53 -> 1.54
**   - added hardware multicast filter support (doesn't work)
**   - little changes in hp100_sense_lan routine
**     - added support for Coax and AUI (J2970)
**   - fix for multiple cards and hp100_mode parameter (insmod)
**   - fix for shared IRQ
**
** 1.52 -> 1.53
**   - fixed bug in multicast support
**
*/

#define HP100_DEFAULT_PRIORITY_TX 0

#undef HP100_DEBUG
#undef HP100_DEBUG_B		
#undef HP100_DEBUG_BM		

#undef HP100_DEBUG_TRAINING	
#undef HP100_DEBUG_TX
#undef HP100_DEBUG_IRQ
#undef HP100_DEBUG_RX

#undef HP100_MULTICAST_FILTER	

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/eisa.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/jiffies.h>

#include <asm/io.h>

#include "hp100.h"


#define HP100_BUS_ISA     0
#define HP100_BUS_EISA    1
#define HP100_BUS_PCI     2

#define HP100_REGION_SIZE	0x20	
#define HP100_SIG_LEN		8	

#define HP100_MAX_PACKET_SIZE	(1536+4)
#define HP100_MIN_PACKET_SIZE	60

#ifndef HP100_DEFAULT_RX_RATIO
#define HP100_DEFAULT_RX_RATIO	75
#endif

#ifndef HP100_DEFAULT_PRIORITY_TX
#define HP100_DEFAULT_PRIORITY_TX 0
#endif


struct hp100_private {
	spinlock_t lock;
	char id[HP100_SIG_LEN];
	u_short chip;
	u_short soft_model;
	u_int memory_size;
	u_int virt_memory_size;
	u_short rx_ratio;	
	u_short priority_tx;	
	u_short mode;		
	u_char bus;
	struct pci_dev *pci_dev;
	short mem_mapped;	
	void __iomem *mem_ptr_virt;	
	unsigned long mem_ptr_phys;	
	short lan_type;		
	int hub_status;		
	u_char mac1_mode;
	u_char mac2_mode;
	u_char hash_bytes[8];

	
	hp100_ring_t *rxrhead;	
	hp100_ring_t *rxrtail;	
	hp100_ring_t *txrhead;	
	hp100_ring_t *txrtail;	

	hp100_ring_t rxring[MAX_RX_PDL];
	hp100_ring_t txring[MAX_TX_PDL];

	u_int *page_vaddr_algn;	
	u_long whatever_offset;	
	int rxrcommit;		
	int txrcommit;		
};

#ifdef CONFIG_ISA
static const char *hp100_isa_tbl[] = {
	"HWPF150", 
	"HWP1950", 
};
#endif

#ifdef CONFIG_EISA
static struct eisa_device_id hp100_eisa_tbl[] = {
	{ "HWPF180" }, 
	{ "HWP1920" }, 
	{ "HWP1940" }, 
	{ "HWP1990" }, 
	{ "CPX0301" }, 
	{ "CPX0401" }, 
	{ "" }	       
};
MODULE_DEVICE_TABLE(eisa, hp100_eisa_tbl);
#endif

#ifdef CONFIG_PCI
static DEFINE_PCI_DEVICE_TABLE(hp100_pci_tbl) = {
	{PCI_VENDOR_ID_HP, PCI_DEVICE_ID_HP_J2585A, PCI_ANY_ID, PCI_ANY_ID,},
	{PCI_VENDOR_ID_HP, PCI_DEVICE_ID_HP_J2585B, PCI_ANY_ID, PCI_ANY_ID,},
	{PCI_VENDOR_ID_HP, PCI_DEVICE_ID_HP_J2970A, PCI_ANY_ID, PCI_ANY_ID,},
	{PCI_VENDOR_ID_HP, PCI_DEVICE_ID_HP_J2973A, PCI_ANY_ID, PCI_ANY_ID,},
	{PCI_VENDOR_ID_COMPEX, PCI_DEVICE_ID_COMPEX_ENET100VG4, PCI_ANY_ID, PCI_ANY_ID,},
	{PCI_VENDOR_ID_COMPEX2, PCI_DEVICE_ID_COMPEX2_100VG, PCI_ANY_ID, PCI_ANY_ID,},
	{}			
};
MODULE_DEVICE_TABLE(pci, hp100_pci_tbl);
#endif

static int hp100_rx_ratio = HP100_DEFAULT_RX_RATIO;
static int hp100_priority_tx = HP100_DEFAULT_PRIORITY_TX;
static int hp100_mode = 1;

module_param(hp100_rx_ratio, int, 0);
module_param(hp100_priority_tx, int, 0);
module_param(hp100_mode, int, 0);


static int hp100_probe1(struct net_device *dev, int ioaddr, u_char bus,
			struct pci_dev *pci_dev);


static int hp100_open(struct net_device *dev);
static int hp100_close(struct net_device *dev);
static netdev_tx_t hp100_start_xmit(struct sk_buff *skb,
				    struct net_device *dev);
static netdev_tx_t hp100_start_xmit_bm(struct sk_buff *skb,
				       struct net_device *dev);
static void hp100_rx(struct net_device *dev);
static struct net_device_stats *hp100_get_stats(struct net_device *dev);
static void hp100_misc_interrupt(struct net_device *dev);
static void hp100_update_stats(struct net_device *dev);
static void hp100_clear_stats(struct hp100_private *lp, int ioaddr);
static void hp100_set_multicast_list(struct net_device *dev);
static irqreturn_t hp100_interrupt(int irq, void *dev_id);
static void hp100_start_interface(struct net_device *dev);
static void hp100_stop_interface(struct net_device *dev);
static void hp100_load_eeprom(struct net_device *dev, u_short ioaddr);
static int hp100_sense_lan(struct net_device *dev);
static int hp100_login_to_vg_hub(struct net_device *dev,
				 u_short force_relogin);
static int hp100_down_vg_link(struct net_device *dev);
static void hp100_cascade_reset(struct net_device *dev, u_short enable);
static void hp100_BM_shutdown(struct net_device *dev);
static void hp100_mmuinit(struct net_device *dev);
static void hp100_init_pdls(struct net_device *dev);
static int hp100_init_rxpdl(struct net_device *dev,
			    register hp100_ring_t * ringptr,
			    register u_int * pdlptr);
static int hp100_init_txpdl(struct net_device *dev,
			    register hp100_ring_t * ringptr,
			    register u_int * pdlptr);
static void hp100_rxfill(struct net_device *dev);
static void hp100_hwinit(struct net_device *dev);
static void hp100_clean_txring(struct net_device *dev);
#ifdef HP100_DEBUG
static void hp100_RegisterDump(struct net_device *dev);
#endif

static inline dma_addr_t virt_to_whatever(struct net_device *dev, u32 * ptr)
{
	struct hp100_private *lp = netdev_priv(dev);
	return ((u_long) ptr) + lp->whatever_offset;
}

static inline u_int pdl_map_data(struct hp100_private *lp, void *data)
{
	return pci_map_single(lp->pci_dev, data,
			      MAX_ETHER_SIZE, PCI_DMA_FROMDEVICE);
}

static void wait(void)
{
	mdelay(1);
}


static __devinit const char *hp100_read_id(int ioaddr)
{
	int i;
	static char str[HP100_SIG_LEN];
	unsigned char sig[4], sum;
        unsigned short rev;

	hp100_page(ID_MAC_ADDR);
	sum = 0;
	for (i = 0; i < 4; i++) {
		sig[i] = hp100_inb(BOARD_ID + i);
		sum += sig[i];
	}

	sum += hp100_inb(BOARD_ID + i);
	if (sum != 0xff)
		return NULL;	

        str[0] = ((sig[0] >> 2) & 0x1f) + ('A' - 1);
        str[1] = (((sig[0] & 3) << 3) | (sig[1] >> 5)) + ('A' - 1);
        str[2] = (sig[1] & 0x1f) + ('A' - 1);
        rev = (sig[2] << 8) | sig[3];
        sprintf(str + 3, "%04X", rev);

	return str;
}

#ifdef CONFIG_ISA
static __init int hp100_isa_probe1(struct net_device *dev, int ioaddr)
{
	const char *sig;
	int i;

	if (!request_region(ioaddr, HP100_REGION_SIZE, "hp100"))
		goto err;

	if (hp100_inw(HW_ID) != HP100_HW_ID_CASCADE) {
		release_region(ioaddr, HP100_REGION_SIZE);
		goto err;
	}

	sig = hp100_read_id(ioaddr);
	release_region(ioaddr, HP100_REGION_SIZE);

	if (sig == NULL)
		goto err;

	for (i = 0; i < ARRAY_SIZE(hp100_isa_tbl); i++) {
		if (!strcmp(hp100_isa_tbl[i], sig))
			break;

	}

	if (i < ARRAY_SIZE(hp100_isa_tbl))
		return hp100_probe1(dev, ioaddr, HP100_BUS_ISA, NULL);
 err:
	return -ENODEV;

}

static int  __init hp100_isa_probe(struct net_device *dev, int addr)
{
	int err = -ENODEV;

	
	if (addr > 0xff && addr < 0x400)
		err = hp100_isa_probe1(dev, addr);

	else if (addr != 0)
		err = -ENXIO;

	else {
		
		for (addr = 0x100; addr < 0x400; addr += 0x20) {
			err = hp100_isa_probe1(dev, addr);
			if (!err)
				break;
		}
	}
	return err;
}
#endif 

#if !defined(MODULE) && defined(CONFIG_ISA)
struct net_device * __init hp100_probe(int unit)
{
	struct net_device *dev = alloc_etherdev(sizeof(struct hp100_private));
	int err;

	if (!dev)
		return ERR_PTR(-ENODEV);

#ifdef HP100_DEBUG_B
	hp100_outw(0x4200, TRACE);
	printk("hp100: %s: probe\n", dev->name);
#endif

	if (unit >= 0) {
		sprintf(dev->name, "eth%d", unit);
		netdev_boot_setup_check(dev);
	}

	err = hp100_isa_probe(dev, dev->base_addr);
	if (err)
		goto out;

	return dev;
 out:
	free_netdev(dev);
	return ERR_PTR(err);
}
#endif 

static const struct net_device_ops hp100_bm_netdev_ops = {
	.ndo_open		= hp100_open,
	.ndo_stop		= hp100_close,
	.ndo_start_xmit		= hp100_start_xmit_bm,
	.ndo_get_stats 		= hp100_get_stats,
	.ndo_set_rx_mode	= hp100_set_multicast_list,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};

static const struct net_device_ops hp100_netdev_ops = {
	.ndo_open		= hp100_open,
	.ndo_stop		= hp100_close,
	.ndo_start_xmit		= hp100_start_xmit,
	.ndo_get_stats 		= hp100_get_stats,
	.ndo_set_rx_mode	= hp100_set_multicast_list,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};

static int __devinit hp100_probe1(struct net_device *dev, int ioaddr,
				  u_char bus, struct pci_dev *pci_dev)
{
	int i;
	int err = -ENODEV;
	const char *eid;
	u_int chip;
	u_char uc;
	u_int memory_size = 0, virt_memory_size = 0;
	u_short local_mode, lsw;
	short mem_mapped;
	unsigned long mem_ptr_phys;
	void __iomem *mem_ptr_virt;
	struct hp100_private *lp;

#ifdef HP100_DEBUG_B
	hp100_outw(0x4201, TRACE);
	printk("hp100: %s: probe1\n", dev->name);
#endif

	
	if (!request_region(ioaddr, HP100_REGION_SIZE, "hp100"))
		goto out1;

	if (hp100_inw(HW_ID) != HP100_HW_ID_CASCADE)
		goto out2;

	chip = hp100_inw(PAGING) & HP100_CHIPID_MASK;
#ifdef HP100_DEBUG
	if (chip == HP100_CHIPID_SHASTA)
		printk("hp100: %s: Shasta Chip detected. (This is a pre 802.12 chip)\n", dev->name);
	else if (chip == HP100_CHIPID_RAINIER)
		printk("hp100: %s: Rainier Chip detected. (This is a pre 802.12 chip)\n", dev->name);
	else if (chip == HP100_CHIPID_LASSEN)
		printk("hp100: %s: Lassen Chip detected.\n", dev->name);
	else
		printk("hp100: %s: Warning: Unknown CASCADE chip (id=0x%.4x).\n", dev->name, chip);
#endif

	dev->base_addr = ioaddr;

	eid = hp100_read_id(ioaddr);
	if (eid == NULL) {	
		printk(KERN_WARNING "hp100_probe: bad ID checksum at base port 0x%x\n", ioaddr);
		goto out2;
	}

	hp100_page(ID_MAC_ADDR);
	for (i = uc = 0; i < 7; i++)
		uc += hp100_inb(LAN_ADDR + i);
	if (uc != 0xff) {
		printk(KERN_WARNING "hp100_probe: bad lan address checksum at port 0x%x)\n", ioaddr);
		err = -EIO;
		goto out2;
	}

	

	hp100_load_eeprom(dev, ioaddr);
	wait();



#if 0
	local_mode = 0x2270;
	hp100_outw(0xfefe, OPTION_LSW);
	hp100_outw(local_mode | HP100_SET_LB | HP100_SET_HB, OPTION_LSW);
#endif

	
	local_mode = hp100_mode;
	if (local_mode < 1 || local_mode > 4)
		local_mode = 1;	
#ifdef HP100_DEBUG
	printk("hp100: %s: original LSW = 0x%x\n", dev->name,
	       hp100_inw(OPTION_LSW));
#endif

	if (local_mode == 3) {
		hp100_outw(HP100_MEM_EN | HP100_RESET_LB, OPTION_LSW);
		hp100_outw(HP100_IO_EN | HP100_SET_LB, OPTION_LSW);
		hp100_outw(HP100_BM_WRITE | HP100_BM_READ | HP100_RESET_HB, OPTION_LSW);
		printk("hp100: IO mapped mode forced.\n");
	} else if (local_mode == 2) {
		hp100_outw(HP100_MEM_EN | HP100_SET_LB, OPTION_LSW);
		hp100_outw(HP100_IO_EN | HP100_SET_LB, OPTION_LSW);
		hp100_outw(HP100_BM_WRITE | HP100_BM_READ | HP100_RESET_HB, OPTION_LSW);
		printk("hp100: Shared memory mode requested.\n");
	} else if (local_mode == 4) {
		if (chip == HP100_CHIPID_LASSEN) {
			hp100_outw(HP100_BM_WRITE | HP100_BM_READ | HP100_SET_HB, OPTION_LSW);
			hp100_outw(HP100_IO_EN | HP100_MEM_EN | HP100_RESET_LB, OPTION_LSW);
			printk("hp100: Busmaster mode requested.\n");
		}
		local_mode = 1;
	}

	if (local_mode == 1) {	
		lsw = hp100_inw(OPTION_LSW);

		if ((lsw & HP100_IO_EN) && (~lsw & HP100_MEM_EN) &&
		    (~lsw & (HP100_BM_WRITE | HP100_BM_READ))) {
#ifdef HP100_DEBUG
			printk("hp100: %s: IO_EN bit is set on card.\n", dev->name);
#endif
			local_mode = 3;
		} else if (chip == HP100_CHIPID_LASSEN &&
			   (lsw & (HP100_BM_WRITE | HP100_BM_READ)) == (HP100_BM_WRITE | HP100_BM_READ)) {
			if((bus == HP100_BUS_PCI) &&
			   (pci_set_dma_mask(pci_dev, DMA_BIT_MASK(32)))) {
				
				goto busmasterfail;
			}
			printk("hp100: Busmaster mode enabled.\n");
			hp100_outw(HP100_MEM_EN | HP100_IO_EN | HP100_RESET_LB, OPTION_LSW);
		} else {
		busmasterfail:
#ifdef HP100_DEBUG
			printk("hp100: %s: Card not configured for BM or BM not supported with this card.\n", dev->name);
			printk("hp100: %s: Trying shared memory mode.\n", dev->name);
#endif
			
			local_mode = 2;
			hp100_outw(HP100_MEM_EN | HP100_SET_LB, OPTION_LSW);
			
		}
	}
#ifdef HP100_DEBUG
	printk("hp100: %s: new LSW = 0x%x\n", dev->name, hp100_inw(OPTION_LSW));
#endif

	
	hp100_page(HW_MAP);
	mem_mapped = ((hp100_inw(OPTION_LSW) & (HP100_MEM_EN)) != 0);
	mem_ptr_phys = 0UL;
	mem_ptr_virt = NULL;
	memory_size = (8192 << ((hp100_inb(SRAM) >> 5) & 0x07));
	virt_memory_size = 0;

	
	if (mem_mapped || (local_mode == 1)) {
		mem_ptr_phys = (hp100_inw(MEM_MAP_LSW) | (hp100_inw(MEM_MAP_MSW) << 16));
		mem_ptr_phys &= ~0x1fff;	

		if (bus == HP100_BUS_ISA && (mem_ptr_phys & ~0xfffff) != 0) {
			printk("hp100: Can only use programmed i/o mode.\n");
			mem_ptr_phys = 0;
			mem_mapped = 0;
			local_mode = 3;	
		}

		
		
		if (local_mode != 1) {	
			
			for (virt_memory_size = memory_size; virt_memory_size > 16383; virt_memory_size >>= 1) {
				if ((mem_ptr_virt = ioremap((u_long) mem_ptr_phys, virt_memory_size)) == NULL) {
#ifdef HP100_DEBUG
					printk("hp100: %s: ioremap for 0x%x bytes high PCI memory at 0x%lx failed\n", dev->name, virt_memory_size, mem_ptr_phys);
#endif
				} else {
#ifdef HP100_DEBUG
					printk("hp100: %s: remapped 0x%x bytes high PCI memory at 0x%lx to %p.\n", dev->name, virt_memory_size, mem_ptr_phys, mem_ptr_virt);
#endif
					break;
				}
			}

			if (mem_ptr_virt == NULL) {	
				printk("hp100: Failed to ioremap the PCI card memory. Will have to use i/o mapped mode.\n");
				local_mode = 3;
				virt_memory_size = 0;
			}
		}
	}

	if (local_mode == 3) {	
		mem_mapped = 0;
		mem_ptr_phys = 0;
		mem_ptr_virt = NULL;
		printk("hp100: Using (slow) programmed i/o mode.\n");
	}

	
	lp = netdev_priv(dev);

	spin_lock_init(&lp->lock);
	strlcpy(lp->id, eid, HP100_SIG_LEN);
	lp->chip = chip;
	lp->mode = local_mode;
	lp->bus = bus;
	lp->pci_dev = pci_dev;
	lp->priority_tx = hp100_priority_tx;
	lp->rx_ratio = hp100_rx_ratio;
	lp->mem_ptr_phys = mem_ptr_phys;
	lp->mem_ptr_virt = mem_ptr_virt;
	hp100_page(ID_MAC_ADDR);
	lp->soft_model = hp100_inb(SOFT_MODEL);
	lp->mac1_mode = HP100_MAC1MODE3;
	lp->mac2_mode = HP100_MAC2MODE3;
	memset(&lp->hash_bytes, 0x00, 8);

	dev->base_addr = ioaddr;

	lp->memory_size = memory_size;
	lp->virt_memory_size = virt_memory_size;
	lp->rx_ratio = hp100_rx_ratio;	

	if (lp->mode == 1)	
		dev->netdev_ops = &hp100_bm_netdev_ops;
	else
		dev->netdev_ops = &hp100_netdev_ops;

	
	if (bus == HP100_BUS_PCI) {
		dev->irq = pci_dev->irq;
	} else {
		hp100_page(HW_MAP);
		dev->irq = hp100_inb(IRQ_CHANNEL) & HP100_IRQMASK;
		if (dev->irq == 2)
			dev->irq = 9;
	}

	if (lp->mode == 1)	
		dev->dma = 4;

	
	hp100_page(ID_MAC_ADDR);
	for (i = uc = 0; i < 6; i++)
		dev->dev_addr[i] = hp100_inb(LAN_ADDR + i);

	
	hp100_clear_stats(lp, ioaddr);


	if (lp->mode == 1) {	
		dma_addr_t page_baddr;
		
		lp->page_vaddr_algn = pci_alloc_consistent(lp->pci_dev, MAX_RINGSIZE, &page_baddr);
		if (!lp->page_vaddr_algn) {
			err = -ENOMEM;
			goto out_mem_ptr;
		}
		lp->whatever_offset = ((u_long) page_baddr) - ((u_long) lp->page_vaddr_algn);

#ifdef HP100_DEBUG_BM
		printk("hp100: %s: Reserved DMA memory from 0x%x to 0x%x\n", dev->name, (u_int) lp->page_vaddr_algn, (u_int) lp->page_vaddr_algn + MAX_RINGSIZE);
#endif
		lp->rxrcommit = lp->txrcommit = 0;
		lp->rxrhead = lp->rxrtail = &(lp->rxring[0]);
		lp->txrhead = lp->txrtail = &(lp->txring[0]);
	}

	
	hp100_hwinit(dev);

	
	lp->lan_type = hp100_sense_lan(dev);

	
	printk("hp100: at 0x%x, IRQ %d, ", ioaddr, dev->irq);
	switch (bus) {
	case HP100_BUS_EISA:
		printk("EISA");
		break;
	case HP100_BUS_PCI:
		printk("PCI");
		break;
	default:
		printk("ISA");
		break;
	}
	printk(" bus, %dk SRAM (rx/tx %d%%).\n", lp->memory_size >> 10, lp->rx_ratio);

	if (lp->mode == 2) {	
		printk("hp100: Memory area at 0x%lx-0x%lx", mem_ptr_phys,
				(mem_ptr_phys + (mem_ptr_phys > 0x100000 ? (u_long) lp->memory_size : 16 * 1024)) - 1);
		if (mem_ptr_virt)
			printk(" (virtual base %p)", mem_ptr_virt);
		printk(".\n");

		
		dev->mem_start = mem_ptr_phys;
		dev->mem_end = mem_ptr_phys + lp->memory_size;
	}

	printk("hp100: ");
	if (lp->lan_type != HP100_LAN_ERR)
		printk("Adapter is attached to ");
	switch (lp->lan_type) {
	case HP100_LAN_100:
		printk("100Mb/s Voice Grade AnyLAN network.\n");
		break;
	case HP100_LAN_10:
		printk("10Mb/s network (10baseT).\n");
		break;
	case HP100_LAN_COAX:
		printk("10Mb/s network (coax).\n");
		break;
	default:
		printk("Warning! Link down.\n");
	}

	err = register_netdev(dev);
	if (err)
		goto out3;

	return 0;
out3:
	if (local_mode == 1)
		pci_free_consistent(lp->pci_dev, MAX_RINGSIZE + 0x0f,
				    lp->page_vaddr_algn,
				    virt_to_whatever(dev, lp->page_vaddr_algn));
out_mem_ptr:
	if (mem_ptr_virt)
		iounmap(mem_ptr_virt);
out2:
	release_region(ioaddr, HP100_REGION_SIZE);
out1:
	return err;
}

static void hp100_hwinit(struct net_device *dev)
{
	int ioaddr = dev->base_addr;
	struct hp100_private *lp = netdev_priv(dev);

#ifdef HP100_DEBUG_B
	hp100_outw(0x4202, TRACE);
	printk("hp100: %s: hwinit\n", dev->name);
#endif

	

	
	hp100_page(PERFORMANCE);
	hp100_outw(0xfefe, IRQ_MASK);	
	hp100_outw(0xffff, IRQ_STATUS);	

	hp100_outw(HP100_INT_EN | HP100_RESET_LB, OPTION_LSW);
	hp100_outw(HP100_TRI_INT | HP100_SET_HB, OPTION_LSW);

	if (lp->mode == 1) {
		hp100_BM_shutdown(dev);	
		wait();
	} else {
		hp100_outw(HP100_INT_EN | HP100_RESET_LB, OPTION_LSW);
		hp100_cascade_reset(dev, 1);
		hp100_page(MAC_CTRL);
		hp100_andb(~(HP100_RX_EN | HP100_TX_EN), MAC_CFG_1);
	}

	
	hp100_load_eeprom(dev, 0);

	wait();

	
	hp100_cascade_reset(dev, 1);

	
	hp100_outw(HP100_DEBUG_EN |
		   HP100_RX_HDR |
		   HP100_EE_EN |
		   HP100_BM_WRITE |
		   HP100_BM_READ | HP100_RESET_HB |
		   HP100_FAKE_INT |
		   HP100_INT_EN |
		   HP100_MEM_EN |
		   HP100_IO_EN | HP100_RESET_LB, OPTION_LSW);

	hp100_outw(HP100_TRI_INT |
		   HP100_MMAP_DIS | HP100_SET_HB, OPTION_LSW);

	hp100_outb(HP100_PRIORITY_TX |
		   HP100_ADV_NXT_PKT |
		   HP100_TX_CMD | HP100_RESET_LB, OPTION_MSW);

	
	

	
	
	
	

	

	

	hp100_mmuinit(dev);

	
	wait();			

	
	hp100_cascade_reset(dev, 0);

	

	
	if ((lp->lan_type == HP100_LAN_100) || (lp->lan_type == HP100_LAN_ERR))
		hp100_login_to_vg_hub(dev, 0);	

}


static void hp100_mmuinit(struct net_device *dev)
{
	int ioaddr = dev->base_addr;
	struct hp100_private *lp = netdev_priv(dev);
	int i;

#ifdef HP100_DEBUG_B
	hp100_outw(0x4203, TRACE);
	printk("hp100: %s: mmuinit\n", dev->name);
#endif

#ifdef HP100_DEBUG
	if (0 != (hp100_inw(OPTION_LSW) & HP100_HW_RST)) {
		printk("hp100: %s: Not in reset when entering mmuinit. Fix me.\n", dev->name);
		return;
	}
#endif

	
	hp100_page(PERFORMANCE);
	hp100_outw(0xfefe, IRQ_MASK);	
	hp100_outw(0xffff, IRQ_STATUS);	


	hp100_outw(HP100_DEBUG_EN |
		   HP100_RX_HDR |
		   HP100_EE_EN | HP100_RESET_HB |
		   HP100_IO_EN |
		   HP100_FAKE_INT |
		   HP100_INT_EN | HP100_RESET_LB, OPTION_LSW);

	hp100_outw(HP100_TRI_INT | HP100_SET_HB, OPTION_LSW);

	if (lp->mode == 1) {	
		hp100_outw(HP100_BM_WRITE |
			   HP100_BM_READ |
			   HP100_MMAP_DIS | HP100_SET_HB, OPTION_LSW);
	} else if (lp->mode == 2) {	
		hp100_outw(HP100_BM_WRITE |
			   HP100_BM_READ | HP100_RESET_HB, OPTION_LSW);
		hp100_outw(HP100_MMAP_DIS | HP100_RESET_HB, OPTION_LSW);
		hp100_outw(HP100_MEM_EN | HP100_SET_LB, OPTION_LSW);
		hp100_outw(HP100_IO_EN | HP100_SET_LB, OPTION_LSW);
	} else if (lp->mode == 3) {	
		hp100_outw(HP100_MMAP_DIS | HP100_SET_HB |
			   HP100_IO_EN | HP100_SET_LB, OPTION_LSW);
	}

	hp100_page(HW_MAP);
	hp100_outb(0, EARLYRXCFG);
	hp100_outw(0, EARLYTXCFG);

	if (lp->mode == 1) {	
		
		hp100_page(HW_MAP);
		hp100_andb(~HP100_PDL_USE3, MODECTRL1);	
		hp100_andb(~HP100_TX_DUALQ, MODECTRL1);	

		
		hp100_orb(HP100_EN_BUS_FAIL, MODECTRL2);

		hp100_outw(HP100_BM_READ | HP100_BM_WRITE | HP100_SET_HB, OPTION_LSW);
		hp100_page(HW_MAP);
		
		hp100_orb(HP100_BM_BURST_RD | HP100_BM_BURST_WR, BM);
		if ((lp->chip == HP100_CHIPID_RAINIER) || (lp->chip == HP100_CHIPID_SHASTA))
			hp100_orb(HP100_BM_PAGE_CK, BM);
		hp100_orb(HP100_BM_MASTER, BM);
	} else {		

		hp100_page(HW_MAP);
		hp100_andb(~HP100_BM_MASTER, BM);
	}

	hp100_page(MMU_CFG);
	if (lp->mode == 1) {	
		int xmit_stop, recv_stop;

		if ((lp->chip == HP100_CHIPID_RAINIER) ||
		    (lp->chip == HP100_CHIPID_SHASTA)) {
			int pdl_stop;

			pdl_stop = lp->memory_size;
			xmit_stop = (pdl_stop - 508 * (MAX_RX_PDL) - 16) & ~(0x03ff);
			recv_stop = (xmit_stop * (lp->rx_ratio) / 100) & ~(0x03ff);
			hp100_outw((pdl_stop >> 4) - 1, PDL_MEM_STOP);
#ifdef HP100_DEBUG_BM
			printk("hp100: %s: PDL_STOP = 0x%x\n", dev->name, pdl_stop);
#endif
		} else {
			
			xmit_stop = (lp->memory_size) - 1;
			recv_stop = ((lp->memory_size * lp->rx_ratio) / 100) & ~(0x03ff);
		}

		hp100_outw(xmit_stop >> 4, TX_MEM_STOP);
		hp100_outw(recv_stop >> 4, RX_MEM_STOP);
#ifdef HP100_DEBUG_BM
		printk("hp100: %s: TX_STOP  = 0x%x\n", dev->name, xmit_stop >> 4);
		printk("hp100: %s: RX_STOP  = 0x%x\n", dev->name, recv_stop >> 4);
#endif
	} else {
		
		hp100_outw((((lp->memory_size * lp->rx_ratio) / 100) >> 4), RX_MEM_STOP);
		hp100_outw(((lp->memory_size - 1) >> 4), TX_MEM_STOP);
#ifdef HP100_DEBUG
		printk("hp100: %s: TX_MEM_STOP: 0x%x\n", dev->name, hp100_inw(TX_MEM_STOP));
		printk("hp100: %s: RX_MEM_STOP: 0x%x\n", dev->name, hp100_inw(RX_MEM_STOP));
#endif
	}

	
	hp100_page(MAC_ADDRESS);
	for (i = 0; i < 6; i++)
		hp100_outb(dev->dev_addr[i], MAC_ADDR + i);

	
	for (i = 0; i < 8; i++)
		hp100_outb(0x0, HASH_BYTE0 + i);

	
	hp100_page(MAC_CTRL);

	
	
	
	hp100_andb(~(HP100_RX_EN |
		     HP100_TX_EN |
		     HP100_ACC_ERRORED |
		     HP100_ACC_MC |
		     HP100_ACC_BC | HP100_ACC_PHY), MAC_CFG_1);

	hp100_outb(0x00, MAC_CFG_2);

	
	
	hp100_outb(0x00, VG_LAN_CFG_2);	

	if (lp->priority_tx)
		hp100_outb(HP100_PRIORITY_TX | HP100_SET_LB, OPTION_MSW);
	else
		hp100_outb(HP100_PRIORITY_TX | HP100_RESET_LB, OPTION_MSW);

	hp100_outb(HP100_ADV_NXT_PKT |
		   HP100_TX_CMD | HP100_RESET_LB, OPTION_MSW);

	
	if (lp->mode == 1)
		hp100_init_pdls(dev);

	
	hp100_page(PERFORMANCE);
	hp100_outw(0xfefe, IRQ_MASK);	
	hp100_outw(0xffff, IRQ_STATUS);	
}


static int hp100_open(struct net_device *dev)
{
	struct hp100_private *lp = netdev_priv(dev);
#ifdef HP100_DEBUG_B
	int ioaddr = dev->base_addr;
#endif

#ifdef HP100_DEBUG_B
	hp100_outw(0x4204, TRACE);
	printk("hp100: %s: open\n", dev->name);
#endif

	
	if (request_irq(dev->irq, hp100_interrupt,
			lp->bus == HP100_BUS_PCI || lp->bus ==
			HP100_BUS_EISA ? IRQF_SHARED : IRQF_DISABLED,
			"hp100", dev)) {
		printk("hp100: %s: unable to get IRQ %d\n", dev->name, dev->irq);
		return -EAGAIN;
	}

	dev->trans_start = jiffies; 
	netif_start_queue(dev);

	lp->lan_type = hp100_sense_lan(dev);
	lp->mac1_mode = HP100_MAC1MODE3;
	lp->mac2_mode = HP100_MAC2MODE3;
	memset(&lp->hash_bytes, 0x00, 8);

	hp100_stop_interface(dev);

	hp100_hwinit(dev);

	hp100_start_interface(dev);	

	return 0;
}

static int hp100_close(struct net_device *dev)
{
	int ioaddr = dev->base_addr;
	struct hp100_private *lp = netdev_priv(dev);

#ifdef HP100_DEBUG_B
	hp100_outw(0x4205, TRACE);
	printk("hp100: %s: close\n", dev->name);
#endif

	hp100_page(PERFORMANCE);
	hp100_outw(0xfefe, IRQ_MASK);	

	hp100_stop_interface(dev);

	if (lp->lan_type == HP100_LAN_100)
		lp->hub_status = hp100_login_to_vg_hub(dev, 0);

	netif_stop_queue(dev);

	free_irq(dev->irq, dev);

#ifdef HP100_DEBUG
	printk("hp100: %s: close LSW = 0x%x\n", dev->name,
	       hp100_inw(OPTION_LSW));
#endif

	return 0;
}


static void hp100_init_pdls(struct net_device *dev)
{
	struct hp100_private *lp = netdev_priv(dev);
	hp100_ring_t *ringptr;
	u_int *pageptr;		
	int i;

#ifdef HP100_DEBUG_B
	int ioaddr = dev->base_addr;
#endif

#ifdef HP100_DEBUG_B
	hp100_outw(0x4206, TRACE);
	printk("hp100: %s: init pdls\n", dev->name);
#endif

	if (!lp->page_vaddr_algn)
		printk("hp100: %s: Warning: lp->page_vaddr_algn not initialised!\n", dev->name);
	else {
		
		
		
		
		memset(lp->page_vaddr_algn, 0, MAX_RINGSIZE);	
		pageptr = lp->page_vaddr_algn;

		lp->rxrcommit = 0;
		ringptr = lp->rxrhead = lp->rxrtail = &(lp->rxring[0]);

		
		for (i = MAX_RX_PDL - 1; i >= 0; i--) {
			lp->rxring[i].next = ringptr;
			ringptr = &(lp->rxring[i]);
			pageptr += hp100_init_rxpdl(dev, ringptr, pageptr);
		}

		
		lp->txrcommit = 0;
		ringptr = lp->txrhead = lp->txrtail = &(lp->txring[0]);
		for (i = MAX_TX_PDL - 1; i >= 0; i--) {
			lp->txring[i].next = ringptr;
			ringptr = &(lp->txring[i]);
			pageptr += hp100_init_txpdl(dev, ringptr, pageptr);
		}
	}
}


static int hp100_init_rxpdl(struct net_device *dev,
			    register hp100_ring_t * ringptr,
			    register u32 * pdlptr)
{
	

	if (0 != (((unsigned long) pdlptr) & 0xf))
		printk("hp100: %s: Init rxpdl: Unaligned pdlptr 0x%lx.\n",
		       dev->name, (unsigned long) pdlptr);

	ringptr->pdl = pdlptr + 1;
	ringptr->pdl_paddr = virt_to_whatever(dev, pdlptr + 1);
	ringptr->skb = (void *) NULL;

	

	*(pdlptr + 2) = (u_int) virt_to_whatever(dev, pdlptr);	
	*(pdlptr + 3) = 4;	

	return roundup(MAX_RX_FRAG * 2 + 2, 4);
}


static int hp100_init_txpdl(struct net_device *dev,
			    register hp100_ring_t * ringptr,
			    register u32 * pdlptr)
{
	if (0 != (((unsigned long) pdlptr) & 0xf))
		printk("hp100: %s: Init txpdl: Unaligned pdlptr 0x%lx.\n", dev->name, (unsigned long) pdlptr);

	ringptr->pdl = pdlptr;	
	ringptr->pdl_paddr = virt_to_whatever(dev, pdlptr);	
	ringptr->skb = (void *) NULL;

	return roundup(MAX_TX_FRAG * 2 + 2, 4);
}

static int hp100_build_rx_pdl(hp100_ring_t * ringptr,
			      struct net_device *dev)
{
#ifdef HP100_DEBUG_B
	int ioaddr = dev->base_addr;
#endif
#ifdef HP100_DEBUG_BM
	u_int *p;
#endif

#ifdef HP100_DEBUG_B
	hp100_outw(0x4207, TRACE);
	printk("hp100: %s: build rx pdl\n", dev->name);
#endif

	

	ringptr->skb = netdev_alloc_skb(dev, roundup(MAX_ETHER_SIZE + 2, 4));

	if (NULL != ringptr->skb) {
		skb_reserve(ringptr->skb, 2);

		ringptr->skb->data = (u_char *) skb_put(ringptr->skb, MAX_ETHER_SIZE);

		
#ifdef HP100_DEBUG_BM
		printk("hp100: %s: build_rx_pdl: PDH@0x%x, skb->data (len %d) at 0x%x\n",
				     dev->name, (u_int) ringptr->pdl,
				     roundup(MAX_ETHER_SIZE + 2, 4),
				     (unsigned int) ringptr->skb->data);
#endif

		ringptr->pdl[0] = 0x00020000;	
		ringptr->pdl[3] = pdl_map_data(netdev_priv(dev),
					       ringptr->skb->data);
		ringptr->pdl[4] = MAX_ETHER_SIZE;	

#ifdef HP100_DEBUG_BM
		for (p = (ringptr->pdl); p < (ringptr->pdl + 5); p++)
			printk("hp100: %s: Adr 0x%.8x = 0x%.8x\n", dev->name, (u_int) p, (u_int) * p);
#endif
		return 1;
	}
	
#ifdef HP100_DEBUG_BM
	printk("hp100: %s: build_rx_pdl: PDH@0x%x, No space for skb.\n", dev->name, (u_int) ringptr->pdl);
#endif

	ringptr->pdl[0] = 0x00010000;	

	return 0;
}

static void hp100_rxfill(struct net_device *dev)
{
	int ioaddr = dev->base_addr;

	struct hp100_private *lp = netdev_priv(dev);
	hp100_ring_t *ringptr;

#ifdef HP100_DEBUG_B
	hp100_outw(0x4208, TRACE);
	printk("hp100: %s: rxfill\n", dev->name);
#endif

	hp100_page(PERFORMANCE);

	while (lp->rxrcommit < MAX_RX_PDL) {
		ringptr = lp->rxrtail;
		if (0 == hp100_build_rx_pdl(ringptr, dev)) {
			return;	
		}

		
		
#ifdef HP100_DEBUG_BM
		printk("hp100: %s: rxfill: Hand to card: pdl #%d @0x%x phys:0x%x, buffer: 0x%x\n",
				     dev->name, lp->rxrcommit, (u_int) ringptr->pdl,
				     (u_int) ringptr->pdl_paddr, (u_int) ringptr->pdl[3]);
#endif

		hp100_outl((u32) ringptr->pdl_paddr, RX_PDA);

		lp->rxrcommit += 1;
		lp->rxrtail = ringptr->next;
	}
}


static void hp100_BM_shutdown(struct net_device *dev)
{
	int ioaddr = dev->base_addr;
	struct hp100_private *lp = netdev_priv(dev);
	unsigned long time;

#ifdef HP100_DEBUG_B
	hp100_outw(0x4209, TRACE);
	printk("hp100: %s: bm shutdown\n", dev->name);
#endif

	hp100_page(PERFORMANCE);
	hp100_outw(0xfefe, IRQ_MASK);	
	hp100_outw(0xffff, IRQ_STATUS);	

	
	hp100_outw(HP100_INT_EN | HP100_RESET_LB, OPTION_LSW);

	
	hp100_page(MAC_CTRL);
	hp100_andb(~(HP100_RX_EN | HP100_TX_EN), MAC_CFG_1);	

	
	if (0 != (hp100_inw(OPTION_LSW) & HP100_HW_RST)) {
		hp100_page(MAC_CTRL);
		for (time = 0; time < 5000; time++) {
			if ((hp100_inb(MAC_CFG_1) & (HP100_TX_IDLE | HP100_RX_IDLE)) == (HP100_TX_IDLE | HP100_RX_IDLE))
				break;
		}

		
		if (lp->chip == HP100_CHIPID_LASSEN) {	
			
			hp100_page(HW_MAP);
			hp100_andb(~HP100_BM_MASTER, BM);
			
			for (time = 0; time < 32000; time++) {
				if (0 == (hp100_inb(BM) & HP100_BM_MASTER))
					break;
			}
		} else {	
			hp100_page(PERFORMANCE);
			
			for (time = 0; time < 10000; time++) {
				
				
				if ((hp100_inb(RX_PDL) == 0) && (hp100_inb(RX_PKT_CNT) == 0))
					break;
			}

			if (time >= 10000)
				printk("hp100: %s: BM shutdown error.\n", dev->name);

			
			for (time = 0; time < 10000; time++) {
				if ((0 == hp100_inb(TX_PKT_CNT)) &&
				    (0 != (hp100_inb(TX_MEM_FREE) & HP100_AUTO_COMPARE)))
					break;
			}

			
			hp100_page(HW_MAP);
			hp100_andb(~HP100_BM_MASTER, BM);
		}	

		hp100_cascade_reset(dev, 1);
	}
	hp100_page(PERFORMANCE);
	
	
}

static int hp100_check_lan(struct net_device *dev)
{
	struct hp100_private *lp = netdev_priv(dev);

	if (lp->lan_type < 0) {	
		hp100_stop_interface(dev);
		if ((lp->lan_type = hp100_sense_lan(dev)) < 0) {
			printk("hp100: %s: no connection found - check wire\n", dev->name);
			hp100_start_interface(dev);	
			return -EIO;
		}
		if (lp->lan_type == HP100_LAN_100)
			lp->hub_status = hp100_login_to_vg_hub(dev, 0);	
		hp100_start_interface(dev);
	}
	return 0;
}


static netdev_tx_t hp100_start_xmit_bm(struct sk_buff *skb,
				       struct net_device *dev)
{
	unsigned long flags;
	int i, ok_flag;
	int ioaddr = dev->base_addr;
	struct hp100_private *lp = netdev_priv(dev);
	hp100_ring_t *ringptr;

#ifdef HP100_DEBUG_B
	hp100_outw(0x4210, TRACE);
	printk("hp100: %s: start_xmit_bm\n", dev->name);
#endif
	if (skb->len <= 0)
		goto drop;

	if (lp->chip == HP100_CHIPID_SHASTA && skb_padto(skb, ETH_ZLEN))
		return NETDEV_TX_OK;

	
	if (lp->txrtail->next == lp->txrhead) {
		
#ifdef HP100_DEBUG
		printk("hp100: %s: start_xmit_bm: No TX PDL available.\n", dev->name);
#endif
		
		if (time_before(jiffies, dev_trans_start(dev) + HZ))
			goto drop;

		if (hp100_check_lan(dev))
			goto drop;

		if (lp->lan_type == HP100_LAN_100 && lp->hub_status < 0) {
			
			printk("hp100: %s: login to 100Mb/s hub retry\n", dev->name);
			hp100_stop_interface(dev);
			lp->hub_status = hp100_login_to_vg_hub(dev, 0);
			hp100_start_interface(dev);
		} else {
			spin_lock_irqsave(&lp->lock, flags);
			hp100_ints_off();	
			i = hp100_sense_lan(dev);
			hp100_ints_on();
			spin_unlock_irqrestore(&lp->lock, flags);
			if (i == HP100_LAN_ERR)
				printk("hp100: %s: link down detected\n", dev->name);
			else if (lp->lan_type != i) {	
				
				printk("hp100: %s: cable change 10Mb/s <-> 100Mb/s detected\n", dev->name);
				lp->lan_type = i;
				hp100_stop_interface(dev);
				if (lp->lan_type == HP100_LAN_100)
					lp->hub_status = hp100_login_to_vg_hub(dev, 0);
				hp100_start_interface(dev);
			} else {
				printk("hp100: %s: interface reset\n", dev->name);
				hp100_stop_interface(dev);
				if (lp->lan_type == HP100_LAN_100)
					lp->hub_status = hp100_login_to_vg_hub(dev, 0);
				hp100_start_interface(dev);
			}
		}

		goto drop;
	}

	spin_lock_irqsave(&lp->lock, flags);
	ringptr = lp->txrtail;
	lp->txrtail = ringptr->next;

	
	ok_flag = skb->len >= HP100_MIN_PACKET_SIZE;
	i = ok_flag ? skb->len : HP100_MIN_PACKET_SIZE;

	ringptr->skb = skb;
	ringptr->pdl[0] = ((1 << 16) | i);	
	if (lp->chip == HP100_CHIPID_SHASTA) {
		
		ringptr->pdl[2] = i;
	} else {		
		
		ringptr->pdl[2] = skb->len;	
	}
	ringptr->pdl[1] = ((u32) pci_map_single(lp->pci_dev, skb->data, ringptr->pdl[2], PCI_DMA_TODEVICE));	

	
	hp100_outl(ringptr->pdl_paddr, TX_PDA_L);	

	lp->txrcommit++;

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;

	spin_unlock_irqrestore(&lp->lock, flags);

	return NETDEV_TX_OK;

drop:
	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}


static void hp100_clean_txring(struct net_device *dev)
{
	struct hp100_private *lp = netdev_priv(dev);
	int ioaddr = dev->base_addr;
	int donecount;

#ifdef HP100_DEBUG_B
	hp100_outw(0x4211, TRACE);
	printk("hp100: %s: clean txring\n", dev->name);
#endif

	
	donecount = (lp->txrcommit) - hp100_inb(TX_PDL);

#ifdef HP100_DEBUG
	if (donecount > MAX_TX_PDL)
		printk("hp100: %s: Warning: More PDLs transmitted than committed to card???\n", dev->name);
#endif

	for (; 0 != donecount; donecount--) {
#ifdef HP100_DEBUG_BM
		printk("hp100: %s: Free skb: data @0x%.8x txrcommit=0x%x TXPDL=0x%x, done=0x%x\n",
				dev->name, (u_int) lp->txrhead->skb->data,
				lp->txrcommit, hp100_inb(TX_PDL), donecount);
#endif
		
		pci_unmap_single(lp->pci_dev, (dma_addr_t) lp->txrhead->pdl[1], lp->txrhead->pdl[2], PCI_DMA_TODEVICE);
		dev_kfree_skb_any(lp->txrhead->skb);
		lp->txrhead->skb = (void *) NULL;
		lp->txrhead = lp->txrhead->next;
		lp->txrcommit--;
	}
}

static netdev_tx_t hp100_start_xmit(struct sk_buff *skb,
				    struct net_device *dev)
{
	unsigned long flags;
	int i, ok_flag;
	int ioaddr = dev->base_addr;
	u_short val;
	struct hp100_private *lp = netdev_priv(dev);

#ifdef HP100_DEBUG_B
	hp100_outw(0x4212, TRACE);
	printk("hp100: %s: start_xmit\n", dev->name);
#endif
	if (skb->len <= 0)
		goto drop;

	if (hp100_check_lan(dev))
		goto drop;

	
	i = hp100_inl(TX_MEM_FREE) & 0x7fffffff;
	if (!(((i / 2) - 539) > (skb->len + 16) && (hp100_inb(TX_PKT_CNT) < 255))) {
#ifdef HP100_DEBUG
		printk("hp100: %s: start_xmit: tx free mem = 0x%x\n", dev->name, i);
#endif
		
		if (time_before(jiffies, dev_trans_start(dev) + HZ)) {
#ifdef HP100_DEBUG
			printk("hp100: %s: trans_start timing problem\n",
			       dev->name);
#endif
			goto drop;
		}
		if (lp->lan_type == HP100_LAN_100 && lp->hub_status < 0) {
			
			printk("hp100: %s: login to 100Mb/s hub retry\n", dev->name);
			hp100_stop_interface(dev);
			lp->hub_status = hp100_login_to_vg_hub(dev, 0);
			hp100_start_interface(dev);
		} else {
			spin_lock_irqsave(&lp->lock, flags);
			hp100_ints_off();	
			i = hp100_sense_lan(dev);
			hp100_ints_on();
			spin_unlock_irqrestore(&lp->lock, flags);
			if (i == HP100_LAN_ERR)
				printk("hp100: %s: link down detected\n", dev->name);
			else if (lp->lan_type != i) {	
				
				printk("hp100: %s: cable change 10Mb/s <-> 100Mb/s detected\n", dev->name);
				lp->lan_type = i;
				hp100_stop_interface(dev);
				if (lp->lan_type == HP100_LAN_100)
					lp->hub_status = hp100_login_to_vg_hub(dev, 0);
				hp100_start_interface(dev);
			} else {
				printk("hp100: %s: interface reset\n", dev->name);
				hp100_stop_interface(dev);
				if (lp->lan_type == HP100_LAN_100)
					lp->hub_status = hp100_login_to_vg_hub(dev, 0);
				hp100_start_interface(dev);
				mdelay(1);
			}
		}
		goto drop;
	}

	for (i = 0; i < 6000 && (hp100_inb(OPTION_MSW) & HP100_TX_CMD); i++) {
#ifdef HP100_DEBUG_TX
		printk("hp100: %s: start_xmit: busy\n", dev->name);
#endif
	}

	spin_lock_irqsave(&lp->lock, flags);
	hp100_ints_off();
	val = hp100_inw(IRQ_STATUS);
	hp100_outw(HP100_TX_COMPLETE, IRQ_STATUS);
#ifdef HP100_DEBUG_TX
	printk("hp100: %s: start_xmit: irq_status=0x%.4x, irqmask=0x%.4x, len=%d\n",
			dev->name, val, hp100_inw(IRQ_MASK), (int) skb->len);
#endif

	ok_flag = skb->len >= HP100_MIN_PACKET_SIZE;
	i = ok_flag ? skb->len : HP100_MIN_PACKET_SIZE;

	hp100_outw(i, DATA32);	
	hp100_outw(i, FRAGMENT_LEN);	

	if (lp->mode == 2) {	
		
		memcpy_toio(lp->mem_ptr_virt, skb->data, (skb->len + 3) & ~3);
		if (!ok_flag)
			memset_io(lp->mem_ptr_virt, 0, HP100_MIN_PACKET_SIZE - skb->len);
	} else {		
		outsl(ioaddr + HP100_REG_DATA32, skb->data,
		      (skb->len + 3) >> 2);
		if (!ok_flag)
			for (i = (skb->len + 3) & ~3; i < HP100_MIN_PACKET_SIZE; i += 4)
				hp100_outl(0, DATA32);
	}

	hp100_outb(HP100_TX_CMD | HP100_SET_LB, OPTION_MSW);	

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;
	hp100_ints_on();
	spin_unlock_irqrestore(&lp->lock, flags);

	dev_kfree_skb_any(skb);

#ifdef HP100_DEBUG_TX
	printk("hp100: %s: start_xmit: end\n", dev->name);
#endif

	return NETDEV_TX_OK;

drop:
	dev_kfree_skb(skb);
	return NETDEV_TX_OK;

}



static void hp100_rx(struct net_device *dev)
{
	int packets, pkt_len;
	int ioaddr = dev->base_addr;
	struct hp100_private *lp = netdev_priv(dev);
	u_int header;
	struct sk_buff *skb;

#ifdef DEBUG_B
	hp100_outw(0x4213, TRACE);
	printk("hp100: %s: rx\n", dev->name);
#endif

	
	
	
	packets = hp100_inb(RX_PKT_CNT);
#ifdef HP100_DEBUG_RX
	if (packets > 1)
		printk("hp100: %s: rx: waiting packets = %d\n", dev->name, packets);
#endif

	while (packets-- > 0) {
		
		
		for (pkt_len = 0; pkt_len < 6000 && (hp100_inb(OPTION_MSW) & HP100_ADV_NXT_PKT); pkt_len++) {
#ifdef HP100_DEBUG_RX
			printk ("hp100: %s: rx: busy, remaining packets = %d\n", dev->name, packets);
#endif
		}

		
		
		if (lp->mode == 2) {	
			header = readl(lp->mem_ptr_virt);
		} else		
			header = hp100_inl(DATA32);

		pkt_len = ((header & HP100_PKT_LEN_MASK) + 3) & ~3;

#ifdef HP100_DEBUG_RX
		printk("hp100: %s: rx: new packet - length=%d, errors=0x%x, dest=0x%x\n",
				     dev->name, header & HP100_PKT_LEN_MASK,
				     (header >> 16) & 0xfff8, (header >> 16) & 7);
#endif

		
		skb = netdev_alloc_skb(dev, pkt_len + 2);
		if (skb == NULL) {	
#ifdef HP100_DEBUG
			printk("hp100: %s: rx: couldn't allocate a sk_buff of size %d\n",
					     dev->name, pkt_len);
#endif
			dev->stats.rx_dropped++;
		} else {	

			u_char *ptr;

			skb_reserve(skb,2);

			
			skb_put(skb, pkt_len);
			ptr = skb->data;

			
			if (lp->mode == 2)
				memcpy_fromio(ptr, lp->mem_ptr_virt,pkt_len);
			else	
				insl(ioaddr + HP100_REG_DATA32, ptr, pkt_len >> 2);

			skb->protocol = eth_type_trans(skb, dev);

#ifdef HP100_DEBUG_RX
			printk("hp100: %s: rx: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
					dev->name, ptr[0], ptr[1], ptr[2], ptr[3],
		 			ptr[4], ptr[5], ptr[6], ptr[7], ptr[8],
					ptr[9], ptr[10], ptr[11]);
#endif
			netif_rx(skb);
			dev->stats.rx_packets++;
			dev->stats.rx_bytes += pkt_len;
		}

		
		hp100_outb(HP100_ADV_NXT_PKT | HP100_SET_LB, OPTION_MSW);

		switch (header & 0x00070000) {
		case (HP100_MULTI_ADDR_HASH << 16):
		case (HP100_MULTI_ADDR_NO_HASH << 16):
			dev->stats.multicast++;
			break;
		}
	}			
#ifdef HP100_DEBUG_RX
	printk("hp100_rx: %s: end\n", dev->name);
#endif
}

static void hp100_rx_bm(struct net_device *dev)
{
	int ioaddr = dev->base_addr;
	struct hp100_private *lp = netdev_priv(dev);
	hp100_ring_t *ptr;
	u_int header;
	int pkt_len;

#ifdef HP100_DEBUG_B
	hp100_outw(0x4214, TRACE);
	printk("hp100: %s: rx_bm\n", dev->name);
#endif

#ifdef HP100_DEBUG
	if (0 == lp->rxrcommit) {
		printk("hp100: %s: rx_bm called although no PDLs were committed to adapter?\n", dev->name);
		return;
	} else
	if ((hp100_inw(RX_PKT_CNT) & 0x00ff) >= lp->rxrcommit) {
		printk("hp100: %s: More packets received than committed? RX_PKT_CNT=0x%x, commit=0x%x\n",
				     dev->name, hp100_inw(RX_PKT_CNT) & 0x00ff,
				     lp->rxrcommit);
		return;
	}
#endif

	while ((lp->rxrcommit > hp100_inb(RX_PDL))) {

		
		

		ptr = lp->rxrhead;

		header = *(ptr->pdl - 1);
		pkt_len = (header & HP100_PKT_LEN_MASK);

		
		pci_unmap_single(lp->pci_dev, (dma_addr_t) ptr->pdl[3], MAX_ETHER_SIZE, PCI_DMA_FROMDEVICE);

#ifdef HP100_DEBUG_BM
		printk("hp100: %s: rx_bm: header@0x%x=0x%x length=%d, errors=0x%x, dest=0x%x\n",
				dev->name, (u_int) (ptr->pdl - 1), (u_int) header,
				pkt_len, (header >> 16) & 0xfff8, (header >> 16) & 7);
		printk("hp100: %s: RX_PDL_COUNT:0x%x TX_PDL_COUNT:0x%x, RX_PKT_CNT=0x%x PDH=0x%x, Data@0x%x len=0x%x\n",
		   		dev->name, hp100_inb(RX_PDL), hp100_inb(TX_PDL),
				hp100_inb(RX_PKT_CNT), (u_int) * (ptr->pdl),
				(u_int) * (ptr->pdl + 3), (u_int) * (ptr->pdl + 4));
#endif

		if ((pkt_len >= MIN_ETHER_SIZE) &&
		    (pkt_len <= MAX_ETHER_SIZE)) {
			if (ptr->skb == NULL) {
				printk("hp100: %s: rx_bm: skb null\n", dev->name);
				
				dev->stats.rx_dropped++;
			} else {
				skb_trim(ptr->skb, pkt_len);	
				ptr->skb->protocol =
				    eth_type_trans(ptr->skb, dev);

				netif_rx(ptr->skb);	

				dev->stats.rx_packets++;
				dev->stats.rx_bytes += pkt_len;
			}

			switch (header & 0x00070000) {
			case (HP100_MULTI_ADDR_HASH << 16):
			case (HP100_MULTI_ADDR_NO_HASH << 16):
				dev->stats.multicast++;
				break;
			}
		} else {
#ifdef HP100_DEBUG
			printk("hp100: %s: rx_bm: Received bad packet (length=%d)\n", dev->name, pkt_len);
#endif
			if (ptr->skb != NULL)
				dev_kfree_skb_any(ptr->skb);
			dev->stats.rx_errors++;
		}

		lp->rxrhead = lp->rxrhead->next;

		
		if (0 == hp100_build_rx_pdl(lp->rxrtail, dev)) {
			
#ifdef HP100_DEBUG
			printk("hp100: %s: rx_bm: No space for new PDL.\n", dev->name);
#endif
			return;
		} else {	
			hp100_outl((u32) lp->rxrtail->pdl_paddr, RX_PDA);
			lp->rxrtail = lp->rxrtail->next;
		}

	}
}

static struct net_device_stats *hp100_get_stats(struct net_device *dev)
{
	unsigned long flags;
	int ioaddr = dev->base_addr;
	struct hp100_private *lp = netdev_priv(dev);

#ifdef HP100_DEBUG_B
	hp100_outw(0x4215, TRACE);
#endif

	spin_lock_irqsave(&lp->lock, flags);
	hp100_ints_off();	
	hp100_update_stats(dev);
	hp100_ints_on();
	spin_unlock_irqrestore(&lp->lock, flags);
	return &(dev->stats);
}

static void hp100_update_stats(struct net_device *dev)
{
	int ioaddr = dev->base_addr;
	u_short val;

#ifdef HP100_DEBUG_B
	hp100_outw(0x4216, TRACE);
	printk("hp100: %s: update-stats\n", dev->name);
#endif

	
	hp100_page(MAC_CTRL);
	val = hp100_inw(DROPPED) & 0x0fff;
	dev->stats.rx_errors += val;
	dev->stats.rx_over_errors += val;
	val = hp100_inb(CRC);
	dev->stats.rx_errors += val;
	dev->stats.rx_crc_errors += val;
	val = hp100_inb(ABORT);
	dev->stats.tx_errors += val;
	dev->stats.tx_aborted_errors += val;
	hp100_page(PERFORMANCE);
}

static void hp100_misc_interrupt(struct net_device *dev)
{
#ifdef HP100_DEBUG_B
	int ioaddr = dev->base_addr;
#endif

#ifdef HP100_DEBUG_B
	int ioaddr = dev->base_addr;
	hp100_outw(0x4216, TRACE);
	printk("hp100: %s: misc_interrupt\n", dev->name);
#endif

	
	dev->stats.rx_errors++;
	dev->stats.tx_errors++;
}

static void hp100_clear_stats(struct hp100_private *lp, int ioaddr)
{
	unsigned long flags;

#ifdef HP100_DEBUG_B
	hp100_outw(0x4217, TRACE);
	printk("hp100: %s: clear_stats\n", dev->name);
#endif

	spin_lock_irqsave(&lp->lock, flags);
	hp100_page(MAC_CTRL);	
	hp100_inw(DROPPED);
	hp100_inb(CRC);
	hp100_inb(ABORT);
	hp100_page(PERFORMANCE);
	spin_unlock_irqrestore(&lp->lock, flags);
}




static void hp100_set_multicast_list(struct net_device *dev)
{
	unsigned long flags;
	int ioaddr = dev->base_addr;
	struct hp100_private *lp = netdev_priv(dev);

#ifdef HP100_DEBUG_B
	hp100_outw(0x4218, TRACE);
	printk("hp100: %s: set_mc_list\n", dev->name);
#endif

	spin_lock_irqsave(&lp->lock, flags);
	hp100_ints_off();
	hp100_page(MAC_CTRL);
	hp100_andb(~(HP100_RX_EN | HP100_TX_EN), MAC_CFG_1);	

	if (dev->flags & IFF_PROMISC) {
		lp->mac2_mode = HP100_MAC2MODE6;	
		lp->mac1_mode = HP100_MAC1MODE6;	
		memset(&lp->hash_bytes, 0xff, 8);
	} else if (!netdev_mc_empty(dev) || (dev->flags & IFF_ALLMULTI)) {
		lp->mac2_mode = HP100_MAC2MODE5;	
		lp->mac1_mode = HP100_MAC1MODE5;	
#ifdef HP100_MULTICAST_FILTER	
		if (dev->flags & IFF_ALLMULTI) {
			
			memset(&lp->hash_bytes, 0xff, 8);
		} else {
			int i, idx;
			u_char *addrs;
			struct netdev_hw_addr *ha;

			memset(&lp->hash_bytes, 0x00, 8);
#ifdef HP100_DEBUG
			printk("hp100: %s: computing hash filter - mc_count = %i\n",
			       dev->name, netdev_mc_count(dev));
#endif
			netdev_for_each_mc_addr(ha, dev) {
				addrs = ha->addr;
#ifdef HP100_DEBUG
				printk("hp100: %s: multicast = %pM, ",
					     dev->name, addrs);
#endif
				for (i = idx = 0; i < 6; i++) {
					idx ^= *addrs++ & 0x3f;
					printk(":%02x:", idx);
				}
#ifdef HP100_DEBUG
				printk("idx = %i\n", idx);
#endif
				lp->hash_bytes[idx >> 3] |= (1 << (idx & 7));
			}
		}
#else
		memset(&lp->hash_bytes, 0xff, 8);
#endif
	} else {
		lp->mac2_mode = HP100_MAC2MODE3;	
		lp->mac1_mode = HP100_MAC1MODE3;	
		memset(&lp->hash_bytes, 0x00, 8);
	}

	if (((hp100_inb(MAC_CFG_1) & 0x0f) != lp->mac1_mode) ||
	    (hp100_inb(MAC_CFG_2) != lp->mac2_mode)) {
		int i;

		hp100_outb(lp->mac2_mode, MAC_CFG_2);
		hp100_andb(HP100_MAC1MODEMASK, MAC_CFG_1);	
		hp100_orb(lp->mac1_mode, MAC_CFG_1);	

		hp100_page(MAC_ADDRESS);
		for (i = 0; i < 8; i++)
			hp100_outb(lp->hash_bytes[i], HASH_BYTE0 + i);
#ifdef HP100_DEBUG
		printk("hp100: %s: mac1 = 0x%x, mac2 = 0x%x, multicast hash = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
				     dev->name, lp->mac1_mode, lp->mac2_mode,
				     lp->hash_bytes[0], lp->hash_bytes[1],
				     lp->hash_bytes[2], lp->hash_bytes[3],
				     lp->hash_bytes[4], lp->hash_bytes[5],
				     lp->hash_bytes[6], lp->hash_bytes[7]);
#endif

		if (lp->lan_type == HP100_LAN_100) {
#ifdef HP100_DEBUG
			printk("hp100: %s: 100VG MAC settings have changed - relogin.\n", dev->name);
#endif
			lp->hub_status = hp100_login_to_vg_hub(dev, 1);	
		}
	} else {
		int i;
		u_char old_hash_bytes[8];

		hp100_page(MAC_ADDRESS);
		for (i = 0; i < 8; i++)
			old_hash_bytes[i] = hp100_inb(HASH_BYTE0 + i);
		if (memcmp(old_hash_bytes, &lp->hash_bytes, 8)) {
			for (i = 0; i < 8; i++)
				hp100_outb(lp->hash_bytes[i], HASH_BYTE0 + i);
#ifdef HP100_DEBUG
			printk("hp100: %s: multicast hash = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
					dev->name, lp->hash_bytes[0],
					lp->hash_bytes[1], lp->hash_bytes[2],
					lp->hash_bytes[3], lp->hash_bytes[4],
					lp->hash_bytes[5], lp->hash_bytes[6],
					lp->hash_bytes[7]);
#endif

			if (lp->lan_type == HP100_LAN_100) {
#ifdef HP100_DEBUG
				printk("hp100: %s: 100VG MAC settings have changed - relogin.\n", dev->name);
#endif
				lp->hub_status = hp100_login_to_vg_hub(dev, 1);	
			}
		}
	}

	hp100_page(MAC_CTRL);
	hp100_orb(HP100_RX_EN | HP100_RX_IDLE |	
		  HP100_TX_EN | HP100_TX_IDLE, MAC_CFG_1);	

	hp100_page(PERFORMANCE);
	hp100_ints_on();
	spin_unlock_irqrestore(&lp->lock, flags);
}


static irqreturn_t hp100_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = (struct net_device *) dev_id;
	struct hp100_private *lp = netdev_priv(dev);

	int ioaddr;
	u_int val;

	if (dev == NULL)
		return IRQ_NONE;
	ioaddr = dev->base_addr;

	spin_lock(&lp->lock);

	hp100_ints_off();

#ifdef HP100_DEBUG_B
	hp100_outw(0x4219, TRACE);
#endif

	
	val = hp100_inw(IRQ_STATUS);
#ifdef HP100_DEBUG_IRQ
	printk("hp100: %s: mode=%x,IRQ_STAT=0x%.4x,RXPKTCNT=0x%.2x RXPDL=0x%.2x TXPKTCNT=0x%.2x TXPDL=0x%.2x\n",
			     dev->name, lp->mode, (u_int) val, hp100_inb(RX_PKT_CNT),
			     hp100_inb(RX_PDL), hp100_inb(TX_PKT_CNT), hp100_inb(TX_PDL));
#endif

	if (val == 0) {		
		spin_unlock(&lp->lock);
		hp100_ints_on();
		return IRQ_NONE;
	}
	
	

	if (val & HP100_RX_PDL_FILL_COMPL) {
		if (lp->mode == 1)
			hp100_rx_bm(dev);
		else {
			printk("hp100: %s: rx_pdl_fill_compl interrupt although not busmaster?\n", dev->name);
		}
	}


	if (val & HP100_RX_PACKET) {	
		if (lp->mode != 1)	
			hp100_rx(dev);
		else if (!(val & HP100_RX_PDL_FILL_COMPL)) {
			
			hp100_rx_bm(dev);
		}
	}

	hp100_outw(val, IRQ_STATUS);

	if (val & (HP100_TX_ERROR | HP100_RX_ERROR)) {
#ifdef HP100_DEBUG_IRQ
		printk("hp100: %s: TX/RX Error IRQ\n", dev->name);
#endif
		hp100_update_stats(dev);
		if (lp->mode == 1) {
			hp100_rxfill(dev);
			hp100_clean_txring(dev);
		}
	}

	if ((lp->mode == 1) && (val & (HP100_RX_PDA_ZERO)))
		hp100_rxfill(dev);

	if ((lp->mode == 1) && (val & (HP100_TX_COMPLETE)))
		hp100_clean_txring(dev);

	if (val & HP100_MISC_ERROR) {	
#ifdef HP100_DEBUG_IRQ
		printk
		    ("hp100: %s: Misc. Error Interrupt - Check cabling.\n",
		     dev->name);
#endif
		if (lp->mode == 1) {
			hp100_clean_txring(dev);
			hp100_rxfill(dev);
		}
		hp100_misc_interrupt(dev);
	}

	spin_unlock(&lp->lock);
	hp100_ints_on();
	return IRQ_HANDLED;
}


static void hp100_start_interface(struct net_device *dev)
{
	unsigned long flags;
	int ioaddr = dev->base_addr;
	struct hp100_private *lp = netdev_priv(dev);

#ifdef HP100_DEBUG_B
	hp100_outw(0x4220, TRACE);
	printk("hp100: %s: hp100_start_interface\n", dev->name);
#endif

	spin_lock_irqsave(&lp->lock, flags);

	
	
	hp100_page(PERFORMANCE);
	hp100_outw(0xfefe, IRQ_MASK);	
	hp100_outw(0xffff, IRQ_STATUS);	
	hp100_outw(HP100_FAKE_INT | HP100_INT_EN | HP100_RESET_LB,
		   OPTION_LSW);
	
	hp100_outw(HP100_TRI_INT | HP100_RESET_HB, OPTION_LSW);

	if (lp->mode == 1) {
		
		hp100_page(HW_MAP);
		hp100_orb(HP100_BM_MASTER, BM);
		hp100_rxfill(dev);
	} else if (lp->mode == 2) {
		
		hp100_outw(HP100_MMAP_DIS | HP100_RESET_HB, OPTION_LSW);
	}

	hp100_page(PERFORMANCE);
	hp100_outw(0xfefe, IRQ_MASK);	
	hp100_outw(0xffff, IRQ_STATUS);	

	
	if (lp->mode == 1) {	
		hp100_outw(HP100_RX_PDL_FILL_COMPL |
			   HP100_RX_PDA_ZERO | HP100_RX_ERROR |
			   
			    HP100_SET_HB |
			   
			   HP100_TX_COMPLETE |
			   
			   HP100_TX_ERROR | HP100_SET_LB, IRQ_MASK);
	} else {
		hp100_outw(HP100_RX_PACKET |
			   HP100_RX_ERROR | HP100_SET_HB |
			   HP100_TX_ERROR | HP100_SET_LB, IRQ_MASK);
	}

	spin_unlock_irqrestore(&lp->lock, flags);

	
	hp100_set_multicast_list(dev);
}

static void hp100_stop_interface(struct net_device *dev)
{
	struct hp100_private *lp = netdev_priv(dev);
	int ioaddr = dev->base_addr;
	u_int val;

#ifdef HP100_DEBUG_B
	printk("hp100: %s: hp100_stop_interface\n", dev->name);
	hp100_outw(0x4221, TRACE);
#endif

	if (lp->mode == 1)
		hp100_BM_shutdown(dev);
	else {
		
		hp100_outw(HP100_INT_EN | HP100_RESET_LB |
			   HP100_TRI_INT | HP100_MMAP_DIS | HP100_SET_HB,
			   OPTION_LSW);
		val = hp100_inw(OPTION_LSW);

		hp100_page(MAC_CTRL);
		hp100_andb(~(HP100_RX_EN | HP100_TX_EN), MAC_CFG_1);

		if (!(val & HP100_HW_RST))
			return;	
		
		for (val = 0; val < 6000; val++)
			if ((hp100_inb(MAC_CFG_1) & (HP100_TX_IDLE | HP100_RX_IDLE)) == (HP100_TX_IDLE | HP100_RX_IDLE)) {
				hp100_page(PERFORMANCE);
				return;
			}
		printk("hp100: %s: hp100_stop_interface - timeout\n", dev->name);
		hp100_page(PERFORMANCE);
	}
}

static void hp100_load_eeprom(struct net_device *dev, u_short probe_ioaddr)
{
	int i;
	int ioaddr = probe_ioaddr > 0 ? probe_ioaddr : dev->base_addr;

#ifdef HP100_DEBUG_B
	hp100_outw(0x4222, TRACE);
#endif

	hp100_page(EEPROM_CTRL);
	hp100_andw(~HP100_EEPROM_LOAD, EEPROM_CTRL);
	hp100_orw(HP100_EEPROM_LOAD, EEPROM_CTRL);
	for (i = 0; i < 10000; i++)
		if (!(hp100_inb(OPTION_MSW) & HP100_EE_LOAD))
			return;
	printk("hp100: %s: hp100_load_eeprom - timeout\n", dev->name);
}

static int hp100_sense_lan(struct net_device *dev)
{
	int ioaddr = dev->base_addr;
	u_short val_VG, val_10;
	struct hp100_private *lp = netdev_priv(dev);

#ifdef HP100_DEBUG_B
	hp100_outw(0x4223, TRACE);
#endif

	hp100_page(MAC_CTRL);
	val_10 = hp100_inb(10_LAN_CFG_1);
	val_VG = hp100_inb(VG_LAN_CFG_1);
	hp100_page(PERFORMANCE);
#ifdef HP100_DEBUG
	printk("hp100: %s: sense_lan: val_VG = 0x%04x, val_10 = 0x%04x\n",
	       dev->name, val_VG, val_10);
#endif

	if (val_10 & HP100_LINK_BEAT_ST)	
		return HP100_LAN_10;

	if (val_10 & HP100_AUI_ST) {	
		val_10 |= HP100_AUI_SEL | HP100_LOW_TH;
		hp100_page(MAC_CTRL);
		hp100_outb(val_10, 10_LAN_CFG_1);
		hp100_page(PERFORMANCE);
		return HP100_LAN_COAX;
	}

	
	if ( !strcmp(lp->id, "HWP1920")  ||
	     (lp->pci_dev &&
	      lp->pci_dev->vendor == PCI_VENDOR_ID &&
	      (lp->pci_dev->device == PCI_DEVICE_ID_HP_J2970A ||
	       lp->pci_dev->device == PCI_DEVICE_ID_HP_J2973A)))
		return HP100_LAN_ERR;

	if (val_VG & HP100_LINK_CABLE_ST)	
		return HP100_LAN_100;
	return HP100_LAN_ERR;
}

static int hp100_down_vg_link(struct net_device *dev)
{
	struct hp100_private *lp = netdev_priv(dev);
	int ioaddr = dev->base_addr;
	unsigned long time;
	long savelan, newlan;

#ifdef HP100_DEBUG_B
	hp100_outw(0x4224, TRACE);
	printk("hp100: %s: down_vg_link\n", dev->name);
#endif

	hp100_page(MAC_CTRL);
	time = jiffies + (HZ / 4);
	do {
		if (hp100_inb(VG_LAN_CFG_1) & HP100_LINK_CABLE_ST)
			break;
		if (!in_interrupt())
			schedule_timeout_interruptible(1);
	} while (time_after(time, jiffies));

	if (time_after_eq(jiffies, time))	
		return 0;

	

	hp100_andb(~(HP100_LOAD_ADDR | HP100_LINK_CMD), VG_LAN_CFG_1);
	hp100_orb(HP100_VG_SEL, VG_LAN_CFG_1);

	
	time = jiffies + (HZ / 2);
	do {
		if (!(hp100_inb(VG_LAN_CFG_1) & HP100_LINK_UP_ST))
			break;
		if (!in_interrupt())
			schedule_timeout_interruptible(1);
	} while (time_after(time, jiffies));

#ifdef HP100_DEBUG
	if (time_after_eq(jiffies, time))
		printk("hp100: %s: down_vg_link: Link does not go down?\n", dev->name);
#endif

	
	
	
	if (lp->chip == HP100_CHIPID_LASSEN) {
		
		
		hp100_andb(~HP100_VG_RESET, VG_LAN_CFG_1);
		udelay(1500);	
		hp100_orb(HP100_VG_RESET, VG_LAN_CFG_1);	
		udelay(1500);
	}

	
	
	
	
	
	if (lp->chip == HP100_CHIPID_LASSEN) {
		
		savelan = newlan = hp100_inl(10_LAN_CFG_1);	
		newlan &= ~(HP100_VG_SEL << 16);
		newlan |= (HP100_DOT3_MAC) << 8;
		hp100_andb(~HP100_AUTO_MODE, MAC_CFG_3);	
		hp100_outl(newlan, 10_LAN_CFG_1);

		
		time = jiffies + (HZ * 5);
		do {
			if (!(hp100_inb(MAC_CFG_4) & HP100_MAC_SEL_ST))
				break;
			if (!in_interrupt())
				schedule_timeout_interruptible(1);
		} while (time_after(time, jiffies));

		hp100_orb(HP100_AUTO_MODE, MAC_CFG_3);	
		hp100_outl(savelan, 10_LAN_CFG_1);
	}

	time = jiffies + (3 * HZ);	
	do {
		if ((hp100_inb(VG_LAN_CFG_1) & HP100_LINK_CABLE_ST) == 0)
			break;
		if (!in_interrupt())
			schedule_timeout_interruptible(1);
	} while (time_after(time, jiffies));

	if (time_before_eq(time, jiffies)) {
#ifdef HP100_DEBUG
		printk("hp100: %s: down_vg_link: timeout\n", dev->name);
#endif
		return -EIO;
	}

	time = jiffies + (2 * HZ);	
	do {
		if (!in_interrupt())
			schedule_timeout_interruptible(1);
	} while (time_after(time, jiffies));

	return 0;
}

static int hp100_login_to_vg_hub(struct net_device *dev, u_short force_relogin)
{
	int ioaddr = dev->base_addr;
	struct hp100_private *lp = netdev_priv(dev);
	u_short val = 0;
	unsigned long time;
	int startst;

#ifdef HP100_DEBUG_B
	hp100_outw(0x4225, TRACE);
	printk("hp100: %s: login_to_vg_hub\n", dev->name);
#endif

	hp100_page(MAC_CTRL);
	startst = hp100_inb(VG_LAN_CFG_1);
	if ((force_relogin == 1) || (hp100_inb(MAC_CFG_4) & HP100_MAC_SEL_ST)) {
#ifdef HP100_DEBUG_TRAINING
		printk("hp100: %s: Start training\n", dev->name);
#endif

		
		hp100_orb(HP100_VG_RESET, VG_LAN_CFG_1);

		
		
		if ((lp->chip == HP100_CHIPID_LASSEN) && (startst & HP100_LINK_CABLE_ST))
			hp100_andb(~HP100_DOT3_MAC, 10_LAN_CFG_2);

		
		hp100_andb(~(HP100_LINK_CMD  ), VG_LAN_CFG_1);

#ifdef HP100_DEBUG_TRAINING
		printk("hp100: %s: Bring down the link\n", dev->name);
#endif

		
		time = jiffies + (HZ / 10);
		do {
			if (~(hp100_inb(VG_LAN_CFG_1) & HP100_LINK_UP_ST))
				break;
			if (!in_interrupt())
				schedule_timeout_interruptible(1);
		} while (time_after(time, jiffies));

		
		if ((dev->flags) & IFF_PROMISC) {
			hp100_orb(HP100_PROM_MODE, VG_LAN_CFG_2);
			if (lp->chip == HP100_CHIPID_LASSEN)
				hp100_orw(HP100_MACRQ_PROMSC, TRAIN_REQUEST);
		} else {
			hp100_andb(~HP100_PROM_MODE, VG_LAN_CFG_2);
			if (lp->chip == HP100_CHIPID_LASSEN) {
				hp100_andw(~HP100_MACRQ_PROMSC, TRAIN_REQUEST);
			}
		}

		
		if (lp->chip == HP100_CHIPID_LASSEN)
			hp100_orb(HP100_MACRQ_FRAMEFMT_EITHER, TRAIN_REQUEST);

		hp100_orb(HP100_LINK_CMD | HP100_LOAD_ADDR | HP100_VG_RESET, VG_LAN_CFG_1);

		
		
		

		
		
		hp100_page(MAC_CTRL);
		time = jiffies + (1 * HZ);	
		do {
			if (hp100_inb(VG_LAN_CFG_1) & HP100_LINK_CABLE_ST)
				break;
			if (!in_interrupt())
				schedule_timeout_interruptible(1);
		} while (time_before(jiffies, time));

		if (time_after_eq(jiffies, time)) {
#ifdef HP100_DEBUG_TRAINING
			printk("hp100: %s: Link cable status not ok? Training aborted.\n", dev->name);
#endif
		} else {
#ifdef HP100_DEBUG_TRAINING
			printk
			    ("hp100: %s: HUB tones detected. Trying to train.\n",
			     dev->name);
#endif

			time = jiffies + (2 * HZ);	
			do {
				val = hp100_inb(VG_LAN_CFG_1);
				if ((val & (HP100_LINK_UP_ST))) {
#ifdef HP100_DEBUG_TRAINING
					printk("hp100: %s: Passed training.\n", dev->name);
#endif
					break;
				}
				if (!in_interrupt())
					schedule_timeout_interruptible(1);
			} while (time_after(time, jiffies));
		}

		
		if (time_before_eq(jiffies, time) && (val & HP100_LINK_UP_ST)) {
#ifdef HP100_DEBUG_TRAINING
			printk("hp100: %s: Successfully logged into the HUB.\n", dev->name);
			if (lp->chip == HP100_CHIPID_LASSEN) {
				val = hp100_inw(TRAIN_ALLOW);
				printk("hp100: %s: Card supports 100VG MAC Version \"%s\" ",
					     dev->name, (hp100_inw(TRAIN_REQUEST) & HP100_CARD_MACVER) ? "802.12" : "Pre");
				printk("Driver will use MAC Version \"%s\"\n", (val & HP100_HUB_MACVER) ? "802.12" : "Pre");
				printk("hp100: %s: Frame format is %s.\n", dev->name, (val & HP100_MALLOW_FRAMEFMT) ? "802.5" : "802.3");
			}
#endif
		} else {
			
			printk("hp100: %s: Problem logging into the HUB.\n", dev->name);
			if (lp->chip == HP100_CHIPID_LASSEN) {
				
				val = hp100_inw(TRAIN_ALLOW);	
#ifdef HP100_DEBUG_TRAINING
				printk("hp100: %s: MAC Configuration requested: 0x%04x, HUB allowed: 0x%04x\n", dev->name, hp100_inw(TRAIN_REQUEST), val);
#endif
				if (val & HP100_MALLOW_ACCDENIED)
					printk("hp100: %s: HUB access denied.\n", dev->name);
				if (val & HP100_MALLOW_CONFIGURE)
					printk("hp100: %s: MAC Configuration is incompatible with the Network.\n", dev->name);
				if (val & HP100_MALLOW_DUPADDR)
					printk("hp100: %s: Duplicate MAC Address on the Network.\n", dev->name);
			}
		}

		
		

		if ((lp->chip == HP100_CHIPID_LASSEN) && (startst & HP100_LINK_CABLE_ST)) {
			hp100_page(MAC_CTRL);
			hp100_orb(HP100_DOT3_MAC, 10_LAN_CFG_2);
		}

		val = hp100_inb(VG_LAN_CFG_1);

		
		hp100_page(PERFORMANCE);
		hp100_outw(HP100_MISC_ERROR, IRQ_STATUS);

		if (val & HP100_LINK_UP_ST)
			return 0;	
		else {
			printk("hp100: %s: Training failed.\n", dev->name);
			hp100_down_vg_link(dev);
			return -EIO;
		}
	}
	
	return -EIO;
}

static void hp100_cascade_reset(struct net_device *dev, u_short enable)
{
	int ioaddr = dev->base_addr;
	struct hp100_private *lp = netdev_priv(dev);

#ifdef HP100_DEBUG_B
	hp100_outw(0x4226, TRACE);
	printk("hp100: %s: cascade_reset\n", dev->name);
#endif

	if (enable) {
		hp100_outw(HP100_HW_RST | HP100_RESET_LB, OPTION_LSW);
		if (lp->chip == HP100_CHIPID_LASSEN) {
			
			hp100_page(HW_MAP);
			hp100_andb(~HP100_PCI_RESET, PCICTRL2);
			hp100_orb(HP100_PCI_RESET, PCICTRL2);
			
			
			
			udelay(400);
			hp100_andb(~HP100_PCI_RESET, PCICTRL2);
			hp100_page(PERFORMANCE);
		}
	} else {		
		hp100_outw(HP100_HW_RST | HP100_SET_LB, OPTION_LSW);
		udelay(400);
		hp100_page(PERFORMANCE);
	}
}

#ifdef HP100_DEBUG
void hp100_RegisterDump(struct net_device *dev)
{
	int ioaddr = dev->base_addr;
	int Page;
	int Register;

	
	printk("hp100: %s: Cascade Register Dump\n", dev->name);
	printk("hardware id #1: 0x%.2x\n", hp100_inb(HW_ID));
	printk("hardware id #2/paging: 0x%.2x\n", hp100_inb(PAGING));
	printk("option #1: 0x%.4x\n", hp100_inw(OPTION_LSW));
	printk("option #2: 0x%.4x\n", hp100_inw(OPTION_MSW));

	
	for (Page = 0; Page < 8; Page++) {
		
		printk("page: 0x%.2x\n", Page);
		outw(Page, ioaddr + 0x02);
		for (Register = 0x8; Register < 0x22; Register += 2) {
			
			if (((Register != 0x10) && (Register != 0x12)) || (Page > 0)) {
				printk("0x%.2x = 0x%.4x\n", Register, inw(ioaddr + Register));
			}
		}
	}
	hp100_page(PERFORMANCE);
}
#endif


static void cleanup_dev(struct net_device *d)
{
	struct hp100_private *p = netdev_priv(d);

	unregister_netdev(d);
	release_region(d->base_addr, HP100_REGION_SIZE);

	if (p->mode == 1)	
		pci_free_consistent(p->pci_dev, MAX_RINGSIZE + 0x0f,
				    p->page_vaddr_algn,
				    virt_to_whatever(d, p->page_vaddr_algn));
	if (p->mem_ptr_virt)
		iounmap(p->mem_ptr_virt);

	free_netdev(d);
}

#ifdef CONFIG_EISA
static int __init hp100_eisa_probe (struct device *gendev)
{
	struct net_device *dev = alloc_etherdev(sizeof(struct hp100_private));
	struct eisa_device *edev = to_eisa_device(gendev);
	int err;

	if (!dev)
		return -ENOMEM;

	SET_NETDEV_DEV(dev, &edev->dev);

	err = hp100_probe1(dev, edev->base_addr + 0xC38, HP100_BUS_EISA, NULL);
	if (err)
		goto out1;

#ifdef HP100_DEBUG
	printk("hp100: %s: EISA adapter found at 0x%x\n", dev->name,
	       dev->base_addr);
#endif
	dev_set_drvdata(gendev, dev);
	return 0;
 out1:
	free_netdev(dev);
	return err;
}

static int __devexit hp100_eisa_remove (struct device *gendev)
{
	struct net_device *dev = dev_get_drvdata(gendev);
	cleanup_dev(dev);
	return 0;
}

static struct eisa_driver hp100_eisa_driver = {
        .id_table = hp100_eisa_tbl,
        .driver   = {
                .name    = "hp100",
                .probe   = hp100_eisa_probe,
                .remove  = __devexit_p (hp100_eisa_remove),
        }
};
#endif

#ifdef CONFIG_PCI
static int __devinit hp100_pci_probe (struct pci_dev *pdev,
				     const struct pci_device_id *ent)
{
	struct net_device *dev;
	int ioaddr;
	u_short pci_command;
	int err;

	if (pci_enable_device(pdev))
		return -ENODEV;

	dev = alloc_etherdev(sizeof(struct hp100_private));
	if (!dev) {
		err = -ENOMEM;
		goto out0;
	}

	SET_NETDEV_DEV(dev, &pdev->dev);

	pci_read_config_word(pdev, PCI_COMMAND, &pci_command);
	if (!(pci_command & PCI_COMMAND_IO)) {
#ifdef HP100_DEBUG
		printk("hp100: %s: PCI I/O Bit has not been set. Setting...\n", dev->name);
#endif
		pci_command |= PCI_COMMAND_IO;
		pci_write_config_word(pdev, PCI_COMMAND, pci_command);
	}

	if (!(pci_command & PCI_COMMAND_MASTER)) {
#ifdef HP100_DEBUG
		printk("hp100: %s: PCI Master Bit has not been set. Setting...\n", dev->name);
#endif
		pci_command |= PCI_COMMAND_MASTER;
		pci_write_config_word(pdev, PCI_COMMAND, pci_command);
	}

	ioaddr = pci_resource_start(pdev, 0);
	err = hp100_probe1(dev, ioaddr, HP100_BUS_PCI, pdev);
	if (err)
		goto out1;

#ifdef HP100_DEBUG
	printk("hp100: %s: PCI adapter found at 0x%x\n", dev->name, ioaddr);
#endif
	pci_set_drvdata(pdev, dev);
	return 0;
 out1:
	free_netdev(dev);
 out0:
	pci_disable_device(pdev);
	return err;
}

static void __devexit hp100_pci_remove (struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);

	cleanup_dev(dev);
	pci_disable_device(pdev);
}


static struct pci_driver hp100_pci_driver = {
	.name		= "hp100",
	.id_table	= hp100_pci_tbl,
	.probe		= hp100_pci_probe,
	.remove		= __devexit_p(hp100_pci_remove),
};
#endif


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jaroslav Kysela <perex@perex.cz>, "
              "Siegfried \"Frieder\" Loeffler (dg1sek) <floeff@mathematik.uni-stuttgart.de>");
MODULE_DESCRIPTION("HP CASCADE Architecture Driver for 100VG-AnyLan Network Adapters");

#if defined(MODULE) && defined(CONFIG_ISA)
#define HP100_DEVICES 5
static int hp100_port[HP100_DEVICES] = { 0, [1 ... (HP100_DEVICES-1)] = -1 };
module_param_array(hp100_port, int, NULL, 0);

static struct net_device *hp100_devlist[HP100_DEVICES];

static int __init hp100_isa_init(void)
{
	struct net_device *dev;
	int i, err, cards = 0;

	
	if (hp100_port[0] == 0)
		return -ENODEV;

	
	for (i = 0; i < HP100_DEVICES && hp100_port[i] != -1; ++i) {
		dev = alloc_etherdev(sizeof(struct hp100_private));
		if (!dev) {
			while (cards > 0)
				cleanup_dev(hp100_devlist[--cards]);

			return -ENOMEM;
		}

		err = hp100_isa_probe(dev, hp100_port[i]);
		if (!err)
			hp100_devlist[cards++] = dev;
		else
			free_netdev(dev);
	}

	return cards > 0 ? 0 : -ENODEV;
}

static void hp100_isa_cleanup(void)
{
	int i;

	for (i = 0; i < HP100_DEVICES; i++) {
		struct net_device *dev = hp100_devlist[i];
		if (dev)
			cleanup_dev(dev);
	}
}
#else
#define hp100_isa_init()	(0)
#define hp100_isa_cleanup()	do { } while(0)
#endif

static int __init hp100_module_init(void)
{
	int err;

	err = hp100_isa_init();
	if (err && err != -ENODEV)
		goto out;
#ifdef CONFIG_EISA
	err = eisa_driver_register(&hp100_eisa_driver);
	if (err && err != -ENODEV)
		goto out2;
#endif
#ifdef CONFIG_PCI
	err = pci_register_driver(&hp100_pci_driver);
	if (err && err != -ENODEV)
		goto out3;
#endif
 out:
	return err;
 out3:
#ifdef CONFIG_EISA
	eisa_driver_unregister (&hp100_eisa_driver);
 out2:
#endif
	hp100_isa_cleanup();
	goto out;
}


static void __exit hp100_module_exit(void)
{
	hp100_isa_cleanup();
#ifdef CONFIG_EISA
	eisa_driver_unregister (&hp100_eisa_driver);
#endif
#ifdef CONFIG_PCI
	pci_unregister_driver (&hp100_pci_driver);
#endif
}

module_init(hp100_module_init)
module_exit(hp100_module_exit)
