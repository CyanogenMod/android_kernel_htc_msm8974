/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2004-2008 Emulex.  All rights reserved.           *
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
#include <linux/kthread.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

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
#include "lpfc_version.h"
#include "lpfc_vport.h"

inline void lpfc_vport_set_state(struct lpfc_vport *vport,
				 enum fc_vport_state new_state)
{
	struct fc_vport *fc_vport = vport->fc_vport;

	if (fc_vport) {
		
		if (new_state != FC_VPORT_INITIALIZING)
			fc_vport->vport_last_state = fc_vport->vport_state;
		fc_vport->vport_state = new_state;
	}

	
	switch (new_state) {
	case FC_VPORT_NO_FABRIC_SUPP:
	case FC_VPORT_NO_FABRIC_RSCS:
	case FC_VPORT_FABRIC_LOGOUT:
	case FC_VPORT_FABRIC_REJ_WWN:
	case FC_VPORT_FAILED:
		vport->port_state = LPFC_VPORT_FAILED;
		break;
	case FC_VPORT_LINKDOWN:
		vport->port_state = LPFC_VPORT_UNKNOWN;
		break;
	default:
		
		break;
	}
}

static int
lpfc_alloc_vpi(struct lpfc_hba *phba)
{
	unsigned long vpi;

	spin_lock_irq(&phba->hbalock);
	
	vpi = find_next_zero_bit(phba->vpi_bmask, (phba->max_vpi + 1), 1);
	if (vpi > phba->max_vpi)
		vpi = 0;
	else
		set_bit(vpi, phba->vpi_bmask);
	if (phba->sli_rev == LPFC_SLI_REV4)
		phba->sli4_hba.max_cfg_param.vpi_used++;
	spin_unlock_irq(&phba->hbalock);
	return vpi;
}

static void
lpfc_free_vpi(struct lpfc_hba *phba, int vpi)
{
	if (vpi == 0)
		return;
	spin_lock_irq(&phba->hbalock);
	clear_bit(vpi, phba->vpi_bmask);
	if (phba->sli_rev == LPFC_SLI_REV4)
		phba->sli4_hba.max_cfg_param.vpi_used--;
	spin_unlock_irq(&phba->hbalock);
}

static int
lpfc_vport_sparm(struct lpfc_hba *phba, struct lpfc_vport *vport)
{
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *mb;
	struct lpfc_dmabuf *mp;
	int  rc;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		return -ENOMEM;
	}
	mb = &pmb->u.mb;

	rc = lpfc_read_sparam(phba, pmb, vport->vpi);
	if (rc) {
		mempool_free(pmb, phba->mbox_mem_pool);
		return -ENOMEM;
	}

	mp = (struct lpfc_dmabuf *) pmb->context1;
	pmb->context1 = NULL;

	pmb->vport = vport;
	rc = lpfc_sli_issue_mbox_wait(phba, pmb, phba->fc_ratov * 2);
	if (rc != MBX_SUCCESS) {
		if (signal_pending(current)) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT | LOG_VPORT,
					 "1830 Signal aborted mbxCmd x%x\n",
					 mb->mbxCommand);
			lpfc_mbuf_free(phba, mp->virt, mp->phys);
			kfree(mp);
			if (rc != MBX_TIMEOUT)
				mempool_free(pmb, phba->mbox_mem_pool);
			return -EINTR;
		} else {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT | LOG_VPORT,
					 "1818 VPort failed init, mbxCmd x%x "
					 "READ_SPARM mbxStatus x%x, rc = x%x\n",
					 mb->mbxCommand, mb->mbxStatus, rc);
			lpfc_mbuf_free(phba, mp->virt, mp->phys);
			kfree(mp);
			if (rc != MBX_TIMEOUT)
				mempool_free(pmb, phba->mbox_mem_pool);
			return -EIO;
		}
	}

	memcpy(&vport->fc_sparam, mp->virt, sizeof (struct serv_parm));
	memcpy(&vport->fc_nodename, &vport->fc_sparam.nodeName,
	       sizeof (struct lpfc_name));
	memcpy(&vport->fc_portname, &vport->fc_sparam.portName,
	       sizeof (struct lpfc_name));

	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	mempool_free(pmb, phba->mbox_mem_pool);

	return 0;
}

