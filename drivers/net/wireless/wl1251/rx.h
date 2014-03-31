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

#ifndef __WL1251_RX_H__
#define __WL1251_RX_H__

#include <linux/bitops.h>

#include "wl1251.h"


#define WL1251_RX_MAX_RSSI -30
#define WL1251_RX_MIN_RSSI -95

#define WL1251_RX_ALIGN_TO 4
#define WL1251_RX_ALIGN(len) (((len) + WL1251_RX_ALIGN_TO - 1) & \
			     ~(WL1251_RX_ALIGN_TO - 1))

#define SHORT_PREAMBLE_BIT   BIT(0)
#define OFDM_RATE_BIT        BIT(6)
#define PBCC_RATE_BIT        BIT(7)

#define PLCP_HEADER_LENGTH 8
#define RX_DESC_PACKETID_SHIFT 11
#define RX_MAX_PACKET_ID 3

#define RX_DESC_VALID_FCS         0x0001
#define RX_DESC_MATCH_RXADDR1     0x0002
#define RX_DESC_MCAST             0x0004
#define RX_DESC_STAINTIM          0x0008
#define RX_DESC_VIRTUAL_BM        0x0010
#define RX_DESC_BCAST             0x0020
#define RX_DESC_MATCH_SSID        0x0040
#define RX_DESC_MATCH_BSSID       0x0080
#define RX_DESC_ENCRYPTION_MASK   0x0300
#define RX_DESC_MEASURMENT        0x0400
#define RX_DESC_SEQNUM_MASK       0x1800
#define	RX_DESC_MIC_FAIL	  0x2000
#define	RX_DESC_DECRYPT_FAIL	  0x4000

struct wl1251_rx_descriptor {
	u32 timestamp; 
	u16 length; 
	u16 flags;

	u8 type;

	u8 rate;

	u8 mod_pre; 
	u8 channel;

	u8 band;

	s8 rssi; 
	u8 rcpi; 
	u8 snr; 
} __packed;

void wl1251_rx(struct wl1251 *wl);

#endif
