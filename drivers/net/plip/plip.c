/*
 * Authors:	Donald Becker <becker@scyld.com>
 *		Tommy Thorn <thorn@daimi.aau.dk>
 *		Tanabe Hiroyasu <hiro@sanpo.t.u-tokyo.ac.jp>
 *		Alan Cox <gw4pts@gw4pts.ampr.org>
 *		Peter Bauer <100136.3530@compuserve.com>
 *		Niibe Yutaka <gniibe@mri.co.jp>
 *		Nimrod Zimerman <zimerman@mailandnews.com>
 *
 * Enhancements:
 *		Modularization and ifreq/ifmap support by Alan Cox.
 *		Rewritten by Niibe Yutaka.
 *		parport-sharing awareness code by Philip Blundell.
 *		SMP locking by Niibe Yutaka.
 *		Support for parallel ports with no IRQ (poll mode),
 *		Modifications to use the parallel port API
 *		by Nimrod Zimerman.
 *
 * Fixes:
 *		Niibe Yutaka
 *		  - Module initialization.
 *		  - MTU fix.
 *		  - Make sure other end is OK, before sending a packet.
 *		  - Fix immediate timer problem.
 *
 *		Al Viro
 *		  - Changed {enable,disable}_irq handling to make it work
 *		    with new ("stack") semantics.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */


static const char version[] = "NET3 PLIP version 2.4-parport gniibe@mri.co.jp\n";


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/inetdevice.h>
#include <linux/skbuff.h>
#include <linux/if_plip.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/parport.h>
#include <linux/bitops.h>

#include <net/neighbour.h>

#include <asm/irq.h>
#include <asm/byteorder.h>

#define PLIP_MAX  8

#ifndef NET_DEBUG
#define NET_DEBUG 1
#endif
static const unsigned int net_debug = NET_DEBUG;

#define ENABLE(irq)  if (irq != -1) enable_irq(irq)
#define DISABLE(irq) if (irq != -1) disable_irq(irq)

#define PLIP_DELAY_UNIT		   1

#define PLIP_TRIGGER_WAIT	 500

#define PLIP_NIBBLE_WAIT        3000

static void plip_kick_bh(struct work_struct *work);
static void plip_bh(struct work_struct *work);
static void plip_timer_bh(struct work_struct *work);

static void plip_interrupt(void *dev_id);

static int plip_tx_packet(struct sk_buff *skb, struct net_device *dev);
static int plip_hard_header(struct sk_buff *skb, struct net_device *dev,
                            unsigned short type, const void *daddr,
			    const void *saddr, unsigned len);
static int plip_hard_header_cache(const struct neighbour *neigh,
                                  struct hh_cache *hh, __be16 type);
static int plip_open(struct net_device *dev);
static int plip_close(struct net_device *dev);
static int plip_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);
static int plip_preempt(void *handle);
static void plip_wakeup(void *handle);

enum plip_connection_state {
	PLIP_CN_NONE=0,
	PLIP_CN_RECEIVE,
	PLIP_CN_SEND,
	PLIP_CN_CLOSING,
	PLIP_CN_ERROR
};

enum plip_packet_state {
	PLIP_PK_DONE=0,
	PLIP_PK_TRIGGER,
	PLIP_PK_LENGTH_LSB,
	PLIP_PK_LENGTH_MSB,
	PLIP_PK_DATA,
	PLIP_PK_CHECKSUM
};

enum plip_nibble_state {
	PLIP_NB_BEGIN,
	PLIP_NB_1,
	PLIP_NB_2,
};

struct plip_local {
	enum plip_packet_state state;
	enum plip_nibble_state nibble;
	union {
		struct {
#if defined(__LITTLE_ENDIAN)
			unsigned char lsb;
			unsigned char msb;
#elif defined(__BIG_ENDIAN)
			unsigned char msb;
			unsigned char lsb;
#else
#error	"Please fix the endianness defines in <asm/byteorder.h>"
#endif
		} b;
		unsigned short h;
	} length;
	unsigned short byte;
	unsigned char  checksum;
	unsigned char  data;
	struct sk_buff *skb;
};

struct net_local {
	struct net_device *dev;
	struct work_struct immediate;
	struct delayed_work deferred;
	struct delayed_work timer;
	struct plip_local snd_data;
	struct plip_local rcv_data;
	struct pardevice *pardev;
	unsigned long  trigger;
	unsigned long  nibble;
	enum plip_connection_state connection;
	unsigned short timeout_count;
	int is_deferred;
	int port_owner;
	int should_relinquish;
	spinlock_t lock;
	atomic_t kill_timer;
	struct completion killed_timer_cmp;
};

