/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2009-2011 Emulex.  All rights reserved.           *
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

#define LPFC_ACTIVE_MBOX_WAIT_CNT               100
#define LPFC_XRI_EXCH_BUSY_WAIT_TMO		10000
#define LPFC_XRI_EXCH_BUSY_WAIT_T1   		10
#define LPFC_XRI_EXCH_BUSY_WAIT_T2              30000
#define LPFC_RELEASE_NOTIFICATION_INTERVAL	32
#define LPFC_RPI_LOW_WATER_MARK			10

#define LPFC_UNREG_FCF                          1
#define LPFC_SKIP_UNREG_FCF                     0

#define LPFC_FCF_REDISCOVER_WAIT_TMO		2000 

#define LPFC_NEMBED_MBOX_SGL_CNT		254

#define LPFC_FN_EQN_MAX       8
#define LPFC_SP_EQN_DEF       1
#define LPFC_FP_EQN_DEF       4
#define LPFC_FP_EQN_MIN       1
#define LPFC_FP_EQN_MAX       (LPFC_FN_EQN_MAX - LPFC_SP_EQN_DEF)

#define LPFC_FN_WQN_MAX       32
#define LPFC_SP_WQN_DEF       1
#define LPFC_FP_WQN_DEF       4
#define LPFC_FP_WQN_MIN       1
#define LPFC_FP_WQN_MAX       (LPFC_FN_WQN_MAX - LPFC_SP_WQN_DEF)

#define LPFC_FCOE_FCF_DEF_INDEX	0
#define LPFC_FCOE_FCF_GET_FIRST	0xFFFF
#define LPFC_FCOE_FCF_NEXT_NONE	0xFFFF

#define LPFC_FCOE_NULL_VID	0xFFF
#define LPFC_FCOE_IGNORE_VID	0xFFFF

#define LPFC_FCOE_FCF_MAC3	0xFF
#define LPFC_FCOE_FCF_MAC4	0xFF
#define LPFC_FCOE_FCF_MAC5	0xFE
#define LPFC_FCOE_FCF_MAP0	0x0E
#define LPFC_FCOE_FCF_MAP1	0xFC
#define LPFC_FCOE_FCF_MAP2	0x00
#define LPFC_FCOE_MAX_RCV_SIZE	0x800
#define LPFC_FCOE_FKA_ADV_PER	0
#define LPFC_FCOE_FIP_PRIORITY	0x80

#define sli4_sid_from_fc_hdr(fc_hdr)  \
	((fc_hdr)->fh_s_id[0] << 16 | \
	 (fc_hdr)->fh_s_id[1] <<  8 | \
	 (fc_hdr)->fh_s_id[2])

#define sli4_fctl_from_fc_hdr(fc_hdr)  \
	((fc_hdr)->fh_f_ctl[0] << 16 | \
	 (fc_hdr)->fh_f_ctl[1] <<  8 | \
	 (fc_hdr)->fh_f_ctl[2])

#define LPFC_FW_RESET_MAXIMUM_WAIT_10MS_CNT 12000

enum lpfc_sli4_queue_type {
	LPFC_EQ,
	LPFC_GCQ,
	LPFC_MCQ,
	LPFC_WCQ,
	LPFC_RCQ,
	LPFC_MQ,
	LPFC_WQ,
	LPFC_HRQ,
	LPFC_DRQ
};

enum lpfc_sli4_queue_subtype {
	LPFC_NONE,
	LPFC_MBOX,
	LPFC_FCP,
	LPFC_ELS,
	LPFC_USOL
};

union sli4_qe {
	void *address;
	struct lpfc_eqe *eqe;
	struct lpfc_cqe *cqe;
	struct lpfc_mcqe *mcqe;
	struct lpfc_wcqe_complete *wcqe_complete;
	struct lpfc_wcqe_release *wcqe_release;
	struct sli4_wcqe_xri_aborted *wcqe_xri_aborted;
	struct lpfc_rcqe_complete *rcqe_complete;
	struct lpfc_mqe *mqe;
	union  lpfc_wqe *wqe;
	struct lpfc_rqe *rqe;
};

