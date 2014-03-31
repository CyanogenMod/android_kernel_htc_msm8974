/*
 * xHCI host controller driver
 *
 * Copyright (C) 2008 Intel Corp.
 *
 * Author: Sarah Sharp
 * Some code borrowed from the Linux EHCI driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __LINUX_XHCI_HCD_H
#define __LINUX_XHCI_HCD_H

#include <linux/usb.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/usb/hcd.h>

#include	"xhci-ext-caps.h"
#include "pci-quirks.h"

#define XHCI_SBRN_OFFSET	(0x60)

#define MAX_HC_SLOTS		256
#define MAX_HC_PORTS		127


struct xhci_cap_regs {
	__le32	hc_capbase;
	__le32	hcs_params1;
	__le32	hcs_params2;
	__le32	hcs_params3;
	__le32	hcc_params;
	__le32	db_off;
	__le32	run_regs_off;
	
};

#define HC_LENGTH(p)		XHCI_HC_LENGTH(p)
#define HC_VERSION(p)		(((p) >> 16) & 0xffff)

#define HCS_MAX_SLOTS(p)	(((p) >> 0) & 0xff)
#define HCS_SLOTS_MASK		0xff
#define HCS_MAX_INTRS(p)	(((p) >> 8) & 0x7ff)
#define HCS_MAX_PORTS(p)	(((p) >> 24) & 0x7f)

#define HCS_IST(p)		(((p) >> 0) & 0xf)
#define HCS_ERST_MAX(p)		(((p) >> 4) & 0xf)
#define HCS_MAX_SCRATCHPAD(p)   (((p) >> 27) & 0x1f)

#define HCS_U1_LATENCY(p)	(((p) >> 0) & 0xff)
#define HCS_U2_LATENCY(p)	(((p) >> 16) & 0xffff)

#define HCC_64BIT_ADDR(p)	((p) & (1 << 0))
#define HCC_BANDWIDTH_NEG(p)	((p) & (1 << 1))
#define HCC_64BYTE_CONTEXT(p)	((p) & (1 << 2))
#define HCC_PPC(p)		((p) & (1 << 3))
#define HCS_INDICATOR(p)	((p) & (1 << 4))
#define HCC_LIGHT_RESET(p)	((p) & (1 << 5))
#define HCC_LTC(p)		((p) & (1 << 6))
#define HCC_NSS(p)		((p) & (1 << 7))
#define HCC_MAX_PSA(p)		(1 << ((((p) >> 12) & 0xf) + 1))
#define HCC_EXT_CAPS(p)		XHCI_HCC_EXT_CAPS(p)

#define	DBOFF_MASK	(~0x3)

#define	RTSOFF_MASK	(~0x1f)


#define	NUM_PORT_REGS	4

struct xhci_op_regs {
	__le32	command;
	__le32	status;
	__le32	page_size;
	__le32	reserved1;
	__le32	reserved2;
	__le32	dev_notification;
	__le64	cmd_ring;
	
	__le32	reserved3[4];
	__le64	dcbaa_ptr;
	__le32	config_reg;
	
	__le32	reserved4[241];
	
	__le32	port_status_base;
	__le32	port_power_base;
	__le32	port_link_base;
	__le32	reserved5;
	
	__le32	reserved6[NUM_PORT_REGS*254];
};

#define CMD_RUN		XHCI_CMD_RUN
#define CMD_RESET	(1 << 1)
#define CMD_EIE		XHCI_CMD_EIE
#define CMD_HSEIE	XHCI_CMD_HSEIE
#define CMD_LRESET	(1 << 7)
#define CMD_CSS		(1 << 8)
#define CMD_CRS		(1 << 9)
#define CMD_EWE		XHCI_CMD_EWE
#define CMD_PM_INDEX	(1 << 11)

#define IMAN_IP		(1 << 1)
#define IMAN_IE		(1 << 0)

#define STS_HALT	XHCI_STS_HALT
#define STS_FATAL	(1 << 2)
#define STS_EINT	(1 << 3)
#define STS_PORT	(1 << 4)
#define STS_SAVE	(1 << 8)
#define STS_RESTORE	(1 << 9)
#define STS_SRE		(1 << 10)
#define STS_CNR		XHCI_STS_CNR
#define STS_HCE		(1 << 12)

#define	DEV_NOTE_MASK		(0xffff)
#define ENABLE_DEV_NOTE(x)	(1 << (x))
#define	DEV_NOTE_FWAKE		ENABLE_DEV_NOTE(1)

#define CMD_RING_PAUSE		(1 << 1)
#define CMD_RING_ABORT		(1 << 2)
#define CMD_RING_RUNNING	(1 << 3)
#define CMD_RING_RSVD_BITS	(0x3f)

#define MAX_DEVS(p)	((p) & 0xff)

#define PORT_CONNECT	(1 << 0)
#define PORT_PE		(1 << 1)
#define PORT_OC		(1 << 3)
#define PORT_RESET	(1 << 4)
#define PORT_PLS_MASK	(0xf << 5)
#define XDEV_U0		(0x0 << 5)
#define XDEV_U2		(0x2 << 5)
#define XDEV_U3		(0x3 << 5)
#define XDEV_RESUME	(0xf << 5)
#define PORT_POWER	(1 << 9)
#define DEV_SPEED_MASK		(0xf << 10)
#define	XDEV_FS			(0x1 << 10)
#define	XDEV_LS			(0x2 << 10)
#define	XDEV_HS			(0x3 << 10)
#define	XDEV_SS			(0x4 << 10)
#define DEV_UNDEFSPEED(p)	(((p) & DEV_SPEED_MASK) == (0x0<<10))
#define DEV_FULLSPEED(p)	(((p) & DEV_SPEED_MASK) == XDEV_FS)
#define DEV_LOWSPEED(p)		(((p) & DEV_SPEED_MASK) == XDEV_LS)
#define DEV_HIGHSPEED(p)	(((p) & DEV_SPEED_MASK) == XDEV_HS)
#define DEV_SUPERSPEED(p)	(((p) & DEV_SPEED_MASK) == XDEV_SS)
#define	SLOT_SPEED_FS		(XDEV_FS << 10)
#define	SLOT_SPEED_LS		(XDEV_LS << 10)
#define	SLOT_SPEED_HS		(XDEV_HS << 10)
#define	SLOT_SPEED_SS		(XDEV_SS << 10)
#define PORT_LED_OFF	(0 << 14)
#define PORT_LED_AMBER	(1 << 14)
#define PORT_LED_GREEN	(2 << 14)
#define PORT_LED_MASK	(3 << 14)
#define PORT_LINK_STROBE	(1 << 16)
#define PORT_CSC	(1 << 17)
#define PORT_PEC	(1 << 18)
#define PORT_WRC	(1 << 19)
#define PORT_OCC	(1 << 20)
#define PORT_RC		(1 << 21)
#define PORT_PLC	(1 << 22)
#define PORT_CEC	(1 << 23)
#define PORT_WKCONN_E	(1 << 25)
#define PORT_WKDISC_E	(1 << 26)
#define PORT_WKOC_E	(1 << 27)
#define PORT_DEV_REMOVE	(1 << 30)
#define PORT_WR		(1 << 31)

#define DUPLICATE_ENTRY ((u8)(-1))

#define PORT_U1_TIMEOUT(p)	((p) & 0xff)
#define PORT_U2_TIMEOUT(p)	(((p) & 0xff) << 8)

#define	PORT_L1S_MASK		7
#define	PORT_L1S_SUCCESS	1
#define	PORT_RWE		(1 << 3)
#define	PORT_HIRD(p)		(((p) & 0xf) << 4)
#define	PORT_HIRD_MASK		(0xf << 4)
#define	PORT_L1DS(p)		(((p) & 0xff) << 8)
#define	PORT_HLE		(1 << 16)

struct xhci_intr_reg {
	__le32	irq_pending;
	__le32	irq_control;
	__le32	erst_size;
	__le32	rsvd;
	__le64	erst_base;
	__le64	erst_dequeue;
};

#define	ER_IRQ_PENDING(p)	((p) & 0x1)
#define	ER_IRQ_CLEAR(p)		((p) & 0xfffffffe)
#define	ER_IRQ_ENABLE(p)	((ER_IRQ_CLEAR(p)) | 0x2)
#define	ER_IRQ_DISABLE(p)	((ER_IRQ_CLEAR(p)) & ~(0x2))

#define ER_IRQ_INTERVAL_MASK	(0xffff)
#define ER_IRQ_COUNTER_MASK	(0xffff << 16)

#define	ERST_SIZE_MASK		(0xffff << 16)

#define ERST_DESI_MASK		(0x7)
#define ERST_EHB		(1 << 3)
#define ERST_PTR_MASK		(0xf)

struct xhci_run_regs {
	__le32			microframe_index;
	__le32			rsvd[7];
	struct xhci_intr_reg	ir_set[128];
};

struct xhci_doorbell_array {
	__le32	doorbell[256];
};

#define DB_VALUE(ep, stream)	((((ep) + 1) & 0xff) | ((stream) << 16))
#define DB_VALUE_HOST		0x00000000

struct xhci_protocol_caps {
	u32	revision;
	u32	name_string;
	u32	port_info;
};

#define	XHCI_EXT_PORT_MAJOR(x)	(((x) >> 24) & 0xff)
#define	XHCI_EXT_PORT_OFF(x)	((x) & 0xff)
#define	XHCI_EXT_PORT_COUNT(x)	(((x) >> 8) & 0xff)

struct xhci_container_ctx {
	unsigned type;
#define XHCI_CTX_TYPE_DEVICE  0x1
#define XHCI_CTX_TYPE_INPUT   0x2

	int size;

	u8 *bytes;
	dma_addr_t dma;
};

struct xhci_slot_ctx {
	__le32	dev_info;
	__le32	dev_info2;
	__le32	tt_info;
	__le32	dev_state;
	
	__le32	reserved[4];
};

#define ROUTE_STRING_MASK	(0xfffff)
#define DEV_SPEED	(0xf << 20)
#define DEV_MTT		(0x1 << 25)
#define DEV_HUB		(0x1 << 26)
#define LAST_CTX_MASK	(0x1f << 27)
#define LAST_CTX(p)	((p) << 27)
#define LAST_CTX_TO_EP_NUM(p)	(((p) >> 27) - 1)
#define SLOT_FLAG	(1 << 0)
#define EP0_FLAG	(1 << 1)

#define MAX_EXIT	(0xffff)
#define ROOT_HUB_PORT(p)	(((p) & 0xff) << 16)
#define DEVINFO_TO_ROOT_HUB_PORT(p)	(((p) >> 16) & 0xff)
#define XHCI_MAX_PORTS(p)	(((p) & 0xff) << 24)

#define TT_SLOT		(0xff)
#define TT_PORT		(0xff << 8)
#define TT_THINK_TIME(p)	(((p) & 0x3) << 16)

#define DEV_ADDR_MASK	(0xff)
#define SLOT_STATE	(0x1f << 27)
#define GET_SLOT_STATE(p)	(((p) & (0x1f << 27)) >> 27)

#define SLOT_STATE_DISABLED	0
#define SLOT_STATE_ENABLED	SLOT_STATE_DISABLED
#define SLOT_STATE_DEFAULT	1
#define SLOT_STATE_ADDRESSED	2
#define SLOT_STATE_CONFIGURED	3

struct xhci_ep_ctx {
	__le32	ep_info;
	__le32	ep_info2;
	__le64	deq;
	__le32	tx_info;
	
	__le32	reserved[3];
};

#define EP_STATE_MASK		(0xf)
#define EP_STATE_DISABLED	0
#define EP_STATE_RUNNING	1
#define EP_STATE_HALTED		2
#define EP_STATE_STOPPED	3
#define EP_STATE_ERROR		4
#define EP_MULT(p)		(((p) & 0x3) << 8)
#define CTX_TO_EP_MULT(p)	(((p) >> 8) & 0x3)
#define EP_INTERVAL(p)		(((p) & 0xff) << 16)
#define EP_INTERVAL_TO_UFRAMES(p)		(1 << (((p) >> 16) & 0xff))
#define CTX_TO_EP_INTERVAL(p)	(((p) >> 16) & 0xff)
#define EP_MAXPSTREAMS_MASK	(0x1f << 10)
#define EP_MAXPSTREAMS(p)	(((p) << 10) & EP_MAXPSTREAMS_MASK)
#define	EP_HAS_LSA		(1 << 15)

#define	FORCE_EVENT	(0x1)
#define ERROR_COUNT(p)	(((p) & 0x3) << 1)
#define CTX_TO_EP_TYPE(p)	(((p) >> 3) & 0x7)
#define EP_TYPE(p)	((p) << 3)
#define ISOC_OUT_EP	1
#define BULK_OUT_EP	2
#define INT_OUT_EP	3
#define CTRL_EP		4
#define ISOC_IN_EP	5
#define BULK_IN_EP	6
#define INT_IN_EP	7
#define MAX_BURST(p)	(((p)&0xff) << 8)
#define CTX_TO_MAX_BURST(p)	(((p) >> 8) & 0xff)
#define MAX_PACKET(p)	(((p)&0xffff) << 16)
#define MAX_PACKET_MASK		(0xffff << 16)
#define MAX_PACKET_DECODED(p)	(((p) >> 16) & 0xffff)

#define GET_MAX_PACKET(p)	((p) & 0x7ff)

#define AVG_TRB_LENGTH_FOR_EP(p)	((p) & 0xffff)
#define MAX_ESIT_PAYLOAD_FOR_EP(p)	(((p) & 0xffff) << 16)
#define CTX_TO_MAX_ESIT_PAYLOAD(p)	(((p) >> 16) & 0xffff)

#define EP_CTX_CYCLE_MASK		(1 << 0)


struct xhci_input_control_ctx {
	__le32	drop_flags;
	__le32	add_flags;
	__le32	rsvd2[6];
};

#define	EP_IS_ADDED(ctrl_ctx, i) \
	(le32_to_cpu(ctrl_ctx->add_flags) & (1 << (i + 1)))
#define	EP_IS_DROPPED(ctrl_ctx, i)       \
	(le32_to_cpu(ctrl_ctx->drop_flags) & (1 << (i + 1)))

struct xhci_command {
	
	struct xhci_container_ctx	*in_ctx;
	u32				status;
	struct completion		*completion;
	union xhci_trb			*command_trb;
	struct list_head		cmd_list;
};

#define	DROP_EP(x)	(0x1 << x)
#define	ADD_EP(x)	(0x1 << x)

struct xhci_stream_ctx {
	
	__le64	stream_ring;
	
	__le32	reserved[2];
};

#define	SCT_FOR_CTX(p)		(((p) << 1) & 0x7)
#define	SCT_SEC_TR		0
#define	SCT_PRI_TR		1
#define SCT_SSA_8		2
#define SCT_SSA_16		3
#define SCT_SSA_32		4
#define SCT_SSA_64		5
#define SCT_SSA_128		6
#define SCT_SSA_256		7

struct xhci_stream_info {
	struct xhci_ring		**stream_rings;
	
	unsigned int			num_streams;
	struct xhci_stream_ctx		*stream_ctx_array;
	unsigned int			num_stream_ctxs;
	dma_addr_t			ctx_array_dma;
	
	struct radix_tree_root		trb_address_map;
	struct xhci_command		*free_streams_command;
};

#define	SMALL_STREAM_ARRAY_SIZE		256
#define	MEDIUM_STREAM_ARRAY_SIZE	1024

struct xhci_bw_info {
	
	unsigned int		ep_interval;
	
	unsigned int		mult;
	unsigned int		num_packets;
	unsigned int		max_packet_size;
	unsigned int		max_esit_payload;
	unsigned int		type;
};

#define	FS_BLOCK	1
#define	HS_BLOCK	4
#define	SS_BLOCK	16
#define	DMI_BLOCK	32

#define DMI_OVERHEAD 8
#define DMI_OVERHEAD_BURST 4
#define SS_OVERHEAD 8
#define SS_OVERHEAD_BURST 32
#define HS_OVERHEAD 26
#define FS_OVERHEAD 20
#define LS_OVERHEAD 128
#define TT_HS_OVERHEAD (31 + 94)
#define TT_DMI_OVERHEAD (25 + 12)

#define FS_BW_LIMIT		1285
#define TT_BW_LIMIT		1320
#define HS_BW_LIMIT		1607
#define SS_BW_LIMIT_IN		3906
#define DMI_BW_LIMIT_IN		3906
#define SS_BW_LIMIT_OUT		3906
#define DMI_BW_LIMIT_OUT	3906

#define FS_BW_RESERVED		10
#define HS_BW_RESERVED		20
#define SS_BW_RESERVED		10

struct xhci_virt_ep {
	struct xhci_ring		*ring;
	
	struct xhci_stream_info		*stream_info;
	struct xhci_ring		*new_ring;
	unsigned int			ep_state;
#define SET_DEQ_PENDING		(1 << 0)
#define EP_HALTED		(1 << 1)	
#define EP_HALT_PENDING		(1 << 2)	
#define EP_GETTING_STREAMS	(1 << 3)
#define EP_HAS_STREAMS		(1 << 4)
#define EP_GETTING_NO_STREAMS	(1 << 5)
	
	struct list_head	cancelled_td_list;
	
	union xhci_trb		*stopped_trb;
	struct xhci_td		*stopped_td;
	unsigned int		stopped_stream;
	
	struct timer_list	stop_cmd_timer;
	int			stop_cmds_pending;
	struct xhci_hcd		*xhci;
	struct xhci_segment	*queued_deq_seg;
	union xhci_trb		*queued_deq_ptr;
	bool			skip;
	
	struct xhci_bw_info	bw_info;
	struct list_head	bw_endpoint_list;
};

enum xhci_overhead_type {
	LS_OVERHEAD_TYPE = 0,
	FS_OVERHEAD_TYPE,
	HS_OVERHEAD_TYPE,
};

struct xhci_interval_bw {
	unsigned int		num_packets;
	struct list_head	endpoints;
	
	unsigned int		overhead[3];
};

#define	XHCI_MAX_INTERVAL	16

struct xhci_interval_bw_table {
	unsigned int		interval0_esit_payload;
	struct xhci_interval_bw	interval_bw[XHCI_MAX_INTERVAL];
	
	unsigned int		bw_used;
	unsigned int		ss_bw_in;
	unsigned int		ss_bw_out;
};


struct xhci_virt_device {
	struct usb_device		*udev;
	struct xhci_container_ctx       *out_ctx;
	
	struct xhci_container_ctx       *in_ctx;
	
	struct xhci_ring		**ring_cache;
	int				num_rings_cached;
	
	int				address;
#define	XHCI_MAX_RINGS_CACHED	31
	struct xhci_virt_ep		eps[31];
	struct completion		cmd_completion;
	
	u32				cmd_status;
	struct list_head		cmd_list;
	u8				fake_port;
	u8				real_port;
	struct xhci_interval_bw_table	*bw_table;
	struct xhci_tt_bw_info		*tt_info;
};

struct xhci_root_port_bw_info {
	struct list_head		tts;
	unsigned int			num_active_tts;
	struct xhci_interval_bw_table	bw_table;
};

struct xhci_tt_bw_info {
	struct list_head		tt_list;
	int				slot_id;
	int				ttport;
	struct xhci_interval_bw_table	bw_table;
	int				active_eps;
};


struct xhci_device_context_array {
	
	__le64			dev_context_ptrs[MAX_HC_SLOTS];
	
	dma_addr_t	dma;
};


struct xhci_transfer_event {
	
	__le64	buffer;
	__le32	transfer_len;
	
	__le32	flags;
};

#define	TRB_TO_EP_ID(p)	(((p) >> 16) & 0x1f)

#define	COMP_CODE_MASK		(0xff << 24)
#define GET_COMP_CODE(p)	(((p) & COMP_CODE_MASK) >> 24)
#define COMP_SUCCESS	1
#define COMP_DB_ERR	2
#define COMP_BABBLE	3
#define COMP_TX_ERR	4
#define COMP_TRB_ERR	5
#define COMP_STALL	6
#define COMP_ENOMEM	7
#define COMP_BW_ERR	8
#define COMP_ENOSLOTS	9
#define COMP_STREAM_ERR	10
#define COMP_EBADSLT	11
#define COMP_EBADEP	12
#define COMP_SHORT_TX	13
#define COMP_UNDERRUN	14
#define COMP_OVERRUN	15
#define COMP_VF_FULL	16
#define COMP_EINVAL	17
#define COMP_BW_OVER	18
#define COMP_CTX_STATE	19
#define COMP_PING_ERR	20
#define COMP_ER_FULL	21
#define COMP_DEV_ERR	22
#define COMP_MISSED_INT	23
#define COMP_CMD_STOP	24
#define COMP_CMD_ABORT	25
#define COMP_STOP	26
#define COMP_STOP_INVAL	27
#define COMP_DBG_ABORT	28
#define COMP_MEL_ERR	29
#define COMP_BUFF_OVER	31
#define COMP_ISSUES	32
#define COMP_UNKNOWN	33
#define COMP_STRID_ERR	34
#define COMP_2ND_BW_ERR	35
#define	COMP_SPLIT_ERR	36

struct xhci_link_trb {
	
	__le64 segment_ptr;
	__le32 intr_target;
	__le32 control;
};

#define LINK_TOGGLE	(0x1<<1)

struct xhci_event_cmd {
	
	__le64 cmd_trb;
	__le32 status;
	__le32 flags;
};

#define TRB_TO_SLOT_ID(p)	(((p) & (0xff<<24)) >> 24)
#define SLOT_ID_FOR_TRB(p)	(((p) & 0xff) << 24)

#define TRB_TO_EP_INDEX(p)		((((p) & (0x1f << 16)) >> 16) - 1)
#define	EP_ID_FOR_TRB(p)		((((p) + 1) & 0x1f) << 16)

#define SUSPEND_PORT_FOR_TRB(p)		(((p) & 1) << 23)
#define TRB_TO_SUSPEND_PORT(p)		(((p) & (1 << 23)) >> 23)
#define LAST_EP_INDEX			30

#define TRB_TO_STREAM_ID(p)		((((p) & (0xffff << 16)) >> 16))
#define STREAM_ID_FOR_TRB(p)		((((p)) & 0xffff) << 16)


#define GET_PORT_ID(p)		(((p) & (0xff << 24)) >> 24)

#define	TRB_LEN(p)		((p) & 0x1ffff)
#define TRB_INTR_TARGET(p)	(((p) & 0x3ff) << 22)
#define GET_INTR_TARGET(p)	(((p) >> 22) & 0x3ff)
#define TRB_TBC(p)		(((p) & 0x3) << 7)
#define TRB_TLBPC(p)		(((p) & 0xf) << 16)

#define TRB_CYCLE		(1<<0)
#define TRB_ENT			(1<<1)
#define TRB_ISP			(1<<2)
#define TRB_NO_SNOOP		(1<<3)
#define TRB_CHAIN		(1<<4)
#define TRB_IOC			(1<<5)
#define TRB_IDT			(1<<6)

#define	TRB_BEI			(1<<9)

#define TRB_DIR_IN		(1<<16)
#define	TRB_TX_TYPE(p)		((p) << 16)
#define	TRB_DATA_OUT		2
#define	TRB_DATA_IN		3

#define TRB_SIA			(1<<31)

struct xhci_generic_trb {
	__le32 field[4];
};

union xhci_trb {
	struct xhci_link_trb		link;
	struct xhci_transfer_event	trans_event;
	struct xhci_event_cmd		event_cmd;
	struct xhci_generic_trb		generic;
};

#define	TRB_TYPE_BITMASK	(0xfc00)
#define TRB_TYPE(p)		((p) << 10)
#define TRB_FIELD_TO_TYPE(p)	(((p) & TRB_TYPE_BITMASK) >> 10)
#define TRB_NORMAL		1
#define TRB_SETUP		2
#define TRB_DATA		3
#define TRB_STATUS		4
#define TRB_ISOC		5
#define TRB_LINK		6
#define TRB_EVENT_DATA		7
#define TRB_TR_NOOP		8
#define TRB_ENABLE_SLOT		9
#define TRB_DISABLE_SLOT	10
#define TRB_ADDR_DEV		11
#define TRB_CONFIG_EP		12
#define TRB_EVAL_CONTEXT	13
#define TRB_RESET_EP		14
#define TRB_STOP_RING		15
#define TRB_SET_DEQ		16
#define TRB_RESET_DEV		17
#define TRB_FORCE_EVENT		18
#define TRB_NEG_BANDWIDTH	19
#define TRB_SET_LT		20
#define TRB_GET_BW		21
#define TRB_FORCE_HEADER	22
#define TRB_CMD_NOOP		23
#define TRB_TRANSFER		32
#define TRB_COMPLETION		33
#define TRB_PORT_STATUS		34
#define TRB_BANDWIDTH_EVENT	35
#define TRB_DOORBELL		36
#define TRB_HC_EVENT		37
#define TRB_DEV_NOTE		38
#define TRB_MFINDEX_WRAP	39

#define	TRB_NEC_CMD_COMP	48
#define	TRB_NEC_GET_FW		49

#define TRB_TYPE_LINK(x)	(((x) & TRB_TYPE_BITMASK) == TRB_TYPE(TRB_LINK))
#define TRB_TYPE_LINK_LE32(x)	(((x) & cpu_to_le32(TRB_TYPE_BITMASK)) == \
				 cpu_to_le32(TRB_TYPE(TRB_LINK)))
#define TRB_TYPE_NOOP_LE32(x)	(((x) & cpu_to_le32(TRB_TYPE_BITMASK)) == \
				 cpu_to_le32(TRB_TYPE(TRB_TR_NOOP)))

#define NEC_FW_MINOR(p)		(((p) >> 0) & 0xff)
#define NEC_FW_MAJOR(p)		(((p) >> 8) & 0xff)

#define TRBS_PER_SEGMENT	64
#define MAX_RSVD_CMD_TRBS	(TRBS_PER_SEGMENT - 3)
#define SEGMENT_SIZE		(TRBS_PER_SEGMENT*16)
#define SEGMENT_SHIFT		(__ffs(SEGMENT_SIZE))
#define TRB_MAX_BUFF_SHIFT		16
#define TRB_MAX_BUFF_SIZE	(1 << TRB_MAX_BUFF_SHIFT)

struct xhci_segment {
	union xhci_trb		*trbs;
	
	struct xhci_segment	*next;
	dma_addr_t		dma;
};

struct xhci_td {
	struct list_head	td_list;
	struct list_head	cancelled_td_list;
	struct urb		*urb;
	struct xhci_segment	*start_seg;
	union xhci_trb		*first_trb;
	union xhci_trb		*last_trb;
};

#define XHCI_CMD_DEFAULT_TIMEOUT	(5 * HZ)

struct xhci_dequeue_state {
	struct xhci_segment *new_deq_seg;
	union xhci_trb *new_deq_ptr;
	int new_cycle_state;
};

enum xhci_ring_type {
	TYPE_CTRL = 0,
	TYPE_ISOC,
	TYPE_BULK,
	TYPE_INTR,
	TYPE_STREAM,
	TYPE_COMMAND,
	TYPE_EVENT,
};

struct xhci_ring {
	struct xhci_segment	*first_seg;
	struct xhci_segment	*last_seg;
	union  xhci_trb		*enqueue;
	struct xhci_segment	*enq_seg;
	unsigned int		enq_updates;
	union  xhci_trb		*dequeue;
	struct xhci_segment	*deq_seg;
	unsigned int		deq_updates;
	struct list_head	td_list;
	u32			cycle_state;
	unsigned int		stream_id;
	unsigned int		num_segs;
	unsigned int		num_trbs_free;
	unsigned int		num_trbs_free_temp;
	enum xhci_ring_type	type;
	bool			last_td_was_short;
};

struct xhci_erst_entry {
	
	__le64	seg_addr;
	__le32	seg_size;
	
	__le32	rsvd;
};

struct xhci_erst {
	struct xhci_erst_entry	*entries;
	unsigned int		num_entries;
	
	dma_addr_t		erst_dma_addr;
	
	unsigned int		erst_size;
};

struct xhci_scratchpad {
	u64 *sp_array;
	dma_addr_t sp_dma;
	void **sp_buffers;
	dma_addr_t *sp_dma_buffers;
};

struct urb_priv {
	int	length;
	int	td_cnt;
	struct	xhci_td	*td[0];
};

#define	ERST_NUM_SEGS	1
#define	ERST_SIZE	64
#define	ERST_ENTRIES	1
#define	POLL_TIMEOUT	60
#define XHCI_STOP_EP_CMD_TIMEOUT	5

struct s3_save {
	u32	command;
	u32	dev_nt;
	u64	dcbaa_ptr;
	u32	config_reg;
	u32	irq_pending;
	u32	irq_control;
	u32	erst_size;
	u64	erst_base;
	u64	erst_dequeue;
};

struct dev_info {
	u32			dev_id;
	struct	list_head	list;
};

struct xhci_bus_state {
	unsigned long		bus_suspended;
	unsigned long		next_statechange;

	
	
	u32			port_c_suspend;
	u32			suspended_ports;
	u32			port_remote_wakeup;
	unsigned long		resume_done[USB_MAXCHILDREN];
};

static inline unsigned int hcd_index(struct usb_hcd *hcd)
{
	if (hcd->speed == HCD_USB3)
		return 0;
	else
		return 1;
}

struct xhci_hcd {
	struct usb_hcd *main_hcd;
	struct usb_hcd *shared_hcd;
	
	struct xhci_cap_regs __iomem *cap_regs;
	struct xhci_op_regs __iomem *op_regs;
	struct xhci_run_regs __iomem *run_regs;
	struct xhci_doorbell_array __iomem *dba;
	
	struct	xhci_intr_reg __iomem *ir_set;

	
	__u32		hcs_params1;
	__u32		hcs_params2;
	__u32		hcs_params3;
	__u32		hcc_params;

	spinlock_t	lock;

	
	u8		sbrn;
	u16		hci_version;
	u8		max_slots;
	u8		max_interrupters;
	u8		max_ports;
	u8		isoc_threshold;
	int		event_ring_max;
	int		addr_64;
	
	int		page_size;
	
	int		page_shift;
	
	int		msix_count;
	struct msix_entry	*msix_entries;
	
	struct xhci_device_context_array *dcbaa;
	struct xhci_ring	*cmd_ring;
	unsigned int		cmd_ring_reserved_trbs;
	struct xhci_ring	*event_ring;
	struct xhci_erst	erst;
	
	struct xhci_scratchpad  *scratchpad;
	
	struct list_head	lpm_failed_devs;

	
	struct completion	addr_dev;
	int slot_id;
	
	struct xhci_virt_device	*devs[MAX_HC_SLOTS];
	
	struct xhci_root_port_bw_info	*rh_bw;

	
	struct dma_pool	*device_pool;
	struct dma_pool	*segment_pool;
	struct dma_pool	*small_streams_pool;
	struct dma_pool	*medium_streams_pool;

#ifdef CONFIG_USB_XHCI_HCD_DEBUGGING
	
	struct timer_list	event_ring_timer;
	int			zombie;
#endif
	
	unsigned int		xhc_state;

	u32			command;
	struct s3_save		s3;
#define XHCI_STATE_DYING	(1 << 0)
#define XHCI_STATE_HALTED	(1 << 1)
	
	int			error_bitmask;
	unsigned int		quirks;
#define	XHCI_LINK_TRB_QUIRK	(1 << 0)
#define XHCI_RESET_EP_QUIRK	(1 << 1)
#define XHCI_NEC_HOST		(1 << 2)
#define XHCI_AMD_PLL_FIX	(1 << 3)
#define XHCI_SPURIOUS_SUCCESS	(1 << 4)
#define XHCI_EP_LIMIT_QUIRK	(1 << 5)
#define XHCI_BROKEN_MSI		(1 << 6)
#define XHCI_RESET_ON_RESUME	(1 << 7)
#define	XHCI_SW_BW_CHECKING	(1 << 8)
#define XHCI_AMD_0x96_HOST	(1 << 9)
#define XHCI_PORTSC_DELAY	(1 << 10)
#define XHCI_RESET_DELAY	(1 << 11)
	unsigned int		num_active_eps;
	unsigned int		limit_active_eps;
	
	struct xhci_bus_state   bus_state[2];
	
	u8			*port_array;
	
	__le32 __iomem		**usb3_ports;
	unsigned int		num_usb3_ports;
	
	__le32 __iomem		**usb2_ports;
	unsigned int		num_usb2_ports;
	
	unsigned		sw_lpm_support:1;
	
	unsigned		hw_lpm_support:1;
};

static inline struct xhci_hcd *hcd_to_xhci(struct usb_hcd *hcd)
{
	return *((struct xhci_hcd **) (hcd->hcd_priv));
}

static inline struct usb_hcd *xhci_to_hcd(struct xhci_hcd *xhci)
{
	return xhci->main_hcd;
}

#ifdef CONFIG_USB_XHCI_HCD_DEBUGGING
#define XHCI_DEBUG	1
#else
#define XHCI_DEBUG	0
#endif

#define xhci_dbg(xhci, fmt, args...) \
	do { if (XHCI_DEBUG) dev_dbg(xhci_to_hcd(xhci)->self.controller , fmt , ## args); } while (0)
#define xhci_info(xhci, fmt, args...) \
	do { if (XHCI_DEBUG) dev_info(xhci_to_hcd(xhci)->self.controller , fmt , ## args); } while (0)
#define xhci_err(xhci, fmt, args...) \
	dev_err(xhci_to_hcd(xhci)->self.controller , fmt , ## args)
#define xhci_warn(xhci, fmt, args...) \
	dev_warn(xhci_to_hcd(xhci)->self.controller , fmt , ## args)

static inline unsigned int xhci_readl(const struct xhci_hcd *xhci,
		__le32 __iomem *regs)
{
	return readl(regs);
}
static inline void xhci_writel(struct xhci_hcd *xhci,
		const unsigned int val, __le32 __iomem *regs)
{
	writel(val, regs);
}

/*
 * Registers should always be accessed with double word or quad word accesses.
 *
 * Some xHCI implementations may support 64-bit address pointers.  Registers
 * with 64-bit address pointers should be written to with dword accesses by
 * writing the low dword first (ptr[0]), then the high dword (ptr[1]) second.
 * xHCI implementations that do not support 64-bit address pointers will ignore
 * the high dword, and write order is irrelevant.
 */
