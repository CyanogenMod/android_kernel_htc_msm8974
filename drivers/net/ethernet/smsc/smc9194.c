/*------------------------------------------------------------------------
 . smc9194.c
 . This is a driver for SMC's 9000 series of Ethernet cards.
 .
 . Copyright (C) 1996 by Erik Stahlman
 . This software may be used and distributed according to the terms
 . of the GNU General Public License, incorporated herein by reference.
 .
 . "Features" of the SMC chip:
 .   4608 byte packet memory. ( for the 91C92.  Others have more )
 .   EEPROM for configuration
 .   AUI/TP selection  ( mine has 10Base2/10BaseT select )
 .
 . Arguments:
 . 	io		 = for the base address
 .	irq	 = for the IRQ
 .	ifport = 0 for autodetect, 1 for TP, 2 for AUI ( or 10base2 )
 .
 . author:
 . 	Erik Stahlman				( erik@vt.edu )
 . contributors:
 .      Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 .
 . Hardware multicast code from Peter Cammaert ( pc@denkart.be )
 .
 . Sources:
 .    o   SMC databook
 .    o   skeleton.c by Donald Becker ( becker@scyld.com )
 .    o   ( a LOT of advice from Becker as well )
 .
 . History:
 .	12/07/95  Erik Stahlman  written, got receive/xmit handled
 . 	01/03/96  Erik Stahlman  worked out some bugs, actually usable!!! :-)
 .	01/06/96  Erik Stahlman	 cleaned up some, better testing, etc
 .	01/29/96  Erik Stahlman	 fixed autoirq, added multicast
 . 	02/01/96  Erik Stahlman	 1. disabled all interrupts in smc_reset
 .		   		 2. got rid of post-decrementing bug -- UGH.
 .	02/13/96  Erik Stahlman  Tried to fix autoirq failure.  Added more
 .				 descriptive error messages.
 .	02/15/96  Erik Stahlman  Fixed typo that caused detection failure
 . 	02/23/96  Erik Stahlman	 Modified it to fit into kernel tree
 .				 Added support to change hardware address
 .				 Cleared stats on opens
 .	02/26/96  Erik Stahlman	 Trial support for Kernel 1.2.13
 .				 Kludge for automatic IRQ detection
 .	03/04/96  Erik Stahlman	 Fixed kernel 1.3.70 +
 .				 Fixed bug reported by Gardner Buchanan in
 .				   smc_enable, with outw instead of outb
 .	03/06/96  Erik Stahlman  Added hardware multicast from Peter Cammaert
 .	04/14/00  Heiko Pruessing (SMA Regelsysteme)  Fixed bug in chip memory
 .				 allocation
 .      08/20/00  Arnaldo Melo   fix kfree(skb) in smc_hardware_send_packet
 .      12/15/00  Christian Jullien fix "Warning: kfree_skb on hard IRQ"
 .      11/08/01 Matt Domsch     Use common crc32 function
 ----------------------------------------------------------------------------*/

static const char version[] =
	"smc9194.c:v0.14 12/15/00 by Erik Stahlman (erik@vt.edu)\n";

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/crc32.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/bitops.h>

#include <asm/io.h>

#include "smc9194.h"

#define DRV_NAME "smc9194"


#ifdef __sh__
#undef USE_32_BIT
#else
#define USE_32_BIT 1
#endif

#if defined(__H8300H__) || defined(__H8300S__)
#define NO_AUTOPROBE
#undef insl
#undef outsl
#define insl(a,b,l)  io_insl_noswap(a,b,l)
#define outsl(a,b,l) io_outsl_noswap(a,b,l)
#endif


struct devlist {
	unsigned int port;
	unsigned int irq;
};

