/*
 *  Linux MegaRAID driver for SAS based RAID controllers
 *
 *  Copyright (c) 2009-2011  LSI Corporation.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *  FILE: megaraid_sas_base.c
 *  Version : v00.00.06.14-rc1
 *
 *  Authors: LSI Corporation
 *           Sreenivas Bagalkote
 *           Sumant Patro
 *           Bo Yang
 *           Adam Radford <linuxraid@lsi.com>
 *
 *  Send feedback to: <megaraidlinux@lsi.com>
 *
 *  Mail to: LSI Corporation, 1621 Barber Lane, Milpitas, CA 95035
 *     ATTN: Linuxraid
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/list.h>
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/uio.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/compat.h>
#include <linux/blkdev.h>
#include <linux/mutex.h>
#include <linux/poll.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include "megaraid_sas_fusion.h"
#include "megaraid_sas.h"

static unsigned int max_sectors;
module_param_named(max_sectors, max_sectors, int, 0);
MODULE_PARM_DESC(max_sectors,
	"Maximum number of sectors per IO command");

static int msix_disable;
module_param(msix_disable, int, S_IRUGO);
MODULE_PARM_DESC(msix_disable, "Disable MSI-X interrupt handling. Default: 0");

MODULE_LICENSE("GPL");
MODULE_VERSION(MEGASAS_VERSION);
MODULE_AUTHOR("megaraidlinux@lsi.com");
MODULE_DESCRIPTION("LSI MegaRAID SAS Driver");

int megasas_transition_to_ready(struct megasas_instance *instance, int ocr);
static int megasas_get_pd_list(struct megasas_instance *instance);
static int megasas_issue_init_mfi(struct megasas_instance *instance);
static int megasas_register_aen(struct megasas_instance *instance,
				u32 seq_num, u32 class_locale_word);
static struct pci_device_id megasas_pci_table[] = {

	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS1064R)},
	
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS1078R)},
	
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS1078DE)},
	
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS1078GEN2)},
	
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS0079GEN2)},
	
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS0073SKINNY)},
	
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS0071SKINNY)},
	
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_VERDE_ZCR)},
	
	{PCI_DEVICE(PCI_VENDOR_ID_DELL, PCI_DEVICE_ID_DELL_PERC5)},
	
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_FUSION)},
	
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_INVADER)},
	
	{}
};

MODULE_DEVICE_TABLE(pci, megasas_pci_table);

static int megasas_mgmt_majorno;
static struct megasas_mgmt_info megasas_mgmt_info;
static struct fasync_struct *megasas_async_queue;
static DEFINE_MUTEX(megasas_async_queue_mutex);

static int megasas_poll_wait_aen;
static DECLARE_WAIT_QUEUE_HEAD(megasas_poll_wait);
static u32 support_poll_for_event;
u32 megasas_dbg_lvl;
static u32 support_device_change;

spinlock_t poll_aen_lock;

void
megasas_complete_cmd(struct megasas_instance *instance, struct megasas_cmd *cmd,
		     u8 alt_status);
static u32
megasas_read_fw_status_reg_gen2(struct megasas_register_set __iomem *regs);
static int
megasas_adp_reset_gen2(struct megasas_instance *instance,
		       struct megasas_register_set __iomem *reg_set);
static irqreturn_t megasas_isr(int irq, void *devp);
static u32
megasas_init_adapter_mfi(struct megasas_instance *instance);
u32
megasas_build_and_issue_cmd(struct megasas_instance *instance,
			    struct scsi_cmnd *scmd);
static void megasas_complete_cmd_dpc(unsigned long instance_addr);
void
megasas_release_fusion(struct megasas_instance *instance);
int
megasas_ioc_init_fusion(struct megasas_instance *instance);
void
megasas_free_cmds_fusion(struct megasas_instance *instance);
u8
megasas_get_map_info(struct megasas_instance *instance);
int
megasas_sync_map_info(struct megasas_instance *instance);
int
wait_and_poll(struct megasas_instance *instance, struct megasas_cmd *cmd);
void megasas_reset_reply_desc(struct megasas_instance *instance);
u8 MR_ValidateMapInfo(struct MR_FW_RAID_MAP_ALL *map,
		      struct LD_LOAD_BALANCE_INFO *lbInfo);
int megasas_reset_fusion(struct Scsi_Host *shost);
void megasas_fusion_ocr_wq(struct work_struct *work);

void
megasas_issue_dcmd(struct megasas_instance *instance, struct megasas_cmd *cmd)
{
	instance->instancet->fire_cmd(instance,
		cmd->frame_phys_addr, 0, instance->reg_set);
}

struct megasas_cmd *megasas_get_cmd(struct megasas_instance
						  *instance)
{
	unsigned long flags;
	struct megasas_cmd *cmd = NULL;

	spin_lock_irqsave(&instance->cmd_pool_lock, flags);

	if (!list_empty(&instance->cmd_pool)) {
		cmd = list_entry((&instance->cmd_pool)->next,
				 struct megasas_cmd, list);
		list_del_init(&cmd->list);
	} else {
		printk(KERN_ERR "megasas: Command pool empty!\n");
	}

	spin_unlock_irqrestore(&instance->cmd_pool_lock, flags);
	return cmd;
}

inline void
megasas_return_cmd(struct megasas_instance *instance, struct megasas_cmd *cmd)
{
	unsigned long flags;

	spin_lock_irqsave(&instance->cmd_pool_lock, flags);

	cmd->scmd = NULL;
	cmd->frame_count = 0;
	if ((instance->pdev->device != PCI_DEVICE_ID_LSI_FUSION) &&
	    (instance->pdev->device != PCI_DEVICE_ID_LSI_INVADER) &&
	    (reset_devices))
		cmd->frame->hdr.cmd = MFI_CMD_INVALID;
	list_add_tail(&cmd->list, &instance->cmd_pool);

	spin_unlock_irqrestore(&instance->cmd_pool_lock, flags);
}



static inline void
megasas_enable_intr_xscale(struct megasas_register_set __iomem * regs)
{
	writel(0, &(regs)->outbound_intr_mask);

	
	readl(&regs->outbound_intr_mask);
}

static inline void
megasas_disable_intr_xscale(struct megasas_register_set __iomem * regs)
{
	u32 mask = 0x1f;
	writel(mask, &regs->outbound_intr_mask);
	
	readl(&regs->outbound_intr_mask);
}

static u32
megasas_read_fw_status_reg_xscale(struct megasas_register_set __iomem * regs)
{
	return readl(&(regs)->outbound_msg_0);
}
static int
megasas_clear_intr_xscale(struct megasas_register_set __iomem * regs)
{
	u32 status;
	u32 mfiStatus = 0;
	status = readl(&regs->outbound_intr_status);

	if (status & MFI_OB_INTR_STATUS_MASK)
		mfiStatus = MFI_INTR_FLAG_REPLY_MESSAGE;
	if (status & MFI_XSCALE_OMR0_CHANGE_INTERRUPT)
		mfiStatus |= MFI_INTR_FLAG_FIRMWARE_STATE_CHANGE;

	if (mfiStatus)
		writel(status, &regs->outbound_intr_status);

	
	readl(&regs->outbound_intr_status);

	return mfiStatus;
}

static inline void
megasas_fire_cmd_xscale(struct megasas_instance *instance,
		dma_addr_t frame_phys_addr,
		u32 frame_count,
		struct megasas_register_set __iomem *regs)
{
	unsigned long flags;
	spin_lock_irqsave(&instance->hba_lock, flags);
	writel((frame_phys_addr >> 3)|(frame_count),
	       &(regs)->inbound_queue_port);
	spin_unlock_irqrestore(&instance->hba_lock, flags);
}

static int
megasas_adp_reset_xscale(struct megasas_instance *instance,
	struct megasas_register_set __iomem *regs)
{
	u32 i;
	u32 pcidata;
	writel(MFI_ADP_RESET, &regs->inbound_doorbell);

	for (i = 0; i < 3; i++)
		msleep(1000); 
	pcidata  = 0;
	pci_read_config_dword(instance->pdev, MFI_1068_PCSR_OFFSET, &pcidata);
	printk(KERN_NOTICE "pcidata = %x\n", pcidata);
	if (pcidata & 0x2) {
		printk(KERN_NOTICE "mfi 1068 offset read=%x\n", pcidata);
		pcidata &= ~0x2;
		pci_write_config_dword(instance->pdev,
				MFI_1068_PCSR_OFFSET, pcidata);

		for (i = 0; i < 2; i++)
			msleep(1000); 

		pcidata  = 0;
		pci_read_config_dword(instance->pdev,
				MFI_1068_FW_HANDSHAKE_OFFSET, &pcidata);
		printk(KERN_NOTICE "1068 offset handshake read=%x\n", pcidata);
		if ((pcidata & 0xffff0000) == MFI_1068_FW_READY) {
			printk(KERN_NOTICE "1068 offset pcidt=%x\n", pcidata);
			pcidata = 0;
			pci_write_config_dword(instance->pdev,
				MFI_1068_FW_HANDSHAKE_OFFSET, pcidata);
		}
	}
	return 0;
}

static int
megasas_check_reset_xscale(struct megasas_instance *instance,
		struct megasas_register_set __iomem *regs)
{
	u32 consumer;
	consumer = *instance->consumer;

	if ((instance->adprecovery != MEGASAS_HBA_OPERATIONAL) &&
		(*instance->consumer == MEGASAS_ADPRESET_INPROG_SIGN)) {
		return 1;
	}
	return 0;
}

static struct megasas_instance_template megasas_instance_template_xscale = {

	.fire_cmd = megasas_fire_cmd_xscale,
	.enable_intr = megasas_enable_intr_xscale,
	.disable_intr = megasas_disable_intr_xscale,
	.clear_intr = megasas_clear_intr_xscale,
	.read_fw_status_reg = megasas_read_fw_status_reg_xscale,
	.adp_reset = megasas_adp_reset_xscale,
	.check_reset = megasas_check_reset_xscale,
	.service_isr = megasas_isr,
	.tasklet = megasas_complete_cmd_dpc,
	.init_adapter = megasas_init_adapter_mfi,
	.build_and_issue_cmd = megasas_build_and_issue_cmd,
	.issue_dcmd = megasas_issue_dcmd,
};



static inline void
megasas_enable_intr_ppc(struct megasas_register_set __iomem * regs)
{
	writel(0xFFFFFFFF, &(regs)->outbound_doorbell_clear);

	writel(~0x80000000, &(regs)->outbound_intr_mask);

	
	readl(&regs->outbound_intr_mask);
}

static inline void
megasas_disable_intr_ppc(struct megasas_register_set __iomem * regs)
{
	u32 mask = 0xFFFFFFFF;
	writel(mask, &regs->outbound_intr_mask);
	
	readl(&regs->outbound_intr_mask);
}

static u32
megasas_read_fw_status_reg_ppc(struct megasas_register_set __iomem * regs)
{
	return readl(&(regs)->outbound_scratch_pad);
}

static int
megasas_clear_intr_ppc(struct megasas_register_set __iomem * regs)
{
	u32 status, mfiStatus = 0;

	status = readl(&regs->outbound_intr_status);

	if (status & MFI_REPLY_1078_MESSAGE_INTERRUPT)
		mfiStatus = MFI_INTR_FLAG_REPLY_MESSAGE;

	if (status & MFI_G2_OUTBOUND_DOORBELL_CHANGE_INTERRUPT)
		mfiStatus |= MFI_INTR_FLAG_FIRMWARE_STATE_CHANGE;

	writel(status, &regs->outbound_doorbell_clear);

	
	readl(&regs->outbound_doorbell_clear);

	return mfiStatus;
}

static inline void
megasas_fire_cmd_ppc(struct megasas_instance *instance,
		dma_addr_t frame_phys_addr,
		u32 frame_count,
		struct megasas_register_set __iomem *regs)
{
	unsigned long flags;
	spin_lock_irqsave(&instance->hba_lock, flags);
	writel((frame_phys_addr | (frame_count<<1))|1,
			&(regs)->inbound_queue_port);
	spin_unlock_irqrestore(&instance->hba_lock, flags);
}

static int
megasas_check_reset_ppc(struct megasas_instance *instance,
			struct megasas_register_set __iomem *regs)
{
	if (instance->adprecovery != MEGASAS_HBA_OPERATIONAL)
		return 1;

	return 0;
}

static struct megasas_instance_template megasas_instance_template_ppc = {

	.fire_cmd = megasas_fire_cmd_ppc,
	.enable_intr = megasas_enable_intr_ppc,
	.disable_intr = megasas_disable_intr_ppc,
	.clear_intr = megasas_clear_intr_ppc,
	.read_fw_status_reg = megasas_read_fw_status_reg_ppc,
	.adp_reset = megasas_adp_reset_xscale,
	.check_reset = megasas_check_reset_ppc,
	.service_isr = megasas_isr,
	.tasklet = megasas_complete_cmd_dpc,
	.init_adapter = megasas_init_adapter_mfi,
	.build_and_issue_cmd = megasas_build_and_issue_cmd,
	.issue_dcmd = megasas_issue_dcmd,
};

static inline void
megasas_enable_intr_skinny(struct megasas_register_set __iomem *regs)
{
	writel(0xFFFFFFFF, &(regs)->outbound_intr_mask);

	writel(~MFI_SKINNY_ENABLE_INTERRUPT_MASK, &(regs)->outbound_intr_mask);

	
	readl(&regs->outbound_intr_mask);
}

static inline void
megasas_disable_intr_skinny(struct megasas_register_set __iomem *regs)
{
	u32 mask = 0xFFFFFFFF;
	writel(mask, &regs->outbound_intr_mask);
	
	readl(&regs->outbound_intr_mask);
}

static u32
megasas_read_fw_status_reg_skinny(struct megasas_register_set __iomem *regs)
{
	return readl(&(regs)->outbound_scratch_pad);
}

static int
megasas_clear_intr_skinny(struct megasas_register_set __iomem *regs)
{
	u32 status;
	u32 mfiStatus = 0;

	status = readl(&regs->outbound_intr_status);

	if (!(status & MFI_SKINNY_ENABLE_INTERRUPT_MASK)) {
		return 0;
	}

	if ((megasas_read_fw_status_reg_gen2(regs) & MFI_STATE_MASK) ==
	    MFI_STATE_FAULT) {
		mfiStatus = MFI_INTR_FLAG_FIRMWARE_STATE_CHANGE;
	} else
		mfiStatus = MFI_INTR_FLAG_REPLY_MESSAGE;

	writel(status, &regs->outbound_intr_status);

	readl(&regs->outbound_intr_status);

	return mfiStatus;
}

static inline void
megasas_fire_cmd_skinny(struct megasas_instance *instance,
			dma_addr_t frame_phys_addr,
			u32 frame_count,
			struct megasas_register_set __iomem *regs)
{
	unsigned long flags;
	spin_lock_irqsave(&instance->hba_lock, flags);
	writel(0, &(regs)->inbound_high_queue_port);
	writel((frame_phys_addr | (frame_count<<1))|1,
		&(regs)->inbound_low_queue_port);
	spin_unlock_irqrestore(&instance->hba_lock, flags);
}

static int
megasas_check_reset_skinny(struct megasas_instance *instance,
				struct megasas_register_set __iomem *regs)
{
	if (instance->adprecovery != MEGASAS_HBA_OPERATIONAL)
		return 1;

	return 0;
}

static struct megasas_instance_template megasas_instance_template_skinny = {

	.fire_cmd = megasas_fire_cmd_skinny,
	.enable_intr = megasas_enable_intr_skinny,
	.disable_intr = megasas_disable_intr_skinny,
	.clear_intr = megasas_clear_intr_skinny,
	.read_fw_status_reg = megasas_read_fw_status_reg_skinny,
	.adp_reset = megasas_adp_reset_gen2,
	.check_reset = megasas_check_reset_skinny,
	.service_isr = megasas_isr,
	.tasklet = megasas_complete_cmd_dpc,
	.init_adapter = megasas_init_adapter_mfi,
	.build_and_issue_cmd = megasas_build_and_issue_cmd,
	.issue_dcmd = megasas_issue_dcmd,
};



static inline void
megasas_enable_intr_gen2(struct megasas_register_set __iomem *regs)
{
	writel(0xFFFFFFFF, &(regs)->outbound_doorbell_clear);

	
	writel(~MFI_GEN2_ENABLE_INTERRUPT_MASK, &(regs)->outbound_intr_mask);

	
	readl(&regs->outbound_intr_mask);
}

static inline void
megasas_disable_intr_gen2(struct megasas_register_set __iomem *regs)
{
	u32 mask = 0xFFFFFFFF;
	writel(mask, &regs->outbound_intr_mask);
	
	readl(&regs->outbound_intr_mask);
}

static u32
megasas_read_fw_status_reg_gen2(struct megasas_register_set __iomem *regs)
{
	return readl(&(regs)->outbound_scratch_pad);
}

static int
megasas_clear_intr_gen2(struct megasas_register_set __iomem *regs)
{
	u32 status;
	u32 mfiStatus = 0;
	status = readl(&regs->outbound_intr_status);

	if (status & MFI_GEN2_ENABLE_INTERRUPT_MASK) {
		mfiStatus = MFI_INTR_FLAG_REPLY_MESSAGE;
	}
	if (status & MFI_G2_OUTBOUND_DOORBELL_CHANGE_INTERRUPT) {
		mfiStatus |= MFI_INTR_FLAG_FIRMWARE_STATE_CHANGE;
	}

	if (mfiStatus)
		writel(status, &regs->outbound_doorbell_clear);

	
	readl(&regs->outbound_intr_status);

	return mfiStatus;
}
static inline void
megasas_fire_cmd_gen2(struct megasas_instance *instance,
			dma_addr_t frame_phys_addr,
			u32 frame_count,
			struct megasas_register_set __iomem *regs)
{
	unsigned long flags;
	spin_lock_irqsave(&instance->hba_lock, flags);
	writel((frame_phys_addr | (frame_count<<1))|1,
			&(regs)->inbound_queue_port);
	spin_unlock_irqrestore(&instance->hba_lock, flags);
}

static int
megasas_adp_reset_gen2(struct megasas_instance *instance,
			struct megasas_register_set __iomem *reg_set)
{
	u32			retry = 0 ;
	u32			HostDiag;
	u32			*seq_offset = &reg_set->seq_offset;
	u32			*hostdiag_offset = &reg_set->host_diag;

	if (instance->instancet == &megasas_instance_template_skinny) {
		seq_offset = &reg_set->fusion_seq_offset;
		hostdiag_offset = &reg_set->fusion_host_diag;
	}

	writel(0, seq_offset);
	writel(4, seq_offset);
	writel(0xb, seq_offset);
	writel(2, seq_offset);
	writel(7, seq_offset);
	writel(0xd, seq_offset);

	msleep(1000);

	HostDiag = (u32)readl(hostdiag_offset);

	while ( !( HostDiag & DIAG_WRITE_ENABLE) ) {
		msleep(100);
		HostDiag = (u32)readl(hostdiag_offset);
		printk(KERN_NOTICE "RESETGEN2: retry=%x, hostdiag=%x\n",
					retry, HostDiag);

		if (retry++ >= 100)
			return 1;

	}

	printk(KERN_NOTICE "ADP_RESET_GEN2: HostDiag=%x\n", HostDiag);

	writel((HostDiag | DIAG_RESET_ADAPTER), hostdiag_offset);

	ssleep(10);

	HostDiag = (u32)readl(hostdiag_offset);
	while ( ( HostDiag & DIAG_RESET_ADAPTER) ) {
		msleep(100);
		HostDiag = (u32)readl(hostdiag_offset);
		printk(KERN_NOTICE "RESET_GEN2: retry=%x, hostdiag=%x\n",
				retry, HostDiag);

		if (retry++ >= 1000)
			return 1;

	}
	return 0;
}

static int
megasas_check_reset_gen2(struct megasas_instance *instance,
		struct megasas_register_set __iomem *regs)
{
	if (instance->adprecovery != MEGASAS_HBA_OPERATIONAL) {
		return 1;
	}

	return 0;
}

static struct megasas_instance_template megasas_instance_template_gen2 = {

	.fire_cmd = megasas_fire_cmd_gen2,
	.enable_intr = megasas_enable_intr_gen2,
	.disable_intr = megasas_disable_intr_gen2,
	.clear_intr = megasas_clear_intr_gen2,
	.read_fw_status_reg = megasas_read_fw_status_reg_gen2,
	.adp_reset = megasas_adp_reset_gen2,
	.check_reset = megasas_check_reset_gen2,
	.service_isr = megasas_isr,
	.tasklet = megasas_complete_cmd_dpc,
	.init_adapter = megasas_init_adapter_mfi,
	.build_and_issue_cmd = megasas_build_and_issue_cmd,
	.issue_dcmd = megasas_issue_dcmd,
};


extern struct megasas_instance_template megasas_instance_template_fusion;

int
megasas_issue_polled(struct megasas_instance *instance, struct megasas_cmd *cmd)
{

	struct megasas_header *frame_hdr = &cmd->frame->hdr;

	frame_hdr->cmd_status = 0xFF;
	frame_hdr->flags |= MFI_FRAME_DONT_POST_IN_REPLY_QUEUE;

	instance->instancet->issue_dcmd(instance, cmd);

	return wait_and_poll(instance, cmd);
}

static int
megasas_issue_blocked_cmd(struct megasas_instance *instance,
			  struct megasas_cmd *cmd)
{
	cmd->cmd_status = ENODATA;

	instance->instancet->issue_dcmd(instance, cmd);

	wait_event(instance->int_cmd_wait_q, cmd->cmd_status != ENODATA);

	return 0;
}

static int
megasas_issue_blocked_abort_cmd(struct megasas_instance *instance,
				struct megasas_cmd *cmd_to_abort)
{
	struct megasas_cmd *cmd;
	struct megasas_abort_frame *abort_fr;

	cmd = megasas_get_cmd(instance);

	if (!cmd)
		return -1;

	abort_fr = &cmd->frame->abort;

	abort_fr->cmd = MFI_CMD_ABORT;
	abort_fr->cmd_status = 0xFF;
	abort_fr->flags = 0;
	abort_fr->abort_context = cmd_to_abort->index;
	abort_fr->abort_mfi_phys_addr_lo = cmd_to_abort->frame_phys_addr;
	abort_fr->abort_mfi_phys_addr_hi = 0;

	cmd->sync_cmd = 1;
	cmd->cmd_status = 0xFF;

	instance->instancet->issue_dcmd(instance, cmd);

	wait_event(instance->abort_cmd_wait_q, cmd->cmd_status != 0xFF);
	cmd->sync_cmd = 0;

	megasas_return_cmd(instance, cmd);
	return 0;
}

static int
megasas_make_sgl32(struct megasas_instance *instance, struct scsi_cmnd *scp,
		   union megasas_sgl *mfi_sgl)
{
	int i;
	int sge_count;
	struct scatterlist *os_sgl;

	sge_count = scsi_dma_map(scp);
	BUG_ON(sge_count < 0);

	if (sge_count) {
		scsi_for_each_sg(scp, os_sgl, sge_count, i) {
			mfi_sgl->sge32[i].length = sg_dma_len(os_sgl);
			mfi_sgl->sge32[i].phys_addr = sg_dma_address(os_sgl);
		}
	}
	return sge_count;
}

static int
megasas_make_sgl64(struct megasas_instance *instance, struct scsi_cmnd *scp,
		   union megasas_sgl *mfi_sgl)
{
	int i;
	int sge_count;
	struct scatterlist *os_sgl;

	sge_count = scsi_dma_map(scp);
	BUG_ON(sge_count < 0);

	if (sge_count) {
		scsi_for_each_sg(scp, os_sgl, sge_count, i) {
			mfi_sgl->sge64[i].length = sg_dma_len(os_sgl);
			mfi_sgl->sge64[i].phys_addr = sg_dma_address(os_sgl);
		}
	}
	return sge_count;
}

static int
megasas_make_sgl_skinny(struct megasas_instance *instance,
		struct scsi_cmnd *scp, union megasas_sgl *mfi_sgl)
{
	int i;
	int sge_count;
	struct scatterlist *os_sgl;

	sge_count = scsi_dma_map(scp);

	if (sge_count) {
		scsi_for_each_sg(scp, os_sgl, sge_count, i) {
			mfi_sgl->sge_skinny[i].length = sg_dma_len(os_sgl);
			mfi_sgl->sge_skinny[i].phys_addr =
						sg_dma_address(os_sgl);
			mfi_sgl->sge_skinny[i].flag = 0;
		}
	}
	return sge_count;
}


static u32 megasas_get_frame_count(struct megasas_instance *instance,
			u8 sge_count, u8 frame_type)
{
	int num_cnt;
	int sge_bytes;
	u32 sge_sz;
	u32 frame_count=0;

	sge_sz = (IS_DMA64) ? sizeof(struct megasas_sge64) :
	    sizeof(struct megasas_sge32);

	if (instance->flag_ieee) {
		sge_sz = sizeof(struct megasas_sge_skinny);
	}

	if (unlikely(frame_type == PTHRU_FRAME)) {
		if (instance->flag_ieee == 1) {
			num_cnt = sge_count - 1;
		} else if (IS_DMA64)
			num_cnt = sge_count - 1;
		else
			num_cnt = sge_count - 2;
	} else {
		if (instance->flag_ieee == 1) {
			num_cnt = sge_count - 1;
		} else if (IS_DMA64)
			num_cnt = sge_count - 2;
		else
			num_cnt = sge_count - 3;
	}

	if(num_cnt>0){
		sge_bytes = sge_sz * num_cnt;

		frame_count = (sge_bytes / MEGAMFI_FRAME_SIZE) +
		    ((sge_bytes % MEGAMFI_FRAME_SIZE) ? 1 : 0) ;
	}
	
	frame_count +=1;

	if (frame_count > 7)
		frame_count = 8;
	return frame_count;
}

static int
megasas_build_dcdb(struct megasas_instance *instance, struct scsi_cmnd *scp,
		   struct megasas_cmd *cmd)
{
	u32 is_logical;
	u32 device_id;
	u16 flags = 0;
	struct megasas_pthru_frame *pthru;

	is_logical = MEGASAS_IS_LOGICAL(scp);
	device_id = MEGASAS_DEV_INDEX(instance, scp);
	pthru = (struct megasas_pthru_frame *)cmd->frame;

	if (scp->sc_data_direction == PCI_DMA_TODEVICE)
		flags = MFI_FRAME_DIR_WRITE;
	else if (scp->sc_data_direction == PCI_DMA_FROMDEVICE)
		flags = MFI_FRAME_DIR_READ;
	else if (scp->sc_data_direction == PCI_DMA_NONE)
		flags = MFI_FRAME_DIR_NONE;

	if (instance->flag_ieee == 1) {
		flags |= MFI_FRAME_IEEE;
	}

	pthru->cmd = (is_logical) ? MFI_CMD_LD_SCSI_IO : MFI_CMD_PD_SCSI_IO;
	pthru->cmd_status = 0x0;
	pthru->scsi_status = 0x0;
	pthru->target_id = device_id;
	pthru->lun = scp->device->lun;
	pthru->cdb_len = scp->cmd_len;
	pthru->timeout = 0;
	pthru->pad_0 = 0;
	pthru->flags = flags;
	pthru->data_xfer_len = scsi_bufflen(scp);

	memcpy(pthru->cdb, scp->cmnd, scp->cmd_len);

	if (scp->device->type == TYPE_TAPE) {
		if ((scp->request->timeout / HZ) > 0xFFFF)
			pthru->timeout = 0xFFFF;
		else
			pthru->timeout = scp->request->timeout / HZ;
	}

	if (instance->flag_ieee == 1) {
		pthru->flags |= MFI_FRAME_SGL64;
		pthru->sge_count = megasas_make_sgl_skinny(instance, scp,
						      &pthru->sgl);
	} else if (IS_DMA64) {
		pthru->flags |= MFI_FRAME_SGL64;
		pthru->sge_count = megasas_make_sgl64(instance, scp,
						      &pthru->sgl);
	} else
		pthru->sge_count = megasas_make_sgl32(instance, scp,
						      &pthru->sgl);

	if (pthru->sge_count > instance->max_num_sge) {
		printk(KERN_ERR "megasas: DCDB two many SGE NUM=%x\n",
			pthru->sge_count);
		return 0;
	}

	pthru->sense_len = SCSI_SENSE_BUFFERSIZE;
	pthru->sense_buf_phys_addr_hi = 0;
	pthru->sense_buf_phys_addr_lo = cmd->sense_phys_addr;

	cmd->frame_count = megasas_get_frame_count(instance, pthru->sge_count,
							PTHRU_FRAME);

	return cmd->frame_count;
}

static int
megasas_build_ldio(struct megasas_instance *instance, struct scsi_cmnd *scp,
		   struct megasas_cmd *cmd)
{
	u32 device_id;
	u8 sc = scp->cmnd[0];
	u16 flags = 0;
	struct megasas_io_frame *ldio;

	device_id = MEGASAS_DEV_INDEX(instance, scp);
	ldio = (struct megasas_io_frame *)cmd->frame;

	if (scp->sc_data_direction == PCI_DMA_TODEVICE)
		flags = MFI_FRAME_DIR_WRITE;
	else if (scp->sc_data_direction == PCI_DMA_FROMDEVICE)
		flags = MFI_FRAME_DIR_READ;

	if (instance->flag_ieee == 1) {
		flags |= MFI_FRAME_IEEE;
	}

	ldio->cmd = (sc & 0x02) ? MFI_CMD_LD_WRITE : MFI_CMD_LD_READ;
	ldio->cmd_status = 0x0;
	ldio->scsi_status = 0x0;
	ldio->target_id = device_id;
	ldio->timeout = 0;
	ldio->reserved_0 = 0;
	ldio->pad_0 = 0;
	ldio->flags = flags;
	ldio->start_lba_hi = 0;
	ldio->access_byte = (scp->cmd_len != 6) ? scp->cmnd[1] : 0;

	if (scp->cmd_len == 6) {
		ldio->lba_count = (u32) scp->cmnd[4];
		ldio->start_lba_lo = ((u32) scp->cmnd[1] << 16) |
		    ((u32) scp->cmnd[2] << 8) | (u32) scp->cmnd[3];

		ldio->start_lba_lo &= 0x1FFFFF;
	}

	else if (scp->cmd_len == 10) {
		ldio->lba_count = (u32) scp->cmnd[8] |
		    ((u32) scp->cmnd[7] << 8);
		ldio->start_lba_lo = ((u32) scp->cmnd[2] << 24) |
		    ((u32) scp->cmnd[3] << 16) |
		    ((u32) scp->cmnd[4] << 8) | (u32) scp->cmnd[5];
	}

	else if (scp->cmd_len == 12) {
		ldio->lba_count = ((u32) scp->cmnd[6] << 24) |
		    ((u32) scp->cmnd[7] << 16) |
		    ((u32) scp->cmnd[8] << 8) | (u32) scp->cmnd[9];

		ldio->start_lba_lo = ((u32) scp->cmnd[2] << 24) |
		    ((u32) scp->cmnd[3] << 16) |
		    ((u32) scp->cmnd[4] << 8) | (u32) scp->cmnd[5];
	}

	else if (scp->cmd_len == 16) {
		ldio->lba_count = ((u32) scp->cmnd[10] << 24) |
		    ((u32) scp->cmnd[11] << 16) |
		    ((u32) scp->cmnd[12] << 8) | (u32) scp->cmnd[13];

		ldio->start_lba_lo = ((u32) scp->cmnd[6] << 24) |
		    ((u32) scp->cmnd[7] << 16) |
		    ((u32) scp->cmnd[8] << 8) | (u32) scp->cmnd[9];

		ldio->start_lba_hi = ((u32) scp->cmnd[2] << 24) |
		    ((u32) scp->cmnd[3] << 16) |
		    ((u32) scp->cmnd[4] << 8) | (u32) scp->cmnd[5];

	}

	if (instance->flag_ieee) {
		ldio->flags |= MFI_FRAME_SGL64;
		ldio->sge_count = megasas_make_sgl_skinny(instance, scp,
					      &ldio->sgl);
	} else if (IS_DMA64) {
		ldio->flags |= MFI_FRAME_SGL64;
		ldio->sge_count = megasas_make_sgl64(instance, scp, &ldio->sgl);
	} else
		ldio->sge_count = megasas_make_sgl32(instance, scp, &ldio->sgl);

	if (ldio->sge_count > instance->max_num_sge) {
		printk(KERN_ERR "megasas: build_ld_io: sge_count = %x\n",
			ldio->sge_count);
		return 0;
	}

	ldio->sense_len = SCSI_SENSE_BUFFERSIZE;
	ldio->sense_buf_phys_addr_hi = 0;
	ldio->sense_buf_phys_addr_lo = cmd->sense_phys_addr;

	cmd->frame_count = megasas_get_frame_count(instance,
			ldio->sge_count, IO_FRAME);

	return cmd->frame_count;
}

inline int megasas_is_ldio(struct scsi_cmnd *cmd)
{
	if (!MEGASAS_IS_LOGICAL(cmd))
		return 0;
	switch (cmd->cmnd[0]) {
	case READ_10:
	case WRITE_10:
	case READ_12:
	case WRITE_12:
	case READ_6:
	case WRITE_6:
	case READ_16:
	case WRITE_16:
		return 1;
	default:
		return 0;
	}
}

static inline void
megasas_dump_pending_frames(struct megasas_instance *instance)
{
	struct megasas_cmd *cmd;
	int i,n;
	union megasas_sgl *mfi_sgl;
	struct megasas_io_frame *ldio;
	struct megasas_pthru_frame *pthru;
	u32 sgcount;
	u32 max_cmd = instance->max_fw_cmds;

	printk(KERN_ERR "\nmegasas[%d]: Dumping Frame Phys Address of all pending cmds in FW\n",instance->host->host_no);
	printk(KERN_ERR "megasas[%d]: Total OS Pending cmds : %d\n",instance->host->host_no,atomic_read(&instance->fw_outstanding));
	if (IS_DMA64)
		printk(KERN_ERR "\nmegasas[%d]: 64 bit SGLs were sent to FW\n",instance->host->host_no);
	else
		printk(KERN_ERR "\nmegasas[%d]: 32 bit SGLs were sent to FW\n",instance->host->host_no);

	printk(KERN_ERR "megasas[%d]: Pending OS cmds in FW : \n",instance->host->host_no);
	for (i = 0; i < max_cmd; i++) {
		cmd = instance->cmd_list[i];
		if(!cmd->scmd)
			continue;
		printk(KERN_ERR "megasas[%d]: Frame addr :0x%08lx : ",instance->host->host_no,(unsigned long)cmd->frame_phys_addr);
		if (megasas_is_ldio(cmd->scmd)){
			ldio = (struct megasas_io_frame *)cmd->frame;
			mfi_sgl = &ldio->sgl;
			sgcount = ldio->sge_count;
			printk(KERN_ERR "megasas[%d]: frame count : 0x%x, Cmd : 0x%x, Tgt id : 0x%x, lba lo : 0x%x, lba_hi : 0x%x, sense_buf addr : 0x%x,sge count : 0x%x\n",instance->host->host_no, cmd->frame_count,ldio->cmd,ldio->target_id, ldio->start_lba_lo,ldio->start_lba_hi,ldio->sense_buf_phys_addr_lo,sgcount);
		}
		else {
			pthru = (struct megasas_pthru_frame *) cmd->frame;
			mfi_sgl = &pthru->sgl;
			sgcount = pthru->sge_count;
			printk(KERN_ERR "megasas[%d]: frame count : 0x%x, Cmd : 0x%x, Tgt id : 0x%x, lun : 0x%x, cdb_len : 0x%x, data xfer len : 0x%x, sense_buf addr : 0x%x,sge count : 0x%x\n",instance->host->host_no,cmd->frame_count,pthru->cmd,pthru->target_id,pthru->lun,pthru->cdb_len , pthru->data_xfer_len,pthru->sense_buf_phys_addr_lo,sgcount);
		}
	if(megasas_dbg_lvl & MEGASAS_DBG_LVL){
		for (n = 0; n < sgcount; n++){
			if (IS_DMA64)
				printk(KERN_ERR "megasas: sgl len : 0x%x, sgl addr : 0x%08lx ",mfi_sgl->sge64[n].length , (unsigned long)mfi_sgl->sge64[n].phys_addr) ;
			else
				printk(KERN_ERR "megasas: sgl len : 0x%x, sgl addr : 0x%x ",mfi_sgl->sge32[n].length , mfi_sgl->sge32[n].phys_addr) ;
			}
		}
		printk(KERN_ERR "\n");
	} 
	printk(KERN_ERR "\nmegasas[%d]: Pending Internal cmds in FW : \n",instance->host->host_no);
	for (i = 0; i < max_cmd; i++) {

		cmd = instance->cmd_list[i];

		if(cmd->sync_cmd == 1){
			printk(KERN_ERR "0x%08lx : ", (unsigned long)cmd->frame_phys_addr);
		}
	}
	printk(KERN_ERR "megasas[%d]: Dumping Done.\n\n",instance->host->host_no);
}

u32
megasas_build_and_issue_cmd(struct megasas_instance *instance,
			    struct scsi_cmnd *scmd)
{
	struct megasas_cmd *cmd;
	u32 frame_count;

	cmd = megasas_get_cmd(instance);
	if (!cmd)
		return SCSI_MLQUEUE_HOST_BUSY;

	if (megasas_is_ldio(scmd))
		frame_count = megasas_build_ldio(instance, scmd, cmd);
	else
		frame_count = megasas_build_dcdb(instance, scmd, cmd);

	if (!frame_count)
		goto out_return_cmd;

	cmd->scmd = scmd;
	scmd->SCp.ptr = (char *)cmd;

	atomic_inc(&instance->fw_outstanding);

	instance->instancet->fire_cmd(instance, cmd->frame_phys_addr,
				cmd->frame_count-1, instance->reg_set);

	return 0;
out_return_cmd:
	megasas_return_cmd(instance, cmd);
	return 1;
}


static int
megasas_queue_command_lck(struct scsi_cmnd *scmd, void (*done) (struct scsi_cmnd *))
{
	struct megasas_instance *instance;
	unsigned long flags;

	instance = (struct megasas_instance *)
	    scmd->device->host->hostdata;

	if (instance->issuepend_done == 0)
		return SCSI_MLQUEUE_HOST_BUSY;

	spin_lock_irqsave(&instance->hba_lock, flags);
	if (instance->adprecovery != MEGASAS_HBA_OPERATIONAL) {
		spin_unlock_irqrestore(&instance->hba_lock, flags);
		return SCSI_MLQUEUE_HOST_BUSY;
	}

	spin_unlock_irqrestore(&instance->hba_lock, flags);

	scmd->scsi_done = done;
	scmd->result = 0;

	if (MEGASAS_IS_LOGICAL(scmd) &&
	    (scmd->device->id >= MEGASAS_MAX_LD || scmd->device->lun)) {
		scmd->result = DID_BAD_TARGET << 16;
		goto out_done;
	}

	switch (scmd->cmnd[0]) {
	case SYNCHRONIZE_CACHE:
		scmd->result = DID_OK << 16;
		goto out_done;
	default:
		break;
	}

	if (instance->instancet->build_and_issue_cmd(instance, scmd)) {
		printk(KERN_ERR "megasas: Err returned from build_and_issue_cmd\n");
		return SCSI_MLQUEUE_HOST_BUSY;
	}

	return 0;

 out_done:
	done(scmd);
	return 0;
}

static DEF_SCSI_QCMD(megasas_queue_command)

static struct megasas_instance *megasas_lookup_instance(u16 host_no)
{
	int i;

	for (i = 0; i < megasas_mgmt_info.max_index; i++) {

		if ((megasas_mgmt_info.instance[i]) &&
		    (megasas_mgmt_info.instance[i]->host->host_no == host_no))
			return megasas_mgmt_info.instance[i];
	}

	return NULL;
}

static int megasas_slave_configure(struct scsi_device *sdev)
{
	u16             pd_index = 0;
	struct  megasas_instance *instance ;

	instance = megasas_lookup_instance(sdev->host->host_no);

	if (sdev->channel < MEGASAS_MAX_PD_CHANNELS &&
				sdev->type == TYPE_DISK) {
		pd_index = (sdev->channel * MEGASAS_MAX_DEV_PER_CHANNEL) +
								sdev->id;
		if (instance->pd_list[pd_index].driveState ==
						MR_PD_STATE_SYSTEM) {
			blk_queue_rq_timeout(sdev->request_queue,
				MEGASAS_DEFAULT_CMD_TIMEOUT * HZ);
			return 0;
		}
		return -ENXIO;
	}

	blk_queue_rq_timeout(sdev->request_queue,
		MEGASAS_DEFAULT_CMD_TIMEOUT * HZ);
	return 0;
}

static int megasas_slave_alloc(struct scsi_device *sdev)
{
	u16             pd_index = 0;
	struct megasas_instance *instance ;
	instance = megasas_lookup_instance(sdev->host->host_no);
	if ((sdev->channel < MEGASAS_MAX_PD_CHANNELS) &&
				(sdev->type == TYPE_DISK)) {
		pd_index =
			(sdev->channel * MEGASAS_MAX_DEV_PER_CHANNEL) +
			sdev->id;
		if ((instance->pd_list[pd_index].driveState ==
					MR_PD_STATE_SYSTEM) &&
			(instance->pd_list[pd_index].driveType ==
						TYPE_DISK)) {
			return 0;
		}
		return -ENXIO;
	}
	return 0;
}

void megaraid_sas_kill_hba(struct megasas_instance *instance)
{
	if ((instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
	    (instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0071SKINNY) ||
	    (instance->pdev->device == PCI_DEVICE_ID_LSI_FUSION) ||
	    (instance->pdev->device == PCI_DEVICE_ID_LSI_INVADER)) {
		writel(MFI_STOP_ADP, &instance->reg_set->doorbell);
	} else {
		writel(MFI_STOP_ADP, &instance->reg_set->inbound_doorbell);
	}
}

void
megasas_check_and_restore_queue_depth(struct megasas_instance *instance)
{
	unsigned long flags;
	if (instance->flag & MEGASAS_FW_BUSY
		&& time_after(jiffies, instance->last_time + 5 * HZ)
		&& atomic_read(&instance->fw_outstanding) < 17) {

		spin_lock_irqsave(instance->host->host_lock, flags);
		instance->flag &= ~MEGASAS_FW_BUSY;
		if ((instance->pdev->device ==
			PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
			(instance->pdev->device ==
			PCI_DEVICE_ID_LSI_SAS0071SKINNY)) {
			instance->host->can_queue =
				instance->max_fw_cmds - MEGASAS_SKINNY_INT_CMDS;
		} else
			instance->host->can_queue =
				instance->max_fw_cmds - MEGASAS_INT_CMDS;

		spin_unlock_irqrestore(instance->host->host_lock, flags);
	}
}

static void megasas_complete_cmd_dpc(unsigned long instance_addr)
{
	u32 producer;
	u32 consumer;
	u32 context;
	struct megasas_cmd *cmd;
	struct megasas_instance *instance =
				(struct megasas_instance *)instance_addr;
	unsigned long flags;

	
	if (instance->adprecovery == MEGASAS_HW_CRITICAL_ERROR )
		return;

	spin_lock_irqsave(&instance->completion_lock, flags);

	producer = *instance->producer;
	consumer = *instance->consumer;

	while (consumer != producer) {
		context = instance->reply_queue[consumer];
		if (context >= instance->max_fw_cmds) {
			printk(KERN_ERR "Unexpected context value %x\n",
				context);
			BUG();
		}

		cmd = instance->cmd_list[context];

		megasas_complete_cmd(instance, cmd, DID_OK);

		consumer++;
		if (consumer == (instance->max_fw_cmds + 1)) {
			consumer = 0;
		}
	}

	*instance->consumer = producer;

	spin_unlock_irqrestore(&instance->completion_lock, flags);

	megasas_check_and_restore_queue_depth(instance);
}

static void
megasas_internal_reset_defer_cmds(struct megasas_instance *instance);

static void
process_fw_state_change_wq(struct work_struct *work);

void megasas_do_ocr(struct megasas_instance *instance)
{
	if ((instance->pdev->device == PCI_DEVICE_ID_LSI_SAS1064R) ||
	(instance->pdev->device == PCI_DEVICE_ID_DELL_PERC5) ||
	(instance->pdev->device == PCI_DEVICE_ID_LSI_VERDE_ZCR)) {
		*instance->consumer     = MEGASAS_ADPRESET_INPROG_SIGN;
	}
	instance->instancet->disable_intr(instance->reg_set);
	instance->adprecovery   = MEGASAS_ADPRESET_SM_INFAULT;
	instance->issuepend_done = 0;

	atomic_set(&instance->fw_outstanding, 0);
	megasas_internal_reset_defer_cmds(instance);
	process_fw_state_change_wq(&instance->work_init);
}

static int megasas_wait_for_outstanding(struct megasas_instance *instance)
{
	int i;
	u32 reset_index;
	u32 wait_time = MEGASAS_RESET_WAIT_TIME;
	u8 adprecovery;
	unsigned long flags;
	struct list_head clist_local;
	struct megasas_cmd *reset_cmd;
	u32 fw_state;
	u8 kill_adapter_flag;

	spin_lock_irqsave(&instance->hba_lock, flags);
	adprecovery = instance->adprecovery;
	spin_unlock_irqrestore(&instance->hba_lock, flags);

	if (adprecovery != MEGASAS_HBA_OPERATIONAL) {

		INIT_LIST_HEAD(&clist_local);
		spin_lock_irqsave(&instance->hba_lock, flags);
		list_splice_init(&instance->internal_reset_pending_q,
				&clist_local);
		spin_unlock_irqrestore(&instance->hba_lock, flags);

		printk(KERN_NOTICE "megasas: HBA reset wait ...\n");
		for (i = 0; i < wait_time; i++) {
			msleep(1000);
			spin_lock_irqsave(&instance->hba_lock, flags);
			adprecovery = instance->adprecovery;
			spin_unlock_irqrestore(&instance->hba_lock, flags);
			if (adprecovery == MEGASAS_HBA_OPERATIONAL)
				break;
		}

		if (adprecovery != MEGASAS_HBA_OPERATIONAL) {
			printk(KERN_NOTICE "megasas: reset: Stopping HBA.\n");
			spin_lock_irqsave(&instance->hba_lock, flags);
			instance->adprecovery	= MEGASAS_HW_CRITICAL_ERROR;
			spin_unlock_irqrestore(&instance->hba_lock, flags);
			return FAILED;
		}

		reset_index	= 0;
		while (!list_empty(&clist_local)) {
			reset_cmd	= list_entry((&clist_local)->next,
						struct megasas_cmd, list);
			list_del_init(&reset_cmd->list);
			if (reset_cmd->scmd) {
				reset_cmd->scmd->result = DID_RESET << 16;
				printk(KERN_NOTICE "%d:%p reset [%02x]\n",
					reset_index, reset_cmd,
					reset_cmd->scmd->cmnd[0]);

				reset_cmd->scmd->scsi_done(reset_cmd->scmd);
				megasas_return_cmd(instance, reset_cmd);
			} else if (reset_cmd->sync_cmd) {
				printk(KERN_NOTICE "megasas:%p synch cmds"
						"reset queue\n",
						reset_cmd);

				reset_cmd->cmd_status = ENODATA;
				instance->instancet->fire_cmd(instance,
						reset_cmd->frame_phys_addr,
						0, instance->reg_set);
			} else {
				printk(KERN_NOTICE "megasas: %p unexpected"
					"cmds lst\n",
					reset_cmd);
			}
			reset_index++;
		}

		return SUCCESS;
	}

	for (i = 0; i < wait_time; i++) {

		int outstanding = atomic_read(&instance->fw_outstanding);

		if (!outstanding)
			break;

		if (!(i % MEGASAS_RESET_NOTICE_INTERVAL)) {
			printk(KERN_NOTICE "megasas: [%2d]waiting for %d "
			       "commands to complete\n",i,outstanding);
			megasas_complete_cmd_dpc((unsigned long)instance);
		}

		msleep(1000);
	}

	i = 0;
	kill_adapter_flag = 0;
	do {
		fw_state = instance->instancet->read_fw_status_reg(
					instance->reg_set) & MFI_STATE_MASK;
		if ((fw_state == MFI_STATE_FAULT) &&
			(instance->disableOnlineCtrlReset == 0)) {
			if (i == 3) {
				kill_adapter_flag = 2;
				break;
			}
			megasas_do_ocr(instance);
			kill_adapter_flag = 1;

			
			msleep(1000);
		}
		i++;
	} while (i <= 3);

	if (atomic_read(&instance->fw_outstanding) &&
					!kill_adapter_flag) {
		if (instance->disableOnlineCtrlReset == 0) {

			megasas_do_ocr(instance);

			
			for (i = 0; i < wait_time; i++) {
				int outstanding =
					atomic_read(&instance->fw_outstanding);
				if (!outstanding)
					return SUCCESS;
				msleep(1000);
			}
		}
	}

	if (atomic_read(&instance->fw_outstanding) ||
					(kill_adapter_flag == 2)) {
		printk(KERN_NOTICE "megaraid_sas: pending cmds after reset\n");
		if ((instance->pdev->device ==
			PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
			(instance->pdev->device ==
			PCI_DEVICE_ID_LSI_SAS0071SKINNY)) {
			writel(MFI_STOP_ADP,
				&instance->reg_set->doorbell);
		} else {
			writel(MFI_STOP_ADP,
				&instance->reg_set->inbound_doorbell);
		}
		megasas_dump_pending_frames(instance);
		spin_lock_irqsave(&instance->hba_lock, flags);
		instance->adprecovery	= MEGASAS_HW_CRITICAL_ERROR;
		spin_unlock_irqrestore(&instance->hba_lock, flags);
		return FAILED;
	}

	printk(KERN_NOTICE "megaraid_sas: no pending cmds after reset\n");

	return SUCCESS;
}

static int megasas_generic_reset(struct scsi_cmnd *scmd)
{
	int ret_val;
	struct megasas_instance *instance;

	instance = (struct megasas_instance *)scmd->device->host->hostdata;

	scmd_printk(KERN_NOTICE, scmd, "megasas: RESET cmd=%x retries=%x\n",
		 scmd->cmnd[0], scmd->retries);

	if (instance->adprecovery == MEGASAS_HW_CRITICAL_ERROR) {
		printk(KERN_ERR "megasas: cannot recover from previous reset "
		       "failures\n");
		return FAILED;
	}

	ret_val = megasas_wait_for_outstanding(instance);
	if (ret_val == SUCCESS)
		printk(KERN_NOTICE "megasas: reset successful \n");
	else
		printk(KERN_ERR "megasas: failed to do reset\n");

	return ret_val;
}

static enum
blk_eh_timer_return megasas_reset_timer(struct scsi_cmnd *scmd)
{
	struct megasas_instance *instance;
	unsigned long flags;

	if (time_after(jiffies, scmd->jiffies_at_alloc +
				(MEGASAS_DEFAULT_CMD_TIMEOUT * 2) * HZ)) {
		return BLK_EH_NOT_HANDLED;
	}

	instance = (struct megasas_instance *)scmd->device->host->hostdata;
	if (!(instance->flag & MEGASAS_FW_BUSY)) {
		
		spin_lock_irqsave(instance->host->host_lock, flags);

		instance->host->can_queue = 16;
		instance->last_time = jiffies;
		instance->flag |= MEGASAS_FW_BUSY;

		spin_unlock_irqrestore(instance->host->host_lock, flags);
	}
	return BLK_EH_RESET_TIMER;
}

static int megasas_reset_device(struct scsi_cmnd *scmd)
{
	int ret;

	ret = megasas_generic_reset(scmd);

	return ret;
}

static int megasas_reset_bus_host(struct scsi_cmnd *scmd)
{
	int ret;
	struct megasas_instance *instance;
	instance = (struct megasas_instance *)scmd->device->host->hostdata;

	if ((instance->pdev->device == PCI_DEVICE_ID_LSI_FUSION) ||
	    (instance->pdev->device == PCI_DEVICE_ID_LSI_INVADER))
		ret = megasas_reset_fusion(scmd->device->host);
	else
		ret = megasas_generic_reset(scmd);

	return ret;
}

static int
megasas_bios_param(struct scsi_device *sdev, struct block_device *bdev,
		 sector_t capacity, int geom[])
{
	int heads;
	int sectors;
	sector_t cylinders;
	unsigned long tmp;
	
	heads = 64;
	sectors = 32;

	tmp = heads * sectors;
	cylinders = capacity;

	sector_div(cylinders, tmp);


	if (capacity >= 0x200000) {
		heads = 255;
		sectors = 63;
		tmp = heads*sectors;
		cylinders = capacity;
		sector_div(cylinders, tmp);
	}

	geom[0] = heads;
	geom[1] = sectors;
	geom[2] = cylinders;

	return 0;
}

static void megasas_aen_polling(struct work_struct *work);

static void
megasas_service_aen(struct megasas_instance *instance, struct megasas_cmd *cmd)
{
	unsigned long flags;
	if ((!cmd->abort_aen) && (instance->unload == 0)) {
		spin_lock_irqsave(&poll_aen_lock, flags);
		megasas_poll_wait_aen = 1;
		spin_unlock_irqrestore(&poll_aen_lock, flags);
		wake_up(&megasas_poll_wait);
		kill_fasync(&megasas_async_queue, SIGIO, POLL_IN);
	}
	else
		cmd->abort_aen = 0;

	instance->aen_cmd = NULL;
	megasas_return_cmd(instance, cmd);

	if ((instance->unload == 0) &&
		((instance->issuepend_done == 1))) {
		struct megasas_aen_event *ev;
		ev = kzalloc(sizeof(*ev), GFP_ATOMIC);
		if (!ev) {
			printk(KERN_ERR "megasas_service_aen: out of memory\n");
		} else {
			ev->instance = instance;
			instance->ev = ev;
			INIT_WORK(&ev->hotplug_work, megasas_aen_polling);
			schedule_delayed_work(
				(struct delayed_work *)&ev->hotplug_work, 0);
		}
	}
}

static int megasas_change_queue_depth(struct scsi_device *sdev,
				      int queue_depth, int reason)
{
	if (reason != SCSI_QDEPTH_DEFAULT)
		return -EOPNOTSUPP;

	if (queue_depth > sdev->host->can_queue)
		queue_depth = sdev->host->can_queue;
	scsi_adjust_queue_depth(sdev, scsi_get_tag_type(sdev),
				queue_depth);

	return queue_depth;
}

static struct scsi_host_template megasas_template = {

	.module = THIS_MODULE,
	.name = "LSI SAS based MegaRAID driver",
	.proc_name = "megaraid_sas",
	.slave_configure = megasas_slave_configure,
	.slave_alloc = megasas_slave_alloc,
	.queuecommand = megasas_queue_command,
	.eh_device_reset_handler = megasas_reset_device,
	.eh_bus_reset_handler = megasas_reset_bus_host,
	.eh_host_reset_handler = megasas_reset_bus_host,
	.eh_timed_out = megasas_reset_timer,
	.bios_param = megasas_bios_param,
	.use_clustering = ENABLE_CLUSTERING,
	.change_queue_depth = megasas_change_queue_depth,
};

static void
megasas_complete_int_cmd(struct megasas_instance *instance,
			 struct megasas_cmd *cmd)
{
	cmd->cmd_status = cmd->frame->io.cmd_status;

	if (cmd->cmd_status == ENODATA) {
		cmd->cmd_status = 0;
	}
	wake_up(&instance->int_cmd_wait_q);
}

static void
megasas_complete_abort(struct megasas_instance *instance,
		       struct megasas_cmd *cmd)
{
	if (cmd->sync_cmd) {
		cmd->sync_cmd = 0;
		cmd->cmd_status = 0;
		wake_up(&instance->abort_cmd_wait_q);
	}

	return;
}

void
megasas_complete_cmd(struct megasas_instance *instance, struct megasas_cmd *cmd,
		     u8 alt_status)
{
	int exception = 0;
	struct megasas_header *hdr = &cmd->frame->hdr;
	unsigned long flags;
	struct fusion_context *fusion = instance->ctrl_context;

	
	cmd->retry_for_fw_reset = 0;

	if (cmd->scmd)
		cmd->scmd->SCp.ptr = NULL;

	switch (hdr->cmd) {
	case MFI_CMD_INVALID:
		printk(KERN_WARNING "megaraid_sas: MFI_CMD_INVALID command "
		       "completed.\n");
		printk(KERN_WARNING "megaraid_sas: If you have a controller "
		       "other than PERC5, please upgrade your firmware.\n");
		break;
	case MFI_CMD_PD_SCSI_IO:
	case MFI_CMD_LD_SCSI_IO:

		if (cmd->sync_cmd) {
			cmd->sync_cmd = 0;
			megasas_complete_int_cmd(instance, cmd);
			break;
		}

	case MFI_CMD_LD_READ:
	case MFI_CMD_LD_WRITE:

		if (alt_status) {
			cmd->scmd->result = alt_status << 16;
			exception = 1;
		}

		if (exception) {

			atomic_dec(&instance->fw_outstanding);

			scsi_dma_unmap(cmd->scmd);
			cmd->scmd->scsi_done(cmd->scmd);
			megasas_return_cmd(instance, cmd);

			break;
		}

		switch (hdr->cmd_status) {

		case MFI_STAT_OK:
			cmd->scmd->result = DID_OK << 16;
			break;

		case MFI_STAT_SCSI_IO_FAILED:
		case MFI_STAT_LD_INIT_IN_PROGRESS:
			cmd->scmd->result =
			    (DID_ERROR << 16) | hdr->scsi_status;
			break;

		case MFI_STAT_SCSI_DONE_WITH_ERROR:

			cmd->scmd->result = (DID_OK << 16) | hdr->scsi_status;

			if (hdr->scsi_status == SAM_STAT_CHECK_CONDITION) {
				memset(cmd->scmd->sense_buffer, 0,
				       SCSI_SENSE_BUFFERSIZE);
				memcpy(cmd->scmd->sense_buffer, cmd->sense,
				       hdr->sense_len);

				cmd->scmd->result |= DRIVER_SENSE << 24;
			}

			break;

		case MFI_STAT_LD_OFFLINE:
		case MFI_STAT_DEVICE_NOT_FOUND:
			cmd->scmd->result = DID_BAD_TARGET << 16;
			break;

		default:
			printk(KERN_DEBUG "megasas: MFI FW status %#x\n",
			       hdr->cmd_status);
			cmd->scmd->result = DID_ERROR << 16;
			break;
		}

		atomic_dec(&instance->fw_outstanding);

		scsi_dma_unmap(cmd->scmd);
		cmd->scmd->scsi_done(cmd->scmd);
		megasas_return_cmd(instance, cmd);

		break;

	case MFI_CMD_SMP:
	case MFI_CMD_STP:
	case MFI_CMD_DCMD:
		
		if ((cmd->frame->dcmd.opcode == MR_DCMD_LD_MAP_GET_INFO) &&
		    (cmd->frame->dcmd.mbox.b[1] == 1)) {
			spin_lock_irqsave(instance->host->host_lock, flags);
			if (cmd->frame->hdr.cmd_status != 0) {
				if (cmd->frame->hdr.cmd_status !=
				    MFI_STAT_NOT_FOUND)
					printk(KERN_WARNING "megasas: map sync"
					       "failed, status = 0x%x.\n",
					       cmd->frame->hdr.cmd_status);
				else {
					megasas_return_cmd(instance, cmd);
					spin_unlock_irqrestore(
						instance->host->host_lock,
						flags);
					break;
				}
			} else
				instance->map_id++;
			megasas_return_cmd(instance, cmd);
			if (MR_ValidateMapInfo(
				    fusion->ld_map[(instance->map_id & 1)],
				    fusion->load_balance_info))
				fusion->fast_path_io = 1;
			else
				fusion->fast_path_io = 0;
			megasas_sync_map_info(instance);
			spin_unlock_irqrestore(instance->host->host_lock,
					       flags);
			break;
		}
		if (cmd->frame->dcmd.opcode == MR_DCMD_CTRL_EVENT_GET_INFO ||
			cmd->frame->dcmd.opcode == MR_DCMD_CTRL_EVENT_GET) {
			spin_lock_irqsave(&poll_aen_lock, flags);
			megasas_poll_wait_aen = 0;
			spin_unlock_irqrestore(&poll_aen_lock, flags);
		}

		if (cmd->frame->dcmd.opcode == MR_DCMD_CTRL_EVENT_WAIT)
			megasas_service_aen(instance, cmd);
		else
			megasas_complete_int_cmd(instance, cmd);

		break;

	case MFI_CMD_ABORT:
		megasas_complete_abort(instance, cmd);
		break;

	default:
		printk("megasas: Unknown command completed! [0x%X]\n",
		       hdr->cmd);
		break;
	}
}

static inline void
megasas_issue_pending_cmds_again(struct megasas_instance *instance)
{
	struct megasas_cmd *cmd;
	struct list_head clist_local;
	union megasas_evt_class_locale class_locale;
	unsigned long flags;
	u32 seq_num;

	INIT_LIST_HEAD(&clist_local);
	spin_lock_irqsave(&instance->hba_lock, flags);
	list_splice_init(&instance->internal_reset_pending_q, &clist_local);
	spin_unlock_irqrestore(&instance->hba_lock, flags);

	while (!list_empty(&clist_local)) {
		cmd	= list_entry((&clist_local)->next,
					struct megasas_cmd, list);
		list_del_init(&cmd->list);

		if (cmd->sync_cmd || cmd->scmd) {
			printk(KERN_NOTICE "megaraid_sas: command %p, %p:%d"
				"detected to be pending while HBA reset.\n",
					cmd, cmd->scmd, cmd->sync_cmd);

			cmd->retry_for_fw_reset++;

			if (cmd->retry_for_fw_reset == 3) {
				printk(KERN_NOTICE "megaraid_sas: cmd %p, %p:%d"
					"was tried multiple times during reset."
					"Shutting down the HBA\n",
					cmd, cmd->scmd, cmd->sync_cmd);
				megaraid_sas_kill_hba(instance);

				instance->adprecovery =
						MEGASAS_HW_CRITICAL_ERROR;
				return;
			}
		}

		if (cmd->sync_cmd == 1) {
			if (cmd->scmd) {
				printk(KERN_NOTICE "megaraid_sas: unexpected"
					"cmd attached to internal command!\n");
			}
			printk(KERN_NOTICE "megasas: %p synchronous cmd"
						"on the internal reset queue,"
						"issue it again.\n", cmd);
			cmd->cmd_status = ENODATA;
			instance->instancet->fire_cmd(instance,
							cmd->frame_phys_addr ,
							0, instance->reg_set);
		} else if (cmd->scmd) {
			printk(KERN_NOTICE "megasas: %p scsi cmd [%02x]"
			"detected on the internal queue, issue again.\n",
			cmd, cmd->scmd->cmnd[0]);

			atomic_inc(&instance->fw_outstanding);
			instance->instancet->fire_cmd(instance,
					cmd->frame_phys_addr,
					cmd->frame_count-1, instance->reg_set);
		} else {
			printk(KERN_NOTICE "megasas: %p unexpected cmd on the"
				"internal reset defer list while re-issue!!\n",
				cmd);
		}
	}

	if (instance->aen_cmd) {
		printk(KERN_NOTICE "megaraid_sas: aen_cmd in def process\n");
		megasas_return_cmd(instance, instance->aen_cmd);

		instance->aen_cmd	= NULL;
	}

	seq_num = instance->last_seq_num;
	class_locale.members.reserved = 0;
	class_locale.members.locale = MR_EVT_LOCALE_ALL;
	class_locale.members.class = MR_EVT_CLASS_DEBUG;

	megasas_register_aen(instance, seq_num, class_locale.word);
}

static void
megasas_internal_reset_defer_cmds(struct megasas_instance *instance)
{
	struct megasas_cmd *cmd;
	int i;
	u32 max_cmd = instance->max_fw_cmds;
	u32 defer_index;
	unsigned long flags;

	defer_index     = 0;
	spin_lock_irqsave(&instance->cmd_pool_lock, flags);
	for (i = 0; i < max_cmd; i++) {
		cmd = instance->cmd_list[i];
		if (cmd->sync_cmd == 1 || cmd->scmd) {
			printk(KERN_NOTICE "megasas: moving cmd[%d]:%p:%d:%p"
					"on the defer queue as internal\n",
				defer_index, cmd, cmd->sync_cmd, cmd->scmd);

			if (!list_empty(&cmd->list)) {
				printk(KERN_NOTICE "megaraid_sas: ERROR while"
					" moving this cmd:%p, %d %p, it was"
					"discovered on some list?\n",
					cmd, cmd->sync_cmd, cmd->scmd);

				list_del_init(&cmd->list);
			}
			defer_index++;
			list_add_tail(&cmd->list,
				&instance->internal_reset_pending_q);
		}
	}
	spin_unlock_irqrestore(&instance->cmd_pool_lock, flags);
}


static void
process_fw_state_change_wq(struct work_struct *work)
{
	struct megasas_instance *instance =
		container_of(work, struct megasas_instance, work_init);
	u32 wait;
	unsigned long flags;

	if (instance->adprecovery != MEGASAS_ADPRESET_SM_INFAULT) {
		printk(KERN_NOTICE "megaraid_sas: error, recovery st %x \n",
				instance->adprecovery);
		return ;
	}

	if (instance->adprecovery == MEGASAS_ADPRESET_SM_INFAULT) {
		printk(KERN_NOTICE "megaraid_sas: FW detected to be in fault"
					"state, restarting it...\n");

		instance->instancet->disable_intr(instance->reg_set);
		atomic_set(&instance->fw_outstanding, 0);

		atomic_set(&instance->fw_reset_no_pci_access, 1);
		instance->instancet->adp_reset(instance, instance->reg_set);
		atomic_set(&instance->fw_reset_no_pci_access, 0 );

		printk(KERN_NOTICE "megaraid_sas: FW restarted successfully,"
					"initiating next stage...\n");

		printk(KERN_NOTICE "megaraid_sas: HBA recovery state machine,"
					"state 2 starting...\n");

		
		for (wait = 0; wait < 30; wait++) {
			msleep(1000);
		}

		if (megasas_transition_to_ready(instance, 1)) {
			printk(KERN_NOTICE "megaraid_sas:adapter not ready\n");

			megaraid_sas_kill_hba(instance);
			instance->adprecovery	= MEGASAS_HW_CRITICAL_ERROR;
			return ;
		}

		if ((instance->pdev->device == PCI_DEVICE_ID_LSI_SAS1064R) ||
			(instance->pdev->device == PCI_DEVICE_ID_DELL_PERC5) ||
			(instance->pdev->device == PCI_DEVICE_ID_LSI_VERDE_ZCR)
			) {
			*instance->consumer = *instance->producer;
		} else {
			*instance->consumer = 0;
			*instance->producer = 0;
		}

		megasas_issue_init_mfi(instance);

		spin_lock_irqsave(&instance->hba_lock, flags);
		instance->adprecovery	= MEGASAS_HBA_OPERATIONAL;
		spin_unlock_irqrestore(&instance->hba_lock, flags);
		instance->instancet->enable_intr(instance->reg_set);

		megasas_issue_pending_cmds_again(instance);
		instance->issuepend_done = 1;
	}
	return ;
}

static int
megasas_deplete_reply_queue(struct megasas_instance *instance,
					u8 alt_status)
{
	u32 mfiStatus;
	u32 fw_state;

	if ((mfiStatus = instance->instancet->check_reset(instance,
					instance->reg_set)) == 1) {
		return IRQ_HANDLED;
	}

	if ((mfiStatus = instance->instancet->clear_intr(
						instance->reg_set)
						) == 0) {
		
		if (!instance->msix_vectors)
			return IRQ_NONE;
	}

	instance->mfiStatus = mfiStatus;

	if ((mfiStatus & MFI_INTR_FLAG_FIRMWARE_STATE_CHANGE)) {
		fw_state = instance->instancet->read_fw_status_reg(
				instance->reg_set) & MFI_STATE_MASK;

		if (fw_state != MFI_STATE_FAULT) {
			printk(KERN_NOTICE "megaraid_sas: fw state:%x\n",
						fw_state);
		}

		if ((fw_state == MFI_STATE_FAULT) &&
				(instance->disableOnlineCtrlReset == 0)) {
			printk(KERN_NOTICE "megaraid_sas: wait adp restart\n");

			if ((instance->pdev->device ==
					PCI_DEVICE_ID_LSI_SAS1064R) ||
				(instance->pdev->device ==
					PCI_DEVICE_ID_DELL_PERC5) ||
				(instance->pdev->device ==
					PCI_DEVICE_ID_LSI_VERDE_ZCR)) {

				*instance->consumer =
					MEGASAS_ADPRESET_INPROG_SIGN;
			}


			instance->instancet->disable_intr(instance->reg_set);
			instance->adprecovery	= MEGASAS_ADPRESET_SM_INFAULT;
			instance->issuepend_done = 0;

			atomic_set(&instance->fw_outstanding, 0);
			megasas_internal_reset_defer_cmds(instance);

			printk(KERN_NOTICE "megasas: fwState=%x, stage:%d\n",
					fw_state, instance->adprecovery);

			schedule_work(&instance->work_init);
			return IRQ_HANDLED;

		} else {
			printk(KERN_NOTICE "megasas: fwstate:%x, dis_OCR=%x\n",
				fw_state, instance->disableOnlineCtrlReset);
		}
	}

	tasklet_schedule(&instance->isr_tasklet);
	return IRQ_HANDLED;
}
static irqreturn_t megasas_isr(int irq, void *devp)
{
	struct megasas_irq_context *irq_context = devp;
	struct megasas_instance *instance = irq_context->instance;
	unsigned long flags;
	irqreturn_t	rc;

	if (atomic_read(&instance->fw_reset_no_pci_access))
		return IRQ_HANDLED;

	spin_lock_irqsave(&instance->hba_lock, flags);
	rc =  megasas_deplete_reply_queue(instance, DID_OK);
	spin_unlock_irqrestore(&instance->hba_lock, flags);

	return rc;
}

int
megasas_transition_to_ready(struct megasas_instance *instance, int ocr)
{
	int i;
	u8 max_wait;
	u32 fw_state;
	u32 cur_state;
	u32 abs_state, curr_abs_state;

	fw_state = instance->instancet->read_fw_status_reg(instance->reg_set) & MFI_STATE_MASK;

	if (fw_state != MFI_STATE_READY)
		printk(KERN_INFO "megasas: Waiting for FW to come to ready"
		       " state\n");

	while (fw_state != MFI_STATE_READY) {

		abs_state =
		instance->instancet->read_fw_status_reg(instance->reg_set);

		switch (fw_state) {

		case MFI_STATE_FAULT:
			printk(KERN_DEBUG "megasas: FW in FAULT state!!\n");
			if (ocr) {
				max_wait = MEGASAS_RESET_WAIT_TIME;
				cur_state = MFI_STATE_FAULT;
				break;
			} else
				return -ENODEV;

		case MFI_STATE_WAIT_HANDSHAKE:
			if ((instance->pdev->device ==
				PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
				(instance->pdev->device ==
				 PCI_DEVICE_ID_LSI_SAS0071SKINNY) ||
				(instance->pdev->device ==
				 PCI_DEVICE_ID_LSI_FUSION) ||
				(instance->pdev->device ==
				PCI_DEVICE_ID_LSI_INVADER)) {
				writel(
				  MFI_INIT_CLEAR_HANDSHAKE|MFI_INIT_HOTPLUG,
				  &instance->reg_set->doorbell);
			} else {
				writel(
				    MFI_INIT_CLEAR_HANDSHAKE|MFI_INIT_HOTPLUG,
					&instance->reg_set->inbound_doorbell);
			}

			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_WAIT_HANDSHAKE;
			break;

		case MFI_STATE_BOOT_MESSAGE_PENDING:
			if ((instance->pdev->device ==
			     PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
				(instance->pdev->device ==
				 PCI_DEVICE_ID_LSI_SAS0071SKINNY) ||
			    (instance->pdev->device ==
			     PCI_DEVICE_ID_LSI_FUSION) ||
			    (instance->pdev->device ==
			     PCI_DEVICE_ID_LSI_INVADER)) {
				writel(MFI_INIT_HOTPLUG,
				       &instance->reg_set->doorbell);
			} else
				writel(MFI_INIT_HOTPLUG,
					&instance->reg_set->inbound_doorbell);

			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_BOOT_MESSAGE_PENDING;
			break;

		case MFI_STATE_OPERATIONAL:
			instance->instancet->disable_intr(instance->reg_set);
			if ((instance->pdev->device ==
				PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
				(instance->pdev->device ==
				PCI_DEVICE_ID_LSI_SAS0071SKINNY)  ||
				(instance->pdev->device
					== PCI_DEVICE_ID_LSI_FUSION) ||
				(instance->pdev->device
					== PCI_DEVICE_ID_LSI_INVADER)) {
				writel(MFI_RESET_FLAGS,
					&instance->reg_set->doorbell);
				if ((instance->pdev->device ==
				    PCI_DEVICE_ID_LSI_FUSION) ||
				    (instance->pdev->device ==
				     PCI_DEVICE_ID_LSI_INVADER)) {
					for (i = 0; i < (10 * 1000); i += 20) {
						if (readl(
							    &instance->
							    reg_set->
							    doorbell) & 1)
							msleep(20);
						else
							break;
					}
				}
			} else
				writel(MFI_RESET_FLAGS,
					&instance->reg_set->inbound_doorbell);

			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_OPERATIONAL;
			break;

		case MFI_STATE_UNDEFINED:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_UNDEFINED;
			break;

		case MFI_STATE_BB_INIT:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_BB_INIT;
			break;

		case MFI_STATE_FW_INIT:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_FW_INIT;
			break;

		case MFI_STATE_FW_INIT_2:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_FW_INIT_2;
			break;

		case MFI_STATE_DEVICE_SCAN:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_DEVICE_SCAN;
			break;

		case MFI_STATE_FLUSH_CACHE:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_FLUSH_CACHE;
			break;

		default:
			printk(KERN_DEBUG "megasas: Unknown state 0x%x\n",
			       fw_state);
			return -ENODEV;
		}

		for (i = 0; i < (max_wait * 1000); i++) {
			fw_state = instance->instancet->read_fw_status_reg(instance->reg_set) &
					MFI_STATE_MASK ;
		curr_abs_state =
		instance->instancet->read_fw_status_reg(instance->reg_set);

			if (abs_state == curr_abs_state) {
				msleep(1);
			} else
				break;
		}

		if (curr_abs_state == abs_state) {
			printk(KERN_DEBUG "FW state [%d] hasn't changed "
			       "in %d secs\n", fw_state, max_wait);
			return -ENODEV;
		}
	}
	printk(KERN_INFO "megasas: FW now in Ready state\n");

	return 0;
}

static void megasas_teardown_frame_pool(struct megasas_instance *instance)
{
	int i;
	u32 max_cmd = instance->max_mfi_cmds;
	struct megasas_cmd *cmd;

	if (!instance->frame_dma_pool)
		return;

	for (i = 0; i < max_cmd; i++) {

		cmd = instance->cmd_list[i];

		if (cmd->frame)
			pci_pool_free(instance->frame_dma_pool, cmd->frame,
				      cmd->frame_phys_addr);

		if (cmd->sense)
			pci_pool_free(instance->sense_dma_pool, cmd->sense,
				      cmd->sense_phys_addr);
	}

	pci_pool_destroy(instance->frame_dma_pool);
	pci_pool_destroy(instance->sense_dma_pool);

	instance->frame_dma_pool = NULL;
	instance->sense_dma_pool = NULL;
}

static int megasas_create_frame_pool(struct megasas_instance *instance)
{
	int i;
	u32 max_cmd;
	u32 sge_sz;
	u32 sgl_sz;
	u32 total_sz;
	u32 frame_count;
	struct megasas_cmd *cmd;

	max_cmd = instance->max_mfi_cmds;

	sge_sz = (IS_DMA64) ? sizeof(struct megasas_sge64) :
	    sizeof(struct megasas_sge32);

	if (instance->flag_ieee) {
		sge_sz = sizeof(struct megasas_sge_skinny);
	}

	sgl_sz = sge_sz * instance->max_num_sge;
	frame_count = (sgl_sz + MEGAMFI_FRAME_SIZE - 1) / MEGAMFI_FRAME_SIZE;
	frame_count = 15;

	frame_count++;

	total_sz = MEGAMFI_FRAME_SIZE * frame_count;
	instance->frame_dma_pool = pci_pool_create("megasas frame pool",
						   instance->pdev, total_sz, 64,
						   0);

	if (!instance->frame_dma_pool) {
		printk(KERN_DEBUG "megasas: failed to setup frame pool\n");
		return -ENOMEM;
	}

	instance->sense_dma_pool = pci_pool_create("megasas sense pool",
						   instance->pdev, 128, 4, 0);

	if (!instance->sense_dma_pool) {
		printk(KERN_DEBUG "megasas: failed to setup sense pool\n");

		pci_pool_destroy(instance->frame_dma_pool);
		instance->frame_dma_pool = NULL;

		return -ENOMEM;
	}

	for (i = 0; i < max_cmd; i++) {

		cmd = instance->cmd_list[i];

		cmd->frame = pci_pool_alloc(instance->frame_dma_pool,
					    GFP_KERNEL, &cmd->frame_phys_addr);

		cmd->sense = pci_pool_alloc(instance->sense_dma_pool,
					    GFP_KERNEL, &cmd->sense_phys_addr);

		if (!cmd->frame || !cmd->sense) {
			printk(KERN_DEBUG "megasas: pci_pool_alloc failed \n");
			megasas_teardown_frame_pool(instance);
			return -ENOMEM;
		}

		memset(cmd->frame, 0, total_sz);
		cmd->frame->io.context = cmd->index;
		cmd->frame->io.pad_0 = 0;
		if ((instance->pdev->device != PCI_DEVICE_ID_LSI_FUSION) &&
		    (instance->pdev->device != PCI_DEVICE_ID_LSI_INVADER) &&
		    (reset_devices))
			cmd->frame->hdr.cmd = MFI_CMD_INVALID;
	}

	return 0;
}

void megasas_free_cmds(struct megasas_instance *instance)
{
	int i;
	
	megasas_teardown_frame_pool(instance);

	
	for (i = 0; i < instance->max_mfi_cmds; i++)

		kfree(instance->cmd_list[i]);

	
	kfree(instance->cmd_list);
	instance->cmd_list = NULL;

	INIT_LIST_HEAD(&instance->cmd_pool);
}

int megasas_alloc_cmds(struct megasas_instance *instance)
{
	int i;
	int j;
	u32 max_cmd;
	struct megasas_cmd *cmd;

	max_cmd = instance->max_mfi_cmds;

	instance->cmd_list = kcalloc(max_cmd, sizeof(struct megasas_cmd*), GFP_KERNEL);

	if (!instance->cmd_list) {
		printk(KERN_DEBUG "megasas: out of memory\n");
		return -ENOMEM;
	}

	memset(instance->cmd_list, 0, sizeof(struct megasas_cmd *) *max_cmd);

	for (i = 0; i < max_cmd; i++) {
		instance->cmd_list[i] = kmalloc(sizeof(struct megasas_cmd),
						GFP_KERNEL);

		if (!instance->cmd_list[i]) {

			for (j = 0; j < i; j++)
				kfree(instance->cmd_list[j]);

			kfree(instance->cmd_list);
			instance->cmd_list = NULL;

			return -ENOMEM;
		}
	}

	for (i = 0; i < max_cmd; i++) {
		cmd = instance->cmd_list[i];
		memset(cmd, 0, sizeof(struct megasas_cmd));
		cmd->index = i;
		cmd->scmd = NULL;
		cmd->instance = instance;

		list_add_tail(&cmd->list, &instance->cmd_pool);
	}

	if (megasas_create_frame_pool(instance)) {
		printk(KERN_DEBUG "megasas: Error creating frame DMA pool\n");
		megasas_free_cmds(instance);
	}

	return 0;
}

static int
megasas_get_pd_list(struct megasas_instance *instance)
{
	int ret = 0, pd_index = 0;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct MR_PD_LIST *ci;
	struct MR_PD_ADDRESS *pd_addr;
	dma_addr_t ci_h = 0;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		printk(KERN_DEBUG "megasas (get_pd_list): Failed to get cmd\n");
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;

	ci = pci_alloc_consistent(instance->pdev,
		  MEGASAS_MAX_PD * sizeof(struct MR_PD_LIST), &ci_h);

	if (!ci) {
		printk(KERN_DEBUG "Failed to alloc mem for pd_list\n");
		megasas_return_cmd(instance, cmd);
		return -ENOMEM;
	}

	memset(ci, 0, sizeof(*ci));
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->mbox.b[0] = MR_PD_QUERY_TYPE_EXPOSED_TO_HOST;
	dcmd->mbox.b[1] = 0;
	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0xFF;
	dcmd->sge_count = 1;
	dcmd->flags = MFI_FRAME_DIR_READ;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = MEGASAS_MAX_PD * sizeof(struct MR_PD_LIST);
	dcmd->opcode = MR_DCMD_PD_LIST_QUERY;
	dcmd->sgl.sge32[0].phys_addr = ci_h;
	dcmd->sgl.sge32[0].length = MEGASAS_MAX_PD * sizeof(struct MR_PD_LIST);

	if (!megasas_issue_polled(instance, cmd)) {
		ret = 0;
	} else {
		ret = -1;
	}


	pd_addr = ci->addr;

	if ( ret == 0 &&
		(ci->count <
		  (MEGASAS_MAX_PD_CHANNELS * MEGASAS_MAX_DEV_PER_CHANNEL))) {

		memset(instance->pd_list, 0,
			MEGASAS_MAX_PD * sizeof(struct megasas_pd_list));

		for (pd_index = 0; pd_index < ci->count; pd_index++) {

			instance->pd_list[pd_addr->deviceId].tid	=
							pd_addr->deviceId;
			instance->pd_list[pd_addr->deviceId].driveType	=
							pd_addr->scsiDevType;
			instance->pd_list[pd_addr->deviceId].driveState	=
							MR_PD_STATE_SYSTEM;
			pd_addr++;
		}
	}

	pci_free_consistent(instance->pdev,
				MEGASAS_MAX_PD * sizeof(struct MR_PD_LIST),
				ci, ci_h);
	megasas_return_cmd(instance, cmd);

	return ret;
}

static int
megasas_get_ld_list(struct megasas_instance *instance)
{
	int ret = 0, ld_index = 0, ids = 0;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct MR_LD_LIST *ci;
	dma_addr_t ci_h = 0;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		printk(KERN_DEBUG "megasas_get_ld_list: Failed to get cmd\n");
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;

	ci = pci_alloc_consistent(instance->pdev,
				sizeof(struct MR_LD_LIST),
				&ci_h);

	if (!ci) {
		printk(KERN_DEBUG "Failed to alloc mem in get_ld_list\n");
		megasas_return_cmd(instance, cmd);
		return -ENOMEM;
	}

	memset(ci, 0, sizeof(*ci));
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0xFF;
	dcmd->sge_count = 1;
	dcmd->flags = MFI_FRAME_DIR_READ;
	dcmd->timeout = 0;
	dcmd->data_xfer_len = sizeof(struct MR_LD_LIST);
	dcmd->opcode = MR_DCMD_LD_GET_LIST;
	dcmd->sgl.sge32[0].phys_addr = ci_h;
	dcmd->sgl.sge32[0].length = sizeof(struct MR_LD_LIST);
	dcmd->pad_0  = 0;

	if (!megasas_issue_polled(instance, cmd)) {
		ret = 0;
	} else {
		ret = -1;
	}

	

	if ((ret == 0) && (ci->ldCount <= MAX_LOGICAL_DRIVES)) {
		memset(instance->ld_ids, 0xff, MEGASAS_MAX_LD_IDS);

		for (ld_index = 0; ld_index < ci->ldCount; ld_index++) {
			if (ci->ldList[ld_index].state != 0) {
				ids = ci->ldList[ld_index].ref.targetId;
				instance->ld_ids[ids] =
					ci->ldList[ld_index].ref.targetId;
			}
		}
	}

	pci_free_consistent(instance->pdev,
				sizeof(struct MR_LD_LIST),
				ci,
				ci_h);

	megasas_return_cmd(instance, cmd);
	return ret;
}

static int
megasas_get_ctrl_info(struct megasas_instance *instance,
		      struct megasas_ctrl_info *ctrl_info)
{
	int ret = 0;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct megasas_ctrl_info *ci;
	dma_addr_t ci_h = 0;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		printk(KERN_DEBUG "megasas: Failed to get a free cmd\n");
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;

	ci = pci_alloc_consistent(instance->pdev,
				  sizeof(struct megasas_ctrl_info), &ci_h);

	if (!ci) {
		printk(KERN_DEBUG "Failed to alloc mem for ctrl info\n");
		megasas_return_cmd(instance, cmd);
		return -ENOMEM;
	}

	memset(ci, 0, sizeof(*ci));
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0xFF;
	dcmd->sge_count = 1;
	dcmd->flags = MFI_FRAME_DIR_READ;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = sizeof(struct megasas_ctrl_info);
	dcmd->opcode = MR_DCMD_CTRL_GET_INFO;
	dcmd->sgl.sge32[0].phys_addr = ci_h;
	dcmd->sgl.sge32[0].length = sizeof(struct megasas_ctrl_info);

	if (!megasas_issue_polled(instance, cmd)) {
		ret = 0;
		memcpy(ctrl_info, ci, sizeof(struct megasas_ctrl_info));
	} else {
		ret = -1;
	}

	pci_free_consistent(instance->pdev, sizeof(struct megasas_ctrl_info),
			    ci, ci_h);

	megasas_return_cmd(instance, cmd);
	return ret;
}

static int
megasas_issue_init_mfi(struct megasas_instance *instance)
{
	u32 context;

	struct megasas_cmd *cmd;

	struct megasas_init_frame *init_frame;
	struct megasas_init_queue_info *initq_info;
	dma_addr_t init_frame_h;
	dma_addr_t initq_info_h;

	cmd = megasas_get_cmd(instance);

	init_frame = (struct megasas_init_frame *)cmd->frame;
	initq_info = (struct megasas_init_queue_info *)
		((unsigned long)init_frame + 64);

	init_frame_h = cmd->frame_phys_addr;
	initq_info_h = init_frame_h + 64;

	context = init_frame->context;
	memset(init_frame, 0, MEGAMFI_FRAME_SIZE);
	memset(initq_info, 0, sizeof(struct megasas_init_queue_info));
	init_frame->context = context;

	initq_info->reply_queue_entries = instance->max_fw_cmds + 1;
	initq_info->reply_queue_start_phys_addr_lo = instance->reply_queue_h;

	initq_info->producer_index_phys_addr_lo = instance->producer_h;
	initq_info->consumer_index_phys_addr_lo = instance->consumer_h;

	init_frame->cmd = MFI_CMD_INIT;
	init_frame->cmd_status = 0xFF;
	init_frame->queue_info_new_phys_addr_lo = initq_info_h;

	init_frame->data_xfer_len = sizeof(struct megasas_init_queue_info);

	instance->instancet->disable_intr(instance->reg_set);


	if (megasas_issue_polled(instance, cmd)) {
		printk(KERN_ERR "megasas: Failed to init firmware\n");
		megasas_return_cmd(instance, cmd);
		goto fail_fw_init;
	}

	megasas_return_cmd(instance, cmd);

	return 0;

fail_fw_init:
	return -EINVAL;
}

static u32
megasas_init_adapter_mfi(struct megasas_instance *instance)
{
	struct megasas_register_set __iomem *reg_set;
	u32 context_sz;
	u32 reply_q_sz;

	reg_set = instance->reg_set;

	instance->max_fw_cmds = instance->instancet->read_fw_status_reg(reg_set) & 0x00FFFF;
	instance->max_fw_cmds = instance->max_fw_cmds-1;
	instance->max_mfi_cmds = instance->max_fw_cmds;
	instance->max_num_sge = (instance->instancet->read_fw_status_reg(reg_set) & 0xFF0000) >>
					0x10;
	if (megasas_alloc_cmds(instance))
		goto fail_alloc_cmds;

	context_sz = sizeof(u32);
	reply_q_sz = context_sz * (instance->max_fw_cmds + 1);

	instance->reply_queue = pci_alloc_consistent(instance->pdev,
						     reply_q_sz,
						     &instance->reply_queue_h);

	if (!instance->reply_queue) {
		printk(KERN_DEBUG "megasas: Out of DMA mem for reply queue\n");
		goto fail_reply_queue;
	}

	if (megasas_issue_init_mfi(instance))
		goto fail_fw_init;

	instance->fw_support_ieee = 0;
	instance->fw_support_ieee =
		(instance->instancet->read_fw_status_reg(reg_set) &
		0x04000000);

	printk(KERN_NOTICE "megasas_init_mfi: fw_support_ieee=%d",
			instance->fw_support_ieee);

	if (instance->fw_support_ieee)
		instance->flag_ieee = 1;

	return 0;

fail_fw_init:

	pci_free_consistent(instance->pdev, reply_q_sz,
			    instance->reply_queue, instance->reply_queue_h);
fail_reply_queue:
	megasas_free_cmds(instance);

fail_alloc_cmds:
	return 1;
}


static int megasas_init_fw(struct megasas_instance *instance)
{
	u32 max_sectors_1;
	u32 max_sectors_2;
	u32 tmp_sectors, msix_enable;
	struct megasas_register_set __iomem *reg_set;
	struct megasas_ctrl_info *ctrl_info;
	unsigned long bar_list;
	int i;

	
	bar_list = pci_select_bars(instance->pdev, IORESOURCE_MEM);
	instance->bar = find_first_bit(&bar_list, sizeof(unsigned long));
	instance->base_addr = pci_resource_start(instance->pdev, instance->bar);
	if (pci_request_selected_regions(instance->pdev, instance->bar,
					 "megasas: LSI")) {
		printk(KERN_DEBUG "megasas: IO memory region busy!\n");
		return -EBUSY;
	}

	instance->reg_set = ioremap_nocache(instance->base_addr, 8192);

	if (!instance->reg_set) {
		printk(KERN_DEBUG "megasas: Failed to map IO mem\n");
		goto fail_ioremap;
	}

	reg_set = instance->reg_set;

	switch (instance->pdev->device) {
	case PCI_DEVICE_ID_LSI_FUSION:
	case PCI_DEVICE_ID_LSI_INVADER:
		instance->instancet = &megasas_instance_template_fusion;
		break;
	case PCI_DEVICE_ID_LSI_SAS1078R:
	case PCI_DEVICE_ID_LSI_SAS1078DE:
		instance->instancet = &megasas_instance_template_ppc;
		break;
	case PCI_DEVICE_ID_LSI_SAS1078GEN2:
	case PCI_DEVICE_ID_LSI_SAS0079GEN2:
		instance->instancet = &megasas_instance_template_gen2;
		break;
	case PCI_DEVICE_ID_LSI_SAS0073SKINNY:
	case PCI_DEVICE_ID_LSI_SAS0071SKINNY:
		instance->instancet = &megasas_instance_template_skinny;
		break;
	case PCI_DEVICE_ID_LSI_SAS1064R:
	case PCI_DEVICE_ID_DELL_PERC5:
	default:
		instance->instancet = &megasas_instance_template_xscale;
		break;
	}

	if (megasas_transition_to_ready(instance, 0))
		goto fail_ready_state;

	
	msix_enable = (instance->instancet->read_fw_status_reg(reg_set) &
		       0x4000000) >> 0x1a;
	if (msix_enable && !msix_disable) {
		
		if ((instance->pdev->device == PCI_DEVICE_ID_LSI_FUSION) ||
		    (instance->pdev->device == PCI_DEVICE_ID_LSI_INVADER)) {
			instance->msix_vectors = (readl(&instance->reg_set->
							outbound_scratch_pad_2
							  ) & 0x1F) + 1;
		} else
			instance->msix_vectors = 1;
		
		instance->msix_vectors = min(instance->msix_vectors,
					     (unsigned int)num_online_cpus());
		for (i = 0; i < instance->msix_vectors; i++)
			instance->msixentry[i].entry = i;
		i = pci_enable_msix(instance->pdev, instance->msixentry,
				    instance->msix_vectors);
		if (i >= 0) {
			if (i) {
				if (!pci_enable_msix(instance->pdev,
						     instance->msixentry, i))
					instance->msix_vectors = i;
				else
					instance->msix_vectors = 0;
			}
		} else
			instance->msix_vectors = 0;
	}

	
	if (instance->instancet->init_adapter(instance))
		goto fail_init_adapter;

	printk(KERN_ERR "megasas: INIT adapter done\n");


	memset(instance->pd_list, 0 ,
		(MEGASAS_MAX_PD * sizeof(struct megasas_pd_list)));
	megasas_get_pd_list(instance);

	memset(instance->ld_ids, 0xff, MEGASAS_MAX_LD_IDS);
	megasas_get_ld_list(instance);

	ctrl_info = kmalloc(sizeof(struct megasas_ctrl_info), GFP_KERNEL);

	tmp_sectors = 0;
	if (ctrl_info && !megasas_get_ctrl_info(instance, ctrl_info)) {

		max_sectors_1 = (1 << ctrl_info->stripe_sz_ops.min) *
		    ctrl_info->max_strips_per_io;
		max_sectors_2 = ctrl_info->max_request_size;

		tmp_sectors = min_t(u32, max_sectors_1 , max_sectors_2);
		instance->disableOnlineCtrlReset =
		ctrl_info->properties.OnOffProperties.disableOnlineCtrlReset;
	}

	instance->max_sectors_per_req = instance->max_num_sge *
						PAGE_SIZE / 512;
	if (tmp_sectors && (instance->max_sectors_per_req > tmp_sectors))
		instance->max_sectors_per_req = tmp_sectors;

	kfree(ctrl_info);


	tasklet_init(&instance->isr_tasklet, instance->instancet->tasklet,
		(unsigned long)instance);

	return 0;

fail_init_adapter:
fail_ready_state:
	iounmap(instance->reg_set);

      fail_ioremap:
	pci_release_selected_regions(instance->pdev, instance->bar);

	return -EINVAL;
}

static void megasas_release_mfi(struct megasas_instance *instance)
{
	u32 reply_q_sz = sizeof(u32) *(instance->max_mfi_cmds + 1);

	if (instance->reply_queue)
		pci_free_consistent(instance->pdev, reply_q_sz,
			    instance->reply_queue, instance->reply_queue_h);

	megasas_free_cmds(instance);

	iounmap(instance->reg_set);

	pci_release_selected_regions(instance->pdev, instance->bar);
}

static int
megasas_get_seq_num(struct megasas_instance *instance,
		    struct megasas_evt_log_info *eli)
{
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct megasas_evt_log_info *el_info;
	dma_addr_t el_info_h = 0;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;
	el_info = pci_alloc_consistent(instance->pdev,
				       sizeof(struct megasas_evt_log_info),
				       &el_info_h);

	if (!el_info) {
		megasas_return_cmd(instance, cmd);
		return -ENOMEM;
	}

	memset(el_info, 0, sizeof(*el_info));
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0x0;
	dcmd->sge_count = 1;
	dcmd->flags = MFI_FRAME_DIR_READ;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = sizeof(struct megasas_evt_log_info);
	dcmd->opcode = MR_DCMD_CTRL_EVENT_GET_INFO;
	dcmd->sgl.sge32[0].phys_addr = el_info_h;
	dcmd->sgl.sge32[0].length = sizeof(struct megasas_evt_log_info);

	megasas_issue_blocked_cmd(instance, cmd);

	memcpy(eli, el_info, sizeof(struct megasas_evt_log_info));

	pci_free_consistent(instance->pdev, sizeof(struct megasas_evt_log_info),
			    el_info, el_info_h);

	megasas_return_cmd(instance, cmd);

	return 0;
}

static int
megasas_register_aen(struct megasas_instance *instance, u32 seq_num,
		     u32 class_locale_word)
{
	int ret_val;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	union megasas_evt_class_locale curr_aen;
	union megasas_evt_class_locale prev_aen;


	curr_aen.word = class_locale_word;

	if (instance->aen_cmd) {

		prev_aen.word = instance->aen_cmd->frame->dcmd.mbox.w[1];

		if ((prev_aen.members.class <= curr_aen.members.class) &&
		    !((prev_aen.members.locale & curr_aen.members.locale) ^
		      curr_aen.members.locale)) {
			return 0;
		} else {
			curr_aen.members.locale |= prev_aen.members.locale;

			if (prev_aen.members.class < curr_aen.members.class)
				curr_aen.members.class = prev_aen.members.class;

			instance->aen_cmd->abort_aen = 1;
			ret_val = megasas_issue_blocked_abort_cmd(instance,
								  instance->
								  aen_cmd);

			if (ret_val) {
				printk(KERN_DEBUG "megasas: Failed to abort "
				       "previous AEN command\n");
				return ret_val;
			}
		}
	}

	cmd = megasas_get_cmd(instance);

	if (!cmd)
		return -ENOMEM;

	dcmd = &cmd->frame->dcmd;

	memset(instance->evt_detail, 0, sizeof(struct megasas_evt_detail));

	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0x0;
	dcmd->sge_count = 1;
	dcmd->flags = MFI_FRAME_DIR_READ;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	instance->last_seq_num = seq_num;
	dcmd->data_xfer_len = sizeof(struct megasas_evt_detail);
	dcmd->opcode = MR_DCMD_CTRL_EVENT_WAIT;
	dcmd->mbox.w[0] = seq_num;
	dcmd->mbox.w[1] = curr_aen.word;
	dcmd->sgl.sge32[0].phys_addr = (u32) instance->evt_detail_h;
	dcmd->sgl.sge32[0].length = sizeof(struct megasas_evt_detail);

	if (instance->aen_cmd != NULL) {
		megasas_return_cmd(instance, cmd);
		return 0;
	}

	instance->aen_cmd = cmd;

	instance->instancet->issue_dcmd(instance, cmd);

	return 0;
}

static int megasas_start_aen(struct megasas_instance *instance)
{
	struct megasas_evt_log_info eli;
	union megasas_evt_class_locale class_locale;

	memset(&eli, 0, sizeof(eli));

	if (megasas_get_seq_num(instance, &eli))
		return -1;

	class_locale.members.reserved = 0;
	class_locale.members.locale = MR_EVT_LOCALE_ALL;
	class_locale.members.class = MR_EVT_CLASS_DEBUG;

	return megasas_register_aen(instance, eli.newest_seq_num + 1,
				    class_locale.word);
}

static int megasas_io_attach(struct megasas_instance *instance)
{
	struct Scsi_Host *host = instance->host;

	host->irq = instance->pdev->irq;
	host->unique_id = instance->unique_id;
	if ((instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
		(instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0071SKINNY)) {
		host->can_queue =
			instance->max_fw_cmds - MEGASAS_SKINNY_INT_CMDS;
	} else
		host->can_queue =
			instance->max_fw_cmds - MEGASAS_INT_CMDS;
	host->this_id = instance->init_id;
	host->sg_tablesize = instance->max_num_sge;

	if (instance->fw_support_ieee)
		instance->max_sectors_per_req = MEGASAS_MAX_SECTORS_IEEE;

	if (max_sectors && max_sectors < instance->max_sectors_per_req)
		instance->max_sectors_per_req = max_sectors;
	else {
		if (max_sectors) {
			if (((instance->pdev->device ==
				PCI_DEVICE_ID_LSI_SAS1078GEN2) ||
				(instance->pdev->device ==
				PCI_DEVICE_ID_LSI_SAS0079GEN2)) &&
				(max_sectors <= MEGASAS_MAX_SECTORS)) {
				instance->max_sectors_per_req = max_sectors;
			} else {
			printk(KERN_INFO "megasas: max_sectors should be > 0"
				"and <= %d (or < 1MB for GEN2 controller)\n",
				instance->max_sectors_per_req);
			}
		}
	}

	host->max_sectors = instance->max_sectors_per_req;
	host->cmd_per_lun = MEGASAS_DEFAULT_CMD_PER_LUN;
	host->max_channel = MEGASAS_MAX_CHANNELS - 1;
	host->max_id = MEGASAS_MAX_DEV_PER_CHANNEL;
	host->max_lun = MEGASAS_MAX_LUN;
	host->max_cmd_len = 16;

	
	if ((instance->pdev->device == PCI_DEVICE_ID_LSI_FUSION) ||
	    (instance->pdev->device == PCI_DEVICE_ID_LSI_INVADER)) {
		host->hostt->eh_device_reset_handler = NULL;
		host->hostt->eh_bus_reset_handler = NULL;
	}

	if (scsi_add_host(host, &instance->pdev->dev)) {
		printk(KERN_DEBUG "megasas: scsi_add_host failed\n");
		return -ENODEV;
	}

	scsi_scan_host(host);
	return 0;
}

static int
megasas_set_dma_mask(struct pci_dev *pdev)
{
	if (IS_DMA64) {
		if (pci_set_dma_mask(pdev, DMA_BIT_MASK(64)) != 0) {

			if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32)) != 0)
				goto fail_set_dma_mask;
		}
	} else {
		if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32)) != 0)
			goto fail_set_dma_mask;
	}
	return 0;

fail_set_dma_mask:
	return 1;
}

static int __devinit
megasas_probe_one(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int rval, pos, i, j;
	struct Scsi_Host *host;
	struct megasas_instance *instance;
	u16 control = 0;

	
	if (reset_devices) {
		pos = pci_find_capability(pdev, PCI_CAP_ID_MSIX);
		if (pos) {
			pci_read_config_word(pdev, msi_control_reg(pos),
					     &control);
			if (control & PCI_MSIX_FLAGS_ENABLE) {
				dev_info(&pdev->dev, "resetting MSI-X\n");
				pci_write_config_word(pdev,
						      msi_control_reg(pos),
						      control &
						      ~PCI_MSIX_FLAGS_ENABLE);
			}
		}
	}

	printk(KERN_INFO "megasas: %#4.04x:%#4.04x:%#4.04x:%#4.04x: ",
	       pdev->vendor, pdev->device, pdev->subsystem_vendor,
	       pdev->subsystem_device);

	printk("bus %d:slot %d:func %d\n",
	       pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

	rval = pci_enable_device_mem(pdev);

	if (rval) {
		return rval;
	}

	pci_set_master(pdev);

	if (megasas_set_dma_mask(pdev))
		goto fail_set_dma_mask;

	host = scsi_host_alloc(&megasas_template,
			       sizeof(struct megasas_instance));

	if (!host) {
		printk(KERN_DEBUG "megasas: scsi_host_alloc failed\n");
		goto fail_alloc_instance;
	}

	instance = (struct megasas_instance *)host->hostdata;
	memset(instance, 0, sizeof(*instance));
	atomic_set( &instance->fw_reset_no_pci_access, 0 );
	instance->pdev = pdev;

	switch (instance->pdev->device) {
	case PCI_DEVICE_ID_LSI_FUSION:
	case PCI_DEVICE_ID_LSI_INVADER:
	{
		struct fusion_context *fusion;

		instance->ctrl_context =
			kzalloc(sizeof(struct fusion_context), GFP_KERNEL);
		if (!instance->ctrl_context) {
			printk(KERN_DEBUG "megasas: Failed to allocate "
			       "memory for Fusion context info\n");
			goto fail_alloc_dma_buf;
		}
		fusion = instance->ctrl_context;
		INIT_LIST_HEAD(&fusion->cmd_pool);
		spin_lock_init(&fusion->cmd_pool_lock);
	}
	break;
	default: 

		instance->producer =
			pci_alloc_consistent(pdev, sizeof(u32),
					     &instance->producer_h);
		instance->consumer =
			pci_alloc_consistent(pdev, sizeof(u32),
					     &instance->consumer_h);

		if (!instance->producer || !instance->consumer) {
			printk(KERN_DEBUG "megasas: Failed to allocate"
			       "memory for producer, consumer\n");
			goto fail_alloc_dma_buf;
		}

		*instance->producer = 0;
		*instance->consumer = 0;
		break;
	}

	megasas_poll_wait_aen = 0;
	instance->flag_ieee = 0;
	instance->ev = NULL;
	instance->issuepend_done = 1;
	instance->adprecovery = MEGASAS_HBA_OPERATIONAL;
	megasas_poll_wait_aen = 0;

	instance->evt_detail = pci_alloc_consistent(pdev,
						    sizeof(struct
							   megasas_evt_detail),
						    &instance->evt_detail_h);

	if (!instance->evt_detail) {
		printk(KERN_DEBUG "megasas: Failed to allocate memory for "
		       "event detail structure\n");
		goto fail_alloc_dma_buf;
	}

	INIT_LIST_HEAD(&instance->cmd_pool);
	INIT_LIST_HEAD(&instance->internal_reset_pending_q);

	atomic_set(&instance->fw_outstanding,0);

	init_waitqueue_head(&instance->int_cmd_wait_q);
	init_waitqueue_head(&instance->abort_cmd_wait_q);

	spin_lock_init(&instance->cmd_pool_lock);
	spin_lock_init(&instance->hba_lock);
	spin_lock_init(&instance->completion_lock);
	spin_lock_init(&poll_aen_lock);

	mutex_init(&instance->aen_mutex);
	mutex_init(&instance->reset_mutex);

	instance->host = host;
	instance->unique_id = pdev->bus->number << 8 | pdev->devfn;
	instance->init_id = MEGASAS_DEFAULT_INIT_ID;

	if ((instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
		(instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0071SKINNY)) {
		instance->flag_ieee = 1;
		sema_init(&instance->ioctl_sem, MEGASAS_SKINNY_INT_CMDS);
	} else
		sema_init(&instance->ioctl_sem, MEGASAS_INT_CMDS);

	megasas_dbg_lvl = 0;
	instance->flag = 0;
	instance->unload = 1;
	instance->last_time = 0;
	instance->disableOnlineCtrlReset = 1;

	if ((instance->pdev->device == PCI_DEVICE_ID_LSI_FUSION) ||
	    (instance->pdev->device == PCI_DEVICE_ID_LSI_INVADER))
		INIT_WORK(&instance->work_init, megasas_fusion_ocr_wq);
	else
		INIT_WORK(&instance->work_init, process_fw_state_change_wq);

	if (megasas_init_fw(instance))
		goto fail_init_mfi;

	if (instance->msix_vectors) {
		for (i = 0 ; i < instance->msix_vectors; i++) {
			instance->irq_context[i].instance = instance;
			instance->irq_context[i].MSIxIndex = i;
			if (request_irq(instance->msixentry[i].vector,
					instance->instancet->service_isr, 0,
					"megasas",
					&instance->irq_context[i])) {
				printk(KERN_DEBUG "megasas: Failed to "
				       "register IRQ for vector %d.\n", i);
				for (j = 0 ; j < i ; j++)
					free_irq(
						instance->msixentry[j].vector,
						&instance->irq_context[j]);
				goto fail_irq;
			}
		}
	} else {
		instance->irq_context[0].instance = instance;
		instance->irq_context[0].MSIxIndex = 0;
		if (request_irq(pdev->irq, instance->instancet->service_isr,
				IRQF_SHARED, "megasas",
				&instance->irq_context[0])) {
			printk(KERN_DEBUG "megasas: Failed to register IRQ\n");
			goto fail_irq;
		}
	}

	instance->instancet->enable_intr(instance->reg_set);

	pci_set_drvdata(pdev, instance);

	megasas_mgmt_info.count++;
	megasas_mgmt_info.instance[megasas_mgmt_info.max_index] = instance;
	megasas_mgmt_info.max_index++;

	if (megasas_io_attach(instance))
		goto fail_io_attach;

	instance->unload = 0;

	if (megasas_start_aen(instance)) {
		printk(KERN_DEBUG "megasas: start aen failed\n");
		goto fail_start_aen;
	}

	return 0;

      fail_start_aen:
      fail_io_attach:
	megasas_mgmt_info.count--;
	megasas_mgmt_info.instance[megasas_mgmt_info.max_index] = NULL;
	megasas_mgmt_info.max_index--;

	pci_set_drvdata(pdev, NULL);
	instance->instancet->disable_intr(instance->reg_set);
	if (instance->msix_vectors)
		for (i = 0 ; i < instance->msix_vectors; i++)
			free_irq(instance->msixentry[i].vector,
				 &instance->irq_context[i]);
	else
		free_irq(instance->pdev->irq, &instance->irq_context[0]);
fail_irq:
	if ((instance->pdev->device == PCI_DEVICE_ID_LSI_FUSION) ||
	    (instance->pdev->device == PCI_DEVICE_ID_LSI_INVADER))
		megasas_release_fusion(instance);
	else
		megasas_release_mfi(instance);
      fail_init_mfi:
	if (instance->msix_vectors)
		pci_disable_msix(instance->pdev);
      fail_alloc_dma_buf:
	if (instance->evt_detail)
		pci_free_consistent(pdev, sizeof(struct megasas_evt_detail),
				    instance->evt_detail,
				    instance->evt_detail_h);

	if (instance->producer)
		pci_free_consistent(pdev, sizeof(u32), instance->producer,
				    instance->producer_h);
	if (instance->consumer)
		pci_free_consistent(pdev, sizeof(u32), instance->consumer,
				    instance->consumer_h);
	scsi_host_put(host);

      fail_alloc_instance:
      fail_set_dma_mask:
	pci_disable_device(pdev);

	return -ENODEV;
}

static void megasas_flush_cache(struct megasas_instance *instance)
{
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;

	if (instance->adprecovery == MEGASAS_HW_CRITICAL_ERROR)
		return;

	cmd = megasas_get_cmd(instance);

	if (!cmd)
		return;

	dcmd = &cmd->frame->dcmd;

	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0x0;
	dcmd->sge_count = 0;
	dcmd->flags = MFI_FRAME_DIR_NONE;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = 0;
	dcmd->opcode = MR_DCMD_CTRL_CACHE_FLUSH;
	dcmd->mbox.b[0] = MR_FLUSH_CTRL_CACHE | MR_FLUSH_DISK_CACHE;

	megasas_issue_blocked_cmd(instance, cmd);

	megasas_return_cmd(instance, cmd);

	return;
}

static void megasas_shutdown_controller(struct megasas_instance *instance,
					u32 opcode)
{
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;

	if (instance->adprecovery == MEGASAS_HW_CRITICAL_ERROR)
		return;

	cmd = megasas_get_cmd(instance);

	if (!cmd)
		return;

	if (instance->aen_cmd)
		megasas_issue_blocked_abort_cmd(instance, instance->aen_cmd);
	if (instance->map_update_cmd)
		megasas_issue_blocked_abort_cmd(instance,
						instance->map_update_cmd);
	dcmd = &cmd->frame->dcmd;

	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0x0;
	dcmd->sge_count = 0;
	dcmd->flags = MFI_FRAME_DIR_NONE;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = 0;
	dcmd->opcode = opcode;

	megasas_issue_blocked_cmd(instance, cmd);

	megasas_return_cmd(instance, cmd);

	return;
}

#ifdef CONFIG_PM
static int
megasas_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct Scsi_Host *host;
	struct megasas_instance *instance;
	int i;

	instance = pci_get_drvdata(pdev);
	host = instance->host;
	instance->unload = 1;

	megasas_flush_cache(instance);
	megasas_shutdown_controller(instance, MR_DCMD_HIBERNATE_SHUTDOWN);

	
	if (instance->ev != NULL) {
		struct megasas_aen_event *ev = instance->ev;
		cancel_delayed_work_sync(
			(struct delayed_work *)&ev->hotplug_work);
		instance->ev = NULL;
	}

	tasklet_kill(&instance->isr_tasklet);

	pci_set_drvdata(instance->pdev, instance);
	instance->instancet->disable_intr(instance->reg_set);

	if (instance->msix_vectors)
		for (i = 0 ; i < instance->msix_vectors; i++)
			free_irq(instance->msixentry[i].vector,
				 &instance->irq_context[i]);
	else
		free_irq(instance->pdev->irq, &instance->irq_context[0]);
	if (instance->msix_vectors)
		pci_disable_msix(instance->pdev);

	pci_save_state(pdev);
	pci_disable_device(pdev);

	pci_set_power_state(pdev, pci_choose_state(pdev, state));

	return 0;
}

static int
megasas_resume(struct pci_dev *pdev)
{
	int rval, i, j;
	struct Scsi_Host *host;
	struct megasas_instance *instance;

	instance = pci_get_drvdata(pdev);
	host = instance->host;
	pci_set_power_state(pdev, PCI_D0);
	pci_enable_wake(pdev, PCI_D0, 0);
	pci_restore_state(pdev);

	rval = pci_enable_device_mem(pdev);

	if (rval) {
		printk(KERN_ERR "megasas: Enable device failed\n");
		return rval;
	}

	pci_set_master(pdev);

	if (megasas_set_dma_mask(pdev))
		goto fail_set_dma_mask;


	atomic_set(&instance->fw_outstanding, 0);

	if (megasas_transition_to_ready(instance, 0))
		goto fail_ready_state;

	
	if (instance->msix_vectors)
		pci_enable_msix(instance->pdev, instance->msixentry,
				instance->msix_vectors);

	switch (instance->pdev->device) {
	case PCI_DEVICE_ID_LSI_FUSION:
	case PCI_DEVICE_ID_LSI_INVADER:
	{
		megasas_reset_reply_desc(instance);
		if (megasas_ioc_init_fusion(instance)) {
			megasas_free_cmds(instance);
			megasas_free_cmds_fusion(instance);
			goto fail_init_mfi;
		}
		if (!megasas_get_map_info(instance))
			megasas_sync_map_info(instance);
	}
	break;
	default:
		*instance->producer = 0;
		*instance->consumer = 0;
		if (megasas_issue_init_mfi(instance))
			goto fail_init_mfi;
		break;
	}

	tasklet_init(&instance->isr_tasklet, instance->instancet->tasklet,
		     (unsigned long)instance);

	if (instance->msix_vectors) {
		for (i = 0 ; i < instance->msix_vectors; i++) {
			instance->irq_context[i].instance = instance;
			instance->irq_context[i].MSIxIndex = i;
			if (request_irq(instance->msixentry[i].vector,
					instance->instancet->service_isr, 0,
					"megasas",
					&instance->irq_context[i])) {
				printk(KERN_DEBUG "megasas: Failed to "
				       "register IRQ for vector %d.\n", i);
				for (j = 0 ; j < i ; j++)
					free_irq(
						instance->msixentry[j].vector,
						&instance->irq_context[j]);
				goto fail_irq;
			}
		}
	} else {
		instance->irq_context[0].instance = instance;
		instance->irq_context[0].MSIxIndex = 0;
		if (request_irq(pdev->irq, instance->instancet->service_isr,
				IRQF_SHARED, "megasas",
				&instance->irq_context[0])) {
			printk(KERN_DEBUG "megasas: Failed to register IRQ\n");
			goto fail_irq;
		}
	}

	instance->instancet->enable_intr(instance->reg_set);
	instance->unload = 0;

	if (megasas_start_aen(instance))
		printk(KERN_ERR "megasas: Start AEN failed\n");

	return 0;

fail_irq:
fail_init_mfi:
	if (instance->evt_detail)
		pci_free_consistent(pdev, sizeof(struct megasas_evt_detail),
				instance->evt_detail,
				instance->evt_detail_h);

	if (instance->producer)
		pci_free_consistent(pdev, sizeof(u32), instance->producer,
				instance->producer_h);
	if (instance->consumer)
		pci_free_consistent(pdev, sizeof(u32), instance->consumer,
				instance->consumer_h);
	scsi_host_put(host);

fail_set_dma_mask:
fail_ready_state:

	pci_disable_device(pdev);

	return -ENODEV;
}
#else
#define megasas_suspend	NULL
#define megasas_resume	NULL
#endif

static void __devexit megasas_detach_one(struct pci_dev *pdev)
{
	int i;
	struct Scsi_Host *host;
	struct megasas_instance *instance;
	struct fusion_context *fusion;

	instance = pci_get_drvdata(pdev);
	instance->unload = 1;
	host = instance->host;
	fusion = instance->ctrl_context;

	scsi_remove_host(instance->host);
	megasas_flush_cache(instance);
	megasas_shutdown_controller(instance, MR_DCMD_CTRL_SHUTDOWN);

	
	if (instance->ev != NULL) {
		struct megasas_aen_event *ev = instance->ev;
		cancel_delayed_work_sync(
			(struct delayed_work *)&ev->hotplug_work);
		instance->ev = NULL;
	}

	tasklet_kill(&instance->isr_tasklet);

	for (i = 0; i < megasas_mgmt_info.max_index; i++) {
		if (megasas_mgmt_info.instance[i] == instance) {
			megasas_mgmt_info.count--;
			megasas_mgmt_info.instance[i] = NULL;

			break;
		}
	}

	pci_set_drvdata(instance->pdev, NULL);

	instance->instancet->disable_intr(instance->reg_set);

	if (instance->msix_vectors)
		for (i = 0 ; i < instance->msix_vectors; i++)
			free_irq(instance->msixentry[i].vector,
				 &instance->irq_context[i]);
	else
		free_irq(instance->pdev->irq, &instance->irq_context[0]);
	if (instance->msix_vectors)
		pci_disable_msix(instance->pdev);

	switch (instance->pdev->device) {
	case PCI_DEVICE_ID_LSI_FUSION:
	case PCI_DEVICE_ID_LSI_INVADER:
		megasas_release_fusion(instance);
		for (i = 0; i < 2 ; i++)
			if (fusion->ld_map[i])
				dma_free_coherent(&instance->pdev->dev,
						  fusion->map_sz,
						  fusion->ld_map[i],
						  fusion->
						  ld_map_phys[i]);
		kfree(instance->ctrl_context);
		break;
	default:
		megasas_release_mfi(instance);
		pci_free_consistent(pdev,
				    sizeof(struct megasas_evt_detail),
				    instance->evt_detail,
				    instance->evt_detail_h);
		pci_free_consistent(pdev, sizeof(u32),
				    instance->producer,
				    instance->producer_h);
		pci_free_consistent(pdev, sizeof(u32),
				    instance->consumer,
				    instance->consumer_h);
		break;
	}

	scsi_host_put(host);

	pci_set_drvdata(pdev, NULL);

	pci_disable_device(pdev);

	return;
}

static void megasas_shutdown(struct pci_dev *pdev)
{
	int i;
	struct megasas_instance *instance = pci_get_drvdata(pdev);

	instance->unload = 1;
	megasas_flush_cache(instance);
	megasas_shutdown_controller(instance, MR_DCMD_CTRL_SHUTDOWN);
	instance->instancet->disable_intr(instance->reg_set);
	if (instance->msix_vectors)
		for (i = 0 ; i < instance->msix_vectors; i++)
			free_irq(instance->msixentry[i].vector,
				 &instance->irq_context[i]);
	else
		free_irq(instance->pdev->irq, &instance->irq_context[0]);
	if (instance->msix_vectors)
		pci_disable_msix(instance->pdev);
}

static int megasas_mgmt_open(struct inode *inode, struct file *filep)
{
	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	return 0;
}

static int megasas_mgmt_fasync(int fd, struct file *filep, int mode)
{
	int rc;

	mutex_lock(&megasas_async_queue_mutex);

	rc = fasync_helper(fd, filep, mode, &megasas_async_queue);

	mutex_unlock(&megasas_async_queue_mutex);

	if (rc >= 0) {
		
		filep->private_data = filep;
		return 0;
	}

	printk(KERN_DEBUG "megasas: fasync_helper failed [%d]\n", rc);

	return rc;
}

static unsigned int megasas_mgmt_poll(struct file *file, poll_table *wait)
{
	unsigned int mask;
	unsigned long flags;
	poll_wait(file, &megasas_poll_wait, wait);
	spin_lock_irqsave(&poll_aen_lock, flags);
	if (megasas_poll_wait_aen)
		mask =   (POLLIN | POLLRDNORM);
	else
		mask = 0;
	spin_unlock_irqrestore(&poll_aen_lock, flags);
	return mask;
}

static int
megasas_mgmt_fw_ioctl(struct megasas_instance *instance,
		      struct megasas_iocpacket __user * user_ioc,
		      struct megasas_iocpacket *ioc)
{
	struct megasas_sge32 *kern_sge32;
	struct megasas_cmd *cmd;
	void *kbuff_arr[MAX_IOCTL_SGE];
	dma_addr_t buf_handle = 0;
	int error = 0, i;
	void *sense = NULL;
	dma_addr_t sense_handle;
	unsigned long *sense_ptr;

	memset(kbuff_arr, 0, sizeof(kbuff_arr));

	if (ioc->sge_count > MAX_IOCTL_SGE) {
		printk(KERN_DEBUG "megasas: SGE count [%d] >  max limit [%d]\n",
		       ioc->sge_count, MAX_IOCTL_SGE);
		return -EINVAL;
	}

	cmd = megasas_get_cmd(instance);
	if (!cmd) {
		printk(KERN_DEBUG "megasas: Failed to get a cmd packet\n");
		return -ENOMEM;
	}

	/*
	 * User's IOCTL packet has 2 frames (maximum). Copy those two
	 * frames into our cmd's frames. cmd->frame's context will get
	 * overwritten when we copy from user's frames. So set that value
	 * alone separately
	 */
	memcpy(cmd->frame, ioc->frame.raw, 2 * MEGAMFI_FRAME_SIZE);
	cmd->frame->hdr.context = cmd->index;
	cmd->frame->hdr.pad_0 = 0;
	cmd->frame->hdr.flags &= ~(MFI_FRAME_IEEE | MFI_FRAME_SGL64 |
				   MFI_FRAME_SENSE64);

	kern_sge32 = (struct megasas_sge32 *)
	    ((unsigned long)cmd->frame + ioc->sgl_off);

	for (i = 0; i < ioc->sge_count; i++) {
		if (!ioc->sgl[i].iov_len)
			continue;

		kbuff_arr[i] = dma_alloc_coherent(&instance->pdev->dev,
						    ioc->sgl[i].iov_len,
						    &buf_handle, GFP_KERNEL);
		if (!kbuff_arr[i]) {
			printk(KERN_DEBUG "megasas: Failed to alloc "
			       "kernel SGL buffer for IOCTL \n");
			error = -ENOMEM;
			goto out;
		}

		kern_sge32[i].phys_addr = (u32) buf_handle;
		kern_sge32[i].length = ioc->sgl[i].iov_len;

		if (copy_from_user(kbuff_arr[i], ioc->sgl[i].iov_base,
				   (u32) (ioc->sgl[i].iov_len))) {
			error = -EFAULT;
			goto out;
		}
	}

	if (ioc->sense_len) {
		sense = dma_alloc_coherent(&instance->pdev->dev, ioc->sense_len,
					     &sense_handle, GFP_KERNEL);
		if (!sense) {
			error = -ENOMEM;
			goto out;
		}

		sense_ptr =
		(unsigned long *) ((unsigned long)cmd->frame + ioc->sense_off);
		*sense_ptr = sense_handle;
	}

	cmd->sync_cmd = 1;
	megasas_issue_blocked_cmd(instance, cmd);
	cmd->sync_cmd = 0;

	for (i = 0; i < ioc->sge_count; i++) {
		if (copy_to_user(ioc->sgl[i].iov_base, kbuff_arr[i],
				 ioc->sgl[i].iov_len)) {
			error = -EFAULT;
			goto out;
		}
	}

	if (ioc->sense_len) {
		sense_ptr = (unsigned long *) ((unsigned long)ioc->frame.raw +
				ioc->sense_off);

		if (copy_to_user((void __user *)((unsigned long)(*sense_ptr)),
				 sense, ioc->sense_len)) {
			printk(KERN_ERR "megasas: Failed to copy out to user "
					"sense data\n");
			error = -EFAULT;
			goto out;
		}
	}

	if (copy_to_user(&user_ioc->frame.hdr.cmd_status,
			 &cmd->frame->hdr.cmd_status, sizeof(u8))) {
		printk(KERN_DEBUG "megasas: Error copying out cmd_status\n");
		error = -EFAULT;
	}

      out:
	if (sense) {
		dma_free_coherent(&instance->pdev->dev, ioc->sense_len,
				    sense, sense_handle);
	}

	for (i = 0; i < ioc->sge_count && kbuff_arr[i]; i++) {
		dma_free_coherent(&instance->pdev->dev,
				    kern_sge32[i].length,
				    kbuff_arr[i], kern_sge32[i].phys_addr);
	}

	megasas_return_cmd(instance, cmd);
	return error;
}

