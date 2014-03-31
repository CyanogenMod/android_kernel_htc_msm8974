/*
	Written 1993,1994 by Donald Becker.

	Copyright 1993 United States Government as represented by the
	Director, National Security Agency.

	This software may be used and distributed according to the terms
	of the GNU General Public License, incorporated herein by reference.

	The author may be reached as becker@scyld.com, or C/O
	Scyld Computing Corporation
	410 Severn Ave., Suite 210
	Annapolis MD 21403


	Thanks go to jennings@Montrouge.SMR.slb.com ( Patrick Jennings)
	and jrs@world.std.com (Rick Sladkey) for testing and bugfixes.
	Mark Salazar <leslie@access.digex.net> made the changes for cards with
	only 16K packet buffers.

	Things remaining to do:
	Verify that the tx and rx buffers don't have fencepost errors.
	Move the theory of operation and memory map documentation.
	The statistics need to be updated correctly.
*/

#define DRV_NAME		"3c507"
#define DRV_VERSION		"1.10a"
#define DRV_RELDATE		"11/17/2001"

static const char version[] =
	DRV_NAME ".c:v" DRV_VERSION " " DRV_RELDATE " Donald Becker (becker@scyld.com)\n";

/*
  Sources:
	This driver wouldn't have been written with the availability of the
	Crynwr driver source code.	It provided a known-working implementation
	that filled in the gaping holes of the Intel documentation.  Three cheers
	for Russ Nelson.

	Intel Microcommunications Databook, Vol. 1, 1990.  It provides just enough
	info that the casual reader might think that it documents the i82586 :-<.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/ethtool.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/bitops.h>

#include <asm/dma.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#ifndef NET_DEBUG
#define NET_DEBUG 1
#endif
static unsigned int net_debug = NET_DEBUG;
#define debug net_debug



#define	 CUC_START	 0x0100
#define	 CUC_RESUME	 0x0200
#define	 CUC_SUSPEND 0x0300
#define	 RX_START	 0x0010
#define	 RX_RESUME	 0x0020
#define	 RX_SUSPEND	 0x0030

/* The Rx unit uses a list of frame descriptors and a list of data buffer
   descriptors.  We use full-sized (1518 byte) data buffers, so there is
   a one-to-one pairing of frame descriptors to buffer descriptors.

   The Tx ("command") unit executes a list of commands that look like:
		Status word		Written by the 82586 when the command is done.
		Command word	Command in lower 3 bits, post-command action in upper 3
		Link word		The address of the next command.
		Parameters		(as needed).

	Some definitions related to the Command Word are:
 */
#define CMD_EOL		0x8000			
#define CMD_SUSP	0x4000			
#define CMD_INTR	0x2000			

enum commands {
	CmdNOp = 0, CmdSASetup = 1, CmdConfigure = 2, CmdMulticastList = 3,
	CmdTx = 4, CmdTDR = 5, CmdDump = 6, CmdDiagnose = 7};

struct net_local {
	int last_restart;
	ushort rx_head;
	ushort rx_tail;
	ushort tx_head;
	ushort tx_cmd_link;
	ushort tx_reap;
	ushort tx_pkts_in_ring;
	spinlock_t lock;
	void __iomem *base;
};


#define	SA_DATA		0	
#define MISC_CTRL	6	
#define RESET_IRQ	10	
#define SIGNAL_CA	11	
#define ROM_CONFIG	13
#define MEM_CONFIG	14
#define IRQ_CONFIG	15
#define EL16_IO_EXTENT 16

#define ID_PORT		0x100

#define iSCB_STATUS	0x8
#define iSCB_CMD		0xA
#define iSCB_CBL		0xC	
#define iSCB_RFA		0xE	

#define SCB_BASE		((unsigned)64*1024 - (dev->mem_end - dev->mem_start))


#define CONFIG_CMD	0x0018
#define SET_SA_CMD	0x0024
#define SA_OFFSET	0x002A
#define IDLELOOP	0x30
#define TDR_CMD		0x38
#define TDR_TIME	0x3C
#define DUMP_CMD	0x40
#define DIAG_CMD	0x48
#define SET_MC_CMD	0x4E
#define DUMP_DATA	0x56	

#define TX_BUF_START	0x0100
#define NUM_TX_BUFS 	5
#define TX_BUF_SIZE 	(1518+14+20+16) 