#if defined(CONFIG_H8S_EDOSK2674)
static struct devlist smc_devlist[] __initdata = {
	{.port = 0xf80000, .irq = 16},
	{.port = 0,        .irq = 0 },
};
#else
static struct devlist smc_devlist[] __initdata = {
	{.port = 0x200, .irq = 0},
	{.port = 0x220, .irq = 0},
	{.port = 0x240, .irq = 0},
	{.port = 0x260, .irq = 0},
	{.port = 0x280, .irq = 0},
	{.port = 0x2A0, .irq = 0},
	{.port = 0x2C0, .irq = 0},
	{.port = 0x2E0, .irq = 0},
	{.port = 0x300, .irq = 0},
	{.port = 0x320, .irq = 0},
	{.port = 0x340, .irq = 0},
	{.port = 0x360, .irq = 0},
	{.port = 0x380, .irq = 0},
	{.port = 0x3A0, .irq = 0},
	{.port = 0x3C0, .irq = 0},
	{.port = 0x3E0, .irq = 0},
	{.port = 0,     .irq = 0},
};
#endif
#define MEMORY_WAIT_TIME 16

#define SMC_DEBUG 0

#if (SMC_DEBUG > 2 )
#define PRINTK3(x) printk x
#else
#define PRINTK3(x)
#endif

#if SMC_DEBUG > 1
#define PRINTK2(x) printk x
#else
#define PRINTK2(x)
#endif

#ifdef SMC_DEBUG
#define PRINTK(x) printk x
#else
#define PRINTK(x)
#endif


#define CARDNAME "SMC9194"


struct smc_local {
	struct sk_buff * saved_skb;

	int	packets_waiting;
};



struct net_device *smc_init(int unit);

static int smc_open(struct net_device *dev);

static void smc_timeout(struct net_device *dev);

static int smc_close(struct net_device *dev);

static void smc_set_multicast_list(struct net_device *dev);



static irqreturn_t smc_interrupt(int irq, void *);
static inline void smc_rcv( struct net_device *dev );
static inline void smc_tx( struct net_device * dev );


static int smc_probe(struct net_device *dev, int ioaddr);

#if SMC_DEBUG > 2
static void print_packet( byte *, int );
#endif

#define tx_done(dev) 1

static void smc_hardware_send_packet( struct net_device * dev );

static netdev_tx_t  smc_wait_to_send_packet( struct sk_buff * skb,
					     struct net_device *dev );

static void smc_reset( int ioaddr );

static void smc_enable( int ioaddr );

static void smc_shutdown( int ioaddr );

static int smc_findirq( int ioaddr );

static void smc_reset( int ioaddr )
{
	SMC_SELECT_BANK( 0 );
	outw( RCR_SOFTRESET, ioaddr + RCR );

	
	SMC_DELAY( );

	outw( RCR_CLEAR, ioaddr + RCR );
	outw( TCR_CLEAR, ioaddr + TCR );

	SMC_SELECT_BANK( 1 );
	outw( inw( ioaddr + CONTROL ) | CTL_AUTO_RELEASE , ioaddr + CONTROL );

	
	SMC_SELECT_BANK( 2 );
	outw( MC_RESET, ioaddr + MMU_CMD );


	outb( 0, ioaddr + INT_MASK );
}

static void smc_enable( int ioaddr )
{
	SMC_SELECT_BANK( 0 );
	
	outw( TCR_NORMAL, ioaddr + TCR );
	outw( RCR_NORMAL, ioaddr + RCR );

	
	SMC_SELECT_BANK( 2 );
	outb( SMC_INTERRUPT_MASK, ioaddr + INT_MASK );
}

static void smc_shutdown( int ioaddr )
{
	
	SMC_SELECT_BANK( 2 );
	outb( 0, ioaddr + INT_MASK );

	
	SMC_SELECT_BANK( 0 );
	outb( RCR_CLEAR, ioaddr + RCR );
	outb( TCR_CLEAR, ioaddr + TCR );
#if 0
	
	SMC_SELECT_BANK( 1 );
	outw( inw( ioaddr + CONTROL ), CTL_POWERDOWN, ioaddr + CONTROL  );
#endif
}




static void smc_setmulticast(int ioaddr, struct net_device *dev)
{
	int			i;
	unsigned char		multicast_table[ 8 ];
	struct netdev_hw_addr *ha;
	
	unsigned char invert3[] = { 0, 4, 2, 6, 1, 5, 3, 7 };

	
	memset( multicast_table, 0, sizeof( multicast_table ) );

	netdev_for_each_mc_addr(ha, dev) {
		int position;

		
		position = ether_crc_le(6, ha->addr) & 0x3f;

		
		multicast_table[invert3[position&7]] |=
					(1<<invert3[(position>>3)&7]);

	}
	
	SMC_SELECT_BANK( 3 );

	for ( i = 0; i < 8 ; i++ ) {
		outb( multicast_table[i], ioaddr + MULTICAST1 + i );
	}
}

