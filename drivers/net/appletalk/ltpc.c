/***    ltpc.c -- a driver for the LocalTalk PC card.
 *
 *      Copyright (c) 1995,1996 Bradford W. Johnson <johns393@maroon.tc.umn.edu>
 *
 *      This software may be used and distributed according to the terms
 *      of the GNU General Public License, incorporated herein by reference.
 *
 *      This is ALPHA code at best.  It may not work for you.  It may
 *      damage your equipment.  It may damage your relations with other
 *      users of your network.  Use it at your own risk!
 *
 *      Based in part on:
 *      skeleton.c      by Donald Becker
 *      dummy.c         by Nick Holloway and Alan Cox
 *      loopback.c      by Ross Biro, Fred van Kampen, Donald Becker
 *      the netatalk source code (UMICH)
 *      lots of work on the card...
 *
 *      I do not have access to the (proprietary) SDK that goes with the card.
 *      If you do, I don't want to know about it, and you can probably write
 *      a better driver yourself anyway.  This does mean that the pieces that
 *      talk to the card are guesswork on my part, so use at your own risk!
 *
 *      This is my first try at writing Linux networking code, and is also
 *      guesswork.  Again, use at your own risk!  (Although on this part, I'd
 *      welcome suggestions)
 *
 *      This is a loadable kernel module which seems to work at my site
 *      consisting of a 1.2.13 linux box running netatalk 1.3.3, and with
 *      the kernel support from 1.3.3b2 including patches routing.patch
 *      and ddp.disappears.from.chooser.  In order to run it, you will need
 *      to patch ddp.c and aarp.c in the kernel, but only a little...
 *
 *      I'm fairly confident that while this is arguably badly written, the
 *      problems that people experience will be "higher level", that is, with
 *      complications in the netatalk code.  The driver itself doesn't do
 *      anything terribly complicated -- it pretends to be an ether device
 *      as far as netatalk is concerned, strips the DDP data out of the ether
 *      frame and builds a LLAP packet to send out the card.  In the other
 *      direction, it receives LLAP frames from the card and builds a fake
 *      ether packet that it then tosses up to the networking code.  You can
 *      argue (correctly) that this is an ugly way to do things, but it
 *      requires a minimal amount of fooling with the code in ddp.c and aarp.c.
 *
 *      The card will do a lot more than is used here -- I *think* it has the
 *      layers up through ATP.  Even if you knew how that part works (which I
 *      don't) it would be a big job to carve up the kernel ddp code to insert
 *      things at a higher level, and probably a bad idea...
 *
 *      There are a number of other cards that do LocalTalk on the PC.  If
 *      nobody finds any insurmountable (at the netatalk level) problems
 *      here, this driver should encourage people to put some work into the
 *      other cards (some of which I gather are still commercially available)
 *      and also to put hooks for LocalTalk into the official ddp code.
 *
 *      I welcome comments and suggestions.  This is my first try at Linux
 *      networking stuff, and there are probably lots of things that I did
 *      suboptimally.  
 *
 ***/




static int debug;
#define DEBUG_VERBOSE 1
#define DEBUG_UPPER 2
#define DEBUG_LOWER 4

static int io;
static int irq;
static int dma;

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/spinlock.h>
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
#include <linux/timer.h>
#include <linux/atalk.h>
#include <linux/bitops.h>
#include <linux/gfp.h>

#include <asm/dma.h>
#include <asm/io.h>

#include "ltpc.h"

static DEFINE_SPINLOCK(txqueue_lock);
static DEFINE_SPINLOCK(mbox_lock);

static int do_read(struct net_device *dev, void *cbuf, int cbuflen,
	void *dbuf, int dbuflen);
static int sendup_buffer (struct net_device *dev);


static unsigned long dma_mem_alloc(int size)
{
        int order = get_order(size);

        return __get_dma_pages(GFP_KERNEL, order);
}

static unsigned char *ltdmabuf;
static unsigned char *ltdmacbuf;


struct ltpc_private
{
	struct atalk_addr my_addr;
};


struct xmitQel {
	struct xmitQel *next;
	
	unsigned char *cbuf;
	short cbuflen;
	
	unsigned char *dbuf;
	short dbuflen;
	unsigned char QWrite;	
	unsigned char mailbox;
};


