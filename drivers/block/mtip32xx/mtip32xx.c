/*
 * Driver for the Micron P320 SSD
 *   Copyright (C) 2011 Micron Technology, Inc.
 *
 * Portions of this code were derived from works subjected to the
 * following copyright:
 *    Copyright (C) 2009 Integrated Device Technology, Inc.
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
 */

#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/ata.h>
#include <linux/delay.h>
#include <linux/hdreg.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/smp.h>
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/dma-mapping.h>
#include <linux/idr.h>
#include <linux/kthread.h>
#include <../drivers/ata/ahci.h>
#include <linux/export.h>
#include "mtip32xx.h"

#define HW_CMD_SLOT_SZ		(MTIP_MAX_COMMAND_SLOTS * 32)
#define HW_CMD_TBL_SZ		(AHCI_CMD_TBL_HDR_SZ + (MTIP_MAX_SG * 16))
#define HW_CMD_TBL_AR_SZ	(HW_CMD_TBL_SZ * MTIP_MAX_COMMAND_SLOTS)
#define HW_PORT_PRIV_DMA_SZ \
		(HW_CMD_SLOT_SZ + HW_CMD_TBL_AR_SZ + AHCI_RX_FIS_SZ)

#define HOST_CAP_NZDMA		(1 << 19)
#define HOST_HSORG		0xFC
#define HSORG_DISABLE_SLOTGRP_INTR (1<<24)
#define HSORG_DISABLE_SLOTGRP_PXIS (1<<16)
#define HSORG_HWREV		0xFF00
#define HSORG_STYLE		0x8
#define HSORG_SLOTGROUPS	0x7

#define PORT_COMMAND_ISSUE	0x38
#define PORT_SDBV		0x7C

#define PORT_OFFSET		0x100
#define PORT_MEM_SIZE		0x80

#define PORT_IRQ_ERR \
	(PORT_IRQ_HBUS_ERR | PORT_IRQ_IF_ERR | PORT_IRQ_CONNECT | \
	 PORT_IRQ_PHYRDY | PORT_IRQ_UNK_FIS | PORT_IRQ_BAD_PMP | \
	 PORT_IRQ_TF_ERR | PORT_IRQ_HBUS_DATA_ERR | PORT_IRQ_IF_NONFATAL | \
	 PORT_IRQ_OVERFLOW)
#define PORT_IRQ_LEGACY \
	(PORT_IRQ_PIOS_FIS | PORT_IRQ_D2H_REG_FIS)
#define PORT_IRQ_HANDLED \
	(PORT_IRQ_SDB_FIS | PORT_IRQ_LEGACY | \
	 PORT_IRQ_TF_ERR | PORT_IRQ_IF_ERR | \
	 PORT_IRQ_CONNECT | PORT_IRQ_PHYRDY)
#define DEF_PORT_IRQ \
	(PORT_IRQ_ERR | PORT_IRQ_LEGACY | PORT_IRQ_SDB_FIS)

#define MTIP_PRODUCT_UNKNOWN	0x00
#define MTIP_PRODUCT_ASICFPGA	0x11

static int instance;

static int mtip_major;

static DEFINE_SPINLOCK(rssd_index_lock);
static DEFINE_IDA(rssd_index_ida);

static int mtip_block_initialize(struct driver_data *dd);

#ifdef CONFIG_COMPAT
struct mtip_compat_ide_task_request_s {
	__u8		io_ports[8];
	__u8		hob_ports[8];
	ide_reg_valid_t	out_flags;
	ide_reg_valid_t	in_flags;
	int		data_phase;
	int		req_cmd;
	compat_ulong_t	out_size;
	compat_ulong_t	in_size;
};
#endif

static bool mtip_check_surprise_removal(struct pci_dev *pdev)
{
	u16 vendor_id = 0;

       
	pci_read_config_word(pdev, 0x00, &vendor_id);
	if (vendor_id == 0xFFFF)
		return true; 

	return false; 
}

static void mtip_command_cleanup(struct driver_data *dd)
{
	int group = 0, commandslot = 0, commandindex = 0;
	struct mtip_cmd *command;
	struct mtip_port *port = dd->port;
	static int in_progress;

	if (in_progress)
		return;

	in_progress = 1;

	for (group = 0; group < 4; group++) {
		for (commandslot = 0; commandslot < 32; commandslot++) {
			if (!(port->allocated[group] & (1 << commandslot)))
				continue;

			commandindex = group << 5 | commandslot;
			command = &port->commands[commandindex];

			if (atomic_read(&command->active)
			    && (command->async_callback)) {
				command->async_callback(command->async_data,
					-ENODEV);
				command->async_callback = NULL;
				command->async_data = NULL;
			}

			dma_unmap_sg(&port->dd->pdev->dev,
				command->sg,
				command->scatter_ents,
				command->direction);
		}
	}

	up(&port->cmd_slot);

	set_bit(MTIP_DDF_CLEANUP_BIT, &dd->dd_flag);
	in_progress = 0;
}

static int get_slot(struct mtip_port *port)
{
	int slot, i;
	unsigned int num_command_slots = port->dd->slot_groups * 32;

	for (i = 0; i < 10; i++) {
		slot = find_next_zero_bit(port->allocated,
					 num_command_slots, 1);
		if ((slot < num_command_slots) &&
		    (!test_and_set_bit(slot, port->allocated)))
			return slot;
	}
	dev_warn(&port->dd->pdev->dev, "Failed to get a tag.\n");

	if (mtip_check_surprise_removal(port->dd->pdev)) {
		
		mtip_command_cleanup(port->dd);
	}
	return -1;
}

static inline void release_slot(struct mtip_port *port, int tag)
{
	smp_mb__before_clear_bit();
	clear_bit(tag, port->allocated);
	smp_mb__after_clear_bit();
}

static int hba_reset_nosleep(struct driver_data *dd)
{
	unsigned long timeout;

	
	mdelay(10);

	
	writel(HOST_RESET, dd->mmio + HOST_CTL);

	
	readl(dd->mmio + HOST_CTL);

	timeout = jiffies + msecs_to_jiffies(1000);
	mdelay(10);
	while ((readl(dd->mmio + HOST_CTL) & HOST_RESET)
		 && time_before(jiffies, timeout))
		mdelay(1);

	if (test_bit(MTIP_DDF_REMOVE_PENDING_BIT, &dd->dd_flag))
		return -1;

	if (readl(dd->mmio + HOST_CTL) & HOST_RESET)
		return -1;

	return 0;
}

static inline void mtip_issue_ncq_command(struct mtip_port *port, int tag)
{
	unsigned long flags = 0;

	atomic_set(&port->commands[tag].active, 1);

	spin_lock_irqsave(&port->cmd_issue_lock, flags);

	writel((1 << MTIP_TAG_BIT(tag)),
			port->s_active[MTIP_TAG_INDEX(tag)]);
	writel((1 << MTIP_TAG_BIT(tag)),
			port->cmd_issue[MTIP_TAG_INDEX(tag)]);

	spin_unlock_irqrestore(&port->cmd_issue_lock, flags);

	
	port->commands[tag].comp_time = jiffies + msecs_to_jiffies(
					MTIP_NCQ_COMMAND_TIMEOUT_MS);
}

static int mtip_enable_fis(struct mtip_port *port, int enable)
{
	u32 tmp;

	
	tmp = readl(port->mmio + PORT_CMD);
	if (enable)
		writel(tmp | PORT_CMD_FIS_RX, port->mmio + PORT_CMD);
	else
		writel(tmp & ~PORT_CMD_FIS_RX, port->mmio + PORT_CMD);

	
	readl(port->mmio + PORT_CMD);

	return (((tmp & PORT_CMD_FIS_RX) == PORT_CMD_FIS_RX));
}

static int mtip_enable_engine(struct mtip_port *port, int enable)
{
	u32 tmp;

	
	tmp = readl(port->mmio + PORT_CMD);
	if (enable)
		writel(tmp | PORT_CMD_START, port->mmio + PORT_CMD);
	else
		writel(tmp & ~PORT_CMD_START, port->mmio + PORT_CMD);

	readl(port->mmio + PORT_CMD);
	return (((tmp & PORT_CMD_START) == PORT_CMD_START));
}

static inline void mtip_start_port(struct mtip_port *port)
{
	
	mtip_enable_fis(port, 1);

	
	mtip_enable_engine(port, 1);
}

static inline void mtip_deinit_port(struct mtip_port *port)
{
	
	writel(0, port->mmio + PORT_IRQ_MASK);

	
	mtip_enable_engine(port, 0);

	
	mtip_enable_fis(port, 0);
}

static void mtip_init_port(struct mtip_port *port)
{
	int i;
	mtip_deinit_port(port);

	
	if (readl(port->dd->mmio + HOST_CAP) & HOST_CAP_64) {
		writel((port->command_list_dma >> 16) >> 16,
			 port->mmio + PORT_LST_ADDR_HI);
		writel((port->rxfis_dma >> 16) >> 16,
			 port->mmio + PORT_FIS_ADDR_HI);
	}

	writel(port->command_list_dma & 0xFFFFFFFF,
			port->mmio + PORT_LST_ADDR);
	writel(port->rxfis_dma & 0xFFFFFFFF, port->mmio + PORT_FIS_ADDR);

	
	writel(readl(port->mmio + PORT_SCR_ERR), port->mmio + PORT_SCR_ERR);

	
	for (i = 0; i < port->dd->slot_groups; i++)
		writel(0xFFFFFFFF, port->completed[i]);

	
	writel(readl(port->dd->mmio + PORT_IRQ_STAT),
					port->dd->mmio + PORT_IRQ_STAT);

	
	writel(readl(port->dd->mmio + HOST_IRQ_STAT),
					port->dd->mmio + HOST_IRQ_STAT);

	
	writel(DEF_PORT_IRQ, port->mmio + PORT_IRQ_MASK);
}

static void mtip_restart_port(struct mtip_port *port)
{
	unsigned long timeout;

	
	mtip_enable_engine(port, 0);

	
	timeout = jiffies + msecs_to_jiffies(500);
	while ((readl(port->mmio + PORT_CMD) & PORT_CMD_LIST_ON)
		 && time_before(jiffies, timeout))
		;

	if (test_bit(MTIP_DDF_REMOVE_PENDING_BIT, &port->dd->dd_flag))
		return;

	if (readl(port->mmio + PORT_CMD) & PORT_CMD_LIST_ON) {
		dev_warn(&port->dd->pdev->dev,
			"PxCMD.CR not clear, escalating reset\n");

		if (hba_reset_nosleep(port->dd))
			dev_err(&port->dd->pdev->dev,
				"HBA reset escalation failed.\n");

		
		mdelay(30);
	}

	dev_warn(&port->dd->pdev->dev, "Issuing COM reset\n");

	
	writel(readl(port->mmio + PORT_SCR_CTL) |
			 1, port->mmio + PORT_SCR_CTL);
	readl(port->mmio + PORT_SCR_CTL);

	
	timeout = jiffies + msecs_to_jiffies(1);
	while (time_before(jiffies, timeout))
		;

	if (test_bit(MTIP_DDF_REMOVE_PENDING_BIT, &port->dd->dd_flag))
		return;

	
	writel(readl(port->mmio + PORT_SCR_CTL) & ~1,
			 port->mmio + PORT_SCR_CTL);
	readl(port->mmio + PORT_SCR_CTL);

	
	timeout = jiffies + msecs_to_jiffies(500);
	while (((readl(port->mmio + PORT_SCR_STAT) & 0x01) == 0)
			 && time_before(jiffies, timeout))
		;

	if (test_bit(MTIP_DDF_REMOVE_PENDING_BIT, &port->dd->dd_flag))
		return;

	if ((readl(port->mmio + PORT_SCR_STAT) & 0x01) == 0)
		dev_warn(&port->dd->pdev->dev,
			"COM reset failed\n");

	mtip_init_port(port);
	mtip_start_port(port);

}

static void print_tags(struct driver_data *dd,
			char *msg,
			unsigned long *tagbits,
			int cnt)
{
	unsigned char tagmap[128];
	int group, tagmap_len = 0;

	memset(tagmap, 0, sizeof(tagmap));
	for (group = SLOTBITS_IN_LONGS; group > 0; group--)
		tagmap_len = sprintf(tagmap + tagmap_len, "%016lX ",
						tagbits[group-1]);
	dev_warn(&dd->pdev->dev,
			"%d command(s) %s: tagmap [%s]", cnt, msg, tagmap);
}

