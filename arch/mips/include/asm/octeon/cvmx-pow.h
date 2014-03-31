/***********************license start***************
 * Author: Cavium Networks
 *
 * Contact: support@caviumnetworks.com
 * This file is part of the OCTEON SDK
 *
 * Copyright (c) 2003-2008 Cavium Networks
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, Version 2, as
 * published by the Free Software Foundation.
 *
 * This file is distributed in the hope that it will be useful, but
 * AS-IS and WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, TITLE, or
 * NONINFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 * or visit http://www.gnu.org/licenses/.
 *
 * This file may also be available under a different license from Cavium.
 * Contact Cavium Networks for more information
 ***********************license end**************************************/


#ifndef __CVMX_POW_H__
#define __CVMX_POW_H__

#include <asm/octeon/cvmx-pow-defs.h>

#include "cvmx-scratch.h"
#include "cvmx-wqe.h"

#ifndef CVMX_ENABLE_POW_CHECKS
#define CVMX_ENABLE_POW_CHECKS 1
#endif

enum cvmx_pow_tag_type {
	
	CVMX_POW_TAG_TYPE_ORDERED   = 0L,
	
	CVMX_POW_TAG_TYPE_ATOMIC    = 1L,
	CVMX_POW_TAG_TYPE_NULL      = 2L,
	CVMX_POW_TAG_TYPE_NULL_NULL = 3L
};

typedef enum {
	CVMX_POW_WAIT = 1,
	CVMX_POW_NO_WAIT = 0,
} cvmx_pow_wait_t;

typedef enum {
	CVMX_POW_TAG_OP_SWTAG = 0L,
	CVMX_POW_TAG_OP_SWTAG_FULL = 1L,
	CVMX_POW_TAG_OP_SWTAG_DESCH = 2L,
	CVMX_POW_TAG_OP_DESCH = 3L,
	CVMX_POW_TAG_OP_ADDWQ = 4L,
	CVMX_POW_TAG_OP_UPDATE_WQP_GRP = 5L,
	CVMX_POW_TAG_OP_SET_NSCHED = 6L,
	CVMX_POW_TAG_OP_CLR_NSCHED = 7L,
	
	CVMX_POW_TAG_OP_NOP = 15L
} cvmx_pow_tag_op_t;

typedef union {
	uint64_t u64;
	struct {
		uint64_t no_sched:1;
		uint64_t unused:2;
		
		uint64_t index:13;
		
		cvmx_pow_tag_op_t op:4;
		uint64_t unused2:2;
		uint64_t qos:3;
		uint64_t grp:4;
		uint64_t type:3;
		uint64_t tag:32;
	} s;
} cvmx_pow_tag_req_t;

typedef union {
	uint64_t u64;

	struct {
		
		uint64_t mem_region:2;
		
		uint64_t reserved_49_61:13;
		
		uint64_t is_io:1;
		
		uint64_t did:8;
		
		uint64_t reserved_4_39:36;
		uint64_t wait:1;
		
		uint64_t reserved_0_2:3;
	} swork;

	struct {
		
		uint64_t mem_region:2;
		
		uint64_t reserved_49_61:13;
		
		uint64_t is_io:1;
		
		uint64_t did:8;
		
		uint64_t reserved_10_39:30;
		
		uint64_t coreid:4;
		uint64_t get_rev:1;
		uint64_t get_cur:1;
		uint64_t get_wqp:1;
		
		uint64_t reserved_0_2:3;
	} sstatus;

	struct {
		
		uint64_t mem_region:2;
		
		uint64_t reserved_49_61:13;
		
		uint64_t is_io:1;
		
		uint64_t did:8;
		
		uint64_t reserved_16_39:24;
		
		uint64_t index:11;
		uint64_t get_des:1;
		uint64_t get_wqp:1;
		
		uint64_t reserved_0_2:3;
	} smemload;

	struct {
		
		uint64_t mem_region:2;
		
		uint64_t reserved_49_61:13;
		
		uint64_t is_io:1;
		
		uint64_t did:8;
		
		uint64_t reserved_9_39:31;
		uint64_t qosgrp:4;
		uint64_t get_des_get_tail:1;
		uint64_t get_rmt:1;
		
		uint64_t reserved_0_2:3;
	} sindexload;

	struct {
		
		uint64_t mem_region:2;
		
		uint64_t reserved_49_61:13;
		
		uint64_t is_io:1;
		
		uint64_t did:8;
		
		uint64_t reserved_0_39:40;
	} snull_rd;
} cvmx_pow_load_addr_t;

