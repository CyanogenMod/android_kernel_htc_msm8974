/*
 * Toshiba RBTX4927 specific interrupt handlers
 *
 * Author: MontaVista Software, Inc.
 *         source@mvista.com
 *
 * Copyright 2001-2002 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <asm/io.h>
#include <asm/mipsregs.h>
#include <asm/txx9/generic.h>
#include <asm/txx9/rbtx4927.h>

static int toshiba_rbtx4927_irq_nested(int sw_irq)
{
	u8 level3;

	level3 = readb(rbtx4927_imstat_addr) & 0x1f;
	if (unlikely(!level3))
		return -1;
	return RBTX4927_IRQ_IOC + __fls8(level3);
}

static void toshiba_rbtx4927_irq_ioc_enable(struct irq_data *d)
{
	unsigned char v;

	v = readb(rbtx4927_imask_addr);
	v |= (1 << (d->irq - RBTX4927_IRQ_IOC));
	writeb(v, rbtx4927_imask_addr);
}

static void toshiba_rbtx4927_irq_ioc_disable(struct irq_data *d)
{
	unsigned char v;

	v = readb(rbtx4927_imask_addr);
	v &= ~(1 << (d->irq - RBTX4927_IRQ_IOC));
	writeb(v, rbtx4927_imask_addr);
	mmiowb();
}

#define TOSHIBA_RBTX4927_IOC_NAME "RBTX4927-IOC"
static struct irq_chip toshiba_rbtx4927_irq_ioc_type = {
	.name = TOSHIBA_RBTX4927_IOC_NAME,
	.irq_mask = toshiba_rbtx4927_irq_ioc_disable,
	.irq_unmask = toshiba_rbtx4927_irq_ioc_enable,
};

static void __init toshiba_rbtx4927_irq_ioc_init(void)
{
	int i;

	
	writeb(0, rbtx4927_imask_addr);
	
	writeb(0, rbtx4927_softint_addr);

	for (i = RBTX4927_IRQ_IOC;
	     i < RBTX4927_IRQ_IOC + RBTX4927_NR_IRQ_IOC; i++)
		irq_set_chip_and_handler(i, &toshiba_rbtx4927_irq_ioc_type,
					 handle_level_irq);
	irq_set_chained_handler(RBTX4927_IRQ_IOCINT, handle_simple_irq);
}

static int rbtx4927_irq_dispatch(int pending)
{
	int irq;

	if (pending & STATUSF_IP7)			
		irq = MIPS_CPU_IRQ_BASE + 7;
	else if (pending & STATUSF_IP2) {		
		irq = txx9_irq();
		if (irq == RBTX4927_IRQ_IOCINT)
			irq = toshiba_rbtx4927_irq_nested(irq);
	} else if (pending & STATUSF_IP0)		
		irq = MIPS_CPU_IRQ_BASE + 0;
	else if (pending & STATUSF_IP1)			
		irq = MIPS_CPU_IRQ_BASE + 1;
	else
		irq = -1;
	return irq;
}

void __init rbtx4927_irq_setup(void)
{
	txx9_irq_dispatch = rbtx4927_irq_dispatch;
	tx4927_irq_init();
	toshiba_rbtx4927_irq_ioc_init();
	
	irq_set_irq_type(RBTX4927_RTL_8019_IRQ, IRQF_TRIGGER_HIGH);
}
