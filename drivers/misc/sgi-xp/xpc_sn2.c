/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2008-2009 Silicon Graphics, Inc.  All Rights Reserved.
 */


#include <linux/delay.h>
#include <linux/slab.h>
#include <asm/uncached.h>
#include <asm/sn/mspec.h>
#include <asm/sn/sn_sal.h>
#include "xpc.h"

#define XPC_MAX_PHYSNODES_SN2	(MAX_NUMALINK_NODES / 2)
#define XP_NASID_MASK_BYTES_SN2	((XPC_MAX_PHYSNODES_SN2 + 7) / 8)
#define XP_NASID_MASK_WORDS_SN2	((XPC_MAX_PHYSNODES_SN2 + 63) / 64)

#define XPC_NOTIFY_IRQ_AMOS_SN2		0
#define XPC_ACTIVATE_IRQ_AMOS_SN2	(XPC_NOTIFY_IRQ_AMOS_SN2 + \
					 XP_MAX_NPARTITIONS_SN2)
#define XPC_ENGAGED_PARTITIONS_AMO_SN2	(XPC_ACTIVATE_IRQ_AMOS_SN2 + \
					 XP_NASID_MASK_WORDS_SN2)
#define XPC_DEACTIVATE_REQUEST_AMO_SN2	(XPC_ENGAGED_PARTITIONS_AMO_SN2 + 1)

static void *xpc_remote_copy_buffer_base_sn2;
static char *xpc_remote_copy_buffer_sn2;

static struct xpc_vars_sn2 *xpc_vars_sn2;
static struct xpc_vars_part_sn2 *xpc_vars_part_sn2;

static int
xpc_setup_partitions_sn2(void)
{
	
	return 0;
}

static void
xpc_teardown_partitions_sn2(void)
{
	
}

static u64 xpc_sh1_IPI_access_sn2;
static u64 xpc_sh2_IPI_access0_sn2;
static u64 xpc_sh2_IPI_access1_sn2;
static u64 xpc_sh2_IPI_access2_sn2;
static u64 xpc_sh2_IPI_access3_sn2;

static void
xpc_allow_IPI_ops_sn2(void)
{
	int node;
	int nasid;

	
	if (is_shub2()) {
		xpc_sh2_IPI_access0_sn2 =
		    (u64)HUB_L((u64 *)LOCAL_MMR_ADDR(SH2_IPI_ACCESS0));
		xpc_sh2_IPI_access1_sn2 =
		    (u64)HUB_L((u64 *)LOCAL_MMR_ADDR(SH2_IPI_ACCESS1));
		xpc_sh2_IPI_access2_sn2 =
		    (u64)HUB_L((u64 *)LOCAL_MMR_ADDR(SH2_IPI_ACCESS2));
		xpc_sh2_IPI_access3_sn2 =
		    (u64)HUB_L((u64 *)LOCAL_MMR_ADDR(SH2_IPI_ACCESS3));

		for_each_online_node(node) {
			nasid = cnodeid_to_nasid(node);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS0),
			      -1UL);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS1),
			      -1UL);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS2),
			      -1UL);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS3),
			      -1UL);
		}
	} else {
		xpc_sh1_IPI_access_sn2 =
		    (u64)HUB_L((u64 *)LOCAL_MMR_ADDR(SH1_IPI_ACCESS));

		for_each_online_node(node) {
			nasid = cnodeid_to_nasid(node);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH1_IPI_ACCESS),
			      -1UL);
		}
	}
}

static void
xpc_disallow_IPI_ops_sn2(void)
{
	int node;
	int nasid;

	
	if (is_shub2()) {
		for_each_online_node(node) {
			nasid = cnodeid_to_nasid(node);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS0),
			      xpc_sh2_IPI_access0_sn2);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS1),
			      xpc_sh2_IPI_access1_sn2);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS2),
			      xpc_sh2_IPI_access2_sn2);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS3),
			      xpc_sh2_IPI_access3_sn2);
		}
	} else {
		for_each_online_node(node) {
			nasid = cnodeid_to_nasid(node);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH1_IPI_ACCESS),
			      xpc_sh1_IPI_access_sn2);
		}
	}
}


static u64
xpc_receive_IRQ_amo_sn2(struct amo *amo)
{
	return FETCHOP_LOAD_OP(TO_AMO((u64)&amo->variable), FETCHOP_CLEAR);
}

static enum xp_retval
xpc_send_IRQ_sn2(struct amo *amo, u64 flag, int nasid, int phys_cpuid,
		 int vector)
{
	int ret = 0;
	unsigned long irq_flags;

	local_irq_save(irq_flags);

	FETCHOP_STORE_OP(TO_AMO((u64)&amo->variable), FETCHOP_OR, flag);
	sn_send_IPI_phys(nasid, phys_cpuid, vector, 0);

	ret = xp_nofault_PIOR((u64 *)GLOBAL_MMR_ADDR(NASID_GET(&amo->variable),
						     xp_nofault_PIOR_target));

	local_irq_restore(irq_flags);

	return (ret == 0) ? xpSuccess : xpPioReadError;
}

static struct amo *
xpc_init_IRQ_amo_sn2(int index)
{
	struct amo *amo = xpc_vars_sn2->amos_page + index;

	(void)xpc_receive_IRQ_amo_sn2(amo);	
	return amo;
}


static irqreturn_t
xpc_handle_activate_IRQ_sn2(int irq, void *dev_id)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&xpc_activate_IRQ_rcvd_lock, irq_flags);
	xpc_activate_IRQ_rcvd++;
	spin_unlock_irqrestore(&xpc_activate_IRQ_rcvd_lock, irq_flags);

	wake_up_interruptible(&xpc_activate_IRQ_wq);
	return IRQ_HANDLED;
}

static void
xpc_send_activate_IRQ_sn2(unsigned long amos_page_pa, int from_nasid,
			  int to_nasid, int to_phys_cpuid)
{
	struct amo *amos = (struct amo *)__va(amos_page_pa +
					      (XPC_ACTIVATE_IRQ_AMOS_SN2 *
					      sizeof(struct amo)));

	(void)xpc_send_IRQ_sn2(&amos[BIT_WORD(from_nasid / 2)],
			       BIT_MASK(from_nasid / 2), to_nasid,
			       to_phys_cpuid, SGI_XPC_ACTIVATE);
}

static void
xpc_send_local_activate_IRQ_sn2(int from_nasid)
{
	unsigned long irq_flags;
	struct amo *amos = (struct amo *)__va(xpc_vars_sn2->amos_page_pa +
					      (XPC_ACTIVATE_IRQ_AMOS_SN2 *
					      sizeof(struct amo)));

	
	FETCHOP_STORE_OP(TO_AMO((u64)&amos[BIT_WORD(from_nasid / 2)].variable),
			 FETCHOP_OR, BIT_MASK(from_nasid / 2));

	spin_lock_irqsave(&xpc_activate_IRQ_rcvd_lock, irq_flags);
	xpc_activate_IRQ_rcvd++;
	spin_unlock_irqrestore(&xpc_activate_IRQ_rcvd_lock, irq_flags);

	wake_up_interruptible(&xpc_activate_IRQ_wq);
}


static void
xpc_check_for_sent_chctl_flags_sn2(struct xpc_partition *part)
{
	union xpc_channel_ctl_flags chctl;
	unsigned long irq_flags;

	chctl.all_flags = xpc_receive_IRQ_amo_sn2(part->sn.sn2.
						  local_chctl_amo_va);
	if (chctl.all_flags == 0)
		return;

	spin_lock_irqsave(&part->chctl_lock, irq_flags);
	part->chctl.all_flags |= chctl.all_flags;
	spin_unlock_irqrestore(&part->chctl_lock, irq_flags);

	dev_dbg(xpc_chan, "received notify IRQ from partid=%d, chctl.all_flags="
		"0x%llx\n", XPC_PARTID(part), chctl.all_flags);

	xpc_wakeup_channel_mgr(part);
}

static irqreturn_t
xpc_handle_notify_IRQ_sn2(int irq, void *dev_id)
{
	short partid = (short)(u64)dev_id;
	struct xpc_partition *part = &xpc_partitions[partid];

	DBUG_ON(partid < 0 || partid >= XP_MAX_NPARTITIONS_SN2);

	if (xpc_part_ref(part)) {
		xpc_check_for_sent_chctl_flags_sn2(part);

		xpc_part_deref(part);
	}
	return IRQ_HANDLED;
}

static void
xpc_check_for_dropped_notify_IRQ_sn2(struct xpc_partition *part)
{
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;

	if (xpc_part_ref(part)) {
		xpc_check_for_sent_chctl_flags_sn2(part);

		part_sn2->dropped_notify_IRQ_timer.expires = jiffies +
		    XPC_DROPPED_NOTIFY_IRQ_WAIT_INTERVAL;
		add_timer(&part_sn2->dropped_notify_IRQ_timer);
		xpc_part_deref(part);
	}
}

