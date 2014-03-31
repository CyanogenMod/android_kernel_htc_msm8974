/*
 * Handles the Intel 27x USB Device Controller (UDC)
 *
 * Inspired by original driver by Frank Becker, David Brownell, and others.
 * Copyright (C) 2008 Robert Jarzmik
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/prefetch.h>

#include <asm/byteorder.h>
#include <mach/hardware.h>

#include <linux/usb.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <mach/udc.h>

#include "pxa27x_udc.h"


#define	DRIVER_VERSION	"2008-04-18"
#define	DRIVER_DESC	"PXA 27x USB Device Controller driver"

static const char driver_name[] = "pxa27x_udc";
static struct pxa_udc *the_controller;

static void handle_ep(struct pxa_ep *ep);

#ifdef CONFIG_USB_GADGET_DEBUG_FS

#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>

static int state_dbg_show(struct seq_file *s, void *p)
{
	struct pxa_udc *udc = s->private;
	int pos = 0, ret;
	u32 tmp;

	ret = -ENODEV;
	if (!udc->driver)
		goto out;

	
	pos += seq_printf(s, DRIVER_DESC "\n"
			 "%s version: %s\nGadget driver: %s\n",
			 driver_name, DRIVER_VERSION,
			 udc->driver ? udc->driver->driver.name : "(none)");

	tmp = udc_readl(udc, UDCCR);
	pos += seq_printf(s,
			 "udccr=0x%0x(%s%s%s%s%s%s%s%s%s%s), "
			 "con=%d,inter=%d,altinter=%d\n", tmp,
			 (tmp & UDCCR_OEN) ? " oen":"",
			 (tmp & UDCCR_AALTHNP) ? " aalthnp":"",
			 (tmp & UDCCR_AHNP) ? " rem" : "",
			 (tmp & UDCCR_BHNP) ? " rstir" : "",
			 (tmp & UDCCR_DWRE) ? " dwre" : "",
			 (tmp & UDCCR_SMAC) ? " smac" : "",
			 (tmp & UDCCR_EMCE) ? " emce" : "",
			 (tmp & UDCCR_UDR) ? " udr" : "",
			 (tmp & UDCCR_UDA) ? " uda" : "",
			 (tmp & UDCCR_UDE) ? " ude" : "",
			 (tmp & UDCCR_ACN) >> UDCCR_ACN_S,
			 (tmp & UDCCR_AIN) >> UDCCR_AIN_S,
			 (tmp & UDCCR_AAISN) >> UDCCR_AAISN_S);
	
	pos += seq_printf(s, "udcicr0=0x%08x udcicr1=0x%08x\n",
			udc_readl(udc, UDCICR0), udc_readl(udc, UDCICR1));
	pos += seq_printf(s, "udcisr0=0x%08x udcisr1=0x%08x\n",
			udc_readl(udc, UDCISR0), udc_readl(udc, UDCISR1));
	pos += seq_printf(s, "udcfnr=%d\n", udc_readl(udc, UDCFNR));
	pos += seq_printf(s, "irqs: reset=%lu, suspend=%lu, resume=%lu, "
			"reconfig=%lu\n",
			udc->stats.irqs_reset, udc->stats.irqs_suspend,
			udc->stats.irqs_resume, udc->stats.irqs_reconfig);

	ret = 0;
out:
	return ret;
}

static int queues_dbg_show(struct seq_file *s, void *p)
{
	struct pxa_udc *udc = s->private;
	struct pxa_ep *ep;
	struct pxa27x_request *req;
	int pos = 0, i, maxpkt, ret;

	ret = -ENODEV;
	if (!udc->driver)
		goto out;

	
	for (i = 0; i < NR_PXA_ENDPOINTS; i++) {
		ep = &udc->pxa_ep[i];
		maxpkt = ep->fifo_size;
		pos += seq_printf(s,  "%-12s max_pkt=%d %s\n",
				EPNAME(ep), maxpkt, "pio");

		if (list_empty(&ep->queue)) {
			pos += seq_printf(s, "\t(nothing queued)\n");
			continue;
		}

		list_for_each_entry(req, &ep->queue, queue) {
			pos += seq_printf(s,  "\treq %p len %d/%d buf %p\n",
					&req->req, req->req.actual,
					req->req.length, req->req.buf);
		}
	}

	ret = 0;
out:
	return ret;
}

static int eps_dbg_show(struct seq_file *s, void *p)
{
	struct pxa_udc *udc = s->private;
	struct pxa_ep *ep;
	int pos = 0, i, ret;
	u32 tmp;

	ret = -ENODEV;
	if (!udc->driver)
		goto out;

	ep = &udc->pxa_ep[0];
	tmp = udc_ep_readl(ep, UDCCSR);
	pos += seq_printf(s, "udccsr0=0x%03x(%s%s%s%s%s%s%s)\n", tmp,
			 (tmp & UDCCSR0_SA) ? " sa" : "",
			 (tmp & UDCCSR0_RNE) ? " rne" : "",
			 (tmp & UDCCSR0_FST) ? " fst" : "",
			 (tmp & UDCCSR0_SST) ? " sst" : "",
			 (tmp & UDCCSR0_DME) ? " dme" : "",
			 (tmp & UDCCSR0_IPR) ? " ipr" : "",
			 (tmp & UDCCSR0_OPC) ? " opc" : "");
	for (i = 0; i < NR_PXA_ENDPOINTS; i++) {
		ep = &udc->pxa_ep[i];
		tmp = i? udc_ep_readl(ep, UDCCR) : udc_readl(udc, UDCCR);
		pos += seq_printf(s, "%-12s: "
				"IN %lu(%lu reqs), OUT %lu(%lu reqs), "
				"irqs=%lu, udccr=0x%08x, udccsr=0x%03x, "
				"udcbcr=%d\n",
				EPNAME(ep),
				ep->stats.in_bytes, ep->stats.in_ops,
				ep->stats.out_bytes, ep->stats.out_ops,
				ep->stats.irqs,
				tmp, udc_ep_readl(ep, UDCCSR),
				udc_ep_readl(ep, UDCBCR));
	}

	ret = 0;
out:
	return ret;
}

static int eps_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, eps_dbg_show, inode->i_private);
}

static int queues_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, queues_dbg_show, inode->i_private);
}

static int state_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, state_dbg_show, inode->i_private);
}

static const struct file_operations state_dbg_fops = {
	.owner		= THIS_MODULE,
	.open		= state_dbg_open,
	.llseek		= seq_lseek,
	.read		= seq_read,
	.release	= single_release,
};

static const struct file_operations queues_dbg_fops = {
	.owner		= THIS_MODULE,
	.open		= queues_dbg_open,
	.llseek		= seq_lseek,
	.read		= seq_read,
	.release	= single_release,
};

static const struct file_operations eps_dbg_fops = {
	.owner		= THIS_MODULE,
	.open		= eps_dbg_open,
	.llseek		= seq_lseek,
	.read		= seq_read,
	.release	= single_release,
};

static void pxa_init_debugfs(struct pxa_udc *udc)
{
	struct dentry *root, *state, *queues, *eps;

	root = debugfs_create_dir(udc->gadget.name, NULL);
	if (IS_ERR(root) || !root)
		goto err_root;

	state = debugfs_create_file("udcstate", 0400, root, udc,
			&state_dbg_fops);
	if (!state)
		goto err_state;
	queues = debugfs_create_file("queues", 0400, root, udc,
			&queues_dbg_fops);
	if (!queues)
		goto err_queues;
	eps = debugfs_create_file("epstate", 0400, root, udc,
			&eps_dbg_fops);
	if (!eps)
		goto err_eps;

	udc->debugfs_root = root;
	udc->debugfs_state = state;
	udc->debugfs_queues = queues;
	udc->debugfs_eps = eps;
	return;
err_eps:
	debugfs_remove(eps);
err_queues:
	debugfs_remove(queues);
err_state:
	debugfs_remove(root);
err_root:
	dev_err(udc->dev, "debugfs is not available\n");
}

static void pxa_cleanup_debugfs(struct pxa_udc *udc)
{
	debugfs_remove(udc->debugfs_eps);
	debugfs_remove(udc->debugfs_queues);
	debugfs_remove(udc->debugfs_state);
	debugfs_remove(udc->debugfs_root);
	udc->debugfs_eps = NULL;
	udc->debugfs_queues = NULL;
	udc->debugfs_state = NULL;
	udc->debugfs_root = NULL;
}

#else
static inline void pxa_init_debugfs(struct pxa_udc *udc)
{
}

static inline void pxa_cleanup_debugfs(struct pxa_udc *udc)
{
}
#endif

static int is_match_usb_pxa(struct udc_usb_ep *udc_usb_ep, struct pxa_ep *ep,
		int config, int interface, int altsetting)
{
	if (usb_endpoint_num(&udc_usb_ep->desc) != ep->addr)
		return 0;
	if (usb_endpoint_dir_in(&udc_usb_ep->desc) != ep->dir_in)
		return 0;
	if (usb_endpoint_type(&udc_usb_ep->desc) != ep->type)
		return 0;
	if ((ep->config != config) || (ep->interface != interface)
			|| (ep->alternate != altsetting))
		return 0;
	return 1;
}

static struct pxa_ep *find_pxa_ep(struct pxa_udc *udc,
		struct udc_usb_ep *udc_usb_ep)
{
	int i;
	struct pxa_ep *ep;
	int cfg = udc->config;
	int iface = udc->last_interface;
	int alt = udc->last_alternate;

	if (udc_usb_ep == &udc->udc_usb_ep[0])
		return &udc->pxa_ep[0];

	for (i = 1; i < NR_PXA_ENDPOINTS; i++) {
		ep = &udc->pxa_ep[i];
		if (is_match_usb_pxa(udc_usb_ep, ep, cfg, iface, alt))
			return ep;
	}
	return NULL;
}

static void update_pxa_ep_matches(struct pxa_udc *udc)
{
	int i;
	struct udc_usb_ep *udc_usb_ep;

	for (i = 1; i < NR_USB_ENDPOINTS; i++) {
		udc_usb_ep = &udc->udc_usb_ep[i];
		if (udc_usb_ep->pxa_ep)
			udc_usb_ep->pxa_ep = find_pxa_ep(udc, udc_usb_ep);
	}
}

static void pio_irq_enable(struct pxa_ep *ep)
{
	struct pxa_udc *udc = ep->dev;
	int index = EPIDX(ep);
	u32 udcicr0 = udc_readl(udc, UDCICR0);
	u32 udcicr1 = udc_readl(udc, UDCICR1);

	if (index < 16)
		udc_writel(udc, UDCICR0, udcicr0 | (3 << (index * 2)));
	else
		udc_writel(udc, UDCICR1, udcicr1 | (3 << ((index - 16) * 2)));
}

static void pio_irq_disable(struct pxa_ep *ep)
{
	struct pxa_udc *udc = ep->dev;
	int index = EPIDX(ep);
	u32 udcicr0 = udc_readl(udc, UDCICR0);
	u32 udcicr1 = udc_readl(udc, UDCICR1);

	if (index < 16)
		udc_writel(udc, UDCICR0, udcicr0 & ~(3 << (index * 2)));
	else
		udc_writel(udc, UDCICR1, udcicr1 & ~(3 << ((index - 16) * 2)));
}

static inline void udc_set_mask_UDCCR(struct pxa_udc *udc, int mask)
{
	u32 udccr = udc_readl(udc, UDCCR);
	udc_writel(udc, UDCCR,
			(udccr & UDCCR_MASK_BITS) | (mask & UDCCR_MASK_BITS));
}

static inline void udc_clear_mask_UDCCR(struct pxa_udc *udc, int mask)
{
	u32 udccr = udc_readl(udc, UDCCR);
	udc_writel(udc, UDCCR,
			(udccr & UDCCR_MASK_BITS) & ~(mask & UDCCR_MASK_BITS));
}

static inline void ep_write_UDCCSR(struct pxa_ep *ep, int mask)
{
	if (is_ep0(ep))
		mask |= UDCCSR0_ACM;
	udc_ep_writel(ep, UDCCSR, mask);
}

static int ep_count_bytes_remain(struct pxa_ep *ep)
{
	if (ep->dir_in)
		return -EOPNOTSUPP;
	return udc_ep_readl(ep, UDCBCR) & 0x3ff;
}

static int ep_is_empty(struct pxa_ep *ep)
{
	int ret;

	if (!is_ep0(ep) && ep->dir_in)
		return -EOPNOTSUPP;
	if (is_ep0(ep))
		ret = !(udc_ep_readl(ep, UDCCSR) & UDCCSR0_RNE);
	else
		ret = !(udc_ep_readl(ep, UDCCSR) & UDCCSR_BNE);
	return ret;
}

static int ep_is_full(struct pxa_ep *ep)
{
	if (is_ep0(ep))
		return (udc_ep_readl(ep, UDCCSR) & UDCCSR0_IPR);
	if (!ep->dir_in)
		return -EOPNOTSUPP;
	return (!(udc_ep_readl(ep, UDCCSR) & UDCCSR_BNF));
}

static int epout_has_pkt(struct pxa_ep *ep)
{
	if (!is_ep0(ep) && ep->dir_in)
		return -EOPNOTSUPP;
	if (is_ep0(ep))
		return (udc_ep_readl(ep, UDCCSR) & UDCCSR0_OPC);
	return (udc_ep_readl(ep, UDCCSR) & UDCCSR_PC);
}

static void set_ep0state(struct pxa_udc *udc, int state)
{
	struct pxa_ep *ep = &udc->pxa_ep[0];
	char *old_stname = EP0_STNAME(udc);

	udc->ep0state = state;
	ep_dbg(ep, "state=%s->%s, udccsr0=0x%03x, udcbcr=%d\n", old_stname,
		EP0_STNAME(udc), udc_ep_readl(ep, UDCCSR),
		udc_ep_readl(ep, UDCBCR));
}

static void ep0_idle(struct pxa_udc *dev)
{
	set_ep0state(dev, WAIT_FOR_SETUP);
}

static void inc_ep_stats_reqs(struct pxa_ep *ep, int is_in)
{
	if (is_in)
		ep->stats.in_ops++;
	else
		ep->stats.out_ops++;
}

static void inc_ep_stats_bytes(struct pxa_ep *ep, int count, int is_in)
{
	if (is_in)
		ep->stats.in_bytes += count;
	else
		ep->stats.out_bytes += count;
}

static __init void pxa_ep_setup(struct pxa_ep *ep)
{
	u32 new_udccr;

	new_udccr = ((ep->config << UDCCONR_CN_S) & UDCCONR_CN)
		| ((ep->interface << UDCCONR_IN_S) & UDCCONR_IN)
		| ((ep->alternate << UDCCONR_AISN_S) & UDCCONR_AISN)
		| ((EPADDR(ep) << UDCCONR_EN_S) & UDCCONR_EN)
		| ((EPXFERTYPE(ep) << UDCCONR_ET_S) & UDCCONR_ET)
		| ((ep->dir_in) ? UDCCONR_ED : 0)
		| ((ep->fifo_size << UDCCONR_MPS_S) & UDCCONR_MPS)
		| UDCCONR_EE;

	udc_ep_writel(ep, UDCCR, new_udccr);
}

static __init void pxa_eps_setup(struct pxa_udc *dev)
{
	unsigned int i;

	dev_dbg(dev->dev, "%s: dev=%p\n", __func__, dev);

	for (i = 1; i < NR_PXA_ENDPOINTS; i++)
		pxa_ep_setup(&dev->pxa_ep[i]);
}

static struct usb_request *
pxa_ep_alloc_request(struct usb_ep *_ep, gfp_t gfp_flags)
{
	struct pxa27x_request *req;

	req = kzalloc(sizeof *req, gfp_flags);
	if (!req)
		return NULL;

	INIT_LIST_HEAD(&req->queue);
	req->in_use = 0;
	req->udc_usb_ep = container_of(_ep, struct udc_usb_ep, usb_ep);

	return &req->req;
}

static void pxa_ep_free_request(struct usb_ep *_ep, struct usb_request *_req)
{
	struct pxa27x_request *req;

	req = container_of(_req, struct pxa27x_request, req);
	WARN_ON(!list_empty(&req->queue));
	kfree(req);
}

static void ep_add_request(struct pxa_ep *ep, struct pxa27x_request *req)
{
	if (unlikely(!req))
		return;
	ep_vdbg(ep, "req:%p, lg=%d, udccsr=0x%03x\n", req,
		req->req.length, udc_ep_readl(ep, UDCCSR));

	req->in_use = 1;
	list_add_tail(&req->queue, &ep->queue);
	pio_irq_enable(ep);
}

static void ep_del_request(struct pxa_ep *ep, struct pxa27x_request *req)
{
	if (unlikely(!req))
		return;
	ep_vdbg(ep, "req:%p, lg=%d, udccsr=0x%03x\n", req,
		req->req.length, udc_ep_readl(ep, UDCCSR));

	list_del_init(&req->queue);
	req->in_use = 0;
	if (!is_ep0(ep) && list_empty(&ep->queue))
		pio_irq_disable(ep);
}

static void req_done(struct pxa_ep *ep, struct pxa27x_request *req, int status,
	unsigned long *pflags)
{
	unsigned long	flags;

	ep_del_request(ep, req);
	if (likely(req->req.status == -EINPROGRESS))
		req->req.status = status;
	else
		status = req->req.status;

	if (status && status != -ESHUTDOWN)
		ep_dbg(ep, "complete req %p stat %d len %u/%u\n",
			&req->req, status,
			req->req.actual, req->req.length);

	if (pflags)
		spin_unlock_irqrestore(&ep->lock, *pflags);
	local_irq_save(flags);
	req->req.complete(&req->udc_usb_ep->usb_ep, &req->req);
	local_irq_restore(flags);
	if (pflags)
		spin_lock_irqsave(&ep->lock, *pflags);
}

static void ep_end_out_req(struct pxa_ep *ep, struct pxa27x_request *req,
	unsigned long *pflags)
{
	inc_ep_stats_reqs(ep, !USB_DIR_IN);
	req_done(ep, req, 0, pflags);
}

static void ep0_end_out_req(struct pxa_ep *ep, struct pxa27x_request *req,
	unsigned long *pflags)
{
	set_ep0state(ep->dev, OUT_STATUS_STAGE);
	ep_end_out_req(ep, req, pflags);
	ep0_idle(ep->dev);
}

static void ep_end_in_req(struct pxa_ep *ep, struct pxa27x_request *req,
	unsigned long *pflags)
{
	inc_ep_stats_reqs(ep, USB_DIR_IN);
	req_done(ep, req, 0, pflags);
}

static void ep0_end_in_req(struct pxa_ep *ep, struct pxa27x_request *req,
	unsigned long *pflags)
{
	set_ep0state(ep->dev, IN_STATUS_STAGE);
	ep_end_in_req(ep, req, pflags);
}

static void nuke(struct pxa_ep *ep, int status)
{
	struct pxa27x_request	*req;
	unsigned long		flags;

	spin_lock_irqsave(&ep->lock, flags);
	while (!list_empty(&ep->queue)) {
		req = list_entry(ep->queue.next, struct pxa27x_request, queue);
		req_done(ep, req, status, &flags);
	}
	spin_unlock_irqrestore(&ep->lock, flags);
}

static int read_packet(struct pxa_ep *ep, struct pxa27x_request *req)
{
	u32 *buf;
	int bytes_ep, bufferspace, count, i;

	bytes_ep = ep_count_bytes_remain(ep);
	bufferspace = req->req.length - req->req.actual;

	buf = (u32 *)(req->req.buf + req->req.actual);
	prefetchw(buf);

	if (likely(!ep_is_empty(ep)))
		count = min(bytes_ep, bufferspace);
	else 
		count = 0;

	for (i = count; i > 0; i -= 4)
		*buf++ = udc_ep_readl(ep, UDCDR);
	req->req.actual += count;

	ep_write_UDCCSR(ep, UDCCSR_PC);

	return count;
}

static int write_packet(struct pxa_ep *ep, struct pxa27x_request *req,
			unsigned int max)
{
	int length, count, remain, i;
	u32 *buf;
	u8 *buf_8;

	buf = (u32 *)(req->req.buf + req->req.actual);
	prefetch(buf);

	length = min(req->req.length - req->req.actual, max);
	req->req.actual += length;

	remain = length & 0x3;
	count = length & ~(0x3);
	for (i = count; i > 0 ; i -= 4)
		udc_ep_writel(ep, UDCDR, *buf++);

	buf_8 = (u8 *)buf;
	for (i = remain; i > 0; i--)
		udc_ep_writeb(ep, UDCDR, *buf_8++);

	ep_vdbg(ep, "length=%d+%d, udccsr=0x%03x\n", count, remain,
		udc_ep_readl(ep, UDCCSR));

	return length;
}

static int read_fifo(struct pxa_ep *ep, struct pxa27x_request *req)
{
	int count, is_short, completed = 0;

	while (epout_has_pkt(ep)) {
		count = read_packet(ep, req);
		inc_ep_stats_bytes(ep, count, !USB_DIR_IN);

		is_short = (count < ep->fifo_size);
		ep_dbg(ep, "read udccsr:%03x, count:%d bytes%s req %p %d/%d\n",
			udc_ep_readl(ep, UDCCSR), count, is_short ? "/S" : "",
			&req->req, req->req.actual, req->req.length);

		
		if (is_short || req->req.actual == req->req.length) {
			completed = 1;
			break;
		}
		
	}
	return completed;
}

static int write_fifo(struct pxa_ep *ep, struct pxa27x_request *req)
{
	unsigned max;
	int count, is_short, is_last = 0, completed = 0, totcount = 0;
	u32 udccsr;

	max = ep->fifo_size;
	do {
		is_short = 0;

		udccsr = udc_ep_readl(ep, UDCCSR);
		if (udccsr & UDCCSR_PC) {
			ep_vdbg(ep, "Clearing Transmit Complete, udccsr=%x\n",
				udccsr);
			ep_write_UDCCSR(ep, UDCCSR_PC);
		}
		if (udccsr & UDCCSR_TRN) {
			ep_vdbg(ep, "Clearing Underrun on, udccsr=%x\n",
				udccsr);
			ep_write_UDCCSR(ep, UDCCSR_TRN);
		}

		count = write_packet(ep, req, max);
		inc_ep_stats_bytes(ep, count, USB_DIR_IN);
		totcount += count;

		
		if (unlikely(count < max)) {
			is_last = 1;
			is_short = 1;
		} else {
			if (likely(req->req.length > req->req.actual)
					|| req->req.zero)
				is_last = 0;
			else
				is_last = 1;
			
			is_short = unlikely(max < ep->fifo_size);
		}

		if (is_short)
			ep_write_UDCCSR(ep, UDCCSR_SP);

		
		if (is_last) {
			completed = 1;
			break;
		}
	} while (!ep_is_full(ep));

	ep_dbg(ep, "wrote count:%d bytes%s%s, left:%d req=%p\n",
			totcount, is_last ? "/L" : "", is_short ? "/S" : "",
			req->req.length - req->req.actual, &req->req);

	return completed;
}

static int read_ep0_fifo(struct pxa_ep *ep, struct pxa27x_request *req)
{
	int count, is_short, completed = 0;

	while (epout_has_pkt(ep)) {
		count = read_packet(ep, req);
		ep_write_UDCCSR(ep, UDCCSR0_OPC);
		inc_ep_stats_bytes(ep, count, !USB_DIR_IN);

		is_short = (count < ep->fifo_size);
		ep_dbg(ep, "read udccsr:%03x, count:%d bytes%s req %p %d/%d\n",
			udc_ep_readl(ep, UDCCSR), count, is_short ? "/S" : "",
			&req->req, req->req.actual, req->req.length);

		if (is_short || req->req.actual >= req->req.length) {
			completed = 1;
			break;
		}
	}

	return completed;
}

/**
 * write_ep0_fifo - Send a request to control endpoint (ep0 in)
 * @ep: control endpoint
 * @req: request
 *
 * Context: callable when in_interrupt()
 *
 * Sends a request (or a part of the request) to the control endpoint (ep0 in).
 * If the request doesn't fit, the remaining part will be sent from irq.
 * The request is considered fully written only if either :
 *   - last write transferred all remaining bytes, but fifo was not fully filled
 *   - last write was a 0 length write
 *
 * Returns 1 if request fully written, 0 if request only partially sent
 */
