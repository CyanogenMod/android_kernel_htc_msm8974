/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2007-2012 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com                                                  *
 *                                                                 *
 * This program is free software; you can redistribute it and/or   *
 * modify it under the terms of version 2 of the GNU General       *
 * Public License as published by the Free Software Foundation.    *
 * This program is distributed in the hope that it will be useful. *
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND          *
 * WARRANTIES, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,  *
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE      *
 * DISCLAIMED, EXCEPT TO THE EXTENT THAT SUCH DISCLAIMERS ARE HELD *
 * TO BE LEGALLY INVALID.  See the GNU General Public License for  *
 * more details, a copy of which can be found in the file COPYING  *
 * included with this package.                                     *
 *******************************************************************/

#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/ctype.h>

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_fc.h>

#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc_disc.h"
#include "lpfc_scsi.h"
#include "lpfc.h"
#include "lpfc_logmsg.h"
#include "lpfc_crtn.h"
#include "lpfc_vport.h"
#include "lpfc_version.h"
#include "lpfc_compat.h"
#include "lpfc_debugfs.h"
#include "lpfc_bsg.h"

#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
static int lpfc_debugfs_enable = 1;
module_param(lpfc_debugfs_enable, int, S_IRUGO);
MODULE_PARM_DESC(lpfc_debugfs_enable, "Enable debugfs services");

static int lpfc_debugfs_max_disc_trc;
module_param(lpfc_debugfs_max_disc_trc, int, S_IRUGO);
MODULE_PARM_DESC(lpfc_debugfs_max_disc_trc,
	"Set debugfs discovery trace depth");

static int lpfc_debugfs_max_slow_ring_trc;
module_param(lpfc_debugfs_max_slow_ring_trc, int, S_IRUGO);
MODULE_PARM_DESC(lpfc_debugfs_max_slow_ring_trc,
	"Set debugfs slow ring trace depth");

static int lpfc_debugfs_mask_disc_trc;
module_param(lpfc_debugfs_mask_disc_trc, int, S_IRUGO);
MODULE_PARM_DESC(lpfc_debugfs_mask_disc_trc,
	"Set debugfs discovery trace mask");

#include <linux/debugfs.h>

static atomic_t lpfc_debugfs_seq_trc_cnt = ATOMIC_INIT(0);
static unsigned long lpfc_debugfs_start_time = 0L;

static struct lpfc_idiag idiag;

static int
lpfc_debugfs_disc_trc_data(struct lpfc_vport *vport, char *buf, int size)
{
	int i, index, len, enable;
	uint32_t ms;
	struct lpfc_debugfs_trc *dtp;
	char *buffer;

	buffer = kmalloc(LPFC_DEBUG_TRC_ENTRY_SIZE, GFP_KERNEL);
	if (!buffer)
		return 0;

	enable = lpfc_debugfs_enable;
	lpfc_debugfs_enable = 0;

	len = 0;
	index = (atomic_read(&vport->disc_trc_cnt) + 1) &
		(lpfc_debugfs_max_disc_trc - 1);
	for (i = index; i < lpfc_debugfs_max_disc_trc; i++) {
		dtp = vport->disc_trc + i;
		if (!dtp->fmt)
			continue;
		ms = jiffies_to_msecs(dtp->jif - lpfc_debugfs_start_time);
		snprintf(buffer,
			LPFC_DEBUG_TRC_ENTRY_SIZE, "%010d:%010d ms:%s\n",
			dtp->seq_cnt, ms, dtp->fmt);
		len +=  snprintf(buf+len, size-len, buffer,
			dtp->data1, dtp->data2, dtp->data3);
	}
	for (i = 0; i < index; i++) {
		dtp = vport->disc_trc + i;
		if (!dtp->fmt)
			continue;
		ms = jiffies_to_msecs(dtp->jif - lpfc_debugfs_start_time);
		snprintf(buffer,
			LPFC_DEBUG_TRC_ENTRY_SIZE, "%010d:%010d ms:%s\n",
			dtp->seq_cnt, ms, dtp->fmt);
		len +=  snprintf(buf+len, size-len, buffer,
			dtp->data1, dtp->data2, dtp->data3);
	}

	lpfc_debugfs_enable = enable;
	kfree(buffer);

	return len;
}

static int
lpfc_debugfs_slow_ring_trc_data(struct lpfc_hba *phba, char *buf, int size)
{
	int i, index, len, enable;
	uint32_t ms;
	struct lpfc_debugfs_trc *dtp;
	char *buffer;

	buffer = kmalloc(LPFC_DEBUG_TRC_ENTRY_SIZE, GFP_KERNEL);
	if (!buffer)
		return 0;

	enable = lpfc_debugfs_enable;
	lpfc_debugfs_enable = 0;

	len = 0;
	index = (atomic_read(&phba->slow_ring_trc_cnt) + 1) &
		(lpfc_debugfs_max_slow_ring_trc - 1);
	for (i = index; i < lpfc_debugfs_max_slow_ring_trc; i++) {
		dtp = phba->slow_ring_trc + i;
		if (!dtp->fmt)
			continue;
		ms = jiffies_to_msecs(dtp->jif - lpfc_debugfs_start_time);
		snprintf(buffer,
			LPFC_DEBUG_TRC_ENTRY_SIZE, "%010d:%010d ms:%s\n",
			dtp->seq_cnt, ms, dtp->fmt);
		len +=  snprintf(buf+len, size-len, buffer,
			dtp->data1, dtp->data2, dtp->data3);
	}
	for (i = 0; i < index; i++) {
		dtp = phba->slow_ring_trc + i;
		if (!dtp->fmt)
			continue;
		ms = jiffies_to_msecs(dtp->jif - lpfc_debugfs_start_time);
		snprintf(buffer,
			LPFC_DEBUG_TRC_ENTRY_SIZE, "%010d:%010d ms:%s\n",
			dtp->seq_cnt, ms, dtp->fmt);
		len +=  snprintf(buf+len, size-len, buffer,
			dtp->data1, dtp->data2, dtp->data3);
	}

	lpfc_debugfs_enable = enable;
	kfree(buffer);

	return len;
}

static int lpfc_debugfs_last_hbq = -1;

static int
lpfc_debugfs_hbqinfo_data(struct lpfc_hba *phba, char *buf, int size)
{
	int len = 0;
	int cnt, i, j, found, posted, low;
	uint32_t phys, raw_index, getidx;
	struct lpfc_hbq_init *hip;
	struct hbq_s *hbqs;
	struct lpfc_hbq_entry *hbqe;
	struct lpfc_dmabuf *d_buf;
	struct hbq_dmabuf *hbq_buf;

	if (phba->sli_rev != 3)
		return 0;
	cnt = LPFC_HBQINFO_SIZE;
	spin_lock_irq(&phba->hbalock);

	
	i = lpfc_sli_hbq_count();
	if (i > 1) {
		 lpfc_debugfs_last_hbq++;
		 if (lpfc_debugfs_last_hbq >= i)
			lpfc_debugfs_last_hbq = 0;
	}
	else
		lpfc_debugfs_last_hbq = 0;

	i = lpfc_debugfs_last_hbq;

	len +=  snprintf(buf+len, size-len, "HBQ %d Info\n", i);

	hbqs =  &phba->hbqs[i];
	posted = 0;
	list_for_each_entry(d_buf, &hbqs->hbq_buffer_list, list)
		posted++;

	hip =  lpfc_hbq_defs[i];
	len +=  snprintf(buf+len, size-len,
		"idx:%d prof:%d rn:%d bufcnt:%d icnt:%d acnt:%d posted %d\n",
		hip->hbq_index, hip->profile, hip->rn,
		hip->buffer_count, hip->init_count, hip->add_count, posted);

	raw_index = phba->hbq_get[i];
	getidx = le32_to_cpu(raw_index);
	len +=  snprintf(buf+len, size-len,
		"entrys:%d bufcnt:%d Put:%d nPut:%d localGet:%d hbaGet:%d\n",
		hbqs->entry_count, hbqs->buffer_count, hbqs->hbqPutIdx,
		hbqs->next_hbqPutIdx, hbqs->local_hbqGetIdx, getidx);

	hbqe = (struct lpfc_hbq_entry *) phba->hbqs[i].hbq_virt;
	for (j=0; j<hbqs->entry_count; j++) {
		len +=  snprintf(buf+len, size-len,
			"%03d: %08x %04x %05x ", j,
			le32_to_cpu(hbqe->bde.addrLow),
			le32_to_cpu(hbqe->bde.tus.w),
			le32_to_cpu(hbqe->buffer_tag));
		i = 0;
		found = 0;

		
		low = hbqs->hbqPutIdx - posted;
		if (low >= 0) {
			if ((j >= hbqs->hbqPutIdx) || (j < low)) {
				len +=  snprintf(buf+len, size-len, "Unused\n");
				goto skipit;
			}
		}
		else {
			if ((j >= hbqs->hbqPutIdx) &&
				(j < (hbqs->entry_count+low))) {
				len +=  snprintf(buf+len, size-len, "Unused\n");
				goto skipit;
			}
		}

		
		list_for_each_entry(d_buf, &hbqs->hbq_buffer_list, list) {
			hbq_buf = container_of(d_buf, struct hbq_dmabuf, dbuf);
			phys = ((uint64_t)hbq_buf->dbuf.phys & 0xffffffff);
			if (phys == le32_to_cpu(hbqe->bde.addrLow)) {
				len +=  snprintf(buf+len, size-len,
					"Buf%d: %p %06x\n", i,
					hbq_buf->dbuf.virt, hbq_buf->tag);
				found = 1;
				break;
			}
			i++;
		}
		if (!found) {
			len +=  snprintf(buf+len, size-len, "No DMAinfo?\n");
		}
skipit:
		hbqe++;
		if (len > LPFC_HBQINFO_SIZE - 54)
			break;
	}
	spin_unlock_irq(&phba->hbalock);
	return len;
}

static int lpfc_debugfs_last_hba_slim_off;

static int
lpfc_debugfs_dumpHBASlim_data(struct lpfc_hba *phba, char *buf, int size)
{
	int len = 0;
	int i, off;
	uint32_t *ptr;
	char *buffer;

	buffer = kmalloc(1024, GFP_KERNEL);
	if (!buffer)
		return 0;

	off = 0;
	spin_lock_irq(&phba->hbalock);

	len +=  snprintf(buf+len, size-len, "HBA SLIM\n");
	lpfc_memcpy_from_slim(buffer,
		phba->MBslimaddr + lpfc_debugfs_last_hba_slim_off, 1024);

	ptr = (uint32_t *)&buffer[0];
	off = lpfc_debugfs_last_hba_slim_off;

	
	lpfc_debugfs_last_hba_slim_off += 1024;
	if (lpfc_debugfs_last_hba_slim_off >= 4096)
		lpfc_debugfs_last_hba_slim_off = 0;

	i = 1024;
	while (i > 0) {
		len +=  snprintf(buf+len, size-len,
		"%08x: %08x %08x %08x %08x %08x %08x %08x %08x\n",
		off, *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4),
		*(ptr+5), *(ptr+6), *(ptr+7));
		ptr += 8;
		i -= (8 * sizeof(uint32_t));
		off += (8 * sizeof(uint32_t));
	}

	spin_unlock_irq(&phba->hbalock);
	kfree(buffer);

	return len;
}

static int
lpfc_debugfs_dumpHostSlim_data(struct lpfc_hba *phba, char *buf, int size)
{
	int len = 0;
	int i, off;
	uint32_t word0, word1, word2, word3;
	uint32_t *ptr;
	struct lpfc_pgp *pgpp;
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring *pring;

	off = 0;
	spin_lock_irq(&phba->hbalock);

	len +=  snprintf(buf+len, size-len, "SLIM Mailbox\n");
	ptr = (uint32_t *)phba->slim2p.virt;
	i = sizeof(MAILBOX_t);
	while (i > 0) {
		len +=  snprintf(buf+len, size-len,
		"%08x: %08x %08x %08x %08x %08x %08x %08x %08x\n",
		off, *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4),
		*(ptr+5), *(ptr+6), *(ptr+7));
		ptr += 8;
		i -= (8 * sizeof(uint32_t));
		off += (8 * sizeof(uint32_t));
	}

	len +=  snprintf(buf+len, size-len, "SLIM PCB\n");
	ptr = (uint32_t *)phba->pcb;
	i = sizeof(PCB_t);
	while (i > 0) {
		len +=  snprintf(buf+len, size-len,
		"%08x: %08x %08x %08x %08x %08x %08x %08x %08x\n",
		off, *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4),
		*(ptr+5), *(ptr+6), *(ptr+7));
		ptr += 8;
		i -= (8 * sizeof(uint32_t));
		off += (8 * sizeof(uint32_t));
	}

	for (i = 0; i < 4; i++) {
		pgpp = &phba->port_gp[i];
		pring = &psli->ring[i];
		len +=  snprintf(buf+len, size-len,
				 "Ring %d: CMD GetInx:%d (Max:%d Next:%d "
				 "Local:%d flg:x%x)  RSP PutInx:%d Max:%d\n",
				 i, pgpp->cmdGetInx, pring->numCiocb,
				 pring->next_cmdidx, pring->local_getidx,
				 pring->flag, pgpp->rspPutInx, pring->numRiocb);
	}

	if (phba->sli_rev <= LPFC_SLI_REV3) {
		word0 = readl(phba->HAregaddr);
		word1 = readl(phba->CAregaddr);
		word2 = readl(phba->HSregaddr);
		word3 = readl(phba->HCregaddr);
		len +=  snprintf(buf+len, size-len, "HA:%08x CA:%08x HS:%08x "
				 "HC:%08x\n", word0, word1, word2, word3);
	}
	spin_unlock_irq(&phba->hbalock);
	return len;
}

static int
lpfc_debugfs_nodelist_data(struct lpfc_vport *vport, char *buf, int size)
{
	int len = 0;
	int cnt;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp;
	unsigned char *statep, *name;

	cnt = (LPFC_NODELIST_SIZE / LPFC_NODELIST_ENTRY_SIZE);

	spin_lock_irq(shost->host_lock);
	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		if (!cnt) {
			len +=  snprintf(buf+len, size-len,
				"Missing Nodelist Entries\n");
			break;
		}
		cnt--;
		switch (ndlp->nlp_state) {
		case NLP_STE_UNUSED_NODE:
			statep = "UNUSED";
			break;
		case NLP_STE_PLOGI_ISSUE:
			statep = "PLOGI ";
			break;
		case NLP_STE_ADISC_ISSUE:
			statep = "ADISC ";
			break;
		case NLP_STE_REG_LOGIN_ISSUE:
			statep = "REGLOG";
			break;
		case NLP_STE_PRLI_ISSUE:
			statep = "PRLI  ";
			break;
		case NLP_STE_UNMAPPED_NODE:
			statep = "UNMAP ";
			break;
		case NLP_STE_MAPPED_NODE:
			statep = "MAPPED";
			break;
		case NLP_STE_NPR_NODE:
			statep = "NPR   ";
			break;
		default:
			statep = "UNKNOWN";
		}
		len +=  snprintf(buf+len, size-len, "%s DID:x%06x ",
			statep, ndlp->nlp_DID);
		name = (unsigned char *)&ndlp->nlp_portname;
		len +=  snprintf(buf+len, size-len,
			"WWPN %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x ",
			*name, *(name+1), *(name+2), *(name+3),
			*(name+4), *(name+5), *(name+6), *(name+7));
		name = (unsigned char *)&ndlp->nlp_nodename;
		len +=  snprintf(buf+len, size-len,
			"WWNN %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x ",
			*name, *(name+1), *(name+2), *(name+3),
			*(name+4), *(name+5), *(name+6), *(name+7));
		len +=  snprintf(buf+len, size-len, "RPI:%03d flag:x%08x ",
			ndlp->nlp_rpi, ndlp->nlp_flag);
		if (!ndlp->nlp_type)
			len +=  snprintf(buf+len, size-len, "UNKNOWN_TYPE ");
		if (ndlp->nlp_type & NLP_FC_NODE)
			len +=  snprintf(buf+len, size-len, "FC_NODE ");
		if (ndlp->nlp_type & NLP_FABRIC)
			len +=  snprintf(buf+len, size-len, "FABRIC ");
		if (ndlp->nlp_type & NLP_FCP_TARGET)
			len +=  snprintf(buf+len, size-len, "FCP_TGT sid:%d ",
				ndlp->nlp_sid);
		if (ndlp->nlp_type & NLP_FCP_INITIATOR)
			len +=  snprintf(buf+len, size-len, "FCP_INITIATOR ");
		len += snprintf(buf+len, size-len, "usgmap:%x ",
			ndlp->nlp_usg_map);
		len += snprintf(buf+len, size-len, "refcnt:%x",
			atomic_read(&ndlp->kref.refcount));
		len +=  snprintf(buf+len, size-len, "\n");
	}
	spin_unlock_irq(shost->host_lock);
	return len;
}
#endif

