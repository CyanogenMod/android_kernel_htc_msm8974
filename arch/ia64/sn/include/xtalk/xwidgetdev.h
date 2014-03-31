/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992-1997,2000-2003 Silicon Graphics, Inc. All Rights Reserved.
 */
#ifndef _ASM_IA64_SN_XTALK_XWIDGET_H
#define _ASM_IA64_SN_XTALK_XWIDGET_H

#define WIDGET_REV_NUM                  0xf0000000
#define WIDGET_PART_NUM                 0x0ffff000
#define WIDGET_MFG_NUM                  0x00000ffe
#define WIDGET_REV_NUM_SHFT             28
#define WIDGET_PART_NUM_SHFT            12
#define WIDGET_MFG_NUM_SHFT             1

#define XWIDGET_PART_NUM(widgetid) (((widgetid) & WIDGET_PART_NUM) >> WIDGET_PART_NUM_SHFT)
#define XWIDGET_REV_NUM(widgetid) (((widgetid) & WIDGET_REV_NUM) >> WIDGET_REV_NUM_SHFT)
#define XWIDGET_MFG_NUM(widgetid) (((widgetid) & WIDGET_MFG_NUM) >> WIDGET_MFG_NUM_SHFT)
#define XWIDGET_PART_REV_NUM(widgetid) ((XWIDGET_PART_NUM(widgetid) << 4) | \
                                        XWIDGET_REV_NUM(widgetid))
#define XWIDGET_PART_REV_NUM_REV(partrev) (partrev & 0xf)

struct widget_cfg{
	u32	w_id;	
	u32	w_pad_0;	
	u32	w_status;	
	u32	w_pad_1;	
	u32	w_err_upper_addr;	
	u32	w_pad_2;	
	u32	w_err_lower_addr;	
	u32	w_pad_3;	
	u32	w_control;	
	u32	w_pad_4;	
	u32	w_req_timeout;	
	u32	w_pad_5;	
	u32	w_intdest_upper_addr;	
	u32	w_pad_6;	
	u32	w_intdest_lower_addr;	
	u32	w_pad_7;	
	u32	w_err_cmd_word;	
	u32	w_pad_8;	
	u32	w_llp_cfg;	
	u32	w_pad_9;	
	u32	w_tflush;	
	u32	w_pad_10;	
};

struct xwidget_hwid{
	int		mfg_num;
	int		rev_num;
	int		part_num;
};

struct xwidget_info{

	struct xwidget_hwid	xwi_hwid;	
	char			xwi_masterxid;	
	void			*xwi_hubinfo;     
	u64			*xwi_hub_provider; 
	void			*xwi_vertex;
};

#endif                          
