/*
 * file_storage.c -- File-backed USB Storage Gadget, for USB development
 *
 * Copyright (C) 2003-2008 Alan Stern
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the above-listed copyright holders may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */








#include <linux/blkdev.h>
#include <linux/completion.h>
#include <linux/dcache.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kref.h>
#include <linux/kthread.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/rwsem.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/freezer.h>
#include <linux/utsname.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

#include "gadget_chips.h"



#include "usbstring.c"
#include "config.c"
#include "epautoconf.c"


#define DRIVER_DESC		"File-backed Storage Gadget"
#define DRIVER_NAME		"g_file_storage"
#define DRIVER_VERSION		"1 September 2010"

static       char fsg_string_manufacturer[64];
static const char fsg_string_product[] = DRIVER_DESC;
static const char fsg_string_config[] = "Self-powered";
static const char fsg_string_interface[] = "Mass Storage";


#include "storage_common.c"


MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Alan Stern");
MODULE_LICENSE("Dual BSD/GPL");






static struct {
	char		*file[FSG_MAX_LUNS];
	char		*serial;
	bool		ro[FSG_MAX_LUNS];
	bool		nofua[FSG_MAX_LUNS];
	unsigned int	num_filenames;
	unsigned int	num_ros;
	unsigned int	num_nofuas;
	unsigned int	nluns;

	bool		removable;
	bool		can_stall;
	bool		cdrom;

	char		*transport_parm;
	char		*protocol_parm;
	unsigned short	vendor;
	unsigned short	product;
	unsigned short	release;
	unsigned int	buflen;

	int		transport_type;
	char		*transport_name;
	int		protocol_type;
	char		*protocol_name;

} mod_data = {					
	.transport_parm		= "BBB",
	.protocol_parm		= "SCSI",
	.removable		= 0,
	.can_stall		= 1,
	.cdrom			= 0,
	.vendor			= FSG_VENDOR_ID,
	.product		= FSG_PRODUCT_ID,
	.release		= 0xffff,	
	.buflen			= 16384,
	};


module_param_array_named(file, mod_data.file, charp, &mod_data.num_filenames,
		S_IRUGO);
MODULE_PARM_DESC(file, "names of backing files or devices");

module_param_named(serial, mod_data.serial, charp, S_IRUGO);
MODULE_PARM_DESC(serial, "USB serial number");

module_param_array_named(ro, mod_data.ro, bool, &mod_data.num_ros, S_IRUGO);
MODULE_PARM_DESC(ro, "true to force read-only");

module_param_array_named(nofua, mod_data.nofua, bool, &mod_data.num_nofuas,
		S_IRUGO);
MODULE_PARM_DESC(nofua, "true to ignore SCSI WRITE(10,12) FUA bit");

module_param_named(luns, mod_data.nluns, uint, S_IRUGO);
MODULE_PARM_DESC(luns, "number of LUNs");

module_param_named(removable, mod_data.removable, bool, S_IRUGO);
MODULE_PARM_DESC(removable, "true to simulate removable media");

module_param_named(stall, mod_data.can_stall, bool, S_IRUGO);
MODULE_PARM_DESC(stall, "false to prevent bulk stalls");

module_param_named(cdrom, mod_data.cdrom, bool, S_IRUGO);
MODULE_PARM_DESC(cdrom, "true to emulate cdrom instead of disk");

#ifdef CONFIG_USB_FILE_STORAGE_TEST

module_param_named(transport, mod_data.transport_parm, charp, S_IRUGO);
MODULE_PARM_DESC(transport, "type of transport (BBB, CBI, or CB)");

module_param_named(protocol, mod_data.protocol_parm, charp, S_IRUGO);
MODULE_PARM_DESC(protocol, "type of protocol (RBC, 8020, QIC, UFI, "
		"8070, or SCSI)");

module_param_named(vendor, mod_data.vendor, ushort, S_IRUGO);
MODULE_PARM_DESC(vendor, "USB Vendor ID");

module_param_named(product, mod_data.product, ushort, S_IRUGO);
MODULE_PARM_DESC(product, "USB Product ID");

module_param_named(release, mod_data.release, ushort, S_IRUGO);
MODULE_PARM_DESC(release, "USB release number");

module_param_named(buflen, mod_data.buflen, uint, S_IRUGO);
MODULE_PARM_DESC(buflen, "I/O buffer size");

#endif 



#ifdef CONFIG_USB_FILE_STORAGE_TEST

#define transport_is_bbb()	(mod_data.transport_type == USB_PR_BULK)
#define transport_is_cbi()	(mod_data.transport_type == USB_PR_CBI)
#define protocol_is_scsi()	(mod_data.protocol_type == USB_SC_SCSI)

#else

#define transport_is_bbb()	1
#define transport_is_cbi()	0
#define protocol_is_scsi()	1

#endif 




struct fsg_dev {
	
	spinlock_t		lock;
	struct usb_gadget	*gadget;

	
	struct rw_semaphore	filesem;

	
	struct kref		ref;

	struct usb_ep		*ep0;		
	struct usb_request	*ep0req;	
	unsigned int		ep0_req_tag;
	const char		*ep0req_name;

	struct usb_request	*intreq;	
	int			intreq_busy;
	struct fsg_buffhd	*intr_buffhd;

	unsigned int		bulk_out_maxpacket;
	enum fsg_state		state;		
	unsigned int		exception_req_tag;

	u8			config, new_config;

	unsigned int		running : 1;
	unsigned int		bulk_in_enabled : 1;
	unsigned int		bulk_out_enabled : 1;
	unsigned int		intr_in_enabled : 1;
	unsigned int		phase_error : 1;
	unsigned int		short_packet_received : 1;
	unsigned int		bad_lun_okay : 1;

	unsigned long		atomic_bitflags;
#define REGISTERED		0
#define IGNORE_BULK_OUT		1
#define SUSPENDED		2

	struct usb_ep		*bulk_in;
	struct usb_ep		*bulk_out;
	struct usb_ep		*intr_in;

	struct fsg_buffhd	*next_buffhd_to_fill;
	struct fsg_buffhd	*next_buffhd_to_drain;

	int			thread_wakeup_needed;
	struct completion	thread_notifier;
	struct task_struct	*thread_task;

	int			cmnd_size;
	u8			cmnd[MAX_COMMAND_SIZE];
	enum data_direction	data_dir;
	u32			data_size;
	u32			data_size_from_cmnd;
	u32			tag;
	unsigned int		lun;
	u32			residue;
	u32			usb_amount_left;

	int			cbbuf_cmnd_size;
	u8			cbbuf_cmnd[MAX_COMMAND_SIZE];

	unsigned int		nluns;
	struct fsg_lun		*luns;
	struct fsg_lun		*curlun;
	
	struct fsg_buffhd	buffhds[];
};

typedef void (*fsg_routine_t)(struct fsg_dev *);

static int exception_in_progress(struct fsg_dev *fsg)
{
	return (fsg->state > FSG_STATE_IDLE);
}

static void set_bulk_out_req_length(struct fsg_dev *fsg,
		struct fsg_buffhd *bh, unsigned int length)
{
	unsigned int	rem;

	bh->bulk_out_intended_length = length;
	rem = length % fsg->bulk_out_maxpacket;
	if (rem > 0)
		length += fsg->bulk_out_maxpacket - rem;
	bh->outreq->length = length;
}

static struct fsg_dev			*the_fsg;
static struct usb_gadget_driver		fsg_driver;



static int fsg_set_halt(struct fsg_dev *fsg, struct usb_ep *ep)
{
	const char	*name;

	if (ep == fsg->bulk_in)
		name = "bulk-in";
	else if (ep == fsg->bulk_out)
		name = "bulk-out";
	else
		name = ep->name;
	DBG(fsg, "%s set halt\n", name);
	return usb_ep_set_halt(ep);
}




#define	CONFIG_VALUE		1

static struct usb_device_descriptor
device_desc = {
	.bLength =		sizeof device_desc,
	.bDescriptorType =	USB_DT_DEVICE,

	.bcdUSB =		cpu_to_le16(0x0200),
	.bDeviceClass =		USB_CLASS_PER_INTERFACE,

	
	.idVendor =		cpu_to_le16(FSG_VENDOR_ID),
	.idProduct =		cpu_to_le16(FSG_PRODUCT_ID),
	.bcdDevice =		cpu_to_le16(0xffff),

	.iManufacturer =	FSG_STRING_MANUFACTURER,
	.iProduct =		FSG_STRING_PRODUCT,
	.iSerialNumber =	FSG_STRING_SERIAL,
	.bNumConfigurations =	1,
};

static struct usb_config_descriptor
config_desc = {
	.bLength =		sizeof config_desc,
	.bDescriptorType =	USB_DT_CONFIG,

	
	.bNumInterfaces =	1,
	.bConfigurationValue =	CONFIG_VALUE,
	.iConfiguration =	FSG_STRING_CONFIG,
	.bmAttributes =		USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	.bMaxPower =		CONFIG_USB_GADGET_VBUS_DRAW / 2,
};


static struct usb_qualifier_descriptor
dev_qualifier = {
	.bLength =		sizeof dev_qualifier,
	.bDescriptorType =	USB_DT_DEVICE_QUALIFIER,

	.bcdUSB =		cpu_to_le16(0x0200),
	.bDeviceClass =		USB_CLASS_PER_INTERFACE,

	.bNumConfigurations =	1,
};

static int populate_bos(struct fsg_dev *fsg, u8 *buf)
{
	memcpy(buf, &fsg_bos_desc, USB_DT_BOS_SIZE);
	buf += USB_DT_BOS_SIZE;

	memcpy(buf, &fsg_ext_cap_desc, USB_DT_USB_EXT_CAP_SIZE);
	buf += USB_DT_USB_EXT_CAP_SIZE;

	memcpy(buf, &fsg_ss_cap_desc, USB_DT_USB_SS_CAP_SIZE);

	return USB_DT_BOS_SIZE + USB_DT_USB_SS_CAP_SIZE
		+ USB_DT_USB_EXT_CAP_SIZE;
}

static int populate_config_buf(struct usb_gadget *gadget,
		u8 *buf, u8 type, unsigned index)
{
	enum usb_device_speed			speed = gadget->speed;
	int					len;
	const struct usb_descriptor_header	**function;

	if (index > 0)
		return -EINVAL;

	if (gadget_is_dualspeed(gadget) && type == USB_DT_OTHER_SPEED_CONFIG)
		speed = (USB_SPEED_FULL + USB_SPEED_HIGH) - speed;
	function = gadget_is_dualspeed(gadget) && speed == USB_SPEED_HIGH
		? (const struct usb_descriptor_header **)fsg_hs_function
		: (const struct usb_descriptor_header **)fsg_fs_function;

	
	if (!gadget_is_otg(gadget))
		function++;

	len = usb_gadget_config_buf(&config_desc, buf, EP0_BUFSIZE, function);
	((struct usb_config_descriptor *) buf)->bDescriptorType = type;
	return len;
}




static void wakeup_thread(struct fsg_dev *fsg)
{
	
	fsg->thread_wakeup_needed = 1;
	if (fsg->thread_task)
		wake_up_process(fsg->thread_task);
}


static void raise_exception(struct fsg_dev *fsg, enum fsg_state new_state)
{
	unsigned long		flags;

	spin_lock_irqsave(&fsg->lock, flags);
	if (fsg->state <= new_state) {
		fsg->exception_req_tag = fsg->ep0_req_tag;
		fsg->state = new_state;
		if (fsg->thread_task)
			send_sig_info(SIGUSR1, SEND_SIG_FORCED,
					fsg->thread_task);
	}
	spin_unlock_irqrestore(&fsg->lock, flags);
}




static void fsg_disconnect(struct usb_gadget *gadget)
{
	struct fsg_dev		*fsg = get_gadget_data(gadget);

	DBG(fsg, "disconnect or port reset\n");
	raise_exception(fsg, FSG_STATE_DISCONNECT);
}


static int ep0_queue(struct fsg_dev *fsg)
{
	int	rc;

	rc = usb_ep_queue(fsg->ep0, fsg->ep0req, GFP_ATOMIC);
	if (rc != 0 && rc != -ESHUTDOWN) {

		
		WARNING(fsg, "error in submission: %s --> %d\n",
				fsg->ep0->name, rc);
	}
	return rc;
}

