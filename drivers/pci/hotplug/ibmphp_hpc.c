/*
 * IBM Hot Plug Controller Driver
 *
 * Written By: Jyoti Shah, IBM Corporation
 *
 * Copyright (C) 2001-2003 IBM Corp.
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Send feedback to <gregkh@us.ibm.com>
 *                  <jshah@us.ibm.com>
 *
 */

#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include "ibmphp.h"

static int to_debug = 0;
#define debug_polling(fmt, arg...)	do { if (to_debug) debug (fmt, arg); } while (0)

#define CMD_COMPLETE_TOUT_SEC	60	
#define HPC_CTLR_WORKING_TOUT	60	
#define HPC_GETACCESS_TIMEOUT	60	
#define POLL_INTERVAL_SEC	2	
#define POLL_LATCH_CNT		5	

#define WPG_I2CMBUFL_OFFSET	0x08	
#define WPG_I2CMOSUP_OFFSET	0x10	
#define WPG_I2CMCNTL_OFFSET	0x20	
#define WPG_I2CPARM_OFFSET	0x40	
#define WPG_I2CSTAT_OFFSET	0x70	

#define WPG_I2C_AND		0x1000	
#define WPG_I2C_OR		0x2000	

#define WPG_READATADDR_MASK	0x00010000	
#define WPG_WRITEATADDR_MASK	0x40010000	
#define WPG_READDIRECT_MASK	0x10010000
#define WPG_WRITEDIRECT_MASK	0x60010000


#define WPG_I2CMCNTL_STARTOP_MASK	0x00000002	

#define WPG_I2C_IOREMAP_SIZE	0x2044	

#define WPG_1ST_SLOT_INDEX	0x01	
#define WPG_CTLR_INDEX		0x0F	
#define WPG_1ST_EXTSLOT_INDEX	0x10	
#define WPG_1ST_BUS_INDEX	0x1F	

#define HPC_I2CSTATUS_CHECK(s)	((u8)((s & 0x00000A76) ? 0 : 1))

static struct mutex sem_hpcaccess;	
static struct semaphore semOperations;	
					
static struct semaphore sem_exit;	
static struct task_struct *ibmphp_poll_thread;
static u8 i2c_ctrl_read (struct controller *, void __iomem *, u8);
static u8 i2c_ctrl_write (struct controller *, void __iomem *, u8, u8);
static u8 hpc_writecmdtoindex (u8, u8);
static u8 hpc_readcmdtoindex (u8, u8);
static void get_hpc_access (void);
static void free_hpc_access (void);
static int poll_hpc(void *data);
static int process_changeinstatus (struct slot *, struct slot *);
static int process_changeinlatch (u8, u8, struct controller *);
static int hpc_wait_ctlr_notworking (int, struct controller *, void __iomem *, u8 *);


void __init ibmphp_hpc_initvars (void)
{
	debug ("%s - Entry\n", __func__);

	mutex_init(&sem_hpcaccess);
	sema_init(&semOperations, 1);
	sema_init(&sem_exit, 0);
	to_debug = 0;

	debug ("%s - Exit\n", __func__);
}

