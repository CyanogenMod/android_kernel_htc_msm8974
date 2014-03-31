/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992-1997,2000-2006 Silicon Graphics, Inc. All Rights
 * Reserved.
 */
#ifndef _ASM_IA64_SN_XTALK_XBOW_H
#define _ASM_IA64_SN_XTALK_XBOW_H

#define XBOW_PORT_8	0x8
#define XBOW_PORT_C	0xc
#define XBOW_PORT_F	0xf

#define MAX_XBOW_PORTS	8	
#define BASE_XBOW_PORT	XBOW_PORT_8	

#define	XBOW_CREDIT	4

#define MAX_XBOW_NAME 	16

typedef volatile struct xb_linkregs_s {
	u32 link_ibf;
	u32 filler0;	
	u32 link_control;
	u32 filler1;
	u32 link_status;
	u32 filler2;
	u32 link_arb_upper;
	u32 filler3;
	u32 link_arb_lower;
	u32 filler4;
	u32 link_status_clr;
	u32 filler5;
	u32 link_reset;
	u32 filler6;
	u32 link_aux_status;
	u32 filler7;
} xb_linkregs_t;

typedef volatile struct xbow_s {
	
	struct widget_cfg xb_widget;  

	

#define xb_wid_id 		xb_widget.w_id
#define xb_wid_stat 		xb_widget.w_status
#define xb_wid_err_upper 	xb_widget.w_err_upper_addr
#define xb_wid_err_lower 	xb_widget.w_err_lower_addr
#define xb_wid_control		xb_widget.w_control
#define xb_wid_req_timeout 	xb_widget.w_req_timeout
#define xb_wid_int_upper 	xb_widget.w_intdest_upper_addr
#define xb_wid_int_lower 	xb_widget.w_intdest_lower_addr
#define xb_wid_err_cmdword 	xb_widget.w_err_cmd_word
#define xb_wid_llp 		xb_widget.w_llp_cfg
#define xb_wid_stat_clr 	xb_widget.w_tflush

	
	u32 xb_wid_arb_reload; 
	u32 _pad_000058;
	u32 xb_perf_ctr_a;	
	u32 _pad_000060;
	u32 xb_perf_ctr_b;	
	u32 _pad_000068;
	u32 xb_nic;		
	u32 _pad_000070;

	
	u32 xb_w0_rst_fnc;	
	u32 _pad_000078;
	u32 xb_l8_rst_fnc;	
	u32 _pad_000080;
	u32 xb_l9_rst_fnc;	
	u32 _pad_000088;
	u32 xb_la_rst_fnc;	
	u32 _pad_000090;
	u32 xb_lb_rst_fnc;	
	u32 _pad_000098;
	u32 xb_lc_rst_fnc;	
	u32 _pad_0000a0;
	u32 xb_ld_rst_fnc;	
	u32 _pad_0000a8;
	u32 xb_le_rst_fnc;	
	u32 _pad_0000b0;
	u32 xb_lf_rst_fnc;	
	u32 _pad_0000b8;
	u32 xb_lock;		
	u32 _pad_0000c0;
	u32 xb_lock_clr;	
	u32 _pad_0000c8;
	
	u32 _pad_0000d0[12];

	
	xb_linkregs_t xb_link_raw[MAX_XBOW_PORTS];
} xbow_t;

#define xb_link(p) xb_link_raw[(p) & (MAX_XBOW_PORTS - 1)]

#define XB_FLAGS_EXISTS		0x1	
#define XB_FLAGS_MASTER		0x2
#define XB_FLAGS_SLAVE		0x0
#define XB_FLAGS_GBR		0x4
#define XB_FLAGS_16BIT		0x8
#define XB_FLAGS_8BIT		0x0

#define XBOW_WIDGET_IS_VALID(wid) ((wid) >= XBOW_PORT_8 && (wid) <= XBOW_PORT_F)

#define XBOW_ARB_IS_UPPER(wid) 	((wid) >= XBOW_PORT_8 && (wid) <= XBOW_PORT_B)
#define XBOW_ARB_IS_LOWER(wid) 	((wid) >= XBOW_PORT_C && (wid) <= XBOW_PORT_F)

#define XBOW_ARB_OFF(wid) 	(XBOW_ARB_IS_UPPER(wid) ? 0x1c : 0x24)

