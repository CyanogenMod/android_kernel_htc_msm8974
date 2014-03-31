#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/acpi_pmtmr.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/clocksource.h>
#include <linux/percpu.h>
#include <linux/timex.h>

#include <asm/hpet.h>
#include <asm/timer.h>
#include <asm/vgtod.h>
#include <asm/time.h>
#include <asm/delay.h>
#include <asm/hypervisor.h>
#include <asm/nmi.h>
#include <asm/x86_init.h>

unsigned int __read_mostly cpu_khz;	
EXPORT_SYMBOL(cpu_khz);

unsigned int __read_mostly tsc_khz;
EXPORT_SYMBOL(tsc_khz);

static int __read_mostly tsc_unstable;

static int __read_mostly tsc_disabled = -1;

int tsc_clocksource_reliable;
u64 native_sched_clock(void)
{
	u64 this_offset;

	if (unlikely(tsc_disabled)) {
		
		return (jiffies_64 - INITIAL_JIFFIES) * (1000000000 / HZ);
	}

	
	rdtscll(this_offset);

	
	return __cycles_2_ns(this_offset);
}

#ifdef CONFIG_PARAVIRT
unsigned long long sched_clock(void)
{
	return paravirt_sched_clock();
}
#else
unsigned long long
sched_clock(void) __attribute__((alias("native_sched_clock")));
#endif

int check_tsc_unstable(void)
{
	return tsc_unstable;
}
EXPORT_SYMBOL_GPL(check_tsc_unstable);

#ifdef CONFIG_X86_TSC
int __init notsc_setup(char *str)
{
	printk(KERN_WARNING "notsc: Kernel compiled with CONFIG_X86_TSC, "
			"cannot disable TSC completely.\n");
	tsc_disabled = 1;
	return 1;
}
#else
int __init notsc_setup(char *str)
{
	setup_clear_cpu_cap(X86_FEATURE_TSC);
	return 1;
}
#endif

__setup("notsc", notsc_setup);

static int no_sched_irq_time;

static int __init tsc_setup(char *str)
{
	if (!strcmp(str, "reliable"))
		tsc_clocksource_reliable = 1;
	if (!strncmp(str, "noirqtime", 9))
		no_sched_irq_time = 1;
	return 1;
}

__setup("tsc=", tsc_setup);

#define MAX_RETRIES     5
#define SMI_TRESHOLD    50000

static u64 tsc_read_refs(u64 *p, int hpet)
{
	u64 t1, t2;
	int i;

	for (i = 0; i < MAX_RETRIES; i++) {
		t1 = get_cycles();
		if (hpet)
			*p = hpet_readl(HPET_COUNTER) & 0xFFFFFFFF;
		else
			*p = acpi_pm_read_early();
		t2 = get_cycles();
		if ((t2 - t1) < SMI_TRESHOLD)
			return t2;
	}
	return ULLONG_MAX;
}

static unsigned long calc_hpet_ref(u64 deltatsc, u64 hpet1, u64 hpet2)
{
	u64 tmp;

	if (hpet2 < hpet1)
		hpet2 += 0x100000000ULL;
	hpet2 -= hpet1;
	tmp = ((u64)hpet2 * hpet_readl(HPET_PERIOD));
	do_div(tmp, 1000000);
	do_div(deltatsc, tmp);

	return (unsigned long) deltatsc;
}

static unsigned long calc_pmtimer_ref(u64 deltatsc, u64 pm1, u64 pm2)
{
	u64 tmp;

	if (!pm1 && !pm2)
		return ULONG_MAX;

	if (pm2 < pm1)
		pm2 += (u64)ACPI_PM_OVRRUN;
	pm2 -= pm1;
	tmp = pm2 * 1000000000LL;
	do_div(tmp, PMTMR_TICKS_PER_SEC);
	do_div(deltatsc, tmp);

	return (unsigned long) deltatsc;
}

#define CAL_MS		10
#define CAL_LATCH	(PIT_TICK_RATE / (1000 / CAL_MS))
#define CAL_PIT_LOOPS	1000

#define CAL2_MS		50
#define CAL2_LATCH	(PIT_TICK_RATE / (1000 / CAL2_MS))
#define CAL2_PIT_LOOPS	5000


