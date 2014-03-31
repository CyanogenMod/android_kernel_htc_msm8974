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
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

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
#include "lpfc_debugfs.h"

static int lpfc_els_retry(struct lpfc_hba *, struct lpfc_iocbq *,
			  struct lpfc_iocbq *);
static void lpfc_cmpl_fabric_iocb(struct lpfc_hba *, struct lpfc_iocbq *,
			struct lpfc_iocbq *);
static void lpfc_fabric_abort_vport(struct lpfc_vport *vport);
static int lpfc_issue_els_fdisc(struct lpfc_vport *vport,
				struct lpfc_nodelist *ndlp, uint8_t retry);
static int lpfc_issue_fabric_iocb(struct lpfc_hba *phba,
				  struct lpfc_iocbq *iocb);

static int lpfc_max_els_tries = 3;

int
lpfc_els_chk_latt(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	uint32_t ha_copy;

	if (vport->port_state >= LPFC_VPORT_READY ||
	    phba->link_state == LPFC_LINK_DOWN ||
	    phba->sli_rev > LPFC_SLI_REV3)
		return 0;

	
	if (lpfc_readl(phba->HAregaddr, &ha_copy))
		return 1;

	if (!(ha_copy & HA_LATT))
		return 0;

	
	lpfc_printf_vlog(vport, KERN_ERR, LOG_DISCOVERY,
			 "0237 Pending Link Event during "
			 "Discovery: State x%x\n",
			 phba->pport->port_state);

	spin_lock_irq(shost->host_lock);
	vport->fc_flag |= FC_ABORT_DISCOVERY;
	spin_unlock_irq(shost->host_lock);

	if (phba->link_state != LPFC_CLEAR_LA)
		lpfc_issue_clear_la(phba, vport);

	return 1;
}

struct lpfc_iocbq *
lpfc_prep_els_iocb(struct lpfc_vport *vport, uint8_t expectRsp,
		   uint16_t cmdSize, uint8_t retry,
		   struct lpfc_nodelist *ndlp, uint32_t did,
		   uint32_t elscmd)
{
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_dmabuf *pcmd, *prsp, *pbuflist;
	struct ulp_bde64 *bpl;
	IOCB_t *icmd;


	if (!lpfc_is_link_up(phba))
		return NULL;

	
	elsiocb = lpfc_sli_get_iocbq(phba);

	if (elsiocb == NULL)
		return NULL;

	if ((did == Fabric_DID) &&
		(phba->hba_flag & HBA_FIP_SUPPORT) &&
		((elscmd == ELS_CMD_FLOGI) ||
		 (elscmd == ELS_CMD_FDISC) ||
		 (elscmd == ELS_CMD_LOGO)))
		switch (elscmd) {
		case ELS_CMD_FLOGI:
		elsiocb->iocb_flag |=
			((LPFC_ELS_ID_FLOGI << LPFC_FIP_ELS_ID_SHIFT)
					& LPFC_FIP_ELS_ID_MASK);
		break;
		case ELS_CMD_FDISC:
		elsiocb->iocb_flag |=
			((LPFC_ELS_ID_FDISC << LPFC_FIP_ELS_ID_SHIFT)
					& LPFC_FIP_ELS_ID_MASK);
		break;
		case ELS_CMD_LOGO:
		elsiocb->iocb_flag |=
			((LPFC_ELS_ID_LOGO << LPFC_FIP_ELS_ID_SHIFT)
					& LPFC_FIP_ELS_ID_MASK);
		break;
		}
	else
		elsiocb->iocb_flag &= ~LPFC_FIP_ELS_ID_MASK;

	icmd = &elsiocb->iocb;

	
	
	pcmd = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (pcmd)
		pcmd->virt = lpfc_mbuf_alloc(phba, MEM_PRI, &pcmd->phys);
	if (!pcmd || !pcmd->virt)
		goto els_iocb_free_pcmb_exit;

	INIT_LIST_HEAD(&pcmd->list);

	
	if (expectRsp) {
		prsp = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
		if (prsp)
			prsp->virt = lpfc_mbuf_alloc(phba, MEM_PRI,
						     &prsp->phys);
		if (!prsp || !prsp->virt)
			goto els_iocb_free_prsp_exit;
		INIT_LIST_HEAD(&prsp->list);
	} else
		prsp = NULL;

	
	pbuflist = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (pbuflist)
		pbuflist->virt = lpfc_mbuf_alloc(phba, MEM_PRI,
						 &pbuflist->phys);
	if (!pbuflist || !pbuflist->virt)
		goto els_iocb_free_pbuf_exit;

	INIT_LIST_HEAD(&pbuflist->list);

	icmd->un.elsreq64.bdl.addrHigh = putPaddrHigh(pbuflist->phys);
	icmd->un.elsreq64.bdl.addrLow = putPaddrLow(pbuflist->phys);
	icmd->un.elsreq64.bdl.bdeFlags = BUFF_TYPE_BLP_64;
	icmd->un.elsreq64.remoteID = did;	
	if (expectRsp) {
		icmd->un.elsreq64.bdl.bdeSize = (2 * sizeof(struct ulp_bde64));
		icmd->ulpCommand = CMD_ELS_REQUEST64_CR;
		icmd->ulpTimeout = phba->fc_ratov * 2;
	} else {
		icmd->un.elsreq64.bdl.bdeSize = sizeof(struct ulp_bde64);
		icmd->ulpCommand = CMD_XMIT_ELS_RSP64_CX;
	}
	icmd->ulpBdeCount = 1;
	icmd->ulpLe = 1;
	icmd->ulpClass = CLASS3;

	if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) {
		icmd->un.elsreq64.myID = vport->fc_myDID;

		
		icmd->ulpContext = phba->vpi_ids[vport->vpi];
		icmd->ulpCt_h = 0;
		
		if (elscmd == ELS_CMD_ECHO)
			icmd->ulpCt_l = 0; 
		else
			icmd->ulpCt_l = 1; 
	}

	bpl = (struct ulp_bde64 *) pbuflist->virt;
	bpl->addrLow = le32_to_cpu(putPaddrLow(pcmd->phys));
	bpl->addrHigh = le32_to_cpu(putPaddrHigh(pcmd->phys));
	bpl->tus.f.bdeSize = cmdSize;
	bpl->tus.f.bdeFlags = 0;
	bpl->tus.w = le32_to_cpu(bpl->tus.w);

	if (expectRsp) {
		bpl++;
		bpl->addrLow = le32_to_cpu(putPaddrLow(prsp->phys));
		bpl->addrHigh = le32_to_cpu(putPaddrHigh(prsp->phys));
		bpl->tus.f.bdeSize = FCELSSIZE;
		bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_64;
		bpl->tus.w = le32_to_cpu(bpl->tus.w);
	}

	
	elsiocb->context1 = lpfc_nlp_get(ndlp);
	if (!elsiocb->context1)
		goto els_iocb_free_pbuf_exit;
	elsiocb->context2 = pcmd;
	elsiocb->context3 = pbuflist;
	elsiocb->retry = retry;
	elsiocb->vport = vport;
	elsiocb->drvrTimeout = (phba->fc_ratov << 1) + LPFC_DRVR_TIMEOUT;

	if (prsp) {
		list_add(&prsp->list, &pcmd->list);
	}
	if (expectRsp) {
		
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
				 "0116 Xmit ELS command x%x to remote "
				 "NPORT x%x I/O tag: x%x, port state: x%x\n",
				 elscmd, did, elsiocb->iotag,
				 vport->port_state);
	} else {
		
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
				 "0117 Xmit ELS response x%x to remote "
				 "NPORT x%x I/O tag: x%x, size: x%x\n",
				 elscmd, ndlp->nlp_DID, elsiocb->iotag,
				 cmdSize);
	}
	return elsiocb;

els_iocb_free_pbuf_exit:
	if (expectRsp)
		lpfc_mbuf_free(phba, prsp->virt, prsp->phys);
	kfree(pbuflist);

els_iocb_free_prsp_exit:
	lpfc_mbuf_free(phba, pcmd->virt, pcmd->phys);
	kfree(prsp);

els_iocb_free_pcmb_exit:
	kfree(pcmd);
	lpfc_sli_release_iocbq(phba, elsiocb);
	return NULL;
}

int
lpfc_issue_fabric_reglogin(struct lpfc_vport *vport)
{
	struct lpfc_hba  *phba = vport->phba;
	LPFC_MBOXQ_t *mbox;
	struct lpfc_dmabuf *mp;
	struct lpfc_nodelist *ndlp;
	struct serv_parm *sp;
	int rc;
	int err = 0;

	sp = &phba->fc_fabparam;
	ndlp = lpfc_findnode_did(vport, Fabric_DID);
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp)) {
		err = 1;
		goto fail;
	}

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		err = 2;
		goto fail;
	}

	vport->port_state = LPFC_FABRIC_CFG_LINK;
	lpfc_config_link(phba, mbox);
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	mbox->vport = vport;

	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		err = 3;
		goto fail_free_mbox;
	}

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		err = 4;
		goto fail;
	}
	rc = lpfc_reg_rpi(phba, vport->vpi, Fabric_DID, (uint8_t *)sp, mbox,
			  ndlp->nlp_rpi);
	if (rc) {
		err = 5;
		goto fail_free_mbox;
	}

	mbox->mbox_cmpl = lpfc_mbx_cmpl_fabric_reg_login;
	mbox->vport = vport;
	mbox->context2 = lpfc_nlp_get(ndlp);

	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		err = 6;
		goto fail_issue_reg_login;
	}

	return 0;

fail_issue_reg_login:
	lpfc_nlp_put(ndlp);
	mp = (struct lpfc_dmabuf *) mbox->context1;
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
fail_free_mbox:
	mempool_free(mbox, phba->mbox_mem_pool);

fail:
	lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
		"0249 Cannot issue Register Fabric login: Err %d\n", err);
	return -ENXIO;
}

int
lpfc_issue_reg_vfi(struct lpfc_vport *vport)
{
	struct lpfc_hba  *phba = vport->phba;
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_nodelist *ndlp;
	struct serv_parm *sp;
	struct lpfc_dmabuf *dmabuf;
	int rc = 0;

	sp = &phba->fc_fabparam;
	
	if ((phba->sli_rev == LPFC_SLI_REV4) &&
	    !(phba->link_flag & LS_LOOPBACK_MODE)) {
		ndlp = lpfc_findnode_did(vport, Fabric_DID);
		if (!ndlp || !NLP_CHK_NODE_ACT(ndlp)) {
			rc = -ENODEV;
			goto fail;
		}
	}

	dmabuf = kzalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (!dmabuf) {
		rc = -ENOMEM;
		goto fail;
	}
	dmabuf->virt = lpfc_mbuf_alloc(phba, MEM_PRI, &dmabuf->phys);
	if (!dmabuf->virt) {
		rc = -ENOMEM;
		goto fail_free_dmabuf;
	}

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		rc = -ENOMEM;
		goto fail_free_coherent;
	}
	vport->port_state = LPFC_FABRIC_CFG_LINK;
	memcpy(dmabuf->virt, &phba->fc_fabparam, sizeof(vport->fc_sparam));
	lpfc_reg_vfi(mboxq, vport, dmabuf->phys);
	mboxq->mbox_cmpl = lpfc_mbx_cmpl_reg_vfi;
	mboxq->vport = vport;
	mboxq->context1 = dmabuf;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		rc = -ENXIO;
		goto fail_free_mbox;
	}
	return 0;

fail_free_mbox:
	mempool_free(mboxq, phba->mbox_mem_pool);
fail_free_coherent:
	lpfc_mbuf_free(phba, dmabuf->virt, dmabuf->phys);
fail_free_dmabuf:
	kfree(dmabuf);
fail:
	lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
		"0289 Issue Register VFI failed: Err %d\n", rc);
	return rc;
}

int
lpfc_issue_unreg_vfi(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	struct Scsi_Host *shost;
	LPFC_MBOXQ_t *mboxq;
	int rc;

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_DISCOVERY|LOG_MBOX,
				"2556 UNREG_VFI mbox allocation failed"
				"HBA state x%x\n", phba->pport->port_state);
		return -ENOMEM;
	}

	lpfc_unreg_vfi(mboxq, vport);
	mboxq->vport = vport;
	mboxq->mbox_cmpl = lpfc_unregister_vfi_cmpl;

	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		lpfc_printf_log(phba, KERN_ERR, LOG_DISCOVERY|LOG_MBOX,
				"2557 UNREG_VFI issue mbox failed rc x%x "
				"HBA state x%x\n",
				rc, phba->pport->port_state);
		mempool_free(mboxq, phba->mbox_mem_pool);
		return -EIO;
	}

	shost = lpfc_shost_from_vport(vport);
	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~FC_VFI_REGISTERED;
	spin_unlock_irq(shost->host_lock);
	return 0;
}

static uint8_t
lpfc_check_clean_addr_bit(struct lpfc_vport *vport,
		struct serv_parm *sp)
{
	uint8_t fabric_param_changed = 0;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	if ((vport->fc_prevDID != vport->fc_myDID) ||
		memcmp(&vport->fabric_portname, &sp->portName,
			sizeof(struct lpfc_name)) ||
		memcmp(&vport->fabric_nodename, &sp->nodeName,
			sizeof(struct lpfc_name)))
		fabric_param_changed = 1;

	if (fabric_param_changed && !sp->cmn.clean_address_bit &&
	    (vport->fc_prevDID || lpfc_delay_discovery)) {
		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_DISC_DELAYED;
		spin_unlock_irq(shost->host_lock);
	}

	return fabric_param_changed;
}


static int
lpfc_cmpl_els_flogi_fabric(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
			   struct serv_parm *sp, IOCB_t *irsp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_nodelist *np;
	struct lpfc_nodelist *next_np;
	uint8_t fabric_param_changed;

	spin_lock_irq(shost->host_lock);
	vport->fc_flag |= FC_FABRIC;
	spin_unlock_irq(shost->host_lock);

	phba->fc_edtov = be32_to_cpu(sp->cmn.e_d_tov);
	if (sp->cmn.edtovResolution)	
		phba->fc_edtov = (phba->fc_edtov + 999999) / 1000000;

	phba->fc_edtovResol = sp->cmn.edtovResolution;
	phba->fc_ratov = (be32_to_cpu(sp->cmn.w2.r_a_tov) + 999) / 1000;

	if (phba->fc_topology == LPFC_TOPOLOGY_LOOP) {
		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_PUBLIC_LOOP;
		spin_unlock_irq(shost->host_lock);
	}

	vport->fc_myDID = irsp->un.ulpWord[4] & Mask_DID;
	memcpy(&ndlp->nlp_portname, &sp->portName, sizeof(struct lpfc_name));
	memcpy(&ndlp->nlp_nodename, &sp->nodeName, sizeof(struct lpfc_name));
	ndlp->nlp_class_sup = 0;
	if (sp->cls1.classValid)
		ndlp->nlp_class_sup |= FC_COS_CLASS1;
	if (sp->cls2.classValid)
		ndlp->nlp_class_sup |= FC_COS_CLASS2;
	if (sp->cls3.classValid)
		ndlp->nlp_class_sup |= FC_COS_CLASS3;
	if (sp->cls4.classValid)
		ndlp->nlp_class_sup |= FC_COS_CLASS4;
	ndlp->nlp_maxframe = ((sp->cmn.bbRcvSizeMsb & 0x0F) << 8) |
				sp->cmn.bbRcvSizeLsb;

	fabric_param_changed = lpfc_check_clean_addr_bit(vport, sp);
	memcpy(&vport->fabric_portname, &sp->portName,
			sizeof(struct lpfc_name));
	memcpy(&vport->fabric_nodename, &sp->nodeName,
			sizeof(struct lpfc_name));
	memcpy(&phba->fc_fabparam, sp, sizeof(struct serv_parm));

	if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) {
		if (sp->cmn.response_multiple_NPort) {
			lpfc_printf_vlog(vport, KERN_WARNING,
					 LOG_ELS | LOG_VPORT,
					 "1816 FLOGI NPIV supported, "
					 "response data 0x%x\n",
					 sp->cmn.response_multiple_NPort);
			spin_lock_irq(&phba->hbalock);
			phba->link_flag |= LS_NPIV_FAB_SUPPORTED;
			spin_unlock_irq(&phba->hbalock);
		} else {
			lpfc_printf_vlog(vport, KERN_WARNING,
					 LOG_ELS | LOG_VPORT,
					 "1817 Fabric does not support NPIV "
					 "- configuring single port mode.\n");
			spin_lock_irq(&phba->hbalock);
			phba->link_flag &= ~LS_NPIV_FAB_SUPPORTED;
			spin_unlock_irq(&phba->hbalock);
		}
	}

	if (fabric_param_changed &&
		!(vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)) {

		list_for_each_entry_safe(np, next_np,
					&vport->fc_nodes, nlp_listp) {
			if (!NLP_CHK_NODE_ACT(np))
				continue;
			if ((np->nlp_state != NLP_STE_NPR_NODE) ||
				   !(np->nlp_flag & NLP_NPR_ADISC))
				continue;
			spin_lock_irq(shost->host_lock);
			np->nlp_flag &= ~NLP_NPR_ADISC;
			spin_unlock_irq(shost->host_lock);
			lpfc_unreg_rpi(vport, np);
		}
		lpfc_cleanup_pending_mbox(vport);

		if (phba->sli_rev == LPFC_SLI_REV4) {
			lpfc_sli4_unreg_all_rpis(vport);
			lpfc_mbx_unreg_vpi(vport);
			spin_lock_irq(shost->host_lock);
			vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
			vport->fc_flag |= FC_VPORT_NEEDS_INIT_VPI;
			spin_unlock_irq(shost->host_lock);
		}
	} else if ((phba->sli_rev == LPFC_SLI_REV4) &&
		!(vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)) {
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_UNMAPPED_NODE);
			lpfc_register_new_vport(phba, vport, ndlp);
			return 0;
	}

	if (phba->sli_rev < LPFC_SLI_REV4) {
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_REG_LOGIN_ISSUE);
		if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED &&
		    vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)
			lpfc_register_new_vport(phba, vport, ndlp);
		else
			lpfc_issue_fabric_reglogin(vport);
	} else {
		ndlp->nlp_type |= NLP_FABRIC;
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_UNMAPPED_NODE);
		if ((!(vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)) &&
			(vport->vpi_state & LPFC_VPI_REGISTERED)) {
			lpfc_start_fdiscs(phba);
			lpfc_do_scr_ns_plogi(phba, vport);
		} else if (vport->fc_flag & FC_VFI_REGISTERED)
			lpfc_issue_init_vpi(vport);
		else {
			lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
					"3135 Need register VFI: (x%x/%x)\n",
					vport->fc_prevDID, vport->fc_myDID);
			lpfc_issue_reg_vfi(vport);
		}
	}
	return 0;
}

static int
lpfc_cmpl_els_flogi_nport(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
			  struct serv_parm *sp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	LPFC_MBOXQ_t *mbox;
	int rc;

	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~(FC_FABRIC | FC_PUBLIC_LOOP);
	spin_unlock_irq(shost->host_lock);

	phba->fc_edtov = FF_DEF_EDTOV;
	phba->fc_ratov = FF_DEF_RATOV;
	rc = memcmp(&vport->fc_portname, &sp->portName,
		    sizeof(vport->fc_portname));
	if (rc >= 0) {
		
		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_PT2PT_PLOGI;
		spin_unlock_irq(shost->host_lock);


		
		if (rc)
			vport->fc_myDID = PT2PT_LocalID;

		mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
		if (!mbox)
			goto fail;

		lpfc_config_link(phba, mbox);

		mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		mbox->vport = vport;
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
		if (rc == MBX_NOT_FINISHED) {
			mempool_free(mbox, phba->mbox_mem_pool);
			goto fail;
		}
		lpfc_nlp_put(ndlp);

		ndlp = lpfc_findnode_did(vport, PT2PT_RemoteID);
		if (!ndlp) {
			ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
			if (!ndlp)
				goto fail;
			lpfc_nlp_init(vport, ndlp, PT2PT_RemoteID);
		} else if (!NLP_CHK_NODE_ACT(ndlp)) {
			ndlp = lpfc_enable_node(vport, ndlp,
						NLP_STE_UNUSED_NODE);
			if(!ndlp)
				goto fail;
		}

		memcpy(&ndlp->nlp_portname, &sp->portName,
		       sizeof(struct lpfc_name));
		memcpy(&ndlp->nlp_nodename, &sp->nodeName,
		       sizeof(struct lpfc_name));
		
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irq(shost->host_lock);
	} else
		lpfc_nlp_put(ndlp);

	
	phba->sli3_options &= ~LPFC_SLI3_NPIV_ENABLED;

	spin_lock_irq(shost->host_lock);
	vport->fc_flag |= FC_PT2PT;
	spin_unlock_irq(shost->host_lock);

	
	lpfc_disc_start(vport);
	return 0;
fail:
	return -ENXIO;
}

