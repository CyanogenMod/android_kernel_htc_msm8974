/*
 *  Copyright (C) 2002 - 2005 Benjamin Herrenschmidt <benh@kernel.crashing.org>
 *  and                       Markus Demleitner <msdemlei@cl.uni-heidelberg.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This driver adds basic cpufreq support for SMU & 970FX based G5 Macs,
 * that is iMac G5 and latest single CPU desktop.
 */

#undef DEBUG

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <asm/prom.h>
#include <asm/machdep.h>
#include <asm/irq.h>
#include <asm/sections.h>
#include <asm/cputable.h>
#include <asm/time.h>
#include <asm/smu.h>
#include <asm/pmac_pfunc.h>

#define DBG(fmt...) pr_debug(fmt)


#define SCOM_PCR 0x0aa001			

#define PCR_HILO_SELECT		0x80000000U	
#define PCR_SPEED_FULL		0x00000000U	
#define PCR_SPEED_HALF		0x00020000U	
#define PCR_SPEED_QUARTER	0x00040000U	
#define PCR_SPEED_MASK		0x000e0000U	
#define PCR_SPEED_SHIFT		17
#define PCR_FREQ_REQ_VALID	0x00010000U	
#define PCR_VOLT_REQ_VALID	0x00008000U	
#define PCR_TARGET_TIME_MASK	0x00006000U	
#define PCR_STATLAT_MASK	0x00001f00U	
#define PCR_SNOOPLAT_MASK	0x000000f0U	
#define PCR_SNOOPACC_MASK	0x0000000fU	

#define SCOM_PSR 0x408001			
#define PSR_CMD_RECEIVED	0x2000000000000000U   
#define PSR_CMD_COMPLETED	0x1000000000000000U   
#define PSR_CUR_SPEED_MASK	0x0300000000000000U   
#define PSR_CUR_SPEED_SHIFT	(56)

#define CPUFREQ_HIGH                  0
#define CPUFREQ_LOW                   1

static struct cpufreq_frequency_table g5_cpu_freqs[] = {
	{CPUFREQ_HIGH, 		0},
	{CPUFREQ_LOW,		0},
	{0,			CPUFREQ_TABLE_END},
};

static struct freq_attr* g5_cpu_freqs_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static int g5_pmode_cur;

static void (*g5_switch_volt)(int speed_mode);
static int (*g5_switch_freq)(int speed_mode);
static int (*g5_query_freq)(void);

static DEFINE_MUTEX(g5_switch_mutex);

static unsigned long transition_latency;

#ifdef CONFIG_PMAC_SMU

static const u32 *g5_pmode_data;
static int g5_pmode_max;

static struct smu_sdbp_fvt *g5_fvt_table;	
static int g5_fvt_count;			
static int g5_fvt_cur;				


static void g5_smu_switch_volt(int speed_mode)
{
	struct smu_simple_cmd	cmd;

	DECLARE_COMPLETION_ONSTACK(comp);
	smu_queue_simple(&cmd, SMU_CMD_POWER_COMMAND, 8, smu_done_complete,
			 &comp, 'V', 'S', 'L', 'E', 'W',
			 0xff, g5_fvt_cur+1, speed_mode);
	wait_for_completion(&comp);
}


static struct pmf_function *pfunc_set_vdnap0;
static struct pmf_function *pfunc_vdnap0_complete;

static void g5_vdnap_switch_volt(int speed_mode)
{
	struct pmf_args args;
	u32 slew, done = 0;
	unsigned long timeout;

	slew = (speed_mode == CPUFREQ_LOW) ? 1 : 0;
	args.count = 1;
	args.u[0].p = &slew;

	pmf_call_one(pfunc_set_vdnap0, &args);

	timeout = jiffies + HZ/10;
	while(!time_after(jiffies, timeout)) {
		args.count = 1;
		args.u[0].p = &done;
		pmf_call_one(pfunc_vdnap0_complete, &args);
		if (done)
			break;
		msleep(1);
	}
	if (done == 0)
		printk(KERN_WARNING "cpufreq: Timeout in clock slewing !\n");
}


