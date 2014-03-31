/*
 * pmcraid.c -- driver for PMC Sierra MaxRAID controller adapters
 *
 * Written By: Anil Ravindranath<anil_ravindranath@pmc-sierra.com>
 *             PMC-Sierra Inc
 *
 * Copyright (C) 2008, 2009 PMC Sierra Inc
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
 * USA
 *
 */
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/blkdev.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/hdreg.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <asm/irq.h>
#include <asm/processor.h>
#include <linux/libata.h>
#include <linux/mutex.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsicam.h>

#include "pmcraid.h"

static unsigned int pmcraid_debug_log;
static unsigned int pmcraid_disable_aen;
static unsigned int pmcraid_log_level = IOASC_LOG_LEVEL_MUST;
static unsigned int pmcraid_enable_msix;

static atomic_t pmcraid_adapter_count = ATOMIC_INIT(0);

static unsigned int pmcraid_major;
static struct class *pmcraid_class;
DECLARE_BITMAP(pmcraid_minor, PMCRAID_MAX_ADAPTERS);

MODULE_AUTHOR("Anil Ravindranath<anil_ravindranath@pmc-sierra.com>");
MODULE_DESCRIPTION("PMC Sierra MaxRAID Controller Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(PMCRAID_DRIVER_VERSION);

module_param_named(log_level, pmcraid_log_level, uint, (S_IRUGO | S_IWUSR));
MODULE_PARM_DESC(log_level,
		 "Enables firmware error code logging, default :1 high-severity"
		 " errors, 2: all errors including high-severity errors,"
		 " 0: disables logging");

module_param_named(debug, pmcraid_debug_log, uint, (S_IRUGO | S_IWUSR));
MODULE_PARM_DESC(debug,
		 "Enable driver verbose message logging. Set 1 to enable."
		 "(default: 0)");

module_param_named(disable_aen, pmcraid_disable_aen, uint, (S_IRUGO | S_IWUSR));
MODULE_PARM_DESC(disable_aen,
		 "Disable driver aen notifications to apps. Set 1 to disable."
		 "(default: 0)");

static struct pmcraid_chip_details pmcraid_chip_cfg[] = {
	{
	 .ioastatus = 0x0,
	 .ioarrin = 0x00040,
	 .mailbox = 0x7FC30,
	 .global_intr_mask = 0x00034,
	 .ioa_host_intr = 0x0009C,
	 .ioa_host_intr_clr = 0x000A0,
	 .ioa_host_msix_intr = 0x7FC40,
	 .ioa_host_mask = 0x7FC28,
	 .ioa_host_mask_clr = 0x7FC28,
	 .host_ioa_intr = 0x00020,
	 .host_ioa_intr_clr = 0x00020,
	 .transop_timeout = 300
	 }
};

static struct pci_device_id pmcraid_pci_table[] __devinitdata = {
	{ PCI_DEVICE(PCI_VENDOR_ID_PMC, PCI_DEVICE_ID_PMC_MAXRAID),
	  0, 0, (kernel_ulong_t)&pmcraid_chip_cfg[0]
	},
	{}
};

MODULE_DEVICE_TABLE(pci, pmcraid_pci_table);



static int pmcraid_slave_alloc(struct scsi_device *scsi_dev)
{
	struct pmcraid_resource_entry *temp, *res = NULL;
	struct pmcraid_instance *pinstance;
	u8 target, bus, lun;
	unsigned long lock_flags;
	int rc = -ENXIO;
	u16 fw_version;

	pinstance = shost_priv(scsi_dev->host);

	fw_version = be16_to_cpu(pinstance->inq_data->fw_version);

	spin_lock_irqsave(&pinstance->resource_lock, lock_flags);
	list_for_each_entry(temp, &pinstance->used_res_q, queue) {

		
		if (RES_IS_VSET(temp->cfg_entry)) {
			if (fw_version <= PMCRAID_FW_VERSION_1)
				target = temp->cfg_entry.unique_flags1;
			else
				target = temp->cfg_entry.array_id & 0xFF;

			if (target > PMCRAID_MAX_VSET_TARGETS)
				continue;
			bus = PMCRAID_VSET_BUS_ID;
			lun = 0;
		} else if (RES_IS_GSCSI(temp->cfg_entry)) {
			target = RES_TARGET(temp->cfg_entry.resource_address);
			bus = PMCRAID_PHYS_BUS_ID;
			lun = RES_LUN(temp->cfg_entry.resource_address);
		} else {
			continue;
		}

		if (bus == scsi_dev->channel &&
		    target == scsi_dev->id &&
		    lun == scsi_dev->lun) {
			res = temp;
			break;
		}
	}

	if (res) {
		res->scsi_dev = scsi_dev;
		scsi_dev->hostdata = res;
		res->change_detected = 0;
		atomic_set(&res->read_failures, 0);
		atomic_set(&res->write_failures, 0);
		rc = 0;
	}
	spin_unlock_irqrestore(&pinstance->resource_lock, lock_flags);
	return rc;
}

/**
 * pmcraid_slave_configure - Configures a SCSI device
 * @scsi_dev: scsi device struct
 *
 * This function is executed by SCSI mid layer just after a device is first
 * scanned (i.e. it has responded to an INQUIRY). For VSET resources, the
 * timeout value (default 30s) will be over-written to a higher value (60s)
 * and max_sectors value will be over-written to 512. It also sets queue depth
 * to host->cmd_per_lun value
 *
 * Return value:
 *	  0 on success
 */
static int pmcraid_slave_configure(struct scsi_device *scsi_dev)
{
	struct pmcraid_resource_entry *res = scsi_dev->hostdata;

	if (!res)
		return 0;

	
	if (RES_IS_GSCSI(res->cfg_entry) &&
	    scsi_dev->type != TYPE_ENCLOSURE)
		return -ENXIO;

	pmcraid_info("configuring %x:%x:%x:%x\n",
		     scsi_dev->host->unique_id,
		     scsi_dev->channel,
		     scsi_dev->id,
		     scsi_dev->lun);

	if (RES_IS_GSCSI(res->cfg_entry)) {
		scsi_dev->allow_restart = 1;
	} else if (RES_IS_VSET(res->cfg_entry)) {
		scsi_dev->allow_restart = 1;
		blk_queue_rq_timeout(scsi_dev->request_queue,
				     PMCRAID_VSET_IO_TIMEOUT);
		blk_queue_max_hw_sectors(scsi_dev->request_queue,
				      PMCRAID_VSET_MAX_SECTORS);
	}

	if (scsi_dev->tagged_supported &&
	    (RES_IS_GSCSI(res->cfg_entry) || RES_IS_VSET(res->cfg_entry))) {
		scsi_activate_tcq(scsi_dev, scsi_dev->queue_depth);
		scsi_adjust_queue_depth(scsi_dev, MSG_SIMPLE_TAG,
					scsi_dev->host->cmd_per_lun);
	} else {
		scsi_adjust_queue_depth(scsi_dev, 0,
					scsi_dev->host->cmd_per_lun);
	}

	return 0;
}

static void pmcraid_slave_destroy(struct scsi_device *scsi_dev)
{
	struct pmcraid_resource_entry *res;

	res = (struct pmcraid_resource_entry *)scsi_dev->hostdata;

	if (res)
		res->scsi_dev = NULL;

	scsi_dev->hostdata = NULL;
}

static int pmcraid_change_queue_depth(struct scsi_device *scsi_dev, int depth,
				      int reason)
{
	if (reason != SCSI_QDEPTH_DEFAULT)
		return -EOPNOTSUPP;

	if (depth > PMCRAID_MAX_CMD_PER_LUN)
		depth = PMCRAID_MAX_CMD_PER_LUN;

	scsi_adjust_queue_depth(scsi_dev, scsi_get_tag_type(scsi_dev), depth);

	return scsi_dev->queue_depth;
}

static int pmcraid_change_queue_type(struct scsi_device *scsi_dev, int tag)
{
	struct pmcraid_resource_entry *res;

	res = (struct pmcraid_resource_entry *)scsi_dev->hostdata;

	if ((res) && scsi_dev->tagged_supported &&
	    (RES_IS_GSCSI(res->cfg_entry) || RES_IS_VSET(res->cfg_entry))) {
		scsi_set_tag_type(scsi_dev, tag);

		if (tag)
			scsi_activate_tcq(scsi_dev, scsi_dev->queue_depth);
		else
			scsi_deactivate_tcq(scsi_dev, scsi_dev->queue_depth);
	} else
		tag = 0;

	return tag;
}


void pmcraid_init_cmdblk(struct pmcraid_cmd *cmd, int index)
{
	struct pmcraid_ioarcb *ioarcb = &(cmd->ioa_cb->ioarcb);
	dma_addr_t dma_addr = cmd->ioa_cb_bus_addr;

	if (index >= 0) {
		
		u32 ioasa_offset =
			offsetof(struct pmcraid_control_block, ioasa);

		cmd->index = index;
		ioarcb->response_handle = cpu_to_le32(index << 2);
		ioarcb->ioarcb_bus_addr = cpu_to_le64(dma_addr);
		ioarcb->ioasa_bus_addr = cpu_to_le64(dma_addr + ioasa_offset);
		ioarcb->ioasa_len = cpu_to_le16(sizeof(struct pmcraid_ioasa));
	} else {
		memset(&cmd->ioa_cb->ioarcb.cdb, 0, PMCRAID_MAX_CDB_LEN);
		ioarcb->hrrq_id = 0;
		ioarcb->request_flags0 = 0;
		ioarcb->request_flags1 = 0;
		ioarcb->cmd_timeout = 0;
		ioarcb->ioarcb_bus_addr &= (~0x1FULL);
		ioarcb->ioadl_bus_addr = 0;
		ioarcb->ioadl_length = 0;
		ioarcb->data_transfer_length = 0;
		ioarcb->add_cmd_param_length = 0;
		ioarcb->add_cmd_param_offset = 0;
		cmd->ioa_cb->ioasa.ioasc = 0;
		cmd->ioa_cb->ioasa.residual_data_length = 0;
		cmd->time_left = 0;
	}

	cmd->cmd_done = NULL;
	cmd->scsi_cmd = NULL;
	cmd->release = 0;
	cmd->completion_req = 0;
	cmd->sense_buffer = 0;
	cmd->sense_buffer_dma = 0;
	cmd->dma_handle = 0;
	init_timer(&cmd->timer);
}

static void pmcraid_reinit_cmdblk(struct pmcraid_cmd *cmd)
{
	pmcraid_init_cmdblk(cmd, -1);
}

static struct pmcraid_cmd *pmcraid_get_free_cmd(
	struct pmcraid_instance *pinstance
)
{
	struct pmcraid_cmd *cmd = NULL;
	unsigned long lock_flags;

	
	spin_lock_irqsave(&pinstance->free_pool_lock, lock_flags);

	if (!list_empty(&pinstance->free_cmd_pool)) {
		cmd = list_entry(pinstance->free_cmd_pool.next,
				 struct pmcraid_cmd, free_list);
		list_del(&cmd->free_list);
	}
	spin_unlock_irqrestore(&pinstance->free_pool_lock, lock_flags);

	
	if (cmd != NULL)
		pmcraid_reinit_cmdblk(cmd);
	return cmd;
}

void pmcraid_return_cmd(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	unsigned long lock_flags;

	spin_lock_irqsave(&pinstance->free_pool_lock, lock_flags);
	list_add_tail(&cmd->free_list, &pinstance->free_cmd_pool);
	spin_unlock_irqrestore(&pinstance->free_pool_lock, lock_flags);
}

static u32 pmcraid_read_interrupts(struct pmcraid_instance *pinstance)
{
	return (pinstance->interrupt_mode) ?
		ioread32(pinstance->int_regs.ioa_host_msix_interrupt_reg) :
		ioread32(pinstance->int_regs.ioa_host_interrupt_reg);
}

static void pmcraid_disable_interrupts(
	struct pmcraid_instance *pinstance,
	u32 intrs
)
{
	u32 gmask = ioread32(pinstance->int_regs.global_interrupt_mask_reg);
	u32 nmask = gmask | GLOBAL_INTERRUPT_MASK;

	iowrite32(intrs, pinstance->int_regs.ioa_host_interrupt_clr_reg);
	iowrite32(nmask, pinstance->int_regs.global_interrupt_mask_reg);
	ioread32(pinstance->int_regs.global_interrupt_mask_reg);

	if (!pinstance->interrupt_mode) {
		iowrite32(intrs,
			pinstance->int_regs.ioa_host_interrupt_mask_reg);
		ioread32(pinstance->int_regs.ioa_host_interrupt_mask_reg);
	}
}

static void pmcraid_enable_interrupts(
	struct pmcraid_instance *pinstance,
	u32 intrs
)
{
	u32 gmask = ioread32(pinstance->int_regs.global_interrupt_mask_reg);
	u32 nmask = gmask & (~GLOBAL_INTERRUPT_MASK);

	iowrite32(nmask, pinstance->int_regs.global_interrupt_mask_reg);

	if (!pinstance->interrupt_mode) {
		iowrite32(~intrs,
			 pinstance->int_regs.ioa_host_interrupt_mask_reg);
		ioread32(pinstance->int_regs.ioa_host_interrupt_mask_reg);
	}

	pmcraid_info("enabled interrupts global mask = %x intr_mask = %x\n",
		ioread32(pinstance->int_regs.global_interrupt_mask_reg),
		ioread32(pinstance->int_regs.ioa_host_interrupt_mask_reg));
}

static void pmcraid_clr_trans_op(
	struct pmcraid_instance *pinstance
)
{
	unsigned long lock_flags;

	if (!pinstance->interrupt_mode) {
		iowrite32(INTRS_TRANSITION_TO_OPERATIONAL,
			pinstance->int_regs.ioa_host_interrupt_mask_reg);
		ioread32(pinstance->int_regs.ioa_host_interrupt_mask_reg);
		iowrite32(INTRS_TRANSITION_TO_OPERATIONAL,
			pinstance->int_regs.ioa_host_interrupt_clr_reg);
		ioread32(pinstance->int_regs.ioa_host_interrupt_clr_reg);
	}

	if (pinstance->reset_cmd != NULL) {
		del_timer(&pinstance->reset_cmd->timer);
		spin_lock_irqsave(
			pinstance->host->host_lock, lock_flags);
		pinstance->reset_cmd->cmd_done(pinstance->reset_cmd);
		spin_unlock_irqrestore(
			pinstance->host->host_lock, lock_flags);
	}
}

static void pmcraid_reset_type(struct pmcraid_instance *pinstance)
{
	u32 mask;
	u32 intrs;
	u32 alerts;

	mask = ioread32(pinstance->int_regs.ioa_host_interrupt_mask_reg);
	intrs = ioread32(pinstance->int_regs.ioa_host_interrupt_reg);
	alerts = ioread32(pinstance->int_regs.host_ioa_interrupt_reg);

	if ((mask & INTRS_HRRQ_VALID) == 0 ||
	    (alerts & DOORBELL_IOA_RESET_ALERT) ||
	    (intrs & PMCRAID_ERROR_INTERRUPTS)) {
		pmcraid_info("IOA requires hard reset\n");
		pinstance->ioa_hard_reset = 1;
	}

	
	if (intrs & INTRS_IOA_UNIT_CHECK)
		pinstance->ioa_unit_check = 1;
}


static void pmcraid_ioa_reset(struct pmcraid_cmd *);

static void pmcraid_bist_done(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	unsigned long lock_flags;
	int rc;
	u16 pci_reg;

	rc = pci_read_config_word(pinstance->pdev, PCI_COMMAND, &pci_reg);

	
	if ((rc != PCIBIOS_SUCCESSFUL || (!(pci_reg & PCI_COMMAND_MEMORY))) &&
	    cmd->time_left > 0) {
		pmcraid_info("BIST not complete, waiting another 2 secs\n");
		cmd->timer.expires = jiffies + cmd->time_left;
		cmd->time_left = 0;
		cmd->timer.data = (unsigned long)cmd;
		cmd->timer.function =
			(void (*)(unsigned long))pmcraid_bist_done;
		add_timer(&cmd->timer);
	} else {
		cmd->time_left = 0;
		pmcraid_info("BIST is complete, proceeding with reset\n");
		spin_lock_irqsave(pinstance->host->host_lock, lock_flags);
		pmcraid_ioa_reset(cmd);
		spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);
	}
}

static void pmcraid_start_bist(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	u32 doorbells, intrs;

	
	iowrite32(DOORBELL_IOA_START_BIST,
		pinstance->int_regs.host_ioa_interrupt_reg);
	doorbells = ioread32(pinstance->int_regs.host_ioa_interrupt_reg);
	intrs = ioread32(pinstance->int_regs.ioa_host_interrupt_reg);
	pmcraid_info("doorbells after start bist: %x intrs: %x\n",
		      doorbells, intrs);

	cmd->time_left = msecs_to_jiffies(PMCRAID_BIST_TIMEOUT);
	cmd->timer.data = (unsigned long)cmd;
	cmd->timer.expires = jiffies + msecs_to_jiffies(PMCRAID_BIST_TIMEOUT);
	cmd->timer.function = (void (*)(unsigned long))pmcraid_bist_done;
	add_timer(&cmd->timer);
}

static void pmcraid_reset_alert_done(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	u32 status = ioread32(pinstance->ioa_status);
	unsigned long lock_flags;

	if (((status & INTRS_CRITICAL_OP_IN_PROGRESS) == 0) ||
	    cmd->time_left <= 0) {
		pmcraid_info("critical op is reset proceeding with reset\n");
		spin_lock_irqsave(pinstance->host->host_lock, lock_flags);
		pmcraid_ioa_reset(cmd);
		spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);
	} else {
		pmcraid_info("critical op is not yet reset waiting again\n");
		
		cmd->time_left -= PMCRAID_CHECK_FOR_RESET_TIMEOUT;
		cmd->timer.data = (unsigned long)cmd;
		cmd->timer.expires = jiffies + PMCRAID_CHECK_FOR_RESET_TIMEOUT;
		cmd->timer.function =
			(void (*)(unsigned long))pmcraid_reset_alert_done;
		add_timer(&cmd->timer);
	}
}

/**
 * pmcraid_reset_alert - alerts IOA for a possible reset
 * @cmd : command block to be used for reset sequence.
 *
 * Return Value
 *	returns 0 if pci config-space is accessible and RESET_DOORBELL is
 *	successfully written to IOA. Returns non-zero in case pci_config_space
 *	is not accessible
 */
static void pmcraid_notify_ioastate(struct pmcraid_instance *, u32);
static void pmcraid_reset_alert(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	u32 doorbells;
	int rc;
	u16 pci_reg;

	rc = pci_read_config_word(pinstance->pdev, PCI_COMMAND, &pci_reg);
	if ((rc == PCIBIOS_SUCCESSFUL) && (pci_reg & PCI_COMMAND_MEMORY)) {

		cmd->time_left = PMCRAID_RESET_TIMEOUT;
		cmd->timer.data = (unsigned long)cmd;
		cmd->timer.expires = jiffies + PMCRAID_CHECK_FOR_RESET_TIMEOUT;
		cmd->timer.function =
			(void (*)(unsigned long))pmcraid_reset_alert_done;
		add_timer(&cmd->timer);

		iowrite32(DOORBELL_IOA_RESET_ALERT,
			pinstance->int_regs.host_ioa_interrupt_reg);
		doorbells =
			ioread32(pinstance->int_regs.host_ioa_interrupt_reg);
		pmcraid_info("doorbells after reset alert: %x\n", doorbells);
	} else {
		pmcraid_info("PCI config is not accessible starting BIST\n");
		pinstance->ioa_state = IOA_STATE_IN_HARD_RESET;
		pmcraid_start_bist(cmd);
	}
}

static void pmcraid_timeout_handler(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	unsigned long lock_flags;

	dev_info(&pinstance->pdev->dev,
		"Adapter being reset due to cmd(CDB[0] = %x) timeout\n",
		cmd->ioa_cb->ioarcb.cdb[0]);

	spin_lock_irqsave(pinstance->host->host_lock, lock_flags);
	if (!pinstance->ioa_reset_in_progress) {
		pinstance->ioa_reset_attempts = 0;
		cmd = pmcraid_get_free_cmd(pinstance);

		if (cmd == NULL) {
			spin_unlock_irqrestore(pinstance->host->host_lock,
					       lock_flags);
			pmcraid_err("no free cmnd block for timeout handler\n");
			return;
		}

		pinstance->reset_cmd = cmd;
		pinstance->ioa_reset_in_progress = 1;
	} else {
		pmcraid_info("reset is already in progress\n");

		if (pinstance->reset_cmd != cmd) {
			pmcraid_err("cmd is pending but reset in progress\n");
		}

		if (cmd == pinstance->reset_cmd)
			cmd->cmd_done = pmcraid_ioa_reset;
	}

	
	if (pinstance->scn.ioa_state != PMC_DEVICE_EVENT_RESET_START &&
	    pinstance->scn.ioa_state != PMC_DEVICE_EVENT_SHUTDOWN_START)
		pmcraid_notify_ioastate(pinstance,
					PMC_DEVICE_EVENT_RESET_START);

	pinstance->ioa_state = IOA_STATE_IN_RESET_ALERT;
	scsi_block_requests(pinstance->host);
	pmcraid_reset_alert(cmd);
	spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);
}

