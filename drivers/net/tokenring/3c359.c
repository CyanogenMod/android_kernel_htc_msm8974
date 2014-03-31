/*
 *   3c359.c (c) 2000 Mike Phillips (mikep@linuxtr.net) All Rights Reserved
 *
 *  Linux driver for 3Com 3c359 Tokenlink Velocity XL PCI NIC
 *
 *  Base Driver Olympic:
 *	Written 1999 Peter De Schrijver & Mike Phillips
 *
 *  This software may be used and distributed according to the terms
 *  of the GNU General Public License, incorporated herein by reference.
 * 
 *  7/17/00 - Clean up, version number 0.9.0. Ready to release to the world.
 *
 *  2/16/01 - Port up to kernel 2.4.2 ready for submission into the kernel.
 *  3/05/01 - Last clean up stuff before submission.
 *  2/15/01 - Finally, update to new pci api. 
 *
 *  To Do:
 */


#define XL_DEBUG 0

#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/in.h>
#include <linux/ioport.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/ptrace.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/trdevice.h>
#include <linux/stddef.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/bitops.h>
#include <linux/firmware.h>
#include <linux/slab.h>

#include <net/checksum.h>

#include <asm/io.h>

#include "3c359.h"

static char version[] __devinitdata  = 
"3c359.c v1.2.0 2/17/01 - Mike Phillips (mikep@linuxtr.net)" ; 

#define FW_NAME		"3com/3C359.bin"
MODULE_AUTHOR("Mike Phillips <mikep@linuxtr.net>") ; 
MODULE_DESCRIPTION("3Com 3C359 Velocity XL Token Ring Adapter Driver\n") ;
MODULE_FIRMWARE(FW_NAME);



static int ringspeed[XL_MAX_ADAPTERS] = {0,} ;

module_param_array(ringspeed, int, NULL, 0);
MODULE_PARM_DESC(ringspeed,"3c359: Ringspeed selection - 4,16 or 0") ;


static int pkt_buf_sz[XL_MAX_ADAPTERS] = {0,} ;
 
module_param_array(pkt_buf_sz, int, NULL, 0) ;
MODULE_PARM_DESC(pkt_buf_sz,"3c359: Initial buffer size") ;

static int message_level[XL_MAX_ADAPTERS] = {0,} ;

module_param_array(message_level, int, NULL, 0) ;
MODULE_PARM_DESC(message_level, "3c359: Level of reported messages") ;

static DEFINE_PCI_DEVICE_TABLE(xl_pci_tbl) =
{
	{PCI_VENDOR_ID_3COM,PCI_DEVICE_ID_3COM_3C359, PCI_ANY_ID, PCI_ANY_ID, },
	{ }			
};
MODULE_DEVICE_TABLE(pci,xl_pci_tbl) ; 

static int xl_init(struct net_device *dev);
static int xl_open(struct net_device *dev);
static int xl_open_hw(struct net_device *dev) ;  
static int xl_hw_reset(struct net_device *dev); 
static netdev_tx_t xl_xmit(struct sk_buff *skb, struct net_device *dev);
static void xl_dn_comp(struct net_device *dev); 
static int xl_close(struct net_device *dev);
static void xl_set_rx_mode(struct net_device *dev);
static irqreturn_t xl_interrupt(int irq, void *dev_id);
static int xl_set_mac_address(struct net_device *dev, void *addr) ; 
static void xl_arb_cmd(struct net_device *dev);
static void xl_asb_cmd(struct net_device *dev) ; 
static void xl_srb_cmd(struct net_device *dev, int srb_cmd) ; 
static void xl_wait_misr_flags(struct net_device *dev) ; 
static int xl_change_mtu(struct net_device *dev, int mtu);
static void xl_srb_bh(struct net_device *dev) ; 
static void xl_asb_bh(struct net_device *dev) ; 
static void xl_reset(struct net_device *dev) ;  
static void xl_freemem(struct net_device *dev) ;  


static u16  xl_ee_read(struct net_device *dev, int ee_addr) ; 
static void  xl_ee_write(struct net_device *dev, int ee_addr, u16 ee_value) ; 

#if XL_DEBUG
static void print_tx_state(struct net_device *dev) ; 
static void print_rx_state(struct net_device *dev) ; 

static void print_tx_state(struct net_device *dev)
{

	struct xl_private *xl_priv = netdev_priv(dev);
	struct xl_tx_desc *txd ; 
	u8 __iomem *xl_mmio = xl_priv->xl_mmio ; 
	int i ; 

	printk("tx_ring_head: %d, tx_ring_tail: %d, free_ent: %d\n",xl_priv->tx_ring_head,
		xl_priv->tx_ring_tail, xl_priv->free_ring_entries) ; 
	printk("Ring    , Address ,   FSH  , DnNextPtr, Buffer, Buffer_Len\n");
	for (i = 0; i < 16; i++) {
		txd = &(xl_priv->xl_tx_ring[i]) ; 
		printk("%d, %08lx, %08x, %08x, %08x, %08x\n", i, virt_to_bus(txd),
			txd->framestartheader, txd->dnnextptr, txd->buffer, txd->buffer_length ) ; 
	}

	printk("DNLISTPTR = %04x\n", readl(xl_mmio + MMIO_DNLISTPTR) );
	
	printk("DmaCtl = %04x\n", readl(xl_mmio + MMIO_DMA_CTRL) );
	printk("Queue status = %0x\n",netif_running(dev) ) ;
}

static void print_rx_state(struct net_device *dev)
{

	struct xl_private *xl_priv = netdev_priv(dev);
	struct xl_rx_desc *rxd ; 
	u8 __iomem *xl_mmio = xl_priv->xl_mmio ; 
	int i ; 

	printk("rx_ring_tail: %d\n", xl_priv->rx_ring_tail);
	printk("Ring    , Address ,   FrameState  , UPNextPtr, FragAddr, Frag_Len\n");
	for (i = 0; i < 16; i++) { 
		
		rxd = &(xl_priv->xl_rx_ring[i]) ; 
		printk("%d, %08lx, %08x, %08x, %08x, %08x\n", i, virt_to_bus(rxd),
			rxd->framestatus, rxd->upnextptr, rxd->upfragaddr, rxd->upfraglen ) ; 
	}

	printk("UPLISTPTR = %04x\n", readl(xl_mmio + MMIO_UPLISTPTR));
	
	printk("DmaCtl = %04x\n", readl(xl_mmio + MMIO_DMA_CTRL));
	printk("Queue status = %0x\n",netif_running(dev));
} 
#endif