static void mtip_timeout_function(unsigned long int data)
{
	struct mtip_port *port = (struct mtip_port *) data;
	struct host_to_dev_fis *fis;
	struct mtip_cmd *command;
	int tag, cmdto_cnt = 0;
	unsigned int bit, group;
	unsigned int num_command_slots = port->dd->slot_groups * 32;
	unsigned long to, tagaccum[SLOTBITS_IN_LONGS];

	if (unlikely(!port))
		return;

	if (test_bit(MTIP_DDF_RESUME_BIT, &port->dd->dd_flag)) {
		mod_timer(&port->cmd_timer,
			jiffies + msecs_to_jiffies(30000));
		return;
	}
	
	memset(tagaccum, 0, SLOTBITS_IN_LONGS * sizeof(long));

	for (tag = 0; tag < num_command_slots; tag++) {
		if (tag == MTIP_TAG_INTERNAL)
			continue;

		if (atomic_read(&port->commands[tag].active) &&
		   (time_after(jiffies, port->commands[tag].comp_time))) {
			group = tag >> 5;
			bit = tag & 0x1F;

			command = &port->commands[tag];
			fis = (struct host_to_dev_fis *) command->command;

			set_bit(tag, tagaccum);
			cmdto_cnt++;
			if (cmdto_cnt == 1)
				set_bit(MTIP_PF_EH_ACTIVE_BIT, &port->flags);

			writel(1 << bit, port->completed[group]);

			
			if (likely(command->async_callback))
				command->async_callback(command->async_data,
							 -EIO);
			command->async_callback = NULL;
			command->comp_func = NULL;

			
			dma_unmap_sg(&port->dd->pdev->dev,
					command->sg,
					command->scatter_ents,
					command->direction);

			atomic_set(&port->commands[tag].active, 0);
			release_slot(port, tag);

			up(&port->cmd_slot);
		}
	}

	if (cmdto_cnt && !test_bit(MTIP_PF_IC_ACTIVE_BIT, &port->flags)) {
		print_tags(port->dd, "timed out", tagaccum, cmdto_cnt);

		mtip_restart_port(port);
		clear_bit(MTIP_PF_EH_ACTIVE_BIT, &port->flags);
		wake_up_interruptible(&port->svc_wait);
	}

	if (port->ic_pause_timer) {
		to  = port->ic_pause_timer + msecs_to_jiffies(1000);
		if (time_after(jiffies, to)) {
			if (!test_bit(MTIP_PF_IC_ACTIVE_BIT, &port->flags)) {
				port->ic_pause_timer = 0;
				clear_bit(MTIP_PF_SE_ACTIVE_BIT, &port->flags);
				clear_bit(MTIP_PF_DM_ACTIVE_BIT, &port->flags);
				clear_bit(MTIP_PF_IC_ACTIVE_BIT, &port->flags);
				wake_up_interruptible(&port->svc_wait);
			}


		}
	}

	
	mod_timer(&port->cmd_timer,
		jiffies + msecs_to_jiffies(MTIP_TIMEOUT_CHECK_PERIOD));
}

static void mtip_async_complete(struct mtip_port *port,
				int tag,
				void *data,
				int status)
{
	struct mtip_cmd *command;
	struct driver_data *dd = data;
	int cb_status = status ? -EIO : 0;

	if (unlikely(!dd) || unlikely(!port))
		return;

	command = &port->commands[tag];

	if (unlikely(status == PORT_IRQ_TF_ERR)) {
		dev_warn(&port->dd->pdev->dev,
			"Command tag %d failed due to TFE\n", tag);
	}

	
	if (likely(command->async_callback))
		command->async_callback(command->async_data, cb_status);

	command->async_callback = NULL;
	command->comp_func = NULL;

	
	dma_unmap_sg(&dd->pdev->dev,
		command->sg,
		command->scatter_ents,
		command->direction);

	
	atomic_set(&port->commands[tag].active, 0);
	release_slot(port, tag);

	up(&port->cmd_slot);
}

static void mtip_completion(struct mtip_port *port,
			    int tag,
			    void *data,
			    int status)
{
	struct mtip_cmd *command = &port->commands[tag];
	struct completion *waiting = data;
	if (unlikely(status == PORT_IRQ_TF_ERR))
		dev_warn(&port->dd->pdev->dev,
			"Internal command %d completed with TFE\n", tag);

	command->async_callback = NULL;
	command->comp_func = NULL;

	complete(waiting);
}

static void mtip_null_completion(struct mtip_port *port,
			    int tag,
			    void *data,
			    int status)
{
	return;
}

static int mtip_read_log_page(struct mtip_port *port, u8 page, u16 *buffer,
				dma_addr_t buffer_dma, unsigned int sectors);
static int mtip_get_smart_attr(struct mtip_port *port, unsigned int id,
						struct smart_attr *attrib);
static void mtip_handle_tfe(struct driver_data *dd)
{
	int group, tag, bit, reissue, rv;
	struct mtip_port *port;
	struct mtip_cmd  *cmd;
	u32 completed;
	struct host_to_dev_fis *fis;
	unsigned long tagaccum[SLOTBITS_IN_LONGS];
	unsigned int cmd_cnt = 0;
	unsigned char *buf;
	char *fail_reason = NULL;
	int fail_all_ncq_write = 0, fail_all_ncq_cmds = 0;

	dev_warn(&dd->pdev->dev, "Taskfile error\n");

	port = dd->port;

	
	del_timer(&port->cmd_timer);

	
	memset(tagaccum, 0, SLOTBITS_IN_LONGS * sizeof(long));

	
	set_bit(MTIP_PF_EH_ACTIVE_BIT, &port->flags);

	
	for (group = 0; group < dd->slot_groups; group++) {
		completed = readl(port->completed[group]);

		
		writel(completed, port->completed[group]);

		
		for (bit = 0; bit < 32 && completed; bit++) {
			if (!(completed & (1<<bit)))
				continue;
			tag = (group << 5) + bit;

			
			if (tag == MTIP_TAG_INTERNAL)
				continue;

			cmd = &port->commands[tag];
			if (likely(cmd->comp_func)) {
				set_bit(tag, tagaccum);
				cmd_cnt++;
				atomic_set(&cmd->active, 0);
				cmd->comp_func(port,
					 tag,
					 cmd->comp_data,
					 0);
			} else {
				dev_err(&port->dd->pdev->dev,
					"Missing completion func for tag %d",
					tag);
				if (mtip_check_surprise_removal(dd->pdev)) {
					mtip_command_cleanup(dd);
					
					return;
				}
			}
		}
	}

	print_tags(dd, "completed (TFE)", tagaccum, cmd_cnt);

	
	mdelay(20);
	mtip_restart_port(port);

	
	rv = mtip_read_log_page(dd->port, ATA_LOG_SATA_NCQ,
				dd->port->log_buf,
				dd->port->log_buf_dma, 1);
	if (rv) {
		dev_warn(&dd->pdev->dev,
			"Error in READ LOG EXT (10h) command\n");
		
	} else {
		buf = (unsigned char *)dd->port->log_buf;
		if (buf[259] & 0x1) {
			dev_info(&dd->pdev->dev,
				"Write protect bit is set.\n");
			set_bit(MTIP_DDF_WRITE_PROTECT_BIT, &dd->dd_flag);
			fail_all_ncq_write = 1;
			fail_reason = "write protect";
		}
		if (buf[288] == 0xF7) {
			dev_info(&dd->pdev->dev,
				"Exceeded Tmax, drive in thermal shutdown.\n");
			set_bit(MTIP_DDF_OVER_TEMP_BIT, &dd->dd_flag);
			fail_all_ncq_cmds = 1;
			fail_reason = "thermal shutdown";
		}
		if (buf[288] == 0xBF) {
			dev_info(&dd->pdev->dev,
				"Drive indicates rebuild has failed.\n");
			fail_all_ncq_cmds = 1;
			fail_reason = "rebuild failed";
		}
	}

	
	memset(tagaccum, 0, SLOTBITS_IN_LONGS * sizeof(long));

	
	for (group = 0; group < dd->slot_groups; group++) {
		for (bit = 0; bit < 32; bit++) {
			reissue = 1;
			tag = (group << 5) + bit;
			cmd = &port->commands[tag];

			
			if (atomic_read(&cmd->active) == 0)
				continue;

			fis = (struct host_to_dev_fis *)cmd->command;

			
			if (tag == MTIP_TAG_INTERNAL ||
			    fis->command == ATA_CMD_SET_FEATURES)
				reissue = 0;
			else {
				if (fail_all_ncq_cmds ||
					(fail_all_ncq_write &&
					fis->command == ATA_CMD_FPDMA_WRITE)) {
					dev_warn(&dd->pdev->dev,
					"  Fail: %s w/tag %d [%s].\n",
					fis->command == ATA_CMD_FPDMA_WRITE ?
						"write" : "read",
					tag,
					fail_reason != NULL ?
						fail_reason : "unknown");
					atomic_set(&cmd->active, 0);
					if (cmd->comp_func) {
						cmd->comp_func(port, tag,
							cmd->comp_data,
							-ENODATA);
					}
					continue;
				}
			}

			if (reissue && (cmd->retries-- > 0)) {

				set_bit(tag, tagaccum);

				
				mtip_issue_ncq_command(port, tag);

				continue;
			}

			
			dev_warn(&port->dd->pdev->dev,
				"retiring tag %d\n", tag);
			atomic_set(&cmd->active, 0);

			if (cmd->comp_func)
				cmd->comp_func(
					port,
					tag,
					cmd->comp_data,
					PORT_IRQ_TF_ERR);
			else
				dev_warn(&port->dd->pdev->dev,
					"Bad completion for tag %d\n",
					tag);
		}
	}
	print_tags(dd, "reissued (TFE)", tagaccum, cmd_cnt);

	
	clear_bit(MTIP_PF_EH_ACTIVE_BIT, &port->flags);
	wake_up_interruptible(&port->svc_wait);

	mod_timer(&port->cmd_timer,
		 jiffies + msecs_to_jiffies(MTIP_TIMEOUT_CHECK_PERIOD));
}

static inline void mtip_process_sdbf(struct driver_data *dd)
{
	struct mtip_port  *port = dd->port;
	int group, tag, bit;
	u32 completed;
	struct mtip_cmd *command;

	
	for (group = 0; group < dd->slot_groups; group++) {
		completed = readl(port->completed[group]);

		
		writel(completed, port->completed[group]);

		
		for (bit = 0;
		     (bit < 32) && completed;
		     bit++, completed >>= 1) {
			if (completed & 0x01) {
				tag = (group << 5) | bit;

				
				if (unlikely(tag == MTIP_TAG_INTERNAL))
					continue;

				command = &port->commands[tag];
				
				if (likely(command->comp_func)) {
					command->comp_func(
						port,
						tag,
						command->comp_data,
						0);
				} else {
					dev_warn(&dd->pdev->dev,
						"Null completion "
						"for tag %d",
						tag);

					if (mtip_check_surprise_removal(
						dd->pdev)) {
						mtip_command_cleanup(dd);
						return;
					}
				}
			}
		}
	}
}

static inline void mtip_process_legacy(struct driver_data *dd, u32 port_stat)
{
	struct mtip_port *port = dd->port;
	struct mtip_cmd *cmd = &port->commands[MTIP_TAG_INTERNAL];

	if (test_bit(MTIP_PF_IC_ACTIVE_BIT, &port->flags) &&
	    (cmd != NULL) && !(readl(port->cmd_issue[MTIP_TAG_INTERNAL])
		& (1 << MTIP_TAG_INTERNAL))) {
		if (cmd->comp_func) {
			cmd->comp_func(port,
				MTIP_TAG_INTERNAL,
				cmd->comp_data,
				0);
			return;
		}
	}

	return;
}

static inline void mtip_process_errors(struct driver_data *dd, u32 port_stat)
{
	if (likely(port_stat & (PORT_IRQ_TF_ERR | PORT_IRQ_IF_ERR)))
		mtip_handle_tfe(dd);

	if (unlikely(port_stat & PORT_IRQ_CONNECT)) {
		dev_warn(&dd->pdev->dev,
			"Clearing PxSERR.DIAG.x\n");
		writel((1 << 26), dd->port->mmio + PORT_SCR_ERR);
	}

	if (unlikely(port_stat & PORT_IRQ_PHYRDY)) {
		dev_warn(&dd->pdev->dev,
			"Clearing PxSERR.DIAG.n\n");
		writel((1 << 16), dd->port->mmio + PORT_SCR_ERR);
	}

	if (unlikely(port_stat & ~PORT_IRQ_HANDLED)) {
		dev_warn(&dd->pdev->dev,
			"Port stat errors %x unhandled\n",
			(port_stat & ~PORT_IRQ_HANDLED));
	}
}