static int g5_scom_switch_freq(int speed_mode)
{
	unsigned long flags;
	int to;

	
	if (speed_mode < g5_pmode_cur)
		g5_switch_volt(speed_mode);

	local_irq_save(flags);

	
	scom970_write(SCOM_PCR, 0);
	
       	scom970_write(SCOM_PCR, PCR_HILO_SELECT | 0);
	
	scom970_write(SCOM_PCR, PCR_HILO_SELECT |
		      g5_pmode_data[speed_mode]);

	
	for (to = 0; to < 10; to++) {
		unsigned long psr = scom970_read(SCOM_PSR);

		if ((psr & PSR_CMD_RECEIVED) == 0 &&
		    (((psr >> PSR_CUR_SPEED_SHIFT) ^
		      (g5_pmode_data[speed_mode] >> PCR_SPEED_SHIFT)) & 0x3)
		    == 0)
			break;
		if (psr & PSR_CMD_COMPLETED)
			break;
		udelay(100);
	}

	local_irq_restore(flags);

	
	if (speed_mode > g5_pmode_cur)
		g5_switch_volt(speed_mode);

	g5_pmode_cur = speed_mode;
	ppc_proc_freq = g5_cpu_freqs[speed_mode].frequency * 1000ul;

	return 0;
}

static int g5_scom_query_freq(void)
{
	unsigned long psr = scom970_read(SCOM_PSR);
	int i;

	for (i = 0; i <= g5_pmode_max; i++)
		if ((((psr >> PSR_CUR_SPEED_SHIFT) ^
		      (g5_pmode_data[i] >> PCR_SPEED_SHIFT)) & 0x3) == 0)
			break;
	return i;
}


static void g5_dummy_switch_volt(int speed_mode)
{
}

#endif 


static struct pmf_function *pfunc_cpu0_volt_high;
static struct pmf_function *pfunc_cpu0_volt_low;
static struct pmf_function *pfunc_cpu1_volt_high;
static struct pmf_function *pfunc_cpu1_volt_low;

static void g5_pfunc_switch_volt(int speed_mode)
{
	if (speed_mode == CPUFREQ_HIGH) {
		if (pfunc_cpu0_volt_high)
			pmf_call_one(pfunc_cpu0_volt_high, NULL);
		if (pfunc_cpu1_volt_high)
			pmf_call_one(pfunc_cpu1_volt_high, NULL);
	} else {
		if (pfunc_cpu0_volt_low)
			pmf_call_one(pfunc_cpu0_volt_low, NULL);
		if (pfunc_cpu1_volt_low)
			pmf_call_one(pfunc_cpu1_volt_low, NULL);
	}
	msleep(10); 
}


static struct pmf_function *pfunc_cpu_setfreq_high;
static struct pmf_function *pfunc_cpu_setfreq_low;
static struct pmf_function *pfunc_cpu_getfreq;
static struct pmf_function *pfunc_slewing_done;

static int g5_pfunc_switch_freq(int speed_mode)
{
	struct pmf_args args;
	u32 done = 0;
	unsigned long timeout;
	int rc;

	DBG("g5_pfunc_switch_freq(%d)\n", speed_mode);

	
	if (speed_mode < g5_pmode_cur)
		g5_switch_volt(speed_mode);

	
	if (speed_mode == CPUFREQ_HIGH)
		rc = pmf_call_one(pfunc_cpu_setfreq_high, NULL);
	else
		rc = pmf_call_one(pfunc_cpu_setfreq_low, NULL);

	if (rc)
		printk(KERN_WARNING "cpufreq: pfunc switch error %d\n", rc);

	timeout = jiffies + HZ/10;
	while(!time_after(jiffies, timeout)) {
		args.count = 1;
		args.u[0].p = &done;
		pmf_call_one(pfunc_slewing_done, &args);
		if (done)
			break;
		msleep(1);
	}
	if (done == 0)
		printk(KERN_WARNING "cpufreq: Timeout in clock slewing !\n");

	
	if (speed_mode > g5_pmode_cur)
		g5_switch_volt(speed_mode);

	g5_pmode_cur = speed_mode;
	ppc_proc_freq = g5_cpu_freqs[speed_mode].frequency * 1000ul;

	return 0;
}

static int g5_pfunc_query_freq(void)
{
	struct pmf_args args;
	u32 val = 0;

	args.count = 1;
	args.u[0].p = &val;
	pmf_call_one(pfunc_cpu_getfreq, &args);
	return val ? CPUFREQ_HIGH : CPUFREQ_LOW;
}



static int g5_cpufreq_verify(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy, g5_cpu_freqs);
}

