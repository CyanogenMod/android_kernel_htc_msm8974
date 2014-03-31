#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/export.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/i8253.h>
#include <linux/slab.h>
#include <linux/hpet.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/pm.h>
#include <linux/io.h>

#include <asm/fixmap.h>
#include <asm/hpet.h>
#include <asm/time.h>

#define HPET_MASK			CLOCKSOURCE_MASK(32)

#define FSEC_PER_NSEC			1000000L

#define HPET_DEV_USED_BIT		2
#define HPET_DEV_USED			(1 << HPET_DEV_USED_BIT)
#define HPET_DEV_VALID			0x8
#define HPET_DEV_FSB_CAP		0x1000
#define HPET_DEV_PERI_CAP		0x2000

#define HPET_MIN_CYCLES			128
#define HPET_MIN_PROG_DELTA		(HPET_MIN_CYCLES + (HPET_MIN_CYCLES >> 1))

unsigned long				hpet_address;
u8					hpet_blockid; 
u8					hpet_msi_disable;

#ifdef CONFIG_PCI_MSI
static unsigned long			hpet_num_timers;
#endif
static void __iomem			*hpet_virt_address;

struct hpet_dev {
	struct clock_event_device	evt;
	unsigned int			num;
	int				cpu;
	unsigned int			irq;
	unsigned int			flags;
	char				name[10];
};

inline struct hpet_dev *EVT_TO_HPET_DEV(struct clock_event_device *evtdev)
{
	return container_of(evtdev, struct hpet_dev, evt);
}

inline unsigned int hpet_readl(unsigned int a)
{
	return readl(hpet_virt_address + a);
}

static inline void hpet_writel(unsigned int d, unsigned int a)
{
	writel(d, hpet_virt_address + a);
}

#ifdef CONFIG_X86_64
#include <asm/pgtable.h>
#endif

static inline void hpet_set_mapping(void)
{
	hpet_virt_address = ioremap_nocache(hpet_address, HPET_MMAP_SIZE);
#ifdef CONFIG_X86_64
	__set_fixmap(VSYSCALL_HPET, hpet_address, PAGE_KERNEL_VVAR_NOCACHE);
#endif
}

static inline void hpet_clear_mapping(void)
{
	iounmap(hpet_virt_address);
	hpet_virt_address = NULL;
}

static int boot_hpet_disable;
int hpet_force_user;
static int hpet_verbose;

static int __init hpet_setup(char *str)
{
	if (str) {
		if (!strncmp("disable", str, 7))
			boot_hpet_disable = 1;
		if (!strncmp("force", str, 5))
			hpet_force_user = 1;
		if (!strncmp("verbose", str, 7))
			hpet_verbose = 1;
	}
	return 1;
}
__setup("hpet=", hpet_setup);

static int __init disable_hpet(char *str)
{
	boot_hpet_disable = 1;
	return 1;
}
__setup("nohpet", disable_hpet);

static inline int is_hpet_capable(void)
{
	return !boot_hpet_disable && hpet_address;
}

static int hpet_legacy_int_enabled;

int is_hpet_enabled(void)
{
	return is_hpet_capable() && hpet_legacy_int_enabled;
}
EXPORT_SYMBOL_GPL(is_hpet_enabled);