struct lpfc_queue {
	struct list_head list;
	enum lpfc_sli4_queue_type type;
	enum lpfc_sli4_queue_subtype subtype;
	struct lpfc_hba *phba;
	struct list_head child_list;
	uint32_t entry_count;	
	uint32_t entry_size;	
	uint32_t entry_repost;	
#define LPFC_QUEUE_MIN_REPOST	8
	uint32_t queue_id;	
	uint32_t assoc_qid;     
	struct list_head page_list;
	uint32_t page_count;	
	uint32_t host_index;	
	uint32_t hba_index;	
	union sli4_qe qe[1];	
};

struct lpfc_sli4_link {
	uint8_t speed;
	uint8_t duplex;
	uint8_t status;
	uint8_t type;
	uint8_t number;
	uint8_t fault;
	uint16_t logical_speed;
	uint16_t topology;
};

struct lpfc_fcf_rec {
	uint8_t  fabric_name[8];
	uint8_t  switch_name[8];
	uint8_t  mac_addr[6];
	uint16_t fcf_indx;
	uint32_t priority;
	uint16_t vlan_id;
	uint32_t addr_mode;
	uint32_t flag;
#define BOOT_ENABLE	0x01
#define RECORD_VALID	0x02
};

struct lpfc_fcf_pri_rec {
	uint16_t fcf_index;
#define LPFC_FCF_ON_PRI_LIST 0x0001
#define LPFC_FCF_FLOGI_FAILED 0x0002
	uint16_t flag;
	uint32_t priority;
};

struct lpfc_fcf_pri {
	struct list_head list;
	struct lpfc_fcf_pri_rec fcf_rec;
};

#define LPFC_SLI4_FCF_TBL_INDX_MAX	32

struct lpfc_fcf {
	uint16_t fcfi;
	uint32_t fcf_flag;
#define FCF_AVAILABLE	0x01 
#define FCF_REGISTERED	0x02 
#define FCF_SCAN_DONE	0x04 
#define FCF_IN_USE	0x08 
#define FCF_INIT_DISC	0x10 
#define FCF_DEAD_DISC	0x20 
#define FCF_ACVL_DISC	0x40 
#define FCF_DISCOVERY	(FCF_INIT_DISC | FCF_DEAD_DISC | FCF_ACVL_DISC)
#define FCF_REDISC_PEND	0x80 
#define FCF_REDISC_EVT	0x100 
#define FCF_REDISC_FOV	0x200 
#define FCF_REDISC_PROG (FCF_REDISC_PEND | FCF_REDISC_EVT)
	uint32_t addr_mode;
	uint32_t eligible_fcf_cnt;
	struct lpfc_fcf_rec current_rec;
	struct lpfc_fcf_rec failover_rec;
	struct list_head fcf_pri_list;
	struct lpfc_fcf_pri fcf_pri[LPFC_SLI4_FCF_TBL_INDX_MAX];
	uint32_t current_fcf_scan_pri;
	struct timer_list redisc_wait;
	unsigned long *fcf_rr_bmask; 
};


#define LPFC_REGION23_SIGNATURE "RG23"
#define LPFC_REGION23_VERSION	1
#define LPFC_REGION23_LAST_REC  0xff
#define DRIVER_SPECIFIC_TYPE	0xA2
#define LINUX_DRIVER_ID		0x20
#define PORT_STE_TYPE		0x1