static inline u64 xhci_read_64(const struct xhci_hcd *xhci,
		__le64 __iomem *regs)
{
	__u32 __iomem *ptr = (__u32 __iomem *) regs;
	u64 val_lo = readl(ptr);
	u64 val_hi = readl(ptr + 1);
	return val_lo + (val_hi << 32);
}
static inline void xhci_write_64(struct xhci_hcd *xhci,
				 const u64 val, __le64 __iomem *regs)
{
	__u32 __iomem *ptr = (__u32 __iomem *) regs;
	u32 val_lo = lower_32_bits(val);
	u32 val_hi = upper_32_bits(val);

	writel(val_lo, ptr);
	writel(val_hi, ptr + 1);
}

static inline int xhci_link_trb_quirk(struct xhci_hcd *xhci)
{
	return xhci->quirks & XHCI_LINK_TRB_QUIRK;
}

void xhci_print_ir_set(struct xhci_hcd *xhci, int set_num);
void xhci_print_registers(struct xhci_hcd *xhci);
void xhci_dbg_regs(struct xhci_hcd *xhci);
void xhci_print_run_regs(struct xhci_hcd *xhci);
void xhci_print_trb_offsets(struct xhci_hcd *xhci, union xhci_trb *trb);
void xhci_debug_trb(struct xhci_hcd *xhci, union xhci_trb *trb);
void xhci_debug_segment(struct xhci_hcd *xhci, struct xhci_segment *seg);
void xhci_debug_ring(struct xhci_hcd *xhci, struct xhci_ring *ring);
void xhci_dbg_erst(struct xhci_hcd *xhci, struct xhci_erst *erst);
void xhci_dbg_cmd_ptrs(struct xhci_hcd *xhci);
void xhci_dbg_ring_ptrs(struct xhci_hcd *xhci, struct xhci_ring *ring);
void xhci_dbg_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx, unsigned int last_ep);
char *xhci_get_slot_state(struct xhci_hcd *xhci,
		struct xhci_container_ctx *ctx);
