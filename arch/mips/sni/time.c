#include <linux/types.h>
#include <linux/i8253.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/smp.h>
#include <linux/time.h>
#include <linux/clockchips.h>

#include <asm/sni.h>
#include <asm/time.h>
#include <asm-generic/rtc.h>

#define SNI_CLOCK_TICK_RATE     3686400
#define SNI_COUNTER2_DIV        64
#define SNI_COUNTER0_DIV        ((SNI_CLOCK_TICK_RATE / SNI_COUNTER2_DIV) / HZ)

static void a20r_set_mode(enum clock_event_mode mode,
                          struct clock_event_device *evt)
{
	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		*(volatile u8 *)(A20R_PT_CLOCK_BASE + 12) = 0x34;
		wmb();
		*(volatile u8 *)(A20R_PT_CLOCK_BASE +  0) = SNI_COUNTER0_DIV;
		wmb();
		*(volatile u8 *)(A20R_PT_CLOCK_BASE +  0) = SNI_COUNTER0_DIV >> 8;
		wmb();

		*(volatile u8 *)(A20R_PT_CLOCK_BASE + 12) = 0xb4;
		wmb();
		*(volatile u8 *)(A20R_PT_CLOCK_BASE +  8) = SNI_COUNTER2_DIV;
		wmb();
		*(volatile u8 *)(A20R_PT_CLOCK_BASE +  8) = SNI_COUNTER2_DIV >> 8;
		wmb();

                break;
        case CLOCK_EVT_MODE_ONESHOT:
        case CLOCK_EVT_MODE_UNUSED:
        case CLOCK_EVT_MODE_SHUTDOWN:
                break;
        case CLOCK_EVT_MODE_RESUME:
                break;
        }
}

static struct clock_event_device a20r_clockevent_device = {
	.name		= "a20r-timer",
	.features	= CLOCK_EVT_FEAT_PERIODIC,

	

	.rating		= 300,
	.irq		= SNI_A20R_IRQ_TIMER,
	.set_mode	= a20r_set_mode,
};

static irqreturn_t a20r_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *cd = dev_id;

	*(volatile u8 *)A20R_PT_TIM0_ACK = 0;
	wmb();

	cd->event_handler(cd);

	return IRQ_HANDLED;
}

static struct irqaction a20r_irqaction = {
	.handler	= a20r_interrupt,
	.flags		= IRQF_PERCPU | IRQF_TIMER,
	.name		= "a20r-timer",
};

static void __init sni_a20r_timer_setup(void)
{
	struct clock_event_device *cd = &a20r_clockevent_device;
	struct irqaction *action = &a20r_irqaction;
	unsigned int cpu = smp_processor_id();

	cd->cpumask             = cpumask_of(cpu);
	clockevents_register_device(cd);
	action->dev_id = cd;
	setup_irq(SNI_A20R_IRQ_TIMER, &a20r_irqaction);
}

#define SNI_8254_TICK_RATE        1193182UL

#define SNI_8254_TCSAMP_COUNTER   ((SNI_8254_TICK_RATE / HZ) + 255)

static __init unsigned long dosample(void)
{
	u32 ct0, ct1;
	volatile u8 msb;

	
	outb_p(0x34, 0x43);
	outb_p(SNI_8254_TCSAMP_COUNTER & 0xff, 0x40);
	outb(SNI_8254_TCSAMP_COUNTER >> 8, 0x40);

	
	ct0 = read_c0_count();

	
	do {
		outb(0x00, 0x43);
		(void) inb(0x40);
		msb = inb(0x40);
		ct1 = read_c0_count();
	} while (msb);

	
	outb(0x38, 0x43);
	
	return (ct1 - ct0) / (500000/HZ) * (500000/HZ);
}

void __init plat_time_init(void)
{
	unsigned long r4k_ticks[3];
	unsigned long r4k_tick;

	printk(KERN_INFO "Calibrating system timer... ");
	dosample();	
	dosample();	
	
	do {
		r4k_ticks[0] = dosample();
	} while (!r4k_ticks[0]);
	do {
		r4k_ticks[1] = dosample();
	} while (!r4k_ticks[1]);

	if (r4k_ticks[0] != r4k_ticks[1]) {
		printk("warning: timer counts differ, retrying... ");
		r4k_ticks[2] = dosample();
		if (r4k_ticks[2] == r4k_ticks[0]
		    || r4k_ticks[2] == r4k_ticks[1])
			r4k_tick = r4k_ticks[2];
		else {
			printk("disagreement, using average... ");
			r4k_tick = (r4k_ticks[0] + r4k_ticks[1]
				   + r4k_ticks[2]) / 3;
		}
	} else
		r4k_tick = r4k_ticks[0];

	printk("%d [%d.%04d MHz CPU]\n", (int) r4k_tick,
		(int) (r4k_tick / (500000 / HZ)),
		(int) (r4k_tick % (500000 / HZ)));

	mips_hpt_frequency = r4k_tick * HZ;

	switch (sni_brd_type) {
	case SNI_BRD_10:
	case SNI_BRD_10NEW:
	case SNI_BRD_TOWER_OASIC:
	case SNI_BRD_MINITOWER:
		sni_a20r_timer_setup();
		break;
	}
	setup_pit_timer();
}

void read_persistent_clock(struct timespec *ts)
{
	ts->tv_sec = -1;
	ts->tv_nsec = 0;
}
