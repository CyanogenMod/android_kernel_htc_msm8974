/*
 *	Copyright (C) 2005 Mike Lee(eemike@gmail.com)
 *
 *	This udc driver is now under testing and code is based on pxa2xx_udc.h
 *	Please use it with your own risk!
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 */

#ifndef __LINUX_USB_GADGET_IMX_H
#define __LINUX_USB_GADGET_IMX_H

#include <linux/types.h>

#define EP_NO(ep)	((ep->bEndpointAddress) & ~USB_DIR_IN) 
#define EP_DIR(ep)	((ep->bEndpointAddress) & USB_DIR_IN ? 1 : 0)
#define IMX_USB_NB_EP	6

struct imx_request {
	struct usb_request			req;
	struct list_head			queue;
	unsigned int				in_use;
};

enum ep0_state {
	EP0_IDLE,
	EP0_IN_DATA_PHASE,
	EP0_OUT_DATA_PHASE,
	EP0_CONFIG,
	EP0_STALL,
};

struct imx_ep_struct {
	struct usb_ep				ep;
	struct imx_udc_struct			*imx_usb;
	struct list_head			queue;
	unsigned char				stopped;
	unsigned char				fifosize;
	unsigned char				bEndpointAddress;
	unsigned char				bmAttributes;
};

struct imx_udc_struct {
	struct usb_gadget			gadget;
	struct usb_gadget_driver		*driver;
	struct device				*dev;
	struct imx_ep_struct			imx_ep[IMX_USB_NB_EP];
	struct clk				*clk;
	struct timer_list			timer;
	enum ep0_state				ep0state;
	struct resource				*res;
	void __iomem				*base;
	unsigned char				set_config;
	int					cfg,
						intf,
						alt,
						usbd_int[7];
};

#define  USB_FRAME		(0x00)	
#define  USB_SPEC		(0x04)	
#define  USB_STAT		(0x08)	
#define  USB_CTRL		(0x0C)	
#define  USB_DADR		(0x10)	
#define  USB_DDAT		(0x14)	
#define  USB_INTR		(0x18)	
#define  USB_MASK		(0x1C)	
#define  USB_ENAB		(0x24)	
#define  USB_EP_STAT(x)		(0x30 + (x*0x30)) 
#define  USB_EP_INTR(x)		(0x34 + (x*0x30)) 
#define  USB_EP_MASK(x)		(0x38 + (x*0x30)) 
#define  USB_EP_FDAT(x)		(0x3C + (x*0x30)) 
#define  USB_EP_FDAT0(x)	(0x3C + (x*0x30)) 
#define  USB_EP_FDAT1(x)	(0x3D + (x*0x30)) 
#define  USB_EP_FDAT2(x)	(0x3E + (x*0x30)) 
#define  USB_EP_FDAT3(x)	(0x3F + (x*0x30)) 
#define  USB_EP_FSTAT(x)	(0x40 + (x*0x30)) 
#define  USB_EP_FCTRL(x)	(0x44 + (x*0x30)) 
#define  USB_EP_LRFP(x)		(0x48 + (x*0x30)) 
#define  USB_EP_LWFP(x)		(0x4C + (x*0x30)) 
#define  USB_EP_FALRM(x)	(0x50 + (x*0x30)) 
#define  USB_EP_FRDP(x)		(0x54 + (x*0x30)) 
#define  USB_EP_FWRP(x)		(0x58 + (x*0x30)) 
#define CTRL_CMDOVER		(1<<6)	
#define CTRL_CMDERROR		(1<<5)	
#define CTRL_FE_ENA		(1<<3)	
#define CTRL_UDC_RST		(1<<2)	
#define CTRL_AFE_ENA		(1<<1)	
#define CTRL_RESUME		(1<<0)	
#define STAT_RST		(1<<8)
#define STAT_SUSP		(1<<7)
#define STAT_CFG		(3<<5)
#define STAT_INTF		(3<<3)
#define STAT_ALTSET		(7<<0)
#define INTR_WAKEUP		(1<<31)	
#define INTR_MSOF		(1<<7)	
#define INTR_SOF		(1<<6)	
#define INTR_RESET_STOP		(1<<5)	
#define INTR_RESET_START	(1<<4)	
#define INTR_RESUME		(1<<3)	
#define INTR_SUSPEND		(1<<2)	
#define INTR_FRAME_MATCH	(1<<1)	
#define INTR_CFG_CHG		(1<<0)	
#define ENAB_RST		(1<<31)	
#define ENAB_ENAB		(1<<30)	
#define ENAB_SUSPEND		(1<<29)	
#define ENAB_ENDIAN		(1<<28)	
#define ENAB_PWRMD		(1<<0)	
#define DADR_CFG		(1<<31)	
#define DADR_BSY		(1<<30)	
#define DADR_DADR		(0x1FF)	
#define DDAT_DDAT		(0xFF)	
#define EPSTAT_BCOUNT		(0x7F<<16)	
#define EPSTAT_SIP		(1<<8)	
#define EPSTAT_DIR		(1<<7)	
#define EPSTAT_MAX		(3<<5)	
#define EPSTAT_TYP		(3<<3)	
#define EPSTAT_ZLPS		(1<<2)	
#define EPSTAT_FLUSH		(1<<1)	
#define EPSTAT_STALL		(1<<0)	
#define FSTAT_FRAME_STAT	(0xF<<24)	
#define FSTAT_ERR		(1<<22)	
#define FSTAT_UF		(1<<21)	
#define FSTAT_OF		(1<<20)	
#define FSTAT_FR		(1<<19)	
#define FSTAT_FULL		(1<<18)	
#define FSTAT_ALRM		(1<<17)	
#define FSTAT_EMPTY		(1<<16)	
#define FCTRL_WFR		(1<<29)	
#define EPINTR_FIFO_FULL	(1<<8)	
#define EPINTR_FIFO_EMPTY	(1<<7)	
#define EPINTR_FIFO_ERROR	(1<<6)	
#define EPINTR_FIFO_HIGH	(1<<5)	
#define EPINTR_FIFO_LOW		(1<<4)	
#define EPINTR_MDEVREQ		(1<<3)	
#define EPINTR_EOT		(1<<2)	
#define EPINTR_DEVREQ		(1<<1)	
#define EPINTR_EOF		(1<<0)	