#define RX_BUF_START	0x2000
#define RX_BUF_SIZE 	(1518+14+18)	
#define RX_BUF_END		(dev->mem_end - dev->mem_start)

#define TX_TIMEOUT (HZ/20)


static unsigned short init_words[] = {
	
	0x0000,					
	0,0,					
	0x0000,0x0000,			

	
	0x0001,					
	0x0008,0,0,				

	
	0,0xf000|RX_START|CUC_START,	
	CONFIG_CMD,				
	RX_BUF_START,				
	0,0,0,0,				

	
	0, CmdConfigure,		
	SET_SA_CMD,				
	0x0804,					
	0x2e40,					
	0,						

	
	0, CmdSASetup,
	SET_MC_CMD,				
	0xaa00,0xb000,0x0bad,	

	
	0, CmdNOp, IDLELOOP, 0 ,

	
	0, CmdTDR, IDLELOOP, 0,

	
	0, CmdDump, IDLELOOP, DUMP_DATA,

	
	0, CmdDiagnose, IDLELOOP,

	
	0, CmdMulticastList, IDLELOOP, 0,
};


static int	el16_probe1(struct net_device *dev, int ioaddr);
static int	el16_open(struct net_device *dev);
static netdev_tx_t el16_send_packet(struct sk_buff *skb,
				    struct net_device *dev);
static irqreturn_t el16_interrupt(int irq, void *dev_id);
static void el16_rx(struct net_device *dev);
static int	el16_close(struct net_device *dev);
static void el16_tx_timeout (struct net_device *dev);

static void hardware_send_packet(struct net_device *dev, void *buf, short length, short pad);
static void init_82586_mem(struct net_device *dev);
static const struct ethtool_ops netdev_ethtool_ops;
static void init_rx_bufs(struct net_device *);

static int io = 0x300;
static int irq;
static int mem_start;



struct net_device * __init el16_probe(int unit)
{
	struct net_device *dev = alloc_etherdev(sizeof(struct net_local));
	static const unsigned ports[] = { 0x300, 0x320, 0x340, 0x280, 0};
	const unsigned *port;
	int err = -ENODEV;

	if (!dev)
		return ERR_PTR(-ENODEV);

	if (unit >= 0) {
		sprintf(dev->name, "eth%d", unit);
		netdev_boot_setup_check(dev);
		io = dev->base_addr;
		irq = dev->irq;
		mem_start = dev->mem_start & 15;
	}

	if (io > 0x1ff) 	
		err = el16_probe1(dev, io);
	else if (io != 0)
		err = -ENXIO;		
	else {
		for (port = ports; *port; port++) {
			err = el16_probe1(dev, *port);
			if (!err)
				break;
		}
	}

	if (err)
		goto out;
	err = register_netdev(dev);
	if (err)
		goto out1;
	return dev;
out1:
	free_irq(dev->irq, dev);
	iounmap(((struct net_local *)netdev_priv(dev))->base);
	release_region(dev->base_addr, EL16_IO_EXTENT);
out:
	free_netdev(dev);
	return ERR_PTR(err);
}

static const struct net_device_ops netdev_ops = {
	.ndo_open		= el16_open,
	.ndo_stop		= el16_close,
	.ndo_start_xmit 	= el16_send_packet,
	.ndo_tx_timeout 	= el16_tx_timeout,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};

