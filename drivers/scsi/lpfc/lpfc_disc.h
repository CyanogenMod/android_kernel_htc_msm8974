/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2004-2008 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com                                                  *
 *                                                                 *
 * This program is free software; you can redistribute it and/or   *
 * modify it under the terms of version 2 of the GNU General       *
 * Public License as published by the Free Software Foundation.    *
 * This program is distributed in the hope that it will be useful. *
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND          *
 * WARRANTIES, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,  *
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE      *
 * DISCLAIMED, EXCEPT TO THE EXTENT THAT SUCH DISCLAIMERS ARE HELD *
 * TO BE LEGALLY INVALID.  See the GNU General Public License for  *
 * more details, a copy of which can be found in the file COPYING  *
 * included with this package.                                     *
 *******************************************************************/

#define FC_MAX_HOLD_RSCN     32	      
#define FC_MAX_NS_RSP        64512    
#define FC_MAXLOOP           126      
#define LPFC_DISC_FLOGI_TMO  10	      



enum lpfc_work_type {
	LPFC_EVT_ONLINE,
	LPFC_EVT_OFFLINE_PREP,
	LPFC_EVT_OFFLINE,
	LPFC_EVT_WARM_START,
	LPFC_EVT_KILL,
	LPFC_EVT_ELS_RETRY,
	LPFC_EVT_DEV_LOSS,
	LPFC_EVT_FASTPATH_MGMT_EVT,
	LPFC_EVT_RESET_HBA,
};

struct lpfc_work_evt {
	struct list_head      evt_listp;
	void                 *evt_arg1;
	void                 *evt_arg2;
	enum lpfc_work_type   evt;
};

struct lpfc_scsi_check_condition_event;
struct lpfc_scsi_varqueuedepth_event;
struct lpfc_scsi_event_header;
struct lpfc_fabric_event_header;
struct lpfc_fcprdchkerr_event;

struct lpfc_fast_path_event {
	struct lpfc_work_evt work_evt;
	struct lpfc_vport     *vport;
	union {
		struct lpfc_scsi_check_condition_event check_cond_evt;
		struct lpfc_scsi_varqueuedepth_event queue_depth_evt;
		struct lpfc_scsi_event_header scsi_evt;
		struct lpfc_fabric_event_header fabric_evt;
		struct lpfc_fcprdchkerr_event read_check_error;
	} un;
};

#define LPFC_SLI4_MAX_XRI	1024	
#define XRI_BITMAP_ULONGS (LPFC_SLI4_MAX_XRI / BITS_PER_LONG)
struct lpfc_node_rrqs {
	unsigned long xri_bitmap[XRI_BITMAP_ULONGS];
};

struct lpfc_nodelist {
	struct list_head nlp_listp;
	struct lpfc_name nlp_portname;
	struct lpfc_name nlp_nodename;
	uint32_t         nlp_flag;		
	uint32_t         nlp_DID;		
	uint32_t         nlp_last_elscmd;	
	uint16_t         nlp_type;
#define NLP_FC_NODE        0x1			
#define NLP_FABRIC         0x4			
#define NLP_FCP_TARGET     0x8			
#define NLP_FCP_INITIATOR  0x10			

	uint16_t        nlp_rpi;
	uint16_t        nlp_state;		
	uint16_t        nlp_prev_state;		
	uint16_t        nlp_xri;		
	uint16_t        nlp_sid;		
#define NLP_NO_SID		0xffff
	uint16_t	nlp_maxframe;		
	uint8_t		nlp_class_sup;		
	uint8_t         nlp_retry;		
	uint8_t         nlp_fcp_info;	        
#define NLP_FCP_2_DEVICE   0x10			

	uint16_t        nlp_usg_map;	
#define NLP_USG_NODE_ACT_BIT	0x1	
#define NLP_USG_IACT_REQ_BIT	0x2	
#define NLP_USG_FREE_REQ_BIT	0x4	
#define NLP_USG_FREE_ACK_BIT	0x8	

	struct timer_list   nlp_delayfunc;	
	struct lpfc_hba *phba;
	struct fc_rport *rport;			
	struct lpfc_vport *vport;
	struct lpfc_work_evt els_retry_evt;
	struct lpfc_work_evt dev_loss_evt;
	struct kref     kref;
	atomic_t cmd_pending;
	uint32_t cmd_qdepth;
	unsigned long last_change_time;
	struct lpfc_node_rrqs active_rrqs;
	struct lpfc_scsicmd_bkt *lat_data;	
};
struct lpfc_node_rrq {
	struct list_head list;
	uint16_t xritag;
	uint16_t send_rrq;
	uint16_t rxid;
	uint32_t         nlp_DID;		
	struct lpfc_vport *vport;
	struct lpfc_nodelist *ndlp;
	unsigned long rrq_stop_time;
};

