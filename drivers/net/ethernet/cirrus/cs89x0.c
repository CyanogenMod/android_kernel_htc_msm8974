
/*
	Written 1996 by Russell Nelson, with reference to skeleton.c
	written 1993-1994 by Donald Becker.

	This software may be used and distributed according to the terms
	of the GNU General Public License, incorporated herein by reference.

        The author may be reached at nelson@crynwr.com, Crynwr
        Software, 521 Pleasant Valley Rd., Potsdam, NY 13676

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

  Russ Nelson       : Jul 13 1998.  Added RxOnly DMA support.

  Melody Lee        : Aug 10 1999.  Changes for Linux 2.2.5 compatibility.
                    : email: ethernet@crystal.cirrus.com

  Alan Cox          : Removed 1.2 support, added 2.1 extra counters.

  Andrew Morton     : Kernel 2.3.48
                    : Handle kmalloc() failures
                    : Other resource allocation fixes
                    : Add SMP locks
                    : Integrate Russ Nelson's ALLOW_DMA functionality back in.
                    : If ALLOW_DMA is true, make DMA runtime selectable
                    : Folded in changes from Cirrus (Melody Lee
                    : <klee@crystal.cirrus.com>)
                    : Don't call netif_wake_queue() in net_send_packet()
                    : Fixed an out-of-mem bug in dma_rx()
                    : Updated Documentation/networking/cs89x0.txt

  Andrew Morton     : Kernel 2.3.99-pre1
                    : Use skb_reserve to longword align IP header (two places)
                    : Remove a delay loop from dma_rx()
                    : Replace '100' with HZ
                    : Clean up a couple of skb API abuses
                    : Added 'cs89x0_dma=N' kernel boot option
                    : Correctly initialise lp->lock in non-module compile

  Andrew Morton     : Kernel 2.3.99-pre4-1
                    : MOD_INC/DEC race fix (see
                    : http://www.uwsg.indiana.edu/hypermail/linux/kernel/0003.3/1532.html)

  Andrew Morton     : Kernel 2.4.0-test7-pre2
                    : Enhanced EEPROM support to cover more devices,
                    :   abstracted IRQ mapping to support CONFIG_ARCH_CLPS7500 arch
                    :   (Jason Gunthorpe <jgg@ualberta.ca>)

  Andrew Morton     : Kernel 2.4.0-test11-pre4
                    : Use dev->name in request_*() (Andrey Panin)
                    : Fix an error-path memleak in init_module()
                    : Preserve return value from request_irq()
                    : Fix type of `media' module parm (Keith Owens)
                    : Use SET_MODULE_OWNER()
                    : Tidied up strange request_irq() abuse in net_open().

  Andrew Morton     : Kernel 2.4.3-pre1
                    : Request correct number of pages for DMA (Hugh Dickens)
                    : Select PP_ChipID _after_ unregister_netdev in cleanup_module()
                    :  because unregister_netdev() calls get_stats.
                    : Make `version[]' __initdata
                    : Uninlined the read/write reg/word functions.

  Oskar Schirmer    : oskar@scara.com
                    : HiCO.SH4 (superh) support added (irq#1, cs89x0_media=)

  Deepak Saxena     : dsaxena@plexity.net
                    : Intel IXDP2x01 (XScale ixp2x00 NPU) platform support

  Dmitry Pervushin  : dpervushin@ru.mvista.com
                    : PNX010X platform support

  Deepak Saxena     : dsaxena@plexity.net
                    : Intel IXDP2351 platform support

  Dmitry Pervushin  : dpervushin@ru.mvista.com
                    : PNX010X platform support

  Domenico Andreoli : cavokz@gmail.com
                    : QQ2440 platform support

*/


#ifndef CONFIG_ISA_DMA_API
#define ALLOW_DMA	0
#else
#define ALLOW_DMA	1
#endif

#define DEBUGGING	1


#include <linux/module.h>
#include <linux/printk.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/gfp.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <linux/atomic.h>
#if ALLOW_DMA
#include <asm/dma.h>
#endif

#include "cs89x0.h"

static char version[] __initdata =
"cs89x0.c: v2.4.3-pre1 Russell Nelson <nelson@crynwr.com>, Andrew Morton\n";

#define DRV_NAME "cs89x0"

#if defined(CONFIG_MACH_IXDP2351)
#define CS89x0_NONISA_IRQ
static unsigned int netcard_portlist[] __used __initdata = {IXDP2351_VIRT_CS8900_BASE, 0};
static unsigned int cs8900_irq_map[] = {IRQ_IXDP2351_CS8900, 0, 0, 0};
#elif defined(CONFIG_ARCH_IXDP2X01)
#define CS89x0_NONISA_IRQ
static unsigned int netcard_portlist[] __used __initdata = {IXDP2X01_CS8900_VIRT_BASE, 0};
static unsigned int cs8900_irq_map[] = {IRQ_IXDP2X01_CS8900, 0, 0, 0};
#else
#ifndef CONFIG_CS89x0_PLATFORM
static unsigned int netcard_portlist[] __used __initdata =
   { 0x300, 0x320, 0x340, 0x360, 0x200, 0x220, 0x240, 0x260, 0x280, 0x2a0, 0x2c0, 0x2e0, 0};
static unsigned int cs8900_irq_map[] = {10,11,12,5};
#endif
#endif

#if DEBUGGING
static unsigned int net_debug = DEBUGGING;
#else
#define net_debug 0	
#endif

#define NETCARD_IO_EXTENT	16

#define FORCE_RJ45	0x0001    
#define FORCE_AUI	0x0002
#define FORCE_BNC	0x0004

#define FORCE_AUTO	0x0010    
#define FORCE_HALF	0x0020
#define FORCE_FULL	0x0030

struct net_local {
	int chip_type;		
	char chip_revision;	
	int send_cmd;		
	int auto_neg_cnf;	
	int adapter_cnf;	
	int isa_config;		
	int irq_map;		
	int rx_mode;		
	int curr_rx_cfg;	
	int linectl;		
	int send_underrun;	
	int force;		
	spinlock_t lock;
#if ALLOW_DMA
	int use_dma;		
	int dma;		
	int dmasize;		
	unsigned char *dma_buff;	
	unsigned char *end_dma_buff;	
	unsigned char *rx_dma_ptr;	
#endif
#ifdef CONFIG_CS89x0_PLATFORM
	void __iomem *virt_addr;
	unsigned long phys_addr;
	unsigned long size;	
#endif
};


static int cs89x0_probe1(struct net_device *dev, unsigned long ioaddr, int modular);
static int net_open(struct net_device *dev);
static netdev_tx_t net_send_packet(struct sk_buff *skb, struct net_device *dev);
static irqreturn_t net_interrupt(int irq, void *dev_id);
static void set_multicast_list(struct net_device *dev);
static void net_timeout(struct net_device *dev);
static void net_rx(struct net_device *dev);
static int net_close(struct net_device *dev);
static struct net_device_stats *net_get_stats(struct net_device *dev);
static void reset_chip(struct net_device *dev);
static int get_eeprom_data(struct net_device *dev, int off, int len, int *buffer);
static int get_eeprom_cksum(int off, int len, int *buffer);
static int set_mac_address(struct net_device *dev, void *addr);
static void count_rx_errors(int status, struct net_device *dev);
#ifdef CONFIG_NET_POLL_CONTROLLER
static void net_poll_controller(struct net_device *dev);
#endif
#if ALLOW_DMA
static void get_dma_channel(struct net_device *dev);
static void release_dma_buff(struct net_local *lp);
#endif

