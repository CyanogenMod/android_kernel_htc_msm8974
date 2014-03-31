/*
 * drivers/s390/cio/device_status.c
 *
 *    Copyright (C) 2002 IBM Deutschland Entwicklung GmbH,
 *			 IBM Corporation
 *    Author(s): Cornelia Huck (cornelia.huck@de.ibm.com)
 *		 Martin Schwidefsky (schwidefsky@de.ibm.com)
 *
 * Status accumulation and basic sense functions.
 */

#include <linux/module.h>
#include <linux/init.h>

#include <asm/ccwdev.h>
#include <asm/cio.h>

#include "cio.h"
#include "cio_debug.h"
#include "css.h"
#include "device.h"
#include "ioasm.h"
#include "io_sch.h"

static void
ccw_device_msg_control_check(struct ccw_device *cdev, struct irb *irb)
{
	char dbf_text[15];

	if (!scsw_is_valid_cstat(&irb->scsw) ||
	    !(scsw_cstat(&irb->scsw) & (SCHN_STAT_CHN_DATA_CHK |
	      SCHN_STAT_CHN_CTRL_CHK | SCHN_STAT_INTF_CTRL_CHK)))
		return;
	CIO_MSG_EVENT(0, "Channel-Check or Interface-Control-Check "
		      "received"
		      " ... device %04x on subchannel 0.%x.%04x, dev_stat "
		      ": %02X sch_stat : %02X\n",
		      cdev->private->dev_id.devno, cdev->private->schid.ssid,
		      cdev->private->schid.sch_no,
		      scsw_dstat(&irb->scsw), scsw_cstat(&irb->scsw));
	sprintf(dbf_text, "chk%x", cdev->private->schid.sch_no);
	CIO_TRACE_EVENT(0, dbf_text);
	CIO_HEX_EVENT(0, irb, sizeof(struct irb));
}

static void
ccw_device_path_notoper(struct ccw_device *cdev)
{
	struct subchannel *sch;

	sch = to_subchannel(cdev->dev.parent);
	if (cio_update_schib(sch))
		goto doverify;

	CIO_MSG_EVENT(0, "%s(0.%x.%04x) - path(s) %02x are "
		      "not operational \n", __func__,
		      sch->schid.ssid, sch->schid.sch_no,
		      sch->schib.pmcw.pnom);

	sch->lpm &= ~sch->schib.pmcw.pnom;
doverify:
	cdev->private->flags.doverify = 1;
}

static void
ccw_device_accumulate_ecw(struct ccw_device *cdev, struct irb *irb)
{
	cdev->private->irb.scsw.cmd.ectl = 0;
	if ((irb->scsw.cmd.stctl & SCSW_STCTL_ALERT_STATUS) &&
	    !(irb->scsw.cmd.stctl & SCSW_STCTL_INTER_STATUS))
		cdev->private->irb.scsw.cmd.ectl = irb->scsw.cmd.ectl;
	
	if (!cdev->private->irb.scsw.cmd.ectl)
		return;
	
	memcpy (&cdev->private->irb.ecw, irb->ecw, sizeof (irb->ecw));
}

static int
ccw_device_accumulate_esw_valid(struct irb *irb)
{
	if (!irb->scsw.cmd.eswf &&
	    (irb->scsw.cmd.stctl == SCSW_STCTL_STATUS_PEND))
		return 0;
	if (irb->scsw.cmd.stctl ==
			(SCSW_STCTL_INTER_STATUS|SCSW_STCTL_STATUS_PEND) &&
	    !(irb->scsw.cmd.actl & SCSW_ACTL_SUSPENDED))
		return 0;
	return 1;
}

