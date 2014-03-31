/*

  Sun3 Lance ethernet driver, by Sam Creasey (sammy@users.qual.net).
  This driver is a part of the linux kernel, and is thus distributed
  under the GNU General Public License.

  The values used in LANCE_OBIO and LANCE_IRQ seem to be empirically
  true for the correct IRQ and address of the lance registers.  They
  have not been widely tested, however.  What we probably need is a
  "proper" way to search for a device in the sun3's prom, but, alas,
  linux has no such thing.

  This driver is largely based on atarilance.c, by Roman Hodek.  Other
  sources of inspiration were the NetBSD sun3 am7990 driver, and the
  linux sparc lance driver (sunlance.c).

  There are more assumptions made throughout this driver, it almost
  certainly still needs work, but it does work at least for RARP/BOOTP and
  mounting the root NFS filesystem.

*/

static char *version = "sun3lance.c: v1.2 1/12/2001  Sam Creasey (sammy@sammy.net)\n";

#include <linux/module.h>
#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/bitops.h>

#include <asm/cacheflush.h>
#include <asm/setup.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/dvma.h>
#include <asm/idprom.h>
#include <asm/machines.h>

#ifdef CONFIG_SUN3
#include <asm/sun3mmu.h>
#else
#include <asm/sun3xprom.h>
#endif

#define LANCE_OBIO 0x120000
#define LANCE_IRQ IRQ_AUTO_3


#define	LANCE_DEBUG	0

#ifdef LANCE_DEBUG
static int lance_debug = LANCE_DEBUG;
#else
static int lance_debug = 1;
#endif
module_param(lance_debug, int, 0);
MODULE_PARM_DESC(lance_debug, "SUN3 Lance debug level (0-3)");
MODULE_LICENSE("GPL");

#define	DPRINTK(n,a) \
	do {  \
		if (lance_debug >= n)  \
			printk a; \
	} while( 0 )



#define TX_LOG_RING_SIZE			3
#define RX_LOG_RING_SIZE			5


#define TX_RING_SIZE			(1 << TX_LOG_RING_SIZE)
#define TX_RING_LEN_BITS		(TX_LOG_RING_SIZE << 5)
#define	TX_RING_MOD_MASK		(TX_RING_SIZE - 1)

#define RX_RING_SIZE			(1 << RX_LOG_RING_SIZE)
#define RX_RING_LEN_BITS		(RX_LOG_RING_SIZE << 5)
#define	RX_RING_MOD_MASK		(RX_RING_SIZE - 1)

#define PKT_BUF_SZ		1544

#define	PKTBUF_ADDR(head)	(void *)((unsigned long)(MEM) | (head)->base)


struct lance_rx_head {
	unsigned short	base;		
	volatile unsigned char	flag;
	unsigned char  base_hi;	
	short buf_length;	
	volatile short msg_length;	
};

struct lance_tx_head {
	unsigned short base;		
	volatile unsigned char	flag;
	unsigned char base_hi;	
	short length;		
	volatile short misc;
};

struct lance_init_block {
	unsigned short	mode;		
	unsigned char	hwaddr[6];	
	unsigned int    filter[2];	
	
	unsigned short rdra;
	unsigned short rlen;
	unsigned short tdra;
	unsigned short tlen;
	unsigned short pad[4]; 
};

struct lance_memory {
	struct lance_init_block	init;
	struct lance_tx_head	tx_head[TX_RING_SIZE];
	struct lance_rx_head	rx_head[RX_RING_SIZE];
	char   rx_data[RX_RING_SIZE][PKT_BUF_SZ];
	char   tx_data[TX_RING_SIZE][PKT_BUF_SZ];
};


struct lance_private {
	volatile unsigned short	*iobase;
	struct lance_memory	*mem;
     	int new_rx, new_tx;	
	int old_tx, old_rx;     
	long	    tx_full;
	long	    lock;
};


#define	MEM	lp->mem
#define	DREG	lp->iobase[0]
#define	AREG	lp->iobase[1]
#define	REGA(a)	(*( AREG = (a), &DREG ))


