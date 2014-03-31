/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2004-2012 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com                                                  *
 * Portions Copyright (C) 2004-2005 Christoph Hellwig              *
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

#include <scsi/scsi_host.h>

#if defined(CONFIG_DEBUG_FS) && !defined(CONFIG_SCSI_LPFC_DEBUG_FS)
#define CONFIG_SCSI_LPFC_DEBUG_FS
#endif

struct lpfc_sli2_slim;

#define LPFC_PCI_DEV_LP		0x1
#define LPFC_PCI_DEV_OC		0x2

#define LPFC_SLI_REV2		2
#define LPFC_SLI_REV3		3
#define LPFC_SLI_REV4		4

#define LPFC_MAX_TARGET		4096	
#define LPFC_MAX_DISC_THREADS	64	
#define LPFC_MAX_NS_RETRY	3	
#define LPFC_CMD_PER_LUN	3	
#define LPFC_DEFAULT_SG_SEG_CNT 64	
#define LPFC_DEFAULT_MENLO_SG_SEG_CNT 128	
#define LPFC_DEFAULT_PROT_SG_SEG_CNT 4096 
#define LPFC_MAX_SG_SEG_CNT	4096	
#define LPFC_MAX_SGE_SIZE       0x80000000 
#define LPFC_MAX_PROT_SG_SEG_CNT 4096	
#define LPFC_IOCB_LIST_CNT	2250	
#define LPFC_Q_RAMP_UP_INTERVAL 120     
#define LPFC_VNAME_LEN		100	
#define LPFC_TGTQ_INTERVAL	40000	
#define LPFC_TGTQ_RAMPUP_PCENT	5	
#define LPFC_MIN_TGT_QDEPTH	10
#define LPFC_MAX_TGT_QDEPTH	0xFFFF

#define  LPFC_MAX_BUCKET_COUNT 20	
#define QUEUE_RAMP_DOWN_INTERVAL	(1 * HZ)   
#define QUEUE_RAMP_UP_INTERVAL		(300 * HZ) 

#define LPFC_DISC_IOCB_BUFF_COUNT 20

#define LPFC_HB_MBOX_INTERVAL   5	
#define LPFC_HB_MBOX_TIMEOUT    30	

#define LPFC_ERATT_POLL_INTERVAL	5 

#define putPaddrLow(addr)    ((uint32_t) (0xffffffff & (u64)(addr)))
#define putPaddrHigh(addr)   ((uint32_t) (0xffffffff & (((u64)(addr))>>32)))
#define getPaddr(high, low)  ((dma_addr_t)( \
			     (( (u64)(high)<<16 ) << 16)|( (u64)(low))))
#define LPFC_DRVR_TIMEOUT	16	
#define FC_MAX_ADPTMSG		64

#define MAX_HBAEVT	32

#define LPFC_MSIX_VECTORS	2

#define LPFC_DATA_READY		(1<<0)

enum lpfc_polling_flags {
	ENABLE_FCP_RING_POLLING = 0x1,
	DISABLE_FCP_RING_INT    = 0x2
};

struct lpfc_dmabuf {
	struct list_head list;
	void *virt;		
	dma_addr_t phys;	
	uint32_t   buffer_tag;	
};

struct lpfc_dma_pool {
	struct lpfc_dmabuf   *elements;
	uint32_t    max_count;
	uint32_t    current_count;
};

struct hbq_dmabuf {
	struct lpfc_dmabuf hbuf;
	struct lpfc_dmabuf dbuf;
	uint32_t size;
	uint32_t tag;
	struct lpfc_cq_event cq_event;
	unsigned long time_stamp;
};

#define MEM_PRI		0x100