static int __init el16_probe1(struct net_device *dev, int ioaddr)
{
	static unsigned char init_ID_done;
	int i, irq, irqval, retval;
	struct net_local *lp;

	if (init_ID_done == 0) {
		ushort lrs_state = 0xff;
		
		outb(0x00, ID_PORT);
		for(i = 0; i < 255; i++) {
			outb(lrs_state, ID_PORT);
			lrs_state <<= 1;
			if (lrs_state & 0x100)
				lrs_state ^= 0xe7;
		}
		outb(0x00, ID_PORT);
		init_ID_done = 1;
	}

	if (!request_region(ioaddr, EL16_IO_EXTENT, DRV_NAME))
		return -ENODEV;

	if ((inb(ioaddr) != '*') || (inb(ioaddr + 1) != '3') ||
	    (inb(ioaddr + 2) != 'C') || (inb(ioaddr + 3) != 'O')) {
		retval = -ENODEV;
		goto out;
	}

	pr_info("%s: 3c507 at %#x,", dev->name, ioaddr);


	irq = inb(ioaddr + IRQ_CONFIG) & 0x0f;

	irqval = request_irq(irq, el16_interrupt, 0, DRV_NAME, dev);
	if (irqval) {
		pr_cont("\n");
		pr_err("3c507: unable to get IRQ %d (irqval=%d).\n", irq, irqval);
		retval = -EAGAIN;
		goto out;
	}

	
	dev->base_addr = ioaddr;

	outb(0x01, ioaddr + MISC_CTRL);
	for (i = 0; i < 6; i++)
		dev->dev_addr[i] = inb(ioaddr + i);
	pr_cont(" %pM", dev->dev_addr);

	if (mem_start)
		net_debug = mem_start & 7;

#ifdef MEM_BASE
	dev->mem_start = MEM_BASE;
	dev->mem_end = dev->mem_start + 0x10000;
#else
	{
		int base;
		int size;
		char mem_config = inb(ioaddr + MEM_CONFIG);
		if (mem_config & 0x20) {
			size = 64*1024;
			base = 0xf00000 + (mem_config & 0x08 ? 0x080000
							   : ((mem_config & 3) << 17));
		} else {
			size = ((mem_config & 3) + 1) << 14;
			base = 0x0c0000 + ( (mem_config & 0x18) << 12);
		}
		dev->mem_start = base;
		dev->mem_end = base + size;
	}
#endif

	dev->if_port = (inb(ioaddr + ROM_CONFIG) & 0x80) ? 1 : 0;
	dev->irq = inb(ioaddr + IRQ_CONFIG) & 0x0f;

	pr_cont(", IRQ %d, %sternal xcvr, memory %#lx-%#lx.\n", dev->irq,
		   dev->if_port ? "ex" : "in", dev->mem_start, dev->mem_end-1);

	if (net_debug)
		pr_debug("%s", version);

	lp = netdev_priv(dev);
	spin_lock_init(&lp->lock);
	lp->base = ioremap(dev->mem_start, RX_BUF_END);
	if (!lp->base) {
		pr_err("3c507: unable to remap memory\n");
		retval = -EAGAIN;
		goto out1;
	}

	dev->netdev_ops = &netdev_ops;
	dev->watchdog_timeo = TX_TIMEOUT;
	dev->ethtool_ops = &netdev_ethtool_ops;
 	dev->flags &= ~IFF_MULTICAST;	
	return 0;
out1:
	free_irq(dev->irq, dev);
out:
	release_region(ioaddr, EL16_IO_EXTENT);
	return retval;
}

static int el16_open(struct net_device *dev)
{
	
	init_82586_mem(dev);

	netif_start_queue(dev);
	return 0;
}


static void el16_tx_timeout (struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);
	int ioaddr = dev->base_addr;
	void __iomem *shmem = lp->base;

	if (net_debug > 1)
		pr_debug("%s: transmit timed out, %s?  ", dev->name,
			readw(shmem + iSCB_STATUS) & 0x8000 ? "IRQ conflict" :
			"network cable problem");
	
	if (lp->last_restart == dev->stats.tx_packets) {
		if (net_debug > 1)
			pr_cont("Resetting board.\n");
		
		init_82586_mem (dev);
		lp->tx_pkts_in_ring = 0;
	} else {
		
		if (net_debug > 1)
			pr_cont("Kicking board.\n");
		writew(0xf000 | CUC_START | RX_START, shmem + iSCB_CMD);
		outb (0, ioaddr + SIGNAL_CA);	
		lp->last_restart = dev->stats.tx_packets;
	}
	dev->trans_start = jiffies; 
	netif_wake_queue (dev);
}


static netdev_tx_t el16_send_packet (struct sk_buff *skb,
				     struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);
	int ioaddr = dev->base_addr;
	unsigned long flags;
	short length = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;
	unsigned char *buf = skb->data;

	netif_stop_queue (dev);

	spin_lock_irqsave (&lp->lock, flags);

	dev->stats.tx_bytes += length;
	
	outb (0x80, ioaddr + MISC_CTRL);

	hardware_send_packet (dev, buf, skb->len, length - skb->len);

	
	outb (0x84, ioaddr + MISC_CTRL);

	spin_unlock_irqrestore (&lp->lock, flags);

	dev_kfree_skb (skb);

	

	return NETDEV_TX_OK;
}

