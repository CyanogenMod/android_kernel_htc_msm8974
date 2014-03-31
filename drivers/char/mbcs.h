/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2005 Silicon Graphics, Inc.  All rights reserved.
 */

#ifndef __MBCS_H__
#define __MBCS_H__

#define MB	(1024*1024)
#define MB2	(2*MB)
#define MB4	(4*MB)
#define MB6	(6*MB)

#define MBCS_CM_ID		0x0000	
#define MBCS_CM_STATUS		0x0008	
#define MBCS_CM_ERROR_DETAIL1	0x0010	
#define MBCS_CM_ERROR_DETAIL2	0x0018	
#define MBCS_CM_CONTROL		0x0020	
#define MBCS_CM_REQ_TOUT	0x0028	
#define MBCS_CM_ERR_INT_DEST	0x0038	
#define MBCS_CM_TARG_FL		0x0050	
#define MBCS_CM_ERR_STAT	0x0060	
#define MBCS_CM_CLR_ERR_STAT	0x0068	
#define MBCS_CM_ERR_INT_EN	0x0070	
#define MBCS_RD_DMA_SYS_ADDR	0x0100	
#define MBCS_RD_DMA_LOC_ADDR	0x0108	
#define MBCS_RD_DMA_CTRL	0x0110	
#define MBCS_RD_DMA_AMO_DEST	0x0118	
#define MBCS_RD_DMA_INT_DEST	0x0120	
#define MBCS_RD_DMA_AUX_STAT	0x0130	
#define MBCS_WR_DMA_SYS_ADDR	0x0200	
#define MBCS_WR_DMA_LOC_ADDR	0x0208	
#define MBCS_WR_DMA_CTRL	0x0210	
#define MBCS_WR_DMA_AMO_DEST	0x0218	
#define MBCS_WR_DMA_INT_DEST	0x0220	
#define MBCS_WR_DMA_AUX_STAT	0x0230	
#define MBCS_ALG_AMO_DEST	0x0300	
#define MBCS_ALG_INT_DEST	0x0308	
#define MBCS_ALG_OFFSETS	0x0310
#define MBCS_ALG_STEP		0x0318	

#define MBCS_GSCR_START		0x0000000
#define MBCS_DEBUG_START	0x0100000
#define MBCS_RAM0_START		0x0200000
#define MBCS_RAM1_START		0x0400000
#define MBCS_RAM2_START		0x0600000

#define MBCS_CM_CONTROL_REQ_TOUT_MASK 0x0000000000ffffffUL

#define MBCS_SRAM_SIZE		(1024*1024)
#define MBCS_CACHELINE_SIZE	128

#define MBCS_MMR_ADDR(mmr_base, offset)((uint64_t *)(mmr_base + offset))
#define MBCS_MMR_SET(mmr_base, offset, value) {			\
	uint64_t *mbcs_mmr_set_u64p, readback;				\
	mbcs_mmr_set_u64p = (uint64_t *)(mmr_base + offset);	\
	*mbcs_mmr_set_u64p = value;					\
	readback = *mbcs_mmr_set_u64p; \
}
#define MBCS_MMR_GET(mmr_base, offset) *(uint64_t *)(mmr_base + offset)
#define MBCS_MMR_ZERO(mmr_base, offset) MBCS_MMR_SET(mmr_base, offset, 0)

union cm_id {
	uint64_t cm_id_reg;
	struct {
		uint64_t always_one:1,	
		 mfg_id:11,	
		 part_num:16,	
		 bitstream_rev:8,	
		:28;		
	};
};

union cm_status {
	uint64_t cm_status_reg;
	struct {
		uint64_t pending_reads:8,	
		 pending_writes:8,	
		 ice_rsp_credits:8,	
		 ice_req_credits:8,	
		 cm_req_credits:8,	
		:1,		
		 rd_dma_in_progress:1,	
		 rd_dma_done:1,	
		:1,		
		 wr_dma_in_progress:1,	
		 wr_dma_done:1,	
		 alg_waiting:1,	
		 alg_pipe_running:1,	
		 alg_done:1,	
		:3,		
		 pending_int_reqs:8,	
		:3,		
		 alg_half_speed_sel:1;	
	};
};

union cm_error_detail1 {
	uint64_t cm_error_detail1_reg;
	struct {
		uint64_t packet_type:4,	
		 source_id:2,	
		 data_size:2,	
		 tnum:8,	
		 byte_enable:8,	
		 gfx_cred:8,	
		 read_type:2,	
		 pio_or_memory:1,	
		 head_cw_error:1,	
		:12,		
		 head_error_bit:1,	
		 data_error_bit:1,	
		:13,		
		 valid:1;	
	};
};