static int megasas_mgmt_ioctl_fw(struct file *file, unsigned long arg)
{
	struct megasas_iocpacket __user *user_ioc =
	    (struct megasas_iocpacket __user *)arg;
	struct megasas_iocpacket *ioc;
	struct megasas_instance *instance;
	int error;
	int i;
	unsigned long flags;
	u32 wait_time = MEGASAS_RESET_WAIT_TIME;

	ioc = kmalloc(sizeof(*ioc), GFP_KERNEL);
	if (!ioc)
		return -ENOMEM;

	if (copy_from_user(ioc, user_ioc, sizeof(*ioc))) {
		error = -EFAULT;
		goto out_kfree_ioc;
	}

	instance = megasas_lookup_instance(ioc->host_no);
	if (!instance) {
		error = -ENODEV;
		goto out_kfree_ioc;
	}

	if (instance->adprecovery == MEGASAS_HW_CRITICAL_ERROR) {
		printk(KERN_ERR "Controller in crit error\n");
		error = -ENODEV;
		goto out_kfree_ioc;
	}

	if (instance->unload == 1) {
		error = -ENODEV;
		goto out_kfree_ioc;
	}

	if (down_interruptible(&instance->ioctl_sem)) {
		error = -ERESTARTSYS;
		goto out_kfree_ioc;
	}

	for (i = 0; i < wait_time; i++) {

		spin_lock_irqsave(&instance->hba_lock, flags);
		if (instance->adprecovery == MEGASAS_HBA_OPERATIONAL) {
			spin_unlock_irqrestore(&instance->hba_lock, flags);
			break;
		}
		spin_unlock_irqrestore(&instance->hba_lock, flags);

		if (!(i % MEGASAS_RESET_NOTICE_INTERVAL)) {
			printk(KERN_NOTICE "megasas: waiting"
				"for controller reset to finish\n");
		}

		msleep(1000);
	}

	spin_lock_irqsave(&instance->hba_lock, flags);
	if (instance->adprecovery != MEGASAS_HBA_OPERATIONAL) {
		spin_unlock_irqrestore(&instance->hba_lock, flags);

		printk(KERN_ERR "megaraid_sas: timed out while"
			"waiting for HBA to recover\n");
		error = -ENODEV;
		goto out_kfree_ioc;
	}
	spin_unlock_irqrestore(&instance->hba_lock, flags);

	error = megasas_mgmt_fw_ioctl(instance, user_ioc, ioc);
	up(&instance->ioctl_sem);

      out_kfree_ioc:
	kfree(ioc);
	return error;
}