static int g5_cpufreq_target(struct cpufreq_policy *policy,
	unsigned int target_freq, unsigned int relation)
{
	unsigned int newstate = 0;
	struct cpufreq_freqs freqs;
	int rc;

	if (cpufreq_frequency_table_target(policy, g5_cpu_freqs,
			target_freq, relation, &newstate))
		return -EINVAL;

	if (g5_pmode_cur == newstate)
		return 0;

	mutex_lock(&g5_switch_mutex);

	freqs.old = g5_cpu_freqs[g5_pmode_cur].frequency;
	freqs.new = g5_cpu_freqs[newstate].frequency;
	freqs.cpu = 0;

	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);
	rc = g5_switch_freq(newstate);
	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	mutex_unlock(&g5_switch_mutex);

	return rc;
}

static unsigned int g5_cpufreq_get_speed(unsigned int cpu)
{
	return g5_cpu_freqs[g5_pmode_cur].frequency;
}

static int g5_cpufreq_cpu_init(struct cpufreq_policy *policy)
{
	policy->cpuinfo.transition_latency = transition_latency;
	policy->cur = g5_cpu_freqs[g5_query_freq()].frequency;
	cpumask_copy(policy->cpus, cpu_online_mask);
	cpufreq_frequency_table_get_attr(g5_cpu_freqs, policy->cpu);

	return cpufreq_frequency_table_cpuinfo(policy,
		g5_cpu_freqs);
}


static struct cpufreq_driver g5_cpufreq_driver = {
	.name		= "powermac",
	.owner		= THIS_MODULE,
	.flags		= CPUFREQ_CONST_LOOPS,
	.init		= g5_cpufreq_cpu_init,
	.verify		= g5_cpufreq_verify,
	.target		= g5_cpufreq_target,
	.get		= g5_cpufreq_get_speed,
	.attr 		= g5_cpu_freqs_attr,
};


#ifdef CONFIG_PMAC_SMU

static int __init g5_neo2_cpufreq_init(struct device_node *cpus)
{
	struct device_node *cpunode;
	unsigned int psize, ssize;
	unsigned long max_freq;
	char *freq_method, *volt_method;
	const u32 *valp;
	u32 pvr_hi;
	int use_volts_vdnap = 0;
	int use_volts_smu = 0;
	int rc = -ENODEV;

	
	if (of_machine_is_compatible("PowerMac8,1") ||
	    of_machine_is_compatible("PowerMac8,2") ||
	    of_machine_is_compatible("PowerMac9,1"))
		use_volts_smu = 1;
	else if (of_machine_is_compatible("PowerMac11,2"))
		use_volts_vdnap = 1;
	else
		return -ENODEV;

	
	for (cpunode = NULL;
	     (cpunode = of_get_next_child(cpus, cpunode)) != NULL;) {
		const u32 *reg = of_get_property(cpunode, "reg", NULL);
		if (reg == NULL || (*reg) != 0)
			continue;
		if (!strcmp(cpunode->type, "cpu"))
			break;
	}
	if (cpunode == NULL) {
		printk(KERN_ERR "cpufreq: Can't find any CPU 0 node\n");
		return -ENODEV;
	}

	
	valp = of_get_property(cpunode, "cpu-version", NULL);
	if (!valp) {
		DBG("No cpu-version property !\n");
		goto bail_noprops;
	}
	pvr_hi = (*valp) >> 16;
	if (pvr_hi != 0x3c && pvr_hi != 0x44) {
		printk(KERN_ERR "cpufreq: Unsupported CPU version\n");
		goto bail_noprops;
	}

	
	g5_pmode_data = of_get_property(cpunode, "power-mode-data",&psize);
	if (!g5_pmode_data) {
		DBG("No power-mode-data !\n");
		goto bail_noprops;
	}
	g5_pmode_max = psize / sizeof(u32) - 1;

	if (use_volts_smu) {
		const struct smu_sdbp_header *shdr;

		
		shdr = smu_get_sdb_partition(SMU_SDB_FVT_ID, NULL);
		if (!shdr)
			goto bail_noprops;
		g5_fvt_table = (struct smu_sdbp_fvt *)&shdr[1];
		ssize = (shdr->len * sizeof(u32)) -
			sizeof(struct smu_sdbp_header);
		g5_fvt_count = ssize / sizeof(struct smu_sdbp_fvt);
		g5_fvt_cur = 0;

		
		if (g5_fvt_count < 1 || g5_pmode_max < 1)
			goto bail_noprops;

		g5_switch_volt = g5_smu_switch_volt;
		volt_method = "SMU";
	} else if (use_volts_vdnap) {
		struct device_node *root;

		root = of_find_node_by_path("/");
		if (root == NULL) {
			printk(KERN_ERR "cpufreq: Can't find root of "
			       "device tree\n");
			goto bail_noprops;
		}
		pfunc_set_vdnap0 = pmf_find_function(root, "set-vdnap0");
		pfunc_vdnap0_complete =
			pmf_find_function(root, "slewing-done");
		if (pfunc_set_vdnap0 == NULL ||
		    pfunc_vdnap0_complete == NULL) {
			printk(KERN_ERR "cpufreq: Can't find required "
			       "platform function\n");
			goto bail_noprops;
		}

		g5_switch_volt = g5_vdnap_switch_volt;
		volt_method = "GPIO";
	} else {
		g5_switch_volt = g5_dummy_switch_volt;
		volt_method = "none";
	}

	valp = of_get_property(cpunode, "clock-frequency", NULL);
	if (!valp)
		return -ENODEV;
	max_freq = (*valp)/1000;
	g5_cpu_freqs[0].frequency = max_freq;
	g5_cpu_freqs[1].frequency = max_freq/2;

	
	transition_latency = 12000;
	g5_switch_freq = g5_scom_switch_freq;
	g5_query_freq = g5_scom_query_freq;
	freq_method = "SCOM";

	g5_switch_volt(CPUFREQ_HIGH);
	msleep(10);
	g5_pmode_cur = -1;
	g5_switch_freq(g5_query_freq());

	printk(KERN_INFO "Registering G5 CPU frequency driver\n");
	printk(KERN_INFO "Frequency method: %s, Voltage method: %s\n",
	       freq_method, volt_method);
	printk(KERN_INFO "Low: %d Mhz, High: %d Mhz, Cur: %d MHz\n",
		g5_cpu_freqs[1].frequency/1000,
		g5_cpu_freqs[0].frequency/1000,
		g5_cpu_freqs[g5_pmode_cur].frequency/1000);

	rc = cpufreq_register_driver(&g5_cpufreq_driver);

	return rc;

 bail_noprops:
	of_node_put(cpunode);

	return rc;
}

