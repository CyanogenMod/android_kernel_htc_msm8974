/*
 * f_qc_rndis.c -- RNDIS link function driver
 *
 * Copyright (C) 2003-2005,2008 David Brownell
 * Copyright (C) 2003-2004 Robert Schwebel, Benedikt Spranger
 * Copyright (C) 2008 Nokia Corporation
 * Copyright (C) 2009 Samsung Electronics
 *			Author: Michal Nazarewicz (mina86@mina86.com)
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
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


#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/etherdevice.h>

#include <linux/atomic.h>

#include "u_ether.h"
#include "u_qc_ether.h"
#include "rndis.h"



struct f_rndis_qc {
	struct qc_gether			port;
	u8				ctrl_id, data_id;
	u8				ethaddr[ETH_ALEN];
	u32				vendorID;
	u8				max_pkt_per_xfer;
	u32				max_pkt_size;
	const char			*manufacturer;
	int				config;
	atomic_t		ioctl_excl;
	atomic_t		open_excl;

	struct usb_ep			*notify;
	struct usb_request		*notify_req;
	atomic_t			notify_count;
	struct data_port		bam_port;
};

static inline struct f_rndis_qc *func_to_rndis_qc(struct usb_function *f)
{
	return container_of(f, struct f_rndis_qc, port.func);
}

static unsigned int rndis_qc_bitrate(struct usb_gadget *g)
{
	if (gadget_is_superspeed(g) && g->speed == USB_SPEED_SUPER)
		return 13 * 1024 * 8 * 1000 * 8;
	else if (gadget_is_dualspeed(g) && g->speed == USB_SPEED_HIGH)
		return 13 * 512 * 8 * 1000 * 8;
	else
		return 19 * 64 * 1 * 1000 * 8;
}


#define RNDIS_QC_LOG2_STATUS_INTERVAL_MSEC	5	
#define RNDIS_QC_STATUS_BYTECOUNT		8	

#define RNDIS_QC_NO_PORTS				1
#define RNDIS_QC_ACTIVE_PORT				0

#define DEFAULT_MAX_PKT_PER_XFER			15


#define RNDIS_QC_IOCTL_MAGIC		'i'
#define RNDIS_QC_GET_MAX_PKT_PER_XFER   _IOR(RNDIS_QC_IOCTL_MAGIC, 1, u8)
#define RNDIS_QC_GET_MAX_PKT_SIZE	_IOR(RNDIS_QC_IOCTL_MAGIC, 2, u32)



static struct usb_interface_descriptor rndis_qc_control_intf = {
	.bLength =		sizeof rndis_qc_control_intf,
	.bDescriptorType =	USB_DT_INTERFACE,

	
	
	.bNumEndpoints =	1,
	.bInterfaceClass =	USB_CLASS_COMM,
	.bInterfaceSubClass =   USB_CDC_SUBCLASS_ACM,
	.bInterfaceProtocol =   USB_CDC_ACM_PROTO_VENDOR,
	
};

static struct usb_cdc_header_desc rndis_qc_header_desc = {
	.bLength =		sizeof rndis_qc_header_desc,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_HEADER_TYPE,

	.bcdCDC =		cpu_to_le16(0x0110),
};

static struct usb_cdc_call_mgmt_descriptor rndis_qc_call_mgmt_descriptor = {
	.bLength =		sizeof rndis_qc_call_mgmt_descriptor,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_CALL_MANAGEMENT_TYPE,

	.bmCapabilities =	0x00,
	.bDataInterface =	0x01,
};

static struct usb_cdc_acm_descriptor rndis_qc_acm_descriptor = {
	.bLength =		sizeof rndis_qc_acm_descriptor,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_ACM_TYPE,

	.bmCapabilities =	0x00,
};

static struct usb_cdc_union_desc rndis_qc_union_desc = {
	.bLength =		sizeof(rndis_qc_union_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_UNION_TYPE,
	
	
};


static struct usb_interface_descriptor rndis_qc_data_intf = {
	.bLength =		sizeof rndis_qc_data_intf,
	.bDescriptorType =	USB_DT_INTERFACE,

	
	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_CDC_DATA,
	.bInterfaceSubClass =	0,
	.bInterfaceProtocol =	0,
	
};


static struct usb_interface_assoc_descriptor
rndis_qc_iad_descriptor = {
	.bLength =		sizeof rndis_qc_iad_descriptor,
	.bDescriptorType =	USB_DT_INTERFACE_ASSOCIATION,
	.bFirstInterface =	0, 
	.bInterfaceCount =	2, 
	.bFunctionClass =	USB_CLASS_COMM,
	.bFunctionSubClass =	USB_CDC_SUBCLASS_ETHERNET,
	.bFunctionProtocol =	USB_CDC_PROTO_NONE,
	
};


static struct usb_endpoint_descriptor rndis_qc_fs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(RNDIS_QC_STATUS_BYTECOUNT),
	.bInterval =		1 << RNDIS_QC_LOG2_STATUS_INTERVAL_MSEC,
};

static struct usb_endpoint_descriptor rndis_qc_fs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor rndis_qc_fs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *eth_qc_fs_function[] = {
	(struct usb_descriptor_header *) &rndis_qc_iad_descriptor,
	
	(struct usb_descriptor_header *) &rndis_qc_control_intf,
	(struct usb_descriptor_header *) &rndis_qc_header_desc,
	(struct usb_descriptor_header *) &rndis_qc_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &rndis_qc_acm_descriptor,
	(struct usb_descriptor_header *) &rndis_qc_union_desc,
	(struct usb_descriptor_header *) &rndis_qc_fs_notify_desc,
	
	(struct usb_descriptor_header *) &rndis_qc_data_intf,
	(struct usb_descriptor_header *) &rndis_qc_fs_in_desc,
	(struct usb_descriptor_header *) &rndis_qc_fs_out_desc,
	NULL,
};


static struct usb_endpoint_descriptor rndis_qc_hs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(RNDIS_QC_STATUS_BYTECOUNT),
	.bInterval =		RNDIS_QC_LOG2_STATUS_INTERVAL_MSEC + 4,
};
static struct usb_endpoint_descriptor rndis_qc_hs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor rndis_qc_hs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *eth_qc_hs_function[] = {
	(struct usb_descriptor_header *) &rndis_qc_iad_descriptor,
	
	(struct usb_descriptor_header *) &rndis_qc_control_intf,
	(struct usb_descriptor_header *) &rndis_qc_header_desc,
	(struct usb_descriptor_header *) &rndis_qc_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &rndis_qc_acm_descriptor,
	(struct usb_descriptor_header *) &rndis_qc_union_desc,
	(struct usb_descriptor_header *) &rndis_qc_hs_notify_desc,
	
	(struct usb_descriptor_header *) &rndis_qc_data_intf,
	(struct usb_descriptor_header *) &rndis_qc_hs_in_desc,
	(struct usb_descriptor_header *) &rndis_qc_hs_out_desc,
	NULL,
};


static struct usb_endpoint_descriptor rndis_qc_ss_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(STATUS_BYTECOUNT),
	.bInterval =		LOG2_STATUS_INTERVAL_MSEC + 4,
};

static struct usb_ss_ep_comp_descriptor rndis_qc_ss_intr_comp_desc = {
	.bLength =		sizeof ss_intr_comp_desc,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	
	
	
	.wBytesPerInterval =	cpu_to_le16(STATUS_BYTECOUNT),
};

static struct usb_endpoint_descriptor rndis_qc_ss_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor rndis_qc_ss_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor rndis_qc_ss_bulk_comp_desc = {
	.bLength =		sizeof ss_bulk_comp_desc,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	
	
	
};

static struct usb_descriptor_header *eth_qc_ss_function[] = {
	(struct usb_descriptor_header *) &rndis_iad_descriptor,

	
	(struct usb_descriptor_header *) &rndis_qc_control_intf,
	(struct usb_descriptor_header *) &rndis_qc_header_desc,
	(struct usb_descriptor_header *) &rndis_qc_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &rndis_qc_acm_descriptor,
	(struct usb_descriptor_header *) &rndis_qc_union_desc,
	(struct usb_descriptor_header *) &rndis_qc_ss_notify_desc,
	(struct usb_descriptor_header *) &rndis_qc_ss_intr_comp_desc,

	
	(struct usb_descriptor_header *) &rndis_qc_data_intf,
	(struct usb_descriptor_header *) &rndis_qc_ss_in_desc,
	(struct usb_descriptor_header *) &rndis_qc_ss_bulk_comp_desc,
	(struct usb_descriptor_header *) &rndis_qc_ss_out_desc,
	(struct usb_descriptor_header *) &rndis_qc_ss_bulk_comp_desc,
	NULL,
};


static struct usb_string rndis_qc_string_defs[] = {
	[0].s = "RNDIS Communications Control",
	[1].s = "RNDIS Ethernet Data",
	[2].s = "RNDIS",
	{  } 
};

static struct usb_gadget_strings rndis_qc_string_table = {
	.language =		0x0409,	
	.strings =		rndis_qc_string_defs,
};

static struct usb_gadget_strings *rndis_qc_strings[] = {
	&rndis_qc_string_table,
	NULL,
};

struct f_rndis_qc *_rndis_qc;

static inline int rndis_qc_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -EBUSY;
	}
}

static inline void rndis_qc_unlock(atomic_t *excl)
{
	atomic_dec(excl);
}


static int rndis_qc_bam_setup(void)
{
	int ret;

	ret = bam_data_setup(RNDIS_QC_NO_PORTS);
	if (ret) {
		pr_err("bam_data_setup failed err: %d\n", ret);
		return ret;
	}

	return 0;
}

static int rndis_qc_bam_connect(struct f_rndis_qc *dev)
{
	int ret;
	u8 src_connection_idx, dst_connection_idx;
	struct usb_composite_dev *cdev = dev->port.func.config->cdev;
	struct usb_gadget *gadget = cdev->gadget;

	dev->bam_port.cdev = cdev;
	dev->bam_port.func = &dev->port.func;
	dev->bam_port.in = dev->port.in_ep;
	dev->bam_port.out = dev->port.out_ep;

	
	src_connection_idx = usb_bam_get_connection_idx(gadget->name, A2_P_BAM,
		USB_TO_PEER_PERIPHERAL, 0);
	dst_connection_idx = usb_bam_get_connection_idx(gadget->name, A2_P_BAM,
		PEER_PERIPHERAL_TO_USB, 0);
	if (src_connection_idx < 0 || dst_connection_idx < 0) {
		pr_err("%s: usb_bam_get_connection_idx failed\n", __func__);
		return ret;
	}
	ret = bam_data_connect(&dev->bam_port, 0, USB_GADGET_XPORT_BAM2BAM,
		src_connection_idx, dst_connection_idx, USB_FUNC_RNDIS);
	if (ret) {
		pr_err("bam_data_connect failed: err:%d\n",
				ret);
		return ret;
	}

	pr_info("rndis bam connected\n");

	return 0;
}

static int rndis_qc_bam_disconnect(struct f_rndis_qc *dev)
{
	pr_debug("dev:%p. %s Disconnect BAM.\n", dev, __func__);

	bam_data_disconnect(&dev->bam_port, 0);

	return 0;
}


static struct sk_buff *rndis_qc_add_header(struct qc_gether *port,
					struct sk_buff *skb)
{
	struct sk_buff *skb2;

	skb2 = skb_realloc_headroom(skb, sizeof(struct rndis_packet_msg_type));
	if (skb2)
		rndis_add_hdr(skb2);

	dev_kfree_skb_any(skb);
	return skb2;
}

int rndis_qc_rm_hdr(struct qc_gether *port,
			struct sk_buff *skb,
			struct sk_buff_head *list)
{
	
	__le32 *tmp = (void *)skb->data;

	
	if (cpu_to_le32(REMOTE_NDIS_PACKET_MSG)
			!= get_unaligned(tmp++)) {
		dev_kfree_skb_any(skb);
		return -EINVAL;
	}
	tmp++;

	
	if (!skb_pull(skb, get_unaligned_le32(tmp++) + 8)) {
		dev_kfree_skb_any(skb);
		return -EOVERFLOW;
	}
	skb_trim(skb, get_unaligned_le32(tmp++));

	skb_queue_tail(list, skb);
	return 0;
}


static void rndis_qc_response_available(void *_rndis)
{
	struct f_rndis_qc			*rndis = _rndis;
	struct usb_request		*req = rndis->notify_req;
	__le32				*data = req->buf;
	int				status;

	if (atomic_inc_return(&rndis->notify_count) != 1)
		return;

	data[0] = cpu_to_le32(1);
	data[1] = cpu_to_le32(0);

	status = usb_ep_queue(rndis->notify, req, GFP_ATOMIC);
	if (status) {
		atomic_dec(&rndis->notify_count);
		pr_info("notify/0 --> %d\n", status);
	}
}

static void rndis_qc_response_complete(struct usb_ep *ep,
						struct usb_request *req)
{
	struct f_rndis_qc		*rndis = req->context;
	int				status = req->status;
	struct usb_composite_dev	*cdev = rndis->port.func.config->cdev;

	switch (status) {
	case -ECONNRESET:
	case -ESHUTDOWN:
		
		atomic_set(&rndis->notify_count, 0);
		break;
	default:
		pr_info("RNDIS %s response error %d, %d/%d\n",
			ep->name, status,
			req->actual, req->length);
		
	case 0:
		if (ep != rndis->notify)
			break;

		if (atomic_dec_and_test(&rndis->notify_count))
			break;
		status = usb_ep_queue(rndis->notify, req, GFP_ATOMIC);
		if (status) {
			atomic_dec(&rndis->notify_count);
			DBG(cdev, "notify/1 --> %d\n", status);
		}
		break;
	}
}

static void rndis_qc_command_complete(struct usb_ep *ep,
							struct usb_request *req)
{
	struct f_rndis_qc		*rndis = req->context;
	int				status;
	rndis_init_msg_type		*buf;

	
	status = rndis_msg_parser(rndis->config, (u8 *) req->buf);
	if (status < 0)
		pr_err("RNDIS command error %d, %d/%d\n",
			status, req->actual, req->length);

	buf = (rndis_init_msg_type *)req->buf;

	if (buf->MessageType == REMOTE_NDIS_INITIALIZE_MSG) {
		rndis->max_pkt_size = buf->MaxTransferSize;
		pr_debug("MaxTransferSize: %d\n", buf->MaxTransferSize);
	}
}

static int
rndis_qc_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct f_rndis_qc		*rndis = func_to_rndis_qc(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);

	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {

	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_SEND_ENCAPSULATED_COMMAND:
		if (w_value || w_index != rndis->ctrl_id)
			goto invalid;
		
		value = w_length;
		req->complete = rndis_qc_command_complete;
		req->context = rndis;
		
		break;

	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_GET_ENCAPSULATED_RESPONSE:
		if (w_value || w_index != rndis->ctrl_id)
			goto invalid;
		else {
			u8 *buf;
			u32 n;

			
			buf = rndis_get_next_response(rndis->config, &n);
			if (buf) {
				memcpy(req->buf, buf, n);
				req->complete = rndis_qc_response_complete;
				rndis_free_response(rndis->config, buf);
				value = n;
			}
			
		}
		break;

	default:
invalid:
		VDBG(cdev, "invalid control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	
	if (value >= 0) {
		DBG(cdev, "rndis req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
		req->zero = (value < w_length);
		req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0)
			pr_err("rndis response on err %d\n", value);
	}

	
	return value;
}


static int rndis_qc_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_rndis_qc		*rndis = func_to_rndis_qc(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	

	if (intf == rndis->ctrl_id) {
		if (rndis->notify->driver_data) {
			VDBG(cdev, "reset rndis control %d\n", intf);
			usb_ep_disable(rndis->notify);
		}
		if (!rndis->notify->desc) {
			VDBG(cdev, "init rndis ctrl %d\n", intf);
			if (config_ep_by_speed(cdev->gadget, f, rndis->notify))
				goto fail;
		}
		usb_ep_enable(rndis->notify);
		rndis->notify->driver_data = rndis;

	} else if (intf == rndis->data_id) {
		struct net_device	*net;

		if (rndis->port.in_ep->driver_data) {
			DBG(cdev, "reset rndis\n");
			rndis_qc_bam_disconnect(rndis);
			gether_qc_disconnect_name(&rndis->port, "rndis0");
		}

		if (!rndis->port.in_ep->desc || !rndis->port.out_ep->desc) {
			DBG(cdev, "init rndis\n");
			if (config_ep_by_speed(cdev->gadget, f,
					       rndis->port.in_ep) ||
			    config_ep_by_speed(cdev->gadget, f,
					       rndis->port.out_ep)) {
				rndis->port.in_ep->desc = NULL;
				rndis->port.out_ep->desc = NULL;
				goto fail;
			}
		}

		
		rndis->port.is_zlp_ok = false;

		rndis->port.cdc_filter = 0;

		if (rndis_qc_bam_connect(rndis))
			goto fail;

		DBG(cdev, "RNDIS RX/TX early activation ...\n");
		net = gether_qc_connect_name(&rndis->port, "rndis0", false);
		if (IS_ERR(net))
			return PTR_ERR(net);

		rndis_set_param_dev(rndis->config, net,
				&rndis->port.cdc_filter);
	} else
		goto fail;

	return 0;
fail:
	return -EINVAL;
}

static void rndis_qc_disable(struct usb_function *f)
{
	struct f_rndis_qc		*rndis = func_to_rndis_qc(f);

	if (!rndis->notify->driver_data)
		return;

	pr_info("rndis deactivated\n");

	rndis_uninit(rndis->config);
	rndis_qc_bam_disconnect(rndis);
	gether_qc_disconnect_name(&rndis->port, "rndis0");

	usb_ep_disable(rndis->notify);
	rndis->notify->driver_data = NULL;
}

static void rndis_qc_suspend(struct usb_function *f)
{
	pr_debug("%s: rndis suspended\n", __func__);

	bam_data_suspend(RNDIS_QC_ACTIVE_PORT);
}

static void rndis_qc_resume(struct usb_function *f)
{
	pr_debug("%s: rndis resumed\n", __func__);

	bam_data_resume(RNDIS_QC_ACTIVE_PORT);
}



static void rndis_qc_open(struct qc_gether *geth)
{
	struct f_rndis_qc		*rndis = func_to_rndis_qc(&geth->func);
	struct usb_composite_dev *cdev = geth->func.config->cdev;

	DBG(cdev, "%s\n", __func__);

	rndis_set_param_medium(rndis->config, NDIS_MEDIUM_802_3,
				rndis_qc_bitrate(cdev->gadget) / 100);
	rndis_signal_connect(rndis->config);
}

static void rndis_qc_close(struct qc_gether *geth)
{
	struct f_rndis_qc		*rndis = func_to_rndis_qc(&geth->func);

	DBG(geth->func.config->cdev, "%s\n", __func__);

	rndis_set_param_medium(rndis->config, NDIS_MEDIUM_802_3, 0);
	rndis_signal_disconnect(rndis->config);
}



static int
rndis_qc_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_rndis_qc		*rndis = func_to_rndis_qc(f);
	int			status;
	struct usb_ep		*ep;

	
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	rndis->ctrl_id = status;
	rndis_qc_iad_descriptor.bFirstInterface = status;

	rndis_qc_control_intf.bInterfaceNumber = status;
	rndis_qc_union_desc.bMasterInterface0 = status;

	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	rndis->data_id = status;

	rndis_qc_data_intf.bInterfaceNumber = status;
	rndis_qc_union_desc.bSlaveInterface0 = status;

	status = -ENODEV;

	
	ep = usb_ep_autoconfig(cdev->gadget, &rndis_qc_fs_in_desc);
	if (!ep)
		goto fail;
	rndis->port.in_ep = ep;
	ep->driver_data = cdev;	

	ep = usb_ep_autoconfig(cdev->gadget, &rndis_qc_fs_out_desc);
	if (!ep)
		goto fail;
	rndis->port.out_ep = ep;
	ep->driver_data = cdev;	

	ep = usb_ep_autoconfig(cdev->gadget, &rndis_qc_fs_notify_desc);
	if (!ep)
		goto fail;
	rndis->notify = ep;
	ep->driver_data = cdev;	

	status = -ENOMEM;

	
	rndis->notify_req = usb_ep_alloc_request(ep, GFP_KERNEL);
	if (!rndis->notify_req)
		goto fail;
	rndis->notify_req->buf = kmalloc(RNDIS_QC_STATUS_BYTECOUNT, GFP_KERNEL);
	if (!rndis->notify_req->buf)
		goto fail;
	rndis->notify_req->length = RNDIS_QC_STATUS_BYTECOUNT;
	rndis->notify_req->context = rndis;
	rndis->notify_req->complete = rndis_qc_response_complete;

	
	f->descriptors = usb_copy_descriptors(eth_qc_fs_function);
	if (!f->descriptors)
		goto fail;

	if (gadget_is_dualspeed(c->cdev->gadget)) {
		rndis_qc_hs_in_desc.bEndpointAddress =
				rndis_qc_fs_in_desc.bEndpointAddress;
		rndis_qc_hs_out_desc.bEndpointAddress =
				rndis_qc_fs_out_desc.bEndpointAddress;
		rndis_qc_hs_notify_desc.bEndpointAddress =
				rndis_qc_fs_notify_desc.bEndpointAddress;

		
		f->hs_descriptors = usb_copy_descriptors(eth_qc_hs_function);

		if (!f->hs_descriptors)
			goto fail;
	}

	if (gadget_is_superspeed(c->cdev->gadget)) {
		rndis_qc_ss_in_desc.bEndpointAddress =
				rndis_qc_fs_in_desc.bEndpointAddress;
		rndis_qc_ss_out_desc.bEndpointAddress =
				rndis_qc_fs_out_desc.bEndpointAddress;
		rndis_qc_ss_notify_desc.bEndpointAddress =
				rndis_qc_fs_notify_desc.bEndpointAddress;

		
		f->ss_descriptors = usb_copy_descriptors(eth_qc_ss_function);
		if (!f->ss_descriptors)
			goto fail;
	}

	rndis->port.open = rndis_qc_open;
	rndis->port.close = rndis_qc_close;

	status = rndis_register(rndis_qc_response_available, rndis);
	if (status < 0)
		goto fail;
	rndis->config = status;

	rndis_set_param_medium(rndis->config, NDIS_MEDIUM_802_3, 0);
	rndis_set_host_mac(rndis->config, rndis->ethaddr);

	if (rndis_set_param_vendor(rndis->config, rndis->vendorID,
				   rndis->manufacturer))
			goto fail;

	rndis_set_max_pkt_xfer(rndis->config, rndis->max_pkt_per_xfer);

	rndis_set_pkt_alignment_factor(rndis->config, 2);


	DBG(cdev, "RNDIS: %s speed IN/%s OUT/%s NOTIFY/%s\n",
			gadget_is_superspeed(c->cdev->gadget) ? "super" :
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			rndis->port.in_ep->name, rndis->port.out_ep->name,
			rndis->notify->name);
	return 0;

fail:
	if (gadget_is_superspeed(c->cdev->gadget) && f->ss_descriptors)
		usb_free_descriptors(f->ss_descriptors);
	if (gadget_is_dualspeed(c->cdev->gadget) && f->hs_descriptors)
		usb_free_descriptors(f->hs_descriptors);
	if (f->descriptors)
		usb_free_descriptors(f->descriptors);

	if (rndis->notify_req) {
		kfree(rndis->notify_req->buf);
		usb_ep_free_request(rndis->notify, rndis->notify_req);
	}

	
	if (rndis->notify)
		rndis->notify->driver_data = NULL;
	if (rndis->port.out_ep->desc)
		rndis->port.out_ep->driver_data = NULL;
	if (rndis->port.in_ep->desc)
		rndis->port.in_ep->driver_data = NULL;

	pr_err("%s: can't bind, err %d\n", f->name, status);

	return status;
}

static void
rndis_qc_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_rndis_qc		*rndis = func_to_rndis_qc(f);

	pr_debug("rndis_qc_unbind: free");
	bam_data_destroy(0);
	rndis_deregister(rndis->config);
	rndis_exit();

	if (gadget_is_dualspeed(c->cdev->gadget))
		usb_free_descriptors(f->hs_descriptors);
	usb_free_descriptors(f->descriptors);

	kfree(rndis->notify_req->buf);
	usb_ep_free_request(rndis->notify, rndis->notify_req);

	kfree(rndis);
}

static inline bool can_support_rndis_qc(struct usb_configuration *c)
{
	
	return true;
}

int
rndis_qc_bind_config(struct usb_configuration *c, u8 ethaddr[ETH_ALEN])
{
	return rndis_qc_bind_config_vendor(c, ethaddr, 0, NULL, 1);
}

int
rndis_qc_bind_config_vendor(struct usb_configuration *c, u8 ethaddr[ETH_ALEN],
					 u32 vendorID, const char *manufacturer,
					 u8 max_pkt_per_xfer)
{
	struct f_rndis_qc	*rndis;
	int		status;

	if (!can_support_rndis_qc(c) || !ethaddr)
		return -EINVAL;

	
	status = rndis_init();
	if (status < 0)
		return status;

	status = rndis_qc_bam_setup();
	if (status) {
		pr_err("bam setup failed");
		return status;
	}

	
	if (rndis_qc_string_defs[0].id == 0) {

		
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		rndis_qc_string_defs[0].id = status;
		rndis_qc_control_intf.iInterface = status;

		
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		rndis_qc_string_defs[1].id = status;
		rndis_qc_data_intf.iInterface = status;

		
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		rndis_qc_string_defs[2].id = status;
		rndis_qc_iad_descriptor.iFunction = status;
	}

	
	status = -ENOMEM;
	rndis = kzalloc(sizeof *rndis, GFP_KERNEL);
	if (!rndis)
		goto fail;

	memcpy(rndis->ethaddr, ethaddr, ETH_ALEN);
	rndis->vendorID = vendorID;
	rndis->manufacturer = manufacturer;

	
	rndis->max_pkt_per_xfer =
		max_pkt_per_xfer ? max_pkt_per_xfer : DEFAULT_MAX_PKT_PER_XFER;

	
	rndis->port.cdc_filter = 0;

	
	rndis->port.header_len = sizeof(struct rndis_packet_msg_type);
	rndis->port.wrap = rndis_qc_add_header;
	rndis->port.unwrap = rndis_qc_rm_hdr;

	rndis->port.func.name = "rndis";
	rndis->port.func.strings = rndis_qc_strings;
	
	rndis->port.func.bind = rndis_qc_bind;
	rndis->port.func.unbind = rndis_qc_unbind;
	rndis->port.func.set_alt = rndis_qc_set_alt;
	rndis->port.func.setup = rndis_qc_setup;
	rndis->port.func.disable = rndis_qc_disable;
	rndis->port.func.suspend = rndis_qc_suspend;
	rndis->port.func.resume = rndis_qc_resume;

	_rndis_qc = rndis;

	status = usb_add_function(c, &rndis->port.func);
	if (status) {
		kfree(rndis);
fail:
		rndis_exit();
	}
	return status;
}

static int rndis_qc_open_dev(struct inode *ip, struct file *fp)
{
	pr_info("Open rndis QC driver\n");

	if (!_rndis_qc) {
		pr_err("rndis_qc_dev not created yet\n");
		return -ENODEV;
	}

	if (rndis_qc_lock(&_rndis_qc->open_excl)) {
		pr_err("Already opened\n");
		return -EBUSY;
	}

	fp->private_data = _rndis_qc;
	pr_info("rndis QC file opened\n");

	return 0;
}

static int rndis_qc_release_dev(struct inode *ip, struct file *fp)
{
	struct f_rndis_qc	*rndis = fp->private_data;

	pr_info("Close rndis QC file");
	rndis_qc_unlock(&rndis->open_excl);

	return 0;
}

static long rndis_qc_ioctl(struct file *fp, unsigned cmd, unsigned long arg)
{
	struct f_rndis_qc	*rndis = fp->private_data;
	int ret = 0;

	pr_info("Received command %d", cmd);

	if (rndis_qc_lock(&rndis->ioctl_excl))
		return -EBUSY;

	switch (cmd) {
	case RNDIS_QC_GET_MAX_PKT_PER_XFER:
		ret = copy_to_user((void __user *)arg,
					&rndis->max_pkt_per_xfer,
					sizeof(rndis->max_pkt_per_xfer));
		if (ret) {
			pr_err("copying to user space failed");
			ret = -EFAULT;
		}
		pr_info("Sent max packets per xfer %d",
				rndis->max_pkt_per_xfer);
		break;
	case RNDIS_QC_GET_MAX_PKT_SIZE:
		ret = copy_to_user((void __user *)arg,
					&rndis->max_pkt_size,
					sizeof(rndis->max_pkt_size));
		if (ret) {
			pr_err("copying to user space failed");
			ret = -EFAULT;
		}
		pr_debug("Sent max packet size %d",
				rndis->max_pkt_size);
		break;
	default:
		pr_err("Unsupported IOCTL");
		ret = -EINVAL;
	}

	rndis_qc_unlock(&rndis->ioctl_excl);

	return ret;
}

static const struct file_operations rndis_qc_fops = {
	.owner = THIS_MODULE,
	.open = rndis_qc_open_dev,
	.release = rndis_qc_release_dev,
	.unlocked_ioctl	= rndis_qc_ioctl,
};

static struct miscdevice rndis_qc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "android_rndis_qc",
	.fops = &rndis_qc_fops,
};

static int rndis_qc_init(void)
{
	int ret;

	pr_info("initialize rndis QC instance\n");

	ret = misc_register(&rndis_qc_device);
	if (ret)
		pr_err("rndis QC driver failed to register");

	return ret;
}

static void rndis_qc_cleanup(void)
{
	pr_info("rndis QC cleanup");

	misc_deregister(&rndis_qc_device);
	_rndis_qc = NULL;
}


