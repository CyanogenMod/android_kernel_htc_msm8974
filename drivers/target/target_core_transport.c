/*******************************************************************************
 * Filename:  target_core_transport.c
 *
 * This file contains the Generic Target Engine Core.
 *
 * Copyright (c) 2002, 2003, 2004, 2005 PyX Technologies, Inc.
 * Copyright (c) 2005, 2006, 2007 SBE, Inc.
 * Copyright (c) 2007-2010 Rising Tide Systems
 * Copyright (c) 2008-2010 Linux-iSCSI.org
 *
 * Nicholas A. Bellinger <nab@kernel.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 ******************************************************************************/

#include <linux/net.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/in.h>
#include <linux/cdrom.h>
#include <linux/module.h>
#include <linux/ratelimit.h>
#include <asm/unaligned.h>
#include <net/sock.h>
#include <net/tcp.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_tcq.h>

#include <target/target_core_base.h>
#include <target/target_core_backend.h>
#include <target/target_core_fabric.h>
#include <target/target_core_configfs.h>

#include "target_core_internal.h"
#include "target_core_alua.h"
#include "target_core_pr.h"
#include "target_core_ua.h"

static int sub_api_initialized;

static struct workqueue_struct *target_completion_wq;
static struct kmem_cache *se_sess_cache;
struct kmem_cache *se_ua_cache;
struct kmem_cache *t10_pr_reg_cache;
struct kmem_cache *t10_alua_lu_gp_cache;
struct kmem_cache *t10_alua_lu_gp_mem_cache;
struct kmem_cache *t10_alua_tg_pt_gp_cache;
struct kmem_cache *t10_alua_tg_pt_gp_mem_cache;

static int transport_generic_write_pending(struct se_cmd *);
static int transport_processing_thread(void *param);
static int __transport_execute_tasks(struct se_device *dev, struct se_cmd *);
static void transport_complete_task_attr(struct se_cmd *cmd);
static void transport_handle_queue_full(struct se_cmd *cmd,
		struct se_device *dev);
static void transport_free_dev_tasks(struct se_cmd *cmd);
static int transport_generic_get_mem(struct se_cmd *cmd);
static void transport_put_cmd(struct se_cmd *cmd);
static void transport_remove_cmd_from_queue(struct se_cmd *cmd);
static int transport_set_sense_codes(struct se_cmd *cmd, u8 asc, u8 ascq);
static void target_complete_ok_work(struct work_struct *work);

int init_se_kmem_caches(void)
{
	se_sess_cache = kmem_cache_create("se_sess_cache",
			sizeof(struct se_session), __alignof__(struct se_session),
			0, NULL);
	if (!se_sess_cache) {
		pr_err("kmem_cache_create() for struct se_session"
				" failed\n");
		goto out;
	}
	se_ua_cache = kmem_cache_create("se_ua_cache",
			sizeof(struct se_ua), __alignof__(struct se_ua),
			0, NULL);
	if (!se_ua_cache) {
		pr_err("kmem_cache_create() for struct se_ua failed\n");
		goto out_free_sess_cache;
	}
	t10_pr_reg_cache = kmem_cache_create("t10_pr_reg_cache",
			sizeof(struct t10_pr_registration),
			__alignof__(struct t10_pr_registration), 0, NULL);
	if (!t10_pr_reg_cache) {
		pr_err("kmem_cache_create() for struct t10_pr_registration"
				" failed\n");
		goto out_free_ua_cache;
	}
	t10_alua_lu_gp_cache = kmem_cache_create("t10_alua_lu_gp_cache",
			sizeof(struct t10_alua_lu_gp), __alignof__(struct t10_alua_lu_gp),
			0, NULL);
	if (!t10_alua_lu_gp_cache) {
		pr_err("kmem_cache_create() for t10_alua_lu_gp_cache"
				" failed\n");
		goto out_free_pr_reg_cache;
	}
	t10_alua_lu_gp_mem_cache = kmem_cache_create("t10_alua_lu_gp_mem_cache",
			sizeof(struct t10_alua_lu_gp_member),
			__alignof__(struct t10_alua_lu_gp_member), 0, NULL);
	if (!t10_alua_lu_gp_mem_cache) {
		pr_err("kmem_cache_create() for t10_alua_lu_gp_mem_"
				"cache failed\n");
		goto out_free_lu_gp_cache;
	}
	t10_alua_tg_pt_gp_cache = kmem_cache_create("t10_alua_tg_pt_gp_cache",
			sizeof(struct t10_alua_tg_pt_gp),
			__alignof__(struct t10_alua_tg_pt_gp), 0, NULL);
	if (!t10_alua_tg_pt_gp_cache) {
		pr_err("kmem_cache_create() for t10_alua_tg_pt_gp_"
				"cache failed\n");
		goto out_free_lu_gp_mem_cache;
	}
	t10_alua_tg_pt_gp_mem_cache = kmem_cache_create(
			"t10_alua_tg_pt_gp_mem_cache",
			sizeof(struct t10_alua_tg_pt_gp_member),
			__alignof__(struct t10_alua_tg_pt_gp_member),
			0, NULL);
	if (!t10_alua_tg_pt_gp_mem_cache) {
		pr_err("kmem_cache_create() for t10_alua_tg_pt_gp_"
				"mem_t failed\n");
		goto out_free_tg_pt_gp_cache;
	}

	target_completion_wq = alloc_workqueue("target_completion",
					       WQ_MEM_RECLAIM, 0);
	if (!target_completion_wq)
		goto out_free_tg_pt_gp_mem_cache;

	return 0;

out_free_tg_pt_gp_mem_cache:
	kmem_cache_destroy(t10_alua_tg_pt_gp_mem_cache);
out_free_tg_pt_gp_cache:
	kmem_cache_destroy(t10_alua_tg_pt_gp_cache);
out_free_lu_gp_mem_cache:
	kmem_cache_destroy(t10_alua_lu_gp_mem_cache);
out_free_lu_gp_cache:
	kmem_cache_destroy(t10_alua_lu_gp_cache);
out_free_pr_reg_cache:
	kmem_cache_destroy(t10_pr_reg_cache);
out_free_ua_cache:
	kmem_cache_destroy(se_ua_cache);
out_free_sess_cache:
	kmem_cache_destroy(se_sess_cache);
out:
	return -ENOMEM;
}

void release_se_kmem_caches(void)
{
	destroy_workqueue(target_completion_wq);
	kmem_cache_destroy(se_sess_cache);
	kmem_cache_destroy(se_ua_cache);
	kmem_cache_destroy(t10_pr_reg_cache);
	kmem_cache_destroy(t10_alua_lu_gp_cache);
	kmem_cache_destroy(t10_alua_lu_gp_mem_cache);
	kmem_cache_destroy(t10_alua_tg_pt_gp_cache);
	kmem_cache_destroy(t10_alua_tg_pt_gp_mem_cache);
}

static DEFINE_SPINLOCK(scsi_mib_index_lock);
static u32 scsi_mib_index[SCSI_INDEX_TYPE_MAX];

u32 scsi_get_new_index(scsi_index_t type)
{
	u32 new_index;

	BUG_ON((type < 0) || (type >= SCSI_INDEX_TYPE_MAX));

	spin_lock(&scsi_mib_index_lock);
	new_index = ++scsi_mib_index[type];
	spin_unlock(&scsi_mib_index_lock);

	return new_index;
}

static void transport_init_queue_obj(struct se_queue_obj *qobj)
{
	atomic_set(&qobj->queue_cnt, 0);
	INIT_LIST_HEAD(&qobj->qobj_list);
	init_waitqueue_head(&qobj->thread_wq);
	spin_lock_init(&qobj->cmd_queue_lock);
}

void transport_subsystem_check_init(void)
{
	int ret;

	if (sub_api_initialized)
		return;

	ret = request_module("target_core_iblock");
	if (ret != 0)
		pr_err("Unable to load target_core_iblock\n");

	ret = request_module("target_core_file");
	if (ret != 0)
		pr_err("Unable to load target_core_file\n");

	ret = request_module("target_core_pscsi");
	if (ret != 0)
		pr_err("Unable to load target_core_pscsi\n");

	ret = request_module("target_core_stgt");
	if (ret != 0)
		pr_err("Unable to load target_core_stgt\n");

	sub_api_initialized = 1;
	return;
}

struct se_session *transport_init_session(void)
{
	struct se_session *se_sess;

	se_sess = kmem_cache_zalloc(se_sess_cache, GFP_KERNEL);
	if (!se_sess) {
		pr_err("Unable to allocate struct se_session from"
				" se_sess_cache\n");
		return ERR_PTR(-ENOMEM);
	}
	INIT_LIST_HEAD(&se_sess->sess_list);
	INIT_LIST_HEAD(&se_sess->sess_acl_list);
	INIT_LIST_HEAD(&se_sess->sess_cmd_list);
	INIT_LIST_HEAD(&se_sess->sess_wait_list);
	spin_lock_init(&se_sess->sess_cmd_lock);
	kref_init(&se_sess->sess_kref);

	return se_sess;
}
EXPORT_SYMBOL(transport_init_session);

void __transport_register_session(
	struct se_portal_group *se_tpg,
	struct se_node_acl *se_nacl,
	struct se_session *se_sess,
	void *fabric_sess_ptr)
{
	unsigned char buf[PR_REG_ISID_LEN];

	se_sess->se_tpg = se_tpg;
	se_sess->fabric_sess_ptr = fabric_sess_ptr;
	if (se_nacl) {
		if (se_tpg->se_tpg_tfo->sess_get_initiator_sid != NULL) {
			memset(&buf[0], 0, PR_REG_ISID_LEN);
			se_tpg->se_tpg_tfo->sess_get_initiator_sid(se_sess,
					&buf[0], PR_REG_ISID_LEN);
			se_sess->sess_bin_isid = get_unaligned_be64(&buf[0]);
		}
		kref_get(&se_nacl->acl_kref);

		spin_lock_irq(&se_nacl->nacl_sess_lock);
		se_nacl->nacl_sess = se_sess;

		list_add_tail(&se_sess->sess_acl_list,
			      &se_nacl->acl_sess_list);
		spin_unlock_irq(&se_nacl->nacl_sess_lock);
	}
	list_add_tail(&se_sess->sess_list, &se_tpg->tpg_sess_list);

	pr_debug("TARGET_CORE[%s]: Registered fabric_sess_ptr: %p\n",
		se_tpg->se_tpg_tfo->get_fabric_name(), se_sess->fabric_sess_ptr);
}
EXPORT_SYMBOL(__transport_register_session);

void transport_register_session(
	struct se_portal_group *se_tpg,
	struct se_node_acl *se_nacl,
	struct se_session *se_sess,
	void *fabric_sess_ptr)
{
	unsigned long flags;

	spin_lock_irqsave(&se_tpg->session_lock, flags);
	__transport_register_session(se_tpg, se_nacl, se_sess, fabric_sess_ptr);
	spin_unlock_irqrestore(&se_tpg->session_lock, flags);
}
EXPORT_SYMBOL(transport_register_session);

static void target_release_session(struct kref *kref)
{
	struct se_session *se_sess = container_of(kref,
			struct se_session, sess_kref);
	struct se_portal_group *se_tpg = se_sess->se_tpg;

	se_tpg->se_tpg_tfo->close_session(se_sess);
}

void target_get_session(struct se_session *se_sess)
{
	kref_get(&se_sess->sess_kref);
}
EXPORT_SYMBOL(target_get_session);

int target_put_session(struct se_session *se_sess)
{
	return kref_put(&se_sess->sess_kref, target_release_session);
}
EXPORT_SYMBOL(target_put_session);

static void target_complete_nacl(struct kref *kref)
{
	struct se_node_acl *nacl = container_of(kref,
				struct se_node_acl, acl_kref);

	complete(&nacl->acl_free_comp);
}

void target_put_nacl(struct se_node_acl *nacl)
{
	kref_put(&nacl->acl_kref, target_complete_nacl);
}

void transport_deregister_session_configfs(struct se_session *se_sess)
{
	struct se_node_acl *se_nacl;
	unsigned long flags;
	se_nacl = se_sess->se_node_acl;
	if (se_nacl) {
		spin_lock_irqsave(&se_nacl->nacl_sess_lock, flags);
		if (se_nacl->acl_stop == 0)
			list_del(&se_sess->sess_acl_list);
		if (list_empty(&se_nacl->acl_sess_list))
			se_nacl->nacl_sess = NULL;
		else {
			se_nacl->nacl_sess = container_of(
					se_nacl->acl_sess_list.prev,
					struct se_session, sess_acl_list);
		}
		spin_unlock_irqrestore(&se_nacl->nacl_sess_lock, flags);
	}
}
EXPORT_SYMBOL(transport_deregister_session_configfs);

void transport_free_session(struct se_session *se_sess)
{
	kmem_cache_free(se_sess_cache, se_sess);
}
EXPORT_SYMBOL(transport_free_session);

void transport_deregister_session(struct se_session *se_sess)
{
	struct se_portal_group *se_tpg = se_sess->se_tpg;
	struct target_core_fabric_ops *se_tfo;
	struct se_node_acl *se_nacl;
	unsigned long flags;
	bool comp_nacl = true;

	if (!se_tpg) {
		transport_free_session(se_sess);
		return;
	}
	se_tfo = se_tpg->se_tpg_tfo;

	spin_lock_irqsave(&se_tpg->session_lock, flags);
	list_del(&se_sess->sess_list);
	se_sess->se_tpg = NULL;
	se_sess->fabric_sess_ptr = NULL;
	spin_unlock_irqrestore(&se_tpg->session_lock, flags);

	se_nacl = se_sess->se_node_acl;

	spin_lock_irqsave(&se_tpg->acl_node_lock, flags);
	if (se_nacl && se_nacl->dynamic_node_acl) {
		if (!se_tfo->tpg_check_demo_mode_cache(se_tpg)) {
			list_del(&se_nacl->acl_list);
			se_tpg->num_node_acls--;
			spin_unlock_irqrestore(&se_tpg->acl_node_lock, flags);
			core_tpg_wait_for_nacl_pr_ref(se_nacl);
			core_free_device_list_for_node(se_nacl, se_tpg);
			se_tfo->tpg_release_fabric_acl(se_tpg, se_nacl);

			comp_nacl = false;
			spin_lock_irqsave(&se_tpg->acl_node_lock, flags);
		}
	}
	spin_unlock_irqrestore(&se_tpg->acl_node_lock, flags);

	pr_debug("TARGET_CORE[%s]: Deregistered fabric_sess\n",
		se_tpg->se_tpg_tfo->get_fabric_name());
	if (se_nacl && comp_nacl == true)
		target_put_nacl(se_nacl);

	transport_free_session(se_sess);
}
EXPORT_SYMBOL(transport_deregister_session);

static void transport_all_task_dev_remove_state(struct se_cmd *cmd)
{
	struct se_device *dev = cmd->se_dev;
	struct se_task *task;
	unsigned long flags;

	if (!dev)
		return;

	list_for_each_entry(task, &cmd->t_task_list, t_list) {
		if (task->task_flags & TF_ACTIVE)
			continue;

		spin_lock_irqsave(&dev->execute_task_lock, flags);
		if (task->t_state_active) {
			pr_debug("Removed ITT: 0x%08x dev: %p task[%p]\n",
				cmd->se_tfo->get_task_tag(cmd), dev, task);

			list_del(&task->t_state_list);
			atomic_dec(&cmd->t_task_cdbs_ex_left);
			task->t_state_active = false;
		}
		spin_unlock_irqrestore(&dev->execute_task_lock, flags);
	}

}

static int transport_cmd_check_stop(
	struct se_cmd *cmd,
	int transport_off,
	u8 t_state)
{
	unsigned long flags;

	spin_lock_irqsave(&cmd->t_state_lock, flags);
	if (cmd->transport_state & CMD_T_LUN_STOP) {
		pr_debug("%s:%d CMD_T_LUN_STOP for ITT: 0x%08x\n",
			__func__, __LINE__, cmd->se_tfo->get_task_tag(cmd));

		cmd->transport_state &= ~CMD_T_ACTIVE;
		if (transport_off == 2)
			transport_all_task_dev_remove_state(cmd);
		spin_unlock_irqrestore(&cmd->t_state_lock, flags);

		complete(&cmd->transport_lun_stop_comp);
		return 1;
	}
	if (cmd->transport_state & CMD_T_STOP) {
		pr_debug("%s:%d CMD_T_STOP for ITT: 0x%08x\n",
			__func__, __LINE__,
			cmd->se_tfo->get_task_tag(cmd));

		if (transport_off == 2)
			transport_all_task_dev_remove_state(cmd);

		if (transport_off == 2)
			cmd->se_lun = NULL;
		spin_unlock_irqrestore(&cmd->t_state_lock, flags);

		complete(&cmd->t_transport_stop_comp);
		return 1;
	}
	if (transport_off) {
		cmd->transport_state &= ~CMD_T_ACTIVE;
		if (transport_off == 2) {
			transport_all_task_dev_remove_state(cmd);
			cmd->se_lun = NULL;
			if (cmd->se_tfo->check_stop_free != NULL) {
				spin_unlock_irqrestore(
					&cmd->t_state_lock, flags);

				return cmd->se_tfo->check_stop_free(cmd);
			}
		}
		spin_unlock_irqrestore(&cmd->t_state_lock, flags);

		return 0;
	} else if (t_state)
		cmd->t_state = t_state;
	spin_unlock_irqrestore(&cmd->t_state_lock, flags);

	return 0;
}

static int transport_cmd_check_stop_to_fabric(struct se_cmd *cmd)
{
	return transport_cmd_check_stop(cmd, 2, 0);
}

static void transport_lun_remove_cmd(struct se_cmd *cmd)
{
	struct se_lun *lun = cmd->se_lun;
	unsigned long flags;

	if (!lun)
		return;

	spin_lock_irqsave(&cmd->t_state_lock, flags);
	if (cmd->transport_state & CMD_T_DEV_ACTIVE) {
		cmd->transport_state &= ~CMD_T_DEV_ACTIVE;
		transport_all_task_dev_remove_state(cmd);
	}
	spin_unlock_irqrestore(&cmd->t_state_lock, flags);

	spin_lock_irqsave(&lun->lun_cmd_lock, flags);
	if (!list_empty(&cmd->se_lun_node))
		list_del_init(&cmd->se_lun_node);
	spin_unlock_irqrestore(&lun->lun_cmd_lock, flags);
}