static netdev_tx_t smc_wait_to_send_packet(struct sk_buff *skb,
					   struct net_device *dev)
{
	struct smc_local *lp = netdev_priv(dev);
	unsigned int ioaddr 	= dev->base_addr;
	word 			length;
	unsigned short 		numPages;
	word			time_out;

	netif_stop_queue(dev);

	if ( lp->saved_skb) {
		
		dev->stats.tx_aborted_errors++;
		printk(CARDNAME": Bad Craziness - sent packet while busy.\n" );
		return NETDEV_TX_BUSY;
	}
	lp->saved_skb = skb;

	length = skb->len;

	if (length < ETH_ZLEN) {
		if (skb_padto(skb, ETH_ZLEN)) {
			netif_wake_queue(dev);
			return NETDEV_TX_OK;
		}
		length = ETH_ZLEN;
	}

	numPages =  ((length & 0xfffe) + 6) / 256;

	if (numPages > 7 ) {
		printk(CARDNAME": Far too big packet error.\n");
		dev_kfree_skb (skb);
		lp->saved_skb = NULL;
		
		netif_wake_queue(dev);
		return NETDEV_TX_OK;
	}
	
	lp->packets_waiting++;

	
	SMC_SELECT_BANK( 2 );
	outw( MC_ALLOC | numPages, ioaddr + MMU_CMD );
	time_out = MEMORY_WAIT_TIME;
	do {
		word	status;

		status = inb( ioaddr + INTERRUPT );
		if ( status & IM_ALLOC_INT ) {
			
			outb( IM_ALLOC_INT, ioaddr + INTERRUPT );
  			break;
		}
   	} while ( -- time_out );

   	if ( !time_out ) {
		
		SMC_ENABLE_INT( IM_ALLOC_INT );
		PRINTK2((CARDNAME": memory allocation deferred.\n"));
		
		return NETDEV_TX_OK;
   	}
	
	smc_hardware_send_packet(dev);
	netif_wake_queue(dev);
	return NETDEV_TX_OK;
}

static void smc_hardware_send_packet( struct net_device * dev )
{
	struct smc_local *lp = netdev_priv(dev);
	byte	 		packet_no;
	struct sk_buff * 	skb = lp->saved_skb;
	word			length;
	unsigned int		ioaddr;
	byte			* buf;

	ioaddr = dev->base_addr;

	if ( !skb ) {
		PRINTK((CARDNAME": In XMIT with no packet to send\n"));
		return;
	}
	length = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;
	buf = skb->data;

	
	packet_no = inb( ioaddr + PNR_ARR + 1 );
	if ( packet_no & 0x80 ) {
		
		printk(KERN_DEBUG CARDNAME": Memory allocation failed.\n");
		dev_kfree_skb_any(skb);
		lp->saved_skb = NULL;
		netif_wake_queue(dev);
		return;
	}

	
	outb( packet_no, ioaddr + PNR_ARR );

	
	outw( PTR_AUTOINC , ioaddr + POINTER );

   	PRINTK3((CARDNAME": Trying to xmit packet of length %x\n", length ));
#if SMC_DEBUG > 2
	print_packet( buf, length );
#endif

#ifdef USE_32_BIT
	outl(  (length +6 ) << 16 , ioaddr + DATA_1 );
#else
	outw( 0, ioaddr + DATA_1 );
	
	outb( (length+6) & 0xFF,ioaddr + DATA_1 );
	outb( (length+6) >> 8 , ioaddr + DATA_1 );
#endif

#ifdef USE_32_BIT
	if ( length & 0x2  ) {
		outsl(ioaddr + DATA_1, buf,  length >> 2 );
#if !defined(__H8300H__) && !defined(__H8300S__)
		outw( *((word *)(buf + (length & 0xFFFFFFFC))),ioaddr +DATA_1);
#else
		ctrl_outw( *((word *)(buf + (length & 0xFFFFFFFC))),ioaddr +DATA_1);
#endif
	}
	else
		outsl(ioaddr + DATA_1, buf,  length >> 2 );
#else
	outsw(ioaddr + DATA_1 , buf, (length ) >> 1);
#endif
	

	if ( (length & 1) == 0 ) {
		outw( 0, ioaddr + DATA_1 );
	} else {
		outb( buf[length -1 ], ioaddr + DATA_1 );
		outb( 0x20, ioaddr + DATA_1);
	}

	
	SMC_ENABLE_INT( (IM_TX_INT | IM_TX_EMPTY_INT) );

	
	outw( MC_ENQUEUE , ioaddr + MMU_CMD );

	PRINTK2((CARDNAME": Sent packet of length %d\n", length));

	lp->saved_skb = NULL;
	dev_kfree_skb_any (skb);

	dev->trans_start = jiffies;

	
	netif_wake_queue(dev);
}

