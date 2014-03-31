/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2004-2012 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com                                                  *
 * Portions Copyright (C) 2004-2005 Christoph Hellwig              *
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
#include <linux/dma-mapping.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/ctype.h>
#include <linux/aer.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/miscdevice.h>

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

char *_dump_buf_data;
unsigned long _dump_buf_data_order;
char *_dump_buf_dif;
unsigned long _dump_buf_dif_order;
spinlock_t _dump_buf_lock;

static void lpfc_get_hba_model_desc(struct lpfc_hba *, uint8_t *, uint8_t *);
static int lpfc_post_rcv_buf(struct lpfc_hba *);
static int lpfc_sli4_queue_verify(struct lpfc_hba *);
static int lpfc_create_bootstrap_mbox(struct lpfc_hba *);
static int lpfc_setup_endian_order(struct lpfc_hba *);
static void lpfc_destroy_bootstrap_mbox(struct lpfc_hba *);
static void lpfc_free_sgl_list(struct lpfc_hba *);
static int lpfc_init_sgl_list(struct lpfc_hba *);
static int lpfc_init_active_sgl_array(struct lpfc_hba *);
static void lpfc_free_active_sgl(struct lpfc_hba *);
static int lpfc_hba_down_post_s3(struct lpfc_hba *phba);
static int lpfc_hba_down_post_s4(struct lpfc_hba *phba);
static int lpfc_sli4_cq_event_pool_create(struct lpfc_hba *);
static void lpfc_sli4_cq_event_pool_destroy(struct lpfc_hba *);
static void lpfc_sli4_cq_event_release_all(struct lpfc_hba *);

static struct scsi_transport_template *lpfc_transport_template = NULL;
static struct scsi_transport_template *lpfc_vport_transport_template = NULL;
static DEFINE_IDR(lpfc_hba_index);

int
lpfc_config_port_prep(struct lpfc_hba *phba)
{
	lpfc_vpd_t *vp = &phba->vpd;
	int i = 0, rc;
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *mb;
	char *lpfc_vpd_data = NULL;
	uint16_t offset = 0;
	static char licensed[56] =
		    "key unlock for use with gnu public licensed code only\0";
	static int init_key = 1;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		phba->link_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}

	mb = &pmb->u.mb;
	phba->link_state = LPFC_INIT_MBX_CMDS;

	if (lpfc_is_LC_HBA(phba->pcidev->device)) {
		if (init_key) {
			uint32_t *ptext = (uint32_t *) licensed;

			for (i = 0; i < 56; i += sizeof (uint32_t), ptext++)
				*ptext = cpu_to_be32(*ptext);
			init_key = 0;
		}

		lpfc_read_nv(phba, pmb);
		memset((char*)mb->un.varRDnvp.rsvd3, 0,
			sizeof (mb->un.varRDnvp.rsvd3));
		memcpy((char*)mb->un.varRDnvp.rsvd3, licensed,
			 sizeof (licensed));

		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);

		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_MBOX,
					"0324 Config Port initialization "
					"error, mbxCmd x%x READ_NVPARM, "
					"mbxStatus x%x\n",
					mb->mbxCommand, mb->mbxStatus);
			mempool_free(pmb, phba->mbox_mem_pool);
			return -ERESTART;
		}
		memcpy(phba->wwnn, (char *)mb->un.varRDnvp.nodename,
		       sizeof(phba->wwnn));
		memcpy(phba->wwpn, (char *)mb->un.varRDnvp.portname,
		       sizeof(phba->wwpn));
	}

	phba->sli3_options = 0x0;

	
	lpfc_read_rev(phba, pmb);
	rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
	if (rc != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0439 Adapter failed to init, mbxCmd x%x "
				"READ_REV, mbxStatus x%x\n",
				mb->mbxCommand, mb->mbxStatus);
		mempool_free( pmb, phba->mbox_mem_pool);
		return -ERESTART;
	}


	if (mb->un.varRdRev.rr == 0) {
		vp->rev.rBit = 0;
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0440 Adapter failed to init, READ_REV has "
				"missing revision information.\n");
		mempool_free(pmb, phba->mbox_mem_pool);
		return -ERESTART;
	}

	if (phba->sli_rev == 3 && !mb->un.varRdRev.v3rsp) {
		mempool_free(pmb, phba->mbox_mem_pool);
		return -EINVAL;
	}

	
	vp->rev.rBit = 1;
	memcpy(&vp->sli3Feat, &mb->un.varRdRev.sli3Feat, sizeof(uint32_t));
	vp->rev.sli1FwRev = mb->un.varRdRev.sli1FwRev;
	memcpy(vp->rev.sli1FwName, (char*) mb->un.varRdRev.sli1FwName, 16);
	vp->rev.sli2FwRev = mb->un.varRdRev.sli2FwRev;
	memcpy(vp->rev.sli2FwName, (char *) mb->un.varRdRev.sli2FwName, 16);
	vp->rev.biuRev = mb->un.varRdRev.biuRev;
	vp->rev.smRev = mb->un.varRdRev.smRev;
	vp->rev.smFwRev = mb->un.varRdRev.un.smFwRev;
	vp->rev.endecRev = mb->un.varRdRev.endecRev;
	vp->rev.fcphHigh = mb->un.varRdRev.fcphHigh;
	vp->rev.fcphLow = mb->un.varRdRev.fcphLow;
	vp->rev.feaLevelHigh = mb->un.varRdRev.feaLevelHigh;
	vp->rev.feaLevelLow = mb->un.varRdRev.feaLevelLow;
	vp->rev.postKernRev = mb->un.varRdRev.postKernRev;
	vp->rev.opFwRev = mb->un.varRdRev.opFwRev;

	if (vp->rev.feaLevelHigh < 9)
		phba->sli3_options |= LPFC_SLI3_VPORT_TEARDOWN;

	if (lpfc_is_LC_HBA(phba->pcidev->device))
		memcpy(phba->RandomData, (char *)&mb->un.varWords[24],
						sizeof (phba->RandomData));

	
	lpfc_vpd_data = kmalloc(DMP_VPD_SIZE, GFP_KERNEL);
	if (!lpfc_vpd_data)
		goto out_free_mbox;
	do {
		lpfc_dump_mem(phba, pmb, offset, DMP_REGION_VPD);
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);

		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"0441 VPD not present on adapter, "
					"mbxCmd x%x DUMP VPD, mbxStatus x%x\n",
					mb->mbxCommand, mb->mbxStatus);
			mb->un.varDmp.word_cnt = 0;
		}
		if (mb->un.varDmp.word_cnt == 0)
			break;
		if (mb->un.varDmp.word_cnt > DMP_VPD_SIZE - offset)
			mb->un.varDmp.word_cnt = DMP_VPD_SIZE - offset;
		lpfc_sli_pcimem_bcopy(((uint8_t *)mb) + DMP_RSP_OFFSET,
				      lpfc_vpd_data + offset,
				      mb->un.varDmp.word_cnt);
		offset += mb->un.varDmp.word_cnt;
	} while (mb->un.varDmp.word_cnt && offset < DMP_VPD_SIZE);
	lpfc_parse_vpd(phba, lpfc_vpd_data, offset);

	kfree(lpfc_vpd_data);
out_free_mbox:
	mempool_free(pmb, phba->mbox_mem_pool);
	return 0;
}

static void
lpfc_config_async_cmpl(struct lpfc_hba * phba, LPFC_MBOXQ_t * pmboxq)
{
	if (pmboxq->u.mb.mbxStatus == MBX_SUCCESS)
		phba->temp_sensor_support = 1;
	else
		phba->temp_sensor_support = 0;
	mempool_free(pmboxq, phba->mbox_mem_pool);
	return;
}

static void
lpfc_dump_wakeup_param_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmboxq)
{
	struct prog_id *prg;
	uint32_t prog_id_word;
	char dist = ' ';
	
	char dist_char[] = "nabx";

	if (pmboxq->u.mb.mbxStatus != MBX_SUCCESS) {
		mempool_free(pmboxq, phba->mbox_mem_pool);
		return;
	}

	prg = (struct prog_id *) &prog_id_word;

	
	prog_id_word = pmboxq->u.mb.un.varWords[7];

	
	if (prg->dist < 4)
		dist = dist_char[prg->dist];

	if ((prg->dist == 3) && (prg->num == 0))
		sprintf(phba->OptionROMVersion, "%d.%d%d",
			prg->ver, prg->rev, prg->lev);
	else
		sprintf(phba->OptionROMVersion, "%d.%d%d%c%d",
			prg->ver, prg->rev, prg->lev,
			dist, prg->num);
	mempool_free(pmboxq, phba->mbox_mem_pool);
	return;
}

void
lpfc_update_vport_wwn(struct lpfc_vport *vport)
{
	
	if (vport->phba->cfg_soft_wwnn)
		u64_to_wwn(vport->phba->cfg_soft_wwnn,
			   vport->fc_sparam.nodeName.u.wwn);
	if (vport->phba->cfg_soft_wwpn)
		u64_to_wwn(vport->phba->cfg_soft_wwpn,
			   vport->fc_sparam.portName.u.wwn);

	if (vport->fc_nodename.u.wwn[0] == 0 || vport->phba->cfg_soft_wwnn)
		memcpy(&vport->fc_nodename, &vport->fc_sparam.nodeName,
			sizeof(struct lpfc_name));
	else
		memcpy(&vport->fc_sparam.nodeName, &vport->fc_nodename,
			sizeof(struct lpfc_name));

	if (vport->fc_portname.u.wwn[0] == 0 || vport->phba->cfg_soft_wwpn)
		memcpy(&vport->fc_portname, &vport->fc_sparam.portName,
			sizeof(struct lpfc_name));
	else
		memcpy(&vport->fc_sparam.portName, &vport->fc_portname,
			sizeof(struct lpfc_name));
}

int
lpfc_config_port_post(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *mb;
	struct lpfc_dmabuf *mp;
	struct lpfc_sli *psli = &phba->sli;
	uint32_t status, timeout;
	int i, j;
	int rc;

	spin_lock_irq(&phba->hbalock);
	if (phba->over_temp_state == HBA_OVER_TEMP)
		phba->over_temp_state = HBA_NORMAL_TEMP;
	spin_unlock_irq(&phba->hbalock);

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		phba->link_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}
	mb = &pmb->u.mb;

	
	rc = lpfc_read_sparam(phba, pmb, 0);
	if (rc) {
		mempool_free(pmb, phba->mbox_mem_pool);
		return -ENOMEM;
	}

	pmb->vport = vport;
	if (lpfc_sli_issue_mbox(phba, pmb, MBX_POLL) != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0448 Adapter failed init, mbxCmd x%x "
				"READ_SPARM mbxStatus x%x\n",
				mb->mbxCommand, mb->mbxStatus);
		phba->link_state = LPFC_HBA_ERROR;
		mp = (struct lpfc_dmabuf *) pmb->context1;
		mempool_free(pmb, phba->mbox_mem_pool);
		lpfc_mbuf_free(phba, mp->virt, mp->phys);
		kfree(mp);
		return -EIO;
	}

	mp = (struct lpfc_dmabuf *) pmb->context1;

	memcpy(&vport->fc_sparam, mp->virt, sizeof (struct serv_parm));
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	pmb->context1 = NULL;
	lpfc_update_vport_wwn(vport);

	
	fc_host_node_name(shost) = wwn_to_u64(vport->fc_nodename.u.wwn);
	fc_host_port_name(shost) = wwn_to_u64(vport->fc_portname.u.wwn);
	fc_host_max_npiv_vports(shost) = phba->max_vpi;

	
	
	if (phba->SerialNumber[0] == 0) {
		uint8_t *outptr;

		outptr = &vport->fc_nodename.u.s.IEEE[0];
		for (i = 0; i < 12; i++) {
			status = *outptr++;
			j = ((status & 0xf0) >> 4);
			if (j <= 9)
				phba->SerialNumber[i] =
				    (char)((uint8_t) 0x30 + (uint8_t) j);
			else
				phba->SerialNumber[i] =
				    (char)((uint8_t) 0x61 + (uint8_t) (j - 10));
			i++;
			j = (status & 0xf);
			if (j <= 9)
				phba->SerialNumber[i] =
				    (char)((uint8_t) 0x30 + (uint8_t) j);
			else
				phba->SerialNumber[i] =
				    (char)((uint8_t) 0x61 + (uint8_t) (j - 10));
		}
	}

	lpfc_read_config(phba, pmb);
	pmb->vport = vport;
	if (lpfc_sli_issue_mbox(phba, pmb, MBX_POLL) != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0453 Adapter failed to init, mbxCmd x%x "
				"READ_CONFIG, mbxStatus x%x\n",
				mb->mbxCommand, mb->mbxStatus);
		phba->link_state = LPFC_HBA_ERROR;
		mempool_free( pmb, phba->mbox_mem_pool);
		return -EIO;
	}

	
	lpfc_sli_read_link_ste(phba);

	
	if (phba->cfg_hba_queue_depth > (mb->un.varRdConfig.max_xri+1))
		phba->cfg_hba_queue_depth =
			(mb->un.varRdConfig.max_xri + 1) -
					lpfc_sli4_get_els_iocb_cnt(phba);

	phba->lmt = mb->un.varRdConfig.lmt;

	
	lpfc_get_hba_model_desc(phba, phba->ModelName, phba->ModelDesc);

	phba->link_state = LPFC_LINK_DOWN;

	
	if (psli->ring[psli->extra_ring].cmdringaddr)
		psli->ring[psli->extra_ring].flag |= LPFC_STOP_IOCB_EVENT;
	if (psli->ring[psli->fcp_ring].cmdringaddr)
		psli->ring[psli->fcp_ring].flag |= LPFC_STOP_IOCB_EVENT;
	if (psli->ring[psli->next_ring].cmdringaddr)
		psli->ring[psli->next_ring].flag |= LPFC_STOP_IOCB_EVENT;

	
	if (phba->sli_rev != 3)
		lpfc_post_rcv_buf(phba);

	if (phba->intr_type == MSIX) {
		rc = lpfc_config_msi(phba, pmb);
		if (rc) {
			mempool_free(pmb, phba->mbox_mem_pool);
			return -EIO;
		}
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_MBOX,
					"0352 Config MSI mailbox command "
					"failed, mbxCmd x%x, mbxStatus x%x\n",
					pmb->u.mb.mbxCommand,
					pmb->u.mb.mbxStatus);
			mempool_free(pmb, phba->mbox_mem_pool);
			return -EIO;
		}
	}

	spin_lock_irq(&phba->hbalock);
	
	phba->hba_flag &= ~HBA_ERATT_HANDLED;

	
	if (lpfc_readl(phba->HCregaddr, &status)) {
		spin_unlock_irq(&phba->hbalock);
		return -EIO;
	}
	status |= HC_MBINT_ENA | HC_ERINT_ENA | HC_LAINT_ENA;
	if (psli->num_rings > 0)
		status |= HC_R0INT_ENA;
	if (psli->num_rings > 1)
		status |= HC_R1INT_ENA;
	if (psli->num_rings > 2)
		status |= HC_R2INT_ENA;
	if (psli->num_rings > 3)
		status |= HC_R3INT_ENA;

	if ((phba->cfg_poll & ENABLE_FCP_RING_POLLING) &&
	    (phba->cfg_poll & DISABLE_FCP_RING_INT))
		status &= ~(HC_R0INT_ENA);

	writel(status, phba->HCregaddr);
	readl(phba->HCregaddr); 
	spin_unlock_irq(&phba->hbalock);

	
	timeout = phba->fc_ratov * 2;
	mod_timer(&vport->els_tmofunc, jiffies + HZ * timeout);
	
	mod_timer(&phba->hb_tmofunc, jiffies + HZ * LPFC_HB_MBOX_INTERVAL);
	phba->hb_outstanding = 0;
	phba->last_completion_time = jiffies;
	
	mod_timer(&phba->eratt_poll, jiffies + HZ * LPFC_ERATT_POLL_INTERVAL);

	if (phba->hba_flag & LINK_DISABLED) {
		lpfc_printf_log(phba,
			KERN_ERR, LOG_INIT,
			"2598 Adapter Link is disabled.\n");
		lpfc_down_link(phba, pmb);
		pmb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);
		if ((rc != MBX_SUCCESS) && (rc != MBX_BUSY)) {
			lpfc_printf_log(phba,
			KERN_ERR, LOG_INIT,
			"2599 Adapter failed to issue DOWN_LINK"
			" mbox command rc 0x%x\n", rc);

			mempool_free(pmb, phba->mbox_mem_pool);
			return -EIO;
		}
	} else if (phba->cfg_suppress_link_up == LPFC_INITIALIZE_LINK) {
		mempool_free(pmb, phba->mbox_mem_pool);
		rc = phba->lpfc_hba_init_link(phba, MBX_NOWAIT);
		if (rc)
			return rc;
	}
	
	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		phba->link_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}

	lpfc_config_async(phba, pmb, LPFC_ELS_RING);
	pmb->mbox_cmpl = lpfc_config_async_cmpl;
	pmb->vport = phba->pport;
	rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);

	if ((rc != MBX_BUSY) && (rc != MBX_SUCCESS)) {
		lpfc_printf_log(phba,
				KERN_ERR,
				LOG_INIT,
				"0456 Adapter failed to issue "
				"ASYNCEVT_ENABLE mbox status x%x\n",
				rc);
		mempool_free(pmb, phba->mbox_mem_pool);
	}

	
	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		phba->link_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}

	lpfc_dump_wakeup_param(phba, pmb);
	pmb->mbox_cmpl = lpfc_dump_wakeup_param_cmpl;
	pmb->vport = phba->pport;
	rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);

	if ((rc != MBX_BUSY) && (rc != MBX_SUCCESS)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT, "0435 Adapter failed "
				"to get Option ROM version status x%x\n", rc);
		mempool_free(pmb, phba->mbox_mem_pool);
	}

	return 0;
}

int
lpfc_hba_init_link(struct lpfc_hba *phba, uint32_t flag)
{
	return lpfc_hba_init_link_fc_topology(phba, phba->cfg_topology, flag);
}

int
lpfc_hba_init_link_fc_topology(struct lpfc_hba *phba, uint32_t fc_topology,
			       uint32_t flag)
{
	struct lpfc_vport *vport = phba->pport;
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *mb;
	int rc;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		phba->link_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}
	mb = &pmb->u.mb;
	pmb->vport = vport;

	if ((phba->cfg_link_speed > LPFC_USER_LINK_SPEED_MAX) ||
	    ((phba->cfg_link_speed == LPFC_USER_LINK_SPEED_1G) &&
	     !(phba->lmt & LMT_1Gb)) ||
	    ((phba->cfg_link_speed == LPFC_USER_LINK_SPEED_2G) &&
	     !(phba->lmt & LMT_2Gb)) ||
	    ((phba->cfg_link_speed == LPFC_USER_LINK_SPEED_4G) &&
	     !(phba->lmt & LMT_4Gb)) ||
	    ((phba->cfg_link_speed == LPFC_USER_LINK_SPEED_8G) &&
	     !(phba->lmt & LMT_8Gb)) ||
	    ((phba->cfg_link_speed == LPFC_USER_LINK_SPEED_10G) &&
	     !(phba->lmt & LMT_10Gb)) ||
	    ((phba->cfg_link_speed == LPFC_USER_LINK_SPEED_16G) &&
	     !(phba->lmt & LMT_16Gb))) {
		
		lpfc_printf_log(phba, KERN_ERR, LOG_LINK_EVENT,
			"1302 Invalid speed for this board:%d "
			"Reset link speed to auto.\n",
			phba->cfg_link_speed);
			phba->cfg_link_speed = LPFC_USER_LINK_SPEED_AUTO;
	}
	lpfc_init_link(phba, pmb, fc_topology, phba->cfg_link_speed);
	pmb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	if (phba->sli_rev < LPFC_SLI_REV4)
		lpfc_set_loopback_flag(phba);
	rc = lpfc_sli_issue_mbox(phba, pmb, flag);
	if ((rc != MBX_BUSY) && (rc != MBX_SUCCESS)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"0498 Adapter failed to init, mbxCmd x%x "
			"INIT_LINK, mbxStatus x%x\n",
			mb->mbxCommand, mb->mbxStatus);
		if (phba->sli_rev <= LPFC_SLI_REV3) {
			
			writel(0, phba->HCregaddr);
			readl(phba->HCregaddr); 
			
			writel(0xffffffff, phba->HAregaddr);
			readl(phba->HAregaddr); 
		}
		phba->link_state = LPFC_HBA_ERROR;
		if (rc != MBX_BUSY || flag == MBX_POLL)
			mempool_free(pmb, phba->mbox_mem_pool);
		return -EIO;
	}
	phba->cfg_suppress_link_up = LPFC_INITIALIZE_LINK;
	if (flag == MBX_POLL)
		mempool_free(pmb, phba->mbox_mem_pool);

	return 0;
}

int
lpfc_hba_down_link(struct lpfc_hba *phba, uint32_t flag)
{
	LPFC_MBOXQ_t *pmb;
	int rc;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		phba->link_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}

	lpfc_printf_log(phba,
		KERN_ERR, LOG_INIT,
		"0491 Adapter Link is disabled.\n");
	lpfc_down_link(phba, pmb);
	pmb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	rc = lpfc_sli_issue_mbox(phba, pmb, flag);
	if ((rc != MBX_SUCCESS) && (rc != MBX_BUSY)) {
		lpfc_printf_log(phba,
		KERN_ERR, LOG_INIT,
		"2522 Adapter failed to issue DOWN_LINK"
		" mbox command rc 0x%x\n", rc);

		mempool_free(pmb, phba->mbox_mem_pool);
		return -EIO;
	}
	if (flag == MBX_POLL)
		mempool_free(pmb, phba->mbox_mem_pool);

	return 0;
}

int
lpfc_hba_down_prep(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	int i;

	if (phba->sli_rev <= LPFC_SLI_REV3) {
		
		writel(0, phba->HCregaddr);
		readl(phba->HCregaddr); 
	}

	if (phba->pport->load_flag & FC_UNLOADING)
		lpfc_cleanup_discovery_resources(phba->pport);
	else {
		vports = lpfc_create_vport_work_array(phba);
		if (vports != NULL)
			for (i = 0; i <= phba->max_vports &&
				vports[i] != NULL; i++)
				lpfc_cleanup_discovery_resources(vports[i]);
		lpfc_destroy_vport_work_array(phba, vports);
	}
	return 0;
}

static int
lpfc_hba_down_post_s3(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring *pring;
	struct lpfc_dmabuf *mp, *next_mp;
	LIST_HEAD(completions);
	int i;

	if (phba->sli3_options & LPFC_SLI3_HBQ_ENABLED)
		lpfc_sli_hbqbuf_free_all(phba);
	else {
		
		pring = &psli->ring[LPFC_ELS_RING];
		list_for_each_entry_safe(mp, next_mp, &pring->postbufq, list) {
			list_del(&mp->list);
			pring->postbufq_cnt--;
			lpfc_mbuf_free(phba, mp->virt, mp->phys);
			kfree(mp);
		}
	}

	spin_lock_irq(&phba->hbalock);
	for (i = 0; i < psli->num_rings; i++) {
		pring = &psli->ring[i];

		list_splice_init(&pring->txcmplq, &completions);
		pring->txcmplq_cnt = 0;
		spin_unlock_irq(&phba->hbalock);

		
		lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
				      IOERR_SLI_ABORTED);

		lpfc_sli_abort_iocb_ring(phba, pring);
		spin_lock_irq(&phba->hbalock);
	}
	spin_unlock_irq(&phba->hbalock);

	return 0;
}

static int
lpfc_hba_down_post_s4(struct lpfc_hba *phba)
{
	struct lpfc_scsi_buf *psb, *psb_next;
	LIST_HEAD(aborts);
	int ret;
	unsigned long iflag = 0;
	struct lpfc_sglq *sglq_entry = NULL;

	ret = lpfc_hba_down_post_s3(phba);
	if (ret)
		return ret;
	spin_lock_irq(&phba->hbalock);  
					
	spin_lock(&phba->sli4_hba.abts_sgl_list_lock);
	list_for_each_entry(sglq_entry,
		&phba->sli4_hba.lpfc_abts_els_sgl_list, list)
		sglq_entry->state = SGL_FREED;

	list_splice_init(&phba->sli4_hba.lpfc_abts_els_sgl_list,
			&phba->sli4_hba.lpfc_sgl_list);
	spin_unlock(&phba->sli4_hba.abts_sgl_list_lock);
	spin_lock(&phba->sli4_hba.abts_scsi_buf_list_lock);
	list_splice_init(&phba->sli4_hba.lpfc_abts_scsi_buf_list,
			&aborts);
	spin_unlock(&phba->sli4_hba.abts_scsi_buf_list_lock);
	spin_unlock_irq(&phba->hbalock);

	list_for_each_entry_safe(psb, psb_next, &aborts, list) {
		psb->pCmd = NULL;
		psb->status = IOSTAT_SUCCESS;
	}
	spin_lock_irqsave(&phba->scsi_buf_list_lock, iflag);
	list_splice(&aborts, &phba->lpfc_scsi_buf_list);
	spin_unlock_irqrestore(&phba->scsi_buf_list_lock, iflag);
	return 0;
}

int
lpfc_hba_down_post(struct lpfc_hba *phba)
{
	return (*phba->lpfc_hba_down_post)(phba);
}

static void
lpfc_hb_timeout(unsigned long ptr)
{
	struct lpfc_hba *phba;
	uint32_t tmo_posted;
	unsigned long iflag;

	phba = (struct lpfc_hba *)ptr;

	
	spin_lock_irqsave(&phba->pport->work_port_lock, iflag);
	tmo_posted = phba->pport->work_port_events & WORKER_HB_TMO;
	if (!tmo_posted)
		phba->pport->work_port_events |= WORKER_HB_TMO;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, iflag);

	
	if (!tmo_posted)
		lpfc_worker_wake_up(phba);
	return;
}

static void
lpfc_rrq_timeout(unsigned long ptr)
{
	struct lpfc_hba *phba;
	unsigned long iflag;

	phba = (struct lpfc_hba *)ptr;
	spin_lock_irqsave(&phba->pport->work_port_lock, iflag);
	phba->hba_flag |= HBA_RRQ_ACTIVE;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, iflag);
	lpfc_worker_wake_up(phba);
}

static void
lpfc_hb_mbox_cmpl(struct lpfc_hba * phba, LPFC_MBOXQ_t * pmboxq)
{
	unsigned long drvr_flag;

	spin_lock_irqsave(&phba->hbalock, drvr_flag);
	phba->hb_outstanding = 0;
	spin_unlock_irqrestore(&phba->hbalock, drvr_flag);

	
	mempool_free(pmboxq, phba->mbox_mem_pool);
	if (!(phba->pport->fc_flag & FC_OFFLINE_MODE) &&
		!(phba->link_state == LPFC_HBA_ERROR) &&
		!(phba->pport->load_flag & FC_UNLOADING))
		mod_timer(&phba->hb_tmofunc,
			jiffies + HZ * LPFC_HB_MBOX_INTERVAL);
	return;
}

void
lpfc_hb_timeout_handler(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	LPFC_MBOXQ_t *pmboxq;
	struct lpfc_dmabuf *buf_ptr;
	int retval, i;
	struct lpfc_sli *psli = &phba->sli;
	LIST_HEAD(completions);

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++)
			lpfc_rcv_seq_check_edtov(vports[i]);
	lpfc_destroy_vport_work_array(phba, vports);

	if ((phba->link_state == LPFC_HBA_ERROR) ||
		(phba->pport->load_flag & FC_UNLOADING) ||
		(phba->pport->fc_flag & FC_OFFLINE_MODE))
		return;

	spin_lock_irq(&phba->pport->work_port_lock);

	if (time_after(phba->last_completion_time + LPFC_HB_MBOX_INTERVAL * HZ,
		jiffies)) {
		spin_unlock_irq(&phba->pport->work_port_lock);
		if (!phba->hb_outstanding)
			mod_timer(&phba->hb_tmofunc,
				jiffies + HZ * LPFC_HB_MBOX_INTERVAL);
		else
			mod_timer(&phba->hb_tmofunc,
				jiffies + HZ * LPFC_HB_MBOX_TIMEOUT);
		return;
	}
	spin_unlock_irq(&phba->pport->work_port_lock);

	if (phba->elsbuf_cnt &&
		(phba->elsbuf_cnt == phba->elsbuf_prev_cnt)) {
		spin_lock_irq(&phba->hbalock);
		list_splice_init(&phba->elsbuf, &completions);
		phba->elsbuf_cnt = 0;
		phba->elsbuf_prev_cnt = 0;
		spin_unlock_irq(&phba->hbalock);

		while (!list_empty(&completions)) {
			list_remove_head(&completions, buf_ptr,
				struct lpfc_dmabuf, list);
			lpfc_mbuf_free(phba, buf_ptr->virt, buf_ptr->phys);
			kfree(buf_ptr);
		}
	}
	phba->elsbuf_prev_cnt = phba->elsbuf_cnt;

	
	if (phba->cfg_enable_hba_heartbeat) {
		if (!phba->hb_outstanding) {
			if ((!(psli->sli_flag & LPFC_SLI_MBOX_ACTIVE)) &&
				(list_empty(&psli->mboxq))) {
				pmboxq = mempool_alloc(phba->mbox_mem_pool,
							GFP_KERNEL);
				if (!pmboxq) {
					mod_timer(&phba->hb_tmofunc,
						 jiffies +
						 HZ * LPFC_HB_MBOX_INTERVAL);
					return;
				}

				lpfc_heart_beat(phba, pmboxq);
				pmboxq->mbox_cmpl = lpfc_hb_mbox_cmpl;
				pmboxq->vport = phba->pport;
				retval = lpfc_sli_issue_mbox(phba, pmboxq,
						MBX_NOWAIT);

				if (retval != MBX_BUSY &&
					retval != MBX_SUCCESS) {
					mempool_free(pmboxq,
							phba->mbox_mem_pool);
					mod_timer(&phba->hb_tmofunc,
						jiffies +
						HZ * LPFC_HB_MBOX_INTERVAL);
					return;
				}
				phba->skipped_hb = 0;
				phba->hb_outstanding = 1;
			} else if (time_before_eq(phba->last_completion_time,
					phba->skipped_hb)) {
				lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"2857 Last completion time not "
					" updated in %d ms\n",
					jiffies_to_msecs(jiffies
						 - phba->last_completion_time));
			} else
				phba->skipped_hb = jiffies;

			mod_timer(&phba->hb_tmofunc,
				  jiffies + HZ * LPFC_HB_MBOX_TIMEOUT);
			return;
		} else {
			lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
					"0459 Adapter heartbeat still out"
					"standing:last compl time was %d ms.\n",
					jiffies_to_msecs(jiffies
						 - phba->last_completion_time));
			mod_timer(&phba->hb_tmofunc,
				  jiffies + HZ * LPFC_HB_MBOX_TIMEOUT);
		}
	}
}

static void
lpfc_offline_eratt(struct lpfc_hba *phba)
{
	struct lpfc_sli   *psli = &phba->sli;

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~LPFC_SLI_ACTIVE;
	spin_unlock_irq(&phba->hbalock);
	lpfc_offline_prep(phba);

	lpfc_offline(phba);
	lpfc_reset_barrier(phba);
	spin_lock_irq(&phba->hbalock);
	lpfc_sli_brdreset(phba);
	spin_unlock_irq(&phba->hbalock);
	lpfc_hba_down_post(phba);
	lpfc_sli_brdready(phba, HS_MBRDY);
	lpfc_unblock_mgmt_io(phba);
	phba->link_state = LPFC_HBA_ERROR;
	return;
}

static void
lpfc_sli4_offline_eratt(struct lpfc_hba *phba)
{
	lpfc_offline_prep(phba);
	lpfc_offline(phba);
	lpfc_sli4_brdreset(phba);
	lpfc_hba_down_post(phba);
	lpfc_sli4_post_status_check(phba);
	lpfc_unblock_mgmt_io(phba);
	phba->link_state = LPFC_HBA_ERROR;
}

static void
lpfc_handle_deferred_eratt(struct lpfc_hba *phba)
{
	uint32_t old_host_status = phba->work_hs;
	struct lpfc_sli_ring  *pring;
	struct lpfc_sli *psli = &phba->sli;

	if (pci_channel_offline(phba->pcidev)) {
		spin_lock_irq(&phba->hbalock);
		phba->hba_flag &= ~DEFER_ERATT;
		spin_unlock_irq(&phba->hbalock);
		return;
	}

	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
		"0479 Deferred Adapter Hardware Error "
		"Data: x%x x%x x%x\n",
		phba->work_hs,
		phba->work_status[0], phba->work_status[1]);

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~LPFC_SLI_ACTIVE;
	spin_unlock_irq(&phba->hbalock);


	pring = &psli->ring[psli->fcp_ring];
	lpfc_sli_abort_iocb_ring(phba, pring);

	lpfc_offline_prep(phba);
	lpfc_offline(phba);

	
	while (phba->work_hs & HS_FFER1) {
		msleep(100);
		if (lpfc_readl(phba->HSregaddr, &phba->work_hs)) {
			phba->work_hs = UNPLUG_ERR ;
			break;
		}
		
		if (phba->pport->load_flag & FC_UNLOADING) {
			phba->work_hs = 0;
			break;
		}
	}

	if ((!phba->work_hs) && (!(phba->pport->load_flag & FC_UNLOADING)))
		phba->work_hs = old_host_status & ~HS_FFER1;

	spin_lock_irq(&phba->hbalock);
	phba->hba_flag &= ~DEFER_ERATT;
	spin_unlock_irq(&phba->hbalock);
	phba->work_status[0] = readl(phba->MBslimaddr + 0xa8);
	phba->work_status[1] = readl(phba->MBslimaddr + 0xac);
}