static void _hpet_print_config(const char *function, int line)
{
	u32 i, timers, l, h;
	printk(KERN_INFO "hpet: %s(%d):\n", function, line);
	l = hpet_readl(HPET_ID);
	h = hpet_readl(HPET_PERIOD);
	timers = ((l & HPET_ID_NUMBER) >> HPET_ID_NUMBER_SHIFT) + 1;
	printk(KERN_INFO "hpet: ID: 0x%x, PERIOD: 0x%x\n", l, h);
	l = hpet_readl(HPET_CFG);
	h = hpet_readl(HPET_STATUS);
	printk(KERN_INFO "hpet: CFG: 0x%x, STATUS: 0x%x\n", l, h);
	l = hpet_readl(HPET_COUNTER);
	h = hpet_readl(HPET_COUNTER+4);
	printk(KERN_INFO "hpet: COUNTER_l: 0x%x, COUNTER_h: 0x%x\n", l, h);

	for (i = 0; i < timers; i++) {
		l = hpet_readl(HPET_Tn_CFG(i));
		h = hpet_readl(HPET_Tn_CFG(i)+4);
		printk(KERN_INFO "hpet: T%d: CFG_l: 0x%x, CFG_h: 0x%x\n",
		       i, l, h);
		l = hpet_readl(HPET_Tn_CMP(i));
		h = hpet_readl(HPET_Tn_CMP(i)+4);
		printk(KERN_INFO "hpet: T%d: CMP_l: 0x%x, CMP_h: 0x%x\n",
		       i, l, h);
		l = hpet_readl(HPET_Tn_ROUTE(i));
		h = hpet_readl(HPET_Tn_ROUTE(i)+4);
		printk(KERN_INFO "hpet: T%d ROUTE_l: 0x%x, ROUTE_h: 0x%x\n",
		       i, l, h);
	}
}

#define hpet_print_config()					\
do {								\
	if (hpet_verbose)					\
		_hpet_print_config(__FUNCTION__, __LINE__);	\
} while (0)

#ifdef CONFIG_HPET

static void hpet_reserve_msi_timers(struct hpet_data *hd);

static void hpet_reserve_platform_timers(unsigned int id)
{
	struct hpet __iomem *hpet = hpet_virt_address;
	struct hpet_timer __iomem *timer = &hpet->hpet_timers[2];
	unsigned int nrtimers, i;
	struct hpet_data hd;

	nrtimers = ((id & HPET_ID_NUMBER) >> HPET_ID_NUMBER_SHIFT) + 1;

	memset(&hd, 0, sizeof(hd));
	hd.hd_phys_address	= hpet_address;
	hd.hd_address		= hpet;
	hd.hd_nirqs		= nrtimers;
	hpet_reserve_timer(&hd, 0);

#ifdef CONFIG_HPET_EMULATE_RTC
	hpet_reserve_timer(&hd, 1);
#endif

	hd.hd_irq[0] = HPET_LEGACY_8254;
	hd.hd_irq[1] = HPET_LEGACY_RTC;

	for (i = 2; i < nrtimers; timer++, i++) {
		hd.hd_irq[i] = (readl(&timer->hpet_config) &
			Tn_INT_ROUTE_CNF_MASK) >> Tn_INT_ROUTE_CNF_SHIFT;
	}

	hpet_reserve_msi_timers(&hd);

	hpet_alloc(&hd);

}
#else
static void hpet_reserve_platform_timers(unsigned int id) { }
#endif

static unsigned long hpet_freq;

static void hpet_legacy_set_mode(enum clock_event_mode mode,
			  struct clock_event_device *evt);
static int hpet_legacy_next_event(unsigned long delta,
			   struct clock_event_device *evt);

static struct clock_event_device hpet_clockevent = {
	.name		= "hpet",
	.features	= CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.set_mode	= hpet_legacy_set_mode,
	.set_next_event = hpet_legacy_next_event,
	.irq		= 0,
	.rating		= 50,
};

static void hpet_stop_counter(void)
{
	unsigned long cfg = hpet_readl(HPET_CFG);
	cfg &= ~HPET_CFG_ENABLE;
	hpet_writel(cfg, HPET_CFG);
}

static void hpet_reset_counter(void)
{
	hpet_writel(0, HPET_COUNTER);
	hpet_writel(0, HPET_COUNTER + 4);
}

static void hpet_start_counter(void)
{
	unsigned int cfg = hpet_readl(HPET_CFG);
	cfg |= HPET_CFG_ENABLE;
	hpet_writel(cfg, HPET_CFG);
}

static void hpet_restart_counter(void)
{
	hpet_stop_counter();
	hpet_reset_counter();
	hpet_start_counter();
}

static void hpet_resume_device(void)
{
	force_hpet_resume();
}

static void hpet_resume_counter(struct clocksource *cs)
{
	hpet_resume_device();
	hpet_restart_counter();
}

