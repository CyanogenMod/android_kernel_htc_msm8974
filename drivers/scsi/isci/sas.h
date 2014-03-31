/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SCI_SAS_H_
#define _SCI_SAS_H_

#include <linux/kernel.h>

#define FIS_REGH2D          0x27
#define FIS_REGD2H          0x34
#define FIS_SETDEVBITS      0xA1
#define FIS_DMA_ACTIVATE    0x39
#define FIS_DMA_SETUP       0x41
#define FIS_BIST_ACTIVATE   0x58
#define FIS_PIO_SETUP       0x5F
#define FIS_DATA            0x46

#define SSP_RESP_IU_MAX_SIZE	280

struct ssp_cmd_iu {
	u8 LUN[8];
	u8 add_cdb_len:6;
	u8 _r_a:2;
	u8 _r_b;
	u8 en_fburst:1;
	u8 task_prio:4;
	u8 task_attr:3;
	u8 _r_c;

	u8 cdb[16];
}  __packed;

struct ssp_task_iu {
	u8 LUN[8];
	u8 _r_a;
	u8 task_func;
	u8 _r_b[4];
	u16 task_tag;
	u8 _r_c[12];
}  __packed;


struct smp_req_phy_id {
	u8 _r_a[4];		

	u8 ign_zone_grp:1;	
	u8 _r_b:7;

	u8 phy_id;		
	u8 _r_c;		
	u8 _r_d;		
}  __packed;

struct smp_req_conf_rtinfo {
	u16 exp_change_cnt;		
	u8 exp_rt_idx_hi;		
	u8 exp_rt_idx;			

	u8 _r_a;			
	u8 phy_id;			
	u16 _r_b;			

	u8 _r_c:7;			
	u8 dis_rt_entry:1;
	u8 _r_d[3];			

	u8 rt_sas_addr[8];		
	u8 _r_e[16];			
}  __packed;

struct smp_req_phycntl {
	u16 exp_change_cnt;		

	u8 _r_a[3];			

	u8 phy_id;			
	u8 phy_op;			

	u8 upd_pathway:1;		
	u8 _r_b:7;

	u8 _r_c[12];			

	u8 att_dev_name[8];             

	u8 _r_d:4;			
	u8 min_linkrate:4;

	u8 _r_e:4;			
	u8 max_linkrate:4;

	u8 _r_f[2];			

	u8 pathway:4;			
	u8 _r_g:4;

	u8 _r_h[3];			
}  __packed;

struct smp_req {
	u8 type;		
	u8 func;		
	u8 alloc_resp_len;	
	u8 req_len;		
	u8 req_data[0];
}  __packed;

struct sci_sas_address {
	u32 high;
	u32 low;
};
#endif