typedef union {
	uint64_t u64;

	struct {
		uint64_t no_work:1;
		
		uint64_t reserved_40_62:23;
		
		uint64_t addr:40;
	} s_work;

	struct {
		uint64_t reserved_62_63:2;
		uint64_t pend_switch:1;
		
		uint64_t pend_switch_full:1;
		uint64_t pend_switch_null:1;
		
		uint64_t pend_desched:1;
		uint64_t pend_desched_switch:1;
		
		uint64_t pend_nosched:1;
		
		uint64_t pend_new_work:1;
		uint64_t pend_new_work_wait:1;
		
		uint64_t pend_null_rd:1;
		
		uint64_t pend_nosched_clr:1;
		uint64_t reserved_51:1;
		
		uint64_t pend_index:11;
		uint64_t pend_grp:4;
		uint64_t reserved_34_35:2;
		uint64_t pend_type:2;
		uint64_t pend_tag:32;
	} s_sstatus0;

	struct {
		uint64_t reserved_62_63:2;
		uint64_t pend_switch:1;
		
		uint64_t pend_switch_full:1;
		uint64_t pend_switch_null:1;
		uint64_t pend_desched:1;
		uint64_t pend_desched_switch:1;
		
		uint64_t pend_nosched:1;
		
		uint64_t pend_new_work:1;
		uint64_t pend_new_work_wait:1;
		
		uint64_t pend_null_rd:1;
		
		uint64_t pend_nosched_clr:1;
		uint64_t reserved_51:1;
		
		uint64_t pend_index:11;
		uint64_t pend_grp:4;
		
		uint64_t pend_wqp:36;
	} s_sstatus1;

	struct {
		uint64_t reserved_62_63:2;
		uint64_t link_index:11;
		
		uint64_t index:11;
		uint64_t grp:4;
		uint64_t head:1;
		uint64_t tail:1;
		uint64_t tag_type:2;
		uint64_t tag:32;
	} s_sstatus2;

	struct {
		uint64_t reserved_62_63:2;
		uint64_t revlink_index:11;
		
		uint64_t index:11;
		uint64_t grp:4;
		uint64_t head:1;
		uint64_t tail:1;
		uint64_t tag_type:2;
		uint64_t tag:32;
	} s_sstatus3;

	struct {
		uint64_t reserved_62_63:2;
		uint64_t link_index:11;
		
		uint64_t index:11;
		uint64_t grp:4;
		uint64_t wqp:36;
	} s_sstatus4;

	struct {
		uint64_t reserved_62_63:2;
		uint64_t revlink_index:11;
		
		uint64_t index:11;
		uint64_t grp:4;
		uint64_t wqp:36;
	} s_sstatus5;

	struct {
		uint64_t reserved_51_63:13;
		uint64_t next_index:11;
		
		uint64_t grp:4;
		uint64_t reserved_35:1;
		uint64_t tail:1;
		
		uint64_t tag_type:2;
		
		uint64_t tag:32;
	} s_smemload0;

	struct {
		uint64_t reserved_51_63:13;
		uint64_t next_index:11;
		
		uint64_t grp:4;
		
		uint64_t wqp:36;
	} s_smemload1;

	struct {
		uint64_t reserved_51_63:13;
		uint64_t fwd_index:11;
		
		uint64_t grp:4;
		
		uint64_t nosched:1;
		
		uint64_t pend_switch:1;
		uint64_t pend_type:2;
		uint64_t pend_tag:32;
	} s_smemload2;

	struct {
		uint64_t reserved_52_63:12;
		uint64_t free_val:1;
		uint64_t free_one:1;
		uint64_t reserved_49:1;
		uint64_t free_head:11;
		uint64_t reserved_37:1;
		uint64_t free_tail:11;
		uint64_t loc_val:1;
		uint64_t loc_one:1;
		uint64_t reserved_23:1;
		uint64_t loc_head:11;
		uint64_t reserved_11:1;
		uint64_t loc_tail:11;
	} sindexload0;

	struct {
		uint64_t reserved_52_63:12;
		uint64_t nosched_val:1;
		uint64_t nosched_one:1;
		uint64_t reserved_49:1;
		uint64_t nosched_head:11;
		uint64_t reserved_37:1;
		uint64_t nosched_tail:11;
		uint64_t des_val:1;
		uint64_t des_one:1;
		uint64_t reserved_23:1;
		uint64_t des_head:11;
		uint64_t reserved_11:1;
		uint64_t des_tail:11;
	} sindexload1;

	struct {
		uint64_t reserved_39_63:25;
		uint64_t rmt_is_head:1;
		uint64_t rmt_val:1;
		uint64_t rmt_one:1;
		uint64_t rmt_head:36;
	} sindexload2;

	struct {
		uint64_t reserved_39_63:25;
		uint64_t rmt_is_head:1;
		uint64_t rmt_val:1;
		uint64_t rmt_one:1;
		uint64_t rmt_tail:36;
	} sindexload3;

	struct {
		uint64_t unused:62;
		uint64_t state:2;
	} s_null_rd;

} cvmx_pow_tag_load_resp_t;

