
/*
	Written by Donald Becker.

	The author may be reached as becker@scyld.com.
	This driver is based on the Linux skeleton driver.  The copyright of the
	skeleton driver is held by the United States Government, as represented
	by DIRNSA, and it is released under the GPL.

	Thanks to Mike Hollick for alpha testing and suggestions.

  References:
	   The Crynwr packet driver.

	  "82593 CSMA/CD Core LAN Controller" Intel datasheet, 1992
	  Intel Microcommunications Databook, Vol. 1, 1990.
    As usual with Intel, the documentation is incomplete and inaccurate.
	I had to read the Crynwr packet driver to figure out how to actually
	use the i82593, and guess at what register bits matched the loosely
	related i82586.

					Theory of Operation

	The i82593 used in the Zenith Z-Note series operates using two(!) slave
	DMA	channels, one interrupt, and one 8-bit I/O port.

	While there	several ways to configure '593 DMA system, I chose the one
	that seemed commensurate with the highest system performance in the face
	of moderate interrupt latency: Both DMA channels are configured as
	recirculating ring buffers, with one channel (#0) dedicated to Rx and
	the other channel (#1) to Tx and configuration.  (Note that this is
	different than the Crynwr driver, where the Tx DMA channel is initialized
	before each operation.  That approach simplifies operation and Tx error
	recovery, but requires additional I/O in normal operation and precludes
	transmit buffer	chaining.)

	Both rings are set to 8192 bytes using {TX,RX}_RING_SIZE.  This provides
	a reasonable ring size for Rx, while simplifying DMA buffer allocation --
	DMA buffers must not cross a 128K boundary.  (In truth the size selection
	was influenced by my lack of '593 documentation.  I thus was constrained
	to use the Crynwr '593 initialization table, which sets the Rx ring size
	to 8K.)

	Despite my usual low opinion about Intel-designed parts, I must admit
	that the bulk data handling of the i82593 is a good design for
	an integrated system, like a laptop, where using two slave DMA channels
	doesn't pose a problem.  I still take issue with using only a single I/O
	port.  In the same controlled environment there are essentially no
	limitations on I/O space, and using multiple locations would eliminate
	the	need for multiple operations when looking at status registers,
	setting the Rx ring boundary, or switching to promiscuous mode.

	I also question Zenith's selection of the '593: one of the advertised
	advantages of earlier Intel parts was that if you figured out the magic
	initialization incantation you could use the same part on many different
	network types.  Zenith's use of the "FriendlyNet" (sic) connector rather
	than an	on-board transceiver leads me to believe that they were planning
	to take advantage of this.  But, uhmmm, the '593 omits all but ethernet
	functionality from the serial subsystem.
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/bitops.h>

#include <asm/io.h>
#include <asm/dma.h>

#include <linux/i82593.h>

static char version[] __initdata = "znet.c:v1.02 9/23/94 becker@scyld.com\n";

#ifndef ZNET_DEBUG
#define ZNET_DEBUG 1
#endif
static unsigned int znet_debug = ZNET_DEBUG;
module_param (znet_debug, int, 0);
MODULE_PARM_DESC (znet_debug, "ZNet debug level");
MODULE_LICENSE("GPL");

#define DMA_RX_MODE		0x14	
#define DMA_TX_MODE		0x18	
#define dma_page_eq(ptr1, ptr2) ((long)(ptr1)>>17 == (long)(ptr2)>>17)
#define RX_BUF_SIZE 8192
#define TX_BUF_SIZE 8192
#define DMA_BUF_SIZE (RX_BUF_SIZE + 16)	

#define TX_TIMEOUT	(HZ/10)

struct znet_private {
	int rx_dma, tx_dma;
	spinlock_t lock;
	short sia_base, sia_size, io_size;
	struct i82593_conf_block i593_init;
	
	ushort *rx_start, *rx_cur, *rx_end;
	ushort *tx_start, *tx_cur, *tx_end;
	ushort tx_buf_len;			
};

static struct net_device *znet_dev;

struct netidblk {
	char magic[8];		
	unsigned char netid[8]; 
	char nettype, globalopt;
	char vendor[8];		
	char product[8];
	char irq1, irq2;		
	char dma1, dma2;
	short dma_mem_misc[8];		
	short iobase1, iosize1;
	short iobase2, iosize2;		
	char driver_options;			
	char pad;
};

static int	znet_open(struct net_device *dev);
static netdev_tx_t znet_send_packet(struct sk_buff *skb,
				    struct net_device *dev);
static irqreturn_t znet_interrupt(int irq, void *dev_id);
static void	znet_rx(struct net_device *dev);
static int	znet_close(struct net_device *dev);
static void hardware_init(struct net_device *dev);
static void update_stop_hit(short ioaddr, unsigned short rx_stop_offset);
static void znet_tx_timeout (struct net_device *dev);

static int znet_request_resources (struct net_device *dev)
{
	struct znet_private *znet = netdev_priv(dev);

	if (request_irq (dev->irq, znet_interrupt, 0, "ZNet", dev))
		goto failed;
	if (request_dma (znet->rx_dma, "ZNet rx"))
		goto free_irq;
	if (request_dma (znet->tx_dma, "ZNet tx"))
		goto free_rx_dma;
	if (!request_region (znet->sia_base, znet->sia_size, "ZNet SIA"))
		goto free_tx_dma;
	if (!request_region (dev->base_addr, znet->io_size, "ZNet I/O"))
		goto free_sia;

	return 0;				

 free_sia:
	release_region (znet->sia_base, znet->sia_size);
 free_tx_dma:
	free_dma (znet->tx_dma);
 free_rx_dma:
	free_dma (znet->rx_dma);
 free_irq:
	free_irq (dev->irq, dev);
 failed:
	return -1;
}

static void znet_release_resources (struct net_device *dev)
{
	struct znet_private *znet = netdev_priv(dev);

	release_region (znet->sia_base, znet->sia_size);
	release_region (dev->base_addr, znet->io_size);
	free_dma (znet->tx_dma);
	free_dma (znet->rx_dma);
	free_irq (dev->irq, dev);
}

static void znet_transceiver_power (struct net_device *dev, int on)
{
	struct znet_private *znet = netdev_priv(dev);
	unsigned char v;

	
	
	outb(0x10, znet->sia_base);

	if (on)
		v = inb(znet->sia_base + 1) | 0x84;
	else
		v = inb(znet->sia_base + 1) & ~0x84;

	outb(v, znet->sia_base+1); 
}

static void znet_set_multicast_list (struct net_device *dev)
{
	struct znet_private *znet = netdev_priv(dev);
	short ioaddr = dev->base_addr;
	struct i82593_conf_block *cfblk = &znet->i593_init;

	memset(cfblk, 0x00, sizeof(struct i82593_conf_block));


	/* maz : Rewritten to take advantage of the wanvelan includes.
	   At least we have names, not just blind values */

	
	cfblk->fifo_limit = 10;	
	cfblk->forgnesi = 0;	
	cfblk->fifo_32 = 1;
	cfblk->d6mod = 0;  	
	cfblk->throttle_enb = 1;

	
	cfblk->throttle = 8;	
	cfblk->cntrxint = 0;	
	cfblk->contin = 1;	

	
	cfblk->addr_len = ETH_ALEN;
	cfblk->acloc = 1;	
	cfblk->preamb_len = 2;	
	cfblk->loopback = 0;	

	
	cfblk->lin_prio = 0;	
	cfblk->tbofstop = 0;
	cfblk->exp_prio = 0;
	cfblk->bof_met = 0;

	
	cfblk->ifrm_spc = 6;	

	
	cfblk->slottim_low = 0; 

	
	cfblk->slottim_hi = 2;	
	cfblk->max_retr = 15;	

	
	cfblk->prmisc = ((dev->flags & IFF_PROMISC) ? 1 : 0); 
	cfblk->bc_dis = 0;	
	cfblk->crs_1 = 0;	
	cfblk->nocrc_ins = 0;	
	cfblk->crc_1632 = 0;	
	cfblk->crs_cdt = 0;	

	
	cfblk->cs_filter = 0;  	
	cfblk->crs_src = 0;	
	cfblk->cd_filter = 0;  	

	
	cfblk->min_fr_len = ETH_ZLEN >> 2; 

	
	cfblk->lng_typ = 1;	
	cfblk->lng_fld = 1; 	
	cfblk->rxcrc_xf = 1;	
	cfblk->artx = 1;	
	cfblk->sarec = 1;	
	cfblk->tx_jabber = 0;	
	cfblk->hash_1 = 1; 	
	cfblk->lbpkpol = 0; 	

	
	cfblk->fdx = 0;		

	
	cfblk->dummy_6 = 0x3f; 	
	cfblk->mult_ia = 0;	
	cfblk->dis_bof = 0;	

	
	cfblk->dummy_1 = 1; 	
	cfblk->tx_ifs_retrig = 3; 
	cfblk->mc_all = (!netdev_mc_empty(dev) ||
			(dev->flags & IFF_ALLMULTI)); 
	cfblk->rcv_mon = 0;	
	cfblk->frag_acpt = 0;	
	cfblk->tstrttrs = 0;	

	
	cfblk->fretx = 1;	
	cfblk->runt_eop = 0;	
	cfblk->hw_sw_pin = 0;	
	cfblk->big_endn = 0;	
	cfblk->syncrqs = 1;	
	cfblk->sttlen = 1;  	
	cfblk->rx_eop = 0;  	
	cfblk->tx_eop = 0;  	

	
	cfblk->rbuf_size = RX_BUF_SIZE >> 12; 
	cfblk->rcvstop = 1; 	

	if (znet_debug > 2) {
		int i;
		unsigned char *c;

		for (i = 0, c = (char *) cfblk; i < sizeof (*cfblk); i++)
			printk ("%02X ", c[i]);
		printk ("\n");
	}

	*znet->tx_cur++ = sizeof(struct i82593_conf_block);
	memcpy(znet->tx_cur, cfblk, sizeof(struct i82593_conf_block));
	znet->tx_cur += sizeof(struct i82593_conf_block)/2;
	outb(OP0_CONFIGURE | CR0_CHNL, ioaddr);

}

