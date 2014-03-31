/*
 * Universal Flash Storage Host controller driver Core
 *
 * This code is based on drivers/scsi/ufs/ufshcd.c
 * Copyright (C) 2011-2013 Samsung India Software Operations
 *
 * Authors:
 *	Santosh Yaraganavi <santosh.sy@samsung.com>
 *	Vinayak Holikatti <h.vinayak@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * See the COPYING file in the top-level directory or visit
 * <http://www.gnu.org/licenses/gpl-2.0.html>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program is provided "AS IS" and "WITH ALL FAULTS" and
 * without warranty of any kind. You are solely responsible for
 * determining the appropriateness of using and distributing
 * the program and assume all risks associated with your exercise
 * of rights with respect to the program, including but not limited
 * to infringement of third party rights, the risks and costs of
 * program errors, damage to or loss of data, programs or equipment,
 * and unavailability or interruption of operations. Under no
 * circumstances will the contributor of this Program be liable for
 * any damages of any kind arising from your use or distribution of
 * this program.
 */

#include <linux/async.h>

#include "ufshcd.h"

#define UFSHCD_ENABLE_INTRS	(UTP_TRANSFER_REQ_COMPL |\
				 UTP_TASK_REQ_COMPL |\
				 UFSHCD_ERROR_MASK)
#define UIC_CMD_TIMEOUT	500

enum {
	UFSHCD_MAX_CHANNEL	= 0,
	UFSHCD_MAX_ID		= 1,
	UFSHCD_MAX_LUNS		= 8,
	UFSHCD_CMD_PER_LUN	= 32,
	UFSHCD_CAN_QUEUE	= 32,
};

enum {
	UFSHCD_STATE_OPERATIONAL,
	UFSHCD_STATE_RESET,
	UFSHCD_STATE_ERROR,
};

enum {
	UFSHCD_INT_DISABLE,
	UFSHCD_INT_ENABLE,
	UFSHCD_INT_CLEAR,
};

enum {
	INT_AGGR_RESET,
	INT_AGGR_CONFIG,
};

static inline u32 ufshcd_get_intr_mask(struct ufs_hba *hba)
{
	if (hba->ufs_version == UFSHCI_VERSION_10)
		return INTERRUPT_MASK_ALL_VER_10;
	else
		return INTERRUPT_MASK_ALL_VER_11;
}

static inline u32 ufshcd_get_ufs_version(struct ufs_hba *hba)
{
	return ufshcd_readl(hba, REG_UFS_VERSION);
}

static inline int ufshcd_is_device_present(u32 reg_hcs)
{
	return (DEVICE_PRESENT & reg_hcs) ? 1 : 0;
}

static inline int ufshcd_get_tr_ocs(struct ufshcd_lrb *lrbp)
{
	return lrbp->utr_descriptor_ptr->header.dword_2 & MASK_OCS;
}

static inline int
ufshcd_get_tmr_ocs(struct utp_task_req_desc *task_req_descp)
{
	return task_req_descp->header.dword_2 & MASK_OCS;
}

static inline int ufshcd_get_tm_free_slot(struct ufs_hba *hba)
{
	return find_first_zero_bit(&hba->outstanding_tasks, hba->nutmrs);
}

static inline void ufshcd_utrl_clear(struct ufs_hba *hba, u32 pos)
{
	ufshcd_writel(hba, ~(1 << pos), REG_UTP_TRANSFER_REQ_LIST_CLEAR);
}

static inline int ufshcd_get_lists_status(u32 reg)
{
	return (((reg) & (0xFF)) >> 1) ^ (0x07);
}

static inline int ufshcd_get_uic_cmd_result(struct ufs_hba *hba)
{
	return ufshcd_readl(hba, REG_UIC_COMMAND_ARG_2) &
	       MASK_UIC_COMMAND_RESULT;
}

static inline int
ufshcd_is_valid_req_rsp(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	return ((be32_to_cpu(ucd_rsp_ptr->header.dword_0) >> 24) ==
		 UPIU_TRANSACTION_RESPONSE) ? 0 : DID_ERROR << 16;
}

static inline int
ufshcd_get_rsp_upiu_result(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	return be32_to_cpu(ucd_rsp_ptr->header.dword_1) & MASK_RSP_UPIU_RESULT;
}

static inline void
ufshcd_config_int_aggr(struct ufs_hba *hba, int option)
{
	switch (option) {
	case INT_AGGR_RESET:
		ufshcd_writel(hba, INT_AGGR_ENABLE |
			      INT_AGGR_COUNTER_AND_TIMER_RESET,
			      REG_UTP_TRANSFER_REQ_INT_AGG_CONTROL);
		break;
	case INT_AGGR_CONFIG:
		ufshcd_writel(hba, INT_AGGR_ENABLE | INT_AGGR_PARAM_WRITE |
			      INT_AGGR_COUNTER_THRESHOLD_VALUE |
			      INT_AGGR_TIMEOUT_VALUE,
			      REG_UTP_TRANSFER_REQ_INT_AGG_CONTROL);
		break;
	}
}

static void ufshcd_enable_run_stop_reg(struct ufs_hba *hba)
{
	ufshcd_writel(hba, UTP_TASK_REQ_LIST_RUN_STOP_BIT,
		      REG_UTP_TASK_REQ_LIST_RUN_STOP);
	ufshcd_writel(hba, UTP_TRANSFER_REQ_LIST_RUN_STOP_BIT,
		      REG_UTP_TRANSFER_REQ_LIST_RUN_STOP);
}