static void
xpc_send_notify_IRQ_sn2(struct xpc_channel *ch, u8 chctl_flag,
			char *chctl_flag_string, unsigned long *irq_flags)
{
	struct xpc_partition *part = &xpc_partitions[ch->partid];
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;
	union xpc_channel_ctl_flags chctl = { 0 };
	enum xp_retval ret;

	if (likely(part->act_state != XPC_P_AS_DEACTIVATING)) {
		chctl.flags[ch->number] = chctl_flag;
		ret = xpc_send_IRQ_sn2(part_sn2->remote_chctl_amo_va,
				       chctl.all_flags,
				       part_sn2->notify_IRQ_nasid,
				       part_sn2->notify_IRQ_phys_cpuid,
				       SGI_XPC_NOTIFY);
		dev_dbg(xpc_chan, "%s sent to partid=%d, channel=%d, ret=%d\n",
			chctl_flag_string, ch->partid, ch->number, ret);
		if (unlikely(ret != xpSuccess)) {
			if (irq_flags != NULL)
				spin_unlock_irqrestore(&ch->lock, *irq_flags);
			XPC_DEACTIVATE_PARTITION(part, ret);
			if (irq_flags != NULL)
				spin_lock_irqsave(&ch->lock, *irq_flags);
		}
	}
}

#define XPC_SEND_NOTIFY_IRQ_SN2(_ch, _ipi_f, _irq_f) \
		xpc_send_notify_IRQ_sn2(_ch, _ipi_f, #_ipi_f, _irq_f)

static void
xpc_send_local_notify_IRQ_sn2(struct xpc_channel *ch, u8 chctl_flag,
			      char *chctl_flag_string)
{
	struct xpc_partition *part = &xpc_partitions[ch->partid];
	union xpc_channel_ctl_flags chctl = { 0 };

	chctl.flags[ch->number] = chctl_flag;
	FETCHOP_STORE_OP(TO_AMO((u64)&part->sn.sn2.local_chctl_amo_va->
				variable), FETCHOP_OR, chctl.all_flags);
	dev_dbg(xpc_chan, "%s sent local from partid=%d, channel=%d\n",
		chctl_flag_string, ch->partid, ch->number);
}

#define XPC_SEND_LOCAL_NOTIFY_IRQ_SN2(_ch, _ipi_f) \
		xpc_send_local_notify_IRQ_sn2(_ch, _ipi_f, #_ipi_f)

static void
xpc_send_chctl_closerequest_sn2(struct xpc_channel *ch,
				unsigned long *irq_flags)
{
	struct xpc_openclose_args *args = ch->sn.sn2.local_openclose_args;

	args->reason = ch->reason;
	XPC_SEND_NOTIFY_IRQ_SN2(ch, XPC_CHCTL_CLOSEREQUEST, irq_flags);
}

static void
xpc_send_chctl_closereply_sn2(struct xpc_channel *ch, unsigned long *irq_flags)
{
	XPC_SEND_NOTIFY_IRQ_SN2(ch, XPC_CHCTL_CLOSEREPLY, irq_flags);
}

static void
xpc_send_chctl_openrequest_sn2(struct xpc_channel *ch, unsigned long *irq_flags)
{
	struct xpc_openclose_args *args = ch->sn.sn2.local_openclose_args;

	args->entry_size = ch->entry_size;
	args->local_nentries = ch->local_nentries;
	XPC_SEND_NOTIFY_IRQ_SN2(ch, XPC_CHCTL_OPENREQUEST, irq_flags);
}

static void
xpc_send_chctl_openreply_sn2(struct xpc_channel *ch, unsigned long *irq_flags)
{
	struct xpc_openclose_args *args = ch->sn.sn2.local_openclose_args;

	args->remote_nentries = ch->remote_nentries;
	args->local_nentries = ch->local_nentries;
	args->local_msgqueue_pa = xp_pa(ch->sn.sn2.local_msgqueue);
	XPC_SEND_NOTIFY_IRQ_SN2(ch, XPC_CHCTL_OPENREPLY, irq_flags);
}

static void
xpc_send_chctl_opencomplete_sn2(struct xpc_channel *ch,
				unsigned long *irq_flags)
{
	XPC_SEND_NOTIFY_IRQ_SN2(ch, XPC_CHCTL_OPENCOMPLETE, irq_flags);
}

static void
xpc_send_chctl_msgrequest_sn2(struct xpc_channel *ch)
{
	XPC_SEND_NOTIFY_IRQ_SN2(ch, XPC_CHCTL_MSGREQUEST, NULL);
}

static void
xpc_send_chctl_local_msgrequest_sn2(struct xpc_channel *ch)
{
	XPC_SEND_LOCAL_NOTIFY_IRQ_SN2(ch, XPC_CHCTL_MSGREQUEST);
}

static enum xp_retval
xpc_save_remote_msgqueue_pa_sn2(struct xpc_channel *ch,
				unsigned long msgqueue_pa)
{
	ch->sn.sn2.remote_msgqueue_pa = msgqueue_pa;
	return xpSuccess;
}


static void
xpc_indicate_partition_engaged_sn2(struct xpc_partition *part)
{
	unsigned long irq_flags;
	struct amo *amo = (struct amo *)__va(part->sn.sn2.remote_amos_page_pa +
					     (XPC_ENGAGED_PARTITIONS_AMO_SN2 *
					     sizeof(struct amo)));

	local_irq_save(irq_flags);

	
	FETCHOP_STORE_OP(TO_AMO((u64)&amo->variable), FETCHOP_OR,
			 BIT(sn_partition_id));

	(void)xp_nofault_PIOR((u64 *)GLOBAL_MMR_ADDR(NASID_GET(&amo->
							       variable),
						     xp_nofault_PIOR_target));

	local_irq_restore(irq_flags);
}

static void
xpc_indicate_partition_disengaged_sn2(struct xpc_partition *part)
{
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;
	unsigned long irq_flags;
	struct amo *amo = (struct amo *)__va(part_sn2->remote_amos_page_pa +
					     (XPC_ENGAGED_PARTITIONS_AMO_SN2 *
					     sizeof(struct amo)));

	local_irq_save(irq_flags);

	
	FETCHOP_STORE_OP(TO_AMO((u64)&amo->variable), FETCHOP_AND,
			 ~BIT(sn_partition_id));

	(void)xp_nofault_PIOR((u64 *)GLOBAL_MMR_ADDR(NASID_GET(&amo->
							       variable),
						     xp_nofault_PIOR_target));

	local_irq_restore(irq_flags);

	xpc_send_activate_IRQ_sn2(part_sn2->remote_amos_page_pa,
				  cnodeid_to_nasid(0),
				  part_sn2->activate_IRQ_nasid,
				  part_sn2->activate_IRQ_phys_cpuid);
}

static void
xpc_assume_partition_disengaged_sn2(short partid)
{
	struct amo *amo = xpc_vars_sn2->amos_page +
			  XPC_ENGAGED_PARTITIONS_AMO_SN2;

	
	FETCHOP_STORE_OP(TO_AMO((u64)&amo->variable), FETCHOP_AND,
			 ~BIT(partid));
}

static int
xpc_partition_engaged_sn2(short partid)
{
	struct amo *amo = xpc_vars_sn2->amos_page +
			  XPC_ENGAGED_PARTITIONS_AMO_SN2;

	
	return (FETCHOP_LOAD_OP(TO_AMO((u64)&amo->variable), FETCHOP_LOAD) &
		BIT(partid)) != 0;
}

static int
xpc_any_partition_engaged_sn2(void)
{
	struct amo *amo = xpc_vars_sn2->amos_page +
			  XPC_ENGAGED_PARTITIONS_AMO_SN2;

	
	return FETCHOP_LOAD_OP(TO_AMO((u64)&amo->variable), FETCHOP_LOAD) != 0;
}

static u64 xpc_prot_vec_sn2[MAX_NUMNODES];

static enum xp_retval
xpc_allow_amo_ops_sn2(struct amo *amos_page)
{
	enum xp_retval ret = xpSuccess;

	if (!enable_shub_wars_1_1())
		ret = xp_expand_memprotect(ia64_tpa((u64)amos_page), PAGE_SIZE);

	return ret;
}

static void
xpc_allow_amo_ops_shub_wars_1_1_sn2(void)
{
	int node;
	int nasid;

	if (!enable_shub_wars_1_1())
		return;

	for_each_online_node(node) {
		nasid = cnodeid_to_nasid(node);
		
		xpc_prot_vec_sn2[node] =
		    (u64)HUB_L((u64 *)GLOBAL_MMR_ADDR(nasid,
						  SH1_MD_DQLP_MMR_DIR_PRIVEC0));
		
		HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid,
					     SH1_MD_DQLP_MMR_DIR_PRIVEC0),
		      -1UL);
		HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid,
					     SH1_MD_DQRP_MMR_DIR_PRIVEC0),
		      -1UL);
	}
}

static enum xp_retval
xpc_get_partition_rsvd_page_pa_sn2(void *buf, u64 *cookie, unsigned long *rp_pa,
				   size_t *len)
{
	s64 status;
	enum xp_retval ret;

	status = sn_partition_reserved_page_pa((u64)buf, cookie,
			(u64 *)rp_pa, (u64 *)len);
	if (status == SALRET_OK)
		ret = xpSuccess;
	else if (status == SALRET_MORE_PASSES)
		ret = xpNeedMoreInfo;
	else
		ret = xpSalError;

	return ret;
}


