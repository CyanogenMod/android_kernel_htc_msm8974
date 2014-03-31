/*
 * Copyright (C) 2004 by Thomas Rathbone, HP Labs
 * Copyright (C) 2005 by Ivan Kokshaysky
 * Copyright (C) 2006 by SAN People
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef AT91_UDC_H
#define AT91_UDC_H


#define AT91_UDP_FRM_NUM	0x00		
#define     AT91_UDP_NUM	(0x7ff <<  0)	
#define     AT91_UDP_FRM_ERR	(1     << 16)	
#define     AT91_UDP_FRM_OK	(1     << 17)	

#define AT91_UDP_GLB_STAT	0x04		
#define     AT91_UDP_FADDEN	(1 <<  0)	
#define     AT91_UDP_CONFG	(1 <<  1)	
#define     AT91_UDP_ESR	(1 <<  2)	
#define     AT91_UDP_RSMINPR	(1 <<  3)	
#define     AT91_UDP_RMWUPE	(1 <<  4)	

#define AT91_UDP_FADDR		0x08		
#define     AT91_UDP_FADD	(0x7f << 0)	
#define     AT91_UDP_FEN	(1    << 8)	

#define AT91_UDP_IER		0x10		
#define AT91_UDP_IDR		0x14		
#define AT91_UDP_IMR		0x18		

#define AT91_UDP_ISR		0x1c		
#define     AT91_UDP_EP(n)	(1 << (n))	
#define     AT91_UDP_RXSUSP	(1 <<  8) 	
#define     AT91_UDP_RXRSM	(1 <<  9)	
#define     AT91_UDP_EXTRSM	(1 << 10)	
#define     AT91_UDP_SOFINT	(1 << 11)	
#define     AT91_UDP_ENDBUSRES	(1 << 12)	
#define     AT91_UDP_WAKEUP	(1 << 13)	

#define AT91_UDP_ICR		0x20		
#define AT91_UDP_RST_EP		0x28		

#define AT91_UDP_CSR(n)		(0x30+((n)*4))	
#define     AT91_UDP_TXCOMP	(1 <<  0)	/* Generates IN packet with data previously written in DPR */
#define     AT91_UDP_RX_DATA_BK0 (1 <<  1)	
#define     AT91_UDP_RXSETUP	(1 <<  2)	
#define     AT91_UDP_STALLSENT	(1 <<  3)	
#define     AT91_UDP_TXPKTRDY	(1 <<  4)	
#define     AT91_UDP_FORCESTALL	(1 <<  5)	
#define     AT91_UDP_RX_DATA_BK1 (1 <<  6)	
#define     AT91_UDP_DIR	(1 <<  7)	
#define     AT91_UDP_EPTYPE	(7 <<  8)	
#define		AT91_UDP_EPTYPE_CTRL		(0 <<  8)
#define		AT91_UDP_EPTYPE_ISO_OUT		(1 <<  8)
#define		AT91_UDP_EPTYPE_BULK_OUT	(2 <<  8)
#define		AT91_UDP_EPTYPE_INT_OUT		(3 <<  8)
#define		AT91_UDP_EPTYPE_ISO_IN		(5 <<  8)
#define		AT91_UDP_EPTYPE_BULK_IN		(6 <<  8)
#define		AT91_UDP_EPTYPE_INT_IN		(7 <<  8)
#define     AT91_UDP_DTGLE	(1 << 11)	
#define     AT91_UDP_EPEDS	(1 << 15)	
#define     AT91_UDP_RXBYTECNT	(0x7ff << 16)	

#define AT91_UDP_FDR(n)		(0x50+((n)*4))	

#define AT91_UDP_TXVC		0x74		
#define     AT91_UDP_TXVC_TXVDIS (1 << 8)	
#define     AT91_UDP_TXVC_PUON   (1 << 9)	



#define	NUM_ENDPOINTS	6

#define	MINIMUS_INTERRUPTUS \
	(AT91_UDP_ENDBUSRES | AT91_UDP_RXRSM | AT91_UDP_RXSUSP)

struct at91_ep {
	struct usb_ep			ep;
	struct list_head		queue;
	struct at91_udc			*udc;
	void __iomem			*creg;

	unsigned			maxpacket:16;
	u8				int_mask;
	unsigned			is_pingpong:1;

	unsigned			stopped:1;
	unsigned			is_in:1;
	unsigned			is_iso:1;
	unsigned			fifo_bank:1;

	const struct usb_endpoint_descriptor
					*desc;
};

struct at91_udc {
	struct usb_gadget		gadget;
	struct at91_ep			ep[NUM_ENDPOINTS];
	struct usb_gadget_driver	*driver;
	unsigned			vbus:1;
	unsigned			enabled:1;
	unsigned			clocked:1;
	unsigned			suspended:1;
	unsigned			req_pending:1;
	unsigned			wait_for_addr_ack:1;
	unsigned			wait_for_config_ack:1;
	unsigned			selfpowered:1;
	unsigned			active_suspend:1;
	u8				addr;
	struct at91_udc_data		board;
	struct clk			*iclk, *fclk;
	struct platform_device		*pdev;
	struct proc_dir_entry		*pde;
	void __iomem			*udp_baseaddr;
	int				udp_irq;
	spinlock_t			lock;
	struct timer_list		vbus_timer;
	struct work_struct		vbus_timer_work;
};

static inline struct at91_udc *to_udc(struct usb_gadget *g)
{
	return container_of(g, struct at91_udc, gadget);
}

struct at91_request {
	struct usb_request		req;
	struct list_head		queue;
};


#ifdef VERBOSE_DEBUG
#    define VDBG		DBG
#else
#    define VDBG(stuff...)	do{}while(0)
#endif

#ifdef PACKET_TRACE
#    define PACKET		VDBG
#else
#    define PACKET(stuff...)	do{}while(0)
#endif

#define ERR(stuff...)		pr_err("udc: " stuff)
#define WARNING(stuff...)	pr_warning("udc: " stuff)
#define INFO(stuff...)		pr_info("udc: " stuff)
#define DBG(stuff...)		pr_debug("udc: " stuff)

#endif