static inline void ufshcd_hba_start(struct ufs_hba *hba)
{
	ufshcd_writel(hba, CONTROLLER_ENABLE, REG_CONTROLLER_ENABLE);
}

static inline int ufshcd_is_hba_active(struct ufs_hba *hba)
{
	return (ufshcd_readl(hba, REG_CONTROLLER_ENABLE) & 0x1) ? 0 : 1;
}

static inline
void ufshcd_send_command(struct ufs_hba *hba, unsigned int task_tag)
{
	__set_bit(task_tag, &hba->outstanding_reqs);
	ufshcd_writel(hba, 1 << task_tag, REG_UTP_TRANSFER_REQ_DOOR_BELL);
}

static inline void ufshcd_copy_sense_data(struct ufshcd_lrb *lrbp)
{
	int len;
	if (lrbp->sense_buffer) {
		len = be16_to_cpu(lrbp->ucd_rsp_ptr->sense_data_len);
		memcpy(lrbp->sense_buffer,
			lrbp->ucd_rsp_ptr->sense_data,
			min_t(int, len, SCSI_SENSE_BUFFERSIZE));
	}
}

static inline void ufshcd_hba_capabilities(struct ufs_hba *hba)
{
	hba->capabilities = ufshcd_readl(hba, REG_CONTROLLER_CAPABILITIES);

	
	hba->nutrs = (hba->capabilities & MASK_TRANSFER_REQUESTS_SLOTS) + 1;
	hba->nutmrs =
	((hba->capabilities & MASK_TASK_MANAGEMENT_REQUEST_SLOTS) >> 16) + 1;
}

static inline bool ufshcd_ready_for_uic_cmd(struct ufs_hba *hba)
{
	if (ufshcd_readl(hba, REG_CONTROLLER_STATUS) & UIC_COMMAND_READY)
		return true;
	else
		return false;
}

static inline void
ufshcd_dispatch_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd)
{
	WARN_ON(hba->active_uic_cmd);

	hba->active_uic_cmd = uic_cmd;

	
	ufshcd_writel(hba, uic_cmd->argument1, REG_UIC_COMMAND_ARG_1);
	ufshcd_writel(hba, uic_cmd->argument2, REG_UIC_COMMAND_ARG_2);
	ufshcd_writel(hba, uic_cmd->argument3, REG_UIC_COMMAND_ARG_3);

	
	ufshcd_writel(hba, uic_cmd->command & COMMAND_OPCODE_MASK,
		      REG_UIC_COMMAND);
}

static int
ufshcd_wait_for_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd)
{
	int ret;
	unsigned long flags;

	if (wait_for_completion_timeout(&uic_cmd->done,
					msecs_to_jiffies(UIC_CMD_TIMEOUT)))
		ret = uic_cmd->argument2 & MASK_UIC_COMMAND_RESULT;
	else
		ret = -ETIMEDOUT;

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->active_uic_cmd = NULL;
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return ret;
}

static int
__ufshcd_send_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd)
{
	int ret;
	unsigned long flags;

	if (!ufshcd_ready_for_uic_cmd(hba)) {
		dev_err(hba->dev,
			"Controller not ready to accept UIC commands\n");
		return -EIO;
	}

	init_completion(&uic_cmd->done);

	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_dispatch_uic_cmd(hba, uic_cmd);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	ret = ufshcd_wait_for_uic_cmd(hba, uic_cmd);

	return ret;
}

static int
ufshcd_send_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd)
{
	int ret;

	mutex_lock(&hba->uic_cmd_mutex);
	ret = __ufshcd_send_uic_cmd(hba, uic_cmd);
	mutex_unlock(&hba->uic_cmd_mutex);

	return ret;
}

static int ufshcd_map_sg(struct ufshcd_lrb *lrbp)
{
	struct ufshcd_sg_entry *prd_table;
	struct scatterlist *sg;
	struct scsi_cmnd *cmd;
	int sg_segments;
	int i;

	cmd = lrbp->cmd;
	sg_segments = scsi_dma_map(cmd);
	if (sg_segments < 0)
		return sg_segments;

	if (sg_segments) {
		lrbp->utr_descriptor_ptr->prd_table_length =
					cpu_to_le16((u16) (sg_segments));

		prd_table = (struct ufshcd_sg_entry *)lrbp->ucd_prdt_ptr;

		scsi_for_each_sg(cmd, sg, sg_segments, i) {
			prd_table[i].size  =
				cpu_to_le32(((u32) sg_dma_len(sg))-1);
			prd_table[i].base_addr =
				cpu_to_le32(lower_32_bits(sg->dma_address));
			prd_table[i].upper_addr =
				cpu_to_le32(upper_32_bits(sg->dma_address));
		}
	} else {
		lrbp->utr_descriptor_ptr->prd_table_length = 0;
	}

	return 0;
}