static void hpet_enable_legacy_int(void)
{
	unsigned int cfg = hpet_readl(HPET_CFG);

	cfg |= HPET_CFG_LEGACY;
	hpet_writel(cfg, HPET_CFG);
	hpet_legacy_int_enabled = 1;
}

static void hpet_legacy_clockevent_register(void)
{
	
	hpet_enable_legacy_int();

	hpet_clockevent.cpumask = cpumask_of(smp_processor_id());
	clockevents_config_and_register(&hpet_clockevent, hpet_freq,
					HPET_MIN_PROG_DELTA, 0x7FFFFFFF);
	global_clock_event = &hpet_clockevent;
	printk(KERN_DEBUG "hpet clockevent registered\n");
}

static int hpet_setup_msi_irq(unsigned int irq);

static void hpet_set_mode(enum clock_event_mode mode,
			  struct clock_event_device *evt, int timer)
{
	unsigned int cfg, cmp, now;
	uint64_t delta;

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		hpet_stop_counter();
		delta = ((uint64_t)(NSEC_PER_SEC/HZ)) * evt->mult;
		delta >>= evt->shift;
		now = hpet_readl(HPET_COUNTER);
		cmp = now + (unsigned int) delta;
		cfg = hpet_readl(HPET_Tn_CFG(timer));
		
		cfg &= ~HPET_TN_LEVEL;
		cfg |= HPET_TN_ENABLE | HPET_TN_PERIODIC |
		       HPET_TN_SETVAL | HPET_TN_32BIT;
		hpet_writel(cfg, HPET_Tn_CFG(timer));
		hpet_writel(cmp, HPET_Tn_CMP(timer));
		udelay(1);
		hpet_writel((unsigned int) delta, HPET_Tn_CMP(timer));
		hpet_start_counter();
		hpet_print_config();
		break;

	case CLOCK_EVT_MODE_ONESHOT:
		cfg = hpet_readl(HPET_Tn_CFG(timer));
		cfg &= ~HPET_TN_PERIODIC;
		cfg |= HPET_TN_ENABLE | HPET_TN_32BIT;
		hpet_writel(cfg, HPET_Tn_CFG(timer));
		break;

	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		cfg = hpet_readl(HPET_Tn_CFG(timer));
		cfg &= ~HPET_TN_ENABLE;
		hpet_writel(cfg, HPET_Tn_CFG(timer));
		break;

	case CLOCK_EVT_MODE_RESUME:
		if (timer == 0) {
			hpet_enable_legacy_int();
		} else {
			struct hpet_dev *hdev = EVT_TO_HPET_DEV(evt);
			hpet_setup_msi_irq(hdev->irq);
			disable_irq(hdev->irq);
			irq_set_affinity(hdev->irq, cpumask_of(hdev->cpu));
			enable_irq(hdev->irq);
		}
		hpet_print_config();
		break;
	}
}

static int hpet_next_event(unsigned long delta,
			   struct clock_event_device *evt, int timer)
{
	u32 cnt;
	s32 res;

	cnt = hpet_readl(HPET_COUNTER);
	cnt += (u32) delta;
	hpet_writel(cnt, HPET_Tn_CMP(timer));

	res = (s32)(cnt - hpet_readl(HPET_COUNTER));

	return res < HPET_MIN_CYCLES ? -ETIME : 0;
}

static void hpet_legacy_set_mode(enum clock_event_mode mode,
			struct clock_event_device *evt)
{
	hpet_set_mode(mode, evt, 0);
}

static int hpet_legacy_next_event(unsigned long delta,
			struct clock_event_device *evt)
{
	return hpet_next_event(delta, evt, 0);
}

#ifdef CONFIG_PCI_MSI

static DEFINE_PER_CPU(struct hpet_dev *, cpu_hpet_dev);
static struct hpet_dev	*hpet_devs;

void hpet_msi_unmask(struct irq_data *data)
{
	struct hpet_dev *hdev = data->handler_data;
	unsigned int cfg;

	
	cfg = hpet_readl(HPET_Tn_CFG(hdev->num));
	cfg |= HPET_TN_FSB;
	hpet_writel(cfg, HPET_Tn_CFG(hdev->num));
}