void xhci_dbg_ep_rings(struct xhci_hcd *xhci,
		unsigned int slot_id, unsigned int ep_index,
		struct xhci_virt_ep *ep);

void xhci_mem_cleanup(struct xhci_hcd *xhci);
int xhci_mem_init(struct xhci_hcd *xhci, gfp_t flags);
void xhci_free_virt_device(struct xhci_hcd *xhci, int slot_id);
int xhci_alloc_virt_device(struct xhci_hcd *xhci, int slot_id, struct usb_device *udev, gfp_t flags);
int xhci_setup_addressable_virt_dev(struct xhci_hcd *xhci, struct usb_device *udev);
void xhci_copy_ep0_dequeue_into_input_ctx(struct xhci_hcd *xhci,
		struct usb_device *udev);
unsigned int xhci_get_endpoint_index(struct usb_endpoint_descriptor *desc);
unsigned int xhci_get_endpoint_flag(struct usb_endpoint_descriptor *desc);
unsigned int xhci_get_endpoint_flag_from_index(unsigned int ep_index);
unsigned int xhci_last_valid_endpoint(u32 added_ctxs);
void xhci_endpoint_zero(struct xhci_hcd *xhci, struct xhci_virt_device *virt_dev, struct usb_host_endpoint *ep);
void xhci_drop_ep_from_interval_table(struct xhci_hcd *xhci,
		struct xhci_bw_info *ep_bw,
		struct xhci_interval_bw_table *bw_table,
		struct usb_device *udev,
		struct xhci_virt_ep *virt_ep,
		struct xhci_tt_bw_info *tt_info);