#ifdef DEBUG


#ifdef DEBUG_REQ
	#define D_REQ(dev, args...)	dev_dbg(dev, ## args)
#else
	#define D_REQ(dev, args...)	do {} while (0)
#endif 

#ifdef DEBUG_TRX
	#define D_TRX(dev, args...)	dev_dbg(dev, ## args)
#else
	#define D_TRX(dev, args...)	do {} while (0)
#endif 

#ifdef DEBUG_INIT
	#define D_INI(dev, args...)	dev_dbg(dev, ## args)
#else
	#define D_INI(dev, args...)	do {} while (0)
#endif 

#ifdef DEBUG_EP0
	static const char *state_name[] = {
		"EP0_IDLE",
		"EP0_IN_DATA_PHASE",
		"EP0_OUT_DATA_PHASE",
		"EP0_CONFIG",
		"EP0_STALL"
	};
	#define D_EP0(dev, args...)	dev_dbg(dev, ## args)
#else
	#define D_EP0(dev, args...)	do {} while (0)
#endif 

#ifdef DEBUG_EPX
	#define D_EPX(dev, args...)	dev_dbg(dev, ## args)
#else
	#define D_EPX(dev, args...)	do {} while (0)
#endif 

#ifdef DEBUG_IRQ
	static void dump_intr(const char *label, int irqreg, struct device *dev)
	{
		dev_dbg(dev, "<%s> USB_INTR=[%s%s%s%s%s%s%s%s%s]\n", label,
			(irqreg & INTR_WAKEUP) ? " wake" : "",
			(irqreg & INTR_MSOF) ? " msof" : "",
			(irqreg & INTR_SOF) ? " sof" : "",
			(irqreg & INTR_RESUME) ? " resume" : "",
			(irqreg & INTR_SUSPEND) ? " suspend" : "",
			(irqreg & INTR_RESET_STOP) ? " noreset" : "",
			(irqreg & INTR_RESET_START) ? " reset" : "",
			(irqreg & INTR_FRAME_MATCH) ? " fmatch" : "",
			(irqreg & INTR_CFG_CHG) ? " config" : "");
	}
#else
	#define dump_intr(x, y, z)		do {} while (0)
#endif 

#ifdef DEBUG_EPIRQ
	static void dump_ep_intr(const char *label, int nr, int irqreg,
							struct device *dev)
	{
		dev_dbg(dev, "<%s> EP%d_INTR=[%s%s%s%s%s%s%s%s%s]\n", label, nr,
			(irqreg & EPINTR_FIFO_FULL) ? " full" : "",
			(irqreg & EPINTR_FIFO_EMPTY) ? " fempty" : "",
			(irqreg & EPINTR_FIFO_ERROR) ? " ferr" : "",
			(irqreg & EPINTR_FIFO_HIGH) ? " fhigh" : "",
			(irqreg & EPINTR_FIFO_LOW) ? " flow" : "",
			(irqreg & EPINTR_MDEVREQ) ? " mreq" : "",
			(irqreg & EPINTR_EOF) ? " eof" : "",
			(irqreg & EPINTR_DEVREQ) ? " devreq" : "",
			(irqreg & EPINTR_EOT) ? " eot" : "");
	}
#else
	#define dump_ep_intr(x, y, z, i)	do {} while (0)
#endif 

