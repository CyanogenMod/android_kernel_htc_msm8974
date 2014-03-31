/*
 * Copyright 2007 David Gibson, IBM Corporation.
 * Based on earlier work, Copyright (C) Paul Mackerras 1997.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <stddef.h>
#include "string.h"
#include "stdio.h"
#include "ops.h"
#include "gunzip_util.h"

#define HEAD_CRC	2
#define EXTRA_FIELD	4
#define ORIG_NAME	8
#define COMMENT		0x10
#define RESERVED	0xe0

void gunzip_start(struct gunzip_state *state, void *src, int srclen)
{
	char *hdr = src;
	int hdrlen = 0;

	memset(state, 0, sizeof(*state));

	
	if ((hdr[0] == 0x1f) && (hdr[1] == 0x8b)) {
		
		int r, flags;

		state->s.workspace = state->scratch;
		if (zlib_inflate_workspacesize() > sizeof(state->scratch))
			fatal("insufficient scratch space for gunzip\n\r");

		
		hdrlen = 10;
		flags = hdr[3];
		if (hdr[2] != Z_DEFLATED || (flags & RESERVED) != 0)
			fatal("bad gzipped data\n\r");
		if ((flags & EXTRA_FIELD) != 0)
			hdrlen = 12 + hdr[10] + (hdr[11] << 8);
		if ((flags & ORIG_NAME) != 0)
			while (hdr[hdrlen++] != 0)
				;
		if ((flags & COMMENT) != 0)
			while (hdr[hdrlen++] != 0)
				;
		if ((flags & HEAD_CRC) != 0)
			hdrlen += 2;
		if (hdrlen >= srclen)
			fatal("gunzip_start: ran out of data in header\n\r");

		r = zlib_inflateInit2(&state->s, -MAX_WBITS);
		if (r != Z_OK)
			fatal("inflateInit2 returned %d\n\r", r);
	}

	state->s.total_in = hdrlen;
	state->s.next_in = src + hdrlen;
	state->s.avail_in = srclen - hdrlen;
}

int gunzip_partial(struct gunzip_state *state, void *dst, int dstlen)
{
	int len;

	if (state->s.workspace) {
		
		int r;

		state->s.next_out = dst;
		state->s.avail_out = dstlen;
		r = zlib_inflate(&state->s, Z_FULL_FLUSH);
		if (r != Z_OK && r != Z_STREAM_END)
			fatal("inflate returned %d msg: %s\n\r", r, state->s.msg);
		len = state->s.next_out - (unsigned char *)dst;
	} else {
		
		len = min(state->s.avail_in, (unsigned)dstlen);
		memcpy(dst, state->s.next_in, len);
		state->s.next_in += len;
		state->s.avail_in -= len;
	}
	return len;
}

void gunzip_exactly(struct gunzip_state *state, void *dst, int dstlen)
{
	int len;

	len  = gunzip_partial(state, dst, dstlen);
	if (len < dstlen)
		fatal("\n\rgunzip_exactly: ran out of data!"
				" Wanted %d, got %d.\n\r", dstlen, len);
}

void gunzip_discard(struct gunzip_state *state, int len)
{
	static char discard_buf[128];

	while (len > sizeof(discard_buf)) {
		gunzip_exactly(state, discard_buf, sizeof(discard_buf));
		len -= sizeof(discard_buf);
	}

	if (len > 0)
		gunzip_exactly(state, discard_buf, len);
}

int gunzip_finish(struct gunzip_state *state, void *dst, int dstlen)
{
	int len;

	len = gunzip_partial(state, dst, dstlen);

	if (state->s.workspace) {
		zlib_inflateEnd(&state->s);
	}

	return len;
}
