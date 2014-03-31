/*
 * Copyright 2008 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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
#ifndef _FNIC_IO_H_
#define _FNIC_IO_H_

#include <scsi/fc/fc_fcp.h>

#define FNIC_DFLT_SG_DESC_CNT  32
#define FNIC_MAX_SG_DESC_CNT        1024    
#define FNIC_SG_DESC_ALIGN          16      

struct host_sg_desc {
	__le64 addr;
	__le32 len;
	u32 _resvd;
};

struct fnic_dflt_sgl_list {
	struct host_sg_desc sg_desc[FNIC_DFLT_SG_DESC_CNT];
};

struct fnic_sgl_list {
	struct host_sg_desc sg_desc[FNIC_MAX_SG_DESC_CNT];
};

enum fnic_sgl_list_type {
	FNIC_SGL_CACHE_DFLT = 0,  
	FNIC_SGL_CACHE_MAX,       
	FNIC_SGL_NUM_CACHES       
};

enum fnic_ioreq_state {
	FNIC_IOREQ_CMD_PENDING = 0,
	FNIC_IOREQ_ABTS_PENDING,
	FNIC_IOREQ_ABTS_COMPLETE,
	FNIC_IOREQ_CMD_COMPLETE,
};

struct fnic_io_req {
	struct host_sg_desc *sgl_list; 
	void *sgl_list_alloc; 
	dma_addr_t sense_buf_pa; 
	dma_addr_t sgl_list_pa;	
	u16 sgl_cnt;
	u8 sgl_type; 
	u8 io_completed:1; 
	u32 port_id; 
	struct completion *abts_done; 
	struct completion *dr_done; 
};

#endif 
