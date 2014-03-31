/*
 * Intel Wireless WiMAX Connection 2400m
 * Glue with the networking stack
 *
 *
 * Copyright (C) 2007 Intel Corporation <linux-wimax@intel.com>
 * Yanir Lubetkin <yanirx.lubetkin@intel.com>
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 *
 * This implements an ethernet device for the i2400m.
 *
 * We fake being an ethernet device to simplify the support from user
 * space and from the other side. The world is (sadly) configured to
 * take in only Ethernet devices...
 *
 * Because of this, when using firmwares <= v1.3, there is an
 * copy-each-rxed-packet overhead on the RX path. Each IP packet has
 * to be reallocated to add an ethernet header (as there is no space
 * in what we get from the device). This is a known drawback and
 * firmwares >= 1.4 add header space that can be used to insert the
 * ethernet header without having to reallocate and copy.
 *
 * TX error handling is tricky; because we have to FIFO/queue the
 * buffers for transmission (as the hardware likes it aggregated), we
 * just give the skb to the TX subsystem and by the time it is
 * transmitted, we have long forgotten about it. So we just don't care
 * too much about it.
 *
 * Note that when the device is in idle mode with the basestation, we
 * need to negotiate coming back up online. That involves negotiation
 * and possible user space interaction. Thus, we defer to a workqueue
 * to do all that. By default, we only queue a single packet and drop
 * the rest, as potentially the time to go back from idle to normal is
 * long.
 *
 * ROADMAP
 *
 * i2400m_open         Called on ifconfig up
 * i2400m_stop         Called on ifconfig down
 *
 * i2400m_hard_start_xmit Called by the network stack to send a packet
 *   i2400m_net_wake_tx	  Wake up device from basestation-IDLE & TX
 *     i2400m_wake_tx_work
 *       i2400m_cmd_exit_idle
 *       i2400m_tx
 *   i2400m_net_tx        TX a data frame
 *     i2400m_tx
 *
 * i2400m_change_mtu      Called on ifconfig mtu XXX
 *
 * i2400m_tx_timeout      Called when the device times out
 *
 * i2400m_net_rx          Called by the RX code when a data frame is
 *                        available (firmware <= 1.3)
 * i2400m_net_erx         Called by the RX code when a data frame is
 *                        available (firmware >= 1.4).
 * i2400m_netdev_setup    Called to setup all the netdev stuff from
 *                        alloc_netdev.
 */
#include <linux/if_arp.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/export.h>
#include "i2400m.h"


#define D_SUBMODULE netdev
#include "debug-levels.h"

enum {
	I2400M_TX_TIMEOUT = 21 * HZ,
	I2400M_TX_QLEN = 20,
};


static
int i2400m_open(struct net_device *net_dev)
{
	int result;
	struct i2400m *i2400m = net_dev_to_i2400m(net_dev);
	struct device *dev = i2400m_dev(i2400m);

	d_fnstart(3, dev, "(net_dev %p [i2400m %p])\n", net_dev, i2400m);
	
	mutex_lock(&i2400m->init_mutex);
	if (i2400m->updown)
		result = 0;
	else
		result = -EBUSY;
	mutex_unlock(&i2400m->init_mutex);
	d_fnend(3, dev, "(net_dev %p [i2400m %p]) = %d\n",
		net_dev, i2400m, result);
	return result;
}


static
int i2400m_stop(struct net_device *net_dev)
{
	struct i2400m *i2400m = net_dev_to_i2400m(net_dev);
	struct device *dev = i2400m_dev(i2400m);

	d_fnstart(3, dev, "(net_dev %p [i2400m %p])\n", net_dev, i2400m);
	i2400m_net_wake_stop(i2400m);
	d_fnend(3, dev, "(net_dev %p [i2400m %p]) = 0\n", net_dev, i2400m);
	return 0;
}