void transport_cmd_finish_abort(struct se_cmd *cmd, int remove)
{
	if (!(cmd->se_cmd_flags & SCF_SCSI_TMR_CDB))
		transport_lun_remove_cmd(cmd);

	if (transport_cmd_check_stop_to_fabric(cmd))
		return;
	if (remove) {
		transport_remove_cmd_from_queue(cmd);
		transport_put_cmd(cmd);
	}
}

static void transport_add_cmd_to_queue(struct se_cmd *cmd, int t_state,
		bool at_head)
{
	struct se_device *dev = cmd->se_dev;
	struct se_queue_obj *qobj = &dev->dev_queue_obj;
	unsigned long flags;

	if (t_state) {
		spin_lock_irqsave(&cmd->t_state_lock, flags);
		cmd->t_state = t_state;
		cmd->transport_state |= CMD_T_ACTIVE;
		spin_unlock_irqrestore(&cmd->t_state_lock, flags);
	}

	spin_lock_irqsave(&qobj->cmd_queue_lock, flags);

	
	if (!list_empty(&cmd->se_queue_node))
		list_del(&cmd->se_queue_node);
	else
		atomic_inc(&qobj->queue_cnt);

	if (at_head)
		list_add(&cmd->se_queue_node, &qobj->qobj_list);
	else
		list_add_tail(&cmd->se_queue_node, &qobj->qobj_list);
	cmd->transport_state |= CMD_T_QUEUED;
	spin_unlock_irqrestore(&qobj->cmd_queue_lock, flags);

	wake_up_interruptible(&qobj->thread_wq);
}

static struct se_cmd *
transport_get_cmd_from_queue(struct se_queue_obj *qobj)
{
	struct se_cmd *cmd;
	unsigned long flags;

	spin_lock_irqsave(&qobj->cmd_queue_lock, flags);
	if (list_empty(&qobj->qobj_list)) {
		spin_unlock_irqrestore(&qobj->cmd_queue_lock, flags);
		return NULL;
	}
	cmd = list_first_entry(&qobj->qobj_list, struct se_cmd, se_queue_node);

	cmd->transport_state &= ~CMD_T_QUEUED;
	list_del_init(&cmd->se_queue_node);
	atomic_dec(&qobj->queue_cnt);
	spin_unlock_irqrestore(&qobj->cmd_queue_lock, flags);

	return cmd;
}

static void transport_remove_cmd_from_queue(struct se_cmd *cmd)
{
	struct se_queue_obj *qobj = &cmd->se_dev->dev_queue_obj;
	unsigned long flags;

	spin_lock_irqsave(&qobj->cmd_queue_lock, flags);
	if (!(cmd->transport_state & CMD_T_QUEUED)) {
		spin_unlock_irqrestore(&qobj->cmd_queue_lock, flags);
		return;
	}
	cmd->transport_state &= ~CMD_T_QUEUED;
	atomic_dec(&qobj->queue_cnt);
	list_del_init(&cmd->se_queue_node);
	spin_unlock_irqrestore(&qobj->cmd_queue_lock, flags);
}

void transport_complete_sync_cache(struct se_cmd *cmd, int good)
{
	struct se_task *task = list_entry(cmd->t_task_list.next,
				struct se_task, t_list);

	if (good) {
		cmd->scsi_status = SAM_STAT_GOOD;
		task->task_scsi_status = GOOD;
	} else {
		task->task_scsi_status = SAM_STAT_CHECK_CONDITION;
		task->task_se_cmd->scsi_sense_reason =
				TCM_LOGICAL_UNIT_COMMUNICATION_FAILURE;

	}

	transport_complete_task(task, good);
}
EXPORT_SYMBOL(transport_complete_sync_cache);

static void target_complete_failure_work(struct work_struct *work)
{
	struct se_cmd *cmd = container_of(work, struct se_cmd, work);

	transport_generic_request_failure(cmd);
}

void transport_complete_task(struct se_task *task, int success)
{
	struct se_cmd *cmd = task->task_se_cmd;
	struct se_device *dev = cmd->se_dev;
	unsigned long flags;

	spin_lock_irqsave(&cmd->t_state_lock, flags);
	task->task_flags &= ~TF_ACTIVE;

	if (dev && dev->transport->transport_complete) {
		if (dev->transport->transport_complete(task) != 0) {
			cmd->se_cmd_flags |= SCF_TRANSPORT_TASK_SENSE;
			task->task_flags |= TF_HAS_SENSE;
			success = 1;
		}
	}

	if (task->task_flags & TF_REQUEST_STOP) {
		spin_unlock_irqrestore(&cmd->t_state_lock, flags);
		complete(&task->task_stop_comp);
		return;
	}

	if (!success)
		cmd->transport_state |= CMD_T_FAILED;

	if (!atomic_dec_and_test(&cmd->t_task_cdbs_left)) {
		spin_unlock_irqrestore(&cmd->t_state_lock, flags);
		return;
	}
	if (cmd->transport_state & CMD_T_ABORTED &&
	    cmd->transport_state & CMD_T_STOP) {
		spin_unlock_irqrestore(&cmd->t_state_lock, flags);
		complete(&cmd->t_transport_stop_comp);
		return;
	} else if (cmd->transport_state & CMD_T_FAILED) {
		cmd->scsi_sense_reason = TCM_LOGICAL_UNIT_COMMUNICATION_FAILURE;
		INIT_WORK(&cmd->work, target_complete_failure_work);
	} else {
		INIT_WORK(&cmd->work, target_complete_ok_work);
	}

	cmd->t_state = TRANSPORT_COMPLETE;
	cmd->transport_state |= (CMD_T_COMPLETE | CMD_T_ACTIVE);
	spin_unlock_irqrestore(&cmd->t_state_lock, flags);

	queue_work(target_completion_wq, &cmd->work);
}
EXPORT_SYMBOL(transport_complete_task);

static inline int transport_add_task_check_sam_attr(
	struct se_task *task,
	struct se_task *task_prev,
	struct se_device *dev)
{
	if (dev->dev_task_attr_type != SAM_TASK_ATTR_EMULATED) {
		list_add_tail(&task->t_execute_list, &dev->execute_task_list);
		return 0;
	}
	if (task->task_se_cmd->sam_task_attr == MSG_HEAD_TAG) {
		list_add(&task->t_execute_list,
				(task_prev != NULL) ?
				&task_prev->t_execute_list :
				&dev->execute_task_list);

		pr_debug("Set HEAD_OF_QUEUE for task CDB: 0x%02x"
				" in execution queue\n",
				task->task_se_cmd->t_task_cdb[0]);
		return 1;
	}
	list_add_tail(&task->t_execute_list, &dev->execute_task_list);
	return 0;
}

static void __transport_add_task_to_execute_queue(
	struct se_task *task,
	struct se_task *task_prev,
	struct se_device *dev)
{
	int head_of_queue;

	head_of_queue = transport_add_task_check_sam_attr(task, task_prev, dev);
	atomic_inc(&dev->execute_tasks);

	if (task->t_state_active)
		return;
	if (head_of_queue)
		list_add(&task->t_state_list, (task_prev) ?
				&task_prev->t_state_list :
				&dev->state_task_list);
	else
		list_add_tail(&task->t_state_list, &dev->state_task_list);

	task->t_state_active = true;

	pr_debug("Added ITT: 0x%08x task[%p] to dev: %p\n",
		task->task_se_cmd->se_tfo->get_task_tag(task->task_se_cmd),
		task, dev);
}

static void transport_add_tasks_to_state_queue(struct se_cmd *cmd)
{
	struct se_device *dev = cmd->se_dev;
	struct se_task *task;
	unsigned long flags;

	spin_lock_irqsave(&cmd->t_state_lock, flags);
	list_for_each_entry(task, &cmd->t_task_list, t_list) {
		spin_lock(&dev->execute_task_lock);
		if (!task->t_state_active) {
			list_add_tail(&task->t_state_list,
				      &dev->state_task_list);
			task->t_state_active = true;

			pr_debug("Added ITT: 0x%08x task[%p] to dev: %p\n",
				task->task_se_cmd->se_tfo->get_task_tag(
				task->task_se_cmd), task, dev);
		}
		spin_unlock(&dev->execute_task_lock);
	}
	spin_unlock_irqrestore(&cmd->t_state_lock, flags);
}

static void __transport_add_tasks_from_cmd(struct se_cmd *cmd)
{
	struct se_device *dev = cmd->se_dev;
	struct se_task *task, *task_prev = NULL;

	list_for_each_entry(task, &cmd->t_task_list, t_list) {
		if (!list_empty(&task->t_execute_list))
			continue;
		__transport_add_task_to_execute_queue(task, task_prev, dev);
		task_prev = task;
	}
}

static void transport_add_tasks_from_cmd(struct se_cmd *cmd)
{
	unsigned long flags;
	struct se_device *dev = cmd->se_dev;

	spin_lock_irqsave(&dev->execute_task_lock, flags);
	__transport_add_tasks_from_cmd(cmd);
	spin_unlock_irqrestore(&dev->execute_task_lock, flags);
}

void __transport_remove_task_from_execute_queue(struct se_task *task,
		struct se_device *dev)
{
	list_del_init(&task->t_execute_list);
	atomic_dec(&dev->execute_tasks);
}

static void transport_remove_task_from_execute_queue(
	struct se_task *task,
	struct se_device *dev)
{
	unsigned long flags;

	if (WARN_ON(list_empty(&task->t_execute_list)))
		return;

	spin_lock_irqsave(&dev->execute_task_lock, flags);
	__transport_remove_task_from_execute_queue(task, dev);
	spin_unlock_irqrestore(&dev->execute_task_lock, flags);
}


static void target_qf_do_work(struct work_struct *work)
{
	struct se_device *dev = container_of(work, struct se_device,
					qf_work_queue);
	LIST_HEAD(qf_cmd_list);
	struct se_cmd *cmd, *cmd_tmp;

	spin_lock_irq(&dev->qf_cmd_lock);
	list_splice_init(&dev->qf_cmd_list, &qf_cmd_list);
	spin_unlock_irq(&dev->qf_cmd_lock);

	list_for_each_entry_safe(cmd, cmd_tmp, &qf_cmd_list, se_qf_node) {
		list_del(&cmd->se_qf_node);
		atomic_dec(&dev->dev_qf_count);
		smp_mb__after_atomic_dec();

		pr_debug("Processing %s cmd: %p QUEUE_FULL in work queue"
			" context: %s\n", cmd->se_tfo->get_fabric_name(), cmd,
			(cmd->t_state == TRANSPORT_COMPLETE_QF_OK) ? "COMPLETE_OK" :
			(cmd->t_state == TRANSPORT_COMPLETE_QF_WP) ? "WRITE_PENDING"
			: "UNKNOWN");

		transport_add_cmd_to_queue(cmd, cmd->t_state, true);
	}
}

unsigned char *transport_dump_cmd_direction(struct se_cmd *cmd)
{
	switch (cmd->data_direction) {
	case DMA_NONE:
		return "NONE";
	case DMA_FROM_DEVICE:
		return "READ";
	case DMA_TO_DEVICE:
		return "WRITE";
	case DMA_BIDIRECTIONAL:
		return "BIDI";
	default:
		break;
	}

	return "UNKNOWN";
}

void transport_dump_dev_state(
	struct se_device *dev,
	char *b,
	int *bl)
{
	*bl += sprintf(b + *bl, "Status: ");
	switch (dev->dev_status) {
	case TRANSPORT_DEVICE_ACTIVATED:
		*bl += sprintf(b + *bl, "ACTIVATED");
		break;
	case TRANSPORT_DEVICE_DEACTIVATED:
		*bl += sprintf(b + *bl, "DEACTIVATED");
		break;
	case TRANSPORT_DEVICE_SHUTDOWN:
		*bl += sprintf(b + *bl, "SHUTDOWN");
		break;
	case TRANSPORT_DEVICE_OFFLINE_ACTIVATED:
	case TRANSPORT_DEVICE_OFFLINE_DEACTIVATED:
		*bl += sprintf(b + *bl, "OFFLINE");
		break;
	default:
		*bl += sprintf(b + *bl, "UNKNOWN=%d", dev->dev_status);
		break;
	}

	*bl += sprintf(b + *bl, "  Execute/Max Queue Depth: %d/%d",
		atomic_read(&dev->execute_tasks), dev->queue_depth);
	*bl += sprintf(b + *bl, "  SectorSize: %u  MaxSectors: %u\n",
		dev->se_sub_dev->se_dev_attrib.block_size, dev->se_sub_dev->se_dev_attrib.max_sectors);
	*bl += sprintf(b + *bl, "        ");
}

void transport_dump_vpd_proto_id(
	struct t10_vpd *vpd,
	unsigned char *p_buf,
	int p_buf_len)
{
	unsigned char buf[VPD_TMP_BUF_SIZE];
	int len;

	memset(buf, 0, VPD_TMP_BUF_SIZE);
	len = sprintf(buf, "T10 VPD Protocol Identifier: ");

	switch (vpd->protocol_identifier) {
	case 0x00:
		sprintf(buf+len, "Fibre Channel\n");
		break;
	case 0x10:
		sprintf(buf+len, "Parallel SCSI\n");
		break;
	case 0x20:
		sprintf(buf+len, "SSA\n");
		break;
	case 0x30:
		sprintf(buf+len, "IEEE 1394\n");
		break;
	case 0x40:
		sprintf(buf+len, "SCSI Remote Direct Memory Access"
				" Protocol\n");
		break;
	case 0x50:
		sprintf(buf+len, "Internet SCSI (iSCSI)\n");
		break;
	case 0x60:
		sprintf(buf+len, "SAS Serial SCSI Protocol\n");
		break;
	case 0x70:
		sprintf(buf+len, "Automation/Drive Interface Transport"
				" Protocol\n");
		break;
	case 0x80:
		sprintf(buf+len, "AT Attachment Interface ATA/ATAPI\n");
		break;
	default:
		sprintf(buf+len, "Unknown 0x%02x\n",
				vpd->protocol_identifier);
		break;
	}

	if (p_buf)
		strncpy(p_buf, buf, p_buf_len);
	else
		pr_debug("%s", buf);
}

void
transport_set_vpd_proto_id(struct t10_vpd *vpd, unsigned char *page_83)
{
	 if (page_83[1] & 0x80) {
		vpd->protocol_identifier = (page_83[0] & 0xf0);
		vpd->protocol_identifier_set = 1;
		transport_dump_vpd_proto_id(vpd, NULL, 0);
	}
}
EXPORT_SYMBOL(transport_set_vpd_proto_id);

int transport_dump_vpd_assoc(
	struct t10_vpd *vpd,
	unsigned char *p_buf,
	int p_buf_len)
{
	unsigned char buf[VPD_TMP_BUF_SIZE];
	int ret = 0;
	int len;

	memset(buf, 0, VPD_TMP_BUF_SIZE);
	len = sprintf(buf, "T10 VPD Identifier Association: ");

	switch (vpd->association) {
	case 0x00:
		sprintf(buf+len, "addressed logical unit\n");
		break;
	case 0x10:
		sprintf(buf+len, "target port\n");
		break;
	case 0x20:
		sprintf(buf+len, "SCSI target device\n");
		break;
	default:
		sprintf(buf+len, "Unknown 0x%02x\n", vpd->association);
		ret = -EINVAL;
		break;
	}

	if (p_buf)
		strncpy(p_buf, buf, p_buf_len);
	else
		pr_debug("%s", buf);

	return ret;
}

int transport_set_vpd_assoc(struct t10_vpd *vpd, unsigned char *page_83)
{
	vpd->association = (page_83[1] & 0x30);
	return transport_dump_vpd_assoc(vpd, NULL, 0);
}
EXPORT_SYMBOL(transport_set_vpd_assoc);

int transport_dump_vpd_ident_type(
	struct t10_vpd *vpd,
	unsigned char *p_buf,
	int p_buf_len)
{
	unsigned char buf[VPD_TMP_BUF_SIZE];
	int ret = 0;
	int len;

	memset(buf, 0, VPD_TMP_BUF_SIZE);
	len = sprintf(buf, "T10 VPD Identifier Type: ");

	switch (vpd->device_identifier_type) {
	case 0x00:
		sprintf(buf+len, "Vendor specific\n");
		break;
	case 0x01:
		sprintf(buf+len, "T10 Vendor ID based\n");
		break;
	case 0x02:
		sprintf(buf+len, "EUI-64 based\n");
		break;
	case 0x03:
		sprintf(buf+len, "NAA\n");
		break;
	case 0x04:
		sprintf(buf+len, "Relative target port identifier\n");
		break;
	case 0x08:
		sprintf(buf+len, "SCSI name string\n");
		break;
	default:
		sprintf(buf+len, "Unsupported: 0x%02x\n",
				vpd->device_identifier_type);
		ret = -EINVAL;
		break;
	}

	if (p_buf) {
		if (p_buf_len < strlen(buf)+1)
			return -EINVAL;
		strncpy(p_buf, buf, p_buf_len);
	} else {
		pr_debug("%s", buf);
	}

	return ret;
}

int transport_set_vpd_ident_type(struct t10_vpd *vpd, unsigned char *page_83)
{
	vpd->device_identifier_type = (page_83[1] & 0x0f);
	return transport_dump_vpd_ident_type(vpd, NULL, 0);
}
EXPORT_SYMBOL(transport_set_vpd_ident_type);

int transport_dump_vpd_ident(
	struct t10_vpd *vpd,
	unsigned char *p_buf,
	int p_buf_len)
{
	unsigned char buf[VPD_TMP_BUF_SIZE];
	int ret = 0;

	memset(buf, 0, VPD_TMP_BUF_SIZE);

	switch (vpd->device_identifier_code_set) {
	case 0x01: 
		sprintf(buf, "T10 VPD Binary Device Identifier: %s\n",
			&vpd->device_identifier[0]);
		break;
	case 0x02: 
		sprintf(buf, "T10 VPD ASCII Device Identifier: %s\n",
			&vpd->device_identifier[0]);
		break;
	case 0x03: 
		sprintf(buf, "T10 VPD UTF-8 Device Identifier: %s\n",
			&vpd->device_identifier[0]);
		break;
	default:
		sprintf(buf, "T10 VPD Device Identifier encoding unsupported:"
			" 0x%02x", vpd->device_identifier_code_set);
		ret = -EINVAL;
		break;
	}

	if (p_buf)
		strncpy(p_buf, buf, p_buf_len);
	else
		pr_debug("%s", buf);

	return ret;
}

