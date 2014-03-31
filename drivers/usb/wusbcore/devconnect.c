/*
 * WUSB Wire Adapter: Control/Data Streaming Interface (WUSB[8])
 * Device Connect handling
 *
 * Copyright (C) 2006 Intel Corporation
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
 * FIXME: docs
 * FIXME: this file needs to be broken up, it's grown too big
 *
 *
 * WUSB1.0[7.1, 7.5.1, ]
 *
 * WUSB device connection is kind of messy. Some background:
 *
 *     When a device wants to connect it scans the UWB radio channels
 *     looking for a WUSB Channel; a WUSB channel is defined by MMCs
 *     (Micro Managed Commands or something like that) [see
 *     Design-overview for more on this] .
 *
 * So, device scans the radio, finds MMCs and thus a host and checks
 * when the next DNTS is. It sends a Device Notification Connect
 * (DN_Connect); the host picks it up (through nep.c and notif.c, ends
 * up in wusb_devconnect_ack(), which creates a wusb_dev structure in
 * wusbhc->port[port_number].wusb_dev), assigns an unauth address
 * to the device (this means from 0x80 to 0xfe) and sends, in the MMC
 * a Connect Ack Information Element (ConnAck IE).
 *
 * So now the device now has a WUSB address. From now on, we use
 * that to talk to it in the RPipes.
 *
 * ASSUMPTIONS:
 *
 *  - We use the the as device address the port number where it is
 *    connected (port 0 doesn't exist). For unauth, it is 128 + that.
 *
 * ROADMAP:
 *
 *   This file contains the logic for doing that--entry points:
 *
 *   wusb_devconnect_ack()      Ack a device until _acked() called.
 *                              Called by notif.c:wusb_handle_dn_connect()
 *                              when a DN_Connect is received.
 *
 *     wusb_devconnect_acked()  Ack done, release resources.
 *
 *   wusb_handle_dn_alive()     Called by notif.c:wusb_handle_dn()
 *                              for processing a DN_Alive pong from a device.
 *
 *   wusb_handle_dn_disconnect()Called by notif.c:wusb_handle_dn() to
 *                              process a disconenct request from a
 *                              device.
 *
 *   __wusb_dev_disable()       Called by rh.c:wusbhc_rh_clear_port_feat() when
 *                              disabling a port.
 *
 *   wusb_devconnect_create()   Called when creating the host by
 *                              lc.c:wusbhc_create().
 *
 *   wusb_devconnect_destroy()  Cleanup called removing the host. Called
 *                              by lc.c:wusbhc_destroy().
 *
 *   Each Wireless USB host maintains a list of DN_Connect requests
 *   (actually we maintain a list of pending Connect Acks, the
 *   wusbhc->ca_list).
 *
 * LIFE CYCLE OF port->wusb_dev
 *
 *   Before the @wusbhc structure put()s the reference it owns for
 *   port->wusb_dev [and clean the wusb_dev pointer], it needs to
 *   lock @wusbhc->mutex.
 */

#include <linux/jiffies.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/export.h>
#include "wusbhc.h"

static void wusbhc_devconnect_acked_work(struct work_struct *work);

static void wusb_dev_free(struct wusb_dev *wusb_dev)
{
	if (wusb_dev) {
		kfree(wusb_dev->set_gtk_req);
		usb_free_urb(wusb_dev->set_gtk_urb);
		kfree(wusb_dev);
	}
}

static struct wusb_dev *wusb_dev_alloc(struct wusbhc *wusbhc)
{
	struct wusb_dev *wusb_dev;
	struct urb *urb;
	struct usb_ctrlrequest *req;

	wusb_dev = kzalloc(sizeof(*wusb_dev), GFP_KERNEL);
	if (wusb_dev == NULL)
		goto err;

	wusb_dev->wusbhc = wusbhc;

	INIT_WORK(&wusb_dev->devconnect_acked_work, wusbhc_devconnect_acked_work);

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (urb == NULL)
		goto err;
	wusb_dev->set_gtk_urb = urb;