void hpet_msi_mask(struct irq_data *data)
{
	struct hpet_dev *hdev = data->handler_data;
	unsigned int cfg;

	
	cfg = hpet_readl(HPET_Tn_CFG(hdev->num));
	cfg &= ~HPET_TN_FSB;
	hpet_writel(cfg, HPET_Tn_CFG(hdev->num));
}

void hpet_msi_write(struct hpet_dev *hdev, struct msi_msg *msg)
{
	hpet_writel(msg->data, HPET_Tn_ROUTE(hdev->num));
	hpet_writel(msg->address_lo, HPET_Tn_ROUTE(hdev->num) + 4);
}

void hpet_msi_read(struct hpet_dev *hdev, struct msi_msg *msg)
{
	msg->data = hpet_readl(HPET_Tn_ROUTE(hdev->num));
	msg->address_lo = hpet_readl(HPET_Tn_ROUTE(hdev->num) + 4);
	msg->address_hi = 0;
}

static void hpet_msi_set_mode(enum clock_event_mode mode,
				struct clock_event_device *evt)
{
	struct hpet_dev *hdev = EVT_TO_HPET_DEV(evt);
	hpet_set_mode(mode, evt, hdev->num);
}

static int hpet_msi_next_event(unsigned long delta,
				struct clock_event_device *evt)
{
	struct hpet_dev *hdev = EVT_TO_HPET_DEV(evt);
	return hpet_next_event(delta, evt, hdev->num);
}

static int hpet_setup_msi_irq(unsigned int irq)
{
	if (arch_setup_hpet_msi(irq, hpet_blockid)) {
		destroy_irq(irq);
		return -EINVAL;
	}
	return 0;
}

static int hpet_assign_irq(struct hpet_dev *dev)
{
	unsigned int irq;

	irq = create_irq_nr(0, -1);
	if (!irq)
		return -EINVAL;

	irq_set_handler_data(irq, dev);

	if (hpet_setup_msi_irq(irq))
		return -EINVAL;

	dev->irq = irq;
	return 0;
}

static irqreturn_t hpet_interrupt_handler(int irq, void *data)
{
	struct hpet_dev *dev = (struct hpet_dev *)data;
	struct clock_event_device *hevt = &dev->evt;

	if (!hevt->event_handler) {
		printk(KERN_INFO "Spurious HPET timer interrupt on HPET timer %d\n",
				dev->num);
		return IRQ_HANDLED;
	}

	hevt->event_handler(hevt);
	return IRQ_HANDLED;
}

static int hpet_setup_irq(struct hpet_dev *dev)
{

	if (request_irq(dev->irq, hpet_interrupt_handler,
			IRQF_TIMER | IRQF_DISABLED | IRQF_NOBALANCING,
			dev->name, dev))
		return -1;

	disable_irq(dev->irq);
	irq_set_affinity(dev->irq, cpumask_of(dev->cpu));
	enable_irq(dev->irq);

	printk(KERN_DEBUG "hpet: %s irq %d for MSI\n",
			 dev->name, dev->irq);

	return 0;
}

static void init_one_hpet_msi_clockevent(struct hpet_dev *hdev, int cpu)
{
	struct clock_event_device *evt = &hdev->evt;

	WARN_ON(cpu != smp_processor_id());
	if (!(hdev->flags & HPET_DEV_VALID))
		return;

	if (hpet_setup_msi_irq(hdev->irq))
		return;

	hdev->cpu = cpu;
	per_cpu(cpu_hpet_dev, cpu) = hdev;
	evt->name = hdev->name;
	hpet_setup_irq(hdev);
	evt->irq = hdev->irq;

	evt->rating = 110;
	evt->features = CLOCK_EVT_FEAT_ONESHOT;
	if (hdev->flags & HPET_DEV_PERI_CAP)
		evt->features |= CLOCK_EVT_FEAT_PERIODIC;

	evt->set_mode = hpet_msi_set_mode;
	evt->set_next_event = hpet_msi_next_event;
	evt->cpumask = cpumask_of(hdev->cpu);

	clockevents_config_and_register(evt, hpet_freq, HPET_MIN_PROG_DELTA,
					0x7FFFFFFF);
}

