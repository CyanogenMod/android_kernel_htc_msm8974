/* arch/arm/mach-msm/smp2p_loopback.c
 *
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/ctype.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/termios.h>
#include <linux/module.h>
#include <linux/remote_spinlock.h>
#include "smem_private.h"
#include "smp2p_private.h"

struct smp2p_loopback_ctx {
	int proc_id;
	struct msm_smp2p_out *out;
	struct notifier_block out_nb;
	bool out_is_active;
	struct notifier_block in_nb;
	bool in_is_active;
	struct work_struct  rmt_lpb_work;
	struct msm_smp2p_update_notif rmt_cmd;
};

static struct smp2p_loopback_ctx  remote_loopback[SMP2P_NUM_PROCS];
static struct msm_smp2p_remote_mock remote_mock;

static void remote_spinlock_test(struct smp2p_loopback_ctx *ctx)
{
	uint32_t test_request;
	uint32_t test_response;
	unsigned long flags;
	int n;
	unsigned lock_count = 0;
	remote_spinlock_t *smem_spinlock;

	test_request = 0x0;
	SMP2P_SET_RMT_CMD_TYPE_REQ(test_request);
	smem_spinlock = smem_get_remote_spinlock();
	if (!smem_spinlock) {
		pr_err("%s: unable to get remote spinlock\n", __func__);
		return;
	}

	for (;;) {
		remote_spin_lock_irqsave(smem_spinlock, flags);
		++lock_count;
		SMP2P_SET_RMT_CMD(test_request, SMP2P_LB_CMD_RSPIN_LOCKED);
		(void)msm_smp2p_out_write(ctx->out, test_request);

		for (n = 0; n < 10000; ++n) {
			(void)msm_smp2p_in_read(ctx->proc_id,
					"smp2p", &test_response);
			test_response = SMP2P_GET_RMT_CMD(test_response);

			if (test_response == SMP2P_LB_CMD_RSPIN_END)
				break;

			if (test_response != SMP2P_LB_CMD_RSPIN_UNLOCKED)
				SMP2P_ERR("%s: invalid spinlock command %x\n",
					__func__, test_response);
		}

		if (test_response == SMP2P_LB_CMD_RSPIN_END) {
			SMP2P_SET_RMT_CMD_TYPE_RESP(test_request);
			SMP2P_SET_RMT_CMD(test_request,
					SMP2P_LB_CMD_RSPIN_END);
			SMP2P_SET_RMT_DATA(test_request, lock_count);
			(void)msm_smp2p_out_write(ctx->out, test_request);
			break;
		}

		SMP2P_SET_RMT_CMD(test_request, SMP2P_LB_CMD_RSPIN_UNLOCKED);
		(void)msm_smp2p_out_write(ctx->out, test_request);
		remote_spin_unlock_irqrestore(smem_spinlock, flags);
	}
	remote_spin_unlock_irqrestore(smem_spinlock, flags);
}

static void smp2p_rmt_lpb_worker(struct work_struct *work)
{
	struct smp2p_loopback_ctx *ctx;
	int lpb_cmd;
	int lpb_cmd_type;
	int lpb_data;

	ctx = container_of(work, struct smp2p_loopback_ctx, rmt_lpb_work);

	if (!ctx->in_is_active || !ctx->out_is_active)
		return;

	if (ctx->rmt_cmd.previous_value == ctx->rmt_cmd.current_value)
		return;

	lpb_cmd_type =  SMP2P_GET_RMT_CMD_TYPE(ctx->rmt_cmd.current_value);
	lpb_cmd = SMP2P_GET_RMT_CMD(ctx->rmt_cmd.current_value);
	lpb_data = SMP2P_GET_RMT_DATA(ctx->rmt_cmd.current_value);

	if (lpb_cmd & SMP2P_RLPB_IGNORE)
		return;

	switch (lpb_cmd) {
	case SMP2P_LB_CMD_NOOP:
	    
	    break;

	case SMP2P_LB_CMD_ECHO:
		SMP2P_SET_RMT_CMD_TYPE(ctx->rmt_cmd.current_value, 0);
		SMP2P_SET_RMT_DATA(ctx->rmt_cmd.current_value,
							lpb_data);
		(void)msm_smp2p_out_write(ctx->out,
					ctx->rmt_cmd.current_value);
	    break;

	case SMP2P_LB_CMD_CLEARALL:
		ctx->rmt_cmd.current_value = 0;
		(void)msm_smp2p_out_write(ctx->out,
					ctx->rmt_cmd.current_value);
	    break;

	case SMP2P_LB_CMD_PINGPONG:
		SMP2P_SET_RMT_CMD_TYPE(ctx->rmt_cmd.current_value, 0);
		if (lpb_data) {
			lpb_data--;
			SMP2P_SET_RMT_DATA(ctx->rmt_cmd.current_value,
					lpb_data);
			(void)msm_smp2p_out_write(ctx->out,
					ctx->rmt_cmd.current_value);
		}
	    break;

	case SMP2P_LB_CMD_RSPIN_START:
		remote_spinlock_test(ctx);
		break;

	case SMP2P_LB_CMD_RSPIN_LOCKED:
	case SMP2P_LB_CMD_RSPIN_UNLOCKED:
	case SMP2P_LB_CMD_RSPIN_END:
		
		break;

	default:
		SMP2P_DBG("%s: Unknown loopback command %x\n",
				__func__, lpb_cmd);
		break;
	}
}

static int smp2p_rmt_in_edge_notify(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct smp2p_loopback_ctx *ctx;

	if (!(event == SMP2P_ENTRY_UPDATE || event == SMP2P_OPEN))
		return 0;

	ctx = container_of(nb, struct smp2p_loopback_ctx, in_nb);
	if (data && ctx->in_is_active) {
			ctx->rmt_cmd =
			    *(struct msm_smp2p_update_notif *)data;
			schedule_work(&ctx->rmt_lpb_work);
	}

	return 0;
}

static int smp2p_rmt_out_edge_notify(struct notifier_block  *nb,
				unsigned long event, void *data)
{
	struct smp2p_loopback_ctx *ctx;

	ctx = container_of(nb, struct smp2p_loopback_ctx, out_nb);
	if (event == SMP2P_OPEN)
		SMP2P_DBG("%s: 'smp2p':%d opened\n", __func__,
				ctx->proc_id);

	return 0;
}

static int msm_smp2p_init_rmt_lpb(struct  smp2p_loopback_ctx *ctx,
			int pid, const char *entry)
{
	int ret = 0;
	int tmp;

	if (!ctx || !entry || pid > SMP2P_NUM_PROCS)
		return -EINVAL;

	ctx->in_nb.notifier_call = smp2p_rmt_in_edge_notify;
	ctx->out_nb.notifier_call = smp2p_rmt_out_edge_notify;
	ctx->proc_id = pid;
	ctx->in_is_active = true;
	ctx->out_is_active = true;
	tmp = msm_smp2p_out_open(pid, entry, &ctx->out_nb,
						&ctx->out);
	if (tmp) {
		SMP2P_ERR("%s: open failed outbound entry '%s':%d - ret %d\n",
				__func__, entry, pid, tmp);
		ret = tmp;
	}

	tmp = msm_smp2p_in_register(ctx->proc_id,
				SMP2P_RLPB_ENTRY_NAME,
				&ctx->in_nb);
	if (tmp) {
		SMP2P_ERR("%s: unable to open inbound entry '%s':%d - ret %d\n",
				__func__, entry, pid, tmp);
		ret = tmp;
	}

	return ret;
}

void *msm_smp2p_init_rmt_lpb_proc(int remote_pid)
{
	int tmp;
	void *ret = NULL;

	tmp = msm_smp2p_init_rmt_lpb(&remote_loopback[remote_pid],
			remote_pid, SMP2P_RLPB_ENTRY_NAME);
	if (!tmp)
		ret = remote_loopback[remote_pid].out;

	return ret;
}
EXPORT_SYMBOL(msm_smp2p_init_rmt_lpb_proc);

int msm_smp2p_deinit_rmt_lpb_proc(int remote_pid)
{
	int ret = 0;
	int tmp;
	struct smp2p_loopback_ctx *ctx;

	if (remote_pid >= SMP2P_NUM_PROCS)
		return -EINVAL;

	ctx = &remote_loopback[remote_pid];

	
	remote_loopback[remote_pid].out_is_active = false;
	remote_loopback[remote_pid].in_is_active = false;
	flush_work(&ctx->rmt_lpb_work);

	
	tmp = msm_smp2p_out_close(&remote_loopback[remote_pid].out);
	remote_loopback[remote_pid].out = NULL;
	if (tmp) {
		SMP2P_ERR("%s: outbound 'smp2p':%d close failed %d\n",
				__func__, remote_pid, tmp);
		ret = tmp;
	}

	tmp = msm_smp2p_in_unregister(remote_pid,
		SMP2P_RLPB_ENTRY_NAME, &remote_loopback[remote_pid].in_nb);
	if (tmp) {
		SMP2P_ERR("%s: inbound 'smp2p':%d close failed %d\n",
				__func__, remote_pid, tmp);
		ret = tmp;
	}

	return ret;
}
EXPORT_SYMBOL(msm_smp2p_deinit_rmt_lpb_proc);

void msm_smp2p_set_remote_mock_exists(bool item_exists)
{
	remote_mock.item_exists = item_exists;
}
EXPORT_SYMBOL(msm_smp2p_set_remote_mock_exists);

void *msm_smp2p_get_remote_mock(void)
{
	return &remote_mock;
}
EXPORT_SYMBOL(msm_smp2p_get_remote_mock);

void *msm_smp2p_get_remote_mock_smem_item(uint32_t *size)
{
	void *ptr = NULL;
	if (remote_mock.item_exists) {
		*size = sizeof(remote_mock.remote_item);
		ptr = &(remote_mock.remote_item);
	}

	return ptr;
}
EXPORT_SYMBOL(msm_smp2p_get_remote_mock_smem_item);

int smp2p_remote_mock_rx_interrupt(void)
{
	remote_mock.rx_interrupt_count++;
	if (remote_mock.initialized)
		complete(&remote_mock.cb_completion);
	return 0;
}
EXPORT_SYMBOL(smp2p_remote_mock_rx_interrupt);

static void smp2p_remote_mock_tx_interrupt(void)
{
	msm_smp2p_interrupt_handler(SMP2P_REMOTE_MOCK_PROC);
}

static int __init smp2p_remote_mock_init(void)
{
	int i;

	smp2p_init_header(&remote_mock.remote_item.header,
			SMP2P_REMOTE_MOCK_PROC, SMP2P_APPS_PROC,
			0, 0);
	remote_mock.rx_interrupt_count = 0;
	remote_mock.rx_interrupt = smp2p_remote_mock_rx_interrupt;
	remote_mock.tx_interrupt = smp2p_remote_mock_tx_interrupt;
	remote_mock.item_exists = false;
	init_completion(&remote_mock.cb_completion);
	remote_mock.initialized = true;

	for (i = 0; i < SMP2P_NUM_PROCS; i++) {
		INIT_WORK(&(remote_loopback[i].rmt_lpb_work),
				smp2p_rmt_lpb_worker);
		if (i == SMP2P_REMOTE_MOCK_PROC)
			
			continue;

		msm_smp2p_init_rmt_lpb(&remote_loopback[i],
			i, SMP2P_RLPB_ENTRY_NAME);
	}
	return 0;
}
module_init(smp2p_remote_mock_init);