static int
xpc_setup_rsvd_page_sn2(struct xpc_rsvd_page *rp)
{
	struct amo *amos_page;
	int i;
	int ret;

	xpc_vars_sn2 = XPC_RP_VARS(rp);

	rp->sn.sn2.vars_pa = xp_pa(xpc_vars_sn2);

	
	xpc_vars_part_sn2 = (struct xpc_vars_part_sn2 *)((u8 *)XPC_RP_VARS(rp) +
							 XPC_RP_VARS_SIZE);

	amos_page = xpc_vars_sn2->amos_page;
	if (amos_page == NULL) {
		amos_page = (struct amo *)TO_AMO(uncached_alloc_page(0, 1));
		if (amos_page == NULL) {
			dev_err(xpc_part, "can't allocate page of amos\n");
			return -ENOMEM;
		}

		ret = xpc_allow_amo_ops_sn2(amos_page);
		if (ret != xpSuccess) {
			dev_err(xpc_part, "can't allow amo operations\n");
			uncached_free_page(__IA64_UNCACHED_OFFSET |
					   TO_PHYS((u64)amos_page), 1);
			return -EPERM;
		}
	}

	
	memset(xpc_vars_sn2, 0, sizeof(struct xpc_vars_sn2));

	xpc_vars_sn2->version = XPC_V_VERSION;
	xpc_vars_sn2->activate_IRQ_nasid = cpuid_to_nasid(0);
	xpc_vars_sn2->activate_IRQ_phys_cpuid = cpu_physical_id(0);
	xpc_vars_sn2->vars_part_pa = xp_pa(xpc_vars_part_sn2);
	xpc_vars_sn2->amos_page_pa = ia64_tpa((u64)amos_page);
	xpc_vars_sn2->amos_page = amos_page;	

	
	memset((u64 *)xpc_vars_part_sn2, 0, sizeof(struct xpc_vars_part_sn2) *
	       XP_MAX_NPARTITIONS_SN2);

	
	for (i = 0; i < xpc_nasid_mask_nlongs; i++)
		(void)xpc_init_IRQ_amo_sn2(XPC_ACTIVATE_IRQ_AMOS_SN2 + i);

	
	(void)xpc_init_IRQ_amo_sn2(XPC_ENGAGED_PARTITIONS_AMO_SN2);
	(void)xpc_init_IRQ_amo_sn2(XPC_DEACTIVATE_REQUEST_AMO_SN2);

	return 0;
}

static int
xpc_hb_allowed_sn2(short partid, void *heartbeating_to_mask)
{
	return test_bit(partid, heartbeating_to_mask);
}

static void
xpc_allow_hb_sn2(short partid)
{
	DBUG_ON(xpc_vars_sn2 == NULL);
	set_bit(partid, xpc_vars_sn2->heartbeating_to_mask);
}

static void
xpc_disallow_hb_sn2(short partid)
{
	DBUG_ON(xpc_vars_sn2 == NULL);
	clear_bit(partid, xpc_vars_sn2->heartbeating_to_mask);
}

static void
xpc_disallow_all_hbs_sn2(void)
{
	DBUG_ON(xpc_vars_sn2 == NULL);
	bitmap_zero(xpc_vars_sn2->heartbeating_to_mask, xp_max_npartitions);
}

static void
xpc_increment_heartbeat_sn2(void)
{
	xpc_vars_sn2->heartbeat++;
}

static void
xpc_offline_heartbeat_sn2(void)
{
	xpc_increment_heartbeat_sn2();
	xpc_vars_sn2->heartbeat_offline = 1;
}

static void
xpc_online_heartbeat_sn2(void)
{
	xpc_increment_heartbeat_sn2();
	xpc_vars_sn2->heartbeat_offline = 0;
}

static void
xpc_heartbeat_init_sn2(void)
{
	DBUG_ON(xpc_vars_sn2 == NULL);

	bitmap_zero(xpc_vars_sn2->heartbeating_to_mask, XP_MAX_NPARTITIONS_SN2);
	xpc_online_heartbeat_sn2();
}

static void
xpc_heartbeat_exit_sn2(void)
{
	xpc_offline_heartbeat_sn2();
}

static enum xp_retval
xpc_get_remote_heartbeat_sn2(struct xpc_partition *part)
{
	struct xpc_vars_sn2 *remote_vars;
	enum xp_retval ret;

	remote_vars = (struct xpc_vars_sn2 *)xpc_remote_copy_buffer_sn2;

	
	ret = xp_remote_memcpy(xp_pa(remote_vars),
			       part->sn.sn2.remote_vars_pa,
			       XPC_RP_VARS_SIZE);
	if (ret != xpSuccess)
		return ret;

	dev_dbg(xpc_part, "partid=%d, heartbeat=%lld, last_heartbeat=%lld, "
		"heartbeat_offline=%lld, HB_mask[0]=0x%lx\n", XPC_PARTID(part),
		remote_vars->heartbeat, part->last_heartbeat,
		remote_vars->heartbeat_offline,
		remote_vars->heartbeating_to_mask[0]);

	if ((remote_vars->heartbeat == part->last_heartbeat &&
	    !remote_vars->heartbeat_offline) ||
	    !xpc_hb_allowed_sn2(sn_partition_id,
				remote_vars->heartbeating_to_mask)) {
		ret = xpNoHeartbeat;
	} else {
		part->last_heartbeat = remote_vars->heartbeat;
	}

	return ret;
}

static enum xp_retval
xpc_get_remote_vars_sn2(unsigned long remote_vars_pa,
			struct xpc_vars_sn2 *remote_vars)
{
	enum xp_retval ret;

	if (remote_vars_pa == 0)
		return xpVarsNotSet;

	
	ret = xp_remote_memcpy(xp_pa(remote_vars), remote_vars_pa,
			       XPC_RP_VARS_SIZE);
	if (ret != xpSuccess)
		return ret;

	if (XPC_VERSION_MAJOR(remote_vars->version) !=
	    XPC_VERSION_MAJOR(XPC_V_VERSION)) {
		return xpBadVersion;
	}

	return xpSuccess;
}

static void
xpc_request_partition_activation_sn2(struct xpc_rsvd_page *remote_rp,
				     unsigned long remote_rp_pa, int nasid)
{
	xpc_send_local_activate_IRQ_sn2(nasid);
}

static void
xpc_request_partition_reactivation_sn2(struct xpc_partition *part)
{
	xpc_send_local_activate_IRQ_sn2(part->sn.sn2.activate_IRQ_nasid);
}

static void
xpc_request_partition_deactivation_sn2(struct xpc_partition *part)
{
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;
	unsigned long irq_flags;
	struct amo *amo = (struct amo *)__va(part_sn2->remote_amos_page_pa +
					     (XPC_DEACTIVATE_REQUEST_AMO_SN2 *
					     sizeof(struct amo)));

	local_irq_save(irq_flags);

	
	FETCHOP_STORE_OP(TO_AMO((u64)&amo->variable), FETCHOP_OR,
			 BIT(sn_partition_id));

	(void)xp_nofault_PIOR((u64 *)GLOBAL_MMR_ADDR(NASID_GET(&amo->
							       variable),
						     xp_nofault_PIOR_target));

	local_irq_restore(irq_flags);

	xpc_send_activate_IRQ_sn2(part_sn2->remote_amos_page_pa,
				  cnodeid_to_nasid(0),
				  part_sn2->activate_IRQ_nasid,
				  part_sn2->activate_IRQ_phys_cpuid);
}

static void
xpc_cancel_partition_deactivation_request_sn2(struct xpc_partition *part)
{
	unsigned long irq_flags;
	struct amo *amo = (struct amo *)__va(part->sn.sn2.remote_amos_page_pa +
					     (XPC_DEACTIVATE_REQUEST_AMO_SN2 *
					     sizeof(struct amo)));

	local_irq_save(irq_flags);

	
	FETCHOP_STORE_OP(TO_AMO((u64)&amo->variable), FETCHOP_AND,
			 ~BIT(sn_partition_id));

	(void)xp_nofault_PIOR((u64 *)GLOBAL_MMR_ADDR(NASID_GET(&amo->
							       variable),
						     xp_nofault_PIOR_target));

	local_irq_restore(irq_flags);
}

static int
xpc_partition_deactivation_requested_sn2(short partid)
{
	struct amo *amo = xpc_vars_sn2->amos_page +
			  XPC_DEACTIVATE_REQUEST_AMO_SN2;

	
	return (FETCHOP_LOAD_OP(TO_AMO((u64)&amo->variable), FETCHOP_LOAD) &
		BIT(partid)) != 0;
}

static void
xpc_update_partition_info_sn2(struct xpc_partition *part, u8 remote_rp_version,
			      unsigned long *remote_rp_ts_jiffies,
			      unsigned long remote_rp_pa,
			      unsigned long remote_vars_pa,
			      struct xpc_vars_sn2 *remote_vars)
{
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;

