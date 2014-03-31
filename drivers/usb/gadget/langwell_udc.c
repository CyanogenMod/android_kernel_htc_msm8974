/*
 * Intel Langwell USB Device Controller driver
 * Copyright (C) 2008-2009, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */



#include <linux/module.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/otg.h>
#include <linux/pm.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <asm/unaligned.h>

#include "langwell_udc.h"


#define	DRIVER_DESC		"Intel Langwell USB Device Controller driver"
#define	DRIVER_VERSION		"16 May 2009"

static const char driver_name[] = "langwell_udc";
static const char driver_desc[] = DRIVER_DESC;


static const struct usb_endpoint_descriptor
langwell_ep0_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	0,
	.bmAttributes =		USB_ENDPOINT_XFER_CONTROL,
	.wMaxPacketSize =	EP0_MAX_PKT_SIZE,
};



#ifdef	VERBOSE_DEBUG
static inline void print_all_registers(struct langwell_udc *dev)
{
	int	i;

	
	dev_dbg(&dev->pdev->dev,
		"Capability Registers (offset: 0x%04x, length: 0x%08x)\n",
		CAP_REG_OFFSET, (u32)sizeof(struct langwell_cap_regs));
	dev_dbg(&dev->pdev->dev, "caplength=0x%02x\n",
			readb(&dev->cap_regs->caplength));
	dev_dbg(&dev->pdev->dev, "hciversion=0x%04x\n",
			readw(&dev->cap_regs->hciversion));
	dev_dbg(&dev->pdev->dev, "hcsparams=0x%08x\n",
			readl(&dev->cap_regs->hcsparams));
	dev_dbg(&dev->pdev->dev, "hccparams=0x%08x\n",
			readl(&dev->cap_regs->hccparams));
	dev_dbg(&dev->pdev->dev, "dciversion=0x%04x\n",
			readw(&dev->cap_regs->dciversion));
	dev_dbg(&dev->pdev->dev, "dccparams=0x%08x\n",
			readl(&dev->cap_regs->dccparams));

	
	dev_dbg(&dev->pdev->dev,
		"Operational Registers (offset: 0x%04x, length: 0x%08x)\n",
		OP_REG_OFFSET, (u32)sizeof(struct langwell_op_regs));
	dev_dbg(&dev->pdev->dev, "extsts=0x%08x\n",
			readl(&dev->op_regs->extsts));
	dev_dbg(&dev->pdev->dev, "extintr=0x%08x\n",
			readl(&dev->op_regs->extintr));
	dev_dbg(&dev->pdev->dev, "usbcmd=0x%08x\n",
			readl(&dev->op_regs->usbcmd));
	dev_dbg(&dev->pdev->dev, "usbsts=0x%08x\n",
			readl(&dev->op_regs->usbsts));
	dev_dbg(&dev->pdev->dev, "usbintr=0x%08x\n",
			readl(&dev->op_regs->usbintr));
	dev_dbg(&dev->pdev->dev, "frindex=0x%08x\n",
			readl(&dev->op_regs->frindex));
	dev_dbg(&dev->pdev->dev, "ctrldssegment=0x%08x\n",
			readl(&dev->op_regs->ctrldssegment));
	dev_dbg(&dev->pdev->dev, "deviceaddr=0x%08x\n",
			readl(&dev->op_regs->deviceaddr));
	dev_dbg(&dev->pdev->dev, "endpointlistaddr=0x%08x\n",
			readl(&dev->op_regs->endpointlistaddr));
	dev_dbg(&dev->pdev->dev, "ttctrl=0x%08x\n",
			readl(&dev->op_regs->ttctrl));
	dev_dbg(&dev->pdev->dev, "burstsize=0x%08x\n",
			readl(&dev->op_regs->burstsize));
	dev_dbg(&dev->pdev->dev, "txfilltuning=0x%08x\n",
			readl(&dev->op_regs->txfilltuning));
	dev_dbg(&dev->pdev->dev, "txttfilltuning=0x%08x\n",
			readl(&dev->op_regs->txttfilltuning));
	dev_dbg(&dev->pdev->dev, "ic_usb=0x%08x\n",
			readl(&dev->op_regs->ic_usb));
	dev_dbg(&dev->pdev->dev, "ulpi_viewport=0x%08x\n",
			readl(&dev->op_regs->ulpi_viewport));
	dev_dbg(&dev->pdev->dev, "configflag=0x%08x\n",
			readl(&dev->op_regs->configflag));
	dev_dbg(&dev->pdev->dev, "portsc1=0x%08x\n",
			readl(&dev->op_regs->portsc1));
	dev_dbg(&dev->pdev->dev, "devlc=0x%08x\n",
			readl(&dev->op_regs->devlc));
	dev_dbg(&dev->pdev->dev, "otgsc=0x%08x\n",
			readl(&dev->op_regs->otgsc));
	dev_dbg(&dev->pdev->dev, "usbmode=0x%08x\n",
			readl(&dev->op_regs->usbmode));
	dev_dbg(&dev->pdev->dev, "endptnak=0x%08x\n",
			readl(&dev->op_regs->endptnak));
	dev_dbg(&dev->pdev->dev, "endptnaken=0x%08x\n",
			readl(&dev->op_regs->endptnaken));
	dev_dbg(&dev->pdev->dev, "endptsetupstat=0x%08x\n",
			readl(&dev->op_regs->endptsetupstat));
	dev_dbg(&dev->pdev->dev, "endptprime=0x%08x\n",
			readl(&dev->op_regs->endptprime));
	dev_dbg(&dev->pdev->dev, "endptflush=0x%08x\n",
			readl(&dev->op_regs->endptflush));
	dev_dbg(&dev->pdev->dev, "endptstat=0x%08x\n",
			readl(&dev->op_regs->endptstat));
	dev_dbg(&dev->pdev->dev, "endptcomplete=0x%08x\n",
			readl(&dev->op_regs->endptcomplete));

	for (i = 0; i < dev->ep_max / 2; i++) {
		dev_dbg(&dev->pdev->dev, "endptctrl[%d]=0x%08x\n",
				i, readl(&dev->op_regs->endptctrl[i]));
	}
}
#else

#define	print_all_registers(dev)	do { } while (0)

#endif 



#define	is_in(ep)	(((ep)->ep_num == 0) ? ((ep)->dev->ep0_dir ==	\
			USB_DIR_IN) : (usb_endpoint_dir_in((ep)->desc)))

#define	DIR_STRING(ep)	(is_in(ep) ? "in" : "out")


static char *type_string(const struct usb_endpoint_descriptor *desc)
{
	switch (usb_endpoint_type(desc)) {
	case USB_ENDPOINT_XFER_BULK:
		return "bulk";
	case USB_ENDPOINT_XFER_ISOC:
		return "iso";
	case USB_ENDPOINT_XFER_INT:
		return "int";
	};

	return "control";
}


static void ep_reset(struct langwell_ep *ep, unsigned char ep_num,
		unsigned char is_in, unsigned char ep_type)
{
	struct langwell_udc	*dev;
	u32			endptctrl;

	dev = ep->dev;
	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	endptctrl = readl(&dev->op_regs->endptctrl[ep_num]);
	if (is_in) {	
		if (ep_num)
			endptctrl |= EPCTRL_TXR;
		endptctrl |= EPCTRL_TXE;
		endptctrl |= ep_type << EPCTRL_TXT_SHIFT;
	} else {	
		if (ep_num)
			endptctrl |= EPCTRL_RXR;
		endptctrl |= EPCTRL_RXE;
		endptctrl |= ep_type << EPCTRL_RXT_SHIFT;
	}

	writel(endptctrl, &dev->op_regs->endptctrl[ep_num]);

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}


static void ep0_reset(struct langwell_udc *dev)
{
	struct langwell_ep	*ep;
	int			i;

	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	
	for (i = 0; i < 2; i++) {
		ep = &dev->ep[i];
		ep->dev = dev;

		
		ep->dqh = &dev->ep_dqh[i];

		
		ep->dqh->dqh_ios = 1;
		ep->dqh->dqh_mpl = EP0_MAX_PKT_SIZE;

		
		if (is_in(ep))
			ep->dqh->dqh_zlt = 0;
		ep->dqh->dqh_mult = 0;

		ep->dqh->dtd_next = DTD_TERM;

		
		ep_reset(&dev->ep[0], 0, i, USB_ENDPOINT_XFER_CONTROL);
	}

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}




static int langwell_ep_enable(struct usb_ep *_ep,
		const struct usb_endpoint_descriptor *desc)
{
	struct langwell_udc	*dev;
	struct langwell_ep	*ep;
	u16			max = 0;
	unsigned long		flags;
	int			i, retval = 0;
	unsigned char		zlt, ios = 0, mult = 0;

	ep = container_of(_ep, struct langwell_ep, ep);
	dev = ep->dev;
	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	if (!_ep || !desc || ep->desc
			|| desc->bDescriptorType != USB_DT_ENDPOINT)
		return -EINVAL;

	if (!dev->driver || dev->gadget.speed == USB_SPEED_UNKNOWN)
		return -ESHUTDOWN;

	max = usb_endpoint_maxp(desc);

	zlt = 1;

	switch (usb_endpoint_type(desc)) {
	case USB_ENDPOINT_XFER_CONTROL:
		ios = 1;
		break;
	case USB_ENDPOINT_XFER_BULK:
		if ((dev->gadget.speed == USB_SPEED_HIGH
					&& max != 512)
				|| (dev->gadget.speed == USB_SPEED_FULL
					&& max > 64)) {
			goto done;
		}
		break;
	case USB_ENDPOINT_XFER_INT:
		if (strstr(ep->ep.name, "-iso")) 
			goto done;

		switch (dev->gadget.speed) {
		case USB_SPEED_HIGH:
			if (max <= 1024)
				break;
		case USB_SPEED_FULL:
			if (max <= 64)
				break;
		default:
			if (max <= 8)
				break;
			goto done;
		}
		break;
	case USB_ENDPOINT_XFER_ISOC:
		if (strstr(ep->ep.name, "-bulk")
				|| strstr(ep->ep.name, "-int"))
			goto done;

		switch (dev->gadget.speed) {
		case USB_SPEED_HIGH:
			if (max <= 1024)
				break;
		case USB_SPEED_FULL:
			if (max <= 1023)
				break;
		default:
			goto done;
		}
		mult = (unsigned char)(1 + ((max >> 11) & 0x03));
		max = max & 0x8ff;	
		
		if (mult > 3)
			goto done;
		break;
	default:
		goto done;
	}

	spin_lock_irqsave(&dev->lock, flags);

	ep->ep.maxpacket = max;
	ep->desc = desc;
	ep->stopped = 0;
	ep->ep_num = usb_endpoint_num(desc);

	
	ep->ep_type = usb_endpoint_type(desc);

	
	ep_reset(ep, ep->ep_num, is_in(ep), ep->ep_type);

	
	i = ep->ep_num * 2 + is_in(ep);
	ep->dqh = &dev->ep_dqh[i];
	ep->dqh->dqh_ios = ios;
	ep->dqh->dqh_mpl = cpu_to_le16(max);
	ep->dqh->dqh_zlt = zlt;
	ep->dqh->dqh_mult = mult;
	ep->dqh->dtd_next = DTD_TERM;

	dev_dbg(&dev->pdev->dev, "enabled %s (ep%d%s-%s), max %04x\n",
			_ep->name,
			ep->ep_num,
			DIR_STRING(ep),
			type_string(desc),
			max);

	spin_unlock_irqrestore(&dev->lock, flags);
done:
	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return retval;
}



static void done(struct langwell_ep *ep, struct langwell_request *req,
		int status)
{
	struct langwell_udc	*dev = ep->dev;
	unsigned		stopped = ep->stopped;
	struct langwell_dtd	*curr_dtd, *next_dtd;
	int			i;

	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	
	list_del_init(&req->queue);

	if (req->req.status == -EINPROGRESS)
		req->req.status = status;
	else
		status = req->req.status;

	
	next_dtd = req->head;
	for (i = 0; i < req->dtd_count; i++) {
		curr_dtd = next_dtd;
		if (i != req->dtd_count - 1)
			next_dtd = curr_dtd->next_dtd_virt;
		dma_pool_free(dev->dtd_pool, curr_dtd, curr_dtd->dtd_dma);
	}

	usb_gadget_unmap_request(&dev->gadget, &req->req, is_in(ep));

	if (status != -ESHUTDOWN)
		dev_dbg(&dev->pdev->dev,
				"complete %s, req %p, stat %d, len %u/%u\n",
				ep->ep.name, &req->req, status,
				req->req.actual, req->req.length);

	
	ep->stopped = 1;

	spin_unlock(&dev->lock);
	
	if (req->req.complete)
		req->req.complete(&ep->ep, &req->req);

	spin_lock(&dev->lock);
	ep->stopped = stopped;

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}