inline void
lpfc_debugfs_disc_trc(struct lpfc_vport *vport, int mask, char *fmt,
	uint32_t data1, uint32_t data2, uint32_t data3)
{
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	struct lpfc_debugfs_trc *dtp;
	int index;

	if (!(lpfc_debugfs_mask_disc_trc & mask))
		return;

	if (!lpfc_debugfs_enable || !lpfc_debugfs_max_disc_trc ||
		!vport || !vport->disc_trc)
		return;

	index = atomic_inc_return(&vport->disc_trc_cnt) &
		(lpfc_debugfs_max_disc_trc - 1);
	dtp = vport->disc_trc + index;
	dtp->fmt = fmt;
	dtp->data1 = data1;
	dtp->data2 = data2;
	dtp->data3 = data3;
	dtp->seq_cnt = atomic_inc_return(&lpfc_debugfs_seq_trc_cnt);
	dtp->jif = jiffies;
#endif
	return;
}

inline void
lpfc_debugfs_slow_ring_trc(struct lpfc_hba *phba, char *fmt,
	uint32_t data1, uint32_t data2, uint32_t data3)
{
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	struct lpfc_debugfs_trc *dtp;
	int index;

	if (!lpfc_debugfs_enable || !lpfc_debugfs_max_slow_ring_trc ||
		!phba || !phba->slow_ring_trc)
		return;

	index = atomic_inc_return(&phba->slow_ring_trc_cnt) &
		(lpfc_debugfs_max_slow_ring_trc - 1);
	dtp = phba->slow_ring_trc + index;
	dtp->fmt = fmt;
	dtp->data1 = data1;
	dtp->data2 = data2;
	dtp->data3 = data3;
	dtp->seq_cnt = atomic_inc_return(&lpfc_debugfs_seq_trc_cnt);
	dtp->jif = jiffies;
#endif
	return;
}

#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
static int
lpfc_debugfs_disc_trc_open(struct inode *inode, struct file *file)
{
	struct lpfc_vport *vport = inode->i_private;
	struct lpfc_debug *debug;
	int size;
	int rc = -ENOMEM;

	if (!lpfc_debugfs_max_disc_trc) {
		 rc = -ENOSPC;
		goto out;
	}

	debug = kmalloc(sizeof(*debug), GFP_KERNEL);
	if (!debug)
		goto out;

	
	size =  (lpfc_debugfs_max_disc_trc * LPFC_DEBUG_TRC_ENTRY_SIZE);
	size = PAGE_ALIGN(size);

	debug->buffer = kmalloc(size, GFP_KERNEL);
	if (!debug->buffer) {
		kfree(debug);
		goto out;
	}

	debug->len = lpfc_debugfs_disc_trc_data(vport, debug->buffer, size);
	file->private_data = debug;

	rc = 0;
out:
	return rc;
}

static int
lpfc_debugfs_slow_ring_trc_open(struct inode *inode, struct file *file)
{
	struct lpfc_hba *phba = inode->i_private;
	struct lpfc_debug *debug;
	int size;
	int rc = -ENOMEM;

	if (!lpfc_debugfs_max_slow_ring_trc) {
		 rc = -ENOSPC;
		goto out;
	}

	debug = kmalloc(sizeof(*debug), GFP_KERNEL);
	if (!debug)
		goto out;

	
	size =  (lpfc_debugfs_max_slow_ring_trc * LPFC_DEBUG_TRC_ENTRY_SIZE);
	size = PAGE_ALIGN(size);

	debug->buffer = kmalloc(size, GFP_KERNEL);
	if (!debug->buffer) {
		kfree(debug);
		goto out;
	}

	debug->len = lpfc_debugfs_slow_ring_trc_data(phba, debug->buffer, size);
	file->private_data = debug;

	rc = 0;
out:
	return rc;
}

static int
lpfc_debugfs_hbqinfo_open(struct inode *inode, struct file *file)
{
	struct lpfc_hba *phba = inode->i_private;
	struct lpfc_debug *debug;
	int rc = -ENOMEM;

	debug = kmalloc(sizeof(*debug), GFP_KERNEL);
	if (!debug)
		goto out;

	
	debug->buffer = kmalloc(LPFC_HBQINFO_SIZE, GFP_KERNEL);
	if (!debug->buffer) {
		kfree(debug);
		goto out;
	}

	debug->len = lpfc_debugfs_hbqinfo_data(phba, debug->buffer,
		LPFC_HBQINFO_SIZE);
	file->private_data = debug;

	rc = 0;
out:
	return rc;
}

static int
lpfc_debugfs_dumpHBASlim_open(struct inode *inode, struct file *file)
{
	struct lpfc_hba *phba = inode->i_private;
	struct lpfc_debug *debug;
	int rc = -ENOMEM;

	debug = kmalloc(sizeof(*debug), GFP_KERNEL);
	if (!debug)
		goto out;

	
	debug->buffer = kmalloc(LPFC_DUMPHBASLIM_SIZE, GFP_KERNEL);
	if (!debug->buffer) {
		kfree(debug);
		goto out;
	}

	debug->len = lpfc_debugfs_dumpHBASlim_data(phba, debug->buffer,
		LPFC_DUMPHBASLIM_SIZE);
	file->private_data = debug;

	rc = 0;
out:
	return rc;
}

static int
lpfc_debugfs_dumpHostSlim_open(struct inode *inode, struct file *file)
{
	struct lpfc_hba *phba = inode->i_private;
	struct lpfc_debug *debug;
	int rc = -ENOMEM;

	debug = kmalloc(sizeof(*debug), GFP_KERNEL);
	if (!debug)
		goto out;

	
	debug->buffer = kmalloc(LPFC_DUMPHOSTSLIM_SIZE, GFP_KERNEL);
	if (!debug->buffer) {
		kfree(debug);
		goto out;
	}

	debug->len = lpfc_debugfs_dumpHostSlim_data(phba, debug->buffer,
		LPFC_DUMPHOSTSLIM_SIZE);
	file->private_data = debug;

	rc = 0;
out:
	return rc;
}

static int
lpfc_debugfs_dumpData_open(struct inode *inode, struct file *file)
{
	struct lpfc_debug *debug;
	int rc = -ENOMEM;

	if (!_dump_buf_data)
		return -EBUSY;

	debug = kmalloc(sizeof(*debug), GFP_KERNEL);
	if (!debug)
		goto out;

	
	printk(KERN_ERR "9059 BLKGRD:  %s: _dump_buf_data=0x%p\n",
			__func__, _dump_buf_data);
	debug->buffer = _dump_buf_data;
	if (!debug->buffer) {
		kfree(debug);
		goto out;
	}

	debug->len = (1 << _dump_buf_data_order) << PAGE_SHIFT;
	file->private_data = debug;

	rc = 0;
out:
	return rc;
}

static int
lpfc_debugfs_dumpDif_open(struct inode *inode, struct file *file)
{
	struct lpfc_debug *debug;
	int rc = -ENOMEM;

	if (!_dump_buf_dif)
		return -EBUSY;

	debug = kmalloc(sizeof(*debug), GFP_KERNEL);
	if (!debug)
		goto out;

	
	printk(KERN_ERR	"9060 BLKGRD: %s: _dump_buf_dif=0x%p file=%s\n",
		__func__, _dump_buf_dif, file->f_dentry->d_name.name);
	debug->buffer = _dump_buf_dif;
	if (!debug->buffer) {
		kfree(debug);
		goto out;
	}

	debug->len = (1 << _dump_buf_dif_order) << PAGE_SHIFT;
	file->private_data = debug;

	rc = 0;
out:
	return rc;
}

static ssize_t
lpfc_debugfs_dumpDataDif_write(struct file *file, const char __user *buf,
		  size_t nbytes, loff_t *ppos)
{
	spin_lock(&_dump_buf_lock);

	memset((void *)_dump_buf_data, 0,
			((1 << PAGE_SHIFT) << _dump_buf_data_order));
	memset((void *)_dump_buf_dif, 0,
			((1 << PAGE_SHIFT) << _dump_buf_dif_order));

	_dump_buf_done = 0;

	spin_unlock(&_dump_buf_lock);

	return nbytes;
}

static ssize_t
lpfc_debugfs_dif_err_read(struct file *file, char __user *buf,
	size_t nbytes, loff_t *ppos)
{
	struct dentry *dent = file->f_dentry;
	struct lpfc_hba *phba = file->private_data;
	char cbuf[32];
	uint64_t tmp = 0;
	int cnt = 0;

	if (dent == phba->debug_writeGuard)
		cnt = snprintf(cbuf, 32, "%u\n", phba->lpfc_injerr_wgrd_cnt);
	else if (dent == phba->debug_writeApp)
		cnt = snprintf(cbuf, 32, "%u\n", phba->lpfc_injerr_wapp_cnt);
	else if (dent == phba->debug_writeRef)
		cnt = snprintf(cbuf, 32, "%u\n", phba->lpfc_injerr_wref_cnt);
	else if (dent == phba->debug_readGuard)
		cnt = snprintf(cbuf, 32, "%u\n", phba->lpfc_injerr_rgrd_cnt);
	else if (dent == phba->debug_readApp)
		cnt = snprintf(cbuf, 32, "%u\n", phba->lpfc_injerr_rapp_cnt);
	else if (dent == phba->debug_readRef)
		cnt = snprintf(cbuf, 32, "%u\n", phba->lpfc_injerr_rref_cnt);
	else if (dent == phba->debug_InjErrNPortID)
		cnt = snprintf(cbuf, 32, "0x%06x\n", phba->lpfc_injerr_nportid);
	else if (dent == phba->debug_InjErrWWPN) {
		memcpy(&tmp, &phba->lpfc_injerr_wwpn, sizeof(struct lpfc_name));
		tmp = cpu_to_be64(tmp);
		cnt = snprintf(cbuf, 32, "0x%016llx\n", tmp);
	} else if (dent == phba->debug_InjErrLBA) {
		if (phba->lpfc_injerr_lba == (sector_t)(-1))
			cnt = snprintf(cbuf, 32, "off\n");
		else
			cnt = snprintf(cbuf, 32, "0x%llx\n",
				 (uint64_t) phba->lpfc_injerr_lba);
	} else
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			 "0547 Unknown debugfs error injection entry\n");

	return simple_read_from_buffer(buf, nbytes, ppos, &cbuf, cnt);
}

static ssize_t
lpfc_debugfs_dif_err_write(struct file *file, const char __user *buf,
	size_t nbytes, loff_t *ppos)
{
	struct dentry *dent = file->f_dentry;
	struct lpfc_hba *phba = file->private_data;
	char dstbuf[32];
	uint64_t tmp = 0;
	int size;

	memset(dstbuf, 0, 32);
	size = (nbytes < 32) ? nbytes : 32;
	if (copy_from_user(dstbuf, buf, size))
		return 0;

	if (dent == phba->debug_InjErrLBA) {
		if ((buf[0] == 'o') && (buf[1] == 'f') && (buf[2] == 'f'))
			tmp = (uint64_t)(-1);
	}

	if ((tmp == 0) && (kstrtoull(dstbuf, 0, &tmp)))
		return 0;

	if (dent == phba->debug_writeGuard)
		phba->lpfc_injerr_wgrd_cnt = (uint32_t)tmp;
	else if (dent == phba->debug_writeApp)
		phba->lpfc_injerr_wapp_cnt = (uint32_t)tmp;
	else if (dent == phba->debug_writeRef)
		phba->lpfc_injerr_wref_cnt = (uint32_t)tmp;
	else if (dent == phba->debug_readGuard)
		phba->lpfc_injerr_rgrd_cnt = (uint32_t)tmp;
	else if (dent == phba->debug_readApp)
		phba->lpfc_injerr_rapp_cnt = (uint32_t)tmp;
	else if (dent == phba->debug_readRef)
		phba->lpfc_injerr_rref_cnt = (uint32_t)tmp;
	else if (dent == phba->debug_InjErrLBA)
		phba->lpfc_injerr_lba = (sector_t)tmp;
	else if (dent == phba->debug_InjErrNPortID)
		phba->lpfc_injerr_nportid = (uint32_t)(tmp & Mask_DID);
	else if (dent == phba->debug_InjErrWWPN) {
		tmp = cpu_to_be64(tmp);
		memcpy(&phba->lpfc_injerr_wwpn, &tmp, sizeof(struct lpfc_name));
	} else
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			 "0548 Unknown debugfs error injection entry\n");

	return nbytes;
}

static int
lpfc_debugfs_dif_err_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int
lpfc_debugfs_nodelist_open(struct inode *inode, struct file *file)
{
	struct lpfc_vport *vport = inode->i_private;
	struct lpfc_debug *debug;
	int rc = -ENOMEM;

	debug = kmalloc(sizeof(*debug), GFP_KERNEL);
	if (!debug)
		goto out;

	
	debug->buffer = kmalloc(LPFC_NODELIST_SIZE, GFP_KERNEL);
	if (!debug->buffer) {
		kfree(debug);
		goto out;
	}

	debug->len = lpfc_debugfs_nodelist_data(vport, debug->buffer,
		LPFC_NODELIST_SIZE);
	file->private_data = debug;

	rc = 0;
out:
	return rc;
}

static loff_t
lpfc_debugfs_lseek(struct file *file, loff_t off, int whence)
{
	struct lpfc_debug *debug;
	loff_t pos = -1;

	debug = file->private_data;

	switch (whence) {
	case 0:
		pos = off;
		break;
	case 1:
		pos = file->f_pos + off;
		break;
	case 2:
		pos = debug->len - off;
	}
	return (pos < 0 || pos > debug->len) ? -EINVAL : (file->f_pos = pos);
}

static ssize_t
lpfc_debugfs_read(struct file *file, char __user *buf,
		  size_t nbytes, loff_t *ppos)
{
	struct lpfc_debug *debug = file->private_data;

	return simple_read_from_buffer(buf, nbytes, ppos, debug->buffer,
				       debug->len);
}

static int
lpfc_debugfs_release(struct inode *inode, struct file *file)
{
	struct lpfc_debug *debug = file->private_data;

	kfree(debug->buffer);
	kfree(debug);

	return 0;
}

static int
lpfc_debugfs_dumpDataDif_release(struct inode *inode, struct file *file)
{
	struct lpfc_debug *debug = file->private_data;

	debug->buffer = NULL;
	kfree(debug);

	return 0;
}


static int lpfc_idiag_cmd_get(const char __user *buf, size_t nbytes,
			      struct lpfc_idiag_cmd *idiag_cmd)
{
	char mybuf[64];
	char *pbuf, *step_str;
	int i;
	size_t bsize;

	
	if (!access_ok(VERIFY_READ, buf, nbytes))
		return -EFAULT;

	memset(mybuf, 0, sizeof(mybuf));
	memset(idiag_cmd, 0, sizeof(*idiag_cmd));
	bsize = min(nbytes, (sizeof(mybuf)-1));

	if (copy_from_user(mybuf, buf, bsize))
		return -EFAULT;
	pbuf = &mybuf[0];
	step_str = strsep(&pbuf, "\t ");

	
	if (!step_str)
		return -EINVAL;

	idiag_cmd->opcode = simple_strtol(step_str, NULL, 0);
	if (idiag_cmd->opcode == 0)
		return -EINVAL;

	for (i = 0; i < LPFC_IDIAG_CMD_DATA_SIZE; i++) {
		step_str = strsep(&pbuf, "\t ");
		if (!step_str)
			return i;
		idiag_cmd->data[i] = simple_strtol(step_str, NULL, 0);
	}
	return i;
}

static int
lpfc_idiag_open(struct inode *inode, struct file *file)
{
	struct lpfc_debug *debug;

	debug = kmalloc(sizeof(*debug), GFP_KERNEL);
	if (!debug)
		return -ENOMEM;

	debug->i_private = inode->i_private;
	debug->buffer = NULL;
	file->private_data = debug;

	return 0;
}

static int
lpfc_idiag_release(struct inode *inode, struct file *file)
{
	struct lpfc_debug *debug = file->private_data;

	
	kfree(debug->buffer);
	kfree(debug);

	return 0;
}

static int
lpfc_idiag_cmd_release(struct inode *inode, struct file *file)
{
	struct lpfc_debug *debug = file->private_data;

	if (debug->op == LPFC_IDIAG_OP_WR) {
		switch (idiag.cmd.opcode) {
		case LPFC_IDIAG_CMD_PCICFG_WR:
		case LPFC_IDIAG_CMD_PCICFG_ST:
		case LPFC_IDIAG_CMD_PCICFG_CL:
		case LPFC_IDIAG_CMD_QUEACC_WR:
		case LPFC_IDIAG_CMD_QUEACC_ST:
		case LPFC_IDIAG_CMD_QUEACC_CL:
			memset(&idiag, 0, sizeof(idiag));
			break;
		default:
			break;
		}
	}

	
	kfree(debug->buffer);
	kfree(debug);

	return 0;
}