static int megasas_mgmt_ioctl_aen(struct file *file, unsigned long arg)
{
	struct megasas_instance *instance;
	struct megasas_aen aen;
	int error;
	int i;
	unsigned long flags;
	u32 wait_time = MEGASAS_RESET_WAIT_TIME;

	if (file->private_data != file) {
		printk(KERN_DEBUG "megasas: fasync_helper was not "
		       "called first\n");
		return -EINVAL;
	}

	if (copy_from_user(&aen, (void __user *)arg, sizeof(aen)))
		return -EFAULT;

	instance = megasas_lookup_instance(aen.host_no);

	if (!instance)
		return -ENODEV;

	if (instance->adprecovery == MEGASAS_HW_CRITICAL_ERROR) {
		return -ENODEV;
	}

	if (instance->unload == 1) {
		return -ENODEV;
	}

	for (i = 0; i < wait_time; i++) {

		spin_lock_irqsave(&instance->hba_lock, flags);
		if (instance->adprecovery == MEGASAS_HBA_OPERATIONAL) {
			spin_unlock_irqrestore(&instance->hba_lock,
						flags);
			break;
		}

		spin_unlock_irqrestore(&instance->hba_lock, flags);

		if (!(i % MEGASAS_RESET_NOTICE_INTERVAL)) {
			printk(KERN_NOTICE "megasas: waiting for"
				"controller reset to finish\n");
		}

		msleep(1000);
	}

	spin_lock_irqsave(&instance->hba_lock, flags);
	if (instance->adprecovery != MEGASAS_HBA_OPERATIONAL) {
		spin_unlock_irqrestore(&instance->hba_lock, flags);
		printk(KERN_ERR "megaraid_sas: timed out while waiting"
				"for HBA to recover.\n");
		return -ENODEV;
	}
	spin_unlock_irqrestore(&instance->hba_lock, flags);

	mutex_lock(&instance->aen_mutex);
	error = megasas_register_aen(instance, aen.seq_num,
				     aen.class_locale_word);
	mutex_unlock(&instance->aen_mutex);
	return error;
}

