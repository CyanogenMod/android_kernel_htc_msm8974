/*
 *    Disk Array driver for HP Smart Array controllers, SCSI Tape module.
 *    (C) Copyright 2001, 2007 Hewlett-Packard Development Company, L.P.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; version 2 of the License.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *    General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 300, Boston, MA
 *    02111-1307, USA.
 *
 *    Questions/Comments/Bugfixes to iss_storagedev@hp.com
 *    
 *    Author: Stephen M. Cameron
 */
#ifdef CONFIG_CISS_SCSI_TAPE


#include <linux/timer.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <linux/atomic.h>

#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h> 

#include "cciss_scsi.h"

#define CCISS_ABORT_MSG 0x00
#define CCISS_RESET_MSG 0x01

static int fill_cmd(ctlr_info_t *h, CommandList_struct *c, __u8 cmd, void *buff,
	size_t size,
	__u8 page_code, unsigned char *scsi3addr,
	int cmd_type);

static CommandList_struct *cmd_alloc(ctlr_info_t *h);
static CommandList_struct *cmd_special_alloc(ctlr_info_t *h);
static void cmd_free(ctlr_info_t *h, CommandList_struct *c);
static void cmd_special_free(ctlr_info_t *h, CommandList_struct *c);

static int cciss_scsi_proc_info(
		struct Scsi_Host *sh,
		char *buffer, 
		char **start, 	   
		off_t offset,	   
		int length, 	   
		int func);	   

static int cciss_scsi_queue_command (struct Scsi_Host *h,
				     struct scsi_cmnd *cmd);
static int cciss_eh_device_reset_handler(struct scsi_cmnd *);
static int cciss_eh_abort_handler(struct scsi_cmnd *);

static struct cciss_scsi_hba_t ccissscsi[MAX_CTLR] = {
	{ .name = "cciss0", .ndevices = 0 },
	{ .name = "cciss1", .ndevices = 0 },
	{ .name = "cciss2", .ndevices = 0 },
	{ .name = "cciss3", .ndevices = 0 },
	{ .name = "cciss4", .ndevices = 0 },
	{ .name = "cciss5", .ndevices = 0 },
	{ .name = "cciss6", .ndevices = 0 },
	{ .name = "cciss7", .ndevices = 0 },
};

static struct scsi_host_template cciss_driver_template = {
	.module			= THIS_MODULE,
	.name			= "cciss",
	.proc_name		= "cciss",
	.proc_info		= cciss_scsi_proc_info,
	.queuecommand		= cciss_scsi_queue_command,
	.this_id		= 7,
	.cmd_per_lun		= 1,
	.use_clustering		= DISABLE_CLUSTERING,
	
	.eh_device_reset_handler= cciss_eh_device_reset_handler,
	.eh_abort_handler	= cciss_eh_abort_handler,
};

#pragma pack(1)

#define SCSI_PAD_32 8
#define SCSI_PAD_64 8

struct cciss_scsi_cmd_stack_elem_t {
	CommandList_struct cmd;
	ErrorInfo_struct Err;
	__u32 busaddr;
	int cmdindex;
	u8 pad[IS_32_BIT * SCSI_PAD_32 + IS_64_BIT * SCSI_PAD_64];
};

#pragma pack()

#pragma pack(1)
struct cciss_scsi_cmd_stack_t {
	struct cciss_scsi_cmd_stack_elem_t *pool;
	struct cciss_scsi_cmd_stack_elem_t **elem;
	dma_addr_t cmd_pool_handle;
	int top;
	int nelems;
};
#pragma pack()

struct cciss_scsi_adapter_data_t {
	struct Scsi_Host *scsi_host;
	struct cciss_scsi_cmd_stack_t cmd_stack;
	SGDescriptor_struct **cmd_sg_list;
	int registered;
	spinlock_t lock; 
};

#define CPQ_TAPE_LOCK(h, flags) spin_lock_irqsave( \
	&h->scsi_ctlr->lock, flags);
#define CPQ_TAPE_UNLOCK(h, flags) spin_unlock_irqrestore( \
	&h->scsi_ctlr->lock, flags);

static CommandList_struct *
scsi_cmd_alloc(ctlr_info_t *h)
{
	
	
	
	

	
	struct cciss_scsi_cmd_stack_elem_t *c;
	struct cciss_scsi_adapter_data_t *sa;
	struct cciss_scsi_cmd_stack_t *stk;
	u64bit temp64;

	sa = h->scsi_ctlr;
	stk = &sa->cmd_stack; 

	if (stk->top < 0) 
		return NULL;
	c = stk->elem[stk->top]; 	
	
	memset(&c->cmd, 0, sizeof(c->cmd));
	memset(&c->Err, 0, sizeof(c->Err));
	
	c->cmd.busaddr = c->busaddr; 
	c->cmd.cmdindex = c->cmdindex;

	temp64.val = (__u64) (c->busaddr + sizeof(CommandList_struct));
	stk->top--;
	c->cmd.ErrDesc.Addr.lower = temp64.val32.lower;
	c->cmd.ErrDesc.Addr.upper = temp64.val32.upper;
	c->cmd.ErrDesc.Len = sizeof(ErrorInfo_struct);
	
	c->cmd.ctlr = h->ctlr;
	c->cmd.err_info = &c->Err;

	return (CommandList_struct *) c;
}

static void 
scsi_cmd_free(ctlr_info_t *h, CommandList_struct *c)
{
	
	
	

	struct cciss_scsi_adapter_data_t *sa;
	struct cciss_scsi_cmd_stack_t *stk;

	sa = h->scsi_ctlr;
	stk = &sa->cmd_stack; 
	stk->top++;
	if (stk->top >= stk->nelems) {
		dev_err(&h->pdev->dev,
			"scsi_cmd_free called too many times.\n");
		BUG();
	}
	stk->elem[stk->top] = (struct cciss_scsi_cmd_stack_elem_t *) c;
}

static int
scsi_cmd_stack_setup(ctlr_info_t *h, struct cciss_scsi_adapter_data_t *sa)
{
	int i;
	struct cciss_scsi_cmd_stack_t *stk;
	size_t size;

	stk = &sa->cmd_stack;
	stk->nelems = cciss_tape_cmds + 2;
	sa->cmd_sg_list = cciss_allocate_sg_chain_blocks(h,
		h->chainsize, stk->nelems);
	if (!sa->cmd_sg_list && h->chainsize > 0)
		return -ENOMEM;

	size = sizeof(struct cciss_scsi_cmd_stack_elem_t) * stk->nelems;

	
	BUILD_BUG_ON((sizeof(*stk->pool) % COMMANDLIST_ALIGNMENT) != 0);
	
	stk->pool = (struct cciss_scsi_cmd_stack_elem_t *)
		pci_alloc_consistent(h->pdev, size, &stk->cmd_pool_handle);

	if (stk->pool == NULL) {
		cciss_free_sg_chain_blocks(sa->cmd_sg_list, stk->nelems);
		sa->cmd_sg_list = NULL;
		return -ENOMEM;
	}
	stk->elem = kmalloc(sizeof(stk->elem[0]) * stk->nelems, GFP_KERNEL);
	if (!stk->elem) {
		pci_free_consistent(h->pdev, size, stk->pool,
		stk->cmd_pool_handle);
		return -1;
	}
	for (i = 0; i < stk->nelems; i++) {
		stk->elem[i] = &stk->pool[i];
		stk->elem[i]->busaddr = (__u32) (stk->cmd_pool_handle + 
			(sizeof(struct cciss_scsi_cmd_stack_elem_t) * i));
		stk->elem[i]->cmdindex = i;
	}
	stk->top = stk->nelems-1;
	return 0;
}