static void
lpfc_board_errevt_to_mgmt(struct lpfc_hba *phba)
{
	struct lpfc_board_event_header board_event;
	struct Scsi_Host *shost;

	board_event.event_type = FC_REG_BOARD_EVENT;
	board_event.subcategory = LPFC_EVENT_PORTINTERR;
	shost = lpfc_shost_from_vport(phba->pport);
	fc_host_post_vendor_event(shost, fc_get_event_number(),
				  sizeof(board_event),
				  (char *) &board_event,
				  LPFC_NL_VENDOR_ID);
}

static void
lpfc_handle_eratt_s3(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;
	struct lpfc_sli   *psli = &phba->sli;
	struct lpfc_sli_ring  *pring;
	uint32_t event_data;
	unsigned long temperature;
	struct temp_event temp_event_data;
	struct Scsi_Host  *shost;

	if (pci_channel_offline(phba->pcidev)) {
		spin_lock_irq(&phba->hbalock);
		phba->hba_flag &= ~DEFER_ERATT;
		spin_unlock_irq(&phba->hbalock);
		return;
	}

	
	if (!phba->cfg_enable_hba_reset)
		return;

	
	lpfc_board_errevt_to_mgmt(phba);

	if (phba->hba_flag & DEFER_ERATT)
		lpfc_handle_deferred_eratt(phba);

	if ((phba->work_hs & HS_FFER6) || (phba->work_hs & HS_FFER8)) {
		if (phba->work_hs & HS_FFER6)
			
			lpfc_printf_log(phba, KERN_INFO, LOG_LINK_EVENT,
					"1301 Re-establishing Link "
					"Data: x%x x%x x%x\n",
					phba->work_hs, phba->work_status[0],
					phba->work_status[1]);
		if (phba->work_hs & HS_FFER8)
			
			lpfc_printf_log(phba, KERN_INFO, LOG_LINK_EVENT,
					"2861 Host Authentication device "
					"zeroization Data:x%x x%x x%x\n",
					phba->work_hs, phba->work_status[0],
					phba->work_status[1]);

		spin_lock_irq(&phba->hbalock);
		psli->sli_flag &= ~LPFC_SLI_ACTIVE;
		spin_unlock_irq(&phba->hbalock);

		pring = &psli->ring[psli->fcp_ring];
		lpfc_sli_abort_iocb_ring(phba, pring);

		lpfc_offline_prep(phba);
		lpfc_offline(phba);
		lpfc_sli_brdrestart(phba);
		if (lpfc_online(phba) == 0) {	
			lpfc_unblock_mgmt_io(phba);
			return;
		}
		lpfc_unblock_mgmt_io(phba);
	} else if (phba->work_hs & HS_CRIT_TEMP) {
		temperature = readl(phba->MBslimaddr + TEMPERATURE_OFFSET);
		temp_event_data.event_type = FC_REG_TEMPERATURE_EVENT;
		temp_event_data.event_code = LPFC_CRIT_TEMP;
		temp_event_data.data = (uint32_t)temperature;

		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0406 Adapter maximum temperature exceeded "
				"(%ld), taking this port offline "
				"Data: x%x x%x x%x\n",
				temperature, phba->work_hs,
				phba->work_status[0], phba->work_status[1]);

		shost = lpfc_shost_from_vport(phba->pport);
		fc_host_post_vendor_event(shost, fc_get_event_number(),
					  sizeof(temp_event_data),
					  (char *) &temp_event_data,
					  SCSI_NL_VID_TYPE_PCI
					  | PCI_VENDOR_ID_EMULEX);

		spin_lock_irq(&phba->hbalock);
		phba->over_temp_state = HBA_OVER_TEMP;
		spin_unlock_irq(&phba->hbalock);
		lpfc_offline_eratt(phba);

	} else {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0457 Adapter Hardware Error "
				"Data: x%x x%x x%x\n",
				phba->work_hs,
				phba->work_status[0], phba->work_status[1]);

		event_data = FC_REG_DUMP_EVENT;
		shost = lpfc_shost_from_vport(vport);
		fc_host_post_vendor_event(shost, fc_get_event_number(),
				sizeof(event_data), (char *) &event_data,
				SCSI_NL_VID_TYPE_PCI | PCI_VENDOR_ID_EMULEX);

		lpfc_offline_eratt(phba);
	}
	return;
}

static void
lpfc_handle_eratt_s4(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;
	uint32_t event_data;
	struct Scsi_Host *shost;
	uint32_t if_type;
	struct lpfc_register portstat_reg = {0};
	uint32_t reg_err1, reg_err2;
	uint32_t uerrlo_reg, uemasklo_reg;
	uint32_t pci_rd_rc1, pci_rd_rc2;
	int rc;

	if (pci_channel_offline(phba->pcidev))
		return;
	
	if (!phba->cfg_enable_hba_reset)
		return;

	if_type = bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf);
	switch (if_type) {
	case LPFC_SLI_INTF_IF_TYPE_0:
		pci_rd_rc1 = lpfc_readl(
				phba->sli4_hba.u.if_type0.UERRLOregaddr,
				&uerrlo_reg);
		pci_rd_rc2 = lpfc_readl(
				phba->sli4_hba.u.if_type0.UEMASKLOregaddr,
				&uemasklo_reg);
		
		if (pci_rd_rc1 == -EIO && pci_rd_rc2 == -EIO)
			return;
		lpfc_sli4_offline_eratt(phba);
		break;
	case LPFC_SLI_INTF_IF_TYPE_2:
		pci_rd_rc1 = lpfc_readl(
				phba->sli4_hba.u.if_type2.STATUSregaddr,
				&portstat_reg.word0);
		
		if (pci_rd_rc1 == -EIO) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"3151 PCI bus read access failure: x%x\n",
				readl(phba->sli4_hba.u.if_type2.STATUSregaddr));
			return;
		}
		reg_err1 = readl(phba->sli4_hba.u.if_type2.ERR1regaddr);
		reg_err2 = readl(phba->sli4_hba.u.if_type2.ERR2regaddr);
		if (bf_get(lpfc_sliport_status_oti, &portstat_reg)) {
			
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2889 Port Overtemperature event, "
				"taking port offline\n");
			spin_lock_irq(&phba->hbalock);
			phba->over_temp_state = HBA_OVER_TEMP;
			spin_unlock_irq(&phba->hbalock);
			lpfc_sli4_offline_eratt(phba);
			break;
		}
		if (reg_err1 == SLIPORT_ERR1_REG_ERR_CODE_2 &&
		    reg_err2 == SLIPORT_ERR2_REG_FW_RESTART)
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"3143 Port Down: Firmware Restarted\n");
		else if (reg_err1 == SLIPORT_ERR1_REG_ERR_CODE_2 &&
			 reg_err2 == SLIPORT_ERR2_REG_FORCED_DUMP)
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"3144 Port Down: Debug Dump\n");
		else if (reg_err1 == SLIPORT_ERR1_REG_ERR_CODE_2 &&
			 reg_err2 == SLIPORT_ERR2_REG_FUNC_PROVISON)
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"3145 Port Down: Provisioning\n");
		rc = lpfc_sli4_pdev_status_reg_wait(phba);
		if (!rc) {
			
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"2887 Reset Needed: Attempting Port "
					"Recovery...\n");
			lpfc_offline_prep(phba);
			lpfc_offline(phba);
			lpfc_sli_brdrestart(phba);
			if (lpfc_online(phba) == 0) {
				lpfc_unblock_mgmt_io(phba);
				
				if (reg_err1 == SLIPORT_ERR1_REG_ERR_CODE_2 &&
				    reg_err2 == SLIPORT_ERR2_REG_FORCED_DUMP)
					return;
				else
					break;
			}
			
		}
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"3152 Unrecoverable error, bring the port "
				"offline\n");
		lpfc_sli4_offline_eratt(phba);
		break;
	case LPFC_SLI_INTF_IF_TYPE_1:
	default:
		break;
	}
	lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
			"3123 Report dump event to upper layer\n");
	
	lpfc_board_errevt_to_mgmt(phba);

	event_data = FC_REG_DUMP_EVENT;
	shost = lpfc_shost_from_vport(vport);
	fc_host_post_vendor_event(shost, fc_get_event_number(),
				  sizeof(event_data), (char *) &event_data,
				  SCSI_NL_VID_TYPE_PCI | PCI_VENDOR_ID_EMULEX);
}

void
lpfc_handle_eratt(struct lpfc_hba *phba)
{
	(*phba->lpfc_handle_eratt)(phba);
}

void
lpfc_handle_latt(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;
	struct lpfc_sli   *psli = &phba->sli;
	LPFC_MBOXQ_t *pmb;
	volatile uint32_t control;
	struct lpfc_dmabuf *mp;
	int rc = 0;

	pmb = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		rc = 1;
		goto lpfc_handle_latt_err_exit;
	}

	mp = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (!mp) {
		rc = 2;
		goto lpfc_handle_latt_free_pmb;
	}

	mp->virt = lpfc_mbuf_alloc(phba, 0, &mp->phys);
	if (!mp->virt) {
		rc = 3;
		goto lpfc_handle_latt_free_mp;
	}

	
	lpfc_els_flush_all_cmd(phba);

	psli->slistat.link_event++;
	lpfc_read_topology(phba, pmb, mp);
	pmb->mbox_cmpl = lpfc_mbx_cmpl_read_topology;
	pmb->vport = vport;
	
	phba->sli.ring[LPFC_ELS_RING].flag |= LPFC_STOP_IOCB_EVENT;
	rc = lpfc_sli_issue_mbox (phba, pmb, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		rc = 4;
		goto lpfc_handle_latt_free_mbuf;
	}

	
	spin_lock_irq(&phba->hbalock);
	writel(HA_LATT, phba->HAregaddr);
	readl(phba->HAregaddr); 
	spin_unlock_irq(&phba->hbalock);

	return;

lpfc_handle_latt_free_mbuf:
	phba->sli.ring[LPFC_ELS_RING].flag &= ~LPFC_STOP_IOCB_EVENT;
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
lpfc_handle_latt_free_mp:
	kfree(mp);
lpfc_handle_latt_free_pmb:
	mempool_free(pmb, phba->mbox_mem_pool);
lpfc_handle_latt_err_exit:
	
	spin_lock_irq(&phba->hbalock);
	psli->sli_flag |= LPFC_PROCESS_LA;
	control = readl(phba->HCregaddr);
	control |= HC_LAINT_ENA;
	writel(control, phba->HCregaddr);
	readl(phba->HCregaddr); 

	
	writel(HA_LATT, phba->HAregaddr);
	readl(phba->HAregaddr); 
	spin_unlock_irq(&phba->hbalock);
	lpfc_linkdown(phba);
	phba->link_state = LPFC_HBA_ERROR;

	lpfc_printf_log(phba, KERN_ERR, LOG_MBOX,
		     "0300 LATT: Cannot issue READ_LA: Data:%d\n", rc);

	return;
}

int
lpfc_parse_vpd(struct lpfc_hba *phba, uint8_t *vpd, int len)
{
	uint8_t lenlo, lenhi;
	int Length;
	int i, j;
	int finished = 0;
	int index = 0;

	if (!vpd)
		return 0;

	
	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"0455 Vital Product Data: x%x x%x x%x x%x\n",
			(uint32_t) vpd[0], (uint32_t) vpd[1], (uint32_t) vpd[2],
			(uint32_t) vpd[3]);
	while (!finished && (index < (len - 4))) {
		switch (vpd[index]) {
		case 0x82:
		case 0x91:
			index += 1;
			lenlo = vpd[index];
			index += 1;
			lenhi = vpd[index];
			index += 1;
			i = ((((unsigned short)lenhi) << 8) + lenlo);
			index += i;
			break;
		case 0x90:
			index += 1;
			lenlo = vpd[index];
			index += 1;
			lenhi = vpd[index];
			index += 1;
			Length = ((((unsigned short)lenhi) << 8) + lenlo);
			if (Length > len - index)
				Length = len - index;
			while (Length > 0) {
			
			if ((vpd[index] == 'S') && (vpd[index+1] == 'N')) {
				index += 2;
				i = vpd[index];
				index += 1;
				j = 0;
				Length -= (3+i);
				while(i--) {
					phba->SerialNumber[j++] = vpd[index++];
					if (j == 31)
						break;
				}
				phba->SerialNumber[j] = 0;
				continue;
			}
			else if ((vpd[index] == 'V') && (vpd[index+1] == '1')) {
				phba->vpd_flag |= VPD_MODEL_DESC;
				index += 2;
				i = vpd[index];
				index += 1;
				j = 0;
				Length -= (3+i);
				while(i--) {
					phba->ModelDesc[j++] = vpd[index++];
					if (j == 255)
						break;
				}
				phba->ModelDesc[j] = 0;
				continue;
			}
			else if ((vpd[index] == 'V') && (vpd[index+1] == '2')) {
				phba->vpd_flag |= VPD_MODEL_NAME;
				index += 2;
				i = vpd[index];
				index += 1;
				j = 0;
				Length -= (3+i);
				while(i--) {
					phba->ModelName[j++] = vpd[index++];
					if (j == 79)
						break;
				}
				phba->ModelName[j] = 0;
				continue;
			}
			else if ((vpd[index] == 'V') && (vpd[index+1] == '3')) {
				phba->vpd_flag |= VPD_PROGRAM_TYPE;
				index += 2;
				i = vpd[index];
				index += 1;
				j = 0;
				Length -= (3+i);
				while(i--) {
					phba->ProgramType[j++] = vpd[index++];
					if (j == 255)
						break;
				}
				phba->ProgramType[j] = 0;
				continue;
			}
			else if ((vpd[index] == 'V') && (vpd[index+1] == '4')) {
				phba->vpd_flag |= VPD_PORT;
				index += 2;
				i = vpd[index];
				index += 1;
				j = 0;
				Length -= (3+i);
				while(i--) {
					if ((phba->sli_rev == LPFC_SLI_REV4) &&
					    (phba->sli4_hba.pport_name_sta ==
					     LPFC_SLI4_PPNAME_GET)) {
						j++;
						index++;
					} else
						phba->Port[j++] = vpd[index++];
					if (j == 19)
						break;
				}
				if ((phba->sli_rev != LPFC_SLI_REV4) ||
				    (phba->sli4_hba.pport_name_sta ==
				     LPFC_SLI4_PPNAME_NON))
					phba->Port[j] = 0;
				continue;
			}
			else {
				index += 2;
				i = vpd[index];
				index += 1;
				index += i;
				Length -= (3 + i);
			}
		}
		finished = 0;
		break;
		case 0x78:
			finished = 1;
			break;
		default:
			index ++;
			break;
		}
	}

	return(1);
}

