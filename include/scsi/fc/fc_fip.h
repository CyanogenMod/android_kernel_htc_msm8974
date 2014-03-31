/*
 * Copyright 2008 Cisco Systems, Inc.  All rights reserved.
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
#ifndef _FC_FIP_H_
#define _FC_FIP_H_

#include <scsi/fc/fc_ns.h>


#define FIP_DEF_PRI	128	
#define FIP_DEF_FC_MAP	0x0efc00 
#define FIP_DEF_FKA	8000	
#define FIP_VN_KA_PERIOD 90000	
#define FIP_FCF_FUZZ	100	

#define FIP_VN_FC_MAP	0x0efd00 
#define FIP_VN_PROBE_WAIT 100	
#define FIP_VN_ANN_WAIT 400	
#define FIP_VN_RLIM_INT 10000	
#define FIP_VN_RLIM_COUNT 10	
#define FIP_VN_BEACON_INT 8000	
#define FIP_VN_BEACON_FUZZ 100	

#define FIP_ALL_FCOE_MACS	((__u8[6]) { 1, 0x10, 0x18, 1, 0, 0 })
#define FIP_ALL_ENODE_MACS	((__u8[6]) { 1, 0x10, 0x18, 1, 0, 1 })
#define FIP_ALL_FCF_MACS	((__u8[6]) { 1, 0x10, 0x18, 1, 0, 2 })
#define FIP_ALL_VN2VN_MACS	((__u8[6]) { 1, 0x10, 0x18, 1, 0, 4 })
#define FIP_ALL_P2P_MACS	((__u8[6]) { 1, 0x10, 0x18, 1, 0, 5 })

#define FIP_VER		1		

struct fip_header {
	__u8	fip_ver;		
	__u8	fip_resv1;		
	__be16	fip_op;			
	__u8	fip_resv2;		
	__u8	fip_subcode;		
	__be16	fip_dl_len;		
	__be16	fip_flags;		
} __attribute__((packed));

#define FIP_VER_SHIFT	4
#define FIP_VER_ENCAPS(v) ((v) << FIP_VER_SHIFT)
#define FIP_VER_DECAPS(v) ((v) >> FIP_VER_SHIFT)
#define FIP_BPW		4		

enum fip_opcode {
	FIP_OP_DISC =	1,		
	FIP_OP_LS =	2,		
	FIP_OP_CTRL =	3,		
	FIP_OP_VLAN =	4,		
	FIP_OP_VN2VN =	5,		
	FIP_OP_VENDOR_MIN = 0xfff8,	
	FIP_OP_VENDOR_MAX = 0xfffe,	
};

enum fip_disc_subcode {
	FIP_SC_SOL =	1,		
	FIP_SC_ADV =	2,		
};

enum fip_trans_subcode {
	FIP_SC_REQ =	1,		
	FIP_SC_REP =	2,		
};

enum fip_reset_subcode {
	FIP_SC_KEEP_ALIVE = 1,		
	FIP_SC_CLR_VLINK = 2,		
};

enum fip_vlan_subcode {
	FIP_SC_VL_REQ =	1,		
	FIP_SC_VL_REP =	2,		
};

enum fip_vn2vn_subcode {
	FIP_SC_VN_PROBE_REQ = 1,	
	FIP_SC_VN_PROBE_REP = 2,	
	FIP_SC_VN_CLAIM_NOTIFY = 3,	
	FIP_SC_VN_CLAIM_REP = 4,	
	FIP_SC_VN_BEACON = 5,		
};

enum fip_flag {
	FIP_FL_FPMA =	0x8000,		
	FIP_FL_SPMA =	0x4000,		
	FIP_FL_REC_OR_P2P = 0x0008,	
	FIP_FL_AVAIL =	0x0004,		
	FIP_FL_SOL =	0x0002,		
	FIP_FL_FPORT =	0x0001,		
};

struct fip_desc {
	__u8	fip_dtype;		
	__u8	fip_dlen;		
};

enum fip_desc_type {
	FIP_DT_PRI =	1,		
	FIP_DT_MAC =	2,		
	FIP_DT_MAP_OUI = 3,		
	FIP_DT_NAME =	4,		
	FIP_DT_FAB =	5,		
	FIP_DT_FCOE_SIZE = 6,		
	FIP_DT_FLOGI =	7,		
	FIP_DT_FDISC =	8,		
	FIP_DT_LOGO =	9,		
	FIP_DT_ELP =	10,		
	FIP_DT_VN_ID =	11,		
	FIP_DT_FKA =	12,		
	FIP_DT_VENDOR =	13,		
	FIP_DT_VLAN =	14,		
	FIP_DT_FC4F =	15,		
	FIP_DT_LIMIT,			
	FIP_DT_VENDOR_BASE = 128,	
};

struct fip_pri_desc {
	struct fip_desc fd_desc;
	__u8		fd_resvd;
	__u8		fd_pri;		
} __attribute__((packed));

struct fip_mac_desc {
	struct fip_desc fd_desc;
	__u8		fd_mac[ETH_ALEN];
} __attribute__((packed));

struct fip_map_desc {
	struct fip_desc fd_desc;
	__u8		fd_resvd[3];
	__u8		fd_map[3];
} __attribute__((packed));

struct fip_wwn_desc {
	struct fip_desc fd_desc;
	__u8		fd_resvd[2];
	__be64		fd_wwn;		
} __attribute__((packed));

struct fip_fab_desc {
	struct fip_desc fd_desc;
	__be16		fd_vfid;	
	__u8		fd_resvd;
	__u8		fd_map[3];	
	__be64		fd_wwn;		
} __attribute__((packed));

struct fip_size_desc {
	struct fip_desc fd_desc;
	__be16		fd_size;
} __attribute__((packed));

struct fip_encaps {
	struct fip_desc fd_desc;
	__u8		fd_resvd[2];
} __attribute__((packed));

struct fip_vn_desc {
	struct fip_desc fd_desc;
	__u8		fd_mac[ETH_ALEN];
	__u8		fd_resvd;
	__u8		fd_fc_id[3];
	__be64		fd_wwpn;	
} __attribute__((packed));

struct fip_fka_desc {
	struct fip_desc fd_desc;
	__u8		fd_resvd;
	__u8		fd_flags;	
	__be32		fd_fka_period;	
} __attribute__((packed));

enum fip_fka_flags {
	FIP_FKA_ADV_D =	0x01,		
};


struct fip_fc4_feat {
	struct fip_desc fd_desc;
	__u8		fd_resvd[2];
	struct fc_ns_fts fd_fts;
	struct fc_ns_ff	fd_ff;
} __attribute__((packed));

struct fip_vendor_desc {
	struct fip_desc fd_desc;
	__u8		fd_resvd[2];
	__u8		fd_vendor_id[8];
} __attribute__((packed));

#endif 