typedef union {
	uint64_t u64;

	struct {
		
		uint64_t mem_reg:2;
		uint64_t reserved_49_61:13;	
		uint64_t is_io:1;	
		
		uint64_t did:8;
		uint64_t reserved_36_39:4;	
		
		uint64_t addr:36;
	} stag;
} cvmx_pow_tag_store_addr_t;

typedef union {
	uint64_t u64;

	struct {
		uint64_t scraddr:8;
		
		uint64_t len:8;
		
		uint64_t did:8;
		uint64_t unused:36;
		
		uint64_t wait:1;
		uint64_t unused2:3;
	} s;

} cvmx_pow_iobdma_store_t;


static inline cvmx_pow_tag_req_t cvmx_pow_get_current_tag(void)
{
	cvmx_pow_load_addr_t load_addr;
	cvmx_pow_tag_load_resp_t load_resp;
	cvmx_pow_tag_req_t result;

	load_addr.u64 = 0;
	load_addr.sstatus.mem_region = CVMX_IO_SEG;
	load_addr.sstatus.is_io = 1;
	load_addr.sstatus.did = CVMX_OCT_DID_TAG_TAG1;
	load_addr.sstatus.coreid = cvmx_get_core_num();
	load_addr.sstatus.get_cur = 1;
	load_resp.u64 = cvmx_read_csr(load_addr.u64);
	result.u64 = 0;
	result.s.grp = load_resp.s_sstatus2.grp;
	result.s.index = load_resp.s_sstatus2.index;
	result.s.type = load_resp.s_sstatus2.tag_type;
	result.s.tag = load_resp.s_sstatus2.tag;
	return result;
}

static inline cvmx_wqe_t *cvmx_pow_get_current_wqp(void)
{
	cvmx_pow_load_addr_t load_addr;
	cvmx_pow_tag_load_resp_t load_resp;

	load_addr.u64 = 0;
	load_addr.sstatus.mem_region = CVMX_IO_SEG;
	load_addr.sstatus.is_io = 1;
	load_addr.sstatus.did = CVMX_OCT_DID_TAG_TAG1;
	load_addr.sstatus.coreid = cvmx_get_core_num();
	load_addr.sstatus.get_cur = 1;
	load_addr.sstatus.get_wqp = 1;
	load_resp.u64 = cvmx_read_csr(load_addr.u64);
	return (cvmx_wqe_t *) cvmx_phys_to_ptr(load_resp.s_sstatus4.wqp);
}

#ifndef CVMX_MF_CHORD
#define CVMX_MF_CHORD(dest)         CVMX_RDHWR(dest, 30)
#endif

static inline void __cvmx_pow_warn_if_pending_switch(const char *function)
{
	uint64_t switch_complete;
	CVMX_MF_CHORD(switch_complete);
	if (!switch_complete)
		pr_warning("%s called with tag switch in progress\n", function);
}

static inline void cvmx_pow_tag_sw_wait(void)
{
	const uint64_t MAX_CYCLES = 1ull << 31;
	uint64_t switch_complete;
	uint64_t start_cycle = cvmx_get_cycle();
	while (1) {
		CVMX_MF_CHORD(switch_complete);
		if (unlikely(switch_complete))
			break;
		if (unlikely(cvmx_get_cycle() > start_cycle + MAX_CYCLES)) {
			pr_warning("Tag switch is taking a long time, "
				   "possible deadlock\n");
			start_cycle = -MAX_CYCLES - 1;
		}
	}
}

