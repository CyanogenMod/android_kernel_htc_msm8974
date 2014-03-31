/*
 * amd5536.c -- AMD 5536 UDC high/full speed USB device controller
 *
 * Copyright (C) 2005-2007 AMD (http://www.amd.com)
 * Author: Thomas Dahlmann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */



#define UDC_MOD_DESCRIPTION		"AMD 5536 UDC - USB Device Controller"
#define UDC_DRIVER_VERSION_STRING	"01.00.0206"

#include <linux/module.h>
#include <linux/pci.h>
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
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/dmapool.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/prefetch.h>

#include <asm/byteorder.h>
#include <asm/unaligned.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

#include "amd5536udc.h"


static void udc_tasklet_disconnect(unsigned long);
static void empty_req_queue(struct udc_ep *);
static int udc_probe(struct udc *dev);
static void udc_basic_init(struct udc *dev);
static void udc_setup_endpoints(struct udc *dev);
static void udc_soft_reset(struct udc *dev);
static struct udc_request *udc_alloc_bna_dummy(struct udc_ep *ep);
static void udc_free_request(struct usb_ep *usbep, struct usb_request *usbreq);
static int udc_free_dma_chain(struct udc *dev, struct udc_request *req);
static int udc_create_dma_chain(struct udc_ep *ep, struct udc_request *req,
				unsigned long buf_len, gfp_t gfp_flags);
static int udc_remote_wakeup(struct udc *dev);
static int udc_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id);
static void udc_pci_remove(struct pci_dev *pdev);

static const char mod_desc[] = UDC_MOD_DESCRIPTION;
static const char name[] = "amd5536udc";

static const struct usb_ep_ops udc_ep_ops;

static union udc_setup_data setup_data;

static struct udc *udc;

static DEFINE_SPINLOCK(udc_irq_spinlock);
static DEFINE_SPINLOCK(udc_stall_spinlock);

static unsigned int udc_rxfifo_pending;

static int soft_reset_occured;
static int soft_reset_after_usbreset_occured;

static struct timer_list udc_timer;
static int stop_timer;

static int set_rde = -1;

static DECLARE_COMPLETION(on_exit);
static struct timer_list udc_pollstall_timer;
static int stop_pollstall_timer;
static DECLARE_COMPLETION(on_pollstall_exit);

static DECLARE_TASKLET(disconnect_tasklet, udc_tasklet_disconnect,
		(unsigned long) &udc);


static const char ep0_string[] = "ep0in";
static const char *const ep_string[] = {
	ep0_string,
	"ep1in-int", "ep2in-bulk", "ep3in-bulk", "ep4in-bulk", "ep5in-bulk",
	"ep6in-bulk", "ep7in-bulk", "ep8in-bulk", "ep9in-bulk", "ep10in-bulk",
	"ep11in-bulk", "ep12in-bulk", "ep13in-bulk", "ep14in-bulk",
	"ep15in-bulk", "ep0out", "ep1out-bulk", "ep2out-bulk", "ep3out-bulk",
	"ep4out-bulk", "ep5out-bulk", "ep6out-bulk", "ep7out-bulk",
	"ep8out-bulk", "ep9out-bulk", "ep10out-bulk", "ep11out-bulk",
	"ep12out-bulk", "ep13out-bulk", "ep14out-bulk", "ep15out-bulk"
};

static bool use_dma = 1;
static bool use_dma_ppb = 1;
static bool use_dma_ppb_du;
static int use_dma_bufferfill_mode;
static bool use_fullspeed;
static unsigned long hs_tx_buf = UDC_EPIN_BUFF_SIZE;

module_param(use_dma, bool, S_IRUGO);
MODULE_PARM_DESC(use_dma, "true for DMA");
module_param(use_dma_ppb, bool, S_IRUGO);
MODULE_PARM_DESC(use_dma_ppb, "true for DMA in packet per buffer mode");
module_param(use_dma_ppb_du, bool, S_IRUGO);
MODULE_PARM_DESC(use_dma_ppb_du,
	"true for DMA in packet per buffer mode with descriptor update");
module_param(use_fullspeed, bool, S_IRUGO);
MODULE_PARM_DESC(use_fullspeed, "true for fullspeed only");

static void print_regs(struct udc *dev)
{
	DBG(dev, "------- Device registers -------\n");
	DBG(dev, "dev config     = %08x\n", readl(&dev->regs->cfg));
	DBG(dev, "dev control    = %08x\n", readl(&dev->regs->ctl));
	DBG(dev, "dev status     = %08x\n", readl(&dev->regs->sts));
	DBG(dev, "\n");
	DBG(dev, "dev int's      = %08x\n", readl(&dev->regs->irqsts));
	DBG(dev, "dev intmask    = %08x\n", readl(&dev->regs->irqmsk));
	DBG(dev, "\n");
	DBG(dev, "dev ep int's   = %08x\n", readl(&dev->regs->ep_irqsts));
	DBG(dev, "dev ep intmask = %08x\n", readl(&dev->regs->ep_irqmsk));
	DBG(dev, "\n");
	DBG(dev, "USE DMA        = %d\n", use_dma);
	if (use_dma && use_dma_ppb && !use_dma_ppb_du) {
		DBG(dev, "DMA mode       = PPBNDU (packet per buffer "
			"WITHOUT desc. update)\n");
		dev_info(&dev->pdev->dev, "DMA mode (%s)\n", "PPBNDU");
	} else if (use_dma && use_dma_ppb && use_dma_ppb_du) {
		DBG(dev, "DMA mode       = PPBDU (packet per buffer "
			"WITH desc. update)\n");
		dev_info(&dev->pdev->dev, "DMA mode (%s)\n", "PPBDU");
	}
	if (use_dma && use_dma_bufferfill_mode) {
		DBG(dev, "DMA mode       = BF (buffer fill mode)\n");
		dev_info(&dev->pdev->dev, "DMA mode (%s)\n", "BF");
	}
	if (!use_dma)
		dev_info(&dev->pdev->dev, "FIFO mode\n");
	DBG(dev, "-------------------------------------------------------\n");
}

static int udc_mask_unused_interrupts(struct udc *dev)
{
	u32 tmp;

	
	tmp =	AMD_BIT(UDC_DEVINT_SVC) |
		AMD_BIT(UDC_DEVINT_ENUM) |
		AMD_BIT(UDC_DEVINT_US) |
		AMD_BIT(UDC_DEVINT_UR) |
		AMD_BIT(UDC_DEVINT_ES) |
		AMD_BIT(UDC_DEVINT_SI) |
		AMD_BIT(UDC_DEVINT_SOF)|
		AMD_BIT(UDC_DEVINT_SC);
	writel(tmp, &dev->regs->irqmsk);

	
	writel(UDC_EPINT_MSK_DISABLE_ALL, &dev->regs->ep_irqmsk);

	return 0;
}

static int udc_enable_ep0_interrupts(struct udc *dev)
{
	u32 tmp;

	DBG(dev, "udc_enable_ep0_interrupts()\n");

	
	tmp = readl(&dev->regs->ep_irqmsk);
	
	tmp &= AMD_UNMASK_BIT(UDC_EPINT_IN_EP0)
		& AMD_UNMASK_BIT(UDC_EPINT_OUT_EP0);
	writel(tmp, &dev->regs->ep_irqmsk);

	return 0;
}

static int udc_enable_dev_setup_interrupts(struct udc *dev)
{
	u32 tmp;

	DBG(dev, "enable device interrupts for setup data\n");

	
	tmp = readl(&dev->regs->irqmsk);

	
	tmp &= AMD_UNMASK_BIT(UDC_DEVINT_SI)
		& AMD_UNMASK_BIT(UDC_DEVINT_SC)
		& AMD_UNMASK_BIT(UDC_DEVINT_UR)
		& AMD_UNMASK_BIT(UDC_DEVINT_SVC)
		& AMD_UNMASK_BIT(UDC_DEVINT_ENUM);
	writel(tmp, &dev->regs->irqmsk);

	return 0;
}

static int udc_set_txfifo_addr(struct udc_ep *ep)
{
	struct udc	*dev;
	u32 tmp;
	int i;

	if (!ep || !(ep->in))
		return -EINVAL;

	dev = ep->dev;
	ep->txfifo = dev->txfifo;

	
	for (i = 0; i < ep->num; i++) {
		if (dev->ep[i].regs) {
			
			tmp = readl(&dev->ep[i].regs->bufin_framenum);
			tmp = AMD_GETBITS(tmp, UDC_EPIN_BUFF_SIZE);
			ep->txfifo += tmp;
		}
	}
	return 0;
}

static u32 cnak_pending;

static void UDC_QUEUE_CNAK(struct udc_ep *ep, unsigned num)
{
	if (readl(&ep->regs->ctl) & AMD_BIT(UDC_EPCTL_NAK)) {
		DBG(ep->dev, "NAK could not be cleared for ep%d\n", num);
		cnak_pending |= 1 << (num);
		ep->naking = 1;
	} else
		cnak_pending = cnak_pending & (~(1 << (num)));
}


static int
udc_ep_enable(struct usb_ep *usbep, const struct usb_endpoint_descriptor *desc)
{
	struct udc_ep		*ep;
	struct udc		*dev;
	u32			tmp;
	unsigned long		iflags;
	u8 udc_csr_epix;
	unsigned		maxpacket;

	if (!usbep
			|| usbep->name == ep0_string
			|| !desc
			|| desc->bDescriptorType != USB_DT_ENDPOINT)
		return -EINVAL;

	ep = container_of(usbep, struct udc_ep, ep);
	dev = ep->dev;

	DBG(dev, "udc_ep_enable() ep %d\n", ep->num);

	if (!dev->driver || dev->gadget.speed == USB_SPEED_UNKNOWN)
		return -ESHUTDOWN;

	spin_lock_irqsave(&dev->lock, iflags);
	ep->desc = desc;

	ep->halted = 0;

	
	tmp = readl(&dev->ep[ep->num].regs->ctl);
	tmp = AMD_ADDBITS(tmp, desc->bmAttributes, UDC_EPCTL_ET);
	writel(tmp, &dev->ep[ep->num].regs->ctl);

	
	maxpacket = usb_endpoint_maxp(desc);
	tmp = readl(&dev->ep[ep->num].regs->bufout_maxpkt);
	tmp = AMD_ADDBITS(tmp, maxpacket, UDC_EP_MAX_PKT_SIZE);
	ep->ep.maxpacket = maxpacket;
	writel(tmp, &dev->ep[ep->num].regs->bufout_maxpkt);

	
	if (ep->in) {

		
		udc_csr_epix = ep->num;

		
		tmp = readl(&dev->ep[ep->num].regs->bufin_framenum);
		
		tmp = AMD_ADDBITS(
				tmp,
				maxpacket * UDC_EPIN_BUFF_SIZE_MULT
					  / UDC_DWORD_BYTES,
				UDC_EPIN_BUFF_SIZE);
		writel(tmp, &dev->ep[ep->num].regs->bufin_framenum);

		
		udc_set_txfifo_addr(ep);

		
		tmp = readl(&ep->regs->ctl);
		tmp |= AMD_BIT(UDC_EPCTL_F);
		writel(tmp, &ep->regs->ctl);

	
	} else {
		
		udc_csr_epix = ep->num - UDC_CSR_EP_OUT_IX_OFS;

		
		tmp = readl(&dev->csr->ne[ep->num - UDC_CSR_EP_OUT_IX_OFS]);
		tmp = AMD_ADDBITS(tmp, maxpacket,
					UDC_CSR_NE_MAX_PKT);
		writel(tmp, &dev->csr->ne[ep->num - UDC_CSR_EP_OUT_IX_OFS]);

		if (use_dma && !ep->in) {
			
			ep->bna_dummy_req = udc_alloc_bna_dummy(ep);
			ep->bna_occurred = 0;
		}

		if (ep->num != UDC_EP0OUT_IX)
			dev->data_ep_enabled = 1;
	}

	
	tmp = readl(&dev->csr->ne[udc_csr_epix]);
	
	tmp = AMD_ADDBITS(tmp, maxpacket, UDC_CSR_NE_MAX_PKT);
	
	tmp = AMD_ADDBITS(tmp, desc->bEndpointAddress, UDC_CSR_NE_NUM);
	
	tmp = AMD_ADDBITS(tmp, ep->in, UDC_CSR_NE_DIR);
	
	tmp = AMD_ADDBITS(tmp, desc->bmAttributes, UDC_CSR_NE_TYPE);
	
	tmp = AMD_ADDBITS(tmp, ep->dev->cur_config, UDC_CSR_NE_CFG);
	
	tmp = AMD_ADDBITS(tmp, ep->dev->cur_intf, UDC_CSR_NE_INTF);
	
	tmp = AMD_ADDBITS(tmp, ep->dev->cur_alt, UDC_CSR_NE_ALT);
	
	writel(tmp, &dev->csr->ne[udc_csr_epix]);

	
	tmp = readl(&dev->regs->ep_irqmsk);
	tmp &= AMD_UNMASK_BIT(ep->num);
	writel(tmp, &dev->regs->ep_irqmsk);

	/*
	 * clear NAK by writing CNAK
	 * avoid BNA for OUT DMA, don't clear NAK until DMA desc. written
	 */
	if (!use_dma || ep->in) {
		tmp = readl(&ep->regs->ctl);
		tmp |= AMD_BIT(UDC_EPCTL_CNAK);
		writel(tmp, &ep->regs->ctl);
		ep->naking = 0;
		UDC_QUEUE_CNAK(ep, ep->num);
	}
	tmp = desc->bEndpointAddress;
	DBG(dev, "%s enabled\n", usbep->name);

	spin_unlock_irqrestore(&dev->lock, iflags);
	return 0;
}

