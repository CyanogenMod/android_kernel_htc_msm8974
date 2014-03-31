
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>

#include <linux/uaccess.h>
#include <linux/bitops.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/lapb.h>
#include <linux/init.h>
#include <linux/rtnetlink.h>
#include <linux/compat.h>
#include <linux/slab.h>
#include <net/x25device.h>
#include "x25_asy.h"

static struct net_device **x25_asy_devs;
static int x25_asy_maxdev = SL_NRUNIT;

module_param(x25_asy_maxdev, int, 0);
MODULE_LICENSE("GPL");

static int x25_asy_esc(unsigned char *p, unsigned char *d, int len);
static void x25_asy_unesc(struct x25_asy *sl, unsigned char c);
static void x25_asy_setup(struct net_device *dev);

static struct x25_asy *x25_asy_alloc(void)
{
	struct net_device *dev = NULL;
	struct x25_asy *sl;
	int i;

	if (x25_asy_devs == NULL)
		return NULL;	

	for (i = 0; i < x25_asy_maxdev; i++) {
		dev = x25_asy_devs[i];

		
		if (dev == NULL)
			break;

		sl = netdev_priv(dev);
		
		if (!test_and_set_bit(SLF_INUSE, &sl->flags))
			return sl;
	}


	
	if (i >= x25_asy_maxdev)
		return NULL;

	
	if (!dev) {
		char name[IFNAMSIZ];
		sprintf(name, "x25asy%d", i);

		dev = alloc_netdev(sizeof(struct x25_asy),
				   name, x25_asy_setup);
		if (!dev)
			return NULL;

		
		sl = netdev_priv(dev);
		dev->base_addr    = i;

		
		if (register_netdev(dev) == 0) {
			
			set_bit(SLF_INUSE, &sl->flags);
			x25_asy_devs[i] = dev;
			return sl;
		} else {
			pr_warn("%s(): register_netdev() failure\n", __func__);
			free_netdev(dev);
		}
	}
	return NULL;
}


static void x25_asy_free(struct x25_asy *sl)
{
	
	kfree(sl->rbuff);
	sl->rbuff = NULL;
	kfree(sl->xbuff);
	sl->xbuff = NULL;

	if (!test_and_clear_bit(SLF_INUSE, &sl->flags))
		netdev_err(sl->dev, "x25_asy_free for already free unit\n");
}

static int x25_asy_change_mtu(struct net_device *dev, int newmtu)
{
	struct x25_asy *sl = netdev_priv(dev);
	unsigned char *xbuff, *rbuff;
	int len = 2 * newmtu;

	xbuff = kmalloc(len + 4, GFP_ATOMIC);
	rbuff = kmalloc(len + 4, GFP_ATOMIC);

	if (xbuff == NULL || rbuff == NULL) {
		netdev_warn(dev, "unable to grow X.25 buffers, MTU change cancelled\n");
		kfree(xbuff);
		kfree(rbuff);
		return -ENOMEM;
	}

	spin_lock_bh(&sl->lock);
	xbuff    = xchg(&sl->xbuff, xbuff);
	if (sl->xleft)  {
		if (sl->xleft <= len)  {
			memcpy(sl->xbuff, sl->xhead, sl->xleft);
		} else  {
			sl->xleft = 0;
			dev->stats.tx_dropped++;
		}
	}
	sl->xhead = sl->xbuff;

	rbuff	 = xchg(&sl->rbuff, rbuff);
	if (sl->rcount)  {
		if (sl->rcount <= len) {
			memcpy(sl->rbuff, rbuff, sl->rcount);
		} else  {
			sl->rcount = 0;
			dev->stats.rx_over_errors++;
			set_bit(SLF_ERROR, &sl->flags);
		}
	}

	dev->mtu    = newmtu;
	sl->buffsize = len;

	spin_unlock_bh(&sl->lock);

	kfree(xbuff);
	kfree(rbuff);
	return 0;
}