static void
lpfc_cmpl_els_flogi(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		    struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp = &rspiocb->iocb;
	struct lpfc_nodelist *ndlp = cmdiocb->context1;
	struct lpfc_dmabuf *pcmd = cmdiocb->context2, *prsp;
	struct serv_parm *sp;
	uint16_t fcf_index;
	int rc;

	
	if (lpfc_els_chk_latt(vport)) {
		lpfc_nlp_put(ndlp);
		goto out;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"FLOGI cmpl:      status:x%x/x%x state:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		vport->port_state);

	if (irsp->ulpStatus) {
		if ((phba->hba_flag & HBA_FIP_SUPPORT) &&
		    (phba->fcf.fcf_flag & FCF_DISCOVERY)) {
			if (phba->link_state < LPFC_LINK_UP)
				goto stop_rr_fcf_flogi;
			if ((phba->fcoe_cvl_eventtag_attn ==
			     phba->fcoe_cvl_eventtag) &&
			    (irsp->ulpStatus == IOSTAT_LOCAL_REJECT) &&
			    (irsp->un.ulpWord[4] == IOERR_SLI_ABORTED))
				goto stop_rr_fcf_flogi;
			else
				phba->fcoe_cvl_eventtag_attn =
					phba->fcoe_cvl_eventtag;
			lpfc_printf_log(phba, KERN_WARNING, LOG_FIP | LOG_ELS,
					"2611 FLOGI failed on FCF (x%x), "
					"status:x%x/x%x, tmo:x%x, perform "
					"roundrobin FCF failover\n",
					phba->fcf.current_rec.fcf_indx,
					irsp->ulpStatus, irsp->un.ulpWord[4],
					irsp->ulpTimeout);
			lpfc_sli4_set_fcf_flogi_fail(phba,
					phba->fcf.current_rec.fcf_indx);
			fcf_index = lpfc_sli4_fcf_rr_next_index_get(phba);
			rc = lpfc_sli4_fcf_rr_next_proc(vport, fcf_index);
			if (rc)
				goto out;
		}

stop_rr_fcf_flogi:
		
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				"2858 FLOGI failure Status:x%x/x%x TMO:x%x\n",
				irsp->ulpStatus, irsp->un.ulpWord[4],
				irsp->ulpTimeout);

		
		if (lpfc_els_retry(phba, cmdiocb, rspiocb))
			goto out;

		
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "0100 FLOGI failure Status:x%x/x%x TMO:x%x\n",
				 irsp->ulpStatus, irsp->un.ulpWord[4],
				 irsp->ulpTimeout);

		
		spin_lock_irq(shost->host_lock);
		vport->fc_flag &= ~(FC_FABRIC | FC_PUBLIC_LOOP);
		spin_unlock_irq(shost->host_lock);

		if (phba->alpa_map[0] == 0)
			vport->cfg_discovery_threads = LPFC_MAX_DISC_THREADS;
		if ((phba->sli_rev == LPFC_SLI_REV4) &&
		    (!(vport->fc_flag & FC_VFI_REGISTERED) ||
		     (vport->fc_prevDID != vport->fc_myDID))) {
			if (vport->fc_flag & FC_VFI_REGISTERED)
				lpfc_sli4_unreg_all_rpis(vport);
			lpfc_issue_reg_vfi(vport);
			lpfc_nlp_put(ndlp);
			goto out;
		}
		goto flogifail;
	}
	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~FC_VPORT_CVL_RCVD;
	vport->fc_flag &= ~FC_VPORT_LOGO_RCVD;
	spin_unlock_irq(shost->host_lock);

	prsp = list_get_first(&pcmd->list, struct lpfc_dmabuf, list);

	sp = prsp->virt + sizeof(uint32_t);

	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0101 FLOGI completes successfully "
			 "Data: x%x x%x x%x x%x\n",
			 irsp->un.ulpWord[4], sp->cmn.e_d_tov,
			 sp->cmn.w2.r_a_tov, sp->cmn.edtovResolution);

	if (vport->port_state == LPFC_FLOGI) {
		if (sp->cmn.fPort)
			rc = lpfc_cmpl_els_flogi_fabric(vport, ndlp, sp, irsp);
		else if (!(phba->hba_flag & HBA_FCOE_MODE))
			rc = lpfc_cmpl_els_flogi_nport(vport, ndlp, sp);
		else {
			lpfc_printf_vlog(vport, KERN_ERR,
				LOG_FIP | LOG_ELS,
				"2831 FLOGI response with cleared Fabric "
				"bit fcf_index 0x%x "
				"Switch Name %02x%02x%02x%02x%02x%02x%02x%02x "
				"Fabric Name "
				"%02x%02x%02x%02x%02x%02x%02x%02x\n",
				phba->fcf.current_rec.fcf_indx,
				phba->fcf.current_rec.switch_name[0],
				phba->fcf.current_rec.switch_name[1],
				phba->fcf.current_rec.switch_name[2],
				phba->fcf.current_rec.switch_name[3],
				phba->fcf.current_rec.switch_name[4],
				phba->fcf.current_rec.switch_name[5],
				phba->fcf.current_rec.switch_name[6],
				phba->fcf.current_rec.switch_name[7],
				phba->fcf.current_rec.fabric_name[0],
				phba->fcf.current_rec.fabric_name[1],
				phba->fcf.current_rec.fabric_name[2],
				phba->fcf.current_rec.fabric_name[3],
				phba->fcf.current_rec.fabric_name[4],
				phba->fcf.current_rec.fabric_name[5],
				phba->fcf.current_rec.fabric_name[6],
				phba->fcf.current_rec.fabric_name[7]);
			lpfc_nlp_put(ndlp);
			spin_lock_irq(&phba->hbalock);
			phba->fcf.fcf_flag &= ~FCF_DISCOVERY;
			phba->hba_flag &= ~(FCF_RR_INPROG | HBA_DEVLOSS_TMO);
			spin_unlock_irq(&phba->hbalock);
			goto out;
		}
		if (!rc) {
			
			if (phba->hba_flag & HBA_FIP_SUPPORT)
				lpfc_printf_vlog(vport, KERN_INFO, LOG_FIP |
						LOG_ELS,
						"2769 FLOGI to FCF (x%x) "
						"completed successfully\n",
						phba->fcf.current_rec.fcf_indx);
			spin_lock_irq(&phba->hbalock);
			phba->fcf.fcf_flag &= ~FCF_DISCOVERY;
			phba->hba_flag &= ~(FCF_RR_INPROG | HBA_DEVLOSS_TMO);
			spin_unlock_irq(&phba->hbalock);
			goto out;
		}
	}

flogifail:
	lpfc_nlp_put(ndlp);

	if (!lpfc_error_lost_link(irsp)) {
		
		lpfc_disc_list_loopmap(vport);

		
		lpfc_disc_start(vport);
	} else if (((irsp->ulpStatus != IOSTAT_LOCAL_REJECT) ||
			((irsp->un.ulpWord[4] != IOERR_SLI_ABORTED) &&
			(irsp->un.ulpWord[4] != IOERR_SLI_DOWN))) &&
			(phba->link_state != LPFC_CLEAR_LA)) {
		
		lpfc_issue_clear_la(phba, vport);
	}
out:
	lpfc_els_free_iocb(phba, cmdiocb);
}

static int
lpfc_issue_els_flogi(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		     uint8_t retry)
{
	struct lpfc_hba  *phba = vport->phba;
	struct serv_parm *sp;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli_ring *pring;
	uint8_t *pcmd;
	uint16_t cmdsize;
	uint32_t tmo;
	int rc;

	pring = &phba->sli.ring[LPFC_ELS_RING];

	cmdsize = (sizeof(uint32_t) + sizeof(struct serv_parm));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_FLOGI);

	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	
	*((uint32_t *) (pcmd)) = ELS_CMD_FLOGI;
	pcmd += sizeof(uint32_t);
	memcpy(pcmd, &vport->fc_sparam, sizeof(struct serv_parm));
	sp = (struct serv_parm *) pcmd;

	
	sp->cmn.e_d_tov = 0;
	sp->cmn.w2.r_a_tov = 0;
	sp->cmn.virtual_fabric_support = 0;
	sp->cls1.classValid = 0;
	sp->cls2.seqDelivery = 1;
	sp->cls3.seqDelivery = 1;
	if (sp->cmn.fcphLow < FC_PH3)
		sp->cmn.fcphLow = FC_PH3;
	if (sp->cmn.fcphHigh < FC_PH3)
		sp->cmn.fcphHigh = FC_PH3;

	if  (phba->sli_rev == LPFC_SLI_REV4) {
		if (bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) ==
		    LPFC_SLI_INTF_IF_TYPE_0) {
			elsiocb->iocb.ulpCt_h = ((SLI4_CT_FCFI >> 1) & 1);
			elsiocb->iocb.ulpCt_l = (SLI4_CT_FCFI & 1);
			
			
			elsiocb->iocb.ulpContext = phba->fcf.fcfi;
		}
	} else {
		if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) {
			sp->cmn.request_multiple_Nport = 1;
			
			icmd->ulpCt_h = 1;
			icmd->ulpCt_l = 0;
		} else
			sp->cmn.request_multiple_Nport = 0;
	}

	if (phba->fc_topology != LPFC_TOPOLOGY_LOOP) {
		icmd->un.elsreq64.myID = 0;
		icmd->un.elsreq64.fl = 1;
	}

	tmo = phba->fc_ratov;
	phba->fc_ratov = LPFC_DISC_FLOGI_TMO;
	lpfc_set_disctmo(vport);
	phba->fc_ratov = tmo;

	phba->fc_stat.elsXmitFLOGI++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_flogi;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue FLOGI:     opt:x%x",
		phba->sli3_options, 0, 0);

	rc = lpfc_issue_fabric_iocb(phba, elsiocb);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

int
lpfc_els_abort_flogi(struct lpfc_hba *phba)
{
	struct lpfc_sli_ring *pring;
	struct lpfc_iocbq *iocb, *next_iocb;
	struct lpfc_nodelist *ndlp;
	IOCB_t *icmd;

	
	lpfc_printf_log(phba, KERN_INFO, LOG_DISCOVERY,
			"0201 Abort outstanding I/O on NPort x%x\n",
			Fabric_DID);

	pring = &phba->sli.ring[LPFC_ELS_RING];

	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(iocb, next_iocb, &pring->txcmplq, list) {
		icmd = &iocb->iocb;
		if (icmd->ulpCommand == CMD_ELS_REQUEST64_CR) {
			ndlp = (struct lpfc_nodelist *)(iocb->context1);
			if (ndlp && NLP_CHK_NODE_ACT(ndlp) &&
			    (ndlp->nlp_DID == Fabric_DID))
				lpfc_sli_issue_abort_iotag(phba, pring, iocb);
		}
	}
	spin_unlock_irq(&phba->hbalock);

	return 0;
}

int
lpfc_initial_flogi(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_nodelist *ndlp;

	vport->port_state = LPFC_FLOGI;
	lpfc_set_disctmo(vport);

	
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

	if (lpfc_issue_els_flogi(vport, ndlp, 0)) {
		lpfc_nlp_put(ndlp);
		return 0;
	}
	return 1;
}

int
lpfc_initial_fdisc(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_nodelist *ndlp;

	
	ndlp = lpfc_findnode_did(vport, Fabric_DID);
	if (!ndlp) {
		
		ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
		if (!ndlp)
			return 0;
		lpfc_nlp_init(vport, ndlp, Fabric_DID);
		
		lpfc_enqueue_node(vport, ndlp);
	} else if (!NLP_CHK_NODE_ACT(ndlp)) {
		
		ndlp = lpfc_enable_node(vport, ndlp, NLP_STE_UNUSED_NODE);
		if (!ndlp)
			return 0;
	}

	if (lpfc_issue_els_fdisc(vport, ndlp, 0)) {
		lpfc_nlp_put(ndlp);
		return 0;
	}
	return 1;
}

void
lpfc_more_plogi(struct lpfc_vport *vport)
{
	int sentplogi;

	if (vport->num_disc_nodes)
		vport->num_disc_nodes--;

	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0232 Continue discovery with %d PLOGIs to go "
			 "Data: x%x x%x x%x\n",
			 vport->num_disc_nodes, vport->fc_plogi_cnt,
			 vport->fc_flag, vport->port_state);
	
	if (vport->fc_flag & FC_NLP_MORE)
		
		sentplogi = lpfc_els_disc_plogi(vport);

	return;
}

static struct lpfc_nodelist *
lpfc_plogi_confirm_nport(struct lpfc_hba *phba, uint32_t *prsp,
			 struct lpfc_nodelist *ndlp)
{
	struct lpfc_vport    *vport = ndlp->vport;
	struct lpfc_nodelist *new_ndlp;
	struct lpfc_rport_data *rdata;
	struct fc_rport *rport;
	struct serv_parm *sp;
	uint8_t  name[sizeof(struct lpfc_name)];
	uint32_t rc, keepDID = 0;
	int  put_node;
	int  put_rport;
	struct lpfc_node_rrqs rrq;

	if (ndlp->nlp_type & NLP_FABRIC)
		return ndlp;

	sp = (struct serv_parm *) ((uint8_t *) prsp + sizeof(uint32_t));
	memset(name, 0, sizeof(struct lpfc_name));

	new_ndlp = lpfc_findnode_wwpn(vport, &sp->portName);

	if (new_ndlp == ndlp && NLP_CHK_NODE_ACT(new_ndlp))
		return ndlp;
	memset(&rrq.xri_bitmap, 0, sizeof(new_ndlp->active_rrqs.xri_bitmap));

	if (!new_ndlp) {
		rc = memcmp(&ndlp->nlp_portname, name,
			    sizeof(struct lpfc_name));
		if (!rc)
			return ndlp;
		new_ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_ATOMIC);
		if (!new_ndlp)
			return ndlp;
		lpfc_nlp_init(vport, new_ndlp, ndlp->nlp_DID);
	} else if (!NLP_CHK_NODE_ACT(new_ndlp)) {
		rc = memcmp(&ndlp->nlp_portname, name,
			    sizeof(struct lpfc_name));
		if (!rc)
			return ndlp;
		new_ndlp = lpfc_enable_node(vport, new_ndlp,
						NLP_STE_UNUSED_NODE);
		if (!new_ndlp)
			return ndlp;
		keepDID = new_ndlp->nlp_DID;
		if (phba->sli_rev == LPFC_SLI_REV4)
			memcpy(&rrq.xri_bitmap,
				&new_ndlp->active_rrqs.xri_bitmap,
				sizeof(new_ndlp->active_rrqs.xri_bitmap));
	} else {
		keepDID = new_ndlp->nlp_DID;
		if (phba->sli_rev == LPFC_SLI_REV4)
			memcpy(&rrq.xri_bitmap,
				&new_ndlp->active_rrqs.xri_bitmap,
				sizeof(new_ndlp->active_rrqs.xri_bitmap));
	}

	lpfc_unreg_rpi(vport, new_ndlp);
	new_ndlp->nlp_DID = ndlp->nlp_DID;
	new_ndlp->nlp_prev_state = ndlp->nlp_prev_state;
	if (phba->sli_rev == LPFC_SLI_REV4)
		memcpy(new_ndlp->active_rrqs.xri_bitmap,
			&ndlp->active_rrqs.xri_bitmap,
			sizeof(ndlp->active_rrqs.xri_bitmap));

	if (ndlp->nlp_flag & NLP_NPR_2B_DISC)
		new_ndlp->nlp_flag |= NLP_NPR_2B_DISC;
	ndlp->nlp_flag &= ~NLP_NPR_2B_DISC;

	
	lpfc_nlp_set_state(vport, new_ndlp, ndlp->nlp_state);

	
	if (memcmp(&ndlp->nlp_portname, name, sizeof(struct lpfc_name)) == 0) {

		
		rport =  ndlp->rport;
		if (rport) {
			rdata = rport->dd_data;
			if (rdata->pnode == ndlp) {
				lpfc_nlp_put(ndlp);
				ndlp->rport = NULL;
				rdata->pnode = lpfc_nlp_get(new_ndlp);
				new_ndlp->rport = rport;
			}
			new_ndlp->nlp_type = ndlp->nlp_type;
		}
		if (ndlp->nlp_DID == 0) {
			spin_lock_irq(&phba->ndlp_lock);
			NLP_SET_FREE_REQ(ndlp);
			spin_unlock_irq(&phba->ndlp_lock);
		}

		
		ndlp->nlp_DID = keepDID;
		if (phba->sli_rev == LPFC_SLI_REV4)
			memcpy(&ndlp->active_rrqs.xri_bitmap,
				&rrq.xri_bitmap,
				sizeof(ndlp->active_rrqs.xri_bitmap));
		lpfc_drop_node(vport, ndlp);
	}
	else {
		lpfc_unreg_rpi(vport, ndlp);
		
		ndlp->nlp_DID = keepDID;
		if (phba->sli_rev == LPFC_SLI_REV4)
			memcpy(&ndlp->active_rrqs.xri_bitmap,
				&rrq.xri_bitmap,
				sizeof(ndlp->active_rrqs.xri_bitmap));
		memcpy(&new_ndlp->nlp_portname, &ndlp->nlp_portname,
			sizeof(struct lpfc_name));
		memcpy(&new_ndlp->nlp_nodename, &ndlp->nlp_nodename,
			sizeof(struct lpfc_name));
		new_ndlp->nlp_state = ndlp->nlp_state;
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		
		rport = ndlp->rport;
		if (rport) {
			rdata = rport->dd_data;
			put_node = rdata->pnode != NULL;
			put_rport = ndlp->rport != NULL;
			rdata->pnode = NULL;
			ndlp->rport = NULL;
			if (put_node)
				lpfc_nlp_put(ndlp);
			if (put_rport)
				put_device(&rport->dev);
		}
	}
	return new_ndlp;
}

void
lpfc_end_rscn(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	if (vport->fc_flag & FC_RSCN_MODE) {
		if (vport->fc_rscn_id_cnt ||
		    (vport->fc_flag & FC_RSCN_DISCOVERY) != 0)
			lpfc_els_handle_rscn(vport);
		else {
			spin_lock_irq(shost->host_lock);
			vport->fc_flag &= ~FC_RSCN_MODE;
			spin_unlock_irq(shost->host_lock);
		}
	}
}


static void
lpfc_cmpl_els_rrq(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		    struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	IOCB_t *irsp;
	struct lpfc_nodelist *ndlp;
	struct lpfc_node_rrq *rrq;

	
	rrq = cmdiocb->context_un.rrq;
	cmdiocb->context_un.rsp_iocb = rspiocb;

	irsp = &rspiocb->iocb;
	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"RRQ cmpl:      status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		irsp->un.elsreq64.remoteID);

	ndlp = lpfc_findnode_did(vport, irsp->un.elsreq64.remoteID);
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp) || ndlp != rrq->ndlp) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "2882 RRQ completes to NPort x%x "
				 "with no ndlp. Data: x%x x%x x%x\n",
				 irsp->un.elsreq64.remoteID,
				 irsp->ulpStatus, irsp->un.ulpWord[4],
				 irsp->ulpIoTag);
		goto out;
	}

	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "2880 RRQ completes to NPort x%x "
			 "Data: x%x x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, irsp->ulpStatus, irsp->un.ulpWord[4],
			 irsp->ulpTimeout, rrq->xritag, rrq->rxid);

	if (irsp->ulpStatus) {
		
		
		if (irsp->ulpStatus != IOSTAT_LS_RJT ||
			(((irsp->un.ulpWord[4]) >> 16 != LSRJT_INVALID_CMD) &&
			((irsp->un.ulpWord[4]) >> 16 != LSRJT_UNABLE_TPC)) ||
			(phba)->pport->cfg_log_verbose & LOG_ELS)
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "2881 RRQ failure DID:%06X Status:x%x/x%x\n",
				 ndlp->nlp_DID, irsp->ulpStatus,
				 irsp->un.ulpWord[4]);
	}
out:
	if (rrq)
		lpfc_clr_rrq_active(phba, rrq->xritag, rrq);
	lpfc_els_free_iocb(phba, cmdiocb);
	return;
}
static void
lpfc_cmpl_els_plogi(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		    struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp;
	struct lpfc_nodelist *ndlp;
	struct lpfc_dmabuf *prsp;
	int disc, rc, did, type;

	
	cmdiocb->context_un.rsp_iocb = rspiocb;

	irsp = &rspiocb->iocb;
	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"PLOGI cmpl:      status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		irsp->un.elsreq64.remoteID);

	ndlp = lpfc_findnode_did(vport, irsp->un.elsreq64.remoteID);
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp)) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "0136 PLOGI completes to NPort x%x "
				 "with no ndlp. Data: x%x x%x x%x\n",
				 irsp->un.elsreq64.remoteID,
				 irsp->ulpStatus, irsp->un.ulpWord[4],
				 irsp->ulpIoTag);
		goto out;
	}

	spin_lock_irq(shost->host_lock);
	disc = (ndlp->nlp_flag & NLP_NPR_2B_DISC);
	ndlp->nlp_flag &= ~NLP_NPR_2B_DISC;
	spin_unlock_irq(shost->host_lock);
	rc   = 0;

	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0102 PLOGI completes to NPort x%x "
			 "Data: x%x x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, irsp->ulpStatus, irsp->un.ulpWord[4],
			 irsp->ulpTimeout, disc, vport->num_disc_nodes);
	
	if (lpfc_els_chk_latt(vport)) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irq(shost->host_lock);
		goto out;
	}

	
	type = ndlp->nlp_type;
	did = ndlp->nlp_DID;

	if (irsp->ulpStatus) {
		
		if (lpfc_els_retry(phba, cmdiocb, rspiocb)) {
			
			if (disc) {
				spin_lock_irq(shost->host_lock);
				ndlp->nlp_flag |= NLP_NPR_2B_DISC;
				spin_unlock_irq(shost->host_lock);
			}
			goto out;
		}
		
		if (irsp->ulpStatus != IOSTAT_LS_RJT ||
			(((irsp->un.ulpWord[4]) >> 16 != LSRJT_INVALID_CMD) &&
			((irsp->un.ulpWord[4]) >> 16 != LSRJT_UNABLE_TPC)) ||
			(phba)->pport->cfg_log_verbose & LOG_ELS)
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "2753 PLOGI failure DID:%06X Status:x%x/x%x\n",
				 ndlp->nlp_DID, irsp->ulpStatus,
				 irsp->un.ulpWord[4]);
		
		if (lpfc_error_lost_link(irsp))
			rc = NLP_STE_FREED_NODE;
		else
			rc = lpfc_disc_state_machine(vport, ndlp, cmdiocb,
						     NLP_EVT_CMPL_PLOGI);
	} else {
		
		prsp = list_entry(((struct lpfc_dmabuf *)
				   cmdiocb->context2)->list.next,
				  struct lpfc_dmabuf, list);
		ndlp = lpfc_plogi_confirm_nport(phba, prsp->virt, ndlp);
		rc = lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					     NLP_EVT_CMPL_PLOGI);
	}

	if (disc && vport->num_disc_nodes) {
		
		lpfc_more_plogi(vport);

		if (vport->num_disc_nodes == 0) {
			spin_lock_irq(shost->host_lock);
			vport->fc_flag &= ~FC_NDISC_ACTIVE;
			spin_unlock_irq(shost->host_lock);

			lpfc_can_disctmo(vport);
			lpfc_end_rscn(vport);
		}
	}

out:
	lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

int
lpfc_issue_els_plogi(struct lpfc_vport *vport, uint32_t did, uint8_t retry)
{
	struct lpfc_hba  *phba = vport->phba;
	struct serv_parm *sp;
	IOCB_t *icmd;
	struct lpfc_nodelist *ndlp;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli *psli;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int ret;

	psli = &phba->sli;

	ndlp = lpfc_findnode_did(vport, did);
	if (ndlp && !NLP_CHK_NODE_ACT(ndlp))
		ndlp = NULL;

	
	cmdsize = (sizeof(uint32_t) + sizeof(struct serv_parm));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp, did,
				     ELS_CMD_PLOGI);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	
	*((uint32_t *) (pcmd)) = ELS_CMD_PLOGI;
	pcmd += sizeof(uint32_t);
	memcpy(pcmd, &vport->fc_sparam, sizeof(struct serv_parm));
	sp = (struct serv_parm *) pcmd;

	if ((vport->fc_flag & FC_FABRIC) && !(vport->fc_flag & FC_PUBLIC_LOOP))
		sp->cmn.altBbCredit = 1;

	if (sp->cmn.fcphLow < FC_PH_4_3)
		sp->cmn.fcphLow = FC_PH_4_3;

	if (sp->cmn.fcphHigh < FC_PH3)
		sp->cmn.fcphHigh = FC_PH3;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue PLOGI:     did:x%x",
		did, 0, 0);

	phba->fc_stat.elsXmitPLOGI++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_plogi;
	ret = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);

	if (ret == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

static void
lpfc_cmpl_els_prli(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		   struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp;
	struct lpfc_sli *psli;
	struct lpfc_nodelist *ndlp;

	psli = &phba->sli;
	
	cmdiocb->context_un.rsp_iocb = rspiocb;

	irsp = &(rspiocb->iocb);
	ndlp = (struct lpfc_nodelist *) cmdiocb->context1;
	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag &= ~NLP_PRLI_SND;
	spin_unlock_irq(shost->host_lock);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"PRLI cmpl:       status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		ndlp->nlp_DID);
	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0103 PRLI completes to NPort x%x "
			 "Data: x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, irsp->ulpStatus, irsp->un.ulpWord[4],
			 irsp->ulpTimeout, vport->num_disc_nodes);

	vport->fc_prli_sent--;
	
	if (lpfc_els_chk_latt(vport))
		goto out;

	if (irsp->ulpStatus) {
		
		if (lpfc_els_retry(phba, cmdiocb, rspiocb)) {
			
			goto out;
		}
		
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "2754 PRLI failure DID:%06X Status:x%x/x%x\n",
				 ndlp->nlp_DID, irsp->ulpStatus,
				 irsp->un.ulpWord[4]);
		
		if (lpfc_error_lost_link(irsp))
			goto out;
		else
			lpfc_disc_state_machine(vport, ndlp, cmdiocb,
						NLP_EVT_CMPL_PRLI);
	} else
		
		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_CMPL_PRLI);