static void langwell_ep_fifo_flush(struct usb_ep *_ep);

static void nuke(struct langwell_ep *ep, int status)
{
	
	ep->stopped = 1;

	
	if (&ep->ep && ep->desc)
		langwell_ep_fifo_flush(&ep->ep);

	while (!list_empty(&ep->queue)) {
		struct langwell_request	*req = NULL;
		req = list_entry(ep->queue.next, struct langwell_request,
				queue);
		done(ep, req, status);
	}
}



static int langwell_ep_disable(struct usb_ep *_ep)
{
	struct langwell_ep	*ep;
	unsigned long		flags;
	struct langwell_udc	*dev;
	int			ep_num;
	u32			endptctrl;

	ep = container_of(_ep, struct langwell_ep, ep);
	dev = ep->dev;
	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	if (!_ep || !ep->desc)
		return -EINVAL;

	spin_lock_irqsave(&dev->lock, flags);

	
	ep_num = ep->ep_num;
	endptctrl = readl(&dev->op_regs->endptctrl[ep_num]);
	if (is_in(ep))
		endptctrl &= ~EPCTRL_TXE;
	else
		endptctrl &= ~EPCTRL_RXE;
	writel(endptctrl, &dev->op_regs->endptctrl[ep_num]);

	
	nuke(ep, -ESHUTDOWN);

	ep->desc = NULL;
	ep->ep.desc = NULL;
	ep->stopped = 1;

	spin_unlock_irqrestore(&dev->lock, flags);

	dev_dbg(&dev->pdev->dev, "disabled %s\n", _ep->name);
	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);

	return 0;
}


static struct usb_request *langwell_alloc_request(struct usb_ep *_ep,
		gfp_t gfp_flags)
{
	struct langwell_ep	*ep;
	struct langwell_udc	*dev;
	struct langwell_request	*req = NULL;

	if (!_ep)
		return NULL;

	ep = container_of(_ep, struct langwell_ep, ep);
	dev = ep->dev;
	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	req = kzalloc(sizeof(*req), gfp_flags);
	if (!req)
		return NULL;

	req->req.dma = DMA_ADDR_INVALID;
	INIT_LIST_HEAD(&req->queue);

	dev_vdbg(&dev->pdev->dev, "alloc request for %s\n", _ep->name);
	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return &req->req;
}


static void langwell_free_request(struct usb_ep *_ep,
		struct usb_request *_req)
{
	struct langwell_ep	*ep;
	struct langwell_udc	*dev;
	struct langwell_request	*req = NULL;

	ep = container_of(_ep, struct langwell_ep, ep);
	dev = ep->dev;
	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	if (!_ep || !_req)
		return;

	req = container_of(_req, struct langwell_request, req);
	WARN_ON(!list_empty(&req->queue));

	if (_req)
		kfree(req);

	dev_vdbg(&dev->pdev->dev, "free request for %s\n", _ep->name);
	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}



static int queue_dtd(struct langwell_ep *ep, struct langwell_request *req)
{
	u32			bit_mask, usbcmd, endptstat, dtd_dma;
	u8			dtd_status;
	int			i;
	struct langwell_dqh	*dqh;
	struct langwell_udc	*dev;

	dev = ep->dev;
	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	i = ep->ep_num * 2 + is_in(ep);
	dqh = &dev->ep_dqh[i];

	if (ep->ep_num)
		dev_vdbg(&dev->pdev->dev, "%s\n", ep->name);
	else
		
		dev_vdbg(&dev->pdev->dev, "%s-%s\n", ep->name, DIR_STRING(ep));

	dev_vdbg(&dev->pdev->dev, "ep_dqh[%d] addr: 0x%p\n",
			i, &(dev->ep_dqh[i]));

	bit_mask = is_in(ep) ?
		(1 << (ep->ep_num + 16)) : (1 << (ep->ep_num));

	dev_vdbg(&dev->pdev->dev, "bit_mask = 0x%08x\n", bit_mask);

	
	if (!(list_empty(&ep->queue))) {
		
		struct langwell_request	*lastreq;
		lastreq = list_entry(ep->queue.prev,
				struct langwell_request, queue);

		lastreq->tail->dtd_next =
			cpu_to_le32(req->head->dtd_dma & DTD_NEXT_MASK);

		
		if (readl(&dev->op_regs->endptprime) & bit_mask)
			goto out;

		do {
			
			usbcmd = readl(&dev->op_regs->usbcmd);
			writel(usbcmd | CMD_ATDTW, &dev->op_regs->usbcmd);

			
			endptstat = readl(&dev->op_regs->endptstat) & bit_mask;

		} while (!(readl(&dev->op_regs->usbcmd) & CMD_ATDTW));

		
		usbcmd = readl(&dev->op_regs->usbcmd);
		writel(usbcmd & ~CMD_ATDTW, &dev->op_regs->usbcmd);

		if (endptstat)
			goto out;
	}

	
	dtd_dma = req->head->dtd_dma & DTD_NEXT_MASK;
	dqh->dtd_next = cpu_to_le32(dtd_dma);

	
	dtd_status = (u8) ~(DTD_STS_ACTIVE | DTD_STS_HALTED);
	dqh->dtd_status &= dtd_status;
	dev_vdbg(&dev->pdev->dev, "dqh->dtd_status = 0x%x\n", dqh->dtd_status);

	
	wmb();

	
	bit_mask = is_in(ep) ? (1 << (ep->ep_num + 16)) : (1 << ep->ep_num);
	dev_vdbg(&dev->pdev->dev, "endprime bit_mask = 0x%08x\n", bit_mask);
	writel(bit_mask, &dev->op_regs->endptprime);
out:
	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return 0;
}


static struct langwell_dtd *build_dtd(struct langwell_request *req,
		unsigned *length, dma_addr_t *dma, int *is_last)
{
	u32			 buf_ptr;
	struct langwell_dtd	*dtd;
	struct langwell_udc	*dev;
	int			i;

	dev = req->ep->dev;
	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	
	*length = min(req->req.length - req->req.actual,
			(unsigned)DTD_MAX_TRANSFER_LENGTH);

	
	dtd = dma_pool_alloc(dev->dtd_pool, GFP_KERNEL, dma);
	if (dtd == NULL)
		return dtd;
	dtd->dtd_dma = *dma;

	
	buf_ptr = (u32)(req->req.dma + req->req.actual);
	for (i = 0; i < 5; i++)
		dtd->dtd_buf[i] = cpu_to_le32(buf_ptr + i * PAGE_SIZE);

	req->req.actual += *length;

	
	dtd->dtd_total = cpu_to_le16(*length);
	dev_vdbg(&dev->pdev->dev, "dtd->dtd_total = %d\n", dtd->dtd_total);

	
	if (req->req.zero) {
		if (*length == 0 || (*length % req->ep->ep.maxpacket) != 0)
			*is_last = 1;
		else
			*is_last = 0;
	} else if (req->req.length == req->req.actual) {
		*is_last = 1;
	} else
		*is_last = 0;

	if (*is_last == 0)
		dev_vdbg(&dev->pdev->dev, "multi-dtd request!\n");

	
	if (*is_last && !req->req.no_interrupt)
		dtd->dtd_ioc = 1;

	
	dtd->dtd_multo = 0;

	
	dtd->dtd_status = DTD_STS_ACTIVE;
	dev_vdbg(&dev->pdev->dev, "dtd->dtd_status = 0x%02x\n",
			dtd->dtd_status);

	dev_vdbg(&dev->pdev->dev, "length = %d, dma addr= 0x%08x\n",
			*length, (int)*dma);
	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return dtd;
}


static int req_to_dtd(struct langwell_request *req)
{
	unsigned		count;
	int			is_last, is_first = 1;
	struct langwell_dtd	*dtd, *last_dtd = NULL;
	struct langwell_udc	*dev;
	dma_addr_t		dma;

	dev = req->ep->dev;
	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);
	do {
		dtd = build_dtd(req, &count, &dma, &is_last);
		if (dtd == NULL)
			return -ENOMEM;

		if (is_first) {
			is_first = 0;
			req->head = dtd;
		} else {
			last_dtd->dtd_next = cpu_to_le32(dma);
			last_dtd->next_dtd_virt = dtd;
		}
		last_dtd = dtd;
		req->dtd_count++;
	} while (!is_last);

	
	dtd->dtd_next = DTD_TERM;

	req->tail = dtd;

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return 0;
}


static int langwell_ep_queue(struct usb_ep *_ep, struct usb_request *_req,
		gfp_t gfp_flags)
{
	struct langwell_request	*req;
	struct langwell_ep	*ep;
	struct langwell_udc	*dev;
	unsigned long		flags;
	int			is_iso = 0;
	int			ret;

	
	req = container_of(_req, struct langwell_request, req);
	ep = container_of(_ep, struct langwell_ep, ep);

	if (!_req || !_req->complete || !_req->buf
			|| !list_empty(&req->queue)) {
		return -EINVAL;
	}

	if (unlikely(!_ep || !ep->desc))
		return -EINVAL;

	dev = ep->dev;
	req->ep = ep;
	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	if (usb_endpoint_xfer_isoc(ep->desc)) {
		if (req->req.length > ep->ep.maxpacket)
			return -EMSGSIZE;
		is_iso = 1;
	}

	if (unlikely(!dev->driver || dev->gadget.speed == USB_SPEED_UNKNOWN))
		return -ESHUTDOWN;

	
	ret = usb_gadget_map_request(&dev->gadget, &req->req, is_in(ep));
	if (ret)
		return ret;

	dev_dbg(&dev->pdev->dev,
			"%s queue req %p, len %u, buf %p, dma 0x%08x\n",
			_ep->name,
			_req, _req->length, _req->buf, (int)_req->dma);

	_req->status = -EINPROGRESS;
	_req->actual = 0;
	req->dtd_count = 0;

	spin_lock_irqsave(&dev->lock, flags);

	
	if (!req_to_dtd(req)) {
		queue_dtd(ep, req);
	} else {
		spin_unlock_irqrestore(&dev->lock, flags);
		return -ENOMEM;
	}

	
	if (ep->ep_num == 0)
		dev->ep0_state = DATA_STATE_XMIT;

	if (likely(req != NULL)) {
		list_add_tail(&req->queue, &ep->queue);
		dev_vdbg(&dev->pdev->dev, "list_add_tail()\n");
	}

	spin_unlock_irqrestore(&dev->lock, flags);

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return 0;
}


static int langwell_ep_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	struct langwell_ep	*ep;
	struct langwell_udc	*dev;
	struct langwell_request	*req;
	unsigned long		flags;
	int			stopped, ep_num, retval = 0;
	u32			endptctrl;

	ep = container_of(_ep, struct langwell_ep, ep);
	dev = ep->dev;
	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	if (!_ep || !ep->desc || !_req)
		return -EINVAL;

	if (!dev->driver)
		return -ESHUTDOWN;

	spin_lock_irqsave(&dev->lock, flags);
	stopped = ep->stopped;

	
	ep->stopped = 1;
	ep_num = ep->ep_num;

	
	endptctrl = readl(&dev->op_regs->endptctrl[ep_num]);
	if (is_in(ep))
		endptctrl &= ~EPCTRL_TXE;
	else
		endptctrl &= ~EPCTRL_RXE;
	writel(endptctrl, &dev->op_regs->endptctrl[ep_num]);

	
	list_for_each_entry(req, &ep->queue, queue) {
		if (&req->req == _req)
			break;
	}

	if (&req->req != _req) {
		retval = -EINVAL;
		goto done;
	}

	
	if (ep->queue.next == &req->queue) {
		dev_dbg(&dev->pdev->dev, "unlink (%s) dma\n", _ep->name);
		_req->status = -ECONNRESET;
		langwell_ep_fifo_flush(&ep->ep);

		
		if (likely(ep->queue.next == &req->queue)) {
			struct langwell_dqh	*dqh;
			struct langwell_request	*next_req;

			dqh = ep->dqh;
			next_req = list_entry(req->queue.next,
					struct langwell_request, queue);

			
			writel((u32) next_req->head, &dqh->dqh_current);
		}
	} else {
		struct langwell_request	*prev_req;

		prev_req = list_entry(req->queue.prev,
				struct langwell_request, queue);
		writel(readl(&req->tail->dtd_next),
				&prev_req->tail->dtd_next);
	}

	done(ep, req, -ECONNRESET);