static int
lpfc_valid_wwn_format(struct lpfc_hba *phba, struct lpfc_name *wwn,
		      const char *name_type)
{
	if (!((wwn->u.wwn[0] >> 4) == 1 &&
	      ((wwn->u.wwn[0] & 0xf) != 0 || (wwn->u.wwn[1] & 0xf) != 0)))
		return 1;

	lpfc_printf_log(phba, KERN_ERR, LOG_VPORT,
			"1822 Invalid %s: %02x:%02x:%02x:%02x:"
			"%02x:%02x:%02x:%02x\n",
			name_type,
			wwn->u.wwn[0], wwn->u.wwn[1],
			wwn->u.wwn[2], wwn->u.wwn[3],
			wwn->u.wwn[4], wwn->u.wwn[5],
			wwn->u.wwn[6], wwn->u.wwn[7]);
	return 0;
}

static int
lpfc_unique_wwpn(struct lpfc_hba *phba, struct lpfc_vport *new_vport)
{
	struct lpfc_vport *vport;
	unsigned long flags;

	spin_lock_irqsave(&phba->hbalock, flags);
	list_for_each_entry(vport, &phba->port_list, listentry) {
		if (vport == new_vport)
			continue;
		
		if (memcmp(&vport->fc_sparam.portName,
			   &new_vport->fc_sparam.portName,
			   sizeof(struct lpfc_name)) == 0) {
			spin_unlock_irqrestore(&phba->hbalock, flags);
			return 0;
		}
	}
	spin_unlock_irqrestore(&phba->hbalock, flags);
	return 1;
}

static void lpfc_discovery_wait(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	uint32_t wait_flags = 0;
	unsigned long wait_time_max;
	unsigned long start_time;

	wait_flags = FC_RSCN_MODE | FC_RSCN_DISCOVERY | FC_NLP_MORE |
		     FC_RSCN_DEFERRED | FC_NDISC_ACTIVE | FC_DISC_TMO;

	wait_time_max = msecs_to_jiffies(((phba->fc_ratov * 3) + 3) * 1000);
	wait_time_max += jiffies;
	start_time = jiffies;
	while (time_before(jiffies, wait_time_max)) {
		if ((vport->num_disc_nodes > 0)    ||
		    (vport->fc_flag & wait_flags)  ||
		    ((vport->port_state > LPFC_VPORT_FAILED) &&
		     (vport->port_state < LPFC_VPORT_READY))) {
			lpfc_printf_vlog(vport, KERN_INFO, LOG_VPORT,
					"1833 Vport discovery quiesce Wait:"
					" state x%x fc_flags x%x"
					" num_nodes x%x, waiting 1000 msecs"
					" total wait msecs x%x\n",
					vport->port_state, vport->fc_flag,
					vport->num_disc_nodes,
					jiffies_to_msecs(jiffies - start_time));
			msleep(1000);
		} else {
			
			lpfc_printf_vlog(vport, KERN_INFO, LOG_VPORT,
					 "1834 Vport discovery quiesced:"
					 " state x%x fc_flags x%x"
					 " wait msecs x%x\n",
					 vport->port_state, vport->fc_flag,
					 jiffies_to_msecs(jiffies
						- start_time));
			break;
		}
	}

	if (time_after(jiffies, wait_time_max))
		lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
				"1835 Vport discovery quiesce failed:"
				" state x%x fc_flags x%x wait msecs x%x\n",
				vport->port_state, vport->fc_flag,
				jiffies_to_msecs(jiffies - start_time));
}

