/*
 * Linux ARCnet driver - device-independent routines
 * 
 * Written 1997 by David Woodhouse.
 * Written 1994-1999 by Avery Pennarun.
 * Written 1999-2000 by Martin Mares <mj@ucw.cz>.
 * Derived from skeleton.c by Donald Becker.
 *
 * Special thanks to Contemporary Controls, Inc. (www.ccontrols.com)
 *  for sponsoring the further development of this driver.
 *
 * **********************
 *
 * The original copyright was as follows:
 *
 * skeleton.c Written 1993 by Donald Becker.
 * Copyright 1993 United States Government as represented by the
 * Director, National Security Agency.  This software may only be used
 * and distributed according to the terms of the GNU General Public License as
 * modified by SRC, incorporated herein by reference.
 *
 * **********************
 * 
 * The change log is now in a file called ChangeLog in this directory.
 *
 * Sources:
 *  - Crynwr arcnet.com/arcether.com packet drivers.
 *  - arcnet.c v0.00 dated 1/1/94 and apparently by 
 *     Donald Becker - it didn't work :)
 *  - skeleton.c v0.05 dated 11/16/93 by Donald Becker
 *     (from Linux Kernel 1.1.45)
 *  - RFC's 1201 and 1051 - re: TCP/IP over ARCnet
 *  - The official ARCnet COM9026 data sheets (!) thanks to
 *     Ken Cornetet <kcornete@nyx10.cs.du.edu>
 *  - The official ARCnet COM20020 data sheets.
 *  - Information on some more obscure ARCnet controller chips, thanks
 *     to the nice people at SMSC.
 *  - net/inet/eth.c (from kernel 1.1.50) for header-building info.
 *  - Alternate Linux ARCnet source by V.Shergin <vsher@sao.stavropol.su>
 *  - Textual information and more alternate source from Joachim Koenig
 *     <jojo@repas.de>
 */

#define VERSION "arcnet: v3.94 BETA 2007/02/08 - by Avery Pennarun et al.\n"

#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <net/arp.h>
#include <linux/init.h>
#include <linux/arcdevice.h>
#include <linux/jiffies.h>

static void null_rx(struct net_device *dev, int bufnum,
		    struct archdr *pkthdr, int length);
static int null_build_header(struct sk_buff *skb, struct net_device *dev,
			     unsigned short type, uint8_t daddr);
static int null_prepare_tx(struct net_device *dev, struct archdr *pkt,
			   int length, int bufnum);

static void arcnet_rx(struct net_device *dev, int bufnum);

 struct ArcProto *arc_proto_map[256], *arc_proto_default,
   *arc_bcast_proto, *arc_raw_proto;

static struct ArcProto arc_proto_null =
{
	.suffix		= '?',
	.mtu		= XMTU,
	.is_ip          = 0,
	.rx		= null_rx,
	.build_header	= null_build_header,
	.prepare_tx	= null_prepare_tx,
	.continue_tx    = NULL,
	.ack_tx         = NULL
};

int arcnet_debug = ARCNET_DEBUG;

EXPORT_SYMBOL(arc_proto_map);
EXPORT_SYMBOL(arc_proto_default);
EXPORT_SYMBOL(arc_bcast_proto);
EXPORT_SYMBOL(arc_raw_proto);
EXPORT_SYMBOL(arcnet_unregister_proto);
EXPORT_SYMBOL(arcnet_debug);
EXPORT_SYMBOL(alloc_arcdev);
EXPORT_SYMBOL(arcnet_interrupt);
EXPORT_SYMBOL(arcnet_open);
EXPORT_SYMBOL(arcnet_close);
EXPORT_SYMBOL(arcnet_send_packet);
EXPORT_SYMBOL(arcnet_timeout);

static int arcnet_header(struct sk_buff *skb, struct net_device *dev,
			 unsigned short type, const void *daddr,
			 const void *saddr, unsigned len);
static int arcnet_rebuild_header(struct sk_buff *skb);
static int go_tx(struct net_device *dev);

static int debug = ARCNET_DEBUG;
module_param(debug, int, 0);
MODULE_LICENSE("GPL");