int
transport_set_vpd_ident(struct t10_vpd *vpd, unsigned char *page_83)
{
	static const char hex_str[] = "0123456789abcdef";
	int j = 0, i = 4; 

	vpd->device_identifier_code_set = (page_83[0] & 0x0f);
	switch (vpd->device_identifier_code_set) {
	case 0x01: 
		vpd->device_identifier[j++] =
				hex_str[vpd->device_identifier_type];
		while (i < (4 + page_83[3])) {
			vpd->device_identifier[j++] =
				hex_str[(page_83[i] & 0xf0) >> 4];
			vpd->device_identifier[j++] =
				hex_str[page_83[i] & 0x0f];
			i++;
		}
		break;
	case 0x02: 
	case 0x03: 
		while (i < (4 + page_83[3]))
			vpd->device_identifier[j++] = page_83[i++];
		break;
	default:
		break;
	}

	return transport_dump_vpd_ident(vpd, NULL, 0);
}
EXPORT_SYMBOL(transport_set_vpd_ident);

static void core_setup_task_attr_emulation(struct se_device *dev)
{
	if (dev->transport->transport_type == TRANSPORT_PLUGIN_PHBA_PDEV) {
		dev->dev_task_attr_type = SAM_TASK_ATTR_PASSTHROUGH;
		return;
	}

	dev->dev_task_attr_type = SAM_TASK_ATTR_EMULATED;
	pr_debug("%s: Using SAM_TASK_ATTR_EMULATED for SPC: 0x%02x"
		" device\n", dev->transport->name,
		dev->transport->get_device_rev(dev));
}

static void scsi_dump_inquiry(struct se_device *dev)
{
	struct t10_wwn *wwn = &dev->se_sub_dev->t10_wwn;
	char buf[17];
	int i, device_type;
	for (i = 0; i < 8; i++)
		if (wwn->vendor[i] >= 0x20)
			buf[i] = wwn->vendor[i];
		else
			buf[i] = ' ';
	buf[i] = '\0';
	pr_debug("  Vendor: %s\n", buf);

	for (i = 0; i < 16; i++)
		if (wwn->model[i] >= 0x20)
			buf[i] = wwn->model[i];
		else
			buf[i] = ' ';
	buf[i] = '\0';
	pr_debug("  Model: %s\n", buf);

	for (i = 0; i < 4; i++)
		if (wwn->revision[i] >= 0x20)
			buf[i] = wwn->revision[i];
		else
			buf[i] = ' ';
	buf[i] = '\0';
	pr_debug("  Revision: %s\n", buf);

	device_type = dev->transport->get_device_type(dev);
	pr_debug("  Type:   %s ", scsi_device_type(device_type));
	pr_debug("                 ANSI SCSI revision: %02x\n",
				dev->transport->get_device_rev(dev));
}

struct se_device *transport_add_device_to_core_hba(
	struct se_hba *hba,
	struct se_subsystem_api *transport,
	struct se_subsystem_dev *se_dev,
	u32 device_flags,
	void *transport_dev,
	struct se_dev_limits *dev_limits,
	const char *inquiry_prod,
	const char *inquiry_rev)
{
	int force_pt;
	struct se_device  *dev;

	dev = kzalloc(sizeof(struct se_device), GFP_KERNEL);
	if (!dev) {
		pr_err("Unable to allocate memory for se_dev_t\n");
		return NULL;
	}

	transport_init_queue_obj(&dev->dev_queue_obj);
	dev->dev_flags		= device_flags;
	dev->dev_status		|= TRANSPORT_DEVICE_DEACTIVATED;
	dev->dev_ptr		= transport_dev;
	dev->se_hba		= hba;
	dev->se_sub_dev		= se_dev;
	dev->transport		= transport;
	INIT_LIST_HEAD(&dev->dev_list);
	INIT_LIST_HEAD(&dev->dev_sep_list);
	INIT_LIST_HEAD(&dev->dev_tmr_list);
	INIT_LIST_HEAD(&dev->execute_task_list);
	INIT_LIST_HEAD(&dev->delayed_cmd_list);
	INIT_LIST_HEAD(&dev->state_task_list);
	INIT_LIST_HEAD(&dev->qf_cmd_list);
	spin_lock_init(&dev->execute_task_lock);
	spin_lock_init(&dev->delayed_cmd_lock);
	spin_lock_init(&dev->dev_reservation_lock);
	spin_lock_init(&dev->dev_status_lock);
	spin_lock_init(&dev->se_port_lock);
	spin_lock_init(&dev->se_tmr_lock);
	spin_lock_init(&dev->qf_cmd_lock);
	atomic_set(&dev->dev_ordered_id, 0);

	se_dev_set_default_attribs(dev, dev_limits);

	dev->dev_index = scsi_get_new_index(SCSI_DEVICE_INDEX);
	dev->creation_time = get_jiffies_64();
	spin_lock_init(&dev->stats_lock);

	spin_lock(&hba->device_lock);
	list_add_tail(&dev->dev_list, &hba->hba_dev_list);
	hba->dev_count++;
	spin_unlock(&hba->device_lock);
	core_setup_task_attr_emulation(dev);
	force_pt = (hba->hba_flags & HBA_FLAGS_INTERNAL_USE);
	core_setup_reservations(dev, force_pt);
	if (core_setup_alua(dev, force_pt) < 0)
		goto out;

	dev->process_thread = kthread_run(transport_processing_thread, dev,
					  "LIO_%s", dev->transport->name);
	if (IS_ERR(dev->process_thread)) {
		pr_err("Unable to create kthread: LIO_%s\n",
			dev->transport->name);
		goto out;
	}
	INIT_WORK(&dev->qf_work_queue, target_qf_do_work);
	if (dev->transport->transport_type != TRANSPORT_PLUGIN_PHBA_PDEV) {
		if (!inquiry_prod || !inquiry_rev) {
			pr_err("All non TCM/pSCSI plugins require"
				" INQUIRY consts\n");
			goto out;
		}

		strncpy(&dev->se_sub_dev->t10_wwn.vendor[0], "LIO-ORG", 8);
		strncpy(&dev->se_sub_dev->t10_wwn.model[0], inquiry_prod, 16);
		strncpy(&dev->se_sub_dev->t10_wwn.revision[0], inquiry_rev, 4);
	}
	scsi_dump_inquiry(dev);

	return dev;
out:
	kthread_stop(dev->process_thread);

	spin_lock(&hba->device_lock);
	list_del(&dev->dev_list);
	hba->dev_count--;
	spin_unlock(&hba->device_lock);

	se_release_vpd_for_dev(dev);

	kfree(dev);

	return NULL;
}
EXPORT_SYMBOL(transport_add_device_to_core_hba);

static inline void transport_generic_prepare_cdb(
	unsigned char *cdb)
{
	switch (cdb[0]) {
	case READ_10: 
	case READ_12: 
	case READ_16: 
	case SEND_DIAGNOSTIC: 
	case VERIFY: 
	case VERIFY_16: 
	case WRITE_VERIFY: 
	case WRITE_VERIFY_12: 
		break;
	default:
		cdb[1] &= 0x1f; 
		break;
	}
}

static struct se_task *
transport_generic_get_task(struct se_cmd *cmd,
		enum dma_data_direction data_direction)
{
	struct se_task *task;
	struct se_device *dev = cmd->se_dev;

	task = dev->transport->alloc_task(cmd->t_task_cdb);
	if (!task) {
		pr_err("Unable to allocate struct se_task\n");
		return NULL;
	}

	INIT_LIST_HEAD(&task->t_list);
	INIT_LIST_HEAD(&task->t_execute_list);
	INIT_LIST_HEAD(&task->t_state_list);
	init_completion(&task->task_stop_comp);
	task->task_se_cmd = cmd;
	task->task_data_direction = data_direction;

	return task;
}

static int transport_generic_cmd_sequencer(struct se_cmd *, unsigned char *);

void transport_init_se_cmd(
	struct se_cmd *cmd,
	struct target_core_fabric_ops *tfo,
	struct se_session *se_sess,
	u32 data_length,
	int data_direction,
	int task_attr,
	unsigned char *sense_buffer)
{
	INIT_LIST_HEAD(&cmd->se_lun_node);
	INIT_LIST_HEAD(&cmd->se_delayed_node);
	INIT_LIST_HEAD(&cmd->se_qf_node);
	INIT_LIST_HEAD(&cmd->se_queue_node);
	INIT_LIST_HEAD(&cmd->se_cmd_list);
	INIT_LIST_HEAD(&cmd->t_task_list);
	init_completion(&cmd->transport_lun_fe_stop_comp);
	init_completion(&cmd->transport_lun_stop_comp);
	init_completion(&cmd->t_transport_stop_comp);
	init_completion(&cmd->cmd_wait_comp);
	spin_lock_init(&cmd->t_state_lock);
	cmd->transport_state = CMD_T_DEV_ACTIVE;

	cmd->se_tfo = tfo;
	cmd->se_sess = se_sess;
	cmd->data_length = data_length;
	cmd->data_direction = data_direction;
	cmd->sam_task_attr = task_attr;
	cmd->sense_buffer = sense_buffer;
}
EXPORT_SYMBOL(transport_init_se_cmd);

static int transport_check_alloc_task_attr(struct se_cmd *cmd)
{
	if (cmd->se_dev->dev_task_attr_type != SAM_TASK_ATTR_EMULATED)
		return 0;

	if (cmd->sam_task_attr == MSG_ACA_TAG) {
		pr_debug("SAM Task Attribute ACA"
			" emulation is not supported\n");
		return -EINVAL;
	}
	cmd->se_ordered_id = atomic_inc_return(&cmd->se_dev->dev_ordered_id);
	smp_mb__after_atomic_inc();
	pr_debug("Allocated se_ordered_id: %u for Task Attr: 0x%02x on %s\n",
			cmd->se_ordered_id, cmd->sam_task_attr,
			cmd->se_dev->transport->name);
	return 0;
}

int target_setup_cmd_from_cdb(
	struct se_cmd *cmd,
	unsigned char *cdb)
{
	int ret;

	transport_generic_prepare_cdb(cdb);
	if (scsi_command_size(cdb) > SCSI_MAX_VARLEN_CDB_SIZE) {
		pr_err("Received SCSI CDB with command_size: %d that"
			" exceeds SCSI_MAX_VARLEN_CDB_SIZE: %d\n",
			scsi_command_size(cdb), SCSI_MAX_VARLEN_CDB_SIZE);
		cmd->se_cmd_flags |= SCF_SCSI_CDB_EXCEPTION;
		cmd->scsi_sense_reason = TCM_INVALID_CDB_FIELD;
		return -EINVAL;
	}
	if (scsi_command_size(cdb) > sizeof(cmd->__t_task_cdb)) {
		cmd->t_task_cdb = kzalloc(scsi_command_size(cdb),
						GFP_KERNEL);
		if (!cmd->t_task_cdb) {
			pr_err("Unable to allocate cmd->t_task_cdb"
				" %u > sizeof(cmd->__t_task_cdb): %lu ops\n",
				scsi_command_size(cdb),
				(unsigned long)sizeof(cmd->__t_task_cdb));
			cmd->se_cmd_flags |= SCF_SCSI_CDB_EXCEPTION;
			cmd->scsi_sense_reason =
					TCM_LOGICAL_UNIT_COMMUNICATION_FAILURE;
			return -ENOMEM;
		}
	} else
		cmd->t_task_cdb = &cmd->__t_task_cdb[0];
	memcpy(cmd->t_task_cdb, cdb, scsi_command_size(cdb));
	ret = transport_generic_cmd_sequencer(cmd, cdb);
	if (ret < 0)
		return ret;
	if (transport_check_alloc_task_attr(cmd) < 0) {
		cmd->se_cmd_flags |= SCF_SCSI_CDB_EXCEPTION;
		cmd->scsi_sense_reason = TCM_INVALID_CDB_FIELD;
		return -EINVAL;
	}
	spin_lock(&cmd->se_lun->lun_sep_lock);
	if (cmd->se_lun->lun_sep)
		cmd->se_lun->lun_sep->sep_stats.cmd_pdus++;
	spin_unlock(&cmd->se_lun->lun_sep_lock);
	return 0;
}
EXPORT_SYMBOL(target_setup_cmd_from_cdb);

int transport_handle_cdb_direct(
	struct se_cmd *cmd)
{
	int ret;

	if (!cmd->se_lun) {
		dump_stack();
		pr_err("cmd->se_lun is NULL\n");
		return -EINVAL;
	}
	if (in_interrupt()) {
		dump_stack();
		pr_err("transport_generic_handle_cdb cannot be called"
				" from interrupt context\n");
		return -EINVAL;
	}
	cmd->t_state = TRANSPORT_NEW_CMD;
	cmd->transport_state |= CMD_T_ACTIVE;

	ret = transport_generic_new_cmd(cmd);
	if (ret < 0)
		transport_generic_request_failure(cmd);

	return 0;
}
EXPORT_SYMBOL(transport_handle_cdb_direct);

void target_submit_cmd(struct se_cmd *se_cmd, struct se_session *se_sess,
		unsigned char *cdb, unsigned char *sense, u32 unpacked_lun,
		u32 data_length, int task_attr, int data_dir, int flags)
{
	struct se_portal_group *se_tpg;
	int rc;

	se_tpg = se_sess->se_tpg;
	BUG_ON(!se_tpg);
	BUG_ON(se_cmd->se_tfo || se_cmd->se_sess);
	BUG_ON(in_interrupt());
	transport_init_se_cmd(se_cmd, se_tpg->se_tpg_tfo, se_sess,
				data_length, data_dir, task_attr, sense);
	if (flags & TARGET_SCF_UNKNOWN_SIZE)
		se_cmd->unknown_data_length = 1;
	target_get_sess_cmd(se_sess, se_cmd, (flags & TARGET_SCF_ACK_KREF));
	if (flags & TARGET_SCF_BIDI_OP)
		se_cmd->se_cmd_flags |= SCF_BIDI;
	if (transport_lookup_cmd_lun(se_cmd, unpacked_lun) < 0) {
		transport_send_check_condition_and_sense(se_cmd,
				se_cmd->scsi_sense_reason, 0);
		target_put_sess_cmd(se_sess, se_cmd);
		return;
	}
	rc = target_setup_cmd_from_cdb(se_cmd, cdb);
	if (rc != 0) {
		transport_generic_request_failure(se_cmd);
		return;
	}
	transport_handle_cdb_direct(se_cmd);
	return;
}
EXPORT_SYMBOL(target_submit_cmd);

static void target_complete_tmr_failure(struct work_struct *work)
{
	struct se_cmd *se_cmd = container_of(work, struct se_cmd, work);

	se_cmd->se_tmr_req->response = TMR_LUN_DOES_NOT_EXIST;
	se_cmd->se_tfo->queue_tm_rsp(se_cmd);
	transport_generic_free_cmd(se_cmd, 0);
}


int target_submit_tmr(struct se_cmd *se_cmd, struct se_session *se_sess,
		unsigned char *sense, u32 unpacked_lun,
		void *fabric_tmr_ptr, unsigned char tm_type,
		gfp_t gfp, unsigned int tag, int flags)
{
	struct se_portal_group *se_tpg;
	int ret;

	se_tpg = se_sess->se_tpg;
	BUG_ON(!se_tpg);

	transport_init_se_cmd(se_cmd, se_tpg->se_tpg_tfo, se_sess,
			      0, DMA_NONE, MSG_SIMPLE_TAG, sense);
	ret = core_tmr_alloc_req(se_cmd, fabric_tmr_ptr, tm_type, gfp);
	if (ret < 0)
		return -ENOMEM;

	if (tm_type == TMR_ABORT_TASK)
		se_cmd->se_tmr_req->ref_task_tag = tag;

	
	target_get_sess_cmd(se_sess, se_cmd, (flags & TARGET_SCF_ACK_KREF));

	ret = transport_lookup_tmr_lun(se_cmd, unpacked_lun);
	if (ret) {
		INIT_WORK(&se_cmd->work, target_complete_tmr_failure);
		schedule_work(&se_cmd->work);
		return 0;
	}
	transport_generic_handle_tmr(se_cmd);
	return 0;
}
EXPORT_SYMBOL(target_submit_tmr);

int transport_generic_handle_cdb_map(
	struct se_cmd *cmd)
{
	if (!cmd->se_lun) {
		dump_stack();
		pr_err("cmd->se_lun is NULL\n");
		return -EINVAL;
	}

	transport_add_cmd_to_queue(cmd, TRANSPORT_NEW_CMD_MAP, false);
	return 0;
}
EXPORT_SYMBOL(transport_generic_handle_cdb_map);

int transport_generic_handle_data(
	struct se_cmd *cmd)
{
	if (!in_interrupt() && signal_pending(current))
		return -EPERM;
	if (transport_check_aborted_status(cmd, 1) != 0)
		return 0;

	transport_add_cmd_to_queue(cmd, TRANSPORT_PROCESS_WRITE, false);
	return 0;
}
EXPORT_SYMBOL(transport_generic_handle_data);

int transport_generic_handle_tmr(
	struct se_cmd *cmd)
{
	transport_add_cmd_to_queue(cmd, TRANSPORT_PROCESS_TMR, false);
	return 0;
}
EXPORT_SYMBOL(transport_generic_handle_tmr);

bool target_stop_task(struct se_task *task, unsigned long *flags)
{
	struct se_cmd *cmd = task->task_se_cmd;
	bool was_active = false;

	if (task->task_flags & TF_ACTIVE) {
		task->task_flags |= TF_REQUEST_STOP;
		spin_unlock_irqrestore(&cmd->t_state_lock, *flags);

		pr_debug("Task %p waiting to complete\n", task);
		wait_for_completion(&task->task_stop_comp);
		pr_debug("Task %p stopped successfully\n", task);

		spin_lock_irqsave(&cmd->t_state_lock, *flags);
		atomic_dec(&cmd->t_task_cdbs_left);
		task->task_flags &= ~(TF_ACTIVE | TF_REQUEST_STOP);
		was_active = true;
	}

	return was_active;
}