static inline void enable_parport_interrupts (struct net_device *dev)
{
	if (dev->irq != -1)
	{
		struct parport *port =
		   ((struct net_local *)netdev_priv(dev))->pardev->port;
		port->ops->enable_irq (port);
	}
}

static inline void disable_parport_interrupts (struct net_device *dev)
{
	if (dev->irq != -1)
	{
		struct parport *port =
		   ((struct net_local *)netdev_priv(dev))->pardev->port;
		port->ops->disable_irq (port);
	}
}

static inline void write_data (struct net_device *dev, unsigned char data)
{
	struct parport *port =
	   ((struct net_local *)netdev_priv(dev))->pardev->port;

	port->ops->write_data (port, data);
}

static inline unsigned char read_status (struct net_device *dev)
{
	struct parport *port =
	   ((struct net_local *)netdev_priv(dev))->pardev->port;

	return port->ops->read_status (port);
}

static const struct header_ops plip_header_ops = {
	.create	= plip_hard_header,
	.cache  = plip_hard_header_cache,
};

static const struct net_device_ops plip_netdev_ops = {
	.ndo_open		 = plip_open,
	.ndo_stop		 = plip_close,
	.ndo_start_xmit		 = plip_tx_packet,
	.ndo_do_ioctl		 = plip_ioctl,
	.ndo_change_mtu		 = eth_change_mtu,
	.ndo_set_mac_address	 = eth_mac_addr,
	.ndo_validate_addr	 = eth_validate_addr,
};

static void
plip_init_netdev(struct net_device *dev)
{
	struct net_local *nl = netdev_priv(dev);

	
	dev->tx_queue_len 	 = 10;
	dev->flags	         = IFF_POINTOPOINT|IFF_NOARP;
	memset(dev->dev_addr, 0xfc, ETH_ALEN);

	dev->netdev_ops		 = &plip_netdev_ops;
	dev->header_ops          = &plip_header_ops;


	nl->port_owner = 0;

	
	nl->trigger	= PLIP_TRIGGER_WAIT;
	nl->nibble	= PLIP_NIBBLE_WAIT;

	
	INIT_WORK(&nl->immediate, plip_bh);
	INIT_DELAYED_WORK(&nl->deferred, plip_kick_bh);

	if (dev->irq == -1)
		INIT_DELAYED_WORK(&nl->timer, plip_timer_bh);

	spin_lock_init(&nl->lock);
}

static void
plip_kick_bh(struct work_struct *work)
{
	struct net_local *nl =
		container_of(work, struct net_local, deferred.work);

	if (nl->is_deferred)
		schedule_work(&nl->immediate);
}

static int plip_none(struct net_device *, struct net_local *,
		     struct plip_local *, struct plip_local *);
static int plip_receive_packet(struct net_device *, struct net_local *,
			       struct plip_local *, struct plip_local *);
static int plip_send_packet(struct net_device *, struct net_local *,
			    struct plip_local *, struct plip_local *);
static int plip_connection_close(struct net_device *, struct net_local *,
				 struct plip_local *, struct plip_local *);
static int plip_error(struct net_device *, struct net_local *,
		      struct plip_local *, struct plip_local *);
static int plip_bh_timeout_error(struct net_device *dev, struct net_local *nl,
				 struct plip_local *snd,
				 struct plip_local *rcv,
				 int error);

#define OK        0
#define TIMEOUT   1
#define ERROR     2
#define HS_TIMEOUT	3

typedef int (*plip_func)(struct net_device *dev, struct net_local *nl,
			 struct plip_local *snd, struct plip_local *rcv);

static const plip_func connection_state_table[] =
{
	plip_none,
	plip_receive_packet,
	plip_send_packet,
	plip_connection_close,
	plip_error
};

static void
plip_bh(struct work_struct *work)
{
	struct net_local *nl = container_of(work, struct net_local, immediate);
	struct plip_local *snd = &nl->snd_data;
	struct plip_local *rcv = &nl->rcv_data;
	plip_func f;
	int r;

	nl->is_deferred = 0;
	f = connection_state_table[nl->connection];
	if ((r = (*f)(nl->dev, nl, snd, rcv)) != OK &&
	    (r = plip_bh_timeout_error(nl->dev, nl, snd, rcv, r)) != OK) {
		nl->is_deferred = 1;
		schedule_delayed_work(&nl->deferred, 1);
	}
}

static void
plip_timer_bh(struct work_struct *work)
{
	struct net_local *nl =
		container_of(work, struct net_local, timer.work);

	if (!(atomic_read (&nl->kill_timer))) {
		plip_interrupt (nl->dev);

		schedule_delayed_work(&nl->timer, 1);
	}
	else {
		complete(&nl->killed_timer_cmp);
	}
}