static inline void x25_asy_lock(struct x25_asy *sl)
{
	netif_stop_queue(sl->dev);
}



static inline void x25_asy_unlock(struct x25_asy *sl)
{
	netif_wake_queue(sl->dev);
}


static void x25_asy_bump(struct x25_asy *sl)
{
	struct net_device *dev = sl->dev;
	struct sk_buff *skb;
	int count;
	int err;

	count = sl->rcount;
	dev->stats.rx_bytes += count;

	skb = dev_alloc_skb(count+1);
	if (skb == NULL) {
		netdev_warn(sl->dev, "memory squeeze, dropping packet\n");
		dev->stats.rx_dropped++;
		return;
	}
	skb_push(skb, 1);	
	memcpy(skb_put(skb, count), sl->rbuff, count);
	skb->protocol = x25_type_trans(skb, sl->dev);
	err = lapb_data_received(skb->dev, skb);
	if (err != LAPB_OK) {
		kfree_skb(skb);
		printk(KERN_DEBUG "x25_asy: data received err - %d\n", err);
	} else {
		netif_rx(skb);
		dev->stats.rx_packets++;
	}
}

static void x25_asy_encaps(struct x25_asy *sl, unsigned char *icp, int len)
{
	unsigned char *p;
	int actual, count, mtu = sl->dev->mtu;

	if (len > mtu) {
		
		len = mtu;
		printk(KERN_DEBUG "%s: truncating oversized transmit packet!\n",
					sl->dev->name);
		sl->dev->stats.tx_dropped++;
		x25_asy_unlock(sl);
		return;
	}

	p = icp;
	count = x25_asy_esc(p, (unsigned char *) sl->xbuff, len);

	set_bit(TTY_DO_WRITE_WAKEUP, &sl->tty->flags);
	actual = sl->tty->ops->write(sl->tty, sl->xbuff, count);
	sl->xleft = count - actual;
	sl->xhead = sl->xbuff + actual;
	
	clear_bit(SLF_OUTWAIT, &sl->flags);	
}

static void x25_asy_write_wakeup(struct tty_struct *tty)
{
	int actual;
	struct x25_asy *sl = tty->disc_data;

	
	if (!sl || sl->magic != X25_ASY_MAGIC || !netif_running(sl->dev))
		return;

	if (sl->xleft <= 0) {
		sl->dev->stats.tx_packets++;
		clear_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
		x25_asy_unlock(sl);
		return;
	}

	actual = tty->ops->write(tty, sl->xhead, sl->xleft);
	sl->xleft -= actual;
	sl->xhead += actual;
}

static void x25_asy_timeout(struct net_device *dev)
{
	struct x25_asy *sl = netdev_priv(dev);

	spin_lock(&sl->lock);
	if (netif_queue_stopped(dev)) {
		netdev_warn(dev, "transmit timed out, %s?\n",
			    (tty_chars_in_buffer(sl->tty) || sl->xleft) ?
			    "bad line quality" : "driver error");
		sl->xleft = 0;
		clear_bit(TTY_DO_WRITE_WAKEUP, &sl->tty->flags);
		x25_asy_unlock(sl);
	}
	spin_unlock(&sl->lock);
}


static netdev_tx_t x25_asy_xmit(struct sk_buff *skb,
				      struct net_device *dev)
{
	struct x25_asy *sl = netdev_priv(dev);
	int err;

