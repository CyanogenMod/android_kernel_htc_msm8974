/*****************************************************************************
*
* Filename:      kingsun-sir.c
* Version:       0.1.1
* Description:   Irda KingSun/DonShine USB Dongle
* Status:        Experimental
* Author:        Alex Villacís Lasso <a_villacis@palosanto.com>
*
*  	Based on stir4200 and mcs7780 drivers, with (strange?) differences
*
*	This program is free software; you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation; either version 2 of the License.
*
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License
*	along with this program; if not, write to the Free Software
*	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
*****************************************************************************/


#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/device.h>
#include <linux/crc32.h>

#include <asm/unaligned.h>
#include <asm/byteorder.h>
#include <asm/uaccess.h>

#include <net/irda/irda.h>
#include <net/irda/wrapper.h>
#include <net/irda/crc.h>

#define KING_VENDOR_ID 0x07c0
#define KING_PRODUCT_ID 0x4200

static struct usb_device_id dongles[] = {
    
    { USB_DEVICE(KING_VENDOR_ID, KING_PRODUCT_ID) },
    { }
};

MODULE_DEVICE_TABLE(usb, dongles);

#define KINGSUN_MTT 0x07

#define KINGSUN_FIFO_SIZE		4096
#define KINGSUN_EP_IN			0
#define KINGSUN_EP_OUT			1

struct kingsun_cb {
	struct usb_device *usbdev;      
	struct net_device *netdev;      
	struct irlap_cb   *irlap;       

	struct qos_info   qos;

	__u8		  *in_buf;	
	__u8		  *out_buf;	
	__u8		  max_rx;	
	__u8		  max_tx;	

	iobuff_t  	  rx_buff;	
	struct timeval	  rx_time;
	spinlock_t lock;
	int receiving;

	__u8 ep_in;
	__u8 ep_out;

	struct urb	 *tx_urb;
	struct urb	 *rx_urb;
};

static void kingsun_send_irq(struct urb *urb)
{
	struct kingsun_cb *kingsun = urb->context;
	struct net_device *netdev = kingsun->netdev;

	
	if (!netif_running(kingsun->netdev)) {
		err("kingsun_send_irq: Network not running!");
		return;
	}

	
	if (urb->status != 0) {
		err("kingsun_send_irq: urb asynchronously failed - %d",
		    urb->status);
	}
	netif_wake_queue(netdev);
}

static netdev_tx_t kingsun_hard_xmit(struct sk_buff *skb,
					   struct net_device *netdev)
{
	struct kingsun_cb *kingsun;
	int wraplen;
	int ret = 0;

	netif_stop_queue(netdev);

	
	SKB_LINEAR_ASSERT(skb);

	kingsun = netdev_priv(netdev);

	spin_lock(&kingsun->lock);

	
	wraplen = async_wrap_skb(skb,
		kingsun->out_buf,
		KINGSUN_FIFO_SIZE);

	
	usb_fill_int_urb(kingsun->tx_urb, kingsun->usbdev,
		usb_sndintpipe(kingsun->usbdev, kingsun->ep_out),
		kingsun->out_buf, wraplen, kingsun_send_irq,
		kingsun, 1);

	if ((ret = usb_submit_urb(kingsun->tx_urb, GFP_ATOMIC))) {
		err("kingsun_hard_xmit: failed tx_urb submit: %d", ret);
		switch (ret) {
		case -ENODEV:
		case -EPIPE:
			break;
		default:
			netdev->stats.tx_errors++;
			netif_start_queue(netdev);
		}
	} else {
		netdev->stats.tx_packets++;
		netdev->stats.tx_bytes += skb->len;
	}

	dev_kfree_skb(skb);
	spin_unlock(&kingsun->lock);

	return NETDEV_TX_OK;
}