static inline irqreturn_t mtip_handle_irq(struct driver_data *data)
{
	struct driver_data *dd = (struct driver_data *) data;
	struct mtip_port *port = dd->port;
	u32 hba_stat, port_stat;
	int rv = IRQ_NONE;

	hba_stat = readl(dd->mmio + HOST_IRQ_STAT);
	if (hba_stat) {
		rv = IRQ_HANDLED;

		
		port_stat = readl(port->mmio + PORT_IRQ_STAT);
		writel(port_stat, port->mmio + PORT_IRQ_STAT);

		
		if (likely(port_stat & PORT_IRQ_SDB_FIS))
			mtip_process_sdbf(dd);

		if (unlikely(port_stat & PORT_IRQ_ERR)) {
			if (unlikely(mtip_check_surprise_removal(dd->pdev))) {
				mtip_command_cleanup(dd);
				
				return IRQ_HANDLED;
			}
			if (test_bit(MTIP_DDF_REMOVE_PENDING_BIT,
							&dd->dd_flag))
				return rv;

			mtip_process_errors(dd, port_stat & PORT_IRQ_ERR);
		}

		if (unlikely(port_stat & PORT_IRQ_LEGACY))
			mtip_process_legacy(dd, port_stat & PORT_IRQ_LEGACY);
	}

	
	writel(hba_stat, dd->mmio + HOST_IRQ_STAT);

	return rv;
}

static void mtip_tasklet(unsigned long data)
{
	mtip_handle_irq((struct driver_data *) data);
}

static irqreturn_t mtip_irq_handler(int irq, void *instance)
{
	struct driver_data *dd = instance;
	tasklet_schedule(&dd->tasklet);
	return IRQ_HANDLED;
}

static void mtip_issue_non_ncq_command(struct mtip_port *port, int tag)
{
	atomic_set(&port->commands[tag].active, 1);
	writel(1 << MTIP_TAG_BIT(tag),
		port->cmd_issue[MTIP_TAG_INDEX(tag)]);
}

static bool mtip_pause_ncq(struct mtip_port *port,
				struct host_to_dev_fis *fis)
{
	struct host_to_dev_fis *reply;
	unsigned long task_file_data;

	reply = port->rxfis + RX_FIS_D2H_REG;
	task_file_data = readl(port->mmio+PORT_TFDATA);

	if ((task_file_data & 1) || (fis->command == ATA_CMD_SEC_ERASE_UNIT))
		return false;

	if (fis->command == ATA_CMD_SEC_ERASE_PREP) {
		set_bit(MTIP_PF_SE_ACTIVE_BIT, &port->flags);
		port->ic_pause_timer = jiffies;
		return true;
	} else if ((fis->command == ATA_CMD_DOWNLOAD_MICRO) &&
					(fis->features == 0x03)) {
		set_bit(MTIP_PF_DM_ACTIVE_BIT, &port->flags);
		port->ic_pause_timer = jiffies;
		return true;
	} else if ((fis->command == ATA_CMD_SEC_ERASE_UNIT) ||
		((fis->command == 0xFC) &&
			(fis->features == 0x27 || fis->features == 0x72 ||
			 fis->features == 0x62 || fis->features == 0x26))) {
		
		mtip_restart_port(port);
		return false;
	}

	return false;
}

static int mtip_quiesce_io(struct mtip_port *port, unsigned long timeout)
{
	unsigned long to;
	unsigned int n;
	unsigned int active = 1;

	to = jiffies + msecs_to_jiffies(timeout);
	do {
		if (test_bit(MTIP_PF_SVC_THD_ACTIVE_BIT, &port->flags) &&
			test_bit(MTIP_PF_ISSUE_CMDS_BIT, &port->flags)) {
			msleep(20);
			continue; 
		}
		if (test_bit(MTIP_DDF_REMOVE_PENDING_BIT, &port->dd->dd_flag))
			return -EFAULT;
		active = readl(port->s_active[0]) & 0xFFFFFFFE;
		for (n = 1; n < port->dd->slot_groups; n++)
			active |= readl(port->s_active[n]);

		if (!active)
			break;

		msleep(20);
	} while (time_before(jiffies, to));

	return active ? -EBUSY : 0;
}

static int mtip_exec_internal_command(struct mtip_port *port,
					struct host_to_dev_fis *fis,
					int fis_len,
					dma_addr_t buffer,
					int buf_len,
					u32 opts,
					gfp_t atomic,
					unsigned long timeout)
{
	struct mtip_cmd_sg *command_sg;
	DECLARE_COMPLETION_ONSTACK(wait);
	int rv = 0, ready2go = 1;
	struct mtip_cmd *int_cmd = &port->commands[MTIP_TAG_INTERNAL];
	unsigned long to;

	
	if (buffer & 0x00000007) {
		dev_err(&port->dd->pdev->dev,
			"SG buffer is not 8 byte aligned\n");
		return -EFAULT;
	}

	to = jiffies + msecs_to_jiffies(timeout);
	do {
		ready2go = !test_and_set_bit(MTIP_TAG_INTERNAL,
						port->allocated);
		if (ready2go)
			break;
		mdelay(100);
	} while (time_before(jiffies, to));
	if (!ready2go) {
		dev_warn(&port->dd->pdev->dev,
			"Internal cmd active. new cmd [%02X]\n", fis->command);
		return -EBUSY;
	}
	set_bit(MTIP_PF_IC_ACTIVE_BIT, &port->flags);
	port->ic_pause_timer = 0;

	if (fis->command == ATA_CMD_SEC_ERASE_UNIT)
		clear_bit(MTIP_PF_SE_ACTIVE_BIT, &port->flags);
	else if (fis->command == ATA_CMD_DOWNLOAD_MICRO)
		clear_bit(MTIP_PF_DM_ACTIVE_BIT, &port->flags);

	if (atomic == GFP_KERNEL) {
		if (fis->command != ATA_CMD_STANDBYNOW1) {
			
			if (mtip_quiesce_io(port, 5000) < 0) {
				dev_warn(&port->dd->pdev->dev,
					"Failed to quiesce IO\n");
				release_slot(port, MTIP_TAG_INTERNAL);
				clear_bit(MTIP_PF_IC_ACTIVE_BIT, &port->flags);
				wake_up_interruptible(&port->svc_wait);
				return -EBUSY;
			}
		}

		
		int_cmd->comp_data = &wait;
		int_cmd->comp_func = mtip_completion;

	} else {
		
		int_cmd->comp_data = NULL;
		int_cmd->comp_func = mtip_null_completion;
	}

	
	memcpy(int_cmd->command, fis, fis_len*4);

	
	int_cmd->command_header->opts =
		 __force_bit2int cpu_to_le32(opts | fis_len);
	if (buf_len) {
		command_sg = int_cmd->command + AHCI_CMD_TBL_HDR_SZ;

		command_sg->info =
			__force_bit2int cpu_to_le32((buf_len-1) & 0x3FFFFF);
		command_sg->dba	=
			__force_bit2int cpu_to_le32(buffer & 0xFFFFFFFF);
		command_sg->dba_upper =
			__force_bit2int cpu_to_le32((buffer >> 16) >> 16);

		int_cmd->command_header->opts |=
			__force_bit2int cpu_to_le32((1 << 16));
	}

	
	int_cmd->command_header->byte_count = 0;

	
	mtip_issue_non_ncq_command(port, MTIP_TAG_INTERNAL);

	
	if (atomic == GFP_KERNEL) {
		
		if (wait_for_completion_timeout(
				&wait,
				msecs_to_jiffies(timeout)) == 0) {
			dev_err(&port->dd->pdev->dev,
				"Internal command did not complete [%d] "
				"within timeout of  %lu ms\n",
				atomic, timeout);
			if (mtip_check_surprise_removal(port->dd->pdev) ||
				test_bit(MTIP_DDF_REMOVE_PENDING_BIT,
						&port->dd->dd_flag)) {
				rv = -ENXIO;
				goto exec_ic_exit;
			}
			rv = -EAGAIN;
		}

		if (readl(port->cmd_issue[MTIP_TAG_INTERNAL])
			& (1 << MTIP_TAG_INTERNAL)) {
			dev_warn(&port->dd->pdev->dev,
				"Retiring internal command but CI is 1.\n");
			if (test_bit(MTIP_DDF_REMOVE_PENDING_BIT,
						&port->dd->dd_flag)) {
				hba_reset_nosleep(port->dd);
				rv = -ENXIO;
			} else {
				mtip_restart_port(port);
				rv = -EAGAIN;
			}
			goto exec_ic_exit;
		}

	} else {
		
		timeout = jiffies + msecs_to_jiffies(timeout);
		while ((readl(port->cmd_issue[MTIP_TAG_INTERNAL])
				& (1 << MTIP_TAG_INTERNAL))
				&& time_before(jiffies, timeout)) {
			if (mtip_check_surprise_removal(port->dd->pdev)) {
				rv = -ENXIO;
				goto exec_ic_exit;
			}
			if ((fis->command != ATA_CMD_STANDBYNOW1) &&
				test_bit(MTIP_DDF_REMOVE_PENDING_BIT,
						&port->dd->dd_flag)) {
				rv = -ENXIO;
				goto exec_ic_exit;
			}
		}

		if (readl(port->cmd_issue[MTIP_TAG_INTERNAL])
			& (1 << MTIP_TAG_INTERNAL)) {
			dev_err(&port->dd->pdev->dev,
				"Internal command did not complete [atomic]\n");
			rv = -EAGAIN;
			if (test_bit(MTIP_DDF_REMOVE_PENDING_BIT,
						&port->dd->dd_flag)) {
				hba_reset_nosleep(port->dd);
				rv = -ENXIO;
			} else {
				mtip_restart_port(port);
				rv = -EAGAIN;
			}
		}
	}
exec_ic_exit:
	
	atomic_set(&int_cmd->active, 0);
	release_slot(port, MTIP_TAG_INTERNAL);
	if (rv >= 0 && mtip_pause_ncq(port, fis)) {
		
		return rv;
	}
	clear_bit(MTIP_PF_IC_ACTIVE_BIT, &port->flags);
	wake_up_interruptible(&port->svc_wait);

	return rv;
}

static inline void ata_swap_string(u16 *buf, unsigned int len)
{
	int i;
	for (i = 0; i < (len/2); i++)
		be16_to_cpus(&buf[i]);
}

static int mtip_get_identify(struct mtip_port *port, void __user *user_buffer)
{
	int rv = 0;
	struct host_to_dev_fis fis;

	if (test_bit(MTIP_DDF_REMOVE_PENDING_BIT, &port->dd->dd_flag))
		return -EFAULT;

	
	memset(&fis, 0, sizeof(struct host_to_dev_fis));
	fis.type	= 0x27;
	fis.opts	= 1 << 7;
	fis.command	= ATA_CMD_ID_ATA;

	
	port->identify_valid = 0;

	
	memset(port->identify, 0, sizeof(u16) * ATA_ID_WORDS);

	
	if (mtip_exec_internal_command(port,
				&fis,
				5,
				port->identify_dma,
				sizeof(u16) * ATA_ID_WORDS,
				0,
				GFP_KERNEL,
				MTIP_INTERNAL_COMMAND_TIMEOUT_MS)
				< 0) {
		rv = -1;
		goto out;
	}

#ifdef __LITTLE_ENDIAN
	ata_swap_string(port->identify + 27, 40);  
	ata_swap_string(port->identify + 23, 8);   
	ata_swap_string(port->identify + 10, 20);  
#else
	{
		int i;
		for (i = 0; i < ATA_ID_WORDS; i++)
			port->identify[i] = le16_to_cpu(port->identify[i]);
	}
#endif

	
	port->identify_valid = 1;

	if (user_buffer) {
		if (copy_to_user(
			user_buffer,
			port->identify,
			ATA_ID_WORDS * sizeof(u16))) {
			rv = -EFAULT;
			goto out;
		}
	}

out:
	return rv;
}

static int mtip_standby_immediate(struct mtip_port *port)
{
	int rv;
	struct host_to_dev_fis	fis;
	unsigned long start;

	
	memset(&fis, 0, sizeof(struct host_to_dev_fis));
	fis.type	= 0x27;
	fis.opts	= 1 << 7;
	fis.command	= ATA_CMD_STANDBYNOW1;

	start = jiffies;
	rv = mtip_exec_internal_command(port,
					&fis,
					5,
					0,
					0,
					0,
					GFP_ATOMIC,
					15000);
	dbg_printk(MTIP_DRV_NAME "Time taken to complete standby cmd: %d ms\n",
			jiffies_to_msecs(jiffies - start));
	if (rv)
		dev_warn(&port->dd->pdev->dev,
			"STANDBY IMMEDIATE command failed.\n");

	return rv;
}