static void ep_init(struct udc_regs __iomem *regs, struct udc_ep *ep)
{
	u32		tmp;

	VDBG(ep->dev, "ep-%d reset\n", ep->num);
	ep->desc = NULL;
	ep->ep.desc = NULL;
	ep->ep.ops = &udc_ep_ops;
	INIT_LIST_HEAD(&ep->queue);

	ep->ep.maxpacket = (u16) ~0;
	
	tmp = readl(&ep->regs->ctl);
	tmp |= AMD_BIT(UDC_EPCTL_SNAK);
	writel(tmp, &ep->regs->ctl);
	ep->naking = 1;

	
	tmp = readl(&regs->ep_irqmsk);
	tmp |= AMD_BIT(ep->num);
	writel(tmp, &regs->ep_irqmsk);

	if (ep->in) {
		
		tmp = readl(&ep->regs->ctl);
		tmp &= AMD_UNMASK_BIT(UDC_EPCTL_P);
		writel(tmp, &ep->regs->ctl);

		tmp = readl(&ep->regs->sts);
		tmp |= AMD_BIT(UDC_EPSTS_IN);
		writel(tmp, &ep->regs->sts);

		
		tmp = readl(&ep->regs->ctl);
		tmp |= AMD_BIT(UDC_EPCTL_F);
		writel(tmp, &ep->regs->ctl);

	}
	
	writel(0, &ep->regs->desptr);
}

static int udc_ep_disable(struct usb_ep *usbep)
{
	struct udc_ep	*ep = NULL;
	unsigned long	iflags;

	if (!usbep)
		return -EINVAL;

	ep = container_of(usbep, struct udc_ep, ep);
	if (usbep->name == ep0_string || !ep->desc)
		return -EINVAL;

	DBG(ep->dev, "Disable ep-%d\n", ep->num);

	spin_lock_irqsave(&ep->dev->lock, iflags);
	udc_free_request(&ep->ep, &ep->bna_dummy_req->req);
	empty_req_queue(ep);
	ep_init(ep->dev->regs, ep);
	spin_unlock_irqrestore(&ep->dev->lock, iflags);

	return 0;
}

static struct usb_request *
udc_alloc_request(struct usb_ep *usbep, gfp_t gfp)
{
	struct udc_request	*req;
	struct udc_data_dma	*dma_desc;
	struct udc_ep	*ep;

	if (!usbep)
		return NULL;

	ep = container_of(usbep, struct udc_ep, ep);

	VDBG(ep->dev, "udc_alloc_req(): ep%d\n", ep->num);
	req = kzalloc(sizeof(struct udc_request), gfp);
	if (!req)
		return NULL;

	req->req.dma = DMA_DONT_USE;
	INIT_LIST_HEAD(&req->queue);

	if (ep->dma) {
		
		dma_desc = pci_pool_alloc(ep->dev->data_requests, gfp,
						&req->td_phys);
		if (!dma_desc) {
			kfree(req);
			return NULL;
		}

		VDBG(ep->dev, "udc_alloc_req: req = %p dma_desc = %p, "
				"td_phys = %lx\n",
				req, dma_desc,
				(unsigned long)req->td_phys);
		
		dma_desc->status = AMD_ADDBITS(dma_desc->status,
						UDC_DMA_STP_STS_BS_HOST_BUSY,
						UDC_DMA_STP_STS_BS);
		dma_desc->bufptr = cpu_to_le32(DMA_DONT_USE);
		req->td_data = dma_desc;
		req->td_data_last = NULL;
		req->chain_len = 1;
	}

	return &req->req;
}

static void
udc_free_request(struct usb_ep *usbep, struct usb_request *usbreq)
{
	struct udc_ep	*ep;
	struct udc_request	*req;

	if (!usbep || !usbreq)
		return;

	ep = container_of(usbep, struct udc_ep, ep);
	req = container_of(usbreq, struct udc_request, req);
	VDBG(ep->dev, "free_req req=%p\n", req);
	BUG_ON(!list_empty(&req->queue));
	if (req->td_data) {
		VDBG(ep->dev, "req->td_data=%p\n", req->td_data);

		
		if (req->chain_len > 1)
			udc_free_dma_chain(ep->dev, req);

		pci_pool_free(ep->dev->data_requests, req->td_data,
							req->td_phys);
	}
	kfree(req);
}

static void udc_init_bna_dummy(struct udc_request *req)
{
	if (req) {
		
		req->td_data->status |= AMD_BIT(UDC_DMA_IN_STS_L);
		
		req->td_data->next = req->td_phys;
		
		req->td_data->status
			= AMD_ADDBITS(req->td_data->status,
					UDC_DMA_STP_STS_BS_DMA_DONE,
					UDC_DMA_STP_STS_BS);
#ifdef UDC_VERBOSE
		pr_debug("bna desc = %p, sts = %08x\n",
			req->td_data, req->td_data->status);
#endif
	}
}

static struct udc_request *udc_alloc_bna_dummy(struct udc_ep *ep)
{
	struct udc_request *req = NULL;
	struct usb_request *_req = NULL;

	
	_req = udc_alloc_request(&ep->ep, GFP_ATOMIC);
	if (_req) {
		req = container_of(_req, struct udc_request, req);
		ep->bna_dummy_req = req;
		udc_init_bna_dummy(req);
	}
	return req;
}

static void
udc_txfifo_write(struct udc_ep *ep, struct usb_request *req)
{
	u8			*req_buf;
	u32			*buf;
	int			i, j;
	unsigned		bytes = 0;
	unsigned		remaining = 0;

	if (!req || !ep)
		return;

	req_buf = req->buf + req->actual;
	prefetch(req_buf);
	remaining = req->length - req->actual;

	buf = (u32 *) req_buf;

	bytes = ep->ep.maxpacket;
	if (bytes > remaining)
		bytes = remaining;

	
	for (i = 0; i < bytes / UDC_DWORD_BYTES; i++)
		writel(*(buf + i), ep->txfifo);

	/* remaining bytes must be written by byte access */
	for (j = 0; j < bytes % UDC_DWORD_BYTES; j++) {
		writeb((u8)(*(buf + i) >> (j << UDC_BITS_PER_BYTE_SHIFT)),
							ep->txfifo);
	}

	
	writel(0, &ep->regs->confirm);
}

static int udc_rxfifo_read_dwords(struct udc *dev, u32 *buf, int dwords)
{
	int i;

	VDBG(dev, "udc_read_dwords(): %d dwords\n", dwords);

	for (i = 0; i < dwords; i++)
		*(buf + i) = readl(dev->rxfifo);
	return 0;
}

static int udc_rxfifo_read_bytes(struct udc *dev, u8 *buf, int bytes)
{
	int i, j;
	u32 tmp;

	VDBG(dev, "udc_read_bytes(): %d bytes\n", bytes);

	
	for (i = 0; i < bytes / UDC_DWORD_BYTES; i++)
		*((u32 *)(buf + (i<<2))) = readl(dev->rxfifo);

	
	if (bytes % UDC_DWORD_BYTES) {
		tmp = readl(dev->rxfifo);
		for (j = 0; j < bytes % UDC_DWORD_BYTES; j++) {
			*(buf + (i<<2) + j) = (u8)(tmp & UDC_BYTE_MASK);
			tmp = tmp >> UDC_BITS_PER_BYTE;
		}
	}

	return 0;
}

static int
udc_rxfifo_read(struct udc_ep *ep, struct udc_request *req)
{
	u8 *buf;
	unsigned buf_space;
	unsigned bytes = 0;
	unsigned finished = 0;

	
	bytes = readl(&ep->regs->sts);
	bytes = AMD_GETBITS(bytes, UDC_EPSTS_RX_PKT_SIZE);

	buf_space = req->req.length - req->req.actual;
	buf = req->req.buf + req->req.actual;
	if (bytes > buf_space) {
		if ((buf_space % ep->ep.maxpacket) != 0) {
			DBG(ep->dev,
				"%s: rx %d bytes, rx-buf space = %d bytesn\n",
				ep->ep.name, bytes, buf_space);
			req->req.status = -EOVERFLOW;
		}
		bytes = buf_space;
	}
	req->req.actual += bytes;

	
	if (((bytes % ep->ep.maxpacket) != 0) || (!bytes)
		|| ((req->req.actual == req->req.length) && !req->req.zero))
		finished = 1;

	
	VDBG(ep->dev, "ep %s: rxfifo read %d bytes\n", ep->ep.name, bytes);
	udc_rxfifo_read_bytes(ep->dev, buf, bytes);

	return finished;
}

static int prep_dma(struct udc_ep *ep, struct udc_request *req, gfp_t gfp)
{
	int	retval = 0;
	u32	tmp;

	VDBG(ep->dev, "prep_dma\n");
	VDBG(ep->dev, "prep_dma ep%d req->td_data=%p\n",
			ep->num, req->td_data);

	
	req->td_data->bufptr = req->req.dma;

	
	req->td_data->status |= AMD_BIT(UDC_DMA_IN_STS_L);

	
	if (use_dma_ppb) {

		retval = udc_create_dma_chain(ep, req, ep->ep.maxpacket, gfp);
		if (retval != 0) {
			if (retval == -ENOMEM)
				DBG(ep->dev, "Out of DMA memory\n");
			return retval;
		}
		if (ep->in) {
			if (req->req.length == ep->ep.maxpacket) {
				
				req->td_data->status =
					AMD_ADDBITS(req->td_data->status,
						ep->ep.maxpacket,
						UDC_DMA_IN_STS_TXBYTES);

			}
		}

	}

	if (ep->in) {
		VDBG(ep->dev, "IN: use_dma_ppb=%d req->req.len=%d "
				"maxpacket=%d ep%d\n",
				use_dma_ppb, req->req.length,
				ep->ep.maxpacket, ep->num);
		/*
		 * if bytes < max packet then tx bytes must
		 * be written in packet per buffer mode
		 */
		if (!use_dma_ppb || req->req.length < ep->ep.maxpacket
				|| ep->num == UDC_EP0OUT_IX
				|| ep->num == UDC_EP0IN_IX) {
			
			req->td_data->status =
				AMD_ADDBITS(req->td_data->status,
						req->req.length,
						UDC_DMA_IN_STS_TXBYTES);
			
			req->td_data->status =
				AMD_ADDBITS(req->td_data->status,
						0,
						UDC_DMA_IN_STS_FRAMENUM);
		}
		
		req->td_data->status =
			AMD_ADDBITS(req->td_data->status,
				UDC_DMA_STP_STS_BS_HOST_BUSY,
				UDC_DMA_STP_STS_BS);
	} else {
		VDBG(ep->dev, "OUT set host ready\n");
		
		req->td_data->status =
			AMD_ADDBITS(req->td_data->status,
				UDC_DMA_STP_STS_BS_HOST_READY,
				UDC_DMA_STP_STS_BS);


			
			if (ep->naking) {
				tmp = readl(&ep->regs->ctl);
				tmp |= AMD_BIT(UDC_EPCTL_CNAK);
				writel(tmp, &ep->regs->ctl);
				ep->naking = 0;
				UDC_QUEUE_CNAK(ep, ep->num);
			}

	}

	return retval;
}