static int write_ep0_fifo(struct pxa_ep *ep, struct pxa27x_request *req)
{
	unsigned	count;
	int		is_last, is_short;

	count = write_packet(ep, req, EP0_FIFO_SIZE);
	inc_ep_stats_bytes(ep, count, USB_DIR_IN);

	is_short = (count < EP0_FIFO_SIZE);
	is_last = ((count == 0) || (count < EP0_FIFO_SIZE));

	
	if (unlikely(is_short))
		ep_write_UDCCSR(ep, UDCCSR0_IPR);

	ep_dbg(ep, "in %d bytes%s%s, %d left, req=%p, udccsr0=0x%03x\n",
		count, is_short ? "/S" : "", is_last ? "/L" : "",
		req->req.length - req->req.actual,
		&req->req, udc_ep_readl(ep, UDCCSR));

	return is_last;
}

static int pxa_ep_queue(struct usb_ep *_ep, struct usb_request *_req,
			gfp_t gfp_flags)
{
	struct udc_usb_ep	*udc_usb_ep;
	struct pxa_ep		*ep;
	struct pxa27x_request	*req;
	struct pxa_udc		*dev;
	unsigned long		flags;
	int			rc = 0;
	int			is_first_req;
	unsigned		length;
	int			recursion_detected;

	req = container_of(_req, struct pxa27x_request, req);
	udc_usb_ep = container_of(_ep, struct udc_usb_ep, usb_ep);

	if (unlikely(!_req || !_req->complete || !_req->buf))
		return -EINVAL;

	if (unlikely(!_ep))
		return -EINVAL;

	dev = udc_usb_ep->dev;
	ep = udc_usb_ep->pxa_ep;
	if (unlikely(!ep))
		return -EINVAL;

	dev = ep->dev;
	if (unlikely(!dev->driver || dev->gadget.speed == USB_SPEED_UNKNOWN)) {
		ep_dbg(ep, "bogus device state\n");
		return -ESHUTDOWN;
	}

	if (unlikely(EPXFERTYPE_is_ISO(ep)
			&& req->req.length > ep->fifo_size))
		return -EMSGSIZE;

	spin_lock_irqsave(&ep->lock, flags);
	recursion_detected = ep->in_handle_ep;

	is_first_req = list_empty(&ep->queue);
	ep_dbg(ep, "queue req %p(first=%s), len %d buf %p\n",
			_req, is_first_req ? "yes" : "no",
			_req->length, _req->buf);

	if (!ep->enabled) {
		_req->status = -ESHUTDOWN;
		rc = -ESHUTDOWN;
		goto out_locked;
	}

	if (req->in_use) {
		ep_err(ep, "refusing to queue req %p (already queued)\n", req);
		goto out_locked;
	}

	length = _req->length;
	_req->status = -EINPROGRESS;
	_req->actual = 0;

	ep_add_request(ep, req);
	spin_unlock_irqrestore(&ep->lock, flags);

	if (is_ep0(ep)) {
		switch (dev->ep0state) {
		case WAIT_ACK_SET_CONF_INTERF:
			if (length == 0) {
				ep_end_in_req(ep, req, NULL);
			} else {
				ep_err(ep, "got a request of %d bytes while"
					"in state WAIT_ACK_SET_CONF_INTERF\n",
					length);
				ep_del_request(ep, req);
				rc = -EL2HLT;
			}
			ep0_idle(ep->dev);
			break;
		case IN_DATA_STAGE:
			if (!ep_is_full(ep))
				if (write_ep0_fifo(ep, req))
					ep0_end_in_req(ep, req, NULL);
			break;
		case OUT_DATA_STAGE:
			if ((length == 0) || !epout_has_pkt(ep))
				if (read_ep0_fifo(ep, req))
					ep0_end_out_req(ep, req, NULL);
			break;
		default:
			ep_err(ep, "odd state %s to send me a request\n",
				EP0_STNAME(ep->dev));
			ep_del_request(ep, req);
			rc = -EL2HLT;
			break;
		}
	} else {
		if (!recursion_detected)
			handle_ep(ep);
	}

out:
	return rc;
out_locked:
	spin_unlock_irqrestore(&ep->lock, flags);
	goto out;
}

