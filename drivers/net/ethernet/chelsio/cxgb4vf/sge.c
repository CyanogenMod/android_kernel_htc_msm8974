/*
 * This file is part of the Chelsio T4 PCI-E SR-IOV Virtual Function Ethernet
 * driver for Linux.
 *
 * Copyright (c) 2009-2010 Chelsio Communications, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <net/ipv6.h>
#include <net/tcp.h>
#include <linux/dma-mapping.h>
#include <linux/prefetch.h>

#include "t4vf_common.h"
#include "t4vf_defs.h"

#include "../cxgb4/t4_regs.h"
#include "../cxgb4/t4fw_api.h"
#include "../cxgb4/t4_msg.h"

static u32 FL_PG_ORDER;		
static u32 STAT_LEN;		
static u32 PKTSHIFT;		
static u32 FL_ALIGN;		

enum {
	EQ_UNIT = SGE_EQ_IDXSIZE,
	FL_PER_EQ_UNIT = EQ_UNIT / sizeof(__be64),
	TXD_PER_EQ_UNIT = EQ_UNIT / sizeof(__be64),

	MAX_TX_RECLAIM = 16,

	MAX_RX_REFILL = 16,

	RX_QCHECK_PERIOD = (HZ / 2),

	TX_QCHECK_PERIOD = (HZ / 2),
	MAX_TIMER_TX_RECLAIM = 100,

	FL_STARVE_THRES = 4,

	ETHTXQ_MAX_FRAGS = MAX_SKB_FRAGS + 1,
	ETHTXQ_MAX_SGL_LEN = ((3 * (ETHTXQ_MAX_FRAGS-1))/2 +
				   ((ETHTXQ_MAX_FRAGS-1) & 1) +
				   2),
	ETHTXQ_MAX_HDR = (sizeof(struct fw_eth_tx_pkt_vm_wr) +
			  sizeof(struct cpl_tx_pkt_lso_core) +
			  sizeof(struct cpl_tx_pkt_core)) / sizeof(__be64),
	ETHTXQ_MAX_FLITS = ETHTXQ_MAX_SGL_LEN + ETHTXQ_MAX_HDR,

	ETHTXQ_STOP_THRES = 1 + DIV_ROUND_UP(ETHTXQ_MAX_FLITS, TXD_PER_EQ_UNIT),

	MAX_IMM_TX_PKT_LEN = FW_WR_IMMDLEN_MASK,

	MAX_CTRL_WR_LEN = 256,

	MAX_IMM_TX_LEN = (MAX_IMM_TX_PKT_LEN > MAX_CTRL_WR_LEN
			  ? MAX_IMM_TX_PKT_LEN
			  : MAX_CTRL_WR_LEN),

	RX_COPY_THRES = 256,
	RX_PULL_LEN = 128,

	RX_SKB_LEN = 512,
};

struct tx_sw_desc {
	struct sk_buff *skb;		
	struct ulptx_sgl *sgl;		
};

struct rx_sw_desc {
	struct page *page;		
	dma_addr_t dma_addr;		
					
};

enum {
	RX_LARGE_BUF    = 1 << 0,	
	RX_UNMAPPED_BUF = 1 << 1,	
};

static inline dma_addr_t get_buf_addr(const struct rx_sw_desc *sdesc)
{
	return sdesc->dma_addr & ~(dma_addr_t)(RX_LARGE_BUF | RX_UNMAPPED_BUF);
}

static inline bool is_buf_mapped(const struct rx_sw_desc *sdesc)
{
	return !(sdesc->dma_addr & RX_UNMAPPED_BUF);
}

static inline int need_skb_unmap(void)
{
#ifdef CONFIG_NEED_DMA_MAP_STATE
	return 1;
#else
	return 0;
#endif
}

static inline unsigned int txq_avail(const struct sge_txq *tq)
{
	return tq->size - 1 - tq->in_use;
}

static inline unsigned int fl_cap(const struct sge_fl *fl)
{
	return fl->size - FL_PER_EQ_UNIT;
}

static inline bool fl_starving(const struct sge_fl *fl)
{
	return fl->avail - fl->pend_cred <= FL_STARVE_THRES;
}

static int map_skb(struct device *dev, const struct sk_buff *skb,
		   dma_addr_t *addr)
{
	const skb_frag_t *fp, *end;
	const struct skb_shared_info *si;

	*addr = dma_map_single(dev, skb->data, skb_headlen(skb), DMA_TO_DEVICE);
	if (dma_mapping_error(dev, *addr))
		goto out_err;

	si = skb_shinfo(skb);
	end = &si->frags[si->nr_frags];
	for (fp = si->frags; fp < end; fp++) {
		*++addr = skb_frag_dma_map(dev, fp, 0, skb_frag_size(fp),
					   DMA_TO_DEVICE);
		if (dma_mapping_error(dev, *addr))
			goto unwind;
	}
	return 0;

unwind:
	while (fp-- > si->frags)
		dma_unmap_page(dev, *--addr, skb_frag_size(fp), DMA_TO_DEVICE);
	dma_unmap_single(dev, addr[-1], skb_headlen(skb), DMA_TO_DEVICE);

out_err:
	return -ENOMEM;
}

static void unmap_sgl(struct device *dev, const struct sk_buff *skb,
		      const struct ulptx_sgl *sgl, const struct sge_txq *tq)
{
	const struct ulptx_sge_pair *p;
	unsigned int nfrags = skb_shinfo(skb)->nr_frags;

	if (likely(skb_headlen(skb)))
		dma_unmap_single(dev, be64_to_cpu(sgl->addr0),
				 be32_to_cpu(sgl->len0), DMA_TO_DEVICE);
	else {
		dma_unmap_page(dev, be64_to_cpu(sgl->addr0),
			       be32_to_cpu(sgl->len0), DMA_TO_DEVICE);
		nfrags--;
	}

	for (p = sgl->sge; nfrags >= 2; nfrags -= 2) {
		if (likely((u8 *)(p + 1) <= (u8 *)tq->stat)) {
unmap:
			dma_unmap_page(dev, be64_to_cpu(p->addr[0]),
				       be32_to_cpu(p->len[0]), DMA_TO_DEVICE);
			dma_unmap_page(dev, be64_to_cpu(p->addr[1]),
				       be32_to_cpu(p->len[1]), DMA_TO_DEVICE);
			p++;
		} else if ((u8 *)p == (u8 *)tq->stat) {
			p = (const struct ulptx_sge_pair *)tq->desc;
			goto unmap;
		} else if ((u8 *)p + 8 == (u8 *)tq->stat) {
			const __be64 *addr = (const __be64 *)tq->desc;

			dma_unmap_page(dev, be64_to_cpu(addr[0]),
				       be32_to_cpu(p->len[0]), DMA_TO_DEVICE);
			dma_unmap_page(dev, be64_to_cpu(addr[1]),
				       be32_to_cpu(p->len[1]), DMA_TO_DEVICE);
			p = (const struct ulptx_sge_pair *)&addr[2];
		} else {
			const __be64 *addr = (const __be64 *)tq->desc;

			dma_unmap_page(dev, be64_to_cpu(p->addr[0]),
				       be32_to_cpu(p->len[0]), DMA_TO_DEVICE);
			dma_unmap_page(dev, be64_to_cpu(addr[0]),
				       be32_to_cpu(p->len[1]), DMA_TO_DEVICE);
			p = (const struct ulptx_sge_pair *)&addr[1];
		}
	}
	if (nfrags) {
		__be64 addr;

		if ((u8 *)p == (u8 *)tq->stat)
			p = (const struct ulptx_sge_pair *)tq->desc;
		addr = ((u8 *)p + 16 <= (u8 *)tq->stat
			? p->addr[0]
			: *(const __be64 *)tq->desc);
		dma_unmap_page(dev, be64_to_cpu(addr), be32_to_cpu(p->len[0]),
			       DMA_TO_DEVICE);
	}
}

static void free_tx_desc(struct adapter *adapter, struct sge_txq *tq,
			 unsigned int n, bool unmap)
{
	struct tx_sw_desc *sdesc;
	unsigned int cidx = tq->cidx;
	struct device *dev = adapter->pdev_dev;

	const int need_unmap = need_skb_unmap() && unmap;

	sdesc = &tq->sdesc[cidx];
	while (n--) {
		if (sdesc->skb) {
			if (need_unmap)
				unmap_sgl(dev, sdesc->skb, sdesc->sgl, tq);
			kfree_skb(sdesc->skb);
			sdesc->skb = NULL;
		}

		sdesc++;
		if (++cidx == tq->size) {
			cidx = 0;
			sdesc = tq->sdesc;
		}
	}
	tq->cidx = cidx;
}

static inline int reclaimable(const struct sge_txq *tq)
{
	int hw_cidx = be16_to_cpu(tq->stat->cidx);
	int reclaimable = hw_cidx - tq->cidx;
	if (reclaimable < 0)
		reclaimable += tq->size;
	return reclaimable;
}

static inline void reclaim_completed_tx(struct adapter *adapter,
					struct sge_txq *tq,
					bool unmap)
{
	int avail = reclaimable(tq);

	if (avail) {
		if (avail > MAX_TX_RECLAIM)
			avail = MAX_TX_RECLAIM;

		free_tx_desc(adapter, tq, avail, unmap);
		tq->in_use -= avail;
	}
}

static inline int get_buf_size(const struct rx_sw_desc *sdesc)
{
	return FL_PG_ORDER > 0 && (sdesc->dma_addr & RX_LARGE_BUF)
		? (PAGE_SIZE << FL_PG_ORDER)
		: PAGE_SIZE;
}

static void free_rx_bufs(struct adapter *adapter, struct sge_fl *fl, int n)
{
	while (n--) {
		struct rx_sw_desc *sdesc = &fl->sdesc[fl->cidx];

		if (is_buf_mapped(sdesc))
			dma_unmap_page(adapter->pdev_dev, get_buf_addr(sdesc),
				       get_buf_size(sdesc), PCI_DMA_FROMDEVICE);
		put_page(sdesc->page);
		sdesc->page = NULL;
		if (++fl->cidx == fl->size)
			fl->cidx = 0;
		fl->avail--;
	}
}

static void unmap_rx_buf(struct adapter *adapter, struct sge_fl *fl)
{
	struct rx_sw_desc *sdesc = &fl->sdesc[fl->cidx];

	if (is_buf_mapped(sdesc))
		dma_unmap_page(adapter->pdev_dev, get_buf_addr(sdesc),
			       get_buf_size(sdesc), PCI_DMA_FROMDEVICE);
	sdesc->page = NULL;
	if (++fl->cidx == fl->size)
		fl->cidx = 0;
	fl->avail--;
}

static inline void ring_fl_db(struct adapter *adapter, struct sge_fl *fl)
{
	if (fl->pend_cred >= FL_PER_EQ_UNIT) {
		wmb();
		t4_write_reg(adapter, T4VF_SGE_BASE_ADDR + SGE_VF_KDOORBELL,
			     DBPRIO |
			     QID(fl->cntxt_id) |
			     PIDX(fl->pend_cred / FL_PER_EQ_UNIT));
		fl->pend_cred %= FL_PER_EQ_UNIT;
	}
}

static inline void set_rx_sw_desc(struct rx_sw_desc *sdesc, struct page *page,
				  dma_addr_t dma_addr)
{
	sdesc->page = page;
	sdesc->dma_addr = dma_addr;
}

#define POISON_BUF_VAL -1

static inline void poison_buf(struct page *page, size_t sz)
{
#if POISON_BUF_VAL >= 0
	memset(page_address(page), POISON_BUF_VAL, sz);
#endif
}

static unsigned int refill_fl(struct adapter *adapter, struct sge_fl *fl,
			      int n, gfp_t gfp)
{
	struct page *page;
	dma_addr_t dma_addr;
	unsigned int cred = fl->avail;
	__be64 *d = &fl->desc[fl->pidx];
	struct rx_sw_desc *sdesc = &fl->sdesc[fl->pidx];

	BUG_ON(fl->avail + n > fl->size - FL_PER_EQ_UNIT);

	if (FL_PG_ORDER == 0)
		goto alloc_small_pages;

	while (n) {
		page = alloc_pages(gfp | __GFP_COMP | __GFP_NOWARN,
				   FL_PG_ORDER);
		if (unlikely(!page)) {
			fl->large_alloc_failed++;
			break;
		}
		poison_buf(page, PAGE_SIZE << FL_PG_ORDER);

		dma_addr = dma_map_page(adapter->pdev_dev, page, 0,
					PAGE_SIZE << FL_PG_ORDER,
					PCI_DMA_FROMDEVICE);
		if (unlikely(dma_mapping_error(adapter->pdev_dev, dma_addr))) {
			__free_pages(page, FL_PG_ORDER);
			goto out;
		}
		dma_addr |= RX_LARGE_BUF;
		*d++ = cpu_to_be64(dma_addr);

		set_rx_sw_desc(sdesc, page, dma_addr);
		sdesc++;

		fl->avail++;
		if (++fl->pidx == fl->size) {
			fl->pidx = 0;
			sdesc = fl->sdesc;
			d = fl->desc;
		}
		n--;
	}

alloc_small_pages:
	while (n--) {
		page = alloc_page(gfp | __GFP_NOWARN | __GFP_COLD);
		if (unlikely(!page)) {
			fl->alloc_failed++;
			break;
		}
		poison_buf(page, PAGE_SIZE);

		dma_addr = dma_map_page(adapter->pdev_dev, page, 0, PAGE_SIZE,
				       PCI_DMA_FROMDEVICE);
		if (unlikely(dma_mapping_error(adapter->pdev_dev, dma_addr))) {
			put_page(page);
			break;
		}
		*d++ = cpu_to_be64(dma_addr);

		set_rx_sw_desc(sdesc, page, dma_addr);
		sdesc++;

		fl->avail++;
		if (++fl->pidx == fl->size) {
			fl->pidx = 0;
			sdesc = fl->sdesc;
			d = fl->desc;
		}
	}

out:
	cred = fl->avail - cred;
	fl->pend_cred += cred;
	ring_fl_db(adapter, fl);

	if (unlikely(fl_starving(fl))) {
		smp_wmb();
		set_bit(fl->cntxt_id, adapter->sge.starving_fl);
	}

	return cred;
}

static inline void __refill_fl(struct adapter *adapter, struct sge_fl *fl)
{
	refill_fl(adapter, fl,
		  min((unsigned int)MAX_RX_REFILL, fl_cap(fl) - fl->avail),
		  GFP_ATOMIC);
}

static void *alloc_ring(struct device *dev, size_t nelem, size_t hwsize,
			size_t swsize, dma_addr_t *busaddrp, void *swringp,
			size_t stat_size)
{
	size_t hwlen = nelem * hwsize + stat_size;
	void *hwring = dma_alloc_coherent(dev, hwlen, busaddrp, GFP_KERNEL);

	if (!hwring)
		return NULL;

	BUG_ON((swsize != 0) != (swringp != NULL));
	if (swsize) {
		void *swring = kcalloc(nelem, swsize, GFP_KERNEL);

		if (!swring) {
			dma_free_coherent(dev, hwlen, hwring, *busaddrp);
			return NULL;
		}
		*(void **)swringp = swring;
	}

	memset(hwring, 0, hwlen);
	return hwring;
}

static inline unsigned int sgl_len(unsigned int n)
{
	n--;
	return (3 * n) / 2 + (n & 1) + 2;
}

static inline unsigned int flits_to_desc(unsigned int flits)
{
	BUG_ON(flits > SGE_MAX_WR_LEN / sizeof(__be64));
	return DIV_ROUND_UP(flits, TXD_PER_EQ_UNIT);
}

static inline int is_eth_imm(const struct sk_buff *skb)
{
	return false;
}

static inline unsigned int calc_tx_flits(const struct sk_buff *skb)
{
	unsigned int flits;

	if (is_eth_imm(skb))
		return DIV_ROUND_UP(skb->len + sizeof(struct cpl_tx_pkt),
				    sizeof(__be64));

	flits = sgl_len(skb_shinfo(skb)->nr_frags + 1);
	if (skb_shinfo(skb)->gso_size)
		flits += (sizeof(struct fw_eth_tx_pkt_vm_wr) +
			  sizeof(struct cpl_tx_pkt_lso_core) +
			  sizeof(struct cpl_tx_pkt_core)) / sizeof(__be64);
	else
		flits += (sizeof(struct fw_eth_tx_pkt_vm_wr) +
			  sizeof(struct cpl_tx_pkt_core)) / sizeof(__be64);
	return flits;
}

/**
 *	write_sgl - populate a Scatter/Gather List for a packet
 *	@skb: the packet
 *	@tq: the TX queue we are writing into
 *	@sgl: starting location for writing the SGL
 *	@end: points right after the end of the SGL
 *	@start: start offset into skb main-body data to include in the SGL
 *	@addr: the list of DMA bus addresses for the SGL elements
 *
 *	Generates a Scatter/Gather List for the buffers that make up a packet.
 *	The caller must provide adequate space for the SGL that will be written.
 *	The SGL includes all of the packet's page fragments and the data in its
 *	main body except for the first @start bytes.  @pos must be 16-byte
 *	aligned and within a TX descriptor with available space.  @end points
 *	write after the end of the SGL but does not account for any potential
 *	wrap around, i.e., @end > @tq->stat.
 */
