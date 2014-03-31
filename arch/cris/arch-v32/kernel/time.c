/*
 *  linux/arch/cris/arch-v32/kernel/time.c
 *
 *  Copyright (C) 2003-2010 Axis Communications AB
 *
 */

#include <linux/timex.h>
#include <linux/time.h>
#include <linux/clocksource.h>
#include <linux/interrupt.h>
#include <linux/swap.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/threads.h>
#include <linux/cpufreq.h>
#include <asm/types.h>
#include <asm/signal.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <asm/rtc.h>
#include <asm/irq.h>
#include <asm/irq_regs.h>

#include <hwregs/reg_map.h>
#include <hwregs/reg_rdwr.h>
#include <hwregs/timer_defs.h>
#include <hwregs/intr_vect_defs.h>
#ifdef CONFIG_CRIS_MACH_ARTPEC3
#include <hwregs/clkgen_defs.h>
#endif

#define ETRAX_WD_KEY_MASK	0x7F 
#define ETRAX_WD_HZ		763 
#define ETRAX_WD_CNT		((2*ETRAX_WD_HZ)/HZ + 1)

static cycle_t read_cont_rotime(struct clocksource *cs)
{
	return (u32)REG_RD(timer, regi_timer0, r_time);
}

static struct clocksource cont_rotime = {
	.name   = "crisv32_rotime",
	.rating = 300,
	.read   = read_cont_rotime,
	.mask   = CLOCKSOURCE_MASK(32),
	.flags  = CLOCK_SOURCE_IS_CONTINUOUS,
};

static int __init etrax_init_cont_rotime(void)
{
	clocksource_register_khz(&cont_rotime, 100000);
	return 0;
}
arch_initcall(etrax_init_cont_rotime);


unsigned long timer_regs[NR_CPUS] =
{
	regi_timer0,
#ifdef CONFIG_SMP
	regi_timer2
#endif
};

extern int set_rtc_mmss(unsigned long nowtime);
extern int have_rtc;

#ifdef CONFIG_CPU_FREQ
static int
cris_time_freq_notifier(struct notifier_block *nb, unsigned long val,
			void *data);

static struct notifier_block cris_time_freq_notifier_block = {
	.notifier_call = cris_time_freq_notifier,
};
#endif

unsigned long get_ns_in_jiffie(void)
{
	reg_timer_r_tmr0_data data;
	unsigned long ns;

	data = REG_RD(timer, regi_timer0, r_tmr0_data);
	ns = (TIMER0_DIV - data) * 10;
	return ns;
}



#define start_watchdog reset_watchdog

#if defined(CONFIG_ETRAX_WATCHDOG)
static short int watchdog_key = 42;  
#endif

#define WATCHDOG_MIN_FREE_PAGES 8

void reset_watchdog(void)
{
#if defined(CONFIG_ETRAX_WATCHDOG)
	reg_timer_rw_wd_ctrl wd_ctrl = { 0 };

	
	if(nr_free_pages() > WATCHDOG_MIN_FREE_PAGES) {
		
		
		watchdog_key ^= ETRAX_WD_KEY_MASK;
		wd_ctrl.cnt = ETRAX_WD_CNT;
		wd_ctrl.cmd = regk_timer_start;
		wd_ctrl.key = watchdog_key;
		REG_WR(timer, regi_timer0, rw_wd_ctrl, wd_ctrl);
	}
#endif
}


void stop_watchdog(void)
{
#if defined(CONFIG_ETRAX_WATCHDOG)
	reg_timer_rw_wd_ctrl wd_ctrl = { 0 };
	watchdog_key ^= ETRAX_WD_KEY_MASK; 
	wd_ctrl.cnt = ETRAX_WD_CNT;
	wd_ctrl.cmd = regk_timer_stop;
	wd_ctrl.key = watchdog_key;
	REG_WR(timer, regi_timer0, rw_wd_ctrl, wd_ctrl);
#endif
}

extern void show_registers(struct pt_regs *regs);