static int pxa_ep_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	struct pxa_ep		*ep;
	struct udc_usb_ep	*udc_usb_ep;
	struct pxa27x_request	*req;
	unsigned long		flags;
	int			rc = -EINVAL;

	if (!_ep)
		return rc;
	udc_usb_ep = container_of(_ep, struct udc_usb_ep, usb_ep);
	ep = udc_usb_ep->pxa_ep;
	if (!ep || is_ep0(ep))
		return rc;

	spin_lock_irqsave(&ep->lock, flags);

	
	list_for_each_entry(req, &ep->queue, queue) {
		if (&req->req == _req) {
			rc = 0;
			break;
		}
	}

	spin_unlock_irqrestore(&ep->lock, flags);
	if (!rc)
		req_done(ep, req, -ECONNRESET, NULL);
	return rc;
}

static int pxa_ep_set_halt(struct usb_ep *_ep, int value)
{
	struct pxa_ep		*ep;
	struct udc_usb_ep	*udc_usb_ep;
	unsigned long flags;
	int rc;


	if (!_ep)
		return -EINVAL;
	udc_usb_ep = container_of(_ep, struct udc_usb_ep, usb_ep);
	ep = udc_usb_ep->pxa_ep;
	if (!ep || is_ep0(ep))
		return -EINVAL;

	if (value == 0) {
		ep_dbg(ep, "only host can clear halt\n");
		return -EROFS;
	}

	spin_lock_irqsave(&ep->lock, flags);

	rc = -EAGAIN;
	if (ep->dir_in	&& (ep_is_full(ep) || !list_empty(&ep->queue)))
		goto out;

	
	rc = 0;
	ep_write_UDCCSR(ep, UDCCSR_FST | UDCCSR_FEF);
	if (is_ep0(ep))
		set_ep0state(ep->dev, STALL);

out:
	spin_unlock_irqrestore(&ep->lock, flags);
	return rc;
}