static const struct net_device_ops znet_netdev_ops = {
	.ndo_open		= znet_open,
	.ndo_stop		= znet_close,
	.ndo_start_xmit		= znet_send_packet,
	.ndo_set_rx_mode	= znet_set_multicast_list,
	.ndo_tx_timeout		= znet_tx_timeout,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};


static int __init znet_probe (void)
{
	int i;
	struct netidblk *netinfo;
	struct znet_private *znet;
	struct net_device *dev;
	char *p;
	int err = -ENOMEM;

	
	for(p = (char *)phys_to_virt(0xf0000); p < (char *)phys_to_virt(0x100000); p++)
		if (*p == 'N'  &&  strncmp(p, "NETIDBLK", 8) == 0)
			break;

	if (p >= (char *)phys_to_virt(0x100000)) {
		if (znet_debug > 1)
			printk(KERN_INFO "No Z-Note ethernet adaptor found.\n");
		return -ENODEV;
	}

	dev = alloc_etherdev(sizeof(struct znet_private));
	if (!dev)
		return -ENOMEM;

	znet = netdev_priv(dev);

	netinfo = (struct netidblk *)p;
	dev->base_addr = netinfo->iobase1;
	dev->irq = netinfo->irq1;

	
	for (i = 0; i < 6; i++)
		dev->dev_addr[i] = netinfo->netid[i];

	printk(KERN_INFO "%s: ZNET at %#3lx, %pM"
	       ", using IRQ %d DMA %d and %d.\n",
	       dev->name, dev->base_addr, dev->dev_addr,
	       dev->irq, netinfo->dma1, netinfo->dma2);

	if (znet_debug > 1) {
		printk(KERN_INFO "%s: vendor '%16.16s' IRQ1 %d IRQ2 %d DMA1 %d DMA2 %d.\n",
		       dev->name, netinfo->vendor,
		       netinfo->irq1, netinfo->irq2,
		       netinfo->dma1, netinfo->dma2);
		printk(KERN_INFO "%s: iobase1 %#x size %d iobase2 %#x size %d net type %2.2x.\n",
		       dev->name, netinfo->iobase1, netinfo->iosize1,
		       netinfo->iobase2, netinfo->iosize2, netinfo->nettype);
	}

	if (znet_debug > 0)
		printk(KERN_INFO "%s", version);

	znet->rx_dma = netinfo->dma1;
	znet->tx_dma = netinfo->dma2;
	spin_lock_init(&znet->lock);
	znet->sia_base = 0xe6;	
	znet->sia_size = 2;
	znet->io_size  = 2;

	if (!(znet->rx_start = kmalloc (DMA_BUF_SIZE, GFP_KERNEL | GFP_DMA)))
		goto free_dev;
	if (!(znet->tx_start = kmalloc (DMA_BUF_SIZE, GFP_KERNEL | GFP_DMA)))
		goto free_rx;

	if (!dma_page_eq (znet->rx_start, znet->rx_start + (RX_BUF_SIZE/2-1)) ||
	    !dma_page_eq (znet->tx_start, znet->tx_start + (TX_BUF_SIZE/2-1))) {
		printk (KERN_WARNING "tx/rx crossing DMA frontiers, giving up\n");
		goto free_tx;
	}

	znet->rx_end = znet->rx_start + RX_BUF_SIZE/2;
	znet->tx_buf_len = TX_BUF_SIZE/2;
	znet->tx_end = znet->tx_start + znet->tx_buf_len;

	
	dev->netdev_ops = &znet_netdev_ops;
	dev->watchdog_timeo = TX_TIMEOUT;
	err = register_netdev(dev);
	if (err)
		goto free_tx;
	znet_dev = dev;
	return 0;

 free_tx:
	kfree(znet->tx_start);
 free_rx:
	kfree(znet->rx_start);
 free_dev:
	free_netdev(dev);
	return err;
}