static void write_sgl(const struct sk_buff *skb, struct sge_txq *tq,
		      struct ulptx_sgl *sgl, u64 *end, unsigned int start,
		      const dma_addr_t *addr)
{
	unsigned int i, len;
	struct ulptx_sge_pair *to;
	const struct skb_shared_info *si = skb_shinfo(skb);
	unsigned int nfrags = si->nr_frags;
	struct ulptx_sge_pair buf[MAX_SKB_FRAGS / 2 + 1];

	len = skb_headlen(skb) - start;
	if (likely(len)) {
		sgl->len0 = htonl(len);
		sgl->addr0 = cpu_to_be64(addr[0] + start);
		nfrags++;
	} else {
		sgl->len0 = htonl(skb_frag_size(&si->frags[0]));
		sgl->addr0 = cpu_to_be64(addr[1]);
	}

	sgl->cmd_nsge = htonl(ULPTX_CMD(ULP_TX_SC_DSGL) |
			      ULPTX_NSGE(nfrags));
	if (likely(--nfrags == 0))
		return;
	to = (u8 *)end > (u8 *)tq->stat ? buf : sgl->sge;

	for (i = (nfrags != si->nr_frags); nfrags >= 2; nfrags -= 2, to++) {
		to->len[0] = cpu_to_be32(skb_frag_size(&si->frags[i]));
		to->len[1] = cpu_to_be32(skb_frag_size(&si->frags[++i]));
		to->addr[0] = cpu_to_be64(addr[i]);
		to->addr[1] = cpu_to_be64(addr[++i]);
	}
	if (nfrags) {
		to->len[0] = cpu_to_be32(skb_frag_size(&si->frags[i]));
		to->len[1] = cpu_to_be32(0);
		to->addr[0] = cpu_to_be64(addr[i + 1]);
	}
	if (unlikely((u8 *)end > (u8 *)tq->stat)) {
		unsigned int part0 = (u8 *)tq->stat - (u8 *)sgl->sge, part1;

		if (likely(part0))
			memcpy(sgl->sge, buf, part0);
		part1 = (u8 *)end - (u8 *)tq->stat;
		memcpy(tq->desc, (u8 *)buf + part0, part1);
		end = (void *)tq->desc + part1;
	}
	if ((uintptr_t)end & 8)           
		*(u64 *)end = 0;
}