static int pxa_ep_fifo_status(struct usb_ep *_ep)
{
	struct pxa_ep		*ep;
	struct udc_usb_ep	*udc_usb_ep;

	if (!_ep)
		return -ENODEV;
	udc_usb_ep = container_of(_ep, struct udc_usb_ep, usb_ep);
	ep = udc_usb_ep->pxa_ep;
	if (!ep || is_ep0(ep))
		return -ENODEV;

	if (ep->dir_in)
		return -EOPNOTSUPP;
	if (ep->dev->gadget.speed == USB_SPEED_UNKNOWN || ep_is_empty(ep))
		return 0;
	else
		return ep_count_bytes_remain(ep) + 1;
}

static void pxa_ep_fifo_flush(struct usb_ep *_ep)
{
	struct pxa_ep		*ep;
	struct udc_usb_ep	*udc_usb_ep;
	unsigned long		flags;

	if (!_ep)
		return;
	udc_usb_ep = container_of(_ep, struct udc_usb_ep, usb_ep);
	ep = udc_usb_ep->pxa_ep;
	if (!ep || is_ep0(ep))
		return;

	spin_lock_irqsave(&ep->lock, flags);

	if (unlikely(!list_empty(&ep->queue)))
		ep_dbg(ep, "called while queue list not empty\n");
	ep_dbg(ep, "called\n");

	
	if (!ep->dir_in) {
		while (!ep_is_empty(ep))
			udc_ep_readl(ep, UDCDR);
	} else {
		
		ep_write_UDCCSR(ep,
				UDCCSR_PC | UDCCSR_FEF | UDCCSR_TRN
				| (EPXFERTYPE_is_ISO(ep) ? 0 : UDCCSR_SST));
	}

	spin_unlock_irqrestore(&ep->lock, flags);
}