static void kingsun_rcv_irq(struct urb *urb)
{
	struct kingsun_cb *kingsun = urb->context;
	int ret;

	
	if (!netif_running(kingsun->netdev)) {
		kingsun->receiving = 0;
		return;
	}

	
	if (urb->status != 0) {
		err("kingsun_rcv_irq: urb asynchronously failed - %d",
		    urb->status);
		kingsun->receiving = 0;
		return;
	}

	if (urb->actual_length == kingsun->max_rx) {
		__u8 *bytes = urb->transfer_buffer;
		int i;

		if (bytes[0] >= 1 && bytes[0] < kingsun->max_rx) {
			for (i = 1; i <= bytes[0]; i++) {
				async_unwrap_char(kingsun->netdev,
						  &kingsun->netdev->stats,
						  &kingsun->rx_buff, bytes[i]);
			}
			do_gettimeofday(&kingsun->rx_time);
			kingsun->receiving =
				(kingsun->rx_buff.state != OUTSIDE_FRAME)
				? 1 : 0;
		}
	} else if (urb->actual_length > 0) {
		err("%s(): Unexpected response length, expected %d got %d",
		    __func__, kingsun->max_rx, urb->actual_length);
	}
	
	ret = usb_submit_urb(urb, GFP_ATOMIC);
}

static int kingsun_net_open(struct net_device *netdev)
{
	struct kingsun_cb *kingsun = netdev_priv(netdev);
	int err = -ENOMEM;
	char hwname[16];

	
	kingsun->receiving = 0;

	
	kingsun->rx_buff.in_frame = FALSE;
	kingsun->rx_buff.state = OUTSIDE_FRAME;
	kingsun->rx_buff.truesize = IRDA_SKB_MAX_MTU;
	kingsun->rx_buff.skb = dev_alloc_skb(IRDA_SKB_MAX_MTU);
	if (!kingsun->rx_buff.skb)
		goto free_mem;

	skb_reserve(kingsun->rx_buff.skb, 1);
	kingsun->rx_buff.head = kingsun->rx_buff.skb->data;
	do_gettimeofday(&kingsun->rx_time);

	kingsun->rx_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!kingsun->rx_urb)
		goto free_mem;

	kingsun->tx_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!kingsun->tx_urb)
		goto free_mem;

	sprintf(hwname, "usb#%d", kingsun->usbdev->devnum);
	kingsun->irlap = irlap_open(netdev, &kingsun->qos, hwname);
	if (!kingsun->irlap) {
		err("kingsun-sir: irlap_open failed");
		goto free_mem;
	}

	
	usb_fill_int_urb(kingsun->rx_urb, kingsun->usbdev,
			  usb_rcvintpipe(kingsun->usbdev, kingsun->ep_in),
			  kingsun->in_buf, kingsun->max_rx,
			  kingsun_rcv_irq, kingsun, 1);
	kingsun->rx_urb->status = 0;
	err = usb_submit_urb(kingsun->rx_urb, GFP_KERNEL);
	if (err) {
		err("kingsun-sir: first urb-submit failed: %d", err);
		goto close_irlap;
	}

	netif_start_queue(netdev);


	return 0;

 close_irlap:
	irlap_close(kingsun->irlap);
 free_mem:
	if (kingsun->tx_urb) {
		usb_free_urb(kingsun->tx_urb);
		kingsun->tx_urb = NULL;
	}
	if (kingsun->rx_urb) {
		usb_free_urb(kingsun->rx_urb);
		kingsun->rx_urb = NULL;
	}
	if (kingsun->rx_buff.skb) {
		kfree_skb(kingsun->rx_buff.skb);
		kingsun->rx_buff.skb = NULL;
		kingsun->rx_buff.head = NULL;
	}
	return err;
}