	part->remote_rp_version = remote_rp_version;
	dev_dbg(xpc_part, "  remote_rp_version = 0x%016x\n",
		part->remote_rp_version);

	part->remote_rp_ts_jiffies = *remote_rp_ts_jiffies;
	dev_dbg(xpc_part, "  remote_rp_ts_jiffies = 0x%016lx\n",
		part->remote_rp_ts_jiffies);

	part->remote_rp_pa = remote_rp_pa;
	dev_dbg(xpc_part, "  remote_rp_pa = 0x%016lx\n", part->remote_rp_pa);

	part_sn2->remote_vars_pa = remote_vars_pa;
	dev_dbg(xpc_part, "  remote_vars_pa = 0x%016lx\n",
		part_sn2->remote_vars_pa);

	part->last_heartbeat = remote_vars->heartbeat - 1;
	dev_dbg(xpc_part, "  last_heartbeat = 0x%016llx\n",
		part->last_heartbeat);

	part_sn2->remote_vars_part_pa = remote_vars->vars_part_pa;
	dev_dbg(xpc_part, "  remote_vars_part_pa = 0x%016lx\n",
		part_sn2->remote_vars_part_pa);

	part_sn2->activate_IRQ_nasid = remote_vars->activate_IRQ_nasid;
	dev_dbg(xpc_part, "  activate_IRQ_nasid = 0x%x\n",
		part_sn2->activate_IRQ_nasid);

	part_sn2->activate_IRQ_phys_cpuid =
	    remote_vars->activate_IRQ_phys_cpuid;
	dev_dbg(xpc_part, "  activate_IRQ_phys_cpuid = 0x%x\n",
		part_sn2->activate_IRQ_phys_cpuid);

	part_sn2->remote_amos_page_pa = remote_vars->amos_page_pa;
	dev_dbg(xpc_part, "  remote_amos_page_pa = 0x%lx\n",
		part_sn2->remote_amos_page_pa);

	part_sn2->remote_vars_version = remote_vars->version;
	dev_dbg(xpc_part, "  remote_vars_version = 0x%x\n",
		part_sn2->remote_vars_version);
}

static void
xpc_identify_activate_IRQ_req_sn2(int nasid)
{
	struct xpc_rsvd_page *remote_rp;
	struct xpc_vars_sn2 *remote_vars;
	unsigned long remote_rp_pa;
	unsigned long remote_vars_pa;
	int remote_rp_version;
	int reactivate = 0;
	unsigned long remote_rp_ts_jiffies = 0;
	short partid;
	struct xpc_partition *part;
	struct xpc_partition_sn2 *part_sn2;
	enum xp_retval ret;

	

	remote_rp = (struct xpc_rsvd_page *)xpc_remote_copy_buffer_sn2;

	ret = xpc_get_remote_rp(nasid, NULL, remote_rp, &remote_rp_pa);
	if (ret != xpSuccess) {
		dev_warn(xpc_part, "unable to get reserved page from nasid %d, "
			 "which sent interrupt, reason=%d\n", nasid, ret);
		return;
	}

	remote_vars_pa = remote_rp->sn.sn2.vars_pa;
	remote_rp_version = remote_rp->version;
	remote_rp_ts_jiffies = remote_rp->ts_jiffies;

	partid = remote_rp->SAL_partid;
	part = &xpc_partitions[partid];
	part_sn2 = &part->sn.sn2;

	

	remote_vars = (struct xpc_vars_sn2 *)xpc_remote_copy_buffer_sn2;

	ret = xpc_get_remote_vars_sn2(remote_vars_pa, remote_vars);
	if (ret != xpSuccess) {
		dev_warn(xpc_part, "unable to get XPC variables from nasid %d, "
			 "which sent interrupt, reason=%d\n", nasid, ret);

		XPC_DEACTIVATE_PARTITION(part, ret);
		return;
	}

	part->activate_IRQ_rcvd++;

	dev_dbg(xpc_part, "partid for nasid %d is %d; IRQs = %d; HB = "
		"%lld:0x%lx\n", (int)nasid, (int)partid,
		part->activate_IRQ_rcvd,
		remote_vars->heartbeat, remote_vars->heartbeating_to_mask[0]);

	if (xpc_partition_disengaged(part) &&
	    part->act_state == XPC_P_AS_INACTIVE) {

		xpc_update_partition_info_sn2(part, remote_rp_version,
					      &remote_rp_ts_jiffies,
					      remote_rp_pa, remote_vars_pa,
					      remote_vars);

		if (xpc_partition_deactivation_requested_sn2(partid)) {
			return;
		}

		xpc_activate_partition(part);
		return;
	}

	DBUG_ON(part->remote_rp_version == 0);
	DBUG_ON(part_sn2->remote_vars_version == 0);

	if (remote_rp_ts_jiffies != part->remote_rp_ts_jiffies) {

		

		DBUG_ON(xpc_partition_engaged_sn2(partid));
		DBUG_ON(xpc_partition_deactivation_requested_sn2(partid));

		xpc_update_partition_info_sn2(part, remote_rp_version,
					      &remote_rp_ts_jiffies,
					      remote_rp_pa, remote_vars_pa,
					      remote_vars);
		reactivate = 1;
	}

	if (part->disengage_timeout > 0 && !xpc_partition_disengaged(part)) {
		
		return;
	}

	if (reactivate)
		XPC_DEACTIVATE_PARTITION(part, xpReactivating);
	else if (xpc_partition_deactivation_requested_sn2(partid))
		XPC_DEACTIVATE_PARTITION(part, xpOtherGoingDown);
}

int
xpc_identify_activate_IRQ_sender_sn2(void)
{
	int l;
	int b;
	unsigned long nasid_mask_long;
	u64 nasid;		
	int n_IRQs_detected = 0;
	struct amo *act_amos;

	act_amos = xpc_vars_sn2->amos_page + XPC_ACTIVATE_IRQ_AMOS_SN2;

	
	for (l = 0; l < xpc_nasid_mask_nlongs; l++) {

		if (xpc_exiting)
			break;

		nasid_mask_long = xpc_receive_IRQ_amo_sn2(&act_amos[l]);

		b = find_first_bit(&nasid_mask_long, BITS_PER_LONG);
		if (b >= BITS_PER_LONG) {
			
			continue;
		}

		dev_dbg(xpc_part, "amo[%d] gave back 0x%lx\n", l,
			nasid_mask_long);

		xpc_mach_nasids[l] |= nasid_mask_long;

		

		do {
			n_IRQs_detected++;
			nasid = (l * BITS_PER_LONG + b) * 2;
			dev_dbg(xpc_part, "interrupt from nasid %lld\n", nasid);
			xpc_identify_activate_IRQ_req_sn2(nasid);

			b = find_next_bit(&nasid_mask_long, BITS_PER_LONG,
					  b + 1);
		} while (b < BITS_PER_LONG);
	}
	return n_IRQs_detected;
}

static void
xpc_process_activate_IRQ_rcvd_sn2(void)
{
	unsigned long irq_flags;
	int n_IRQs_expected;
	int n_IRQs_detected;

	spin_lock_irqsave(&xpc_activate_IRQ_rcvd_lock, irq_flags);
	n_IRQs_expected = xpc_activate_IRQ_rcvd;
	xpc_activate_IRQ_rcvd = 0;
	spin_unlock_irqrestore(&xpc_activate_IRQ_rcvd_lock, irq_flags);

	n_IRQs_detected = xpc_identify_activate_IRQ_sender_sn2();
	if (n_IRQs_detected < n_IRQs_expected) {
		
		(void)xpc_identify_activate_IRQ_sender_sn2();
	}
}

