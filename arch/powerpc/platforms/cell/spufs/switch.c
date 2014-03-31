/*
 * spu_switch.c
 *
 * (C) Copyright IBM Corp. 2005
 *
 * Author: Mark Nutter <mnutter@us.ibm.com>
 *
 * Host-side part of SPU context switch sequence outlined in
 * Synergistic Processor Element, Book IV.
 *
 * A fully premptive switch of an SPE is very expensive in terms
 * of time and system resources.  SPE Book IV indicates that SPE
 * allocation should follow a "serially reusable device" model,
 * in which the SPE is assigned a task until it completes.  When
 * this is not possible, this sequence may be used to premptively
 * save, and then later (optionally) restore the context of a
 * program executing on an SPE.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/export.h>
#include <linux/errno.h>
#include <linux/hardirq.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/smp.h>
#include <linux/stddef.h>
#include <linux/unistd.h>

#include <asm/io.h>
#include <asm/spu.h>
#include <asm/spu_priv1.h>
#include <asm/spu_csa.h>
#include <asm/mmu_context.h>

#include "spufs.h"

#include "spu_save_dump.h"
#include "spu_restore_dump.h"

#if 0
#define POLL_WHILE_TRUE(_c) {				\
    do {						\
    } while (_c);					\
  }
#else
#define RELAX_SPIN_COUNT				1000
#define POLL_WHILE_TRUE(_c) {				\
    do {						\
	int _i;						\
	for (_i=0; _i<RELAX_SPIN_COUNT && (_c); _i++) { \
	    cpu_relax();				\
	}						\
	if (unlikely(_c)) yield();			\
	else break;					\
    } while (_c);					\
  }
#endif				

#define POLL_WHILE_FALSE(_c)	POLL_WHILE_TRUE(!(_c))

static inline void acquire_spu_lock(struct spu *spu)
{
}

static inline void release_spu_lock(struct spu *spu)
{
}

static inline int check_spu_isolate(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;
	u32 isolate_state;

	isolate_state = SPU_STATUS_ISOLATED_STATE |
	    SPU_STATUS_ISOLATED_LOAD_STATUS | SPU_STATUS_ISOLATED_EXIT_STATUS;
	return (in_be32(&prob->spu_status_R) & isolate_state) ? 1 : 0;
}

static inline void disable_interrupts(struct spu_state *csa, struct spu *spu)
{
	spin_lock_irq(&spu->register_lock);
	if (csa) {
		csa->priv1.int_mask_class0_RW = spu_int_mask_get(spu, 0);
		csa->priv1.int_mask_class1_RW = spu_int_mask_get(spu, 1);
		csa->priv1.int_mask_class2_RW = spu_int_mask_get(spu, 2);
	}
	spu_int_mask_set(spu, 0, 0ul);
	spu_int_mask_set(spu, 1, 0ul);
	spu_int_mask_set(spu, 2, 0ul);
	eieio();
	spin_unlock_irq(&spu->register_lock);

	set_bit(SPU_CONTEXT_SWITCH_PENDING, &spu->flags);
	clear_bit(SPU_CONTEXT_FAULT_PENDING, &spu->flags);
	synchronize_irq(spu->irqs[0]);
	synchronize_irq(spu->irqs[1]);
	synchronize_irq(spu->irqs[2]);
}

static inline void set_watchdog_timer(struct spu_state *csa, struct spu *spu)
{
}

static inline void inhibit_user_access(struct spu_state *csa, struct spu *spu)
{
}

static inline void set_switch_pending(struct spu_state *csa, struct spu *spu)
{
}

static inline void save_mfc_cntl(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	switch (in_be64(&priv2->mfc_control_RW) &
	       MFC_CNTL_SUSPEND_DMA_STATUS_MASK) {
	case MFC_CNTL_SUSPEND_IN_PROGRESS:
		POLL_WHILE_FALSE((in_be64(&priv2->mfc_control_RW) &
				  MFC_CNTL_SUSPEND_DMA_STATUS_MASK) ==
				 MFC_CNTL_SUSPEND_COMPLETE);
		
	case MFC_CNTL_SUSPEND_COMPLETE:
		if (csa)
			csa->priv2.mfc_control_RW =
				in_be64(&priv2->mfc_control_RW) |
				MFC_CNTL_SUSPEND_DMA_QUEUE;
		break;
	case MFC_CNTL_NORMAL_DMA_QUEUE_OPERATION:
		out_be64(&priv2->mfc_control_RW, MFC_CNTL_SUSPEND_DMA_QUEUE);
		POLL_WHILE_FALSE((in_be64(&priv2->mfc_control_RW) &
				  MFC_CNTL_SUSPEND_DMA_STATUS_MASK) ==
				 MFC_CNTL_SUSPEND_COMPLETE);
		if (csa)
			csa->priv2.mfc_control_RW =
				in_be64(&priv2->mfc_control_RW) &
				~MFC_CNTL_SUSPEND_DMA_QUEUE &
				~MFC_CNTL_SUSPEND_MASK;
		break;
	}
}

static inline void save_spu_runcntl(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;

	csa->prob.spu_runcntl_RW = in_be32(&prob->spu_runcntl_RW);
}

static inline void save_mfc_sr1(struct spu_state *csa, struct spu *spu)
{
	csa->priv1.mfc_sr1_RW = spu_mfc_sr1_get(spu);
}

static inline void save_spu_status(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;

	if ((in_be32(&prob->spu_status_R) & SPU_STATUS_RUNNING) == 0) {
		csa->prob.spu_status_R = in_be32(&prob->spu_status_R);
	} else {
		u32 stopped;

		out_be32(&prob->spu_runcntl_RW, SPU_RUNCNTL_STOP);
		eieio();
		POLL_WHILE_TRUE(in_be32(&prob->spu_status_R) &
				SPU_STATUS_RUNNING);
		stopped =
		    SPU_STATUS_INVALID_INSTR | SPU_STATUS_SINGLE_STEP |
		    SPU_STATUS_STOPPED_BY_HALT | SPU_STATUS_STOPPED_BY_STOP;
		if ((in_be32(&prob->spu_status_R) & stopped) == 0)
			csa->prob.spu_status_R = SPU_STATUS_RUNNING;
		else
			csa->prob.spu_status_R = in_be32(&prob->spu_status_R);
	}
}

static inline void save_mfc_stopped_status(struct spu_state *csa,
		struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;
	const u64 mask = MFC_CNTL_DECREMENTER_RUNNING |
			MFC_CNTL_DMA_QUEUES_EMPTY;

	csa->priv2.mfc_control_RW &= ~mask;
	csa->priv2.mfc_control_RW |= in_be64(&priv2->mfc_control_RW) & mask;
}

static inline void halt_mfc_decr(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	out_be64(&priv2->mfc_control_RW,
		 MFC_CNTL_DECREMENTER_HALTED | MFC_CNTL_SUSPEND_MASK);
	eieio();
}

static inline void save_timebase(struct spu_state *csa, struct spu *spu)
{
	csa->suspend_time = get_cycles();
}

static inline void remove_other_spu_access(struct spu_state *csa,
					   struct spu *spu)
{
}

static inline void do_mfc_mssync(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;

	out_be64(&prob->spc_mssync_RW, 1UL);
	POLL_WHILE_TRUE(in_be64(&prob->spc_mssync_RW) & MS_SYNC_PENDING);
}

static inline void issue_mfc_tlbie(struct spu_state *csa, struct spu *spu)
{
	spu_tlb_invalidate(spu);
	mb();
}

static inline void handle_pending_interrupts(struct spu_state *csa,
					     struct spu *spu)
{
}

static inline void save_mfc_queues(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;
	int i;

	if ((in_be64(&priv2->mfc_control_RW) & MFC_CNTL_DMA_QUEUES_EMPTY) == 0) {
		for (i = 0; i < 8; i++) {
			csa->priv2.puq[i].mfc_cq_data0_RW =
			    in_be64(&priv2->puq[i].mfc_cq_data0_RW);
			csa->priv2.puq[i].mfc_cq_data1_RW =
			    in_be64(&priv2->puq[i].mfc_cq_data1_RW);
			csa->priv2.puq[i].mfc_cq_data2_RW =
			    in_be64(&priv2->puq[i].mfc_cq_data2_RW);
			csa->priv2.puq[i].mfc_cq_data3_RW =
			    in_be64(&priv2->puq[i].mfc_cq_data3_RW);
		}
		for (i = 0; i < 16; i++) {
			csa->priv2.spuq[i].mfc_cq_data0_RW =
			    in_be64(&priv2->spuq[i].mfc_cq_data0_RW);
			csa->priv2.spuq[i].mfc_cq_data1_RW =
			    in_be64(&priv2->spuq[i].mfc_cq_data1_RW);
			csa->priv2.spuq[i].mfc_cq_data2_RW =
			    in_be64(&priv2->spuq[i].mfc_cq_data2_RW);
			csa->priv2.spuq[i].mfc_cq_data3_RW =
			    in_be64(&priv2->spuq[i].mfc_cq_data3_RW);
		}
	}
}

static inline void save_ppu_querymask(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;

	csa->prob.dma_querymask_RW = in_be32(&prob->dma_querymask_RW);
}

static inline void save_ppu_querytype(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;

	csa->prob.dma_querytype_RW = in_be32(&prob->dma_querytype_RW);
}

static inline void save_ppu_tagstatus(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;

	csa->prob.dma_tagstatus_R = in_be32(&prob->dma_tagstatus_R);
}

static inline void save_mfc_csr_tsq(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	csa->priv2.spu_tag_status_query_RW =
	    in_be64(&priv2->spu_tag_status_query_RW);
}

static inline void save_mfc_csr_cmd(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	csa->priv2.spu_cmd_buf1_RW = in_be64(&priv2->spu_cmd_buf1_RW);
	csa->priv2.spu_cmd_buf2_RW = in_be64(&priv2->spu_cmd_buf2_RW);
}

static inline void save_mfc_csr_ato(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	csa->priv2.spu_atomic_status_RW = in_be64(&priv2->spu_atomic_status_RW);
}

static inline void save_mfc_tclass_id(struct spu_state *csa, struct spu *spu)
{
	csa->priv1.mfc_tclass_id_RW = spu_mfc_tclass_id_get(spu);
}

static inline void set_mfc_tclass_id(struct spu_state *csa, struct spu *spu)
{
	spu_mfc_tclass_id_set(spu, 0x10000000);
	eieio();
}

static inline void purge_mfc_queue(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	out_be64(&priv2->mfc_control_RW,
			MFC_CNTL_PURGE_DMA_REQUEST |
			MFC_CNTL_SUSPEND_MASK);
	eieio();
}

static inline void wait_purge_complete(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	POLL_WHILE_FALSE((in_be64(&priv2->mfc_control_RW) &
			 MFC_CNTL_PURGE_DMA_STATUS_MASK) ==
			 MFC_CNTL_PURGE_DMA_COMPLETE);
}

static inline void setup_mfc_sr1(struct spu_state *csa, struct spu *spu)
{
	spu_mfc_sr1_set(spu, (MFC_STATE1_MASTER_RUN_CONTROL_MASK |
			      MFC_STATE1_RELOCATE_MASK |
			      MFC_STATE1_BUS_TLBIE_MASK));
}

static inline void save_spu_npc(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;

	csa->prob.spu_npc_RW = in_be32(&prob->spu_npc_RW);
}

static inline void save_spu_privcntl(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	csa->priv2.spu_privcntl_RW = in_be64(&priv2->spu_privcntl_RW);
}

static inline void reset_spu_privcntl(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	out_be64(&priv2->spu_privcntl_RW, 0UL);
	eieio();
}

static inline void save_spu_lslr(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	csa->priv2.spu_lslr_RW = in_be64(&priv2->spu_lslr_RW);
}

static inline void reset_spu_lslr(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	out_be64(&priv2->spu_lslr_RW, LS_ADDR_MASK);
	eieio();
}

static inline void save_spu_cfg(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	csa->priv2.spu_cfg_RW = in_be64(&priv2->spu_cfg_RW);
}

static inline void save_pm_trace(struct spu_state *csa, struct spu *spu)
{
}

static inline void save_mfc_rag(struct spu_state *csa, struct spu *spu)
{
	csa->priv1.resource_allocation_groupID_RW =
		spu_resource_allocation_groupID_get(spu);
	csa->priv1.resource_allocation_enable_RW =
		spu_resource_allocation_enable_get(spu);
}

static inline void save_ppu_mb_stat(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;

	csa->prob.mb_stat_R = in_be32(&prob->mb_stat_R);
}

static inline void save_ppu_mb(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;

	csa->prob.pu_mb_R = in_be32(&prob->pu_mb_R);
}

static inline void save_ppuint_mb(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	csa->priv2.puint_mb_R = in_be64(&priv2->puint_mb_R);
}

static inline void save_ch_part1(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;
	u64 idx, ch_indices[] = { 0UL, 3UL, 4UL, 24UL, 25UL, 27UL };
	int i;


	
	out_be64(&priv2->spu_chnlcntptr_RW, 1);
	csa->spu_chnldata_RW[1] = in_be64(&priv2->spu_chnldata_RW);

	
	for (i = 0; i < ARRAY_SIZE(ch_indices); i++) {
		idx = ch_indices[i];
		out_be64(&priv2->spu_chnlcntptr_RW, idx);
		eieio();
		csa->spu_chnldata_RW[idx] = in_be64(&priv2->spu_chnldata_RW);
		csa->spu_chnlcnt_RW[idx] = in_be64(&priv2->spu_chnlcnt_RW);
		out_be64(&priv2->spu_chnldata_RW, 0UL);
		out_be64(&priv2->spu_chnlcnt_RW, 0UL);
		eieio();
	}
}

static inline void save_spu_mb(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;
	int i;

	out_be64(&priv2->spu_chnlcntptr_RW, 29UL);
	eieio();
	csa->spu_chnlcnt_RW[29] = in_be64(&priv2->spu_chnlcnt_RW);
	for (i = 0; i < 4; i++) {
		csa->spu_mailbox_data[i] = in_be64(&priv2->spu_chnldata_RW);
	}
	out_be64(&priv2->spu_chnlcnt_RW, 0UL);
	eieio();
}

static inline void save_mfc_cmd(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	out_be64(&priv2->spu_chnlcntptr_RW, 21UL);
	eieio();
	csa->spu_chnlcnt_RW[21] = in_be64(&priv2->spu_chnlcnt_RW);
	eieio();
}

static inline void reset_ch(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;
	u64 ch_indices[4] = { 21UL, 23UL, 28UL, 30UL };
	u64 ch_counts[4] = { 16UL, 1UL, 1UL, 1UL };
	u64 idx;
	int i;

	for (i = 0; i < 4; i++) {
		idx = ch_indices[i];
		out_be64(&priv2->spu_chnlcntptr_RW, idx);
		eieio();
		out_be64(&priv2->spu_chnlcnt_RW, ch_counts[i]);
		eieio();
	}
}

static inline void resume_mfc_queue(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	out_be64(&priv2->mfc_control_RW, MFC_CNTL_RESUME_DMA_QUEUE);
}

static inline void setup_mfc_slbs(struct spu_state *csa, struct spu *spu,
		unsigned int *code, int code_size)
{
	spu_invalidate_slbs(spu);
	spu_setup_kernel_slbs(spu, csa->lscsa, code, code_size);
}

static inline void set_switch_active(struct spu_state *csa, struct spu *spu)
{
	if (test_bit(SPU_CONTEXT_FAULT_PENDING, &spu->flags))
		csa->priv2.mfc_control_RW |= MFC_CNTL_RESTART_DMA_COMMAND;
	clear_bit(SPU_CONTEXT_SWITCH_PENDING, &spu->flags);
	mb();
}

static inline void enable_interrupts(struct spu_state *csa, struct spu *spu)
{
	unsigned long class1_mask = CLASS1_ENABLE_SEGMENT_FAULT_INTR |
	    CLASS1_ENABLE_STORAGE_FAULT_INTR;

	spin_lock_irq(&spu->register_lock);
	spu_int_stat_clear(spu, 0, CLASS0_INTR_MASK);
	spu_int_stat_clear(spu, 1, CLASS1_INTR_MASK);
	spu_int_stat_clear(spu, 2, CLASS2_INTR_MASK);
	spu_int_mask_set(spu, 0, 0ul);
	spu_int_mask_set(spu, 1, class1_mask);
	spu_int_mask_set(spu, 2, 0ul);
	spin_unlock_irq(&spu->register_lock);
}

static inline int send_mfc_dma(struct spu *spu, unsigned long ea,
			       unsigned int ls_offset, unsigned int size,
			       unsigned int tag, unsigned int rclass,
			       unsigned int cmd)
{
	struct spu_problem __iomem *prob = spu->problem;
	union mfc_tag_size_class_cmd command;
	unsigned int transfer_size;
	volatile unsigned int status = 0x0;

	while (size > 0) {
		transfer_size =
		    (size > MFC_MAX_DMA_SIZE) ? MFC_MAX_DMA_SIZE : size;
		command.u.mfc_size = transfer_size;
		command.u.mfc_tag = tag;
		command.u.mfc_rclassid = rclass;
		command.u.mfc_cmd = cmd;
		do {
			out_be32(&prob->mfc_lsa_W, ls_offset);
			out_be64(&prob->mfc_ea_W, ea);
			out_be64(&prob->mfc_union_W.all64, command.all64);
			status =
			    in_be32(&prob->mfc_union_W.by32.mfc_class_cmd32);
			if (unlikely(status & 0x2)) {
				cpu_relax();
			}
		} while (status & 0x3);
		size -= transfer_size;
		ea += transfer_size;
		ls_offset += transfer_size;
	}
	return 0;
}

static inline void save_ls_16kb(struct spu_state *csa, struct spu *spu)
{
	unsigned long addr = (unsigned long)&csa->lscsa->ls[0];
	unsigned int ls_offset = 0x0;
	unsigned int size = 16384;
	unsigned int tag = 0;
	unsigned int rclass = 0;
	unsigned int cmd = MFC_PUT_CMD;

	send_mfc_dma(spu, addr, ls_offset, size, tag, rclass, cmd);
}

static inline void set_spu_npc(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;

	out_be32(&prob->spu_npc_RW, 0);
	eieio();
}

static inline void set_signot1(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;
	union {
		u64 ull;
		u32 ui[2];
	} addr64;

	addr64.ull = (u64) csa->lscsa;
	out_be32(&prob->signal_notify1, addr64.ui[0]);
	eieio();
}

static inline void set_signot2(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;
	union {
		u64 ull;
		u32 ui[2];
	} addr64;

	addr64.ull = (u64) csa->lscsa;
	out_be32(&prob->signal_notify2, addr64.ui[1]);
	eieio();
}

static inline void send_save_code(struct spu_state *csa, struct spu *spu)
{
	unsigned long addr = (unsigned long)&spu_save_code[0];
	unsigned int ls_offset = 0x0;
	unsigned int size = sizeof(spu_save_code);
	unsigned int tag = 0;
	unsigned int rclass = 0;
	unsigned int cmd = MFC_GETFS_CMD;

	send_mfc_dma(spu, addr, ls_offset, size, tag, rclass, cmd);
}

static inline void set_ppu_querymask(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;

	out_be32(&prob->dma_querymask_RW, MFC_TAGID_TO_TAGMASK(0));
	eieio();
}

static inline void wait_tag_complete(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;
	u32 mask = MFC_TAGID_TO_TAGMASK(0);
	unsigned long flags;

	POLL_WHILE_FALSE(in_be32(&prob->dma_tagstatus_R) & mask);

	local_irq_save(flags);
	spu_int_stat_clear(spu, 0, CLASS0_INTR_MASK);
	spu_int_stat_clear(spu, 2, CLASS2_INTR_MASK);
	local_irq_restore(flags);
}

static inline void wait_spu_stopped(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;
	unsigned long flags;

	POLL_WHILE_TRUE(in_be32(&prob->spu_status_R) & SPU_STATUS_RUNNING);

	local_irq_save(flags);
	spu_int_stat_clear(spu, 0, CLASS0_INTR_MASK);
	spu_int_stat_clear(spu, 2, CLASS2_INTR_MASK);
	local_irq_restore(flags);
}

static inline int check_save_status(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;
	u32 complete;

	complete = ((SPU_SAVE_COMPLETE << SPU_STOP_STATUS_SHIFT) |
		    SPU_STATUS_STOPPED_BY_STOP);
	return (in_be32(&prob->spu_status_R) != complete) ? 1 : 0;
}

static inline void terminate_spu_app(struct spu_state *csa, struct spu *spu)
{
}

static inline void suspend_mfc_and_halt_decr(struct spu_state *csa,
		struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	out_be64(&priv2->mfc_control_RW, MFC_CNTL_SUSPEND_DMA_QUEUE |
		 MFC_CNTL_DECREMENTER_HALTED);
	eieio();
}

static inline void wait_suspend_mfc_complete(struct spu_state *csa,
					     struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	POLL_WHILE_FALSE((in_be64(&priv2->mfc_control_RW) &
			 MFC_CNTL_SUSPEND_DMA_STATUS_MASK) ==
			 MFC_CNTL_SUSPEND_COMPLETE);
}

static inline int suspend_spe(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;

	if (in_be32(&prob->spu_status_R) & SPU_STATUS_RUNNING) {
		if (in_be32(&prob->spu_status_R) &
		    SPU_STATUS_ISOLATED_EXIT_STATUS) {
			POLL_WHILE_TRUE(in_be32(&prob->spu_status_R) &
					SPU_STATUS_RUNNING);
		}
		if ((in_be32(&prob->spu_status_R) &
		     SPU_STATUS_ISOLATED_LOAD_STATUS)
		    || (in_be32(&prob->spu_status_R) &
			SPU_STATUS_ISOLATED_STATE)) {
			out_be32(&prob->spu_runcntl_RW, SPU_RUNCNTL_STOP);
			eieio();
			POLL_WHILE_TRUE(in_be32(&prob->spu_status_R) &
					SPU_STATUS_RUNNING);
			out_be32(&prob->spu_runcntl_RW, 0x2);
			eieio();
			POLL_WHILE_TRUE(in_be32(&prob->spu_status_R) &
					SPU_STATUS_RUNNING);
		}
		if (in_be32(&prob->spu_status_R) &
		    SPU_STATUS_WAITING_FOR_CHANNEL) {
			out_be32(&prob->spu_runcntl_RW, SPU_RUNCNTL_STOP);
			eieio();
			POLL_WHILE_TRUE(in_be32(&prob->spu_status_R) &
					SPU_STATUS_RUNNING);
		}
		return 1;
	}
	return 0;
}

static inline void clear_spu_status(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;

	if (!(in_be32(&prob->spu_status_R) & SPU_STATUS_RUNNING)) {
		if (in_be32(&prob->spu_status_R) &
		    SPU_STATUS_ISOLATED_EXIT_STATUS) {
			spu_mfc_sr1_set(spu,
					MFC_STATE1_MASTER_RUN_CONTROL_MASK);
			eieio();
			out_be32(&prob->spu_runcntl_RW, SPU_RUNCNTL_RUNNABLE);
			eieio();
			POLL_WHILE_TRUE(in_be32(&prob->spu_status_R) &
					SPU_STATUS_RUNNING);
		}
		if ((in_be32(&prob->spu_status_R) &
		     SPU_STATUS_ISOLATED_LOAD_STATUS)
		    || (in_be32(&prob->spu_status_R) &
			SPU_STATUS_ISOLATED_STATE)) {
			spu_mfc_sr1_set(spu,
					MFC_STATE1_MASTER_RUN_CONTROL_MASK);
			eieio();
			out_be32(&prob->spu_runcntl_RW, 0x2);
			eieio();
			POLL_WHILE_TRUE(in_be32(&prob->spu_status_R) &
					SPU_STATUS_RUNNING);
		}
	}
}

static inline void reset_ch_part1(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;
	u64 ch_indices[] = { 0UL, 3UL, 4UL, 24UL, 25UL, 27UL };
	u64 idx;
	int i;


	
	out_be64(&priv2->spu_chnlcntptr_RW, 1);
	out_be64(&priv2->spu_chnldata_RW, 0UL);

	
	for (i = 0; i < ARRAY_SIZE(ch_indices); i++) {
		idx = ch_indices[i];
		out_be64(&priv2->spu_chnlcntptr_RW, idx);
		eieio();
		out_be64(&priv2->spu_chnldata_RW, 0UL);
		out_be64(&priv2->spu_chnlcnt_RW, 0UL);
		eieio();
	}
}

static inline void reset_ch_part2(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;
	u64 ch_indices[5] = { 21UL, 23UL, 28UL, 29UL, 30UL };
	u64 ch_counts[5] = { 16UL, 1UL, 1UL, 0UL, 1UL };
	u64 idx;
	int i;

	for (i = 0; i < 5; i++) {
		idx = ch_indices[i];
		out_be64(&priv2->spu_chnlcntptr_RW, idx);
		eieio();
		out_be64(&priv2->spu_chnlcnt_RW, ch_counts[i]);
		eieio();
	}
}

static inline void setup_spu_status_part1(struct spu_state *csa,
					  struct spu *spu)
{
	u32 status_P = SPU_STATUS_STOPPED_BY_STOP;
	u32 status_I = SPU_STATUS_INVALID_INSTR;
	u32 status_H = SPU_STATUS_STOPPED_BY_HALT;
	u32 status_S = SPU_STATUS_SINGLE_STEP;
	u32 status_S_I = SPU_STATUS_SINGLE_STEP | SPU_STATUS_INVALID_INSTR;
	u32 status_S_P = SPU_STATUS_SINGLE_STEP | SPU_STATUS_STOPPED_BY_STOP;
	u32 status_P_H = SPU_STATUS_STOPPED_BY_HALT |SPU_STATUS_STOPPED_BY_STOP;
	u32 status_P_I = SPU_STATUS_STOPPED_BY_STOP |SPU_STATUS_INVALID_INSTR;
	u32 status_code;


	status_code =
	    (csa->prob.spu_status_R >> SPU_STOP_STATUS_SHIFT) & 0xFFFF;
	if ((csa->prob.spu_status_R & status_P_I) == status_P_I) {

		csa->lscsa->stopped_status.slot[0] = SPU_STOPPED_STATUS_P_I;
		csa->lscsa->stopped_status.slot[1] = status_code;

	} else if ((csa->prob.spu_status_R & status_P_H) == status_P_H) {

		csa->lscsa->stopped_status.slot[0] = SPU_STOPPED_STATUS_P_H;
		csa->lscsa->stopped_status.slot[1] = status_code;

	} else if ((csa->prob.spu_status_R & status_S_P) == status_S_P) {

		csa->lscsa->stopped_status.slot[0] = SPU_STOPPED_STATUS_S_P;
		csa->lscsa->stopped_status.slot[1] = status_code;

	} else if ((csa->prob.spu_status_R & status_S_I) == status_S_I) {

		csa->lscsa->stopped_status.slot[0] = SPU_STOPPED_STATUS_S_I;
		csa->lscsa->stopped_status.slot[1] = status_code;

	} else if ((csa->prob.spu_status_R & status_P) == status_P) {

		csa->lscsa->stopped_status.slot[0] = SPU_STOPPED_STATUS_P;
		csa->lscsa->stopped_status.slot[1] = status_code;

	} else if ((csa->prob.spu_status_R & status_H) == status_H) {

		csa->lscsa->stopped_status.slot[0] = SPU_STOPPED_STATUS_H;

	} else if ((csa->prob.spu_status_R & status_S) == status_S) {

		csa->lscsa->stopped_status.slot[0] = SPU_STOPPED_STATUS_S;

	} else if ((csa->prob.spu_status_R & status_I) == status_I) {

		csa->lscsa->stopped_status.slot[0] = SPU_STOPPED_STATUS_I;

	}
}

static inline void setup_spu_status_part2(struct spu_state *csa,
					  struct spu *spu)
{
	u32 mask;

	mask = SPU_STATUS_INVALID_INSTR |
	    SPU_STATUS_SINGLE_STEP |
	    SPU_STATUS_STOPPED_BY_HALT |
	    SPU_STATUS_STOPPED_BY_STOP | SPU_STATUS_RUNNING;
	if (!(csa->prob.spu_status_R & mask)) {
		csa->lscsa->stopped_status.slot[0] = SPU_STOPPED_STATUS_R;
	}
}

static inline void restore_mfc_rag(struct spu_state *csa, struct spu *spu)
{
	spu_resource_allocation_groupID_set(spu,
			csa->priv1.resource_allocation_groupID_RW);
	spu_resource_allocation_enable_set(spu,
			csa->priv1.resource_allocation_enable_RW);
}

static inline void send_restore_code(struct spu_state *csa, struct spu *spu)
{
	unsigned long addr = (unsigned long)&spu_restore_code[0];
	unsigned int ls_offset = 0x0;
	unsigned int size = sizeof(spu_restore_code);
	unsigned int tag = 0;
	unsigned int rclass = 0;
	unsigned int cmd = MFC_GETFS_CMD;

	send_mfc_dma(spu, addr, ls_offset, size, tag, rclass, cmd);
}

static inline void setup_decr(struct spu_state *csa, struct spu *spu)
{
	if (csa->priv2.mfc_control_RW & MFC_CNTL_DECREMENTER_RUNNING) {
		cycles_t resume_time = get_cycles();
		cycles_t delta_time = resume_time - csa->suspend_time;

		csa->lscsa->decr_status.slot[0] = SPU_DECR_STATUS_RUNNING;
		if (csa->lscsa->decr.slot[0] < delta_time) {
			csa->lscsa->decr_status.slot[0] |=
				 SPU_DECR_STATUS_WRAPPED;
		}

		csa->lscsa->decr.slot[0] -= delta_time;
	} else {
		csa->lscsa->decr_status.slot[0] = 0;
	}
}

static inline void setup_ppu_mb(struct spu_state *csa, struct spu *spu)
{
	csa->lscsa->ppu_mb.slot[0] = csa->prob.pu_mb_R;
}

static inline void setup_ppuint_mb(struct spu_state *csa, struct spu *spu)
{
	csa->lscsa->ppuint_mb.slot[0] = csa->priv2.puint_mb_R;
}

static inline int check_restore_status(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;
	u32 complete;

	complete = ((SPU_RESTORE_COMPLETE << SPU_STOP_STATUS_SHIFT) |
		    SPU_STATUS_STOPPED_BY_STOP);
	return (in_be32(&prob->spu_status_R) != complete) ? 1 : 0;
}

static inline void restore_spu_privcntl(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	out_be64(&priv2->spu_privcntl_RW, csa->priv2.spu_privcntl_RW);
	eieio();
}

static inline void restore_status_part1(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;
	u32 mask;

	mask = SPU_STATUS_INVALID_INSTR |
	    SPU_STATUS_SINGLE_STEP |
	    SPU_STATUS_STOPPED_BY_HALT | SPU_STATUS_STOPPED_BY_STOP;
	if (csa->prob.spu_status_R & mask) {
		out_be32(&prob->spu_runcntl_RW, SPU_RUNCNTL_RUNNABLE);
		eieio();
		POLL_WHILE_TRUE(in_be32(&prob->spu_status_R) &
				SPU_STATUS_RUNNING);
	}
}

static inline void restore_status_part2(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;
	u32 mask;

	mask = SPU_STATUS_INVALID_INSTR |
	    SPU_STATUS_SINGLE_STEP |
	    SPU_STATUS_STOPPED_BY_HALT |
	    SPU_STATUS_STOPPED_BY_STOP | SPU_STATUS_RUNNING;
	if (!(csa->prob.spu_status_R & mask)) {
		out_be32(&prob->spu_runcntl_RW, SPU_RUNCNTL_RUNNABLE);
		eieio();
		POLL_WHILE_FALSE(in_be32(&prob->spu_status_R) &
				 SPU_STATUS_RUNNING);
		out_be32(&prob->spu_runcntl_RW, SPU_RUNCNTL_STOP);
		eieio();
		POLL_WHILE_TRUE(in_be32(&prob->spu_status_R) &
				SPU_STATUS_RUNNING);
	}
}

static inline void restore_ls_16kb(struct spu_state *csa, struct spu *spu)
{
	unsigned long addr = (unsigned long)&csa->lscsa->ls[0];
	unsigned int ls_offset = 0x0;
	unsigned int size = 16384;
	unsigned int tag = 0;
	unsigned int rclass = 0;
	unsigned int cmd = MFC_GET_CMD;

	send_mfc_dma(spu, addr, ls_offset, size, tag, rclass, cmd);
}

static inline void suspend_mfc(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	out_be64(&priv2->mfc_control_RW, MFC_CNTL_SUSPEND_DMA_QUEUE);
	eieio();
}

static inline void clear_interrupts(struct spu_state *csa, struct spu *spu)
{
	spin_lock_irq(&spu->register_lock);
	spu_int_mask_set(spu, 0, 0ul);
	spu_int_mask_set(spu, 1, 0ul);
	spu_int_mask_set(spu, 2, 0ul);
	spu_int_stat_clear(spu, 0, CLASS0_INTR_MASK);
	spu_int_stat_clear(spu, 1, CLASS1_INTR_MASK);
	spu_int_stat_clear(spu, 2, CLASS2_INTR_MASK);
	spin_unlock_irq(&spu->register_lock);
}

static inline void restore_mfc_queues(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;
	int i;

	if ((csa->priv2.mfc_control_RW & MFC_CNTL_DMA_QUEUES_EMPTY_MASK) == 0) {
		for (i = 0; i < 8; i++) {
			out_be64(&priv2->puq[i].mfc_cq_data0_RW,
				 csa->priv2.puq[i].mfc_cq_data0_RW);
			out_be64(&priv2->puq[i].mfc_cq_data1_RW,
				 csa->priv2.puq[i].mfc_cq_data1_RW);
			out_be64(&priv2->puq[i].mfc_cq_data2_RW,
				 csa->priv2.puq[i].mfc_cq_data2_RW);
			out_be64(&priv2->puq[i].mfc_cq_data3_RW,
				 csa->priv2.puq[i].mfc_cq_data3_RW);
		}
		for (i = 0; i < 16; i++) {
			out_be64(&priv2->spuq[i].mfc_cq_data0_RW,
				 csa->priv2.spuq[i].mfc_cq_data0_RW);
			out_be64(&priv2->spuq[i].mfc_cq_data1_RW,
				 csa->priv2.spuq[i].mfc_cq_data1_RW);
			out_be64(&priv2->spuq[i].mfc_cq_data2_RW,
				 csa->priv2.spuq[i].mfc_cq_data2_RW);
			out_be64(&priv2->spuq[i].mfc_cq_data3_RW,
				 csa->priv2.spuq[i].mfc_cq_data3_RW);
		}
	}
	eieio();
}

static inline void restore_ppu_querymask(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;

	out_be32(&prob->dma_querymask_RW, csa->prob.dma_querymask_RW);
	eieio();
}

static inline void restore_ppu_querytype(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;

	out_be32(&prob->dma_querytype_RW, csa->prob.dma_querytype_RW);
	eieio();
}

static inline void restore_mfc_csr_tsq(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	out_be64(&priv2->spu_tag_status_query_RW,
		 csa->priv2.spu_tag_status_query_RW);
	eieio();
}

static inline void restore_mfc_csr_cmd(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	out_be64(&priv2->spu_cmd_buf1_RW, csa->priv2.spu_cmd_buf1_RW);
	out_be64(&priv2->spu_cmd_buf2_RW, csa->priv2.spu_cmd_buf2_RW);
	eieio();
}

static inline void restore_mfc_csr_ato(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	out_be64(&priv2->spu_atomic_status_RW, csa->priv2.spu_atomic_status_RW);
}

static inline void restore_mfc_tclass_id(struct spu_state *csa, struct spu *spu)
{
	spu_mfc_tclass_id_set(spu, csa->priv1.mfc_tclass_id_RW);
	eieio();
}

static inline void set_llr_event(struct spu_state *csa, struct spu *spu)
{
	u64 ch0_cnt, ch0_data;
	u64 ch1_data;

	ch0_cnt = csa->spu_chnlcnt_RW[0];
	ch0_data = csa->spu_chnldata_RW[0];
	ch1_data = csa->spu_chnldata_RW[1];
	csa->spu_chnldata_RW[0] |= MFC_LLR_LOST_EVENT;
	if ((ch0_cnt == 0) && !(ch0_data & MFC_LLR_LOST_EVENT) &&
	    (ch1_data & MFC_LLR_LOST_EVENT)) {
		csa->spu_chnlcnt_RW[0] = 1;
	}
}

static inline void restore_decr_wrapped(struct spu_state *csa, struct spu *spu)
{
	if (!(csa->lscsa->decr_status.slot[0] & SPU_DECR_STATUS_WRAPPED))
		return;

	if ((csa->spu_chnlcnt_RW[0] == 0) &&
	    (csa->spu_chnldata_RW[1] & 0x20) &&
	    !(csa->spu_chnldata_RW[0] & 0x20))
		csa->spu_chnlcnt_RW[0] = 1;

	csa->spu_chnldata_RW[0] |= 0x20;
}

static inline void restore_ch_part1(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;
	u64 idx, ch_indices[] = { 0UL, 3UL, 4UL, 24UL, 25UL, 27UL };
	int i;

	for (i = 0; i < ARRAY_SIZE(ch_indices); i++) {
		idx = ch_indices[i];
		out_be64(&priv2->spu_chnlcntptr_RW, idx);
		eieio();
		out_be64(&priv2->spu_chnldata_RW, csa->spu_chnldata_RW[idx]);
		out_be64(&priv2->spu_chnlcnt_RW, csa->spu_chnlcnt_RW[idx]);
		eieio();
	}
}

static inline void restore_ch_part2(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;
	u64 ch_indices[3] = { 9UL, 21UL, 23UL };
	u64 ch_counts[3] = { 1UL, 16UL, 1UL };
	u64 idx;
	int i;

	ch_counts[0] = 1UL;
	ch_counts[1] = csa->spu_chnlcnt_RW[21];
	ch_counts[2] = 1UL;
	for (i = 0; i < 3; i++) {
		idx = ch_indices[i];
		out_be64(&priv2->spu_chnlcntptr_RW, idx);
		eieio();
		out_be64(&priv2->spu_chnlcnt_RW, ch_counts[i]);
		eieio();
	}
}

static inline void restore_spu_lslr(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	out_be64(&priv2->spu_lslr_RW, csa->priv2.spu_lslr_RW);
	eieio();
}

static inline void restore_spu_cfg(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	out_be64(&priv2->spu_cfg_RW, csa->priv2.spu_cfg_RW);
	eieio();
}

static inline void restore_pm_trace(struct spu_state *csa, struct spu *spu)
{
}

static inline void restore_spu_npc(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;

	out_be32(&prob->spu_npc_RW, csa->prob.spu_npc_RW);
	eieio();
}

static inline void restore_spu_mb(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;
	int i;

	out_be64(&priv2->spu_chnlcntptr_RW, 29UL);
	eieio();
	out_be64(&priv2->spu_chnlcnt_RW, csa->spu_chnlcnt_RW[29]);
	for (i = 0; i < 4; i++) {
		out_be64(&priv2->spu_chnldata_RW, csa->spu_mailbox_data[i]);
	}
	eieio();
}

static inline void check_ppu_mb_stat(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;
	u32 dummy = 0;

	if ((csa->prob.mb_stat_R & 0xFF) == 0) {
		dummy = in_be32(&prob->pu_mb_R);
		eieio();
	}
}

static inline void check_ppuint_mb_stat(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;
	u64 dummy = 0UL;

	if ((csa->prob.mb_stat_R & 0xFF0000) == 0) {
		dummy = in_be64(&priv2->puint_mb_R);
		eieio();
		spu_int_stat_clear(spu, 2, CLASS2_ENABLE_MAILBOX_INTR);
		eieio();
	}
}

static inline void restore_mfc_sr1(struct spu_state *csa, struct spu *spu)
{
	spu_mfc_sr1_set(spu, csa->priv1.mfc_sr1_RW);
	eieio();
}

static inline void set_int_route(struct spu_state *csa, struct spu *spu)
{
	struct spu_context *ctx = spu->ctx;

	spu_cpu_affinity_set(spu, ctx->last_ran);
}

static inline void restore_other_spu_access(struct spu_state *csa,
					    struct spu *spu)
{
}

static inline void restore_spu_runcntl(struct spu_state *csa, struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;

	if (csa->prob.spu_status_R & SPU_STATUS_RUNNING) {
		out_be32(&prob->spu_runcntl_RW, SPU_RUNCNTL_RUNNABLE);
		eieio();
	}
}

static inline void restore_mfc_cntl(struct spu_state *csa, struct spu *spu)
{
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	out_be64(&priv2->mfc_control_RW, csa->priv2.mfc_control_RW);
	eieio();

}

static inline void enable_user_access(struct spu_state *csa, struct spu *spu)
{
}

static inline void reset_switch_active(struct spu_state *csa, struct spu *spu)
{
}

static inline void reenable_interrupts(struct spu_state *csa, struct spu *spu)
{
	spin_lock_irq(&spu->register_lock);
	spu_int_mask_set(spu, 0, csa->priv1.int_mask_class0_RW);
	spu_int_mask_set(spu, 1, csa->priv1.int_mask_class1_RW);
	spu_int_mask_set(spu, 2, csa->priv1.int_mask_class2_RW);
	spin_unlock_irq(&spu->register_lock);
}

static int quiece_spu(struct spu_state *prev, struct spu *spu)
{

	if (check_spu_isolate(prev, spu)) {	
		return 2;
	}
	disable_interrupts(prev, spu);	        
	set_watchdog_timer(prev, spu);	        
	inhibit_user_access(prev, spu);	        
	if (check_spu_isolate(prev, spu)) {	
		return 6;
	}
	set_switch_pending(prev, spu);	        
	save_mfc_cntl(prev, spu);		
	save_spu_runcntl(prev, spu);	        
	save_mfc_sr1(prev, spu);	        
	save_spu_status(prev, spu);	        
	save_mfc_stopped_status(prev, spu);     
	halt_mfc_decr(prev, spu);	        
	save_timebase(prev, spu);		
	remove_other_spu_access(prev, spu);	
	do_mfc_mssync(prev, spu);	        
	issue_mfc_tlbie(prev, spu);	        
	handle_pending_interrupts(prev, spu);	

	return 0;
}

static void save_csa(struct spu_state *prev, struct spu *spu)
{

	save_mfc_queues(prev, spu);	
	save_ppu_querymask(prev, spu);	
	save_ppu_querytype(prev, spu);	
	save_ppu_tagstatus(prev, spu);  
	save_mfc_csr_tsq(prev, spu);	
	save_mfc_csr_cmd(prev, spu);	
	save_mfc_csr_ato(prev, spu);	
	save_mfc_tclass_id(prev, spu);	
	set_mfc_tclass_id(prev, spu);	
	save_mfc_cmd(prev, spu);	
	purge_mfc_queue(prev, spu);	
	wait_purge_complete(prev, spu);	
	setup_mfc_sr1(prev, spu);	
	save_spu_npc(prev, spu);	
	save_spu_privcntl(prev, spu);	
	reset_spu_privcntl(prev, spu);	
	save_spu_lslr(prev, spu);	
	reset_spu_lslr(prev, spu);	
	save_spu_cfg(prev, spu);	
	save_pm_trace(prev, spu);	
	save_mfc_rag(prev, spu);	
	save_ppu_mb_stat(prev, spu);	
	save_ppu_mb(prev, spu);	        
	save_ppuint_mb(prev, spu);	
	save_ch_part1(prev, spu);	
	save_spu_mb(prev, spu);	        
	reset_ch(prev, spu);	        
}

static void save_lscsa(struct spu_state *prev, struct spu *spu)
{

	resume_mfc_queue(prev, spu);	
	
	setup_mfc_slbs(prev, spu, spu_save_code, sizeof(spu_save_code));
	set_switch_active(prev, spu);	
	enable_interrupts(prev, spu);	
	save_ls_16kb(prev, spu);	
	set_spu_npc(prev, spu);	        
	set_signot1(prev, spu);		
	set_signot2(prev, spu);		
	send_save_code(prev, spu);	
	set_ppu_querymask(prev, spu);	
	wait_tag_complete(prev, spu);	
	wait_spu_stopped(prev, spu);	
}

static void force_spu_isolate_exit(struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;
	struct spu_priv2 __iomem *priv2 = spu->priv2;

	
	out_be32(&prob->spu_runcntl_RW, SPU_RUNCNTL_STOP);
	iobarrier_rw();
	POLL_WHILE_TRUE(in_be32(&prob->spu_status_R) & SPU_STATUS_RUNNING);

	
	spu_mfc_sr1_set(spu, MFC_STATE1_MASTER_RUN_CONTROL_MASK);
	iobarrier_w();

	
	out_be64(&priv2->spu_privcntl_RW, 4LL);
	iobarrier_w();
	out_be32(&prob->spu_runcntl_RW, 2);
	iobarrier_rw();
	POLL_WHILE_FALSE((in_be32(&prob->spu_status_R)
				& SPU_STATUS_STOPPED_BY_STOP));

	
	out_be64(&priv2->spu_privcntl_RW, SPU_PRIVCNT_LOAD_REQUEST_NORMAL);
	iobarrier_w();
}

static void stop_spu_isolate(struct spu *spu)
{
	struct spu_problem __iomem *prob = spu->problem;

	if (in_be32(&prob->spu_status_R) & SPU_STATUS_ISOLATED_STATE) {
		force_spu_isolate_exit(spu);
	}
}

static void harvest(struct spu_state *prev, struct spu *spu)
{

	disable_interrupts(prev, spu);	        
	inhibit_user_access(prev, spu);	        
	terminate_spu_app(prev, spu);	        
	set_switch_pending(prev, spu);	        
	stop_spu_isolate(spu);			
	remove_other_spu_access(prev, spu);	
	suspend_mfc_and_halt_decr(prev, spu);	
	wait_suspend_mfc_complete(prev, spu);	
	if (!suspend_spe(prev, spu))	        
		clear_spu_status(prev, spu);	
	do_mfc_mssync(prev, spu);	        
	issue_mfc_tlbie(prev, spu);	        
	handle_pending_interrupts(prev, spu);	
	purge_mfc_queue(prev, spu);	        
	wait_purge_complete(prev, spu);	        
	reset_spu_privcntl(prev, spu);	        
	reset_spu_lslr(prev, spu);              
	setup_mfc_sr1(prev, spu);	        
	spu_invalidate_slbs(spu);		
	reset_ch_part1(prev, spu);	        
	reset_ch_part2(prev, spu);	        
	enable_interrupts(prev, spu);	        
	set_switch_active(prev, spu);	        
	set_mfc_tclass_id(prev, spu);	        
	resume_mfc_queue(prev, spu);	        
}

static void restore_lscsa(struct spu_state *next, struct spu *spu)
{

	set_watchdog_timer(next, spu);	        
	setup_spu_status_part1(next, spu);	
	setup_spu_status_part2(next, spu);	
	restore_mfc_rag(next, spu);	        
	
	setup_mfc_slbs(next, spu, spu_restore_code, sizeof(spu_restore_code));
	set_spu_npc(next, spu);	                
	set_signot1(next, spu);	                
	set_signot2(next, spu);	                
	setup_decr(next, spu);	                
	setup_ppu_mb(next, spu);	        
	setup_ppuint_mb(next, spu);	        
	send_restore_code(next, spu);	        
	set_ppu_querymask(next, spu);	        
	wait_tag_complete(next, spu);	        
	wait_spu_stopped(next, spu);	        
}

static void restore_csa(struct spu_state *next, struct spu *spu)
{

	restore_spu_privcntl(next, spu);	
	restore_status_part1(next, spu);	
	restore_status_part2(next, spu);	
	restore_ls_16kb(next, spu);	        
	wait_tag_complete(next, spu);	        
	suspend_mfc(next, spu);	                
	wait_suspend_mfc_complete(next, spu);	
	issue_mfc_tlbie(next, spu);	        
	clear_interrupts(next, spu);	        
	restore_mfc_queues(next, spu);	        
	restore_ppu_querymask(next, spu);	
	restore_ppu_querytype(next, spu);	
	restore_mfc_csr_tsq(next, spu);	        
	restore_mfc_csr_cmd(next, spu);	        
	restore_mfc_csr_ato(next, spu);	        
	restore_mfc_tclass_id(next, spu);	
	set_llr_event(next, spu);	        
	restore_decr_wrapped(next, spu);	
	restore_ch_part1(next, spu);	        
	restore_ch_part2(next, spu);	        
	restore_spu_lslr(next, spu);	        
	restore_spu_cfg(next, spu);	        
	restore_pm_trace(next, spu);	        
	restore_spu_npc(next, spu);	        
	restore_spu_mb(next, spu);	        
	check_ppu_mb_stat(next, spu);	        
	check_ppuint_mb_stat(next, spu);	
	spu_invalidate_slbs(spu);		
	restore_mfc_sr1(next, spu);	        
	set_int_route(next, spu);		
	restore_other_spu_access(next, spu);	
	restore_spu_runcntl(next, spu);	        
	restore_mfc_cntl(next, spu);	        
	enable_user_access(next, spu);	        
	reset_switch_active(next, spu);	        
	reenable_interrupts(next, spu);	        
}

static int __do_spu_save(struct spu_state *prev, struct spu *spu)
{
	int rc;


	rc = quiece_spu(prev, spu);	        
	switch (rc) {
	default:
	case 2:
	case 6:
		harvest(prev, spu);
		return rc;
		break;
	case 0:
		break;
	}
	save_csa(prev, spu);	                
	save_lscsa(prev, spu);	                
	return check_save_status(prev, spu);	
}

static int __do_spu_restore(struct spu_state *next, struct spu *spu)
{
	int rc;


	restore_lscsa(next, spu);	        
	rc = check_restore_status(next, spu);	
	switch (rc) {
	default:
		
		return rc;
		break;
	case 0:
		
		break;
	}
	restore_csa(next, spu);

	return 0;
}

int spu_save(struct spu_state *prev, struct spu *spu)
{
	int rc;

	acquire_spu_lock(spu);	        
	rc = __do_spu_save(prev, spu);	
	release_spu_lock(spu);
	if (rc != 0 && rc != 2 && rc != 6) {
		panic("%s failed on SPU[%d], rc=%d.\n",
		      __func__, spu->number, rc);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(spu_save);

int spu_restore(struct spu_state *new, struct spu *spu)
{
	int rc;

	acquire_spu_lock(spu);
	harvest(NULL, spu);
	spu->slb_replace = 0;
	rc = __do_spu_restore(new, spu);
	release_spu_lock(spu);
	if (rc) {
		panic("%s failed on SPU[%d] rc=%d.\n",
		       __func__, spu->number, rc);
	}
	return rc;
}
EXPORT_SYMBOL_GPL(spu_restore);

static void init_prob(struct spu_state *csa)
{
	csa->spu_chnlcnt_RW[9] = 1;
	csa->spu_chnlcnt_RW[21] = 16;
	csa->spu_chnlcnt_RW[23] = 1;
	csa->spu_chnlcnt_RW[28] = 1;
	csa->spu_chnlcnt_RW[30] = 1;
	csa->prob.spu_runcntl_RW = SPU_RUNCNTL_STOP;
	csa->prob.mb_stat_R = 0x000400;
}

static void init_priv1(struct spu_state *csa)
{
	
	csa->priv1.mfc_sr1_RW = MFC_STATE1_LOCAL_STORAGE_DECODE_MASK |
	    MFC_STATE1_MASTER_RUN_CONTROL_MASK |
	    MFC_STATE1_PROBLEM_STATE_MASK |
	    MFC_STATE1_RELOCATE_MASK | MFC_STATE1_BUS_TLBIE_MASK;

	
	csa->priv1.int_mask_class0_RW = CLASS0_ENABLE_DMA_ALIGNMENT_INTR |
	    CLASS0_ENABLE_INVALID_DMA_COMMAND_INTR |
	    CLASS0_ENABLE_SPU_ERROR_INTR;
	csa->priv1.int_mask_class1_RW = CLASS1_ENABLE_SEGMENT_FAULT_INTR |
	    CLASS1_ENABLE_STORAGE_FAULT_INTR;
	csa->priv1.int_mask_class2_RW = CLASS2_ENABLE_SPU_STOP_INTR |
	    CLASS2_ENABLE_SPU_HALT_INTR |
	    CLASS2_ENABLE_SPU_DMA_TAG_GROUP_COMPLETE_INTR;
}

static void init_priv2(struct spu_state *csa)
{
	csa->priv2.spu_lslr_RW = LS_ADDR_MASK;
	csa->priv2.mfc_control_RW = MFC_CNTL_RESUME_DMA_QUEUE |
	    MFC_CNTL_NORMAL_DMA_QUEUE_OPERATION |
	    MFC_CNTL_DMA_QUEUES_EMPTY_MASK;
}

int spu_init_csa(struct spu_state *csa)
{
	int rc;

	if (!csa)
		return -EINVAL;
	memset(csa, 0, sizeof(struct spu_state));

	rc = spu_alloc_lscsa(csa);
	if (rc)
		return rc;

	spin_lock_init(&csa->register_lock);

	init_prob(csa);
	init_priv1(csa);
	init_priv2(csa);

	return 0;
}

void spu_fini_csa(struct spu_state *csa)
{
	spu_free_lscsa(csa);
}
