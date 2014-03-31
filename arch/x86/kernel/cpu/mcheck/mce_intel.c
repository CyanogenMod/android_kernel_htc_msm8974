/*
 * Intel specific MCE features.
 * Copyright 2004 Zwane Mwaikambo <zwane@linuxpower.ca>
 * Copyright (C) 2008, 2009 Intel Corporation
 * Author: Andi Kleen
 */

#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/percpu.h>
#include <linux/sched.h>
#include <asm/apic.h>
#include <asm/processor.h>
#include <asm/msr.h>
#include <asm/mce.h>


static DEFINE_PER_CPU(mce_banks_t, mce_banks_owned);

static DEFINE_RAW_SPINLOCK(cmci_discover_lock);

#define CMCI_THRESHOLD 1

static int cmci_supported(int *banks)
{
	u64 cap;

	if (mce_cmci_disabled || mce_ignore_ce)
		return 0;

	if (boot_cpu_data.x86_vendor != X86_VENDOR_INTEL)
		return 0;
	if (!cpu_has_apic || lapic_get_maxlvt() < 6)
		return 0;
	rdmsrl(MSR_IA32_MCG_CAP, cap);
	*banks = min_t(unsigned, MAX_NR_BANKS, cap & 0xff);
	return !!(cap & MCG_CMCI_P);
}

static void intel_threshold_interrupt(void)
{
	machine_check_poll(MCP_TIMESTAMP, &__get_cpu_var(mce_banks_owned));
	mce_notify_irq();
}

static void print_update(char *type, int *hdr, int num)
{
	if (*hdr == 0)
		printk(KERN_INFO "CPU %d MCA banks", smp_processor_id());
	*hdr = 1;
	printk(KERN_CONT " %s:%d", type, num);
}

static void cmci_discover(int banks, int boot)
{
	unsigned long *owned = (void *)&__get_cpu_var(mce_banks_owned);
	unsigned long flags;
	int hdr = 0;
	int i;

	raw_spin_lock_irqsave(&cmci_discover_lock, flags);
	for (i = 0; i < banks; i++) {
		u64 val;

		if (test_bit(i, owned))
			continue;

		rdmsrl(MSR_IA32_MCx_CTL2(i), val);

		
		if (val & MCI_CTL2_CMCI_EN) {
			if (test_and_clear_bit(i, owned) && !boot)
				print_update("SHD", &hdr, i);
			__clear_bit(i, __get_cpu_var(mce_poll_banks));
			continue;
		}

		val &= ~MCI_CTL2_CMCI_THRESHOLD_MASK;
		val |= MCI_CTL2_CMCI_EN | CMCI_THRESHOLD;
		wrmsrl(MSR_IA32_MCx_CTL2(i), val);
		rdmsrl(MSR_IA32_MCx_CTL2(i), val);

		
		if (val & MCI_CTL2_CMCI_EN) {
			if (!test_and_set_bit(i, owned) && !boot)
				print_update("CMCI", &hdr, i);
			__clear_bit(i, __get_cpu_var(mce_poll_banks));
		} else {
			WARN_ON(!test_bit(i, __get_cpu_var(mce_poll_banks)));
		}
	}
	raw_spin_unlock_irqrestore(&cmci_discover_lock, flags);
	if (hdr)
		printk(KERN_CONT "\n");
}

void cmci_recheck(void)
{
	unsigned long flags;
	int banks;

	if (!mce_available(__this_cpu_ptr(&cpu_info)) || !cmci_supported(&banks))
		return;
	local_irq_save(flags);
	machine_check_poll(MCP_TIMESTAMP, &__get_cpu_var(mce_banks_owned));
	local_irq_restore(flags);
}

void cmci_clear(void)
{
	unsigned long flags;
	int i;
	int banks;
	u64 val;

	if (!cmci_supported(&banks))
		return;
	raw_spin_lock_irqsave(&cmci_discover_lock, flags);
	for (i = 0; i < banks; i++) {
		if (!test_bit(i, __get_cpu_var(mce_banks_owned)))
			continue;
		
		rdmsrl(MSR_IA32_MCx_CTL2(i), val);
		val &= ~(MCI_CTL2_CMCI_EN|MCI_CTL2_CMCI_THRESHOLD_MASK);
		wrmsrl(MSR_IA32_MCx_CTL2(i), val);
		__clear_bit(i, __get_cpu_var(mce_banks_owned));
	}
	raw_spin_unlock_irqrestore(&cmci_discover_lock, flags);
}

void cmci_rediscover(int dying)
{
	int banks;
	int cpu;
	cpumask_var_t old;

	if (!cmci_supported(&banks))
		return;
	if (!alloc_cpumask_var(&old, GFP_KERNEL))
		return;
	cpumask_copy(old, &current->cpus_allowed);

	for_each_online_cpu(cpu) {
		if (cpu == dying)
			continue;
		if (set_cpus_allowed_ptr(current, cpumask_of(cpu)))
			continue;
		
		if (cmci_supported(&banks))
			cmci_discover(banks, 0);
	}

	set_cpus_allowed_ptr(current, old);
	free_cpumask_var(old);
}

void cmci_reenable(void)
{
	int banks;
	if (cmci_supported(&banks))
		cmci_discover(banks, 0);
}

static void intel_init_cmci(void)
{
	int banks;

	if (!cmci_supported(&banks))
		return;

	mce_threshold_vector = intel_threshold_interrupt;
	cmci_discover(banks, 1);
	apic_write(APIC_LVTCMCI, THRESHOLD_APIC_VECTOR|APIC_DM_FIXED);
	cmci_recheck();
}

void mce_intel_feature_init(struct cpuinfo_x86 *c)
{
	intel_init_thermal(c);
	intel_init_cmci();
}