static void ufshcd_enable_intr(struct ufs_hba *hba, u32 intrs)
{
	u32 set = ufshcd_readl(hba, REG_INTERRUPT_ENABLE);

	if (hba->ufs_version == UFSHCI_VERSION_10) {
		u32 rw;
		rw = set & INTERRUPT_MASK_RW_VER_10;
		set = rw | ((set ^ intrs) & intrs);
	} else {
		set |= intrs;
	}

	ufshcd_writel(hba, set, REG_INTERRUPT_ENABLE);
}

static void ufshcd_disable_intr(struct ufs_hba *hba, u32 intrs)
{
	u32 set = ufshcd_readl(hba, REG_INTERRUPT_ENABLE);

	if (hba->ufs_version == UFSHCI_VERSION_10) {
		u32 rw;
		rw = (set & INTERRUPT_MASK_RW_VER_10) &
			~(intrs & INTERRUPT_MASK_RW_VER_10);
		set = rw | ((set & intrs) & ~INTERRUPT_MASK_RW_VER_10);

	} else {
		set &= ~intrs;
	}

	ufshcd_writel(hba, set, REG_INTERRUPT_ENABLE);
}

static void ufshcd_compose_upiu(struct ufshcd_lrb *lrbp)
{
	struct utp_transfer_req_desc *req_desc;
	struct utp_upiu_cmd *ucd_cmd_ptr;
	u32 data_direction;
	u32 upiu_flags;

	ucd_cmd_ptr = lrbp->ucd_cmd_ptr;
	req_desc = lrbp->utr_descriptor_ptr;

	switch (lrbp->command_type) {
	case UTP_CMD_TYPE_SCSI:
		if (lrbp->cmd->sc_data_direction == DMA_FROM_DEVICE) {
			data_direction = UTP_DEVICE_TO_HOST;
			upiu_flags = UPIU_CMD_FLAGS_READ;
		} else if (lrbp->cmd->sc_data_direction == DMA_TO_DEVICE) {
			data_direction = UTP_HOST_TO_DEVICE;
			upiu_flags = UPIU_CMD_FLAGS_WRITE;
		} else {
			data_direction = UTP_NO_DATA_TRANSFER;
			upiu_flags = UPIU_CMD_FLAGS_NONE;
		}

		
		req_desc->header.dword_0 =
			cpu_to_le32(data_direction | UTP_SCSI_COMMAND);

		req_desc->header.dword_2 =
			cpu_to_le32(OCS_INVALID_COMMAND_STATUS);

		
		ucd_cmd_ptr->header.dword_0 =
			cpu_to_be32(UPIU_HEADER_DWORD(UPIU_TRANSACTION_COMMAND,
						      upiu_flags,
						      lrbp->lun,
						      lrbp->task_tag));
		ucd_cmd_ptr->header.dword_1 =
			cpu_to_be32(
				UPIU_HEADER_DWORD(UPIU_COMMAND_SET_TYPE_SCSI,
						  0,
						  0,
						  0));

		
		ucd_cmd_ptr->header.dword_2 = 0;

		ucd_cmd_ptr->exp_data_transfer_len =
			cpu_to_be32(lrbp->cmd->sdb.length);

		memcpy(ucd_cmd_ptr->cdb,
		       lrbp->cmd->cmnd,
		       (min_t(unsigned short,
			      lrbp->cmd->cmd_len,
			      MAX_CDB_SIZE)));
		break;
	case UTP_CMD_TYPE_DEV_MANAGE:
		
		break;
	case UTP_CMD_TYPE_UFS:
		
		break;
	} 
}

static int ufshcd_queuecommand(struct Scsi_Host *host, struct scsi_cmnd *cmd)
{
	struct ufshcd_lrb *lrbp;
	struct ufs_hba *hba;
	unsigned long flags;
	int tag;
	int err = 0;

	hba = shost_priv(host);

	tag = cmd->request->tag;

	if (hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL) {
		err = SCSI_MLQUEUE_HOST_BUSY;
		goto out;
	}

	lrbp = &hba->lrb[tag];

	lrbp->cmd = cmd;
	lrbp->sense_bufflen = SCSI_SENSE_BUFFERSIZE;
	lrbp->sense_buffer = cmd->sense_buffer;
	lrbp->task_tag = tag;
	lrbp->lun = cmd->device->lun;

	lrbp->command_type = UTP_CMD_TYPE_SCSI;

	
	ufshcd_compose_upiu(lrbp);
	err = ufshcd_map_sg(lrbp);
	if (err)
		goto out;

	
	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_send_command(hba, tag);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
out:
	return err;
}