#define	XBOW_WID_ID		WIDGET_ID
#define	XBOW_WID_STAT		WIDGET_STATUS
#define	XBOW_WID_ERR_UPPER	WIDGET_ERR_UPPER_ADDR
#define	XBOW_WID_ERR_LOWER	WIDGET_ERR_LOWER_ADDR
#define	XBOW_WID_CONTROL	WIDGET_CONTROL
#define	XBOW_WID_REQ_TO		WIDGET_REQ_TIMEOUT
#define	XBOW_WID_INT_UPPER	WIDGET_INTDEST_UPPER_ADDR
#define	XBOW_WID_INT_LOWER	WIDGET_INTDEST_LOWER_ADDR
#define	XBOW_WID_ERR_CMDWORD	WIDGET_ERR_CMD_WORD
#define	XBOW_WID_LLP		WIDGET_LLP_CFG
#define	XBOW_WID_STAT_CLR	WIDGET_TFLUSH
#define XBOW_WID_ARB_RELOAD 	0x5c
#define XBOW_WID_PERF_CTR_A 	0x64
#define XBOW_WID_PERF_CTR_B 	0x6c
#define XBOW_WID_NIC 		0x74

#define XBOW_W0_RST_FNC		0x00007C
#define	XBOW_L8_RST_FNC		0x000084
#define	XBOW_L9_RST_FNC		0x00008c
#define	XBOW_LA_RST_FNC		0x000094
#define	XBOW_LB_RST_FNC		0x00009c
#define	XBOW_LC_RST_FNC		0x0000a4
#define	XBOW_LD_RST_FNC		0x0000ac
#define	XBOW_LE_RST_FNC		0x0000b4
#define	XBOW_LF_RST_FNC		0x0000bc
#define XBOW_RESET_FENCE(x) ((x) > 7 && (x) < 16) ? \
				(XBOW_W0_RST_FNC + ((x) - 7) * 8) : \
				((x) == 0) ? XBOW_W0_RST_FNC : 0
#define XBOW_LOCK		0x0000c4
#define XBOW_LOCK_CLR		0x0000cc

#define	XBOW_WID_UNDEF		0xe4

#define	XB_LINK_BASE		0x100
#define	XB_LINK_OFFSET		0x40
#define	XB_LINK_REG_BASE(x)	(XB_LINK_BASE + ((x) & (MAX_XBOW_PORTS - 1)) * XB_LINK_OFFSET)

#define	XB_LINK_IBUF_FLUSH(x)	(XB_LINK_REG_BASE(x) + 0x4)
#define	XB_LINK_CTRL(x)		(XB_LINK_REG_BASE(x) + 0xc)
#define	XB_LINK_STATUS(x)	(XB_LINK_REG_BASE(x) + 0x14)
#define	XB_LINK_ARB_UPPER(x)	(XB_LINK_REG_BASE(x) + 0x1c)
#define	XB_LINK_ARB_LOWER(x)	(XB_LINK_REG_BASE(x) + 0x24)
#define	XB_LINK_STATUS_CLR(x)	(XB_LINK_REG_BASE(x) + 0x2c)
#define	XB_LINK_RESET(x)	(XB_LINK_REG_BASE(x) + 0x34)
#define	XB_LINK_AUX_STATUS(x)	(XB_LINK_REG_BASE(x) + 0x3c)

#define	XB_CTRL_LINKALIVE_IE		0x80000000	
#define	XB_CTRL_PERF_CTR_MODE_MSK	0x30000000	
#define	XB_CTRL_IBUF_LEVEL_MSK		0x0e000000	
#define	XB_CTRL_8BIT_MODE		0x01000000	
#define XB_CTRL_BAD_LLP_PKT		0x00800000	
#define XB_CTRL_WIDGET_CR_MSK		0x007c0000	
#define XB_CTRL_WIDGET_CR_SHFT	18			
#define XB_CTRL_ILLEGAL_DST_IE		0x00020000	
#define XB_CTRL_OALLOC_IBUF_IE		0x00010000	
#define XB_CTRL_BNDWDTH_ALLOC_IE	0x00000100	
#define XB_CTRL_RCV_CNT_OFLOW_IE	0x00000080	
#define XB_CTRL_XMT_CNT_OFLOW_IE	0x00000040	
#define XB_CTRL_XMT_MAX_RTRY_IE		0x00000020	
#define XB_CTRL_RCV_IE			0x00000010	
#define XB_CTRL_XMT_RTRY_IE		0x00000008	
#define	XB_CTRL_MAXREQ_TOUT_IE		0x00000002	
#define	XB_CTRL_SRC_TOUT_IE		0x00000001	