int
lpfc_vport_create(struct fc_vport *fc_vport, bool disable)
{
	struct lpfc_nodelist *ndlp;
	struct Scsi_Host *shost = fc_vport->shost;
	struct lpfc_vport *pport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = pport->phba;
	struct lpfc_vport *vport = NULL;
	int instance;
	int vpi;
	int rc = VPORT_ERROR;
	int status;

	if ((phba->sli_rev < 3) || !(phba->cfg_enable_npiv)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_VPORT,
				"1808 Create VPORT failed: "
				"NPIV is not enabled: SLImode:%d\n",
				phba->sli_rev);
		rc = VPORT_INVAL;
		goto error_out;
	}

	vpi = lpfc_alloc_vpi(phba);
	if (vpi == 0) {
		lpfc_printf_log(phba, KERN_ERR, LOG_VPORT,
				"1809 Create VPORT failed: "
				"Max VPORTs (%d) exceeded\n",
				phba->max_vpi);
		rc = VPORT_NORESOURCES;
		goto error_out;
	}

	
	if ((instance = lpfc_get_instance()) < 0) {
		lpfc_printf_log(phba, KERN_ERR, LOG_VPORT,
				"1810 Create VPORT failed: Cannot get "
				"instance number\n");
		lpfc_free_vpi(phba, vpi);
		rc = VPORT_NORESOURCES;
		goto error_out;
	}

	vport = lpfc_create_port(phba, instance, &fc_vport->dev);
	if (!vport) {
		lpfc_printf_log(phba, KERN_ERR, LOG_VPORT,
				"1811 Create VPORT failed: vpi x%x\n", vpi);
		lpfc_free_vpi(phba, vpi);
		rc = VPORT_NORESOURCES;
		goto error_out;
	}

	vport->vpi = vpi;
	lpfc_debugfs_initialize(vport);

	if ((status = lpfc_vport_sparm(phba, vport))) {
		if (status == -EINTR) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
					 "1831 Create VPORT Interrupted.\n");
			rc = VPORT_ERROR;
		} else {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
					 "1813 Create VPORT failed. "
					 "Cannot get sparam\n");
			rc = VPORT_NORESOURCES;
		}
		lpfc_free_vpi(phba, vpi);
		destroy_port(vport);
		goto error_out;
	}

	u64_to_wwn(fc_vport->node_name, vport->fc_nodename.u.wwn);
	u64_to_wwn(fc_vport->port_name, vport->fc_portname.u.wwn);

	memcpy(&vport->fc_sparam.portName, vport->fc_portname.u.wwn, 8);
	memcpy(&vport->fc_sparam.nodeName, vport->fc_nodename.u.wwn, 8);

	if (!lpfc_valid_wwn_format(phba, &vport->fc_sparam.nodeName, "WWNN") ||
	    !lpfc_valid_wwn_format(phba, &vport->fc_sparam.portName, "WWPN")) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
				 "1821 Create VPORT failed. "
				 "Invalid WWN format\n");
		lpfc_free_vpi(phba, vpi);
		destroy_port(vport);
		rc = VPORT_INVAL;
		goto error_out;
	}

	if (!lpfc_unique_wwpn(phba, vport)) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
				 "1823 Create VPORT failed. "
				 "Duplicate WWN on HBA\n");
		lpfc_free_vpi(phba, vpi);
		destroy_port(vport);
		rc = VPORT_INVAL;
		goto error_out;
	}

	
	lpfc_alloc_sysfs_attr(vport);

	*(struct lpfc_vport **)fc_vport->dd_data = vport;
	vport->fc_vport = fc_vport;

	if ((phba->sli_rev == LPFC_SLI_REV4) &&
	    (pport->fc_flag & FC_VFI_REGISTERED)) {
		rc = lpfc_sli4_init_vpi(vport);
		if (rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_VPORT,
					"1838 Failed to INIT_VPI on vpi %d "
					"status %d\n", vpi, rc);
			rc = VPORT_NORESOURCES;
			lpfc_free_vpi(phba, vpi);
			goto error_out;
		}
	} else if (phba->sli_rev == LPFC_SLI_REV4) {
		vport->fc_flag |= FC_VPORT_NEEDS_INIT_VPI;
		lpfc_vport_set_state(vport, FC_VPORT_LINKDOWN);
		rc = VPORT_OK;
		goto out;
	}

	if ((phba->link_state < LPFC_LINK_UP) ||
	    (pport->port_state < LPFC_FABRIC_CFG_LINK) ||
	    (phba->fc_topology == LPFC_TOPOLOGY_LOOP)) {
		lpfc_vport_set_state(vport, FC_VPORT_LINKDOWN);
		rc = VPORT_OK;
		goto out;
	}

	if (disable) {
		lpfc_vport_set_state(vport, FC_VPORT_DISABLED);
		rc = VPORT_OK;
		goto out;
	}

	ndlp = lpfc_findnode_did(phba->pport, Fabric_DID);
	if (ndlp && NLP_CHK_NODE_ACT(ndlp) &&
	    ndlp->nlp_state == NLP_STE_UNMAPPED_NODE) {
		if (phba->link_flag & LS_NPIV_FAB_SUPPORTED) {
			lpfc_set_disctmo(vport);
			lpfc_initial_fdisc(vport);
		} else {
			lpfc_vport_set_state(vport, FC_VPORT_NO_FABRIC_SUPP);
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
					 "0262 No NPIV Fabric support\n");
		}
	} else {
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	}
	rc = VPORT_OK;

