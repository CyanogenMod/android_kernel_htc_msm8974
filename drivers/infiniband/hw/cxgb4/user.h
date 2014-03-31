/*
 * Copyright (c) 2009-2010 Chelsio, Inc. All rights reserved.
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
#ifndef __C4IW_USER_H__
#define __C4IW_USER_H__

#define C4IW_UVERBS_ABI_VERSION	1

struct c4iw_create_cq_resp {
	__u64 key;
	__u64 gts_key;
	__u64 memsize;
	__u32 cqid;
	__u32 size;
	__u32 qid_mask;
};


enum {
	C4IW_QPF_ONCHIP = (1<<0)
};

struct c4iw_create_qp_resp {
	__u64 ma_sync_key;
	__u64 sq_key;
	__u64 rq_key;
	__u64 sq_db_gts_key;
	__u64 rq_db_gts_key;
	__u64 sq_memsize;
	__u64 rq_memsize;
	__u32 sqid;
	__u32 rqid;
	__u32 sq_size;
	__u32 rq_size;
	__u32 qid_mask;
	__u32 flags;
};
#endif