static void pmcraid_internal_done(struct pmcraid_cmd *cmd)
{
	pmcraid_info("response internal cmd CDB[0] = %x ioasc = %x\n",
		     cmd->ioa_cb->ioarcb.cdb[0],
		     le32_to_cpu(cmd->ioa_cb->ioasa.ioasc));

	if (cmd->completion_req) {
		cmd->completion_req = 0;
		complete(&cmd->wait_for_completion);
	}

	if (cmd->release) {
		cmd->release = 0;
		pmcraid_return_cmd(cmd);
	}
}

static void pmcraid_reinit_cfgtable_done(struct pmcraid_cmd *cmd)
{
	pmcraid_info("response internal cmd CDB[0] = %x ioasc = %x\n",
		     cmd->ioa_cb->ioarcb.cdb[0],
		     le32_to_cpu(cmd->ioa_cb->ioasa.ioasc));

	if (cmd->release) {
		cmd->release = 0;
		pmcraid_return_cmd(cmd);
	}
	pmcraid_info("scheduling worker for config table reinitialization\n");
	schedule_work(&cmd->drv_inst->worker_q);
}

static void pmcraid_erp_done(struct pmcraid_cmd *cmd)
{
	struct scsi_cmnd *scsi_cmd = cmd->scsi_cmd;
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	u32 ioasc = le32_to_cpu(cmd->ioa_cb->ioasa.ioasc);

	if (PMCRAID_IOASC_SENSE_KEY(ioasc) > 0) {
		scsi_cmd->result |= (DID_ERROR << 16);
		scmd_printk(KERN_INFO, scsi_cmd,
			    "command CDB[0] = %x failed with IOASC: 0x%08X\n",
			    cmd->ioa_cb->ioarcb.cdb[0], ioasc);
	}

	if (cmd->sense_buffer != NULL) {
		memcpy(scsi_cmd->sense_buffer,
		       cmd->sense_buffer,
		       SCSI_SENSE_BUFFERSIZE);
		pci_free_consistent(pinstance->pdev,
				    SCSI_SENSE_BUFFERSIZE,
				    cmd->sense_buffer, cmd->sense_buffer_dma);
		cmd->sense_buffer = NULL;
		cmd->sense_buffer_dma = 0;
	}

	scsi_dma_unmap(scsi_cmd);
	pmcraid_return_cmd(cmd);
	scsi_cmd->scsi_done(scsi_cmd);
}

static void _pmcraid_fire_command(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	unsigned long lock_flags;

	spin_lock_irqsave(&pinstance->pending_pool_lock, lock_flags);
	list_add_tail(&cmd->free_list, &pinstance->pending_cmd_pool);
	spin_unlock_irqrestore(&pinstance->pending_pool_lock, lock_flags);
	atomic_inc(&pinstance->outstanding_cmds);

	
	mb();
	iowrite32(le32_to_cpu(cmd->ioa_cb->ioarcb.ioarcb_bus_addr),
		  pinstance->ioarrin);
}

static void pmcraid_send_cmd(
	struct pmcraid_cmd *cmd,
	void (*cmd_done) (struct pmcraid_cmd *),
	unsigned long timeout,
	void (*timeout_func) (struct pmcraid_cmd *)
)
{
	
	cmd->cmd_done = cmd_done;

	if (timeout_func) {
		
		cmd->timer.data = (unsigned long)cmd;
		cmd->timer.expires = jiffies + timeout;
		cmd->timer.function = (void (*)(unsigned long))timeout_func;
		add_timer(&cmd->timer);
	}

	
	_pmcraid_fire_command(cmd);
}

static void pmcraid_ioa_shutdown_done(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	unsigned long lock_flags;

	spin_lock_irqsave(pinstance->host->host_lock, lock_flags);
	pmcraid_ioa_reset(cmd);
	spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);
}

static void pmcraid_ioa_shutdown(struct pmcraid_cmd *cmd)
{
	pmcraid_info("response for Cancel CCN CDB[0] = %x ioasc = %x\n",
		     cmd->ioa_cb->ioarcb.cdb[0],
		     le32_to_cpu(cmd->ioa_cb->ioasa.ioasc));

	pmcraid_reinit_cmdblk(cmd);
	cmd->ioa_cb->ioarcb.request_type = REQ_TYPE_IOACMD;
	cmd->ioa_cb->ioarcb.resource_handle =
		cpu_to_le32(PMCRAID_IOA_RES_HANDLE);
	cmd->ioa_cb->ioarcb.cdb[0] = PMCRAID_IOA_SHUTDOWN;
	cmd->ioa_cb->ioarcb.cdb[1] = PMCRAID_SHUTDOWN_NORMAL;

	
	pmcraid_info("firing normal shutdown command (%d) to IOA\n",
		     le32_to_cpu(cmd->ioa_cb->ioarcb.response_handle));

	pmcraid_notify_ioastate(cmd->drv_inst, PMC_DEVICE_EVENT_SHUTDOWN_START);

	pmcraid_send_cmd(cmd, pmcraid_ioa_shutdown_done,
			 PMCRAID_SHUTDOWN_TIMEOUT,
			 pmcraid_timeout_handler);
}

static void pmcraid_querycfg(struct pmcraid_cmd *);

static void pmcraid_get_fwversion_done(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	u32 ioasc = le32_to_cpu(cmd->ioa_cb->ioasa.ioasc);
	unsigned long lock_flags;

	if (ioasc) {
		pmcraid_err("IOA Inquiry failed with %x\n", ioasc);
		spin_lock_irqsave(pinstance->host->host_lock, lock_flags);
		pinstance->ioa_state = IOA_STATE_IN_RESET_ALERT;
		pmcraid_reset_alert(cmd);
		spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);
	} else  {
		pmcraid_querycfg(cmd);
	}
}

static void pmcraid_get_fwversion(struct pmcraid_cmd *cmd)
{
	struct pmcraid_ioarcb *ioarcb = &cmd->ioa_cb->ioarcb;
	struct pmcraid_ioadl_desc *ioadl = ioarcb->add_data.u.ioadl;
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	u16 data_size = sizeof(struct pmcraid_inquiry_data);

	pmcraid_reinit_cmdblk(cmd);
	ioarcb->request_type = REQ_TYPE_SCSI;
	ioarcb->resource_handle = cpu_to_le32(PMCRAID_IOA_RES_HANDLE);
	ioarcb->cdb[0] = INQUIRY;
	ioarcb->cdb[1] = 1;
	ioarcb->cdb[2] = 0xD0;
	ioarcb->cdb[3] = (data_size >> 8) & 0xFF;
	ioarcb->cdb[4] = data_size & 0xFF;

	ioarcb->ioadl_bus_addr = cpu_to_le64((cmd->ioa_cb_bus_addr) +
					offsetof(struct pmcraid_ioarcb,
						add_data.u.ioadl[0]));
	ioarcb->ioadl_length = cpu_to_le32(sizeof(struct pmcraid_ioadl_desc));
	ioarcb->ioarcb_bus_addr &= ~(0x1FULL);

	ioarcb->request_flags0 |= NO_LINK_DESCS;
	ioarcb->data_transfer_length = cpu_to_le32(data_size);
	ioadl = &(ioarcb->add_data.u.ioadl[0]);
	ioadl->flags = IOADL_FLAGS_LAST_DESC;
	ioadl->address = cpu_to_le64(pinstance->inq_data_baddr);
	ioadl->data_len = cpu_to_le32(data_size);

	pmcraid_send_cmd(cmd, pmcraid_get_fwversion_done,
			 PMCRAID_INTERNAL_TIMEOUT, pmcraid_timeout_handler);
}

static void pmcraid_identify_hrrq(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	struct pmcraid_ioarcb *ioarcb = &cmd->ioa_cb->ioarcb;
	int index = cmd->hrrq_index;
	__be64 hrrq_addr = cpu_to_be64(pinstance->hrrq_start_bus_addr[index]);
	u32 hrrq_size = cpu_to_be32(sizeof(u32) * PMCRAID_MAX_CMD);
	void (*done_function)(struct pmcraid_cmd *);

	pmcraid_reinit_cmdblk(cmd);
	cmd->hrrq_index = index + 1;

	if (cmd->hrrq_index < pinstance->num_hrrq) {
		done_function = pmcraid_identify_hrrq;
	} else {
		cmd->hrrq_index = 0;
		done_function = pmcraid_get_fwversion;
	}

	
	ioarcb->request_type = REQ_TYPE_IOACMD;
	ioarcb->resource_handle = cpu_to_le32(PMCRAID_IOA_RES_HANDLE);

	
	ioarcb->hrrq_id = index;
	ioarcb->cdb[0] = PMCRAID_IDENTIFY_HRRQ;
	ioarcb->cdb[1] = index;

	/* IOA expects 64-bit pci address to be written in B.E format
	 * (i.e cdb[2]=MSByte..cdb[9]=LSB.
	 */
	pmcraid_info("HRRQ_IDENTIFY with hrrq:ioarcb:index => %llx:%llx:%x\n",
		     hrrq_addr, ioarcb->ioarcb_bus_addr, index);

	memcpy(&(ioarcb->cdb[2]), &hrrq_addr, sizeof(hrrq_addr));
	memcpy(&(ioarcb->cdb[10]), &hrrq_size, sizeof(hrrq_size));

	pmcraid_send_cmd(cmd, done_function,
			 PMCRAID_INTERNAL_TIMEOUT,
			 pmcraid_timeout_handler);
}

static void pmcraid_process_ccn(struct pmcraid_cmd *cmd);
static void pmcraid_process_ldn(struct pmcraid_cmd *cmd);

static void pmcraid_send_hcam_cmd(struct pmcraid_cmd *cmd)
{
	if (cmd->ioa_cb->ioarcb.cdb[1] == PMCRAID_HCAM_CODE_CONFIG_CHANGE)
		atomic_set(&(cmd->drv_inst->ccn.ignore), 0);
	else
		atomic_set(&(cmd->drv_inst->ldn.ignore), 0);

	pmcraid_send_cmd(cmd, cmd->cmd_done, 0, NULL);
}

static struct pmcraid_cmd *pmcraid_init_hcam
(
	struct pmcraid_instance *pinstance,
	u8 type
)
{
	struct pmcraid_cmd *cmd;
	struct pmcraid_ioarcb *ioarcb;
	struct pmcraid_ioadl_desc *ioadl;
	struct pmcraid_hostrcb *hcam;
	void (*cmd_done) (struct pmcraid_cmd *);
	dma_addr_t dma;
	int rcb_size;

	cmd = pmcraid_get_free_cmd(pinstance);

	if (!cmd) {
		pmcraid_err("no free command blocks for hcam\n");
		return cmd;
	}

	if (type == PMCRAID_HCAM_CODE_CONFIG_CHANGE) {
		rcb_size = sizeof(struct pmcraid_hcam_ccn_ext);
		cmd_done = pmcraid_process_ccn;
		dma = pinstance->ccn.baddr + PMCRAID_AEN_HDR_SIZE;
		hcam = &pinstance->ccn;
	} else {
		rcb_size = sizeof(struct pmcraid_hcam_ldn);
		cmd_done = pmcraid_process_ldn;
		dma = pinstance->ldn.baddr + PMCRAID_AEN_HDR_SIZE;
		hcam = &pinstance->ldn;
	}

	
	hcam->cmd = cmd;

	ioarcb = &cmd->ioa_cb->ioarcb;
	ioarcb->ioadl_bus_addr = cpu_to_le64((cmd->ioa_cb_bus_addr) +
					offsetof(struct pmcraid_ioarcb,
						add_data.u.ioadl[0]));
	ioarcb->ioadl_length = cpu_to_le32(sizeof(struct pmcraid_ioadl_desc));
	ioadl = ioarcb->add_data.u.ioadl;

	
	ioarcb->request_type = REQ_TYPE_HCAM;
	ioarcb->resource_handle = cpu_to_le32(PMCRAID_IOA_RES_HANDLE);
	ioarcb->cdb[0] = PMCRAID_HOST_CONTROLLED_ASYNC;
	ioarcb->cdb[1] = type;
	ioarcb->cdb[7] = (rcb_size >> 8) & 0xFF;
	ioarcb->cdb[8] = (rcb_size) & 0xFF;

	ioarcb->data_transfer_length = cpu_to_le32(rcb_size);

	ioadl[0].flags |= IOADL_FLAGS_READ_LAST;
	ioadl[0].data_len = cpu_to_le32(rcb_size);
	ioadl[0].address = cpu_to_le32(dma);

	cmd->cmd_done = cmd_done;
	return cmd;
}

static void pmcraid_send_hcam(struct pmcraid_instance *pinstance, u8 type)
{
	struct pmcraid_cmd *cmd = pmcraid_init_hcam(pinstance, type);
	pmcraid_send_hcam_cmd(cmd);
}


static void pmcraid_prepare_cancel_cmd(
	struct pmcraid_cmd *cmd,
	struct pmcraid_cmd *cmd_to_cancel
)
{
	struct pmcraid_ioarcb *ioarcb = &cmd->ioa_cb->ioarcb;
	__be64 ioarcb_addr = cmd_to_cancel->ioa_cb->ioarcb.ioarcb_bus_addr;

	ioarcb->resource_handle = cmd_to_cancel->ioa_cb->ioarcb.resource_handle;
	ioarcb->request_type = REQ_TYPE_IOACMD;
	memset(ioarcb->cdb, 0, PMCRAID_MAX_CDB_LEN);
	ioarcb->cdb[0] = PMCRAID_ABORT_CMD;

	ioarcb_addr = cpu_to_be64(ioarcb_addr);
	memcpy(&(ioarcb->cdb[2]), &ioarcb_addr, sizeof(ioarcb_addr));
}

static void pmcraid_cancel_hcam(
	struct pmcraid_cmd *cmd,
	u8 type,
	void (*cmd_done) (struct pmcraid_cmd *)
)
{
	struct pmcraid_instance *pinstance;
	struct pmcraid_hostrcb  *hcam;

	pinstance = cmd->drv_inst;
	hcam =  (type == PMCRAID_HCAM_CODE_LOG_DATA) ?
		&pinstance->ldn : &pinstance->ccn;

	if (hcam->cmd == NULL)
		return;

	pmcraid_prepare_cancel_cmd(cmd, hcam->cmd);

	pmcraid_send_cmd(cmd, cmd_done,
			 PMCRAID_INTERNAL_TIMEOUT,
			 pmcraid_timeout_handler);
}

static void pmcraid_cancel_ccn(struct pmcraid_cmd *cmd)
{
	pmcraid_info("response for Cancel LDN CDB[0] = %x ioasc = %x\n",
		     cmd->ioa_cb->ioarcb.cdb[0],
		     le32_to_cpu(cmd->ioa_cb->ioasa.ioasc));

	pmcraid_reinit_cmdblk(cmd);

	pmcraid_cancel_hcam(cmd,
			    PMCRAID_HCAM_CODE_CONFIG_CHANGE,
			    pmcraid_ioa_shutdown);
}

static void pmcraid_cancel_ldn(struct pmcraid_cmd *cmd)
{
	pmcraid_cancel_hcam(cmd,
			    PMCRAID_HCAM_CODE_LOG_DATA,
			    pmcraid_cancel_ccn);
}

static int pmcraid_expose_resource(u16 fw_version,
				   struct pmcraid_config_table_entry *cfgte)
{
	int retval = 0;

	if (cfgte->resource_type == RES_TYPE_VSET) {
		if (fw_version <= PMCRAID_FW_VERSION_1)
			retval = ((cfgte->unique_flags1 & 0x80) == 0);
		else
			retval = ((cfgte->unique_flags0 & 0x80) == 0 &&
				  (cfgte->unique_flags1 & 0x80) == 0);

	} else if (cfgte->resource_type == RES_TYPE_GSCSI)
		retval = (RES_BUS(cfgte->resource_address) !=
				PMCRAID_VIRTUAL_ENCL_BUS_ID);
	return retval;
}

enum {
	PMCRAID_AEN_ATTR_UNSPEC,
	PMCRAID_AEN_ATTR_EVENT,
	__PMCRAID_AEN_ATTR_MAX,
};
#define PMCRAID_AEN_ATTR_MAX (__PMCRAID_AEN_ATTR_MAX - 1)

enum {
	PMCRAID_AEN_CMD_UNSPEC,
	PMCRAID_AEN_CMD_EVENT,
	__PMCRAID_AEN_CMD_MAX,
};
#define PMCRAID_AEN_CMD_MAX (__PMCRAID_AEN_CMD_MAX - 1)

static struct genl_family pmcraid_event_family = {
	.id = GENL_ID_GENERATE,
	.name = "pmcraid",
	.version = 1,
	.maxattr = PMCRAID_AEN_ATTR_MAX
};

static int pmcraid_netlink_init(void)
{
	int result;

	result = genl_register_family(&pmcraid_event_family);

	if (result)
		return result;

	pmcraid_info("registered NETLINK GENERIC group: %d\n",
		     pmcraid_event_family.id);

	return result;
}

static void pmcraid_netlink_release(void)
{
	genl_unregister_family(&pmcraid_event_family);
}

static int pmcraid_notify_aen(
	struct pmcraid_instance *pinstance,
	struct pmcraid_aen_msg  *aen_msg,
	u32    data_size
)
{
	struct sk_buff *skb;
	void *msg_header;
	u32  total_size, nla_genl_hdr_total_size;
	int result;

	aen_msg->hostno = (pinstance->host->unique_id << 16 |
			   MINOR(pinstance->cdev.dev));
	aen_msg->length = data_size;

	data_size += sizeof(*aen_msg);

	total_size = nla_total_size(data_size);
	
	nla_genl_hdr_total_size =
		(total_size + (GENL_HDRLEN +
		((struct genl_family *)&pmcraid_event_family)->hdrsize)
		 + NLMSG_HDRLEN);
	skb = genlmsg_new(nla_genl_hdr_total_size, GFP_ATOMIC);


	if (!skb) {
		pmcraid_err("Failed to allocate aen data SKB of size: %x\n",
			     total_size);
		return -ENOMEM;
	}

	
	msg_header = genlmsg_put(skb, 0, 0,
				 &pmcraid_event_family, 0,
				 PMCRAID_AEN_CMD_EVENT);
	if (!msg_header) {
		pmcraid_err("failed to copy command details\n");
		nlmsg_free(skb);
		return -ENOMEM;
	}

	result = nla_put(skb, PMCRAID_AEN_ATTR_EVENT, data_size, aen_msg);

	if (result) {
		pmcraid_err("failed to copy AEN attribute data\n");
		nlmsg_free(skb);
		return -EINVAL;
	}

	
	result = genlmsg_end(skb, msg_header);

	if (result < 0) {
		pmcraid_err("genlmsg_end failed\n");
		nlmsg_free(skb);
		return result;
	}

	result =
		genlmsg_multicast(skb, 0, pmcraid_event_family.id, GFP_ATOMIC);

	if (result)
		pmcraid_info("error (%x) sending aen event message\n", result);
	return result;
}

static int pmcraid_notify_ccn(struct pmcraid_instance *pinstance)
{
	return pmcraid_notify_aen(pinstance,
				pinstance->ccn.msg,
				pinstance->ccn.hcam->data_len +
				sizeof(struct pmcraid_hcam_hdr));
}

static int pmcraid_notify_ldn(struct pmcraid_instance *pinstance)
{
	return pmcraid_notify_aen(pinstance,
				pinstance->ldn.msg,
				pinstance->ldn.hcam->data_len +
				sizeof(struct pmcraid_hcam_hdr));
}

static void pmcraid_notify_ioastate(struct pmcraid_instance *pinstance, u32 evt)
{
	pinstance->scn.ioa_state = evt;
	pmcraid_notify_aen(pinstance,
			  &pinstance->scn.msg,
			  sizeof(u32));
}