static void
ccw_device_accumulate_esw(struct ccw_device *cdev, struct irb *irb)
{
	struct irb *cdev_irb;
	struct sublog *cdev_sublog, *sublog;

	if (!ccw_device_accumulate_esw_valid(irb))
		return;

	cdev_irb = &cdev->private->irb;

	
	cdev_irb->esw.esw1.lpum = irb->esw.esw1.lpum;

	
	if (irb->scsw.cmd.eswf) {
		cdev_sublog = &cdev_irb->esw.esw0.sublog;
		sublog = &irb->esw.esw0.sublog;
		
		cdev_sublog->esf = sublog->esf;
		if (irb->scsw.cmd.cstat & (SCHN_STAT_CHN_DATA_CHK |
				       SCHN_STAT_CHN_CTRL_CHK |
				       SCHN_STAT_INTF_CTRL_CHK)) {
			
			cdev_sublog->arep = sublog->arep;
			
			cdev_sublog->fvf = sublog->fvf;
			
			cdev_sublog->sacc = sublog->sacc;
			
			cdev_sublog->termc = sublog->termc;
			
			cdev_sublog->seqc = sublog->seqc;
		}
		
		cdev_sublog->devsc = sublog->devsc;
		
		cdev_sublog->serr = sublog->serr;
		
		cdev_sublog->ioerr = sublog->ioerr;
		
		if (irb->scsw.cmd.cstat & SCHN_STAT_INTF_CTRL_CHK)
			cdev_irb->esw.esw0.erw.cpt = irb->esw.esw0.erw.cpt;
		
		cdev_irb->esw.esw0.erw.fsavf = irb->esw.esw0.erw.fsavf;
		if (cdev_irb->esw.esw0.erw.fsavf) {
			
			memcpy(cdev_irb->esw.esw0.faddr, irb->esw.esw0.faddr,
			       sizeof (irb->esw.esw0.faddr));
			
			cdev_irb->esw.esw0.erw.fsaf = irb->esw.esw0.erw.fsaf;
		}
		
		cdev_irb->esw.esw0.erw.scavf = irb->esw.esw0.erw.scavf;
		if (irb->esw.esw0.erw.scavf)
			
			cdev_irb->esw.esw0.saddr = irb->esw.esw0.saddr;
		
	}
	

	
	cdev_irb->esw.esw0.erw.auth = irb->esw.esw0.erw.auth;
	
	cdev_irb->esw.esw0.erw.pvrf = irb->esw.esw0.erw.pvrf;
	if (irb->esw.esw0.erw.pvrf)
		cdev->private->flags.doverify = 1;
	
	cdev_irb->esw.esw0.erw.cons = irb->esw.esw0.erw.cons;
	if (irb->esw.esw0.erw.cons)
		cdev_irb->esw.esw0.erw.scnt = irb->esw.esw0.erw.scnt;
}

void
ccw_device_accumulate_irb(struct ccw_device *cdev, struct irb *irb)
{
	struct irb *cdev_irb;

	if (!(scsw_stctl(&irb->scsw) & SCSW_STCTL_STATUS_PEND))
		return;

	
	ccw_device_msg_control_check(cdev, irb);

	
	if (scsw_is_valid_pno(&irb->scsw) && scsw_pno(&irb->scsw))
		ccw_device_path_notoper(cdev);
	
	if (scsw_is_tm(&irb->scsw)) {
		memcpy(&cdev->private->irb, irb, sizeof(struct irb));
		return;
	}
	if (!scsw_is_solicited(&irb->scsw))
		return;

	cdev_irb = &cdev->private->irb;

	if (irb->scsw.cmd.fctl & SCSW_FCTL_CLEAR_FUNC)
		memset(&cdev->private->irb, 0, sizeof(struct irb));

	
	if (irb->scsw.cmd.fctl & SCSW_FCTL_START_FUNC) {
		
		cdev_irb->scsw.cmd.key = irb->scsw.cmd.key;
		
		cdev_irb->scsw.cmd.sctl = irb->scsw.cmd.sctl;
		
		cdev_irb->scsw.cmd.cc |= irb->scsw.cmd.cc;
		
		cdev_irb->scsw.cmd.fmt = irb->scsw.cmd.fmt;
		
		cdev_irb->scsw.cmd.pfch = irb->scsw.cmd.pfch;
		
		cdev_irb->scsw.cmd.isic = irb->scsw.cmd.isic;
		
		cdev_irb->scsw.cmd.alcc = irb->scsw.cmd.alcc;
		
		cdev_irb->scsw.cmd.ssi = irb->scsw.cmd.ssi;
	}

	
	ccw_device_accumulate_ecw(cdev, irb);
	    
	
	cdev_irb->scsw.cmd.fctl |= irb->scsw.cmd.fctl;
	
	cdev_irb->scsw.cmd.actl = irb->scsw.cmd.actl;
	
	cdev_irb->scsw.cmd.stctl |= irb->scsw.cmd.stctl;
	if ((irb->scsw.cmd.stctl & SCSW_STCTL_PRIM_STATUS) ||
	    ((irb->scsw.cmd.stctl ==
	      (SCSW_STCTL_INTER_STATUS|SCSW_STCTL_STATUS_PEND)) &&
	     (irb->scsw.cmd.actl & SCSW_ACTL_DEVACT) &&
	     (irb->scsw.cmd.actl & SCSW_ACTL_SCHACT)) ||
	    (irb->scsw.cmd.actl & SCSW_ACTL_SUSPENDED))
		cdev_irb->scsw.cmd.cpa = irb->scsw.cmd.cpa;
	
	cdev_irb->scsw.cmd.dstat &= ~DEV_STAT_BUSY;
	
	if (irb->scsw.cmd.stctl &
	    (SCSW_STCTL_PRIM_STATUS | SCSW_STCTL_SEC_STATUS
	     | SCSW_STCTL_INTER_STATUS | SCSW_STCTL_ALERT_STATUS))
		cdev_irb->scsw.cmd.dstat |= irb->scsw.cmd.dstat;
	
	cdev_irb->scsw.cmd.cstat |= irb->scsw.cmd.cstat;
	
	if ((irb->scsw.cmd.stctl & SCSW_STCTL_PRIM_STATUS) &&
	    (irb->scsw.cmd.cstat & ~(SCHN_STAT_PCI | SCHN_STAT_INCORR_LEN))
	     == 0)
		cdev_irb->scsw.cmd.count = irb->scsw.cmd.count;

	
	ccw_device_accumulate_esw(cdev, irb);

	if ((cdev_irb->scsw.cmd.dstat & DEV_STAT_UNIT_CHECK) &&
	    !(cdev_irb->esw.esw0.erw.cons))
		cdev->private->flags.dosense = 1;
}