static int mtip_read_log_page(struct mtip_port *port, u8 page, u16 *buffer,
				dma_addr_t buffer_dma, unsigned int sectors)
{
	struct host_to_dev_fis fis;

	memset(&fis, 0, sizeof(struct host_to_dev_fis));
	fis.type	= 0x27;
	fis.opts	= 1 << 7;
	fis.command	= ATA_CMD_READ_LOG_EXT;
	fis.sect_count	= sectors & 0xFF;
	fis.sect_cnt_ex	= (sectors >> 8) & 0xFF;
	fis.lba_low	= page;
	fis.lba_mid	= 0;
	fis.device	= ATA_DEVICE_OBS;

	memset(buffer, 0, sectors * ATA_SECT_SIZE);

	return mtip_exec_internal_command(port,
					&fis,
					5,
					buffer_dma,
					sectors * ATA_SECT_SIZE,
					0,
					GFP_ATOMIC,
					MTIP_INTERNAL_COMMAND_TIMEOUT_MS);
}

static int mtip_get_smart_data(struct mtip_port *port, u8 *buffer,
					dma_addr_t buffer_dma)
{
	struct host_to_dev_fis fis;

	memset(&fis, 0, sizeof(struct host_to_dev_fis));
	fis.type	= 0x27;
	fis.opts	= 1 << 7;
	fis.command	= ATA_CMD_SMART;
	fis.features	= 0xD0;
	fis.sect_count	= 1;
	fis.lba_mid	= 0x4F;
	fis.lba_hi	= 0xC2;
	fis.device	= ATA_DEVICE_OBS;

	return mtip_exec_internal_command(port,
					&fis,
					5,
					buffer_dma,
					ATA_SECT_SIZE,
					0,
					GFP_ATOMIC,
					15000);
}

static int mtip_get_smart_attr(struct mtip_port *port, unsigned int id,
						struct smart_attr *attrib)
{
	int rv, i;
	struct smart_attr *pattr;

	if (!attrib)
		return -EINVAL;

	if (!port->identify_valid) {
		dev_warn(&port->dd->pdev->dev, "IDENTIFY DATA not valid\n");
		return -EPERM;
	}
	if (!(port->identify[82] & 0x1)) {
		dev_warn(&port->dd->pdev->dev, "SMART not supported\n");
		return -EPERM;
	}
	if (!(port->identify[85] & 0x1)) {
		dev_warn(&port->dd->pdev->dev, "SMART not enabled\n");
		return -EPERM;
	}

	memset(port->smart_buf, 0, ATA_SECT_SIZE);
	rv = mtip_get_smart_data(port, port->smart_buf, port->smart_buf_dma);
	if (rv) {
		dev_warn(&port->dd->pdev->dev, "Failed to ge SMART data\n");
		return rv;
	}

	pattr = (struct smart_attr *)(port->smart_buf + 2);
	for (i = 0; i < 29; i++, pattr++)
		if (pattr->attr_id == id) {
			memcpy(attrib, pattr, sizeof(struct smart_attr));
			break;
		}

	if (i == 29) {
		dev_warn(&port->dd->pdev->dev,
			"Query for invalid SMART attribute ID\n");
		rv = -EINVAL;
	}

	return rv;
}

static bool mtip_hw_get_capacity(struct driver_data *dd, sector_t *sectors)
{
	struct mtip_port *port = dd->port;
	u64 total, raw0, raw1, raw2, raw3;
	raw0 = port->identify[100];
	raw1 = port->identify[101];
	raw2 = port->identify[102];
	raw3 = port->identify[103];
	total = raw0 | raw1<<16 | raw2<<32 | raw3<<48;
	*sectors = total;
	return (bool) !!port->identify_valid;
}

static int mtip_hba_reset(struct driver_data *dd)
{
	mtip_deinit_port(dd->port);

	
	writel(HOST_RESET, dd->mmio + HOST_CTL);

	
	readl(dd->mmio + HOST_CTL);

	
	ssleep(1);

	
	if (readl(dd->mmio + HOST_CTL) & HOST_RESET) {
		dev_err(&dd->pdev->dev,
			"Reset bit did not clear.\n");
		return -1;
	}

	return 0;
}

static void mtip_dump_identify(struct mtip_port *port)
{
	sector_t sectors;
	unsigned short revid;
	char cbuf[42];

	if (!port->identify_valid)
		return;

	strlcpy(cbuf, (char *)(port->identify+10), 21);
	dev_info(&port->dd->pdev->dev,
		"Serial No.: %s\n", cbuf);

	strlcpy(cbuf, (char *)(port->identify+23), 9);
	dev_info(&port->dd->pdev->dev,
		"Firmware Ver.: %s\n", cbuf);

	strlcpy(cbuf, (char *)(port->identify+27), 41);
	dev_info(&port->dd->pdev->dev, "Model: %s\n", cbuf);

	if (mtip_hw_get_capacity(port->dd, &sectors))
		dev_info(&port->dd->pdev->dev,
			"Capacity: %llu sectors (%llu MB)\n",
			 (u64)sectors,
			 ((u64)sectors) * ATA_SECT_SIZE >> 20);

	pci_read_config_word(port->dd->pdev, PCI_REVISION_ID, &revid);
	switch (revid & 0xFF) {
	case 0x1:
		strlcpy(cbuf, "A0", 3);
		break;
	case 0x3:
		strlcpy(cbuf, "A2", 3);
		break;
	default:
		strlcpy(cbuf, "?", 2);
		break;
	}
	dev_info(&port->dd->pdev->dev,
		"Card Type: %s\n", cbuf);
}

static inline void fill_command_sg(struct driver_data *dd,
				struct mtip_cmd *command,
				int nents)
{
	int n;
	unsigned int dma_len;
	struct mtip_cmd_sg *command_sg;
	struct scatterlist *sg = command->sg;

	command_sg = command->command + AHCI_CMD_TBL_HDR_SZ;

	for (n = 0; n < nents; n++) {
		dma_len = sg_dma_len(sg);
		if (dma_len > 0x400000)
			dev_err(&dd->pdev->dev,
				"DMA segment length truncated\n");
		command_sg->info = __force_bit2int
			cpu_to_le32((dma_len-1) & 0x3FFFFF);
		command_sg->dba	= __force_bit2int
			cpu_to_le32(sg_dma_address(sg));
		command_sg->dba_upper = __force_bit2int
			cpu_to_le32((sg_dma_address(sg) >> 16) >> 16);
		command_sg++;
		sg++;
	}
}

static int exec_drive_task(struct mtip_port *port, u8 *command)
{
	struct host_to_dev_fis	fis;
	struct host_to_dev_fis *reply = (port->rxfis + RX_FIS_D2H_REG);

	
	memset(&fis, 0, sizeof(struct host_to_dev_fis));
	fis.type	= 0x27;
	fis.opts	= 1 << 7;
	fis.command	= command[0];
	fis.features	= command[1];
	fis.sect_count	= command[2];
	fis.sector	= command[3];
	fis.cyl_low	= command[4];
	fis.cyl_hi	= command[5];
	fis.device	= command[6] & ~0x10; 

	dbg_printk(MTIP_DRV_NAME " %s: User Command: cmd %x, feat %x, nsect %x, sect %x, lcyl %x, hcyl %x, sel %x\n",
		__func__,
		command[0],
		command[1],
		command[2],
		command[3],
		command[4],
		command[5],
		command[6]);

	
	if (mtip_exec_internal_command(port,
				 &fis,
				 5,
				 0,
				 0,
				 0,
				 GFP_KERNEL,
				 MTIP_IOCTL_COMMAND_TIMEOUT_MS) < 0) {
		return -1;
	}

	command[0] = reply->command; 
	command[1] = reply->features; 
	command[4] = reply->cyl_low;
	command[5] = reply->cyl_hi;

	dbg_printk(MTIP_DRV_NAME " %s: Completion Status: stat %x, err %x , cyl_lo %x cyl_hi %x\n",
		__func__,
		command[0],
		command[1],
		command[4],
		command[5]);

	return 0;
}

static int exec_drive_command(struct mtip_port *port, u8 *command,
				void __user *user_buffer)
{
	struct host_to_dev_fis	fis;
	struct host_to_dev_fis *reply = (port->rxfis + RX_FIS_D2H_REG);

	
	memset(&fis, 0, sizeof(struct host_to_dev_fis));
	fis.type		= 0x27;
	fis.opts		= 1 << 7;
	fis.command		= command[0];
	fis.features	= command[2];
	fis.sect_count	= command[3];
	if (fis.command == ATA_CMD_SMART) {
		fis.sector	= command[1];
		fis.cyl_low	= 0x4F;
		fis.cyl_hi	= 0xC2;
	}

	dbg_printk(MTIP_DRV_NAME
		" %s: User Command: cmd %x, sect %x, "
		"feat %x, sectcnt %x\n",
		__func__,
		command[0],
		command[1],
		command[2],
		command[3]);

	memset(port->sector_buffer, 0x00, ATA_SECT_SIZE);

	
	if (mtip_exec_internal_command(port,
				&fis,
				 5,
				 port->sector_buffer_dma,
				 (command[3] != 0) ? ATA_SECT_SIZE : 0,
				 0,
				 GFP_KERNEL,
				 MTIP_IOCTL_COMMAND_TIMEOUT_MS)
				 < 0) {
		return -1;
	}

	
	command[0] = reply->command; 
	command[1] = reply->features; 
	command[2] = command[3];

	dbg_printk(MTIP_DRV_NAME
		" %s: Completion Status: stat %x, "
		"err %x, cmd %x\n",
		__func__,
		command[0],
		command[1],
		command[2]);

	if (user_buffer && command[3]) {
		if (copy_to_user(user_buffer,
				 port->sector_buffer,
				 ATA_SECT_SIZE * command[3])) {
			return -EFAULT;
		}
	}

	return 0;
}

static unsigned int implicit_sector(unsigned char command,
				    unsigned char features)
{
	unsigned int rv = 0;

	
	switch (command) {
	case ATA_CMD_SEC_SET_PASS:
	case ATA_CMD_SEC_UNLOCK:
	case ATA_CMD_SEC_ERASE_PREP:
	case ATA_CMD_SEC_ERASE_UNIT:
	case ATA_CMD_SEC_FREEZE_LOCK:
	case ATA_CMD_SEC_DISABLE_PASS:
	case ATA_CMD_PMP_READ:
	case ATA_CMD_PMP_WRITE:
		rv = 1;
		break;
	case ATA_CMD_SET_MAX:
		if (features == ATA_SET_MAX_UNLOCK)
			rv = 1;
		break;
	case ATA_CMD_SMART:
		if ((features == ATA_SMART_READ_VALUES) ||
				(features == ATA_SMART_READ_THRESHOLDS))
			rv = 1;
		break;
	case ATA_CMD_CONF_OVERLAY:
		if ((features == ATA_DCO_IDENTIFY) ||
				(features == ATA_DCO_SET))
			rv = 1;
		break;
	}
	return rv;
}