static int pxa_ep_enable(struct usb_ep *_ep,
	const struct usb_endpoint_descriptor *desc)
{
	struct pxa_ep		*ep;
	struct udc_usb_ep	*udc_usb_ep;
	struct pxa_udc		*udc;

	if (!_ep || !desc)
		return -EINVAL;

	udc_usb_ep = container_of(_ep, struct udc_usb_ep, usb_ep);
	if (udc_usb_ep->pxa_ep) {
		ep = udc_usb_ep->pxa_ep;
		ep_warn(ep, "usb_ep %s already enabled, doing nothing\n",
			_ep->name);
	} else {
		ep = find_pxa_ep(udc_usb_ep->dev, udc_usb_ep);
	}

	if (!ep || is_ep0(ep)) {
		dev_err(udc_usb_ep->dev->dev,
			"unable to match pxa_ep for ep %s\n",
			_ep->name);
		return -EINVAL;
	}

	if ((desc->bDescriptorType != USB_DT_ENDPOINT)
			|| (ep->type != usb_endpoint_type(desc))) {
		ep_err(ep, "type mismatch\n");
		return -EINVAL;
	}

	if (ep->fifo_size < usb_endpoint_maxp(desc)) {
		ep_err(ep, "bad maxpacket\n");
		return -ERANGE;
	}

	udc_usb_ep->pxa_ep = ep;
	udc = ep->dev;

	if (!udc->driver || udc->gadget.speed == USB_SPEED_UNKNOWN) {
		ep_err(ep, "bogus device state\n");
		return -ESHUTDOWN;
	}

	ep->enabled = 1;

	
	pxa_ep_fifo_flush(_ep);

	ep_dbg(ep, "enabled\n");
	return 0;
}

static int pxa_ep_disable(struct usb_ep *_ep)
{
	struct pxa_ep		*ep;
	struct udc_usb_ep	*udc_usb_ep;

	if (!_ep)
		return -EINVAL;

	udc_usb_ep = container_of(_ep, struct udc_usb_ep, usb_ep);
	ep = udc_usb_ep->pxa_ep;
	if (!ep || is_ep0(ep) || !list_empty(&ep->queue))
		return -EINVAL;

	ep->enabled = 0;
	nuke(ep, -ESHUTDOWN);

	pxa_ep_fifo_flush(_ep);
	udc_usb_ep->pxa_ep = NULL;

	ep_dbg(ep, "disabled\n");
	return 0;
}

static struct usb_ep_ops pxa_ep_ops = {
	.enable		= pxa_ep_enable,
	.disable	= pxa_ep_disable,

	.alloc_request	= pxa_ep_alloc_request,
	.free_request	= pxa_ep_free_request,

	.queue		= pxa_ep_queue,
	.dequeue	= pxa_ep_dequeue,

	.set_halt	= pxa_ep_set_halt,
	.fifo_status	= pxa_ep_fifo_status,
	.fifo_flush	= pxa_ep_fifo_flush,
};

static void dplus_pullup(struct pxa_udc *udc, int on)
{
	if (on) {
		if (gpio_is_valid(udc->mach->gpio_pullup))
			gpio_set_value(udc->mach->gpio_pullup,
				       !udc->mach->gpio_pullup_inverted);
		if (udc->mach->udc_command)
			udc->mach->udc_command(PXA2XX_UDC_CMD_CONNECT);
	} else {
		if (gpio_is_valid(udc->mach->gpio_pullup))
			gpio_set_value(udc->mach->gpio_pullup,
				       udc->mach->gpio_pullup_inverted);
		if (udc->mach->udc_command)
			udc->mach->udc_command(PXA2XX_UDC_CMD_DISCONNECT);
	}
	udc->pullup_on = on;
}

static int pxa_udc_get_frame(struct usb_gadget *_gadget)
{
	struct pxa_udc *udc = to_gadget_udc(_gadget);

	return (udc_readl(udc, UDCFNR) & 0x7ff);
}

static int pxa_udc_wakeup(struct usb_gadget *_gadget)
{
	struct pxa_udc *udc = to_gadget_udc(_gadget);

	
	if ((udc_readl(udc, UDCCR) & UDCCR_DWRE) == 0)
		return -EHOSTUNREACH;
	udc_set_mask_UDCCR(udc, UDCCR_UDR);
	return 0;
}

static void udc_enable(struct pxa_udc *udc);
static void udc_disable(struct pxa_udc *udc);

static int should_enable_udc(struct pxa_udc *udc)
{
	int put_on;

	put_on = ((udc->pullup_on) && (udc->driver));
	put_on &= ((udc->vbus_sensed) || (!udc->transceiver));
	return put_on;
}

static int should_disable_udc(struct pxa_udc *udc)
{
	int put_off;

	put_off = ((!udc->pullup_on) || (!udc->driver));
	put_off |= ((!udc->vbus_sensed) && (udc->transceiver));
	return put_off;
}

static int pxa_udc_pullup(struct usb_gadget *_gadget, int is_active)
{
	struct pxa_udc *udc = to_gadget_udc(_gadget);

	if (!gpio_is_valid(udc->mach->gpio_pullup) && !udc->mach->udc_command)
		return -EOPNOTSUPP;

	dplus_pullup(udc, is_active);

	if (should_enable_udc(udc))
		udc_enable(udc);
	if (should_disable_udc(udc))
		udc_disable(udc);
	return 0;
}

static void udc_enable(struct pxa_udc *udc);
static void udc_disable(struct pxa_udc *udc);

static int pxa_udc_vbus_session(struct usb_gadget *_gadget, int is_active)
{
	struct pxa_udc *udc = to_gadget_udc(_gadget);

	udc->vbus_sensed = is_active;
	if (should_enable_udc(udc))
		udc_enable(udc);
	if (should_disable_udc(udc))
		udc_disable(udc);

	return 0;
}

static int pxa_udc_vbus_draw(struct usb_gadget *_gadget, unsigned mA)
{
	struct pxa_udc *udc;

	udc = to_gadget_udc(_gadget);
	if (udc->transceiver)
		return usb_phy_set_power(udc->transceiver, mA);
	return -EOPNOTSUPP;
}

static int pxa27x_udc_start(struct usb_gadget_driver *driver,
		int (*bind)(struct usb_gadget *));
static int pxa27x_udc_stop(struct usb_gadget_driver *driver);

static const struct usb_gadget_ops pxa_udc_ops = {
	.get_frame	= pxa_udc_get_frame,
	.wakeup		= pxa_udc_wakeup,
	.pullup		= pxa_udc_pullup,
	.vbus_session	= pxa_udc_vbus_session,
	.vbus_draw	= pxa_udc_vbus_draw,
	.start		= pxa27x_udc_start,
	.stop		= pxa27x_udc_stop,
};

static void udc_disable(struct pxa_udc *udc)
{
	if (!udc->enabled)
		return;

	udc_writel(udc, UDCICR0, 0);
	udc_writel(udc, UDCICR1, 0);

	udc_clear_mask_UDCCR(udc, UDCCR_UDE);
	clk_disable(udc->clk);

	ep0_idle(udc);
	udc->gadget.speed = USB_SPEED_UNKNOWN;

	udc->enabled = 0;
}

static __init void udc_init_data(struct pxa_udc *dev)
{
	int i;
	struct pxa_ep *ep;

	
	INIT_LIST_HEAD(&dev->gadget.ep_list);
	INIT_LIST_HEAD(&dev->gadget.ep0->ep_list);
	dev->udc_usb_ep[0].pxa_ep = &dev->pxa_ep[0];
	ep0_idle(dev);

	
	for (i = 0; i < NR_PXA_ENDPOINTS; i++) {
		ep = &dev->pxa_ep[i];

		ep->enabled = is_ep0(ep);
		INIT_LIST_HEAD(&ep->queue);
		spin_lock_init(&ep->lock);
	}

	
	for (i = 1; i < NR_USB_ENDPOINTS; i++)
		list_add_tail(&dev->udc_usb_ep[i].usb_ep.ep_list,
				&dev->gadget.ep_list);
}