static unsigned long pit_calibrate_tsc(u32 latch, unsigned long ms, int loopmin)
{
	u64 tsc, t1, t2, delta;
	unsigned long tscmin, tscmax;
	int pitcnt;

	
	outb((inb(0x61) & ~0x02) | 0x01, 0x61);

	outb(0xb0, 0x43);
	outb(latch & 0xff, 0x42);
	outb(latch >> 8, 0x42);

	tsc = t1 = t2 = get_cycles();

	pitcnt = 0;
	tscmax = 0;
	tscmin = ULONG_MAX;
	while ((inb(0x61) & 0x20) == 0) {
		t2 = get_cycles();
		delta = t2 - tsc;
		tsc = t2;
		if ((unsigned long) delta < tscmin)
			tscmin = (unsigned int) delta;
		if ((unsigned long) delta > tscmax)
			tscmax = (unsigned int) delta;
		pitcnt++;
	}

	if (pitcnt < loopmin || tscmax > 10 * tscmin)
		return ULONG_MAX;

	
	delta = t2 - t1;
	do_div(delta, ms);
	return delta;
}

static inline int pit_verify_msb(unsigned char val)
{
	
	inb(0x42);
	return inb(0x42) == val;
}

static inline int pit_expect_msb(unsigned char val, u64 *tscp, unsigned long *deltap)
{
	int count;
	u64 tsc = 0, prev_tsc = 0;

	for (count = 0; count < 50000; count++) {
		if (!pit_verify_msb(val))
			break;
		prev_tsc = tsc;
		tsc = get_cycles();
	}
	*deltap = get_cycles() - prev_tsc;
	*tscp = tsc;

	return count > 5;
}

#define MAX_QUICK_PIT_MS 50
#define MAX_QUICK_PIT_ITERATIONS (MAX_QUICK_PIT_MS * PIT_TICK_RATE / 1000 / 256)

static unsigned long quick_pit_calibrate(void)
{
	int i;
	u64 tsc, delta;
	unsigned long d1, d2;

	
	outb((inb(0x61) & ~0x02) | 0x01, 0x61);

	outb(0xb0, 0x43);

	
	outb(0xff, 0x42);
	outb(0xff, 0x42);

	pit_verify_msb(0);

	if (pit_expect_msb(0xff, &tsc, &d1)) {
		for (i = 1; i <= MAX_QUICK_PIT_ITERATIONS; i++) {
			if (!pit_expect_msb(0xff-i, &delta, &d2))
				break;

			delta -= tsc;
			if (d1+d2 >= delta >> 11)
				continue;

			if (!pit_verify_msb(0xfe - i))
				break;
			goto success;
		}
	}
	printk("Fast TSC calibration failed\n");
	return 0;

success:
	delta *= PIT_TICK_RATE;
	do_div(delta, i*256*1000);
	printk("Fast TSC calibration using PIT\n");
	return delta;
}