#define tx_done(dev) 1

#if !defined(MODULE) && (ALLOW_DMA != 0)
static int g_cs89x0_dma;

static int __init dma_fn(char *str)
{
	g_cs89x0_dma = simple_strtol(str,NULL,0);
	return 1;
}

__setup("cs89x0_dma=", dma_fn);
#endif	

#ifndef MODULE
static int g_cs89x0_media__force;

static int __init media_fn(char *str)
{
	if (!strcmp(str, "rj45")) g_cs89x0_media__force = FORCE_RJ45;
	else if (!strcmp(str, "aui")) g_cs89x0_media__force = FORCE_AUI;
	else if (!strcmp(str, "bnc")) g_cs89x0_media__force = FORCE_BNC;
	return 1;
}

__setup("cs89x0_media=", media_fn);


#ifndef CONFIG_CS89x0_PLATFORM

struct net_device * __init cs89x0_probe(int unit)
{
	struct net_device *dev = alloc_etherdev(sizeof(struct net_local));
	unsigned *port;
	int err = 0;
	int irq;
	int io;

	if (!dev)
		return ERR_PTR(-ENODEV);

	sprintf(dev->name, "eth%d", unit);
	netdev_boot_setup_check(dev);
	io = dev->base_addr;
	irq = dev->irq;

	if (net_debug)
		printk("cs89x0:cs89x0_probe(0x%x)\n", io);

	if (io > 0x1ff)	{	
		err = cs89x0_probe1(dev, io, 0);
	} else if (io != 0) {	
		err = -ENXIO;
	} else {
		for (port = netcard_portlist; *port; port++) {
			if (cs89x0_probe1(dev, *port, 0) == 0)
				break;
			dev->irq = irq;
		}
		if (!*port)
			err = -ENODEV;
	}
	if (err)
		goto out;
	return dev;
out:
	free_netdev(dev);
	printk(KERN_WARNING "cs89x0: no cs8900 or cs8920 detected.  Be sure to disable PnP with SETUP\n");
	return ERR_PTR(err);
}
#endif
#endif

#if defined(CONFIG_MACH_IXDP2351)
static u16
readword(unsigned long base_addr, int portno)
{
	return __raw_readw(base_addr + (portno << 1));
}

static void
writeword(unsigned long base_addr, int portno, u16 value)
{
	__raw_writew(value, base_addr + (portno << 1));
}
#elif defined(CONFIG_ARCH_IXDP2X01)
static u16
readword(unsigned long base_addr, int portno)
{
	return __raw_readl(base_addr + (portno << 1));
}

static void
writeword(unsigned long base_addr, int portno, u16 value)
{
	__raw_writel(value, base_addr + (portno << 1));
}
#else
static u16
readword(unsigned long base_addr, int portno)
{
	return inw(base_addr + portno);
}

static void
writeword(unsigned long base_addr, int portno, u16 value)
{
	outw(value, base_addr + portno);
}
#endif

static void
readwords(unsigned long base_addr, int portno, void *buf, int length)
{
	u8 *buf8 = (u8 *)buf;

	do {
		u16 tmp16;

		tmp16 = readword(base_addr, portno);
		*buf8++ = (u8)tmp16;
		*buf8++ = (u8)(tmp16 >> 8);
	} while (--length);
}

static void
writewords(unsigned long base_addr, int portno, void *buf, int length)
{
	u8 *buf8 = (u8 *)buf;

	do {
		u16 tmp16;

		tmp16 = *buf8++;
		tmp16 |= (*buf8++) << 8;
		writeword(base_addr, portno, tmp16);
	} while (--length);
}

static u16
readreg(struct net_device *dev, u16 regno)
{
	writeword(dev->base_addr, ADD_PORT, regno);
	return readword(dev->base_addr, DATA_PORT);
}

static void
writereg(struct net_device *dev, u16 regno, u16 value)
{
	writeword(dev->base_addr, ADD_PORT, regno);
	writeword(dev->base_addr, DATA_PORT, value);
}

static int __init
wait_eeprom_ready(struct net_device *dev)
{
	int timeout = jiffies;
	while(readreg(dev, PP_SelfST) & SI_BUSY)
		if (jiffies - timeout >= 40)
			return -1;
	return 0;
}

static int __init
get_eeprom_data(struct net_device *dev, int off, int len, int *buffer)
{
	int i;

	if (net_debug > 3) printk("EEPROM data from %x for %x:\n",off,len);
	for (i = 0; i < len; i++) {
		if (wait_eeprom_ready(dev) < 0) return -1;
		
		writereg(dev, PP_EECMD, (off + i) | EEPROM_READ_CMD);
		if (wait_eeprom_ready(dev) < 0) return -1;
		buffer[i] = readreg(dev, PP_EEData);
		if (net_debug > 3) printk("%04x ", buffer[i]);
	}
	if (net_debug > 3) printk("\n");
        return 0;
}