static void pmcraid_handle_config_change(struct pmcraid_instance *pinstance)
{
	struct pmcraid_config_table_entry *cfg_entry;
	struct pmcraid_hcam_ccn *ccn_hcam;
	struct pmcraid_cmd *cmd;
	struct pmcraid_cmd *cfgcmd;
	struct pmcraid_resource_entry *res = NULL;
	unsigned long lock_flags;
	unsigned long host_lock_flags;
	u32 new_entry = 1;
	u32 hidden_entry = 0;
	u16 fw_version;
	int rc;

	ccn_hcam = (struct pmcraid_hcam_ccn *)pinstance->ccn.hcam;
	cfg_entry = &ccn_hcam->cfg_entry;
	fw_version = be16_to_cpu(pinstance->inq_data->fw_version);

	pmcraid_info("CCN(%x): %x timestamp: %llx type: %x lost: %x flags: %x \
		 res: %x:%x:%x:%x\n",
		 pinstance->ccn.hcam->ilid,
		 pinstance->ccn.hcam->op_code,
		((pinstance->ccn.hcam->timestamp1) |
		((pinstance->ccn.hcam->timestamp2 & 0xffffffffLL) << 32)),
		 pinstance->ccn.hcam->notification_type,
		 pinstance->ccn.hcam->notification_lost,
		 pinstance->ccn.hcam->flags,
		 pinstance->host->unique_id,
		 RES_IS_VSET(*cfg_entry) ? PMCRAID_VSET_BUS_ID :
		 (RES_IS_GSCSI(*cfg_entry) ? PMCRAID_PHYS_BUS_ID :
			RES_BUS(cfg_entry->resource_address)),
		 RES_IS_VSET(*cfg_entry) ?
			(fw_version <= PMCRAID_FW_VERSION_1 ?
				cfg_entry->unique_flags1 :
					cfg_entry->array_id & 0xFF) :
			RES_TARGET(cfg_entry->resource_address),
		 RES_LUN(cfg_entry->resource_address));


	
	if (pinstance->ccn.hcam->notification_lost) {
		cfgcmd = pmcraid_get_free_cmd(pinstance);
		if (cfgcmd) {
			pmcraid_info("lost CCN, reading config table\b");
			pinstance->reinit_cfg_table = 1;
			pmcraid_querycfg(cfgcmd);
		} else {
			pmcraid_err("lost CCN, no free cmd for querycfg\n");
		}
		goto out_notify_apps;
	}

	if (pinstance->ccn.hcam->notification_type ==
	    NOTIFICATION_TYPE_ENTRY_CHANGED &&
	    cfg_entry->resource_type == RES_TYPE_VSET) {

		if (fw_version <= PMCRAID_FW_VERSION_1)
			hidden_entry = (cfg_entry->unique_flags1 & 0x80) != 0;
		else
			hidden_entry = (cfg_entry->unique_flags1 & 0x80) != 0;

	} else if (!pmcraid_expose_resource(fw_version, cfg_entry)) {
		goto out_notify_apps;
	}

	spin_lock_irqsave(&pinstance->resource_lock, lock_flags);
	list_for_each_entry(res, &pinstance->used_res_q, queue) {
		rc = memcmp(&res->cfg_entry.resource_address,
			    &cfg_entry->resource_address,
			    sizeof(cfg_entry->resource_address));
		if (!rc) {
			new_entry = 0;
			break;
		}
	}

	if (new_entry) {

		if (hidden_entry) {
			spin_unlock_irqrestore(&pinstance->resource_lock,
						lock_flags);
			goto out_notify_apps;
		}

		if (list_empty(&pinstance->free_res_q)) {
			spin_unlock_irqrestore(&pinstance->resource_lock,
						lock_flags);
			pmcraid_err("too many resources attached\n");
			spin_lock_irqsave(pinstance->host->host_lock,
					  host_lock_flags);
			pmcraid_send_hcam(pinstance,
					  PMCRAID_HCAM_CODE_CONFIG_CHANGE);
			spin_unlock_irqrestore(pinstance->host->host_lock,
					       host_lock_flags);
			return;
		}

		res = list_entry(pinstance->free_res_q.next,
				 struct pmcraid_resource_entry, queue);

		list_del(&res->queue);
		res->scsi_dev = NULL;
		res->reset_progress = 0;
		list_add_tail(&res->queue, &pinstance->used_res_q);
	}

	memcpy(&res->cfg_entry, cfg_entry, pinstance->config_table_entry_size);

	if (pinstance->ccn.hcam->notification_type ==
	    NOTIFICATION_TYPE_ENTRY_DELETED || hidden_entry) {
		if (res->scsi_dev) {
			if (fw_version <= PMCRAID_FW_VERSION_1)
				res->cfg_entry.unique_flags1 &= 0x7F;
			else
				res->cfg_entry.array_id &= 0xFF;
			res->change_detected = RES_CHANGE_DEL;
			res->cfg_entry.resource_handle =
				PMCRAID_INVALID_RES_HANDLE;
			schedule_work(&pinstance->worker_q);
		} else {
			
			list_move_tail(&res->queue, &pinstance->free_res_q);
		}
	} else if (!res->scsi_dev) {
		res->change_detected = RES_CHANGE_ADD;
		schedule_work(&pinstance->worker_q);
	}
	spin_unlock_irqrestore(&pinstance->resource_lock, lock_flags);

out_notify_apps:

	
	if (!pmcraid_disable_aen)
		pmcraid_notify_ccn(pinstance);

	cmd = pmcraid_init_hcam(pinstance, PMCRAID_HCAM_CODE_CONFIG_CHANGE);
	if (cmd)
		pmcraid_send_hcam_cmd(cmd);
}

static struct pmcraid_ioasc_error *pmcraid_get_error_info(u32 ioasc)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(pmcraid_ioasc_error_table); i++) {
		if (pmcraid_ioasc_error_table[i].ioasc_code == ioasc)
			return &pmcraid_ioasc_error_table[i];
	}
	return NULL;
}

void pmcraid_ioasc_logger(u32 ioasc, struct pmcraid_cmd *cmd)
{
	struct pmcraid_ioasc_error *error_info = pmcraid_get_error_info(ioasc);

	if (error_info == NULL ||
		cmd->drv_inst->current_log_level < error_info->log_level)
		return;

	
	pmcraid_err("cmd [%x] for resource %x failed with %x(%s)\n",
		cmd->ioa_cb->ioarcb.cdb[0],
		cmd->ioa_cb->ioarcb.resource_handle,
		le32_to_cpu(ioasc), error_info->error_string);
}

static void pmcraid_handle_error_log(struct pmcraid_instance *pinstance)
{
	struct pmcraid_hcam_ldn *hcam_ldn;
	u32 ioasc;

	hcam_ldn = (struct pmcraid_hcam_ldn *)pinstance->ldn.hcam;

	pmcraid_info
		("LDN(%x): %x type: %x lost: %x flags: %x overlay id: %x\n",
		 pinstance->ldn.hcam->ilid,
		 pinstance->ldn.hcam->op_code,
		 pinstance->ldn.hcam->notification_type,
		 pinstance->ldn.hcam->notification_lost,
		 pinstance->ldn.hcam->flags,
		 pinstance->ldn.hcam->overlay_id);

	
	if (pinstance->ldn.hcam->notification_type !=
	    NOTIFICATION_TYPE_ERROR_LOG)
		return;

	if (pinstance->ldn.hcam->notification_lost ==
	    HOSTRCB_NOTIFICATIONS_LOST)
		dev_info(&pinstance->pdev->dev, "Error notifications lost\n");

	ioasc = le32_to_cpu(hcam_ldn->error_log.fd_ioasc);

	if (ioasc == PMCRAID_IOASC_UA_BUS_WAS_RESET ||
		ioasc == PMCRAID_IOASC_UA_BUS_WAS_RESET_BY_OTHER) {
		dev_info(&pinstance->pdev->dev,
			"UnitAttention due to IOA Bus Reset\n");
		scsi_report_bus_reset(
			pinstance->host,
			RES_BUS(hcam_ldn->error_log.fd_ra));
	}

	return;
}

static void pmcraid_process_ccn(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	u32 ioasc = le32_to_cpu(cmd->ioa_cb->ioasa.ioasc);
	unsigned long lock_flags;

	pinstance->ccn.cmd = NULL;
	pmcraid_return_cmd(cmd);

	if (ioasc == PMCRAID_IOASC_IOA_WAS_RESET ||
	    atomic_read(&pinstance->ccn.ignore) == 1) {
		return;
	} else if (ioasc) {
		dev_info(&pinstance->pdev->dev,
			"Host RCB (CCN) failed with IOASC: 0x%08X\n", ioasc);
		spin_lock_irqsave(pinstance->host->host_lock, lock_flags);
		pmcraid_send_hcam(pinstance, PMCRAID_HCAM_CODE_CONFIG_CHANGE);
		spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);
	} else {
		pmcraid_handle_config_change(pinstance);
	}
}

static void pmcraid_initiate_reset(struct pmcraid_instance *);
static void pmcraid_set_timestamp(struct pmcraid_cmd *cmd);

static void pmcraid_process_ldn(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	struct pmcraid_hcam_ldn *ldn_hcam =
			(struct pmcraid_hcam_ldn *)pinstance->ldn.hcam;
	u32 ioasc = le32_to_cpu(cmd->ioa_cb->ioasa.ioasc);
	u32 fd_ioasc = le32_to_cpu(ldn_hcam->error_log.fd_ioasc);
	unsigned long lock_flags;

	
	pinstance->ldn.cmd = NULL;
	pmcraid_return_cmd(cmd);

	if (ioasc == PMCRAID_IOASC_IOA_WAS_RESET ||
	    atomic_read(&pinstance->ccn.ignore) == 1) {
		return;
	} else if (!ioasc) {
		pmcraid_handle_error_log(pinstance);
		if (fd_ioasc == PMCRAID_IOASC_NR_IOA_RESET_REQUIRED) {
			spin_lock_irqsave(pinstance->host->host_lock,
					  lock_flags);
			pmcraid_initiate_reset(pinstance);
			spin_unlock_irqrestore(pinstance->host->host_lock,
					       lock_flags);
			return;
		}
		if (fd_ioasc == PMCRAID_IOASC_TIME_STAMP_OUT_OF_SYNC) {
			pinstance->timestamp_error = 1;
			pmcraid_set_timestamp(cmd);
		}
	} else {
		dev_info(&pinstance->pdev->dev,
			"Host RCB(LDN) failed with IOASC: 0x%08X\n", ioasc);
	}
	
	if (!pmcraid_disable_aen)
		pmcraid_notify_ldn(pinstance);

	cmd = pmcraid_init_hcam(pinstance, PMCRAID_HCAM_CODE_LOG_DATA);
	if (cmd)
		pmcraid_send_hcam_cmd(cmd);
}

static void pmcraid_register_hcams(struct pmcraid_instance *pinstance)
{
	pmcraid_send_hcam(pinstance, PMCRAID_HCAM_CODE_CONFIG_CHANGE);
	pmcraid_send_hcam(pinstance, PMCRAID_HCAM_CODE_LOG_DATA);
}

static void pmcraid_unregister_hcams(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;

	atomic_set(&pinstance->ccn.ignore, 1);
	atomic_set(&pinstance->ldn.ignore, 1);

	if ((pinstance->force_ioa_reset && !pinstance->ioa_bringdown) ||
	     pinstance->ioa_unit_check) {
		pinstance->force_ioa_reset = 0;
		pinstance->ioa_unit_check = 0;
		pinstance->ioa_state = IOA_STATE_IN_RESET_ALERT;
		pmcraid_reset_alert(cmd);
		return;
	}

	pmcraid_cancel_ldn(cmd);
}

static void pmcraid_reinit_buffers(struct pmcraid_instance *);

static int pmcraid_reset_enable_ioa(struct pmcraid_instance *pinstance)
{
	u32 intrs;

	pmcraid_reinit_buffers(pinstance);
	intrs = pmcraid_read_interrupts(pinstance);

	pmcraid_enable_interrupts(pinstance, PMCRAID_PCI_INTERRUPTS);

	if (intrs & INTRS_TRANSITION_TO_OPERATIONAL) {
		if (!pinstance->interrupt_mode) {
			iowrite32(INTRS_TRANSITION_TO_OPERATIONAL,
				pinstance->int_regs.
				ioa_host_interrupt_mask_reg);
			iowrite32(INTRS_TRANSITION_TO_OPERATIONAL,
				pinstance->int_regs.ioa_host_interrupt_clr_reg);
		}
		return 1;
	} else {
		return 0;
	}
}

static void pmcraid_soft_reset(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	u32 int_reg;
	u32 doorbell;

	cmd->cmd_done = pmcraid_ioa_reset;
	cmd->timer.data = (unsigned long)cmd;
	cmd->timer.expires = jiffies +
			     msecs_to_jiffies(PMCRAID_TRANSOP_TIMEOUT);
	cmd->timer.function = (void (*)(unsigned long))pmcraid_timeout_handler;

	if (!timer_pending(&cmd->timer))
		add_timer(&cmd->timer);

	doorbell = DOORBELL_RUNTIME_RESET |
		   DOORBELL_ENABLE_DESTRUCTIVE_DIAGS;

	if (pinstance->interrupt_mode) {
		iowrite32(DOORBELL_INTR_MODE_MSIX,
			  pinstance->int_regs.host_ioa_interrupt_reg);
		ioread32(pinstance->int_regs.host_ioa_interrupt_reg);
	}

	iowrite32(doorbell, pinstance->int_regs.host_ioa_interrupt_reg);
	ioread32(pinstance->int_regs.host_ioa_interrupt_reg),
	int_reg = ioread32(pinstance->int_regs.ioa_host_interrupt_reg);

	pmcraid_info("Waiting for IOA to become operational %x:%x\n",
		     ioread32(pinstance->int_regs.host_ioa_interrupt_reg),
		     int_reg);
}

static void pmcraid_get_dump(struct pmcraid_instance *pinstance)
{
	pmcraid_info("%s is not yet implemented\n", __func__);
}

static void pmcraid_fail_outstanding_cmds(struct pmcraid_instance *pinstance)
{
	struct pmcraid_cmd *cmd, *temp;
	unsigned long lock_flags;

	spin_lock_irqsave(&pinstance->pending_pool_lock, lock_flags);
	list_for_each_entry_safe(cmd, temp, &pinstance->pending_cmd_pool,
				 free_list) {
		list_del(&cmd->free_list);
		spin_unlock_irqrestore(&pinstance->pending_pool_lock,
					lock_flags);
		cmd->ioa_cb->ioasa.ioasc =
			cpu_to_le32(PMCRAID_IOASC_IOA_WAS_RESET);
		cmd->ioa_cb->ioasa.ilid =
			cpu_to_be32(PMCRAID_DRIVER_ILID);

		
		del_timer(&cmd->timer);

		if (cmd->scsi_cmd) {

			struct scsi_cmnd *scsi_cmd = cmd->scsi_cmd;
			__le32 resp = cmd->ioa_cb->ioarcb.response_handle;

			scsi_cmd->result |= DID_ERROR << 16;

			scsi_dma_unmap(scsi_cmd);
			pmcraid_return_cmd(cmd);

			pmcraid_info("failing(%d) CDB[0] = %x result: %x\n",
				     le32_to_cpu(resp) >> 2,
				     cmd->ioa_cb->ioarcb.cdb[0],
				     scsi_cmd->result);
			scsi_cmd->scsi_done(scsi_cmd);
		} else if (cmd->cmd_done == pmcraid_internal_done ||
			   cmd->cmd_done == pmcraid_erp_done) {
			cmd->cmd_done(cmd);
		} else if (cmd->cmd_done != pmcraid_ioa_reset &&
			   cmd->cmd_done != pmcraid_ioa_shutdown_done) {
			pmcraid_return_cmd(cmd);
		}

		atomic_dec(&pinstance->outstanding_cmds);
		spin_lock_irqsave(&pinstance->pending_pool_lock, lock_flags);
	}

	spin_unlock_irqrestore(&pinstance->pending_pool_lock, lock_flags);
}

static void pmcraid_ioa_reset(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	u8 reset_complete = 0;

	pinstance->ioa_reset_in_progress = 1;

	if (pinstance->reset_cmd != cmd) {
		pmcraid_err("reset is called with different command block\n");
		pinstance->reset_cmd = cmd;
	}

	pmcraid_info("reset_engine: state = %d, command = %p\n",
		      pinstance->ioa_state, cmd);

	switch (pinstance->ioa_state) {

	case IOA_STATE_DEAD:
		pmcraid_err("IOA is offline no reset is possible\n");
		reset_complete = 1;
		break;

	case IOA_STATE_IN_BRINGDOWN:
		pmcraid_disable_interrupts(pinstance, ~0);
		pinstance->ioa_state = IOA_STATE_IN_RESET_ALERT;
		pmcraid_reset_alert(cmd);
		break;

	case IOA_STATE_UNKNOWN:
		scsi_block_requests(pinstance->host);

		if (pinstance->ioa_hard_reset == 0) {
			if (ioread32(pinstance->ioa_status) &
			    INTRS_TRANSITION_TO_OPERATIONAL) {
				pmcraid_info("sticky bit set, bring-up\n");
				pinstance->ioa_state = IOA_STATE_IN_BRINGUP;
				pmcraid_reinit_cmdblk(cmd);
				pmcraid_identify_hrrq(cmd);
			} else {
				pinstance->ioa_state = IOA_STATE_IN_SOFT_RESET;
				pmcraid_soft_reset(cmd);
			}
		} else {
			pinstance->ioa_state = IOA_STATE_IN_RESET_ALERT;
			pmcraid_reset_alert(cmd);
		}
		break;

	case IOA_STATE_IN_RESET_ALERT:
		pinstance->ioa_state = IOA_STATE_IN_HARD_RESET;
		pmcraid_start_bist(cmd);
		break;

	case IOA_STATE_IN_HARD_RESET:
		pinstance->ioa_reset_attempts++;

		
		if (pinstance->ioa_reset_attempts > PMCRAID_RESET_ATTEMPTS) {
			pinstance->ioa_reset_attempts = 0;
			pmcraid_err("IOA didn't respond marking it as dead\n");
			pinstance->ioa_state = IOA_STATE_DEAD;

			if (pinstance->ioa_bringdown)
				pmcraid_notify_ioastate(pinstance,
					PMC_DEVICE_EVENT_SHUTDOWN_FAILED);
			else
				pmcraid_notify_ioastate(pinstance,
						PMC_DEVICE_EVENT_RESET_FAILED);
			reset_complete = 1;
			break;
		}

		pci_restore_state(pinstance->pdev);

		
		pmcraid_fail_outstanding_cmds(pinstance);

		
		if (pinstance->ioa_unit_check) {
			pmcraid_info("unit check is active\n");
			pinstance->ioa_unit_check = 0;
			pmcraid_get_dump(pinstance);
			pinstance->ioa_reset_attempts--;
			pinstance->ioa_state = IOA_STATE_IN_RESET_ALERT;
			pmcraid_reset_alert(cmd);
			break;
		}

		if (pinstance->ioa_bringdown) {
			pmcraid_info("bringing down the adapter\n");
			pinstance->ioa_shutdown_type = SHUTDOWN_NONE;
			pinstance->ioa_bringdown = 0;
			pinstance->ioa_state = IOA_STATE_UNKNOWN;
			pmcraid_notify_ioastate(pinstance,
					PMC_DEVICE_EVENT_SHUTDOWN_SUCCESS);
			reset_complete = 1;
		} else {
			if (pmcraid_reset_enable_ioa(pinstance)) {
				pinstance->ioa_state = IOA_STATE_IN_BRINGUP;
				pmcraid_info("bringing up the adapter\n");
				pmcraid_reinit_cmdblk(cmd);
				pmcraid_identify_hrrq(cmd);
			} else {
				pinstance->ioa_state = IOA_STATE_IN_SOFT_RESET;
				pmcraid_soft_reset(cmd);
			}
		}
		break;

	case IOA_STATE_IN_SOFT_RESET:
		pmcraid_info("In softreset proceeding with bring-up\n");
		pinstance->ioa_state = IOA_STATE_IN_BRINGUP;

		pmcraid_identify_hrrq(cmd);
		break;

	case IOA_STATE_IN_BRINGUP:
		pinstance->ioa_state = IOA_STATE_OPERATIONAL;
		reset_complete = 1;
		break;

	case IOA_STATE_OPERATIONAL:
	default:
		if (pinstance->ioa_shutdown_type == SHUTDOWN_NONE &&
		    pinstance->force_ioa_reset == 0) {
			pmcraid_notify_ioastate(pinstance,
						PMC_DEVICE_EVENT_RESET_SUCCESS);
			reset_complete = 1;
		} else {
			if (pinstance->ioa_shutdown_type != SHUTDOWN_NONE)
				pinstance->ioa_state = IOA_STATE_IN_BRINGDOWN;
			pmcraid_reinit_cmdblk(cmd);
			pmcraid_unregister_hcams(cmd);
		}
		break;
	}

	if (reset_complete) {
		pinstance->ioa_reset_in_progress = 0;
		pinstance->ioa_reset_attempts = 0;
		pinstance->reset_cmd = NULL;
		pinstance->ioa_shutdown_type = SHUTDOWN_NONE;
		pinstance->ioa_bringdown = 0;
		pmcraid_return_cmd(cmd);

		if (pinstance->ioa_state == IOA_STATE_OPERATIONAL)
			pmcraid_register_hcams(pinstance);

		wake_up_all(&pinstance->reset_wait_q);
	}

	return;
}

