/*
 *  This file was based upon code in Powertweak Linux (http://powertweak.sf.net)
 *  (C) 2000-2003  Dave Jones, Arjan van de Ven, Janne Pänkälä,
 *                 Dominik Brodowski.
 *
 *  Licensed under the terms of the GNU GPL License version 2.
 *
 *  BIG FAT DISCLAIMER: Work in progress code. Possibly *dangerous*
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/ioport.h>
#include <linux/timex.h>
#include <linux/io.h>

#include <asm/cpu_device_id.h>
#include <asm/msr.h>

#define POWERNOW_IOPORT 0xfff0          

#define PFX "powernow-k6: "
static unsigned int                     busfreq;   
static unsigned int                     max_multiplier;


static struct cpufreq_frequency_table clock_ratio[] = {
	{45,   0},
	{50,   0},
	{40,   0},
	{55,   0},
	{20,   0},
	{30,   0},
	{60,   0},
	{35,   0},
	{0, CPUFREQ_TABLE_END}
};


static int powernow_k6_get_cpu_multiplier(void)
{
	u64 invalue = 0;
	u32 msrval;

	msrval = POWERNOW_IOPORT + 0x1;
	wrmsr(MSR_K6_EPMR, msrval, 0); 
	invalue = inl(POWERNOW_IOPORT + 0x8);
	msrval = POWERNOW_IOPORT + 0x0;
	wrmsr(MSR_K6_EPMR, msrval, 0); 

	return clock_ratio[(invalue >> 5)&7].index;
}


static void powernow_k6_set_state(unsigned int best_i)
{
	unsigned long outvalue = 0, invalue = 0;
	unsigned long msrval;
	struct cpufreq_freqs freqs;

	if (clock_ratio[best_i].index > max_multiplier) {
		printk(KERN_ERR PFX "invalid target frequency\n");
		return;
	}

	freqs.old = busfreq * powernow_k6_get_cpu_multiplier();
	freqs.new = busfreq * clock_ratio[best_i].index;
	freqs.cpu = 0; 

	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	

	outvalue = (1<<12) | (1<<10) | (1<<9) | (best_i<<5);

	msrval = POWERNOW_IOPORT + 0x1;
	wrmsr(MSR_K6_EPMR, msrval, 0); 
	invalue = inl(POWERNOW_IOPORT + 0x8);
	invalue = invalue & 0xf;
	outvalue = outvalue | invalue;
	outl(outvalue , (POWERNOW_IOPORT + 0x8));
	msrval = POWERNOW_IOPORT + 0x0;
	wrmsr(MSR_K6_EPMR, msrval, 0); 

	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	return;
}


static int powernow_k6_verify(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy, &clock_ratio[0]);
}


static int powernow_k6_target(struct cpufreq_policy *policy,
			       unsigned int target_freq,
			       unsigned int relation)
{
	unsigned int newstate = 0;

	if (cpufreq_frequency_table_target(policy, &clock_ratio[0],
				target_freq, relation, &newstate))
		return -EINVAL;

	powernow_k6_set_state(newstate);

	return 0;
}


static int powernow_k6_cpu_init(struct cpufreq_policy *policy)
{
	unsigned int i, f;
	int result;

	if (policy->cpu != 0)
		return -ENODEV;

	
	max_multiplier = powernow_k6_get_cpu_multiplier();
	busfreq = cpu_khz / max_multiplier;

	
	for (i = 0; (clock_ratio[i].frequency != CPUFREQ_TABLE_END); i++) {
		f = clock_ratio[i].index;
		if (f > max_multiplier)
			clock_ratio[i].frequency = CPUFREQ_ENTRY_INVALID;
		else
			clock_ratio[i].frequency = busfreq * f;
	}

	
	policy->cpuinfo.transition_latency = 200000;
	policy->cur = busfreq * max_multiplier;

	result = cpufreq_frequency_table_cpuinfo(policy, clock_ratio);
	if (result)
		return result;

	cpufreq_frequency_table_get_attr(clock_ratio, policy->cpu);

	return 0;
}


static int powernow_k6_cpu_exit(struct cpufreq_policy *policy)
{
	unsigned int i;
	for (i = 0; i < 8; i++) {
		if (i == max_multiplier)
			powernow_k6_set_state(i);
	}
	cpufreq_frequency_table_put_attr(policy->cpu);
	return 0;
}

static unsigned int powernow_k6_get(unsigned int cpu)
{
	unsigned int ret;
	ret = (busfreq * powernow_k6_get_cpu_multiplier());
	return ret;
}

static struct freq_attr *powernow_k6_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver powernow_k6_driver = {
	.verify		= powernow_k6_verify,
	.target		= powernow_k6_target,
	.init		= powernow_k6_cpu_init,
	.exit		= powernow_k6_cpu_exit,
	.get		= powernow_k6_get,
	.name		= "powernow-k6",
	.owner		= THIS_MODULE,
	.attr		= powernow_k6_attr,
};

static const struct x86_cpu_id powernow_k6_ids[] = {
	{ X86_VENDOR_AMD, 5, 12 },
	{ X86_VENDOR_AMD, 5, 13 },
	{}
};
MODULE_DEVICE_TABLE(x86cpu, powernow_k6_ids);

static int __init powernow_k6_init(void)
{
	if (!x86_match_cpu(powernow_k6_ids))
		return -ENODEV;

	if (!request_region(POWERNOW_IOPORT, 16, "PowerNow!")) {
		printk(KERN_INFO PFX "PowerNow IOPORT region already used.\n");
		return -EIO;
	}

	if (cpufreq_register_driver(&powernow_k6_driver)) {
		release_region(POWERNOW_IOPORT, 16);
		return -EINVAL;
	}

	return 0;
}


static void __exit powernow_k6_exit(void)
{
	cpufreq_unregister_driver(&powernow_k6_driver);
	release_region(POWERNOW_IOPORT, 16);
}


MODULE_AUTHOR("Arjan van de Ven, Dave Jones <davej@redhat.com>, "
		"Dominik Brodowski <linux@brodo.de>");
MODULE_DESCRIPTION("PowerNow! driver for AMD K6-2+ / K6-3+ processors.");
MODULE_LICENSE("GPL");

module_init(powernow_k6_init);
module_exit(powernow_k6_exit);