static int  __init
get_eeprom_cksum(int off, int len, int *buffer)
{
	int i, cksum;

	cksum = 0;
	for (i = 0; i < len; i++)
		cksum += buffer[i];
	cksum &= 0xffff;
	if (cksum == 0)
		return 0;
	return -1;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void net_poll_controller(struct net_device *dev)
{
	disable_irq(dev->irq);
	net_interrupt(dev->irq, dev);
	enable_irq(dev->irq);
}
#endif

static const struct net_device_ops net_ops = {
	.ndo_open		= net_open,
	.ndo_stop		= net_close,
	.ndo_tx_timeout		= net_timeout,
	.ndo_start_xmit 	= net_send_packet,
	.ndo_get_stats		= net_get_stats,
	.ndo_set_rx_mode	= set_multicast_list,
	.ndo_set_mac_address 	= set_mac_address,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= net_poll_controller,
#endif
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_validate_addr	= eth_validate_addr,
};


static int __init
cs89x0_probe1(struct net_device *dev, unsigned long ioaddr, int modular)
{
	struct net_local *lp = netdev_priv(dev);
	static unsigned version_printed;
	int i;
	int tmp;
	unsigned rev_type = 0;
	int eeprom_buff[CHKSUM_LEN];
	int retval;

	
	if (!modular) {
		memset(lp, 0, sizeof(*lp));
		spin_lock_init(&lp->lock);
#ifndef MODULE
#if ALLOW_DMA
		if (g_cs89x0_dma) {
			lp->use_dma = 1;
			lp->dma = g_cs89x0_dma;
			lp->dmasize = 16;	
		}
#endif
		lp->force = g_cs89x0_media__force;
#endif

        }

	
	
	if (!request_region(ioaddr & ~3, NETCARD_IO_EXTENT, DRV_NAME)) {
		printk(KERN_ERR "%s: request_region(0x%lx, 0x%x) failed\n",
				DRV_NAME, ioaddr, NETCARD_IO_EXTENT);
		retval = -EBUSY;
		goto out1;
	}

	if (ioaddr & 1) {
		if (net_debug > 1)
			printk(KERN_INFO "%s: odd ioaddr 0x%lx\n", dev->name, ioaddr);
	        if ((ioaddr & 2) != 2)
	        	if ((readword(ioaddr & ~3, ADD_PORT) & ADD_MASK) != ADD_SIG) {
				printk(KERN_ERR "%s: bad signature 0x%x\n",
					dev->name, readword(ioaddr & ~3, ADD_PORT));
		        	retval = -ENODEV;
				goto out2;
			}
	}

	ioaddr &= ~3;
	printk(KERN_DEBUG "PP_addr at %lx[%x]: 0x%x\n",
			ioaddr, ADD_PORT, readword(ioaddr, ADD_PORT));
	writeword(ioaddr, ADD_PORT, PP_ChipID);

	tmp = readword(ioaddr, DATA_PORT);
	if (tmp != CHIP_EISA_ID_SIG) {
		printk(KERN_DEBUG "%s: incorrect signature at %lx[%x]: 0x%x!="
			CHIP_EISA_ID_SIG_STR "\n",
			dev->name, ioaddr, DATA_PORT, tmp);
  		retval = -ENODEV;
  		goto out2;
	}

	
	dev->base_addr = ioaddr;

	
	rev_type = readreg(dev, PRODUCT_ID_ADD);
	lp->chip_type = rev_type &~ REVISON_BITS;
	lp->chip_revision = ((rev_type & REVISON_BITS) >> 8) + 'A';

	lp->send_cmd = TX_AFTER_381;
	if (lp->chip_type == CS8900 && lp->chip_revision >= 'F')
		lp->send_cmd = TX_NOW;
	if (lp->chip_type != CS8900 && lp->chip_revision >= 'C')
		lp->send_cmd = TX_NOW;

	if (net_debug  &&  version_printed++ == 0)
		printk(version);

	printk(KERN_INFO "%s: cs89%c0%s rev %c found at %#3lx ",
	       dev->name,
	       lp->chip_type==CS8900?'0':'2',
	       lp->chip_type==CS8920M?"M":"",
	       lp->chip_revision,
	       dev->base_addr);

	reset_chip(dev);



        if ((readreg(dev, PP_SelfST) & (EEPROM_OK | EEPROM_PRESENT)) ==
	      (EEPROM_OK|EEPROM_PRESENT)) {
	        
		for (i=0; i < ETH_ALEN/2; i++) {
	                unsigned int Addr;
			Addr = readreg(dev, PP_IA+i*2);
		        dev->dev_addr[i*2] = Addr & 0xFF;
		        dev->dev_addr[i*2+1] = Addr >> 8;
		}


		lp->adapter_cnf = 0;
		i = readreg(dev, PP_LineCTL);
		
		if ((i & (HCB1 | HCB1_ENBL)) ==  (HCB1 | HCB1_ENBL))
			lp->adapter_cnf |= A_CNF_DC_DC_POLARITY;
		
		if ((i & LOW_RX_SQUELCH) == LOW_RX_SQUELCH)
			lp->adapter_cnf |= A_CNF_EXTND_10B_2 | A_CNF_LOW_RX_SQUELCH;
		
		if ((i & (AUI_ONLY | AUTO_AUI_10BASET)) == 0)
			lp->adapter_cnf |=  A_CNF_10B_T | A_CNF_MEDIA_10B_T;
		
		if ((i & (AUI_ONLY | AUTO_AUI_10BASET)) == AUI_ONLY)
			lp->adapter_cnf |=  A_CNF_AUI | A_CNF_MEDIA_AUI;
		
		if ((i & (AUI_ONLY | AUTO_AUI_10BASET)) == AUTO_AUI_10BASET)
			lp->adapter_cnf |=  A_CNF_AUI | A_CNF_10B_T |
			A_CNF_MEDIA_AUI | A_CNF_MEDIA_10B_T | A_CNF_MEDIA_AUTO;

		if (net_debug > 1)
			printk(KERN_INFO "%s: PP_LineCTL=0x%x, adapter_cnf=0x%x\n",
					dev->name, i, lp->adapter_cnf);

		
		if (lp->chip_type == CS8900)
			lp->isa_config = readreg(dev, PP_CS8900_ISAINT) & INT_NO_MASK;

		printk( "[Cirrus EEPROM] ");
	}

        printk("\n");

	

	if ((readreg(dev, PP_SelfST) & EEPROM_PRESENT) == 0)
		printk(KERN_WARNING "cs89x0: No EEPROM, relying on command line....\n");
	else if (get_eeprom_data(dev, START_EEPROM_DATA,CHKSUM_LEN,eeprom_buff) < 0) {
		printk(KERN_WARNING "\ncs89x0: EEPROM read failed, relying on command line.\n");
        } else if (get_eeprom_cksum(START_EEPROM_DATA,CHKSUM_LEN,eeprom_buff) < 0) {
		if ((readreg(dev, PP_SelfST) & (EEPROM_OK | EEPROM_PRESENT)) !=
		    (EEPROM_OK|EEPROM_PRESENT))
                	printk(KERN_WARNING "cs89x0: Extended EEPROM checksum bad and no Cirrus EEPROM, relying on command line\n");

        } else {

                
                if (!lp->auto_neg_cnf) lp->auto_neg_cnf = eeprom_buff[AUTO_NEG_CNF_OFFSET/2];
                
                if (!lp->adapter_cnf) lp->adapter_cnf = eeprom_buff[ADAPTER_CNF_OFFSET/2];
                
                lp->isa_config = eeprom_buff[ISA_CNF_OFFSET/2];
                dev->mem_start = eeprom_buff[PACKET_PAGE_OFFSET/2] << 8;

                
                
                for (i = 0; i < ETH_ALEN/2; i++) {
                        dev->dev_addr[i*2] = eeprom_buff[i];
                        dev->dev_addr[i*2+1] = eeprom_buff[i] >> 8;
                }
		if (net_debug > 1)
			printk(KERN_DEBUG "%s: new adapter_cnf: 0x%x\n",
				dev->name, lp->adapter_cnf);
        }

        
        {
		int count = 0;
		if (lp->force & FORCE_RJ45)	{lp->adapter_cnf |= A_CNF_10B_T; count++; }
		if (lp->force & FORCE_AUI) 	{lp->adapter_cnf |= A_CNF_AUI; count++; }
		if (lp->force & FORCE_BNC)	{lp->adapter_cnf |= A_CNF_10B_2; count++; }
		if (count > 1)			{lp->adapter_cnf |= A_CNF_MEDIA_AUTO; }
		else if (lp->force & FORCE_RJ45){lp->adapter_cnf |= A_CNF_MEDIA_10B_T; }
		else if (lp->force & FORCE_AUI)	{lp->adapter_cnf |= A_CNF_MEDIA_AUI; }
		else if (lp->force & FORCE_BNC)	{lp->adapter_cnf |= A_CNF_MEDIA_10B_2; }
        }

	if (net_debug > 1)
		printk(KERN_DEBUG "%s: after force 0x%x, adapter_cnf=0x%x\n",
			dev->name, lp->force, lp->adapter_cnf);

        

        


	printk(KERN_INFO "cs89x0 media %s%s%s",
	       (lp->adapter_cnf & A_CNF_10B_T)?"RJ-45,":"",
	       (lp->adapter_cnf & A_CNF_AUI)?"AUI,":"",
	       (lp->adapter_cnf & A_CNF_10B_2)?"BNC,":"");

	lp->irq_map = 0xffff;

	
	if (lp->chip_type != CS8900 &&
	    
		(i = readreg(dev, PP_CS8920_ISAINT) & 0xff,
		 (i != 0 && i < CS8920_NO_INTS))) {
		if (!dev->irq)
			dev->irq = i;
	} else {
		i = lp->isa_config & INT_NO_MASK;
#ifndef CONFIG_CS89x0_PLATFORM
		if (lp->chip_type == CS8900) {
#ifdef CS89x0_NONISA_IRQ
		        i = cs8900_irq_map[0];
#else
			
			if (i >= ARRAY_SIZE(cs8900_irq_map))
				printk("\ncs89x0: invalid ISA interrupt number %d\n", i);
			else
				i = cs8900_irq_map[i];

			lp->irq_map = CS8900_IRQ_MAP; 
		} else {
			int irq_map_buff[IRQ_MAP_LEN/2];

			if (get_eeprom_data(dev, IRQ_MAP_EEPROM_DATA,
					    IRQ_MAP_LEN/2,
					    irq_map_buff) >= 0) {
				if ((irq_map_buff[0] & 0xff) == PNP_IRQ_FRMT)
					lp->irq_map = (irq_map_buff[0]>>8) | (irq_map_buff[1] << 8);
			}
#endif
		}
#endif
		if (!dev->irq)
			dev->irq = i;
	}

	printk(" IRQ %d", dev->irq);

#if ALLOW_DMA
	if (lp->use_dma) {
		get_dma_channel(dev);
		printk(", DMA %d", dev->dma);
	}
	else
#endif
	{
		printk(", programmed I/O");
	}

	
	printk(", MAC %pM", dev->dev_addr);

	dev->netdev_ops	= &net_ops;
	dev->watchdog_timeo = HZ;

	printk("\n");
	if (net_debug)
		printk("cs89x0_probe1() successful\n");

	retval = register_netdev(dev);
	if (retval)
		goto out3;
	return 0;
out3:
	writeword(dev->base_addr, ADD_PORT, PP_ChipID);
out2:
	release_region(ioaddr & ~3, NETCARD_IO_EXTENT);
out1:
	return retval;
}



#if ALLOW_DMA

#define dma_page_eq(ptr1, ptr2) ((long)(ptr1)>>17 == (long)(ptr2)>>17)

static void
get_dma_channel(struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);

	if (lp->dma) {
		dev->dma = lp->dma;
		lp->isa_config |= ISA_RxDMA;
	} else {
		if ((lp->isa_config & ANY_ISA_DMA) == 0)
			return;
		dev->dma = lp->isa_config & DMA_NO_MASK;
		if (lp->chip_type == CS8900)
			dev->dma += 5;
		if (dev->dma < 5 || dev->dma > 7) {
			lp->isa_config &= ~ANY_ISA_DMA;
			return;
		}
	}
}