out:
	lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
			"1825 Vport Created.\n");
	lpfc_host_attrib_init(lpfc_shost_from_vport(vport));
error_out:
	return rc;
}

static int
disable_vport(struct fc_vport *fc_vport)
{
	struct lpfc_vport *vport = *(struct lpfc_vport **)fc_vport->dd_data;
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_nodelist *ndlp = NULL, *next_ndlp = NULL;
	long timeout;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	ndlp = lpfc_findnode_did(vport, Fabric_DID);
	if (ndlp && NLP_CHK_NODE_ACT(ndlp)
	    && phba->link_state >= LPFC_LINK_UP) {
		vport->unreg_vpi_cmpl = VPORT_INVAL;
		timeout = msecs_to_jiffies(phba->fc_ratov * 2000);
		if (!lpfc_issue_els_npiv_logo(vport, ndlp))
			while (vport->unreg_vpi_cmpl == VPORT_INVAL && timeout)
				timeout = schedule_timeout(timeout);
	}

	lpfc_sli_host_down(vport);

	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp))
			continue;
		if (ndlp->nlp_state == NLP_STE_UNUSED_NODE)
			continue;
		lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RECOVERY);
	}
	lpfc_cleanup_rpis(vport, 1);

	lpfc_stop_vport_timers(vport);
	lpfc_unreg_all_rpis(vport);
	lpfc_unreg_default_rpis(vport);
	lpfc_mbx_unreg_vpi(vport);
	spin_lock_irq(shost->host_lock);
	vport->fc_flag |= FC_VPORT_NEEDS_INIT_VPI;
	spin_unlock_irq(shost->host_lock);

	lpfc_vport_set_state(vport, FC_VPORT_DISABLED);
	lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
			 "1826 Vport Disabled.\n");
	return VPORT_OK;
}

static int
enable_vport(struct fc_vport *fc_vport)
{
	struct lpfc_vport *vport = *(struct lpfc_vport **)fc_vport->dd_data;
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_nodelist *ndlp = NULL;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	if ((phba->link_state < LPFC_LINK_UP) ||
	    (phba->fc_topology == LPFC_TOPOLOGY_LOOP)) {
		lpfc_vport_set_state(vport, FC_VPORT_LINKDOWN);
		return VPORT_OK;
	}

	spin_lock_irq(shost->host_lock);
	vport->load_flag |= FC_LOADING;
	vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
	spin_unlock_irq(shost->host_lock);

	ndlp = lpfc_findnode_did(phba->pport, Fabric_DID);
	if (ndlp && NLP_CHK_NODE_ACT(ndlp)
	    && ndlp->nlp_state == NLP_STE_UNMAPPED_NODE) {
		if (phba->link_flag & LS_NPIV_FAB_SUPPORTED) {
			lpfc_set_disctmo(vport);
			lpfc_initial_fdisc(vport);
		} else {
			lpfc_vport_set_state(vport, FC_VPORT_NO_FABRIC_SUPP);
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
					 "0264 No NPIV Fabric support\n");
		}
	} else {
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	}
	lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
			 "1827 Vport Enabled.\n");
	return VPORT_OK;
}

int
lpfc_vport_disable(struct fc_vport *fc_vport, bool disable)
{
	if (disable)
		return disable_vport(fc_vport);
	else
		return enable_vport(fc_vport);
}