static void
scsi_cmd_stack_free(ctlr_info_t *h)
{
	struct cciss_scsi_adapter_data_t *sa;
	struct cciss_scsi_cmd_stack_t *stk;
	size_t size;

	sa = h->scsi_ctlr;
	stk = &sa->cmd_stack; 
	if (stk->top != stk->nelems-1) {
		dev_warn(&h->pdev->dev,
			"bug: %d scsi commands are still outstanding.\n",
			stk->nelems - stk->top);
	}
	size = sizeof(struct cciss_scsi_cmd_stack_elem_t) * stk->nelems;

	pci_free_consistent(h->pdev, size, stk->pool, stk->cmd_pool_handle);
	stk->pool = NULL;
	cciss_free_sg_chain_blocks(sa->cmd_sg_list, stk->nelems);
	kfree(stk->elem);
	stk->elem = NULL;
}

#if 0
static int xmargin=8;
static int amargin=60;

static void
print_bytes (unsigned char *c, int len, int hex, int ascii)
{

	int i;
	unsigned char *x;

	if (hex)
	{
		x = c;
		for (i=0;i<len;i++)
		{
			if ((i % xmargin) == 0 && i>0) printk("\n");
			if ((i % xmargin) == 0) printk("0x%04x:", i);
			printk(" %02x", *x);
			x++;
		}
		printk("\n");
	}
	if (ascii)
	{
		x = c;
		for (i=0;i<len;i++)
		{
			if ((i % amargin) == 0 && i>0) printk("\n");
			if ((i % amargin) == 0) printk("0x%04x:", i);
			if (*x > 26 && *x < 128) printk("%c", *x);
			else printk(".");
			x++;
		}
		printk("\n");
	}
}

static void
print_cmd(CommandList_struct *cp)
{
	printk("queue:%d\n", cp->Header.ReplyQueue);
	printk("sglist:%d\n", cp->Header.SGList);
	printk("sgtot:%d\n", cp->Header.SGTotal);
	printk("Tag:0x%08x/0x%08x\n", cp->Header.Tag.upper, 
			cp->Header.Tag.lower);
	printk("LUN:0x%02x%02x%02x%02x%02x%02x%02x%02x\n",
		cp->Header.LUN.LunAddrBytes[0],
		cp->Header.LUN.LunAddrBytes[1],
		cp->Header.LUN.LunAddrBytes[2],
		cp->Header.LUN.LunAddrBytes[3],
		cp->Header.LUN.LunAddrBytes[4],
		cp->Header.LUN.LunAddrBytes[5],
		cp->Header.LUN.LunAddrBytes[6],
		cp->Header.LUN.LunAddrBytes[7]);
	printk("CDBLen:%d\n", cp->Request.CDBLen);
	printk("Type:%d\n",cp->Request.Type.Type);
	printk("Attr:%d\n",cp->Request.Type.Attribute);
	printk(" Dir:%d\n",cp->Request.Type.Direction);
	printk("Timeout:%d\n",cp->Request.Timeout);
	printk( "CDB: %02x %02x %02x %02x %02x %02x %02x %02x"
		" %02x %02x %02x %02x %02x %02x %02x %02x\n",
		cp->Request.CDB[0], cp->Request.CDB[1],
		cp->Request.CDB[2], cp->Request.CDB[3],
		cp->Request.CDB[4], cp->Request.CDB[5],
		cp->Request.CDB[6], cp->Request.CDB[7],
		cp->Request.CDB[8], cp->Request.CDB[9],
		cp->Request.CDB[10], cp->Request.CDB[11],
		cp->Request.CDB[12], cp->Request.CDB[13],
		cp->Request.CDB[14], cp->Request.CDB[15]),
	printk("edesc.Addr: 0x%08x/0%08x, Len  = %d\n", 
		cp->ErrDesc.Addr.upper, cp->ErrDesc.Addr.lower, 
			cp->ErrDesc.Len);
	printk("sgs..........Errorinfo:\n");
	printk("scsistatus:%d\n", cp->err_info->ScsiStatus);
	printk("senselen:%d\n", cp->err_info->SenseLen);
	printk("cmd status:%d\n", cp->err_info->CommandStatus);
	printk("resid cnt:%d\n", cp->err_info->ResidualCnt);
	printk("offense size:%d\n", cp->err_info->MoreErrInfo.Invalid_Cmd.offense_size);
	printk("offense byte:%d\n", cp->err_info->MoreErrInfo.Invalid_Cmd.offense_num);
	printk("offense value:%d\n", cp->err_info->MoreErrInfo.Invalid_Cmd.offense_value);
			
}

#endif

static int 
find_bus_target_lun(ctlr_info_t *h, int *bus, int *target, int *lun)
{
	
	
	int i, found=0;
	unsigned char target_taken[CCISS_MAX_SCSI_DEVS_PER_HBA];

	memset(&target_taken[0], 0, CCISS_MAX_SCSI_DEVS_PER_HBA);

	target_taken[SELF_SCSI_ID] = 1;	
	for (i = 0; i < ccissscsi[h->ctlr].ndevices; i++)
		target_taken[ccissscsi[h->ctlr].dev[i].target] = 1;
	
	for (i = 0; i < CCISS_MAX_SCSI_DEVS_PER_HBA; i++) {
		if (!target_taken[i]) {
			*bus = 0; *target=i; *lun = 0; found=1;
			break;
		}
	}
	return (!found);	
}
struct scsi2map {
	char scsi3addr[8];
	int bus, target, lun;
};

static int 
cciss_scsi_add_entry(ctlr_info_t *h, int hostno,
		struct cciss_scsi_dev_t *device,
		struct scsi2map *added, int *nadded)
{
	
	int n = ccissscsi[h->ctlr].ndevices;
	struct cciss_scsi_dev_t *sd;
	int i, bus, target, lun;
	unsigned char addr1[8], addr2[8];

