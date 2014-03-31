#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/percpu.h>
#include <linux/export.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/cpu.h>

#include <asm/processor.h>
#include <asm/apic.h>
#include <asm/idle.h>
#include <asm/mce.h>
#include <asm/msr.h>

#define CHECK_INTERVAL		(300 * HZ)

#define THERMAL_THROTTLING_EVENT	0
#define POWER_LIMIT_EVENT		1

struct _thermal_state {
	bool			new_event;
	int			event;
	u64			next_check;
	unsigned long		count;
	unsigned long		last_count;
};

struct thermal_state {
	struct _thermal_state core_throttle;
	struct _thermal_state core_power_limit;
	struct _thermal_state package_throttle;
	struct _thermal_state package_power_limit;
	struct _thermal_state core_thresh0;
	struct _thermal_state core_thresh1;
};

int (*platform_thermal_notify)(__u64 msr_val);
EXPORT_SYMBOL(platform_thermal_notify);

static DEFINE_PER_CPU(struct thermal_state, thermal_state);

static atomic_t therm_throt_en	= ATOMIC_INIT(0);

static u32 lvtthmr_init __read_mostly;

#ifdef CONFIG_SYSFS
#define define_therm_throt_device_one_ro(_name)				\
	static DEVICE_ATTR(_name, 0444,					\
			   therm_throt_device_show_##_name,		\
				   NULL)				\

#define define_therm_throt_device_show_func(event, name)		\
									\
static ssize_t therm_throt_device_show_##event##_##name(		\
			struct device *dev,				\
			struct device_attribute *attr,			\
			char *buf)					\
{									\
	unsigned int cpu = dev->id;					\
	ssize_t ret;							\
									\
	preempt_disable();				\
	if (cpu_online(cpu)) {						\
		ret = sprintf(buf, "%lu\n",				\
			      per_cpu(thermal_state, cpu).event.name);	\
	} else								\
		ret = 0;						\
	preempt_enable();						\
									\
	return ret;							\
}

define_therm_throt_device_show_func(core_throttle, count);
define_therm_throt_device_one_ro(core_throttle_count);

define_therm_throt_device_show_func(core_power_limit, count);
define_therm_throt_device_one_ro(core_power_limit_count);

define_therm_throt_device_show_func(package_throttle, count);
define_therm_throt_device_one_ro(package_throttle_count);

define_therm_throt_device_show_func(package_power_limit, count);
define_therm_throt_device_one_ro(package_power_limit_count);

static struct attribute *thermal_throttle_attrs[] = {
	&dev_attr_core_throttle_count.attr,
	NULL
};

static struct attribute_group thermal_attr_group = {
	.attrs	= thermal_throttle_attrs,
	.name	= "thermal_throttle"
};
#endif 

#define CORE_LEVEL	0
#define PACKAGE_LEVEL	1

static int therm_throt_process(bool new_event, int event, int level)
{
	struct _thermal_state *state;
	unsigned int this_cpu = smp_processor_id();
	bool old_event;
	u64 now;
	struct thermal_state *pstate = &per_cpu(thermal_state, this_cpu);

	now = get_jiffies_64();
	if (level == CORE_LEVEL) {
		if (event == THERMAL_THROTTLING_EVENT)
			state = &pstate->core_throttle;
		else if (event == POWER_LIMIT_EVENT)
			state = &pstate->core_power_limit;
		else
			 return 0;
	} else if (level == PACKAGE_LEVEL) {
		if (event == THERMAL_THROTTLING_EVENT)
			state = &pstate->package_throttle;
		else if (event == POWER_LIMIT_EVENT)
			state = &pstate->package_power_limit;
		else
			return 0;
	} else
		return 0;

	old_event = state->new_event;
	state->new_event = new_event;

	if (new_event)
		state->count++;

	if (time_before64(now, state->next_check) &&
			state->count != state->last_count)
		return 0;

	state->next_check = now + CHECK_INTERVAL;
	state->last_count = state->count;

	
	if (new_event) {
		if (event == THERMAL_THROTTLING_EVENT)
			printk(KERN_CRIT "CPU%d: %s temperature above threshold, cpu clock throttled (total events = %lu)\n",
				this_cpu,
				level == CORE_LEVEL ? "Core" : "Package",
				state->count);
		else
			printk(KERN_CRIT "CPU%d: %s power limit notification (total events = %lu)\n",
				this_cpu,
				level == CORE_LEVEL ? "Core" : "Package",
				state->count);
		return 1;
	}
	if (old_event) {
		if (event == THERMAL_THROTTLING_EVENT)
			printk(KERN_INFO "CPU%d: %s temperature/speed normal\n",
				this_cpu,
				level == CORE_LEVEL ? "Core" : "Package");
		else
			printk(KERN_INFO "CPU%d: %s power limit normal\n",
				this_cpu,
				level == CORE_LEVEL ? "Core" : "Package");
		return 1;
	}

	return 0;
}