static struct xmitQel *xmQhd, *xmQtl;

static void enQ(struct xmitQel *qel)
{
	unsigned long flags;
	qel->next = NULL;
	
	spin_lock_irqsave(&txqueue_lock, flags);
	if (xmQtl) {
		xmQtl->next = qel;
	} else {
		xmQhd = qel;
	}
	xmQtl = qel;
	spin_unlock_irqrestore(&txqueue_lock, flags);

	if (debug & DEBUG_LOWER)
		printk("enqueued a 0x%02x command\n",qel->cbuf[0]);
}

static struct xmitQel *deQ(void)
{
	unsigned long flags;
	int i;
	struct xmitQel *qel=NULL;
	
	spin_lock_irqsave(&txqueue_lock, flags);
	if (xmQhd) {
		qel = xmQhd;
		xmQhd = qel->next;
		if(!xmQhd) xmQtl = NULL;
	}
	spin_unlock_irqrestore(&txqueue_lock, flags);

	if ((debug & DEBUG_LOWER) && qel) {
		int n;
		printk(KERN_DEBUG "ltpc: dequeued command ");
		n = qel->cbuflen;
		if (n>100) n=100;
		for(i=0;i<n;i++) printk("%02x ",qel->cbuf[i]);
		printk("\n");
	}

	return qel;
}

static struct xmitQel qels[16];

static unsigned char mailbox[16];
static unsigned char mboxinuse[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static int wait_timeout(struct net_device *dev, int c)
{
	
	
	int i;

	

	for(i=0;i<200000;i++) {
		if ( c != inb_p(dev->base_addr+6) ) return 0;
		udelay(100);
	}
	return 1; 
}


static int getmbox(void)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&mbox_lock, flags);
	for(i=1;i<16;i++) if(!mboxinuse[i]) {
		mboxinuse[i]=1;
		spin_unlock_irqrestore(&mbox_lock, flags);
		return i;
	}
	spin_unlock_irqrestore(&mbox_lock, flags);
	return 0;
}

static void handlefc(struct net_device *dev)
{
	
	int dma = dev->dma;
	int base = dev->base_addr;
	unsigned long flags;


	flags=claim_dma_lock();
	disable_dma(dma);
	clear_dma_ff(dma);
	set_dma_mode(dma,DMA_MODE_READ);
	set_dma_addr(dma,virt_to_bus(ltdmacbuf));
	set_dma_count(dma,50);
	enable_dma(dma);
	release_dma_lock(flags);

	inb_p(base+3);
	inb_p(base+2);

	if ( wait_timeout(dev,0xfc) ) printk("timed out in handlefc\n");
}

static void handlefd(struct net_device *dev)
{
	int dma = dev->dma;
	int base = dev->base_addr;
	unsigned long flags;

	flags=claim_dma_lock();
	disable_dma(dma);
	clear_dma_ff(dma);
	set_dma_mode(dma,DMA_MODE_READ);
	set_dma_addr(dma,virt_to_bus(ltdmabuf));
	set_dma_count(dma,800);
	enable_dma(dma);
	release_dma_lock(flags);

	inb_p(base+3);
	inb_p(base+2);

	if ( wait_timeout(dev,0xfd) ) printk("timed out in handlefd\n");
	sendup_buffer(dev);
} 

static void handlewrite(struct net_device *dev)
{
	
	
	int dma = dev->dma;
	int base = dev->base_addr;
	unsigned long flags;
	
	flags=claim_dma_lock();
	disable_dma(dma);
	clear_dma_ff(dma);
	set_dma_mode(dma,DMA_MODE_WRITE);
	set_dma_addr(dma,virt_to_bus(ltdmabuf));
	set_dma_count(dma,800);
	enable_dma(dma);
	release_dma_lock(flags);
	
	inb_p(base+3);
	inb_p(base+2);

	if ( wait_timeout(dev,0xfb) ) {
		flags=claim_dma_lock();
		printk("timed out in handlewrite, dma res %d\n",
			get_dma_residue(dev->dma) );
		release_dma_lock(flags);
	}
}