	req = kmalloc(sizeof(*req), GFP_KERNEL);
	if (req == NULL)
		goto err;
	wusb_dev->set_gtk_req = req;

	req->bRequestType = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
	req->bRequest = USB_REQ_SET_DESCRIPTOR;
	req->wValue = cpu_to_le16(USB_DT_KEY << 8 | wusbhc->gtk_index);
	req->wIndex = 0;
	req->wLength = cpu_to_le16(wusbhc->gtk.descr.bLength);

	return wusb_dev;
err:
	wusb_dev_free(wusb_dev);
	return NULL;
}


static void wusbhc_fill_cack_ie(struct wusbhc *wusbhc)
{
	unsigned cnt;
	struct wusb_dev *dev_itr;
	struct wuie_connect_ack *cack_ie;

	cack_ie = &wusbhc->cack_ie;
	cnt = 0;
	list_for_each_entry(dev_itr, &wusbhc->cack_list, cack_node) {
		cack_ie->blk[cnt].CDID = dev_itr->cdid;
		cack_ie->blk[cnt].bDeviceAddress = dev_itr->addr;
		if (++cnt >= WUIE_ELT_MAX)
			break;
	}
	cack_ie->hdr.bLength = sizeof(cack_ie->hdr)
		+ cnt * sizeof(cack_ie->blk[0]);
}

static struct wusb_dev *wusbhc_cack_add(struct wusbhc *wusbhc,
					struct wusb_dn_connect *dnc,
					const char *pr_cdid, u8 port_idx)
{
	struct device *dev = wusbhc->dev;
	struct wusb_dev *wusb_dev;
	int new_connection = wusb_dn_connect_new_connection(dnc);
	u8 dev_addr;
	int result;

	
	list_for_each_entry(wusb_dev, &wusbhc->cack_list, cack_node)
		if (!memcmp(&wusb_dev->cdid, &dnc->CDID,
			    sizeof(wusb_dev->cdid)))
			return wusb_dev;
	
	wusb_dev = wusb_dev_alloc(wusbhc);
	if (wusb_dev == NULL)
		return NULL;
	wusb_dev_init(wusb_dev);
	wusb_dev->cdid = dnc->CDID;
	wusb_dev->port_idx = port_idx;

	bitmap_fill(wusb_dev->availability.bm, UWB_NUM_MAS);

	if (1 && new_connection == 0)
		new_connection = 1;
	if (new_connection) {
		dev_addr = (port_idx + 2) | WUSB_DEV_ADDR_UNAUTH;

		dev_info(dev, "Connecting new WUSB device to address %u, "
			"port %u\n", dev_addr, port_idx);

		result = wusb_set_dev_addr(wusbhc, wusb_dev, dev_addr);
		if (result < 0)
			return NULL;
	}
	wusb_dev->entry_ts = jiffies;
	list_add_tail(&wusb_dev->cack_node, &wusbhc->cack_list);
	wusbhc->cack_count++;
	wusbhc_fill_cack_ie(wusbhc);

	return wusb_dev;
}

static void wusbhc_cack_rm(struct wusbhc *wusbhc, struct wusb_dev *wusb_dev)
{
	list_del_init(&wusb_dev->cack_node);
	wusbhc->cack_count--;
	wusbhc_fill_cack_ie(wusbhc);
}

static
void wusbhc_devconnect_acked(struct wusbhc *wusbhc, struct wusb_dev *wusb_dev)
{
	wusbhc_cack_rm(wusbhc, wusb_dev);
	if (wusbhc->cack_count)
		wusbhc_mmcie_set(wusbhc, 0, 0, &wusbhc->cack_ie.hdr);
	else
		wusbhc_mmcie_rm(wusbhc, &wusbhc->cack_ie.hdr);
}

static void wusbhc_devconnect_acked_work(struct work_struct *work)
{
	struct wusb_dev *wusb_dev = container_of(work, struct wusb_dev,
						 devconnect_acked_work);
	struct wusbhc *wusbhc = wusb_dev->wusbhc;

	mutex_lock(&wusbhc->mutex);
	wusbhc_devconnect_acked(wusbhc, wusb_dev);
	mutex_unlock(&wusbhc->mutex);

	wusb_dev_put(wusb_dev);
}