static int thresh_event_valid(int event)
{
	struct _thermal_state *state;
	unsigned int this_cpu = smp_processor_id();
	struct thermal_state *pstate = &per_cpu(thermal_state, this_cpu);
	u64 now = get_jiffies_64();

	state = (event == 0) ? &pstate->core_thresh0 : &pstate->core_thresh1;

	if (time_before64(now, state->next_check))
		return 0;

	state->next_check = now + CHECK_INTERVAL;
	return 1;
}

#ifdef CONFIG_SYSFS
static __cpuinit int thermal_throttle_add_dev(struct device *dev,
				unsigned int cpu)
{
	int err;
	struct cpuinfo_x86 *c = &cpu_data(cpu);

	err = sysfs_create_group(&dev->kobj, &thermal_attr_group);
	if (err)
		return err;

	if (cpu_has(c, X86_FEATURE_PLN))
		err = sysfs_add_file_to_group(&dev->kobj,
					      &dev_attr_core_power_limit_count.attr,
					      thermal_attr_group.name);
	if (cpu_has(c, X86_FEATURE_PTS)) {
		err = sysfs_add_file_to_group(&dev->kobj,
					      &dev_attr_package_throttle_count.attr,
					      thermal_attr_group.name);
		if (cpu_has(c, X86_FEATURE_PLN))
			err = sysfs_add_file_to_group(&dev->kobj,
					&dev_attr_package_power_limit_count.attr,
					thermal_attr_group.name);
	}

	return err;
}

static __cpuinit void thermal_throttle_remove_dev(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &thermal_attr_group);
}

static DEFINE_MUTEX(therm_cpu_lock);

static __cpuinit int
thermal_throttle_cpu_callback(struct notifier_block *nfb,
			      unsigned long action,
			      void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	struct device *dev;
	int err = 0;

	dev = get_cpu_device(cpu);

	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		mutex_lock(&therm_cpu_lock);
		err = thermal_throttle_add_dev(dev, cpu);
		mutex_unlock(&therm_cpu_lock);
		WARN_ON(err);
		break;
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		mutex_lock(&therm_cpu_lock);
		thermal_throttle_remove_dev(dev);
		mutex_unlock(&therm_cpu_lock);
		break;
	}
	return notifier_from_errno(err);
}

static struct notifier_block thermal_throttle_cpu_notifier __cpuinitdata =
{
	.notifier_call = thermal_throttle_cpu_callback,
};

static __init int thermal_throttle_init_device(void)
{
	unsigned int cpu = 0;
	int err;

	if (!atomic_read(&therm_throt_en))
		return 0;

	register_hotcpu_notifier(&thermal_throttle_cpu_notifier);

#ifdef CONFIG_HOTPLUG_CPU
	mutex_lock(&therm_cpu_lock);
#endif
	
	for_each_online_cpu(cpu) {
		err = thermal_throttle_add_dev(get_cpu_device(cpu), cpu);
		WARN_ON(err);
	}
#ifdef CONFIG_HOTPLUG_CPU
	mutex_unlock(&therm_cpu_lock);
#endif

	return 0;
}
device_initcall(thermal_throttle_init_device);

#endif 

static void notify_thresholds(__u64 msr_val)
{
	if (!platform_thermal_notify)
		return;

	
	if ((msr_val & THERM_LOG_THRESHOLD0) &&	thresh_event_valid(0))
		platform_thermal_notify(msr_val);
	
	if ((msr_val & THERM_LOG_THRESHOLD1) && thresh_event_valid(1))
		platform_thermal_notify(msr_val);
}

static void intel_thermal_interrupt(void)
{
	__u64 msr_val;

	rdmsrl(MSR_IA32_THERM_STATUS, msr_val);

	
	notify_thresholds(msr_val);

	if (therm_throt_process(msr_val & THERM_STATUS_PROCHOT,
				THERMAL_THROTTLING_EVENT,
				CORE_LEVEL) != 0)
		mce_log_therm_throt_event(msr_val);

	if (this_cpu_has(X86_FEATURE_PLN))
		therm_throt_process(msr_val & THERM_STATUS_POWER_LIMIT,
					POWER_LIMIT_EVENT,
					CORE_LEVEL);

	if (this_cpu_has(X86_FEATURE_PTS)) {
		rdmsrl(MSR_IA32_PACKAGE_THERM_STATUS, msr_val);
		therm_throt_process(msr_val & PACKAGE_THERM_STATUS_PROCHOT,
					THERMAL_THROTTLING_EVENT,
					PACKAGE_LEVEL);
		if (this_cpu_has(X86_FEATURE_PLN))
			therm_throt_process(msr_val &
					PACKAGE_THERM_STATUS_POWER_LIMIT,
					POWER_LIMIT_EVENT,
					PACKAGE_LEVEL);
	}
}