static irqreturn_t el16_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct net_local *lp;
	int ioaddr, status, boguscount = 0;
	ushort ack_cmd = 0;
	void __iomem *shmem;

	if (dev == NULL) {
		pr_err("net_interrupt(): irq %d for unknown device.\n", irq);
		return IRQ_NONE;
	}

	ioaddr = dev->base_addr;
	lp = netdev_priv(dev);
	shmem = lp->base;

	spin_lock(&lp->lock);

	status = readw(shmem+iSCB_STATUS);

	if (net_debug > 4) {
		pr_debug("%s: 3c507 interrupt, status %4.4x.\n", dev->name, status);
	}

	
	outb(0x80, ioaddr + MISC_CTRL);

	
	while (lp->tx_pkts_in_ring) {
	  unsigned short tx_status = readw(shmem+lp->tx_reap);
	  if (!(tx_status & 0x8000)) {
		if (net_debug > 5)
			pr_debug("Tx command incomplete (%#x).\n", lp->tx_reap);
		break;
	  }
	  
	  if (!(tx_status & 0x2000) || (tx_status & 0x0f3f)) {
		dev->stats.tx_errors++;
		if (tx_status & 0x0600)  dev->stats.tx_carrier_errors++;
		if (tx_status & 0x0100)  dev->stats.tx_fifo_errors++;
		if (!(tx_status & 0x0040))  dev->stats.tx_heartbeat_errors++;
		if (tx_status & 0x0020)  dev->stats.tx_aborted_errors++;
		dev->stats.collisions += tx_status & 0xf;
	  }
	  dev->stats.tx_packets++;
	  if (net_debug > 5)
		  pr_debug("Reaped %x, Tx status %04x.\n" , lp->tx_reap, tx_status);
	  lp->tx_reap += TX_BUF_SIZE;
	  if (lp->tx_reap > RX_BUF_START - TX_BUF_SIZE)
		lp->tx_reap = TX_BUF_START;

	  lp->tx_pkts_in_ring--;
	  
	  netif_wake_queue(dev);

	  if (++boguscount > 10)
		break;
	}

	if (status & 0x4000) { 
		if (net_debug > 5)
			pr_debug("Received packet, rx_head %04x.\n", lp->rx_head);
		el16_rx(dev);
	}

	
	ack_cmd = status & 0xf000;

	if ((status & 0x0700) != 0x0200 && netif_running(dev)) {
		if (net_debug)
			pr_debug("%s: Command unit stopped, status %04x, restarting.\n",
				   dev->name, status);
		ack_cmd |= CUC_RESUME;
	}

	if ((status & 0x0070) != 0x0040 && netif_running(dev)) {
		if (net_debug)
			pr_debug("%s: Rx unit stopped, status %04x, restarting.\n",
				   dev->name, status);
		init_rx_bufs(dev);
		writew(RX_BUF_START,shmem+iSCB_RFA);
		ack_cmd |= RX_START;
	}

	writew(ack_cmd,shmem+iSCB_CMD);
	outb(0, ioaddr + SIGNAL_CA);			

	
	outb(0, ioaddr + RESET_IRQ);

	
	outb(0x84, ioaddr + MISC_CTRL);
	spin_unlock(&lp->lock);
	return IRQ_HANDLED;
}

static int el16_close(struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);
	int ioaddr = dev->base_addr;
	void __iomem *shmem = lp->base;

	netif_stop_queue(dev);

	
	writew(RX_SUSPEND | CUC_SUSPEND,shmem+iSCB_CMD);
	outb(0, ioaddr + SIGNAL_CA);

	
	outb(0x80, ioaddr + MISC_CTRL);

	

	

	return 0;
}