static void
write_dma(struct net_device *dev, int chip_type, int dma)
{
	struct net_local *lp = netdev_priv(dev);
	if ((lp->isa_config & ANY_ISA_DMA) == 0)
		return;
	if (chip_type == CS8900) {
		writereg(dev, PP_CS8900_ISADMA, dma-5);
	} else {
		writereg(dev, PP_CS8920_ISADMA, dma);
	}
}

static void
set_dma_cfg(struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);

	if (lp->use_dma) {
		if ((lp->isa_config & ANY_ISA_DMA) == 0) {
			if (net_debug > 3)
				printk("set_dma_cfg(): no DMA\n");
			return;
		}
		if (lp->isa_config & ISA_RxDMA) {
			lp->curr_rx_cfg |= RX_DMA_ONLY;
			if (net_debug > 3)
				printk("set_dma_cfg(): RX_DMA_ONLY\n");
		} else {
			lp->curr_rx_cfg |= AUTO_RX_DMA;	
			if (net_debug > 3)
				printk("set_dma_cfg(): AUTO_RX_DMA\n");
		}
	}
}

static int
dma_bufcfg(struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);
	if (lp->use_dma)
		return (lp->isa_config & ANY_ISA_DMA)? RX_DMA_ENBL : 0;
	else
		return 0;
}

static int
dma_busctl(struct net_device *dev)
{
	int retval = 0;
	struct net_local *lp = netdev_priv(dev);
	if (lp->use_dma) {
		if (lp->isa_config & ANY_ISA_DMA)
			retval |= RESET_RX_DMA; 
		if (lp->isa_config & DMA_BURST)
			retval |= DMA_BURST_MODE; 
		if (lp->dmasize == 64)
			retval |= RX_DMA_SIZE_64K; 
		retval |= MEMORY_ON;	
	}
	return retval;
}

static void
dma_rx(struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);
	struct sk_buff *skb;
	int status, length;
	unsigned char *bp = lp->rx_dma_ptr;

	status = bp[0] + (bp[1]<<8);
	length = bp[2] + (bp[3]<<8);
	bp += 4;
	if (net_debug > 5) {
		printk(	"%s: receiving DMA packet at %lx, status %x, length %x\n",
			dev->name, (unsigned long)bp, status, length);
	}
	if ((status & RX_OK) == 0) {
		count_rx_errors(status, dev);
		goto skip_this_frame;
	}

	
	skb = netdev_alloc_skb(dev, length + 2);
	if (skb == NULL) {
		if (net_debug)	
			printk("%s: Memory squeeze, dropping packet.\n", dev->name);
		dev->stats.rx_dropped++;

		
skip_this_frame:
		bp += (length + 3) & ~3;
		if (bp >= lp->end_dma_buff) bp -= lp->dmasize*1024;
		lp->rx_dma_ptr = bp;
		return;
	}
	skb_reserve(skb, 2);	

	if (bp + length > lp->end_dma_buff) {
		int semi_cnt = lp->end_dma_buff - bp;
		memcpy(skb_put(skb,semi_cnt), bp, semi_cnt);
		memcpy(skb_put(skb,length - semi_cnt), lp->dma_buff,
		       length - semi_cnt);
	} else {
		memcpy(skb_put(skb,length), bp, length);
	}
	bp += (length + 3) & ~3;
	if (bp >= lp->end_dma_buff) bp -= lp->dmasize*1024;
	lp->rx_dma_ptr = bp;

	if (net_debug > 3) {
		printk(	"%s: received %d byte DMA packet of type %x\n",
			dev->name, length,
			(skb->data[ETH_ALEN+ETH_ALEN] << 8) | skb->data[ETH_ALEN+ETH_ALEN+1]);
	}
        skb->protocol=eth_type_trans(skb,dev);
	netif_rx(skb);
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += length;
}

#endif	

static void __init reset_chip(struct net_device *dev)
{
#if !defined(CONFIG_MACH_MX31ADS)
#if !defined(CS89x0_NONISA_IRQ)
	struct net_local *lp = netdev_priv(dev);
	int ioaddr = dev->base_addr;
#endif 
	int reset_start_time;

	writereg(dev, PP_SelfCTL, readreg(dev, PP_SelfCTL) | POWER_ON_RESET);

	
	msleep(30);

#if !defined(CS89x0_NONISA_IRQ)
	if (lp->chip_type != CS8900) {
		
		writeword(ioaddr, ADD_PORT, PP_CS8920_ISAINT);
		outb(dev->irq, ioaddr + DATA_PORT);
		outb(0,      ioaddr + DATA_PORT + 1);

		writeword(ioaddr, ADD_PORT, PP_CS8920_ISAMemB);
		outb((dev->mem_start >> 16) & 0xff, ioaddr + DATA_PORT);
		outb((dev->mem_start >> 8) & 0xff,   ioaddr + DATA_PORT + 1);
	}
#endif 

	
	reset_start_time = jiffies;
	while( (readreg(dev, PP_SelfST) & INIT_DONE) == 0 && jiffies - reset_start_time < 2)
		;
#endif 
}