union cm_error_detail2 {
	uint64_t cm_error_detail2_reg;
	struct {
		uint64_t address:56,	
		:8;		
	};
};

union cm_control {
	uint64_t cm_control_reg;
	struct {
		uint64_t cm_id:2,	
		:2,		
		 max_trans:5,	
		:3,		
		 address_mode:1,	
		:7,		
		 credit_limit:8,	
		:5,		
		 rearm_stat_regs:1,	
		 prescalar_byp:1,	
		 force_gap_war:1,	
		 rd_dma_go:1,	
		 wr_dma_go:1,	
		 alg_go:1,	
		 rd_dma_clr:1,	
		 wr_dma_clr:1,	
		 alg_clr:1,	
		:2,		
		 alg_wait_step:1,	
		 alg_done_amo_en:1,	
		 alg_done_int_en:1,	
		:1,		
		 alg_sram0_locked:1,	
		 alg_sram1_locked:1,	
		 alg_sram2_locked:1,	
		 alg_done_clr:1,	
		:12;		
	};
};

union cm_req_timeout {
	uint64_t cm_req_timeout_reg;
	struct {
		uint64_t time_out:24,	
		:40;		
	};
};

union intr_dest {
	uint64_t intr_dest_reg;
	struct {
		uint64_t address:56,	
		 int_vector:8;	
	};
};

union cm_error_status {
	uint64_t cm_error_status_reg;
	struct {
		uint64_t ecc_sbe:1,	
		 ecc_mbe:1,	
		 unsupported_req:1,	
		 unexpected_rsp:1,	
		 bad_length:1,	
		 bad_datavalid:1,	
		 buffer_overflow:1,	
		 request_timeout:1,	
		:8,		
		 head_inv_data_size:1,	
		 rsp_pactype_inv:1,	
		 head_sb_err:1,	
		 missing_head:1,	
		 head_inv_rd_type:1,	
		 head_cmd_err_bit:1,	
		 req_addr_align_inv:1,	
		 pio_req_addr_inv:1,	
		 req_range_dsize_inv:1,	
		 early_term:1,	
		 early_tail:1,	
		 missing_tail:1,	
		 data_flit_sb_err:1,	
		 cm2hcm_req_cred_of:1,	
		 cm2hcm_rsp_cred_of:1,	
		 rx_bad_didn:1,	
		 rd_dma_err_rsp:1,	
		 rd_dma_tnum_tout:1,	
		 rd_dma_multi_tnum_tou:1,	
		 wr_dma_err_rsp:1,	
		 wr_dma_tnum_tout:1,	
		 wr_dma_multi_tnum_tou:1,	
		 alg_data_overflow:1,	
		 alg_data_underflow:1,	
		 ram0_access_conflict:1,	
		 ram1_access_conflict:1,	
		 ram2_access_conflict:1,	
		 ram0_perr:1,	
		 ram1_perr:1,	
		 ram2_perr:1,	
		 int_gen_rsp_err:1,	
		 int_gen_tnum_tout:1,	
		 rd_dma_prog_err:1,	
		 wr_dma_prog_err:1,	
		:14;		
	};
};

union cm_clr_error_status {
	uint64_t cm_clr_error_status_reg;
	struct {
		uint64_t clr_ecc_sbe:1,	
		 clr_ecc_mbe:1,	
		 clr_unsupported_req:1,	
		 clr_unexpected_rsp:1,	
		 clr_bad_length:1,	
		 clr_bad_datavalid:1,	
		 clr_buffer_overflow:1,	
		 clr_request_timeout:1,	
		:8,		
		 clr_head_inv_data_siz:1,	
		 clr_rsp_pactype_inv:1,	
		 clr_head_sb_err:1,	
		 clr_missing_head:1,	
		 clr_head_inv_rd_type:1,	
		 clr_head_cmd_err_bit:1,	
		 clr_req_addr_align_in:1,	
		 clr_pio_req_addr_inv:1,	
		 clr_req_range_dsize_i:1,	
		 clr_early_term:1,	
		 clr_early_tail:1,	
		 clr_missing_tail:1,	
		 clr_data_flit_sb_err:1,	
		 clr_cm2hcm_req_cred_o:1,	
		 clr_cm2hcm_rsp_cred_o:1,	
		 clr_rx_bad_didn:1,	
		 clr_rd_dma_err_rsp:1,	
		 clr_rd_dma_tnum_tout:1,	
		 clr_rd_dma_multi_tnum:1,	
		 clr_wr_dma_err_rsp:1,	
		 clr_wr_dma_tnum_tout:1,	
		 clr_wr_dma_multi_tnum:1,	
		 clr_alg_data_overflow:1,	
		 clr_alg_data_underflo:1,	
		 clr_ram0_access_confl:1,	
		 clr_ram1_access_confl:1,	
		 clr_ram2_access_confl:1,	
		 clr_ram0_perr:1,	
		 clr_ram1_perr:1,	
		 clr_ram2_perr:1,	
		 clr_int_gen_rsp_err:1,	
		 clr_int_gen_tnum_tout:1,	
		 clr_rd_dma_prog_err:1,	
		 clr_wr_dma_prog_err:1,	
		:14;		
	};
};