static int kingsun_net_close(struct net_device *netdev)
{
	struct kingsun_cb *kingsun = netdev_priv(netdev);

	
	netif_stop_queue(netdev);

	
	usb_kill_urb(kingsun->tx_urb);
	usb_kill_urb(kingsun->rx_urb);

	usb_free_urb(kingsun->tx_urb);
	usb_free_urb(kingsun->rx_urb);

	kingsun->tx_urb = NULL;
	kingsun->rx_urb = NULL;

	kfree_skb(kingsun->rx_buff.skb);
	kingsun->rx_buff.skb = NULL;
	kingsun->rx_buff.head = NULL;
	kingsun->rx_buff.in_frame = FALSE;
	kingsun->rx_buff.state = OUTSIDE_FRAME;
	kingsun->receiving = 0;

	
	if (kingsun->irlap)
		irlap_close(kingsun->irlap);

	kingsun->irlap = NULL;

	return 0;
}

static int kingsun_net_ioctl(struct net_device *netdev, struct ifreq *rq,
			     int cmd)
{
	struct if_irda_req *irq = (struct if_irda_req *) rq;
	struct kingsun_cb *kingsun = netdev_priv(netdev);
	int ret = 0;

	switch (cmd) {
	case SIOCSBANDWIDTH: 
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		
		if (netif_device_present(kingsun->netdev))
			
			ret = -EOPNOTSUPP;
		break;

	case SIOCSMEDIABUSY: 
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		
		if (netif_running(kingsun->netdev))
			irda_device_set_media_busy(kingsun->netdev, TRUE);
		break;

	case SIOCGRECEIVING:
		
		irq->ifr_receiving = kingsun->receiving;
		break;

	default:
		ret = -EOPNOTSUPP;
	}

	return ret;
}

static const struct net_device_ops kingsun_ops = {
	.ndo_start_xmit	     = kingsun_hard_xmit,
	.ndo_open            = kingsun_net_open,
	.ndo_stop            = kingsun_net_close,
	.ndo_do_ioctl        = kingsun_net_ioctl,
};

static int kingsun_probe(struct usb_interface *intf,
		      const struct usb_device_id *id)
{
	struct usb_host_interface *interface;
	struct usb_endpoint_descriptor *endpoint;

	struct usb_device *dev = interface_to_usbdev(intf);
	struct kingsun_cb *kingsun = NULL;
	struct net_device *net = NULL;
	int ret = -ENOMEM;
	int pipe, maxp_in, maxp_out;
	__u8 ep_in;
	__u8 ep_out;

	interface = intf->cur_altsetting;
	if (interface->desc.bNumEndpoints != 2) {
		err("kingsun-sir: expected 2 endpoints, found %d",
		    interface->desc.bNumEndpoints);
		return -ENODEV;
	}
	endpoint = &interface->endpoint[KINGSUN_EP_IN].desc;
	if (!usb_endpoint_is_int_in(endpoint)) {
		err("kingsun-sir: endpoint 0 is not interrupt IN");
		return -ENODEV;
	}

	ep_in = endpoint->bEndpointAddress;
	pipe = usb_rcvintpipe(dev, ep_in);
	maxp_in = usb_maxpacket(dev, pipe, usb_pipeout(pipe));
	if (maxp_in > 255 || maxp_in <= 1) {
		err("%s: endpoint 0 has max packet size %d not in range",
		    __FILE__, maxp_in);
		return -ENODEV;
	}

	endpoint = &interface->endpoint[KINGSUN_EP_OUT].desc;
	if (!usb_endpoint_is_int_out(endpoint)) {
		err("kingsun-sir: endpoint 1 is not interrupt OUT");
		return -ENODEV;
	}

	ep_out = endpoint->bEndpointAddress;
	pipe = usb_sndintpipe(dev, ep_out);
	maxp_out = usb_maxpacket(dev, pipe, usb_pipeout(pipe));

	
	net = alloc_irdadev(sizeof(*kingsun));
	if(!net)
		goto err_out1;

	SET_NETDEV_DEV(net, &intf->dev);
	kingsun = netdev_priv(net);
	kingsun->irlap = NULL;
	kingsun->tx_urb = NULL;
	kingsun->rx_urb = NULL;
	kingsun->ep_in = ep_in;
	kingsun->ep_out = ep_out;
	kingsun->in_buf = NULL;
	kingsun->out_buf = NULL;
	kingsun->max_rx = (__u8)maxp_in;
	kingsun->max_tx = (__u8)maxp_out;
	kingsun->netdev = net;
	kingsun->usbdev = dev;
	kingsun->rx_buff.in_frame = FALSE;
	kingsun->rx_buff.state = OUTSIDE_FRAME;
	kingsun->rx_buff.skb = NULL;
	kingsun->receiving = 0;
	spin_lock_init(&kingsun->lock);

	
	kingsun->in_buf = kmalloc(kingsun->max_rx, GFP_KERNEL);
	if (!kingsun->in_buf)
		goto free_mem;

	
	kingsun->out_buf = kmalloc(KINGSUN_FIFO_SIZE, GFP_KERNEL);
	if (!kingsun->out_buf)
		goto free_mem;

	printk(KERN_INFO "KingSun/DonShine IRDA/USB found at address %d, "
		"Vendor: %x, Product: %x\n",
	       dev->devnum, le16_to_cpu(dev->descriptor.idVendor),
	       le16_to_cpu(dev->descriptor.idProduct));

	
	irda_init_max_qos_capabilies(&kingsun->qos);

	
	kingsun->qos.baud_rate.bits       &= IR_9600;
	kingsun->qos.min_turn_time.bits   &= KINGSUN_MTT;
	irda_qos_bits_to_value(&kingsun->qos);

	
	net->netdev_ops = &kingsun_ops;

	ret = register_netdev(net);
	if (ret != 0)
		goto free_mem;

	dev_info(&net->dev, "IrDA: Registered KingSun/DonShine device %s\n",
		 net->name);

	usb_set_intfdata(intf, kingsun);


	return 0;

free_mem:
	if (kingsun->out_buf) kfree(kingsun->out_buf);
	if (kingsun->in_buf) kfree(kingsun->in_buf);
	free_netdev(net);
err_out1:
	return ret;
}