typedef struct lpfc_vpd {
	uint32_t status;	
	uint32_t length;	
	struct {
		uint32_t rsvd1;	
		uint32_t biuRev;
		uint32_t smRev;
		uint32_t smFwRev;
		uint32_t endecRev;
		uint16_t rBit;
		uint8_t fcphHigh;
		uint8_t fcphLow;
		uint8_t feaLevelHigh;
		uint8_t feaLevelLow;
		uint32_t postKernRev;
		uint32_t opFwRev;
		uint8_t opFwName[16];
		uint32_t sli1FwRev;
		uint8_t sli1FwName[16];
		uint32_t sli2FwRev;
		uint8_t sli2FwName[16];
	} rev;
	struct {
#ifdef __BIG_ENDIAN_BITFIELD
		uint32_t rsvd3  :19;  
		uint32_t cdss	: 1;  
		uint32_t rsvd2	: 3;  
		uint32_t cbg	: 1;  
		uint32_t cmv	: 1;  
		uint32_t ccrp   : 1;  
		uint32_t csah   : 1;  
		uint32_t chbs   : 1;  
		uint32_t cinb   : 1;  
		uint32_t cerbm	: 1;  
		uint32_t cmx	: 1;  
		uint32_t cmr	: 1;  
#else	
		uint32_t cmr	: 1;  
		uint32_t cmx	: 1;  
		uint32_t cerbm	: 1;  
		uint32_t cinb   : 1;  
		uint32_t chbs   : 1;  
		uint32_t csah   : 1;  
		uint32_t ccrp   : 1;  
		uint32_t cmv	: 1;  
		uint32_t cbg	: 1;  
		uint32_t rsvd2	: 3;  
		uint32_t cdss	: 1;  
		uint32_t rsvd3  :19;  
#endif
	} sli3Feat;
} lpfc_vpd_t;

struct lpfc_scsi_buf;


struct lpfc_stats {
	
	uint32_t elsLogiCol;
	uint32_t elsRetryExceeded;
	uint32_t elsXmitRetry;
	uint32_t elsDelayRetry;
	uint32_t elsRcvDrop;
	uint32_t elsRcvFrame;
	uint32_t elsRcvRSCN;
	uint32_t elsRcvRNID;
	uint32_t elsRcvFARP;
	uint32_t elsRcvFARPR;
	uint32_t elsRcvFLOGI;
	uint32_t elsRcvPLOGI;
	uint32_t elsRcvADISC;
	uint32_t elsRcvPDISC;
	uint32_t elsRcvFAN;
	uint32_t elsRcvLOGO;
	uint32_t elsRcvPRLO;
	uint32_t elsRcvPRLI;
	uint32_t elsRcvLIRR;
	uint32_t elsRcvRLS;
	uint32_t elsRcvRPS;
	uint32_t elsRcvRPL;
	uint32_t elsRcvRRQ;
	uint32_t elsRcvRTV;
	uint32_t elsRcvECHO;
	uint32_t elsXmitFLOGI;
	uint32_t elsXmitFDISC;
	uint32_t elsXmitPLOGI;
	uint32_t elsXmitPRLI;
	uint32_t elsXmitADISC;
	uint32_t elsXmitLOGO;
	uint32_t elsXmitSCR;
	uint32_t elsXmitRNID;
	uint32_t elsXmitFARP;
	uint32_t elsXmitFARPR;
	uint32_t elsXmitACC;
	uint32_t elsXmitLSRJT;

	uint32_t frameRcvBcast;
	uint32_t frameRcvMulti;
	uint32_t strayXmitCmpl;
	uint32_t frameXmitDelay;
	uint32_t xriCmdCmpl;
	uint32_t xriStatErr;
	uint32_t LinkUp;
	uint32_t LinkDown;
	uint32_t LinkMultiEvent;
	uint32_t NoRcvBuf;
	uint32_t fcpCmd;
	uint32_t fcpCmpl;
	uint32_t fcpRspErr;
	uint32_t fcpRemoteStop;
	uint32_t fcpPortRjt;
	uint32_t fcpPortBusy;
	uint32_t fcpError;
	uint32_t fcpLocalErr;
};

struct lpfc_hba;


enum discovery_state {
	LPFC_VPORT_UNKNOWN     =  0,    
	LPFC_VPORT_FAILED      =  1,    
	LPFC_LOCAL_CFG_LINK    =  6,    
	LPFC_FLOGI             =  7,    
	LPFC_FDISC             =  8,    
	LPFC_FABRIC_CFG_LINK   =  9,    
	LPFC_NS_REG            =  10,   
	LPFC_NS_QRY            =  11,   
	LPFC_BUILD_DISC_LIST   =  12,   
	LPFC_DISC_AUTH         =  13,   
	LPFC_VPORT_READY       =  32,
};

enum hba_state {
	LPFC_LINK_UNKNOWN    =   0,   
	LPFC_WARM_START      =   1,   
	LPFC_INIT_START      =   2,   
	LPFC_INIT_MBX_CMDS   =   3,   
	LPFC_LINK_DOWN       =   4,   
	LPFC_LINK_UP         =   5,   
	LPFC_CLEAR_LA        =   6,   
	LPFC_HBA_READY       =  32,
	LPFC_HBA_ERROR       =  -1
};

