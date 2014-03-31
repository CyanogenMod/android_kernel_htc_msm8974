/*
 * xircom_cb: A driver for the (tulip-like) Xircom Cardbus ethernet cards
 *
 * This software is (C) by the respective authors, and licensed under the GPL
 * License.
 *
 * Written by Arjan van de Ven for Red Hat, Inc.
 * Based on work by Jeff Garzik, Doug Ledford and Donald Becker
 *
 *  	This software may be used and distributed according to the terms
 *      of the GNU General Public License, incorporated herein by reference.
 *
 *
 * 	$Id: xircom_cb.c,v 1.33 2001/03/19 14:02:07 arjanv Exp $
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/bitops.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#ifdef CONFIG_NET_POLL_CONTROLLER
#include <asm/irq.h>
#endif

MODULE_DESCRIPTION("Xircom Cardbus ethernet driver");
MODULE_AUTHOR("Arjan van de Ven <arjanv@redhat.com>");
MODULE_LICENSE("GPL");



#define CSR0	0x00
#define CSR1	0x08
#define CSR2	0x10
#define CSR3	0x18
#define CSR4	0x20
#define CSR5	0x28
#define CSR6	0x30
#define CSR7	0x38
#define CSR8	0x40
#define CSR9	0x48
#define CSR10	0x50
#define CSR11	0x58
#define CSR12	0x60
#define CSR13	0x68
#define CSR14	0x70
#define CSR15	0x78
#define CSR16	0x80

#define PCI_POWERMGMT 	0x40


#define NUMDESCRIPTORS 4

static int bufferoffsets[NUMDESCRIPTORS] = {128,2048,4096,6144};


struct xircom_private {
	

	__le32 *rx_buffer;
	__le32 *tx_buffer;

	dma_addr_t rx_dma_handle;
	dma_addr_t tx_dma_handle;

	struct sk_buff *tx_skb[4];

	unsigned long io_port;
	int open;

	int transmit_used;

	spinlock_t lock;

	struct pci_dev *pdev;
	struct net_device *dev;
};


static int xircom_probe(struct pci_dev *pdev, const struct pci_device_id *id);
static void xircom_remove(struct pci_dev *pdev);
static irqreturn_t xircom_interrupt(int irq, void *dev_instance);
static netdev_tx_t xircom_start_xmit(struct sk_buff *skb,
					   struct net_device *dev);
static int xircom_open(struct net_device *dev);
static int xircom_close(struct net_device *dev);
static void xircom_up(struct xircom_private *card);
#ifdef CONFIG_NET_POLL_CONTROLLER
static void xircom_poll_controller(struct net_device *dev);
#endif

static void investigate_read_descriptor(struct net_device *dev,struct xircom_private *card, int descnr, unsigned int bufferoffset);
static void investigate_write_descriptor(struct net_device *dev, struct xircom_private *card, int descnr, unsigned int bufferoffset);
static void read_mac_address(struct xircom_private *card);
static void transceiver_voodoo(struct xircom_private *card);
static void initialize_card(struct xircom_private *card);
static void trigger_transmit(struct xircom_private *card);
static void trigger_receive(struct xircom_private *card);
static void setup_descriptors(struct xircom_private *card);
static void remove_descriptors(struct xircom_private *card);
static int link_status_changed(struct xircom_private *card);
static void activate_receiver(struct xircom_private *card);
static void deactivate_receiver(struct xircom_private *card);
static void activate_transmitter(struct xircom_private *card);
static void deactivate_transmitter(struct xircom_private *card);
static void enable_transmit_interrupt(struct xircom_private *card);
static void enable_receive_interrupt(struct xircom_private *card);
static void enable_link_interrupt(struct xircom_private *card);
static void disable_all_interrupts(struct xircom_private *card);
static int link_status(struct xircom_private *card);



static DEFINE_PCI_DEVICE_TABLE(xircom_pci_table) = {
	{0x115D, 0x0003, PCI_ANY_ID, PCI_ANY_ID,},
	{0,},
};
MODULE_DEVICE_TABLE(pci, xircom_pci_table);

static struct pci_driver xircom_ops = {
	.name		= "xircom_cb",
	.id_table	= xircom_pci_table,
	.probe		= xircom_probe,
	.remove		= xircom_remove,
	.suspend =NULL,
	.resume =NULL
};


#if defined DEBUG && DEBUG > 1
static void print_binary(unsigned int number)
{
	int i,i2;
	char buffer[64];
	memset(buffer,0,64);
	i2=0;
	for (i=31;i>=0;i--) {
		if (number & (1<<i))
			buffer[i2++]='1';
		else
			buffer[i2++]='0';
		if ((i&3)==0)
			buffer[i2++]=' ';
	}
	pr_debug("%s\n",buffer);
}
#endif

static const struct net_device_ops netdev_ops = {
	.ndo_open		= xircom_open,
	.ndo_stop		= xircom_close,
	.ndo_start_xmit		= xircom_start_xmit,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_set_mac_address	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= xircom_poll_controller,
#endif
};

static int __devinit xircom_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct net_device *dev = NULL;
	struct xircom_private *private;
	unsigned long flags;
	unsigned short tmp16;

	

	if (pci_enable_device(pdev))
		return -ENODEV;

	
	pci_write_config_dword(pdev, PCI_POWERMGMT, 0x0000);

	pci_set_master(pdev); 

	
	pci_read_config_word (pdev,PCI_STATUS, &tmp16);
	pci_write_config_word (pdev, PCI_STATUS,tmp16);

	if (!request_region(pci_resource_start(pdev, 0), 128, "xircom_cb")) {
		pr_err("%s: failed to allocate io-region\n", __func__);
		return -ENODEV;
	}

	dev = alloc_etherdev(sizeof(struct xircom_private));
	if (!dev)
		goto device_fail;

	private = netdev_priv(dev);

	
	private->rx_buffer = pci_alloc_consistent(pdev,8192,&private->rx_dma_handle);
	if (private->rx_buffer == NULL) {
		pr_err("%s: no memory for rx buffer\n", __func__);
		goto rx_buf_fail;
	}
	private->tx_buffer = pci_alloc_consistent(pdev,8192,&private->tx_dma_handle);
	if (private->tx_buffer == NULL) {
		pr_err("%s: no memory for tx buffer\n", __func__);
		goto tx_buf_fail;
	}

	SET_NETDEV_DEV(dev, &pdev->dev);


	private->dev = dev;
	private->pdev = pdev;
	private->io_port = pci_resource_start(pdev, 0);
	spin_lock_init(&private->lock);
	dev->irq = pdev->irq;
	dev->base_addr = private->io_port;

	initialize_card(private);
	read_mac_address(private);
	setup_descriptors(private);

	dev->netdev_ops = &netdev_ops;
	pci_set_drvdata(pdev, dev);

	if (register_netdev(dev)) {
		pr_err("%s: netdevice registration failed\n", __func__);
		goto reg_fail;
	}

	netdev_info(dev, "Xircom cardbus revision %i at irq %i\n",
		    pdev->revision, pdev->irq);
	
	
	transceiver_voodoo(private);

	spin_lock_irqsave(&private->lock,flags);
	activate_transmitter(private);
	activate_receiver(private);
	spin_unlock_irqrestore(&private->lock,flags);

	trigger_receive(private);

	return 0;

reg_fail:
	kfree(private->tx_buffer);
tx_buf_fail:
	kfree(private->rx_buffer);
rx_buf_fail:
	free_netdev(dev);
device_fail:
	return -ENODEV;
}


static void __devexit xircom_remove(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct xircom_private *card = netdev_priv(dev);

	pci_free_consistent(pdev,8192,card->rx_buffer,card->rx_dma_handle);
	pci_free_consistent(pdev,8192,card->tx_buffer,card->tx_dma_handle);

	release_region(dev->base_addr, 128);
	unregister_netdev(dev);
	free_netdev(dev);
	pci_set_drvdata(pdev, NULL);
}

static irqreturn_t xircom_interrupt(int irq, void *dev_instance)
{
	struct net_device *dev = (struct net_device *) dev_instance;
	struct xircom_private *card = netdev_priv(dev);
	unsigned int status;
	int i;

	spin_lock(&card->lock);
	status = inl(card->io_port+CSR5);

#if defined DEBUG && DEBUG > 1
	print_binary(status);
	pr_debug("tx status 0x%08x 0x%08x\n",
		 card->tx_buffer[0], card->tx_buffer[4]);
	pr_debug("rx status 0x%08x 0x%08x\n",
		 card->rx_buffer[0], card->rx_buffer[4]);
#endif
	
	if (status == 0 || status == 0xffffffff) {
		spin_unlock(&card->lock);
		return IRQ_NONE;
	}

	if (link_status_changed(card)) {
		int newlink;
		netdev_dbg(dev, "Link status has changed\n");
		newlink = link_status(card);
		netdev_info(dev, "Link is %d mbit\n", newlink);
		if (newlink)
			netif_carrier_on(dev);
		else
			netif_carrier_off(dev);

	}

	
	status |= 0xffffffff; 
	outl(status,card->io_port+CSR5);


	for (i=0;i<NUMDESCRIPTORS;i++)
		investigate_write_descriptor(dev,card,i,bufferoffsets[i]);
	for (i=0;i<NUMDESCRIPTORS;i++)
		investigate_read_descriptor(dev,card,i,bufferoffsets[i]);

	spin_unlock(&card->lock);
	return IRQ_HANDLED;
}

static netdev_tx_t xircom_start_xmit(struct sk_buff *skb,
					   struct net_device *dev)
{
	struct xircom_private *card;
	unsigned long flags;
	int nextdescriptor;
	int desc;

	card = netdev_priv(dev);
	spin_lock_irqsave(&card->lock,flags);

	
	for (desc=0;desc<NUMDESCRIPTORS;desc++)
		investigate_write_descriptor(dev,card,desc,bufferoffsets[desc]);


	nextdescriptor = (card->transmit_used +1) % (NUMDESCRIPTORS);
	desc = card->transmit_used;

	
	if (card->tx_buffer[4*desc]==0) {

			memset(&card->tx_buffer[bufferoffsets[desc]/4],0,1536);
			skb_copy_from_linear_data(skb,
				  &(card->tx_buffer[bufferoffsets[desc] / 4]),
						  skb->len);

			card->tx_buffer[4*desc+1] = cpu_to_le32(skb->len);
			if (desc == NUMDESCRIPTORS - 1) 
				card->tx_buffer[4*desc+1] |= cpu_to_le32(1<<25);  

			card->tx_buffer[4*desc+1] |= cpu_to_le32(0xF0000000);
						 
			card->tx_skb[desc] = skb;

			wmb();
			
			card->tx_buffer[4*desc] = cpu_to_le32(0x80000000);
			trigger_transmit(card);
			if (card->tx_buffer[nextdescriptor*4] & cpu_to_le32(0x8000000)) {
				
				netif_stop_queue(dev);
			}
			card->transmit_used = nextdescriptor;
			spin_unlock_irqrestore(&card->lock,flags);
			return NETDEV_TX_OK;
	}

	
	netif_stop_queue(dev);
	spin_unlock_irqrestore(&card->lock,flags);
	trigger_transmit(card);

	return NETDEV_TX_BUSY;
}




static int xircom_open(struct net_device *dev)
{
	struct xircom_private *xp = netdev_priv(dev);
	int retval;

	netdev_info(dev, "xircom cardbus adaptor found, using irq %i\n",
		    dev->irq);
	retval = request_irq(dev->irq, xircom_interrupt, IRQF_SHARED, dev->name, dev);
	if (retval)
		return retval;

	xircom_up(xp);
	xp->open = 1;

	return 0;
}

static int xircom_close(struct net_device *dev)
{
	struct xircom_private *card;
	unsigned long flags;

	card = netdev_priv(dev);
	netif_stop_queue(dev); 


	spin_lock_irqsave(&card->lock,flags);

	disable_all_interrupts(card);
#if 0
	
	deactivate_receiver(card);
	deactivate_transmitter(card);
#endif
	remove_descriptors(card);

	spin_unlock_irqrestore(&card->lock,flags);

	card->open = 0;
	free_irq(dev->irq,dev);

	return 0;

}


#ifdef CONFIG_NET_POLL_CONTROLLER
static void xircom_poll_controller(struct net_device *dev)
{
	disable_irq(dev->irq);
	xircom_interrupt(dev->irq, dev);
	enable_irq(dev->irq);
}
#endif


static void initialize_card(struct xircom_private *card)
{
	unsigned int val;
	unsigned long flags;

	spin_lock_irqsave(&card->lock, flags);

	
	val = inl(card->io_port + CSR0);
	val |= 0x01;		
	outl(val, card->io_port + CSR0);

	udelay(100);		

	val = inl(card->io_port + CSR0);
	val &= ~0x01;		
	outl(val, card->io_port + CSR0);


	val = 0;		
	outl(val, card->io_port + CSR0);


	disable_all_interrupts(card);
	deactivate_receiver(card);
	deactivate_transmitter(card);

	spin_unlock_irqrestore(&card->lock, flags);
}

static void trigger_transmit(struct xircom_private *card)
{
	unsigned int val;

	val = 0;
	outl(val, card->io_port + CSR1);
}

static void trigger_receive(struct xircom_private *card)
{
	unsigned int val;

	val = 0;
	outl(val, card->io_port + CSR2);
}

static void setup_descriptors(struct xircom_private *card)
{
	u32 address;
	int i;

	BUG_ON(card->rx_buffer == NULL);
	BUG_ON(card->tx_buffer == NULL);

	
	memset(card->rx_buffer, 0, 128);	
	for (i=0;i<NUMDESCRIPTORS;i++ ) {

		
		card->rx_buffer[i*4 + 0] = cpu_to_le32(0x80000000);
		
		card->rx_buffer[i*4 + 1] = cpu_to_le32(1536);
		if (i == NUMDESCRIPTORS - 1) 
			card->rx_buffer[i*4 + 1] |= cpu_to_le32(1 << 25);


		address = card->rx_dma_handle;
		card->rx_buffer[i*4 + 2] = cpu_to_le32(address + bufferoffsets[i]);
		
		card->rx_buffer[i*4 + 3] = 0;
	}

	wmb();
	
	address = card->rx_dma_handle;
	outl(address, card->io_port + CSR3);	


	
	memset(card->tx_buffer, 0, 128);	

	for (i=0;i<NUMDESCRIPTORS;i++ ) {
		
		card->tx_buffer[i*4 + 0] = 0x00000000;
		
		card->tx_buffer[i*4 + 1] = cpu_to_le32(1536);
		if (i == NUMDESCRIPTORS - 1) 
			card->tx_buffer[i*4 + 1] |= cpu_to_le32(1 << 25);

		address = card->tx_dma_handle;
		card->tx_buffer[i*4 + 2] = cpu_to_le32(address + bufferoffsets[i]);
		
		card->tx_buffer[i*4 + 3] = 0;
	}

	wmb();
	
	address = card->tx_dma_handle;
	outl(address, card->io_port + CSR4);	
}

static void remove_descriptors(struct xircom_private *card)
{
	unsigned int val;

	val = 0;
	outl(val, card->io_port + CSR3);	
	outl(val, card->io_port + CSR4);	
}

static int link_status_changed(struct xircom_private *card)
{
	unsigned int val;

	val = inl(card->io_port + CSR5);	

	if ((val & (1 << 27)) == 0)		
		return 0;

	val = (1 << 27);
	outl(val, card->io_port + CSR5);

	return 1;
}


static int transmit_active(struct xircom_private *card)
{
	unsigned int val;

	val = inl(card->io_port + CSR5);	

	if ((val & (7 << 20)) == 0)		
		return 0;

	return 1;
}

static int receive_active(struct xircom_private *card)
{
	unsigned int val;

	val = inl(card->io_port + CSR5);	

	if ((val & (7 << 17)) == 0)		
		return 0;

	return 1;
}

static void activate_receiver(struct xircom_private *card)
{
	unsigned int val;
	int counter;

	val = inl(card->io_port + CSR6);	

	if ((val&2) && (receive_active(card)))
		return;


	val = val & ~2;		
	outl(val, card->io_port + CSR6);

	counter = 10;
	while (counter > 0) {
		if (!receive_active(card))
			break;
		
		udelay(50);
		counter--;
		if (counter <= 0)
			netdev_err(card->dev, "Receiver failed to deactivate\n");
	}

	
	val = inl(card->io_port + CSR6);	
	val = val | 2;				
	outl(val, card->io_port + CSR6);

	
	counter = 10;
	while (counter > 0) {
		if (receive_active(card))
			break;
		
		udelay(50);
		counter--;
		if (counter <= 0)
			netdev_err(card->dev,
				   "Receiver failed to re-activate\n");
	}
}

static void deactivate_receiver(struct xircom_private *card)
{
	unsigned int val;
	int counter;

	val = inl(card->io_port + CSR6);	
	val = val & ~2;				
	outl(val, card->io_port + CSR6);

	counter = 10;
	while (counter > 0) {
		if (!receive_active(card))
			break;
		
		udelay(50);
		counter--;
		if (counter <= 0)
			netdev_err(card->dev, "Receiver failed to deactivate\n");
	}
}


static void activate_transmitter(struct xircom_private *card)
{
	unsigned int val;
	int counter;

	val = inl(card->io_port + CSR6);	

	if ((val&(1<<13)) && (transmit_active(card)))
		return;

	val = val & ~(1 << 13);	
	outl(val, card->io_port + CSR6);

	counter = 10;
	while (counter > 0) {
		if (!transmit_active(card))
			break;
		
		udelay(50);
		counter--;
		if (counter <= 0)
			netdev_err(card->dev,
				   "Transmitter failed to deactivate\n");
	}

	
	val = inl(card->io_port + CSR6);	
	val = val | (1 << 13);	
	outl(val, card->io_port + CSR6);

	
	counter = 10;
	while (counter > 0) {
		if (transmit_active(card))
			break;
		
		udelay(50);
		counter--;
		if (counter <= 0)
			netdev_err(card->dev,
				   "Transmitter failed to re-activate\n");
	}
}

static void deactivate_transmitter(struct xircom_private *card)
{
	unsigned int val;
	int counter;

	val = inl(card->io_port + CSR6);	
	val = val & ~2;		
	outl(val, card->io_port + CSR6);

	counter = 20;
	while (counter > 0) {
		if (!transmit_active(card))
			break;
		
		udelay(50);
		counter--;
		if (counter <= 0)
			netdev_err(card->dev,
				   "Transmitter failed to deactivate\n");
	}
}


static void enable_transmit_interrupt(struct xircom_private *card)
{
	unsigned int val;

	val = inl(card->io_port + CSR7);	
	val |= 1;				
	outl(val, card->io_port + CSR7);
}


static void enable_receive_interrupt(struct xircom_private *card)
{
	unsigned int val;

	val = inl(card->io_port + CSR7);	
	val = val | (1 << 6);			
	outl(val, card->io_port + CSR7);
}

static void enable_link_interrupt(struct xircom_private *card)
{
	unsigned int val;

	val = inl(card->io_port + CSR7);	
	val = val | (1 << 27);			
	outl(val, card->io_port + CSR7);
}



static void disable_all_interrupts(struct xircom_private *card)
{
	unsigned int val;

	val = 0;				
	outl(val, card->io_port + CSR7);
}

static void enable_common_interrupts(struct xircom_private *card)
{
	unsigned int val;

	val = inl(card->io_port + CSR7);	
	val |= (1<<16); 
	val |= (1<<15); 
	val |= (1<<13); 
	val |= (1<<8);  
	val |= (1<<7);  
	val |= (1<<5);  
	val |= (1<<2);  
	val |= (1<<1);  
	outl(val, card->io_port + CSR7);
}

static int enable_promisc(struct xircom_private *card)
{
	unsigned int val;

	val = inl(card->io_port + CSR6);
	val = val | (1 << 6);
	outl(val, card->io_port + CSR6);

	return 1;
}




static int link_status(struct xircom_private *card)
{
	unsigned int val;

	val = inb(card->io_port + CSR12);

	if (!(val&(1<<2)))  
		return 10;
	if (!(val&(1<<1)))  
		return 100;

	

	return 0;
}





static void read_mac_address(struct xircom_private *card)
{
	unsigned char j, tuple, link, data_id, data_count;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&card->lock, flags);

	outl(1 << 12, card->io_port + CSR9);	
	for (i = 0x100; i < 0x1f7; i += link + 2) {
		outl(i, card->io_port + CSR10);
		tuple = inl(card->io_port + CSR9) & 0xff;
		outl(i + 1, card->io_port + CSR10);
		link = inl(card->io_port + CSR9) & 0xff;
		outl(i + 2, card->io_port + CSR10);
		data_id = inl(card->io_port + CSR9) & 0xff;
		outl(i + 3, card->io_port + CSR10);
		data_count = inl(card->io_port + CSR9) & 0xff;
		if ((tuple == 0x22) && (data_id == 0x04) && (data_count == 0x06)) {
			for (j = 0; j < 6; j++) {
				outl(i + j + 4, card->io_port + CSR10);
				card->dev->dev_addr[j] = inl(card->io_port + CSR9) & 0xff;
			}
			break;
		} else if (link == 0) {
			break;
		}
	}
	spin_unlock_irqrestore(&card->lock, flags);
	pr_debug(" %pM\n", card->dev->dev_addr);
}


static void transceiver_voodoo(struct xircom_private *card)
{
	unsigned long flags;

	
	pci_write_config_dword(card->pdev, PCI_POWERMGMT, 0x0000);

	setup_descriptors(card);

	spin_lock_irqsave(&card->lock, flags);

	outl(0x0008, card->io_port + CSR15);
        udelay(25);
        outl(0xa8050000, card->io_port + CSR15);
        udelay(25);
        outl(0xa00f0000, card->io_port + CSR15);
        udelay(25);

        spin_unlock_irqrestore(&card->lock, flags);

	netif_start_queue(card->dev);
}


static void xircom_up(struct xircom_private *card)
{
	unsigned long flags;
	int i;

	
	pci_write_config_dword(card->pdev, PCI_POWERMGMT, 0x0000);

	setup_descriptors(card);

	spin_lock_irqsave(&card->lock, flags);


	enable_link_interrupt(card);
	enable_transmit_interrupt(card);
	enable_receive_interrupt(card);
	enable_common_interrupts(card);
	enable_promisc(card);

	
	for (i=0;i<NUMDESCRIPTORS;i++)
		investigate_read_descriptor(card->dev,card,i,bufferoffsets[i]);


	spin_unlock_irqrestore(&card->lock, flags);
	trigger_receive(card);
	trigger_transmit(card);
	netif_start_queue(card->dev);
}

static void
investigate_read_descriptor(struct net_device *dev, struct xircom_private *card,
			    int descnr, unsigned int bufferoffset)
{
	int status;

	status = le32_to_cpu(card->rx_buffer[4*descnr]);

	if (status > 0) {		

		

		short pkt_len = ((status >> 16) & 0x7ff) - 4;
					
		struct sk_buff *skb;

		if (pkt_len > 1518) {
			netdev_err(dev, "Packet length %i is bogus\n", pkt_len);
			pkt_len = 1518;
		}

		skb = netdev_alloc_skb(dev, pkt_len + 2);
		if (skb == NULL) {
			dev->stats.rx_dropped++;
			goto out;
		}
		skb_reserve(skb, 2);
		skb_copy_to_linear_data(skb,
					&card->rx_buffer[bufferoffset / 4],
					pkt_len);
		skb_put(skb, pkt_len);
		skb->protocol = eth_type_trans(skb, dev);
		netif_rx(skb);
		dev->stats.rx_packets++;
		dev->stats.rx_bytes += pkt_len;

out:
		
		card->rx_buffer[4*descnr] = cpu_to_le32(0x80000000);
		trigger_receive(card);
	}
}


static void
investigate_write_descriptor(struct net_device *dev,
			     struct xircom_private *card,
			     int descnr, unsigned int bufferoffset)
{
	int status;

	status = le32_to_cpu(card->tx_buffer[4*descnr]);
#if 0
	if (status & 0x8000) {	
		pr_err("Major transmit error status %x\n", status);
		card->tx_buffer[4*descnr] = 0;
		netif_wake_queue (dev);
	}
#endif
	if (status > 0) {	
		if (card->tx_skb[descnr]!=NULL) {
			dev->stats.tx_bytes += card->tx_skb[descnr]->len;
			dev_kfree_skb_irq(card->tx_skb[descnr]);
		}
		card->tx_skb[descnr] = NULL;
		
		if (status & (1 << 8))
			dev->stats.collisions++;
		card->tx_buffer[4*descnr] = 0; 
		netif_wake_queue (dev);
		dev->stats.tx_packets++;
	}
}

static int __init xircom_init(void)
{
	return pci_register_driver(&xircom_ops);
}

static void __exit xircom_exit(void)
{
	pci_unregister_driver(&xircom_ops);
}

module_init(xircom_init)
module_exit(xircom_exit)