static u8 i2c_ctrl_read (struct controller *ctlr_ptr, void __iomem *WPGBbar, u8 index)
{
	u8 status;
	int i;
	void __iomem *wpg_addr;	
	unsigned long wpg_data;	
	unsigned long ultemp;
	unsigned long data;	

	debug_polling ("%s - Entry WPGBbar[%p] index[%x] \n", __func__, WPGBbar, index);

	
	
	
	
	if (ctlr_ptr->ctlr_type == 0x02) {
		data = WPG_READATADDR_MASK;
		
		ultemp = (unsigned long)ctlr_ptr->u.wpeg_ctlr.i2c_addr;
		ultemp = ultemp >> 1;
		data |= (ultemp << 8);

		
		data |= (unsigned long)index;
	} else if (ctlr_ptr->ctlr_type == 0x04) {
		data = WPG_READDIRECT_MASK;

		
		ultemp = (unsigned long)index;
		ultemp = ultemp << 8;
		data |= ultemp;
	} else {
		err ("this controller type is not supported \n");
		return HPC_ERROR;
	}

	wpg_data = swab32 (data);	
	wpg_addr = WPGBbar + WPG_I2CMOSUP_OFFSET;
	writel (wpg_data, wpg_addr);

	
	
	data = 0x00000000;
	wpg_data = swab32 (data);
	wpg_addr = WPGBbar + WPG_I2CMBUFL_OFFSET;
	writel (wpg_data, wpg_addr);

	
	
	
	data = WPG_I2CMCNTL_STARTOP_MASK;
	wpg_data = swab32 (data);
	wpg_addr = WPGBbar + WPG_I2CMCNTL_OFFSET + WPG_I2C_OR;
	writel (wpg_data, wpg_addr);

	
	
	i = CMD_COMPLETE_TOUT_SEC;
	while (i) {
		msleep(10);
		wpg_addr = WPGBbar + WPG_I2CMCNTL_OFFSET;
		wpg_data = readl (wpg_addr);
		data = swab32 (wpg_data);
		if (!(data & WPG_I2CMCNTL_STARTOP_MASK))
			break;
		i--;
	}
	if (i == 0) {
		debug ("%s - Error : WPG timeout\n", __func__);
		return HPC_ERROR;
	}
	
	
	i = CMD_COMPLETE_TOUT_SEC;
	while (i) {
		msleep(10);
		wpg_addr = WPGBbar + WPG_I2CSTAT_OFFSET;
		wpg_data = readl (wpg_addr);
		data = swab32 (wpg_data);
		if (HPC_I2CSTATUS_CHECK (data))
			break;
		i--;
	}
	if (i == 0) {
		debug ("ctrl_read - Exit Error:I2C timeout\n");
		return HPC_ERROR;
	}

	
	
	wpg_addr = WPGBbar + WPG_I2CMBUFL_OFFSET;
	wpg_data = readl (wpg_addr);
	data = swab32 (wpg_data);

	status = (u8) data;

	debug_polling ("%s - Exit index[%x] status[%x]\n", __func__, index, status);

	return (status);
}

static u8 i2c_ctrl_write (struct controller *ctlr_ptr, void __iomem *WPGBbar, u8 index, u8 cmd)
{
	u8 rc;
	void __iomem *wpg_addr;	
	unsigned long wpg_data;	
	unsigned long ultemp;
	unsigned long data;	
	int i;

	debug_polling ("%s - Entry WPGBbar[%p] index[%x] cmd[%x]\n", __func__, WPGBbar, index, cmd);

	rc = 0;
	
	
	
	
	data = 0x00000000;

	if (ctlr_ptr->ctlr_type == 0x02) {
		data = WPG_WRITEATADDR_MASK;
		
		ultemp = (unsigned long)ctlr_ptr->u.wpeg_ctlr.i2c_addr;
		ultemp = ultemp >> 1;
		data |= (ultemp << 8);

		
		data |= (unsigned long)index;
	} else if (ctlr_ptr->ctlr_type == 0x04) {
		data = WPG_WRITEDIRECT_MASK;

		
		ultemp = (unsigned long)index;
		ultemp = ultemp << 8;
		data |= ultemp;
	} else {
		err ("this controller type is not supported \n");
		return HPC_ERROR;
	}

	wpg_data = swab32 (data);	
	wpg_addr = WPGBbar + WPG_I2CMOSUP_OFFSET;
	writel (wpg_data, wpg_addr);

	
	
	data = 0x00000000 | (unsigned long)cmd;
	wpg_data = swab32 (data);
	wpg_addr = WPGBbar + WPG_I2CMBUFL_OFFSET;
	writel (wpg_data, wpg_addr);

	
	
	
	data = WPG_I2CMCNTL_STARTOP_MASK;
	wpg_data = swab32 (data);
	wpg_addr = WPGBbar + WPG_I2CMCNTL_OFFSET + WPG_I2C_OR;
	writel (wpg_data, wpg_addr);

	
	
	i = CMD_COMPLETE_TOUT_SEC;
	while (i) {
		msleep(10);
		wpg_addr = WPGBbar + WPG_I2CMCNTL_OFFSET;
		wpg_data = readl (wpg_addr);
		data = swab32 (wpg_data);
		if (!(data & WPG_I2CMCNTL_STARTOP_MASK))
			break;
		i--;
	}
	if (i == 0) {
		debug ("%s - Exit Error:WPG timeout\n", __func__);
		rc = HPC_ERROR;
	}

	
	
	i = CMD_COMPLETE_TOUT_SEC;
	while (i) {
		msleep(10);
		wpg_addr = WPGBbar + WPG_I2CSTAT_OFFSET;
		wpg_data = readl (wpg_addr);
		data = swab32 (wpg_data);
		if (HPC_I2CSTATUS_CHECK (data))
			break;
		i--;
	}
	if (i == 0) {
		debug ("ctrl_read - Error : I2C timeout\n");
		rc = HPC_ERROR;
	}

	debug_polling ("%s Exit rc[%x]\n", __func__, rc);
	return (rc);
}