#define	XB_STAT_LINKALIVE		XB_CTRL_LINKALIVE_IE
#define	XB_STAT_MULTI_ERR		0x00040000	
#define	XB_STAT_ILLEGAL_DST_ERR		XB_CTRL_ILLEGAL_DST_IE
#define	XB_STAT_OALLOC_IBUF_ERR		XB_CTRL_OALLOC_IBUF_IE
#define	XB_STAT_BNDWDTH_ALLOC_ID_MSK	0x0000ff00	
#define	XB_STAT_RCV_CNT_OFLOW_ERR	XB_CTRL_RCV_CNT_OFLOW_IE
#define	XB_STAT_XMT_CNT_OFLOW_ERR	XB_CTRL_XMT_CNT_OFLOW_IE
#define	XB_STAT_XMT_MAX_RTRY_ERR	XB_CTRL_XMT_MAX_RTRY_IE
#define	XB_STAT_RCV_ERR			XB_CTRL_RCV_IE
#define	XB_STAT_XMT_RTRY_ERR		XB_CTRL_XMT_RTRY_IE
#define	XB_STAT_MAXREQ_TOUT_ERR		XB_CTRL_MAXREQ_TOUT_IE
#define	XB_STAT_SRC_TOUT_ERR		XB_CTRL_SRC_TOUT_IE

#define	XB_AUX_STAT_RCV_CNT	0xff000000
#define	XB_AUX_STAT_XMT_CNT	0x00ff0000
#define	XB_AUX_STAT_TOUT_DST	0x0000ff00
#define	XB_AUX_LINKFAIL_RST_BAD	0x00000040
#define	XB_AUX_STAT_PRESENT	0x00000020
#define	XB_AUX_STAT_PORT_WIDTH	0x00000010

#define	XB_ARB_GBR_MSK		0x1f
#define	XB_ARB_RR_MSK		0x7
#define	XB_ARB_GBR_SHFT(x)	(((x) & 0x3) * 8)
#define	XB_ARB_RR_SHFT(x)	(((x) & 0x3) * 8 + 5)
#define	XB_ARB_GBR_CNT(reg,x)	((reg) >> XB_ARB_GBR_SHFT(x) & XB_ARB_GBR_MSK)
#define	XB_ARB_RR_CNT(reg,x)	((reg) >> XB_ARB_RR_SHFT(x) & XB_ARB_RR_MSK)

#define	XB_WID_STAT_LINK_INTR_SHFT	(24)
#define	XB_WID_STAT_LINK_INTR_MASK	(0xFF << XB_WID_STAT_LINK_INTR_SHFT)
#define	XB_WID_STAT_LINK_INTR(x) \
	(0x1 << (((x)&7) + XB_WID_STAT_LINK_INTR_SHFT))
#define	XB_WID_STAT_WIDGET0_INTR	0x00800000
#define XB_WID_STAT_SRCID_MASK		0x000003c0	
#define	XB_WID_STAT_REG_ACC_ERR		0x00000020
#define XB_WID_STAT_RECV_TOUT		0x00000010	
#define XB_WID_STAT_ARB_TOUT		0x00000008	
#define	XB_WID_STAT_XTALK_ERR		0x00000004
#define XB_WID_STAT_DST_TOUT		0x00000002	
#define	XB_WID_STAT_MULTI_ERR		0x00000001

#define XB_WID_STAT_SRCID_SHFT		6

#define XB_WID_CTRL_REG_ACC_IE		XB_WID_STAT_REG_ACC_ERR
#define XB_WID_CTRL_RECV_TOUT		XB_WID_STAT_RECV_TOUT
#define XB_WID_CTRL_ARB_TOUT		XB_WID_STAT_ARB_TOUT
#define XB_WID_CTRL_XTALK_IE		XB_WID_STAT_XTALK_ERR


#define XBOW_WIDGET_PART_NUM	0x0		
#define XXBOW_WIDGET_PART_NUM	0xd000		
#define	XBOW_WIDGET_MFGR_NUM	0x0
#define	XXBOW_WIDGET_MFGR_NUM	0x0
#define PXBOW_WIDGET_PART_NUM   0xd100		

#define	XBOW_REV_1_0		0x1	
#define	XBOW_REV_1_1		0x2	
#define XBOW_REV_1_2		0x3	
#define XBOW_REV_1_3		0x4	
#define XBOW_REV_2_0		0x5	

#define XXBOW_PART_REV_1_0		(XXBOW_WIDGET_PART_NUM << 4 | 0x1 )
#define XXBOW_PART_REV_2_0		(XXBOW_WIDGET_PART_NUM << 4 | 0x2 )

#define	XBOW_WID_ARB_RELOAD_INT	0x3f	

#define IS_XBRIDGE_XBOW(wid) \
	(XWIDGET_PART_NUM(wid) == XXBOW_WIDGET_PART_NUM && \
	XWIDGET_MFG_NUM(wid) == XXBOW_WIDGET_MFGR_NUM)

#define IS_PIC_XBOW(wid) \
	(XWIDGET_PART_NUM(wid) == PXBOW_WIDGET_PART_NUM && \
	XWIDGET_MFG_NUM(wid) == XXBOW_WIDGET_MFGR_NUM)

#define XBOW_WAR_ENABLED(pv, widid) ((1 << XWIDGET_REV_NUM(widid)) & pv)

#endif 