done:
	
	endptctrl = readl(&dev->op_regs->endptctrl[ep_num]);
	if (is_in(ep))
		endptctrl |= EPCTRL_TXE;
	else
		endptctrl |= EPCTRL_RXE;
	writel(endptctrl, &dev->op_regs->endptctrl[ep_num]);

	ep->stopped = stopped;
	spin_unlock_irqrestore(&dev->lock, flags);

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return retval;
}



static void ep_set_halt(struct langwell_ep *ep, int value)
{
	u32			endptctrl = 0;
	int			ep_num;
	struct langwell_udc	*dev = ep->dev;
	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	ep_num = ep->ep_num;
	endptctrl = readl(&dev->op_regs->endptctrl[ep_num]);

	
	if (value) {
		
		if (is_in(ep))
			endptctrl |= EPCTRL_TXS;
		else
			endptctrl |= EPCTRL_RXS;
	} else {
		
		if (is_in(ep)) {
			endptctrl &= ~EPCTRL_TXS;
			endptctrl |= EPCTRL_TXR;
		} else {
			endptctrl &= ~EPCTRL_RXS;
			endptctrl |= EPCTRL_RXR;
		}
	}

	writel(endptctrl, &dev->op_regs->endptctrl[ep_num]);

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}


static int langwell_ep_set_halt(struct usb_ep *_ep, int value)
{
	struct langwell_ep	*ep;
	struct langwell_udc	*dev;
	unsigned long		flags;
	int			retval = 0;

	ep = container_of(_ep, struct langwell_ep, ep);
	dev = ep->dev;

	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	if (!_ep || !ep->desc)
		return -EINVAL;

	if (!dev->driver || dev->gadget.speed == USB_SPEED_UNKNOWN)
		return -ESHUTDOWN;

	if (usb_endpoint_xfer_isoc(ep->desc))
		return  -EOPNOTSUPP;

	spin_lock_irqsave(&dev->lock, flags);

	if (!list_empty(&ep->queue) && is_in(ep) && value) {
		
		dev_dbg(&dev->pdev->dev, "%s FIFO holds bytes\n", _ep->name);
		retval = -EAGAIN;
		goto done;
	}

	
	if (ep->ep_num) {
		ep_set_halt(ep, value);
	} else { 
		dev->ep0_state = WAIT_FOR_SETUP;
		dev->ep0_dir = USB_DIR_OUT;
	}
done:
	spin_unlock_irqrestore(&dev->lock, flags);
	dev_dbg(&dev->pdev->dev, "%s %s halt\n",
			_ep->name, value ? "set" : "clear");
	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return retval;
}


static int langwell_ep_set_wedge(struct usb_ep *_ep)
{
	struct langwell_ep	*ep;
	struct langwell_udc	*dev;

	ep = container_of(_ep, struct langwell_ep, ep);
	dev = ep->dev;

	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	if (!_ep || !ep->desc)
		return -EINVAL;

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return usb_ep_set_halt(_ep);
}


static void langwell_ep_fifo_flush(struct usb_ep *_ep)
{
	struct langwell_ep	*ep;
	struct langwell_udc	*dev;
	u32			flush_bit;
	unsigned long		timeout;

	ep = container_of(_ep, struct langwell_ep, ep);
	dev = ep->dev;

	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	if (!_ep || !ep->desc) {
		dev_vdbg(&dev->pdev->dev, "ep or ep->desc is NULL\n");
		dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
		return;
	}

	dev_vdbg(&dev->pdev->dev, "%s-%s fifo flush\n",
			_ep->name, DIR_STRING(ep));

	
	if (ep->ep_num == 0)
		flush_bit = (1 << 16) | 1;
	else if (is_in(ep))
		flush_bit = 1 << (ep->ep_num + 16);	
	else
		flush_bit = 1 << ep->ep_num;		

	
	timeout = jiffies + FLUSH_TIMEOUT;
	do {
		writel(flush_bit, &dev->op_regs->endptflush);
		while (readl(&dev->op_regs->endptflush)) {
			if (time_after(jiffies, timeout)) {
				dev_err(&dev->pdev->dev, "ep flush timeout\n");
				goto done;
			}
			cpu_relax();
		}
	} while (readl(&dev->op_regs->endptstat) & flush_bit);
done:
	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}


static const struct usb_ep_ops langwell_ep_ops = {

	
	.enable		= langwell_ep_enable,

	
	.disable	= langwell_ep_disable,

	
	.alloc_request	= langwell_alloc_request,

	
	.free_request	= langwell_free_request,

	
	.queue		= langwell_ep_queue,

	
	.dequeue	= langwell_ep_dequeue,

	
	.set_halt	= langwell_ep_set_halt,

	
	.set_wedge	= langwell_ep_set_wedge,

	
	.fifo_flush	= langwell_ep_fifo_flush,
};




static int langwell_get_frame(struct usb_gadget *_gadget)
{
	struct langwell_udc	*dev;
	u16			retval;

	if (!_gadget)
		return -ENODEV;

	dev = container_of(_gadget, struct langwell_udc, gadget);
	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	retval = readl(&dev->op_regs->frindex) & FRINDEX_MASK;

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return retval;
}


static void langwell_phy_low_power(struct langwell_udc *dev, bool flag)
{
	u32		devlc;
	u8		devlc_byte2;
	dev_dbg(&dev->pdev->dev, "---> %s()\n", __func__);

	devlc = readl(&dev->op_regs->devlc);
	dev_vdbg(&dev->pdev->dev, "devlc = 0x%08x\n", devlc);

	if (flag)
		devlc |= LPM_PHCD;
	else
		devlc &= ~LPM_PHCD;

	
	devlc_byte2 = (devlc >> 16) & 0xff;
	writeb(devlc_byte2, (u8 *)&dev->op_regs->devlc + 2);

	devlc = readl(&dev->op_regs->devlc);
	dev_vdbg(&dev->pdev->dev,
			"%s PHY low power suspend, devlc = 0x%08x\n",
			flag ? "enter" : "exit", devlc);
}


static int langwell_wakeup(struct usb_gadget *_gadget)
{
	struct langwell_udc	*dev;
	u32			portsc1;
	unsigned long		flags;

	if (!_gadget)
		return 0;

	dev = container_of(_gadget, struct langwell_udc, gadget);
	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	
	if (!dev->remote_wakeup) {
		dev_info(&dev->pdev->dev, "remote wakeup is disabled\n");
		return -ENOTSUPP;
	}

	spin_lock_irqsave(&dev->lock, flags);

	portsc1 = readl(&dev->op_regs->portsc1);
	if (!(portsc1 & PORTS_SUSP)) {
		spin_unlock_irqrestore(&dev->lock, flags);
		return 0;
	}

	
	if (dev->lpm && dev->lpm_state == LPM_L1)
		dev_info(&dev->pdev->dev, "LPM L1 to L0 remote wakeup\n");
	else
		dev_info(&dev->pdev->dev, "device remote wakeup\n");

	
	if (dev->pdev->device != 0x0829)
		langwell_phy_low_power(dev, 0);

	
	portsc1 |= PORTS_FPR;
	writel(portsc1, &dev->op_regs->portsc1);

	spin_unlock_irqrestore(&dev->lock, flags);

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return 0;
}


static int langwell_vbus_session(struct usb_gadget *_gadget, int is_active)
{
	struct langwell_udc	*dev;
	unsigned long		flags;
	u32			usbcmd;

	if (!_gadget)
		return -ENODEV;

	dev = container_of(_gadget, struct langwell_udc, gadget);
	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	spin_lock_irqsave(&dev->lock, flags);
	dev_vdbg(&dev->pdev->dev, "VBUS status: %s\n",
			is_active ? "on" : "off");

	dev->vbus_active = (is_active != 0);
	if (dev->driver && dev->softconnected && dev->vbus_active) {
		usbcmd = readl(&dev->op_regs->usbcmd);
		usbcmd |= CMD_RUNSTOP;
		writel(usbcmd, &dev->op_regs->usbcmd);
	} else {
		usbcmd = readl(&dev->op_regs->usbcmd);
		usbcmd &= ~CMD_RUNSTOP;
		writel(usbcmd, &dev->op_regs->usbcmd);
	}

	spin_unlock_irqrestore(&dev->lock, flags);

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return 0;
}


static int langwell_vbus_draw(struct usb_gadget *_gadget, unsigned mA)
{
	struct langwell_udc	*dev;

	if (!_gadget)
		return -ENODEV;

	dev = container_of(_gadget, struct langwell_udc, gadget);
	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	if (dev->transceiver) {
		dev_vdbg(&dev->pdev->dev, "usb_phy_set_power\n");
		dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
		return usb_phy_set_power(dev->transceiver, mA);
	}

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return -ENOTSUPP;
}