#endif 


static int __init g5_pm72_cpufreq_init(struct device_node *cpus)
{
	struct device_node *cpuid = NULL, *hwclock = NULL, *cpunode = NULL;
	const u8 *eeprom = NULL;
	const u32 *valp;
	u64 max_freq, min_freq, ih, il;
	int has_volt = 1, rc = 0;

	DBG("cpufreq: Initializing for PowerMac7,2, PowerMac7,3 and"
	    " RackMac3,1...\n");

	
	for (cpunode = NULL;
	     (cpunode = of_get_next_child(cpus, cpunode)) != NULL;) {
		if (!strcmp(cpunode->type, "cpu"))
			break;
	}
	if (cpunode == NULL) {
		printk(KERN_ERR "cpufreq: Can't find any CPU node\n");
		return -ENODEV;
	}

	
        cpuid = of_find_node_by_path("/u3@0,f8000000/i2c@f8001000/cpuid@a0");
	if (cpuid != NULL)
		eeprom = of_get_property(cpuid, "cpuid", NULL);
	if (eeprom == NULL) {
		printk(KERN_ERR "cpufreq: Can't find cpuid EEPROM !\n");
		rc = -ENODEV;
		goto bail;
	}

	
	for (hwclock = NULL;
	     (hwclock = of_find_node_by_name(hwclock, "i2c-hwclock")) != NULL;){
		const char *loc = of_get_property(hwclock,
				"hwctrl-location", NULL);
		if (loc == NULL)
			continue;
		if (strcmp(loc, "CPU CLOCK"))
			continue;
		if (!of_get_property(hwclock, "platform-get-frequency", NULL))
			continue;
		break;
	}
	if (hwclock == NULL) {
		printk(KERN_ERR "cpufreq: Can't find i2c clock chip !\n");
		rc = -ENODEV;
		goto bail;
	}

	DBG("cpufreq: i2c clock chip found: %s\n", hwclock->full_name);

	
	pfunc_cpu_getfreq =
		pmf_find_function(hwclock, "get-frequency");
	pfunc_cpu_setfreq_high =
		pmf_find_function(hwclock, "set-frequency-high");
	pfunc_cpu_setfreq_low =
		pmf_find_function(hwclock, "set-frequency-low");
	pfunc_slewing_done =
		pmf_find_function(hwclock, "slewing-done");
	pfunc_cpu0_volt_high =
		pmf_find_function(hwclock, "set-voltage-high-0");
	pfunc_cpu0_volt_low =
		pmf_find_function(hwclock, "set-voltage-low-0");
	pfunc_cpu1_volt_high =
		pmf_find_function(hwclock, "set-voltage-high-1");
	pfunc_cpu1_volt_low =
		pmf_find_function(hwclock, "set-voltage-low-1");

	
	if (pfunc_cpu_getfreq == NULL || pfunc_cpu_setfreq_high == NULL ||
	    pfunc_cpu_setfreq_low == NULL || pfunc_slewing_done == NULL) {
		printk(KERN_ERR "cpufreq: Can't find platform functions !\n");
		rc = -ENODEV;
		goto bail;
	}

	
	if (pfunc_cpu0_volt_high == NULL || pfunc_cpu0_volt_low == NULL) {
		pmf_put_function(pfunc_cpu0_volt_high);
		pmf_put_function(pfunc_cpu0_volt_low);
		pfunc_cpu0_volt_high = pfunc_cpu0_volt_low = NULL;
		has_volt = 0;
	}
	if (!has_volt ||
	    pfunc_cpu1_volt_high == NULL || pfunc_cpu1_volt_low == NULL) {
		pmf_put_function(pfunc_cpu1_volt_high);
		pmf_put_function(pfunc_cpu1_volt_low);
		pfunc_cpu1_volt_high = pfunc_cpu1_volt_low = NULL;
	}


	
	valp = of_get_property(cpunode, "clock-frequency", NULL);
	if (!valp) {
		printk(KERN_ERR "cpufreq: Can't find CPU frequency !\n");
		rc = -ENODEV;
		goto bail;
	}

	max_freq = (*valp)/1000;

	ih = *((u32 *)(eeprom + 0x10));
	il = *((u32 *)(eeprom + 0x20));

	
	if (il == ih) {
		printk(KERN_WARNING "cpufreq: No low frequency mode available"
		       " on this model !\n");
		rc = -ENODEV;
		goto bail;
	}

	min_freq = 0;
	if (ih != 0 && il != 0)
		min_freq = (max_freq * il) / ih;

	
	if (min_freq >= max_freq || min_freq < 1000) {
		printk(KERN_ERR "cpufreq: Can't calculate low frequency !\n");
		rc = -ENXIO;
		goto bail;
	}
	g5_cpu_freqs[0].frequency = max_freq;
	g5_cpu_freqs[1].frequency = min_freq;

	
	transition_latency = CPUFREQ_ETERNAL;
	g5_switch_volt = g5_pfunc_switch_volt;
	g5_switch_freq = g5_pfunc_switch_freq;
	g5_query_freq = g5_pfunc_query_freq;

	g5_switch_volt(CPUFREQ_HIGH);
	msleep(10);
	g5_pmode_cur = -1;
	g5_switch_freq(g5_query_freq());

	printk(KERN_INFO "Registering G5 CPU frequency driver\n");
	printk(KERN_INFO "Frequency method: i2c/pfunc, "
	       "Voltage method: %s\n", has_volt ? "i2c/pfunc" : "none");
	printk(KERN_INFO "Low: %d Mhz, High: %d Mhz, Cur: %d MHz\n",
		g5_cpu_freqs[1].frequency/1000,
		g5_cpu_freqs[0].frequency/1000,
		g5_cpu_freqs[g5_pmode_cur].frequency/1000);

	rc = cpufreq_register_driver(&g5_cpufreq_driver);
 bail:
	if (rc != 0) {
		pmf_put_function(pfunc_cpu_getfreq);
		pmf_put_function(pfunc_cpu_setfreq_high);
		pmf_put_function(pfunc_cpu_setfreq_low);
		pmf_put_function(pfunc_slewing_done);
		pmf_put_function(pfunc_cpu0_volt_high);
		pmf_put_function(pfunc_cpu0_volt_low);
		pmf_put_function(pfunc_cpu1_volt_high);
		pmf_put_function(pfunc_cpu1_volt_low);
	}
	of_node_put(hwclock);
	of_node_put(cpuid);
	of_node_put(cpunode);

	return rc;
}

static int __init g5_cpufreq_init(void)
{
	struct device_node *cpus;
	int rc = 0;

	cpus = of_find_node_by_path("/cpus");
	if (cpus == NULL) {
		DBG("No /cpus node !\n");
		return -ENODEV;
	}

	if (of_machine_is_compatible("PowerMac7,2") ||
	    of_machine_is_compatible("PowerMac7,3") ||
	    of_machine_is_compatible("RackMac3,1"))
		rc = g5_pm72_cpufreq_init(cpus);
#ifdef CONFIG_PMAC_SMU
	else
		rc = g5_neo2_cpufreq_init(cpus);
#endif 

	of_node_put(cpus);
	return rc;
}

module_init(g5_cpufreq_init);


MODULE_LICENSE("GPL");