static long
megasas_mgmt_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case MEGASAS_IOC_FIRMWARE:
		return megasas_mgmt_ioctl_fw(file, arg);

	case MEGASAS_IOC_GET_AEN:
		return megasas_mgmt_ioctl_aen(file, arg);
	}

	return -ENOTTY;
}

#ifdef CONFIG_COMPAT
static int megasas_mgmt_compat_ioctl_fw(struct file *file, unsigned long arg)
{
	struct compat_megasas_iocpacket __user *cioc =
	    (struct compat_megasas_iocpacket __user *)arg;
	struct megasas_iocpacket __user *ioc =
	    compat_alloc_user_space(sizeof(struct megasas_iocpacket));
	int i;
	int error = 0;
	compat_uptr_t ptr;

	if (clear_user(ioc, sizeof(*ioc)))
		return -EFAULT;

	if (copy_in_user(&ioc->host_no, &cioc->host_no, sizeof(u16)) ||
	    copy_in_user(&ioc->sgl_off, &cioc->sgl_off, sizeof(u32)) ||
	    copy_in_user(&ioc->sense_off, &cioc->sense_off, sizeof(u32)) ||
	    copy_in_user(&ioc->sense_len, &cioc->sense_len, sizeof(u32)) ||
	    copy_in_user(ioc->frame.raw, cioc->frame.raw, 128) ||
	    copy_in_user(&ioc->sge_count, &cioc->sge_count, sizeof(u32)))
		return -EFAULT;

	if (ioc->sense_len) {
		void __user **sense_ioc_ptr =
			(void __user **)(ioc->frame.raw + ioc->sense_off);
		compat_uptr_t *sense_cioc_ptr =
			(compat_uptr_t *)(cioc->frame.raw + cioc->sense_off);
		if (get_user(ptr, sense_cioc_ptr) ||
		    put_user(compat_ptr(ptr), sense_ioc_ptr))
			return -EFAULT;
	}

	for (i = 0; i < MAX_IOCTL_SGE; i++) {
		if (get_user(ptr, &cioc->sgl[i].iov_base) ||
		    put_user(compat_ptr(ptr), &ioc->sgl[i].iov_base) ||
		    copy_in_user(&ioc->sgl[i].iov_len,
				 &cioc->sgl[i].iov_len, sizeof(compat_size_t)))
			return -EFAULT;
	}

	error = megasas_mgmt_ioctl_fw(file, (unsigned long)ioc);

	if (copy_in_user(&cioc->frame.hdr.cmd_status,
			 &ioc->frame.hdr.cmd_status, sizeof(u8))) {
		printk(KERN_DEBUG "megasas: error copy_in_user cmd_status\n");
		return -EFAULT;
	}
	return error;
}