static int transport_stop_tasks_for_cmd(struct se_cmd *cmd)
{
	struct se_task *task, *task_tmp;
	unsigned long flags;
	int ret = 0;

	pr_debug("ITT[0x%08x] - Stopping tasks\n",
		cmd->se_tfo->get_task_tag(cmd));

	spin_lock_irqsave(&cmd->t_state_lock, flags);
	list_for_each_entry_safe(task, task_tmp,
				&cmd->t_task_list, t_list) {
		pr_debug("Processing task %p\n", task);
		if (!(task->task_flags & (TF_ACTIVE | TF_SENT))) {
			spin_unlock_irqrestore(&cmd->t_state_lock,
					flags);
			transport_remove_task_from_execute_queue(task,
					cmd->se_dev);

			pr_debug("Task %p removed from execute queue\n", task);
			spin_lock_irqsave(&cmd->t_state_lock, flags);
			continue;
		}

		if (!target_stop_task(task, &flags)) {
			pr_debug("Task %p - did nothing\n", task);
			ret++;
		}
	}
	spin_unlock_irqrestore(&cmd->t_state_lock, flags);

	return ret;
}

void transport_generic_request_failure(struct se_cmd *cmd)
{
	int ret = 0;

	pr_debug("-----[ Storage Engine Exception for cmd: %p ITT: 0x%08x"
		" CDB: 0x%02x\n", cmd, cmd->se_tfo->get_task_tag(cmd),
		cmd->t_task_cdb[0]);
	pr_debug("-----[ i_state: %d t_state: %d scsi_sense_reason: %d\n",
		cmd->se_tfo->get_cmd_state(cmd),
		cmd->t_state, cmd->scsi_sense_reason);
	pr_debug("-----[ t_tasks: %d t_task_cdbs_left: %d"
		" t_task_cdbs_sent: %d t_task_cdbs_ex_left: %d --"
		" CMD_T_ACTIVE: %d CMD_T_STOP: %d CMD_T_SENT: %d\n",
		cmd->t_task_list_num,
		atomic_read(&cmd->t_task_cdbs_left),
		atomic_read(&cmd->t_task_cdbs_sent),
		atomic_read(&cmd->t_task_cdbs_ex_left),
		(cmd->transport_state & CMD_T_ACTIVE) != 0,
		(cmd->transport_state & CMD_T_STOP) != 0,
		(cmd->transport_state & CMD_T_SENT) != 0);

	if (cmd->se_dev->dev_task_attr_type == SAM_TASK_ATTR_EMULATED)
		transport_complete_task_attr(cmd);

	switch (cmd->scsi_sense_reason) {
	case TCM_NON_EXISTENT_LUN:
	case TCM_UNSUPPORTED_SCSI_OPCODE:
	case TCM_INVALID_CDB_FIELD:
	case TCM_INVALID_PARAMETER_LIST:
	case TCM_LOGICAL_UNIT_COMMUNICATION_FAILURE:
	case TCM_UNKNOWN_MODE_PAGE:
	case TCM_WRITE_PROTECTED:
	case TCM_CHECK_CONDITION_ABORT_CMD:
	case TCM_CHECK_CONDITION_UNIT_ATTENTION:
	case TCM_CHECK_CONDITION_NOT_READY:
		break;
	case TCM_RESERVATION_CONFLICT:
		cmd->scsi_status = SAM_STAT_RESERVATION_CONFLICT;
		if (cmd->se_sess &&
		    cmd->se_dev->se_sub_dev->se_dev_attrib.emulate_ua_intlck_ctrl == 2)
			core_scsi3_ua_allocate(cmd->se_sess->se_node_acl,
				cmd->orig_fe_lun, 0x2C,
				ASCQ_2CH_PREVIOUS_RESERVATION_CONFLICT_STATUS);

		ret = cmd->se_tfo->queue_status(cmd);
		if (ret == -EAGAIN || ret == -ENOMEM)
			goto queue_full;
		goto check_stop;
	default:
		pr_err("Unknown transport error for CDB 0x%02x: %d\n",
			cmd->t_task_cdb[0], cmd->scsi_sense_reason);
		cmd->scsi_sense_reason = TCM_UNSUPPORTED_SCSI_OPCODE;
		break;
	}
	ret = transport_send_check_condition_and_sense(cmd,
			cmd->scsi_sense_reason, 0);
	if (ret == -EAGAIN || ret == -ENOMEM)
		goto queue_full;

check_stop:
	transport_lun_remove_cmd(cmd);
	if (!transport_cmd_check_stop_to_fabric(cmd))
		;
	return;

queue_full:
	cmd->t_state = TRANSPORT_COMPLETE_QF_OK;
	transport_handle_queue_full(cmd, cmd->se_dev);
}
EXPORT_SYMBOL(transport_generic_request_failure);

static inline u32 transport_lba_21(unsigned char *cdb)
{
	return ((cdb[1] & 0x1f) << 16) | (cdb[2] << 8) | cdb[3];
}