static int io;
static int irq;
static int ifport;

struct net_device * __init smc_init(int unit)
{
	struct net_device *dev = alloc_etherdev(sizeof(struct smc_local));
	struct devlist *smcdev = smc_devlist;
	int err = 0;

	if (!dev)
		return ERR_PTR(-ENODEV);

	if (unit >= 0) {
		sprintf(dev->name, "eth%d", unit);
		netdev_boot_setup_check(dev);
		io = dev->base_addr;
		irq = dev->irq;
	}

	if (io > 0x1ff) {	
		err = smc_probe(dev, io);
	} else if (io != 0) {	
		err = -ENXIO;
	} else {
		for (;smcdev->port; smcdev++) {
			if (smc_probe(dev, smcdev->port) == 0)
				break;
		}
		if (!smcdev->port)
			err = -ENODEV;
	}
	if (err)
		goto out;
	err = register_netdev(dev);
	if (err)
		goto out1;
	return dev;
out1:
	free_irq(dev->irq, dev);
	release_region(dev->base_addr, SMC_IO_EXTENT);
out:
	free_netdev(dev);
	return ERR_PTR(err);
}

static int __init smc_findirq(int ioaddr)
{
#ifndef NO_AUTOPROBE
	int	timeout = 20;
	unsigned long cookie;


	cookie = probe_irq_on();



	SMC_SELECT_BANK(2);
	
	outb( IM_ALLOC_INT, ioaddr + INT_MASK );

	outw( MC_ALLOC | 1, ioaddr + MMU_CMD );

	while ( timeout ) {
		byte	int_status;

		int_status = inb( ioaddr + INTERRUPT );

		if ( int_status & IM_ALLOC_INT )
			break;		
		timeout--;
	}

	SMC_DELAY();
	SMC_DELAY();

	
	outb( 0, ioaddr + INT_MASK );

	
	return probe_irq_off(cookie);
#else 
	struct devlist *smcdev;
	for (smcdev = smc_devlist; smcdev->port; smcdev++) {
		if (smcdev->port == ioaddr)
			return smcdev->irq;
	}
	return 0;
#endif
}

static const struct net_device_ops smc_netdev_ops = {
	.ndo_open		 = smc_open,
	.ndo_stop		= smc_close,
	.ndo_start_xmit    	= smc_wait_to_send_packet,
	.ndo_tx_timeout	    	= smc_timeout,
	.ndo_set_rx_mode	= smc_set_multicast_list,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};