static int __init arcnet_init(void)
{
	int count;

	arcnet_debug = debug;

	printk("arcnet loaded.\n");

#ifdef ALPHA_WARNING
	BUGLVL(D_EXTRA) {
		printk("arcnet: ***\n"
		"arcnet: * Read arcnet.txt for important release notes!\n"
		       "arcnet: *\n"
		       "arcnet: * This is an ALPHA version! (Last stable release: v3.02)  E-mail\n"
		       "arcnet: * me if you have any questions, comments, or bug reports.\n"
		       "arcnet: ***\n");
	}
#endif

	
	arc_raw_proto = arc_proto_default = arc_bcast_proto = &arc_proto_null;
	for (count = 0; count < 256; count++)
		arc_proto_map[count] = arc_proto_default;

	BUGLVL(D_DURING)
	    printk("arcnet: struct sizes: %Zd %Zd %Zd %Zd %Zd\n",
		 sizeof(struct arc_hardware), sizeof(struct arc_rfc1201),
		sizeof(struct arc_rfc1051), sizeof(struct arc_eth_encap),
		   sizeof(struct archdr));

	return 0;
}

static void __exit arcnet_exit(void)
{
}

module_init(arcnet_init);
module_exit(arcnet_exit);

#if ARCNET_DEBUG_MAX & D_SKB
void arcnet_dump_skb(struct net_device *dev,
		     struct sk_buff *skb, char *desc)
{
	char hdr[32];

	
	snprintf(hdr, sizeof(hdr), "%6s:%s skb->data:", dev->name, desc);
	print_hex_dump(KERN_DEBUG, hdr, DUMP_PREFIX_OFFSET,
		       16, 1, skb->data, skb->len, true);
}

EXPORT_SYMBOL(arcnet_dump_skb);
#endif


#if (ARCNET_DEBUG_MAX & (D_RX | D_TX))
static void arcnet_dump_packet(struct net_device *dev, int bufnum,
			       char *desc, int take_arcnet_lock)
{
	struct arcnet_local *lp = netdev_priv(dev);
	int i, length;
	unsigned long flags = 0;
	static uint8_t buf[512];
	char hdr[32];

	if(take_arcnet_lock)
		spin_lock_irqsave(&lp->lock, flags);

	lp->hw.copy_from_card(dev, bufnum, 0, buf, 512);
	if(take_arcnet_lock)
		spin_unlock_irqrestore(&lp->lock, flags);

	
	length = (buf[2] ? 256 : 512);

	
	snprintf(hdr, sizeof(hdr), "%6s:%s packet dump:", dev->name, desc);
	print_hex_dump(KERN_DEBUG, hdr, DUMP_PREFIX_OFFSET,
		       16, 1, buf, length, true);
}

#else

#define arcnet_dump_packet(dev, bufnum, desc,take_arcnet_lock) do { } while (0)

#endif


void arcnet_unregister_proto(struct ArcProto *proto)
{
	int count;

	if (arc_proto_default == proto)
		arc_proto_default = &arc_proto_null;
	if (arc_bcast_proto == proto)
		arc_bcast_proto = arc_proto_default;
	if (arc_raw_proto == proto)
		arc_raw_proto = arc_proto_default;

	for (count = 0; count < 256; count++) {
		if (arc_proto_map[count] == proto)
			arc_proto_map[count] = arc_proto_default;
	}
}


static void release_arcbuf(struct net_device *dev, int bufnum)
{
	struct arcnet_local *lp = netdev_priv(dev);
	int i;

	lp->buf_queue[lp->first_free_buf++] = bufnum;
	lp->first_free_buf %= 5;

	BUGLVL(D_DURING) {
		BUGMSG(D_DURING, "release_arcbuf: freed #%d; buffer queue is now: ",
		       bufnum);
		for (i = lp->next_buf; i != lp->first_free_buf; i = (i+1) % 5)
			BUGMSG2(D_DURING, "#%d ", lp->buf_queue[i]);
		BUGMSG2(D_DURING, "\n");
	}
}