static u16 xl_ee_read(struct net_device *dev, int ee_addr)
{ 
	struct xl_private *xl_priv = netdev_priv(dev);
	u8 __iomem *xl_mmio = xl_priv->xl_mmio ; 

	
	writel(IO_WORD_READ | EECONTROL, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	while ( readw(xl_mmio + MMIO_MACDATA) & EEBUSY ) ;

	
	writel(IO_WORD_WRITE | EECONTROL, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writew(EEREAD + ee_addr, xl_mmio + MMIO_MACDATA) ; 

	
	writel(IO_WORD_READ | EECONTROL, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	while ( readw(xl_mmio + MMIO_MACDATA) & EEBUSY ) ; 
	
	
	writel(IO_WORD_WRITE | EECONTROL , xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writew(EEREAD + ee_addr, xl_mmio + MMIO_MACDATA) ; 

	
	writel(IO_WORD_READ | EEDATA , xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	return readw(xl_mmio + MMIO_MACDATA) ; 
}


static void  xl_ee_write(struct net_device *dev, int ee_addr, u16 ee_value) 
{
	struct xl_private *xl_priv = netdev_priv(dev);
	u8 __iomem *xl_mmio = xl_priv->xl_mmio ; 

	
	writel(IO_WORD_READ | EECONTROL, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	while ( readw(xl_mmio + MMIO_MACDATA) & EEBUSY ) ;
	
	
	writel(IO_WORD_WRITE | EECONTROL, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writew(EE_ENABLE_WRITE, xl_mmio + MMIO_MACDATA) ; 

	
	writel(IO_WORD_READ | EECONTROL, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	while ( readw(xl_mmio + MMIO_MACDATA) & EEBUSY ) ;

	 
	writel(IO_WORD_WRITE | EEDATA, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writew(ee_value, xl_mmio + MMIO_MACDATA) ;

	
	writel(IO_WORD_WRITE | EECONTROL, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writew(EEWRITE + ee_addr, xl_mmio + MMIO_MACDATA) ; 

	
	writel(IO_WORD_READ | EECONTROL, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	while ( readw(xl_mmio + MMIO_MACDATA) & EEBUSY ) ;
	
	return ; 
}

static const struct net_device_ops xl_netdev_ops = {
	.ndo_open		= xl_open,
	.ndo_stop		= xl_close,
	.ndo_start_xmit		= xl_xmit,
	.ndo_change_mtu		= xl_change_mtu,
	.ndo_set_rx_mode	= xl_set_rx_mode,
	.ndo_set_mac_address	= xl_set_mac_address,
};
 
static int __devinit xl_probe(struct pci_dev *pdev,
			      const struct pci_device_id *ent) 
{
	struct net_device *dev ; 
	struct xl_private *xl_priv ; 
	static int card_no = -1 ;
	int i ; 

	card_no++ ; 

	if (pci_enable_device(pdev)) { 
		return -ENODEV ; 
	} 

	pci_set_master(pdev);

	if ((i = pci_request_regions(pdev,"3c359"))) { 
		return i ; 
	}


	dev = alloc_trdev(sizeof(struct xl_private)) ; 
	if (!dev) { 
		pci_release_regions(pdev) ; 
		return -ENOMEM ; 
	} 
	xl_priv = netdev_priv(dev);

#if XL_DEBUG  
	printk("pci_device: %p, dev:%p, dev->priv: %p, ba[0]: %10x, ba[1]:%10x\n", 
		pdev, dev, netdev_priv(dev), (unsigned int)pdev->resource[0].start, (unsigned int)pdev->resource[1].start);
#endif 

	dev->irq=pdev->irq;
	dev->base_addr=pci_resource_start(pdev,0) ; 
	xl_priv->xl_card_name = pci_name(pdev);
	xl_priv->xl_mmio=ioremap(pci_resource_start(pdev,1), XL_IO_SPACE);
	xl_priv->pdev = pdev ; 
		
	if ((pkt_buf_sz[card_no] < 100) || (pkt_buf_sz[card_no] > 18000) )
		xl_priv->pkt_buf_sz = PKT_BUF_SZ ; 
	else
		xl_priv->pkt_buf_sz = pkt_buf_sz[card_no] ; 

	dev->mtu = xl_priv->pkt_buf_sz - TR_HLEN ; 
	xl_priv->xl_ring_speed = ringspeed[card_no] ; 
	xl_priv->xl_message_level = message_level[card_no] ; 
	xl_priv->xl_functional_addr[0] = xl_priv->xl_functional_addr[1] = xl_priv->xl_functional_addr[2] = xl_priv->xl_functional_addr[3] = 0 ; 
	xl_priv->xl_copy_all_options = 0 ; 
		
	if((i = xl_init(dev))) {
		iounmap(xl_priv->xl_mmio) ; 
		free_netdev(dev) ; 
		pci_release_regions(pdev) ; 
		return i ; 
	}				

	dev->netdev_ops = &xl_netdev_ops;
	SET_NETDEV_DEV(dev, &pdev->dev);

	pci_set_drvdata(pdev,dev) ; 
	if ((i = register_netdev(dev))) { 
		printk(KERN_ERR "3C359, register netdev failed\n") ;  
		pci_set_drvdata(pdev,NULL) ; 
		iounmap(xl_priv->xl_mmio) ; 
		free_netdev(dev) ; 
		pci_release_regions(pdev) ; 
		return i ; 
	}
   
	printk(KERN_INFO "3C359: %s registered as: %s\n",xl_priv->xl_card_name,dev->name) ; 

	return 0; 
}

static int xl_init_firmware(struct xl_private *xl_priv)
{
	int err;

	err = request_firmware(&xl_priv->fw, FW_NAME, &xl_priv->pdev->dev);
	if (err) {
		printk(KERN_ERR "Failed to load firmware \"%s\"\n", FW_NAME);
		return err;
	}

	if (xl_priv->fw->size < 16) {
		printk(KERN_ERR "Bogus length %zu in \"%s\"\n",
		       xl_priv->fw->size, FW_NAME);
		release_firmware(xl_priv->fw);
		err = -EINVAL;
	}

	return err;
}

static int __devinit xl_init(struct net_device *dev) 
{
	struct xl_private *xl_priv = netdev_priv(dev);
	int err;

	printk(KERN_INFO "%s\n", version);
	printk(KERN_INFO "%s: I/O at %hx, MMIO at %p, using irq %d\n",
		xl_priv->xl_card_name, (unsigned int)dev->base_addr ,xl_priv->xl_mmio, dev->irq);

	spin_lock_init(&xl_priv->xl_lock) ; 

	err = xl_init_firmware(xl_priv);
	if (err == 0)
		err = xl_hw_reset(dev);

	return err;
}



static int xl_hw_reset(struct net_device *dev) 
{
	struct xl_private *xl_priv = netdev_priv(dev);
	u8 __iomem *xl_mmio = xl_priv->xl_mmio ; 
	unsigned long t ; 
	u16 i ; 
    	u16 result_16 ; 
	u8 result_8 ;
	u16 start ; 
	int j ;

	if (xl_priv->fw == NULL)
		return -EINVAL;


	writew( GLOBAL_RESET, xl_mmio + MMIO_COMMAND ) ; 


	t=jiffies;
	while (readw(xl_mmio + MMIO_INTSTATUS) & INTSTAT_CMD_IN_PROGRESS) { 
		schedule();		
		if (time_after(jiffies, t + 40 * HZ)) {
			printk(KERN_ERR "%s: 3COM 3C359 Velocity XL  card not responding to global reset.\n", dev->name);
			return -ENODEV;
		}
	}


	writel( (IO_BYTE_READ | CPATTENTION), xl_mmio + MMIO_MAC_ACCESS_CMD) ;
	result_8 = readb(xl_mmio + MMIO_MACDATA) ; 
	result_8 = result_8 | CPA_PMBARVIS ; 
	writel( (IO_BYTE_WRITE | CPATTENTION), xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writeb(result_8, xl_mmio + MMIO_MACDATA) ; 
	

	writel( (IO_WORD_READ | PMBAR),xl_mmio + MMIO_MAC_ACCESS_CMD);  

#if XL_DEBUG
	printk(KERN_INFO "Read from PMBAR = %04x\n", readw(xl_mmio + MMIO_MACDATA));
#endif

	if ( readw( (xl_mmio + MMIO_MACDATA))  & PMB_CPHOLD ) { 

		

		writel( (IO_WORD_READ | PMBAR),xl_mmio + MMIO_MAC_ACCESS_CMD);  
		result_16 = readw(xl_mmio + MMIO_MACDATA) ; 
		result_16 = result_16 & ~((0x7F) << 2) ; 
		writel( (IO_WORD_WRITE | PMBAR), xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		writew(result_16,xl_mmio + MMIO_MACDATA) ; 
	
		

		writel( (IO_BYTE_READ | CPATTENTION), xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		result_8 = readb(xl_mmio + MMIO_MACDATA) ; 
		result_8 = result_8 | CPA_MEMWREN  ; 
		writel( (IO_BYTE_WRITE | CPATTENTION), xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		writeb(result_8, xl_mmio + MMIO_MACDATA) ; 

		start = (0xFFFF - (xl_priv->fw->size) + 1) ;

		printk(KERN_INFO "3C359: Uploading Microcode: "); 

		for (i = start, j = 0; j < xl_priv->fw->size; i++, j++) {
			writel(MEM_BYTE_WRITE | 0XD0000 | i,
			       xl_mmio + MMIO_MAC_ACCESS_CMD);
			writeb(xl_priv->fw->data[j], xl_mmio + MMIO_MACDATA);
			if (j % 1024 == 0)
				printk(".");
		}
		printk("\n") ; 

		for (i = 0; i < 16; i++) {
			writel((MEM_BYTE_WRITE | 0xDFFF0) + i,
			       xl_mmio + MMIO_MAC_ACCESS_CMD);
			writeb(xl_priv->fw->data[xl_priv->fw->size - 16 + i],
			       xl_mmio + MMIO_MACDATA);
		}


		writel(MEM_WORD_WRITE | 0xDFFF4, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		writew(start >> 4, xl_mmio + MMIO_MACDATA);

		
	
		writel( (IO_BYTE_READ | CPATTENTION), xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		result_8 = readb(xl_mmio + MMIO_MACDATA) ; 
		result_8 = result_8 & ~CPA_MEMWREN ; 
		writel( (IO_BYTE_WRITE | CPATTENTION), xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		writeb(result_8, xl_mmio + MMIO_MACDATA) ; 

		

		writel( (IO_WORD_READ | PMBAR),xl_mmio + MMIO_MAC_ACCESS_CMD);  
		result_16 = readw(xl_mmio + MMIO_MACDATA) ; 
		result_16 = result_16 & ~PMB_CPHOLD ; 
		writel( (IO_WORD_WRITE | PMBAR), xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		writew(result_16,xl_mmio + MMIO_MACDATA) ; 


	} 


#if XL_DEBUG
	printk(KERN_INFO "%s: Microcode uploaded, must wait for the self test to complete\n", dev->name);
#endif

	writew(SETINDENABLE | 0xFFF, xl_mmio + MMIO_COMMAND) ; 

	t=jiffies;
	while ( !(readw(xl_mmio + MMIO_INTSTATUS_AUTO) & INTSTAT_SRB) ) { 
		schedule();		
		if (time_after(jiffies, t + 15 * HZ)) {
			printk(KERN_ERR "3COM 3C359 Velocity XL  card not responding.\n");
			return -ENODEV; 
		}
	}


	writel(MMIO_WORD_WRITE | RXBUFAREA, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writew(0xD000, xl_mmio + MMIO_MACDATA) ; 
	
	writel(MMIO_WORD_WRITE | RXEARLYTHRESH, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writew(0X0020, xl_mmio + MMIO_MACDATA) ; 
	
	writew( SETTXSTARTTHRESH | 0x40 , xl_mmio + MMIO_COMMAND) ; 

	writeb(0x04, xl_mmio + MMIO_DNBURSTTHRESH) ; 
	writeb(0x04, xl_mmio + DNPRIREQTHRESH) ;


	writel(MMIO_BYTE_READ | WRBR, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	xl_priv->srb = readb(xl_mmio + MMIO_MACDATA) << 8 ; 
	writel( (MMIO_BYTE_READ | WRBR) + 1, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	xl_priv->srb = xl_priv->srb | readb(xl_mmio + MMIO_MACDATA) ;

#if XL_DEBUG
	writel(IO_WORD_READ | SWITCHSETTINGS, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	if ( readw(xl_mmio + MMIO_MACDATA) & 2) { 
		printk(KERN_INFO "Default ring speed 4 mbps\n");
	} else {
		printk(KERN_INFO "Default ring speed 16 mbps\n");
	} 
	printk(KERN_INFO "%s: xl_priv->srb = %04x\n",xl_priv->xl_card_name, xl_priv->srb);
#endif

	return 0;
}

static int xl_open(struct net_device *dev) 
{
	struct xl_private *xl_priv=netdev_priv(dev);
	u8 __iomem *xl_mmio = xl_priv->xl_mmio ; 
	u8 i ; 
	__le16 hwaddr[3] ; 
	int open_err ;

	u16 switchsettings, switchsettings_eeprom  ;
 
	if (request_irq(dev->irq, xl_interrupt, IRQF_SHARED , "3c359", dev))
		return -EAGAIN;

	
	hwaddr[0] = cpu_to_le16(xl_ee_read(dev,0x10));
	hwaddr[1] = cpu_to_le16(xl_ee_read(dev,0x11));
	hwaddr[2] = cpu_to_le16(xl_ee_read(dev,0x12));

	

	switchsettings_eeprom = xl_ee_read(dev,0x08) ;
	switchsettings = switchsettings_eeprom ;  

	if (xl_priv->xl_ring_speed != 0) { 
		if (xl_priv->xl_ring_speed == 4)  
			switchsettings = switchsettings | 0x02 ; 
		else 
			switchsettings = switchsettings & ~0x02 ; 
	}

	
	if (switchsettings != switchsettings_eeprom) { 
		xl_ee_write(dev,0x08,switchsettings) ; 
		
		xl_hw_reset(dev) ; 
	}

	memcpy(dev->dev_addr,hwaddr,dev->addr_len) ; 
	
	open_err = xl_open_hw(dev) ; 


	if (open_err != 0) { 
		if (open_err & 0x07) { 
			printk(KERN_WARNING "%s: Open Error, retrying at different ringspeed\n", dev->name);
			switchsettings = switchsettings ^ 2 ; 
			xl_ee_write(dev,0x08,switchsettings) ; 
			xl_hw_reset(dev) ; 
			open_err = xl_open_hw(dev) ; 
			if (open_err != 0) { 
				printk(KERN_WARNING "%s: Open error returned a second time, we're bombing out now\n", dev->name); 
				free_irq(dev->irq,dev) ; 						
				return -ENODEV ;
			}  
		} else { 
			printk(KERN_WARNING "%s: Open Error = %04x\n", dev->name, open_err) ; 
			free_irq(dev->irq,dev) ; 
			return -ENODEV ; 
		}
	}

	
	xl_priv->xl_tx_ring = kzalloc((sizeof(struct xl_tx_desc) * XL_TX_RING_SIZE) + 7, GFP_DMA | GFP_KERNEL);
	if (xl_priv->xl_tx_ring == NULL) {
		free_irq(dev->irq,dev);
		return -ENOMEM;
	}
	xl_priv->xl_rx_ring = kzalloc((sizeof(struct xl_rx_desc) * XL_RX_RING_SIZE) +7, GFP_DMA | GFP_KERNEL);
	if (xl_priv->xl_rx_ring == NULL) {
		free_irq(dev->irq,dev);
		kfree(xl_priv->xl_tx_ring);
		return -ENOMEM;
	}

	 
	 for (i=0 ; i < XL_RX_RING_SIZE ; i++) { 
		struct sk_buff *skb ; 

		skb = dev_alloc_skb(xl_priv->pkt_buf_sz) ; 
		if (skb==NULL) 
			break ; 

		skb->dev = dev ; 
		xl_priv->xl_rx_ring[i].upfragaddr = cpu_to_le32(pci_map_single(xl_priv->pdev, skb->data,xl_priv->pkt_buf_sz, PCI_DMA_FROMDEVICE));
		xl_priv->xl_rx_ring[i].upfraglen = cpu_to_le32(xl_priv->pkt_buf_sz) | RXUPLASTFRAG;
		xl_priv->rx_ring_skb[i] = skb ; 	
	}

	if (i==0) { 
		printk(KERN_WARNING "%s: Not enough memory to allocate rx buffers. Adapter disabled\n",dev->name);
		free_irq(dev->irq,dev) ; 
		kfree(xl_priv->xl_tx_ring);
		kfree(xl_priv->xl_rx_ring);
		return -EIO ; 
	} 

	xl_priv->rx_ring_no = i ; 
	xl_priv->rx_ring_tail = 0 ; 
	xl_priv->rx_ring_dma_addr = pci_map_single(xl_priv->pdev,xl_priv->xl_rx_ring, sizeof(struct xl_rx_desc) * XL_RX_RING_SIZE, PCI_DMA_TODEVICE) ; 
	for (i=0;i<(xl_priv->rx_ring_no-1);i++) { 
		xl_priv->xl_rx_ring[i].upnextptr = cpu_to_le32(xl_priv->rx_ring_dma_addr + (sizeof (struct xl_rx_desc) * (i+1)));
	} 
	xl_priv->xl_rx_ring[i].upnextptr = 0 ; 

	writel(xl_priv->rx_ring_dma_addr, xl_mmio + MMIO_UPLISTPTR) ; 
	
	
	
	xl_priv->tx_ring_dma_addr = pci_map_single(xl_priv->pdev,xl_priv->xl_tx_ring, sizeof(struct xl_tx_desc) * XL_TX_RING_SIZE,PCI_DMA_TODEVICE) ; 
	
	xl_priv->tx_ring_head = 1 ; 
	xl_priv->tx_ring_tail = 255 ; 
	xl_priv->free_ring_entries = XL_TX_RING_SIZE ; 


	xl_priv->xl_tx_ring[0].framestartheader = TXDPDEMPTY;
	xl_priv->xl_tx_ring[0].buffer = 0 ; 
	xl_priv->xl_tx_ring[0].buffer_length = 0 ; 
	xl_priv->xl_tx_ring[0].dnnextptr = 0 ; 

	writel(xl_priv->tx_ring_dma_addr, xl_mmio + MMIO_DNLISTPTR) ; 
	writel(DNUNSTALL, xl_mmio + MMIO_COMMAND) ; 
	writel(UPUNSTALL, xl_mmio + MMIO_COMMAND) ; 
	writel(DNENABLE, xl_mmio + MMIO_COMMAND) ; 
	writeb(0x40, xl_mmio + MMIO_DNPOLL) ;	


	writel(SETINTENABLE | INT_MASK, xl_mmio + MMIO_COMMAND) ; 
	writel(SETINDENABLE | INT_MASK, xl_mmio + MMIO_COMMAND) ; 

	netif_start_queue(dev) ; 	
	return 0;
	
}	

static int xl_open_hw(struct net_device *dev) 
{ 
	struct xl_private *xl_priv=netdev_priv(dev);
	u8 __iomem *xl_mmio = xl_priv->xl_mmio ; 
	u16 vsoff ;
	char ver_str[33];  
	int open_err ; 
	int i ; 
	unsigned long t ; 

		
	writel( (MEM_BYTE_WRITE | 0xD0000 | xl_priv->srb), xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writeb(OPEN_NIC, xl_mmio + MMIO_MACDATA) ; 
	

	writel( (MEM_BYTE_WRITE | 0xD0000 | xl_priv->srb)+ 2, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writeb(0xff,xl_mmio + MMIO_MACDATA) ; 

	
	writel( (MEM_BYTE_WRITE | 0xD0000 | xl_priv->srb) + 8, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writeb(0x00, xl_mmio + MMIO_MACDATA) ; 
	writel( (MEM_BYTE_WRITE | 0xD0000 | xl_priv->srb) + 9, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writeb(0x00, xl_mmio + MMIO_MACDATA) ; 


	if (xl_priv->xl_laa[0]) {  
		for (i=10;i<16;i++) { 
			writel( (MEM_BYTE_WRITE | 0xD0000 | xl_priv->srb) + i, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
			writeb(xl_priv->xl_laa[i-10],xl_mmio + MMIO_MACDATA) ;
		}
		memcpy(dev->dev_addr,xl_priv->xl_laa,dev->addr_len) ; 
	} else {  
		for (i=10;i<16;i++) { 
			writel( (MEM_BYTE_WRITE | 0xD0000 | xl_priv->srb) + i, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
			writeb(dev->dev_addr[i-10], xl_mmio + MMIO_MACDATA) ; 
		}
	}

	
	for (i = 16; i < 34; i++) {
		writel( (MEM_BYTE_WRITE | 0xD0000 | xl_priv->srb) + i, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		writeb(0x00,xl_mmio + MMIO_MACDATA) ; 
	}
	

	xl_wait_misr_flags(dev) ; 
	writel(MEM_BYTE_WRITE | MF_CSRB, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writeb(0xFF, xl_mmio + MMIO_MACDATA) ; 
	writel(MMIO_BYTE_WRITE | MISR_SET, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writeb(MISR_CSRB , xl_mmio + MMIO_MACDATA) ; 


	t=jiffies;
	while (! (readw(xl_mmio + MMIO_INTSTATUS) & INTSTAT_SRB)) { 
		schedule();		
		if (time_after(jiffies, t + 40 * HZ)) {
			printk(KERN_ERR "3COM 3C359 Velocity XL  card not responding.\n");
			break ; 
		}
	}


	writel( (MEM_BYTE_READ | 0xD0000 | xl_priv->srb)+2, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	if (readb(xl_mmio + MMIO_MACDATA)!=0) {
		open_err = readb(xl_mmio + MMIO_MACDATA) << 8 ; 
		writel( (MEM_BYTE_READ | 0xD0000 | xl_priv->srb) + 7, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		open_err |= readb(xl_mmio + MMIO_MACDATA) ; 
		return open_err ; 
	} else { 
		writel( (MEM_WORD_READ | 0xD0000 | xl_priv->srb) + 8, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		xl_priv->asb = swab16(readw(xl_mmio + MMIO_MACDATA)) ;
		printk(KERN_INFO "%s: Adapter Opened Details: ",dev->name) ; 
		printk("ASB: %04x",xl_priv->asb ) ; 
		writel( (MEM_WORD_READ | 0xD0000 | xl_priv->srb) + 10, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		printk(", SRB: %04x",swab16(readw(xl_mmio + MMIO_MACDATA)) ) ;
 
		writel( (MEM_WORD_READ | 0xD0000 | xl_priv->srb) + 12, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		xl_priv->arb = swab16(readw(xl_mmio + MMIO_MACDATA)) ;
		printk(", ARB: %04x\n",xl_priv->arb );
		writel( (MEM_WORD_READ | 0xD0000 | xl_priv->srb) + 14, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		vsoff = swab16(readw(xl_mmio + MMIO_MACDATA)) ;


		for (i=0;i<0x20;i++) { 
			writel( (MEM_BYTE_READ | 0xD0000 | vsoff) + i, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
			ver_str[i] = readb(xl_mmio + MMIO_MACDATA) ; 
		}
		ver_str[i] = '\0' ; 
		printk(KERN_INFO "%s: Microcode version String: %s\n",dev->name,ver_str);
	} 	
	
	writew(ACK_INTERRUPT | SRBRACK | LATCH_ACK, xl_mmio + MMIO_COMMAND) ; 

	return 0 ; 
}


static void adv_rx_ring(struct net_device *dev)  
{
	struct xl_private *xl_priv=netdev_priv(dev);
	int n = xl_priv->rx_ring_tail;
	int prev_ring_loc;

	prev_ring_loc = (n + XL_RX_RING_SIZE - 1) & (XL_RX_RING_SIZE - 1);
	xl_priv->xl_rx_ring[prev_ring_loc].upnextptr = cpu_to_le32(xl_priv->rx_ring_dma_addr + (sizeof (struct xl_rx_desc) * n));
	xl_priv->xl_rx_ring[n].framestatus = 0;
	xl_priv->xl_rx_ring[n].upnextptr = 0;
	xl_priv->rx_ring_tail++;
	xl_priv->rx_ring_tail &= (XL_RX_RING_SIZE-1);
}

static void xl_rx(struct net_device *dev)
{
	struct xl_private *xl_priv=netdev_priv(dev);
	u8 __iomem * xl_mmio = xl_priv->xl_mmio ; 
	struct sk_buff *skb, *skb2 ; 
	int frame_length = 0, copy_len = 0  ; 	
	int temp_ring_loc ;  

 	 
	
	while (xl_priv->xl_rx_ring[xl_priv->rx_ring_tail].framestatus & (RXUPDCOMPLETE | RXUPDFULL) ) { 

		if (xl_priv->xl_rx_ring[xl_priv->rx_ring_tail].framestatus & RXUPDFULL ) { 


			temp_ring_loc = xl_priv->rx_ring_tail ; 

			while (xl_priv->xl_rx_ring[temp_ring_loc].framestatus & RXUPDFULL ) {
				temp_ring_loc++ ; 
				temp_ring_loc &= (XL_RX_RING_SIZE-1) ; 
			}

			frame_length = le32_to_cpu(xl_priv->xl_rx_ring[temp_ring_loc].framestatus) & 0x7FFF;

			skb = dev_alloc_skb(frame_length) ;
 
			if (skb==NULL) { 
				printk(KERN_WARNING "%s: dev_alloc_skb failed - multi buffer !\n", dev->name) ; 
				while (xl_priv->rx_ring_tail != temp_ring_loc)  
					adv_rx_ring(dev) ; 
				
				adv_rx_ring(dev) ;  
				dev->stats.rx_dropped++ ; 

				writel(ACK_INTERRUPT | UPCOMPACK | LATCH_ACK , xl_mmio + MMIO_COMMAND) ; 
				return ; 				
			}
	
			while (xl_priv->rx_ring_tail != temp_ring_loc) { 
				copy_len = le32_to_cpu(xl_priv->xl_rx_ring[xl_priv->rx_ring_tail].upfraglen) & 0x7FFF;
				frame_length -= copy_len ;  
				pci_dma_sync_single_for_cpu(xl_priv->pdev,le32_to_cpu(xl_priv->xl_rx_ring[xl_priv->rx_ring_tail].upfragaddr),xl_priv->pkt_buf_sz,PCI_DMA_FROMDEVICE);
				skb_copy_from_linear_data(xl_priv->rx_ring_skb[xl_priv->rx_ring_tail],
							  skb_put(skb, copy_len),
							  copy_len);
				pci_dma_sync_single_for_device(xl_priv->pdev,le32_to_cpu(xl_priv->xl_rx_ring[xl_priv->rx_ring_tail].upfragaddr),xl_priv->pkt_buf_sz,PCI_DMA_FROMDEVICE);
				adv_rx_ring(dev) ; 
			} 

			
			pci_dma_sync_single_for_cpu(xl_priv->pdev,le32_to_cpu(xl_priv->xl_rx_ring[xl_priv->rx_ring_tail].upfragaddr),xl_priv->pkt_buf_sz,PCI_DMA_FROMDEVICE);
			skb_copy_from_linear_data(xl_priv->rx_ring_skb[xl_priv->rx_ring_tail],
				      skb_put(skb,copy_len), frame_length);
			pci_dma_sync_single_for_device(xl_priv->pdev,le32_to_cpu(xl_priv->xl_rx_ring[xl_priv->rx_ring_tail].upfragaddr),xl_priv->pkt_buf_sz,PCI_DMA_FROMDEVICE);
			adv_rx_ring(dev) ; 
			skb->protocol = tr_type_trans(skb,dev) ; 
			netif_rx(skb) ; 

		} else { 

			frame_length = le32_to_cpu(xl_priv->xl_rx_ring[xl_priv->rx_ring_tail].framestatus) & 0x7FFF;
			
			skb = dev_alloc_skb(xl_priv->pkt_buf_sz) ; 

			if (skb==NULL) { 
				printk(KERN_WARNING "%s: dev_alloc_skb failed in rx, single buffer\n",dev->name);
				adv_rx_ring(dev) ; 
				dev->stats.rx_dropped++ ; 
				writel(ACK_INTERRUPT | UPCOMPACK | LATCH_ACK , xl_mmio + MMIO_COMMAND) ; 
				return ; 
			}

			skb2 = xl_priv->rx_ring_skb[xl_priv->rx_ring_tail] ; 
			pci_unmap_single(xl_priv->pdev, le32_to_cpu(xl_priv->xl_rx_ring[xl_priv->rx_ring_tail].upfragaddr), xl_priv->pkt_buf_sz,PCI_DMA_FROMDEVICE) ;
			skb_put(skb2, frame_length) ; 
			skb2->protocol = tr_type_trans(skb2,dev) ; 

			xl_priv->rx_ring_skb[xl_priv->rx_ring_tail] = skb ; 	
			xl_priv->xl_rx_ring[xl_priv->rx_ring_tail].upfragaddr = cpu_to_le32(pci_map_single(xl_priv->pdev,skb->data,xl_priv->pkt_buf_sz, PCI_DMA_FROMDEVICE));
			xl_priv->xl_rx_ring[xl_priv->rx_ring_tail].upfraglen = cpu_to_le32(xl_priv->pkt_buf_sz) | RXUPLASTFRAG;
			adv_rx_ring(dev) ; 
			dev->stats.rx_packets++ ; 
			dev->stats.rx_bytes += frame_length ; 	

			netif_rx(skb2) ; 		
		 } 
	} 

	
	writel(ACK_INTERRUPT | UPCOMPACK | LATCH_ACK , xl_mmio + MMIO_COMMAND) ; 
	return ; 	
}


static void xl_reset(struct net_device *dev) 
{
	struct xl_private *xl_priv=netdev_priv(dev);
	u8 __iomem * xl_mmio = xl_priv->xl_mmio ; 
	unsigned long t; 

	writew( GLOBAL_RESET, xl_mmio + MMIO_COMMAND ) ; 


	t=jiffies;
	while (readw(xl_mmio + MMIO_INTSTATUS) & INTSTAT_CMD_IN_PROGRESS) { 
		if (time_after(jiffies, t + 40 * HZ)) {
			printk(KERN_ERR "3COM 3C359 Velocity XL  card not responding.\n");
			break ; 
		}
	}
	
}

static void xl_freemem(struct net_device *dev) 
{
	struct xl_private *xl_priv=netdev_priv(dev);
	int i ; 

	for (i=0;i<XL_RX_RING_SIZE;i++) {
		dev_kfree_skb_irq(xl_priv->rx_ring_skb[xl_priv->rx_ring_tail]) ; 
		pci_unmap_single(xl_priv->pdev,le32_to_cpu(xl_priv->xl_rx_ring[xl_priv->rx_ring_tail].upfragaddr),xl_priv->pkt_buf_sz, PCI_DMA_FROMDEVICE);
		xl_priv->rx_ring_tail++ ; 
		xl_priv->rx_ring_tail &= XL_RX_RING_SIZE-1; 
	} 

	
	pci_unmap_single(xl_priv->pdev,xl_priv->rx_ring_dma_addr, sizeof(struct xl_rx_desc) * XL_RX_RING_SIZE, PCI_DMA_FROMDEVICE) ; 
	
	pci_unmap_single(xl_priv->pdev,xl_priv->tx_ring_dma_addr, sizeof(struct xl_tx_desc) * XL_TX_RING_SIZE, PCI_DMA_TODEVICE) ; 

	kfree(xl_priv->xl_rx_ring) ; 
	kfree(xl_priv->xl_tx_ring) ; 

	return  ; 
}

static irqreturn_t xl_interrupt(int irq, void *dev_id) 
{
	struct net_device *dev = (struct net_device *)dev_id;
	struct xl_private *xl_priv =netdev_priv(dev);
	u8 __iomem * xl_mmio = xl_priv->xl_mmio ; 
	u16 intstatus, macstatus  ;

	intstatus = readw(xl_mmio + MMIO_INTSTATUS) ;  

	if (!(intstatus & 1)) 
		return IRQ_NONE;

	spin_lock(&xl_priv->xl_lock) ; 

	if (intstatus == 0x0001) {  
		writel(ACK_INTERRUPT | LATCH_ACK, xl_mmio + MMIO_COMMAND) ;
		printk(KERN_INFO "%s: 00001 int received\n",dev->name);
	} else {  
		if (intstatus &	(HOSTERRINT | SRBRINT | ARBCINT | UPCOMPINT | DNCOMPINT | HARDERRINT | (1<<8) | TXUNDERRUN | ASBFINT)) { 
			

			if (intstatus & HOSTERRINT) {
				printk(KERN_WARNING "%s: Host Error, performing global reset, intstatus = %04x\n",dev->name,intstatus);
				writew( GLOBAL_RESET, xl_mmio + MMIO_COMMAND ) ;
				printk(KERN_WARNING "%s: Resetting hardware:\n", dev->name);
				netif_stop_queue(dev) ;
				xl_freemem(dev) ; 
				free_irq(dev->irq,dev); 	
				xl_reset(dev) ; 
				writel(ACK_INTERRUPT | LATCH_ACK, xl_mmio + MMIO_COMMAND) ; 
				spin_unlock(&xl_priv->xl_lock) ; 
				return IRQ_HANDLED;
			} 

			if (intstatus & SRBRINT ) {  
				writel(ACK_INTERRUPT | SRBRACK | LATCH_ACK, xl_mmio + MMIO_COMMAND) ;
				if (xl_priv->srb_queued)
					xl_srb_bh(dev) ; 
			} 

			if (intstatus & TXUNDERRUN) { 
				writel(DNRESET, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
				while (readw(xl_mmio + MMIO_INTSTATUS) & INTSTAT_CMD_IN_PROGRESS) { 
					
				} 
				printk(KERN_WARNING "%s: TX Underrun received\n",dev->name);
				writel(ACK_INTERRUPT | LATCH_ACK, xl_mmio + MMIO_COMMAND) ; 
			} 
	
			if (intstatus & ARBCINT ) { 
				xl_arb_cmd(dev) ; 
			} 

			if (intstatus & ASBFINT) { 
				if (xl_priv->asb_queued == 1) {
					xl_asb_cmd(dev) ; 
				} else if (xl_priv->asb_queued == 2) {
					xl_asb_bh(dev) ; 
				} else { 
					writel(ACK_INTERRUPT | LATCH_ACK | ASBFACK, xl_mmio + MMIO_COMMAND) ; 
				}  
			} 

			if (intstatus & UPCOMPINT ) 
				xl_rx(dev) ; 

			if (intstatus & DNCOMPINT )  
				xl_dn_comp(dev) ; 

			if (intstatus & HARDERRINT ) { 
				writel(MMIO_WORD_READ | MACSTATUS, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
				macstatus = readw(xl_mmio + MMIO_MACDATA) ; 
				printk(KERN_WARNING "%s: MacStatusError, details: ", dev->name);
				if (macstatus & (1<<14)) 
					printk(KERN_WARNING "tchk error: Unrecoverable error\n");
				if (macstatus & (1<<3))
					printk(KERN_WARNING "eint error: Internal watchdog timer expired\n");
				if (macstatus & (1<<2))
					printk(KERN_WARNING "aint error: Host tried to perform invalid operation\n");
				printk(KERN_WARNING "Instatus = %02x, macstatus = %02x\n",intstatus,macstatus) ; 
				printk(KERN_WARNING "%s: Resetting hardware:\n", dev->name);
				netif_stop_queue(dev) ;
				xl_freemem(dev) ; 
				free_irq(dev->irq,dev); 
				unregister_netdev(dev) ; 
				free_netdev(dev) ;  
				xl_reset(dev) ; 
				writel(ACK_INTERRUPT | LATCH_ACK, xl_mmio + MMIO_COMMAND) ; 
				spin_unlock(&xl_priv->xl_lock) ; 
				return IRQ_HANDLED;
			}
		} else { 
			printk(KERN_WARNING "%s: Received Unknown interrupt : %04x\n", dev->name, intstatus);
			writel(ACK_INTERRUPT | LATCH_ACK, xl_mmio + MMIO_COMMAND) ; 	
		}
	} 

	

	writel( SETINDENABLE | INT_MASK, xl_mmio + MMIO_COMMAND) ; 
	writel( SETINTENABLE | INT_MASK, xl_mmio + MMIO_COMMAND) ; 

	spin_unlock(&xl_priv->xl_lock) ;
	return IRQ_HANDLED;
}	

	
static netdev_tx_t xl_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct xl_private *xl_priv=netdev_priv(dev);
	struct xl_tx_desc *txd ; 
	int tx_head, tx_tail, tx_prev ; 
	unsigned long flags ; 	

	spin_lock_irqsave(&xl_priv->xl_lock,flags) ; 

	netif_stop_queue(dev) ; 

	if (xl_priv->free_ring_entries > 1 ) { 	
		tx_head = xl_priv->tx_ring_head ; 
		tx_tail = xl_priv->tx_ring_tail ; 

		txd = &(xl_priv->xl_tx_ring[tx_head]) ; 
		txd->dnnextptr = 0 ; 
		txd->framestartheader = cpu_to_le32(skb->len) | TXDNINDICATE;
		txd->buffer = cpu_to_le32(pci_map_single(xl_priv->pdev, skb->data, skb->len, PCI_DMA_TODEVICE));
		txd->buffer_length = cpu_to_le32(skb->len) | TXDNFRAGLAST;
		xl_priv->tx_ring_skb[tx_head] = skb ; 
		dev->stats.tx_packets++ ; 
		dev->stats.tx_bytes += skb->len ;

 
	
		tx_prev = (xl_priv->tx_ring_head + XL_TX_RING_SIZE - 1) & (XL_TX_RING_SIZE - 1) ; 

		xl_priv->tx_ring_head++ ; 
		xl_priv->tx_ring_head &= (XL_TX_RING_SIZE - 1) ;
		xl_priv->free_ring_entries-- ; 

		xl_priv->xl_tx_ring[tx_prev].dnnextptr = cpu_to_le32(xl_priv->tx_ring_dma_addr + (sizeof (struct xl_tx_desc) * tx_head));

		
		

		netif_wake_queue(dev) ; 

		spin_unlock_irqrestore(&xl_priv->xl_lock,flags) ; 
 
		return NETDEV_TX_OK;
	} else {
		spin_unlock_irqrestore(&xl_priv->xl_lock,flags) ; 
		return NETDEV_TX_BUSY;
	}

}
	

static void xl_dn_comp(struct net_device *dev) 
{
	struct xl_private *xl_priv=netdev_priv(dev);
	u8 __iomem * xl_mmio = xl_priv->xl_mmio ; 
	struct xl_tx_desc *txd ; 


	if (xl_priv->tx_ring_tail == 255) {
		xl_priv->xl_tx_ring[0].framestartheader = 0 ; 
		xl_priv->xl_tx_ring[0].dnnextptr = 0 ;  
		xl_priv->tx_ring_tail = 1 ; 
	}

	while (xl_priv->xl_tx_ring[xl_priv->tx_ring_tail].framestartheader & TXDNCOMPLETE ) { 
		txd = &(xl_priv->xl_tx_ring[xl_priv->tx_ring_tail]) ;
		pci_unmap_single(xl_priv->pdev, le32_to_cpu(txd->buffer), xl_priv->tx_ring_skb[xl_priv->tx_ring_tail]->len, PCI_DMA_TODEVICE);
		txd->framestartheader = 0 ; 
		txd->buffer = cpu_to_le32(0xdeadbeef);
		txd->buffer_length  = 0 ;  
		dev_kfree_skb_irq(xl_priv->tx_ring_skb[xl_priv->tx_ring_tail]) ;
		xl_priv->tx_ring_tail++ ; 
		xl_priv->tx_ring_tail &= (XL_TX_RING_SIZE - 1) ; 
		xl_priv->free_ring_entries++ ; 
	}

	netif_wake_queue(dev) ; 

	writel(ACK_INTERRUPT | DNCOMPACK | LATCH_ACK , xl_mmio + MMIO_COMMAND) ; 
}


static int xl_close(struct net_device *dev) 
{
	struct xl_private *xl_priv = netdev_priv(dev);
	u8 __iomem * xl_mmio = xl_priv->xl_mmio ; 
	unsigned long t ; 

	netif_stop_queue(dev) ; 


    	writew(DNSTALL, xl_mmio + MMIO_COMMAND) ; 
	t=jiffies;
	while (readw(xl_mmio + MMIO_INTSTATUS) & INTSTAT_CMD_IN_PROGRESS) { 
		schedule();		
		if (time_after(jiffies, t + 10 * HZ)) {
			printk(KERN_ERR "%s: 3COM 3C359 Velocity XL-DNSTALL not responding.\n", dev->name);
			break ; 
		}
	}
    	writew(DNDISABLE, xl_mmio + MMIO_COMMAND) ; 
	t=jiffies;
	while (readw(xl_mmio + MMIO_INTSTATUS) & INTSTAT_CMD_IN_PROGRESS) { 
		schedule();		
		if (time_after(jiffies, t + 10 * HZ)) {
			printk(KERN_ERR "%s: 3COM 3C359 Velocity XL-DNDISABLE not responding.\n", dev->name);
			break ;
		}
	}
    	writew(UPSTALL, xl_mmio + MMIO_COMMAND) ; 
	t=jiffies;
	while (readw(xl_mmio + MMIO_INTSTATUS) & INTSTAT_CMD_IN_PROGRESS) { 
		schedule();		
		if (time_after(jiffies, t + 10 * HZ)) {
			printk(KERN_ERR "%s: 3COM 3C359 Velocity XL-UPSTALL not responding.\n", dev->name);
			break ; 
		}
	}


	writel(SETINTENABLE, xl_mmio + MMIO_COMMAND) ; 

	xl_srb_cmd(dev,CLOSE_NIC) ; 

	t=jiffies;
	while (!(readw(xl_mmio + MMIO_INTSTATUS) & INTSTAT_SRB)) { 
		schedule();		
		if (time_after(jiffies, t + 10 * HZ)) {
			printk(KERN_ERR "%s: 3COM 3C359 Velocity XL-CLOSENIC not responding.\n", dev->name);
			break ; 
		}
	}
	

	writel(MEM_BYTE_READ | 0xd0000 | xl_priv->srb, xl_mmio + MMIO_MAC_ACCESS_CMD);
	if (readb(xl_mmio + MMIO_MACDATA) != CLOSE_NIC) { 
		printk(KERN_INFO "%s: CLOSE_NIC did not get a CLOSE_NIC response\n",dev->name);
	} else { 
		writel((MEM_BYTE_READ | 0xd0000 | xl_priv->srb) +2, xl_mmio + MMIO_MAC_ACCESS_CMD) ;
		if (readb(xl_mmio + MMIO_MACDATA)==0) { 
			printk(KERN_INFO "%s: Adapter has been closed\n",dev->name);
			writew(ACK_INTERRUPT | SRBRACK | LATCH_ACK, xl_mmio + MMIO_COMMAND) ; 

			xl_freemem(dev) ; 
			free_irq(dev->irq,dev) ; 
		} else { 
			printk(KERN_INFO "%s: Close nic command returned error code %02x\n",dev->name, readb(xl_mmio + MMIO_MACDATA)) ;
		} 
	}

	
 
    	writew(UPRESET, xl_mmio + MMIO_COMMAND) ; 
	t=jiffies;
	while (readw(xl_mmio + MMIO_INTSTATUS) & INTSTAT_CMD_IN_PROGRESS) { 
		schedule();		
		if (time_after(jiffies, t + 10 * HZ)) {
			printk(KERN_ERR "%s: 3COM 3C359 Velocity XL-UPRESET not responding.\n", dev->name);
			break ; 
		}
	}
    	writew(DNRESET, xl_mmio + MMIO_COMMAND) ; 
	t=jiffies;
	while (readw(xl_mmio + MMIO_INTSTATUS) & INTSTAT_CMD_IN_PROGRESS) { 
		schedule();		
		if (time_after(jiffies, t + 10 * HZ)) {
			printk(KERN_ERR "%s: 3COM 3C359 Velocity XL-DNRESET not responding.\n", dev->name);
			break ; 
		}
	}
	xl_hw_reset(dev) ; 
	return 0 ;
}

static void xl_set_rx_mode(struct net_device *dev) 
{
	struct xl_private *xl_priv = netdev_priv(dev);
	struct netdev_hw_addr *ha;
	unsigned char dev_mc_address[4] ; 
	u16 options ; 

	if (dev->flags & IFF_PROMISC)
		options = 0x0004 ; 
	else
		options = 0x0000 ; 

	if (options ^ xl_priv->xl_copy_all_options) { 
		xl_priv->xl_copy_all_options = options ; 
		xl_srb_cmd(dev, SET_RECEIVE_MODE) ;
		return ;  
	}

	dev_mc_address[0] = dev_mc_address[1] = dev_mc_address[2] = dev_mc_address[3] = 0 ;

	netdev_for_each_mc_addr(ha, dev) {
		dev_mc_address[0] |= ha->addr[2];
		dev_mc_address[1] |= ha->addr[3];
		dev_mc_address[2] |= ha->addr[4];
		dev_mc_address[3] |= ha->addr[5];
        }

	if (memcmp(xl_priv->xl_functional_addr,dev_mc_address,4) != 0) { 
		memcpy(xl_priv->xl_functional_addr, dev_mc_address,4) ; 
		xl_srb_cmd(dev, SET_FUNC_ADDRESS) ; 
	}
	return ; 
}



static void xl_srb_bh(struct net_device *dev) 
{ 
	struct xl_private *xl_priv = netdev_priv(dev);
	u8 __iomem * xl_mmio = xl_priv->xl_mmio ; 
	u8 srb_cmd, ret_code ; 
	int i ; 

	writel(MEM_BYTE_READ | 0xd0000 | xl_priv->srb, xl_mmio + MMIO_MAC_ACCESS_CMD) ;
	srb_cmd = readb(xl_mmio + MMIO_MACDATA) ; 
	writel((MEM_BYTE_READ | 0xd0000 | xl_priv->srb) +2, xl_mmio + MMIO_MAC_ACCESS_CMD) ;
	ret_code = readb(xl_mmio + MMIO_MACDATA) ; 

	

	switch (ret_code) { 
	case 1:
		printk(KERN_INFO "%s: Command: %d - Invalid Command code\n",dev->name,srb_cmd) ; 
		break ; 
	case 4:
		printk(KERN_INFO "%s: Command: %d - Adapter is closed, must be open for this command\n",dev->name,srb_cmd);
		break ;
	
	case 6:
		printk(KERN_INFO "%s: Command: %d - Options Invalid for command\n",dev->name,srb_cmd);
		break ;

	case 0:  
		switch (srb_cmd) { 
		case READ_LOG: 
			if(xl_priv->xl_message_level)
				printk(KERN_INFO "%s: READ.LOG 14 bytes of data ",dev->name) ; 
			for (i=0;i<14;i++) { 
				writel(MEM_BYTE_READ | 0xd0000 | xl_priv->srb | i, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
				if(xl_priv->xl_message_level) 
					printk("%02x:",readb(xl_mmio + MMIO_MACDATA)) ; 	
			} 
			printk("\n") ; 
			break ; 
		case SET_FUNC_ADDRESS:
			if(xl_priv->xl_message_level) 
				printk(KERN_INFO "%s: Functional Address Set\n",dev->name);
			break ; 
		case CLOSE_NIC:
			if(xl_priv->xl_message_level)
				printk(KERN_INFO "%s: Received CLOSE_NIC interrupt in interrupt handler\n",dev->name);
			break ; 
		case SET_MULTICAST_MODE:
			if(xl_priv->xl_message_level)
				printk(KERN_INFO "%s: Multicast options successfully changed\n",dev->name) ; 
			break ;
		case SET_RECEIVE_MODE:
			if(xl_priv->xl_message_level) {  
				if (xl_priv->xl_copy_all_options == 0x0004) 
					printk(KERN_INFO "%s: Entering promiscuous mode\n", dev->name);
				else
					printk(KERN_INFO "%s: Entering normal receive mode\n",dev->name);
			}
			break ; 
 
		} 
		break ; 
	} 
	return ; 	
} 

static int xl_set_mac_address (struct net_device *dev, void *addr) 
{
	struct sockaddr *saddr = addr ; 
	struct xl_private *xl_priv = netdev_priv(dev);

	if (netif_running(dev)) { 
		printk(KERN_WARNING "%s: Cannot set mac/laa address while card is open\n", dev->name) ; 
		return -EIO ; 
	}

	memcpy(xl_priv->xl_laa, saddr->sa_data,dev->addr_len) ; 
	
	if (xl_priv->xl_message_level) { 
 		printk(KERN_INFO "%s: MAC/LAA Set to  = %x.%x.%x.%x.%x.%x\n",dev->name, xl_priv->xl_laa[0],
		xl_priv->xl_laa[1], xl_priv->xl_laa[2],
		xl_priv->xl_laa[3], xl_priv->xl_laa[4],
		xl_priv->xl_laa[5]);
	} 

	return 0 ; 
}

static void xl_arb_cmd(struct net_device *dev)
{
	struct xl_private *xl_priv = netdev_priv(dev);
	u8 __iomem * xl_mmio = xl_priv->xl_mmio ; 
	u8 arb_cmd ; 
	u16 lan_status, lan_status_diff ; 

	writel( ( MEM_BYTE_READ | 0xD0000 | xl_priv->arb), xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	arb_cmd = readb(xl_mmio + MMIO_MACDATA) ; 
	
	if (arb_cmd == RING_STATUS_CHANGE) { 
		writel( ( (MEM_WORD_READ | 0xD0000 | xl_priv->arb) + 6), xl_mmio + MMIO_MAC_ACCESS_CMD) ;
		 
		printk(KERN_INFO "%s: Ring Status Change: New Status = %04x\n", dev->name, swab16(readw(xl_mmio + MMIO_MACDATA) )) ;

		lan_status = swab16(readw(xl_mmio + MMIO_MACDATA));
	
		
		writel(ACK_INTERRUPT | ARBCACK | LATCH_ACK, xl_mmio + MMIO_COMMAND) ; 
			
		lan_status_diff = xl_priv->xl_lan_status ^ lan_status ; 

		if (lan_status_diff & (LSC_LWF | LSC_ARW | LSC_FPE | LSC_RR) ) { 
			if (lan_status_diff & LSC_LWF) 
				printk(KERN_WARNING "%s: Short circuit detected on the lobe\n",dev->name);
			if (lan_status_diff & LSC_ARW) 
				printk(KERN_WARNING "%s: Auto removal error\n",dev->name);
			if (lan_status_diff & LSC_FPE)
				printk(KERN_WARNING "%s: FDX Protocol Error\n",dev->name);
			if (lan_status_diff & LSC_RR) 
				printk(KERN_WARNING "%s: Force remove MAC frame received\n",dev->name);
		
			

			netif_stop_queue(dev);
			xl_freemem(dev) ; 
			free_irq(dev->irq,dev);
			
			printk(KERN_WARNING "%s: Adapter has been closed\n", dev->name);
		} 
		
		if (xl_priv->xl_message_level) { 
			if (lan_status_diff & LSC_SIG_LOSS) 
					printk(KERN_WARNING "%s: No receive signal detected\n", dev->name);
			if (lan_status_diff & LSC_HARD_ERR)
					printk(KERN_INFO "%s: Beaconing\n",dev->name);
			if (lan_status_diff & LSC_SOFT_ERR)
					printk(KERN_WARNING "%s: Adapter transmitted Soft Error Report Mac Frame\n",dev->name);
			if (lan_status_diff & LSC_TRAN_BCN) 
					printk(KERN_INFO "%s: We are transmitting the beacon, aaah\n",dev->name);
			if (lan_status_diff & LSC_SS) 
					printk(KERN_INFO "%s: Single Station on the ring\n", dev->name);
			if (lan_status_diff & LSC_RING_REC)
					printk(KERN_INFO "%s: Ring recovery ongoing\n",dev->name);
			if (lan_status_diff & LSC_FDX_MODE)
					printk(KERN_INFO "%s: Operating in FDX mode\n",dev->name);
		} 	
		
		if (lan_status_diff & LSC_CO) { 
				if (xl_priv->xl_message_level) 
					printk(KERN_INFO "%s: Counter Overflow\n", dev->name);
				
				xl_srb_cmd(dev, READ_LOG) ; 	
		}

		
		if (lan_status_diff & LSC_SR_CO) { 
			if (xl_priv->xl_message_level)
				printk(KERN_INFO "%s: Source routing counters overflow\n", dev->name);
		}

		xl_priv->xl_lan_status = lan_status ; 
	
	}  
	else if ( arb_cmd == RECEIVE_DATA) { 
#if XL_DEBUG
		printk(KERN_INFO "Received.Data\n");
#endif 		
		writel( ((MEM_WORD_READ | 0xD0000 | xl_priv->arb) + 6), xl_mmio + MMIO_MAC_ACCESS_CMD) ;
		xl_priv->mac_buffer = swab16(readw(xl_mmio + MMIO_MACDATA)) ;
		

		
		writel(ACK_INTERRUPT | ARBCACK | LATCH_ACK, xl_mmio + MMIO_COMMAND) ; 

		 	
			
		xl_priv->asb_queued = 0 ; 			
		writel( ((MEM_BYTE_READ | 0xD0000 | xl_priv->asb) + 2), xl_mmio + MMIO_MAC_ACCESS_CMD) ;
		if (readb(xl_mmio + MMIO_MACDATA) != 0xff) { 
			xl_priv->asb_queued = 1 ;

			xl_wait_misr_flags(dev) ;  

			writel(MEM_BYTE_WRITE | MF_ASBFR, xl_mmio + MMIO_MAC_ACCESS_CMD); 
			writeb(0xff, xl_mmio + MMIO_MACDATA) ;
			writel(MMIO_BYTE_WRITE | MISR_SET, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
			writeb(MISR_ASBFR, xl_mmio + MMIO_MACDATA) ; 
			return ; 	
			
		}
	
		xl_asb_cmd(dev) ; 
		
	} else {
		printk(KERN_WARNING "%s: Received unknown arb (xl_priv) command: %02x\n",dev->name,arb_cmd);
	}

	

	writel(ACK_INTERRUPT | ARBCACK | LATCH_ACK , xl_mmio + MMIO_COMMAND) ; 

	return ; 
}



static void xl_asb_cmd(struct net_device *dev)
{
	struct xl_private *xl_priv = netdev_priv(dev);
	u8 __iomem * xl_mmio = xl_priv->xl_mmio ; 

	if (xl_priv->asb_queued == 1) 
		writel(ACK_INTERRUPT | LATCH_ACK | ASBFACK, xl_mmio + MMIO_COMMAND) ; 
		
	writel(MEM_BYTE_WRITE | 0xd0000 | xl_priv->asb, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writeb(0x81, xl_mmio + MMIO_MACDATA) ; 

	writel(MEM_WORD_WRITE | 0xd0000 | xl_priv->asb | 6, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writew(swab16(xl_priv->mac_buffer), xl_mmio + MMIO_MACDATA) ;

	xl_wait_misr_flags(dev) ; 	

	writel(MEM_BYTE_WRITE | MF_RASB, xl_mmio + MMIO_MAC_ACCESS_CMD); 
	writeb(0xff, xl_mmio + MMIO_MACDATA) ;

	writel(MMIO_BYTE_WRITE | MISR_SET, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writeb(MISR_RASB, xl_mmio + MMIO_MACDATA) ; 

	xl_priv->asb_queued = 2 ; 

	return ; 
}

static void xl_asb_bh(struct net_device *dev) 
{
	struct xl_private *xl_priv = netdev_priv(dev);
	u8 __iomem * xl_mmio = xl_priv->xl_mmio ; 
	u8 ret_code ; 

	writel(MMIO_BYTE_READ | 0xd0000 | xl_priv->asb | 2, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	ret_code = readb(xl_mmio + MMIO_MACDATA) ; 
	switch (ret_code) { 
		case 0x01:
			printk(KERN_INFO "%s: ASB Command, unrecognized command code\n",dev->name);
			break ;
		case 0x26:
			printk(KERN_INFO "%s: ASB Command, unexpected receive buffer\n", dev->name);
			break ; 
		case 0x40:
			printk(KERN_INFO "%s: ASB Command, Invalid Station ID\n", dev->name);
			break ;  
	}
	xl_priv->asb_queued = 0 ; 
	writel(ACK_INTERRUPT | LATCH_ACK | ASBFACK, xl_mmio + MMIO_COMMAND) ;
	return ;  
}


static void xl_srb_cmd(struct net_device *dev, int srb_cmd) 
{
	struct xl_private *xl_priv = netdev_priv(dev);
	u8 __iomem * xl_mmio = xl_priv->xl_mmio ; 

	switch (srb_cmd) { 
	case READ_LOG:
		writel(MEM_BYTE_WRITE | 0xD0000 | xl_priv->srb, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		writeb(READ_LOG, xl_mmio + MMIO_MACDATA) ; 
		break; 

	case CLOSE_NIC:
		writel(MEM_BYTE_WRITE | 0xD0000 | xl_priv->srb, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		writeb(CLOSE_NIC, xl_mmio + MMIO_MACDATA) ; 
		break ;

	case SET_RECEIVE_MODE:
		writel(MEM_BYTE_WRITE | 0xD0000 | xl_priv->srb, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		writeb(SET_RECEIVE_MODE, xl_mmio + MMIO_MACDATA) ; 
		writel(MEM_WORD_WRITE | 0xD0000 | xl_priv->srb | 4, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		writew(xl_priv->xl_copy_all_options, xl_mmio + MMIO_MACDATA) ; 
		break ;

	case SET_FUNC_ADDRESS:
		writel(MEM_BYTE_WRITE | 0xD0000 | xl_priv->srb, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		writeb(SET_FUNC_ADDRESS, xl_mmio + MMIO_MACDATA) ; 
		writel(MEM_BYTE_WRITE | 0xD0000 | xl_priv->srb | 6 , xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		writeb(xl_priv->xl_functional_addr[0], xl_mmio + MMIO_MACDATA) ; 
		writel(MEM_BYTE_WRITE | 0xD0000 | xl_priv->srb | 7 , xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		writeb(xl_priv->xl_functional_addr[1], xl_mmio + MMIO_MACDATA) ; 
		writel(MEM_BYTE_WRITE | 0xD0000 | xl_priv->srb | 8 , xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		writeb(xl_priv->xl_functional_addr[2], xl_mmio + MMIO_MACDATA) ; 
		writel(MEM_BYTE_WRITE | 0xD0000 | xl_priv->srb | 9 , xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
		writeb(xl_priv->xl_functional_addr[3], xl_mmio + MMIO_MACDATA) ;
		break ;  
	} 


	xl_wait_misr_flags(dev)  ; 

	
	writel(MEM_BYTE_WRITE | MF_CSRB , xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writeb(0xFF, xl_mmio + MMIO_MACDATA) ; 
	
	writel(MMIO_BYTE_WRITE | MISR_SET, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writeb(MISR_CSRB, xl_mmio + MMIO_MACDATA) ; 
	xl_priv->srb_queued = 1 ; 

	return ; 
}


static void xl_wait_misr_flags(struct net_device *dev) 
{
	struct xl_private *xl_priv = netdev_priv(dev);
	u8 __iomem * xl_mmio = xl_priv->xl_mmio ; 
	
	int i  ; 
	
	writel(MMIO_BYTE_READ | MISR_RW, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	if (readb(xl_mmio + MMIO_MACDATA) != 0) {  
		for (i=0; i<6; i++) { 
			writel(MEM_BYTE_READ | 0xDFFE0 | i, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
			while (readb(xl_mmio + MMIO_MACDATA) != 0) {
				;	
			}
		} 
	}

	writel(MMIO_BYTE_WRITE | MISR_AND, xl_mmio + MMIO_MAC_ACCESS_CMD) ; 
	writeb(0x80, xl_mmio + MMIO_MACDATA) ; 

	return ; 
} 


static int xl_change_mtu(struct net_device *dev, int mtu) 
{
	struct xl_private *xl_priv = netdev_priv(dev);
	u16 max_mtu ; 

	if (xl_priv->xl_ring_speed == 4)
		max_mtu = 4500 ; 
	else
		max_mtu = 18000 ; 
	
	if (mtu > max_mtu)
		return -EINVAL ; 
	if (mtu < 100) 
		return -EINVAL ; 

	dev->mtu = mtu ; 
	xl_priv->pkt_buf_sz = mtu + TR_HLEN ; 

	return 0 ; 
}

static void __devexit xl_remove_one (struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct xl_private *xl_priv=netdev_priv(dev);
	
	release_firmware(xl_priv->fw);
	unregister_netdev(dev);
	iounmap(xl_priv->xl_mmio) ; 
	pci_release_regions(pdev) ; 
	pci_set_drvdata(pdev,NULL) ; 
	free_netdev(dev);
	return ; 
}

static struct pci_driver xl_3c359_driver = {
	.name		= "3c359",
	.id_table	= xl_pci_tbl,
	.probe		= xl_probe,
	.remove		= __devexit_p(xl_remove_one),
};

static int __init xl_pci_init (void)
{
	return pci_register_driver(&xl_3c359_driver);
}


static void __exit xl_pci_cleanup (void)
{
	pci_unregister_driver (&xl_3c359_driver);
}

module_init(xl_pci_init);
module_exit(xl_pci_cleanup);

MODULE_LICENSE("GPL") ; 