struct lpfc_vport {
	struct lpfc_hba *phba;
	struct list_head listentry;
	uint8_t port_type;
#define LPFC_PHYSICAL_PORT 1
#define LPFC_NPIV_PORT  2
#define LPFC_FABRIC_PORT 3
	enum discovery_state port_state;

	uint16_t vpi;
	uint16_t vfi;
	uint8_t vpi_state;
#define LPFC_VPI_REGISTERED	0x1

	uint32_t fc_flag;	
#define FC_PT2PT                0x1	 
#define FC_PT2PT_PLOGI          0x2	 
#define FC_DISC_TMO             0x4	 
#define FC_PUBLIC_LOOP          0x8	 
#define FC_LBIT                 0x10	 
#define FC_RSCN_MODE            0x20	 
#define FC_NLP_MORE             0x40	 
#define FC_OFFLINE_MODE         0x80	 
#define FC_FABRIC               0x100	 
#define FC_VPORT_LOGO_RCVD      0x200    
#define FC_RSCN_DISCOVERY       0x400	 
#define FC_LOGO_RCVD_DID_CHNG   0x800    
#define FC_SCSI_SCAN_TMO        0x4000	 
#define FC_ABORT_DISCOVERY      0x8000	 
#define FC_NDISC_ACTIVE         0x10000	 
#define FC_BYPASSED_MODE        0x20000	 
#define FC_VPORT_NEEDS_REG_VPI	0x80000  
#define FC_RSCN_DEFERRED	0x100000 
#define FC_VPORT_NEEDS_INIT_VPI 0x200000 
#define FC_VPORT_CVL_RCVD	0x400000 
#define FC_VFI_REGISTERED	0x800000 
#define FC_FDISC_COMPLETED	0x1000000
#define FC_DISC_DELAYED		0x2000000

	uint32_t ct_flags;
#define FC_CT_RFF_ID		0x1	 
#define FC_CT_RNN_ID		0x2	 
#define FC_CT_RSNN_NN		0x4	 
#define FC_CT_RSPN_ID		0x8	 
#define FC_CT_RFT_ID		0x10	 

	struct list_head fc_nodes;

	
	uint16_t fc_plogi_cnt;
	uint16_t fc_adisc_cnt;
	uint16_t fc_reglogin_cnt;
	uint16_t fc_prli_cnt;
	uint16_t fc_unmap_cnt;
	uint16_t fc_map_cnt;
	uint16_t fc_npr_cnt;
	uint16_t fc_unused_cnt;
	struct serv_parm fc_sparam;	

	uint32_t fc_myDID;	
	uint32_t fc_prevDID;	
	struct lpfc_name fabric_portname;
	struct lpfc_name fabric_nodename;

	int32_t stopped;   
	uint8_t fc_linkspeed;	

	uint32_t num_disc_nodes;	

	uint32_t fc_nlp_cnt;	
	uint32_t fc_rscn_id_cnt;	
	uint32_t fc_rscn_flush;		
	struct lpfc_dmabuf *fc_rscn_id_list[FC_MAX_HOLD_RSCN];
	struct lpfc_name fc_nodename;	
	struct lpfc_name fc_portname;	

	struct lpfc_work_evt disc_timeout_evt;

	struct timer_list fc_disctmo;	
	uint8_t fc_ns_retry;	
	uint32_t fc_prli_sent;	

	spinlock_t work_port_lock;
	uint32_t work_port_events; 
#define WORKER_DISC_TMO                0x1	
#define WORKER_ELS_TMO                 0x2	
#define WORKER_FDMI_TMO                0x4	
#define WORKER_DELAYED_DISC_TMO        0x8	

#define WORKER_MBOX_TMO                0x100	
#define WORKER_HB_TMO                  0x200	
#define WORKER_FABRIC_BLOCK_TMO        0x400	
#define WORKER_RAMP_DOWN_QUEUE         0x800	
#define WORKER_RAMP_UP_QUEUE           0x1000	
#define WORKER_SERVICE_TXQ             0x2000	

	struct timer_list fc_fdmitmo;
	struct timer_list els_tmofunc;
	struct timer_list delayed_disc_tmo;

	int unreg_vpi_cmpl;

	uint8_t load_flag;
#define FC_LOADING		0x1	
#define FC_UNLOADING		0x2	
	