void xhci_update_tt_active_eps(struct xhci_hcd *xhci,
		struct xhci_virt_device *virt_dev,
		int old_active_eps);
void xhci_clear_endpoint_bw_info(struct xhci_bw_info *bw_info);
void xhci_update_bw_info(struct xhci_hcd *xhci,
		struct xhci_container_ctx *in_ctx,
		struct xhci_input_control_ctx *ctrl_ctx,
		struct xhci_virt_device *virt_dev);
void xhci_endpoint_copy(struct xhci_hcd *xhci,
		struct xhci_container_ctx *in_ctx,
		struct xhci_container_ctx *out_ctx,
		unsigned int ep_index);
void xhci_slot_copy(struct xhci_hcd *xhci,
		struct xhci_container_ctx *in_ctx,
		struct xhci_container_ctx *out_ctx);
int xhci_endpoint_init(struct xhci_hcd *xhci, struct xhci_virt_device *virt_dev,
		struct usb_device *udev, struct usb_host_endpoint *ep,
		gfp_t mem_flags);
void xhci_ring_free(struct xhci_hcd *xhci, struct xhci_ring *ring);
int xhci_ring_expansion(struct xhci_hcd *xhci, struct xhci_ring *ring,
				unsigned int num_trbs, gfp_t flags);
