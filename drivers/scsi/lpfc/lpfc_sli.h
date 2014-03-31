/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2004-2007 Emulex.  All rights reserved.           *
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

struct lpfc_hba;
struct lpfc_vport;

typedef enum _lpfc_ctx_cmd {
	LPFC_CTX_LUN,
	LPFC_CTX_TGT,
	LPFC_CTX_HOST
} lpfc_ctx_cmd;

struct lpfc_cq_event {
	struct list_head list;
	union {
		struct lpfc_mcqe		mcqe_cmpl;
		struct lpfc_acqe_link		acqe_link;
		struct lpfc_acqe_fip		acqe_fip;
		struct lpfc_acqe_dcbx		acqe_dcbx;
		struct lpfc_acqe_grp5		acqe_grp5;
		struct lpfc_acqe_fc_la		acqe_fc;
		struct lpfc_acqe_sli		acqe_sli;
		struct lpfc_rcqe		rcqe_cmpl;
		struct sli4_wcqe_xri_aborted	wcqe_axri;
		struct lpfc_wcqe_complete	wcqe_cmpl;
	} cqe;
};

struct lpfc_iocbq {
	
	struct list_head list;
	struct list_head clist;
	struct list_head dlist;
	uint16_t iotag;         
	uint16_t sli4_lxritag;  
	uint16_t sli4_xritag;   
	struct lpfc_cq_event cq_event;

	IOCB_t iocb;		
	uint8_t retry;		
	uint16_t iocb_flag;
#define LPFC_IO_LIBDFC		1	
#define LPFC_IO_WAKE		2	
#define LPFC_IO_FCP		4	
#define LPFC_DRIVER_ABORTED	8	
#define LPFC_IO_FABRIC		0x10	
#define LPFC_DELAY_MEM_FREE	0x20    
#define LPFC_EXCHANGE_BUSY	0x40    
#define LPFC_USE_FCPWQIDX	0x80    
#define DSS_SECURITY_OP		0x100	
#define LPFC_IO_ON_Q		0x200	
#define LPFC_IO_DIF		0x400	

#define LPFC_FIP_ELS_ID_MASK	0xc000	
#define LPFC_FIP_ELS_ID_SHIFT	14

	uint8_t rsvd2;
	uint32_t drvrTimeout;	
	uint32_t fcp_wqidx;	
	struct lpfc_vport *vport;
	void *context1;		
	void *context2;		
	void *context3;		
	union {
		wait_queue_head_t    *wait_queue;
		struct lpfc_iocbq    *rsp_iocb;
		struct lpfcMboxq     *mbox;
		struct lpfc_nodelist *ndlp;
		struct lpfc_node_rrq *rrq;
	} context_un;

	void (*fabric_iocb_cmpl) (struct lpfc_hba *, struct lpfc_iocbq *,
			   struct lpfc_iocbq *);
	void (*iocb_cmpl) (struct lpfc_hba *, struct lpfc_iocbq *,
			   struct lpfc_iocbq *);
};

#define SLI_IOCB_RET_IOCB      1	

#define IOCB_SUCCESS        0
#define IOCB_BUSY           1
#define IOCB_ERROR          2
#define IOCB_TIMEDOUT       3

#define LPFC_MBX_WAKE		1
#define LPFC_MBX_IMED_UNREG	2

typedef struct lpfcMboxq {
	
	struct list_head list;	
	union {
		MAILBOX_t mb;		
		struct lpfc_mqe mqe;
	} u;
	struct lpfc_vport *vport;
	void *context1;		
	void *context2;		

	void (*mbox_cmpl) (struct lpfc_hba *, struct lpfcMboxq *);
	uint8_t mbox_flag;
	uint16_t in_ext_byte_len;
	uint16_t out_ext_byte_len;
	uint8_t  mbox_offset_word;
	struct lpfc_mcqe mcqe;
	struct lpfc_mbx_nembed_sge_virt *sge_array;
} LPFC_MBOXQ_t;

#define MBX_POLL        1	
#define MBX_NOWAIT      2	

#define LPFC_MAX_RING_MASK  5	
#define LPFC_MAX_RING       4	

struct lpfc_sli_ring;

struct lpfc_sli_ring_mask {
	uint8_t profile;	
	uint8_t rctl;	
	uint8_t type;	
	uint8_t rsvd;
	
	void (*lpfc_sli_rcv_unsol_event) (struct lpfc_hba *,
					 struct lpfc_sli_ring *,
					 struct lpfc_iocbq *);
};


struct lpfc_sli_ring_stat {
	uint64_t iocb_event;	 
	uint64_t iocb_cmd;	 
	uint64_t iocb_rsp;	 
	uint64_t iocb_cmd_delay; 
	uint64_t iocb_cmd_full;	 
	uint64_t iocb_cmd_empty; 
	uint64_t iocb_rsp_full;	 
};