out:
	lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

int
lpfc_issue_els_prli(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		    uint8_t retry)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba *phba = vport->phba;
	PRLI *npr;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;

	cmdsize = (sizeof(uint32_t) + sizeof(PRLI));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_PRLI);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	
	memset(pcmd, 0, (sizeof(PRLI) + sizeof(uint32_t)));
	*((uint32_t *) (pcmd)) = ELS_CMD_PRLI;
	pcmd += sizeof(uint32_t);

	
	npr = (PRLI *) pcmd;
	if (phba->vpd.rev.feaLevelHigh >= 0x02) {
		npr->ConfmComplAllowed = 1;
		npr->Retry = 1;
		npr->TaskRetryIdReq = 1;
	}
	npr->estabImagePair = 1;
	npr->readXferRdyDis = 1;

	
	npr->prliType = PRLI_FCP_TYPE;
	npr->initiatorFunc = 1;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue PRLI:      did:x%x",
		ndlp->nlp_DID, 0, 0);

	phba->fc_stat.elsXmitPRLI++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_prli;
	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag |= NLP_PRLI_SND;
	spin_unlock_irq(shost->host_lock);
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) ==
	    IOCB_ERROR) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag &= ~NLP_PRLI_SND;
		spin_unlock_irq(shost->host_lock);
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	vport->fc_prli_sent++;
	return 0;
}

static void
lpfc_rscn_disc(struct lpfc_vport *vport)
{
	lpfc_can_disctmo(vport);

	
	
	if (vport->fc_npr_cnt)
		if (lpfc_els_disc_plogi(vport))
			return;

	lpfc_end_rscn(vport);
}

static void
lpfc_adisc_done(struct lpfc_vport *vport)
{
	struct Scsi_Host   *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba   *phba = vport->phba;

	if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
	    !(vport->fc_flag & FC_RSCN_MODE) &&
	    (phba->sli_rev < LPFC_SLI_REV4)) {
		lpfc_issue_reg_vpi(phba, vport);
		return;
	}
	if (vport->port_state < LPFC_VPORT_READY) {
		
		if (vport->port_type == LPFC_PHYSICAL_PORT)
			lpfc_issue_clear_la(phba, vport);
		if (!(vport->fc_flag & FC_ABORT_DISCOVERY)) {
			vport->num_disc_nodes = 0;
			
			if (vport->fc_npr_cnt)
				lpfc_els_disc_plogi(vport);
			if (!vport->num_disc_nodes) {
				spin_lock_irq(shost->host_lock);
				vport->fc_flag &= ~FC_NDISC_ACTIVE;
				spin_unlock_irq(shost->host_lock);
				lpfc_can_disctmo(vport);
				lpfc_end_rscn(vport);
			}
		}
		vport->port_state = LPFC_VPORT_READY;
	} else
		lpfc_rscn_disc(vport);
}

void
lpfc_more_adisc(struct lpfc_vport *vport)
{
	int sentadisc;

	if (vport->num_disc_nodes)
		vport->num_disc_nodes--;
	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0210 Continue discovery with %d ADISCs to go "
			 "Data: x%x x%x x%x\n",
			 vport->num_disc_nodes, vport->fc_adisc_cnt,
			 vport->fc_flag, vport->port_state);
	
	if (vport->fc_flag & FC_NLP_MORE) {
		lpfc_set_disctmo(vport);
		
		sentadisc = lpfc_els_disc_adisc(vport);
	}
	if (!vport->num_disc_nodes)
		lpfc_adisc_done(vport);
	return;
}

static void
lpfc_cmpl_els_adisc(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		    struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp;
	struct lpfc_nodelist *ndlp;
	int  disc;

	
	cmdiocb->context_un.rsp_iocb = rspiocb;

	irsp = &(rspiocb->iocb);
	ndlp = (struct lpfc_nodelist *) cmdiocb->context1;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"ADISC cmpl:      status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		ndlp->nlp_DID);

	spin_lock_irq(shost->host_lock);
	disc = (ndlp->nlp_flag & NLP_NPR_2B_DISC);
	ndlp->nlp_flag &= ~(NLP_ADISC_SND | NLP_NPR_2B_DISC);
	spin_unlock_irq(shost->host_lock);
	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0104 ADISC completes to NPort x%x "
			 "Data: x%x x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, irsp->ulpStatus, irsp->un.ulpWord[4],
			 irsp->ulpTimeout, disc, vport->num_disc_nodes);
	
	if (lpfc_els_chk_latt(vport)) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irq(shost->host_lock);
		goto out;
	}

	if (irsp->ulpStatus) {
		
		if (lpfc_els_retry(phba, cmdiocb, rspiocb)) {
			
			if (disc) {
				spin_lock_irq(shost->host_lock);
				ndlp->nlp_flag |= NLP_NPR_2B_DISC;
				spin_unlock_irq(shost->host_lock);
				lpfc_set_disctmo(vport);
			}
			goto out;
		}
		
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "2755 ADISC failure DID:%06X Status:x%x/x%x\n",
				 ndlp->nlp_DID, irsp->ulpStatus,
				 irsp->un.ulpWord[4]);
		
		if (!lpfc_error_lost_link(irsp))
			lpfc_disc_state_machine(vport, ndlp, cmdiocb,
						NLP_EVT_CMPL_ADISC);
	} else
		
		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_CMPL_ADISC);

	
	if (disc && vport->num_disc_nodes)
		lpfc_more_adisc(vport);
out:
	lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

int
lpfc_issue_els_adisc(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		     uint8_t retry)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	ADISC *ap;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;

	cmdsize = (sizeof(uint32_t) + sizeof(ADISC));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ADISC);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	
	*((uint32_t *) (pcmd)) = ELS_CMD_ADISC;
	pcmd += sizeof(uint32_t);

	
	ap = (ADISC *) pcmd;
	ap->hardAL_PA = phba->fc_pref_ALPA;
	memcpy(&ap->portName, &vport->fc_portname, sizeof(struct lpfc_name));
	memcpy(&ap->nodeName, &vport->fc_nodename, sizeof(struct lpfc_name));
	ap->DID = be32_to_cpu(vport->fc_myDID);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue ADISC:     did:x%x",
		ndlp->nlp_DID, 0, 0);

	phba->fc_stat.elsXmitADISC++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_adisc;
	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag |= NLP_ADISC_SND;
	spin_unlock_irq(shost->host_lock);
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) ==
	    IOCB_ERROR) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag &= ~NLP_ADISC_SND;
		spin_unlock_irq(shost->host_lock);
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

static void
lpfc_cmpl_els_logo(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		   struct lpfc_iocbq *rspiocb)
{
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) cmdiocb->context1;
	struct lpfc_vport *vport = ndlp->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp;
	struct lpfc_sli *psli;
	struct lpfcMboxq *mbox;

	psli = &phba->sli;
	
	cmdiocb->context_un.rsp_iocb = rspiocb;

	irsp = &(rspiocb->iocb);
	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag &= ~NLP_LOGO_SND;
	spin_unlock_irq(shost->host_lock);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"LOGO cmpl:       status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		ndlp->nlp_DID);
	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0105 LOGO completes to NPort x%x "
			 "Data: x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, irsp->ulpStatus, irsp->un.ulpWord[4],
			 irsp->ulpTimeout, vport->num_disc_nodes);
	
	if (lpfc_els_chk_latt(vport))
		goto out;

	if (ndlp->nlp_flag & NLP_TARGET_REMOVE) {
		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_DEVICE_RM);
		goto out;
	}

	if (irsp->ulpStatus) {
		
		if (lpfc_els_retry(phba, cmdiocb, rspiocb))
			
			goto out;
		
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "2756 LOGO failure DID:%06X Status:x%x/x%x\n",
				 ndlp->nlp_DID, irsp->ulpStatus,
				 irsp->un.ulpWord[4]);
		
		if (lpfc_error_lost_link(irsp))
			goto out;
		else
			lpfc_disc_state_machine(vport, ndlp, cmdiocb,
						NLP_EVT_CMPL_LOGO);
	} else
		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_CMPL_LOGO);
out:
	lpfc_els_free_iocb(phba, cmdiocb);
	
	if ((vport->fc_flag & FC_PT2PT) &&
		!(vport->fc_flag & FC_PT2PT_PLOGI)) {
		phba->pport->fc_myDID = 0;
		mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
		if (mbox) {
			lpfc_config_link(phba, mbox);
			mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
			mbox->vport = vport;
			if (lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT) ==
				MBX_NOT_FINISHED) {
				mempool_free(mbox, phba->mbox_mem_pool);
			}
		}
	}
	return;
}

int
lpfc_issue_els_logo(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		    uint8_t retry)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int rc;

	spin_lock_irq(shost->host_lock);
	if (ndlp->nlp_flag & NLP_LOGO_SND) {
		spin_unlock_irq(shost->host_lock);
		return 0;
	}
	spin_unlock_irq(shost->host_lock);

	cmdsize = (2 * sizeof(uint32_t)) + sizeof(struct lpfc_name);
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_LOGO);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);
	*((uint32_t *) (pcmd)) = ELS_CMD_LOGO;
	pcmd += sizeof(uint32_t);

	
	*((uint32_t *) (pcmd)) = be32_to_cpu(vport->fc_myDID);
	pcmd += sizeof(uint32_t);
	memcpy(pcmd, &vport->fc_portname, sizeof(struct lpfc_name));

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue LOGO:      did:x%x",
		ndlp->nlp_DID, 0, 0);

	phba->fc_stat.elsXmitLOGO++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_logo;
	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag |= NLP_LOGO_SND;
	spin_unlock_irq(shost->host_lock);
	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);

	if (rc == IOCB_ERROR) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag &= ~NLP_LOGO_SND;
		spin_unlock_irq(shost->host_lock);
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

static void
lpfc_cmpl_els_cmd(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	IOCB_t *irsp;

	irsp = &rspiocb->iocb;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"ELS cmd cmpl:    status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		irsp->un.elsreq64.remoteID);
	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0106 ELS cmd tag x%x completes Data: x%x x%x x%x\n",
			 irsp->ulpIoTag, irsp->ulpStatus,
			 irsp->un.ulpWord[4], irsp->ulpTimeout);
	
	lpfc_els_chk_latt(vport);
	lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

int
lpfc_issue_els_scr(struct lpfc_vport *vport, uint32_t nportid, uint8_t retry)
{
	struct lpfc_hba  *phba = vport->phba;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli *psli;
	uint8_t *pcmd;
	uint16_t cmdsize;
	struct lpfc_nodelist *ndlp;

	psli = &phba->sli;
	cmdsize = (sizeof(uint32_t) + sizeof(SCR));

	ndlp = lpfc_findnode_did(vport, nportid);
	if (!ndlp) {
		ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
		if (!ndlp)
			return 1;
		lpfc_nlp_init(vport, ndlp, nportid);
		lpfc_enqueue_node(vport, ndlp);
	} else if (!NLP_CHK_NODE_ACT(ndlp)) {
		ndlp = lpfc_enable_node(vport, ndlp, NLP_STE_UNUSED_NODE);
		if (!ndlp)
			return 1;
	}

	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_SCR);

	if (!elsiocb) {
		lpfc_nlp_put(ndlp);
		return 1;
	}

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	*((uint32_t *) (pcmd)) = ELS_CMD_SCR;
	pcmd += sizeof(uint32_t);

	
	memset(pcmd, 0, sizeof(SCR));
	((SCR *) pcmd)->Function = SCR_FUNC_FULL;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue SCR:       did:x%x",
		ndlp->nlp_DID, 0, 0);

	phba->fc_stat.elsXmitSCR++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_cmd;
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) ==
	    IOCB_ERROR) {
		lpfc_nlp_put(ndlp);
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	lpfc_nlp_put(ndlp);
	return 0;
}

static int
lpfc_issue_els_farpr(struct lpfc_vport *vport, uint32_t nportid, uint8_t retry)
{
	struct lpfc_hba  *phba = vport->phba;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli *psli;
	FARP *fp;
	uint8_t *pcmd;
	uint32_t *lp;
	uint16_t cmdsize;
	struct lpfc_nodelist *ondlp;
	struct lpfc_nodelist *ndlp;

	psli = &phba->sli;
	cmdsize = (sizeof(uint32_t) + sizeof(FARP));

	ndlp = lpfc_findnode_did(vport, nportid);
	if (!ndlp) {
		ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
		if (!ndlp)
			return 1;
		lpfc_nlp_init(vport, ndlp, nportid);
		lpfc_enqueue_node(vport, ndlp);
	} else if (!NLP_CHK_NODE_ACT(ndlp)) {
		ndlp = lpfc_enable_node(vport, ndlp, NLP_STE_UNUSED_NODE);
		if (!ndlp)
			return 1;
	}

	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_RNID);
	if (!elsiocb) {
		lpfc_nlp_put(ndlp);
		return 1;
	}

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	*((uint32_t *) (pcmd)) = ELS_CMD_FARPR;
	pcmd += sizeof(uint32_t);

	
	fp = (FARP *) (pcmd);
	memset(fp, 0, sizeof(FARP));
	lp = (uint32_t *) pcmd;
	*lp++ = be32_to_cpu(nportid);
	*lp++ = be32_to_cpu(vport->fc_myDID);
	fp->Rflags = 0;
	fp->Mflags = (FARP_MATCH_PORT | FARP_MATCH_NODE);

	memcpy(&fp->RportName, &vport->fc_portname, sizeof(struct lpfc_name));
	memcpy(&fp->RnodeName, &vport->fc_nodename, sizeof(struct lpfc_name));
	ondlp = lpfc_findnode_did(vport, nportid);
	if (ondlp && NLP_CHK_NODE_ACT(ondlp)) {
		memcpy(&fp->OportName, &ondlp->nlp_portname,
		       sizeof(struct lpfc_name));
		memcpy(&fp->OnodeName, &ondlp->nlp_nodename,
		       sizeof(struct lpfc_name));
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue FARPR:     did:x%x",
		ndlp->nlp_DID, 0, 0);

	phba->fc_stat.elsXmitFARPR++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_cmd;
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) ==
	    IOCB_ERROR) {
		lpfc_nlp_put(ndlp);
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	lpfc_nlp_put(ndlp);
	return 0;
}

void
lpfc_cancel_retry_delay_tmo(struct lpfc_vport *vport, struct lpfc_nodelist *nlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_work_evt *evtp;

	if (!(nlp->nlp_flag & NLP_DELAY_TMO))
		return;
	spin_lock_irq(shost->host_lock);
	nlp->nlp_flag &= ~NLP_DELAY_TMO;
	spin_unlock_irq(shost->host_lock);
	del_timer_sync(&nlp->nlp_delayfunc);
	nlp->nlp_last_elscmd = 0;
	if (!list_empty(&nlp->els_retry_evt.evt_listp)) {
		list_del_init(&nlp->els_retry_evt.evt_listp);
		
		evtp = &nlp->els_retry_evt;
		lpfc_nlp_put((struct lpfc_nodelist *)evtp->evt_arg1);
	}
	if (nlp->nlp_flag & NLP_NPR_2B_DISC) {
		spin_lock_irq(shost->host_lock);
		nlp->nlp_flag &= ~NLP_NPR_2B_DISC;
		spin_unlock_irq(shost->host_lock);
		if (vport->num_disc_nodes) {
			if (vport->port_state < LPFC_VPORT_READY) {
				
				lpfc_more_adisc(vport);
			} else {
				
				lpfc_more_plogi(vport);
				if (vport->num_disc_nodes == 0) {
					spin_lock_irq(shost->host_lock);
					vport->fc_flag &= ~FC_NDISC_ACTIVE;
					spin_unlock_irq(shost->host_lock);
					lpfc_can_disctmo(vport);
					lpfc_end_rscn(vport);
				}
			}
		}
	}
	return;
}

void
lpfc_els_retry_delay(unsigned long ptr)
{
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) ptr;
	struct lpfc_vport *vport = ndlp->vport;
	struct lpfc_hba   *phba = vport->phba;
	unsigned long flags;
	struct lpfc_work_evt  *evtp = &ndlp->els_retry_evt;

	spin_lock_irqsave(&phba->hbalock, flags);
	if (!list_empty(&evtp->evt_listp)) {
		spin_unlock_irqrestore(&phba->hbalock, flags);
		return;
	}

	evtp->evt_arg1  = lpfc_nlp_get(ndlp);
	if (evtp->evt_arg1) {
		evtp->evt = LPFC_EVT_ELS_RETRY;
		list_add_tail(&evtp->evt_listp, &phba->work_list);
		lpfc_worker_wake_up(phba);
	}
	spin_unlock_irqrestore(&phba->hbalock, flags);
	return;
}

void
lpfc_els_retry_delay_handler(struct lpfc_nodelist *ndlp)
{
	struct lpfc_vport *vport = ndlp->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	uint32_t cmd, did, retry;

	spin_lock_irq(shost->host_lock);
	did = ndlp->nlp_DID;
	cmd = ndlp->nlp_last_elscmd;
	ndlp->nlp_last_elscmd = 0;

	if (!(ndlp->nlp_flag & NLP_DELAY_TMO)) {
		spin_unlock_irq(shost->host_lock);
		return;
	}

	ndlp->nlp_flag &= ~NLP_DELAY_TMO;
	spin_unlock_irq(shost->host_lock);
	del_timer_sync(&ndlp->nlp_delayfunc);
	retry = ndlp->nlp_retry;
	ndlp->nlp_retry = 0;

	switch (cmd) {
	case ELS_CMD_FLOGI:
		lpfc_issue_els_flogi(vport, ndlp, retry);
		break;
	case ELS_CMD_PLOGI:
		if (!lpfc_issue_els_plogi(vport, ndlp->nlp_DID, retry)) {
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_PLOGI_ISSUE);
		}
		break;
	case ELS_CMD_ADISC:
		if (!lpfc_issue_els_adisc(vport, ndlp, retry)) {
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_ADISC_ISSUE);
		}
		break;
	case ELS_CMD_PRLI:
		if (!lpfc_issue_els_prli(vport, ndlp, retry)) {
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_PRLI_ISSUE);
		}
		break;
	case ELS_CMD_LOGO:
		if (!lpfc_issue_els_logo(vport, ndlp, retry)) {
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		}
		break;
	case ELS_CMD_FDISC:
		if (!(vport->fc_flag & FC_VPORT_NEEDS_INIT_VPI))
			lpfc_issue_els_fdisc(vport, ndlp, retry);
		break;
	}
	return;
}