static
void wusbhc_devconnect_ack(struct wusbhc *wusbhc, struct wusb_dn_connect *dnc,
			   const char *pr_cdid)
{
	int result;
	struct device *dev = wusbhc->dev;
	struct wusb_dev *wusb_dev;
	struct wusb_port *port;
	unsigned idx, devnum;

	mutex_lock(&wusbhc->mutex);

	
	for (idx = 0; idx < wusbhc->ports_max; idx++) {
		port = wusb_port_by_idx(wusbhc, idx);
		if (port->wusb_dev
		    && memcmp(&dnc->CDID, &port->wusb_dev->cdid, sizeof(dnc->CDID)) == 0)
			goto error_unlock;
	}
	
	for (idx = 0; idx < wusbhc->ports_max; idx++) {
		port = wusb_port_by_idx(wusbhc, idx);
		if ((port->status & USB_PORT_STAT_POWER)
		    && !(port->status & USB_PORT_STAT_CONNECTION))
			break;
	}
	if (idx >= wusbhc->ports_max) {
		dev_err(dev, "Host controller can't connect more devices "
			"(%u already connected); device %s rejected\n",
			wusbhc->ports_max, pr_cdid);
		goto error_unlock;
	}

	devnum = idx + 2;

	
	wusbhc->set_ptk(wusbhc, idx, 0, NULL, 0);

	wusb_dev = wusbhc_cack_add(wusbhc, dnc, pr_cdid, idx);
	if (wusb_dev == NULL)
		goto error_unlock;
	result = wusbhc_mmcie_set(wusbhc, 0, 0, &wusbhc->cack_ie.hdr);
	if (result < 0)
		goto error_unlock;
	msleep(3);
	port->wusb_dev = wusb_dev;
	port->status |= USB_PORT_STAT_CONNECTION;
	port->change |= USB_PORT_STAT_C_CONNECTION;
error_unlock:
	mutex_unlock(&wusbhc->mutex);
	return;

}

static void __wusbhc_dev_disconnect(struct wusbhc *wusbhc,
				    struct wusb_port *port)
{
	struct wusb_dev *wusb_dev = port->wusb_dev;

	port->status &= ~(USB_PORT_STAT_CONNECTION | USB_PORT_STAT_ENABLE
			  | USB_PORT_STAT_SUSPEND | USB_PORT_STAT_RESET
			  | USB_PORT_STAT_LOW_SPEED | USB_PORT_STAT_HIGH_SPEED);
	port->change |= USB_PORT_STAT_C_CONNECTION | USB_PORT_STAT_C_ENABLE;
	if (wusb_dev) {
		dev_dbg(wusbhc->dev, "disconnecting device from port %d\n", wusb_dev->port_idx);
		if (!list_empty(&wusb_dev->cack_node))
			list_del_init(&wusb_dev->cack_node);
		
		wusb_dev_put(wusb_dev);
	}
	port->wusb_dev = NULL;

	if (wusbhc->active)
		wusbhc_gtk_rekey(wusbhc);

}