static void pmcraid_initiate_reset(struct pmcraid_instance *pinstance)
{
	struct pmcraid_cmd *cmd;

	if (!pinstance->ioa_reset_in_progress) {
		scsi_block_requests(pinstance->host);
		cmd = pmcraid_get_free_cmd(pinstance);

		if (cmd == NULL) {
			pmcraid_err("no cmnd blocks for initiate_reset\n");
			return;
		}

		pinstance->ioa_shutdown_type = SHUTDOWN_NONE;
		pinstance->reset_cmd = cmd;
		pinstance->force_ioa_reset = 1;
		pmcraid_notify_ioastate(pinstance,
					PMC_DEVICE_EVENT_RESET_START);
		pmcraid_ioa_reset(cmd);
	}
}

static int pmcraid_reset_reload(
	struct pmcraid_instance *pinstance,
	u8 shutdown_type,
	u8 target_state
)
{
	struct pmcraid_cmd *reset_cmd = NULL;
	unsigned long lock_flags;
	int reset = 1;

	spin_lock_irqsave(pinstance->host->host_lock, lock_flags);

	if (pinstance->ioa_reset_in_progress) {
		pmcraid_info("reset_reload: reset is already in progress\n");

		spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);

		wait_event(pinstance->reset_wait_q,
			   !pinstance->ioa_reset_in_progress);

		spin_lock_irqsave(pinstance->host->host_lock, lock_flags);

		if (pinstance->ioa_state == IOA_STATE_DEAD) {
			spin_unlock_irqrestore(pinstance->host->host_lock,
					       lock_flags);
			pmcraid_info("reset_reload: IOA is dead\n");
			return reset;
		} else if (pinstance->ioa_state == target_state) {
			reset = 0;
		}
	}

	if (reset) {
		pmcraid_info("reset_reload: proceeding with reset\n");
		scsi_block_requests(pinstance->host);
		reset_cmd = pmcraid_get_free_cmd(pinstance);

		if (reset_cmd == NULL) {
			pmcraid_err("no free cmnd for reset_reload\n");
			spin_unlock_irqrestore(pinstance->host->host_lock,
					       lock_flags);
			return reset;
		}

		if (shutdown_type == SHUTDOWN_NORMAL)
			pinstance->ioa_bringdown = 1;

		pinstance->ioa_shutdown_type = shutdown_type;
		pinstance->reset_cmd = reset_cmd;
		pinstance->force_ioa_reset = reset;
		pmcraid_info("reset_reload: initiating reset\n");
		pmcraid_ioa_reset(reset_cmd);
		spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);
		pmcraid_info("reset_reload: waiting for reset to complete\n");
		wait_event(pinstance->reset_wait_q,
			   !pinstance->ioa_reset_in_progress);

		pmcraid_info("reset_reload: reset is complete !!\n");
		scsi_unblock_requests(pinstance->host);
		if (pinstance->ioa_state == target_state)
			reset = 0;
	}

	return reset;
}

static int pmcraid_reset_bringdown(struct pmcraid_instance *pinstance)
{
	return pmcraid_reset_reload(pinstance,
				    SHUTDOWN_NORMAL,
				    IOA_STATE_UNKNOWN);
}

static int pmcraid_reset_bringup(struct pmcraid_instance *pinstance)
{
	pmcraid_notify_ioastate(pinstance, PMC_DEVICE_EVENT_RESET_START);

	return pmcraid_reset_reload(pinstance,
				    SHUTDOWN_NONE,
				    IOA_STATE_OPERATIONAL);
}

static void pmcraid_request_sense(struct pmcraid_cmd *cmd)
{
	struct pmcraid_ioarcb *ioarcb = &cmd->ioa_cb->ioarcb;
	struct pmcraid_ioadl_desc *ioadl = ioarcb->add_data.u.ioadl;

	
	cmd->sense_buffer = pci_alloc_consistent(cmd->drv_inst->pdev,
						 SCSI_SENSE_BUFFERSIZE,
						 &cmd->sense_buffer_dma);

	if (cmd->sense_buffer == NULL) {
		pmcraid_err
			("couldn't allocate sense buffer for request sense\n");
		pmcraid_erp_done(cmd);
		return;
	}

	
	memset(&cmd->ioa_cb->ioasa, 0, sizeof(struct pmcraid_ioasa));
	memset(ioarcb->cdb, 0, PMCRAID_MAX_CDB_LEN);
	ioarcb->request_flags0 = (SYNC_COMPLETE |
				  NO_LINK_DESCS |
				  INHIBIT_UL_CHECK);
	ioarcb->request_type = REQ_TYPE_SCSI;
	ioarcb->cdb[0] = REQUEST_SENSE;
	ioarcb->cdb[4] = SCSI_SENSE_BUFFERSIZE;

	ioarcb->ioadl_bus_addr = cpu_to_le64((cmd->ioa_cb_bus_addr) +
					offsetof(struct pmcraid_ioarcb,
						add_data.u.ioadl[0]));
	ioarcb->ioadl_length = cpu_to_le32(sizeof(struct pmcraid_ioadl_desc));

	ioarcb->data_transfer_length = cpu_to_le32(SCSI_SENSE_BUFFERSIZE);

	ioadl->address = cpu_to_le64(cmd->sense_buffer_dma);
	ioadl->data_len = cpu_to_le32(SCSI_SENSE_BUFFERSIZE);
	ioadl->flags = IOADL_FLAGS_LAST_DESC;

	pmcraid_send_cmd(cmd, pmcraid_erp_done,
			 PMCRAID_REQUEST_SENSE_TIMEOUT,
			 pmcraid_timeout_handler);
}

static void pmcraid_cancel_all(struct pmcraid_cmd *cmd, u32 sense)
{
	struct scsi_cmnd *scsi_cmd = cmd->scsi_cmd;
	struct pmcraid_ioarcb *ioarcb = &cmd->ioa_cb->ioarcb;
	struct pmcraid_resource_entry *res = scsi_cmd->device->hostdata;
	void (*cmd_done) (struct pmcraid_cmd *) = sense ? pmcraid_erp_done
							: pmcraid_request_sense;

	memset(ioarcb->cdb, 0, PMCRAID_MAX_CDB_LEN);
	ioarcb->request_flags0 = SYNC_OVERRIDE;
	ioarcb->request_type = REQ_TYPE_IOACMD;
	ioarcb->cdb[0] = PMCRAID_CANCEL_ALL_REQUESTS;

	if (RES_IS_GSCSI(res->cfg_entry))
		ioarcb->cdb[1] = PMCRAID_SYNC_COMPLETE_AFTER_CANCEL;

	ioarcb->ioadl_bus_addr = 0;
	ioarcb->ioadl_length = 0;
	ioarcb->data_transfer_length = 0;
	ioarcb->ioarcb_bus_addr &= (~0x1FULL);

	pmcraid_send_cmd(cmd, cmd_done,
			 PMCRAID_REQUEST_SENSE_TIMEOUT,
			 pmcraid_timeout_handler);
}

static void pmcraid_frame_auto_sense(struct pmcraid_cmd *cmd)
{
	u8 *sense_buf = cmd->scsi_cmd->sense_buffer;
	struct pmcraid_resource_entry *res = cmd->scsi_cmd->device->hostdata;
	struct pmcraid_ioasa *ioasa = &cmd->ioa_cb->ioasa;
	u32 ioasc = le32_to_cpu(ioasa->ioasc);
	u32 failing_lba = 0;

	memset(sense_buf, 0, SCSI_SENSE_BUFFERSIZE);
	cmd->scsi_cmd->result = SAM_STAT_CHECK_CONDITION;

	if (RES_IS_VSET(res->cfg_entry) &&
	    ioasc == PMCRAID_IOASC_ME_READ_ERROR_NO_REALLOC &&
	    ioasa->u.vset.failing_lba_hi != 0) {

		sense_buf[0] = 0x72;
		sense_buf[1] = PMCRAID_IOASC_SENSE_KEY(ioasc);
		sense_buf[2] = PMCRAID_IOASC_SENSE_CODE(ioasc);
		sense_buf[3] = PMCRAID_IOASC_SENSE_QUAL(ioasc);

		sense_buf[7] = 12;
		sense_buf[8] = 0;
		sense_buf[9] = 0x0A;
		sense_buf[10] = 0x80;

		failing_lba = le32_to_cpu(ioasa->u.vset.failing_lba_hi);

		sense_buf[12] = (failing_lba & 0xff000000) >> 24;
		sense_buf[13] = (failing_lba & 0x00ff0000) >> 16;
		sense_buf[14] = (failing_lba & 0x0000ff00) >> 8;
		sense_buf[15] = failing_lba & 0x000000ff;

		failing_lba = le32_to_cpu(ioasa->u.vset.failing_lba_lo);

		sense_buf[16] = (failing_lba & 0xff000000) >> 24;
		sense_buf[17] = (failing_lba & 0x00ff0000) >> 16;
		sense_buf[18] = (failing_lba & 0x0000ff00) >> 8;
		sense_buf[19] = failing_lba & 0x000000ff;
	} else {
		sense_buf[0] = 0x70;
		sense_buf[2] = PMCRAID_IOASC_SENSE_KEY(ioasc);
		sense_buf[12] = PMCRAID_IOASC_SENSE_CODE(ioasc);
		sense_buf[13] = PMCRAID_IOASC_SENSE_QUAL(ioasc);

		if (ioasc == PMCRAID_IOASC_ME_READ_ERROR_NO_REALLOC) {
			if (RES_IS_VSET(res->cfg_entry))
				failing_lba =
					le32_to_cpu(ioasa->u.
						 vset.failing_lba_lo);
			sense_buf[0] |= 0x80;
			sense_buf[3] = (failing_lba >> 24) & 0xff;
			sense_buf[4] = (failing_lba >> 16) & 0xff;
			sense_buf[5] = (failing_lba >> 8) & 0xff;
			sense_buf[6] = failing_lba & 0xff;
		}

		sense_buf[7] = 6; 
	}
}

static int pmcraid_error_handler(struct pmcraid_cmd *cmd)
{
	struct scsi_cmnd *scsi_cmd = cmd->scsi_cmd;
	struct pmcraid_resource_entry *res = scsi_cmd->device->hostdata;
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	struct pmcraid_ioasa *ioasa = &cmd->ioa_cb->ioasa;
	u32 ioasc = le32_to_cpu(ioasa->ioasc);
	u32 masked_ioasc = ioasc & PMCRAID_IOASC_SENSE_MASK;
	u32 sense_copied = 0;

	if (!res) {
		pmcraid_info("resource pointer is NULL\n");
		return 0;
	}

	
	if (SCSI_CMD_TYPE(scsi_cmd->cmnd[0]) == SCSI_READ_CMD)
		atomic_inc(&res->read_failures);
	else if (SCSI_CMD_TYPE(scsi_cmd->cmnd[0]) == SCSI_WRITE_CMD)
		atomic_inc(&res->write_failures);

	if (!RES_IS_GSCSI(res->cfg_entry) &&
		masked_ioasc != PMCRAID_IOASC_HW_DEVICE_BUS_STATUS_ERROR) {
		pmcraid_frame_auto_sense(cmd);
	}

	
	pmcraid_ioasc_logger(ioasc, cmd);

	switch (masked_ioasc) {

	case PMCRAID_IOASC_AC_TERMINATED_BY_HOST:
		scsi_cmd->result |= (DID_ABORT << 16);
		break;

	case PMCRAID_IOASC_IR_INVALID_RESOURCE_HANDLE:
	case PMCRAID_IOASC_HW_CANNOT_COMMUNICATE:
		scsi_cmd->result |= (DID_NO_CONNECT << 16);
		break;

	case PMCRAID_IOASC_NR_SYNC_REQUIRED:
		res->sync_reqd = 1;
		scsi_cmd->result |= (DID_IMM_RETRY << 16);
		break;

	case PMCRAID_IOASC_ME_READ_ERROR_NO_REALLOC:
		scsi_cmd->result |= (DID_PASSTHROUGH << 16);
		break;

	case PMCRAID_IOASC_UA_BUS_WAS_RESET:
	case PMCRAID_IOASC_UA_BUS_WAS_RESET_BY_OTHER:
		if (!res->reset_progress)
			scsi_report_bus_reset(pinstance->host,
					      scsi_cmd->device->channel);
		scsi_cmd->result |= (DID_ERROR << 16);
		break;

	case PMCRAID_IOASC_HW_DEVICE_BUS_STATUS_ERROR:
		scsi_cmd->result |= PMCRAID_IOASC_SENSE_STATUS(ioasc);
		res->sync_reqd = 1;

		if (PMCRAID_IOASC_SENSE_STATUS(ioasc) !=
		    SAM_STAT_CHECK_CONDITION &&
		    PMCRAID_IOASC_SENSE_STATUS(ioasc) != SAM_STAT_ACA_ACTIVE)
			return 0;

		if (ioasa->auto_sense_length != 0) {
			short sense_len = ioasa->auto_sense_length;
			int data_size = min_t(u16, le16_to_cpu(sense_len),
					      SCSI_SENSE_BUFFERSIZE);

			memcpy(scsi_cmd->sense_buffer,
			       ioasa->sense_data,
			       data_size);
			sense_copied = 1;
		}

		if (RES_IS_GSCSI(res->cfg_entry))
			pmcraid_cancel_all(cmd, sense_copied);
		else if (sense_copied)
			pmcraid_erp_done(cmd);
		else
			pmcraid_request_sense(cmd);

		return 1;

	case PMCRAID_IOASC_NR_INIT_CMD_REQUIRED:
		break;

	default:
		if (PMCRAID_IOASC_SENSE_KEY(ioasc) > RECOVERED_ERROR)
			scsi_cmd->result |= (DID_ERROR << 16);
		break;
	}
	return 0;
}

static int pmcraid_reset_device(
	struct scsi_cmnd *scsi_cmd,
	unsigned long timeout,
	u8 modifier
)
{
	struct pmcraid_cmd *cmd;
	struct pmcraid_instance *pinstance;
	struct pmcraid_resource_entry *res;
	struct pmcraid_ioarcb *ioarcb;
	unsigned long lock_flags;
	u32 ioasc;

	pinstance =
		(struct pmcraid_instance *)scsi_cmd->device->host->hostdata;
	res = scsi_cmd->device->hostdata;

	if (!res) {
		sdev_printk(KERN_ERR, scsi_cmd->device,
			    "reset_device: NULL resource pointer\n");
		return FAILED;
	}

	spin_lock_irqsave(pinstance->host->host_lock, lock_flags);
	if (pinstance->ioa_reset_in_progress ||
	    pinstance->ioa_state == IOA_STATE_DEAD) {
		spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);
		return FAILED;
	}

	res->reset_progress = 1;
	pmcraid_info("Resetting %s resource with addr %x\n",
		     ((modifier & RESET_DEVICE_LUN) ? "LUN" :
		     ((modifier & RESET_DEVICE_TARGET) ? "TARGET" : "BUS")),
		     le32_to_cpu(res->cfg_entry.resource_address));

	
	cmd = pmcraid_get_free_cmd(pinstance);

	if (cmd == NULL) {
		spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);
		pmcraid_err("%s: no cmd blocks are available\n", __func__);
		return FAILED;
	}

	ioarcb = &cmd->ioa_cb->ioarcb;
	ioarcb->resource_handle = res->cfg_entry.resource_handle;
	ioarcb->request_type = REQ_TYPE_IOACMD;
	ioarcb->cdb[0] = PMCRAID_RESET_DEVICE;

	
	if (modifier)
		modifier = ENABLE_RESET_MODIFIER | modifier;

	ioarcb->cdb[1] = modifier;

	init_completion(&cmd->wait_for_completion);
	cmd->completion_req = 1;

	pmcraid_info("cmd(CDB[0] = %x) for %x with index = %d\n",
		     cmd->ioa_cb->ioarcb.cdb[0],
		     le32_to_cpu(cmd->ioa_cb->ioarcb.resource_handle),
		     le32_to_cpu(cmd->ioa_cb->ioarcb.response_handle) >> 2);

	pmcraid_send_cmd(cmd,
			 pmcraid_internal_done,
			 timeout,
			 pmcraid_timeout_handler);

	spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);

	wait_for_completion(&cmd->wait_for_completion);

	pmcraid_return_cmd(cmd);
	res->reset_progress = 0;
	ioasc = le32_to_cpu(cmd->ioa_cb->ioasa.ioasc);

	
	return PMCRAID_IOASC_SENSE_KEY(ioasc) ? FAILED : SUCCESS;
}


static int _pmcraid_io_done(struct pmcraid_cmd *cmd, int reslen, int ioasc)
{
	struct scsi_cmnd *scsi_cmd = cmd->scsi_cmd;
	int rc = 0;

	scsi_set_resid(scsi_cmd, reslen);

	pmcraid_info("response(%d) CDB[0] = %x ioasc:result: %x:%x\n",
		le32_to_cpu(cmd->ioa_cb->ioarcb.response_handle) >> 2,
		cmd->ioa_cb->ioarcb.cdb[0],
		ioasc, scsi_cmd->result);

	if (PMCRAID_IOASC_SENSE_KEY(ioasc) != 0)
		rc = pmcraid_error_handler(cmd);

	if (rc == 0) {
		scsi_dma_unmap(scsi_cmd);
		scsi_cmd->scsi_done(scsi_cmd);
	}

	return rc;
}


static void pmcraid_io_done(struct pmcraid_cmd *cmd)
{
	u32 ioasc = le32_to_cpu(cmd->ioa_cb->ioasa.ioasc);
	u32 reslen = le32_to_cpu(cmd->ioa_cb->ioasa.residual_data_length);

	if (_pmcraid_io_done(cmd, reslen, ioasc) == 0)
		pmcraid_return_cmd(cmd);
}

static struct pmcraid_cmd *pmcraid_abort_cmd(struct pmcraid_cmd *cmd)
{
	struct pmcraid_cmd *cancel_cmd;
	struct pmcraid_instance *pinstance;
	struct pmcraid_resource_entry *res;

	pinstance = (struct pmcraid_instance *)cmd->drv_inst;
	res = cmd->scsi_cmd->device->hostdata;

	cancel_cmd = pmcraid_get_free_cmd(pinstance);

	if (cancel_cmd == NULL) {
		pmcraid_err("%s: no cmd blocks are available\n", __func__);
		return NULL;
	}

	pmcraid_prepare_cancel_cmd(cancel_cmd, cmd);

	pmcraid_info("aborting command CDB[0]= %x with index = %d\n",
		cmd->ioa_cb->ioarcb.cdb[0],
		cmd->ioa_cb->ioarcb.response_handle >> 2);

	init_completion(&cancel_cmd->wait_for_completion);
	cancel_cmd->completion_req = 1;

	pmcraid_info("command (%d) CDB[0] = %x for %x\n",
		le32_to_cpu(cancel_cmd->ioa_cb->ioarcb.response_handle) >> 2,
		cancel_cmd->ioa_cb->ioarcb.cdb[0],
		le32_to_cpu(cancel_cmd->ioa_cb->ioarcb.resource_handle));

	pmcraid_send_cmd(cancel_cmd,
			 pmcraid_internal_done,
			 PMCRAID_INTERNAL_TIMEOUT,
			 pmcraid_timeout_handler);
	return cancel_cmd;
}

static int pmcraid_abort_complete(struct pmcraid_cmd *cancel_cmd)
{
	struct pmcraid_resource_entry *res;
	u32 ioasc;

	wait_for_completion(&cancel_cmd->wait_for_completion);
	res = cancel_cmd->res;
	cancel_cmd->res = NULL;
	ioasc = le32_to_cpu(cancel_cmd->ioa_cb->ioasa.ioasc);

	if (ioasc == PMCRAID_IOASC_UA_BUS_WAS_RESET ||
	    ioasc == PMCRAID_IOASC_NR_SYNC_REQUIRED) {
		if (ioasc == PMCRAID_IOASC_NR_SYNC_REQUIRED)
			res->sync_reqd = 1;
		ioasc = 0;
	}

	
	pmcraid_return_cmd(cancel_cmd);
	return PMCRAID_IOASC_SENSE_KEY(ioasc) ? FAILED : SUCCESS;
}

