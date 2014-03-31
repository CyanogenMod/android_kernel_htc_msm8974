/*      cops.c: LocalTalk driver for Linux.
 *
 *	Authors:
 *      - Jay Schulist <jschlst@samba.org>
 *
 *	With more than a little help from;
 *	- Alan Cox <alan@lxorguk.ukuu.org.uk>
 *
 *      Derived from:
 *      - skeleton.c: A network driver outline for linux.
 *        Written 1993-94 by Donald Becker.
 *	- ltpc.c: A driver for the LocalTalk PC card.
 *	  Written by Bradford W. Johnson.
 *
 *      Copyright 1993 United States Government as represented by the
 *      Director, National Security Agency.
 *
 *      This software may be used and distributed according to the terms
 *      of the GNU General Public License, incorporated herein by reference.
 *
 *	Changes:
 *	19970608	Alan Cox	Allowed dual card type support
 *					Can set board type in insmod
 *					Hooks for cops_setup routine
 *					(not yet implemented).
 *	19971101	Jay Schulist	Fixes for multiple lt* devices.
 *	19980607	Steven Hirsch	Fixed the badly broken support
 *					for Tangent type cards. Only
 *                                      tested on Daystar LT200. Some
 *                                      cleanup of formatting and program
 *                                      logic.  Added emacs 'local-vars'
 *                                      setup for Jay's brace style.
 *	20000211	Alan Cox	Cleaned up for softnet
 */

static const char *version =
"cops.c:v0.04 6/7/98 Jay Schulist <jschlst@samba.org>\n";


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/if_ltalk.h>
#include <linux/delay.h>	
#include <linux/atalk.h>
#include <linux/spinlock.h>
#include <linux/bitops.h>
#include <linux/jiffies.h>

#include <asm/io.h>
#include <asm/dma.h>

#include "cops.h"		
#include "cops_ltdrv.h"		
#include "cops_ffdrv.h"		


static const char *cardname = "cops";

#ifdef CONFIG_COPS_DAYNA
static int board_type = DAYNA;	
#else
static int board_type = TANGENT;
#endif

static int io = 0x240;		
static int irq = 5;		

/*
 *	COPS Autoprobe information.
 *	Right now if port address is right but IRQ is not 5 this will
 *      return a 5 no matter what since we will still get a status response.
 *      Need one more additional check to narrow down after we have gotten
 *      the ioaddr. But since only other possible IRQs is 3 and 4 so no real
 *	hurry on this. I *STRONGLY* recommend using IRQ 5 for your card with
 *	this driver.
 * 
 *	This driver has 2 modes and they are: Dayna mode and Tangent mode.
 *	Each mode corresponds with the type of card. It has been found
 *	that there are 2 main types of cards and all other cards are
 *	the same and just have different names or only have minor differences
 *	such as more IO ports. As this driver is tested it will
 *	become more clear on exactly what cards are supported. The driver
 *	defaults to using Dayna mode. To change the drivers mode, simply
 *	select Dayna or Tangent mode when configuring the kernel.
 *
 *      This driver should support:
 *      TANGENT driver mode:
 *              Tangent ATB-II, Novell NL-1000, Daystar Digital LT-200,
 *		COPS LT-1
 *      DAYNA driver mode:
 *              Dayna DL2000/DaynaTalk PC (Half Length), COPS LT-95, 
 *		Farallon PhoneNET PC III, Farallon PhoneNET PC II
 *	Other cards possibly supported mode unknown though:
 *		Dayna DL2000 (Full length), COPS LT/M (Micro-Channel)
 *
 *	Cards NOT supported by this driver but supported by the ltpc.c
 *	driver written by Bradford W. Johnson <johns393@maroon.tc.umn.edu>
 *		Farallon PhoneNET PC
 *		Original Apple LocalTalk PC card
 * 
 *      N.B.
 *
 *      The Daystar Digital LT200 boards do not support interrupt-driven
 *      IO.  You must specify 'irq=0xff' as a module parameter to invoke
 *      polled mode.  I also believe that the port probing logic is quite
 *      dangerous at best and certainly hopeless for a polled card.  Best to 
 *      specify both. - Steve H.
 *
 */


