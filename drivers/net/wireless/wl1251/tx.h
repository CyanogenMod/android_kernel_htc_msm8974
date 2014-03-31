/*
 * This file is part of wl1251
 *
 * Copyright (c) 1998-2007 Texas Instruments Incorporated
 * Copyright (C) 2008 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __WL1251_TX_H__
#define __WL1251_TX_H__

#include <linux/bitops.h>
#include "acx.h"


#define TX_COMPLETE_REQUIRED_BIT	0x80
#define TX_STATUS_DATA_OUT_COUNT_MASK   0xf

#define WL1251_TX_ALIGN_TO 4
#define WL1251_TX_ALIGN(len) (((len) + WL1251_TX_ALIGN_TO - 1) & \
			     ~(WL1251_TX_ALIGN_TO - 1))
#define WL1251_TKIP_IV_SPACE 4

struct tx_control {
	
	unsigned rate_policy:3;

	
	unsigned ack_policy:1;

	unsigned packet_type:2;

	
	unsigned qos:1;

	unsigned tx_complete:1;

	
	unsigned xfer_pad:1;

	unsigned reserved:7;
} __packed;


struct tx_double_buffer_desc {
	
	__le16 length;

	__le16 rate;

	
	__le32 expiry_time;

	
	u8 xmit_queue;

	
	u8 id;

	struct tx_control control;

	__le16 frag_threshold;

	
	u8 num_mem_blocks;

	u8 reserved;
} __packed;

enum {
	TX_SUCCESS              = 0,
	TX_DMA_ERROR            = BIT(7),
	TX_DISABLED             = BIT(6),
	TX_RETRY_EXCEEDED       = BIT(5),
	TX_TIMEOUT              = BIT(4),
	TX_KEY_NOT_FOUND        = BIT(3),
	TX_ENCRYPT_FAIL         = BIT(2),
	TX_UNAVAILABLE_PRIORITY = BIT(1),
};

struct tx_result {
	u8 done_1;

	
	u8 id;

	u16 medium_usage;

	
	u32 medium_delay;

	
	u32 fw_hnadling_time;

	
	u8 lsb_seq_num;

	
	u8 ack_failures;

	
	u16 rate;

	u16 reserved;

	
	u8 status;

	
	u8 done_2;
} __packed;

static inline int wl1251_tx_get_queue(int queue)
{
	switch (queue) {
	case 0:
		return QOS_AC_VO;
	case 1:
		return QOS_AC_VI;
	case 2:
		return QOS_AC_BE;
	case 3:
		return QOS_AC_BK;
	default:
		return QOS_AC_BE;
	}
}

void wl1251_tx_work(struct work_struct *work);
void wl1251_tx_complete(struct wl1251 *wl);
void wl1251_tx_flush(struct wl1251 *wl);

#endif