static void
complete_req(struct udc_ep *ep, struct udc_request *req, int sts)
__releases(ep->dev->lock)
__acquires(ep->dev->lock)
{
	struct udc		*dev;
	unsigned		halted;

	VDBG(ep->dev, "complete_req(): ep%d\n", ep->num);

	dev = ep->dev;
	
	if (ep->dma)
		usb_gadget_unmap_request(&dev->gadget, &req->req, ep->in);

	halted = ep->halted;
	ep->halted = 1;

	
	if (req->req.status == -EINPROGRESS)
		req->req.status = sts;

	
	list_del_init(&req->queue);

	VDBG(ep->dev, "req %p => complete %d bytes at %s with sts %d\n",
		&req->req, req->req.length, ep->ep.name, sts);

	spin_unlock(&dev->lock);
	req->req.complete(&ep->ep, &req->req);
	spin_lock(&dev->lock);
	ep->halted = halted;
}

static int udc_free_dma_chain(struct udc *dev, struct udc_request *req)
{

	int ret_val = 0;
	struct udc_data_dma	*td;
	struct udc_data_dma	*td_last = NULL;
	unsigned int i;

	DBG(dev, "free chain req = %p\n", req);

	
	td_last = req->td_data;
	td = phys_to_virt(td_last->next);

	for (i = 1; i < req->chain_len; i++) {

		pci_pool_free(dev->data_requests, td,
				(dma_addr_t) td_last->next);
		td_last = td;
		td = phys_to_virt(td_last->next);
	}

	return ret_val;
}

static struct udc_data_dma *udc_get_last_dma_desc(struct udc_request *req)
{
	struct udc_data_dma	*td;

	td = req->td_data;
	while (td && !(td->status & AMD_BIT(UDC_DMA_IN_STS_L)))
		td = phys_to_virt(td->next);

	return td;

}

static u32 udc_get_ppbdu_rxbytes(struct udc_request *req)
{
	struct udc_data_dma	*td;
	u32 count;

	td = req->td_data;
	
	count = AMD_GETBITS(td->status, UDC_DMA_OUT_STS_RXBYTES);

	while (td && !(td->status & AMD_BIT(UDC_DMA_IN_STS_L))) {
		td = phys_to_virt(td->next);
		
		if (td) {
			count += AMD_GETBITS(td->status,
				UDC_DMA_OUT_STS_RXBYTES);
		}
	}

	return count;

}

static int udc_create_dma_chain(
	struct udc_ep *ep,
	struct udc_request *req,
	unsigned long buf_len, gfp_t gfp_flags
)
{
	unsigned long bytes = req->req.length;
	unsigned int i;
	dma_addr_t dma_addr;
	struct udc_data_dma	*td = NULL;
	struct udc_data_dma	*last = NULL;
	unsigned long txbytes;
	unsigned create_new_chain = 0;
	unsigned len;

	VDBG(ep->dev, "udc_create_dma_chain: bytes=%ld buf_len=%ld\n",
			bytes, buf_len);
	dma_addr = DMA_DONT_USE;

	
	if (!ep->in)
		req->td_data->status &= AMD_CLEAR_BIT(UDC_DMA_IN_STS_L);

	
	len = req->req.length / ep->ep.maxpacket;
	if (req->req.length % ep->ep.maxpacket)
		len++;

	if (len > req->chain_len) {
		
		if (req->chain_len > 1)
			udc_free_dma_chain(ep->dev, req);
		req->chain_len = len;
		create_new_chain = 1;
	}

	td = req->td_data;
	
	for (i = buf_len; i < bytes; i += buf_len) {
		
		if (create_new_chain) {

			td = pci_pool_alloc(ep->dev->data_requests,
					gfp_flags, &dma_addr);
			if (!td)
				return -ENOMEM;

			td->status = 0;
		} else if (i == buf_len) {
			
			td = (struct udc_data_dma *) phys_to_virt(
						req->td_data->next);
			td->status = 0;
		} else {
			td = (struct udc_data_dma *) phys_to_virt(last->next);
			td->status = 0;
		}


		if (td)
			td->bufptr = req->req.dma + i; 
		else
			break;

		
		if ((bytes - i) >= buf_len) {
			txbytes = buf_len;
		} else {
			
			txbytes = bytes - i;
		}

		
		if (i == buf_len) {
			if (create_new_chain)
				req->td_data->next = dma_addr;
			
			if (ep->in) {
				
				req->td_data->status =
					AMD_ADDBITS(req->td_data->status,
							ep->ep.maxpacket,
							UDC_DMA_IN_STS_TXBYTES);
				
				td->status = AMD_ADDBITS(td->status,
							txbytes,
							UDC_DMA_IN_STS_TXBYTES);
			}
		} else {
			if (create_new_chain)
				last->next = dma_addr;
			if (ep->in) {
				
				td->status = AMD_ADDBITS(td->status,
							txbytes,
							UDC_DMA_IN_STS_TXBYTES);
			}
		}
		last = td;
	}
	
	if (td) {
		td->status |= AMD_BIT(UDC_DMA_IN_STS_L);
		
		req->td_data_last = td;
	}

	return 0;
}

static void udc_set_rde(struct udc *dev)
{
	u32 tmp;

	VDBG(dev, "udc_set_rde()\n");
	
	if (timer_pending(&udc_timer)) {
		set_rde = 0;
		mod_timer(&udc_timer, jiffies - 1);
	}
	
	tmp = readl(&dev->regs->ctl);
	tmp |= AMD_BIT(UDC_DEVCTL_RDE);
	writel(tmp, &dev->regs->ctl);
}

static int
udc_queue(struct usb_ep *usbep, struct usb_request *usbreq, gfp_t gfp)
{
	int			retval = 0;
	u8			open_rxfifo = 0;
	unsigned long		iflags;
	struct udc_ep		*ep;
	struct udc_request	*req;
	struct udc		*dev;
	u32			tmp;

	
	req = container_of(usbreq, struct udc_request, req);

	if (!usbep || !usbreq || !usbreq->complete || !usbreq->buf
			|| !list_empty(&req->queue))
		return -EINVAL;

	ep = container_of(usbep, struct udc_ep, ep);
	if (!ep->desc && (ep->num != 0 && ep->num != UDC_EP0OUT_IX))
		return -EINVAL;

	VDBG(ep->dev, "udc_queue(): ep%d-in=%d\n", ep->num, ep->in);
	dev = ep->dev;

	if (!dev->driver || dev->gadget.speed == USB_SPEED_UNKNOWN)
		return -ESHUTDOWN;

	
	if (ep->dma) {
		VDBG(dev, "DMA map req %p\n", req);
		retval = usb_gadget_map_request(&udc->gadget, usbreq, ep->in);
		if (retval)
			return retval;
	}

	VDBG(dev, "%s queue req %p, len %d req->td_data=%p buf %p\n",
			usbep->name, usbreq, usbreq->length,
			req->td_data, usbreq->buf);

	spin_lock_irqsave(&dev->lock, iflags);
	usbreq->actual = 0;
	usbreq->status = -EINPROGRESS;
	req->dma_done = 0;

	
	if (list_empty(&ep->queue)) {
		
		if (usbreq->length == 0) {
			
			complete_req(ep, req, 0);
			VDBG(dev, "%s: zlp\n", ep->ep.name);
			if (dev->set_cfg_not_acked) {
				tmp = readl(&dev->regs->ctl);
				tmp |= AMD_BIT(UDC_DEVCTL_CSR_DONE);
				writel(tmp, &dev->regs->ctl);
				dev->set_cfg_not_acked = 0;
			}
			
			if (dev->waiting_zlp_ack_ep0in) {
				
				tmp = readl(&dev->ep[UDC_EP0IN_IX].regs->ctl);
				tmp |= AMD_BIT(UDC_EPCTL_CNAK);
				writel(tmp, &dev->ep[UDC_EP0IN_IX].regs->ctl);
				dev->ep[UDC_EP0IN_IX].naking = 0;
				UDC_QUEUE_CNAK(&dev->ep[UDC_EP0IN_IX],
							UDC_EP0IN_IX);
				dev->waiting_zlp_ack_ep0in = 0;
			}
			goto finished;
		}
		if (ep->dma) {
			retval = prep_dma(ep, req, gfp);
			if (retval != 0)
				goto finished;
			
			if (ep->in) {
				
				req->td_data->status =
					AMD_ADDBITS(req->td_data->status,
						UDC_DMA_IN_STS_BS_HOST_READY,
						UDC_DMA_IN_STS_BS);
			}

			
			if (!ep->in) {
				
				if (timer_pending(&udc_timer)) {
					set_rde = 0;
					mod_timer(&udc_timer, jiffies - 1);
				}
				
				tmp = readl(&dev->regs->ctl);
				tmp &= AMD_UNMASK_BIT(UDC_DEVCTL_RDE);
				writel(tmp, &dev->regs->ctl);
				open_rxfifo = 1;

				if (ep->bna_occurred) {
					VDBG(dev, "copy to BNA dummy desc.\n");
					memcpy(ep->bna_dummy_req->td_data,
						req->td_data,
						sizeof(struct udc_data_dma));
				}
			}
			
			writel(req->td_phys, &ep->regs->desptr);

			
			if (ep->naking) {
				tmp = readl(&ep->regs->ctl);
				tmp |= AMD_BIT(UDC_EPCTL_CNAK);
				writel(tmp, &ep->regs->ctl);
				ep->naking = 0;
				UDC_QUEUE_CNAK(ep, ep->num);
			}

			if (ep->in) {
				
				tmp = readl(&dev->regs->ep_irqmsk);
				tmp &= AMD_UNMASK_BIT(ep->num);
				writel(tmp, &dev->regs->ep_irqmsk);
			}
		} else if (ep->in) {
				
				tmp = readl(&dev->regs->ep_irqmsk);
				tmp &= AMD_UNMASK_BIT(ep->num);
				writel(tmp, &dev->regs->ep_irqmsk);
			}

	} else if (ep->dma) {

		if (ep->in) {
			retval = prep_dma(ep, req, gfp);
			if (retval != 0)
				goto finished;
		}
	}
	VDBG(dev, "list_add\n");
	
	if (req) {

		list_add_tail(&req->queue, &ep->queue);

		
		if (open_rxfifo) {
			
			req->dma_going = 1;
			udc_set_rde(dev);
			if (ep->num != UDC_EP0OUT_IX)
				dev->data_ep_queued = 1;
		}
		
		if (!ep->in) {
			if (!use_dma && udc_rxfifo_pending) {
				DBG(dev, "udc_queue(): pending bytes in "
					"rxfifo after nyet\n");
				if (udc_rxfifo_read(ep, req)) {
					
					complete_req(ep, req, 0);
				}
				udc_rxfifo_pending = 0;

			}
		}
	}

finished:
	spin_unlock_irqrestore(&dev->lock, iflags);
	return retval;
}

static void empty_req_queue(struct udc_ep *ep)
{
	struct udc_request	*req;

	ep->halted = 1;
	while (!list_empty(&ep->queue)) {
		req = list_entry(ep->queue.next,
			struct udc_request,
			queue);
		complete_req(ep, req, -ESHUTDOWN);
	}
}

static int udc_dequeue(struct usb_ep *usbep, struct usb_request *usbreq)
{
	struct udc_ep		*ep;
	struct udc_request	*req;
	unsigned		halted;
	unsigned long		iflags;

	ep = container_of(usbep, struct udc_ep, ep);
	if (!usbep || !usbreq || (!ep->desc && (ep->num != 0
				&& ep->num != UDC_EP0OUT_IX)))
		return -EINVAL;

	req = container_of(usbreq, struct udc_request, req);

	spin_lock_irqsave(&ep->dev->lock, iflags);
	halted = ep->halted;
	ep->halted = 1;
	
	if (ep->queue.next == &req->queue) {
		if (ep->dma && req->dma_going) {
			if (ep->in)
				ep->cancel_transfer = 1;
			else {
				u32 tmp;
				u32 dma_sts;
				
				tmp = readl(&udc->regs->ctl);
				writel(tmp & AMD_UNMASK_BIT(UDC_DEVCTL_RDE),
							&udc->regs->ctl);
				dma_sts = AMD_GETBITS(req->td_data->status,
							UDC_DMA_OUT_STS_BS);
				if (dma_sts != UDC_DMA_OUT_STS_BS_HOST_READY)
					ep->cancel_transfer = 1;
				else {
					udc_init_bna_dummy(ep->req);
					writel(ep->bna_dummy_req->td_phys,
						&ep->regs->desptr);
				}
				writel(tmp, &udc->regs->ctl);
			}
		}
	}
	complete_req(ep, req, -ECONNRESET);
	ep->halted = halted;

	spin_unlock_irqrestore(&ep->dev->lock, iflags);
	return 0;
}