static ssize_t
lpfc_idiag_pcicfg_read(struct file *file, char __user *buf, size_t nbytes,
		       loff_t *ppos)
{
	struct lpfc_debug *debug = file->private_data;
	struct lpfc_hba *phba = (struct lpfc_hba *)debug->i_private;
	int offset_label, offset, len = 0, index = LPFC_PCI_CFG_RD_SIZE;
	int where, count;
	char *pbuffer;
	struct pci_dev *pdev;
	uint32_t u32val;
	uint16_t u16val;
	uint8_t u8val;

	pdev = phba->pcidev;
	if (!pdev)
		return 0;

	
	debug->op = LPFC_IDIAG_OP_RD;

	if (!debug->buffer)
		debug->buffer = kmalloc(LPFC_PCI_CFG_SIZE, GFP_KERNEL);
	if (!debug->buffer)
		return 0;
	pbuffer = debug->buffer;

	if (*ppos)
		return 0;

	if (idiag.cmd.opcode == LPFC_IDIAG_CMD_PCICFG_RD) {
		where = idiag.cmd.data[IDIAG_PCICFG_WHERE_INDX];
		count = idiag.cmd.data[IDIAG_PCICFG_COUNT_INDX];
	} else
		return 0;

	
	switch (count) {
	case SIZE_U8: 
		pci_read_config_byte(pdev, where, &u8val);
		len += snprintf(pbuffer+len, LPFC_PCI_CFG_SIZE-len,
				"%03x: %02x\n", where, u8val);
		break;
	case SIZE_U16: 
		pci_read_config_word(pdev, where, &u16val);
		len += snprintf(pbuffer+len, LPFC_PCI_CFG_SIZE-len,
				"%03x: %04x\n", where, u16val);
		break;
	case SIZE_U32: 
		pci_read_config_dword(pdev, where, &u32val);
		len += snprintf(pbuffer+len, LPFC_PCI_CFG_SIZE-len,
				"%03x: %08x\n", where, u32val);
		break;
	case LPFC_PCI_CFG_BROWSE: 
		goto pcicfg_browse;
		break;
	default:
		
		len = 0;
		break;
	}
	return simple_read_from_buffer(buf, nbytes, ppos, pbuffer, len);

pcicfg_browse:

	
	offset_label = idiag.offset.last_rd;
	offset = offset_label;

	
	len += snprintf(pbuffer+len, LPFC_PCI_CFG_SIZE-len,
			"%03x: ", offset_label);
	while (index > 0) {
		pci_read_config_dword(pdev, offset, &u32val);
		len += snprintf(pbuffer+len, LPFC_PCI_CFG_SIZE-len,
				"%08x ", u32val);
		offset += sizeof(uint32_t);
		if (offset >= LPFC_PCI_CFG_SIZE) {
			len += snprintf(pbuffer+len,
					LPFC_PCI_CFG_SIZE-len, "\n");
			break;
		}
		index -= sizeof(uint32_t);
		if (!index)
			len += snprintf(pbuffer+len, LPFC_PCI_CFG_SIZE-len,
					"\n");
		else if (!(index % (8 * sizeof(uint32_t)))) {
			offset_label += (8 * sizeof(uint32_t));
			len += snprintf(pbuffer+len, LPFC_PCI_CFG_SIZE-len,
					"\n%03x: ", offset_label);
		}
	}

	
	if (index == 0) {
		idiag.offset.last_rd += LPFC_PCI_CFG_RD_SIZE;
		if (idiag.offset.last_rd >= LPFC_PCI_CFG_SIZE)
			idiag.offset.last_rd = 0;
	} else
		idiag.offset.last_rd = 0;

	return simple_read_from_buffer(buf, nbytes, ppos, pbuffer, len);
}

static ssize_t
lpfc_idiag_pcicfg_write(struct file *file, const char __user *buf,
			size_t nbytes, loff_t *ppos)
{
	struct lpfc_debug *debug = file->private_data;
	struct lpfc_hba *phba = (struct lpfc_hba *)debug->i_private;
	uint32_t where, value, count;
	uint32_t u32val;
	uint16_t u16val;
	uint8_t u8val;
	struct pci_dev *pdev;
	int rc;

	pdev = phba->pcidev;
	if (!pdev)
		return -EFAULT;

	
	debug->op = LPFC_IDIAG_OP_WR;

	rc = lpfc_idiag_cmd_get(buf, nbytes, &idiag.cmd);
	if (rc < 0)
		return rc;

	if (idiag.cmd.opcode == LPFC_IDIAG_CMD_PCICFG_RD) {
		
		if (rc != LPFC_PCI_CFG_RD_CMD_ARG)
			goto error_out;
		
		where = idiag.cmd.data[IDIAG_PCICFG_WHERE_INDX];
		count = idiag.cmd.data[IDIAG_PCICFG_COUNT_INDX];
		if (count == LPFC_PCI_CFG_BROWSE) {
			if (where % sizeof(uint32_t))
				goto error_out;
			
			idiag.offset.last_rd = where;
		} else if ((count != sizeof(uint8_t)) &&
			   (count != sizeof(uint16_t)) &&
			   (count != sizeof(uint32_t)))
			goto error_out;
		if (count == sizeof(uint8_t)) {
			if (where > LPFC_PCI_CFG_SIZE - sizeof(uint8_t))
				goto error_out;
			if (where % sizeof(uint8_t))
				goto error_out;
		}
		if (count == sizeof(uint16_t)) {
			if (where > LPFC_PCI_CFG_SIZE - sizeof(uint16_t))
				goto error_out;
			if (where % sizeof(uint16_t))
				goto error_out;
		}
		if (count == sizeof(uint32_t)) {
			if (where > LPFC_PCI_CFG_SIZE - sizeof(uint32_t))
				goto error_out;
			if (where % sizeof(uint32_t))
				goto error_out;
		}
	} else if (idiag.cmd.opcode == LPFC_IDIAG_CMD_PCICFG_WR ||
		   idiag.cmd.opcode == LPFC_IDIAG_CMD_PCICFG_ST ||
		   idiag.cmd.opcode == LPFC_IDIAG_CMD_PCICFG_CL) {
		
		if (rc != LPFC_PCI_CFG_WR_CMD_ARG)
			goto error_out;
		
		where = idiag.cmd.data[IDIAG_PCICFG_WHERE_INDX];
		count = idiag.cmd.data[IDIAG_PCICFG_COUNT_INDX];
		value = idiag.cmd.data[IDIAG_PCICFG_VALUE_INDX];
		
		if ((count != sizeof(uint8_t)) &&
		    (count != sizeof(uint16_t)) &&
		    (count != sizeof(uint32_t)))
			goto error_out;
		if (count == sizeof(uint8_t)) {
			if (where > LPFC_PCI_CFG_SIZE - sizeof(uint8_t))
				goto error_out;
			if (where % sizeof(uint8_t))
				goto error_out;
			if (idiag.cmd.opcode == LPFC_IDIAG_CMD_PCICFG_WR)
				pci_write_config_byte(pdev, where,
						      (uint8_t)value);
			if (idiag.cmd.opcode == LPFC_IDIAG_CMD_PCICFG_ST) {
				rc = pci_read_config_byte(pdev, where, &u8val);
				if (!rc) {
					u8val |= (uint8_t)value;
					pci_write_config_byte(pdev, where,
							      u8val);
				}
			}
			if (idiag.cmd.opcode == LPFC_IDIAG_CMD_PCICFG_CL) {
				rc = pci_read_config_byte(pdev, where, &u8val);
				if (!rc) {
					u8val &= (uint8_t)(~value);
					pci_write_config_byte(pdev, where,
							      u8val);
				}
			}
		}
		if (count == sizeof(uint16_t)) {
			if (where > LPFC_PCI_CFG_SIZE - sizeof(uint16_t))
				goto error_out;
			if (where % sizeof(uint16_t))
				goto error_out;
			if (idiag.cmd.opcode == LPFC_IDIAG_CMD_PCICFG_WR)
				pci_write_config_word(pdev, where,
						      (uint16_t)value);
			if (idiag.cmd.opcode == LPFC_IDIAG_CMD_PCICFG_ST) {
				rc = pci_read_config_word(pdev, where, &u16val);
				if (!rc) {
					u16val |= (uint16_t)value;
					pci_write_config_word(pdev, where,
							      u16val);
				}
			}
			if (idiag.cmd.opcode == LPFC_IDIAG_CMD_PCICFG_CL) {
				rc = pci_read_config_word(pdev, where, &u16val);
				if (!rc) {
					u16val &= (uint16_t)(~value);
					pci_write_config_word(pdev, where,
							      u16val);
				}
			}
		}
		if (count == sizeof(uint32_t)) {
			if (where > LPFC_PCI_CFG_SIZE - sizeof(uint32_t))
				goto error_out;
			if (where % sizeof(uint32_t))
				goto error_out;
			if (idiag.cmd.opcode == LPFC_IDIAG_CMD_PCICFG_WR)
				pci_write_config_dword(pdev, where, value);
			if (idiag.cmd.opcode == LPFC_IDIAG_CMD_PCICFG_ST) {
				rc = pci_read_config_dword(pdev, where,
							   &u32val);
				if (!rc) {
					u32val |= value;
					pci_write_config_dword(pdev, where,
							       u32val);
				}
			}
			if (idiag.cmd.opcode == LPFC_IDIAG_CMD_PCICFG_CL) {
				rc = pci_read_config_dword(pdev, where,
							   &u32val);
				if (!rc) {
					u32val &= ~value;
					pci_write_config_dword(pdev, where,
							       u32val);
				}
			}
		}
	} else
		
		goto error_out;

	return nbytes;
error_out:
	memset(&idiag, 0, sizeof(idiag));
	return -EINVAL;
}

static ssize_t
lpfc_idiag_baracc_read(struct file *file, char __user *buf, size_t nbytes,
		       loff_t *ppos)
{
	struct lpfc_debug *debug = file->private_data;
	struct lpfc_hba *phba = (struct lpfc_hba *)debug->i_private;
	int offset_label, offset, offset_run, len = 0, index;
	int bar_num, acc_range, bar_size;
	char *pbuffer;
	void __iomem *mem_mapped_bar;
	uint32_t if_type;
	struct pci_dev *pdev;
	uint32_t u32val;

	pdev = phba->pcidev;
	if (!pdev)
		return 0;

	
	debug->op = LPFC_IDIAG_OP_RD;

	if (!debug->buffer)
		debug->buffer = kmalloc(LPFC_PCI_BAR_RD_BUF_SIZE, GFP_KERNEL);
	if (!debug->buffer)
		return 0;
	pbuffer = debug->buffer;

	if (*ppos)
		return 0;

	if (idiag.cmd.opcode == LPFC_IDIAG_CMD_BARACC_RD) {
		bar_num   = idiag.cmd.data[IDIAG_BARACC_BAR_NUM_INDX];
		offset    = idiag.cmd.data[IDIAG_BARACC_OFF_SET_INDX];
		acc_range = idiag.cmd.data[IDIAG_BARACC_ACC_MOD_INDX];
		bar_size = idiag.cmd.data[IDIAG_BARACC_BAR_SZE_INDX];
	} else
		return 0;

	if (acc_range == 0)
		return 0;

	if_type = bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf);
	if (if_type == LPFC_SLI_INTF_IF_TYPE_0) {
		if (bar_num == IDIAG_BARACC_BAR_0)
			mem_mapped_bar = phba->sli4_hba.conf_regs_memmap_p;
		else if (bar_num == IDIAG_BARACC_BAR_1)
			mem_mapped_bar = phba->sli4_hba.ctrl_regs_memmap_p;
		else if (bar_num == IDIAG_BARACC_BAR_2)
			mem_mapped_bar = phba->sli4_hba.drbl_regs_memmap_p;
		else
			return 0;
	} else if (if_type == LPFC_SLI_INTF_IF_TYPE_2) {
		if (bar_num == IDIAG_BARACC_BAR_0)
			mem_mapped_bar = phba->sli4_hba.conf_regs_memmap_p;
		else
			return 0;
	} else
		return 0;

	
	if (acc_range == SINGLE_WORD) {
		offset_run = offset;
		u32val = readl(mem_mapped_bar + offset_run);
		len += snprintf(pbuffer+len, LPFC_PCI_BAR_RD_BUF_SIZE-len,
				"%05x: %08x\n", offset_run, u32val);
	} else
		goto baracc_browse;

	return simple_read_from_buffer(buf, nbytes, ppos, pbuffer, len);

baracc_browse:

	
	offset_label = idiag.offset.last_rd;
	offset_run = offset_label;

	
	len += snprintf(pbuffer+len, LPFC_PCI_BAR_RD_BUF_SIZE-len,
			"%05x: ", offset_label);
	index = LPFC_PCI_BAR_RD_SIZE;
	while (index > 0) {
		u32val = readl(mem_mapped_bar + offset_run);
		len += snprintf(pbuffer+len, LPFC_PCI_BAR_RD_BUF_SIZE-len,
				"%08x ", u32val);
		offset_run += sizeof(uint32_t);
		if (acc_range == LPFC_PCI_BAR_BROWSE) {
			if (offset_run >= bar_size) {
				len += snprintf(pbuffer+len,
					LPFC_PCI_BAR_RD_BUF_SIZE-len, "\n");
				break;
			}
		} else {
			if (offset_run >= offset +
			    (acc_range * sizeof(uint32_t))) {
				len += snprintf(pbuffer+len,
					LPFC_PCI_BAR_RD_BUF_SIZE-len, "\n");
				break;
			}
		}
		index -= sizeof(uint32_t);
		if (!index)
			len += snprintf(pbuffer+len,
					LPFC_PCI_BAR_RD_BUF_SIZE-len, "\n");
		else if (!(index % (8 * sizeof(uint32_t)))) {
			offset_label += (8 * sizeof(uint32_t));
			len += snprintf(pbuffer+len,
					LPFC_PCI_BAR_RD_BUF_SIZE-len,
					"\n%05x: ", offset_label);
		}
	}

	
	if (index == 0) {
		idiag.offset.last_rd += LPFC_PCI_BAR_RD_SIZE;
		if (acc_range == LPFC_PCI_BAR_BROWSE) {
			if (idiag.offset.last_rd >= bar_size)
				idiag.offset.last_rd = 0;
		} else {
			if (offset_run >= offset +
			    (acc_range * sizeof(uint32_t)))
				idiag.offset.last_rd = offset;
		}
	} else {
		if (acc_range == LPFC_PCI_BAR_BROWSE)
			idiag.offset.last_rd = 0;
		else
			idiag.offset.last_rd = offset;
	}

	return simple_read_from_buffer(buf, nbytes, ppos, pbuffer, len);
}