	if (n >= CCISS_MAX_SCSI_DEVS_PER_HBA) {
		dev_warn(&h->pdev->dev, "Too many devices, "
			"some will be inaccessible.\n");
		return -1;
	}

	bus = target = -1;
	lun = 0;
	
	
	if (device->scsi3addr[4] != 0) {
		
		
		
		
		memcpy(addr1, device->scsi3addr, 8);
		addr1[4] = 0;
		for (i = 0; i < n; i++) {
			sd = &ccissscsi[h->ctlr].dev[i];
			memcpy(addr2, sd->scsi3addr, 8);
			addr2[4] = 0;
			
			if (memcmp(addr1, addr2, 8) == 0) {
				bus = sd->bus;
				target = sd->target;
				lun = device->scsi3addr[4];
				break;
			}
		}
	}

	sd = &ccissscsi[h->ctlr].dev[n];
	if (lun == 0) {
		if (find_bus_target_lun(h,
			&sd->bus, &sd->target, &sd->lun) != 0)
			return -1;
	} else {
		sd->bus = bus;
		sd->target = target;
		sd->lun = lun;
	}
	added[*nadded].bus = sd->bus;
	added[*nadded].target = sd->target;
	added[*nadded].lun = sd->lun;
	(*nadded)++;

	memcpy(sd->scsi3addr, device->scsi3addr, 8);
	memcpy(sd->vendor, device->vendor, sizeof(sd->vendor));
	memcpy(sd->revision, device->revision, sizeof(sd->revision));
	memcpy(sd->device_id, device->device_id, sizeof(sd->device_id));
	sd->devtype = device->devtype;

	ccissscsi[h->ctlr].ndevices++;

	if (hostno != -1)
		dev_info(&h->pdev->dev, "%s device c%db%dt%dl%d added.\n",
			scsi_device_type(sd->devtype), hostno,
			sd->bus, sd->target, sd->lun);
	return 0;
}

static void
cciss_scsi_remove_entry(ctlr_info_t *h, int hostno, int entry,
	struct scsi2map *removed, int *nremoved)
{
	
	int i;
	struct cciss_scsi_dev_t sd;

	if (entry < 0 || entry >= CCISS_MAX_SCSI_DEVS_PER_HBA) return;
	sd = ccissscsi[h->ctlr].dev[entry];
	removed[*nremoved].bus    = sd.bus;
	removed[*nremoved].target = sd.target;
	removed[*nremoved].lun    = sd.lun;
	(*nremoved)++;
	for (i = entry; i < ccissscsi[h->ctlr].ndevices-1; i++)
		ccissscsi[h->ctlr].dev[i] = ccissscsi[h->ctlr].dev[i+1];
	ccissscsi[h->ctlr].ndevices--;
	dev_info(&h->pdev->dev, "%s device c%db%dt%dl%d removed.\n",
		scsi_device_type(sd.devtype), hostno,
			sd.bus, sd.target, sd.lun);
}


#define SCSI3ADDR_EQ(a,b) ( \
	(a)[7] == (b)[7] && \
	(a)[6] == (b)[6] && \
	(a)[5] == (b)[5] && \
	(a)[4] == (b)[4] && \
	(a)[3] == (b)[3] && \
	(a)[2] == (b)[2] && \
	(a)[1] == (b)[1] && \
	(a)[0] == (b)[0])

static void fixup_botched_add(ctlr_info_t *h, char *scsi3addr)
{
	
	
	unsigned long flags;
	int i, j;
	CPQ_TAPE_LOCK(h, flags);
	for (i = 0; i < ccissscsi[h->ctlr].ndevices; i++) {
		if (memcmp(scsi3addr,
				ccissscsi[h->ctlr].dev[i].scsi3addr, 8) == 0) {
			for (j = i; j < ccissscsi[h->ctlr].ndevices-1; j++)
				ccissscsi[h->ctlr].dev[j] =
					ccissscsi[h->ctlr].dev[j+1];
			ccissscsi[h->ctlr].ndevices--;
			break;
		}
	}
	CPQ_TAPE_UNLOCK(h, flags);
}

static int device_is_the_same(struct cciss_scsi_dev_t *dev1,
	struct cciss_scsi_dev_t *dev2)
{
	return dev1->devtype == dev2->devtype &&
		memcmp(dev1->scsi3addr, dev2->scsi3addr,
			sizeof(dev1->scsi3addr)) == 0 &&
		memcmp(dev1->device_id, dev2->device_id,
			sizeof(dev1->device_id)) == 0 &&
		memcmp(dev1->vendor, dev2->vendor,
			sizeof(dev1->vendor)) == 0 &&
		memcmp(dev1->model, dev2->model,
			sizeof(dev1->model)) == 0 &&
		memcmp(dev1->revision, dev2->revision,
			sizeof(dev1->revision)) == 0;
}

static int
adjust_cciss_scsi_table(ctlr_info_t *h, int hostno,
	struct cciss_scsi_dev_t sd[], int nsds)
{
 

	int i,j, found, changes=0;
	struct cciss_scsi_dev_t *csd;
	unsigned long flags;
	struct scsi2map *added, *removed;
	int nadded, nremoved;
	struct Scsi_Host *sh = NULL;

	added = kzalloc(sizeof(*added) * CCISS_MAX_SCSI_DEVS_PER_HBA,
			GFP_KERNEL);
	removed = kzalloc(sizeof(*removed) * CCISS_MAX_SCSI_DEVS_PER_HBA,
			GFP_KERNEL);

	if (!added || !removed) {
		dev_warn(&h->pdev->dev,
			"Out of memory in adjust_cciss_scsi_table\n");
		goto free_and_out;
	}

	CPQ_TAPE_LOCK(h, flags);

	if (hostno != -1)  
		sh = h->scsi_ctlr->scsi_host;


	i = 0;
	nremoved = 0;
	nadded = 0;
	while (i < ccissscsi[h->ctlr].ndevices) {
		csd = &ccissscsi[h->ctlr].dev[i];
		found=0;
		for (j=0;j<nsds;j++) {
			if (SCSI3ADDR_EQ(sd[j].scsi3addr,
				csd->scsi3addr)) {
				if (device_is_the_same(&sd[j], csd))
					found=2;
				else
					found=1;
				break;
			}
		}

		if (found == 0) {  
			changes++;
			cciss_scsi_remove_entry(h, hostno, i,
				removed, &nremoved);
			
		} else if (found == 1) { 
			changes++;
			dev_info(&h->pdev->dev,
				"device c%db%dt%dl%d has changed.\n",
				hostno, csd->bus, csd->target, csd->lun);
			cciss_scsi_remove_entry(h, hostno, i,
				removed, &nremoved);
			
			if (cciss_scsi_add_entry(h, hostno, &sd[j],
				added, &nadded) != 0)
				
					BUG();
			csd->devtype = sd[j].devtype;
			memcpy(csd->device_id, sd[j].device_id,
				sizeof(csd->device_id));
			memcpy(csd->vendor, sd[j].vendor,
				sizeof(csd->vendor));
			memcpy(csd->model, sd[j].model,
				sizeof(csd->model));
			memcpy(csd->revision, sd[j].revision,
				sizeof(csd->revision));
		} else 		
			i++;	
	}