static void
lpfc_get_hba_model_desc(struct lpfc_hba *phba, uint8_t *mdp, uint8_t *descp)
{
	lpfc_vpd_t *vp;
	uint16_t dev_id = phba->pcidev->device;
	int max_speed;
	int GE = 0;
	int oneConnect = 0; 
	struct {
		char *name;
		char *bus;
		char *function;
	} m = {"<Unknown>", "", ""};

	if (mdp && mdp[0] != '\0'
		&& descp && descp[0] != '\0')
		return;

	if (phba->lmt & LMT_16Gb)
		max_speed = 16;
	else if (phba->lmt & LMT_10Gb)
		max_speed = 10;
	else if (phba->lmt & LMT_8Gb)
		max_speed = 8;
	else if (phba->lmt & LMT_4Gb)
		max_speed = 4;
	else if (phba->lmt & LMT_2Gb)
		max_speed = 2;
	else
		max_speed = 1;

	vp = &phba->vpd;

	switch (dev_id) {
	case PCI_DEVICE_ID_FIREFLY:
		m = (typeof(m)){"LP6000", "PCI", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_SUPERFLY:
		if (vp->rev.biuRev >= 1 && vp->rev.biuRev <= 3)
			m = (typeof(m)){"LP7000", "PCI",
					"Fibre Channel Adapter"};
		else
			m = (typeof(m)){"LP7000E", "PCI",
					"Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_DRAGONFLY:
		m = (typeof(m)){"LP8000", "PCI",
				"Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_CENTAUR:
		if (FC_JEDEC_ID(vp->rev.biuRev) == CENTAUR_2G_JEDEC_ID)
			m = (typeof(m)){"LP9002", "PCI",
					"Fibre Channel Adapter"};
		else
			m = (typeof(m)){"LP9000", "PCI",
					"Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_RFLY:
		m = (typeof(m)){"LP952", "PCI",
				"Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_PEGASUS:
		m = (typeof(m)){"LP9802", "PCI-X",
				"Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_THOR:
		m = (typeof(m)){"LP10000", "PCI-X",
				"Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_VIPER:
		m = (typeof(m)){"LPX1000",  "PCI-X",
				"Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_PFLY:
		m = (typeof(m)){"LP982", "PCI-X",
				"Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_TFLY:
		m = (typeof(m)){"LP1050", "PCI-X",
				"Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_HELIOS:
		m = (typeof(m)){"LP11000", "PCI-X2",
				"Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_HELIOS_SCSP:
		m = (typeof(m)){"LP11000-SP", "PCI-X2",
				"Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_HELIOS_DCSP:
		m = (typeof(m)){"LP11002-SP",  "PCI-X2",
				"Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_NEPTUNE:
		m = (typeof(m)){"LPe1000", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_NEPTUNE_SCSP:
		m = (typeof(m)){"LPe1000-SP", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_NEPTUNE_DCSP:
		m = (typeof(m)){"LPe1002-SP", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_BMID:
		m = (typeof(m)){"LP1150", "PCI-X2", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_BSMB:
		m = (typeof(m)){"LP111", "PCI-X2", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_ZEPHYR:
		m = (typeof(m)){"LPe11000", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_ZEPHYR_SCSP:
		m = (typeof(m)){"LPe11000", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_ZEPHYR_DCSP:
		m = (typeof(m)){"LP2105", "PCIe", "FCoE Adapter"};
		GE = 1;
		break;
	case PCI_DEVICE_ID_ZMID:
		m = (typeof(m)){"LPe1150", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_ZSMB:
		m = (typeof(m)){"LPe111", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_LP101:
		m = (typeof(m)){"LP101", "PCI-X", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_LP10000S:
		m = (typeof(m)){"LP10000-S", "PCI", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_LP11000S:
		m = (typeof(m)){"LP11000-S", "PCI-X2", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_LPE11000S:
		m = (typeof(m)){"LPe11000-S", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_SAT:
		m = (typeof(m)){"LPe12000", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_SAT_MID:
		m = (typeof(m)){"LPe1250", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_SAT_SMB:
		m = (typeof(m)){"LPe121", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_SAT_DCSP:
		m = (typeof(m)){"LPe12002-SP", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_SAT_SCSP:
		m = (typeof(m)){"LPe12000-SP", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_SAT_S:
		m = (typeof(m)){"LPe12000-S", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_HORNET:
		m = (typeof(m)){"LP21000", "PCIe", "FCoE Adapter"};
		GE = 1;
		break;
	case PCI_DEVICE_ID_PROTEUS_VF:
		m = (typeof(m)){"LPev12000", "PCIe IOV",
				"Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_PROTEUS_PF:
		m = (typeof(m)){"LPev12000", "PCIe IOV",
				"Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_PROTEUS_S:
		m = (typeof(m)){"LPemv12002-S", "PCIe IOV",
				"Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_TIGERSHARK:
		oneConnect = 1;
		m = (typeof(m)){"OCe10100", "PCIe", "FCoE"};
		break;
	case PCI_DEVICE_ID_TOMCAT:
		oneConnect = 1;
		m = (typeof(m)){"OCe11100", "PCIe", "FCoE"};
		break;
	case PCI_DEVICE_ID_FALCON:
		m = (typeof(m)){"LPSe12002-ML1-E", "PCIe",
				"EmulexSecure Fibre"};
		break;
	case PCI_DEVICE_ID_BALIUS:
		m = (typeof(m)){"LPVe12002", "PCIe Shared I/O",
				"Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_LANCER_FC:
	case PCI_DEVICE_ID_LANCER_FC_VF:
		m = (typeof(m)){"LPe16000", "PCIe", "Fibre Channel Adapter"};
		break;
	case PCI_DEVICE_ID_LANCER_FCOE:
	case PCI_DEVICE_ID_LANCER_FCOE_VF:
		oneConnect = 1;
		m = (typeof(m)){"OCe15100", "PCIe", "FCoE"};
		break;
	default:
		m = (typeof(m)){"Unknown", "", ""};
		break;
	}

	if (mdp && mdp[0] == '\0')
		snprintf(mdp, 79,"%s", m.name);
	if (descp && descp[0] == '\0') {
		if (oneConnect)
			snprintf(descp, 255,
				"Emulex OneConnect %s, %s Initiator, Port %s",
				m.name, m.function,
				phba->Port);
		else
			snprintf(descp, 255,
				"Emulex %s %d%s %s %s",
				m.name, max_speed, (GE) ? "GE" : "Gb",
				m.bus, m.function);
	}
}

int
lpfc_post_buffer(struct lpfc_hba *phba, struct lpfc_sli_ring *pring, int cnt)
{
	IOCB_t *icmd;
	struct lpfc_iocbq *iocb;
	struct lpfc_dmabuf *mp1, *mp2;

	cnt += pring->missbufcnt;

	
	while (cnt > 0) {
		
		iocb = lpfc_sli_get_iocbq(phba);
		if (iocb == NULL) {
			pring->missbufcnt = cnt;
			return cnt;
		}
		icmd = &iocb->iocb;

		
		
		mp1 = kmalloc(sizeof (struct lpfc_dmabuf), GFP_KERNEL);
		if (mp1)
		    mp1->virt = lpfc_mbuf_alloc(phba, MEM_PRI, &mp1->phys);
		if (!mp1 || !mp1->virt) {
			kfree(mp1);
			lpfc_sli_release_iocbq(phba, iocb);
			pring->missbufcnt = cnt;
			return cnt;
		}

		INIT_LIST_HEAD(&mp1->list);
		
		if (cnt > 1) {
			mp2 = kmalloc(sizeof (struct lpfc_dmabuf), GFP_KERNEL);
			if (mp2)
				mp2->virt = lpfc_mbuf_alloc(phba, MEM_PRI,
							    &mp2->phys);
			if (!mp2 || !mp2->virt) {
				kfree(mp2);
				lpfc_mbuf_free(phba, mp1->virt, mp1->phys);
				kfree(mp1);
				lpfc_sli_release_iocbq(phba, iocb);
				pring->missbufcnt = cnt;
				return cnt;
			}

			INIT_LIST_HEAD(&mp2->list);
		} else {
			mp2 = NULL;
		}

		icmd->un.cont64[0].addrHigh = putPaddrHigh(mp1->phys);
		icmd->un.cont64[0].addrLow = putPaddrLow(mp1->phys);
		icmd->un.cont64[0].tus.f.bdeSize = FCELSSIZE;
		icmd->ulpBdeCount = 1;
		cnt--;
		if (mp2) {
			icmd->un.cont64[1].addrHigh = putPaddrHigh(mp2->phys);
			icmd->un.cont64[1].addrLow = putPaddrLow(mp2->phys);
			icmd->un.cont64[1].tus.f.bdeSize = FCELSSIZE;
			cnt--;
			icmd->ulpBdeCount = 2;
		}

		icmd->ulpCommand = CMD_QUE_RING_BUF64_CN;
		icmd->ulpLe = 1;

		if (lpfc_sli_issue_iocb(phba, pring->ringno, iocb, 0) ==
		    IOCB_ERROR) {
			lpfc_mbuf_free(phba, mp1->virt, mp1->phys);
			kfree(mp1);
			cnt++;
			if (mp2) {
				lpfc_mbuf_free(phba, mp2->virt, mp2->phys);
				kfree(mp2);
				cnt++;
			}
			lpfc_sli_release_iocbq(phba, iocb);
			pring->missbufcnt = cnt;
			return cnt;
		}
		lpfc_sli_ringpostbuf_put(phba, pring, mp1);
		if (mp2)
			lpfc_sli_ringpostbuf_put(phba, pring, mp2);
	}
	pring->missbufcnt = 0;
	return 0;
}

static int
lpfc_post_rcv_buf(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;

	
	lpfc_post_buffer(phba, &psli->ring[LPFC_ELS_RING], LPFC_BUF_RING0);
	

	return 0;
}

#define S(N,V) (((V)<<(N))|((V)>>(32-(N))))

static void
lpfc_sha_init(uint32_t * HashResultPointer)
{
	HashResultPointer[0] = 0x67452301;
	HashResultPointer[1] = 0xEFCDAB89;
	HashResultPointer[2] = 0x98BADCFE;
	HashResultPointer[3] = 0x10325476;
	HashResultPointer[4] = 0xC3D2E1F0;
}

static void
lpfc_sha_iterate(uint32_t * HashResultPointer, uint32_t * HashWorkingPointer)
{
	int t;
	uint32_t TEMP;
	uint32_t A, B, C, D, E;
	t = 16;
	do {
		HashWorkingPointer[t] =
		    S(1,
		      HashWorkingPointer[t - 3] ^ HashWorkingPointer[t -
								     8] ^
		      HashWorkingPointer[t - 14] ^ HashWorkingPointer[t - 16]);
	} while (++t <= 79);
	t = 0;
	A = HashResultPointer[0];
	B = HashResultPointer[1];
	C = HashResultPointer[2];
	D = HashResultPointer[3];
	E = HashResultPointer[4];

	do {
		if (t < 20) {
			TEMP = ((B & C) | ((~B) & D)) + 0x5A827999;
		} else if (t < 40) {
			TEMP = (B ^ C ^ D) + 0x6ED9EBA1;
		} else if (t < 60) {
			TEMP = ((B & C) | (B & D) | (C & D)) + 0x8F1BBCDC;
		} else {
			TEMP = (B ^ C ^ D) + 0xCA62C1D6;
		}
		TEMP += S(5, A) + E + HashWorkingPointer[t];
		E = D;
		D = C;
		C = S(30, B);
		B = A;
		A = TEMP;
	} while (++t <= 79);

	HashResultPointer[0] += A;
	HashResultPointer[1] += B;
	HashResultPointer[2] += C;
	HashResultPointer[3] += D;
	HashResultPointer[4] += E;

}

static void
lpfc_challenge_key(uint32_t * RandomChallenge, uint32_t * HashWorking)
{
	*HashWorking = (*RandomChallenge ^ *HashWorking);
}

void
lpfc_hba_init(struct lpfc_hba *phba, uint32_t *hbainit)
{
	int t;
	uint32_t *HashWorking;
	uint32_t *pwwnn = (uint32_t *) phba->wwnn;

	HashWorking = kcalloc(80, sizeof(uint32_t), GFP_KERNEL);
	if (!HashWorking)
		return;

	HashWorking[0] = HashWorking[78] = *pwwnn++;
	HashWorking[1] = HashWorking[79] = *pwwnn;

	for (t = 0; t < 7; t++)
		lpfc_challenge_key(phba->RandomData + t, HashWorking + t);

	lpfc_sha_init(hbainit);
	lpfc_sha_iterate(hbainit, HashWorking);
	kfree(HashWorking);
}

void
lpfc_cleanup(struct lpfc_vport *vport)
{
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_nodelist *ndlp, *next_ndlp;
	int i = 0;

	if (phba->link_state > LPFC_LINK_DOWN)
		lpfc_port_link_failure(vport);

	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp)) {
			ndlp = lpfc_enable_node(vport, ndlp,
						NLP_STE_UNUSED_NODE);
			if (!ndlp)
				continue;
			spin_lock_irq(&phba->ndlp_lock);
			NLP_SET_FREE_REQ(ndlp);
			spin_unlock_irq(&phba->ndlp_lock);
			
			lpfc_nlp_put(ndlp);
			continue;
		}
		spin_lock_irq(&phba->ndlp_lock);
		if (NLP_CHK_FREE_REQ(ndlp)) {
			
			spin_unlock_irq(&phba->ndlp_lock);
			continue;
		} else
			
			NLP_SET_FREE_REQ(ndlp);
		spin_unlock_irq(&phba->ndlp_lock);

		if (vport->port_type != LPFC_PHYSICAL_PORT &&
		    ndlp->nlp_DID == Fabric_DID) {
			
			lpfc_nlp_put(ndlp);
			continue;
		}

		if (ndlp->nlp_state == NLP_STE_UNUSED_NODE) {
			lpfc_nlp_put(ndlp);
			continue;
		}

		if (ndlp->nlp_type & NLP_FABRIC)
			lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RECOVERY);

		lpfc_disc_state_machine(vport, ndlp, NULL,
					     NLP_EVT_DEVICE_RM);
	}

	while (!list_empty(&vport->fc_nodes)) {
		if (i++ > 3000) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_DISCOVERY,
				"0233 Nodelist not empty\n");
			list_for_each_entry_safe(ndlp, next_ndlp,
						&vport->fc_nodes, nlp_listp) {
				lpfc_printf_vlog(ndlp->vport, KERN_ERR,
						LOG_NODE,
						"0282 did:x%x ndlp:x%p "
						"usgmap:x%x refcnt:%d\n",
						ndlp->nlp_DID, (void *)ndlp,
						ndlp->nlp_usg_map,
						atomic_read(
							&ndlp->kref.refcount));
			}
			break;
		}

		
		msleep(10);
	}
	lpfc_cleanup_vports_rrqs(vport, NULL);
}

void
lpfc_stop_vport_timers(struct lpfc_vport *vport)
{
	del_timer_sync(&vport->els_tmofunc);
	del_timer_sync(&vport->fc_fdmitmo);
	del_timer_sync(&vport->delayed_disc_tmo);
	lpfc_can_disctmo(vport);
	return;
}

void
__lpfc_sli4_stop_fcf_redisc_wait_timer(struct lpfc_hba *phba)
{
	
	phba->fcf.fcf_flag &= ~FCF_REDISC_PEND;

	
	del_timer(&phba->fcf.redisc_wait);
}

void
lpfc_sli4_stop_fcf_redisc_wait_timer(struct lpfc_hba *phba)
{
	spin_lock_irq(&phba->hbalock);
	if (!(phba->fcf.fcf_flag & FCF_REDISC_PEND)) {
		
		spin_unlock_irq(&phba->hbalock);
		return;
	}
	__lpfc_sli4_stop_fcf_redisc_wait_timer(phba);
	
	phba->fcf.fcf_flag &= ~(FCF_DEAD_DISC | FCF_ACVL_DISC);
	spin_unlock_irq(&phba->hbalock);
}

void
lpfc_stop_hba_timers(struct lpfc_hba *phba)
{
	lpfc_stop_vport_timers(phba->pport);
	del_timer_sync(&phba->sli.mbox_tmo);
	del_timer_sync(&phba->fabric_block_timer);
	del_timer_sync(&phba->eratt_poll);
	del_timer_sync(&phba->hb_tmofunc);
	if (phba->sli_rev == LPFC_SLI_REV4) {
		del_timer_sync(&phba->rrq_tmr);
		phba->hba_flag &= ~HBA_RRQ_ACTIVE;
	}
	phba->hb_outstanding = 0;

	switch (phba->pci_dev_grp) {
	case LPFC_PCI_DEV_LP:
		
		del_timer_sync(&phba->fcp_poll_timer);
		break;
	case LPFC_PCI_DEV_OC:
		
		lpfc_sli4_stop_fcf_redisc_wait_timer(phba);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0297 Invalid device group (x%x)\n",
				phba->pci_dev_grp);
		break;
	}
	return;
}

static void
lpfc_block_mgmt_io(struct lpfc_hba * phba)
{
	unsigned long iflag;
	uint8_t actcmd = MBX_HEARTBEAT;
	unsigned long timeout;

	timeout = msecs_to_jiffies(LPFC_MBOX_TMO * 1000) + jiffies;
	spin_lock_irqsave(&phba->hbalock, iflag);
	phba->sli.sli_flag |= LPFC_BLOCK_MGMT_IO;
	if (phba->sli.mbox_active) {
		actcmd = phba->sli.mbox_active->u.mb.mbxCommand;
		timeout = msecs_to_jiffies(lpfc_mbox_tmo_val(phba,
				phba->sli.mbox_active) * 1000) + jiffies;
	}
	spin_unlock_irqrestore(&phba->hbalock, iflag);

	
	while (phba->sli.mbox_active) {
		
		msleep(2);
		if (time_after(jiffies, timeout)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2813 Mgmt IO is Blocked %x "
				"- mbox cmd %x still active\n",
				phba->sli.sli_flag, actcmd);
			break;
		}
	}
}

void
lpfc_sli4_node_prep(struct lpfc_hba *phba)
{
	struct lpfc_nodelist  *ndlp, *next_ndlp;
	struct lpfc_vport **vports;
	int i;

	if (phba->sli_rev != LPFC_SLI_REV4)
		return;

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL) {
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			if (vports[i]->load_flag & FC_UNLOADING)
				continue;

			list_for_each_entry_safe(ndlp, next_ndlp,
						 &vports[i]->fc_nodes,
						 nlp_listp) {
				if (NLP_CHK_NODE_ACT(ndlp))
					ndlp->nlp_rpi =
						lpfc_sli4_alloc_rpi(phba);
			}
		}
	}
	lpfc_destroy_vport_work_array(phba, vports);
}

int
lpfc_online(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport;
	struct lpfc_vport **vports;
	int i;

	if (!phba)
		return 0;
	vport = phba->pport;

	if (!(vport->fc_flag & FC_OFFLINE_MODE))
		return 0;

	lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
			"0458 Bring Adapter online\n");

	lpfc_block_mgmt_io(phba);

	if (!lpfc_sli_queue_setup(phba)) {
		lpfc_unblock_mgmt_io(phba);
		return 1;
	}

	if (phba->sli_rev == LPFC_SLI_REV4) {
		if (lpfc_sli4_hba_setup(phba)) { 
			lpfc_unblock_mgmt_io(phba);
			return 1;
		}
	} else {
		if (lpfc_sli_hba_setup(phba)) {	
			lpfc_unblock_mgmt_io(phba);
			return 1;
		}
	}

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			struct Scsi_Host *shost;
			shost = lpfc_shost_from_vport(vports[i]);
			spin_lock_irq(shost->host_lock);
			vports[i]->fc_flag &= ~FC_OFFLINE_MODE;
			if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED)
				vports[i]->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
			if (phba->sli_rev == LPFC_SLI_REV4)
				vports[i]->fc_flag |= FC_VPORT_NEEDS_INIT_VPI;
			spin_unlock_irq(shost->host_lock);
		}
		lpfc_destroy_vport_work_array(phba, vports);

	lpfc_unblock_mgmt_io(phba);
	return 0;
}

void
lpfc_unblock_mgmt_io(struct lpfc_hba * phba)
{
	unsigned long iflag;

	spin_lock_irqsave(&phba->hbalock, iflag);
	phba->sli.sli_flag &= ~LPFC_BLOCK_MGMT_IO;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
}

void
lpfc_offline_prep(struct lpfc_hba * phba)
{
	struct lpfc_vport *vport = phba->pport;
	struct lpfc_nodelist  *ndlp, *next_ndlp;
	struct lpfc_vport **vports;
	struct Scsi_Host *shost;
	int i;

	if (vport->fc_flag & FC_OFFLINE_MODE)
		return;

	lpfc_block_mgmt_io(phba);

	lpfc_linkdown(phba);

	
	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL) {
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			if (vports[i]->load_flag & FC_UNLOADING)
				continue;
			shost = lpfc_shost_from_vport(vports[i]);
			spin_lock_irq(shost->host_lock);
			vports[i]->vpi_state &= ~LPFC_VPI_REGISTERED;
			vports[i]->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
			vports[i]->fc_flag &= ~FC_VFI_REGISTERED;
			spin_unlock_irq(shost->host_lock);

			shost =	lpfc_shost_from_vport(vports[i]);
			list_for_each_entry_safe(ndlp, next_ndlp,
						 &vports[i]->fc_nodes,
						 nlp_listp) {
				if (!NLP_CHK_NODE_ACT(ndlp))
					continue;
				if (ndlp->nlp_state == NLP_STE_UNUSED_NODE)
					continue;
				if (ndlp->nlp_type & NLP_FABRIC) {
					lpfc_disc_state_machine(vports[i], ndlp,
						NULL, NLP_EVT_DEVICE_RECOVERY);
					lpfc_disc_state_machine(vports[i], ndlp,
						NULL, NLP_EVT_DEVICE_RM);
				}
				spin_lock_irq(shost->host_lock);
				ndlp->nlp_flag &= ~NLP_NPR_ADISC;
				spin_unlock_irq(shost->host_lock);
				if (phba->sli_rev == LPFC_SLI_REV4)
					lpfc_sli4_free_rpi(phba, ndlp->nlp_rpi);
				lpfc_unreg_rpi(vports[i], ndlp);
			}
		}
	}
	lpfc_destroy_vport_work_array(phba, vports);

	lpfc_sli_mbox_sys_shutdown(phba);
}

void
lpfc_offline(struct lpfc_hba *phba)
{
	struct Scsi_Host  *shost;
	struct lpfc_vport **vports;
	int i;

	if (phba->pport->fc_flag & FC_OFFLINE_MODE)
		return;

	
	lpfc_stop_port(phba);
	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++)
			lpfc_stop_vport_timers(vports[i]);
	lpfc_destroy_vport_work_array(phba, vports);
	lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
			"0460 Bring Adapter offline\n");
	lpfc_sli_hba_down(phba);
	spin_lock_irq(&phba->hbalock);
	phba->work_ha = 0;
	spin_unlock_irq(&phba->hbalock);
	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			shost = lpfc_shost_from_vport(vports[i]);
			spin_lock_irq(shost->host_lock);
			vports[i]->work_port_events = 0;
			vports[i]->fc_flag |= FC_OFFLINE_MODE;
			spin_unlock_irq(shost->host_lock);
		}
	lpfc_destroy_vport_work_array(phba, vports);
}

int
lpfc_scsi_buf_update(struct lpfc_hba *phba)
{
	struct lpfc_scsi_buf *sb, *sb_next;

	spin_lock_irq(&phba->hbalock);
	spin_lock(&phba->scsi_buf_list_lock);
	list_for_each_entry_safe(sb, sb_next, &phba->lpfc_scsi_buf_list, list) {
		sb->cur_iocbq.sli4_xritag =
			phba->sli4_hba.xri_ids[sb->cur_iocbq.sli4_lxritag];
		set_bit(sb->cur_iocbq.sli4_lxritag, phba->sli4_hba.xri_bmask);
		phba->sli4_hba.max_cfg_param.xri_used++;
		phba->sli4_hba.xri_count++;
	}
	spin_unlock(&phba->scsi_buf_list_lock);
	spin_unlock_irq(&phba->hbalock);
	return 0;
}

static int
lpfc_scsi_free(struct lpfc_hba *phba)
{
	struct lpfc_scsi_buf *sb, *sb_next;
	struct lpfc_iocbq *io, *io_next;

	spin_lock_irq(&phba->hbalock);
	
	spin_lock(&phba->scsi_buf_list_lock);
	list_for_each_entry_safe(sb, sb_next, &phba->lpfc_scsi_buf_list, list) {
		list_del(&sb->list);
		pci_pool_free(phba->lpfc_scsi_dma_buf_pool, sb->data,
			      sb->dma_handle);
		kfree(sb);
		phba->total_scsi_bufs--;
	}
	spin_unlock(&phba->scsi_buf_list_lock);

	
	list_for_each_entry_safe(io, io_next, &phba->lpfc_iocb_list, list) {
		list_del(&io->list);
		kfree(io);
		phba->total_iocbq_bufs--;
	}

	spin_unlock_irq(&phba->hbalock);
	return 0;
}

struct lpfc_vport *
lpfc_create_port(struct lpfc_hba *phba, int instance, struct device *dev)
{
	struct lpfc_vport *vport;
	struct Scsi_Host  *shost;
	int error = 0;

	if (dev != &phba->pcidev->dev)
		shost = scsi_host_alloc(&lpfc_vport_template,
					sizeof(struct lpfc_vport));
	else
		shost = scsi_host_alloc(&lpfc_template,
					sizeof(struct lpfc_vport));
	if (!shost)
		goto out;

	vport = (struct lpfc_vport *) shost->hostdata;
	vport->phba = phba;
	vport->load_flag |= FC_LOADING;
	vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
	vport->fc_rscn_flush = 0;

	lpfc_get_vport_cfgparam(vport);
	shost->unique_id = instance;
	shost->max_id = LPFC_MAX_TARGET;
	shost->max_lun = vport->cfg_max_luns;
	shost->this_id = -1;
	shost->max_cmd_len = 16;
	if (phba->sli_rev == LPFC_SLI_REV4) {
		shost->dma_boundary =
			phba->sli4_hba.pc_sli4_params.sge_supp_len-1;
		shost->sg_tablesize = phba->cfg_sg_seg_cnt;
	}

	shost->can_queue = phba->cfg_hba_queue_depth - 10;
	if (dev != &phba->pcidev->dev) {
		shost->transportt = lpfc_vport_transport_template;
		vport->port_type = LPFC_NPIV_PORT;
	} else {
		shost->transportt = lpfc_transport_template;
		vport->port_type = LPFC_PHYSICAL_PORT;
	}

	
	INIT_LIST_HEAD(&vport->fc_nodes);
	INIT_LIST_HEAD(&vport->rcv_buffer_list);
	spin_lock_init(&vport->work_port_lock);

	init_timer(&vport->fc_disctmo);
	vport->fc_disctmo.function = lpfc_disc_timeout;
	vport->fc_disctmo.data = (unsigned long)vport;

	init_timer(&vport->fc_fdmitmo);
	vport->fc_fdmitmo.function = lpfc_fdmi_tmo;
	vport->fc_fdmitmo.data = (unsigned long)vport;

	init_timer(&vport->els_tmofunc);
	vport->els_tmofunc.function = lpfc_els_timeout;
	vport->els_tmofunc.data = (unsigned long)vport;

	init_timer(&vport->delayed_disc_tmo);
	vport->delayed_disc_tmo.function = lpfc_delayed_disc_tmo;
	vport->delayed_disc_tmo.data = (unsigned long)vport;

	error = scsi_add_host_with_dma(shost, dev, &phba->pcidev->dev);
	if (error)
		goto out_put_shost;

	spin_lock_irq(&phba->hbalock);
	list_add_tail(&vport->listentry, &phba->port_list);
	spin_unlock_irq(&phba->hbalock);
	return vport;

out_put_shost:
	scsi_host_put(shost);
out:
	return NULL;
}

void
destroy_port(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;

	lpfc_debugfs_terminate(vport);
	fc_remove_host(shost);
	scsi_remove_host(shost);

	spin_lock_irq(&phba->hbalock);
	list_del_init(&vport->listentry);
	spin_unlock_irq(&phba->hbalock);

	lpfc_cleanup(vport);
	return;
}

int
lpfc_get_instance(void)
{
	int instance = 0;

	
	if (!idr_pre_get(&lpfc_hba_index, GFP_KERNEL))
		return -1;
	if (idr_get_new(&lpfc_hba_index, NULL, &instance))
		return -1;
	return instance;
}

int lpfc_scan_finished(struct Scsi_Host *shost, unsigned long time)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	int stat = 0;

	spin_lock_irq(shost->host_lock);

	if (vport->load_flag & FC_UNLOADING) {
		stat = 1;
		goto finished;
	}
	if (time >= 30 * HZ) {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0461 Scanning longer than 30 "
				"seconds.  Continuing initialization\n");
		stat = 1;
		goto finished;
	}
	if (time >= 15 * HZ && phba->link_state <= LPFC_LINK_DOWN) {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0465 Link down longer than 15 "
				"seconds.  Continuing initialization\n");
		stat = 1;
		goto finished;
	}

	if (vport->port_state != LPFC_VPORT_READY)
		goto finished;
	if (vport->num_disc_nodes || vport->fc_prli_sent)
		goto finished;
	if (vport->fc_map_cnt == 0 && time < 2 * HZ)
		goto finished;
	if ((phba->sli.sli_flag & LPFC_SLI_MBOX_ACTIVE) != 0)
		goto finished;

	stat = 1;

finished:
	spin_unlock_irq(shost->host_lock);
	return stat;
}

void lpfc_host_attrib_init(struct Scsi_Host *shost)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	fc_host_node_name(shost) = wwn_to_u64(vport->fc_nodename.u.wwn);
	fc_host_port_name(shost) = wwn_to_u64(vport->fc_portname.u.wwn);
	fc_host_supported_classes(shost) = FC_COS_CLASS3;

	memset(fc_host_supported_fc4s(shost), 0,
	       sizeof(fc_host_supported_fc4s(shost)));
	fc_host_supported_fc4s(shost)[2] = 1;
	fc_host_supported_fc4s(shost)[7] = 1;

	lpfc_vport_symbolic_node_name(vport, fc_host_symbolic_name(shost),
				 sizeof fc_host_symbolic_name(shost));

	fc_host_supported_speeds(shost) = 0;
	if (phba->lmt & LMT_16Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_16GBIT;
	if (phba->lmt & LMT_10Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_10GBIT;
	if (phba->lmt & LMT_8Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_8GBIT;
	if (phba->lmt & LMT_4Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_4GBIT;
	if (phba->lmt & LMT_2Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_2GBIT;
	if (phba->lmt & LMT_1Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_1GBIT;

	fc_host_maxframe_size(shost) =
		(((uint32_t) vport->fc_sparam.cmn.bbRcvSizeMsb & 0x0F) << 8) |
		(uint32_t) vport->fc_sparam.cmn.bbRcvSizeLsb;

	fc_host_dev_loss_tmo(shost) = vport->cfg_devloss_tmo;

	
	memset(fc_host_active_fc4s(shost), 0,
	       sizeof(fc_host_active_fc4s(shost)));
	fc_host_active_fc4s(shost)[2] = 1;
	fc_host_active_fc4s(shost)[7] = 1;

	fc_host_max_npiv_vports(shost) = phba->max_vpi;
	spin_lock_irq(shost->host_lock);
	vport->load_flag &= ~FC_LOADING;
	spin_unlock_irq(shost->host_lock);
}

static void
lpfc_stop_port_s3(struct lpfc_hba *phba)
{
	
	writel(0, phba->HCregaddr);
	readl(phba->HCregaddr); 
	
	writel(0xffffffff, phba->HAregaddr);
	readl(phba->HAregaddr); 

	
	lpfc_stop_hba_timers(phba);
	phba->pport->work_port_events = 0;
}

static void
lpfc_stop_port_s4(struct lpfc_hba *phba)
{
	
	lpfc_stop_hba_timers(phba);
	phba->pport->work_port_events = 0;
	phba->sli4_hba.intr_enable = 0;
}

void
lpfc_stop_port(struct lpfc_hba *phba)
{
	phba->lpfc_stop_port(phba);
}

void
lpfc_fcf_redisc_wait_start_timer(struct lpfc_hba *phba)
{
	unsigned long fcf_redisc_wait_tmo =
		(jiffies + msecs_to_jiffies(LPFC_FCF_REDISCOVER_WAIT_TMO));
	
	mod_timer(&phba->fcf.redisc_wait, fcf_redisc_wait_tmo);
	spin_lock_irq(&phba->hbalock);
	
	phba->fcf.fcf_flag &= ~(FCF_AVAILABLE | FCF_SCAN_DONE);
	
	phba->fcf.fcf_flag |= FCF_REDISC_PEND;
	spin_unlock_irq(&phba->hbalock);
}

void
lpfc_sli4_fcf_redisc_wait_tmo(unsigned long ptr)
{
	struct lpfc_hba *phba = (struct lpfc_hba *)ptr;

	
	spin_lock_irq(&phba->hbalock);
	if (!(phba->fcf.fcf_flag & FCF_REDISC_PEND)) {
		spin_unlock_irq(&phba->hbalock);
		return;
	}
	
	phba->fcf.fcf_flag &= ~FCF_REDISC_PEND;
	
	phba->fcf.fcf_flag |= FCF_REDISC_EVT;
	spin_unlock_irq(&phba->hbalock);
	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
			"2776 FCF rediscover quiescent timer expired\n");
	
	lpfc_worker_wake_up(phba);
}

static uint16_t
lpfc_sli4_parse_latt_fault(struct lpfc_hba *phba,
			   struct lpfc_acqe_link *acqe_link)
{
	uint16_t latt_fault;

	switch (bf_get(lpfc_acqe_link_fault, acqe_link)) {
	case LPFC_ASYNC_LINK_FAULT_NONE:
	case LPFC_ASYNC_LINK_FAULT_LOCAL:
	case LPFC_ASYNC_LINK_FAULT_REMOTE:
		latt_fault = 0;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0398 Invalid link fault code: x%x\n",
				bf_get(lpfc_acqe_link_fault, acqe_link));
		latt_fault = MBXERR_ERROR;
		break;
	}
	return latt_fault;
}

static uint8_t
lpfc_sli4_parse_latt_type(struct lpfc_hba *phba,
			  struct lpfc_acqe_link *acqe_link)
{
	uint8_t att_type;

	switch (bf_get(lpfc_acqe_link_status, acqe_link)) {
	case LPFC_ASYNC_LINK_STATUS_DOWN:
	case LPFC_ASYNC_LINK_STATUS_LOGICAL_DOWN:
		att_type = LPFC_ATT_LINK_DOWN;
		break;
	case LPFC_ASYNC_LINK_STATUS_UP:
		
		att_type = LPFC_ATT_RESERVED;
		break;
	case LPFC_ASYNC_LINK_STATUS_LOGICAL_UP:
		att_type = LPFC_ATT_LINK_UP;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0399 Invalid link attention type: x%x\n",
				bf_get(lpfc_acqe_link_status, acqe_link));
		att_type = LPFC_ATT_RESERVED;
		break;
	}
	return att_type;
}

static uint8_t
lpfc_sli4_parse_latt_link_speed(struct lpfc_hba *phba,
				struct lpfc_acqe_link *acqe_link)
{
	uint8_t link_speed;

	switch (bf_get(lpfc_acqe_link_speed, acqe_link)) {
	case LPFC_ASYNC_LINK_SPEED_ZERO:
	case LPFC_ASYNC_LINK_SPEED_10MBPS:
	case LPFC_ASYNC_LINK_SPEED_100MBPS:
		link_speed = LPFC_LINK_SPEED_UNKNOWN;
		break;
	case LPFC_ASYNC_LINK_SPEED_1GBPS:
		link_speed = LPFC_LINK_SPEED_1GHZ;
		break;
	case LPFC_ASYNC_LINK_SPEED_10GBPS:
		link_speed = LPFC_LINK_SPEED_10GHZ;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0483 Invalid link-attention link speed: x%x\n",
				bf_get(lpfc_acqe_link_speed, acqe_link));
		link_speed = LPFC_LINK_SPEED_UNKNOWN;
		break;
	}
	return link_speed;
}

static void
lpfc_sli4_async_link_evt(struct lpfc_hba *phba,
			 struct lpfc_acqe_link *acqe_link)
{
	struct lpfc_dmabuf *mp;
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *mb;
	struct lpfc_mbx_read_top *la;
	uint8_t att_type;
	int rc;

	att_type = lpfc_sli4_parse_latt_type(phba, acqe_link);
	if (att_type != LPFC_ATT_LINK_DOWN && att_type != LPFC_ATT_LINK_UP)
		return;
	phba->fcoe_eventtag = acqe_link->event_tag;
	pmb = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0395 The mboxq allocation failed\n");
		return;
	}
	mp = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (!mp) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0396 The lpfc_dmabuf allocation failed\n");
		goto out_free_pmb;
	}
	mp->virt = lpfc_mbuf_alloc(phba, 0, &mp->phys);
	if (!mp->virt) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0397 The mbuf allocation failed\n");
		goto out_free_dmabuf;
	}

	
	lpfc_els_flush_all_cmd(phba);

	
	phba->sli.ring[LPFC_ELS_RING].flag |= LPFC_STOP_IOCB_EVENT;

	
	phba->sli.slistat.link_event++;

	
	lpfc_read_topology(phba, pmb, mp);
	pmb->mbox_cmpl = lpfc_mbx_cmpl_read_topology;
	pmb->vport = phba->pport;

	
	phba->sli4_hba.link_state.speed =
				bf_get(lpfc_acqe_link_speed, acqe_link);
	phba->sli4_hba.link_state.duplex =
				bf_get(lpfc_acqe_link_duplex, acqe_link);
	phba->sli4_hba.link_state.status =
				bf_get(lpfc_acqe_link_status, acqe_link);
	phba->sli4_hba.link_state.type =
				bf_get(lpfc_acqe_link_type, acqe_link);
	phba->sli4_hba.link_state.number =
				bf_get(lpfc_acqe_link_number, acqe_link);
	phba->sli4_hba.link_state.fault =
				bf_get(lpfc_acqe_link_fault, acqe_link);
	phba->sli4_hba.link_state.logical_speed =
			bf_get(lpfc_acqe_logical_link_speed, acqe_link);
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"2900 Async FC/FCoE Link event - Speed:%dGBit "
			"duplex:x%x LA Type:x%x Port Type:%d Port Number:%d "
			"Logical speed:%dMbps Fault:%d\n",
			phba->sli4_hba.link_state.speed,
			phba->sli4_hba.link_state.topology,
			phba->sli4_hba.link_state.status,
			phba->sli4_hba.link_state.type,
			phba->sli4_hba.link_state.number,
			phba->sli4_hba.link_state.logical_speed * 10,
			phba->sli4_hba.link_state.fault);
	if (!(phba->hba_flag & HBA_FCOE_MODE)) {
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);
		if (rc == MBX_NOT_FINISHED)
			goto out_free_dmabuf;
		return;
	}
	
	mb = &pmb->u.mb;
	mb->mbxStatus = lpfc_sli4_parse_latt_fault(phba, acqe_link);

	
	la = (struct lpfc_mbx_read_top *) &pmb->u.mb.un.varReadTop;
	la->eventTag = acqe_link->event_tag;
	bf_set(lpfc_mbx_read_top_att_type, la, att_type);
	bf_set(lpfc_mbx_read_top_link_spd, la,
	       lpfc_sli4_parse_latt_link_speed(phba, acqe_link));

	
	bf_set(lpfc_mbx_read_top_topology, la, LPFC_TOPOLOGY_PT_PT);
	bf_set(lpfc_mbx_read_top_alpa_granted, la, 0);
	bf_set(lpfc_mbx_read_top_il, la, 0);
	bf_set(lpfc_mbx_read_top_pb, la, 0);
	bf_set(lpfc_mbx_read_top_fa, la, 0);
	bf_set(lpfc_mbx_read_top_mm, la, 0);

	
	lpfc_mbx_cmpl_read_topology(phba, pmb);

	return;

out_free_dmabuf:
	kfree(mp);
out_free_pmb:
	mempool_free(pmb, phba->mbox_mem_pool);
}

static void
lpfc_sli4_async_fc_evt(struct lpfc_hba *phba, struct lpfc_acqe_fc_la *acqe_fc)
{
	struct lpfc_dmabuf *mp;
	LPFC_MBOXQ_t *pmb;
	int rc;

	if (bf_get(lpfc_trailer_type, acqe_fc) !=
	    LPFC_FC_LA_EVENT_TYPE_FC_LINK) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2895 Non FC link Event detected.(%d)\n",
				bf_get(lpfc_trailer_type, acqe_fc));
		return;
	}
	
	phba->sli4_hba.link_state.speed =
				bf_get(lpfc_acqe_fc_la_speed, acqe_fc);
	phba->sli4_hba.link_state.duplex = LPFC_ASYNC_LINK_DUPLEX_FULL;
	phba->sli4_hba.link_state.topology =
				bf_get(lpfc_acqe_fc_la_topology, acqe_fc);
	phba->sli4_hba.link_state.status =
				bf_get(lpfc_acqe_fc_la_att_type, acqe_fc);
	phba->sli4_hba.link_state.type =
				bf_get(lpfc_acqe_fc_la_port_type, acqe_fc);
	phba->sli4_hba.link_state.number =
				bf_get(lpfc_acqe_fc_la_port_number, acqe_fc);
	phba->sli4_hba.link_state.fault =
				bf_get(lpfc_acqe_link_fault, acqe_fc);
	phba->sli4_hba.link_state.logical_speed =
				bf_get(lpfc_acqe_fc_la_llink_spd, acqe_fc);
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"2896 Async FC event - Speed:%dGBaud Topology:x%x "
			"LA Type:x%x Port Type:%d Port Number:%d Logical speed:"
			"%dMbps Fault:%d\n",
			phba->sli4_hba.link_state.speed,
			phba->sli4_hba.link_state.topology,
			phba->sli4_hba.link_state.status,
			phba->sli4_hba.link_state.type,
			phba->sli4_hba.link_state.number,
			phba->sli4_hba.link_state.logical_speed * 10,
			phba->sli4_hba.link_state.fault);
	pmb = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2897 The mboxq allocation failed\n");
		return;
	}
	mp = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (!mp) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2898 The lpfc_dmabuf allocation failed\n");
		goto out_free_pmb;
	}
	mp->virt = lpfc_mbuf_alloc(phba, 0, &mp->phys);
	if (!mp->virt) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2899 The mbuf allocation failed\n");
		goto out_free_dmabuf;
	}

	
	lpfc_els_flush_all_cmd(phba);

	
	phba->sli.ring[LPFC_ELS_RING].flag |= LPFC_STOP_IOCB_EVENT;

	
	phba->sli.slistat.link_event++;

	
	lpfc_read_topology(phba, pmb, mp);
	pmb->mbox_cmpl = lpfc_mbx_cmpl_read_topology;
	pmb->vport = phba->pport;

	rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED)
		goto out_free_dmabuf;
	return;

out_free_dmabuf:
	kfree(mp);
out_free_pmb:
	mempool_free(pmb, phba->mbox_mem_pool);
}

static void
lpfc_sli4_async_sli_evt(struct lpfc_hba *phba, struct lpfc_acqe_sli *acqe_sli)
{
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"2901 Async SLI event - Event Data1:x%08x Event Data2:"
			"x%08x SLI Event Type:%d",
			acqe_sli->event_data1, acqe_sli->event_data2,
			bf_get(lpfc_trailer_type, acqe_sli));
	return;
}

static struct lpfc_nodelist *
lpfc_sli4_perform_vport_cvl(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp;
	struct Scsi_Host *shost;
	struct lpfc_hba *phba;

	if (!vport)
		return NULL;
	phba = vport->phba;
	if (!phba)
		return NULL;
	ndlp = lpfc_findnode_did(vport, Fabric_DID);
	if (!ndlp) {
		
		ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
		if (!ndlp)
			return 0;
		lpfc_nlp_init(vport, ndlp, Fabric_DID);
		
		ndlp->nlp_type |= NLP_FABRIC;
		
		lpfc_enqueue_node(vport, ndlp);
	} else if (!NLP_CHK_NODE_ACT(ndlp)) {
		
		ndlp = lpfc_enable_node(vport, ndlp, NLP_STE_UNUSED_NODE);
		if (!ndlp)
			return 0;
	}
	if ((phba->pport->port_state < LPFC_FLOGI) &&
		(phba->pport->port_state != LPFC_VPORT_FAILED))
		return NULL;
	
	if ((vport != phba->pport) && (vport->port_state < LPFC_FDISC)
		&& (vport->port_state != LPFC_VPORT_FAILED))
		return NULL;
	shost = lpfc_shost_from_vport(vport);
	if (!shost)
		return NULL;
	lpfc_linkdown_port(vport);
	lpfc_cleanup_pending_mbox(vport);
	spin_lock_irq(shost->host_lock);
	vport->fc_flag |= FC_VPORT_CVL_RCVD;
	spin_unlock_irq(shost->host_lock);

	return ndlp;
}

static void
lpfc_sli4_perform_all_vport_cvl(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	int i;

	vports = lpfc_create_vport_work_array(phba);
	if (vports)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++)
			lpfc_sli4_perform_vport_cvl(vports[i]);
	lpfc_destroy_vport_work_array(phba, vports);
}

static void
lpfc_sli4_async_fip_evt(struct lpfc_hba *phba,
			struct lpfc_acqe_fip *acqe_fip)
{
	uint8_t event_type = bf_get(lpfc_trailer_type, acqe_fip);
	int rc;
	struct lpfc_vport *vport;
	struct lpfc_nodelist *ndlp;
	struct Scsi_Host  *shost;
	int active_vlink_present;
	struct lpfc_vport **vports;
	int i;

	phba->fc_eventTag = acqe_fip->event_tag;
	phba->fcoe_eventtag = acqe_fip->event_tag;
	switch (event_type) {
	case LPFC_FIP_EVENT_TYPE_NEW_FCF:
	case LPFC_FIP_EVENT_TYPE_FCF_PARAM_MOD:
		if (event_type == LPFC_FIP_EVENT_TYPE_NEW_FCF)
			lpfc_printf_log(phba, KERN_ERR, LOG_FIP |
					LOG_DISCOVERY,
					"2546 New FCF event, evt_tag:x%x, "
					"index:x%x\n",
					acqe_fip->event_tag,
					acqe_fip->index);
		else
			lpfc_printf_log(phba, KERN_WARNING, LOG_FIP |
					LOG_DISCOVERY,
					"2788 FCF param modified event, "
					"evt_tag:x%x, index:x%x\n",
					acqe_fip->event_tag,
					acqe_fip->index);
		if (phba->fcf.fcf_flag & FCF_DISCOVERY) {
			lpfc_printf_log(phba, KERN_INFO, LOG_FIP |
					LOG_DISCOVERY,
					"2779 Read FCF (x%x) for updating "
					"roundrobin FCF failover bmask\n",
					acqe_fip->index);
			rc = lpfc_sli4_read_fcf_rec(phba, acqe_fip->index);
		}

		
		spin_lock_irq(&phba->hbalock);
		if (phba->hba_flag & FCF_TS_INPROG) {
			spin_unlock_irq(&phba->hbalock);
			break;
		}
		
		if (phba->fcf.fcf_flag & FCF_REDISC_EVT) {
			spin_unlock_irq(&phba->hbalock);
			break;
		}

		
		if (phba->fcf.fcf_flag & FCF_SCAN_DONE) {
			spin_unlock_irq(&phba->hbalock);
			break;
		}
		spin_unlock_irq(&phba->hbalock);

		
		lpfc_printf_log(phba, KERN_INFO, LOG_FIP | LOG_DISCOVERY,
				"2770 Start FCF table scan per async FCF "
				"event, evt_tag:x%x, index:x%x\n",
				acqe_fip->event_tag, acqe_fip->index);
		rc = lpfc_sli4_fcf_scan_read_fcf_rec(phba,
						     LPFC_FCOE_FCF_GET_FIRST);
		if (rc)
			lpfc_printf_log(phba, KERN_ERR, LOG_FIP | LOG_DISCOVERY,
					"2547 Issue FCF scan read FCF mailbox "
					"command failed (x%x)\n", rc);
		break;

	case LPFC_FIP_EVENT_TYPE_FCF_TABLE_FULL:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"2548 FCF Table full count 0x%x tag 0x%x\n",
			bf_get(lpfc_acqe_fip_fcf_count, acqe_fip),
			acqe_fip->event_tag);
		break;

	case LPFC_FIP_EVENT_TYPE_FCF_DEAD:
		phba->fcoe_cvl_eventtag = acqe_fip->event_tag;
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP | LOG_DISCOVERY,
			"2549 FCF (x%x) disconnected from network, "
			"tag:x%x\n", acqe_fip->index, acqe_fip->event_tag);
		spin_lock_irq(&phba->hbalock);
		if (phba->fcf.fcf_flag & FCF_DISCOVERY) {
			spin_unlock_irq(&phba->hbalock);
			
			lpfc_sli4_fcf_rr_index_clear(phba, acqe_fip->index);
			break;
		}
		spin_unlock_irq(&phba->hbalock);

		
		if (phba->fcf.current_rec.fcf_indx != acqe_fip->index)
			break;

		spin_lock_irq(&phba->hbalock);
		
		phba->fcf.fcf_flag |= FCF_DEAD_DISC;
		spin_unlock_irq(&phba->hbalock);

		lpfc_printf_log(phba, KERN_INFO, LOG_FIP | LOG_DISCOVERY,
				"2771 Start FCF fast failover process due to "
				"FCF DEAD event: evt_tag:x%x, fcf_index:x%x "
				"\n", acqe_fip->event_tag, acqe_fip->index);
		rc = lpfc_sli4_redisc_fcf_table(phba);
		if (rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_FIP |
					LOG_DISCOVERY,
					"2772 Issue FCF rediscover mabilbox "
					"command failed, fail through to FCF "
					"dead event\n");
			spin_lock_irq(&phba->hbalock);
			phba->fcf.fcf_flag &= ~FCF_DEAD_DISC;
			spin_unlock_irq(&phba->hbalock);
			lpfc_sli4_fcf_dead_failthrough(phba);
		} else {
			
			lpfc_sli4_clear_fcf_rr_bmask(phba);
			lpfc_sli4_perform_all_vport_cvl(phba);
		}
		break;
	case LPFC_FIP_EVENT_TYPE_CVL:
		phba->fcoe_cvl_eventtag = acqe_fip->event_tag;
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP | LOG_DISCOVERY,
			"2718 Clear Virtual Link Received for VPI 0x%x"
			" tag 0x%x\n", acqe_fip->index, acqe_fip->event_tag);

		vport = lpfc_find_vport_by_vpid(phba,
						acqe_fip->index);
		ndlp = lpfc_sli4_perform_vport_cvl(vport);
		if (!ndlp)
			break;
		active_vlink_present = 0;

		vports = lpfc_create_vport_work_array(phba);
		if (vports) {
			for (i = 0; i <= phba->max_vports && vports[i] != NULL;
					i++) {
				if ((!(vports[i]->fc_flag &
					FC_VPORT_CVL_RCVD)) &&
					(vports[i]->port_state > LPFC_FDISC)) {
					active_vlink_present = 1;
					break;
				}
			}
			lpfc_destroy_vport_work_array(phba, vports);
		}

		if (active_vlink_present) {
			mod_timer(&ndlp->nlp_delayfunc, jiffies + HZ);
			shost = lpfc_shost_from_vport(vport);
			spin_lock_irq(shost->host_lock);
			ndlp->nlp_flag |= NLP_DELAY_TMO;
			spin_unlock_irq(shost->host_lock);
			ndlp->nlp_last_elscmd = ELS_CMD_FDISC;
			vport->port_state = LPFC_FDISC;
		} else {
			spin_lock_irq(&phba->hbalock);
			if (phba->fcf.fcf_flag & FCF_DISCOVERY) {
				spin_unlock_irq(&phba->hbalock);
				break;
			}
			
			phba->fcf.fcf_flag |= FCF_ACVL_DISC;
			spin_unlock_irq(&phba->hbalock);
			lpfc_printf_log(phba, KERN_INFO, LOG_FIP |
					LOG_DISCOVERY,
					"2773 Start FCF failover per CVL, "
					"evt_tag:x%x\n", acqe_fip->event_tag);
			rc = lpfc_sli4_redisc_fcf_table(phba);
			if (rc) {
				lpfc_printf_log(phba, KERN_ERR, LOG_FIP |
						LOG_DISCOVERY,
						"2774 Issue FCF rediscover "
						"mabilbox command failed, "
						"through to CVL event\n");
				spin_lock_irq(&phba->hbalock);
				phba->fcf.fcf_flag &= ~FCF_ACVL_DISC;
				spin_unlock_irq(&phba->hbalock);
				lpfc_retry_pport_discovery(phba);
			} else
				lpfc_sli4_clear_fcf_rr_bmask(phba);
		}
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0288 Unknown FCoE event type 0x%x event tag "
			"0x%x\n", event_type, acqe_fip->event_tag);
		break;
	}
}

