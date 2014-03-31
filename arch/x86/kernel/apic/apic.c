
#include <linux/perf_event.h>
#include <linux/kernel_stat.h>
#include <linux/mc146818rtc.h>
#include <linux/acpi_pmtmr.h>
#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/bootmem.h>
#include <linux/ftrace.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/syscore_ops.h>
#include <linux/delay.h>
#include <linux/timex.h>
#include <linux/i8253.h>
#include <linux/dmar.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/dmi.h>
#include <linux/smp.h>
#include <linux/mm.h>

#include <asm/perf_event.h>
#include <asm/x86_init.h>
#include <asm/pgalloc.h>
#include <linux/atomic.h>
#include <asm/mpspec.h>
#include <asm/i8259.h>
#include <asm/proto.h>
#include <asm/apic.h>
#include <asm/io_apic.h>
#include <asm/desc.h>
#include <asm/hpet.h>
#include <asm/idle.h>
#include <asm/mtrr.h>
#include <asm/time.h>
#include <asm/smp.h>
#include <asm/mce.h>
#include <asm/tsc.h>
#include <asm/hypervisor.h>

unsigned int num_processors;

unsigned disabled_cpus __cpuinitdata;

unsigned int boot_cpu_physical_apicid = -1U;

unsigned int max_physical_apicid;

physid_mask_t phys_cpu_present_map;

DEFINE_EARLY_PER_CPU(u16, x86_cpu_to_apicid, BAD_APICID);
DEFINE_EARLY_PER_CPU(u16, x86_bios_cpu_apicid, BAD_APICID);
EXPORT_EARLY_PER_CPU_SYMBOL(x86_cpu_to_apicid);
EXPORT_EARLY_PER_CPU_SYMBOL(x86_bios_cpu_apicid);

#ifdef CONFIG_X86_32

DEFINE_EARLY_PER_CPU(int, x86_cpu_to_logical_apicid, BAD_APICID);

static int force_enable_local_apic __initdata;
static int __init parse_lapic(char *arg)
{
	force_enable_local_apic = 1;
	return 0;
}
early_param("lapic", parse_lapic);
static int enabled_via_apicbase;

static inline void imcr_pic_to_apic(void)
{
	
	outb(0x70, 0x22);
	
	outb(0x01, 0x23);
}

static inline void imcr_apic_to_pic(void)
{
	
	outb(0x70, 0x22);
	
	outb(0x00, 0x23);
}
#endif

#ifdef CONFIG_X86_64
static int apic_calibrate_pmtmr __initdata;
static __init int setup_apicpmtimer(char *s)
{
	apic_calibrate_pmtmr = 1;
	notsc_setup(NULL);
	return 0;
}
__setup("apicpmtimer", setup_apicpmtimer);
#endif

int x2apic_mode;
#ifdef CONFIG_X86_X2APIC
int x2apic_preenabled;
static int x2apic_disabled;
static int nox2apic;
static __init int setup_nox2apic(char *str)
{
	if (x2apic_enabled()) {
		int apicid = native_apic_msr_read(APIC_ID);

		if (apicid >= 255) {
			pr_warning("Apicid: %08x, cannot enforce nox2apic\n",
				   apicid);
			return 0;
		}

		pr_warning("x2apic already enabled. will disable it\n");
	} else
		setup_clear_cpu_cap(X86_FEATURE_X2APIC);

	nox2apic = 1;

	return 0;
}
early_param("nox2apic", setup_nox2apic);
#endif

unsigned long mp_lapic_addr;
int disable_apic;
static int disable_apic_timer __initdata;
int local_apic_timer_c2_ok;
EXPORT_SYMBOL_GPL(local_apic_timer_c2_ok);

int first_system_vector = 0xfe;

unsigned int apic_verbosity;

int pic_mode;

int smp_found_config;

static struct resource lapic_resource = {
	.name = "Local APIC",
	.flags = IORESOURCE_MEM | IORESOURCE_BUSY,
};

unsigned int lapic_timer_frequency = 0;

static void apic_pm_activate(void);

static unsigned long apic_phys;

static inline int lapic_get_version(void)
{
	return GET_APIC_VERSION(apic_read(APIC_LVR));
}

static inline int lapic_is_integrated(void)
{
#ifdef CONFIG_X86_64
	return 1;
#else
	return APIC_INTEGRATED(lapic_get_version());
#endif
}

static int modern_apic(void)
{
	
	if (boot_cpu_data.x86_vendor == X86_VENDOR_AMD &&
	    boot_cpu_data.x86 >= 0xf)
		return 1;
	return lapic_get_version() >= 0x14;
}

static void __init apic_disable(void)
{
	pr_info("APIC: switched to apic NOOP\n");
	apic = &apic_noop;
}

void native_apic_wait_icr_idle(void)
{
	while (apic_read(APIC_ICR) & APIC_ICR_BUSY)
		cpu_relax();
}

u32 native_safe_apic_wait_icr_idle(void)
{
	u32 send_status;
	int timeout;

	timeout = 0;
	do {
		send_status = apic_read(APIC_ICR) & APIC_ICR_BUSY;
		if (!send_status)
			break;
		inc_irq_stat(icr_read_retry_count);
		udelay(100);
	} while (timeout++ < 1000);

	return send_status;
}

void native_apic_icr_write(u32 low, u32 id)
{
	apic_write(APIC_ICR2, SET_APIC_DEST_FIELD(id));
	apic_write(APIC_ICR, low);
}

u64 native_apic_icr_read(void)
{
	u32 icr1, icr2;

	icr2 = apic_read(APIC_ICR2);
	icr1 = apic_read(APIC_ICR);

	return icr1 | ((u64)icr2 << 32);
}

#ifdef CONFIG_X86_32
int get_physical_broadcast(void)
{
	return modern_apic() ? 0xff : 0xf;
}
#endif

int lapic_get_maxlvt(void)
{
	unsigned int v;

	v = apic_read(APIC_LVR);
	return APIC_INTEGRATED(GET_APIC_VERSION(v)) ? GET_APIC_MAXLVT(v) : 2;
}


#define APIC_DIVISOR 16

static void __setup_APIC_LVTT(unsigned int clocks, int oneshot, int irqen)
{
	unsigned int lvtt_value, tmp_value;

	lvtt_value = LOCAL_TIMER_VECTOR;
	if (!oneshot)
		lvtt_value |= APIC_LVT_TIMER_PERIODIC;
	if (!lapic_is_integrated())
		lvtt_value |= SET_APIC_TIMER_BASE(APIC_TIMER_BASE_DIV);

	if (!irqen)
		lvtt_value |= APIC_LVT_MASKED;

	apic_write(APIC_LVTT, lvtt_value);

	tmp_value = apic_read(APIC_TDCR);
	apic_write(APIC_TDCR,
		(tmp_value & ~(APIC_TDR_DIV_1 | APIC_TDR_DIV_TMBASE)) |
		APIC_TDR_DIV_16);

	if (!oneshot)
		apic_write(APIC_TMICT, clocks / APIC_DIVISOR);
}


static atomic_t eilvt_offsets[APIC_EILVT_NR_MAX];