static int get_arcbuf(struct net_device *dev)
{
	struct arcnet_local *lp = netdev_priv(dev);
	int buf = -1, i;

	if (!atomic_dec_and_test(&lp->buf_lock)) {
		
		BUGMSG(D_NORMAL, "get_arcbuf: overlap (%d)!\n",
		       lp->buf_lock.counter);
	}
	else {			
		if (lp->next_buf >= 5)
			lp->next_buf -= 5;

		if (lp->next_buf == lp->first_free_buf)
			BUGMSG(D_NORMAL, "get_arcbuf: BUG: no buffers are available??\n");
		else {
			buf = lp->buf_queue[lp->next_buf++];
			lp->next_buf %= 5;
		}
	}


	BUGLVL(D_DURING) {
		BUGMSG(D_DURING, "get_arcbuf: got #%d; buffer queue is now: ", buf);
		for (i = lp->next_buf; i != lp->first_free_buf; i = (i+1) % 5)
			BUGMSG2(D_DURING, "#%d ", lp->buf_queue[i]);
		BUGMSG2(D_DURING, "\n");
	}

	atomic_inc(&lp->buf_lock);
	return buf;
}


static int choose_mtu(void)
{
	int count, mtu = 65535;

	
	for (count = 0; count < 256; count++) {
		if (arc_proto_map[count] != &arc_proto_null &&
		    arc_proto_map[count]->mtu < mtu) {
			mtu = arc_proto_map[count]->mtu;
		}
	}

	return mtu == 65535 ? XMTU : mtu;
}

static const struct header_ops arcnet_header_ops = {
	.create = arcnet_header,
	.rebuild = arcnet_rebuild_header,
};

static const struct net_device_ops arcnet_netdev_ops = {
	.ndo_open	= arcnet_open,
	.ndo_stop	= arcnet_close,
	.ndo_start_xmit = arcnet_send_packet,
	.ndo_tx_timeout = arcnet_timeout,
};

static void arcdev_setup(struct net_device *dev)
{
	dev->type = ARPHRD_ARCNET;
	dev->netdev_ops = &arcnet_netdev_ops;
	dev->header_ops = &arcnet_header_ops;
	dev->hard_header_len = sizeof(struct archdr);
	dev->mtu = choose_mtu();

	dev->addr_len = ARCNET_ALEN;
	dev->tx_queue_len = 100;
	dev->broadcast[0] = 0x00;	
	dev->watchdog_timeo = TX_TIMEOUT;

	
	dev->flags = IFF_BROADCAST;

}

struct net_device *alloc_arcdev(const char *name)
{
	struct net_device *dev;

	dev = alloc_netdev(sizeof(struct arcnet_local),
			   name && *name ? name : "arc%d", arcdev_setup);
	if(dev) {
		struct arcnet_local *lp = netdev_priv(dev);
		spin_lock_init(&lp->lock);
	}

	return dev;
}