	uint32_t cfg_scan_down;
	uint32_t cfg_lun_queue_depth;
	uint32_t cfg_nodev_tmo;
	uint32_t cfg_devloss_tmo;
	uint32_t cfg_restrict_login;
	uint32_t cfg_peer_port_login;
	uint32_t cfg_fcp_class;
	uint32_t cfg_use_adisc;
	uint32_t cfg_fdmi_on;
	uint32_t cfg_discovery_threads;
	uint32_t cfg_log_verbose;
	uint32_t cfg_max_luns;
	uint32_t cfg_enable_da_id;
	uint32_t cfg_max_scsicmpl_time;
	uint32_t cfg_tgt_queue_depth;

	uint32_t dev_loss_tmo_changed;

	struct fc_vport *fc_vport;

#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	struct dentry *debug_disc_trc;
	struct dentry *debug_nodelist;
	struct dentry *vport_debugfs_root;
	struct lpfc_debugfs_trc *disc_trc;
	atomic_t disc_trc_cnt;
#endif
	uint8_t stat_data_enabled;
	uint8_t stat_data_blocked;
	struct list_head rcv_buffer_list;
	unsigned long rcv_buffer_time_stamp;
	uint32_t vport_flag;
#define STATIC_VPORT	1
};

struct hbq_s {
	uint16_t entry_count;	  
	uint16_t buffer_count;	  
	uint32_t next_hbqPutIdx;  
	uint32_t hbqPutIdx;	  
	uint32_t local_hbqGetIdx; 
	void    *hbq_virt;	  
	struct list_head hbq_buffer_list;  
				  
	struct hbq_dmabuf *(*hbq_alloc_buffer) (struct lpfc_hba *);
				  
	void               (*hbq_free_buffer) (struct lpfc_hba *,
					       struct hbq_dmabuf *);
};

#define LPFC_MAX_HBQS  4
#define LPFC_ELS_HBQ	0
#define LPFC_EXTRA_HBQ	1

enum hba_temp_state {
	HBA_NORMAL_TEMP,
	HBA_OVER_TEMP
};

enum intr_type_t {
	NONE = 0,
	INTx,
	MSI,
	MSIX,
};

struct unsol_rcv_ct_ctx {
	uint32_t ctxt_id;
	uint32_t SID;
	uint32_t flags;
#define UNSOL_VALID	0x00000001
	uint16_t oxid;
	uint16_t rxid;
};

#define LPFC_USER_LINK_SPEED_AUTO	0	
#define LPFC_USER_LINK_SPEED_1G		1	
#define LPFC_USER_LINK_SPEED_2G		2	
#define LPFC_USER_LINK_SPEED_4G		4	
#define LPFC_USER_LINK_SPEED_8G		8	
#define LPFC_USER_LINK_SPEED_10G	10	
#define LPFC_USER_LINK_SPEED_16G	16	
#define LPFC_USER_LINK_SPEED_MAX	LPFC_USER_LINK_SPEED_16G
#define LPFC_USER_LINK_SPEED_BITMAP ((1 << LPFC_USER_LINK_SPEED_16G) | \
				     (1 << LPFC_USER_LINK_SPEED_10G) | \
				     (1 << LPFC_USER_LINK_SPEED_8G) | \
				     (1 << LPFC_USER_LINK_SPEED_4G) | \
				     (1 << LPFC_USER_LINK_SPEED_2G) | \
				     (1 << LPFC_USER_LINK_SPEED_1G) | \
				     (1 << LPFC_USER_LINK_SPEED_AUTO))
#define LPFC_LINK_SPEED_STRING "0, 1, 2, 4, 8, 10, 16"

enum nemb_type {
	nemb_mse = 1,
	nemb_hbd
};

enum mbox_type {
	mbox_rd = 1,
	mbox_wr
};

enum dma_type {
	dma_mbox = 1,
	dma_ebuf
};

enum sta_type {
	sta_pre_addr = 1,
	sta_pos_addr
};

struct lpfc_mbox_ext_buf_ctx {
	uint32_t state;
#define LPFC_BSG_MBOX_IDLE		0
#define LPFC_BSG_MBOX_HOST              1
#define LPFC_BSG_MBOX_PORT		2
#define LPFC_BSG_MBOX_DONE		3
#define LPFC_BSG_MBOX_ABTS		4
	enum nemb_type nembType;
	enum mbox_type mboxType;
	uint32_t numBuf;
	uint32_t mbxTag;
	uint32_t seqNum;
	struct lpfc_dmabuf *mbx_dmabuf;
	struct list_head ext_dmabuf_list;
};