void i2400m_wake_tx_work(struct work_struct *ws)
{
	int result;
	struct i2400m *i2400m = container_of(ws, struct i2400m, wake_tx_ws);
	struct net_device *net_dev = i2400m->wimax_dev.net_dev;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *skb = i2400m->wake_tx_skb;
	unsigned long flags;

	spin_lock_irqsave(&i2400m->tx_lock, flags);
	skb = i2400m->wake_tx_skb;
	i2400m->wake_tx_skb = NULL;
	spin_unlock_irqrestore(&i2400m->tx_lock, flags);

	d_fnstart(3, dev, "(ws %p i2400m %p skb %p)\n", ws, i2400m, skb);
	result = -EINVAL;
	if (skb == NULL) {
		dev_err(dev, "WAKE&TX: skb disappeared!\n");
		goto out_put;
	}
	if (unlikely(!netif_carrier_ok(net_dev)))
		goto out_kfree;
	result = i2400m_cmd_exit_idle(i2400m);
	if (result == -EILSEQ)
		result = 0;
	if (result < 0) {
		dev_err(dev, "WAKE&TX: device didn't get out of idle: "
			"%d - resetting\n", result);
		i2400m_reset(i2400m, I2400M_RT_BUS);
		goto error;
	}
	result = wait_event_timeout(i2400m->state_wq,
				    i2400m->state != I2400M_SS_IDLE,
				    net_dev->watchdog_timeo - HZ/2);
	if (result == 0)
		result = -ETIMEDOUT;
	if (result < 0) {
		dev_err(dev, "WAKE&TX: error waiting for device to exit IDLE: "
			"%d - resetting\n", result);
		i2400m_reset(i2400m, I2400M_RT_BUS);
		goto error;
	}
	msleep(20);	
	result = i2400m_tx(i2400m, skb->data, skb->len, I2400M_PT_DATA);
error:
	netif_wake_queue(net_dev);
out_kfree:
	kfree_skb(skb);	
out_put:
	i2400m_put(i2400m);
	d_fnend(3, dev, "(ws %p i2400m %p skb %p) = void [%d]\n",
		ws, i2400m, skb, result);
}


static
void i2400m_tx_prep_header(struct sk_buff *skb)
{
	struct i2400m_pl_data_hdr *pl_hdr;
	skb_pull(skb, ETH_HLEN);
	pl_hdr = (struct i2400m_pl_data_hdr *) skb_push(skb, sizeof(*pl_hdr));
	pl_hdr->reserved = 0;
}



void i2400m_net_wake_stop(struct i2400m *i2400m)
{
	struct device *dev = i2400m_dev(i2400m);

	d_fnstart(3, dev, "(i2400m %p)\n", i2400m);
	if (cancel_work_sync(&i2400m->wake_tx_ws) == 0
	    && i2400m->wake_tx_skb != NULL) {
		unsigned long flags;
		struct sk_buff *wake_tx_skb;
		spin_lock_irqsave(&i2400m->tx_lock, flags);
		wake_tx_skb = i2400m->wake_tx_skb;	
		i2400m->wake_tx_skb = NULL;	
		spin_unlock_irqrestore(&i2400m->tx_lock, flags);
		i2400m_put(i2400m);
		kfree_skb(wake_tx_skb);
	}
	d_fnend(3, dev, "(i2400m %p) = void\n", i2400m);
}


static
int i2400m_net_wake_tx(struct i2400m *i2400m, struct net_device *net_dev,
		       struct sk_buff *skb)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	unsigned long flags;

	d_fnstart(3, dev, "(skb %p net_dev %p)\n", skb, net_dev);
	if (net_ratelimit()) {
		d_printf(3, dev, "WAKE&NETTX: "
			 "skb %p sending %d bytes to radio\n",
			 skb, skb->len);
		d_dump(4, dev, skb->data, skb->len);
	}
	result = 0;
	spin_lock_irqsave(&i2400m->tx_lock, flags);
	if (!work_pending(&i2400m->wake_tx_ws)) {
		netif_stop_queue(net_dev);
		i2400m_get(i2400m);
		i2400m->wake_tx_skb = skb_get(skb);	
		i2400m_tx_prep_header(skb);
		result = schedule_work(&i2400m->wake_tx_ws);
		WARN_ON(result == 0);
	}
	spin_unlock_irqrestore(&i2400m->tx_lock, flags);
	if (result == 0) {
		if (net_ratelimit())
			d_printf(1, dev, "NETTX: device exiting idle, "
				 "dropping skb %p, queue running %d\n",
				 skb, netif_queue_stopped(net_dev));
		result = -EBUSY;
	}
	d_fnend(3, dev, "(skb %p net_dev %p) = %d\n", skb, net_dev, result);
	return result;
}


