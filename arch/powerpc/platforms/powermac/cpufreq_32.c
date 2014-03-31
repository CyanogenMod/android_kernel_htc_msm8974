/*
 *  Copyright (C) 2002 - 2005 Benjamin Herrenschmidt <benh@kernel.crashing.org>
 *  Copyright (C) 2004        John Steele Scott <toojays@toojays.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * TODO: Need a big cleanup here. Basically, we need to have different
 * cpufreq_driver structures for the different type of HW instead of the
 * current mess. We also need to better deal with the detection of the
 * type of machine.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/adb.h>
#include <linux/pmu.h>
#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/hardirq.h>
#include <asm/prom.h>
#include <asm/machdep.h>
#include <asm/irq.h>
#include <asm/pmac_feature.h>
#include <asm/mmu_context.h>
#include <asm/sections.h>
#include <asm/cputable.h>
#include <asm/time.h>
#include <asm/mpic.h>
#include <asm/keylargo.h>
#include <asm/switch_to.h>

#undef DEBUG_FREQ

extern void low_choose_7447a_dfs(int dfs);
extern void low_choose_750fx_pll(int pll);
extern void low_sleep_handler(void);

static unsigned int low_freq;
static unsigned int hi_freq;
static unsigned int cur_freq;
static unsigned int sleep_freq;

static int (*set_speed_proc)(int low_speed);
static unsigned int (*get_speed_proc)(void);

static u32 voltage_gpio;
static u32 frequency_gpio;
static u32 slew_done_gpio;
static int no_schedule;
static int has_cpu_l2lve;
static int is_pmu_based;

#define CPUFREQ_HIGH                  0
#define CPUFREQ_LOW                   1

static struct cpufreq_frequency_table pmac_cpu_freqs[] = {
	{CPUFREQ_HIGH, 		0},
	{CPUFREQ_LOW,		0},
	{0,			CPUFREQ_TABLE_END},
};

static struct freq_attr* pmac_cpu_freqs_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static inline void local_delay(unsigned long ms)
{
	if (no_schedule)
		mdelay(ms);
	else
		msleep(ms);
}

#ifdef DEBUG_FREQ
static inline void debug_calc_bogomips(void)
{
	unsigned long save_lpj = loops_per_jiffy;
	calibrate_delay();
	loops_per_jiffy = save_lpj;
}
#endif 

static int cpu_750fx_cpu_speed(int low_speed)
{
	u32 hid2;

	if (low_speed == 0) {
		
		pmac_call_feature(PMAC_FTR_WRITE_GPIO, NULL, voltage_gpio, 0x05);
		
		local_delay(10);

		
		if (has_cpu_l2lve) {
			hid2 = mfspr(SPRN_HID2);
			hid2 &= ~0x2000;
			mtspr(SPRN_HID2, hid2);
		}
	}
#ifdef CONFIG_6xx
	low_choose_750fx_pll(low_speed);
#endif
	if (low_speed == 1) {
		
		if (has_cpu_l2lve) {
			hid2 = mfspr(SPRN_HID2);
			hid2 |= 0x2000;
			mtspr(SPRN_HID2, hid2);
		}

		
		pmac_call_feature(PMAC_FTR_WRITE_GPIO, NULL, voltage_gpio, 0x04);
		local_delay(10);
	}

	return 0;
}

static unsigned int cpu_750fx_get_cpu_speed(void)
{
	if (mfspr(SPRN_HID1) & HID1_PS)
		return low_freq;
	else
		return hi_freq;
}

static int dfs_set_cpu_speed(int low_speed)
{
	if (low_speed == 0) {
		
		pmac_call_feature(PMAC_FTR_WRITE_GPIO, NULL, voltage_gpio, 0x05);
		
		local_delay(1);
	}

	
#ifdef CONFIG_6xx
	low_choose_7447a_dfs(low_speed);
#endif
	udelay(100);

	if (low_speed == 1) {
		
		pmac_call_feature(PMAC_FTR_WRITE_GPIO, NULL, voltage_gpio, 0x04);
		local_delay(1);
	}

	return 0;
}

static unsigned int dfs_get_cpu_speed(void)
{
	if (mfspr(SPRN_HID1) & HID1_DFS)
		return low_freq;
	else
		return hi_freq;
}


static int gpios_set_cpu_speed(int low_speed)
{
	int gpio, timeout = 0;

	
	if (low_speed == 0) {
		pmac_call_feature(PMAC_FTR_WRITE_GPIO, NULL, voltage_gpio, 0x05);
		
		local_delay(10);
	}

	
	gpio = 	pmac_call_feature(PMAC_FTR_READ_GPIO, NULL, frequency_gpio, 0);
	if (low_speed == ((gpio & 0x01) == 0))
		goto skip;

	pmac_call_feature(PMAC_FTR_WRITE_GPIO, NULL, frequency_gpio,
			  low_speed ? 0x04 : 0x05);
	udelay(200);
	do {
		if (++timeout > 100)
			break;
		local_delay(1);
		gpio = pmac_call_feature(PMAC_FTR_READ_GPIO, NULL, slew_done_gpio, 0);
	} while((gpio & 0x02) == 0);
 skip:
	
	if (low_speed == 1) {
		pmac_call_feature(PMAC_FTR_WRITE_GPIO, NULL, voltage_gpio, 0x04);
		
		local_delay(10);
	}

#ifdef DEBUG_FREQ
	debug_calc_bogomips();
#endif

	return 0;
}

static int pmu_set_cpu_speed(int low_speed)
{
	struct adb_request req;
	unsigned long save_l2cr;
	unsigned long save_l3cr;
	unsigned int pic_prio;
	unsigned long flags;

	preempt_disable();

#ifdef DEBUG_FREQ
	printk(KERN_DEBUG "HID1, before: %x\n", mfspr(SPRN_HID1));
#endif
	pmu_suspend();

	
 	pic_prio = mpic_cpu_get_priority();
	mpic_cpu_set_priority(0xf);

	
	asm volatile("mtdec %0" : : "r" (0x7fffffff));
	mb();
	asm volatile("mtdec %0" : : "r" (0x7fffffff));

	
	local_irq_save(flags);

	
	enable_kernel_fp();

#ifdef CONFIG_ALTIVEC
	if (cpu_has_feature(CPU_FTR_ALTIVEC))
		enable_kernel_altivec();
#endif 

	
	save_l3cr = _get_L3CR();	
	save_l2cr = _get_L2CR();	

	pmu_request(&req, NULL, 6, PMU_CPU_SPEED, 'W', 'O', 'O', 'F', low_speed);
	while (!req.complete)
		pmu_poll();

	
	pmac_call_feature(PMAC_FTR_SLEEP_STATE,NULL,1,1);

	low_sleep_handler();

	
	pmac_call_feature(PMAC_FTR_SLEEP_STATE,NULL,1,0);

	
	if (save_l2cr != 0xffffffff && (save_l2cr & L2CR_L2E) != 0)
 		_set_L2CR(save_l2cr);
	
	if (save_l3cr != 0xffffffff && (save_l3cr & L3CR_L3E) != 0)
 		_set_L3CR(save_l3cr);

	
	switch_mmu_context(NULL, current->active_mm);

#ifdef DEBUG_FREQ
	printk(KERN_DEBUG "HID1, after: %x\n", mfspr(SPRN_HID1));
#endif

	
	pmu_unlock();

	set_dec(1);

	
 	mpic_cpu_set_priority(pic_prio);

	
	local_irq_restore(flags);

#ifdef DEBUG_FREQ
	debug_calc_bogomips();
#endif

	pmu_resume();

	preempt_enable();

	return 0;
}

static int do_set_cpu_speed(int speed_mode, int notify)
{
	struct cpufreq_freqs freqs;
	unsigned long l3cr;
	static unsigned long prev_l3cr;

	freqs.old = cur_freq;
	freqs.new = (speed_mode == CPUFREQ_HIGH) ? hi_freq : low_freq;
	freqs.cpu = smp_processor_id();

	if (freqs.old == freqs.new)
		return 0;

	if (notify)
		cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);
	if (speed_mode == CPUFREQ_LOW &&
	    cpu_has_feature(CPU_FTR_L3CR)) {
		l3cr = _get_L3CR();
		if (l3cr & L3CR_L3E) {
			prev_l3cr = l3cr;
			_set_L3CR(0);
		}
	}
	set_speed_proc(speed_mode == CPUFREQ_LOW);
	if (speed_mode == CPUFREQ_HIGH &&
	    cpu_has_feature(CPU_FTR_L3CR)) {
		l3cr = _get_L3CR();
		if ((prev_l3cr & L3CR_L3E) && l3cr != prev_l3cr)
			_set_L3CR(prev_l3cr);
	}
	if (notify)
		cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
	cur_freq = (speed_mode == CPUFREQ_HIGH) ? hi_freq : low_freq;

	return 0;
}

static unsigned int pmac_cpufreq_get_speed(unsigned int cpu)
{
	return cur_freq;
}

static int pmac_cpufreq_verify(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy, pmac_cpu_freqs);
}

static int pmac_cpufreq_target(	struct cpufreq_policy *policy,
					unsigned int target_freq,
					unsigned int relation)
{
	unsigned int    newstate = 0;
	int		rc;

	if (cpufreq_frequency_table_target(policy, pmac_cpu_freqs,
			target_freq, relation, &newstate))
		return -EINVAL;

	rc = do_set_cpu_speed(newstate, 1);

	ppc_proc_freq = cur_freq * 1000ul;
	return rc;
}

static int pmac_cpufreq_cpu_init(struct cpufreq_policy *policy)
{
	if (policy->cpu != 0)
		return -ENODEV;

	policy->cpuinfo.transition_latency	= CPUFREQ_ETERNAL;
	policy->cur = cur_freq;

	cpufreq_frequency_table_get_attr(pmac_cpu_freqs, policy->cpu);
	return cpufreq_frequency_table_cpuinfo(policy, pmac_cpu_freqs);
}

static u32 read_gpio(struct device_node *np)
{
	const u32 *reg = of_get_property(np, "reg", NULL);
	u32 offset;

	if (reg == NULL)
		return 0;
	offset = *reg;
	if (offset < KEYLARGO_GPIO_LEVELS0)
		offset += KEYLARGO_GPIO_LEVELS0;
	return offset;
}

static int pmac_cpufreq_suspend(struct cpufreq_policy *policy)
{
	no_schedule = 1;
	sleep_freq = cur_freq;
	if (cur_freq == low_freq && !is_pmu_based)
		do_set_cpu_speed(CPUFREQ_HIGH, 0);
	return 0;
}

static int pmac_cpufreq_resume(struct cpufreq_policy *policy)
{
	
	if (get_speed_proc)
		cur_freq = get_speed_proc();
	else
		cur_freq = 0;

	do_set_cpu_speed(sleep_freq == low_freq ?
			 CPUFREQ_LOW : CPUFREQ_HIGH, 0);

	ppc_proc_freq = cur_freq * 1000ul;

	no_schedule = 0;
	return 0;
}

static struct cpufreq_driver pmac_cpufreq_driver = {
	.verify 	= pmac_cpufreq_verify,
	.target 	= pmac_cpufreq_target,
	.get		= pmac_cpufreq_get_speed,
	.init		= pmac_cpufreq_cpu_init,
	.suspend	= pmac_cpufreq_suspend,
	.resume		= pmac_cpufreq_resume,
	.flags		= CPUFREQ_PM_NO_WARN,
	.attr		= pmac_cpu_freqs_attr,
	.name		= "powermac",
	.owner		= THIS_MODULE,
};


static int pmac_cpufreq_init_MacRISC3(struct device_node *cpunode)
{
	struct device_node *volt_gpio_np = of_find_node_by_name(NULL,
								"voltage-gpio");
	struct device_node *freq_gpio_np = of_find_node_by_name(NULL,
								"frequency-gpio");
	struct device_node *slew_done_gpio_np = of_find_node_by_name(NULL,
								     "slewing-done");
	const u32 *value;


	if (volt_gpio_np)
		voltage_gpio = read_gpio(volt_gpio_np);
	if (freq_gpio_np)
		frequency_gpio = read_gpio(freq_gpio_np);
	if (slew_done_gpio_np)
		slew_done_gpio = read_gpio(slew_done_gpio_np);

	if (frequency_gpio && slew_done_gpio) {
		int lenp, rc;
		const u32 *freqs, *ratio;

		freqs = of_get_property(cpunode, "bus-frequencies", &lenp);
		lenp /= sizeof(u32);
		if (freqs == NULL || lenp != 2) {
			printk(KERN_ERR "cpufreq: bus-frequencies incorrect or missing\n");
			return 1;
		}
		ratio = of_get_property(cpunode, "processor-to-bus-ratio*2",
						NULL);
		if (ratio == NULL) {
			printk(KERN_ERR "cpufreq: processor-to-bus-ratio*2 missing\n");
			return 1;
		}

		
		low_freq = min(freqs[0], freqs[1]);
		hi_freq = max(freqs[0], freqs[1]);

		if (low_freq < 98000000)
			low_freq = 101000000;

		
		low_freq = (low_freq * (*ratio)) / 2000;
		hi_freq = (hi_freq * (*ratio)) / 2000;

		rc = pmac_call_feature(PMAC_FTR_READ_GPIO, NULL, frequency_gpio, 0);
		cur_freq = (rc & 0x01) ? hi_freq : low_freq;

		set_speed_proc = gpios_set_cpu_speed;
		return 1;
	}

	value = of_get_property(cpunode, "min-clock-frequency", NULL);
	if (!value)
		return 1;
	low_freq = (*value) / 1000;
	if (low_freq < 100000)
		low_freq *= 10;

	value = of_get_property(cpunode, "max-clock-frequency", NULL);
	if (!value)
		return 1;
	hi_freq = (*value) / 1000;
	set_speed_proc = pmu_set_cpu_speed;
	is_pmu_based = 1;

	return 0;
}

static int pmac_cpufreq_init_7447A(struct device_node *cpunode)
{
	struct device_node *volt_gpio_np;

	if (of_get_property(cpunode, "dynamic-power-step", NULL) == NULL)
		return 1;

	volt_gpio_np = of_find_node_by_name(NULL, "cpu-vcore-select");
	if (volt_gpio_np)
		voltage_gpio = read_gpio(volt_gpio_np);
	if (!voltage_gpio){
		printk(KERN_ERR "cpufreq: missing cpu-vcore-select gpio\n");
		return 1;
	}

	
	hi_freq = cur_freq;
	low_freq = cur_freq/2;

	
	cur_freq = dfs_get_cpu_speed();
	set_speed_proc = dfs_set_cpu_speed;
	get_speed_proc = dfs_get_cpu_speed;

	return 0;
}

static int pmac_cpufreq_init_750FX(struct device_node *cpunode)
{
	struct device_node *volt_gpio_np;
	u32 pvr;
	const u32 *value;

	if (of_get_property(cpunode, "dynamic-power-step", NULL) == NULL)
		return 1;

	hi_freq = cur_freq;
	value = of_get_property(cpunode, "reduced-clock-frequency", NULL);
	if (!value)
		return 1;
	low_freq = (*value) / 1000;

	volt_gpio_np = of_find_node_by_name(NULL, "cpu-vcore-select");
	if (volt_gpio_np)
		voltage_gpio = read_gpio(volt_gpio_np);

	pvr = mfspr(SPRN_PVR);
	has_cpu_l2lve = !((pvr & 0xf00) == 0x100);

	set_speed_proc = cpu_750fx_cpu_speed;
	get_speed_proc = cpu_750fx_get_cpu_speed;
	cur_freq = cpu_750fx_get_cpu_speed();

	return 0;
}

static int __init pmac_cpufreq_setup(void)
{
	struct device_node	*cpunode;
	const u32		*value;

	if (strstr(cmd_line, "nocpufreq"))
		return 0;

	
	cpunode = of_find_node_by_type(NULL, "cpu");
	if (!cpunode)
		goto out;

	
	value = of_get_property(cpunode, "clock-frequency", NULL);
	if (!value)
		goto out;
	cur_freq = (*value) / 1000;

	
	if (of_machine_is_compatible("MacRISC3") &&
	    of_get_property(cpunode, "dynamic-power-step", NULL) &&
	    PVR_VER(mfspr(SPRN_PVR)) == 0x8003) {
		pmac_cpufreq_init_7447A(cpunode);
	
	} else if (of_machine_is_compatible("PowerBook3,4") ||
		   of_machine_is_compatible("PowerBook3,5") ||
		   of_machine_is_compatible("MacRISC3")) {
		pmac_cpufreq_init_MacRISC3(cpunode);
	
	} else if (of_machine_is_compatible("PowerBook4,1")) {
		hi_freq = cur_freq;
		low_freq = 400000;
		set_speed_proc = pmu_set_cpu_speed;
		is_pmu_based = 1;
	}
	
	else if (of_machine_is_compatible("PowerBook3,3") && cur_freq == 550000) {
		hi_freq = cur_freq;
		low_freq = 500000;
		set_speed_proc = pmu_set_cpu_speed;
		is_pmu_based = 1;
	}
	
	else if (of_machine_is_compatible("PowerBook3,2")) {
		if (cur_freq < 350000 || cur_freq > 550000)
			goto out;
		hi_freq = cur_freq;
		low_freq = 300000;
		set_speed_proc = pmu_set_cpu_speed;
		is_pmu_based = 1;
	}
	
	else if (PVR_VER(mfspr(SPRN_PVR)) == 0x7000)
		pmac_cpufreq_init_750FX(cpunode);
out:
	of_node_put(cpunode);
	if (set_speed_proc == NULL)
		return -ENODEV;

	pmac_cpu_freqs[CPUFREQ_LOW].frequency = low_freq;
	pmac_cpu_freqs[CPUFREQ_HIGH].frequency = hi_freq;
	ppc_proc_freq = cur_freq * 1000ul;

	printk(KERN_INFO "Registering PowerMac CPU frequency driver\n");
	printk(KERN_INFO "Low: %d Mhz, High: %d Mhz, Boot: %d Mhz\n",
	       low_freq/1000, hi_freq/1000, cur_freq/1000);

	return cpufreq_register_driver(&pmac_cpufreq_driver);
}

module_init(pmac_cpufreq_setup);