	for (i=0;i<nsds;i++) {
		found=0;
		for (j = 0; j < ccissscsi[h->ctlr].ndevices; j++) {
			csd = &ccissscsi[h->ctlr].dev[j];
			if (SCSI3ADDR_EQ(sd[i].scsi3addr,
				csd->scsi3addr)) {
				if (device_is_the_same(&sd[i], csd))
					found=2;	
				else
					found=1; 	
				break;
			}
		}
		if (!found) {
			changes++;
			if (cciss_scsi_add_entry(h, hostno, &sd[i],
				added, &nadded) != 0)
				break;
		} else if (found == 1) {
			
			changes++;
			dev_warn(&h->pdev->dev,
				"device unexpectedly changed\n");
			
		}
	}
	CPQ_TAPE_UNLOCK(h, flags);

	
	
	
	if (hostno == -1 || !changes)
		goto free_and_out;

	
	for (i = 0; i < nremoved; i++) {
		struct scsi_device *sdev =
			scsi_device_lookup(sh, removed[i].bus,
				removed[i].target, removed[i].lun);
		if (sdev != NULL) {
			scsi_remove_device(sdev);
			scsi_device_put(sdev);
		} else {
			
			
			
			dev_warn(&h->pdev->dev, "didn't find "
				"c%db%dt%dl%d\n for removal.",
				hostno, removed[i].bus,
				removed[i].target, removed[i].lun);
		}
	}

	
	for (i = 0; i < nadded; i++) {
		int rc;
		rc = scsi_add_device(sh, added[i].bus,
			added[i].target, added[i].lun);
		if (rc == 0)
			continue;
		dev_warn(&h->pdev->dev, "scsi_add_device "
			"c%db%dt%dl%d failed, device not added.\n",
			hostno, added[i].bus, added[i].target, added[i].lun);
		
		
		fixup_botched_add(h, added[i].scsi3addr);
	}

free_and_out:
	kfree(added);
	kfree(removed);
	return 0;
}

static int
lookup_scsi3addr(ctlr_info_t *h, int bus, int target, int lun, char *scsi3addr)
{
	int i;
	struct cciss_scsi_dev_t *sd;
	unsigned long flags;

	CPQ_TAPE_LOCK(h, flags);
	for (i = 0; i < ccissscsi[h->ctlr].ndevices; i++) {
		sd = &ccissscsi[h->ctlr].dev[i];
		if (sd->bus == bus &&
		    sd->target == target &&
		    sd->lun == lun) {
			memcpy(scsi3addr, &sd->scsi3addr[0], 8);
			CPQ_TAPE_UNLOCK(h, flags);
			return 0;
		}
	}
	CPQ_TAPE_UNLOCK(h, flags);
	return -1;
}

static void 
cciss_scsi_setup(ctlr_info_t *h)
{
	struct cciss_scsi_adapter_data_t * shba;

	ccissscsi[h->ctlr].ndevices = 0;
	shba = (struct cciss_scsi_adapter_data_t *)
		kmalloc(sizeof(*shba), GFP_KERNEL);	
	if (shba == NULL)
		return;
	shba->scsi_host = NULL;
	spin_lock_init(&shba->lock);
	shba->registered = 0;
	if (scsi_cmd_stack_setup(h, shba) != 0) {
		kfree(shba);
		shba = NULL;
	}
	h->scsi_ctlr = shba;
	return;
}

static void complete_scsi_command(CommandList_struct *c, int timeout,
	__u32 tag)
{
	struct scsi_cmnd *cmd;
	ctlr_info_t *h;
	ErrorInfo_struct *ei;

	ei = c->err_info;

	
	if (c->Request.Type.Type == TYPE_MSG)  {
		c->cmd_type = CMD_MSG_DONE;
		return;
	}

	cmd = (struct scsi_cmnd *) c->scsi_cmd;
	h = hba[c->ctlr];

	scsi_dma_unmap(cmd);
	if (c->Header.SGTotal > h->max_cmd_sgentries)
		cciss_unmap_sg_chain_block(h, c);

	cmd->result = (DID_OK << 16); 		
	cmd->result |= (COMMAND_COMPLETE << 8);	
			

	cmd->result |= (ei->ScsiStatus);
	

	

	memcpy(cmd->sense_buffer, ei->SenseInfo, 
		ei->SenseLen > SCSI_SENSE_BUFFERSIZE ?
			SCSI_SENSE_BUFFERSIZE : 
			ei->SenseLen);
	scsi_set_resid(cmd, ei->ResidualCnt);

	if(ei->CommandStatus != 0) 
	{  
		switch(ei->CommandStatus)
		{
			case CMD_TARGET_STATUS:
				
				if( ei->ScsiStatus)
                		{
#if 0
                    			printk(KERN_WARNING "cciss: cmd %p "
						"has SCSI Status = %x\n",
						c, ei->ScsiStatus);
#endif
					cmd->result |= (ei->ScsiStatus << 1);
                		}
				else {  
					

					cmd->result = DID_NO_CONNECT << 16;
				}
			break;
			case CMD_DATA_UNDERRUN: 
			break;
			case CMD_DATA_OVERRUN:
				dev_warn(&h->pdev->dev, "%p has"
					" completed with data overrun "
					"reported\n", c);
			break;
			case CMD_INVALID: {
				cmd->result = DID_NO_CONNECT << 16;
				}
			break;
			case CMD_PROTOCOL_ERR:
				dev_warn(&h->pdev->dev,
					"%p has protocol error\n", c);
                        break;
			case CMD_HARDWARE_ERR:
				cmd->result = DID_ERROR << 16;
				dev_warn(&h->pdev->dev,
					"%p had hardware error\n", c);
                        break;
			case CMD_CONNECTION_LOST:
				cmd->result = DID_ERROR << 16;
				dev_warn(&h->pdev->dev,
					"%p had connection lost\n", c);
			break;
			case CMD_ABORTED:
				cmd->result = DID_ABORT << 16;
				dev_warn(&h->pdev->dev, "%p was aborted\n", c);
			break;
			case CMD_ABORT_FAILED:
				cmd->result = DID_ERROR << 16;
				dev_warn(&h->pdev->dev,
					"%p reports abort failed\n", c);
			break;
			case CMD_UNSOLICITED_ABORT:
				cmd->result = DID_ABORT << 16;
				dev_warn(&h->pdev->dev, "%p aborted due to an "
					"unsolicited abort\n", c);
			break;
			case CMD_TIMEOUT:
				cmd->result = DID_TIME_OUT << 16;
				dev_warn(&h->pdev->dev, "%p timedout\n", c);
			break;
			case CMD_UNABORTABLE:
				cmd->result = DID_ERROR << 16;
				dev_warn(&h->pdev->dev, "c %p command "
					"unabortable\n", c);
			break;
			default:
				cmd->result = DID_ERROR << 16;
				dev_warn(&h->pdev->dev,
					"%p returned unknown status %x\n", c,
						ei->CommandStatus); 
		}
	}
	cmd->scsi_done(cmd);
	scsi_cmd_free(h, c);
}