#ifdef CONFIG_HPET
#define RESERVE_TIMERS 1
#else
#define RESERVE_TIMERS 0
#endif

static void hpet_msi_capability_lookup(unsigned int start_timer)
{
	unsigned int id;
	unsigned int num_timers;
	unsigned int num_timers_used = 0;
	int i;

	if (hpet_msi_disable)
		return;

	if (boot_cpu_has(X86_FEATURE_ARAT))
		return;
	id = hpet_readl(HPET_ID);

	num_timers = ((id & HPET_ID_NUMBER) >> HPET_ID_NUMBER_SHIFT);
	num_timers++; 
	hpet_print_config();

	hpet_devs = kzalloc(sizeof(struct hpet_dev) * num_timers, GFP_KERNEL);
	if (!hpet_devs)
		return;

	hpet_num_timers = num_timers;

	for (i = start_timer; i < num_timers - RESERVE_TIMERS; i++) {
		struct hpet_dev *hdev = &hpet_devs[num_timers_used];
		unsigned int cfg = hpet_readl(HPET_Tn_CFG(i));

		
		if (!(cfg & HPET_TN_FSB_CAP))
			continue;

		hdev->flags = 0;
		if (cfg & HPET_TN_PERIODIC_CAP)
			hdev->flags |= HPET_DEV_PERI_CAP;
		hdev->num = i;

		sprintf(hdev->name, "hpet%d", i);
		if (hpet_assign_irq(hdev))
			continue;

		hdev->flags |= HPET_DEV_FSB_CAP;
		hdev->flags |= HPET_DEV_VALID;
		num_timers_used++;
		if (num_timers_used == num_possible_cpus())
			break;
	}

	printk(KERN_INFO "HPET: %d timers in total, %d timers will be used for per-cpu timer\n",
		num_timers, num_timers_used);
}

#ifdef CONFIG_HPET
static void hpet_reserve_msi_timers(struct hpet_data *hd)
{
	int i;

	if (!hpet_devs)
		return;

	for (i = 0; i < hpet_num_timers; i++) {
		struct hpet_dev *hdev = &hpet_devs[i];

		if (!(hdev->flags & HPET_DEV_VALID))
			continue;

		hd->hd_irq[hdev->num] = hdev->irq;
		hpet_reserve_timer(hd, hdev->num);
	}
}
#endif

static struct hpet_dev *hpet_get_unused_timer(void)
{
	int i;

	if (!hpet_devs)
		return NULL;

	for (i = 0; i < hpet_num_timers; i++) {
		struct hpet_dev *hdev = &hpet_devs[i];

		if (!(hdev->flags & HPET_DEV_VALID))
			continue;
		if (test_and_set_bit(HPET_DEV_USED_BIT,
			(unsigned long *)&hdev->flags))
			continue;
		return hdev;
	}
	return NULL;
}

struct hpet_work_struct {
	struct delayed_work work;
	struct completion complete;
};

static void hpet_work(struct work_struct *w)
{
	struct hpet_dev *hdev;
	int cpu = smp_processor_id();
	struct hpet_work_struct *hpet_work;

	hpet_work = container_of(w, struct hpet_work_struct, work.work);

	hdev = hpet_get_unused_timer();
	if (hdev)
		init_one_hpet_msi_clockevent(hdev, cpu);

	complete(&hpet_work->complete);
}

static int hpet_cpuhp_notify(struct notifier_block *n,
		unsigned long action, void *hcpu)
{
	unsigned long cpu = (unsigned long)hcpu;
	struct hpet_work_struct work;
	struct hpet_dev *hdev = per_cpu(cpu_hpet_dev, cpu);

	switch (action & 0xf) {
	case CPU_ONLINE:
		INIT_DELAYED_WORK_ONSTACK(&work.work, hpet_work);
		init_completion(&work.complete);
		
		schedule_delayed_work_on(cpu, &work.work, 0);
		wait_for_completion(&work.complete);
		destroy_timer_on_stack(&work.work.timer);
		break;
	case CPU_DEAD:
		if (hdev) {
			free_irq(hdev->irq, hdev);
			hdev->flags &= ~HPET_DEV_USED;
			per_cpu(cpu_hpet_dev, cpu) = NULL;
		}
		break;
	}
	return NOTIFY_OK;
}
#else