static inline u32 transport_lba_32(unsigned char *cdb)
{
	return (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
}

static inline unsigned long long transport_lba_64(unsigned char *cdb)
{
	unsigned int __v1, __v2;

	__v1 = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
	__v2 = (cdb[6] << 24) | (cdb[7] << 16) | (cdb[8] << 8) | cdb[9];

	return ((unsigned long long)__v2) | (unsigned long long)__v1 << 32;
}

static inline unsigned long long transport_lba_64_ext(unsigned char *cdb)
{
	unsigned int __v1, __v2;

	__v1 = (cdb[12] << 24) | (cdb[13] << 16) | (cdb[14] << 8) | cdb[15];
	__v2 = (cdb[16] << 24) | (cdb[17] << 16) | (cdb[18] << 8) | cdb[19];

	return ((unsigned long long)__v2) | (unsigned long long)__v1 << 32;
}

static void transport_set_supported_SAM_opcode(struct se_cmd *se_cmd)
{
	unsigned long flags;

	spin_lock_irqsave(&se_cmd->t_state_lock, flags);
	se_cmd->se_cmd_flags |= SCF_SUPPORTED_SAM_OPCODE;
	spin_unlock_irqrestore(&se_cmd->t_state_lock, flags);
}

static inline int transport_execute_task_attr(struct se_cmd *cmd)
{
	if (cmd->se_dev->dev_task_attr_type != SAM_TASK_ATTR_EMULATED)
		return 1;
	 if (cmd->sam_task_attr == MSG_HEAD_TAG) {
		pr_debug("Added HEAD_OF_QUEUE for CDB:"
			" 0x%02x, se_ordered_id: %u\n",
			cmd->t_task_cdb[0],
			cmd->se_ordered_id);
		return 1;
	} else if (cmd->sam_task_attr == MSG_ORDERED_TAG) {
		atomic_inc(&cmd->se_dev->dev_ordered_sync);
		smp_mb__after_atomic_inc();

		pr_debug("Added ORDERED for CDB: 0x%02x to ordered"
				" list, se_ordered_id: %u\n",
				cmd->t_task_cdb[0],
				cmd->se_ordered_id);
		if (!atomic_read(&cmd->se_dev->simple_cmds))
			return 1;
	} else {
		atomic_inc(&cmd->se_dev->simple_cmds);
		smp_mb__after_atomic_inc();
	}
	if (atomic_read(&cmd->se_dev->dev_ordered_sync) != 0) {
		spin_lock(&cmd->se_dev->delayed_cmd_lock);
		cmd->se_cmd_flags |= SCF_DELAYED_CMD_FROM_SAM_ATTR;
		list_add_tail(&cmd->se_delayed_node,
				&cmd->se_dev->delayed_cmd_list);
		spin_unlock(&cmd->se_dev->delayed_cmd_lock);

		pr_debug("Added CDB: 0x%02x Task Attr: 0x%02x to"
			" delayed CMD list, se_ordered_id: %u\n",
			cmd->t_task_cdb[0], cmd->sam_task_attr,
			cmd->se_ordered_id);
		return 0;
	}
	return 1;
}

static int transport_execute_tasks(struct se_cmd *cmd)
{
	int add_tasks;
	struct se_device *se_dev = cmd->se_dev;
	if (!transport_cmd_check_stop(cmd, 0, TRANSPORT_PROCESSING)) {
		add_tasks = transport_execute_task_attr(cmd);
		if (!add_tasks)
			goto execute_tasks;
		__transport_execute_tasks(se_dev, cmd);
		return 0;
	}

execute_tasks:
	__transport_execute_tasks(se_dev, NULL);
	return 0;
}

static int __transport_execute_tasks(struct se_device *dev, struct se_cmd *new_cmd)
{
	int error;
	struct se_cmd *cmd = NULL;
	struct se_task *task = NULL;
	unsigned long flags;

check_depth:
	spin_lock_irq(&dev->execute_task_lock);
	if (new_cmd != NULL)
		__transport_add_tasks_from_cmd(new_cmd);

	if (list_empty(&dev->execute_task_list)) {
		spin_unlock_irq(&dev->execute_task_lock);
		return 0;
	}
	task = list_first_entry(&dev->execute_task_list,
				struct se_task, t_execute_list);
	__transport_remove_task_from_execute_queue(task, dev);
	spin_unlock_irq(&dev->execute_task_lock);

	cmd = task->task_se_cmd;
	spin_lock_irqsave(&cmd->t_state_lock, flags);
	task->task_flags |= (TF_ACTIVE | TF_SENT);
	atomic_inc(&cmd->t_task_cdbs_sent);

	if (atomic_read(&cmd->t_task_cdbs_sent) ==
	    cmd->t_task_list_num)
		cmd->transport_state |= CMD_T_SENT;

	spin_unlock_irqrestore(&cmd->t_state_lock, flags);

	if (cmd->execute_task)
		error = cmd->execute_task(task);
	else
		error = dev->transport->do_task(task);
	if (error != 0) {
		spin_lock_irqsave(&cmd->t_state_lock, flags);
		task->task_flags &= ~TF_ACTIVE;
		cmd->transport_state &= ~CMD_T_SENT;
		spin_unlock_irqrestore(&cmd->t_state_lock, flags);

		transport_stop_tasks_for_cmd(cmd);
		transport_generic_request_failure(cmd);
	}

	new_cmd = NULL;
	goto check_depth;

	return 0;
}

static inline u32 transport_get_sectors_6(
	unsigned char *cdb,
	struct se_cmd *cmd,
	int *ret)
{
	struct se_device *dev = cmd->se_dev;

	if (!dev)
		goto type_disk;

	if (dev->transport->get_device_type(dev) == TYPE_TAPE)
		return (u32)(cdb[2] << 16) + (cdb[3] << 8) + cdb[4];

	/*
	 * Everything else assume TYPE_DISK Sector CDB location.
	 * Use 8-bit sector value.  SBC-3 says:
	 *
	 *   A TRANSFER LENGTH field set to zero specifies that 256
	 *   logical blocks shall be written.  Any other value
	 *   specifies the number of logical blocks that shall be
	 *   written.
	 */
type_disk:
	return cdb[4] ? : 256;
}

static inline u32 transport_get_sectors_10(
	unsigned char *cdb,
	struct se_cmd *cmd,
	int *ret)
{
	struct se_device *dev = cmd->se_dev;

	if (!dev)
		goto type_disk;

	if (dev->transport->get_device_type(dev) == TYPE_TAPE) {
		*ret = -EINVAL;
		return 0;
	}

type_disk:
	return (u32)(cdb[7] << 8) + cdb[8];
}

static inline u32 transport_get_sectors_12(
	unsigned char *cdb,
	struct se_cmd *cmd,
	int *ret)
{
	struct se_device *dev = cmd->se_dev;

	if (!dev)
		goto type_disk;

	if (dev->transport->get_device_type(dev) == TYPE_TAPE) {
		*ret = -EINVAL;
		return 0;
	}

type_disk:
	return (u32)(cdb[6] << 24) + (cdb[7] << 16) + (cdb[8] << 8) + cdb[9];
}

static inline u32 transport_get_sectors_16(
	unsigned char *cdb,
	struct se_cmd *cmd,
	int *ret)
{
	struct se_device *dev = cmd->se_dev;

	if (!dev)
		goto type_disk;

	if (dev->transport->get_device_type(dev) == TYPE_TAPE)
		return (u32)(cdb[12] << 16) + (cdb[13] << 8) + cdb[14];

type_disk:
	return (u32)(cdb[10] << 24) + (cdb[11] << 16) +
		    (cdb[12] << 8) + cdb[13];
}

static inline u32 transport_get_sectors_32(
	unsigned char *cdb,
	struct se_cmd *cmd,
	int *ret)
{
	return (u32)(cdb[28] << 24) + (cdb[29] << 16) +
		    (cdb[30] << 8) + cdb[31];

}

static inline u32 transport_get_size(
	u32 sectors,
	unsigned char *cdb,
	struct se_cmd *cmd)
{
	struct se_device *dev = cmd->se_dev;

	if (dev->transport->get_device_type(dev) == TYPE_TAPE) {
		if (cdb[1] & 1) { 
			return dev->se_sub_dev->se_dev_attrib.block_size * sectors;
		} else 
			return sectors;
	}
#if 0
	pr_debug("Returning block_size: %u, sectors: %u == %u for"
			" %s object\n", dev->se_sub_dev->se_dev_attrib.block_size, sectors,
			dev->se_sub_dev->se_dev_attrib.block_size * sectors,
			dev->transport->name);
#endif
	return dev->se_sub_dev->se_dev_attrib.block_size * sectors;
}

static void transport_xor_callback(struct se_cmd *cmd)
{
	unsigned char *buf, *addr;
	struct scatterlist *sg;
	unsigned int offset;
	int i;
	int count;
	buf = kmalloc(cmd->data_length, GFP_KERNEL);
	if (!buf) {
		pr_err("Unable to allocate xor_callback buf\n");
		return;
	}
	sg_copy_to_buffer(cmd->t_data_sg,
			  cmd->t_data_nents,
			  buf,
			  cmd->data_length);


	offset = 0;
	for_each_sg(cmd->t_bidi_data_sg, sg, cmd->t_bidi_data_nents, count) {
		addr = kmap_atomic(sg_page(sg));
		if (!addr)
			goto out;

		for (i = 0; i < sg->length; i++)
			*(addr + sg->offset + i) ^= *(buf + offset + i);

		offset += sg->length;
		kunmap_atomic(addr);
	}

out:
	kfree(buf);
}

static int transport_get_sense_data(struct se_cmd *cmd)
{
	unsigned char *buffer = cmd->sense_buffer, *sense_buffer = NULL;
	struct se_device *dev = cmd->se_dev;
	struct se_task *task = NULL, *task_tmp;
	unsigned long flags;
	u32 offset = 0;

	WARN_ON(!cmd->se_lun);

	if (!dev)
		return 0;

	spin_lock_irqsave(&cmd->t_state_lock, flags);
	if (cmd->se_cmd_flags & SCF_SENT_CHECK_CONDITION) {
		spin_unlock_irqrestore(&cmd->t_state_lock, flags);
		return 0;
	}

	list_for_each_entry_safe(task, task_tmp,
				&cmd->t_task_list, t_list) {
		if (!(task->task_flags & TF_HAS_SENSE))
			continue;

		if (!dev->transport->get_sense_buffer) {
			pr_err("dev->transport->get_sense_buffer"
					" is NULL\n");
			continue;
		}

		sense_buffer = dev->transport->get_sense_buffer(task);
		if (!sense_buffer) {
			pr_err("ITT[0x%08x]_TASK[%p]: Unable to locate"
				" sense buffer for task with sense\n",
				cmd->se_tfo->get_task_tag(cmd), task);
			continue;
		}
		spin_unlock_irqrestore(&cmd->t_state_lock, flags);

		offset = cmd->se_tfo->set_fabric_sense_len(cmd,
				TRANSPORT_SENSE_BUFFER);

		memcpy(&buffer[offset], sense_buffer,
				TRANSPORT_SENSE_BUFFER);
		cmd->scsi_status = task->task_scsi_status;
		
		cmd->scsi_sense_length =
				(TRANSPORT_SENSE_BUFFER + offset);

		pr_debug("HBA_[%u]_PLUG[%s]: Set SAM STATUS: 0x%02x"
				" and sense\n",
			dev->se_hba->hba_id, dev->transport->name,
				cmd->scsi_status);
		return 0;
	}
	spin_unlock_irqrestore(&cmd->t_state_lock, flags);

	return -1;
}

static inline long long transport_dev_end_lba(struct se_device *dev)
{
	return dev->transport->get_blocks(dev) + 1;
}

static int transport_cmd_get_valid_sectors(struct se_cmd *cmd)
{
	struct se_device *dev = cmd->se_dev;
	u32 sectors;

	if (dev->transport->get_device_type(dev) != TYPE_DISK)
		return 0;

	sectors = (cmd->data_length / dev->se_sub_dev->se_dev_attrib.block_size);

	if ((cmd->t_task_lba + sectors) > transport_dev_end_lba(dev)) {
		pr_err("LBA: %llu Sectors: %u exceeds"
			" transport_dev_end_lba(): %llu\n",
			cmd->t_task_lba, sectors,
			transport_dev_end_lba(dev));
		return -EINVAL;
	}

	return 0;
}

static int target_check_write_same_discard(unsigned char *flags, struct se_device *dev)
{
	int passthrough = (dev->transport->transport_type ==
				TRANSPORT_PLUGIN_PHBA_PDEV);

	if (!passthrough) {
		if ((flags[0] & 0x04) || (flags[0] & 0x02)) {
			pr_err("WRITE_SAME PBDATA and LBDATA"
				" bits not supported for Block Discard"
				" Emulation\n");
			return -ENOSYS;
		}
		if (!(flags[0] & 0x08)) {
			pr_err("WRITE_SAME w/o UNMAP bit not"
				" supported for Block Discard Emulation\n");
			return -ENOSYS;
		}
	}

	return 0;
}

static int transport_generic_cmd_sequencer(
	struct se_cmd *cmd,
	unsigned char *cdb)
{
	struct se_device *dev = cmd->se_dev;
	struct se_subsystem_dev *su_dev = dev->se_sub_dev;
	int ret = 0, sector_ret = 0, passthrough;
	u32 sectors = 0, size = 0, pr_reg_type = 0;
	u16 service_action;
	u8 alua_ascq = 0;
	if (core_scsi3_ua_check(cmd, cdb) < 0) {
		cmd->se_cmd_flags |= SCF_SCSI_CDB_EXCEPTION;
		cmd->scsi_sense_reason = TCM_CHECK_CONDITION_UNIT_ATTENTION;
		return -EINVAL;
	}
	ret = su_dev->t10_alua.alua_state_check(cmd, cdb, &alua_ascq);
	if (ret != 0) {
		if (ret > 0) {
#if 0
			pr_debug("[%s]: ALUA TG Port not available,"
				" SenseKey: NOT_READY, ASC/ASCQ: 0x04/0x%02x\n",
				cmd->se_tfo->get_fabric_name(), alua_ascq);
#endif
			transport_set_sense_codes(cmd, 0x04, alua_ascq);
			cmd->se_cmd_flags |= SCF_SCSI_CDB_EXCEPTION;
			cmd->scsi_sense_reason = TCM_CHECK_CONDITION_NOT_READY;
			return -EINVAL;
		}
		goto out_invalid_cdb_field;
	}
	if (su_dev->t10_pr.pr_ops.t10_reservation_check(cmd, &pr_reg_type) != 0) {
		if (su_dev->t10_pr.pr_ops.t10_seq_non_holder(
					cmd, cdb, pr_reg_type) != 0) {
			cmd->se_cmd_flags |= SCF_SCSI_CDB_EXCEPTION;
			cmd->se_cmd_flags |= SCF_SCSI_RESERVATION_CONFLICT;
			cmd->scsi_status = SAM_STAT_RESERVATION_CONFLICT;
			cmd->scsi_sense_reason = TCM_RESERVATION_CONFLICT;
			return -EBUSY;
		}
	}

	passthrough =
		(dev->transport->transport_type == TRANSPORT_PLUGIN_PHBA_PDEV);

	switch (cdb[0]) {
	case READ_6:
		sectors = transport_get_sectors_6(cdb, cmd, &sector_ret);
		if (sector_ret)
			goto out_unsupported_cdb;
		size = transport_get_size(sectors, cdb, cmd);
		cmd->t_task_lba = transport_lba_21(cdb);
		cmd->se_cmd_flags |= SCF_SCSI_DATA_SG_IO_CDB;
		break;
	case READ_10:
		sectors = transport_get_sectors_10(cdb, cmd, &sector_ret);
		if (sector_ret)
			goto out_unsupported_cdb;
		size = transport_get_size(sectors, cdb, cmd);
		cmd->t_task_lba = transport_lba_32(cdb);
		cmd->se_cmd_flags |= SCF_SCSI_DATA_SG_IO_CDB;
		break;
	case READ_12:
		sectors = transport_get_sectors_12(cdb, cmd, &sector_ret);
		if (sector_ret)
			goto out_unsupported_cdb;
		size = transport_get_size(sectors, cdb, cmd);
		cmd->t_task_lba = transport_lba_32(cdb);
		cmd->se_cmd_flags |= SCF_SCSI_DATA_SG_IO_CDB;
		break;
	case READ_16:
		sectors = transport_get_sectors_16(cdb, cmd, &sector_ret);
		if (sector_ret)
			goto out_unsupported_cdb;
		size = transport_get_size(sectors, cdb, cmd);
		cmd->t_task_lba = transport_lba_64(cdb);
		cmd->se_cmd_flags |= SCF_SCSI_DATA_SG_IO_CDB;
		break;
	case WRITE_6:
		sectors = transport_get_sectors_6(cdb, cmd, &sector_ret);
		if (sector_ret)
			goto out_unsupported_cdb;
		size = transport_get_size(sectors, cdb, cmd);
		cmd->t_task_lba = transport_lba_21(cdb);
		cmd->se_cmd_flags |= SCF_SCSI_DATA_SG_IO_CDB;
		break;
	case WRITE_10:
		sectors = transport_get_sectors_10(cdb, cmd, &sector_ret);
		if (sector_ret)
			goto out_unsupported_cdb;
		size = transport_get_size(sectors, cdb, cmd);
		cmd->t_task_lba = transport_lba_32(cdb);
		if (cdb[1] & 0x8)
			cmd->se_cmd_flags |= SCF_FUA;
		cmd->se_cmd_flags |= SCF_SCSI_DATA_SG_IO_CDB;
		break;
	case WRITE_12:
		sectors = transport_get_sectors_12(cdb, cmd, &sector_ret);
		if (sector_ret)
			goto out_unsupported_cdb;
		size = transport_get_size(sectors, cdb, cmd);
		cmd->t_task_lba = transport_lba_32(cdb);
		if (cdb[1] & 0x8)
			cmd->se_cmd_flags |= SCF_FUA;
		cmd->se_cmd_flags |= SCF_SCSI_DATA_SG_IO_CDB;
		break;
	case WRITE_16:
		sectors = transport_get_sectors_16(cdb, cmd, &sector_ret);
		if (sector_ret)
			goto out_unsupported_cdb;
		size = transport_get_size(sectors, cdb, cmd);
		cmd->t_task_lba = transport_lba_64(cdb);
		if (cdb[1] & 0x8)
			cmd->se_cmd_flags |= SCF_FUA;
		cmd->se_cmd_flags |= SCF_SCSI_DATA_SG_IO_CDB;
		break;
	case XDWRITEREAD_10:
		if ((cmd->data_direction != DMA_TO_DEVICE) ||
		    !(cmd->se_cmd_flags & SCF_BIDI))
			goto out_invalid_cdb_field;
		sectors = transport_get_sectors_10(cdb, cmd, &sector_ret);
		if (sector_ret)
			goto out_unsupported_cdb;
		size = transport_get_size(sectors, cdb, cmd);
		cmd->t_task_lba = transport_lba_32(cdb);
		cmd->se_cmd_flags |= SCF_SCSI_DATA_SG_IO_CDB;

		if (passthrough)
			goto out_unsupported_cdb;

		cmd->transport_complete_callback = &transport_xor_callback;
		if (cdb[1] & 0x8)
			cmd->se_cmd_flags |= SCF_FUA;
		break;
	case VARIABLE_LENGTH_CMD:
		service_action = get_unaligned_be16(&cdb[8]);
		switch (service_action) {
		case XDWRITEREAD_32:
			sectors = transport_get_sectors_32(cdb, cmd, &sector_ret);
			if (sector_ret)
				goto out_unsupported_cdb;
			size = transport_get_size(sectors, cdb, cmd);
			cmd->t_task_lba = transport_lba_64_ext(cdb);
			cmd->se_cmd_flags |= SCF_SCSI_DATA_SG_IO_CDB;

			if (passthrough)
				goto out_unsupported_cdb;

			cmd->transport_complete_callback = &transport_xor_callback;
			if (cdb[1] & 0x8)
				cmd->se_cmd_flags |= SCF_FUA;
			break;
		case WRITE_SAME_32:
			sectors = transport_get_sectors_32(cdb, cmd, &sector_ret);
			if (sector_ret)
				goto out_unsupported_cdb;

			if (sectors)
				size = transport_get_size(1, cdb, cmd);
			else {
				pr_err("WSNZ=1, WRITE_SAME w/sectors=0 not"
				       " supported\n");
				goto out_invalid_cdb_field;
			}

			cmd->t_task_lba = get_unaligned_be64(&cdb[12]);
			cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;

			if (target_check_write_same_discard(&cdb[10], dev) < 0)
				goto out_unsupported_cdb;
			if (!passthrough)
				cmd->execute_task = target_emulate_write_same;
			break;
		default:
			pr_err("VARIABLE_LENGTH_CMD service action"
				" 0x%04x not supported\n", service_action);
			goto out_unsupported_cdb;
		}
		break;
	case MAINTENANCE_IN:
		if (dev->transport->get_device_type(dev) != TYPE_ROM) {
			
			if (cdb[1] == MI_REPORT_TARGET_PGS &&
			    su_dev->t10_alua.alua_type == SPC3_ALUA_EMULATED) {
				cmd->execute_task =
					target_emulate_report_target_port_groups;
			}
			size = (cdb[6] << 24) | (cdb[7] << 16) |
			       (cdb[8] << 8) | cdb[9];
		} else {
			
			size = (cdb[8] << 8) + cdb[9];
		}
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
	case MODE_SELECT:
		size = cdb[4];
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
	case MODE_SELECT_10:
		size = (cdb[7] << 8) + cdb[8];
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
	case MODE_SENSE:
		size = cdb[4];
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		if (!passthrough)
			cmd->execute_task = target_emulate_modesense;
		break;
	case MODE_SENSE_10:
		size = (cdb[7] << 8) + cdb[8];
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		if (!passthrough)
			cmd->execute_task = target_emulate_modesense;
		break;
	case GPCMD_READ_BUFFER_CAPACITY:
	case GPCMD_SEND_OPC:
	case LOG_SELECT:
	case LOG_SENSE:
		size = (cdb[7] << 8) + cdb[8];
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
	case READ_BLOCK_LIMITS:
		size = READ_BLOCK_LEN;
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
	case GPCMD_GET_CONFIGURATION:
	case GPCMD_READ_FORMAT_CAPACITIES:
	case GPCMD_READ_DISC_INFO:
	case GPCMD_READ_TRACK_RZONE_INFO:
		size = (cdb[7] << 8) + cdb[8];
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
	case PERSISTENT_RESERVE_IN:
		if (su_dev->t10_pr.res_type == SPC3_PERSISTENT_RESERVATIONS)
			cmd->execute_task = target_scsi3_emulate_pr_in;
		size = (cdb[7] << 8) + cdb[8];
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
	case PERSISTENT_RESERVE_OUT:
		if (su_dev->t10_pr.res_type == SPC3_PERSISTENT_RESERVATIONS)
			cmd->execute_task = target_scsi3_emulate_pr_out;
		size = (cdb[7] << 8) + cdb[8];
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
	case GPCMD_MECHANISM_STATUS:
	case GPCMD_READ_DVD_STRUCTURE:
		size = (cdb[8] << 8) + cdb[9];
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
	case READ_POSITION:
		size = READ_POSITION_LEN;
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
	case MAINTENANCE_OUT:
		if (dev->transport->get_device_type(dev) != TYPE_ROM) {
			if (cdb[1] == MO_SET_TARGET_PGS &&
			    su_dev->t10_alua.alua_type == SPC3_ALUA_EMULATED) {
				cmd->execute_task =
					target_emulate_set_target_port_groups;
			}

			size = (cdb[6] << 24) | (cdb[7] << 16) |
			       (cdb[8] << 8) | cdb[9];
		} else  {
			
			size = (cdb[8] << 8) + cdb[9];
		}
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
	case INQUIRY:
		size = (cdb[3] << 8) + cdb[4];
		if (cmd->se_dev->dev_task_attr_type == SAM_TASK_ATTR_EMULATED)
			cmd->sam_task_attr = MSG_HEAD_TAG;
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		if (!passthrough)
			cmd->execute_task = target_emulate_inquiry;
		break;
	case READ_BUFFER:
		size = (cdb[6] << 16) + (cdb[7] << 8) + cdb[8];
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
	case READ_CAPACITY:
		size = READ_CAP_LEN;
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		if (!passthrough)
			cmd->execute_task = target_emulate_readcapacity;
		break;
	case READ_MEDIA_SERIAL_NUMBER:
	case SECURITY_PROTOCOL_IN:
	case SECURITY_PROTOCOL_OUT:
		size = (cdb[6] << 24) | (cdb[7] << 16) | (cdb[8] << 8) | cdb[9];
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
	case SERVICE_ACTION_IN:
		switch (cmd->t_task_cdb[1] & 0x1f) {
		case SAI_READ_CAPACITY_16:
			if (!passthrough)
				cmd->execute_task =
					target_emulate_readcapacity_16;
			break;
		default:
			if (passthrough)
				break;

			pr_err("Unsupported SA: 0x%02x\n",
				cmd->t_task_cdb[1] & 0x1f);
			goto out_invalid_cdb_field;
		}
		
	case ACCESS_CONTROL_IN:
	case ACCESS_CONTROL_OUT:
	case EXTENDED_COPY:
	case READ_ATTRIBUTE:
	case RECEIVE_COPY_RESULTS:
	case WRITE_ATTRIBUTE:
		size = (cdb[10] << 24) | (cdb[11] << 16) |
		       (cdb[12] << 8) | cdb[13];
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
	case RECEIVE_DIAGNOSTIC:
	case SEND_DIAGNOSTIC:
		size = (cdb[3] << 8) | cdb[4];
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
#if 0
	case GPCMD_READ_CD:
		sectors = (cdb[6] << 16) + (cdb[7] << 8) + cdb[8];
		size = (2336 * sectors);
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
#endif
	case READ_TOC:
		size = cdb[8];
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
	case REQUEST_SENSE:
		size = cdb[4];
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		if (!passthrough)
			cmd->execute_task = target_emulate_request_sense;
		break;
	case READ_ELEMENT_STATUS:
		size = 65536 * cdb[7] + 256 * cdb[8] + cdb[9];
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
	case WRITE_BUFFER:
		size = (cdb[6] << 16) + (cdb[7] << 8) + cdb[8];
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
	case RESERVE:
	case RESERVE_10:
		if (cdb[0] == RESERVE_10)
			size = (cdb[7] << 8) | cdb[8];
		else
			size = cmd->data_length;

		if (su_dev->t10_pr.res_type != SPC_PASSTHROUGH)
			cmd->execute_task = target_scsi2_reservation_reserve;
		cmd->se_cmd_flags |= SCF_SCSI_NON_DATA_CDB;
		break;
	case RELEASE:
	case RELEASE_10:
		if (cdb[0] == RELEASE_10)
			size = (cdb[7] << 8) | cdb[8];
		else
			size = cmd->data_length;

		if (su_dev->t10_pr.res_type != SPC_PASSTHROUGH)
			cmd->execute_task = target_scsi2_reservation_release;
		cmd->se_cmd_flags |= SCF_SCSI_NON_DATA_CDB;
		break;
	case SYNCHRONIZE_CACHE:
	case SYNCHRONIZE_CACHE_16:
		if (cdb[0] == SYNCHRONIZE_CACHE) {
			sectors = transport_get_sectors_10(cdb, cmd, &sector_ret);
			cmd->t_task_lba = transport_lba_32(cdb);
		} else {
			sectors = transport_get_sectors_16(cdb, cmd, &sector_ret);
			cmd->t_task_lba = transport_lba_64(cdb);
		}
		if (sector_ret)
			goto out_unsupported_cdb;

		size = transport_get_size(sectors, cdb, cmd);
		cmd->se_cmd_flags |= SCF_SCSI_NON_DATA_CDB;

		if (passthrough)
			break;

		if ((cmd->t_task_lba != 0) || (sectors != 0)) {
			if (transport_cmd_get_valid_sectors(cmd) < 0)
				goto out_invalid_cdb_field;
		}
		cmd->execute_task = target_emulate_synchronize_cache;
		break;
	case UNMAP:
		size = get_unaligned_be16(&cdb[7]);
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		if (!passthrough)
			cmd->execute_task = target_emulate_unmap;
		break;
	case WRITE_SAME_16:
		sectors = transport_get_sectors_16(cdb, cmd, &sector_ret);
		if (sector_ret)
			goto out_unsupported_cdb;

		if (sectors)
			size = transport_get_size(1, cdb, cmd);
		else {
			pr_err("WSNZ=1, WRITE_SAME w/sectors=0 not supported\n");
			goto out_invalid_cdb_field;
		}

		cmd->t_task_lba = get_unaligned_be64(&cdb[2]);
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;

		if (target_check_write_same_discard(&cdb[1], dev) < 0)
			goto out_unsupported_cdb;
		if (!passthrough)
			cmd->execute_task = target_emulate_write_same;
		break;
	case WRITE_SAME:
		sectors = transport_get_sectors_10(cdb, cmd, &sector_ret);
		if (sector_ret)
			goto out_unsupported_cdb;

		if (sectors)
			size = transport_get_size(1, cdb, cmd);
		else {
			pr_err("WSNZ=1, WRITE_SAME w/sectors=0 not supported\n");
			goto out_invalid_cdb_field;
		}

		cmd->t_task_lba = get_unaligned_be32(&cdb[2]);
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		if (target_check_write_same_discard(&cdb[1], dev) < 0)
			goto out_unsupported_cdb;
		if (!passthrough)
			cmd->execute_task = target_emulate_write_same;
		break;
	case ALLOW_MEDIUM_REMOVAL:
	case ERASE:
	case REZERO_UNIT:
	case SEEK_10:
	case SPACE:
	case START_STOP:
	case TEST_UNIT_READY:
	case VERIFY:
	case WRITE_FILEMARKS:
		cmd->se_cmd_flags |= SCF_SCSI_NON_DATA_CDB;
		if (!passthrough)
			cmd->execute_task = target_emulate_noop;
		break;
	case GPCMD_CLOSE_TRACK:
	case INITIALIZE_ELEMENT_STATUS:
	case GPCMD_LOAD_UNLOAD:
	case GPCMD_SET_SPEED:
	case MOVE_MEDIUM:
		cmd->se_cmd_flags |= SCF_SCSI_NON_DATA_CDB;
		break;
	case REPORT_LUNS:
		cmd->execute_task = target_report_luns;
		size = (cdb[6] << 24) | (cdb[7] << 16) | (cdb[8] << 8) | cdb[9];
		if (cmd->se_dev->dev_task_attr_type == SAM_TASK_ATTR_EMULATED)
			cmd->sam_task_attr = MSG_HEAD_TAG;
		cmd->se_cmd_flags |= SCF_SCSI_CONTROL_SG_IO_CDB;
		break;
	default:
		pr_warn("TARGET_CORE[%s]: Unsupported SCSI Opcode"
			" 0x%02x, sending CHECK_CONDITION.\n",
			cmd->se_tfo->get_fabric_name(), cdb[0]);
		goto out_unsupported_cdb;
	}

	if (cmd->unknown_data_length)
		cmd->data_length = size;

	if (size != cmd->data_length) {
		pr_warn("TARGET_CORE[%s]: Expected Transfer Length:"
			" %u does not match SCSI CDB Length: %u for SAM Opcode:"
			" 0x%02x\n", cmd->se_tfo->get_fabric_name(),
				cmd->data_length, size, cdb[0]);

		cmd->cmd_spdtl = size;

		if (cmd->data_direction == DMA_TO_DEVICE) {
			pr_err("Rejecting underflow/overflow"
					" WRITE data\n");
			goto out_invalid_cdb_field;
		}
		if (!ret && (dev->se_sub_dev->se_dev_attrib.block_size != 512))  {
			pr_err("Failing OVERFLOW/UNDERFLOW for LBA op"
				" CDB on non 512-byte sector setup subsystem"
				" plugin: %s\n", dev->transport->name);
			
			goto out_invalid_cdb_field;
		}

		if (size > cmd->data_length) {
			cmd->se_cmd_flags |= SCF_OVERFLOW_BIT;
			cmd->residual_count = (size - cmd->data_length);
		} else {
			cmd->se_cmd_flags |= SCF_UNDERFLOW_BIT;
			cmd->residual_count = (cmd->data_length - size);
		}
		cmd->data_length = size;
	}

	if (cmd->se_cmd_flags & SCF_SCSI_DATA_SG_IO_CDB &&
	    sectors > dev->se_sub_dev->se_dev_attrib.fabric_max_sectors) {
		printk_ratelimited(KERN_ERR "SCSI OP %02xh with too big sectors %u\n",
				   cdb[0], sectors);
		goto out_invalid_cdb_field;
	}

	
	if (!(passthrough || cmd->execute_task ||
	     (cmd->se_cmd_flags & SCF_SCSI_DATA_SG_IO_CDB)))
		goto out_unsupported_cdb;

	transport_set_supported_SAM_opcode(cmd);
	return ret;

out_unsupported_cdb:
	cmd->se_cmd_flags |= SCF_SCSI_CDB_EXCEPTION;
	cmd->scsi_sense_reason = TCM_UNSUPPORTED_SCSI_OPCODE;
	return -EINVAL;
out_invalid_cdb_field:
	cmd->se_cmd_flags |= SCF_SCSI_CDB_EXCEPTION;
	cmd->scsi_sense_reason = TCM_INVALID_CDB_FIELD;
	return -EINVAL;
}

static void transport_complete_task_attr(struct se_cmd *cmd)
{
	struct se_device *dev = cmd->se_dev;
	struct se_cmd *cmd_p, *cmd_tmp;
	int new_active_tasks = 0;

	if (cmd->sam_task_attr == MSG_SIMPLE_TAG) {
		atomic_dec(&dev->simple_cmds);
		smp_mb__after_atomic_dec();
		dev->dev_cur_ordered_id++;
		pr_debug("Incremented dev->dev_cur_ordered_id: %u for"
			" SIMPLE: %u\n", dev->dev_cur_ordered_id,
			cmd->se_ordered_id);
	} else if (cmd->sam_task_attr == MSG_HEAD_TAG) {
		dev->dev_cur_ordered_id++;
		pr_debug("Incremented dev_cur_ordered_id: %u for"
			" HEAD_OF_QUEUE: %u\n", dev->dev_cur_ordered_id,
			cmd->se_ordered_id);
	} else if (cmd->sam_task_attr == MSG_ORDERED_TAG) {
		atomic_dec(&dev->dev_ordered_sync);
		smp_mb__after_atomic_dec();

		dev->dev_cur_ordered_id++;
		pr_debug("Incremented dev_cur_ordered_id: %u for ORDERED:"
			" %u\n", dev->dev_cur_ordered_id, cmd->se_ordered_id);
	}
	spin_lock(&dev->delayed_cmd_lock);
	list_for_each_entry_safe(cmd_p, cmd_tmp,
			&dev->delayed_cmd_list, se_delayed_node) {

		list_del(&cmd_p->se_delayed_node);
		spin_unlock(&dev->delayed_cmd_lock);

		pr_debug("Calling add_tasks() for"
			" cmd_p: 0x%02x Task Attr: 0x%02x"
			" Dormant -> Active, se_ordered_id: %u\n",
			cmd_p->t_task_cdb[0],
			cmd_p->sam_task_attr, cmd_p->se_ordered_id);

		transport_add_tasks_from_cmd(cmd_p);
		new_active_tasks++;

		spin_lock(&dev->delayed_cmd_lock);
		if (cmd_p->sam_task_attr == MSG_ORDERED_TAG)
			break;
	}
	spin_unlock(&dev->delayed_cmd_lock);
	if (new_active_tasks != 0)
		wake_up_interruptible(&dev->dev_queue_obj.thread_wq);
}

static void transport_complete_qf(struct se_cmd *cmd)
{
	int ret = 0;

	if (cmd->se_dev->dev_task_attr_type == SAM_TASK_ATTR_EMULATED)
		transport_complete_task_attr(cmd);

	if (cmd->se_cmd_flags & SCF_TRANSPORT_TASK_SENSE) {
		ret = cmd->se_tfo->queue_status(cmd);
		if (ret)
			goto out;
	}

	switch (cmd->data_direction) {
	case DMA_FROM_DEVICE:
		ret = cmd->se_tfo->queue_data_in(cmd);
		break;
	case DMA_TO_DEVICE:
		if (cmd->t_bidi_data_sg) {
			ret = cmd->se_tfo->queue_data_in(cmd);
			if (ret < 0)
				break;
		}
		
	case DMA_NONE:
		ret = cmd->se_tfo->queue_status(cmd);
		break;
	default:
		break;
	}

out:
	if (ret < 0) {
		transport_handle_queue_full(cmd, cmd->se_dev);
		return;
	}
	transport_lun_remove_cmd(cmd);
	transport_cmd_check_stop_to_fabric(cmd);
}

static void transport_handle_queue_full(
	struct se_cmd *cmd,
	struct se_device *dev)
{
	spin_lock_irq(&dev->qf_cmd_lock);
	list_add_tail(&cmd->se_qf_node, &cmd->se_dev->qf_cmd_list);
	atomic_inc(&dev->dev_qf_count);
	smp_mb__after_atomic_inc();
	spin_unlock_irq(&cmd->se_dev->qf_cmd_lock);

	schedule_work(&cmd->se_dev->qf_work_queue);
}

static void target_complete_ok_work(struct work_struct *work)
{
	struct se_cmd *cmd = container_of(work, struct se_cmd, work);
	int reason = 0, ret;

	if (cmd->se_dev->dev_task_attr_type == SAM_TASK_ATTR_EMULATED)
		transport_complete_task_attr(cmd);
	if (atomic_read(&cmd->se_dev->dev_qf_count) != 0)
		schedule_work(&cmd->se_dev->qf_work_queue);

	if (cmd->se_cmd_flags & SCF_TRANSPORT_TASK_SENSE) {
		if (transport_get_sense_data(cmd) < 0)
			reason = TCM_NON_EXISTENT_LUN;

		if (cmd->scsi_status) {
			ret = transport_send_check_condition_and_sense(
					cmd, reason, 1);
			if (ret == -EAGAIN || ret == -ENOMEM)
				goto queue_full;

			transport_lun_remove_cmd(cmd);
			transport_cmd_check_stop_to_fabric(cmd);
			return;
		}
	}
	if (cmd->transport_complete_callback)
		cmd->transport_complete_callback(cmd);

	switch (cmd->data_direction) {
	case DMA_FROM_DEVICE:
		spin_lock(&cmd->se_lun->lun_sep_lock);
		if (cmd->se_lun->lun_sep) {
			cmd->se_lun->lun_sep->sep_stats.tx_data_octets +=
					cmd->data_length;
		}
		spin_unlock(&cmd->se_lun->lun_sep_lock);

		ret = cmd->se_tfo->queue_data_in(cmd);
		if (ret == -EAGAIN || ret == -ENOMEM)
			goto queue_full;
		break;
	case DMA_TO_DEVICE:
		spin_lock(&cmd->se_lun->lun_sep_lock);
		if (cmd->se_lun->lun_sep) {
			cmd->se_lun->lun_sep->sep_stats.rx_data_octets +=
				cmd->data_length;
		}
		spin_unlock(&cmd->se_lun->lun_sep_lock);
		if (cmd->t_bidi_data_sg) {
			spin_lock(&cmd->se_lun->lun_sep_lock);
			if (cmd->se_lun->lun_sep) {
				cmd->se_lun->lun_sep->sep_stats.tx_data_octets +=
					cmd->data_length;
			}
			spin_unlock(&cmd->se_lun->lun_sep_lock);
			ret = cmd->se_tfo->queue_data_in(cmd);
			if (ret == -EAGAIN || ret == -ENOMEM)
				goto queue_full;
			break;
		}
		
	case DMA_NONE:
		ret = cmd->se_tfo->queue_status(cmd);
		if (ret == -EAGAIN || ret == -ENOMEM)
			goto queue_full;
		break;
	default:
		break;
	}

	transport_lun_remove_cmd(cmd);
	transport_cmd_check_stop_to_fabric(cmd);
	return;

queue_full:
	pr_debug("Handling complete_ok QUEUE_FULL: se_cmd: %p,"
		" data_direction: %d\n", cmd, cmd->data_direction);
	cmd->t_state = TRANSPORT_COMPLETE_QF_OK;
	transport_handle_queue_full(cmd, cmd->se_dev);
}

static void transport_free_dev_tasks(struct se_cmd *cmd)
{
	struct se_task *task, *task_tmp;
	unsigned long flags;
	LIST_HEAD(dispose_list);

	spin_lock_irqsave(&cmd->t_state_lock, flags);
	list_for_each_entry_safe(task, task_tmp,
				&cmd->t_task_list, t_list) {
		if (!(task->task_flags & TF_ACTIVE))
			list_move_tail(&task->t_list, &dispose_list);
	}
	spin_unlock_irqrestore(&cmd->t_state_lock, flags);

	while (!list_empty(&dispose_list)) {
		task = list_first_entry(&dispose_list, struct se_task, t_list);

		if (task->task_sg != cmd->t_data_sg &&
		    task->task_sg != cmd->t_bidi_data_sg)
			kfree(task->task_sg);

		list_del(&task->t_list);

		cmd->se_dev->transport->free_task(task);
	}
}

static inline void transport_free_sgl(struct scatterlist *sgl, int nents)
{
	struct scatterlist *sg;
	int count;

	for_each_sg(sgl, sg, nents, count)
		__free_page(sg_page(sg));

	kfree(sgl);
}

static inline void transport_free_pages(struct se_cmd *cmd)
{
	if (cmd->se_cmd_flags & SCF_PASSTHROUGH_SG_TO_MEM_NOALLOC)
		return;

	transport_free_sgl(cmd->t_data_sg, cmd->t_data_nents);
	cmd->t_data_sg = NULL;
	cmd->t_data_nents = 0;

	transport_free_sgl(cmd->t_bidi_data_sg, cmd->t_bidi_data_nents);
	cmd->t_bidi_data_sg = NULL;
	cmd->t_bidi_data_nents = 0;
}

static void transport_release_cmd(struct se_cmd *cmd)
{
	BUG_ON(!cmd->se_tfo);

	if (cmd->se_cmd_flags & SCF_SCSI_TMR_CDB)
		core_tmr_release_req(cmd->se_tmr_req);
	if (cmd->t_task_cdb != cmd->__t_task_cdb)
		kfree(cmd->t_task_cdb);
	 if (cmd->check_release != 0) {
		target_put_sess_cmd(cmd->se_sess, cmd);
		return;
	}
	cmd->se_tfo->release_cmd(cmd);
}

static void transport_put_cmd(struct se_cmd *cmd)
{
	unsigned long flags;
	int free_tasks = 0;

	spin_lock_irqsave(&cmd->t_state_lock, flags);
	if (atomic_read(&cmd->t_fe_count)) {
		if (!atomic_dec_and_test(&cmd->t_fe_count))
			goto out_busy;
	}

	if (atomic_read(&cmd->t_se_count)) {
		if (!atomic_dec_and_test(&cmd->t_se_count))
			goto out_busy;
	}

	if (cmd->transport_state & CMD_T_DEV_ACTIVE) {
		cmd->transport_state &= ~CMD_T_DEV_ACTIVE;
		transport_all_task_dev_remove_state(cmd);
		free_tasks = 1;
	}
	spin_unlock_irqrestore(&cmd->t_state_lock, flags);

	if (free_tasks != 0)
		transport_free_dev_tasks(cmd);

	transport_free_pages(cmd);
	transport_release_cmd(cmd);
	return;
out_busy:
	spin_unlock_irqrestore(&cmd->t_state_lock, flags);
}

int transport_generic_map_mem_to_cmd(
	struct se_cmd *cmd,
	struct scatterlist *sgl,
	u32 sgl_count,
	struct scatterlist *sgl_bidi,
	u32 sgl_bidi_count)
{
	if (!sgl || !sgl_count)
		return 0;

	if ((cmd->se_cmd_flags & SCF_SCSI_DATA_SG_IO_CDB) ||
	    (cmd->se_cmd_flags & SCF_SCSI_CONTROL_SG_IO_CDB)) {
		if (cmd->se_cmd_flags & SCF_OVERFLOW_BIT) {
			pr_warn("Rejecting SCSI DATA overflow for fabric using"
				" SCF_PASSTHROUGH_SG_TO_MEM_NOALLOC\n");
			cmd->se_cmd_flags |= SCF_SCSI_CDB_EXCEPTION;
			cmd->scsi_sense_reason = TCM_INVALID_CDB_FIELD;
			return -EINVAL;
		}

		cmd->t_data_sg = sgl;
		cmd->t_data_nents = sgl_count;

		if (sgl_bidi && sgl_bidi_count) {
			cmd->t_bidi_data_sg = sgl_bidi;
			cmd->t_bidi_data_nents = sgl_bidi_count;
		}
		cmd->se_cmd_flags |= SCF_PASSTHROUGH_SG_TO_MEM_NOALLOC;
	}

	return 0;
}
EXPORT_SYMBOL(transport_generic_map_mem_to_cmd);

void *transport_kmap_data_sg(struct se_cmd *cmd)
{
	struct scatterlist *sg = cmd->t_data_sg;
	struct page **pages;
	int i;

	BUG_ON(!sg);
	if (!cmd->t_data_nents)
		return NULL;
	else if (cmd->t_data_nents == 1)
		return kmap(sg_page(sg)) + sg->offset;

	
	pages = kmalloc(sizeof(*pages) * cmd->t_data_nents, GFP_KERNEL);
	if (!pages)
		return NULL;

	
	for_each_sg(cmd->t_data_sg, sg, cmd->t_data_nents, i) {
		pages[i] = sg_page(sg);
	}

	cmd->t_data_vmap = vmap(pages, cmd->t_data_nents,  VM_MAP, PAGE_KERNEL);
	kfree(pages);
	if (!cmd->t_data_vmap)
		return NULL;

	return cmd->t_data_vmap + cmd->t_data_sg[0].offset;
}
EXPORT_SYMBOL(transport_kmap_data_sg);

void transport_kunmap_data_sg(struct se_cmd *cmd)
{
	if (!cmd->t_data_nents) {
		return;
	} else if (cmd->t_data_nents == 1) {
		kunmap(sg_page(cmd->t_data_sg));
		return;
	}

	vunmap(cmd->t_data_vmap);
	cmd->t_data_vmap = NULL;
}
EXPORT_SYMBOL(transport_kunmap_data_sg);

static int
transport_generic_get_mem(struct se_cmd *cmd)
{
	u32 length = cmd->data_length;
	unsigned int nents;
	struct page *page;
	gfp_t zero_flag;
	int i = 0;

	nents = DIV_ROUND_UP(length, PAGE_SIZE);
	cmd->t_data_sg = kmalloc(sizeof(struct scatterlist) * nents, GFP_KERNEL);
	if (!cmd->t_data_sg)
		return -ENOMEM;

	cmd->t_data_nents = nents;
	sg_init_table(cmd->t_data_sg, nents);

	zero_flag = cmd->se_cmd_flags & SCF_SCSI_DATA_SG_IO_CDB ? 0 : __GFP_ZERO;

	while (length) {
		u32 page_len = min_t(u32, length, PAGE_SIZE);
		page = alloc_page(GFP_KERNEL | zero_flag);
		if (!page)
			goto out;

		sg_set_page(&cmd->t_data_sg[i], page, page_len, 0);
		length -= page_len;
		i++;
	}
	return 0;

out:
	while (i >= 0) {
		__free_page(sg_page(&cmd->t_data_sg[i]));
		i--;
	}
	kfree(cmd->t_data_sg);
	cmd->t_data_sg = NULL;
	return -ENOMEM;
}

static inline sector_t transport_limit_task_sectors(
	struct se_device *dev,
	unsigned long long lba,
	sector_t sectors)
{
	sectors = min_t(sector_t, sectors, dev->se_sub_dev->se_dev_attrib.max_sectors);

	if (dev->transport->get_device_type(dev) == TYPE_DISK)
		if ((lba + sectors) > transport_dev_end_lba(dev))
			sectors = ((transport_dev_end_lba(dev) - lba) + 1);

	return sectors;
}


void transport_do_task_sg_chain(struct se_cmd *cmd)
{
	struct scatterlist *sg_first = NULL;
	struct scatterlist *sg_prev = NULL;
	int sg_prev_nents = 0;
	struct scatterlist *sg;
	struct se_task *task;
	u32 chained_nents = 0;
	int i;

	BUG_ON(!cmd->se_tfo->task_sg_chaining);

	list_for_each_entry(task, &cmd->t_task_list, t_list) {
		if (!task->task_sg)
			continue;

		if (!sg_first) {
			sg_first = task->task_sg;
			chained_nents = task->task_sg_nents;
		} else {
			sg_chain(sg_prev, sg_prev_nents, task->task_sg);
			chained_nents += task->task_sg_nents;
		}
		sg_prev_nents = (task->task_sg_nents + 1);
		sg_prev = task->task_sg;
	}
	cmd->t_tasks_sg_chained = sg_first;
	cmd->t_tasks_sg_chained_no = chained_nents;

	pr_debug("Setup cmd: %p cmd->t_tasks_sg_chained: %p and"
		" t_tasks_sg_chained_no: %u\n", cmd, cmd->t_tasks_sg_chained,
		cmd->t_tasks_sg_chained_no);

	for_each_sg(cmd->t_tasks_sg_chained, sg,
			cmd->t_tasks_sg_chained_no, i) {

		pr_debug("SG[%d]: %p page: %p length: %d offset: %d\n",
			i, sg, sg_page(sg), sg->length, sg->offset);
		if (sg_is_chain(sg))
			pr_debug("SG: %p sg_is_chain=1\n", sg);
		if (sg_is_last(sg))
			pr_debug("SG: %p sg_is_last=1\n", sg);
	}
}
EXPORT_SYMBOL(transport_do_task_sg_chain);

static int
transport_allocate_data_tasks(struct se_cmd *cmd,
	enum dma_data_direction data_direction,
	struct scatterlist *cmd_sg, unsigned int sgl_nents)
{
	struct se_device *dev = cmd->se_dev;
	int task_count, i;
	unsigned long long lba;
	sector_t sectors, dev_max_sectors;
	u32 sector_size;

	if (transport_cmd_get_valid_sectors(cmd) < 0)
		return -EINVAL;

	dev_max_sectors = dev->se_sub_dev->se_dev_attrib.max_sectors;
	sector_size = dev->se_sub_dev->se_dev_attrib.block_size;

	WARN_ON(cmd->data_length % sector_size);

	lba = cmd->t_task_lba;
	sectors = DIV_ROUND_UP(cmd->data_length, sector_size);
	task_count = DIV_ROUND_UP_SECTOR_T(sectors, dev_max_sectors);

	if (task_count == 1) {
		struct se_task *task;
		unsigned long flags;

		task = transport_generic_get_task(cmd, data_direction);
		if (!task)
			return -ENOMEM;

		task->task_sg = cmd_sg;
		task->task_sg_nents = sgl_nents;

		task->task_lba = lba;
		task->task_sectors = sectors;
		task->task_size = task->task_sectors * sector_size;

		spin_lock_irqsave(&cmd->t_state_lock, flags);
		list_add_tail(&task->t_list, &cmd->t_task_list);
		spin_unlock_irqrestore(&cmd->t_state_lock, flags);

		return task_count;
	}

	for (i = 0; i < task_count; i++) {
		struct se_task *task;
		unsigned int task_size, task_sg_nents_padded;
		struct scatterlist *sg;
		unsigned long flags;
		int count;

		task = transport_generic_get_task(cmd, data_direction);
		if (!task)
			return -ENOMEM;

		task->task_lba = lba;
		task->task_sectors = min(sectors, dev_max_sectors);
		task->task_size = task->task_sectors * sector_size;

		task->task_sg_nents = DIV_ROUND_UP(task->task_size, PAGE_SIZE);
		if (cmd->se_tfo->task_sg_chaining && (i < (task_count - 1))) {
			task_sg_nents_padded = (task->task_sg_nents + 1);
		} else
			task_sg_nents_padded = task->task_sg_nents;

		task->task_sg = kmalloc(sizeof(struct scatterlist) *
					task_sg_nents_padded, GFP_KERNEL);
		if (!task->task_sg) {
			cmd->se_dev->transport->free_task(task);
			return -ENOMEM;
		}

		sg_init_table(task->task_sg, task_sg_nents_padded);

		task_size = task->task_size;

		
		for_each_sg(task->task_sg, sg, task->task_sg_nents, count) {
			if (cmd_sg->length > task_size)
				break;

			*sg = *cmd_sg;
			task_size -= cmd_sg->length;
			cmd_sg = sg_next(cmd_sg);
		}

		lba += task->task_sectors;
		sectors -= task->task_sectors;

		spin_lock_irqsave(&cmd->t_state_lock, flags);
		list_add_tail(&task->t_list, &cmd->t_task_list);
		spin_unlock_irqrestore(&cmd->t_state_lock, flags);
	}

	return task_count;
}

static int
transport_allocate_control_task(struct se_cmd *cmd)
{
	struct se_task *task;
	unsigned long flags;

	
	if ((cmd->se_cmd_flags & SCF_SCSI_CONTROL_SG_IO_CDB) &&
	    !cmd->data_length)
		return 0;

	task = transport_generic_get_task(cmd, cmd->data_direction);
	if (!task)
		return -ENOMEM;

	task->task_sg = cmd->t_data_sg;
	task->task_size = cmd->data_length;
	task->task_sg_nents = cmd->t_data_nents;

	spin_lock_irqsave(&cmd->t_state_lock, flags);
	list_add_tail(&task->t_list, &cmd->t_task_list);
	spin_unlock_irqrestore(&cmd->t_state_lock, flags);

	
	return 1;
}

int transport_generic_new_cmd(struct se_cmd *cmd)
{
	struct se_device *dev = cmd->se_dev;
	int task_cdbs, task_cdbs_bidi = 0;
	int set_counts = 1;
	int ret = 0;

	if (!(cmd->se_cmd_flags & SCF_PASSTHROUGH_SG_TO_MEM_NOALLOC) &&
	    cmd->data_length) {
		ret = transport_generic_get_mem(cmd);
		if (ret < 0)
			goto out_fail;
	}

	if (cmd->t_bidi_data_sg &&
	    dev->transport->transport_type != TRANSPORT_PLUGIN_PHBA_PDEV) {
		BUG_ON(!(cmd->se_cmd_flags & SCF_SCSI_DATA_SG_IO_CDB));

		task_cdbs_bidi = transport_allocate_data_tasks(cmd,
				DMA_FROM_DEVICE, cmd->t_bidi_data_sg,
				cmd->t_bidi_data_nents);
		if (task_cdbs_bidi <= 0)
			goto out_fail;

		atomic_inc(&cmd->t_fe_count);
		atomic_inc(&cmd->t_se_count);
		set_counts = 0;
	}

	if (cmd->se_cmd_flags & SCF_SCSI_DATA_SG_IO_CDB) {
		task_cdbs = transport_allocate_data_tasks(cmd,
					cmd->data_direction, cmd->t_data_sg,
					cmd->t_data_nents);
	} else {
		task_cdbs = transport_allocate_control_task(cmd);
	}

	if (task_cdbs < 0)
		goto out_fail;
	else if (!task_cdbs && (cmd->se_cmd_flags & SCF_SCSI_DATA_SG_IO_CDB)) {
		spin_lock_irq(&cmd->t_state_lock);
		cmd->t_state = TRANSPORT_COMPLETE;
		cmd->transport_state |= CMD_T_ACTIVE;
		spin_unlock_irq(&cmd->t_state_lock);

		if (cmd->t_task_cdb[0] == REQUEST_SENSE) {
			u8 ua_asc = 0, ua_ascq = 0;

			core_scsi3_ua_clear_for_request_sense(cmd,
					&ua_asc, &ua_ascq);
		}

		INIT_WORK(&cmd->work, target_complete_ok_work);
		queue_work(target_completion_wq, &cmd->work);
		return 0;
	}

	if (set_counts) {
		atomic_inc(&cmd->t_fe_count);
		atomic_inc(&cmd->t_se_count);
	}

	cmd->t_task_list_num = (task_cdbs + task_cdbs_bidi);
	atomic_set(&cmd->t_task_cdbs_left, cmd->t_task_list_num);
	atomic_set(&cmd->t_task_cdbs_ex_left, cmd->t_task_list_num);

	if (cmd->data_direction == DMA_TO_DEVICE) {
		transport_add_tasks_to_state_queue(cmd);
		return transport_generic_write_pending(cmd);
	}
	transport_execute_tasks(cmd);
	return 0;

out_fail:
	cmd->se_cmd_flags |= SCF_SCSI_CDB_EXCEPTION;
	cmd->scsi_sense_reason = TCM_LOGICAL_UNIT_COMMUNICATION_FAILURE;
	return -EINVAL;
}
EXPORT_SYMBOL(transport_generic_new_cmd);

void transport_generic_process_write(struct se_cmd *cmd)
{
	transport_execute_tasks(cmd);
}
EXPORT_SYMBOL(transport_generic_process_write);

static void transport_write_pending_qf(struct se_cmd *cmd)
{
	int ret;

	ret = cmd->se_tfo->write_pending(cmd);
	if (ret == -EAGAIN || ret == -ENOMEM) {
		pr_debug("Handling write_pending QUEUE__FULL: se_cmd: %p\n",
			 cmd);
		transport_handle_queue_full(cmd, cmd->se_dev);
	}
}

static int transport_generic_write_pending(struct se_cmd *cmd)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&cmd->t_state_lock, flags);
	cmd->t_state = TRANSPORT_WRITE_PENDING;
	spin_unlock_irqrestore(&cmd->t_state_lock, flags);

	transport_cmd_check_stop(cmd, 1, 0);

	ret = cmd->se_tfo->write_pending(cmd);
	if (ret == -EAGAIN || ret == -ENOMEM)
		goto queue_full;
	else if (ret < 0)
		return ret;

	return 1;