unsigned long native_calibrate_tsc(void)
{
	u64 tsc1, tsc2, delta, ref1, ref2;
	unsigned long tsc_pit_min = ULONG_MAX, tsc_ref_min = ULONG_MAX;
	unsigned long flags, latch, ms, fast_calibrate;
	int hpet = is_hpet_enabled(), i, loopmin;

	local_irq_save(flags);
	fast_calibrate = quick_pit_calibrate();
	local_irq_restore(flags);
	if (fast_calibrate)
		return fast_calibrate;


	
	latch = CAL_LATCH;
	ms = CAL_MS;
	loopmin = CAL_PIT_LOOPS;

	for (i = 0; i < 3; i++) {
		unsigned long tsc_pit_khz;

		local_irq_save(flags);
		tsc1 = tsc_read_refs(&ref1, hpet);
		tsc_pit_khz = pit_calibrate_tsc(latch, ms, loopmin);
		tsc2 = tsc_read_refs(&ref2, hpet);
		local_irq_restore(flags);

		
		tsc_pit_min = min(tsc_pit_min, tsc_pit_khz);

		
		if (ref1 == ref2)
			continue;

		
		if (tsc1 == ULLONG_MAX || tsc2 == ULLONG_MAX)
			continue;

		tsc2 = (tsc2 - tsc1) * 1000000LL;
		if (hpet)
			tsc2 = calc_hpet_ref(tsc2, ref1, ref2);
		else
			tsc2 = calc_pmtimer_ref(tsc2, ref1, ref2);

		tsc_ref_min = min(tsc_ref_min, (unsigned long) tsc2);

		
		delta = ((u64) tsc_pit_min) * 100;
		do_div(delta, tsc_ref_min);

		if (delta >= 90 && delta <= 110) {
			printk(KERN_INFO
			       "TSC: PIT calibration matches %s. %d loops\n",
			       hpet ? "HPET" : "PMTIMER", i + 1);
			return tsc_ref_min;
		}

		if (i == 1 && tsc_pit_min == ULONG_MAX) {
			latch = CAL2_LATCH;
			ms = CAL2_MS;
			loopmin = CAL2_PIT_LOOPS;
		}
	}

	if (tsc_pit_min == ULONG_MAX) {
		
		printk(KERN_WARNING "TSC: Unable to calibrate against PIT\n");

		
		if (!hpet && !ref1 && !ref2) {
			printk("TSC: No reference (HPET/PMTIMER) available\n");
			return 0;
		}

		
		if (tsc_ref_min == ULONG_MAX) {
			printk(KERN_WARNING "TSC: HPET/PMTIMER calibration "
			       "failed.\n");
			return 0;
		}

		
		printk(KERN_INFO "TSC: using %s reference calibration\n",
		       hpet ? "HPET" : "PMTIMER");

		return tsc_ref_min;
	}

	
	if (!hpet && !ref1 && !ref2) {
		printk(KERN_INFO "TSC: Using PIT calibration value\n");
		return tsc_pit_min;
	}

	
	if (tsc_ref_min == ULONG_MAX) {
		printk(KERN_WARNING "TSC: HPET/PMTIMER calibration failed. "
		       "Using PIT calibration\n");
		return tsc_pit_min;
	}

	printk(KERN_WARNING "TSC: PIT calibration deviates from %s: %lu %lu.\n",
	       hpet ? "HPET" : "PMTIMER", tsc_pit_min, tsc_ref_min);
	printk(KERN_INFO "TSC: Using PIT calibration value\n");
	return tsc_pit_min;
}

int recalibrate_cpu_khz(void)
{
#ifndef CONFIG_SMP
	unsigned long cpu_khz_old = cpu_khz;

	if (cpu_has_tsc) {
		tsc_khz = x86_platform.calibrate_tsc();
		cpu_khz = tsc_khz;
		cpu_data(0).loops_per_jiffy =
			cpufreq_scale(cpu_data(0).loops_per_jiffy,
					cpu_khz_old, cpu_khz);
		return 0;
	} else
		return -ENODEV;
#else
	return -ENODEV;
#endif
}

EXPORT_SYMBOL(recalibrate_cpu_khz);



DEFINE_PER_CPU(unsigned long, cyc2ns);
DEFINE_PER_CPU(unsigned long long, cyc2ns_offset);

static void set_cyc2ns_scale(unsigned long cpu_khz, int cpu)
{
	unsigned long long tsc_now, ns_now, *offset;
	unsigned long flags, *scale;

	local_irq_save(flags);
	sched_clock_idle_sleep_event();

	scale = &per_cpu(cyc2ns, cpu);
	offset = &per_cpu(cyc2ns_offset, cpu);

	rdtscll(tsc_now);
	ns_now = __cycles_2_ns(tsc_now);

	if (cpu_khz) {
		*scale = (NSEC_PER_MSEC << CYC2NS_SCALE_FACTOR)/cpu_khz;
		*offset = ns_now - mult_frac(tsc_now, *scale,
					     (1UL << CYC2NS_SCALE_FACTOR));
	}

	sched_clock_idle_wakeup_event(0);
	local_irq_restore(flags);
}

static unsigned long long cyc2ns_suspend;

void tsc_save_sched_clock_state(void)
{
	if (!sched_clock_stable)
		return;

	cyc2ns_suspend = sched_clock();
}

void tsc_restore_sched_clock_state(void)
{
	unsigned long long offset;
	unsigned long flags;
	int cpu;

	if (!sched_clock_stable)
		return;

	local_irq_save(flags);

	__this_cpu_write(cyc2ns_offset, 0);
	offset = cyc2ns_suspend - sched_clock();

	for_each_possible_cpu(cpu)
		per_cpu(cyc2ns_offset, cpu) = offset;

	local_irq_restore(flags);
}

#ifdef CONFIG_CPU_FREQ