void handle_watchdog_bite(struct pt_regs *regs)
{
#if defined(CONFIG_ETRAX_WATCHDOG)
	extern int cause_of_death;

	oops_in_progress = 1;
	printk(KERN_WARNING "Watchdog bite\n");

	
	if (cause_of_death == 0xbedead) {
#ifdef CONFIG_CRIS_MACH_ARTPEC3
		reg_clkgen_rw_clk_ctrl ctrl =
			REG_RD(clkgen, regi_clkgen, rw_clk_ctrl);
		ctrl.pll = 0;
		REG_WR(clkgen, regi_clkgen, rw_clk_ctrl, ctrl);
#endif
		while(1);
	}

	
	stop_watchdog();
	printk(KERN_WARNING "Oops: bitten by watchdog\n");
	show_registers(regs);
	oops_in_progress = 0;
#ifndef CONFIG_ETRAX_WATCHDOG_NICE_DOGGY
	reset_watchdog();
#endif
	while(1) ;
#endif
}

extern void cris_do_profile(struct pt_regs *regs);

static inline irqreturn_t timer_interrupt(int irq, void *dev_id)
{
	struct pt_regs *regs = get_irq_regs();
	int cpu = smp_processor_id();
	reg_timer_r_masked_intr masked_intr;
	reg_timer_rw_ack_intr ack_intr = { 0 };

	
	masked_intr = REG_RD(timer, timer_regs[cpu], r_masked_intr);
	if (!masked_intr.tmr0)
		return IRQ_NONE;

	
	ack_intr.tmr0 = 1;
	REG_WR(timer, timer_regs[cpu], rw_ack_intr, ack_intr);

	
	reset_watchdog();

        
	update_process_times(user_mode(regs));

	cris_do_profile(regs); 

	
	if (cpu != 0)
		return IRQ_HANDLED;

	
	xtime_update(1);
        return IRQ_HANDLED;
}

static struct irqaction irq_timer = {
	.handler = timer_interrupt,
	.flags = IRQF_SHARED | IRQF_DISABLED,
	.name = "timer"
};

void __init cris_timer_init(void)
{
	int cpu = smp_processor_id();
	reg_timer_rw_tmr0_ctrl tmr0_ctrl = { 0 };
	reg_timer_rw_tmr0_div tmr0_div = TIMER0_DIV;
	reg_timer_rw_intr_mask timer_intr_mask;


	tmr0_ctrl.op = regk_timer_ld;
	tmr0_ctrl.freq = regk_timer_f100;
	REG_WR(timer, timer_regs[cpu], rw_tmr0_div, tmr0_div);
	REG_WR(timer, timer_regs[cpu], rw_tmr0_ctrl, tmr0_ctrl); 
	tmr0_ctrl.op = regk_timer_run;
	REG_WR(timer, timer_regs[cpu], rw_tmr0_ctrl, tmr0_ctrl); 

	
	timer_intr_mask = REG_RD(timer, timer_regs[cpu], rw_intr_mask);
	timer_intr_mask.tmr0 = 1;
	REG_WR(timer, timer_regs[cpu], rw_intr_mask, timer_intr_mask);
}

void __init time_init(void)
{
	reg_intr_vect_rw_mask intr_mask;

	loops_per_usec = 50;

	if(RTC_INIT() < 0)
		have_rtc = 0;
	else
		have_rtc = 1;

	
	cris_timer_init();

	
	intr_mask = REG_RD_VECT(intr_vect, regi_irq, rw_mask, 1);
	intr_mask.timer0 = 1;
	REG_WR_VECT(intr_vect, regi_irq, rw_mask, 1, intr_mask);

	setup_irq(TIMER0_INTR_VECT, &irq_timer);

	

#if defined(CONFIG_ETRAX_WATCHDOG)
	printk(KERN_INFO "Enabling watchdog...\n");
	start_watchdog();

	{
		unsigned long flags;
		local_save_flags(flags);
		flags |= (1<<30); 
		local_irq_restore(flags);
	}
#endif

#ifdef CONFIG_CPU_FREQ
	cpufreq_register_notifier(&cris_time_freq_notifier_block,
		CPUFREQ_TRANSITION_NOTIFIER);
#endif
}

#ifdef CONFIG_CPU_FREQ
static int
cris_time_freq_notifier(struct notifier_block *nb, unsigned long val,
			void *data)
{
	struct cpufreq_freqs *freqs = data;
	if (val == CPUFREQ_POSTCHANGE) {
		reg_timer_r_tmr0_data data;
		reg_timer_rw_tmr0_div div = (freqs->new * 500) / HZ;
		do {
			data = REG_RD(timer, timer_regs[freqs->cpu],
				r_tmr0_data);
		} while (data > 20);
		REG_WR(timer, timer_regs[freqs->cpu], rw_tmr0_div, div);
	}
	return 0;
}
#endif