#ifdef DEBUG_DUMP
	static void dump_usb_stat(const char *label,
						struct imx_udc_struct *imx_usb)
	{
		int temp = __raw_readl(imx_usb->base + USB_STAT);

		dev_dbg(imx_usb->dev,
			"<%s> USB_STAT=[%s%s CFG=%d, INTF=%d, ALTR=%d]\n", label,
			(temp & STAT_RST) ? " reset" : "",
			(temp & STAT_SUSP) ? " suspend" : "",
			(temp & STAT_CFG) >> 5,
			(temp & STAT_INTF) >> 3,
			(temp & STAT_ALTSET));
	}

	static void dump_ep_stat(const char *label,
						struct imx_ep_struct *imx_ep)
	{
		int temp = __raw_readl(imx_ep->imx_usb->base
						+ USB_EP_INTR(EP_NO(imx_ep)));

		dev_dbg(imx_ep->imx_usb->dev,
			"<%s> EP%d_INTR=[%s%s%s%s%s%s%s%s%s]\n",
			label, EP_NO(imx_ep),
			(temp & EPINTR_FIFO_FULL) ? " full" : "",
			(temp & EPINTR_FIFO_EMPTY) ? " fempty" : "",
			(temp & EPINTR_FIFO_ERROR) ? " ferr" : "",
			(temp & EPINTR_FIFO_HIGH) ? " fhigh" : "",
			(temp & EPINTR_FIFO_LOW) ? " flow" : "",
			(temp & EPINTR_MDEVREQ) ? " mreq" : "",
			(temp & EPINTR_EOF) ? " eof" : "",
			(temp & EPINTR_DEVREQ) ? " devreq" : "",
			(temp & EPINTR_EOT) ? " eot" : "");

		temp = __raw_readl(imx_ep->imx_usb->base
						+ USB_EP_STAT(EP_NO(imx_ep)));

		dev_dbg(imx_ep->imx_usb->dev,
			"<%s> EP%d_STAT=[%s%s bcount=%d]\n",
			label, EP_NO(imx_ep),
			(temp & EPSTAT_SIP) ? " sip" : "",
			(temp & EPSTAT_STALL) ? " stall" : "",
			(temp & EPSTAT_BCOUNT) >> 16);

		temp = __raw_readl(imx_ep->imx_usb->base
						+ USB_EP_FSTAT(EP_NO(imx_ep)));

		dev_dbg(imx_ep->imx_usb->dev,
			"<%s> EP%d_FSTAT=[%s%s%s%s%s%s%s]\n",
			label, EP_NO(imx_ep),
			(temp & FSTAT_ERR) ? " ferr" : "",
			(temp & FSTAT_UF) ? " funder" : "",
			(temp & FSTAT_OF) ? " fover" : "",
			(temp & FSTAT_FR) ? " fready" : "",
			(temp & FSTAT_FULL) ? " ffull" : "",
			(temp & FSTAT_ALRM) ? " falarm" : "",
			(temp & FSTAT_EMPTY) ? " fempty" : "");
	}

	static void dump_req(const char *label, struct imx_ep_struct *imx_ep,
							struct usb_request *req)
	{
		int i;

		if (!req || !req->buf) {
			dev_dbg(imx_ep->imx_usb->dev,
					"<%s> req or req buf is free\n", label);
			return;
		}

		if ((!EP_NO(imx_ep) && imx_ep->imx_usb->ep0state
			== EP0_IN_DATA_PHASE)
			|| (EP_NO(imx_ep) && EP_DIR(imx_ep))) {

			dev_dbg(imx_ep->imx_usb->dev,
						"<%s> request dump <", label);
			for (i = 0; i < req->length; i++)
				printk("%02x-", *((u8 *)req->buf + i));
			printk(">\n");
		}
	}

#else
	#define dump_ep_stat(x, y)		do {} while (0)
	#define dump_usb_stat(x, y)		do {} while (0)
	#define dump_req(x, y, z)		do {} while (0)
#endif 

#ifdef DEBUG_ERR
	#define D_ERR(dev, args...)	dev_dbg(dev, ## args)
#else
	#define D_ERR(dev, args...)	do {} while (0)
#endif

#else
	#define D_REQ(dev, args...)		do {} while (0)
	#define D_TRX(dev, args...)		do {} while (0)
	#define D_INI(dev, args...)		do {} while (0)
	#define D_EP0(dev, args...)		do {} while (0)
	#define D_EPX(dev, args...)		do {} while (0)
	#define dump_ep_intr(x, y, z, i)	do {} while (0)
	#define dump_intr(x, y, z)		do {} while (0)
	#define dump_ep_stat(x, y)		do {} while (0)
	#define dump_usb_stat(x, y)		do {} while (0)
	#define dump_req(x, y, z)		do {} while (0)
	#define D_ERR(dev, args...)		do {} while (0)
#endif 

#endif 