#define TMD1_ENP		0x01	
#define TMD1_STP		0x02	
#define TMD1_DEF		0x04	
#define TMD1_ONE		0x08	
#define TMD1_MORE		0x10	
#define TMD1_ERR		0x40	
#define TMD1_OWN 		0x80	

#define TMD1_OWN_CHIP	TMD1_OWN
#define TMD1_OWN_HOST	0

#define TMD3_TDR		0x03FF	
#define TMD3_RTRY		0x0400	
#define TMD3_LCAR		0x0800	
#define TMD3_LCOL		0x1000	
#define TMD3_UFLO		0x4000	
#define TMD3_BUFF		0x8000	

#define RMD1_ENP		0x01	
#define RMD1_STP		0x02	
#define RMD1_BUFF		0x04	
#define RMD1_CRC		0x08	
#define RMD1_OFLO		0x10	
#define RMD1_FRAM		0x20	
#define RMD1_ERR		0x40	
#define RMD1_OWN 		0x80	

#define RMD1_OWN_CHIP	RMD1_OWN
#define RMD1_OWN_HOST	0

#define CSR0	0		
#define CSR1	1		
#define CSR2	2		
#define CSR3	3		
#define CSR8	8	  	
#define CSR15	15		

#define CSR0_INIT	0x0001		
#define CSR0_STRT	0x0002		
#define CSR0_STOP	0x0004		
#define CSR0_TDMD	0x0008		
#define CSR0_TXON	0x0010		
#define CSR0_RXON	0x0020		
#define CSR0_INEA	0x0040		
#define CSR0_INTR	0x0080		
#define CSR0_IDON	0x0100		
#define CSR0_TINT	0x0200		
#define CSR0_RINT	0x0400		
#define CSR0_MERR	0x0800		
#define CSR0_MISS	0x1000		
#define CSR0_CERR	0x2000		
#define CSR0_BABL	0x4000		
#define CSR0_ERR	0x8000		

#define CSR3_BCON	0x0001		
#define CSR3_ACON	0x0002		
#define CSR3_BSWP	0x0004		


static int lance_probe( struct net_device *dev);
static int lance_open( struct net_device *dev );
static void lance_init_ring( struct net_device *dev );
static int lance_start_xmit( struct sk_buff *skb, struct net_device *dev );
static irqreturn_t lance_interrupt( int irq, void *dev_id);
static int lance_rx( struct net_device *dev );
static int lance_close( struct net_device *dev );
static void set_multicast_list( struct net_device *dev );


struct net_device * __init sun3lance_probe(int unit)
{
	struct net_device *dev;
	static int found;
	int err = -ENODEV;

	if (!MACH_IS_SUN3 && !MACH_IS_SUN3X)
		return ERR_PTR(-ENODEV);

	
	switch(idprom->id_machtype) {
	case SM_SUN3|SM_3_50:
	case SM_SUN3|SM_3_60:
	case SM_SUN3X|SM_3_80:
		
		break;

	default:
		return ERR_PTR(-ENODEV);
	}

	if (found)
		return ERR_PTR(-ENODEV);

	dev = alloc_etherdev(sizeof(struct lance_private));
	if (!dev)
		return ERR_PTR(-ENOMEM);
	if (unit >= 0) {
		sprintf(dev->name, "eth%d", unit);
		netdev_boot_setup_check(dev);
	}

	if (!lance_probe(dev))
		goto out;

	err = register_netdev(dev);
	if (err)
		goto out1;
	found = 1;
	return dev;

out1:
#ifdef CONFIG_SUN3
	iounmap((void __iomem *)dev->base_addr);
#endif
out:
	free_netdev(dev);
	return ERR_PTR(err);
}

static const struct net_device_ops lance_netdev_ops = {
	.ndo_open		= lance_open,
	.ndo_stop		= lance_close,
	.ndo_start_xmit		= lance_start_xmit,
	.ndo_set_rx_mode	= set_multicast_list,
	.ndo_set_mac_address	= NULL,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_validate_addr	= eth_validate_addr,
};