static void udc_enable(struct pxa_udc *udc)
{
	if (udc->enabled)
		return;

	udc_writel(udc, UDCICR0, 0);
	udc_writel(udc, UDCICR1, 0);
	udc_clear_mask_UDCCR(udc, UDCCR_UDE);

	clk_enable(udc->clk);

	ep0_idle(udc);
	udc->gadget.speed = USB_SPEED_FULL;
	memset(&udc->stats, 0, sizeof(udc->stats));

	udc_set_mask_UDCCR(udc, UDCCR_UDE);
	ep_write_UDCCSR(&udc->pxa_ep[0], UDCCSR0_ACM);
	udelay(2);
	if (udc_readl(udc, UDCCR) & UDCCR_EMCE)
		dev_err(udc->dev, "Configuration errors, udc disabled\n");

	msleep(100);

	
	udc_writel(udc, UDCICR1,
			UDCICR1_IECC | UDCICR1_IERU
			| UDCICR1_IESU | UDCICR1_IERS);

	
	pio_irq_enable(&udc->pxa_ep[0]);

	udc->enabled = 1;
}

static int pxa27x_udc_start(struct usb_gadget_driver *driver,
		int (*bind)(struct usb_gadget *))
{
	struct pxa_udc *udc = the_controller;
	int retval;

	if (!driver || driver->max_speed < USB_SPEED_FULL || !bind
			|| !driver->disconnect || !driver->setup)
		return -EINVAL;
	if (!udc)
		return -ENODEV;
	if (udc->driver)
		return -EBUSY;

	
	udc->driver = driver;
	udc->gadget.dev.driver = &driver->driver;
	dplus_pullup(udc, 1);

	retval = device_add(&udc->gadget.dev);
	if (retval) {
		dev_err(udc->dev, "device_add error %d\n", retval);
		goto add_fail;
	}
	retval = bind(&udc->gadget);
	if (retval) {
		dev_err(udc->dev, "bind to driver %s --> error %d\n",
			driver->driver.name, retval);
		goto bind_fail;
	}
	dev_dbg(udc->dev, "registered gadget driver '%s'\n",
		driver->driver.name);

	if (udc->transceiver) {
		retval = otg_set_peripheral(udc->transceiver->otg,
						&udc->gadget);
		if (retval) {
			dev_err(udc->dev, "can't bind to transceiver\n");
			goto transceiver_fail;
		}
	}

	if (should_enable_udc(udc))
		udc_enable(udc);
	return 0;

transceiver_fail:
	if (driver->unbind)
		driver->unbind(&udc->gadget);
bind_fail:
	device_del(&udc->gadget.dev);
add_fail:
	udc->driver = NULL;
	udc->gadget.dev.driver = NULL;
	return retval;
}

static void stop_activity(struct pxa_udc *udc, struct usb_gadget_driver *driver)
{
	int i;

	
	if (udc->gadget.speed == USB_SPEED_UNKNOWN)
		driver = NULL;
	udc->gadget.speed = USB_SPEED_UNKNOWN;

	for (i = 0; i < NR_USB_ENDPOINTS; i++)
		pxa_ep_disable(&udc->udc_usb_ep[i].usb_ep);

	if (driver)
		driver->disconnect(&udc->gadget);
}

static int pxa27x_udc_stop(struct usb_gadget_driver *driver)
{
	struct pxa_udc *udc = the_controller;

	if (!udc)
		return -ENODEV;
	if (!driver || driver != udc->driver || !driver->unbind)
		return -EINVAL;

	stop_activity(udc, driver);
	udc_disable(udc);
	dplus_pullup(udc, 0);

	driver->unbind(&udc->gadget);
	udc->driver = NULL;

	device_del(&udc->gadget.dev);
	dev_info(udc->dev, "unregistered gadget driver '%s'\n",
		 driver->driver.name);

	if (udc->transceiver)
		return otg_set_peripheral(udc->transceiver->otg, NULL);
	return 0;
}

static void handle_ep0_ctrl_req(struct pxa_udc *udc,
				struct pxa27x_request *req)
{
	struct pxa_ep *ep = &udc->pxa_ep[0];
	union {
		struct usb_ctrlrequest	r;
		u32			word[2];
	} u;
	int i;
	int have_extrabytes = 0;
	unsigned long flags;

	nuke(ep, -EPROTO);
	spin_lock_irqsave(&ep->lock, flags);

	if (epout_has_pkt(ep) && (ep_count_bytes_remain(ep) == 0))
		ep_write_UDCCSR(ep, UDCCSR0_OPC);

	
	for (i = 0; i < 2; i++) {
		if (unlikely(ep_is_empty(ep)))
			goto stall;
		u.word[i] = udc_ep_readl(ep, UDCDR);
	}

	have_extrabytes = !ep_is_empty(ep);
	while (!ep_is_empty(ep)) {
		i = udc_ep_readl(ep, UDCDR);
		ep_err(ep, "wrong to have extra bytes for setup : 0x%08x\n", i);
	}

	ep_dbg(ep, "SETUP %02x.%02x v%04x i%04x l%04x\n",
		u.r.bRequestType, u.r.bRequest,
		le16_to_cpu(u.r.wValue), le16_to_cpu(u.r.wIndex),
		le16_to_cpu(u.r.wLength));
	if (unlikely(have_extrabytes))
		goto stall;

	if (u.r.bRequestType & USB_DIR_IN)
		set_ep0state(udc, IN_DATA_STAGE);
	else
		set_ep0state(udc, OUT_DATA_STAGE);

	
	ep_write_UDCCSR(ep, UDCCSR0_SA | UDCCSR0_OPC);

	spin_unlock_irqrestore(&ep->lock, flags);
	i = udc->driver->setup(&udc->gadget, &u.r);
	spin_lock_irqsave(&ep->lock, flags);
	if (i < 0)
		goto stall;
out:
	spin_unlock_irqrestore(&ep->lock, flags);
	return;
stall:
	ep_dbg(ep, "protocol STALL, udccsr0=%03x err %d\n",
		udc_ep_readl(ep, UDCCSR), i);
	ep_write_UDCCSR(ep, UDCCSR0_FST | UDCCSR0_FTF);
	set_ep0state(udc, STALL);
	goto out;
}

static void handle_ep0(struct pxa_udc *udc, int fifo_irq, int opc_irq)
{
	u32			udccsr0;
	struct pxa_ep		*ep = &udc->pxa_ep[0];
	struct pxa27x_request	*req = NULL;
	int			completed = 0;

	if (!list_empty(&ep->queue))
		req = list_entry(ep->queue.next, struct pxa27x_request, queue);

	udccsr0 = udc_ep_readl(ep, UDCCSR);
	ep_dbg(ep, "state=%s, req=%p, udccsr0=0x%03x, udcbcr=%d, irq_msk=%x\n",
		EP0_STNAME(udc), req, udccsr0, udc_ep_readl(ep, UDCBCR),
		(fifo_irq << 1 | opc_irq));

	if (udccsr0 & UDCCSR0_SST) {
		ep_dbg(ep, "clearing stall status\n");
		nuke(ep, -EPIPE);
		ep_write_UDCCSR(ep, UDCCSR0_SST);
		ep0_idle(udc);
	}

	if (udccsr0 & UDCCSR0_SA) {
		nuke(ep, 0);
		set_ep0state(udc, SETUP_STAGE);
	}

	switch (udc->ep0state) {
	case WAIT_FOR_SETUP:
		break;
	case SETUP_STAGE:
		udccsr0 &= UDCCSR0_CTRL_REQ_MASK;
		if (likely(udccsr0 == UDCCSR0_CTRL_REQ_MASK))
			handle_ep0_ctrl_req(udc, req);
		break;
	case IN_DATA_STAGE:			
		if (epout_has_pkt(ep))
			ep_write_UDCCSR(ep, UDCCSR0_OPC);
		if (req && !ep_is_full(ep))
			completed = write_ep0_fifo(ep, req);
		if (completed)
			ep0_end_in_req(ep, req, NULL);
		break;
	case OUT_DATA_STAGE:			
		if (epout_has_pkt(ep) && req)
			completed = read_ep0_fifo(ep, req);
		if (completed)
			ep0_end_out_req(ep, req, NULL);
		break;
	case STALL:
		ep_write_UDCCSR(ep, UDCCSR0_FST);
		break;
	case IN_STATUS_STAGE:
		if (opc_irq)
			ep0_idle(udc);
		break;
	case OUT_STATUS_STAGE:
	case WAIT_ACK_SET_CONF_INTERF:
		ep_warn(ep, "should never get in %s state here!!!\n",
				EP0_STNAME(ep->dev));
		ep0_idle(udc);
		break;
	}
}