struct lpfc_hba {
	
	int (*lpfc_new_scsi_buf)
		(struct lpfc_vport *, int);
	struct lpfc_scsi_buf * (*lpfc_get_scsi_buf)
		(struct lpfc_hba *, struct lpfc_nodelist *);
	int (*lpfc_scsi_prep_dma_buf)
		(struct lpfc_hba *, struct lpfc_scsi_buf *);
	void (*lpfc_scsi_unprep_dma_buf)
		(struct lpfc_hba *, struct lpfc_scsi_buf *);
	void (*lpfc_release_scsi_buf)
		(struct lpfc_hba *, struct lpfc_scsi_buf *);
	void (*lpfc_rampdown_queue_depth)
		(struct lpfc_hba *);
	void (*lpfc_scsi_prep_cmnd)
		(struct lpfc_vport *, struct lpfc_scsi_buf *,
		 struct lpfc_nodelist *);

	
	int (*__lpfc_sli_issue_iocb)
		(struct lpfc_hba *, uint32_t,
		 struct lpfc_iocbq *, uint32_t);
	void (*__lpfc_sli_release_iocbq)(struct lpfc_hba *,
			 struct lpfc_iocbq *);
	int (*lpfc_hba_down_post)(struct lpfc_hba *phba);
	IOCB_t * (*lpfc_get_iocb_from_iocbq)
		(struct lpfc_iocbq *);
	void (*lpfc_scsi_cmd_iocb_cmpl)
		(struct lpfc_hba *, struct lpfc_iocbq *, struct lpfc_iocbq *);

	
	int (*lpfc_sli_issue_mbox)
		(struct lpfc_hba *, LPFC_MBOXQ_t *, uint32_t);

	
	void (*lpfc_sli_handle_slow_ring_event)
		(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
		 uint32_t mask);

	
	int (*lpfc_sli_hbq_to_firmware)
		(struct lpfc_hba *, uint32_t, struct hbq_dmabuf *);
	int (*lpfc_sli_brdrestart)
		(struct lpfc_hba *);
	int (*lpfc_sli_brdready)
		(struct lpfc_hba *, uint32_t);
	void (*lpfc_handle_eratt)
		(struct lpfc_hba *);
	void (*lpfc_stop_port)
		(struct lpfc_hba *);
	int (*lpfc_hba_init_link)
		(struct lpfc_hba *, uint32_t);
	int (*lpfc_hba_down_link)
		(struct lpfc_hba *, uint32_t);
	int (*lpfc_selective_reset)
		(struct lpfc_hba *);

	int (*lpfc_bg_scsi_prep_dma_buf)
		(struct lpfc_hba *, struct lpfc_scsi_buf *);
	

	
	struct lpfc_sli4_hba sli4_hba;

	struct lpfc_sli sli;
	uint8_t pci_dev_grp;	
	uint32_t sli_rev;		
	uint32_t sli3_options;		
#define LPFC_SLI3_HBQ_ENABLED		0x01
#define LPFC_SLI3_NPIV_ENABLED		0x02
#define LPFC_SLI3_VPORT_TEARDOWN	0x04
#define LPFC_SLI3_CRP_ENABLED		0x08
#define LPFC_SLI3_BG_ENABLED		0x20
#define LPFC_SLI3_DSS_ENABLED		0x40
#define LPFC_SLI4_PERFH_ENABLED		0x80
#define LPFC_SLI4_PHWQ_ENABLED		0x100
	uint32_t iocb_cmd_size;
	uint32_t iocb_rsp_size;

	enum hba_state link_state;
	uint32_t link_flag;	
#define LS_LOOPBACK_MODE      0x1	
					
					
#define LS_NPIV_FAB_SUPPORTED 0x2	
#define LS_IGNORE_ERATT       0x4	

	uint32_t hba_flag;	
#define HBA_ERATT_HANDLED	0x1 
#define DEFER_ERATT		0x2 
#define HBA_FCOE_MODE		0x4 
#define HBA_SP_QUEUE_EVT	0x8 
#define HBA_POST_RECEIVE_BUFFER 0x10 
#define FCP_XRI_ABORT_EVENT	0x20
#define ELS_XRI_ABORT_EVENT	0x40
#define ASYNC_EVENT		0x80
#define LINK_DISABLED		0x100 
#define FCF_TS_INPROG           0x200 
#define FCF_RR_INPROG           0x400 
#define HBA_FIP_SUPPORT		0x800 
#define HBA_AER_ENABLED		0x1000 
#define HBA_DEVLOSS_TMO         0x2000 
#define HBA_RRQ_ACTIVE		0x4000 
	uint32_t fcp_ring_in_use; 
	struct lpfc_dmabuf slim2p;