struct lpfc_fip_param_hdr {
	uint8_t type;
#define FCOE_PARAM_TYPE		0xA0
	uint8_t length;
#define FCOE_PARAM_LENGTH	2
	uint8_t parm_version;
#define FIPP_VERSION		0x01
	uint8_t parm_flags;
#define	lpfc_fip_param_hdr_fipp_mode_SHIFT	6
#define	lpfc_fip_param_hdr_fipp_mode_MASK	0x3
#define lpfc_fip_param_hdr_fipp_mode_WORD	parm_flags
#define	FIPP_MODE_ON				0x1
#define	FIPP_MODE_OFF				0x0
#define FIPP_VLAN_VALID				0x1
};

struct lpfc_fcoe_params {
	uint8_t fc_map[3];
	uint8_t reserved1;
	uint16_t vlan_tag;
	uint8_t reserved[2];
};

struct lpfc_fcf_conn_hdr {
	uint8_t type;
#define FCOE_CONN_TBL_TYPE		0xA1
	uint8_t length;   
	uint8_t reserved[2];
};

struct lpfc_fcf_conn_rec {
	uint16_t flags;
#define	FCFCNCT_VALID		0x0001
#define	FCFCNCT_BOOT		0x0002
#define	FCFCNCT_PRIMARY		0x0004   
#define	FCFCNCT_FBNM_VALID	0x0008
#define	FCFCNCT_SWNM_VALID	0x0010
#define	FCFCNCT_VLAN_VALID	0x0020
#define	FCFCNCT_AM_VALID	0x0040
#define	FCFCNCT_AM_PREFERRED	0x0080   
#define	FCFCNCT_AM_SPMA		0x0100	 

	uint16_t vlan_tag;
	uint8_t fabric_name[8];
	uint8_t switch_name[8];
};

struct lpfc_fcf_conn_entry {
	struct list_head list;
	struct lpfc_fcf_conn_rec conn_rec;
};

struct lpfc_bmbx {
	struct lpfc_dmabuf *dmabuf;
	struct dma_address dma_address;
	void *avirt;
	dma_addr_t aphys;
	uint32_t bmbx_size;
};

#define LPFC_EQE_SIZE LPFC_EQE_SIZE_4

#define LPFC_EQE_SIZE_4B 	4
#define LPFC_EQE_SIZE_16B	16
#define LPFC_CQE_SIZE		16
#define LPFC_WQE_SIZE		64
#define LPFC_MQE_SIZE		256
#define LPFC_RQE_SIZE		8

#define LPFC_EQE_DEF_COUNT	1024
#define LPFC_CQE_DEF_COUNT      1024
#define LPFC_WQE_DEF_COUNT      256
#define LPFC_MQE_DEF_COUNT      16
#define LPFC_RQE_DEF_COUNT	512

#define LPFC_QUEUE_NOARM	false
#define LPFC_QUEUE_REARM	true


#define SLI4_CT_RPI 0
#define SLI4_CT_VPI 1
#define SLI4_CT_VFI 2
#define SLI4_CT_FCFI 3

#define LPFC_SLI4_FL1_MAX_SEGMENT_SIZE	0x10000
#define LPFC_SLI4_FL1_MAX_BUF_SIZE	0X2000
#define LPFC_SLI4_MIN_BUF_SIZE		0x400
#define LPFC_SLI4_MAX_BUF_SIZE		0x20000

struct lpfc_max_cfg_param {
	uint16_t max_xri;
	uint16_t xri_base;
	uint16_t xri_used;
	uint16_t max_rpi;
	uint16_t rpi_base;
	uint16_t rpi_used;
	uint16_t max_vpi;
	uint16_t vpi_base;
	uint16_t vpi_used;
	uint16_t max_vfi;
	uint16_t vfi_base;
	uint16_t vfi_used;
	uint16_t max_fcfi;
	uint16_t fcfi_used;
	uint16_t max_eq;
	uint16_t max_rq;
	uint16_t max_cq;
	uint16_t max_wq;
};

struct lpfc_hba;
struct lpfc_fcp_eq_hdl {
	uint32_t idx;
	struct lpfc_hba *phba;
};