static inline void ring_tx_db(struct adapter *adapter, struct sge_txq *tq,
			      int n)
{
	WARN_ON((QID(tq->cntxt_id) | PIDX(n)) & DBPRIO);
	wmb();
	t4_write_reg(adapter, T4VF_SGE_BASE_ADDR + SGE_VF_KDOORBELL,
		     QID(tq->cntxt_id) | PIDX(n));
}

static void inline_tx_skb(const struct sk_buff *skb, const struct sge_txq *tq,
			  void *pos)
{
	u64 *p;
	int left = (void *)tq->stat - pos;

	if (likely(skb->len <= left)) {
		if (likely(!skb->data_len))
			skb_copy_from_linear_data(skb, pos, skb->len);
		else
			skb_copy_bits(skb, 0, pos, skb->len);
		pos += skb->len;
	} else {
		skb_copy_bits(skb, 0, pos, left);
		skb_copy_bits(skb, left, tq->desc, skb->len - left);
		pos = (void *)tq->desc + (skb->len - left);
	}

	
	p = PTR_ALIGN(pos, 8);
	if ((uintptr_t)p & 8)
		*p = 0;
}

static u64 hwcsum(const struct sk_buff *skb)
{
	int csum_type;
	const struct iphdr *iph = ip_hdr(skb);

	if (iph->version == 4) {
		if (iph->protocol == IPPROTO_TCP)
			csum_type = TX_CSUM_TCPIP;
		else if (iph->protocol == IPPROTO_UDP)
			csum_type = TX_CSUM_UDPIP;
		else {
nocsum:
			return TXPKT_L4CSUM_DIS;
		}
	} else {
		const struct ipv6hdr *ip6h = (const struct ipv6hdr *)iph;

		if (ip6h->nexthdr == IPPROTO_TCP)
			csum_type = TX_CSUM_TCPIP6;
		else if (ip6h->nexthdr == IPPROTO_UDP)
			csum_type = TX_CSUM_UDPIP6;
		else
			goto nocsum;
	}

	if (likely(csum_type >= TX_CSUM_TCPIP))
		return TXPKT_CSUM_TYPE(csum_type) |
			TXPKT_IPHDR_LEN(skb_network_header_len(skb)) |
			TXPKT_ETHHDR_LEN(skb_network_offset(skb) - ETH_HLEN);
	else {
		int start = skb_transport_offset(skb);

		return TXPKT_CSUM_TYPE(csum_type) |
			TXPKT_CSUM_START(start) |
			TXPKT_CSUM_LOC(start + skb->csum_offset);
	}
}