static enum xp_retval
xpc_setup_ch_structures_sn2(struct xpc_partition *part)
{
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;
	struct xpc_channel_sn2 *ch_sn2;
	enum xp_retval retval;
	int ret;
	int cpuid;
	int ch_number;
	struct timer_list *timer;
	short partid = XPC_PARTID(part);

	

	part_sn2->local_GPs =
	    xpc_kzalloc_cacheline_aligned(XPC_GP_SIZE, GFP_KERNEL,
					  &part_sn2->local_GPs_base);
	if (part_sn2->local_GPs == NULL) {
		dev_err(xpc_chan, "can't get memory for local get/put "
			"values\n");
		return xpNoMemory;
	}

	part_sn2->remote_GPs =
	    xpc_kzalloc_cacheline_aligned(XPC_GP_SIZE, GFP_KERNEL,
					  &part_sn2->remote_GPs_base);
	if (part_sn2->remote_GPs == NULL) {
		dev_err(xpc_chan, "can't get memory for remote get/put "
			"values\n");
		retval = xpNoMemory;
		goto out_1;
	}

	part_sn2->remote_GPs_pa = 0;

	

	part_sn2->local_openclose_args =
	    xpc_kzalloc_cacheline_aligned(XPC_OPENCLOSE_ARGS_SIZE,
					  GFP_KERNEL, &part_sn2->
					  local_openclose_args_base);
	if (part_sn2->local_openclose_args == NULL) {
		dev_err(xpc_chan, "can't get memory for local connect args\n");
		retval = xpNoMemory;
		goto out_2;
	}

	part_sn2->remote_openclose_args_pa = 0;

	part_sn2->local_chctl_amo_va = xpc_init_IRQ_amo_sn2(partid);

	part_sn2->notify_IRQ_nasid = 0;
	part_sn2->notify_IRQ_phys_cpuid = 0;
	part_sn2->remote_chctl_amo_va = NULL;

	sprintf(part_sn2->notify_IRQ_owner, "xpc%02d", partid);
	ret = request_irq(SGI_XPC_NOTIFY, xpc_handle_notify_IRQ_sn2,
			  IRQF_SHARED, part_sn2->notify_IRQ_owner,
			  (void *)(u64)partid);
	if (ret != 0) {
		dev_err(xpc_chan, "can't register NOTIFY IRQ handler, "
			"errno=%d\n", -ret);
		retval = xpLackOfResources;
		goto out_3;
	}

	
	timer = &part_sn2->dropped_notify_IRQ_timer;
	init_timer(timer);
	timer->function =
	    (void (*)(unsigned long))xpc_check_for_dropped_notify_IRQ_sn2;
	timer->data = (unsigned long)part;
	timer->expires = jiffies + XPC_DROPPED_NOTIFY_IRQ_WAIT_INTERVAL;
	add_timer(timer);

	for (ch_number = 0; ch_number < part->nchannels; ch_number++) {
		ch_sn2 = &part->channels[ch_number].sn.sn2;

		ch_sn2->local_GP = &part_sn2->local_GPs[ch_number];
		ch_sn2->local_openclose_args =
		    &part_sn2->local_openclose_args[ch_number];

		mutex_init(&ch_sn2->msg_to_pull_mutex);
	}

	xpc_vars_part_sn2[partid].GPs_pa = xp_pa(part_sn2->local_GPs);
	xpc_vars_part_sn2[partid].openclose_args_pa =
	    xp_pa(part_sn2->local_openclose_args);
	xpc_vars_part_sn2[partid].chctl_amo_pa =
	    xp_pa(part_sn2->local_chctl_amo_va);
	cpuid = raw_smp_processor_id();	
	xpc_vars_part_sn2[partid].notify_IRQ_nasid = cpuid_to_nasid(cpuid);
	xpc_vars_part_sn2[partid].notify_IRQ_phys_cpuid =
	    cpu_physical_id(cpuid);
	xpc_vars_part_sn2[partid].nchannels = part->nchannels;
	xpc_vars_part_sn2[partid].magic = XPC_VP_MAGIC1_SN2;

	return xpSuccess;

	
out_3:
	kfree(part_sn2->local_openclose_args_base);
	part_sn2->local_openclose_args = NULL;
out_2:
	kfree(part_sn2->remote_GPs_base);
	part_sn2->remote_GPs = NULL;
out_1:
	kfree(part_sn2->local_GPs_base);
	part_sn2->local_GPs = NULL;
	return retval;
}

static void
xpc_teardown_ch_structures_sn2(struct xpc_partition *part)
{
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;
	short partid = XPC_PARTID(part);

	xpc_vars_part_sn2[partid].magic = 0;

	
	del_timer_sync(&part_sn2->dropped_notify_IRQ_timer);
	free_irq(SGI_XPC_NOTIFY, (void *)(u64)partid);

	kfree(part_sn2->local_openclose_args_base);
	part_sn2->local_openclose_args = NULL;
	kfree(part_sn2->remote_GPs_base);
	part_sn2->remote_GPs = NULL;
	kfree(part_sn2->local_GPs_base);
	part_sn2->local_GPs = NULL;
	part_sn2->local_chctl_amo_va = NULL;
}

static enum xp_retval
xpc_pull_remote_cachelines_sn2(struct xpc_partition *part, void *dst,
			       const unsigned long src_pa, size_t cnt)
{
	enum xp_retval ret;

	DBUG_ON(src_pa != L1_CACHE_ALIGN(src_pa));
	DBUG_ON((unsigned long)dst != L1_CACHE_ALIGN((unsigned long)dst));
	DBUG_ON(cnt != L1_CACHE_ALIGN(cnt));

	if (part->act_state == XPC_P_AS_DEACTIVATING)
		return part->reason;

	ret = xp_remote_memcpy(xp_pa(dst), src_pa, cnt);
	if (ret != xpSuccess) {
		dev_dbg(xpc_chan, "xp_remote_memcpy() from partition %d failed,"
			" ret=%d\n", XPC_PARTID(part), ret);
	}
	return ret;
}

static enum xp_retval
xpc_pull_remote_vars_part_sn2(struct xpc_partition *part)
{
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;
	u8 buffer[L1_CACHE_BYTES * 2];
	struct xpc_vars_part_sn2 *pulled_entry_cacheline =
	    (struct xpc_vars_part_sn2 *)L1_CACHE_ALIGN((u64)buffer);
	struct xpc_vars_part_sn2 *pulled_entry;
	unsigned long remote_entry_cacheline_pa;
	unsigned long remote_entry_pa;
	short partid = XPC_PARTID(part);
	enum xp_retval ret;

	

	DBUG_ON(part_sn2->remote_vars_part_pa !=
		L1_CACHE_ALIGN(part_sn2->remote_vars_part_pa));
	DBUG_ON(sizeof(struct xpc_vars_part_sn2) != L1_CACHE_BYTES / 2);

	remote_entry_pa = part_sn2->remote_vars_part_pa +
	    sn_partition_id * sizeof(struct xpc_vars_part_sn2);

	remote_entry_cacheline_pa = (remote_entry_pa & ~(L1_CACHE_BYTES - 1));

	pulled_entry = (struct xpc_vars_part_sn2 *)((u64)pulled_entry_cacheline
						    + (remote_entry_pa &
						    (L1_CACHE_BYTES - 1)));

	ret = xpc_pull_remote_cachelines_sn2(part, pulled_entry_cacheline,
					     remote_entry_cacheline_pa,
					     L1_CACHE_BYTES);
	if (ret != xpSuccess) {
		dev_dbg(xpc_chan, "failed to pull XPC vars_part from "
			"partition %d, ret=%d\n", partid, ret);
		return ret;
	}

	

	if (pulled_entry->magic != XPC_VP_MAGIC1_SN2 &&
	    pulled_entry->magic != XPC_VP_MAGIC2_SN2) {

		if (pulled_entry->magic != 0) {
			dev_dbg(xpc_chan, "partition %d's XPC vars_part for "
				"partition %d has bad magic value (=0x%llx)\n",
				partid, sn_partition_id, pulled_entry->magic);
			return xpBadMagic;
		}

		
		return xpRetry;
	}

	if (xpc_vars_part_sn2[partid].magic == XPC_VP_MAGIC1_SN2) {

		

		if (pulled_entry->GPs_pa == 0 ||
		    pulled_entry->openclose_args_pa == 0 ||
		    pulled_entry->chctl_amo_pa == 0) {

			dev_err(xpc_chan, "partition %d's XPC vars_part for "
				"partition %d are not valid\n", partid,
				sn_partition_id);
			return xpInvalidAddress;
		}

		

		part_sn2->remote_GPs_pa = pulled_entry->GPs_pa;
		part_sn2->remote_openclose_args_pa =
		    pulled_entry->openclose_args_pa;
		part_sn2->remote_chctl_amo_va =
		    (struct amo *)__va(pulled_entry->chctl_amo_pa);
		part_sn2->notify_IRQ_nasid = pulled_entry->notify_IRQ_nasid;
		part_sn2->notify_IRQ_phys_cpuid =
		    pulled_entry->notify_IRQ_phys_cpuid;

		if (part->nchannels > pulled_entry->nchannels)
			part->nchannels = pulled_entry->nchannels;

		

		xpc_vars_part_sn2[partid].magic = XPC_VP_MAGIC2_SN2;
	}

	if (pulled_entry->magic == XPC_VP_MAGIC1_SN2)
		return xpRetry;

	return xpSuccess;
}

static enum xp_retval
xpc_make_first_contact_sn2(struct xpc_partition *part)
{
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;
	enum xp_retval ret;

	if (sn_register_xp_addr_region(part_sn2->remote_amos_page_pa,
				       PAGE_SIZE, 1) < 0) {
		dev_warn(xpc_part, "xpc_activating(%d) failed to register "
			 "xp_addr region\n", XPC_PARTID(part));

		ret = xpPhysAddrRegFailed;
		XPC_DEACTIVATE_PARTITION(part, ret);
		return ret;
	}

	xpc_send_activate_IRQ_sn2(part_sn2->remote_amos_page_pa,
				  cnodeid_to_nasid(0),
				  part_sn2->activate_IRQ_nasid,
				  part_sn2->activate_IRQ_phys_cpuid);

	while ((ret = xpc_pull_remote_vars_part_sn2(part)) != xpSuccess) {
		if (ret != xpRetry) {
			XPC_DEACTIVATE_PARTITION(part, ret);
			return ret;
		}

		dev_dbg(xpc_part, "waiting to make first contact with "
			"partition %d\n", XPC_PARTID(part));

		
		(void)msleep_interruptible(250);

		if (part->act_state == XPC_P_AS_DEACTIVATING)
			return part->reason;
	}

	return xpSuccess;
}