struct lpfc_pc_sli4_params {
	uint32_t supported;
	uint32_t if_type;
	uint32_t sli_rev;
	uint32_t sli_family;
	uint32_t featurelevel_1;
	uint32_t featurelevel_2;
	uint32_t proto_types;
#define LPFC_SLI4_PROTO_FCOE	0x0000001
#define LPFC_SLI4_PROTO_FC	0x0000002
#define LPFC_SLI4_PROTO_NIC	0x0000004
#define LPFC_SLI4_PROTO_ISCSI	0x0000008
#define LPFC_SLI4_PROTO_RDMA	0x0000010
	uint32_t sge_supp_len;
	uint32_t if_page_sz;
	uint32_t rq_db_window;
	uint32_t loopbk_scope;
	uint32_t eq_pages_max;
	uint32_t eqe_size;
	uint32_t cq_pages_max;
	uint32_t cqe_size;
	uint32_t mq_pages_max;
	uint32_t mqe_size;
	uint32_t mq_elem_cnt;
	uint32_t wq_pages_max;
	uint32_t wqe_size;
	uint32_t rq_pages_max;
	uint32_t rqe_size;
	uint32_t hdr_pages_max;
	uint32_t hdr_size;
	uint32_t hdr_pp_align;
	uint32_t sgl_pages_max;
	uint32_t sgl_pp_align;
	uint8_t cqv;
	uint8_t mqv;
	uint8_t wqv;
	uint8_t rqv;
};

struct lpfc_iov {
	uint32_t pf_number;
	uint32_t vf_number;
};

struct lpfc_sli4_lnk_info {
	uint8_t lnk_dv;
#define LPFC_LNK_DAT_INVAL	0
#define LPFC_LNK_DAT_VAL	1
	uint8_t lnk_tp;
#define LPFC_LNK_GE	0x0 
#define LPFC_LNK_FC	0x1 
	uint8_t lnk_no;
};

struct lpfc_sli4_hba {
	void __iomem *conf_regs_memmap_p; 
	void __iomem *ctrl_regs_memmap_p; 
	void __iomem *drbl_regs_memmap_p; 
	union {
		struct {
			
			void __iomem *UERRLOregaddr;
			void __iomem *UERRHIregaddr;
			void __iomem *UEMASKLOregaddr;
			void __iomem *UEMASKHIregaddr;
		} if_type0;
		struct {
			
			void __iomem *STATUSregaddr;
			void __iomem *CTRLregaddr;
			void __iomem *ERR1regaddr;
#define SLIPORT_ERR1_REG_ERR_CODE_1		0x1
#define SLIPORT_ERR1_REG_ERR_CODE_2		0x2
			void __iomem *ERR2regaddr;
#define SLIPORT_ERR2_REG_FW_RESTART		0x0
#define SLIPORT_ERR2_REG_FUNC_PROVISON		0x1
#define SLIPORT_ERR2_REG_FORCED_DUMP		0x2
#define SLIPORT_ERR2_REG_FAILURE_EQ		0x3
#define SLIPORT_ERR2_REG_FAILURE_CQ		0x4
#define SLIPORT_ERR2_REG_FAILURE_BUS		0x5
#define SLIPORT_ERR2_REG_FAILURE_RQ		0x6
		} if_type2;
	} u;

	
	void __iomem *PSMPHRregaddr;

	
	void __iomem *SLIINTFregaddr;

	
	void __iomem *ISRregaddr;	
	void __iomem *IMRregaddr;	
	void __iomem *ISCRregaddr;	
	
	void __iomem *RQDBregaddr;	
	void __iomem *WQDBregaddr;	
	void __iomem *EQCQDBregaddr;	
	void __iomem *MQDBregaddr;	
	void __iomem *BMBXregaddr;	