static int pmcraid_eh_abort_handler(struct scsi_cmnd *scsi_cmd)
{
	struct pmcraid_instance *pinstance;
	struct pmcraid_cmd *cmd;
	struct pmcraid_resource_entry *res;
	unsigned long host_lock_flags;
	unsigned long pending_lock_flags;
	struct pmcraid_cmd *cancel_cmd = NULL;
	int cmd_found = 0;
	int rc = FAILED;

	pinstance =
		(struct pmcraid_instance *)scsi_cmd->device->host->hostdata;

	scmd_printk(KERN_INFO, scsi_cmd,
		    "I/O command timed out, aborting it.\n");

	res = scsi_cmd->device->hostdata;

	if (res == NULL)
		return rc;

	spin_lock_irqsave(pinstance->host->host_lock, host_lock_flags);

	if (pinstance->ioa_reset_in_progress ||
	    pinstance->ioa_state == IOA_STATE_DEAD) {
		spin_unlock_irqrestore(pinstance->host->host_lock,
				       host_lock_flags);
		return rc;
	}

	spin_lock_irqsave(&pinstance->pending_pool_lock, pending_lock_flags);
	list_for_each_entry(cmd, &pinstance->pending_cmd_pool, free_list) {

		if (cmd->scsi_cmd == scsi_cmd) {
			cmd_found = 1;
			break;
		}
	}

	spin_unlock_irqrestore(&pinstance->pending_pool_lock,
				pending_lock_flags);

	if (cmd_found)
		cancel_cmd = pmcraid_abort_cmd(cmd);

	spin_unlock_irqrestore(pinstance->host->host_lock,
			       host_lock_flags);

	if (cancel_cmd) {
		cancel_cmd->res = cmd->scsi_cmd->device->hostdata;
		rc = pmcraid_abort_complete(cancel_cmd);
	}

	return cmd_found ? rc : SUCCESS;
}

static int pmcraid_eh_device_reset_handler(struct scsi_cmnd *scmd)
{
	scmd_printk(KERN_INFO, scmd,
		    "resetting device due to an I/O command timeout.\n");
	return pmcraid_reset_device(scmd,
				    PMCRAID_INTERNAL_TIMEOUT,
				    RESET_DEVICE_LUN);
}

static int pmcraid_eh_bus_reset_handler(struct scsi_cmnd *scmd)
{
	scmd_printk(KERN_INFO, scmd,
		    "Doing bus reset due to an I/O command timeout.\n");
	return pmcraid_reset_device(scmd,
				    PMCRAID_RESET_BUS_TIMEOUT,
				    RESET_DEVICE_BUS);
}

static int pmcraid_eh_target_reset_handler(struct scsi_cmnd *scmd)
{
	scmd_printk(KERN_INFO, scmd,
		    "Doing target reset due to an I/O command timeout.\n");
	return pmcraid_reset_device(scmd,
				    PMCRAID_INTERNAL_TIMEOUT,
				    RESET_DEVICE_TARGET);
}

static int pmcraid_eh_host_reset_handler(struct scsi_cmnd *scmd)
{
	unsigned long interval = 10000; 
	int waits = jiffies_to_msecs(PMCRAID_RESET_HOST_TIMEOUT) / interval;
	struct pmcraid_instance *pinstance =
		(struct pmcraid_instance *)(scmd->device->host->hostdata);


	while (waits--) {
		if (atomic_read(&pinstance->outstanding_cmds) <=
		    PMCRAID_MAX_HCAM_CMD)
			return SUCCESS;
		msleep(interval);
	}

	dev_err(&pinstance->pdev->dev,
		"Adapter being reset due to an I/O command timeout.\n");
	return pmcraid_reset_bringup(pinstance) == 0 ? SUCCESS : FAILED;
}

static u8 pmcraid_task_attributes(struct scsi_cmnd *scsi_cmd)
{
	char tag[2];
	u8 rc = 0;

	if (scsi_populate_tag_msg(scsi_cmd, tag)) {
		switch (tag[0]) {
		case MSG_SIMPLE_TAG:
			rc = TASK_TAG_SIMPLE;
			break;
		case MSG_HEAD_TAG:
			rc = TASK_TAG_QUEUE_HEAD;
			break;
		case MSG_ORDERED_TAG:
			rc = TASK_TAG_ORDERED;
			break;
		};
	}

	return rc;
}


struct pmcraid_ioadl_desc *
pmcraid_init_ioadls(struct pmcraid_cmd *cmd, int sgcount)
{
	struct pmcraid_ioadl_desc *ioadl;
	struct pmcraid_ioarcb *ioarcb = &cmd->ioa_cb->ioarcb;
	int ioadl_count = 0;

	if (ioarcb->add_cmd_param_length)
		ioadl_count = DIV_ROUND_UP(ioarcb->add_cmd_param_length, 16);
	ioarcb->ioadl_length =
		sizeof(struct pmcraid_ioadl_desc) * sgcount;

	if ((sgcount + ioadl_count) > (ARRAY_SIZE(ioarcb->add_data.u.ioadl))) {
		ioarcb->ioarcb_bus_addr &= ~(0x1FULL);
		ioarcb->ioadl_bus_addr =
			cpu_to_le64((cmd->ioa_cb_bus_addr) +
				offsetof(struct pmcraid_ioarcb,
					add_data.u.ioadl[3]));
		ioadl = &ioarcb->add_data.u.ioadl[3];
	} else {
		ioarcb->ioadl_bus_addr =
			cpu_to_le64((cmd->ioa_cb_bus_addr) +
				offsetof(struct pmcraid_ioarcb,
					add_data.u.ioadl[ioadl_count]));

		ioadl = &ioarcb->add_data.u.ioadl[ioadl_count];
		ioarcb->ioarcb_bus_addr |=
				DIV_ROUND_CLOSEST(sgcount + ioadl_count, 8);
	}

	return ioadl;
}

static int pmcraid_build_ioadl(
	struct pmcraid_instance *pinstance,
	struct pmcraid_cmd *cmd
)
{
	int i, nseg;
	struct scatterlist *sglist;

	struct scsi_cmnd *scsi_cmd = cmd->scsi_cmd;
	struct pmcraid_ioarcb *ioarcb = &(cmd->ioa_cb->ioarcb);
	struct pmcraid_ioadl_desc *ioadl = ioarcb->add_data.u.ioadl;

	u32 length = scsi_bufflen(scsi_cmd);

	if (!length)
		return 0;

	nseg = scsi_dma_map(scsi_cmd);

	if (nseg < 0) {
		scmd_printk(KERN_ERR, scsi_cmd, "scsi_map_dma failed!\n");
		return -1;
	} else if (nseg > PMCRAID_MAX_IOADLS) {
		scsi_dma_unmap(scsi_cmd);
		scmd_printk(KERN_ERR, scsi_cmd,
			"sg count is (%d) more than allowed!\n", nseg);
		return -1;
	}

	
	if (scsi_cmd->sc_data_direction == DMA_TO_DEVICE)
		ioarcb->request_flags0 |= TRANSFER_DIR_WRITE;

	ioarcb->request_flags0 |= NO_LINK_DESCS;
	ioarcb->data_transfer_length = cpu_to_le32(length);
	ioadl = pmcraid_init_ioadls(cmd, nseg);

	
	scsi_for_each_sg(scsi_cmd, sglist, nseg, i) {
		ioadl[i].data_len = cpu_to_le32(sg_dma_len(sglist));
		ioadl[i].address = cpu_to_le64(sg_dma_address(sglist));
		ioadl[i].flags = 0;
	}
	
	ioadl[i - 1].flags = IOADL_FLAGS_LAST_DESC;

	return 0;
}

static void pmcraid_free_sglist(struct pmcraid_sglist *sglist)
{
	int i;

	for (i = 0; i < sglist->num_sg; i++)
		__free_pages(sg_page(&(sglist->scatterlist[i])),
			     sglist->order);

	kfree(sglist);
}

static struct pmcraid_sglist *pmcraid_alloc_sglist(int buflen)
{
	struct pmcraid_sglist *sglist;
	struct scatterlist *scatterlist;
	struct page *page;
	int num_elem, i, j;
	int sg_size;
	int order;
	int bsize_elem;

	sg_size = buflen / (PMCRAID_MAX_IOADLS - 1);
	order = (sg_size > 0) ? get_order(sg_size) : 0;
	bsize_elem = PAGE_SIZE * (1 << order);

	
	if (buflen % bsize_elem)
		num_elem = (buflen / bsize_elem) + 1;
	else
		num_elem = buflen / bsize_elem;

	
	sglist = kzalloc(sizeof(struct pmcraid_sglist) +
			 (sizeof(struct scatterlist) * (num_elem - 1)),
			 GFP_KERNEL);

	if (sglist == NULL)
		return NULL;

	scatterlist = sglist->scatterlist;
	sg_init_table(scatterlist, num_elem);
	sglist->order = order;
	sglist->num_sg = num_elem;
	sg_size = buflen;

	for (i = 0; i < num_elem; i++) {
		page = alloc_pages(GFP_KERNEL|GFP_DMA|__GFP_ZERO, order);
		if (!page) {
			for (j = i - 1; j >= 0; j--)
				__free_pages(sg_page(&scatterlist[j]), order);
			kfree(sglist);
			return NULL;
		}

		sg_set_page(&scatterlist[i], page,
			sg_size < bsize_elem ? sg_size : bsize_elem, 0);
		sg_size -= bsize_elem;
	}

	return sglist;
}

static int pmcraid_copy_sglist(
	struct pmcraid_sglist *sglist,
	unsigned long buffer,
	u32 len,
	int direction
)
{
	struct scatterlist *scatterlist;
	void *kaddr;
	int bsize_elem;
	int i;
	int rc = 0;

	
	bsize_elem = PAGE_SIZE * (1 << sglist->order);

	scatterlist = sglist->scatterlist;

	for (i = 0; i < (len / bsize_elem); i++, buffer += bsize_elem) {
		struct page *page = sg_page(&scatterlist[i]);

		kaddr = kmap(page);
		if (direction == DMA_TO_DEVICE)
			rc = __copy_from_user(kaddr,
					      (void *)buffer,
					      bsize_elem);
		else
			rc = __copy_to_user((void *)buffer, kaddr, bsize_elem);

		kunmap(page);

		if (rc) {
			pmcraid_err("failed to copy user data into sg list\n");
			return -EFAULT;
		}

		scatterlist[i].length = bsize_elem;
	}

	if (len % bsize_elem) {
		struct page *page = sg_page(&scatterlist[i]);

		kaddr = kmap(page);

		if (direction == DMA_TO_DEVICE)
			rc = __copy_from_user(kaddr,
					      (void *)buffer,
					      len % bsize_elem);
		else
			rc = __copy_to_user((void *)buffer,
					    kaddr,
					    len % bsize_elem);

		kunmap(page);

		scatterlist[i].length = len % bsize_elem;
	}

	if (rc) {
		pmcraid_err("failed to copy user data into sg list\n");
		rc = -EFAULT;
	}

	return rc;
}

static int pmcraid_queuecommand_lck(
	struct scsi_cmnd *scsi_cmd,
	void (*done) (struct scsi_cmnd *)
)
{
	struct pmcraid_instance *pinstance;
	struct pmcraid_resource_entry *res;
	struct pmcraid_ioarcb *ioarcb;
	struct pmcraid_cmd *cmd;
	u32 fw_version;
	int rc = 0;

	pinstance =
		(struct pmcraid_instance *)scsi_cmd->device->host->hostdata;
	fw_version = be16_to_cpu(pinstance->inq_data->fw_version);
	scsi_cmd->scsi_done = done;
	res = scsi_cmd->device->hostdata;
	scsi_cmd->result = (DID_OK << 16);

	if (pinstance->ioa_state == IOA_STATE_DEAD) {
		pmcraid_info("IOA is dead, but queuecommand is scheduled\n");
		scsi_cmd->result = (DID_NO_CONNECT << 16);
		scsi_cmd->scsi_done(scsi_cmd);
		return 0;
	}

	
	if (pinstance->ioa_reset_in_progress)
		return SCSI_MLQUEUE_HOST_BUSY;

	if (scsi_cmd->cmnd[0] == SYNCHRONIZE_CACHE) {
		pmcraid_info("SYNC_CACHE(0x35), completing in driver itself\n");
		scsi_cmd->scsi_done(scsi_cmd);
		return 0;
	}

	
	cmd = pmcraid_get_free_cmd(pinstance);

	if (cmd == NULL) {
		pmcraid_err("free command block is not available\n");
		return SCSI_MLQUEUE_HOST_BUSY;
	}

	cmd->scsi_cmd = scsi_cmd;
	ioarcb = &(cmd->ioa_cb->ioarcb);
	memcpy(ioarcb->cdb, scsi_cmd->cmnd, scsi_cmd->cmd_len);
	ioarcb->resource_handle = res->cfg_entry.resource_handle;
	ioarcb->request_type = REQ_TYPE_SCSI;

	ioarcb->hrrq_id = atomic_add_return(1, &(pinstance->last_message_id)) %
			  pinstance->num_hrrq;
	cmd->cmd_done = pmcraid_io_done;

	if (RES_IS_GSCSI(res->cfg_entry) || RES_IS_VSET(res->cfg_entry)) {
		if (scsi_cmd->underflow == 0)
			ioarcb->request_flags0 |= INHIBIT_UL_CHECK;

		if (res->sync_reqd) {
			ioarcb->request_flags0 |= SYNC_COMPLETE;
			res->sync_reqd = 0;
		}

		ioarcb->request_flags0 |= NO_LINK_DESCS;
		ioarcb->request_flags1 |= pmcraid_task_attributes(scsi_cmd);

		if (RES_IS_GSCSI(res->cfg_entry))
			ioarcb->request_flags1 |= DELAY_AFTER_RESET;
	}

	rc = pmcraid_build_ioadl(pinstance, cmd);

	pmcraid_info("command (%d) CDB[0] = %x for %x:%x:%x:%x\n",
		     le32_to_cpu(ioarcb->response_handle) >> 2,
		     scsi_cmd->cmnd[0], pinstance->host->unique_id,
		     RES_IS_VSET(res->cfg_entry) ? PMCRAID_VSET_BUS_ID :
			PMCRAID_PHYS_BUS_ID,
		     RES_IS_VSET(res->cfg_entry) ?
			(fw_version <= PMCRAID_FW_VERSION_1 ?
				res->cfg_entry.unique_flags1 :
					res->cfg_entry.array_id & 0xFF) :
			RES_TARGET(res->cfg_entry.resource_address),
		     RES_LUN(res->cfg_entry.resource_address));

	if (likely(rc == 0)) {
		_pmcraid_fire_command(cmd);
	} else {
		pmcraid_err("queuecommand could not build ioadl\n");
		pmcraid_return_cmd(cmd);
		rc = SCSI_MLQUEUE_HOST_BUSY;
	}

	return rc;
}

static DEF_SCSI_QCMD(pmcraid_queuecommand)

static int pmcraid_chr_open(struct inode *inode, struct file *filep)
{
	struct pmcraid_instance *pinstance;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	
	pinstance = container_of(inode->i_cdev, struct pmcraid_instance, cdev);
	filep->private_data = pinstance;

	return 0;
}

static int pmcraid_chr_release(struct inode *inode, struct file *filep)
{
	struct pmcraid_instance *pinstance = filep->private_data;

	filep->private_data = NULL;
	fasync_helper(-1, filep, 0, &pinstance->aen_queue);

	return 0;
}

static int pmcraid_chr_fasync(int fd, struct file *filep, int mode)
{
	struct pmcraid_instance *pinstance;
	int rc;

	pinstance = filep->private_data;
	mutex_lock(&pinstance->aen_queue_lock);
	rc = fasync_helper(fd, filep, mode, &pinstance->aen_queue);
	mutex_unlock(&pinstance->aen_queue_lock);

	return rc;
}


static int pmcraid_build_passthrough_ioadls(
	struct pmcraid_cmd *cmd,
	int buflen,
	int direction
)
{
	struct pmcraid_sglist *sglist = NULL;
	struct scatterlist *sg = NULL;
	struct pmcraid_ioarcb *ioarcb = &cmd->ioa_cb->ioarcb;
	struct pmcraid_ioadl_desc *ioadl;
	int i;

	sglist = pmcraid_alloc_sglist(buflen);

	if (!sglist) {
		pmcraid_err("can't allocate memory for passthrough SGls\n");
		return -ENOMEM;
	}

	sglist->num_dma_sg = pci_map_sg(cmd->drv_inst->pdev,
					sglist->scatterlist,
					sglist->num_sg, direction);

	if (!sglist->num_dma_sg || sglist->num_dma_sg > PMCRAID_MAX_IOADLS) {
		dev_err(&cmd->drv_inst->pdev->dev,
			"Failed to map passthrough buffer!\n");
		pmcraid_free_sglist(sglist);
		return -EIO;
	}

	cmd->sglist = sglist;
	ioarcb->request_flags0 |= NO_LINK_DESCS;

	ioadl = pmcraid_init_ioadls(cmd, sglist->num_dma_sg);

	
	for_each_sg(sglist->scatterlist, sg, sglist->num_dma_sg, i) {
		ioadl[i].data_len = cpu_to_le32(sg_dma_len(sg));
		ioadl[i].address = cpu_to_le64(sg_dma_address(sg));
		ioadl[i].flags = 0;
	}

	
	ioadl[i - 1].flags = IOADL_FLAGS_LAST_DESC;

	return 0;
}


static void pmcraid_release_passthrough_ioadls(
	struct pmcraid_cmd *cmd,
	int buflen,
	int direction
)
{
	struct pmcraid_sglist *sglist = cmd->sglist;

	if (buflen > 0) {
		pci_unmap_sg(cmd->drv_inst->pdev,
			     sglist->scatterlist,
			     sglist->num_sg,
			     direction);
		pmcraid_free_sglist(sglist);
		cmd->sglist = NULL;
	}
}

