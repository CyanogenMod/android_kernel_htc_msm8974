/*
 * CAAM/SEC 4.x driver backend
 * Private/internal definitions between modules
 *
 * Copyright 2008-2011 Freescale Semiconductor, Inc.
 *
 */

#ifndef INTERN_H
#define INTERN_H

#define JOBR_UNASSIGNED 0
#define JOBR_ASSIGNED 1

#define JOBR_DEPTH (1 << CONFIG_CRYPTO_DEV_FSL_CAAM_RINGSIZE)

#ifdef CONFIG_CRYPTO_DEV_FSL_CAAM_INTC
#define JOBR_INTC JRCFG_ICEN
#define JOBR_INTC_TIME_THLD CONFIG_CRYPTO_DEV_FSL_CAAM_INTC_TIME_THLD
#define JOBR_INTC_COUNT_THLD CONFIG_CRYPTO_DEV_FSL_CAAM_INTC_COUNT_THLD
#else
#define JOBR_INTC 0
#define JOBR_INTC_TIME_THLD 0
#define JOBR_INTC_COUNT_THLD 0
#endif

struct caam_jrentry_info {
	void (*callbk)(struct device *dev, u32 *desc, u32 status, void *arg);
	void *cbkarg;	
	u32 *desc_addr_virt;	
	dma_addr_t desc_addr_dma;	
	u32 desc_size;	
};

struct caam_drv_private_jr {
	struct device *parentdev;	
	int ridx;
	struct caam_job_ring __iomem *rregs;	
	struct tasklet_struct irqtask[NR_CPUS];
	int irq;			
	int assign;			

	
	int ringsize;	
	struct caam_jrentry_info *entinfo;	
	spinlock_t inplock ____cacheline_aligned; 
	int inp_ring_write_index;	
	int head;			
	dma_addr_t *inpring;	
	spinlock_t outlock ____cacheline_aligned; 
	int out_ring_read_index;	
	int tail;			
	struct jr_outentry *outring;	
};

struct caam_drv_private {

	struct device *dev;
	struct device **jrdev; 
	spinlock_t jr_alloc_lock;
	struct platform_device *pdev;

	
	struct caam_ctrl *ctrl; 
	struct caam_deco **deco; 
	struct caam_assurance *ac;
	struct caam_queue_if *qi; 

	u8 total_jobrs;		
	u8 qi_present;		
	int secvio_irq;		

	
	atomic_t tfm_count ____cacheline_aligned;
	int num_jrs_for_algapi;
	struct device **algapi_jr;
	
	struct list_head alg_list;

#ifdef CONFIG_DEBUG_FS
	struct dentry *dfs_root;
	struct dentry *ctl; 
	struct dentry *ctl_rq_dequeued, *ctl_ob_enc_req, *ctl_ib_dec_req;
	struct dentry *ctl_ob_enc_bytes, *ctl_ob_prot_bytes;
	struct dentry *ctl_ib_dec_bytes, *ctl_ib_valid_bytes;
	struct dentry *ctl_faultaddr, *ctl_faultdetail, *ctl_faultstatus;

	struct debugfs_blob_wrapper ctl_kek_wrap, ctl_tkek_wrap, ctl_tdsk_wrap;
	struct dentry *ctl_kek, *ctl_tkek, *ctl_tdsk;
#endif
};

void caam_jr_algapi_init(struct device *dev);
void caam_jr_algapi_remove(struct device *dev);
#endif 
