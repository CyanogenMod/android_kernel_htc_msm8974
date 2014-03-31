/*
 * arch/arm/mach-pnx4008/time.c
 *
 * PNX4008 Timers
 *
 * Authors: Vitaly Wool, Dmitry Chigirev, Grigory Tolstolytkin <source@mvista.com>
 *
 * 2005 (c) MontaVista Software, Inc. This file is licensed under
 * the terms of the GNU General Public License version 2. This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/irq.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <asm/leds.h>
#include <asm/mach/time.h>
#include <asm/errno.h>

#include "time.h"


static unsigned long pnx4008_gettimeoffset(void)
{
	u32 ticks_to_match =
	    __raw_readl(HSTIM_MATCH0) - __raw_readl(HSTIM_COUNTER);
	u32 elapsed = LATCH - ticks_to_match;
	return (elapsed * (tick_nsec / 1000)) / LATCH;
}

static irqreturn_t pnx4008_timer_interrupt(int irq, void *dev_id)
{
	if (__raw_readl(HSTIM_INT) & MATCH0_INT) {

		do {
			timer_tick();

			__raw_writel(__raw_readl(HSTIM_MATCH0) + LATCH,
				     HSTIM_MATCH0);
			__raw_writel(MATCH0_INT, HSTIM_INT);	

		} while ((signed)
			 (__raw_readl(HSTIM_MATCH0) -
			  __raw_readl(HSTIM_COUNTER)) < 0);
	}

	return IRQ_HANDLED;
}

static struct irqaction pnx4008_timer_irq = {
	.name = "PNX4008 Tick Timer",
	.flags = IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler = pnx4008_timer_interrupt
};

static __init void pnx4008_setup_timer(void)
{
	__raw_writel(RESET_COUNT, MSTIM_CTRL);
	while (__raw_readl(MSTIM_COUNTER)) ;	
	__raw_writel(0, MSTIM_CTRL);	
	__raw_writel(0, MSTIM_MCTRL);

	__raw_writel(RESET_COUNT, HSTIM_CTRL);
	while (__raw_readl(HSTIM_COUNTER)) ;	
	__raw_writel(0, HSTIM_CTRL);
	__raw_writel(0, HSTIM_MCTRL);
	__raw_writel(0, HSTIM_CCR);
	__raw_writel(12, HSTIM_PMATCH);	
	__raw_writel(LATCH, HSTIM_MATCH0);
	__raw_writel(MR0_INT, HSTIM_MCTRL);

	setup_irq(HSTIMER_INT, &pnx4008_timer_irq);

	__raw_writel(COUNT_ENAB | DEBUG_EN, HSTIM_CTRL);	
}

#define TIMCLK_CTRL_REG  IO_ADDRESS((PNX4008_PWRMAN_BASE + 0xBC))
#define WATCHDOG_CLK_EN                   1
#define TIMER_CLK_EN                      2	

static u32 timclk_ctrl_reg_save;

void pnx4008_timer_suspend(void)
{
	timclk_ctrl_reg_save = __raw_readl(TIMCLK_CTRL_REG);
	__raw_writel(0, TIMCLK_CTRL_REG);	
}

void pnx4008_timer_resume(void)
{
	__raw_writel(timclk_ctrl_reg_save, TIMCLK_CTRL_REG);	
}

struct sys_timer pnx4008_timer = {
	.init = pnx4008_setup_timer,
	.offset = pnx4008_gettimeoffset,
	.suspend = pnx4008_timer_suspend,
	.resume = pnx4008_timer_resume,
};