	MAILBOX_t *mbox;
	uint32_t *mbox_ext;
	struct lpfc_mbox_ext_buf_ctx mbox_ext_buf_ctx;
	uint32_t ha_copy;
	struct _PCB *pcb;
	struct _IOCB *IOCBs;

	struct lpfc_dmabuf hbqslimp;

	uint16_t pci_cfg_value;

	uint8_t fc_linkspeed;	

	uint32_t fc_eventTag;	
	uint32_t link_events;

	
	uint32_t fc_pref_DID;	
	uint8_t  fc_pref_ALPA;	
	uint32_t fc_edtovResol; 
	uint32_t fc_edtov;	
	uint32_t fc_arbtov;	
	uint32_t fc_ratov;	
	uint32_t fc_rttov;	
	uint32_t fc_altov;	
	uint32_t fc_crtov;	
	uint32_t fc_citov;	

	struct serv_parm fc_fabparam;	
	uint8_t alpa_map[128];	

	uint32_t lmt;

	uint32_t fc_topology;	

	struct lpfc_stats fc_stat;

	struct lpfc_nodelist fc_fcpnodev; 
	uint32_t nport_event_cnt;	

	uint8_t  wwnn[8];
	uint8_t  wwpn[8];
	uint32_t RandomData[7];

	
	uint32_t cfg_ack0;
	uint32_t cfg_enable_npiv;
	uint32_t cfg_enable_rrq;
	uint32_t cfg_topology;
	uint32_t cfg_link_speed;
#define LPFC_FCF_FOV 1		
#define LPFC_FCF_PRIORITY 2	
	uint32_t cfg_fcf_failover_policy;
	uint32_t cfg_cr_delay;
	uint32_t cfg_cr_count;
	uint32_t cfg_multi_ring_support;
	uint32_t cfg_multi_ring_rctl;
	uint32_t cfg_multi_ring_type;
	uint32_t cfg_poll;
	uint32_t cfg_poll_tmo;
	uint32_t cfg_use_msi;
	uint32_t cfg_fcp_imax;
	uint32_t cfg_fcp_wq_count;
	uint32_t cfg_fcp_eq_count;
	uint32_t cfg_sg_seg_cnt;
	uint32_t cfg_prot_sg_seg_cnt;
	uint32_t cfg_sg_dma_buf_size;
	uint64_t cfg_soft_wwnn;
	uint64_t cfg_soft_wwpn;
	uint32_t cfg_hba_queue_depth;
	uint32_t cfg_enable_hba_reset;
	uint32_t cfg_enable_hba_heartbeat;
	uint32_t cfg_enable_bg;
	uint32_t cfg_hostmem_hgp;
	uint32_t cfg_log_verbose;
	uint32_t cfg_aer_support;
	uint32_t cfg_sriov_nr_virtfn;
	uint32_t cfg_iocb_cnt;
	uint32_t cfg_suppress_link_up;
#define LPFC_INITIALIZE_LINK              0	
#define LPFC_DELAY_INIT_LINK              1	
#define LPFC_DELAY_INIT_LINK_INDEFINITELY 2	
	uint32_t cfg_enable_dss;
	lpfc_vpd_t vpd;		

	struct pci_dev *pcidev;
	struct list_head      work_list;
	uint32_t              work_ha;      
	uint32_t              work_ha_mask; 
	uint32_t              work_hs;      
	uint32_t              work_status[2]; 

	wait_queue_head_t    work_waitq;
	struct task_struct   *worker_thread;
	unsigned long data_flags;

	uint32_t hbq_in_use;		
	struct list_head rb_pend_list;  
	uint32_t hbq_count;	        
	struct hbq_s hbqs[LPFC_MAX_HBQS]; 

	uint32_t fcp_qidx;		

	unsigned long pci_bar0_map;     
	unsigned long pci_bar1_map;     
	unsigned long pci_bar2_map;     
	void __iomem *slim_memmap_p;	
	void __iomem *ctrl_regs_memmap_p;