static void kingsun_disconnect(struct usb_interface *intf)
{
	struct kingsun_cb *kingsun = usb_get_intfdata(intf);

	if (!kingsun)
		return;

	unregister_netdev(kingsun->netdev);

	
	if (kingsun->tx_urb != NULL) {
		usb_kill_urb(kingsun->tx_urb);
		usb_free_urb(kingsun->tx_urb);
		kingsun->tx_urb = NULL;
	}
	if (kingsun->rx_urb != NULL) {
		usb_kill_urb(kingsun->rx_urb);
		usb_free_urb(kingsun->rx_urb);
		kingsun->rx_urb = NULL;
	}

	kfree(kingsun->out_buf);
	kfree(kingsun->in_buf);
	free_netdev(kingsun->netdev);

	usb_set_intfdata(intf, NULL);
}

#ifdef CONFIG_PM
static int kingsun_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct kingsun_cb *kingsun = usb_get_intfdata(intf);

	netif_device_detach(kingsun->netdev);
	if (kingsun->tx_urb != NULL) usb_kill_urb(kingsun->tx_urb);
	if (kingsun->rx_urb != NULL) usb_kill_urb(kingsun->rx_urb);
	return 0;
}

static int kingsun_resume(struct usb_interface *intf)
{
	struct kingsun_cb *kingsun = usb_get_intfdata(intf);

	if (kingsun->rx_urb != NULL)
		usb_submit_urb(kingsun->rx_urb, GFP_KERNEL);
	netif_device_attach(kingsun->netdev);

	return 0;
}
#endif

static struct usb_driver irda_driver = {
	.name		= "kingsun-sir",
	.probe		= kingsun_probe,
	.disconnect	= kingsun_disconnect,
	.id_table	= dongles,
#ifdef CONFIG_PM
	.suspend	= kingsun_suspend,
	.resume		= kingsun_resume,
#endif
};

module_usb_driver(irda_driver);

MODULE_AUTHOR("Alex Villacís Lasso <a_villacis@palosanto.com>");
MODULE_DESCRIPTION("IrDA-USB Dongle Driver for KingSun/DonShine");
MODULE_LICENSE("GPL");