void xhci_free_or_cache_endpoint_ring(struct xhci_hcd *xhci,
		struct xhci_virt_device *virt_dev,
		unsigned int ep_index);
struct xhci_stream_info *xhci_alloc_stream_info(struct xhci_hcd *xhci,
		unsigned int num_stream_ctxs,
		unsigned int num_streams, gfp_t flags);
void xhci_free_stream_info(struct xhci_hcd *xhci,
		struct xhci_stream_info *stream_info);
void xhci_setup_streams_ep_input_ctx(struct xhci_hcd *xhci,
		struct xhci_ep_ctx *ep_ctx,
		struct xhci_stream_info *stream_info);
void xhci_setup_no_streams_ep_input_ctx(struct xhci_hcd *xhci,
		struct xhci_ep_ctx *ep_ctx,
		struct xhci_virt_ep *ep);
void xhci_free_device_endpoint_resources(struct xhci_hcd *xhci,
	struct xhci_virt_device *virt_dev, bool drop_control_ep);
struct xhci_ring *xhci_dma_to_transfer_ring(
		struct xhci_virt_ep *ep,
		u64 address);
struct xhci_ring *xhci_stream_id_to_ring(
		struct xhci_virt_device *dev,
		unsigned int ep_index,
		unsigned int stream_id);