static void txq_stop(struct sge_eth_txq *txq)
{
	netif_tx_stop_queue(txq->txq);
	txq->q.stops++;
}

static inline void txq_advance(struct sge_txq *tq, unsigned int n)
{
	tq->in_use += n;
	tq->pidx += n;
	if (tq->pidx >= tq->size)
		tq->pidx -= tq->size;
}

int t4vf_eth_xmit(struct sk_buff *skb, struct net_device *dev)
{
	u32 wr_mid;
	u64 cntrl, *end;
	int qidx, credits;
	unsigned int flits, ndesc;
	struct adapter *adapter;
	struct sge_eth_txq *txq;
	const struct port_info *pi;
	struct fw_eth_tx_pkt_vm_wr *wr;
	struct cpl_tx_pkt_core *cpl;
	const struct skb_shared_info *ssi;
	dma_addr_t addr[MAX_SKB_FRAGS + 1];
	const size_t fw_hdr_copy_len = (sizeof(wr->ethmacdst) +
					sizeof(wr->ethmacsrc) +
					sizeof(wr->ethtype) +
					sizeof(wr->vlantci));

	if (unlikely(skb->len < fw_hdr_copy_len))
		goto out_free;

	pi = netdev_priv(dev);
	adapter = pi->adapter;
	qidx = skb_get_queue_mapping(skb);
	BUG_ON(qidx >= pi->nqsets);
	txq = &adapter->sge.ethtxq[pi->first_qset + qidx];

	reclaim_completed_tx(adapter, &txq->q, true);

	flits = calc_tx_flits(skb);
	ndesc = flits_to_desc(flits);
	credits = txq_avail(&txq->q) - ndesc;

	if (unlikely(credits < 0)) {
		txq_stop(txq);
		dev_err(adapter->pdev_dev,
			"%s: TX ring %u full while queue awake!\n",
			dev->name, qidx);
		return NETDEV_TX_BUSY;
	}

	if (!is_eth_imm(skb) &&
	    unlikely(map_skb(adapter->pdev_dev, skb, addr) < 0)) {
		txq->mapping_err++;
		goto out_free;
	}

	wr_mid = FW_WR_LEN16(DIV_ROUND_UP(flits, 2));
	if (unlikely(credits < ETHTXQ_STOP_THRES)) {
		txq_stop(txq);
		wr_mid |= FW_WR_EQUEQ | FW_WR_EQUIQ;
	}

	BUG_ON(DIV_ROUND_UP(ETHTXQ_MAX_HDR, TXD_PER_EQ_UNIT) > 1);
	wr = (void *)&txq->q.desc[txq->q.pidx];
	wr->equiq_to_len16 = cpu_to_be32(wr_mid);
	wr->r3[0] = cpu_to_be64(0);
	wr->r3[1] = cpu_to_be64(0);
	skb_copy_from_linear_data(skb, (void *)wr->ethmacdst, fw_hdr_copy_len);
	end = (u64 *)wr + flits;

	ssi = skb_shinfo(skb);
	if (ssi->gso_size) {
		struct cpl_tx_pkt_lso_core *lso = (void *)(wr + 1);
		bool v6 = (ssi->gso_type & SKB_GSO_TCPV6) != 0;
		int l3hdr_len = skb_network_header_len(skb);
		int eth_xtra_len = skb_network_offset(skb) - ETH_HLEN;

		wr->op_immdlen =
			cpu_to_be32(FW_WR_OP(FW_ETH_TX_PKT_VM_WR) |
				    FW_WR_IMMDLEN(sizeof(*lso) +
						  sizeof(*cpl)));
		lso->lso_ctrl =
			cpu_to_be32(LSO_OPCODE(CPL_TX_PKT_LSO) |
				    LSO_FIRST_SLICE |
				    LSO_LAST_SLICE |
				    LSO_IPV6(v6) |
				    LSO_ETHHDR_LEN(eth_xtra_len/4) |
				    LSO_IPHDR_LEN(l3hdr_len/4) |
				    LSO_TCPHDR_LEN(tcp_hdr(skb)->doff));
		lso->ipid_ofst = cpu_to_be16(0);
		lso->mss = cpu_to_be16(ssi->gso_size);
		lso->seqno_offset = cpu_to_be32(0);
		lso->len = cpu_to_be32(skb->len);

		cpl = (void *)(lso + 1);
		cntrl = (TXPKT_CSUM_TYPE(v6 ? TX_CSUM_TCPIP6 : TX_CSUM_TCPIP) |
			 TXPKT_IPHDR_LEN(l3hdr_len) |
			 TXPKT_ETHHDR_LEN(eth_xtra_len));
		txq->tso++;
		txq->tx_cso += ssi->gso_segs;
	} else {
		int len;

		len = is_eth_imm(skb) ? skb->len + sizeof(*cpl) : sizeof(*cpl);
		wr->op_immdlen =
			cpu_to_be32(FW_WR_OP(FW_ETH_TX_PKT_VM_WR) |
				    FW_WR_IMMDLEN(len));

		cpl = (void *)(wr + 1);
		if (skb->ip_summed == CHECKSUM_PARTIAL) {
			cntrl = hwcsum(skb) | TXPKT_IPCSUM_DIS;
			txq->tx_cso++;
		} else
			cntrl = TXPKT_L4CSUM_DIS | TXPKT_IPCSUM_DIS;
	}

	if (vlan_tx_tag_present(skb)) {
		txq->vlan_ins++;
		cntrl |= TXPKT_VLAN_VLD | TXPKT_VLAN(vlan_tx_tag_get(skb));
	}

	cpl->ctrl0 = cpu_to_be32(TXPKT_OPCODE(CPL_TX_PKT_XT) |
				 TXPKT_INTF(pi->port_id) |
				 TXPKT_PF(0));
	cpl->pack = cpu_to_be16(0);
	cpl->len = cpu_to_be16(skb->len);
	cpl->ctrl1 = cpu_to_be64(cntrl);

#ifdef T4_TRACE
	T4_TRACE5(adapter->tb[txq->q.cntxt_id & 7],
		  "eth_xmit: ndesc %u, credits %u, pidx %u, len %u, frags %u",
		  ndesc, credits, txq->q.pidx, skb->len, ssi->nr_frags);
#endif

	if (is_eth_imm(skb)) {
		inline_tx_skb(skb, &txq->q, cpl + 1);
		dev_kfree_skb(skb);
	} else {
		struct ulptx_sgl *sgl = (struct ulptx_sgl *)(cpl + 1);
		struct sge_txq *tq = &txq->q;
		int last_desc;

		if (unlikely((void *)sgl == (void *)tq->stat)) {
			sgl = (void *)tq->desc;
			end = (void *)((void *)tq->desc +
				       ((void *)end - (void *)tq->stat));
		}

		write_sgl(skb, tq, sgl, end, 0, addr);
		skb_orphan(skb);

		last_desc = tq->pidx + ndesc - 1;
		if (last_desc >= tq->size)
			last_desc -= tq->size;
		tq->sdesc[last_desc].skb = skb;
		tq->sdesc[last_desc].sgl = sgl;
	}

	txq_advance(&txq->q, ndesc);
	dev->trans_start = jiffies;
	ring_tx_db(adapter, &txq->q, ndesc);
	return NETDEV_TX_OK;

out_free:
	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}