static int
cciss_scsi_detect(ctlr_info_t *h)
{
	struct Scsi_Host *sh;
	int error;

	sh = scsi_host_alloc(&cciss_driver_template, sizeof(struct ctlr_info *));
	if (sh == NULL)
		goto fail;
	sh->io_port = 0;	
	sh->n_io_port = 0;	
	sh->this_id = SELF_SCSI_ID;  
	sh->can_queue = cciss_tape_cmds;
	sh->sg_tablesize = h->maxsgentries;
	sh->max_cmd_len = MAX_COMMAND_SIZE;
	sh->max_sectors = h->cciss_max_sectors;

	((struct cciss_scsi_adapter_data_t *) 
		h->scsi_ctlr)->scsi_host = sh;
	sh->hostdata[0] = (unsigned long) h;
	sh->irq = h->intr[SIMPLE_MODE_INT];
	sh->unique_id = sh->irq;
	error = scsi_add_host(sh, &h->pdev->dev);
	if (error)
		goto fail_host_put;
	scsi_scan_host(sh);
	return 1;

 fail_host_put:
	scsi_host_put(sh);
 fail:
	return 0;
}

static void
cciss_unmap_one(struct pci_dev *pdev,
		CommandList_struct *c,
		size_t buflen,
		int data_direction)
{
	u64bit addr64;

	addr64.val32.lower = c->SG[0].Addr.lower;
	addr64.val32.upper = c->SG[0].Addr.upper;
	pci_unmap_single(pdev, (dma_addr_t) addr64.val, buflen, data_direction);
}

static void
cciss_map_one(struct pci_dev *pdev,
		CommandList_struct *c,
		unsigned char *buf,
		size_t buflen,
		int data_direction)
{
	__u64 addr64;

	addr64 = (__u64) pci_map_single(pdev, buf, buflen, data_direction);
	c->SG[0].Addr.lower =
	  (__u32) (addr64 & (__u64) 0x00000000FFFFFFFF);
	c->SG[0].Addr.upper =
	  (__u32) ((addr64 >> 32) & (__u64) 0x00000000FFFFFFFF);
	c->SG[0].Len = buflen;
	c->Header.SGList = (__u8) 1;   
	c->Header.SGTotal = (__u16) 1; 
}

static int
cciss_scsi_do_simple_cmd(ctlr_info_t *h,
			CommandList_struct *c,
			unsigned char *scsi3addr, 
			unsigned char *cdb,
			unsigned char cdblen,
			unsigned char *buf, int bufsize,
			int direction)
{
	DECLARE_COMPLETION_ONSTACK(wait);

	c->cmd_type = CMD_IOCTL_PEND; 
	c->scsi_cmd = NULL;
	c->Header.ReplyQueue = 0;  
	memcpy(&c->Header.LUN, scsi3addr, sizeof(c->Header.LUN));
	c->Header.Tag.lower = c->busaddr;  
	


	memset(c->Request.CDB, 0, sizeof(c->Request.CDB));
	memcpy(c->Request.CDB, cdb, cdblen);
	c->Request.Timeout = 0;
	c->Request.CDBLen = cdblen;
	c->Request.Type.Type = TYPE_CMD;
	c->Request.Type.Attribute = ATTR_SIMPLE;
	c->Request.Type.Direction = direction;

	
	cciss_map_one(h->pdev, c, (unsigned char *) buf,
			bufsize, DMA_FROM_DEVICE); 

	c->waiting = &wait;
	enqueue_cmd_and_start_io(h, c);
	wait_for_completion(&wait);

	
	cciss_unmap_one(h->pdev, c, bufsize, DMA_FROM_DEVICE);
	return(0);
}

static void 
cciss_scsi_interpret_error(ctlr_info_t *h, CommandList_struct *c)
{
	ErrorInfo_struct *ei;

	ei = c->err_info;
	switch(ei->CommandStatus)
	{
		case CMD_TARGET_STATUS:
			dev_warn(&h->pdev->dev,
				"cmd %p has completed with errors\n", c);
			dev_warn(&h->pdev->dev,
				"cmd %p has SCSI Status = %x\n",
				c, ei->ScsiStatus);
			if (ei->ScsiStatus == 0)
				dev_warn(&h->pdev->dev,
				"SCSI status is abnormally zero.  "
				"(probably indicates selection timeout "
				"reported incorrectly due to a known "
				"firmware bug, circa July, 2001.)\n");
		break;
		case CMD_DATA_UNDERRUN: 
			dev_info(&h->pdev->dev, "UNDERRUN\n");
		break;
		case CMD_DATA_OVERRUN:
			dev_warn(&h->pdev->dev, "%p has"
				" completed with data overrun "
				"reported\n", c);
		break;
		case CMD_INVALID: {
			
			
			dev_warn(&h->pdev->dev,
				"%p is reported invalid (probably means "
				"target device no longer present)\n", c);
			}
		break;
		case CMD_PROTOCOL_ERR:
			dev_warn(&h->pdev->dev, "%p has protocol error\n", c);
		break;
		case CMD_HARDWARE_ERR:
			
			dev_warn(&h->pdev->dev, "%p had hardware error\n", c);
		break;
		case CMD_CONNECTION_LOST:
			dev_warn(&h->pdev->dev, "%p had connection lost\n", c);
		break;
		case CMD_ABORTED:
			dev_warn(&h->pdev->dev, "%p was aborted\n", c);
		break;
		case CMD_ABORT_FAILED:
			dev_warn(&h->pdev->dev,
				"%p reports abort failed\n", c);
		break;
		case CMD_UNSOLICITED_ABORT:
			dev_warn(&h->pdev->dev,
				"%p aborted due to an unsolicited abort\n", c);
		break;
		case CMD_TIMEOUT:
			dev_warn(&h->pdev->dev, "%p timedout\n", c);
		break;
		case CMD_UNABORTABLE:
			dev_warn(&h->pdev->dev,
				"%p unabortable\n", c);
		break;
		default:
			dev_warn(&h->pdev->dev,
				"%p returned unknown status %x\n",
				c, ei->CommandStatus);
	}
}