static void
lpfc_sli4_async_dcbx_evt(struct lpfc_hba *phba,
			 struct lpfc_acqe_dcbx *acqe_dcbx)
{
	phba->fc_eventTag = acqe_dcbx->event_tag;
	lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0290 The SLI4 DCBX asynchronous event is not "
			"handled yet\n");
}

static void
lpfc_sli4_async_grp5_evt(struct lpfc_hba *phba,
			 struct lpfc_acqe_grp5 *acqe_grp5)
{
	uint16_t prev_ll_spd;

	phba->fc_eventTag = acqe_grp5->event_tag;
	phba->fcoe_eventtag = acqe_grp5->event_tag;
	prev_ll_spd = phba->sli4_hba.link_state.logical_speed;
	phba->sli4_hba.link_state.logical_speed =
		(bf_get(lpfc_acqe_grp5_llink_spd, acqe_grp5));
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"2789 GRP5 Async Event: Updating logical link speed "
			"from %dMbps to %dMbps\n", (prev_ll_spd * 10),
			(phba->sli4_hba.link_state.logical_speed*10));
}

void lpfc_sli4_async_event_proc(struct lpfc_hba *phba)
{
	struct lpfc_cq_event *cq_event;

	
	spin_lock_irq(&phba->hbalock);
	phba->hba_flag &= ~ASYNC_EVENT;
	spin_unlock_irq(&phba->hbalock);
	
	while (!list_empty(&phba->sli4_hba.sp_asynce_work_queue)) {
		
		spin_lock_irq(&phba->hbalock);
		list_remove_head(&phba->sli4_hba.sp_asynce_work_queue,
				 cq_event, struct lpfc_cq_event, list);
		spin_unlock_irq(&phba->hbalock);
		
		switch (bf_get(lpfc_trailer_code, &cq_event->cqe.mcqe_cmpl)) {
		case LPFC_TRAILER_CODE_LINK:
			lpfc_sli4_async_link_evt(phba,
						 &cq_event->cqe.acqe_link);
			break;
		case LPFC_TRAILER_CODE_FCOE:
			lpfc_sli4_async_fip_evt(phba, &cq_event->cqe.acqe_fip);
			break;
		case LPFC_TRAILER_CODE_DCBX:
			lpfc_sli4_async_dcbx_evt(phba,
						 &cq_event->cqe.acqe_dcbx);
			break;
		case LPFC_TRAILER_CODE_GRP5:
			lpfc_sli4_async_grp5_evt(phba,
						 &cq_event->cqe.acqe_grp5);
			break;
		case LPFC_TRAILER_CODE_FC:
			lpfc_sli4_async_fc_evt(phba, &cq_event->cqe.acqe_fc);
			break;
		case LPFC_TRAILER_CODE_SLI:
			lpfc_sli4_async_sli_evt(phba, &cq_event->cqe.acqe_sli);
			break;
		default:
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"1804 Invalid asynchrous event code: "
					"x%x\n", bf_get(lpfc_trailer_code,
					&cq_event->cqe.mcqe_cmpl));
			break;
		}
		
		lpfc_sli4_cq_event_release(phba, cq_event);
	}
}

void lpfc_sli4_fcf_redisc_event_proc(struct lpfc_hba *phba)
{
	int rc;

	spin_lock_irq(&phba->hbalock);
	
	phba->fcf.fcf_flag &= ~FCF_REDISC_EVT;
	
	phba->fcf.failover_rec.flag = 0;
	
	phba->fcf.fcf_flag |= FCF_REDISC_FOV;
	spin_unlock_irq(&phba->hbalock);

	
	lpfc_printf_log(phba, KERN_INFO, LOG_FIP | LOG_DISCOVERY,
			"2777 Start post-quiescent FCF table scan\n");
	rc = lpfc_sli4_fcf_scan_read_fcf_rec(phba, LPFC_FCOE_FCF_GET_FIRST);
	if (rc)
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP | LOG_DISCOVERY,
				"2747 Issue FCF scan read FCF mailbox "
				"command failed 0x%x\n", rc);
}

int
lpfc_api_table_setup(struct lpfc_hba *phba, uint8_t dev_grp)
{
	int rc;

	
	phba->pci_dev_grp = dev_grp;

	
	if (dev_grp == LPFC_PCI_DEV_OC)
		phba->sli_rev = LPFC_SLI_REV4;

	
	rc = lpfc_init_api_table_setup(phba, dev_grp);
	if (rc)
		return -ENODEV;
	
	rc = lpfc_scsi_api_table_setup(phba, dev_grp);
	if (rc)
		return -ENODEV;
	
	rc = lpfc_sli_api_table_setup(phba, dev_grp);
	if (rc)
		return -ENODEV;
	
	rc = lpfc_mbox_api_table_setup(phba, dev_grp);
	if (rc)
		return -ENODEV;

	return 0;
}

static void lpfc_log_intr_mode(struct lpfc_hba *phba, uint32_t intr_mode)
{
	switch (intr_mode) {
	case 0:
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0470 Enable INTx interrupt mode.\n");
		break;
	case 1:
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0481 Enabled MSI interrupt mode.\n");
		break;
	case 2:
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0480 Enabled MSI-X interrupt mode.\n");
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0482 Illegal interrupt mode.\n");
		break;
	}
	return;
}

static int
lpfc_enable_pci_dev(struct lpfc_hba *phba)
{
	struct pci_dev *pdev;
	int bars = 0;

	
	if (!phba->pcidev)
		goto out_error;
	else
		pdev = phba->pcidev;
	
	bars = pci_select_bars(pdev, IORESOURCE_MEM);
	
	if (pci_enable_device_mem(pdev))
		goto out_error;
	
	if (pci_request_selected_regions(pdev, bars, LPFC_DRIVER_NAME))
		goto out_disable_device;
	
	pci_set_master(pdev);
	pci_try_set_mwi(pdev);
	pci_save_state(pdev);

	
	if (pci_find_capability(pdev, PCI_CAP_ID_EXP))
		pdev->needs_freset = 1;

	return 0;

out_disable_device:
	pci_disable_device(pdev);
out_error:
	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"1401 Failed to enable pci device, bars:x%x\n", bars);
	return -ENODEV;
}

static void
lpfc_disable_pci_dev(struct lpfc_hba *phba)
{
	struct pci_dev *pdev;
	int bars;

	
	if (!phba->pcidev)
		return;
	else
		pdev = phba->pcidev;
	
	bars = pci_select_bars(pdev, IORESOURCE_MEM);
	
	pci_release_selected_regions(pdev, bars);
	pci_disable_device(pdev);
	
	pci_set_drvdata(pdev, NULL);

	return;
}

void
lpfc_reset_hba(struct lpfc_hba *phba)
{
	
	if (!phba->cfg_enable_hba_reset) {
		phba->link_state = LPFC_HBA_ERROR;
		return;
	}
	lpfc_offline_prep(phba);
	lpfc_offline(phba);
	lpfc_sli_brdrestart(phba);
	lpfc_online(phba);
	lpfc_unblock_mgmt_io(phba);
}

uint16_t
lpfc_sli_sriov_nr_virtfn_get(struct lpfc_hba *phba)
{
	struct pci_dev *pdev = phba->pcidev;
	uint16_t nr_virtfn;
	int pos;

	pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_SRIOV);
	if (pos == 0)
		return 0;

	pci_read_config_word(pdev, pos + PCI_SRIOV_TOTAL_VF, &nr_virtfn);
	return nr_virtfn;
}

int
lpfc_sli_probe_sriov_nr_virtfn(struct lpfc_hba *phba, int nr_vfn)
{
	struct pci_dev *pdev = phba->pcidev;
	uint16_t max_nr_vfn;
	int rc;

	max_nr_vfn = lpfc_sli_sriov_nr_virtfn_get(phba);
	if (nr_vfn > max_nr_vfn) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"3057 Requested vfs (%d) greater than "
				"supported vfs (%d)", nr_vfn, max_nr_vfn);
		return -EINVAL;
	}

	rc = pci_enable_sriov(pdev, nr_vfn);
	if (rc) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"2806 Failed to enable sriov on this device "
				"with vfn number nr_vf:%d, rc:%d\n",
				nr_vfn, rc);
	} else
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"2807 Successful enable sriov on this device "
				"with vfn number nr_vf:%d\n", nr_vfn);
	return rc;
}

static int
lpfc_sli_driver_resource_setup(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli;
	int rc;


	
	init_timer(&phba->hb_tmofunc);
	phba->hb_tmofunc.function = lpfc_hb_timeout;
	phba->hb_tmofunc.data = (unsigned long)phba;

	psli = &phba->sli;
	
	init_timer(&psli->mbox_tmo);
	psli->mbox_tmo.function = lpfc_mbox_timeout;
	psli->mbox_tmo.data = (unsigned long) phba;
	
	init_timer(&phba->fcp_poll_timer);
	phba->fcp_poll_timer.function = lpfc_poll_timeout;
	phba->fcp_poll_timer.data = (unsigned long) phba;
	
	init_timer(&phba->fabric_block_timer);
	phba->fabric_block_timer.function = lpfc_fabric_block_timeout;
	phba->fabric_block_timer.data = (unsigned long) phba;
	
	init_timer(&phba->eratt_poll);
	phba->eratt_poll.function = lpfc_poll_eratt;
	phba->eratt_poll.data = (unsigned long) phba;

	
	phba->work_ha_mask = (HA_ERATT | HA_MBATT | HA_LATT);
	phba->work_ha_mask |= (HA_RXMASK << (LPFC_ELS_RING * 4));

	
	lpfc_get_cfgparam(phba);
	if (phba->pcidev->device == PCI_DEVICE_ID_HORNET) {
		phba->menlo_flag |= HBA_MENLO_SUPPORT;
		
		if (phba->cfg_sg_seg_cnt < LPFC_DEFAULT_MENLO_SG_SEG_CNT)
			phba->cfg_sg_seg_cnt = LPFC_DEFAULT_MENLO_SG_SEG_CNT;
	}

	phba->cfg_sg_dma_buf_size = sizeof(struct fcp_cmnd) +
		sizeof(struct fcp_rsp) +
			((phba->cfg_sg_seg_cnt + 2) * sizeof(struct ulp_bde64));

	if (phba->cfg_enable_bg) {
		phba->cfg_sg_seg_cnt = LPFC_MAX_SG_SEG_CNT;
		phba->cfg_sg_dma_buf_size +=
			phba->cfg_prot_sg_seg_cnt * sizeof(struct ulp_bde64);
	}

	
	lpfc_vport_template.sg_tablesize = phba->cfg_sg_seg_cnt;
	lpfc_template.sg_tablesize = phba->cfg_sg_seg_cnt;

	phba->max_vpi = LPFC_MAX_VPI;
	
	phba->max_vports = 0;

	lpfc_sli_setup(phba);
	lpfc_sli_queue_setup(phba);

	
	if (lpfc_mem_alloc(phba, BPL_ALIGN_SZ))
		return -ENOMEM;

	if (phba->cfg_sriov_nr_virtfn > 0) {
		rc = lpfc_sli_probe_sriov_nr_virtfn(phba,
						 phba->cfg_sriov_nr_virtfn);
		if (rc) {
			lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
					"2808 Requested number of SR-IOV "
					"virtual functions (%d) is not "
					"supported\n",
					phba->cfg_sriov_nr_virtfn);
			phba->cfg_sriov_nr_virtfn = 0;
		}
	}

	return 0;
}

static void
lpfc_sli_driver_resource_unset(struct lpfc_hba *phba)
{
	
	lpfc_mem_free_all(phba);

	return;
}

static int
lpfc_sli4_driver_resource_setup(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli;
	LPFC_MBOXQ_t *mboxq;
	int rc, i, hbq_count, buf_size, dma_buf_size, max_buf_size;
	uint8_t pn_page[LPFC_MAX_SUPPORTED_PAGES] = {0};
	struct lpfc_mqe *mqe;
	int longs, sli_family;
	int sges_per_segment;

	
	rc = lpfc_sli4_post_status_check(phba);
	if (rc)
		return -ENODEV;


	
	init_timer(&phba->hb_tmofunc);
	phba->hb_tmofunc.function = lpfc_hb_timeout;
	phba->hb_tmofunc.data = (unsigned long)phba;
	init_timer(&phba->rrq_tmr);
	phba->rrq_tmr.function = lpfc_rrq_timeout;
	phba->rrq_tmr.data = (unsigned long)phba;

	psli = &phba->sli;
	
	init_timer(&psli->mbox_tmo);
	psli->mbox_tmo.function = lpfc_mbox_timeout;
	psli->mbox_tmo.data = (unsigned long) phba;
	
	init_timer(&phba->fabric_block_timer);
	phba->fabric_block_timer.function = lpfc_fabric_block_timeout;
	phba->fabric_block_timer.data = (unsigned long) phba;
	
	init_timer(&phba->eratt_poll);
	phba->eratt_poll.function = lpfc_poll_eratt;
	phba->eratt_poll.data = (unsigned long) phba;
	
	init_timer(&phba->fcf.redisc_wait);
	phba->fcf.redisc_wait.function = lpfc_sli4_fcf_redisc_wait_tmo;
	phba->fcf.redisc_wait.data = (unsigned long)phba;

	memset((uint8_t *)&phba->mbox_ext_buf_ctx, 0,
		sizeof(struct lpfc_mbox_ext_buf_ctx));
	INIT_LIST_HEAD(&phba->mbox_ext_buf_ctx.ext_dmabuf_list);

	
	lpfc_get_cfgparam(phba);
	phba->max_vpi = LPFC_MAX_VPI;
	
	phba->max_vports = 0;

	
	phba->valid_vlan = 0;
	phba->fc_map[0] = LPFC_FCOE_FCF_MAP0;
	phba->fc_map[1] = LPFC_FCOE_FCF_MAP1;
	phba->fc_map[2] = LPFC_FCOE_FCF_MAP2;

	
	sges_per_segment = 1;
	if (phba->cfg_enable_bg)
		sges_per_segment = 2;

	buf_size = (sizeof(struct fcp_cmnd) + sizeof(struct fcp_rsp) +
		    (((phba->cfg_sg_seg_cnt * sges_per_segment) + 2) *
		    sizeof(struct sli4_sge)));

	sli_family = bf_get(lpfc_sli_intf_sli_family, &phba->sli4_hba.sli_intf);
	max_buf_size = LPFC_SLI4_MAX_BUF_SIZE;
	switch (sli_family) {
	case LPFC_SLI_INTF_FAMILY_BE2:
	case LPFC_SLI_INTF_FAMILY_BE3:
		
		if (bf_get(lpfc_sli_intf_sli_hint1, &phba->sli4_hba.sli_intf) ==
		    LPFC_SLI_INTF_SLI_HINT1_1)
			max_buf_size = LPFC_SLI4_FL1_MAX_BUF_SIZE;
		break;
	case LPFC_SLI_INTF_FAMILY_LNCR_A0:
	case LPFC_SLI_INTF_FAMILY_LNCR_B0:
	default:
		break;
	}

	for (dma_buf_size = LPFC_SLI4_MIN_BUF_SIZE;
	     dma_buf_size < max_buf_size && buf_size > dma_buf_size;
	     dma_buf_size = dma_buf_size << 1)
		;
	if (dma_buf_size == max_buf_size)
		phba->cfg_sg_seg_cnt = (dma_buf_size -
			sizeof(struct fcp_cmnd) - sizeof(struct fcp_rsp) -
			(2 * sizeof(struct sli4_sge))) /
				sizeof(struct sli4_sge);
	phba->cfg_sg_dma_buf_size = dma_buf_size;

	
	hbq_count = lpfc_sli_hbq_count();
	for (i = 0; i < hbq_count; ++i)
		INIT_LIST_HEAD(&phba->hbqs[i].hbq_buffer_list);
	INIT_LIST_HEAD(&phba->rb_pend_list);
	phba->hbqs[LPFC_ELS_HBQ].hbq_alloc_buffer = lpfc_sli4_rb_alloc;
	phba->hbqs[LPFC_ELS_HBQ].hbq_free_buffer = lpfc_sli4_rb_free;

	
	spin_lock_init(&phba->sli4_hba.abts_scsi_buf_list_lock);
	INIT_LIST_HEAD(&phba->sli4_hba.lpfc_abts_scsi_buf_list);
	
	spin_lock_init(&phba->sli4_hba.abts_sgl_list_lock);


	
	INIT_LIST_HEAD(&phba->sli4_hba.sp_cqe_event_pool);
	
	INIT_LIST_HEAD(&phba->sli4_hba.sp_queue_event);
	
	INIT_LIST_HEAD(&phba->sli4_hba.sp_asynce_work_queue);
	
	INIT_LIST_HEAD(&phba->sli4_hba.sp_fcp_xri_aborted_work_queue);
	
	INIT_LIST_HEAD(&phba->sli4_hba.sp_els_xri_aborted_work_queue);
	
	INIT_LIST_HEAD(&phba->sli4_hba.sp_unsol_work_queue);

	
	INIT_LIST_HEAD(&phba->sli4_hba.lpfc_rpi_blk_list);
	INIT_LIST_HEAD(&phba->sli4_hba.lpfc_xri_blk_list);
	INIT_LIST_HEAD(&phba->sli4_hba.lpfc_vfi_blk_list);
	INIT_LIST_HEAD(&phba->lpfc_vpi_blk_list);

	
	lpfc_sli_setup(phba);
	lpfc_sli_queue_setup(phba);

	
	rc = lpfc_mem_alloc(phba, SGL_ALIGN_SZ);
	if (rc)
		return -ENOMEM;

	
	if (bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) ==
	    LPFC_SLI_INTF_IF_TYPE_2) {
		rc = lpfc_pci_function_reset(phba);
		if (unlikely(rc))
			return -ENODEV;
	}

	
	rc = lpfc_create_bootstrap_mbox(phba);
	if (unlikely(rc))
		goto out_free_mem;

	
	rc = lpfc_setup_endian_order(phba);
	if (unlikely(rc))
		goto out_free_bsmbx;

	
	rc = lpfc_sli4_read_config(phba);
	if (unlikely(rc))
		goto out_free_bsmbx;

	
	if (bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) ==
	    LPFC_SLI_INTF_IF_TYPE_0) {
		rc = lpfc_pci_function_reset(phba);
		if (unlikely(rc))
			goto out_free_bsmbx;
	}

	mboxq = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool,
						       GFP_KERNEL);
	if (!mboxq) {
		rc = -ENOMEM;
		goto out_free_bsmbx;
	}

	
	lpfc_supported_pages(mboxq);
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	if (!rc) {
		mqe = &mboxq->u.mqe;
		memcpy(&pn_page[0], ((uint8_t *)&mqe->un.supp_pages.word3),
		       LPFC_MAX_SUPPORTED_PAGES);
		for (i = 0; i < LPFC_MAX_SUPPORTED_PAGES; i++) {
			switch (pn_page[i]) {
			case LPFC_SLI4_PARAMETERS:
				phba->sli4_hba.pc_sli4_params.supported = 1;
				break;
			default:
				break;
			}
		}
		
		if (phba->sli4_hba.pc_sli4_params.supported)
			rc = lpfc_pc_sli4_params_get(phba, mboxq);
		if (rc) {
			mempool_free(mboxq, phba->mbox_mem_pool);
			rc = -EIO;
			goto out_free_bsmbx;
		}
	}
	rc = lpfc_get_sli4_parameters(phba, mboxq);
	if (rc) {
		if (phba->sli4_hba.extents_in_use &&
		    phba->sli4_hba.rpi_hdrs_in_use) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2999 Unsupported SLI4 Parameters "
				"Extents and RPI headers enabled.\n");
			goto out_free_bsmbx;
		}
	}
	mempool_free(mboxq, phba->mbox_mem_pool);
	
	rc = lpfc_sli4_queue_verify(phba);
	if (rc)
		goto out_free_bsmbx;

	
	rc = lpfc_sli4_cq_event_pool_create(phba);
	if (rc)
		goto out_free_bsmbx;

	
	rc = lpfc_init_sgl_list(phba);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1400 Failed to initialize sgl list.\n");
		goto out_destroy_cq_event_pool;
	}
	rc = lpfc_init_active_sgl_array(phba);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1430 Failed to initialize sgl list.\n");
		goto out_free_sgl_list;
	}
	rc = lpfc_sli4_init_rpi_hdrs(phba);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1432 Failed to initialize rpi headers.\n");
		goto out_free_active_sgl;
	}

	
	longs = (LPFC_SLI4_FCF_TBL_INDX_MAX + BITS_PER_LONG - 1)/BITS_PER_LONG;
	phba->fcf.fcf_rr_bmask = kzalloc(longs * sizeof(unsigned long),
					 GFP_KERNEL);
	if (!phba->fcf.fcf_rr_bmask) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2759 Failed allocate memory for FCF round "
				"robin failover bmask\n");
		rc = -ENOMEM;
		goto out_remove_rpi_hdrs;
	}

	if (phba->cfg_fcp_eq_count) {
		phba->sli4_hba.fcp_eq_hdl =
				kzalloc((sizeof(struct lpfc_fcp_eq_hdl) *
				    phba->cfg_fcp_eq_count), GFP_KERNEL);
		if (!phba->sli4_hba.fcp_eq_hdl) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"2572 Failed allocate memory for "
					"fast-path per-EQ handle array\n");
			rc = -ENOMEM;
			goto out_free_fcf_rr_bmask;
		}
	}

	phba->sli4_hba.msix_entries = kzalloc((sizeof(struct msix_entry) *
				      phba->sli4_hba.cfg_eqn), GFP_KERNEL);
	if (!phba->sli4_hba.msix_entries) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2573 Failed allocate memory for msi-x "
				"interrupt vector entries\n");
		rc = -ENOMEM;
		goto out_free_fcp_eq_hdl;
	}

	if (phba->cfg_sriov_nr_virtfn > 0) {
		rc = lpfc_sli_probe_sriov_nr_virtfn(phba,
						 phba->cfg_sriov_nr_virtfn);
		if (rc) {
			lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
					"3020 Requested number of SR-IOV "
					"virtual functions (%d) is not "
					"supported\n",
					phba->cfg_sriov_nr_virtfn);
			phba->cfg_sriov_nr_virtfn = 0;
		}
	}

	return 0;

out_free_fcp_eq_hdl:
	kfree(phba->sli4_hba.fcp_eq_hdl);
out_free_fcf_rr_bmask:
	kfree(phba->fcf.fcf_rr_bmask);
out_remove_rpi_hdrs:
	lpfc_sli4_remove_rpi_hdrs(phba);
out_free_active_sgl:
	lpfc_free_active_sgl(phba);
out_free_sgl_list:
	lpfc_free_sgl_list(phba);
out_destroy_cq_event_pool:
	lpfc_sli4_cq_event_pool_destroy(phba);
out_free_bsmbx:
	lpfc_destroy_bootstrap_mbox(phba);
out_free_mem:
	lpfc_mem_free(phba);
	return rc;
}

static void
lpfc_sli4_driver_resource_unset(struct lpfc_hba *phba)
{
	struct lpfc_fcf_conn_entry *conn_entry, *next_conn_entry;

	
	kfree(phba->sli4_hba.msix_entries);

	
	kfree(phba->sli4_hba.fcp_eq_hdl);

	
	lpfc_sli4_remove_rpi_hdrs(phba);
	lpfc_sli4_remove_rpis(phba);

	
	kfree(phba->fcf.fcf_rr_bmask);

	
	lpfc_free_active_sgl(phba);
	lpfc_free_sgl_list(phba);

	
	kfree(phba->sli4_hba.lpfc_scsi_psb_array);

	
	lpfc_sli4_cq_event_release_all(phba);
	lpfc_sli4_cq_event_pool_destroy(phba);

	
	lpfc_sli4_dealloc_resource_identifiers(phba);

	
	lpfc_destroy_bootstrap_mbox(phba);

	
	lpfc_mem_free_all(phba);

	
	list_for_each_entry_safe(conn_entry, next_conn_entry,
		&phba->fcf_conn_rec_list, list) {
		list_del_init(&conn_entry->list);
		kfree(conn_entry);
	}

	return;
}

int
lpfc_init_api_table_setup(struct lpfc_hba *phba, uint8_t dev_grp)
{
	phba->lpfc_hba_init_link = lpfc_hba_init_link;
	phba->lpfc_hba_down_link = lpfc_hba_down_link;
	phba->lpfc_selective_reset = lpfc_selective_reset;
	switch (dev_grp) {
	case LPFC_PCI_DEV_LP:
		phba->lpfc_hba_down_post = lpfc_hba_down_post_s3;
		phba->lpfc_handle_eratt = lpfc_handle_eratt_s3;
		phba->lpfc_stop_port = lpfc_stop_port_s3;
		break;
	case LPFC_PCI_DEV_OC:
		phba->lpfc_hba_down_post = lpfc_hba_down_post_s4;
		phba->lpfc_handle_eratt = lpfc_handle_eratt_s4;
		phba->lpfc_stop_port = lpfc_stop_port_s4;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1431 Invalid HBA PCI-device group: 0x%x\n",
				dev_grp);
		return -ENODEV;
		break;
	}
	return 0;
}

static int
lpfc_setup_driver_resource_phase1(struct lpfc_hba *phba)
{
	atomic_set(&phba->fast_event_count, 0);
	spin_lock_init(&phba->hbalock);

	
	spin_lock_init(&phba->ndlp_lock);

	INIT_LIST_HEAD(&phba->port_list);
	INIT_LIST_HEAD(&phba->work_list);
	init_waitqueue_head(&phba->wait_4_mlo_m_q);

	
	init_waitqueue_head(&phba->work_waitq);

	
	spin_lock_init(&phba->scsi_buf_list_lock);
	INIT_LIST_HEAD(&phba->lpfc_scsi_buf_list);

	
	INIT_LIST_HEAD(&phba->fabric_iocb_list);

	
	INIT_LIST_HEAD(&phba->elsbuf);

	
	INIT_LIST_HEAD(&phba->fcf_conn_rec_list);

	return 0;
}

static int
lpfc_setup_driver_resource_phase2(struct lpfc_hba *phba)
{
	int error;

	
	phba->worker_thread = kthread_run(lpfc_do_work, phba,
					  "lpfc_worker_%d", phba->brd_no);
	if (IS_ERR(phba->worker_thread)) {
		error = PTR_ERR(phba->worker_thread);
		return error;
	}

	return 0;
}

static void
lpfc_unset_driver_resource_phase2(struct lpfc_hba *phba)
{
	
	kthread_stop(phba->worker_thread);
}

static void
lpfc_free_iocb_list(struct lpfc_hba *phba)
{
	struct lpfc_iocbq *iocbq_entry = NULL, *iocbq_next = NULL;

	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(iocbq_entry, iocbq_next,
				 &phba->lpfc_iocb_list, list) {
		list_del(&iocbq_entry->list);
		kfree(iocbq_entry);
		phba->total_iocbq_bufs--;
	}
	spin_unlock_irq(&phba->hbalock);

	return;
}

static int
lpfc_init_iocb_list(struct lpfc_hba *phba, int iocb_count)
{
	struct lpfc_iocbq *iocbq_entry = NULL;
	uint16_t iotag;
	int i;

	
	INIT_LIST_HEAD(&phba->lpfc_iocb_list);
	for (i = 0; i < iocb_count; i++) {
		iocbq_entry = kzalloc(sizeof(struct lpfc_iocbq), GFP_KERNEL);
		if (iocbq_entry == NULL) {
			printk(KERN_ERR "%s: only allocated %d iocbs of "
				"expected %d count. Unloading driver.\n",
				__func__, i, LPFC_IOCB_LIST_CNT);
			goto out_free_iocbq;
		}

		iotag = lpfc_sli_next_iotag(phba, iocbq_entry);
		if (iotag == 0) {
			kfree(iocbq_entry);
			printk(KERN_ERR "%s: failed to allocate IOTAG. "
				"Unloading driver.\n", __func__);
			goto out_free_iocbq;
		}
		iocbq_entry->sli4_lxritag = NO_XRI;
		iocbq_entry->sli4_xritag = NO_XRI;

		spin_lock_irq(&phba->hbalock);
		list_add(&iocbq_entry->list, &phba->lpfc_iocb_list);
		phba->total_iocbq_bufs++;
		spin_unlock_irq(&phba->hbalock);
	}

	return 0;

out_free_iocbq:
	lpfc_free_iocb_list(phba);

	return -ENOMEM;
}

static void
lpfc_free_sgl_list(struct lpfc_hba *phba)
{
	struct lpfc_sglq *sglq_entry = NULL, *sglq_next = NULL;
	LIST_HEAD(sglq_list);

	spin_lock_irq(&phba->hbalock);
	list_splice_init(&phba->sli4_hba.lpfc_sgl_list, &sglq_list);
	spin_unlock_irq(&phba->hbalock);

	list_for_each_entry_safe(sglq_entry, sglq_next,
				 &sglq_list, list) {
		list_del(&sglq_entry->list);
		lpfc_mbuf_free(phba, sglq_entry->virt, sglq_entry->phys);
		kfree(sglq_entry);
		phba->sli4_hba.total_sglq_bufs--;
	}
	kfree(phba->sli4_hba.lpfc_els_sgl_array);
}

static int
lpfc_init_active_sgl_array(struct lpfc_hba *phba)
{
	int size;
	size = sizeof(struct lpfc_sglq *);
	size *= phba->sli4_hba.max_cfg_param.max_xri;

	phba->sli4_hba.lpfc_sglq_active_list =
		kzalloc(size, GFP_KERNEL);
	if (!phba->sli4_hba.lpfc_sglq_active_list)
		return -ENOMEM;
	return 0;
}

static void
lpfc_free_active_sgl(struct lpfc_hba *phba)
{
	kfree(phba->sli4_hba.lpfc_sglq_active_list);
}