static void handleread(struct net_device *dev)
{
	
	
	int dma = dev->dma;
	int base = dev->base_addr;
	unsigned long flags;

	
	flags=claim_dma_lock();
	disable_dma(dma);
	clear_dma_ff(dma);
	set_dma_mode(dma,DMA_MODE_READ);
	set_dma_addr(dma,virt_to_bus(ltdmabuf));
	set_dma_count(dma,800);
	enable_dma(dma);
	release_dma_lock(flags);

	inb_p(base+3);
	inb_p(base+2);
	if ( wait_timeout(dev,0xfb) ) printk("timed out in handleread\n");
}

static void handlecommand(struct net_device *dev)
{
	
	int dma = dev->dma;
	int base = dev->base_addr;
	unsigned long flags;

	flags=claim_dma_lock();
	disable_dma(dma);
	clear_dma_ff(dma);
	set_dma_mode(dma,DMA_MODE_WRITE);
	set_dma_addr(dma,virt_to_bus(ltdmacbuf));
	set_dma_count(dma,50);
	enable_dma(dma);
	release_dma_lock(flags);
	inb_p(base+3);
	inb_p(base+2);
	if ( wait_timeout(dev,0xfa) ) printk("timed out in handlecommand\n");
} 

static unsigned char rescbuf[2] = {LT_GETRESULT,0};
static unsigned char resdbuf[2];

static int QInIdle;


static void idle(struct net_device *dev)
{
	unsigned long flags;
	int state;
	struct xmitQel *q = NULL;
	int oops;
	int i;
	int base = dev->base_addr;

	spin_lock_irqsave(&txqueue_lock, flags);
	if(QInIdle) {
		spin_unlock_irqrestore(&txqueue_lock, flags);
		return;
	}
	QInIdle = 1;
	spin_unlock_irqrestore(&txqueue_lock, flags);

	
	(void) inb_p(base+6);

	oops = 100;

loop:
	if (0>oops--) { 
		printk("idle: looped too many times\n");
		goto done;
	}

	state = inb_p(base+6);
	if (state != inb_p(base+6)) goto loop;

	switch(state) {
		case 0xfc:
			
			if (debug & DEBUG_LOWER) printk("idle: fc\n");
			handlefc(dev); 
			break;
		case 0xfd:
			
			if(debug & DEBUG_LOWER) printk("idle: fd\n");
			handlefd(dev); 
			break;
		case 0xf9:
			
			if (debug & DEBUG_LOWER) printk("idle: f9\n");
			if(!mboxinuse[0]) {
				mboxinuse[0] = 1;
				qels[0].cbuf = rescbuf;
				qels[0].cbuflen = 2;
				qels[0].dbuf = resdbuf;
				qels[0].dbuflen = 2;
				qels[0].QWrite = 0;
				qels[0].mailbox = 0;
				enQ(&qels[0]);
			}
			inb_p(dev->base_addr+1);
			inb_p(dev->base_addr+0);
			if( wait_timeout(dev,0xf9) )
				printk("timed out idle f9\n");
			break;
		case 0xf8:
			
			if (xmQhd) {
				inb_p(dev->base_addr+1);
				inb_p(dev->base_addr+0);
				if(wait_timeout(dev,0xf8) )
					printk("timed out idle f8\n");
			} else {
				goto done;
			}
			break;
		case 0xfa:
			
			if(debug & DEBUG_LOWER) printk("idle: fa\n");
			if (xmQhd) {
				q=deQ();
				memcpy(ltdmacbuf,q->cbuf,q->cbuflen);
				ltdmacbuf[1] = q->mailbox;
				if (debug>1) { 
					int n;
					printk("ltpc: sent command     ");
					n = q->cbuflen;
					if (n>100) n=100;
					for(i=0;i<n;i++)
						printk("%02x ",ltdmacbuf[i]);
					printk("\n");
				}
				handlecommand(dev);
					if(0xfa==inb_p(base+6)) {
						
						goto done;
					} 
			} else {
				
				if (!mboxinuse[0]) {
					mboxinuse[0] = 1;
					qels[0].cbuf = rescbuf;
					qels[0].cbuflen = 2;
					qels[0].dbuf = resdbuf;
					qels[0].dbuflen = 2;
					qels[0].QWrite = 0;
					qels[0].mailbox = 0;
					enQ(&qels[0]);
				} else {
					printk("trouble: response command already queued\n");
					goto done;
				}
			} 
			break;
		case 0Xfb:
			
			if(debug & DEBUG_LOWER) printk("idle: fb\n");
			if(q->QWrite) {
				memcpy(ltdmabuf,q->dbuf,q->dbuflen);
				handlewrite(dev);
			} else {
				handleread(dev);
				if(q->mailbox) {
					memcpy(q->dbuf,ltdmabuf,q->dbuflen);
				} else { 
					
					mailbox[ 0x0f & ltdmabuf[0] ] = ltdmabuf[1];
					mboxinuse[0]=0;
				}
			}
			break;
	}
	goto loop;

done:
	QInIdle=0;

	
	
	

	if (dev->irq) {
		inb_p(base+7);
		inb_p(base+7);
	}
}


