/*
 * USB-to-WWAN Driver for Sierra Wireless modems
 *
 * Copyright (C) 2008, 2009, 2010 Paxton Smith, Matthew Safar, Rory Filer
 *                          <linux@sierrawireless.com>
 *
 * Portions of this based on the cdc_ether driver by David Brownell (2003-2005)
 * and Ole Andre Vadla Ravnas (ActiveSync) (2006).
 *
 * IMPORTANT DISCLAIMER: This driver is not commercially supported by
 * Sierra Wireless. Use at your own risk.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define DRIVER_VERSION "v.2.0"
#define DRIVER_AUTHOR "Paxton Smith, Matthew Safar, Rory Filer"
#define DRIVER_DESC "USB-to-WWAN Driver for Sierra Wireless modems"
static const char driver_name[] = "sierra_net";


#include <linux/module.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/usb.h>
#include <linux/usb/cdc.h>
#include <net/ip.h>
#include <net/udp.h>
#include <asm/unaligned.h>
#include <linux/usb/usbnet.h>

#define SWI_USB_REQUEST_GET_FW_ATTR	0x06
#define SWI_GET_FW_ATTR_MASK		0x08

static	atomic_t iface_counter = ATOMIC_INIT(0);

#define SIERRA_NET_SYNCDELAY (2*HZ)

#define SIERRA_NET_MAX_SUPPORTED_MTU	1500

#define SIERRA_NET_USBCTL_BUF_LEN	1024

struct sierra_net_iface_info {
	const u32 infolen;	
	const u8  *ifaceinfo;	
};

struct sierra_net_info_data {
	u16 rx_urb_size;
	struct sierra_net_iface_info whitelist;
};

struct sierra_net_data {

	u8 ethr_hdr_tmpl[ETH_HLEN]; 

	u16 link_up;		
	u8 tx_hdr_template[4];	

	u8 sync_msg[4];		
	u8 shdwn_msg[4];	

	
	struct usbnet *usbnet;

	u8 ifnum;	

#define SIERRA_NET_EVENT_RESP_AVAIL    0x01
#define SIERRA_NET_TIMER_EXPIRY        0x02
	unsigned long kevent_flags;
	struct work_struct sierra_net_kevent;
	struct timer_list sync_timer; 
};

struct param {
	int is_present;
	union {
		void  *ptr;
		u32    dword;
		u16    word;
		u8     byte;
	};
};

#define SIERRA_NET_HIP_EXTENDEDID	0x7F
#define SIERRA_NET_HIP_HSYNC_ID		0x60	
#define SIERRA_NET_HIP_RESTART_ID	0x62	
#define SIERRA_NET_HIP_MSYNC_ID		0x20	
#define SIERRA_NET_HIP_SHUTD_ID		0x26	

#define SIERRA_NET_HIP_EXT_IP_IN_ID   0x0202
#define SIERRA_NET_HIP_EXT_IP_OUT_ID  0x0002

#define SIERRA_NET_HIP_LSI_UMTSID	0x78

#define SIERRA_NET_HIP_RCGI		0x64

#define SIERRA_NET_PROTOCOL_UMTS      0x01
#define SIERRA_NET_COVERAGE_NONE      0x00
#define SIERRA_NET_COVERAGE_NOPACKET  0x01

#define SIERRA_NET_SESSION_IDLE       0x00
#define SIERRA_NET_AS_LINK_TYPE_IPv4  0x00

struct lsi_umts {
	u8 protocol;
	u8 unused1;
	__be16 length;
	
	u8 coverage;
	u8 unused2[41];
	u8 session_state;
	u8 unused3[33];
	u8 link_type;
	u8 pdp_addr_len; 
	u8 pdp_addr[16]; 
	u8 unused4[23];
	u8 dns1_addr_len; 
	u8 dns1_addr[16]; 
	u8 dns2_addr_len; 
	u8 dns2_addr[16]; 
	u8 wins1_addr_len; 
	u8 wins1_addr[16]; 
	u8 wins2_addr_len; 
	u8 wins2_addr[16]; 
	u8 unused5[4];
	u8 gw_addr_len; 
	u8 gw_addr[16]; 
	u8 reserved[8];
} __packed;

#define SIERRA_NET_LSI_COMMON_LEN      4
#define SIERRA_NET_LSI_UMTS_LEN        (sizeof(struct lsi_umts))
#define SIERRA_NET_LSI_UMTS_STATUS_LEN \
	(SIERRA_NET_LSI_UMTS_LEN - SIERRA_NET_LSI_COMMON_LEN)

static void sierra_sync_timer(unsigned long syncdata);
static int sierra_net_change_mtu(struct net_device *net, int new_mtu);

static const struct net_device_ops sierra_net_device_ops = {
	.ndo_open               = usbnet_open,
	.ndo_stop               = usbnet_stop,
	.ndo_start_xmit         = usbnet_start_xmit,
	.ndo_tx_timeout         = usbnet_tx_timeout,
	.ndo_change_mtu         = sierra_net_change_mtu,
	.ndo_set_mac_address    = eth_mac_addr,
	.ndo_validate_addr      = eth_validate_addr,
};

static inline struct sierra_net_data *sierra_net_get_private(struct usbnet *dev)
{
	return (struct sierra_net_data *)dev->data[0];
}

static inline void sierra_net_set_private(struct usbnet *dev,
			struct sierra_net_data *priv)
{
	dev->data[0] = (unsigned long)priv;
}

static inline int is_ip(struct sk_buff *skb)
{
	return skb->protocol == cpu_to_be16(ETH_P_IP);
}

static int check_ethip_packet(struct sk_buff *skb, struct usbnet *dev)
{
	skb_reset_mac_header(skb); 

	if (skb_is_nonlinear(skb)) {
		netdev_err(dev->net, "Non linear buffer-dropping\n");
		return 0;
	}

	if (!pskb_may_pull(skb, ETH_HLEN))
		return 0;
	skb->protocol = eth_hdr(skb)->h_proto;

	return 1;
}

static const u8 *save16bit(struct param *p, const u8 *datap)
{
	p->is_present = 1;
	p->word = get_unaligned_be16(datap);
	return datap + sizeof(p->word);
}

static const u8 *save8bit(struct param *p, const u8 *datap)
{
	p->is_present = 1;
	p->byte = *datap;
	return datap + sizeof(p->byte);
}

#define SIERRA_NET_HIP_HDR_LEN 4
#define SIERRA_NET_HIP_EXT_HDR_LEN 6

struct hip_hdr {
	int    hdrlen;
	struct param payload_len;
	struct param msgid;
	struct param msgspecific;
	struct param extmsgid;
};

static int parse_hip(const u8 *buf, const u32 buflen, struct hip_hdr *hh)
{
	const u8 *curp = buf;
	int    padded;

	if (buflen < SIERRA_NET_HIP_HDR_LEN)
		return -EPROTO;

	curp = save16bit(&hh->payload_len, curp);
	curp = save8bit(&hh->msgid, curp);
	curp = save8bit(&hh->msgspecific, curp);

	padded = hh->msgid.byte & 0x80;
	hh->msgid.byte &= 0x7F;			

	hh->extmsgid.is_present = (hh->msgid.byte == SIERRA_NET_HIP_EXTENDEDID);
	if (hh->extmsgid.is_present) {
		if (buflen < SIERRA_NET_HIP_EXT_HDR_LEN)
			return -EPROTO;

		hh->payload_len.word &= 0x3FFF; 

		curp = save16bit(&hh->extmsgid, curp);
		hh->extmsgid.word &= 0x03FF;	

		hh->hdrlen = SIERRA_NET_HIP_EXT_HDR_LEN;
	} else {
		hh->payload_len.word &= 0x07FF;	
		hh->hdrlen = SIERRA_NET_HIP_HDR_LEN;
	}

	if (padded) {
		hh->hdrlen++;
		hh->payload_len.word--;
	}

	
	if (buflen < (hh->hdrlen + hh->payload_len.word))
		return -EINVAL;

	return 0;
}

static void build_hip(u8 *buf, const u16 payloadlen,
		struct sierra_net_data *priv)
{
	put_unaligned_be16(payloadlen, buf);
	memcpy(buf+2, priv->tx_hdr_template, sizeof(priv->tx_hdr_template));
}

static int sierra_net_send_cmd(struct usbnet *dev,
		u8 *cmd, int cmdlen, const char * cmd_name)
{
	struct sierra_net_data *priv = sierra_net_get_private(dev);
	int  status;

	status = usb_control_msg(dev->udev, usb_sndctrlpipe(dev->udev, 0),
			USB_CDC_SEND_ENCAPSULATED_COMMAND,
			USB_DIR_OUT|USB_TYPE_CLASS|USB_RECIP_INTERFACE,	0,
			priv->ifnum, cmd, cmdlen, USB_CTRL_SET_TIMEOUT);

	if (status != cmdlen && status != -ENODEV)
		netdev_err(dev->net, "Submit %s failed %d\n", cmd_name, status);

	return status;
}

static int sierra_net_send_sync(struct usbnet *dev)
{
	int  status;
	struct sierra_net_data *priv = sierra_net_get_private(dev);

	dev_dbg(&dev->udev->dev, "%s", __func__);

	status = sierra_net_send_cmd(dev, priv->sync_msg,
			sizeof(priv->sync_msg), "SYNC");

	return status;
}

static void sierra_net_set_ctx_index(struct sierra_net_data *priv, u8 ctx_ix)
{
	dev_dbg(&(priv->usbnet->udev->dev), "%s %d", __func__, ctx_ix);
	priv->tx_hdr_template[0] = 0x3F;
	priv->tx_hdr_template[1] = ctx_ix;
	*((u16 *)&priv->tx_hdr_template[2]) =
		cpu_to_be16(SIERRA_NET_HIP_EXT_IP_OUT_ID);
}

static inline int sierra_net_is_valid_addrlen(u8 len)
{
	return len == sizeof(struct in_addr);
}

static int sierra_net_parse_lsi(struct usbnet *dev, char *data, int datalen)
{
	struct lsi_umts *lsi = (struct lsi_umts *)data;

	if (datalen < sizeof(struct lsi_umts)) {
		netdev_err(dev->net, "%s: Data length %d, exp %Zu\n",
				__func__, datalen,
				sizeof(struct lsi_umts));
		return -1;
	}

	if (lsi->length != cpu_to_be16(SIERRA_NET_LSI_UMTS_STATUS_LEN)) {
		netdev_err(dev->net, "%s: LSI_UMTS_STATUS_LEN %d, exp %u\n",
				__func__, be16_to_cpu(lsi->length),
				(u32)SIERRA_NET_LSI_UMTS_STATUS_LEN);
		return -1;
	}

	
	if (lsi->protocol != SIERRA_NET_PROTOCOL_UMTS) {
		netdev_err(dev->net, "Protocol unsupported, 0x%02x\n",
			lsi->protocol);
		return -1;
	}

	
	if (lsi->link_type != SIERRA_NET_AS_LINK_TYPE_IPv4) {
		netdev_err(dev->net, "Link type unsupported: 0x%02x\n",
			lsi->link_type);
		return -1;
	}

	
	if (lsi->coverage == SIERRA_NET_COVERAGE_NONE
	   || lsi->coverage == SIERRA_NET_COVERAGE_NOPACKET) {
		netdev_err(dev->net, "No coverage, 0x%02x\n", lsi->coverage);
		return 0;
	}

	
	if (lsi->session_state == SIERRA_NET_SESSION_IDLE) {
		netdev_err(dev->net, "Session idle, 0x%02x\n",
			lsi->session_state);
		return 0;
	}

	
	return 1;
}

static void sierra_net_handle_lsi(struct usbnet *dev, char *data,
		struct hip_hdr	*hh)
{
	struct sierra_net_data *priv = sierra_net_get_private(dev);
	int link_up;

	link_up = sierra_net_parse_lsi(dev, data + hh->hdrlen,
					hh->payload_len.word);
	if (link_up < 0) {
		netdev_err(dev->net, "Invalid LSI\n");
		return;
	}
	if (link_up) {
		sierra_net_set_ctx_index(priv, hh->msgspecific.byte);
		priv->link_up = 1;
		netif_carrier_on(dev->net);
	} else {
		priv->link_up = 0;
		netif_carrier_off(dev->net);
	}
}

static void sierra_net_dosync(struct usbnet *dev)
{
	int status;
	struct sierra_net_data *priv = sierra_net_get_private(dev);

	dev_dbg(&dev->udev->dev, "%s", __func__);

	
	status = sierra_net_send_sync(dev);
	if (status < 0)
		netdev_err(dev->net,
			"Send SYNC failed, status %d\n", status);
	status = sierra_net_send_sync(dev);
	if (status < 0)
		netdev_err(dev->net,
			"Send SYNC failed, status %d\n", status);

	
	priv->sync_timer.function = sierra_sync_timer;
	priv->sync_timer.data = (unsigned long) dev;
	priv->sync_timer.expires = jiffies + SIERRA_NET_SYNCDELAY;
	add_timer(&priv->sync_timer);
}

static void sierra_net_kevent(struct work_struct *work)
{
	struct sierra_net_data *priv =
		container_of(work, struct sierra_net_data, sierra_net_kevent);
	struct usbnet *dev = priv->usbnet;
	int  len;
	int  err;
	u8  *buf;
	u8   ifnum;

	if (test_bit(SIERRA_NET_EVENT_RESP_AVAIL, &priv->kevent_flags)) {
		clear_bit(SIERRA_NET_EVENT_RESP_AVAIL, &priv->kevent_flags);

		
		buf = kzalloc(SIERRA_NET_USBCTL_BUF_LEN, GFP_KERNEL);
		if (!buf) {
			netdev_err(dev->net,
				"failed to allocate buf for LS msg\n");
			return;
		}
		ifnum = priv->ifnum;
		len = usb_control_msg(dev->udev, usb_rcvctrlpipe(dev->udev, 0),
				USB_CDC_GET_ENCAPSULATED_RESPONSE,
				USB_DIR_IN|USB_TYPE_CLASS|USB_RECIP_INTERFACE,
				0, ifnum, buf, SIERRA_NET_USBCTL_BUF_LEN,
				USB_CTRL_SET_TIMEOUT);

		if (len < 0) {
			netdev_err(dev->net,
				"usb_control_msg failed, status %d\n", len);
		} else {
			struct hip_hdr	hh;

			dev_dbg(&dev->udev->dev, "%s: Received status message,"
				" %04x bytes", __func__, len);

			err = parse_hip(buf, len, &hh);
			if (err) {
				netdev_err(dev->net, "%s: Bad packet,"
					" parse result %d\n", __func__, err);
				kfree(buf);
				return;
			}

			
			if (len != hh.hdrlen + hh.payload_len.word) {
				netdev_err(dev->net, "%s: Bad packet, received"
					" %d, expected %d\n",	__func__, len,
					hh.hdrlen + hh.payload_len.word);
				kfree(buf);
				return;
			}

			
			switch (hh.msgid.byte) {
			case SIERRA_NET_HIP_LSI_UMTSID:
				dev_dbg(&dev->udev->dev, "LSI for ctx:%d",
					hh.msgspecific.byte);
				sierra_net_handle_lsi(dev, buf, &hh);
				break;
			case SIERRA_NET_HIP_RESTART_ID:
				dev_dbg(&dev->udev->dev, "Restart reported: %d,"
						" stopping sync timer",
						hh.msgspecific.byte);
				
				del_timer_sync(&priv->sync_timer);
				clear_bit(SIERRA_NET_TIMER_EXPIRY,
					  &priv->kevent_flags);
				break;
			case SIERRA_NET_HIP_HSYNC_ID:
				dev_dbg(&dev->udev->dev, "SYNC received");
				err = sierra_net_send_sync(dev);
				if (err < 0)
					netdev_err(dev->net,
						"Send SYNC failed %d\n", err);
				break;
			case SIERRA_NET_HIP_EXTENDEDID:
				netdev_err(dev->net, "Unrecognized HIP msg, "
					"extmsgid 0x%04x\n", hh.extmsgid.word);
				break;
			case SIERRA_NET_HIP_RCGI:
				
				break;
			default:
				netdev_err(dev->net, "Unrecognized HIP msg, "
					"msgid 0x%02x\n", hh.msgid.byte);
				break;
			}
		}
		kfree(buf);
	}
	
	if (test_bit(SIERRA_NET_TIMER_EXPIRY, &priv->kevent_flags)) {
		clear_bit(SIERRA_NET_TIMER_EXPIRY, &priv->kevent_flags);
		dev_dbg(&dev->udev->dev, "Deferred sync timer expiry");
		sierra_net_dosync(priv->usbnet);
	}

	if (priv->kevent_flags)
		dev_dbg(&dev->udev->dev, "sierra_net_kevent done, "
			"kevent_flags = 0x%lx", priv->kevent_flags);
}

static void sierra_net_defer_kevent(struct usbnet *dev, int work)
{
	struct sierra_net_data *priv = sierra_net_get_private(dev);

	set_bit(work, &priv->kevent_flags);
	schedule_work(&priv->sierra_net_kevent);
}

void sierra_sync_timer(unsigned long syncdata)
{
	struct usbnet *dev = (struct usbnet *)syncdata;

	dev_dbg(&dev->udev->dev, "%s", __func__);
	
	sierra_net_defer_kevent(dev, SIERRA_NET_TIMER_EXPIRY);
}

static void sierra_net_status(struct usbnet *dev, struct urb *urb)
{
	struct usb_cdc_notification *event;

	dev_dbg(&dev->udev->dev, "%s", __func__);

	if (urb->actual_length < sizeof *event)
		return;

	
	event = urb->transfer_buffer;
	switch (event->bNotificationType) {
	case USB_CDC_NOTIFY_NETWORK_CONNECTION:
	case USB_CDC_NOTIFY_SPEED_CHANGE:
		
		break;
	case USB_CDC_NOTIFY_RESPONSE_AVAILABLE:
		sierra_net_defer_kevent(dev, SIERRA_NET_EVENT_RESP_AVAIL);
		break;
	default:
		netdev_err(dev->net, ": unexpected notification %02x!\n",
				event->bNotificationType);
		break;
	}
}

static void sierra_net_get_drvinfo(struct net_device *net,
		struct ethtool_drvinfo *info)
{
	
	usbnet_get_drvinfo(net, info);
	strncpy(info->driver, driver_name, sizeof info->driver);
	strncpy(info->version, DRIVER_VERSION, sizeof info->version);
}

static u32 sierra_net_get_link(struct net_device *net)
{
	struct usbnet *dev = netdev_priv(net);
	
	return sierra_net_get_private(dev)->link_up && netif_running(net);
}

static const struct ethtool_ops sierra_net_ethtool_ops = {
	.get_drvinfo = sierra_net_get_drvinfo,
	.get_link = sierra_net_get_link,
	.get_msglevel = usbnet_get_msglevel,
	.set_msglevel = usbnet_set_msglevel,
	.get_settings = usbnet_get_settings,
	.set_settings = usbnet_set_settings,
	.nway_reset = usbnet_nway_reset,
};

static int sierra_net_change_mtu(struct net_device *net, int new_mtu)
{
	if (new_mtu > SIERRA_NET_MAX_SUPPORTED_MTU)
		return -EINVAL;

	return usbnet_change_mtu(net, new_mtu);
}

static int is_whitelisted(const u8 ifnum,
			const struct sierra_net_iface_info *whitelist)
{
	if (whitelist) {
		const u8 *list = whitelist->ifaceinfo;
		int i;

		for (i = 0; i < whitelist->infolen; i++) {
			if (list[i] == ifnum)
				return 1;
		}
	}
	return 0;
}

static int sierra_net_get_fw_attr(struct usbnet *dev, u16 *datap)
{
	int result = 0;
	u16 *attrdata;

	attrdata = kmalloc(sizeof(*attrdata), GFP_KERNEL);
	if (!attrdata)
		return -ENOMEM;

	result = usb_control_msg(
			dev->udev,
			usb_rcvctrlpipe(dev->udev, 0),
			
			SWI_USB_REQUEST_GET_FW_ATTR,
			USB_DIR_IN | USB_TYPE_VENDOR,	
			0x0000,		
			0x0000,		
			attrdata,	
			sizeof(*attrdata),		
			USB_CTRL_SET_TIMEOUT);	

	if (result < 0) {
		kfree(attrdata);
		return -EIO;
	}

	*datap = *attrdata;

	kfree(attrdata);
	return result;
}

static int sierra_net_bind(struct usbnet *dev, struct usb_interface *intf)
{
	u8	ifacenum;
	u8	numendpoints;
	u16	fwattr = 0;
	int	status;
	struct ethhdr *eth;
	struct sierra_net_data *priv;
	static const u8 sync_tmplate[sizeof(priv->sync_msg)] = {
		0x00, 0x00, SIERRA_NET_HIP_MSYNC_ID, 0x00};
	static const u8 shdwn_tmplate[sizeof(priv->shdwn_msg)] = {
		0x00, 0x00, SIERRA_NET_HIP_SHUTD_ID, 0x00};

	struct sierra_net_info_data *data =
			(struct sierra_net_info_data *)dev->driver_info->data;

	dev_dbg(&dev->udev->dev, "%s", __func__);

	ifacenum = intf->cur_altsetting->desc.bInterfaceNumber;
	
	if (!is_whitelisted(ifacenum, &data->whitelist)) {
		dev_dbg(&dev->udev->dev, "Ignoring interface: %d", ifacenum);
		return -ENODEV;
	}
	numendpoints = intf->cur_altsetting->desc.bNumEndpoints;
	
	if (numendpoints != 3) {
		dev_err(&dev->udev->dev, "Expected 3 endpoints, found: %d",
			numendpoints);
		return -ENODEV;
	}
	
	dev->status = NULL;
	status = usbnet_get_endpoints(dev, intf);
	if (status < 0) {
		dev_err(&dev->udev->dev, "Error in usbnet_get_endpoints (%d)",
			status);
		return -ENODEV;
	}
	
	priv = kzalloc(sizeof *priv, GFP_KERNEL);
	if (!priv) {
		dev_err(&dev->udev->dev, "No memory");
		return -ENOMEM;
	}

	priv->usbnet = dev;
	priv->ifnum = ifacenum;
	dev->net->netdev_ops = &sierra_net_device_ops;

	
	dev->net->dev_addr[ETH_ALEN-2] = atomic_inc_return(&iface_counter);
	dev->net->dev_addr[ETH_ALEN-1] = ifacenum;

	
	eth = (struct ethhdr *)priv->ethr_hdr_tmpl;
	memcpy(&eth->h_dest, dev->net->dev_addr, ETH_ALEN);
	eth->h_proto = cpu_to_be16(ETH_P_IP);

	
	memcpy(priv->shdwn_msg, shdwn_tmplate, sizeof(priv->shdwn_msg));
	
	sierra_net_set_ctx_index(priv, 0);

	
	dev->rx_urb_size  = data->rx_urb_size;
	if (dev->udev->speed != USB_SPEED_HIGH)
		dev->rx_urb_size  = min_t(size_t, 4096, data->rx_urb_size);

	dev->net->hard_header_len += SIERRA_NET_HIP_EXT_HDR_LEN;
	dev->hard_mtu = dev->net->mtu + dev->net->hard_header_len;

	
	dev->net->flags |= IFF_NOARP;
	dev->net->ethtool_ops = &sierra_net_ethtool_ops;
	netif_carrier_off(dev->net);

	sierra_net_set_private(dev, priv);

	priv->kevent_flags = 0;

	
	INIT_WORK(&priv->sierra_net_kevent, sierra_net_kevent);

	
	init_timer(&priv->sync_timer);

	
	status = sierra_net_get_fw_attr(dev, &fwattr);
	dev_dbg(&dev->udev->dev, "Fw attr: %x\n", fwattr);

	
	if (!(status == sizeof(fwattr) && (fwattr & SWI_GET_FW_ATTR_MASK))) {
		
		dev_err(&dev->udev->dev, "Incompatible driver and firmware"
			" versions\n");
		kfree(priv);
		return -ENODEV;
	}
	
	memcpy(priv->sync_msg, sync_tmplate, sizeof(priv->sync_msg));

	
	sierra_net_dosync(dev);

	return 0;
}

static void sierra_net_unbind(struct usbnet *dev, struct usb_interface *intf)
{
	int status;
	struct sierra_net_data *priv = sierra_net_get_private(dev);

	dev_dbg(&dev->udev->dev, "%s", __func__);

	
	del_timer_sync(&priv->sync_timer);
	cancel_work_sync(&priv->sierra_net_kevent);

	
	status = sierra_net_send_cmd(dev, priv->shdwn_msg,
			sizeof(priv->shdwn_msg), "Shutdown");
	if (status < 0)
		netdev_err(dev->net,
			"usb_control_msg failed, status %d\n", status);

	sierra_net_set_private(dev, NULL);

	kfree(priv);
}

static struct sk_buff *sierra_net_skb_clone(struct usbnet *dev,
		struct sk_buff *skb, int len)
{
	struct sk_buff *new_skb;

	
	new_skb = skb_clone(skb, GFP_ATOMIC);

	
	skb_pull(skb, len);

	
	if (new_skb) {
		skb_trim(new_skb, len);
	} else {
		if (netif_msg_rx_err(dev))
			netdev_err(dev->net, "failed to get skb\n");
		dev->net->stats.rx_dropped++;
	}

	return new_skb;
}

static int sierra_net_rx_fixup(struct usbnet *dev, struct sk_buff *skb)
{
	int err;
	struct hip_hdr  hh;
	struct sk_buff *new_skb;

	dev_dbg(&dev->udev->dev, "%s", __func__);

	
	while (likely(skb->len)) {
		err = parse_hip(skb->data, skb->len, &hh);
		if (err) {
			if (netif_msg_rx_err(dev))
				netdev_err(dev->net, "Invalid HIP header %d\n",
					err);
			
			dev->net->stats.rx_length_errors++;
			return 0;
		}

		
		if (!hh.extmsgid.is_present
		    || hh.extmsgid.word != SIERRA_NET_HIP_EXT_IP_IN_ID) {
			if (netif_msg_rx_err(dev))
				netdev_err(dev->net, "HIP/ETH: Invalid pkt\n");

			dev->net->stats.rx_frame_errors++;
			;
			return 0;
		}

		skb_pull(skb, hh.hdrlen);

		
		memcpy(skb->data, sierra_net_get_private(dev)->ethr_hdr_tmpl,
			ETH_HLEN);

		
		if (hh.payload_len.word == skb->len)
			return 1;

		new_skb = sierra_net_skb_clone(dev, skb, hh.payload_len.word);
		if (new_skb)
			usbnet_skb_return(dev, new_skb);

	} 

	return 0;
}

struct sk_buff *sierra_net_tx_fixup(struct usbnet *dev, struct sk_buff *skb,
		gfp_t flags)
{
	struct sierra_net_data *priv = sierra_net_get_private(dev);
	u16 len;
	bool need_tail;

	BUILD_BUG_ON(FIELD_SIZEOF(struct usbnet, data)
				< sizeof(struct cdc_state));

	dev_dbg(&dev->udev->dev, "%s", __func__);
	if (priv->link_up && check_ethip_packet(skb, dev) && is_ip(skb)) {
		
		if (SIERRA_NET_HIP_EXT_HDR_LEN <= skb_headroom(skb)) {
			
			len = skb->len;
			skb_push(skb, SIERRA_NET_HIP_EXT_HDR_LEN);
			
			need_tail = ((len + SIERRA_NET_HIP_EXT_HDR_LEN)
				% dev->maxpacket == 0);
			if (need_tail) {
				if (unlikely(skb_tailroom(skb) == 0)) {
					netdev_err(dev->net, "tx_fixup:"
						"no room for packet\n");
					dev_kfree_skb_any(skb);
					return NULL;
				} else {
					skb->data[skb->len] = 0;
					__skb_put(skb, 1);
					len = len + 1;
				}
			}
			build_hip(skb->data, len, priv);
			return skb;
		} else {
			netdev_err(dev->net, "tx_fixup: no room for HIP\n");
		} 
	}

	if (!priv->link_up)
		dev->net->stats.tx_carrier_errors++;

	

	
	dev_kfree_skb_any(skb);
	return NULL;
}

static const u8 sierra_net_ifnum_list[] = { 7, 10, 11 };
static const struct sierra_net_info_data sierra_net_info_data_68A3 = {
	.rx_urb_size = 8 * 1024,
	.whitelist = {
		.infolen = ARRAY_SIZE(sierra_net_ifnum_list),
		.ifaceinfo = sierra_net_ifnum_list
	}
};

static const struct driver_info sierra_net_info_68A3 = {
	.description = "Sierra Wireless USB-to-WWAN Modem",
	.flags = FLAG_WWAN | FLAG_SEND_ZLP,
	.bind = sierra_net_bind,
	.unbind = sierra_net_unbind,
	.status = sierra_net_status,
	.rx_fixup = sierra_net_rx_fixup,
	.tx_fixup = sierra_net_tx_fixup,
	.data = (unsigned long)&sierra_net_info_data_68A3,
};

static const struct usb_device_id products[] = {
	{USB_DEVICE(0x1199, 0x68A3), 
	.driver_info = (unsigned long) &sierra_net_info_68A3},

	{}, 
};
MODULE_DEVICE_TABLE(usb, products);

static struct usb_driver sierra_net_driver = {
	.name = "sierra_net",
	.id_table = products,
	.probe = usbnet_probe,
	.disconnect = usbnet_disconnect,
	.suspend = usbnet_suspend,
	.resume = usbnet_resume,
	.no_dynamic_id = 1,
};

module_usb_driver(sierra_net_driver);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");
