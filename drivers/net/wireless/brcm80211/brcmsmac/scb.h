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

#ifndef _BRCM_SCB_H_
#define _BRCM_SCB_H_

#include <linux/if_ether.h>
#include <brcmu_utils.h>
#include <defs.h>
#include "types.h"

#define AMPDU_TX_BA_MAX_WSIZE	64	

#define AMPDU_MAX_SCB_TID	NUMPRIO

#define SCB_WMECAP		0x0040
#define SCB_HTCAP		0x10000	
#define SCB_IS40		0x80000	
#define SCB_STBCCAP		0x40000000	

#define SCB_MAGIC	0xbeefcafe

struct scb_ampdu_tid_ini {
	u8 tx_in_transit; 
	u8 tid;		  
	
	u8 txretry[AMPDU_TX_BA_MAX_WSIZE];
	struct scb *scb;  
	u8 ba_wsize;	  
};

struct scb_ampdu {
	struct scb *scb;	
	u8 mpdu_density;	
	u8 max_pdu;		
	u8 release;		
	u16 min_len;		
	u32 max_rx_ampdu_bytes;	

	
	struct scb_ampdu_tid_ini ini[AMPDU_MAX_SCB_TID];
};

struct scb {
	u32 magic;
	u32 flags;	
	u32 flags2;	
	u8 state;	
	u8 ea[ETH_ALEN];	
	uint fragresid[NUMPRIO];

	u16 seqctl[NUMPRIO];	
	u16 seqctl_nonqos;
	u16 seqnum[NUMPRIO];

	struct scb_ampdu scb_ampdu;	
};

#endif				