static int znet_open(struct net_device *dev)
{
	int ioaddr = dev->base_addr;

	if (znet_debug > 2)
		printk(KERN_DEBUG "%s: znet_open() called.\n", dev->name);

	
	if (znet_request_resources (dev)) {
		printk(KERN_WARNING "%s: Not opened -- resource busy?!?\n", dev->name);
		return -EBUSY;
	}

	znet_transceiver_power (dev, 1);


	mdelay (50);

	
	if (inb(ioaddr) != 0x10 && inb(ioaddr) != 0x00)
		printk(KERN_WARNING "%s: Problem turning on the transceiver power.\n",
		       dev->name);

	hardware_init(dev);
	netif_start_queue (dev);

	return 0;
}


static void znet_tx_timeout (struct net_device *dev)
{
	int ioaddr = dev->base_addr;
	ushort event, tx_status, rx_offset, state;

	outb (CR0_STATUS_0, ioaddr);
	event = inb (ioaddr);
	outb (CR0_STATUS_1, ioaddr);
	tx_status = inw (ioaddr);
	outb (CR0_STATUS_2, ioaddr);
	rx_offset = inw (ioaddr);
	outb (CR0_STATUS_3, ioaddr);
	state = inb (ioaddr);
	printk (KERN_WARNING "%s: transmit timed out, status %02x %04x %04x %02x,"
	 " resetting.\n", dev->name, event, tx_status, rx_offset, state);
	if (tx_status == TX_LOST_CRS)
		printk (KERN_WARNING "%s: Tx carrier error, check transceiver cable.\n",
			dev->name);
	outb (OP0_RESET, ioaddr);
	hardware_init (dev);
	netif_wake_queue (dev);
}