int
lpfc_vport_delete(struct fc_vport *fc_vport)
{
	struct lpfc_nodelist *ndlp = NULL;
	struct Scsi_Host *shost = (struct Scsi_Host *) fc_vport->shost;
	struct lpfc_vport *vport = *(struct lpfc_vport **)fc_vport->dd_data;
	struct lpfc_hba   *phba = vport->phba;
	long timeout;

	if (vport->port_type == LPFC_PHYSICAL_PORT) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
				 "1812 vport_delete failed: Cannot delete "
				 "physical host\n");
		return VPORT_ERROR;
	}

	
	if ((vport->vport_flag & STATIC_VPORT) &&
		!(phba->pport->load_flag & FC_UNLOADING)) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
				 "1837 vport_delete failed: Cannot delete "
				 "static vport.\n");
		return VPORT_ERROR;
	}
	spin_lock_irq(&phba->hbalock);
	vport->load_flag |= FC_UNLOADING;
	spin_unlock_irq(&phba->hbalock);
	if (!(phba->pport->load_flag & FC_UNLOADING)) {
		int check_count = 0;
		while (check_count < ((phba->fc_ratov * 3) + 3) &&
		       vport->port_state > LPFC_VPORT_FAILED &&
		       vport->port_state < LPFC_VPORT_READY) {
			check_count++;
			msleep(1000);
		}
		if (vport->port_state > LPFC_VPORT_FAILED &&
		    vport->port_state < LPFC_VPORT_READY)
			return -EAGAIN;
	}
	if (!scsi_host_get(shost))
		return VPORT_INVAL;
	if (!scsi_host_get(shost)) {
		scsi_host_put(shost);
		return VPORT_INVAL;
	}
	lpfc_free_sysfs_attr(vport);

	lpfc_debugfs_terminate(vport);

	
	fc_remove_host(lpfc_shost_from_vport(vport));
	scsi_remove_host(lpfc_shost_from_vport(vport));

	ndlp = lpfc_findnode_did(phba->pport, Fabric_DID);

	if (phba->pport->load_flag & FC_UNLOADING) {
		if (ndlp && NLP_CHK_NODE_ACT(ndlp) &&
		    ndlp->nlp_state == NLP_STE_UNMAPPED_NODE &&
		    phba->link_state >= LPFC_LINK_UP) {
			
			ndlp = lpfc_findnode_did(vport, Fabric_DID);
			if (!ndlp)
				goto skip_logo;
			else if (!NLP_CHK_NODE_ACT(ndlp)) {
				ndlp = lpfc_enable_node(vport, ndlp,
							NLP_STE_UNUSED_NODE);
				if (!ndlp)
					goto skip_logo;
			}
			
			lpfc_dequeue_node(vport, ndlp);

			
			spin_lock_irq(&phba->ndlp_lock);
			NLP_SET_FREE_REQ(ndlp);
			spin_unlock_irq(&phba->ndlp_lock);
			
			lpfc_nlp_put(ndlp);
		}
		goto skip_logo;
	}

	
	if (ndlp && NLP_CHK_NODE_ACT(ndlp) &&
	    ndlp->nlp_state == NLP_STE_UNMAPPED_NODE &&
	    phba->link_state >= LPFC_LINK_UP &&
	    phba->fc_topology != LPFC_TOPOLOGY_LOOP) {
		if (vport->cfg_enable_da_id) {
			timeout = msecs_to_jiffies(phba->fc_ratov * 2000);
			if (!lpfc_ns_cmd(vport, SLI_CTNS_DA_ID, 0, 0))
				while (vport->ct_flags && timeout)
					timeout = schedule_timeout(timeout);
			else
				lpfc_printf_log(vport->phba, KERN_WARNING,
						LOG_VPORT,
						"1829 CT command failed to "
						"delete objects on fabric\n");
		}
		
		ndlp = lpfc_findnode_did(vport, Fabric_DID);
		if (!ndlp) {
			
			ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
			if (!ndlp)
				goto skip_logo;
			lpfc_nlp_init(vport, ndlp, Fabric_DID);
			
			NLP_SET_FREE_REQ(ndlp);
		} else {
			if (!NLP_CHK_NODE_ACT(ndlp)) {
				ndlp = lpfc_enable_node(vport, ndlp,
						NLP_STE_UNUSED_NODE);
				if (!ndlp)
					goto skip_logo;
			}

			
			lpfc_dequeue_node(vport, ndlp);
			spin_lock_irq(&phba->ndlp_lock);
			if (!NLP_CHK_FREE_REQ(ndlp))
				
				NLP_SET_FREE_REQ(ndlp);
			else {
				
				spin_unlock_irq(&phba->ndlp_lock);
				goto skip_logo;
			}
			spin_unlock_irq(&phba->ndlp_lock);
		}

		if (!(vport->vpi_state & LPFC_VPI_REGISTERED)) {
			lpfc_nlp_put(ndlp);
			goto skip_logo;
		}

		vport->unreg_vpi_cmpl = VPORT_INVAL;
		timeout = msecs_to_jiffies(phba->fc_ratov * 2000);
		if (!lpfc_issue_els_npiv_logo(vport, ndlp))
			while (vport->unreg_vpi_cmpl == VPORT_INVAL && timeout)
				timeout = schedule_timeout(timeout);
	}

	if (!(phba->pport->load_flag & FC_UNLOADING))
		lpfc_discovery_wait(vport);