static int hpet_setup_msi_irq(unsigned int irq)
{
	return 0;
}
static void hpet_msi_capability_lookup(unsigned int start_timer)
{
	return;
}

#ifdef CONFIG_HPET
static void hpet_reserve_msi_timers(struct hpet_data *hd)
{
	return;
}
#endif

static int hpet_cpuhp_notify(struct notifier_block *n,
		unsigned long action, void *hcpu)
{
	return NOTIFY_OK;
}

#endif

static cycle_t read_hpet(struct clocksource *cs)
{
	return (cycle_t)hpet_readl(HPET_COUNTER);
}

static struct clocksource clocksource_hpet = {
	.name		= "hpet",
	.rating		= 250,
	.read		= read_hpet,
	.mask		= HPET_MASK,
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
	.resume		= hpet_resume_counter,
#ifdef CONFIG_X86_64
	.archdata	= { .vclock_mode = VCLOCK_HPET },
#endif
};

static int hpet_clocksource_register(void)
{
	u64 start, now;
	cycle_t t1;

	
	hpet_restart_counter();

	
	t1 = hpet_readl(HPET_COUNTER);
	rdtscll(start);

	do {
		rep_nop();
		rdtscll(now);
	} while ((now - start) < 200000UL);

	if (t1 == hpet_readl(HPET_COUNTER)) {
		printk(KERN_WARNING
		       "HPET counter not counting. HPET disabled\n");
		return -ENODEV;
	}

	clocksource_register_hz(&clocksource_hpet, (u32)hpet_freq);
	return 0;
}

int __init hpet_enable(void)
{
	unsigned long hpet_period;
	unsigned int id;
	u64 freq;
	int i;

	if (!is_hpet_capable())
		return 0;

	hpet_set_mapping();

	hpet_period = hpet_readl(HPET_PERIOD);

	for (i = 0; hpet_readl(HPET_CFG) == 0xFFFFFFFF; i++) {
		if (i == 1000) {
			printk(KERN_WARNING
			       "HPET config register value = 0xFFFFFFFF. "
			       "Disabling HPET\n");
			goto out_nohpet;
		}
	}

	if (hpet_period < HPET_MIN_PERIOD || hpet_period > HPET_MAX_PERIOD)
		goto out_nohpet;

	freq = FSEC_PER_SEC;
	do_div(freq, hpet_period);
	hpet_freq = freq;

	id = hpet_readl(HPET_ID);
	hpet_print_config();

#ifdef CONFIG_HPET_EMULATE_RTC
	if (!(id & HPET_ID_NUMBER))
		goto out_nohpet;
#endif

	if (hpet_clocksource_register())
		goto out_nohpet;

	if (id & HPET_ID_LEGSUP) {
		hpet_legacy_clockevent_register();
		return 1;
	}
	return 0;

out_nohpet:
	hpet_clear_mapping();
	hpet_address = 0;
	return 0;
}

static __init int hpet_late_init(void)
{
	int cpu;

	if (boot_hpet_disable)
		return -ENODEV;

	if (!hpet_address) {
		if (!force_hpet_address)
			return -ENODEV;

		hpet_address = force_hpet_address;
		hpet_enable();
	}

	if (!hpet_virt_address)
		return -ENODEV;

	if (hpet_readl(HPET_ID) & HPET_ID_LEGSUP)
		hpet_msi_capability_lookup(2);
	else
		hpet_msi_capability_lookup(0);

	hpet_reserve_platform_timers(hpet_readl(HPET_ID));
	hpet_print_config();

	if (hpet_msi_disable)
		return 0;

	if (boot_cpu_has(X86_FEATURE_ARAT))
		return 0;

	for_each_online_cpu(cpu) {
		hpet_cpuhp_notify(NULL, CPU_ONLINE, (void *)(long)cpu);
	}

	
	hotcpu_notifier(hpet_cpuhp_notify, -20);

	return 0;
}
fs_initcall(hpet_late_init);