static inline int eilvt_entry_is_changeable(unsigned int old, unsigned int new)
{
	return (old & APIC_EILVT_MASKED)
		|| (new == APIC_EILVT_MASKED)
		|| ((new & ~APIC_EILVT_MASKED) == old);
}

static unsigned int reserve_eilvt_offset(int offset, unsigned int new)
{
	unsigned int rsvd, vector;

	if (offset >= APIC_EILVT_NR_MAX)
		return ~0;

	rsvd = atomic_read(&eilvt_offsets[offset]);
	do {
		vector = rsvd & ~APIC_EILVT_MASKED;	
		if (vector && !eilvt_entry_is_changeable(vector, new))
			
			return rsvd;
		rsvd = atomic_cmpxchg(&eilvt_offsets[offset], rsvd, new);
	} while (rsvd != new);

	rsvd &= ~APIC_EILVT_MASKED;
	if (rsvd && rsvd != vector)
		pr_info("LVT offset %d assigned for vector 0x%02x\n",
			offset, rsvd);

	return new;
}


int setup_APIC_eilvt(u8 offset, u8 vector, u8 msg_type, u8 mask)
{
	unsigned long reg = APIC_EILVTn(offset);
	unsigned int new, old, reserved;

	new = (mask << 16) | (msg_type << 8) | vector;
	old = apic_read(reg);
	reserved = reserve_eilvt_offset(offset, new);

	if (reserved != new) {
		pr_err(FW_BUG "cpu %d, try to use APIC%lX (LVT offset %d) for "
		       "vector 0x%x, but the register is already in use for "
		       "vector 0x%x on another cpu\n",
		       smp_processor_id(), reg, offset, new, reserved);
		return -EINVAL;
	}

	if (!eilvt_entry_is_changeable(old, new)) {
		pr_err(FW_BUG "cpu %d, try to use APIC%lX (LVT offset %d) for "
		       "vector 0x%x, but the register is already in use for "
		       "vector 0x%x on this cpu\n",
		       smp_processor_id(), reg, offset, new, old);
		return -EBUSY;
	}

	apic_write(reg, new);

	return 0;
}
EXPORT_SYMBOL_GPL(setup_APIC_eilvt);

static int lapic_next_event(unsigned long delta,
			    struct clock_event_device *evt)
{
	apic_write(APIC_TMICT, delta);
	return 0;
}

static void lapic_timer_setup(enum clock_event_mode mode,
			      struct clock_event_device *evt)
{
	unsigned long flags;
	unsigned int v;

	
	if (evt->features & CLOCK_EVT_FEAT_DUMMY)
		return;

	local_irq_save(flags);

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
	case CLOCK_EVT_MODE_ONESHOT:
		__setup_APIC_LVTT(lapic_timer_frequency,
				  mode != CLOCK_EVT_MODE_PERIODIC, 1);
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		v = apic_read(APIC_LVTT);
		v |= (APIC_LVT_MASKED | LOCAL_TIMER_VECTOR);
		apic_write(APIC_LVTT, v);
		apic_write(APIC_TMICT, 0);
		break;
	case CLOCK_EVT_MODE_RESUME:
		
		break;
	}

	local_irq_restore(flags);
}

static void lapic_timer_broadcast(const struct cpumask *mask)
{
#ifdef CONFIG_SMP
	apic->send_IPI_mask(mask, LOCAL_TIMER_VECTOR);
#endif
}


static struct clock_event_device lapic_clockevent = {
	.name		= "lapic",
	.features	= CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT
			| CLOCK_EVT_FEAT_C3STOP | CLOCK_EVT_FEAT_DUMMY,
	.shift		= 32,
	.set_mode	= lapic_timer_setup,
	.set_next_event	= lapic_next_event,
	.broadcast	= lapic_timer_broadcast,
	.rating		= 100,
	.irq		= -1,
};
static DEFINE_PER_CPU(struct clock_event_device, lapic_events);

static void __cpuinit setup_APIC_timer(void)
{
	struct clock_event_device *levt = &__get_cpu_var(lapic_events);

	if (this_cpu_has(X86_FEATURE_ARAT)) {
		lapic_clockevent.features &= ~CLOCK_EVT_FEAT_C3STOP;
		
		lapic_clockevent.rating = 150;
	}

	memcpy(levt, &lapic_clockevent, sizeof(*levt));
	levt->cpumask = cpumask_of(smp_processor_id());

	clockevents_register_device(levt);
}


#define LAPIC_CAL_LOOPS		(HZ/10)

static __initdata int lapic_cal_loops = -1;
static __initdata long lapic_cal_t1, lapic_cal_t2;
static __initdata unsigned long long lapic_cal_tsc1, lapic_cal_tsc2;
static __initdata unsigned long lapic_cal_pm1, lapic_cal_pm2;
static __initdata unsigned long lapic_cal_j1, lapic_cal_j2;

static void __init lapic_cal_handler(struct clock_event_device *dev)
{
	unsigned long long tsc = 0;
	long tapic = apic_read(APIC_TMCCT);
	unsigned long pm = acpi_pm_read_early();

	if (cpu_has_tsc)
		rdtscll(tsc);

	switch (lapic_cal_loops++) {
	case 0:
		lapic_cal_t1 = tapic;
		lapic_cal_tsc1 = tsc;
		lapic_cal_pm1 = pm;
		lapic_cal_j1 = jiffies;
		break;

	case LAPIC_CAL_LOOPS:
		lapic_cal_t2 = tapic;
		lapic_cal_tsc2 = tsc;
		if (pm < lapic_cal_pm1)
			pm += ACPI_PM_OVRRUN;
		lapic_cal_pm2 = pm;
		lapic_cal_j2 = jiffies;
		break;
	}
}

static int __init
calibrate_by_pmtimer(long deltapm, long *delta, long *deltatsc)
{
	const long pm_100ms = PMTMR_TICKS_PER_SEC / 10;
	const long pm_thresh = pm_100ms / 100;
	unsigned long mult;
	u64 res;

#ifndef CONFIG_X86_PM_TIMER
	return -1;
#endif

	apic_printk(APIC_VERBOSE, "... PM-Timer delta = %ld\n", deltapm);

	
	if (!deltapm)
		return -1;

	mult = clocksource_hz2mult(PMTMR_TICKS_PER_SEC, 22);

	if (deltapm > (pm_100ms - pm_thresh) &&
	    deltapm < (pm_100ms + pm_thresh)) {
		apic_printk(APIC_VERBOSE, "... PM-Timer result ok\n");
		return 0;
	}

	res = (((u64)deltapm) *  mult) >> 22;
	do_div(res, 1000000);
	pr_warning("APIC calibration not consistent "
		   "with PM-Timer: %ldms instead of 100ms\n",(long)res);

	
	res = (((u64)(*delta)) * pm_100ms);
	do_div(res, deltapm);
	pr_info("APIC delta adjusted to PM-Timer: "
		"%lu (%ld)\n", (unsigned long)res, *delta);
	*delta = (long)res;

	
	if (cpu_has_tsc) {
		res = (((u64)(*deltatsc)) * pm_100ms);
		do_div(res, deltapm);
		apic_printk(APIC_VERBOSE, "TSC delta adjusted to "
					  "PM-Timer: %lu (%ld)\n",
					(unsigned long)res, *deltatsc);
		*deltatsc = (long)res;
	}

	return 0;
}