static u8 isa_ctrl_read (struct controller *ctlr_ptr, u8 offset)
{
	u16 start_address;
	u16 end_address;
	u8 data;

	start_address = ctlr_ptr->u.isa_ctlr.io_start;
	end_address = ctlr_ptr->u.isa_ctlr.io_end;
	data = inb (start_address + offset);
	return data;
}

static void isa_ctrl_write (struct controller *ctlr_ptr, u8 offset, u8 data)
{
	u16 start_address;
	u16 port_address;
	
	start_address = ctlr_ptr->u.isa_ctlr.io_start;
	port_address = start_address + (u16) offset;
	outb (data, port_address);
}

static u8 pci_ctrl_read (struct controller *ctrl, u8 offset)
{
	u8 data = 0x00;
	debug ("inside pci_ctrl_read\n");
	if (ctrl->ctrl_dev)
		pci_read_config_byte (ctrl->ctrl_dev, HPC_PCI_OFFSET + offset, &data);
	return data;
}

static u8 pci_ctrl_write (struct controller *ctrl, u8 offset, u8 data)
{
	u8 rc = -ENODEV;
	debug ("inside pci_ctrl_write\n");
	if (ctrl->ctrl_dev) {
		pci_write_config_byte (ctrl->ctrl_dev, HPC_PCI_OFFSET + offset, data);
		rc = 0;
	}
	return rc;
}

static u8 ctrl_read (struct controller *ctlr, void __iomem *base, u8 offset)
{
	u8 rc;
	switch (ctlr->ctlr_type) {
	case 0:
		rc = isa_ctrl_read (ctlr, offset);
		break;
	case 1:
		rc = pci_ctrl_read (ctlr, offset);
		break;
	case 2:
	case 4:
		rc = i2c_ctrl_read (ctlr, base, offset);
		break;
	default:
		return -ENODEV;
	}
	return rc;
}