static u64
xpc_get_chctl_all_flags_sn2(struct xpc_partition *part)
{
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;
	unsigned long irq_flags;
	union xpc_channel_ctl_flags chctl;
	enum xp_retval ret;


	spin_lock_irqsave(&part->chctl_lock, irq_flags);
	chctl = part->chctl;
	if (chctl.all_flags != 0)
		part->chctl.all_flags = 0;

	spin_unlock_irqrestore(&part->chctl_lock, irq_flags);

	if (xpc_any_openclose_chctl_flags_set(&chctl)) {
		ret = xpc_pull_remote_cachelines_sn2(part, part->
						     remote_openclose_args,
						     part_sn2->
						     remote_openclose_args_pa,
						     XPC_OPENCLOSE_ARGS_SIZE);
		if (ret != xpSuccess) {
			XPC_DEACTIVATE_PARTITION(part, ret);

			dev_dbg(xpc_chan, "failed to pull openclose args from "
				"partition %d, ret=%d\n", XPC_PARTID(part),
				ret);

			
			chctl.all_flags = 0;
		}
	}

	if (xpc_any_msg_chctl_flags_set(&chctl)) {
		ret = xpc_pull_remote_cachelines_sn2(part, part_sn2->remote_GPs,
						     part_sn2->remote_GPs_pa,
						     XPC_GP_SIZE);
		if (ret != xpSuccess) {
			XPC_DEACTIVATE_PARTITION(part, ret);

			dev_dbg(xpc_chan, "failed to pull GPs from partition "
				"%d, ret=%d\n", XPC_PARTID(part), ret);

			
			chctl.all_flags = 0;
		}
	}

	return chctl.all_flags;
}

static enum xp_retval
xpc_allocate_local_msgqueue_sn2(struct xpc_channel *ch)
{
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	unsigned long irq_flags;
	int nentries;
	size_t nbytes;

	for (nentries = ch->local_nentries; nentries > 0; nentries--) {

		nbytes = nentries * ch->entry_size;
		ch_sn2->local_msgqueue =
		    xpc_kzalloc_cacheline_aligned(nbytes, GFP_KERNEL,
						  &ch_sn2->local_msgqueue_base);
		if (ch_sn2->local_msgqueue == NULL)
			continue;

		nbytes = nentries * sizeof(struct xpc_notify_sn2);
		ch_sn2->notify_queue = kzalloc(nbytes, GFP_KERNEL);
		if (ch_sn2->notify_queue == NULL) {
			kfree(ch_sn2->local_msgqueue_base);
			ch_sn2->local_msgqueue = NULL;
			continue;
		}

		spin_lock_irqsave(&ch->lock, irq_flags);
		if (nentries < ch->local_nentries) {
			dev_dbg(xpc_chan, "nentries=%d local_nentries=%d, "
				"partid=%d, channel=%d\n", nentries,
				ch->local_nentries, ch->partid, ch->number);

			ch->local_nentries = nentries;
		}
		spin_unlock_irqrestore(&ch->lock, irq_flags);
		return xpSuccess;
	}

	dev_dbg(xpc_chan, "can't get memory for local message queue and notify "
		"queue, partid=%d, channel=%d\n", ch->partid, ch->number);
	return xpNoMemory;
}

static enum xp_retval
xpc_allocate_remote_msgqueue_sn2(struct xpc_channel *ch)
{
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	unsigned long irq_flags;
	int nentries;
	size_t nbytes;

	DBUG_ON(ch->remote_nentries <= 0);

	for (nentries = ch->remote_nentries; nentries > 0; nentries--) {

		nbytes = nentries * ch->entry_size;
		ch_sn2->remote_msgqueue =
		    xpc_kzalloc_cacheline_aligned(nbytes, GFP_KERNEL, &ch_sn2->
						  remote_msgqueue_base);
		if (ch_sn2->remote_msgqueue == NULL)
			continue;

		spin_lock_irqsave(&ch->lock, irq_flags);
		if (nentries < ch->remote_nentries) {
			dev_dbg(xpc_chan, "nentries=%d remote_nentries=%d, "
				"partid=%d, channel=%d\n", nentries,
				ch->remote_nentries, ch->partid, ch->number);

			ch->remote_nentries = nentries;
		}
		spin_unlock_irqrestore(&ch->lock, irq_flags);
		return xpSuccess;
	}

	dev_dbg(xpc_chan, "can't get memory for cached remote message queue, "
		"partid=%d, channel=%d\n", ch->partid, ch->number);
	return xpNoMemory;
}

static enum xp_retval
xpc_setup_msg_structures_sn2(struct xpc_channel *ch)
{
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	enum xp_retval ret;

	DBUG_ON(ch->flags & XPC_C_SETUP);

	ret = xpc_allocate_local_msgqueue_sn2(ch);
	if (ret == xpSuccess) {

		ret = xpc_allocate_remote_msgqueue_sn2(ch);
		if (ret != xpSuccess) {
			kfree(ch_sn2->local_msgqueue_base);
			ch_sn2->local_msgqueue = NULL;
			kfree(ch_sn2->notify_queue);
			ch_sn2->notify_queue = NULL;
		}
	}
	return ret;
}

static void
xpc_teardown_msg_structures_sn2(struct xpc_channel *ch)
{
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;

	DBUG_ON(!spin_is_locked(&ch->lock));

	ch_sn2->remote_msgqueue_pa = 0;

	ch_sn2->local_GP->get = 0;
	ch_sn2->local_GP->put = 0;
	ch_sn2->remote_GP.get = 0;
	ch_sn2->remote_GP.put = 0;
	ch_sn2->w_local_GP.get = 0;
	ch_sn2->w_local_GP.put = 0;
	ch_sn2->w_remote_GP.get = 0;
	ch_sn2->w_remote_GP.put = 0;
	ch_sn2->next_msg_to_pull = 0;

	if (ch->flags & XPC_C_SETUP) {
		dev_dbg(xpc_chan, "ch->flags=0x%x, partid=%d, channel=%d\n",
			ch->flags, ch->partid, ch->number);

		kfree(ch_sn2->local_msgqueue_base);
		ch_sn2->local_msgqueue = NULL;
		kfree(ch_sn2->remote_msgqueue_base);
		ch_sn2->remote_msgqueue = NULL;
		kfree(ch_sn2->notify_queue);
		ch_sn2->notify_queue = NULL;
	}
}

static void
xpc_notify_senders_sn2(struct xpc_channel *ch, enum xp_retval reason, s64 put)
{
	struct xpc_notify_sn2 *notify;
	u8 notify_type;
	s64 get = ch->sn.sn2.w_remote_GP.get - 1;

	while (++get < put && atomic_read(&ch->n_to_notify) > 0) {

		notify = &ch->sn.sn2.notify_queue[get % ch->local_nentries];

		notify_type = notify->type;
		if (notify_type == 0 ||
		    cmpxchg(&notify->type, notify_type, 0) != notify_type) {
			continue;
		}

		DBUG_ON(notify_type != XPC_N_CALL);

		atomic_dec(&ch->n_to_notify);

		if (notify->func != NULL) {
			dev_dbg(xpc_chan, "notify->func() called, notify=0x%p "
				"msg_number=%lld partid=%d channel=%d\n",
				(void *)notify, get, ch->partid, ch->number);

			notify->func(reason, ch->partid, ch->number,
				     notify->key);

			dev_dbg(xpc_chan, "notify->func() returned, notify=0x%p"
				" msg_number=%lld partid=%d channel=%d\n",
				(void *)notify, get, ch->partid, ch->number);
		}
	}
}

static void
xpc_notify_senders_of_disconnect_sn2(struct xpc_channel *ch)
{
	xpc_notify_senders_sn2(ch, ch->reason, ch->sn.sn2.w_local_GP.put);
}

static inline void
xpc_clear_local_msgqueue_flags_sn2(struct xpc_channel *ch)
{
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	struct xpc_msg_sn2 *msg;
	s64 get;

	get = ch_sn2->w_remote_GP.get;
	do {
		msg = (struct xpc_msg_sn2 *)((u64)ch_sn2->local_msgqueue +
					     (get % ch->local_nentries) *
					     ch->entry_size);
		DBUG_ON(!(msg->flags & XPC_M_SN2_READY));
		msg->flags = 0;
	} while (++get < ch_sn2->remote_GP.get);
}

static inline void
xpc_clear_remote_msgqueue_flags_sn2(struct xpc_channel *ch)
{
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	struct xpc_msg_sn2 *msg;
	s64 put, remote_nentries = ch->remote_nentries;

	
	if (ch_sn2->remote_GP.put < remote_nentries)
		return;

	put = max(ch_sn2->w_remote_GP.put, remote_nentries);
	do {
		msg = (struct xpc_msg_sn2 *)((u64)ch_sn2->remote_msgqueue +
					     (put % remote_nentries) *
					     ch->entry_size);
		DBUG_ON(!(msg->flags & XPC_M_SN2_READY));
		DBUG_ON(!(msg->flags & XPC_M_SN2_DONE));
		DBUG_ON(msg->number != put - remote_nentries);
		msg->flags = 0;
	} while (++put < ch_sn2->remote_GP.put);
}

