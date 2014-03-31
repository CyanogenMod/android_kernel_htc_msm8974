/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/bitops.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/spinlock.h>

#include <asm/hardware/gic.h>
#include <mach/msm_smsm.h>

#include "mpm-8625.h"

#define NUM_REGS_ENABLE		2
#define NUM_REGS_DISABLE	3
#define GIC_IRQ_MASK(irq)	BIT(irq % 32)
#define GIC_IRQ_INDEX(irq)	(irq / 32)

enum {
	IRQ_DEBUG_SLEEP_INT_TRIGGER	= BIT(0),
	IRQ_DEBUG_SLEEP_INT		= BIT(1),
	IRQ_DEBUG_SLEEP_ABORT		= BIT(2),
	IRQ_DEBUG_SLEEP			= BIT(3),
	IRQ_DEBUG_SLEEP_REQUEST		= BIT(4)
};

static int msm_gic_irq_debug_mask;
module_param_named(debug_mask, msm_gic_irq_debug_mask, int,
		S_IRUGO | S_IWUSR | S_IWGRP);

static uint32_t msm_gic_irq_smsm_wake_enable[NUM_REGS_ENABLE];
static uint32_t msm_gic_irq_idle_disable[NUM_REGS_DISABLE];

#define SMSM_FAKE_IRQ	(0xff)

 
static uint8_t msm_gic_irq_to_smsm[NR_IRQS] = {
	[MSM8625_INT_USB_OTG]		= 4,
	[MSM8625_INT_PWB_I2C]		= 5,
	[MSM8625_INT_SDC1_0]		= 6,
	[MSM8625_INT_SDC1_1]		= 7,
	[MSM8625_INT_SDC2_0]		= 8,
	[MSM8625_INT_SDC2_1]		= 9,
	[MSM8625_INT_ADSP_A9_A11]	= 10,
	[MSM8625_INT_UART1]		= 11,
	[MSM8625_INT_UART2]		= 12,
	[MSM8625_INT_UART3]		= 13,
	[MSM8625_INT_UART1_RX]		= 14,
	[MSM8625_INT_UART2_RX]		= 15,
	[MSM8625_INT_UART3_RX]		= 16,
	[MSM8625_INT_UART1DM_IRQ]	= 17,
	[MSM8625_INT_UART1DM_RX]	= 18,
	[MSM8625_INT_KEYSENSE]		= 19,
	[MSM8625_INT_AD_HSSD]		= 20,
	[MSM8625_INT_NAND_WR_ER_DONE]	= 21,
	[MSM8625_INT_NAND_OP_DONE]	= 22,
	[MSM8625_INT_TCHSCRN1]		= 23,
	[MSM8625_INT_TCHSCRN2]		= 24,
	[MSM8625_INT_TCHSCRN_SSBI]	= 25,
	[MSM8625_INT_USB_HS]		= 26,
	[MSM8625_INT_UART2DM_RX]	= 27,
	[MSM8625_INT_UART2DM_IRQ]	= 28,
	[MSM8625_INT_SDC4_1]		= 29,
	[MSM8625_INT_SDC4_0]		= 30,
	[MSM8625_INT_SDC3_1]		= 31,
	[MSM8625_INT_SDC3_0]		= 32,

	
	[MSM8625_INT_GPIO_GROUP1]	= SMSM_FAKE_IRQ,
	[MSM8625_INT_GPIO_GROUP2]	= SMSM_FAKE_IRQ,
	[MSM8625_INT_A9_M2A_0]		= SMSM_FAKE_IRQ,
	[MSM8625_INT_A9_M2A_1]		= SMSM_FAKE_IRQ,
	[MSM8625_INT_A9_M2A_2]          = SMSM_FAKE_IRQ,
	[MSM8625_INT_A9_M2A_5]		= SMSM_FAKE_IRQ,
	[MSM8625_INT_GP_TIMER_EXP]	= SMSM_FAKE_IRQ,
	[MSM8625_INT_DEBUG_TIMER_EXP]	= SMSM_FAKE_IRQ,
	[MSM8625_INT_ADSP_A11]		= SMSM_FAKE_IRQ,
};

static uint16_t msm_bypassed_apps_irqs[] = {
	MSM8625_INT_CPR_IRQ0,
	MSM8625_INT_SC_SICL2PERFMONIRPTREQ,
};

static bool msm_mpm_bypass_apps_irq(unsigned int irq)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(msm_bypassed_apps_irqs); i++)
		if (irq == msm_bypassed_apps_irqs[i])
			return true;

	return false;
}

static void msm_gic_mask_irq(struct irq_data *d)
{
	unsigned int index = GIC_IRQ_INDEX(d->irq);
	uint32_t mask;
	int smsm_irq = msm_gic_irq_to_smsm[d->irq];

	mask = GIC_IRQ_MASK(d->irq);

	
	if (msm_mpm_bypass_apps_irq(d->irq))
		return;

	if (smsm_irq == 0) {
		msm_gic_irq_idle_disable[index] &= ~mask;
	} else {
		mask = GIC_IRQ_MASK(smsm_irq - 1);
		msm_gic_irq_smsm_wake_enable[0] &= ~mask;
	}
}