static void
control_dc_dc(struct net_device *dev, int on_not_off)
{
	struct net_local *lp = netdev_priv(dev);
	unsigned int selfcontrol;
	int timenow = jiffies;

	selfcontrol = HCB1_ENBL; 
	if (((lp->adapter_cnf & A_CNF_DC_DC_POLARITY) != 0) ^ on_not_off)
		selfcontrol |= HCB1;
	else
		selfcontrol &= ~HCB1;
	writereg(dev, PP_SelfCTL, selfcontrol);

	
	while (jiffies - timenow < HZ)
		;
}

#define DETECTED_NONE  0
#define DETECTED_RJ45H 1
#define DETECTED_RJ45F 2
#define DETECTED_AUI   3
#define DETECTED_BNC   4

static int
detect_tp(struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);
	int timenow = jiffies;
	int fdx;

	if (net_debug > 1) printk("%s: Attempting TP\n", dev->name);

	writereg(dev, PP_LineCTL, lp->linectl &~ AUI_ONLY);
	control_dc_dc(dev, 0);

        
	for (timenow = jiffies; jiffies - timenow < 15; )
                ;
	if ((readreg(dev, PP_LineST) & LINK_OK) == 0)
		return DETECTED_NONE;

	if (lp->chip_type == CS8900) {
                switch (lp->force & 0xf0) {
#if 0
                case FORCE_AUTO:
			printk("%s: cs8900 doesn't autonegotiate\n",dev->name);
                        return DETECTED_NONE;
#endif
		
                case FORCE_AUTO:
			lp->force &= ~FORCE_AUTO;
                        lp->force |= FORCE_HALF;
			break;
		case FORCE_HALF:
			break;
                case FORCE_FULL:
			writereg(dev, PP_TestCTL, readreg(dev, PP_TestCTL) | FDX_8900);
			break;
                }
		fdx = readreg(dev, PP_TestCTL) & FDX_8900;
	} else {
		switch (lp->force & 0xf0) {
		case FORCE_AUTO:
			lp->auto_neg_cnf = AUTO_NEG_ENABLE;
			break;
		case FORCE_HALF:
			lp->auto_neg_cnf = 0;
			break;
		case FORCE_FULL:
			lp->auto_neg_cnf = RE_NEG_NOW | ALLOW_FDX;
			break;
                }

		writereg(dev, PP_AutoNegCTL, lp->auto_neg_cnf & AUTO_NEG_MASK);

		if ((lp->auto_neg_cnf & AUTO_NEG_BITS) == AUTO_NEG_ENABLE) {
			printk(KERN_INFO "%s: negotiating duplex...\n",dev->name);
			while (readreg(dev, PP_AutoNegST) & AUTO_NEG_BUSY) {
				if (jiffies - timenow > 4000) {
					printk(KERN_ERR "**** Full / half duplex auto-negotiation timed out ****\n");
					break;
				}
			}
		}
		fdx = readreg(dev, PP_AutoNegST) & FDX_ACTIVE;
	}
	if (fdx)
		return DETECTED_RJ45F;
	else
		return DETECTED_RJ45H;
}

static int
send_test_pkt(struct net_device *dev)
{
	char test_packet[] = { 0,0,0,0,0,0, 0,0,0,0,0,0,
				 0, 46, 
				 0, 0, 
				 0xf3, 0  };
	long timenow = jiffies;

	writereg(dev, PP_LineCTL, readreg(dev, PP_LineCTL) | SERIAL_TX_ON);

	memcpy(test_packet,          dev->dev_addr, ETH_ALEN);
	memcpy(test_packet+ETH_ALEN, dev->dev_addr, ETH_ALEN);

        writeword(dev->base_addr, TX_CMD_PORT, TX_AFTER_ALL);
        writeword(dev->base_addr, TX_LEN_PORT, ETH_ZLEN);

	
	while (jiffies - timenow < 5)
		if (readreg(dev, PP_BusST) & READY_FOR_TX_NOW)
			break;
	if (jiffies - timenow >= 5)
		return 0;	

	
	writewords(dev->base_addr, TX_FRAME_PORT,test_packet,(ETH_ZLEN+1) >>1);

	if (net_debug > 1) printk("Sending test packet ");
	
	for (timenow = jiffies; jiffies - timenow < 3; )
                ;
        if ((readreg(dev, PP_TxEvent) & TX_SEND_OK_BITS) == TX_OK) {
                if (net_debug > 1) printk("succeeded\n");
                return 1;
        }
	if (net_debug > 1) printk("failed\n");
	return 0;
}


static int
detect_aui(struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);

	if (net_debug > 1) printk("%s: Attempting AUI\n", dev->name);
	control_dc_dc(dev, 0);

	writereg(dev, PP_LineCTL, (lp->linectl &~ AUTO_AUI_10BASET) | AUI_ONLY);

	if (send_test_pkt(dev))
		return DETECTED_AUI;
	else
		return DETECTED_NONE;
}

static int
detect_bnc(struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);

	if (net_debug > 1) printk("%s: Attempting BNC\n", dev->name);
	control_dc_dc(dev, 1);

	writereg(dev, PP_LineCTL, (lp->linectl &~ AUTO_AUI_10BASET) | AUI_ONLY);

	if (send_test_pkt(dev))
		return DETECTED_BNC;
	else
		return DETECTED_NONE;
}


static void
write_irq(struct net_device *dev, int chip_type, int irq)
{
	int i;

	if (chip_type == CS8900) {
#ifndef CONFIG_CS89x0_PLATFORM
		
		for (i = 0; i != ARRAY_SIZE(cs8900_irq_map); i++)
			if (cs8900_irq_map[i] == irq)
				break;
		
		if (i == ARRAY_SIZE(cs8900_irq_map))
			i = 3;
#else
		
		i = 0;
#endif
		writereg(dev, PP_CS8900_ISAINT, i);
	} else {
		writereg(dev, PP_CS8920_ISAINT, irq);
	}
}



static int
net_open(struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);
	int result = 0;
	int i;
	int ret;

	if (dev->irq < 2) {
		
#if 0
		writereg(dev, PP_BusCTL, readreg(dev, PP_BusCTL)|ENABLE_IRQ );
#endif
		writereg(dev, PP_BusCTL, ENABLE_IRQ | MEMORY_ON);

		for (i = 2; i < CS8920_NO_INTS; i++) {
			if ((1 << i) & lp->irq_map) {
				if (request_irq(i, net_interrupt, 0, dev->name, dev) == 0) {
					dev->irq = i;
					write_irq(dev, lp->chip_type, i);
					
					break;
				}
			}
		}

		if (i >= CS8920_NO_INTS) {
			writereg(dev, PP_BusCTL, 0);	
			printk(KERN_ERR "cs89x0: can't get an interrupt\n");
			ret = -EAGAIN;
			goto bad_out;
		}
	}
	else
	{
#if !defined(CS89x0_NONISA_IRQ) && !defined(CONFIG_CS89x0_PLATFORM)
		if (((1 << dev->irq) & lp->irq_map) == 0) {
			printk(KERN_ERR "%s: IRQ %d is not in our map of allowable IRQs, which is %x\n",
                               dev->name, dev->irq, lp->irq_map);
			ret = -EAGAIN;
			goto bad_out;
		}
#endif
		writereg(dev, PP_BusCTL, readreg(dev, PP_BusCTL)|ENABLE_IRQ );
#if 0
		writereg(dev, PP_BusCTL, ENABLE_IRQ | MEMORY_ON);
#endif
		write_irq(dev, lp->chip_type, dev->irq);
		ret = request_irq(dev->irq, net_interrupt, 0, dev->name, dev);
		if (ret) {
			printk(KERN_ERR "cs89x0: request_irq(%d) failed\n", dev->irq);
			goto bad_out;
		}
	}