static int
lpfc_els_retry(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
	       struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp = &rspiocb->iocb;
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) cmdiocb->context1;
	struct lpfc_dmabuf *pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;
	uint32_t *elscmd;
	struct ls_rjt stat;
	int retry = 0, maxretry = lpfc_max_els_tries, delay = 0;
	int logerr = 0;
	uint32_t cmd = 0;
	uint32_t did;



	if (pcmd && pcmd->virt) {
		elscmd = (uint32_t *) (pcmd->virt);
		cmd = *elscmd++;
	}

	if (ndlp && NLP_CHK_NODE_ACT(ndlp))
		did = ndlp->nlp_DID;
	else {
		
		did = irsp->un.elsreq64.remoteID;
		ndlp = lpfc_findnode_did(vport, did);
		if ((!ndlp || !NLP_CHK_NODE_ACT(ndlp))
		    && (cmd != ELS_CMD_PLOGI))
			return 1;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Retry ELS:       wd7:x%x wd4:x%x did:x%x",
		*(((uint32_t *) irsp) + 7), irsp->un.ulpWord[4], ndlp->nlp_DID);

	switch (irsp->ulpStatus) {
	case IOSTAT_FCP_RSP_ERROR:
		break;
	case IOSTAT_REMOTE_STOP:
		if (phba->sli_rev == LPFC_SLI_REV4) {
			lpfc_set_rrq_active(phba, ndlp,
					 cmdiocb->sli4_xritag, 0, 0);
		}
		break;
	case IOSTAT_LOCAL_REJECT:
		switch ((irsp->un.ulpWord[4] & 0xff)) {
		case IOERR_LOOP_OPEN_FAILURE:
			if (cmd == ELS_CMD_FLOGI) {
				if (PCI_DEVICE_ID_HORNET ==
					phba->pcidev->device) {
					phba->fc_topology = LPFC_TOPOLOGY_LOOP;
					phba->pport->fc_myDID = 0;
					phba->alpa_map[0] = 0;
					phba->alpa_map[1] = 0;
				}
			}
			if (cmd == ELS_CMD_PLOGI && cmdiocb->retry == 0)
				delay = 1000;
			retry = 1;
			break;

		case IOERR_ILLEGAL_COMMAND:
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
					 "0124 Retry illegal cmd x%x "
					 "retry:x%x delay:x%x\n",
					 cmd, cmdiocb->retry, delay);
			retry = 1;
			
			maxretry = 8;
			if (cmdiocb->retry > 2)
				delay = 1000;
			break;

		case IOERR_NO_RESOURCES:
			logerr = 1; 
			retry = 1;
			if (cmdiocb->retry > 100)
				delay = 100;
			maxretry = 250;
			break;

		case IOERR_ILLEGAL_FRAME:
			delay = 100;
			retry = 1;
			break;

		case IOERR_SEQUENCE_TIMEOUT:
		case IOERR_INVALID_RPI:
			retry = 1;
			break;
		}
		break;

	case IOSTAT_NPORT_RJT:
	case IOSTAT_FABRIC_RJT:
		if (irsp->un.ulpWord[4] & RJT_UNAVAIL_TEMP) {
			retry = 1;
			break;
		}
		break;

	case IOSTAT_NPORT_BSY:
	case IOSTAT_FABRIC_BSY:
		logerr = 1; 
		retry = 1;
		break;

	case IOSTAT_LS_RJT:
		stat.un.lsRjtError = be32_to_cpu(irsp->un.ulpWord[4]);
		switch (stat.un.b.lsRjtRsnCode) {
		case LSRJT_UNABLE_TPC:
			if (stat.un.b.lsRjtRsnCodeExp ==
			    LSEXP_CMD_IN_PROGRESS) {
				if (cmd == ELS_CMD_PLOGI) {
					delay = 1000;
					maxretry = 48;
				}
				retry = 1;
				break;
			}
			if (stat.un.b.lsRjtRsnCodeExp ==
			    LSEXP_CANT_GIVE_DATA) {
				if (cmd == ELS_CMD_PLOGI) {
					delay = 1000;
					maxretry = 48;
				}
				retry = 1;
				break;
			}
			if (cmd == ELS_CMD_PLOGI) {
				delay = 1000;
				maxretry = lpfc_max_els_tries + 1;
				retry = 1;
				break;
			}
			if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
			  (cmd == ELS_CMD_FDISC) &&
			  (stat.un.b.lsRjtRsnCodeExp == LSEXP_OUT_OF_RESOURCE)){
				lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
						 "0125 FDISC Failed (x%x). "
						 "Fabric out of resources\n",
						 stat.un.lsRjtError);
				lpfc_vport_set_state(vport,
						     FC_VPORT_NO_FABRIC_RSCS);
			}
			break;

		case LSRJT_LOGICAL_BSY:
			if ((cmd == ELS_CMD_PLOGI) ||
			    (cmd == ELS_CMD_PRLI)) {
				delay = 1000;
				maxretry = 48;
			} else if (cmd == ELS_CMD_FDISC) {
				
				maxretry = 48;
				if (cmdiocb->retry >= 32)
					delay = 1000;
			}
			retry = 1;
			break;

		case LSRJT_LOGICAL_ERR:
			if (cmd == ELS_CMD_FDISC &&
			    stat.un.b.lsRjtRsnCodeExp == LSEXP_PORT_LOGIN_REQ) {
				maxretry = 3;
				delay = 1000;
				retry = 1;
				break;
			}
		case LSRJT_PROTOCOL_ERR:
			if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
			  (cmd == ELS_CMD_FDISC) &&
			  ((stat.un.b.lsRjtRsnCodeExp == LSEXP_INVALID_PNAME) ||
			  (stat.un.b.lsRjtRsnCodeExp == LSEXP_INVALID_NPORT_ID))
			  ) {
				lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
						 "0122 FDISC Failed (x%x). "
						 "Fabric Detected Bad WWN\n",
						 stat.un.lsRjtError);
				lpfc_vport_set_state(vport,
						     FC_VPORT_FABRIC_REJ_WWN);
			}
			break;
		}
		break;

	case IOSTAT_INTERMED_RSP:
	case IOSTAT_BA_RJT:
		break;

	default:
		break;
	}

	if (did == FDMI_DID)
		retry = 1;

	if ((cmd == ELS_CMD_FLOGI) &&
	    (phba->fc_topology != LPFC_TOPOLOGY_LOOP) &&
	    !lpfc_error_lost_link(irsp)) {
		
		retry = 1;
		
		maxretry = 0;
		if (cmdiocb->retry >= 100)
			delay = 5000;
		else if (cmdiocb->retry >= 32)
			delay = 1000;
	} else if ((cmd == ELS_CMD_FDISC) && !lpfc_error_lost_link(irsp)) {
		
		retry = 1;
		maxretry = vport->cfg_devloss_tmo;
		delay = 1000;
	}

	cmdiocb->retry++;
	if (maxretry && (cmdiocb->retry >= maxretry)) {
		phba->fc_stat.elsRetryExceeded++;
		retry = 0;
	}

	if ((vport->load_flag & FC_UNLOADING) != 0)
		retry = 0;

	if (retry) {
		if ((cmd == ELS_CMD_PLOGI) || (cmd == ELS_CMD_FDISC)) {
			
			if (phba->fcf.fcf_flag & FCF_DISCOVERY) {
				lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
						 "2849 Stop retry ELS command "
						 "x%x to remote NPORT x%x, "
						 "Data: x%x x%x\n", cmd, did,
						 cmdiocb->retry, delay);
				return 0;
			}
		}

		
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
				 "0107 Retry ELS command x%x to remote "
				 "NPORT x%x Data: x%x x%x\n",
				 cmd, did, cmdiocb->retry, delay);

		if (((cmd == ELS_CMD_PLOGI) || (cmd == ELS_CMD_ADISC)) &&
			((irsp->ulpStatus != IOSTAT_LOCAL_REJECT) ||
			((irsp->un.ulpWord[4] & 0xff) != IOERR_NO_RESOURCES))) {
			

			
			if (timer_pending(&vport->fc_disctmo) ||
			    (vport->fc_flag & FC_RSCN_MODE))
				lpfc_set_disctmo(vport);
		}

		phba->fc_stat.elsXmitRetry++;
		if (ndlp && NLP_CHK_NODE_ACT(ndlp) && delay) {
			phba->fc_stat.elsDelayRetry++;
			ndlp->nlp_retry = cmdiocb->retry;

			
			mod_timer(&ndlp->nlp_delayfunc,
				jiffies + msecs_to_jiffies(delay));
			spin_lock_irq(shost->host_lock);
			ndlp->nlp_flag |= NLP_DELAY_TMO;
			spin_unlock_irq(shost->host_lock);

			ndlp->nlp_prev_state = ndlp->nlp_state;
			if (cmd == ELS_CMD_PRLI)
				lpfc_nlp_set_state(vport, ndlp,
					NLP_STE_REG_LOGIN_ISSUE);
			else
				lpfc_nlp_set_state(vport, ndlp,
					NLP_STE_NPR_NODE);
			ndlp->nlp_last_elscmd = cmd;

			return 1;
		}
		switch (cmd) {
		case ELS_CMD_FLOGI:
			lpfc_issue_els_flogi(vport, ndlp, cmdiocb->retry);
			return 1;
		case ELS_CMD_FDISC:
			lpfc_issue_els_fdisc(vport, ndlp, cmdiocb->retry);
			return 1;
		case ELS_CMD_PLOGI:
			if (ndlp && NLP_CHK_NODE_ACT(ndlp)) {
				ndlp->nlp_prev_state = ndlp->nlp_state;
				lpfc_nlp_set_state(vport, ndlp,
						   NLP_STE_PLOGI_ISSUE);
			}
			lpfc_issue_els_plogi(vport, did, cmdiocb->retry);
			return 1;
		case ELS_CMD_ADISC:
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_ADISC_ISSUE);
			lpfc_issue_els_adisc(vport, ndlp, cmdiocb->retry);
			return 1;
		case ELS_CMD_PRLI:
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_PRLI_ISSUE);
			lpfc_issue_els_prli(vport, ndlp, cmdiocb->retry);
			return 1;
		case ELS_CMD_LOGO:
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
			lpfc_issue_els_logo(vport, ndlp, cmdiocb->retry);
			return 1;
		}
	}
	
	if (logerr) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
			 "0137 No retry ELS command x%x to remote "
			 "NPORT x%x: Out of Resources: Error:x%x/%x\n",
			 cmd, did, irsp->ulpStatus,
			 irsp->un.ulpWord[4]);
	}
	else {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0108 No retry ELS command x%x to remote "
			 "NPORT x%x Retried:%d Error:x%x/%x\n",
			 cmd, did, cmdiocb->retry, irsp->ulpStatus,
			 irsp->un.ulpWord[4]);
	}
	return 0;
}

static int
lpfc_els_free_data(struct lpfc_hba *phba, struct lpfc_dmabuf *buf_ptr1)
{
	struct lpfc_dmabuf *buf_ptr;

	
	if (!list_empty(&buf_ptr1->list)) {
		list_remove_head(&buf_ptr1->list, buf_ptr,
				 struct lpfc_dmabuf,
				 list);
		lpfc_mbuf_free(phba, buf_ptr->virt, buf_ptr->phys);
		kfree(buf_ptr);
	}
	lpfc_mbuf_free(phba, buf_ptr1->virt, buf_ptr1->phys);
	kfree(buf_ptr1);
	return 0;
}

static int
lpfc_els_free_bpl(struct lpfc_hba *phba, struct lpfc_dmabuf *buf_ptr)
{
	lpfc_mbuf_free(phba, buf_ptr->virt, buf_ptr->phys);
	kfree(buf_ptr);
	return 0;
}

int
lpfc_els_free_iocb(struct lpfc_hba *phba, struct lpfc_iocbq *elsiocb)
{
	struct lpfc_dmabuf *buf_ptr, *buf_ptr1;
	struct lpfc_nodelist *ndlp;

	ndlp = (struct lpfc_nodelist *)elsiocb->context1;
	if (ndlp) {
		if (ndlp->nlp_flag & NLP_DEFER_RM) {
			lpfc_nlp_put(ndlp);

			if (!lpfc_nlp_not_used(ndlp)) {
				ndlp->nlp_flag &= ~NLP_DEFER_RM;
			}
		}
		else
			lpfc_nlp_put(ndlp);
		elsiocb->context1 = NULL;
	}
	
	if (elsiocb->context2) {
		if (elsiocb->iocb_flag & LPFC_DELAY_MEM_FREE) {
			elsiocb->iocb_flag &= ~LPFC_DELAY_MEM_FREE;
			buf_ptr = elsiocb->context2;
			elsiocb->context2 = NULL;
			if (buf_ptr) {
				buf_ptr1 = NULL;
				spin_lock_irq(&phba->hbalock);
				if (!list_empty(&buf_ptr->list)) {
					list_remove_head(&buf_ptr->list,
						buf_ptr1, struct lpfc_dmabuf,
						list);
					INIT_LIST_HEAD(&buf_ptr1->list);
					list_add_tail(&buf_ptr1->list,
						&phba->elsbuf);
					phba->elsbuf_cnt++;
				}
				INIT_LIST_HEAD(&buf_ptr->list);
				list_add_tail(&buf_ptr->list, &phba->elsbuf);
				phba->elsbuf_cnt++;
				spin_unlock_irq(&phba->hbalock);
			}
		} else {
			buf_ptr1 = (struct lpfc_dmabuf *) elsiocb->context2;
			lpfc_els_free_data(phba, buf_ptr1);
		}
	}

	if (elsiocb->context3) {
		buf_ptr = (struct lpfc_dmabuf *) elsiocb->context3;
		lpfc_els_free_bpl(phba, buf_ptr);
	}
	lpfc_sli_release_iocbq(phba, elsiocb);
	return 0;
}

static void
lpfc_cmpl_els_logo_acc(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		       struct lpfc_iocbq *rspiocb)
{
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) cmdiocb->context1;
	struct lpfc_vport *vport = cmdiocb->vport;
	IOCB_t *irsp;

	irsp = &rspiocb->iocb;
	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"ACC LOGO cmpl:   status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4], ndlp->nlp_DID);
	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0109 ACC to LOGO completes to NPort x%x "
			 "Data: x%x x%x x%x\n",
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);

	if (ndlp->nlp_state == NLP_STE_NPR_NODE) {
		
		if (!lpfc_nlp_not_used(ndlp)) {
			lpfc_unreg_rpi(vport, ndlp);
		} else {
			cmdiocb->context1 = NULL;
		}
	}

	lpfc_els_free_iocb(phba, cmdiocb);

	if (ndlp->nlp_type & NLP_FABRIC)
		lpfc_nlp_put(ndlp);

	return;
}

void
lpfc_mbx_cmpl_dflt_rpi(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_dmabuf *mp = (struct lpfc_dmabuf *) (pmb->context1);
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) pmb->context2;

	pmb->context1 = NULL;
	pmb->context2 = NULL;

	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	mempool_free(pmb, phba->mbox_mem_pool);
	if (ndlp && NLP_CHK_NODE_ACT(ndlp)) {
		lpfc_nlp_put(ndlp);
		lpfc_nlp_not_used(ndlp);
	}

	return;
}

static void
lpfc_cmpl_els_rsp(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_iocbq *rspiocb)
{
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) cmdiocb->context1;
	struct lpfc_vport *vport = ndlp ? ndlp->vport : NULL;
	struct Scsi_Host  *shost = vport ? lpfc_shost_from_vport(vport) : NULL;
	IOCB_t  *irsp;
	uint8_t *pcmd;
	LPFC_MBOXQ_t *mbox = NULL;
	struct lpfc_dmabuf *mp = NULL;
	uint32_t ls_rjt = 0;

	irsp = &rspiocb->iocb;

	if (cmdiocb->context_un.mbox)
		mbox = cmdiocb->context_un.mbox;

	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) cmdiocb->context2)->virt);
	if (ndlp && NLP_CHK_NODE_ACT(ndlp) &&
	    (*((uint32_t *) (pcmd)) == ELS_CMD_LS_RJT)) {
		if (!(ndlp->nlp_flag & NLP_RM_DFLT_RPI))
			ls_rjt = 1;
	}

	
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp) || lpfc_els_chk_latt(vport)) {
		if (mbox) {
			mp = (struct lpfc_dmabuf *) mbox->context1;
			if (mp) {
				lpfc_mbuf_free(phba, mp->virt, mp->phys);
				kfree(mp);
			}
			mempool_free(mbox, phba->mbox_mem_pool);
		}
		if (ndlp && NLP_CHK_NODE_ACT(ndlp) &&
		    (ndlp->nlp_flag & NLP_RM_DFLT_RPI))
			if (lpfc_nlp_not_used(ndlp)) {
				ndlp = NULL;
				cmdiocb->context1 = NULL;
			}
		goto out;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"ELS rsp cmpl:    status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		cmdiocb->iocb.un.elsreq64.remoteID);
	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0110 ELS response tag x%x completes "
			 "Data: x%x x%x x%x x%x x%x x%x x%x\n",
			 cmdiocb->iocb.ulpIoTag, rspiocb->iocb.ulpStatus,
			 rspiocb->iocb.un.ulpWord[4], rspiocb->iocb.ulpTimeout,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);
	if (mbox) {
		if ((rspiocb->iocb.ulpStatus == 0)
		    && (ndlp->nlp_flag & NLP_ACC_REGLOGIN)) {
			lpfc_unreg_rpi(vport, ndlp);
			mbox->context2 = lpfc_nlp_get(ndlp);
			mbox->vport = vport;
			if (ndlp->nlp_flag & NLP_RM_DFLT_RPI) {
				mbox->mbox_flag |= LPFC_MBX_IMED_UNREG;
				mbox->mbox_cmpl = lpfc_mbx_cmpl_dflt_rpi;
			}
			else {
				mbox->mbox_cmpl = lpfc_mbx_cmpl_reg_login;
				ndlp->nlp_prev_state = ndlp->nlp_state;
				lpfc_nlp_set_state(vport, ndlp,
					   NLP_STE_REG_LOGIN_ISSUE);
			}
			if (lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT)
			    != MBX_NOT_FINISHED)
				goto out;
			else
				lpfc_nlp_put(ndlp);

			
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				"0138 ELS rsp: Cannot issue reg_login for x%x "
				"Data: x%x x%x x%x\n",
				ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
				ndlp->nlp_rpi);

			if (lpfc_nlp_not_used(ndlp)) {
				ndlp = NULL;
				cmdiocb->context1 = NULL;
			}
		} else {
			
			if (!lpfc_error_lost_link(irsp) &&
			    ndlp->nlp_flag & NLP_ACC_REGLOGIN) {
				if (lpfc_nlp_not_used(ndlp)) {
					ndlp = NULL;
					cmdiocb->context1 = NULL;
				}
			}
		}
		mp = (struct lpfc_dmabuf *) mbox->context1;
		if (mp) {
			lpfc_mbuf_free(phba, mp->virt, mp->phys);
			kfree(mp);
		}
		mempool_free(mbox, phba->mbox_mem_pool);
	}
out:
	if (ndlp && NLP_CHK_NODE_ACT(ndlp)) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag &= ~(NLP_ACC_REGLOGIN | NLP_RM_DFLT_RPI);
		spin_unlock_irq(shost->host_lock);

		if (ls_rjt)
			if (lpfc_nlp_not_used(ndlp))
				cmdiocb->context1 = NULL;
	}

	lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

int
lpfc_els_rsp_acc(struct lpfc_vport *vport, uint32_t flag,
		 struct lpfc_iocbq *oldiocb, struct lpfc_nodelist *ndlp,
		 LPFC_MBOXQ_t *mbox)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	IOCB_t *icmd;
	IOCB_t *oldcmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli *psli;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int rc;
	ELS_PKT *els_pkt_ptr;

	psli = &phba->sli;
	oldcmd = &oldiocb->iocb;

	switch (flag) {
	case ELS_CMD_ACC:
		cmdsize = sizeof(uint32_t);
		elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry,
					     ndlp, ndlp->nlp_DID, ELS_CMD_ACC);
		if (!elsiocb) {
			spin_lock_irq(shost->host_lock);
			ndlp->nlp_flag &= ~NLP_LOGO_ACC;
			spin_unlock_irq(shost->host_lock);
			return 1;
		}

		icmd = &elsiocb->iocb;
		icmd->ulpContext = oldcmd->ulpContext;	
		icmd->unsli3.rcvsli3.ox_id = oldcmd->unsli3.rcvsli3.ox_id;
		pcmd = (((struct lpfc_dmabuf *) elsiocb->context2)->virt);
		*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
		pcmd += sizeof(uint32_t);

		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
			"Issue ACC:       did:x%x flg:x%x",
			ndlp->nlp_DID, ndlp->nlp_flag, 0);
		break;
	case ELS_CMD_PLOGI:
		cmdsize = (sizeof(struct serv_parm) + sizeof(uint32_t));
		elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry,
					     ndlp, ndlp->nlp_DID, ELS_CMD_ACC);
		if (!elsiocb)
			return 1;

		icmd = &elsiocb->iocb;
		icmd->ulpContext = oldcmd->ulpContext;	
		icmd->unsli3.rcvsli3.ox_id = oldcmd->unsli3.rcvsli3.ox_id;
		pcmd = (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

		if (mbox)
			elsiocb->context_un.mbox = mbox;

		*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
		pcmd += sizeof(uint32_t);
		memcpy(pcmd, &vport->fc_sparam, sizeof(struct serv_parm));

		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
			"Issue ACC PLOGI: did:x%x flg:x%x",
			ndlp->nlp_DID, ndlp->nlp_flag, 0);
		break;
	case ELS_CMD_PRLO:
		cmdsize = sizeof(uint32_t) + sizeof(PRLO);
		elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry,
					     ndlp, ndlp->nlp_DID, ELS_CMD_PRLO);
		if (!elsiocb)
			return 1;

		icmd = &elsiocb->iocb;
		icmd->ulpContext = oldcmd->ulpContext;	
		icmd->unsli3.rcvsli3.ox_id = oldcmd->unsli3.rcvsli3.ox_id;
		pcmd = (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

		memcpy(pcmd, ((struct lpfc_dmabuf *) oldiocb->context2)->virt,
		       sizeof(uint32_t) + sizeof(PRLO));
		*((uint32_t *) (pcmd)) = ELS_CMD_PRLO_ACC;
		els_pkt_ptr = (ELS_PKT *) pcmd;
		els_pkt_ptr->un.prlo.acceptRspCode = PRLO_REQ_EXECUTED;

		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
			"Issue ACC PRLO:  did:x%x flg:x%x",
			ndlp->nlp_DID, ndlp->nlp_flag, 0);
		break;
	default:
		return 1;
	}
	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0128 Xmit ELS ACC response tag x%x, XRI: x%x, "
			 "DID: x%x, nlp_flag: x%x nlp_state: x%x RPI: x%x\n",
			 elsiocb->iotag, elsiocb->iocb.ulpContext,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);
	if (ndlp->nlp_flag & NLP_LOGO_ACC) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag &= ~NLP_LOGO_ACC;
		spin_unlock_irq(shost->host_lock);
		elsiocb->iocb_cmpl = lpfc_cmpl_els_logo_acc;
	} else {
		elsiocb->iocb_cmpl = lpfc_cmpl_els_rsp;
	}

	phba->fc_stat.elsXmitACC++;
	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

int
lpfc_els_rsp_reject(struct lpfc_vport *vport, uint32_t rejectError,
		    struct lpfc_iocbq *oldiocb, struct lpfc_nodelist *ndlp,
		    LPFC_MBOXQ_t *mbox)
{
	struct lpfc_hba  *phba = vport->phba;
	IOCB_t *icmd;
	IOCB_t *oldcmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli *psli;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int rc;

	psli = &phba->sli;
	cmdsize = 2 * sizeof(uint32_t);
	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_LS_RJT);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	oldcmd = &oldiocb->iocb;
	icmd->ulpContext = oldcmd->ulpContext;	
	icmd->unsli3.rcvsli3.ox_id = oldcmd->unsli3.rcvsli3.ox_id;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	*((uint32_t *) (pcmd)) = ELS_CMD_LS_RJT;
	pcmd += sizeof(uint32_t);
	*((uint32_t *) (pcmd)) = rejectError;

	if (mbox)
		elsiocb->context_un.mbox = mbox;

	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0129 Xmit ELS RJT x%x response tag x%x "
			 "xri x%x, did x%x, nlp_flag x%x, nlp_state x%x, "
			 "rpi x%x\n",
			 rejectError, elsiocb->iotag,
			 elsiocb->iocb.ulpContext, ndlp->nlp_DID,
			 ndlp->nlp_flag, ndlp->nlp_state, ndlp->nlp_rpi);
	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"Issue LS_RJT:    did:x%x flg:x%x err:x%x",
		ndlp->nlp_DID, ndlp->nlp_flag, rejectError);

	phba->fc_stat.elsXmitLSRJT++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_rsp;
	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);

	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

int
lpfc_els_rsp_adisc_acc(struct lpfc_vport *vport, struct lpfc_iocbq *oldiocb,
		       struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba  *phba = vport->phba;
	ADISC *ap;
	IOCB_t *icmd, *oldcmd;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int rc;

	cmdsize = sizeof(uint32_t) + sizeof(ADISC);
	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ACC);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	oldcmd = &oldiocb->iocb;
	icmd->ulpContext = oldcmd->ulpContext;	
	icmd->unsli3.rcvsli3.ox_id = oldcmd->unsli3.rcvsli3.ox_id;

	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0130 Xmit ADISC ACC response iotag x%x xri: "
			 "x%x, did x%x, nlp_flag x%x, nlp_state x%x rpi x%x\n",
			 elsiocb->iotag, elsiocb->iocb.ulpContext,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
	pcmd += sizeof(uint32_t);

	ap = (ADISC *) (pcmd);
	ap->hardAL_PA = phba->fc_pref_ALPA;
	memcpy(&ap->portName, &vport->fc_portname, sizeof(struct lpfc_name));
	memcpy(&ap->nodeName, &vport->fc_nodename, sizeof(struct lpfc_name));
	ap->DID = be32_to_cpu(vport->fc_myDID);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"Issue ACC ADISC: did:x%x flg:x%x",
		ndlp->nlp_DID, ndlp->nlp_flag, 0);

	phba->fc_stat.elsXmitACC++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_rsp;
	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

int
lpfc_els_rsp_prli_acc(struct lpfc_vport *vport, struct lpfc_iocbq *oldiocb,
		      struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba  *phba = vport->phba;
	PRLI *npr;
	lpfc_vpd_t *vpd;
	IOCB_t *icmd;
	IOCB_t *oldcmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli *psli;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int rc;

	psli = &phba->sli;

	cmdsize = sizeof(uint32_t) + sizeof(PRLI);
	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry, ndlp,
		ndlp->nlp_DID, (ELS_CMD_ACC | (ELS_CMD_PRLI & ~ELS_RSP_MASK)));
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	oldcmd = &oldiocb->iocb;
	icmd->ulpContext = oldcmd->ulpContext;	
	icmd->unsli3.rcvsli3.ox_id = oldcmd->unsli3.rcvsli3.ox_id;

	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0131 Xmit PRLI ACC response tag x%x xri x%x, "
			 "did x%x, nlp_flag x%x, nlp_state x%x, rpi x%x\n",
			 elsiocb->iotag, elsiocb->iocb.ulpContext,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	*((uint32_t *) (pcmd)) = (ELS_CMD_ACC | (ELS_CMD_PRLI & ~ELS_RSP_MASK));
	pcmd += sizeof(uint32_t);

	
	memset(pcmd, 0, sizeof(PRLI));

	npr = (PRLI *) pcmd;
	vpd = &phba->vpd;
	if ((ndlp->nlp_type & NLP_FCP_TARGET) &&
	    (vpd->rev.feaLevelHigh >= 0x02)) {
		npr->ConfmComplAllowed = 1;
		npr->Retry = 1;
		npr->TaskRetryIdReq = 1;
	}

	npr->acceptRspCode = PRLI_REQ_EXECUTED;
	npr->estabImagePair = 1;
	npr->readXferRdyDis = 1;
	npr->ConfmComplAllowed = 1;

	npr->prliType = PRLI_FCP_TYPE;
	npr->initiatorFunc = 1;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"Issue ACC PRLI:  did:x%x flg:x%x",
		ndlp->nlp_DID, ndlp->nlp_flag, 0);

	phba->fc_stat.elsXmitACC++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_rsp;

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