static void ep0_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct fsg_dev		*fsg = ep->driver_data;

	if (req->actual > 0)
		dump_msg(fsg, fsg->ep0req_name, req->buf, req->actual);
	if (req->status || req->actual != req->length)
		DBG(fsg, "%s --> %d, %u/%u\n", __func__,
				req->status, req->actual, req->length);
	if (req->status == -ECONNRESET)		
		usb_ep_fifo_flush(ep);

	if (req->status == 0 && req->context)
		((fsg_routine_t) (req->context))(fsg);
}




static void bulk_in_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct fsg_dev		*fsg = ep->driver_data;
	struct fsg_buffhd	*bh = req->context;

	if (req->status || req->actual != req->length)
		DBG(fsg, "%s --> %d, %u/%u\n", __func__,
				req->status, req->actual, req->length);
	if (req->status == -ECONNRESET)		
		usb_ep_fifo_flush(ep);

	
	smp_wmb();
	spin_lock(&fsg->lock);
	bh->inreq_busy = 0;
	bh->state = BUF_STATE_EMPTY;
	wakeup_thread(fsg);
	spin_unlock(&fsg->lock);
}

static void bulk_out_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct fsg_dev		*fsg = ep->driver_data;
	struct fsg_buffhd	*bh = req->context;

	dump_msg(fsg, "bulk-out", req->buf, req->actual);
	if (req->status || req->actual != bh->bulk_out_intended_length)
		DBG(fsg, "%s --> %d, %u/%u\n", __func__,
				req->status, req->actual,
				bh->bulk_out_intended_length);
	if (req->status == -ECONNRESET)		
		usb_ep_fifo_flush(ep);

	
	smp_wmb();
	spin_lock(&fsg->lock);
	bh->outreq_busy = 0;
	bh->state = BUF_STATE_FULL;
	wakeup_thread(fsg);
	spin_unlock(&fsg->lock);
}


#ifdef CONFIG_USB_FILE_STORAGE_TEST
static void intr_in_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct fsg_dev		*fsg = ep->driver_data;
	struct fsg_buffhd	*bh = req->context;

	if (req->status || req->actual != req->length)
		DBG(fsg, "%s --> %d, %u/%u\n", __func__,
				req->status, req->actual, req->length);
	if (req->status == -ECONNRESET)		
		usb_ep_fifo_flush(ep);

	
	smp_wmb();
	spin_lock(&fsg->lock);
	fsg->intreq_busy = 0;
	bh->state = BUF_STATE_EMPTY;
	wakeup_thread(fsg);
	spin_unlock(&fsg->lock);
}

#else
static void intr_in_complete(struct usb_ep *ep, struct usb_request *req)
{}
#endif 




#ifdef CONFIG_USB_FILE_STORAGE_TEST
static void received_cbi_adsc(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	struct usb_request	*req = fsg->ep0req;
	static u8		cbi_reset_cmnd[6] = {
			SEND_DIAGNOSTIC, 4, 0xff, 0xff, 0xff, 0xff};

	
	if (req->status || req->length != req->actual ||
			req->actual < 6 || req->actual > MAX_COMMAND_SIZE) {

		fsg_set_halt(fsg, fsg->ep0);
		return;			
	}

	
	if (req->actual >= sizeof cbi_reset_cmnd &&
			memcmp(req->buf, cbi_reset_cmnd,
				sizeof cbi_reset_cmnd) == 0) {

		DBG(fsg, "cbi reset request\n");
		raise_exception(fsg, FSG_STATE_RESET);
		return;
	}

	VDBG(fsg, "CB[I] accept device-specific command\n");
	spin_lock(&fsg->lock);

	
	if (fsg->cbbuf_cmnd_size)
		WARNING(fsg, "CB[I] overwriting previous command\n");
	fsg->cbbuf_cmnd_size = req->actual;
	memcpy(fsg->cbbuf_cmnd, req->buf, fsg->cbbuf_cmnd_size);

	wakeup_thread(fsg);
	spin_unlock(&fsg->lock);
}

#else
static void received_cbi_adsc(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{}
#endif 


static int class_setup_req(struct fsg_dev *fsg,
		const struct usb_ctrlrequest *ctrl)
{
	struct usb_request	*req = fsg->ep0req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16                     w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);

	if (!fsg->config)
		return value;

	
	if (transport_is_bbb()) {
		switch (ctrl->bRequest) {

		case US_BULK_RESET_REQUEST:
			if (ctrl->bRequestType != (USB_DIR_OUT |
					USB_TYPE_CLASS | USB_RECIP_INTERFACE))
				break;
			if (w_index != 0 || w_value != 0 || w_length != 0) {
				value = -EDOM;
				break;
			}

			DBG(fsg, "bulk reset request\n");
			raise_exception(fsg, FSG_STATE_RESET);
			value = DELAYED_STATUS;
			break;

		case US_BULK_GET_MAX_LUN:
			if (ctrl->bRequestType != (USB_DIR_IN |
					USB_TYPE_CLASS | USB_RECIP_INTERFACE))
				break;
			if (w_index != 0 || w_value != 0 || w_length != 1) {
				value = -EDOM;
				break;
			}
			VDBG(fsg, "get max LUN\n");
			*(u8 *) req->buf = fsg->nluns - 1;
			value = 1;
			break;
		}
	}

	
	else {
		switch (ctrl->bRequest) {

		case USB_CBI_ADSC_REQUEST:
			if (ctrl->bRequestType != (USB_DIR_OUT |
					USB_TYPE_CLASS | USB_RECIP_INTERFACE))
				break;
			if (w_index != 0 || w_value != 0) {
				value = -EDOM;
				break;
			}
			if (w_length > MAX_COMMAND_SIZE) {
				value = -EOVERFLOW;
				break;
			}
			value = w_length;
			fsg->ep0req->context = received_cbi_adsc;
			break;
		}
	}

	if (value == -EOPNOTSUPP)
		VDBG(fsg,
			"unknown class-specific control req "
			"%02x.%02x v%04x i%04x l%u\n",
			ctrl->bRequestType, ctrl->bRequest,
			le16_to_cpu(ctrl->wValue), w_index, w_length);
	return value;
}




static int standard_setup_req(struct fsg_dev *fsg,
		const struct usb_ctrlrequest *ctrl)
{
	struct usb_request	*req = fsg->ep0req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);

	switch (ctrl->bRequest) {

	case USB_REQ_GET_DESCRIPTOR:
		if (ctrl->bRequestType != (USB_DIR_IN | USB_TYPE_STANDARD |
				USB_RECIP_DEVICE))
			break;
		switch (w_value >> 8) {

		case USB_DT_DEVICE:
			VDBG(fsg, "get device descriptor\n");
			device_desc.bMaxPacketSize0 = fsg->ep0->maxpacket;
			value = sizeof device_desc;
			memcpy(req->buf, &device_desc, value);
			break;
		case USB_DT_DEVICE_QUALIFIER:
			VDBG(fsg, "get device qualifier\n");
			if (!gadget_is_dualspeed(fsg->gadget) ||
					fsg->gadget->speed == USB_SPEED_SUPER)
				break;
			dev_qualifier.bMaxPacketSize0 = fsg->ep0->maxpacket;
			value = sizeof dev_qualifier;
			memcpy(req->buf, &dev_qualifier, value);
			break;

		case USB_DT_OTHER_SPEED_CONFIG:
			VDBG(fsg, "get other-speed config descriptor\n");
			if (!gadget_is_dualspeed(fsg->gadget) ||
					fsg->gadget->speed == USB_SPEED_SUPER)
				break;
			goto get_config;
		case USB_DT_CONFIG:
			VDBG(fsg, "get configuration descriptor\n");
get_config:
			value = populate_config_buf(fsg->gadget,
					req->buf,
					w_value >> 8,
					w_value & 0xff);
			break;

		case USB_DT_STRING:
			VDBG(fsg, "get string descriptor\n");

			
			value = usb_gadget_get_string(&fsg_stringtab,
					w_value & 0xff, req->buf);
			break;

		case USB_DT_BOS:
			VDBG(fsg, "get bos descriptor\n");

			if (gadget_is_superspeed(fsg->gadget))
				value = populate_bos(fsg, req->buf);
			break;
		}

		break;

	
	case USB_REQ_SET_CONFIGURATION:
		if (ctrl->bRequestType != (USB_DIR_OUT | USB_TYPE_STANDARD |
				USB_RECIP_DEVICE))
			break;
		VDBG(fsg, "set configuration\n");
		if (w_value == CONFIG_VALUE || w_value == 0) {
			fsg->new_config = w_value;

			raise_exception(fsg, FSG_STATE_CONFIG_CHANGE);
			value = DELAYED_STATUS;
		}
		break;
	case USB_REQ_GET_CONFIGURATION:
		if (ctrl->bRequestType != (USB_DIR_IN | USB_TYPE_STANDARD |
				USB_RECIP_DEVICE))
			break;
		VDBG(fsg, "get configuration\n");
		*(u8 *) req->buf = fsg->config;
		value = 1;
		break;

	case USB_REQ_SET_INTERFACE:
		if (ctrl->bRequestType != (USB_DIR_OUT| USB_TYPE_STANDARD |
				USB_RECIP_INTERFACE))
			break;
		if (fsg->config && w_index == 0) {

			raise_exception(fsg, FSG_STATE_INTERFACE_CHANGE);
			value = DELAYED_STATUS;
		}
		break;
	case USB_REQ_GET_INTERFACE:
		if (ctrl->bRequestType != (USB_DIR_IN | USB_TYPE_STANDARD |
				USB_RECIP_INTERFACE))
			break;
		if (!fsg->config)
			break;
		if (w_index != 0) {
			value = -EDOM;
			break;
		}
		VDBG(fsg, "get interface\n");
		*(u8 *) req->buf = 0;
		value = 1;
		break;

	default:
		VDBG(fsg,
			"unknown control req %02x.%02x v%04x i%04x l%u\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, le16_to_cpu(ctrl->wLength));
	}

	return value;
}


static int fsg_setup(struct usb_gadget *gadget,
		const struct usb_ctrlrequest *ctrl)
{
	struct fsg_dev		*fsg = get_gadget_data(gadget);
	int			rc;
	int			w_length = le16_to_cpu(ctrl->wLength);

	++fsg->ep0_req_tag;		
	fsg->ep0req->context = NULL;
	fsg->ep0req->length = 0;
	dump_msg(fsg, "ep0-setup", (u8 *) ctrl, sizeof(*ctrl));

	if ((ctrl->bRequestType & USB_TYPE_MASK) == USB_TYPE_CLASS)
		rc = class_setup_req(fsg, ctrl);
	else
		rc = standard_setup_req(fsg, ctrl);

	
	if (rc >= 0 && rc != DELAYED_STATUS) {
		rc = min(rc, w_length);
		fsg->ep0req->length = rc;
		fsg->ep0req->zero = rc < w_length;
		fsg->ep0req_name = (ctrl->bRequestType & USB_DIR_IN ?
				"ep0-in" : "ep0-out");
		rc = ep0_queue(fsg);
	}

	
	return rc;
}





static void start_transfer(struct fsg_dev *fsg, struct usb_ep *ep,
		struct usb_request *req, int *pbusy,
		enum fsg_buffer_state *state)
{
	int	rc;

	if (ep == fsg->bulk_in)
		dump_msg(fsg, "bulk-in", req->buf, req->length);
	else if (ep == fsg->intr_in)
		dump_msg(fsg, "intr-in", req->buf, req->length);

	spin_lock_irq(&fsg->lock);
	*pbusy = 1;
	*state = BUF_STATE_BUSY;
	spin_unlock_irq(&fsg->lock);
	rc = usb_ep_queue(ep, req, GFP_KERNEL);
	if (rc != 0) {
		*pbusy = 0;
		*state = BUF_STATE_EMPTY;

		

		if (rc != -ESHUTDOWN && !(rc == -EOPNOTSUPP &&
						req->length == 0))
			WARNING(fsg, "error in submission: %s --> %d\n",
					ep->name, rc);
	}
}