static int __init calibrate_APIC_clock(void)
{
	struct clock_event_device *levt = &__get_cpu_var(lapic_events);
	void (*real_handler)(struct clock_event_device *dev);
	unsigned long deltaj;
	long delta, deltatsc;
	int pm_referenced = 0;


	if (lapic_timer_frequency) {
		apic_printk(APIC_VERBOSE, "lapic timer already calibrated %d\n",
				lapic_timer_frequency);
		lapic_clockevent.mult = div_sc(lapic_timer_frequency/APIC_DIVISOR,
					TICK_NSEC, lapic_clockevent.shift);
		lapic_clockevent.max_delta_ns =
			clockevent_delta2ns(0x7FFFFF, &lapic_clockevent);
		lapic_clockevent.min_delta_ns =
			clockevent_delta2ns(0xF, &lapic_clockevent);
		lapic_clockevent.features &= ~CLOCK_EVT_FEAT_DUMMY;
		return 0;
	}

	local_irq_disable();

	
	real_handler = global_clock_event->event_handler;
	global_clock_event->event_handler = lapic_cal_handler;

	__setup_APIC_LVTT(0xffffffff, 0, 0);

	
	local_irq_enable();

	while (lapic_cal_loops <= LAPIC_CAL_LOOPS)
		cpu_relax();

	local_irq_disable();

	
	global_clock_event->event_handler = real_handler;

	
	delta = lapic_cal_t1 - lapic_cal_t2;
	apic_printk(APIC_VERBOSE, "... lapic delta = %ld\n", delta);

	deltatsc = (long)(lapic_cal_tsc2 - lapic_cal_tsc1);

	
	pm_referenced = !calibrate_by_pmtimer(lapic_cal_pm2 - lapic_cal_pm1,
					&delta, &deltatsc);

	
	lapic_clockevent.mult = div_sc(delta, TICK_NSEC * LAPIC_CAL_LOOPS,
				       lapic_clockevent.shift);
	lapic_clockevent.max_delta_ns =
		clockevent_delta2ns(0x7FFFFFFF, &lapic_clockevent);
	lapic_clockevent.min_delta_ns =
		clockevent_delta2ns(0xF, &lapic_clockevent);

	lapic_timer_frequency = (delta * APIC_DIVISOR) / LAPIC_CAL_LOOPS;

	apic_printk(APIC_VERBOSE, "..... delta %ld\n", delta);
	apic_printk(APIC_VERBOSE, "..... mult: %u\n", lapic_clockevent.mult);
	apic_printk(APIC_VERBOSE, "..... calibration result: %u\n",
		    lapic_timer_frequency);

	if (cpu_has_tsc) {
		apic_printk(APIC_VERBOSE, "..... CPU clock speed is "
			    "%ld.%04ld MHz.\n",
			    (deltatsc / LAPIC_CAL_LOOPS) / (1000000 / HZ),
			    (deltatsc / LAPIC_CAL_LOOPS) % (1000000 / HZ));
	}

	apic_printk(APIC_VERBOSE, "..... host bus clock speed is "
		    "%u.%04u MHz.\n",
		    lapic_timer_frequency / (1000000 / HZ),
		    lapic_timer_frequency % (1000000 / HZ));

	if (lapic_timer_frequency < (1000000 / HZ)) {
		local_irq_enable();
		pr_warning("APIC frequency too slow, disabling apic timer\n");
		return -1;
	}

	levt->features &= ~CLOCK_EVT_FEAT_DUMMY;

	if (!pm_referenced) {
		apic_printk(APIC_VERBOSE, "... verify APIC timer\n");

		levt->event_handler = lapic_cal_handler;
		lapic_timer_setup(CLOCK_EVT_MODE_PERIODIC, levt);
		lapic_cal_loops = -1;

		
		local_irq_enable();

		while (lapic_cal_loops <= LAPIC_CAL_LOOPS)
			cpu_relax();

		
		lapic_timer_setup(CLOCK_EVT_MODE_SHUTDOWN, levt);

		
		deltaj = lapic_cal_j2 - lapic_cal_j1;
		apic_printk(APIC_VERBOSE, "... jiffies delta = %lu\n", deltaj);

		
		if (deltaj >= LAPIC_CAL_LOOPS-2 && deltaj <= LAPIC_CAL_LOOPS+2)
			apic_printk(APIC_VERBOSE, "... jiffies result ok\n");
		else
			levt->features |= CLOCK_EVT_FEAT_DUMMY;
	} else
		local_irq_enable();

	if (levt->features & CLOCK_EVT_FEAT_DUMMY) {
		pr_warning("APIC timer disabled due to verification failure\n");
			return -1;
	}

	return 0;
}

void __init setup_boot_APIC_clock(void)
{
	if (disable_apic_timer) {
		pr_info("Disabling APIC timer\n");
		
		if (num_possible_cpus() > 1) {
			lapic_clockevent.mult = 1;
			setup_APIC_timer();
		}
		return;
	}

	apic_printk(APIC_VERBOSE, "Using local APIC timer interrupts.\n"
		    "calibrating APIC timer ...\n");

	if (calibrate_APIC_clock()) {
		
		if (num_possible_cpus() > 1)
			setup_APIC_timer();
		return;
	}

	lapic_clockevent.features &= ~CLOCK_EVT_FEAT_DUMMY;

	
	setup_APIC_timer();
}

void __cpuinit setup_secondary_APIC_clock(void)
{
	setup_APIC_timer();
}

static void local_apic_timer_interrupt(void)
{
	int cpu = smp_processor_id();
	struct clock_event_device *evt = &per_cpu(lapic_events, cpu);

	if (!evt->event_handler) {
		pr_warning("Spurious LAPIC timer interrupt on cpu %d\n", cpu);
		
		lapic_timer_setup(CLOCK_EVT_MODE_SHUTDOWN, evt);
		return;
	}

	inc_irq_stat(apic_timer_irqs);

	evt->event_handler(evt);
}

void __irq_entry smp_apic_timer_interrupt(struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);

	ack_APIC_irq();
	irq_enter();
	exit_idle();
	local_apic_timer_interrupt();
	irq_exit();

	set_irq_regs(old_regs);
}

int setup_profiling_timer(unsigned int multiplier)
{
	return -EINVAL;
}