static int exec_drive_taskfile(struct driver_data *dd,
			       void __user *buf,
			       ide_task_request_t *req_task,
			       int outtotal)
{
	struct host_to_dev_fis	fis;
	struct host_to_dev_fis *reply;
	u8 *outbuf = NULL;
	u8 *inbuf = NULL;
	dma_addr_t outbuf_dma = 0;
	dma_addr_t inbuf_dma = 0;
	dma_addr_t dma_buffer = 0;
	int err = 0;
	unsigned int taskin = 0;
	unsigned int taskout = 0;
	u8 nsect = 0;
	unsigned int timeout = MTIP_IOCTL_COMMAND_TIMEOUT_MS;
	unsigned int force_single_sector;
	unsigned int transfer_size;
	unsigned long task_file_data;
	int intotal = outtotal + req_task->out_size;

	taskout = req_task->out_size;
	taskin = req_task->in_size;
	
	if (taskin > 130560 || taskout > 130560) {
		err = -EINVAL;
		goto abort;
	}

	if (taskout) {
		outbuf = kzalloc(taskout, GFP_KERNEL);
		if (outbuf == NULL) {
			err = -ENOMEM;
			goto abort;
		}
		if (copy_from_user(outbuf, buf + outtotal, taskout)) {
			err = -EFAULT;
			goto abort;
		}
		outbuf_dma = pci_map_single(dd->pdev,
					 outbuf,
					 taskout,
					 DMA_TO_DEVICE);
		if (outbuf_dma == 0) {
			err = -ENOMEM;
			goto abort;
		}
		dma_buffer = outbuf_dma;
	}

	if (taskin) {
		inbuf = kzalloc(taskin, GFP_KERNEL);
		if (inbuf == NULL) {
			err = -ENOMEM;
			goto abort;
		}

		if (copy_from_user(inbuf, buf + intotal, taskin)) {
			err = -EFAULT;
			goto abort;
		}
		inbuf_dma = pci_map_single(dd->pdev,
					 inbuf,
					 taskin, DMA_FROM_DEVICE);
		if (inbuf_dma == 0) {
			err = -ENOMEM;
			goto abort;
		}
		dma_buffer = inbuf_dma;
	}

	
	switch (req_task->data_phase) {
	case TASKFILE_OUT:
		nsect = taskout / ATA_SECT_SIZE;
		reply = (dd->port->rxfis + RX_FIS_PIO_SETUP);
		break;
	case TASKFILE_IN:
		reply = (dd->port->rxfis + RX_FIS_PIO_SETUP);
		break;
	case TASKFILE_NO_DATA:
		reply = (dd->port->rxfis + RX_FIS_D2H_REG);
		break;
	default:
		err = -EINVAL;
		goto abort;
	}

	
	memset(&fis, 0, sizeof(struct host_to_dev_fis));

	fis.type	= 0x27;
	fis.opts	= 1 << 7;
	fis.command	= req_task->io_ports[7];
	fis.features	= req_task->io_ports[1];
	fis.sect_count	= req_task->io_ports[2];
	fis.lba_low	= req_task->io_ports[3];
	fis.lba_mid	= req_task->io_ports[4];
	fis.lba_hi	= req_task->io_ports[5];
	 
	fis.device	= req_task->io_ports[6] & ~0x10;

	if ((req_task->in_flags.all == 0) && (req_task->out_flags.all & 1)) {
		req_task->in_flags.all	=
			IDE_TASKFILE_STD_IN_FLAGS |
			(IDE_HOB_STD_IN_FLAGS << 8);
		fis.lba_low_ex		= req_task->hob_ports[3];
		fis.lba_mid_ex		= req_task->hob_ports[4];
		fis.lba_hi_ex		= req_task->hob_ports[5];
		fis.features_ex		= req_task->hob_ports[1];
		fis.sect_cnt_ex		= req_task->hob_ports[2];

	} else {
		req_task->in_flags.all = IDE_TASKFILE_STD_IN_FLAGS;
	}

	force_single_sector = implicit_sector(fis.command, fis.features);

	if ((taskin || taskout) && (!fis.sect_count)) {
		if (nsect)
			fis.sect_count = nsect;
		else {
			if (!force_single_sector) {
				dev_warn(&dd->pdev->dev,
					"data movement but "
					"sect_count is 0\n");
					err = -EINVAL;
					goto abort;
			}
		}
	}

	dbg_printk(MTIP_DRV_NAME
		" %s: cmd %x, feat %x, nsect %x,"
		" sect/lbal %x, lcyl/lbam %x, hcyl/lbah %x,"
		" head/dev %x\n",
		__func__,
		fis.command,
		fis.features,
		fis.sect_count,
		fis.lba_low,
		fis.lba_mid,
		fis.lba_hi,
		fis.device);

	switch (fis.command) {
	case ATA_CMD_DOWNLOAD_MICRO:
		
		timeout = 120000;
		break;
	case ATA_CMD_SEC_ERASE_UNIT:
		
		timeout = 240000;
		break;
	case ATA_CMD_STANDBYNOW1:
		
		timeout = 10000;
		break;
	case 0xF7:
	case 0xFA:
		
		timeout = 10000;
		break;
	case ATA_CMD_SMART:
		
		timeout = 15000;
		break;
	default:
		timeout = MTIP_IOCTL_COMMAND_TIMEOUT_MS;
		break;
	}

	
	if (force_single_sector)
		transfer_size = ATA_SECT_SIZE;
	else
		transfer_size = ATA_SECT_SIZE * fis.sect_count;

	
	if (mtip_exec_internal_command(dd->port,
				 &fis,
				 5,
				 dma_buffer,
				 transfer_size,
				 0,
				 GFP_KERNEL,
				 timeout) < 0) {
		err = -EIO;
		goto abort;
	}

	task_file_data = readl(dd->port->mmio+PORT_TFDATA);

	if ((req_task->data_phase == TASKFILE_IN) && !(task_file_data & 1)) {
		reply = dd->port->rxfis + RX_FIS_PIO_SETUP;
		req_task->io_ports[7] = reply->control;
	} else {
		reply = dd->port->rxfis + RX_FIS_D2H_REG;
		req_task->io_ports[7] = reply->command;
	}

	
	if (inbuf_dma)
		pci_unmap_single(dd->pdev, inbuf_dma,
			taskin, DMA_FROM_DEVICE);
	if (outbuf_dma)
		pci_unmap_single(dd->pdev, outbuf_dma,
			taskout, DMA_TO_DEVICE);
	inbuf_dma  = 0;
	outbuf_dma = 0;

	
	req_task->io_ports[1] = reply->features;
	req_task->io_ports[2] = reply->sect_count;
	req_task->io_ports[3] = reply->lba_low;
	req_task->io_ports[4] = reply->lba_mid;
	req_task->io_ports[5] = reply->lba_hi;
	req_task->io_ports[6] = reply->device;

	if (req_task->out_flags.all & 1)  {

		req_task->hob_ports[3] = reply->lba_low_ex;
		req_task->hob_ports[4] = reply->lba_mid_ex;
		req_task->hob_ports[5] = reply->lba_hi_ex;
		req_task->hob_ports[1] = reply->features_ex;
		req_task->hob_ports[2] = reply->sect_cnt_ex;
	}
	dbg_printk(MTIP_DRV_NAME
		" %s: Completion: stat %x,"
		"err %x, sect_cnt %x, lbalo %x,"
		"lbamid %x, lbahi %x, dev %x\n",
		__func__,
		req_task->io_ports[7],
		req_task->io_ports[1],
		req_task->io_ports[2],
		req_task->io_ports[3],
		req_task->io_ports[4],
		req_task->io_ports[5],
		req_task->io_ports[6]);

	if (taskout) {
		if (copy_to_user(buf + outtotal, outbuf, taskout)) {
			err = -EFAULT;
			goto abort;
		}
	}
	if (taskin) {
		if (copy_to_user(buf + intotal, inbuf, taskin)) {
			err = -EFAULT;
			goto abort;
		}
	}
abort:
	if (inbuf_dma)
		pci_unmap_single(dd->pdev, inbuf_dma,
					taskin, DMA_FROM_DEVICE);
	if (outbuf_dma)
		pci_unmap_single(dd->pdev, outbuf_dma,
					taskout, DMA_TO_DEVICE);
	kfree(outbuf);
	kfree(inbuf);

	return err;
}

static int mtip_hw_ioctl(struct driver_data *dd, unsigned int cmd,
			 unsigned long arg)
{
	switch (cmd) {
	case HDIO_GET_IDENTITY:
		if (mtip_get_identify(dd->port, (void __user *) arg) < 0) {
			dev_warn(&dd->pdev->dev,
				"Unable to read identity\n");
			return -EIO;
		}

		break;
	case HDIO_DRIVE_CMD:
	{
		u8 drive_command[4];

		
		if (copy_from_user(drive_command,
					 (void __user *) arg,
					 sizeof(drive_command)))
			return -EFAULT;

		
		if (exec_drive_command(dd->port,
					 drive_command,
					 (void __user *) (arg+4)))
			return -EIO;

		
		if (copy_to_user((void __user *) arg,
					 drive_command,
					 sizeof(drive_command)))
			return -EFAULT;

		break;
	}
	case HDIO_DRIVE_TASK:
	{
		u8 drive_command[7];

		
		if (copy_from_user(drive_command,
					 (void __user *) arg,
					 sizeof(drive_command)))
			return -EFAULT;

		
		if (exec_drive_task(dd->port, drive_command))
			return -EIO;

		
		if (copy_to_user((void __user *) arg,
					 drive_command,
					 sizeof(drive_command)))
			return -EFAULT;

		break;
	}
	case HDIO_DRIVE_TASKFILE: {
		ide_task_request_t req_task;
		int ret, outtotal;

		if (copy_from_user(&req_task, (void __user *) arg,
					sizeof(req_task)))
			return -EFAULT;

		outtotal = sizeof(req_task);

		ret = exec_drive_taskfile(dd, (void __user *) arg,
						&req_task, outtotal);

		if (copy_to_user((void __user *) arg, &req_task,
							sizeof(req_task)))
			return -EFAULT;

		return ret;
	}

	default:
		return -EINVAL;
	}
	return 0;
}

static void mtip_hw_submit_io(struct driver_data *dd, sector_t start,
			      int nsect, int nents, int tag, void *callback,
			      void *data, int dir)
{
	struct host_to_dev_fis	*fis;
	struct mtip_port *port = dd->port;
	struct mtip_cmd *command = &port->commands[tag];
	int dma_dir = (dir == READ) ? DMA_FROM_DEVICE : DMA_TO_DEVICE;

	
	nents = dma_map_sg(&dd->pdev->dev, command->sg, nents, dma_dir);

	command->scatter_ents = nents;

	command->retries = MTIP_MAX_RETRIES;

	
	fis = command->command;
	fis->type        = 0x27;
	fis->opts        = 1 << 7;
	fis->command     =
		(dir == READ ? ATA_CMD_FPDMA_READ : ATA_CMD_FPDMA_WRITE);
	*((unsigned int *) &fis->lba_low) = (start & 0xFFFFFF);
	*((unsigned int *) &fis->lba_low_ex) = ((start >> 24) & 0xFFFFFF);
	fis->device	 = 1 << 6;
	fis->features    = nsect & 0xFF;
	fis->features_ex = (nsect >> 8) & 0xFF;
	fis->sect_count  = ((tag << 3) | (tag >> 5));
	fis->sect_cnt_ex = 0;
	fis->control     = 0;
	fis->res2        = 0;
	fis->res3        = 0;
	fill_command_sg(dd, command, nents);

	
	command->command_header->opts =
			__force_bit2int cpu_to_le32(
				(nents << 16) | 5 | AHCI_CMD_PREFETCH);
	command->command_header->byte_count = 0;

	command->comp_data = dd;
	command->comp_func = mtip_async_complete;
	command->direction = dma_dir;

	command->async_data = data;
	command->async_callback = callback;

	if (port->flags & MTIP_PF_PAUSE_IO) {
		set_bit(tag, port->cmds_to_issue);
		set_bit(MTIP_PF_ISSUE_CMDS_BIT, &port->flags);
		return;
	}

	
	mtip_issue_ncq_command(port, tag);

	return;
}

static void mtip_hw_release_scatterlist(struct driver_data *dd, int tag)
{
	release_slot(dd->port, tag);
}

static struct scatterlist *mtip_hw_get_scatterlist(struct driver_data *dd,
						   int *tag)
{
	down(&dd->port->cmd_slot);
	*tag = get_slot(dd->port);

	if (unlikely(test_bit(MTIP_DDF_REMOVE_PENDING_BIT, &dd->dd_flag))) {
		up(&dd->port->cmd_slot);
		return NULL;
	}
	if (unlikely(*tag < 0)) {
		up(&dd->port->cmd_slot);
		return NULL;
	}

	return dd->port->commands[*tag].sg;
}

static ssize_t mtip_hw_show_registers(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	u32 group_allocated;
	struct driver_data *dd = dev_to_disk(dev)->private_data;
	int size = 0;
	int n;

	size += sprintf(&buf[size], "S ACTive:\n");

	for (n = 0; n < dd->slot_groups; n++)
		size += sprintf(&buf[size], "0x%08x\n",
					 readl(dd->port->s_active[n]));

	size += sprintf(&buf[size], "Command Issue:\n");

	for (n = 0; n < dd->slot_groups; n++)
		size += sprintf(&buf[size], "0x%08x\n",
					readl(dd->port->cmd_issue[n]));

	size += sprintf(&buf[size], "Allocated:\n");

	for (n = 0; n < dd->slot_groups; n++) {
		if (sizeof(long) > sizeof(u32))
			group_allocated =
				dd->port->allocated[n/2] >> (32*(n&1));
		else
			group_allocated = dd->port->allocated[n];
		size += sprintf(&buf[size], "0x%08x\n",
				 group_allocated);
	}

	size += sprintf(&buf[size], "Completed:\n");

	for (n = 0; n < dd->slot_groups; n++)
		size += sprintf(&buf[size], "0x%08x\n",
				readl(dd->port->completed[n]));

	size += sprintf(&buf[size], "PORT IRQ STAT : 0x%08x\n",
				readl(dd->port->mmio + PORT_IRQ_STAT));
	size += sprintf(&buf[size], "HOST IRQ STAT : 0x%08x\n",
				readl(dd->mmio + HOST_IRQ_STAT));

	return size;
}