static inline void copy_frags(struct sk_buff *skb,
			      const struct pkt_gl *gl,
			      unsigned int offset)
{
	int i;

	
	__skb_fill_page_desc(skb, 0, gl->frags[0].page,
			     gl->frags[0].offset + offset,
			     gl->frags[0].size - offset);
	skb_shinfo(skb)->nr_frags = gl->nfrags;
	for (i = 1; i < gl->nfrags; i++)
		__skb_fill_page_desc(skb, i, gl->frags[i].page,
				     gl->frags[i].offset,
				     gl->frags[i].size);

	
	get_page(gl->frags[gl->nfrags - 1].page);
}

struct sk_buff *t4vf_pktgl_to_skb(const struct pkt_gl *gl,
				  unsigned int skb_len, unsigned int pull_len)
{
	struct sk_buff *skb;

	if (gl->tot_len <= RX_COPY_THRES) {
		
		skb = alloc_skb(gl->tot_len, GFP_ATOMIC);
		if (unlikely(!skb))
			goto out;
		__skb_put(skb, gl->tot_len);
		skb_copy_to_linear_data(skb, gl->va, gl->tot_len);
	} else {
		skb = alloc_skb(skb_len, GFP_ATOMIC);
		if (unlikely(!skb))
			goto out;
		__skb_put(skb, pull_len);
		skb_copy_to_linear_data(skb, gl->va, pull_len);

		copy_frags(skb, gl, pull_len);
		skb->len = gl->tot_len;
		skb->data_len = skb->len - pull_len;
		skb->truesize += skb->data_len;
	}

out:
	return skb;
}

void t4vf_pktgl_free(const struct pkt_gl *gl)
{
	int frag;

	frag = gl->nfrags - 1;
	while (frag--)
		put_page(gl->frags[frag].page);
}

static void do_gro(struct sge_eth_rxq *rxq, const struct pkt_gl *gl,
		   const struct cpl_rx_pkt *pkt)
{
	int ret;
	struct sk_buff *skb;

	skb = napi_get_frags(&rxq->rspq.napi);
	if (unlikely(!skb)) {
		t4vf_pktgl_free(gl);
		rxq->stats.rx_drops++;
		return;
	}

	copy_frags(skb, gl, PKTSHIFT);
	skb->len = gl->tot_len - PKTSHIFT;
	skb->data_len = skb->len;
	skb->truesize += skb->data_len;
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	skb_record_rx_queue(skb, rxq->rspq.idx);

	if (pkt->vlan_ex)
		__vlan_hwaccel_put_tag(skb, be16_to_cpu(pkt->vlan));
	ret = napi_gro_frags(&rxq->rspq.napi);

	if (ret == GRO_HELD)
		rxq->stats.lro_pkts++;
	else if (ret == GRO_MERGED || ret == GRO_MERGED_FREE)
		rxq->stats.lro_merged++;
	rxq->stats.pkts++;
	rxq->stats.rx_cso++;
}

int t4vf_ethrx_handler(struct sge_rspq *rspq, const __be64 *rsp,
		       const struct pkt_gl *gl)
{
	struct sk_buff *skb;
	const struct cpl_rx_pkt *pkt = (void *)&rsp[1];
	bool csum_ok = pkt->csum_calc && !pkt->err_vec;
	struct sge_eth_rxq *rxq = container_of(rspq, struct sge_eth_rxq, rspq);

	if ((pkt->l2info & cpu_to_be32(RXF_TCP)) &&
	    (rspq->netdev->features & NETIF_F_GRO) && csum_ok &&
	    !pkt->ip_frag) {
		do_gro(rxq, gl, pkt);
		return 0;
	}

	skb = t4vf_pktgl_to_skb(gl, RX_SKB_LEN, RX_PULL_LEN);
	if (unlikely(!skb)) {
		t4vf_pktgl_free(gl);
		rxq->stats.rx_drops++;
		return 0;
	}
	__skb_pull(skb, PKTSHIFT);
	skb->protocol = eth_type_trans(skb, rspq->netdev);
	skb_record_rx_queue(skb, rspq->idx);
	rxq->stats.pkts++;

	if (csum_ok && (rspq->netdev->features & NETIF_F_RXCSUM) &&
	    !pkt->err_vec && (be32_to_cpu(pkt->l2info) & (RXF_UDP|RXF_TCP))) {
		if (!pkt->ip_frag)
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		else {
			__sum16 c = (__force __sum16)pkt->csum;
			skb->csum = csum_unfold(c);
			skb->ip_summed = CHECKSUM_COMPLETE;
		}
		rxq->stats.rx_cso++;
	} else
		skb_checksum_none_assert(skb);

	if (pkt->vlan_ex) {
		rxq->stats.vlan_ex++;
		__vlan_hwaccel_put_tag(skb, be16_to_cpu(pkt->vlan));
	}

	netif_receive_skb(skb);

	return 0;
}

/**
 *	is_new_response - check if a response is newly written
 *	@rc: the response control descriptor
 *	@rspq: the response queue
 *
 *	Returns true if a response descriptor contains a yet unprocessed
 *	response.
 */
static inline bool is_new_response(const struct rsp_ctrl *rc,
				   const struct sge_rspq *rspq)
{
	return RSPD_GEN(rc->type_gen) == rspq->gen;
}

static void restore_rx_bufs(const struct pkt_gl *gl, struct sge_fl *fl,
			    int frags)
{
	struct rx_sw_desc *sdesc;

	while (frags--) {
		if (fl->cidx == 0)
			fl->cidx = fl->size - 1;
		else
			fl->cidx--;
		sdesc = &fl->sdesc[fl->cidx];
		sdesc->page = gl->frags[frags].page;
		sdesc->dma_addr |= RX_UNMAPPED_BUF;
		fl->avail++;
	}
}

static inline void rspq_next(struct sge_rspq *rspq)
{
	rspq->cur_desc = (void *)rspq->cur_desc + rspq->iqe_len;
	if (unlikely(++rspq->cidx == rspq->size)) {
		rspq->cidx = 0;
		rspq->gen ^= 1;
		rspq->cur_desc = rspq->desc;
	}
}