static
int i2400m_net_tx(struct i2400m *i2400m, struct net_device *net_dev,
		  struct sk_buff *skb)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);

	d_fnstart(3, dev, "(i2400m %p net_dev %p skb %p)\n",
		  i2400m, net_dev, skb);
	
	net_dev->trans_start = jiffies;
	i2400m_tx_prep_header(skb);
	d_printf(3, dev, "NETTX: skb %p sending %d bytes to radio\n",
		 skb, skb->len);
	d_dump(4, dev, skb->data, skb->len);
	result = i2400m_tx(i2400m, skb->data, skb->len, I2400M_PT_DATA);
	d_fnend(3, dev, "(i2400m %p net_dev %p skb %p) = %d\n",
		i2400m, net_dev, skb, result);
	return result;
}


static
netdev_tx_t i2400m_hard_start_xmit(struct sk_buff *skb,
					 struct net_device *net_dev)
{
	struct i2400m *i2400m = net_dev_to_i2400m(net_dev);
	struct device *dev = i2400m_dev(i2400m);
	int result = -1;

	d_fnstart(3, dev, "(skb %p net_dev %p)\n", skb, net_dev);

	if (skb_header_cloned(skb) && 
	    pskb_expand_head(skb, 0, 0, GFP_ATOMIC))
		goto drop;

	if (i2400m->state == I2400M_SS_IDLE)
		result = i2400m_net_wake_tx(i2400m, net_dev, skb);
	else
		result = i2400m_net_tx(i2400m, net_dev, skb);
	if (result <  0) {
drop:
		net_dev->stats.tx_dropped++;
	} else {
		net_dev->stats.tx_packets++;
		net_dev->stats.tx_bytes += skb->len;
	}
	dev_kfree_skb(skb);
	d_fnend(3, dev, "(skb %p net_dev %p) = %d\n", skb, net_dev, result);
	return NETDEV_TX_OK;
}


static
int i2400m_change_mtu(struct net_device *net_dev, int new_mtu)
{
	int result;
	struct i2400m *i2400m = net_dev_to_i2400m(net_dev);
	struct device *dev = i2400m_dev(i2400m);

	if (new_mtu >= I2400M_MAX_MTU) {
		dev_err(dev, "Cannot change MTU to %d (max is %d)\n",
			new_mtu, I2400M_MAX_MTU);
		result = -EINVAL;
	} else {
		net_dev->mtu = new_mtu;
		result = 0;
	}
	return result;
}


static
void i2400m_tx_timeout(struct net_device *net_dev)
{
	net_dev->stats.tx_errors++;
}


static
void i2400m_rx_fake_eth_header(struct net_device *net_dev,
			       void *_eth_hdr, __be16 protocol)
{
	struct i2400m *i2400m = net_dev_to_i2400m(net_dev);
	struct ethhdr *eth_hdr = _eth_hdr;

	memcpy(eth_hdr->h_dest, net_dev->dev_addr, sizeof(eth_hdr->h_dest));
	memcpy(eth_hdr->h_source, i2400m->src_mac_addr,
	       sizeof(eth_hdr->h_source));
	eth_hdr->h_proto = protocol;
}


void i2400m_net_rx(struct i2400m *i2400m, struct sk_buff *skb_rx,
		   unsigned i, const void *buf, int buf_len)
{
	struct net_device *net_dev = i2400m->wimax_dev.net_dev;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *skb;

	d_fnstart(2, dev, "(i2400m %p buf %p buf_len %d)\n",
		  i2400m, buf, buf_len);
	if (i) {
		skb = skb_get(skb_rx);
		d_printf(2, dev, "RX: reusing first payload skb %p\n", skb);
		skb_pull(skb, buf - (void *) skb->data);
		skb_trim(skb, (void *) skb_end_pointer(skb) - buf);
	} else {
		skb = __netdev_alloc_skb(net_dev, buf_len, GFP_KERNEL);
		if (skb == NULL) {
			dev_err(dev, "NETRX: no memory to realloc skb\n");
			net_dev->stats.rx_dropped++;
			goto error_skb_realloc;
		}
		memcpy(skb_put(skb, buf_len), buf, buf_len);
	}
	i2400m_rx_fake_eth_header(i2400m->wimax_dev.net_dev,
				  skb->data - ETH_HLEN,
				  cpu_to_be16(ETH_P_IP));
	skb_set_mac_header(skb, -ETH_HLEN);
	skb->dev = i2400m->wimax_dev.net_dev;
	skb->protocol = htons(ETH_P_IP);
	net_dev->stats.rx_packets++;
	net_dev->stats.rx_bytes += buf_len;
	d_printf(3, dev, "NETRX: receiving %d bytes to network stack\n",
		buf_len);
	d_dump(4, dev, buf, buf_len);
	netif_rx_ni(skb);	
error_skb_realloc:
	d_fnend(2, dev, "(i2400m %p buf %p buf_len %d) = void\n",
		i2400m, buf, buf_len);
}