	void __iomem *MBslimaddr;	
	void __iomem *HAregaddr;	
	void __iomem *CAregaddr;	
	void __iomem *HSregaddr;	
	void __iomem *HCregaddr;	

	struct lpfc_hgp __iomem *host_gp; 
	struct lpfc_pgp   *port_gp;
	uint32_t __iomem  *hbq_put;     
	uint32_t          *hbq_get;     

	int brd_no;			
	char SerialNumber[32];		
	char OptionROMVersion[32];	
	char ModelDesc[256];		
	char ModelName[80];		
	char ProgramType[256];		
	char Port[20];			
	uint8_t vpd_flag;               

#define VPD_MODEL_DESC      0x1         
#define VPD_MODEL_NAME      0x2         
#define VPD_PROGRAM_TYPE    0x4         
#define VPD_PORT            0x8         
#define VPD_MASK            0xf         

	uint8_t soft_wwn_enable;

	struct timer_list fcp_poll_timer;
	struct timer_list eratt_poll;

	uint64_t fc4InputRequests;
	uint64_t fc4OutputRequests;
	uint64_t fc4ControlRequests;
	uint64_t bg_guard_err_cnt;
	uint64_t bg_apptag_err_cnt;
	uint64_t bg_reftag_err_cnt;

	
	spinlock_t scsi_buf_list_lock;
	struct list_head lpfc_scsi_buf_list;
	uint32_t total_scsi_bufs;
	struct list_head lpfc_iocb_list;
	uint32_t total_iocbq_bufs;
	struct list_head active_rrq_list;
	spinlock_t hbalock;

	
	struct pci_pool *lpfc_scsi_dma_buf_pool;
	struct pci_pool *lpfc_mbuf_pool;
	struct pci_pool *lpfc_hrb_pool;	
	struct pci_pool *lpfc_drb_pool; 
	struct pci_pool *lpfc_hbq_pool;	
	struct lpfc_dma_pool lpfc_mbuf_safety_pool;

	mempool_t *mbox_mem_pool;
	mempool_t *nlp_mem_pool;
	mempool_t *rrq_pool;

	struct fc_host_statistics link_stats;
	enum intr_type_t intr_type;
	uint32_t intr_mode;
#define LPFC_INTR_ERROR	0xFFFFFFFF
	struct msix_entry msix_entries[LPFC_MSIX_VECTORS];

	struct list_head port_list;
	struct lpfc_vport *pport;	
	uint16_t max_vpi;		
#define LPFC_MAX_VPI 0xFFFF		
	uint16_t max_vports;            
	uint16_t vpi_base;
	uint16_t vfi_base;
	unsigned long *vpi_bmask;	
	uint16_t *vpi_ids;
	uint16_t vpi_count;
	struct list_head lpfc_vpi_blk_list;

	
	struct list_head fabric_iocb_list;
	atomic_t fabric_iocb_count;
	struct timer_list fabric_block_timer;
	unsigned long bit_flags;
#define	FABRIC_COMANDS_BLOCKED	0
	atomic_t num_rsrc_err;
	atomic_t num_cmd_success;
	unsigned long last_rsrc_error_time;
	unsigned long last_ramp_down_time;
	unsigned long last_ramp_up_time;
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	struct dentry *hba_debugfs_root;
	atomic_t debugfs_vport_count;
	struct dentry *debug_hbqinfo;
	struct dentry *debug_dumpHostSlim;
	struct dentry *debug_dumpHBASlim;
	struct dentry *debug_dumpData;   
	struct dentry *debug_dumpDif;    
	struct dentry *debug_InjErrLBA;  
	struct dentry *debug_InjErrNPortID;  
	struct dentry *debug_InjErrWWPN;  
	struct dentry *debug_writeGuard; 
	struct dentry *debug_writeApp;   
	struct dentry *debug_writeRef;   
	struct dentry *debug_readGuard;  
	struct dentry *debug_readApp;    
	struct dentry *debug_readRef;    

	
	uint32_t lpfc_injerr_wgrd_cnt;
	uint32_t lpfc_injerr_wapp_cnt;
	uint32_t lpfc_injerr_wref_cnt;
	uint32_t lpfc_injerr_rgrd_cnt;
	uint32_t lpfc_injerr_rapp_cnt;
	uint32_t lpfc_injerr_rref_cnt;
	uint32_t lpfc_injerr_nportid;
	struct lpfc_name lpfc_injerr_wwpn;
	sector_t lpfc_injerr_lba;
#define LPFC_INJERR_LBA_OFF	(sector_t)(-1)