static void __wusbhc_keep_alive(struct wusbhc *wusbhc)
{
	struct device *dev = wusbhc->dev;
	unsigned cnt;
	struct wusb_dev *wusb_dev;
	struct wusb_port *wusb_port;
	struct wuie_keep_alive *ie = &wusbhc->keep_alive_ie;
	unsigned keep_alives, old_keep_alives;

	old_keep_alives = ie->hdr.bLength - sizeof(ie->hdr);
	keep_alives = 0;
	for (cnt = 0;
	     keep_alives < WUIE_ELT_MAX && cnt < wusbhc->ports_max;
	     cnt++) {
		unsigned tt = msecs_to_jiffies(wusbhc->trust_timeout);

		wusb_port = wusb_port_by_idx(wusbhc, cnt);
		wusb_dev = wusb_port->wusb_dev;

		if (wusb_dev == NULL)
			continue;
		if (wusb_dev->usb_dev == NULL || !wusb_dev->usb_dev->authenticated)
			continue;

		if (time_after(jiffies, wusb_dev->entry_ts + tt)) {
			dev_err(dev, "KEEPALIVE: device %u timed out\n",
				wusb_dev->addr);
			__wusbhc_dev_disconnect(wusbhc, wusb_port);
		} else if (time_after(jiffies, wusb_dev->entry_ts + tt/2)) {
			
			ie->bDeviceAddress[keep_alives++] = wusb_dev->addr;
		}
	}
	if (keep_alives & 0x1)	
		ie->bDeviceAddress[keep_alives++] = 0x7f;
	ie->hdr.bLength = sizeof(ie->hdr) +
		keep_alives*sizeof(ie->bDeviceAddress[0]);
	if (keep_alives > 0)
		wusbhc_mmcie_set(wusbhc, 10, 5, &ie->hdr);
	else if (old_keep_alives != 0)
		wusbhc_mmcie_rm(wusbhc, &ie->hdr);
}

static void wusbhc_keep_alive_run(struct work_struct *ws)
{
	struct delayed_work *dw = to_delayed_work(ws);
	struct wusbhc *wusbhc =	container_of(dw, struct wusbhc, keep_alive_timer);

	mutex_lock(&wusbhc->mutex);
	__wusbhc_keep_alive(wusbhc);
	mutex_unlock(&wusbhc->mutex);

	queue_delayed_work(wusbd, &wusbhc->keep_alive_timer,
			   msecs_to_jiffies(wusbhc->trust_timeout / 2));
}

static struct wusb_dev *wusbhc_find_dev_by_addr(struct wusbhc *wusbhc, u8 addr)
{
	int p;

	if (addr == 0xff) 
		return NULL;

	if (addr > 0) {
		int port = (addr & ~0x80) - 2;
		if (port < 0 || port >= wusbhc->ports_max)
			return NULL;
		return wusb_port_by_idx(wusbhc, port)->wusb_dev;
	}

	
	for (p = 0; p < wusbhc->ports_max; p++) {
		struct wusb_dev *wusb_dev = wusb_port_by_idx(wusbhc, p)->wusb_dev;
		if (wusb_dev && wusb_dev->addr == addr)
			return wusb_dev;
	}
	return NULL;
}

static void wusbhc_handle_dn_alive(struct wusbhc *wusbhc, struct wusb_dev *wusb_dev)
{
	mutex_lock(&wusbhc->mutex);
	wusb_dev->entry_ts = jiffies;
	__wusbhc_keep_alive(wusbhc);
	mutex_unlock(&wusbhc->mutex);
}

static void wusbhc_handle_dn_connect(struct wusbhc *wusbhc,
				     struct wusb_dn_hdr *dn_hdr,
				     size_t size)
{
	struct device *dev = wusbhc->dev;
	struct wusb_dn_connect *dnc;
	char pr_cdid[WUSB_CKHDID_STRSIZE];
	static const char *beacon_behaviour[] = {
		"reserved",
		"self-beacon",
		"directed-beacon",
		"no-beacon"
	};

	if (size < sizeof(*dnc)) {
		dev_err(dev, "DN CONNECT: short notification (%zu < %zu)\n",
			size, sizeof(*dnc));
		return;
	}

	dnc = container_of(dn_hdr, struct wusb_dn_connect, hdr);
	ckhdid_printf(pr_cdid, sizeof(pr_cdid), &dnc->CDID);
	dev_info(dev, "DN CONNECT: device %s @ %x (%s) wants to %s\n",
		 pr_cdid,
		 wusb_dn_connect_prev_dev_addr(dnc),
		 beacon_behaviour[wusb_dn_connect_beacon_behavior(dnc)],
		 wusb_dn_connect_new_connection(dnc) ? "connect" : "reconnect");
	
	wusbhc_devconnect_ack(wusbhc, dnc, pr_cdid);
}