static int sleep_thread(struct fsg_dev *fsg)
{
	int	rc = 0;

	
	for (;;) {
		try_to_freeze();
		set_current_state(TASK_INTERRUPTIBLE);
		if (signal_pending(current)) {
			rc = -EINTR;
			break;
		}
		if (fsg->thread_wakeup_needed)
			break;
		schedule();
	}
	__set_current_state(TASK_RUNNING);
	fsg->thread_wakeup_needed = 0;
	return rc;
}



static int do_read(struct fsg_dev *fsg)
{
	struct fsg_lun		*curlun = fsg->curlun;
	u32			lba;
	struct fsg_buffhd	*bh;
	int			rc;
	u32			amount_left;
	loff_t			file_offset, file_offset_tmp;
	unsigned int		amount;
	ssize_t			nread;

	if (fsg->cmnd[0] == READ_6)
		lba = get_unaligned_be24(&fsg->cmnd[1]);
	else {
		lba = get_unaligned_be32(&fsg->cmnd[2]);

		if ((fsg->cmnd[1] & ~0x18) != 0) {
			curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
			return -EINVAL;
		}
	}
	if (lba >= curlun->num_sectors) {
		curlun->sense_data = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		return -EINVAL;
	}
	file_offset = ((loff_t) lba) << curlun->blkbits;

	
	amount_left = fsg->data_size_from_cmnd;
	if (unlikely(amount_left == 0))
		return -EIO;		

	for (;;) {

		amount = min((unsigned int) amount_left, mod_data.buflen);
		amount = min((loff_t) amount,
				curlun->file_length - file_offset);

		
		bh = fsg->next_buffhd_to_fill;
		while (bh->state != BUF_STATE_EMPTY) {
			rc = sleep_thread(fsg);
			if (rc)
				return rc;
		}

		if (amount == 0) {
			curlun->sense_data =
					SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
			curlun->sense_data_info = file_offset >> curlun->blkbits;
			curlun->info_valid = 1;
			bh->inreq->length = 0;
			bh->state = BUF_STATE_FULL;
			break;
		}

		
		file_offset_tmp = file_offset;
		nread = vfs_read(curlun->filp,
				(char __user *) bh->buf,
				amount, &file_offset_tmp);
		VLDBG(curlun, "file read %u @ %llu -> %d\n", amount,
				(unsigned long long) file_offset,
				(int) nread);
		if (signal_pending(current))
			return -EINTR;

		if (nread < 0) {
			LDBG(curlun, "error in file read: %d\n",
					(int) nread);
			nread = 0;
		} else if (nread < amount) {
			LDBG(curlun, "partial file read: %d/%u\n",
					(int) nread, amount);
			nread = round_down(nread, curlun->blksize);
		}
		file_offset  += nread;
		amount_left  -= nread;
		fsg->residue -= nread;

		bh->inreq->length = nread;
		bh->state = BUF_STATE_FULL;

		
		if (nread < amount) {
			curlun->sense_data = SS_UNRECOVERED_READ_ERROR;
			curlun->sense_data_info = file_offset >> curlun->blkbits;
			curlun->info_valid = 1;
			break;
		}

		if (amount_left == 0)
			break;		

		
		bh->inreq->zero = 0;
		start_transfer(fsg, fsg->bulk_in, bh->inreq,
				&bh->inreq_busy, &bh->state);
		fsg->next_buffhd_to_fill = bh->next;
	}

	return -EIO;		
}



static int do_write(struct fsg_dev *fsg)
{
	struct fsg_lun		*curlun = fsg->curlun;
	u32			lba;
	struct fsg_buffhd	*bh;
	int			get_some_more;
	u32			amount_left_to_req, amount_left_to_write;
	loff_t			usb_offset, file_offset, file_offset_tmp;
	unsigned int		amount;
	ssize_t			nwritten;
	int			rc;

	if (curlun->ro) {
		curlun->sense_data = SS_WRITE_PROTECTED;
		return -EINVAL;
	}
	spin_lock(&curlun->filp->f_lock);
	curlun->filp->f_flags &= ~O_SYNC;	
	spin_unlock(&curlun->filp->f_lock);

	if (fsg->cmnd[0] == WRITE_6)
		lba = get_unaligned_be24(&fsg->cmnd[1]);
	else {
		lba = get_unaligned_be32(&fsg->cmnd[2]);

		if ((fsg->cmnd[1] & ~0x18) != 0) {
			curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
			return -EINVAL;
		}
		
		if (!curlun->nofua && (fsg->cmnd[1] & 0x08)) {
			spin_lock(&curlun->filp->f_lock);
			curlun->filp->f_flags |= O_DSYNC;
			spin_unlock(&curlun->filp->f_lock);
		}
	}
	if (lba >= curlun->num_sectors) {
		curlun->sense_data = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		return -EINVAL;
	}

	
	get_some_more = 1;
	file_offset = usb_offset = ((loff_t) lba) << curlun->blkbits;
	amount_left_to_req = amount_left_to_write = fsg->data_size_from_cmnd;

	while (amount_left_to_write > 0) {

		
		bh = fsg->next_buffhd_to_fill;
		if (bh->state == BUF_STATE_EMPTY && get_some_more) {

			amount = min(amount_left_to_req, mod_data.buflen);

			
			if (usb_offset >= curlun->file_length) {
				get_some_more = 0;
				curlun->sense_data =
					SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
				curlun->sense_data_info = usb_offset >> curlun->blkbits;
				curlun->info_valid = 1;
				continue;
			}

			
			usb_offset += amount;
			fsg->usb_amount_left -= amount;
			amount_left_to_req -= amount;
			if (amount_left_to_req == 0)
				get_some_more = 0;

			set_bulk_out_req_length(fsg, bh, amount);
			start_transfer(fsg, fsg->bulk_out, bh->outreq,
					&bh->outreq_busy, &bh->state);
			fsg->next_buffhd_to_fill = bh->next;
			continue;
		}

		
		bh = fsg->next_buffhd_to_drain;
		if (bh->state == BUF_STATE_EMPTY && !get_some_more)
			break;			
		if (bh->state == BUF_STATE_FULL) {
			smp_rmb();
			fsg->next_buffhd_to_drain = bh->next;
			bh->state = BUF_STATE_EMPTY;

			
			if (bh->outreq->status != 0) {
				curlun->sense_data = SS_COMMUNICATION_FAILURE;
				curlun->sense_data_info = file_offset >> curlun->blkbits;
				curlun->info_valid = 1;
				break;
			}

			amount = bh->outreq->actual;
			if (curlun->file_length - file_offset < amount) {
				LERROR(curlun,
	"write %u @ %llu beyond end %llu\n",
	amount, (unsigned long long) file_offset,
	(unsigned long long) curlun->file_length);
				amount = curlun->file_length - file_offset;
			}

			amount = min(amount, bh->bulk_out_intended_length);

			
			amount = round_down(amount, curlun->blksize);
			if (amount == 0)
				goto empty_write;

			
			file_offset_tmp = file_offset;
			nwritten = vfs_write(curlun->filp,
					(char __user *) bh->buf,
					amount, &file_offset_tmp);
			VLDBG(curlun, "file write %u @ %llu -> %d\n", amount,
					(unsigned long long) file_offset,
					(int) nwritten);
			if (signal_pending(current))
				return -EINTR;		

			if (nwritten < 0) {
				LDBG(curlun, "error in file write: %d\n",
						(int) nwritten);
				nwritten = 0;
			} else if (nwritten < amount) {
				LDBG(curlun, "partial file write: %d/%u\n",
						(int) nwritten, amount);
				nwritten = round_down(nwritten, curlun->blksize);
			}
			file_offset += nwritten;
			amount_left_to_write -= nwritten;
			fsg->residue -= nwritten;

			
			if (nwritten < amount) {
				curlun->sense_data = SS_WRITE_ERROR;
				curlun->sense_data_info = file_offset >> curlun->blkbits;
				curlun->info_valid = 1;
				break;
			}

 empty_write:
			
			if (bh->outreq->actual < bh->bulk_out_intended_length) {
				fsg->short_packet_received = 1;
				break;
			}
			continue;
		}

		
		rc = sleep_thread(fsg);
		if (rc)
			return rc;
	}

	return -EIO;		
}



static int do_synchronize_cache(struct fsg_dev *fsg)
{
	struct fsg_lun	*curlun = fsg->curlun;
	int		rc;

	rc = fsg_lun_fsync_sub(curlun);
	if (rc)
		curlun->sense_data = SS_WRITE_ERROR;
	return 0;
}



static void invalidate_sub(struct fsg_lun *curlun)
{
	struct file	*filp = curlun->filp;
	struct inode	*inode = filp->f_path.dentry->d_inode;
	unsigned long	rc;

	rc = invalidate_mapping_pages(inode->i_mapping, 0, -1);
	VLDBG(curlun, "invalidate_mapping_pages -> %ld\n", rc);
}