static int langwell_pullup(struct usb_gadget *_gadget, int is_on)
{
	struct langwell_udc	*dev;
	u32			usbcmd;
	unsigned long		flags;

	if (!_gadget)
		return -ENODEV;

	dev = container_of(_gadget, struct langwell_udc, gadget);

	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	spin_lock_irqsave(&dev->lock, flags);
	dev->softconnected = (is_on != 0);

	if (dev->driver && dev->softconnected && dev->vbus_active) {
		usbcmd = readl(&dev->op_regs->usbcmd);
		usbcmd |= CMD_RUNSTOP;
		writel(usbcmd, &dev->op_regs->usbcmd);
	} else {
		usbcmd = readl(&dev->op_regs->usbcmd);
		usbcmd &= ~CMD_RUNSTOP;
		writel(usbcmd, &dev->op_regs->usbcmd);
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return 0;
}

static int langwell_start(struct usb_gadget *g,
		struct usb_gadget_driver *driver);

static int langwell_stop(struct usb_gadget *g,
		struct usb_gadget_driver *driver);

static const struct usb_gadget_ops langwell_ops = {

	
	.get_frame	= langwell_get_frame,

	
	.wakeup		= langwell_wakeup,

	
	

	
	.vbus_session	= langwell_vbus_session,

	
	.vbus_draw	= langwell_vbus_draw,

	
	.pullup		= langwell_pullup,

	.udc_start	= langwell_start,
	.udc_stop	= langwell_stop,
};




static int langwell_udc_reset(struct langwell_udc *dev)
{
	u32		usbcmd, usbmode, devlc, endpointlistaddr;
	u8		devlc_byte0, devlc_byte2;
	unsigned long	timeout;

	if (!dev)
		return -EINVAL;

	dev_dbg(&dev->pdev->dev, "---> %s()\n", __func__);

	
	usbcmd = readl(&dev->op_regs->usbcmd);
	usbcmd &= ~CMD_RUNSTOP;
	writel(usbcmd, &dev->op_regs->usbcmd);

	
	usbcmd = readl(&dev->op_regs->usbcmd);
	usbcmd |= CMD_RST;
	writel(usbcmd, &dev->op_regs->usbcmd);

	
	timeout = jiffies + RESET_TIMEOUT;
	while (readl(&dev->op_regs->usbcmd) & CMD_RST) {
		if (time_after(jiffies, timeout)) {
			dev_err(&dev->pdev->dev, "device reset timeout\n");
			return -ETIMEDOUT;
		}
		cpu_relax();
	}

	
	usbmode = readl(&dev->op_regs->usbmode);
	usbmode |= MODE_DEVICE;

	
	usbmode |= MODE_SLOM;

	writel(usbmode, &dev->op_regs->usbmode);
	usbmode = readl(&dev->op_regs->usbmode);
	dev_vdbg(&dev->pdev->dev, "usbmode=0x%08x\n", usbmode);

	
	writel(0, &dev->op_regs->usbsts);

	
	if (dev->lpm) {
		devlc = readl(&dev->op_regs->devlc);
		dev_vdbg(&dev->pdev->dev, "devlc = 0x%08x\n", devlc);
		
		devlc &= ~LPM_STL;	
		devlc &= ~LPM_NYT_ACK;	
		devlc_byte0 = devlc & 0xff;
		devlc_byte2 = (devlc >> 16) & 0xff;
		writeb(devlc_byte0, (u8 *)&dev->op_regs->devlc);
		writeb(devlc_byte2, (u8 *)&dev->op_regs->devlc + 2);
		devlc = readl(&dev->op_regs->devlc);
		dev_vdbg(&dev->pdev->dev,
				"ACK LPM token, devlc = 0x%08x\n", devlc);
	}

	
	endpointlistaddr = dev->ep_dqh_dma;
	endpointlistaddr &= ENDPOINTLISTADDR_MASK;
	writel(endpointlistaddr, &dev->op_regs->endpointlistaddr);

	dev_vdbg(&dev->pdev->dev,
		"dQH base (vir: %p, phy: 0x%08x), endpointlistaddr=0x%08x\n",
		dev->ep_dqh, endpointlistaddr,
		readl(&dev->op_regs->endpointlistaddr));
	dev_dbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return 0;
}


static int eps_reinit(struct langwell_udc *dev)
{
	struct langwell_ep	*ep;
	char			name[14];
	int			i;

	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	
	ep = &dev->ep[0];
	ep->dev = dev;
	strncpy(ep->name, "ep0", sizeof(ep->name));
	ep->ep.name = ep->name;
	ep->ep.ops = &langwell_ep_ops;
	ep->stopped = 0;
	ep->ep.maxpacket = EP0_MAX_PKT_SIZE;
	ep->ep_num = 0;
	ep->desc = &langwell_ep0_desc;
	INIT_LIST_HEAD(&ep->queue);

	ep->ep_type = USB_ENDPOINT_XFER_CONTROL;

	
	for (i = 2; i < dev->ep_max; i++) {
		ep = &dev->ep[i];
		if (i % 2)
			snprintf(name, sizeof(name), "ep%din", i / 2);
		else
			snprintf(name, sizeof(name), "ep%dout", i / 2);
		ep->dev = dev;
		strncpy(ep->name, name, sizeof(ep->name));
		ep->ep.name = ep->name;

		ep->ep.ops = &langwell_ep_ops;
		ep->stopped = 0;
		ep->ep.maxpacket = (unsigned short) ~0;
		ep->ep_num = i / 2;

		INIT_LIST_HEAD(&ep->queue);
		list_add_tail(&ep->ep.ep_list, &dev->gadget.ep_list);
	}

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return 0;
}


static void langwell_udc_start(struct langwell_udc *dev)
{
	u32	usbintr, usbcmd;
	dev_dbg(&dev->pdev->dev, "---> %s()\n", __func__);

	
	usbintr = INTR_ULPIE	
		| INTR_SLE	
		
		| INTR_URE	
		| INTR_AAE	
		| INTR_SEE	
		| INTR_FRE	
		| INTR_PCE	
		| INTR_UEE	
		| INTR_UE;	
	writel(usbintr, &dev->op_regs->usbintr);

	
	dev->stopped = 0;

	
	usbcmd = readl(&dev->op_regs->usbcmd);
	usbcmd |= CMD_RUNSTOP;
	writel(usbcmd, &dev->op_regs->usbcmd);

	dev_dbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}


static void langwell_udc_stop(struct langwell_udc *dev)
{
	u32	usbcmd;

	dev_dbg(&dev->pdev->dev, "---> %s()\n", __func__);

	
	writel(0, &dev->op_regs->usbintr);

	
	dev->stopped = 1;

	
	usbcmd = readl(&dev->op_regs->usbcmd);
	usbcmd &= ~CMD_RUNSTOP;
	writel(usbcmd, &dev->op_regs->usbcmd);

	dev_dbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}


static void stop_activity(struct langwell_udc *dev)
{
	struct langwell_ep	*ep;
	dev_dbg(&dev->pdev->dev, "---> %s()\n", __func__);

	nuke(&dev->ep[0], -ESHUTDOWN);

	list_for_each_entry(ep, &dev->gadget.ep_list, ep.ep_list) {
		nuke(ep, -ESHUTDOWN);
	}

	
	if (dev->driver) {
		spin_unlock(&dev->lock);
		dev->driver->disconnect(&dev->gadget);
		spin_lock(&dev->lock);
	}

	dev_dbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}



static ssize_t show_function(struct device *_dev,
		struct device_attribute *attr, char *buf)
{
	struct langwell_udc	*dev = dev_get_drvdata(_dev);

	if (!dev->driver || !dev->driver->function
			|| strlen(dev->driver->function) > PAGE_SIZE)
		return 0;

	return scnprintf(buf, PAGE_SIZE, "%s\n", dev->driver->function);
}
static DEVICE_ATTR(function, S_IRUGO, show_function, NULL);


static inline enum usb_device_speed lpm_device_speed(u32 reg)
{
	switch (LPM_PSPD(reg)) {
	case LPM_SPEED_HIGH:
		return USB_SPEED_HIGH;
	case LPM_SPEED_FULL:
		return USB_SPEED_FULL;
	case LPM_SPEED_LOW:
		return USB_SPEED_LOW;
	default:
		return USB_SPEED_UNKNOWN;
	}
}

static ssize_t show_langwell_udc(struct device *_dev,
		struct device_attribute *attr, char *buf)
{
	struct langwell_udc	*dev = dev_get_drvdata(_dev);
	struct langwell_request *req;
	struct langwell_ep	*ep = NULL;
	char			*next;
	unsigned		size;
	unsigned		t;
	unsigned		i;
	unsigned long		flags;
	u32			tmp_reg;

	next = buf;
	size = PAGE_SIZE;
	spin_lock_irqsave(&dev->lock, flags);

	
	t = scnprintf(next, size,
			DRIVER_DESC "\n"
			"%s version: %s\n"
			"Gadget driver: %s\n\n",
			driver_name, DRIVER_VERSION,
			dev->driver ? dev->driver->driver.name : "(none)");
	size -= t;
	next += t;

	
	tmp_reg = readl(&dev->op_regs->usbcmd);
	t = scnprintf(next, size,
			"USBCMD reg:\n"
			"SetupTW: %d\n"
			"Run/Stop: %s\n\n",
			(tmp_reg & CMD_SUTW) ? 1 : 0,
			(tmp_reg & CMD_RUNSTOP) ? "Run" : "Stop");
	size -= t;
	next += t;

	tmp_reg = readl(&dev->op_regs->usbsts);
	t = scnprintf(next, size,
			"USB Status Reg:\n"
			"Device Suspend: %d\n"
			"Reset Received: %d\n"
			"System Error: %s\n"
			"USB Error Interrupt: %s\n\n",
			(tmp_reg & STS_SLI) ? 1 : 0,
			(tmp_reg & STS_URI) ? 1 : 0,
			(tmp_reg & STS_SEI) ? "Error" : "No error",
			(tmp_reg & STS_UEI) ? "Error detected" : "No error");
	size -= t;
	next += t;

	tmp_reg = readl(&dev->op_regs->usbintr);
	t = scnprintf(next, size,
			"USB Intrrupt Enable Reg:\n"
			"Sleep Enable: %d\n"
			"SOF Received Enable: %d\n"
			"Reset Enable: %d\n"
			"System Error Enable: %d\n"
			"Port Change Dectected Enable: %d\n"
			"USB Error Intr Enable: %d\n"
			"USB Intr Enable: %d\n\n",
			(tmp_reg & INTR_SLE) ? 1 : 0,
			(tmp_reg & INTR_SRE) ? 1 : 0,
			(tmp_reg & INTR_URE) ? 1 : 0,
			(tmp_reg & INTR_SEE) ? 1 : 0,
			(tmp_reg & INTR_PCE) ? 1 : 0,
			(tmp_reg & INTR_UEE) ? 1 : 0,
			(tmp_reg & INTR_UE) ? 1 : 0);
	size -= t;
	next += t;

	tmp_reg = readl(&dev->op_regs->frindex);
	t = scnprintf(next, size,
			"USB Frame Index Reg:\n"
			"Frame Number is 0x%08x\n\n",
			(tmp_reg & FRINDEX_MASK));
	size -= t;
	next += t;

	tmp_reg = readl(&dev->op_regs->deviceaddr);
	t = scnprintf(next, size,
			"USB Device Address Reg:\n"
			"Device Addr is 0x%x\n\n",
			USBADR(tmp_reg));
	size -= t;
	next += t;

	tmp_reg = readl(&dev->op_regs->endpointlistaddr);
	t = scnprintf(next, size,
			"USB Endpoint List Address Reg:\n"
			"Endpoint List Pointer is 0x%x\n\n",
			EPBASE(tmp_reg));
	size -= t;
	next += t;

	tmp_reg = readl(&dev->op_regs->portsc1);
	t = scnprintf(next, size,
		"USB Port Status & Control Reg:\n"
		"Port Reset: %s\n"
		"Port Suspend Mode: %s\n"
		"Over-current Change: %s\n"
		"Port Enable/Disable Change: %s\n"
		"Port Enabled/Disabled: %s\n"
		"Current Connect Status: %s\n"
		"LPM Suspend Status: %s\n\n",
		(tmp_reg & PORTS_PR) ? "Reset" : "Not Reset",
		(tmp_reg & PORTS_SUSP) ? "Suspend " : "Not Suspend",
		(tmp_reg & PORTS_OCC) ? "Detected" : "No",
		(tmp_reg & PORTS_PEC) ? "Changed" : "Not Changed",
		(tmp_reg & PORTS_PE) ? "Enable" : "Not Correct",
		(tmp_reg & PORTS_CCS) ?  "Attached" : "Not Attached",
		(tmp_reg & PORTS_SLP) ? "LPM L1" : "LPM L0");
	size -= t;
	next += t;

	tmp_reg = readl(&dev->op_regs->devlc);
	t = scnprintf(next, size,
		"Device LPM Control Reg:\n"
		"Parallel Transceiver : %d\n"
		"Serial Transceiver : %d\n"
		"Port Speed: %s\n"
		"Port Force Full Speed Connenct: %s\n"
		"PHY Low Power Suspend Clock: %s\n"
		"BmAttributes: %d\n\n",
		LPM_PTS(tmp_reg),
		(tmp_reg & LPM_STS) ? 1 : 0,
		usb_speed_string(lpm_device_speed(tmp_reg)),
		(tmp_reg & LPM_PFSC) ? "Force Full Speed" : "Not Force",
		(tmp_reg & LPM_PHCD) ? "Disabled" : "Enabled",
		LPM_BA(tmp_reg));
	size -= t;
	next += t;

	tmp_reg = readl(&dev->op_regs->usbmode);
	t = scnprintf(next, size,
			"USB Mode Reg:\n"
			"Controller Mode is : %s\n\n", ({
				char *s;
				switch (MODE_CM(tmp_reg)) {
				case MODE_IDLE:
					s = "Idle"; break;
				case MODE_DEVICE:
					s = "Device Controller"; break;
				case MODE_HOST:
					s = "Host Controller"; break;
				default:
					s = "None"; break;
				}
				s;
			}));
	size -= t;
	next += t;

	tmp_reg = readl(&dev->op_regs->endptsetupstat);
	t = scnprintf(next, size,
			"Endpoint Setup Status Reg:\n"
			"SETUP on ep 0x%04x\n\n",
			tmp_reg & SETUPSTAT_MASK);
	size -= t;
	next += t;

	for (i = 0; i < dev->ep_max / 2; i++) {
		tmp_reg = readl(&dev->op_regs->endptctrl[i]);
		t = scnprintf(next, size, "EP Ctrl Reg [%d]: 0x%08x\n",
				i, tmp_reg);
		size -= t;
		next += t;
	}
	tmp_reg = readl(&dev->op_regs->endptprime);
	t = scnprintf(next, size, "EP Prime Reg: 0x%08x\n\n", tmp_reg);
	size -= t;
	next += t;

	
	ep = &dev->ep[0];
	t = scnprintf(next, size, "%s MaxPacketSize: 0x%x, ep_num: %d\n",
			ep->ep.name, ep->ep.maxpacket, ep->ep_num);
	size -= t;
	next += t;

	if (list_empty(&ep->queue)) {
		t = scnprintf(next, size, "its req queue is empty\n\n");
		size -= t;
		next += t;
	} else {
		list_for_each_entry(req, &ep->queue, queue) {
			t = scnprintf(next, size,
				"req %p actual 0x%x length 0x%x  buf %p\n",
				&req->req, req->req.actual,
				req->req.length, req->req.buf);
			size -= t;
			next += t;
		}
	}
	
	list_for_each_entry(ep, &dev->gadget.ep_list, ep.ep_list) {
		if (ep->desc) {
			t = scnprintf(next, size,
					"\n%s MaxPacketSize: 0x%x, "
					"ep_num: %d\n",
					ep->ep.name, ep->ep.maxpacket,
					ep->ep_num);
			size -= t;
			next += t;

			if (list_empty(&ep->queue)) {
				t = scnprintf(next, size,
						"its req queue is empty\n\n");
				size -= t;
				next += t;
			} else {
				list_for_each_entry(req, &ep->queue, queue) {
					t = scnprintf(next, size,
						"req %p actual 0x%x length "
						"0x%x  buf %p\n",
						&req->req, req->req.actual,
						req->req.length, req->req.buf);
					size -= t;
					next += t;
				}
			}
		}
	}

	spin_unlock_irqrestore(&dev->lock, flags);
	return PAGE_SIZE - size;
}
static DEVICE_ATTR(langwell_udc, S_IRUGO, show_langwell_udc, NULL);


static ssize_t store_remote_wakeup(struct device *_dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct langwell_udc	*dev = dev_get_drvdata(_dev);
	unsigned long		flags;
	ssize_t			rc = count;

	if (count > 2)
		return -EINVAL;

	if (count > 0 && buf[count-1] == '\n')
		((char *) buf)[count-1] = 0;

	if (buf[0] != '1')
		return -EINVAL;

	
	spin_lock_irqsave(&dev->lock, flags);
	dev->remote_wakeup = 1;
	dev->dev_status |= (1 << USB_DEVICE_REMOTE_WAKEUP);
	spin_unlock_irqrestore(&dev->lock, flags);

	langwell_wakeup(&dev->gadget);

	return rc;
}
static DEVICE_ATTR(remote_wakeup, S_IWUSR, NULL, store_remote_wakeup);




static int langwell_start(struct usb_gadget *g,
		struct usb_gadget_driver *driver)
{
	struct langwell_udc	*dev = gadget_to_langwell(g);
	unsigned long		flags;
	int			retval;

	dev_dbg(&dev->pdev->dev, "---> %s()\n", __func__);

	spin_lock_irqsave(&dev->lock, flags);

	
	driver->driver.bus = NULL;
	dev->driver = driver;
	dev->gadget.dev.driver = &driver->driver;

	spin_unlock_irqrestore(&dev->lock, flags);

	retval = device_create_file(&dev->pdev->dev, &dev_attr_function);
	if (retval)
		goto err;

	dev->usb_state = USB_STATE_ATTACHED;
	dev->ep0_state = WAIT_FOR_SETUP;
	dev->ep0_dir = USB_DIR_OUT;

	
	if (dev->got_irq)
		langwell_udc_start(dev);

	dev_vdbg(&dev->pdev->dev,
			"After langwell_udc_start(), print all registers:\n");
	print_all_registers(dev);

	dev_info(&dev->pdev->dev, "register driver: %s\n",
			driver->driver.name);
	dev_dbg(&dev->pdev->dev, "<--- %s()\n", __func__);

	return 0;

err:
	dev->gadget.dev.driver = NULL;
	dev->driver = NULL;

	dev_dbg(&dev->pdev->dev, "<--- %s()\n", __func__);

	return retval;
}

static int langwell_stop(struct usb_gadget *g,
		struct usb_gadget_driver *driver)
{
	struct langwell_udc	*dev = gadget_to_langwell(g);
	unsigned long		flags;

	dev_dbg(&dev->pdev->dev, "---> %s()\n", __func__);

	
	if (dev->pdev->device != 0x0829)
		langwell_phy_low_power(dev, 0);

	
	if (dev->transceiver)
		(void)otg_set_peripheral(dev->transceiver->otg, 0);

	
	langwell_udc_stop(dev);

	dev->usb_state = USB_STATE_ATTACHED;
	dev->ep0_state = WAIT_FOR_SETUP;
	dev->ep0_dir = USB_DIR_OUT;

	spin_lock_irqsave(&dev->lock, flags);

	
	dev->gadget.speed = USB_SPEED_UNKNOWN;
	dev->gadget.dev.driver = NULL;
	dev->driver = NULL;
	stop_activity(dev);
	spin_unlock_irqrestore(&dev->lock, flags);

	device_remove_file(&dev->pdev->dev, &dev_attr_function);

	dev_info(&dev->pdev->dev, "unregistered driver '%s'\n",
			driver->driver.name);
	dev_dbg(&dev->pdev->dev, "<--- %s()\n", __func__);

	return 0;
}


static void setup_tripwire(struct langwell_udc *dev)
{
	u32			usbcmd,
				endptsetupstat;
	unsigned long		timeout;
	struct langwell_dqh	*dqh;

	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	
	dqh = &dev->ep_dqh[EP_DIR_OUT];

	
	endptsetupstat = readl(&dev->op_regs->endptsetupstat);
	writel(endptsetupstat, &dev->op_regs->endptsetupstat);

	
	timeout = jiffies + SETUPSTAT_TIMEOUT;
	while (readl(&dev->op_regs->endptsetupstat)) {
		if (time_after(jiffies, timeout)) {
			dev_err(&dev->pdev->dev, "setup_tripwire timeout\n");
			break;
		}
		cpu_relax();
	}

	
	do {
		
		usbcmd = readl(&dev->op_regs->usbcmd);
		writel(usbcmd | CMD_SUTW, &dev->op_regs->usbcmd);

		
		memcpy(&dev->local_setup_buff, &dqh->dqh_setup, 8);
	} while (!(readl(&dev->op_regs->usbcmd) & CMD_SUTW));

	
	usbcmd = readl(&dev->op_regs->usbcmd);
	writel(usbcmd & ~CMD_SUTW, &dev->op_regs->usbcmd);

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}


static void ep0_stall(struct langwell_udc *dev)
{
	u32	endptctrl;

	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	
	endptctrl = readl(&dev->op_regs->endptctrl[0]);
	endptctrl |= EPCTRL_TXS | EPCTRL_RXS;
	writel(endptctrl, &dev->op_regs->endptctrl[0]);

	
	dev->ep0_state = WAIT_FOR_SETUP;
	dev->ep0_dir = USB_DIR_OUT;

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}


static int prime_status_phase(struct langwell_udc *dev, int dir)
{
	struct langwell_request	*req;
	struct langwell_ep	*ep;
	int			status = 0;

	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	if (dir == EP_DIR_IN)
		dev->ep0_dir = USB_DIR_IN;
	else
		dev->ep0_dir = USB_DIR_OUT;

	ep = &dev->ep[0];
	dev->ep0_state = WAIT_FOR_OUT_STATUS;

	req = dev->status_req;

	req->ep = ep;
	req->req.length = 0;
	req->req.status = -EINPROGRESS;
	req->req.actual = 0;
	req->req.complete = NULL;
	req->dtd_count = 0;

	if (!req_to_dtd(req))
		status = queue_dtd(ep, req);
	else
		return -ENOMEM;

	if (status)
		dev_err(&dev->pdev->dev, "can't queue ep0 status request\n");

	list_add_tail(&req->queue, &ep->queue);

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return status;
}


static void set_address(struct langwell_udc *dev, u16 value,
		u16 index, u16 length)
{
	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	
	dev->dev_addr = (u8) value;
	dev_vdbg(&dev->pdev->dev, "dev->dev_addr = %d\n", dev->dev_addr);

	
	dev->usb_state = USB_STATE_ADDRESS;

	
	if (prime_status_phase(dev, EP_DIR_IN))
		ep0_stall(dev);

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}


static struct langwell_ep *get_ep_by_windex(struct langwell_udc *dev,
		u16 wIndex)
{
	struct langwell_ep		*ep;
	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	if ((wIndex & USB_ENDPOINT_NUMBER_MASK) == 0)
		return &dev->ep[0];

	list_for_each_entry(ep, &dev->gadget.ep_list, ep.ep_list) {
		u8	bEndpointAddress;
		if (!ep->desc)
			continue;

		bEndpointAddress = ep->desc->bEndpointAddress;
		if ((wIndex ^ bEndpointAddress) & USB_DIR_IN)
			continue;

		if ((wIndex & USB_ENDPOINT_NUMBER_MASK)
			== (bEndpointAddress & USB_ENDPOINT_NUMBER_MASK))
			return ep;
	}

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return NULL;
}


static int ep_is_stall(struct langwell_ep *ep)
{
	struct langwell_udc	*dev = ep->dev;
	u32			endptctrl;
	int			retval;

	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	endptctrl = readl(&dev->op_regs->endptctrl[ep->ep_num]);
	if (is_in(ep))
		retval = endptctrl & EPCTRL_TXS ? 1 : 0;
	else
		retval = endptctrl & EPCTRL_RXS ? 1 : 0;

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return retval;
}


static void get_status(struct langwell_udc *dev, u8 request_type, u16 value,
		u16 index, u16 length)
{
	struct langwell_request	*req;
	struct langwell_ep	*ep;
	u16	status_data = 0;	
	int	status = 0;

	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	ep = &dev->ep[0];

	if ((request_type & USB_RECIP_MASK) == USB_RECIP_DEVICE) {
		
		status_data = dev->dev_status;
	} else if ((request_type & USB_RECIP_MASK) == USB_RECIP_INTERFACE) {
		
		status_data = 0;
	} else if ((request_type & USB_RECIP_MASK) == USB_RECIP_ENDPOINT) {
		
		struct langwell_ep	*epn;
		epn = get_ep_by_windex(dev, index);
		
		if (!epn)
			goto stall;

		status_data = ep_is_stall(epn) << USB_ENDPOINT_HALT;
	}

	dev_dbg(&dev->pdev->dev, "get status data: 0x%04x\n", status_data);

	dev->ep0_dir = USB_DIR_IN;

	
	req = dev->status_req;

	
	*((u16 *) req->req.buf) = cpu_to_le16(status_data);
	req->ep = ep;
	req->req.length = 2;
	req->req.status = -EINPROGRESS;
	req->req.actual = 0;
	req->req.complete = NULL;
	req->dtd_count = 0;

	
	if (!req_to_dtd(req))
		status = queue_dtd(ep, req);
	else			
		goto stall;

	if (status) {
		dev_err(&dev->pdev->dev,
				"response error on GET_STATUS request\n");
		goto stall;
	}

	list_add_tail(&req->queue, &ep->queue);
	dev->ep0_state = DATA_STATE_XMIT;

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return;
stall:
	ep0_stall(dev);
	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}


static void handle_setup_packet(struct langwell_udc *dev,
		struct usb_ctrlrequest *setup)
{
	u16	wValue = le16_to_cpu(setup->wValue);
	u16	wIndex = le16_to_cpu(setup->wIndex);
	u16	wLength = le16_to_cpu(setup->wLength);
	u32	portsc1;

	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	
	nuke(&dev->ep[0], -ESHUTDOWN);

	dev_dbg(&dev->pdev->dev, "SETUP %02x.%02x v%04x i%04x l%04x\n",
			setup->bRequestType, setup->bRequest,
			wValue, wIndex, wLength);

	
	if ((setup->bRequestType == 0x21) && (setup->bRequest == 0x00)) {
		
		goto delegate;
	}

	
	if ((setup->bRequestType == 0xa1) && (setup->bRequest == 0x01)) {
		
		goto delegate;
	}

	
	switch (setup->bRequest) {
	case USB_REQ_GET_STATUS:
		dev_dbg(&dev->pdev->dev, "SETUP: USB_REQ_GET_STATUS\n");
		
		if ((setup->bRequestType & (USB_DIR_IN | USB_TYPE_MASK))
					!= (USB_DIR_IN | USB_TYPE_STANDARD))
			break;
		get_status(dev, setup->bRequestType, wValue, wIndex, wLength);
		goto end;

	case USB_REQ_SET_ADDRESS:
		dev_dbg(&dev->pdev->dev, "SETUP: USB_REQ_SET_ADDRESS\n");
		
		if (setup->bRequestType != (USB_DIR_OUT | USB_TYPE_STANDARD
						| USB_RECIP_DEVICE))
			break;
		set_address(dev, wValue, wIndex, wLength);
		goto end;

	case USB_REQ_CLEAR_FEATURE:
	case USB_REQ_SET_FEATURE:
		
	{
		int rc = -EOPNOTSUPP;
		if (setup->bRequest == USB_REQ_SET_FEATURE)
			dev_dbg(&dev->pdev->dev,
					"SETUP: USB_REQ_SET_FEATURE\n");
		else if (setup->bRequest == USB_REQ_CLEAR_FEATURE)
			dev_dbg(&dev->pdev->dev,
					"SETUP: USB_REQ_CLEAR_FEATURE\n");

		if ((setup->bRequestType & (USB_RECIP_MASK | USB_TYPE_MASK))
				== (USB_RECIP_ENDPOINT | USB_TYPE_STANDARD)) {
			struct langwell_ep	*epn;
			epn = get_ep_by_windex(dev, wIndex);
			
			if (!epn) {
				ep0_stall(dev);
				goto end;
			}

			if (wValue != 0 || wLength != 0
					|| epn->ep_num > dev->ep_max)
				break;

			spin_unlock(&dev->lock);
			rc = langwell_ep_set_halt(&epn->ep,
				(setup->bRequest == USB_REQ_SET_FEATURE)
				? 1 : 0);
			spin_lock(&dev->lock);

		} else if ((setup->bRequestType & (USB_RECIP_MASK
				| USB_TYPE_MASK)) == (USB_RECIP_DEVICE
				| USB_TYPE_STANDARD)) {
			rc = 0;
			switch (wValue) {
			case USB_DEVICE_REMOTE_WAKEUP:
				if (setup->bRequest == USB_REQ_SET_FEATURE) {
					dev->remote_wakeup = 1;
					dev->dev_status |= (1 << wValue);
				} else {
					dev->remote_wakeup = 0;
					dev->dev_status &= ~(1 << wValue);
				}
				break;
			case USB_DEVICE_TEST_MODE:
				dev_dbg(&dev->pdev->dev, "SETUP: TEST MODE\n");
				if ((wIndex & 0xff) ||
					(dev->gadget.speed != USB_SPEED_HIGH))
					ep0_stall(dev);

				switch (wIndex >> 8) {
				case TEST_J:
				case TEST_K:
				case TEST_SE0_NAK:
				case TEST_PACKET:
				case TEST_FORCE_EN:
					if (prime_status_phase(dev, EP_DIR_IN))
						ep0_stall(dev);
					portsc1 = readl(&dev->op_regs->portsc1);
					portsc1 |= (wIndex & 0xf00) << 8;
					writel(portsc1, &dev->op_regs->portsc1);
					goto end;
				default:
					rc = -EOPNOTSUPP;
				}
				break;
			default:
				rc = -EOPNOTSUPP;
				break;
			}

			if (!gadget_is_otg(&dev->gadget))
				break;
			else if (setup->bRequest == USB_DEVICE_B_HNP_ENABLE)
				dev->gadget.b_hnp_enable = 1;
			else if (setup->bRequest == USB_DEVICE_A_HNP_SUPPORT)
				dev->gadget.a_hnp_support = 1;
			else if (setup->bRequest ==
					USB_DEVICE_A_ALT_HNP_SUPPORT)
				dev->gadget.a_alt_hnp_support = 1;
			else
				break;
		} else
			break;

		if (rc == 0) {
			if (prime_status_phase(dev, EP_DIR_IN))
				ep0_stall(dev);
		}
		goto end;
	}

	case USB_REQ_GET_DESCRIPTOR:
		dev_dbg(&dev->pdev->dev,
				"SETUP: USB_REQ_GET_DESCRIPTOR\n");
		goto delegate;

	case USB_REQ_SET_DESCRIPTOR:
		dev_dbg(&dev->pdev->dev,
				"SETUP: USB_REQ_SET_DESCRIPTOR unsupported\n");
		goto delegate;

	case USB_REQ_GET_CONFIGURATION:
		dev_dbg(&dev->pdev->dev,
				"SETUP: USB_REQ_GET_CONFIGURATION\n");
		goto delegate;

	case USB_REQ_SET_CONFIGURATION:
		dev_dbg(&dev->pdev->dev,
				"SETUP: USB_REQ_SET_CONFIGURATION\n");
		goto delegate;

	case USB_REQ_GET_INTERFACE:
		dev_dbg(&dev->pdev->dev,
				"SETUP: USB_REQ_GET_INTERFACE\n");
		goto delegate;

	case USB_REQ_SET_INTERFACE:
		dev_dbg(&dev->pdev->dev,
				"SETUP: USB_REQ_SET_INTERFACE\n");
		goto delegate;

	case USB_REQ_SYNCH_FRAME:
		dev_dbg(&dev->pdev->dev,
				"SETUP: USB_REQ_SYNCH_FRAME unsupported\n");
		goto delegate;

	default:
		
		goto delegate;
delegate:
		
		if (wLength) {
			
			dev->ep0_dir = (setup->bRequestType & USB_DIR_IN)
					?  USB_DIR_IN : USB_DIR_OUT;
			dev_vdbg(&dev->pdev->dev,
					"dev->ep0_dir = 0x%x, wLength = %d\n",
					dev->ep0_dir, wLength);
			spin_unlock(&dev->lock);
			if (dev->driver->setup(&dev->gadget,
					&dev->local_setup_buff) < 0)
				ep0_stall(dev);
			spin_lock(&dev->lock);
			dev->ep0_state = (setup->bRequestType & USB_DIR_IN)
					?  DATA_STATE_XMIT : DATA_STATE_RECV;
		} else {
			
			dev->ep0_dir = USB_DIR_IN;
			dev_vdbg(&dev->pdev->dev,
					"dev->ep0_dir = 0x%x, wLength = %d\n",
					dev->ep0_dir, wLength);
			spin_unlock(&dev->lock);
			if (dev->driver->setup(&dev->gadget,
					&dev->local_setup_buff) < 0)
				ep0_stall(dev);
			spin_lock(&dev->lock);
			dev->ep0_state = WAIT_FOR_OUT_STATUS;
		}
		break;
	}
end:
	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}


static int process_ep_req(struct langwell_udc *dev, int index,
		struct langwell_request *curr_req)
{
	struct langwell_dtd	*curr_dtd;
	struct langwell_dqh	*curr_dqh;
	int			td_complete, actual, remaining_length;
	int			i, dir;
	u8			dtd_status = 0;
	int			retval = 0;

	curr_dqh = &dev->ep_dqh[index];
	dir = index % 2;

	curr_dtd = curr_req->head;
	td_complete = 0;
	actual = curr_req->req.length;

	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	for (i = 0; i < curr_req->dtd_count; i++) {

		
		dtd_status = curr_dtd->dtd_status;

		barrier();
		remaining_length = le16_to_cpu(curr_dtd->dtd_total);
		actual -= remaining_length;

		if (!dtd_status) {
			
			if (!remaining_length) {
				td_complete++;
				dev_vdbg(&dev->pdev->dev,
					"dTD transmitted successfully\n");
			} else {
				if (dir) {
					dev_vdbg(&dev->pdev->dev,
						"TX dTD remains data\n");
					retval = -EPROTO;
					break;

				} else {
					td_complete++;
					break;
				}
			}
		} else {
			
			if (dtd_status & DTD_STS_ACTIVE) {
				dev_dbg(&dev->pdev->dev,
					"dTD status ACTIVE dQH[%d]\n", index);
				retval = 1;
				return retval;
			} else if (dtd_status & DTD_STS_HALTED) {
				dev_err(&dev->pdev->dev,
					"dTD error %08x dQH[%d]\n",
					dtd_status, index);
				
				curr_dqh->dtd_status = 0;
				retval = -EPIPE;
				break;
			} else if (dtd_status & DTD_STS_DBE) {
				dev_dbg(&dev->pdev->dev,
					"data buffer (overflow) error\n");
				retval = -EPROTO;
				break;
			} else if (dtd_status & DTD_STS_TRE) {
				dev_dbg(&dev->pdev->dev,
					"transaction(ISO) error\n");
				retval = -EILSEQ;
				break;
			} else
				dev_err(&dev->pdev->dev,
					"unknown error (0x%x)!\n",
					dtd_status);
		}

		if (i != curr_req->dtd_count - 1)
			curr_dtd = (struct langwell_dtd *)
				curr_dtd->next_dtd_virt;
	}

	if (retval)
		return retval;

	curr_req->req.actual = actual;

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return 0;
}


static void ep0_req_complete(struct langwell_udc *dev,
		struct langwell_ep *ep0, struct langwell_request *req)
{
	u32	new_addr;
	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	if (dev->usb_state == USB_STATE_ADDRESS) {
		
		new_addr = (u32)dev->dev_addr;
		writel(new_addr << USBADR_SHIFT, &dev->op_regs->deviceaddr);

		new_addr = USBADR(readl(&dev->op_regs->deviceaddr));
		dev_vdbg(&dev->pdev->dev, "new_addr = %d\n", new_addr);
	}

	done(ep0, req, 0);

	switch (dev->ep0_state) {
	case DATA_STATE_XMIT:
		
		if (prime_status_phase(dev, EP_DIR_OUT))
			ep0_stall(dev);
		break;
	case DATA_STATE_RECV:
		
		if (prime_status_phase(dev, EP_DIR_IN))
			ep0_stall(dev);
		break;
	case WAIT_FOR_OUT_STATUS:
		dev->ep0_state = WAIT_FOR_SETUP;
		break;
	case WAIT_FOR_SETUP:
		dev_err(&dev->pdev->dev, "unexpect ep0 packets\n");
		break;
	default:
		ep0_stall(dev);
		break;
	}

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}


static void handle_trans_complete(struct langwell_udc *dev)
{
	u32			complete_bits;
	int			i, ep_num, dir, bit_mask, status;
	struct langwell_ep	*epn;
	struct langwell_request	*curr_req, *temp_req;

	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	complete_bits = readl(&dev->op_regs->endptcomplete);
	dev_vdbg(&dev->pdev->dev, "endptcomplete register: 0x%08x\n",
			complete_bits);

	
	writel(complete_bits, &dev->op_regs->endptcomplete);

	if (!complete_bits) {
		dev_dbg(&dev->pdev->dev, "complete_bits = 0\n");
		goto done;
	}

	for (i = 0; i < dev->ep_max; i++) {
		ep_num = i / 2;
		dir = i % 2;

		bit_mask = 1 << (ep_num + 16 * dir);

		if (!(complete_bits & bit_mask))
			continue;

		
		if (i == 1)
			epn = &dev->ep[0];
		else
			epn = &dev->ep[i];

		if (epn->name == NULL) {
			dev_warn(&dev->pdev->dev, "invalid endpoint\n");
			continue;
		}

		if (i < 2)
			
			dev_dbg(&dev->pdev->dev, "%s-%s transfer completed\n",
					epn->name,
					is_in(epn) ? "in" : "out");
		else
			dev_dbg(&dev->pdev->dev, "%s transfer completed\n",
					epn->name);

		
		list_for_each_entry_safe(curr_req, temp_req,
				&epn->queue, queue) {
			status = process_ep_req(dev, i, curr_req);
			dev_vdbg(&dev->pdev->dev, "%s req status: %d\n",
					epn->name, status);

			if (status)
				break;

			
			curr_req->req.status = status;

			
			if (ep_num == 0) {
				ep0_req_complete(dev, epn, curr_req);
				break;
			} else {
				done(epn, curr_req, status);
			}
		}
	}
done:
	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}

static void handle_port_change(struct langwell_udc *dev)
{
	u32	portsc1, devlc;

	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	if (dev->bus_reset)
		dev->bus_reset = 0;

	portsc1 = readl(&dev->op_regs->portsc1);
	devlc = readl(&dev->op_regs->devlc);
	dev_vdbg(&dev->pdev->dev, "portsc1 = 0x%08x, devlc = 0x%08x\n",
			portsc1, devlc);

	
	if (!(portsc1 & PORTS_PR)) {
		
		dev->gadget.speed = lpm_device_speed(devlc);
		dev_vdbg(&dev->pdev->dev, "dev->gadget.speed = %d\n",
			dev->gadget.speed);
	}

	
	if (dev->lpm && dev->lpm_state == LPM_L0)
		if (portsc1 & PORTS_SUSP && portsc1 & PORTS_SLP) {
			dev_info(&dev->pdev->dev, "LPM L0 to L1\n");
			dev->lpm_state = LPM_L1;
		}

	
	if (dev->lpm && dev->lpm_state == LPM_L1)
		if (!(portsc1 & PORTS_SUSP)) {
			dev_info(&dev->pdev->dev, "LPM L1 to L0\n");
			dev->lpm_state = LPM_L0;
		}

	
	if (!dev->resume_state)
		dev->usb_state = USB_STATE_DEFAULT;

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}


static void handle_usb_reset(struct langwell_udc *dev)
{
	u32		deviceaddr,
			endptsetupstat,
			endptcomplete;
	unsigned long	timeout;

	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	
	deviceaddr = readl(&dev->op_regs->deviceaddr);
	writel(deviceaddr & ~USBADR_MASK, &dev->op_regs->deviceaddr);

	dev->dev_addr = 0;

	
	dev->resume_state = 0;

	
	if (dev->lpm)
		dev->lpm_state = LPM_L0;

	dev->ep0_dir = USB_DIR_OUT;
	dev->ep0_state = WAIT_FOR_SETUP;

	
	dev->remote_wakeup = 0;
	dev->dev_status = 1 << USB_DEVICE_SELF_POWERED;
	dev->gadget.b_hnp_enable = 0;
	dev->gadget.a_hnp_support = 0;
	dev->gadget.a_alt_hnp_support = 0;

	
	endptsetupstat = readl(&dev->op_regs->endptsetupstat);
	writel(endptsetupstat, &dev->op_regs->endptsetupstat);

	
	endptcomplete = readl(&dev->op_regs->endptcomplete);
	writel(endptcomplete, &dev->op_regs->endptcomplete);

	
	timeout = jiffies + PRIME_TIMEOUT;
	while (readl(&dev->op_regs->endptprime)) {
		if (time_after(jiffies, timeout)) {
			dev_err(&dev->pdev->dev, "USB reset timeout\n");
			break;
		}
		cpu_relax();
	}

	
	writel((u32) ~0, &dev->op_regs->endptflush);

	if (readl(&dev->op_regs->portsc1) & PORTS_PR) {
		dev_vdbg(&dev->pdev->dev, "USB bus reset\n");
		
		dev->bus_reset = 1;

		
		stop_activity(dev);
		dev->usb_state = USB_STATE_DEFAULT;
	} else {
		dev_vdbg(&dev->pdev->dev, "device controller reset\n");
		
		langwell_udc_reset(dev);

		
		stop_activity(dev);

		
		ep0_reset(dev);

		
		langwell_udc_start(dev);

		dev->usb_state = USB_STATE_ATTACHED;
	}

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}


static void handle_bus_suspend(struct langwell_udc *dev)
{
	dev_dbg(&dev->pdev->dev, "---> %s()\n", __func__);

	dev->resume_state = dev->usb_state;
	dev->usb_state = USB_STATE_SUSPENDED;

	
	if (dev->driver) {
		if (dev->driver->suspend) {
			spin_unlock(&dev->lock);
			dev->driver->suspend(&dev->gadget);
			spin_lock(&dev->lock);
			dev_dbg(&dev->pdev->dev, "suspend %s\n",
					dev->driver->driver.name);
		}
	}

	
	if (dev->pdev->device != 0x0829)
		langwell_phy_low_power(dev, 0);

	dev_dbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}


static void handle_bus_resume(struct langwell_udc *dev)
{
	dev_dbg(&dev->pdev->dev, "---> %s()\n", __func__);

	dev->usb_state = dev->resume_state;
	dev->resume_state = 0;

	
	if (dev->pdev->device != 0x0829)
		langwell_phy_low_power(dev, 0);

	
	if (dev->driver) {
		if (dev->driver->resume) {
			spin_unlock(&dev->lock);
			dev->driver->resume(&dev->gadget);
			spin_lock(&dev->lock);
			dev_dbg(&dev->pdev->dev, "resume %s\n",
					dev->driver->driver.name);
		}
	}

	dev_dbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}


static irqreturn_t langwell_irq(int irq, void *_dev)
{
	struct langwell_udc	*dev = _dev;
	u32			usbsts,
				usbintr,
				irq_sts,
				portsc1;

	dev_vdbg(&dev->pdev->dev, "---> %s()\n", __func__);

	if (dev->stopped) {
		dev_vdbg(&dev->pdev->dev, "handle IRQ_NONE\n");
		dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
		return IRQ_NONE;
	}

	spin_lock(&dev->lock);

	
	usbsts = readl(&dev->op_regs->usbsts);

	
	usbintr = readl(&dev->op_regs->usbintr);

	irq_sts = usbsts & usbintr;
	dev_vdbg(&dev->pdev->dev,
			"usbsts = 0x%08x, usbintr = 0x%08x, irq_sts = 0x%08x\n",
			usbsts, usbintr, irq_sts);

	if (!irq_sts) {
		dev_vdbg(&dev->pdev->dev, "handle IRQ_NONE\n");
		dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
		spin_unlock(&dev->lock);
		return IRQ_NONE;
	}

	
	writel(irq_sts, &dev->op_regs->usbsts);

	
	portsc1 = readl(&dev->op_regs->portsc1);
	if (dev->usb_state == USB_STATE_SUSPENDED)
		if (!(portsc1 & PORTS_SUSP))
			handle_bus_resume(dev);

	
	if (irq_sts & STS_UI) {
		dev_vdbg(&dev->pdev->dev, "USB interrupt\n");

		
		if (readl(&dev->op_regs->endptsetupstat)
				& EP0SETUPSTAT_MASK) {
			dev_vdbg(&dev->pdev->dev,
				"USB SETUP packet received interrupt\n");
			
			setup_tripwire(dev);
			handle_setup_packet(dev, &dev->local_setup_buff);
		}

		
		if (readl(&dev->op_regs->endptcomplete)) {
			dev_vdbg(&dev->pdev->dev,
				"USB transfer completion interrupt\n");
			handle_trans_complete(dev);
		}
	}

	
	if (irq_sts & STS_SRI) {
		
		
	}

	
	if (irq_sts & STS_PCI) {
		dev_vdbg(&dev->pdev->dev, "port change detect interrupt\n");
		handle_port_change(dev);
	}

	
	if (irq_sts & STS_SLI) {
		dev_vdbg(&dev->pdev->dev, "suspend interrupt\n");
		handle_bus_suspend(dev);
	}

	
	if (irq_sts & STS_URI) {
		dev_vdbg(&dev->pdev->dev, "USB reset interrupt\n");
		handle_usb_reset(dev);
	}

	
	if (irq_sts & (STS_UEI | STS_SEI)) {
		
		dev_warn(&dev->pdev->dev, "error IRQ, irq_sts: %x\n", irq_sts);
	}

	spin_unlock(&dev->lock);

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return IRQ_HANDLED;
}



static void gadget_release(struct device *_dev)
{
	struct langwell_udc	*dev = dev_get_drvdata(_dev);

	dev_dbg(&dev->pdev->dev, "---> %s()\n", __func__);

	complete(dev->done);

	dev_dbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	kfree(dev);
}


static void sram_init(struct langwell_udc *dev)
{
	struct pci_dev		*pdev = dev->pdev;

	dev_dbg(&dev->pdev->dev, "---> %s()\n", __func__);

	dev->sram_addr = pci_resource_start(pdev, 1);
	dev->sram_size = pci_resource_len(pdev, 1);
	dev_info(&dev->pdev->dev, "Found private SRAM at %x size:%x\n",
			dev->sram_addr, dev->sram_size);
	dev->got_sram = 1;

	if (pci_request_region(pdev, 1, kobject_name(&pdev->dev.kobj))) {
		dev_warn(&dev->pdev->dev, "SRAM request failed\n");
		dev->got_sram = 0;
	} else if (!dma_declare_coherent_memory(&pdev->dev, dev->sram_addr,
			dev->sram_addr, dev->sram_size, DMA_MEMORY_MAP)) {
		dev_warn(&dev->pdev->dev, "SRAM DMA declare failed\n");
		pci_release_region(pdev, 1);
		dev->got_sram = 0;
	}

	dev_dbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}


static void sram_deinit(struct langwell_udc *dev)
{
	struct pci_dev *pdev = dev->pdev;

	dev_dbg(&dev->pdev->dev, "---> %s()\n", __func__);

	dma_release_declared_memory(&pdev->dev);
	pci_release_region(pdev, 1);

	dev->got_sram = 0;

	dev_info(&dev->pdev->dev, "release SRAM caching\n");
	dev_dbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}


static void langwell_udc_remove(struct pci_dev *pdev)
{
	struct langwell_udc	*dev = pci_get_drvdata(pdev);

	DECLARE_COMPLETION(done);

	BUG_ON(dev->driver);
	dev_dbg(&dev->pdev->dev, "---> %s()\n", __func__);

	dev->done = &done;

	
	if (dev->dtd_pool)
		dma_pool_destroy(dev->dtd_pool);

	if (dev->ep_dqh)
		dma_free_coherent(&pdev->dev, dev->ep_dqh_size,
			dev->ep_dqh, dev->ep_dqh_dma);

	
	if (dev->has_sram && dev->got_sram)
		sram_deinit(dev);

	if (dev->status_req) {
		kfree(dev->status_req->req.buf);
		kfree(dev->status_req);
	}

	kfree(dev->ep);

	
	if (dev->got_irq)
		free_irq(pdev->irq, dev);

	if (dev->cap_regs)
		iounmap(dev->cap_regs);

	if (dev->region)
		release_mem_region(pci_resource_start(pdev, 0),
				pci_resource_len(pdev, 0));

	if (dev->enabled)
		pci_disable_device(pdev);

	dev->cap_regs = NULL;

	dev_info(&dev->pdev->dev, "unbind\n");
	dev_dbg(&dev->pdev->dev, "<--- %s()\n", __func__);

	device_unregister(&dev->gadget.dev);
	device_remove_file(&pdev->dev, &dev_attr_langwell_udc);
	device_remove_file(&pdev->dev, &dev_attr_remote_wakeup);

	pci_set_drvdata(pdev, NULL);

	
	wait_for_completion(&done);
}


static int langwell_udc_probe(struct pci_dev *pdev,
		const struct pci_device_id *id)
{
	struct langwell_udc	*dev;
	unsigned long		resource, len;
	void			__iomem *base = NULL;
	size_t			size;
	int			retval;

	
	dev = kzalloc(sizeof *dev, GFP_KERNEL);
	if (dev == NULL) {
		retval = -ENOMEM;
		goto error;
	}

	
	spin_lock_init(&dev->lock);

	dev->pdev = pdev;
	dev_dbg(&dev->pdev->dev, "---> %s()\n", __func__);

	pci_set_drvdata(pdev, dev);

	
	if (pci_enable_device(pdev) < 0) {
		retval = -ENODEV;
		goto error;
	}
	dev->enabled = 1;

	
	resource = pci_resource_start(pdev, 0);
	len = pci_resource_len(pdev, 0);
	if (!request_mem_region(resource, len, driver_name)) {
		dev_err(&dev->pdev->dev, "controller already in use\n");
		retval = -EBUSY;
		goto error;
	}
	dev->region = 1;

	base = ioremap_nocache(resource, len);
	if (base == NULL) {
		dev_err(&dev->pdev->dev, "can't map memory\n");
		retval = -EFAULT;
		goto error;
	}

	dev->cap_regs = (struct langwell_cap_regs __iomem *) base;
	dev_vdbg(&dev->pdev->dev, "dev->cap_regs: %p\n", dev->cap_regs);
	dev->op_regs = (struct langwell_op_regs __iomem *)
		(base + OP_REG_OFFSET);
	dev_vdbg(&dev->pdev->dev, "dev->op_regs: %p\n", dev->op_regs);

	
	if (!pdev->irq) {
		dev_err(&dev->pdev->dev, "No IRQ. Check PCI setup!\n");
		retval = -ENODEV;
		goto error;
	}

	dev->has_sram = 1;
	dev->got_sram = 0;
	dev_vdbg(&dev->pdev->dev, "dev->has_sram: %d\n", dev->has_sram);

	
	if (dev->has_sram && !dev->got_sram)
		sram_init(dev);

	dev_info(&dev->pdev->dev,
			"irq %d, io mem: 0x%08lx, len: 0x%08lx, pci mem 0x%p\n",
			pdev->irq, resource, len, base);
	
	pci_set_master(pdev);

	if (request_irq(pdev->irq, langwell_irq, IRQF_SHARED,
				driver_name, dev) != 0) {
		dev_err(&dev->pdev->dev,
				"request interrupt %d failed\n", pdev->irq);
		retval = -EBUSY;
		goto error;
	}
	dev->got_irq = 1;

	
	dev->stopped = 1;

	
	dev->lpm = (readl(&dev->cap_regs->hccparams) & HCC_LEN) ? 1 : 0;
	dev->dciversion = readw(&dev->cap_regs->dciversion);
	dev->devcap = (readl(&dev->cap_regs->dccparams) & DEVCAP) ? 1 : 0;
	dev_vdbg(&dev->pdev->dev, "dev->lpm: %d\n", dev->lpm);
	dev_vdbg(&dev->pdev->dev, "dev->dciversion: 0x%04x\n",
			dev->dciversion);
	dev_vdbg(&dev->pdev->dev, "dccparams: 0x%08x\n",
			readl(&dev->cap_regs->dccparams));
	dev_vdbg(&dev->pdev->dev, "dev->devcap: %d\n", dev->devcap);
	if (!dev->devcap) {
		dev_err(&dev->pdev->dev, "can't support device mode\n");
		retval = -ENODEV;
		goto error;
	}

	
	dev->ep_max = DEN(readl(&dev->cap_regs->dccparams)) * 2;
	dev_vdbg(&dev->pdev->dev, "dev->ep_max: %d\n", dev->ep_max);

	
	dev->ep = kzalloc(sizeof(struct langwell_ep) * dev->ep_max,
			GFP_KERNEL);
	if (!dev->ep) {
		dev_err(&dev->pdev->dev, "allocate endpoints memory failed\n");
		retval = -ENOMEM;
		goto error;
	}

	
	size = dev->ep_max * sizeof(struct langwell_dqh);
	dev_vdbg(&dev->pdev->dev, "orig size = %zd\n", size);
	if (size < DQH_ALIGNMENT)
		size = DQH_ALIGNMENT;
	else if ((size % DQH_ALIGNMENT) != 0) {
		size += DQH_ALIGNMENT + 1;
		size &= ~(DQH_ALIGNMENT - 1);
	}
	dev->ep_dqh = dma_alloc_coherent(&pdev->dev, size,
					&dev->ep_dqh_dma, GFP_KERNEL);
	if (!dev->ep_dqh) {
		dev_err(&dev->pdev->dev, "allocate dQH memory failed\n");
		retval = -ENOMEM;
		goto error;
	}
	dev->ep_dqh_size = size;
	dev_vdbg(&dev->pdev->dev, "ep_dqh_size = %zd\n", dev->ep_dqh_size);

	
	dev->status_req = kzalloc(sizeof(struct langwell_request), GFP_KERNEL);
	if (!dev->status_req) {
		dev_err(&dev->pdev->dev,
				"allocate status_req memory failed\n");
		retval = -ENOMEM;
		goto error;
	}
	INIT_LIST_HEAD(&dev->status_req->queue);

	
	dev->status_req->req.buf = kmalloc(8, GFP_KERNEL);
	dev->status_req->req.dma = virt_to_phys(dev->status_req->req.buf);

	dev->resume_state = USB_STATE_NOTATTACHED;
	dev->usb_state = USB_STATE_POWERED;
	dev->ep0_dir = USB_DIR_OUT;

	
	dev->remote_wakeup = 0;
	dev->dev_status = 1 << USB_DEVICE_SELF_POWERED;

	
	langwell_udc_reset(dev);

	
	dev->gadget.ops = &langwell_ops;	
	dev->gadget.ep0 = &dev->ep[0].ep;	
	INIT_LIST_HEAD(&dev->gadget.ep_list);	
	dev->gadget.speed = USB_SPEED_UNKNOWN;	
	dev->gadget.max_speed = USB_SPEED_HIGH;	

	
	dev_set_name(&dev->gadget.dev, "gadget");
	dev->gadget.dev.parent = &pdev->dev;
	dev->gadget.dev.dma_mask = pdev->dev.dma_mask;
	dev->gadget.dev.release = gadget_release;
	dev->gadget.name = driver_name;		

	
	eps_reinit(dev);

	
	ep0_reset(dev);

	
	dev->dtd_pool = dma_pool_create("langwell_dtd",
			&dev->pdev->dev,
			sizeof(struct langwell_dtd),
			DTD_ALIGNMENT,
			DMA_BOUNDARY);

	if (!dev->dtd_pool) {
		retval = -ENOMEM;
		goto error;
	}

	
	dev_info(&dev->pdev->dev, "%s\n", driver_desc);
	dev_info(&dev->pdev->dev, "irq %d, pci mem %p\n", pdev->irq, base);
	dev_info(&dev->pdev->dev, "Driver version: " DRIVER_VERSION "\n");
	dev_info(&dev->pdev->dev, "Support (max) %d endpoints\n", dev->ep_max);
	dev_info(&dev->pdev->dev, "Device interface version: 0x%04x\n",
			dev->dciversion);
	dev_info(&dev->pdev->dev, "Controller mode: %s\n",
			dev->devcap ? "Device" : "Host");
	dev_info(&dev->pdev->dev, "Support USB LPM: %s\n",
			dev->lpm ? "Yes" : "No");

	dev_vdbg(&dev->pdev->dev,
			"After langwell_udc_probe(), print all registers:\n");
	print_all_registers(dev);

	retval = device_register(&dev->gadget.dev);
	if (retval)
		goto error;

	retval = usb_add_gadget_udc(&pdev->dev, &dev->gadget);
	if (retval)
		goto error;

	retval = device_create_file(&pdev->dev, &dev_attr_langwell_udc);
	if (retval)
		goto error;

	retval = device_create_file(&pdev->dev, &dev_attr_remote_wakeup);
	if (retval)
		goto error_attr1;

	dev_vdbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return 0;

error_attr1:
	device_remove_file(&pdev->dev, &dev_attr_langwell_udc);
error:
	if (dev) {
		dev_dbg(&dev->pdev->dev, "<--- %s()\n", __func__);
		langwell_udc_remove(pdev);
	}

	return retval;
}


static int langwell_udc_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct langwell_udc	*dev = pci_get_drvdata(pdev);

	dev_dbg(&dev->pdev->dev, "---> %s()\n", __func__);

	usb_del_gadget_udc(&dev->gadget);
	
	langwell_udc_stop(dev);

	
	if (dev->got_irq)
		free_irq(pdev->irq, dev);
	dev->got_irq = 0;

	
	pci_save_state(pdev);

	spin_lock_irq(&dev->lock);
	
	stop_activity(dev);
	spin_unlock_irq(&dev->lock);

	
	if (dev->dtd_pool)
		dma_pool_destroy(dev->dtd_pool);

	if (dev->ep_dqh)
		dma_free_coherent(&pdev->dev, dev->ep_dqh_size,
			dev->ep_dqh, dev->ep_dqh_dma);

	
	if (dev->has_sram && dev->got_sram)
		sram_deinit(dev);

	
	pci_set_power_state(pdev, PCI_D3hot);

	
	if (dev->pdev->device != 0x0829)
		langwell_phy_low_power(dev, 1);

	dev_dbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return 0;
}