static int do_write(struct net_device *dev, void *cbuf, int cbuflen,
	void *dbuf, int dbuflen)
{

	int i = getmbox();
	int ret;

	if(i) {
		qels[i].cbuf = cbuf;
		qels[i].cbuflen = cbuflen;
		qels[i].dbuf = dbuf;
		qels[i].dbuflen = dbuflen;
		qels[i].QWrite = 1;
		qels[i].mailbox = i;  
		enQ(&qels[i]);
		idle(dev);
		ret = mailbox[i];
		mboxinuse[i]=0;
		return ret;
	}
	printk("ltpc: could not allocate mbox\n");
	return -1;
}

static int do_read(struct net_device *dev, void *cbuf, int cbuflen,
	void *dbuf, int dbuflen)
{

	int i = getmbox();
	int ret;

	if(i) {
		qels[i].cbuf = cbuf;
		qels[i].cbuflen = cbuflen;
		qels[i].dbuf = dbuf;
		qels[i].dbuflen = dbuflen;
		qels[i].QWrite = 0;
		qels[i].mailbox = i;  
		enQ(&qels[i]);
		idle(dev);
		ret = mailbox[i];
		mboxinuse[i]=0;
		return ret;
	}
	printk("ltpc: could not allocate mbox\n");
	return -1;
}


static struct timer_list ltpc_timer;

static netdev_tx_t ltpc_xmit(struct sk_buff *skb, struct net_device *dev);

static int read_30 ( struct net_device *dev)
{
	lt_command c;
	c.getflags.command = LT_GETFLAGS;
	return do_read(dev, &c, sizeof(c.getflags),&c,0);
}

static int set_30 (struct net_device *dev,int x)
{
	lt_command c;
	c.setflags.command = LT_SETFLAGS;
	c.setflags.flags = x;
	return do_write(dev, &c, sizeof(c.setflags),&c,0);
}


static int sendup_buffer (struct net_device *dev)
{
	
	

	int dnode, snode, llaptype, len; 
	int sklen;
	struct sk_buff *skb;
	struct lt_rcvlap *ltc = (struct lt_rcvlap *) ltdmacbuf;

	if (ltc->command != LT_RCVLAP) {
		printk("unknown command 0x%02x from ltpc card\n",ltc->command);
		return -1;
	}
	dnode = ltc->dnode;
	snode = ltc->snode;
	llaptype = ltc->laptype;
	len = ltc->length; 

	sklen = len;
	if (llaptype == 1) 
		sklen += 8;  
	if(sklen > 800) {
		printk(KERN_INFO "%s: nonsense length in ltpc command 0x14: 0x%08x\n",
			dev->name,sklen);
		return -1;
	}

	if ( (llaptype==0) || (llaptype>2) ) {
		printk(KERN_INFO "%s: unknown LLAP type: %d\n",dev->name,llaptype);
		return -1;
	}


	skb = dev_alloc_skb(3+sklen);
	if (skb == NULL) 
	{
		printk("%s: dropping packet due to memory squeeze.\n",
			dev->name);
		return -1;
	}
	skb->dev = dev;

	if (sklen > len)
		skb_reserve(skb,8);
	skb_put(skb,len+3);
	skb->protocol = htons(ETH_P_LOCALTALK);
	
	skb->data[0] = dnode;
	skb->data[1] = snode;
	skb->data[2] = llaptype;
	skb_reset_mac_header(skb);	
	skb_pull(skb,3);

	
	skb_copy_to_linear_data(skb, ltdmabuf, len);

	skb_reset_transport_header(skb);

	dev->stats.rx_packets++;
	dev->stats.rx_bytes += skb->len;

	
	netif_rx(skb);
	return 0;
}

 
static irqreturn_t
ltpc_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;

	if (dev==NULL) {
		printk("ltpc_interrupt: unknown device.\n");
		return IRQ_NONE;
	}

	inb_p(dev->base_addr+6);  

	idle(dev); 
 
	 

	return IRQ_HANDLED;
}