static int ufshcd_memory_alloc(struct ufs_hba *hba)
{
	size_t utmrdl_size, utrdl_size, ucdl_size;

	
	ucdl_size = (sizeof(struct utp_transfer_cmd_desc) * hba->nutrs);
	hba->ucdl_base_addr = dmam_alloc_coherent(hba->dev,
						  ucdl_size,
						  &hba->ucdl_dma_addr,
						  GFP_KERNEL);

	if (!hba->ucdl_base_addr ||
	    WARN_ON(hba->ucdl_dma_addr & (PAGE_SIZE - 1))) {
		dev_err(hba->dev,
			"Command Descriptor Memory allocation failed\n");
		goto out;
	}

	utrdl_size = (sizeof(struct utp_transfer_req_desc) * hba->nutrs);
	hba->utrdl_base_addr = dmam_alloc_coherent(hba->dev,
						   utrdl_size,
						   &hba->utrdl_dma_addr,
						   GFP_KERNEL);
	if (!hba->utrdl_base_addr ||
	    WARN_ON(hba->utrdl_dma_addr & (PAGE_SIZE - 1))) {
		dev_err(hba->dev,
			"Transfer Descriptor Memory allocation failed\n");
		goto out;
	}

	utmrdl_size = sizeof(struct utp_task_req_desc) * hba->nutmrs;
	hba->utmrdl_base_addr = dmam_alloc_coherent(hba->dev,
						    utmrdl_size,
						    &hba->utmrdl_dma_addr,
						    GFP_KERNEL);
	if (!hba->utmrdl_base_addr ||
	    WARN_ON(hba->utmrdl_dma_addr & (PAGE_SIZE - 1))) {
		dev_err(hba->dev,
		"Task Management Descriptor Memory allocation failed\n");
		goto out;
	}

	
	hba->lrb = devm_kzalloc(hba->dev,
				hba->nutrs * sizeof(struct ufshcd_lrb),
				GFP_KERNEL);
	if (!hba->lrb) {
		dev_err(hba->dev, "LRB Memory allocation failed\n");
		goto out;
	}
	return 0;
out:
	return -ENOMEM;
}

static void ufshcd_host_memory_configure(struct ufs_hba *hba)
{
	struct utp_transfer_cmd_desc *cmd_descp;
	struct utp_transfer_req_desc *utrdlp;
	dma_addr_t cmd_desc_dma_addr;
	dma_addr_t cmd_desc_element_addr;
	u16 response_offset;
	u16 prdt_offset;
	int cmd_desc_size;
	int i;

	utrdlp = hba->utrdl_base_addr;
	cmd_descp = hba->ucdl_base_addr;

	response_offset =
		offsetof(struct utp_transfer_cmd_desc, response_upiu);
	prdt_offset =
		offsetof(struct utp_transfer_cmd_desc, prd_table);

	cmd_desc_size = sizeof(struct utp_transfer_cmd_desc);
	cmd_desc_dma_addr = hba->ucdl_dma_addr;

	for (i = 0; i < hba->nutrs; i++) {
		
		cmd_desc_element_addr =
				(cmd_desc_dma_addr + (cmd_desc_size * i));
		utrdlp[i].command_desc_base_addr_lo =
				cpu_to_le32(lower_32_bits(cmd_desc_element_addr));
		utrdlp[i].command_desc_base_addr_hi =
				cpu_to_le32(upper_32_bits(cmd_desc_element_addr));

		
		utrdlp[i].response_upiu_offset =
				cpu_to_le16((response_offset >> 2));
		utrdlp[i].prd_table_offset =
				cpu_to_le16((prdt_offset >> 2));
		utrdlp[i].response_upiu_length =
				cpu_to_le16(ALIGNED_UPIU_SIZE >> 2);

		hba->lrb[i].utr_descriptor_ptr = (utrdlp + i);
		hba->lrb[i].ucd_cmd_ptr =
			(struct utp_upiu_cmd *)(cmd_descp + i);
		hba->lrb[i].ucd_rsp_ptr =
			(struct utp_upiu_rsp *)cmd_descp[i].response_upiu;
		hba->lrb[i].ucd_prdt_ptr =
			(struct ufshcd_sg_entry *)cmd_descp[i].prd_table;
	}
}

static int ufshcd_dme_link_startup(struct ufs_hba *hba)
{
	struct uic_command uic_cmd = {0};
	int ret;

	uic_cmd.command = UIC_CMD_DME_LINK_STARTUP;

	ret = ufshcd_send_uic_cmd(hba, &uic_cmd);
	if (ret)
		dev_err(hba->dev,
			"dme-link-startup: error code %d\n", ret);
	return ret;
}

static int ufshcd_make_hba_operational(struct ufs_hba *hba)
{
	int err = 0;
	u32 reg;

	
	reg = ufshcd_readl(hba, REG_CONTROLLER_STATUS);
	if (!ufshcd_is_device_present(reg)) {
		dev_err(hba->dev, "cc: Device not present\n");
		err = -ENXIO;
		goto out;
	}

	
	ufshcd_enable_intr(hba, UFSHCD_ENABLE_INTRS);

	
	ufshcd_config_int_aggr(hba, INT_AGGR_CONFIG);

	
	ufshcd_writel(hba, lower_32_bits(hba->utrdl_dma_addr),
			REG_UTP_TRANSFER_REQ_LIST_BASE_L);
	ufshcd_writel(hba, upper_32_bits(hba->utrdl_dma_addr),
			REG_UTP_TRANSFER_REQ_LIST_BASE_H);
	ufshcd_writel(hba, lower_32_bits(hba->utmrdl_dma_addr),
			REG_UTP_TASK_REQ_LIST_BASE_L);
	ufshcd_writel(hba, upper_32_bits(hba->utmrdl_dma_addr),
			REG_UTP_TASK_REQ_LIST_BASE_H);

	if (!(ufshcd_get_lists_status(reg))) {
		ufshcd_enable_run_stop_reg(hba);
	} else {
		dev_err(hba->dev,
			"Host controller not ready to process requests");
		err = -EIO;
		goto out;
	}

	if (hba->ufshcd_state == UFSHCD_STATE_RESET)
		scsi_unblock_requests(hba->host);

	hba->ufshcd_state = UFSHCD_STATE_OPERATIONAL;

out:
	return err;
}