static int langwell_udc_resume(struct pci_dev *pdev)
{
	struct langwell_udc	*dev = pci_get_drvdata(pdev);
	size_t			size;

	dev_dbg(&dev->pdev->dev, "---> %s()\n", __func__);

	
	if (dev->pdev->device != 0x0829)
		langwell_phy_low_power(dev, 0);

	
	pci_set_power_state(pdev, PCI_D0);

	
	if (dev->has_sram && !dev->got_sram)
		sram_init(dev);

	
	size = dev->ep_max * sizeof(struct langwell_dqh);
	dev_vdbg(&dev->pdev->dev, "orig size = %zd\n", size);
	if (size < DQH_ALIGNMENT)
		size = DQH_ALIGNMENT;
	else if ((size % DQH_ALIGNMENT) != 0) {
		size += DQH_ALIGNMENT + 1;
		size &= ~(DQH_ALIGNMENT - 1);
	}
	dev->ep_dqh = dma_alloc_coherent(&pdev->dev, size,
					&dev->ep_dqh_dma, GFP_KERNEL);
	if (!dev->ep_dqh) {
		dev_err(&dev->pdev->dev, "allocate dQH memory failed\n");
		return -ENOMEM;
	}
	dev->ep_dqh_size = size;
	dev_vdbg(&dev->pdev->dev, "ep_dqh_size = %zd\n", dev->ep_dqh_size);

	
	dev->dtd_pool = dma_pool_create("langwell_dtd",
			&dev->pdev->dev,
			sizeof(struct langwell_dtd),
			DTD_ALIGNMENT,
			DMA_BOUNDARY);

	if (!dev->dtd_pool)
		return -ENOMEM;

	
	pci_restore_state(pdev);

	
	if (request_irq(pdev->irq, langwell_irq, IRQF_SHARED,
				driver_name, dev) != 0) {
		dev_err(&dev->pdev->dev, "request interrupt %d failed\n",
				pdev->irq);
		return -EBUSY;
	}
	dev->got_irq = 1;

	
	if (dev->stopped) {
		
		langwell_udc_reset(dev);

		
		ep0_reset(dev);

		
		if (dev->driver)
			langwell_udc_start(dev);
	}

	
	dev->usb_state = USB_STATE_ATTACHED;
	dev->ep0_state = WAIT_FOR_SETUP;
	dev->ep0_dir = USB_DIR_OUT;

	dev_dbg(&dev->pdev->dev, "<--- %s()\n", __func__);
	return 0;
}