void i2400m_net_erx(struct i2400m *i2400m, struct sk_buff *skb,
		    enum i2400m_cs cs)
{
	struct net_device *net_dev = i2400m->wimax_dev.net_dev;
	struct device *dev = i2400m_dev(i2400m);
	int protocol;

	d_fnstart(2, dev, "(i2400m %p skb %p [%u] cs %d)\n",
		  i2400m, skb, skb->len, cs);
	switch(cs) {
	case I2400M_CS_IPV4_0:
	case I2400M_CS_IPV4:
		protocol = ETH_P_IP;
		i2400m_rx_fake_eth_header(i2400m->wimax_dev.net_dev,
					  skb->data - ETH_HLEN,
					  cpu_to_be16(ETH_P_IP));
		skb_set_mac_header(skb, -ETH_HLEN);
		skb->dev = i2400m->wimax_dev.net_dev;
		skb->protocol = htons(ETH_P_IP);
		net_dev->stats.rx_packets++;
		net_dev->stats.rx_bytes += skb->len;
		break;
	default:
		dev_err(dev, "ERX: BUG? CS type %u unsupported\n", cs);
		goto error;

	}
	d_printf(3, dev, "ERX: receiving %d bytes to the network stack\n",
		 skb->len);
	d_dump(4, dev, skb->data, skb->len);
	netif_rx_ni(skb);	
error:
	d_fnend(2, dev, "(i2400m %p skb %p [%u] cs %d) = void\n",
		i2400m, skb, skb->len, cs);
}

static const struct net_device_ops i2400m_netdev_ops = {
	.ndo_open = i2400m_open,
	.ndo_stop = i2400m_stop,
	.ndo_start_xmit = i2400m_hard_start_xmit,
	.ndo_tx_timeout = i2400m_tx_timeout,
	.ndo_change_mtu = i2400m_change_mtu,
};

static void i2400m_get_drvinfo(struct net_device *net_dev,
			       struct ethtool_drvinfo *info)
{
	struct i2400m *i2400m = net_dev_to_i2400m(net_dev);

	strncpy(info->driver, KBUILD_MODNAME, sizeof(info->driver) - 1);
	strncpy(info->fw_version,
	        i2400m->fw_name ? : "", sizeof(info->fw_version) - 1);
	if (net_dev->dev.parent)
		strncpy(info->bus_info, dev_name(net_dev->dev.parent),
			sizeof(info->bus_info) - 1);
}

static const struct ethtool_ops i2400m_ethtool_ops = {
	.get_drvinfo = i2400m_get_drvinfo,
	.get_link = ethtool_op_get_link,
};

void i2400m_netdev_setup(struct net_device *net_dev)
{
	d_fnstart(3, NULL, "(net_dev %p)\n", net_dev);
	ether_setup(net_dev);
	net_dev->mtu = I2400M_MAX_MTU;
	net_dev->tx_queue_len = I2400M_TX_QLEN;
	net_dev->features =
		  NETIF_F_VLAN_CHALLENGED
		| NETIF_F_HIGHDMA;
	net_dev->flags =
		IFF_NOARP		
		& (~IFF_BROADCAST	
		   & ~IFF_MULTICAST);
	net_dev->watchdog_timeo = I2400M_TX_TIMEOUT;
	net_dev->netdev_ops = &i2400m_netdev_ops;
	net_dev->ethtool_ops = &i2400m_ethtool_ops;
	d_fnend(3, NULL, "(net_dev %p) = void\n", net_dev);
}
EXPORT_SYMBOL_GPL(i2400m_netdev_setup);

