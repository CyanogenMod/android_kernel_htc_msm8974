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

#ifndef	_BRCM_DMA_H_
#define	_BRCM_DMA_H_

#include <linux/delay.h>
#include <linux/skbuff.h>
#include "types.h"		

#define	DMA_TX	1		
#define	DMA_RX	2		



struct dma32diag {	
	u32 fifoaddr;	
	u32 fifodatalow;	
	u32 fifodatahigh;	
	u32 pad;		
};


struct dma64regs {
	u32 control;	
	u32 ptr;	
	u32 addrlow;	
	u32 addrhigh;	
	u32 status0;	
	u32 status1;	
};

enum txd_range {
	DMA_RANGE_ALL = 1,
	DMA_RANGE_TRANSMITTED,
	DMA_RANGE_TRANSFERED
};

struct dma_pub {
	uint txavail;		
	uint dmactrlflags;	

	
	uint rxgiants;		
	uint rxnobuf;		
	
	uint txnobuf;		
};

extern struct dma_pub *dma_attach(char *name, struct si_pub *sih,
				  struct bcma_device *d11core,
				  uint txregbase, uint rxregbase,
				  uint ntxd, uint nrxd,
				  uint rxbufsize, int rxextheadroom,
				  uint nrxpost, uint rxoffset, uint *msg_level);

void dma_rxinit(struct dma_pub *pub);
int dma_rx(struct dma_pub *pub, struct sk_buff_head *skb_list);
bool dma_rxfill(struct dma_pub *pub);
bool dma_rxreset(struct dma_pub *pub);
bool dma_txreset(struct dma_pub *pub);
void dma_txinit(struct dma_pub *pub);
int dma_txfast(struct dma_pub *pub, struct sk_buff *p0, bool commit);
void dma_txsuspend(struct dma_pub *pub);
bool dma_txsuspended(struct dma_pub *pub);
void dma_txresume(struct dma_pub *pub);
void dma_txreclaim(struct dma_pub *pub, enum txd_range range);
void dma_rxreclaim(struct dma_pub *pub);
void dma_detach(struct dma_pub *pub);
unsigned long dma_getvar(struct dma_pub *pub, const char *name);
struct sk_buff *dma_getnexttxp(struct dma_pub *pub, enum txd_range range);
void dma_counterreset(struct dma_pub *pub);

void dma_walk_packets(struct dma_pub *dmah, void (*callback_fnc)
		      (void *pkt, void *arg_a), void *arg_a);

static inline void dma_spin_for_len(uint len, struct sk_buff *head)
{
#if defined(CONFIG_BCM47XX)
	if (!len) {
		while (!(len = *(u16 *) KSEG1ADDR(head->data)))
			udelay(1);

		*(u16 *) (head->data) = cpu_to_le16((u16) len);
	}
#endif				
}

#endif				