void clear_local_APIC(void)
{
	int maxlvt;
	u32 v;

	
	if (!x2apic_mode && !apic_phys)
		return;

	maxlvt = lapic_get_maxlvt();
	if (maxlvt >= 3) {
		v = ERROR_APIC_VECTOR; 
		apic_write(APIC_LVTERR, v | APIC_LVT_MASKED);
	}
	v = apic_read(APIC_LVTT);
	apic_write(APIC_LVTT, v | APIC_LVT_MASKED);
	v = apic_read(APIC_LVT0);
	apic_write(APIC_LVT0, v | APIC_LVT_MASKED);
	v = apic_read(APIC_LVT1);
	apic_write(APIC_LVT1, v | APIC_LVT_MASKED);
	if (maxlvt >= 4) {
		v = apic_read(APIC_LVTPC);
		apic_write(APIC_LVTPC, v | APIC_LVT_MASKED);
	}

	
#ifdef CONFIG_X86_THERMAL_VECTOR
	if (maxlvt >= 5) {
		v = apic_read(APIC_LVTTHMR);
		apic_write(APIC_LVTTHMR, v | APIC_LVT_MASKED);
	}
#endif
#ifdef CONFIG_X86_MCE_INTEL
	if (maxlvt >= 6) {
		v = apic_read(APIC_LVTCMCI);
		if (!(v & APIC_LVT_MASKED))
			apic_write(APIC_LVTCMCI, v | APIC_LVT_MASKED);
	}
#endif

	apic_write(APIC_LVTT, APIC_LVT_MASKED);
	apic_write(APIC_LVT0, APIC_LVT_MASKED);
	apic_write(APIC_LVT1, APIC_LVT_MASKED);
	if (maxlvt >= 3)
		apic_write(APIC_LVTERR, APIC_LVT_MASKED);
	if (maxlvt >= 4)
		apic_write(APIC_LVTPC, APIC_LVT_MASKED);

	
	if (lapic_is_integrated()) {
		if (maxlvt > 3)
			
			apic_write(APIC_ESR, 0);
		apic_read(APIC_ESR);
	}
}

void disable_local_APIC(void)
{
	unsigned int value;

	
	if (!x2apic_mode && !apic_phys)
		return;

	clear_local_APIC();

	value = apic_read(APIC_SPIV);
	value &= ~APIC_SPIV_APIC_ENABLED;
	apic_write(APIC_SPIV, value);

#ifdef CONFIG_X86_32
	if (enabled_via_apicbase) {
		unsigned int l, h;

		rdmsr(MSR_IA32_APICBASE, l, h);
		l &= ~MSR_IA32_APICBASE_ENABLE;
		wrmsr(MSR_IA32_APICBASE, l, h);
	}
#endif
}

void lapic_shutdown(void)
{
	unsigned long flags;

	if (!cpu_has_apic && !apic_from_smp_config())
		return;

	local_irq_save(flags);

#ifdef CONFIG_X86_32
	if (!enabled_via_apicbase)
		clear_local_APIC();
	else
#endif
		disable_local_APIC();


	local_irq_restore(flags);
}

int __init verify_local_APIC(void)
{
	unsigned int reg0, reg1;

	reg0 = apic_read(APIC_LVR);
	apic_printk(APIC_DEBUG, "Getting VERSION: %x\n", reg0);
	apic_write(APIC_LVR, reg0 ^ APIC_LVR_MASK);
	reg1 = apic_read(APIC_LVR);
	apic_printk(APIC_DEBUG, "Getting VERSION: %x\n", reg1);

	if (reg1 != reg0)
		return 0;

	reg1 = GET_APIC_VERSION(reg0);
	if (reg1 == 0x00 || reg1 == 0xff)
		return 0;
	reg1 = lapic_get_maxlvt();
	if (reg1 < 0x02 || reg1 == 0xff)
		return 0;

	reg0 = apic_read(APIC_ID);
	apic_printk(APIC_DEBUG, "Getting ID: %x\n", reg0);
	apic_write(APIC_ID, reg0 ^ apic->apic_id_mask);
	reg1 = apic_read(APIC_ID);
	apic_printk(APIC_DEBUG, "Getting ID: %x\n", reg1);
	apic_write(APIC_ID, reg0);
	if (reg1 != (reg0 ^ apic->apic_id_mask))
		return 0;

	reg0 = apic_read(APIC_LVT0);
	apic_printk(APIC_DEBUG, "Getting LVT0: %x\n", reg0);
	reg1 = apic_read(APIC_LVT1);
	apic_printk(APIC_DEBUG, "Getting LVT1: %x\n", reg1);

	return 1;
}

void __init sync_Arb_IDs(void)
{
	if (modern_apic() || boot_cpu_data.x86_vendor == X86_VENDOR_AMD)
		return;

	apic_wait_icr_idle();

	apic_printk(APIC_DEBUG, "Synchronizing Arb IDs.\n");
	apic_write(APIC_ICR, APIC_DEST_ALLINC |
			APIC_INT_LEVELTRIG | APIC_DM_INIT);
}

void __init init_bsp_APIC(void)
{
	unsigned int value;

	if (smp_found_config || !cpu_has_apic)
		return;

	clear_local_APIC();

	value = apic_read(APIC_SPIV);
	value &= ~APIC_VECTOR_MASK;
	value |= APIC_SPIV_APIC_ENABLED;

#ifdef CONFIG_X86_32
	
	if ((boot_cpu_data.x86_vendor == X86_VENDOR_INTEL) &&
	    (boot_cpu_data.x86 == 15))
		value &= ~APIC_SPIV_FOCUS_DISABLED;
	else
#endif
		value |= APIC_SPIV_FOCUS_DISABLED;
	value |= SPURIOUS_APIC_VECTOR;
	apic_write(APIC_SPIV, value);

	apic_write(APIC_LVT0, APIC_DM_EXTINT);
	value = APIC_DM_NMI;
	if (!lapic_is_integrated())		
		value |= APIC_LVT_LEVEL_TRIGGER;
	apic_write(APIC_LVT1, value);
}

static void __cpuinit lapic_setup_esr(void)
{
	unsigned int oldvalue, value, maxlvt;

	if (!lapic_is_integrated()) {
		pr_info("No ESR for 82489DX.\n");
		return;
	}

	if (apic->disable_esr) {
		pr_info("Leaving ESR disabled.\n");
		return;
	}

	maxlvt = lapic_get_maxlvt();
	if (maxlvt > 3)		
		apic_write(APIC_ESR, 0);
	oldvalue = apic_read(APIC_ESR);

	
	value = ERROR_APIC_VECTOR;
	apic_write(APIC_LVTERR, value);

	if (maxlvt > 3)
		apic_write(APIC_ESR, 0);
	value = apic_read(APIC_ESR);
	if (value != oldvalue)
		apic_printk(APIC_VERBOSE, "ESR value before enabling "
			"vector: 0x%08x  after: 0x%08x\n",
			oldvalue, value);
}

