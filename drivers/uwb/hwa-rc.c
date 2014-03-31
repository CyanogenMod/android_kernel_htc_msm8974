/*
 * WUSB Host Wire Adapter: Radio Control Interface (WUSB[8.6])
 * Radio Control command/event transport
 *
 * Copyright (C) 2005-2006 Intel Corporation
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 *
 * Initialize the Radio Control interface Driver.
 *
 * For each device probed, creates an 'struct hwarc' which contains
 * just the representation of the UWB Radio Controller, and the logic
 * for reading notifications and passing them to the UWB Core.
 *
 * So we initialize all of those, register the UWB Radio Controller
 * and setup the notification/event handle to pipe the notifications
 * to the UWB management Daemon.
 *
 * Command and event filtering.
 *
 * This is the driver for the Radio Control Interface described in WUSB
 * 1.0. The core UWB module assumes that all drivers are compliant to the
 * WHCI 0.95 specification. We thus create a filter that parses all
 * incoming messages from the (WUSB 1.0) device and manipulate them to
 * conform to the WHCI 0.95 specification. Similarly, outgoing messages
 * are parsed and manipulated to conform to the WUSB 1.0 compliant messages
 * that the device expects. Only a few messages are affected:
 * Affected events:
 *    UWB_RC_EVT_BEACON
 *    UWB_RC_EVT_BP_SLOT_CHANGE
 *    UWB_RC_EVT_DRP_AVAIL
 *    UWB_RC_EVT_DRP
 * Affected commands:
 *    UWB_RC_CMD_SCAN
 *    UWB_RC_CMD_SET_DRP_IE
 *
 *
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/usb/wusb.h>
#include <linux/usb/wusb-wa.h>
#include <linux/uwb.h>

#include "uwb-internal.h"

#define WUSB_QUIRK_WHCI_CMD_EVT		0x01

struct hwarc {
	struct usb_device *usb_dev;
	struct usb_interface *usb_iface;
	struct uwb_rc *uwb_rc;		
	struct urb *neep_urb;		
	struct edc neep_edc;
	void *rd_buffer;		
};


struct uwb_rc_evt_beacon_WUSB_0100 {
	struct uwb_rceb rceb;
	u8	bChannelNumber;
	__le16	wBPSTOffset;
	u8	bLQI;
	u8	bRSSI;
	__le16	wBeaconInfoLength;
	u8	BeaconInfo[];
} __attribute__((packed));

static
int hwarc_filter_evt_beacon_WUSB_0100(struct uwb_rc *rc,
				      struct uwb_rceb **header,
				      const size_t buf_size,
				      size_t *new_size)
{
	struct uwb_rc_evt_beacon_WUSB_0100 *be;
	struct uwb_rc_evt_beacon *newbe;
	size_t bytes_left, ielength;
	struct device *dev = &rc->uwb_dev.dev;

	be = container_of(*header, struct uwb_rc_evt_beacon_WUSB_0100, rceb);
	bytes_left = buf_size;
	if (bytes_left < sizeof(*be)) {
		dev_err(dev, "Beacon Received Notification: Not enough data "
			"to decode for filtering (%zu vs %zu bytes needed)\n",
			bytes_left, sizeof(*be));
		return -EINVAL;
	}
	bytes_left -= sizeof(*be);
	ielength = le16_to_cpu(be->wBeaconInfoLength);
	if (bytes_left < ielength) {
		dev_err(dev, "Beacon Received Notification: Not enough data "
			"to decode IEs (%zu vs %zu bytes needed)\n",
			bytes_left, ielength);
		return -EINVAL;
	}
	newbe = kzalloc(sizeof(*newbe) + ielength, GFP_ATOMIC);
	if (newbe == NULL)
		return -ENOMEM;
	newbe->rceb = be->rceb;
	newbe->bChannelNumber = be->bChannelNumber;
	newbe->bBeaconType = UWB_RC_BEACON_TYPE_NEIGHBOR;
	newbe->wBPSTOffset = be->wBPSTOffset;
	newbe->bLQI = be->bLQI;
	newbe->bRSSI = be->bRSSI;
	newbe->wBeaconInfoLength = be->wBeaconInfoLength;
	memcpy(newbe->BeaconInfo, be->BeaconInfo, ielength);
	*header = &newbe->rceb;
	*new_size = sizeof(*newbe) + ielength;
	return 1;  
}


struct uwb_rc_evt_drp_avail_WUSB_0100 {
	struct uwb_rceb rceb;
	__le16 wIELength;
	u8 IEData[];
} __attribute__((packed));

static
int hwarc_filter_evt_drp_avail_WUSB_0100(struct uwb_rc *rc,
					 struct uwb_rceb **header,
					 const size_t buf_size,
					 size_t *new_size)
{
	struct uwb_rc_evt_drp_avail_WUSB_0100 *da;
	struct uwb_rc_evt_drp_avail *newda;
	struct uwb_ie_hdr *ie_hdr;
	size_t bytes_left, ielength;
	struct device *dev = &rc->uwb_dev.dev;


	da = container_of(*header, struct uwb_rc_evt_drp_avail_WUSB_0100, rceb);
	bytes_left = buf_size;
	if (bytes_left < sizeof(*da)) {
		dev_err(dev, "Not enough data to decode DRP Avail "
			"Notification for filtering. Expected %zu, "
			"received %zu.\n", (size_t)sizeof(*da), bytes_left);
		return -EINVAL;
	}
	bytes_left -= sizeof(*da);
	ielength = le16_to_cpu(da->wIELength);
	if (bytes_left < ielength) {
		dev_err(dev, "DRP Avail Notification filter: IE length "
			"[%zu bytes] does not match actual length "
			"[%zu bytes].\n", ielength, bytes_left);
		return -EINVAL;
	}
	if (ielength < sizeof(*ie_hdr)) {
		dev_err(dev, "DRP Avail Notification filter: Not enough "
			"data to decode IE [%zu bytes, %zu needed]\n",
			ielength, sizeof(*ie_hdr));
		return -EINVAL;
	}
	ie_hdr = (void *) da->IEData;
	if (ie_hdr->length > 32) {
		dev_err(dev, "DRP Availability Change event has unexpected "
			"length for filtering. Expected < 32 bytes, "
			"got %zu bytes.\n", (size_t)ie_hdr->length);
		return -EINVAL;
	}
	newda = kzalloc(sizeof(*newda), GFP_ATOMIC);
	if (newda == NULL)
		return -ENOMEM;
	newda->rceb = da->rceb;
	memcpy(newda->bmp, (u8 *) ie_hdr + sizeof(*ie_hdr), ie_hdr->length);
	*header = &newda->rceb;
	*new_size = sizeof(*newda);
	return 1; 
}


struct uwb_rc_evt_drp_WUSB_0100 {
	struct uwb_rceb rceb;
	struct uwb_dev_addr wSrcAddr;
	u8 bExplicit;
	__le16 wIELength;
	u8 IEData[];
} __attribute__((packed));

static
int hwarc_filter_evt_drp_WUSB_0100(struct uwb_rc *rc,
				   struct uwb_rceb **header,
				   const size_t buf_size,
				   size_t *new_size)
{
	struct uwb_rc_evt_drp_WUSB_0100 *drpev;
	struct uwb_rc_evt_drp *newdrpev;
	size_t bytes_left, ielength;
	struct device *dev = &rc->uwb_dev.dev;

	drpev = container_of(*header, struct uwb_rc_evt_drp_WUSB_0100, rceb);
	bytes_left = buf_size;
	if (bytes_left < sizeof(*drpev)) {
		dev_err(dev, "Not enough data to decode DRP Notification "
			"for filtering. Expected %zu, received %zu.\n",
			(size_t)sizeof(*drpev), bytes_left);
		return -EINVAL;
	}
	ielength = le16_to_cpu(drpev->wIELength);
	bytes_left -= sizeof(*drpev);
	if (bytes_left < ielength) {
		dev_err(dev, "DRP Notification filter: header length [%zu "
			"bytes] does not match actual length [%zu "
			"bytes].\n", ielength, bytes_left);
		return -EINVAL;
	}
	newdrpev = kzalloc(sizeof(*newdrpev) + ielength, GFP_ATOMIC);
	if (newdrpev == NULL)
		return -ENOMEM;
	newdrpev->rceb = drpev->rceb;
	newdrpev->src_addr = drpev->wSrcAddr;
	newdrpev->reason = UWB_DRP_NOTIF_DRP_IE_RCVD;
	newdrpev->beacon_slot_number = 0;
	newdrpev->ie_length = drpev->wIELength;
	memcpy(newdrpev->ie_data, drpev->IEData, ielength);
	*header = &newdrpev->rceb;
	*new_size = sizeof(*newdrpev) + ielength;
	return 1; 
}


struct uwb_rc_cmd_scan_WUSB_0100 {
	struct uwb_rccb rccb;
	u8 bChannelNumber;
	u8 bScanState;
} __attribute__((packed));

static
int hwarc_filter_cmd_scan_WUSB_0100(struct uwb_rc *rc,
				    struct uwb_rccb **header,
				    size_t *size)
{
	struct uwb_rc_cmd_scan *sc;

	sc = container_of(*header, struct uwb_rc_cmd_scan, rccb);

	if (sc->bScanState == UWB_SCAN_ONLY_STARTTIME)
		sc->bScanState = UWB_SCAN_ONLY;
	
	*size -= 2;
	return 0;
}


struct uwb_rc_cmd_set_drp_ie_WUSB_0100 {
	struct uwb_rccb rccb;
	u8 bExplicit;
	__le16 wIELength;
	struct uwb_ie_drp IEData[];
} __attribute__((packed));

static
int hwarc_filter_cmd_set_drp_ie_WUSB_0100(struct uwb_rc *rc,
					  struct uwb_rccb **header,
					  size_t *size)
{
	struct uwb_rc_cmd_set_drp_ie *orgcmd;
	struct uwb_rc_cmd_set_drp_ie_WUSB_0100 *cmd;
	size_t ielength;

	orgcmd = container_of(*header, struct uwb_rc_cmd_set_drp_ie, rccb);
	ielength = le16_to_cpu(orgcmd->wIELength);
	cmd = kzalloc(sizeof(*cmd) + ielength, GFP_KERNEL);
	if (cmd == NULL)
		return -ENOMEM;
	cmd->rccb = orgcmd->rccb;
	cmd->bExplicit = 0;
	cmd->wIELength = orgcmd->wIELength;
	memcpy(cmd->IEData, orgcmd->IEData, ielength);
	*header = &cmd->rccb;
	*size = sizeof(*cmd) + ielength;
	return 1; 
}


static
int hwarc_filter_cmd_WUSB_0100(struct uwb_rc *rc, struct uwb_rccb **header,
			       size_t *size)
{
	int result;
	struct uwb_rccb *rccb = *header;
	int cmd = le16_to_cpu(rccb->wCommand);
	switch (cmd) {
	case UWB_RC_CMD_SCAN:
		result = hwarc_filter_cmd_scan_WUSB_0100(rc, header, size);
		break;
	case UWB_RC_CMD_SET_DRP_IE:
		result = hwarc_filter_cmd_set_drp_ie_WUSB_0100(rc, header, size);
		break;
	default:
		result = -ENOANO;
		break;
	}
	return result;
}


static
int hwarc_filter_cmd(struct uwb_rc *rc, struct uwb_rccb **header,
		     size_t *size)
{
	int result = -ENOANO;
	if (rc->version == 0x0100)
		result = hwarc_filter_cmd_WUSB_0100(rc, header, size);
	return result;
}


static
ssize_t hwarc_get_event_size(struct uwb_rc *rc, const struct uwb_rceb *rceb,
			     size_t core_size, size_t offset,
			     const size_t buf_size)
{
	ssize_t size = -ENOSPC;
	const void *ptr = rceb;
	size_t type_size = sizeof(__le16);
	struct device *dev = &rc->uwb_dev.dev;

	if (offset + type_size >= buf_size) {
		dev_err(dev, "Not enough data to read extra size of event "
			"0x%02x/%04x/%02x, only got %zu bytes.\n",
			rceb->bEventType, le16_to_cpu(rceb->wEvent),
			rceb->bEventContext, buf_size);
		goto out;
	}
	ptr += offset;
	size = core_size + le16_to_cpu(*(__le16 *)ptr);
out:
	return size;
}


struct uwb_rc_evt_bp_slot_change_WUSB_0100 {
	struct uwb_rceb rceb;
	u8 bSlotNumber;
} __attribute__((packed));


static
int hwarc_filter_event_WUSB_0100(struct uwb_rc *rc, struct uwb_rceb **header,
				 const size_t buf_size, size_t *_real_size,
				 size_t *_new_size)
{
	int result = -ENOANO;
	struct uwb_rceb *rceb = *header;
	int event = le16_to_cpu(rceb->wEvent);
	ssize_t event_size;
	size_t core_size, offset;

	if (rceb->bEventType != UWB_RC_CET_GENERAL)
		goto out;
	switch (event) {
	case UWB_RC_EVT_BEACON:
		core_size = sizeof(struct uwb_rc_evt_beacon_WUSB_0100);
		offset = offsetof(struct uwb_rc_evt_beacon_WUSB_0100,
				  wBeaconInfoLength);
		event_size = hwarc_get_event_size(rc, rceb, core_size,
						  offset, buf_size);
		if (event_size < 0)
			goto out;
		*_real_size = event_size;
		result = hwarc_filter_evt_beacon_WUSB_0100(rc, header,
							   buf_size, _new_size);
		break;
	case UWB_RC_EVT_BP_SLOT_CHANGE:
		*_new_size = *_real_size =
			sizeof(struct uwb_rc_evt_bp_slot_change_WUSB_0100);
		result = 0;
		break;

	case UWB_RC_EVT_DRP_AVAIL:
		core_size = sizeof(struct uwb_rc_evt_drp_avail_WUSB_0100);
		offset = offsetof(struct uwb_rc_evt_drp_avail_WUSB_0100,
				  wIELength);
		event_size = hwarc_get_event_size(rc, rceb, core_size,
						  offset, buf_size);
		if (event_size < 0)
			goto out;
		*_real_size = event_size;
		result = hwarc_filter_evt_drp_avail_WUSB_0100(
			rc, header, buf_size, _new_size);
		break;

	case UWB_RC_EVT_DRP:
		core_size = sizeof(struct uwb_rc_evt_drp_WUSB_0100);
		offset = offsetof(struct uwb_rc_evt_drp_WUSB_0100, wIELength);
		event_size = hwarc_get_event_size(rc, rceb, core_size,
						  offset, buf_size);
		if (event_size < 0)
			goto out;
		*_real_size = event_size;
		result = hwarc_filter_evt_drp_WUSB_0100(rc, header,
							buf_size, _new_size);
		break;

	default:
		break;
	}
out:
	return result;
}

static
int hwarc_filter_event(struct uwb_rc *rc, struct uwb_rceb **header,
		       const size_t buf_size, size_t *_real_size,
		       size_t *_new_size)
{
	int result = -ENOANO;
	if (rc->version == 0x0100)
		result =  hwarc_filter_event_WUSB_0100(
			rc, header, buf_size, _real_size, _new_size);
	return result;
}


static
int hwarc_cmd(struct uwb_rc *uwb_rc, const struct uwb_rccb *cmd, size_t cmd_size)
{
	struct hwarc *hwarc = uwb_rc->priv;
	return usb_control_msg(
		hwarc->usb_dev, usb_sndctrlpipe(hwarc->usb_dev, 0),
		WA_EXEC_RC_CMD, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
		0, hwarc->usb_iface->cur_altsetting->desc.bInterfaceNumber,
		(void *) cmd, cmd_size, 100 );
}

static
int hwarc_reset(struct uwb_rc *uwb_rc)
{
	struct hwarc *hwarc = uwb_rc->priv;
	return usb_reset_device(hwarc->usb_dev);
}

static
void hwarc_neep_cb(struct urb *urb)
{
	struct hwarc *hwarc = urb->context;
	struct usb_interface *usb_iface = hwarc->usb_iface;
	struct device *dev = &usb_iface->dev;
	int result;

	switch (result = urb->status) {
	case 0:
		uwb_rc_neh_grok(hwarc->uwb_rc, urb->transfer_buffer,
				urb->actual_length);
		break;
	case -ECONNRESET:	
	case -ENOENT:		
		goto out;
	case -ESHUTDOWN:	
		goto out;
	default:		
		if (edc_inc(&hwarc->neep_edc, EDC_MAX_ERRORS,
			    EDC_ERROR_TIMEFRAME))
			goto error_exceeded;
		dev_err(dev, "NEEP: URB error %d\n", urb->status);
	}
	result = usb_submit_urb(urb, GFP_ATOMIC);
	if (result < 0 && result != -ENODEV && result != -EPERM) {
		
		dev_err(dev, "NEEP: Can't resubmit URB (%d) resetting device\n",
			result);
		goto error;
	}
out:
	return;

error_exceeded:
	dev_err(dev, "NEEP: URB max acceptable errors "
		"exceeded, resetting device\n");
error:
	uwb_rc_neh_error(hwarc->uwb_rc, result);
	uwb_rc_reset_all(hwarc->uwb_rc);
	return;
}

static void hwarc_init(struct hwarc *hwarc)
{
	edc_init(&hwarc->neep_edc);
}

static int hwarc_neep_init(struct uwb_rc *rc)
{
	struct hwarc *hwarc = rc->priv;
	struct usb_interface *iface = hwarc->usb_iface;
	struct usb_device *usb_dev = interface_to_usbdev(iface);
	struct device *dev = &iface->dev;
	int result;
	struct usb_endpoint_descriptor *epd;

	epd = &iface->cur_altsetting->endpoint[0].desc;
	hwarc->rd_buffer = (void *) __get_free_page(GFP_KERNEL);
	if (hwarc->rd_buffer == NULL) {
		dev_err(dev, "Unable to allocate notification's read buffer\n");
		goto error_rd_buffer;
	}
	hwarc->neep_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (hwarc->neep_urb == NULL) {
		dev_err(dev, "Unable to allocate notification URB\n");
		goto error_urb_alloc;
	}
	usb_fill_int_urb(hwarc->neep_urb, usb_dev,
			 usb_rcvintpipe(usb_dev, epd->bEndpointAddress),
			 hwarc->rd_buffer, PAGE_SIZE,
			 hwarc_neep_cb, hwarc, epd->bInterval);
	result = usb_submit_urb(hwarc->neep_urb, GFP_ATOMIC);
	if (result < 0) {
		dev_err(dev, "Cannot submit notification URB: %d\n", result);
		goto error_neep_submit;
	}
	return 0;

error_neep_submit:
	usb_free_urb(hwarc->neep_urb);
error_urb_alloc:
	free_page((unsigned long)hwarc->rd_buffer);
error_rd_buffer:
	return -ENOMEM;
}


static void hwarc_neep_release(struct uwb_rc *rc)
{
	struct hwarc *hwarc = rc->priv;

	usb_kill_urb(hwarc->neep_urb);
	usb_free_urb(hwarc->neep_urb);
	free_page((unsigned long)hwarc->rd_buffer);
}

static int hwarc_get_version(struct uwb_rc *rc)
{
	int result;

	struct hwarc *hwarc = rc->priv;
	struct uwb_rc_control_intf_class_desc *descr;
	struct device *dev = &rc->uwb_dev.dev;
	struct usb_device *usb_dev = hwarc->usb_dev;
	char *itr;
	struct usb_descriptor_header *hdr;
	size_t itr_size, actconfig_idx;
	u16 version;

	actconfig_idx = (usb_dev->actconfig - usb_dev->config) /
		sizeof(usb_dev->config[0]);
	itr = usb_dev->rawdescriptors[actconfig_idx];
	itr_size = le16_to_cpu(usb_dev->actconfig->desc.wTotalLength);
	while (itr_size >= sizeof(*hdr)) {
		hdr = (struct usb_descriptor_header *) itr;
		dev_dbg(dev, "Extra device descriptor: "
			"type %02x/%u bytes @ %zu (%zu left)\n",
			hdr->bDescriptorType, hdr->bLength,
			(itr - usb_dev->rawdescriptors[actconfig_idx]),
			itr_size);
		if (hdr->bDescriptorType == USB_DT_CS_RADIO_CONTROL)
			goto found;
		itr += hdr->bLength;
		itr_size -= hdr->bLength;
	}
	dev_err(dev, "cannot find Radio Control Interface Class descriptor\n");
	return -ENODEV;

found:
	result = -EINVAL;
	if (hdr->bLength > itr_size) {	
		dev_err(dev, "incomplete Radio Control Interface Class "
			"descriptor (%zu bytes left, %u needed)\n",
			itr_size, hdr->bLength);
		goto error;
	}
	if (hdr->bLength < sizeof(*descr)) {
		dev_err(dev, "short Radio Control Interface Class "
			"descriptor\n");
		goto error;
	}
	descr = (struct uwb_rc_control_intf_class_desc *) hdr;
	
	version = __le16_to_cpu(descr->bcdRCIVersion);
	if (version != 0x0100) {
		dev_err(dev, "Device reports protocol version 0x%04x. We "
			"do not support that. \n", version);
		result = -EINVAL;
		goto error;
	}
	rc->version = version;
	dev_dbg(dev, "Device supports WUSB protocol version 0x%04x \n",	rc->version);
	result = 0;
error:
	return result;
}

static int hwarc_probe(struct usb_interface *iface,
		       const struct usb_device_id *id)
{
	int result;
	struct uwb_rc *uwb_rc;
	struct hwarc *hwarc;
	struct device *dev = &iface->dev;

	result = -ENOMEM;
	uwb_rc = uwb_rc_alloc();
	if (uwb_rc == NULL) {
		dev_err(dev, "unable to allocate RC instance\n");
		goto error_rc_alloc;
	}
	hwarc = kzalloc(sizeof(*hwarc), GFP_KERNEL);
	if (hwarc == NULL) {
		dev_err(dev, "unable to allocate HWA RC instance\n");
		goto error_alloc;
	}
	hwarc_init(hwarc);
	hwarc->usb_dev = usb_get_dev(interface_to_usbdev(iface));
	hwarc->usb_iface = usb_get_intf(iface);
	hwarc->uwb_rc = uwb_rc;

	uwb_rc->owner = THIS_MODULE;
	uwb_rc->start = hwarc_neep_init;
	uwb_rc->stop  = hwarc_neep_release;
	uwb_rc->cmd   = hwarc_cmd;
	uwb_rc->reset = hwarc_reset;
	if (id->driver_info & WUSB_QUIRK_WHCI_CMD_EVT) {
		uwb_rc->filter_cmd   = NULL;
		uwb_rc->filter_event = NULL;
	} else {
		uwb_rc->filter_cmd   = hwarc_filter_cmd;
		uwb_rc->filter_event = hwarc_filter_event;
	}

	result = uwb_rc_add(uwb_rc, dev, hwarc);
	if (result < 0)
		goto error_rc_add;
	result = hwarc_get_version(uwb_rc);
	if (result < 0) {
		dev_err(dev, "cannot retrieve version of RC \n");
		goto error_get_version;
	}
	usb_set_intfdata(iface, hwarc);
	return 0;

error_get_version:
	uwb_rc_rm(uwb_rc);
error_rc_add:
	usb_put_intf(iface);
	usb_put_dev(hwarc->usb_dev);
error_alloc:
	uwb_rc_put(uwb_rc);
error_rc_alloc:
	return result;
}

static void hwarc_disconnect(struct usb_interface *iface)
{
	struct hwarc *hwarc = usb_get_intfdata(iface);
	struct uwb_rc *uwb_rc = hwarc->uwb_rc;

	usb_set_intfdata(hwarc->usb_iface, NULL);
	uwb_rc_rm(uwb_rc);
	usb_put_intf(hwarc->usb_iface);
	usb_put_dev(hwarc->usb_dev);
	kfree(hwarc);
	uwb_rc_put(uwb_rc);	
}

static int hwarc_pre_reset(struct usb_interface *iface)
{
	struct hwarc *hwarc = usb_get_intfdata(iface);
	struct uwb_rc *uwb_rc = hwarc->uwb_rc;

	uwb_rc_pre_reset(uwb_rc);
	return 0;
}

static int hwarc_post_reset(struct usb_interface *iface)
{
	struct hwarc *hwarc = usb_get_intfdata(iface);
	struct uwb_rc *uwb_rc = hwarc->uwb_rc;

	return uwb_rc_post_reset(uwb_rc);
}

static const struct usb_device_id hwarc_id_table[] = {
	
	{ USB_DEVICE_AND_INTERFACE_INFO(0x07d1, 0x3d02, 0xe0, 0x01, 0x02),
	  .driver_info = WUSB_QUIRK_WHCI_CMD_EVT },
	
	{ USB_DEVICE_AND_INTERFACE_INFO(0x8086, 0x0c3b, 0xe0, 0x01, 0x02),
	  .driver_info = WUSB_QUIRK_WHCI_CMD_EVT },
	
	{ USB_INTERFACE_INFO(0xe0, 0x01, 0x02), },
	{ },
};
MODULE_DEVICE_TABLE(usb, hwarc_id_table);

static struct usb_driver hwarc_driver = {
	.name =		"hwa-rc",
	.id_table =	hwarc_id_table,
	.probe =	hwarc_probe,
	.disconnect =	hwarc_disconnect,
	.pre_reset =    hwarc_pre_reset,
	.post_reset =   hwarc_post_reset,
};

module_usb_driver(hwarc_driver);

MODULE_AUTHOR("Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>");
MODULE_DESCRIPTION("Host Wireless Adapter Radio Control Driver");
MODULE_LICENSE("GPL");