static ssize_t
lpfc_idiag_baracc_write(struct file *file, const char __user *buf,
			size_t nbytes, loff_t *ppos)
{
	struct lpfc_debug *debug = file->private_data;
	struct lpfc_hba *phba = (struct lpfc_hba *)debug->i_private;
	uint32_t bar_num, bar_size, offset, value, acc_range;
	struct pci_dev *pdev;
	void __iomem *mem_mapped_bar;
	uint32_t if_type;
	uint32_t u32val;
	int rc;

	pdev = phba->pcidev;
	if (!pdev)
		return -EFAULT;

	
	debug->op = LPFC_IDIAG_OP_WR;

	rc = lpfc_idiag_cmd_get(buf, nbytes, &idiag.cmd);
	if (rc < 0)
		return rc;

	if_type = bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf);
	bar_num = idiag.cmd.data[IDIAG_BARACC_BAR_NUM_INDX];

	if (if_type == LPFC_SLI_INTF_IF_TYPE_0) {
		if ((bar_num != IDIAG_BARACC_BAR_0) &&
		    (bar_num != IDIAG_BARACC_BAR_1) &&
		    (bar_num != IDIAG_BARACC_BAR_2))
			goto error_out;
	} else if (if_type == LPFC_SLI_INTF_IF_TYPE_2) {
		if (bar_num != IDIAG_BARACC_BAR_0)
			goto error_out;
	} else
		goto error_out;

	if (if_type == LPFC_SLI_INTF_IF_TYPE_0) {
		if (bar_num == IDIAG_BARACC_BAR_0) {
			idiag.cmd.data[IDIAG_BARACC_BAR_SZE_INDX] =
				LPFC_PCI_IF0_BAR0_SIZE;
			mem_mapped_bar = phba->sli4_hba.conf_regs_memmap_p;
		} else if (bar_num == IDIAG_BARACC_BAR_1) {
			idiag.cmd.data[IDIAG_BARACC_BAR_SZE_INDX] =
				LPFC_PCI_IF0_BAR1_SIZE;
			mem_mapped_bar = phba->sli4_hba.ctrl_regs_memmap_p;
		} else if (bar_num == IDIAG_BARACC_BAR_2) {
			idiag.cmd.data[IDIAG_BARACC_BAR_SZE_INDX] =
				LPFC_PCI_IF0_BAR2_SIZE;
			mem_mapped_bar = phba->sli4_hba.drbl_regs_memmap_p;
		} else
			goto error_out;
	} else if (if_type == LPFC_SLI_INTF_IF_TYPE_2) {
		if (bar_num == IDIAG_BARACC_BAR_0) {
			idiag.cmd.data[IDIAG_BARACC_BAR_SZE_INDX] =
				LPFC_PCI_IF2_BAR0_SIZE;
			mem_mapped_bar = phba->sli4_hba.conf_regs_memmap_p;
		} else
			goto error_out;
	} else
		goto error_out;

	offset = idiag.cmd.data[IDIAG_BARACC_OFF_SET_INDX];
	if (offset % sizeof(uint32_t))
		goto error_out;

	bar_size = idiag.cmd.data[IDIAG_BARACC_BAR_SZE_INDX];
	if (idiag.cmd.opcode == LPFC_IDIAG_CMD_BARACC_RD) {
		
		if (rc != LPFC_PCI_BAR_RD_CMD_ARG)
			goto error_out;
		acc_range = idiag.cmd.data[IDIAG_BARACC_ACC_MOD_INDX];
		if (acc_range == LPFC_PCI_BAR_BROWSE) {
			if (offset > bar_size - sizeof(uint32_t))
				goto error_out;
			
			idiag.offset.last_rd = offset;
		} else if (acc_range > SINGLE_WORD) {
			if (offset + acc_range * sizeof(uint32_t) > bar_size)
				goto error_out;
			
			idiag.offset.last_rd = offset;
		} else if (acc_range != SINGLE_WORD)
			goto error_out;
	} else if (idiag.cmd.opcode == LPFC_IDIAG_CMD_BARACC_WR ||
		   idiag.cmd.opcode == LPFC_IDIAG_CMD_BARACC_ST ||
		   idiag.cmd.opcode == LPFC_IDIAG_CMD_BARACC_CL) {
		
		if (rc != LPFC_PCI_BAR_WR_CMD_ARG)
			goto error_out;
		
		acc_range = SINGLE_WORD;
		value = idiag.cmd.data[IDIAG_BARACC_REG_VAL_INDX];
		if (idiag.cmd.opcode == LPFC_IDIAG_CMD_BARACC_WR) {
			writel(value, mem_mapped_bar + offset);
			readl(mem_mapped_bar + offset);
		}
		if (idiag.cmd.opcode == LPFC_IDIAG_CMD_BARACC_ST) {
			u32val = readl(mem_mapped_bar + offset);
			u32val |= value;
			writel(u32val, mem_mapped_bar + offset);
			readl(mem_mapped_bar + offset);
		}
		if (idiag.cmd.opcode == LPFC_IDIAG_CMD_BARACC_CL) {
			u32val = readl(mem_mapped_bar + offset);
			u32val &= ~value;
			writel(u32val, mem_mapped_bar + offset);
			readl(mem_mapped_bar + offset);
		}
	} else
		
		goto error_out;

	return nbytes;
error_out:
	memset(&idiag, 0, sizeof(idiag));
	return -EINVAL;
}

static ssize_t
lpfc_idiag_queinfo_read(struct file *file, char __user *buf, size_t nbytes,
			loff_t *ppos)
{
	struct lpfc_debug *debug = file->private_data;
	struct lpfc_hba *phba = (struct lpfc_hba *)debug->i_private;
	int len = 0, fcp_qidx;
	char *pbuffer;

	if (!debug->buffer)
		debug->buffer = kmalloc(LPFC_QUE_INFO_GET_BUF_SIZE, GFP_KERNEL);
	if (!debug->buffer)
		return 0;
	pbuffer = debug->buffer;

	if (*ppos)
		return 0;

	
	len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"Slow-path EQ information:\n");
	if (phba->sli4_hba.sp_eq) {
		len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"\tEQID[%02d], "
			"QE-COUNT[%04d], QE-SIZE[%04d], "
			"HOST-INDEX[%04d], PORT-INDEX[%04d]\n\n",
			phba->sli4_hba.sp_eq->queue_id,
			phba->sli4_hba.sp_eq->entry_count,
			phba->sli4_hba.sp_eq->entry_size,
			phba->sli4_hba.sp_eq->host_index,
			phba->sli4_hba.sp_eq->hba_index);
	}

	
	len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"Fast-path EQ information:\n");
	if (phba->sli4_hba.fp_eq) {
		for (fcp_qidx = 0; fcp_qidx < phba->cfg_fcp_eq_count;
		     fcp_qidx++) {
			if (phba->sli4_hba.fp_eq[fcp_qidx]) {
				len += snprintf(pbuffer+len,
					LPFC_QUE_INFO_GET_BUF_SIZE-len,
				"\tEQID[%02d], "
				"QE-COUNT[%04d], QE-SIZE[%04d], "
				"HOST-INDEX[%04d], PORT-INDEX[%04d]\n",
				phba->sli4_hba.fp_eq[fcp_qidx]->queue_id,
				phba->sli4_hba.fp_eq[fcp_qidx]->entry_count,
				phba->sli4_hba.fp_eq[fcp_qidx]->entry_size,
				phba->sli4_hba.fp_eq[fcp_qidx]->host_index,
				phba->sli4_hba.fp_eq[fcp_qidx]->hba_index);
			}
		}
	}
	len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len, "\n");

	
	len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"Slow-path MBX CQ information:\n");
	if (phba->sli4_hba.mbx_cq) {
		len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"Associated EQID[%02d]:\n",
			phba->sli4_hba.mbx_cq->assoc_qid);
		len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"\tCQID[%02d], "
			"QE-COUNT[%04d], QE-SIZE[%04d], "
			"HOST-INDEX[%04d], PORT-INDEX[%04d]\n\n",
			phba->sli4_hba.mbx_cq->queue_id,
			phba->sli4_hba.mbx_cq->entry_count,
			phba->sli4_hba.mbx_cq->entry_size,
			phba->sli4_hba.mbx_cq->host_index,
			phba->sli4_hba.mbx_cq->hba_index);
	}

	
	len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"Slow-path ELS CQ information:\n");
	if (phba->sli4_hba.els_cq) {
		len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"Associated EQID[%02d]:\n",
			phba->sli4_hba.els_cq->assoc_qid);
		len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"\tCQID [%02d], "
			"QE-COUNT[%04d], QE-SIZE[%04d], "
			"HOST-INDEX[%04d], PORT-INDEX[%04d]\n\n",
			phba->sli4_hba.els_cq->queue_id,
			phba->sli4_hba.els_cq->entry_count,
			phba->sli4_hba.els_cq->entry_size,
			phba->sli4_hba.els_cq->host_index,
			phba->sli4_hba.els_cq->hba_index);
	}

	
	len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"Fast-path FCP CQ information:\n");
	fcp_qidx = 0;
	if (phba->sli4_hba.fcp_cq) {
		do {
			if (phba->sli4_hba.fcp_cq[fcp_qidx]) {
				len += snprintf(pbuffer+len,
					LPFC_QUE_INFO_GET_BUF_SIZE-len,
				"Associated EQID[%02d]:\n",
				phba->sli4_hba.fcp_cq[fcp_qidx]->assoc_qid);
				len += snprintf(pbuffer+len,
					LPFC_QUE_INFO_GET_BUF_SIZE-len,
				"\tCQID[%02d], "
				"QE-COUNT[%04d], QE-SIZE[%04d], "
				"HOST-INDEX[%04d], PORT-INDEX[%04d]\n",
				phba->sli4_hba.fcp_cq[fcp_qidx]->queue_id,
				phba->sli4_hba.fcp_cq[fcp_qidx]->entry_count,
				phba->sli4_hba.fcp_cq[fcp_qidx]->entry_size,
				phba->sli4_hba.fcp_cq[fcp_qidx]->host_index,
				phba->sli4_hba.fcp_cq[fcp_qidx]->hba_index);
			}
		} while (++fcp_qidx < phba->cfg_fcp_eq_count);
		len += snprintf(pbuffer+len,
				LPFC_QUE_INFO_GET_BUF_SIZE-len, "\n");
	}

	
	len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"Slow-path MBX MQ information:\n");
	if (phba->sli4_hba.mbx_wq) {
		len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"Associated CQID[%02d]:\n",
			phba->sli4_hba.mbx_wq->assoc_qid);
		len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"\tWQID[%02d], "
			"QE-COUNT[%04d], QE-SIZE[%04d], "
			"HOST-INDEX[%04d], PORT-INDEX[%04d]\n\n",
			phba->sli4_hba.mbx_wq->queue_id,
			phba->sli4_hba.mbx_wq->entry_count,
			phba->sli4_hba.mbx_wq->entry_size,
			phba->sli4_hba.mbx_wq->host_index,
			phba->sli4_hba.mbx_wq->hba_index);
	}

	
	len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"Slow-path ELS WQ information:\n");
	if (phba->sli4_hba.els_wq) {
		len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"Associated CQID[%02d]:\n",
			phba->sli4_hba.els_wq->assoc_qid);
		len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"\tWQID[%02d], "
			"QE-COUNT[%04d], QE-SIZE[%04d], "
			"HOST-INDEX[%04d], PORT-INDEX[%04d]\n\n",
			phba->sli4_hba.els_wq->queue_id,
			phba->sli4_hba.els_wq->entry_count,
			phba->sli4_hba.els_wq->entry_size,
			phba->sli4_hba.els_wq->host_index,
			phba->sli4_hba.els_wq->hba_index);
	}

	
	len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"Fast-path FCP WQ information:\n");
	if (phba->sli4_hba.fcp_wq) {
		for (fcp_qidx = 0; fcp_qidx < phba->cfg_fcp_wq_count;
		     fcp_qidx++) {
			if (!phba->sli4_hba.fcp_wq[fcp_qidx])
				continue;
			len += snprintf(pbuffer+len,
					LPFC_QUE_INFO_GET_BUF_SIZE-len,
				"Associated CQID[%02d]:\n",
				phba->sli4_hba.fcp_wq[fcp_qidx]->assoc_qid);
			len += snprintf(pbuffer+len,
					LPFC_QUE_INFO_GET_BUF_SIZE-len,
				"\tWQID[%02d], "
				"QE-COUNT[%04d], WQE-SIZE[%04d], "
				"HOST-INDEX[%04d], PORT-INDEX[%04d]\n",
				phba->sli4_hba.fcp_wq[fcp_qidx]->queue_id,
				phba->sli4_hba.fcp_wq[fcp_qidx]->entry_count,
				phba->sli4_hba.fcp_wq[fcp_qidx]->entry_size,
				phba->sli4_hba.fcp_wq[fcp_qidx]->host_index,
				phba->sli4_hba.fcp_wq[fcp_qidx]->hba_index);
		}
		len += snprintf(pbuffer+len,
				LPFC_QUE_INFO_GET_BUF_SIZE-len, "\n");
	}

	
	len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"Slow-path RQ information:\n");
	if (phba->sli4_hba.hdr_rq && phba->sli4_hba.dat_rq) {
		len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"Associated CQID[%02d]:\n",
			phba->sli4_hba.hdr_rq->assoc_qid);
		len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"\tHQID[%02d], "
			"QE-COUNT[%04d], QE-SIZE[%04d], "
			"HOST-INDEX[%04d], PORT-INDEX[%04d]\n",
			phba->sli4_hba.hdr_rq->queue_id,
			phba->sli4_hba.hdr_rq->entry_count,
			phba->sli4_hba.hdr_rq->entry_size,
			phba->sli4_hba.hdr_rq->host_index,
			phba->sli4_hba.hdr_rq->hba_index);
		len += snprintf(pbuffer+len, LPFC_QUE_INFO_GET_BUF_SIZE-len,
			"\tDQID[%02d], "
			"QE-COUNT[%04d], QE-SIZE[%04d], "
			"HOST-INDEX[%04d], PORT-INDEX[%04d]\n",
			phba->sli4_hba.dat_rq->queue_id,
			phba->sli4_hba.dat_rq->entry_count,
			phba->sli4_hba.dat_rq->entry_size,
			phba->sli4_hba.dat_rq->host_index,
			phba->sli4_hba.dat_rq->hba_index);
	}
	return simple_read_from_buffer(buf, nbytes, ppos, pbuffer, len);
}

static int
lpfc_idiag_que_param_check(struct lpfc_queue *q, int index, int count)
{
	
	if ((count != 1) && (count != LPFC_QUE_ACC_BROWSE))
		return -EINVAL;
	if (index > q->entry_count - 1)
		return -EINVAL;
	return 0;
}

static int
lpfc_idiag_queacc_read_qe(char *pbuffer, int len, struct lpfc_queue *pque,
			  uint32_t index)
{
	int offset, esize;
	uint32_t *pentry;

	if (!pbuffer || !pque)
		return 0;

	esize = pque->entry_size;
	len += snprintf(pbuffer+len, LPFC_QUE_ACC_BUF_SIZE-len,
			"QE-INDEX[%04d]:\n", index);

	offset = 0;
	pentry = pque->qe[index].address;
	while (esize > 0) {
		len += snprintf(pbuffer+len, LPFC_QUE_ACC_BUF_SIZE-len,
				"%08x ", *pentry);
		pentry++;
		offset += sizeof(uint32_t);
		esize -= sizeof(uint32_t);
		if (esize > 0 && !(offset % (4 * sizeof(uint32_t))))
			len += snprintf(pbuffer+len,
					LPFC_QUE_ACC_BUF_SIZE-len, "\n");
	}
	len += snprintf(pbuffer+len, LPFC_QUE_ACC_BUF_SIZE-len, "\n");

	return len;
}

static ssize_t
lpfc_idiag_queacc_read(struct file *file, char __user *buf, size_t nbytes,
		       loff_t *ppos)
{
	struct lpfc_debug *debug = file->private_data;
	uint32_t last_index, index, count;
	struct lpfc_queue *pque = NULL;
	char *pbuffer;
	int len = 0;

	
	debug->op = LPFC_IDIAG_OP_RD;

	if (!debug->buffer)
		debug->buffer = kmalloc(LPFC_QUE_ACC_BUF_SIZE, GFP_KERNEL);
	if (!debug->buffer)
		return 0;
	pbuffer = debug->buffer;

	if (*ppos)
		return 0;

	if (idiag.cmd.opcode == LPFC_IDIAG_CMD_QUEACC_RD) {
		index = idiag.cmd.data[IDIAG_QUEACC_INDEX_INDX];
		count = idiag.cmd.data[IDIAG_QUEACC_COUNT_INDX];
		pque = (struct lpfc_queue *)idiag.ptr_private;
	} else
		return 0;

	
	if (count == LPFC_QUE_ACC_BROWSE)
		goto que_browse;

	
	len = lpfc_idiag_queacc_read_qe(pbuffer, len, pque, index);

	return simple_read_from_buffer(buf, nbytes, ppos, pbuffer, len);

que_browse:

	
	last_index = idiag.offset.last_rd;
	index = last_index;

	while (len < LPFC_QUE_ACC_SIZE - pque->entry_size) {
		len = lpfc_idiag_queacc_read_qe(pbuffer, len, pque, index);
		index++;
		if (index > pque->entry_count - 1)
			break;
	}

	
	if (index > pque->entry_count - 1)
		index = 0;
	idiag.offset.last_rd = index;

	return simple_read_from_buffer(buf, nbytes, ppos, pbuffer, len);
}

