/*
 *  linux/arch/m68k/atari/stmda.c
 *
 *  Copyright (C) 1994 Roman Hodek
 *
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */




#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/genhd.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/module.h>

#include <asm/atari_stdma.h>
#include <asm/atariints.h>
#include <asm/atarihw.h>
#include <asm/io.h>
#include <asm/irq.h>

static int stdma_locked;			
						
static irq_handler_t stdma_isr;
static void *stdma_isr_data;			
static DECLARE_WAIT_QUEUE_HEAD(stdma_wait);	





static irqreturn_t stdma_int (int irq, void *dummy);





void stdma_lock(irq_handler_t handler, void *data)
{
	unsigned long flags;

	local_irq_save(flags);		

	wait_event(stdma_wait, !stdma_locked);

	stdma_locked   = 1;
	stdma_isr      = handler;
	stdma_isr_data = data;
	local_irq_restore(flags);
}
EXPORT_SYMBOL(stdma_lock);



void stdma_release(void)
{
	unsigned long flags;

	local_irq_save(flags);

	stdma_locked   = 0;
	stdma_isr      = NULL;
	stdma_isr_data = NULL;
	wake_up(&stdma_wait);

	local_irq_restore(flags);
}
EXPORT_SYMBOL(stdma_release);



int stdma_others_waiting(void)
{
	return waitqueue_active(&stdma_wait);
}
EXPORT_SYMBOL(stdma_others_waiting);



int stdma_islocked(void)
{
	return stdma_locked;
}
EXPORT_SYMBOL(stdma_islocked);



void __init stdma_init(void)
{
	stdma_isr = NULL;
	if (request_irq(IRQ_MFP_FDC, stdma_int, IRQ_TYPE_SLOW | IRQF_SHARED,
			"ST-DMA floppy,ACSI,IDE,Falcon-SCSI", stdma_int))
		pr_err("Couldn't register ST-DMA interrupt\n");
}



static irqreturn_t stdma_int(int irq, void *dummy)
{
  if (stdma_isr)
      (*stdma_isr)(irq, stdma_isr_data);
  return IRQ_HANDLED;
}
