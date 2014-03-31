/*
 * amd5536.h -- header for AMD 5536 UDC high/full speed USB device controller
 *
 * Copyright (C) 2007 AMD (http://www.amd.com)
 * Author: Thomas Dahlmann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef AMD5536UDC_H
#define AMD5536UDC_H

#define UDC_RDE_TIMER_SECONDS		1
#define UDC_RDE_TIMER_DIV		10
#define UDC_POLLSTALL_TIMER_USECONDS	500

#define UDC_HSA0_REV 1
#define UDC_HSB1_REV 2

#define UDC_SETCONFIG_DWORD0			0x00000900
#define UDC_SETCONFIG_DWORD0_VALUE_MASK		0xffff0000
#define UDC_SETCONFIG_DWORD0_VALUE_OFS		16

#define UDC_SETCONFIG_DWORD1			0x00000000

#define UDC_SETINTF_DWORD0			0x00000b00
#define UDC_SETINTF_DWORD0_ALT_MASK		0xffff0000
#define UDC_SETINTF_DWORD0_ALT_OFS		16

#define UDC_SETINTF_DWORD1			0x00000000
#define UDC_SETINTF_DWORD1_INTF_MASK		0x0000ffff
#define UDC_SETINTF_DWORD1_INTF_OFS		0

#define UDC_MSCRES_DWORD0			0x0000ff21
#define UDC_MSCRES_DWORD1			0x00000000

#define UDC_CSR_ADDR				0x500

#define UDC_CSR_NE_NUM_MASK			0x0000000f
#define UDC_CSR_NE_NUM_OFS			0
#define UDC_CSR_NE_DIR_MASK			0x00000010
#define UDC_CSR_NE_DIR_OFS			4
#define UDC_CSR_NE_TYPE_MASK			0x00000060
#define UDC_CSR_NE_TYPE_OFS			5
#define UDC_CSR_NE_CFG_MASK			0x00000780
#define UDC_CSR_NE_CFG_OFS			7
#define UDC_CSR_NE_INTF_MASK			0x00007800
#define UDC_CSR_NE_INTF_OFS			11
#define UDC_CSR_NE_ALT_MASK			0x00078000
#define UDC_CSR_NE_ALT_OFS			15

#define UDC_CSR_NE_MAX_PKT_MASK			0x3ff80000
#define UDC_CSR_NE_MAX_PKT_OFS			19

#define UDC_DEVCFG_ADDR				0x400

#define UDC_DEVCFG_SOFTRESET			31
#define UDC_DEVCFG_HNPSFEN			30
#define UDC_DEVCFG_DMARST			29
#define UDC_DEVCFG_SET_DESC			18
#define UDC_DEVCFG_CSR_PRG			17
#define UDC_DEVCFG_STATUS			7
#define UDC_DEVCFG_DIR				6
#define UDC_DEVCFG_PI				5
#define UDC_DEVCFG_SS				4
#define UDC_DEVCFG_SP				3
#define UDC_DEVCFG_RWKP				2

#define UDC_DEVCFG_SPD_MASK			0x3
#define UDC_DEVCFG_SPD_OFS			0
#define UDC_DEVCFG_SPD_HS			0x0
#define UDC_DEVCFG_SPD_FS			0x1
#define UDC_DEVCFG_SPD_LS			0x2


#define UDC_DEVCTL_ADDR				0x404

#define UDC_DEVCTL_THLEN_MASK			0xff000000
#define UDC_DEVCTL_THLEN_OFS			24

#define UDC_DEVCTL_BRLEN_MASK			0x00ff0000
#define UDC_DEVCTL_BRLEN_OFS			16

#define UDC_DEVCTL_CSR_DONE			13
#define UDC_DEVCTL_DEVNAK			12
#define UDC_DEVCTL_SD				10
#define UDC_DEVCTL_MODE				9
#define UDC_DEVCTL_BREN				8
#define UDC_DEVCTL_THE				7
#define UDC_DEVCTL_BF				6
#define UDC_DEVCTL_BE				5
#define UDC_DEVCTL_DU				4
#define UDC_DEVCTL_TDE				3
#define UDC_DEVCTL_RDE				2
#define UDC_DEVCTL_RES				0


#define UDC_DEVSTS_ADDR				0x408

#define UDC_DEVSTS_TS_MASK			0xfffc0000
#define UDC_DEVSTS_TS_OFS			18

#define UDC_DEVSTS_SESSVLD			17
#define UDC_DEVSTS_PHY_ERROR			16
#define UDC_DEVSTS_RXFIFO_EMPTY			15

#define UDC_DEVSTS_ENUM_SPEED_MASK		0x00006000
#define UDC_DEVSTS_ENUM_SPEED_OFS		13
#define UDC_DEVSTS_ENUM_SPEED_FULL		1
#define UDC_DEVSTS_ENUM_SPEED_HIGH		0

#define UDC_DEVSTS_SUSP				12

#define UDC_DEVSTS_ALT_MASK			0x00000f00
#define UDC_DEVSTS_ALT_OFS			8

#define UDC_DEVSTS_INTF_MASK			0x000000f0
#define UDC_DEVSTS_INTF_OFS			4

#define UDC_DEVSTS_CFG_MASK			0x0000000f
#define UDC_DEVSTS_CFG_OFS			0


#define UDC_DEVINT_ADDR				0x40c

#define UDC_DEVINT_SVC				7
#define UDC_DEVINT_ENUM				6
#define UDC_DEVINT_SOF				5
#define UDC_DEVINT_US				4
#define UDC_DEVINT_UR				3
#define UDC_DEVINT_ES				2
#define UDC_DEVINT_SI				1
#define UDC_DEVINT_SC				0

#define UDC_DEVINT_MSK_ADDR			0x410

#define UDC_DEVINT_MSK				0x7f

#define UDC_EPINT_ADDR				0x414

#define UDC_EPINT_OUT_MASK			0xffff0000
#define UDC_EPINT_OUT_OFS			16
#define UDC_EPINT_IN_MASK			0x0000ffff
#define UDC_EPINT_IN_OFS			0

#define UDC_EPINT_IN_EP0			0
#define UDC_EPINT_IN_EP1			1
#define UDC_EPINT_IN_EP2			2
#define UDC_EPINT_IN_EP3			3
#define UDC_EPINT_OUT_EP0			16
#define UDC_EPINT_OUT_EP1			17
#define UDC_EPINT_OUT_EP2			18
#define UDC_EPINT_OUT_EP3			19

#define UDC_EPINT_EP0_ENABLE_MSK		0x001e001e

#define UDC_EPINT_MSK_ADDR			0x418

#define UDC_EPINT_OUT_MSK_MASK			0xffff0000
#define UDC_EPINT_OUT_MSK_OFS			16
#define UDC_EPINT_IN_MSK_MASK			0x0000ffff
#define UDC_EPINT_IN_MSK_OFS			0

#define UDC_EPINT_MSK_DISABLE_ALL		0xffffffff
#define UDC_EPDATAINT_MSK_DISABLE		0xfffefffe
#define UDC_DEV_MSK_DISABLE			0x7f

#define UDC_EPREGS_ADDR				0x0
#define UDC_EPIN_REGS_ADDR			0x0
#define UDC_EPOUT_REGS_ADDR			0x200

#define UDC_EPCTL_ADDR				0x0

#define UDC_EPCTL_RRDY				9
#define UDC_EPCTL_CNAK				8
#define UDC_EPCTL_SNAK				7
#define UDC_EPCTL_NAK				6

#define UDC_EPCTL_ET_MASK			0x00000030
#define UDC_EPCTL_ET_OFS			4
#define UDC_EPCTL_ET_CONTROL			0
#define UDC_EPCTL_ET_ISO			1
#define UDC_EPCTL_ET_BULK			2
#define UDC_EPCTL_ET_INTERRUPT			3

#define UDC_EPCTL_P				3
#define UDC_EPCTL_SN				2
#define UDC_EPCTL_F				1
#define UDC_EPCTL_S				0

#define UDC_EPSTS_ADDR				0x4

#define UDC_EPSTS_RX_PKT_SIZE_MASK		0x007ff800
#define UDC_EPSTS_RX_PKT_SIZE_OFS		11

#define UDC_EPSTS_TDC				10
#define UDC_EPSTS_HE				9
#define UDC_EPSTS_BNA				7
#define UDC_EPSTS_IN				6

#define UDC_EPSTS_OUT_MASK			0x00000030
#define UDC_EPSTS_OUT_OFS			4
#define UDC_EPSTS_OUT_DATA			1
#define UDC_EPSTS_OUT_DATA_CLEAR		0x10
#define UDC_EPSTS_OUT_SETUP			2
#define UDC_EPSTS_OUT_SETUP_CLEAR		0x20
#define UDC_EPSTS_OUT_CLEAR			0x30

#define UDC_EPIN_BUFF_SIZE_ADDR			0x8
#define UDC_EPOUT_FRAME_NUMBER_ADDR		0x8

#define UDC_EPIN_BUFF_SIZE_MASK			0x0000ffff
#define UDC_EPIN_BUFF_SIZE_OFS			0
#define UDC_EPIN0_BUFF_SIZE			32
#define UDC_FS_EPIN0_BUFF_SIZE			32

#define UDC_EPIN_BUFF_SIZE_MULT			2

#define UDC_EPIN_BUFF_SIZE			256
#define UDC_EPIN_SMALLINT_BUFF_SIZE		32

#define UDC_FS_EPIN_BUFF_SIZE			32

#define UDC_EPOUT_FRAME_NUMBER_MASK		0x0000ffff
#define UDC_EPOUT_FRAME_NUMBER_OFS		0

#define UDC_EPOUT_BUFF_SIZE_ADDR		0x0c
#define UDC_EP_MAX_PKT_SIZE_ADDR		0x0c

#define UDC_EPOUT_BUFF_SIZE_MASK		0xffff0000
#define UDC_EPOUT_BUFF_SIZE_OFS			16
#define UDC_EP_MAX_PKT_SIZE_MASK		0x0000ffff
#define UDC_EP_MAX_PKT_SIZE_OFS			0
#define UDC_EP0IN_MAX_PKT_SIZE			64
#define UDC_EP0OUT_MAX_PKT_SIZE			64
#define UDC_FS_EP0IN_MAX_PKT_SIZE		64
#define UDC_FS_EP0OUT_MAX_PKT_SIZE		64

#define UDC_DMA_STP_STS_CFG_MASK		0x0fff0000
#define UDC_DMA_STP_STS_CFG_OFS			16
#define UDC_DMA_STP_STS_CFG_ALT_MASK		0x000f0000
#define UDC_DMA_STP_STS_CFG_ALT_OFS		16
#define UDC_DMA_STP_STS_CFG_INTF_MASK		0x00f00000
#define UDC_DMA_STP_STS_CFG_INTF_OFS		20
#define UDC_DMA_STP_STS_CFG_NUM_MASK		0x0f000000
#define UDC_DMA_STP_STS_CFG_NUM_OFS		24
#define UDC_DMA_STP_STS_RX_MASK			0x30000000
#define UDC_DMA_STP_STS_RX_OFS			28
#define UDC_DMA_STP_STS_BS_MASK			0xc0000000
#define UDC_DMA_STP_STS_BS_OFS			30
#define UDC_DMA_STP_STS_BS_HOST_READY		0
#define UDC_DMA_STP_STS_BS_DMA_BUSY		1
#define UDC_DMA_STP_STS_BS_DMA_DONE		2
#define UDC_DMA_STP_STS_BS_HOST_BUSY		3
#define UDC_DMA_IN_STS_TXBYTES_MASK		0x0000ffff
#define UDC_DMA_IN_STS_TXBYTES_OFS		0
#define	UDC_DMA_IN_STS_FRAMENUM_MASK		0x07ff0000
#define UDC_DMA_IN_STS_FRAMENUM_OFS		0
#define UDC_DMA_IN_STS_L			27
#define UDC_DMA_IN_STS_TX_MASK			0x30000000
#define UDC_DMA_IN_STS_TX_OFS			28
#define UDC_DMA_IN_STS_BS_MASK			0xc0000000
#define UDC_DMA_IN_STS_BS_OFS			30
#define UDC_DMA_IN_STS_BS_HOST_READY		0
#define UDC_DMA_IN_STS_BS_DMA_BUSY		1
#define UDC_DMA_IN_STS_BS_DMA_DONE		2
#define UDC_DMA_IN_STS_BS_HOST_BUSY		3
#define UDC_DMA_OUT_STS_RXBYTES_MASK		0x0000ffff
#define UDC_DMA_OUT_STS_RXBYTES_OFS		0
#define UDC_DMA_OUT_STS_FRAMENUM_MASK		0x07ff0000
#define UDC_DMA_OUT_STS_FRAMENUM_OFS		0
#define UDC_DMA_OUT_STS_L			27
#define UDC_DMA_OUT_STS_RX_MASK			0x30000000
#define UDC_DMA_OUT_STS_RX_OFS			28
#define UDC_DMA_OUT_STS_BS_MASK			0xc0000000
#define UDC_DMA_OUT_STS_BS_OFS			30
#define UDC_DMA_OUT_STS_BS_HOST_READY		0
#define UDC_DMA_OUT_STS_BS_DMA_BUSY		1
#define UDC_DMA_OUT_STS_BS_DMA_DONE		2
#define UDC_DMA_OUT_STS_BS_HOST_BUSY		3
#define UDC_EP0IN_MAXPACKET			1000
#define UDC_DMA_MAXPACKET			65536

#define DMA_DONT_USE				(~(dma_addr_t) 0 )

#define UDC_EP_SUBPTR_ADDR			0x10
#define UDC_EP_DESPTR_ADDR			0x14
#define UDC_EP_WRITE_CONFIRM_ADDR		0x1c

#define UDC_EP_NUM				32
#define UDC_EPIN_NUM				16
#define UDC_EPIN_NUM_USED			5
#define UDC_EPOUT_NUM				16
#define UDC_USED_EP_NUM				9
#define UDC_CSR_EP_OUT_IX_OFS			12

#define UDC_EP0OUT_IX				16
#define UDC_EP0IN_IX				0

#define UDC_RXFIFO_ADDR				0x800
#define UDC_RXFIFO_SIZE				0x400

#define UDC_TXFIFO_ADDR				0xc00
#define UDC_TXFIFO_SIZE				0x600

#define UDC_EPIN_STATUS_IX			1
#define UDC_EPIN_IX				2
#define UDC_EPOUT_IX				18

#define UDC_DWORD_BYTES				4
#define UDC_BITS_PER_BYTE_SHIFT			3
#define UDC_BYTE_MASK				0xff
#define UDC_BITS_PER_BYTE			8

struct udc_csrs {

	
	u32 sca;

	
	u32 ne[UDC_USED_EP_NUM];
} __attribute__ ((packed));

struct udc_regs {

	
	u32 cfg;

	
	u32 ctl;

	
	u32 sts;

	
	u32 irqsts;

	
	u32 irqmsk;

	
	u32 ep_irqsts;

	
	u32 ep_irqmsk;
} __attribute__ ((packed));

struct udc_ep_regs {

	
	u32 ctl;

	
	u32 sts;

	
	u32 bufin_framenum;

	
	u32 bufout_maxpkt;

	
	u32 subptr;

	
	u32 desptr;

	
	u32 reserved;

	
	u32 confirm;

} __attribute__ ((packed));

struct udc_stp_dma {
	
	u32	status;
	
	u32	_reserved;
	
	u32	data12;
	
	u32	data34;
} __attribute__ ((aligned (16)));

struct udc_data_dma {
	
	u32	status;
	
	u32	_reserved;
	
	u32	bufptr;
	
	u32	next;
} __attribute__ ((aligned (16)));

struct udc_request {
	
	struct usb_request		req;

	
	unsigned			dma_going : 1,
					dma_mapping : 1,
					dma_done : 1;
	
	dma_addr_t			td_phys;
	
	struct udc_data_dma		*td_data;
	
	struct udc_data_dma		*td_data_last;
	struct list_head		queue;

	
	unsigned			chain_len;

};

struct udc_ep {
	struct usb_ep			ep;
	struct udc_ep_regs __iomem	*regs;
	u32 __iomem			*txfifo;
	u32 __iomem			*dma;
	dma_addr_t			td_phys;
	dma_addr_t			td_stp_dma;
	struct udc_stp_dma		*td_stp;
	struct udc_data_dma		*td;
	
	struct udc_request		*req;
	unsigned			req_used;
	unsigned			req_completed;
	
	struct udc_request		*bna_dummy_req;
	unsigned			bna_occurred;

	
	unsigned			naking;

	struct udc			*dev;

	
	struct list_head		queue;
	const struct usb_endpoint_descriptor	*desc;
	unsigned			halted;
	unsigned			cancel_transfer;
	unsigned			num : 5,
					fifo_depth : 14,
					in : 1;
};

struct udc {
	struct usb_gadget		gadget;
	spinlock_t			lock;	
	
	struct udc_ep			ep[UDC_EP_NUM];
	struct usb_gadget_driver	*driver;
	
	unsigned			active : 1,
					stall_ep0in : 1,
					waiting_zlp_ack_ep0in : 1,
					set_cfg_not_acked : 1,
					irq_registered : 1,
					data_ep_enabled : 1,
					data_ep_queued : 1,
					mem_region : 1,
					sys_suspended : 1,
					connected;

	u16				chiprev;

	
	struct pci_dev			*pdev;
	struct udc_csrs __iomem		*csr;
	struct udc_regs __iomem		*regs;
	struct udc_ep_regs __iomem	*ep_regs;
	u32 __iomem			*rxfifo;
	u32 __iomem			*txfifo;

	
	struct pci_pool			*data_requests;
	struct pci_pool			*stp_requests;

	
	unsigned long			phys_addr;
	void __iomem			*virt_addr;
	unsigned			irq;

	
	u16				cur_config;
	u16				cur_intf;
	u16				cur_alt;
};

union udc_setup_data {
	u32			data[2];
	struct usb_ctrlrequest	request;
};

#define AMD_ADDBITS(u32Val, bitfield_val, bitfield_stub_name)		\
	(((u32Val) & (((u32) ~((u32) bitfield_stub_name##_MASK))))	\
	| (((bitfield_val) << ((u32) bitfield_stub_name##_OFS))		\
		& ((u32) bitfield_stub_name##_MASK)))

#define AMD_INIT_SETBITS(u32Val, bitfield_val, bitfield_stub_name)	\
	((u32Val)							\
	| (((bitfield_val) << ((u32) bitfield_stub_name##_OFS))		\
		& ((u32) bitfield_stub_name##_MASK)))

#define AMD_GETBITS(u32Val, bitfield_stub_name)				\
	((u32Val & ((u32) bitfield_stub_name##_MASK))			\
		>> ((u32) bitfield_stub_name##_OFS))

#define AMD_BIT(bit_stub_name) (1 << bit_stub_name)
#define AMD_UNMASK_BIT(bit_stub_name) (~AMD_BIT(bit_stub_name))
#define AMD_CLEAR_BIT(bit_stub_name) (~AMD_BIT(bit_stub_name))


#define DBG(udc , args...)	dev_dbg(&(udc)->pdev->dev, args)

#ifdef UDC_VERBOSE
#define VDBG			DBG
#else
#define VDBG(udc , args...)	do {} while (0)
#endif

#endif 