static int
lpfc_els_rsp_rnid_acc(struct lpfc_vport *vport, uint8_t format,
		      struct lpfc_iocbq *oldiocb, struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba  *phba = vport->phba;
	RNID *rn;
	IOCB_t *icmd, *oldcmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli *psli;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int rc;

	psli = &phba->sli;
	cmdsize = sizeof(uint32_t) + sizeof(uint32_t)
					+ (2 * sizeof(struct lpfc_name));
	if (format)
		cmdsize += sizeof(RNID_TOP_DISC);

	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ACC);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	oldcmd = &oldiocb->iocb;
	icmd->ulpContext = oldcmd->ulpContext;	
	icmd->unsli3.rcvsli3.ox_id = oldcmd->unsli3.rcvsli3.ox_id;

	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0132 Xmit RNID ACC response tag x%x xri x%x\n",
			 elsiocb->iotag, elsiocb->iocb.ulpContext);
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);
	*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
	pcmd += sizeof(uint32_t);

	memset(pcmd, 0, sizeof(RNID));
	rn = (RNID *) (pcmd);
	rn->Format = format;
	rn->CommonLen = (2 * sizeof(struct lpfc_name));
	memcpy(&rn->portName, &vport->fc_portname, sizeof(struct lpfc_name));
	memcpy(&rn->nodeName, &vport->fc_nodename, sizeof(struct lpfc_name));
	switch (format) {
	case 0:
		rn->SpecificLen = 0;
		break;
	case RNID_TOPOLOGY_DISC:
		rn->SpecificLen = sizeof(RNID_TOP_DISC);
		memcpy(&rn->un.topologyDisc.portName,
		       &vport->fc_portname, sizeof(struct lpfc_name));
		rn->un.topologyDisc.unitType = RNID_HBA;
		rn->un.topologyDisc.physPort = 0;
		rn->un.topologyDisc.attachedNodes = 0;
		break;
	default:
		rn->CommonLen = 0;
		rn->SpecificLen = 0;
		break;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"Issue ACC RNID:  did:x%x flg:x%x",
		ndlp->nlp_DID, ndlp->nlp_flag, 0);

	phba->fc_stat.elsXmitACC++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_rsp;

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

static void
lpfc_els_clear_rrq(struct lpfc_vport *vport,
      struct lpfc_iocbq *iocb, struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba  *phba = vport->phba;
	uint8_t *pcmd;
	struct RRQ *rrq;
	uint16_t rxid;
	uint16_t xri;
	struct lpfc_node_rrq *prrq;


	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) iocb->context2)->virt);
	pcmd += sizeof(uint32_t);
	rrq = (struct RRQ *)pcmd;
	rrq->rrq_exchg = be32_to_cpu(rrq->rrq_exchg);
	rxid = bf_get(rrq_rxid, rrq);

	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			"2883 Clear RRQ for SID:x%x OXID:x%x RXID:x%x"
			" x%x x%x\n",
			be32_to_cpu(bf_get(rrq_did, rrq)),
			bf_get(rrq_oxid, rrq),
			rxid,
			iocb->iotag, iocb->iocb.ulpContext);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"Clear RRQ:  did:x%x flg:x%x exchg:x%.08x",
		ndlp->nlp_DID, ndlp->nlp_flag, rrq->rrq_exchg);
	if (vport->fc_myDID == be32_to_cpu(bf_get(rrq_did, rrq)))
		xri = bf_get(rrq_oxid, rrq);
	else
		xri = rxid;
	prrq = lpfc_get_active_rrq(vport, xri, ndlp->nlp_DID);
	if (prrq)
		lpfc_clr_rrq_active(phba, xri, prrq);
	return;
}

static int
lpfc_els_rsp_echo_acc(struct lpfc_vport *vport, uint8_t *data,
		      struct lpfc_iocbq *oldiocb, struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli *psli;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int rc;

	psli = &phba->sli;
	cmdsize = oldiocb->iocb.unsli3.rcvsli3.acc_len;

	if (cmdsize > LPFC_BPL_SIZE)
		cmdsize = LPFC_BPL_SIZE;
	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ACC);
	if (!elsiocb)
		return 1;

	elsiocb->iocb.ulpContext = oldiocb->iocb.ulpContext;  
	elsiocb->iocb.unsli3.rcvsli3.ox_id = oldiocb->iocb.unsli3.rcvsli3.ox_id;

	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "2876 Xmit ECHO ACC response tag x%x xri x%x\n",
			 elsiocb->iotag, elsiocb->iocb.ulpContext);
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);
	*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
	pcmd += sizeof(uint32_t);
	memcpy(pcmd, data, cmdsize - sizeof(uint32_t));

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"Issue ACC ECHO:  did:x%x flg:x%x",
		ndlp->nlp_DID, ndlp->nlp_flag, 0);

	phba->fc_stat.elsXmitACC++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_rsp;

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

int
lpfc_els_disc_adisc(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp, *next_ndlp;
	int sentadisc = 0;

	
	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp))
			continue;
		if (ndlp->nlp_state == NLP_STE_NPR_NODE &&
		    (ndlp->nlp_flag & NLP_NPR_2B_DISC) != 0 &&
		    (ndlp->nlp_flag & NLP_NPR_ADISC) != 0) {
			spin_lock_irq(shost->host_lock);
			ndlp->nlp_flag &= ~NLP_NPR_ADISC;
			spin_unlock_irq(shost->host_lock);
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_ADISC_ISSUE);
			lpfc_issue_els_adisc(vport, ndlp, 0);
			sentadisc++;
			vport->num_disc_nodes++;
			if (vport->num_disc_nodes >=
			    vport->cfg_discovery_threads) {
				spin_lock_irq(shost->host_lock);
				vport->fc_flag |= FC_NLP_MORE;
				spin_unlock_irq(shost->host_lock);
				break;
			}
		}
	}
	if (sentadisc == 0) {
		spin_lock_irq(shost->host_lock);
		vport->fc_flag &= ~FC_NLP_MORE;
		spin_unlock_irq(shost->host_lock);
	}
	return sentadisc;
}

int
lpfc_els_disc_plogi(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp, *next_ndlp;
	int sentplogi = 0;

	
	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp))
			continue;
		if (ndlp->nlp_state == NLP_STE_NPR_NODE &&
		    (ndlp->nlp_flag & NLP_NPR_2B_DISC) != 0 &&
		    (ndlp->nlp_flag & NLP_DELAY_TMO) == 0 &&
		    (ndlp->nlp_flag & NLP_NPR_ADISC) == 0) {
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_PLOGI_ISSUE);
			lpfc_issue_els_plogi(vport, ndlp->nlp_DID, 0);
			sentplogi++;
			vport->num_disc_nodes++;
			if (vport->num_disc_nodes >=
			    vport->cfg_discovery_threads) {
				spin_lock_irq(shost->host_lock);
				vport->fc_flag |= FC_NLP_MORE;
				spin_unlock_irq(shost->host_lock);
				break;
			}
		}
	}
	if (sentplogi) {
		lpfc_set_disctmo(vport);
	}
	else {
		spin_lock_irq(shost->host_lock);
		vport->fc_flag &= ~FC_NLP_MORE;
		spin_unlock_irq(shost->host_lock);
	}
	return sentplogi;
}

void
lpfc_els_flush_rscn(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	int i;

	spin_lock_irq(shost->host_lock);
	if (vport->fc_rscn_flush) {
		
		spin_unlock_irq(shost->host_lock);
		return;
	}
	
	vport->fc_rscn_flush = 1;
	spin_unlock_irq(shost->host_lock);

	for (i = 0; i < vport->fc_rscn_id_cnt; i++) {
		lpfc_in_buf_free(phba, vport->fc_rscn_id_list[i]);
		vport->fc_rscn_id_list[i] = NULL;
	}
	spin_lock_irq(shost->host_lock);
	vport->fc_rscn_id_cnt = 0;
	vport->fc_flag &= ~(FC_RSCN_MODE | FC_RSCN_DISCOVERY);
	spin_unlock_irq(shost->host_lock);
	lpfc_can_disctmo(vport);
	
	vport->fc_rscn_flush = 0;
}

int
lpfc_rscn_payload_check(struct lpfc_vport *vport, uint32_t did)
{
	D_ID ns_did;
	D_ID rscn_did;
	uint32_t *lp;
	uint32_t payload_len, i;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	ns_did.un.word = did;

	
	if ((did & Fabric_DID_MASK) == Fabric_DID_MASK)
		return 0;

	
	if (vport->fc_flag & FC_RSCN_DISCOVERY)
		return did;

	spin_lock_irq(shost->host_lock);
	if (vport->fc_rscn_flush) {
		
		spin_unlock_irq(shost->host_lock);
		return 0;
	}
	
	vport->fc_rscn_flush = 1;
	spin_unlock_irq(shost->host_lock);
	for (i = 0; i < vport->fc_rscn_id_cnt; i++) {
		lp = vport->fc_rscn_id_list[i]->virt;
		payload_len = be32_to_cpu(*lp++ & ~ELS_CMD_MASK);
		payload_len -= sizeof(uint32_t);	
		while (payload_len) {
			rscn_did.un.word = be32_to_cpu(*lp++);
			payload_len -= sizeof(uint32_t);
			switch (rscn_did.un.b.resv & RSCN_ADDRESS_FORMAT_MASK) {
			case RSCN_ADDRESS_FORMAT_PORT:
				if ((ns_did.un.b.domain == rscn_did.un.b.domain)
				    && (ns_did.un.b.area == rscn_did.un.b.area)
				    && (ns_did.un.b.id == rscn_did.un.b.id))
					goto return_did_out;
				break;
			case RSCN_ADDRESS_FORMAT_AREA:
				if ((ns_did.un.b.domain == rscn_did.un.b.domain)
				    && (ns_did.un.b.area == rscn_did.un.b.area))
					goto return_did_out;
				break;
			case RSCN_ADDRESS_FORMAT_DOMAIN:
				if (ns_did.un.b.domain == rscn_did.un.b.domain)
					goto return_did_out;
				break;
			case RSCN_ADDRESS_FORMAT_FABRIC:
				goto return_did_out;
			}
		}
	}
	
	vport->fc_rscn_flush = 0;
	return 0;
return_did_out:
	
	vport->fc_rscn_flush = 0;
	return did;
}

static int
lpfc_rscn_recovery_check(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp = NULL;

	
	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp) ||
		    (ndlp->nlp_state == NLP_STE_UNUSED_NODE) ||
		    !lpfc_rscn_payload_check(vport, ndlp->nlp_DID))
			continue;
		lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RECOVERY);
		lpfc_cancel_retry_delay_tmo(vport, ndlp);
	}
	return 0;
}

static void
lpfc_send_rscn_event(struct lpfc_vport *vport,
		struct lpfc_iocbq *cmdiocb)
{
	struct lpfc_dmabuf *pcmd;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	uint32_t *payload_ptr;
	uint32_t payload_len;
	struct lpfc_rscn_event_header *rscn_event_data;

	pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;
	payload_ptr = (uint32_t *) pcmd->virt;
	payload_len = be32_to_cpu(*payload_ptr & ~ELS_CMD_MASK);

	rscn_event_data = kmalloc(sizeof(struct lpfc_rscn_event_header) +
		payload_len, GFP_KERNEL);
	if (!rscn_event_data) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
			"0147 Failed to allocate memory for RSCN event\n");
		return;
	}
	rscn_event_data->event_type = FC_REG_RSCN_EVENT;
	rscn_event_data->payload_length = payload_len;
	memcpy(rscn_event_data->rscn_payload, payload_ptr,
		payload_len);

	fc_host_post_vendor_event(shost,
		fc_get_event_number(),
		sizeof(struct lpfc_els_event_header) + payload_len,
		(char *)rscn_event_data,
		LPFC_NL_VENDOR_ID);

	kfree(rscn_event_data);
}

static int
lpfc_els_rcv_rscn(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_dmabuf *pcmd;
	uint32_t *lp, *datap;
	IOCB_t *icmd;
	uint32_t payload_len, length, nportid, *cmd;
	int rscn_cnt;
	int rscn_id = 0, hba_id = 0;
	int i;

	icmd = &cmdiocb->iocb;
	pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;
	lp = (uint32_t *) pcmd->virt;

	payload_len = be32_to_cpu(*lp++ & ~ELS_CMD_MASK);
	payload_len -= sizeof(uint32_t);	
	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0214 RSCN received Data: x%x x%x x%x x%x\n",
			 vport->fc_flag, payload_len, *lp,
			 vport->fc_rscn_id_cnt);

	
	lpfc_send_rscn_event(vport, cmdiocb);

	for (i = 0; i < payload_len/sizeof(uint32_t); i++)
		fc_host_post_event(shost, fc_get_event_number(),
			FCH_EVT_RSCN, lp[i]);

	if (vport->port_state <= LPFC_NS_QRY) {
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RSCN ignore: did:x%x/ste:x%x flg:x%x",
			ndlp->nlp_DID, vport->port_state, ndlp->nlp_flag);

		lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb, ndlp, NULL);
		return 0;
	}

	if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
		!(vport->cfg_peer_port_login)) {
		i = payload_len;
		datap = lp;
		while (i > 0) {
			nportid = *datap++;
			nportid = ((be32_to_cpu(nportid)) & Mask_DID);
			i -= sizeof(uint32_t);
			rscn_id++;
			if (lpfc_find_vport_by_did(phba, nportid))
				hba_id++;
		}
		if (rscn_id == hba_id) {
			
			lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
					 "0219 Ignore RSCN "
					 "Data: x%x x%x x%x x%x\n",
					 vport->fc_flag, payload_len,
					 *lp, vport->fc_rscn_id_cnt);
			lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
				"RCV RSCN vport:  did:x%x/ste:x%x flg:x%x",
				ndlp->nlp_DID, vport->port_state,
				ndlp->nlp_flag);

			lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb,
				ndlp, NULL);
			return 0;
		}
	}

	spin_lock_irq(shost->host_lock);
	if (vport->fc_rscn_flush) {
		
		vport->fc_flag |= FC_RSCN_DISCOVERY;
		spin_unlock_irq(shost->host_lock);
		
		lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb, ndlp, NULL);
		return 0;
	}
	
	vport->fc_rscn_flush = 1;
	spin_unlock_irq(shost->host_lock);
	
	rscn_cnt = vport->fc_rscn_id_cnt;
	if (vport->fc_flag & (FC_RSCN_MODE | FC_NDISC_ACTIVE)) {
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RSCN defer:  did:x%x/ste:x%x flg:x%x",
			ndlp->nlp_DID, vport->port_state, ndlp->nlp_flag);

		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_RSCN_DEFERRED;
		if ((rscn_cnt < FC_MAX_HOLD_RSCN) &&
		    !(vport->fc_flag & FC_RSCN_DISCOVERY)) {
			vport->fc_flag |= FC_RSCN_MODE;
			spin_unlock_irq(shost->host_lock);
			if (rscn_cnt) {
				cmd = vport->fc_rscn_id_list[rscn_cnt-1]->virt;
				length = be32_to_cpu(*cmd & ~ELS_CMD_MASK);
			}
			if ((rscn_cnt) &&
			    (payload_len + length <= LPFC_BPL_SIZE)) {
				*cmd &= ELS_CMD_MASK;
				*cmd |= cpu_to_be32(payload_len + length);
				memcpy(((uint8_t *)cmd) + length, lp,
				       payload_len);
			} else {
				vport->fc_rscn_id_list[rscn_cnt] = pcmd;
				vport->fc_rscn_id_cnt++;
				cmdiocb->context2 = NULL;
			}
			
			lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
					 "0235 Deferred RSCN "
					 "Data: x%x x%x x%x\n",
					 vport->fc_rscn_id_cnt, vport->fc_flag,
					 vport->port_state);
		} else {
			vport->fc_flag |= FC_RSCN_DISCOVERY;
			spin_unlock_irq(shost->host_lock);
			
			lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
					 "0234 ReDiscovery RSCN "
					 "Data: x%x x%x x%x\n",
					 vport->fc_rscn_id_cnt, vport->fc_flag,
					 vport->port_state);
		}
		
		vport->fc_rscn_flush = 0;
		
		lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb, ndlp, NULL);
		
		lpfc_rscn_recovery_check(vport);
		spin_lock_irq(shost->host_lock);
		vport->fc_flag &= ~FC_RSCN_DEFERRED;
		spin_unlock_irq(shost->host_lock);
		return 0;
	}
	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
		"RCV RSCN:        did:x%x/ste:x%x flg:x%x",
		ndlp->nlp_DID, vport->port_state, ndlp->nlp_flag);

	spin_lock_irq(shost->host_lock);
	vport->fc_flag |= FC_RSCN_MODE;
	spin_unlock_irq(shost->host_lock);
	vport->fc_rscn_id_list[vport->fc_rscn_id_cnt++] = pcmd;
	
	vport->fc_rscn_flush = 0;
	cmdiocb->context2 = NULL;
	lpfc_set_disctmo(vport);
	
	lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb, ndlp, NULL);
	
	lpfc_rscn_recovery_check(vport);
	return lpfc_els_handle_rscn(vport);
}

int
lpfc_els_handle_rscn(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp;
	struct lpfc_hba *phba = vport->phba;

	
	if (vport->load_flag & FC_UNLOADING) {
		lpfc_els_flush_rscn(vport);
		return 0;
	}

	
	lpfc_set_disctmo(vport);

	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0215 RSCN processed Data: x%x x%x x%x x%x\n",
			 vport->fc_flag, 0, vport->fc_rscn_id_cnt,
			 vport->port_state);

	
	vport->fc_ns_retry = 0;
	vport->num_disc_nodes = 0;

	ndlp = lpfc_findnode_did(vport, NameServer_DID);
	if (ndlp && NLP_CHK_NODE_ACT(ndlp)
	    && ndlp->nlp_state == NLP_STE_UNMAPPED_NODE) {
		
		if (lpfc_ns_cmd(vport, SLI_CTNS_GID_FT, 0, 0) == 0)
			return 1;
	} else {
		
		
		ndlp = lpfc_findnode_did(vport, NameServer_DID);
		if (ndlp && NLP_CHK_NODE_ACT(ndlp))
			return 1;

		if (ndlp) {
			ndlp = lpfc_enable_node(vport, ndlp,
						NLP_STE_PLOGI_ISSUE);
			if (!ndlp) {
				lpfc_els_flush_rscn(vport);
				return 0;
			}
			ndlp->nlp_prev_state = NLP_STE_UNUSED_NODE;
		} else {
			ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
			if (!ndlp) {
				lpfc_els_flush_rscn(vport);
				return 0;
			}
			lpfc_nlp_init(vport, ndlp, NameServer_DID);
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_PLOGI_ISSUE);
		}
		ndlp->nlp_type |= NLP_FABRIC;
		lpfc_issue_els_plogi(vport, NameServer_DID, 0);
		return 1;
	}

	lpfc_els_flush_rscn(vport);
	return 0;
}

static int
lpfc_els_rcv_flogi(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		   struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_dmabuf *pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;
	uint32_t *lp = (uint32_t *) pcmd->virt;
	IOCB_t *icmd = &cmdiocb->iocb;
	struct serv_parm *sp;
	LPFC_MBOXQ_t *mbox;
	struct ls_rjt stat;
	uint32_t cmd, did;
	int rc;

	cmd = *lp++;
	sp = (struct serv_parm *) lp;

	

	lpfc_set_disctmo(vport);

	if (phba->fc_topology == LPFC_TOPOLOGY_LOOP) {
		
		did = icmd->un.elsreq64.remoteID;

		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "0113 An FLOGI ELS command x%x was "
				 "received from DID x%x in Loop Mode\n",
				 cmd, did);
		return 1;
	}

	did = Fabric_DID;

	if ((lpfc_check_sparm(vport, ndlp, sp, CLASS3, 1))) {

		rc = memcmp(&vport->fc_portname, &sp->portName,
			    sizeof(struct lpfc_name));

		if (!rc) {
			if (phba->sli_rev < LPFC_SLI_REV4) {
				mbox = mempool_alloc(phba->mbox_mem_pool,
						     GFP_KERNEL);
				if (!mbox)
					return 1;
				lpfc_linkdown(phba);
				lpfc_init_link(phba, mbox,
					       phba->cfg_topology,
					       phba->cfg_link_speed);
				mbox->u.mb.un.varInitLnk.lipsr_AL_PA = 0;
				mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
				mbox->vport = vport;
				rc = lpfc_sli_issue_mbox(phba, mbox,
							 MBX_NOWAIT);
				lpfc_set_loopback_flag(phba);
				if (rc == MBX_NOT_FINISHED)
					mempool_free(mbox, phba->mbox_mem_pool);
				return 1;
			} else {
				lpfc_els_abort_flogi(phba);
				return 0;
			}
		} else if (rc > 0) {	
			spin_lock_irq(shost->host_lock);
			vport->fc_flag |= FC_PT2PT_PLOGI;
			spin_unlock_irq(shost->host_lock);
		}
		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_PT2PT;
		vport->fc_flag &= ~(FC_FABRIC | FC_PUBLIC_LOOP);
		spin_unlock_irq(shost->host_lock);
	} else {
		
		stat.un.b.lsRjtRsvd0 = 0;
		stat.un.b.lsRjtRsnCode = LSRJT_UNABLE_TPC;
		stat.un.b.lsRjtRsnCodeExp = LSEXP_SPARM_OPTIONS;
		stat.un.b.vendorUnique = 0;
		lpfc_els_rsp_reject(vport, stat.un.lsRjtError, cmdiocb, ndlp,
			NULL);
		return 1;
	}

	
	lpfc_els_rsp_acc(vport, ELS_CMD_PLOGI, cmdiocb, ndlp, NULL);

	return 0;
}

static int
lpfc_els_rcv_rnid(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_nodelist *ndlp)
{
	struct lpfc_dmabuf *pcmd;
	uint32_t *lp;
	IOCB_t *icmd;
	RNID *rn;
	struct ls_rjt stat;
	uint32_t cmd, did;

	icmd = &cmdiocb->iocb;
	did = icmd->un.elsreq64.remoteID;
	pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;
	lp = (uint32_t *) pcmd->virt;

	cmd = *lp++;
	rn = (RNID *) lp;

	

	switch (rn->Format) {
	case 0:
	case RNID_TOPOLOGY_DISC:
		
		lpfc_els_rsp_rnid_acc(vport, rn->Format, cmdiocb, ndlp);
		break;
	default:
		
		stat.un.b.lsRjtRsvd0 = 0;
		stat.un.b.lsRjtRsnCode = LSRJT_UNABLE_TPC;
		stat.un.b.lsRjtRsnCodeExp = LSEXP_CANT_GIVE_DATA;
		stat.un.b.vendorUnique = 0;
		lpfc_els_rsp_reject(vport, stat.un.lsRjtError, cmdiocb, ndlp,
			NULL);
	}
	return 0;
}

static int
lpfc_els_rcv_echo(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_nodelist *ndlp)
{
	uint8_t *pcmd;

	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) cmdiocb->context2)->virt);

	
	pcmd += sizeof(uint32_t);

	lpfc_els_rsp_echo_acc(vport, pcmd, cmdiocb, ndlp);
	return 0;
}

static int
lpfc_els_rcv_lirr(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_nodelist *ndlp)
{
	struct ls_rjt stat;

	
	stat.un.b.lsRjtRsvd0 = 0;
	stat.un.b.lsRjtRsnCode = LSRJT_UNABLE_TPC;
	stat.un.b.lsRjtRsnCodeExp = LSEXP_CANT_GIVE_DATA;
	stat.un.b.vendorUnique = 0;
	lpfc_els_rsp_reject(vport, stat.un.lsRjtError, cmdiocb, ndlp, NULL);
	return 0;
}

