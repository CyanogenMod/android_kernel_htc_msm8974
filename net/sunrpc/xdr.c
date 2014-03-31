/*
 * linux/net/sunrpc/xdr.c
 *
 * Generic XDR support.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/pagemap.h>
#include <linux/errno.h>
#include <linux/sunrpc/xdr.h>
#include <linux/sunrpc/msg_prot.h>

__be32 *
xdr_encode_netobj(__be32 *p, const struct xdr_netobj *obj)
{
	unsigned int	quadlen = XDR_QUADLEN(obj->len);

	p[quadlen] = 0;		
	*p++ = cpu_to_be32(obj->len);
	memcpy(p, obj->data, obj->len);
	return p + XDR_QUADLEN(obj->len);
}
EXPORT_SYMBOL_GPL(xdr_encode_netobj);

__be32 *
xdr_decode_netobj(__be32 *p, struct xdr_netobj *obj)
{
	unsigned int	len;

	if ((len = be32_to_cpu(*p++)) > XDR_MAX_NETOBJ)
		return NULL;
	obj->len  = len;
	obj->data = (u8 *) p;
	return p + XDR_QUADLEN(len);
}
EXPORT_SYMBOL_GPL(xdr_decode_netobj);

__be32 *xdr_encode_opaque_fixed(__be32 *p, const void *ptr, unsigned int nbytes)
{
	if (likely(nbytes != 0)) {
		unsigned int quadlen = XDR_QUADLEN(nbytes);
		unsigned int padding = (quadlen << 2) - nbytes;

		if (ptr != NULL)
			memcpy(p, ptr, nbytes);
		if (padding != 0)
			memset((char *)p + nbytes, 0, padding);
		p += quadlen;
	}
	return p;
}
EXPORT_SYMBOL_GPL(xdr_encode_opaque_fixed);

__be32 *xdr_encode_opaque(__be32 *p, const void *ptr, unsigned int nbytes)
{
	*p++ = cpu_to_be32(nbytes);
	return xdr_encode_opaque_fixed(p, ptr, nbytes);
}
EXPORT_SYMBOL_GPL(xdr_encode_opaque);

__be32 *
xdr_encode_string(__be32 *p, const char *string)
{
	return xdr_encode_array(p, string, strlen(string));
}
EXPORT_SYMBOL_GPL(xdr_encode_string);

__be32 *
xdr_decode_string_inplace(__be32 *p, char **sp,
			  unsigned int *lenp, unsigned int maxlen)
{
	u32 len;

	len = be32_to_cpu(*p++);
	if (len > maxlen)
		return NULL;
	*lenp = len;
	*sp = (char *) p;
	return p + XDR_QUADLEN(len);
}
EXPORT_SYMBOL_GPL(xdr_decode_string_inplace);

void
xdr_terminate_string(struct xdr_buf *buf, const u32 len)
{
	char *kaddr;

	kaddr = kmap_atomic(buf->pages[0]);
	kaddr[buf->page_base + len] = '\0';
	kunmap_atomic(kaddr);
}
EXPORT_SYMBOL_GPL(xdr_terminate_string);

void
xdr_encode_pages(struct xdr_buf *xdr, struct page **pages, unsigned int base,
		 unsigned int len)
{
	struct kvec *tail = xdr->tail;
	u32 *p;

	xdr->pages = pages;
	xdr->page_base = base;
	xdr->page_len = len;

	p = (u32 *)xdr->head[0].iov_base + XDR_QUADLEN(xdr->head[0].iov_len);
	tail->iov_base = p;
	tail->iov_len = 0;

	if (len & 3) {
		unsigned int pad = 4 - (len & 3);

		*p = 0;
		tail->iov_base = (char *)p + (len & 3);
		tail->iov_len  = pad;
		len += pad;
	}
	xdr->buflen += len;
	xdr->len += len;
}
EXPORT_SYMBOL_GPL(xdr_encode_pages);

void
xdr_inline_pages(struct xdr_buf *xdr, unsigned int offset,
		 struct page **pages, unsigned int base, unsigned int len)
{
	struct kvec *head = xdr->head;
	struct kvec *tail = xdr->tail;
	char *buf = (char *)head->iov_base;
	unsigned int buflen = head->iov_len;

	head->iov_len  = offset;

	xdr->pages = pages;
	xdr->page_base = base;
	xdr->page_len = len;

	tail->iov_base = buf + offset;
	tail->iov_len = buflen - offset;

	xdr->buflen += len;
}
EXPORT_SYMBOL_GPL(xdr_inline_pages);

static void
_shift_data_right_pages(struct page **pages, size_t pgto_base,
		size_t pgfrom_base, size_t len)
{
	struct page **pgfrom, **pgto;
	char *vfrom, *vto;
	size_t copy;

	BUG_ON(pgto_base <= pgfrom_base);

	pgto_base += len;
	pgfrom_base += len;

	pgto = pages + (pgto_base >> PAGE_CACHE_SHIFT);
	pgfrom = pages + (pgfrom_base >> PAGE_CACHE_SHIFT);

	pgto_base &= ~PAGE_CACHE_MASK;
	pgfrom_base &= ~PAGE_CACHE_MASK;

	do {
		
		if (pgto_base == 0) {
			pgto_base = PAGE_CACHE_SIZE;
			pgto--;
		}
		if (pgfrom_base == 0) {
			pgfrom_base = PAGE_CACHE_SIZE;
			pgfrom--;
		}

		copy = len;
		if (copy > pgto_base)
			copy = pgto_base;
		if (copy > pgfrom_base)
			copy = pgfrom_base;
		pgto_base -= copy;
		pgfrom_base -= copy;

		vto = kmap_atomic(*pgto);
		vfrom = kmap_atomic(*pgfrom);
		memmove(vto + pgto_base, vfrom + pgfrom_base, copy);
		flush_dcache_page(*pgto);
		kunmap_atomic(vfrom);
		kunmap_atomic(vto);

	} while ((len -= copy) != 0);
}

static void
_copy_to_pages(struct page **pages, size_t pgbase, const char *p, size_t len)
{
	struct page **pgto;
	char *vto;
	size_t copy;

	pgto = pages + (pgbase >> PAGE_CACHE_SHIFT);
	pgbase &= ~PAGE_CACHE_MASK;

	for (;;) {
		copy = PAGE_CACHE_SIZE - pgbase;
		if (copy > len)
			copy = len;

		vto = kmap_atomic(*pgto);
		memcpy(vto + pgbase, p, copy);
		kunmap_atomic(vto);

		len -= copy;
		if (len == 0)
			break;

		pgbase += copy;
		if (pgbase == PAGE_CACHE_SIZE) {
			flush_dcache_page(*pgto);
			pgbase = 0;
			pgto++;
		}
		p += copy;
	}
	flush_dcache_page(*pgto);
}

void
_copy_from_pages(char *p, struct page **pages, size_t pgbase, size_t len)
{
	struct page **pgfrom;
	char *vfrom;
	size_t copy;

	pgfrom = pages + (pgbase >> PAGE_CACHE_SHIFT);
	pgbase &= ~PAGE_CACHE_MASK;

	do {
		copy = PAGE_CACHE_SIZE - pgbase;
		if (copy > len)
			copy = len;

		vfrom = kmap_atomic(*pgfrom);
		memcpy(p, vfrom + pgbase, copy);
		kunmap_atomic(vfrom);

		pgbase += copy;
		if (pgbase == PAGE_CACHE_SIZE) {
			pgbase = 0;
			pgfrom++;
		}
		p += copy;

	} while ((len -= copy) != 0);
}
EXPORT_SYMBOL_GPL(_copy_from_pages);

static void
xdr_shrink_bufhead(struct xdr_buf *buf, size_t len)
{
	struct kvec *head, *tail;
	size_t copy, offs;
	unsigned int pglen = buf->page_len;

	tail = buf->tail;
	head = buf->head;
	BUG_ON (len > head->iov_len);

	
	if (tail->iov_len != 0) {
		if (tail->iov_len > len) {
			copy = tail->iov_len - len;
			memmove((char *)tail->iov_base + len,
					tail->iov_base, copy);
		}
		
		copy = len;
		if (copy > pglen)
			copy = pglen;
		offs = len - copy;
		if (offs >= tail->iov_len)
			copy = 0;
		else if (copy > tail->iov_len - offs)
			copy = tail->iov_len - offs;
		if (copy != 0)
			_copy_from_pages((char *)tail->iov_base + offs,
					buf->pages,
					buf->page_base + pglen + offs - len,
					copy);
		
		if (len > pglen) {
			offs = copy = len - pglen;
			if (copy > tail->iov_len)
				copy = tail->iov_len;
			memcpy(tail->iov_base,
					(char *)head->iov_base +
					head->iov_len - offs,
					copy);
		}
	}
	
	if (pglen != 0) {
		if (pglen > len)
			_shift_data_right_pages(buf->pages,
					buf->page_base + len,
					buf->page_base,
					pglen - len);
		copy = len;
		if (len > pglen)
			copy = pglen;
		_copy_to_pages(buf->pages, buf->page_base,
				(char *)head->iov_base + head->iov_len - len,
				copy);
	}
	head->iov_len -= len;
	buf->buflen -= len;
	
	if (buf->len > buf->buflen)
		buf->len = buf->buflen;
}

static void
xdr_shrink_pagelen(struct xdr_buf *buf, size_t len)
{
	struct kvec *tail;
	size_t copy;
	unsigned int pglen = buf->page_len;
	unsigned int tailbuf_len;

	tail = buf->tail;
	BUG_ON (len > pglen);

	tailbuf_len = buf->buflen - buf->head->iov_len - buf->page_len;

	
	if (tailbuf_len != 0) {
		unsigned int free_space = tailbuf_len - tail->iov_len;

		if (len < free_space)
			free_space = len;
		tail->iov_len += free_space;

		copy = len;
		if (tail->iov_len > len) {
			char *p = (char *)tail->iov_base + len;
			memmove(p, tail->iov_base, tail->iov_len - len);
		} else
			copy = tail->iov_len;
		
		_copy_from_pages((char *)tail->iov_base,
				buf->pages, buf->page_base + pglen - len,
				copy);
	}
	buf->page_len -= len;
	buf->buflen -= len;
	
	if (buf->len > buf->buflen)
		buf->len = buf->buflen;
}

void
xdr_shift_buf(struct xdr_buf *buf, size_t len)
{
	xdr_shrink_bufhead(buf, len);
}
EXPORT_SYMBOL_GPL(xdr_shift_buf);

void xdr_init_encode(struct xdr_stream *xdr, struct xdr_buf *buf, __be32 *p)
{
	struct kvec *iov = buf->head;
	int scratch_len = buf->buflen - buf->page_len - buf->tail[0].iov_len;

	BUG_ON(scratch_len < 0);
	xdr->buf = buf;
	xdr->iov = iov;
	xdr->p = (__be32 *)((char *)iov->iov_base + iov->iov_len);
	xdr->end = (__be32 *)((char *)iov->iov_base + scratch_len);
	BUG_ON(iov->iov_len > scratch_len);

	if (p != xdr->p && p != NULL) {
		size_t len;

		BUG_ON(p < xdr->p || p > xdr->end);
		len = (char *)p - (char *)xdr->p;
		xdr->p = p;
		buf->len += len;
		iov->iov_len += len;
	}
}
EXPORT_SYMBOL_GPL(xdr_init_encode);

__be32 * xdr_reserve_space(struct xdr_stream *xdr, size_t nbytes)
{
	__be32 *p = xdr->p;
	__be32 *q;

	
	nbytes += 3;
	nbytes &= ~3;
	q = p + (nbytes >> 2);
	if (unlikely(q > xdr->end || q < p))
		return NULL;
	xdr->p = q;
	xdr->iov->iov_len += nbytes;
	xdr->buf->len += nbytes;
	return p;
}
EXPORT_SYMBOL_GPL(xdr_reserve_space);

void xdr_write_pages(struct xdr_stream *xdr, struct page **pages, unsigned int base,
		 unsigned int len)
{
	struct xdr_buf *buf = xdr->buf;
	struct kvec *iov = buf->tail;
	buf->pages = pages;
	buf->page_base = base;
	buf->page_len = len;

	iov->iov_base = (char *)xdr->p;
	iov->iov_len  = 0;
	xdr->iov = iov;

	if (len & 3) {
		unsigned int pad = 4 - (len & 3);

		BUG_ON(xdr->p >= xdr->end);
		iov->iov_base = (char *)xdr->p + (len & 3);
		iov->iov_len  += pad;
		len += pad;
		*xdr->p++ = 0;
	}
	buf->buflen += len;
	buf->len += len;
}
EXPORT_SYMBOL_GPL(xdr_write_pages);

static void xdr_set_iov(struct xdr_stream *xdr, struct kvec *iov,
		__be32 *p, unsigned int len)
{
	if (len > iov->iov_len)
		len = iov->iov_len;
	if (p == NULL)
		p = (__be32*)iov->iov_base;
	xdr->p = p;
	xdr->end = (__be32*)(iov->iov_base + len);
	xdr->iov = iov;
	xdr->page_ptr = NULL;
}

static int xdr_set_page_base(struct xdr_stream *xdr,
		unsigned int base, unsigned int len)
{
	unsigned int pgnr;
	unsigned int maxlen;
	unsigned int pgoff;
	unsigned int pgend;
	void *kaddr;

	maxlen = xdr->buf->page_len;
	if (base >= maxlen)
		return -EINVAL;
	maxlen -= base;
	if (len > maxlen)
		len = maxlen;

	base += xdr->buf->page_base;

	pgnr = base >> PAGE_SHIFT;
	xdr->page_ptr = &xdr->buf->pages[pgnr];
	kaddr = page_address(*xdr->page_ptr);

	pgoff = base & ~PAGE_MASK;
	xdr->p = (__be32*)(kaddr + pgoff);

	pgend = pgoff + len;
	if (pgend > PAGE_SIZE)
		pgend = PAGE_SIZE;
	xdr->end = (__be32*)(kaddr + pgend);
	xdr->iov = NULL;
	return 0;
}

static void xdr_set_next_page(struct xdr_stream *xdr)
{
	unsigned int newbase;

	newbase = (1 + xdr->page_ptr - xdr->buf->pages) << PAGE_SHIFT;
	newbase -= xdr->buf->page_base;

	if (xdr_set_page_base(xdr, newbase, PAGE_SIZE) < 0)
		xdr_set_iov(xdr, xdr->buf->tail, NULL, xdr->buf->len);
}

static bool xdr_set_next_buffer(struct xdr_stream *xdr)
{
	if (xdr->page_ptr != NULL)
		xdr_set_next_page(xdr);
	else if (xdr->iov == xdr->buf->head) {
		if (xdr_set_page_base(xdr, 0, PAGE_SIZE) < 0)
			xdr_set_iov(xdr, xdr->buf->tail, NULL, xdr->buf->len);
	}
	return xdr->p != xdr->end;
}

void xdr_init_decode(struct xdr_stream *xdr, struct xdr_buf *buf, __be32 *p)
{
	xdr->buf = buf;
	xdr->scratch.iov_base = NULL;
	xdr->scratch.iov_len = 0;
	if (buf->head[0].iov_len != 0)
		xdr_set_iov(xdr, buf->head, p, buf->len);
	else if (buf->page_len != 0)
		xdr_set_page_base(xdr, 0, buf->len);
}
EXPORT_SYMBOL_GPL(xdr_init_decode);

void xdr_init_decode_pages(struct xdr_stream *xdr, struct xdr_buf *buf,
			   struct page **pages, unsigned int len)
{
	memset(buf, 0, sizeof(*buf));
	buf->pages =  pages;
	buf->page_len =  len;
	buf->buflen =  len;
	buf->len = len;
	xdr_init_decode(xdr, buf, NULL);
}
EXPORT_SYMBOL_GPL(xdr_init_decode_pages);

static __be32 * __xdr_inline_decode(struct xdr_stream *xdr, size_t nbytes)
{
	__be32 *p = xdr->p;
	__be32 *q = p + XDR_QUADLEN(nbytes);

	if (unlikely(q > xdr->end || q < p))
		return NULL;
	xdr->p = q;
	return p;
}

void xdr_set_scratch_buffer(struct xdr_stream *xdr, void *buf, size_t buflen)
{
	xdr->scratch.iov_base = buf;
	xdr->scratch.iov_len = buflen;
}
EXPORT_SYMBOL_GPL(xdr_set_scratch_buffer);

static __be32 *xdr_copy_to_scratch(struct xdr_stream *xdr, size_t nbytes)
{
	__be32 *p;
	void *cpdest = xdr->scratch.iov_base;
	size_t cplen = (char *)xdr->end - (char *)xdr->p;

	if (nbytes > xdr->scratch.iov_len)
		return NULL;
	memcpy(cpdest, xdr->p, cplen);
	cpdest += cplen;
	nbytes -= cplen;
	if (!xdr_set_next_buffer(xdr))
		return NULL;
	p = __xdr_inline_decode(xdr, nbytes);
	if (p == NULL)
		return NULL;
	memcpy(cpdest, p, nbytes);
	return xdr->scratch.iov_base;
}

__be32 * xdr_inline_decode(struct xdr_stream *xdr, size_t nbytes)
{
	__be32 *p;

	if (nbytes == 0)
		return xdr->p;
	if (xdr->p == xdr->end && !xdr_set_next_buffer(xdr))
		return NULL;
	p = __xdr_inline_decode(xdr, nbytes);
	if (p != NULL)
		return p;
	return xdr_copy_to_scratch(xdr, nbytes);
}
EXPORT_SYMBOL_GPL(xdr_inline_decode);

void xdr_read_pages(struct xdr_stream *xdr, unsigned int len)
{
	struct xdr_buf *buf = xdr->buf;
	struct kvec *iov;
	ssize_t shift;
	unsigned int end;
	int padding;

	
	iov  = buf->head;
	shift = iov->iov_len + (char *)iov->iov_base - (char *)xdr->p;
	if (shift > 0)
		xdr_shrink_bufhead(buf, shift);

	
	if (buf->page_len > len)
		xdr_shrink_pagelen(buf, buf->page_len - len);
	padding = (XDR_QUADLEN(len) << 2) - len;
	xdr->iov = iov = buf->tail;
	
	end = iov->iov_len;
	shift = buf->buflen - buf->len;
	if (shift < end)
		end -= shift;
	else if (shift > 0)
		end = 0;
	xdr->p = (__be32 *)((char *)iov->iov_base + padding);
	xdr->end = (__be32 *)((char *)iov->iov_base + end);
}
EXPORT_SYMBOL_GPL(xdr_read_pages);

void xdr_enter_page(struct xdr_stream *xdr, unsigned int len)
{
	xdr_read_pages(xdr, len);
	xdr_set_page_base(xdr, 0, len);
}
EXPORT_SYMBOL_GPL(xdr_enter_page);

static struct kvec empty_iov = {.iov_base = NULL, .iov_len = 0};

void
xdr_buf_from_iov(struct kvec *iov, struct xdr_buf *buf)
{
	buf->head[0] = *iov;
	buf->tail[0] = empty_iov;
	buf->page_len = 0;
	buf->buflen = buf->len = iov->iov_len;
}
EXPORT_SYMBOL_GPL(xdr_buf_from_iov);

int
xdr_buf_subsegment(struct xdr_buf *buf, struct xdr_buf *subbuf,
			unsigned int base, unsigned int len)
{
	subbuf->buflen = subbuf->len = len;
	if (base < buf->head[0].iov_len) {
		subbuf->head[0].iov_base = buf->head[0].iov_base + base;
		subbuf->head[0].iov_len = min_t(unsigned int, len,
						buf->head[0].iov_len - base);
		len -= subbuf->head[0].iov_len;
		base = 0;
	} else {
		subbuf->head[0].iov_base = NULL;
		subbuf->head[0].iov_len = 0;
		base -= buf->head[0].iov_len;
	}

	if (base < buf->page_len) {
		subbuf->page_len = min(buf->page_len - base, len);
		base += buf->page_base;
		subbuf->page_base = base & ~PAGE_CACHE_MASK;
		subbuf->pages = &buf->pages[base >> PAGE_CACHE_SHIFT];
		len -= subbuf->page_len;
		base = 0;
	} else {
		base -= buf->page_len;
		subbuf->page_len = 0;
	}

	if (base < buf->tail[0].iov_len) {
		subbuf->tail[0].iov_base = buf->tail[0].iov_base + base;
		subbuf->tail[0].iov_len = min_t(unsigned int, len,
						buf->tail[0].iov_len - base);
		len -= subbuf->tail[0].iov_len;
		base = 0;
	} else {
		subbuf->tail[0].iov_base = NULL;
		subbuf->tail[0].iov_len = 0;
		base -= buf->tail[0].iov_len;
	}

	if (base || len)
		return -1;
	return 0;
}
EXPORT_SYMBOL_GPL(xdr_buf_subsegment);

static void __read_bytes_from_xdr_buf(struct xdr_buf *subbuf, void *obj, unsigned int len)
{
	unsigned int this_len;

	this_len = min_t(unsigned int, len, subbuf->head[0].iov_len);
	memcpy(obj, subbuf->head[0].iov_base, this_len);
	len -= this_len;
	obj += this_len;
	this_len = min_t(unsigned int, len, subbuf->page_len);
	if (this_len)
		_copy_from_pages(obj, subbuf->pages, subbuf->page_base, this_len);
	len -= this_len;
	obj += this_len;
	this_len = min_t(unsigned int, len, subbuf->tail[0].iov_len);
	memcpy(obj, subbuf->tail[0].iov_base, this_len);
}

int read_bytes_from_xdr_buf(struct xdr_buf *buf, unsigned int base, void *obj, unsigned int len)
{
	struct xdr_buf subbuf;
	int status;

	status = xdr_buf_subsegment(buf, &subbuf, base, len);
	if (status != 0)
		return status;
	__read_bytes_from_xdr_buf(&subbuf, obj, len);
	return 0;
}
EXPORT_SYMBOL_GPL(read_bytes_from_xdr_buf);

static void __write_bytes_to_xdr_buf(struct xdr_buf *subbuf, void *obj, unsigned int len)
{
	unsigned int this_len;

	this_len = min_t(unsigned int, len, subbuf->head[0].iov_len);
	memcpy(subbuf->head[0].iov_base, obj, this_len);
	len -= this_len;
	obj += this_len;
	this_len = min_t(unsigned int, len, subbuf->page_len);
	if (this_len)
		_copy_to_pages(subbuf->pages, subbuf->page_base, obj, this_len);
	len -= this_len;
	obj += this_len;
	this_len = min_t(unsigned int, len, subbuf->tail[0].iov_len);
	memcpy(subbuf->tail[0].iov_base, obj, this_len);
}

int write_bytes_to_xdr_buf(struct xdr_buf *buf, unsigned int base, void *obj, unsigned int len)
{
	struct xdr_buf subbuf;
	int status;

	status = xdr_buf_subsegment(buf, &subbuf, base, len);
	if (status != 0)
		return status;
	__write_bytes_to_xdr_buf(&subbuf, obj, len);
	return 0;
}
EXPORT_SYMBOL_GPL(write_bytes_to_xdr_buf);

int
xdr_decode_word(struct xdr_buf *buf, unsigned int base, u32 *obj)
{
	__be32	raw;
	int	status;

	status = read_bytes_from_xdr_buf(buf, base, &raw, sizeof(*obj));
	if (status)
		return status;
	*obj = be32_to_cpu(raw);
	return 0;
}
EXPORT_SYMBOL_GPL(xdr_decode_word);

int
xdr_encode_word(struct xdr_buf *buf, unsigned int base, u32 obj)
{
	__be32	raw = cpu_to_be32(obj);

	return write_bytes_to_xdr_buf(buf, base, &raw, sizeof(obj));
}
EXPORT_SYMBOL_GPL(xdr_encode_word);

int xdr_buf_read_netobj(struct xdr_buf *buf, struct xdr_netobj *obj, unsigned int offset)
{
	struct xdr_buf subbuf;

	if (xdr_decode_word(buf, offset, &obj->len))
		return -EFAULT;
	if (xdr_buf_subsegment(buf, &subbuf, offset + 4, obj->len))
		return -EFAULT;

	
	obj->data = subbuf.head[0].iov_base;
	if (subbuf.head[0].iov_len == obj->len)
		return 0;
	
	obj->data = subbuf.tail[0].iov_base;
	if (subbuf.tail[0].iov_len == obj->len)
		return 0;

	if (obj->len > buf->buflen - buf->len)
		return -ENOMEM;
	if (buf->tail[0].iov_len != 0)
		obj->data = buf->tail[0].iov_base + buf->tail[0].iov_len;
	else
		obj->data = buf->head[0].iov_base + buf->head[0].iov_len;
	__read_bytes_from_xdr_buf(&subbuf, obj->data, obj->len);
	return 0;
}
EXPORT_SYMBOL_GPL(xdr_buf_read_netobj);

static int
xdr_xcode_array2(struct xdr_buf *buf, unsigned int base,
		 struct xdr_array2_desc *desc, int encode)
{
	char *elem = NULL, *c;
	unsigned int copied = 0, todo, avail_here;
	struct page **ppages = NULL;
	int err;

	if (encode) {
		if (xdr_encode_word(buf, base, desc->array_len) != 0)
			return -EINVAL;
	} else {
		if (xdr_decode_word(buf, base, &desc->array_len) != 0 ||
		    desc->array_len > desc->array_maxlen ||
		    (unsigned long) base + 4 + desc->array_len *
				    desc->elem_size > buf->len)
			return -EINVAL;
	}
	base += 4;

	if (!desc->xcode)
		return 0;

	todo = desc->array_len * desc->elem_size;

	
	if (todo && base < buf->head->iov_len) {
		c = buf->head->iov_base + base;
		avail_here = min_t(unsigned int, todo,
				   buf->head->iov_len - base);
		todo -= avail_here;

		while (avail_here >= desc->elem_size) {
			err = desc->xcode(desc, c);
			if (err)
				goto out;
			c += desc->elem_size;
			avail_here -= desc->elem_size;
		}
		if (avail_here) {
			if (!elem) {
				elem = kmalloc(desc->elem_size, GFP_KERNEL);
				err = -ENOMEM;
				if (!elem)
					goto out;
			}
			if (encode) {
				err = desc->xcode(desc, elem);
				if (err)
					goto out;
				memcpy(c, elem, avail_here);
			} else
				memcpy(elem, c, avail_here);
			copied = avail_here;
		}
		base = buf->head->iov_len;  
	}

	
	base -= buf->head->iov_len;
	if (todo && base < buf->page_len) {
		unsigned int avail_page;

		avail_here = min(todo, buf->page_len - base);
		todo -= avail_here;

		base += buf->page_base;
		ppages = buf->pages + (base >> PAGE_CACHE_SHIFT);
		base &= ~PAGE_CACHE_MASK;
		avail_page = min_t(unsigned int, PAGE_CACHE_SIZE - base,
					avail_here);
		c = kmap(*ppages) + base;

		while (avail_here) {
			avail_here -= avail_page;
			if (copied || avail_page < desc->elem_size) {
				unsigned int l = min(avail_page,
					desc->elem_size - copied);
				if (!elem) {
					elem = kmalloc(desc->elem_size,
						       GFP_KERNEL);
					err = -ENOMEM;
					if (!elem)
						goto out;
				}
				if (encode) {
					if (!copied) {
						err = desc->xcode(desc, elem);
						if (err)
							goto out;
					}
					memcpy(c, elem + copied, l);
					copied += l;
					if (copied == desc->elem_size)
						copied = 0;
				} else {
					memcpy(elem + copied, c, l);
					copied += l;
					if (copied == desc->elem_size) {
						err = desc->xcode(desc, elem);
						if (err)
							goto out;
						copied = 0;
					}
				}
				avail_page -= l;
				c += l;
			}
			while (avail_page >= desc->elem_size) {
				err = desc->xcode(desc, c);
				if (err)
					goto out;
				c += desc->elem_size;
				avail_page -= desc->elem_size;
			}
			if (avail_page) {
				unsigned int l = min(avail_page,
					    desc->elem_size - copied);
				if (!elem) {
					elem = kmalloc(desc->elem_size,
						       GFP_KERNEL);
					err = -ENOMEM;
					if (!elem)
						goto out;
				}
				if (encode) {
					if (!copied) {
						err = desc->xcode(desc, elem);
						if (err)
							goto out;
					}
					memcpy(c, elem + copied, l);
					copied += l;
					if (copied == desc->elem_size)
						copied = 0;
				} else {
					memcpy(elem + copied, c, l);
					copied += l;
					if (copied == desc->elem_size) {
						err = desc->xcode(desc, elem);
						if (err)
							goto out;
						copied = 0;
					}
				}
			}
			if (avail_here) {
				kunmap(*ppages);
				ppages++;
				c = kmap(*ppages);
			}

			avail_page = min(avail_here,
				 (unsigned int) PAGE_CACHE_SIZE);
		}
		base = buf->page_len;  
	}

	
	base -= buf->page_len;
	if (todo) {
		c = buf->tail->iov_base + base;
		if (copied) {
			unsigned int l = desc->elem_size - copied;

			if (encode)
				memcpy(c, elem + copied, l);
			else {
				memcpy(elem + copied, c, l);
				err = desc->xcode(desc, elem);
				if (err)
					goto out;
			}
			todo -= l;
			c += l;
		}
		while (todo) {
			err = desc->xcode(desc, c);
			if (err)
				goto out;
			c += desc->elem_size;
			todo -= desc->elem_size;
		}
	}
	err = 0;

out:
	kfree(elem);
	if (ppages)
		kunmap(*ppages);
	return err;
}

int
xdr_decode_array2(struct xdr_buf *buf, unsigned int base,
		  struct xdr_array2_desc *desc)
{
	if (base >= buf->len)
		return -EINVAL;

	return xdr_xcode_array2(buf, base, desc, 0);
}
EXPORT_SYMBOL_GPL(xdr_decode_array2);

int
xdr_encode_array2(struct xdr_buf *buf, unsigned int base,
		  struct xdr_array2_desc *desc)
{
	if ((unsigned long) base + 4 + desc->array_len * desc->elem_size >
	    buf->head->iov_len + buf->page_len + buf->tail->iov_len)
		return -EINVAL;

	return xdr_xcode_array2(buf, base, desc, 1);
}
EXPORT_SYMBOL_GPL(xdr_encode_array2);

int
xdr_process_buf(struct xdr_buf *buf, unsigned int offset, unsigned int len,
		int (*actor)(struct scatterlist *, void *), void *data)
{
	int i, ret = 0;
	unsigned page_len, thislen, page_offset;
	struct scatterlist      sg[1];

	sg_init_table(sg, 1);

	if (offset >= buf->head[0].iov_len) {
		offset -= buf->head[0].iov_len;
	} else {
		thislen = buf->head[0].iov_len - offset;
		if (thislen > len)
			thislen = len;
		sg_set_buf(sg, buf->head[0].iov_base + offset, thislen);
		ret = actor(sg, data);
		if (ret)
			goto out;
		offset = 0;
		len -= thislen;
	}
	if (len == 0)
		goto out;

	if (offset >= buf->page_len) {
		offset -= buf->page_len;
	} else {
		page_len = buf->page_len - offset;
		if (page_len > len)
			page_len = len;
		len -= page_len;
		page_offset = (offset + buf->page_base) & (PAGE_CACHE_SIZE - 1);
		i = (offset + buf->page_base) >> PAGE_CACHE_SHIFT;
		thislen = PAGE_CACHE_SIZE - page_offset;
		do {
			if (thislen > page_len)
				thislen = page_len;
			sg_set_page(sg, buf->pages[i], thislen, page_offset);
			ret = actor(sg, data);
			if (ret)
				goto out;
			page_len -= thislen;
			i++;
			page_offset = 0;
			thislen = PAGE_CACHE_SIZE;
		} while (page_len != 0);
		offset = 0;
	}
	if (len == 0)
		goto out;
	if (offset < buf->tail[0].iov_len) {
		thislen = buf->tail[0].iov_len - offset;
		if (thislen > len)
			thislen = len;
		sg_set_buf(sg, buf->tail[0].iov_base + offset, thislen);
		ret = actor(sg, data);
		len -= thislen;
	}
	if (len != 0)
		ret = -EINVAL;
out:
	return ret;
}
EXPORT_SYMBOL_GPL(xdr_process_buf);