static void msm_gic_unmask_irq(struct irq_data *d)
{
	unsigned int index = GIC_IRQ_INDEX(d->irq);
	uint32_t mask;
	int smsm_irq = msm_gic_irq_to_smsm[d->irq];

	mask = GIC_IRQ_MASK(d->irq);

	
	if (msm_mpm_bypass_apps_irq(d->irq))
		return;

	if (smsm_irq == 0) {
		msm_gic_irq_idle_disable[index] |= mask;
	} else {
		mask = GIC_IRQ_MASK(smsm_irq - 1);
		msm_gic_irq_smsm_wake_enable[0] |= mask;
	}
}

static int msm_gic_set_irq_wake(struct irq_data *d, unsigned int on)
{
	uint32_t mask;
	int smsm_irq = msm_gic_irq_to_smsm[d->irq];

	if (smsm_irq == 0) {
		pr_err("bad wake up irq %d\n", d->irq);
		return  -EINVAL;
	}

	
	if (msm_mpm_bypass_apps_irq(d->irq))
		return 0;

	if (smsm_irq == SMSM_FAKE_IRQ)
		return 0;

	mask = GIC_IRQ_MASK(smsm_irq - 1);
	if (on)
		msm_gic_irq_smsm_wake_enable[1] |= mask;
	else
		msm_gic_irq_smsm_wake_enable[1] &= ~mask;

	return 0;
}

void __init msm_gic_irq_extn_init(void)
{
	gic_arch_extn.irq_mask	= msm_gic_mask_irq;
	gic_arch_extn.irq_unmask = msm_gic_unmask_irq;
	gic_arch_extn.irq_disable = msm_gic_mask_irq;
	gic_arch_extn.irq_set_wake = msm_gic_set_irq_wake;
}



int msm_gic_irq_idle_sleep_allowed(void)
{
	uint32_t i, disable = 0;

	for (i = 0; i < NUM_REGS_DISABLE; i++)
		disable |= msm_gic_irq_idle_disable[i];

	return !disable;
}

void msm_gic_irq_enter_sleep1(bool modem_wake, int from_idle, uint32_t
		*irq_mask)
{
	if (modem_wake) {
		*irq_mask = msm_gic_irq_smsm_wake_enable[!from_idle];
		if (msm_gic_irq_debug_mask & IRQ_DEBUG_SLEEP)
			pr_info("%s irq_mask %x\n", __func__, *irq_mask);
	}
}

int msm_gic_irq_enter_sleep2(bool modem_wake, int from_idle)
{
	if (from_idle && !modem_wake)
		return 0;

	
	WARN_ON_ONCE(!modem_wake && !from_idle);

	if (msm_gic_irq_debug_mask & IRQ_DEBUG_SLEEP)
		pr_info("%s interrupts pending\n", __func__);

	
	if (msm_gic_spi_ppi_pending()) {
		if (msm_gic_irq_debug_mask & IRQ_DEBUG_SLEEP_ABORT)
			pr_info("%s aborted....\n", __func__);
		return -EAGAIN;
	}

	if (modem_wake) {
		msm_gic_save();
		irq_set_irq_type(MSM8625_INT_A9_M2A_6, IRQF_TRIGGER_RISING);
		enable_irq(MSM8625_INT_A9_M2A_6);
		pr_debug("%s going for sleep now\n", __func__);
	}

	return 0;
}

void msm_gic_irq_exit_sleep1(uint32_t irq_mask, uint32_t wakeup_reason,
		uint32_t pending_irqs)
{
	
	msm_gic_restore();

	
	disable_irq(MSM8625_INT_A9_M2A_6);

	if (msm_gic_irq_debug_mask & IRQ_DEBUG_SLEEP)
		pr_info("%s %x %x %x now\n", __func__, irq_mask,
				pending_irqs, wakeup_reason);
}

void msm_gic_irq_exit_sleep2(uint32_t irq_mask, uint32_t wakeup_reason,
			uint32_t pending)
{
	int i, smsm_irq, smsm_mask;
	struct irq_desc *desc;

	if (msm_gic_irq_debug_mask & IRQ_DEBUG_SLEEP)
		pr_info("%s %x %x %x now\n", __func__, irq_mask,
				 pending, wakeup_reason);

	for (i = 0; pending && i < ARRAY_SIZE(msm_gic_irq_to_smsm); i++) {
		smsm_irq = msm_gic_irq_to_smsm[i];

		if (smsm_irq == 0)
			continue;

		smsm_mask = BIT(smsm_irq - 1);
		if (!(pending & smsm_mask))
			continue;

		pending &= ~smsm_mask;

		if (msm_gic_irq_debug_mask & IRQ_DEBUG_SLEEP_INT)
			pr_info("%s, irq %d, still pending %x now\n",
					__func__, i, pending);
		
		desc = i ? irq_to_desc(i) : NULL;

		
		if (desc && !irqd_is_level_type(&desc->irq_data)) {
			
			irq_set_pending(i);
			check_irq_resend(desc, i);
		}
	}
}

void msm_gic_irq_exit_sleep3(uint32_t irq_mask, uint32_t wakeup_reason,
		uint32_t pending_irqs)
{
	if (msm_gic_irq_debug_mask & IRQ_DEBUG_SLEEP)
		pr_info("%s, irq_mask %x pending_irqs %x, wakeup_reason %x,"
				"state %x now\n", __func__, irq_mask,
				pending_irqs, wakeup_reason,
				smsm_get_state(SMSM_MODEM_STATE));
}