static inline cvmx_wqe_t *cvmx_pow_work_request_sync_nocheck(cvmx_pow_wait_t
							     wait)
{
	cvmx_pow_load_addr_t ptr;
	cvmx_pow_tag_load_resp_t result;

	if (CVMX_ENABLE_POW_CHECKS)
		__cvmx_pow_warn_if_pending_switch(__func__);

	ptr.u64 = 0;
	ptr.swork.mem_region = CVMX_IO_SEG;
	ptr.swork.is_io = 1;
	ptr.swork.did = CVMX_OCT_DID_TAG_SWTAG;
	ptr.swork.wait = wait;

	result.u64 = cvmx_read_csr(ptr.u64);

	if (result.s_work.no_work)
		return NULL;
	else
		return (cvmx_wqe_t *) cvmx_phys_to_ptr(result.s_work.addr);
}

static inline cvmx_wqe_t *cvmx_pow_work_request_sync(cvmx_pow_wait_t wait)
{
	if (CVMX_ENABLE_POW_CHECKS)
		__cvmx_pow_warn_if_pending_switch(__func__);

	
	cvmx_pow_tag_sw_wait();
	return cvmx_pow_work_request_sync_nocheck(wait);

}

static inline enum cvmx_pow_tag_type cvmx_pow_work_request_null_rd(void)
{
	cvmx_pow_load_addr_t ptr;
	cvmx_pow_tag_load_resp_t result;

	if (CVMX_ENABLE_POW_CHECKS)
		__cvmx_pow_warn_if_pending_switch(__func__);

	
	cvmx_pow_tag_sw_wait();

	ptr.u64 = 0;
	ptr.snull_rd.mem_region = CVMX_IO_SEG;
	ptr.snull_rd.is_io = 1;
	ptr.snull_rd.did = CVMX_OCT_DID_TAG_NULL_RD;

	result.u64 = cvmx_read_csr(ptr.u64);

	return (enum cvmx_pow_tag_type) result.s_null_rd.state;
}

static inline void cvmx_pow_work_request_async_nocheck(int scr_addr,
						       cvmx_pow_wait_t wait)
{
	cvmx_pow_iobdma_store_t data;

	if (CVMX_ENABLE_POW_CHECKS)
		__cvmx_pow_warn_if_pending_switch(__func__);

	
	data.s.scraddr = scr_addr >> 3;
	data.s.len = 1;
	data.s.did = CVMX_OCT_DID_TAG_SWTAG;
	data.s.wait = wait;
	cvmx_send_single(data.u64);
}

static inline void cvmx_pow_work_request_async(int scr_addr,
					       cvmx_pow_wait_t wait)
{
	if (CVMX_ENABLE_POW_CHECKS)
		__cvmx_pow_warn_if_pending_switch(__func__);

	
	cvmx_pow_tag_sw_wait();
	cvmx_pow_work_request_async_nocheck(scr_addr, wait);
}

static inline cvmx_wqe_t *cvmx_pow_work_response_async(int scr_addr)
{
	cvmx_pow_tag_load_resp_t result;

	CVMX_SYNCIOBDMA;
	result.u64 = cvmx_scratch_read64(scr_addr);

	if (result.s_work.no_work)
		return NULL;
	else
		return (cvmx_wqe_t *) cvmx_phys_to_ptr(result.s_work.addr);
}

static inline uint64_t cvmx_pow_work_invalid(cvmx_wqe_t *wqe_ptr)
{
	return wqe_ptr == NULL;
}

static inline void cvmx_pow_tag_sw_nocheck(uint32_t tag,
					   enum cvmx_pow_tag_type tag_type)
{
	cvmx_addr_t ptr;
	cvmx_pow_tag_req_t tag_req;

	if (CVMX_ENABLE_POW_CHECKS) {
		cvmx_pow_tag_req_t current_tag;
		__cvmx_pow_warn_if_pending_switch(__func__);
		current_tag = cvmx_pow_get_current_tag();
		if (current_tag.s.type == CVMX_POW_TAG_TYPE_NULL_NULL)
			pr_warning("%s called with NULL_NULL tag\n",
				   __func__);
		if (current_tag.s.type == CVMX_POW_TAG_TYPE_NULL)
			pr_warning("%s called with NULL tag\n", __func__);
		if ((current_tag.s.type == tag_type)
		   && (current_tag.s.tag == tag))
			pr_warning("%s called to perform a tag switch to the "
				   "same tag\n",
			     __func__);
		if (tag_type == CVMX_POW_TAG_TYPE_NULL)
			pr_warning("%s called to perform a tag switch to "
				   "NULL. Use cvmx_pow_tag_sw_null() instead\n",
			     __func__);
	}