static int __init lance_probe( struct net_device *dev)
{
	unsigned long ioaddr;

	struct lance_private	*lp;
	int 			i;
	static int 		did_version;
	volatile unsigned short *ioaddr_probe;
	unsigned short tmp1, tmp2;

#ifdef CONFIG_SUN3
	ioaddr = (unsigned long)ioremap(LANCE_OBIO, PAGE_SIZE);
	if (!ioaddr)
		return 0;
#else
	ioaddr = SUN3X_LANCE;
#endif

	
	

	ioaddr_probe = (volatile unsigned short *)ioaddr;
	tmp1 = ioaddr_probe[0];
	tmp2 = ioaddr_probe[1];

	ioaddr_probe[1] = CSR0;
	ioaddr_probe[0] = CSR0_INIT | CSR0_STOP;

	if(ioaddr_probe[0] != CSR0_STOP) {
		ioaddr_probe[0] = tmp1;
		ioaddr_probe[1] = tmp2;

#ifdef CONFIG_SUN3
		iounmap((void __iomem *)ioaddr);
#endif
		return 0;
	}

	lp = netdev_priv(dev);

	
	MEM = dvma_malloc_align(sizeof(struct lance_memory), 0x10000);
	if (MEM == NULL) {
#ifdef CONFIG_SUN3
		iounmap((void __iomem *)ioaddr);
#endif
		printk(KERN_WARNING "SUN3 Lance couldn't allocate DVMA memory\n");
		return 0;
	}

	lp->iobase = (volatile unsigned short *)ioaddr;
	dev->base_addr = (unsigned long)ioaddr; 

	REGA(CSR0) = CSR0_STOP;

	if (request_irq(LANCE_IRQ, lance_interrupt, IRQF_DISABLED, "SUN3 Lance", dev) < 0) {
#ifdef CONFIG_SUN3
		iounmap((void __iomem *)ioaddr);
#endif
		dvma_free((void *)MEM);
		printk(KERN_WARNING "SUN3 Lance unable to allocate IRQ\n");
		return 0;
	}
	dev->irq = (unsigned short)LANCE_IRQ;


	printk("%s: SUN3 Lance at io %#lx, mem %#lx, irq %d, hwaddr ",
		   dev->name,
		   (unsigned long)ioaddr,
		   (unsigned long)MEM,
		   dev->irq);

	
	for(i = 0; i < 6 ; i++)
	     dev->dev_addr[i] = idprom->id_ethaddr[i];

	
	MEM->init.hwaddr[0] = dev->dev_addr[1];
	MEM->init.hwaddr[1] = dev->dev_addr[0];
	MEM->init.hwaddr[2] = dev->dev_addr[3];
	MEM->init.hwaddr[3] = dev->dev_addr[2];
	MEM->init.hwaddr[4] = dev->dev_addr[5];
	MEM->init.hwaddr[5] = dev->dev_addr[4];

	printk("%pM\n", dev->dev_addr);

	MEM->init.mode = 0x0000;
	MEM->init.filter[0] = 0x00000000;
	MEM->init.filter[1] = 0x00000000;
	MEM->init.rdra = dvma_vtob(MEM->rx_head);
	MEM->init.rlen    = (RX_LOG_RING_SIZE << 13) |
		(dvma_vtob(MEM->rx_head) >> 16);
	MEM->init.tdra = dvma_vtob(MEM->tx_head);
	MEM->init.tlen    = (TX_LOG_RING_SIZE << 13) |
		(dvma_vtob(MEM->tx_head) >> 16);

	DPRINTK(2, ("initaddr: %08lx rx_ring: %08lx tx_ring: %08lx\n",
	       dvma_vtob(&(MEM->init)), dvma_vtob(MEM->rx_head),
	       (dvma_vtob(MEM->tx_head))));

	if (did_version++ == 0)
		printk( version );

	dev->netdev_ops = &lance_netdev_ops;
	set_bit(__LINK_STATE_PRESENT, &dev->state);


	return 1;
}