	uint32_t ue_mask_lo;
	uint32_t ue_mask_hi;
	struct lpfc_register sli_intf;
	struct lpfc_pc_sli4_params pc_sli4_params;
	struct msix_entry *msix_entries;
	uint32_t cfg_eqn;
	uint32_t msix_vec_nr;
	struct lpfc_fcp_eq_hdl *fcp_eq_hdl; 
	
	struct lpfc_queue **fp_eq; 
	struct lpfc_queue *sp_eq;  
	struct lpfc_queue **fcp_wq;
	struct lpfc_queue *mbx_wq; 
	struct lpfc_queue *els_wq; 
	struct lpfc_queue *hdr_rq; 
	struct lpfc_queue *dat_rq; 
	struct lpfc_queue **fcp_cq;
	struct lpfc_queue *mbx_cq; 
	struct lpfc_queue *els_cq; 

	
	int eq_esize;
	int eq_ecount;
	int cq_esize;
	int cq_ecount;
	int wq_esize;
	int wq_ecount;
	int mq_esize;
	int mq_ecount;
	int rq_esize;
	int rq_ecount;
#define LPFC_SP_EQ_MAX_INTR_SEC         10000
#define LPFC_FP_EQ_MAX_INTR_SEC         10000

	uint32_t intr_enable;
	struct lpfc_bmbx bmbx;
	struct lpfc_max_cfg_param max_cfg_param;
	uint16_t extents_in_use; 
	uint16_t rpi_hdrs_in_use; 
	uint16_t next_xri; 
	uint16_t next_rpi;
	uint16_t scsi_xri_max;
	uint16_t scsi_xri_cnt;
	uint16_t scsi_xri_start;
	struct list_head lpfc_free_sgl_list;
	struct list_head lpfc_sgl_list;
	struct lpfc_sglq **lpfc_els_sgl_array;
	struct list_head lpfc_abts_els_sgl_list;
	struct lpfc_scsi_buf **lpfc_scsi_psb_array;
	struct list_head lpfc_abts_scsi_buf_list;
	uint32_t total_sglq_bufs;
	struct lpfc_sglq **lpfc_sglq_active_list;
	struct list_head lpfc_rpi_hdr_list;
	unsigned long *rpi_bmask;
	uint16_t *rpi_ids;
	uint16_t rpi_count;
	struct list_head lpfc_rpi_blk_list;
	unsigned long *xri_bmask;
	uint16_t *xri_ids;
	uint16_t xri_count;
	struct list_head lpfc_xri_blk_list;
	unsigned long *vfi_bmask;
	uint16_t *vfi_ids;
	uint16_t vfi_count;
	struct list_head lpfc_vfi_blk_list;
	struct lpfc_sli4_flags sli4_flags;
	struct list_head sp_queue_event;
	struct list_head sp_cqe_event_pool;
	struct list_head sp_asynce_work_queue;
	struct list_head sp_fcp_xri_aborted_work_queue;
	struct list_head sp_els_xri_aborted_work_queue;
	struct list_head sp_unsol_work_queue;
	struct lpfc_sli4_link link_state;
	struct lpfc_sli4_lnk_info lnk_info;
	uint32_t pport_name_sta;
#define LPFC_SLI4_PPNAME_NON	0
#define LPFC_SLI4_PPNAME_GET	1
	struct lpfc_iov iov;
	spinlock_t abts_scsi_buf_list_lock; 
	spinlock_t abts_sgl_list_lock; 
};

enum lpfc_sge_type {
	GEN_BUFF_TYPE,
	SCSI_BUFF_TYPE
};

enum lpfc_sgl_state {
	SGL_FREED,
	SGL_ALLOCATED,
	SGL_XRI_ABORTED
};

struct lpfc_sglq {
	
	struct list_head list;
	struct list_head clist;
	enum lpfc_sge_type buff_type; 
	enum lpfc_sgl_state state;
	struct lpfc_nodelist *ndlp; 
	uint16_t iotag;         
	uint16_t sli4_lxritag;  
	uint16_t sli4_xritag;   
	struct sli4_sge *sgl;	
	void *virt;		
	dma_addr_t phys;	
};