static long
megasas_mgmt_compat_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	switch (cmd) {
	case MEGASAS_IOC_FIRMWARE32:
		return megasas_mgmt_compat_ioctl_fw(file, arg);
	case MEGASAS_IOC_GET_AEN:
		return megasas_mgmt_ioctl_aen(file, arg);
	}

	return -ENOTTY;
}
#endif

static const struct file_operations megasas_mgmt_fops = {
	.owner = THIS_MODULE,
	.open = megasas_mgmt_open,
	.fasync = megasas_mgmt_fasync,
	.unlocked_ioctl = megasas_mgmt_ioctl,
	.poll = megasas_mgmt_poll,
#ifdef CONFIG_COMPAT
	.compat_ioctl = megasas_mgmt_compat_ioctl,
#endif
	.llseek = noop_llseek,
};

static struct pci_driver megasas_pci_driver = {

	.name = "megaraid_sas",
	.id_table = megasas_pci_table,
	.probe = megasas_probe_one,
	.remove = __devexit_p(megasas_detach_one),
	.suspend = megasas_suspend,
	.resume = megasas_resume,
	.shutdown = megasas_shutdown,
};

static ssize_t megasas_sysfs_show_version(struct device_driver *dd, char *buf)
{
	return snprintf(buf, strlen(MEGASAS_VERSION) + 2, "%s\n",
			MEGASAS_VERSION);
}