queue_full:
	pr_debug("Handling write_pending QUEUE__FULL: se_cmd: %p\n", cmd);
	cmd->t_state = TRANSPORT_COMPLETE_QF_WP;
	transport_handle_queue_full(cmd, cmd->se_dev);
	return 0;
}

void transport_generic_free_cmd(struct se_cmd *cmd, int wait_for_tasks)
{
	if (!(cmd->se_cmd_flags & SCF_SE_LUN_CMD)) {
		if (wait_for_tasks && (cmd->se_cmd_flags & SCF_SCSI_TMR_CDB))
			 transport_wait_for_tasks(cmd);

		transport_release_cmd(cmd);
	} else {
		if (wait_for_tasks)
			transport_wait_for_tasks(cmd);

		core_dec_lacl_count(cmd->se_sess->se_node_acl, cmd);

		if (cmd->se_lun)
			transport_lun_remove_cmd(cmd);

		transport_free_dev_tasks(cmd);

		transport_put_cmd(cmd);
	}
}
EXPORT_SYMBOL(transport_generic_free_cmd);

void target_get_sess_cmd(struct se_session *se_sess, struct se_cmd *se_cmd,
			bool ack_kref)
{
	unsigned long flags;

	kref_init(&se_cmd->cmd_kref);
	if (ack_kref == true) {
		kref_get(&se_cmd->cmd_kref);
		se_cmd->se_cmd_flags |= SCF_ACK_KREF;
	}

	spin_lock_irqsave(&se_sess->sess_cmd_lock, flags);
	list_add_tail(&se_cmd->se_cmd_list, &se_sess->sess_cmd_list);
	se_cmd->check_release = 1;
	spin_unlock_irqrestore(&se_sess->sess_cmd_lock, flags);
}
EXPORT_SYMBOL(target_get_sess_cmd);

