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

#ifndef _BRCMF_BUS_H_
#define _BRCMF_BUS_H_

enum brcmf_bus_state {
	BRCMF_BUS_DOWN,		
	BRCMF_BUS_LOAD,		
	BRCMF_BUS_DATA		
};

struct dngl_stats {
	unsigned long rx_packets;	
	unsigned long tx_packets;	
	unsigned long rx_bytes;	
	unsigned long tx_bytes;	
	unsigned long rx_errors;	
	unsigned long tx_errors;	
	unsigned long rx_dropped;	
	unsigned long tx_dropped;	
	unsigned long multicast;	
};

struct brcmf_bus {
	u8 type;		
	union {
		struct brcmf_sdio_dev *sdio;
		struct brcmf_usbdev *usb;
	} bus_priv;
	struct brcmf_pub *drvr;	
	enum brcmf_bus_state state;
	uint maxctl;		
	bool drvr_up;		
	unsigned long tx_realloc;	
	struct dngl_stats dstats;	
	u8 align;		

	
	
	void (*brcmf_bus_stop)(struct device *);
	
	int (*brcmf_bus_init)(struct device *);
	
	int (*brcmf_bus_txdata)(struct device *, struct sk_buff *);
	int (*brcmf_bus_txctl)(struct device *, unsigned char *, uint);
	int (*brcmf_bus_rxctl)(struct device *, unsigned char *, uint);
};


extern int brcmf_proto_hdrpull(struct device *dev, int *ifidx,
			       struct sk_buff *rxp);

extern bool brcmf_c_prec_enq(struct device *dev, struct pktq *q,
			 struct sk_buff *pkt, int prec);

extern void brcmf_rx_frame(struct device *dev, int ifidx,
			   struct sk_buff_head *rxlist);
static inline void brcmf_rx_packet(struct device *dev, int ifidx,
				   struct sk_buff *pkt)
{
	struct sk_buff_head q;

	skb_queue_head_init(&q);
	skb_queue_tail(&q, pkt);
	brcmf_rx_frame(dev, ifidx, &q);
}

extern int brcmf_attach(uint bus_hdrlen, struct device *dev);
extern void brcmf_detach(struct device *dev);

extern void brcmf_txflowcontrol(struct device *dev, int ifidx, bool on);

extern void brcmf_txcomplete(struct device *dev, struct sk_buff *txp,
			     bool success);

extern int brcmf_bus_start(struct device *dev);

extern int brcmf_add_if(struct device *dev, int ifidx,
			char *name, u8 *mac_addr);

#ifdef CONFIG_BRCMFMAC_SDIO
extern void brcmf_sdio_exit(void);
extern void brcmf_sdio_init(void);
#endif
#ifdef CONFIG_BRCMFMAC_USB
extern void brcmf_usb_exit(void);
extern void brcmf_usb_init(void);
#endif

#endif				