static void handle_ep(struct pxa_ep *ep)
{
	struct pxa27x_request	*req;
	int completed;
	u32 udccsr;
	int is_in = ep->dir_in;
	int loop = 0;
	unsigned long		flags;

	spin_lock_irqsave(&ep->lock, flags);
	if (ep->in_handle_ep)
		goto recursion_detected;
	ep->in_handle_ep = 1;

	do {
		completed = 0;
		udccsr = udc_ep_readl(ep, UDCCSR);

		if (likely(!list_empty(&ep->queue)))
			req = list_entry(ep->queue.next,
					struct pxa27x_request, queue);
		else
			req = NULL;

		ep_dbg(ep, "req:%p, udccsr 0x%03x loop=%d\n",
				req, udccsr, loop++);

		if (unlikely(udccsr & (UDCCSR_SST | UDCCSR_TRN)))
			udc_ep_writel(ep, UDCCSR,
					udccsr & (UDCCSR_SST | UDCCSR_TRN));
		if (!req)
			break;

		if (unlikely(is_in)) {
			if (likely(!ep_is_full(ep)))
				completed = write_fifo(ep, req);
		} else {
			if (likely(epout_has_pkt(ep)))
				completed = read_fifo(ep, req);
		}

		if (completed) {
			if (is_in)
				ep_end_in_req(ep, req, &flags);
			else
				ep_end_out_req(ep, req, &flags);
		}
	} while (completed);

	ep->in_handle_ep = 0;
recursion_detected:
	spin_unlock_irqrestore(&ep->lock, flags);
}

static void pxa27x_change_configuration(struct pxa_udc *udc, int config)
{
	struct usb_ctrlrequest req ;

	dev_dbg(udc->dev, "config=%d\n", config);

	udc->config = config;
	udc->last_interface = 0;
	udc->last_alternate = 0;

	req.bRequestType = 0;
	req.bRequest = USB_REQ_SET_CONFIGURATION;
	req.wValue = config;
	req.wIndex = 0;
	req.wLength = 0;

	set_ep0state(udc, WAIT_ACK_SET_CONF_INTERF);
	udc->driver->setup(&udc->gadget, &req);
	ep_write_UDCCSR(&udc->pxa_ep[0], UDCCSR0_AREN);
}

static void pxa27x_change_interface(struct pxa_udc *udc, int iface, int alt)
{
	struct usb_ctrlrequest  req;

	dev_dbg(udc->dev, "interface=%d, alternate setting=%d\n", iface, alt);

	udc->last_interface = iface;
	udc->last_alternate = alt;

	req.bRequestType = USB_RECIP_INTERFACE;
	req.bRequest = USB_REQ_SET_INTERFACE;
	req.wValue = alt;
	req.wIndex = iface;
	req.wLength = 0;

	set_ep0state(udc, WAIT_ACK_SET_CONF_INTERF);
	udc->driver->setup(&udc->gadget, &req);
	ep_write_UDCCSR(&udc->pxa_ep[0], UDCCSR0_AREN);
}

static void irq_handle_data(int irq, struct pxa_udc *udc)
{
	int i;
	struct pxa_ep *ep;
	u32 udcisr0 = udc_readl(udc, UDCISR0) & UDCCISR0_EP_MASK;
	u32 udcisr1 = udc_readl(udc, UDCISR1) & UDCCISR1_EP_MASK;

	if (udcisr0 & UDCISR_INT_MASK) {
		udc->pxa_ep[0].stats.irqs++;
		udc_writel(udc, UDCISR0, UDCISR_INT(0, UDCISR_INT_MASK));
		handle_ep0(udc, !!(udcisr0 & UDCICR_FIFOERR),
				!!(udcisr0 & UDCICR_PKTCOMPL));
	}

	udcisr0 >>= 2;
	for (i = 1; udcisr0 != 0 && i < 16; udcisr0 >>= 2, i++) {
		if (!(udcisr0 & UDCISR_INT_MASK))
			continue;

		udc_writel(udc, UDCISR0, UDCISR_INT(i, UDCISR_INT_MASK));

		WARN_ON(i >= ARRAY_SIZE(udc->pxa_ep));
		if (i < ARRAY_SIZE(udc->pxa_ep)) {
			ep = &udc->pxa_ep[i];
			ep->stats.irqs++;
			handle_ep(ep);
		}
	}

	for (i = 16; udcisr1 != 0 && i < 24; udcisr1 >>= 2, i++) {
		udc_writel(udc, UDCISR1, UDCISR_INT(i - 16, UDCISR_INT_MASK));
		if (!(udcisr1 & UDCISR_INT_MASK))
			continue;

		WARN_ON(i >= ARRAY_SIZE(udc->pxa_ep));
		if (i < ARRAY_SIZE(udc->pxa_ep)) {
			ep = &udc->pxa_ep[i];
			ep->stats.irqs++;
			handle_ep(ep);
		}
	}

}

static void irq_udc_suspend(struct pxa_udc *udc)
{
	udc_writel(udc, UDCISR1, UDCISR1_IRSU);
	udc->stats.irqs_suspend++;

	if (udc->gadget.speed != USB_SPEED_UNKNOWN
			&& udc->driver && udc->driver->suspend)
		udc->driver->suspend(&udc->gadget);
	ep0_idle(udc);
}

static void irq_udc_resume(struct pxa_udc *udc)
{
	udc_writel(udc, UDCISR1, UDCISR1_IRRU);
	udc->stats.irqs_resume++;

	if (udc->gadget.speed != USB_SPEED_UNKNOWN
			&& udc->driver && udc->driver->resume)
		udc->driver->resume(&udc->gadget);
}

static void irq_udc_reconfig(struct pxa_udc *udc)
{
	unsigned config, interface, alternate, config_change;
	u32 udccr = udc_readl(udc, UDCCR);

	udc_writel(udc, UDCISR1, UDCISR1_IRCC);
	udc->stats.irqs_reconfig++;

	config = (udccr & UDCCR_ACN) >> UDCCR_ACN_S;
	config_change = (config != udc->config);
	pxa27x_change_configuration(udc, config);

	interface = (udccr & UDCCR_AIN) >> UDCCR_AIN_S;
	alternate = (udccr & UDCCR_AAISN) >> UDCCR_AAISN_S;
	pxa27x_change_interface(udc, interface, alternate);

	if (config_change)
		update_pxa_ep_matches(udc);
	udc_set_mask_UDCCR(udc, UDCCR_SMAC);
}

static void irq_udc_reset(struct pxa_udc *udc)
{
	u32 udccr = udc_readl(udc, UDCCR);
	struct pxa_ep *ep = &udc->pxa_ep[0];

	dev_info(udc->dev, "USB reset\n");
	udc_writel(udc, UDCISR1, UDCISR1_IRRS);
	udc->stats.irqs_reset++;

	if ((udccr & UDCCR_UDA) == 0) {
		dev_dbg(udc->dev, "USB reset start\n");
		stop_activity(udc, udc->driver);
	}
	udc->gadget.speed = USB_SPEED_FULL;
	memset(&udc->stats, 0, sizeof udc->stats);

	nuke(ep, -EPROTO);
	ep_write_UDCCSR(ep, UDCCSR0_FTF | UDCCSR0_OPC);
	ep0_idle(udc);
}

static irqreturn_t pxa_udc_irq(int irq, void *_dev)
{
	struct pxa_udc *udc = _dev;
	u32 udcisr0 = udc_readl(udc, UDCISR0);
	u32 udcisr1 = udc_readl(udc, UDCISR1);
	u32 udccr = udc_readl(udc, UDCCR);
	u32 udcisr1_spec;

	dev_vdbg(udc->dev, "Interrupt, UDCISR0:0x%08x, UDCISR1:0x%08x, "
		 "UDCCR:0x%08x\n", udcisr0, udcisr1, udccr);

	udcisr1_spec = udcisr1 & 0xf8000000;
	if (unlikely(udcisr1_spec & UDCISR1_IRSU))
		irq_udc_suspend(udc);
	if (unlikely(udcisr1_spec & UDCISR1_IRRU))
		irq_udc_resume(udc);
	if (unlikely(udcisr1_spec & UDCISR1_IRCC))
		irq_udc_reconfig(udc);
	if (unlikely(udcisr1_spec & UDCISR1_IRRS))
		irq_udc_reset(udc);

	if ((udcisr0 & UDCCISR0_EP_MASK) | (udcisr1 & UDCCISR1_EP_MASK))
		irq_handle_data(irq, udc);

	return IRQ_HANDLED;
}