static u8 ctrl_write (struct controller *ctlr, void __iomem *base, u8 offset, u8 data)
{
	u8 rc = 0;
	switch (ctlr->ctlr_type) {
	case 0:
		isa_ctrl_write(ctlr, offset, data);
		break;
	case 1:
		rc = pci_ctrl_write (ctlr, offset, data);
		break;
	case 2:
	case 4:
		rc = i2c_ctrl_write(ctlr, base, offset, data);
		break;
	default:
		return -ENODEV;
	}
	return rc;
}
static u8 hpc_writecmdtoindex (u8 cmd, u8 index)
{
	u8 rc;

	switch (cmd) {
	case HPC_CTLR_ENABLEIRQ:	
	case HPC_CTLR_CLEARIRQ:	
	case HPC_CTLR_RESET:	
	case HPC_CTLR_IRQSTEER:	
	case HPC_CTLR_DISABLEIRQ:	
	case HPC_ALLSLOT_ON:	
	case HPC_ALLSLOT_OFF:	
		rc = 0x0F;
		break;

	case HPC_SLOT_OFF:	
	case HPC_SLOT_ON:	
	case HPC_SLOT_ATTNOFF:	
	case HPC_SLOT_ATTNON:	
	case HPC_SLOT_BLINKLED:	
		rc = index;
		break;

	case HPC_BUS_33CONVMODE:
	case HPC_BUS_66CONVMODE:
	case HPC_BUS_66PCIXMODE:
	case HPC_BUS_100PCIXMODE:
	case HPC_BUS_133PCIXMODE:
		rc = index + WPG_1ST_BUS_INDEX - 1;
		break;

	default:
		err ("hpc_writecmdtoindex - Error invalid cmd[%x]\n", cmd);
		rc = HPC_ERROR;
	}

	return rc;
}

static u8 hpc_readcmdtoindex (u8 cmd, u8 index)
{
	u8 rc;

	switch (cmd) {
	case READ_CTLRSTATUS:
		rc = 0x0F;
		break;
	case READ_SLOTSTATUS:
	case READ_ALLSTAT:
		rc = index;
		break;
	case READ_EXTSLOTSTATUS:
		rc = index + WPG_1ST_EXTSLOT_INDEX;
		break;
	case READ_BUSSTATUS:
		rc = index + WPG_1ST_BUS_INDEX - 1;
		break;
	case READ_SLOTLATCHLOWREG:
		rc = 0x28;
		break;
	case READ_REVLEVEL:
		rc = 0x25;
		break;
	case READ_HPCOPTIONS:
		rc = 0x27;
		break;
	default:
		rc = HPC_ERROR;
	}
	return rc;
}