static void init_rx_bufs(struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);
	void __iomem *write_ptr;
	unsigned short SCB_base = SCB_BASE;

	int cur_rxbuf = lp->rx_head = RX_BUF_START;

	
	do {	

		write_ptr = lp->base + cur_rxbuf;

		writew(0x0000,write_ptr);			
		writew(0x0000,write_ptr+=2);			
		writew(cur_rxbuf + RX_BUF_SIZE,write_ptr+=2);	
		writew(cur_rxbuf + 22,write_ptr+=2);		
		writew(0x0000,write_ptr+=2);			
		writew(0x0000,write_ptr+=2);
		writew(0x0000,write_ptr+=2);
		writew(0x0000,write_ptr+=2);			
		writew(0x0000,write_ptr+=2);
		writew(0x0000,write_ptr+=2);
		writew(0x0000,write_ptr+=2);			

		writew(0x0000,write_ptr+=2);			
		writew(-1,write_ptr+=2);			
		writew(cur_rxbuf + 0x20 + SCB_base,write_ptr+=2);
		writew(0x0000,write_ptr+=2);
		
		writew(0x8000 + RX_BUF_SIZE-0x20,write_ptr+=2);

		lp->rx_tail = cur_rxbuf;
		cur_rxbuf += RX_BUF_SIZE;
	} while (cur_rxbuf <= RX_BUF_END - RX_BUF_SIZE);

	write_ptr = lp->base + lp->rx_tail + 2;
	writew(0xC000,write_ptr);				
	writew(lp->rx_head,write_ptr+2);			
}

static void init_82586_mem(struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);
	short ioaddr = dev->base_addr;
	void __iomem *shmem = lp->base;

	outb(0x20, ioaddr + MISC_CTRL);

	
	init_words[3] = SCB_BASE;
	init_words[7] = SCB_BASE;

	
	memcpy_toio(lp->base + RX_BUF_END - 10, init_words, 10);

	
	memcpy_toio(lp->base, init_words + 5, sizeof(init_words) - 10);

	
	memcpy_toio(lp->base+SA_OFFSET, dev->dev_addr, ETH_ALEN);

	/* The Tx-block list is written as needed.  We just set up the values. */
	lp->tx_cmd_link = IDLELOOP + 4;
	lp->tx_head = lp->tx_reap = TX_BUF_START;

	init_rx_bufs(dev);

	
	outb(0xA0, ioaddr + MISC_CTRL);

	outb(0, ioaddr + SIGNAL_CA);

	{
		int boguscnt = 50;
		while (readw(shmem+iSCB_STATUS) == 0)
			if (--boguscnt == 0) {
				pr_warning("%s: i82586 initialization timed out with status %04x, cmd %04x.\n",
					dev->name, readw(shmem+iSCB_STATUS), readw(shmem+iSCB_CMD));
				break;
			}
		
		outb(0, ioaddr + SIGNAL_CA);
	}

	
	outb(0x84, ioaddr + MISC_CTRL);
	if (net_debug > 4)
		pr_debug("%s: Initialized 82586, status %04x.\n", dev->name,
			   readw(shmem+iSCB_STATUS));
}

static void hardware_send_packet(struct net_device *dev, void *buf, short length, short pad)
{
	struct net_local *lp = netdev_priv(dev);
	short ioaddr = dev->base_addr;
	ushort tx_block = lp->tx_head;
	void __iomem *write_ptr = lp->base + tx_block;
	static char padding[ETH_ZLEN];

	
	writew(0x0000,write_ptr);			
	writew(CMD_INTR|CmdTx,write_ptr+=2);		
	writew(tx_block+16,write_ptr+=2);		
	writew(tx_block+8,write_ptr+=2);			

	
	writew((pad + length) | 0x8000,write_ptr+=2);		
	writew(-1,write_ptr+=2);			
	writew(tx_block+22+SCB_BASE,write_ptr+=2);	
	writew(0x0000,write_ptr+=2);			

	
	writew(0x0000,write_ptr+=2);			
	writew(CmdNOp,write_ptr+=2);			
	writew(tx_block+16,write_ptr+=2);		

	
	memcpy_toio(write_ptr+2, buf, length);
	if (pad)
		memcpy_toio(write_ptr+length+2, padding, pad);

	
	writew(tx_block,lp->base + lp->tx_cmd_link);
	lp->tx_cmd_link = tx_block + 20;

	
	lp->tx_head = tx_block + TX_BUF_SIZE;
	if (lp->tx_head > RX_BUF_START - TX_BUF_SIZE)
		lp->tx_head = TX_BUF_START;

	if (net_debug > 4) {
		pr_debug("%s: 3c507 @%x send length = %d, tx_block %3x, next %3x.\n",
			   dev->name, ioaddr, length, tx_block, lp->tx_head);
	}

	
	if (++lp->tx_pkts_in_ring < NUM_TX_BUFS)
		netif_wake_queue(dev);
}