static int
udc_set_halt(struct usb_ep *usbep, int halt)
{
	struct udc_ep	*ep;
	u32 tmp;
	unsigned long iflags;
	int retval = 0;

	if (!usbep)
		return -EINVAL;

	pr_debug("set_halt %s: halt=%d\n", usbep->name, halt);

	ep = container_of(usbep, struct udc_ep, ep);
	if (!ep->desc && (ep->num != 0 && ep->num != UDC_EP0OUT_IX))
		return -EINVAL;
	if (!ep->dev->driver || ep->dev->gadget.speed == USB_SPEED_UNKNOWN)
		return -ESHUTDOWN;

	spin_lock_irqsave(&udc_stall_spinlock, iflags);
	
	if (halt) {
		if (ep->num == 0)
			ep->dev->stall_ep0in = 1;
		else {
			tmp = readl(&ep->regs->ctl);
			tmp |= AMD_BIT(UDC_EPCTL_S);
			writel(tmp, &ep->regs->ctl);
			ep->halted = 1;

			
			if (!timer_pending(&udc_pollstall_timer)) {
				udc_pollstall_timer.expires = jiffies +
					HZ * UDC_POLLSTALL_TIMER_USECONDS
					/ (1000 * 1000);
				if (!stop_pollstall_timer) {
					DBG(ep->dev, "start polltimer\n");
					add_timer(&udc_pollstall_timer);
				}
			}
		}
	} else {
		
		if (ep->halted) {
			tmp = readl(&ep->regs->ctl);
			
			tmp = tmp & AMD_CLEAR_BIT(UDC_EPCTL_S);
			
			tmp |= AMD_BIT(UDC_EPCTL_CNAK);
			writel(tmp, &ep->regs->ctl);
			ep->halted = 0;
			UDC_QUEUE_CNAK(ep, ep->num);
		}
	}
	spin_unlock_irqrestore(&udc_stall_spinlock, iflags);
	return retval;
}

static const struct usb_ep_ops udc_ep_ops = {
	.enable		= udc_ep_enable,
	.disable	= udc_ep_disable,

	.alloc_request	= udc_alloc_request,
	.free_request	= udc_free_request,

	.queue		= udc_queue,
	.dequeue	= udc_dequeue,

	.set_halt	= udc_set_halt,
	
};


static int udc_get_frame(struct usb_gadget *gadget)
{
	return -EOPNOTSUPP;
}

static int udc_wakeup(struct usb_gadget *gadget)
{
	struct udc		*dev;

	if (!gadget)
		return -EINVAL;
	dev = container_of(gadget, struct udc, gadget);
	udc_remote_wakeup(dev);

	return 0;
}

static int amd5536_start(struct usb_gadget_driver *driver,
		int (*bind)(struct usb_gadget *));
static int amd5536_stop(struct usb_gadget_driver *driver);
static const struct usb_gadget_ops udc_ops = {
	.wakeup		= udc_wakeup,
	.get_frame	= udc_get_frame,
	.start		= amd5536_start,
	.stop		= amd5536_stop,
};

static void make_ep_lists(struct udc *dev)
{
	
	INIT_LIST_HEAD(&dev->gadget.ep_list);
	list_add_tail(&dev->ep[UDC_EPIN_STATUS_IX].ep.ep_list,
						&dev->gadget.ep_list);
	list_add_tail(&dev->ep[UDC_EPIN_IX].ep.ep_list,
						&dev->gadget.ep_list);
	list_add_tail(&dev->ep[UDC_EPOUT_IX].ep.ep_list,
						&dev->gadget.ep_list);

	
	dev->ep[UDC_EPIN_STATUS_IX].fifo_depth = UDC_EPIN_SMALLINT_BUFF_SIZE;
	if (dev->gadget.speed == USB_SPEED_FULL)
		dev->ep[UDC_EPIN_IX].fifo_depth = UDC_FS_EPIN_BUFF_SIZE;
	else if (dev->gadget.speed == USB_SPEED_HIGH)
		dev->ep[UDC_EPIN_IX].fifo_depth = hs_tx_buf;
	dev->ep[UDC_EPOUT_IX].fifo_depth = UDC_RXFIFO_SIZE;
}

static int startup_registers(struct udc *dev)
{
	u32 tmp;

	
	udc_soft_reset(dev);

	
	udc_mask_unused_interrupts(dev);

	
	udc_basic_init(dev);
	
	udc_setup_endpoints(dev);

	
	tmp = readl(&dev->regs->cfg);
	if (use_fullspeed)
		tmp = AMD_ADDBITS(tmp, UDC_DEVCFG_SPD_FS, UDC_DEVCFG_SPD);
	else
		tmp = AMD_ADDBITS(tmp, UDC_DEVCFG_SPD_HS, UDC_DEVCFG_SPD);
	writel(tmp, &dev->regs->cfg);

	return 0;
}

static void udc_basic_init(struct udc *dev)
{
	u32	tmp;

	DBG(dev, "udc_basic_init()\n");

	dev->gadget.speed = USB_SPEED_UNKNOWN;

	
	if (timer_pending(&udc_timer)) {
		set_rde = 0;
		mod_timer(&udc_timer, jiffies - 1);
	}
	
	if (timer_pending(&udc_pollstall_timer))
		mod_timer(&udc_pollstall_timer, jiffies - 1);
	
	tmp = readl(&dev->regs->ctl);
	tmp &= AMD_UNMASK_BIT(UDC_DEVCTL_RDE);
	tmp &= AMD_UNMASK_BIT(UDC_DEVCTL_TDE);
	writel(tmp, &dev->regs->ctl);

	
	tmp = readl(&dev->regs->cfg);
	tmp |= AMD_BIT(UDC_DEVCFG_CSR_PRG);
	
	tmp |= AMD_BIT(UDC_DEVCFG_SP);
	
	tmp |= AMD_BIT(UDC_DEVCFG_RWKP);
	writel(tmp, &dev->regs->cfg);

	make_ep_lists(dev);

	dev->data_ep_enabled = 0;
	dev->data_ep_queued = 0;
}

static void udc_setup_endpoints(struct udc *dev)
{
	struct udc_ep	*ep;
	u32	tmp;
	u32	reg;

	DBG(dev, "udc_setup_endpoints()\n");

	
	tmp = readl(&dev->regs->sts);
	tmp = AMD_GETBITS(tmp, UDC_DEVSTS_ENUM_SPEED);
	if (tmp == UDC_DEVSTS_ENUM_SPEED_HIGH)
		dev->gadget.speed = USB_SPEED_HIGH;
	else if (tmp == UDC_DEVSTS_ENUM_SPEED_FULL)
		dev->gadget.speed = USB_SPEED_FULL;

	
	for (tmp = 0; tmp < UDC_EP_NUM; tmp++) {
		ep = &dev->ep[tmp];
		ep->dev = dev;
		ep->ep.name = ep_string[tmp];
		ep->num = tmp;
		
		ep->txfifo = dev->txfifo;

		
		if (tmp < UDC_EPIN_NUM) {
			ep->fifo_depth = UDC_TXFIFO_SIZE;
			ep->in = 1;
		} else {
			ep->fifo_depth = UDC_RXFIFO_SIZE;
			ep->in = 0;

		}
		ep->regs = &dev->ep_regs[tmp];
		if (!ep->desc)
			ep_init(dev->regs, ep);

		if (use_dma) {
			ep->dma = &dev->regs->ctl;

			
			if (tmp != UDC_EP0IN_IX && tmp != UDC_EP0OUT_IX
						&& tmp > UDC_EPIN_NUM) {
				
				reg = readl(&dev->ep[tmp].regs->ctl);
				reg |= AMD_BIT(UDC_EPCTL_SNAK);
				writel(reg, &dev->ep[tmp].regs->ctl);
				dev->ep[tmp].naking = 1;

			}
		}
	}
	
	if (dev->gadget.speed == USB_SPEED_FULL) {
		dev->ep[UDC_EP0IN_IX].ep.maxpacket = UDC_FS_EP0IN_MAX_PKT_SIZE;
		dev->ep[UDC_EP0OUT_IX].ep.maxpacket =
						UDC_FS_EP0OUT_MAX_PKT_SIZE;
	} else if (dev->gadget.speed == USB_SPEED_HIGH) {
		dev->ep[UDC_EP0IN_IX].ep.maxpacket = UDC_EP0IN_MAX_PKT_SIZE;
		dev->ep[UDC_EP0OUT_IX].ep.maxpacket = UDC_EP0OUT_MAX_PKT_SIZE;
	}

	dev->gadget.ep0 = &dev->ep[UDC_EP0IN_IX].ep;
	dev->ep[UDC_EP0IN_IX].halted = 0;
	INIT_LIST_HEAD(&dev->gadget.ep0->ep_list);

	
	dev->cur_config = 0;
	dev->cur_intf = 0;
	dev->cur_alt = 0;
}

static void usb_connect(struct udc *dev)
{

	dev_info(&dev->pdev->dev, "USB Connect\n");

	dev->connected = 1;

	
	udc_basic_init(dev);

	
	udc_enable_dev_setup_interrupts(dev);
}

static void usb_disconnect(struct udc *dev)
{

	dev_info(&dev->pdev->dev, "USB Disconnect\n");

	dev->connected = 0;

	
	udc_mask_unused_interrupts(dev);


	tasklet_schedule(&disconnect_tasklet);
}

static void udc_tasklet_disconnect(unsigned long par)
{
	struct udc *dev = (struct udc *)(*((struct udc **) par));
	u32 tmp;

	DBG(dev, "Tasklet disconnect\n");
	spin_lock_irq(&dev->lock);

	if (dev->driver) {
		spin_unlock(&dev->lock);
		dev->driver->disconnect(&dev->gadget);
		spin_lock(&dev->lock);

		
		for (tmp = 0; tmp < UDC_EP_NUM; tmp++)
			empty_req_queue(&dev->ep[tmp]);

	}

	
	ep_init(dev->regs,
			&dev->ep[UDC_EP0IN_IX]);


	if (!soft_reset_occured) {
		
		udc_soft_reset(dev);
		soft_reset_occured++;
	}

	
	udc_enable_dev_setup_interrupts(dev);
	
	if (use_fullspeed) {
		tmp = readl(&dev->regs->cfg);
		tmp = AMD_ADDBITS(tmp, UDC_DEVCFG_SPD_FS, UDC_DEVCFG_SPD);
		writel(tmp, &dev->regs->cfg);
	}

	spin_unlock_irq(&dev->lock);
}

static void udc_soft_reset(struct udc *dev)
{
	unsigned long	flags;

	DBG(dev, "Soft reset\n");
	writel(UDC_EPINT_MSK_DISABLE_ALL, &dev->regs->ep_irqsts);
	
	writel(UDC_DEV_MSK_DISABLE, &dev->regs->irqsts);

	spin_lock_irqsave(&udc_irq_spinlock, flags);
	writel(AMD_BIT(UDC_DEVCFG_SOFTRESET), &dev->regs->cfg);
	readl(&dev->regs->cfg);
	spin_unlock_irqrestore(&udc_irq_spinlock, flags);

}

static void udc_timer_function(unsigned long v)
{
	u32 tmp;

	spin_lock_irq(&udc_irq_spinlock);

	if (set_rde > 0) {
		if (set_rde > 1) {
			
			tmp = readl(&udc->regs->ctl);
			tmp |= AMD_BIT(UDC_DEVCTL_RDE);
			writel(tmp, &udc->regs->ctl);
			set_rde = -1;
		} else if (readl(&udc->regs->sts)
				& AMD_BIT(UDC_DEVSTS_RXFIFO_EMPTY)) {
			udc_timer.expires = jiffies + HZ/UDC_RDE_TIMER_DIV;
			if (!stop_timer)
				add_timer(&udc_timer);
		} else {
			set_rde++;
			
			udc_timer.expires = jiffies + HZ*UDC_RDE_TIMER_SECONDS;
			if (!stop_timer)
				add_timer(&udc_timer);
		}

	} else
		set_rde = -1; 
	spin_unlock_irq(&udc_irq_spinlock);
	if (stop_timer)
		complete(&on_exit);

}