static netdev_tx_t znet_send_packet(struct sk_buff *skb, struct net_device *dev)
{
	int ioaddr = dev->base_addr;
	struct znet_private *znet = netdev_priv(dev);
	unsigned long flags;
	short length = skb->len;

	if (znet_debug > 4)
		printk(KERN_DEBUG "%s: ZNet_send_packet.\n", dev->name);

	if (length < ETH_ZLEN) {
		if (skb_padto(skb, ETH_ZLEN))
			return NETDEV_TX_OK;
		length = ETH_ZLEN;
	}

	netif_stop_queue (dev);

	
	outb(CR0_STATUS_0, ioaddr);
	if (inw(ioaddr) == 0x0010 &&
	    inw(ioaddr) == 0x0000 &&
	    inw(ioaddr) == 0x0010) {
		if (znet_debug > 1)
			printk (KERN_WARNING "%s : waking up\n", dev->name);
		hardware_init(dev);
		znet_transceiver_power (dev, 1);
	}

	if (1) {
		unsigned char *buf = (void *)skb->data;
		ushort *tx_link = znet->tx_cur - 1;
		ushort rnd_len = (length + 1)>>1;

		dev->stats.tx_bytes+=length;

		if (znet->tx_cur >= znet->tx_end)
		  znet->tx_cur = znet->tx_start;
		*znet->tx_cur++ = length;
		if (znet->tx_cur + rnd_len + 1 > znet->tx_end) {
			int semi_cnt = (znet->tx_end - znet->tx_cur)<<1; 
			memcpy(znet->tx_cur, buf, semi_cnt);
			rnd_len -= semi_cnt>>1;
			memcpy(znet->tx_start, buf + semi_cnt, length - semi_cnt);
			znet->tx_cur = znet->tx_start + rnd_len;
		} else {
			memcpy(znet->tx_cur, buf, skb->len);
			znet->tx_cur += rnd_len;
		}
		*znet->tx_cur++ = 0;

		spin_lock_irqsave(&znet->lock, flags);
		{
			*tx_link = OP0_TRANSMIT | CR0_CHNL;
			
			outb(OP0_TRANSMIT | CR0_CHNL, ioaddr);
		}
		spin_unlock_irqrestore (&znet->lock, flags);

		netif_start_queue (dev);

		if (znet_debug > 4)
		  printk(KERN_DEBUG "%s: Transmitter queued, length %d.\n", dev->name, length);
	}
	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}

