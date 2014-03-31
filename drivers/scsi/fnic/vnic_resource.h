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
#ifndef _VNIC_RESOURCE_H_
#define _VNIC_RESOURCE_H_

#define VNIC_RES_MAGIC		0x766E6963L	
#define VNIC_RES_VERSION	0x00000000L

enum vnic_res_type {
	RES_TYPE_EOL,			
	RES_TYPE_WQ,			
	RES_TYPE_RQ,			
	RES_TYPE_CQ,			
	RES_TYPE_RSVD1,
	RES_TYPE_NIC_CFG,		
	RES_TYPE_RSVD2,
	RES_TYPE_RSVD3,
	RES_TYPE_RSVD4,
	RES_TYPE_RSVD5,
	RES_TYPE_INTR_CTRL,		
	RES_TYPE_INTR_TABLE,		
	RES_TYPE_INTR_PBA,		
	RES_TYPE_INTR_PBA_LEGACY,	
	RES_TYPE_RSVD6,
	RES_TYPE_RSVD7,
	RES_TYPE_DEVCMD,		
	RES_TYPE_PASS_THRU_PAGE,	

	RES_TYPE_MAX,			
};

struct vnic_resource_header {
	u32 magic;
	u32 version;
};

struct vnic_resource {
	u8 type;
	u8 bar;
	u8 pad[2];
	u32 bar_offset;
	u32 count;
};

#endif 
