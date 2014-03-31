/******************************************************************************
 * Client-facing interface for the Xenbus driver.  In other words, the
 * interface between the Xenbus and the device-specific code, be it the
 * frontend or the backend of that driver.
 *
 * Copyright (C) 2005 XenSource Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation; or, when distributed
 * separately from the Linux kernel or incorporated into other
 * software packages, subject to the following license:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/export.h>
#include <asm/xen/hypervisor.h>
#include <asm/xen/page.h>
#include <xen/interface/xen.h>
#include <xen/interface/event_channel.h>
#include <xen/balloon.h>
#include <xen/events.h>
#include <xen/grant_table.h>
#include <xen/xenbus.h>
#include <xen/xen.h>

#include "xenbus_probe.h"

struct xenbus_map_node {
	struct list_head next;
	union {
		struct vm_struct *area; 
		struct page *page;     
	};
	grant_handle_t handle;
};

static DEFINE_SPINLOCK(xenbus_valloc_lock);
static LIST_HEAD(xenbus_valloc_pages);

struct xenbus_ring_ops {
	int (*map)(struct xenbus_device *dev, int gnt, void **vaddr);
	int (*unmap)(struct xenbus_device *dev, void *vaddr);
};

static const struct xenbus_ring_ops *ring_ops __read_mostly;

const char *xenbus_strstate(enum xenbus_state state)
{
	static const char *const name[] = {
		[ XenbusStateUnknown      ] = "Unknown",
		[ XenbusStateInitialising ] = "Initialising",
		[ XenbusStateInitWait     ] = "InitWait",
		[ XenbusStateInitialised  ] = "Initialised",
		[ XenbusStateConnected    ] = "Connected",
		[ XenbusStateClosing      ] = "Closing",
		[ XenbusStateClosed	  ] = "Closed",
		[XenbusStateReconfiguring] = "Reconfiguring",
		[XenbusStateReconfigured] = "Reconfigured",
	};
	return (state < ARRAY_SIZE(name)) ? name[state] : "INVALID";
}
EXPORT_SYMBOL_GPL(xenbus_strstate);

int xenbus_watch_path(struct xenbus_device *dev, const char *path,
		      struct xenbus_watch *watch,
		      void (*callback)(struct xenbus_watch *,
				       const char **, unsigned int))
{
	int err;

	watch->node = path;
	watch->callback = callback;

	err = register_xenbus_watch(watch);

	if (err) {
		watch->node = NULL;
		watch->callback = NULL;
		xenbus_dev_fatal(dev, err, "adding watch on %s", path);
	}

	return err;
}
EXPORT_SYMBOL_GPL(xenbus_watch_path);


int xenbus_watch_pathfmt(struct xenbus_device *dev,
			 struct xenbus_watch *watch,
			 void (*callback)(struct xenbus_watch *,
					const char **, unsigned int),
			 const char *pathfmt, ...)
{
	int err;
	va_list ap;
	char *path;

	va_start(ap, pathfmt);
	path = kvasprintf(GFP_NOIO | __GFP_HIGH, pathfmt, ap);
	va_end(ap);

	if (!path) {
		xenbus_dev_fatal(dev, -ENOMEM, "allocating path for watch");
		return -ENOMEM;
	}
	err = xenbus_watch_path(dev, path, watch, callback);

	if (err)
		kfree(path);
	return err;
}
EXPORT_SYMBOL_GPL(xenbus_watch_pathfmt);

static void xenbus_switch_fatal(struct xenbus_device *, int, int,
				const char *, ...);

static int
__xenbus_switch_state(struct xenbus_device *dev,
		      enum xenbus_state state, int depth)
{

	struct xenbus_transaction xbt;
	int current_state;
	int err, abort;

	if (state == dev->state)
		return 0;

again:
	abort = 1;

	err = xenbus_transaction_start(&xbt);
	if (err) {
		xenbus_switch_fatal(dev, depth, err, "starting transaction");
		return 0;
	}

	err = xenbus_scanf(xbt, dev->nodename, "state", "%d", &current_state);
	if (err != 1)
		goto abort;

	err = xenbus_printf(xbt, dev->nodename, "state", "%d", state);
	if (err) {
		xenbus_switch_fatal(dev, depth, err, "writing new state");
		goto abort;
	}

	abort = 0;
abort:
	err = xenbus_transaction_end(xbt, abort);
	if (err) {
		if (err == -EAGAIN && !abort)
			goto again;
		xenbus_switch_fatal(dev, depth, err, "ending transaction");
	} else
		dev->state = state;

	return 0;
}

int xenbus_switch_state(struct xenbus_device *dev, enum xenbus_state state)
{
	return __xenbus_switch_state(dev, state, 0);
}

EXPORT_SYMBOL_GPL(xenbus_switch_state);

int xenbus_frontend_closed(struct xenbus_device *dev)
{
	xenbus_switch_state(dev, XenbusStateClosed);
	complete(&dev->down);
	return 0;
}
EXPORT_SYMBOL_GPL(xenbus_frontend_closed);

static char *error_path(struct xenbus_device *dev)
{
	return kasprintf(GFP_KERNEL, "error/%s", dev->nodename);
}


static void xenbus_va_dev_error(struct xenbus_device *dev, int err,
				const char *fmt, va_list ap)
{
	int ret;
	unsigned int len;
	char *printf_buffer = NULL;
	char *path_buffer = NULL;

#define PRINTF_BUFFER_SIZE 4096
	printf_buffer = kmalloc(PRINTF_BUFFER_SIZE, GFP_KERNEL);
	if (printf_buffer == NULL)
		goto fail;

	len = sprintf(printf_buffer, "%i ", -err);
	ret = vsnprintf(printf_buffer+len, PRINTF_BUFFER_SIZE-len, fmt, ap);

	BUG_ON(len + ret > PRINTF_BUFFER_SIZE-1);

	dev_err(&dev->dev, "%s\n", printf_buffer);

	path_buffer = error_path(dev);

	if (path_buffer == NULL) {
		dev_err(&dev->dev, "failed to write error node for %s (%s)\n",
		       dev->nodename, printf_buffer);
		goto fail;
	}

	if (xenbus_write(XBT_NIL, path_buffer, "error", printf_buffer) != 0) {
		dev_err(&dev->dev, "failed to write error node for %s (%s)\n",
		       dev->nodename, printf_buffer);
		goto fail;
	}

fail:
	kfree(printf_buffer);
	kfree(path_buffer);
}


void xenbus_dev_error(struct xenbus_device *dev, int err, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	xenbus_va_dev_error(dev, err, fmt, ap);
	va_end(ap);
}
EXPORT_SYMBOL_GPL(xenbus_dev_error);


void xenbus_dev_fatal(struct xenbus_device *dev, int err, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	xenbus_va_dev_error(dev, err, fmt, ap);
	va_end(ap);

	xenbus_switch_state(dev, XenbusStateClosing);
}
EXPORT_SYMBOL_GPL(xenbus_dev_fatal);

static void xenbus_switch_fatal(struct xenbus_device *dev, int depth, int err,
				const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	xenbus_va_dev_error(dev, err, fmt, ap);
	va_end(ap);

	if (!depth)
		__xenbus_switch_state(dev, XenbusStateClosing, 1);
}

int xenbus_grant_ring(struct xenbus_device *dev, unsigned long ring_mfn)
{
	int err = gnttab_grant_foreign_access(dev->otherend_id, ring_mfn, 0);
	if (err < 0)
		xenbus_dev_fatal(dev, err, "granting access to ring page");
	return err;
}
EXPORT_SYMBOL_GPL(xenbus_grant_ring);


int xenbus_alloc_evtchn(struct xenbus_device *dev, int *port)
{
	struct evtchn_alloc_unbound alloc_unbound;
	int err;

	alloc_unbound.dom = DOMID_SELF;
	alloc_unbound.remote_dom = dev->otherend_id;

	err = HYPERVISOR_event_channel_op(EVTCHNOP_alloc_unbound,
					  &alloc_unbound);
	if (err)
		xenbus_dev_fatal(dev, err, "allocating event channel");
	else
		*port = alloc_unbound.port;

	return err;
}
EXPORT_SYMBOL_GPL(xenbus_alloc_evtchn);


int xenbus_bind_evtchn(struct xenbus_device *dev, int remote_port, int *port)
{
	struct evtchn_bind_interdomain bind_interdomain;
	int err;

	bind_interdomain.remote_dom = dev->otherend_id;
	bind_interdomain.remote_port = remote_port;

	err = HYPERVISOR_event_channel_op(EVTCHNOP_bind_interdomain,
					  &bind_interdomain);
	if (err)
		xenbus_dev_fatal(dev, err,
				 "binding to event channel %d from domain %d",
				 remote_port, dev->otherend_id);
	else
		*port = bind_interdomain.local_port;

	return err;
}
EXPORT_SYMBOL_GPL(xenbus_bind_evtchn);


int xenbus_free_evtchn(struct xenbus_device *dev, int port)
{
	struct evtchn_close close;
	int err;

	close.port = port;

	err = HYPERVISOR_event_channel_op(EVTCHNOP_close, &close);
	if (err)
		xenbus_dev_error(dev, err, "freeing event channel %d", port);

	return err;
}
EXPORT_SYMBOL_GPL(xenbus_free_evtchn);


int xenbus_map_ring_valloc(struct xenbus_device *dev, int gnt_ref, void **vaddr)
{
	return ring_ops->map(dev, gnt_ref, vaddr);
}
EXPORT_SYMBOL_GPL(xenbus_map_ring_valloc);

static int xenbus_map_ring_valloc_pv(struct xenbus_device *dev,
				     int gnt_ref, void **vaddr)
{
	struct gnttab_map_grant_ref op = {
		.flags = GNTMAP_host_map | GNTMAP_contains_pte,
		.ref   = gnt_ref,
		.dom   = dev->otherend_id,
	};
	struct xenbus_map_node *node;
	struct vm_struct *area;
	pte_t *pte;

	*vaddr = NULL;

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	area = alloc_vm_area(PAGE_SIZE, &pte);
	if (!area) {
		kfree(node);
		return -ENOMEM;
	}

	op.host_addr = arbitrary_virt_to_machine(pte).maddr;

	if (HYPERVISOR_grant_table_op(GNTTABOP_map_grant_ref, &op, 1))
		BUG();

	if (op.status != GNTST_okay) {
		free_vm_area(area);
		kfree(node);
		xenbus_dev_fatal(dev, op.status,
				 "mapping in shared page %d from domain %d",
				 gnt_ref, dev->otherend_id);
		return op.status;
	}

	node->handle = op.handle;
	node->area = area;

	spin_lock(&xenbus_valloc_lock);
	list_add(&node->next, &xenbus_valloc_pages);
	spin_unlock(&xenbus_valloc_lock);

	*vaddr = area->addr;
	return 0;
}

static int xenbus_map_ring_valloc_hvm(struct xenbus_device *dev,
				      int gnt_ref, void **vaddr)
{
	struct xenbus_map_node *node;
	int err;
	void *addr;

	*vaddr = NULL;

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	err = alloc_xenballooned_pages(1, &node->page, false );
	if (err)
		goto out_err;

	addr = pfn_to_kaddr(page_to_pfn(node->page));

	err = xenbus_map_ring(dev, gnt_ref, &node->handle, addr);
	if (err)
		goto out_err;

	spin_lock(&xenbus_valloc_lock);
	list_add(&node->next, &xenbus_valloc_pages);
	spin_unlock(&xenbus_valloc_lock);

	*vaddr = addr;
	return 0;

 out_err:
	free_xenballooned_pages(1, &node->page);
	kfree(node);
	return err;
}


int xenbus_map_ring(struct xenbus_device *dev, int gnt_ref,
		    grant_handle_t *handle, void *vaddr)
{
	struct gnttab_map_grant_ref op;

	gnttab_set_map_op(&op, (unsigned long)vaddr, GNTMAP_host_map, gnt_ref,
			  dev->otherend_id);

	if (HYPERVISOR_grant_table_op(GNTTABOP_map_grant_ref, &op, 1))
		BUG();

	if (op.status != GNTST_okay) {
		xenbus_dev_fatal(dev, op.status,
				 "mapping in shared page %d from domain %d",
				 gnt_ref, dev->otherend_id);
	} else
		*handle = op.handle;

	return op.status;
}
EXPORT_SYMBOL_GPL(xenbus_map_ring);


int xenbus_unmap_ring_vfree(struct xenbus_device *dev, void *vaddr)
{
	return ring_ops->unmap(dev, vaddr);
}
EXPORT_SYMBOL_GPL(xenbus_unmap_ring_vfree);

static int xenbus_unmap_ring_vfree_pv(struct xenbus_device *dev, void *vaddr)
{
	struct xenbus_map_node *node;
	struct gnttab_unmap_grant_ref op = {
		.host_addr = (unsigned long)vaddr,
	};
	unsigned int level;

	spin_lock(&xenbus_valloc_lock);
	list_for_each_entry(node, &xenbus_valloc_pages, next) {
		if (node->area->addr == vaddr) {
			list_del(&node->next);
			goto found;
		}
	}
	node = NULL;
 found:
	spin_unlock(&xenbus_valloc_lock);

	if (!node) {
		xenbus_dev_error(dev, -ENOENT,
				 "can't find mapped virtual address %p", vaddr);
		return GNTST_bad_virt_addr;
	}

	op.handle = node->handle;
	op.host_addr = arbitrary_virt_to_machine(
		lookup_address((unsigned long)vaddr, &level)).maddr;

	if (HYPERVISOR_grant_table_op(GNTTABOP_unmap_grant_ref, &op, 1))
		BUG();

	if (op.status == GNTST_okay)
		free_vm_area(node->area);
	else
		xenbus_dev_error(dev, op.status,
				 "unmapping page at handle %d error %d",
				 node->handle, op.status);

	kfree(node);
	return op.status;
}

static int xenbus_unmap_ring_vfree_hvm(struct xenbus_device *dev, void *vaddr)
{
	int rv;
	struct xenbus_map_node *node;
	void *addr;

	spin_lock(&xenbus_valloc_lock);
	list_for_each_entry(node, &xenbus_valloc_pages, next) {
		addr = pfn_to_kaddr(page_to_pfn(node->page));
		if (addr == vaddr) {
			list_del(&node->next);
			goto found;
		}
	}
	node = addr = NULL;
 found:
	spin_unlock(&xenbus_valloc_lock);

	if (!node) {
		xenbus_dev_error(dev, -ENOENT,
				 "can't find mapped virtual address %p", vaddr);
		return GNTST_bad_virt_addr;
	}

	rv = xenbus_unmap_ring(dev, node->handle, addr);

	if (!rv)
		free_xenballooned_pages(1, &node->page);
	else
		WARN(1, "Leaking %p\n", vaddr);

	kfree(node);
	return rv;
}

int xenbus_unmap_ring(struct xenbus_device *dev,
		      grant_handle_t handle, void *vaddr)
{
	struct gnttab_unmap_grant_ref op;

	gnttab_set_unmap_op(&op, (unsigned long)vaddr, GNTMAP_host_map, handle);

	if (HYPERVISOR_grant_table_op(GNTTABOP_unmap_grant_ref, &op, 1))
		BUG();

	if (op.status != GNTST_okay)
		xenbus_dev_error(dev, op.status,
				 "unmapping page at handle %d error %d",
				 handle, op.status);

	return op.status;
}
EXPORT_SYMBOL_GPL(xenbus_unmap_ring);


enum xenbus_state xenbus_read_driver_state(const char *path)
{
	enum xenbus_state result;
	int err = xenbus_gather(XBT_NIL, path, "state", "%d", &result, NULL);
	if (err)
		result = XenbusStateUnknown;

	return result;
}
EXPORT_SYMBOL_GPL(xenbus_read_driver_state);

static const struct xenbus_ring_ops ring_ops_pv = {
	.map = xenbus_map_ring_valloc_pv,
	.unmap = xenbus_unmap_ring_vfree_pv,
};

static const struct xenbus_ring_ops ring_ops_hvm = {
	.map = xenbus_map_ring_valloc_hvm,
	.unmap = xenbus_unmap_ring_vfree_hvm,
};

void __init xenbus_ring_ops_init(void)
{
	if (xen_pv_domain())
		ring_ops = &ring_ops_pv;
	else
		ring_ops = &ring_ops_hvm;
}
