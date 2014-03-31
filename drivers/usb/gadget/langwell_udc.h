/*
 * Intel Langwell USB Device Controller driver
 * Copyright (C) 2008-2009, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#include <linux/usb/langwell_udc.h>



struct langwell_dtd {
	u32	dtd_next;
#define	DTD_NEXT(d)	(((d)>>5)&0x7ffffff)
#define	DTD_NEXT_MASK	(0x7ffffff << 5)
#define	DTD_TERM	BIT(0)
	
	u32	dtd_status:8;
#define	DTD_STATUS(d)	(((d)>>0)&0xff)
#define	DTD_STS_ACTIVE	BIT(7)	
#define	DTD_STS_HALTED	BIT(6)	
#define	DTD_STS_DBE	BIT(5)	
#define	DTD_STS_TRE	BIT(3)	
	
	u32	dtd_res0:2;
	
	u32	dtd_multo:2;
#define	DTD_MULTO	(BIT(11) | BIT(10))
	
	u32	dtd_res1:3;
	
	u32	dtd_ioc:1;
#define	DTD_IOC		BIT(15)
	
	u32	dtd_total:15;
#define	DTD_TOTAL(d)	(((d)>>16)&0x7fff)
#define	DTD_MAX_TRANSFER_LENGTH	0x4000
	
	u32	dtd_res2:1;
	
	u32	dtd_buf[5];
#define	DTD_OFFSET_MASK	0xfff
#define	DTD_BUFFER(d)	(((d)>>12)&0x3ff)
#define	DTD_C_OFFSET(d)	(((d)>>0)&0xfff)
#define	DTD_FRAME(d)	(((d)>>0)&0x7ff)

	

	
	dma_addr_t		dtd_dma;
	
	struct langwell_dtd	*next_dtd_virt;
};


struct langwell_dqh {
	
	u32	dqh_res0:15;	
	u32	dqh_ios:1;	
#define	DQH_IOS		BIT(15)
	u32	dqh_mpl:11;	
#define	DQH_MPL		(0x7ff << 16)
	u32	dqh_res1:2;	
	u32	dqh_zlt:1;	
#define	DQH_ZLT		BIT(29)
	u32	dqh_mult:2;	
#define	DQH_MULT	(BIT(30) | BIT(31))

	
	u32	dqh_current;	
#define DQH_C_DTD(e)	\
	(((e)>>5)&0x7ffffff)	

	
	u32	dtd_next;
	u32	dtd_status:8;	
	u32	dtd_res0:2;	
	u32	dtd_multo:2;	
	u32	dtd_res1:3;	
	u32	dtd_ioc:1;	
	u32	dtd_total:15;	
	u32	dtd_res2:1;	
	u32	dtd_buf[5];	

	u32	dqh_res2;
	struct usb_ctrlrequest	dqh_setup;	
} __attribute__ ((aligned(64)));


struct langwell_ep {
	struct usb_ep		ep;
	dma_addr_t		dma;
	struct langwell_udc	*dev;
	unsigned long		irqs;
	struct list_head	queue;
	struct langwell_dqh	*dqh;
	const struct usb_endpoint_descriptor	*desc;
	char			name[14];
	unsigned		stopped:1,
				ep_type:2,
				ep_num:8;
};


struct langwell_request {
	struct usb_request	req;
	struct langwell_dtd	*dtd, *head, *tail;
	struct langwell_ep	*ep;
	dma_addr_t		dtd_dma;
	struct list_head	queue;
	unsigned		dtd_count;
	unsigned		mapped:1;
};


enum ep0_state {
	WAIT_FOR_SETUP,
	DATA_STATE_XMIT,
	DATA_STATE_NEED_ZLP,
	WAIT_FOR_OUT_STATUS,
	DATA_STATE_RECV,
};


enum lpm_state {
	LPM_L0,	
	LPM_L1,	
	LPM_L2,	
	LPM_L3,	
};


struct langwell_udc {
	
	struct usb_gadget	gadget;
	spinlock_t		lock;	
	struct langwell_ep	*ep;
	struct usb_gadget_driver	*driver;
	struct usb_phy		*transceiver;
	u8			dev_addr;
	u32			usb_state;
	u32			resume_state;
	u32			bus_reset;
	enum lpm_state		lpm_state;
	enum ep0_state		ep0_state;
	u32			ep0_dir;
	u16			dciversion;
	unsigned		ep_max;
	unsigned		devcap:1,
				enabled:1,
				region:1,
				got_irq:1,
				powered:1,
				remote_wakeup:1,
				rate:1,
				is_reset:1,
				softconnected:1,
				vbus_active:1,
				suspended:1,
				stopped:1,
				lpm:1,		
				has_sram:1,	
				got_sram:1;

	
	struct pci_dev		*pdev;

	
	struct langwell_otg	*lotg;

	
	struct langwell_cap_regs	__iomem	*cap_regs;
	struct langwell_op_regs		__iomem	*op_regs;

	struct usb_ctrlrequest	local_setup_buff;
	struct langwell_dqh	*ep_dqh;
	size_t			ep_dqh_size;
	dma_addr_t		ep_dqh_dma;

	
	struct langwell_request	*status_req;

	
	struct dma_pool		*dtd_pool;

	
	struct completion	*done;

	
	unsigned int		sram_addr;
	unsigned int		sram_size;

	
	u16			dev_status;
};

#define gadget_to_langwell(g)	container_of((g), struct langwell_udc, gadget)