union cm_error_intr_enable {
	uint64_t cm_error_intr_enable_reg;
	struct {
		uint64_t int_en_ecc_sbe:1,	
		 int_en_ecc_mbe:1,	
		 int_en_unsupported_re:1,	
		 int_en_unexpected_rsp:1,	
		 int_en_bad_length:1,	
		 int_en_bad_datavalid:1,	
		 int_en_buffer_overflo:1,	
		 int_en_request_timeou:1,	
		:8,		
		 int_en_head_inv_data_:1,	
		 int_en_rsp_pactype_in:1,	
		 int_en_head_sb_err:1,	
		 int_en_missing_head:1,	
		 int_en_head_inv_rd_ty:1,	
		 int_en_head_cmd_err_b:1,	
		 int_en_req_addr_align:1,	
		 int_en_pio_req_addr_i:1,	
		 int_en_req_range_dsiz:1,	
		 int_en_early_term:1,	
		 int_en_early_tail:1,	
		 int_en_missing_tail:1,	
		 int_en_data_flit_sb_e:1,	
		 int_en_cm2hcm_req_cre:1,	
		 int_en_cm2hcm_rsp_cre:1,	
		 int_en_rx_bad_didn:1,	
		 int_en_rd_dma_err_rsp:1,	
		 int_en_rd_dma_tnum_to:1,	
		 int_en_rd_dma_multi_t:1,	
		 int_en_wr_dma_err_rsp:1,	
		 int_en_wr_dma_tnum_to:1,	
		 int_en_wr_dma_multi_t:1,	
		 int_en_alg_data_overf:1,	
		 int_en_alg_data_under:1,	
		 int_en_ram0_access_co:1,	
		 int_en_ram1_access_co:1,	
		 int_en_ram2_access_co:1,	
		 int_en_ram0_perr:1,	
		 int_en_ram1_perr:1,	
		 int_en_ram2_perr:1,	
		 int_en_int_gen_rsp_er:1,	
		 int_en_int_gen_tnum_t:1,	
		 int_en_rd_dma_prog_er:1,	
		 int_en_wr_dma_prog_er:1,	
		:14;		
	};
};

struct cm_mmr {
	union cm_id id;
	union cm_status status;
	union cm_error_detail1 err_detail1;
	union cm_error_detail2 err_detail2;
	union cm_control control;
	union cm_req_timeout req_timeout;
	uint64_t reserved1[1];
	union intr_dest int_dest;
	uint64_t reserved2[2];
	uint64_t targ_flush;
	uint64_t reserved3[1];
	union cm_error_status err_status;
	union cm_clr_error_status clr_err_status;
	union cm_error_intr_enable int_enable;
};

union dma_hostaddr {
	uint64_t dma_hostaddr_reg;
	struct {
		uint64_t dma_sys_addr:56,	
		:8;		
	};
};

union dma_localaddr {
	uint64_t dma_localaddr_reg;
	struct {
		uint64_t dma_ram_addr:21,	
		 dma_ram_sel:2,	
		:41;		
	};
};

union dma_control {
	uint64_t dma_control_reg;
	struct {
		uint64_t dma_op_length:16,	
		:18,		
		 done_amo_en:1,	
		 done_int_en:1,	
		:1,		
		 pio_mem_n:1,	
		:26;		
	};
};

union dma_amo_dest {
	uint64_t dma_amo_dest_reg;
	struct {
		uint64_t dma_amo_sys_addr:56,	
		 dma_amo_mod_type:3,	
		:5;		
	};
};

