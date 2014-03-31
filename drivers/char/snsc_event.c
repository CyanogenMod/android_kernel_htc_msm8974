/*
 * SN Platform system controller communication support
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004-2006 Silicon Graphics, Inc. All rights reserved.
 */


#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <asm/byteorder.h>
#include <asm/sn/sn_sal.h>
#include <asm/unaligned.h>
#include "snsc.h"

static struct subch_data_s *event_sd;

void scdrv_event(unsigned long);
DECLARE_TASKLET(sn_sysctl_event, scdrv_event, 0);

static irqreturn_t
scdrv_event_interrupt(int irq, void *subch_data)
{
	struct subch_data_s *sd = subch_data;
	unsigned long flags;
	int status;

	spin_lock_irqsave(&sd->sd_rlock, flags);
	status = ia64_sn_irtr_intr(sd->sd_nasid, sd->sd_subch);

	if ((status > 0) && (status & SAL_IROUTER_INTR_RECV)) {
		tasklet_schedule(&sn_sysctl_event);
	}
	spin_unlock_irqrestore(&sd->sd_rlock, flags);
	return IRQ_HANDLED;
}


static int
scdrv_parse_event(char *event, int *src, int *code, int *esp_code, char *desc)
{
	char *desc_end;

	
	*src = get_unaligned_be32(event);
	event += 4; 			

	
	*code = get_unaligned_be32(event);
	event += 4;			

	
	if (*event++ != 2) {
		
		return -1;
	}

	
	if (*event++ != IR_ARG_INT) {
		
		return -1;
	}
	*esp_code = get_unaligned_be32(event);
	event += 4;

	
	if (*event++ != IR_ARG_ASCII) {
		
		return -1;
	}
	event[CHUNKSIZE-1] = '\0';	
	event += 2; 			
	desc_end = desc + sprintf(desc, "%s", event);

	
	for (desc_end--;
	     (desc_end != desc) && ((*desc_end == 0xd) || (*desc_end == 0xa));
	     desc_end--) {
		*desc_end = '\0';
	}

	return 0;
}


static char *
scdrv_event_severity(int code)
{
	int ev_class = (code & EV_CLASS_MASK);
	int ev_severity = (code & EV_SEVERITY_MASK);
	char *pk_severity = KERN_NOTICE;

	switch (ev_class) {
	case EV_CLASS_POWER:
		switch (ev_severity) {
		case EV_SEVERITY_POWER_LOW_WARNING:
		case EV_SEVERITY_POWER_HIGH_WARNING:
			pk_severity = KERN_WARNING;
			break;
		case EV_SEVERITY_POWER_HIGH_FAULT:
		case EV_SEVERITY_POWER_LOW_FAULT:
			pk_severity = KERN_ALERT;
			break;
		}
		break;
	case EV_CLASS_FAN:
		switch (ev_severity) {
		case EV_SEVERITY_FAN_WARNING:
			pk_severity = KERN_WARNING;
			break;
		case EV_SEVERITY_FAN_FAULT:
			pk_severity = KERN_CRIT;
			break;
		}
		break;
	case EV_CLASS_TEMP:
		switch (ev_severity) {
		case EV_SEVERITY_TEMP_ADVISORY:
			pk_severity = KERN_WARNING;
			break;
		case EV_SEVERITY_TEMP_CRITICAL:
			pk_severity = KERN_CRIT;
			break;
		case EV_SEVERITY_TEMP_FAULT:
			pk_severity = KERN_ALERT;
			break;
		}
		break;
	case EV_CLASS_ENV:
		pk_severity = KERN_ALERT;
		break;
	case EV_CLASS_TEST_FAULT:
		pk_severity = KERN_ALERT;
		break;
	case EV_CLASS_TEST_WARNING:
		pk_severity = KERN_WARNING;
		break;
	case EV_CLASS_PWRD_NOTIFY:
		pk_severity = KERN_ALERT;
		break;
	}

	return pk_severity;
}


static void
scdrv_dispatch_event(char *event, int len)
{
	static int snsc_shutting_down = 0;
	int code, esp_code, src, class;
	char desc[CHUNKSIZE];
	char *severity;

	if (scdrv_parse_event(event, &src, &code, &esp_code, desc) < 0) {
		
		return;
	}

	
	severity = scdrv_event_severity(code);

	class = (code & EV_CLASS_MASK);

	if (class == EV_CLASS_PWRD_NOTIFY || code == ENV_PWRDN_PEND) {
		if (snsc_shutting_down)
			return;

		snsc_shutting_down = 1;

		
		if (class == EV_CLASS_PWRD_NOTIFY)
			printk(KERN_NOTICE "Power off indication received."
			       " Sending SIGPWR to init...\n");
		else if (code == ENV_PWRDN_PEND)
			printk(KERN_CRIT "WARNING: Shutting down the system"
			       " due to a critical environmental condition."
			       " Sending SIGPWR to init...\n");

		
		kill_cad_pid(SIGPWR, 0);
	} else {
		
		printk("%s|$(0x%x)%s\n", severity, esp_code, desc);
	}
}


void
scdrv_event(unsigned long dummy)
{
	int status;
	int len;
	unsigned long flags;
	struct subch_data_s *sd = event_sd;

	
	len = CHUNKSIZE;
	spin_lock_irqsave(&sd->sd_rlock, flags);
	status = ia64_sn_irtr_recv(sd->sd_nasid, sd->sd_subch,
				   sd->sd_rb, &len);

	while (!(status < 0)) {
		spin_unlock_irqrestore(&sd->sd_rlock, flags);
		scdrv_dispatch_event(sd->sd_rb, len);
		len = CHUNKSIZE;
		spin_lock_irqsave(&sd->sd_rlock, flags);
		status = ia64_sn_irtr_recv(sd->sd_nasid, sd->sd_subch,
					   sd->sd_rb, &len);
	}
	spin_unlock_irqrestore(&sd->sd_rlock, flags);
}


void
scdrv_event_init(struct sysctl_data_s *scd)
{
	int rv;

	event_sd = kzalloc(sizeof (struct subch_data_s), GFP_KERNEL);
	if (event_sd == NULL) {
		printk(KERN_WARNING "%s: couldn't allocate subchannel info"
		       " for event monitoring\n", __func__);
		return;
	}

	
	event_sd->sd_nasid = scd->scd_nasid;
	spin_lock_init(&event_sd->sd_rlock);

	
	event_sd->sd_subch = ia64_sn_sysctl_event_init(scd->scd_nasid);

	if (event_sd->sd_subch < 0) {
		kfree(event_sd);
		printk(KERN_WARNING "%s: couldn't open event subchannel\n",
		       __func__);
		return;
	}

	
	rv = request_irq(SGI_UART_VECTOR, scdrv_event_interrupt,
			 IRQF_SHARED | IRQF_DISABLED,
			 "system controller events", event_sd);
	if (rv) {
		printk(KERN_WARNING "%s: irq request failed (%d)\n",
		       __func__, rv);
		ia64_sn_irtr_close(event_sd->sd_nasid, event_sd->sd_subch);
		kfree(event_sd);
		return;
	}
}