void __cpuinit setup_local_APIC(void)
{
	int cpu = smp_processor_id();
	unsigned int value, queued;
	int i, j, acked = 0;
	unsigned long long tsc = 0, ntsc;
	long long max_loops = cpu_khz;

	if (cpu_has_tsc)
		rdtscll(tsc);

	if (disable_apic) {
		disable_ioapic_support();
		return;
	}

#ifdef CONFIG_X86_32
	
	if (lapic_is_integrated() && apic->disable_esr) {
		apic_write(APIC_ESR, 0);
		apic_write(APIC_ESR, 0);
		apic_write(APIC_ESR, 0);
		apic_write(APIC_ESR, 0);
	}
#endif
	perf_events_lapic_init();

	BUG_ON(!apic->apic_id_registered());

	apic->init_apic_ldr();

#ifdef CONFIG_X86_32
	i = early_per_cpu(x86_cpu_to_logical_apicid, cpu);
	WARN_ON(i != BAD_APICID && i != logical_smp_processor_id());
	
	early_per_cpu(x86_cpu_to_logical_apicid, cpu) =
		logical_smp_processor_id();

	if (apic->x86_32_numa_cpu_node)
		set_apicid_to_node(early_per_cpu(x86_cpu_to_apicid, cpu),
				   apic->x86_32_numa_cpu_node(cpu));
#endif

	value = apic_read(APIC_TASKPRI);
	value &= ~APIC_TPRI_MASK;
	apic_write(APIC_TASKPRI, value);

	do {
		queued = 0;
		for (i = APIC_ISR_NR - 1; i >= 0; i--)
			queued |= apic_read(APIC_IRR + i*0x10);

		for (i = APIC_ISR_NR - 1; i >= 0; i--) {
			value = apic_read(APIC_ISR + i*0x10);
			for (j = 31; j >= 0; j--) {
				if (value & (1<<j)) {
					ack_APIC_irq();
					acked++;
				}
			}
		}
		if (acked > 256) {
			printk(KERN_ERR "LAPIC pending interrupts after %d EOI\n",
			       acked);
			break;
		}
		if (cpu_has_tsc) {
			rdtscll(ntsc);
			max_loops = (cpu_khz << 10) - (ntsc - tsc);
		} else
			max_loops--;
	} while (queued && max_loops > 0);
	WARN_ON(max_loops <= 0);

	value = apic_read(APIC_SPIV);
	value &= ~APIC_VECTOR_MASK;
	value |= APIC_SPIV_APIC_ENABLED;

#ifdef CONFIG_X86_32

	value &= ~APIC_SPIV_FOCUS_DISABLED;
#endif

	value |= SPURIOUS_APIC_VECTOR;
	apic_write(APIC_SPIV, value);

	value = apic_read(APIC_LVT0) & APIC_LVT_MASKED;
	if (!cpu && (pic_mode || !value)) {
		value = APIC_DM_EXTINT;
		apic_printk(APIC_VERBOSE, "enabled ExtINT on CPU#%d\n", cpu);
	} else {
		value = APIC_DM_EXTINT | APIC_LVT_MASKED;
		apic_printk(APIC_VERBOSE, "masked ExtINT on CPU#%d\n", cpu);
	}
	apic_write(APIC_LVT0, value);

	if (!cpu)
		value = APIC_DM_NMI;
	else
		value = APIC_DM_NMI | APIC_LVT_MASKED;
	if (!lapic_is_integrated())		
		value |= APIC_LVT_LEVEL_TRIGGER;
	apic_write(APIC_LVT1, value);

#ifdef CONFIG_X86_MCE_INTEL
	
	if (!cpu)
		cmci_recheck();
#endif
}

void __cpuinit end_local_APIC_setup(void)
{
	lapic_setup_esr();

#ifdef CONFIG_X86_32
	{
		unsigned int value;
		
		value = apic_read(APIC_LVTT);
		value |= (APIC_LVT_MASKED | LOCAL_TIMER_VECTOR);
		apic_write(APIC_LVTT, value);
	}
#endif

	apic_pm_activate();
}

void __init bsp_end_local_APIC_setup(void)
{
	end_local_APIC_setup();

	if (intr_remapping_enabled)
		enable_drhd_fault_handling();

}

#ifdef CONFIG_X86_X2APIC
static inline void __disable_x2apic(u64 msr)
{
	wrmsrl(MSR_IA32_APICBASE,
	       msr & ~(X2APIC_ENABLE | XAPIC_ENABLE));
	wrmsrl(MSR_IA32_APICBASE, msr & ~X2APIC_ENABLE);
}

static __init void disable_x2apic(void)
{
	u64 msr;

	if (!cpu_has_x2apic)
		return;

	rdmsrl(MSR_IA32_APICBASE, msr);
	if (msr & X2APIC_ENABLE) {
		u32 x2apic_id = read_apic_id();

		if (x2apic_id >= 255)
			panic("Cannot disable x2apic, id: %08x\n", x2apic_id);

		pr_info("Disabling x2apic\n");
		__disable_x2apic(msr);

		if (nox2apic) {
			clear_cpu_cap(&cpu_data(0), X86_FEATURE_X2APIC);
			setup_clear_cpu_cap(X86_FEATURE_X2APIC);
		}

		x2apic_disabled = 1;
		x2apic_mode = 0;

		register_lapic_address(mp_lapic_addr);
	}
}

void check_x2apic(void)
{
	if (x2apic_enabled()) {
		pr_info("x2apic enabled by BIOS, switching to x2apic ops\n");
		x2apic_preenabled = x2apic_mode = 1;
	}
}

void enable_x2apic(void)
{
	u64 msr;

	rdmsrl(MSR_IA32_APICBASE, msr);
	if (x2apic_disabled) {
		__disable_x2apic(msr);
		return;
	}

	if (!x2apic_mode)
		return;

	if (!(msr & X2APIC_ENABLE)) {
		printk_once(KERN_INFO "Enabling x2apic\n");
		wrmsrl(MSR_IA32_APICBASE, msr | X2APIC_ENABLE);
	}
}
#endif 

int __init enable_IR(void)
{
#ifdef CONFIG_IRQ_REMAP
	if (!intr_remapping_supported()) {
		pr_debug("intr-remapping not supported\n");
		return -1;
	}

	if (!x2apic_preenabled && skip_ioapic_setup) {
		pr_info("Skipped enabling intr-remap because of skipping "
			"io-apic setup\n");
		return -1;
	}

	return enable_intr_remapping();
#endif
	return -1;
}

void __init enable_IR_x2apic(void)
{
	unsigned long flags;
	int ret, x2apic_enabled = 0;
	int dmar_table_init_ret;

	dmar_table_init_ret = dmar_table_init();
	if (dmar_table_init_ret && !x2apic_supported())
		return;

	ret = save_ioapic_entries();
	if (ret) {
		pr_info("Saving IO-APIC state failed: %d\n", ret);
		return;
	}

	local_irq_save(flags);
	legacy_pic->mask_all();
	mask_ioapic_entries();

	if (x2apic_preenabled && nox2apic)
		disable_x2apic();

	if (dmar_table_init_ret)
		ret = -1;
	else
		ret = enable_IR();

	if (!x2apic_supported())
		goto skip_x2apic;

	if (ret < 0) {
		if (max_physical_apicid > 255 ||
		    !hypervisor_x2apic_available()) {
			if (x2apic_preenabled)
				disable_x2apic();
			goto skip_x2apic;
		}
		x2apic_force_phys();
	}

	if (ret == IRQ_REMAP_XAPIC_MODE) {
		pr_info("x2apic not enabled, IRQ remapping is in xapic mode\n");
		goto skip_x2apic;
	}

	x2apic_enabled = 1;

	if (x2apic_supported() && !x2apic_mode) {
		x2apic_mode = 1;
		enable_x2apic();
		pr_info("Enabled x2apic\n");
	}

skip_x2apic:
	if (ret < 0) 
		restore_ioapic_entries();
	legacy_pic->restore_mask();
	local_irq_restore(flags);
}

#ifdef CONFIG_X86_64
/*
 * Detect and enable local APICs on non-SMP boards.
 * Original code written by Keir Fraser.
 * On AMD64 we trust the BIOS - if it says no APIC it is likely
 * not correctly set up (usually the APIC timer won't work etc.)
 */