static void el16_rx(struct net_device *dev)
{
	struct net_local *lp = netdev_priv(dev);
	void __iomem *shmem = lp->base;
	ushort rx_head = lp->rx_head;
	ushort rx_tail = lp->rx_tail;
	ushort boguscount = 10;
	short frame_status;

	while ((frame_status = readw(shmem+rx_head)) < 0) {   
		void __iomem *read_frame = lp->base + rx_head;
		ushort rfd_cmd = readw(read_frame+2);
		ushort next_rx_frame = readw(read_frame+4);
		ushort data_buffer_addr = readw(read_frame+6);
		void __iomem *data_frame = lp->base + data_buffer_addr;
		ushort pkt_len = readw(data_frame);

		if (rfd_cmd != 0 || data_buffer_addr != rx_head + 22 ||
		    (pkt_len & 0xC000) != 0xC000) {
			pr_err("%s: Rx frame at %#x corrupted, "
			       "status %04x cmd %04x next %04x "
			       "data-buf @%04x %04x.\n",
			       dev->name, rx_head, frame_status, rfd_cmd,
			       next_rx_frame, data_buffer_addr, pkt_len);
		} else if ((frame_status & 0x2000) == 0) {
			
			dev->stats.rx_errors++;
			if (frame_status & 0x0800) dev->stats.rx_crc_errors++;
			if (frame_status & 0x0400) dev->stats.rx_frame_errors++;
			if (frame_status & 0x0200) dev->stats.rx_fifo_errors++;
			if (frame_status & 0x0100) dev->stats.rx_over_errors++;
			if (frame_status & 0x0080) dev->stats.rx_length_errors++;
		} else {
			
			struct sk_buff *skb;

			pkt_len &= 0x3fff;
			skb = netdev_alloc_skb(dev, pkt_len + 2);
			if (skb == NULL) {
				pr_err("%s: Memory squeeze, dropping packet.\n",
				       dev->name);
				dev->stats.rx_dropped++;
				break;
			}

			skb_reserve(skb,2);

			
			memcpy_fromio(skb_put(skb,pkt_len), data_frame + 10, pkt_len);

			skb->protocol=eth_type_trans(skb,dev);
			netif_rx(skb);
			dev->stats.rx_packets++;
			dev->stats.rx_bytes += pkt_len;
		}

		
		writew(0,read_frame);
		writew(0xC000,read_frame+2);
		
		writew(0x0000,lp->base + rx_tail + 2);

		rx_tail = rx_head;
		rx_head = next_rx_frame;
		if (--boguscount == 0)
			break;
	}

	lp->rx_head = rx_head;
	lp->rx_tail = rx_tail;
}

static void netdev_get_drvinfo(struct net_device *dev,
			       struct ethtool_drvinfo *info)
{
	strcpy(info->driver, DRV_NAME);
	strcpy(info->version, DRV_VERSION);
	sprintf(info->bus_info, "ISA 0x%lx", dev->base_addr);
}

static u32 netdev_get_msglevel(struct net_device *dev)
{
	return debug;
}

static void netdev_set_msglevel(struct net_device *dev, u32 level)
{
	debug = level;
}

static const struct ethtool_ops netdev_ethtool_ops = {
	.get_drvinfo		= netdev_get_drvinfo,
	.get_msglevel		= netdev_get_msglevel,
	.set_msglevel		= netdev_set_msglevel,
};

#ifdef MODULE
static struct net_device *dev_3c507;
module_param(io, int, 0);
module_param(irq, int, 0);
MODULE_PARM_DESC(io, "EtherLink16 I/O base address");
MODULE_PARM_DESC(irq, "(ignored)");

int __init init_module(void)
{
	if (io == 0)
		pr_notice("3c507: You should not use auto-probing with insmod!\n");
	dev_3c507 = el16_probe(-1);
	return IS_ERR(dev_3c507) ? PTR_ERR(dev_3c507) : 0;
}

void __exit
cleanup_module(void)
{
	struct net_device *dev = dev_3c507;
	unregister_netdev(dev);
	free_irq(dev->irq, dev);
	iounmap(((struct net_local *)netdev_priv(dev))->base);
	release_region(dev->base_addr, EL16_IO_EXTENT);
	free_netdev(dev);
}
#endif 
MODULE_LICENSE("GPL");