int arcnet_open(struct net_device *dev)
{
	struct arcnet_local *lp = netdev_priv(dev);
	int count, newmtu, error;

	BUGMSG(D_INIT,"opened.");

	if (!try_module_get(lp->hw.owner))
		return -ENODEV;

	BUGLVL(D_PROTO) {
		BUGMSG(D_PROTO, "protocol map (default is '%c'): ",
		       arc_proto_default->suffix);
		for (count = 0; count < 256; count++)
			BUGMSG2(D_PROTO, "%c", arc_proto_map[count]->suffix);
		BUGMSG2(D_PROTO, "\n");
	}


	BUGMSG(D_INIT, "arcnet_open: resetting card.\n");

	error = -ENODEV;
	if (ARCRESET(0) && ARCRESET(1))
		goto out_module_put;

	newmtu = choose_mtu();
	if (newmtu < dev->mtu)
		dev->mtu = newmtu;

	BUGMSG(D_INIT, "arcnet_open: mtu: %d.\n", dev->mtu);

	
	memset(lp->default_proto, 0, sizeof(lp->default_proto));

	
	for (count = 0; count < 256; count++) {
		if (arc_proto_map[count] == arc_bcast_proto) {
			lp->default_proto[0] = count;
			break;
		}
	}

	
	atomic_set(&lp->buf_lock, 1);

	lp->next_buf = lp->first_free_buf = 0;
	release_arcbuf(dev, 0);
	release_arcbuf(dev, 1);
	release_arcbuf(dev, 2);
	release_arcbuf(dev, 3);
	lp->cur_tx = lp->next_tx = -1;
	lp->cur_rx = -1;

	lp->rfc1201.sequence = 1;

	
	if (lp->hw.open)
		lp->hw.open(dev);

	if (dev->dev_addr[0] == 0)
		BUGMSG(D_NORMAL, "WARNING!  Station address 00 is reserved "
		       "for broadcasts!\n");
	else if (dev->dev_addr[0] == 255)
		BUGMSG(D_NORMAL, "WARNING!  Station address FF may confuse "
		       "DOS networking programs!\n");

	BUGMSG(D_DEBUG, "%s: %d: %s\n",__FILE__,__LINE__,__func__);
	if (ASTATUS() & RESETflag) {
	  	BUGMSG(D_DEBUG, "%s: %d: %s\n",__FILE__,__LINE__,__func__);
		ACOMMAND(CFLAGScmd | RESETclear);
	}


	BUGMSG(D_DEBUG, "%s: %d: %s\n",__FILE__,__LINE__,__func__);
	
	AINTMASK(0);
	udelay(1);		
	BUGMSG(D_DEBUG, "%s: %d: %s\n",__FILE__,__LINE__,__func__);
	lp->intmask = NORXflag | RECONflag;
	AINTMASK(lp->intmask);
	BUGMSG(D_DEBUG, "%s: %d: %s\n",__FILE__,__LINE__,__func__);

	netif_start_queue(dev);

	return 0;

 out_module_put:
	module_put(lp->hw.owner);
	return error;
}


int arcnet_close(struct net_device *dev)
{
	struct arcnet_local *lp = netdev_priv(dev);

	netif_stop_queue(dev);

	
	AINTMASK(0);
	ACOMMAND(NOTXcmd);	
	ACOMMAND(NORXcmd);	
	mdelay(1);

	
	lp->hw.close(dev);
	module_put(lp->hw.owner);
	return 0;
}


static int arcnet_header(struct sk_buff *skb, struct net_device *dev,
			 unsigned short type, const void *daddr,
			 const void *saddr, unsigned len)
{
	const struct arcnet_local *lp = netdev_priv(dev);
	uint8_t _daddr, proto_num;
	struct ArcProto *proto;

	BUGMSG(D_DURING,
	    "create header from %d to %d; protocol %d (%Xh); size %u.\n",
	       saddr ? *(uint8_t *) saddr : -1,
	       daddr ? *(uint8_t *) daddr : -1,
	       type, type, len);

	if (skb->len!=0 && len != skb->len)
		BUGMSG(D_NORMAL, "arcnet_header: Yikes!  skb->len(%d) != len(%d)!\n",
		       skb->len, len);


  	
  	if(type == ETH_P_ARCNET) {
  		proto = arc_raw_proto;
  		BUGMSG(D_DEBUG, "arc_raw_proto used. proto='%c'\n",proto->suffix);
  		_daddr = daddr ? *(uint8_t *) daddr : 0;
  	}
	else if (!daddr) {
		*(uint16_t *) skb_push(skb, 2) = type;
		if (skb->network_header - skb->mac_header != 2)
			BUGMSG(D_NORMAL, "arcnet_header: Yikes!  diff (%d) is not 2!\n",
			       (int)(skb->network_header - skb->mac_header));
		return -2;	
	}
	else {
		
		_daddr = *(uint8_t *) daddr;
		proto_num = lp->default_proto[_daddr];
		proto = arc_proto_map[proto_num];
		BUGMSG(D_DURING, "building header for %02Xh using protocol '%c'\n",
		       proto_num, proto->suffix);
		if (proto == &arc_proto_null && arc_bcast_proto != proto) {
			BUGMSG(D_DURING, "actually, let's use '%c' instead.\n",
			       arc_bcast_proto->suffix);
			proto = arc_bcast_proto;
		}
	}
	return proto->build_header(skb, dev, type, _daddr);
}