int ibmphp_hpc_readslot (struct slot * pslot, u8 cmd, u8 * pstatus)
{
	void __iomem *wpg_bbar = NULL;
	struct controller *ctlr_ptr;
	struct list_head *pslotlist;
	u8 index, status;
	int rc = 0;
	int busindex;

	debug_polling ("%s - Entry pslot[%p] cmd[%x] pstatus[%p]\n", __func__, pslot, cmd, pstatus);

	if ((pslot == NULL)
	    || ((pstatus == NULL) && (cmd != READ_ALLSTAT) && (cmd != READ_BUSSTATUS))) {
		rc = -EINVAL;
		err ("%s - Error invalid pointer, rc[%d]\n", __func__, rc);
		return rc;
	}

	if (cmd == READ_BUSSTATUS) {
		busindex = ibmphp_get_bus_index (pslot->bus);
		if (busindex < 0) {
			rc = -EINVAL;
			err ("%s - Exit Error:invalid bus, rc[%d]\n", __func__, rc);
			return rc;
		} else
			index = (u8) busindex;
	} else
		index = pslot->ctlr_index;

	index = hpc_readcmdtoindex (cmd, index);

	if (index == HPC_ERROR) {
		rc = -EINVAL;
		err ("%s - Exit Error:invalid index, rc[%d]\n", __func__, rc);
		return rc;
	}

	ctlr_ptr = pslot->ctrl;

	get_hpc_access ();

	
	
	
	if ((ctlr_ptr->ctlr_type == 2) || (ctlr_ptr->ctlr_type == 4))
		wpg_bbar = ioremap (ctlr_ptr->u.wpeg_ctlr.wpegbbar, WPG_I2C_IOREMAP_SIZE);

	
	
	
	rc = hpc_wait_ctlr_notworking (HPC_CTLR_WORKING_TOUT, ctlr_ptr, wpg_bbar, &status);
	if (!rc) {
		switch (cmd) {
		case READ_ALLSTAT:
			
			pslot->ctrl->status = status;
			pslot->status = ctrl_read (ctlr_ptr, wpg_bbar, index);
			rc = hpc_wait_ctlr_notworking (HPC_CTLR_WORKING_TOUT, ctlr_ptr, wpg_bbar,
						       &status);
			if (!rc)
				pslot->ext_status = ctrl_read (ctlr_ptr, wpg_bbar, index + WPG_1ST_EXTSLOT_INDEX);

			break;

		case READ_SLOTSTATUS:
			
			*pstatus = ctrl_read (ctlr_ptr, wpg_bbar, index);
			break;

		case READ_EXTSLOTSTATUS:
			
			*pstatus = ctrl_read (ctlr_ptr, wpg_bbar, index);
			break;

		case READ_CTLRSTATUS:
			
			*pstatus = status;
			break;

		case READ_BUSSTATUS:
			pslot->busstatus = ctrl_read (ctlr_ptr, wpg_bbar, index);
			break;
		case READ_REVLEVEL:
			*pstatus = ctrl_read (ctlr_ptr, wpg_bbar, index);
			break;
		case READ_HPCOPTIONS:
			*pstatus = ctrl_read (ctlr_ptr, wpg_bbar, index);
			break;
		case READ_SLOTLATCHLOWREG:
			
			*pstatus = ctrl_read (ctlr_ptr, wpg_bbar, index);
			break;

			
		case READ_ALLSLOT:
			list_for_each (pslotlist, &ibmphp_slot_head) {
				pslot = list_entry (pslotlist, struct slot, ibm_slot_list);
				index = pslot->ctlr_index;
				rc = hpc_wait_ctlr_notworking (HPC_CTLR_WORKING_TOUT, ctlr_ptr,
								wpg_bbar, &status);
				if (!rc) {
					pslot->status = ctrl_read (ctlr_ptr, wpg_bbar, index);
					rc = hpc_wait_ctlr_notworking (HPC_CTLR_WORKING_TOUT,
									ctlr_ptr, wpg_bbar, &status);
					if (!rc)
						pslot->ext_status =
						    ctrl_read (ctlr_ptr, wpg_bbar,
								index + WPG_1ST_EXTSLOT_INDEX);
				} else {
					err ("%s - Error ctrl_read failed\n", __func__);
					rc = -EINVAL;
					break;
				}
			}
			break;
		default:
			rc = -EINVAL;
			break;
		}
	}
	
	
	
	
	
	if ((ctlr_ptr->ctlr_type == 2) || (ctlr_ptr->ctlr_type == 4))
		iounmap (wpg_bbar);
	
	free_hpc_access ();

	debug_polling ("%s - Exit rc[%d]\n", __func__, rc);
	return rc;
}