static ssize_t
lpfc_idiag_queacc_write(struct file *file, const char __user *buf,
			size_t nbytes, loff_t *ppos)
{
	struct lpfc_debug *debug = file->private_data;
	struct lpfc_hba *phba = (struct lpfc_hba *)debug->i_private;
	uint32_t qidx, quetp, queid, index, count, offset, value;
	uint32_t *pentry;
	struct lpfc_queue *pque;
	int rc;

	
	debug->op = LPFC_IDIAG_OP_WR;

	rc = lpfc_idiag_cmd_get(buf, nbytes, &idiag.cmd);
	if (rc < 0)
		return rc;

	
	quetp  = idiag.cmd.data[IDIAG_QUEACC_QUETP_INDX];
	queid  = idiag.cmd.data[IDIAG_QUEACC_QUEID_INDX];
	index  = idiag.cmd.data[IDIAG_QUEACC_INDEX_INDX];
	count  = idiag.cmd.data[IDIAG_QUEACC_COUNT_INDX];
	offset = idiag.cmd.data[IDIAG_QUEACC_OFFST_INDX];
	value  = idiag.cmd.data[IDIAG_QUEACC_VALUE_INDX];

	
	if (idiag.cmd.opcode == LPFC_IDIAG_CMD_QUEACC_WR ||
	    idiag.cmd.opcode == LPFC_IDIAG_CMD_QUEACC_ST ||
	    idiag.cmd.opcode == LPFC_IDIAG_CMD_QUEACC_CL) {
		if (rc != LPFC_QUE_ACC_WR_CMD_ARG)
			goto error_out;
		if (count != 1)
			goto error_out;
	} else if (idiag.cmd.opcode == LPFC_IDIAG_CMD_QUEACC_RD) {
		if (rc != LPFC_QUE_ACC_RD_CMD_ARG)
			goto error_out;
	} else
		goto error_out;

	switch (quetp) {
	case LPFC_IDIAG_EQ:
		
		if (phba->sli4_hba.sp_eq &&
		    phba->sli4_hba.sp_eq->queue_id == queid) {
			
			rc = lpfc_idiag_que_param_check(
					phba->sli4_hba.sp_eq, index, count);
			if (rc)
				goto error_out;
			idiag.ptr_private = phba->sli4_hba.sp_eq;
			goto pass_check;
		}
		
		if (phba->sli4_hba.fp_eq) {
			for (qidx = 0; qidx < phba->cfg_fcp_eq_count; qidx++) {
				if (phba->sli4_hba.fp_eq[qidx] &&
				    phba->sli4_hba.fp_eq[qidx]->queue_id ==
				    queid) {
					
					rc = lpfc_idiag_que_param_check(
						phba->sli4_hba.fp_eq[qidx],
						index, count);
					if (rc)
						goto error_out;
					idiag.ptr_private =
						phba->sli4_hba.fp_eq[qidx];
					goto pass_check;
				}
			}
		}
		goto error_out;
		break;
	case LPFC_IDIAG_CQ:
		
		if (phba->sli4_hba.mbx_cq &&
		    phba->sli4_hba.mbx_cq->queue_id == queid) {
			
			rc = lpfc_idiag_que_param_check(
					phba->sli4_hba.mbx_cq, index, count);
			if (rc)
				goto error_out;
			idiag.ptr_private = phba->sli4_hba.mbx_cq;
			goto pass_check;
		}
		
		if (phba->sli4_hba.els_cq &&
		    phba->sli4_hba.els_cq->queue_id == queid) {
			
			rc = lpfc_idiag_que_param_check(
					phba->sli4_hba.els_cq, index, count);
			if (rc)
				goto error_out;
			idiag.ptr_private = phba->sli4_hba.els_cq;
			goto pass_check;
		}
		
		if (phba->sli4_hba.fcp_cq) {
			qidx = 0;
			do {
				if (phba->sli4_hba.fcp_cq[qidx] &&
				    phba->sli4_hba.fcp_cq[qidx]->queue_id ==
				    queid) {
					
					rc = lpfc_idiag_que_param_check(
						phba->sli4_hba.fcp_cq[qidx],
						index, count);
					if (rc)
						goto error_out;
					idiag.ptr_private =
						phba->sli4_hba.fcp_cq[qidx];
					goto pass_check;
				}
			} while (++qidx < phba->cfg_fcp_eq_count);
		}
		goto error_out;
		break;
	case LPFC_IDIAG_MQ:
		
		if (phba->sli4_hba.mbx_wq &&
		    phba->sli4_hba.mbx_wq->queue_id == queid) {
			
			rc = lpfc_idiag_que_param_check(
					phba->sli4_hba.mbx_wq, index, count);
			if (rc)
				goto error_out;
			idiag.ptr_private = phba->sli4_hba.mbx_wq;
			goto pass_check;
		}
		goto error_out;
		break;
	case LPFC_IDIAG_WQ:
		
		if (phba->sli4_hba.els_wq &&
		    phba->sli4_hba.els_wq->queue_id == queid) {
			
			rc = lpfc_idiag_que_param_check(
					phba->sli4_hba.els_wq, index, count);
			if (rc)
				goto error_out;
			idiag.ptr_private = phba->sli4_hba.els_wq;
			goto pass_check;
		}
		
		if (phba->sli4_hba.fcp_wq) {
			for (qidx = 0; qidx < phba->cfg_fcp_wq_count; qidx++) {
				if (!phba->sli4_hba.fcp_wq[qidx])
					continue;
				if (phba->sli4_hba.fcp_wq[qidx]->queue_id ==
				    queid) {
					
					rc = lpfc_idiag_que_param_check(
						phba->sli4_hba.fcp_wq[qidx],
						index, count);
					if (rc)
						goto error_out;
					idiag.ptr_private =
						phba->sli4_hba.fcp_wq[qidx];
					goto pass_check;
				}
			}
		}
		goto error_out;
		break;
	case LPFC_IDIAG_RQ:
		
		if (phba->sli4_hba.hdr_rq &&
		    phba->sli4_hba.hdr_rq->queue_id == queid) {
			
			rc = lpfc_idiag_que_param_check(
					phba->sli4_hba.hdr_rq, index, count);
			if (rc)
				goto error_out;
			idiag.ptr_private = phba->sli4_hba.hdr_rq;
			goto pass_check;
		}
		
		if (phba->sli4_hba.dat_rq &&
		    phba->sli4_hba.dat_rq->queue_id == queid) {
			
			rc = lpfc_idiag_que_param_check(
					phba->sli4_hba.dat_rq, index, count);
			if (rc)
				goto error_out;
			idiag.ptr_private = phba->sli4_hba.dat_rq;
			goto pass_check;
		}
		goto error_out;
		break;
	default:
		goto error_out;
		break;
	}

pass_check:

	if (idiag.cmd.opcode == LPFC_IDIAG_CMD_QUEACC_RD) {
		if (count == LPFC_QUE_ACC_BROWSE)
			idiag.offset.last_rd = index;
	}

	if (idiag.cmd.opcode == LPFC_IDIAG_CMD_QUEACC_WR ||
	    idiag.cmd.opcode == LPFC_IDIAG_CMD_QUEACC_ST ||
	    idiag.cmd.opcode == LPFC_IDIAG_CMD_QUEACC_CL) {
		
		pque = (struct lpfc_queue *)idiag.ptr_private;
		if (offset > pque->entry_size/sizeof(uint32_t) - 1)
			goto error_out;
		pentry = pque->qe[index].address;
		pentry += offset;
		if (idiag.cmd.opcode == LPFC_IDIAG_CMD_QUEACC_WR)
			*pentry = value;
		if (idiag.cmd.opcode == LPFC_IDIAG_CMD_QUEACC_ST)
			*pentry |= value;
		if (idiag.cmd.opcode == LPFC_IDIAG_CMD_QUEACC_CL)
			*pentry &= ~value;
	}
	return nbytes;

error_out:
	
	memset(&idiag, 0, sizeof(idiag));
	return -EINVAL;
}

static int
lpfc_idiag_drbacc_read_reg(struct lpfc_hba *phba, char *pbuffer,
			   int len, uint32_t drbregid)
{

	if (!pbuffer)
		return 0;

	switch (drbregid) {
	case LPFC_DRB_EQCQ:
		len += snprintf(pbuffer+len, LPFC_DRB_ACC_BUF_SIZE-len,
				"EQCQ-DRB-REG: 0x%08x\n",
				readl(phba->sli4_hba.EQCQDBregaddr));
		break;
	case LPFC_DRB_MQ:
		len += snprintf(pbuffer+len, LPFC_DRB_ACC_BUF_SIZE-len,
				"MQ-DRB-REG:   0x%08x\n",
				readl(phba->sli4_hba.MQDBregaddr));
		break;
	case LPFC_DRB_WQ:
		len += snprintf(pbuffer+len, LPFC_DRB_ACC_BUF_SIZE-len,
				"WQ-DRB-REG:   0x%08x\n",
				readl(phba->sli4_hba.WQDBregaddr));
		break;
	case LPFC_DRB_RQ:
		len += snprintf(pbuffer+len, LPFC_DRB_ACC_BUF_SIZE-len,
				"RQ-DRB-REG:   0x%08x\n",
				readl(phba->sli4_hba.RQDBregaddr));
		break;
	default:
		break;
	}

	return len;
}

static ssize_t
lpfc_idiag_drbacc_read(struct file *file, char __user *buf, size_t nbytes,
		       loff_t *ppos)
{
	struct lpfc_debug *debug = file->private_data;
	struct lpfc_hba *phba = (struct lpfc_hba *)debug->i_private;
	uint32_t drb_reg_id, i;
	char *pbuffer;
	int len = 0;

	
	debug->op = LPFC_IDIAG_OP_RD;

	if (!debug->buffer)
		debug->buffer = kmalloc(LPFC_DRB_ACC_BUF_SIZE, GFP_KERNEL);
	if (!debug->buffer)
		return 0;
	pbuffer = debug->buffer;

	if (*ppos)
		return 0;

	if (idiag.cmd.opcode == LPFC_IDIAG_CMD_DRBACC_RD)
		drb_reg_id = idiag.cmd.data[IDIAG_DRBACC_REGID_INDX];
	else
		return 0;

	if (drb_reg_id == LPFC_DRB_ACC_ALL)
		for (i = 1; i <= LPFC_DRB_MAX; i++)
			len = lpfc_idiag_drbacc_read_reg(phba,
							 pbuffer, len, i);
	else
		len = lpfc_idiag_drbacc_read_reg(phba,
						 pbuffer, len, drb_reg_id);

	return simple_read_from_buffer(buf, nbytes, ppos, pbuffer, len);
}

static ssize_t
lpfc_idiag_drbacc_write(struct file *file, const char __user *buf,
			size_t nbytes, loff_t *ppos)
{
	struct lpfc_debug *debug = file->private_data;
	struct lpfc_hba *phba = (struct lpfc_hba *)debug->i_private;
	uint32_t drb_reg_id, value, reg_val = 0;
	void __iomem *drb_reg;
	int rc;

	
	debug->op = LPFC_IDIAG_OP_WR;

	rc = lpfc_idiag_cmd_get(buf, nbytes, &idiag.cmd);
	if (rc < 0)
		return rc;

	
	drb_reg_id = idiag.cmd.data[IDIAG_DRBACC_REGID_INDX];
	value = idiag.cmd.data[IDIAG_DRBACC_VALUE_INDX];

	if (idiag.cmd.opcode == LPFC_IDIAG_CMD_DRBACC_WR ||
	    idiag.cmd.opcode == LPFC_IDIAG_CMD_DRBACC_ST ||
	    idiag.cmd.opcode == LPFC_IDIAG_CMD_DRBACC_CL) {
		if (rc != LPFC_DRB_ACC_WR_CMD_ARG)
			goto error_out;
		if (drb_reg_id > LPFC_DRB_MAX)
			goto error_out;
	} else if (idiag.cmd.opcode == LPFC_IDIAG_CMD_DRBACC_RD) {
		if (rc != LPFC_DRB_ACC_RD_CMD_ARG)
			goto error_out;
		if ((drb_reg_id > LPFC_DRB_MAX) &&
		    (drb_reg_id != LPFC_DRB_ACC_ALL))
			goto error_out;
	} else
		goto error_out;

	
	if (idiag.cmd.opcode == LPFC_IDIAG_CMD_DRBACC_WR ||
	    idiag.cmd.opcode == LPFC_IDIAG_CMD_DRBACC_ST ||
	    idiag.cmd.opcode == LPFC_IDIAG_CMD_DRBACC_CL) {
		switch (drb_reg_id) {
		case LPFC_DRB_EQCQ:
			drb_reg = phba->sli4_hba.EQCQDBregaddr;
			break;
		case LPFC_DRB_MQ:
			drb_reg = phba->sli4_hba.MQDBregaddr;
			break;
		case LPFC_DRB_WQ:
			drb_reg = phba->sli4_hba.WQDBregaddr;
			break;
		case LPFC_DRB_RQ:
			drb_reg = phba->sli4_hba.RQDBregaddr;
			break;
		default:
			goto error_out;
		}

		if (idiag.cmd.opcode == LPFC_IDIAG_CMD_DRBACC_WR)
			reg_val = value;
		if (idiag.cmd.opcode == LPFC_IDIAG_CMD_DRBACC_ST) {
			reg_val = readl(drb_reg);
			reg_val |= value;
		}
		if (idiag.cmd.opcode == LPFC_IDIAG_CMD_DRBACC_CL) {
			reg_val = readl(drb_reg);
			reg_val &= ~value;
		}
		writel(reg_val, drb_reg);
		readl(drb_reg); 
	}
	return nbytes;

error_out:
	
	memset(&idiag, 0, sizeof(idiag));
	return -EINVAL;
}

static int
lpfc_idiag_ctlacc_read_reg(struct lpfc_hba *phba, char *pbuffer,
			   int len, uint32_t ctlregid)
{

	if (!pbuffer)
		return 0;

	switch (ctlregid) {
	case LPFC_CTL_PORT_SEM:
		len += snprintf(pbuffer+len, LPFC_CTL_ACC_BUF_SIZE-len,
				"Port SemReg:   0x%08x\n",
				readl(phba->sli4_hba.conf_regs_memmap_p +
				      LPFC_CTL_PORT_SEM_OFFSET));
		break;
	case LPFC_CTL_PORT_STA:
		len += snprintf(pbuffer+len, LPFC_CTL_ACC_BUF_SIZE-len,
				"Port StaReg:   0x%08x\n",
				readl(phba->sli4_hba.conf_regs_memmap_p +
				      LPFC_CTL_PORT_STA_OFFSET));
		break;
	case LPFC_CTL_PORT_CTL:
		len += snprintf(pbuffer+len, LPFC_CTL_ACC_BUF_SIZE-len,
				"Port CtlReg:   0x%08x\n",
				readl(phba->sli4_hba.conf_regs_memmap_p +
				      LPFC_CTL_PORT_CTL_OFFSET));
		break;
	case LPFC_CTL_PORT_ER1:
		len += snprintf(pbuffer+len, LPFC_CTL_ACC_BUF_SIZE-len,
				"Port Er1Reg:   0x%08x\n",
				readl(phba->sli4_hba.conf_regs_memmap_p +
				      LPFC_CTL_PORT_ER1_OFFSET));
		break;
	case LPFC_CTL_PORT_ER2:
		len += snprintf(pbuffer+len, LPFC_CTL_ACC_BUF_SIZE-len,
				"Port Er2Reg:   0x%08x\n",
				readl(phba->sli4_hba.conf_regs_memmap_p +
				      LPFC_CTL_PORT_ER2_OFFSET));
		break;
	case LPFC_CTL_PDEV_CTL:
		len += snprintf(pbuffer+len, LPFC_CTL_ACC_BUF_SIZE-len,
				"PDev CtlReg:   0x%08x\n",
				readl(phba->sli4_hba.conf_regs_memmap_p +
				      LPFC_CTL_PDEV_CTL_OFFSET));
		break;
	default:
		break;
	}
	return len;
}

static ssize_t
lpfc_idiag_ctlacc_read(struct file *file, char __user *buf, size_t nbytes,
		       loff_t *ppos)
{
	struct lpfc_debug *debug = file->private_data;
	struct lpfc_hba *phba = (struct lpfc_hba *)debug->i_private;
	uint32_t ctl_reg_id, i;
	char *pbuffer;
	int len = 0;

	
	debug->op = LPFC_IDIAG_OP_RD;

	if (!debug->buffer)
		debug->buffer = kmalloc(LPFC_CTL_ACC_BUF_SIZE, GFP_KERNEL);
	if (!debug->buffer)
		return 0;
	pbuffer = debug->buffer;

	if (*ppos)
		return 0;

	if (idiag.cmd.opcode == LPFC_IDIAG_CMD_CTLACC_RD)
		ctl_reg_id = idiag.cmd.data[IDIAG_CTLACC_REGID_INDX];
	else
		return 0;

	if (ctl_reg_id == LPFC_CTL_ACC_ALL)
		for (i = 1; i <= LPFC_CTL_MAX; i++)
			len = lpfc_idiag_ctlacc_read_reg(phba,
							 pbuffer, len, i);
	else
		len = lpfc_idiag_ctlacc_read_reg(phba,
						 pbuffer, len, ctl_reg_id);

	return simple_read_from_buffer(buf, nbytes, ppos, pbuffer, len);
}