	tag_req.u64 = 0;
	tag_req.s.op = CVMX_POW_TAG_OP_SWTAG;
	tag_req.s.tag = tag;
	tag_req.s.type = tag_type;

	ptr.u64 = 0;
	ptr.sio.mem_region = CVMX_IO_SEG;
	ptr.sio.is_io = 1;
	ptr.sio.did = CVMX_OCT_DID_TAG_SWTAG;

	cvmx_write_io(ptr.u64, tag_req.u64);
}

static inline void cvmx_pow_tag_sw(uint32_t tag,
				   enum cvmx_pow_tag_type tag_type)
{
	if (CVMX_ENABLE_POW_CHECKS)
		__cvmx_pow_warn_if_pending_switch(__func__);


	cvmx_pow_tag_sw_wait();
	cvmx_pow_tag_sw_nocheck(tag, tag_type);
}

static inline void cvmx_pow_tag_sw_full_nocheck(cvmx_wqe_t *wqp, uint32_t tag,
						enum cvmx_pow_tag_type tag_type,
						uint64_t group)
{
	cvmx_addr_t ptr;
	cvmx_pow_tag_req_t tag_req;

	if (CVMX_ENABLE_POW_CHECKS) {
		cvmx_pow_tag_req_t current_tag;
		__cvmx_pow_warn_if_pending_switch(__func__);
		current_tag = cvmx_pow_get_current_tag();
		if (current_tag.s.type == CVMX_POW_TAG_TYPE_NULL_NULL)
			pr_warning("%s called with NULL_NULL tag\n",
				   __func__);
		if ((current_tag.s.type == tag_type)
		   && (current_tag.s.tag == tag))
			pr_warning("%s called to perform a tag switch to "
				   "the same tag\n",
			     __func__);
		if (tag_type == CVMX_POW_TAG_TYPE_NULL)
			pr_warning("%s called to perform a tag switch to "
				   "NULL. Use cvmx_pow_tag_sw_null() instead\n",
			     __func__);
		if (wqp != cvmx_phys_to_ptr(0x80))
			if (wqp != cvmx_pow_get_current_wqp())
				pr_warning("%s passed WQE(%p) doesn't match "
					   "the address in the POW(%p)\n",
				     __func__, wqp,
				     cvmx_pow_get_current_wqp());
	}


	tag_req.u64 = 0;
	tag_req.s.op = CVMX_POW_TAG_OP_SWTAG_FULL;
	tag_req.s.tag = tag;
	tag_req.s.type = tag_type;
	tag_req.s.grp = group;

	ptr.u64 = 0;
	ptr.sio.mem_region = CVMX_IO_SEG;
	ptr.sio.is_io = 1;
	ptr.sio.did = CVMX_OCT_DID_TAG_SWTAG;
	ptr.sio.offset = CAST64(wqp);

	cvmx_write_io(ptr.u64, tag_req.u64);
}

static inline void cvmx_pow_tag_sw_full(cvmx_wqe_t *wqp, uint32_t tag,
					enum cvmx_pow_tag_type tag_type,
					uint64_t group)
{
	if (CVMX_ENABLE_POW_CHECKS)
		__cvmx_pow_warn_if_pending_switch(__func__);

	cvmx_pow_tag_sw_wait();
	cvmx_pow_tag_sw_full_nocheck(wqp, tag, tag_type, group);
}

static inline void cvmx_pow_tag_sw_null_nocheck(void)
{
	cvmx_addr_t ptr;
	cvmx_pow_tag_req_t tag_req;

	if (CVMX_ENABLE_POW_CHECKS) {
		cvmx_pow_tag_req_t current_tag;
		__cvmx_pow_warn_if_pending_switch(__func__);
		current_tag = cvmx_pow_get_current_tag();
		if (current_tag.s.type == CVMX_POW_TAG_TYPE_NULL_NULL)
			pr_warning("%s called with NULL_NULL tag\n",
				   __func__);
		if (current_tag.s.type == CVMX_POW_TAG_TYPE_NULL)
			pr_warning("%s called when we already have a "
				   "NULL tag\n",
			     __func__);
	}

	tag_req.u64 = 0;
	tag_req.s.op = CVMX_POW_TAG_OP_SWTAG;
	tag_req.s.type = CVMX_POW_TAG_TYPE_NULL;

	ptr.u64 = 0;
	ptr.sio.mem_region = CVMX_IO_SEG;
	ptr.sio.is_io = 1;
	ptr.sio.did = CVMX_OCT_DID_TAG_TAG1;

	cvmx_write_io(ptr.u64, tag_req.u64);

	
}