static int
xpc_n_of_deliverable_payloads_sn2(struct xpc_channel *ch)
{
	return ch->sn.sn2.w_remote_GP.put - ch->sn.sn2.w_local_GP.get;
}

static void
xpc_process_msg_chctl_flags_sn2(struct xpc_partition *part, int ch_number)
{
	struct xpc_channel *ch = &part->channels[ch_number];
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	int npayloads_sent;

	ch_sn2->remote_GP = part->sn.sn2.remote_GPs[ch_number];

	

	xpc_msgqueue_ref(ch);

	if (ch_sn2->w_remote_GP.get == ch_sn2->remote_GP.get &&
	    ch_sn2->w_remote_GP.put == ch_sn2->remote_GP.put) {
		
		xpc_msgqueue_deref(ch);
		return;
	}

	if (!(ch->flags & XPC_C_CONNECTED)) {
		xpc_msgqueue_deref(ch);
		return;
	}


	if (ch_sn2->w_remote_GP.get != ch_sn2->remote_GP.get) {

		if (atomic_read(&ch->n_to_notify) > 0) {
			xpc_notify_senders_sn2(ch, xpMsgDelivered,
					       ch_sn2->remote_GP.get);
		}

		xpc_clear_local_msgqueue_flags_sn2(ch);

		ch_sn2->w_remote_GP.get = ch_sn2->remote_GP.get;

		dev_dbg(xpc_chan, "w_remote_GP.get changed to %lld, partid=%d, "
			"channel=%d\n", ch_sn2->w_remote_GP.get, ch->partid,
			ch->number);

		if (atomic_read(&ch->n_on_msg_allocate_wq) > 0)
			wake_up(&ch->msg_allocate_wq);
	}


	if (ch_sn2->w_remote_GP.put != ch_sn2->remote_GP.put) {
		xpc_clear_remote_msgqueue_flags_sn2(ch);

		smp_wmb(); 
		ch_sn2->w_remote_GP.put = ch_sn2->remote_GP.put;

		dev_dbg(xpc_chan, "w_remote_GP.put changed to %lld, partid=%d, "
			"channel=%d\n", ch_sn2->w_remote_GP.put, ch->partid,
			ch->number);

		npayloads_sent = xpc_n_of_deliverable_payloads_sn2(ch);
		if (npayloads_sent > 0) {
			dev_dbg(xpc_chan, "msgs waiting to be copied and "
				"delivered=%d, partid=%d, channel=%d\n",
				npayloads_sent, ch->partid, ch->number);

			if (ch->flags & XPC_C_CONNECTEDCALLOUT_MADE)
				xpc_activate_kthreads(ch, npayloads_sent);
		}
	}

	xpc_msgqueue_deref(ch);
}

static struct xpc_msg_sn2 *
xpc_pull_remote_msg_sn2(struct xpc_channel *ch, s64 get)
{
	struct xpc_partition *part = &xpc_partitions[ch->partid];
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	unsigned long remote_msg_pa;
	struct xpc_msg_sn2 *msg;
	u32 msg_index;
	u32 nmsgs;
	u64 msg_offset;
	enum xp_retval ret;

	if (mutex_lock_interruptible(&ch_sn2->msg_to_pull_mutex) != 0) {
		
		return NULL;
	}

	while (get >= ch_sn2->next_msg_to_pull) {

		

		msg_index = ch_sn2->next_msg_to_pull % ch->remote_nentries;

		DBUG_ON(ch_sn2->next_msg_to_pull >= ch_sn2->w_remote_GP.put);
		nmsgs = ch_sn2->w_remote_GP.put - ch_sn2->next_msg_to_pull;
		if (msg_index + nmsgs > ch->remote_nentries) {
			
			nmsgs = ch->remote_nentries - msg_index;
		}

		msg_offset = msg_index * ch->entry_size;
		msg = (struct xpc_msg_sn2 *)((u64)ch_sn2->remote_msgqueue +
		    msg_offset);
		remote_msg_pa = ch_sn2->remote_msgqueue_pa + msg_offset;

		ret = xpc_pull_remote_cachelines_sn2(part, msg, remote_msg_pa,
						     nmsgs * ch->entry_size);
		if (ret != xpSuccess) {

			dev_dbg(xpc_chan, "failed to pull %d msgs starting with"
				" msg %lld from partition %d, channel=%d, "
				"ret=%d\n", nmsgs, ch_sn2->next_msg_to_pull,
				ch->partid, ch->number, ret);

			XPC_DEACTIVATE_PARTITION(part, ret);

			mutex_unlock(&ch_sn2->msg_to_pull_mutex);
			return NULL;
		}

		ch_sn2->next_msg_to_pull += nmsgs;
	}

	mutex_unlock(&ch_sn2->msg_to_pull_mutex);

	
	msg_offset = (get % ch->remote_nentries) * ch->entry_size;
	msg = (struct xpc_msg_sn2 *)((u64)ch_sn2->remote_msgqueue + msg_offset);

	return msg;
}

static void *
xpc_get_deliverable_payload_sn2(struct xpc_channel *ch)
{
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	struct xpc_msg_sn2 *msg;
	void *payload = NULL;
	s64 get;

	do {
		if (ch->flags & XPC_C_DISCONNECTING)
			break;

		get = ch_sn2->w_local_GP.get;
		smp_rmb();	
		if (get == ch_sn2->w_remote_GP.put)
			break;


		if (cmpxchg(&ch_sn2->w_local_GP.get, get, get + 1) == get) {
			

			dev_dbg(xpc_chan, "w_local_GP.get changed to %lld, "
				"partid=%d, channel=%d\n", get + 1,
				ch->partid, ch->number);

			

			msg = xpc_pull_remote_msg_sn2(ch, get);

			if (msg != NULL) {
				DBUG_ON(msg->number != get);
				DBUG_ON(msg->flags & XPC_M_SN2_DONE);
				DBUG_ON(!(msg->flags & XPC_M_SN2_READY));

				payload = &msg->payload;
			}
			break;
		}

	} while (1);

	return payload;
}

static void
xpc_send_msgs_sn2(struct xpc_channel *ch, s64 initial_put)
{
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	struct xpc_msg_sn2 *msg;
	s64 put = initial_put + 1;
	int send_msgrequest = 0;

	while (1) {

		while (1) {
			if (put == ch_sn2->w_local_GP.put)
				break;

			msg = (struct xpc_msg_sn2 *)((u64)ch_sn2->
						     local_msgqueue + (put %
						     ch->local_nentries) *
						     ch->entry_size);

			if (!(msg->flags & XPC_M_SN2_READY))
				break;

			put++;
		}

		if (put == initial_put) {
			
			break;
		}

		if (cmpxchg_rel(&ch_sn2->local_GP->put, initial_put, put) !=
		    initial_put) {
			
			DBUG_ON(ch_sn2->local_GP->put < initial_put);
			break;
		}

		

		dev_dbg(xpc_chan, "local_GP->put changed to %lld, partid=%d, "
			"channel=%d\n", put, ch->partid, ch->number);

		send_msgrequest = 1;

		initial_put = put;
	}

	if (send_msgrequest)
		xpc_send_chctl_msgrequest_sn2(ch);
}

static enum xp_retval
xpc_allocate_msg_sn2(struct xpc_channel *ch, u32 flags,
		     struct xpc_msg_sn2 **address_of_msg)
{
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	struct xpc_msg_sn2 *msg;
	enum xp_retval ret;
	s64 put;

	ret = xpTimeout;

	while (1) {

		put = ch_sn2->w_local_GP.put;
		smp_rmb();	
		if (put - ch_sn2->w_remote_GP.get < ch->local_nentries) {

			if (cmpxchg(&ch_sn2->w_local_GP.put, put, put + 1) ==
			    put) {
				
				break;
			}
			continue;	
		}

		if (ret == xpTimeout)
			xpc_send_chctl_local_msgrequest_sn2(ch);

		if (flags & XPC_NOWAIT)
			return xpNoWait;

		ret = xpc_allocate_msg_wait(ch);
		if (ret != xpInterrupted && ret != xpTimeout)
			return ret;
	}

	
	msg = (struct xpc_msg_sn2 *)((u64)ch_sn2->local_msgqueue +
				     (put % ch->local_nentries) *
				     ch->entry_size);

	DBUG_ON(msg->flags != 0);
	msg->number = put;

	dev_dbg(xpc_chan, "w_local_GP.put changed to %lld; msg=0x%p, "
		"msg_number=%lld, partid=%d, channel=%d\n", put + 1,
		(void *)msg, msg->number, ch->partid, ch->number);

	*address_of_msg = msg;
	return xpSuccess;
}