static void udc_handle_halt_state(struct udc_ep *ep)
{
	u32 tmp;
	
	if (ep->halted == 1) {
		tmp = readl(&ep->regs->ctl);
		
		if (!(tmp & AMD_BIT(UDC_EPCTL_S))) {

			
			tmp |= AMD_BIT(UDC_EPCTL_CNAK);
			writel(tmp, &ep->regs->ctl);
			ep->halted = 0;
			UDC_QUEUE_CNAK(ep, ep->num);
		}
	}
}

static void udc_pollstall_timer_function(unsigned long v)
{
	struct udc_ep *ep;
	int halted = 0;

	spin_lock_irq(&udc_stall_spinlock);
	ep = &udc->ep[UDC_EPIN_IX];
	udc_handle_halt_state(ep);
	if (ep->halted)
		halted = 1;
	
	ep = &udc->ep[UDC_EPOUT_IX];
	udc_handle_halt_state(ep);
	if (ep->halted)
		halted = 1;

	
	if (!stop_pollstall_timer && halted) {
		udc_pollstall_timer.expires = jiffies +
					HZ * UDC_POLLSTALL_TIMER_USECONDS
					/ (1000 * 1000);
		add_timer(&udc_pollstall_timer);
	}
	spin_unlock_irq(&udc_stall_spinlock);

	if (stop_pollstall_timer)
		complete(&on_pollstall_exit);
}

static void activate_control_endpoints(struct udc *dev)
{
	u32 tmp;

	DBG(dev, "activate_control_endpoints\n");

	
	tmp = readl(&dev->ep[UDC_EP0IN_IX].regs->ctl);
	tmp |= AMD_BIT(UDC_EPCTL_F);
	writel(tmp, &dev->ep[UDC_EP0IN_IX].regs->ctl);

	
	dev->ep[UDC_EP0IN_IX].in = 1;
	dev->ep[UDC_EP0OUT_IX].in = 0;

	
	tmp = readl(&dev->ep[UDC_EP0IN_IX].regs->bufin_framenum);
	if (dev->gadget.speed == USB_SPEED_FULL)
		tmp = AMD_ADDBITS(tmp, UDC_FS_EPIN0_BUFF_SIZE,
					UDC_EPIN_BUFF_SIZE);
	else if (dev->gadget.speed == USB_SPEED_HIGH)
		tmp = AMD_ADDBITS(tmp, UDC_EPIN0_BUFF_SIZE,
					UDC_EPIN_BUFF_SIZE);
	writel(tmp, &dev->ep[UDC_EP0IN_IX].regs->bufin_framenum);

	
	tmp = readl(&dev->ep[UDC_EP0IN_IX].regs->bufout_maxpkt);
	if (dev->gadget.speed == USB_SPEED_FULL)
		tmp = AMD_ADDBITS(tmp, UDC_FS_EP0IN_MAX_PKT_SIZE,
					UDC_EP_MAX_PKT_SIZE);
	else if (dev->gadget.speed == USB_SPEED_HIGH)
		tmp = AMD_ADDBITS(tmp, UDC_EP0IN_MAX_PKT_SIZE,
				UDC_EP_MAX_PKT_SIZE);
	writel(tmp, &dev->ep[UDC_EP0IN_IX].regs->bufout_maxpkt);

	
	tmp = readl(&dev->ep[UDC_EP0OUT_IX].regs->bufout_maxpkt);
	if (dev->gadget.speed == USB_SPEED_FULL)
		tmp = AMD_ADDBITS(tmp, UDC_FS_EP0OUT_MAX_PKT_SIZE,
					UDC_EP_MAX_PKT_SIZE);
	else if (dev->gadget.speed == USB_SPEED_HIGH)
		tmp = AMD_ADDBITS(tmp, UDC_EP0OUT_MAX_PKT_SIZE,
					UDC_EP_MAX_PKT_SIZE);
	writel(tmp, &dev->ep[UDC_EP0OUT_IX].regs->bufout_maxpkt);

	
	tmp = readl(&dev->csr->ne[0]);
	if (dev->gadget.speed == USB_SPEED_FULL)
		tmp = AMD_ADDBITS(tmp, UDC_FS_EP0OUT_MAX_PKT_SIZE,
					UDC_CSR_NE_MAX_PKT);
	else if (dev->gadget.speed == USB_SPEED_HIGH)
		tmp = AMD_ADDBITS(tmp, UDC_EP0OUT_MAX_PKT_SIZE,
					UDC_CSR_NE_MAX_PKT);
	writel(tmp, &dev->csr->ne[0]);

	if (use_dma) {
		dev->ep[UDC_EP0OUT_IX].td->status |=
			AMD_BIT(UDC_DMA_OUT_STS_L);
		
		writel(dev->ep[UDC_EP0OUT_IX].td_stp_dma,
			&dev->ep[UDC_EP0OUT_IX].regs->subptr);
		writel(dev->ep[UDC_EP0OUT_IX].td_phys,
			&dev->ep[UDC_EP0OUT_IX].regs->desptr);
		
		if (timer_pending(&udc_timer)) {
			set_rde = 0;
			mod_timer(&udc_timer, jiffies - 1);
		}
		
		if (timer_pending(&udc_pollstall_timer))
			mod_timer(&udc_pollstall_timer, jiffies - 1);
		
		tmp = readl(&dev->regs->ctl);
		tmp |= AMD_BIT(UDC_DEVCTL_MODE)
				| AMD_BIT(UDC_DEVCTL_RDE)
				| AMD_BIT(UDC_DEVCTL_TDE);
		if (use_dma_bufferfill_mode)
			tmp |= AMD_BIT(UDC_DEVCTL_BF);
		else if (use_dma_ppb_du)
			tmp |= AMD_BIT(UDC_DEVCTL_DU);
		writel(tmp, &dev->regs->ctl);
	}

	
	tmp = readl(&dev->ep[UDC_EP0IN_IX].regs->ctl);
	tmp |= AMD_BIT(UDC_EPCTL_CNAK);
	writel(tmp, &dev->ep[UDC_EP0IN_IX].regs->ctl);
	dev->ep[UDC_EP0IN_IX].naking = 0;
	UDC_QUEUE_CNAK(&dev->ep[UDC_EP0IN_IX], UDC_EP0IN_IX);

	
	tmp = readl(&dev->ep[UDC_EP0OUT_IX].regs->ctl);
	tmp |= AMD_BIT(UDC_EPCTL_CNAK);
	writel(tmp, &dev->ep[UDC_EP0OUT_IX].regs->ctl);
	dev->ep[UDC_EP0OUT_IX].naking = 0;
	UDC_QUEUE_CNAK(&dev->ep[UDC_EP0OUT_IX], UDC_EP0OUT_IX);
}

static int setup_ep0(struct udc *dev)
{
	activate_control_endpoints(dev);
	
	udc_enable_ep0_interrupts(dev);
	
	udc_enable_dev_setup_interrupts(dev);

	return 0;
}

static int amd5536_start(struct usb_gadget_driver *driver,
		int (*bind)(struct usb_gadget *))
{
	struct udc		*dev = udc;
	int			retval;
	u32 tmp;

	if (!driver || !bind || !driver->setup
			|| driver->max_speed < USB_SPEED_HIGH)
		return -EINVAL;
	if (!dev)
		return -ENODEV;
	if (dev->driver)
		return -EBUSY;

	driver->driver.bus = NULL;
	dev->driver = driver;
	dev->gadget.dev.driver = &driver->driver;

	retval = bind(&dev->gadget);

	dev->ep[UDC_EP0OUT_IX].ep.driver_data =
		dev->ep[UDC_EP0IN_IX].ep.driver_data;

	if (retval) {
		DBG(dev, "binding to %s returning %d\n",
				driver->driver.name, retval);
		dev->driver = NULL;
		dev->gadget.dev.driver = NULL;
		return retval;
	}

	
	setup_ep0(dev);

	
	tmp = readl(&dev->regs->ctl);
	tmp = tmp & AMD_CLEAR_BIT(UDC_DEVCTL_SD);
	writel(tmp, &dev->regs->ctl);

	usb_connect(dev);

	return 0;
}

static void
shutdown(struct udc *dev, struct usb_gadget_driver *driver)
__releases(dev->lock)
__acquires(dev->lock)
{
	int tmp;

	if (dev->gadget.speed != USB_SPEED_UNKNOWN) {
		spin_unlock(&dev->lock);
		driver->disconnect(&dev->gadget);
		spin_lock(&dev->lock);
	}

	
	udc_basic_init(dev);
	for (tmp = 0; tmp < UDC_EP_NUM; tmp++)
		empty_req_queue(&dev->ep[tmp]);

	udc_setup_endpoints(dev);
}

static int amd5536_stop(struct usb_gadget_driver *driver)
{
	struct udc	*dev = udc;
	unsigned long	flags;
	u32 tmp;

	if (!dev)
		return -ENODEV;
	if (!driver || driver != dev->driver || !driver->unbind)
		return -EINVAL;

	spin_lock_irqsave(&dev->lock, flags);
	udc_mask_unused_interrupts(dev);
	shutdown(dev, driver);
	spin_unlock_irqrestore(&dev->lock, flags);

	driver->unbind(&dev->gadget);
	dev->gadget.dev.driver = NULL;
	dev->driver = NULL;

	
	tmp = readl(&dev->regs->ctl);
	tmp |= AMD_BIT(UDC_DEVCTL_SD);
	writel(tmp, &dev->regs->ctl);


	DBG(dev, "%s: unregistered\n", driver->driver.name);

	return 0;
}

static void udc_process_cnak_queue(struct udc *dev)
{
	u32 tmp;
	u32 reg;

	
	DBG(dev, "CNAK pending queue processing\n");
	for (tmp = 0; tmp < UDC_EPIN_NUM_USED; tmp++) {
		if (cnak_pending & (1 << tmp)) {
			DBG(dev, "CNAK pending for ep%d\n", tmp);
			
			reg = readl(&dev->ep[tmp].regs->ctl);
			reg |= AMD_BIT(UDC_EPCTL_CNAK);
			writel(reg, &dev->ep[tmp].regs->ctl);
			dev->ep[tmp].naking = 0;
			UDC_QUEUE_CNAK(&dev->ep[tmp], dev->ep[tmp].num);
		}
	}
	
	if (cnak_pending & (1 << UDC_EP0OUT_IX)) {
		DBG(dev, "CNAK pending for ep%d\n", UDC_EP0OUT_IX);
		
		reg = readl(&dev->ep[UDC_EP0OUT_IX].regs->ctl);
		reg |= AMD_BIT(UDC_EPCTL_CNAK);
		writel(reg, &dev->ep[UDC_EP0OUT_IX].regs->ctl);
		dev->ep[UDC_EP0OUT_IX].naking = 0;
		UDC_QUEUE_CNAK(&dev->ep[UDC_EP0OUT_IX],
				dev->ep[UDC_EP0OUT_IX].num);
	}
}

static void udc_ep0_set_rde(struct udc *dev)
{
	if (use_dma) {
		if (!dev->data_ep_enabled || dev->data_ep_queued) {
			udc_set_rde(dev);
		} else {
			if (set_rde != 0 && !timer_pending(&udc_timer)) {
				udc_timer.expires =
					jiffies + HZ/UDC_RDE_TIMER_DIV;
				set_rde = 1;
				if (!stop_timer)
					add_timer(&udc_timer);
			}
		}
	}
}