static void wusbhc_handle_dn_disconnect(struct wusbhc *wusbhc, struct wusb_dev *wusb_dev)
{
	struct device *dev = wusbhc->dev;

	dev_info(dev, "DN DISCONNECT: device 0x%02x going down\n", wusb_dev->addr);

	mutex_lock(&wusbhc->mutex);
	__wusbhc_dev_disconnect(wusbhc, wusb_port_by_idx(wusbhc, wusb_dev->port_idx));
	mutex_unlock(&wusbhc->mutex);
}

void wusbhc_handle_dn(struct wusbhc *wusbhc, u8 srcaddr,
		      struct wusb_dn_hdr *dn_hdr, size_t size)
{
	struct device *dev = wusbhc->dev;
	struct wusb_dev *wusb_dev;

	if (size < sizeof(struct wusb_dn_hdr)) {
		dev_err(dev, "DN data shorter than DN header (%d < %d)\n",
			(int)size, (int)sizeof(struct wusb_dn_hdr));
		return;
	}

	wusb_dev = wusbhc_find_dev_by_addr(wusbhc, srcaddr);
	if (wusb_dev == NULL && dn_hdr->bType != WUSB_DN_CONNECT) {
		dev_dbg(dev, "ignoring DN %d from unconnected device %02x\n",
			dn_hdr->bType, srcaddr);
		return;
	}

	switch (dn_hdr->bType) {
	case WUSB_DN_CONNECT:
		wusbhc_handle_dn_connect(wusbhc, dn_hdr, size);
		break;
	case WUSB_DN_ALIVE:
		wusbhc_handle_dn_alive(wusbhc, wusb_dev);
		break;
	case WUSB_DN_DISCONNECT:
		wusbhc_handle_dn_disconnect(wusbhc, wusb_dev);
		break;
	case WUSB_DN_MASAVAILCHANGED:
	case WUSB_DN_RWAKE:
	case WUSB_DN_SLEEP:
		
		break;
	case WUSB_DN_EPRDY:
		
		break;
	default:
		dev_warn(dev, "unknown DN %u (%d octets) from %u\n",
			 dn_hdr->bType, (int)size, srcaddr);
	}
}
EXPORT_SYMBOL_GPL(wusbhc_handle_dn);

void __wusbhc_dev_disable(struct wusbhc *wusbhc, u8 port_idx)
{
	int result;
	struct device *dev = wusbhc->dev;
	struct wusb_dev *wusb_dev;
	struct wuie_disconnect *ie;

	wusb_dev = wusb_port_by_idx(wusbhc, port_idx)->wusb_dev;
	if (wusb_dev == NULL) {
		
		dev_dbg(dev, "DISCONNECT: no device at port %u, ignoring\n",
			port_idx);
		return;
	}
	__wusbhc_dev_disconnect(wusbhc, wusb_port_by_idx(wusbhc, port_idx));

	ie = kzalloc(sizeof(*ie), GFP_KERNEL);
	if (ie == NULL)
		return;
	ie->hdr.bLength = sizeof(*ie);
	ie->hdr.bIEIdentifier = WUIE_ID_DEVICE_DISCONNECT;
	ie->bDeviceAddress = wusb_dev->addr;
	result = wusbhc_mmcie_set(wusbhc, 0, 0, &ie->hdr);
	if (result < 0)
		dev_err(dev, "DISCONNECT: can't set MMC: %d\n", result);
	else {
		
		msleep(7*4);
		wusbhc_mmcie_rm(wusbhc, &ie->hdr);
	}
	kfree(ie);
}