static int arcnet_rebuild_header(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	struct arcnet_local *lp = netdev_priv(dev);
	int status = 0;		
	unsigned short type;
	uint8_t daddr=0;
	struct ArcProto *proto;
	if (skb->network_header - skb->mac_header != 2) {
		BUGMSG(D_NORMAL,
		       "rebuild_header: shouldn't be here! (hdrsize=%d)\n",
		       (int)(skb->network_header - skb->mac_header));
		return 0;
	}
	type = *(uint16_t *) skb_pull(skb, 2);
	BUGMSG(D_DURING, "rebuild header for protocol %Xh\n", type);

	if (type == ETH_P_IP) {
#ifdef CONFIG_INET
		BUGMSG(D_DURING, "rebuild header for ethernet protocol %Xh\n", type);
		status = arp_find(&daddr, skb) ? 1 : 0;
		BUGMSG(D_DURING, " rebuilt: dest is %d; protocol %Xh\n",
		       daddr, type);
#endif
	} else {
		BUGMSG(D_NORMAL,
		       "I don't understand ethernet protocol %Xh addresses!\n", type);
		dev->stats.tx_errors++;
		dev->stats.tx_aborted_errors++;
	}

	
	if (!status)
		return 0;

	
	proto = arc_proto_map[lp->default_proto[daddr]];
	proto->build_header(skb, dev, type, daddr);

	return 1;		
}



netdev_tx_t arcnet_send_packet(struct sk_buff *skb,
				     struct net_device *dev)
{
	struct arcnet_local *lp = netdev_priv(dev);
	struct archdr *pkt;
	struct arc_rfc1201 *soft;
	struct ArcProto *proto;
	int txbuf;
	unsigned long flags;
	int freeskb, retval;

	BUGMSG(D_DURING,
	       "transmit requested (status=%Xh, txbufs=%d/%d, len=%d, protocol %x)\n",
	       ASTATUS(), lp->cur_tx, lp->next_tx, skb->len,skb->protocol);

	pkt = (struct archdr *) skb->data;
	soft = &pkt->soft.rfc1201;
	proto = arc_proto_map[soft->proto];

	BUGMSG(D_SKB_SIZE, "skb: transmitting %d bytes to %02X\n",
		skb->len, pkt->hard.dest);
	BUGLVL(D_SKB) arcnet_dump_skb(dev, skb, "tx");

	
	if (skb->len - ARC_HDR_SIZE > XMTU && !proto->continue_tx) {
		BUGMSG(D_NORMAL, "fixme: packet too large: compensating badly!\n");
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;	
	}

	
	netif_stop_queue(dev);

	spin_lock_irqsave(&lp->lock, flags);
	AINTMASK(0);
	if(lp->next_tx == -1)
		txbuf = get_arcbuf(dev);
	else {
		txbuf = -1;
	}
	if (txbuf != -1) {
		if (proto->prepare_tx(dev, pkt, skb->len, txbuf) &&
		    !proto->ack_tx) {
			dev->stats.tx_bytes += skb->len;
			freeskb = 1;
		} else {
			
			lp->outgoing.proto = proto;
			lp->outgoing.skb = skb;
			lp->outgoing.pkt = pkt;

			freeskb = 0;

			if (proto->continue_tx &&
			    proto->continue_tx(dev, txbuf)) {
			  BUGMSG(D_NORMAL,
				 "bug! continue_tx finished the first time! "
				 "(proto='%c')\n", proto->suffix);
			}
		}
		retval = NETDEV_TX_OK;
		lp->next_tx = txbuf;
	} else {
		retval = NETDEV_TX_BUSY;
		freeskb = 0;
	}

	BUGMSG(D_DEBUG, "%s: %d: %s, status: %x\n",__FILE__,__LINE__,__func__,ASTATUS());
	
	AINTMASK(0);

	BUGMSG(D_DEBUG, "%s: %d: %s\n",__FILE__,__LINE__,__func__);
	lp->intmask |= TXFREEflag|EXCNAKflag;
	AINTMASK(lp->intmask);
	BUGMSG(D_DEBUG, "%s: %d: %s, status: %x\n",__FILE__,__LINE__,__func__,ASTATUS());

	spin_unlock_irqrestore(&lp->lock, flags);
	if (freeskb) {
		dev_kfree_skb(skb);
	}
	return retval;		
}