struct lpfc_sli_ring {
	uint16_t flag;		
#define LPFC_DEFERRED_RING_EVENT 0x001	
#define LPFC_CALL_RING_AVAILABLE 0x002	
#define LPFC_STOP_IOCB_EVENT     0x020	
	uint16_t abtsiotag;	

	uint32_t local_getidx;   
	uint32_t next_cmdidx;    
	uint32_t rspidx;	
	uint32_t cmdidx;	
	uint8_t rsvd;
	uint8_t ringno;		
	uint16_t numCiocb;	
	uint16_t numRiocb;	
	uint16_t sizeCiocb;	
	uint16_t sizeRiocb;	

	uint32_t fast_iotag;	
	uint32_t iotag_ctr;	
	uint32_t iotag_max;	
	struct list_head txq;
	uint16_t txq_cnt;	
	uint16_t txq_max;	
	struct list_head txcmplq;
	uint16_t txcmplq_cnt;	
	uint16_t txcmplq_max;	
	uint32_t *cmdringaddr;	
	uint32_t *rspringaddr;	
	uint32_t missbufcnt;	
	struct list_head postbufq;
	uint16_t postbufq_cnt;	
	uint16_t postbufq_max;	
	struct list_head iocb_continueq;
	uint16_t iocb_continueq_cnt;	
	uint16_t iocb_continueq_max;	
	struct list_head iocb_continue_saveq;

	struct lpfc_sli_ring_mask prt[LPFC_MAX_RING_MASK];
	uint32_t num_mask;	
	void (*lpfc_sli_rcv_async_status) (struct lpfc_hba *,
		struct lpfc_sli_ring *, struct lpfc_iocbq *);

	struct lpfc_sli_ring_stat stats;	

	
	void (*lpfc_sli_cmd_available) (struct lpfc_hba *,
					struct lpfc_sli_ring *);
};

struct lpfc_hbq_init {
	uint32_t rn;		
	uint32_t entry_count;	
	uint32_t headerLen;	
	uint32_t logEntry;	
	uint32_t profile;	
	uint32_t ring_mask;	
	uint32_t hbq_index;	

	uint32_t seqlenoff;
	uint32_t maxlen;
	uint32_t seqlenbcnt;
	uint32_t cmdcodeoff;
	uint32_t cmdmatch[8];
	uint32_t mask_count;	
	struct hbq_mask hbqMasks[6];

	
	uint32_t buffer_count;	
	uint32_t init_count;	
	uint32_t add_count;	
} ;

struct lpfc_sli_stat {
	uint64_t mbox_stat_err;  
	uint64_t mbox_cmd;       
	uint64_t sli_intr;       
	uint32_t err_attn_event; 
	uint32_t link_event;     
	uint32_t mbox_event;     
	uint32_t mbox_busy;	 
};

struct lpfc_lnk_stat {
	uint32_t link_failure_count;
	uint32_t loss_of_sync_count;
	uint32_t loss_of_signal_count;
	uint32_t prim_seq_protocol_err_count;
	uint32_t invalid_tx_word_count;
	uint32_t invalid_crc_count;
	uint32_t error_frames;
	uint32_t link_events;
};

struct lpfc_sli {
	uint32_t num_rings;
	uint32_t sli_flag;

	
#define LPFC_SLI_MBOX_ACTIVE      0x100	
#define LPFC_SLI_ACTIVE           0x200	
#define LPFC_PROCESS_LA           0x400	
#define LPFC_BLOCK_MGMT_IO        0x800	
#define LPFC_MENLO_MAINT          0x1000 
#define LPFC_SLI_ASYNC_MBX_BLK    0x2000 

	struct lpfc_sli_ring ring[LPFC_MAX_RING];
	int fcp_ring;		
	int next_ring;

	int extra_ring;		

	struct lpfc_sli_stat slistat;	
	struct list_head mboxq;
	uint16_t mboxq_cnt;	
	uint16_t mboxq_max;	
	LPFC_MBOXQ_t *mbox_active;	
	struct list_head mboxq_cmpl;

	struct timer_list mbox_tmo;	

#define LPFC_IOCBQ_LOOKUP_INCREMENT  1024
	struct lpfc_iocbq ** iocbq_lookup; 
	size_t iocbq_lookup_len;           
	uint16_t  last_iotag;              
	unsigned long  stats_start;        
	struct lpfc_lnk_stat lnk_stat_offsets;
};

#define LPFC_MBOX_TMO				30
#define LPFC_MBOX_SLI4_CONFIG_TMO		60
#define LPFC_MBOX_SLI4_CONFIG_EXTENDED_TMO	300
#define LPFC_MBOX_TMO_FLASH_CMD			300