static struct pxa_udc memory = {
	.gadget = {
		.ops		= &pxa_udc_ops,
		.ep0		= &memory.udc_usb_ep[0].usb_ep,
		.name		= driver_name,
		.dev = {
			.init_name	= "gadget",
		},
	},

	.udc_usb_ep = {
		USB_EP_CTRL,
		USB_EP_OUT_BULK(1),
		USB_EP_IN_BULK(2),
		USB_EP_IN_ISO(3),
		USB_EP_OUT_ISO(4),
		USB_EP_IN_INT(5),
	},

	.pxa_ep = {
		PXA_EP_CTRL,
		
		PXA_EP_OUT_BULK(1, 1, 3, 0, 0),
		PXA_EP_IN_BULK(2,  2, 3, 0, 0),
		
		PXA_EP_OUT_BULK(3, 1, 1, 0, 0),
		PXA_EP_IN_BULK(4,  2, 1, 0, 0),
		PXA_EP_IN_ISO(5,   3, 1, 0, 0),
		PXA_EP_OUT_ISO(6,  4, 1, 0, 0),
		PXA_EP_IN_INT(7,   5, 1, 0, 0),
		
		PXA_EP_OUT_BULK(8, 1, 2, 0, 0),
		PXA_EP_IN_BULK(9,  2, 2, 0, 0),
		PXA_EP_IN_INT(10,  5, 2, 0, 0),
		PXA_EP_OUT_BULK(11, 1, 2, 1, 0),
		PXA_EP_IN_BULK(12,  2, 2, 1, 0),
		
		PXA_EP_OUT_BULK(13, 1, 1, 1, 1),
		PXA_EP_IN_BULK(14,  2, 1, 1, 1),
	}
};

static int __init pxa_udc_probe(struct platform_device *pdev)
{
	struct resource *regs;
	struct pxa_udc *udc = &memory;
	int retval = 0, gpio;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs)
		return -ENXIO;
	udc->irq = platform_get_irq(pdev, 0);
	if (udc->irq < 0)
		return udc->irq;

	udc->dev = &pdev->dev;
	udc->mach = pdev->dev.platform_data;
	udc->transceiver = usb_get_transceiver();

	gpio = udc->mach->gpio_pullup;
	if (gpio_is_valid(gpio)) {
		retval = gpio_request(gpio, "USB D+ pullup");
		if (retval == 0)
			gpio_direction_output(gpio,
				       udc->mach->gpio_pullup_inverted);
	}
	if (retval) {
		dev_err(&pdev->dev, "Couldn't request gpio %d : %d\n",
			gpio, retval);
		return retval;
	}

	udc->clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(udc->clk)) {
		retval = PTR_ERR(udc->clk);
		goto err_clk;
	}

	retval = -ENOMEM;
	udc->regs = ioremap(regs->start, resource_size(regs));
	if (!udc->regs) {
		dev_err(&pdev->dev, "Unable to map UDC I/O memory\n");
		goto err_map;
	}

	device_initialize(&udc->gadget.dev);
	udc->gadget.dev.parent = &pdev->dev;
	udc->gadget.dev.dma_mask = NULL;
	udc->vbus_sensed = 0;

	the_controller = udc;
	platform_set_drvdata(pdev, udc);
	udc_init_data(udc);
	pxa_eps_setup(udc);

	
	retval = request_irq(udc->irq, pxa_udc_irq,
			IRQF_SHARED, driver_name, udc);
	if (retval != 0) {
		dev_err(udc->dev, "%s: can't get irq %i, err %d\n",
			driver_name, IRQ_USB, retval);
		goto err_irq;
	}
	retval = usb_add_gadget_udc(&pdev->dev, &udc->gadget);
	if (retval)
		goto err_add_udc;

	pxa_init_debugfs(udc);
	return 0;
err_add_udc:
	free_irq(udc->irq, udc);
err_irq:
	iounmap(udc->regs);
err_map:
	clk_put(udc->clk);
	udc->clk = NULL;
err_clk:
	return retval;
}

static int __exit pxa_udc_remove(struct platform_device *_dev)
{
	struct pxa_udc *udc = platform_get_drvdata(_dev);
	int gpio = udc->mach->gpio_pullup;

	usb_del_gadget_udc(&udc->gadget);
	usb_gadget_unregister_driver(udc->driver);
	free_irq(udc->irq, udc);
	pxa_cleanup_debugfs(udc);
	if (gpio_is_valid(gpio))
		gpio_free(gpio);

	usb_put_transceiver(udc->transceiver);

	udc->transceiver = NULL;
	platform_set_drvdata(_dev, NULL);
	the_controller = NULL;
	clk_put(udc->clk);
	iounmap(udc->regs);

	return 0;
}

static void pxa_udc_shutdown(struct platform_device *_dev)
{
	struct pxa_udc *udc = platform_get_drvdata(_dev);

	if (udc_readl(udc, UDCCR) & UDCCR_UDE)
		udc_disable(udc);
}

#ifdef CONFIG_PXA27x
extern void pxa27x_clear_otgph(void);
#else
#define pxa27x_clear_otgph()   do {} while (0)
#endif

#ifdef CONFIG_PM
static int pxa_udc_suspend(struct platform_device *_dev, pm_message_t state)
{
	int i;
	struct pxa_udc *udc = platform_get_drvdata(_dev);
	struct pxa_ep *ep;

	ep = &udc->pxa_ep[0];
	udc->udccsr0 = udc_ep_readl(ep, UDCCSR);
	for (i = 1; i < NR_PXA_ENDPOINTS; i++) {
		ep = &udc->pxa_ep[i];
		ep->udccsr_value = udc_ep_readl(ep, UDCCSR);
		ep->udccr_value  = udc_ep_readl(ep, UDCCR);
		ep_dbg(ep, "udccsr:0x%03x, udccr:0x%x\n",
				ep->udccsr_value, ep->udccr_value);
	}

	udc_disable(udc);
	udc->pullup_resume = udc->pullup_on;
	dplus_pullup(udc, 0);

	return 0;
}

static int pxa_udc_resume(struct platform_device *_dev)
{
	int i;
	struct pxa_udc *udc = platform_get_drvdata(_dev);
	struct pxa_ep *ep;

	ep = &udc->pxa_ep[0];
	udc_ep_writel(ep, UDCCSR, udc->udccsr0 & (UDCCSR0_FST | UDCCSR0_DME));
	for (i = 1; i < NR_PXA_ENDPOINTS; i++) {
		ep = &udc->pxa_ep[i];
		udc_ep_writel(ep, UDCCSR, ep->udccsr_value);
		udc_ep_writel(ep, UDCCR,  ep->udccr_value);
		ep_dbg(ep, "udccsr:0x%03x, udccr:0x%x\n",
				ep->udccsr_value, ep->udccr_value);
	}

	dplus_pullup(udc, udc->pullup_resume);
	if (should_enable_udc(udc))
		udc_enable(udc);
	pxa27x_clear_otgph();

	return 0;
}
#endif

MODULE_ALIAS("platform:pxa27x-udc");

static struct platform_driver udc_driver = {
	.driver		= {
		.name	= "pxa27x-udc",
		.owner	= THIS_MODULE,
	},
	.remove		= __exit_p(pxa_udc_remove),
	.shutdown	= pxa_udc_shutdown,
#ifdef CONFIG_PM
	.suspend	= pxa_udc_suspend,
	.resume		= pxa_udc_resume
#endif
};

static int __init udc_init(void)
{
	if (!cpu_is_pxa27x() && !cpu_is_pxa3xx())
		return -ENODEV;

	printk(KERN_INFO "%s: version %s\n", driver_name, DRIVER_VERSION);
	return platform_driver_probe(&udc_driver, pxa_udc_probe);
}
module_init(udc_init);


static void __exit udc_exit(void)
{
	platform_driver_unregister(&udc_driver);
}
module_exit(udc_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Robert Jarzmik");
MODULE_LICENSE("GPL");