static int __init detect_init_APIC(void)
{
	if (!cpu_has_apic) {
		pr_info("No local APIC present\n");
		return -1;
	}

	mp_lapic_addr = APIC_DEFAULT_PHYS_BASE;
	return 0;
}
#else

static int __init apic_verify(void)
{
	u32 features, h, l;

	features = cpuid_edx(1);
	if (!(features & (1 << X86_FEATURE_APIC))) {
		pr_warning("Could not enable APIC!\n");
		return -1;
	}
	set_cpu_cap(&boot_cpu_data, X86_FEATURE_APIC);
	mp_lapic_addr = APIC_DEFAULT_PHYS_BASE;

	
	if (boot_cpu_data.x86 >= 6) {
		rdmsr(MSR_IA32_APICBASE, l, h);
		if (l & MSR_IA32_APICBASE_ENABLE)
			mp_lapic_addr = l & MSR_IA32_APICBASE_BASE;
	}

	pr_info("Found and enabled local APIC!\n");
	return 0;
}

int __init apic_force_enable(unsigned long addr)
{
	u32 h, l;

	if (disable_apic)
		return -1;

	if (boot_cpu_data.x86 >= 6) {
		rdmsr(MSR_IA32_APICBASE, l, h);
		if (!(l & MSR_IA32_APICBASE_ENABLE)) {
			pr_info("Local APIC disabled by BIOS -- reenabling.\n");
			l &= ~MSR_IA32_APICBASE_BASE;
			l |= MSR_IA32_APICBASE_ENABLE | addr;
			wrmsr(MSR_IA32_APICBASE, l, h);
			enabled_via_apicbase = 1;
		}
	}
	return apic_verify();
}

static int __init detect_init_APIC(void)
{
	
	if (disable_apic)
		return -1;

	switch (boot_cpu_data.x86_vendor) {
	case X86_VENDOR_AMD:
		if ((boot_cpu_data.x86 == 6 && boot_cpu_data.x86_model > 1) ||
		    (boot_cpu_data.x86 >= 15))
			break;
		goto no_apic;
	case X86_VENDOR_INTEL:
		if (boot_cpu_data.x86 == 6 || boot_cpu_data.x86 == 15 ||
		    (boot_cpu_data.x86 == 5 && cpu_has_apic))
			break;
		goto no_apic;
	default:
		goto no_apic;
	}

	if (!cpu_has_apic) {
		if (!force_enable_local_apic) {
			pr_info("Local APIC disabled by BIOS -- "
				"you can enable it with \"lapic\"\n");
			return -1;
		}
		if (apic_force_enable(APIC_DEFAULT_PHYS_BASE))
			return -1;
	} else {
		if (apic_verify())
			return -1;
	}

	apic_pm_activate();

	return 0;

no_apic:
	pr_info("No local APIC present or hardware disabled\n");
	return -1;
}
#endif

void __init init_apic_mappings(void)
{
	unsigned int new_apicid;

	if (x2apic_mode) {
		boot_cpu_physical_apicid = read_apic_id();
		return;
	}

	
	if (!smp_found_config && detect_init_APIC()) {
		
		pr_info("APIC: disable apic facility\n");
		apic_disable();
	} else {
		apic_phys = mp_lapic_addr;

		if (!acpi_lapic && !smp_found_config)
			register_lapic_address(apic_phys);
	}

	new_apicid = read_apic_id();
	if (boot_cpu_physical_apicid != new_apicid) {
		boot_cpu_physical_apicid = new_apicid;
		apic_version[new_apicid] =
			 GET_APIC_VERSION(apic_read(APIC_LVR));
	}
}

void __init register_lapic_address(unsigned long address)
{
	mp_lapic_addr = address;

	if (!x2apic_mode) {
		set_fixmap_nocache(FIX_APIC_BASE, address);
		apic_printk(APIC_VERBOSE, "mapped APIC to %16lx (%16lx)\n",
			    APIC_BASE, mp_lapic_addr);
	}
	if (boot_cpu_physical_apicid == -1U) {
		boot_cpu_physical_apicid  = read_apic_id();
		apic_version[boot_cpu_physical_apicid] =
			 GET_APIC_VERSION(apic_read(APIC_LVR));
	}
}

int apic_version[MAX_LOCAL_APIC];

int __init APIC_init_uniprocessor(void)
{
	if (disable_apic) {
		pr_info("Apic disabled\n");
		return -1;
	}
#ifdef CONFIG_X86_64
	if (!cpu_has_apic) {
		disable_apic = 1;
		pr_info("Apic disabled by BIOS\n");
		return -1;
	}
#else
	if (!smp_found_config && !cpu_has_apic)
		return -1;

	if (!cpu_has_apic &&
	    APIC_INTEGRATED(apic_version[boot_cpu_physical_apicid])) {
		pr_err("BIOS bug, local APIC 0x%x not detected!...\n",
			boot_cpu_physical_apicid);
		return -1;
	}
#endif

	default_setup_apic_routing();

	verify_local_APIC();
	connect_bsp_APIC();

#ifdef CONFIG_X86_64
	apic_write(APIC_ID, SET_APIC_ID(boot_cpu_physical_apicid));
#else
# ifdef CONFIG_CRASH_DUMP
	boot_cpu_physical_apicid = read_apic_id();
# endif
#endif
	physid_set_mask_of_physid(boot_cpu_physical_apicid, &phys_cpu_present_map);
	setup_local_APIC();

#ifdef CONFIG_X86_IO_APIC
	if (!skip_ioapic_setup && nr_ioapics)
		enable_IO_APIC();
#endif

	bsp_end_local_APIC_setup();

#ifdef CONFIG_X86_IO_APIC
	if (smp_found_config && !skip_ioapic_setup && nr_ioapics)
		setup_IO_APIC();
	else {
		nr_ioapics = 0;
	}
#endif

	x86_init.timers.setup_percpu_clockev();
	return 0;
}


void smp_spurious_interrupt(struct pt_regs *regs)
{
	u32 v;

	irq_enter();
	exit_idle();
	v = apic_read(APIC_ISR + ((SPURIOUS_APIC_VECTOR & ~0x1f) >> 1));
	if (v & (1 << (SPURIOUS_APIC_VECTOR & 0x1f)))
		ack_APIC_irq();

	inc_irq_stat(irq_spurious_count);

	
	pr_info("spurious APIC interrupt on CPU#%d, "
		"should never happen.\n", smp_processor_id());
	irq_exit();
}

void smp_error_interrupt(struct pt_regs *regs)
{
	u32 v0, v1;
	u32 i = 0;
	static const char * const error_interrupt_reason[] = {
		"Send CS error",		
		"Receive CS error",		
		"Send accept error",		
		"Receive accept error",		
		"Redirectable IPI",		
		"Send illegal vector",		
		"Received illegal vector",	
		"Illegal register address",	
	};

	irq_enter();
	exit_idle();
	
	v0 = apic_read(APIC_ESR);
	apic_write(APIC_ESR, 0);
	v1 = apic_read(APIC_ESR);
	ack_APIC_irq();
	atomic_inc(&irq_err_count);

	apic_printk(APIC_DEBUG, KERN_DEBUG "APIC error on CPU%d: %02x(%02x)",
		    smp_processor_id(), v0 , v1);

	v1 = v1 & 0xff;
	while (v1) {
		if (v1 & 0x1)
			apic_printk(APIC_DEBUG, KERN_CONT " : %s", error_interrupt_reason[i]);
		i++;
		v1 >>= 1;
	};

	apic_printk(APIC_DEBUG, KERN_CONT "\n");

	irq_exit();
}

