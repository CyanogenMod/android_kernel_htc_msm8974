/*
 * Linux network driver for Brocade Converged Network Adapter.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License (GPL) Version 2 as
 * published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
/*
 * Copyright (c) 2005-2010 Brocade Communications Systems, Inc.
 * All rights reserved
 * www.brocade.com
 */
#ifndef __BFI_CNA_H__
#define __BFI_CNA_H__

#include "bfi.h"
#include "bfa_defs_cna.h"

#pragma pack(1)

enum bfi_port_h2i {
	BFI_PORT_H2I_ENABLE_REQ		= (1),
	BFI_PORT_H2I_DISABLE_REQ	= (2),
	BFI_PORT_H2I_GET_STATS_REQ	= (3),
	BFI_PORT_H2I_CLEAR_STATS_REQ	= (4),
};

enum bfi_port_i2h {
	BFI_PORT_I2H_ENABLE_RSP		= BFA_I2HM(1),
	BFI_PORT_I2H_DISABLE_RSP	= BFA_I2HM(2),
	BFI_PORT_I2H_GET_STATS_RSP	= BFA_I2HM(3),
	BFI_PORT_I2H_CLEAR_STATS_RSP	= BFA_I2HM(4),
};

struct bfi_port_generic_req {
	struct bfi_mhdr mh;		
	u32	msgtag;		
	u32	rsvd;
};

struct bfi_port_generic_rsp {
	struct bfi_mhdr mh;		
	u8		status;		
	u8		rsvd[3];
	u32	msgtag;		
};





struct bfi_port_get_stats_req {
	struct bfi_mhdr mh;		
	union bfi_addr_u   dma_addr;
};




union bfi_port_h2i_msg_u {
	struct bfi_mhdr mh;
	struct bfi_port_generic_req enable_req;
	struct bfi_port_generic_req disable_req;
	struct bfi_port_get_stats_req getstats_req;
	struct bfi_port_generic_req clearstats_req;
};

union bfi_port_i2h_msg_u {
	struct bfi_mhdr mh;
	struct bfi_port_generic_rsp enable_rsp;
	struct bfi_port_generic_rsp disable_rsp;
	struct bfi_port_generic_rsp getstats_rsp;
	struct bfi_port_generic_rsp clearstats_rsp;
};

enum bfi_cee_h2i_msgs {
	BFI_CEE_H2I_GET_CFG_REQ = 1,
	BFI_CEE_H2I_RESET_STATS = 2,
	BFI_CEE_H2I_GET_STATS_REQ = 3,
};

enum bfi_cee_i2h_msgs {
	BFI_CEE_I2H_GET_CFG_RSP = BFA_I2HM(1),
	BFI_CEE_I2H_RESET_STATS_RSP = BFA_I2HM(2),
	BFI_CEE_I2H_GET_STATS_RSP = BFA_I2HM(3),
};


struct bfi_lldp_reset_stats {
	struct bfi_mhdr mh;
};

struct bfi_cee_reset_stats {
	struct bfi_mhdr mh;
};

struct bfi_cee_get_req {
	struct bfi_mhdr mh;
	union bfi_addr_u   dma_addr;
};

struct bfi_cee_get_rsp {
	struct bfi_mhdr mh;
	u8			cmd_status;
	u8			rsvd[3];
};

struct bfi_cee_stats_req {
	struct bfi_mhdr mh;
	union bfi_addr_u   dma_addr;
};

struct bfi_cee_stats_rsp {
	struct bfi_mhdr mh;
	u8			cmd_status;
	u8			rsvd[3];
};

union bfi_cee_h2i_msg_u {
	struct bfi_mhdr mh;
	struct bfi_cee_get_req get_req;
	struct bfi_cee_stats_req stats_req;
};

union bfi_cee_i2h_msg_u {
	struct bfi_mhdr mh;
	struct bfi_cee_get_rsp get_rsp;
	struct bfi_cee_stats_rsp stats_rsp;
};

#pragma pack()

#endif 