static unsigned int ports[] = { 
	0x240, 0x340, 0x200, 0x210, 0x220, 0x230, 0x260, 
	0x2A0, 0x300, 0x310, 0x320, 0x330, 0x350, 0x360,
	0
};


static int cops_irqlist[] = {
	5, 4, 3, 0 
};

static struct timer_list cops_timer;

#ifndef COPS_DEBUG
#define COPS_DEBUG 1 
#endif
static unsigned int cops_debug = COPS_DEBUG;

#define COPS_IO_EXTENT       8


struct cops_local
{
        int board;			
	int nodeid;			
        unsigned char node_acquire;	
        struct atalk_addr node_addr;	
	spinlock_t lock;		
};

static int  cops_probe1 (struct net_device *dev, int ioaddr);
static int  cops_irq (int ioaddr, int board);

static int  cops_open (struct net_device *dev);
static int  cops_jumpstart (struct net_device *dev);
static void cops_reset (struct net_device *dev, int sleep);
static void cops_load (struct net_device *dev);
static int  cops_nodeid (struct net_device *dev, int nodeid);

static irqreturn_t cops_interrupt (int irq, void *dev_id);
static void cops_poll (unsigned long ltdev);
static void cops_timeout(struct net_device *dev);
static void cops_rx (struct net_device *dev);
static netdev_tx_t  cops_send_packet (struct sk_buff *skb,
					    struct net_device *dev);
static void set_multicast_list (struct net_device *dev);
static int  cops_ioctl (struct net_device *dev, struct ifreq *rq, int cmd);
static int  cops_close (struct net_device *dev);

static void cleanup_card(struct net_device *dev)
{
	if (dev->irq)
		free_irq(dev->irq, dev);
	release_region(dev->base_addr, COPS_IO_EXTENT);
}

struct net_device * __init cops_probe(int unit)
{
	struct net_device *dev;
	unsigned *port;
	int base_addr;
	int err = 0;

	dev = alloc_ltalkdev(sizeof(struct cops_local));
	if (!dev)
		return ERR_PTR(-ENOMEM);

	if (unit >= 0) {
		sprintf(dev->name, "lt%d", unit);
		netdev_boot_setup_check(dev);
		irq = dev->irq;
		base_addr = dev->base_addr;
	} else {
		base_addr = dev->base_addr = io;
	}

	if (base_addr > 0x1ff) {    
		err = cops_probe1(dev, base_addr);
	} else if (base_addr != 0) { 
		err = -ENXIO;
	} else {
		for (port = ports; *port && cops_probe1(dev, *port) < 0; port++)
			;
		if (!*port)
			err = -ENODEV;
	}
	if (err)
		goto out;
	err = register_netdev(dev);
	if (err)
		goto out1;
	return dev;
out1:
	cleanup_card(dev);
out:
	free_netdev(dev);
	return ERR_PTR(err);
}

static const struct net_device_ops cops_netdev_ops = {
	.ndo_open               = cops_open,
        .ndo_stop               = cops_close,
	.ndo_start_xmit   	= cops_send_packet,
	.ndo_tx_timeout		= cops_timeout,
        .ndo_do_ioctl           = cops_ioctl,
	.ndo_set_rx_mode	= set_multicast_list,
};

static int __init cops_probe1(struct net_device *dev, int ioaddr)
{
        struct cops_local *lp;
	static unsigned version_printed;
	int board = board_type;
	int retval;
	
        if(cops_debug && version_printed++ == 0)
		printk("%s", version);

	
	if (!request_region(ioaddr, COPS_IO_EXTENT, dev->name))
		return -EBUSY;

	dev->irq = irq;
	switch (dev->irq)
	{
		case 0:
			
			dev->irq = cops_irq(ioaddr, board);
			if (dev->irq)
				break;
			
		case 1:
			retval = -EINVAL;
			goto err_out;

		case 2:
			dev->irq = 9;
			break;

		case 0xff:
			dev->irq = 0;
			break;

		default:
			break;
	}

	
	if (dev->irq) {
		retval = request_irq(dev->irq, cops_interrupt, 0, dev->name, dev);
		if (retval)
			goto err_out;
	}

	dev->base_addr = ioaddr;

        lp = netdev_priv(dev);
        spin_lock_init(&lp->lock);

	
	lp->board               = board;

	dev->netdev_ops 	= &cops_netdev_ops;
	dev->watchdog_timeo	= HZ * 2;


	
	if(board==DAYNA)
		printk("%s: %s at %#3x, using IRQ %d, in Dayna mode.\n", 
			dev->name, cardname, ioaddr, dev->irq);
	if(board==TANGENT) {
		if(dev->irq)
			printk("%s: %s at %#3x, IRQ %d, in Tangent mode\n", 
				dev->name, cardname, ioaddr, dev->irq);
		else
			printk("%s: %s at %#3x, using polled IO, in Tangent mode.\n", 
				dev->name, cardname, ioaddr);

	}
        return 0;

err_out:
	release_region(ioaddr, COPS_IO_EXTENT);
	return retval;
}