static ssize_t
lpfc_idiag_ctlacc_write(struct file *file, const char __user *buf,
			size_t nbytes, loff_t *ppos)
{
	struct lpfc_debug *debug = file->private_data;
	struct lpfc_hba *phba = (struct lpfc_hba *)debug->i_private;
	uint32_t ctl_reg_id, value, reg_val = 0;
	void __iomem *ctl_reg;
	int rc;

	
	debug->op = LPFC_IDIAG_OP_WR;

	rc = lpfc_idiag_cmd_get(buf, nbytes, &idiag.cmd);
	if (rc < 0)
		return rc;

	
	ctl_reg_id = idiag.cmd.data[IDIAG_CTLACC_REGID_INDX];
	value = idiag.cmd.data[IDIAG_CTLACC_VALUE_INDX];

	if (idiag.cmd.opcode == LPFC_IDIAG_CMD_CTLACC_WR ||
	    idiag.cmd.opcode == LPFC_IDIAG_CMD_CTLACC_ST ||
	    idiag.cmd.opcode == LPFC_IDIAG_CMD_CTLACC_CL) {
		if (rc != LPFC_CTL_ACC_WR_CMD_ARG)
			goto error_out;
		if (ctl_reg_id > LPFC_CTL_MAX)
			goto error_out;
	} else if (idiag.cmd.opcode == LPFC_IDIAG_CMD_CTLACC_RD) {
		if (rc != LPFC_CTL_ACC_RD_CMD_ARG)
			goto error_out;
		if ((ctl_reg_id > LPFC_CTL_MAX) &&
		    (ctl_reg_id != LPFC_CTL_ACC_ALL))
			goto error_out;
	} else
		goto error_out;

	
	if (idiag.cmd.opcode == LPFC_IDIAG_CMD_CTLACC_WR ||
	    idiag.cmd.opcode == LPFC_IDIAG_CMD_CTLACC_ST ||
	    idiag.cmd.opcode == LPFC_IDIAG_CMD_CTLACC_CL) {
		switch (ctl_reg_id) {
		case LPFC_CTL_PORT_SEM:
			ctl_reg = phba->sli4_hba.conf_regs_memmap_p +
					LPFC_CTL_PORT_SEM_OFFSET;
			break;
		case LPFC_CTL_PORT_STA:
			ctl_reg = phba->sli4_hba.conf_regs_memmap_p +
					LPFC_CTL_PORT_STA_OFFSET;
			break;
		case LPFC_CTL_PORT_CTL:
			ctl_reg = phba->sli4_hba.conf_regs_memmap_p +
					LPFC_CTL_PORT_CTL_OFFSET;
			break;
		case LPFC_CTL_PORT_ER1:
			ctl_reg = phba->sli4_hba.conf_regs_memmap_p +
					LPFC_CTL_PORT_ER1_OFFSET;
			break;
		case LPFC_CTL_PORT_ER2:
			ctl_reg = phba->sli4_hba.conf_regs_memmap_p +
					LPFC_CTL_PORT_ER2_OFFSET;
			break;
		case LPFC_CTL_PDEV_CTL:
			ctl_reg = phba->sli4_hba.conf_regs_memmap_p +
					LPFC_CTL_PDEV_CTL_OFFSET;
			break;
		default:
			goto error_out;
		}

		if (idiag.cmd.opcode == LPFC_IDIAG_CMD_CTLACC_WR)
			reg_val = value;
		if (idiag.cmd.opcode == LPFC_IDIAG_CMD_CTLACC_ST) {
			reg_val = readl(ctl_reg);
			reg_val |= value;
		}
		if (idiag.cmd.opcode == LPFC_IDIAG_CMD_CTLACC_CL) {
			reg_val = readl(ctl_reg);
			reg_val &= ~value;
		}
		writel(reg_val, ctl_reg);
		readl(ctl_reg); 
	}
	return nbytes;

error_out:
	
	memset(&idiag, 0, sizeof(idiag));
	return -EINVAL;
}

static int
lpfc_idiag_mbxacc_get_setup(struct lpfc_hba *phba, char *pbuffer)
{
	uint32_t mbx_dump_map, mbx_dump_cnt, mbx_word_cnt, mbx_mbox_cmd;
	int len = 0;

	mbx_mbox_cmd = idiag.cmd.data[IDIAG_MBXACC_MBCMD_INDX];
	mbx_dump_map = idiag.cmd.data[IDIAG_MBXACC_DPMAP_INDX];
	mbx_dump_cnt = idiag.cmd.data[IDIAG_MBXACC_DPCNT_INDX];
	mbx_word_cnt = idiag.cmd.data[IDIAG_MBXACC_WDCNT_INDX];

	len += snprintf(pbuffer+len, LPFC_MBX_ACC_BUF_SIZE-len,
			"mbx_dump_map: 0x%08x\n", mbx_dump_map);
	len += snprintf(pbuffer+len, LPFC_MBX_ACC_BUF_SIZE-len,
			"mbx_dump_cnt: %04d\n", mbx_dump_cnt);
	len += snprintf(pbuffer+len, LPFC_MBX_ACC_BUF_SIZE-len,
			"mbx_word_cnt: %04d\n", mbx_word_cnt);
	len += snprintf(pbuffer+len, LPFC_MBX_ACC_BUF_SIZE-len,
			"mbx_mbox_cmd: 0x%02x\n", mbx_mbox_cmd);

	return len;
}

static ssize_t
lpfc_idiag_mbxacc_read(struct file *file, char __user *buf, size_t nbytes,
		       loff_t *ppos)
{
	struct lpfc_debug *debug = file->private_data;
	struct lpfc_hba *phba = (struct lpfc_hba *)debug->i_private;
	char *pbuffer;
	int len = 0;

	
	debug->op = LPFC_IDIAG_OP_RD;

	if (!debug->buffer)
		debug->buffer = kmalloc(LPFC_MBX_ACC_BUF_SIZE, GFP_KERNEL);
	if (!debug->buffer)
		return 0;
	pbuffer = debug->buffer;

	if (*ppos)
		return 0;

	if ((idiag.cmd.opcode != LPFC_IDIAG_CMD_MBXACC_DP) &&
	    (idiag.cmd.opcode != LPFC_IDIAG_BSG_MBXACC_DP))
		return 0;

	len = lpfc_idiag_mbxacc_get_setup(phba, pbuffer);

	return simple_read_from_buffer(buf, nbytes, ppos, pbuffer, len);
}

static ssize_t
lpfc_idiag_mbxacc_write(struct file *file, const char __user *buf,
			size_t nbytes, loff_t *ppos)
{
	struct lpfc_debug *debug = file->private_data;
	uint32_t mbx_dump_map, mbx_dump_cnt, mbx_word_cnt, mbx_mbox_cmd;
	int rc;

	
	debug->op = LPFC_IDIAG_OP_WR;

	rc = lpfc_idiag_cmd_get(buf, nbytes, &idiag.cmd);
	if (rc < 0)
		return rc;

	
	mbx_mbox_cmd = idiag.cmd.data[IDIAG_MBXACC_MBCMD_INDX];
	mbx_dump_map = idiag.cmd.data[IDIAG_MBXACC_DPMAP_INDX];
	mbx_dump_cnt = idiag.cmd.data[IDIAG_MBXACC_DPCNT_INDX];
	mbx_word_cnt = idiag.cmd.data[IDIAG_MBXACC_WDCNT_INDX];

	if (idiag.cmd.opcode == LPFC_IDIAG_CMD_MBXACC_DP) {
		if (!(mbx_dump_map & LPFC_MBX_DMP_MBX_ALL))
			goto error_out;
		if ((mbx_dump_map & ~LPFC_MBX_DMP_MBX_ALL) &&
		    (mbx_dump_map != LPFC_MBX_DMP_ALL))
			goto error_out;
		if (mbx_word_cnt > sizeof(MAILBOX_t))
			goto error_out;
	} else if (idiag.cmd.opcode == LPFC_IDIAG_BSG_MBXACC_DP) {
		if (!(mbx_dump_map & LPFC_BSG_DMP_MBX_ALL))
			goto error_out;
		if ((mbx_dump_map & ~LPFC_BSG_DMP_MBX_ALL) &&
		    (mbx_dump_map != LPFC_MBX_DMP_ALL))
			goto error_out;
		if (mbx_word_cnt > (BSG_MBOX_SIZE)/4)
			goto error_out;
		if (mbx_mbox_cmd != 0x9b)
			goto error_out;
	} else
		goto error_out;

	if (mbx_word_cnt == 0)
		goto error_out;
	if (rc != LPFC_MBX_DMP_ARG)
		goto error_out;
	if (mbx_mbox_cmd & ~0xff)
		goto error_out;

	
	if (mbx_dump_cnt == 0)
		goto reset_out;

	return nbytes;

reset_out:
	
	memset(&idiag, 0, sizeof(idiag));
	return nbytes;

error_out:
	
	memset(&idiag, 0, sizeof(idiag));
	return -EINVAL;
}

static int
lpfc_idiag_extacc_avail_get(struct lpfc_hba *phba, char *pbuffer, int len)
{
	uint16_t ext_cnt, ext_size;

	len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
			"\nAvailable Extents Information:\n");

	len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
			"\tPort Available VPI extents: ");
	lpfc_sli4_get_avail_extnt_rsrc(phba, LPFC_RSC_TYPE_FCOE_VPI,
				       &ext_cnt, &ext_size);
	len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
			"Count %3d, Size %3d\n", ext_cnt, ext_size);

	len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
			"\tPort Available VFI extents: ");
	lpfc_sli4_get_avail_extnt_rsrc(phba, LPFC_RSC_TYPE_FCOE_VFI,
				       &ext_cnt, &ext_size);
	len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
			"Count %3d, Size %3d\n", ext_cnt, ext_size);

	len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
			"\tPort Available RPI extents: ");
	lpfc_sli4_get_avail_extnt_rsrc(phba, LPFC_RSC_TYPE_FCOE_RPI,
				       &ext_cnt, &ext_size);
	len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
			"Count %3d, Size %3d\n", ext_cnt, ext_size);

	len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
			"\tPort Available XRI extents: ");
	lpfc_sli4_get_avail_extnt_rsrc(phba, LPFC_RSC_TYPE_FCOE_XRI,
				       &ext_cnt, &ext_size);
	len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
			"Count %3d, Size %3d\n", ext_cnt, ext_size);

	return len;
}

static int
lpfc_idiag_extacc_alloc_get(struct lpfc_hba *phba, char *pbuffer, int len)
{
	uint16_t ext_cnt, ext_size;
	int rc;

	len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
			"\nAllocated Extents Information:\n");

	len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
			"\tHost Allocated VPI extents: ");
	rc = lpfc_sli4_get_allocated_extnts(phba, LPFC_RSC_TYPE_FCOE_VPI,
					    &ext_cnt, &ext_size);
	if (!rc)
		len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
				"Port %d Extent %3d, Size %3d\n",
				phba->brd_no, ext_cnt, ext_size);
	else
		len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
				"N/A\n");

	len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
			"\tHost Allocated VFI extents: ");
	rc = lpfc_sli4_get_allocated_extnts(phba, LPFC_RSC_TYPE_FCOE_VFI,
					    &ext_cnt, &ext_size);
	if (!rc)
		len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
				"Port %d Extent %3d, Size %3d\n",
				phba->brd_no, ext_cnt, ext_size);
	else
		len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
				"N/A\n");

	len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
			"\tHost Allocated RPI extents: ");
	rc = lpfc_sli4_get_allocated_extnts(phba, LPFC_RSC_TYPE_FCOE_RPI,
					    &ext_cnt, &ext_size);
	if (!rc)
		len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
				"Port %d Extent %3d, Size %3d\n",
				phba->brd_no, ext_cnt, ext_size);
	else
		len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
				"N/A\n");

	len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
			"\tHost Allocated XRI extents: ");
	rc = lpfc_sli4_get_allocated_extnts(phba, LPFC_RSC_TYPE_FCOE_XRI,
					    &ext_cnt, &ext_size);
	if (!rc)
		len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
				"Port %d Extent %3d, Size %3d\n",
				phba->brd_no, ext_cnt, ext_size);
	else
		len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
				"N/A\n");

	return len;
}

static int
lpfc_idiag_extacc_drivr_get(struct lpfc_hba *phba, char *pbuffer, int len)
{
	struct lpfc_rsrc_blks *rsrc_blks;
	int index;

	len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
			"\nDriver Extents Information:\n");

	len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
			"\tVPI extents:\n");
	index = 0;
	list_for_each_entry(rsrc_blks, &phba->lpfc_vpi_blk_list, list) {
		len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
				"\t\tBlock %3d: Start %4d, Count %4d\n",
				index, rsrc_blks->rsrc_start,
				rsrc_blks->rsrc_size);
		index++;
	}
	len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
			"\tVFI extents:\n");
	index = 0;
	list_for_each_entry(rsrc_blks, &phba->sli4_hba.lpfc_vfi_blk_list,
			    list) {
		len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
				"\t\tBlock %3d: Start %4d, Count %4d\n",
				index, rsrc_blks->rsrc_start,
				rsrc_blks->rsrc_size);
		index++;
	}

	len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
			"\tRPI extents:\n");
	index = 0;
	list_for_each_entry(rsrc_blks, &phba->sli4_hba.lpfc_rpi_blk_list,
			    list) {
		len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
				"\t\tBlock %3d: Start %4d, Count %4d\n",
				index, rsrc_blks->rsrc_start,
				rsrc_blks->rsrc_size);
		index++;
	}

	len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
			"\tXRI extents:\n");
	index = 0;
	list_for_each_entry(rsrc_blks, &phba->sli4_hba.lpfc_xri_blk_list,
			    list) {
		len += snprintf(pbuffer+len, LPFC_EXT_ACC_BUF_SIZE-len,
				"\t\tBlock %3d: Start %4d, Count %4d\n",
				index, rsrc_blks->rsrc_start,
				rsrc_blks->rsrc_size);
		index++;
	}

	return len;
}

static ssize_t
lpfc_idiag_extacc_write(struct file *file, const char __user *buf,
			size_t nbytes, loff_t *ppos)
{
	struct lpfc_debug *debug = file->private_data;
	uint32_t ext_map;
	int rc;

	
	debug->op = LPFC_IDIAG_OP_WR;

	rc = lpfc_idiag_cmd_get(buf, nbytes, &idiag.cmd);
	if (rc < 0)
		return rc;

	ext_map = idiag.cmd.data[IDIAG_EXTACC_EXMAP_INDX];

	if (idiag.cmd.opcode != LPFC_IDIAG_CMD_EXTACC_RD)
		goto error_out;
	if (rc != LPFC_EXT_ACC_CMD_ARG)
		goto error_out;
	if (!(ext_map & LPFC_EXT_ACC_ALL))
		goto error_out;

	return nbytes;
error_out:
	
	memset(&idiag, 0, sizeof(idiag));
	return -EINVAL;
}

static ssize_t
lpfc_idiag_extacc_read(struct file *file, char __user *buf, size_t nbytes,
		       loff_t *ppos)
{
	struct lpfc_debug *debug = file->private_data;
	struct lpfc_hba *phba = (struct lpfc_hba *)debug->i_private;
	char *pbuffer;
	uint32_t ext_map;
	int len = 0;

	
	debug->op = LPFC_IDIAG_OP_RD;

	if (!debug->buffer)
		debug->buffer = kmalloc(LPFC_EXT_ACC_BUF_SIZE, GFP_KERNEL);
	if (!debug->buffer)
		return 0;
	pbuffer = debug->buffer;
	if (*ppos)
		return 0;
	if (idiag.cmd.opcode != LPFC_IDIAG_CMD_EXTACC_RD)
		return 0;

	ext_map = idiag.cmd.data[IDIAG_EXTACC_EXMAP_INDX];
	if (ext_map & LPFC_EXT_ACC_AVAIL)
		len = lpfc_idiag_extacc_avail_get(phba, pbuffer, len);
	if (ext_map & LPFC_EXT_ACC_ALLOC)
		len = lpfc_idiag_extacc_alloc_get(phba, pbuffer, len);
	if (ext_map & LPFC_EXT_ACC_DRIVR)
		len = lpfc_idiag_extacc_drivr_get(phba, pbuffer, len);

	return simple_read_from_buffer(buf, nbytes, ppos, pbuffer, len);
}

#undef lpfc_debugfs_op_disc_trc
static const struct file_operations lpfc_debugfs_op_disc_trc = {
	.owner =        THIS_MODULE,
	.open =         lpfc_debugfs_disc_trc_open,
	.llseek =       lpfc_debugfs_lseek,
	.read =         lpfc_debugfs_read,
	.release =      lpfc_debugfs_release,
};