static int
lpfc_init_sgl_list(struct lpfc_hba *phba)
{
	struct lpfc_sglq *sglq_entry = NULL;
	int i;
	int els_xri_cnt;

	els_xri_cnt = lpfc_sli4_get_els_iocb_cnt(phba);
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"2400 ELS XRI count %d.\n",
				els_xri_cnt);
	
	INIT_LIST_HEAD(&phba->sli4_hba.lpfc_sgl_list);
	INIT_LIST_HEAD(&phba->sli4_hba.lpfc_abts_els_sgl_list);

	
	if (phba->sli4_hba.max_cfg_param.max_xri <= els_xri_cnt) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2562 No room left for SCSI XRI allocation: "
				"max_xri=%d, els_xri=%d\n",
				phba->sli4_hba.max_cfg_param.max_xri,
				els_xri_cnt);
		return -ENOMEM;
	}

	
	phba->sli4_hba.lpfc_els_sgl_array =
			kzalloc((sizeof(struct lpfc_sglq *) * els_xri_cnt),
			GFP_KERNEL);

	if (!phba->sli4_hba.lpfc_els_sgl_array) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2401 Failed to allocate memory for ELS "
				"XRI management array of size %d.\n",
				els_xri_cnt);
		return -ENOMEM;
	}

	
	phba->sli4_hba.scsi_xri_max =
			phba->sli4_hba.max_cfg_param.max_xri - els_xri_cnt;
	phba->sli4_hba.scsi_xri_cnt = 0;
	phba->sli4_hba.lpfc_scsi_psb_array =
			kzalloc((sizeof(struct lpfc_scsi_buf *) *
			phba->sli4_hba.scsi_xri_max), GFP_KERNEL);

	if (!phba->sli4_hba.lpfc_scsi_psb_array) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2563 Failed to allocate memory for SCSI "
				"XRI management array of size %d.\n",
				phba->sli4_hba.scsi_xri_max);
		kfree(phba->sli4_hba.lpfc_els_sgl_array);
		return -ENOMEM;
	}

	for (i = 0; i < els_xri_cnt; i++) {
		sglq_entry = kzalloc(sizeof(struct lpfc_sglq), GFP_KERNEL);
		if (sglq_entry == NULL) {
			printk(KERN_ERR "%s: only allocated %d sgls of "
				"expected %d count. Unloading driver.\n",
				__func__, i, els_xri_cnt);
			goto out_free_mem;
		}

		sglq_entry->buff_type = GEN_BUFF_TYPE;
		sglq_entry->virt = lpfc_mbuf_alloc(phba, 0, &sglq_entry->phys);
		if (sglq_entry->virt == NULL) {
			kfree(sglq_entry);
			printk(KERN_ERR "%s: failed to allocate mbuf.\n"
				"Unloading driver.\n", __func__);
			goto out_free_mem;
		}
		sglq_entry->sgl = sglq_entry->virt;
		memset(sglq_entry->sgl, 0, LPFC_BPL_SIZE);

		
		spin_lock_irq(&phba->hbalock);
		sglq_entry->state = SGL_FREED;
		list_add_tail(&sglq_entry->list, &phba->sli4_hba.lpfc_sgl_list);
		phba->sli4_hba.lpfc_els_sgl_array[i] = sglq_entry;
		phba->sli4_hba.total_sglq_bufs++;
		spin_unlock_irq(&phba->hbalock);
	}
	return 0;

out_free_mem:
	kfree(phba->sli4_hba.lpfc_scsi_psb_array);
	lpfc_free_sgl_list(phba);
	return -ENOMEM;
}

int
lpfc_sli4_init_rpi_hdrs(struct lpfc_hba *phba)
{
	int rc = 0;
	struct lpfc_rpi_hdr *rpi_hdr;

	INIT_LIST_HEAD(&phba->sli4_hba.lpfc_rpi_hdr_list);
	if (!phba->sli4_hba.rpi_hdrs_in_use)
		return rc;
	if (phba->sli4_hba.extents_in_use)
		return -EIO;

	rpi_hdr = lpfc_sli4_create_rpi_hdr(phba);
	if (!rpi_hdr) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"0391 Error during rpi post operation\n");
		lpfc_sli4_remove_rpis(phba);
		rc = -ENODEV;
	}

	return rc;
}

struct lpfc_rpi_hdr *
lpfc_sli4_create_rpi_hdr(struct lpfc_hba *phba)
{
	uint16_t rpi_limit, curr_rpi_range;
	struct lpfc_dmabuf *dmabuf;
	struct lpfc_rpi_hdr *rpi_hdr;
	uint32_t rpi_count;

	if (!phba->sli4_hba.rpi_hdrs_in_use)
		return NULL;
	if (phba->sli4_hba.extents_in_use)
		return NULL;

	
	rpi_limit = phba->sli4_hba.max_cfg_param.rpi_base +
	phba->sli4_hba.max_cfg_param.max_rpi - 1;

	spin_lock_irq(&phba->hbalock);
	curr_rpi_range = phba->sli4_hba.next_rpi;
	spin_unlock_irq(&phba->hbalock);

	if ((curr_rpi_range + (LPFC_RPI_HDR_COUNT - 1)) > rpi_limit)
		rpi_count = rpi_limit - curr_rpi_range;
	else
		rpi_count = LPFC_RPI_HDR_COUNT;

	if (!rpi_count)
		return NULL;
	dmabuf = kzalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (!dmabuf)
		return NULL;

	dmabuf->virt = dma_alloc_coherent(&phba->pcidev->dev,
					  LPFC_HDR_TEMPLATE_SIZE,
					  &dmabuf->phys,
					  GFP_KERNEL);
	if (!dmabuf->virt) {
		rpi_hdr = NULL;
		goto err_free_dmabuf;
	}

	memset(dmabuf->virt, 0, LPFC_HDR_TEMPLATE_SIZE);
	if (!IS_ALIGNED(dmabuf->phys, LPFC_HDR_TEMPLATE_SIZE)) {
		rpi_hdr = NULL;
		goto err_free_coherent;
	}

	
	rpi_hdr = kzalloc(sizeof(struct lpfc_rpi_hdr), GFP_KERNEL);
	if (!rpi_hdr)
		goto err_free_coherent;

	rpi_hdr->dmabuf = dmabuf;
	rpi_hdr->len = LPFC_HDR_TEMPLATE_SIZE;
	rpi_hdr->page_count = 1;
	spin_lock_irq(&phba->hbalock);

	
	rpi_hdr->start_rpi = curr_rpi_range;
	list_add_tail(&rpi_hdr->list, &phba->sli4_hba.lpfc_rpi_hdr_list);

	phba->sli4_hba.next_rpi += rpi_count;
	spin_unlock_irq(&phba->hbalock);
	return rpi_hdr;

 err_free_coherent:
	dma_free_coherent(&phba->pcidev->dev, LPFC_HDR_TEMPLATE_SIZE,
			  dmabuf->virt, dmabuf->phys);
 err_free_dmabuf:
	kfree(dmabuf);
	return NULL;
}

void
lpfc_sli4_remove_rpi_hdrs(struct lpfc_hba *phba)
{
	struct lpfc_rpi_hdr *rpi_hdr, *next_rpi_hdr;

	if (!phba->sli4_hba.rpi_hdrs_in_use)
		goto exit;

	list_for_each_entry_safe(rpi_hdr, next_rpi_hdr,
				 &phba->sli4_hba.lpfc_rpi_hdr_list, list) {
		list_del(&rpi_hdr->list);
		dma_free_coherent(&phba->pcidev->dev, rpi_hdr->len,
				  rpi_hdr->dmabuf->virt, rpi_hdr->dmabuf->phys);
		kfree(rpi_hdr->dmabuf);
		kfree(rpi_hdr);
	}
 exit:
	
	phba->sli4_hba.next_rpi = 0;
}

static struct lpfc_hba *
lpfc_hba_alloc(struct pci_dev *pdev)
{
	struct lpfc_hba *phba;

	
	phba = kzalloc(sizeof(struct lpfc_hba), GFP_KERNEL);
	if (!phba) {
		dev_err(&pdev->dev, "failed to allocate hba struct\n");
		return NULL;
	}

	
	phba->pcidev = pdev;

	
	phba->brd_no = lpfc_get_instance();
	if (phba->brd_no < 0) {
		kfree(phba);
		return NULL;
	}

	spin_lock_init(&phba->ct_ev_lock);
	INIT_LIST_HEAD(&phba->ct_ev_waiters);

	return phba;
}

static void
lpfc_hba_free(struct lpfc_hba *phba)
{
	
	idr_remove(&lpfc_hba_index, phba->brd_no);

	kfree(phba);
	return;
}

static int
lpfc_create_shost(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport;
	struct Scsi_Host  *shost;

	
	phba->fc_edtov = FF_DEF_EDTOV;
	phba->fc_ratov = FF_DEF_RATOV;
	phba->fc_altov = FF_DEF_ALTOV;
	phba->fc_arbtov = FF_DEF_ARBTOV;

	atomic_set(&phba->sdev_cnt, 0);
	vport = lpfc_create_port(phba, phba->brd_no, &phba->pcidev->dev);
	if (!vport)
		return -ENODEV;

	shost = lpfc_shost_from_vport(vport);
	phba->pport = vport;
	lpfc_debugfs_initialize(vport);
	
	pci_set_drvdata(phba->pcidev, shost);

	return 0;
}

static void
lpfc_destroy_shost(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;

	
	destroy_port(vport);

	return;
}

static void
lpfc_setup_bg(struct lpfc_hba *phba, struct Scsi_Host *shost)
{
	int pagecnt = 10;
	if (lpfc_prot_mask && lpfc_prot_guard) {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"1478 Registering BlockGuard with the "
				"SCSI layer\n");
		scsi_host_set_prot(shost, lpfc_prot_mask);
		scsi_host_set_guard(shost, lpfc_prot_guard);
	}
	if (!_dump_buf_data) {
		while (pagecnt) {
			spin_lock_init(&_dump_buf_lock);
			_dump_buf_data =
				(char *) __get_free_pages(GFP_KERNEL, pagecnt);
			if (_dump_buf_data) {
				lpfc_printf_log(phba, KERN_ERR, LOG_BG,
					"9043 BLKGRD: allocated %d pages for "
				       "_dump_buf_data at 0x%p\n",
				       (1 << pagecnt), _dump_buf_data);
				_dump_buf_data_order = pagecnt;
				memset(_dump_buf_data, 0,
				       ((1 << PAGE_SHIFT) << pagecnt));
				break;
			} else
				--pagecnt;
		}
		if (!_dump_buf_data_order)
			lpfc_printf_log(phba, KERN_ERR, LOG_BG,
				"9044 BLKGRD: ERROR unable to allocate "
			       "memory for hexdump\n");
	} else
		lpfc_printf_log(phba, KERN_ERR, LOG_BG,
			"9045 BLKGRD: already allocated _dump_buf_data=0x%p"
		       "\n", _dump_buf_data);
	if (!_dump_buf_dif) {
		while (pagecnt) {
			_dump_buf_dif =
				(char *) __get_free_pages(GFP_KERNEL, pagecnt);
			if (_dump_buf_dif) {
				lpfc_printf_log(phba, KERN_ERR, LOG_BG,
					"9046 BLKGRD: allocated %d pages for "
				       "_dump_buf_dif at 0x%p\n",
				       (1 << pagecnt), _dump_buf_dif);
				_dump_buf_dif_order = pagecnt;
				memset(_dump_buf_dif, 0,
				       ((1 << PAGE_SHIFT) << pagecnt));
				break;
			} else
				--pagecnt;
		}
		if (!_dump_buf_dif_order)
			lpfc_printf_log(phba, KERN_ERR, LOG_BG,
			"9047 BLKGRD: ERROR unable to allocate "
			       "memory for hexdump\n");
	} else
		lpfc_printf_log(phba, KERN_ERR, LOG_BG,
			"9048 BLKGRD: already allocated _dump_buf_dif=0x%p\n",
		       _dump_buf_dif);
}

static void
lpfc_post_init_setup(struct lpfc_hba *phba)
{
	struct Scsi_Host  *shost;
	struct lpfc_adapter_event_header adapter_event;

	
	lpfc_get_hba_model_desc(phba, phba->ModelName, phba->ModelDesc);

	shost = pci_get_drvdata(phba->pcidev);
	shost->can_queue = phba->cfg_hba_queue_depth - 10;
	if (phba->sli3_options & LPFC_SLI3_BG_ENABLED)
		lpfc_setup_bg(phba, shost);

	lpfc_host_attrib_init(shost);

	if (phba->cfg_poll & DISABLE_FCP_RING_INT) {
		spin_lock_irq(shost->host_lock);
		lpfc_poll_start_timer(phba);
		spin_unlock_irq(shost->host_lock);
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"0428 Perform SCSI scan\n");
	
	adapter_event.event_type = FC_REG_ADAPTER_EVENT;
	adapter_event.subcategory = LPFC_EVENT_ARRIVAL;
	fc_host_post_vendor_event(shost, fc_get_event_number(),
				  sizeof(adapter_event),
				  (char *) &adapter_event,
				  LPFC_NL_VENDOR_ID);
	return;
}

static int
lpfc_sli_pci_mem_setup(struct lpfc_hba *phba)
{
	struct pci_dev *pdev;
	unsigned long bar0map_len, bar2map_len;
	int i, hbq_count;
	void *ptr;
	int error = -ENODEV;

	
	if (!phba->pcidev)
		return error;
	else
		pdev = phba->pcidev;

	
	if (pci_set_dma_mask(pdev, DMA_BIT_MASK(64)) != 0
	 || pci_set_consistent_dma_mask(pdev,DMA_BIT_MASK(64)) != 0) {
		if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32)) != 0
		 || pci_set_consistent_dma_mask(pdev,DMA_BIT_MASK(32)) != 0) {
			return error;
		}
	}

	phba->pci_bar0_map = pci_resource_start(pdev, 0);
	bar0map_len = pci_resource_len(pdev, 0);

	phba->pci_bar2_map = pci_resource_start(pdev, 2);
	bar2map_len = pci_resource_len(pdev, 2);

	
	phba->slim_memmap_p = ioremap(phba->pci_bar0_map, bar0map_len);
	if (!phba->slim_memmap_p) {
		dev_printk(KERN_ERR, &pdev->dev,
			   "ioremap failed for SLIM memory.\n");
		goto out;
	}

	
	phba->ctrl_regs_memmap_p = ioremap(phba->pci_bar2_map, bar2map_len);
	if (!phba->ctrl_regs_memmap_p) {
		dev_printk(KERN_ERR, &pdev->dev,
			   "ioremap failed for HBA control registers.\n");
		goto out_iounmap_slim;
	}

	
	phba->slim2p.virt = dma_alloc_coherent(&pdev->dev,
					       SLI2_SLIM_SIZE,
					       &phba->slim2p.phys,
					       GFP_KERNEL);
	if (!phba->slim2p.virt)
		goto out_iounmap;

	memset(phba->slim2p.virt, 0, SLI2_SLIM_SIZE);
	phba->mbox = phba->slim2p.virt + offsetof(struct lpfc_sli2_slim, mbx);
	phba->mbox_ext = (phba->slim2p.virt +
		offsetof(struct lpfc_sli2_slim, mbx_ext_words));
	phba->pcb = (phba->slim2p.virt + offsetof(struct lpfc_sli2_slim, pcb));
	phba->IOCBs = (phba->slim2p.virt +
		       offsetof(struct lpfc_sli2_slim, IOCBs));

	phba->hbqslimp.virt = dma_alloc_coherent(&pdev->dev,
						 lpfc_sli_hbq_size(),
						 &phba->hbqslimp.phys,
						 GFP_KERNEL);
	if (!phba->hbqslimp.virt)
		goto out_free_slim;

	hbq_count = lpfc_sli_hbq_count();
	ptr = phba->hbqslimp.virt;
	for (i = 0; i < hbq_count; ++i) {
		phba->hbqs[i].hbq_virt = ptr;
		INIT_LIST_HEAD(&phba->hbqs[i].hbq_buffer_list);
		ptr += (lpfc_hbq_defs[i]->entry_count *
			sizeof(struct lpfc_hbq_entry));
	}
	phba->hbqs[LPFC_ELS_HBQ].hbq_alloc_buffer = lpfc_els_hbq_alloc;
	phba->hbqs[LPFC_ELS_HBQ].hbq_free_buffer = lpfc_els_hbq_free;

	memset(phba->hbqslimp.virt, 0, lpfc_sli_hbq_size());

	INIT_LIST_HEAD(&phba->rb_pend_list);

	phba->MBslimaddr = phba->slim_memmap_p;
	phba->HAregaddr = phba->ctrl_regs_memmap_p + HA_REG_OFFSET;
	phba->CAregaddr = phba->ctrl_regs_memmap_p + CA_REG_OFFSET;
	phba->HSregaddr = phba->ctrl_regs_memmap_p + HS_REG_OFFSET;
	phba->HCregaddr = phba->ctrl_regs_memmap_p + HC_REG_OFFSET;

	return 0;

out_free_slim:
	dma_free_coherent(&pdev->dev, SLI2_SLIM_SIZE,
			  phba->slim2p.virt, phba->slim2p.phys);
out_iounmap:
	iounmap(phba->ctrl_regs_memmap_p);
out_iounmap_slim:
	iounmap(phba->slim_memmap_p);
out:
	return error;
}

static void
lpfc_sli_pci_mem_unset(struct lpfc_hba *phba)
{
	struct pci_dev *pdev;

	
	if (!phba->pcidev)
		return;
	else
		pdev = phba->pcidev;

	
	dma_free_coherent(&pdev->dev, lpfc_sli_hbq_size(),
			  phba->hbqslimp.virt, phba->hbqslimp.phys);
	dma_free_coherent(&pdev->dev, SLI2_SLIM_SIZE,
			  phba->slim2p.virt, phba->slim2p.phys);

	
	iounmap(phba->ctrl_regs_memmap_p);
	iounmap(phba->slim_memmap_p);

	return;
}

int
lpfc_sli4_post_status_check(struct lpfc_hba *phba)
{
	struct lpfc_register portsmphr_reg, uerrlo_reg, uerrhi_reg;
	struct lpfc_register reg_data;
	int i, port_error = 0;
	uint32_t if_type;

	memset(&portsmphr_reg, 0, sizeof(portsmphr_reg));
	memset(&reg_data, 0, sizeof(reg_data));
	if (!phba->sli4_hba.PSMPHRregaddr)
		return -ENODEV;

	
	for (i = 0; i < 3000; i++) {
		if (lpfc_readl(phba->sli4_hba.PSMPHRregaddr,
			&portsmphr_reg.word0) ||
			(bf_get(lpfc_port_smphr_perr, &portsmphr_reg))) {
			
			port_error = -ENODEV;
			break;
		}
		if (LPFC_POST_STAGE_PORT_READY ==
		    bf_get(lpfc_port_smphr_port_status, &portsmphr_reg))
			break;
		msleep(10);
	}

	if (port_error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"1408 Port Failed POST - portsmphr=0x%x, "
			"perr=x%x, sfi=x%x, nip=x%x, ipc=x%x, scr1=x%x, "
			"scr2=x%x, hscratch=x%x, pstatus=x%x\n",
			portsmphr_reg.word0,
			bf_get(lpfc_port_smphr_perr, &portsmphr_reg),
			bf_get(lpfc_port_smphr_sfi, &portsmphr_reg),
			bf_get(lpfc_port_smphr_nip, &portsmphr_reg),
			bf_get(lpfc_port_smphr_ipc, &portsmphr_reg),
			bf_get(lpfc_port_smphr_scr1, &portsmphr_reg),
			bf_get(lpfc_port_smphr_scr2, &portsmphr_reg),
			bf_get(lpfc_port_smphr_host_scratch, &portsmphr_reg),
			bf_get(lpfc_port_smphr_port_status, &portsmphr_reg));
	} else {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"2534 Device Info: SLIFamily=0x%x, "
				"SLIRev=0x%x, IFType=0x%x, SLIHint_1=0x%x, "
				"SLIHint_2=0x%x, FT=0x%x\n",
				bf_get(lpfc_sli_intf_sli_family,
				       &phba->sli4_hba.sli_intf),
				bf_get(lpfc_sli_intf_slirev,
				       &phba->sli4_hba.sli_intf),
				bf_get(lpfc_sli_intf_if_type,
				       &phba->sli4_hba.sli_intf),
				bf_get(lpfc_sli_intf_sli_hint1,
				       &phba->sli4_hba.sli_intf),
				bf_get(lpfc_sli_intf_sli_hint2,
				       &phba->sli4_hba.sli_intf),
				bf_get(lpfc_sli_intf_func_type,
				       &phba->sli4_hba.sli_intf));
		if_type = bf_get(lpfc_sli_intf_if_type,
				 &phba->sli4_hba.sli_intf);
		switch (if_type) {
		case LPFC_SLI_INTF_IF_TYPE_0:
			phba->sli4_hba.ue_mask_lo =
			      readl(phba->sli4_hba.u.if_type0.UEMASKLOregaddr);
			phba->sli4_hba.ue_mask_hi =
			      readl(phba->sli4_hba.u.if_type0.UEMASKHIregaddr);
			uerrlo_reg.word0 =
			      readl(phba->sli4_hba.u.if_type0.UERRLOregaddr);
			uerrhi_reg.word0 =
				readl(phba->sli4_hba.u.if_type0.UERRHIregaddr);
			if ((~phba->sli4_hba.ue_mask_lo & uerrlo_reg.word0) ||
			    (~phba->sli4_hba.ue_mask_hi & uerrhi_reg.word0)) {
				lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
						"1422 Unrecoverable Error "
						"Detected during POST "
						"uerr_lo_reg=0x%x, "
						"uerr_hi_reg=0x%x, "
						"ue_mask_lo_reg=0x%x, "
						"ue_mask_hi_reg=0x%x\n",
						uerrlo_reg.word0,
						uerrhi_reg.word0,
						phba->sli4_hba.ue_mask_lo,
						phba->sli4_hba.ue_mask_hi);
				port_error = -ENODEV;
			}
			break;
		case LPFC_SLI_INTF_IF_TYPE_2:
			
			if (lpfc_readl(phba->sli4_hba.u.if_type2.STATUSregaddr,
				&reg_data.word0) ||
				(bf_get(lpfc_sliport_status_err, &reg_data) &&
				 !bf_get(lpfc_sliport_status_rn, &reg_data))) {
				phba->work_status[0] =
					readl(phba->sli4_hba.u.if_type2.
					      ERR1regaddr);
				phba->work_status[1] =
					readl(phba->sli4_hba.u.if_type2.
					      ERR2regaddr);
				lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"2888 Unrecoverable port error "
					"following POST: port status reg "
					"0x%x, port_smphr reg 0x%x, "
					"error 1=0x%x, error 2=0x%x\n",
					reg_data.word0,
					portsmphr_reg.word0,
					phba->work_status[0],
					phba->work_status[1]);
				port_error = -ENODEV;
			}
			break;
		case LPFC_SLI_INTF_IF_TYPE_1:
		default:
			break;
		}
	}
	return port_error;
}

static void
lpfc_sli4_bar0_register_memmap(struct lpfc_hba *phba, uint32_t if_type)
{
	switch (if_type) {
	case LPFC_SLI_INTF_IF_TYPE_0:
		phba->sli4_hba.u.if_type0.UERRLOregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_UERR_STATUS_LO;
		phba->sli4_hba.u.if_type0.UERRHIregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_UERR_STATUS_HI;
		phba->sli4_hba.u.if_type0.UEMASKLOregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_UE_MASK_LO;
		phba->sli4_hba.u.if_type0.UEMASKHIregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_UE_MASK_HI;
		phba->sli4_hba.SLIINTFregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_SLI_INTF;
		break;
	case LPFC_SLI_INTF_IF_TYPE_2:
		phba->sli4_hba.u.if_type2.ERR1regaddr =
			phba->sli4_hba.conf_regs_memmap_p +
						LPFC_CTL_PORT_ER1_OFFSET;
		phba->sli4_hba.u.if_type2.ERR2regaddr =
			phba->sli4_hba.conf_regs_memmap_p +
						LPFC_CTL_PORT_ER2_OFFSET;
		phba->sli4_hba.u.if_type2.CTRLregaddr =
			phba->sli4_hba.conf_regs_memmap_p +
						LPFC_CTL_PORT_CTL_OFFSET;
		phba->sli4_hba.u.if_type2.STATUSregaddr =
			phba->sli4_hba.conf_regs_memmap_p +
						LPFC_CTL_PORT_STA_OFFSET;
		phba->sli4_hba.SLIINTFregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_SLI_INTF;
		phba->sli4_hba.PSMPHRregaddr =
			phba->sli4_hba.conf_regs_memmap_p +
						LPFC_CTL_PORT_SEM_OFFSET;
		phba->sli4_hba.RQDBregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_RQ_DOORBELL;
		phba->sli4_hba.WQDBregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_WQ_DOORBELL;
		phba->sli4_hba.EQCQDBregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_EQCQ_DOORBELL;
		phba->sli4_hba.MQDBregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_MQ_DOORBELL;
		phba->sli4_hba.BMBXregaddr =
			phba->sli4_hba.conf_regs_memmap_p + LPFC_BMBX;
		break;
	case LPFC_SLI_INTF_IF_TYPE_1:
	default:
		dev_printk(KERN_ERR, &phba->pcidev->dev,
			   "FATAL - unsupported SLI4 interface type - %d\n",
			   if_type);
		break;
	}
}

static void
lpfc_sli4_bar1_register_memmap(struct lpfc_hba *phba)
{
	phba->sli4_hba.PSMPHRregaddr = phba->sli4_hba.ctrl_regs_memmap_p +
		LPFC_SLIPORT_IF0_SMPHR;
	phba->sli4_hba.ISRregaddr = phba->sli4_hba.ctrl_regs_memmap_p +
		LPFC_HST_ISR0;
	phba->sli4_hba.IMRregaddr = phba->sli4_hba.ctrl_regs_memmap_p +
		LPFC_HST_IMR0;
	phba->sli4_hba.ISCRregaddr = phba->sli4_hba.ctrl_regs_memmap_p +
		LPFC_HST_ISCR0;
}

static int
lpfc_sli4_bar2_register_memmap(struct lpfc_hba *phba, uint32_t vf)
{
	if (vf > LPFC_VIR_FUNC_MAX)
		return -ENODEV;

	phba->sli4_hba.RQDBregaddr = (phba->sli4_hba.drbl_regs_memmap_p +
				vf * LPFC_VFR_PAGE_SIZE + LPFC_RQ_DOORBELL);
	phba->sli4_hba.WQDBregaddr = (phba->sli4_hba.drbl_regs_memmap_p +
				vf * LPFC_VFR_PAGE_SIZE + LPFC_WQ_DOORBELL);
	phba->sli4_hba.EQCQDBregaddr = (phba->sli4_hba.drbl_regs_memmap_p +
				vf * LPFC_VFR_PAGE_SIZE + LPFC_EQCQ_DOORBELL);
	phba->sli4_hba.MQDBregaddr = (phba->sli4_hba.drbl_regs_memmap_p +
				vf * LPFC_VFR_PAGE_SIZE + LPFC_MQ_DOORBELL);
	phba->sli4_hba.BMBXregaddr = (phba->sli4_hba.drbl_regs_memmap_p +
				vf * LPFC_VFR_PAGE_SIZE + LPFC_BMBX);
	return 0;
}

static int
lpfc_create_bootstrap_mbox(struct lpfc_hba *phba)
{
	uint32_t bmbx_size;
	struct lpfc_dmabuf *dmabuf;
	struct dma_address *dma_address;
	uint32_t pa_addr;
	uint64_t phys_addr;

	dmabuf = kzalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (!dmabuf)
		return -ENOMEM;

	bmbx_size = sizeof(struct lpfc_bmbx_create) + (LPFC_ALIGN_16_BYTE - 1);
	dmabuf->virt = dma_alloc_coherent(&phba->pcidev->dev,
					  bmbx_size,
					  &dmabuf->phys,
					  GFP_KERNEL);
	if (!dmabuf->virt) {
		kfree(dmabuf);
		return -ENOMEM;
	}
	memset(dmabuf->virt, 0, bmbx_size);

	phba->sli4_hba.bmbx.dmabuf = dmabuf;
	phba->sli4_hba.bmbx.bmbx_size = bmbx_size;

	phba->sli4_hba.bmbx.avirt = PTR_ALIGN(dmabuf->virt,
					      LPFC_ALIGN_16_BYTE);
	phba->sli4_hba.bmbx.aphys = ALIGN(dmabuf->phys,
					      LPFC_ALIGN_16_BYTE);

	dma_address = &phba->sli4_hba.bmbx.dma_address;
	phys_addr = (uint64_t)phba->sli4_hba.bmbx.aphys;
	pa_addr = (uint32_t) ((phys_addr >> 34) & 0x3fffffff);
	dma_address->addr_hi = (uint32_t) ((pa_addr << 2) |
					   LPFC_BMBX_BIT1_ADDR_HI);

	pa_addr = (uint32_t) ((phba->sli4_hba.bmbx.aphys >> 4) & 0x3fffffff);
	dma_address->addr_lo = (uint32_t) ((pa_addr << 2) |
					   LPFC_BMBX_BIT1_ADDR_LO);
	return 0;
}

static void
lpfc_destroy_bootstrap_mbox(struct lpfc_hba *phba)
{
	dma_free_coherent(&phba->pcidev->dev,
			  phba->sli4_hba.bmbx.bmbx_size,
			  phba->sli4_hba.bmbx.dmabuf->virt,
			  phba->sli4_hba.bmbx.dmabuf->phys);

	kfree(phba->sli4_hba.bmbx.dmabuf);
	memset(&phba->sli4_hba.bmbx, 0, sizeof(struct lpfc_bmbx));
}

int
lpfc_sli4_read_config(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *pmb;
	struct lpfc_mbx_read_config *rd_config;
	union  lpfc_sli4_cfg_shdr *shdr;
	uint32_t shdr_status, shdr_add_status;
	struct lpfc_mbx_get_func_cfg *get_func_cfg;
	struct lpfc_rsrc_desc_fcfcoe *desc;
	uint32_t desc_count;
	int length, i, rc = 0;

	pmb = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2011 Unable to allocate memory for issuing "
				"SLI_CONFIG_SPECIAL mailbox command\n");
		return -ENOMEM;
	}

	lpfc_read_config(phba, pmb);

	rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
	if (rc != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"2012 Mailbox failed , mbxCmd x%x "
			"READ_CONFIG, mbxStatus x%x\n",
			bf_get(lpfc_mqe_command, &pmb->u.mqe),
			bf_get(lpfc_mqe_status, &pmb->u.mqe));
		rc = -EIO;
	} else {
		rd_config = &pmb->u.mqe.un.rd_config;
		if (bf_get(lpfc_mbx_rd_conf_lnk_ldv, rd_config)) {
			phba->sli4_hba.lnk_info.lnk_dv = LPFC_LNK_DAT_VAL;
			phba->sli4_hba.lnk_info.lnk_tp =
				bf_get(lpfc_mbx_rd_conf_lnk_type, rd_config);
			phba->sli4_hba.lnk_info.lnk_no =
				bf_get(lpfc_mbx_rd_conf_lnk_numb, rd_config);
			lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
					"3081 lnk_type:%d, lnk_numb:%d\n",
					phba->sli4_hba.lnk_info.lnk_tp,
					phba->sli4_hba.lnk_info.lnk_no);
		} else
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					"3082 Mailbox (x%x) returned ldv:x0\n",
					bf_get(lpfc_mqe_command, &pmb->u.mqe));
		phba->sli4_hba.extents_in_use =
			bf_get(lpfc_mbx_rd_conf_extnts_inuse, rd_config);
		phba->sli4_hba.max_cfg_param.max_xri =
			bf_get(lpfc_mbx_rd_conf_xri_count, rd_config);
		phba->sli4_hba.max_cfg_param.xri_base =
			bf_get(lpfc_mbx_rd_conf_xri_base, rd_config);
		phba->sli4_hba.max_cfg_param.max_vpi =
			bf_get(lpfc_mbx_rd_conf_vpi_count, rd_config);
		phba->sli4_hba.max_cfg_param.vpi_base =
			bf_get(lpfc_mbx_rd_conf_vpi_base, rd_config);
		phba->sli4_hba.max_cfg_param.max_rpi =
			bf_get(lpfc_mbx_rd_conf_rpi_count, rd_config);
		phba->sli4_hba.max_cfg_param.rpi_base =
			bf_get(lpfc_mbx_rd_conf_rpi_base, rd_config);
		phba->sli4_hba.max_cfg_param.max_vfi =
			bf_get(lpfc_mbx_rd_conf_vfi_count, rd_config);
		phba->sli4_hba.max_cfg_param.vfi_base =
			bf_get(lpfc_mbx_rd_conf_vfi_base, rd_config);
		phba->sli4_hba.max_cfg_param.max_fcfi =
			bf_get(lpfc_mbx_rd_conf_fcfi_count, rd_config);
		phba->sli4_hba.max_cfg_param.max_eq =
			bf_get(lpfc_mbx_rd_conf_eq_count, rd_config);
		phba->sli4_hba.max_cfg_param.max_rq =
			bf_get(lpfc_mbx_rd_conf_rq_count, rd_config);
		phba->sli4_hba.max_cfg_param.max_wq =
			bf_get(lpfc_mbx_rd_conf_wq_count, rd_config);
		phba->sli4_hba.max_cfg_param.max_cq =
			bf_get(lpfc_mbx_rd_conf_cq_count, rd_config);
		phba->lmt = bf_get(lpfc_mbx_rd_conf_lmt, rd_config);
		phba->sli4_hba.next_xri = phba->sli4_hba.max_cfg_param.xri_base;
		phba->vpi_base = phba->sli4_hba.max_cfg_param.vpi_base;
		phba->vfi_base = phba->sli4_hba.max_cfg_param.vfi_base;
		phba->max_vpi = (phba->sli4_hba.max_cfg_param.max_vpi > 0) ?
				(phba->sli4_hba.max_cfg_param.max_vpi - 1) : 0;
		phba->max_vports = phba->max_vpi;
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"2003 cfg params Extents? %d "
				"XRI(B:%d M:%d), "
				"VPI(B:%d M:%d) "
				"VFI(B:%d M:%d) "
				"RPI(B:%d M:%d) "
				"FCFI(Count:%d)\n",
				phba->sli4_hba.extents_in_use,
				phba->sli4_hba.max_cfg_param.xri_base,
				phba->sli4_hba.max_cfg_param.max_xri,
				phba->sli4_hba.max_cfg_param.vpi_base,
				phba->sli4_hba.max_cfg_param.max_vpi,
				phba->sli4_hba.max_cfg_param.vfi_base,
				phba->sli4_hba.max_cfg_param.max_vfi,
				phba->sli4_hba.max_cfg_param.rpi_base,
				phba->sli4_hba.max_cfg_param.max_rpi,
				phba->sli4_hba.max_cfg_param.max_fcfi);
	}

	if (rc)
		goto read_cfg_out;

	
	if (phba->cfg_hba_queue_depth >
		(phba->sli4_hba.max_cfg_param.max_xri -
			lpfc_sli4_get_els_iocb_cnt(phba)))
		phba->cfg_hba_queue_depth =
			phba->sli4_hba.max_cfg_param.max_xri -
				lpfc_sli4_get_els_iocb_cnt(phba);

	if (bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) !=
	    LPFC_SLI_INTF_IF_TYPE_2)
		goto read_cfg_out;

	
	length = (sizeof(struct lpfc_mbx_get_func_cfg) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, pmb, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_GET_FUNCTION_CONFIG,
			 length, LPFC_SLI4_MBX_EMBED);

	rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
	shdr = (union lpfc_sli4_cfg_shdr *)
				&pmb->u.mqe.un.sli4_config.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (rc || shdr_status || shdr_add_status) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"3026 Mailbox failed , mbxCmd x%x "
				"GET_FUNCTION_CONFIG, mbxStatus x%x\n",
				bf_get(lpfc_mqe_command, &pmb->u.mqe),
				bf_get(lpfc_mqe_status, &pmb->u.mqe));
		rc = -EIO;
		goto read_cfg_out;
	}

	
	get_func_cfg = &pmb->u.mqe.un.get_func_cfg;
	desc_count = get_func_cfg->func_cfg.rsrc_desc_count;

	for (i = 0; i < LPFC_RSRC_DESC_MAX_NUM; i++) {
		desc = (struct lpfc_rsrc_desc_fcfcoe *)
			&get_func_cfg->func_cfg.desc[i];
		if (LPFC_RSRC_DESC_TYPE_FCFCOE ==
		    bf_get(lpfc_rsrc_desc_pcie_type, desc)) {
			phba->sli4_hba.iov.pf_number =
				bf_get(lpfc_rsrc_desc_fcfcoe_pfnum, desc);
			phba->sli4_hba.iov.vf_number =
				bf_get(lpfc_rsrc_desc_fcfcoe_vfnum, desc);
			break;
		}
	}

	if (i < LPFC_RSRC_DESC_MAX_NUM)
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"3027 GET_FUNCTION_CONFIG: pf_number:%d, "
				"vf_number:%d\n", phba->sli4_hba.iov.pf_number,
				phba->sli4_hba.iov.vf_number);
	else {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"3028 GET_FUNCTION_CONFIG: failed to find "
				"Resrouce Descriptor:x%x\n",
				LPFC_RSRC_DESC_TYPE_FCFCOE);
		rc = -EIO;
	}