static DRIVER_ATTR(version, S_IRUGO, megasas_sysfs_show_version, NULL);

static ssize_t
megasas_sysfs_show_release_date(struct device_driver *dd, char *buf)
{
	return snprintf(buf, strlen(MEGASAS_RELDATE) + 2, "%s\n",
			MEGASAS_RELDATE);
}

static DRIVER_ATTR(release_date, S_IRUGO, megasas_sysfs_show_release_date,
		   NULL);

static ssize_t
megasas_sysfs_show_support_poll_for_event(struct device_driver *dd, char *buf)
{
	return sprintf(buf, "%u\n", support_poll_for_event);
}

static DRIVER_ATTR(support_poll_for_event, S_IRUGO,
			megasas_sysfs_show_support_poll_for_event, NULL);

 static ssize_t
megasas_sysfs_show_support_device_change(struct device_driver *dd, char *buf)
{
	return sprintf(buf, "%u\n", support_device_change);
}

static DRIVER_ATTR(support_device_change, S_IRUGO,
			megasas_sysfs_show_support_device_change, NULL);

static ssize_t
megasas_sysfs_show_dbg_lvl(struct device_driver *dd, char *buf)
{
	return sprintf(buf, "%u\n", megasas_dbg_lvl);
}

static ssize_t
megasas_sysfs_set_dbg_lvl(struct device_driver *dd, const char *buf, size_t count)
{
	int retval = count;
	if(sscanf(buf,"%u",&megasas_dbg_lvl)<1){
		printk(KERN_ERR "megasas: could not set dbg_lvl\n");
		retval = -EINVAL;
	}
	return retval;
}