static int __init smc_probe(struct net_device *dev, int ioaddr)
{
	int i, memory, retval;
	static unsigned version_printed;
	unsigned int bank;

	const char *version_string;
	const char *if_string;

	
	word revision_register;
	word base_address_register;
	word configuration_register;
	word memory_info_register;
	word memory_cfg_register;

	
	if (!request_region(ioaddr, SMC_IO_EXTENT, DRV_NAME))
		return -EBUSY;

	dev->irq = irq;
	dev->if_port = ifport;

	
	bank = inw( ioaddr + BANK_SELECT );
	if ( (bank & 0xFF00) != 0x3300 ) {
		retval = -ENODEV;
		goto err_out;
	}
	outw( 0x0, ioaddr + BANK_SELECT );
	bank = inw( ioaddr + BANK_SELECT );
	if ( (bank & 0xFF00 ) != 0x3300 ) {
		retval = -ENODEV;
		goto err_out;
	}
#if !defined(CONFIG_H8S_EDOSK2674)
	/* well, we've already written once, so hopefully another time won't
 	   hurt.  This time, I need to switch the bank register to bank 1,
	   so I can access the base address register */
	SMC_SELECT_BANK(1);
	base_address_register = inw( ioaddr + BASE );
	if ( ioaddr != ( base_address_register >> 3 & 0x3E0 ) )  {
		printk(CARDNAME ": IOADDR %x doesn't match configuration (%x). "
			"Probably not a SMC chip\n",
			ioaddr, base_address_register >> 3 & 0x3E0 );
		retval = -ENODEV;
		goto err_out;
	}
#else
	(void)base_address_register; 
#endif


	SMC_SELECT_BANK(3);
	revision_register  = inw( ioaddr + REVISION );
	if ( !chip_ids[ ( revision_register  >> 4 ) & 0xF  ] ) {
		
		printk(CARDNAME ": IO %x: Unrecognized revision register:"
			" %x, Contact author.\n", ioaddr, revision_register);

		retval = -ENODEV;
		goto err_out;
	}


	if (version_printed++ == 0)
		printk("%s", version);

	
	dev->base_addr = ioaddr;

	SMC_SELECT_BANK( 1 );
	for ( i = 0; i < 6; i += 2 ) {
		word	address;

		address = inw( ioaddr + ADDR0 + i  );
		dev->dev_addr[ i + 1] = address >> 8;
		dev->dev_addr[ i ] = address & 0xFF;
	}

	

	SMC_SELECT_BANK( 0 );
	memory_info_register = inw( ioaddr + MIR );
	memory_cfg_register  = inw( ioaddr + MCR );
	memory = ( memory_cfg_register >> 9 )  & 0x7;  
	memory *= 256 * ( memory_info_register & 0xFF );

	SMC_SELECT_BANK(3);
	revision_register  = inw( ioaddr + REVISION );
	version_string = chip_ids[ ( revision_register  >> 4 ) & 0xF  ];
	if ( !version_string ) {
		
		retval = -ENODEV;
		goto err_out;
	}

	
	if ( dev->if_port == 0 ) {
		SMC_SELECT_BANK(1);
		configuration_register = inw( ioaddr + CONFIG );
		if ( configuration_register & CFG_AUI_SELECT )
			dev->if_port = 2;
		else
			dev->if_port = 1;
	}
	if_string = interfaces[ dev->if_port - 1 ];

	
	smc_reset( ioaddr );

	if ( dev->irq < 2 ) {
		int	trials;

		trials = 3;
		while ( trials-- ) {
			dev->irq = smc_findirq( ioaddr );
			if ( dev->irq )
				break;
			
			smc_reset( ioaddr );
		}
	}
	if (dev->irq == 0 ) {
		printk(CARDNAME": Couldn't autodetect your IRQ. Use irq=xx.\n");
		retval = -ENODEV;
		goto err_out;
	}

	

	printk("%s: %s(r:%d) at %#3x IRQ:%d INTF:%s MEM:%db ", dev->name,
		version_string, revision_register & 0xF, ioaddr, dev->irq,
		if_string, memory );
	printk("ADDR: %pM\n", dev->dev_addr);

	
      	retval = request_irq(dev->irq, smc_interrupt, 0, DRV_NAME, dev);
      	if (retval) {
		printk("%s: unable to get IRQ %d (irqval=%d).\n", DRV_NAME,
			dev->irq, retval);
  	  	goto err_out;
      	}

	dev->netdev_ops			= &smc_netdev_ops;
	dev->watchdog_timeo		= HZ/20;

	return 0;

err_out:
	release_region(ioaddr, SMC_IO_EXTENT);
	return retval;
}