static void
lpfc_els_rcv_rrq(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		 struct lpfc_nodelist *ndlp)
{
	lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb, ndlp, NULL);
	if (vport->phba->sli_rev == LPFC_SLI_REV4)
		lpfc_els_clear_rrq(vport, cmdiocb, ndlp);
}

static void
lpfc_els_rsp_rls_acc(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	MAILBOX_t *mb;
	IOCB_t *icmd;
	struct RLS_RSP *rls_rsp;
	uint8_t *pcmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_nodelist *ndlp;
	uint16_t oxid;
	uint16_t rxid;
	uint32_t cmdsize;

	mb = &pmb->u.mb;

	ndlp = (struct lpfc_nodelist *) pmb->context2;
	rxid = (uint16_t) ((unsigned long)(pmb->context1) & 0xffff);
	oxid = (uint16_t) (((unsigned long)(pmb->context1) >> 16) & 0xffff);
	pmb->context1 = NULL;
	pmb->context2 = NULL;

	if (mb->mbxStatus) {
		mempool_free(pmb, phba->mbox_mem_pool);
		return;
	}

	cmdsize = sizeof(struct RLS_RSP) + sizeof(uint32_t);
	mempool_free(pmb, phba->mbox_mem_pool);
	elsiocb = lpfc_prep_els_iocb(phba->pport, 0, cmdsize,
				     lpfc_max_els_tries, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ACC);

	
	lpfc_nlp_put(ndlp);

	if (!elsiocb)
		return;

	icmd = &elsiocb->iocb;
	icmd->ulpContext = rxid;
	icmd->unsli3.rcvsli3.ox_id = oxid;

	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);
	*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
	pcmd += sizeof(uint32_t); 
	rls_rsp = (struct RLS_RSP *)pcmd;

	rls_rsp->linkFailureCnt = cpu_to_be32(mb->un.varRdLnk.linkFailureCnt);
	rls_rsp->lossSyncCnt = cpu_to_be32(mb->un.varRdLnk.lossSyncCnt);
	rls_rsp->lossSignalCnt = cpu_to_be32(mb->un.varRdLnk.lossSignalCnt);
	rls_rsp->primSeqErrCnt = cpu_to_be32(mb->un.varRdLnk.primSeqErrCnt);
	rls_rsp->invalidXmitWord = cpu_to_be32(mb->un.varRdLnk.invalidXmitWord);
	rls_rsp->crcCnt = cpu_to_be32(mb->un.varRdLnk.crcCnt);

	
	lpfc_printf_vlog(ndlp->vport, KERN_INFO, LOG_ELS,
			 "2874 Xmit ELS RLS ACC response tag x%x xri x%x, "
			 "did x%x, nlp_flag x%x, nlp_state x%x, rpi x%x\n",
			 elsiocb->iotag, elsiocb->iocb.ulpContext,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);
	elsiocb->iocb_cmpl = lpfc_cmpl_els_rsp;
	phba->fc_stat.elsXmitACC++;
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) == IOCB_ERROR)
		lpfc_els_free_iocb(phba, elsiocb);
}

static void
lpfc_els_rsp_rps_acc(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	MAILBOX_t *mb;
	IOCB_t *icmd;
	RPS_RSP *rps_rsp;
	uint8_t *pcmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_nodelist *ndlp;
	uint16_t status;
	uint16_t oxid;
	uint16_t rxid;
	uint32_t cmdsize;

	mb = &pmb->u.mb;

	ndlp = (struct lpfc_nodelist *) pmb->context2;
	rxid = (uint16_t) ((unsigned long)(pmb->context1) & 0xffff);
	oxid = (uint16_t) (((unsigned long)(pmb->context1) >> 16) & 0xffff);
	pmb->context1 = NULL;
	pmb->context2 = NULL;

	if (mb->mbxStatus) {
		mempool_free(pmb, phba->mbox_mem_pool);
		return;
	}

	cmdsize = sizeof(RPS_RSP) + sizeof(uint32_t);
	mempool_free(pmb, phba->mbox_mem_pool);
	elsiocb = lpfc_prep_els_iocb(phba->pport, 0, cmdsize,
				     lpfc_max_els_tries, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ACC);

	
	lpfc_nlp_put(ndlp);

	if (!elsiocb)
		return;

	icmd = &elsiocb->iocb;
	icmd->ulpContext = rxid;
	icmd->unsli3.rcvsli3.ox_id = oxid;

	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);
	*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
	pcmd += sizeof(uint32_t); 
	rps_rsp = (RPS_RSP *)pcmd;

	if (phba->fc_topology != LPFC_TOPOLOGY_LOOP)
		status = 0x10;
	else
		status = 0x8;
	if (phba->pport->fc_flag & FC_FABRIC)
		status |= 0x4;

	rps_rsp->rsvd1 = 0;
	rps_rsp->portStatus = cpu_to_be16(status);
	rps_rsp->linkFailureCnt = cpu_to_be32(mb->un.varRdLnk.linkFailureCnt);
	rps_rsp->lossSyncCnt = cpu_to_be32(mb->un.varRdLnk.lossSyncCnt);
	rps_rsp->lossSignalCnt = cpu_to_be32(mb->un.varRdLnk.lossSignalCnt);
	rps_rsp->primSeqErrCnt = cpu_to_be32(mb->un.varRdLnk.primSeqErrCnt);
	rps_rsp->invalidXmitWord = cpu_to_be32(mb->un.varRdLnk.invalidXmitWord);
	rps_rsp->crcCnt = cpu_to_be32(mb->un.varRdLnk.crcCnt);
	
	lpfc_printf_vlog(ndlp->vport, KERN_INFO, LOG_ELS,
			 "0118 Xmit ELS RPS ACC response tag x%x xri x%x, "
			 "did x%x, nlp_flag x%x, nlp_state x%x, rpi x%x\n",
			 elsiocb->iotag, elsiocb->iocb.ulpContext,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);
	elsiocb->iocb_cmpl = lpfc_cmpl_els_rsp;
	phba->fc_stat.elsXmitACC++;
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) == IOCB_ERROR)
		lpfc_els_free_iocb(phba, elsiocb);
	return;
}

static int
lpfc_els_rcv_rls(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		 struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba *phba = vport->phba;
	LPFC_MBOXQ_t *mbox;
	struct lpfc_dmabuf *pcmd;
	struct ls_rjt stat;

	if ((ndlp->nlp_state != NLP_STE_UNMAPPED_NODE) &&
	    (ndlp->nlp_state != NLP_STE_MAPPED_NODE))
		
		goto reject_out;

	pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_ATOMIC);
	if (mbox) {
		lpfc_read_lnk_stat(phba, mbox);
		mbox->context1 = (void *)((unsigned long)
			((cmdiocb->iocb.unsli3.rcvsli3.ox_id << 16) |
			cmdiocb->iocb.ulpContext)); 
		mbox->context2 = lpfc_nlp_get(ndlp);
		mbox->vport = vport;
		mbox->mbox_cmpl = lpfc_els_rsp_rls_acc;
		if (lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT)
			!= MBX_NOT_FINISHED)
			
			return 0;
		lpfc_nlp_put(ndlp);
		mempool_free(mbox, phba->mbox_mem_pool);
	}
reject_out:
	
	stat.un.b.lsRjtRsvd0 = 0;
	stat.un.b.lsRjtRsnCode = LSRJT_UNABLE_TPC;
	stat.un.b.lsRjtRsnCodeExp = LSEXP_CANT_GIVE_DATA;
	stat.un.b.vendorUnique = 0;
	lpfc_els_rsp_reject(vport, stat.un.lsRjtError, cmdiocb, ndlp, NULL);
	return 0;
}

static int
lpfc_els_rcv_rtv(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		 struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba *phba = vport->phba;
	struct ls_rjt stat;
	struct RTV_RSP *rtv_rsp;
	uint8_t *pcmd;
	struct lpfc_iocbq *elsiocb;
	uint32_t cmdsize;


	if ((ndlp->nlp_state != NLP_STE_UNMAPPED_NODE) &&
	    (ndlp->nlp_state != NLP_STE_MAPPED_NODE))
		
		goto reject_out;

	cmdsize = sizeof(struct RTV_RSP) + sizeof(uint32_t);
	elsiocb = lpfc_prep_els_iocb(phba->pport, 0, cmdsize,
				     lpfc_max_els_tries, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ACC);

	if (!elsiocb)
		return 1;

	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);
		*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
	pcmd += sizeof(uint32_t); 

	
	elsiocb->iocb.ulpContext = cmdiocb->iocb.ulpContext;  
	elsiocb->iocb.unsli3.rcvsli3.ox_id = cmdiocb->iocb.unsli3.rcvsli3.ox_id;

	rtv_rsp = (struct RTV_RSP *)pcmd;

	
	rtv_rsp->ratov = cpu_to_be32(phba->fc_ratov * 1000); 
	rtv_rsp->edtov = cpu_to_be32(phba->fc_edtov);
	bf_set(qtov_edtovres, rtv_rsp, phba->fc_edtovResol ? 1 : 0);
	bf_set(qtov_rttov, rtv_rsp, 0); 
	rtv_rsp->qtov = cpu_to_be32(rtv_rsp->qtov);

	
	lpfc_printf_vlog(ndlp->vport, KERN_INFO, LOG_ELS,
			 "2875 Xmit ELS RTV ACC response tag x%x xri x%x, "
			 "did x%x, nlp_flag x%x, nlp_state x%x, rpi x%x, "
			 "Data: x%x x%x x%x\n",
			 elsiocb->iotag, elsiocb->iocb.ulpContext,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi,
			rtv_rsp->ratov, rtv_rsp->edtov, rtv_rsp->qtov);
	elsiocb->iocb_cmpl = lpfc_cmpl_els_rsp;
	phba->fc_stat.elsXmitACC++;
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) == IOCB_ERROR)
		lpfc_els_free_iocb(phba, elsiocb);
	return 0;

reject_out:
	
	stat.un.b.lsRjtRsvd0 = 0;
	stat.un.b.lsRjtRsnCode = LSRJT_UNABLE_TPC;
	stat.un.b.lsRjtRsnCodeExp = LSEXP_CANT_GIVE_DATA;
	stat.un.b.vendorUnique = 0;
	lpfc_els_rsp_reject(vport, stat.un.lsRjtError, cmdiocb, ndlp, NULL);
	return 0;
}

static int
lpfc_els_rcv_rps(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		 struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba *phba = vport->phba;
	uint32_t *lp;
	uint8_t flag;
	LPFC_MBOXQ_t *mbox;
	struct lpfc_dmabuf *pcmd;
	RPS *rps;
	struct ls_rjt stat;

	if ((ndlp->nlp_state != NLP_STE_UNMAPPED_NODE) &&
	    (ndlp->nlp_state != NLP_STE_MAPPED_NODE))
		
		goto reject_out;

	pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;
	lp = (uint32_t *) pcmd->virt;
	flag = (be32_to_cpu(*lp++) & 0xf);
	rps = (RPS *) lp;

	if ((flag == 0) ||
	    ((flag == 1) && (be32_to_cpu(rps->un.portNum) == 0)) ||
	    ((flag == 2) && (memcmp(&rps->un.portName, &vport->fc_portname,
				    sizeof(struct lpfc_name)) == 0))) {

		printk("Fix me....\n");
		dump_stack();
		mbox = mempool_alloc(phba->mbox_mem_pool, GFP_ATOMIC);
		if (mbox) {
			lpfc_read_lnk_stat(phba, mbox);
			mbox->context1 = (void *)((unsigned long)
				((cmdiocb->iocb.unsli3.rcvsli3.ox_id << 16) |
				cmdiocb->iocb.ulpContext)); 
			mbox->context2 = lpfc_nlp_get(ndlp);
			mbox->vport = vport;
			mbox->mbox_cmpl = lpfc_els_rsp_rps_acc;
			if (lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT)
				!= MBX_NOT_FINISHED)
				
				return 0;
			lpfc_nlp_put(ndlp);
			mempool_free(mbox, phba->mbox_mem_pool);
		}
	}

reject_out:
	
	stat.un.b.lsRjtRsvd0 = 0;
	stat.un.b.lsRjtRsnCode = LSRJT_UNABLE_TPC;
	stat.un.b.lsRjtRsnCodeExp = LSEXP_CANT_GIVE_DATA;
	stat.un.b.vendorUnique = 0;
	lpfc_els_rsp_reject(vport, stat.un.lsRjtError, cmdiocb, ndlp, NULL);
	return 0;
}

static int
lpfc_issue_els_rrq(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
			uint32_t did, struct lpfc_node_rrq *rrq)
{
	struct lpfc_hba  *phba = vport->phba;
	struct RRQ *els_rrq;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int ret;


	if (ndlp != rrq->ndlp)
		ndlp = rrq->ndlp;
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp))
		return 1;

	
	cmdsize = (sizeof(uint32_t) + sizeof(struct RRQ));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, 0, ndlp, did,
				     ELS_CMD_RRQ);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	
	*((uint32_t *) (pcmd)) = ELS_CMD_RRQ;
	pcmd += sizeof(uint32_t);
	els_rrq = (struct RRQ *) pcmd;

	bf_set(rrq_oxid, els_rrq, rrq->xritag);
	bf_set(rrq_rxid, els_rrq, rrq->rxid);
	bf_set(rrq_did, els_rrq, vport->fc_myDID);
	els_rrq->rrq = cpu_to_be32(els_rrq->rrq);
	els_rrq->rrq_exchg = cpu_to_be32(els_rrq->rrq_exchg);


	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue RRQ:     did:x%x",
		did, rrq->xritag, rrq->rxid);
	elsiocb->context_un.rrq = rrq;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_rrq;
	ret = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);

	if (ret == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

int
lpfc_send_rrq(struct lpfc_hba *phba, struct lpfc_node_rrq *rrq)
{
	struct lpfc_nodelist *ndlp = lpfc_findnode_did(rrq->vport,
							rrq->nlp_DID);
	if (lpfc_test_rrq_active(phba, ndlp, rrq->xritag))
		return lpfc_issue_els_rrq(rrq->vport, ndlp,
					 rrq->nlp_DID, rrq);
	else
		return 1;
}

static int
lpfc_els_rsp_rpl_acc(struct lpfc_vport *vport, uint16_t cmdsize,
		     struct lpfc_iocbq *oldiocb, struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba *phba = vport->phba;
	IOCB_t *icmd, *oldcmd;
	RPL_RSP rpl_rsp;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;

	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ACC);

	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	oldcmd = &oldiocb->iocb;
	icmd->ulpContext = oldcmd->ulpContext;	
	icmd->unsli3.rcvsli3.ox_id = oldcmd->unsli3.rcvsli3.ox_id;

	pcmd = (((struct lpfc_dmabuf *) elsiocb->context2)->virt);
	*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
	pcmd += sizeof(uint16_t);
	*((uint16_t *)(pcmd)) = be16_to_cpu(cmdsize);
	pcmd += sizeof(uint16_t);

	
	rpl_rsp.listLen = be32_to_cpu(1);
	rpl_rsp.index = 0;
	rpl_rsp.port_num_blk.portNum = 0;
	rpl_rsp.port_num_blk.portID = be32_to_cpu(vport->fc_myDID);
	memcpy(&rpl_rsp.port_num_blk.portName, &vport->fc_portname,
	    sizeof(struct lpfc_name));
	memcpy(pcmd, &rpl_rsp, cmdsize - sizeof(uint32_t));
	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0120 Xmit ELS RPL ACC response tag x%x "
			 "xri x%x, did x%x, nlp_flag x%x, nlp_state x%x, "
			 "rpi x%x\n",
			 elsiocb->iotag, elsiocb->iocb.ulpContext,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);
	elsiocb->iocb_cmpl = lpfc_cmpl_els_rsp;
	phba->fc_stat.elsXmitACC++;
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) ==
	    IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

static int
lpfc_els_rcv_rpl(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		 struct lpfc_nodelist *ndlp)
{
	struct lpfc_dmabuf *pcmd;
	uint32_t *lp;
	uint32_t maxsize;
	uint16_t cmdsize;
	RPL *rpl;
	struct ls_rjt stat;

	if ((ndlp->nlp_state != NLP_STE_UNMAPPED_NODE) &&
	    (ndlp->nlp_state != NLP_STE_MAPPED_NODE)) {
		
		stat.un.b.lsRjtRsvd0 = 0;
		stat.un.b.lsRjtRsnCode = LSRJT_UNABLE_TPC;
		stat.un.b.lsRjtRsnCodeExp = LSEXP_CANT_GIVE_DATA;
		stat.un.b.vendorUnique = 0;
		lpfc_els_rsp_reject(vport, stat.un.lsRjtError, cmdiocb, ndlp,
			NULL);
		
		return 0;
	}

	pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;
	lp = (uint32_t *) pcmd->virt;
	rpl = (RPL *) (lp + 1);
	maxsize = be32_to_cpu(rpl->maxsize);

	
	if ((rpl->index == 0) &&
	    ((maxsize == 0) ||
	     ((maxsize * sizeof(uint32_t)) >= sizeof(RPL_RSP)))) {
		cmdsize = sizeof(uint32_t) + sizeof(RPL_RSP);
	} else {
		cmdsize = sizeof(uint32_t) + maxsize * sizeof(uint32_t);
	}
	lpfc_els_rsp_rpl_acc(vport, cmdsize, cmdiocb, ndlp);

	return 0;
}

static int
lpfc_els_rcv_farp(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_nodelist *ndlp)
{
	struct lpfc_dmabuf *pcmd;
	uint32_t *lp;
	IOCB_t *icmd;
	FARP *fp;
	uint32_t cmd, cnt, did;

	icmd = &cmdiocb->iocb;
	did = icmd->un.elsreq64.remoteID;
	pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;
	lp = (uint32_t *) pcmd->virt;

	cmd = *lp++;
	fp = (FARP *) lp;
	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0601 FARP-REQ received from DID x%x\n", did);
	
	if (fp->Mflags & ~(FARP_MATCH_NODE | FARP_MATCH_PORT)) {
		return 0;
	}

	cnt = 0;
	
	if (fp->Mflags & FARP_MATCH_PORT) {
		if (memcmp(&fp->RportName, &vport->fc_portname,
			   sizeof(struct lpfc_name)) == 0)
			cnt = 1;
	}

	
	if (fp->Mflags & FARP_MATCH_NODE) {
		if (memcmp(&fp->RnodeName, &vport->fc_nodename,
			   sizeof(struct lpfc_name)) == 0)
			cnt = 1;
	}

	if (cnt) {
		if ((ndlp->nlp_state == NLP_STE_UNMAPPED_NODE) ||
		   (ndlp->nlp_state == NLP_STE_MAPPED_NODE)) {
			
			if (fp->Rflags & FARP_REQUEST_PLOGI) {
				ndlp->nlp_prev_state = ndlp->nlp_state;
				lpfc_nlp_set_state(vport, ndlp,
						   NLP_STE_PLOGI_ISSUE);
				lpfc_issue_els_plogi(vport, ndlp->nlp_DID, 0);
			}

			
			if (fp->Rflags & FARP_REQUEST_FARPR)
				lpfc_issue_els_farpr(vport, did, 0);
		}
	}
	return 0;
}

static int
lpfc_els_rcv_farpr(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		   struct lpfc_nodelist  *ndlp)
{
	struct lpfc_dmabuf *pcmd;
	uint32_t *lp;
	IOCB_t *icmd;
	uint32_t cmd, did;

	icmd = &cmdiocb->iocb;
	did = icmd->un.elsreq64.remoteID;
	pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;
	lp = (uint32_t *) pcmd->virt;

	cmd = *lp++;
	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0600 FARP-RSP received from DID x%x\n", did);
	
	lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb, ndlp, NULL);

	return 0;
}

static int
lpfc_els_rcv_fan(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		 struct lpfc_nodelist *fan_ndlp)
{
	struct lpfc_hba *phba = vport->phba;
	uint32_t *lp;
	FAN *fp;

	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS, "0265 FAN received\n");
	lp = (uint32_t *)((struct lpfc_dmabuf *)cmdiocb->context2)->virt;
	fp = (FAN *) ++lp;
	
	if ((vport == phba->pport) &&
	    (vport->port_state == LPFC_LOCAL_CFG_LINK)) {
		if ((memcmp(&phba->fc_fabparam.nodeName, &fp->FnodeName,
			    sizeof(struct lpfc_name))) ||
		    (memcmp(&phba->fc_fabparam.portName, &fp->FportName,
			    sizeof(struct lpfc_name)))) {
			
			lpfc_issue_init_vfi(vport);
		} else {
			
			vport->fc_myDID = vport->fc_prevDID;
			if (phba->sli_rev < LPFC_SLI_REV4)
				lpfc_issue_fabric_reglogin(vport);
			else {
				lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
					"3138 Need register VFI: (x%x/%x)\n",
					vport->fc_prevDID, vport->fc_myDID);
				lpfc_issue_reg_vfi(vport);
			}
		}
	}
	return 0;
}

void
lpfc_els_timeout(unsigned long ptr)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) ptr;
	struct lpfc_hba   *phba = vport->phba;
	uint32_t tmo_posted;
	unsigned long iflag;

	spin_lock_irqsave(&vport->work_port_lock, iflag);
	tmo_posted = vport->work_port_events & WORKER_ELS_TMO;
	if (!tmo_posted)
		vport->work_port_events |= WORKER_ELS_TMO;
	spin_unlock_irqrestore(&vport->work_port_lock, iflag);

	if (!tmo_posted)
		lpfc_worker_wake_up(phba);
	return;
}


void
lpfc_els_timeout_handler(struct lpfc_vport *vport)
{
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_sli_ring *pring;
	struct lpfc_iocbq *tmp_iocb, *piocb;
	IOCB_t *cmd = NULL;
	struct lpfc_dmabuf *pcmd;
	uint32_t els_command = 0;
	uint32_t timeout;
	uint32_t remote_ID = 0xffffffff;
	LIST_HEAD(txcmplq_completions);
	LIST_HEAD(abort_list);


	timeout = (uint32_t)(phba->fc_ratov << 1);

	pring = &phba->sli.ring[LPFC_ELS_RING];

	spin_lock_irq(&phba->hbalock);
	list_splice_init(&pring->txcmplq, &txcmplq_completions);
	spin_unlock_irq(&phba->hbalock);

	list_for_each_entry_safe(piocb, tmp_iocb, &txcmplq_completions, list) {
		cmd = &piocb->iocb;

		if ((piocb->iocb_flag & LPFC_IO_LIBDFC) != 0 ||
		    piocb->iocb.ulpCommand == CMD_ABORT_XRI_CN ||
		    piocb->iocb.ulpCommand == CMD_CLOSE_XRI_CN)
			continue;

		if (piocb->vport != vport)
			continue;

		pcmd = (struct lpfc_dmabuf *) piocb->context2;
		if (pcmd)
			els_command = *(uint32_t *) (pcmd->virt);

		if (els_command == ELS_CMD_FARP ||
		    els_command == ELS_CMD_FARPR ||
		    els_command == ELS_CMD_FDISC)
			continue;

		if (piocb->drvrTimeout > 0) {
			if (piocb->drvrTimeout >= timeout)
				piocb->drvrTimeout -= timeout;
			else
				piocb->drvrTimeout = 0;
			continue;
		}

		remote_ID = 0xffffffff;
		if (cmd->ulpCommand != CMD_GEN_REQUEST64_CR)
			remote_ID = cmd->un.elsreq64.remoteID;
		else {
			struct lpfc_nodelist *ndlp;
			ndlp = __lpfc_findnode_rpi(vport, cmd->ulpContext);
			if (ndlp && NLP_CHK_NODE_ACT(ndlp))
				remote_ID = ndlp->nlp_DID;
		}
		list_add_tail(&piocb->dlist, &abort_list);
	}
	spin_lock_irq(&phba->hbalock);
	list_splice(&txcmplq_completions, &pring->txcmplq);
	spin_unlock_irq(&phba->hbalock);

	list_for_each_entry_safe(piocb, tmp_iocb, &abort_list, dlist) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
			 "0127 ELS timeout Data: x%x x%x x%x "
			 "x%x\n", els_command,
			 remote_ID, cmd->ulpCommand, cmd->ulpIoTag);
		spin_lock_irq(&phba->hbalock);
		list_del_init(&piocb->dlist);
		lpfc_sli_issue_abort_iotag(phba, pring, piocb);
		spin_unlock_irq(&phba->hbalock);
	}

	if (phba->sli.ring[LPFC_ELS_RING].txcmplq_cnt)
		mod_timer(&vport->els_tmofunc, jiffies + HZ * timeout);
}