static unsigned int  ref_freq;
static unsigned long loops_per_jiffy_ref;
static unsigned long tsc_khz_ref;

static int time_cpufreq_notifier(struct notifier_block *nb, unsigned long val,
				void *data)
{
	struct cpufreq_freqs *freq = data;
	unsigned long *lpj;

	if (cpu_has(&cpu_data(freq->cpu), X86_FEATURE_CONSTANT_TSC))
		return 0;

	lpj = &boot_cpu_data.loops_per_jiffy;
#ifdef CONFIG_SMP
	if (!(freq->flags & CPUFREQ_CONST_LOOPS))
		lpj = &cpu_data(freq->cpu).loops_per_jiffy;
#endif

	if (!ref_freq) {
		ref_freq = freq->old;
		loops_per_jiffy_ref = *lpj;
		tsc_khz_ref = tsc_khz;
	}
	if ((val == CPUFREQ_PRECHANGE  && freq->old < freq->new) ||
			(val == CPUFREQ_POSTCHANGE && freq->old > freq->new) ||
			(val == CPUFREQ_RESUMECHANGE)) {
		*lpj = cpufreq_scale(loops_per_jiffy_ref, ref_freq, freq->new);

		tsc_khz = cpufreq_scale(tsc_khz_ref, ref_freq, freq->new);
		if (!(freq->flags & CPUFREQ_CONST_LOOPS))
			mark_tsc_unstable("cpufreq changes");
	}

	set_cyc2ns_scale(tsc_khz, freq->cpu);

	return 0;
}

static struct notifier_block time_cpufreq_notifier_block = {
	.notifier_call  = time_cpufreq_notifier
};

static int __init cpufreq_tsc(void)
{
	if (!cpu_has_tsc)
		return 0;
	if (boot_cpu_has(X86_FEATURE_CONSTANT_TSC))
		return 0;
	cpufreq_register_notifier(&time_cpufreq_notifier_block,
				CPUFREQ_TRANSITION_NOTIFIER);
	return 0;
}

core_initcall(cpufreq_tsc);

#endif 


static struct clocksource clocksource_tsc;

static cycle_t read_tsc(struct clocksource *cs)
{
	cycle_t ret = (cycle_t)get_cycles();

	return ret >= clocksource_tsc.cycle_last ?
		ret : clocksource_tsc.cycle_last;
}

static void resume_tsc(struct clocksource *cs)
{
	clocksource_tsc.cycle_last = 0;
}

static struct clocksource clocksource_tsc = {
	.name                   = "tsc",
	.rating                 = 300,
	.read                   = read_tsc,
	.resume			= resume_tsc,
	.mask                   = CLOCKSOURCE_MASK(64),
	.flags                  = CLOCK_SOURCE_IS_CONTINUOUS |
				  CLOCK_SOURCE_MUST_VERIFY,
#ifdef CONFIG_X86_64
	.archdata               = { .vclock_mode = VCLOCK_TSC },
#endif
};

void mark_tsc_unstable(char *reason)
{
	if (!tsc_unstable) {
		tsc_unstable = 1;
		sched_clock_stable = 0;
		disable_sched_clock_irqtime();
		printk(KERN_INFO "Marking TSC unstable due to %s\n", reason);
		
		if (clocksource_tsc.mult)
			clocksource_mark_unstable(&clocksource_tsc);
		else {
			clocksource_tsc.flags |= CLOCK_SOURCE_UNSTABLE;
			clocksource_tsc.rating = 0;
		}
	}
}

EXPORT_SYMBOL_GPL(mark_tsc_unstable);

static void __init check_system_tsc_reliable(void)
{
#ifdef CONFIG_MGEODE_LX
	
#define RTSC_SUSP 0x100
	unsigned long res_low, res_high;

	rdmsr_safe(MSR_GEODE_BUSCONT_CONF0, &res_low, &res_high);
	
	if (res_low & RTSC_SUSP)
		tsc_clocksource_reliable = 1;
#endif
	if (boot_cpu_has(X86_FEATURE_TSC_RELIABLE))
		tsc_clocksource_reliable = 1;
}