#if SMC_DEBUG > 2
static void print_packet( byte * buf, int length )
{
#if 0
	int i;
	int remainder;
	int lines;

	printk("Packet of length %d\n", length);
	lines = length / 16;
	remainder = length % 16;

	for ( i = 0; i < lines ; i ++ ) {
		int cur;

		for ( cur = 0; cur < 8; cur ++ ) {
			byte a, b;

			a = *(buf ++ );
			b = *(buf ++ );
			printk("%02x%02x ", a, b );
		}
		printk("\n");
	}
	for ( i = 0; i < remainder/2 ; i++ ) {
		byte a, b;

		a = *(buf ++ );
		b = *(buf ++ );
		printk("%02x%02x ", a, b );
	}
	printk("\n");
#endif
}
#endif


static int smc_open(struct net_device *dev)
{
	int	ioaddr = dev->base_addr;

	int	i;	

	
	memset(netdev_priv(dev), 0, sizeof(struct smc_local));

	

	smc_reset( ioaddr );
	smc_enable( ioaddr );

	

	SMC_SELECT_BANK( 1 );
	if ( dev->if_port == 1 ) {
		outw( inw( ioaddr + CONFIG ) & ~CFG_AUI_SELECT,
			ioaddr + CONFIG );
	}
	else if ( dev->if_port == 2 ) {
		outw( inw( ioaddr + CONFIG ) | CFG_AUI_SELECT,
			ioaddr + CONFIG );
	}

	SMC_SELECT_BANK( 1 );
	for ( i = 0; i < 6; i += 2 ) {
		word	address;

		address = dev->dev_addr[ i + 1 ] << 8 ;
		address  |= dev->dev_addr[ i ];
		outw( address, ioaddr + ADDR0 + i );
	}

	netif_start_queue(dev);
	return 0;
}


static void smc_timeout(struct net_device *dev)
{
	printk(KERN_WARNING CARDNAME": transmit timed out, %s?\n",
		tx_done(dev) ? "IRQ conflict" :
		"network cable problem");
	
	smc_reset( dev->base_addr );
	smc_enable( dev->base_addr );
	dev->trans_start = jiffies; 
	
	((struct smc_local *)netdev_priv(dev))->saved_skb = NULL;
	netif_wake_queue(dev);
}

static void smc_rcv(struct net_device *dev)
{
	int 	ioaddr = dev->base_addr;
	int 	packet_number;
	word	status;
	word	packet_length;

	

	packet_number = inw( ioaddr + FIFO_PORTS );

	if ( packet_number & FP_RXEMPTY ) {
		
		PRINTK((CARDNAME ": WARNING: smc_rcv with nothing on FIFO.\n"));
		
		return;
	}

	
	outw( PTR_READ | PTR_RCV | PTR_AUTOINC, ioaddr + POINTER );

	
	status 		= inw( ioaddr + DATA_1 );
	packet_length 	= inw( ioaddr + DATA_1 );

	packet_length &= 0x07ff;  

	PRINTK2(("RCV: STATUS %4x LENGTH %4x\n", status, packet_length ));
	packet_length -= 6;

	if ( !(status & RS_ERRORS ) ){
		
		struct sk_buff  * skb;
		byte		* data;

		
		if ( status & RS_ODDFRAME )
			packet_length++;

		
		if ( status & RS_MULTICAST )
			dev->stats.multicast++;

		skb = netdev_alloc_skb(dev, packet_length + 5);

		if ( skb == NULL ) {
			printk(KERN_NOTICE CARDNAME ": Low memory, packet dropped.\n");
			dev->stats.rx_dropped++;
			goto done;
		}


		skb_reserve( skb, 2 );   

		data = skb_put( skb, packet_length);

#ifdef USE_32_BIT
		PRINTK3((" Reading %d dwords (and %d bytes)\n",
			packet_length >> 2, packet_length & 3 ));
		insl(ioaddr + DATA_1 , data, packet_length >> 2 );
		
		insb( ioaddr + DATA_1, data + (packet_length & 0xFFFFFC),
			packet_length & 0x3  );
#else
		PRINTK3((" Reading %d words and %d byte(s)\n",
			(packet_length >> 1 ), packet_length & 1 ));
		insw(ioaddr + DATA_1 , data, packet_length >> 1);
		if ( packet_length & 1 ) {
			data += packet_length & ~1;
			*(data++) = inb( ioaddr + DATA_1 );
		}
#endif
#if	SMC_DEBUG > 2
			print_packet( data, packet_length );
#endif

		skb->protocol = eth_type_trans(skb, dev );
		netif_rx(skb);
		dev->stats.rx_packets++;
		dev->stats.rx_bytes += packet_length;
	} else {
		
		dev->stats.rx_errors++;

		if ( status & RS_ALGNERR )  dev->stats.rx_frame_errors++;
		if ( status & (RS_TOOSHORT | RS_TOOLONG ) )
			dev->stats.rx_length_errors++;
		if ( status & RS_BADCRC)	dev->stats.rx_crc_errors++;
	}

done:
	
	outw( MC_RELEASE, ioaddr + MMU_CMD );
}