	struct dentry *debug_slow_ring_trc;
	struct lpfc_debugfs_trc *slow_ring_trc;
	atomic_t slow_ring_trc_cnt;
	
	struct dentry *idiag_root;
	struct dentry *idiag_pci_cfg;
	struct dentry *idiag_bar_acc;
	struct dentry *idiag_que_info;
	struct dentry *idiag_que_acc;
	struct dentry *idiag_drb_acc;
	struct dentry *idiag_ctl_acc;
	struct dentry *idiag_mbx_acc;
	struct dentry *idiag_ext_acc;
#endif

	
	struct list_head elsbuf;
	int elsbuf_cnt;
	int elsbuf_prev_cnt;

	uint8_t temp_sensor_support;
	
	unsigned long last_completion_time;
	unsigned long skipped_hb;
	struct timer_list hb_tmofunc;
	uint8_t hb_outstanding;
	struct timer_list rrq_tmr;
	enum hba_temp_state over_temp_state;
	
	spinlock_t ndlp_lock;
#define QUE_BUFTAG_BIT  (1<<31)
	uint32_t buffer_tag_count;
	int wait_4_mlo_maint_flg;
	wait_queue_head_t wait_4_mlo_m_q;
	
#define LPFC_NO_BUCKET	   0
#define LPFC_LINEAR_BUCKET 1
#define LPFC_POWER2_BUCKET 2
	uint8_t  bucket_type;
	uint32_t bucket_base;
	uint32_t bucket_step;

#define LPFC_MAX_EVT_COUNT 512
	atomic_t fast_event_count;
	uint32_t fcoe_eventtag;
	uint32_t fcoe_eventtag_at_fcf_scan;
	uint32_t fcoe_cvl_eventtag;
	uint32_t fcoe_cvl_eventtag_attn;
	struct lpfc_fcf fcf;
	uint8_t fc_map[3];
	uint8_t valid_vlan;
	uint16_t vlan_id;
	struct list_head fcf_conn_rec_list;

	spinlock_t ct_ev_lock; 
	struct list_head ct_ev_waiters;
	struct unsol_rcv_ct_ctx ct_ctx[64];
	uint32_t ctx_idx;

	uint8_t menlo_flag;	
#define HBA_MENLO_SUPPORT	0x1 
	uint32_t iocb_cnt;
	uint32_t iocb_max;
	atomic_t sdev_cnt;
	uint8_t fips_spec_rev;
	uint8_t fips_level;
};

static inline struct Scsi_Host *
lpfc_shost_from_vport(struct lpfc_vport *vport)
{
	return container_of((void *) vport, struct Scsi_Host, hostdata[0]);
}

static inline void
lpfc_set_loopback_flag(struct lpfc_hba *phba)
{
	if (phba->cfg_topology == FLAGS_LOCAL_LB)
		phba->link_flag |= LS_LOOPBACK_MODE;
	else
		phba->link_flag &= ~LS_LOOPBACK_MODE;
}

static inline int
lpfc_is_link_up(struct lpfc_hba *phba)
{
	return  phba->link_state == LPFC_LINK_UP ||
		phba->link_state == LPFC_CLEAR_LA ||
		phba->link_state == LPFC_HBA_READY;
}

static inline void
lpfc_worker_wake_up(struct lpfc_hba *phba)
{
	
	set_bit(LPFC_DATA_READY, &phba->data_flags);

	
	wake_up(&phba->work_waitq);
	return;
}

static inline int
lpfc_readl(void __iomem *addr, uint32_t *data)
{
	uint32_t temp;
	temp = readl(addr);
	if (temp == 0xffffffff)
		return -EIO;
	*data = temp;
	return 0;
}

static inline int
lpfc_sli_read_hs(struct lpfc_hba *phba)
{
	phba->sli.slistat.err_attn_event++;

	
	if (lpfc_readl(phba->HSregaddr, &phba->work_hs) ||
		lpfc_readl(phba->MBslimaddr + 0xa8, &phba->work_status[0]) ||
		lpfc_readl(phba->MBslimaddr + 0xac, &phba->work_status[1])) {
		return -EIO;
	}

	
	writel(HA_ERATT, phba->HAregaddr);
	readl(phba->HAregaddr); 
	phba->pport->stopped = 1;

	return 0;
}