void
lpfc_els_flush_cmd(struct lpfc_vport *vport)
{
	LIST_HEAD(completions);
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_ELS_RING];
	struct lpfc_iocbq *tmp_iocb, *piocb;
	IOCB_t *cmd = NULL;

	lpfc_fabric_abort_vport(vport);

	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(piocb, tmp_iocb, &pring->txq, list) {
		cmd = &piocb->iocb;

		if (piocb->iocb_flag & LPFC_IO_LIBDFC) {
			continue;
		}

		
		if (cmd->ulpCommand == CMD_QUE_RING_BUF_CN ||
		    cmd->ulpCommand == CMD_QUE_RING_BUF64_CN ||
		    cmd->ulpCommand == CMD_CLOSE_XRI_CN ||
		    cmd->ulpCommand == CMD_ABORT_XRI_CN)
			continue;

		if (piocb->vport != vport)
			continue;

		list_move_tail(&piocb->list, &completions);
		pring->txq_cnt--;
	}

	list_for_each_entry_safe(piocb, tmp_iocb, &pring->txcmplq, list) {
		if (piocb->iocb_flag & LPFC_IO_LIBDFC) {
			continue;
		}

		if (piocb->vport != vport)
			continue;

		lpfc_sli_issue_abort_iotag(phba, pring, piocb);
	}
	spin_unlock_irq(&phba->hbalock);

	
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);

	return;
}

void
lpfc_els_flush_all_cmd(struct lpfc_hba  *phba)
{
	LIST_HEAD(completions);
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_ELS_RING];
	struct lpfc_iocbq *tmp_iocb, *piocb;
	IOCB_t *cmd = NULL;

	lpfc_fabric_abort_hba(phba);
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(piocb, tmp_iocb, &pring->txq, list) {
		cmd = &piocb->iocb;
		if (piocb->iocb_flag & LPFC_IO_LIBDFC)
			continue;
		
		if (cmd->ulpCommand == CMD_QUE_RING_BUF_CN ||
		    cmd->ulpCommand == CMD_QUE_RING_BUF64_CN ||
		    cmd->ulpCommand == CMD_CLOSE_XRI_CN ||
		    cmd->ulpCommand == CMD_ABORT_XRI_CN)
			continue;
		list_move_tail(&piocb->list, &completions);
		pring->txq_cnt--;
	}
	list_for_each_entry_safe(piocb, tmp_iocb, &pring->txcmplq, list) {
		if (piocb->iocb_flag & LPFC_IO_LIBDFC)
			continue;
		lpfc_sli_issue_abort_iotag(phba, pring, piocb);
	}
	spin_unlock_irq(&phba->hbalock);

	
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);

	return;
}

void
lpfc_send_els_failure_event(struct lpfc_hba *phba,
			struct lpfc_iocbq *cmdiocbp,
			struct lpfc_iocbq *rspiocbp)
{
	struct lpfc_vport *vport = cmdiocbp->vport;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_lsrjt_event lsrjt_event;
	struct lpfc_fabric_event_header fabric_event;
	struct ls_rjt stat;
	struct lpfc_nodelist *ndlp;
	uint32_t *pcmd;

	ndlp = cmdiocbp->context1;
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp))
		return;

	if (rspiocbp->iocb.ulpStatus == IOSTAT_LS_RJT) {
		lsrjt_event.header.event_type = FC_REG_ELS_EVENT;
		lsrjt_event.header.subcategory = LPFC_EVENT_LSRJT_RCV;
		memcpy(lsrjt_event.header.wwpn, &ndlp->nlp_portname,
			sizeof(struct lpfc_name));
		memcpy(lsrjt_event.header.wwnn, &ndlp->nlp_nodename,
			sizeof(struct lpfc_name));
		pcmd = (uint32_t *) (((struct lpfc_dmabuf *)
			cmdiocbp->context2)->virt);
		lsrjt_event.command = (pcmd != NULL) ? *pcmd : 0;
		stat.un.lsRjtError = be32_to_cpu(rspiocbp->iocb.un.ulpWord[4]);
		lsrjt_event.reason_code = stat.un.b.lsRjtRsnCode;
		lsrjt_event.explanation = stat.un.b.lsRjtRsnCodeExp;
		fc_host_post_vendor_event(shost,
			fc_get_event_number(),
			sizeof(lsrjt_event),
			(char *)&lsrjt_event,
			LPFC_NL_VENDOR_ID);
		return;
	}
	if ((rspiocbp->iocb.ulpStatus == IOSTAT_NPORT_BSY) ||
		(rspiocbp->iocb.ulpStatus == IOSTAT_FABRIC_BSY)) {
		fabric_event.event_type = FC_REG_FABRIC_EVENT;
		if (rspiocbp->iocb.ulpStatus == IOSTAT_NPORT_BSY)
			fabric_event.subcategory = LPFC_EVENT_PORT_BUSY;
		else
			fabric_event.subcategory = LPFC_EVENT_FABRIC_BUSY;
		memcpy(fabric_event.wwpn, &ndlp->nlp_portname,
			sizeof(struct lpfc_name));
		memcpy(fabric_event.wwnn, &ndlp->nlp_nodename,
			sizeof(struct lpfc_name));
		fc_host_post_vendor_event(shost,
			fc_get_event_number(),
			sizeof(fabric_event),
			(char *)&fabric_event,
			LPFC_NL_VENDOR_ID);
		return;
	}

}

static void
lpfc_send_els_event(struct lpfc_vport *vport,
		    struct lpfc_nodelist *ndlp,
		    uint32_t *payload)
{
	struct lpfc_els_event_header *els_data = NULL;
	struct lpfc_logo_event *logo_data = NULL;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	if (*payload == ELS_CMD_LOGO) {
		logo_data = kmalloc(sizeof(struct lpfc_logo_event), GFP_KERNEL);
		if (!logo_data) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				"0148 Failed to allocate memory "
				"for LOGO event\n");
			return;
		}
		els_data = &logo_data->header;
	} else {
		els_data = kmalloc(sizeof(struct lpfc_els_event_header),
			GFP_KERNEL);
		if (!els_data) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				"0149 Failed to allocate memory "
				"for ELS event\n");
			return;
		}
	}
	els_data->event_type = FC_REG_ELS_EVENT;
	switch (*payload) {
	case ELS_CMD_PLOGI:
		els_data->subcategory = LPFC_EVENT_PLOGI_RCV;
		break;
	case ELS_CMD_PRLO:
		els_data->subcategory = LPFC_EVENT_PRLO_RCV;
		break;
	case ELS_CMD_ADISC:
		els_data->subcategory = LPFC_EVENT_ADISC_RCV;
		break;
	case ELS_CMD_LOGO:
		els_data->subcategory = LPFC_EVENT_LOGO_RCV;
		
		memcpy(logo_data->logo_wwpn, &payload[2],
			sizeof(struct lpfc_name));
		break;
	default:
		kfree(els_data);
		return;
	}
	memcpy(els_data->wwpn, &ndlp->nlp_portname, sizeof(struct lpfc_name));
	memcpy(els_data->wwnn, &ndlp->nlp_nodename, sizeof(struct lpfc_name));
	if (*payload == ELS_CMD_LOGO) {
		fc_host_post_vendor_event(shost,
			fc_get_event_number(),
			sizeof(struct lpfc_logo_event),
			(char *)logo_data,
			LPFC_NL_VENDOR_ID);
		kfree(logo_data);
	} else {
		fc_host_post_vendor_event(shost,
			fc_get_event_number(),
			sizeof(struct lpfc_els_event_header),
			(char *)els_data,
			LPFC_NL_VENDOR_ID);
		kfree(els_data);
	}

	return;
}


static void
lpfc_els_unsol_buffer(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
		      struct lpfc_vport *vport, struct lpfc_iocbq *elsiocb)
{
	struct Scsi_Host  *shost;
	struct lpfc_nodelist *ndlp;
	struct ls_rjt stat;
	uint32_t *payload;
	uint32_t cmd, did, newnode, rjt_err = 0;
	IOCB_t *icmd = &elsiocb->iocb;

	if (!vport || !(elsiocb->context2))
		goto dropit;

	newnode = 0;
	payload = ((struct lpfc_dmabuf *)elsiocb->context2)->virt;
	cmd = *payload;
	if ((phba->sli3_options & LPFC_SLI3_HBQ_ENABLED) == 0)
		lpfc_post_buffer(phba, pring, 1);

	did = icmd->un.rcvels.remoteID;
	if (icmd->ulpStatus) {
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV Unsol ELS:  status:x%x/x%x did:x%x",
			icmd->ulpStatus, icmd->un.ulpWord[4], did);
		goto dropit;
	}

	
	if (lpfc_els_chk_latt(vport))
		goto dropit;

	
	if (vport->load_flag & FC_UNLOADING)
		goto dropit;

	
	if ((vport->fc_flag & FC_DISC_DELAYED) &&
			(cmd != ELS_CMD_PLOGI))
		goto dropit;

	ndlp = lpfc_findnode_did(vport, did);
	if (!ndlp) {
		
		ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
		if (!ndlp)
			goto dropit;

		lpfc_nlp_init(vport, ndlp, did);
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		newnode = 1;
		if ((did & Fabric_DID_MASK) == Fabric_DID_MASK)
			ndlp->nlp_type |= NLP_FABRIC;
	} else if (!NLP_CHK_NODE_ACT(ndlp)) {
		ndlp = lpfc_enable_node(vport, ndlp,
					NLP_STE_UNUSED_NODE);
		if (!ndlp)
			goto dropit;
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		newnode = 1;
		if ((did & Fabric_DID_MASK) == Fabric_DID_MASK)
			ndlp->nlp_type |= NLP_FABRIC;
	} else if (ndlp->nlp_state == NLP_STE_UNUSED_NODE) {
		
		ndlp = lpfc_nlp_get(ndlp);
		if (!ndlp)
			goto dropit;
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		newnode = 1;
	}

	phba->fc_stat.elsRcvFrame++;

	elsiocb->context1 = lpfc_nlp_get(ndlp);
	elsiocb->vport = vport;

	if ((cmd & ELS_CMD_MASK) == ELS_CMD_RSCN) {
		cmd &= ELS_CMD_MASK;
	}
	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0112 ELS command x%x received from NPORT x%x "
			 "Data: x%x\n", cmd, did, vport->port_state);
	switch (cmd) {
	case ELS_CMD_PLOGI:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV PLOGI:       did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvPLOGI++;
		ndlp = lpfc_plogi_confirm_nport(phba, payload, ndlp);

		lpfc_send_els_event(vport, ndlp, payload);

		
		if (vport->fc_flag & FC_DISC_DELAYED) {
			rjt_err = LSRJT_UNABLE_TPC;
			break;
		}
		if (vport->port_state < LPFC_DISC_AUTH) {
			if (!(phba->pport->fc_flag & FC_PT2PT) ||
				(phba->pport->fc_flag & FC_PT2PT_PLOGI)) {
				rjt_err = LSRJT_UNABLE_TPC;
				break;
			}
		}

		shost = lpfc_shost_from_vport(vport);
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag &= ~NLP_TARGET_REMOVE;
		spin_unlock_irq(shost->host_lock);

		lpfc_disc_state_machine(vport, ndlp, elsiocb,
					NLP_EVT_RCV_PLOGI);

		break;
	case ELS_CMD_FLOGI:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV FLOGI:       did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvFLOGI++;
		lpfc_els_rcv_flogi(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_nlp_put(ndlp);
		break;
	case ELS_CMD_LOGO:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV LOGO:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvLOGO++;
		lpfc_send_els_event(vport, ndlp, payload);
		if (vport->port_state < LPFC_DISC_AUTH) {
			rjt_err = LSRJT_UNABLE_TPC;
			break;
		}
		lpfc_disc_state_machine(vport, ndlp, elsiocb, NLP_EVT_RCV_LOGO);
		break;
	case ELS_CMD_PRLO:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV PRLO:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvPRLO++;
		lpfc_send_els_event(vport, ndlp, payload);
		if (vport->port_state < LPFC_DISC_AUTH) {
			rjt_err = LSRJT_UNABLE_TPC;
			break;
		}
		lpfc_disc_state_machine(vport, ndlp, elsiocb, NLP_EVT_RCV_PRLO);
		break;
	case ELS_CMD_RSCN:
		phba->fc_stat.elsRcvRSCN++;
		lpfc_els_rcv_rscn(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_nlp_put(ndlp);
		break;
	case ELS_CMD_ADISC:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV ADISC:       did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		lpfc_send_els_event(vport, ndlp, payload);
		phba->fc_stat.elsRcvADISC++;
		if (vport->port_state < LPFC_DISC_AUTH) {
			rjt_err = LSRJT_UNABLE_TPC;
			break;
		}
		lpfc_disc_state_machine(vport, ndlp, elsiocb,
					NLP_EVT_RCV_ADISC);
		break;
	case ELS_CMD_PDISC:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV PDISC:       did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvPDISC++;
		if (vport->port_state < LPFC_DISC_AUTH) {
			rjt_err = LSRJT_UNABLE_TPC;
			break;
		}
		lpfc_disc_state_machine(vport, ndlp, elsiocb,
					NLP_EVT_RCV_PDISC);
		break;
	case ELS_CMD_FARPR:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV FARPR:       did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvFARPR++;
		lpfc_els_rcv_farpr(vport, elsiocb, ndlp);
		break;
	case ELS_CMD_FARP:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV FARP:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvFARP++;
		lpfc_els_rcv_farp(vport, elsiocb, ndlp);
		break;
	case ELS_CMD_FAN:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV FAN:         did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvFAN++;
		lpfc_els_rcv_fan(vport, elsiocb, ndlp);
		break;
	case ELS_CMD_PRLI:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV PRLI:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvPRLI++;
		if (vport->port_state < LPFC_DISC_AUTH) {
			rjt_err = LSRJT_UNABLE_TPC;
			break;
		}
		lpfc_disc_state_machine(vport, ndlp, elsiocb, NLP_EVT_RCV_PRLI);
		break;
	case ELS_CMD_LIRR:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV LIRR:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvLIRR++;
		lpfc_els_rcv_lirr(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_nlp_put(ndlp);
		break;
	case ELS_CMD_RLS:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RLS:         did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvRLS++;
		lpfc_els_rcv_rls(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_nlp_put(ndlp);
		break;
	case ELS_CMD_RPS:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RPS:         did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvRPS++;
		lpfc_els_rcv_rps(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_nlp_put(ndlp);
		break;
	case ELS_CMD_RPL:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RPL:         did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvRPL++;
		lpfc_els_rcv_rpl(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_nlp_put(ndlp);
		break;
	case ELS_CMD_RNID:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RNID:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvRNID++;
		lpfc_els_rcv_rnid(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_nlp_put(ndlp);
		break;
	case ELS_CMD_RTV:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RTV:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);
		phba->fc_stat.elsRcvRTV++;
		lpfc_els_rcv_rtv(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_nlp_put(ndlp);
		break;
	case ELS_CMD_RRQ:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RRQ:         did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvRRQ++;
		lpfc_els_rcv_rrq(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_nlp_put(ndlp);
		break;
	case ELS_CMD_ECHO:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV ECHO:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvECHO++;
		lpfc_els_rcv_echo(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_nlp_put(ndlp);
		break;
	default:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV ELS cmd:     cmd:x%x did:x%x/ste:x%x",
			cmd, did, vport->port_state);

		
		rjt_err = LSRJT_CMD_UNSUPPORTED;

		
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "0115 Unknown ELS command x%x "
				 "received from NPORT x%x\n", cmd, did);
		if (newnode)
			lpfc_nlp_put(ndlp);
		break;
	}

	
	if (rjt_err) {
		memset(&stat, 0, sizeof(stat));
		stat.un.b.lsRjtRsnCode = rjt_err;
		stat.un.b.lsRjtRsnCodeExp = LSEXP_NOTHING_MORE;
		lpfc_els_rsp_reject(vport, stat.un.lsRjtError, elsiocb, ndlp,
			NULL);
	}

	lpfc_nlp_put(elsiocb->context1);
	elsiocb->context1 = NULL;
	return;

dropit:
	if (vport && !(vport->load_flag & FC_UNLOADING))
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
			"0111 Dropping received ELS cmd "
			"Data: x%x x%x x%x\n",
			icmd->ulpStatus, icmd->un.ulpWord[4], icmd->ulpTimeout);
	phba->fc_stat.elsRcvDrop++;
}

void
lpfc_els_unsol_event(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
		     struct lpfc_iocbq *elsiocb)
{
	struct lpfc_vport *vport = phba->pport;
	IOCB_t *icmd = &elsiocb->iocb;
	dma_addr_t paddr;
	struct lpfc_dmabuf *bdeBuf1 = elsiocb->context2;
	struct lpfc_dmabuf *bdeBuf2 = elsiocb->context3;

	elsiocb->context1 = NULL;
	elsiocb->context2 = NULL;
	elsiocb->context3 = NULL;

	if (icmd->ulpStatus == IOSTAT_NEED_BUFFER) {
		lpfc_sli_hbqbuf_add_hbqs(phba, LPFC_ELS_HBQ);
	} else if (icmd->ulpStatus == IOSTAT_LOCAL_REJECT &&
	    (icmd->un.ulpWord[4] & 0xff) == IOERR_RCV_BUFFER_WAITING) {
		phba->fc_stat.NoRcvBuf++;
		
		if (!(phba->sli3_options & LPFC_SLI3_HBQ_ENABLED))
			lpfc_post_buffer(phba, pring, 0);
		return;
	}

	if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
	    (icmd->ulpCommand == CMD_IOCB_RCV_ELS64_CX ||
	     icmd->ulpCommand == CMD_IOCB_RCV_SEQ64_CX)) {
		if (icmd->unsli3.rcvsli3.vpi == 0xffff)
			vport = phba->pport;
		else
			vport = lpfc_find_vport_by_vpid(phba,
						icmd->unsli3.rcvsli3.vpi);
	}

	if (icmd->ulpBdeCount == 0)
		return;

	if (phba->sli3_options & LPFC_SLI3_HBQ_ENABLED) {
		elsiocb->context2 = bdeBuf1;
	} else {
		paddr = getPaddr(icmd->un.cont64[0].addrHigh,
				 icmd->un.cont64[0].addrLow);
		elsiocb->context2 = lpfc_sli_ringpostbuf_get(phba, pring,
							     paddr);
	}

	lpfc_els_unsol_buffer(phba, pring, vport, elsiocb);
	if (elsiocb->context2) {
		lpfc_in_buf_free(phba, (struct lpfc_dmabuf *)elsiocb->context2);
		elsiocb->context2 = NULL;
	}

	
	if ((phba->sli3_options & LPFC_SLI3_HBQ_ENABLED) &&
	    icmd->ulpBdeCount == 2) {
		elsiocb->context2 = bdeBuf2;
		lpfc_els_unsol_buffer(phba, pring, vport, elsiocb);
		
		if (elsiocb->context2) {
			lpfc_in_buf_free(phba, elsiocb->context2);
			elsiocb->context2 = NULL;
		}
	}
}

void
lpfc_do_scr_ns_plogi(struct lpfc_hba *phba, struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp, *ndlp_fdmi;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	spin_lock_irq(shost->host_lock);
	if (vport->fc_flag & FC_DISC_DELAYED) {
		spin_unlock_irq(shost->host_lock);
		mod_timer(&vport->delayed_disc_tmo,
			jiffies + HZ * phba->fc_ratov);
		return;
	}
	spin_unlock_irq(shost->host_lock);

	ndlp = lpfc_findnode_did(vport, NameServer_DID);
	if (!ndlp) {
		ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
		if (!ndlp) {
			if (phba->fc_topology == LPFC_TOPOLOGY_LOOP) {
				lpfc_disc_start(vport);
				return;
			}
			lpfc_vport_set_state(vport, FC_VPORT_FAILED);
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
					 "0251 NameServer login: no memory\n");
			return;
		}
		lpfc_nlp_init(vport, ndlp, NameServer_DID);
	} else if (!NLP_CHK_NODE_ACT(ndlp)) {
		ndlp = lpfc_enable_node(vport, ndlp, NLP_STE_UNUSED_NODE);
		if (!ndlp) {
			if (phba->fc_topology == LPFC_TOPOLOGY_LOOP) {
				lpfc_disc_start(vport);
				return;
			}
			lpfc_vport_set_state(vport, FC_VPORT_FAILED);
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
					"0348 NameServer login: node freed\n");
			return;
		}
	}
	ndlp->nlp_type |= NLP_FABRIC;

	lpfc_nlp_set_state(vport, ndlp, NLP_STE_PLOGI_ISSUE);

	if (lpfc_issue_els_plogi(vport, ndlp->nlp_DID, 0)) {
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "0252 Cannot issue NameServer login\n");
		return;
	}

	if (vport->cfg_fdmi_on) {
		ndlp_fdmi = lpfc_findnode_did(vport, FDMI_DID);
		if (!ndlp_fdmi) {
			ndlp_fdmi = mempool_alloc(phba->nlp_mem_pool,
						  GFP_KERNEL);
			if (ndlp_fdmi) {
				lpfc_nlp_init(vport, ndlp_fdmi, FDMI_DID);
				ndlp_fdmi->nlp_type |= NLP_FABRIC;
			} else
				return;
		}
		if (!NLP_CHK_NODE_ACT(ndlp_fdmi))
			ndlp_fdmi = lpfc_enable_node(vport,
						     ndlp_fdmi,
						     NLP_STE_NPR_NODE);

		if (ndlp_fdmi) {
			lpfc_nlp_set_state(vport, ndlp_fdmi,
					   NLP_STE_PLOGI_ISSUE);
			lpfc_issue_els_plogi(vport, ndlp_fdmi->nlp_DID, 0);
		}
	}
}

static void
lpfc_cmpl_reg_new_vport(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport *vport = pmb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) pmb->context2;
	MAILBOX_t *mb = &pmb->u.mb;
	int rc;

	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~FC_VPORT_NEEDS_REG_VPI;
	spin_unlock_irq(shost->host_lock);

	if (mb->mbxStatus) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_MBOX,
				"0915 Register VPI failed : Status: x%x"
				" upd bit: x%x \n", mb->mbxStatus,
				 mb->un.varRegVpi.upd);
		if (phba->sli_rev == LPFC_SLI_REV4 &&
			mb->un.varRegVpi.upd)
			goto mbox_err_exit ;

		switch (mb->mbxStatus) {
		case 0x11:	
		case 0x9603:	
		case 0x9602:	
			
			lpfc_vport_set_state(vport, FC_VPORT_FAILED);
			spin_lock_irq(shost->host_lock);
			vport->fc_flag &= ~(FC_FABRIC | FC_PUBLIC_LOOP);
			spin_unlock_irq(shost->host_lock);
			lpfc_can_disctmo(vport);
			break;
		
		case 0x20:
			spin_lock_irq(shost->host_lock);
			vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
			spin_unlock_irq(shost->host_lock);
			lpfc_init_vpi(phba, pmb, vport->vpi);
			pmb->vport = vport;
			pmb->mbox_cmpl = lpfc_init_vpi_cmpl;
			rc = lpfc_sli_issue_mbox(phba, pmb,
				MBX_NOWAIT);
			if (rc == MBX_NOT_FINISHED) {
				lpfc_printf_vlog(vport,
					KERN_ERR, LOG_MBOX,
					"2732 Failed to issue INIT_VPI"
					" mailbox command\n");
			} else {
				lpfc_nlp_put(ndlp);
				return;
			}

		default:
			
			if (phba->sli_rev == LPFC_SLI_REV4)
				lpfc_sli4_unreg_all_rpis(vport);
			lpfc_mbx_unreg_vpi(vport);
			spin_lock_irq(shost->host_lock);
			vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
			spin_unlock_irq(shost->host_lock);
			if (vport->port_type == LPFC_PHYSICAL_PORT
				&& !(vport->fc_flag & FC_LOGO_RCVD_DID_CHNG))
				lpfc_issue_init_vfi(vport);
			else
				lpfc_initial_fdisc(vport);
			break;
		}
	} else {
		spin_lock_irq(shost->host_lock);
		vport->vpi_state |= LPFC_VPI_REGISTERED;
		spin_unlock_irq(shost->host_lock);
		if (vport == phba->pport) {
			if (phba->sli_rev < LPFC_SLI_REV4)
				lpfc_issue_fabric_reglogin(vport);
			else {
				if (vport->port_state != LPFC_FDISC)
					lpfc_start_fdiscs(phba);
				lpfc_do_scr_ns_plogi(phba, vport);
			}
		} else
			lpfc_do_scr_ns_plogi(phba, vport);
	}