int process_responses(struct sge_rspq *rspq, int budget)
{
	struct sge_eth_rxq *rxq = container_of(rspq, struct sge_eth_rxq, rspq);
	int budget_left = budget;

	while (likely(budget_left)) {
		int ret, rsp_type;
		const struct rsp_ctrl *rc;

		rc = (void *)rspq->cur_desc + (rspq->iqe_len - sizeof(*rc));
		if (!is_new_response(rc, rspq))
			break;

		rmb();
		rsp_type = RSPD_TYPE(rc->type_gen);
		if (likely(rsp_type == RSP_TYPE_FLBUF)) {
			struct page_frag *fp;
			struct pkt_gl gl;
			const struct rx_sw_desc *sdesc;
			u32 bufsz, frag;
			u32 len = be32_to_cpu(rc->pldbuflen_qid);

			if (len & RSPD_NEWBUF) {
				if (likely(rspq->offset > 0)) {
					free_rx_bufs(rspq->adapter, &rxq->fl,
						     1);
					rspq->offset = 0;
				}
				len = RSPD_LEN(len);
			}
			gl.tot_len = len;

			for (frag = 0, fp = gl.frags; ; frag++, fp++) {
				BUG_ON(frag >= MAX_SKB_FRAGS);
				BUG_ON(rxq->fl.avail == 0);
				sdesc = &rxq->fl.sdesc[rxq->fl.cidx];
				bufsz = get_buf_size(sdesc);
				fp->page = sdesc->page;
				fp->offset = rspq->offset;
				fp->size = min(bufsz, len);
				len -= fp->size;
				if (!len)
					break;
				unmap_rx_buf(rspq->adapter, &rxq->fl);
			}
			gl.nfrags = frag+1;

			dma_sync_single_for_cpu(rspq->adapter->pdev_dev,
						get_buf_addr(sdesc),
						fp->size, DMA_FROM_DEVICE);
			gl.va = (page_address(gl.frags[0].page) +
				 gl.frags[0].offset);
			prefetch(gl.va);

			ret = rspq->handler(rspq, rspq->cur_desc, &gl);
			if (likely(ret == 0))
				rspq->offset += ALIGN(fp->size, FL_ALIGN);
			else
				restore_rx_bufs(&gl, &rxq->fl, frag);
		} else if (likely(rsp_type == RSP_TYPE_CPL)) {
			ret = rspq->handler(rspq, rspq->cur_desc, NULL);
		} else {
			WARN_ON(rsp_type > RSP_TYPE_CPL);
			ret = 0;
		}

		if (unlikely(ret)) {
			const int NOMEM_TIMER_IDX = SGE_NTIMERS-1;
			rspq->next_intr_params =
				QINTR_TIMER_IDX(NOMEM_TIMER_IDX);
			break;
		}

		rspq_next(rspq);
		budget_left--;
	}

	if (rspq->offset >= 0 &&
	    rxq->fl.size - rxq->fl.avail >= 2*FL_PER_EQ_UNIT)
		__refill_fl(rspq->adapter, &rxq->fl);
	return budget - budget_left;
}

static int napi_rx_handler(struct napi_struct *napi, int budget)
{
	unsigned int intr_params;
	struct sge_rspq *rspq = container_of(napi, struct sge_rspq, napi);
	int work_done = process_responses(rspq, budget);

	if (likely(work_done < budget)) {
		napi_complete(napi);
		intr_params = rspq->next_intr_params;
		rspq->next_intr_params = rspq->intr_params;
	} else
		intr_params = QINTR_TIMER_IDX(SGE_TIMER_UPD_CIDX);

	if (unlikely(work_done == 0))
		rspq->unhandled_irqs++;

	t4_write_reg(rspq->adapter,
		     T4VF_SGE_BASE_ADDR + SGE_VF_GTS,
		     CIDXINC(work_done) |
		     INGRESSQID((u32)rspq->cntxt_id) |
		     SEINTARM(intr_params));
	return work_done;
}

irqreturn_t t4vf_sge_intr_msix(int irq, void *cookie)
{
	struct sge_rspq *rspq = cookie;

	napi_schedule(&rspq->napi);
	return IRQ_HANDLED;
}

static unsigned int process_intrq(struct adapter *adapter)
{
	struct sge *s = &adapter->sge;
	struct sge_rspq *intrq = &s->intrq;
	unsigned int work_done;

	spin_lock(&adapter->sge.intrq_lock);
	for (work_done = 0; ; work_done++) {
		const struct rsp_ctrl *rc;
		unsigned int qid, iq_idx;
		struct sge_rspq *rspq;

		rc = (void *)intrq->cur_desc + (intrq->iqe_len - sizeof(*rc));
		if (!is_new_response(rc, intrq))
			break;

		rmb();
		if (unlikely(RSPD_TYPE(rc->type_gen) != RSP_TYPE_INTR)) {
			dev_err(adapter->pdev_dev,
				"Unexpected INTRQ response type %d\n",
				RSPD_TYPE(rc->type_gen));
			continue;
		}

		qid = RSPD_QID(be32_to_cpu(rc->pldbuflen_qid));
		iq_idx = IQ_IDX(s, qid);
		if (unlikely(iq_idx >= MAX_INGQ)) {
			dev_err(adapter->pdev_dev,
				"Ingress QID %d out of range\n", qid);
			continue;
		}
		rspq = s->ingr_map[iq_idx];
		if (unlikely(rspq == NULL)) {
			dev_err(adapter->pdev_dev,
				"Ingress QID %d RSPQ=NULL\n", qid);
			continue;
		}
		if (unlikely(rspq->abs_id != qid)) {
			dev_err(adapter->pdev_dev,
				"Ingress QID %d refers to RSPQ %d\n",
				qid, rspq->abs_id);
			continue;
		}

		napi_schedule(&rspq->napi);
		rspq_next(intrq);
	}

	t4_write_reg(adapter, T4VF_SGE_BASE_ADDR + SGE_VF_GTS,
		     CIDXINC(work_done) |
		     INGRESSQID(intrq->cntxt_id) |
		     SEINTARM(intrq->intr_params));

	spin_unlock(&adapter->sge.intrq_lock);

	return work_done;
}

irqreturn_t t4vf_intr_msi(int irq, void *cookie)
{
	struct adapter *adapter = cookie;

	process_intrq(adapter);
	return IRQ_HANDLED;
}

irq_handler_t t4vf_intr_handler(struct adapter *adapter)
{
	BUG_ON((adapter->flags & (USING_MSIX|USING_MSI)) == 0);
	if (adapter->flags & USING_MSIX)
		return t4vf_sge_intr_msix;
	else
		return t4vf_intr_msi;
}

static void sge_rx_timer_cb(unsigned long data)
{
	struct adapter *adapter = (struct adapter *)data;
	struct sge *s = &adapter->sge;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(s->starving_fl); i++) {
		unsigned long m;

		for (m = s->starving_fl[i]; m; m &= m - 1) {
			unsigned int id = __ffs(m) + i * BITS_PER_LONG;
			struct sge_fl *fl = s->egr_map[id];

			clear_bit(id, s->starving_fl);
			smp_mb__after_clear_bit();

			if (fl_starving(fl)) {
				struct sge_eth_rxq *rxq;

				rxq = container_of(fl, struct sge_eth_rxq, fl);
				if (napi_reschedule(&rxq->rspq.napi))
					fl->starving++;
				else
					set_bit(id, s->starving_fl);
			}
		}
	}

	mod_timer(&s->rx_timer, jiffies + RX_QCHECK_PERIOD);
}

