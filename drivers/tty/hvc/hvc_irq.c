/*
 * Copyright IBM Corp. 2001,2008
 *
 * This file contains the IRQ specific code for hvc_console
 *
 */

#include <linux/interrupt.h>

#include "hvc_console.h"

static irqreturn_t hvc_handle_interrupt(int irq, void *dev_instance)
{
	
	if (hvc_poll(dev_instance))
		hvc_kick();
	return IRQ_HANDLED;
}

int notifier_add_irq(struct hvc_struct *hp, int irq)
{
	int rc;

	if (!irq) {
		hp->irq_requested = 0;
		return 0;
	}
	rc = request_irq(irq, hvc_handle_interrupt, 0,
			   "hvc_console", hp);
	if (!rc)
		hp->irq_requested = 1;
	return rc;
}

void notifier_del_irq(struct hvc_struct *hp, int irq)
{
	if (!hp->irq_requested)
		return;
	free_irq(irq, hp);
	hp->irq_requested = 0;
}

void notifier_hangup_irq(struct hvc_struct *hp, int irq)
{
	notifier_del_irq(hp, irq);
}
