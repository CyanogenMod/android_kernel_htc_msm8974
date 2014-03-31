/*
 * Copyright(c) 2007 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Maintained at www.Open-FCoE.org
 */

#ifndef _FC_FCOE_H_
#define	_FC_FCOE_H_


#define	FC_FCOE_OUI	0x0efc00	

#define	FC_FCOE_FLOGI_MAC { 0x0e, 0xfc, 0x00, 0xff, 0xff, 0xfe }

#define	FC_FCOE_VER	0			

#define	FC_FCOE_ENCAPS_ID(n)	(((u64) FC_FCOE_OUI << 24) | (n))
#define	FC_FCOE_DECAPS_ID(n)	((n) >> 24)

struct fcoe_hdr {
	__u8		fcoe_ver;	
	__u8		fcoe_resvd[12];	
	__u8		fcoe_sof;	
};

#define FC_FCOE_DECAPS_VER(hp)	    ((hp)->fcoe_ver >> 4)
#define FC_FCOE_ENCAPS_VER(hp, ver) ((hp)->fcoe_ver = (ver) << 4)

struct fcoe_crc_eof {
	__le32		fcoe_crc32;	
	__u8		fcoe_eof;	
	__u8		fcoe_resvd[3];	
} __attribute__((packed));

#define FCOE_HEADER_LEN 38

#define FCOE_MIN_FRAME 46

struct fcoe_fc_els_lesb {
	__be32		lesb_link_fail;	
	__be32		lesb_vlink_fail; 
	__be32		lesb_miss_fka;	
	__be32		lesb_symb_err;	
	__be32		lesb_err_block;	
	__be32		lesb_fcs_error; 
};

static inline void fc_fcoe_set_mac(u8 *mac, u8 *did)
{
	mac[0] = (u8) (FC_FCOE_OUI >> 16);
	mac[1] = (u8) (FC_FCOE_OUI >> 8);
	mac[2] = (u8) FC_FCOE_OUI;
	mac[3] = did[0];
	mac[4] = did[1];
	mac[5] = did[2];
}

#endif 