static long pmcraid_ioctl_passthrough(
	struct pmcraid_instance *pinstance,
	unsigned int ioctl_cmd,
	unsigned int buflen,
	unsigned long arg
)
{
	struct pmcraid_passthrough_ioctl_buffer *buffer;
	struct pmcraid_ioarcb *ioarcb;
	struct pmcraid_cmd *cmd;
	struct pmcraid_cmd *cancel_cmd;
	unsigned long request_buffer;
	unsigned long request_offset;
	unsigned long lock_flags;
	void *ioasa;
	u32 ioasc;
	int request_size;
	int buffer_size;
	u8 access, direction;
	int rc = 0;

	
	if (pinstance->ioa_reset_in_progress) {
		rc = wait_event_interruptible_timeout(
				pinstance->reset_wait_q,
				!pinstance->ioa_reset_in_progress,
				msecs_to_jiffies(10000));

		if (!rc)
			return -ETIMEDOUT;
		else if (rc < 0)
			return -ERESTARTSYS;
	}

	
	if (pinstance->ioa_state != IOA_STATE_OPERATIONAL) {
		pmcraid_err("IOA is not operational\n");
		return -ENOTTY;
	}

	buffer_size = sizeof(struct pmcraid_passthrough_ioctl_buffer);
	buffer = kmalloc(buffer_size, GFP_KERNEL);

	if (!buffer) {
		pmcraid_err("no memory for passthrough buffer\n");
		return -ENOMEM;
	}

	request_offset =
	    offsetof(struct pmcraid_passthrough_ioctl_buffer, request_buffer);

	request_buffer = arg + request_offset;

	rc = __copy_from_user(buffer,
			     (struct pmcraid_passthrough_ioctl_buffer *) arg,
			     sizeof(struct pmcraid_passthrough_ioctl_buffer));

	ioasa =
	(void *)(arg +
		offsetof(struct pmcraid_passthrough_ioctl_buffer, ioasa));

	if (rc) {
		pmcraid_err("ioctl: can't copy passthrough buffer\n");
		rc = -EFAULT;
		goto out_free_buffer;
	}

	request_size = buffer->ioarcb.data_transfer_length;

	if (buffer->ioarcb.request_flags0 & TRANSFER_DIR_WRITE) {
		access = VERIFY_READ;
		direction = DMA_TO_DEVICE;
	} else {
		access = VERIFY_WRITE;
		direction = DMA_FROM_DEVICE;
	}

	if (request_size > 0) {
		rc = access_ok(access, arg, request_offset + request_size);

		if (!rc) {
			rc = -EFAULT;
			goto out_free_buffer;
		}
	} else if (request_size < 0) {
		rc = -EINVAL;
		goto out_free_buffer;
	}

	
	if (buffer->ioarcb.add_cmd_param_length > PMCRAID_ADD_CMD_PARAM_LEN) {
		rc = -EINVAL;
		goto out_free_buffer;
	}

	cmd = pmcraid_get_free_cmd(pinstance);

	if (!cmd) {
		pmcraid_err("free command block is not available\n");
		rc = -ENOMEM;
		goto out_free_buffer;
	}

	cmd->scsi_cmd = NULL;
	ioarcb = &(cmd->ioa_cb->ioarcb);

	
	ioarcb->resource_handle = buffer->ioarcb.resource_handle;
	ioarcb->data_transfer_length = buffer->ioarcb.data_transfer_length;
	ioarcb->cmd_timeout = buffer->ioarcb.cmd_timeout;
	ioarcb->request_type = buffer->ioarcb.request_type;
	ioarcb->request_flags0 = buffer->ioarcb.request_flags0;
	ioarcb->request_flags1 = buffer->ioarcb.request_flags1;
	memcpy(ioarcb->cdb, buffer->ioarcb.cdb, PMCRAID_MAX_CDB_LEN);

	if (buffer->ioarcb.add_cmd_param_length) {
		ioarcb->add_cmd_param_length =
			buffer->ioarcb.add_cmd_param_length;
		ioarcb->add_cmd_param_offset =
			buffer->ioarcb.add_cmd_param_offset;
		memcpy(ioarcb->add_data.u.add_cmd_params,
			buffer->ioarcb.add_data.u.add_cmd_params,
			buffer->ioarcb.add_cmd_param_length);
	}

	ioarcb->hrrq_id = atomic_add_return(1, &(pinstance->last_message_id)) %
			  pinstance->num_hrrq;

	if (request_size) {
		rc = pmcraid_build_passthrough_ioadls(cmd,
						      request_size,
						      direction);
		if (rc) {
			pmcraid_err("couldn't build passthrough ioadls\n");
			goto out_free_buffer;
		}
	} else if (request_size < 0) {
		rc = -EINVAL;
		goto out_free_buffer;
	}

	/* If data is being written into the device, copy the data from user
	 * buffers
	 */
	if (direction == DMA_TO_DEVICE && request_size > 0) {
		rc = pmcraid_copy_sglist(cmd->sglist,
					 request_buffer,
					 request_size,
					 direction);
		if (rc) {
			pmcraid_err("failed to copy user buffer\n");
			goto out_free_sglist;
		}
	}

	cmd->cmd_done = pmcraid_internal_done;
	init_completion(&cmd->wait_for_completion);
	cmd->completion_req = 1;

	pmcraid_info("command(%d) (CDB[0] = %x) for %x\n",
		     le32_to_cpu(cmd->ioa_cb->ioarcb.response_handle) >> 2,
		     cmd->ioa_cb->ioarcb.cdb[0],
		     le32_to_cpu(cmd->ioa_cb->ioarcb.resource_handle));

	spin_lock_irqsave(pinstance->host->host_lock, lock_flags);
	_pmcraid_fire_command(cmd);
	spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);

	buffer->ioarcb.cmd_timeout = 0;

	if (buffer->ioarcb.cmd_timeout == 0) {
		wait_for_completion(&cmd->wait_for_completion);
	} else if (!wait_for_completion_timeout(
			&cmd->wait_for_completion,
			msecs_to_jiffies(buffer->ioarcb.cmd_timeout * 1000))) {

		pmcraid_info("aborting cmd %d (CDB[0] = %x) due to timeout\n",
			le32_to_cpu(cmd->ioa_cb->ioarcb.response_handle >> 2),
			cmd->ioa_cb->ioarcb.cdb[0]);

		spin_lock_irqsave(pinstance->host->host_lock, lock_flags);
		cancel_cmd = pmcraid_abort_cmd(cmd);
		spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);

		if (cancel_cmd) {
			wait_for_completion(&cancel_cmd->wait_for_completion);
			ioasc = cancel_cmd->ioa_cb->ioasa.ioasc;
			pmcraid_return_cmd(cancel_cmd);

			if (ioasc == PMCRAID_IOASC_IOA_WAS_RESET ||
			    PMCRAID_IOASC_SENSE_KEY(ioasc) == 0x00) {
				if (ioasc != PMCRAID_IOASC_GC_IOARCB_NOTFOUND)
					rc = -ETIMEDOUT;
				goto out_handle_response;
			}
		}

		if (!wait_for_completion_timeout(
			&cmd->wait_for_completion,
			msecs_to_jiffies(150 * 1000))) {
			pmcraid_reset_bringup(cmd->drv_inst);
			rc = -ETIMEDOUT;
		}
	}

out_handle_response:
	if (copy_to_user(ioasa, &cmd->ioa_cb->ioasa,
		sizeof(struct pmcraid_ioasa))) {
		pmcraid_err("failed to copy ioasa buffer to user\n");
		rc = -EFAULT;
	}

	else if (direction == DMA_FROM_DEVICE && request_size > 0) {
		rc = pmcraid_copy_sglist(cmd->sglist,
					 request_buffer,
					 request_size,
					 direction);
		if (rc) {
			pmcraid_err("failed to copy user buffer\n");
			rc = -EFAULT;
		}
	}

out_free_sglist:
	pmcraid_release_passthrough_ioadls(cmd, request_size, direction);
	pmcraid_return_cmd(cmd);

out_free_buffer:
	kfree(buffer);

	return rc;
}




static long pmcraid_ioctl_driver(
	struct pmcraid_instance *pinstance,
	unsigned int cmd,
	unsigned int buflen,
	void __user *user_buffer
)
{
	int rc = -ENOSYS;

	if (!access_ok(VERIFY_READ, user_buffer, _IOC_SIZE(cmd))) {
		pmcraid_err("ioctl_driver: access fault in request buffer\n");
		return -EFAULT;
	}

	switch (cmd) {
	case PMCRAID_IOCTL_RESET_ADAPTER:
		pmcraid_reset_bringup(pinstance);
		rc = 0;
		break;

	default:
		break;
	}

	return rc;
}


static int pmcraid_check_ioctl_buffer(
	int cmd,
	void __user *arg,
	struct pmcraid_ioctl_header *hdr
)
{
	int rc = 0;
	int access = VERIFY_READ;

	if (copy_from_user(hdr, arg, sizeof(struct pmcraid_ioctl_header))) {
		pmcraid_err("couldn't copy ioctl header from user buffer\n");
		return -EFAULT;
	}

	
	rc = memcmp(hdr->signature,
		    PMCRAID_IOCTL_SIGNATURE,
		    sizeof(hdr->signature));
	if (rc) {
		pmcraid_err("signature verification failed\n");
		return -EINVAL;
	}

	
	if ((_IOC_DIR(cmd) & _IOC_READ) == _IOC_READ)
		access = VERIFY_WRITE;

	rc = access_ok(access,
		       (arg + sizeof(struct pmcraid_ioctl_header)),
		       hdr->buffer_length);
	if (!rc) {
		pmcraid_err("access failed for user buffer of size %d\n",
			     hdr->buffer_length);
		return -EFAULT;
	}

	return 0;
}

static long pmcraid_chr_ioctl(
	struct file *filep,
	unsigned int cmd,
	unsigned long arg
)
{
	struct pmcraid_instance *pinstance = NULL;
	struct pmcraid_ioctl_header *hdr = NULL;
	int retval = -ENOTTY;

	hdr = kmalloc(sizeof(struct pmcraid_ioctl_header), GFP_KERNEL);

	if (!hdr) {
		pmcraid_err("failed to allocate memory for ioctl header\n");
		return -ENOMEM;
	}

	retval = pmcraid_check_ioctl_buffer(cmd, (void *)arg, hdr);

	if (retval) {
		pmcraid_info("chr_ioctl: header check failed\n");
		kfree(hdr);
		return retval;
	}

	pinstance = filep->private_data;

	if (!pinstance) {
		pmcraid_info("adapter instance is not found\n");
		kfree(hdr);
		return -ENOTTY;
	}

	switch (_IOC_TYPE(cmd)) {

	case PMCRAID_PASSTHROUGH_IOCTL:
		if (cmd == PMCRAID_IOCTL_DOWNLOAD_MICROCODE)
			scsi_block_requests(pinstance->host);

		retval = pmcraid_ioctl_passthrough(pinstance,
						   cmd,
						   hdr->buffer_length,
						   arg);

		if (cmd == PMCRAID_IOCTL_DOWNLOAD_MICROCODE)
			scsi_unblock_requests(pinstance->host);
		break;

	case PMCRAID_DRIVER_IOCTL:
		arg += sizeof(struct pmcraid_ioctl_header);
		retval = pmcraid_ioctl_driver(pinstance,
					      cmd,
					      hdr->buffer_length,
					      (void __user *)arg);
		break;

	default:
		retval = -ENOTTY;
		break;
	}

	kfree(hdr);

	return retval;
}

static const struct file_operations pmcraid_fops = {
	.owner = THIS_MODULE,
	.open = pmcraid_chr_open,
	.release = pmcraid_chr_release,
	.fasync = pmcraid_chr_fasync,
	.unlocked_ioctl = pmcraid_chr_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = pmcraid_chr_ioctl,
#endif
	.llseek = noop_llseek,
};




static ssize_t pmcraid_show_log_level(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct pmcraid_instance *pinstance =
		(struct pmcraid_instance *)shost->hostdata;
	return snprintf(buf, PAGE_SIZE, "%d\n", pinstance->current_log_level);
}

static ssize_t pmcraid_store_log_level(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count
)
{
	struct Scsi_Host *shost;
	struct pmcraid_instance *pinstance;
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	
	if (val > 2)
		return -EINVAL;

	shost = class_to_shost(dev);
	pinstance = (struct pmcraid_instance *)shost->hostdata;
	pinstance->current_log_level = val;

	return strlen(buf);
}

static struct device_attribute pmcraid_log_level_attr = {
	.attr = {
		 .name = "log_level",
		 .mode = S_IRUGO | S_IWUSR,
		 },
	.show = pmcraid_show_log_level,
	.store = pmcraid_store_log_level,
};

static ssize_t pmcraid_show_drv_version(
	struct device *dev,
	struct device_attribute *attr,
	char *buf
)
{
	return snprintf(buf, PAGE_SIZE, "version: %s\n",
			PMCRAID_DRIVER_VERSION);
}

static struct device_attribute pmcraid_driver_version_attr = {
	.attr = {
		 .name = "drv_version",
		 .mode = S_IRUGO,
		 },
	.show = pmcraid_show_drv_version,
};

static ssize_t pmcraid_show_adapter_id(
	struct device *dev,
	struct device_attribute *attr,
	char *buf
)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct pmcraid_instance *pinstance =
		(struct pmcraid_instance *)shost->hostdata;
	u32 adapter_id = (pinstance->pdev->bus->number << 8) |
		pinstance->pdev->devfn;
	u32 aen_group = pmcraid_event_family.id;

	return snprintf(buf, PAGE_SIZE,
			"adapter id: %d\nminor: %d\naen group: %d\n",
			adapter_id, MINOR(pinstance->cdev.dev), aen_group);
}

static struct device_attribute pmcraid_adapter_id_attr = {
	.attr = {
		 .name = "adapter_id",
		 .mode = S_IRUGO | S_IWUSR,
		 },
	.show = pmcraid_show_adapter_id,
};

static struct device_attribute *pmcraid_host_attrs[] = {
	&pmcraid_log_level_attr,
	&pmcraid_driver_version_attr,
	&pmcraid_adapter_id_attr,
	NULL,
};


static struct scsi_host_template pmcraid_host_template = {
	.module = THIS_MODULE,
	.name = PMCRAID_DRIVER_NAME,
	.queuecommand = pmcraid_queuecommand,
	.eh_abort_handler = pmcraid_eh_abort_handler,
	.eh_bus_reset_handler = pmcraid_eh_bus_reset_handler,
	.eh_target_reset_handler = pmcraid_eh_target_reset_handler,
	.eh_device_reset_handler = pmcraid_eh_device_reset_handler,
	.eh_host_reset_handler = pmcraid_eh_host_reset_handler,

	.slave_alloc = pmcraid_slave_alloc,
	.slave_configure = pmcraid_slave_configure,
	.slave_destroy = pmcraid_slave_destroy,
	.change_queue_depth = pmcraid_change_queue_depth,
	.change_queue_type  = pmcraid_change_queue_type,
	.can_queue = PMCRAID_MAX_IO_CMD,
	.this_id = -1,
	.sg_tablesize = PMCRAID_MAX_IOADLS,
	.max_sectors = PMCRAID_IOA_MAX_SECTORS,
	.cmd_per_lun = PMCRAID_MAX_CMD_PER_LUN,
	.use_clustering = ENABLE_CLUSTERING,
	.shost_attrs = pmcraid_host_attrs,
	.proc_name = PMCRAID_DRIVER_NAME
};


static irqreturn_t pmcraid_isr_msix(int irq, void *dev_id)
{
	struct pmcraid_isr_param *hrrq_vector;
	struct pmcraid_instance *pinstance;
	unsigned long lock_flags;
	u32 intrs_val;
	int hrrq_id;

	hrrq_vector = (struct pmcraid_isr_param *)dev_id;
	hrrq_id = hrrq_vector->hrrq_id;
	pinstance = hrrq_vector->drv_inst;

	if (!hrrq_id) {
		
		intrs_val = pmcraid_read_interrupts(pinstance);
		if (intrs_val &&
			((ioread32(pinstance->int_regs.host_ioa_interrupt_reg)
			& DOORBELL_INTR_MSIX_CLR) == 0)) {
			if (intrs_val & PMCRAID_ERROR_INTERRUPTS) {
				if (intrs_val & INTRS_IOA_UNIT_CHECK)
					pinstance->ioa_unit_check = 1;

				pmcraid_err("ISR: error interrupts: %x \
					initiating reset\n", intrs_val);
				spin_lock_irqsave(pinstance->host->host_lock,
					lock_flags);
				pmcraid_initiate_reset(pinstance);
				spin_unlock_irqrestore(
					pinstance->host->host_lock,
					lock_flags);
			}
			if (intrs_val & INTRS_TRANSITION_TO_OPERATIONAL)
				pmcraid_clr_trans_op(pinstance);

			iowrite32(DOORBELL_INTR_MSIX_CLR,
				pinstance->int_regs.host_ioa_interrupt_reg);
			ioread32(pinstance->int_regs.host_ioa_interrupt_reg);


		}
	}

	tasklet_schedule(&(pinstance->isr_tasklet[hrrq_id]));

	return IRQ_HANDLED;
}

static irqreturn_t pmcraid_isr(int irq, void *dev_id)
{
	struct pmcraid_isr_param *hrrq_vector;
	struct pmcraid_instance *pinstance;
	u32 intrs;
	unsigned long lock_flags;
	int hrrq_id = 0;

	if (!dev_id) {
		printk(KERN_INFO "%s(): NULL host pointer\n", __func__);
		return IRQ_NONE;
	}
	hrrq_vector = (struct pmcraid_isr_param *)dev_id;
	pinstance = hrrq_vector->drv_inst;

	intrs = pmcraid_read_interrupts(pinstance);

	if (unlikely((intrs & PMCRAID_PCI_INTERRUPTS) == 0))
		return IRQ_NONE;

	if (intrs & PMCRAID_ERROR_INTERRUPTS) {

		if (intrs & INTRS_IOA_UNIT_CHECK)
			pinstance->ioa_unit_check = 1;

		iowrite32(intrs,
			  pinstance->int_regs.ioa_host_interrupt_clr_reg);
		pmcraid_err("ISR: error interrupts: %x initiating reset\n",
			    intrs);
		intrs = ioread32(
				pinstance->int_regs.ioa_host_interrupt_clr_reg);
		spin_lock_irqsave(pinstance->host->host_lock, lock_flags);
		pmcraid_initiate_reset(pinstance);
		spin_unlock_irqrestore(pinstance->host->host_lock, lock_flags);
	} else {
		if (intrs & INTRS_TRANSITION_TO_OPERATIONAL) {
			pmcraid_clr_trans_op(pinstance);
		} else {
			iowrite32(intrs,
				pinstance->int_regs.ioa_host_interrupt_clr_reg);
			ioread32(
				pinstance->int_regs.ioa_host_interrupt_clr_reg);

			tasklet_schedule(
					&(pinstance->isr_tasklet[hrrq_id]));
		}
	}

	return IRQ_HANDLED;
}



static void pmcraid_worker_function(struct work_struct *workp)
{
	struct pmcraid_instance *pinstance;
	struct pmcraid_resource_entry *res;
	struct pmcraid_resource_entry *temp;
	struct scsi_device *sdev;
	unsigned long lock_flags;
	unsigned long host_lock_flags;
	u16 fw_version;
	u8 bus, target, lun;

	pinstance = container_of(workp, struct pmcraid_instance, worker_q);
	
	if (!atomic_read(&pinstance->expose_resources))
		return;

	fw_version = be16_to_cpu(pinstance->inq_data->fw_version);

	spin_lock_irqsave(&pinstance->resource_lock, lock_flags);
	list_for_each_entry_safe(res, temp, &pinstance->used_res_q, queue) {

		if (res->change_detected == RES_CHANGE_DEL && res->scsi_dev) {
			sdev = res->scsi_dev;

			spin_lock_irqsave(pinstance->host->host_lock,
					  host_lock_flags);
			if (!scsi_device_get(sdev)) {
				spin_unlock_irqrestore(
						pinstance->host->host_lock,
						host_lock_flags);
				pmcraid_info("deleting %x from midlayer\n",
					     res->cfg_entry.resource_address);
				list_move_tail(&res->queue,
						&pinstance->free_res_q);
				spin_unlock_irqrestore(
					&pinstance->resource_lock,
					lock_flags);
				scsi_remove_device(sdev);
				scsi_device_put(sdev);
				spin_lock_irqsave(&pinstance->resource_lock,
						   lock_flags);
				res->change_detected = 0;
			} else {
				spin_unlock_irqrestore(
						pinstance->host->host_lock,
						host_lock_flags);
			}
		}
	}

	list_for_each_entry(res, &pinstance->used_res_q, queue) {

		if (res->change_detected == RES_CHANGE_ADD) {

			if (!pmcraid_expose_resource(fw_version,
						     &res->cfg_entry))
				continue;

			if (RES_IS_VSET(res->cfg_entry)) {
				bus = PMCRAID_VSET_BUS_ID;
				if (fw_version <= PMCRAID_FW_VERSION_1)
					target = res->cfg_entry.unique_flags1;
				else
					target = res->cfg_entry.array_id & 0xFF;
				lun = PMCRAID_VSET_LUN_ID;
			} else {
				bus = PMCRAID_PHYS_BUS_ID;
				target =
				     RES_TARGET(
					res->cfg_entry.resource_address);
				lun = RES_LUN(res->cfg_entry.resource_address);
			}

			res->change_detected = 0;
			spin_unlock_irqrestore(&pinstance->resource_lock,
						lock_flags);
			scsi_add_device(pinstance->host, bus, target, lun);
			spin_lock_irqsave(&pinstance->resource_lock,
					   lock_flags);
		}
	}

	spin_unlock_irqrestore(&pinstance->resource_lock, lock_flags);
}

static void pmcraid_tasklet_function(unsigned long instance)
{
	struct pmcraid_isr_param *hrrq_vector;
	struct pmcraid_instance *pinstance;
	unsigned long hrrq_lock_flags;
	unsigned long pending_lock_flags;
	unsigned long host_lock_flags;
	spinlock_t *lockp; 
	int id;
	__le32 resp;

	hrrq_vector = (struct pmcraid_isr_param *)instance;
	pinstance = hrrq_vector->drv_inst;
	id = hrrq_vector->hrrq_id;
	lockp = &(pinstance->hrrq_lock[id]);

	spin_lock_irqsave(lockp, hrrq_lock_flags);

	resp = le32_to_cpu(*(pinstance->hrrq_curr[id]));

	while ((resp & HRRQ_TOGGLE_BIT) ==
		pinstance->host_toggle_bit[id]) {

		int cmd_index = resp >> 2;
		struct pmcraid_cmd *cmd = NULL;

		if (pinstance->hrrq_curr[id] < pinstance->hrrq_end[id]) {
			pinstance->hrrq_curr[id]++;
		} else {
			pinstance->hrrq_curr[id] = pinstance->hrrq_start[id];
			pinstance->host_toggle_bit[id] ^= 1u;
		}

		if (cmd_index >= PMCRAID_MAX_CMD) {
			
			pmcraid_err("Invalid response handle %d\n", cmd_index);
			resp = le32_to_cpu(*(pinstance->hrrq_curr[id]));
			continue;
		}

		cmd = pinstance->cmd_list[cmd_index];
		spin_unlock_irqrestore(lockp, hrrq_lock_flags);

		spin_lock_irqsave(&pinstance->pending_pool_lock,
				   pending_lock_flags);
		list_del(&cmd->free_list);
		spin_unlock_irqrestore(&pinstance->pending_pool_lock,
					pending_lock_flags);
		del_timer(&cmd->timer);
		atomic_dec(&pinstance->outstanding_cmds);

		if (cmd->cmd_done == pmcraid_ioa_reset) {
			spin_lock_irqsave(pinstance->host->host_lock,
					  host_lock_flags);
			cmd->cmd_done(cmd);
			spin_unlock_irqrestore(pinstance->host->host_lock,
					       host_lock_flags);
		} else if (cmd->cmd_done != NULL) {
			cmd->cmd_done(cmd);
		}
		
		spin_lock_irqsave(lockp, hrrq_lock_flags);
		resp = le32_to_cpu(*(pinstance->hrrq_curr[id]));
	}

	spin_unlock_irqrestore(lockp, hrrq_lock_flags);
}

