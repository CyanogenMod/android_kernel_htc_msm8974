/*
 * Copyright 2010 2011 Mark Nelson and Tseng-Hui (Frank) Lin, IBM Corporation
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/notifier.h>

#include <asm/machdep.h>
#include <asm/rtas.h>
#include <asm/irq.h>
#include <asm/io_event_irq.h>

#include "pseries.h"


ATOMIC_NOTIFIER_HEAD(pseries_ioei_notifier_list);
EXPORT_SYMBOL_GPL(pseries_ioei_notifier_list);

static int ioei_check_exception_token;

static char ioei_rtas_buf[RTAS_DATA_BUF_SIZE] __cacheline_aligned;

static struct pseries_io_event * ioei_find_event(struct rtas_error_log *elog)
{
	struct pseries_errorlog *sect;

	if (unlikely(elog->type != RTAS_TYPE_IO)) {
		printk_once(KERN_WARNING "io_event_irq: Unexpected event type %d",
			    elog->type);
		return NULL;
	}

	sect = get_pseries_errorlog(elog, PSERIES_ELOG_SECT_ID_IO_EVENT);
	if (unlikely(!sect)) {
		printk_once(KERN_WARNING "io_event_irq: RTAS extended event "
			    "log does not contain an IO Event section. "
			    "Could be a bug in system firmware!\n");
		return NULL;
	}
	return (struct pseries_io_event *) &sect->data;
}


static irqreturn_t ioei_interrupt(int irq, void *dev_id)
{
	struct pseries_io_event *event;
	int rtas_rc;

	for (;;) {
		rtas_rc = rtas_call(ioei_check_exception_token, 6, 1, NULL,
				    RTAS_VECTOR_EXTERNAL_INTERRUPT,
				    virq_to_hw(irq),
				    RTAS_IO_EVENTS, 1 ,
				    __pa(ioei_rtas_buf),
				    RTAS_DATA_BUF_SIZE);
		if (rtas_rc != 0)
			break;

		event = ioei_find_event((struct rtas_error_log *)ioei_rtas_buf);
		if (!event)
			continue;

		atomic_notifier_call_chain(&pseries_ioei_notifier_list,
					   0, event);
	}
	return IRQ_HANDLED;
}

static int __init ioei_init(void)
{
	struct device_node *np;

	ioei_check_exception_token = rtas_token("check-exception");
	if (ioei_check_exception_token == RTAS_UNKNOWN_SERVICE)
		return -ENODEV;

	np = of_find_node_by_path("/event-sources/ibm,io-events");
	if (np) {
		request_event_sources_irqs(np, ioei_interrupt, "IO_EVENT");
		pr_info("IBM I/O event interrupts enabled\n");
		of_node_put(np);
	} else {
		return -ENODEV;
	}
	return 0;
}
machine_subsys_initcall(pseries, ioei_init);