static enum xp_retval
xpc_send_payload_sn2(struct xpc_channel *ch, u32 flags, void *payload,
		     u16 payload_size, u8 notify_type, xpc_notify_func func,
		     void *key)
{
	enum xp_retval ret = xpSuccess;
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	struct xpc_msg_sn2 *msg = msg;
	struct xpc_notify_sn2 *notify = notify;
	s64 msg_number;
	s64 put;

	DBUG_ON(notify_type == XPC_N_CALL && func == NULL);

	if (XPC_MSG_SIZE(payload_size) > ch->entry_size)
		return xpPayloadTooBig;

	xpc_msgqueue_ref(ch);

	if (ch->flags & XPC_C_DISCONNECTING) {
		ret = ch->reason;
		goto out_1;
	}
	if (!(ch->flags & XPC_C_CONNECTED)) {
		ret = xpNotConnected;
		goto out_1;
	}

	ret = xpc_allocate_msg_sn2(ch, flags, &msg);
	if (ret != xpSuccess)
		goto out_1;

	msg_number = msg->number;

	if (notify_type != 0) {
		msg->flags |= XPC_M_SN2_INTERRUPT;

		atomic_inc(&ch->n_to_notify);

		notify = &ch_sn2->notify_queue[msg_number % ch->local_nentries];
		notify->func = func;
		notify->key = key;
		notify->type = notify_type;

		

		if (ch->flags & XPC_C_DISCONNECTING) {
			if (cmpxchg(&notify->type, notify_type, 0) ==
			    notify_type) {
				atomic_dec(&ch->n_to_notify);
				ret = ch->reason;
			}
			goto out_1;
		}
	}

	memcpy(&msg->payload, payload, payload_size);

	msg->flags |= XPC_M_SN2_READY;

	smp_mb();

	

	put = ch_sn2->local_GP->put;
	if (put == msg_number)
		xpc_send_msgs_sn2(ch, put);

out_1:
	xpc_msgqueue_deref(ch);
	return ret;
}

static void
xpc_acknowledge_msgs_sn2(struct xpc_channel *ch, s64 initial_get, u8 msg_flags)
{
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	struct xpc_msg_sn2 *msg;
	s64 get = initial_get + 1;
	int send_msgrequest = 0;

	while (1) {

		while (1) {
			if (get == ch_sn2->w_local_GP.get)
				break;

			msg = (struct xpc_msg_sn2 *)((u64)ch_sn2->
						     remote_msgqueue + (get %
						     ch->remote_nentries) *
						     ch->entry_size);

			if (!(msg->flags & XPC_M_SN2_DONE))
				break;

			msg_flags |= msg->flags;
			get++;
		}

		if (get == initial_get) {
			
			break;
		}

		if (cmpxchg_rel(&ch_sn2->local_GP->get, initial_get, get) !=
		    initial_get) {
			
			DBUG_ON(ch_sn2->local_GP->get <= initial_get);
			break;
		}

		

		dev_dbg(xpc_chan, "local_GP->get changed to %lld, partid=%d, "
			"channel=%d\n", get, ch->partid, ch->number);

		send_msgrequest = (msg_flags & XPC_M_SN2_INTERRUPT);

		initial_get = get;
	}

	if (send_msgrequest)
		xpc_send_chctl_msgrequest_sn2(ch);
}

static void
xpc_received_payload_sn2(struct xpc_channel *ch, void *payload)
{
	struct xpc_msg_sn2 *msg;
	s64 msg_number;
	s64 get;

	msg = container_of(payload, struct xpc_msg_sn2, payload);
	msg_number = msg->number;

	dev_dbg(xpc_chan, "msg=0x%p, msg_number=%lld, partid=%d, channel=%d\n",
		(void *)msg, msg_number, ch->partid, ch->number);

	DBUG_ON((((u64)msg - (u64)ch->sn.sn2.remote_msgqueue) / ch->entry_size) !=
		msg_number % ch->remote_nentries);
	DBUG_ON(!(msg->flags & XPC_M_SN2_READY));
	DBUG_ON(msg->flags & XPC_M_SN2_DONE);

	msg->flags |= XPC_M_SN2_DONE;

	smp_mb();

	get = ch->sn.sn2.local_GP->get;
	if (get == msg_number)
		xpc_acknowledge_msgs_sn2(ch, get, msg->flags);
}

static struct xpc_arch_operations xpc_arch_ops_sn2 = {
	.setup_partitions = xpc_setup_partitions_sn2,
	.teardown_partitions = xpc_teardown_partitions_sn2,
	.process_activate_IRQ_rcvd = xpc_process_activate_IRQ_rcvd_sn2,
	.get_partition_rsvd_page_pa = xpc_get_partition_rsvd_page_pa_sn2,
	.setup_rsvd_page = xpc_setup_rsvd_page_sn2,

	.allow_hb = xpc_allow_hb_sn2,
	.disallow_hb = xpc_disallow_hb_sn2,
	.disallow_all_hbs = xpc_disallow_all_hbs_sn2,
	.increment_heartbeat = xpc_increment_heartbeat_sn2,
	.offline_heartbeat = xpc_offline_heartbeat_sn2,
	.online_heartbeat = xpc_online_heartbeat_sn2,
	.heartbeat_init = xpc_heartbeat_init_sn2,
	.heartbeat_exit = xpc_heartbeat_exit_sn2,
	.get_remote_heartbeat = xpc_get_remote_heartbeat_sn2,

	.request_partition_activation =
		xpc_request_partition_activation_sn2,
	.request_partition_reactivation =
		xpc_request_partition_reactivation_sn2,
	.request_partition_deactivation =
		xpc_request_partition_deactivation_sn2,
	.cancel_partition_deactivation_request =
		xpc_cancel_partition_deactivation_request_sn2,

	.setup_ch_structures = xpc_setup_ch_structures_sn2,
	.teardown_ch_structures = xpc_teardown_ch_structures_sn2,

	.make_first_contact = xpc_make_first_contact_sn2,

	.get_chctl_all_flags = xpc_get_chctl_all_flags_sn2,
	.send_chctl_closerequest = xpc_send_chctl_closerequest_sn2,
	.send_chctl_closereply = xpc_send_chctl_closereply_sn2,
	.send_chctl_openrequest = xpc_send_chctl_openrequest_sn2,
	.send_chctl_openreply = xpc_send_chctl_openreply_sn2,
	.send_chctl_opencomplete = xpc_send_chctl_opencomplete_sn2,
	.process_msg_chctl_flags = xpc_process_msg_chctl_flags_sn2,

	.save_remote_msgqueue_pa = xpc_save_remote_msgqueue_pa_sn2,

	.setup_msg_structures = xpc_setup_msg_structures_sn2,
	.teardown_msg_structures = xpc_teardown_msg_structures_sn2,

	.indicate_partition_engaged = xpc_indicate_partition_engaged_sn2,
	.indicate_partition_disengaged = xpc_indicate_partition_disengaged_sn2,
	.partition_engaged = xpc_partition_engaged_sn2,
	.any_partition_engaged = xpc_any_partition_engaged_sn2,
	.assume_partition_disengaged = xpc_assume_partition_disengaged_sn2,

	.n_of_deliverable_payloads = xpc_n_of_deliverable_payloads_sn2,
	.send_payload = xpc_send_payload_sn2,
	.get_deliverable_payload = xpc_get_deliverable_payload_sn2,
	.received_payload = xpc_received_payload_sn2,
	.notify_senders_of_disconnect = xpc_notify_senders_of_disconnect_sn2,
};

int
xpc_init_sn2(void)
{
	int ret;
	size_t buf_size;

	xpc_arch_ops = xpc_arch_ops_sn2;

	if (offsetof(struct xpc_msg_sn2, payload) > XPC_MSG_HDR_MAX_SIZE) {
		dev_err(xpc_part, "header portion of struct xpc_msg_sn2 is "
			"larger than %d\n", XPC_MSG_HDR_MAX_SIZE);
		return -E2BIG;
	}

	buf_size = max(XPC_RP_VARS_SIZE,
		       XPC_RP_HEADER_SIZE + XP_NASID_MASK_BYTES_SN2);
	xpc_remote_copy_buffer_sn2 = xpc_kmalloc_cacheline_aligned(buf_size,
								   GFP_KERNEL,
					      &xpc_remote_copy_buffer_base_sn2);
	if (xpc_remote_copy_buffer_sn2 == NULL) {
		dev_err(xpc_part, "can't get memory for remote copy buffer\n");
		return -ENOMEM;
	}

	
	xpc_allow_IPI_ops_sn2();
	xpc_allow_amo_ops_shub_wars_1_1_sn2();

	ret = request_irq(SGI_XPC_ACTIVATE, xpc_handle_activate_IRQ_sn2, 0,
			  "xpc hb", NULL);
	if (ret != 0) {
		dev_err(xpc_part, "can't register ACTIVATE IRQ handler, "
			"errno=%d\n", -ret);
		xpc_disallow_IPI_ops_sn2();
		kfree(xpc_remote_copy_buffer_base_sn2);
	}
	return ret;
}

void
xpc_exit_sn2(void)
{
	free_irq(SGI_XPC_ACTIVATE, NULL);
	xpc_disallow_IPI_ops_sn2();
	kfree(xpc_remote_copy_buffer_base_sn2);
}
