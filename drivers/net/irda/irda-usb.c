/*****************************************************************************
 *
 * Filename:      irda-usb.c
 * Version:       0.10
 * Description:   IrDA-USB Driver
 * Status:        Experimental 
 * Author:        Dag Brattli <dag@brattli.net>
 *
 *	Copyright (C) 2000, Roman Weissgaerber <weissg@vienna.at>
 *      Copyright (C) 2001, Dag Brattli <dag@brattli.net>
 *      Copyright (C) 2001, Jean Tourrilhes <jt@hpl.hp.com>
 *      Copyright (C) 2004, SigmaTel, Inc. <irquality@sigmatel.com>
 *      Copyright (C) 2005, Milan Beno <beno@pobox.sk>
 *      Copyright (C) 2006, Nick Fedchik <nick@fedchik.org.ua>
 *          
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
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
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/slab.h>
#include <linux/rtnetlink.h>
#include <linux/usb.h>
#include <linux/firmware.h>

#include "irda-usb.h"


static int qos_mtt_bits = 0;

static struct usb_device_id dongles[] = {
	
	{ USB_DEVICE(0x9c4, 0x011), .driver_info = IUC_SPEED_BUG | IUC_NO_WINDOW },
	
	{ USB_DEVICE(0x4428, 0x012), .driver_info = IUC_SPEED_BUG | IUC_NO_WINDOW },
	
	{ USB_DEVICE(0x50f, 0x180), .driver_info = IUC_SPEED_BUG | IUC_NO_WINDOW },
	
	{ USB_DEVICE(0x8e9, 0x100), .driver_info = IUC_SPEED_BUG | IUC_NO_WINDOW },
	
	{ USB_DEVICE(0x66f, 0x4210), .driver_info = IUC_STIR421X | IUC_SPEED_BUG },
	{ USB_DEVICE(0x66f, 0x4220), .driver_info = IUC_STIR421X | IUC_SPEED_BUG },
	{ USB_DEVICE(0x66f, 0x4116), .driver_info = IUC_STIR421X | IUC_SPEED_BUG },
	{ .match_flags = USB_DEVICE_ID_MATCH_INT_CLASS |
	  USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	  .bInterfaceClass = USB_CLASS_APP_SPEC,
	  .bInterfaceSubClass = USB_CLASS_IRDA,
	  .driver_info = IUC_DEFAULT, },
	{ }, 
};


MODULE_DEVICE_TABLE(usb, dongles);


static void irda_usb_init_qos(struct irda_usb_cb *self) ;
static struct irda_class_desc *irda_usb_find_class_desc(struct usb_interface *intf);
static void irda_usb_disconnect(struct usb_interface *intf);
static void irda_usb_change_speed_xbofs(struct irda_usb_cb *self);
static netdev_tx_t irda_usb_hard_xmit(struct sk_buff *skb,
					    struct net_device *dev);
static int irda_usb_open(struct irda_usb_cb *self);
static void irda_usb_close(struct irda_usb_cb *self);
static void speed_bulk_callback(struct urb *urb);
static void write_bulk_callback(struct urb *urb);
static void irda_usb_receive(struct urb *urb);
static void irda_usb_rx_defer_expired(unsigned long data);
static int irda_usb_net_open(struct net_device *dev);
static int irda_usb_net_close(struct net_device *dev);
static int irda_usb_net_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static void irda_usb_net_timeout(struct net_device *dev);


static void irda_usb_build_header(struct irda_usb_cb *self,
				  __u8 *header,
				  int	force)
{
	if (self->capability & IUC_STIR421X &&
	    ((self->new_speed != -1) || (self->new_xbofs != -1))) {

		if (self->new_speed == -1)
			self->new_speed = self->speed ;

		if (self->new_xbofs == -1)
			self->new_xbofs = self->xbofs ;
	}

	
	if (self->new_speed != -1) {
		if ((self->capability & IUC_SPEED_BUG) &&
		    (!force) && (self->speed != -1)) {
			IRDA_DEBUG(2, "%s(), not changing speed yet\n", __func__);
			*header = 0;
			return;
		}

		IRDA_DEBUG(2, "%s(), changing speed to %d\n", __func__, self->new_speed);
		self->speed = self->new_speed;

		switch (self->speed) {
		case 2400:
		        *header = SPEED_2400;
			break;
		default:
		case 9600:
			*header = SPEED_9600;
			break;
		case 19200:
			*header = SPEED_19200;
			break;
		case 38400:
			*header = SPEED_38400;
			break;
		case 57600:
		        *header = SPEED_57600;
			break;
		case 115200:
		        *header = SPEED_115200;
			break;
		case 576000:
		        *header = SPEED_576000;
			break;
		case 1152000:
		        *header = SPEED_1152000;
			break;
		case 4000000:
		        *header = SPEED_4000000;
			self->new_xbofs = 0;
			break;
		case 16000000:
			*header = SPEED_16000000;
  			self->new_xbofs = 0;
  			break;
  		}
	} else
		
		*header = 0;
	
	
	if (self->new_xbofs != -1) {
		IRDA_DEBUG(2, "%s(), changing xbofs to %d\n", __func__, self->new_xbofs);
		self->xbofs = self->new_xbofs;

		switch (self->xbofs) {
		case 48:
			*header |= 0x10;
			break;
		case 28:
		case 24:	
			*header |= 0x20;
			break;
		default:
		case 12:
			*header |= 0x30;
			break;
		case 5: 
		case 6:
			*header |= 0x40;
			break;
		case 3:
			*header |= 0x50;
			break;
		case 2:
			*header |= 0x60;
			break;
		case 1:
			*header |= 0x70;
			break;
		case 0:
			*header |= 0x80;
			break;
		}
	}
}

static __u8 get_turnaround_time(struct sk_buff *skb)
{
	int turnaround_time = irda_get_mtt(skb);

	if ( turnaround_time == 0 )
		return 0;
	else if ( turnaround_time <= 10 )
		return 1;
	else if ( turnaround_time <= 50 )
		return 2;
	else if ( turnaround_time <= 100 )
		return 3;
	else if ( turnaround_time <= 500 )
		return 4;
	else if ( turnaround_time <= 1000 )
		return 5;
	else if ( turnaround_time <= 5000 )
		return 6;
	else
		return 7;
}


static void irda_usb_change_speed_xbofs(struct irda_usb_cb *self)
{
	__u8 *frame;
	struct urb *urb;
	int ret;

	IRDA_DEBUG(2, "%s(), speed=%d, xbofs=%d\n", __func__,
		   self->new_speed, self->new_xbofs);

	
	urb = self->speed_urb;
	if (urb->status != 0) {
		IRDA_WARNING("%s(), URB still in use!\n", __func__);
		return;
	}

	
	frame = self->speed_buff;

	
	irda_usb_build_header(self, frame, 1);

	if (self->capability & IUC_STIR421X) {
		if (frame[0] == 0) return ; 
		frame[1] = 0; 
		frame[2] = 0;
	}

	
        usb_fill_bulk_urb(urb, self->usbdev,
		      usb_sndbulkpipe(self->usbdev, self->bulk_out_ep),
                      frame, IRDA_USB_SPEED_MTU,
                      speed_bulk_callback, self);
	urb->transfer_buffer_length = self->header_length;
	urb->transfer_flags = 0;

	
	if ((ret = usb_submit_urb(urb, GFP_ATOMIC))) {
		IRDA_WARNING("%s(), failed Speed URB\n", __func__);
	}
}

static void speed_bulk_callback(struct urb *urb)
{
	struct irda_usb_cb *self = urb->context;
	
	IRDA_DEBUG(2, "%s()\n", __func__);

	
	IRDA_ASSERT(self != NULL, return;);
	
	IRDA_ASSERT(urb == self->speed_urb, return;);

	
	if (urb->status != 0) {
		
		IRDA_DEBUG(0, "%s(), URB complete status %d, transfer_flags 0x%04X\n", __func__, urb->status, urb->transfer_flags);

		
		return;
	}

	
	

	
	self->new_speed = -1;
	self->new_xbofs = -1;

	
	netif_wake_queue(self->netdev);
}

static netdev_tx_t irda_usb_hard_xmit(struct sk_buff *skb,
					    struct net_device *netdev)
{
	struct irda_usb_cb *self = netdev_priv(netdev);
	struct urb *urb = self->tx_urb;
	unsigned long flags;
	s32 speed;
	s16 xbofs;
	int res, mtt;

	IRDA_DEBUG(4, "%s() on %s\n", __func__, netdev->name);

	netif_stop_queue(netdev);

	
	spin_lock_irqsave(&self->lock, flags);

	if (!self->present) {
		IRDA_DEBUG(0, "%s(), Device is gone...\n", __func__);
		goto drop;
	}

	
        xbofs = irda_get_next_xbofs(skb);
        if ((xbofs != self->xbofs) && (xbofs != -1)) {
		self->new_xbofs = xbofs;
	}

        
	speed = irda_get_next_speed(skb);
	if ((speed != self->speed) && (speed != -1)) {
		
		self->new_speed = speed;

		
		if (!skb->len) {
			irda_usb_change_speed_xbofs(self);
			netdev->trans_start = jiffies;
			
			goto drop;
		}
	}

	if (urb->status != 0) {
		IRDA_WARNING("%s(), URB still in use!\n", __func__);
		goto drop;
	}

	skb_copy_from_linear_data(skb, self->tx_buff + self->header_length, skb->len);

	
	if (self->capability & IUC_STIR421X) {
		__u8 turnaround_time;
		__u8* frame = self->tx_buff;
		turnaround_time = get_turnaround_time( skb );
		irda_usb_build_header(self, frame, 0);
		frame[2] = turnaround_time;
		if ((skb->len != 0) &&
		    ((skb->len % 128) == 0) &&
		    ((skb->len % 512) != 0)) {
			
			frame[1] = 1;
			skb_put(skb, 1);
		} else {
			frame[1] = 0;
		}
	} else {
		irda_usb_build_header(self, self->tx_buff, 0);
	}

	
	((struct irda_skb_cb *)skb->cb)->context = self;

	usb_fill_bulk_urb(urb, self->usbdev,
		      usb_sndbulkpipe(self->usbdev, self->bulk_out_ep),
                      self->tx_buff, skb->len + self->header_length,
                      write_bulk_callback, skb);

	urb->transfer_flags = URB_ZERO_PACKET;

	
	if (self->capability & IUC_NO_TURN) {
		mtt = irda_get_mtt(skb);
		if (mtt) {
			int diff;
			do_gettimeofday(&self->now);
			diff = self->now.tv_usec - self->stamp.tv_usec;
#ifdef IU_USB_MIN_RTT
			diff += IU_USB_MIN_RTT;
#endif 
			if (diff < 0)
				diff += 1000000;

			if (mtt > diff) {
				mtt -= diff;
				if (mtt > 1000)
					mdelay(mtt/1000);
				else
					udelay(mtt);
			}
		}
	}
	
	
	if ((res = usb_submit_urb(urb, GFP_ATOMIC))) {
		IRDA_WARNING("%s(), failed Tx URB\n", __func__);
		netdev->stats.tx_errors++;
		
		
	} else {
		
		netdev->stats.tx_packets++;
                netdev->stats.tx_bytes += skb->len;
		
		netdev->trans_start = jiffies;
	}
	spin_unlock_irqrestore(&self->lock, flags);
	
	return NETDEV_TX_OK;

drop:
	
	dev_kfree_skb(skb);
	spin_unlock_irqrestore(&self->lock, flags);
	return NETDEV_TX_OK;
}

static void write_bulk_callback(struct urb *urb)
{
	unsigned long flags;
	struct sk_buff *skb = urb->context;
	struct irda_usb_cb *self = ((struct irda_skb_cb *) skb->cb)->context;
	
	IRDA_DEBUG(2, "%s()\n", __func__);

	
	IRDA_ASSERT(self != NULL, return;);
	
	IRDA_ASSERT(urb == self->tx_urb, return;);

	
	dev_kfree_skb_any(skb);
	urb->context = NULL;

	
	if (urb->status != 0) {
		
		IRDA_DEBUG(0, "%s(), URB complete status %d, transfer_flags 0x%04X\n", __func__, urb->status, urb->transfer_flags);

		
		return;
	}

	
	

	
	spin_lock_irqsave(&self->lock, flags);

	
	if ((!self->netopen) || (!self->present)) {
		IRDA_DEBUG(0, "%s(), Network is gone...\n", __func__);
		spin_unlock_irqrestore(&self->lock, flags);
		return;
	}

	
	if ((self->new_speed != -1) || (self->new_xbofs != -1)) {
		if ((self->new_speed != self->speed) ||
		    (self->new_xbofs != self->xbofs)) {
			IRDA_DEBUG(1, "%s(), Changing speed now...\n", __func__);
			irda_usb_change_speed_xbofs(self);
		} else {
			
			self->new_speed = -1;
			self->new_xbofs = -1;
			
			netif_wake_queue(self->netdev);
		}
	} else {
		
		netif_wake_queue(self->netdev);
	}
	spin_unlock_irqrestore(&self->lock, flags);
}

static void irda_usb_net_timeout(struct net_device *netdev)
{
	unsigned long flags;
	struct irda_usb_cb *self = netdev_priv(netdev);
	struct urb *urb;
	int	done = 0;	

	IRDA_DEBUG(0, "%s(), Network layer thinks we timed out!\n", __func__);
	IRDA_ASSERT(self != NULL, return;);

	
	spin_lock_irqsave(&self->lock, flags);

	
	if (!self->present) {
		IRDA_WARNING("%s(), device not present!\n", __func__);
		netif_stop_queue(netdev);
		spin_unlock_irqrestore(&self->lock, flags);
		return;
	}

	
	urb = self->speed_urb;
	if (urb->status != 0) {
		IRDA_DEBUG(0, "%s: Speed change timed out, urb->status=%d, urb->transfer_flags=0x%04X\n", netdev->name, urb->status, urb->transfer_flags);

		switch (urb->status) {
		case -EINPROGRESS:
			usb_unlink_urb(urb);
			done = 1;
			break;
		case -ECONNRESET:
		case -ENOENT:			
		default:			
			urb->status = 0;
			netif_wake_queue(self->netdev);
			done = 1;
			break;
		}
	}

	
	urb = self->tx_urb;
	if (urb->status != 0) {
		struct sk_buff *skb = urb->context;

		IRDA_DEBUG(0, "%s: Tx timed out, urb->status=%d, urb->transfer_flags=0x%04X\n", netdev->name, urb->status, urb->transfer_flags);

		
		netdev->stats.tx_errors++;

#ifdef IU_BUG_KICK_TIMEOUT
		
		if(self->new_speed == -1)
			self->new_speed = self->speed;
		if(self->new_xbofs == -1)
			self->new_xbofs = self->xbofs;
		irda_usb_change_speed_xbofs(self);
#endif 

		switch (urb->status) {
		case -EINPROGRESS:
			usb_unlink_urb(urb);
			done = 1;
			break;
		case -ECONNRESET:
		case -ENOENT:			
		default:			
			if(skb != NULL) {
				dev_kfree_skb_any(skb);
				urb->context = NULL;
			}
			urb->status = 0;
			netif_wake_queue(self->netdev);
			done = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&self->lock, flags);

	

	
}



static void irda_usb_submit(struct irda_usb_cb *self, struct sk_buff *skb, struct urb *urb)
{
	struct irda_skb_cb *cb;
	int ret;

	IRDA_DEBUG(2, "%s()\n", __func__);

	
	IRDA_ASSERT(skb != NULL, return;);
	IRDA_ASSERT(urb != NULL, return;);

	
	cb = (struct irda_skb_cb *) skb->cb;
	cb->context = self;

	
	usb_fill_bulk_urb(urb, self->usbdev, 
		      usb_rcvbulkpipe(self->usbdev, self->bulk_in_ep), 
		      skb->data, IRDA_SKB_MAX_MTU,
                      irda_usb_receive, skb);
	urb->status = 0;

	
	ret = usb_submit_urb(urb, GFP_ATOMIC);
	if (ret) {
		IRDA_WARNING("%s(), Failed to submit Rx URB %d\n",
			     __func__, ret);
	}
}

static void irda_usb_receive(struct urb *urb)
{
	struct sk_buff *skb = (struct sk_buff *) urb->context;
	struct irda_usb_cb *self; 
	struct irda_skb_cb *cb;
	struct sk_buff *newskb;
	struct sk_buff *dataskb;
	struct urb *next_urb;
	unsigned int len, docopy;

	IRDA_DEBUG(2, "%s(), len=%d\n", __func__, urb->actual_length);
	
	
	cb = (struct irda_skb_cb *) skb->cb;
	IRDA_ASSERT(cb != NULL, return;);
	self = (struct irda_usb_cb *) cb->context;
	IRDA_ASSERT(self != NULL, return;);

	
	if ((!self->netopen) || (!self->present)) {
		IRDA_DEBUG(0, "%s(), Network is gone!\n", __func__);
		
		return;
	}
	
	
	if (urb->status != 0) {
		switch (urb->status) {
		case -EILSEQ:
			self->netdev->stats.rx_crc_errors++;
			
			
		case -ECONNRESET:
			
		case -ESHUTDOWN:
			
		case -ETIME:
			
		default:
			self->netdev->stats.rx_errors++;
			IRDA_DEBUG(0, "%s(), RX status %d, transfer_flags 0x%04X\n", __func__, urb->status, urb->transfer_flags);
			break;
		}
		self->rx_defer_timer.function = irda_usb_rx_defer_expired;
		self->rx_defer_timer.data = (unsigned long) urb;
		mod_timer(&self->rx_defer_timer, jiffies + (10 * HZ / 1000));
		return;
	}
	
	
	if (urb->actual_length <= self->header_length) {
		IRDA_WARNING("%s(), empty frame!\n", __func__);
		goto done;
	}

        do_gettimeofday(&self->stamp);

	docopy = (urb->actual_length < IRDA_RX_COPY_THRESHOLD);

	
	if (self->capability & IUC_STIR421X)
		newskb = dev_alloc_skb(docopy ? urb->actual_length :
				       IRDA_SKB_MAX_MTU +
				       USB_IRDA_STIR421X_HEADER);
	else
		newskb = dev_alloc_skb(docopy ? urb->actual_length :
				       IRDA_SKB_MAX_MTU);

	if (!newskb)  {
		self->netdev->stats.rx_dropped++;
		goto done;  
	}

	
	
	

	if(docopy) {
		
		skb_copy_from_linear_data(skb, newskb->data, urb->actual_length);
		
		dataskb = newskb;
	} else {
		
		dataskb = skb;
		
		skb = newskb;
	}

	
	skb_put(dataskb, urb->actual_length);
	skb_pull(dataskb, self->header_length);

	
	dataskb->dev = self->netdev;
	skb_reset_mac_header(dataskb);
	dataskb->protocol = htons(ETH_P_IRDA);
	len = dataskb->len;
	netif_rx(dataskb);

	
	self->netdev->stats.rx_bytes += len;
	self->netdev->stats.rx_packets++;

done:
	next_urb = self->idle_rx_urb;

	
	urb->context = NULL;
	self->idle_rx_urb = urb;

	irda_usb_submit(self, skb, next_urb);
}

static void irda_usb_rx_defer_expired(unsigned long data)
{
	struct urb *urb = (struct urb *) data;
	struct sk_buff *skb = (struct sk_buff *) urb->context;
	struct irda_usb_cb *self; 
	struct irda_skb_cb *cb;
	struct urb *next_urb;

	IRDA_DEBUG(2, "%s()\n", __func__);

	
	cb = (struct irda_skb_cb *) skb->cb;
	IRDA_ASSERT(cb != NULL, return;);
	self = (struct irda_usb_cb *) cb->context;
	IRDA_ASSERT(self != NULL, return;);

	
	next_urb = self->idle_rx_urb;
	urb->context = NULL;
	self->idle_rx_urb = urb;
	irda_usb_submit(self, skb, next_urb);
}

static int irda_usb_is_receiving(struct irda_usb_cb *self)
{

	return 0; 
}

#define STIR421X_PATCH_PRODUCT_VER     "Product Version: "
#define STIR421X_PATCH_STMP_TAG        "STMP"
#define STIR421X_PATCH_CODE_OFFSET     512 
#define STIR421X_PATCH_END_OF_HDR_TAG  0x1A
#define STIR421X_PATCH_BLOCK_SIZE      1023

static int stir421x_fw_upload(struct irda_usb_cb *self,
			     const unsigned char *patch,
			     const unsigned int patch_len)
{
        int ret = -ENOMEM;
        int actual_len = 0;
        unsigned int i;
        unsigned int block_size = 0;
        unsigned char *patch_block;

        patch_block = kzalloc(STIR421X_PATCH_BLOCK_SIZE, GFP_KERNEL);
	if (patch_block == NULL)
		return -ENOMEM;

	
	for (i = 0; i < patch_len; i += block_size) {
		block_size = patch_len - i;

		if (block_size > STIR421X_PATCH_BLOCK_SIZE)
			block_size = STIR421X_PATCH_BLOCK_SIZE;

		
		memcpy(patch_block, patch + i, block_size);

		ret = usb_bulk_msg(self->usbdev,
				   usb_sndbulkpipe(self->usbdev,
						   self->bulk_out_ep),
				   patch_block, block_size,
				   &actual_len, msecs_to_jiffies(500));
		IRDA_DEBUG(3,"%s(): Bulk send %u bytes, ret=%d\n",
			   __func__, actual_len, ret);

		if (ret < 0)
			break;

		mdelay(10);
	}

	kfree(patch_block);

        return ret;
 }

static int stir421x_patch_device(struct irda_usb_cb *self)
{
	unsigned int i;
	int ret;
	char stir421x_fw_name[12];
	const struct firmware *fw;
	const unsigned char *fw_version_ptr; 
	unsigned long fw_version = 0;

        sprintf(stir421x_fw_name, "4210%4X.sb",
                self->usbdev->descriptor.bcdDevice);
        ret = request_firmware(&fw, stir421x_fw_name, &self->usbdev->dev);
        if (ret < 0)
                return ret;

        
        IRDA_MESSAGE("%s(): Received firmware %s (%zu bytes)\n",
                     __func__, stir421x_fw_name, fw->size);

        ret = -EINVAL;

	
        if (!memcmp(fw->data, STIR421X_PATCH_PRODUCT_VER,
                    sizeof(STIR421X_PATCH_PRODUCT_VER) - 1)) {
                fw_version_ptr = fw->data +
			sizeof(STIR421X_PATCH_PRODUCT_VER) - 1;

                
                if (fw_version_ptr[3] == '.' &&
		    fw_version_ptr[7] == '.') {
			unsigned long major, minor, build;
			major = simple_strtoul(fw_version_ptr, NULL, 10);
			minor = simple_strtoul(fw_version_ptr + 4, NULL, 10);
			build = simple_strtoul(fw_version_ptr + 8, NULL, 10);

			fw_version = (major << 12)
				+ (minor << 8)
				+ ((build / 10) << 4)
				+ (build % 10);

			IRDA_DEBUG(3, "%s(): Firmware Product version %ld\n",
                                   __func__, fw_version);
                }
        }

        if (self->usbdev->descriptor.bcdDevice == cpu_to_le16(fw_version)) {
                for (i = 0; i < fw->size && fw->data[i] !=
			     STIR421X_PATCH_END_OF_HDR_TAG; i++) ;
                
                if (i < STIR421X_PATCH_CODE_OFFSET && i < fw->size &&
				STIR421X_PATCH_END_OF_HDR_TAG == fw->data[i]) {
                        if (!memcmp(fw->data + i + 1, STIR421X_PATCH_STMP_TAG,
                                    sizeof(STIR421X_PATCH_STMP_TAG) - 1)) {

				
				i += sizeof(STIR421X_PATCH_STMP_TAG);
                                ret = stir421x_fw_upload(self, &fw->data[i],
							 fw->size - i);
                        }
                }
        }

        release_firmware(fw);

        return ret;
}




static int irda_usb_net_open(struct net_device *netdev)
{
	struct irda_usb_cb *self;
	unsigned long flags;
	char	hwname[16];
	int i;
	
	IRDA_DEBUG(1, "%s()\n", __func__);

	IRDA_ASSERT(netdev != NULL, return -1;);
	self = netdev_priv(netdev);
	IRDA_ASSERT(self != NULL, return -1;);

	spin_lock_irqsave(&self->lock, flags);
	
	if(!self->present) {
		spin_unlock_irqrestore(&self->lock, flags);
		IRDA_WARNING("%s(), device not present!\n", __func__);
		return -1;
	}

	if(self->needspatch) {
		spin_unlock_irqrestore(&self->lock, flags);
		IRDA_WARNING("%s(), device needs patch\n", __func__) ;
		return -EIO ;
	}

	self->speed = -1;
	self->xbofs = -1;
	self->new_speed = -1;
	self->new_xbofs = -1;

	self->netopen = 1;
	spin_unlock_irqrestore(&self->lock, flags);

	sprintf(hwname, "usb#%d", self->usbdev->devnum);
	self->irlap = irlap_open(netdev, &self->qos, hwname);
	IRDA_ASSERT(self->irlap != NULL, return -1;);

	
	netif_start_queue(netdev);

	self->idle_rx_urb = self->rx_urb[IU_MAX_ACTIVE_RX_URBS];
	self->idle_rx_urb->context = NULL;

	for (i = 0; i < IU_MAX_ACTIVE_RX_URBS; i++) {
		struct sk_buff *skb = dev_alloc_skb(IRDA_SKB_MAX_MTU);
		if (!skb) {
			IRDA_WARNING("%s(), Failed to allocate Rx skb\n",
				     __func__);
			return -1;
		}
		
		irda_usb_submit(self, skb, self->rx_urb[i]);
	}

	
	return 0;
}

static int irda_usb_net_close(struct net_device *netdev)
{
	struct irda_usb_cb *self;
	int	i;

	IRDA_DEBUG(1, "%s()\n", __func__);

	IRDA_ASSERT(netdev != NULL, return -1;);
	self = netdev_priv(netdev);
	IRDA_ASSERT(self != NULL, return -1;);

	self->netopen = 0;

	
	netif_stop_queue(netdev);

	
	del_timer(&self->rx_defer_timer);

	
	for (i = 0; i < self->max_rx_urb; i++) {
		struct urb *urb = self->rx_urb[i];
		struct sk_buff *skb = (struct sk_buff *) urb->context;
		
		usb_kill_urb(urb);
		
		if(skb) {
			dev_kfree_skb(skb);
			urb->context = NULL;
		}
	}
	
	usb_kill_urb(self->tx_urb);
	usb_kill_urb(self->speed_urb);

	
	if (self->irlap)
		irlap_close(self->irlap);
	self->irlap = NULL;

	return 0;
}

static int irda_usb_net_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	unsigned long flags;
	struct if_irda_req *irq = (struct if_irda_req *) rq;
	struct irda_usb_cb *self;
	int ret = 0;

	IRDA_ASSERT(dev != NULL, return -1;);
	self = netdev_priv(dev);
	IRDA_ASSERT(self != NULL, return -1;);

	IRDA_DEBUG(2, "%s(), %s, (cmd=0x%X)\n", __func__, dev->name, cmd);

	switch (cmd) {
	case SIOCSBANDWIDTH: 
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		
		spin_lock_irqsave(&self->lock, flags);
		
		if(self->present) {
			
			self->new_speed = irq->ifr_baudrate;
			irda_usb_change_speed_xbofs(self);
		}
		spin_unlock_irqrestore(&self->lock, flags);
		break;
	case SIOCSMEDIABUSY: 
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		
		if(self->netopen)
			irda_device_set_media_busy(self->netdev, TRUE);
		break;
	case SIOCGRECEIVING: 
		irq->ifr_receiving = irda_usb_is_receiving(self);
		break;
	default:
		ret = -EOPNOTSUPP;
	}
	
	return ret;
}



static inline void irda_usb_init_qos(struct irda_usb_cb *self)
{
	struct irda_class_desc *desc;

	IRDA_DEBUG(3, "%s()\n", __func__);
	
	desc = self->irda_desc;
	
	
	irda_init_max_qos_capabilies(&self->qos);

	self->qos.baud_rate.bits       = le16_to_cpu(desc->wBaudRate);
	self->qos.min_turn_time.bits   = desc->bmMinTurnaroundTime;
	self->qos.additional_bofs.bits = desc->bmAdditionalBOFs;
	self->qos.window_size.bits     = desc->bmWindowSize;
	self->qos.data_size.bits       = desc->bmDataSize;

	IRDA_DEBUG(0, "%s(), dongle says speed=0x%X, size=0x%X, window=0x%X, bofs=0x%X, turn=0x%X\n", 
		__func__, self->qos.baud_rate.bits, self->qos.data_size.bits, self->qos.window_size.bits, self->qos.additional_bofs.bits, self->qos.min_turn_time.bits);

	
	if(self->capability & IUC_SIR_ONLY)
		self->qos.baud_rate.bits	&= 0x00ff;
	if(self->capability & IUC_SMALL_PKT)
		self->qos.data_size.bits	 = 0x07;
	if(self->capability & IUC_NO_WINDOW)
		self->qos.window_size.bits	 = 0x01;
	if(self->capability & IUC_MAX_WINDOW)
		self->qos.window_size.bits	 = 0x7f;
	if(self->capability & IUC_MAX_XBOFS)
		self->qos.additional_bofs.bits	 = 0x01;

#if 1
	
	if (qos_mtt_bits)
		self->qos.min_turn_time.bits = qos_mtt_bits;
#endif	    
	irda_qos_bits_to_value(&self->qos);
}

static const struct net_device_ops irda_usb_netdev_ops = {
	.ndo_open       = irda_usb_net_open,
	.ndo_stop       = irda_usb_net_close,
	.ndo_do_ioctl   = irda_usb_net_ioctl,
	.ndo_start_xmit = irda_usb_hard_xmit,
	.ndo_tx_timeout	= irda_usb_net_timeout,
};

static inline int irda_usb_open(struct irda_usb_cb *self)
{
	struct net_device *netdev = self->netdev;

	IRDA_DEBUG(1, "%s()\n", __func__);

	netdev->netdev_ops = &irda_usb_netdev_ops;

	irda_usb_init_qos(self);

	return register_netdev(netdev);
}

static inline void irda_usb_close(struct irda_usb_cb *self)
{
	IRDA_DEBUG(1, "%s()\n", __func__);

	
	unregister_netdev(self->netdev);

	
	kfree(self->speed_buff);
	self->speed_buff = NULL;

	kfree(self->tx_buff);
	self->tx_buff = NULL;
}


static inline int irda_usb_parse_endpoints(struct irda_usb_cb *self, struct usb_host_endpoint *endpoint, int ennum)
{
	int i;		
		
	
	self->bulk_in_ep = 0;
	self->bulk_out_ep = 0;
	self->bulk_int_ep = 0;

	
	for(i = 0; i < ennum; i++) {
		__u8 ep;	
		__u8 dir;	
		__u8 attr;	
		__u16 psize;	

		
		ep = endpoint[i].desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
		dir = endpoint[i].desc.bEndpointAddress & USB_ENDPOINT_DIR_MASK;
		attr = endpoint[i].desc.bmAttributes;
		psize = le16_to_cpu(endpoint[i].desc.wMaxPacketSize);

		
		if(attr == USB_ENDPOINT_XFER_BULK) {
			
			if(dir == USB_DIR_IN) {
				
				self->bulk_in_ep = ep;
			} else {
				
				self->bulk_out_ep = ep;
				self->bulk_out_mtu = psize;
			}
		} else {
			if((attr == USB_ENDPOINT_XFER_INT) &&
			   (dir == USB_DIR_IN)) {
				
				self->bulk_int_ep = ep;
			} else {
				IRDA_ERROR("%s(), Unrecognised endpoint %02X.\n", __func__, ep);
			}
		}
	}

	IRDA_DEBUG(0, "%s(), And our endpoints are : in=%02X, out=%02X (%d), int=%02X\n",
		__func__, self->bulk_in_ep, self->bulk_out_ep, self->bulk_out_mtu, self->bulk_int_ep);

	return (self->bulk_in_ep != 0) && (self->bulk_out_ep != 0);
}

#ifdef IU_DUMP_CLASS_DESC
static inline void irda_usb_dump_class_desc(struct irda_class_desc *desc)
{
	
	printk("bLength=%x\n", desc->bLength);
	printk("bDescriptorType=%x\n", desc->bDescriptorType);
	printk("bcdSpecRevision=%x\n", le16_to_cpu(desc->bcdSpecRevision)); 
	printk("bmDataSize=%x\n", desc->bmDataSize);
	printk("bmWindowSize=%x\n", desc->bmWindowSize);
	printk("bmMinTurnaroundTime=%d\n", desc->bmMinTurnaroundTime);
	printk("wBaudRate=%x\n", le16_to_cpu(desc->wBaudRate));
	printk("bmAdditionalBOFs=%x\n", desc->bmAdditionalBOFs);
	printk("bIrdaRateSniff=%x\n", desc->bIrdaRateSniff);
	printk("bMaxUnicastList=%x\n", desc->bMaxUnicastList);
}
#endif 

static inline struct irda_class_desc *irda_usb_find_class_desc(struct usb_interface *intf)
{
	struct usb_device *dev = interface_to_usbdev (intf);
	struct irda_class_desc *desc;
	int ret;

	desc = kzalloc(sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return NULL;


	ret = usb_control_msg(dev, usb_rcvctrlpipe(dev,0),
		IU_REQ_GET_CLASS_DESC,
		USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
		0, intf->altsetting->desc.bInterfaceNumber, desc,
		sizeof(*desc), 500);
	
	IRDA_DEBUG(1, "%s(), ret=%d\n", __func__, ret);
	if (ret < sizeof(*desc)) {
		IRDA_WARNING("usb-irda: class_descriptor read %s (%d)\n",
			     (ret<0) ? "failed" : "too short", ret);
	}
	else if (desc->bDescriptorType != USB_DT_IRDA) {
		IRDA_WARNING("usb-irda: bad class_descriptor type\n");
	}
	else {
#ifdef IU_DUMP_CLASS_DESC
		irda_usb_dump_class_desc(desc);
#endif	

		return desc;
	}
	kfree(desc);
	return NULL;
}


static int irda_usb_probe(struct usb_interface *intf,
			  const struct usb_device_id *id)
{
	struct net_device *net;
	struct usb_device *dev = interface_to_usbdev(intf);
	struct irda_usb_cb *self;
	struct usb_host_interface *interface;
	struct irda_class_desc *irda_desc;
	int ret = -ENOMEM;
	int i;		


	IRDA_MESSAGE("IRDA-USB found at address %d, Vendor: %x, Product: %x\n",
		     dev->devnum, le16_to_cpu(dev->descriptor.idVendor),
		     le16_to_cpu(dev->descriptor.idProduct));

	net = alloc_irdadev(sizeof(*self));
	if (!net) 
		goto err_out;

	SET_NETDEV_DEV(net, &intf->dev);
	self = netdev_priv(net);
	self->netdev = net;
	spin_lock_init(&self->lock);
	init_timer(&self->rx_defer_timer);

	self->capability = id->driver_info;
	self->needspatch = ((self->capability & IUC_STIR421X) != 0);

	
	if (self->capability & IUC_STIR421X) {
		self->max_rx_urb = IU_SIGMATEL_MAX_RX_URBS;
		self->header_length = USB_IRDA_STIR421X_HEADER;
	} else {
		self->max_rx_urb = IU_MAX_RX_URBS;
		self->header_length = USB_IRDA_HEADER;
	}

	self->rx_urb = kcalloc(self->max_rx_urb, sizeof(struct urb *),
				GFP_KERNEL);
	if (!self->rx_urb)
		goto err_free_net;

	for (i = 0; i < self->max_rx_urb; i++) {
		self->rx_urb[i] = usb_alloc_urb(0, GFP_KERNEL);
		if (!self->rx_urb[i]) {
			goto err_out_1;
		}
	}
	self->tx_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!self->tx_urb) {
		goto err_out_1;
	}
	self->speed_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!self->speed_urb) {
		goto err_out_2;
	}

	
	if (usb_reset_configuration (dev) < 0) {
		err("reset_configuration failed");
		ret = -EIO;
		goto err_out_3;
	}

	
	ret = usb_set_interface(dev, intf->altsetting->desc.bInterfaceNumber, 0);
	IRDA_DEBUG(1, "usb-irda: set interface %d result %d\n", intf->altsetting->desc.bInterfaceNumber, ret);
	switch (ret) {
		case 0:
			break;
		case -EPIPE:		
			IRDA_DEBUG(0, "%s(), Received -EPIPE, ignoring...\n", __func__);
			break;
		default:
			IRDA_DEBUG(0, "%s(), Unknown error %d\n", __func__, ret);
			ret = -EIO;
			goto err_out_3;
	}

	
	interface = intf->cur_altsetting;
	if(!irda_usb_parse_endpoints(self, interface->endpoint,
				     interface->desc.bNumEndpoints)) {
		IRDA_ERROR("%s(), Bogus endpoints...\n", __func__);
		ret = -EIO;
		goto err_out_3;
	}

	self->usbdev = dev;

	
	irda_desc = irda_usb_find_class_desc(intf);
	ret = -ENODEV;
	if (!irda_desc)
		goto err_out_3;

	if (self->needspatch) {
		ret = usb_control_msg (self->usbdev, usb_sndctrlpipe (self->usbdev, 0),
				       0x02, 0x40, 0, 0, NULL, 0, 500);
		if (ret < 0) {
			IRDA_DEBUG (0, "usb_control_msg failed %d\n", ret);
			goto err_out_3;
		} else {
			mdelay(10);
		}
	}

	self->irda_desc =  irda_desc;
	self->present = 1;
	self->netopen = 0;
	self->usbintf = intf;

	
	self->speed_buff = kzalloc(IRDA_USB_SPEED_MTU, GFP_KERNEL);
	if (!self->speed_buff)
		goto err_out_3;

	self->tx_buff = kzalloc(IRDA_SKB_MAX_MTU + self->header_length,
				GFP_KERNEL);
	if (!self->tx_buff)
		goto err_out_4;

	ret = irda_usb_open(self);
	if (ret) 
		goto err_out_5;

	IRDA_MESSAGE("IrDA: Registered device %s\n", net->name);
	usb_set_intfdata(intf, self);

	if (self->needspatch) {
		
		ret = stir421x_patch_device(self);
		self->needspatch = (ret < 0);
		if (self->needspatch) {
			IRDA_ERROR("STIR421X: Couldn't upload patch\n");
			goto err_out_6;
		}

		
		irda_desc = irda_usb_find_class_desc (self->usbintf);
		if (!irda_desc) {
			ret = -ENODEV;
			goto err_out_6;
		}
		kfree(self->irda_desc);
		self->irda_desc = irda_desc;
		irda_usb_init_qos(self);
	}

	return 0;
err_out_6:
	unregister_netdev(self->netdev);
err_out_5:
	kfree(self->tx_buff);
err_out_4:
	kfree(self->speed_buff);
err_out_3:
	
	usb_free_urb(self->speed_urb);
err_out_2:
	usb_free_urb(self->tx_urb);
err_out_1:
	for (i = 0; i < self->max_rx_urb; i++)
		usb_free_urb(self->rx_urb[i]);
	kfree(self->rx_urb);
err_free_net:
	free_netdev(net);
err_out:
	return ret;
}

static void irda_usb_disconnect(struct usb_interface *intf)
{
	unsigned long flags;
	struct irda_usb_cb *self = usb_get_intfdata(intf);
	int i;

	IRDA_DEBUG(1, "%s()\n", __func__);

	usb_set_intfdata(intf, NULL);
	if (!self)
		return;

	
	spin_lock_irqsave(&self->lock, flags);

	self->present = 0;

	
	del_timer(&self->rx_defer_timer);

	spin_unlock_irqrestore(&self->lock, flags);

	
	if((self->netopen) || (self->irlap)) {
		
		
		netif_stop_queue(self->netdev);
		
		for (i = 0; i < self->max_rx_urb; i++)
			usb_kill_urb(self->rx_urb[i]);
		usb_kill_urb(self->tx_urb);
		usb_kill_urb(self->speed_urb);
	}

	
	irda_usb_close(self);
	
	self->usbdev = NULL;
	self->usbintf = NULL;

	
	for (i = 0; i < self->max_rx_urb; i++)
		usb_free_urb(self->rx_urb[i]);
	kfree(self->rx_urb);
	
	usb_free_urb(self->tx_urb);
	usb_free_urb(self->speed_urb);

	
	free_netdev(self->netdev);
	IRDA_DEBUG(0, "%s(), USB IrDA Disconnected\n", __func__);
}

#ifdef CONFIG_PM
static int irda_usb_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct irda_usb_cb *self = usb_get_intfdata(intf);
	int i;

	netif_device_detach(self->netdev);

	if (self->tx_urb != NULL)
		usb_kill_urb(self->tx_urb);
	if (self->speed_urb != NULL)
		usb_kill_urb(self->speed_urb);
	for (i = 0; i < self->max_rx_urb; i++) {
		if (self->rx_urb[i] != NULL)
			usb_kill_urb(self->rx_urb[i]);
	}
	return 0;
}

static int irda_usb_resume(struct usb_interface *intf)
{
	struct irda_usb_cb *self = usb_get_intfdata(intf);
	int i;

	for (i = 0; i < self->max_rx_urb; i++) {
		if (self->rx_urb[i] != NULL)
			usb_submit_urb(self->rx_urb[i], GFP_KERNEL);
	}

	netif_device_attach(self->netdev);
	return 0;
}
#endif

static struct usb_driver irda_driver = {
	.name		= "irda-usb",
	.probe		= irda_usb_probe,
	.disconnect	= irda_usb_disconnect,
	.id_table	= dongles,
#ifdef CONFIG_PM
	.suspend	= irda_usb_suspend,
	.resume		= irda_usb_resume,
#endif
};

module_usb_driver(irda_driver);

module_param(qos_mtt_bits, int, 0);
MODULE_PARM_DESC(qos_mtt_bits, "Minimum Turn Time");
MODULE_AUTHOR("Roman Weissgaerber <weissg@vienna.at>, Dag Brattli <dag@brattli.net>, Jean Tourrilhes <jt@hpl.hp.com> and Nick Fedchik <nick@fedchik.org.ua>");
MODULE_DESCRIPTION("IrDA-USB Dongle Driver");
MODULE_LICENSE("GPL");