static DRIVER_ATTR(dbg_lvl, S_IRUGO|S_IWUSR, megasas_sysfs_show_dbg_lvl,
		megasas_sysfs_set_dbg_lvl);

static void
megasas_aen_polling(struct work_struct *work)
{
	struct megasas_aen_event *ev =
		container_of(work, struct megasas_aen_event, hotplug_work);
	struct megasas_instance *instance = ev->instance;
	union megasas_evt_class_locale class_locale;
	struct  Scsi_Host *host;
	struct  scsi_device *sdev1;
	u16     pd_index = 0;
	u16	ld_index = 0;
	int     i, j, doscan = 0;
	u32 seq_num;
	int error;

	if (!instance) {
		printk(KERN_ERR "invalid instance!\n");
		kfree(ev);
		return;
	}
	instance->ev = NULL;
	host = instance->host;
	if (instance->evt_detail) {

		switch (instance->evt_detail->code) {
		case MR_EVT_PD_INSERTED:
			if (megasas_get_pd_list(instance) == 0) {
			for (i = 0; i < MEGASAS_MAX_PD_CHANNELS; i++) {
				for (j = 0;
				j < MEGASAS_MAX_DEV_PER_CHANNEL;
				j++) {

				pd_index =
				(i * MEGASAS_MAX_DEV_PER_CHANNEL) + j;

				sdev1 =
				scsi_device_lookup(host, i, j, 0);

				if (instance->pd_list[pd_index].driveState
						== MR_PD_STATE_SYSTEM) {
						if (!sdev1) {
						scsi_add_device(host, i, j, 0);
						}

					if (sdev1)
						scsi_device_put(sdev1);
					}
				}
			}
			}
			doscan = 0;
			break;

		case MR_EVT_PD_REMOVED:
			if (megasas_get_pd_list(instance) == 0) {
			megasas_get_pd_list(instance);
			for (i = 0; i < MEGASAS_MAX_PD_CHANNELS; i++) {
				for (j = 0;
				j < MEGASAS_MAX_DEV_PER_CHANNEL;
				j++) {

				pd_index =
				(i * MEGASAS_MAX_DEV_PER_CHANNEL) + j;

				sdev1 =
				scsi_device_lookup(host, i, j, 0);

				if (instance->pd_list[pd_index].driveState
					== MR_PD_STATE_SYSTEM) {
					if (sdev1) {
						scsi_device_put(sdev1);
					}
				} else {
					if (sdev1) {
						scsi_remove_device(sdev1);
						scsi_device_put(sdev1);
					}
				}
				}
			}
			}
			doscan = 0;
			break;

		case MR_EVT_LD_OFFLINE:
		case MR_EVT_CFG_CLEARED:
		case MR_EVT_LD_DELETED:
			megasas_get_ld_list(instance);
			for (i = 0; i < MEGASAS_MAX_LD_CHANNELS; i++) {
				for (j = 0;
				j < MEGASAS_MAX_DEV_PER_CHANNEL;
				j++) {

				ld_index =
				(i * MEGASAS_MAX_DEV_PER_CHANNEL) + j;

				sdev1 = scsi_device_lookup(host,
					i + MEGASAS_MAX_LD_CHANNELS,
					j,
					0);

				if (instance->ld_ids[ld_index] != 0xff) {
					if (sdev1) {
						scsi_device_put(sdev1);
					}
				} else {
					if (sdev1) {
						scsi_remove_device(sdev1);
						scsi_device_put(sdev1);
					}
				}
				}
			}
			doscan = 0;
			break;
		case MR_EVT_LD_CREATED:
			megasas_get_ld_list(instance);
			for (i = 0; i < MEGASAS_MAX_LD_CHANNELS; i++) {
				for (j = 0;
					j < MEGASAS_MAX_DEV_PER_CHANNEL;
					j++) {
					ld_index =
					(i * MEGASAS_MAX_DEV_PER_CHANNEL) + j;

					sdev1 = scsi_device_lookup(host,
						i+MEGASAS_MAX_LD_CHANNELS,
						j, 0);

					if (instance->ld_ids[ld_index] !=
								0xff) {
						if (!sdev1) {
							scsi_add_device(host,
								i + 2,
								j, 0);
						}
					}
					if (sdev1) {
						scsi_device_put(sdev1);
					}
				}
			}
			doscan = 0;
			break;
		case MR_EVT_CTRL_HOST_BUS_SCAN_REQUESTED:
		case MR_EVT_FOREIGN_CFG_IMPORTED:
		case MR_EVT_LD_STATE_CHANGE:
			doscan = 1;
			break;
		default:
			doscan = 0;
			break;
		}
	} else {
		printk(KERN_ERR "invalid evt_detail!\n");
		kfree(ev);
		return;
	}

	if (doscan) {
		printk(KERN_INFO "scanning ...\n");
		megasas_get_pd_list(instance);
		for (i = 0; i < MEGASAS_MAX_PD_CHANNELS; i++) {
			for (j = 0; j < MEGASAS_MAX_DEV_PER_CHANNEL; j++) {
				pd_index = i*MEGASAS_MAX_DEV_PER_CHANNEL + j;
				sdev1 = scsi_device_lookup(host, i, j, 0);
				if (instance->pd_list[pd_index].driveState ==
							MR_PD_STATE_SYSTEM) {
					if (!sdev1) {
						scsi_add_device(host, i, j, 0);
					}
					if (sdev1)
						scsi_device_put(sdev1);
				} else {
					if (sdev1) {
						scsi_remove_device(sdev1);
						scsi_device_put(sdev1);
					}
				}
			}
		}

		megasas_get_ld_list(instance);
		for (i = 0; i < MEGASAS_MAX_LD_CHANNELS; i++) {
			for (j = 0; j < MEGASAS_MAX_DEV_PER_CHANNEL; j++) {
				ld_index =
				(i * MEGASAS_MAX_DEV_PER_CHANNEL) + j;

				sdev1 = scsi_device_lookup(host,
					i+MEGASAS_MAX_LD_CHANNELS, j, 0);
				if (instance->ld_ids[ld_index] != 0xff) {
					if (!sdev1) {
						scsi_add_device(host,
								i+2,
								j, 0);
					} else {
						scsi_device_put(sdev1);
					}
				} else {
					if (sdev1) {
						scsi_remove_device(sdev1);
						scsi_device_put(sdev1);
					}
				}
			}
		}
	}

	if ( instance->aen_cmd != NULL ) {
		kfree(ev);
		return ;
	}

	seq_num = instance->evt_detail->seq_num + 1;

	
	class_locale.members.reserved = 0;
	class_locale.members.locale = MR_EVT_LOCALE_ALL;
	class_locale.members.class = MR_EVT_CLASS_DEBUG;
	mutex_lock(&instance->aen_mutex);
	error = megasas_register_aen(instance, seq_num,
					class_locale.word);
	mutex_unlock(&instance->aen_mutex);

	if (error)
		printk(KERN_ERR "register aen failed error %x\n", error);

	kfree(ev);
}