static void sge_tx_timer_cb(unsigned long data)
{
	struct adapter *adapter = (struct adapter *)data;
	struct sge *s = &adapter->sge;
	unsigned int i, budget;

	budget = MAX_TIMER_TX_RECLAIM;
	i = s->ethtxq_rover;
	do {
		struct sge_eth_txq *txq = &s->ethtxq[i];

		if (reclaimable(&txq->q) && __netif_tx_trylock(txq->txq)) {
			int avail = reclaimable(&txq->q);

			if (avail > budget)
				avail = budget;

			free_tx_desc(adapter, &txq->q, avail, true);
			txq->q.in_use -= avail;
			__netif_tx_unlock(txq->txq);

			budget -= avail;
			if (!budget)
				break;
		}

		i++;
		if (i >= s->ethqsets)
			i = 0;
	} while (i != s->ethtxq_rover);
	s->ethtxq_rover = i;

	mod_timer(&s->tx_timer, jiffies + (budget ? TX_QCHECK_PERIOD : 2));
}

int t4vf_sge_alloc_rxq(struct adapter *adapter, struct sge_rspq *rspq,
		       bool iqasynch, struct net_device *dev,
		       int intr_dest,
		       struct sge_fl *fl, rspq_handler_t hnd)
{
	struct port_info *pi = netdev_priv(dev);
	struct fw_iq_cmd cmd, rpl;
	int ret, iqandst, flsz = 0;

	if ((adapter->flags & USING_MSI) && rspq != &adapter->sge.intrq) {
		iqandst = SGE_INTRDST_IQ;
		intr_dest = adapter->sge.intrq.abs_id;
	} else
		iqandst = SGE_INTRDST_PCI;

	rspq->size = roundup(rspq->size, 16);
	rspq->desc = alloc_ring(adapter->pdev_dev, rspq->size, rspq->iqe_len,
				0, &rspq->phys_addr, NULL, 0);
	if (!rspq->desc)
		return -ENOMEM;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_vfn = cpu_to_be32(FW_CMD_OP(FW_IQ_CMD) |
				    FW_CMD_REQUEST |
				    FW_CMD_WRITE |
				    FW_CMD_EXEC);
	cmd.alloc_to_len16 = cpu_to_be32(FW_IQ_CMD_ALLOC |
					 FW_IQ_CMD_IQSTART(1) |
					 FW_LEN16(cmd));
	cmd.type_to_iqandstindex =
		cpu_to_be32(FW_IQ_CMD_TYPE(FW_IQ_TYPE_FL_INT_CAP) |
			    FW_IQ_CMD_IQASYNCH(iqasynch) |
			    FW_IQ_CMD_VIID(pi->viid) |
			    FW_IQ_CMD_IQANDST(iqandst) |
			    FW_IQ_CMD_IQANUS(1) |
			    FW_IQ_CMD_IQANUD(SGE_UPDATEDEL_INTR) |
			    FW_IQ_CMD_IQANDSTINDEX(intr_dest));
	cmd.iqdroprss_to_iqesize =
		cpu_to_be16(FW_IQ_CMD_IQPCIECH(pi->port_id) |
			    FW_IQ_CMD_IQGTSMODE |
			    FW_IQ_CMD_IQINTCNTTHRESH(rspq->pktcnt_idx) |
			    FW_IQ_CMD_IQESIZE(ilog2(rspq->iqe_len) - 4));
	cmd.iqsize = cpu_to_be16(rspq->size);
	cmd.iqaddr = cpu_to_be64(rspq->phys_addr);

	if (fl) {
		fl->size = roundup(fl->size, FL_PER_EQ_UNIT);
		fl->desc = alloc_ring(adapter->pdev_dev, fl->size,
				      sizeof(__be64), sizeof(struct rx_sw_desc),
				      &fl->addr, &fl->sdesc, STAT_LEN);
		if (!fl->desc) {
			ret = -ENOMEM;
			goto err;
		}

		flsz = (fl->size / FL_PER_EQ_UNIT +
			STAT_LEN / EQ_UNIT);

		cmd.iqns_to_fl0congen =
			cpu_to_be32(
				FW_IQ_CMD_FL0HOSTFCMODE(SGE_HOSTFCMODE_NONE) |
				FW_IQ_CMD_FL0PACKEN |
				FW_IQ_CMD_FL0PADEN);
		cmd.fl0dcaen_to_fl0cidxfthresh =
			cpu_to_be16(
				FW_IQ_CMD_FL0FBMIN(SGE_FETCHBURSTMIN_64B) |
				FW_IQ_CMD_FL0FBMAX(SGE_FETCHBURSTMAX_512B));
		cmd.fl0size = cpu_to_be16(flsz);
		cmd.fl0addr = cpu_to_be64(fl->addr);
	}

	ret = t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), &rpl);
	if (ret)
		goto err;

	netif_napi_add(dev, &rspq->napi, napi_rx_handler, 64);
	rspq->cur_desc = rspq->desc;
	rspq->cidx = 0;
	rspq->gen = 1;
	rspq->next_intr_params = rspq->intr_params;
	rspq->cntxt_id = be16_to_cpu(rpl.iqid);
	rspq->abs_id = be16_to_cpu(rpl.physiqid);
	rspq->size--;			
	rspq->adapter = adapter;
	rspq->netdev = dev;
	rspq->handler = hnd;

	
	rspq->offset = fl ? 0 : -1;

	if (fl) {
		fl->cntxt_id = be16_to_cpu(rpl.fl0id);
		fl->avail = 0;
		fl->pend_cred = 0;
		fl->pidx = 0;
		fl->cidx = 0;
		fl->alloc_failed = 0;
		fl->large_alloc_failed = 0;
		fl->starving = 0;
		refill_fl(adapter, fl, fl_cap(fl), GFP_KERNEL);
	}

	return 0;

err:
	if (rspq->desc) {
		dma_free_coherent(adapter->pdev_dev, rspq->size * rspq->iqe_len,
				  rspq->desc, rspq->phys_addr);
		rspq->desc = NULL;
	}
	if (fl && fl->desc) {
		kfree(fl->sdesc);
		fl->sdesc = NULL;
		dma_free_coherent(adapter->pdev_dev, flsz * EQ_UNIT,
				  fl->desc, fl->addr);
		fl->desc = NULL;
	}
	return ret;
}