static ssize_t mtip_hw_show_status(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct driver_data *dd = dev_to_disk(dev)->private_data;
	int size = 0;

	if (test_bit(MTIP_DDF_OVER_TEMP_BIT, &dd->dd_flag))
		size += sprintf(buf, "%s", "thermal_shutdown\n");
	else if (test_bit(MTIP_DDF_WRITE_PROTECT_BIT, &dd->dd_flag))
		size += sprintf(buf, "%s", "write_protect\n");
	else
		size += sprintf(buf, "%s", "online\n");

	return size;
}

static DEVICE_ATTR(registers, S_IRUGO, mtip_hw_show_registers, NULL);
static DEVICE_ATTR(status, S_IRUGO, mtip_hw_show_status, NULL);

static int mtip_hw_sysfs_init(struct driver_data *dd, struct kobject *kobj)
{
	if (!kobj || !dd)
		return -EINVAL;

	if (sysfs_create_file(kobj, &dev_attr_registers.attr))
		dev_warn(&dd->pdev->dev,
			"Error creating 'registers' sysfs entry\n");
	if (sysfs_create_file(kobj, &dev_attr_status.attr))
		dev_warn(&dd->pdev->dev,
			"Error creating 'status' sysfs entry\n");
	return 0;
}

static int mtip_hw_sysfs_exit(struct driver_data *dd, struct kobject *kobj)
{
	if (!kobj || !dd)
		return -EINVAL;

	sysfs_remove_file(kobj, &dev_attr_registers.attr);
	sysfs_remove_file(kobj, &dev_attr_status.attr);

	return 0;
}

static inline void hba_setup(struct driver_data *dd)
{
	u32 hwdata;
	hwdata = readl(dd->mmio + HOST_HSORG);

	
	writel(hwdata |
		HSORG_DISABLE_SLOTGRP_INTR |
		HSORG_DISABLE_SLOTGRP_PXIS,
		dd->mmio + HOST_HSORG);
}

static void mtip_detect_product(struct driver_data *dd)
{
	u32 hwdata;
	unsigned int rev, slotgroups;

	hwdata = readl(dd->mmio + HOST_HSORG);

	dd->product_type = MTIP_PRODUCT_UNKNOWN;
	dd->slot_groups = 1;

	if (hwdata & 0x8) {
		dd->product_type = MTIP_PRODUCT_ASICFPGA;
		rev = (hwdata & HSORG_HWREV) >> 8;
		slotgroups = (hwdata & HSORG_SLOTGROUPS) + 1;
		dev_info(&dd->pdev->dev,
			"ASIC-FPGA design, HS rev 0x%x, "
			"%i slot groups [%i slots]\n",
			 rev,
			 slotgroups,
			 slotgroups * 32);

		if (slotgroups > MTIP_MAX_SLOT_GROUPS) {
			dev_warn(&dd->pdev->dev,
				"Warning: driver only supports "
				"%i slot groups.\n", MTIP_MAX_SLOT_GROUPS);
			slotgroups = MTIP_MAX_SLOT_GROUPS;
		}
		dd->slot_groups = slotgroups;
		return;
	}

	dev_warn(&dd->pdev->dev, "Unrecognized product id\n");
}

static int mtip_ftl_rebuild_poll(struct driver_data *dd)
{
	unsigned long timeout, cnt = 0, start;

	dev_warn(&dd->pdev->dev,
		"FTL rebuild in progress. Polling for completion.\n");

	start = jiffies;
	timeout = jiffies + msecs_to_jiffies(MTIP_FTL_REBUILD_TIMEOUT_MS);

	do {
		if (unlikely(test_bit(MTIP_DDF_REMOVE_PENDING_BIT,
				&dd->dd_flag)))
			return -EFAULT;
		if (mtip_check_surprise_removal(dd->pdev))
			return -EFAULT;

		if (mtip_get_identify(dd->port, NULL) < 0)
			return -EFAULT;

		if (*(dd->port->identify + MTIP_FTL_REBUILD_OFFSET) ==
			MTIP_FTL_REBUILD_MAGIC) {
			ssleep(1);
			
			if (cnt++ >= 180) {
				dev_warn(&dd->pdev->dev,
				"FTL rebuild in progress (%d secs).\n",
				jiffies_to_msecs(jiffies - start) / 1000);
				cnt = 0;
			}
		} else {
			dev_warn(&dd->pdev->dev,
				"FTL rebuild complete (%d secs).\n",
			jiffies_to_msecs(jiffies - start) / 1000);
			mtip_block_initialize(dd);
			return 0;
		}
		ssleep(10);
	} while (time_before(jiffies, timeout));

	
	dev_err(&dd->pdev->dev,
		"Timed out waiting for FTL rebuild to complete (%d secs).\n",
		jiffies_to_msecs(jiffies - start) / 1000);
	return -EFAULT;
}


static int mtip_service_thread(void *data)
{
	struct driver_data *dd = (struct driver_data *)data;
	unsigned long slot, slot_start, slot_wrap;
	unsigned int num_cmd_slots = dd->slot_groups * 32;
	struct mtip_port *port = dd->port;

	while (1) {
		wait_event_interruptible(port->svc_wait, (port->flags) &&
			!(port->flags & MTIP_PF_PAUSE_IO));

		if (kthread_should_stop())
			break;

		if (unlikely(test_bit(MTIP_DDF_REMOVE_PENDING_BIT,
				&dd->dd_flag)))
			break;

		set_bit(MTIP_PF_SVC_THD_ACTIVE_BIT, &port->flags);
		if (test_bit(MTIP_PF_ISSUE_CMDS_BIT, &port->flags)) {
			slot = 1;
			
			slot_start = num_cmd_slots;
			slot_wrap = 0;
			while (1) {
				slot = find_next_bit(port->cmds_to_issue,
						num_cmd_slots, slot);
				if (slot_wrap == 1) {
					if ((slot_start >= slot) ||
						(slot >= num_cmd_slots))
						break;
				}
				if (unlikely(slot_start == num_cmd_slots))
					slot_start = slot;

				if (unlikely(slot == num_cmd_slots)) {
					slot = 1;
					slot_wrap = 1;
					continue;
				}

				
				mtip_issue_ncq_command(port, slot);

				clear_bit(slot, port->cmds_to_issue);
			}

			clear_bit(MTIP_PF_ISSUE_CMDS_BIT, &port->flags);
		} else if (test_bit(MTIP_PF_REBUILD_BIT, &port->flags)) {
			if (!mtip_ftl_rebuild_poll(dd))
				set_bit(MTIP_DDF_REBUILD_FAILED_BIT,
							&dd->dd_flag);
			clear_bit(MTIP_PF_REBUILD_BIT, &port->flags);
		}
		clear_bit(MTIP_PF_SVC_THD_ACTIVE_BIT, &port->flags);

		if (test_bit(MTIP_PF_SVC_THD_STOP_BIT, &port->flags))
			break;
	}
	return 0;
}

static int mtip_hw_init(struct driver_data *dd)
{
	int i;
	int rv;
	unsigned int num_command_slots;
	unsigned long timeout, timetaken;
	unsigned char *buf;
	struct smart_attr attr242;

	dd->mmio = pcim_iomap_table(dd->pdev)[MTIP_ABAR];

	mtip_detect_product(dd);
	if (dd->product_type == MTIP_PRODUCT_UNKNOWN) {
		rv = -EIO;
		goto out1;
	}
	num_command_slots = dd->slot_groups * 32;

	hba_setup(dd);

	tasklet_init(&dd->tasklet, mtip_tasklet, (unsigned long)dd);

	dd->port = kzalloc(sizeof(struct mtip_port), GFP_KERNEL);
	if (!dd->port) {
		dev_err(&dd->pdev->dev,
			"Memory allocation: port structure\n");
		return -ENOMEM;
	}

	
	sema_init(&dd->port->cmd_slot, num_command_slots - 1);

	
	spin_lock_init(&dd->port->cmd_issue_lock);

	
	dd->port->mmio	= dd->mmio + PORT_OFFSET;
	dd->port->dd	= dd;

	
	dd->port->command_list =
		dmam_alloc_coherent(&dd->pdev->dev,
			HW_PORT_PRIV_DMA_SZ + (ATA_SECT_SIZE * 4),
			&dd->port->command_list_dma,
			GFP_KERNEL);
	if (!dd->port->command_list) {
		dev_err(&dd->pdev->dev,
			"Memory allocation: command list\n");
		rv = -ENOMEM;
		goto out1;
	}

	
	memset(dd->port->command_list,
		0,
		HW_PORT_PRIV_DMA_SZ + (ATA_SECT_SIZE * 4));

	
	dd->port->rxfis	    = dd->port->command_list + HW_CMD_SLOT_SZ;
	dd->port->rxfis_dma = dd->port->command_list_dma + HW_CMD_SLOT_SZ;

	
	dd->port->command_table	  = dd->port->rxfis + AHCI_RX_FIS_SZ;
	dd->port->command_tbl_dma = dd->port->rxfis_dma + AHCI_RX_FIS_SZ;

	
	dd->port->identify     = dd->port->command_table +
					HW_CMD_TBL_AR_SZ;
	dd->port->identify_dma = dd->port->command_tbl_dma +
					HW_CMD_TBL_AR_SZ;

	
	dd->port->sector_buffer	= (void *) dd->port->identify + ATA_SECT_SIZE;
	dd->port->sector_buffer_dma = dd->port->identify_dma + ATA_SECT_SIZE;

	
	dd->port->log_buf = (void *)dd->port->sector_buffer  + ATA_SECT_SIZE;
	dd->port->log_buf_dma = dd->port->sector_buffer_dma + ATA_SECT_SIZE;

	
	dd->port->smart_buf = (void *)dd->port->log_buf  + ATA_SECT_SIZE;
	dd->port->smart_buf_dma = dd->port->log_buf_dma + ATA_SECT_SIZE;


	
	for (i = 0; i < num_command_slots; i++) {
		dd->port->commands[i].command_header =
					dd->port->command_list +
					(sizeof(struct mtip_cmd_hdr) * i);
		dd->port->commands[i].command_header_dma =
					dd->port->command_list_dma +
					(sizeof(struct mtip_cmd_hdr) * i);

		dd->port->commands[i].command =
			dd->port->command_table + (HW_CMD_TBL_SZ * i);
		dd->port->commands[i].command_dma =
			dd->port->command_tbl_dma + (HW_CMD_TBL_SZ * i);

		if (readl(dd->mmio + HOST_CAP) & HOST_CAP_64)
			dd->port->commands[i].command_header->ctbau =
			__force_bit2int cpu_to_le32(
			(dd->port->commands[i].command_dma >> 16) >> 16);
		dd->port->commands[i].command_header->ctba =
			__force_bit2int cpu_to_le32(
			dd->port->commands[i].command_dma & 0xFFFFFFFF);

		sg_init_table(dd->port->commands[i].sg, MTIP_MAX_SG);

		
		atomic_set(&dd->port->commands[i].active, 0);
	}

	
	for (i = 0; i < dd->slot_groups; i++) {
		dd->port->s_active[i] =
			dd->port->mmio + i*0x80 + PORT_SCR_ACT;
		dd->port->cmd_issue[i] =
			dd->port->mmio + i*0x80 + PORT_COMMAND_ISSUE;
		dd->port->completed[i] =
			dd->port->mmio + i*0x80 + PORT_SDBV;
	}

	timetaken = jiffies;
	timeout = jiffies + msecs_to_jiffies(30000);
	while (((readl(dd->port->mmio + PORT_SCR_STAT) & 0x0F) != 0x03) &&
		 time_before(jiffies, timeout)) {
		mdelay(100);
	}
	if (unlikely(mtip_check_surprise_removal(dd->pdev))) {
		timetaken = jiffies - timetaken;
		dev_warn(&dd->pdev->dev,
			"Surprise removal detected at %u ms\n",
			jiffies_to_msecs(timetaken));
		rv = -ENODEV;
		goto out2 ;
	}
	if (unlikely(test_bit(MTIP_DDF_REMOVE_PENDING_BIT, &dd->dd_flag))) {
		timetaken = jiffies - timetaken;
		dev_warn(&dd->pdev->dev,
			"Removal detected at %u ms\n",
			jiffies_to_msecs(timetaken));
		rv = -EFAULT;
		goto out2;
	}

	
	if (!(readl(dd->mmio + HOST_CAP) & HOST_CAP_NZDMA)) {
		if (mtip_hba_reset(dd) < 0) {
			dev_err(&dd->pdev->dev,
				"Card did not reset within timeout\n");
			rv = -EIO;
			goto out2;
		}
	} else {
		
		writel(readl(dd->mmio + HOST_IRQ_STAT),
			dd->mmio + HOST_IRQ_STAT);
	}

	mtip_init_port(dd->port);
	mtip_start_port(dd->port);

	
	rv = devm_request_irq(&dd->pdev->dev,
				dd->pdev->irq,
				mtip_irq_handler,
				IRQF_SHARED,
				dev_driver_string(&dd->pdev->dev),
				dd);

	if (rv) {
		dev_err(&dd->pdev->dev,
			"Unable to allocate IRQ %d\n", dd->pdev->irq);
		goto out2;
	}

	
	writel(readl(dd->mmio + HOST_CTL) | HOST_IRQ_EN,
					dd->mmio + HOST_CTL);

	init_timer(&dd->port->cmd_timer);
	init_waitqueue_head(&dd->port->svc_wait);

	dd->port->cmd_timer.data = (unsigned long int) dd->port;
	dd->port->cmd_timer.function = mtip_timeout_function;
	mod_timer(&dd->port->cmd_timer,
		jiffies + msecs_to_jiffies(MTIP_TIMEOUT_CHECK_PERIOD));


	if (test_bit(MTIP_DDF_REMOVE_PENDING_BIT, &dd->dd_flag)) {
		rv = -EFAULT;
		goto out3;
	}

	if (mtip_get_identify(dd->port, NULL) < 0) {
		rv = -EFAULT;
		goto out3;
	}

	if (*(dd->port->identify + MTIP_FTL_REBUILD_OFFSET) ==
		MTIP_FTL_REBUILD_MAGIC) {
		set_bit(MTIP_PF_REBUILD_BIT, &dd->port->flags);
		return MTIP_FTL_REBUILD_MAGIC;
	}
	mtip_dump_identify(dd->port);

	
	rv = mtip_read_log_page(dd->port, ATA_LOG_SATA_NCQ,
				dd->port->log_buf,
				dd->port->log_buf_dma, 1);
	if (rv) {
		dev_warn(&dd->pdev->dev,
			"Error in READ LOG EXT (10h) command\n");
		
	} else {
		buf = (unsigned char *)dd->port->log_buf;
		if (buf[259] & 0x1) {
			dev_info(&dd->pdev->dev,
				"Write protect bit is set.\n");
			set_bit(MTIP_DDF_WRITE_PROTECT_BIT, &dd->dd_flag);
		}
		if (buf[288] == 0xF7) {
			dev_info(&dd->pdev->dev,
				"Exceeded Tmax, drive in thermal shutdown.\n");
			set_bit(MTIP_DDF_OVER_TEMP_BIT, &dd->dd_flag);
		}
		if (buf[288] == 0xBF) {
			dev_info(&dd->pdev->dev,
				"Drive indicates rebuild has failed.\n");
			
		}
	}

	
	memset(&attr242, 0, sizeof(struct smart_attr));
	if (mtip_get_smart_attr(dd->port, 242, &attr242))
		dev_warn(&dd->pdev->dev,
				"Unable to check write protect progress\n");
	else
		dev_info(&dd->pdev->dev,
				"Write protect progress: %d%% (%d blocks)\n",
				attr242.cur, attr242.data);
	return rv;

out3:
	del_timer_sync(&dd->port->cmd_timer);

	
	writel(readl(dd->mmio + HOST_CTL) & ~HOST_IRQ_EN,
			dd->mmio + HOST_CTL);

	
	devm_free_irq(&dd->pdev->dev, dd->pdev->irq, dd);

out2:
	mtip_deinit_port(dd->port);

	
	dmam_free_coherent(&dd->pdev->dev,
				HW_PORT_PRIV_DMA_SZ + (ATA_SECT_SIZE * 4),
				dd->port->command_list,
				dd->port->command_list_dma);
out1:
	
	kfree(dd->port);

	return rv;
}