static void target_release_cmd_kref(struct kref *kref)
{
	struct se_cmd *se_cmd = container_of(kref, struct se_cmd, cmd_kref);
	struct se_session *se_sess = se_cmd->se_sess;
	unsigned long flags;

	spin_lock_irqsave(&se_sess->sess_cmd_lock, flags);
	if (list_empty(&se_cmd->se_cmd_list)) {
		spin_unlock_irqrestore(&se_sess->sess_cmd_lock, flags);
		se_cmd->se_tfo->release_cmd(se_cmd);
		return;
	}
	if (se_sess->sess_tearing_down && se_cmd->cmd_wait_set) {
		spin_unlock_irqrestore(&se_sess->sess_cmd_lock, flags);
		complete(&se_cmd->cmd_wait_comp);
		return;
	}
	list_del(&se_cmd->se_cmd_list);
	spin_unlock_irqrestore(&se_sess->sess_cmd_lock, flags);

	se_cmd->se_tfo->release_cmd(se_cmd);
}

int target_put_sess_cmd(struct se_session *se_sess, struct se_cmd *se_cmd)
{
	return kref_put(&se_cmd->cmd_kref, target_release_cmd_kref);
}
EXPORT_SYMBOL(target_put_sess_cmd);

void target_splice_sess_cmd_list(struct se_session *se_sess)
{
	struct se_cmd *se_cmd;
	unsigned long flags;

	WARN_ON(!list_empty(&se_sess->sess_wait_list));
	INIT_LIST_HEAD(&se_sess->sess_wait_list);

	spin_lock_irqsave(&se_sess->sess_cmd_lock, flags);
	se_sess->sess_tearing_down = 1;

	list_splice_init(&se_sess->sess_cmd_list, &se_sess->sess_wait_list);

	list_for_each_entry(se_cmd, &se_sess->sess_wait_list, se_cmd_list)
		se_cmd->cmd_wait_set = 1;

	spin_unlock_irqrestore(&se_sess->sess_cmd_lock, flags);
}
EXPORT_SYMBOL(target_splice_sess_cmd_list);

void target_wait_for_sess_cmds(
	struct se_session *se_sess,
	int wait_for_tasks)
{
	struct se_cmd *se_cmd, *tmp_cmd;
	bool rc = false;

	list_for_each_entry_safe(se_cmd, tmp_cmd,
				&se_sess->sess_wait_list, se_cmd_list) {
		list_del(&se_cmd->se_cmd_list);

		pr_debug("Waiting for se_cmd: %p t_state: %d, fabric state:"
			" %d\n", se_cmd, se_cmd->t_state,
			se_cmd->se_tfo->get_cmd_state(se_cmd));

		if (wait_for_tasks) {
			pr_debug("Calling transport_wait_for_tasks se_cmd: %p t_state: %d,"
				" fabric state: %d\n", se_cmd, se_cmd->t_state,
				se_cmd->se_tfo->get_cmd_state(se_cmd));

			rc = transport_wait_for_tasks(se_cmd);

			pr_debug("After transport_wait_for_tasks se_cmd: %p t_state: %d,"
				" fabric state: %d\n", se_cmd, se_cmd->t_state,
				se_cmd->se_tfo->get_cmd_state(se_cmd));
		}

		if (!rc) {
			wait_for_completion(&se_cmd->cmd_wait_comp);
			pr_debug("After cmd_wait_comp: se_cmd: %p t_state: %d"
				" fabric state: %d\n", se_cmd, se_cmd->t_state,
				se_cmd->se_tfo->get_cmd_state(se_cmd));
		}

		se_cmd->se_tfo->release_cmd(se_cmd);
	}
}
EXPORT_SYMBOL(target_wait_for_sess_cmds);

static int transport_lun_wait_for_tasks(struct se_cmd *cmd, struct se_lun *lun)
{
	unsigned long flags;
	int ret;
	spin_lock_irqsave(&cmd->t_state_lock, flags);
	if (cmd->transport_state & CMD_T_STOP) {
		cmd->transport_state &= ~CMD_T_LUN_STOP;

		pr_debug("ConfigFS ITT[0x%08x] - CMD_T_STOP, skipping\n",
			 cmd->se_tfo->get_task_tag(cmd));
		spin_unlock_irqrestore(&cmd->t_state_lock, flags);
		transport_cmd_check_stop(cmd, 1, 0);
		return -EPERM;
	}
	cmd->transport_state |= CMD_T_LUN_FE_STOP;
	spin_unlock_irqrestore(&cmd->t_state_lock, flags);

	wake_up_interruptible(&cmd->se_dev->dev_queue_obj.thread_wq);

	ret = transport_stop_tasks_for_cmd(cmd);

	pr_debug("ConfigFS: cmd: %p t_tasks: %d stop tasks ret:"
			" %d\n", cmd, cmd->t_task_list_num, ret);
	if (!ret) {
		pr_debug("ConfigFS: ITT[0x%08x] - stopping cmd....\n",
				cmd->se_tfo->get_task_tag(cmd));
		wait_for_completion(&cmd->transport_lun_stop_comp);
		pr_debug("ConfigFS: ITT[0x%08x] - stopped cmd....\n",
				cmd->se_tfo->get_task_tag(cmd));
	}
	transport_remove_cmd_from_queue(cmd);

	return 0;
}

static void __transport_clear_lun_from_sessions(struct se_lun *lun)
{
	struct se_cmd *cmd = NULL;
	unsigned long lun_flags, cmd_flags;
	spin_lock_irqsave(&lun->lun_cmd_lock, lun_flags);
	while (!list_empty(&lun->lun_cmd_list)) {
		cmd = list_first_entry(&lun->lun_cmd_list,
		       struct se_cmd, se_lun_node);
		list_del_init(&cmd->se_lun_node);

		spin_lock(&cmd->t_state_lock);
		pr_debug("SE_LUN[%d] - Setting cmd->transport"
			"_lun_stop for  ITT: 0x%08x\n",
			cmd->se_lun->unpacked_lun,
			cmd->se_tfo->get_task_tag(cmd));
		cmd->transport_state |= CMD_T_LUN_STOP;
		spin_unlock(&cmd->t_state_lock);

		spin_unlock_irqrestore(&lun->lun_cmd_lock, lun_flags);

		if (!cmd->se_lun) {
			pr_err("ITT: 0x%08x, [i,t]_state: %u/%u\n",
				cmd->se_tfo->get_task_tag(cmd),
				cmd->se_tfo->get_cmd_state(cmd), cmd->t_state);
			BUG();
		}
		pr_debug("SE_LUN[%d] - ITT: 0x%08x before transport"
			"_lun_wait_for_tasks()\n", cmd->se_lun->unpacked_lun,
			cmd->se_tfo->get_task_tag(cmd));

		if (transport_lun_wait_for_tasks(cmd, cmd->se_lun) < 0) {
			spin_lock_irqsave(&lun->lun_cmd_lock, lun_flags);
			continue;
		}

		pr_debug("SE_LUN[%d] - ITT: 0x%08x after transport_lun"
			"_wait_for_tasks(): SUCCESS\n",
			cmd->se_lun->unpacked_lun,
			cmd->se_tfo->get_task_tag(cmd));

		spin_lock_irqsave(&cmd->t_state_lock, cmd_flags);
		if (!(cmd->transport_state & CMD_T_DEV_ACTIVE)) {
			spin_unlock_irqrestore(&cmd->t_state_lock, cmd_flags);
			goto check_cond;
		}
		cmd->transport_state &= ~CMD_T_DEV_ACTIVE;
		transport_all_task_dev_remove_state(cmd);
		spin_unlock_irqrestore(&cmd->t_state_lock, cmd_flags);

		transport_free_dev_tasks(cmd);
check_cond:
		transport_send_check_condition_and_sense(cmd,
				TCM_NON_EXISTENT_LUN, 0);
		spin_lock_irqsave(&cmd->t_state_lock, cmd_flags);
		if (cmd->transport_state & CMD_T_LUN_FE_STOP) {
			pr_debug("SE_LUN[%d] - Detected FE stop for"
				" struct se_cmd: %p ITT: 0x%08x\n",
				lun->unpacked_lun,
				cmd, cmd->se_tfo->get_task_tag(cmd));

			spin_unlock_irqrestore(&cmd->t_state_lock,
					cmd_flags);
			transport_cmd_check_stop(cmd, 1, 0);
			complete(&cmd->transport_lun_fe_stop_comp);
			spin_lock_irqsave(&lun->lun_cmd_lock, lun_flags);
			continue;
		}
		pr_debug("SE_LUN[%d] - ITT: 0x%08x finished processing\n",
			lun->unpacked_lun, cmd->se_tfo->get_task_tag(cmd));

		spin_unlock_irqrestore(&cmd->t_state_lock, cmd_flags);
		spin_lock_irqsave(&lun->lun_cmd_lock, lun_flags);
	}
	spin_unlock_irqrestore(&lun->lun_cmd_lock, lun_flags);
}