#undef lpfc_debugfs_op_nodelist
static const struct file_operations lpfc_debugfs_op_nodelist = {
	.owner =        THIS_MODULE,
	.open =         lpfc_debugfs_nodelist_open,
	.llseek =       lpfc_debugfs_lseek,
	.read =         lpfc_debugfs_read,
	.release =      lpfc_debugfs_release,
};

#undef lpfc_debugfs_op_hbqinfo
static const struct file_operations lpfc_debugfs_op_hbqinfo = {
	.owner =        THIS_MODULE,
	.open =         lpfc_debugfs_hbqinfo_open,
	.llseek =       lpfc_debugfs_lseek,
	.read =         lpfc_debugfs_read,
	.release =      lpfc_debugfs_release,
};

#undef lpfc_debugfs_op_dumpHBASlim
static const struct file_operations lpfc_debugfs_op_dumpHBASlim = {
	.owner =        THIS_MODULE,
	.open =         lpfc_debugfs_dumpHBASlim_open,
	.llseek =       lpfc_debugfs_lseek,
	.read =         lpfc_debugfs_read,
	.release =      lpfc_debugfs_release,
};

#undef lpfc_debugfs_op_dumpHostSlim
static const struct file_operations lpfc_debugfs_op_dumpHostSlim = {
	.owner =        THIS_MODULE,
	.open =         lpfc_debugfs_dumpHostSlim_open,
	.llseek =       lpfc_debugfs_lseek,
	.read =         lpfc_debugfs_read,
	.release =      lpfc_debugfs_release,
};

#undef lpfc_debugfs_op_dumpData
static const struct file_operations lpfc_debugfs_op_dumpData = {
	.owner =        THIS_MODULE,
	.open =         lpfc_debugfs_dumpData_open,
	.llseek =       lpfc_debugfs_lseek,
	.read =         lpfc_debugfs_read,
	.write =	lpfc_debugfs_dumpDataDif_write,
	.release =      lpfc_debugfs_dumpDataDif_release,
};

#undef lpfc_debugfs_op_dumpDif
static const struct file_operations lpfc_debugfs_op_dumpDif = {
	.owner =        THIS_MODULE,
	.open =         lpfc_debugfs_dumpDif_open,
	.llseek =       lpfc_debugfs_lseek,
	.read =         lpfc_debugfs_read,
	.write =	lpfc_debugfs_dumpDataDif_write,
	.release =      lpfc_debugfs_dumpDataDif_release,
};

#undef lpfc_debugfs_op_dif_err
static const struct file_operations lpfc_debugfs_op_dif_err = {
	.owner =	THIS_MODULE,
	.open =		simple_open,
	.llseek =	lpfc_debugfs_lseek,
	.read =		lpfc_debugfs_dif_err_read,
	.write =	lpfc_debugfs_dif_err_write,
	.release =	lpfc_debugfs_dif_err_release,
};

#undef lpfc_debugfs_op_slow_ring_trc
static const struct file_operations lpfc_debugfs_op_slow_ring_trc = {
	.owner =        THIS_MODULE,
	.open =         lpfc_debugfs_slow_ring_trc_open,
	.llseek =       lpfc_debugfs_lseek,
	.read =         lpfc_debugfs_read,
	.release =      lpfc_debugfs_release,
};

static struct dentry *lpfc_debugfs_root = NULL;
static atomic_t lpfc_debugfs_hba_count;

#undef lpfc_idiag_op_pciCfg
static const struct file_operations lpfc_idiag_op_pciCfg = {
	.owner =        THIS_MODULE,
	.open =         lpfc_idiag_open,
	.llseek =       lpfc_debugfs_lseek,
	.read =         lpfc_idiag_pcicfg_read,
	.write =        lpfc_idiag_pcicfg_write,
	.release =      lpfc_idiag_cmd_release,
};

#undef lpfc_idiag_op_barAcc
static const struct file_operations lpfc_idiag_op_barAcc = {
	.owner =        THIS_MODULE,
	.open =         lpfc_idiag_open,
	.llseek =       lpfc_debugfs_lseek,
	.read =         lpfc_idiag_baracc_read,
	.write =        lpfc_idiag_baracc_write,
	.release =      lpfc_idiag_cmd_release,
};

#undef lpfc_idiag_op_queInfo
static const struct file_operations lpfc_idiag_op_queInfo = {
	.owner =        THIS_MODULE,
	.open =         lpfc_idiag_open,
	.read =         lpfc_idiag_queinfo_read,
	.release =      lpfc_idiag_release,
};

#undef lpfc_idiag_op_queAcc
static const struct file_operations lpfc_idiag_op_queAcc = {
	.owner =        THIS_MODULE,
	.open =         lpfc_idiag_open,
	.llseek =       lpfc_debugfs_lseek,
	.read =         lpfc_idiag_queacc_read,
	.write =        lpfc_idiag_queacc_write,
	.release =      lpfc_idiag_cmd_release,
};

#undef lpfc_idiag_op_drbAcc
static const struct file_operations lpfc_idiag_op_drbAcc = {
	.owner =        THIS_MODULE,
	.open =         lpfc_idiag_open,
	.llseek =       lpfc_debugfs_lseek,
	.read =         lpfc_idiag_drbacc_read,
	.write =        lpfc_idiag_drbacc_write,
	.release =      lpfc_idiag_cmd_release,
};

#undef lpfc_idiag_op_ctlAcc
static const struct file_operations lpfc_idiag_op_ctlAcc = {
	.owner =        THIS_MODULE,
	.open =         lpfc_idiag_open,
	.llseek =       lpfc_debugfs_lseek,
	.read =         lpfc_idiag_ctlacc_read,
	.write =        lpfc_idiag_ctlacc_write,
	.release =      lpfc_idiag_cmd_release,
};

#undef lpfc_idiag_op_mbxAcc
static const struct file_operations lpfc_idiag_op_mbxAcc = {
	.owner =        THIS_MODULE,
	.open =         lpfc_idiag_open,
	.llseek =       lpfc_debugfs_lseek,
	.read =         lpfc_idiag_mbxacc_read,
	.write =        lpfc_idiag_mbxacc_write,
	.release =      lpfc_idiag_cmd_release,
};

#undef lpfc_idiag_op_extAcc
static const struct file_operations lpfc_idiag_op_extAcc = {
	.owner =        THIS_MODULE,
	.open =         lpfc_idiag_open,
	.llseek =       lpfc_debugfs_lseek,
	.read =         lpfc_idiag_extacc_read,
	.write =        lpfc_idiag_extacc_write,
	.release =      lpfc_idiag_cmd_release,
};

#endif

void
lpfc_idiag_mbxacc_dump_bsg_mbox(struct lpfc_hba *phba, enum nemb_type nemb_tp,
				enum mbox_type mbox_tp, enum dma_type dma_tp,
				enum sta_type sta_tp,
				struct lpfc_dmabuf *dmabuf, uint32_t ext_buf)
{
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	uint32_t *mbx_mbox_cmd, *mbx_dump_map, *mbx_dump_cnt, *mbx_word_cnt;
	char line_buf[LPFC_MBX_ACC_LBUF_SZ];
	int len = 0;
	uint32_t do_dump = 0;
	uint32_t *pword;
	uint32_t i;

	if (idiag.cmd.opcode != LPFC_IDIAG_BSG_MBXACC_DP)
		return;

	mbx_mbox_cmd = &idiag.cmd.data[IDIAG_MBXACC_MBCMD_INDX];
	mbx_dump_map = &idiag.cmd.data[IDIAG_MBXACC_DPMAP_INDX];
	mbx_dump_cnt = &idiag.cmd.data[IDIAG_MBXACC_DPCNT_INDX];
	mbx_word_cnt = &idiag.cmd.data[IDIAG_MBXACC_WDCNT_INDX];

	if (!(*mbx_dump_map & LPFC_MBX_DMP_ALL) ||
	    (*mbx_dump_cnt == 0) ||
	    (*mbx_word_cnt == 0))
		return;

	if (*mbx_mbox_cmd != 0x9B)
		return;

	if ((mbox_tp == mbox_rd) && (dma_tp == dma_mbox)) {
		if (*mbx_dump_map & LPFC_BSG_DMP_MBX_RD_MBX) {
			do_dump |= LPFC_BSG_DMP_MBX_RD_MBX;
			printk(KERN_ERR "\nRead mbox command (x%x), "
			       "nemb:0x%x, extbuf_cnt:%d:\n",
			       sta_tp, nemb_tp, ext_buf);
		}
	}
	if ((mbox_tp == mbox_rd) && (dma_tp == dma_ebuf)) {
		if (*mbx_dump_map & LPFC_BSG_DMP_MBX_RD_BUF) {
			do_dump |= LPFC_BSG_DMP_MBX_RD_BUF;
			printk(KERN_ERR "\nRead mbox buffer (x%x), "
			       "nemb:0x%x, extbuf_seq:%d:\n",
			       sta_tp, nemb_tp, ext_buf);
		}
	}
	if ((mbox_tp == mbox_wr) && (dma_tp == dma_mbox)) {
		if (*mbx_dump_map & LPFC_BSG_DMP_MBX_WR_MBX) {
			do_dump |= LPFC_BSG_DMP_MBX_WR_MBX;
			printk(KERN_ERR "\nWrite mbox command (x%x), "
			       "nemb:0x%x, extbuf_cnt:%d:\n",
			       sta_tp, nemb_tp, ext_buf);
		}
	}
	if ((mbox_tp == mbox_wr) && (dma_tp == dma_ebuf)) {
		if (*mbx_dump_map & LPFC_BSG_DMP_MBX_WR_BUF) {
			do_dump |= LPFC_BSG_DMP_MBX_WR_BUF;
			printk(KERN_ERR "\nWrite mbox buffer (x%x), "
			       "nemb:0x%x, extbuf_seq:%d:\n",
			       sta_tp, nemb_tp, ext_buf);
		}
	}

	
	if (do_dump) {
		pword = (uint32_t *)dmabuf->virt;
		for (i = 0; i < *mbx_word_cnt; i++) {
			if (!(i % 8)) {
				if (i != 0)
					printk(KERN_ERR "%s\n", line_buf);
				len = 0;
				len += snprintf(line_buf+len,
						LPFC_MBX_ACC_LBUF_SZ-len,
						"%03d: ", i);
			}
			len += snprintf(line_buf+len, LPFC_MBX_ACC_LBUF_SZ-len,
					"%08x ", (uint32_t)*pword);
			pword++;
		}
		if ((i - 1) % 8)
			printk(KERN_ERR "%s\n", line_buf);
		(*mbx_dump_cnt)--;
	}

	
	if (*mbx_dump_cnt == 0)
		memset(&idiag, 0, sizeof(idiag));
	return;
#endif
}

void
lpfc_idiag_mbxacc_dump_issue_mbox(struct lpfc_hba *phba, MAILBOX_t *pmbox)
{
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	uint32_t *mbx_dump_map, *mbx_dump_cnt, *mbx_word_cnt, *mbx_mbox_cmd;
	char line_buf[LPFC_MBX_ACC_LBUF_SZ];
	int len = 0;
	uint32_t *pword;
	uint8_t *pbyte;
	uint32_t i, j;

	if (idiag.cmd.opcode != LPFC_IDIAG_CMD_MBXACC_DP)
		return;

	mbx_mbox_cmd = &idiag.cmd.data[IDIAG_MBXACC_MBCMD_INDX];
	mbx_dump_map = &idiag.cmd.data[IDIAG_MBXACC_DPMAP_INDX];
	mbx_dump_cnt = &idiag.cmd.data[IDIAG_MBXACC_DPCNT_INDX];
	mbx_word_cnt = &idiag.cmd.data[IDIAG_MBXACC_WDCNT_INDX];

	if (!(*mbx_dump_map & LPFC_MBX_DMP_MBX_ALL) ||
	    (*mbx_dump_cnt == 0) ||
	    (*mbx_word_cnt == 0))
		return;

	if ((*mbx_mbox_cmd != LPFC_MBX_ALL_CMD) &&
	    (*mbx_mbox_cmd != pmbox->mbxCommand))
		return;

	
	if (*mbx_dump_map & LPFC_MBX_DMP_MBX_WORD) {
		printk(KERN_ERR "Mailbox command:0x%x dump by word:\n",
		       pmbox->mbxCommand);
		pword = (uint32_t *)pmbox;
		for (i = 0; i < *mbx_word_cnt; i++) {
			if (!(i % 8)) {
				if (i != 0)
					printk(KERN_ERR "%s\n", line_buf);
				len = 0;
				memset(line_buf, 0, LPFC_MBX_ACC_LBUF_SZ);
				len += snprintf(line_buf+len,
						LPFC_MBX_ACC_LBUF_SZ-len,
						"%03d: ", i);
			}
			len += snprintf(line_buf+len, LPFC_MBX_ACC_LBUF_SZ-len,
					"%08x ",
					((uint32_t)*pword) & 0xffffffff);
			pword++;
		}
		if ((i - 1) % 8)
			printk(KERN_ERR "%s\n", line_buf);
		printk(KERN_ERR "\n");
	}
	if (*mbx_dump_map & LPFC_MBX_DMP_MBX_BYTE) {
		printk(KERN_ERR "Mailbox command:0x%x dump by byte:\n",
		       pmbox->mbxCommand);
		pbyte = (uint8_t *)pmbox;
		for (i = 0; i < *mbx_word_cnt; i++) {
			if (!(i % 8)) {
				if (i != 0)
					printk(KERN_ERR "%s\n", line_buf);
				len = 0;
				memset(line_buf, 0, LPFC_MBX_ACC_LBUF_SZ);
				len += snprintf(line_buf+len,
						LPFC_MBX_ACC_LBUF_SZ-len,
						"%03d: ", i);
			}
			for (j = 0; j < 4; j++) {
				len += snprintf(line_buf+len,
						LPFC_MBX_ACC_LBUF_SZ-len,
						"%02x",
						((uint8_t)*pbyte) & 0xff);
				pbyte++;
			}
			len += snprintf(line_buf+len,
					LPFC_MBX_ACC_LBUF_SZ-len, " ");
		}
		if ((i - 1) % 8)
			printk(KERN_ERR "%s\n", line_buf);
		printk(KERN_ERR "\n");
	}
	(*mbx_dump_cnt)--;

	
	if (*mbx_dump_cnt == 0)
		memset(&idiag, 0, sizeof(idiag));
	return;
#endif
}