read_cfg_out:
	mempool_free(pmb, phba->mbox_mem_pool);
	return rc;
}

static int
lpfc_setup_endian_order(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mboxq;
	uint32_t if_type, rc = 0;
	uint32_t endian_mb_data[2] = {HOST_ENDIAN_LOW_WORD0,
				      HOST_ENDIAN_HIGH_WORD1};

	if_type = bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf);
	switch (if_type) {
	case LPFC_SLI_INTF_IF_TYPE_0:
		mboxq = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool,
						       GFP_KERNEL);
		if (!mboxq) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0492 Unable to allocate memory for "
					"issuing SLI_CONFIG_SPECIAL mailbox "
					"command\n");
			return -ENOMEM;
		}

		memset(mboxq, 0, sizeof(LPFC_MBOXQ_t));
		memcpy(&mboxq->u.mqe, &endian_mb_data, sizeof(endian_mb_data));
		rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0493 SLI_CONFIG_SPECIAL mailbox "
					"failed with status x%x\n",
					rc);
			rc = -EIO;
		}
		mempool_free(mboxq, phba->mbox_mem_pool);
		break;
	case LPFC_SLI_INTF_IF_TYPE_2:
	case LPFC_SLI_INTF_IF_TYPE_1:
	default:
		break;
	}
	return rc;
}

static int
lpfc_sli4_queue_verify(struct lpfc_hba *phba)
{
	int cfg_fcp_wq_count;
	int cfg_fcp_eq_count;


	
	cfg_fcp_wq_count = phba->cfg_fcp_wq_count;
	if (cfg_fcp_wq_count >
	    (phba->sli4_hba.max_cfg_param.max_wq - LPFC_SP_WQN_DEF)) {
		cfg_fcp_wq_count = phba->sli4_hba.max_cfg_param.max_wq -
				   LPFC_SP_WQN_DEF;
		if (cfg_fcp_wq_count < LPFC_FP_WQN_MIN) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"2581 Not enough WQs (%d) from "
					"the pci function for supporting "
					"FCP WQs (%d)\n",
					phba->sli4_hba.max_cfg_param.max_wq,
					phba->cfg_fcp_wq_count);
			goto out_error;
		}
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"2582 Not enough WQs (%d) from the pci "
				"function for supporting the requested "
				"FCP WQs (%d), the actual FCP WQs can "
				"be supported: %d\n",
				phba->sli4_hba.max_cfg_param.max_wq,
				phba->cfg_fcp_wq_count, cfg_fcp_wq_count);
	}
	
	phba->cfg_fcp_wq_count = cfg_fcp_wq_count;

	
	cfg_fcp_eq_count = phba->cfg_fcp_eq_count;
	if (cfg_fcp_eq_count >
	    (phba->sli4_hba.max_cfg_param.max_eq - LPFC_SP_EQN_DEF)) {
		cfg_fcp_eq_count = phba->sli4_hba.max_cfg_param.max_eq -
				   LPFC_SP_EQN_DEF;
		if (cfg_fcp_eq_count < LPFC_FP_EQN_MIN) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"2574 Not enough EQs (%d) from the "
					"pci function for supporting FCP "
					"EQs (%d)\n",
					phba->sli4_hba.max_cfg_param.max_eq,
					phba->cfg_fcp_eq_count);
			goto out_error;
		}
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"2575 Not enough EQs (%d) from the pci "
				"function for supporting the requested "
				"FCP EQs (%d), the actual FCP EQs can "
				"be supported: %d\n",
				phba->sli4_hba.max_cfg_param.max_eq,
				phba->cfg_fcp_eq_count, cfg_fcp_eq_count);
	}
	
	if (cfg_fcp_eq_count > phba->cfg_fcp_wq_count) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"2593 The FCP EQ count(%d) cannot be greater "
				"than the FCP WQ count(%d), limiting the "
				"FCP EQ count to %d\n", cfg_fcp_eq_count,
				phba->cfg_fcp_wq_count,
				phba->cfg_fcp_wq_count);
		cfg_fcp_eq_count = phba->cfg_fcp_wq_count;
	}
	
	phba->cfg_fcp_eq_count = cfg_fcp_eq_count;
	
	phba->sli4_hba.cfg_eqn = phba->cfg_fcp_eq_count + LPFC_SP_EQN_DEF;

	
	phba->sli4_hba.eq_esize = LPFC_EQE_SIZE_4B;
	phba->sli4_hba.eq_ecount = LPFC_EQE_DEF_COUNT;

	
	phba->sli4_hba.cq_esize = LPFC_CQE_SIZE;
	phba->sli4_hba.cq_ecount = LPFC_CQE_DEF_COUNT;

	return 0;
out_error:
	return -ENOMEM;
}

int
lpfc_sli4_queue_create(struct lpfc_hba *phba)
{
	struct lpfc_queue *qdesc;
	int fcp_eqidx, fcp_cqidx, fcp_wqidx;


	
	qdesc = lpfc_sli4_queue_alloc(phba, phba->sli4_hba.eq_esize,
				      phba->sli4_hba.eq_ecount);
	if (!qdesc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0496 Failed allocate slow-path EQ\n");
		goto out_error;
	}
	phba->sli4_hba.sp_eq = qdesc;

	if (phba->cfg_fcp_eq_count) {
		phba->sli4_hba.fp_eq = kzalloc((sizeof(struct lpfc_queue *) *
				       phba->cfg_fcp_eq_count), GFP_KERNEL);
		if (!phba->sli4_hba.fp_eq) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"2576 Failed allocate memory for "
					"fast-path EQ record array\n");
			goto out_free_sp_eq;
		}
	}
	for (fcp_eqidx = 0; fcp_eqidx < phba->cfg_fcp_eq_count; fcp_eqidx++) {
		qdesc = lpfc_sli4_queue_alloc(phba, phba->sli4_hba.eq_esize,
					      phba->sli4_hba.eq_ecount);
		if (!qdesc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0497 Failed allocate fast-path EQ\n");
			goto out_free_fp_eq;
		}
		phba->sli4_hba.fp_eq[fcp_eqidx] = qdesc;
	}


	
	qdesc = lpfc_sli4_queue_alloc(phba, phba->sli4_hba.cq_esize,
				      phba->sli4_hba.cq_ecount);
	if (!qdesc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0500 Failed allocate slow-path mailbox CQ\n");
		goto out_free_fp_eq;
	}
	phba->sli4_hba.mbx_cq = qdesc;

	
	qdesc = lpfc_sli4_queue_alloc(phba, phba->sli4_hba.cq_esize,
				      phba->sli4_hba.cq_ecount);
	if (!qdesc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0501 Failed allocate slow-path ELS CQ\n");
		goto out_free_mbx_cq;
	}
	phba->sli4_hba.els_cq = qdesc;


	if (phba->cfg_fcp_eq_count)
		phba->sli4_hba.fcp_cq = kzalloc((sizeof(struct lpfc_queue *) *
						 phba->cfg_fcp_eq_count),
						GFP_KERNEL);
	else
		phba->sli4_hba.fcp_cq = kzalloc(sizeof(struct lpfc_queue *),
						GFP_KERNEL);
	if (!phba->sli4_hba.fcp_cq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2577 Failed allocate memory for fast-path "
				"CQ record array\n");
		goto out_free_els_cq;
	}
	fcp_cqidx = 0;
	do {
		qdesc = lpfc_sli4_queue_alloc(phba, phba->sli4_hba.cq_esize,
					      phba->sli4_hba.cq_ecount);
		if (!qdesc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0499 Failed allocate fast-path FCP "
					"CQ (%d)\n", fcp_cqidx);
			goto out_free_fcp_cq;
		}
		phba->sli4_hba.fcp_cq[fcp_cqidx] = qdesc;
	} while (++fcp_cqidx < phba->cfg_fcp_eq_count);

	
	phba->sli4_hba.mq_esize = LPFC_MQE_SIZE;
	phba->sli4_hba.mq_ecount = LPFC_MQE_DEF_COUNT;

	qdesc = lpfc_sli4_queue_alloc(phba, phba->sli4_hba.mq_esize,
				      phba->sli4_hba.mq_ecount);
	if (!qdesc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0505 Failed allocate slow-path MQ\n");
		goto out_free_fcp_cq;
	}
	phba->sli4_hba.mbx_wq = qdesc;

	phba->sli4_hba.wq_esize = LPFC_WQE_SIZE;
	phba->sli4_hba.wq_ecount = LPFC_WQE_DEF_COUNT;

	
	qdesc = lpfc_sli4_queue_alloc(phba, phba->sli4_hba.wq_esize,
				      phba->sli4_hba.wq_ecount);
	if (!qdesc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0504 Failed allocate slow-path ELS WQ\n");
		goto out_free_mbx_wq;
	}
	phba->sli4_hba.els_wq = qdesc;

	
	phba->sli4_hba.fcp_wq = kzalloc((sizeof(struct lpfc_queue *) *
				phba->cfg_fcp_wq_count), GFP_KERNEL);
	if (!phba->sli4_hba.fcp_wq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2578 Failed allocate memory for fast-path "
				"WQ record array\n");
		goto out_free_els_wq;
	}
	for (fcp_wqidx = 0; fcp_wqidx < phba->cfg_fcp_wq_count; fcp_wqidx++) {
		qdesc = lpfc_sli4_queue_alloc(phba, phba->sli4_hba.wq_esize,
					      phba->sli4_hba.wq_ecount);
		if (!qdesc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0503 Failed allocate fast-path FCP "
					"WQ (%d)\n", fcp_wqidx);
			goto out_free_fcp_wq;
		}
		phba->sli4_hba.fcp_wq[fcp_wqidx] = qdesc;
	}

	phba->sli4_hba.rq_esize = LPFC_RQE_SIZE;
	phba->sli4_hba.rq_ecount = LPFC_RQE_DEF_COUNT;

	
	qdesc = lpfc_sli4_queue_alloc(phba, phba->sli4_hba.rq_esize,
				      phba->sli4_hba.rq_ecount);
	if (!qdesc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0506 Failed allocate receive HRQ\n");
		goto out_free_fcp_wq;
	}
	phba->sli4_hba.hdr_rq = qdesc;

	
	qdesc = lpfc_sli4_queue_alloc(phba, phba->sli4_hba.rq_esize,
				      phba->sli4_hba.rq_ecount);
	if (!qdesc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0507 Failed allocate receive DRQ\n");
		goto out_free_hdr_rq;
	}
	phba->sli4_hba.dat_rq = qdesc;

	return 0;

out_free_hdr_rq:
	lpfc_sli4_queue_free(phba->sli4_hba.hdr_rq);
	phba->sli4_hba.hdr_rq = NULL;
out_free_fcp_wq:
	for (--fcp_wqidx; fcp_wqidx >= 0; fcp_wqidx--) {
		lpfc_sli4_queue_free(phba->sli4_hba.fcp_wq[fcp_wqidx]);
		phba->sli4_hba.fcp_wq[fcp_wqidx] = NULL;
	}
	kfree(phba->sli4_hba.fcp_wq);
	phba->sli4_hba.fcp_wq = NULL;
out_free_els_wq:
	lpfc_sli4_queue_free(phba->sli4_hba.els_wq);
	phba->sli4_hba.els_wq = NULL;
out_free_mbx_wq:
	lpfc_sli4_queue_free(phba->sli4_hba.mbx_wq);
	phba->sli4_hba.mbx_wq = NULL;
out_free_fcp_cq:
	for (--fcp_cqidx; fcp_cqidx >= 0; fcp_cqidx--) {
		lpfc_sli4_queue_free(phba->sli4_hba.fcp_cq[fcp_cqidx]);
		phba->sli4_hba.fcp_cq[fcp_cqidx] = NULL;
	}
	kfree(phba->sli4_hba.fcp_cq);
	phba->sli4_hba.fcp_cq = NULL;
out_free_els_cq:
	lpfc_sli4_queue_free(phba->sli4_hba.els_cq);
	phba->sli4_hba.els_cq = NULL;
out_free_mbx_cq:
	lpfc_sli4_queue_free(phba->sli4_hba.mbx_cq);
	phba->sli4_hba.mbx_cq = NULL;
out_free_fp_eq:
	for (--fcp_eqidx; fcp_eqidx >= 0; fcp_eqidx--) {
		lpfc_sli4_queue_free(phba->sli4_hba.fp_eq[fcp_eqidx]);
		phba->sli4_hba.fp_eq[fcp_eqidx] = NULL;
	}
	kfree(phba->sli4_hba.fp_eq);
	phba->sli4_hba.fp_eq = NULL;
out_free_sp_eq:
	lpfc_sli4_queue_free(phba->sli4_hba.sp_eq);
	phba->sli4_hba.sp_eq = NULL;
out_error:
	return -ENOMEM;
}

void
lpfc_sli4_queue_destroy(struct lpfc_hba *phba)
{
	int fcp_qidx;

	
	lpfc_sli4_queue_free(phba->sli4_hba.mbx_wq);
	phba->sli4_hba.mbx_wq = NULL;

	
	lpfc_sli4_queue_free(phba->sli4_hba.els_wq);
	phba->sli4_hba.els_wq = NULL;

	
	if (phba->sli4_hba.fcp_wq != NULL)
		for (fcp_qidx = 0; fcp_qidx < phba->cfg_fcp_wq_count;
		     fcp_qidx++)
			lpfc_sli4_queue_free(phba->sli4_hba.fcp_wq[fcp_qidx]);
	kfree(phba->sli4_hba.fcp_wq);
	phba->sli4_hba.fcp_wq = NULL;

	
	lpfc_sli4_queue_free(phba->sli4_hba.hdr_rq);
	phba->sli4_hba.hdr_rq = NULL;
	lpfc_sli4_queue_free(phba->sli4_hba.dat_rq);
	phba->sli4_hba.dat_rq = NULL;

	
	lpfc_sli4_queue_free(phba->sli4_hba.els_cq);
	phba->sli4_hba.els_cq = NULL;

	
	lpfc_sli4_queue_free(phba->sli4_hba.mbx_cq);
	phba->sli4_hba.mbx_cq = NULL;

	
	fcp_qidx = 0;
	if (phba->sli4_hba.fcp_cq != NULL)
		do
			lpfc_sli4_queue_free(phba->sli4_hba.fcp_cq[fcp_qidx]);
		while (++fcp_qidx < phba->cfg_fcp_eq_count);
	kfree(phba->sli4_hba.fcp_cq);
	phba->sli4_hba.fcp_cq = NULL;

	
	if (phba->sli4_hba.fp_eq != NULL)
		for (fcp_qidx = 0; fcp_qidx < phba->cfg_fcp_eq_count;
		     fcp_qidx++)
			lpfc_sli4_queue_free(phba->sli4_hba.fp_eq[fcp_qidx]);
	kfree(phba->sli4_hba.fp_eq);
	phba->sli4_hba.fp_eq = NULL;

	
	lpfc_sli4_queue_free(phba->sli4_hba.sp_eq);
	phba->sli4_hba.sp_eq = NULL;

	return;
}

int
lpfc_sli4_queue_setup(struct lpfc_hba *phba)
{
	int rc = -ENOMEM;
	int fcp_eqidx, fcp_cqidx, fcp_wqidx;
	int fcp_cq_index = 0;


	
	if (!phba->sli4_hba.sp_eq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0520 Slow-path EQ not allocated\n");
		goto out_error;
	}
	rc = lpfc_eq_create(phba, phba->sli4_hba.sp_eq,
			    LPFC_SP_DEF_IMAX);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0521 Failed setup of slow-path EQ: "
				"rc = 0x%x\n", rc);
		goto out_error;
	}
	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"2583 Slow-path EQ setup: queue-id=%d\n",
			phba->sli4_hba.sp_eq->queue_id);

	
	if (phba->cfg_fcp_eq_count && !phba->sli4_hba.fp_eq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"3147 Fast-path EQs not allocated\n");
		rc = -ENOMEM;
		goto out_destroy_sp_eq;
	}
	for (fcp_eqidx = 0; fcp_eqidx < phba->cfg_fcp_eq_count; fcp_eqidx++) {
		if (!phba->sli4_hba.fp_eq[fcp_eqidx]) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0522 Fast-path EQ (%d) not "
					"allocated\n", fcp_eqidx);
			rc = -ENOMEM;
			goto out_destroy_fp_eq;
		}
		rc = lpfc_eq_create(phba, phba->sli4_hba.fp_eq[fcp_eqidx],
				    phba->cfg_fcp_imax);
		if (rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0523 Failed setup of fast-path EQ "
					"(%d), rc = 0x%x\n", fcp_eqidx, rc);
			goto out_destroy_fp_eq;
		}
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"2584 Fast-path EQ setup: "
				"queue[%d]-id=%d\n", fcp_eqidx,
				phba->sli4_hba.fp_eq[fcp_eqidx]->queue_id);
	}


	
	if (!phba->sli4_hba.mbx_cq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0528 Mailbox CQ not allocated\n");
		rc = -ENOMEM;
		goto out_destroy_fp_eq;
	}
	rc = lpfc_cq_create(phba, phba->sli4_hba.mbx_cq, phba->sli4_hba.sp_eq,
			    LPFC_MCQ, LPFC_MBOX);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0529 Failed setup of slow-path mailbox CQ: "
				"rc = 0x%x\n", rc);
		goto out_destroy_fp_eq;
	}
	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"2585 MBX CQ setup: cq-id=%d, parent eq-id=%d\n",
			phba->sli4_hba.mbx_cq->queue_id,
			phba->sli4_hba.sp_eq->queue_id);

	
	if (!phba->sli4_hba.els_cq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0530 ELS CQ not allocated\n");
		rc = -ENOMEM;
		goto out_destroy_mbx_cq;
	}
	rc = lpfc_cq_create(phba, phba->sli4_hba.els_cq, phba->sli4_hba.sp_eq,
			    LPFC_WCQ, LPFC_ELS);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0531 Failed setup of slow-path ELS CQ: "
				"rc = 0x%x\n", rc);
		goto out_destroy_mbx_cq;
	}
	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"2586 ELS CQ setup: cq-id=%d, parent eq-id=%d\n",
			phba->sli4_hba.els_cq->queue_id,
			phba->sli4_hba.sp_eq->queue_id);

	
	if (!phba->sli4_hba.fcp_cq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"3148 Fast-path FCP CQ array not "
				"allocated\n");
		rc = -ENOMEM;
		goto out_destroy_els_cq;
	}
	fcp_cqidx = 0;
	do {
		if (!phba->sli4_hba.fcp_cq[fcp_cqidx]) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0526 Fast-path FCP CQ (%d) not "
					"allocated\n", fcp_cqidx);
			rc = -ENOMEM;
			goto out_destroy_fcp_cq;
		}
		if (phba->cfg_fcp_eq_count)
			rc = lpfc_cq_create(phba,
					    phba->sli4_hba.fcp_cq[fcp_cqidx],
					    phba->sli4_hba.fp_eq[fcp_cqidx],
					    LPFC_WCQ, LPFC_FCP);
		else
			rc = lpfc_cq_create(phba,
					    phba->sli4_hba.fcp_cq[fcp_cqidx],
					    phba->sli4_hba.sp_eq,
					    LPFC_WCQ, LPFC_FCP);
		if (rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0527 Failed setup of fast-path FCP "
					"CQ (%d), rc = 0x%x\n", fcp_cqidx, rc);
			goto out_destroy_fcp_cq;
		}
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"2588 FCP CQ setup: cq[%d]-id=%d, "
				"parent %seq[%d]-id=%d\n",
				fcp_cqidx,
				phba->sli4_hba.fcp_cq[fcp_cqidx]->queue_id,
				(phba->cfg_fcp_eq_count) ? "" : "sp_",
				fcp_cqidx,
				(phba->cfg_fcp_eq_count) ?
				   phba->sli4_hba.fp_eq[fcp_cqidx]->queue_id :
				   phba->sli4_hba.sp_eq->queue_id);
	} while (++fcp_cqidx < phba->cfg_fcp_eq_count);


	
	if (!phba->sli4_hba.mbx_wq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0538 Slow-path MQ not allocated\n");
		rc = -ENOMEM;
		goto out_destroy_fcp_cq;
	}
	rc = lpfc_mq_create(phba, phba->sli4_hba.mbx_wq,
			    phba->sli4_hba.mbx_cq, LPFC_MBOX);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0539 Failed setup of slow-path MQ: "
				"rc = 0x%x\n", rc);
		goto out_destroy_fcp_cq;
	}
	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"2589 MBX MQ setup: wq-id=%d, parent cq-id=%d\n",
			phba->sli4_hba.mbx_wq->queue_id,
			phba->sli4_hba.mbx_cq->queue_id);

	
	if (!phba->sli4_hba.els_wq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0536 Slow-path ELS WQ not allocated\n");
		rc = -ENOMEM;
		goto out_destroy_mbx_wq;
	}
	rc = lpfc_wq_create(phba, phba->sli4_hba.els_wq,
			    phba->sli4_hba.els_cq, LPFC_ELS);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0537 Failed setup of slow-path ELS WQ: "
				"rc = 0x%x\n", rc);
		goto out_destroy_mbx_wq;
	}
	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"2590 ELS WQ setup: wq-id=%d, parent cq-id=%d\n",
			phba->sli4_hba.els_wq->queue_id,
			phba->sli4_hba.els_cq->queue_id);

	
	if (!phba->sli4_hba.fcp_wq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"3149 Fast-path FCP WQ array not "
				"allocated\n");
		rc = -ENOMEM;
		goto out_destroy_els_wq;
	}
	for (fcp_wqidx = 0; fcp_wqidx < phba->cfg_fcp_wq_count; fcp_wqidx++) {
		if (!phba->sli4_hba.fcp_wq[fcp_wqidx]) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0534 Fast-path FCP WQ (%d) not "
					"allocated\n", fcp_wqidx);
			rc = -ENOMEM;
			goto out_destroy_fcp_wq;
		}
		rc = lpfc_wq_create(phba, phba->sli4_hba.fcp_wq[fcp_wqidx],
				    phba->sli4_hba.fcp_cq[fcp_cq_index],
				    LPFC_FCP);
		if (rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0535 Failed setup of fast-path FCP "
					"WQ (%d), rc = 0x%x\n", fcp_wqidx, rc);
			goto out_destroy_fcp_wq;
		}
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"2591 FCP WQ setup: wq[%d]-id=%d, "
				"parent cq[%d]-id=%d\n",
				fcp_wqidx,
				phba->sli4_hba.fcp_wq[fcp_wqidx]->queue_id,
				fcp_cq_index,
				phba->sli4_hba.fcp_cq[fcp_cq_index]->queue_id);
		
		if (phba->cfg_fcp_eq_count)
			fcp_cq_index = ((fcp_cq_index + 1) %
					phba->cfg_fcp_eq_count);
	}

	if (!phba->sli4_hba.hdr_rq || !phba->sli4_hba.dat_rq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0540 Receive Queue not allocated\n");
		rc = -ENOMEM;
		goto out_destroy_fcp_wq;
	}

	lpfc_rq_adjust_repost(phba, phba->sli4_hba.hdr_rq, LPFC_ELS_HBQ);
	lpfc_rq_adjust_repost(phba, phba->sli4_hba.dat_rq, LPFC_ELS_HBQ);

	rc = lpfc_rq_create(phba, phba->sli4_hba.hdr_rq, phba->sli4_hba.dat_rq,
			    phba->sli4_hba.els_cq, LPFC_USOL);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0541 Failed setup of Receive Queue: "
				"rc = 0x%x\n", rc);
		goto out_destroy_fcp_wq;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"2592 USL RQ setup: hdr-rq-id=%d, dat-rq-id=%d "
			"parent cq-id=%d\n",
			phba->sli4_hba.hdr_rq->queue_id,
			phba->sli4_hba.dat_rq->queue_id,
			phba->sli4_hba.els_cq->queue_id);
	return 0;

out_destroy_fcp_wq:
	for (--fcp_wqidx; fcp_wqidx >= 0; fcp_wqidx--)
		lpfc_wq_destroy(phba, phba->sli4_hba.fcp_wq[fcp_wqidx]);
out_destroy_els_wq:
	lpfc_wq_destroy(phba, phba->sli4_hba.els_wq);
out_destroy_mbx_wq:
	lpfc_mq_destroy(phba, phba->sli4_hba.mbx_wq);
out_destroy_fcp_cq:
	for (--fcp_cqidx; fcp_cqidx >= 0; fcp_cqidx--)
		lpfc_cq_destroy(phba, phba->sli4_hba.fcp_cq[fcp_cqidx]);
out_destroy_els_cq:
	lpfc_cq_destroy(phba, phba->sli4_hba.els_cq);
out_destroy_mbx_cq:
	lpfc_cq_destroy(phba, phba->sli4_hba.mbx_cq);
out_destroy_fp_eq:
	for (--fcp_eqidx; fcp_eqidx >= 0; fcp_eqidx--)
		lpfc_eq_destroy(phba, phba->sli4_hba.fp_eq[fcp_eqidx]);
out_destroy_sp_eq:
	lpfc_eq_destroy(phba, phba->sli4_hba.sp_eq);
out_error:
	return rc;
}

void
lpfc_sli4_queue_unset(struct lpfc_hba *phba)
{
	int fcp_qidx;

	
	lpfc_mq_destroy(phba, phba->sli4_hba.mbx_wq);
	
	lpfc_wq_destroy(phba, phba->sli4_hba.els_wq);
	
	lpfc_rq_destroy(phba, phba->sli4_hba.hdr_rq, phba->sli4_hba.dat_rq);
	
	for (fcp_qidx = 0; fcp_qidx < phba->cfg_fcp_wq_count; fcp_qidx++)
		lpfc_wq_destroy(phba, phba->sli4_hba.fcp_wq[fcp_qidx]);
	
	lpfc_cq_destroy(phba, phba->sli4_hba.mbx_cq);
	
	lpfc_cq_destroy(phba, phba->sli4_hba.els_cq);
	
	if (phba->sli4_hba.fcp_cq) {
		fcp_qidx = 0;
		do {
			lpfc_cq_destroy(phba, phba->sli4_hba.fcp_cq[fcp_qidx]);
		} while (++fcp_qidx < phba->cfg_fcp_eq_count);
	}
	
	if (phba->sli4_hba.fp_eq) {
		for (fcp_qidx = 0; fcp_qidx < phba->cfg_fcp_eq_count;
		     fcp_qidx++)
			lpfc_eq_destroy(phba, phba->sli4_hba.fp_eq[fcp_qidx]);
	}
	
	lpfc_eq_destroy(phba, phba->sli4_hba.sp_eq);
}

static int
lpfc_sli4_cq_event_pool_create(struct lpfc_hba *phba)
{
	struct lpfc_cq_event *cq_event;
	int i;

	for (i = 0; i < (4 * phba->sli4_hba.cq_ecount); i++) {
		cq_event = kmalloc(sizeof(struct lpfc_cq_event), GFP_KERNEL);
		if (!cq_event)
			goto out_pool_create_fail;
		list_add_tail(&cq_event->list,
			      &phba->sli4_hba.sp_cqe_event_pool);
	}
	return 0;

out_pool_create_fail:
	lpfc_sli4_cq_event_pool_destroy(phba);
	return -ENOMEM;
}

static void
lpfc_sli4_cq_event_pool_destroy(struct lpfc_hba *phba)
{
	struct lpfc_cq_event *cq_event, *next_cq_event;

	list_for_each_entry_safe(cq_event, next_cq_event,
				 &phba->sli4_hba.sp_cqe_event_pool, list) {
		list_del(&cq_event->list);
		kfree(cq_event);
	}
}

struct lpfc_cq_event *
__lpfc_sli4_cq_event_alloc(struct lpfc_hba *phba)
{
	struct lpfc_cq_event *cq_event = NULL;

	list_remove_head(&phba->sli4_hba.sp_cqe_event_pool, cq_event,
			 struct lpfc_cq_event, list);
	return cq_event;
}

struct lpfc_cq_event *
lpfc_sli4_cq_event_alloc(struct lpfc_hba *phba)
{
	struct lpfc_cq_event *cq_event;
	unsigned long iflags;

	spin_lock_irqsave(&phba->hbalock, iflags);
	cq_event = __lpfc_sli4_cq_event_alloc(phba);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return cq_event;
}

void
__lpfc_sli4_cq_event_release(struct lpfc_hba *phba,
			     struct lpfc_cq_event *cq_event)
{
	list_add_tail(&cq_event->list, &phba->sli4_hba.sp_cqe_event_pool);
}

void
lpfc_sli4_cq_event_release(struct lpfc_hba *phba,
			   struct lpfc_cq_event *cq_event)
{
	unsigned long iflags;
	spin_lock_irqsave(&phba->hbalock, iflags);
	__lpfc_sli4_cq_event_release(phba, cq_event);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
}

static void
lpfc_sli4_cq_event_release_all(struct lpfc_hba *phba)
{
	LIST_HEAD(cqelist);
	struct lpfc_cq_event *cqe;
	unsigned long iflags;

	
	spin_lock_irqsave(&phba->hbalock, iflags);
	
	list_splice_init(&phba->sli4_hba.sp_fcp_xri_aborted_work_queue,
			 &cqelist);
	
	list_splice_init(&phba->sli4_hba.sp_els_xri_aborted_work_queue,
			 &cqelist);
	
	list_splice_init(&phba->sli4_hba.sp_asynce_work_queue,
			 &cqelist);
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	while (!list_empty(&cqelist)) {
		list_remove_head(&cqelist, cqe, struct lpfc_cq_event, list);
		lpfc_sli4_cq_event_release(phba, cqe);
	}
}