static int
plip_bh_timeout_error(struct net_device *dev, struct net_local *nl,
		      struct plip_local *snd, struct plip_local *rcv,
		      int error)
{
	unsigned char c0;

	spin_lock_irq(&nl->lock);
	if (nl->connection == PLIP_CN_SEND) {

		if (error != ERROR) { 
			nl->timeout_count++;
			if ((error == HS_TIMEOUT && nl->timeout_count <= 10) ||
			    nl->timeout_count <= 3) {
				spin_unlock_irq(&nl->lock);
				
				return TIMEOUT;
			}
			c0 = read_status(dev);
			printk(KERN_WARNING "%s: transmit timeout(%d,%02x)\n",
			       dev->name, snd->state, c0);
		} else
			error = HS_TIMEOUT;
		dev->stats.tx_errors++;
		dev->stats.tx_aborted_errors++;
	} else if (nl->connection == PLIP_CN_RECEIVE) {
		if (rcv->state == PLIP_PK_TRIGGER) {
			
			spin_unlock_irq(&nl->lock);
			return OK;
		}
		if (error != ERROR) { 
			if (++nl->timeout_count <= 3) {
				spin_unlock_irq(&nl->lock);
				
				return TIMEOUT;
			}
			c0 = read_status(dev);
			printk(KERN_WARNING "%s: receive timeout(%d,%02x)\n",
			       dev->name, rcv->state, c0);
		}
		dev->stats.rx_dropped++;
	}
	rcv->state = PLIP_PK_DONE;
	if (rcv->skb) {
		kfree_skb(rcv->skb);
		rcv->skb = NULL;
	}
	snd->state = PLIP_PK_DONE;
	if (snd->skb) {
		dev_kfree_skb(snd->skb);
		snd->skb = NULL;
	}
	spin_unlock_irq(&nl->lock);
	if (error == HS_TIMEOUT) {
		DISABLE(dev->irq);
		synchronize_irq(dev->irq);
	}
	disable_parport_interrupts (dev);
	netif_stop_queue (dev);
	nl->connection = PLIP_CN_ERROR;
	write_data (dev, 0x00);

	return TIMEOUT;
}

static int
plip_none(struct net_device *dev, struct net_local *nl,
	  struct plip_local *snd, struct plip_local *rcv)
{
	return OK;
}

static inline int
plip_receive(unsigned short nibble_timeout, struct net_device *dev,
	     enum plip_nibble_state *ns_p, unsigned char *data_p)
{
	unsigned char c0, c1;
	unsigned int cx;

	switch (*ns_p) {
	case PLIP_NB_BEGIN:
		cx = nibble_timeout;
		while (1) {
			c0 = read_status(dev);
			udelay(PLIP_DELAY_UNIT);
			if ((c0 & 0x80) == 0) {
				c1 = read_status(dev);
				if (c0 == c1)
					break;
			}
			if (--cx == 0)
				return TIMEOUT;
		}
		*data_p = (c0 >> 3) & 0x0f;
		write_data (dev, 0x10); 
		*ns_p = PLIP_NB_1;

	case PLIP_NB_1:
		cx = nibble_timeout;
		while (1) {
			c0 = read_status(dev);
			udelay(PLIP_DELAY_UNIT);
			if (c0 & 0x80) {
				c1 = read_status(dev);
				if (c0 == c1)
					break;
			}
			if (--cx == 0)
				return TIMEOUT;
		}
		*data_p |= (c0 << 1) & 0xf0;
		write_data (dev, 0x00); 
		*ns_p = PLIP_NB_BEGIN;
	case PLIP_NB_2:
		break;
	}
	return OK;
}


static __be16 plip_type_trans(struct sk_buff *skb, struct net_device *dev)
{
	struct ethhdr *eth;
	unsigned char *rawp;

	skb_reset_mac_header(skb);
	skb_pull(skb,dev->hard_header_len);
	eth = eth_hdr(skb);

	if(*eth->h_dest&1)
	{
		if(memcmp(eth->h_dest,dev->broadcast, ETH_ALEN)==0)
			skb->pkt_type=PACKET_BROADCAST;
		else
			skb->pkt_type=PACKET_MULTICAST;
	}


	if (ntohs(eth->h_proto) >= 1536)
		return eth->h_proto;

	rawp = skb->data;

	if (*(unsigned short *)rawp == 0xFFFF)
		return htons(ETH_P_802_3);

	return htons(ETH_P_802_2);
}