static int __init cops_irq (int ioaddr, int board)
{       
        int irqaddr=0;
        int i, x, status;

        if(board==DAYNA)
        {
                outb(0, ioaddr+DAYNA_RESET);
                inb(ioaddr+DAYNA_RESET);
                mdelay(333);
        }
        if(board==TANGENT)
        {
                inb(ioaddr);
                outb(0, ioaddr);
                outb(0, ioaddr+TANG_RESET);
        }

        for(i=0; cops_irqlist[i] !=0; i++)
        {
                irqaddr = cops_irqlist[i];
                for(x = 0xFFFF; x>0; x --)    
                {
                        if(board==DAYNA)
                        {
                                status = (inb(ioaddr+DAYNA_CARD_STATUS)&3);
                                if(status == 1)
                                        return irqaddr;
                        }
                        if(board==TANGENT)
                        {
                                if((inb(ioaddr+TANG_CARD_STATUS)& TANG_TX_READY) !=0)
                                        return irqaddr;
                        }
                }
        }
        return 0;       
}

static int cops_open(struct net_device *dev)
{
    struct cops_local *lp = netdev_priv(dev);

	if(dev->irq==0)
	{
		if(lp->board==TANGENT)	
		{
		    init_timer(&cops_timer);
		    cops_timer.function = cops_poll;
		    cops_timer.data 	= (unsigned long)dev;
		    cops_timer.expires 	= jiffies + HZ/20;
		    add_timer(&cops_timer);
		} 
		else 
		{
			printk(KERN_WARNING "%s: No irq line set\n", dev->name);
			return -EAGAIN;
		}
	}

	cops_jumpstart(dev);	

	netif_start_queue(dev);
        return 0;
}

static int cops_jumpstart(struct net_device *dev)
{
	struct cops_local *lp = netdev_priv(dev);

        cops_reset(dev,1);	
        cops_load(dev);		

	
	if(lp->nodeid == 1)
		cops_nodeid(dev,lp->node_acquire);

	return 0;
}

static void tangent_wait_reset(int ioaddr)
{
	int timeout=0;

	while(timeout++ < 5 && (inb(ioaddr+TANG_CARD_STATUS)&TANG_TX_READY)==0)
		mdelay(1);   
}

static void cops_reset(struct net_device *dev, int sleep)
{
        struct cops_local *lp = netdev_priv(dev);
        int ioaddr=dev->base_addr;

        if(lp->board==TANGENT)
        {
                inb(ioaddr);		
                outb(0,ioaddr);		
                outb(0, ioaddr+TANG_RESET);	

		tangent_wait_reset(ioaddr);
                outb(0, ioaddr+TANG_CLEAR_INT);
        }
        if(lp->board==DAYNA)
        {
                outb(0, ioaddr+DAYNA_RESET);	
                inb(ioaddr+DAYNA_RESET);	
		if (sleep)
			msleep(333);
		else
			mdelay(333);
        }

	netif_wake_queue(dev);
}