struct xhci_command *xhci_alloc_command(struct xhci_hcd *xhci,
		bool allocate_in_ctx, bool allocate_completion,
		gfp_t mem_flags);
void xhci_urb_free_priv(struct xhci_hcd *xhci, struct urb_priv *urb_priv);
void xhci_free_command(struct xhci_hcd *xhci,
		struct xhci_command *command);

#ifdef CONFIG_PCI
int xhci_register_pci(void);
void xhci_unregister_pci(void);
#else
static inline int xhci_register_pci(void) { return 0; }
static inline void xhci_unregister_pci(void) {}
#endif

struct xhci_plat_data {
	unsigned vendor;
	unsigned revision;
};

#if defined(CONFIG_USB_XHCI_PLATFORM) \
	|| defined(CONFIG_USB_XHCI_PLATFORM_MODULE)
int xhci_register_plat(void);
void xhci_unregister_plat(void);
#else
static inline int xhci_register_plat(void)
{ return 0; }
static inline void xhci_unregister_plat(void)
{  }
#endif

typedef void (*xhci_get_quirks_t)(struct device *, struct xhci_hcd *);
void xhci_quiesce(struct xhci_hcd *xhci);
int xhci_halt(struct xhci_hcd *xhci);
int xhci_reset(struct xhci_hcd *xhci);
int xhci_init(struct usb_hcd *hcd);
int xhci_run(struct usb_hcd *hcd);
void xhci_stop(struct usb_hcd *hcd);
void xhci_shutdown(struct usb_hcd *hcd);
int xhci_gen_setup(struct usb_hcd *hcd, xhci_get_quirks_t get_quirks);