static int wusb_dev_bos_grok(struct usb_device *usb_dev,
			     struct wusb_dev *wusb_dev,
			     struct usb_bos_descriptor *bos, size_t desc_size)
{
	ssize_t result;
	struct device *dev = &usb_dev->dev;
	void *itr, *top;

	
	itr = (void *)bos + sizeof(*bos);
	top = itr + desc_size - sizeof(*bos);
	while (itr < top) {
		struct usb_dev_cap_header *cap_hdr = itr;
		size_t cap_size;
		u8 cap_type;
		if (top - itr < sizeof(*cap_hdr)) {
			dev_err(dev, "Device BUG? premature end of BOS header "
				"data [offset 0x%02x]: only %zu bytes left\n",
				(int)(itr - (void *)bos), top - itr);
			result = -ENOSPC;
			goto error_bad_cap;
		}
		cap_size = cap_hdr->bLength;
		cap_type = cap_hdr->bDevCapabilityType;
		if (cap_size == 0)
			break;
		if (cap_size > top - itr) {
			dev_err(dev, "Device BUG? premature end of BOS data "
				"[offset 0x%02x cap %02x %zu bytes]: "
				"only %zu bytes left\n",
				(int)(itr - (void *)bos),
				cap_type, cap_size, top - itr);
			result = -EBADF;
			goto error_bad_cap;
		}
		switch (cap_type) {
		case USB_CAP_TYPE_WIRELESS_USB:
			if (cap_size != sizeof(*wusb_dev->wusb_cap_descr))
				dev_err(dev, "Device BUG? WUSB Capability "
					"descriptor is %zu bytes vs %zu "
					"needed\n", cap_size,
					sizeof(*wusb_dev->wusb_cap_descr));
			else
				wusb_dev->wusb_cap_descr = itr;
			break;
		default:
			dev_err(dev, "BUG? Unknown BOS capability 0x%02x "
				"(%zu bytes) at offset 0x%02x\n", cap_type,
				cap_size, (int)(itr - (void *)bos));
		}
		itr += cap_size;
	}
	result = 0;
error_bad_cap:
	return result;
}

static int wusb_dev_bos_add(struct usb_device *usb_dev,
			    struct wusb_dev *wusb_dev)
{
	ssize_t result;
	struct device *dev = &usb_dev->dev;
	struct usb_bos_descriptor *bos;
	size_t alloc_size = 32, desc_size = 4;

	bos = kmalloc(alloc_size, GFP_KERNEL);
	if (bos == NULL)
		return -ENOMEM;
	result = usb_get_descriptor(usb_dev, USB_DT_BOS, 0, bos, desc_size);
	if (result < 4) {
		dev_err(dev, "Can't get BOS descriptor or too short: %zd\n",
			result);
		goto error_get_descriptor;
	}
	desc_size = le16_to_cpu(bos->wTotalLength);
	if (desc_size >= alloc_size) {
		kfree(bos);
		alloc_size = desc_size;
		bos = kmalloc(alloc_size, GFP_KERNEL);
		if (bos == NULL)
			return -ENOMEM;
	}
	result = usb_get_descriptor(usb_dev, USB_DT_BOS, 0, bos, desc_size);
	if (result < 0 || result != desc_size) {
		dev_err(dev, "Can't get  BOS descriptor or too short (need "
			"%zu bytes): %zd\n", desc_size, result);
		goto error_get_descriptor;
	}
	if (result < sizeof(*bos)
	    || le16_to_cpu(bos->wTotalLength) != desc_size) {
		dev_err(dev, "Can't get  BOS descriptor or too short (need "
			"%zu bytes): %zd\n", desc_size, result);
		goto error_get_descriptor;
	}

	result = wusb_dev_bos_grok(usb_dev, wusb_dev, bos, result);
	if (result < 0)
		goto error_bad_bos;
	wusb_dev->bos = bos;
	return 0;

error_bad_bos:
error_get_descriptor:
	kfree(bos);
	wusb_dev->wusb_cap_descr = NULL;
	return result;
}

static void wusb_dev_bos_rm(struct wusb_dev *wusb_dev)
{
	kfree(wusb_dev->bos);
	wusb_dev->wusb_cap_descr = NULL;
};

static struct usb_wireless_cap_descriptor wusb_cap_descr_default = {
	.bLength = sizeof(wusb_cap_descr_default),
	.bDescriptorType = USB_DT_DEVICE_CAPABILITY,
	.bDevCapabilityType = USB_CAP_TYPE_WIRELESS_USB,