inline void
lpfc_debugfs_initialize(struct lpfc_vport *vport)
{
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	struct lpfc_hba   *phba = vport->phba;
	char name[64];
	uint32_t num, i;

	if (!lpfc_debugfs_enable)
		return;

	
	if (!lpfc_debugfs_root) {
		lpfc_debugfs_root = debugfs_create_dir("lpfc", NULL);
		atomic_set(&lpfc_debugfs_hba_count, 0);
		if (!lpfc_debugfs_root) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
					 "0408 Cannot create debugfs root\n");
			goto debug_failed;
		}
	}
	if (!lpfc_debugfs_start_time)
		lpfc_debugfs_start_time = jiffies;

	
	snprintf(name, sizeof(name), "fn%d", phba->brd_no);
	if (!phba->hba_debugfs_root) {
		phba->hba_debugfs_root =
			debugfs_create_dir(name, lpfc_debugfs_root);
		if (!phba->hba_debugfs_root) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
					 "0412 Cannot create debugfs hba\n");
			goto debug_failed;
		}
		atomic_inc(&lpfc_debugfs_hba_count);
		atomic_set(&phba->debugfs_vport_count, 0);

		
		snprintf(name, sizeof(name), "hbqinfo");
		phba->debug_hbqinfo =
			debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
				 phba->hba_debugfs_root,
				 phba, &lpfc_debugfs_op_hbqinfo);
		if (!phba->debug_hbqinfo) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
				"0411 Cannot create debugfs hbqinfo\n");
			goto debug_failed;
		}

		
		if (phba->sli_rev < LPFC_SLI_REV4) {
			snprintf(name, sizeof(name), "dumpHBASlim");
			phba->debug_dumpHBASlim =
				debugfs_create_file(name,
					S_IFREG|S_IRUGO|S_IWUSR,
					phba->hba_debugfs_root,
					phba, &lpfc_debugfs_op_dumpHBASlim);
			if (!phba->debug_dumpHBASlim) {
				lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
						 "0413 Cannot create debugfs "
						"dumpHBASlim\n");
				goto debug_failed;
			}
		} else
			phba->debug_dumpHBASlim = NULL;

		
		if (phba->sli_rev < LPFC_SLI_REV4) {
			snprintf(name, sizeof(name), "dumpHostSlim");
			phba->debug_dumpHostSlim =
				debugfs_create_file(name,
					S_IFREG|S_IRUGO|S_IWUSR,
					phba->hba_debugfs_root,
					phba, &lpfc_debugfs_op_dumpHostSlim);
			if (!phba->debug_dumpHostSlim) {
				lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
						 "0414 Cannot create debugfs "
						 "dumpHostSlim\n");
				goto debug_failed;
			}
		} else
			phba->debug_dumpHBASlim = NULL;

		
		snprintf(name, sizeof(name), "dumpData");
		phba->debug_dumpData =
			debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
				 phba->hba_debugfs_root,
				 phba, &lpfc_debugfs_op_dumpData);
		if (!phba->debug_dumpData) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
				"0800 Cannot create debugfs dumpData\n");
			goto debug_failed;
		}

		
		snprintf(name, sizeof(name), "dumpDif");
		phba->debug_dumpDif =
			debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
				 phba->hba_debugfs_root,
				 phba, &lpfc_debugfs_op_dumpDif);
		if (!phba->debug_dumpDif) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
				"0801 Cannot create debugfs dumpDif\n");
			goto debug_failed;
		}

		
		snprintf(name, sizeof(name), "InjErrLBA");
		phba->debug_InjErrLBA =
			debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
			phba->hba_debugfs_root,
			phba, &lpfc_debugfs_op_dif_err);
		if (!phba->debug_InjErrLBA) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
				"0807 Cannot create debugfs InjErrLBA\n");
			goto debug_failed;
		}
		phba->lpfc_injerr_lba = LPFC_INJERR_LBA_OFF;

		snprintf(name, sizeof(name), "InjErrNPortID");
		phba->debug_InjErrNPortID =
			debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
			phba->hba_debugfs_root,
			phba, &lpfc_debugfs_op_dif_err);
		if (!phba->debug_InjErrNPortID) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
				"0809 Cannot create debugfs InjErrNPortID\n");
			goto debug_failed;
		}

		snprintf(name, sizeof(name), "InjErrWWPN");
		phba->debug_InjErrWWPN =
			debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
			phba->hba_debugfs_root,
			phba, &lpfc_debugfs_op_dif_err);
		if (!phba->debug_InjErrWWPN) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
				"0810 Cannot create debugfs InjErrWWPN\n");
			goto debug_failed;
		}

		snprintf(name, sizeof(name), "writeGuardInjErr");
		phba->debug_writeGuard =
			debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
			phba->hba_debugfs_root,
			phba, &lpfc_debugfs_op_dif_err);
		if (!phba->debug_writeGuard) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
				"0802 Cannot create debugfs writeGuard\n");
			goto debug_failed;
		}

		snprintf(name, sizeof(name), "writeAppInjErr");
		phba->debug_writeApp =
			debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
			phba->hba_debugfs_root,
			phba, &lpfc_debugfs_op_dif_err);
		if (!phba->debug_writeApp) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
				"0803 Cannot create debugfs writeApp\n");
			goto debug_failed;
		}

		snprintf(name, sizeof(name), "writeRefInjErr");
		phba->debug_writeRef =
			debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
			phba->hba_debugfs_root,
			phba, &lpfc_debugfs_op_dif_err);
		if (!phba->debug_writeRef) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
				"0804 Cannot create debugfs writeRef\n");
			goto debug_failed;
		}

		snprintf(name, sizeof(name), "readGuardInjErr");
		phba->debug_readGuard =
			debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
			phba->hba_debugfs_root,
			phba, &lpfc_debugfs_op_dif_err);
		if (!phba->debug_readGuard) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
				"0808 Cannot create debugfs readGuard\n");
			goto debug_failed;
		}

		snprintf(name, sizeof(name), "readAppInjErr");
		phba->debug_readApp =
			debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
			phba->hba_debugfs_root,
			phba, &lpfc_debugfs_op_dif_err);
		if (!phba->debug_readApp) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
				"0805 Cannot create debugfs readApp\n");
			goto debug_failed;
		}

		snprintf(name, sizeof(name), "readRefInjErr");
		phba->debug_readRef =
			debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
			phba->hba_debugfs_root,
			phba, &lpfc_debugfs_op_dif_err);
		if (!phba->debug_readRef) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
				"0806 Cannot create debugfs readApp\n");
			goto debug_failed;
		}

		
		if (lpfc_debugfs_max_slow_ring_trc) {
			num = lpfc_debugfs_max_slow_ring_trc - 1;
			if (num & lpfc_debugfs_max_slow_ring_trc) {
				
				num = lpfc_debugfs_max_slow_ring_trc;
				i = 0;
				while (num > 1) {
					num = num >> 1;
					i++;
				}
				lpfc_debugfs_max_slow_ring_trc = (1 << i);
				printk(KERN_ERR
				       "lpfc_debugfs_max_disc_trc changed to "
				       "%d\n", lpfc_debugfs_max_disc_trc);
			}
		}

		snprintf(name, sizeof(name), "slow_ring_trace");
		phba->debug_slow_ring_trc =
			debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
				 phba->hba_debugfs_root,
				 phba, &lpfc_debugfs_op_slow_ring_trc);
		if (!phba->debug_slow_ring_trc) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
					 "0415 Cannot create debugfs "
					 "slow_ring_trace\n");
			goto debug_failed;
		}
		if (!phba->slow_ring_trc) {
			phba->slow_ring_trc = kmalloc(
				(sizeof(struct lpfc_debugfs_trc) *
				lpfc_debugfs_max_slow_ring_trc),
				GFP_KERNEL);
			if (!phba->slow_ring_trc) {
				lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
						 "0416 Cannot create debugfs "
						 "slow_ring buffer\n");
				goto debug_failed;
			}
			atomic_set(&phba->slow_ring_trc_cnt, 0);
			memset(phba->slow_ring_trc, 0,
				(sizeof(struct lpfc_debugfs_trc) *
				lpfc_debugfs_max_slow_ring_trc));
		}
	}

	snprintf(name, sizeof(name), "vport%d", vport->vpi);
	if (!vport->vport_debugfs_root) {
		vport->vport_debugfs_root =
			debugfs_create_dir(name, phba->hba_debugfs_root);
		if (!vport->vport_debugfs_root) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
					 "0417 Can't create debugfs\n");
			goto debug_failed;
		}
		atomic_inc(&phba->debugfs_vport_count);
	}

	if (lpfc_debugfs_max_disc_trc) {
		num = lpfc_debugfs_max_disc_trc - 1;
		if (num & lpfc_debugfs_max_disc_trc) {
			
			num = lpfc_debugfs_max_disc_trc;
			i = 0;
			while (num > 1) {
				num = num >> 1;
				i++;
			}
			lpfc_debugfs_max_disc_trc = (1 << i);
			printk(KERN_ERR
			       "lpfc_debugfs_max_disc_trc changed to %d\n",
			       lpfc_debugfs_max_disc_trc);
		}
	}

	vport->disc_trc = kzalloc(
		(sizeof(struct lpfc_debugfs_trc) * lpfc_debugfs_max_disc_trc),
		GFP_KERNEL);

	if (!vport->disc_trc) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
				 "0418 Cannot create debugfs disc trace "
				 "buffer\n");
		goto debug_failed;
	}
	atomic_set(&vport->disc_trc_cnt, 0);

	snprintf(name, sizeof(name), "discovery_trace");
	vport->debug_disc_trc =
		debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
				 vport->vport_debugfs_root,
				 vport, &lpfc_debugfs_op_disc_trc);
	if (!vport->debug_disc_trc) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
				 "0419 Cannot create debugfs "
				 "discovery_trace\n");
		goto debug_failed;
	}
	snprintf(name, sizeof(name), "nodelist");
	vport->debug_nodelist =
		debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
				 vport->vport_debugfs_root,
				 vport, &lpfc_debugfs_op_nodelist);
	if (!vport->debug_nodelist) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
				 "2985 Can't create debugfs nodelist\n");
		goto debug_failed;
	}

	if (phba->sli_rev < LPFC_SLI_REV4)
		goto debug_failed;

	snprintf(name, sizeof(name), "iDiag");
	if (!phba->idiag_root) {
		phba->idiag_root =
			debugfs_create_dir(name, phba->hba_debugfs_root);
		if (!phba->idiag_root) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
					 "2922 Can't create idiag debugfs\n");
			goto debug_failed;
		}
		
		memset(&idiag, 0, sizeof(idiag));
	}

	
	snprintf(name, sizeof(name), "pciCfg");
	if (!phba->idiag_pci_cfg) {
		phba->idiag_pci_cfg =
			debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
				phba->idiag_root, phba, &lpfc_idiag_op_pciCfg);
		if (!phba->idiag_pci_cfg) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
					 "2923 Can't create idiag debugfs\n");
			goto debug_failed;
		}
		idiag.offset.last_rd = 0;
	}

	
	snprintf(name, sizeof(name), "barAcc");
	if (!phba->idiag_bar_acc) {
		phba->idiag_bar_acc =
			debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
				phba->idiag_root, phba, &lpfc_idiag_op_barAcc);
		if (!phba->idiag_bar_acc) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
					"3056 Can't create idiag debugfs\n");
			goto debug_failed;
		}
		idiag.offset.last_rd = 0;
	}

	
	snprintf(name, sizeof(name), "queInfo");
	if (!phba->idiag_que_info) {
		phba->idiag_que_info =
			debugfs_create_file(name, S_IFREG|S_IRUGO,
			phba->idiag_root, phba, &lpfc_idiag_op_queInfo);
		if (!phba->idiag_que_info) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
					 "2924 Can't create idiag debugfs\n");
			goto debug_failed;
		}
	}

	
	snprintf(name, sizeof(name), "queAcc");
	if (!phba->idiag_que_acc) {
		phba->idiag_que_acc =
			debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
				phba->idiag_root, phba, &lpfc_idiag_op_queAcc);
		if (!phba->idiag_que_acc) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
					 "2926 Can't create idiag debugfs\n");
			goto debug_failed;
		}
	}

	
	snprintf(name, sizeof(name), "drbAcc");
	if (!phba->idiag_drb_acc) {
		phba->idiag_drb_acc =
			debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
				phba->idiag_root, phba, &lpfc_idiag_op_drbAcc);
		if (!phba->idiag_drb_acc) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
					 "2927 Can't create idiag debugfs\n");
			goto debug_failed;
		}
	}

	
	snprintf(name, sizeof(name), "ctlAcc");
	if (!phba->idiag_ctl_acc) {
		phba->idiag_ctl_acc =
			debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
				phba->idiag_root, phba, &lpfc_idiag_op_ctlAcc);
		if (!phba->idiag_ctl_acc) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
					 "2981 Can't create idiag debugfs\n");
			goto debug_failed;
		}
	}

	
	snprintf(name, sizeof(name), "mbxAcc");
	if (!phba->idiag_mbx_acc) {
		phba->idiag_mbx_acc =
			debugfs_create_file(name, S_IFREG|S_IRUGO|S_IWUSR,
				phba->idiag_root, phba, &lpfc_idiag_op_mbxAcc);
		if (!phba->idiag_mbx_acc) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
					"2980 Can't create idiag debugfs\n");
			goto debug_failed;
		}
	}

	
	if (phba->sli4_hba.extents_in_use) {
		snprintf(name, sizeof(name), "extAcc");
		if (!phba->idiag_ext_acc) {
			phba->idiag_ext_acc =
				debugfs_create_file(name,
						    S_IFREG|S_IRUGO|S_IWUSR,
						    phba->idiag_root, phba,
						    &lpfc_idiag_op_extAcc);
			if (!phba->idiag_ext_acc) {
				lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
						"2986 Cant create "
						"idiag debugfs\n");
				goto debug_failed;
			}
		}
	}

debug_failed:
	return;
#endif
}

inline void
lpfc_debugfs_terminate(struct lpfc_vport *vport)
{
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	struct lpfc_hba   *phba = vport->phba;

	if (vport->disc_trc) {
		kfree(vport->disc_trc);
		vport->disc_trc = NULL;
	}
	if (vport->debug_disc_trc) {
		debugfs_remove(vport->debug_disc_trc); 
		vport->debug_disc_trc = NULL;
	}
	if (vport->debug_nodelist) {
		debugfs_remove(vport->debug_nodelist); 
		vport->debug_nodelist = NULL;
	}
	if (vport->vport_debugfs_root) {
		debugfs_remove(vport->vport_debugfs_root); 
		vport->vport_debugfs_root = NULL;
		atomic_dec(&phba->debugfs_vport_count);
	}
	if (atomic_read(&phba->debugfs_vport_count) == 0) {

		if (phba->debug_hbqinfo) {
			debugfs_remove(phba->debug_hbqinfo); 
			phba->debug_hbqinfo = NULL;
		}
		if (phba->debug_dumpHBASlim) {
			debugfs_remove(phba->debug_dumpHBASlim); 
			phba->debug_dumpHBASlim = NULL;
		}
		if (phba->debug_dumpHostSlim) {
			debugfs_remove(phba->debug_dumpHostSlim); 
			phba->debug_dumpHostSlim = NULL;
		}
		if (phba->debug_dumpData) {
			debugfs_remove(phba->debug_dumpData); 
			phba->debug_dumpData = NULL;
		}

		if (phba->debug_dumpDif) {
			debugfs_remove(phba->debug_dumpDif); 
			phba->debug_dumpDif = NULL;
		}
		if (phba->debug_InjErrLBA) {
			debugfs_remove(phba->debug_InjErrLBA); 
			phba->debug_InjErrLBA = NULL;
		}
		if (phba->debug_InjErrNPortID) {	 
			debugfs_remove(phba->debug_InjErrNPortID);
			phba->debug_InjErrNPortID = NULL;
		}
		if (phba->debug_InjErrWWPN) {
			debugfs_remove(phba->debug_InjErrWWPN); 
			phba->debug_InjErrWWPN = NULL;
		}
		if (phba->debug_writeGuard) {
			debugfs_remove(phba->debug_writeGuard); 
			phba->debug_writeGuard = NULL;
		}
		if (phba->debug_writeApp) {
			debugfs_remove(phba->debug_writeApp); 
			phba->debug_writeApp = NULL;
		}
		if (phba->debug_writeRef) {
			debugfs_remove(phba->debug_writeRef); 
			phba->debug_writeRef = NULL;
		}
		if (phba->debug_readGuard) {
			debugfs_remove(phba->debug_readGuard); 
			phba->debug_readGuard = NULL;
		}
		if (phba->debug_readApp) {
			debugfs_remove(phba->debug_readApp); 
			phba->debug_readApp = NULL;
		}
		if (phba->debug_readRef) {
			debugfs_remove(phba->debug_readRef); 
			phba->debug_readRef = NULL;
		}

		if (phba->slow_ring_trc) {
			kfree(phba->slow_ring_trc);
			phba->slow_ring_trc = NULL;
		}
		if (phba->debug_slow_ring_trc) {
			
			debugfs_remove(phba->debug_slow_ring_trc);
			phba->debug_slow_ring_trc = NULL;
		}

		if (phba->sli_rev == LPFC_SLI_REV4) {
			if (phba->idiag_ext_acc) {
				
				debugfs_remove(phba->idiag_ext_acc);
				phba->idiag_ext_acc = NULL;
			}
			if (phba->idiag_mbx_acc) {
				
				debugfs_remove(phba->idiag_mbx_acc);
				phba->idiag_mbx_acc = NULL;
			}
			if (phba->idiag_ctl_acc) {
				
				debugfs_remove(phba->idiag_ctl_acc);
				phba->idiag_ctl_acc = NULL;
			}
			if (phba->idiag_drb_acc) {
				
				debugfs_remove(phba->idiag_drb_acc);
				phba->idiag_drb_acc = NULL;
			}
			if (phba->idiag_que_acc) {
				
				debugfs_remove(phba->idiag_que_acc);
				phba->idiag_que_acc = NULL;
			}
			if (phba->idiag_que_info) {
				
				debugfs_remove(phba->idiag_que_info);
				phba->idiag_que_info = NULL;
			}
			if (phba->idiag_bar_acc) {
				
				debugfs_remove(phba->idiag_bar_acc);
				phba->idiag_bar_acc = NULL;
			}
			if (phba->idiag_pci_cfg) {
				
				debugfs_remove(phba->idiag_pci_cfg);
				phba->idiag_pci_cfg = NULL;
			}

			
			if (phba->idiag_root) {
				
				debugfs_remove(phba->idiag_root);
				phba->idiag_root = NULL;
			}
		}

		if (phba->hba_debugfs_root) {
			debugfs_remove(phba->hba_debugfs_root); 
			phba->hba_debugfs_root = NULL;
			atomic_dec(&lpfc_debugfs_hba_count);
		}

		if (atomic_read(&lpfc_debugfs_hba_count) == 0) {
			debugfs_remove(lpfc_debugfs_root); 
			lpfc_debugfs_root = NULL;
		}
	}
#endif
	return;
}