static int go_tx(struct net_device *dev)
{
	struct arcnet_local *lp = netdev_priv(dev);

	BUGMSG(D_DURING, "go_tx: status=%Xh, intmask=%Xh, next_tx=%d, cur_tx=%d\n",
	       ASTATUS(), lp->intmask, lp->next_tx, lp->cur_tx);

	if (lp->cur_tx != -1 || lp->next_tx == -1)
		return 0;

	BUGLVL(D_TX) arcnet_dump_packet(dev, lp->next_tx, "go_tx", 0);

	lp->cur_tx = lp->next_tx;
	lp->next_tx = -1;

	
	ACOMMAND(TXcmd | (lp->cur_tx << 3));

	dev->stats.tx_packets++;
	lp->lasttrans_dest = lp->lastload_dest;
	lp->lastload_dest = 0;
	lp->excnak_pending = 0;
	lp->intmask |= TXFREEflag|EXCNAKflag;

	return 1;
}


void arcnet_timeout(struct net_device *dev)
{
	unsigned long flags;
	struct arcnet_local *lp = netdev_priv(dev);
	int status = ASTATUS();
	char *msg;

	spin_lock_irqsave(&lp->lock, flags);
	if (status & TXFREEflag) {	
		msg = " - missed IRQ?";
	} else {
		msg = "";
		dev->stats.tx_aborted_errors++;
		lp->timed_out = 1;
		ACOMMAND(NOTXcmd | (lp->cur_tx << 3));
	}
	dev->stats.tx_errors++;

	
	AINTMASK(0);
	lp->intmask |= TXFREEflag|EXCNAKflag;
	AINTMASK(lp->intmask);
	
	spin_unlock_irqrestore(&lp->lock, flags);

	if (time_after(jiffies, lp->last_timeout + 10*HZ)) {
		BUGMSG(D_EXTRA, "tx timed out%s (status=%Xh, intmask=%Xh, dest=%02Xh)\n",
		       msg, status, lp->intmask, lp->lasttrans_dest);
		lp->last_timeout = jiffies;
	}

	if (lp->cur_tx == -1)
		netif_wake_queue(dev);
}


