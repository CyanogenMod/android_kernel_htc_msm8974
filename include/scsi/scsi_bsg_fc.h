/*
 *  FC Transport BSG Interface
 *
 *  Copyright (C) 2008   James Smart, Emulex Corporation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef SCSI_BSG_FC_H
#define SCSI_BSG_FC_H


#include <scsi/scsi.h>


#define FC_DEFAULT_BSG_TIMEOUT		(10 * HZ)



#define FC_BSG_CLS_MASK		0xF0000000	
#define FC_BSG_HST_MASK		0x80000000	
#define FC_BSG_RPT_MASK		0x40000000	

	
#define FC_BSG_HST_ADD_RPORT		(FC_BSG_HST_MASK | 0x00000001)
#define FC_BSG_HST_DEL_RPORT		(FC_BSG_HST_MASK | 0x00000002)
#define FC_BSG_HST_ELS_NOLOGIN		(FC_BSG_HST_MASK | 0x00000003)
#define FC_BSG_HST_CT			(FC_BSG_HST_MASK | 0x00000004)
#define FC_BSG_HST_VENDOR		(FC_BSG_HST_MASK | 0x000000FF)

	
#define FC_BSG_RPT_ELS			(FC_BSG_RPT_MASK | 0x00000001)
#define FC_BSG_RPT_CT			(FC_BSG_RPT_MASK | 0x00000002)







struct fc_bsg_host_add_rport {
	uint8_t		reserved;

	
	uint8_t		port_id[3];
};




struct fc_bsg_host_del_rport {
	uint8_t		reserved;

	
	uint8_t		port_id[3];
};




struct fc_bsg_host_els {
	uint8_t 	command_code;

	
	uint8_t		port_id[3];
};

#define FC_CTELS_STATUS_OK	0x00000000
#define FC_CTELS_STATUS_REJECT	0x00000001
#define FC_CTELS_STATUS_P_RJT	0x00000002
#define FC_CTELS_STATUS_F_RJT	0x00000003
#define FC_CTELS_STATUS_P_BSY	0x00000004
#define FC_CTELS_STATUS_F_BSY	0x00000006
struct fc_bsg_ctels_reply {
	uint32_t	status;		

	
	struct	{
		uint8_t	action;		
		uint8_t	reason_code;
		uint8_t	reason_explanation;
		uint8_t	vendor_unique;
	} rjt_data;
};



struct fc_bsg_host_ct {
	uint8_t		reserved;

	
	uint8_t		port_id[3];

	uint32_t	preamble_word0;	
	uint32_t	preamble_word1;	
	uint32_t	preamble_word2;	

};



struct fc_bsg_host_vendor {
	uint64_t vendor_id;

	
	uint32_t vendor_cmd[0];
};

struct fc_bsg_host_vendor_reply {
	
	uint32_t vendor_rsp[0];
};





struct fc_bsg_rport_els {
	uint8_t els_code;
};




struct fc_bsg_rport_ct {
	uint32_t	preamble_word0;	
	uint32_t	preamble_word1;	
	uint32_t	preamble_word2;	
};




struct fc_bsg_request {
	uint32_t msgcode;
	union {
		struct fc_bsg_host_add_rport	h_addrport;
		struct fc_bsg_host_del_rport	h_delrport;
		struct fc_bsg_host_els		h_els;
		struct fc_bsg_host_ct		h_ct;
		struct fc_bsg_host_vendor	h_vendor;

		struct fc_bsg_rport_els		r_els;
		struct fc_bsg_rport_ct		r_ct;
	} rqst_data;
} __attribute__((packed));


struct fc_bsg_reply {
	uint32_t result;

	
	uint32_t reply_payload_rcv_len;

	union {
		struct fc_bsg_host_vendor_reply		vendor_reply;

		struct fc_bsg_ctels_reply		ctels_reply;
	} reply_data;
};


#endif 