static irqreturn_t udc_data_out_isr(struct udc *dev, int ep_ix)
{
	irqreturn_t		ret_val = IRQ_NONE;
	u32			tmp;
	struct udc_ep		*ep;
	struct udc_request	*req;
	unsigned int		count;
	struct udc_data_dma	*td = NULL;
	unsigned		dma_done;

	VDBG(dev, "ep%d irq\n", ep_ix);
	ep = &dev->ep[ep_ix];

	tmp = readl(&ep->regs->sts);
	if (use_dma) {
		
		if (tmp & AMD_BIT(UDC_EPSTS_BNA)) {
			DBG(dev, "BNA ep%dout occurred - DESPTR = %x\n",
					ep->num, readl(&ep->regs->desptr));
			
			writel(tmp | AMD_BIT(UDC_EPSTS_BNA), &ep->regs->sts);
			if (!ep->cancel_transfer)
				ep->bna_occurred = 1;
			else
				ep->cancel_transfer = 0;
			ret_val = IRQ_HANDLED;
			goto finished;
		}
	}
	
	if (tmp & AMD_BIT(UDC_EPSTS_HE)) {
		dev_err(&dev->pdev->dev, "HE ep%dout occurred\n", ep->num);

		
		writel(tmp | AMD_BIT(UDC_EPSTS_HE), &ep->regs->sts);
		ret_val = IRQ_HANDLED;
		goto finished;
	}

	if (!list_empty(&ep->queue)) {

		
		req = list_entry(ep->queue.next,
			struct udc_request, queue);
	} else {
		req = NULL;
		udc_rxfifo_pending = 1;
	}
	VDBG(dev, "req = %p\n", req);
	
	if (!use_dma) {

		
		if (req && udc_rxfifo_read(ep, req)) {
			ret_val = IRQ_HANDLED;

			
			complete_req(ep, req, 0);
			
			if (!list_empty(&ep->queue) && !ep->halted) {
				req = list_entry(ep->queue.next,
					struct udc_request, queue);
			} else
				req = NULL;
		}

	
	} else if (!ep->cancel_transfer && req != NULL) {
		ret_val = IRQ_HANDLED;

		
		if (!use_dma_ppb) {
			dma_done = AMD_GETBITS(req->td_data->status,
						UDC_DMA_OUT_STS_BS);
		
		} else {
			if (ep->bna_occurred) {
				VDBG(dev, "Recover desc. from BNA dummy\n");
				memcpy(req->td_data, ep->bna_dummy_req->td_data,
						sizeof(struct udc_data_dma));
				ep->bna_occurred = 0;
				udc_init_bna_dummy(ep->req);
			}
			td = udc_get_last_dma_desc(req);
			dma_done = AMD_GETBITS(td->status, UDC_DMA_OUT_STS_BS);
		}
		if (dma_done == UDC_DMA_OUT_STS_BS_DMA_DONE) {
			
			if (!use_dma_ppb) {
				
				count = AMD_GETBITS(req->td_data->status,
						UDC_DMA_OUT_STS_RXBYTES);
				VDBG(dev, "rx bytes=%u\n", count);
			
			} else {
				VDBG(dev, "req->td_data=%p\n", req->td_data);
				VDBG(dev, "last desc = %p\n", td);
				
				if (use_dma_ppb_du) {
					
					count = udc_get_ppbdu_rxbytes(req);
				} else {
					
					count = AMD_GETBITS(td->status,
						UDC_DMA_OUT_STS_RXBYTES);
					if (!count && req->req.length
						== UDC_DMA_MAXPACKET) {
						count = UDC_DMA_MAXPACKET;
					}
				}
				VDBG(dev, "last desc rx bytes=%u\n", count);
			}

			tmp = req->req.length - req->req.actual;
			if (count > tmp) {
				if ((tmp % ep->ep.maxpacket) != 0) {
					DBG(dev, "%s: rx %db, space=%db\n",
						ep->ep.name, count, tmp);
					req->req.status = -EOVERFLOW;
				}
				count = tmp;
			}
			req->req.actual += count;
			req->dma_going = 0;
			
			complete_req(ep, req, 0);

			
			if (!list_empty(&ep->queue) && !ep->halted) {
				req = list_entry(ep->queue.next,
					struct udc_request,
					queue);
				if (req->dma_going == 0) {
					
					if (prep_dma(ep, req, GFP_ATOMIC) != 0)
						goto finished;
					
					writel(req->td_phys,
						&ep->regs->desptr);
					req->dma_going = 1;
					
					udc_set_rde(dev);
				}
			} else {
				if (ep->bna_dummy_req) {
					
					writel(ep->bna_dummy_req->td_phys,
						&ep->regs->desptr);
					ep->bna_occurred = 0;
				}

				if (set_rde != 0
						&& !timer_pending(&udc_timer)) {
					udc_timer.expires =
						jiffies
						+ HZ*UDC_RDE_TIMER_SECONDS;
					set_rde = 1;
					if (!stop_timer)
						add_timer(&udc_timer);
				}
				if (ep->num != UDC_EP0OUT_IX)
					dev->data_ep_queued = 0;
			}

		} else {
			udc_set_rde(dev);
		}

	} else if (ep->cancel_transfer) {
		ret_val = IRQ_HANDLED;
		ep->cancel_transfer = 0;
	}

	
	if (cnak_pending) {
		
		if (readl(&dev->regs->sts) & AMD_BIT(UDC_DEVSTS_RXFIFO_EMPTY))
			udc_process_cnak_queue(dev);
	}

	
	writel(UDC_EPSTS_OUT_CLEAR, &ep->regs->sts);
finished:
	return ret_val;
}

static irqreturn_t udc_data_in_isr(struct udc *dev, int ep_ix)
{
	irqreturn_t ret_val = IRQ_NONE;
	u32 tmp;
	u32 epsts;
	struct udc_ep *ep;
	struct udc_request *req;
	struct udc_data_dma *td;
	unsigned dma_done;
	unsigned len;

	ep = &dev->ep[ep_ix];

	epsts = readl(&ep->regs->sts);
	if (use_dma) {
		
		if (epsts & AMD_BIT(UDC_EPSTS_BNA)) {
			dev_err(&dev->pdev->dev,
				"BNA ep%din occurred - DESPTR = %08lx\n",
				ep->num,
				(unsigned long) readl(&ep->regs->desptr));

			
			writel(epsts, &ep->regs->sts);
			ret_val = IRQ_HANDLED;
			goto finished;
		}
	}
	
	if (epsts & AMD_BIT(UDC_EPSTS_HE)) {
		dev_err(&dev->pdev->dev,
			"HE ep%dn occurred - DESPTR = %08lx\n",
			ep->num, (unsigned long) readl(&ep->regs->desptr));

		
		writel(epsts | AMD_BIT(UDC_EPSTS_HE), &ep->regs->sts);
		ret_val = IRQ_HANDLED;
		goto finished;
	}

	
	if (epsts & AMD_BIT(UDC_EPSTS_TDC)) {
		VDBG(dev, "TDC set- completion\n");
		ret_val = IRQ_HANDLED;
		if (!ep->cancel_transfer && !list_empty(&ep->queue)) {
			req = list_entry(ep->queue.next,
					struct udc_request, queue);
			if (use_dma_ppb_du) {
				td = udc_get_last_dma_desc(req);
				if (td) {
					dma_done =
						AMD_GETBITS(td->status,
						UDC_DMA_IN_STS_BS);
					
					req->req.actual = req->req.length;
				}
			} else {
				
				req->req.actual = req->req.length;
			}

			if (req->req.actual == req->req.length) {
				
				complete_req(ep, req, 0);
				req->dma_going = 0;
				
				if (list_empty(&ep->queue)) {
					
					tmp = readl(&dev->regs->ep_irqmsk);
					tmp |= AMD_BIT(ep->num);
					writel(tmp, &dev->regs->ep_irqmsk);
				}
			}
		}
		ep->cancel_transfer = 0;

	}
	if ((epsts & AMD_BIT(UDC_EPSTS_IN))
			&& !(epsts & AMD_BIT(UDC_EPSTS_TDC))) {
		ret_val = IRQ_HANDLED;
		if (!list_empty(&ep->queue)) {
			
			req = list_entry(ep->queue.next,
					struct udc_request, queue);
			
			if (!use_dma) {
				
				udc_txfifo_write(ep, &req->req);
				len = req->req.length - req->req.actual;
				if (len > ep->ep.maxpacket)
					len = ep->ep.maxpacket;
				req->req.actual += len;
				if (req->req.actual == req->req.length
					|| (len != ep->ep.maxpacket)) {
					
					complete_req(ep, req, 0);
				}
			
			} else if (req && !req->dma_going) {
				VDBG(dev, "IN DMA : req=%p req->td_data=%p\n",
					req, req->td_data);
				if (req->td_data) {

					req->dma_going = 1;

					if (use_dma_ppb && req->req.length >
							ep->ep.maxpacket) {
						req->td_data->status &=
							AMD_CLEAR_BIT(
							UDC_DMA_IN_STS_L);
					}

					
					writel(req->td_phys, &ep->regs->desptr);

					
					req->td_data->status =
						AMD_ADDBITS(
						req->td_data->status,
						UDC_DMA_IN_STS_BS_HOST_READY,
						UDC_DMA_IN_STS_BS);

					
					tmp = readl(&ep->regs->ctl);
					tmp |= AMD_BIT(UDC_EPCTL_P);
					writel(tmp, &ep->regs->ctl);
				}
			}

		} else if (!use_dma && ep->in) {
			
			tmp = readl(
				&dev->regs->ep_irqmsk);
			tmp |= AMD_BIT(ep->num);
			writel(tmp,
				&dev->regs->ep_irqmsk);
		}
	}
	
	writel(epsts, &ep->regs->sts);

finished:
	return ret_val;

}

static irqreturn_t udc_control_out_isr(struct udc *dev)
__releases(dev->lock)
__acquires(dev->lock)
{
	irqreturn_t ret_val = IRQ_NONE;
	u32 tmp;
	int setup_supported;
	u32 count;
	int set = 0;
	struct udc_ep	*ep;
	struct udc_ep	*ep_tmp;

	ep = &dev->ep[UDC_EP0OUT_IX];

	
	writel(AMD_BIT(UDC_EPINT_OUT_EP0), &dev->regs->ep_irqsts);

	tmp = readl(&dev->ep[UDC_EP0OUT_IX].regs->sts);
	
	if (tmp & AMD_BIT(UDC_EPSTS_BNA)) {
		VDBG(dev, "ep0: BNA set\n");
		writel(AMD_BIT(UDC_EPSTS_BNA),
			&dev->ep[UDC_EP0OUT_IX].regs->sts);
		ep->bna_occurred = 1;
		ret_val = IRQ_HANDLED;
		goto finished;
	}

	
	tmp = AMD_GETBITS(tmp, UDC_EPSTS_OUT);
	VDBG(dev, "data_typ = %x\n", tmp);

	
	if (tmp == UDC_EPSTS_OUT_SETUP) {
		ret_val = IRQ_HANDLED;

		ep->dev->stall_ep0in = 0;
		dev->waiting_zlp_ack_ep0in = 0;

		
		tmp = readl(&dev->ep[UDC_EP0IN_IX].regs->ctl);
		tmp |= AMD_BIT(UDC_EPCTL_SNAK);
		writel(tmp, &dev->ep[UDC_EP0IN_IX].regs->ctl);
		dev->ep[UDC_EP0IN_IX].naking = 1;
		
		if (use_dma) {

			
			writel(UDC_EPSTS_OUT_CLEAR,
				&dev->ep[UDC_EP0OUT_IX].regs->sts);

			setup_data.data[0] =
				dev->ep[UDC_EP0OUT_IX].td_stp->data12;
			setup_data.data[1] =
				dev->ep[UDC_EP0OUT_IX].td_stp->data34;
			
			dev->ep[UDC_EP0OUT_IX].td_stp->status =
					UDC_DMA_STP_STS_BS_HOST_READY;
		} else {
			
			udc_rxfifo_read_dwords(dev, setup_data.data, 2);
		}

		
		if ((setup_data.request.bRequestType & USB_DIR_IN) != 0) {
			dev->gadget.ep0 = &dev->ep[UDC_EP0IN_IX].ep;
			
			udc_ep0_set_rde(dev);
			set = 0;
		} else {
			dev->gadget.ep0 = &dev->ep[UDC_EP0OUT_IX].ep;
			if (ep->bna_dummy_req) {
				
				writel(ep->bna_dummy_req->td_phys,
					&dev->ep[UDC_EP0OUT_IX].regs->desptr);
				ep->bna_occurred = 0;
			}

			set = 1;
			dev->ep[UDC_EP0OUT_IX].naking = 1;
			set_rde = 1;
			if (!timer_pending(&udc_timer)) {
				udc_timer.expires = jiffies +
							HZ/UDC_RDE_TIMER_DIV;
				if (!stop_timer)
					add_timer(&udc_timer);
			}
		}

		if (setup_data.data[0] == UDC_MSCRES_DWORD0
				&& setup_data.data[1] == UDC_MSCRES_DWORD1) {
			DBG(dev, "MSC Reset\n");
			ep_tmp = &udc->ep[UDC_EPIN_IX];
			udc_set_halt(&ep_tmp->ep, 0);
			ep_tmp = &udc->ep[UDC_EPOUT_IX];
			udc_set_halt(&ep_tmp->ep, 0);
		}

		
		spin_unlock(&dev->lock);
		setup_supported = dev->driver->setup(&dev->gadget,
						&setup_data.request);
		spin_lock(&dev->lock);

		tmp = readl(&dev->ep[UDC_EP0IN_IX].regs->ctl);
		
		if (setup_supported >= 0 && setup_supported <
				UDC_EP0IN_MAXPACKET) {
			
			tmp |= AMD_BIT(UDC_EPCTL_CNAK);
			writel(tmp, &dev->ep[UDC_EP0IN_IX].regs->ctl);
			dev->ep[UDC_EP0IN_IX].naking = 0;
			UDC_QUEUE_CNAK(&dev->ep[UDC_EP0IN_IX], UDC_EP0IN_IX);

		
		} else if (setup_supported < 0) {
			tmp |= AMD_BIT(UDC_EPCTL_S);
			writel(tmp, &dev->ep[UDC_EP0IN_IX].regs->ctl);
		} else
			dev->waiting_zlp_ack_ep0in = 1;


		
		if (!set) {
			tmp = readl(&dev->ep[UDC_EP0OUT_IX].regs->ctl);
			tmp |= AMD_BIT(UDC_EPCTL_CNAK);
			writel(tmp, &dev->ep[UDC_EP0OUT_IX].regs->ctl);
			dev->ep[UDC_EP0OUT_IX].naking = 0;
			UDC_QUEUE_CNAK(&dev->ep[UDC_EP0OUT_IX], UDC_EP0OUT_IX);
		}

		if (!use_dma) {
			
			writel(UDC_EPSTS_OUT_CLEAR,
				&dev->ep[UDC_EP0OUT_IX].regs->sts);
		}

	
	} else if (tmp == UDC_EPSTS_OUT_DATA) {
		
		writel(UDC_EPSTS_OUT_CLEAR, &dev->ep[UDC_EP0OUT_IX].regs->sts);

		
		if (use_dma) {
			
			if (list_empty(&dev->ep[UDC_EP0OUT_IX].queue)) {
				VDBG(dev, "ZLP\n");

				
				dev->ep[UDC_EP0OUT_IX].td->status =
					AMD_ADDBITS(
					dev->ep[UDC_EP0OUT_IX].td->status,
					UDC_DMA_OUT_STS_BS_HOST_READY,
					UDC_DMA_OUT_STS_BS);
				
				udc_ep0_set_rde(dev);
				ret_val = IRQ_HANDLED;

			} else {
				
				ret_val |= udc_data_out_isr(dev, UDC_EP0OUT_IX);
				
				writel(dev->ep[UDC_EP0OUT_IX].td_phys,
					&dev->ep[UDC_EP0OUT_IX].regs->desptr);
				
				udc_ep0_set_rde(dev);
			}
		} else {

			
			count = readl(&dev->ep[UDC_EP0OUT_IX].regs->sts);
			count = AMD_GETBITS(count, UDC_EPSTS_RX_PKT_SIZE);
			
			count = 0;

			
			if (count != 0) {
				ret_val |= udc_data_out_isr(dev, UDC_EP0OUT_IX);
			} else {
				
				readl(&dev->ep[UDC_EP0OUT_IX].regs->confirm);
				ret_val = IRQ_HANDLED;
			}
		}
	}

	
	if (cnak_pending) {
		
		if (readl(&dev->regs->sts) & AMD_BIT(UDC_DEVSTS_RXFIFO_EMPTY))
			udc_process_cnak_queue(dev);
	}

finished:
	return ret_val;
}