static inline void cvmx_pow_tag_sw_null(void)
{
	if (CVMX_ENABLE_POW_CHECKS)
		__cvmx_pow_warn_if_pending_switch(__func__);

	cvmx_pow_tag_sw_wait();
	cvmx_pow_tag_sw_null_nocheck();

	
}

static inline void cvmx_pow_work_submit(cvmx_wqe_t *wqp, uint32_t tag,
					enum cvmx_pow_tag_type tag_type,
					uint64_t qos, uint64_t grp)
{
	cvmx_addr_t ptr;
	cvmx_pow_tag_req_t tag_req;

	wqp->qos = qos;
	wqp->tag = tag;
	wqp->tag_type = tag_type;
	wqp->grp = grp;

	tag_req.u64 = 0;
	tag_req.s.op = CVMX_POW_TAG_OP_ADDWQ;
	tag_req.s.type = tag_type;
	tag_req.s.tag = tag;
	tag_req.s.qos = qos;
	tag_req.s.grp = grp;

	ptr.u64 = 0;
	ptr.sio.mem_region = CVMX_IO_SEG;
	ptr.sio.is_io = 1;
	ptr.sio.did = CVMX_OCT_DID_TAG_TAG1;
	ptr.sio.offset = cvmx_ptr_to_phys(wqp);

	CVMX_SYNCWS;
	cvmx_write_io(ptr.u64, tag_req.u64);
}

static inline void cvmx_pow_set_group_mask(uint64_t core_num, uint64_t mask)
{
	union cvmx_pow_pp_grp_mskx grp_msk;

	grp_msk.u64 = cvmx_read_csr(CVMX_POW_PP_GRP_MSKX(core_num));
	grp_msk.s.grp_msk = mask;
	cvmx_write_csr(CVMX_POW_PP_GRP_MSKX(core_num), grp_msk.u64);
}

static inline void cvmx_pow_set_priority(uint64_t core_num,
					 const uint8_t priority[])
{
	
	if (!OCTEON_IS_MODEL(OCTEON_CN3XXX)) {
		union cvmx_pow_pp_grp_mskx grp_msk;

		grp_msk.u64 = cvmx_read_csr(CVMX_POW_PP_GRP_MSKX(core_num));
		grp_msk.s.qos0_pri = priority[0];
		grp_msk.s.qos1_pri = priority[1];
		grp_msk.s.qos2_pri = priority[2];
		grp_msk.s.qos3_pri = priority[3];
		grp_msk.s.qos4_pri = priority[4];
		grp_msk.s.qos5_pri = priority[5];
		grp_msk.s.qos6_pri = priority[6];
		grp_msk.s.qos7_pri = priority[7];

		
		{
			int i;
			uint32_t prio_mask = 0;

			for (i = 0; i < 8; i++)
				if (priority[i] != 0xF)
					prio_mask |= 1 << priority[i];

			if (prio_mask ^ ((1 << cvmx_pop(prio_mask)) - 1)) {
				pr_err("POW static priorities should be "
				       "contiguous (0x%llx)\n",
				     (unsigned long long)prio_mask);
				return;
			}
		}

		cvmx_write_csr(CVMX_POW_PP_GRP_MSKX(core_num), grp_msk.u64);
	}
}