irqreturn_t arcnet_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct arcnet_local *lp;
	int recbuf, status, diagstatus, didsomething, boguscount;
	int retval = IRQ_NONE;

	BUGMSG(D_DURING, "\n");

	BUGMSG(D_DURING, "in arcnet_interrupt\n");

	lp = netdev_priv(dev);
	BUG_ON(!lp);
		
	spin_lock(&lp->lock);

	if (!netif_running(dev)) {
		if (ASTATUS() & RESETflag)
			ACOMMAND(CFLAGScmd | RESETclear);
		AINTMASK(0);
		spin_unlock(&lp->lock);
		return IRQ_HANDLED;
	}

	BUGMSG(D_DURING, "in arcnet_inthandler (status=%Xh, intmask=%Xh)\n",
	       ASTATUS(), lp->intmask);

	boguscount = 5;
	do {
		status = ASTATUS();
                diagstatus = (status >> 8) & 0xFF;

		BUGMSG(D_DEBUG, "%s: %d: %s: status=%x\n",
			__FILE__,__LINE__,__func__,status);
		didsomething = 0;

		if (status & RESETflag) {
			BUGMSG(D_NORMAL, "spurious reset (status=%Xh)\n", status);
			arcnet_close(dev);
			arcnet_open(dev);

			
			break;
		}
		recbuf = -1;
		if (status & lp->intmask & NORXflag) {
			recbuf = lp->cur_rx;
			BUGMSG(D_DURING, "Buffer #%d: receive irq (status=%Xh)\n",
			       recbuf, status);

			lp->cur_rx = get_arcbuf(dev);
			if (lp->cur_rx != -1) {
				BUGMSG(D_DURING, "enabling receive to buffer #%d\n",
				       lp->cur_rx);
				ACOMMAND(RXcmd | (lp->cur_rx << 3) | RXbcasts);
			}
			didsomething++;
		}

		if((diagstatus & EXCNAKflag)) {
			BUGMSG(D_DURING, "EXCNAK IRQ (diagstat=%Xh)\n",
			       diagstatus);

                        ACOMMAND(NOTXcmd);      
                        lp->excnak_pending = 1;

                        ACOMMAND(EXCNAKclear);
			lp->intmask &= ~(EXCNAKflag);
                        didsomething++;
                }


		
		if ((status & lp->intmask & TXFREEflag) || lp->timed_out) {
			lp->intmask &= ~(TXFREEflag|EXCNAKflag);

			BUGMSG(D_DURING, "TX IRQ (stat=%Xh)\n", status);

			if (lp->cur_tx != -1 && !lp->timed_out) {
				if(!(status & TXACKflag)) {
					if (lp->lasttrans_dest != 0) {
						BUGMSG(D_EXTRA,
						       "transmit was not acknowledged! "
						       "(status=%Xh, dest=%02Xh)\n",
						       status, lp->lasttrans_dest);
						dev->stats.tx_errors++;
						dev->stats.tx_carrier_errors++;
					} else {
						BUGMSG(D_DURING,
						       "broadcast was not acknowledged; that's normal "
						       "(status=%Xh, dest=%02Xh)\n",
						       status, lp->lasttrans_dest);
					}
				}

				if (lp->outgoing.proto &&
				    lp->outgoing.proto->ack_tx) {
				  int ackstatus;
				  if(status & TXACKflag)
                                    ackstatus=2;
                                  else if(lp->excnak_pending)
                                    ackstatus=1;
                                  else
                                    ackstatus=0;

                                  lp->outgoing.proto
                                    ->ack_tx(dev, ackstatus);
				}
			}
			if (lp->cur_tx != -1)
				release_arcbuf(dev, lp->cur_tx);

			lp->cur_tx = -1;
			lp->timed_out = 0;
			didsomething++;

			
			go_tx(dev);

			
			if (lp->outgoing.proto && lp->outgoing.proto->continue_tx) {
				int txbuf = get_arcbuf(dev);
				if (txbuf != -1) {
					if (lp->outgoing.proto->continue_tx(dev, txbuf)) {
						
						dev->stats.tx_bytes += lp->outgoing.skb->len;
						if(!lp->outgoing.proto->ack_tx)
						  {
						    dev_kfree_skb_irq(lp->outgoing.skb);
						    lp->outgoing.proto = NULL;
						  }
					}
					lp->next_tx = txbuf;
				}
			}
			
			if (lp->cur_tx == -1)
				netif_wake_queue(dev);
		}
		
		if (recbuf != -1) {
			BUGLVL(D_RX) arcnet_dump_packet(dev, recbuf, "rx irq", 0);

			arcnet_rx(dev, recbuf);
			release_arcbuf(dev, recbuf);

			didsomething++;
		}
		if (status & lp->intmask & RECONflag) {
			ACOMMAND(CFLAGScmd | CONFIGclear);
			dev->stats.tx_carrier_errors++;

			BUGMSG(D_RECON, "Network reconfiguration detected (status=%Xh)\n",
			       status);
			
			if(diagstatus & 0x80)
				BUGMSG(D_RECON,"Put out that recon myself\n");

			
			if (!lp->first_recon || !lp->last_recon ||
			    time_after(jiffies, lp->last_recon + HZ * 10)) {
				if (lp->network_down)
					BUGMSG(D_NORMAL, "reconfiguration detected: cabling restored?\n");
				lp->first_recon = lp->last_recon = jiffies;
				lp->num_recons = lp->network_down = 0;

				BUGMSG(D_DURING, "recon: clearing counters.\n");
			} else {	
				lp->last_recon = jiffies;
				lp->num_recons++;

				BUGMSG(D_DURING, "recon: counter=%d, time=%lds, net=%d\n",
				       lp->num_recons,
				 (lp->last_recon - lp->first_recon) / HZ,
				       lp->network_down);

				if (!lp->network_down &&
				    (lp->last_recon - lp->first_recon) <= HZ * 60 &&
				    lp->num_recons >= RECON_THRESHOLD) {
					lp->network_down = 1;
					BUGMSG(D_NORMAL, "many reconfigurations detected: cabling problem?\n");
				} else if (!lp->network_down &&
					   lp->last_recon - lp->first_recon > HZ * 60) {
					
					lp->first_recon = lp->last_recon;
					lp->num_recons = 1;
				}
			}
		} else if (lp->network_down &&
				time_after(jiffies, lp->last_recon + HZ * 10)) {
			if (lp->network_down)
				BUGMSG(D_NORMAL, "cabling restored?\n");
			lp->first_recon = lp->last_recon = 0;
			lp->num_recons = lp->network_down = 0;

			BUGMSG(D_DURING, "not recon: clearing counters anyway.\n");
		}

		if(didsomething) {
			retval |= IRQ_HANDLED;
		}
	}
	while (--boguscount && didsomething);

	BUGMSG(D_DURING, "arcnet_interrupt complete (status=%Xh, count=%d)\n",
	       ASTATUS(), boguscount);
	BUGMSG(D_DURING, "\n");


	AINTMASK(0);
	udelay(1);
	AINTMASK(lp->intmask);
	
	spin_unlock(&lp->lock);
	return retval;
}


