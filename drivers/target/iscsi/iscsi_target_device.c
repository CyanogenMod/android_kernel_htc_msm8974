/*******************************************************************************
 * This file contains the iSCSI Virtual Device and Disk Transport
 * agnostic related functions.
 *
 \u00a9 Copyright 2007-2011 RisingTide Systems LLC.
 *
 * Licensed to the Linux Foundation under the General Public License (GPL) version 2.
 *
 * Author: Nicholas A. Bellinger <nab@linux-iscsi.org>
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
 ******************************************************************************/

#include <scsi/scsi_device.h>
#include <target/target_core_base.h>
#include <target/target_core_fabric.h>

#include "iscsi_target_core.h"
#include "iscsi_target_device.h"
#include "iscsi_target_tpg.h"
#include "iscsi_target_util.h"

void iscsit_determine_maxcmdsn(struct iscsi_session *sess)
{
	struct se_node_acl *se_nacl;

	if (sess->sess_ops->SessionType)
		return;

	se_nacl = sess->se_sess->se_node_acl;

	sess->cmdsn_window = se_nacl->queue_depth;
	sess->max_cmd_sn = (sess->max_cmd_sn + se_nacl->queue_depth) - 1;
}

void iscsit_increment_maxcmdsn(struct iscsi_cmd *cmd, struct iscsi_session *sess)
{
	if (cmd->immediate_cmd || cmd->maxcmdsn_inc)
		return;

	cmd->maxcmdsn_inc = 1;

	mutex_lock(&sess->cmdsn_mutex);
	sess->max_cmd_sn += 1;
	pr_debug("Updated MaxCmdSN to 0x%08x\n", sess->max_cmd_sn);
	mutex_unlock(&sess->cmdsn_mutex);
}