static inline void cvmx_pow_tag_sw_desched_nocheck(
	uint32_t tag,
	enum cvmx_pow_tag_type tag_type,
	uint64_t group,
	uint64_t no_sched)
{
	cvmx_addr_t ptr;
	cvmx_pow_tag_req_t tag_req;

	if (CVMX_ENABLE_POW_CHECKS) {
		cvmx_pow_tag_req_t current_tag;
		__cvmx_pow_warn_if_pending_switch(__func__);
		current_tag = cvmx_pow_get_current_tag();
		if (current_tag.s.type == CVMX_POW_TAG_TYPE_NULL_NULL)
			pr_warning("%s called with NULL_NULL tag\n",
				   __func__);
		if (current_tag.s.type == CVMX_POW_TAG_TYPE_NULL)
			pr_warning("%s called with NULL tag. Deschedule not "
				   "allowed from NULL state\n",
			     __func__);
		if ((current_tag.s.type != CVMX_POW_TAG_TYPE_ATOMIC)
			&& (tag_type != CVMX_POW_TAG_TYPE_ATOMIC))
			pr_warning("%s called where neither the before or "
				   "after tag is ATOMIC\n",
			     __func__);
	}

	tag_req.u64 = 0;
	tag_req.s.op = CVMX_POW_TAG_OP_SWTAG_DESCH;
	tag_req.s.tag = tag;
	tag_req.s.type = tag_type;
	tag_req.s.grp = group;
	tag_req.s.no_sched = no_sched;

	ptr.u64 = 0;
	ptr.sio.mem_region = CVMX_IO_SEG;
	ptr.sio.is_io = 1;
	ptr.sio.did = CVMX_OCT_DID_TAG_TAG3;
	cvmx_write_io(ptr.u64, tag_req.u64);
}

static inline void cvmx_pow_tag_sw_desched(uint32_t tag,
					   enum cvmx_pow_tag_type tag_type,
					   uint64_t group, uint64_t no_sched)
{
	if (CVMX_ENABLE_POW_CHECKS)
		__cvmx_pow_warn_if_pending_switch(__func__);

	
	CVMX_SYNCWS;
	cvmx_pow_tag_sw_wait();
	cvmx_pow_tag_sw_desched_nocheck(tag, tag_type, group, no_sched);
}

static inline void cvmx_pow_desched(uint64_t no_sched)
{
	cvmx_addr_t ptr;
	cvmx_pow_tag_req_t tag_req;

	if (CVMX_ENABLE_POW_CHECKS) {
		cvmx_pow_tag_req_t current_tag;
		__cvmx_pow_warn_if_pending_switch(__func__);
		current_tag = cvmx_pow_get_current_tag();
		if (current_tag.s.type == CVMX_POW_TAG_TYPE_NULL_NULL)
			pr_warning("%s called with NULL_NULL tag\n",
				   __func__);
		if (current_tag.s.type == CVMX_POW_TAG_TYPE_NULL)
			pr_warning("%s called with NULL tag. Deschedule not "
				   "expected from NULL state\n",
			     __func__);
	}

	
	CVMX_SYNCWS;

	tag_req.u64 = 0;
	tag_req.s.op = CVMX_POW_TAG_OP_DESCH;
	tag_req.s.no_sched = no_sched;

	ptr.u64 = 0;
	ptr.sio.mem_region = CVMX_IO_SEG;
	ptr.sio.is_io = 1;
	ptr.sio.did = CVMX_OCT_DID_TAG_TAG3;
	cvmx_write_io(ptr.u64, tag_req.u64);
}


#define CVMX_TAG_SW_BITS    (8)
#define CVMX_TAG_SW_SHIFT   (32 - CVMX_TAG_SW_BITS)

#define CVMX_TAG_SW_BITS_INTERNAL  0x1
#define CVMX_TAG_SUBGROUP_MASK  0xFFFF
#define CVMX_TAG_SUBGROUP_SHIFT 16
#define CVMX_TAG_SUBGROUP_PKO  0x1



static inline uint32_t cvmx_pow_tag_compose(uint64_t sw_bits, uint64_t hw_bits)
{
	return ((sw_bits & cvmx_build_mask(CVMX_TAG_SW_BITS)) <<
			CVMX_TAG_SW_SHIFT) |
		(hw_bits & cvmx_build_mask(32 - CVMX_TAG_SW_BITS));
}

static inline uint32_t cvmx_pow_tag_get_sw_bits(uint64_t tag)
{
	return (tag >> (32 - CVMX_TAG_SW_BITS)) &
		cvmx_build_mask(CVMX_TAG_SW_BITS);
}

static inline uint32_t cvmx_pow_tag_get_hw_bits(uint64_t tag)
{
	return tag & cvmx_build_mask(32 - CVMX_TAG_SW_BITS);
}

extern int cvmx_pow_capture(void *buffer, int buffer_size);

extern void cvmx_pow_display(void *buffer, int buffer_size);

extern int cvmx_pow_get_num_entries(void);

#endif 