static void cops_load (struct net_device *dev)
{
        struct ifreq ifr;
        struct ltfirmware *ltf= (struct ltfirmware *)&ifr.ifr_ifru;
        struct cops_local *lp = netdev_priv(dev);
        int ioaddr=dev->base_addr;
	int length, i = 0;

        strcpy(ifr.ifr_name,"lt0");

        
#ifdef CONFIG_COPS_DAYNA        
        if(lp->board==DAYNA)
        {
                ltf->length=sizeof(ffdrv_code);
                ltf->data=ffdrv_code;
        }
        else
#endif        
#ifdef CONFIG_COPS_TANGENT
        if(lp->board==TANGENT)
        {
                ltf->length=sizeof(ltdrv_code);
                ltf->data=ltdrv_code;
        }
        else
#endif
	{
		printk(KERN_INFO "%s; unsupported board type.\n", dev->name);
		return;
	}
	
        
        if(lp->board==DAYNA && ltf->length!=5983)
        {
                printk(KERN_WARNING "%s: Firmware is not length of FFDRV.BIN.\n", dev->name);
                return;
        }
        if(lp->board==TANGENT && ltf->length!=2501)
        {
                printk(KERN_WARNING "%s: Firmware is not length of DRVCODE.BIN.\n", dev->name);
                return;
        }

        if(lp->board==DAYNA)
        {
                while(++i<65536)
                {
                       if((inb(ioaddr+DAYNA_CARD_STATUS)&3)==1)
                                break;
                }

                if(i==65536)
                        return;
        }

	i=0;
        length = ltf->length;
        while(length--)
        {
                outb(ltf->data[i], ioaddr);
                i++;
        }

	if(cops_debug > 1)
		printk("%s: Uploaded firmware - %d bytes of %d bytes.\n", 
			dev->name, i, ltf->length);

        if(lp->board==DAYNA) 	
                outb(1, ioaddr+DAYNA_INT_CARD);
	else			
		inb(ioaddr);

        if(lp->board==TANGENT)
        {
                tangent_wait_reset(ioaddr);
                inb(ioaddr);	
        }
}

static int cops_nodeid (struct net_device *dev, int nodeid)
{
	struct cops_local *lp = netdev_priv(dev);
	int ioaddr = dev->base_addr;

	if(lp->board == DAYNA)
        {
        	
                while((inb(ioaddr+DAYNA_CARD_STATUS)&DAYNA_TX_READY)==0)
                {
			outb(0, ioaddr+COPS_CLEAR_INT);	
        		if((inb(ioaddr+DAYNA_CARD_STATUS)&0x03)==DAYNA_RX_REQUEST)
                		cops_rx(dev);	
			schedule();
                }

                outb(2, ioaddr);       	
                outb(0, ioaddr);
                outb(LAP_INIT, ioaddr);	
                outb(nodeid, ioaddr);  	
        }

	if(lp->board == TANGENT)
        {
                
                while(inb(ioaddr+TANG_CARD_STATUS)&TANG_RX_READY)
                {
			outb(0, ioaddr+COPS_CLEAR_INT);	
                	cops_rx(dev);          	
			schedule();
                }

		
                if(nodeid == 0)	         		
                	nodeid = jiffies&0xFF;		
                outb(2, ioaddr);        		
                outb(0, ioaddr);       			
                outb(LAP_INIT, ioaddr); 		
                outb(nodeid, ioaddr); 		  	
                outb(0xFF, ioaddr);     		
        }

	lp->node_acquire=0;		
        while(lp->node_acquire==0)	
	{
		outb(0, ioaddr+COPS_CLEAR_INT);	

		if(lp->board == DAYNA)
		{
                	if((inb(ioaddr+DAYNA_CARD_STATUS)&0x03)==DAYNA_RX_REQUEST)
                		cops_rx(dev);	
		}
		if(lp->board == TANGENT)
		{	
			if(inb(ioaddr+TANG_CARD_STATUS)&TANG_RX_READY)
                                cops_rx(dev);   
		}
		schedule();
	}

	if(cops_debug > 1)
		printk(KERN_DEBUG "%s: Node ID %d has been acquired.\n", 
			dev->name, lp->node_acquire);

	lp->nodeid=1;	

        return 0;
}

 
static void cops_poll(unsigned long ltdev)
{
	int ioaddr, status;
	int boguscount = 0;

	struct net_device *dev = (struct net_device *)ltdev;

	del_timer(&cops_timer);

	if(dev == NULL)
		return;	

	ioaddr = dev->base_addr;
	do {
		status=inb(ioaddr+TANG_CARD_STATUS);
		if(status & TANG_RX_READY)
			cops_rx(dev);
		if(status & TANG_TX_READY)
			netif_wake_queue(dev);
		status = inb(ioaddr+TANG_CARD_STATUS);
	} while((++boguscount < 20) && (status&(TANG_RX_READY|TANG_TX_READY)));

	
	cops_timer.expires = jiffies + HZ/20;
	add_timer(&cops_timer);
}

