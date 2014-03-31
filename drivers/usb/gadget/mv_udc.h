/*
 * Copyright (C) 2011 Marvell International Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __MV_UDC_H
#define __MV_UDC_H

#define VUSBHS_MAX_PORTS	8

#define DQH_ALIGNMENT		2048
#define DTD_ALIGNMENT		64
#define DMA_BOUNDARY		4096

#define EP_DIR_IN	1
#define EP_DIR_OUT	0

#define DMA_ADDR_INVALID	(~(dma_addr_t)0)

#define EP0_MAX_PKT_SIZE	64
#define WAIT_FOR_SETUP		0
#define DATA_STATE_XMIT		1
#define DATA_STATE_NEED_ZLP	2
#define WAIT_FOR_OUT_STATUS	3
#define DATA_STATE_RECV		4

#define CAPLENGTH_MASK		(0xff)
#define DCCPARAMS_DEN_MASK	(0x1f)

#define HCSPARAMS_PPC		(0x10)

#define USB_FRINDEX_MASKS	0x3fff

#define USBCMD_RUN_STOP				(0x00000001)
#define USBCMD_CTRL_RESET			(0x00000002)
#define USBCMD_SETUP_TRIPWIRE_SET		(0x00002000)
#define USBCMD_SETUP_TRIPWIRE_CLEAR		(~USBCMD_SETUP_TRIPWIRE_SET)

#define USBCMD_ATDTW_TRIPWIRE_SET		(0x00004000)
#define USBCMD_ATDTW_TRIPWIRE_CLEAR		(~USBCMD_ATDTW_TRIPWIRE_SET)

#define USBCMD_FRAME_SIZE_1024			(0x00000000) 
#define USBCMD_FRAME_SIZE_512			(0x00000004) 
#define USBCMD_FRAME_SIZE_256			(0x00000008) 
#define USBCMD_FRAME_SIZE_128			(0x0000000C) 
#define USBCMD_FRAME_SIZE_64			(0x00008000) 
#define USBCMD_FRAME_SIZE_32			(0x00008004) 
#define USBCMD_FRAME_SIZE_16			(0x00008008) 
#define USBCMD_FRAME_SIZE_8			(0x0000800C) 

#define EPCTRL_TX_ALL_MASK			(0xFFFF0000)
#define EPCTRL_RX_ALL_MASK			(0x0000FFFF)

#define EPCTRL_TX_DATA_TOGGLE_RST		(0x00400000)
#define EPCTRL_TX_EP_STALL			(0x00010000)
#define EPCTRL_RX_EP_STALL			(0x00000001)
#define EPCTRL_RX_DATA_TOGGLE_RST		(0x00000040)
#define EPCTRL_RX_ENABLE			(0x00000080)
#define EPCTRL_TX_ENABLE			(0x00800000)
#define EPCTRL_CONTROL				(0x00000000)
#define EPCTRL_ISOCHRONOUS			(0x00040000)
#define EPCTRL_BULK				(0x00080000)
#define EPCTRL_INT				(0x000C0000)
#define EPCTRL_TX_TYPE				(0x000C0000)
#define EPCTRL_RX_TYPE				(0x0000000C)
#define EPCTRL_DATA_TOGGLE_INHIBIT		(0x00000020)
#define EPCTRL_TX_EP_TYPE_SHIFT			(18)
#define EPCTRL_RX_EP_TYPE_SHIFT			(2)

#define EPCOMPLETE_MAX_ENDPOINTS		(16)

#define USB_EP_LIST_ADDRESS_MASK              0xfffff800

#define PORTSCX_W1C_BITS			0x2a
#define PORTSCX_PORT_RESET			0x00000100
#define PORTSCX_PORT_POWER			0x00001000
#define PORTSCX_FORCE_FULL_SPEED_CONNECT	0x01000000
#define PORTSCX_PAR_XCVR_SELECT			0xC0000000
#define PORTSCX_PORT_FORCE_RESUME		0x00000040
#define PORTSCX_PORT_SUSPEND			0x00000080
#define PORTSCX_PORT_SPEED_FULL			0x00000000
#define PORTSCX_PORT_SPEED_LOW			0x04000000
#define PORTSCX_PORT_SPEED_HIGH			0x08000000
#define PORTSCX_PORT_SPEED_MASK			0x0C000000

#define USBMODE_CTRL_MODE_IDLE			0x00000000
#define USBMODE_CTRL_MODE_DEVICE		0x00000002
#define USBMODE_CTRL_MODE_HOST			0x00000003
#define USBMODE_CTRL_MODE_RSV			0x00000001
#define USBMODE_SETUP_LOCK_OFF			0x00000008
#define USBMODE_STREAM_DISABLE			0x00000010

#define USBSTS_INT			0x00000001
#define USBSTS_ERR			0x00000002
#define USBSTS_PORT_CHANGE		0x00000004
#define USBSTS_FRM_LST_ROLL		0x00000008
#define USBSTS_SYS_ERR			0x00000010
#define USBSTS_IAA			0x00000020
#define USBSTS_RESET			0x00000040
#define USBSTS_SOF			0x00000080
#define USBSTS_SUSPEND			0x00000100
#define USBSTS_HC_HALTED		0x00001000
#define USBSTS_RCL			0x00002000
#define USBSTS_PERIODIC_SCHEDULE	0x00004000
#define USBSTS_ASYNC_SCHEDULE		0x00008000


#define USBINTR_INT_EN                          (0x00000001)
#define USBINTR_ERR_INT_EN                      (0x00000002)
#define USBINTR_PORT_CHANGE_DETECT_EN           (0x00000004)

#define USBINTR_ASYNC_ADV_AAE                   (0x00000020)
#define USBINTR_ASYNC_ADV_AAE_ENABLE            (0x00000020)
#define USBINTR_ASYNC_ADV_AAE_DISABLE           (0xFFFFFFDF)

#define USBINTR_RESET_EN                        (0x00000040)
#define USBINTR_SOF_UFRAME_EN                   (0x00000080)
#define USBINTR_DEVICE_SUSPEND                  (0x00000100)

#define USB_DEVICE_ADDRESS_MASK			(0xfe000000)
#define USB_DEVICE_ADDRESS_BIT_SHIFT		(25)

struct mv_cap_regs {
	u32	caplength_hciversion;
	u32	hcsparams;	
	u32	hccparams;	
	u32	reserved[5];
	u32	dciversion;	
	u32	dccparams;	
};

struct mv_op_regs {
	u32	usbcmd;		
	u32	usbsts;		
	u32	usbintr;	
	u32	frindex;	
	u32	reserved1[1];
	u32	deviceaddr;	
	u32	eplistaddr;	
	u32	ttctrl;		
	u32	burstsize;	
	u32	txfilltuning;	
	u32	reserved[4];
	u32	epnak;		
	u32	epnaken;	
	u32	configflag;	
	u32	portsc[VUSBHS_MAX_PORTS]; 
	u32	otgsc;
	u32	usbmode;	
	u32	epsetupstat;	
	u32	epprime;	
	u32	epflush;	
	u32	epstatus;	
	u32	epcomplete;	
	u32	epctrlx[16];	
	u32	mcr;		
	u32	isr;		
	u32	ier;		
};

struct mv_udc {
	struct usb_gadget		gadget;
	struct usb_gadget_driver	*driver;
	spinlock_t			lock;
	struct completion		*done;
	struct platform_device		*dev;
	int				irq;

	struct mv_cap_regs __iomem	*cap_regs;
	struct mv_op_regs __iomem	*op_regs;
	void __iomem                    *phy_regs;
	unsigned int			max_eps;
	struct mv_dqh			*ep_dqh;
	size_t				ep_dqh_size;
	dma_addr_t			ep_dqh_dma;

	struct dma_pool			*dtd_pool;
	struct mv_ep			*eps;

	struct mv_dtd			*dtd_head;
	struct mv_dtd			*dtd_tail;
	unsigned int			dtd_entries;

	struct mv_req			*status_req;
	struct usb_ctrlrequest		local_setup_buff;

	unsigned int		resume_state;	
	unsigned int		usb_state;	
	unsigned int		ep0_state;	
	unsigned int		ep0_dir;

	unsigned int		dev_addr;
	unsigned int		test_mode;

	int			errors;
	unsigned		softconnect:1,
				vbus_active:1,
				remote_wakeup:1,
				softconnected:1,
				force_fs:1,
				clock_gating:1,
				active:1,
				stopped:1;      

	struct work_struct	vbus_work;
	struct workqueue_struct *qwork;

	struct usb_phy		*transceiver;

	struct mv_usb_platform_data     *pdata;

	
	unsigned int    clknum;
	struct clk      *clk[0];
};

struct mv_ep {
	struct usb_ep		ep;
	struct mv_udc		*udc;
	struct list_head	queue;
	struct mv_dqh		*dqh;
	const struct usb_endpoint_descriptor	*desc;
	u32			direction;
	char			name[14];
	unsigned		stopped:1,
				wedge:1,
				ep_type:2,
				ep_num:8;
};

struct mv_req {
	struct usb_request	req;
	struct mv_dtd		*dtd, *head, *tail;
	struct mv_ep		*ep;
	struct list_head	queue;
	unsigned int            test_mode;
	unsigned		dtd_count;
	unsigned		mapped:1;
};

#define EP_QUEUE_HEAD_MULT_POS			30
#define EP_QUEUE_HEAD_ZLT_SEL			0x20000000
#define EP_QUEUE_HEAD_MAX_PKT_LEN_POS		16
#define EP_QUEUE_HEAD_MAX_PKT_LEN(ep_info)	(((ep_info)>>16)&0x07ff)
#define EP_QUEUE_HEAD_IOS			0x00008000
#define EP_QUEUE_HEAD_NEXT_TERMINATE		0x00000001
#define EP_QUEUE_HEAD_IOC			0x00008000
#define EP_QUEUE_HEAD_MULTO			0x00000C00
#define EP_QUEUE_HEAD_STATUS_HALT		0x00000040
#define EP_QUEUE_HEAD_STATUS_ACTIVE		0x00000080
#define EP_QUEUE_CURRENT_OFFSET_MASK		0x00000FFF
#define EP_QUEUE_HEAD_NEXT_POINTER_MASK		0xFFFFFFE0
#define EP_QUEUE_FRINDEX_MASK			0x000007FF
#define EP_MAX_LENGTH_TRANSFER			0x4000

struct mv_dqh {
	
	u32	max_packet_length;
	u32	curr_dtd_ptr;		
	u32	next_dtd_ptr;		
	
	u32	size_ioc_int_sts;
	u32	buff_ptr0;		
	u32	buff_ptr1;		
	u32	buff_ptr2;		
	u32	buff_ptr3;		
	u32	buff_ptr4;		
	u32	reserved1;
	
	u8	setup_buffer[8];
	u32	reserved2[4];
};


#define DTD_NEXT_TERMINATE		(0x00000001)
#define DTD_IOC				(0x00008000)
#define DTD_STATUS_ACTIVE		(0x00000080)
#define DTD_STATUS_HALTED		(0x00000040)
#define DTD_STATUS_DATA_BUFF_ERR	(0x00000020)
#define DTD_STATUS_TRANSACTION_ERR	(0x00000008)
#define DTD_RESERVED_FIELDS		(0x00007F00)
#define DTD_ERROR_MASK			(0x68)
#define DTD_ADDR_MASK			(0xFFFFFFE0)
#define DTD_PACKET_SIZE			0x7FFF0000
#define DTD_LENGTH_BIT_POS		(16)

struct mv_dtd {
	u32	dtd_next;
	u32	size_ioc_sts;
	u32	buff_ptr0;		
	u32	buff_ptr1;		
	u32	buff_ptr2;		
	u32	buff_ptr3;		
	u32	buff_ptr4;		
	u32	scratch_ptr;
	
	dma_addr_t td_dma;		
	struct mv_dtd *next_dtd_virt;
};

#endif