static int lance_open( struct net_device *dev )
{
	struct lance_private *lp = netdev_priv(dev);
	int i;

	DPRINTK( 2, ( "%s: lance_open()\n", dev->name ));

	REGA(CSR0) = CSR0_STOP;

	lance_init_ring(dev);

	
	REGA(CSR0) = CSR0_INIT;

	i = 1000000;
	while (--i > 0)
		if (DREG & CSR0_IDON)
			break;
	if (i <= 0 || (DREG & CSR0_ERR)) {
		DPRINTK( 2, ( "lance_open(): opening %s failed, i=%d, csr0=%04x\n",
					  dev->name, i, DREG ));
		DREG = CSR0_STOP;
		return -EIO;
	}

	DREG = CSR0_IDON | CSR0_STRT | CSR0_INEA;

	netif_start_queue(dev);

	DPRINTK( 2, ( "%s: LANCE is open, csr0 %04x\n", dev->name, DREG ));

	return 0;
}



static void lance_init_ring( struct net_device *dev )
{
	struct lance_private *lp = netdev_priv(dev);
	int i;

	lp->lock = 0;
	lp->tx_full = 0;
	lp->new_rx = lp->new_tx = 0;
	lp->old_rx = lp->old_tx = 0;

	for( i = 0; i < TX_RING_SIZE; i++ ) {
		MEM->tx_head[i].base = dvma_vtob(MEM->tx_data[i]);
		MEM->tx_head[i].flag = 0;
 		MEM->tx_head[i].base_hi =
			(dvma_vtob(MEM->tx_data[i])) >>16;
		MEM->tx_head[i].length = 0;
		MEM->tx_head[i].misc = 0;
	}

	for( i = 0; i < RX_RING_SIZE; i++ ) {
		MEM->rx_head[i].base = dvma_vtob(MEM->rx_data[i]);
		MEM->rx_head[i].flag = RMD1_OWN_CHIP;
		MEM->rx_head[i].base_hi =
			(dvma_vtob(MEM->rx_data[i])) >> 16;
		MEM->rx_head[i].buf_length = -PKT_BUF_SZ | 0xf000;
		MEM->rx_head[i].msg_length = 0;
	}

	
	MEM->init.hwaddr[0] = dev->dev_addr[1];
	MEM->init.hwaddr[1] = dev->dev_addr[0];
	MEM->init.hwaddr[2] = dev->dev_addr[3];
	MEM->init.hwaddr[3] = dev->dev_addr[2];
	MEM->init.hwaddr[4] = dev->dev_addr[5];
	MEM->init.hwaddr[5] = dev->dev_addr[4];

	MEM->init.mode = 0x0000;
	MEM->init.filter[0] = 0x00000000;
	MEM->init.filter[1] = 0x00000000;
	MEM->init.rdra = dvma_vtob(MEM->rx_head);
	MEM->init.rlen    = (RX_LOG_RING_SIZE << 13) |
		(dvma_vtob(MEM->rx_head) >> 16);
	MEM->init.tdra = dvma_vtob(MEM->tx_head);
	MEM->init.tlen    = (TX_LOG_RING_SIZE << 13) |
		(dvma_vtob(MEM->tx_head) >> 16);


	
	REGA(CSR1) = dvma_vtob(&(MEM->init));
	REGA(CSR2) = dvma_vtob(&(MEM->init)) >> 16;

#ifdef CONFIG_SUN3X
	REGA(CSR3) = CSR3_BSWP | CSR3_ACON | CSR3_BCON;
#else
	REGA(CSR3) = CSR3_BSWP;
#endif

}


