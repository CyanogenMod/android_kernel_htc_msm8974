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

#ifndef _BRCMF_PROTO_H_
#define _BRCMF_PROTO_H_


extern int brcmf_proto_attach(struct brcmf_pub *drvr);

extern void brcmf_proto_detach(struct brcmf_pub *drvr);

extern int brcmf_proto_init(struct brcmf_pub *drvr);

extern void brcmf_proto_stop(struct brcmf_pub *drvr);

extern void brcmf_proto_hdrpush(struct brcmf_pub *, int ifidx,
				struct sk_buff *txp);

extern int brcmf_proto_dcmd(struct brcmf_pub *drvr, int ifidx,
				struct brcmf_dcmd *dcmd, int len);

extern int brcmf_c_preinit_dcmds(struct brcmf_pub *drvr);

extern int brcmf_proto_cdc_set_dcmd(struct brcmf_pub *drvr, int ifidx,
				     uint cmd, void *buf, uint len);

#endif				
