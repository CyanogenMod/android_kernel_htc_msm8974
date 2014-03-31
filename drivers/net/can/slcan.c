/*
 * slcan.c - serial line CAN interface driver (using tty line discipline)
 *
 * This file is derived from linux/drivers/net/slip/slip.c
 *
 * slip.c Authors  : Laurence Culhane <loz@holmes.demon.co.uk>
 *                   Fred N. van Kempen <waltje@uwalt.nl.mugnet.org>
 * slcan.c Author  : Oliver Hartkopp <socketcan@hartkopp.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307. You can also get it
 * at http://www.gnu.org/licenses/gpl.html
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/uaccess.h>
#include <linux/bitops.h>
#include <linux/string.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/can.h>

static __initdata const char banner[] =
	KERN_INFO "slcan: serial line CAN interface driver\n";

MODULE_ALIAS_LDISC(N_SLCAN);
MODULE_DESCRIPTION("serial line CAN interface");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Oliver Hartkopp <socketcan@hartkopp.net>");

#define SLCAN_MAGIC 0x53CA

static int maxdev = 10;		
module_param(maxdev, int, 0);
MODULE_PARM_DESC(maxdev, "Maximum number of slcan interfaces");

#define SLC_MTU (sizeof("T1111222281122334455667788EA5F\r")+1)

struct slcan {
	int			magic;

	
	struct tty_struct	*tty;		
	struct net_device	*dev;		
	spinlock_t		lock;

	
	unsigned char		rbuff[SLC_MTU];	
	int			rcount;         
	unsigned char		xbuff[SLC_MTU];	
	unsigned char		*xhead;         
	int			xleft;          

	unsigned long		flags;		
#define SLF_INUSE		0		
#define SLF_ERROR		1               
};

static struct net_device **slcan_devs;




static void slc_bump(struct slcan *sl)
{
	struct sk_buff *skb;
	struct can_frame cf;
	int i, dlc_pos, tmp;
	unsigned long ultmp;
	char cmd = sl->rbuff[0];

	if ((cmd != 't') && (cmd != 'T') && (cmd != 'r') && (cmd != 'R'))
		return;

	if (cmd & 0x20) 
		dlc_pos = 4; 
	else
		dlc_pos = 9; 

	if (!((sl->rbuff[dlc_pos] >= '0') && (sl->rbuff[dlc_pos] < '9')))
		return;

	cf.can_dlc = sl->rbuff[dlc_pos] - '0'; 

	sl->rbuff[dlc_pos] = 0; 

	if (strict_strtoul(sl->rbuff+1, 16, &ultmp))
		return;

	cf.can_id = ultmp;

	if (!(cmd & 0x20)) 
		cf.can_id |= CAN_EFF_FLAG;

	if ((cmd | 0x20) == 'r') 
		cf.can_id |= CAN_RTR_FLAG;

	*(u64 *) (&cf.data) = 0; 

	for (i = 0, dlc_pos++; i < cf.can_dlc; i++) {
		tmp = hex_to_bin(sl->rbuff[dlc_pos++]);
		if (tmp < 0)
			return;
		cf.data[i] = (tmp << 4);
		tmp = hex_to_bin(sl->rbuff[dlc_pos++]);
		if (tmp < 0)
			return;
		cf.data[i] |= tmp;
	}

	skb = dev_alloc_skb(sizeof(struct can_frame));
	if (!skb)
		return;

	skb->dev = sl->dev;
	skb->protocol = htons(ETH_P_CAN);
	skb->pkt_type = PACKET_BROADCAST;
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	memcpy(skb_put(skb, sizeof(struct can_frame)),
	       &cf, sizeof(struct can_frame));
	netif_rx_ni(skb);

	sl->dev->stats.rx_packets++;
	sl->dev->stats.rx_bytes += cf.can_dlc;
}

static void slcan_unesc(struct slcan *sl, unsigned char s)
{

	if ((s == '\r') || (s == '\a')) { 
		if (!test_and_clear_bit(SLF_ERROR, &sl->flags) &&
		    (sl->rcount > 4))  {
			slc_bump(sl);
		}
		sl->rcount = 0;
	} else {
		if (!test_bit(SLF_ERROR, &sl->flags))  {
			if (sl->rcount < SLC_MTU)  {
				sl->rbuff[sl->rcount++] = s;
				return;
			} else {
				sl->dev->stats.rx_over_errors++;
				set_bit(SLF_ERROR, &sl->flags);
			}
		}
	}
}


static void slc_encaps(struct slcan *sl, struct can_frame *cf)
{
	int actual, idx, i;
	char cmd;

	if (cf->can_id & CAN_RTR_FLAG)
		cmd = 'R'; 
	else
		cmd = 'T'; 

	if (cf->can_id & CAN_EFF_FLAG)
		sprintf(sl->xbuff, "%c%08X%d", cmd,
			cf->can_id & CAN_EFF_MASK, cf->can_dlc);
	else
		sprintf(sl->xbuff, "%c%03X%d", cmd | 0x20,
			cf->can_id & CAN_SFF_MASK, cf->can_dlc);

	idx = strlen(sl->xbuff);

	for (i = 0; i < cf->can_dlc; i++)
		sprintf(&sl->xbuff[idx + 2*i], "%02X", cf->data[i]);

	strcat(sl->xbuff, "\r"); 

	set_bit(TTY_DO_WRITE_WAKEUP, &sl->tty->flags);
	actual = sl->tty->ops->write(sl->tty, sl->xbuff, strlen(sl->xbuff));
	sl->xleft = strlen(sl->xbuff) - actual;
	sl->xhead = sl->xbuff + actual;
	sl->dev->stats.tx_bytes += cf->can_dlc;
}

static void slcan_write_wakeup(struct tty_struct *tty)
{
	int actual;
	struct slcan *sl = (struct slcan *) tty->disc_data;

	
	if (!sl || sl->magic != SLCAN_MAGIC || !netif_running(sl->dev))
		return;

	if (sl->xleft <= 0)  {
		sl->dev->stats.tx_packets++;
		clear_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
		netif_wake_queue(sl->dev);
		return;
	}

	actual = tty->ops->write(tty, sl->xhead, sl->xleft);
	sl->xleft -= actual;
	sl->xhead += actual;
}

static netdev_tx_t slc_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct slcan *sl = netdev_priv(dev);

	if (skb->len != sizeof(struct can_frame))
		goto out;

	spin_lock(&sl->lock);
	if (!netif_running(dev))  {
		spin_unlock(&sl->lock);
		printk(KERN_WARNING "%s: xmit: iface is down\n", dev->name);
		goto out;
	}
	if (sl->tty == NULL) {
		spin_unlock(&sl->lock);
		goto out;
	}

	netif_stop_queue(sl->dev);
	slc_encaps(sl, (struct can_frame *) skb->data); 
	spin_unlock(&sl->lock);

out:
	kfree_skb(skb);
	return NETDEV_TX_OK;
}



static int slc_close(struct net_device *dev)
{
	struct slcan *sl = netdev_priv(dev);

	spin_lock_bh(&sl->lock);
	if (sl->tty) {
		
		clear_bit(TTY_DO_WRITE_WAKEUP, &sl->tty->flags);
	}
	netif_stop_queue(dev);
	sl->rcount   = 0;
	sl->xleft    = 0;
	spin_unlock_bh(&sl->lock);

	return 0;
}

static int slc_open(struct net_device *dev)
{
	struct slcan *sl = netdev_priv(dev);

	if (sl->tty == NULL)
		return -ENODEV;

	sl->flags &= (1 << SLF_INUSE);
	netif_start_queue(dev);
	return 0;
}

static void slc_free_netdev(struct net_device *dev)
{
	int i = dev->base_addr;
	free_netdev(dev);
	slcan_devs[i] = NULL;
}

static const struct net_device_ops slc_netdev_ops = {
	.ndo_open               = slc_open,
	.ndo_stop               = slc_close,
	.ndo_start_xmit         = slc_xmit,
};

static void slc_setup(struct net_device *dev)
{
	dev->netdev_ops		= &slc_netdev_ops;
	dev->destructor		= slc_free_netdev;

	dev->hard_header_len	= 0;
	dev->addr_len		= 0;
	dev->tx_queue_len	= 10;

	dev->mtu		= sizeof(struct can_frame);
	dev->type		= ARPHRD_CAN;

	
	dev->flags		= IFF_NOARP;
	dev->features           = NETIF_F_HW_CSUM;
}



static void slcan_receive_buf(struct tty_struct *tty,
			      const unsigned char *cp, char *fp, int count)
{
	struct slcan *sl = (struct slcan *) tty->disc_data;

	if (!sl || sl->magic != SLCAN_MAGIC || !netif_running(sl->dev))
		return;

	
	while (count--) {
		if (fp && *fp++) {
			if (!test_and_set_bit(SLF_ERROR, &sl->flags))
				sl->dev->stats.rx_errors++;
			cp++;
			continue;
		}
		slcan_unesc(sl, *cp++);
	}
}


static void slc_sync(void)
{
	int i;
	struct net_device *dev;
	struct slcan	  *sl;

	for (i = 0; i < maxdev; i++) {
		dev = slcan_devs[i];
		if (dev == NULL)
			break;

		sl = netdev_priv(dev);
		if (sl->tty)
			continue;
		if (dev->flags & IFF_UP)
			dev_close(dev);
	}
}

static struct slcan *slc_alloc(dev_t line)
{
	int i;
	char name[IFNAMSIZ];
	struct net_device *dev = NULL;
	struct slcan       *sl;

	for (i = 0; i < maxdev; i++) {
		dev = slcan_devs[i];
		if (dev == NULL)
			break;

	}

	
	if (i >= maxdev)
		return NULL;

	sprintf(name, "slcan%d", i);
	dev = alloc_netdev(sizeof(*sl), name, slc_setup);
	if (!dev)
		return NULL;

	dev->base_addr  = i;
	sl = netdev_priv(dev);

	
	sl->magic = SLCAN_MAGIC;
	sl->dev	= dev;
	spin_lock_init(&sl->lock);
	slcan_devs[i] = dev;

	return sl;
}


static int slcan_open(struct tty_struct *tty)
{
	struct slcan *sl;
	int err;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (tty->ops->write == NULL)
		return -EOPNOTSUPP;

	rtnl_lock();

	
	slc_sync();

	sl = tty->disc_data;

	err = -EEXIST;
	
	if (sl && sl->magic == SLCAN_MAGIC)
		goto err_exit;

	
	err = -ENFILE;
	sl = slc_alloc(tty_devnum(tty));
	if (sl == NULL)
		goto err_exit;

	sl->tty = tty;
	tty->disc_data = sl;

	if (!test_bit(SLF_INUSE, &sl->flags)) {
		
		sl->rcount   = 0;
		sl->xleft    = 0;

		set_bit(SLF_INUSE, &sl->flags);

		err = register_netdevice(sl->dev);
		if (err)
			goto err_free_chan;
	}

	
	rtnl_unlock();
	tty->receive_room = 65536;	

	
	return 0;

err_free_chan:
	sl->tty = NULL;
	tty->disc_data = NULL;
	clear_bit(SLF_INUSE, &sl->flags);

err_exit:
	rtnl_unlock();

	
	return err;
}


static void slcan_close(struct tty_struct *tty)
{
	struct slcan *sl = (struct slcan *) tty->disc_data;

	
	if (!sl || sl->magic != SLCAN_MAGIC || sl->tty != tty)
		return;

	tty->disc_data = NULL;
	sl->tty = NULL;

	
	unregister_netdev(sl->dev);
	
}

static int slcan_hangup(struct tty_struct *tty)
{
	slcan_close(tty);
	return 0;
}

static int slcan_ioctl(struct tty_struct *tty, struct file *file,
		       unsigned int cmd, unsigned long arg)
{
	struct slcan *sl = (struct slcan *) tty->disc_data;
	unsigned int tmp;

	
	if (!sl || sl->magic != SLCAN_MAGIC)
		return -EINVAL;

	switch (cmd) {
	case SIOCGIFNAME:
		tmp = strlen(sl->dev->name) + 1;
		if (copy_to_user((void __user *)arg, sl->dev->name, tmp))
			return -EFAULT;
		return 0;

	case SIOCSIFHWADDR:
		return -EINVAL;

	default:
		return tty_mode_ioctl(tty, file, cmd, arg);
	}
}

static struct tty_ldisc_ops slc_ldisc = {
	.owner		= THIS_MODULE,
	.magic		= TTY_LDISC_MAGIC,
	.name		= "slcan",
	.open		= slcan_open,
	.close		= slcan_close,
	.hangup		= slcan_hangup,
	.ioctl		= slcan_ioctl,
	.receive_buf	= slcan_receive_buf,
	.write_wakeup	= slcan_write_wakeup,
};

static int __init slcan_init(void)
{
	int status;

	if (maxdev < 4)
		maxdev = 4; 

	printk(banner);
	printk(KERN_INFO "slcan: %d dynamic interface channels.\n", maxdev);

	slcan_devs = kzalloc(sizeof(struct net_device *)*maxdev, GFP_KERNEL);
	if (!slcan_devs)
		return -ENOMEM;

	
	status = tty_register_ldisc(N_SLCAN, &slc_ldisc);
	if (status)  {
		printk(KERN_ERR "slcan: can't register line discipline\n");
		kfree(slcan_devs);
	}
	return status;
}

static void __exit slcan_exit(void)
{
	int i;
	struct net_device *dev;
	struct slcan *sl;
	unsigned long timeout = jiffies + HZ;
	int busy = 0;

	if (slcan_devs == NULL)
		return;

	do {
		if (busy)
			msleep_interruptible(100);

		busy = 0;
		for (i = 0; i < maxdev; i++) {
			dev = slcan_devs[i];
			if (!dev)
				continue;
			sl = netdev_priv(dev);
			spin_lock_bh(&sl->lock);
			if (sl->tty) {
				busy++;
				tty_hangup(sl->tty);
			}
			spin_unlock_bh(&sl->lock);
		}
	} while (busy && time_before(jiffies, timeout));


	for (i = 0; i < maxdev; i++) {
		dev = slcan_devs[i];
		if (!dev)
			continue;
		slcan_devs[i] = NULL;

		sl = netdev_priv(dev);
		if (sl->tty) {
			printk(KERN_ERR "%s: tty discipline still running\n",
			       dev->name);
			
			dev->destructor = NULL;
		}

		unregister_netdev(dev);
	}

	kfree(slcan_devs);
	slcan_devs = NULL;

	i = tty_unregister_ldisc(N_SLCAN);
	if (i)
		printk(KERN_ERR "slcan: can't unregister ldisc (err %d)\n", i);
}

module_init(slcan_init);
module_exit(slcan_exit);