static irqreturn_t udc_control_in_isr(struct udc *dev)
{
	irqreturn_t ret_val = IRQ_NONE;
	u32 tmp;
	struct udc_ep *ep;
	struct udc_request *req;
	unsigned len;

	ep = &dev->ep[UDC_EP0IN_IX];

	
	writel(AMD_BIT(UDC_EPINT_IN_EP0), &dev->regs->ep_irqsts);

	tmp = readl(&dev->ep[UDC_EP0IN_IX].regs->sts);
	
	if (tmp & AMD_BIT(UDC_EPSTS_TDC)) {
		VDBG(dev, "isr: TDC clear\n");
		ret_val = IRQ_HANDLED;

		
		writel(AMD_BIT(UDC_EPSTS_TDC),
				&dev->ep[UDC_EP0IN_IX].regs->sts);

	
	} else if (tmp & AMD_BIT(UDC_EPSTS_IN)) {
		ret_val = IRQ_HANDLED;

		if (ep->dma) {
			
			writel(AMD_BIT(UDC_EPSTS_IN),
				&dev->ep[UDC_EP0IN_IX].regs->sts);
		}
		if (dev->stall_ep0in) {
			DBG(dev, "stall ep0in\n");
			
			tmp = readl(&ep->regs->ctl);
			tmp |= AMD_BIT(UDC_EPCTL_S);
			writel(tmp, &ep->regs->ctl);
		} else {
			if (!list_empty(&ep->queue)) {
				
				req = list_entry(ep->queue.next,
						struct udc_request, queue);

				if (ep->dma) {
					
					writel(req->td_phys, &ep->regs->desptr);
					
					req->td_data->status =
						AMD_ADDBITS(
						req->td_data->status,
						UDC_DMA_STP_STS_BS_HOST_READY,
						UDC_DMA_STP_STS_BS);

					
					tmp =
					readl(&dev->ep[UDC_EP0IN_IX].regs->ctl);
					tmp |= AMD_BIT(UDC_EPCTL_P);
					writel(tmp,
					&dev->ep[UDC_EP0IN_IX].regs->ctl);

					
					req->req.actual = req->req.length;

					
					complete_req(ep, req, 0);

				} else {
					
					udc_txfifo_write(ep, &req->req);

					
					len = req->req.length - req->req.actual;
					if (len > ep->ep.maxpacket)
						len = ep->ep.maxpacket;

					req->req.actual += len;
					if (req->req.actual == req->req.length
						|| (len != ep->ep.maxpacket)) {
						
						complete_req(ep, req, 0);
					}
				}

			}
		}
		ep->halted = 0;
		dev->stall_ep0in = 0;
		if (!ep->dma) {
			
			writel(AMD_BIT(UDC_EPSTS_IN),
				&dev->ep[UDC_EP0IN_IX].regs->sts);
		}
	}

	return ret_val;
}


static irqreturn_t udc_dev_isr(struct udc *dev, u32 dev_irq)
__releases(dev->lock)
__acquires(dev->lock)
{
	irqreturn_t ret_val = IRQ_NONE;
	u32 tmp;
	u32 cfg;
	struct udc_ep *ep;
	u16 i;
	u8 udc_csr_epix;

	
	if (dev_irq & AMD_BIT(UDC_DEVINT_SC)) {
		ret_val = IRQ_HANDLED;

		
		tmp = readl(&dev->regs->sts);
		cfg = AMD_GETBITS(tmp, UDC_DEVSTS_CFG);
		DBG(dev, "SET_CONFIG interrupt: config=%d\n", cfg);
		dev->cur_config = cfg;
		dev->set_cfg_not_acked = 1;

		
		memset(&setup_data, 0 , sizeof(union udc_setup_data));
		setup_data.request.bRequest = USB_REQ_SET_CONFIGURATION;
		setup_data.request.wValue = cpu_to_le16(dev->cur_config);

		
		for (i = 0; i < UDC_EP_NUM; i++) {
			ep = &dev->ep[i];
			if (ep->in) {

				
				udc_csr_epix = ep->num;


			
			} else {
				
				udc_csr_epix = ep->num - UDC_CSR_EP_OUT_IX_OFS;
			}

			tmp = readl(&dev->csr->ne[udc_csr_epix]);
			
			tmp = AMD_ADDBITS(tmp, ep->dev->cur_config,
						UDC_CSR_NE_CFG);
			
			writel(tmp, &dev->csr->ne[udc_csr_epix]);

			
			ep->halted = 0;
			tmp = readl(&ep->regs->ctl);
			tmp = tmp & AMD_CLEAR_BIT(UDC_EPCTL_S);
			writel(tmp, &ep->regs->ctl);
		}
		
		spin_unlock(&dev->lock);
		tmp = dev->driver->setup(&dev->gadget, &setup_data.request);
		spin_lock(&dev->lock);

	} 
	if (dev_irq & AMD_BIT(UDC_DEVINT_SI)) {
		ret_val = IRQ_HANDLED;

		dev->set_cfg_not_acked = 1;
		
		tmp = readl(&dev->regs->sts);
		dev->cur_alt = AMD_GETBITS(tmp, UDC_DEVSTS_ALT);
		dev->cur_intf = AMD_GETBITS(tmp, UDC_DEVSTS_INTF);

		
		memset(&setup_data, 0 , sizeof(union udc_setup_data));
		setup_data.request.bRequest = USB_REQ_SET_INTERFACE;
		setup_data.request.bRequestType = USB_RECIP_INTERFACE;
		setup_data.request.wValue = cpu_to_le16(dev->cur_alt);
		setup_data.request.wIndex = cpu_to_le16(dev->cur_intf);

		DBG(dev, "SET_INTERFACE interrupt: alt=%d intf=%d\n",
				dev->cur_alt, dev->cur_intf);

		
		for (i = 0; i < UDC_EP_NUM; i++) {
			ep = &dev->ep[i];
			if (ep->in) {

				
				udc_csr_epix = ep->num;


			
			} else {
				
				udc_csr_epix = ep->num - UDC_CSR_EP_OUT_IX_OFS;
			}

			
			
			tmp = readl(&dev->csr->ne[udc_csr_epix]);
			
			tmp = AMD_ADDBITS(tmp, ep->dev->cur_intf,
						UDC_CSR_NE_INTF);
			
			
			tmp = AMD_ADDBITS(tmp, ep->dev->cur_alt,
						UDC_CSR_NE_ALT);
			
			writel(tmp, &dev->csr->ne[udc_csr_epix]);

			
			ep->halted = 0;
			tmp = readl(&ep->regs->ctl);
			tmp = tmp & AMD_CLEAR_BIT(UDC_EPCTL_S);
			writel(tmp, &ep->regs->ctl);
		}

		
		spin_unlock(&dev->lock);
		tmp = dev->driver->setup(&dev->gadget, &setup_data.request);
		spin_lock(&dev->lock);

	} 
	if (dev_irq & AMD_BIT(UDC_DEVINT_UR)) {
		DBG(dev, "USB Reset interrupt\n");
		ret_val = IRQ_HANDLED;

		
		soft_reset_occured = 0;

		dev->waiting_zlp_ack_ep0in = 0;
		dev->set_cfg_not_acked = 0;

		
		udc_mask_unused_interrupts(dev);

		
		spin_unlock(&dev->lock);
		if (dev->sys_suspended && dev->driver->resume) {
			dev->driver->resume(&dev->gadget);
			dev->sys_suspended = 0;
		}
		dev->driver->disconnect(&dev->gadget);
		spin_lock(&dev->lock);

		
		empty_req_queue(&dev->ep[UDC_EP0IN_IX]);
		ep_init(dev->regs, &dev->ep[UDC_EP0IN_IX]);

		
		tmp = readl(&dev->regs->sts);
		if (!(tmp & AMD_BIT(UDC_DEVSTS_RXFIFO_EMPTY))
				&& !soft_reset_after_usbreset_occured) {
			udc_soft_reset(dev);
			soft_reset_after_usbreset_occured++;
		}

		DBG(dev, "DMA machine reset\n");
		tmp = readl(&dev->regs->cfg);
		writel(tmp | AMD_BIT(UDC_DEVCFG_DMARST), &dev->regs->cfg);
		writel(tmp, &dev->regs->cfg);

		
		udc_basic_init(dev);

		
		udc_enable_dev_setup_interrupts(dev);

		
		tmp = readl(&dev->regs->irqmsk);
		tmp &= AMD_UNMASK_BIT(UDC_DEVINT_US);
		writel(tmp, &dev->regs->irqmsk);

	} 
	if (dev_irq & AMD_BIT(UDC_DEVINT_US)) {
		DBG(dev, "USB Suspend interrupt\n");
		ret_val = IRQ_HANDLED;
		if (dev->driver->suspend) {
			spin_unlock(&dev->lock);
			dev->sys_suspended = 1;
			dev->driver->suspend(&dev->gadget);
			spin_lock(&dev->lock);
		}
	} 
	if (dev_irq & AMD_BIT(UDC_DEVINT_ENUM)) {
		DBG(dev, "ENUM interrupt\n");
		ret_val = IRQ_HANDLED;
		soft_reset_after_usbreset_occured = 0;

		
		empty_req_queue(&dev->ep[UDC_EP0IN_IX]);
		ep_init(dev->regs, &dev->ep[UDC_EP0IN_IX]);

		
		udc_setup_endpoints(dev);
		dev_info(&dev->pdev->dev, "Connect: %s\n",
			 usb_speed_string(dev->gadget.speed));

		
		activate_control_endpoints(dev);

		
		udc_enable_ep0_interrupts(dev);
	}
	
	if (dev_irq & AMD_BIT(UDC_DEVINT_SVC)) {
		DBG(dev, "USB SVC interrupt\n");
		ret_val = IRQ_HANDLED;

		
		tmp = readl(&dev->regs->sts);
		if (!(tmp & AMD_BIT(UDC_DEVSTS_SESSVLD))) {
			
			tmp = readl(&dev->regs->irqmsk);
			tmp |= AMD_BIT(UDC_DEVINT_US);
			writel(tmp, &dev->regs->irqmsk);
			DBG(dev, "USB Disconnect (session valid low)\n");
			
			usb_disconnect(udc);
		}

	}

	return ret_val;
}