#if ALLOW_DMA
	if (lp->use_dma) {
		if (lp->isa_config & ANY_ISA_DMA) {
			unsigned long flags;
			lp->dma_buff = (unsigned char *)__get_dma_pages(GFP_KERNEL,
							get_order(lp->dmasize * 1024));

			if (!lp->dma_buff) {
				printk(KERN_ERR "%s: cannot get %dK memory for DMA\n", dev->name, lp->dmasize);
				goto release_irq;
			}
			if (net_debug > 1) {
				printk(	"%s: dma %lx %lx\n",
					dev->name,
					(unsigned long)lp->dma_buff,
					(unsigned long)isa_virt_to_bus(lp->dma_buff));
			}
			if ((unsigned long) lp->dma_buff >= MAX_DMA_ADDRESS ||
			    !dma_page_eq(lp->dma_buff, lp->dma_buff+lp->dmasize*1024-1)) {
				printk(KERN_ERR "%s: not usable as DMA buffer\n", dev->name);
				goto release_irq;
			}
			memset(lp->dma_buff, 0, lp->dmasize * 1024);	
			if (request_dma(dev->dma, dev->name)) {
				printk(KERN_ERR "%s: cannot get dma channel %d\n", dev->name, dev->dma);
				goto release_irq;
			}
			write_dma(dev, lp->chip_type, dev->dma);
			lp->rx_dma_ptr = lp->dma_buff;
			lp->end_dma_buff = lp->dma_buff + lp->dmasize*1024;
			spin_lock_irqsave(&lp->lock, flags);
			disable_dma(dev->dma);
			clear_dma_ff(dev->dma);
			set_dma_mode(dev->dma, DMA_RX_MODE); 
			set_dma_addr(dev->dma, isa_virt_to_bus(lp->dma_buff));
			set_dma_count(dev->dma, lp->dmasize*1024);
			enable_dma(dev->dma);
			spin_unlock_irqrestore(&lp->lock, flags);
		}
	}
#endif	

	
	for (i=0; i < ETH_ALEN/2; i++)
		writereg(dev, PP_IA+i*2, dev->dev_addr[i*2] | (dev->dev_addr[i*2+1] << 8));

	
	writereg(dev, PP_BusCTL, MEMORY_ON);

	
	if ((lp->adapter_cnf & A_CNF_EXTND_10B_2) && (lp->adapter_cnf & A_CNF_LOW_RX_SQUELCH))
                lp->linectl = LOW_RX_SQUELCH;
	else
                lp->linectl = 0;

        
	switch(lp->adapter_cnf & A_CNF_MEDIA_TYPE) {
	case A_CNF_MEDIA_10B_T: result = lp->adapter_cnf & A_CNF_10B_T; break;
	case A_CNF_MEDIA_AUI:   result = lp->adapter_cnf & A_CNF_AUI; break;
	case A_CNF_MEDIA_10B_2: result = lp->adapter_cnf & A_CNF_10B_2; break;
        default: result = lp->adapter_cnf & (A_CNF_10B_T | A_CNF_AUI | A_CNF_10B_2);
        }
        if (!result) {
                printk(KERN_ERR "%s: EEPROM is configured for unavailable media\n", dev->name);
release_dma:
#if ALLOW_DMA
		free_dma(dev->dma);
release_irq:
		release_dma_buff(lp);
#endif
                writereg(dev, PP_LineCTL, readreg(dev, PP_LineCTL) & ~(SERIAL_TX_ON | SERIAL_RX_ON));
                free_irq(dev->irq, dev);
		ret = -EAGAIN;
		goto bad_out;
	}

        
	switch(lp->adapter_cnf & A_CNF_MEDIA_TYPE) {
	case A_CNF_MEDIA_10B_T:
                result = detect_tp(dev);
                if (result==DETECTED_NONE) {
                        printk(KERN_WARNING "%s: 10Base-T (RJ-45) has no cable\n", dev->name);
                        if (lp->auto_neg_cnf & IMM_BIT) 
                                result = DETECTED_RJ45H; 
                }
		break;
	case A_CNF_MEDIA_AUI:
                result = detect_aui(dev);
                if (result==DETECTED_NONE) {
                        printk(KERN_WARNING "%s: 10Base-5 (AUI) has no cable\n", dev->name);
                        if (lp->auto_neg_cnf & IMM_BIT) 
                                result = DETECTED_AUI; 
                }
		break;
	case A_CNF_MEDIA_10B_2:
                result = detect_bnc(dev);
                if (result==DETECTED_NONE) {
                        printk(KERN_WARNING "%s: 10Base-2 (BNC) has no cable\n", dev->name);
                        if (lp->auto_neg_cnf & IMM_BIT) 
                                result = DETECTED_BNC; 
                }
		break;
	case A_CNF_MEDIA_AUTO:
		writereg(dev, PP_LineCTL, lp->linectl | AUTO_AUI_10BASET);
		if (lp->adapter_cnf & A_CNF_10B_T)
			if ((result = detect_tp(dev)) != DETECTED_NONE)
				break;
		if (lp->adapter_cnf & A_CNF_AUI)
			if ((result = detect_aui(dev)) != DETECTED_NONE)
				break;
		if (lp->adapter_cnf & A_CNF_10B_2)
			if ((result = detect_bnc(dev)) != DETECTED_NONE)
				break;
		printk(KERN_ERR "%s: no media detected\n", dev->name);
		goto release_dma;
	}
	switch(result) {
	case DETECTED_NONE:
		printk(KERN_ERR "%s: no network cable attached to configured media\n", dev->name);
		goto release_dma;
	case DETECTED_RJ45H:
		printk(KERN_INFO "%s: using half-duplex 10Base-T (RJ-45)\n", dev->name);
		break;
	case DETECTED_RJ45F:
		printk(KERN_INFO "%s: using full-duplex 10Base-T (RJ-45)\n", dev->name);
		break;
	case DETECTED_AUI:
		printk(KERN_INFO "%s: using 10Base-5 (AUI)\n", dev->name);
		break;
	case DETECTED_BNC:
		printk(KERN_INFO "%s: using 10Base-2 (BNC)\n", dev->name);
		break;
	}

	
	writereg(dev, PP_LineCTL, readreg(dev, PP_LineCTL) | SERIAL_RX_ON | SERIAL_TX_ON);

	
	lp->rx_mode = 0;
	writereg(dev, PP_RxCTL, DEF_RX_ACCEPT);

	lp->curr_rx_cfg = RX_OK_ENBL | RX_CRC_ERROR_ENBL;

	if (lp->isa_config & STREAM_TRANSFER)
		lp->curr_rx_cfg |= RX_STREAM_ENBL;
#if ALLOW_DMA
	set_dma_cfg(dev);