static int ltpc_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct sockaddr_at *sa = (struct sockaddr_at *) &ifr->ifr_addr;
	
	struct ltpc_private *ltpc_priv = netdev_priv(dev);
	struct atalk_addr *aa = &ltpc_priv->my_addr;
	struct lt_init c;
	int ltflags;

	if(debug & DEBUG_VERBOSE) printk("ltpc_ioctl called\n");

	switch(cmd) {
		case SIOCSIFADDR:

			aa->s_net  = sa->sat_addr.s_net;
      
			
			c.command = LT_INIT;
			c.hint = sa->sat_addr.s_node;

			aa->s_node = do_read(dev,&c,sizeof(c),&c,0);

			
			ltflags = read_30(dev);
			ltflags |= LT_FLAG_ALLLAP;
			set_30 (dev,ltflags);  

			dev->broadcast[0] = 0xFF;
			dev->dev_addr[0] = aa->s_node;

			dev->addr_len=1;
   
			return 0;

		case SIOCGIFADDR:

			sa->sat_addr.s_net = aa->s_net;
			sa->sat_addr.s_node = aa->s_node;

			return 0;

		default: 
			return -EINVAL;
	}
}

static void set_multicast_list(struct net_device *dev)
{
	
	
}

static int ltpc_poll_counter;

static void ltpc_poll(unsigned long l)
{
	struct net_device *dev = (struct net_device *) l;

	del_timer(&ltpc_timer);

	if(debug & DEBUG_VERBOSE) {
		if (!ltpc_poll_counter) {
			ltpc_poll_counter = 50;
			printk("ltpc poll is alive\n");
		}
		ltpc_poll_counter--;
	}
  
	if (!dev)
		return;  

	
	idle(dev);
	ltpc_timer.expires = jiffies + HZ/20;
	
	add_timer(&ltpc_timer);
}


static netdev_tx_t ltpc_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int i;
	struct lt_sendlap cbuf;
	unsigned char *hdr;

	cbuf.command = LT_SENDLAP;
	cbuf.dnode = skb->data[0];
	cbuf.laptype = skb->data[2];
	skb_pull(skb,3);	
	cbuf.length = skb->len;	
	skb_reset_transport_header(skb);

	if(debug & DEBUG_UPPER) {
		printk("command ");
		for(i=0;i<6;i++)
			printk("%02x ",((unsigned char *)&cbuf)[i]);
		printk("\n");
	}

	hdr = skb_transport_header(skb);
	do_write(dev, &cbuf, sizeof(cbuf), hdr, skb->len);

	if(debug & DEBUG_UPPER) {
		printk("sent %d ddp bytes\n",skb->len);
		for (i = 0; i < skb->len; i++)
			printk("%02x ", hdr[i]);
		printk("\n");
	}

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;

	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}

  