static irqreturn_t cops_interrupt(int irq, void *dev_id)
{
        struct net_device *dev = dev_id;
        struct cops_local *lp;
        int ioaddr, status;
        int boguscount = 0;

        ioaddr = dev->base_addr;
        lp = netdev_priv(dev);

	if(lp->board==DAYNA)
	{
		do {
			outb(0, ioaddr + COPS_CLEAR_INT);
                       	status=inb(ioaddr+DAYNA_CARD_STATUS);
                       	if((status&0x03)==DAYNA_RX_REQUEST)
                       	        cops_rx(dev);
                	netif_wake_queue(dev);
		} while(++boguscount < 20);
	}
	else
	{
		do {
                       	status=inb(ioaddr+TANG_CARD_STATUS);
			if(status & TANG_RX_READY)
				cops_rx(dev);
			if(status & TANG_TX_READY)
				netif_wake_queue(dev);
			status=inb(ioaddr+TANG_CARD_STATUS);
		} while((++boguscount < 20) && (status&(TANG_RX_READY|TANG_TX_READY)));
	}

        return IRQ_HANDLED;
}

static void cops_rx(struct net_device *dev)
{
        int pkt_len = 0;
        int rsp_type = 0;
        struct sk_buff *skb = NULL;
        struct cops_local *lp = netdev_priv(dev);
        int ioaddr = dev->base_addr;
        int boguscount = 0;
        unsigned long flags;


	spin_lock_irqsave(&lp->lock, flags);
	
        if(lp->board==DAYNA)
        {
                outb(0, ioaddr);                
                outb(0, ioaddr);
                outb(DATA_READ, ioaddr);        

                
                while(++boguscount<1000000)
                {
			barrier();
                        if((inb(ioaddr+DAYNA_CARD_STATUS)&0x03)==DAYNA_RX_READY)
                                break;
                }

                if(boguscount==1000000)
                {
                        printk(KERN_WARNING "%s: DMA timed out.\n",dev->name);
			spin_unlock_irqrestore(&lp->lock, flags);
                        return;
                }
        }

        
	if(lp->board==DAYNA)
        	pkt_len = inb(ioaddr) & 0xFF;
	else
		pkt_len = inb(ioaddr) & 0x00FF;
        pkt_len |= (inb(ioaddr) << 8);
        
        rsp_type=inb(ioaddr);

        
        skb = dev_alloc_skb(pkt_len);
        if(skb == NULL)
        {
                printk(KERN_WARNING "%s: Memory squeeze, dropping packet.\n",
			dev->name);
                dev->stats.rx_dropped++;
                while(pkt_len--)        
                        inb(ioaddr);
                spin_unlock_irqrestore(&lp->lock, flags);
                return;
        }
        skb->dev = dev;
        skb_put(skb, pkt_len);
        skb->protocol = htons(ETH_P_LOCALTALK);

        insb(ioaddr, skb->data, pkt_len);               

        if(lp->board==DAYNA)
                outb(1, ioaddr+DAYNA_INT_CARD);         

        spin_unlock_irqrestore(&lp->lock, flags);  

        
        if(pkt_len < 0 || pkt_len > MAX_LLAP_SIZE)
        {
		printk(KERN_WARNING "%s: Bad packet length of %d bytes.\n", 
			dev->name, pkt_len);
                dev->stats.tx_errors++;
                dev_kfree_skb_any(skb);
                return;
        }

        
        if(rsp_type == LAP_INIT_RSP)
        {	
                lp->node_acquire = skb->data[0];
                dev_kfree_skb_any(skb);
                return;
        }

        
        if(rsp_type != LAP_RESPONSE)
        {
                printk(KERN_WARNING "%s: Bad packet type %d.\n", dev->name, rsp_type);
                dev->stats.tx_errors++;
                dev_kfree_skb_any(skb);
                return;
        }

        skb_reset_mac_header(skb);    
        skb_pull(skb,3);
        skb_reset_transport_header(skb);    

        
        dev->stats.rx_packets++;
        dev->stats.rx_bytes += skb->len;

        
        netif_rx(skb);
}