#endif
	writereg(dev, PP_RxCFG, lp->curr_rx_cfg);

	writereg(dev, PP_TxCFG, TX_LOST_CRS_ENBL | TX_SQE_ERROR_ENBL | TX_OK_ENBL |
		TX_LATE_COL_ENBL | TX_JBR_ENBL | TX_ANY_COL_ENBL | TX_16_COL_ENBL);

	writereg(dev, PP_BufCFG, READY_FOR_TX_ENBL | RX_MISS_COUNT_OVRFLOW_ENBL |
#if ALLOW_DMA
		dma_bufcfg(dev) |
#endif
		TX_COL_COUNT_OVRFLOW_ENBL | TX_UNDERRUN_ENBL);

	
	writereg(dev, PP_BusCTL, ENABLE_IRQ
		 | (dev->mem_start?MEMORY_ON : 0) 
#if ALLOW_DMA
		 | dma_busctl(dev)
#endif
                 );
        netif_start_queue(dev);
	if (net_debug > 1)
		printk("cs89x0: net_open() succeeded\n");
	return 0;
bad_out:
	return ret;
}

static void net_timeout(struct net_device *dev)
{
	if (net_debug > 0) printk("%s: transmit timed out, %s?\n", dev->name,
		   tx_done(dev) ? "IRQ conflict ?" : "network cable problem");
	
	netif_wake_queue(dev);
}

static netdev_tx_t net_send_packet(struct sk_buff *skb,struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);
	unsigned long flags;

	if (net_debug > 3) {
		printk("%s: sent %d byte packet of type %x\n",
			dev->name, skb->len,
			(skb->data[ETH_ALEN+ETH_ALEN] << 8) | skb->data[ETH_ALEN+ETH_ALEN+1]);
	}


	spin_lock_irqsave(&lp->lock, flags);
	netif_stop_queue(dev);

	
	writeword(dev->base_addr, TX_CMD_PORT, lp->send_cmd);
	writeword(dev->base_addr, TX_LEN_PORT, skb->len);

	
	if ((readreg(dev, PP_BusST) & READY_FOR_TX_NOW) == 0) {

		spin_unlock_irqrestore(&lp->lock, flags);
		if (net_debug) printk("cs89x0: Tx buffer not free!\n");
		return NETDEV_TX_BUSY;
	}
	
	writewords(dev->base_addr, TX_FRAME_PORT,skb->data,(skb->len+1) >>1);
	spin_unlock_irqrestore(&lp->lock, flags);
	dev->stats.tx_bytes += skb->len;
	dev_kfree_skb (skb);


	return NETDEV_TX_OK;
}


static irqreturn_t net_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct net_local *lp;
	int ioaddr, status;
 	int handled = 0;

	ioaddr = dev->base_addr;
	lp = netdev_priv(dev);

	while ((status = readword(dev->base_addr, ISQ_PORT))) {
		if (net_debug > 4)printk("%s: event=%04x\n", dev->name, status);
		handled = 1;
		switch(status & ISQ_EVENT_MASK) {
		case ISQ_RECEIVER_EVENT:
			
			net_rx(dev);
			break;
		case ISQ_TRANSMITTER_EVENT:
			dev->stats.tx_packets++;
			netif_wake_queue(dev);	
			if ((status & (	TX_OK |
					TX_LOST_CRS |
					TX_SQE_ERROR |
					TX_LATE_COL |
					TX_16_COL)) != TX_OK) {
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
			}
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
				netif_wake_queue(dev);	
                        }
#if ALLOW_DMA
			if (lp->use_dma && (status & RX_DMA)) {
				int count = readreg(dev, PP_DmaFrameCnt);
				while(count) {
					if (net_debug > 5)
						printk("%s: receiving %d DMA frames\n", dev->name, count);
					if (net_debug > 2 && count >1)
						printk("%s: receiving %d DMA frames\n", dev->name, count);
					dma_rx(dev);
					if (--count == 0)
						count = readreg(dev, PP_DmaFrameCnt);
					if (net_debug > 2 && count > 0)
						printk("%s: continuing with %d DMA frames\n", dev->name, count);
				}
			}
#endif
			break;
		case ISQ_RX_MISS_EVENT:
			dev->stats.rx_missed_errors += (status >> 6);
			break;
		case ISQ_TX_COL_EVENT:
			dev->stats.collisions += (status >> 6);
			break;
		}
	}
	return IRQ_RETVAL(handled);
}

static void
count_rx_errors(int status, struct net_device *dev)
{
	dev->stats.rx_errors++;
	if (status & RX_RUNT)
		dev->stats.rx_length_errors++;
	if (status & RX_EXTRA_DATA)
		dev->stats.rx_length_errors++;
	if ((status & RX_CRC_ERROR) && !(status & (RX_EXTRA_DATA|RX_RUNT)))
		
		dev->stats.rx_crc_errors++;
	if (status & RX_DRIBBLE)
		dev->stats.rx_frame_errors++;
}

static void
net_rx(struct net_device *dev)
{
	struct sk_buff *skb;
	int status, length;

	int ioaddr = dev->base_addr;
	status = readword(ioaddr, RX_FRAME_PORT);
	length = readword(ioaddr, RX_FRAME_PORT);

	if ((status & RX_OK) == 0) {
		count_rx_errors(status, dev);
		return;
	}

	
	skb = netdev_alloc_skb(dev, length + 2);
	if (skb == NULL) {
#if 0		
		printk(KERN_WARNING "%s: Memory squeeze, dropping packet.\n", dev->name);
#endif
		dev->stats.rx_dropped++;
		return;
	}
	skb_reserve(skb, 2);	

	readwords(ioaddr, RX_FRAME_PORT, skb_put(skb, length), length >> 1);
	if (length & 1)
		skb->data[length-1] = readword(ioaddr, RX_FRAME_PORT);

	if (net_debug > 3) {
		printk(	"%s: received %d byte packet of type %x\n",
			dev->name, length,
			(skb->data[ETH_ALEN+ETH_ALEN] << 8) | skb->data[ETH_ALEN+ETH_ALEN+1]);
	}

        skb->protocol=eth_type_trans(skb,dev);
	netif_rx(skb);
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += length;
}

#if ALLOW_DMA
static void release_dma_buff(struct net_local *lp)
{
	if (lp->dma_buff) {
		free_pages((unsigned long)(lp->dma_buff), get_order(lp->dmasize * 1024));
		lp->dma_buff = NULL;
	}
}
#endif

static int
net_close(struct net_device *dev)
{
#if ALLOW_DMA
	struct net_local *lp = netdev_priv(dev);
#endif

	netif_stop_queue(dev);

	writereg(dev, PP_RxCFG, 0);
	writereg(dev, PP_TxCFG, 0);
	writereg(dev, PP_BufCFG, 0);
	writereg(dev, PP_BusCTL, 0);

	free_irq(dev->irq, dev);

#if ALLOW_DMA
	if (lp->use_dma && lp->dma) {
		free_dma(dev->dma);
		release_dma_buff(lp);
	}
#endif

	
	return 0;
}

static struct net_device_stats *
net_get_stats(struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);
	unsigned long flags;

	spin_lock_irqsave(&lp->lock, flags);
	
	dev->stats.rx_missed_errors += (readreg(dev, PP_RxMiss) >> 6);
	dev->stats.collisions += (readreg(dev, PP_TxCol) >> 6);
	spin_unlock_irqrestore(&lp->lock, flags);

	return &dev->stats;
}

static void set_multicast_list(struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);
	unsigned long flags;

	spin_lock_irqsave(&lp->lock, flags);
	if(dev->flags&IFF_PROMISC)
	{
		lp->rx_mode = RX_ALL_ACCEPT;
	}
	else if ((dev->flags & IFF_ALLMULTI) || !netdev_mc_empty(dev))
	{
		lp->rx_mode = RX_MULTCAST_ACCEPT;
	}
	else
		lp->rx_mode = 0;

	writereg(dev, PP_RxCTL, DEF_RX_ACCEPT | lp->rx_mode);

	
	writereg(dev, PP_RxCFG, lp->curr_rx_cfg |
	     (lp->rx_mode == RX_ALL_ACCEPT? (RX_CRC_ERROR_ENBL|RX_RUNT_ENBL|RX_EXTRA_DATA_ENBL) : 0));
	spin_unlock_irqrestore(&lp->lock, flags);
}