static int __init ltpc_probe_dma(int base, int dma)
{
	int want = (dma == 3) ? 2 : (dma == 1) ? 1 : 3;
  	unsigned long timeout;
  	unsigned long f;
  
  	if (want & 1) {
		if (request_dma(1,"ltpc")) {
			want &= ~1;
		} else {
			f=claim_dma_lock();
			disable_dma(1);
			clear_dma_ff(1);
			set_dma_mode(1,DMA_MODE_WRITE);
			set_dma_addr(1,virt_to_bus(ltdmabuf));
			set_dma_count(1,sizeof(struct lt_mem));
			enable_dma(1);
			release_dma_lock(f);
		}
	}
	if (want & 2) {
		if (request_dma(3,"ltpc")) {
			want &= ~2;
		} else {
			f=claim_dma_lock();
			disable_dma(3);
			clear_dma_ff(3);
			set_dma_mode(3,DMA_MODE_WRITE);
			set_dma_addr(3,virt_to_bus(ltdmabuf));
			set_dma_count(3,sizeof(struct lt_mem));
			enable_dma(3);
			release_dma_lock(f);
		}
	}
	

	

	ltdmabuf[0] = LT_READMEM;
	ltdmabuf[1] = 1;  
	ltdmabuf[2] = 0; ltdmabuf[3] = 0;  
	ltdmabuf[4] = 0; ltdmabuf[5] = 1;  
	ltdmabuf[6] = 0; 

	inb_p(io+1);
	inb_p(io+0);
	timeout = jiffies+100*HZ/100;
	while(time_before(jiffies, timeout)) {
		if ( 0xfa == inb_p(io+6) ) break;
	}

	inb_p(io+3);
	inb_p(io+2);
	while(time_before(jiffies, timeout)) {
		if ( 0xfb == inb_p(io+6) ) break;
	}

	

	if ((want & 2) && (get_dma_residue(3)==sizeof(struct lt_mem))) {
		want &= ~2;
		free_dma(3);
	}

	if ((want & 1) && (get_dma_residue(1)==sizeof(struct lt_mem))) {
		want &= ~1;
		free_dma(1);
	}

	if (!want)
		return 0;

	return (want & 2) ? 3 : 1;
}

static const struct net_device_ops ltpc_netdev = {
	.ndo_start_xmit		= ltpc_xmit,
	.ndo_do_ioctl		= ltpc_ioctl,
	.ndo_set_rx_mode	= set_multicast_list,
};

struct net_device * __init ltpc_probe(void)
{
	struct net_device *dev;
	int err = -ENOMEM;
	int x=0,y=0;
	int autoirq;
	unsigned long f;
	unsigned long timeout;

	dev = alloc_ltalkdev(sizeof(struct ltpc_private));
	if (!dev)
		goto out;

	
	
	if (io != 0x240 && request_region(0x220,8,"ltpc")) {
		x = inb_p(0x220+6);
		if ( (x!=0xff) && (x>=0xf0) ) {
			io = 0x220;
			goto got_port;
		}
		release_region(0x220,8);
	}
	if (io != 0x220 && request_region(0x240,8,"ltpc")) {
		y = inb_p(0x240+6);
		if ( (y!=0xff) && (y>=0xf0) ){ 
			io = 0x240;
			goto got_port;
		}
		release_region(0x240,8);
	} 

	
	printk(KERN_ERR "LocalTalk card not found; 220 = %02x, 240 = %02x.\n", x,y);
	err = -ENODEV;
	goto out1;

 got_port:
	
	if (irq < 2) {
		unsigned long irq_mask;

		irq_mask = probe_irq_on();
		
		inb_p(io+7);
		inb_p(io+7);
		
		inb_p(io+6);
		mdelay(2);
		autoirq = probe_irq_off(irq_mask);

		if (autoirq == 0) {
			printk(KERN_ERR "ltpc: probe at %#x failed to detect IRQ line.\n", io);
		} else {
			irq = autoirq;
		}
	}

	
	ltdmabuf = (unsigned char *) dma_mem_alloc(1000);
	if (!ltdmabuf) {
		printk(KERN_ERR "ltpc: mem alloc failed\n");
		err = -ENOMEM;
		goto out2;
	}

	ltdmacbuf = &ltdmabuf[800];

	if(debug & DEBUG_VERBOSE) {
		printk("ltdmabuf pointer %08lx\n",(unsigned long) ltdmabuf);
	}

	

	inb_p(io+1);
	inb_p(io+3);

	msleep(20);

	inb_p(io+0);
	inb_p(io+2);
	inb_p(io+7); 
	inb_p(io+4); 
	inb_p(io+5);
	inb_p(io+5); 
	inb_p(io+6); 

	ssleep(1);
	
	dma = ltpc_probe_dma(io, dma);
	if (!dma) {  
		printk(KERN_ERR "No DMA channel found on ltpc card.\n");
		err = -ENODEV;
		goto out3;
	}

	
	if(irq)
		printk(KERN_INFO "Apple/Farallon LocalTalk-PC card at %03x, IR%d, DMA%d.\n",io,irq,dma);
	else
		printk(KERN_INFO "Apple/Farallon LocalTalk-PC card at %03x, DMA%d.  Using polled mode.\n",io,dma);