static irqreturn_t udc_irq(int irq, void *pdev)
{
	struct udc *dev = pdev;
	u32 reg;
	u16 i;
	u32 ep_irq;
	irqreturn_t ret_val = IRQ_NONE;

	spin_lock(&dev->lock);

	
	reg = readl(&dev->regs->ep_irqsts);
	if (reg) {
		if (reg & AMD_BIT(UDC_EPINT_OUT_EP0))
			ret_val |= udc_control_out_isr(dev);
		if (reg & AMD_BIT(UDC_EPINT_IN_EP0))
			ret_val |= udc_control_in_isr(dev);

		for (i = 1; i < UDC_EP_NUM; i++) {
			ep_irq = 1 << i;
			if (!(reg & ep_irq) || i == UDC_EPINT_OUT_EP0)
				continue;

			
			writel(ep_irq, &dev->regs->ep_irqsts);

			
			if (i > UDC_EPIN_NUM)
				ret_val |= udc_data_out_isr(dev, i);
			else
				ret_val |= udc_data_in_isr(dev, i);
		}

	}


	
	reg = readl(&dev->regs->irqsts);
	if (reg) {
		
		writel(reg, &dev->regs->irqsts);
		ret_val |= udc_dev_isr(dev, reg);
	}


	spin_unlock(&dev->lock);
	return ret_val;
}

static void gadget_release(struct device *pdev)
{
	struct amd5536udc *dev = dev_get_drvdata(pdev);
	kfree(dev);
}

static void udc_remove(struct udc *dev)
{
	
	stop_timer++;
	if (timer_pending(&udc_timer))
		wait_for_completion(&on_exit);
	if (udc_timer.data)
		del_timer_sync(&udc_timer);
	
	stop_pollstall_timer++;
	if (timer_pending(&udc_pollstall_timer))
		wait_for_completion(&on_pollstall_exit);
	if (udc_pollstall_timer.data)
		del_timer_sync(&udc_pollstall_timer);
	udc = NULL;
}

static void udc_pci_remove(struct pci_dev *pdev)
{
	struct udc		*dev;

	dev = pci_get_drvdata(pdev);

	usb_del_gadget_udc(&udc->gadget);
	
	BUG_ON(dev->driver != NULL);

	
	if (dev->data_requests)
		pci_pool_destroy(dev->data_requests);

	if (dev->stp_requests) {
		
		pci_pool_free(dev->stp_requests,
			dev->ep[UDC_EP0OUT_IX].td_stp,
			dev->ep[UDC_EP0OUT_IX].td_stp_dma);
		pci_pool_free(dev->stp_requests,
			dev->ep[UDC_EP0OUT_IX].td,
			dev->ep[UDC_EP0OUT_IX].td_phys);

		pci_pool_destroy(dev->stp_requests);
	}

	
	writel(AMD_BIT(UDC_DEVCFG_SOFTRESET), &dev->regs->cfg);
	if (dev->irq_registered)
		free_irq(pdev->irq, dev);
	if (dev->regs)
		iounmap(dev->regs);
	if (dev->mem_region)
		release_mem_region(pci_resource_start(pdev, 0),
				pci_resource_len(pdev, 0));
	if (dev->active)
		pci_disable_device(pdev);

	device_unregister(&dev->gadget.dev);
	pci_set_drvdata(pdev, NULL);

	udc_remove(dev);
}

static int init_dma_pools(struct udc *dev)
{
	struct udc_stp_dma	*td_stp;
	struct udc_data_dma	*td_data;
	int retval;

	
	if (use_dma_ppb) {
		use_dma_bufferfill_mode = 0;
	} else {
		use_dma_ppb_du = 0;
		use_dma_bufferfill_mode = 1;
	}

	
	dev->data_requests = dma_pool_create("data_requests", NULL,
		sizeof(struct udc_data_dma), 0, 0);
	if (!dev->data_requests) {
		DBG(dev, "can't get request data pool\n");
		retval = -ENOMEM;
		goto finished;
	}

	
	dev->ep[UDC_EP0IN_IX].dma = &dev->regs->ctl;

	
	dev->stp_requests = dma_pool_create("setup requests", NULL,
		sizeof(struct udc_stp_dma), 0, 0);
	if (!dev->stp_requests) {
		DBG(dev, "can't get stp request pool\n");
		retval = -ENOMEM;
		goto finished;
	}
	
	td_stp = dma_pool_alloc(dev->stp_requests, GFP_KERNEL,
				&dev->ep[UDC_EP0OUT_IX].td_stp_dma);
	if (td_stp == NULL) {
		retval = -ENOMEM;
		goto finished;
	}
	dev->ep[UDC_EP0OUT_IX].td_stp = td_stp;

	
	td_data = dma_pool_alloc(dev->stp_requests, GFP_KERNEL,
				&dev->ep[UDC_EP0OUT_IX].td_phys);
	if (td_data == NULL) {
		retval = -ENOMEM;
		goto finished;
	}
	dev->ep[UDC_EP0OUT_IX].td = td_data;
	return 0;

finished:
	return retval;
}

static int udc_pci_probe(
	struct pci_dev *pdev,
	const struct pci_device_id *id
)
{
	struct udc		*dev;
	unsigned long		resource;
	unsigned long		len;
	int			retval = 0;

	
	if (udc) {
		dev_dbg(&pdev->dev, "already probed\n");
		return -EBUSY;
	}

	
	dev = kzalloc(sizeof(struct udc), GFP_KERNEL);
	if (!dev) {
		retval = -ENOMEM;
		goto finished;
	}

	
	if (pci_enable_device(pdev) < 0) {
		kfree(dev);
		dev = NULL;
		retval = -ENODEV;
		goto finished;
	}
	dev->active = 1;

	
	resource = pci_resource_start(pdev, 0);
	len = pci_resource_len(pdev, 0);

	if (!request_mem_region(resource, len, name)) {
		dev_dbg(&pdev->dev, "pci device used already\n");
		kfree(dev);
		dev = NULL;
		retval = -EBUSY;
		goto finished;
	}
	dev->mem_region = 1;

	dev->virt_addr = ioremap_nocache(resource, len);
	if (dev->virt_addr == NULL) {
		dev_dbg(&pdev->dev, "start address cannot be mapped\n");
		kfree(dev);
		dev = NULL;
		retval = -EFAULT;
		goto finished;
	}

	if (!pdev->irq) {
		dev_err(&dev->pdev->dev, "irq not set\n");
		kfree(dev);
		dev = NULL;
		retval = -ENODEV;
		goto finished;
	}

	spin_lock_init(&dev->lock);
	
	dev->csr = dev->virt_addr + UDC_CSR_ADDR;
	
	dev->regs = dev->virt_addr + UDC_DEVCFG_ADDR;
	
	dev->ep_regs = dev->virt_addr + UDC_EPREGS_ADDR;
	
	dev->rxfifo = (u32 __iomem *)(dev->virt_addr + UDC_RXFIFO_ADDR);
	dev->txfifo = (u32 __iomem *)(dev->virt_addr + UDC_TXFIFO_ADDR);

	if (request_irq(pdev->irq, udc_irq, IRQF_SHARED, name, dev) != 0) {
		dev_dbg(&dev->pdev->dev, "request_irq(%d) fail\n", pdev->irq);
		kfree(dev);
		dev = NULL;
		retval = -EBUSY;
		goto finished;
	}
	dev->irq_registered = 1;

	pci_set_drvdata(pdev, dev);

	
	dev->chiprev = pdev->revision;

	pci_set_master(pdev);
	pci_try_set_mwi(pdev);

	
	if (use_dma) {
		retval = init_dma_pools(dev);
		if (retval != 0)
			goto finished;
	}

	dev->phys_addr = resource;
	dev->irq = pdev->irq;
	dev->pdev = pdev;
	dev->gadget.dev.parent = &pdev->dev;
	dev->gadget.dev.dma_mask = pdev->dev.dma_mask;

	
	if (udc_probe(dev) == 0)
		return 0;

finished:
	if (dev)
		udc_pci_remove(pdev);
	return retval;
}

static int udc_probe(struct udc *dev)
{
	char		tmp[128];
	u32		reg;
	int		retval;

	
	udc_timer.data = 0;
	udc_pollstall_timer.data = 0;

	
	dev->gadget.ops = &udc_ops;

	dev_set_name(&dev->gadget.dev, "gadget");
	dev->gadget.dev.release = gadget_release;
	dev->gadget.name = name;
	dev->gadget.max_speed = USB_SPEED_HIGH;

	
	startup_registers(dev);

	dev_info(&dev->pdev->dev, "%s\n", mod_desc);

	snprintf(tmp, sizeof tmp, "%d", dev->irq);
	dev_info(&dev->pdev->dev,
		"irq %s, pci mem %08lx, chip rev %02x(Geode5536 %s)\n",
		tmp, dev->phys_addr, dev->chiprev,
		(dev->chiprev == UDC_HSA0_REV) ? "A0" : "B1");
	strcpy(tmp, UDC_DRIVER_VERSION_STRING);
	if (dev->chiprev == UDC_HSA0_REV) {
		dev_err(&dev->pdev->dev, "chip revision is A0; too old\n");
		retval = -ENODEV;
		goto finished;
	}
	dev_info(&dev->pdev->dev,
		"driver version: %s(for Geode5536 B1)\n", tmp);
	udc = dev;

	retval = usb_add_gadget_udc(&udc->pdev->dev, &dev->gadget);
	if (retval)
		goto finished;

	retval = device_register(&dev->gadget.dev);
	if (retval) {
		usb_del_gadget_udc(&dev->gadget);
		put_device(&dev->gadget.dev);
		goto finished;
	}

	
	init_timer(&udc_timer);
	udc_timer.function = udc_timer_function;
	udc_timer.data = 1;
	
	init_timer(&udc_pollstall_timer);
	udc_pollstall_timer.function = udc_pollstall_timer_function;
	udc_pollstall_timer.data = 1;

	
	reg = readl(&dev->regs->ctl);
	reg |= AMD_BIT(UDC_DEVCTL_SD);
	writel(reg, &dev->regs->ctl);

	
	print_regs(dev);

	return 0;

finished:
	return retval;
}

static int udc_remote_wakeup(struct udc *dev)
{
	unsigned long flags;
	u32 tmp;

	DBG(dev, "UDC initiates remote wakeup\n");

	spin_lock_irqsave(&dev->lock, flags);

	tmp = readl(&dev->regs->ctl);
	tmp |= AMD_BIT(UDC_DEVCTL_RES);
	writel(tmp, &dev->regs->ctl);
	tmp &= AMD_CLEAR_BIT(UDC_DEVCTL_RES);
	writel(tmp, &dev->regs->ctl);

	spin_unlock_irqrestore(&dev->lock, flags);
	return 0;
}

static DEFINE_PCI_DEVICE_TABLE(pci_id) = {
	{
		PCI_DEVICE(PCI_VENDOR_ID_AMD, 0x2096),
		.class =	(PCI_CLASS_SERIAL_USB << 8) | 0xfe,
		.class_mask =	0xffffffff,
	},
	{},
};
MODULE_DEVICE_TABLE(pci, pci_id);

static struct pci_driver udc_pci_driver = {
	.name =		(char *) name,
	.id_table =	pci_id,
	.probe =	udc_pci_probe,
	.remove =	udc_pci_remove,
};

static int __init init(void)
{
	return pci_register_driver(&udc_pci_driver);
}
module_init(init);

static void __exit cleanup(void)
{
	pci_unregister_driver(&udc_pci_driver);
}
module_exit(cleanup);

MODULE_DESCRIPTION(UDC_MOD_DESCRIPTION);
MODULE_AUTHOR("Thomas Dahlmann");
MODULE_LICENSE("GPL");