static int
cciss_scsi_do_inquiry(ctlr_info_t *h, unsigned char *scsi3addr,
	unsigned char page, unsigned char *buf,
	unsigned char bufsize)
{
	int rc;
	CommandList_struct *c;
	char cdb[6];
	ErrorInfo_struct *ei;
	unsigned long flags;

	spin_lock_irqsave(&h->lock, flags);
	c = scsi_cmd_alloc(h);
	spin_unlock_irqrestore(&h->lock, flags);

	if (c == NULL) {			
		printk("cmd_alloc returned NULL!\n");
		return -1;
	}

	ei = c->err_info;

	cdb[0] = CISS_INQUIRY;
	cdb[1] = (page != 0);
	cdb[2] = page;
	cdb[3] = 0;
	cdb[4] = bufsize;
	cdb[5] = 0;
	rc = cciss_scsi_do_simple_cmd(h, c, scsi3addr, cdb,
				6, buf, bufsize, XFER_READ);

	if (rc != 0) return rc; 

	if (ei->CommandStatus != 0 && 
	    ei->CommandStatus != CMD_DATA_UNDERRUN) {
		cciss_scsi_interpret_error(h, c);
		rc = -1;
	}
	spin_lock_irqsave(&h->lock, flags);
	scsi_cmd_free(h, c);
	spin_unlock_irqrestore(&h->lock, flags);
	return rc;	
}

static int cciss_scsi_get_device_id(ctlr_info_t *h, unsigned char *scsi3addr,
	unsigned char *device_id, int buflen)
{
	int rc;
	unsigned char *buf;

	if (buflen > 16)
		buflen = 16;
	buf = kzalloc(64, GFP_KERNEL);
	if (!buf)
		return -1;
	rc = cciss_scsi_do_inquiry(h, scsi3addr, 0x83, buf, 64);
	if (rc == 0)
		memcpy(device_id, &buf[8], buflen);
	kfree(buf);
	return rc != 0;
}

static int
cciss_scsi_do_report_phys_luns(ctlr_info_t *h,
		ReportLunData_struct *buf, int bufsize)
{
	int rc;
	CommandList_struct *c;
	unsigned char cdb[12];
	unsigned char scsi3addr[8]; 
	ErrorInfo_struct *ei;
	unsigned long flags;

	spin_lock_irqsave(&h->lock, flags);
	c = scsi_cmd_alloc(h);
	spin_unlock_irqrestore(&h->lock, flags);
	if (c == NULL) {			
		printk("cmd_alloc returned NULL!\n");
		return -1;
	}

	memset(&scsi3addr[0], 0, 8); 
	cdb[0] = CISS_REPORT_PHYS;
	cdb[1] = 0;
	cdb[2] = 0;
	cdb[3] = 0;
	cdb[4] = 0;
	cdb[5] = 0;
	cdb[6] = (bufsize >> 24) & 0xFF;  
	cdb[7] = (bufsize >> 16) & 0xFF;
	cdb[8] = (bufsize >> 8) & 0xFF;
	cdb[9] = bufsize & 0xFF;
	cdb[10] = 0;
	cdb[11] = 0;

	rc = cciss_scsi_do_simple_cmd(h, c, scsi3addr,
				cdb, 12, 
				(unsigned char *) buf, 
				bufsize, XFER_READ);

	if (rc != 0) return rc; 

	ei = c->err_info;
	if (ei->CommandStatus != 0 && 
	    ei->CommandStatus != CMD_DATA_UNDERRUN) {
		cciss_scsi_interpret_error(h, c);
		rc = -1;
	}
	spin_lock_irqsave(&h->lock, flags);
	scsi_cmd_free(h, c);
	spin_unlock_irqrestore(&h->lock, flags);
	return rc;	
}

static void
cciss_update_non_disk_devices(ctlr_info_t *h, int hostno)
{
#define OBDR_TAPE_INQ_SIZE 49
#define OBDR_TAPE_SIG "$DR-10"
	ReportLunData_struct *ld_buff;
	unsigned char *inq_buff;
	unsigned char scsi3addr[8];
	__u32 num_luns=0;
	unsigned char *ch;
	struct cciss_scsi_dev_t *currentsd, *this_device;
	int ncurrent=0;
	int reportlunsize = sizeof(*ld_buff) + CISS_MAX_PHYS_LUN * 8;
	int i;

	ld_buff = kzalloc(reportlunsize, GFP_KERNEL);
	inq_buff = kmalloc(OBDR_TAPE_INQ_SIZE, GFP_KERNEL);
	currentsd = kzalloc(sizeof(*currentsd) *
			(CCISS_MAX_SCSI_DEVS_PER_HBA+1), GFP_KERNEL);
	if (ld_buff == NULL || inq_buff == NULL || currentsd == NULL) {
		printk(KERN_ERR "cciss: out of memory\n");
		goto out;
	}
	this_device = &currentsd[CCISS_MAX_SCSI_DEVS_PER_HBA];
	if (cciss_scsi_do_report_phys_luns(h, ld_buff, reportlunsize) == 0) {
		ch = &ld_buff->LUNListLength[0];
		num_luns = ((ch[0]<<24) | (ch[1]<<16) | (ch[2]<<8) | ch[3]) / 8;
		if (num_luns > CISS_MAX_PHYS_LUN) {
			printk(KERN_WARNING 
				"cciss: Maximum physical LUNs (%d) exceeded.  "
				"%d LUNs ignored.\n", CISS_MAX_PHYS_LUN, 
				num_luns - CISS_MAX_PHYS_LUN);
			num_luns = CISS_MAX_PHYS_LUN;
		}
	}
	else {
		printk(KERN_ERR  "cciss: Report physical LUNs failed.\n");
		goto out;
	}


		
	for (i = 0; i < num_luns; i++) {
		
		if (ld_buff->LUN[i][3] & 0xC0) continue;
		memset(inq_buff, 0, OBDR_TAPE_INQ_SIZE);
		memcpy(&scsi3addr[0], &ld_buff->LUN[i][0], 8);

		if (cciss_scsi_do_inquiry(h, scsi3addr, 0, inq_buff,
			(unsigned char) OBDR_TAPE_INQ_SIZE) != 0)
			
			continue; 

		this_device->devtype = (inq_buff[0] & 0x1f);
		this_device->bus = -1;
		this_device->target = -1;
		this_device->lun = -1;
		memcpy(this_device->scsi3addr, scsi3addr, 8);
		memcpy(this_device->vendor, &inq_buff[8],
			sizeof(this_device->vendor));
		memcpy(this_device->model, &inq_buff[16],
			sizeof(this_device->model));
		memcpy(this_device->revision, &inq_buff[32],
			sizeof(this_device->revision));
		memset(this_device->device_id, 0,
			sizeof(this_device->device_id));
		cciss_scsi_get_device_id(h, scsi3addr,
			this_device->device_id, sizeof(this_device->device_id));

		switch (this_device->devtype)
		{
		  case 0x05:  {

				char obdr_sig[7];

				strncpy(obdr_sig, &inq_buff[43], 6);
				obdr_sig[6] = '\0';
				if (strncmp(obdr_sig, OBDR_TAPE_SIG, 6) != 0)
					
					break;
			}
			
		  case 0x01: 
		  case 0x08: 
			if (ncurrent >= CCISS_MAX_SCSI_DEVS_PER_HBA) {
				printk(KERN_INFO "cciss%d: %s ignored, "
					"too many devices.\n", h->ctlr,
					scsi_device_type(this_device->devtype));
				break;
			}
			currentsd[ncurrent] = *this_device;
			ncurrent++;
			break;
		  default: 
			break;
		}
	}

	adjust_cciss_scsi_table(h, hostno, currentsd, ncurrent);
out:
	kfree(inq_buff);
	kfree(ld_buff);
	kfree(currentsd);
	return;
}