void hpet_disable(void)
{
	if (is_hpet_capable() && hpet_virt_address) {
		unsigned int cfg = hpet_readl(HPET_CFG);

		if (hpet_legacy_int_enabled) {
			cfg &= ~HPET_CFG_LEGACY;
			hpet_legacy_int_enabled = 0;
		}
		cfg &= ~HPET_CFG_ENABLE;
		hpet_writel(cfg, HPET_CFG);
	}
}

#ifdef CONFIG_HPET_EMULATE_RTC

#include <linux/mc146818rtc.h>
#include <linux/rtc.h>
#include <asm/rtc.h>

#define DEFAULT_RTC_INT_FREQ	64
#define DEFAULT_RTC_SHIFT	6
#define RTC_NUM_INTS		1

static unsigned long hpet_rtc_flags;
static int hpet_prev_update_sec;
static struct rtc_time hpet_alarm_time;
static unsigned long hpet_pie_count;
static u32 hpet_t1_cmp;
static u32 hpet_default_delta;
static u32 hpet_pie_delta;
static unsigned long hpet_pie_limit;

static rtc_irq_handler irq_handler;

static inline int hpet_cnt_ahead(u32 c1, u32 c2)
{
	return (s32)(c2 - c1) < 0;
}

int hpet_register_irq_handler(rtc_irq_handler handler)
{
	if (!is_hpet_enabled())
		return -ENODEV;
	if (irq_handler)
		return -EBUSY;

	irq_handler = handler;

	return 0;
}
EXPORT_SYMBOL_GPL(hpet_register_irq_handler);

void hpet_unregister_irq_handler(rtc_irq_handler handler)
{
	if (!is_hpet_enabled())
		return;

	irq_handler = NULL;
	hpet_rtc_flags = 0;
}
EXPORT_SYMBOL_GPL(hpet_unregister_irq_handler);

int hpet_rtc_timer_init(void)
{
	unsigned int cfg, cnt, delta;
	unsigned long flags;

	if (!is_hpet_enabled())
		return 0;

	if (!hpet_default_delta) {
		uint64_t clc;

		clc = (uint64_t) hpet_clockevent.mult * NSEC_PER_SEC;
		clc >>= hpet_clockevent.shift + DEFAULT_RTC_SHIFT;
		hpet_default_delta = clc;
	}

	if (!(hpet_rtc_flags & RTC_PIE) || hpet_pie_limit)
		delta = hpet_default_delta;
	else
		delta = hpet_pie_delta;

	local_irq_save(flags);

	cnt = delta + hpet_readl(HPET_COUNTER);
	hpet_writel(cnt, HPET_T1_CMP);
	hpet_t1_cmp = cnt;

	cfg = hpet_readl(HPET_T1_CFG);
	cfg &= ~HPET_TN_PERIODIC;
	cfg |= HPET_TN_ENABLE | HPET_TN_32BIT;
	hpet_writel(cfg, HPET_T1_CFG);

	local_irq_restore(flags);

	return 1;
}
EXPORT_SYMBOL_GPL(hpet_rtc_timer_init);

static void hpet_disable_rtc_channel(void)
{
	unsigned long cfg;
	cfg = hpet_readl(HPET_T1_CFG);
	cfg &= ~HPET_TN_ENABLE;
	hpet_writel(cfg, HPET_T1_CFG);
}

int hpet_mask_rtc_irq_bit(unsigned long bit_mask)
{
	if (!is_hpet_enabled())
		return 0;

	hpet_rtc_flags &= ~bit_mask;
	if (unlikely(!hpet_rtc_flags))
		hpet_disable_rtc_channel();

	return 1;
}
EXPORT_SYMBOL_GPL(hpet_mask_rtc_irq_bit);