static int mtip_hw_exit(struct driver_data *dd)
{
	if (!test_bit(MTIP_DDF_CLEANUP_BIT, &dd->dd_flag)) {

		if (!test_bit(MTIP_PF_REBUILD_BIT, &dd->port->flags))
			if (mtip_standby_immediate(dd->port))
				dev_warn(&dd->pdev->dev,
					"STANDBY IMMEDIATE failed\n");

		
		mtip_deinit_port(dd->port);

		
		writel(readl(dd->mmio + HOST_CTL) & ~HOST_IRQ_EN,
				dd->mmio + HOST_CTL);
	}

	del_timer_sync(&dd->port->cmd_timer);

	
	devm_free_irq(&dd->pdev->dev, dd->pdev->irq, dd);

	
	tasklet_kill(&dd->tasklet);

	
	dmam_free_coherent(&dd->pdev->dev,
			HW_PORT_PRIV_DMA_SZ + (ATA_SECT_SIZE * 4),
			dd->port->command_list,
			dd->port->command_list_dma);
	
	kfree(dd->port);

	return 0;
}

static int mtip_hw_shutdown(struct driver_data *dd)
{
	mtip_standby_immediate(dd->port);

	return 0;
}

static int mtip_hw_suspend(struct driver_data *dd)
{
	if (mtip_standby_immediate(dd->port) != 0) {
		dev_err(&dd->pdev->dev,
			"Failed standby-immediate command\n");
		return -EFAULT;
	}

	
	writel(readl(dd->mmio + HOST_CTL) & ~HOST_IRQ_EN,
			dd->mmio + HOST_CTL);
	mtip_deinit_port(dd->port);

	return 0;
}

static int mtip_hw_resume(struct driver_data *dd)
{
	
	hba_setup(dd);

	
	if (mtip_hba_reset(dd) != 0) {
		dev_err(&dd->pdev->dev,
			"Unable to reset the HBA\n");
		return -EFAULT;
	}

	mtip_init_port(dd->port);
	mtip_start_port(dd->port);

	
	writel(readl(dd->mmio + HOST_CTL) | HOST_IRQ_EN,
			dd->mmio + HOST_CTL);

	return 0;
}

static int rssd_disk_name_format(char *prefix,
				 int index,
				 char *buf,
				 int buflen)
{
	const int base = 'z' - 'a' + 1;
	char *begin = buf + strlen(prefix);
	char *end = buf + buflen;
	char *p;
	int unit;

	p = end - 1;
	*p = '\0';
	unit = base;
	do {
		if (p == begin)
			return -EINVAL;
		*--p = 'a' + (index % unit);
		index = (index / unit) - 1;
	} while (index >= 0);

	memmove(begin, p, end - p);
	memcpy(buf, prefix, strlen(prefix));

	return 0;
}

static int mtip_block_ioctl(struct block_device *dev,
			    fmode_t mode,
			    unsigned cmd,
			    unsigned long arg)
{
	struct driver_data *dd = dev->bd_disk->private_data;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (!dd)
		return -ENOTTY;

	if (unlikely(test_bit(MTIP_DDF_REMOVE_PENDING_BIT, &dd->dd_flag)))
		return -ENOTTY;

	switch (cmd) {
	case BLKFLSBUF:
		return -ENOTTY;
	default:
		return mtip_hw_ioctl(dd, cmd, arg);
	}
}

#ifdef CONFIG_COMPAT
static int mtip_block_compat_ioctl(struct block_device *dev,
			    fmode_t mode,
			    unsigned cmd,
			    unsigned long arg)
{
	struct driver_data *dd = dev->bd_disk->private_data;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (!dd)
		return -ENOTTY;

	if (unlikely(test_bit(MTIP_DDF_REMOVE_PENDING_BIT, &dd->dd_flag)))
		return -ENOTTY;

	switch (cmd) {
	case BLKFLSBUF:
		return -ENOTTY;
	case HDIO_DRIVE_TASKFILE: {
		struct mtip_compat_ide_task_request_s __user *compat_req_task;
		ide_task_request_t req_task;
		int compat_tasksize, outtotal, ret;

		compat_tasksize =
			sizeof(struct mtip_compat_ide_task_request_s);

		compat_req_task =
			(struct mtip_compat_ide_task_request_s __user *) arg;

		if (copy_from_user(&req_task, (void __user *) arg,
			compat_tasksize - (2 * sizeof(compat_long_t))))
			return -EFAULT;

		if (get_user(req_task.out_size, &compat_req_task->out_size))
			return -EFAULT;

		if (get_user(req_task.in_size, &compat_req_task->in_size))
			return -EFAULT;

		outtotal = sizeof(struct mtip_compat_ide_task_request_s);

		ret = exec_drive_taskfile(dd, (void __user *) arg,
						&req_task, outtotal);

		if (copy_to_user((void __user *) arg, &req_task,
				compat_tasksize -
				(2 * sizeof(compat_long_t))))
			return -EFAULT;

		if (put_user(req_task.out_size, &compat_req_task->out_size))
			return -EFAULT;

		if (put_user(req_task.in_size, &compat_req_task->in_size))
			return -EFAULT;

		return ret;
	}
	default:
		return mtip_hw_ioctl(dd, cmd, arg);
	}
}
#endif

static int mtip_block_getgeo(struct block_device *dev,
				struct hd_geometry *geo)
{
	struct driver_data *dd = dev->bd_disk->private_data;
	sector_t capacity;

	if (!dd)
		return -ENOTTY;

	if (!(mtip_hw_get_capacity(dd, &capacity))) {
		dev_warn(&dd->pdev->dev,
			"Could not get drive capacity.\n");
		return -ENOTTY;
	}

	geo->heads = 224;
	geo->sectors = 56;
	sector_div(capacity, (geo->heads * geo->sectors));
	geo->cylinders = capacity;
	return 0;
}

static const struct block_device_operations mtip_block_ops = {
	.ioctl		= mtip_block_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= mtip_block_compat_ioctl,
#endif
	.getgeo		= mtip_block_getgeo,
	.owner		= THIS_MODULE
};

static void mtip_make_request(struct request_queue *queue, struct bio *bio)
{
	struct driver_data *dd = queue->queuedata;
	struct scatterlist *sg;
	struct bio_vec *bvec;
	int nents = 0;
	int tag = 0;

	if (unlikely(dd->dd_flag & MTIP_DDF_STOP_IO)) {
		if (unlikely(test_bit(MTIP_DDF_REMOVE_PENDING_BIT,
							&dd->dd_flag))) {
			bio_endio(bio, -ENXIO);
			return;
		}
		if (unlikely(test_bit(MTIP_DDF_OVER_TEMP_BIT, &dd->dd_flag))) {
			bio_endio(bio, -ENODATA);
			return;
		}
		if (unlikely(test_bit(MTIP_DDF_WRITE_PROTECT_BIT,
							&dd->dd_flag) &&
				bio_data_dir(bio))) {
			bio_endio(bio, -ENODATA);
			return;
		}
	}

	if (unlikely(!bio_has_data(bio))) {
		blk_queue_flush(queue, 0);
		bio_endio(bio, 0);
		return;
	}

	sg = mtip_hw_get_scatterlist(dd, &tag);
	if (likely(sg != NULL)) {
		blk_queue_bounce(queue, &bio);

		if (unlikely((bio)->bi_vcnt > MTIP_MAX_SG)) {
			dev_warn(&dd->pdev->dev,
				"Maximum number of SGL entries exceeded\n");
			bio_io_error(bio);
			mtip_hw_release_scatterlist(dd, tag);
			return;
		}

		
		bio_for_each_segment(bvec, bio, nents) {
			sg_set_page(&sg[nents],
					bvec->bv_page,
					bvec->bv_len,
					bvec->bv_offset);
		}

		
		mtip_hw_submit_io(dd,
				bio->bi_sector,
				bio_sectors(bio),
				nents,
				tag,
				bio_endio,
				bio,
				bio_data_dir(bio));
	} else
		bio_io_error(bio);
}