int ibmphp_hpc_writeslot (struct slot * pslot, u8 cmd)
{
	void __iomem *wpg_bbar = NULL;
	struct controller *ctlr_ptr;
	u8 index, status;
	int busindex;
	u8 done;
	int rc = 0;
	int timeout;

	debug_polling ("%s - Entry pslot[%p] cmd[%x]\n", __func__, pslot, cmd);
	if (pslot == NULL) {
		rc = -EINVAL;
		err ("%s - Error Exit rc[%d]\n", __func__, rc);
		return rc;
	}

	if ((cmd == HPC_BUS_33CONVMODE) || (cmd == HPC_BUS_66CONVMODE) ||
		(cmd == HPC_BUS_66PCIXMODE) || (cmd == HPC_BUS_100PCIXMODE) ||
		(cmd == HPC_BUS_133PCIXMODE)) {
		busindex = ibmphp_get_bus_index (pslot->bus);
		if (busindex < 0) {
			rc = -EINVAL;
			err ("%s - Exit Error:invalid bus, rc[%d]\n", __func__, rc);
			return rc;
		} else
			index = (u8) busindex;
	} else
		index = pslot->ctlr_index;

	index = hpc_writecmdtoindex (cmd, index);

	if (index == HPC_ERROR) {
		rc = -EINVAL;
		err ("%s - Error Exit rc[%d]\n", __func__, rc);
		return rc;
	}

	ctlr_ptr = pslot->ctrl;

	get_hpc_access ();

	
	
	
	if ((ctlr_ptr->ctlr_type == 2) || (ctlr_ptr->ctlr_type == 4)) {
		wpg_bbar = ioremap (ctlr_ptr->u.wpeg_ctlr.wpegbbar, WPG_I2C_IOREMAP_SIZE);

		debug ("%s - ctlr id[%x] physical[%lx] logical[%lx] i2c[%x]\n", __func__,
		ctlr_ptr->ctlr_id, (ulong) (ctlr_ptr->u.wpeg_ctlr.wpegbbar), (ulong) wpg_bbar,
		ctlr_ptr->u.wpeg_ctlr.i2c_addr);
	}
	
	
	
	rc = hpc_wait_ctlr_notworking (HPC_CTLR_WORKING_TOUT, ctlr_ptr, wpg_bbar, &status);
	if (!rc) {

		ctrl_write (ctlr_ptr, wpg_bbar, index, cmd);

		
		
		
		timeout = CMD_COMPLETE_TOUT_SEC;
		done = 0;
		while (!done) {
			rc = hpc_wait_ctlr_notworking (HPC_CTLR_WORKING_TOUT, ctlr_ptr, wpg_bbar,
							&status);
			if (!rc) {
				if (NEEDTOCHECK_CMDSTATUS (cmd)) {
					if (CTLR_FINISHED (status) == HPC_CTLR_FINISHED_YES)
						done = 1;
				} else
					done = 1;
			}
			if (!done) {
				msleep(1000);
				if (timeout < 1) {
					done = 1;
					err ("%s - Error command complete timeout\n", __func__);
					rc = -EFAULT;
				} else
					timeout--;
			}
		}
		ctlr_ptr->status = status;
	}
	

	
	if ((ctlr_ptr->ctlr_type == 2) || (ctlr_ptr->ctlr_type == 4))
		iounmap (wpg_bbar);
	free_hpc_access ();

	debug_polling ("%s - Exit rc[%d]\n", __func__, rc);
	return rc;
}

static void get_hpc_access (void)
{
	mutex_lock(&sem_hpcaccess);
}

void free_hpc_access (void)
{
	mutex_unlock(&sem_hpcaccess);
}

void ibmphp_lock_operations (void)
{
	down (&semOperations);
	to_debug = 1;
}

void ibmphp_unlock_operations (void)
{
	debug ("%s - Entry\n", __func__);
	up (&semOperations);
	to_debug = 0;
	debug ("%s - Exit\n", __func__);
}