	dev->netdev_ops = &ltpc_netdev;
	dev->base_addr = io;
	dev->irq = irq;
	dev->dma = dma;

	

	f=claim_dma_lock();
	disable_dma(dma);
	clear_dma_ff(dma);
	set_dma_mode(dma,DMA_MODE_READ);
	set_dma_addr(dma,virt_to_bus(ltdmabuf));
	set_dma_count(dma,0x100);
	enable_dma(dma);
	release_dma_lock(f);

	(void) inb_p(io+3);
	(void) inb_p(io+2);
	timeout = jiffies+100*HZ/100;

	while(time_before(jiffies, timeout)) {
		if( 0xf9 == inb_p(io+6))
			break;
		schedule();
	}

	if(debug & DEBUG_VERBOSE) {
		printk("setting up timer and irq\n");
	}

	
	if (irq && request_irq( irq, ltpc_interrupt, 0, "ltpc", dev) >= 0)
	{
		(void) inb_p(io+7);  
		(void) inb_p(io+7);  
	} else {
		if( irq )
			printk(KERN_ERR "ltpc: IRQ already in use, using polled mode.\n");
		dev->irq = 0;
		
		
		init_timer(&ltpc_timer);
		ltpc_timer.function=ltpc_poll;
		ltpc_timer.data = (unsigned long) dev;

		ltpc_timer.expires = jiffies + HZ/20;
		add_timer(&ltpc_timer);
	}
	err = register_netdev(dev);
	if (err)
		goto out4;

	return NULL;
out4:
	del_timer_sync(&ltpc_timer);
	if (dev->irq)
		free_irq(dev->irq, dev);
out3:
	free_pages((unsigned long)ltdmabuf, get_order(1000));
out2:
	release_region(io, 8);
out1:
	free_netdev(dev);
out:
	return ERR_PTR(err);
}

#ifndef MODULE
static int __init ltpc_setup(char *str)
{
	int ints[5];

	str = get_options(str, ARRAY_SIZE(ints), ints);

	if (ints[0] == 0) {
		if (str && !strncmp(str, "auto", 4)) {
			
		}
		else {
			
			printk (KERN_ERR
				"ltpc: usage: ltpc=auto|iobase[,irq[,dma]]\n");
			return 0;
		}
	} else {
		io = ints[1];
		if (ints[0] > 1) {
			irq = ints[2];
		}
		if (ints[0] > 2) {
			dma = ints[3];
		}
		
	}
	return 1;
}

__setup("ltpc=", ltpc_setup);
#endif 

static struct net_device *dev_ltpc;

#ifdef MODULE

MODULE_LICENSE("GPL");
module_param(debug, int, 0);
module_param(io, int, 0);
module_param(irq, int, 0);
module_param(dma, int, 0);


static int __init ltpc_module_init(void)
{
        if(io == 0)
		printk(KERN_NOTICE
		       "ltpc: Autoprobing is not recommended for modules\n");

	dev_ltpc = ltpc_probe();
	if (IS_ERR(dev_ltpc))
		return PTR_ERR(dev_ltpc);
	return 0;
}
module_init(ltpc_module_init);
#endif

static void __exit ltpc_cleanup(void)
{

	if(debug & DEBUG_VERBOSE) printk("unregister_netdev\n");
	unregister_netdev(dev_ltpc);

	ltpc_timer.data = 0;  

	del_timer_sync(&ltpc_timer);

	if(debug & DEBUG_VERBOSE) printk("freeing irq\n");

	if (dev_ltpc->irq)
		free_irq(dev_ltpc->irq, dev_ltpc);

	if(debug & DEBUG_VERBOSE) printk("freeing dma\n");

	if (dev_ltpc->dma)
		free_dma(dev_ltpc->dma);

	if(debug & DEBUG_VERBOSE) printk("freeing ioaddr\n");

	if (dev_ltpc->base_addr)
		release_region(dev_ltpc->base_addr,8);

	free_netdev(dev_ltpc);

	if(debug & DEBUG_VERBOSE) printk("free_pages\n");

	free_pages( (unsigned long) ltdmabuf, get_order(1000));

	if(debug & DEBUG_VERBOSE) printk("returning from cleanup_module\n");
}

module_exit(ltpc_cleanup);
