/*
 * Copyright (c) 2003-2008 Chelsio, Inc. All rights reserved.
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
#ifndef _CXGB3_OFFLOAD_CTL_DEFS_H
#define _CXGB3_OFFLOAD_CTL_DEFS_H

enum {
	GET_MAX_OUTSTANDING_WR 	= 0,
	GET_TX_MAX_CHUNK	= 1,
	GET_TID_RANGE		= 2,
	GET_STID_RANGE		= 3,
	GET_RTBL_RANGE		= 4,
	GET_L2T_CAPACITY	= 5,
	GET_MTUS		= 6,
	GET_WR_LEN		= 7,
	GET_IFF_FROM_MAC	= 8,
	GET_DDP_PARAMS		= 9,
	GET_PORTS		= 10,

	ULP_ISCSI_GET_PARAMS	= 11,
	ULP_ISCSI_SET_PARAMS	= 12,

	RDMA_GET_PARAMS		= 13,
	RDMA_CQ_OP		= 14,
	RDMA_CQ_SETUP		= 15,
	RDMA_CQ_DISABLE		= 16,
	RDMA_CTRL_QP_SETUP	= 17,
	RDMA_GET_MEM		= 18,
	RDMA_GET_MIB		= 19,

	GET_RX_PAGE_INFO	= 50,
	GET_ISCSI_IPV4ADDR	= 51,

	GET_EMBEDDED_INFO	= 70,
};

struct tid_range {
	unsigned int base;	
	unsigned int num;	
};

struct mtutab {
	unsigned int size;	
	const unsigned short *mtus;	
};

struct net_device;

struct iff_mac {
	struct net_device *dev;	
	const unsigned char *mac_addr;	
	u16 vlan_tag;
};

struct iscsi_ipv4addr {
	struct net_device *dev;	
	__be32 ipv4addr;	
};

struct pci_dev;

struct ddp_params {
	unsigned int llimit;	
	unsigned int ulimit;	
	unsigned int tag_mask;	
	struct pci_dev *pdev;
};

struct adap_ports {
	unsigned int nports;	
	struct net_device *lldevs[2];
};

struct ulp_iscsi_info {
	unsigned int offset;
	unsigned int llimit;
	unsigned int ulimit;
	unsigned int tagmask;
	u8 pgsz_factor[4];
	unsigned int max_rxsz;
	unsigned int max_txsz;
	struct pci_dev *pdev;
};

struct rdma_info {
	unsigned int tpt_base;	
	unsigned int tpt_top;	
	unsigned int pbl_base;	
	unsigned int pbl_top;	
	unsigned int rqt_base;	
	unsigned int rqt_top;	
	unsigned int udbell_len;	
	unsigned long udbell_physbase;	
	void __iomem *kdb_addr;	
	struct pci_dev *pdev;	
};

struct rdma_cq_op {
	unsigned int id;
	unsigned int op;
	unsigned int credits;
};

struct rdma_cq_setup {
	unsigned int id;
	unsigned long long base_addr;
	unsigned int size;
	unsigned int credits;
	unsigned int credit_thres;
	unsigned int ovfl_mode;
};

struct rdma_ctrlqp_setup {
	unsigned long long base_addr;
	unsigned int size;
};

struct ofld_page_info {
	unsigned int page_size;  
	unsigned int num;        
};

struct ch_embedded_info {
	u32 fw_vers;
	u32 tp_vers;
};
#endif				