#define POLL_LATCH_REGISTER	0
#define POLL_SLOTS		1
#define POLL_SLEEP		2
static int poll_hpc(void *data)
{
	struct slot myslot;
	struct slot *pslot = NULL;
	struct list_head *pslotlist;
	int rc;
	int poll_state = POLL_LATCH_REGISTER;
	u8 oldlatchlow = 0x00;
	u8 curlatchlow = 0x00;
	int poll_count = 0;
	u8 ctrl_count = 0x00;

	debug ("%s - Entry\n", __func__);

	while (!kthread_should_stop()) {
		
		down (&semOperations);

		switch (poll_state) {
		case POLL_LATCH_REGISTER: 
			oldlatchlow = curlatchlow;
			ctrl_count = 0x00;
			list_for_each (pslotlist, &ibmphp_slot_head) {
				if (ctrl_count >= ibmphp_get_total_controllers())
					break;
				pslot = list_entry (pslotlist, struct slot, ibm_slot_list);
				if (pslot->ctrl->ctlr_relative_id == ctrl_count) {
					ctrl_count++;
					if (READ_SLOT_LATCH (pslot->ctrl)) {
						rc = ibmphp_hpc_readslot (pslot,
									  READ_SLOTLATCHLOWREG,
									  &curlatchlow);
						if (oldlatchlow != curlatchlow)
							process_changeinlatch (oldlatchlow,
									       curlatchlow,
									       pslot->ctrl);
					}
				}
			}
			++poll_count;
			poll_state = POLL_SLEEP;
			break;
		case POLL_SLOTS:
			list_for_each (pslotlist, &ibmphp_slot_head) {
				pslot = list_entry (pslotlist, struct slot, ibm_slot_list);
				
				memcpy ((void *) &myslot, (void *) pslot,
					sizeof (struct slot));
				rc = ibmphp_hpc_readslot (pslot, READ_ALLSTAT, NULL);
				if ((myslot.status != pslot->status)
				    || (myslot.ext_status != pslot->ext_status))
					process_changeinstatus (pslot, &myslot);
			}
			ctrl_count = 0x00;
			list_for_each (pslotlist, &ibmphp_slot_head) {
				if (ctrl_count >= ibmphp_get_total_controllers())
					break;
				pslot = list_entry (pslotlist, struct slot, ibm_slot_list);
				if (pslot->ctrl->ctlr_relative_id == ctrl_count) {
					ctrl_count++;
					if (READ_SLOT_LATCH (pslot->ctrl))
						rc = ibmphp_hpc_readslot (pslot,
									  READ_SLOTLATCHLOWREG,
									  &curlatchlow);
				}
			}
			++poll_count;
			poll_state = POLL_SLEEP;
			break;
		case POLL_SLEEP:
			
			up (&semOperations);
			msleep(POLL_INTERVAL_SEC * 1000);

			if (kthread_should_stop())
				goto out_sleep;
			
			down (&semOperations);
			
			if (poll_count >= POLL_LATCH_CNT) {
				poll_count = 0;
				poll_state = POLL_SLOTS;
			} else
				poll_state = POLL_LATCH_REGISTER;
			break;
		}	
		
		up (&semOperations);
		
out_sleep:
		msleep(100);
	}
	up (&sem_exit);
	debug ("%s - Exit\n", __func__);
	return 0;
}


static int process_changeinstatus (struct slot *pslot, struct slot *poldslot)
{
	u8 status;
	int rc = 0;
	u8 disable = 0;
	u8 update = 0;

	debug ("process_changeinstatus - Entry pslot[%p], poldslot[%p]\n", pslot, poldslot);

	
	if ((pslot->status & 0x01) != (poldslot->status & 0x01))
		update = 1;

	
	

	
	if ((pslot->status & 0x04) != (poldslot->status & 0x04))
		update = 1;

	
	
	if (((pslot->status & 0x08) != (poldslot->status & 0x08))
		|| ((pslot->status & 0x10) != (poldslot->status & 0x10)))
		update = 1;

	
	if ((pslot->status & 0x20) != (poldslot->status & 0x20))
		
		if ((poldslot->status & 0x20) && (SLOT_CONNECT (poldslot->status) == HPC_SLOT_CONNECTED) && (SLOT_PRESENT (poldslot->status))) 
			disable = 1;

	
	

	
	if ((pslot->status & 0x80) != (poldslot->status & 0x80)) {
		update = 1;
		
		if (pslot->status & 0x80) {
			if (SLOT_PWRGD (pslot->status)) {
				
				
				msleep(1000);
				rc = ibmphp_hpc_readslot (pslot, READ_SLOTSTATUS, &status);
				if (SLOT_PWRGD (status))
					update = 1;
				else	
					pslot->status &= ~HPC_SLOT_POWER;
			}
		}
		
		else if ((SLOT_PWRGD (poldslot->status) == HPC_SLOT_PWRGD_GOOD)
			&& (SLOT_CONNECT (poldslot->status) == HPC_SLOT_CONNECTED) && (SLOT_PRESENT (poldslot->status))) {
			disable = 1;
		}
		
	}
	
	if ((pslot->ext_status & 0x08) != (poldslot->ext_status & 0x08))
		update = 1;

	if (disable) {
		debug ("process_changeinstatus - disable slot\n");
		pslot->flag = 0;
		rc = ibmphp_do_disable_slot (pslot);
	}

	if (update || disable) {
		ibmphp_update_slot_info (pslot);
	}

	debug ("%s - Exit rc[%d] disable[%x] update[%x]\n", __func__, rc, disable, update);

	return rc;
}