int
lpfc_pci_function_reset(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mboxq;
	uint32_t rc = 0, if_type;
	uint32_t shdr_status, shdr_add_status;
	uint32_t rdy_chk, num_resets = 0, reset_again = 0;
	union lpfc_sli4_cfg_shdr *shdr;
	struct lpfc_register reg_data;
	uint16_t devid;

	if_type = bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf);
	switch (if_type) {
	case LPFC_SLI_INTF_IF_TYPE_0:
		mboxq = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool,
						       GFP_KERNEL);
		if (!mboxq) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0494 Unable to allocate memory for "
					"issuing SLI_FUNCTION_RESET mailbox "
					"command\n");
			return -ENOMEM;
		}

		
		lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_COMMON,
				 LPFC_MBOX_OPCODE_FUNCTION_RESET, 0,
				 LPFC_SLI4_MBX_EMBED);
		rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
		shdr = (union lpfc_sli4_cfg_shdr *)
			&mboxq->u.mqe.un.sli4_config.header.cfg_shdr;
		shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
		shdr_add_status = bf_get(lpfc_mbox_hdr_add_status,
					 &shdr->response);
		if (rc != MBX_TIMEOUT)
			mempool_free(mboxq, phba->mbox_mem_pool);
		if (shdr_status || shdr_add_status || rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0495 SLI_FUNCTION_RESET mailbox "
					"failed with status x%x add_status x%x,"
					" mbx status x%x\n",
					shdr_status, shdr_add_status, rc);
			rc = -ENXIO;
		}
		break;
	case LPFC_SLI_INTF_IF_TYPE_2:
		for (num_resets = 0;
		     num_resets < MAX_IF_TYPE_2_RESETS;
		     num_resets++) {
			reg_data.word0 = 0;
			bf_set(lpfc_sliport_ctrl_end, &reg_data,
			       LPFC_SLIPORT_LITTLE_ENDIAN);
			bf_set(lpfc_sliport_ctrl_ip, &reg_data,
			       LPFC_SLIPORT_INIT_PORT);
			writel(reg_data.word0, phba->sli4_hba.u.if_type2.
			       CTRLregaddr);
			
			pci_read_config_word(phba->pcidev,
					     PCI_DEVICE_ID, &devid);
			for (rdy_chk = 0; rdy_chk < 1000; rdy_chk++) {
				msleep(10);
				if (lpfc_readl(phba->sli4_hba.u.if_type2.
					      STATUSregaddr, &reg_data.word0)) {
					rc = -ENODEV;
					goto out;
				}
				if (bf_get(lpfc_sliport_status_rn, &reg_data))
					reset_again++;
				if (bf_get(lpfc_sliport_status_rdy, &reg_data))
					break;
			}

			if (reset_again && (rdy_chk < 1000)) {
				msleep(10);
				reset_again = 0;
				continue;
			}

			
			if ((bf_get(lpfc_sliport_status_err, &reg_data)) ||
			    (rdy_chk >= 1000)) {
				phba->work_status[0] = readl(
					phba->sli4_hba.u.if_type2.ERR1regaddr);
				phba->work_status[1] = readl(
					phba->sli4_hba.u.if_type2.ERR2regaddr);
				lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"2890 Port error detected during port "
					"reset(%d): port status reg 0x%x, "
					"error 1=0x%x, error 2=0x%x\n",
					num_resets, reg_data.word0,
					phba->work_status[0],
					phba->work_status[1]);
				rc = -ENODEV;
			}

			if (rdy_chk < 1000)
				break;
		}
		
		msleep(100);
		break;
	case LPFC_SLI_INTF_IF_TYPE_1:
	default:
		break;
	}

out:
	
	if (num_resets >= MAX_IF_TYPE_2_RESETS)
		rc = -ENODEV;

	return rc;
}

static int
lpfc_sli4_send_nop_mbox_cmds(struct lpfc_hba *phba, uint32_t cnt)
{
	LPFC_MBOXQ_t *mboxq;
	int length, cmdsent;
	uint32_t mbox_tmo;
	uint32_t rc = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	if (cnt == 0) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"2518 Requested to send 0 NOP mailbox cmd\n");
		return cnt;
	}

	mboxq = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2519 Unable to allocate memory for issuing "
				"NOP mailbox command\n");
		return 0;
	}

	
	length = (sizeof(struct lpfc_mbx_nop) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_NOP, length, LPFC_SLI4_MBX_EMBED);

	for (cmdsent = 0; cmdsent < cnt; cmdsent++) {
		if (!phba->sli4_hba.intr_enable)
			rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
		else {
			mbox_tmo = lpfc_mbox_tmo_val(phba, mboxq);
			rc = lpfc_sli_issue_mbox_wait(phba, mboxq, mbox_tmo);
		}
		if (rc == MBX_TIMEOUT)
			break;
		
		shdr = (union lpfc_sli4_cfg_shdr *)
			&mboxq->u.mqe.un.sli4_config.header.cfg_shdr;
		shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
		shdr_add_status = bf_get(lpfc_mbox_hdr_add_status,
					 &shdr->response);
		if (shdr_status || shdr_add_status || rc) {
			lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
					"2520 NOP mailbox command failed "
					"status x%x add_status x%x mbx "
					"status x%x\n", shdr_status,
					shdr_add_status, rc);
			break;
		}
	}

	if (rc != MBX_TIMEOUT)
		mempool_free(mboxq, phba->mbox_mem_pool);

	return cmdsent;
}

static int
lpfc_sli4_pci_mem_setup(struct lpfc_hba *phba)
{
	struct pci_dev *pdev;
	unsigned long bar0map_len, bar1map_len, bar2map_len;
	int error = -ENODEV;
	uint32_t if_type;

	
	if (!phba->pcidev)
		return error;
	else
		pdev = phba->pcidev;

	
	if (pci_set_dma_mask(pdev, DMA_BIT_MASK(64)) != 0
	 || pci_set_consistent_dma_mask(pdev,DMA_BIT_MASK(64)) != 0) {
		if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32)) != 0
		 || pci_set_consistent_dma_mask(pdev,DMA_BIT_MASK(32)) != 0) {
			return error;
		}
	}

	if (pci_read_config_dword(pdev, LPFC_SLI_INTF,
				  &phba->sli4_hba.sli_intf.word0)) {
		return error;
	}

	
	if (bf_get(lpfc_sli_intf_valid, &phba->sli4_hba.sli_intf) !=
	    LPFC_SLI_INTF_VALID) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2894 SLI_INTF reg contents invalid "
				"sli_intf reg 0x%x\n",
				phba->sli4_hba.sli_intf.word0);
		return error;
	}

	if_type = bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf);
	if (pci_resource_start(pdev, 0)) {
		phba->pci_bar0_map = pci_resource_start(pdev, 0);
		bar0map_len = pci_resource_len(pdev, 0);

		phba->sli4_hba.conf_regs_memmap_p =
			ioremap(phba->pci_bar0_map, bar0map_len);
		if (!phba->sli4_hba.conf_regs_memmap_p) {
			dev_printk(KERN_ERR, &pdev->dev,
				   "ioremap failed for SLI4 PCI config "
				   "registers.\n");
			goto out;
		}
		
		lpfc_sli4_bar0_register_memmap(phba, if_type);
	} else {
		phba->pci_bar0_map = pci_resource_start(pdev, 1);
		bar0map_len = pci_resource_len(pdev, 1);
		if (if_type == LPFC_SLI_INTF_IF_TYPE_2) {
			dev_printk(KERN_ERR, &pdev->dev,
			   "FATAL - No BAR0 mapping for SLI4, if_type 2\n");
			goto out;
		}
		phba->sli4_hba.conf_regs_memmap_p =
				ioremap(phba->pci_bar0_map, bar0map_len);
		if (!phba->sli4_hba.conf_regs_memmap_p) {
			dev_printk(KERN_ERR, &pdev->dev,
				"ioremap failed for SLI4 PCI config "
				"registers.\n");
				goto out;
		}
		lpfc_sli4_bar0_register_memmap(phba, if_type);
	}

	if ((if_type == LPFC_SLI_INTF_IF_TYPE_0) &&
	    (pci_resource_start(pdev, 2))) {
		phba->pci_bar1_map = pci_resource_start(pdev, 2);
		bar1map_len = pci_resource_len(pdev, 2);
		phba->sli4_hba.ctrl_regs_memmap_p =
				ioremap(phba->pci_bar1_map, bar1map_len);
		if (!phba->sli4_hba.ctrl_regs_memmap_p) {
			dev_printk(KERN_ERR, &pdev->dev,
			   "ioremap failed for SLI4 HBA control registers.\n");
			goto out_iounmap_conf;
		}
		lpfc_sli4_bar1_register_memmap(phba);
	}

	if ((if_type == LPFC_SLI_INTF_IF_TYPE_0) &&
	    (pci_resource_start(pdev, 4))) {
		phba->pci_bar2_map = pci_resource_start(pdev, 4);
		bar2map_len = pci_resource_len(pdev, 4);
		phba->sli4_hba.drbl_regs_memmap_p =
				ioremap(phba->pci_bar2_map, bar2map_len);
		if (!phba->sli4_hba.drbl_regs_memmap_p) {
			dev_printk(KERN_ERR, &pdev->dev,
			   "ioremap failed for SLI4 HBA doorbell registers.\n");
			goto out_iounmap_ctrl;
		}
		error = lpfc_sli4_bar2_register_memmap(phba, LPFC_VF0);
		if (error)
			goto out_iounmap_all;
	}

	return 0;

out_iounmap_all:
	iounmap(phba->sli4_hba.drbl_regs_memmap_p);
out_iounmap_ctrl:
	iounmap(phba->sli4_hba.ctrl_regs_memmap_p);
out_iounmap_conf:
	iounmap(phba->sli4_hba.conf_regs_memmap_p);
out:
	return error;
}

static void
lpfc_sli4_pci_mem_unset(struct lpfc_hba *phba)
{
	uint32_t if_type;
	if_type = bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf);

	switch (if_type) {
	case LPFC_SLI_INTF_IF_TYPE_0:
		iounmap(phba->sli4_hba.drbl_regs_memmap_p);
		iounmap(phba->sli4_hba.ctrl_regs_memmap_p);
		iounmap(phba->sli4_hba.conf_regs_memmap_p);
		break;
	case LPFC_SLI_INTF_IF_TYPE_2:
		iounmap(phba->sli4_hba.conf_regs_memmap_p);
		break;
	case LPFC_SLI_INTF_IF_TYPE_1:
	default:
		dev_printk(KERN_ERR, &phba->pcidev->dev,
			   "FATAL - unsupported SLI4 interface type - %d\n",
			   if_type);
		break;
	}
}

static int
lpfc_sli_enable_msix(struct lpfc_hba *phba)
{
	int rc, i;
	LPFC_MBOXQ_t *pmb;

	
	for (i = 0; i < LPFC_MSIX_VECTORS; i++)
		phba->msix_entries[i].entry = i;

	
	rc = pci_enable_msix(phba->pcidev, phba->msix_entries,
				ARRAY_SIZE(phba->msix_entries));
	if (rc) {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0420 PCI enable MSI-X failed (%d)\n", rc);
		goto msi_fail_out;
	}
	for (i = 0; i < LPFC_MSIX_VECTORS; i++)
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0477 MSI-X entry[%d]: vector=x%x "
				"message=%d\n", i,
				phba->msix_entries[i].vector,
				phba->msix_entries[i].entry);

	
	rc = request_irq(phba->msix_entries[0].vector,
			 &lpfc_sli_sp_intr_handler, IRQF_SHARED,
			 LPFC_SP_DRIVER_HANDLER_NAME, phba);
	if (rc) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"0421 MSI-X slow-path request_irq failed "
				"(%d)\n", rc);
		goto msi_fail_out;
	}

	
	rc = request_irq(phba->msix_entries[1].vector,
			 &lpfc_sli_fp_intr_handler, IRQF_SHARED,
			 LPFC_FP_DRIVER_HANDLER_NAME, phba);

	if (rc) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"0429 MSI-X fast-path request_irq failed "
				"(%d)\n", rc);
		goto irq_fail_out;
	}

	pmb = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);

	if (!pmb) {
		rc = -ENOMEM;
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0474 Unable to allocate memory for issuing "
				"MBOX_CONFIG_MSI command\n");
		goto mem_fail_out;
	}
	rc = lpfc_config_msi(phba, pmb);
	if (rc)
		goto mbx_fail_out;
	rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
	if (rc != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX,
				"0351 Config MSI mailbox command failed, "
				"mbxCmd x%x, mbxStatus x%x\n",
				pmb->u.mb.mbxCommand, pmb->u.mb.mbxStatus);
		goto mbx_fail_out;
	}

	
	mempool_free(pmb, phba->mbox_mem_pool);
	return rc;

mbx_fail_out:
	
	mempool_free(pmb, phba->mbox_mem_pool);

mem_fail_out:
	
	free_irq(phba->msix_entries[1].vector, phba);

irq_fail_out:
	
	free_irq(phba->msix_entries[0].vector, phba);

msi_fail_out:
	
	pci_disable_msix(phba->pcidev);
	return rc;
}

static void
lpfc_sli_disable_msix(struct lpfc_hba *phba)
{
	int i;

	
	for (i = 0; i < LPFC_MSIX_VECTORS; i++)
		free_irq(phba->msix_entries[i].vector, phba);
	
	pci_disable_msix(phba->pcidev);

	return;
}

static int
lpfc_sli_enable_msi(struct lpfc_hba *phba)
{
	int rc;

	rc = pci_enable_msi(phba->pcidev);
	if (!rc)
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0462 PCI enable MSI mode success.\n");
	else {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0471 PCI enable MSI mode failed (%d)\n", rc);
		return rc;
	}

	rc = request_irq(phba->pcidev->irq, lpfc_sli_intr_handler,
			 IRQF_SHARED, LPFC_DRIVER_NAME, phba);
	if (rc) {
		pci_disable_msi(phba->pcidev);
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"0478 MSI request_irq failed (%d)\n", rc);
	}
	return rc;
}

static void
lpfc_sli_disable_msi(struct lpfc_hba *phba)
{
	free_irq(phba->pcidev->irq, phba);
	pci_disable_msi(phba->pcidev);
	return;
}

static uint32_t
lpfc_sli_enable_intr(struct lpfc_hba *phba, uint32_t cfg_mode)
{
	uint32_t intr_mode = LPFC_INTR_ERROR;
	int retval;

	if (cfg_mode == 2) {
		
		retval = lpfc_sli_config_port(phba, LPFC_SLI_REV3);
		if (!retval) {
			
			retval = lpfc_sli_enable_msix(phba);
			if (!retval) {
				
				phba->intr_type = MSIX;
				intr_mode = 2;
			}
		}
	}

	
	if (cfg_mode >= 1 && phba->intr_type == NONE) {
		retval = lpfc_sli_enable_msi(phba);
		if (!retval) {
			
			phba->intr_type = MSI;
			intr_mode = 1;
		}
	}

	
	if (phba->intr_type == NONE) {
		retval = request_irq(phba->pcidev->irq, lpfc_sli_intr_handler,
				     IRQF_SHARED, LPFC_DRIVER_NAME, phba);
		if (!retval) {
			
			phba->intr_type = INTx;
			intr_mode = 0;
		}
	}
	return intr_mode;
}

static void
lpfc_sli_disable_intr(struct lpfc_hba *phba)
{
	
	if (phba->intr_type == MSIX)
		lpfc_sli_disable_msix(phba);
	else if (phba->intr_type == MSI)
		lpfc_sli_disable_msi(phba);
	else if (phba->intr_type == INTx)
		free_irq(phba->pcidev->irq, phba);

	
	phba->intr_type = NONE;
	phba->sli.slistat.sli_intr = 0;

	return;
}

static int
lpfc_sli4_enable_msix(struct lpfc_hba *phba)
{
	int vectors, rc, index;

	
	for (index = 0; index < phba->sli4_hba.cfg_eqn; index++)
		phba->sli4_hba.msix_entries[index].entry = index;

	
	vectors = phba->sli4_hba.cfg_eqn;
enable_msix_vectors:
	rc = pci_enable_msix(phba->pcidev, phba->sli4_hba.msix_entries,
			     vectors);
	if (rc > 1) {
		vectors = rc;
		goto enable_msix_vectors;
	} else if (rc) {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0484 PCI enable MSI-X failed (%d)\n", rc);
		goto msi_fail_out;
	}

	
	for (index = 0; index < vectors; index++)
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0489 MSI-X entry[%d]: vector=x%x "
				"message=%d\n", index,
				phba->sli4_hba.msix_entries[index].vector,
				phba->sli4_hba.msix_entries[index].entry);
	if (vectors > 1)
		rc = request_irq(phba->sli4_hba.msix_entries[0].vector,
				 &lpfc_sli4_sp_intr_handler, IRQF_SHARED,
				 LPFC_SP_DRIVER_HANDLER_NAME, phba);
	else
		
		rc = request_irq(phba->sli4_hba.msix_entries[0].vector,
				 &lpfc_sli4_intr_handler, IRQF_SHARED,
				 LPFC_DRIVER_NAME, phba);
	if (rc) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"0485 MSI-X slow-path request_irq failed "
				"(%d)\n", rc);
		goto msi_fail_out;
	}

	
	for (index = 1; index < vectors; index++) {
		phba->sli4_hba.fcp_eq_hdl[index - 1].idx = index - 1;
		phba->sli4_hba.fcp_eq_hdl[index - 1].phba = phba;
		rc = request_irq(phba->sli4_hba.msix_entries[index].vector,
				 &lpfc_sli4_fp_intr_handler, IRQF_SHARED,
				 LPFC_FP_DRIVER_HANDLER_NAME,
				 &phba->sli4_hba.fcp_eq_hdl[index - 1]);
		if (rc) {
			lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
					"0486 MSI-X fast-path (%d) "
					"request_irq failed (%d)\n", index, rc);
			goto cfg_fail_out;
		}
	}
	phba->sli4_hba.msix_vec_nr = vectors;

	return rc;

cfg_fail_out:
	
	for (--index; index >= 1; index--)
		free_irq(phba->sli4_hba.msix_entries[index - 1].vector,
			 &phba->sli4_hba.fcp_eq_hdl[index - 1]);

	
	free_irq(phba->sli4_hba.msix_entries[0].vector, phba);

msi_fail_out:
	
	pci_disable_msix(phba->pcidev);
	return rc;
}

static void
lpfc_sli4_disable_msix(struct lpfc_hba *phba)
{
	int index;

	
	free_irq(phba->sli4_hba.msix_entries[0].vector, phba);

	for (index = 1; index < phba->sli4_hba.msix_vec_nr; index++)
		free_irq(phba->sli4_hba.msix_entries[index].vector,
			 &phba->sli4_hba.fcp_eq_hdl[index - 1]);

	
	pci_disable_msix(phba->pcidev);

	return;
}

static int
lpfc_sli4_enable_msi(struct lpfc_hba *phba)
{
	int rc, index;

	rc = pci_enable_msi(phba->pcidev);
	if (!rc)
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0487 PCI enable MSI mode success.\n");
	else {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0488 PCI enable MSI mode failed (%d)\n", rc);
		return rc;
	}

	rc = request_irq(phba->pcidev->irq, lpfc_sli4_intr_handler,
			 IRQF_SHARED, LPFC_DRIVER_NAME, phba);
	if (rc) {
		pci_disable_msi(phba->pcidev);
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"0490 MSI request_irq failed (%d)\n", rc);
		return rc;
	}

	for (index = 0; index < phba->cfg_fcp_eq_count; index++) {
		phba->sli4_hba.fcp_eq_hdl[index].idx = index;
		phba->sli4_hba.fcp_eq_hdl[index].phba = phba;
	}

	return 0;
}

static void
lpfc_sli4_disable_msi(struct lpfc_hba *phba)
{
	free_irq(phba->pcidev->irq, phba);
	pci_disable_msi(phba->pcidev);
	return;
}

static uint32_t
lpfc_sli4_enable_intr(struct lpfc_hba *phba, uint32_t cfg_mode)
{
	uint32_t intr_mode = LPFC_INTR_ERROR;
	int retval, index;

	if (cfg_mode == 2) {
		
		retval = 0;
		if (!retval) {
			
			retval = lpfc_sli4_enable_msix(phba);
			if (!retval) {
				
				phba->intr_type = MSIX;
				intr_mode = 2;
			}
		}
	}

	
	if (cfg_mode >= 1 && phba->intr_type == NONE) {
		retval = lpfc_sli4_enable_msi(phba);
		if (!retval) {
			
			phba->intr_type = MSI;
			intr_mode = 1;
		}
	}

	
	if (phba->intr_type == NONE) {
		retval = request_irq(phba->pcidev->irq, lpfc_sli4_intr_handler,
				     IRQF_SHARED, LPFC_DRIVER_NAME, phba);
		if (!retval) {
			
			phba->intr_type = INTx;
			intr_mode = 0;
			for (index = 0; index < phba->cfg_fcp_eq_count;
			     index++) {
				phba->sli4_hba.fcp_eq_hdl[index].idx = index;
				phba->sli4_hba.fcp_eq_hdl[index].phba = phba;
			}
		}
	}
	return intr_mode;
}

static void
lpfc_sli4_disable_intr(struct lpfc_hba *phba)
{
	
	if (phba->intr_type == MSIX)
		lpfc_sli4_disable_msix(phba);
	else if (phba->intr_type == MSI)
		lpfc_sli4_disable_msi(phba);
	else if (phba->intr_type == INTx)
		free_irq(phba->pcidev->irq, phba);

	
	phba->intr_type = NONE;
	phba->sli.slistat.sli_intr = 0;

	return;
}

static void
lpfc_unset_hba(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);

	spin_lock_irq(shost->host_lock);
	vport->load_flag |= FC_UNLOADING;
	spin_unlock_irq(shost->host_lock);

	kfree(phba->vpi_bmask);
	kfree(phba->vpi_ids);

	lpfc_stop_hba_timers(phba);

	phba->pport->work_port_events = 0;

	lpfc_sli_hba_down(phba);

	lpfc_sli_brdrestart(phba);

	lpfc_sli_disable_intr(phba);

	return;
}

static void
lpfc_sli4_unset_hba(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);

	spin_lock_irq(shost->host_lock);
	vport->load_flag |= FC_UNLOADING;
	spin_unlock_irq(shost->host_lock);

	phba->pport->work_port_events = 0;

	
	lpfc_stop_port(phba);

	lpfc_sli4_disable_intr(phba);

	
	lpfc_pci_function_reset(phba);
	lpfc_sli4_queue_destroy(phba);

	return;
}

static void
lpfc_sli4_xri_exchange_busy_wait(struct lpfc_hba *phba)
{
	int wait_time = 0;
	int fcp_xri_cmpl = list_empty(&phba->sli4_hba.lpfc_abts_scsi_buf_list);
	int els_xri_cmpl = list_empty(&phba->sli4_hba.lpfc_abts_els_sgl_list);

	while (!fcp_xri_cmpl || !els_xri_cmpl) {
		if (wait_time > LPFC_XRI_EXCH_BUSY_WAIT_TMO) {
			if (!fcp_xri_cmpl)
				lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
						"2877 FCP XRI exchange busy "
						"wait time: %d seconds.\n",
						wait_time/1000);
			if (!els_xri_cmpl)
				lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
						"2878 ELS XRI exchange busy "
						"wait time: %d seconds.\n",
						wait_time/1000);
			msleep(LPFC_XRI_EXCH_BUSY_WAIT_T2);
			wait_time += LPFC_XRI_EXCH_BUSY_WAIT_T2;
		} else {
			msleep(LPFC_XRI_EXCH_BUSY_WAIT_T1);
			wait_time += LPFC_XRI_EXCH_BUSY_WAIT_T1;
		}
		fcp_xri_cmpl =
			list_empty(&phba->sli4_hba.lpfc_abts_scsi_buf_list);
		els_xri_cmpl =
			list_empty(&phba->sli4_hba.lpfc_abts_els_sgl_list);
	}
}

static void
lpfc_sli4_hba_unset(struct lpfc_hba *phba)
{
	int wait_cnt = 0;
	LPFC_MBOXQ_t *mboxq;
	struct pci_dev *pdev = phba->pcidev;

	lpfc_stop_hba_timers(phba);
	phba->sli4_hba.intr_enable = 0;


	
	spin_lock_irq(&phba->hbalock);
	phba->sli.sli_flag |= LPFC_SLI_ASYNC_MBX_BLK;
	spin_unlock_irq(&phba->hbalock);
	
	while (phba->sli.sli_flag & LPFC_SLI_MBOX_ACTIVE) {
		msleep(10);
		if (++wait_cnt > LPFC_ACTIVE_MBOX_WAIT_CNT)
			break;
	}
	
	if (phba->sli.sli_flag & LPFC_SLI_MBOX_ACTIVE) {
		spin_lock_irq(&phba->hbalock);
		mboxq = phba->sli.mbox_active;
		mboxq->u.mb.mbxStatus = MBX_NOT_FINISHED;
		__lpfc_mbox_cmpl_put(phba, mboxq);
		phba->sli.sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
		phba->sli.mbox_active = NULL;
		spin_unlock_irq(&phba->hbalock);
	}

	
	lpfc_sli_hba_iocb_abort(phba);

	
	lpfc_sli4_xri_exchange_busy_wait(phba);

	
	lpfc_sli4_disable_intr(phba);

	
	if (phba->cfg_sriov_nr_virtfn)
		pci_disable_sriov(pdev);

	
	kthread_stop(phba->worker_thread);

	
	lpfc_pci_function_reset(phba);
	lpfc_sli4_queue_destroy(phba);

	
	phba->pport->work_port_events = 0;
}

int
lpfc_pc_sli4_params_get(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	int rc;
	struct lpfc_mqe *mqe;
	struct lpfc_pc_sli4_params *sli4_params;
	uint32_t mbox_tmo;

	rc = 0;
	mqe = &mboxq->u.mqe;

	
	lpfc_pc_sli4_params(mboxq);
	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mboxq);
		rc = lpfc_sli_issue_mbox_wait(phba, mboxq, mbox_tmo);
	}

	if (unlikely(rc))
		return 1;

	sli4_params = &phba->sli4_hba.pc_sli4_params;
	sli4_params->if_type = bf_get(if_type, &mqe->un.sli4_params);
	sli4_params->sli_rev = bf_get(sli_rev, &mqe->un.sli4_params);
	sli4_params->sli_family = bf_get(sli_family, &mqe->un.sli4_params);
	sli4_params->featurelevel_1 = bf_get(featurelevel_1,
					     &mqe->un.sli4_params);
	sli4_params->featurelevel_2 = bf_get(featurelevel_2,
					     &mqe->un.sli4_params);
	sli4_params->proto_types = mqe->un.sli4_params.word3;
	sli4_params->sge_supp_len = mqe->un.sli4_params.sge_supp_len;
	sli4_params->if_page_sz = bf_get(if_page_sz, &mqe->un.sli4_params);
	sli4_params->rq_db_window = bf_get(rq_db_window, &mqe->un.sli4_params);
	sli4_params->loopbk_scope = bf_get(loopbk_scope, &mqe->un.sli4_params);
	sli4_params->eq_pages_max = bf_get(eq_pages, &mqe->un.sli4_params);
	sli4_params->eqe_size = bf_get(eqe_size, &mqe->un.sli4_params);
	sli4_params->cq_pages_max = bf_get(cq_pages, &mqe->un.sli4_params);
	sli4_params->cqe_size = bf_get(cqe_size, &mqe->un.sli4_params);
	sli4_params->mq_pages_max = bf_get(mq_pages, &mqe->un.sli4_params);
	sli4_params->mqe_size = bf_get(mqe_size, &mqe->un.sli4_params);
	sli4_params->mq_elem_cnt = bf_get(mq_elem_cnt, &mqe->un.sli4_params);
	sli4_params->wq_pages_max = bf_get(wq_pages, &mqe->un.sli4_params);
	sli4_params->wqe_size = bf_get(wqe_size, &mqe->un.sli4_params);
	sli4_params->rq_pages_max = bf_get(rq_pages, &mqe->un.sli4_params);
	sli4_params->rqe_size = bf_get(rqe_size, &mqe->un.sli4_params);
	sli4_params->hdr_pages_max = bf_get(hdr_pages, &mqe->un.sli4_params);
	sli4_params->hdr_size = bf_get(hdr_size, &mqe->un.sli4_params);
	sli4_params->hdr_pp_align = bf_get(hdr_pp_align, &mqe->un.sli4_params);
	sli4_params->sgl_pages_max = bf_get(sgl_pages, &mqe->un.sli4_params);
	sli4_params->sgl_pp_align = bf_get(sgl_pp_align, &mqe->un.sli4_params);

	
	if (sli4_params->sge_supp_len > LPFC_MAX_SGE_SIZE)
		sli4_params->sge_supp_len = LPFC_MAX_SGE_SIZE;

	return rc;
}

int
lpfc_get_sli4_parameters(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	int rc;
	struct lpfc_mqe *mqe = &mboxq->u.mqe;
	struct lpfc_pc_sli4_params *sli4_params;
	uint32_t mbox_tmo;
	int length;
	struct lpfc_sli4_parameters *mbx_sli4_parameters;

	phba->sli4_hba.rpi_hdrs_in_use = 1;

	
	length = (sizeof(struct lpfc_mbx_get_sli4_parameters) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_GET_SLI4_PARAMETERS,
			 length, LPFC_SLI4_MBX_EMBED);
	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mboxq);
		rc = lpfc_sli_issue_mbox_wait(phba, mboxq, mbox_tmo);
	}
	if (unlikely(rc))
		return rc;
	sli4_params = &phba->sli4_hba.pc_sli4_params;
	mbx_sli4_parameters = &mqe->un.get_sli4_parameters.sli4_parameters;
	sli4_params->if_type = bf_get(cfg_if_type, mbx_sli4_parameters);
	sli4_params->sli_rev = bf_get(cfg_sli_rev, mbx_sli4_parameters);
	sli4_params->sli_family = bf_get(cfg_sli_family, mbx_sli4_parameters);
	sli4_params->featurelevel_1 = bf_get(cfg_sli_hint_1,
					     mbx_sli4_parameters);
	sli4_params->featurelevel_2 = bf_get(cfg_sli_hint_2,
					     mbx_sli4_parameters);
	if (bf_get(cfg_phwq, mbx_sli4_parameters))
		phba->sli3_options |= LPFC_SLI4_PHWQ_ENABLED;
	else
		phba->sli3_options &= ~LPFC_SLI4_PHWQ_ENABLED;
	sli4_params->sge_supp_len = mbx_sli4_parameters->sge_supp_len;
	sli4_params->loopbk_scope = bf_get(loopbk_scope, mbx_sli4_parameters);
	sli4_params->cqv = bf_get(cfg_cqv, mbx_sli4_parameters);
	sli4_params->mqv = bf_get(cfg_mqv, mbx_sli4_parameters);
	sli4_params->wqv = bf_get(cfg_wqv, mbx_sli4_parameters);
	sli4_params->rqv = bf_get(cfg_rqv, mbx_sli4_parameters);
	sli4_params->sgl_pages_max = bf_get(cfg_sgl_page_cnt,
					    mbx_sli4_parameters);
	sli4_params->sgl_pp_align = bf_get(cfg_sgl_pp_align,
					   mbx_sli4_parameters);
	phba->sli4_hba.extents_in_use = bf_get(cfg_ext, mbx_sli4_parameters);
	phba->sli4_hba.rpi_hdrs_in_use = bf_get(cfg_hdrr, mbx_sli4_parameters);

	
	if (sli4_params->sge_supp_len > LPFC_MAX_SGE_SIZE)
		sli4_params->sge_supp_len = LPFC_MAX_SGE_SIZE;

	return 0;
}