struct lpfc_rpi_hdr {
	struct list_head list;
	uint32_t len;
	struct lpfc_dmabuf *dmabuf;
	uint32_t page_count;
	uint32_t start_rpi;
};

struct lpfc_rsrc_blks {
	struct list_head list;
	uint16_t rsrc_start;
	uint16_t rsrc_size;
	uint16_t rsrc_used;
};

int lpfc_pci_function_reset(struct lpfc_hba *);
int lpfc_sli4_pdev_status_reg_wait(struct lpfc_hba *);
int lpfc_sli4_hba_setup(struct lpfc_hba *);
int lpfc_sli4_config(struct lpfc_hba *, struct lpfcMboxq *, uint8_t,
		     uint8_t, uint32_t, bool);
void lpfc_sli4_mbox_cmd_free(struct lpfc_hba *, struct lpfcMboxq *);
void lpfc_sli4_mbx_sge_set(struct lpfcMboxq *, uint32_t, dma_addr_t, uint32_t);
void lpfc_sli4_mbx_sge_get(struct lpfcMboxq *, uint32_t,
			   struct lpfc_mbx_sge *);
int lpfc_sli4_mbx_read_fcf_rec(struct lpfc_hba *, struct lpfcMboxq *,
			       uint16_t);

void lpfc_sli4_hba_reset(struct lpfc_hba *);
struct lpfc_queue *lpfc_sli4_queue_alloc(struct lpfc_hba *, uint32_t,
			uint32_t);
void lpfc_sli4_queue_free(struct lpfc_queue *);
uint32_t lpfc_eq_create(struct lpfc_hba *, struct lpfc_queue *, uint16_t);
uint32_t lpfc_cq_create(struct lpfc_hba *, struct lpfc_queue *,
			struct lpfc_queue *, uint32_t, uint32_t);
int32_t lpfc_mq_create(struct lpfc_hba *, struct lpfc_queue *,
		       struct lpfc_queue *, uint32_t);
uint32_t lpfc_wq_create(struct lpfc_hba *, struct lpfc_queue *,
			struct lpfc_queue *, uint32_t);
uint32_t lpfc_rq_create(struct lpfc_hba *, struct lpfc_queue *,
			struct lpfc_queue *, struct lpfc_queue *, uint32_t);
void lpfc_rq_adjust_repost(struct lpfc_hba *, struct lpfc_queue *, int);
uint32_t lpfc_eq_destroy(struct lpfc_hba *, struct lpfc_queue *);
uint32_t lpfc_cq_destroy(struct lpfc_hba *, struct lpfc_queue *);
uint32_t lpfc_mq_destroy(struct lpfc_hba *, struct lpfc_queue *);
uint32_t lpfc_wq_destroy(struct lpfc_hba *, struct lpfc_queue *);
uint32_t lpfc_rq_destroy(struct lpfc_hba *, struct lpfc_queue *,
			 struct lpfc_queue *);
int lpfc_sli4_queue_setup(struct lpfc_hba *);
void lpfc_sli4_queue_unset(struct lpfc_hba *);
int lpfc_sli4_post_sgl(struct lpfc_hba *, dma_addr_t, dma_addr_t, uint16_t);
int lpfc_sli4_repost_scsi_sgl_list(struct lpfc_hba *);
uint16_t lpfc_sli4_next_xritag(struct lpfc_hba *);
int lpfc_sli4_post_async_mbox(struct lpfc_hba *);
int lpfc_sli4_post_els_sgl_list(struct lpfc_hba *phba);
int lpfc_sli4_post_els_sgl_list_ext(struct lpfc_hba *phba);
int lpfc_sli4_post_scsi_sgl_block(struct lpfc_hba *, struct list_head *, int);
int lpfc_sli4_post_scsi_sgl_blk_ext(struct lpfc_hba *, struct list_head *,
				    int);