int
ccw_device_do_sense(struct ccw_device *cdev, struct irb *irb)
{
	struct subchannel *sch;
	struct ccw1 *sense_ccw;
	int rc;

	sch = to_subchannel(cdev->dev.parent);

	
	if (scsw_actl(&irb->scsw) & (SCSW_ACTL_DEVACT | SCSW_ACTL_SCHACT))
		return -EBUSY;

	sense_ccw = &to_io_private(sch)->sense_ccw;
	sense_ccw->cmd_code = CCW_CMD_BASIC_SENSE;
	sense_ccw->cda = (__u32) __pa(cdev->private->irb.ecw);
	sense_ccw->count = SENSE_MAX_COUNT;
	sense_ccw->flags = CCW_FLAG_SLI;

	rc = cio_start(sch, sense_ccw, 0xff);
	if (rc == -ENODEV || rc == -EACCES)
		dev_fsm_event(cdev, DEV_EVENT_VERIFY);
	return rc;
}

void
ccw_device_accumulate_basic_sense(struct ccw_device *cdev, struct irb *irb)
{
	if (!(scsw_stctl(&irb->scsw) & SCSW_STCTL_STATUS_PEND))
		return;

	
	ccw_device_msg_control_check(cdev, irb);

	
	if (scsw_is_valid_pno(&irb->scsw) && scsw_pno(&irb->scsw))
		ccw_device_path_notoper(cdev);

	if (!(irb->scsw.cmd.dstat & DEV_STAT_UNIT_CHECK) &&
	    (irb->scsw.cmd.dstat & DEV_STAT_CHN_END)) {
		cdev->private->irb.esw.esw0.erw.cons = 1;
		cdev->private->flags.dosense = 0;
	}
	
	if (ccw_device_accumulate_esw_valid(irb) &&
	    irb->esw.esw0.erw.pvrf)
		cdev->private->flags.doverify = 1;
}

int
ccw_device_accumulate_and_sense(struct ccw_device *cdev, struct irb *irb)
{
	ccw_device_accumulate_irb(cdev, irb);
	if ((irb->scsw.cmd.actl  & (SCSW_ACTL_DEVACT | SCSW_ACTL_SCHACT)) != 0)
		return -EBUSY;
	
	if (cdev->private->flags.dosense &&
	    !(irb->scsw.cmd.dstat & DEV_STAT_UNIT_CHECK)) {
		cdev->private->irb.esw.esw0.erw.cons = 1;
		cdev->private->flags.dosense = 0;
		return 0;
	}
	if (cdev->private->flags.dosense) {
		ccw_device_do_sense(cdev, irb);
		return -EBUSY;
	}
	return 0;
}