static void arcnet_rx(struct net_device *dev, int bufnum)
{
	struct arcnet_local *lp = netdev_priv(dev);
	struct archdr pkt;
	struct arc_rfc1201 *soft;
	int length, ofs;

	soft = &pkt.soft.rfc1201;

	lp->hw.copy_from_card(dev, bufnum, 0, &pkt, sizeof(ARC_HDR_SIZE));
	if (pkt.hard.offset[0]) {
		ofs = pkt.hard.offset[0];
		length = 256 - ofs;
	} else {
		ofs = pkt.hard.offset[1];
		length = 512 - ofs;
	}

	
	if (sizeof(pkt.soft) <= length)
		lp->hw.copy_from_card(dev, bufnum, ofs, soft, sizeof(pkt.soft));
	else {
		memset(&pkt.soft, 0, sizeof(pkt.soft));
		lp->hw.copy_from_card(dev, bufnum, ofs, soft, length);
	}

	BUGMSG(D_DURING, "Buffer #%d: received packet from %02Xh to %02Xh "
	       "(%d+4 bytes)\n",
	       bufnum, pkt.hard.source, pkt.hard.dest, length);

	dev->stats.rx_packets++;
	dev->stats.rx_bytes += length + ARC_HDR_SIZE;

	
	if (arc_proto_map[soft->proto]->is_ip) {
		BUGLVL(D_PROTO) {
			struct ArcProto
			*oldp = arc_proto_map[lp->default_proto[pkt.hard.source]],
			*newp = arc_proto_map[soft->proto];

			if (oldp != newp) {
				BUGMSG(D_PROTO,
				       "got protocol %02Xh; encap for host %02Xh is now '%c'"
				       " (was '%c')\n", soft->proto, pkt.hard.source,
				       newp->suffix, oldp->suffix);
			}
		}

		
		lp->default_proto[0] = soft->proto;

		
		lp->default_proto[pkt.hard.source] = soft->proto;
	}
	
	arc_proto_map[soft->proto]->rx(dev, bufnum, &pkt, length);
}


static void null_rx(struct net_device *dev, int bufnum,
		    struct archdr *pkthdr, int length)
{
	BUGMSG(D_PROTO,
	"rx: don't know how to deal with proto %02Xh from host %02Xh.\n",
	       pkthdr->soft.rfc1201.proto, pkthdr->hard.source);
}


static int null_build_header(struct sk_buff *skb, struct net_device *dev,
			     unsigned short type, uint8_t daddr)
{
	struct arcnet_local *lp = netdev_priv(dev);

	BUGMSG(D_PROTO,
	       "tx: can't build header for encap %02Xh; load a protocol driver.\n",
	       lp->default_proto[daddr]);

	
	return 0;
}


static int null_prepare_tx(struct net_device *dev, struct archdr *pkt,
			   int length, int bufnum)
{
	struct arcnet_local *lp = netdev_priv(dev);
	struct arc_hardware newpkt;

	BUGMSG(D_PROTO, "tx: no encap for this host; load a protocol driver.\n");

	
	newpkt.source = newpkt.dest = dev->dev_addr[0];

	
	newpkt.offset[0] = 0xFF;

	lp->hw.copy_to_card(dev, bufnum, 0, &newpkt, ARC_HDR_SIZE);

	return 1;		
}