static int __devinit
lpfc_pci_probe_one_s3(struct pci_dev *pdev, const struct pci_device_id *pid)
{
	struct lpfc_hba   *phba;
	struct lpfc_vport *vport = NULL;
	struct Scsi_Host  *shost = NULL;
	int error;
	uint32_t cfg_mode, intr_mode;

	
	phba = lpfc_hba_alloc(pdev);
	if (!phba)
		return -ENOMEM;

	
	error = lpfc_enable_pci_dev(phba);
	if (error)
		goto out_free_phba;

	
	error = lpfc_api_table_setup(phba, LPFC_PCI_DEV_LP);
	if (error)
		goto out_disable_pci_dev;

	
	error = lpfc_sli_pci_mem_setup(phba);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1402 Failed to set up pci memory space.\n");
		goto out_disable_pci_dev;
	}

	
	error = lpfc_setup_driver_resource_phase1(phba);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1403 Failed to set up driver resource.\n");
		goto out_unset_pci_mem_s3;
	}

	
	error = lpfc_sli_driver_resource_setup(phba);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1404 Failed to set up driver resource.\n");
		goto out_unset_pci_mem_s3;
	}

	
	error = lpfc_init_iocb_list(phba, LPFC_IOCB_LIST_CNT);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1405 Failed to initialize iocb list.\n");
		goto out_unset_driver_resource_s3;
	}

	
	error = lpfc_setup_driver_resource_phase2(phba);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1406 Failed to set up driver resource.\n");
		goto out_free_iocb_list;
	}

	
	lpfc_get_hba_model_desc(phba, phba->ModelName, phba->ModelDesc);

	
	error = lpfc_create_shost(phba);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1407 Failed to create scsi host.\n");
		goto out_unset_driver_resource;
	}

	
	vport = phba->pport;
	error = lpfc_alloc_sysfs_attr(vport);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1476 Failed to allocate sysfs attr\n");
		goto out_destroy_shost;
	}

	shost = lpfc_shost_from_vport(vport); 
	
	cfg_mode = phba->cfg_use_msi;
	while (true) {
		
		lpfc_stop_port(phba);
		
		intr_mode = lpfc_sli_enable_intr(phba, cfg_mode);
		if (intr_mode == LPFC_INTR_ERROR) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0431 Failed to enable interrupt.\n");
			error = -ENODEV;
			goto out_free_sysfs_attr;
		}
		
		if (lpfc_sli_hba_setup(phba)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"1477 Failed to set up hba\n");
			error = -ENODEV;
			goto out_remove_device;
		}

		
		msleep(50);
		
		if (intr_mode == 0 ||
		    phba->sli.slistat.sli_intr > LPFC_MSIX_VECTORS) {
			
			phba->intr_mode = intr_mode;
			lpfc_log_intr_mode(phba, intr_mode);
			break;
		} else {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"0447 Configure interrupt mode (%d) "
					"failed active interrupt test.\n",
					intr_mode);
			
			lpfc_sli_disable_intr(phba);
			
			cfg_mode = --intr_mode;
		}
	}

	
	lpfc_post_init_setup(phba);

	
	lpfc_create_static_vport(phba);

	return 0;

out_remove_device:
	lpfc_unset_hba(phba);
out_free_sysfs_attr:
	lpfc_free_sysfs_attr(vport);
out_destroy_shost:
	lpfc_destroy_shost(phba);
out_unset_driver_resource:
	lpfc_unset_driver_resource_phase2(phba);
out_free_iocb_list:
	lpfc_free_iocb_list(phba);
out_unset_driver_resource_s3:
	lpfc_sli_driver_resource_unset(phba);
out_unset_pci_mem_s3:
	lpfc_sli_pci_mem_unset(phba);
out_disable_pci_dev:
	lpfc_disable_pci_dev(phba);
	if (shost)
		scsi_host_put(shost);
out_free_phba:
	lpfc_hba_free(phba);
	return error;
}

static void __devexit
lpfc_pci_remove_one_s3(struct pci_dev *pdev)
{
	struct Scsi_Host  *shost = pci_get_drvdata(pdev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_vport **vports;
	struct lpfc_hba   *phba = vport->phba;
	int i;
	int bars = pci_select_bars(pdev, IORESOURCE_MEM);

	spin_lock_irq(&phba->hbalock);
	vport->load_flag |= FC_UNLOADING;
	spin_unlock_irq(&phba->hbalock);

	lpfc_free_sysfs_attr(vport);

	
	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 1; i <= phba->max_vports && vports[i] != NULL; i++)
			fc_vport_terminate(vports[i]->fc_vport);
	lpfc_destroy_vport_work_array(phba, vports);

	
	fc_remove_host(shost);
	scsi_remove_host(shost);
	lpfc_cleanup(vport);


	
	lpfc_sli_hba_down(phba);
	
	kthread_stop(phba->worker_thread);
	
	lpfc_sli_brdrestart(phba);

	kfree(phba->vpi_bmask);
	kfree(phba->vpi_ids);

	lpfc_stop_hba_timers(phba);
	spin_lock_irq(&phba->hbalock);
	list_del_init(&vport->listentry);
	spin_unlock_irq(&phba->hbalock);

	lpfc_debugfs_terminate(vport);

	
	if (phba->cfg_sriov_nr_virtfn)
		pci_disable_sriov(pdev);

	
	lpfc_sli_disable_intr(phba);

	pci_set_drvdata(pdev, NULL);
	scsi_host_put(shost);

	lpfc_scsi_free(phba);
	lpfc_mem_free_all(phba);

	dma_free_coherent(&pdev->dev, lpfc_sli_hbq_size(),
			  phba->hbqslimp.virt, phba->hbqslimp.phys);

	
	dma_free_coherent(&pdev->dev, SLI2_SLIM_SIZE,
			  phba->slim2p.virt, phba->slim2p.phys);

	
	iounmap(phba->ctrl_regs_memmap_p);
	iounmap(phba->slim_memmap_p);

	lpfc_hba_free(phba);

	pci_release_selected_regions(pdev, bars);
	pci_disable_device(pdev);
}

static int
lpfc_pci_suspend_one_s3(struct pci_dev *pdev, pm_message_t msg)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"0473 PCI device Power Management suspend.\n");

	
	lpfc_offline_prep(phba);
	lpfc_offline(phba);
	kthread_stop(phba->worker_thread);

	
	lpfc_sli_disable_intr(phba);

	
	pci_save_state(pdev);
	pci_set_power_state(pdev, PCI_D3hot);

	return 0;
}

static int
lpfc_pci_resume_one_s3(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;
	uint32_t intr_mode;
	int error;

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"0452 PCI device Power Management resume.\n");

	
	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);

	pci_save_state(pdev);

	if (pdev->is_busmaster)
		pci_set_master(pdev);

	
	phba->worker_thread = kthread_run(lpfc_do_work, phba,
					"lpfc_worker_%d", phba->brd_no);
	if (IS_ERR(phba->worker_thread)) {
		error = PTR_ERR(phba->worker_thread);
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0434 PM resume failed to start worker "
				"thread: error=x%x.\n", error);
		return error;
	}

	
	intr_mode = lpfc_sli_enable_intr(phba, phba->intr_mode);
	if (intr_mode == LPFC_INTR_ERROR) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0430 PM resume Failed to enable interrupt\n");
		return -EIO;
	} else
		phba->intr_mode = intr_mode;

	
	lpfc_sli_brdrestart(phba);
	lpfc_online(phba);

	
	lpfc_log_intr_mode(phba, phba->intr_mode);

	return 0;
}

static void
lpfc_sli_prep_dev_for_recover(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring  *pring;

	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2723 PCI channel I/O abort preparing for recovery\n");

	pring = &psli->ring[psli->fcp_ring];
	lpfc_sli_abort_iocb_ring(phba, pring);
}

static void
lpfc_sli_prep_dev_for_reset(struct lpfc_hba *phba)
{
	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2710 PCI channel disable preparing for reset\n");

	
	lpfc_block_mgmt_io(phba);

	
	lpfc_scsi_dev_block(phba);

	
	lpfc_stop_hba_timers(phba);

	
	lpfc_sli_disable_intr(phba);
	pci_disable_device(phba->pcidev);

	
	lpfc_sli_flush_fcp_rings(phba);
}

static void
lpfc_sli_prep_dev_for_perm_failure(struct lpfc_hba *phba)
{
	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2711 PCI channel permanent disable for failure\n");
	
	lpfc_scsi_dev_block(phba);

	
	lpfc_stop_hba_timers(phba);

	
	lpfc_sli_flush_fcp_rings(phba);
}

static pci_ers_result_t
lpfc_io_error_detected_s3(struct pci_dev *pdev, pci_channel_state_t state)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;

	switch (state) {
	case pci_channel_io_normal:
		
		lpfc_sli_prep_dev_for_recover(phba);
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:
		
		lpfc_sli_prep_dev_for_reset(phba);
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		
		lpfc_sli_prep_dev_for_perm_failure(phba);
		return PCI_ERS_RESULT_DISCONNECT;
	default:
		
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0472 Unknown PCI error state: x%x\n", state);
		lpfc_sli_prep_dev_for_reset(phba);
		return PCI_ERS_RESULT_NEED_RESET;
	}
}

static pci_ers_result_t
lpfc_io_slot_reset_s3(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;
	struct lpfc_sli *psli = &phba->sli;
	uint32_t intr_mode;

	dev_printk(KERN_INFO, &pdev->dev, "recovering from a slot reset.\n");
	if (pci_enable_device_mem(pdev)) {
		printk(KERN_ERR "lpfc: Cannot re-enable "
			"PCI device after reset.\n");
		return PCI_ERS_RESULT_DISCONNECT;
	}

	pci_restore_state(pdev);

	pci_save_state(pdev);

	if (pdev->is_busmaster)
		pci_set_master(pdev);

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~LPFC_SLI_ACTIVE;
	spin_unlock_irq(&phba->hbalock);

	
	intr_mode = lpfc_sli_enable_intr(phba, phba->intr_mode);
	if (intr_mode == LPFC_INTR_ERROR) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0427 Cannot re-enable interrupt after "
				"slot reset.\n");
		return PCI_ERS_RESULT_DISCONNECT;
	} else
		phba->intr_mode = intr_mode;

	
	lpfc_offline_prep(phba);
	lpfc_offline(phba);
	lpfc_sli_brdrestart(phba);

	
	lpfc_log_intr_mode(phba, phba->intr_mode);

	return PCI_ERS_RESULT_RECOVERED;
}

static void
lpfc_io_resume_s3(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;

	
	lpfc_online(phba);

	
	if (phba->hba_flag & HBA_AER_ENABLED)
		pci_cleanup_aer_uncorrect_error_status(pdev);
}

int
lpfc_sli4_get_els_iocb_cnt(struct lpfc_hba *phba)
{
	int max_xri = phba->sli4_hba.max_cfg_param.max_xri;

	if (phba->sli_rev == LPFC_SLI_REV4) {
		if (max_xri <= 100)
			return 10;
		else if (max_xri <= 256)
			return 25;
		else if (max_xri <= 512)
			return 50;
		else if (max_xri <= 1024)
			return 100;
		else
			return 150;
	} else
		return 0;
}

/**
 * lpfc_write_firmware - attempt to write a firmware image to the port
 * @phba: pointer to lpfc hba data structure.
 * @fw: pointer to firmware image returned from request_firmware.
 *
 * returns the number of bytes written if write is successful.
 * returns a negative error value if there were errors.
 * returns 0 if firmware matches currently active firmware on port.
 **/
int
lpfc_write_firmware(struct lpfc_hba *phba, const struct firmware *fw)
{
	char fwrev[FW_REV_STR_SIZE];
	struct lpfc_grp_hdr *image = (struct lpfc_grp_hdr *)fw->data;
	struct list_head dma_buffer_list;
	int i, rc = 0;
	struct lpfc_dmabuf *dmabuf, *next;
	uint32_t offset = 0, temp_offset = 0;

	INIT_LIST_HEAD(&dma_buffer_list);
	if ((be32_to_cpu(image->magic_number) != LPFC_GROUP_OJECT_MAGIC_NUM) ||
	    (bf_get_be32(lpfc_grp_hdr_file_type, image) !=
	     LPFC_FILE_TYPE_GROUP) ||
	    (bf_get_be32(lpfc_grp_hdr_id, image) != LPFC_FILE_ID_GROUP) ||
	    (be32_to_cpu(image->size) != fw->size)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"3022 Invalid FW image found. "
				"Magic:%x Type:%x ID:%x\n",
				be32_to_cpu(image->magic_number),
				bf_get_be32(lpfc_grp_hdr_file_type, image),
				bf_get_be32(lpfc_grp_hdr_id, image));
		return -EINVAL;
	}
	lpfc_decode_firmware_rev(phba, fwrev, 1);
	if (strncmp(fwrev, image->revision, strnlen(image->revision, 16))) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"3023 Updating Firmware. Current Version:%s "
				"New Version:%s\n",
				fwrev, image->revision);
		for (i = 0; i < LPFC_MBX_WR_CONFIG_MAX_BDE; i++) {
			dmabuf = kzalloc(sizeof(struct lpfc_dmabuf),
					 GFP_KERNEL);
			if (!dmabuf) {
				rc = -ENOMEM;
				goto out;
			}
			dmabuf->virt = dma_alloc_coherent(&phba->pcidev->dev,
							  SLI4_PAGE_SIZE,
							  &dmabuf->phys,
							  GFP_KERNEL);
			if (!dmabuf->virt) {
				kfree(dmabuf);
				rc = -ENOMEM;
				goto out;
			}
			list_add_tail(&dmabuf->list, &dma_buffer_list);
		}
		while (offset < fw->size) {
			temp_offset = offset;
			list_for_each_entry(dmabuf, &dma_buffer_list, list) {
				if (temp_offset + SLI4_PAGE_SIZE > fw->size) {
					memcpy(dmabuf->virt,
					       fw->data + temp_offset,
					       fw->size - temp_offset);
					temp_offset = fw->size;
					break;
				}
				memcpy(dmabuf->virt, fw->data + temp_offset,
				       SLI4_PAGE_SIZE);
				temp_offset += SLI4_PAGE_SIZE;
			}
			rc = lpfc_wr_object(phba, &dma_buffer_list,
				    (fw->size - offset), &offset);
			if (rc) {
				lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
						"3024 Firmware update failed. "
						"%d\n", rc);
				goto out;
			}
		}
		rc = offset;
	}
out:
	list_for_each_entry_safe(dmabuf, next, &dma_buffer_list, list) {
		list_del(&dmabuf->list);
		dma_free_coherent(&phba->pcidev->dev, SLI4_PAGE_SIZE,
				  dmabuf->virt, dmabuf->phys);
		kfree(dmabuf);
	}
	return rc;
}

static int __devinit
lpfc_pci_probe_one_s4(struct pci_dev *pdev, const struct pci_device_id *pid)
{
	struct lpfc_hba   *phba;
	struct lpfc_vport *vport = NULL;
	struct Scsi_Host  *shost = NULL;
	int error;
	uint32_t cfg_mode, intr_mode;
	int mcnt;
	int adjusted_fcp_eq_count;
	const struct firmware *fw;
	uint8_t file_name[16];

	
	phba = lpfc_hba_alloc(pdev);
	if (!phba)
		return -ENOMEM;

	
	error = lpfc_enable_pci_dev(phba);
	if (error)
		goto out_free_phba;

	
	error = lpfc_api_table_setup(phba, LPFC_PCI_DEV_OC);
	if (error)
		goto out_disable_pci_dev;

	
	error = lpfc_sli4_pci_mem_setup(phba);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1410 Failed to set up pci memory space.\n");
		goto out_disable_pci_dev;
	}

	
	error = lpfc_setup_driver_resource_phase1(phba);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1411 Failed to set up driver resource.\n");
		goto out_unset_pci_mem_s4;
	}

	
	error = lpfc_sli4_driver_resource_setup(phba);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1412 Failed to set up driver resource.\n");
		goto out_unset_pci_mem_s4;
	}

	

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"2821 initialize iocb list %d.\n",
			phba->cfg_iocb_cnt*1024);
	error = lpfc_init_iocb_list(phba, phba->cfg_iocb_cnt*1024);

	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1413 Failed to initialize iocb list.\n");
		goto out_unset_driver_resource_s4;
	}

	INIT_LIST_HEAD(&phba->active_rrq_list);
	INIT_LIST_HEAD(&phba->fcf.fcf_pri_list);

	
	error = lpfc_setup_driver_resource_phase2(phba);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1414 Failed to set up driver resource.\n");
		goto out_free_iocb_list;
	}

	
	lpfc_get_hba_model_desc(phba, phba->ModelName, phba->ModelDesc);

	
	error = lpfc_create_shost(phba);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1415 Failed to create scsi host.\n");
		goto out_unset_driver_resource;
	}

	
	vport = phba->pport;
	error = lpfc_alloc_sysfs_attr(vport);
	if (error) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1416 Failed to allocate sysfs attr\n");
		goto out_destroy_shost;
	}

	shost = lpfc_shost_from_vport(vport); 
	
	cfg_mode = phba->cfg_use_msi;
	while (true) {
		
		lpfc_stop_port(phba);
		
		intr_mode = lpfc_sli4_enable_intr(phba, cfg_mode);
		if (intr_mode == LPFC_INTR_ERROR) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0426 Failed to enable interrupt.\n");
			error = -ENODEV;
			goto out_free_sysfs_attr;
		}
		
		if (phba->intr_type != MSIX)
			adjusted_fcp_eq_count = 0;
		else if (phba->sli4_hba.msix_vec_nr <
					phba->cfg_fcp_eq_count + 1)
			adjusted_fcp_eq_count = phba->sli4_hba.msix_vec_nr - 1;
		else
			adjusted_fcp_eq_count = phba->cfg_fcp_eq_count;
		phba->cfg_fcp_eq_count = adjusted_fcp_eq_count;
		
		if (lpfc_sli4_hba_setup(phba)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"1421 Failed to set up hba\n");
			error = -ENODEV;
			goto out_disable_intr;
		}

		
		if (intr_mode != 0)
			mcnt = lpfc_sli4_send_nop_mbox_cmds(phba,
							    LPFC_ACT_INTR_CNT);

		
		if (intr_mode == 0 ||
		    phba->sli.slistat.sli_intr >= LPFC_ACT_INTR_CNT) {
			
			phba->intr_mode = intr_mode;
			lpfc_log_intr_mode(phba, intr_mode);
			break;
		}
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0451 Configure interrupt mode (%d) "
				"failed active interrupt test.\n",
				intr_mode);
		
		lpfc_sli4_unset_hba(phba);
		
		cfg_mode = --intr_mode;
	}

	
	lpfc_post_init_setup(phba);

	
	if (bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) ==
	    LPFC_SLI_INTF_IF_TYPE_2) {
		snprintf(file_name, 16, "%s.grp", phba->ModelName);
		error = request_firmware(&fw, file_name, &phba->pcidev->dev);
		if (!error) {
			lpfc_write_firmware(phba, fw);
			release_firmware(fw);
		}
	}

	
	lpfc_create_static_vport(phba);
	return 0;

out_disable_intr:
	lpfc_sli4_disable_intr(phba);
out_free_sysfs_attr:
	lpfc_free_sysfs_attr(vport);
out_destroy_shost:
	lpfc_destroy_shost(phba);
out_unset_driver_resource:
	lpfc_unset_driver_resource_phase2(phba);
out_free_iocb_list:
	lpfc_free_iocb_list(phba);
out_unset_driver_resource_s4:
	lpfc_sli4_driver_resource_unset(phba);
out_unset_pci_mem_s4:
	lpfc_sli4_pci_mem_unset(phba);
out_disable_pci_dev:
	lpfc_disable_pci_dev(phba);
	if (shost)
		scsi_host_put(shost);
out_free_phba:
	lpfc_hba_free(phba);
	return error;
}

static void __devexit
lpfc_pci_remove_one_s4(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_vport **vports;
	struct lpfc_hba *phba = vport->phba;
	int i;

	
	spin_lock_irq(&phba->hbalock);
	vport->load_flag |= FC_UNLOADING;
	spin_unlock_irq(&phba->hbalock);

	
	lpfc_free_sysfs_attr(vport);

	
	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 1; i <= phba->max_vports && vports[i] != NULL; i++)
			fc_vport_terminate(vports[i]->fc_vport);
	lpfc_destroy_vport_work_array(phba, vports);

	
	fc_remove_host(shost);
	scsi_remove_host(shost);

	
	lpfc_cleanup(vport);

	lpfc_debugfs_terminate(vport);
	lpfc_sli4_hba_unset(phba);

	spin_lock_irq(&phba->hbalock);
	list_del_init(&vport->listentry);
	spin_unlock_irq(&phba->hbalock);

	lpfc_scsi_free(phba);
	lpfc_sli4_driver_resource_unset(phba);

	
	lpfc_sli4_pci_mem_unset(phba);

	
	scsi_host_put(shost);
	lpfc_disable_pci_dev(phba);

	
	lpfc_hba_free(phba);

	return;
}

static int
lpfc_pci_suspend_one_s4(struct pci_dev *pdev, pm_message_t msg)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"2843 PCI device Power Management suspend.\n");

	
	lpfc_offline_prep(phba);
	lpfc_offline(phba);
	kthread_stop(phba->worker_thread);

	
	lpfc_sli4_disable_intr(phba);
	lpfc_sli4_queue_destroy(phba);

	
	pci_save_state(pdev);
	pci_set_power_state(pdev, PCI_D3hot);

	return 0;
}

static int
lpfc_pci_resume_one_s4(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;
	uint32_t intr_mode;
	int error;

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"0292 PCI device Power Management resume.\n");

	
	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);

	pci_save_state(pdev);

	if (pdev->is_busmaster)
		pci_set_master(pdev);

	 
	phba->worker_thread = kthread_run(lpfc_do_work, phba,
					"lpfc_worker_%d", phba->brd_no);
	if (IS_ERR(phba->worker_thread)) {
		error = PTR_ERR(phba->worker_thread);
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0293 PM resume failed to start worker "
				"thread: error=x%x.\n", error);
		return error;
	}

	
	intr_mode = lpfc_sli4_enable_intr(phba, phba->intr_mode);
	if (intr_mode == LPFC_INTR_ERROR) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0294 PM resume Failed to enable interrupt\n");
		return -EIO;
	} else
		phba->intr_mode = intr_mode;

	
	lpfc_sli_brdrestart(phba);
	lpfc_online(phba);

	
	lpfc_log_intr_mode(phba, phba->intr_mode);

	return 0;
}

static void
lpfc_sli4_prep_dev_for_recover(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring  *pring;

	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2828 PCI channel I/O abort preparing for recovery\n");
	pring = &psli->ring[psli->fcp_ring];
	lpfc_sli_abort_iocb_ring(phba, pring);
}

static void
lpfc_sli4_prep_dev_for_reset(struct lpfc_hba *phba)
{
	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2826 PCI channel disable preparing for reset\n");

	
	lpfc_block_mgmt_io(phba);

	
	lpfc_scsi_dev_block(phba);

	
	lpfc_stop_hba_timers(phba);

	
	lpfc_sli4_disable_intr(phba);
	lpfc_sli4_queue_destroy(phba);
	pci_disable_device(phba->pcidev);

	
	lpfc_sli_flush_fcp_rings(phba);
}

static void
lpfc_sli4_prep_dev_for_perm_failure(struct lpfc_hba *phba)
{
	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2827 PCI channel permanent disable for failure\n");

	
	lpfc_scsi_dev_block(phba);

	
	lpfc_stop_hba_timers(phba);

	
	lpfc_sli_flush_fcp_rings(phba);
}

static pci_ers_result_t
lpfc_io_error_detected_s4(struct pci_dev *pdev, pci_channel_state_t state)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;

	switch (state) {
	case pci_channel_io_normal:
		
		lpfc_sli4_prep_dev_for_recover(phba);
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:
		
		lpfc_sli4_prep_dev_for_reset(phba);
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		
		lpfc_sli4_prep_dev_for_perm_failure(phba);
		return PCI_ERS_RESULT_DISCONNECT;
	default:
		
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2825 Unknown PCI error state: x%x\n", state);
		lpfc_sli4_prep_dev_for_reset(phba);
		return PCI_ERS_RESULT_NEED_RESET;
	}
}

static pci_ers_result_t
lpfc_io_slot_reset_s4(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;
	struct lpfc_sli *psli = &phba->sli;
	uint32_t intr_mode;

	dev_printk(KERN_INFO, &pdev->dev, "recovering from a slot reset.\n");
	if (pci_enable_device_mem(pdev)) {
		printk(KERN_ERR "lpfc: Cannot re-enable "
			"PCI device after reset.\n");
		return PCI_ERS_RESULT_DISCONNECT;
	}

	pci_restore_state(pdev);

	pci_save_state(pdev);

	if (pdev->is_busmaster)
		pci_set_master(pdev);

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~LPFC_SLI_ACTIVE;
	spin_unlock_irq(&phba->hbalock);

	
	intr_mode = lpfc_sli4_enable_intr(phba, phba->intr_mode);
	if (intr_mode == LPFC_INTR_ERROR) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2824 Cannot re-enable interrupt after "
				"slot reset.\n");
		return PCI_ERS_RESULT_DISCONNECT;
	} else
		phba->intr_mode = intr_mode;

	
	lpfc_log_intr_mode(phba, phba->intr_mode);

	return PCI_ERS_RESULT_RECOVERED;
}

static void
lpfc_io_resume_s4(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;

	if (!(phba->sli.sli_flag & LPFC_SLI_ACTIVE)) {
		
		lpfc_offline_prep(phba);
		lpfc_offline(phba);
		lpfc_sli_brdrestart(phba);
		
		lpfc_online(phba);
	}

	
	if (phba->hba_flag & HBA_AER_ENABLED)
		pci_cleanup_aer_uncorrect_error_status(pdev);
}

static int __devinit
lpfc_pci_probe_one(struct pci_dev *pdev, const struct pci_device_id *pid)
{
	int rc;
	struct lpfc_sli_intf intf;

	if (pci_read_config_dword(pdev, LPFC_SLI_INTF, &intf.word0))
		return -ENODEV;

	if ((bf_get(lpfc_sli_intf_valid, &intf) == LPFC_SLI_INTF_VALID) &&
	    (bf_get(lpfc_sli_intf_slirev, &intf) == LPFC_SLI_INTF_REV_SLI4))
		rc = lpfc_pci_probe_one_s4(pdev, pid);
	else
		rc = lpfc_pci_probe_one_s3(pdev, pid);

	return rc;
}

static void __devexit
lpfc_pci_remove_one(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;

	switch (phba->pci_dev_grp) {
	case LPFC_PCI_DEV_LP:
		lpfc_pci_remove_one_s3(pdev);
		break;
	case LPFC_PCI_DEV_OC:
		lpfc_pci_remove_one_s4(pdev);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1424 Invalid PCI device group: 0x%x\n",
				phba->pci_dev_grp);
		break;
	}
	return;
}

static int
lpfc_pci_suspend_one(struct pci_dev *pdev, pm_message_t msg)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;
	int rc = -ENODEV;

	switch (phba->pci_dev_grp) {
	case LPFC_PCI_DEV_LP:
		rc = lpfc_pci_suspend_one_s3(pdev, msg);
		break;
	case LPFC_PCI_DEV_OC:
		rc = lpfc_pci_suspend_one_s4(pdev, msg);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1425 Invalid PCI device group: 0x%x\n",
				phba->pci_dev_grp);
		break;
	}
	return rc;
}

static int
lpfc_pci_resume_one(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;
	int rc = -ENODEV;

	switch (phba->pci_dev_grp) {
	case LPFC_PCI_DEV_LP:
		rc = lpfc_pci_resume_one_s3(pdev);
		break;
	case LPFC_PCI_DEV_OC:
		rc = lpfc_pci_resume_one_s4(pdev);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1426 Invalid PCI device group: 0x%x\n",
				phba->pci_dev_grp);
		break;
	}
	return rc;
}

static pci_ers_result_t
lpfc_io_error_detected(struct pci_dev *pdev, pci_channel_state_t state)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;
	pci_ers_result_t rc = PCI_ERS_RESULT_DISCONNECT;

	switch (phba->pci_dev_grp) {
	case LPFC_PCI_DEV_LP:
		rc = lpfc_io_error_detected_s3(pdev, state);
		break;
	case LPFC_PCI_DEV_OC:
		rc = lpfc_io_error_detected_s4(pdev, state);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1427 Invalid PCI device group: 0x%x\n",
				phba->pci_dev_grp);
		break;
	}
	return rc;
}

static pci_ers_result_t
lpfc_io_slot_reset(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;
	pci_ers_result_t rc = PCI_ERS_RESULT_DISCONNECT;

	switch (phba->pci_dev_grp) {
	case LPFC_PCI_DEV_LP:
		rc = lpfc_io_slot_reset_s3(pdev);
		break;
	case LPFC_PCI_DEV_OC:
		rc = lpfc_io_slot_reset_s4(pdev);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1428 Invalid PCI device group: 0x%x\n",
				phba->pci_dev_grp);
		break;
	}
	return rc;
}

static void
lpfc_io_resume(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;

	switch (phba->pci_dev_grp) {
	case LPFC_PCI_DEV_LP:
		lpfc_io_resume_s3(pdev);
		break;
	case LPFC_PCI_DEV_OC:
		lpfc_io_resume_s4(pdev);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1429 Invalid PCI device group: 0x%x\n",
				phba->pci_dev_grp);
		break;
	}
	return;
}

static int
lpfc_mgmt_open(struct inode *inode, struct file *filep)
{
	try_module_get(THIS_MODULE);
	return 0;
}

static int
lpfc_mgmt_release(struct inode *inode, struct file *filep)
{
	module_put(THIS_MODULE);
	return 0;
}

static struct pci_device_id lpfc_id_table[] = {
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_VIPER,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_FIREFLY,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_THOR,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_PEGASUS,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_CENTAUR,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_DRAGONFLY,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_SUPERFLY,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_RFLY,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_PFLY,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_NEPTUNE,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_NEPTUNE_SCSP,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_NEPTUNE_DCSP,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_HELIOS,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_HELIOS_SCSP,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_HELIOS_DCSP,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_BMID,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_BSMB,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_ZEPHYR,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_HORNET,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_ZEPHYR_SCSP,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_ZEPHYR_DCSP,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_ZMID,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_ZSMB,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_TFLY,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_LP101,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_LP10000S,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_LP11000S,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_LPE11000S,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_SAT,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_SAT_MID,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_SAT_SMB,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_SAT_DCSP,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_SAT_SCSP,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_SAT_S,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_PROTEUS_VF,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_PROTEUS_PF,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_PROTEUS_S,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_SERVERENGINE, PCI_DEVICE_ID_TIGERSHARK,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_SERVERENGINE, PCI_DEVICE_ID_TOMCAT,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_FALCON,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_BALIUS,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_LANCER_FC,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_LANCER_FCOE,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_LANCER_FC_VF,
		PCI_ANY_ID, PCI_ANY_ID, },
	{PCI_VENDOR_ID_EMULEX, PCI_DEVICE_ID_LANCER_FCOE_VF,
		PCI_ANY_ID, PCI_ANY_ID, },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, lpfc_id_table);

static struct pci_error_handlers lpfc_err_handler = {
	.error_detected = lpfc_io_error_detected,
	.slot_reset = lpfc_io_slot_reset,
	.resume = lpfc_io_resume,
};

static struct pci_driver lpfc_driver = {
	.name		= LPFC_DRIVER_NAME,
	.id_table	= lpfc_id_table,
	.probe		= lpfc_pci_probe_one,
	.remove		= __devexit_p(lpfc_pci_remove_one),
	.suspend        = lpfc_pci_suspend_one,
	.resume		= lpfc_pci_resume_one,
	.err_handler    = &lpfc_err_handler,
};

static const struct file_operations lpfc_mgmt_fop = {
	.open = lpfc_mgmt_open,
	.release = lpfc_mgmt_release,
};

static struct miscdevice lpfc_mgmt_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "lpfcmgmt",
	.fops = &lpfc_mgmt_fop,
};

static int __init
lpfc_init(void)
{
	int error = 0;

	printk(LPFC_MODULE_DESC "\n");
	printk(LPFC_COPYRIGHT "\n");

	error = misc_register(&lpfc_mgmt_dev);
	if (error)
		printk(KERN_ERR "Could not register lpfcmgmt device, "
			"misc_register returned with status %d", error);

	if (lpfc_enable_npiv) {
		lpfc_transport_functions.vport_create = lpfc_vport_create;
		lpfc_transport_functions.vport_delete = lpfc_vport_delete;
	}
	lpfc_transport_template =
				fc_attach_transport(&lpfc_transport_functions);
	if (lpfc_transport_template == NULL)
		return -ENOMEM;
	if (lpfc_enable_npiv) {
		lpfc_vport_transport_template =
			fc_attach_transport(&lpfc_vport_transport_functions);
		if (lpfc_vport_transport_template == NULL) {
			fc_release_transport(lpfc_transport_template);
			return -ENOMEM;
		}
	}
	error = pci_register_driver(&lpfc_driver);
	if (error) {
		fc_release_transport(lpfc_transport_template);
		if (lpfc_enable_npiv)
			fc_release_transport(lpfc_vport_transport_template);
	}

	return error;
}

static void __exit
lpfc_exit(void)
{
	misc_deregister(&lpfc_mgmt_dev);
	pci_unregister_driver(&lpfc_driver);
	fc_release_transport(lpfc_transport_template);
	if (lpfc_enable_npiv)
		fc_release_transport(lpfc_vport_transport_template);
	if (_dump_buf_data) {
		printk(KERN_ERR	"9062 BLKGRD: freeing %lu pages for "
				"_dump_buf_data at 0x%p\n",
				(1L << _dump_buf_data_order), _dump_buf_data);
		free_pages((unsigned long)_dump_buf_data, _dump_buf_data_order);
	}

	if (_dump_buf_dif) {
		printk(KERN_ERR	"9049 BLKGRD: freeing %lu pages for "
				"_dump_buf_dif at 0x%p\n",
				(1L << _dump_buf_dif_order), _dump_buf_dif);
		free_pages((unsigned long)_dump_buf_dif, _dump_buf_dif_order);
	}
}

module_init(lpfc_init);
module_exit(lpfc_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(LPFC_MODULE_DESC);
MODULE_AUTHOR("Emulex Corporation - tech.support@emulex.com");
MODULE_VERSION("0:" LPFC_DRIVER_VERSION);