static
void pmcraid_unregister_interrupt_handler(struct pmcraid_instance *pinstance)
{
	int i;

	for (i = 0; i < pinstance->num_hrrq; i++)
		free_irq(pinstance->hrrq_vector[i].vector,
			 &(pinstance->hrrq_vector[i]));

	if (pinstance->interrupt_mode) {
		pci_disable_msix(pinstance->pdev);
		pinstance->interrupt_mode = 0;
	}
}

static int
pmcraid_register_interrupt_handler(struct pmcraid_instance *pinstance)
{
	int rc;
	struct pci_dev *pdev = pinstance->pdev;

	if ((pmcraid_enable_msix) &&
		(pci_find_capability(pdev, PCI_CAP_ID_MSIX))) {
		int num_hrrq = PMCRAID_NUM_MSIX_VECTORS;
		struct msix_entry entries[PMCRAID_NUM_MSIX_VECTORS];
		int i;
		for (i = 0; i < PMCRAID_NUM_MSIX_VECTORS; i++)
			entries[i].entry = i;

		rc = pci_enable_msix(pdev, entries, num_hrrq);
		if (rc < 0)
			goto pmcraid_isr_legacy;

		if (rc > 0) {
			num_hrrq = rc;
			if (pci_enable_msix(pdev, entries, num_hrrq))
				goto pmcraid_isr_legacy;
		}

		for (i = 0; i < num_hrrq; i++) {
			pinstance->hrrq_vector[i].hrrq_id = i;
			pinstance->hrrq_vector[i].drv_inst = pinstance;
			pinstance->hrrq_vector[i].vector = entries[i].vector;
			rc = request_irq(pinstance->hrrq_vector[i].vector,
					pmcraid_isr_msix, 0,
					PMCRAID_DRIVER_NAME,
					&(pinstance->hrrq_vector[i]));

			if (rc) {
				int j;
				for (j = 0; j < i; j++)
					free_irq(entries[j].vector,
						 &(pinstance->hrrq_vector[j]));
				pci_disable_msix(pdev);
				goto pmcraid_isr_legacy;
			}
		}

		pinstance->num_hrrq = num_hrrq;
		pinstance->interrupt_mode = 1;
		iowrite32(DOORBELL_INTR_MODE_MSIX,
			  pinstance->int_regs.host_ioa_interrupt_reg);
		ioread32(pinstance->int_regs.host_ioa_interrupt_reg);
		goto pmcraid_isr_out;
	}

pmcraid_isr_legacy:
	pinstance->hrrq_vector[0].hrrq_id = 0;
	pinstance->hrrq_vector[0].drv_inst = pinstance;
	pinstance->hrrq_vector[0].vector = pdev->irq;
	pinstance->num_hrrq = 1;
	rc = 0;

	rc = request_irq(pdev->irq, pmcraid_isr, IRQF_SHARED,
			 PMCRAID_DRIVER_NAME, &pinstance->hrrq_vector[0]);
pmcraid_isr_out:
	return rc;
}

static void
pmcraid_release_cmd_blocks(struct pmcraid_instance *pinstance, int max_index)
{
	int i;
	for (i = 0; i < max_index; i++) {
		kmem_cache_free(pinstance->cmd_cachep, pinstance->cmd_list[i]);
		pinstance->cmd_list[i] = NULL;
	}
	kmem_cache_destroy(pinstance->cmd_cachep);
	pinstance->cmd_cachep = NULL;
}

static void
pmcraid_release_control_blocks(
	struct pmcraid_instance *pinstance,
	int max_index
)
{
	int i;

	if (pinstance->control_pool == NULL)
		return;

	for (i = 0; i < max_index; i++) {
		pci_pool_free(pinstance->control_pool,
			      pinstance->cmd_list[i]->ioa_cb,
			      pinstance->cmd_list[i]->ioa_cb_bus_addr);
		pinstance->cmd_list[i]->ioa_cb = NULL;
		pinstance->cmd_list[i]->ioa_cb_bus_addr = 0;
	}
	pci_pool_destroy(pinstance->control_pool);
	pinstance->control_pool = NULL;
}

static int __devinit
pmcraid_allocate_cmd_blocks(struct pmcraid_instance *pinstance)
{
	int i;

	sprintf(pinstance->cmd_pool_name, "pmcraid_cmd_pool_%d",
		pinstance->host->unique_id);


	pinstance->cmd_cachep = kmem_cache_create(
					pinstance->cmd_pool_name,
					sizeof(struct pmcraid_cmd), 0,
					SLAB_HWCACHE_ALIGN, NULL);
	if (!pinstance->cmd_cachep)
		return -ENOMEM;

	for (i = 0; i < PMCRAID_MAX_CMD; i++) {
		pinstance->cmd_list[i] =
			kmem_cache_alloc(pinstance->cmd_cachep, GFP_KERNEL);
		if (!pinstance->cmd_list[i]) {
			pmcraid_release_cmd_blocks(pinstance, i);
			return -ENOMEM;
		}
	}
	return 0;
}

static int __devinit
pmcraid_allocate_control_blocks(struct pmcraid_instance *pinstance)
{
	int i;

	sprintf(pinstance->ctl_pool_name, "pmcraid_control_pool_%d",
		pinstance->host->unique_id);

	pinstance->control_pool =
		pci_pool_create(pinstance->ctl_pool_name,
				pinstance->pdev,
				sizeof(struct pmcraid_control_block),
				PMCRAID_IOARCB_ALIGNMENT, 0);

	if (!pinstance->control_pool)
		return -ENOMEM;

	for (i = 0; i < PMCRAID_MAX_CMD; i++) {
		pinstance->cmd_list[i]->ioa_cb =
			pci_pool_alloc(
				pinstance->control_pool,
				GFP_KERNEL,
				&(pinstance->cmd_list[i]->ioa_cb_bus_addr));

		if (!pinstance->cmd_list[i]->ioa_cb) {
			pmcraid_release_control_blocks(pinstance, i);
			return -ENOMEM;
		}
		memset(pinstance->cmd_list[i]->ioa_cb, 0,
			sizeof(struct pmcraid_control_block));
	}
	return 0;
}

static void
pmcraid_release_host_rrqs(struct pmcraid_instance *pinstance, int maxindex)
{
	int i;
	for (i = 0; i < maxindex; i++) {

		pci_free_consistent(pinstance->pdev,
				    HRRQ_ENTRY_SIZE * PMCRAID_MAX_CMD,
				    pinstance->hrrq_start[i],
				    pinstance->hrrq_start_bus_addr[i]);

		
		pinstance->hrrq_start[i] = NULL;
		pinstance->hrrq_start_bus_addr[i] = 0;
		pinstance->host_toggle_bit[i] = 0;
	}
}

static int __devinit
pmcraid_allocate_host_rrqs(struct pmcraid_instance *pinstance)
{
	int i, buffer_size;

	buffer_size = HRRQ_ENTRY_SIZE * PMCRAID_MAX_CMD;

	for (i = 0; i < pinstance->num_hrrq; i++) {
		pinstance->hrrq_start[i] =
			pci_alloc_consistent(
					pinstance->pdev,
					buffer_size,
					&(pinstance->hrrq_start_bus_addr[i]));

		if (pinstance->hrrq_start[i] == 0) {
			pmcraid_err("pci_alloc failed for hrrq vector : %d\n",
				    i);
			pmcraid_release_host_rrqs(pinstance, i);
			return -ENOMEM;
		}

		memset(pinstance->hrrq_start[i], 0, buffer_size);
		pinstance->hrrq_curr[i] = pinstance->hrrq_start[i];
		pinstance->hrrq_end[i] =
			pinstance->hrrq_start[i] + PMCRAID_MAX_CMD - 1;
		pinstance->host_toggle_bit[i] = 1;
		spin_lock_init(&pinstance->hrrq_lock[i]);
	}
	return 0;
}

static void pmcraid_release_hcams(struct pmcraid_instance *pinstance)
{
	if (pinstance->ccn.msg != NULL) {
		pci_free_consistent(pinstance->pdev,
				    PMCRAID_AEN_HDR_SIZE +
				    sizeof(struct pmcraid_hcam_ccn_ext),
				    pinstance->ccn.msg,
				    pinstance->ccn.baddr);

		pinstance->ccn.msg = NULL;
		pinstance->ccn.hcam = NULL;
		pinstance->ccn.baddr = 0;
	}

	if (pinstance->ldn.msg != NULL) {
		pci_free_consistent(pinstance->pdev,
				    PMCRAID_AEN_HDR_SIZE +
				    sizeof(struct pmcraid_hcam_ldn),
				    pinstance->ldn.msg,
				    pinstance->ldn.baddr);

		pinstance->ldn.msg = NULL;
		pinstance->ldn.hcam = NULL;
		pinstance->ldn.baddr = 0;
	}
}

static int pmcraid_allocate_hcams(struct pmcraid_instance *pinstance)
{
	pinstance->ccn.msg = pci_alloc_consistent(
					pinstance->pdev,
					PMCRAID_AEN_HDR_SIZE +
					sizeof(struct pmcraid_hcam_ccn_ext),
					&(pinstance->ccn.baddr));

	pinstance->ldn.msg = pci_alloc_consistent(
					pinstance->pdev,
					PMCRAID_AEN_HDR_SIZE +
					sizeof(struct pmcraid_hcam_ldn),
					&(pinstance->ldn.baddr));

	if (pinstance->ldn.msg == NULL || pinstance->ccn.msg == NULL) {
		pmcraid_release_hcams(pinstance);
	} else {
		pinstance->ccn.hcam =
			(void *)pinstance->ccn.msg + PMCRAID_AEN_HDR_SIZE;
		pinstance->ldn.hcam =
			(void *)pinstance->ldn.msg + PMCRAID_AEN_HDR_SIZE;

		atomic_set(&pinstance->ccn.ignore, 0);
		atomic_set(&pinstance->ldn.ignore, 0);
	}

	return (pinstance->ldn.msg == NULL) ? -ENOMEM : 0;
}

static void pmcraid_release_config_buffers(struct pmcraid_instance *pinstance)
{
	if (pinstance->cfg_table != NULL &&
	    pinstance->cfg_table_bus_addr != 0) {
		pci_free_consistent(pinstance->pdev,
				    sizeof(struct pmcraid_config_table),
				    pinstance->cfg_table,
				    pinstance->cfg_table_bus_addr);
		pinstance->cfg_table = NULL;
		pinstance->cfg_table_bus_addr = 0;
	}

	if (pinstance->res_entries != NULL) {
		int i;

		for (i = 0; i < PMCRAID_MAX_RESOURCES; i++)
			list_del(&pinstance->res_entries[i].queue);
		kfree(pinstance->res_entries);
		pinstance->res_entries = NULL;
	}

	pmcraid_release_hcams(pinstance);
}

static int __devinit
pmcraid_allocate_config_buffers(struct pmcraid_instance *pinstance)
{
	int i;

	pinstance->res_entries =
			kzalloc(sizeof(struct pmcraid_resource_entry) *
				PMCRAID_MAX_RESOURCES, GFP_KERNEL);

	if (NULL == pinstance->res_entries) {
		pmcraid_err("failed to allocate memory for resource table\n");
		return -ENOMEM;
	}

	for (i = 0; i < PMCRAID_MAX_RESOURCES; i++)
		list_add_tail(&pinstance->res_entries[i].queue,
			      &pinstance->free_res_q);

	pinstance->cfg_table =
		pci_alloc_consistent(pinstance->pdev,
				     sizeof(struct pmcraid_config_table),
				     &pinstance->cfg_table_bus_addr);

	if (NULL == pinstance->cfg_table) {
		pmcraid_err("couldn't alloc DMA memory for config table\n");
		pmcraid_release_config_buffers(pinstance);
		return -ENOMEM;
	}

	if (pmcraid_allocate_hcams(pinstance)) {
		pmcraid_err("could not alloc DMA memory for HCAMS\n");
		pmcraid_release_config_buffers(pinstance);
		return -ENOMEM;
	}

	return 0;
}

static void pmcraid_init_tasklets(struct pmcraid_instance *pinstance)
{
	int i;
	for (i = 0; i < pinstance->num_hrrq; i++)
		tasklet_init(&pinstance->isr_tasklet[i],
			     pmcraid_tasklet_function,
			     (unsigned long)&pinstance->hrrq_vector[i]);
}

static void pmcraid_kill_tasklets(struct pmcraid_instance *pinstance)
{
	int i;
	for (i = 0; i < pinstance->num_hrrq; i++)
		tasklet_kill(&pinstance->isr_tasklet[i]);
}

static void pmcraid_release_buffers(struct pmcraid_instance *pinstance)
{
	pmcraid_release_config_buffers(pinstance);
	pmcraid_release_control_blocks(pinstance, PMCRAID_MAX_CMD);
	pmcraid_release_cmd_blocks(pinstance, PMCRAID_MAX_CMD);
	pmcraid_release_host_rrqs(pinstance, pinstance->num_hrrq);

	if (pinstance->inq_data != NULL) {
		pci_free_consistent(pinstance->pdev,
				    sizeof(struct pmcraid_inquiry_data),
				    pinstance->inq_data,
				    pinstance->inq_data_baddr);

		pinstance->inq_data = NULL;
		pinstance->inq_data_baddr = 0;
	}

	if (pinstance->timestamp_data != NULL) {
		pci_free_consistent(pinstance->pdev,
				    sizeof(struct pmcraid_timestamp_data),
				    pinstance->timestamp_data,
				    pinstance->timestamp_data_baddr);

		pinstance->timestamp_data = NULL;
		pinstance->timestamp_data_baddr = 0;
	}
}

static int __devinit pmcraid_init_buffers(struct pmcraid_instance *pinstance)
{
	int i;

	if (pmcraid_allocate_host_rrqs(pinstance)) {
		pmcraid_err("couldn't allocate memory for %d host rrqs\n",
			     pinstance->num_hrrq);
		return -ENOMEM;
	}

	if (pmcraid_allocate_config_buffers(pinstance)) {
		pmcraid_err("couldn't allocate memory for config buffers\n");
		pmcraid_release_host_rrqs(pinstance, pinstance->num_hrrq);
		return -ENOMEM;
	}

	if (pmcraid_allocate_cmd_blocks(pinstance)) {
		pmcraid_err("couldn't allocate memory for cmd blocks\n");
		pmcraid_release_config_buffers(pinstance);
		pmcraid_release_host_rrqs(pinstance, pinstance->num_hrrq);
		return -ENOMEM;
	}

	if (pmcraid_allocate_control_blocks(pinstance)) {
		pmcraid_err("couldn't allocate memory control blocks\n");
		pmcraid_release_config_buffers(pinstance);
		pmcraid_release_cmd_blocks(pinstance, PMCRAID_MAX_CMD);
		pmcraid_release_host_rrqs(pinstance, pinstance->num_hrrq);
		return -ENOMEM;
	}

	
	pinstance->inq_data = pci_alloc_consistent(
					pinstance->pdev,
					sizeof(struct pmcraid_inquiry_data),
					&pinstance->inq_data_baddr);

	if (pinstance->inq_data == NULL) {
		pmcraid_err("couldn't allocate DMA memory for INQUIRY\n");
		pmcraid_release_buffers(pinstance);
		return -ENOMEM;
	}

	
	pinstance->timestamp_data = pci_alloc_consistent(
					pinstance->pdev,
					sizeof(struct pmcraid_timestamp_data),
					&pinstance->timestamp_data_baddr);

	if (pinstance->timestamp_data == NULL) {
		pmcraid_err("couldn't allocate DMA memory for \
				set time_stamp \n");
		pmcraid_release_buffers(pinstance);
		return -ENOMEM;
	}


	for (i = 0; i < PMCRAID_MAX_CMD; i++) {
		struct pmcraid_cmd *cmdp = pinstance->cmd_list[i];
		pmcraid_init_cmdblk(cmdp, i);
		cmdp->drv_inst = pinstance;
		list_add_tail(&cmdp->free_list, &pinstance->free_cmd_pool);
	}

	return 0;
}

static void pmcraid_reinit_buffers(struct pmcraid_instance *pinstance)
{
	int i;
	int buffer_size = HRRQ_ENTRY_SIZE * PMCRAID_MAX_CMD;

	for (i = 0; i < pinstance->num_hrrq; i++) {
		memset(pinstance->hrrq_start[i], 0, buffer_size);
		pinstance->hrrq_curr[i] = pinstance->hrrq_start[i];
		pinstance->hrrq_end[i] =
			pinstance->hrrq_start[i] + PMCRAID_MAX_CMD - 1;
		pinstance->host_toggle_bit[i] = 1;
	}
}

static int __devinit pmcraid_init_instance(
	struct pci_dev *pdev,
	struct Scsi_Host *host,
	void __iomem *mapped_pci_addr
)
{
	struct pmcraid_instance *pinstance =
		(struct pmcraid_instance *)host->hostdata;

	pinstance->host = host;
	pinstance->pdev = pdev;

	
	pinstance->mapped_dma_addr = mapped_pci_addr;

	
	{
		struct pmcraid_chip_details *chip_cfg = pinstance->chip_cfg;
		struct pmcraid_interrupts *pint_regs = &pinstance->int_regs;

		pinstance->ioarrin = mapped_pci_addr + chip_cfg->ioarrin;

		pint_regs->ioa_host_interrupt_reg =
			mapped_pci_addr + chip_cfg->ioa_host_intr;
		pint_regs->ioa_host_interrupt_clr_reg =
			mapped_pci_addr + chip_cfg->ioa_host_intr_clr;
		pint_regs->ioa_host_msix_interrupt_reg =
			mapped_pci_addr + chip_cfg->ioa_host_msix_intr;
		pint_regs->host_ioa_interrupt_reg =
			mapped_pci_addr + chip_cfg->host_ioa_intr;
		pint_regs->host_ioa_interrupt_clr_reg =
			mapped_pci_addr + chip_cfg->host_ioa_intr_clr;

		pinstance->mailbox = mapped_pci_addr + chip_cfg->mailbox;
		pinstance->ioa_status = mapped_pci_addr + chip_cfg->ioastatus;
		pint_regs->ioa_host_interrupt_mask_reg =
			mapped_pci_addr + chip_cfg->ioa_host_mask;
		pint_regs->ioa_host_interrupt_mask_clr_reg =
			mapped_pci_addr + chip_cfg->ioa_host_mask_clr;
		pint_regs->global_interrupt_mask_reg =
			mapped_pci_addr + chip_cfg->global_intr_mask;
	};

	pinstance->ioa_reset_attempts = 0;
	init_waitqueue_head(&pinstance->reset_wait_q);

	atomic_set(&pinstance->outstanding_cmds, 0);
	atomic_set(&pinstance->last_message_id, 0);
	atomic_set(&pinstance->expose_resources, 0);

	INIT_LIST_HEAD(&pinstance->free_res_q);
	INIT_LIST_HEAD(&pinstance->used_res_q);
	INIT_LIST_HEAD(&pinstance->free_cmd_pool);
	INIT_LIST_HEAD(&pinstance->pending_cmd_pool);

	spin_lock_init(&pinstance->free_pool_lock);
	spin_lock_init(&pinstance->pending_pool_lock);
	spin_lock_init(&pinstance->resource_lock);
	mutex_init(&pinstance->aen_queue_lock);

	
	INIT_WORK(&pinstance->worker_q, pmcraid_worker_function);

	
	pinstance->current_log_level = pmcraid_log_level;

	
	pinstance->ioa_state = IOA_STATE_UNKNOWN;
	pinstance->reset_cmd = NULL;
	return 0;
}

static void pmcraid_shutdown(struct pci_dev *pdev)
{
	struct pmcraid_instance *pinstance = pci_get_drvdata(pdev);
	pmcraid_reset_bringdown(pinstance);
}


static unsigned short pmcraid_get_minor(void)
{
	int minor;

	minor = find_first_zero_bit(pmcraid_minor, sizeof(pmcraid_minor));
	__set_bit(minor, pmcraid_minor);
	return minor;
}

static void pmcraid_release_minor(unsigned short minor)
{
	__clear_bit(minor, pmcraid_minor);
}

static int pmcraid_setup_chrdev(struct pmcraid_instance *pinstance)
{
	int minor;
	int error;

	minor = pmcraid_get_minor();
	cdev_init(&pinstance->cdev, &pmcraid_fops);
	pinstance->cdev.owner = THIS_MODULE;

	error = cdev_add(&pinstance->cdev, MKDEV(pmcraid_major, minor), 1);

	if (error)
		pmcraid_release_minor(minor);
	else
		device_create(pmcraid_class, NULL, MKDEV(pmcraid_major, minor),
			      NULL, "%s%u", PMCRAID_DEVFILE, minor);
	return error;
}