	if (!netif_running(sl->dev)) {
		netdev_err(dev, "xmit call when iface is down\n");
		kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	switch (skb->data[0]) {
	case X25_IFACE_DATA:
		break;
	case X25_IFACE_CONNECT: 
		err = lapb_connect_request(dev);
		if (err != LAPB_OK)
			netdev_err(dev, "lapb_connect_request error: %d\n",
				   err);
		kfree_skb(skb);
		return NETDEV_TX_OK;
	case X25_IFACE_DISCONNECT: 
		err = lapb_disconnect_request(dev);
		if (err != LAPB_OK)
			netdev_err(dev, "lapb_disconnect_request error: %d\n",
				   err);
	default:
		kfree_skb(skb);
		return NETDEV_TX_OK;
	}
	skb_pull(skb, 1);	

	err = lapb_data_request(dev, skb);
	if (err != LAPB_OK) {
		netdev_err(dev, "lapb_data_request error: %d\n", err);
		kfree_skb(skb);
		return NETDEV_TX_OK;
	}
	return NETDEV_TX_OK;
}




static int x25_asy_data_indication(struct net_device *dev, struct sk_buff *skb)
{
	return netif_rx(skb);
}


static void x25_asy_data_transmit(struct net_device *dev, struct sk_buff *skb)
{
	struct x25_asy *sl = netdev_priv(dev);

	spin_lock(&sl->lock);
	if (netif_queue_stopped(sl->dev) || sl->tty == NULL) {
		spin_unlock(&sl->lock);
		netdev_err(dev, "tbusy drop\n");
		kfree_skb(skb);
		return;
	}
	
	if (skb != NULL) {
		x25_asy_lock(sl);
		dev->stats.tx_bytes += skb->len;
		x25_asy_encaps(sl, skb->data, skb->len);
		dev_kfree_skb(skb);
	}
	spin_unlock(&sl->lock);
}


static void x25_asy_connected(struct net_device *dev, int reason)
{
	struct x25_asy *sl = netdev_priv(dev);
	struct sk_buff *skb;
	unsigned char *ptr;

	skb = dev_alloc_skb(1);
	if (skb == NULL) {
		netdev_err(dev, "out of memory\n");
		return;
	}

	ptr  = skb_put(skb, 1);
	*ptr = X25_IFACE_CONNECT;

	skb->protocol = x25_type_trans(skb, sl->dev);
	netif_rx(skb);
}

static void x25_asy_disconnected(struct net_device *dev, int reason)
{
	struct x25_asy *sl = netdev_priv(dev);
	struct sk_buff *skb;
	unsigned char *ptr;

	skb = dev_alloc_skb(1);
	if (skb == NULL) {
		netdev_err(dev, "out of memory\n");
		return;
	}

	ptr  = skb_put(skb, 1);
	*ptr = X25_IFACE_DISCONNECT;

	skb->protocol = x25_type_trans(skb, sl->dev);
	netif_rx(skb);
}

static const struct lapb_register_struct x25_asy_callbacks = {
	.connect_confirmation = x25_asy_connected,
	.connect_indication = x25_asy_connected,
	.disconnect_confirmation = x25_asy_disconnected,
	.disconnect_indication = x25_asy_disconnected,
	.data_indication = x25_asy_data_indication,
	.data_transmit = x25_asy_data_transmit,
};


static int x25_asy_open(struct net_device *dev)
{
	struct x25_asy *sl = netdev_priv(dev);
	unsigned long len;
	int err;

	if (sl->tty == NULL)
		return -ENODEV;


	len = dev->mtu * 2;

	sl->rbuff = kmalloc(len + 4, GFP_KERNEL);
	if (sl->rbuff == NULL)
		goto norbuff;
	sl->xbuff = kmalloc(len + 4, GFP_KERNEL);
	if (sl->xbuff == NULL)
		goto noxbuff;

	sl->buffsize = len;
	sl->rcount   = 0;
	sl->xleft    = 0;
	sl->flags   &= (1 << SLF_INUSE);      

	netif_start_queue(dev);

	err = lapb_register(dev, &x25_asy_callbacks);
	if (err == LAPB_OK)
		return 0;

	
	kfree(sl->xbuff);
noxbuff:
	kfree(sl->rbuff);
norbuff:
	return -ENOMEM;
}


static int x25_asy_close(struct net_device *dev)
{
	struct x25_asy *sl = netdev_priv(dev);

	spin_lock(&sl->lock);
	if (sl->tty)
		clear_bit(TTY_DO_WRITE_WAKEUP, &sl->tty->flags);

	netif_stop_queue(dev);
	sl->rcount = 0;
	sl->xleft  = 0;
	spin_unlock(&sl->lock);
	return 0;
}


static void x25_asy_receive_buf(struct tty_struct *tty,
				const unsigned char *cp, char *fp, int count)
{
	struct x25_asy *sl = tty->disc_data;

	if (!sl || sl->magic != X25_ASY_MAGIC || !netif_running(sl->dev))
		return;


	
	while (count--) {
		if (fp && *fp++) {
			if (!test_and_set_bit(SLF_ERROR, &sl->flags))
				sl->dev->stats.rx_errors++;
			cp++;
			continue;
		}
		x25_asy_unesc(sl, *cp++);
	}
}


static int x25_asy_open_tty(struct tty_struct *tty)
{
	struct x25_asy *sl = tty->disc_data;
	int err;

	if (tty->ops->write == NULL)
		return -EOPNOTSUPP;

	
	if (sl && sl->magic == X25_ASY_MAGIC)
		return -EEXIST;

	
	sl = x25_asy_alloc();
	if (sl == NULL)
		return -ENFILE;

	sl->tty = tty;
	tty->disc_data = sl;
	tty->receive_room = 65536;
	tty_driver_flush_buffer(tty);
	tty_ldisc_flush(tty);

	
	sl->dev->type = ARPHRD_X25;

	
	err = x25_asy_open(sl->dev);
	if (err)
		return err;
	
	return 0;
}


static void x25_asy_close_tty(struct tty_struct *tty)
{
	struct x25_asy *sl = tty->disc_data;
	int err;

	
	if (!sl || sl->magic != X25_ASY_MAGIC)
		return;

	rtnl_lock();
	if (sl->dev->flags & IFF_UP)
		dev_close(sl->dev);
	rtnl_unlock();

	err = lapb_unregister(sl->dev);
	if (err != LAPB_OK)
		pr_err("x25_asy_close: lapb_unregister error: %d\n",
		       err);

	tty->disc_data = NULL;
	sl->tty = NULL;
	x25_asy_free(sl);
}


static int x25_asy_esc(unsigned char *s, unsigned char *d, int len)
{
	unsigned char *ptr = d;
	unsigned char c;


	*ptr++ = X25_END;	


	while (len-- > 0) {
		switch (c = *s++) {
		case X25_END:
			*ptr++ = X25_ESC;
			*ptr++ = X25_ESCAPE(X25_END);
			break;
		case X25_ESC:
			*ptr++ = X25_ESC;
			*ptr++ = X25_ESCAPE(X25_ESC);
			break;
		default:
			*ptr++ = c;
			break;
		}
	}
	*ptr++ = X25_END;
	return ptr - d;
}

static void x25_asy_unesc(struct x25_asy *sl, unsigned char s)
{

	switch (s) {
	case X25_END:
		if (!test_and_clear_bit(SLF_ERROR, &sl->flags) &&
		    sl->rcount > 2)
			x25_asy_bump(sl);
		clear_bit(SLF_ESCAPE, &sl->flags);
		sl->rcount = 0;
		return;
	case X25_ESC:
		set_bit(SLF_ESCAPE, &sl->flags);
		return;
	case X25_ESCAPE(X25_ESC):
	case X25_ESCAPE(X25_END):
		if (test_and_clear_bit(SLF_ESCAPE, &sl->flags))
			s = X25_UNESCAPE(s);
		break;
	}
	if (!test_bit(SLF_ERROR, &sl->flags)) {
		if (sl->rcount < sl->buffsize) {
			sl->rbuff[sl->rcount++] = s;
			return;
		}
		sl->dev->stats.rx_over_errors++;
		set_bit(SLF_ERROR, &sl->flags);
	}
}


static int x25_asy_ioctl(struct tty_struct *tty, struct file *file,
			 unsigned int cmd,  unsigned long arg)
{
	struct x25_asy *sl = tty->disc_data;

	
	if (!sl || sl->magic != X25_ASY_MAGIC)
		return -EINVAL;

	switch (cmd) {
	case SIOCGIFNAME:
		if (copy_to_user((void __user *)arg, sl->dev->name,
					strlen(sl->dev->name) + 1))
			return -EFAULT;
		return 0;
	case SIOCSIFHWADDR:
		return -EINVAL;
	default:
		return tty_mode_ioctl(tty, file, cmd, arg);
	}
}

#ifdef CONFIG_COMPAT
static long x25_asy_compat_ioctl(struct tty_struct *tty, struct file *file,
			 unsigned int cmd,  unsigned long arg)
{
	switch (cmd) {
	case SIOCGIFNAME:
	case SIOCSIFHWADDR:
		return x25_asy_ioctl(tty, file, cmd,
				     (unsigned long)compat_ptr(arg));
	}

	return -ENOIOCTLCMD;
}
#endif

static int x25_asy_open_dev(struct net_device *dev)
{
	struct x25_asy *sl = netdev_priv(dev);
	if (sl->tty == NULL)
		return -ENODEV;
	return 0;
}

static const struct net_device_ops x25_asy_netdev_ops = {
	.ndo_open	= x25_asy_open_dev,
	.ndo_stop	= x25_asy_close,
	.ndo_start_xmit	= x25_asy_xmit,
	.ndo_tx_timeout	= x25_asy_timeout,
	.ndo_change_mtu	= x25_asy_change_mtu,
};

static void x25_asy_setup(struct net_device *dev)
{
	struct x25_asy *sl = netdev_priv(dev);

	sl->magic  = X25_ASY_MAGIC;
	sl->dev	   = dev;
	spin_lock_init(&sl->lock);
	set_bit(SLF_INUSE, &sl->flags);


	dev->mtu		= SL_MTU;
	dev->netdev_ops		= &x25_asy_netdev_ops;
	dev->watchdog_timeo	= HZ*20;
	dev->hard_header_len	= 0;
	dev->addr_len		= 0;
	dev->type		= ARPHRD_X25;
	dev->tx_queue_len	= 10;

	
	dev->flags		= IFF_NOARP;
}

static struct tty_ldisc_ops x25_ldisc = {
	.owner		= THIS_MODULE,
	.magic		= TTY_LDISC_MAGIC,
	.name		= "X.25",
	.open		= x25_asy_open_tty,
	.close		= x25_asy_close_tty,
	.ioctl		= x25_asy_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= x25_asy_compat_ioctl,
#endif
	.receive_buf	= x25_asy_receive_buf,
	.write_wakeup	= x25_asy_write_wakeup,
};

static int __init init_x25_asy(void)
{
	if (x25_asy_maxdev < 4)
		x25_asy_maxdev = 4; 

	pr_info("X.25 async: version 0.00 ALPHA (dynamic channels, max=%d)\n",
		x25_asy_maxdev);

	x25_asy_devs = kcalloc(x25_asy_maxdev, sizeof(struct net_device *),
				GFP_KERNEL);
	if (!x25_asy_devs)
		return -ENOMEM;

	return tty_register_ldisc(N_X25, &x25_ldisc);
}


static void __exit exit_x25_asy(void)
{
	struct net_device *dev;
	int i;

	for (i = 0; i < x25_asy_maxdev; i++) {
		dev = x25_asy_devs[i];
		if (dev) {
			struct x25_asy *sl = netdev_priv(dev);

			spin_lock_bh(&sl->lock);
			if (sl->tty)
				tty_hangup(sl->tty);

			spin_unlock_bh(&sl->lock);
			unregister_netdev(dev);
			free_netdev(dev);
		}
	}

	kfree(x25_asy_devs);
	tty_unregister_ldisc(N_X25);
}

module_init(init_x25_asy);
module_exit(exit_x25_asy);