static int
plip_receive_packet(struct net_device *dev, struct net_local *nl,
		    struct plip_local *snd, struct plip_local *rcv)
{
	unsigned short nibble_timeout = nl->nibble;
	unsigned char *lbuf;

	switch (rcv->state) {
	case PLIP_PK_TRIGGER:
		DISABLE(dev->irq);
		
		disable_parport_interrupts (dev);
		write_data (dev, 0x01); 
		if (net_debug > 2)
			printk(KERN_DEBUG "%s: receive start\n", dev->name);
		rcv->state = PLIP_PK_LENGTH_LSB;
		rcv->nibble = PLIP_NB_BEGIN;

	case PLIP_PK_LENGTH_LSB:
		if (snd->state != PLIP_PK_DONE) {
			if (plip_receive(nl->trigger, dev,
					 &rcv->nibble, &rcv->length.b.lsb)) {
				
				rcv->state = PLIP_PK_DONE;
				nl->is_deferred = 1;
				nl->connection = PLIP_CN_SEND;
				schedule_delayed_work(&nl->deferred, 1);
				enable_parport_interrupts (dev);
				ENABLE(dev->irq);
				return OK;
			}
		} else {
			if (plip_receive(nibble_timeout, dev,
					 &rcv->nibble, &rcv->length.b.lsb))
				return TIMEOUT;
		}
		rcv->state = PLIP_PK_LENGTH_MSB;

	case PLIP_PK_LENGTH_MSB:
		if (plip_receive(nibble_timeout, dev,
				 &rcv->nibble, &rcv->length.b.msb))
			return TIMEOUT;
		if (rcv->length.h > dev->mtu + dev->hard_header_len ||
		    rcv->length.h < 8) {
			printk(KERN_WARNING "%s: bogus packet size %d.\n", dev->name, rcv->length.h);
			return ERROR;
		}
		
		rcv->skb = dev_alloc_skb(rcv->length.h + 2);
		if (rcv->skb == NULL) {
			printk(KERN_ERR "%s: Memory squeeze.\n", dev->name);
			return ERROR;
		}
		skb_reserve(rcv->skb, 2);	
		skb_put(rcv->skb,rcv->length.h);
		rcv->skb->dev = dev;
		rcv->state = PLIP_PK_DATA;
		rcv->byte = 0;
		rcv->checksum = 0;

	case PLIP_PK_DATA:
		lbuf = rcv->skb->data;
		do {
			if (plip_receive(nibble_timeout, dev,
					 &rcv->nibble, &lbuf[rcv->byte]))
				return TIMEOUT;
		} while (++rcv->byte < rcv->length.h);
		do {
			rcv->checksum += lbuf[--rcv->byte];
		} while (rcv->byte);
		rcv->state = PLIP_PK_CHECKSUM;

	case PLIP_PK_CHECKSUM:
		if (plip_receive(nibble_timeout, dev,
				 &rcv->nibble, &rcv->data))
			return TIMEOUT;
		if (rcv->data != rcv->checksum) {
			dev->stats.rx_crc_errors++;
			if (net_debug)
				printk(KERN_DEBUG "%s: checksum error\n", dev->name);
			return ERROR;
		}
		rcv->state = PLIP_PK_DONE;

	case PLIP_PK_DONE:
		
		rcv->skb->protocol=plip_type_trans(rcv->skb, dev);
		netif_rx_ni(rcv->skb);
		dev->stats.rx_bytes += rcv->length.h;
		dev->stats.rx_packets++;
		rcv->skb = NULL;
		if (net_debug > 2)
			printk(KERN_DEBUG "%s: receive end\n", dev->name);

		
		write_data (dev, 0x00);
		spin_lock_irq(&nl->lock);
		if (snd->state != PLIP_PK_DONE) {
			nl->connection = PLIP_CN_SEND;
			spin_unlock_irq(&nl->lock);
			schedule_work(&nl->immediate);
			enable_parport_interrupts (dev);
			ENABLE(dev->irq);
			return OK;
		} else {
			nl->connection = PLIP_CN_NONE;
			spin_unlock_irq(&nl->lock);
			enable_parport_interrupts (dev);
			ENABLE(dev->irq);
			return OK;
		}
	}
	return OK;
}

static inline int
plip_send(unsigned short nibble_timeout, struct net_device *dev,
	  enum plip_nibble_state *ns_p, unsigned char data)
{
	unsigned char c0;
	unsigned int cx;