skip_logo:
	lpfc_cleanup(vport);
	lpfc_sli_host_down(vport);

	lpfc_stop_vport_timers(vport);

	if (!(phba->pport->load_flag & FC_UNLOADING)) {
		lpfc_unreg_all_rpis(vport);
		lpfc_unreg_default_rpis(vport);
		if (lpfc_mbx_unreg_vpi(vport))
			scsi_host_put(shost);
	} else
		scsi_host_put(shost);

	lpfc_free_vpi(phba, vport->vpi);
	vport->work_port_events = 0;
	spin_lock_irq(&phba->hbalock);
	list_del_init(&vport->listentry);
	spin_unlock_irq(&phba->hbalock);
	lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
			 "1828 Vport Deleted.\n");
	scsi_host_put(shost);
	return VPORT_OK;
}

struct lpfc_vport **
lpfc_create_vport_work_array(struct lpfc_hba *phba)
{
	struct lpfc_vport *port_iterator;
	struct lpfc_vport **vports;
	int index = 0;
	vports = kzalloc((phba->max_vports + 1) * sizeof(struct lpfc_vport *),
			 GFP_KERNEL);
	if (vports == NULL)
		return NULL;
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry(port_iterator, &phba->port_list, listentry) {
		if (port_iterator->load_flag & FC_UNLOADING)
			continue;
		if (!scsi_host_get(lpfc_shost_from_vport(port_iterator))) {
			lpfc_printf_vlog(port_iterator, KERN_ERR, LOG_VPORT,
					 "1801 Create vport work array FAILED: "
					 "cannot do scsi_host_get\n");
			continue;
		}
		vports[index++] = port_iterator;
	}
	spin_unlock_irq(&phba->hbalock);
	return vports;
}

void
lpfc_destroy_vport_work_array(struct lpfc_hba *phba, struct lpfc_vport **vports)
{
	int i;
	if (vports == NULL)
		return;
	for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++)
		scsi_host_put(lpfc_shost_from_vport(vports[i]));
	kfree(vports);
}


void
lpfc_vport_reset_stat_data(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp = NULL, *next_ndlp = NULL;

	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp))
			continue;
		if (ndlp->lat_data)
			memset(ndlp->lat_data, 0, LPFC_MAX_BUCKET_COUNT *
				sizeof(struct lpfc_scsicmd_bkt));
	}
}


void
lpfc_alloc_bucket(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp = NULL, *next_ndlp = NULL;

	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp))
			continue;

		kfree(ndlp->lat_data);
		ndlp->lat_data = NULL;

		if (ndlp->nlp_state == NLP_STE_MAPPED_NODE) {
			ndlp->lat_data = kcalloc(LPFC_MAX_BUCKET_COUNT,
					 sizeof(struct lpfc_scsicmd_bkt),
					 GFP_ATOMIC);

			if (!ndlp->lat_data)
				lpfc_printf_vlog(vport, KERN_ERR, LOG_NODE,
					"0287 lpfc_alloc_bucket failed to "
					"allocate statistical data buffer DID "
					"0x%x\n", ndlp->nlp_DID);
		}
	}
}

void
lpfc_free_bucket(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp = NULL, *next_ndlp = NULL;

	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp))
			continue;

		kfree(ndlp->lat_data);
		ndlp->lat_data = NULL;
	}
}
