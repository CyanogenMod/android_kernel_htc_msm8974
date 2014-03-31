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

#ifndef _FC_FCP_H_
#define	_FC_FCP_H_

#include <scsi/scsi.h>



#define	FCP_SPPF_TASK_RETRY_ID	0x0200	
#define	FCP_SPPF_RETRY		0x0100	
#define	FCP_SPPF_CONF_COMPL	0x0080	
#define	FCP_SPPF_OVLY_ALLOW	0x0040	
#define	FCP_SPPF_INIT_FCN	0x0020	
#define	FCP_SPPF_TARG_FCN	0x0010	
#define	FCP_SPPF_RD_XRDY_DIS	0x0002	
#define	FCP_SPPF_WR_XRDY_DIS	0x0001	

struct fcp_cmnd {
	struct scsi_lun	fc_lun;		
	__u8		fc_cmdref;	
	__u8		fc_pri_ta;	
	__u8		fc_tm_flags;	
	__u8		fc_flags;	
	__u8		fc_cdb[16];	
	__be32		fc_dl;		
};

#define	FCP_CMND_LEN	32	

struct fcp_cmnd32 {
	struct scsi_lun	fc_lun;		
	__u8		fc_cmdref;	
	__u8		fc_pri_ta;	
	__u8		fc_tm_flags;	
	__u8		fc_flags;	
	__u8		fc_cdb[32];	
	__be32		fc_dl;		
};

#define	FCP_CMND32_LEN	    48	
#define	FCP_CMND32_ADD_LEN  (16 / 4)	

#define	FCP_PTA_SIMPLE	    0	
#define	FCP_PTA_HEADQ	    1	
#define	FCP_PTA_ORDERED     2	
#define	FCP_PTA_ACA	    4	
#define	FCP_PTA_MASK	    7	
#define	FCP_PRI_SHIFT	    3	
#define	FCP_PRI_RESVD_MASK  0x80	

#define	FCP_TMF_CLR_ACA		0x40	
#define	FCP_TMF_TGT_RESET	0x20	
#define	FCP_TMF_LUN_RESET	0x10	
#define	FCP_TMF_CLR_TASK_SET	0x04	
#define	FCP_TMF_ABT_TASK_SET	0x02	

#define	FCP_CFL_LEN_MASK	0xfc	
#define	FCP_CFL_LEN_SHIFT	2	
#define	FCP_CFL_RDDATA		0x02	
#define	FCP_CFL_WRDATA		0x01	

struct fcp_txrdy {
	__be32		ft_data_ro;	
	__be32		ft_burst_len;	
	__u8		_ft_resvd[4];	
};

#define	FCP_TXRDY_LEN	12	

struct fcp_resp {
	__u8		_fr_resvd[8];	
	__be16		fr_retry_delay;	
	__u8		fr_flags;	
	__u8		fr_status;	
};

#define	FCP_RESP_LEN	12	

struct fcp_resp_ext {
	__be32		fr_resid;	
	__be32		fr_sns_len;	
	__be32		fr_rsp_len;	

};

#define FCP_RESP_EXT_LEN    12  

struct fcp_resp_rsp_info {
    __u8      _fr_resvd[3];       
    __u8      rsp_code;           
    __u8      _fr_resvd2[4];      
};

struct fcp_resp_with_ext {
	struct fcp_resp resp;
	struct fcp_resp_ext ext;
};

#define	FCP_RESP_WITH_EXT   (FCP_RESP_LEN + FCP_RESP_EXT_LEN)

#define	FCP_BIDI_RSP	    0x80	
#define	FCP_BIDI_READ_UNDER 0x40	
#define	FCP_BIDI_READ_OVER  0x20	
#define	FCP_CONF_REQ	    0x10	
#define	FCP_RESID_UNDER     0x08	
#define	FCP_RESID_OVER	    0x04	
#define	FCP_SNS_LEN_VAL     0x02	
#define	FCP_RSP_LEN_VAL     0x01	

enum fcp_resp_rsp_codes {
	FCP_TMF_CMPL = 0,
	FCP_DATA_LEN_INVALID = 1,
	FCP_CMND_FIELDS_INVALID = 2,
	FCP_DATA_PARAM_MISMATCH = 3,
	FCP_TMF_REJECTED = 4,
	FCP_TMF_FAILED = 5,
	FCP_TMF_INVALID_LUN = 9,
};

struct fcp_srr {
	__u8		srr_op;		
	__u8		srr_resvd[3];	
	__be16		srr_ox_id;	
	__be16		srr_rx_id;	
	__be32		srr_rel_off;	
	__u8		srr_r_ctl;	
	__u8		srr_resvd2[3];	
};

#define	FCP_FEAT_TARG	(1 << 0)	
#define	FCP_FEAT_INIT	(1 << 1)	

#endif 