	switch (*ns_p) {
	case PLIP_NB_BEGIN:
		write_data (dev, data & 0x0f);
		*ns_p = PLIP_NB_1;

	case PLIP_NB_1:
		write_data (dev, 0x10 | (data & 0x0f));
		cx = nibble_timeout;
		while (1) {
			c0 = read_status(dev);
			if ((c0 & 0x80) == 0)
				break;
			if (--cx == 0)
				return TIMEOUT;
			udelay(PLIP_DELAY_UNIT);
		}
		write_data (dev, 0x10 | (data >> 4));
		*ns_p = PLIP_NB_2;

	case PLIP_NB_2:
		write_data (dev, (data >> 4));
		cx = nibble_timeout;
		while (1) {
			c0 = read_status(dev);
			if (c0 & 0x80)
				break;
			if (--cx == 0)
				return TIMEOUT;
			udelay(PLIP_DELAY_UNIT);
		}
		*ns_p = PLIP_NB_BEGIN;
		return OK;
	}
	return OK;
}

static int
plip_send_packet(struct net_device *dev, struct net_local *nl,
		 struct plip_local *snd, struct plip_local *rcv)
{
	unsigned short nibble_timeout = nl->nibble;
	unsigned char *lbuf;
	unsigned char c0;
	unsigned int cx;

	if (snd->skb == NULL || (lbuf = snd->skb->data) == NULL) {
		printk(KERN_DEBUG "%s: send skb lost\n", dev->name);
		snd->state = PLIP_PK_DONE;
		snd->skb = NULL;
		return ERROR;
	}

	switch (snd->state) {
	case PLIP_PK_TRIGGER:
		if ((read_status(dev) & 0xf8) != 0x80)
			return HS_TIMEOUT;

		
		write_data (dev, 0x08);
		cx = nl->trigger;
		while (1) {
			udelay(PLIP_DELAY_UNIT);
			spin_lock_irq(&nl->lock);
			if (nl->connection == PLIP_CN_RECEIVE) {
				spin_unlock_irq(&nl->lock);
				
				dev->stats.collisions++;
				return OK;
			}
			c0 = read_status(dev);
			if (c0 & 0x08) {
				spin_unlock_irq(&nl->lock);
				DISABLE(dev->irq);
				synchronize_irq(dev->irq);
				if (nl->connection == PLIP_CN_RECEIVE) {
					ENABLE(dev->irq);
					dev->stats.collisions++;
					return OK;
				}
				disable_parport_interrupts (dev);
				if (net_debug > 2)
					printk(KERN_DEBUG "%s: send start\n", dev->name);
				snd->state = PLIP_PK_LENGTH_LSB;
				snd->nibble = PLIP_NB_BEGIN;
				nl->timeout_count = 0;
				break;
			}
			spin_unlock_irq(&nl->lock);
			if (--cx == 0) {
				write_data (dev, 0x00);
				return HS_TIMEOUT;
			}
		}

	case PLIP_PK_LENGTH_LSB:
		if (plip_send(nibble_timeout, dev,
			      &snd->nibble, snd->length.b.lsb))
			return TIMEOUT;
		snd->state = PLIP_PK_LENGTH_MSB;

	case PLIP_PK_LENGTH_MSB:
		if (plip_send(nibble_timeout, dev,
			      &snd->nibble, snd->length.b.msb))
			return TIMEOUT;
		snd->state = PLIP_PK_DATA;
		snd->byte = 0;
		snd->checksum = 0;

	case PLIP_PK_DATA:
		do {
			if (plip_send(nibble_timeout, dev,
				      &snd->nibble, lbuf[snd->byte]))
				return TIMEOUT;
		} while (++snd->byte < snd->length.h);
		do {
			snd->checksum += lbuf[--snd->byte];
		} while (snd->byte);
		snd->state = PLIP_PK_CHECKSUM;

	case PLIP_PK_CHECKSUM:
		if (plip_send(nibble_timeout, dev,
			      &snd->nibble, snd->checksum))
			return TIMEOUT;

		dev->stats.tx_bytes += snd->skb->len;
		dev_kfree_skb(snd->skb);
		dev->stats.tx_packets++;
		snd->state = PLIP_PK_DONE;

	case PLIP_PK_DONE:
		
		write_data (dev, 0x00);
		snd->skb = NULL;
		if (net_debug > 2)
			printk(KERN_DEBUG "%s: send end\n", dev->name);
		nl->connection = PLIP_CN_CLOSING;
		nl->is_deferred = 1;
		schedule_delayed_work(&nl->deferred, 1);
		enable_parport_interrupts (dev);
		ENABLE(dev->irq);
		return OK;
	}
	return OK;
}