#define NLP_IGNR_REG_CMPL  0x00000001 
#define NLP_REG_LOGIN_SEND 0x00000002   
#define NLP_PLOGI_SND      0x00000020	
#define NLP_PRLI_SND       0x00000040	
#define NLP_ADISC_SND      0x00000080	
#define NLP_LOGO_SND       0x00000100	
#define NLP_RNID_SND       0x00000400	
#define NLP_ELS_SND_MASK   0x000007e0	
#define NLP_DEFER_RM       0x00010000	
#define NLP_DELAY_TMO      0x00020000	
#define NLP_NPR_2B_DISC    0x00040000	
#define NLP_RCV_PLOGI      0x00080000	
#define NLP_LOGO_ACC       0x00100000	
#define NLP_TGT_NO_SCSIID  0x00200000	
#define NLP_ACC_REGLOGIN   0x01000000	
#define NLP_NPR_ADISC      0x02000000	
#define NLP_RM_DFLT_RPI    0x04000000	
#define NLP_NODEV_REMOVE   0x08000000	
#define NLP_TARGET_REMOVE  0x10000000   
#define NLP_SC_REQ         0x20000000	
#define NLP_RPI_REGISTERED 0x80000000	

#define NLP_CHK_NODE_ACT(ndlp)		(((ndlp)->nlp_usg_map \
						& NLP_USG_NODE_ACT_BIT) \
					&& \
					!((ndlp)->nlp_usg_map \
						& NLP_USG_FREE_ACK_BIT))
#define NLP_SET_NODE_ACT(ndlp)		((ndlp)->nlp_usg_map \
						|= NLP_USG_NODE_ACT_BIT)
#define NLP_INT_NODE_ACT(ndlp)		((ndlp)->nlp_usg_map \
						= NLP_USG_NODE_ACT_BIT)
#define NLP_CLR_NODE_ACT(ndlp)		((ndlp)->nlp_usg_map \
						&= ~NLP_USG_NODE_ACT_BIT)
#define NLP_CHK_IACT_REQ(ndlp)          ((ndlp)->nlp_usg_map \
						& NLP_USG_IACT_REQ_BIT)
#define NLP_SET_IACT_REQ(ndlp)          ((ndlp)->nlp_usg_map \
						|= NLP_USG_IACT_REQ_BIT)
#define NLP_CHK_FREE_REQ(ndlp)		((ndlp)->nlp_usg_map \
						& NLP_USG_FREE_REQ_BIT)
#define NLP_SET_FREE_REQ(ndlp)		((ndlp)->nlp_usg_map \
						|= NLP_USG_FREE_REQ_BIT)
#define NLP_CHK_FREE_ACK(ndlp)		((ndlp)->nlp_usg_map \
						& NLP_USG_FREE_ACK_BIT)
#define NLP_SET_FREE_ACK(ndlp)		((ndlp)->nlp_usg_map \
						|= NLP_USG_FREE_ACK_BIT)


#define NLP_STE_UNUSED_NODE       0x0	
#define NLP_STE_PLOGI_ISSUE       0x1	
#define NLP_STE_ADISC_ISSUE       0x2	
#define NLP_STE_REG_LOGIN_ISSUE   0x3	
#define NLP_STE_PRLI_ISSUE        0x4	
#define NLP_STE_UNMAPPED_NODE     0x5	
#define NLP_STE_MAPPED_NODE       0x6	
#define NLP_STE_NPR_NODE          0x7	
#define NLP_STE_MAX_STATE         0x8
#define NLP_STE_FREED_NODE        0xff	


#define NLP_EVT_RCV_PLOGI         0x0	
#define NLP_EVT_RCV_PRLI          0x1	
#define NLP_EVT_RCV_LOGO          0x2	
#define NLP_EVT_RCV_ADISC         0x3	
#define NLP_EVT_RCV_PDISC         0x4	
#define NLP_EVT_RCV_PRLO          0x5	
#define NLP_EVT_CMPL_PLOGI        0x6	
#define NLP_EVT_CMPL_PRLI         0x7	
#define NLP_EVT_CMPL_LOGO         0x8	
#define NLP_EVT_CMPL_ADISC        0x9	
#define NLP_EVT_CMPL_REG_LOGIN    0xa	
#define NLP_EVT_DEVICE_RM         0xb	
#define NLP_EVT_DEVICE_RECOVERY   0xc	
#define NLP_EVT_MAX_EVENT         0xd

