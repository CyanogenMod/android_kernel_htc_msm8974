/*
 * Copyright (c) 2010 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _BRCM_CHANNEL_H_
#define _BRCM_CHANNEL_H_

#define BRCMS_TXPWR_DB_FACTOR 4

#define BRCMS_PEAK_CONDUCTED	0x00	
#define BRCMS_EIRP		0x01	
#define BRCMS_DFS_TPC		0x02	
#define BRCMS_NO_OFDM		0x04	
#define BRCMS_NO_40MHZ		0x08	
#define BRCMS_NO_MIMO		0x10	
#define BRCMS_RADAR_TYPE_EU       0x20	
#define BRCMS_DFS_FCC             BRCMS_DFS_TPC	

#define BRCMS_DFS_EU (BRCMS_DFS_TPC | BRCMS_RADAR_TYPE_EU) 

extern struct brcms_cm_info *
brcms_c_channel_mgr_attach(struct brcms_c_info *wlc);

extern void brcms_c_channel_mgr_detach(struct brcms_cm_info *wlc_cm);

extern u8 brcms_c_channel_locale_flags_in_band(struct brcms_cm_info *wlc_cm,
					   uint bandunit);

extern bool brcms_c_valid_chanspec_db(struct brcms_cm_info *wlc_cm,
				      u16 chspec);

extern void brcms_c_channel_reg_limits(struct brcms_cm_info *wlc_cm,
				   u16 chanspec,
				   struct txpwr_limits *txpwr);
extern void brcms_c_channel_set_chanspec(struct brcms_cm_info *wlc_cm,
				     u16 chanspec,
				     u8 local_constraint_qdbm);

#endif				