static int
is_keyword(char *ptr, int len, char *verb)  
{
	int verb_len = strlen(verb);
	if (len >= verb_len && !memcmp(verb,ptr,verb_len))
		return verb_len;
	else
		return 0;
}

static int
cciss_scsi_user_command(ctlr_info_t *h, int hostno, char *buffer, int length)
{
	int arg_len;

	if ((arg_len = is_keyword(buffer, length, "rescan")) != 0)
		cciss_update_non_disk_devices(h, hostno);
	else
		return -EINVAL;
	return length;
}


static int
cciss_scsi_proc_info(struct Scsi_Host *sh,
		char *buffer, 
		char **start, 	   
		off_t offset,	   
		int length, 	   
		int func)	   
{

	int buflen, datalen;
	ctlr_info_t *h;
	int i;

	h = (ctlr_info_t *) sh->hostdata[0];
	if (h == NULL)  
		return -EINVAL;

	if (func == 0) {	
		buflen = sprintf(buffer, "cciss%d: SCSI host: %d\n",
				h->ctlr, sh->host_no);


		for (i = 0; i < ccissscsi[h->ctlr].ndevices; i++) {
			struct cciss_scsi_dev_t *sd =
				&ccissscsi[h->ctlr].dev[i];
			buflen += sprintf(&buffer[buflen], "c%db%dt%dl%d %02d "
				"0x%02x%02x%02x%02x%02x%02x%02x%02x\n",
				sh->host_no, sd->bus, sd->target, sd->lun,
				sd->devtype,
				sd->scsi3addr[0], sd->scsi3addr[1],
				sd->scsi3addr[2], sd->scsi3addr[3],
				sd->scsi3addr[4], sd->scsi3addr[5],
				sd->scsi3addr[6], sd->scsi3addr[7]);
		}
		datalen = buflen - offset;
		if (datalen < 0) { 	
			datalen = 0;
			*start = buffer+buflen;	
		} else
			*start = buffer + offset;
		return(datalen);
	} else 	
		return cciss_scsi_user_command(h, sh->host_no,
			buffer, length);	
} 


static void cciss_scatter_gather(ctlr_info_t *h, CommandList_struct *c,
	struct scsi_cmnd *cmd)
{
	unsigned int len;
	struct scatterlist *sg;
	__u64 addr64;
	int request_nsgs, i, chained, sg_index;
	struct cciss_scsi_adapter_data_t *sa = h->scsi_ctlr;
	SGDescriptor_struct *curr_sg;

	BUG_ON(scsi_sg_count(cmd) > h->maxsgentries);

	chained = 0;
	sg_index = 0;
	curr_sg = c->SG;
	request_nsgs = scsi_dma_map(cmd);
	if (request_nsgs) {
		scsi_for_each_sg(cmd, sg, request_nsgs, i) {
			if (sg_index + 1 == h->max_cmd_sgentries &&
				!chained && request_nsgs - i > 1) {
				chained = 1;
				sg_index = 0;
				curr_sg = sa->cmd_sg_list[c->cmdindex];
			}
			addr64 = (__u64) sg_dma_address(sg);
			len  = sg_dma_len(sg);
			curr_sg[sg_index].Addr.lower =
				(__u32) (addr64 & 0x0FFFFFFFFULL);
			curr_sg[sg_index].Addr.upper =
				(__u32) ((addr64 >> 32) & 0x0FFFFFFFFULL);
			curr_sg[sg_index].Len = len;
			curr_sg[sg_index].Ext = 0;
			++sg_index;
		}
		if (chained)
			cciss_map_sg_chain_block(h, c,
				sa->cmd_sg_list[c->cmdindex],
				(request_nsgs - (h->max_cmd_sgentries - 1)) *
					sizeof(SGDescriptor_struct));
	}
	
	if (request_nsgs > h->maxSG)
		h->maxSG = request_nsgs;
	c->Header.SGTotal = (u16) request_nsgs + chained;
	if (request_nsgs > h->max_cmd_sgentries)
		c->Header.SGList = h->max_cmd_sgentries;
	else
		c->Header.SGList = c->Header.SGTotal;
	return;
}


static int
cciss_scsi_queue_command_lck(struct scsi_cmnd *cmd, void (*done)(struct scsi_cmnd *))
{
	ctlr_info_t *h;
	int rc;
	unsigned char scsi3addr[8];
	CommandList_struct *c;
	unsigned long flags;

	
	
	h = (ctlr_info_t *) cmd->device->host->hostdata[0];

	rc = lookup_scsi3addr(h, cmd->device->channel, cmd->device->id,
			cmd->device->lun, scsi3addr);
	if (rc != 0) {
		
		
		cmd->result = DID_NO_CONNECT << 16;
		done(cmd);
		return 0;
	}


	spin_lock_irqsave(&h->lock, flags);
	c = scsi_cmd_alloc(h);
	spin_unlock_irqrestore(&h->lock, flags);
	if (c == NULL) {			
		dev_warn(&h->pdev->dev, "scsi_cmd_alloc returned NULL!\n");
		
		cmd->result = DID_NO_CONNECT << 16;
		done(cmd);
		return 0;
	}

	

	cmd->scsi_done = done;    

	
	cmd->host_scribble = (unsigned char *) c;

	c->cmd_type = CMD_SCSI;
	c->scsi_cmd = cmd;
	c->Header.ReplyQueue = 0;  
	memcpy(&c->Header.LUN.LunAddrBytes[0], &scsi3addr[0], 8);
	c->Header.Tag.lower = c->busaddr;  
	
	

	c->Request.Timeout = 0;
	memset(c->Request.CDB, 0, sizeof(c->Request.CDB));
	BUG_ON(cmd->cmd_len > sizeof(c->Request.CDB));
	c->Request.CDBLen = cmd->cmd_len;
	memcpy(c->Request.CDB, cmd->cmnd, cmd->cmd_len);
	c->Request.Type.Type = TYPE_CMD;
	c->Request.Type.Attribute = ATTR_SIMPLE;
	switch(cmd->sc_data_direction)
	{
	  case DMA_TO_DEVICE:
		c->Request.Type.Direction = XFER_WRITE;
		break;
	  case DMA_FROM_DEVICE:
		c->Request.Type.Direction = XFER_READ;
		break;
	  case DMA_NONE:
		c->Request.Type.Direction = XFER_NONE;
		break;
	  case DMA_BIDIRECTIONAL:
		
		
		

		c->Request.Type.Direction = XFER_RSVD;
		
		
		
		
		
		

		break;

	  default: 
		dev_warn(&h->pdev->dev, "unknown data direction: %d\n",
			cmd->sc_data_direction);
		BUG();
		break;
	}
	cciss_scatter_gather(h, c, cmd);
	enqueue_cmd_and_start_io(h, c);
	
	return 0;
}

