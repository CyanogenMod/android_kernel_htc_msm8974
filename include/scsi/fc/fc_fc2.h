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

#ifndef _FC_FC2_H_
#define _FC_FC2_H_

#ifndef PACKED
#define PACKED  __attribute__ ((__packed__))
#endif 


struct fc_ssb {
	__u8	ssb_seq_id;		
	__u8	_ssb_resvd;
	__be16	ssb_low_seq_cnt;	

	__be16	ssb_high_seq_cnt;	
	__be16	ssb_s_stat;		

	__be16	ssb_err_seq_cnt;	
	__u8	ssb_fh_cs_ctl;		
	__be16	ssb_fh_ox_id;		
	__be16	ssb_rx_id;		
	__u8	_ssb_resvd2[2];
} PACKED;

#define FC_SSB_SIZE         17          

#define SSB_ST_RESP         (1 << 15)   
#define SSB_ST_ACTIVE       (1 << 14)   
#define SSB_ST_ABNORMAL     (1 << 12)   

#define SSB_ST_REQ_MASK     (3 << 10)   
#define SSB_ST_REQ_CONT     (0 << 10)
#define SSB_ST_REQ_ABORT    (1 << 10)
#define SSB_ST_REQ_STOP     (2 << 10)
#define SSB_ST_REQ_RETRANS  (3 << 10)

#define SSB_ST_ABTS         (1 << 9)    
#define SSB_ST_RETRANS      (1 << 8)    
#define SSB_ST_TIMEOUT      (1 << 7)    
#define SSB_ST_P_RJT        (1 << 6)    

#define SSB_ST_CLASS_BIT    4           
#define SSB_ST_CLASS_MASK   3           
#define SSB_ST_ACK          (1 << 3)    

struct fc_esb {
	__u8	esb_cs_ctl;		
	__be16	esb_ox_id;		
	__be16	esb_rx_id;		
	__be32	esb_orig_fid;		
	__be32	esb_resp_fid;		
	__be32	esb_e_stat;		
	__u8	_esb_resvd[4];
	__u8	esb_service_params[112]; 
	__u8	esb_seq_status[8];	
} __attribute__((packed));

#define FC_ESB_SIZE         (1 + 5*4 + 112 + 8)     

#define ESB_ST_RESP         (1 << 31)   
#define ESB_ST_SEQ_INIT     (1 << 30)   
#define ESB_ST_COMPLETE     (1 << 29)   
#define ESB_ST_ABNORMAL     (1 << 28)   
#define ESB_ST_REC_QUAL     (1 << 26)   

#define ESB_ST_ERRP_BIT     24          
#define ESB_ST_ERRP_MASK    (3 << 24)   
#define ESB_ST_ERRP_MULT    (0 << 24)   
#define ESB_ST_ERRP_SING    (1 << 24)   
#define ESB_ST_ERRP_INF     (2 << 24)   
#define ESB_ST_ERRP_IMM     (3 << 24)   

#define ESB_ST_OX_ID_INVL   (1 << 23)   
#define ESB_ST_RX_ID_INVL   (1 << 22)   
#define ESB_ST_PRI_INUSE    (1 << 21)   

#endif 