static int do_verify(struct fsg_dev *fsg)
{
	struct fsg_lun		*curlun = fsg->curlun;
	u32			lba;
	u32			verification_length;
	struct fsg_buffhd	*bh = fsg->next_buffhd_to_fill;
	loff_t			file_offset, file_offset_tmp;
	u32			amount_left;
	unsigned int		amount;
	ssize_t			nread;

	lba = get_unaligned_be32(&fsg->cmnd[2]);
	if (lba >= curlun->num_sectors) {
		curlun->sense_data = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		return -EINVAL;
	}

	if ((fsg->cmnd[1] & ~0x10) != 0) {
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	verification_length = get_unaligned_be16(&fsg->cmnd[7]);
	if (unlikely(verification_length == 0))
		return -EIO;		

	
	amount_left = verification_length << curlun->blkbits;
	file_offset = ((loff_t) lba) << curlun->blkbits;

	
	fsg_lun_fsync_sub(curlun);
	if (signal_pending(current))
		return -EINTR;

	invalidate_sub(curlun);
	if (signal_pending(current))
		return -EINTR;

	
	while (amount_left > 0) {

		amount = min((unsigned int) amount_left, mod_data.buflen);
		amount = min((loff_t) amount,
				curlun->file_length - file_offset);
		if (amount == 0) {
			curlun->sense_data =
					SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
			curlun->sense_data_info = file_offset >> curlun->blkbits;
			curlun->info_valid = 1;
			break;
		}

		
		file_offset_tmp = file_offset;
		nread = vfs_read(curlun->filp,
				(char __user *) bh->buf,
				amount, &file_offset_tmp);
		VLDBG(curlun, "file read %u @ %llu -> %d\n", amount,
				(unsigned long long) file_offset,
				(int) nread);
		if (signal_pending(current))
			return -EINTR;

		if (nread < 0) {
			LDBG(curlun, "error in file verify: %d\n",
					(int) nread);
			nread = 0;
		} else if (nread < amount) {
			LDBG(curlun, "partial file verify: %d/%u\n",
					(int) nread, amount);
			nread = round_down(nread, curlun->blksize);
		}
		if (nread == 0) {
			curlun->sense_data = SS_UNRECOVERED_READ_ERROR;
			curlun->sense_data_info = file_offset >> curlun->blkbits;
			curlun->info_valid = 1;
			break;
		}
		file_offset += nread;
		amount_left -= nread;
	}
	return 0;
}



static int do_inquiry(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	u8	*buf = (u8 *) bh->buf;

	static char vendor_id[] = "Linux   ";
	static char product_disk_id[] = "File-Stor Gadget";
	static char product_cdrom_id[] = "File-CD Gadget  ";

	if (!fsg->curlun) {		
		fsg->bad_lun_okay = 1;
		memset(buf, 0, 36);
		buf[0] = 0x7f;		
		buf[4] = 31;		
		return 36;
	}

	memset(buf, 0, 8);
	buf[0] = (mod_data.cdrom ? TYPE_ROM : TYPE_DISK);
	if (mod_data.removable)
		buf[1] = 0x80;
	buf[2] = 2;		
	buf[3] = 2;		
	buf[4] = 31;		
				
	sprintf(buf + 8, "%-8s%-16s%04x", vendor_id,
			(mod_data.cdrom ? product_cdrom_id :
				product_disk_id),
			mod_data.release);
	return 36;
}


static int do_request_sense(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = fsg->curlun;
	u8		*buf = (u8 *) bh->buf;
	u32		sd, sdinfo;
	int		valid;

#if 0
	if (curlun && curlun->unit_attention_data != SS_NO_SENSE) {
		curlun->sense_data = curlun->unit_attention_data;
		curlun->unit_attention_data = SS_NO_SENSE;
	}
#endif

	if (!curlun) {		
		fsg->bad_lun_okay = 1;
		sd = SS_LOGICAL_UNIT_NOT_SUPPORTED;
		sdinfo = 0;
		valid = 0;
	} else {
		sd = curlun->sense_data;
		sdinfo = curlun->sense_data_info;
		valid = curlun->info_valid << 7;
		curlun->sense_data = SS_NO_SENSE;
		curlun->sense_data_info = 0;
		curlun->info_valid = 0;
	}

	memset(buf, 0, 18);
	buf[0] = valid | 0x70;			
	buf[2] = SK(sd);
	put_unaligned_be32(sdinfo, &buf[3]);	
	buf[7] = 18 - 8;			
	buf[12] = ASC(sd);
	buf[13] = ASCQ(sd);
	return 18;
}


static int do_read_capacity(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = fsg->curlun;
	u32		lba = get_unaligned_be32(&fsg->cmnd[2]);
	int		pmi = fsg->cmnd[8];
	u8		*buf = (u8 *) bh->buf;

	
	if (pmi > 1 || (pmi == 0 && lba != 0)) {
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	put_unaligned_be32(curlun->num_sectors - 1, &buf[0]);
						
	put_unaligned_be32(curlun->blksize, &buf[4]);	
	return 8;
}


static int do_read_header(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = fsg->curlun;
	int		msf = fsg->cmnd[1] & 0x02;
	u32		lba = get_unaligned_be32(&fsg->cmnd[2]);
	u8		*buf = (u8 *) bh->buf;

	if ((fsg->cmnd[1] & ~0x02) != 0) {		
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}
	if (lba >= curlun->num_sectors) {
		curlun->sense_data = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		return -EINVAL;
	}

	memset(buf, 0, 8);
	buf[0] = 0x01;		
	store_cdrom_address(&buf[4], msf, lba);
	return 8;
}


static int do_read_toc(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = fsg->curlun;
	int		msf = fsg->cmnd[1] & 0x02;
	int		start_track = fsg->cmnd[6];
	u8		*buf = (u8 *) bh->buf;

	if ((fsg->cmnd[1] & ~0x02) != 0 ||		
			start_track > 1) {
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	memset(buf, 0, 20);
	buf[1] = (20-2);		
	buf[2] = 1;			
	buf[3] = 1;			
	buf[5] = 0x16;			
	buf[6] = 0x01;			
	store_cdrom_address(&buf[8], msf, 0);

	buf[13] = 0x16;			
	buf[14] = 0xAA;			
	store_cdrom_address(&buf[16], msf, curlun->num_sectors);
	return 20;
}


static int do_mode_sense(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = fsg->curlun;
	int		mscmnd = fsg->cmnd[0];
	u8		*buf = (u8 *) bh->buf;
	u8		*buf0 = buf;
	int		pc, page_code;
	int		changeable_values, all_pages;
	int		valid_page = 0;
	int		len, limit;

	if ((fsg->cmnd[1] & ~0x08) != 0) {		
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}
	pc = fsg->cmnd[2] >> 6;
	page_code = fsg->cmnd[2] & 0x3f;
	if (pc == 3) {
		curlun->sense_data = SS_SAVING_PARAMETERS_NOT_SUPPORTED;
		return -EINVAL;
	}
	changeable_values = (pc == 1);
	all_pages = (page_code == 0x3f);

	memset(buf, 0, 8);
	if (mscmnd == MODE_SENSE) {
		buf[2] = (curlun->ro ? 0x80 : 0x00);		
		buf += 4;
		limit = 255;
	} else {			
		buf[3] = (curlun->ro ? 0x80 : 0x00);		
		buf += 8;
		limit = 65535;		
	}

	

	if (page_code == 0x08 || all_pages) {
		valid_page = 1;
		buf[0] = 0x08;		
		buf[1] = 10;		
		memset(buf+2, 0, 10);	

		if (!changeable_values) {
			buf[2] = 0x04;	
					
					
			put_unaligned_be16(0xffff, &buf[4]);
					
					
			put_unaligned_be16(0xffff, &buf[8]);
					
			put_unaligned_be16(0xffff, &buf[10]);
					
		}
		buf += 12;
	}

	len = buf - buf0;
	if (!valid_page || len > limit) {
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	
	if (mscmnd == MODE_SENSE)
		buf0[0] = len - 1;
	else
		put_unaligned_be16(len - 2, buf0);
	return len;
}


static int do_start_stop(struct fsg_dev *fsg)
{
	struct fsg_lun	*curlun = fsg->curlun;
	int		loej, start;

	if (!mod_data.removable) {
		curlun->sense_data = SS_INVALID_COMMAND;
		return -EINVAL;
	}

	
	loej = fsg->cmnd[4] & 0x02;
	start = fsg->cmnd[4] & 0x01;

#ifdef CONFIG_USB_FILE_STORAGE_TEST
	if ((fsg->cmnd[1] & ~0x01) != 0 ||		
			(fsg->cmnd[4] & ~0x03) != 0) {	
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	if (!start) {

		
		if (curlun->prevent_medium_removal) {
			LDBG(curlun, "unload attempt prevented\n");
			curlun->sense_data = SS_MEDIUM_REMOVAL_PREVENTED;
			return -EINVAL;
		}
		if (loej) {		
			up_read(&fsg->filesem);
			down_write(&fsg->filesem);
			fsg_lun_close(curlun);
			up_write(&fsg->filesem);
			down_read(&fsg->filesem);
		}
	} else {

		if (!fsg_lun_is_open(curlun)) {
			curlun->sense_data = SS_MEDIUM_NOT_PRESENT;
			return -EINVAL;
		}
	}
#endif
	return 0;
}


static int do_prevent_allow(struct fsg_dev *fsg)
{
	struct fsg_lun	*curlun = fsg->curlun;
	int		prevent;

	if (!mod_data.removable) {
		curlun->sense_data = SS_INVALID_COMMAND;
		return -EINVAL;
	}

	prevent = fsg->cmnd[4] & 0x01;
	if ((fsg->cmnd[4] & ~0x01) != 0) {		
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	if (curlun->prevent_medium_removal && !prevent)
		fsg_lun_fsync_sub(curlun);
	curlun->prevent_medium_removal = prevent;
	return 0;
}


static int do_read_format_capacities(struct fsg_dev *fsg,
			struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = fsg->curlun;
	u8		*buf = (u8 *) bh->buf;

	buf[0] = buf[1] = buf[2] = 0;
	buf[3] = 8;		
	buf += 4;

	put_unaligned_be32(curlun->num_sectors, &buf[0]);
						
	put_unaligned_be32(curlun->blksize, &buf[4]);	
	buf[4] = 0x02;				
	return 12;
}


static int do_mode_select(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = fsg->curlun;

	
	curlun->sense_data = SS_INVALID_COMMAND;
	return -EINVAL;
}



static int halt_bulk_in_endpoint(struct fsg_dev *fsg)
{
	int	rc;

	rc = fsg_set_halt(fsg, fsg->bulk_in);
	if (rc == -EAGAIN)
		VDBG(fsg, "delayed bulk-in endpoint halt\n");
	while (rc != 0) {
		if (rc != -EAGAIN) {
			WARNING(fsg, "usb_ep_set_halt -> %d\n", rc);
			rc = 0;
			break;
		}

		
		if (msleep_interruptible(100) != 0)
			return -EINTR;
		rc = usb_ep_set_halt(fsg->bulk_in);
	}
	return rc;
}

static int wedge_bulk_in_endpoint(struct fsg_dev *fsg)
{
	int	rc;

	DBG(fsg, "bulk-in set wedge\n");
	rc = usb_ep_set_wedge(fsg->bulk_in);
	if (rc == -EAGAIN)
		VDBG(fsg, "delayed bulk-in endpoint wedge\n");
	while (rc != 0) {
		if (rc != -EAGAIN) {
			WARNING(fsg, "usb_ep_set_wedge -> %d\n", rc);
			rc = 0;
			break;
		}

		
		if (msleep_interruptible(100) != 0)
			return -EINTR;
		rc = usb_ep_set_wedge(fsg->bulk_in);
	}
	return rc;
}

static int throw_away_data(struct fsg_dev *fsg)
{
	struct fsg_buffhd	*bh;
	u32			amount;
	int			rc;

	while ((bh = fsg->next_buffhd_to_drain)->state != BUF_STATE_EMPTY ||
			fsg->usb_amount_left > 0) {

		
		if (bh->state == BUF_STATE_FULL) {
			smp_rmb();
			bh->state = BUF_STATE_EMPTY;
			fsg->next_buffhd_to_drain = bh->next;

			
			if (bh->outreq->actual < bh->bulk_out_intended_length ||
					bh->outreq->status != 0) {
				raise_exception(fsg, FSG_STATE_ABORT_BULK_OUT);
				return -EINTR;
			}
			continue;
		}

		
		bh = fsg->next_buffhd_to_fill;
		if (bh->state == BUF_STATE_EMPTY && fsg->usb_amount_left > 0) {
			amount = min(fsg->usb_amount_left,
					(u32) mod_data.buflen);

			set_bulk_out_req_length(fsg, bh, amount);
			start_transfer(fsg, fsg->bulk_out, bh->outreq,
					&bh->outreq_busy, &bh->state);
			fsg->next_buffhd_to_fill = bh->next;
			fsg->usb_amount_left -= amount;
			continue;
		}

		
		rc = sleep_thread(fsg);
		if (rc)
			return rc;
	}
	return 0;
}


static int finish_reply(struct fsg_dev *fsg)
{
	struct fsg_buffhd	*bh = fsg->next_buffhd_to_fill;
	int			rc = 0;

	switch (fsg->data_dir) {
	case DATA_DIR_NONE:
		break;			

	case DATA_DIR_UNKNOWN:
		if (mod_data.can_stall) {
			fsg_set_halt(fsg, fsg->bulk_out);
			rc = halt_bulk_in_endpoint(fsg);
		}
		break;

	
	case DATA_DIR_TO_HOST:
		if (fsg->data_size == 0)
			;		

		
		else if (fsg->residue == 0) {
			bh->inreq->zero = 0;
			start_transfer(fsg, fsg->bulk_in, bh->inreq,
					&bh->inreq_busy, &bh->state);
			fsg->next_buffhd_to_fill = bh->next;
		}

		else if (!transport_is_bbb()) {
			if (mod_data.can_stall &&
					fsg->residue == fsg->data_size &&
	(!fsg->curlun || fsg->curlun->sense_data != SS_NO_SENSE)) {
				bh->state = BUF_STATE_EMPTY;
				rc = halt_bulk_in_endpoint(fsg);
			} else {
				bh->inreq->zero = 1;
				start_transfer(fsg, fsg->bulk_in, bh->inreq,
						&bh->inreq_busy, &bh->state);
				fsg->next_buffhd_to_fill = bh->next;
			}
		}

		else {
			bh->inreq->zero = 1;
			start_transfer(fsg, fsg->bulk_in, bh->inreq,
					&bh->inreq_busy, &bh->state);
			fsg->next_buffhd_to_fill = bh->next;
			if (mod_data.can_stall)
				rc = halt_bulk_in_endpoint(fsg);
		}
		break;

	case DATA_DIR_FROM_HOST:
		if (fsg->residue == 0)
			;		

		
		else if (fsg->short_packet_received) {
			raise_exception(fsg, FSG_STATE_ABORT_BULK_OUT);
			rc = -EINTR;
		}

#if 0
		else if (mod_data.can_stall) {
			fsg_set_halt(fsg, fsg->bulk_out);
			raise_exception(fsg, FSG_STATE_ABORT_BULK_OUT);
			rc = -EINTR;
		}
#endif

		else
			rc = throw_away_data(fsg);
		break;
	}
	return rc;
}


static int send_status(struct fsg_dev *fsg)
{
	struct fsg_lun		*curlun = fsg->curlun;
	struct fsg_buffhd	*bh;
	int			rc;
	u8			status = US_BULK_STAT_OK;
	u32			sd, sdinfo = 0;

	
	bh = fsg->next_buffhd_to_fill;
	while (bh->state != BUF_STATE_EMPTY) {
		rc = sleep_thread(fsg);
		if (rc)
			return rc;
	}

	if (curlun) {
		sd = curlun->sense_data;
		sdinfo = curlun->sense_data_info;
	} else if (fsg->bad_lun_okay)
		sd = SS_NO_SENSE;
	else
		sd = SS_LOGICAL_UNIT_NOT_SUPPORTED;

	if (fsg->phase_error) {
		DBG(fsg, "sending phase-error status\n");
		status = US_BULK_STAT_PHASE;
		sd = SS_INVALID_COMMAND;
	} else if (sd != SS_NO_SENSE) {
		DBG(fsg, "sending command-failure status\n");
		status = US_BULK_STAT_FAIL;
		VDBG(fsg, "  sense data: SK x%02x, ASC x%02x, ASCQ x%02x;"
				"  info x%x\n",
				SK(sd), ASC(sd), ASCQ(sd), sdinfo);
	}

	if (transport_is_bbb()) {
		struct bulk_cs_wrap	*csw = bh->buf;

		
		csw->Signature = cpu_to_le32(US_BULK_CS_SIGN);
		csw->Tag = fsg->tag;
		csw->Residue = cpu_to_le32(fsg->residue);
		csw->Status = status;

		bh->inreq->length = US_BULK_CS_WRAP_LEN;
		bh->inreq->zero = 0;
		start_transfer(fsg, fsg->bulk_in, bh->inreq,
				&bh->inreq_busy, &bh->state);

	} else if (mod_data.transport_type == USB_PR_CB) {

		
		return 0;

	} else {			
		struct interrupt_data	*buf = bh->buf;

		if (mod_data.protocol_type == USB_SC_UFI) {
			buf->bType = ASC(sd);
			buf->bValue = ASCQ(sd);
		} else {
			buf->bType = 0;
			buf->bValue = status;
		}
		fsg->intreq->length = CBI_INTERRUPT_DATA_LEN;

		fsg->intr_buffhd = bh;		
		fsg->intreq->buf = bh->inreq->buf;
		fsg->intreq->context = bh;
		start_transfer(fsg, fsg->intr_in, fsg->intreq,
				&fsg->intreq_busy, &bh->state);
	}

	fsg->next_buffhd_to_fill = bh->next;
	return 0;
}



static int check_command(struct fsg_dev *fsg, int cmnd_size,
		enum data_direction data_dir, unsigned int mask,
		int needs_medium, const char *name)
{
	int			i;
	int			lun = fsg->cmnd[1] >> 5;
	static const char	dirletter[4] = {'u', 'o', 'i', 'n'};
	char			hdlen[20];
	struct fsg_lun		*curlun;

	if (protocol_is_scsi())
		;

	else if (mod_data.protocol_type == USB_SC_RBC) {
		if (fsg->cmnd_size == 12)
			cmnd_size = 12;

	
	} else
		cmnd_size = 12;

	hdlen[0] = 0;
	if (fsg->data_dir != DATA_DIR_UNKNOWN)
		sprintf(hdlen, ", H%c=%u", dirletter[(int) fsg->data_dir],
				fsg->data_size);
	VDBG(fsg, "SCSI command: %s;  Dc=%d, D%c=%u;  Hc=%d%s\n",
			name, cmnd_size, dirletter[(int) data_dir],
			fsg->data_size_from_cmnd, fsg->cmnd_size, hdlen);

	if (fsg->data_size_from_cmnd == 0)
		data_dir = DATA_DIR_NONE;
	if (fsg->data_dir == DATA_DIR_UNKNOWN) {	
		fsg->data_dir = data_dir;
		fsg->data_size = fsg->data_size_from_cmnd;

	} else {					
		if (fsg->data_size < fsg->data_size_from_cmnd) {

			fsg->data_size_from_cmnd = fsg->data_size;
			fsg->phase_error = 1;
		}
	}
	fsg->residue = fsg->usb_amount_left = fsg->data_size;

	
	if (fsg->data_dir != data_dir && fsg->data_size_from_cmnd > 0) {
		fsg->phase_error = 1;
		return -EINVAL;
	}

	
	if (cmnd_size != fsg->cmnd_size) {

		if (cmnd_size <= fsg->cmnd_size) {
			DBG(fsg, "%s is buggy! Expected length %d "
					"but we got %d\n", name,
					cmnd_size, fsg->cmnd_size);
			cmnd_size = fsg->cmnd_size;
		} else {
			fsg->phase_error = 1;
			return -EINVAL;
		}
	}

	
	if (transport_is_bbb()) {
		if (fsg->lun != lun)
			DBG(fsg, "using LUN %d from CBW, "
					"not LUN %d from CDB\n",
					fsg->lun, lun);
	}

	
	curlun = fsg->curlun;
	if (curlun) {
		if (fsg->cmnd[0] != REQUEST_SENSE) {
			curlun->sense_data = SS_NO_SENSE;
			curlun->sense_data_info = 0;
			curlun->info_valid = 0;
		}
	} else {
		fsg->bad_lun_okay = 0;

		if (fsg->cmnd[0] != INQUIRY &&
				fsg->cmnd[0] != REQUEST_SENSE) {
			DBG(fsg, "unsupported LUN %d\n", fsg->lun);
			return -EINVAL;
		}
	}

	if (curlun && curlun->unit_attention_data != SS_NO_SENSE &&
			fsg->cmnd[0] != INQUIRY &&
			fsg->cmnd[0] != REQUEST_SENSE) {
		curlun->sense_data = curlun->unit_attention_data;
		curlun->unit_attention_data = SS_NO_SENSE;
		return -EINVAL;
	}

	
	fsg->cmnd[1] &= 0x1f;			
	for (i = 1; i < cmnd_size; ++i) {
		if (fsg->cmnd[i] && !(mask & (1 << i))) {
			if (curlun)
				curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
			return -EINVAL;
		}
	}

	if (curlun && !fsg_lun_is_open(curlun) && needs_medium) {
		curlun->sense_data = SS_MEDIUM_NOT_PRESENT;
		return -EINVAL;
	}

	return 0;
}

static int check_command_size_in_blocks(struct fsg_dev *fsg, int cmnd_size,
		enum data_direction data_dir, unsigned int mask,
		int needs_medium, const char *name)
{
	if (fsg->curlun)
		fsg->data_size_from_cmnd <<= fsg->curlun->blkbits;
	return check_command(fsg, cmnd_size, data_dir,
			mask, needs_medium, name);
}

static int do_scsi_command(struct fsg_dev *fsg)
{
	struct fsg_buffhd	*bh;
	int			rc;
	int			reply = -EINVAL;
	int			i;
	static char		unknown[16];

	dump_cdb(fsg);

	
	bh = fsg->next_buffhd_to_drain = fsg->next_buffhd_to_fill;
	while (bh->state != BUF_STATE_EMPTY) {
		rc = sleep_thread(fsg);
		if (rc)
			return rc;
	}
	fsg->phase_error = 0;
	fsg->short_packet_received = 0;

	down_read(&fsg->filesem);	
	switch (fsg->cmnd[0]) {

	case INQUIRY:
		fsg->data_size_from_cmnd = fsg->cmnd[4];
		if ((reply = check_command(fsg, 6, DATA_DIR_TO_HOST,
				(1<<4), 0,
				"INQUIRY")) == 0)
			reply = do_inquiry(fsg, bh);
		break;

	case MODE_SELECT:
		fsg->data_size_from_cmnd = fsg->cmnd[4];
		if ((reply = check_command(fsg, 6, DATA_DIR_FROM_HOST,
				(1<<1) | (1<<4), 0,
				"MODE SELECT(6)")) == 0)
			reply = do_mode_select(fsg, bh);
		break;

	case MODE_SELECT_10:
		fsg->data_size_from_cmnd = get_unaligned_be16(&fsg->cmnd[7]);
		if ((reply = check_command(fsg, 10, DATA_DIR_FROM_HOST,
				(1<<1) | (3<<7), 0,
				"MODE SELECT(10)")) == 0)
			reply = do_mode_select(fsg, bh);
		break;

	case MODE_SENSE:
		fsg->data_size_from_cmnd = fsg->cmnd[4];
		if ((reply = check_command(fsg, 6, DATA_DIR_TO_HOST,
				(1<<1) | (1<<2) | (1<<4), 0,
				"MODE SENSE(6)")) == 0)
			reply = do_mode_sense(fsg, bh);
		break;

	case MODE_SENSE_10:
		fsg->data_size_from_cmnd = get_unaligned_be16(&fsg->cmnd[7]);
		if ((reply = check_command(fsg, 10, DATA_DIR_TO_HOST,
				(1<<1) | (1<<2) | (3<<7), 0,
				"MODE SENSE(10)")) == 0)
			reply = do_mode_sense(fsg, bh);
		break;

	case ALLOW_MEDIUM_REMOVAL:
		fsg->data_size_from_cmnd = 0;
		if ((reply = check_command(fsg, 6, DATA_DIR_NONE,
				(1<<4), 0,
				"PREVENT-ALLOW MEDIUM REMOVAL")) == 0)
			reply = do_prevent_allow(fsg);
		break;

	case READ_6:
		i = fsg->cmnd[4];
		fsg->data_size_from_cmnd = (i == 0) ? 256 : i;
		if ((reply = check_command_size_in_blocks(fsg, 6,
				DATA_DIR_TO_HOST,
				(7<<1) | (1<<4), 1,
				"READ(6)")) == 0)
			reply = do_read(fsg);
		break;

	case READ_10:
		fsg->data_size_from_cmnd = get_unaligned_be16(&fsg->cmnd[7]);
		if ((reply = check_command_size_in_blocks(fsg, 10,
				DATA_DIR_TO_HOST,
				(1<<1) | (0xf<<2) | (3<<7), 1,
				"READ(10)")) == 0)
			reply = do_read(fsg);
		break;

	case READ_12:
		fsg->data_size_from_cmnd = get_unaligned_be32(&fsg->cmnd[6]);
		if ((reply = check_command_size_in_blocks(fsg, 12,
				DATA_DIR_TO_HOST,
				(1<<1) | (0xf<<2) | (0xf<<6), 1,
				"READ(12)")) == 0)
			reply = do_read(fsg);
		break;

	case READ_CAPACITY:
		fsg->data_size_from_cmnd = 8;
		if ((reply = check_command(fsg, 10, DATA_DIR_TO_HOST,
				(0xf<<2) | (1<<8), 1,
				"READ CAPACITY")) == 0)
			reply = do_read_capacity(fsg, bh);
		break;

	case READ_HEADER:
		if (!mod_data.cdrom)
			goto unknown_cmnd;
		fsg->data_size_from_cmnd = get_unaligned_be16(&fsg->cmnd[7]);
		if ((reply = check_command(fsg, 10, DATA_DIR_TO_HOST,
				(3<<7) | (0x1f<<1), 1,
				"READ HEADER")) == 0)
			reply = do_read_header(fsg, bh);
		break;

	case READ_TOC:
		if (!mod_data.cdrom)
			goto unknown_cmnd;
		fsg->data_size_from_cmnd = get_unaligned_be16(&fsg->cmnd[7]);
		if ((reply = check_command(fsg, 10, DATA_DIR_TO_HOST,
				(7<<6) | (1<<1), 1,
				"READ TOC")) == 0)
			reply = do_read_toc(fsg, bh);
		break;

	case READ_FORMAT_CAPACITIES:
		fsg->data_size_from_cmnd = get_unaligned_be16(&fsg->cmnd[7]);
		if ((reply = check_command(fsg, 10, DATA_DIR_TO_HOST,
				(3<<7), 1,
				"READ FORMAT CAPACITIES")) == 0)
			reply = do_read_format_capacities(fsg, bh);
		break;

	case REQUEST_SENSE:
		fsg->data_size_from_cmnd = fsg->cmnd[4];
		if ((reply = check_command(fsg, 6, DATA_DIR_TO_HOST,
				(1<<4), 0,
				"REQUEST SENSE")) == 0)
			reply = do_request_sense(fsg, bh);
		break;

	case START_STOP:
		fsg->data_size_from_cmnd = 0;
		if ((reply = check_command(fsg, 6, DATA_DIR_NONE,
				(1<<1) | (1<<4), 0,
				"START-STOP UNIT")) == 0)
			reply = do_start_stop(fsg);
		break;

	case SYNCHRONIZE_CACHE:
		fsg->data_size_from_cmnd = 0;
		if ((reply = check_command(fsg, 10, DATA_DIR_NONE,
				(0xf<<2) | (3<<7), 1,
				"SYNCHRONIZE CACHE")) == 0)
			reply = do_synchronize_cache(fsg);
		break;

	case TEST_UNIT_READY:
		fsg->data_size_from_cmnd = 0;
		reply = check_command(fsg, 6, DATA_DIR_NONE,
				0, 1,
				"TEST UNIT READY");
		break;

	case VERIFY:
		fsg->data_size_from_cmnd = 0;
		if ((reply = check_command(fsg, 10, DATA_DIR_NONE,
				(1<<1) | (0xf<<2) | (3<<7), 1,
				"VERIFY")) == 0)
			reply = do_verify(fsg);
		break;

	case WRITE_6:
		i = fsg->cmnd[4];
		fsg->data_size_from_cmnd = (i == 0) ? 256 : i;
		if ((reply = check_command_size_in_blocks(fsg, 6,
				DATA_DIR_FROM_HOST,
				(7<<1) | (1<<4), 1,
				"WRITE(6)")) == 0)
			reply = do_write(fsg);
		break;

	case WRITE_10:
		fsg->data_size_from_cmnd = get_unaligned_be16(&fsg->cmnd[7]);
		if ((reply = check_command_size_in_blocks(fsg, 10,
				DATA_DIR_FROM_HOST,
				(1<<1) | (0xf<<2) | (3<<7), 1,
				"WRITE(10)")) == 0)
			reply = do_write(fsg);
		break;

	case WRITE_12:
		fsg->data_size_from_cmnd = get_unaligned_be32(&fsg->cmnd[6]);
		if ((reply = check_command_size_in_blocks(fsg, 12,
				DATA_DIR_FROM_HOST,
				(1<<1) | (0xf<<2) | (0xf<<6), 1,
				"WRITE(12)")) == 0)
			reply = do_write(fsg);
		break;

	case FORMAT_UNIT:
	case RELEASE:
	case RESERVE:
	case SEND_DIAGNOSTIC:
		

	default:
 unknown_cmnd:
		fsg->data_size_from_cmnd = 0;
		sprintf(unknown, "Unknown x%02x", fsg->cmnd[0]);
		if ((reply = check_command(fsg, fsg->cmnd_size,
				DATA_DIR_UNKNOWN, ~0, 0, unknown)) == 0) {
			fsg->curlun->sense_data = SS_INVALID_COMMAND;
			reply = -EINVAL;
		}
		break;
	}
	up_read(&fsg->filesem);

	if (reply == -EINTR || signal_pending(current))
		return -EINTR;

	
	if (reply == -EINVAL)
		reply = 0;		
	if (reply >= 0 && fsg->data_dir == DATA_DIR_TO_HOST) {
		reply = min((u32) reply, fsg->data_size_from_cmnd);
		bh->inreq->length = reply;
		bh->state = BUF_STATE_FULL;
		fsg->residue -= reply;
	}				

	return 0;
}



static int received_cbw(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	struct usb_request		*req = bh->outreq;
	struct bulk_cb_wrap	*cbw = req->buf;

	
	if (req->status || test_bit(IGNORE_BULK_OUT, &fsg->atomic_bitflags))
		return -EINVAL;

	
	if (req->actual != US_BULK_CB_WRAP_LEN ||
			cbw->Signature != cpu_to_le32(
				US_BULK_CB_SIGN)) {
		DBG(fsg, "invalid CBW: len %u sig 0x%x\n",
				req->actual,
				le32_to_cpu(cbw->Signature));

		wedge_bulk_in_endpoint(fsg);
		set_bit(IGNORE_BULK_OUT, &fsg->atomic_bitflags);
		return -EINVAL;
	}

	
	if (cbw->Lun >= FSG_MAX_LUNS || cbw->Flags & ~US_BULK_FLAG_IN ||
			cbw->Length <= 0 || cbw->Length > MAX_COMMAND_SIZE) {
		DBG(fsg, "non-meaningful CBW: lun = %u, flags = 0x%x, "
				"cmdlen %u\n",
				cbw->Lun, cbw->Flags, cbw->Length);

		if (mod_data.can_stall) {
			fsg_set_halt(fsg, fsg->bulk_out);
			halt_bulk_in_endpoint(fsg);
		}
		return -EINVAL;
	}

	
	fsg->cmnd_size = cbw->Length;
	memcpy(fsg->cmnd, cbw->CDB, fsg->cmnd_size);
	if (cbw->Flags & US_BULK_FLAG_IN)
		fsg->data_dir = DATA_DIR_TO_HOST;
	else
		fsg->data_dir = DATA_DIR_FROM_HOST;
	fsg->data_size = le32_to_cpu(cbw->DataTransferLength);
	if (fsg->data_size == 0)
		fsg->data_dir = DATA_DIR_NONE;
	fsg->lun = cbw->Lun;
	fsg->tag = cbw->Tag;
	return 0;
}


static int get_next_command(struct fsg_dev *fsg)
{
	struct fsg_buffhd	*bh;
	int			rc = 0;

	if (transport_is_bbb()) {

		
		bh = fsg->next_buffhd_to_fill;
		while (bh->state != BUF_STATE_EMPTY) {
			rc = sleep_thread(fsg);
			if (rc)
				return rc;
		}

		
		set_bulk_out_req_length(fsg, bh, US_BULK_CB_WRAP_LEN);
		start_transfer(fsg, fsg->bulk_out, bh->outreq,
				&bh->outreq_busy, &bh->state);


		
		while (bh->state != BUF_STATE_FULL) {
			rc = sleep_thread(fsg);
			if (rc)
				return rc;
		}
		smp_rmb();
		rc = received_cbw(fsg, bh);
		bh->state = BUF_STATE_EMPTY;

	} else {		

		
		while (fsg->cbbuf_cmnd_size == 0) {
			rc = sleep_thread(fsg);
			if (rc)
				return rc;
		}

		if (fsg->intreq_busy)
			usb_ep_dequeue(fsg->intr_in, fsg->intreq);

		
		fsg->data_dir = DATA_DIR_UNKNOWN;
		spin_lock_irq(&fsg->lock);
		fsg->cmnd_size = fsg->cbbuf_cmnd_size;
		memcpy(fsg->cmnd, fsg->cbbuf_cmnd, fsg->cmnd_size);
		fsg->cbbuf_cmnd_size = 0;
		spin_unlock_irq(&fsg->lock);

		
		fsg->lun = fsg->cmnd[1] >> 5;
	}

	
	if (fsg->lun >= 0 && fsg->lun < fsg->nluns)
		fsg->curlun = &fsg->luns[fsg->lun];
	else
		fsg->curlun = NULL;

	return rc;
}



static int enable_endpoint(struct fsg_dev *fsg, struct usb_ep *ep,
		const struct usb_endpoint_descriptor *d)
{
	int	rc;

	ep->driver_data = fsg;
	ep->desc = d;
	rc = usb_ep_enable(ep);
	if (rc)
		ERROR(fsg, "can't enable %s, result %d\n", ep->name, rc);
	return rc;
}

static int alloc_request(struct fsg_dev *fsg, struct usb_ep *ep,
		struct usb_request **preq)
{
	*preq = usb_ep_alloc_request(ep, GFP_ATOMIC);
	if (*preq)
		return 0;
	ERROR(fsg, "can't allocate request for %s\n", ep->name);
	return -ENOMEM;
}

static int do_set_interface(struct fsg_dev *fsg, int altsetting)
{
	int	rc = 0;
	int	i;
	const struct usb_endpoint_descriptor	*d;

	if (fsg->running)
		DBG(fsg, "reset interface\n");

reset:
	
	for (i = 0; i < fsg_num_buffers; ++i) {
		struct fsg_buffhd *bh = &fsg->buffhds[i];

		if (bh->inreq) {
			usb_ep_free_request(fsg->bulk_in, bh->inreq);
			bh->inreq = NULL;
		}
		if (bh->outreq) {
			usb_ep_free_request(fsg->bulk_out, bh->outreq);
			bh->outreq = NULL;
		}
	}
	if (fsg->intreq) {
		usb_ep_free_request(fsg->intr_in, fsg->intreq);
		fsg->intreq = NULL;
	}

	
	if (fsg->bulk_in_enabled) {
		usb_ep_disable(fsg->bulk_in);
		fsg->bulk_in_enabled = 0;
	}
	if (fsg->bulk_out_enabled) {
		usb_ep_disable(fsg->bulk_out);
		fsg->bulk_out_enabled = 0;
	}
	if (fsg->intr_in_enabled) {
		usb_ep_disable(fsg->intr_in);
		fsg->intr_in_enabled = 0;
	}

	fsg->running = 0;
	if (altsetting < 0 || rc != 0)
		return rc;

	DBG(fsg, "set interface %d\n", altsetting);

	
	d = fsg_ep_desc(fsg->gadget,
			&fsg_fs_bulk_in_desc, &fsg_hs_bulk_in_desc,
			&fsg_ss_bulk_in_desc);
	if ((rc = enable_endpoint(fsg, fsg->bulk_in, d)) != 0)
		goto reset;
	fsg->bulk_in_enabled = 1;

	d = fsg_ep_desc(fsg->gadget,
			&fsg_fs_bulk_out_desc, &fsg_hs_bulk_out_desc,
			&fsg_ss_bulk_out_desc);
	if ((rc = enable_endpoint(fsg, fsg->bulk_out, d)) != 0)
		goto reset;
	fsg->bulk_out_enabled = 1;
	fsg->bulk_out_maxpacket = usb_endpoint_maxp(d);
	clear_bit(IGNORE_BULK_OUT, &fsg->atomic_bitflags);

	if (transport_is_cbi()) {
		d = fsg_ep_desc(fsg->gadget,
				&fsg_fs_intr_in_desc, &fsg_hs_intr_in_desc,
				&fsg_ss_intr_in_desc);
		if ((rc = enable_endpoint(fsg, fsg->intr_in, d)) != 0)
			goto reset;
		fsg->intr_in_enabled = 1;
	}

	
	for (i = 0; i < fsg_num_buffers; ++i) {
		struct fsg_buffhd	*bh = &fsg->buffhds[i];

		if ((rc = alloc_request(fsg, fsg->bulk_in, &bh->inreq)) != 0)
			goto reset;
		if ((rc = alloc_request(fsg, fsg->bulk_out, &bh->outreq)) != 0)
			goto reset;
		bh->inreq->buf = bh->outreq->buf = bh->buf;
		bh->inreq->context = bh->outreq->context = bh;
		bh->inreq->complete = bulk_in_complete;
		bh->outreq->complete = bulk_out_complete;
	}
	if (transport_is_cbi()) {
		if ((rc = alloc_request(fsg, fsg->intr_in, &fsg->intreq)) != 0)
			goto reset;
		fsg->intreq->complete = intr_in_complete;
	}

	fsg->running = 1;
	for (i = 0; i < fsg->nluns; ++i)
		fsg->luns[i].unit_attention_data = SS_RESET_OCCURRED;
	return rc;
}


static int do_set_config(struct fsg_dev *fsg, u8 new_config)
{
	int	rc = 0;

	
	if (fsg->config != 0) {
		DBG(fsg, "reset config\n");
		fsg->config = 0;
		rc = do_set_interface(fsg, -1);
	}

	
	if (new_config != 0) {
		fsg->config = new_config;
		if ((rc = do_set_interface(fsg, 0)) != 0)
			fsg->config = 0;	
		else
			INFO(fsg, "%s config #%d\n",
			     usb_speed_string(fsg->gadget->speed),
			     fsg->config);
	}
	return rc;
}



static void handle_exception(struct fsg_dev *fsg)
{
	siginfo_t		info;
	int			sig;
	int			i;
	int			num_active;
	struct fsg_buffhd	*bh;
	enum fsg_state		old_state;
	u8			new_config;
	struct fsg_lun		*curlun;
	unsigned int		exception_req_tag;
	int			rc;

	for (;;) {
		sig = dequeue_signal_lock(current, &current->blocked, &info);
		if (!sig)
			break;
		if (sig != SIGUSR1) {
			if (fsg->state < FSG_STATE_EXIT)
				DBG(fsg, "Main thread exiting on signal\n");
			raise_exception(fsg, FSG_STATE_EXIT);
		}
	}

	
	if (fsg->intreq_busy)
		usb_ep_dequeue(fsg->intr_in, fsg->intreq);
	for (i = 0; i < fsg_num_buffers; ++i) {
		bh = &fsg->buffhds[i];
		if (bh->inreq_busy)
			usb_ep_dequeue(fsg->bulk_in, bh->inreq);
		if (bh->outreq_busy)
			usb_ep_dequeue(fsg->bulk_out, bh->outreq);
	}

	
	for (;;) {
		num_active = fsg->intreq_busy;
		for (i = 0; i < fsg_num_buffers; ++i) {
			bh = &fsg->buffhds[i];
			num_active += bh->inreq_busy + bh->outreq_busy;
		}
		if (num_active == 0)
			break;
		if (sleep_thread(fsg))
			return;
	}

	
	if (fsg->bulk_in_enabled)
		usb_ep_fifo_flush(fsg->bulk_in);
	if (fsg->bulk_out_enabled)
		usb_ep_fifo_flush(fsg->bulk_out);
	if (fsg->intr_in_enabled)
		usb_ep_fifo_flush(fsg->intr_in);

	spin_lock_irq(&fsg->lock);

	for (i = 0; i < fsg_num_buffers; ++i) {
		bh = &fsg->buffhds[i];
		bh->state = BUF_STATE_EMPTY;
	}
	fsg->next_buffhd_to_fill = fsg->next_buffhd_to_drain =
			&fsg->buffhds[0];

	exception_req_tag = fsg->exception_req_tag;
	new_config = fsg->new_config;
	old_state = fsg->state;

	if (old_state == FSG_STATE_ABORT_BULK_OUT)
		fsg->state = FSG_STATE_STATUS_PHASE;
	else {
		for (i = 0; i < fsg->nluns; ++i) {
			curlun = &fsg->luns[i];
			curlun->prevent_medium_removal = 0;
			curlun->sense_data = curlun->unit_attention_data =
					SS_NO_SENSE;
			curlun->sense_data_info = 0;
			curlun->info_valid = 0;
		}
		fsg->state = FSG_STATE_IDLE;
	}
	spin_unlock_irq(&fsg->lock);

	
	switch (old_state) {
	default:
		break;

	case FSG_STATE_ABORT_BULK_OUT:
		send_status(fsg);
		spin_lock_irq(&fsg->lock);
		if (fsg->state == FSG_STATE_STATUS_PHASE)
			fsg->state = FSG_STATE_IDLE;
		spin_unlock_irq(&fsg->lock);
		break;

	case FSG_STATE_RESET:
		if (test_and_clear_bit(IGNORE_BULK_OUT, &fsg->atomic_bitflags))
			usb_ep_clear_halt(fsg->bulk_in);

		if (transport_is_bbb()) {
			if (fsg->ep0_req_tag == exception_req_tag)
				ep0_queue(fsg);	

		} else if (transport_is_cbi())
			send_status(fsg);	

		
		
		break;

	case FSG_STATE_INTERFACE_CHANGE:
		rc = do_set_interface(fsg, 0);
		if (fsg->ep0_req_tag != exception_req_tag)
			break;
		if (rc != 0)			
			fsg_set_halt(fsg, fsg->ep0);
		else				
			ep0_queue(fsg);
		break;

	case FSG_STATE_CONFIG_CHANGE:
		rc = do_set_config(fsg, new_config);
		if (fsg->ep0_req_tag != exception_req_tag)
			break;
		if (rc != 0)			
			fsg_set_halt(fsg, fsg->ep0);
		else				
			ep0_queue(fsg);
		break;

	case FSG_STATE_DISCONNECT:
		for (i = 0; i < fsg->nluns; ++i)
			fsg_lun_fsync_sub(fsg->luns + i);
		do_set_config(fsg, 0);		
		break;

	case FSG_STATE_EXIT:
	case FSG_STATE_TERMINATED:
		do_set_config(fsg, 0);			
		spin_lock_irq(&fsg->lock);
		fsg->state = FSG_STATE_TERMINATED;	
		spin_unlock_irq(&fsg->lock);
		break;
	}
}



static int fsg_main_thread(void *fsg_)
{
	struct fsg_dev		*fsg = fsg_;

	allow_signal(SIGINT);
	allow_signal(SIGTERM);
	allow_signal(SIGKILL);
	allow_signal(SIGUSR1);

	
	set_freezable();

	set_fs(get_ds());

	
	while (fsg->state != FSG_STATE_TERMINATED) {
		if (exception_in_progress(fsg) || signal_pending(current)) {
			handle_exception(fsg);
			continue;
		}

		if (!fsg->running) {
			sleep_thread(fsg);
			continue;
		}

		if (get_next_command(fsg))
			continue;

		spin_lock_irq(&fsg->lock);
		if (!exception_in_progress(fsg))
			fsg->state = FSG_STATE_DATA_PHASE;
		spin_unlock_irq(&fsg->lock);

		if (do_scsi_command(fsg) || finish_reply(fsg))
			continue;

		spin_lock_irq(&fsg->lock);
		if (!exception_in_progress(fsg))
			fsg->state = FSG_STATE_STATUS_PHASE;
		spin_unlock_irq(&fsg->lock);

		if (send_status(fsg))
			continue;

		spin_lock_irq(&fsg->lock);
		if (!exception_in_progress(fsg))
			fsg->state = FSG_STATE_IDLE;
		spin_unlock_irq(&fsg->lock);
		}

	spin_lock_irq(&fsg->lock);
	fsg->thread_task = NULL;
	spin_unlock_irq(&fsg->lock);

	if (test_and_clear_bit(REGISTERED, &fsg->atomic_bitflags))
		usb_gadget_unregister_driver(&fsg_driver);

	
	complete_and_exit(&fsg->thread_notifier, 0);
}




static DEVICE_ATTR(ro, 0444, fsg_show_ro, NULL);
static DEVICE_ATTR(nofua, 0644, fsg_show_nofua, NULL);
static DEVICE_ATTR(file, 0444, fsg_show_file, NULL);



static void fsg_release(struct kref *ref)
{
	struct fsg_dev	*fsg = container_of(ref, struct fsg_dev, ref);

	kfree(fsg->luns);
	kfree(fsg);
}

static void lun_release(struct device *dev)
{
	struct rw_semaphore	*filesem = dev_get_drvdata(dev);
	struct fsg_dev		*fsg =
		container_of(filesem, struct fsg_dev, filesem);

	kref_put(&fsg->ref, fsg_release);
}

static void  fsg_unbind(struct usb_gadget *gadget)
{
	struct fsg_dev		*fsg = get_gadget_data(gadget);
	int			i;
	struct fsg_lun		*curlun;
	struct usb_request	*req = fsg->ep0req;

	DBG(fsg, "unbind\n");
	clear_bit(REGISTERED, &fsg->atomic_bitflags);

	
	if (fsg->state != FSG_STATE_TERMINATED) {
		raise_exception(fsg, FSG_STATE_EXIT);
		wait_for_completion(&fsg->thread_notifier);

		
		complete(&fsg->thread_notifier);
	}

	
	for (i = 0; i < fsg->nluns; ++i) {
		curlun = &fsg->luns[i];
		if (curlun->registered) {
			device_remove_file(&curlun->dev, &dev_attr_nofua);
			device_remove_file(&curlun->dev, &dev_attr_ro);
			device_remove_file(&curlun->dev, &dev_attr_file);
			fsg_lun_close(curlun);
			device_unregister(&curlun->dev);
			curlun->registered = 0;
		}
	}

	
	for (i = 0; i < fsg_num_buffers; ++i)
		kfree(fsg->buffhds[i].buf);

	
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(fsg->ep0, req);
	}

	set_gadget_data(gadget, NULL);
}


static int __init check_parameters(struct fsg_dev *fsg)
{
	int	prot;
	int	gcnum;

	
	mod_data.transport_type = USB_PR_BULK;
	mod_data.transport_name = "Bulk-only";
	mod_data.protocol_type = USB_SC_SCSI;
	mod_data.protocol_name = "Transparent SCSI";

	if (gadget_is_at91(fsg->gadget))
		mod_data.can_stall = 0;

	if (mod_data.release == 0xffff) {	
		gcnum = usb_gadget_controller_number(fsg->gadget);
		if (gcnum >= 0)
			mod_data.release = 0x0300 + gcnum;
		else {
			WARNING(fsg, "controller '%s' not recognized\n",
				fsg->gadget->name);
			mod_data.release = 0x0399;
		}
	}

	prot = simple_strtol(mod_data.protocol_parm, NULL, 0);

#ifdef CONFIG_USB_FILE_STORAGE_TEST
	if (strnicmp(mod_data.transport_parm, "BBB", 10) == 0) {
		;		
	} else if (strnicmp(mod_data.transport_parm, "CB", 10) == 0) {
		mod_data.transport_type = USB_PR_CB;
		mod_data.transport_name = "Control-Bulk";
	} else if (strnicmp(mod_data.transport_parm, "CBI", 10) == 0) {
		mod_data.transport_type = USB_PR_CBI;
		mod_data.transport_name = "Control-Bulk-Interrupt";
	} else {
		ERROR(fsg, "invalid transport: %s\n", mod_data.transport_parm);
		return -EINVAL;
	}

	if (strnicmp(mod_data.protocol_parm, "SCSI", 10) == 0 ||
			prot == USB_SC_SCSI) {
		;		
	} else if (strnicmp(mod_data.protocol_parm, "RBC", 10) == 0 ||
			prot == USB_SC_RBC) {
		mod_data.protocol_type = USB_SC_RBC;
		mod_data.protocol_name = "RBC";
	} else if (strnicmp(mod_data.protocol_parm, "8020", 4) == 0 ||
			strnicmp(mod_data.protocol_parm, "ATAPI", 10) == 0 ||
			prot == USB_SC_8020) {
		mod_data.protocol_type = USB_SC_8020;
		mod_data.protocol_name = "8020i (ATAPI)";
	} else if (strnicmp(mod_data.protocol_parm, "QIC", 3) == 0 ||
			prot == USB_SC_QIC) {
		mod_data.protocol_type = USB_SC_QIC;
		mod_data.protocol_name = "QIC-157";
	} else if (strnicmp(mod_data.protocol_parm, "UFI", 10) == 0 ||
			prot == USB_SC_UFI) {
		mod_data.protocol_type = USB_SC_UFI;
		mod_data.protocol_name = "UFI";
	} else if (strnicmp(mod_data.protocol_parm, "8070", 4) == 0 ||
			prot == USB_SC_8070) {
		mod_data.protocol_type = USB_SC_8070;
		mod_data.protocol_name = "8070i";
	} else {
		ERROR(fsg, "invalid protocol: %s\n", mod_data.protocol_parm);
		return -EINVAL;
	}

	mod_data.buflen &= PAGE_CACHE_MASK;
	if (mod_data.buflen <= 0) {
		ERROR(fsg, "invalid buflen\n");
		return -ETOOSMALL;
	}

#endif 

	if (mod_data.serial) {
		const char *ch;
		unsigned len = 0;

		for (ch = mod_data.serial; *ch; ++ch) {
			++len;
			if ((*ch < '0' || *ch > '9') &&
			    (*ch < 'A' || *ch > 'F')) { 
				WARNING(fsg,
					"Invalid serial string character: %c\n",
					*ch);
				goto no_serial;
			}
		}
		if (len > 126 ||
		    (mod_data.transport_type == USB_PR_BULK && len < 12) ||
		    (mod_data.transport_type != USB_PR_BULK && len > 12)) {
			WARNING(fsg, "Invalid serial string length!\n");
			goto no_serial;
		}
		fsg_strings[FSG_STRING_SERIAL - 1].s = mod_data.serial;
	} else {
		WARNING(fsg, "No serial-number string provided!\n");
 no_serial:
		device_desc.iSerialNumber = 0;
	}

	return 0;
}


static int __init fsg_bind(struct usb_gadget *gadget)
{
	struct fsg_dev		*fsg = the_fsg;
	int			rc;
	int			i;
	struct fsg_lun		*curlun;
	struct usb_ep		*ep;
	struct usb_request	*req;
	char			*pathbuf, *p;

	fsg->gadget = gadget;
	set_gadget_data(gadget, fsg);
	fsg->ep0 = gadget->ep0;
	fsg->ep0->driver_data = fsg;

	if ((rc = check_parameters(fsg)) != 0)
		goto out;

	if (mod_data.removable) {	
		dev_attr_file.attr.mode = 0644;
		dev_attr_file.store = fsg_store_file;
		if (!mod_data.cdrom) {
			dev_attr_ro.attr.mode = 0644;
			dev_attr_ro.store = fsg_store_ro;
		}
	}

	
	dev_attr_nofua.attr.mode = 0644;
	dev_attr_nofua.store = fsg_store_nofua;

	
	i = mod_data.nluns;
	if (i == 0)
		i = max(mod_data.num_filenames, 1u);
	if (i > FSG_MAX_LUNS) {
		ERROR(fsg, "invalid number of LUNs: %d\n", i);
		rc = -EINVAL;
		goto out;
	}

	fsg->luns = kzalloc(i * sizeof(struct fsg_lun), GFP_KERNEL);
	if (!fsg->luns) {
		rc = -ENOMEM;
		goto out;
	}
	fsg->nluns = i;

	for (i = 0; i < fsg->nluns; ++i) {
		curlun = &fsg->luns[i];
		curlun->cdrom = !!mod_data.cdrom;
		curlun->ro = mod_data.cdrom || mod_data.ro[i];
		curlun->initially_ro = curlun->ro;
		curlun->removable = mod_data.removable;
		curlun->nofua = mod_data.nofua[i];
		curlun->dev.release = lun_release;
		curlun->dev.parent = &gadget->dev;
		curlun->dev.driver = &fsg_driver.driver;
		dev_set_drvdata(&curlun->dev, &fsg->filesem);
		dev_set_name(&curlun->dev,"%s-lun%d",
			     dev_name(&gadget->dev), i);

		kref_get(&fsg->ref);
		rc = device_register(&curlun->dev);
		if (rc) {
			INFO(fsg, "failed to register LUN%d: %d\n", i, rc);
			put_device(&curlun->dev);
			goto out;
		}
		curlun->registered = 1;

		rc = device_create_file(&curlun->dev, &dev_attr_ro);
		if (rc)
			goto out;
		rc = device_create_file(&curlun->dev, &dev_attr_nofua);
		if (rc)
			goto out;
		rc = device_create_file(&curlun->dev, &dev_attr_file);
		if (rc)
			goto out;

		if (mod_data.file[i] && *mod_data.file[i]) {
			rc = fsg_lun_open(curlun, mod_data.file[i]);
			if (rc)
				goto out;
		} else if (!mod_data.removable) {
			ERROR(fsg, "no file given for LUN%d\n", i);
			rc = -EINVAL;
			goto out;
		}
	}

	
	usb_ep_autoconfig_reset(gadget);
	ep = usb_ep_autoconfig(gadget, &fsg_fs_bulk_in_desc);
	if (!ep)
		goto autoconf_fail;
	ep->driver_data = fsg;		
	fsg->bulk_in = ep;

	ep = usb_ep_autoconfig(gadget, &fsg_fs_bulk_out_desc);
	if (!ep)
		goto autoconf_fail;
	ep->driver_data = fsg;		
	fsg->bulk_out = ep;

	if (transport_is_cbi()) {
		ep = usb_ep_autoconfig(gadget, &fsg_fs_intr_in_desc);
		if (!ep)
			goto autoconf_fail;
		ep->driver_data = fsg;		
		fsg->intr_in = ep;
	}

	
	device_desc.idVendor = cpu_to_le16(mod_data.vendor);
	device_desc.idProduct = cpu_to_le16(mod_data.product);
	device_desc.bcdDevice = cpu_to_le16(mod_data.release);

	i = (transport_is_cbi() ? 3 : 2);	
	fsg_intf_desc.bNumEndpoints = i;
	fsg_intf_desc.bInterfaceSubClass = mod_data.protocol_type;
	fsg_intf_desc.bInterfaceProtocol = mod_data.transport_type;
	fsg_fs_function[i + FSG_FS_FUNCTION_PRE_EP_ENTRIES] = NULL;

	if (gadget_is_dualspeed(gadget)) {
		fsg_hs_function[i + FSG_HS_FUNCTION_PRE_EP_ENTRIES] = NULL;

		
		fsg_hs_bulk_in_desc.bEndpointAddress =
			fsg_fs_bulk_in_desc.bEndpointAddress;
		fsg_hs_bulk_out_desc.bEndpointAddress =
			fsg_fs_bulk_out_desc.bEndpointAddress;
		fsg_hs_intr_in_desc.bEndpointAddress =
			fsg_fs_intr_in_desc.bEndpointAddress;
	}

	if (gadget_is_superspeed(gadget)) {
		unsigned		max_burst;

		fsg_ss_function[i + FSG_SS_FUNCTION_PRE_EP_ENTRIES] = NULL;

		
		max_burst = min_t(unsigned, mod_data.buflen / 1024, 15);

		
		fsg_ss_bulk_in_desc.bEndpointAddress =
			fsg_fs_bulk_in_desc.bEndpointAddress;
		fsg_ss_bulk_in_comp_desc.bMaxBurst = max_burst;

		fsg_ss_bulk_out_desc.bEndpointAddress =
			fsg_fs_bulk_out_desc.bEndpointAddress;
		fsg_ss_bulk_out_comp_desc.bMaxBurst = max_burst;
	}

	if (gadget_is_otg(gadget))
		fsg_otg_desc.bmAttributes |= USB_OTG_HNP;

	rc = -ENOMEM;

	
	fsg->ep0req = req = usb_ep_alloc_request(fsg->ep0, GFP_KERNEL);
	if (!req)
		goto out;
	req->buf = kmalloc(EP0_BUFSIZE, GFP_KERNEL);
	if (!req->buf)
		goto out;
	req->complete = ep0_complete;

	
	for (i = 0; i < fsg_num_buffers; ++i) {
		struct fsg_buffhd	*bh = &fsg->buffhds[i];

		bh->buf = kmalloc(mod_data.buflen, GFP_KERNEL);
		if (!bh->buf)
			goto out;
		bh->next = bh + 1;
	}
	fsg->buffhds[fsg_num_buffers - 1].next = &fsg->buffhds[0];

	
	usb_gadget_set_selfpowered(gadget);

	snprintf(fsg_string_manufacturer, sizeof fsg_string_manufacturer,
			"%s %s with %s",
			init_utsname()->sysname, init_utsname()->release,
			gadget->name);

	fsg->thread_task = kthread_create(fsg_main_thread, fsg,
			"file-storage-gadget");
	if (IS_ERR(fsg->thread_task)) {
		rc = PTR_ERR(fsg->thread_task);
		goto out;
	}

	INFO(fsg, DRIVER_DESC ", version: " DRIVER_VERSION "\n");
	INFO(fsg, "NOTE: This driver is deprecated.  "
			"Consider using g_mass_storage instead.\n");
	INFO(fsg, "Number of LUNs=%d\n", fsg->nluns);

	pathbuf = kmalloc(PATH_MAX, GFP_KERNEL);
	for (i = 0; i < fsg->nluns; ++i) {
		curlun = &fsg->luns[i];
		if (fsg_lun_is_open(curlun)) {
			p = NULL;
			if (pathbuf) {
				p = d_path(&curlun->filp->f_path,
					   pathbuf, PATH_MAX);
				if (IS_ERR(p))
					p = NULL;
			}
			LINFO(curlun, "ro=%d, nofua=%d, file: %s\n",
			      curlun->ro, curlun->nofua, (p ? p : "(error)"));
		}
	}
	kfree(pathbuf);

	DBG(fsg, "transport=%s (x%02x)\n",
			mod_data.transport_name, mod_data.transport_type);
	DBG(fsg, "protocol=%s (x%02x)\n",
			mod_data.protocol_name, mod_data.protocol_type);
	DBG(fsg, "VendorID=x%04x, ProductID=x%04x, Release=x%04x\n",
			mod_data.vendor, mod_data.product, mod_data.release);
	DBG(fsg, "removable=%d, stall=%d, cdrom=%d, buflen=%u\n",
			mod_data.removable, mod_data.can_stall,
			mod_data.cdrom, mod_data.buflen);
	DBG(fsg, "I/O thread pid: %d\n", task_pid_nr(fsg->thread_task));

	set_bit(REGISTERED, &fsg->atomic_bitflags);

	
	wake_up_process(fsg->thread_task);
	return 0;

autoconf_fail:
	ERROR(fsg, "unable to autoconfigure all endpoints\n");
	rc = -ENOTSUPP;

out:
	fsg->state = FSG_STATE_TERMINATED;	
	fsg_unbind(gadget);
	complete(&fsg->thread_notifier);
	return rc;
}



static void fsg_suspend(struct usb_gadget *gadget)
{
	struct fsg_dev		*fsg = get_gadget_data(gadget);

	DBG(fsg, "suspend\n");
	set_bit(SUSPENDED, &fsg->atomic_bitflags);
}

static void fsg_resume(struct usb_gadget *gadget)
{
	struct fsg_dev		*fsg = get_gadget_data(gadget);

	DBG(fsg, "resume\n");
	clear_bit(SUSPENDED, &fsg->atomic_bitflags);
}



static struct usb_gadget_driver		fsg_driver = {
	.max_speed	= USB_SPEED_SUPER,
	.function	= (char *) fsg_string_product,
	.unbind		= fsg_unbind,
	.disconnect	= fsg_disconnect,
	.setup		= fsg_setup,
	.suspend	= fsg_suspend,
	.resume		= fsg_resume,

	.driver		= {
		.name		= DRIVER_NAME,
		.owner		= THIS_MODULE,
		
		
		
	},
};


static int __init fsg_alloc(void)
{
	struct fsg_dev		*fsg;

	fsg = kzalloc(sizeof *fsg +
		      fsg_num_buffers * sizeof *(fsg->buffhds), GFP_KERNEL);

	if (!fsg)
		return -ENOMEM;
	spin_lock_init(&fsg->lock);
	init_rwsem(&fsg->filesem);
	kref_init(&fsg->ref);
	init_completion(&fsg->thread_notifier);

	the_fsg = fsg;
	return 0;
}


static int __init fsg_init(void)
{
	int		rc;
	struct fsg_dev	*fsg;

	rc = fsg_num_buffers_validate();
	if (rc != 0)
		return rc;

	if ((rc = fsg_alloc()) != 0)
		return rc;
	fsg = the_fsg;
	if ((rc = usb_gadget_probe_driver(&fsg_driver, fsg_bind)) != 0)
		kref_put(&fsg->ref, fsg_release);
	return rc;
}
module_init(fsg_init);


static void __exit fsg_cleanup(void)
{
	struct fsg_dev	*fsg = the_fsg;

	
	if (test_and_clear_bit(REGISTERED, &fsg->atomic_bitflags))
		usb_gadget_unregister_driver(&fsg_driver);

	
	wait_for_completion(&fsg->thread_notifier);

	kref_put(&fsg->ref, fsg_release);
}
module_exit(fsg_cleanup);