static int
plip_connection_close(struct net_device *dev, struct net_local *nl,
		      struct plip_local *snd, struct plip_local *rcv)
{
	spin_lock_irq(&nl->lock);
	if (nl->connection == PLIP_CN_CLOSING) {
		nl->connection = PLIP_CN_NONE;
		netif_wake_queue (dev);
	}
	spin_unlock_irq(&nl->lock);
	if (nl->should_relinquish) {
		nl->should_relinquish = nl->port_owner = 0;
		parport_release(nl->pardev);
	}
	return OK;
}

static int
plip_error(struct net_device *dev, struct net_local *nl,
	   struct plip_local *snd, struct plip_local *rcv)
{
	unsigned char status;

	status = read_status(dev);
	if ((status & 0xf8) == 0x80) {
		if (net_debug > 2)
			printk(KERN_DEBUG "%s: reset interface.\n", dev->name);
		nl->connection = PLIP_CN_NONE;
		nl->should_relinquish = 0;
		netif_start_queue (dev);
		enable_parport_interrupts (dev);
		ENABLE(dev->irq);
		netif_wake_queue (dev);
	} else {
		nl->is_deferred = 1;
		schedule_delayed_work(&nl->deferred, 1);
	}

	return OK;
}

static void
plip_interrupt(void *dev_id)
{
	struct net_device *dev = dev_id;
	struct net_local *nl;
	struct plip_local *rcv;
	unsigned char c0;
	unsigned long flags;

	nl = netdev_priv(dev);
	rcv = &nl->rcv_data;

	spin_lock_irqsave (&nl->lock, flags);

	c0 = read_status(dev);
	if ((c0 & 0xf8) != 0xc0) {
		if ((dev->irq != -1) && (net_debug > 1))
			printk(KERN_DEBUG "%s: spurious interrupt\n", dev->name);
		spin_unlock_irqrestore (&nl->lock, flags);
		return;
	}

	if (net_debug > 3)
		printk(KERN_DEBUG "%s: interrupt.\n", dev->name);

	switch (nl->connection) {
	case PLIP_CN_CLOSING:
		netif_wake_queue (dev);
	case PLIP_CN_NONE:
	case PLIP_CN_SEND:
		rcv->state = PLIP_PK_TRIGGER;
		nl->connection = PLIP_CN_RECEIVE;
		nl->timeout_count = 0;
		schedule_work(&nl->immediate);
		break;

	case PLIP_CN_RECEIVE:
		break;

	case PLIP_CN_ERROR:
		printk(KERN_ERR "%s: receive interrupt in error state\n", dev->name);
		break;
	}

	spin_unlock_irqrestore(&nl->lock, flags);
}

static int
plip_tx_packet(struct sk_buff *skb, struct net_device *dev)
{
	struct net_local *nl = netdev_priv(dev);
	struct plip_local *snd = &nl->snd_data;

	if (netif_queue_stopped(dev))
		return NETDEV_TX_BUSY;

	
	if (!nl->port_owner) {
		if (parport_claim(nl->pardev))
			return NETDEV_TX_BUSY;
		nl->port_owner = 1;
	}

	netif_stop_queue (dev);

	if (skb->len > dev->mtu + dev->hard_header_len) {
		printk(KERN_WARNING "%s: packet too big, %d.\n", dev->name, (int)skb->len);
		netif_start_queue (dev);
		return NETDEV_TX_BUSY;
	}

	if (net_debug > 2)
		printk(KERN_DEBUG "%s: send request\n", dev->name);

	spin_lock_irq(&nl->lock);
	snd->skb = skb;
	snd->length.h = skb->len;
	snd->state = PLIP_PK_TRIGGER;
	if (nl->connection == PLIP_CN_NONE) {
		nl->connection = PLIP_CN_SEND;
		nl->timeout_count = 0;
	}
	schedule_work(&nl->immediate);
	spin_unlock_irq(&nl->lock);

	return NETDEV_TX_OK;
}

static void
plip_rewrite_address(const struct net_device *dev, struct ethhdr *eth)
{
	const struct in_device *in_dev;

	rcu_read_lock();
	in_dev = __in_dev_get_rcu(dev);
	if (in_dev) {
		
		const struct in_ifaddr *ifa = in_dev->ifa_list;
		if (ifa) {
			memcpy(eth->h_source, dev->dev_addr, 6);
			memset(eth->h_dest, 0xfc, 2);
			memcpy(eth->h_dest+2, &ifa->ifa_address, 4);
		}
	}
	rcu_read_unlock();
}

static int
plip_hard_header(struct sk_buff *skb, struct net_device *dev,
		 unsigned short type, const void *daddr,
		 const void *saddr, unsigned len)
{
	int ret;

	ret = eth_header(skb, dev, type, daddr, saddr, len);
	if (ret >= 0)
		plip_rewrite_address (dev, (struct ethhdr *)skb->data);

	return ret;
}