static int lance_start_xmit( struct sk_buff *skb, struct net_device *dev )
{
	struct lance_private *lp = netdev_priv(dev);
	int entry, len;
	struct lance_tx_head *head;
	unsigned long flags;

	DPRINTK( 1, ( "%s: transmit start.\n",
		      dev->name));

	
	if (netif_queue_stopped(dev)) {
		int tickssofar = jiffies - dev_trans_start(dev);
		if (tickssofar < HZ/5)
			return NETDEV_TX_BUSY;

		DPRINTK( 1, ( "%s: transmit timed out, status %04x, resetting.\n",
					  dev->name, DREG ));
		DREG = CSR0_STOP;
		REGA(CSR3) = CSR3_BSWP;
		dev->stats.tx_errors++;

		if(lance_debug >= 2) {
			int i;
			printk("Ring data: old_tx %d new_tx %d%s new_rx %d\n",
			       lp->old_tx, lp->new_tx,
			       lp->tx_full ? " (full)" : "",
			       lp->new_rx );
			for( i = 0 ; i < RX_RING_SIZE; i++ )
				printk( "rx #%d: base=%04x blen=%04x mlen=%04x\n",
					i, MEM->rx_head[i].base,
					-MEM->rx_head[i].buf_length,
					MEM->rx_head[i].msg_length);
			for( i = 0 ; i < TX_RING_SIZE; i++ )
				printk("tx #%d: base=%04x len=%04x misc=%04x\n",
				       i, MEM->tx_head[i].base,
				       -MEM->tx_head[i].length,
				       MEM->tx_head[i].misc );
		}

		lance_init_ring(dev);
		REGA( CSR0 ) = CSR0_INEA | CSR0_INIT | CSR0_STRT;

		netif_start_queue(dev);

		return NETDEV_TX_OK;
	}




	netif_stop_queue(dev);

	if (test_and_set_bit( 0, (void*)&lp->lock ) != 0) {
		printk( "%s: tx queue lock!.\n", dev->name);
		
		return NETDEV_TX_BUSY;
	}

	AREG = CSR0;
  	DPRINTK( 2, ( "%s: lance_start_xmit() called, csr0 %4.4x.\n",
  				  dev->name, DREG ));

#ifdef CONFIG_SUN3X
	
	if(!(DREG & CSR0_INIT)) {
		DPRINTK( 1, ("INIT not set, reinitializing...\n"));
		REGA( CSR0 ) = CSR0_STOP;
		lance_init_ring(dev);
		REGA( CSR0 ) = CSR0_INIT | CSR0_STRT;
	}
#endif

	
#if 0
	if (lance_debug >= 2) {
		printk( "%s: TX pkt %d type 0x%04x"
			" from %s to %s"
			" data at 0x%08x len %d\n",
			dev->name, lp->new_tx, ((u_short *)skb->data)[6],
			DEV_ADDR(&skb->data[6]), DEV_ADDR(skb->data),
			(int)skb->data, (int)skb->len );
	}
#endif
	local_irq_save(flags);

	
	entry = lp->new_tx;
	head  = &(MEM->tx_head[entry]);


	len = (ETH_ZLEN < skb->len) ? skb->len : ETH_ZLEN;

	head->length = (-len) | 0xf000;
	head->misc = 0;

	skb_copy_from_linear_data(skb, PKTBUF_ADDR(head), skb->len);
	if (len != skb->len)
		memset(PKTBUF_ADDR(head) + skb->len, 0, len-skb->len);

	head->flag = TMD1_OWN_CHIP | TMD1_ENP | TMD1_STP;
	lp->new_tx = (lp->new_tx + 1) & TX_RING_MOD_MASK;
	dev->stats.tx_bytes += skb->len;

	
	REGA(CSR0) = CSR0_INEA | CSR0_TDMD | CSR0_STRT;
	AREG = CSR0;
  	DPRINTK( 2, ( "%s: lance_start_xmit() exiting, csr0 %4.4x.\n",
  				  dev->name, DREG ));
	dev_kfree_skb(skb);

	lp->lock = 0;
	if ((MEM->tx_head[(entry+1) & TX_RING_MOD_MASK].flag & TMD1_OWN) ==
	    TMD1_OWN_HOST)
		netif_start_queue(dev);

	local_irq_restore(flags);

	return NETDEV_TX_OK;
}