struct lpfc_cq_event *__lpfc_sli4_cq_event_alloc(struct lpfc_hba *);
struct lpfc_cq_event *lpfc_sli4_cq_event_alloc(struct lpfc_hba *);
void __lpfc_sli4_cq_event_release(struct lpfc_hba *, struct lpfc_cq_event *);
void lpfc_sli4_cq_event_release(struct lpfc_hba *, struct lpfc_cq_event *);
int lpfc_sli4_init_rpi_hdrs(struct lpfc_hba *);
int lpfc_sli4_post_rpi_hdr(struct lpfc_hba *, struct lpfc_rpi_hdr *);
int lpfc_sli4_post_all_rpi_hdrs(struct lpfc_hba *);
struct lpfc_rpi_hdr *lpfc_sli4_create_rpi_hdr(struct lpfc_hba *);
void lpfc_sli4_remove_rpi_hdrs(struct lpfc_hba *);
int lpfc_sli4_alloc_rpi(struct lpfc_hba *);
void lpfc_sli4_free_rpi(struct lpfc_hba *, int);
void lpfc_sli4_remove_rpis(struct lpfc_hba *);
void lpfc_sli4_async_event_proc(struct lpfc_hba *);
void lpfc_sli4_fcf_redisc_event_proc(struct lpfc_hba *);
int lpfc_sli4_resume_rpi(struct lpfc_nodelist *,
			void (*)(struct lpfc_hba *, LPFC_MBOXQ_t *), void *);
void lpfc_sli4_fcp_xri_abort_event_proc(struct lpfc_hba *);
void lpfc_sli4_els_xri_abort_event_proc(struct lpfc_hba *);
void lpfc_sli4_fcp_xri_aborted(struct lpfc_hba *,
			       struct sli4_wcqe_xri_aborted *);
void lpfc_sli4_els_xri_aborted(struct lpfc_hba *,
			       struct sli4_wcqe_xri_aborted *);
void lpfc_sli4_vport_delete_els_xri_aborted(struct lpfc_vport *);
void lpfc_sli4_vport_delete_fcp_xri_aborted(struct lpfc_vport *);
int lpfc_sli4_brdreset(struct lpfc_hba *);
int lpfc_sli4_add_fcf_record(struct lpfc_hba *, struct fcf_record *);
void lpfc_sli_remove_dflt_fcf(struct lpfc_hba *);
int lpfc_sli4_get_els_iocb_cnt(struct lpfc_hba *);
int lpfc_sli4_init_vpi(struct lpfc_vport *);
uint32_t lpfc_sli4_cq_release(struct lpfc_queue *, bool);
uint32_t lpfc_sli4_eq_release(struct lpfc_queue *, bool);
void lpfc_sli4_fcfi_unreg(struct lpfc_hba *, uint16_t);
int lpfc_sli4_fcf_scan_read_fcf_rec(struct lpfc_hba *, uint16_t);
int lpfc_sli4_fcf_rr_read_fcf_rec(struct lpfc_hba *, uint16_t);
int lpfc_sli4_read_fcf_rec(struct lpfc_hba *, uint16_t);
void lpfc_mbx_cmpl_fcf_scan_read_fcf_rec(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_mbx_cmpl_fcf_rr_read_fcf_rec(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_mbx_cmpl_read_fcf_rec(struct lpfc_hba *, LPFC_MBOXQ_t *);
int lpfc_sli4_unregister_fcf(struct lpfc_hba *);
int lpfc_sli4_post_status_check(struct lpfc_hba *);
uint8_t lpfc_sli_config_mbox_subsys_get(struct lpfc_hba *, LPFC_MBOXQ_t *);
uint8_t lpfc_sli_config_mbox_opcode_get(struct lpfc_hba *, LPFC_MBOXQ_t *);