static int __init megasas_init(void)
{
	int rval;

	printk(KERN_INFO "megasas: %s %s\n", MEGASAS_VERSION,
	       MEGASAS_EXT_VERSION);

	support_poll_for_event = 2;
	support_device_change = 1;

	memset(&megasas_mgmt_info, 0, sizeof(megasas_mgmt_info));

	rval = register_chrdev(0, "megaraid_sas_ioctl", &megasas_mgmt_fops);

	if (rval < 0) {
		printk(KERN_DEBUG "megasas: failed to open device node\n");
		return rval;
	}

	megasas_mgmt_majorno = rval;

	rval = pci_register_driver(&megasas_pci_driver);

	if (rval) {
		printk(KERN_DEBUG "megasas: PCI hotplug regisration failed \n");
		goto err_pcidrv;
	}

	rval = driver_create_file(&megasas_pci_driver.driver,
				  &driver_attr_version);
	if (rval)
		goto err_dcf_attr_ver;
	rval = driver_create_file(&megasas_pci_driver.driver,
				  &driver_attr_release_date);
	if (rval)
		goto err_dcf_rel_date;

	rval = driver_create_file(&megasas_pci_driver.driver,
				&driver_attr_support_poll_for_event);
	if (rval)
		goto err_dcf_support_poll_for_event;

	rval = driver_create_file(&megasas_pci_driver.driver,
				  &driver_attr_dbg_lvl);
	if (rval)
		goto err_dcf_dbg_lvl;
	rval = driver_create_file(&megasas_pci_driver.driver,
				&driver_attr_support_device_change);
	if (rval)
		goto err_dcf_support_device_change;

	return rval;

err_dcf_support_device_change:
	driver_remove_file(&megasas_pci_driver.driver,
			   &driver_attr_dbg_lvl);
err_dcf_dbg_lvl:
	driver_remove_file(&megasas_pci_driver.driver,
			&driver_attr_support_poll_for_event);

err_dcf_support_poll_for_event:
	driver_remove_file(&megasas_pci_driver.driver,
			   &driver_attr_release_date);

err_dcf_rel_date:
	driver_remove_file(&megasas_pci_driver.driver, &driver_attr_version);
err_dcf_attr_ver:
	pci_unregister_driver(&megasas_pci_driver);
err_pcidrv:
	unregister_chrdev(megasas_mgmt_majorno, "megaraid_sas_ioctl");
	return rval;
}

static void __exit megasas_exit(void)
{
	driver_remove_file(&megasas_pci_driver.driver,
			   &driver_attr_dbg_lvl);
	driver_remove_file(&megasas_pci_driver.driver,
			&driver_attr_support_poll_for_event);
	driver_remove_file(&megasas_pci_driver.driver,
			&driver_attr_support_device_change);
	driver_remove_file(&megasas_pci_driver.driver,
			   &driver_attr_release_date);
	driver_remove_file(&megasas_pci_driver.driver, &driver_attr_version);

	pci_unregister_driver(&megasas_pci_driver);
	unregister_chrdev(megasas_mgmt_majorno, "megaraid_sas_ioctl");
}

module_init(megasas_init);
module_exit(megasas_exit);