static irqreturn_t lance_interrupt( int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct lance_private *lp = netdev_priv(dev);
	int csr0;
	static int in_interrupt;

	if (dev == NULL) {
		DPRINTK( 1, ( "lance_interrupt(): invalid dev_id\n" ));
		return IRQ_NONE;
	}

	if (in_interrupt)
		DPRINTK( 2, ( "%s: Re-entering the interrupt handler.\n", dev->name ));
	in_interrupt = 1;

 still_more:
	flush_cache_all();

	AREG = CSR0;
	csr0 = DREG;

	
	DREG = csr0 & (CSR0_TINT | CSR0_RINT | CSR0_IDON);

	
	if(csr0 & CSR0_ERR)
		DREG = CSR0_BABL | CSR0_MERR | CSR0_CERR | CSR0_MISS;


	DPRINTK( 2, ( "%s: interrupt  csr0=%04x new csr=%04x.\n",
		      dev->name, csr0, DREG ));

	if (csr0 & CSR0_TINT) {			
		int old_tx = lp->old_tx;


		while( old_tx != lp->new_tx) {
			struct lance_tx_head *head = &(MEM->tx_head[old_tx]);

			DPRINTK(3, ("on tx_ring %d\n", old_tx));

			if (head->flag & TMD1_OWN_CHIP)
				break; 

			if (head->flag & TMD1_ERR) {
				int status = head->misc;
				dev->stats.tx_errors++;
				if (status & TMD3_RTRY) dev->stats.tx_aborted_errors++;
				if (status & TMD3_LCAR) dev->stats.tx_carrier_errors++;
				if (status & TMD3_LCOL) dev->stats.tx_window_errors++;
				if (status & (TMD3_UFLO | TMD3_BUFF)) {
					dev->stats.tx_fifo_errors++;
					printk("%s: Tx FIFO error\n",
					       dev->name);
					REGA(CSR0) = CSR0_STOP;
					REGA(CSR3) = CSR3_BSWP;
					lance_init_ring(dev);
					REGA(CSR0) = CSR0_STRT | CSR0_INEA;
					return IRQ_HANDLED;
				}
			} else if(head->flag & (TMD1_ENP | TMD1_STP)) {

				head->flag &= ~(TMD1_ENP | TMD1_STP);
				if(head->flag & (TMD1_ONE | TMD1_MORE))
					dev->stats.collisions++;

				dev->stats.tx_packets++;
				DPRINTK(3, ("cleared tx ring %d\n", old_tx));
			}
			old_tx = (old_tx +1) & TX_RING_MOD_MASK;
		}

		lp->old_tx = old_tx;
	}


	if (netif_queue_stopped(dev)) {
		
		netif_start_queue(dev);
		netif_wake_queue(dev);
	}

	if (csr0 & CSR0_RINT)			
		lance_rx( dev );

	
	if (csr0 & CSR0_BABL) dev->stats.tx_errors++; 
	if (csr0 & CSR0_MISS) dev->stats.rx_errors++; 
	if (csr0 & CSR0_MERR) {
		DPRINTK( 1, ( "%s: Bus master arbitration failure (?!?), "
			      "status %04x.\n", dev->name, csr0 ));
		
		REGA(CSR0) = CSR0_STOP;
		REGA(CSR3) = CSR3_BSWP;
		lance_init_ring(dev);
		REGA(CSR0) = CSR0_STRT | CSR0_INEA;
	}


    

	REGA(CSR0) = CSR0_INEA;

	if(DREG & (CSR0_RINT | CSR0_TINT)) {
	     DPRINTK(2, ("restarting interrupt, csr0=%#04x\n", DREG));
	     goto still_more;
	}

	DPRINTK( 2, ( "%s: exiting interrupt, csr0=%#04x.\n",
				  dev->name, DREG ));
	in_interrupt = 0;
	return IRQ_HANDLED;
}

