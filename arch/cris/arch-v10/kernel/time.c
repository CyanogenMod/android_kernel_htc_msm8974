/*
 *  linux/arch/cris/arch-v10/kernel/time.c
 *
 *  Copyright (C) 1991, 1992, 1995  Linus Torvalds
 *  Copyright (C) 1999-2002 Axis Communications AB
 *
 */

#include <linux/timex.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/swap.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <arch/svinto.h>
#include <asm/types.h>
#include <asm/signal.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <asm/rtc.h>
#include <asm/irq_regs.h>

#undef USE_CASCADE_TIMERS

extern int set_rtc_mmss(unsigned long nowtime);
extern int have_rtc;

unsigned long get_ns_in_jiffie(void)
{
	unsigned char timer_count, t1;
	unsigned short presc_count;
	unsigned long ns;
	unsigned long flags;

	local_irq_save(flags);
	timer_count = *R_TIMER0_DATA;
	presc_count = *R_TIM_PRESC_STATUS;  
	
	t1 = *R_TIMER0_DATA;

	if (timer_count != t1){
		
		presc_count = *R_TIM_PRESC_STATUS;
		timer_count = t1;
	}
	local_irq_restore(flags);
	if (presc_count >= PRESCALE_VALUE/2 ){
		presc_count =  PRESCALE_VALUE - presc_count + PRESCALE_VALUE/2;
	} else {
		presc_count =  PRESCALE_VALUE - presc_count - PRESCALE_VALUE/2;
	}

	ns = ( (TIMER0_DIV - timer_count) * ((1000000000/HZ)/TIMER0_DIV )) + 
	     ( (presc_count) * (1000000000/PRESCALE_FREQ));
	return ns;
}

unsigned long do_slow_gettimeoffset(void)
{
	unsigned long count;

	count = *R_TIMER0_DATA;

	
	return (TIMER0_DIV - count) * ((NSEC_PER_SEC/1000)/HZ)/TIMER0_DIV;
}

/* Excerpt from the Etrax100 HSDD about the built-in watchdog:
 *
 * 3.10.4 Watchdog timer

 * When the watchdog timer is started, it generates an NMI if the watchdog
 * isn't restarted or stopped within 0.1 s. If it still isn't restarted or
 * stopped after an additional 3.3 ms, the watchdog resets the chip.
 * The watchdog timer is stopped after reset. The watchdog timer is controlled
 * by the R_WATCHDOG register. The R_WATCHDOG register contains an enable bit
 * and a 3-bit key value. The effect of writing to the R_WATCHDOG register is
 * described in the table below:
 * 
 *   Watchdog    Value written:
 *   state:      To enable:  To key:      Operation:
 *   --------    ----------  -------      ----------
 *   stopped         0         X          No effect.
 *   stopped         1       key_val      Start watchdog with key = key_val.
 *   started         0       ~key         Stop watchdog
 *   started         1       ~key         Restart watchdog with key = ~key.
 *   started         X       new_key_val  Change key to new_key_val.
 * 
 * Note: '~' is the bitwise NOT operator.
 * 
 */

#define start_watchdog reset_watchdog

#if defined(CONFIG_ETRAX_WATCHDOG) && !defined(CONFIG_SVINTO_SIM)
static int watchdog_key = 0;  
#endif


#define WATCHDOG_MIN_FREE_PAGES 8

void
reset_watchdog(void)
{
#if defined(CONFIG_ETRAX_WATCHDOG) && !defined(CONFIG_SVINTO_SIM)
	
	if(nr_free_pages() > WATCHDOG_MIN_FREE_PAGES) {
		
		watchdog_key ^= 0x7; 
		*R_WATCHDOG = IO_FIELD(R_WATCHDOG, key, watchdog_key) |
			IO_STATE(R_WATCHDOG, enable, start);
	}
#endif
}


void 
stop_watchdog(void)
{
#if defined(CONFIG_ETRAX_WATCHDOG) && !defined(CONFIG_SVINTO_SIM)
	watchdog_key ^= 0x7; 
	*R_WATCHDOG = IO_FIELD(R_WATCHDOG, key, watchdog_key) |
		IO_STATE(R_WATCHDOG, enable, stop);
#endif	
}




extern void cris_do_profile(struct pt_regs *regs);