static void smc_tx( struct net_device * dev )
{
	int	ioaddr = dev->base_addr;
	struct smc_local *lp = netdev_priv(dev);
	byte saved_packet;
	byte packet_no;
	word tx_status;


	

	saved_packet = inb( ioaddr + PNR_ARR );
	packet_no = inw( ioaddr + FIFO_PORTS );
	packet_no &= 0x7F;

	
	outb( packet_no, ioaddr + PNR_ARR );

	
	outw( PTR_AUTOINC | PTR_READ, ioaddr + POINTER );

	tx_status = inw( ioaddr + DATA_1 );
	PRINTK3((CARDNAME": TX DONE STATUS: %4x\n", tx_status));

	dev->stats.tx_errors++;
	if ( tx_status & TS_LOSTCAR ) dev->stats.tx_carrier_errors++;
	if ( tx_status & TS_LATCOL  ) {
		printk(KERN_DEBUG CARDNAME
			": Late collision occurred on last xmit.\n");
		dev->stats.tx_window_errors++;
	}
#if 0
		if ( tx_status & TS_16COL ) { ... }
#endif

	if ( tx_status & TS_SUCCESS ) {
		printk(CARDNAME": Successful packet caused interrupt\n");
	}
	
	SMC_SELECT_BANK( 0 );
	outw( inw( ioaddr + TCR ) | TCR_ENABLE, ioaddr + TCR );

	
	SMC_SELECT_BANK( 2 );
	outw( MC_FREEPKT, ioaddr + MMU_CMD );

	
	lp->packets_waiting--;

	outb( saved_packet, ioaddr + PNR_ARR );
}


static irqreturn_t smc_interrupt(int irq, void * dev_id)
{
	struct net_device *dev 	= dev_id;
	int ioaddr 		= dev->base_addr;
	struct smc_local *lp = netdev_priv(dev);

	byte	status;
	word	card_stats;
	byte	mask;
	int	timeout;
	
	word	saved_bank;
	word	saved_pointer;
	int handled = 0;


	PRINTK3((CARDNAME": SMC interrupt started\n"));

	saved_bank = inw( ioaddr + BANK_SELECT );

	SMC_SELECT_BANK(2);
	saved_pointer = inw( ioaddr + POINTER );

	mask = inb( ioaddr + INT_MASK );
	
	outb( 0, ioaddr + INT_MASK );


	
	timeout = 4;

	PRINTK2((KERN_WARNING CARDNAME ": MASK IS %x\n", mask));
	do {
		
		status = inb( ioaddr + INTERRUPT ) & mask;
		if (!status )
			break;

		handled = 1;

		PRINTK3((KERN_WARNING CARDNAME
			": Handling interrupt status %x\n", status));

		if (status & IM_RCV_INT) {
			
			PRINTK2((KERN_WARNING CARDNAME
				": Receive Interrupt\n"));
			smc_rcv(dev);
		} else if (status & IM_TX_INT ) {
			PRINTK2((KERN_WARNING CARDNAME
				": TX ERROR handled\n"));
			smc_tx(dev);
			outb(IM_TX_INT, ioaddr + INTERRUPT );
		} else if (status & IM_TX_EMPTY_INT ) {
			
			SMC_SELECT_BANK( 0 );
			card_stats = inw( ioaddr + COUNTER );
			
			dev->stats.collisions += card_stats & 0xF;
			card_stats >>= 4;
			
			dev->stats.collisions += card_stats & 0xF;

			

			SMC_SELECT_BANK( 2 );
			PRINTK2((KERN_WARNING CARDNAME
				": TX_BUFFER_EMPTY handled\n"));
			outb( IM_TX_EMPTY_INT, ioaddr + INTERRUPT );
			mask &= ~IM_TX_EMPTY_INT;
			dev->stats.tx_packets += lp->packets_waiting;
			lp->packets_waiting = 0;

		} else if (status & IM_ALLOC_INT ) {
			PRINTK2((KERN_DEBUG CARDNAME
				": Allocation interrupt\n"));
			
			mask &= ~IM_ALLOC_INT;

			smc_hardware_send_packet( dev );

			
			mask |= ( IM_TX_EMPTY_INT | IM_TX_INT );

			
			netif_wake_queue(dev);

			PRINTK2((CARDNAME": Handoff done successfully.\n"));
		} else if (status & IM_RX_OVRN_INT ) {
			dev->stats.rx_errors++;
			dev->stats.rx_fifo_errors++;
			outb( IM_RX_OVRN_INT, ioaddr + INTERRUPT );
		} else if (status & IM_EPH_INT ) {
			PRINTK((CARDNAME ": UNSUPPORTED: EPH INTERRUPT\n"));
		} else if (status & IM_ERCV_INT ) {
			PRINTK((CARDNAME ": UNSUPPORTED: ERCV INTERRUPT\n"));
			outb( IM_ERCV_INT, ioaddr + INTERRUPT );
		}
	} while ( timeout -- );


	
	SMC_SELECT_BANK( 2 );
	outb( mask, ioaddr + INT_MASK );

	PRINTK3((KERN_WARNING CARDNAME ": MASK is now %x\n", mask));
	outw( saved_pointer, ioaddr + POINTER );

	SMC_SELECT_BANK( saved_bank );

	PRINTK3((CARDNAME ": Interrupt done\n"));
	return IRQ_RETVAL(handled);
}