static irqreturn_t znet_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct znet_private *znet = netdev_priv(dev);
	int ioaddr;
	int boguscnt = 20;
	int handled = 0;

	spin_lock (&znet->lock);

	ioaddr = dev->base_addr;

	outb(CR0_STATUS_0, ioaddr);
	do {
		ushort status = inb(ioaddr);
		if (znet_debug > 5) {
			ushort result, rx_ptr, running;
			outb(CR0_STATUS_1, ioaddr);
			result = inw(ioaddr);
			outb(CR0_STATUS_2, ioaddr);
			rx_ptr = inw(ioaddr);
			outb(CR0_STATUS_3, ioaddr);
			running = inb(ioaddr);
			printk(KERN_DEBUG "%s: interrupt, status %02x, %04x %04x %02x serial %d.\n",
				 dev->name, status, result, rx_ptr, running, boguscnt);
		}
		if ((status & SR0_INTERRUPT) == 0)
			break;

		handled = 1;

		if ((status & SR0_EVENT_MASK) == SR0_TRANSMIT_DONE ||
		    (status & SR0_EVENT_MASK) == SR0_RETRANSMIT_DONE ||
		    (status & SR0_EVENT_MASK) == SR0_TRANSMIT_NO_CRC_DONE) {
			int tx_status;
			outb(CR0_STATUS_1, ioaddr);
			tx_status = inw(ioaddr);
			
			if (tx_status & TX_OK) {
				dev->stats.tx_packets++;
				dev->stats.collisions += tx_status & TX_NCOL_MASK;
			} else {
				if (tx_status & (TX_LOST_CTS | TX_LOST_CRS))
					dev->stats.tx_carrier_errors++;
				if (tx_status & TX_UND_RUN)
					dev->stats.tx_fifo_errors++;
				if (!(tx_status & TX_HRT_BEAT))
					dev->stats.tx_heartbeat_errors++;
				if (tx_status & TX_MAX_COL)
					dev->stats.tx_aborted_errors++;
				
				if ((tx_status | (TX_LOST_CRS | TX_LOST_CTS | TX_UND_RUN | TX_HRT_BEAT | TX_MAX_COL)) != (TX_LOST_CRS | TX_LOST_CTS | TX_UND_RUN | TX_HRT_BEAT | TX_MAX_COL))
					dev->stats.tx_errors++;


				znet_transceiver_power (dev, 0);
				znet_transceiver_power (dev, 1);
			}
			netif_wake_queue (dev);
		}

		if ((status & SR0_RECEPTION) ||
		    (status & SR0_EVENT_MASK) == SR0_STOP_REG_HIT) {
			znet_rx(dev);
		}
		
		outb(CR0_INT_ACK, ioaddr);
	} while (boguscnt--);

	spin_unlock (&znet->lock);

	return IRQ_RETVAL(handled);
}