static inline irqreturn_t
timer_interrupt(int irq, void *dev_id)
{
	struct pt_regs *regs = get_irq_regs();
	

#ifdef USE_CASCADE_TIMERS
	*R_TIMER_CTRL =
		IO_FIELD( R_TIMER_CTRL, timerdiv1, 0) |
		IO_FIELD( R_TIMER_CTRL, timerdiv0, 0) |
		IO_STATE( R_TIMER_CTRL, i1, clr) |
		IO_STATE( R_TIMER_CTRL, tm1, run) |
		IO_STATE( R_TIMER_CTRL, clksel1, cascade0) |
		IO_STATE( R_TIMER_CTRL, i0, clr) |
		IO_STATE( R_TIMER_CTRL, tm0, run) |
		IO_STATE( R_TIMER_CTRL, clksel0, c6250kHz);
#else
	*R_TIMER_CTRL = r_timer_ctrl_shadow | 
		IO_STATE(R_TIMER_CTRL, i0, clr);
#endif

	
	reset_watchdog();
	
	
	update_process_times(user_mode(regs));

	

	xtime_update(1);
	
        cris_do_profile(regs); 
        return IRQ_HANDLED;
}


static struct irqaction irq2  = {
	.handler = timer_interrupt,
	.flags = IRQF_SHARED | IRQF_DISABLED,
	.name = "timer",
};

void __init
time_init(void)
{	
	loops_per_usec = 50;

	if(RTC_INIT() < 0)
		have_rtc = 0;
	else
		have_rtc = 1;

	
#ifdef USE_CASCADE_TIMERS
	*R_TIMER_CTRL =
		IO_FIELD( R_TIMER_CTRL, timerdiv1, 0) |
		IO_FIELD( R_TIMER_CTRL, timerdiv0, 0) |
		IO_STATE( R_TIMER_CTRL, i1, nop) |
		IO_STATE( R_TIMER_CTRL, tm1, stop_ld) |
		IO_STATE( R_TIMER_CTRL, clksel1, cascade0) |
		IO_STATE( R_TIMER_CTRL, i0, nop) |
		IO_STATE( R_TIMER_CTRL, tm0, stop_ld) |
		IO_STATE( R_TIMER_CTRL, clksel0, c6250kHz);
	
	*R_TIMER_CTRL = r_timer_ctrl_shadow = 
		IO_FIELD( R_TIMER_CTRL, timerdiv1, 0) |
		IO_FIELD( R_TIMER_CTRL, timerdiv0, 0) |
		IO_STATE( R_TIMER_CTRL, i1, nop) |
		IO_STATE( R_TIMER_CTRL, tm1, run) |
		IO_STATE( R_TIMER_CTRL, clksel1, cascade0) |
		IO_STATE( R_TIMER_CTRL, i0, nop) |
		IO_STATE( R_TIMER_CTRL, tm0, run) |
		IO_STATE( R_TIMER_CTRL, clksel0, c6250kHz);
#else
	*R_TIMER_CTRL = 
		IO_FIELD(R_TIMER_CTRL, timerdiv1, 192)      | 
		IO_FIELD(R_TIMER_CTRL, timerdiv0, TIMER0_DIV)      |
		IO_STATE(R_TIMER_CTRL, i1,        nop)      | 
		IO_STATE(R_TIMER_CTRL, tm1,       stop_ld)  |
		IO_STATE(R_TIMER_CTRL, clksel1,   c19k2Hz)  |
		IO_STATE(R_TIMER_CTRL, i0,        nop)      |
		IO_STATE(R_TIMER_CTRL, tm0,       stop_ld)  |
		IO_STATE(R_TIMER_CTRL, clksel0,   flexible);
	
	*R_TIMER_CTRL = r_timer_ctrl_shadow =
		IO_FIELD(R_TIMER_CTRL, timerdiv1, 192)      | 
		IO_FIELD(R_TIMER_CTRL, timerdiv0, TIMER0_DIV)      |
		IO_STATE(R_TIMER_CTRL, i1,        nop)      |
		IO_STATE(R_TIMER_CTRL, tm1,       run)      |
		IO_STATE(R_TIMER_CTRL, clksel1,   c19k2Hz)  |
		IO_STATE(R_TIMER_CTRL, i0,        nop)      |
		IO_STATE(R_TIMER_CTRL, tm0,       run)      |
		IO_STATE(R_TIMER_CTRL, clksel0,   flexible);

	*R_TIMER_PRESCALE = PRESCALE_VALUE;
#endif

	*R_IRQ_MASK0_SET =
		IO_STATE(R_IRQ_MASK0_SET, timer0, set); 
	
	
	
	setup_irq(2, &irq2); 

	

#if defined(CONFIG_ETRAX_WATCHDOG) && !defined(CONFIG_SVINTO_SIM)
	printk("Enabling watchdog...\n");
	start_watchdog();

	asm ("setf m");

	*R_IRQ_MASK0_SET =
		IO_STATE(R_IRQ_MASK0_SET, watchdog_nmi, set);
	*R_VECT_MASK_SET =
		IO_STATE(R_VECT_MASK_SET, nmi, set);
#endif
}