int t4vf_sge_alloc_eth_txq(struct adapter *adapter, struct sge_eth_txq *txq,
			   struct net_device *dev, struct netdev_queue *devq,
			   unsigned int iqid)
{
	int ret, nentries;
	struct fw_eq_eth_cmd cmd, rpl;
	struct port_info *pi = netdev_priv(dev);

	nentries = txq->q.size + STAT_LEN / sizeof(struct tx_desc);

	txq->q.desc = alloc_ring(adapter->pdev_dev, txq->q.size,
				 sizeof(struct tx_desc),
				 sizeof(struct tx_sw_desc),
				 &txq->q.phys_addr, &txq->q.sdesc, STAT_LEN);
	if (!txq->q.desc)
		return -ENOMEM;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_vfn = cpu_to_be32(FW_CMD_OP(FW_EQ_ETH_CMD) |
				    FW_CMD_REQUEST |
				    FW_CMD_WRITE |
				    FW_CMD_EXEC);
	cmd.alloc_to_len16 = cpu_to_be32(FW_EQ_ETH_CMD_ALLOC |
					 FW_EQ_ETH_CMD_EQSTART |
					 FW_LEN16(cmd));
	cmd.viid_pkd = cpu_to_be32(FW_EQ_ETH_CMD_VIID(pi->viid));
	cmd.fetchszm_to_iqid =
		cpu_to_be32(FW_EQ_ETH_CMD_HOSTFCMODE(SGE_HOSTFCMODE_STPG) |
			    FW_EQ_ETH_CMD_PCIECHN(pi->port_id) |
			    FW_EQ_ETH_CMD_IQID(iqid));
	cmd.dcaen_to_eqsize =
		cpu_to_be32(FW_EQ_ETH_CMD_FBMIN(SGE_FETCHBURSTMIN_64B) |
			    FW_EQ_ETH_CMD_FBMAX(SGE_FETCHBURSTMAX_512B) |
			    FW_EQ_ETH_CMD_CIDXFTHRESH(SGE_CIDXFLUSHTHRESH_32) |
			    FW_EQ_ETH_CMD_EQSIZE(nentries));
	cmd.eqaddr = cpu_to_be64(txq->q.phys_addr);

	ret = t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), &rpl);
	if (ret) {
		kfree(txq->q.sdesc);
		txq->q.sdesc = NULL;
		dma_free_coherent(adapter->pdev_dev,
				  nentries * sizeof(struct tx_desc),
				  txq->q.desc, txq->q.phys_addr);
		txq->q.desc = NULL;
		return ret;
	}

	txq->q.in_use = 0;
	txq->q.cidx = 0;
	txq->q.pidx = 0;
	txq->q.stat = (void *)&txq->q.desc[txq->q.size];
	txq->q.cntxt_id = FW_EQ_ETH_CMD_EQID_GET(be32_to_cpu(rpl.eqid_pkd));
	txq->q.abs_id =
		FW_EQ_ETH_CMD_PHYSEQID_GET(be32_to_cpu(rpl.physeqid_pkd));
	txq->txq = devq;
	txq->tso = 0;
	txq->tx_cso = 0;
	txq->vlan_ins = 0;
	txq->q.stops = 0;
	txq->q.restarts = 0;
	txq->mapping_err = 0;
	return 0;
}

static void free_txq(struct adapter *adapter, struct sge_txq *tq)
{
	dma_free_coherent(adapter->pdev_dev,
			  tq->size * sizeof(*tq->desc) + STAT_LEN,
			  tq->desc, tq->phys_addr);
	tq->cntxt_id = 0;
	tq->sdesc = NULL;
	tq->desc = NULL;
}

static void free_rspq_fl(struct adapter *adapter, struct sge_rspq *rspq,
			 struct sge_fl *fl)
{
	unsigned int flid = fl ? fl->cntxt_id : 0xffff;

	t4vf_iq_free(adapter, FW_IQ_TYPE_FL_INT_CAP,
		     rspq->cntxt_id, flid, 0xffff);
	dma_free_coherent(adapter->pdev_dev, (rspq->size + 1) * rspq->iqe_len,
			  rspq->desc, rspq->phys_addr);
	netif_napi_del(&rspq->napi);
	rspq->netdev = NULL;
	rspq->cntxt_id = 0;
	rspq->abs_id = 0;
	rspq->desc = NULL;

	if (fl) {
		free_rx_bufs(adapter, fl, fl->avail);
		dma_free_coherent(adapter->pdev_dev,
				  fl->size * sizeof(*fl->desc) + STAT_LEN,
				  fl->desc, fl->addr);
		kfree(fl->sdesc);
		fl->sdesc = NULL;
		fl->cntxt_id = 0;
		fl->desc = NULL;
	}
}

void t4vf_free_sge_resources(struct adapter *adapter)
{
	struct sge *s = &adapter->sge;
	struct sge_eth_rxq *rxq = s->ethrxq;
	struct sge_eth_txq *txq = s->ethtxq;
	struct sge_rspq *evtq = &s->fw_evtq;
	struct sge_rspq *intrq = &s->intrq;
	int qs;

	for (qs = 0; qs < adapter->sge.ethqsets; qs++, rxq++, txq++) {
		if (rxq->rspq.desc)
			free_rspq_fl(adapter, &rxq->rspq, &rxq->fl);
		if (txq->q.desc) {
			t4vf_eth_eq_free(adapter, txq->q.cntxt_id);
			free_tx_desc(adapter, &txq->q, txq->q.in_use, true);
			kfree(txq->q.sdesc);
			free_txq(adapter, &txq->q);
		}
	}
	if (evtq->desc)
		free_rspq_fl(adapter, evtq, NULL);
	if (intrq->desc)
		free_rspq_fl(adapter, intrq, NULL);
}

void t4vf_sge_start(struct adapter *adapter)
{
	adapter->sge.ethtxq_rover = 0;
	mod_timer(&adapter->sge.rx_timer, jiffies + RX_QCHECK_PERIOD);
	mod_timer(&adapter->sge.tx_timer, jiffies + TX_QCHECK_PERIOD);
}

void t4vf_sge_stop(struct adapter *adapter)
{
	struct sge *s = &adapter->sge;

	if (s->rx_timer.function)
		del_timer_sync(&s->rx_timer);
	if (s->tx_timer.function)
		del_timer_sync(&s->tx_timer);
}

int t4vf_sge_init(struct adapter *adapter)
{
	struct sge_params *sge_params = &adapter->params.sge;
	u32 fl0 = sge_params->sge_fl_buffer_size[0];
	u32 fl1 = sge_params->sge_fl_buffer_size[1];
	struct sge *s = &adapter->sge;

	if (fl0 != PAGE_SIZE || (fl1 != 0 && fl1 <= fl0)) {
		dev_err(adapter->pdev_dev, "bad SGE FL buffer sizes [%d, %d]\n",
			fl0, fl1);
		return -EINVAL;
	}
	if ((sge_params->sge_control & RXPKTCPLMODE) == 0) {
		dev_err(adapter->pdev_dev, "bad SGE CPL MODE\n");
		return -EINVAL;
	}

	if (fl1)
		FL_PG_ORDER = ilog2(fl1) - PAGE_SHIFT;
	STAT_LEN = ((sge_params->sge_control & EGRSTATUSPAGESIZE) ? 128 : 64);
	PKTSHIFT = PKTSHIFT_GET(sge_params->sge_control);
	FL_ALIGN = 1 << (INGPADBOUNDARY_GET(sge_params->sge_control) +
			 SGE_INGPADBOUNDARY_SHIFT);

	setup_timer(&s->rx_timer, sge_rx_timer_cb, (unsigned long)adapter);
	setup_timer(&s->tx_timer, sge_tx_timer_cb, (unsigned long)adapter);

	spin_lock_init(&s->intrq_lock);

	return 0;
}