static int plip_hard_header_cache(const struct neighbour *neigh,
				  struct hh_cache *hh, __be16 type)
{
	int ret;

	ret = eth_header_cache(neigh, hh, type);
	if (ret == 0) {
		struct ethhdr *eth;

		eth = (struct ethhdr*)(((u8*)hh->hh_data) +
				       HH_DATA_OFF(sizeof(*eth)));
		plip_rewrite_address (neigh->dev, eth);
	}

	return ret;
}

static int
plip_open(struct net_device *dev)
{
	struct net_local *nl = netdev_priv(dev);
	struct in_device *in_dev;

	
	if (!nl->port_owner) {
		if (parport_claim(nl->pardev)) return -EAGAIN;
		nl->port_owner = 1;
	}

	nl->should_relinquish = 0;

	
	write_data (dev, 0x00);

	
	enable_parport_interrupts (dev);
	if (dev->irq == -1)
	{
		atomic_set (&nl->kill_timer, 0);
		schedule_delayed_work(&nl->timer, 1);
	}

	
	nl->rcv_data.state = nl->snd_data.state = PLIP_PK_DONE;
	nl->rcv_data.skb = nl->snd_data.skb = NULL;
	nl->connection = PLIP_CN_NONE;
	nl->is_deferred = 0;


	in_dev=__in_dev_get_rtnl(dev);
	if (in_dev) {
		struct in_ifaddr *ifa=in_dev->ifa_list;
		if (ifa != NULL) {
			memcpy(dev->dev_addr+2, &ifa->ifa_local, 4);
		}
	}

	netif_start_queue (dev);

	return 0;
}

static int
plip_close(struct net_device *dev)
{
	struct net_local *nl = netdev_priv(dev);
	struct plip_local *snd = &nl->snd_data;
	struct plip_local *rcv = &nl->rcv_data;

	netif_stop_queue (dev);
	DISABLE(dev->irq);
	synchronize_irq(dev->irq);

	if (dev->irq == -1)
	{
		init_completion(&nl->killed_timer_cmp);
		atomic_set (&nl->kill_timer, 1);
		wait_for_completion(&nl->killed_timer_cmp);
	}

#ifdef NOTDEF
	outb(0x00, PAR_DATA(dev));
#endif
	nl->is_deferred = 0;
	nl->connection = PLIP_CN_NONE;
	if (nl->port_owner) {
		parport_release(nl->pardev);
		nl->port_owner = 0;
	}

	snd->state = PLIP_PK_DONE;
	if (snd->skb) {
		dev_kfree_skb(snd->skb);
		snd->skb = NULL;
	}
	rcv->state = PLIP_PK_DONE;
	if (rcv->skb) {
		kfree_skb(rcv->skb);
		rcv->skb = NULL;
	}

#ifdef NOTDEF
	
	outb(0x00, PAR_CONTROL(dev));
#endif
	return 0;
}

static int
plip_preempt(void *handle)
{
	struct net_device *dev = (struct net_device *)handle;
	struct net_local *nl = netdev_priv(dev);

	
	if (nl->connection != PLIP_CN_NONE) {
		nl->should_relinquish = 1;
		return 1;
	}

	nl->port_owner = 0;	
	return 0;
}

static void
plip_wakeup(void *handle)
{
	struct net_device *dev = (struct net_device *)handle;
	struct net_local *nl = netdev_priv(dev);

	if (nl->port_owner) {
		
		printk(KERN_DEBUG "%s: why am I being woken up?\n", dev->name);
		if (!parport_claim(nl->pardev))
			
			printk(KERN_DEBUG "%s: I'm broken.\n", dev->name);
		else
			return;
	}

	if (!(dev->flags & IFF_UP))
		
		return;

	if (!parport_claim(nl->pardev)) {
		nl->port_owner = 1;
		
		write_data (dev, 0x00);
	}
}