static int process_changeinlatch (u8 old, u8 new, struct controller *ctrl)
{
	struct slot myslot, *pslot;
	u8 i;
	u8 mask;
	int rc = 0;

	debug ("%s - Entry old[%x], new[%x]\n", __func__, old, new);
	

	for (i = ctrl->starting_slot_num; i <= ctrl->ending_slot_num; i++) {
		mask = 0x01 << i;
		if ((mask & old) != (mask & new)) {
			pslot = ibmphp_get_slot_from_physical_num (i);
			if (pslot) {
				memcpy ((void *) &myslot, (void *) pslot, sizeof (struct slot));
				rc = ibmphp_hpc_readslot (pslot, READ_ALLSTAT, NULL);
				debug ("%s - call process_changeinstatus for slot[%d]\n", __func__, i);
				process_changeinstatus (pslot, &myslot);
			} else {
				rc = -EINVAL;
				err ("%s - Error bad pointer for slot[%d]\n", __func__, i);
			}
		}
	}
	debug ("%s - Exit rc[%d]\n", __func__, rc);
	return rc;
}

int __init ibmphp_hpc_start_poll_thread (void)
{
	debug ("%s - Entry\n", __func__);

	ibmphp_poll_thread = kthread_run(poll_hpc, NULL, "hpc_poll");
	if (IS_ERR(ibmphp_poll_thread)) {
		err ("%s - Error, thread not started\n", __func__);
		return PTR_ERR(ibmphp_poll_thread);
	}
	return 0;
}

void __exit ibmphp_hpc_stop_poll_thread (void)
{
	debug ("%s - Entry\n", __func__);

	kthread_stop(ibmphp_poll_thread);
	debug ("before locking operations \n");
	ibmphp_lock_operations ();
	debug ("after locking operations \n");
	
	
	debug ("before sem_exit down \n");
	down (&sem_exit);
	debug ("after sem_exit down \n");

	
	debug ("before free_hpc_access \n");
	free_hpc_access ();
	debug ("after free_hpc_access \n");
	ibmphp_unlock_operations ();
	debug ("after unlock operations \n");
	up (&sem_exit);
	debug ("after sem exit up\n");

	debug ("%s - Exit\n", __func__);
}

static int hpc_wait_ctlr_notworking (int timeout, struct controller *ctlr_ptr, void __iomem *wpg_bbar,
				    u8 * pstatus)
{
	int rc = 0;
	u8 done = 0;

	debug_polling ("hpc_wait_ctlr_notworking - Entry timeout[%d]\n", timeout);

	while (!done) {
		*pstatus = ctrl_read (ctlr_ptr, wpg_bbar, WPG_CTLR_INDEX);
		if (*pstatus == HPC_ERROR) {
			rc = HPC_ERROR;
			done = 1;
		}
		if (CTLR_WORKING (*pstatus) == HPC_CTLR_WORKING_NO)
			done = 1;
		if (!done) {
			msleep(1000);
			if (timeout < 1) {
				done = 1;
				err ("HPCreadslot - Error ctlr timeout\n");
				rc = HPC_ERROR;
			} else
				timeout--;
		}
	}
	debug_polling ("hpc_wait_ctlr_notworking - Exit rc[%x] status[%x]\n", rc, *pstatus);
	return rc;
}