static void cops_timeout(struct net_device *dev)
{
        struct cops_local *lp = netdev_priv(dev);
        int ioaddr = dev->base_addr;

	dev->stats.tx_errors++;
        if(lp->board==TANGENT)
        {
		if((inb(ioaddr+TANG_CARD_STATUS)&TANG_TX_READY)==0)
               		printk(KERN_WARNING "%s: No TX complete interrupt.\n", dev->name);
	}
	printk(KERN_WARNING "%s: Transmit timed out.\n", dev->name);
	cops_jumpstart(dev);	
	dev->trans_start = jiffies; 
	netif_wake_queue(dev);
}



static netdev_tx_t cops_send_packet(struct sk_buff *skb,
					  struct net_device *dev)
{
        struct cops_local *lp = netdev_priv(dev);
        int ioaddr = dev->base_addr;
        unsigned long flags;

	 
	netif_stop_queue(dev);

	spin_lock_irqsave(&lp->lock, flags);
	if(lp->board == DAYNA)	 
		while((inb(ioaddr+DAYNA_CARD_STATUS)&DAYNA_TX_READY)==0)
			cpu_relax();
	if(lp->board == TANGENT) 
		while((inb(ioaddr+TANG_CARD_STATUS)&TANG_TX_READY)==0)
			cpu_relax();

	
	outb(skb->len, ioaddr);
	if(lp->board == DAYNA)
               	outb(skb->len >> 8, ioaddr);
	else
		outb((skb->len >> 8)&0x0FF, ioaddr);

	
	outb(LAP_WRITE, ioaddr);

	if(lp->board == DAYNA)	
        	while((inb(ioaddr+DAYNA_CARD_STATUS)&DAYNA_TX_READY)==0);

	outsb(ioaddr, skb->data, skb->len);	

	if(lp->board==DAYNA)	
		outb(1, ioaddr+DAYNA_INT_CARD);

	spin_unlock_irqrestore(&lp->lock, flags);	

	
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;
	dev_kfree_skb (skb);
	return NETDEV_TX_OK;
}

 
static void set_multicast_list(struct net_device *dev)
{
        if(cops_debug >= 3)
		printk("%s: set_multicast_list executed\n", dev->name);
}

 
static int cops_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
        struct cops_local *lp = netdev_priv(dev);
        struct sockaddr_at *sa = (struct sockaddr_at *)&ifr->ifr_addr;
        struct atalk_addr *aa = (struct atalk_addr *)&lp->node_addr;

        switch(cmd)
        {
                case SIOCSIFADDR:
			
			cops_nodeid(dev, sa->sat_addr.s_node);
			aa->s_net               = sa->sat_addr.s_net;
                        aa->s_node              = lp->node_acquire;

			
                        dev->broadcast[0]       = 0xFF;
			
			
                        dev->dev_addr[0]        = aa->s_node;
                        dev->addr_len           = 1;
                        return 0;

                case SIOCGIFADDR:
                        sa->sat_addr.s_net      = aa->s_net;
                        sa->sat_addr.s_node     = aa->s_node;
                        return 0;

                default:
                        return -EOPNOTSUPP;
        }
}

 
static int cops_close(struct net_device *dev)
{
	struct cops_local *lp = netdev_priv(dev);

	if(lp->board==TANGENT && dev->irq==0)
		del_timer(&cops_timer);

	netif_stop_queue(dev);
        return 0;
}


#ifdef MODULE
static struct net_device *cops_dev;

MODULE_LICENSE("GPL");
module_param(io, int, 0);
module_param(irq, int, 0);
module_param(board_type, int, 0);

static int __init cops_module_init(void)
{
	if (io == 0)
		printk(KERN_WARNING "%s: You shouldn't autoprobe with insmod\n",
			cardname);
	cops_dev = cops_probe(-1);
	if (IS_ERR(cops_dev))
		return PTR_ERR(cops_dev);
        return 0;
}

static void __exit cops_module_exit(void)
{
	unregister_netdev(cops_dev);
	cleanup_card(cops_dev);
	free_netdev(cops_dev);
}
module_init(cops_module_init);
module_exit(cops_module_exit);
#endif 