static void znet_rx(struct net_device *dev)
{
	struct znet_private *znet = netdev_priv(dev);
	int ioaddr = dev->base_addr;
	int boguscount = 1;
	short next_frame_end_offset = 0; 		
	short *cur_frame_end;
	short cur_frame_end_offset;

	outb(CR0_STATUS_2, ioaddr);
	cur_frame_end_offset = inw(ioaddr);

	if (cur_frame_end_offset == znet->rx_cur - znet->rx_start) {
		printk(KERN_WARNING "%s: Interrupted, but nothing to receive, offset %03x.\n",
			   dev->name, cur_frame_end_offset);
		return;
	}

	while (znet->rx_start + cur_frame_end_offset != znet->rx_cur &&
	       ++boguscount < 5) {
		unsigned short hi_cnt, lo_cnt, hi_status, lo_status;
		int count, status;

		if (cur_frame_end_offset < 4) {
			memcpy(znet->rx_end, znet->rx_start, 8);
			cur_frame_end_offset += (RX_BUF_SIZE/2);
		}
		cur_frame_end = znet->rx_start + cur_frame_end_offset - 4;

		lo_status = *cur_frame_end++;
		hi_status = *cur_frame_end++;
		status = ((hi_status & 0xff) << 8) + (lo_status & 0xff);
		lo_cnt = *cur_frame_end++;
		hi_cnt = *cur_frame_end++;
		count = ((hi_cnt & 0xff) << 8) + (lo_cnt & 0xff);

		if (znet_debug > 5)
		  printk(KERN_DEBUG "Constructing trailer at location %03x, %04x %04x %04x %04x"
				 " count %#x status %04x.\n",
				 cur_frame_end_offset<<1, lo_status, hi_status, lo_cnt, hi_cnt,
				 count, status);
		cur_frame_end[-4] = status;
		cur_frame_end[-3] = next_frame_end_offset;
		cur_frame_end[-2] = count;
		next_frame_end_offset = cur_frame_end_offset;
		cur_frame_end_offset -= ((count + 1)>>1) + 3;
		if (cur_frame_end_offset < 0)
		  cur_frame_end_offset += RX_BUF_SIZE/2;
	}

	
	do {
		ushort *this_rfp_ptr = znet->rx_start + next_frame_end_offset;
		int status = this_rfp_ptr[-4];
		int pkt_len = this_rfp_ptr[-2];

		if (znet_debug > 5)
		  printk(KERN_DEBUG "Looking at trailer ending at %04x status %04x length %03x"
				 " next %04x.\n", next_frame_end_offset<<1, status, pkt_len,
				 this_rfp_ptr[-3]<<1);
		
		if ( ! (status & RX_RCV_OK)) { 
			dev->stats.rx_errors++;
			if (status & RX_CRC_ERR) dev->stats.rx_crc_errors++;
			if (status & RX_ALG_ERR) dev->stats.rx_frame_errors++;
#if 0
			if (status & 0x0200) dev->stats.rx_over_errors++; 
			if (status & 0x0100) dev->stats.rx_fifo_errors++;
#else
			
			if (status & RX_OVRRUN) dev->stats.rx_over_errors++;
#endif
			if (status & RX_SRT_FRM) dev->stats.rx_length_errors++;
		} else if (pkt_len > 1536) {
			dev->stats.rx_length_errors++;
		} else {
			
			struct sk_buff *skb;

			skb = netdev_alloc_skb(dev, pkt_len);
			if (skb == NULL) {
				if (znet_debug)
				  printk(KERN_WARNING "%s: Memory squeeze, dropping packet.\n", dev->name);
				dev->stats.rx_dropped++;
				break;
			}

			if (&znet->rx_cur[(pkt_len+1)>>1] > znet->rx_end) {
				int semi_cnt = (znet->rx_end - znet->rx_cur)<<1;
				memcpy(skb_put(skb,semi_cnt), znet->rx_cur, semi_cnt);
				memcpy(skb_put(skb,pkt_len-semi_cnt), znet->rx_start,
					   pkt_len - semi_cnt);
			} else {
				memcpy(skb_put(skb,pkt_len), znet->rx_cur, pkt_len);
				if (znet_debug > 6) {
					unsigned int *packet = (unsigned int *) skb->data;
					printk(KERN_DEBUG "Packet data is %08x %08x %08x %08x.\n", packet[0],
						   packet[1], packet[2], packet[3]);
				}
		  }
		  skb->protocol=eth_type_trans(skb,dev);
		  netif_rx(skb);
		  dev->stats.rx_packets++;
		  dev->stats.rx_bytes += pkt_len;
		}
		znet->rx_cur = this_rfp_ptr;
		if (znet->rx_cur >= znet->rx_end)
			znet->rx_cur -= RX_BUF_SIZE/2;
		update_stop_hit(ioaddr, (znet->rx_cur - znet->rx_start)<<1);
		next_frame_end_offset = this_rfp_ptr[-3];
		if (next_frame_end_offset == 0)		
			break;			
		this_rfp_ptr = znet->rx_start + next_frame_end_offset;
	} while (--boguscount);

}