int hpet_set_rtc_irq_bit(unsigned long bit_mask)
{
	unsigned long oldbits = hpet_rtc_flags;

	if (!is_hpet_enabled())
		return 0;

	hpet_rtc_flags |= bit_mask;

	if ((bit_mask & RTC_UIE) && !(oldbits & RTC_UIE))
		hpet_prev_update_sec = -1;

	if (!oldbits)
		hpet_rtc_timer_init();

	return 1;
}
EXPORT_SYMBOL_GPL(hpet_set_rtc_irq_bit);

int hpet_set_alarm_time(unsigned char hrs, unsigned char min,
			unsigned char sec)
{
	if (!is_hpet_enabled())
		return 0;

	hpet_alarm_time.tm_hour = hrs;
	hpet_alarm_time.tm_min = min;
	hpet_alarm_time.tm_sec = sec;

	return 1;
}
EXPORT_SYMBOL_GPL(hpet_set_alarm_time);

int hpet_set_periodic_freq(unsigned long freq)
{
	uint64_t clc;

	if (!is_hpet_enabled())
		return 0;

	if (freq <= DEFAULT_RTC_INT_FREQ)
		hpet_pie_limit = DEFAULT_RTC_INT_FREQ / freq;
	else {
		clc = (uint64_t) hpet_clockevent.mult * NSEC_PER_SEC;
		do_div(clc, freq);
		clc >>= hpet_clockevent.shift;
		hpet_pie_delta = clc;
		hpet_pie_limit = 0;
	}
	return 1;
}
EXPORT_SYMBOL_GPL(hpet_set_periodic_freq);

int hpet_rtc_dropped_irq(void)
{
	return is_hpet_enabled();
}
EXPORT_SYMBOL_GPL(hpet_rtc_dropped_irq);

static void hpet_rtc_timer_reinit(void)
{
	unsigned int delta;
	int lost_ints = -1;

	if (unlikely(!hpet_rtc_flags))
		hpet_disable_rtc_channel();

	if (!(hpet_rtc_flags & RTC_PIE) || hpet_pie_limit)
		delta = hpet_default_delta;
	else
		delta = hpet_pie_delta;

	do {
		hpet_t1_cmp += delta;
		hpet_writel(hpet_t1_cmp, HPET_T1_CMP);
		lost_ints++;
	} while (!hpet_cnt_ahead(hpet_t1_cmp, hpet_readl(HPET_COUNTER)));

	if (lost_ints) {
		if (hpet_rtc_flags & RTC_PIE)
			hpet_pie_count += lost_ints;
		if (printk_ratelimit())
			printk(KERN_WARNING "hpet1: lost %d rtc interrupts\n",
				lost_ints);
	}
}

irqreturn_t hpet_rtc_interrupt(int irq, void *dev_id)
{
	struct rtc_time curr_time;
	unsigned long rtc_int_flag = 0;

	hpet_rtc_timer_reinit();
	memset(&curr_time, 0, sizeof(struct rtc_time));

	if (hpet_rtc_flags & (RTC_UIE | RTC_AIE))
		get_rtc_time(&curr_time);

	if (hpet_rtc_flags & RTC_UIE &&
	    curr_time.tm_sec != hpet_prev_update_sec) {
		if (hpet_prev_update_sec >= 0)
			rtc_int_flag = RTC_UF;
		hpet_prev_update_sec = curr_time.tm_sec;
	}

	if (hpet_rtc_flags & RTC_PIE &&
	    ++hpet_pie_count >= hpet_pie_limit) {
		rtc_int_flag |= RTC_PF;
		hpet_pie_count = 0;
	}

	if (hpet_rtc_flags & RTC_AIE &&
	    (curr_time.tm_sec == hpet_alarm_time.tm_sec) &&
	    (curr_time.tm_min == hpet_alarm_time.tm_min) &&
	    (curr_time.tm_hour == hpet_alarm_time.tm_hour))
			rtc_int_flag |= RTC_AF;

	if (rtc_int_flag) {
		rtc_int_flag |= (RTC_IRQF | (RTC_NUM_INTS << 8));
		if (irq_handler)
			irq_handler(rtc_int_flag, dev_id);
	}
	return IRQ_HANDLED;
}
EXPORT_SYMBOL_GPL(hpet_rtc_interrupt);
#endif