static int lance_rx( struct net_device *dev )
{
	struct lance_private *lp = netdev_priv(dev);
	int entry = lp->new_rx;

	
	while( (MEM->rx_head[entry].flag & RMD1_OWN) == RMD1_OWN_HOST ) {
		struct lance_rx_head *head = &(MEM->rx_head[entry]);
		int status = head->flag;

		if (status != (RMD1_ENP|RMD1_STP)) {  
			if (status & RMD1_ENP)	
				dev->stats.rx_errors++; 
			if (status & RMD1_FRAM) dev->stats.rx_frame_errors++;
			if (status & RMD1_OFLO) dev->stats.rx_over_errors++;
			if (status & RMD1_CRC) dev->stats.rx_crc_errors++;
			if (status & RMD1_BUFF) dev->stats.rx_fifo_errors++;
			head->flag &= (RMD1_ENP|RMD1_STP);
		} else {
			
			short pkt_len = (head->msg_length & 0xfff) - 4;
			struct sk_buff *skb;

			if (pkt_len < 60) {
				printk( "%s: Runt packet!\n", dev->name );
				dev->stats.rx_errors++;
			}
			else {
				skb = netdev_alloc_skb(dev, pkt_len + 2);
				if (skb == NULL) {
					DPRINTK( 1, ( "%s: Memory squeeze, deferring packet.\n",
						      dev->name ));

					dev->stats.rx_dropped++;
					head->msg_length = 0;
					head->flag |= RMD1_OWN_CHIP;
					lp->new_rx = (lp->new_rx+1) &
					     RX_RING_MOD_MASK;
				}

#if 0
				if (lance_debug >= 3) {
					u_char *data = PKTBUF_ADDR(head);
					printk("%s: RX pkt %d type 0x%04x"
					       " from %pM to %pM",
					       dev->name, lp->new_tx, ((u_short *)data)[6],
					       &data[6], data);

					printk(" data %02x %02x %02x %02x %02x %02x %02x %02x "
					       "len %d at %08x\n",
					       data[15], data[16], data[17], data[18],
					       data[19], data[20], data[21], data[22],
					       pkt_len, data);
				}
#endif
				if (lance_debug >= 3) {
					u_char *data = PKTBUF_ADDR(head);
					printk( "%s: RX pkt %d type 0x%04x len %d\n ", dev->name, entry, ((u_short *)data)[6], pkt_len);
				}


				skb_reserve( skb, 2 );	
				skb_put( skb, pkt_len );	
				skb_copy_to_linear_data(skb,
						 PKTBUF_ADDR(head),
						 pkt_len);

				skb->protocol = eth_type_trans( skb, dev );
				netif_rx( skb );
				dev->stats.rx_packets++;
				dev->stats.rx_bytes += pkt_len;
			}
		}

		head->msg_length = 0;
		head->flag = RMD1_OWN_CHIP;

		entry = lp->new_rx = (lp->new_rx +1) & RX_RING_MOD_MASK;
	}

	

	return 0;
}


static int lance_close( struct net_device *dev )
{
	struct lance_private *lp = netdev_priv(dev);

	netif_stop_queue(dev);

	AREG = CSR0;

	DPRINTK( 2, ( "%s: Shutting down ethercard, status was %2.2x.\n",
				  dev->name, DREG ));

	DREG = CSR0_STOP;
	return 0;
}



static void set_multicast_list( struct net_device *dev )
{
	struct lance_private *lp = netdev_priv(dev);

	if(netif_queue_stopped(dev))
		
		return;

	
	DREG = CSR0_STOP; 

	if (dev->flags & IFF_PROMISC) {
		
		DPRINTK( 3, ( "%s: Promiscuous mode enabled.\n", dev->name ));
		REGA( CSR15 ) = 0x8000; 
	} else {
		short multicast_table[4];
		int num_addrs = netdev_mc_count(dev);
		int i;
		memset( multicast_table, (num_addrs == 0) ? 0 : -1,
				sizeof(multicast_table) );
		for( i = 0; i < 4; i++ )
			REGA( CSR8+i ) = multicast_table[i];
		REGA( CSR15 ) = 0; 
	}

	REGA( CSR3 ) = CSR3_BSWP;

	
	REGA( CSR0 ) = CSR0_IDON | CSR0_INEA | CSR0_STRT;
}


#ifdef MODULE

static struct net_device *sun3lance_dev;

int __init init_module(void)
{
	sun3lance_dev = sun3lance_probe(-1);
	if (IS_ERR(sun3lance_dev))
		return PTR_ERR(sun3lance_dev);
	return 0;
}

void __exit cleanup_module(void)
{
	unregister_netdev(sun3lance_dev);
#ifdef CONFIG_SUN3
	iounmap((void __iomem *)sun3lance_dev->base_addr);
#endif
	free_netdev(sun3lance_dev);
}

#endif 