	.bmAttributes = USB_WIRELESS_BEACON_NONE,
	.wPHYRates = cpu_to_le16(USB_WIRELESS_PHY_53),
	.bmTFITXPowerInfo = 0,
	.bmFFITXPowerInfo = 0,
	.bmBandGroup = cpu_to_le16(0x0001),	
	.bReserved = 0
};

static void wusb_dev_add_ncb(struct usb_device *usb_dev)
{
	int result = 0;
	struct wusb_dev *wusb_dev;
	struct wusbhc *wusbhc;
	struct device *dev = &usb_dev->dev;
	u8 port_idx;

	if (usb_dev->wusb == 0 || usb_dev->devnum == 1)
		return;		

	usb_set_device_state(usb_dev, USB_STATE_UNAUTHENTICATED);

	wusbhc = wusbhc_get_by_usb_dev(usb_dev);
	if (wusbhc == NULL)
		goto error_nodev;
	mutex_lock(&wusbhc->mutex);
	wusb_dev = __wusb_dev_get_by_usb_dev(wusbhc, usb_dev);
	port_idx = wusb_port_no_to_idx(usb_dev->portnum);
	mutex_unlock(&wusbhc->mutex);
	if (wusb_dev == NULL)
		goto error_nodev;
	wusb_dev->usb_dev = usb_get_dev(usb_dev);
	usb_dev->wusb_dev = wusb_dev_get(wusb_dev);
	result = wusb_dev_sec_add(wusbhc, usb_dev, wusb_dev);
	if (result < 0) {
		dev_err(dev, "Cannot enable security: %d\n", result);
		goto error_sec_add;
	}
	
	result = wusb_dev_bos_add(usb_dev, wusb_dev);
	if (result < 0) {
		dev_err(dev, "Cannot get BOS descriptors: %d\n", result);
		goto error_bos_add;
	}
	result = wusb_dev_sysfs_add(wusbhc, usb_dev, wusb_dev);
	if (result < 0)
		goto error_add_sysfs;
out:
	wusb_dev_put(wusb_dev);
	wusbhc_put(wusbhc);
error_nodev:
	return;

	wusb_dev_sysfs_rm(wusb_dev);
error_add_sysfs:
	wusb_dev_bos_rm(wusb_dev);
error_bos_add:
	wusb_dev_sec_rm(wusb_dev);
error_sec_add:
	mutex_lock(&wusbhc->mutex);
	__wusbhc_dev_disconnect(wusbhc, wusb_port_by_idx(wusbhc, port_idx));
	mutex_unlock(&wusbhc->mutex);
	goto out;
}

static void wusb_dev_rm_ncb(struct usb_device *usb_dev)
{
	struct wusb_dev *wusb_dev = usb_dev->wusb_dev;

	if (usb_dev->wusb == 0 || usb_dev->devnum == 1)
		return;		

	wusb_dev_sysfs_rm(wusb_dev);
	wusb_dev_bos_rm(wusb_dev);
	wusb_dev_sec_rm(wusb_dev);
	wusb_dev->usb_dev = NULL;
	usb_dev->wusb_dev = NULL;
	wusb_dev_put(wusb_dev);
	usb_put_dev(usb_dev);
}

int wusb_usb_ncb(struct notifier_block *nb, unsigned long val,
		 void *priv)
{
	int result = NOTIFY_OK;

	switch (val) {
	case USB_DEVICE_ADD:
		wusb_dev_add_ncb(priv);
		break;
	case USB_DEVICE_REMOVE:
		wusb_dev_rm_ncb(priv);
		break;
	case USB_BUS_ADD:
		
	case USB_BUS_REMOVE:
		break;
	default:
		WARN_ON(1);
		result = NOTIFY_BAD;
	};
	return result;
}

struct wusb_dev *__wusb_dev_get_by_usb_dev(struct wusbhc *wusbhc,
					   struct usb_device *usb_dev)
{
	struct wusb_dev *wusb_dev;
	u8 port_idx;