static void langwell_udc_shutdown(struct pci_dev *pdev)
{
	struct langwell_udc	*dev = pci_get_drvdata(pdev);
	u32			usbmode;

	dev_dbg(&dev->pdev->dev, "---> %s()\n", __func__);

	
	usbmode = readl(&dev->op_regs->usbmode);
	dev_dbg(&dev->pdev->dev, "usbmode = 0x%08x\n", usbmode);
	usbmode &= (~3 | MODE_IDLE);
	writel(usbmode, &dev->op_regs->usbmode);

	dev_dbg(&dev->pdev->dev, "<--- %s()\n", __func__);
}


static const struct pci_device_id pci_ids[] = { {
	.class =	((PCI_CLASS_SERIAL_USB << 8) | 0xfe),
	.class_mask =	~0,
	.vendor =	0x8086,
	.device =	0x0811,
	.subvendor =	PCI_ANY_ID,
	.subdevice =	PCI_ANY_ID,
}, {  }
};

MODULE_DEVICE_TABLE(pci, pci_ids);


static struct pci_driver langwell_pci_driver = {
	.name =		(char *) driver_name,
	.id_table =	pci_ids,

	.probe =	langwell_udc_probe,
	.remove =	langwell_udc_remove,

	
	.suspend =	langwell_udc_suspend,
	.resume =	langwell_udc_resume,

	.shutdown =	langwell_udc_shutdown,
};


static int __init init(void)
{
	return pci_register_driver(&langwell_pci_driver);
}
module_init(init);


static void __exit cleanup(void)
{
	pci_unregister_driver(&langwell_pci_driver);
}
module_exit(cleanup);


MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Xiaochen Shen <xiaochen.shen@intel.com>");
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");