static int znet_close(struct net_device *dev)
{
	int ioaddr = dev->base_addr;

	netif_stop_queue (dev);

	outb(OP0_RESET, ioaddr);			

	if (znet_debug > 1)
		printk(KERN_DEBUG "%s: Shutting down ethercard.\n", dev->name);
	
	znet_transceiver_power (dev, 0);

	znet_release_resources (dev);

	return 0;
}

static void show_dma(struct net_device *dev)
{
	short ioaddr = dev->base_addr;
	unsigned char stat = inb (ioaddr);
	struct znet_private *znet = netdev_priv(dev);
	unsigned long flags;
	short dma_port = ((znet->tx_dma&3)<<2) + IO_DMA2_BASE;
	unsigned addr = inb(dma_port);
	short residue;

	addr |= inb(dma_port) << 8;
	residue = get_dma_residue(znet->tx_dma);

	if (znet_debug > 1) {
		flags=claim_dma_lock();
		printk(KERN_DEBUG "Stat:%02x Addr: %04x cnt:%3x\n",
		       stat, addr<<1, residue);
		release_dma_lock(flags);
	}
}

static void hardware_init(struct net_device *dev)
{
	unsigned long flags;
	short ioaddr = dev->base_addr;
	struct znet_private *znet = netdev_priv(dev);

	znet->rx_cur = znet->rx_start;
	znet->tx_cur = znet->tx_start;

	
	outb(OP0_RESET, ioaddr);

	flags=claim_dma_lock();
	disable_dma(znet->rx_dma); 		
	clear_dma_ff(znet->rx_dma);
	set_dma_mode(znet->rx_dma, DMA_RX_MODE);
	set_dma_addr(znet->rx_dma, (unsigned int) znet->rx_start);
	set_dma_count(znet->rx_dma, RX_BUF_SIZE);
	enable_dma(znet->rx_dma);
	
	disable_dma(znet->tx_dma);
	clear_dma_ff(znet->tx_dma);
	set_dma_mode(znet->tx_dma, DMA_TX_MODE);
	set_dma_addr(znet->tx_dma, (unsigned int) znet->tx_start);
	set_dma_count(znet->tx_dma, znet->tx_buf_len<<1);
	enable_dma(znet->tx_dma);
	release_dma_lock(flags);

	if (znet_debug > 1)
	  printk(KERN_DEBUG "%s: Initializing the i82593, rx buf %p tx buf %p\n",
			 dev->name, znet->rx_start,znet->tx_start);
	*znet->tx_cur++ = 0;
	*znet->tx_cur++ = 0;
	show_dma(dev);
	outb(OP0_CONFIGURE | CR0_CHNL, ioaddr);

	znet_set_multicast_list (dev);

	*znet->tx_cur++ = 6;
	memcpy(znet->tx_cur, dev->dev_addr, 6);
	znet->tx_cur += 3;
	show_dma(dev);
	outb(OP0_IA_SETUP | CR0_CHNL, ioaddr);
	show_dma(dev);

	update_stop_hit(ioaddr, 8192);
	if (znet_debug > 1)  printk(KERN_DEBUG "enabling Rx.\n");
	outb(OP0_RCV_ENABLE, ioaddr);
	netif_start_queue (dev);
}

static void update_stop_hit(short ioaddr, unsigned short rx_stop_offset)
{
	outb(OP0_SWIT_TO_PORT_1 | CR0_CHNL, ioaddr);
	if (znet_debug > 5)
	  printk(KERN_DEBUG "Updating stop hit with value %02x.\n",
			 (rx_stop_offset >> 6) | CR1_STOP_REG_UPDATE);
	outb((rx_stop_offset >> 6) | CR1_STOP_REG_UPDATE, ioaddr);
	outb(OP1_SWIT_TO_PORT_0, ioaddr);
}

static __exit void znet_cleanup (void)
{
	if (znet_dev) {
		struct znet_private *znet = netdev_priv(znet_dev);

		unregister_netdev (znet_dev);
		kfree (znet->rx_start);
		kfree (znet->tx_start);
		free_netdev (znet_dev);
	}
}

module_init (znet_probe);
module_exit (znet_cleanup);