#ifdef	CONFIG_PM
int xhci_suspend(struct xhci_hcd *xhci);
int xhci_resume(struct xhci_hcd *xhci, bool hibernated);
#else
#define	xhci_suspend	NULL
#define	xhci_resume	NULL
#endif

int xhci_get_frame(struct usb_hcd *hcd);
irqreturn_t xhci_irq(struct usb_hcd *hcd);
irqreturn_t xhci_msi_irq(int irq, struct usb_hcd *hcd);
int xhci_alloc_dev(struct usb_hcd *hcd, struct usb_device *udev);
void xhci_free_dev(struct usb_hcd *hcd, struct usb_device *udev);
int xhci_alloc_tt_info(struct xhci_hcd *xhci,
		struct xhci_virt_device *virt_dev,
		struct usb_device *hdev,
		struct usb_tt *tt, gfp_t mem_flags);
int xhci_alloc_streams(struct usb_hcd *hcd, struct usb_device *udev,
		struct usb_host_endpoint **eps, unsigned int num_eps,
		unsigned int num_streams, gfp_t mem_flags);
int xhci_free_streams(struct usb_hcd *hcd, struct usb_device *udev,
		struct usb_host_endpoint **eps, unsigned int num_eps,
		gfp_t mem_flags);
int xhci_address_device(struct usb_hcd *hcd, struct usb_device *udev);
int xhci_update_device(struct usb_hcd *hcd, struct usb_device *udev);
int xhci_set_usb2_hardware_lpm(struct usb_hcd *hcd,
				struct usb_device *udev, int enable);
