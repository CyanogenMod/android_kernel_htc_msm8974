/*
 * linux/net/sunrpc/socklib.c
 *
 * Common socket helper routines for RPC client and server
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/compiler.h>
#include <linux/netdevice.h>
#include <linux/gfp.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <linux/pagemap.h>
#include <linux/udp.h>
#include <linux/sunrpc/xdr.h>
#include <linux/export.h>


size_t xdr_skb_read_bits(struct xdr_skb_reader *desc, void *to, size_t len)
{
	if (len > desc->count)
		len = desc->count;
	if (unlikely(skb_copy_bits(desc->skb, desc->offset, to, len)))
		return 0;
	desc->count -= len;
	desc->offset += len;
	return len;
}
EXPORT_SYMBOL_GPL(xdr_skb_read_bits);

static size_t xdr_skb_read_and_csum_bits(struct xdr_skb_reader *desc, void *to, size_t len)
{
	unsigned int pos;
	__wsum csum2;

	if (len > desc->count)
		len = desc->count;
	pos = desc->offset;
	csum2 = skb_copy_and_csum_bits(desc->skb, pos, to, len, 0);
	desc->csum = csum_block_add(desc->csum, csum2, pos);
	desc->count -= len;
	desc->offset += len;
	return len;
}

ssize_t xdr_partial_copy_from_skb(struct xdr_buf *xdr, unsigned int base, struct xdr_skb_reader *desc, xdr_skb_read_actor copy_actor)
{
	struct page	**ppage = xdr->pages;
	unsigned int	len, pglen = xdr->page_len;
	ssize_t		copied = 0;
	size_t		ret;

	len = xdr->head[0].iov_len;
	if (base < len) {
		len -= base;
		ret = copy_actor(desc, (char *)xdr->head[0].iov_base + base, len);
		copied += ret;
		if (ret != len || !desc->count)
			goto out;
		base = 0;
	} else
		base -= len;

	if (unlikely(pglen == 0))
		goto copy_tail;
	if (unlikely(base >= pglen)) {
		base -= pglen;
		goto copy_tail;
	}
	if (base || xdr->page_base) {
		pglen -= base;
		base += xdr->page_base;
		ppage += base >> PAGE_CACHE_SHIFT;
		base &= ~PAGE_CACHE_MASK;
	}
	do {
		char *kaddr;

		if (unlikely(*ppage == NULL)) {
			*ppage = alloc_page(GFP_ATOMIC);
			if (unlikely(*ppage == NULL)) {
				if (copied == 0)
					copied = -ENOMEM;
				goto out;
			}
		}

		len = PAGE_CACHE_SIZE;
		kaddr = kmap_atomic(*ppage);
		if (base) {
			len -= base;
			if (pglen < len)
				len = pglen;
			ret = copy_actor(desc, kaddr + base, len);
			base = 0;
		} else {
			if (pglen < len)
				len = pglen;
			ret = copy_actor(desc, kaddr, len);
		}
		flush_dcache_page(*ppage);
		kunmap_atomic(kaddr);
		copied += ret;
		if (ret != len || !desc->count)
			goto out;
		ppage++;
	} while ((pglen -= len) != 0);
copy_tail:
	len = xdr->tail[0].iov_len;
	if (base < len)
		copied += copy_actor(desc, (char *)xdr->tail[0].iov_base + base, len - base);
out:
	return copied;
}
EXPORT_SYMBOL_GPL(xdr_partial_copy_from_skb);

int csum_partial_copy_to_xdr(struct xdr_buf *xdr, struct sk_buff *skb)
{
	struct xdr_skb_reader	desc;

	desc.skb = skb;
	desc.offset = sizeof(struct udphdr);
	desc.count = skb->len - desc.offset;

	if (skb_csum_unnecessary(skb))
		goto no_checksum;

	desc.csum = csum_partial(skb->data, desc.offset, skb->csum);
	if (xdr_partial_copy_from_skb(xdr, 0, &desc, xdr_skb_read_and_csum_bits) < 0)
		return -1;
	if (desc.offset != skb->len) {
		__wsum csum2;
		csum2 = skb_checksum(skb, desc.offset, skb->len - desc.offset, 0);
		desc.csum = csum_block_add(desc.csum, csum2, desc.offset);
	}
	if (desc.count)
		return -1;
	if (csum_fold(desc.csum))
		return -1;
	if (unlikely(skb->ip_summed == CHECKSUM_COMPLETE))
		netdev_rx_csum_fault(skb->dev);
	return 0;
no_checksum:
	if (xdr_partial_copy_from_skb(xdr, 0, &desc, xdr_skb_read_bits) < 0)
		return -1;
	if (desc.count)
		return -1;
	return 0;
}
EXPORT_SYMBOL_GPL(csum_partial_copy_to_xdr);