static DEF_SCSI_QCMD(cciss_scsi_queue_command)

static void cciss_unregister_scsi(ctlr_info_t *h)
{
	struct cciss_scsi_adapter_data_t *sa;
	struct cciss_scsi_cmd_stack_t *stk;
	unsigned long flags;

	

	spin_lock_irqsave(&h->lock, flags);
	sa = h->scsi_ctlr;
	stk = &sa->cmd_stack; 

	 
	if (sa->registered) {
		spin_unlock_irqrestore(&h->lock, flags);
		scsi_remove_host(sa->scsi_host);
		scsi_host_put(sa->scsi_host);
		spin_lock_irqsave(&h->lock, flags);
	}

	sa->scsi_host = NULL;
	spin_unlock_irqrestore(&h->lock, flags);
	scsi_cmd_stack_free(h);
	kfree(sa);
}

static int cciss_engage_scsi(ctlr_info_t *h)
{
	struct cciss_scsi_adapter_data_t *sa;
	struct cciss_scsi_cmd_stack_t *stk;
	unsigned long flags;

	spin_lock_irqsave(&h->lock, flags);
	sa = h->scsi_ctlr;
	stk = &sa->cmd_stack; 

	if (sa->registered) {
		dev_info(&h->pdev->dev, "SCSI subsystem already engaged.\n");
		spin_unlock_irqrestore(&h->lock, flags);
		return -ENXIO;
	}
	sa->registered = 1;
	spin_unlock_irqrestore(&h->lock, flags);
	cciss_update_non_disk_devices(h, -1);
	cciss_scsi_detect(h);
	return 0;
}

static void
cciss_seq_tape_report(struct seq_file *seq, ctlr_info_t *h)
{
	unsigned long flags;

	CPQ_TAPE_LOCK(h, flags);
	seq_printf(seq,
		"Sequential access devices: %d\n\n",
			ccissscsi[h->ctlr].ndevices);
	CPQ_TAPE_UNLOCK(h, flags);
}

static int wait_for_device_to_become_ready(ctlr_info_t *h,
	unsigned char lunaddr[])
{
	int rc;
	int count = 0;
	int waittime = HZ;
	CommandList_struct *c;

	c = cmd_alloc(h);
	if (!c) {
		dev_warn(&h->pdev->dev, "out of memory in "
			"wait_for_device_to_become_ready.\n");
		return IO_ERROR;
	}

	
	while (count < 20) {

		schedule_timeout_uninterruptible(waittime);
		count++;

		
		if (waittime < (HZ * 30))
			waittime = waittime * 2;

		
		rc = fill_cmd(h, c, TEST_UNIT_READY, NULL, 0, 0,
			lunaddr, TYPE_CMD);
		if (rc == 0)
			rc = sendcmd_withirq_core(h, c, 0);

		(void) process_sendcmd_error(h, c);

		if (rc != 0)
			goto retry_tur;

		if (c->err_info->CommandStatus == CMD_SUCCESS)
			break;

		if (c->err_info->CommandStatus == CMD_TARGET_STATUS &&
			c->err_info->ScsiStatus == SAM_STAT_CHECK_CONDITION) {
			if (c->err_info->SenseInfo[2] == NO_SENSE)
				break;
			if (c->err_info->SenseInfo[2] == UNIT_ATTENTION) {
				unsigned char asc;
				asc = c->err_info->SenseInfo[12];
				check_for_unit_attention(h, c);
				if (asc == POWER_OR_RESET)
					break;
			}
		}
retry_tur:
		dev_warn(&h->pdev->dev, "Waiting %d secs "
			"for device to become ready.\n",
			waittime / HZ);
		rc = 1; 
	}

	if (rc)
		dev_warn(&h->pdev->dev, "giving up on device.\n");
	else
		dev_warn(&h->pdev->dev, "device is ready.\n");

	cmd_free(h, c);
	return rc;
}


static int cciss_eh_device_reset_handler(struct scsi_cmnd *scsicmd)
{
	int rc;
	CommandList_struct *cmd_in_trouble;
	unsigned char lunaddr[8];
	ctlr_info_t *h;

	
	h = (ctlr_info_t *) scsicmd->device->host->hostdata[0];
	if (h == NULL) 
		return FAILED;
	dev_warn(&h->pdev->dev, "resetting tape drive or medium changer.\n");
	
	cmd_in_trouble = (CommandList_struct *) scsicmd->host_scribble;
	if (cmd_in_trouble == NULL) 
		return FAILED;
	memcpy(lunaddr, &cmd_in_trouble->Header.LUN.LunAddrBytes[0], 8);
	
	rc = sendcmd_withirq(h, CCISS_RESET_MSG, NULL, 0, 0, lunaddr,
		TYPE_MSG);
	if (rc == 0 && wait_for_device_to_become_ready(h, lunaddr) == 0)
		return SUCCESS;
	dev_warn(&h->pdev->dev, "resetting device failed.\n");
	return FAILED;
}

static int  cciss_eh_abort_handler(struct scsi_cmnd *scsicmd)
{
	int rc;
	CommandList_struct *cmd_to_abort;
	unsigned char lunaddr[8];
	ctlr_info_t *h;

	
	h = (ctlr_info_t *) scsicmd->device->host->hostdata[0];
	if (h == NULL) 
		return FAILED;
	dev_warn(&h->pdev->dev, "aborting tardy SCSI cmd\n");

	
	cmd_to_abort = (CommandList_struct *) scsicmd->host_scribble;
	if (cmd_to_abort == NULL) 
		return FAILED;
	memcpy(lunaddr, &cmd_to_abort->Header.LUN.LunAddrBytes[0], 8);
	rc = sendcmd_withirq(h, CCISS_ABORT_MSG, &cmd_to_abort->Header.Tag,
		0, 0, lunaddr, TYPE_MSG);
	if (rc == 0)
		return SUCCESS;
	return FAILED;

}

#else 


#define cciss_scsi_setup(cntl_num)
#define cciss_engage_scsi(h)

#endif 