union rdma_aux_status {
	uint64_t rdma_aux_status_reg;
	struct {
		uint64_t op_num_pacs_left:17,	
		:5,		
		 lrsp_buff_empty:1,	
		:17,		
		 pending_reqs_left:6,	
		:18;		
	};
};

struct rdma_mmr {
	union dma_hostaddr host_addr;
	union dma_localaddr local_addr;
	union dma_control control;
	union dma_amo_dest amo_dest;
	union intr_dest intr_dest;
	union rdma_aux_status aux_status;
};

union wdma_aux_status {
	uint64_t wdma_aux_status_reg;
	struct {
		uint64_t op_num_pacs_left:17,	
		:4,		
		 lreq_buff_empty:1,	
		:18,		
		 pending_reqs_left:6,	
		:18;		
	};
};

struct wdma_mmr {
	union dma_hostaddr host_addr;
	union dma_localaddr local_addr;
	union dma_control control;
	union dma_amo_dest amo_dest;
	union intr_dest intr_dest;
	union wdma_aux_status aux_status;
};

union algo_step {
	uint64_t algo_step_reg;
	struct {
		uint64_t alg_step_cnt:16,	
		:48;		
	};
};

struct algo_mmr {
	union dma_amo_dest amo_dest;
	union intr_dest intr_dest;
	union {
		uint64_t algo_offset_reg;
		struct {
			uint64_t sram0_offset:7,	
			reserved0:1,	
			sram1_offset:7,	
			reserved1:1,	
			sram2_offset:7,	
			reserved2:14;	
		};
	} sram_offset;
	union algo_step step;
};

struct mbcs_mmr {
	struct cm_mmr cm;
	uint64_t reserved1[17];
	struct rdma_mmr rdDma;
	uint64_t reserved2[25];
	struct wdma_mmr wrDma;
	uint64_t reserved3[25];
	struct algo_mmr algo;
	uint64_t reserved4[156];
};

#define DEVICE_NAME "mbcs"
#define MBCS_PART_NUM 0xfff0
#define MBCS_PART_NUM_ALG0 0xf001
#define MBCS_MFG_NUM  0x1

struct algoblock {
	uint64_t amoHostDest;
	uint64_t amoModType;
	uint64_t intrHostDest;
	uint64_t intrVector;
	uint64_t algoStepCount;
};

struct getdma {
	uint64_t hostAddr;
	uint64_t localAddr;
	uint64_t bytes;
	uint64_t DoneAmoEnable;
	uint64_t DoneIntEnable;
	uint64_t peerIO;
	uint64_t amoHostDest;
	uint64_t amoModType;
	uint64_t intrHostDest;
	uint64_t intrVector;
};

struct putdma {
	uint64_t hostAddr;
	uint64_t localAddr;
	uint64_t bytes;
	uint64_t DoneAmoEnable;
	uint64_t DoneIntEnable;
	uint64_t peerIO;
	uint64_t amoHostDest;
	uint64_t amoModType;
	uint64_t intrHostDest;
	uint64_t intrVector;
};

struct mbcs_soft {
	struct list_head list;
	struct cx_dev *cxdev;
	int major;
	int nasid;
	void *mmr_base;
	wait_queue_head_t dmawrite_queue;
	wait_queue_head_t dmaread_queue;
	wait_queue_head_t algo_queue;
	struct sn_irq_info *get_sn_irq;
	struct sn_irq_info *put_sn_irq;
	struct sn_irq_info *algo_sn_irq;
	struct getdma getdma;
	struct putdma putdma;
	struct algoblock algo;
	uint64_t gscr_addr;	
	uint64_t ram0_addr;	
	uint64_t ram1_addr;	
	uint64_t ram2_addr;	
	uint64_t debug_addr;	
	atomic_t dmawrite_done;
	atomic_t dmaread_done;
	atomic_t algo_done;
	struct mutex dmawritelock;
	struct mutex dmareadlock;
	struct mutex algolock;
};

static int mbcs_open(struct inode *ip, struct file *fp);
static ssize_t mbcs_sram_read(struct file *fp, char __user *buf, size_t len,
			      loff_t * off);
static ssize_t mbcs_sram_write(struct file *fp, const char __user *buf, size_t len,
			       loff_t * off);
static loff_t mbcs_sram_llseek(struct file *filp, loff_t off, int whence);
static int mbcs_gscr_mmap(struct file *fp, struct vm_area_struct *vma);

#endif				