static int mtip_block_initialize(struct driver_data *dd)
{
	int rv = 0, wait_for_rebuild = 0;
	sector_t capacity;
	unsigned int index = 0;
	struct kobject *kobj;
	unsigned char thd_name[16];

	if (dd->disk)
		goto skip_create_disk; 

	
	wait_for_rebuild = mtip_hw_init(dd);
	if (wait_for_rebuild < 0) {
		dev_err(&dd->pdev->dev,
			"Protocol layer initialization failed\n");
		rv = -EINVAL;
		goto protocol_init_error;
	}

	dd->disk = alloc_disk(MTIP_MAX_MINORS);
	if (dd->disk  == NULL) {
		dev_err(&dd->pdev->dev,
			"Unable to allocate gendisk structure\n");
		rv = -EINVAL;
		goto alloc_disk_error;
	}

	
	do {
		if (!ida_pre_get(&rssd_index_ida, GFP_KERNEL))
			goto ida_get_error;

		spin_lock(&rssd_index_lock);
		rv = ida_get_new(&rssd_index_ida, &index);
		spin_unlock(&rssd_index_lock);
	} while (rv == -EAGAIN);

	if (rv)
		goto ida_get_error;

	rv = rssd_disk_name_format("rssd",
				index,
				dd->disk->disk_name,
				DISK_NAME_LEN);
	if (rv)
		goto disk_index_error;

	dd->disk->driverfs_dev	= &dd->pdev->dev;
	dd->disk->major		= dd->major;
	dd->disk->first_minor	= dd->instance * MTIP_MAX_MINORS;
	dd->disk->fops		= &mtip_block_ops;
	dd->disk->private_data	= dd;
	dd->index		= index;

	if (wait_for_rebuild == MTIP_FTL_REBUILD_MAGIC)
		goto start_service_thread;

skip_create_disk:
	
	dd->queue = blk_alloc_queue(GFP_KERNEL);
	if (dd->queue == NULL) {
		dev_err(&dd->pdev->dev,
			"Unable to allocate request queue\n");
		rv = -ENOMEM;
		goto block_queue_alloc_init_error;
	}

	
	blk_queue_make_request(dd->queue, mtip_make_request);

	dd->disk->queue		= dd->queue;
	dd->queue->queuedata	= dd;

	
	set_bit(QUEUE_FLAG_NONROT, &dd->queue->queue_flags);
	blk_queue_max_segments(dd->queue, MTIP_MAX_SG);
	blk_queue_physical_block_size(dd->queue, 4096);
	blk_queue_io_min(dd->queue, 4096);
	blk_queue_flush(dd->queue, 0);

	
	if (!(mtip_hw_get_capacity(dd, &capacity))) {
		dev_warn(&dd->pdev->dev,
			"Could not read drive capacity\n");
		rv = -EIO;
		goto read_capacity_error;
	}
	set_capacity(dd->disk, capacity);

	
	add_disk(dd->disk);

	kobj = kobject_get(&disk_to_dev(dd->disk)->kobj);
	if (kobj) {
		mtip_hw_sysfs_init(dd, kobj);
		kobject_put(kobj);
	}

	if (dd->mtip_svc_handler) {
		set_bit(MTIP_DDF_INIT_DONE_BIT, &dd->dd_flag);
		return rv; 
	}

start_service_thread:
	sprintf(thd_name, "mtip_svc_thd_%02d", index);

	dd->mtip_svc_handler = kthread_run(mtip_service_thread,
						dd, thd_name);

	if (IS_ERR(dd->mtip_svc_handler)) {
		dev_err(&dd->pdev->dev, "service thread failed to start\n");
		dd->mtip_svc_handler = NULL;
		rv = -EFAULT;
		goto kthread_run_error;
	}

	if (wait_for_rebuild == MTIP_FTL_REBUILD_MAGIC)
		rv = wait_for_rebuild;

	return rv;

kthread_run_error:
	
	del_gendisk(dd->disk);

read_capacity_error:
	blk_cleanup_queue(dd->queue);

block_queue_alloc_init_error:
disk_index_error:
	spin_lock(&rssd_index_lock);
	ida_remove(&rssd_index_ida, index);
	spin_unlock(&rssd_index_lock);

ida_get_error:
	put_disk(dd->disk);

alloc_disk_error:
	mtip_hw_exit(dd); 

protocol_init_error:
	return rv;
}

static int mtip_block_remove(struct driver_data *dd)
{
	struct kobject *kobj;

	if (dd->mtip_svc_handler) {
		set_bit(MTIP_PF_SVC_THD_STOP_BIT, &dd->port->flags);
		wake_up_interruptible(&dd->port->svc_wait);
		kthread_stop(dd->mtip_svc_handler);
	}

	
	if (test_bit(MTIP_DDF_INIT_DONE_BIT, &dd->dd_flag)) {
		kobj = kobject_get(&disk_to_dev(dd->disk)->kobj);
		if (kobj) {
			mtip_hw_sysfs_exit(dd, kobj);
			kobject_put(kobj);
		}
	}

	del_gendisk(dd->disk);

	spin_lock(&rssd_index_lock);
	ida_remove(&rssd_index_ida, dd->index);
	spin_unlock(&rssd_index_lock);

	blk_cleanup_queue(dd->queue);
	dd->disk  = NULL;
	dd->queue = NULL;

	
	mtip_hw_exit(dd);

	return 0;
}

static int mtip_block_shutdown(struct driver_data *dd)
{
	dev_info(&dd->pdev->dev,
		"Shutting down %s ...\n", dd->disk->disk_name);

	
	del_gendisk(dd->disk);

	spin_lock(&rssd_index_lock);
	ida_remove(&rssd_index_ida, dd->index);
	spin_unlock(&rssd_index_lock);

	blk_cleanup_queue(dd->queue);
	dd->disk  = NULL;
	dd->queue = NULL;

	mtip_hw_shutdown(dd);
	return 0;
}

static int mtip_block_suspend(struct driver_data *dd)
{
	dev_info(&dd->pdev->dev,
		"Suspending %s ...\n", dd->disk->disk_name);
	mtip_hw_suspend(dd);
	return 0;
}

static int mtip_block_resume(struct driver_data *dd)
{
	dev_info(&dd->pdev->dev, "Resuming %s ...\n",
		dd->disk->disk_name);
	mtip_hw_resume(dd);
	return 0;
}

static int mtip_pci_probe(struct pci_dev *pdev,
			const struct pci_device_id *ent)
{
	int rv = 0;
	struct driver_data *dd = NULL;

	
	dd = kzalloc(sizeof(struct driver_data), GFP_KERNEL);
	if (dd == NULL) {
		dev_err(&pdev->dev,
			"Unable to allocate memory for driver data\n");
		return -ENOMEM;
	}

	
	pci_set_drvdata(pdev, dd);

	rv = pcim_enable_device(pdev);
	if (rv < 0) {
		dev_err(&pdev->dev, "Unable to enable device\n");
		goto iomap_err;
	}

	
	rv = pcim_iomap_regions(pdev, 1 << MTIP_ABAR, MTIP_DRV_NAME);
	if (rv < 0) {
		dev_err(&pdev->dev, "Unable to map regions\n");
		goto iomap_err;
	}

	if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(64))) {
		rv = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));

		if (rv) {
			rv = pci_set_consistent_dma_mask(pdev,
						DMA_BIT_MASK(32));
			if (rv) {
				dev_warn(&pdev->dev,
					"64-bit DMA enable failed\n");
				goto setmask_err;
			}
		}
	}

	pci_set_master(pdev);

	if (pci_enable_msi(pdev)) {
		dev_warn(&pdev->dev,
			"Unable to enable MSI interrupt.\n");
		goto block_initialize_err;
	}

	
	dd->major	= mtip_major;
	dd->instance	= instance;
	dd->pdev	= pdev;

	
	rv = mtip_block_initialize(dd);
	if (rv < 0) {
		dev_err(&pdev->dev,
			"Unable to initialize block layer\n");
		goto block_initialize_err;
	}

	instance++;
	if (rv != MTIP_FTL_REBUILD_MAGIC)
		set_bit(MTIP_DDF_INIT_DONE_BIT, &dd->dd_flag);
	goto done;

block_initialize_err:
	pci_disable_msi(pdev);

setmask_err:
	pcim_iounmap_regions(pdev, 1 << MTIP_ABAR);

iomap_err:
	kfree(dd);
	pci_set_drvdata(pdev, NULL);
	return rv;
done:
	return rv;
}

static void mtip_pci_remove(struct pci_dev *pdev)
{
	struct driver_data *dd = pci_get_drvdata(pdev);
	int counter = 0;

	set_bit(MTIP_DDF_REMOVE_PENDING_BIT, &dd->dd_flag);

	if (mtip_check_surprise_removal(pdev)) {
		while (!test_bit(MTIP_DDF_CLEANUP_BIT, &dd->dd_flag)) {
			counter++;
			msleep(20);
			if (counter == 10) {
				
				mtip_command_cleanup(dd);
				break;
			}
		}
	}

	
	mtip_block_remove(dd);

	pci_disable_msi(pdev);

	kfree(dd);
	pcim_iounmap_regions(pdev, 1 << MTIP_ABAR);
}

static int mtip_pci_suspend(struct pci_dev *pdev, pm_message_t mesg)
{
	int rv = 0;
	struct driver_data *dd = pci_get_drvdata(pdev);

	if (!dd) {
		dev_err(&pdev->dev,
			"Driver private datastructure is NULL\n");
		return -EFAULT;
	}

	set_bit(MTIP_DDF_RESUME_BIT, &dd->dd_flag);

	
	rv = mtip_block_suspend(dd);
	if (rv < 0) {
		dev_err(&pdev->dev,
			"Failed to suspend controller\n");
		return rv;
	}

	pci_save_state(pdev);
	pci_disable_device(pdev);

	
	pci_set_power_state(pdev, PCI_D3hot);

	return rv;
}

static int mtip_pci_resume(struct pci_dev *pdev)
{
	int rv = 0;
	struct driver_data *dd;

	dd = pci_get_drvdata(pdev);
	if (!dd) {
		dev_err(&pdev->dev,
			"Driver private datastructure is NULL\n");
		return -EFAULT;
	}

	
	pci_set_power_state(pdev, PCI_D0);

	
	pci_restore_state(pdev);

	
	rv = pcim_enable_device(pdev);
	if (rv < 0) {
		dev_err(&pdev->dev,
			"Failed to enable card during resume\n");
		goto err;
	}
	pci_set_master(pdev);

	rv = mtip_block_resume(dd);
	if (rv < 0)
		dev_err(&pdev->dev, "Unable to resume\n");

err:
	clear_bit(MTIP_DDF_RESUME_BIT, &dd->dd_flag);

	return rv;
}

static void mtip_pci_shutdown(struct pci_dev *pdev)
{
	struct driver_data *dd = pci_get_drvdata(pdev);
	if (dd)
		mtip_block_shutdown(dd);
}

static DEFINE_PCI_DEVICE_TABLE(mtip_pci_tbl) = {
	{  PCI_DEVICE(PCI_VENDOR_ID_MICRON, P320_DEVICE_ID) },
	{ 0 }
};

static struct pci_driver mtip_pci_driver = {
	.name			= MTIP_DRV_NAME,
	.id_table		= mtip_pci_tbl,
	.probe			= mtip_pci_probe,
	.remove			= mtip_pci_remove,
	.suspend		= mtip_pci_suspend,
	.resume			= mtip_pci_resume,
	.shutdown		= mtip_pci_shutdown,
};

MODULE_DEVICE_TABLE(pci, mtip_pci_tbl);

static int __init mtip_init(void)
{
	int error;

	printk(KERN_INFO MTIP_DRV_NAME " Version " MTIP_DRV_VERSION "\n");

	
	error = register_blkdev(0, MTIP_DRV_NAME);
	if (error <= 0) {
		printk(KERN_ERR "Unable to register block device (%d)\n",
		error);
		return -EBUSY;
	}
	mtip_major = error;

	
	error = pci_register_driver(&mtip_pci_driver);
	if (error)
		unregister_blkdev(mtip_major, MTIP_DRV_NAME);

	return error;
}

static void __exit mtip_exit(void)
{
	
	unregister_blkdev(mtip_major, MTIP_DRV_NAME);

	
	pci_unregister_driver(&mtip_pci_driver);
}

MODULE_AUTHOR("Micron Technology, Inc");
MODULE_DESCRIPTION("Micron RealSSD PCIe Block Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(MTIP_DRV_VERSION);

module_init(mtip_init);
module_exit(mtip_exit);
