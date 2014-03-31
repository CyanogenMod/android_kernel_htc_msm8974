/*
 * Copyright (C) 2003-2008 Takahiro Hirofuchi
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef __USBIP_VHCI_H
#define __USBIP_VHCI_H

#include <linux/device.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/wait.h>

struct vhci_device {
	struct usb_device *udev;

	__u32 devid;

	
	enum usb_device_speed speed;

	
	__u32 rhport;

	struct usbip_device ud;

	
	spinlock_t priv_lock;

	
	struct list_head priv_tx;
	struct list_head priv_rx;

	
	struct list_head unlink_tx;
	struct list_head unlink_rx;

	
	wait_queue_head_t waitq_tx;
};

struct vhci_priv {
	unsigned long seqnum;
	struct list_head list;

	struct vhci_device *vdev;
	struct urb *urb;
};

struct vhci_unlink {
	
	unsigned long seqnum;

	struct list_head list;

	
	unsigned long unlink_seqnum;
};

#define VHCI_NPORTS 8

struct vhci_hcd {
	spinlock_t lock;

	u32 port_status[VHCI_NPORTS];

	unsigned resuming:1;
	unsigned long re_timeout;

	atomic_t seqnum;

	struct vhci_device vdev[VHCI_NPORTS];
};

extern struct vhci_hcd *the_controller;
extern const struct attribute_group dev_attr_group;
#define hardware (&the_controller->pdev.dev)

void rh_port_connect(int rhport, enum usb_device_speed speed);
void rh_port_disconnect(int rhport);

struct urb *pickup_urb_and_free_priv(struct vhci_device *vdev, __u32 seqnum);
int vhci_rx_loop(void *data);

int vhci_tx_loop(void *data);

static inline struct vhci_device *port_to_vdev(__u32 port)
{
	return &the_controller->vdev[port];
}

static inline struct vhci_hcd *hcd_to_vhci(struct usb_hcd *hcd)
{
	return (struct vhci_hcd *) (hcd->hcd_priv);
}

static inline struct usb_hcd *vhci_to_hcd(struct vhci_hcd *vhci)
{
	return container_of((void *) vhci, struct usb_hcd, hcd_priv);
}

static inline struct device *vhci_dev(struct vhci_hcd *vhci)
{
	return vhci_to_hcd(vhci)->self.controller;
}

#endif 