static void unexpected_thermal_interrupt(void)
{
	printk(KERN_ERR "CPU%d: Unexpected LVT thermal interrupt!\n",
			smp_processor_id());
}

static void (*smp_thermal_vector)(void) = unexpected_thermal_interrupt;

asmlinkage void smp_thermal_interrupt(struct pt_regs *regs)
{
	irq_enter();
	exit_idle();
	inc_irq_stat(irq_thermal_count);
	smp_thermal_vector();
	irq_exit();
	
	ack_APIC_irq();
}

static int intel_thermal_supported(struct cpuinfo_x86 *c)
{
	if (!cpu_has_apic)
		return 0;
	if (!cpu_has(c, X86_FEATURE_ACPI) || !cpu_has(c, X86_FEATURE_ACC))
		return 0;
	return 1;
}

void __init mcheck_intel_therm_init(void)
{
	if (intel_thermal_supported(&boot_cpu_data))
		lvtthmr_init = apic_read(APIC_LVTTHMR);
}

void intel_init_thermal(struct cpuinfo_x86 *c)
{
	unsigned int cpu = smp_processor_id();
	int tm2 = 0;
	u32 l, h;

	if (!intel_thermal_supported(c))
		return;

	rdmsr(MSR_IA32_MISC_ENABLE, l, h);

	h = lvtthmr_init;
	if ((h & APIC_DM_FIXED_MASK) != APIC_DM_FIXED)
		apic_write(APIC_LVTTHMR, lvtthmr_init);


	if ((l & MSR_IA32_MISC_ENABLE_TM1) && (h & APIC_DM_SMI)) {
		printk(KERN_DEBUG
		       "CPU%d: Thermal monitoring handled by SMI\n", cpu);
		return;
	}

	
	if (h & APIC_VECTOR_MASK) {
		printk(KERN_DEBUG
		       "CPU%d: Thermal LVT vector (%#x) already installed\n",
		       cpu, (h & APIC_VECTOR_MASK));
		return;
	}

	
	if (cpu_has(c, X86_FEATURE_TM2)) {
		if (c->x86 == 6 && (c->x86_model == 9 || c->x86_model == 13)) {
			rdmsr(MSR_THERM2_CTL, l, h);
			if (l & MSR_THERM2_CTL_TM_SELECT)
				tm2 = 1;
		} else if (l & MSR_IA32_MISC_ENABLE_TM2)
			tm2 = 1;
	}

	
	h = THERMAL_APIC_VECTOR | APIC_DM_FIXED | APIC_LVT_MASKED;
	apic_write(APIC_LVTTHMR, h);

	rdmsr(MSR_IA32_THERM_INTERRUPT, l, h);
	if (cpu_has(c, X86_FEATURE_PLN))
		wrmsr(MSR_IA32_THERM_INTERRUPT,
		      l | (THERM_INT_LOW_ENABLE
			| THERM_INT_HIGH_ENABLE | THERM_INT_PLN_ENABLE), h);
	else
		wrmsr(MSR_IA32_THERM_INTERRUPT,
		      l | (THERM_INT_LOW_ENABLE | THERM_INT_HIGH_ENABLE), h);

	if (cpu_has(c, X86_FEATURE_PTS)) {
		rdmsr(MSR_IA32_PACKAGE_THERM_INTERRUPT, l, h);
		if (cpu_has(c, X86_FEATURE_PLN))
			wrmsr(MSR_IA32_PACKAGE_THERM_INTERRUPT,
			      l | (PACKAGE_THERM_INT_LOW_ENABLE
				| PACKAGE_THERM_INT_HIGH_ENABLE
				| PACKAGE_THERM_INT_PLN_ENABLE), h);
		else
			wrmsr(MSR_IA32_PACKAGE_THERM_INTERRUPT,
			      l | (PACKAGE_THERM_INT_LOW_ENABLE
				| PACKAGE_THERM_INT_HIGH_ENABLE), h);
	}

	smp_thermal_vector = intel_thermal_interrupt;

	rdmsr(MSR_IA32_MISC_ENABLE, l, h);
	wrmsr(MSR_IA32_MISC_ENABLE, l | MSR_IA32_MISC_ENABLE_TM1, h);

	
	l = apic_read(APIC_LVTTHMR);
	apic_write(APIC_LVTTHMR, l & ~APIC_LVT_MASKED);

	printk_once(KERN_INFO "CPU0: Thermal monitoring enabled (%s)\n",
		       tm2 ? "TM2" : "TM1");

	
	atomic_set(&therm_throt_en, 1);
}
