/*
-*- linux-c -*-
   drbd_receiver.c
   This file is part of DRBD by Philipp Reisner and Lars Ellenberg.

   Copyright (C) 2001-2008, LINBIT Information Technologies GmbH.
   Copyright (C) 1999-2008, Philipp Reisner <philipp.reisner@linbit.com>.
   Copyright (C) 2002-2008, Lars Ellenberg <lars.ellenberg@linbit.com>.

   drbd is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   drbd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with drbd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _DRBD_VLI_H
#define _DRBD_VLI_H





#define VLI_L_1_1() do { \
	LEVEL( 2, 1, 0x00); \
	LEVEL( 3, 2, 0x01); \
	LEVEL( 5, 3, 0x03); \
	LEVEL( 7, 4, 0x07); \
	LEVEL(10, 5, 0x0f); \
	LEVEL(14, 6, 0x1f); \
	LEVEL(21, 8, 0x3f); \
	LEVEL(29, 8, 0x7f); \
	LEVEL(42, 8, 0xbf); \
	LEVEL(64, 8, 0xff); \
	} while (0)

static inline int vli_decode_bits(u64 *out, const u64 in)
{
	u64 adj = 1;

#define LEVEL(t,b,v)					\
	do {						\
		if ((in & ((1 << b) -1)) == v) {	\
			*out = ((in & ((~0ULL) >> (64-t))) >> b) + adj;	\
			return t;			\
		}					\
		adj += 1ULL << (t - b);			\
	} while (0)

	VLI_L_1_1();

	
	BUG();
#undef LEVEL
}

static inline int __vli_encode_bits(u64 *out, const u64 in)
{
	u64 max = 0;
	u64 adj = 1;

	if (in == 0)
		return -EINVAL;

#define LEVEL(t,b,v) do {		\
		max += 1ULL << (t - b);	\
		if (in <= max) {	\
			if (out)	\
				*out = ((in - adj) << b) | v;	\
			return t;	\
		}			\
		adj = max + 1;		\
	} while (0)

	VLI_L_1_1();

	return -EOVERFLOW;
#undef LEVEL
}

#undef VLI_L_1_1



struct bitstream_cursor {
	
	u8 *b;
	
	unsigned int bit;
};

static inline void bitstream_cursor_reset(struct bitstream_cursor *cur, void *s)
{
	cur->b = s;
	cur->bit = 0;
}

static inline void bitstream_cursor_advance(struct bitstream_cursor *cur, unsigned int bits)
{
	bits += cur->bit;
	cur->b = cur->b + (bits >> 3);
	cur->bit = bits & 7;
}

struct bitstream {
	struct bitstream_cursor cur;
	unsigned char *buf;
	size_t buf_len;		

	unsigned int pad_bits;
};

static inline void bitstream_init(struct bitstream *bs, void *s, size_t len, unsigned int pad_bits)
{
	bs->buf = s;
	bs->buf_len = len;
	bs->pad_bits = pad_bits;
	bitstream_cursor_reset(&bs->cur, bs->buf);
}

static inline void bitstream_rewind(struct bitstream *bs)
{
	bitstream_cursor_reset(&bs->cur, bs->buf);
	memset(bs->buf, 0, bs->buf_len);
}

static inline int bitstream_put_bits(struct bitstream *bs, u64 val, const unsigned int bits)
{
	unsigned char *b = bs->cur.b;
	unsigned int tmp;

	if (bits == 0)
		return 0;

	if ((bs->cur.b + ((bs->cur.bit + bits -1) >> 3)) - bs->buf >= bs->buf_len)
		return -ENOBUFS;

	
	if (bits < 64)
		val &= ~0ULL >> (64 - bits);

	*b++ |= (val & 0xff) << bs->cur.bit;

	for (tmp = 8 - bs->cur.bit; tmp < bits; tmp += 8)
		*b++ |= (val >> tmp) & 0xff;

	bitstream_cursor_advance(&bs->cur, bits);
	return bits;
}

static inline int bitstream_get_bits(struct bitstream *bs, u64 *out, int bits)
{
	u64 val;
	unsigned int n;

	if (bits > 64)
		return -EINVAL;

	if (bs->cur.b + ((bs->cur.bit + bs->pad_bits + bits -1) >> 3) - bs->buf >= bs->buf_len)
		bits = ((bs->buf_len - (bs->cur.b - bs->buf)) << 3)
			- bs->cur.bit - bs->pad_bits;

	if (bits == 0) {
		*out = 0;
		return 0;
	}

	
	val = 0;
	n = (bs->cur.bit + bits + 7) >> 3;
	
	
	if (n) {
		memcpy(&val, bs->cur.b+1, n - 1);
		val = le64_to_cpu(val) << (8 - bs->cur.bit);
	}

	
	val |= bs->cur.b[0] >> bs->cur.bit;

	
	val &= ~0ULL >> (64 - bits);

	bitstream_cursor_advance(&bs->cur, bits);
	*out = val;

	return bits;
}

static inline int vli_encode_bits(struct bitstream *bs, u64 in)
{
	u64 code = code;
	int bits = __vli_encode_bits(&code, in);

	if (bits <= 0)
		return bits;

	return bitstream_put_bits(bs, code, bits);
}

#endif