static void pmcraid_release_chrdev(struct pmcraid_instance *pinstance)
{
	pmcraid_release_minor(MINOR(pinstance->cdev.dev));
	device_destroy(pmcraid_class,
		       MKDEV(pmcraid_major, MINOR(pinstance->cdev.dev)));
	cdev_del(&pinstance->cdev);
}

static void __devexit pmcraid_remove(struct pci_dev *pdev)
{
	struct pmcraid_instance *pinstance = pci_get_drvdata(pdev);

	
	pmcraid_release_chrdev(pinstance);

	
	scsi_remove_host(pinstance->host);

	
	scsi_block_requests(pinstance->host);

	
	pmcraid_shutdown(pdev);

	pmcraid_disable_interrupts(pinstance, ~0);
	flush_work_sync(&pinstance->worker_q);

	pmcraid_kill_tasklets(pinstance);
	pmcraid_unregister_interrupt_handler(pinstance);
	pmcraid_release_buffers(pinstance);
	iounmap(pinstance->mapped_dma_addr);
	pci_release_regions(pdev);
	scsi_host_put(pinstance->host);
	pci_disable_device(pdev);

	return;
}

#ifdef CONFIG_PM
static int pmcraid_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct pmcraid_instance *pinstance = pci_get_drvdata(pdev);

	pmcraid_shutdown(pdev);
	pmcraid_disable_interrupts(pinstance, ~0);
	pmcraid_kill_tasklets(pinstance);
	pci_set_drvdata(pinstance->pdev, pinstance);
	pmcraid_unregister_interrupt_handler(pinstance);
	pci_save_state(pdev);
	pci_disable_device(pdev);
	pci_set_power_state(pdev, pci_choose_state(pdev, state));

	return 0;
}

static int pmcraid_resume(struct pci_dev *pdev)
{
	struct pmcraid_instance *pinstance = pci_get_drvdata(pdev);
	struct Scsi_Host *host = pinstance->host;
	int rc;

	pci_set_power_state(pdev, PCI_D0);
	pci_enable_wake(pdev, PCI_D0, 0);
	pci_restore_state(pdev);

	rc = pci_enable_device(pdev);

	if (rc) {
		dev_err(&pdev->dev, "resume: Enable device failed\n");
		return rc;
	}

	pci_set_master(pdev);

	if ((sizeof(dma_addr_t) == 4) ||
	     pci_set_dma_mask(pdev, DMA_BIT_MASK(64)))
		rc = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));

	if (rc == 0)
		rc = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));

	if (rc != 0) {
		dev_err(&pdev->dev, "resume: Failed to set PCI DMA mask\n");
		goto disable_device;
	}

	pmcraid_disable_interrupts(pinstance, ~0);
	atomic_set(&pinstance->outstanding_cmds, 0);
	rc = pmcraid_register_interrupt_handler(pinstance);

	if (rc) {
		dev_err(&pdev->dev,
			"resume: couldn't register interrupt handlers\n");
		rc = -ENODEV;
		goto release_host;
	}

	pmcraid_init_tasklets(pinstance);
	pmcraid_enable_interrupts(pinstance, PMCRAID_PCI_INTERRUPTS);

	pinstance->ioa_hard_reset = 1;

	if (pmcraid_reset_bringup(pinstance)) {
		dev_err(&pdev->dev, "couldn't initialize IOA\n");
		rc = -ENODEV;
		goto release_tasklets;
	}

	return 0;

release_tasklets:
	pmcraid_disable_interrupts(pinstance, ~0);
	pmcraid_kill_tasklets(pinstance);
	pmcraid_unregister_interrupt_handler(pinstance);

release_host:
	scsi_host_put(host);

disable_device:
	pci_disable_device(pdev);

	return rc;
}

#else

#define pmcraid_suspend NULL
#define pmcraid_resume  NULL

#endif 

static void pmcraid_complete_ioa_reset(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	unsigned long flags;

	spin_lock_irqsave(pinstance->host->host_lock, flags);
	pmcraid_ioa_reset(cmd);
	spin_unlock_irqrestore(pinstance->host->host_lock, flags);
	scsi_unblock_requests(pinstance->host);
	schedule_work(&pinstance->worker_q);
}

static void pmcraid_set_supported_devs(struct pmcraid_cmd *cmd)
{
	struct pmcraid_ioarcb *ioarcb = &cmd->ioa_cb->ioarcb;
	void (*cmd_done) (struct pmcraid_cmd *) = pmcraid_complete_ioa_reset;

	pmcraid_reinit_cmdblk(cmd);

	ioarcb->resource_handle = cpu_to_le32(PMCRAID_IOA_RES_HANDLE);
	ioarcb->request_type = REQ_TYPE_IOACMD;
	ioarcb->cdb[0] = PMCRAID_SET_SUPPORTED_DEVICES;
	ioarcb->cdb[1] = ALL_DEVICES_SUPPORTED;

	if (cmd->drv_inst->reinit_cfg_table) {
		cmd->drv_inst->reinit_cfg_table = 0;
		cmd->release = 1;
		cmd_done = pmcraid_reinit_cfgtable_done;
	}

	pmcraid_send_cmd(cmd,
			 cmd_done,
			 PMCRAID_SET_SUP_DEV_TIMEOUT,
			 pmcraid_timeout_handler);
	return;
}

static void pmcraid_set_timestamp(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	struct pmcraid_ioarcb *ioarcb = &cmd->ioa_cb->ioarcb;
	__be32 time_stamp_len = cpu_to_be32(PMCRAID_TIMESTAMP_LEN);
	struct pmcraid_ioadl_desc *ioadl = ioarcb->add_data.u.ioadl;

	struct timeval tv;
	__le64 timestamp;

	do_gettimeofday(&tv);
	timestamp = tv.tv_sec * 1000;

	pinstance->timestamp_data->timestamp[0] = (__u8)(timestamp);
	pinstance->timestamp_data->timestamp[1] = (__u8)((timestamp) >> 8);
	pinstance->timestamp_data->timestamp[2] = (__u8)((timestamp) >> 16);
	pinstance->timestamp_data->timestamp[3] = (__u8)((timestamp) >> 24);
	pinstance->timestamp_data->timestamp[4] = (__u8)((timestamp) >> 32);
	pinstance->timestamp_data->timestamp[5] = (__u8)((timestamp)  >> 40);

	pmcraid_reinit_cmdblk(cmd);
	ioarcb->request_type = REQ_TYPE_SCSI;
	ioarcb->resource_handle = cpu_to_le32(PMCRAID_IOA_RES_HANDLE);
	ioarcb->cdb[0] = PMCRAID_SCSI_SET_TIMESTAMP;
	ioarcb->cdb[1] = PMCRAID_SCSI_SERVICE_ACTION;
	memcpy(&(ioarcb->cdb[6]), &time_stamp_len, sizeof(time_stamp_len));

	ioarcb->ioadl_bus_addr = cpu_to_le64((cmd->ioa_cb_bus_addr) +
					offsetof(struct pmcraid_ioarcb,
						add_data.u.ioadl[0]));
	ioarcb->ioadl_length = cpu_to_le32(sizeof(struct pmcraid_ioadl_desc));
	ioarcb->ioarcb_bus_addr &= ~(0x1FULL);

	ioarcb->request_flags0 |= NO_LINK_DESCS;
	ioarcb->request_flags0 |= TRANSFER_DIR_WRITE;
	ioarcb->data_transfer_length =
		cpu_to_le32(sizeof(struct pmcraid_timestamp_data));
	ioadl = &(ioarcb->add_data.u.ioadl[0]);
	ioadl->flags = IOADL_FLAGS_LAST_DESC;
	ioadl->address = cpu_to_le64(pinstance->timestamp_data_baddr);
	ioadl->data_len = cpu_to_le32(sizeof(struct pmcraid_timestamp_data));

	if (!pinstance->timestamp_error) {
		pinstance->timestamp_error = 0;
		pmcraid_send_cmd(cmd, pmcraid_set_supported_devs,
			 PMCRAID_INTERNAL_TIMEOUT, pmcraid_timeout_handler);
	} else {
		pmcraid_send_cmd(cmd, pmcraid_return_cmd,
			 PMCRAID_INTERNAL_TIMEOUT, pmcraid_timeout_handler);
		return;
	}
}


static void pmcraid_init_res_table(struct pmcraid_cmd *cmd)
{
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	struct pmcraid_resource_entry *res, *temp;
	struct pmcraid_config_table_entry *cfgte;
	unsigned long lock_flags;
	int found, rc, i;
	u16 fw_version;
	LIST_HEAD(old_res);

	if (pinstance->cfg_table->flags & MICROCODE_UPDATE_REQUIRED)
		pmcraid_err("IOA requires microcode download\n");

	fw_version = be16_to_cpu(pinstance->inq_data->fw_version);

	spin_lock_irqsave(&pinstance->resource_lock, lock_flags);

	list_for_each_entry_safe(res, temp, &pinstance->used_res_q, queue)
		list_move_tail(&res->queue, &old_res);

	for (i = 0; i < pinstance->cfg_table->num_entries; i++) {
		if (be16_to_cpu(pinstance->inq_data->fw_version) <=
						PMCRAID_FW_VERSION_1)
			cfgte = &pinstance->cfg_table->entries[i];
		else
			cfgte = (struct pmcraid_config_table_entry *)
					&pinstance->cfg_table->entries_ext[i];

		if (!pmcraid_expose_resource(fw_version, cfgte))
			continue;

		found = 0;

		
		list_for_each_entry_safe(res, temp, &old_res, queue) {

			rc = memcmp(&res->cfg_entry.resource_address,
				    &cfgte->resource_address,
				    sizeof(cfgte->resource_address));
			if (!rc) {
				list_move_tail(&res->queue,
						&pinstance->used_res_q);
				found = 1;
				break;
			}
		}

		
		if (!found) {

			if (list_empty(&pinstance->free_res_q)) {
				pmcraid_err("Too many devices attached\n");
				break;
			}

			found = 1;
			res = list_entry(pinstance->free_res_q.next,
					 struct pmcraid_resource_entry, queue);

			res->scsi_dev = NULL;
			res->change_detected = RES_CHANGE_ADD;
			res->reset_progress = 0;
			list_move_tail(&res->queue, &pinstance->used_res_q);
		}

		if (found) {
			memcpy(&res->cfg_entry, cfgte,
					pinstance->config_table_entry_size);
			pmcraid_info("New res type:%x, vset:%x, addr:%x:\n",
				 res->cfg_entry.resource_type,
				 (fw_version <= PMCRAID_FW_VERSION_1 ?
					res->cfg_entry.unique_flags1 :
						res->cfg_entry.array_id & 0xFF),
				 le32_to_cpu(res->cfg_entry.resource_address));
		}
	}

	
	list_for_each_entry_safe(res, temp, &old_res, queue) {

		if (res->scsi_dev) {
			res->change_detected = RES_CHANGE_DEL;
			res->cfg_entry.resource_handle =
				PMCRAID_INVALID_RES_HANDLE;
			list_move_tail(&res->queue, &pinstance->used_res_q);
		} else {
			list_move_tail(&res->queue, &pinstance->free_res_q);
		}
	}

	
	spin_unlock_irqrestore(&pinstance->resource_lock, lock_flags);
	pmcraid_set_timestamp(cmd);
}

static void pmcraid_querycfg(struct pmcraid_cmd *cmd)
{
	struct pmcraid_ioarcb *ioarcb = &cmd->ioa_cb->ioarcb;
	struct pmcraid_ioadl_desc *ioadl = ioarcb->add_data.u.ioadl;
	struct pmcraid_instance *pinstance = cmd->drv_inst;
	int cfg_table_size = cpu_to_be32(sizeof(struct pmcraid_config_table));

	if (be16_to_cpu(pinstance->inq_data->fw_version) <=
					PMCRAID_FW_VERSION_1)
		pinstance->config_table_entry_size =
			sizeof(struct pmcraid_config_table_entry);
	else
		pinstance->config_table_entry_size =
			sizeof(struct pmcraid_config_table_entry_ext);

	ioarcb->request_type = REQ_TYPE_IOACMD;
	ioarcb->resource_handle = cpu_to_le32(PMCRAID_IOA_RES_HANDLE);

	ioarcb->cdb[0] = PMCRAID_QUERY_IOA_CONFIG;

	
	memcpy(&(ioarcb->cdb[10]), &cfg_table_size, sizeof(cfg_table_size));

	ioarcb->ioadl_bus_addr = cpu_to_le64((cmd->ioa_cb_bus_addr) +
					offsetof(struct pmcraid_ioarcb,
						add_data.u.ioadl[0]));
	ioarcb->ioadl_length = cpu_to_le32(sizeof(struct pmcraid_ioadl_desc));
	ioarcb->ioarcb_bus_addr &= ~(0x1FULL);

	ioarcb->request_flags0 |= NO_LINK_DESCS;
	ioarcb->data_transfer_length =
		cpu_to_le32(sizeof(struct pmcraid_config_table));

	ioadl = &(ioarcb->add_data.u.ioadl[0]);
	ioadl->flags = IOADL_FLAGS_LAST_DESC;
	ioadl->address = cpu_to_le64(pinstance->cfg_table_bus_addr);
	ioadl->data_len = cpu_to_le32(sizeof(struct pmcraid_config_table));

	pmcraid_send_cmd(cmd, pmcraid_init_res_table,
			 PMCRAID_INTERNAL_TIMEOUT, pmcraid_timeout_handler);
}


static int __devinit pmcraid_probe(
	struct pci_dev *pdev,
	const struct pci_device_id *dev_id
)
{
	struct pmcraid_instance *pinstance;
	struct Scsi_Host *host;
	void __iomem *mapped_pci_addr;
	int rc = PCIBIOS_SUCCESSFUL;

	if (atomic_read(&pmcraid_adapter_count) >= PMCRAID_MAX_ADAPTERS) {
		pmcraid_err
			("maximum number(%d) of supported adapters reached\n",
			 atomic_read(&pmcraid_adapter_count));
		return -ENOMEM;
	}

	atomic_inc(&pmcraid_adapter_count);
	rc = pci_enable_device(pdev);

	if (rc) {
		dev_err(&pdev->dev, "Cannot enable adapter\n");
		atomic_dec(&pmcraid_adapter_count);
		return rc;
	}

	dev_info(&pdev->dev,
		"Found new IOA(%x:%x), Total IOA count: %d\n",
		 pdev->vendor, pdev->device,
		 atomic_read(&pmcraid_adapter_count));

	rc = pci_request_regions(pdev, PMCRAID_DRIVER_NAME);

	if (rc < 0) {
		dev_err(&pdev->dev,
			"Couldn't register memory range of registers\n");
		goto out_disable_device;
	}

	mapped_pci_addr = pci_iomap(pdev, 0, 0);

	if (!mapped_pci_addr) {
		dev_err(&pdev->dev, "Couldn't map PCI registers memory\n");
		rc = -ENOMEM;
		goto out_release_regions;
	}

	pci_set_master(pdev);

	if ((sizeof(dma_addr_t) == 4) ||
	    pci_set_dma_mask(pdev, DMA_BIT_MASK(64)))
		rc = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));

	if (rc == 0)
		rc = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));

	if (rc != 0) {
		dev_err(&pdev->dev, "Failed to set PCI DMA mask\n");
		goto cleanup_nomem;
	}

	host = scsi_host_alloc(&pmcraid_host_template,
				sizeof(struct pmcraid_instance));

	if (!host) {
		dev_err(&pdev->dev, "scsi_host_alloc failed!\n");
		rc = -ENOMEM;
		goto cleanup_nomem;
	}

	host->max_id = PMCRAID_MAX_NUM_TARGETS_PER_BUS;
	host->max_lun = PMCRAID_MAX_NUM_LUNS_PER_TARGET;
	host->unique_id = host->host_no;
	host->max_channel = PMCRAID_MAX_BUS_TO_SCAN;
	host->max_cmd_len = PMCRAID_MAX_CDB_LEN;

	
	pinstance = (struct pmcraid_instance *)host->hostdata;
	memset(pinstance, 0, sizeof(*pinstance));

	pinstance->chip_cfg =
		(struct pmcraid_chip_details *)(dev_id->driver_data);

	rc = pmcraid_init_instance(pdev, host, mapped_pci_addr);

	if (rc < 0) {
		dev_err(&pdev->dev, "failed to initialize adapter instance\n");
		goto out_scsi_host_put;
	}

	pci_set_drvdata(pdev, pinstance);

	
	rc = pci_save_state(pinstance->pdev);

	if (rc != 0) {
		dev_err(&pdev->dev, "Failed to save PCI config space\n");
		goto out_scsi_host_put;
	}

	pmcraid_disable_interrupts(pinstance, ~0);

	rc = pmcraid_register_interrupt_handler(pinstance);

	if (rc) {
		dev_err(&pdev->dev, "couldn't register interrupt handler\n");
		goto out_scsi_host_put;
	}

	pmcraid_init_tasklets(pinstance);

	
	rc = pmcraid_init_buffers(pinstance);

	if (rc) {
		pmcraid_err("couldn't allocate memory blocks\n");
		goto out_unregister_isr;
	}

	
	pmcraid_reset_type(pinstance);

	pmcraid_enable_interrupts(pinstance, PMCRAID_PCI_INTERRUPTS);

	pmcraid_info("starting IOA initialization sequence\n");
	if (pmcraid_reset_bringup(pinstance)) {
		dev_err(&pdev->dev, "couldn't initialize IOA\n");
		rc = 1;
		goto out_release_bufs;
	}

	
	rc = scsi_add_host(pinstance->host, &pdev->dev);
	if (rc != 0) {
		pmcraid_err("couldn't add host into mid-layer: %d\n", rc);
		goto out_release_bufs;
	}

	scsi_scan_host(pinstance->host);

	rc = pmcraid_setup_chrdev(pinstance);

	if (rc != 0) {
		pmcraid_err("couldn't create mgmt interface, error: %x\n",
			     rc);
		goto out_remove_host;
	}

	atomic_set(&pinstance->expose_resources, 1);
	schedule_work(&pinstance->worker_q);
	return rc;

out_remove_host:
	scsi_remove_host(host);

out_release_bufs:
	pmcraid_release_buffers(pinstance);

out_unregister_isr:
	pmcraid_kill_tasklets(pinstance);
	pmcraid_unregister_interrupt_handler(pinstance);

out_scsi_host_put:
	scsi_host_put(host);

cleanup_nomem:
	iounmap(mapped_pci_addr);

out_release_regions:
	pci_release_regions(pdev);

out_disable_device:
	atomic_dec(&pmcraid_adapter_count);
	pci_set_drvdata(pdev, NULL);
	pci_disable_device(pdev);
	return -ENODEV;
}

static struct pci_driver pmcraid_driver = {
	.name = PMCRAID_DRIVER_NAME,
	.id_table = pmcraid_pci_table,
	.probe = pmcraid_probe,
	.remove = pmcraid_remove,
	.suspend = pmcraid_suspend,
	.resume = pmcraid_resume,
	.shutdown = pmcraid_shutdown
};

static int __init pmcraid_init(void)
{
	dev_t dev;
	int error;

	pmcraid_info("%s Device Driver version: %s\n",
			 PMCRAID_DRIVER_NAME, PMCRAID_DRIVER_VERSION);

	error = alloc_chrdev_region(&dev, 0,
				    PMCRAID_MAX_ADAPTERS,
				    PMCRAID_DEVFILE);

	if (error) {
		pmcraid_err("failed to get a major number for adapters\n");
		goto out_init;
	}

	pmcraid_major = MAJOR(dev);
	pmcraid_class = class_create(THIS_MODULE, PMCRAID_DEVFILE);

	if (IS_ERR(pmcraid_class)) {
		error = PTR_ERR(pmcraid_class);
		pmcraid_err("failed to register with with sysfs, error = %x\n",
			    error);
		goto out_unreg_chrdev;
	}

	error = pmcraid_netlink_init();

	if (error)
		goto out_unreg_chrdev;

	error = pci_register_driver(&pmcraid_driver);

	if (error == 0)
		goto out_init;

	pmcraid_err("failed to register pmcraid driver, error = %x\n",
		     error);
	class_destroy(pmcraid_class);
	pmcraid_netlink_release();

out_unreg_chrdev:
	unregister_chrdev_region(MKDEV(pmcraid_major, 0), PMCRAID_MAX_ADAPTERS);

out_init:
	return error;
}

static void __exit pmcraid_exit(void)
{
	pmcraid_netlink_release();
	unregister_chrdev_region(MKDEV(pmcraid_major, 0),
				 PMCRAID_MAX_ADAPTERS);
	pci_unregister_driver(&pmcraid_driver);
	class_destroy(pmcraid_class);
}

module_init(pmcraid_init);
module_exit(pmcraid_exit);