	port_idx = wusb_port_no_to_idx(usb_dev->portnum);
	BUG_ON(port_idx > wusbhc->ports_max);
	wusb_dev = wusb_port_by_idx(wusbhc, port_idx)->wusb_dev;
	if (wusb_dev != NULL)		
		wusb_dev_get(wusb_dev);
	return wusb_dev;
}
EXPORT_SYMBOL_GPL(__wusb_dev_get_by_usb_dev);

void wusb_dev_destroy(struct kref *_wusb_dev)
{
	struct wusb_dev *wusb_dev = container_of(_wusb_dev, struct wusb_dev, refcnt);

	list_del_init(&wusb_dev->cack_node);
	wusb_dev_free(wusb_dev);
}
EXPORT_SYMBOL_GPL(wusb_dev_destroy);

int wusbhc_devconnect_create(struct wusbhc *wusbhc)
{
	wusbhc->keep_alive_ie.hdr.bIEIdentifier = WUIE_ID_KEEP_ALIVE;
	wusbhc->keep_alive_ie.hdr.bLength = sizeof(wusbhc->keep_alive_ie.hdr);
	INIT_DELAYED_WORK(&wusbhc->keep_alive_timer, wusbhc_keep_alive_run);

	wusbhc->cack_ie.hdr.bIEIdentifier = WUIE_ID_CONNECTACK;
	wusbhc->cack_ie.hdr.bLength = sizeof(wusbhc->cack_ie.hdr);
	INIT_LIST_HEAD(&wusbhc->cack_list);

	return 0;
}

void wusbhc_devconnect_destroy(struct wusbhc *wusbhc)
{
	
}

int wusbhc_devconnect_start(struct wusbhc *wusbhc)
{
	struct device *dev = wusbhc->dev;
	struct wuie_host_info *hi;
	int result;

	hi = kzalloc(sizeof(*hi), GFP_KERNEL);
	if (hi == NULL)
		return -ENOMEM;

	hi->hdr.bLength       = sizeof(*hi);
	hi->hdr.bIEIdentifier = WUIE_ID_HOST_INFO;
	hi->attributes        = cpu_to_le16((wusbhc->rsv->stream << 3) | WUIE_HI_CAP_ALL);
	hi->CHID              = wusbhc->chid;
	result = wusbhc_mmcie_set(wusbhc, 0, 0, &hi->hdr);
	if (result < 0) {
		dev_err(dev, "Cannot add Host Info MMCIE: %d\n", result);
		goto error_mmcie_set;
	}
	wusbhc->wuie_host_info = hi;

	queue_delayed_work(wusbd, &wusbhc->keep_alive_timer,
			   (wusbhc->trust_timeout*CONFIG_HZ)/1000/2);

	return 0;

error_mmcie_set:
	kfree(hi);
	return result;
}

void wusbhc_devconnect_stop(struct wusbhc *wusbhc)
{
	int i;

	mutex_lock(&wusbhc->mutex);
	for (i = 0; i < wusbhc->ports_max; i++) {
		if (wusbhc->port[i].wusb_dev)
			__wusbhc_dev_disconnect(wusbhc, &wusbhc->port[i]);
	}
	mutex_unlock(&wusbhc->mutex);

	cancel_delayed_work_sync(&wusbhc->keep_alive_timer);
	wusbhc_mmcie_rm(wusbhc, &wusbhc->wuie_host_info->hdr);
	kfree(wusbhc->wuie_host_info);
	wusbhc->wuie_host_info = NULL;
}

int wusb_set_dev_addr(struct wusbhc *wusbhc, struct wusb_dev *wusb_dev, u8 addr)
{
	int result;

	wusb_dev->addr = addr;
	result = wusbhc->dev_info_set(wusbhc, wusb_dev);
	if (result < 0)
		dev_err(wusbhc->dev, "device %d: failed to set device "
			"address\n", wusb_dev->port_idx);
	else
		dev_info(wusbhc->dev, "device %d: %s addr %u\n",
			 wusb_dev->port_idx,
			 (addr & WUSB_DEV_ADDR_UNAUTH) ? "unauth" : "auth",
			 wusb_dev->addr);

	return result;
}
