/*
 * linux/drivers/usb/gadget/pxa27x_udc.h
 * Intel PXA27x on-chip full speed USB device controller
 *
 * Inspired by original driver by Frank Becker, David Brownell, and others.
 * Copyright (C) 2008 Robert Jarzmik
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __LINUX_USB_GADGET_PXA27X_H
#define __LINUX_USB_GADGET_PXA27X_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/usb/otg.h>

#define UDCCR		0x0000		
#define UDCICR0		0x0004		
#define UDCICR1		0x0008		
#define UDCISR0		0x000C		
#define UDCISR1		0x0010		
#define UDCFNR		0x0014		
#define UDCOTGICR	0x0018		
#define UP2OCR		0x0020		
#define UP3OCR		0x0024		
#define UDCCSRn(x)	(0x0100 + ((x)<<2)) 
#define UDCBCRn(x)	(0x0200 + ((x)<<2)) 
#define UDCDRn(x)	(0x0300 + ((x)<<2)) 
#define UDCCRn(x)	(0x0400 + ((x)<<2)) 

#define UDCCR_OEN	(1 << 31)	
#define UDCCR_AALTHNP	(1 << 30)	
#define UDCCR_AHNP	(1 << 29)	
#define UDCCR_BHNP	(1 << 28)	
#define UDCCR_DWRE	(1 << 16)	
#define UDCCR_ACN	(0x03 << 11)	
#define UDCCR_ACN_S	11
#define UDCCR_AIN	(0x07 << 8)	
#define UDCCR_AIN_S	8
#define UDCCR_AAISN	(0x07 << 5)	
#define UDCCR_AAISN_S	5
#define UDCCR_SMAC	(1 << 4)	
#define UDCCR_EMCE	(1 << 3)	
#define UDCCR_UDR	(1 << 2)	
#define UDCCR_UDA	(1 << 1)	
#define UDCCR_UDE	(1 << 0)	

#define UDCICR_INT(n, intr) (((intr) & 0x03) << (((n) & 0x0F) * 2))
#define UDCICR1_IECC	(1 << 31)	
#define UDCICR1_IESOF	(1 << 30)	
#define UDCICR1_IERU	(1 << 29)	
#define UDCICR1_IESU	(1 << 28)	
#define UDCICR1_IERS	(1 << 27)	
#define UDCICR_FIFOERR	(1 << 1)	
#define UDCICR_PKTCOMPL	(1 << 0)	
#define UDCICR_INT_MASK	(UDCICR_FIFOERR | UDCICR_PKTCOMPL)

#define UDCISR_INT(n, intr) (((intr) & 0x03) << (((n) & 0x0F) * 2))
#define UDCISR1_IRCC	(1 << 31)	
#define UDCISR1_IRSOF	(1 << 30)	
#define UDCISR1_IRRU	(1 << 29)	
#define UDCISR1_IRSU	(1 << 28)	
#define UDCISR1_IRRS	(1 << 27)	
#define UDCISR_INT_MASK	(UDCICR_FIFOERR | UDCICR_PKTCOMPL)

#define UDCOTGICR_IESF	(1 << 24)	
#define UDCOTGICR_IEXR	(1 << 17)	
#define UDCOTGICR_IEXF	(1 << 16)	
#define UDCOTGICR_IEVV40R (1 << 9)	
#define UDCOTGICR_IEVV40F (1 << 8)	
#define UDCOTGICR_IEVV44R (1 << 7)	
#define UDCOTGICR_IEVV44F (1 << 6)	
#define UDCOTGICR_IESVR	(1 << 5)	
#define UDCOTGICR_IESVF	(1 << 4)	
#define UDCOTGICR_IESDR	(1 << 3)	
#define UDCOTGICR_IESDF	(1 << 2)	
#define UDCOTGICR_IEIDR	(1 << 1)	
#define UDCOTGICR_IEIDF	(1 << 0)	

#define UP2OCR_CPVEN	(1 << 0)	
#define UP2OCR_CPVPE	(1 << 1)	
					
#define UP2OCR_DPPDE	(1 << 2)	
#define UP2OCR_DMPDE	(1 << 3)	
#define UP2OCR_DPPUE	(1 << 4)	
#define UP2OCR_DMPUE	(1 << 5)	
#define UP2OCR_DPPUBE	(1 << 6)	
#define UP2OCR_DMPUBE	(1 << 7)	
#define UP2OCR_EXSP	(1 << 8)	
#define UP2OCR_EXSUS	(1 << 9)	
#define UP2OCR_IDON	(1 << 10)	
#define UP2OCR_HXS	(1 << 16)	
#define UP2OCR_HXOE	(1 << 17)	
#define UP2OCR_SEOS	(1 << 24)	

#define UDCCSR0_ACM	(1 << 9)	
#define UDCCSR0_AREN	(1 << 8)	
#define UDCCSR0_SA	(1 << 7)	
#define UDCCSR0_RNE	(1 << 6)	
#define UDCCSR0_FST	(1 << 5)	
#define UDCCSR0_SST	(1 << 4)	
#define UDCCSR0_DME	(1 << 3)	
#define UDCCSR0_FTF	(1 << 2)	
#define UDCCSR0_IPR	(1 << 1)	
#define UDCCSR0_OPC	(1 << 0)	

#define UDCCSR_DPE	(1 << 9)	
#define UDCCSR_FEF	(1 << 8)	
#define UDCCSR_SP	(1 << 7)	
#define UDCCSR_BNE	(1 << 6)	
#define UDCCSR_BNF	(1 << 6)	
#define UDCCSR_FST	(1 << 5)	
#define UDCCSR_SST	(1 << 4)	
#define UDCCSR_DME	(1 << 3)	
#define UDCCSR_TRN	(1 << 2)	
#define UDCCSR_PC	(1 << 1)	
#define UDCCSR_FS	(1 << 0)	

#define UDCCONR_CN	(0x03 << 25)	
#define UDCCONR_CN_S	25
#define UDCCONR_IN	(0x07 << 22)	
#define UDCCONR_IN_S	22
#define UDCCONR_AISN	(0x07 << 19)	
#define UDCCONR_AISN_S	19
#define UDCCONR_EN	(0x0f << 15)	
#define UDCCONR_EN_S	15
#define UDCCONR_ET	(0x03 << 13)	
#define UDCCONR_ET_S	13
#define UDCCONR_ET_INT	(0x03 << 13)	
#define UDCCONR_ET_BULK	(0x02 << 13)	
#define UDCCONR_ET_ISO	(0x01 << 13)	
#define UDCCONR_ET_NU	(0x00 << 13)	
#define UDCCONR_ED	(1 << 12)	
#define UDCCONR_MPS	(0x3ff << 2)	
#define UDCCONR_MPS_S	2
#define UDCCONR_DE	(1 << 1)	
#define UDCCONR_EE	(1 << 0)	

#define UDCCR_MASK_BITS (UDCCR_OEN | UDCCR_SMAC | UDCCR_UDR | UDCCR_UDE)
#define UDCCSR_WR_MASK	(UDCCSR_DME | UDCCSR_FST)
#define UDC_FNR_MASK	(0x7ff)
#define UDC_BCR_MASK	(0x3ff)

#define ofs_UDCCR(ep)	(UDCCRn(ep->idx))
#define ofs_UDCCSR(ep)	(UDCCSRn(ep->idx))
#define ofs_UDCBCR(ep)	(UDCBCRn(ep->idx))
#define ofs_UDCDR(ep)	(UDCDRn(ep->idx))

#define udc_ep_readl(ep, reg)	\
	__raw_readl((ep)->dev->regs + ofs_##reg(ep))
#define udc_ep_writel(ep, reg, value)	\
	__raw_writel((value), ep->dev->regs + ofs_##reg(ep))
#define udc_ep_readb(ep, reg)	\
	__raw_readb((ep)->dev->regs + ofs_##reg(ep))
#define udc_ep_writeb(ep, reg, value)	\
	__raw_writeb((value), ep->dev->regs + ofs_##reg(ep))
#define udc_readl(dev, reg)	\
	__raw_readl((dev)->regs + (reg))
#define udc_writel(udc, reg, value)	\
	__raw_writel((value), (udc)->regs + (reg))

#define UDCCSR_MASK		(UDCCSR_FST | UDCCSR_DME)
#define UDCCISR0_EP_MASK	~0
#define UDCCISR1_EP_MASK	0xffff
#define UDCCSR0_CTRL_REQ_MASK	(UDCCSR0_OPC | UDCCSR0_SA | UDCCSR0_RNE)

#define EPIDX(ep)	(ep->idx)
#define EPADDR(ep)	(ep->addr)
#define EPXFERTYPE(ep)	(ep->type)
#define EPNAME(ep)	(ep->name)
#define is_ep0(ep)	(!ep->idx)
#define EPXFERTYPE_is_ISO(ep) (EPXFERTYPE(ep) == USB_ENDPOINT_XFER_ISOC)


#define USB_EP_DEF(addr, bname, dir, type, maxpkt) \
{ .usb_ep = { .name = bname, .ops = &pxa_ep_ops, .maxpacket = maxpkt, }, \
  .desc = {	.bEndpointAddress = addr | (dir ? USB_DIR_IN : 0), \
		.bmAttributes = type, \
		.wMaxPacketSize = maxpkt, }, \
  .dev = &memory \
}
#define USB_EP_BULK(addr, bname, dir) \
  USB_EP_DEF(addr, bname, dir, USB_ENDPOINT_XFER_BULK, BULK_FIFO_SIZE)
#define USB_EP_ISO(addr, bname, dir) \
  USB_EP_DEF(addr, bname, dir, USB_ENDPOINT_XFER_ISOC, ISO_FIFO_SIZE)
#define USB_EP_INT(addr, bname, dir) \
  USB_EP_DEF(addr, bname, dir, USB_ENDPOINT_XFER_INT, INT_FIFO_SIZE)
#define USB_EP_IN_BULK(n)	USB_EP_BULK(n, "ep" #n "in-bulk", 1)
#define USB_EP_OUT_BULK(n)	USB_EP_BULK(n, "ep" #n "out-bulk", 0)
#define USB_EP_IN_ISO(n)	USB_EP_ISO(n,  "ep" #n "in-iso", 1)
#define USB_EP_OUT_ISO(n)	USB_EP_ISO(n,  "ep" #n "out-iso", 0)
#define USB_EP_IN_INT(n)	USB_EP_INT(n,  "ep" #n "in-int", 1)
#define USB_EP_CTRL		USB_EP_DEF(0,  "ep0", 0, 0, EP0_FIFO_SIZE)

#define PXA_EP_DEF(_idx, _addr, dir, _type, maxpkt, _config, iface, altset) \
{ \
	.dev = &memory, \
	.name = "ep" #_idx, \
	.idx = _idx, .enabled = 0, \
	.dir_in = dir, .addr = _addr, \
	.config = _config, .interface = iface, .alternate = altset, \
	.type = _type, .fifo_size = maxpkt, \
}
#define PXA_EP_BULK(_idx, addr, dir, config, iface, alt) \
  PXA_EP_DEF(_idx, addr, dir, USB_ENDPOINT_XFER_BULK, BULK_FIFO_SIZE, \
		config, iface, alt)
#define PXA_EP_ISO(_idx, addr, dir, config, iface, alt) \
  PXA_EP_DEF(_idx, addr, dir, USB_ENDPOINT_XFER_ISOC, ISO_FIFO_SIZE, \
		config, iface, alt)
#define PXA_EP_INT(_idx, addr, dir, config, iface, alt) \
  PXA_EP_DEF(_idx, addr, dir, USB_ENDPOINT_XFER_INT, INT_FIFO_SIZE, \
		config, iface, alt)
#define PXA_EP_IN_BULK(i, adr, c, f, a)		PXA_EP_BULK(i, adr, 1, c, f, a)
#define PXA_EP_OUT_BULK(i, adr, c, f, a)	PXA_EP_BULK(i, adr, 0, c, f, a)
#define PXA_EP_IN_ISO(i, adr, c, f, a)		PXA_EP_ISO(i, adr, 1, c, f, a)
#define PXA_EP_OUT_ISO(i, adr, c, f, a)		PXA_EP_ISO(i, adr, 0, c, f, a)
#define PXA_EP_IN_INT(i, adr, c, f, a)		PXA_EP_INT(i, adr, 1, c, f, a)
#define PXA_EP_CTRL	PXA_EP_DEF(0, 0, 0, 0, EP0_FIFO_SIZE, 0, 0, 0)

struct pxa27x_udc;

struct stats {
	unsigned long in_ops;
	unsigned long out_ops;
	unsigned long in_bytes;
	unsigned long out_bytes;
	unsigned long irqs;
};

struct udc_usb_ep {
	struct usb_ep usb_ep;
	struct usb_endpoint_descriptor desc;
	struct pxa_udc *dev;
	struct pxa_ep *pxa_ep;
};

struct pxa_ep {
	struct pxa_udc		*dev;

	struct list_head	queue;
	spinlock_t		lock;		
						
	unsigned		enabled:1;
	unsigned		in_handle_ep:1;

	unsigned		idx:5;
	char			*name;

	unsigned		dir_in:1;
	unsigned		addr:4;
	unsigned		config:2;
	unsigned		interface:3;
	unsigned		alternate:3;
	unsigned		fifo_size;
	unsigned		type;

#ifdef CONFIG_PM
	u32			udccsr_value;
	u32			udccr_value;
#endif
	struct stats		stats;
};

struct pxa27x_request {
	struct usb_request			req;
	struct udc_usb_ep			*udc_usb_ep;
	unsigned				in_use:1;
	struct list_head			queue;
};

enum ep0_state {
	WAIT_FOR_SETUP,
	SETUP_STAGE,
	IN_DATA_STAGE,
	OUT_DATA_STAGE,
	IN_STATUS_STAGE,
	OUT_STATUS_STAGE,
	STALL,
	WAIT_ACK_SET_CONF_INTERF
};

static char *ep0_state_name[] = {
	"WAIT_FOR_SETUP", "SETUP_STAGE", "IN_DATA_STAGE", "OUT_DATA_STAGE",
	"IN_STATUS_STAGE", "OUT_STATUS_STAGE", "STALL",
	"WAIT_ACK_SET_CONF_INTERF"
};
#define EP0_STNAME(udc) ep0_state_name[(udc)->ep0state]

#define EP0_FIFO_SIZE	16U
#define BULK_FIFO_SIZE	64U
#define ISO_FIFO_SIZE	256U
#define INT_FIFO_SIZE	16U

struct udc_stats {
	unsigned long	irqs_reset;
	unsigned long	irqs_suspend;
	unsigned long	irqs_resume;
	unsigned long	irqs_reconfig;
};

#define NR_USB_ENDPOINTS (1 + 5)	
#define NR_PXA_ENDPOINTS (1 + 14)	

struct pxa_udc {
	void __iomem				*regs;
	int					irq;
	struct clk				*clk;

	struct usb_gadget			gadget;
	struct usb_gadget_driver		*driver;
	struct device				*dev;
	struct pxa2xx_udc_mach_info		*mach;
	struct usb_phy				*transceiver;

	enum ep0_state				ep0state;
	struct udc_stats			stats;

	struct udc_usb_ep			udc_usb_ep[NR_USB_ENDPOINTS];
	struct pxa_ep				pxa_ep[NR_PXA_ENDPOINTS];

	unsigned				enabled:1;
	unsigned				pullup_on:1;
	unsigned				pullup_resume:1;
	unsigned				vbus_sensed:1;
	unsigned				config:2;
	unsigned				last_interface:3;
	unsigned				last_alternate:3;

#ifdef CONFIG_PM
	unsigned				udccsr0;
#endif
#ifdef CONFIG_USB_GADGET_DEBUG_FS
	struct dentry				*debugfs_root;
	struct dentry				*debugfs_state;
	struct dentry				*debugfs_queues;
	struct dentry				*debugfs_eps;
#endif
};

static inline struct pxa_udc *to_gadget_udc(struct usb_gadget *gadget)
{
	return container_of(gadget, struct pxa_udc, gadget);
}

#define ep_dbg(ep, fmt, arg...) \
	dev_dbg(ep->dev->dev, "%s:%s: " fmt, EPNAME(ep), __func__, ## arg)
#define ep_vdbg(ep, fmt, arg...) \
	dev_vdbg(ep->dev->dev, "%s:%s: " fmt, EPNAME(ep), __func__, ## arg)
#define ep_err(ep, fmt, arg...) \
	dev_err(ep->dev->dev, "%s:%s: " fmt, EPNAME(ep), __func__, ## arg)
#define ep_info(ep, fmt, arg...) \
	dev_info(ep->dev->dev, "%s:%s: " fmt, EPNAME(ep), __func__, ## arg)
#define ep_warn(ep, fmt, arg...) \
	dev_warn(ep->dev->dev, "%s:%s:" fmt, EPNAME(ep), __func__, ## arg)

#endif 