void __init connect_bsp_APIC(void)
{
#ifdef CONFIG_X86_32
	if (pic_mode) {
		clear_local_APIC();
		apic_printk(APIC_VERBOSE, "leaving PIC mode, "
				"enabling APIC mode.\n");
		imcr_pic_to_apic();
	}
#endif
	if (apic->enable_apic_mode)
		apic->enable_apic_mode();
}

void disconnect_bsp_APIC(int virt_wire_setup)
{
	unsigned int value;

#ifdef CONFIG_X86_32
	if (pic_mode) {
		apic_printk(APIC_VERBOSE, "disabling APIC mode, "
				"entering PIC mode.\n");
		imcr_apic_to_pic();
		return;
	}
#endif

	

	
	value = apic_read(APIC_SPIV);
	value &= ~APIC_VECTOR_MASK;
	value |= APIC_SPIV_APIC_ENABLED;
	value |= 0xf;
	apic_write(APIC_SPIV, value);

	if (!virt_wire_setup) {
		value = apic_read(APIC_LVT0);
		value &= ~(APIC_MODE_MASK | APIC_SEND_PENDING |
			APIC_INPUT_POLARITY | APIC_LVT_REMOTE_IRR |
			APIC_LVT_LEVEL_TRIGGER | APIC_LVT_MASKED);
		value |= APIC_LVT_REMOTE_IRR | APIC_SEND_PENDING;
		value = SET_APIC_DELIVERY_MODE(value, APIC_MODE_EXTINT);
		apic_write(APIC_LVT0, value);
	} else {
		
		apic_write(APIC_LVT0, APIC_LVT_MASKED);
	}

	value = apic_read(APIC_LVT1);
	value &= ~(APIC_MODE_MASK | APIC_SEND_PENDING |
			APIC_INPUT_POLARITY | APIC_LVT_REMOTE_IRR |
			APIC_LVT_LEVEL_TRIGGER | APIC_LVT_MASKED);
	value |= APIC_LVT_REMOTE_IRR | APIC_SEND_PENDING;
	value = SET_APIC_DELIVERY_MODE(value, APIC_MODE_NMI);
	apic_write(APIC_LVT1, value);
}

void __cpuinit generic_processor_info(int apicid, int version)
{
	int cpu, max = nr_cpu_ids;
	bool boot_cpu_detected = physid_isset(boot_cpu_physical_apicid,
				phys_cpu_present_map);

	if (!boot_cpu_detected && num_processors >= nr_cpu_ids - 1 &&
	    apicid != boot_cpu_physical_apicid) {
		int thiscpu = max + disabled_cpus - 1;

		pr_warning(
			"ACPI: NR_CPUS/possible_cpus limit of %i almost"
			" reached. Keeping one slot for boot cpu."
			"  Processor %d/0x%x ignored.\n", max, thiscpu, apicid);

		disabled_cpus++;
		return;
	}

	if (num_processors >= nr_cpu_ids) {
		int thiscpu = max + disabled_cpus;

		pr_warning(
			"ACPI: NR_CPUS/possible_cpus limit of %i reached."
			"  Processor %d/0x%x ignored.\n", max, thiscpu, apicid);

		disabled_cpus++;
		return;
	}

	num_processors++;
	if (apicid == boot_cpu_physical_apicid) {
		cpu = 0;
	} else
		cpu = cpumask_next_zero(-1, cpu_present_mask);

	if (version == 0x0) {
		pr_warning("BIOS bug: APIC version is 0 for CPU %d/0x%x, fixing up to 0x10\n",
			   cpu, apicid);
		version = 0x10;
	}
	apic_version[apicid] = version;

	if (version != apic_version[boot_cpu_physical_apicid]) {
		pr_warning("BIOS bug: APIC version mismatch, boot CPU: %x, CPU %d: version %x\n",
			apic_version[boot_cpu_physical_apicid], cpu, version);
	}

	physid_set(apicid, phys_cpu_present_map);
	if (apicid > max_physical_apicid)
		max_physical_apicid = apicid;

#if defined(CONFIG_SMP) || defined(CONFIG_X86_64)
	early_per_cpu(x86_cpu_to_apicid, cpu) = apicid;
	early_per_cpu(x86_bios_cpu_apicid, cpu) = apicid;
#endif
#ifdef CONFIG_X86_32
	early_per_cpu(x86_cpu_to_logical_apicid, cpu) =
		apic->x86_32_early_logical_apicid(cpu);
#endif
	set_cpu_possible(cpu, true);
	set_cpu_present(cpu, true);
}

int hard_smp_processor_id(void)
{
	return read_apic_id();
}

void default_init_apic_ldr(void)
{
	unsigned long val;

	apic_write(APIC_DFR, APIC_DFR_VALUE);
	val = apic_read(APIC_LDR) & ~APIC_LDR_MASK;
	val |= SET_APIC_LOGICAL_ID(1UL << smp_processor_id());
	apic_write(APIC_LDR, val);
}

#ifdef CONFIG_PM

static struct {
	int active;
	
	unsigned int apic_id;
	unsigned int apic_taskpri;
	unsigned int apic_ldr;
	unsigned int apic_dfr;
	unsigned int apic_spiv;
	unsigned int apic_lvtt;
	unsigned int apic_lvtpc;
	unsigned int apic_lvt0;
	unsigned int apic_lvt1;
	unsigned int apic_lvterr;
	unsigned int apic_tmict;
	unsigned int apic_tdcr;
	unsigned int apic_thmr;
} apic_pm_state;

static int lapic_suspend(void)
{
	unsigned long flags;
	int maxlvt;

	if (!apic_pm_state.active)
		return 0;

	maxlvt = lapic_get_maxlvt();

	apic_pm_state.apic_id = apic_read(APIC_ID);
	apic_pm_state.apic_taskpri = apic_read(APIC_TASKPRI);
	apic_pm_state.apic_ldr = apic_read(APIC_LDR);
	apic_pm_state.apic_dfr = apic_read(APIC_DFR);
	apic_pm_state.apic_spiv = apic_read(APIC_SPIV);
	apic_pm_state.apic_lvtt = apic_read(APIC_LVTT);
	if (maxlvt >= 4)
		apic_pm_state.apic_lvtpc = apic_read(APIC_LVTPC);
	apic_pm_state.apic_lvt0 = apic_read(APIC_LVT0);
	apic_pm_state.apic_lvt1 = apic_read(APIC_LVT1);
	apic_pm_state.apic_lvterr = apic_read(APIC_LVTERR);
	apic_pm_state.apic_tmict = apic_read(APIC_TMICT);
	apic_pm_state.apic_tdcr = apic_read(APIC_TDCR);
#ifdef CONFIG_X86_THERMAL_VECTOR
	if (maxlvt >= 5)
		apic_pm_state.apic_thmr = apic_read(APIC_LVTTHMR);
#endif

	local_irq_save(flags);
	disable_local_APIC();

	if (intr_remapping_enabled)
		disable_intr_remapping();

	local_irq_restore(flags);
	return 0;
}