static int ufshcd_hba_enable(struct ufs_hba *hba)
{
	int retry;

	if (!ufshcd_is_hba_active(hba)) {

		
		ufshcd_hba_stop(hba);

		msleep(5);
	}

	
	ufshcd_hba_start(hba);

	msleep(1);

	
	retry = 10;
	while (ufshcd_is_hba_active(hba)) {
		if (retry) {
			retry--;
		} else {
			dev_err(hba->dev,
				"Controller enable failed\n");
			return -EIO;
		}
		msleep(5);
	}
	return 0;
}

static int ufshcd_link_startup(struct ufs_hba *hba)
{
	int ret;

	
	ufshcd_enable_intr(hba, UIC_COMMAND_COMPL);

	ret = ufshcd_dme_link_startup(hba);
	if (ret)
		goto out;

	ret = ufshcd_make_hba_operational(hba);

out:
	if (ret)
		dev_err(hba->dev, "link startup failed %d\n", ret);
	return ret;
}

static int ufshcd_do_reset(struct ufs_hba *hba)
{
	struct ufshcd_lrb *lrbp;
	unsigned long flags;
	int tag;

	
	scsi_block_requests(hba->host);

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->ufshcd_state = UFSHCD_STATE_RESET;

	
	ufshcd_hba_stop(hba);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	
	for (tag = 0; tag < hba->nutrs; tag++) {
		if (test_bit(tag, &hba->outstanding_reqs)) {
			lrbp = &hba->lrb[tag];
			scsi_dma_unmap(lrbp->cmd);
			lrbp->cmd->result = DID_RESET << 16;
			lrbp->cmd->scsi_done(lrbp->cmd);
			lrbp->cmd = NULL;
		}
	}

	
	hba->outstanding_reqs = 0;
	hba->outstanding_tasks = 0;

	
	if (ufshcd_hba_enable(hba)) {
		dev_err(hba->dev,
			"Reset: Controller initialization failed\n");
		return FAILED;
	}

	if (ufshcd_link_startup(hba)) {
		dev_err(hba->dev,
			"Reset: Link start-up failed\n");
		return FAILED;
	}

	return SUCCESS;
}

static int ufshcd_slave_alloc(struct scsi_device *sdev)
{
	struct ufs_hba *hba;

	hba = shost_priv(sdev->host);
	sdev->tagged_supported = 1;

	
	sdev->use_10_for_ms = 1;
	scsi_set_tag_type(sdev, MSG_SIMPLE_TAG);

	scsi_activate_tcq(sdev, hba->nutrs);
	return 0;
}

static void ufshcd_slave_destroy(struct scsi_device *sdev)
{
	struct ufs_hba *hba;

	hba = shost_priv(sdev->host);
	scsi_deactivate_tcq(sdev, hba->nutrs);
}