static int
plip_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct net_local *nl = netdev_priv(dev);
	struct plipconf *pc = (struct plipconf *) &rq->ifr_ifru;

	if (cmd != SIOCDEVPLIP)
		return -EOPNOTSUPP;

	switch(pc->pcmd) {
	case PLIP_GET_TIMEOUT:
		pc->trigger = nl->trigger;
		pc->nibble  = nl->nibble;
		break;
	case PLIP_SET_TIMEOUT:
		if(!capable(CAP_NET_ADMIN))
			return -EPERM;
		nl->trigger = pc->trigger;
		nl->nibble  = pc->nibble;
		break;
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

static int parport[PLIP_MAX] = { [0 ... PLIP_MAX-1] = -1 };
static int timid;

module_param_array(parport, int, NULL, 0);
module_param(timid, int, 0);
MODULE_PARM_DESC(parport, "List of parport device numbers to use by plip");

static struct net_device *dev_plip[PLIP_MAX] = { NULL, };

static inline int
plip_searchfor(int list[], int a)
{
	int i;
	for (i = 0; i < PLIP_MAX && list[i] != -1; i++) {
		if (list[i] == a) return 1;
	}
	return 0;
}

static void plip_attach (struct parport *port)
{
	static int unit;
	struct net_device *dev;
	struct net_local *nl;
	char name[IFNAMSIZ];

	if ((parport[0] == -1 && (!timid || !port->devices)) ||
	    plip_searchfor(parport, port->number)) {
		if (unit == PLIP_MAX) {
			printk(KERN_ERR "plip: too many devices\n");
			return;
		}

		sprintf(name, "plip%d", unit);
		dev = alloc_etherdev(sizeof(struct net_local));
		if (!dev)
			return;

		strcpy(dev->name, name);

		dev->irq = port->irq;
		dev->base_addr = port->base;
		if (port->irq == -1) {
			printk(KERN_INFO "plip: %s has no IRQ. Using IRQ-less mode,"
		                 "which is fairly inefficient!\n", port->name);
		}

		nl = netdev_priv(dev);
		nl->dev = dev;
		nl->pardev = parport_register_device(port, dev->name, plip_preempt,
						 plip_wakeup, plip_interrupt,
						 0, dev);

		if (!nl->pardev) {
			printk(KERN_ERR "%s: parport_register failed\n", name);
			goto err_free_dev;
		}

		plip_init_netdev(dev);

		if (register_netdev(dev)) {
			printk(KERN_ERR "%s: network register failed\n", name);
			goto err_parport_unregister;
		}

		printk(KERN_INFO "%s", version);
		if (dev->irq != -1)
			printk(KERN_INFO "%s: Parallel port at %#3lx, "
					 "using IRQ %d.\n",
				         dev->name, dev->base_addr, dev->irq);
		else
			printk(KERN_INFO "%s: Parallel port at %#3lx, "
					 "not using IRQ.\n",
					 dev->name, dev->base_addr);
		dev_plip[unit++] = dev;
	}
	return;

err_parport_unregister:
	parport_unregister_device(nl->pardev);
err_free_dev:
	free_netdev(dev);
}

static void plip_detach (struct parport *port)
{
	
}

static struct parport_driver plip_driver = {
	.name	= "plip",
	.attach = plip_attach,
	.detach = plip_detach
};

static void __exit plip_cleanup_module (void)
{
	struct net_device *dev;
	int i;

	parport_unregister_driver (&plip_driver);

	for (i=0; i < PLIP_MAX; i++) {
		if ((dev = dev_plip[i])) {
			struct net_local *nl = netdev_priv(dev);
			unregister_netdev(dev);
			if (nl->port_owner)
				parport_release(nl->pardev);
			parport_unregister_device(nl->pardev);
			free_netdev(dev);
			dev_plip[i] = NULL;
		}
	}
}

#ifndef MODULE

static int parport_ptr;

static int __init plip_setup(char *str)
{
	int ints[4];

	str = get_options(str, ARRAY_SIZE(ints), ints);

	
	if (!strncmp(str, "parport", 7)) {
		int n = simple_strtoul(str+7, NULL, 10);
		if (parport_ptr < PLIP_MAX)
			parport[parport_ptr++] = n;
		else
			printk(KERN_INFO "plip: too many ports, %s ignored.\n",
			       str);
	} else if (!strcmp(str, "timid")) {
		timid = 1;
	} else {
		if (ints[0] == 0 || ints[1] == 0) {
			
			parport[0] = -2;
		} else {
			printk(KERN_WARNING "warning: 'plip=0x%x' ignored\n",
			       ints[1]);
		}
	}
	return 1;
}

__setup("plip=", plip_setup);

#endif 

static int __init plip_init (void)
{
	if (parport[0] == -2)
		return 0;

	if (parport[0] != -1 && timid) {
		printk(KERN_WARNING "plip: warning, ignoring `timid' since specific ports given.\n");
		timid = 0;
	}

	if (parport_register_driver (&plip_driver)) {
		printk (KERN_WARNING "plip: couldn't register driver\n");
		return 1;
	}

	return 0;
}

module_init(plip_init);
module_exit(plip_cleanup_module);
MODULE_LICENSE("GPL");