__cpuinit int unsynchronized_tsc(void)
{
	if (!cpu_has_tsc || tsc_unstable)
		return 1;

#ifdef CONFIG_SMP
	if (apic_is_clustered_box())
		return 1;
#endif

	if (boot_cpu_has(X86_FEATURE_CONSTANT_TSC))
		return 0;

	if (tsc_clocksource_reliable)
		return 0;
	if (boot_cpu_data.x86_vendor != X86_VENDOR_INTEL) {
		
		if (num_possible_cpus() > 1)
			return 1;
	}

	return 0;
}


static void tsc_refine_calibration_work(struct work_struct *work);
static DECLARE_DELAYED_WORK(tsc_irqwork, tsc_refine_calibration_work);
static void tsc_refine_calibration_work(struct work_struct *work)
{
	static u64 tsc_start = -1, ref_start;
	static int hpet;
	u64 tsc_stop, ref_stop, delta;
	unsigned long freq;

	
	if (check_tsc_unstable())
		goto out;

	if (tsc_start == -1) {
		hpet = is_hpet_enabled();
		schedule_delayed_work(&tsc_irqwork, HZ);
		tsc_start = tsc_read_refs(&ref_start, hpet);
		return;
	}

	tsc_stop = tsc_read_refs(&ref_stop, hpet);

	
	if (ref_start == ref_stop)
		goto out;

	
	if (tsc_start == ULLONG_MAX || tsc_stop == ULLONG_MAX)
		goto out;

	delta = tsc_stop - tsc_start;
	delta *= 1000000LL;
	if (hpet)
		freq = calc_hpet_ref(delta, ref_start, ref_stop);
	else
		freq = calc_pmtimer_ref(delta, ref_start, ref_stop);

	
	if (abs(tsc_khz - freq) > tsc_khz/100)
		goto out;

	tsc_khz = freq;
	printk(KERN_INFO "Refined TSC clocksource calibration: "
		"%lu.%03lu MHz.\n", (unsigned long)tsc_khz / 1000,
					(unsigned long)tsc_khz % 1000);

out:
	clocksource_register_khz(&clocksource_tsc, tsc_khz);
}


static int __init init_tsc_clocksource(void)
{
	if (!cpu_has_tsc || tsc_disabled > 0 || !tsc_khz)
		return 0;

	if (tsc_clocksource_reliable)
		clocksource_tsc.flags &= ~CLOCK_SOURCE_MUST_VERIFY;
	
	if (check_tsc_unstable()) {
		clocksource_tsc.rating = 0;
		clocksource_tsc.flags &= ~CLOCK_SOURCE_IS_CONTINUOUS;
	}

	if (boot_cpu_has(X86_FEATURE_TSC_RELIABLE)) {
		clocksource_register_khz(&clocksource_tsc, tsc_khz);
		return 0;
	}

	schedule_delayed_work(&tsc_irqwork, 0);
	return 0;
}
device_initcall(init_tsc_clocksource);

void __init tsc_init(void)
{
	u64 lpj;
	int cpu;

	x86_init.timers.tsc_pre_init();

	if (!cpu_has_tsc)
		return;

	tsc_khz = x86_platform.calibrate_tsc();
	cpu_khz = tsc_khz;

	if (!tsc_khz) {
		mark_tsc_unstable("could not calculate TSC khz");
		return;
	}

	printk("Detected %lu.%03lu MHz processor.\n",
			(unsigned long)cpu_khz / 1000,
			(unsigned long)cpu_khz % 1000);

	for_each_possible_cpu(cpu)
		set_cyc2ns_scale(cpu_khz, cpu);

	if (tsc_disabled > 0)
		return;

	
	tsc_disabled = 0;

	if (!no_sched_irq_time)
		enable_sched_clock_irqtime();

	lpj = ((u64)tsc_khz * 1000);
	do_div(lpj, HZ);
	lpj_fine = lpj;

	use_tsc_delay();

	if (unsynchronized_tsc())
		mark_tsc_unstable("TSCs unsynchronized");

	check_system_tsc_reliable();
}

#ifdef CONFIG_SMP
unsigned long __cpuinit calibrate_delay_is_known(void)
{
	int i, cpu = smp_processor_id();

	if (!tsc_disabled && !cpu_has(&cpu_data(cpu), X86_FEATURE_CONSTANT_TSC))
		return 0;

	for_each_online_cpu(i)
		if (cpu_data(i).phys_proc_id == cpu_data(cpu).phys_proc_id)
			return cpu_data(i).loops_per_jiffy;
	return 0;
}
#endif