static int set_mac_address(struct net_device *dev, void *p)
{
	int i;
	struct sockaddr *addr = p;

	if (netif_running(dev))
		return -EBUSY;

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

	if (net_debug)
		printk("%s: Setting MAC address to %pM.\n",
		       dev->name, dev->dev_addr);

	
	for (i=0; i < ETH_ALEN/2; i++)
		writereg(dev, PP_IA+i*2, dev->dev_addr[i*2] | (dev->dev_addr[i*2+1] << 8));

	return 0;
}

#if defined(MODULE) && !defined(CONFIG_CS89x0_PLATFORM)

static struct net_device *dev_cs89x0;


static int io;
static int irq;
static int debug;
static char media[8];
static int duplex=-1;

static int use_dma;			
static int dma;
static int dmasize=16;			

module_param(io, int, 0);
module_param(irq, int, 0);
module_param(debug, int, 0);
module_param_string(media, media, sizeof(media), 0);
module_param(duplex, int, 0);
module_param(dma , int, 0);
module_param(dmasize , int, 0);
module_param(use_dma , int, 0);
MODULE_PARM_DESC(io, "cs89x0 I/O base address");
MODULE_PARM_DESC(irq, "cs89x0 IRQ number");
#if DEBUGGING
MODULE_PARM_DESC(debug, "cs89x0 debug level (0-6)");
#else
MODULE_PARM_DESC(debug, "(ignored)");
#endif
MODULE_PARM_DESC(media, "Set cs89x0 adapter(s) media type(s) (rj45,bnc,aui)");
MODULE_PARM_DESC(duplex, "(ignored)");
#if ALLOW_DMA
MODULE_PARM_DESC(dma , "cs89x0 ISA DMA channel; ignored if use_dma=0");
MODULE_PARM_DESC(dmasize , "cs89x0 DMA size in kB (16,64); ignored if use_dma=0");
MODULE_PARM_DESC(use_dma , "cs89x0 using DMA (0-1)");
#else
MODULE_PARM_DESC(dma , "(ignored)");
MODULE_PARM_DESC(dmasize , "(ignored)");
MODULE_PARM_DESC(use_dma , "(ignored)");
#endif

MODULE_AUTHOR("Mike Cruse, Russwll Nelson <nelson@crynwr.com>, Andrew Morton");
MODULE_LICENSE("GPL");



int __init init_module(void)
{
	struct net_device *dev = alloc_etherdev(sizeof(struct net_local));
	struct net_local *lp;
	int ret = 0;

#if DEBUGGING
	net_debug = debug;
#else
	debug = 0;
#endif
	if (!dev)
		return -ENOMEM;

	dev->irq = irq;
	dev->base_addr = io;
	lp = netdev_priv(dev);

#if ALLOW_DMA
	if (use_dma) {
		lp->use_dma = use_dma;
		lp->dma = dma;
		lp->dmasize = dmasize;
	}
#endif

	spin_lock_init(&lp->lock);

        
        if (!strcmp(media, "rj45"))
		lp->adapter_cnf = A_CNF_MEDIA_10B_T | A_CNF_10B_T;
	else if (!strcmp(media, "aui"))
		lp->adapter_cnf = A_CNF_MEDIA_AUI   | A_CNF_AUI;
	else if (!strcmp(media, "bnc"))
		lp->adapter_cnf = A_CNF_MEDIA_10B_2 | A_CNF_10B_2;
	else
		lp->adapter_cnf = A_CNF_MEDIA_10B_T | A_CNF_10B_T;

        if (duplex==-1)
		lp->auto_neg_cnf = AUTO_NEG_ENABLE;

        if (io == 0) {
                printk(KERN_ERR "cs89x0.c: Module autoprobing not allowed.\n");
                printk(KERN_ERR "cs89x0.c: Append io=0xNNN\n");
                ret = -EPERM;
		goto out;
        } else if (io <= 0x1ff) {
		ret = -ENXIO;
		goto out;
	}

#if ALLOW_DMA
	if (use_dma && dmasize != 16 && dmasize != 64) {
		printk(KERN_ERR "cs89x0.c: dma size must be either 16K or 64K, not %dK\n", dmasize);
		ret = -EPERM;
		goto out;
	}
#endif
	ret = cs89x0_probe1(dev, io, 1);
	if (ret)
		goto out;

	dev_cs89x0 = dev;
	return 0;
out:
	free_netdev(dev);
	return ret;
}

void __exit
cleanup_module(void)
{
	unregister_netdev(dev_cs89x0);
	writeword(dev_cs89x0->base_addr, ADD_PORT, PP_ChipID);
	release_region(dev_cs89x0->base_addr, NETCARD_IO_EXTENT);
	free_netdev(dev_cs89x0);
}
#endif 

#ifdef CONFIG_CS89x0_PLATFORM
static int __init cs89x0_platform_probe(struct platform_device *pdev)
{
	struct net_device *dev = alloc_etherdev(sizeof(struct net_local));
	struct net_local *lp;
	struct resource *mem_res;
	int err;

	if (!dev)
		return -ENOMEM;

	lp = netdev_priv(dev);

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dev->irq = platform_get_irq(pdev, 0);
	if (mem_res == NULL || dev->irq <= 0) {
		dev_warn(&dev->dev, "memory/interrupt resource missing.\n");
		err = -ENXIO;
		goto free;
	}

	lp->phys_addr = mem_res->start;
	lp->size = resource_size(mem_res);
	if (!request_mem_region(lp->phys_addr, lp->size, DRV_NAME)) {
		dev_warn(&dev->dev, "request_mem_region() failed.\n");
		err = -EBUSY;
		goto free;
	}

	lp->virt_addr = ioremap(lp->phys_addr, lp->size);
	if (!lp->virt_addr) {
		dev_warn(&dev->dev, "ioremap() failed.\n");
		err = -ENOMEM;
		goto release;
	}

	err = cs89x0_probe1(dev, (unsigned long)lp->virt_addr, 0);
	if (err) {
		dev_warn(&dev->dev, "no cs8900 or cs8920 detected.\n");
		goto unmap;
	}

	platform_set_drvdata(pdev, dev);
	return 0;

unmap:
	iounmap(lp->virt_addr);
release:
	release_mem_region(lp->phys_addr, lp->size);
free:
	free_netdev(dev);
	return err;
}

static int cs89x0_platform_remove(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct net_local *lp = netdev_priv(dev);

	unregister_netdev(dev);
	iounmap(lp->virt_addr);
	release_mem_region(lp->phys_addr, lp->size);
	free_netdev(dev);
	return 0;
}

static struct platform_driver cs89x0_driver = {
	.driver	= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
	},
	.remove	= cs89x0_platform_remove,
};

static int __init cs89x0_init(void)
{
	return platform_driver_probe(&cs89x0_driver, cs89x0_platform_probe);
}

module_init(cs89x0_init);

static void __exit cs89x0_cleanup(void)
{
	platform_driver_unregister(&cs89x0_driver);
}

module_exit(cs89x0_cleanup);

#endif 