static int transport_clear_lun_thread(void *p)
{
	struct se_lun *lun = p;

	__transport_clear_lun_from_sessions(lun);
	complete(&lun->lun_shutdown_comp);

	return 0;
}

int transport_clear_lun_from_sessions(struct se_lun *lun)
{
	struct task_struct *kt;

	kt = kthread_run(transport_clear_lun_thread, lun,
			"tcm_cl_%u", lun->unpacked_lun);
	if (IS_ERR(kt)) {
		pr_err("Unable to start clear_lun thread\n");
		return PTR_ERR(kt);
	}
	wait_for_completion(&lun->lun_shutdown_comp);

	return 0;
}

bool transport_wait_for_tasks(struct se_cmd *cmd)
{
	unsigned long flags;

	spin_lock_irqsave(&cmd->t_state_lock, flags);
	if (!(cmd->se_cmd_flags & SCF_SE_LUN_CMD) &&
	    !(cmd->se_cmd_flags & SCF_SCSI_TMR_CDB)) {
		spin_unlock_irqrestore(&cmd->t_state_lock, flags);
		return false;
	}
	if (!(cmd->se_cmd_flags & SCF_SUPPORTED_SAM_OPCODE) &&
	    !(cmd->se_cmd_flags & SCF_SCSI_TMR_CDB)) {
		spin_unlock_irqrestore(&cmd->t_state_lock, flags);
		return false;
	}
	if (cmd->transport_state & CMD_T_LUN_STOP) {
		pr_debug("wait_for_tasks: Stopping"
			" wait_for_completion(&cmd->t_tasktransport_lun_fe"
			"_stop_comp); for ITT: 0x%08x\n",
			cmd->se_tfo->get_task_tag(cmd));
		spin_unlock_irqrestore(&cmd->t_state_lock, flags);
		complete(&cmd->transport_lun_stop_comp);
		wait_for_completion(&cmd->transport_lun_fe_stop_comp);
		spin_lock_irqsave(&cmd->t_state_lock, flags);

		transport_all_task_dev_remove_state(cmd);
		pr_debug("wait_for_tasks: Stopped"
			" wait_for_completion(&cmd->t_tasktransport_lun_fe_"
			"stop_comp); for ITT: 0x%08x\n",
			cmd->se_tfo->get_task_tag(cmd));

		cmd->transport_state &= ~CMD_T_LUN_STOP;
	}

	if (!(cmd->transport_state & CMD_T_ACTIVE)) {
		spin_unlock_irqrestore(&cmd->t_state_lock, flags);
		return false;
	}

	cmd->transport_state |= CMD_T_STOP;

	pr_debug("wait_for_tasks: Stopping %p ITT: 0x%08x"
		" i_state: %d, t_state: %d, CMD_T_STOP\n",
		cmd, cmd->se_tfo->get_task_tag(cmd),
		cmd->se_tfo->get_cmd_state(cmd), cmd->t_state);

	spin_unlock_irqrestore(&cmd->t_state_lock, flags);

	wake_up_interruptible(&cmd->se_dev->dev_queue_obj.thread_wq);

	wait_for_completion(&cmd->t_transport_stop_comp);

	spin_lock_irqsave(&cmd->t_state_lock, flags);
	cmd->transport_state &= ~(CMD_T_ACTIVE | CMD_T_STOP);

	pr_debug("wait_for_tasks: Stopped wait_for_compltion("
		"&cmd->t_transport_stop_comp) for ITT: 0x%08x\n",
		cmd->se_tfo->get_task_tag(cmd));

	spin_unlock_irqrestore(&cmd->t_state_lock, flags);

	return true;
}
EXPORT_SYMBOL(transport_wait_for_tasks);

static int transport_get_sense_codes(
	struct se_cmd *cmd,
	u8 *asc,
	u8 *ascq)
{
	*asc = cmd->scsi_asc;
	*ascq = cmd->scsi_ascq;

	return 0;
}

static int transport_set_sense_codes(
	struct se_cmd *cmd,
	u8 asc,
	u8 ascq)
{
	cmd->scsi_asc = asc;
	cmd->scsi_ascq = ascq;

	return 0;
}

int transport_send_check_condition_and_sense(
	struct se_cmd *cmd,
	u8 reason,
	int from_transport)
{
	unsigned char *buffer = cmd->sense_buffer;
	unsigned long flags;
	int offset;
	u8 asc = 0, ascq = 0;

	spin_lock_irqsave(&cmd->t_state_lock, flags);
	if (cmd->se_cmd_flags & SCF_SENT_CHECK_CONDITION) {
		spin_unlock_irqrestore(&cmd->t_state_lock, flags);
		return 0;
	}
	cmd->se_cmd_flags |= SCF_SENT_CHECK_CONDITION;
	spin_unlock_irqrestore(&cmd->t_state_lock, flags);

	if (!reason && from_transport)
		goto after_reason;

	if (!from_transport)
		cmd->se_cmd_flags |= SCF_EMULATED_TASK_SENSE;
	offset = cmd->se_tfo->set_fabric_sense_len(cmd,
				TRANSPORT_SENSE_BUFFER);
	switch (reason) {
	case TCM_NON_EXISTENT_LUN:
		
		buffer[offset] = 0x70;
		buffer[offset+SPC_ADD_SENSE_LEN_OFFSET] = 10;
		
		buffer[offset+SPC_SENSE_KEY_OFFSET] = ILLEGAL_REQUEST;
		
		buffer[offset+SPC_ASC_KEY_OFFSET] = 0x25;
		break;
	case TCM_UNSUPPORTED_SCSI_OPCODE:
	case TCM_SECTOR_COUNT_TOO_MANY:
		
		buffer[offset] = 0x70;
		buffer[offset+SPC_ADD_SENSE_LEN_OFFSET] = 10;
		
		buffer[offset+SPC_SENSE_KEY_OFFSET] = ILLEGAL_REQUEST;
		
		buffer[offset+SPC_ASC_KEY_OFFSET] = 0x20;
		break;
	case TCM_UNKNOWN_MODE_PAGE:
		
		buffer[offset] = 0x70;
		buffer[offset+SPC_ADD_SENSE_LEN_OFFSET] = 10;
		
		buffer[offset+SPC_SENSE_KEY_OFFSET] = ILLEGAL_REQUEST;
		
		buffer[offset+SPC_ASC_KEY_OFFSET] = 0x24;
		break;
	case TCM_CHECK_CONDITION_ABORT_CMD:
		
		buffer[offset] = 0x70;
		buffer[offset+SPC_ADD_SENSE_LEN_OFFSET] = 10;
		
		buffer[offset+SPC_SENSE_KEY_OFFSET] = ABORTED_COMMAND;
		
		buffer[offset+SPC_ASC_KEY_OFFSET] = 0x29;
		buffer[offset+SPC_ASCQ_KEY_OFFSET] = 0x03;
		break;
	case TCM_INCORRECT_AMOUNT_OF_DATA:
		
		buffer[offset] = 0x70;
		buffer[offset+SPC_ADD_SENSE_LEN_OFFSET] = 10;
		
		buffer[offset+SPC_SENSE_KEY_OFFSET] = ABORTED_COMMAND;
		
		buffer[offset+SPC_ASC_KEY_OFFSET] = 0x0c;
		
		buffer[offset+SPC_ASCQ_KEY_OFFSET] = 0x0d;
		break;
	case TCM_INVALID_CDB_FIELD:
		
		buffer[offset] = 0x70;
		buffer[offset+SPC_ADD_SENSE_LEN_OFFSET] = 10;
		
		buffer[offset+SPC_SENSE_KEY_OFFSET] = ILLEGAL_REQUEST;
		
		buffer[offset+SPC_ASC_KEY_OFFSET] = 0x24;
		break;
	case TCM_INVALID_PARAMETER_LIST:
		
		buffer[offset] = 0x70;
		buffer[offset+SPC_ADD_SENSE_LEN_OFFSET] = 10;
		
		buffer[offset+SPC_SENSE_KEY_OFFSET] = ILLEGAL_REQUEST;
		
		buffer[offset+SPC_ASC_KEY_OFFSET] = 0x26;
		break;
	case TCM_UNEXPECTED_UNSOLICITED_DATA:
		
		buffer[offset] = 0x70;
		buffer[offset+SPC_ADD_SENSE_LEN_OFFSET] = 10;
		
		buffer[offset+SPC_SENSE_KEY_OFFSET] = ABORTED_COMMAND;
		
		buffer[offset+SPC_ASC_KEY_OFFSET] = 0x0c;
		
		buffer[offset+SPC_ASCQ_KEY_OFFSET] = 0x0c;
		break;
	case TCM_SERVICE_CRC_ERROR:
		
		buffer[offset] = 0x70;
		buffer[offset+SPC_ADD_SENSE_LEN_OFFSET] = 10;
		
		buffer[offset+SPC_SENSE_KEY_OFFSET] = ABORTED_COMMAND;
		
		buffer[offset+SPC_ASC_KEY_OFFSET] = 0x47;
		
		buffer[offset+SPC_ASCQ_KEY_OFFSET] = 0x05;
		break;
	case TCM_SNACK_REJECTED:
		
		buffer[offset] = 0x70;
		buffer[offset+SPC_ADD_SENSE_LEN_OFFSET] = 10;
		
		buffer[offset+SPC_SENSE_KEY_OFFSET] = ABORTED_COMMAND;
		
		buffer[offset+SPC_ASC_KEY_OFFSET] = 0x11;
		
		buffer[offset+SPC_ASCQ_KEY_OFFSET] = 0x13;
		break;
	case TCM_WRITE_PROTECTED:
		
		buffer[offset] = 0x70;
		buffer[offset+SPC_ADD_SENSE_LEN_OFFSET] = 10;
		
		buffer[offset+SPC_SENSE_KEY_OFFSET] = DATA_PROTECT;
		
		buffer[offset+SPC_ASC_KEY_OFFSET] = 0x27;
		break;
	case TCM_CHECK_CONDITION_UNIT_ATTENTION:
		
		buffer[offset] = 0x70;
		buffer[offset+SPC_ADD_SENSE_LEN_OFFSET] = 10;
		
		buffer[offset+SPC_SENSE_KEY_OFFSET] = UNIT_ATTENTION;
		core_scsi3_ua_for_check_condition(cmd, &asc, &ascq);
		buffer[offset+SPC_ASC_KEY_OFFSET] = asc;
		buffer[offset+SPC_ASCQ_KEY_OFFSET] = ascq;
		break;
	case TCM_CHECK_CONDITION_NOT_READY:
		
		buffer[offset] = 0x70;
		buffer[offset+SPC_ADD_SENSE_LEN_OFFSET] = 10;
		
		buffer[offset+SPC_SENSE_KEY_OFFSET] = NOT_READY;
		transport_get_sense_codes(cmd, &asc, &ascq);
		buffer[offset+SPC_ASC_KEY_OFFSET] = asc;
		buffer[offset+SPC_ASCQ_KEY_OFFSET] = ascq;
		break;
	case TCM_LOGICAL_UNIT_COMMUNICATION_FAILURE:
	default:
		
		buffer[offset] = 0x70;
		buffer[offset+SPC_ADD_SENSE_LEN_OFFSET] = 10;
		
		buffer[offset+SPC_SENSE_KEY_OFFSET] = ILLEGAL_REQUEST;
		
		buffer[offset+SPC_ASC_KEY_OFFSET] = 0x80;
		break;
	}
	cmd->scsi_status = SAM_STAT_CHECK_CONDITION;
	cmd->scsi_sense_length  = TRANSPORT_SENSE_BUFFER + offset;

after_reason:
	return cmd->se_tfo->queue_status(cmd);
}
EXPORT_SYMBOL(transport_send_check_condition_and_sense);

int transport_check_aborted_status(struct se_cmd *cmd, int send_status)
{
	int ret = 0;

	if (cmd->transport_state & CMD_T_ABORTED) {
		if (!send_status ||
		     (cmd->se_cmd_flags & SCF_SENT_DELAYED_TAS))
			return 1;
#if 0
		pr_debug("Sending delayed SAM_STAT_TASK_ABORTED"
			" status for CDB: 0x%02x ITT: 0x%08x\n",
			cmd->t_task_cdb[0],
			cmd->se_tfo->get_task_tag(cmd));
#endif
		cmd->se_cmd_flags |= SCF_SENT_DELAYED_TAS;
		cmd->se_tfo->queue_status(cmd);
		ret = 1;
	}
	return ret;
}
EXPORT_SYMBOL(transport_check_aborted_status);

void transport_send_task_abort(struct se_cmd *cmd)
{
	unsigned long flags;

	spin_lock_irqsave(&cmd->t_state_lock, flags);
	if (cmd->se_cmd_flags & SCF_SENT_CHECK_CONDITION) {
		spin_unlock_irqrestore(&cmd->t_state_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&cmd->t_state_lock, flags);

	if (cmd->data_direction == DMA_TO_DEVICE) {
		if (cmd->se_tfo->write_pending_status(cmd) != 0) {
			cmd->transport_state |= CMD_T_ABORTED;
			smp_mb__after_atomic_inc();
		}
	}
	cmd->scsi_status = SAM_STAT_TASK_ABORTED;
#if 0
	pr_debug("Setting SAM_STAT_TASK_ABORTED status for CDB: 0x%02x,"
		" ITT: 0x%08x\n", cmd->t_task_cdb[0],
		cmd->se_tfo->get_task_tag(cmd));
#endif
	cmd->se_tfo->queue_status(cmd);
}

static int transport_generic_do_tmr(struct se_cmd *cmd)
{
	struct se_device *dev = cmd->se_dev;
	struct se_tmr_req *tmr = cmd->se_tmr_req;
	int ret;

	switch (tmr->function) {
	case TMR_ABORT_TASK:
		core_tmr_abort_task(dev, tmr, cmd->se_sess);
		break;
	case TMR_ABORT_TASK_SET:
	case TMR_CLEAR_ACA:
	case TMR_CLEAR_TASK_SET:
		tmr->response = TMR_TASK_MGMT_FUNCTION_NOT_SUPPORTED;
		break;
	case TMR_LUN_RESET:
		ret = core_tmr_lun_reset(dev, tmr, NULL, NULL);
		tmr->response = (!ret) ? TMR_FUNCTION_COMPLETE :
					 TMR_FUNCTION_REJECTED;
		break;
	case TMR_TARGET_WARM_RESET:
		tmr->response = TMR_FUNCTION_REJECTED;
		break;
	case TMR_TARGET_COLD_RESET:
		tmr->response = TMR_FUNCTION_REJECTED;
		break;
	default:
		pr_err("Uknown TMR function: 0x%02x.\n",
				tmr->function);
		tmr->response = TMR_FUNCTION_REJECTED;
		break;
	}

	cmd->t_state = TRANSPORT_ISTATE_PROCESSING;
	cmd->se_tfo->queue_tm_rsp(cmd);

	transport_cmd_check_stop_to_fabric(cmd);
	return 0;
}

static int transport_processing_thread(void *param)
{
	int ret;
	struct se_cmd *cmd;
	struct se_device *dev = param;

	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(dev->dev_queue_obj.thread_wq,
				atomic_read(&dev->dev_queue_obj.queue_cnt) ||
				kthread_should_stop());
		if (ret < 0)
			goto out;

get_cmd:
		cmd = transport_get_cmd_from_queue(&dev->dev_queue_obj);
		if (!cmd)
			continue;

		switch (cmd->t_state) {
		case TRANSPORT_NEW_CMD:
			BUG();
			break;
		case TRANSPORT_NEW_CMD_MAP:
			if (!cmd->se_tfo->new_cmd_map) {
				pr_err("cmd->se_tfo->new_cmd_map is"
					" NULL for TRANSPORT_NEW_CMD_MAP\n");
				BUG();
			}
			ret = cmd->se_tfo->new_cmd_map(cmd);
			if (ret < 0) {
				transport_generic_request_failure(cmd);
				break;
			}
			ret = transport_generic_new_cmd(cmd);
			if (ret < 0) {
				transport_generic_request_failure(cmd);
				break;
			}
			break;
		case TRANSPORT_PROCESS_WRITE:
			transport_generic_process_write(cmd);
			break;
		case TRANSPORT_PROCESS_TMR:
			transport_generic_do_tmr(cmd);
			break;
		case TRANSPORT_COMPLETE_QF_WP:
			transport_write_pending_qf(cmd);
			break;
		case TRANSPORT_COMPLETE_QF_OK:
			transport_complete_qf(cmd);
			break;
		default:
			pr_err("Unknown t_state: %d  for ITT: 0x%08x "
				"i_state: %d on SE LUN: %u\n",
				cmd->t_state,
				cmd->se_tfo->get_task_tag(cmd),
				cmd->se_tfo->get_cmd_state(cmd),
				cmd->se_lun->unpacked_lun);
			BUG();
		}

		goto get_cmd;
	}

out:
	WARN_ON(!list_empty(&dev->state_task_list));
	WARN_ON(!list_empty(&dev->dev_queue_obj.qobj_list));
	dev->process_thread = NULL;
	return 0;
}
