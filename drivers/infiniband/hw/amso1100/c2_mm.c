/*
 * Copyright (c) 2005 Ammasso, Inc. All rights reserved.
 * Copyright (c) 2005 Open Grid Computing, Inc. All rights reserved.
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
#include <linux/slab.h>

#include "c2.h"
#include "c2_vq.h"

#define PBL_VIRT 1
#define PBL_PHYS 2

static int
send_pbl_messages(struct c2_dev *c2dev, __be32 stag_index,
		  unsigned long va, u32 pbl_depth,
		  struct c2_vq_req *vq_req, int pbl_type)
{
	u32 pbe_count;		
	u32 count;		
	struct c2wr_nsmr_pbl_req *wr;	
	struct c2wr_nsmr_pbl_rep *reply;	
 	int err, pbl_virt, pbl_index, i;

	switch (pbl_type) {
	case PBL_VIRT:
		pbl_virt = 1;
		break;
	case PBL_PHYS:
		pbl_virt = 0;
		break;
	default:
		return -EINVAL;
		break;
	}

	pbe_count = (c2dev->req_vq.msg_size -
		     sizeof(struct c2wr_nsmr_pbl_req)) / sizeof(u64);
	wr = kmalloc(c2dev->req_vq.msg_size, GFP_KERNEL);
	if (!wr) {
		return -ENOMEM;
	}
	c2_wr_set_id(wr, CCWR_NSMR_PBL);

	wr->hdr.context = 0;
	wr->rnic_handle = c2dev->adapter_handle;
	wr->stag_index = stag_index;	
	wr->flags = 0;
	pbl_index = 0;
	while (pbl_depth) {
		count = min(pbe_count, pbl_depth);
		wr->addrs_length = cpu_to_be32(count);

		if (count == pbl_depth) {
			vq_req_get(c2dev, vq_req);
			wr->flags = cpu_to_be32(MEM_PBL_COMPLETE);

			wr->hdr.context = (unsigned long) vq_req;
		}

		for (i = 0; i < count; i++) {
			if (pbl_virt) {
				va += PAGE_SIZE;
			} else {
 				wr->paddrs[i] =
				    cpu_to_be64(((u64 *)va)[pbl_index + i]);
			}
		}

		err = vq_send_wr(c2dev, (union c2wr *) wr);
		if (err) {
			if (count <= pbe_count) {
				vq_req_put(c2dev, vq_req);
			}
			goto bail0;
		}
		pbl_depth -= count;
		pbl_index += count;
	}

	err = vq_wait_for_reply(c2dev, vq_req);
	if (err) {
		goto bail0;
	}

	reply = (struct c2wr_nsmr_pbl_rep *) (unsigned long) vq_req->reply_msg;
	if (!reply) {
		err = -ENOMEM;
		goto bail0;
	}

	err = c2_errno(reply);

	vq_repbuf_free(c2dev, reply);
      bail0:
	kfree(wr);
	return err;
}

#define C2_PBL_MAX_DEPTH 131072
int
c2_nsmr_register_phys_kern(struct c2_dev *c2dev, u64 *addr_list,
 			   int page_size, int pbl_depth, u32 length,
 			   u32 offset, u64 *va, enum c2_acf acf,
			   struct c2_mr *mr)
{
	struct c2_vq_req *vq_req;
	struct c2wr_nsmr_register_req *wr;
	struct c2wr_nsmr_register_rep *reply;
	u16 flags;
	int i, pbe_count, count;
	int err;

	if (!va || !length || !addr_list || !pbl_depth)
		return -EINTR;

	if (pbl_depth > C2_PBL_MAX_DEPTH) {
		return -EINTR;
	}

	vq_req = vq_req_alloc(c2dev);
	if (!vq_req)
		return -ENOMEM;

	wr = kmalloc(c2dev->req_vq.msg_size, GFP_KERNEL);
	if (!wr) {
		err = -ENOMEM;
		goto bail0;
	}

	c2_wr_set_id(wr, CCWR_NSMR_REGISTER);
	wr->hdr.context = (unsigned long) vq_req;
	wr->rnic_handle = c2dev->adapter_handle;

	flags = (acf | MEM_VA_BASED | MEM_REMOTE);

	pbe_count = (c2dev->req_vq.msg_size -
		     sizeof(struct c2wr_nsmr_register_req)) / sizeof(u64);

	if (pbl_depth <= pbe_count) {
		flags |= MEM_PBL_COMPLETE;
	}
	wr->flags = cpu_to_be16(flags);
	wr->stag_key = 0;	
	wr->va = cpu_to_be64(*va);
	wr->pd_id = mr->pd->pd_id;
	wr->pbe_size = cpu_to_be32(page_size);
	wr->length = cpu_to_be32(length);
	wr->pbl_depth = cpu_to_be32(pbl_depth);
	wr->fbo = cpu_to_be32(offset);
	count = min(pbl_depth, pbe_count);
	wr->addrs_length = cpu_to_be32(count);

	for (i = 0; i < count; i++) {
		wr->paddrs[i] = cpu_to_be64(addr_list[i]);
	}

	vq_req_get(c2dev, vq_req);

	err = vq_send_wr(c2dev, (union c2wr *) wr);
	if (err) {
		vq_req_put(c2dev, vq_req);
		goto bail1;
	}

	err = vq_wait_for_reply(c2dev, vq_req);
	if (err) {
		goto bail1;
	}

	reply =
	    (struct c2wr_nsmr_register_rep *) (unsigned long) (vq_req->reply_msg);
	if (!reply) {
		err = -ENOMEM;
		goto bail1;
	}
	if ((err = c2_errno(reply))) {
		goto bail2;
	}
	
	mr->ibmr.lkey = mr->ibmr.rkey = be32_to_cpu(reply->stag_index);
	vq_repbuf_free(c2dev, reply);

	pbl_depth -= count;
	if (pbl_depth) {

		vq_req->reply_msg = (unsigned long) NULL;
		atomic_set(&vq_req->reply_ready, 0);
		err = send_pbl_messages(c2dev,
					cpu_to_be32(mr->ibmr.lkey),
					(unsigned long) &addr_list[i],
					pbl_depth, vq_req, PBL_PHYS);
		if (err) {
			goto bail1;
		}
	}

	vq_req_free(c2dev, vq_req);
	kfree(wr);

	return err;

      bail2:
	vq_repbuf_free(c2dev, reply);
      bail1:
	kfree(wr);
      bail0:
	vq_req_free(c2dev, vq_req);
	return err;
}

int c2_stag_dealloc(struct c2_dev *c2dev, u32 stag_index)
{
	struct c2_vq_req *vq_req;	
	struct c2wr_stag_dealloc_req wr;	
	struct c2wr_stag_dealloc_rep *reply;	
	int err;


	vq_req = vq_req_alloc(c2dev);
	if (!vq_req) {
		return -ENOMEM;
	}

	c2_wr_set_id(&wr, CCWR_STAG_DEALLOC);
	wr.hdr.context = (u64) (unsigned long) vq_req;
	wr.rnic_handle = c2dev->adapter_handle;
	wr.stag_index = cpu_to_be32(stag_index);

	vq_req_get(c2dev, vq_req);

	err = vq_send_wr(c2dev, (union c2wr *) & wr);
	if (err) {
		vq_req_put(c2dev, vq_req);
		goto bail0;
	}

	err = vq_wait_for_reply(c2dev, vq_req);
	if (err) {
		goto bail0;
	}

	reply = (struct c2wr_stag_dealloc_rep *) (unsigned long) vq_req->reply_msg;
	if (!reply) {
		err = -ENOMEM;
		goto bail0;
	}

	err = c2_errno(reply);

	vq_repbuf_free(c2dev, reply);
      bail0:
	vq_req_free(c2dev, vq_req);
	return err;
}
