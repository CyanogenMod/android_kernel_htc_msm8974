/*
 *  linux/arch/arm/mach-sa1100/ssp.c
 *
 *  Copyright (C) 2003 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Generic SSP driver.  This provides the generic core for simple
 *  IO-based SSP applications.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <asm/hardware/ssp.h>

#define TIMEOUT 100000

static irqreturn_t ssp_interrupt(int irq, void *dev_id)
{
	unsigned int status = Ser4SSSR;

	if (status & SSSR_ROR)
		printk(KERN_WARNING "SSP: receiver overrun\n");

	Ser4SSSR = SSSR_ROR;

	return status ? IRQ_HANDLED : IRQ_NONE;
}

int ssp_write_word(u16 data)
{
	int timeout = TIMEOUT;

	while (!(Ser4SSSR & SSSR_TNF)) {
	        if (!--timeout)
	        	return -ETIMEDOUT;
		cpu_relax();
	}

	Ser4SSDR = data;

	timeout = TIMEOUT;
	while (!(Ser4SSSR & SSSR_BSY)) {
	        if (!--timeout)
	        	return -ETIMEDOUT;
		cpu_relax();
	}

	return 0;
}

int ssp_read_word(u16 *data)
{
	int timeout = TIMEOUT;

	while (!(Ser4SSSR & SSSR_RNE)) {
	        if (!--timeout)
	        	return -ETIMEDOUT;
		cpu_relax();
	}

	*data = (u16)Ser4SSDR;

	return 0;
}

int ssp_flush(void)
{
	int timeout = TIMEOUT * 2;

	do {
		while (Ser4SSSR & SSSR_RNE) {
		        if (!--timeout)
		        	return -ETIMEDOUT;
			(void) Ser4SSDR;
		}
	        if (!--timeout)
	        	return -ETIMEDOUT;
	} while (Ser4SSSR & SSSR_BSY);

	return 0;
}

void ssp_enable(void)
{
	Ser4SSCR0 |= SSCR0_SSE;
}

void ssp_disable(void)
{
	Ser4SSCR0 &= ~SSCR0_SSE;
}

void ssp_save_state(struct ssp_state *ssp)
{
	ssp->cr0 = Ser4SSCR0;
	ssp->cr1 = Ser4SSCR1;

	Ser4SSCR0 &= ~SSCR0_SSE;
}

void ssp_restore_state(struct ssp_state *ssp)
{
	Ser4SSSR = SSSR_ROR;

	Ser4SSCR0 = ssp->cr0 & ~SSCR0_SSE;
	Ser4SSCR1 = ssp->cr1;
	Ser4SSCR0 = ssp->cr0;
}

int ssp_init(void)
{
	int ret;

	if (!(PPAR & PPAR_SPR) && (Ser4MCCR0 & MCCR0_MCE))
		return -ENODEV;

	if (!request_mem_region(__PREG(Ser4SSCR0), 0x18, "SSP")) {
		return -EBUSY;
	}

	Ser4SSSR = SSSR_ROR;

	ret = request_irq(IRQ_Ser4SSP, ssp_interrupt, 0, "SSP", NULL);
	if (ret)
		goto out_region;

	return 0;

 out_region:
	release_mem_region(__PREG(Ser4SSCR0), 0x18);
	return ret;
}

void ssp_exit(void)
{
	Ser4SSCR0 &= ~SSCR0_SSE;

	free_irq(IRQ_Ser4SSP, NULL);
	release_mem_region(__PREG(Ser4SSCR0), 0x18);
}

MODULE_AUTHOR("Russell King");
MODULE_DESCRIPTION("SA11x0 SSP PIO driver");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(ssp_write_word);
EXPORT_SYMBOL(ssp_read_word);
EXPORT_SYMBOL(ssp_flush);
EXPORT_SYMBOL(ssp_enable);
EXPORT_SYMBOL(ssp_disable);
EXPORT_SYMBOL(ssp_save_state);
EXPORT_SYMBOL(ssp_restore_state);
EXPORT_SYMBOL(ssp_init);
EXPORT_SYMBOL(ssp_exit);