static void lapic_resume(void)
{
	unsigned int l, h;
	unsigned long flags;
	int maxlvt;

	if (!apic_pm_state.active)
		return;

	local_irq_save(flags);
	if (intr_remapping_enabled) {
		mask_ioapic_entries();
		legacy_pic->mask_all();
	}

	if (x2apic_mode)
		enable_x2apic();
	else {
		if (boot_cpu_data.x86 >= 6) {
			rdmsr(MSR_IA32_APICBASE, l, h);
			l &= ~MSR_IA32_APICBASE_BASE;
			l |= MSR_IA32_APICBASE_ENABLE | mp_lapic_addr;
			wrmsr(MSR_IA32_APICBASE, l, h);
		}
	}

	maxlvt = lapic_get_maxlvt();
	apic_write(APIC_LVTERR, ERROR_APIC_VECTOR | APIC_LVT_MASKED);
	apic_write(APIC_ID, apic_pm_state.apic_id);
	apic_write(APIC_DFR, apic_pm_state.apic_dfr);
	apic_write(APIC_LDR, apic_pm_state.apic_ldr);
	apic_write(APIC_TASKPRI, apic_pm_state.apic_taskpri);
	apic_write(APIC_SPIV, apic_pm_state.apic_spiv);
	apic_write(APIC_LVT0, apic_pm_state.apic_lvt0);
	apic_write(APIC_LVT1, apic_pm_state.apic_lvt1);
#if defined(CONFIG_X86_MCE_P4THERMAL) || defined(CONFIG_X86_MCE_INTEL)
	if (maxlvt >= 5)
		apic_write(APIC_LVTTHMR, apic_pm_state.apic_thmr);
#endif
	if (maxlvt >= 4)
		apic_write(APIC_LVTPC, apic_pm_state.apic_lvtpc);
	apic_write(APIC_LVTT, apic_pm_state.apic_lvtt);
	apic_write(APIC_TDCR, apic_pm_state.apic_tdcr);
	apic_write(APIC_TMICT, apic_pm_state.apic_tmict);
	apic_write(APIC_ESR, 0);
	apic_read(APIC_ESR);
	apic_write(APIC_LVTERR, apic_pm_state.apic_lvterr);
	apic_write(APIC_ESR, 0);
	apic_read(APIC_ESR);

	if (intr_remapping_enabled)
		reenable_intr_remapping(x2apic_mode);

	local_irq_restore(flags);
}


static struct syscore_ops lapic_syscore_ops = {
	.resume		= lapic_resume,
	.suspend	= lapic_suspend,
};

static void __cpuinit apic_pm_activate(void)
{
	apic_pm_state.active = 1;
}

static int __init init_lapic_sysfs(void)
{
	
	if (cpu_has_apic)
		register_syscore_ops(&lapic_syscore_ops);

	return 0;
}

core_initcall(init_lapic_sysfs);

#else	

static void apic_pm_activate(void) { }

#endif	

#ifdef CONFIG_X86_64

static int __cpuinit apic_cluster_num(void)
{
	int i, clusters, zeros;
	unsigned id;
	u16 *bios_cpu_apicid;
	DECLARE_BITMAP(clustermap, NUM_APIC_CLUSTERS);

	bios_cpu_apicid = early_per_cpu_ptr(x86_bios_cpu_apicid);
	bitmap_zero(clustermap, NUM_APIC_CLUSTERS);

	for (i = 0; i < nr_cpu_ids; i++) {
		
		if (bios_cpu_apicid) {
			id = bios_cpu_apicid[i];
		} else if (i < nr_cpu_ids) {
			if (cpu_present(i))
				id = per_cpu(x86_bios_cpu_apicid, i);
			else
				continue;
		} else
			break;

		if (id != BAD_APICID)
			__set_bit(APIC_CLUSTERID(id), clustermap);
	}

	clusters = 0;
	zeros = 0;
	for (i = 0; i < NUM_APIC_CLUSTERS; i++) {
		if (test_bit(i, clustermap)) {
			clusters += 1 + zeros;
			zeros = 0;
		} else
			++zeros;
	}

	return clusters;
}

static int __cpuinitdata multi_checked;
static int __cpuinitdata multi;

static int __cpuinit set_multi(const struct dmi_system_id *d)
{
	if (multi)
		return 0;
	pr_info("APIC: %s detected, Multi Chassis\n", d->ident);
	multi = 1;
	return 0;
}

static const __cpuinitconst struct dmi_system_id multi_dmi_table[] = {
	{
		.callback = set_multi,
		.ident = "IBM System Summit2",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "IBM"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Summit2"),
		},
	},
	{}
};

static void __cpuinit dmi_check_multi(void)
{
	if (multi_checked)
		return;

	dmi_check_system(multi_dmi_table);
	multi_checked = 1;
}

__cpuinit int apic_is_clustered_box(void)
{
	dmi_check_multi();
	if (multi)
		return 1;

	if (!is_vsmp_box())
		return 0;

	if (apic_cluster_num() > 1)
		return 1;

	return 0;
}
#endif

static int __init setup_disableapic(char *arg)
{
	disable_apic = 1;
	setup_clear_cpu_cap(X86_FEATURE_APIC);
	return 0;
}
early_param("disableapic", setup_disableapic);

static int __init setup_nolapic(char *arg)
{
	return setup_disableapic(arg);
}
early_param("nolapic", setup_nolapic);

static int __init parse_lapic_timer_c2_ok(char *arg)
{
	local_apic_timer_c2_ok = 1;
	return 0;
}
early_param("lapic_timer_c2_ok", parse_lapic_timer_c2_ok);

static int __init parse_disable_apic_timer(char *arg)
{
	disable_apic_timer = 1;
	return 0;
}
early_param("noapictimer", parse_disable_apic_timer);

static int __init parse_nolapic_timer(char *arg)
{
	disable_apic_timer = 1;
	return 0;
}
early_param("nolapic_timer", parse_nolapic_timer);

static int __init apic_set_verbosity(char *arg)
{
	if (!arg)  {
#ifdef CONFIG_X86_64
		skip_ioapic_setup = 0;
		return 0;
#endif
		return -EINVAL;
	}

	if (strcmp("debug", arg) == 0)
		apic_verbosity = APIC_DEBUG;
	else if (strcmp("verbose", arg) == 0)
		apic_verbosity = APIC_VERBOSE;
	else {
		pr_warning("APIC Verbosity level %s not recognised"
			" use apic=verbose or apic=debug\n", arg);
		return -EINVAL;
	}

	return 0;
}
early_param("apic", apic_set_verbosity);

static int __init lapic_insert_resource(void)
{
	if (!apic_phys)
		return -1;

	
	lapic_resource.start = apic_phys;
	lapic_resource.end = lapic_resource.start + PAGE_SIZE - 1;
	insert_resource(&iomem_resource, &lapic_resource);

	return 0;
}

late_initcall(lapic_insert_resource);