mbox_err_exit:
	lpfc_nlp_put(ndlp);

	mempool_free(pmb, phba->mbox_mem_pool);
	return;
}

void
lpfc_register_new_vport(struct lpfc_hba *phba, struct lpfc_vport *vport,
			struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	LPFC_MBOXQ_t *mbox;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (mbox) {
		lpfc_reg_vpi(vport, mbox);
		mbox->vport = vport;
		mbox->context2 = lpfc_nlp_get(ndlp);
		mbox->mbox_cmpl = lpfc_cmpl_reg_new_vport;
		if (lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT)
		    == MBX_NOT_FINISHED) {
			lpfc_nlp_put(ndlp);
			mempool_free(mbox, phba->mbox_mem_pool);

			lpfc_printf_vlog(vport, KERN_ERR, LOG_MBOX,
				"0253 Register VPI: Can't send mbox\n");
			goto mbox_err_exit;
		}
	} else {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_MBOX,
				 "0254 Register VPI: no memory\n");
		goto mbox_err_exit;
	}
	return;

mbox_err_exit:
	lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~FC_VPORT_NEEDS_REG_VPI;
	spin_unlock_irq(shost->host_lock);
	return;
}

void
lpfc_cancel_all_vport_retry_delay_timer(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	struct lpfc_nodelist *ndlp;
	uint32_t link_state;
	int i;

	
	link_state = phba->link_state;
	lpfc_linkdown(phba);
	phba->link_state = link_state;

	vports = lpfc_create_vport_work_array(phba);

	if (vports) {
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			ndlp = lpfc_findnode_did(vports[i], Fabric_DID);
			if (ndlp)
				lpfc_cancel_retry_delay_tmo(vports[i], ndlp);
			lpfc_els_flush_cmd(vports[i]);
		}
		lpfc_destroy_vport_work_array(phba, vports);
	}
}

void
lpfc_retry_pport_discovery(struct lpfc_hba *phba)
{
	struct lpfc_nodelist *ndlp;
	struct Scsi_Host  *shost;

	
	lpfc_cancel_all_vport_retry_delay_timer(phba);

	
	ndlp = lpfc_findnode_did(phba->pport, Fabric_DID);
	if (!ndlp)
		return;

	shost = lpfc_shost_from_vport(phba->pport);
	mod_timer(&ndlp->nlp_delayfunc, jiffies + HZ);
	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag |= NLP_DELAY_TMO;
	spin_unlock_irq(shost->host_lock);
	ndlp->nlp_last_elscmd = ELS_CMD_FLOGI;
	phba->pport->port_state = LPFC_FLOGI;
	return;
}

static int
lpfc_fabric_login_reqd(struct lpfc_hba *phba,
		struct lpfc_iocbq *cmdiocb,
		struct lpfc_iocbq *rspiocb)
{

	if ((rspiocb->iocb.ulpStatus != IOSTAT_FABRIC_RJT) ||
		(rspiocb->iocb.un.ulpWord[4] != RJT_LOGIN_REQUIRED))
		return 0;
	else
		return 1;
}

static void
lpfc_cmpl_els_fdisc(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		    struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) cmdiocb->context1;
	struct lpfc_nodelist *np;
	struct lpfc_nodelist *next_np;
	IOCB_t *irsp = &rspiocb->iocb;
	struct lpfc_iocbq *piocb;
	struct lpfc_dmabuf *pcmd = cmdiocb->context2, *prsp;
	struct serv_parm *sp;
	uint8_t fabric_param_changed;

	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0123 FDISC completes. x%x/x%x prevDID: x%x\n",
			 irsp->ulpStatus, irsp->un.ulpWord[4],
			 vport->fc_prevDID);
	list_for_each_entry(piocb, &phba->fabric_iocb_list, list) {
		lpfc_set_disctmo(piocb->vport);
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"FDISC cmpl:      status:x%x/x%x prevdid:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4], vport->fc_prevDID);

	if (irsp->ulpStatus) {

		if (lpfc_fabric_login_reqd(phba, cmdiocb, rspiocb)) {
			lpfc_retry_pport_discovery(phba);
			goto out;
		}

		
		if (lpfc_els_retry(phba, cmdiocb, rspiocb))
			goto out;
		
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "0126 FDISC failed. (x%x/x%x)\n",
				 irsp->ulpStatus, irsp->un.ulpWord[4]);
		goto fdisc_failed;
	}
	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~FC_VPORT_CVL_RCVD;
	vport->fc_flag &= ~FC_VPORT_LOGO_RCVD;
	vport->fc_flag |= FC_FABRIC;
	if (vport->phba->fc_topology == LPFC_TOPOLOGY_LOOP)
		vport->fc_flag |=  FC_PUBLIC_LOOP;
	spin_unlock_irq(shost->host_lock);

	vport->fc_myDID = irsp->un.ulpWord[4] & Mask_DID;
	lpfc_vport_set_state(vport, FC_VPORT_ACTIVE);
	prsp = list_get_first(&pcmd->list, struct lpfc_dmabuf, list);
	sp = prsp->virt + sizeof(uint32_t);
	fabric_param_changed = lpfc_check_clean_addr_bit(vport, sp);
	memcpy(&vport->fabric_portname, &sp->portName,
		sizeof(struct lpfc_name));
	memcpy(&vport->fabric_nodename, &sp->nodeName,
		sizeof(struct lpfc_name));
	if (fabric_param_changed &&
		!(vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)) {
		list_for_each_entry_safe(np, next_np,
			&vport->fc_nodes, nlp_listp) {
			if (!NLP_CHK_NODE_ACT(ndlp) ||
			    (np->nlp_state != NLP_STE_NPR_NODE) ||
			    !(np->nlp_flag & NLP_NPR_ADISC))
				continue;
			spin_lock_irq(shost->host_lock);
			np->nlp_flag &= ~NLP_NPR_ADISC;
			spin_unlock_irq(shost->host_lock);
			lpfc_unreg_rpi(vport, np);
		}
		lpfc_cleanup_pending_mbox(vport);

		if (phba->sli_rev == LPFC_SLI_REV4)
			lpfc_sli4_unreg_all_rpis(vport);

		lpfc_mbx_unreg_vpi(vport);
		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
		if (phba->sli_rev == LPFC_SLI_REV4)
			vport->fc_flag |= FC_VPORT_NEEDS_INIT_VPI;
		else
			vport->fc_flag |= FC_LOGO_RCVD_DID_CHNG;
		spin_unlock_irq(shost->host_lock);
	} else if ((phba->sli_rev == LPFC_SLI_REV4) &&
		!(vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)) {
		lpfc_register_new_vport(phba, vport, ndlp);
		goto out;
	}

	if (vport->fc_flag & FC_VPORT_NEEDS_INIT_VPI)
		lpfc_issue_init_vpi(vport);
	else if (vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)
		lpfc_register_new_vport(phba, vport, ndlp);
	else
		lpfc_do_scr_ns_plogi(phba, vport);
	goto out;
fdisc_failed:
	lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	
	lpfc_can_disctmo(vport);
	lpfc_nlp_put(ndlp);
out:
	lpfc_els_free_iocb(phba, cmdiocb);
}

static int
lpfc_issue_els_fdisc(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		     uint8_t retry)
{
	struct lpfc_hba *phba = vport->phba;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	struct serv_parm *sp;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int did = ndlp->nlp_DID;
	int rc;

	vport->port_state = LPFC_FDISC;
	vport->fc_myDID = 0;
	cmdsize = (sizeof(uint32_t) + sizeof(struct serv_parm));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp, did,
				     ELS_CMD_FDISC);
	if (!elsiocb) {
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "0255 Issue FDISC: no IOCB\n");
		return 1;
	}

	icmd = &elsiocb->iocb;
	icmd->un.elsreq64.myID = 0;
	icmd->un.elsreq64.fl = 1;

	if (phba->sli_rev == LPFC_SLI_REV3) {
		icmd->ulpCt_h = 1;
		icmd->ulpCt_l = 0;
	}

	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);
	*((uint32_t *) (pcmd)) = ELS_CMD_FDISC;
	pcmd += sizeof(uint32_t); 
	memcpy(pcmd, &vport->phba->pport->fc_sparam, sizeof(struct serv_parm));
	sp = (struct serv_parm *) pcmd;
	
	sp->cmn.e_d_tov = 0;
	sp->cmn.w2.r_a_tov = 0;
	sp->cmn.virtual_fabric_support = 0;
	sp->cls1.classValid = 0;
	sp->cls2.seqDelivery = 1;
	sp->cls3.seqDelivery = 1;

	pcmd += sizeof(uint32_t); 
	pcmd += sizeof(uint32_t); 
	pcmd += sizeof(uint32_t); 
	pcmd += sizeof(uint32_t); 
	memcpy(pcmd, &vport->fc_portname, 8);
	pcmd += sizeof(uint32_t); 
	pcmd += sizeof(uint32_t); 
	memcpy(pcmd, &vport->fc_nodename, 8);

	lpfc_set_disctmo(vport);

	phba->fc_stat.elsXmitFDISC++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_fdisc;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue FDISC:     did:x%x",
		did, 0, 0);

	rc = lpfc_issue_fabric_iocb(phba, elsiocb);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "0256 Issue FDISC: Cannot send IOCB\n");
		return 1;
	}
	lpfc_vport_set_state(vport, FC_VPORT_INITIALIZING);
	return 0;
}

static void
lpfc_cmpl_els_npiv_logo(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
			struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	IOCB_t *irsp;
	struct lpfc_nodelist *ndlp;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	ndlp = (struct lpfc_nodelist *)cmdiocb->context1;
	irsp = &rspiocb->iocb;
	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"LOGO npiv cmpl:  status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4], irsp->un.rcvels.remoteID);

	lpfc_els_free_iocb(phba, cmdiocb);
	vport->unreg_vpi_cmpl = VPORT_ERROR;

	
	lpfc_nlp_put(ndlp);

	
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "2928 NPIV LOGO completes to NPort x%x "
			 "Data: x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, irsp->ulpStatus, irsp->un.ulpWord[4],
			 irsp->ulpTimeout, vport->num_disc_nodes);

	if (irsp->ulpStatus == IOSTAT_SUCCESS) {
		spin_lock_irq(shost->host_lock);
		vport->fc_flag &= ~FC_FABRIC;
		spin_unlock_irq(shost->host_lock);
	}
}

int
lpfc_issue_els_npiv_logo(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;

	cmdsize = 2 * sizeof(uint32_t) + sizeof(struct lpfc_name);
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, 0, ndlp, ndlp->nlp_DID,
				     ELS_CMD_LOGO);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);
	*((uint32_t *) (pcmd)) = ELS_CMD_LOGO;
	pcmd += sizeof(uint32_t);

	
	*((uint32_t *) (pcmd)) = be32_to_cpu(vport->fc_myDID);
	pcmd += sizeof(uint32_t);
	memcpy(pcmd, &vport->fc_portname, sizeof(struct lpfc_name));

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue LOGO npiv  did:x%x flg:x%x",
		ndlp->nlp_DID, ndlp->nlp_flag, 0);

	elsiocb->iocb_cmpl = lpfc_cmpl_els_npiv_logo;
	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag |= NLP_LOGO_SND;
	spin_unlock_irq(shost->host_lock);
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) ==
	    IOCB_ERROR) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag &= ~NLP_LOGO_SND;
		spin_unlock_irq(shost->host_lock);
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

void
lpfc_fabric_block_timeout(unsigned long ptr)
{
	struct lpfc_hba  *phba = (struct lpfc_hba *) ptr;
	unsigned long iflags;
	uint32_t tmo_posted;

	spin_lock_irqsave(&phba->pport->work_port_lock, iflags);
	tmo_posted = phba->pport->work_port_events & WORKER_FABRIC_BLOCK_TMO;
	if (!tmo_posted)
		phba->pport->work_port_events |= WORKER_FABRIC_BLOCK_TMO;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, iflags);

	if (!tmo_posted)
		lpfc_worker_wake_up(phba);
	return;
}

static void
lpfc_resume_fabric_iocbs(struct lpfc_hba *phba)
{
	struct lpfc_iocbq *iocb;
	unsigned long iflags;
	int ret;
	IOCB_t *cmd;

repeat:
	iocb = NULL;
	spin_lock_irqsave(&phba->hbalock, iflags);
	
	if (atomic_read(&phba->fabric_iocb_count) == 0) {
		list_remove_head(&phba->fabric_iocb_list, iocb, typeof(*iocb),
				 list);
		if (iocb)
			
			atomic_inc(&phba->fabric_iocb_count);
	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	if (iocb) {
		iocb->fabric_iocb_cmpl = iocb->iocb_cmpl;
		iocb->iocb_cmpl = lpfc_cmpl_fabric_iocb;
		iocb->iocb_flag |= LPFC_IO_FABRIC;

		lpfc_debugfs_disc_trc(iocb->vport, LPFC_DISC_TRC_ELS_CMD,
			"Fabric sched1:   ste:x%x",
			iocb->vport->port_state, 0, 0);

		ret = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, iocb, 0);

		if (ret == IOCB_ERROR) {
			iocb->iocb_cmpl = iocb->fabric_iocb_cmpl;
			iocb->fabric_iocb_cmpl = NULL;
			iocb->iocb_flag &= ~LPFC_IO_FABRIC;
			cmd = &iocb->iocb;
			cmd->ulpStatus = IOSTAT_LOCAL_REJECT;
			cmd->un.ulpWord[4] = IOERR_SLI_ABORTED;
			iocb->iocb_cmpl(phba, iocb, iocb);

			atomic_dec(&phba->fabric_iocb_count);
			goto repeat;
		}
	}

	return;
}

void
lpfc_unblock_fabric_iocbs(struct lpfc_hba *phba)
{
	clear_bit(FABRIC_COMANDS_BLOCKED, &phba->bit_flags);

	lpfc_resume_fabric_iocbs(phba);
	return;
}

static void
lpfc_block_fabric_iocbs(struct lpfc_hba *phba)
{
	int blocked;

	blocked = test_and_set_bit(FABRIC_COMANDS_BLOCKED, &phba->bit_flags);
	
	if (!blocked)
		mod_timer(&phba->fabric_block_timer, jiffies + HZ/10 );

	return;
}

static void
lpfc_cmpl_fabric_iocb(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
	struct lpfc_iocbq *rspiocb)
{
	struct ls_rjt stat;

	if ((cmdiocb->iocb_flag & LPFC_IO_FABRIC) != LPFC_IO_FABRIC)
		BUG();

	switch (rspiocb->iocb.ulpStatus) {
		case IOSTAT_NPORT_RJT:
		case IOSTAT_FABRIC_RJT:
			if (rspiocb->iocb.un.ulpWord[4] & RJT_UNAVAIL_TEMP) {
				lpfc_block_fabric_iocbs(phba);
			}
			break;

		case IOSTAT_NPORT_BSY:
		case IOSTAT_FABRIC_BSY:
			lpfc_block_fabric_iocbs(phba);
			break;

		case IOSTAT_LS_RJT:
			stat.un.lsRjtError =
				be32_to_cpu(rspiocb->iocb.un.ulpWord[4]);
			if ((stat.un.b.lsRjtRsnCode == LSRJT_UNABLE_TPC) ||
				(stat.un.b.lsRjtRsnCode == LSRJT_LOGICAL_BSY))
				lpfc_block_fabric_iocbs(phba);
			break;
	}

	if (atomic_read(&phba->fabric_iocb_count) == 0)
		BUG();

	cmdiocb->iocb_cmpl = cmdiocb->fabric_iocb_cmpl;
	cmdiocb->fabric_iocb_cmpl = NULL;
	cmdiocb->iocb_flag &= ~LPFC_IO_FABRIC;
	cmdiocb->iocb_cmpl(phba, cmdiocb, rspiocb);

	atomic_dec(&phba->fabric_iocb_count);
	if (!test_bit(FABRIC_COMANDS_BLOCKED, &phba->bit_flags)) {
		
		lpfc_resume_fabric_iocbs(phba);
	}
}

static int
lpfc_issue_fabric_iocb(struct lpfc_hba *phba, struct lpfc_iocbq *iocb)
{
	unsigned long iflags;
	int ready;
	int ret;

	if (atomic_read(&phba->fabric_iocb_count) > 1)
		BUG();

	spin_lock_irqsave(&phba->hbalock, iflags);
	ready = atomic_read(&phba->fabric_iocb_count) == 0 &&
		!test_bit(FABRIC_COMANDS_BLOCKED, &phba->bit_flags);

	if (ready)
		
		atomic_inc(&phba->fabric_iocb_count);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	if (ready) {
		iocb->fabric_iocb_cmpl = iocb->iocb_cmpl;
		iocb->iocb_cmpl = lpfc_cmpl_fabric_iocb;
		iocb->iocb_flag |= LPFC_IO_FABRIC;

		lpfc_debugfs_disc_trc(iocb->vport, LPFC_DISC_TRC_ELS_CMD,
			"Fabric sched2:   ste:x%x",
			iocb->vport->port_state, 0, 0);

		ret = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, iocb, 0);

		if (ret == IOCB_ERROR) {
			iocb->iocb_cmpl = iocb->fabric_iocb_cmpl;
			iocb->fabric_iocb_cmpl = NULL;
			iocb->iocb_flag &= ~LPFC_IO_FABRIC;
			atomic_dec(&phba->fabric_iocb_count);
		}
	} else {
		spin_lock_irqsave(&phba->hbalock, iflags);
		list_add_tail(&iocb->list, &phba->fabric_iocb_list);
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		ret = IOCB_SUCCESS;
	}
	return ret;
}

static void lpfc_fabric_abort_vport(struct lpfc_vport *vport)
{
	LIST_HEAD(completions);
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_iocbq *tmp_iocb, *piocb;

	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(piocb, tmp_iocb, &phba->fabric_iocb_list,
				 list) {

		if (piocb->vport != vport)
			continue;

		list_move_tail(&piocb->list, &completions);
	}
	spin_unlock_irq(&phba->hbalock);

	
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);
}

void lpfc_fabric_abort_nport(struct lpfc_nodelist *ndlp)
{
	LIST_HEAD(completions);
	struct lpfc_hba  *phba = ndlp->phba;
	struct lpfc_iocbq *tmp_iocb, *piocb;
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_ELS_RING];

	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(piocb, tmp_iocb, &phba->fabric_iocb_list,
				 list) {
		if ((lpfc_check_sli_ndlp(phba, pring, piocb, ndlp))) {

			list_move_tail(&piocb->list, &completions);
		}
	}
	spin_unlock_irq(&phba->hbalock);

	
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);
}

void lpfc_fabric_abort_hba(struct lpfc_hba *phba)
{
	LIST_HEAD(completions);

	spin_lock_irq(&phba->hbalock);
	list_splice_init(&phba->fabric_iocb_list, &completions);
	spin_unlock_irq(&phba->hbalock);

	
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);
}

void
lpfc_sli4_vport_delete_els_xri_aborted(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_sglq *sglq_entry = NULL, *sglq_next = NULL;
	unsigned long iflag = 0;

	spin_lock_irqsave(&phba->hbalock, iflag);
	spin_lock(&phba->sli4_hba.abts_sgl_list_lock);
	list_for_each_entry_safe(sglq_entry, sglq_next,
			&phba->sli4_hba.lpfc_abts_els_sgl_list, list) {
		if (sglq_entry->ndlp && sglq_entry->ndlp->vport == vport)
			sglq_entry->ndlp = NULL;
	}
	spin_unlock(&phba->sli4_hba.abts_sgl_list_lock);
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return;
}

void
lpfc_sli4_els_xri_aborted(struct lpfc_hba *phba,
			  struct sli4_wcqe_xri_aborted *axri)
{
	uint16_t xri = bf_get(lpfc_wcqe_xa_xri, axri);
	uint16_t rxid = bf_get(lpfc_wcqe_xa_remote_xid, axri);
	uint16_t lxri = 0;

	struct lpfc_sglq *sglq_entry = NULL, *sglq_next = NULL;
	unsigned long iflag = 0;
	struct lpfc_nodelist *ndlp;
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_ELS_RING];

	spin_lock_irqsave(&phba->hbalock, iflag);
	spin_lock(&phba->sli4_hba.abts_sgl_list_lock);
	list_for_each_entry_safe(sglq_entry, sglq_next,
			&phba->sli4_hba.lpfc_abts_els_sgl_list, list) {
		if (sglq_entry->sli4_xritag == xri) {
			list_del(&sglq_entry->list);
			ndlp = sglq_entry->ndlp;
			sglq_entry->ndlp = NULL;
			list_add_tail(&sglq_entry->list,
				&phba->sli4_hba.lpfc_sgl_list);
			sglq_entry->state = SGL_FREED;
			spin_unlock(&phba->sli4_hba.abts_sgl_list_lock);
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			lpfc_set_rrq_active(phba, ndlp, xri, rxid, 1);

			
			if (pring->txq_cnt)
				lpfc_worker_wake_up(phba);
			return;
		}
	}
	spin_unlock(&phba->sli4_hba.abts_sgl_list_lock);
	lxri = lpfc_sli4_xri_inrange(phba, xri);
	if (lxri == NO_XRI) {
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		return;
	}
	sglq_entry = __lpfc_get_active_sglq(phba, lxri);
	if (!sglq_entry || (sglq_entry->sli4_xritag != xri)) {
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		return;
	}
	sglq_entry->state = SGL_XRI_ABORTED;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return;
}