static int ufshcd_task_req_compl(struct ufs_hba *hba, u32 index)
{
	struct utp_task_req_desc *task_req_descp;
	struct utp_upiu_task_rsp *task_rsp_upiup;
	unsigned long flags;
	int ocs_value;
	int task_result;

	spin_lock_irqsave(hba->host->host_lock, flags);

	
	__clear_bit(index, &hba->outstanding_tasks);

	task_req_descp = hba->utmrdl_base_addr;
	ocs_value = ufshcd_get_tmr_ocs(&task_req_descp[index]);

	if (ocs_value == OCS_SUCCESS) {
		task_rsp_upiup = (struct utp_upiu_task_rsp *)
				task_req_descp[index].task_rsp_upiu;
		task_result = be32_to_cpu(task_rsp_upiup->header.dword_1);
		task_result = ((task_result & MASK_TASK_RESPONSE) >> 8);

		if (task_result != UPIU_TASK_MANAGEMENT_FUNC_COMPL &&
		    task_result != UPIU_TASK_MANAGEMENT_FUNC_SUCCEEDED)
			task_result = FAILED;
		else
			task_result = SUCCESS;
	} else {
		task_result = FAILED;
		dev_err(hba->dev,
			"trc: Invalid ocs = %x\n", ocs_value);
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return task_result;
}

static void ufshcd_adjust_lun_qdepth(struct scsi_cmnd *cmd)
{
	struct ufs_hba *hba;
	int i;
	int lun_qdepth = 0;

	hba = shost_priv(cmd->device->host);

	for (i = 0; i < hba->nutrs; i++) {
		if (test_bit(i, &hba->outstanding_reqs)) {

			if (cmd->device->lun == hba->lrb[i].lun)
				lun_qdepth++;
		}
	}

	scsi_adjust_queue_depth(cmd->device, MSG_SIMPLE_TAG, lun_qdepth - 1);
}

static inline int
ufshcd_scsi_cmd_status(struct ufshcd_lrb *lrbp, int scsi_status)
{
	int result = 0;

	switch (scsi_status) {
	case SAM_STAT_GOOD:
		result |= DID_OK << 16 |
			  COMMAND_COMPLETE << 8 |
			  SAM_STAT_GOOD;
		break;
	case SAM_STAT_CHECK_CONDITION:
		result |= DID_OK << 16 |
			  COMMAND_COMPLETE << 8 |
			  SAM_STAT_CHECK_CONDITION;
		ufshcd_copy_sense_data(lrbp);
		break;
	case SAM_STAT_BUSY:
		result |= SAM_STAT_BUSY;
		break;
	case SAM_STAT_TASK_SET_FULL:

		ufshcd_adjust_lun_qdepth(lrbp->cmd);
		result |= SAM_STAT_TASK_SET_FULL;
		break;
	case SAM_STAT_TASK_ABORTED:
		result |= SAM_STAT_TASK_ABORTED;
		break;
	default:
		result |= DID_ERROR << 16;
		break;
	} 

	return result;
}

static inline int
ufshcd_transfer_rsp_status(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	int result = 0;
	int scsi_status;
	int ocs;

	
	ocs = ufshcd_get_tr_ocs(lrbp);

	switch (ocs) {
	case OCS_SUCCESS:

		
		result = ufshcd_is_valid_req_rsp(lrbp->ucd_rsp_ptr);
		if (result) {
			dev_err(hba->dev,
				"Invalid response = %x\n", result);
			break;
		}

		result = ufshcd_get_rsp_upiu_result(lrbp->ucd_rsp_ptr);

		scsi_status = result & MASK_SCSI_STATUS;
		result = ufshcd_scsi_cmd_status(lrbp, scsi_status);
		break;
	case OCS_ABORTED:
		result |= DID_ABORT << 16;
		break;
	case OCS_INVALID_CMD_TABLE_ATTR:
	case OCS_INVALID_PRDT_ATTR:
	case OCS_MISMATCH_DATA_BUF_SIZE:
	case OCS_MISMATCH_RESP_UPIU_SIZE:
	case OCS_PEER_COMM_FAILURE:
	case OCS_FATAL_ERROR:
	default:
		result |= DID_ERROR << 16;
		dev_err(hba->dev,
		"OCS error from controller = %x\n", ocs);
		break;
	} 

	return result;
}

static void ufshcd_uic_cmd_compl(struct ufs_hba *hba)
{
	if (hba->active_uic_cmd) {
		hba->active_uic_cmd->argument2 |=
			ufshcd_get_uic_cmd_result(hba);
		complete(&hba->active_uic_cmd->done);
	}
}

static void ufshcd_transfer_req_compl(struct ufs_hba *hba)
{
	struct ufshcd_lrb *lrb;
	unsigned long completed_reqs;
	u32 tr_doorbell;
	int result;
	int index;

	lrb = hba->lrb;
	tr_doorbell = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
	completed_reqs = tr_doorbell ^ hba->outstanding_reqs;

	for (index = 0; index < hba->nutrs; index++) {
		if (test_bit(index, &completed_reqs)) {

			result = ufshcd_transfer_rsp_status(hba, &lrb[index]);

			if (lrb[index].cmd) {
				scsi_dma_unmap(lrb[index].cmd);
				lrb[index].cmd->result = result;
				lrb[index].cmd->scsi_done(lrb[index].cmd);

				
				lrb[index].cmd = NULL;
			}
		} 
	} 

	
	hba->outstanding_reqs ^= completed_reqs;

	
	ufshcd_config_int_aggr(hba, INT_AGGR_RESET);
}

static void ufshcd_fatal_err_handler(struct work_struct *work)
{
	struct ufs_hba *hba;
	hba = container_of(work, struct ufs_hba, feh_workq);

	
	if (hba->ufshcd_state != UFSHCD_STATE_RESET)
		ufshcd_do_reset(hba);
}

static void ufshcd_err_handler(struct ufs_hba *hba)
{
	u32 reg;

	if (hba->errors & INT_FATAL_ERRORS)
		goto fatal_eh;

	if (hba->errors & UIC_ERROR) {
		reg = ufshcd_readl(hba, REG_UIC_ERROR_CODE_DATA_LINK_LAYER);
		if (reg & UIC_DATA_LINK_LAYER_ERROR_PA_INIT)
			goto fatal_eh;
	}
	return;
fatal_eh:
	hba->ufshcd_state = UFSHCD_STATE_ERROR;
	schedule_work(&hba->feh_workq);
}

static void ufshcd_tmc_handler(struct ufs_hba *hba)
{
	u32 tm_doorbell;

	tm_doorbell = ufshcd_readl(hba, REG_UTP_TASK_REQ_DOOR_BELL);
	hba->tm_condition = tm_doorbell ^ hba->outstanding_tasks;
	wake_up_interruptible(&hba->ufshcd_tm_wait_queue);
}

static void ufshcd_sl_intr(struct ufs_hba *hba, u32 intr_status)
{
	hba->errors = UFSHCD_ERROR_MASK & intr_status;
	if (hba->errors)
		ufshcd_err_handler(hba);

	if (intr_status & UIC_COMMAND_COMPL)
		ufshcd_uic_cmd_compl(hba);

	if (intr_status & UTP_TASK_REQ_COMPL)
		ufshcd_tmc_handler(hba);

	if (intr_status & UTP_TRANSFER_REQ_COMPL)
		ufshcd_transfer_req_compl(hba);
}

static irqreturn_t ufshcd_intr(int irq, void *__hba)
{
	u32 intr_status;
	irqreturn_t retval = IRQ_NONE;
	struct ufs_hba *hba = __hba;

	spin_lock(hba->host->host_lock);
	intr_status = ufshcd_readl(hba, REG_INTERRUPT_STATUS);

	if (intr_status) {
		ufshcd_writel(hba, intr_status, REG_INTERRUPT_STATUS);
		ufshcd_sl_intr(hba, intr_status);
		retval = IRQ_HANDLED;
	}
	spin_unlock(hba->host->host_lock);
	return retval;
}

static int
ufshcd_issue_tm_cmd(struct ufs_hba *hba,
		    struct ufshcd_lrb *lrbp,
		    u8 tm_function)
{
	struct utp_task_req_desc *task_req_descp;
	struct utp_upiu_task_req *task_req_upiup;
	struct Scsi_Host *host;
	unsigned long flags;
	int free_slot = 0;
	int err;

	host = hba->host;

	spin_lock_irqsave(host->host_lock, flags);

	
	free_slot = ufshcd_get_tm_free_slot(hba);
	if (free_slot >= hba->nutmrs) {
		spin_unlock_irqrestore(host->host_lock, flags);
		dev_err(hba->dev, "Task management queue full\n");
		err = FAILED;
		goto out;
	}

	task_req_descp = hba->utmrdl_base_addr;
	task_req_descp += free_slot;

	
	task_req_descp->header.dword_0 = cpu_to_le32(UTP_REQ_DESC_INT_CMD);
	task_req_descp->header.dword_2 =
			cpu_to_le32(OCS_INVALID_COMMAND_STATUS);

	
	task_req_upiup =
		(struct utp_upiu_task_req *) task_req_descp->task_req_upiu;
	task_req_upiup->header.dword_0 =
		cpu_to_be32(UPIU_HEADER_DWORD(UPIU_TRANSACTION_TASK_REQ, 0,
					      lrbp->lun, lrbp->task_tag));
	task_req_upiup->header.dword_1 =
	cpu_to_be32(UPIU_HEADER_DWORD(0, tm_function, 0, 0));

	task_req_upiup->input_param1 = lrbp->lun;
	task_req_upiup->input_param1 =
		cpu_to_be32(task_req_upiup->input_param1);
	task_req_upiup->input_param2 = lrbp->task_tag;
	task_req_upiup->input_param2 =
		cpu_to_be32(task_req_upiup->input_param2);

	
	__set_bit(free_slot, &hba->outstanding_tasks);
	ufshcd_writel(hba, 1 << free_slot, REG_UTP_TASK_REQ_DOOR_BELL);

	spin_unlock_irqrestore(host->host_lock, flags);

	
	err =
	wait_event_interruptible_timeout(hba->ufshcd_tm_wait_queue,
					 (test_bit(free_slot,
					 &hba->tm_condition) != 0),
					 60 * HZ);
	if (!err) {
		dev_err(hba->dev,
			"Task management command timed-out\n");
		err = FAILED;
		goto out;
	}
	clear_bit(free_slot, &hba->tm_condition);
	err = ufshcd_task_req_compl(hba, free_slot);
out:
	return err;
}

static int ufshcd_device_reset(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *host;
	struct ufs_hba *hba;
	unsigned int tag;
	u32 pos;
	int err;

	host = cmd->device->host;
	hba = shost_priv(host);
	tag = cmd->request->tag;

	err = ufshcd_issue_tm_cmd(hba, &hba->lrb[tag], UFS_LOGICAL_RESET);
	if (err == FAILED)
		goto out;

	for (pos = 0; pos < hba->nutrs; pos++) {
		if (test_bit(pos, &hba->outstanding_reqs) &&
		    (hba->lrb[tag].lun == hba->lrb[pos].lun)) {

			
			ufshcd_utrl_clear(hba, pos);

			clear_bit(pos, &hba->outstanding_reqs);

			if (hba->lrb[pos].cmd) {
				scsi_dma_unmap(hba->lrb[pos].cmd);
				hba->lrb[pos].cmd->result =
						DID_ABORT << 16;
				hba->lrb[pos].cmd->scsi_done(cmd);
				hba->lrb[pos].cmd = NULL;
			}
		}
	} 
out:
	return err;
}

static int ufshcd_host_reset(struct scsi_cmnd *cmd)
{
	struct ufs_hba *hba;

	hba = shost_priv(cmd->device->host);

	if (hba->ufshcd_state == UFSHCD_STATE_RESET)
		return SUCCESS;

	return ufshcd_do_reset(hba);
}

static int ufshcd_abort(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *host;
	struct ufs_hba *hba;
	unsigned long flags;
	unsigned int tag;
	int err;

	host = cmd->device->host;
	hba = shost_priv(host);
	tag = cmd->request->tag;

	spin_lock_irqsave(host->host_lock, flags);

	
	if (!(test_bit(tag, &hba->outstanding_reqs))) {
		err = FAILED;
		spin_unlock_irqrestore(host->host_lock, flags);
		goto out;
	}
	spin_unlock_irqrestore(host->host_lock, flags);

	err = ufshcd_issue_tm_cmd(hba, &hba->lrb[tag], UFS_ABORT_TASK);
	if (err == FAILED)
		goto out;

	scsi_dma_unmap(cmd);

	spin_lock_irqsave(host->host_lock, flags);

	
	ufshcd_utrl_clear(hba, tag);

	__clear_bit(tag, &hba->outstanding_reqs);
	hba->lrb[tag].cmd = NULL;
	spin_unlock_irqrestore(host->host_lock, flags);
out:
	return err;
}

static void ufshcd_async_scan(void *data, async_cookie_t cookie)
{
	struct ufs_hba *hba = (struct ufs_hba *)data;
	int ret;

	ret = ufshcd_link_startup(hba);
	if (!ret)
		scsi_scan_host(hba->host);
}

static struct scsi_host_template ufshcd_driver_template = {
	.module			= THIS_MODULE,
	.name			= UFSHCD,
	.proc_name		= UFSHCD,
	.queuecommand		= ufshcd_queuecommand,
	.slave_alloc		= ufshcd_slave_alloc,
	.slave_destroy		= ufshcd_slave_destroy,
	.eh_abort_handler	= ufshcd_abort,
	.eh_device_reset_handler = ufshcd_device_reset,
	.eh_host_reset_handler	= ufshcd_host_reset,
	.this_id		= -1,
	.sg_tablesize		= SG_ALL,
	.cmd_per_lun		= UFSHCD_CMD_PER_LUN,
	.can_queue		= UFSHCD_CAN_QUEUE,
};

int ufshcd_suspend(struct ufs_hba *hba, pm_message_t state)
{

	return -ENOSYS;
}
EXPORT_SYMBOL_GPL(ufshcd_suspend);

int ufshcd_resume(struct ufs_hba *hba)
{

	return -ENOSYS;
}
EXPORT_SYMBOL_GPL(ufshcd_resume);

void ufshcd_remove(struct ufs_hba *hba)
{
	
	ufshcd_disable_intr(hba, hba->intr_mask);
	ufshcd_hba_stop(hba);

	scsi_remove_host(hba->host);
	scsi_host_put(hba->host);
}
EXPORT_SYMBOL_GPL(ufshcd_remove);

int ufshcd_init(struct device *dev, struct ufs_hba **hba_handle,
		 void __iomem *mmio_base, unsigned int irq)
{
	struct Scsi_Host *host;
	struct ufs_hba *hba;
	int err;

	if (!dev) {
		dev_err(dev,
		"Invalid memory reference for dev is NULL\n");
		err = -ENODEV;
		goto out_error;
	}

	if (!mmio_base) {
		dev_err(dev,
		"Invalid memory reference for mmio_base is NULL\n");
		err = -ENODEV;
		goto out_error;
	}

	host = scsi_host_alloc(&ufshcd_driver_template,
				sizeof(struct ufs_hba));
	if (!host) {
		dev_err(dev, "scsi_host_alloc failed\n");
		err = -ENOMEM;
		goto out_error;
	}
	hba = shost_priv(host);
	hba->host = host;
	hba->dev = dev;
	hba->mmio_base = mmio_base;
	hba->irq = irq;

	
	ufshcd_hba_capabilities(hba);

	
	hba->ufs_version = ufshcd_get_ufs_version(hba);

	
	hba->intr_mask = ufshcd_get_intr_mask(hba);

	
	err = ufshcd_memory_alloc(hba);
	if (err) {
		dev_err(hba->dev, "Memory allocation failed\n");
		goto out_disable;
	}

	
	ufshcd_host_memory_configure(hba);

	host->can_queue = hba->nutrs;
	host->cmd_per_lun = hba->nutrs;
	host->max_id = UFSHCD_MAX_ID;
	host->max_lun = UFSHCD_MAX_LUNS;
	host->max_channel = UFSHCD_MAX_CHANNEL;
	host->unique_id = host->host_no;
	host->max_cmd_len = MAX_CDB_SIZE;

	
	init_waitqueue_head(&hba->ufshcd_tm_wait_queue);

	
	INIT_WORK(&hba->feh_workq, ufshcd_fatal_err_handler);

	
	mutex_init(&hba->uic_cmd_mutex);

	
	err = devm_request_irq(dev, irq, ufshcd_intr, IRQF_SHARED, UFSHCD, hba);
	if (err) {
		dev_err(hba->dev, "request irq failed\n");
		goto out_disable;
	}

	
	err = scsi_init_shared_tag_map(host, host->can_queue);
	if (err) {
		dev_err(hba->dev, "init shared queue failed\n");
		goto out_disable;
	}

	err = scsi_add_host(host, hba->dev);
	if (err) {
		dev_err(hba->dev, "scsi_add_host failed\n");
		goto out_disable;
	}

	
	err = ufshcd_hba_enable(hba);
	if (err) {
		dev_err(hba->dev, "Host controller enable failed\n");
		goto out_remove_scsi_host;
	}

	*hba_handle = hba;

	async_schedule(ufshcd_async_scan, hba);

	return 0;

out_remove_scsi_host:
	scsi_remove_host(hba->host);
out_disable:
	scsi_host_put(host);
out_error:
	return err;
}
EXPORT_SYMBOL_GPL(ufshcd_init);

MODULE_AUTHOR("Santosh Yaragnavi <santosh.sy@samsung.com>");
MODULE_AUTHOR("Vinayak Holikatti <h.vinayak@samsung.com>");
MODULE_DESCRIPTION("Generic UFS host controller driver Core");
MODULE_LICENSE("GPL");
MODULE_VERSION(UFSHCD_DRIVER_VERSION);