int xhci_update_hub_device(struct usb_hcd *hcd, struct usb_device *hdev,
			struct usb_tt *tt, gfp_t mem_flags);
int xhci_urb_enqueue(struct usb_hcd *hcd, struct urb *urb, gfp_t mem_flags);
int xhci_urb_dequeue(struct usb_hcd *hcd, struct urb *urb, int status);
int xhci_add_endpoint(struct usb_hcd *hcd, struct usb_device *udev, struct usb_host_endpoint *ep);
int xhci_drop_endpoint(struct usb_hcd *hcd, struct usb_device *udev, struct usb_host_endpoint *ep);
void xhci_endpoint_reset(struct usb_hcd *hcd, struct usb_host_endpoint *ep);
int xhci_discover_or_reset_device(struct usb_hcd *hcd, struct usb_device *udev);
int xhci_check_bandwidth(struct usb_hcd *hcd, struct usb_device *udev);
void xhci_reset_bandwidth(struct usb_hcd *hcd, struct usb_device *udev);

dma_addr_t xhci_trb_virt_to_dma(struct xhci_segment *seg, union xhci_trb *trb);
struct xhci_segment *trb_in_td(struct xhci_segment *start_seg,
		union xhci_trb *start_trb, union xhci_trb *end_trb,
		dma_addr_t suspect_dma);
int xhci_is_vendor_info_code(struct xhci_hcd *xhci, unsigned int trb_comp_code);
void xhci_ring_cmd_db(struct xhci_hcd *xhci);
int xhci_queue_slot_control(struct xhci_hcd *xhci, u32 trb_type, u32 slot_id);
int xhci_queue_address_device(struct xhci_hcd *xhci, dma_addr_t in_ctx_ptr,
		u32 slot_id);
int xhci_queue_vendor_command(struct xhci_hcd *xhci,
		u32 field1, u32 field2, u32 field3, u32 field4);
int xhci_queue_stop_endpoint(struct xhci_hcd *xhci, int slot_id,
		unsigned int ep_index, int suspend);
int xhci_queue_ctrl_tx(struct xhci_hcd *xhci, gfp_t mem_flags, struct urb *urb,
		int slot_id, unsigned int ep_index);
int xhci_queue_bulk_tx(struct xhci_hcd *xhci, gfp_t mem_flags, struct urb *urb,
		int slot_id, unsigned int ep_index);
int xhci_queue_intr_tx(struct xhci_hcd *xhci, gfp_t mem_flags, struct urb *urb,
		int slot_id, unsigned int ep_index);
int xhci_queue_isoc_tx_prepare(struct xhci_hcd *xhci, gfp_t mem_flags,
		struct urb *urb, int slot_id, unsigned int ep_index);
int xhci_queue_configure_endpoint(struct xhci_hcd *xhci, dma_addr_t in_ctx_ptr,
		u32 slot_id, bool command_must_succeed);
int xhci_queue_evaluate_context(struct xhci_hcd *xhci, dma_addr_t in_ctx_ptr,
		u32 slot_id);
int xhci_queue_reset_ep(struct xhci_hcd *xhci, int slot_id,
		unsigned int ep_index);
int xhci_queue_reset_device(struct xhci_hcd *xhci, u32 slot_id);
void xhci_find_new_dequeue_state(struct xhci_hcd *xhci,
		unsigned int slot_id, unsigned int ep_index,
		unsigned int stream_id, struct xhci_td *cur_td,
		struct xhci_dequeue_state *state);
void xhci_queue_new_dequeue_state(struct xhci_hcd *xhci,
		unsigned int slot_id, unsigned int ep_index,
		unsigned int stream_id,
		struct xhci_dequeue_state *deq_state);
void xhci_cleanup_stalled_ring(struct xhci_hcd *xhci,
		struct usb_device *udev, unsigned int ep_index);
void xhci_queue_config_ep_quirk(struct xhci_hcd *xhci,
		unsigned int slot_id, unsigned int ep_index,
		struct xhci_dequeue_state *deq_state);
void xhci_stop_endpoint_command_watchdog(unsigned long arg);
void xhci_ring_ep_doorbell(struct xhci_hcd *xhci, unsigned int slot_id,
		unsigned int ep_index, unsigned int stream_id);

void xhci_set_link_state(struct xhci_hcd *xhci, __le32 __iomem **port_array,
				int port_id, u32 link_state);
void xhci_test_and_clear_bit(struct xhci_hcd *xhci, __le32 __iomem **port_array,
				int port_id, u32 port_bit);
int xhci_hub_control(struct usb_hcd *hcd, u16 typeReq, u16 wValue, u16 wIndex,
		char *buf, u16 wLength);
int xhci_hub_status_data(struct usb_hcd *hcd, char *buf);

#ifdef CONFIG_PM
int xhci_bus_suspend(struct usb_hcd *hcd);
int xhci_bus_resume(struct usb_hcd *hcd);
#else
#define	xhci_bus_suspend	NULL
#define	xhci_bus_resume		NULL
#endif	

u32 xhci_port_state_to_neutral(u32 state);
int xhci_find_slot_id_by_port(struct usb_hcd *hcd, struct xhci_hcd *xhci,
		u16 port);
void xhci_ring_device(struct xhci_hcd *xhci, int slot_id);

struct xhci_input_control_ctx *xhci_get_input_control_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx);
struct xhci_slot_ctx *xhci_get_slot_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx);
struct xhci_ep_ctx *xhci_get_ep_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx, unsigned int ep_index);

int xhci_submit_single_step_set_feature(struct usb_hcd *hcd, struct urb *urb,
					int is_setup);

#endif 