static int smc_close(struct net_device *dev)
{
	netif_stop_queue(dev);
	
	smc_shutdown( dev->base_addr );

	
	return 0;
}

static void smc_set_multicast_list(struct net_device *dev)
{
	short ioaddr = dev->base_addr;

	SMC_SELECT_BANK(0);
	if ( dev->flags & IFF_PROMISC )
		outw( inw(ioaddr + RCR ) | RCR_PROMISC, ioaddr + RCR );


	else if (dev->flags & IFF_ALLMULTI)
		outw( inw(ioaddr + RCR ) | RCR_ALMUL, ioaddr + RCR );

	else if (!netdev_mc_empty(dev)) {
		

		
		outw( inw( ioaddr + RCR ) & ~(RCR_PROMISC | RCR_ALMUL),
			ioaddr + RCR );
		smc_setmulticast(ioaddr, dev);
	}
	else  {
		outw( inw( ioaddr + RCR ) & ~(RCR_PROMISC | RCR_ALMUL),
			ioaddr + RCR );

		SMC_SELECT_BANK( 3 );
		outw( 0, ioaddr + MULTICAST1 );
		outw( 0, ioaddr + MULTICAST2 );
		outw( 0, ioaddr + MULTICAST3 );
		outw( 0, ioaddr + MULTICAST4 );
	}
}

#ifdef MODULE

static struct net_device *devSMC9194;
MODULE_LICENSE("GPL");

module_param(io, int, 0);
module_param(irq, int, 0);
module_param(ifport, int, 0);
MODULE_PARM_DESC(io, "SMC 99194 I/O base address");
MODULE_PARM_DESC(irq, "SMC 99194 IRQ number");
MODULE_PARM_DESC(ifport, "SMC 99194 interface port (0-default, 1-TP, 2-AUI)");

int __init init_module(void)
{
	if (io == 0)
		printk(KERN_WARNING
		CARDNAME": You shouldn't use auto-probing with insmod!\n" );

	
	devSMC9194 = smc_init(-1);
	if (IS_ERR(devSMC9194))
		return PTR_ERR(devSMC9194);
	return 0;
}

void __exit cleanup_module(void)
{
	unregister_netdev(devSMC9194);
	free_irq(devSMC9194->irq, devSMC9194);
	release_region(devSMC9194->base_addr, SMC_IO_EXTENT);
	free_netdev(devSMC9194);
}

#endif 
