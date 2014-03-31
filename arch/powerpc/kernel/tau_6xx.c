/*
 * temp.c	Thermal management for cpu's with Thermal Assist Units
 *
 * Written by Troy Benjegerdes <hozer@drgw.net>
 *
 * TODO:
 * dynamic power management to limit peak CPU temp (using ICTC)
 * calibration???
 *
 * Silly, crazy ideas: use cpu load (from scheduler) and ICTC to extend battery
 * life in portables, and add a 'performance/watt' metric somewhere in /proc
 */

#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/reg.h>
#include <asm/nvram.h>
#include <asm/cache.h>
#include <asm/8xx_immap.h>
#include <asm/machdep.h>

static struct tau_temp
{
	int interrupts;
	unsigned char low;
	unsigned char high;
	unsigned char grew;
} tau[NR_CPUS];

struct timer_list tau_timer;

#undef DEBUG

#define step_size		2	
#define window_expand		1	
#define shrink_timer	2*HZ	
#define min_window	2	

void set_thresholds(unsigned long cpu)
{
#ifdef CONFIG_TAU_INT
	mtspr(SPRN_THRM1, THRM1_THRES(tau[cpu].low) | THRM1_V | THRM1_TIE | THRM1_TID);

	mtspr (SPRN_THRM2, THRM1_THRES(tau[cpu].high) | THRM1_V | THRM1_TIE);
#else
	
	mtspr(SPRN_THRM1, THRM1_THRES(tau[cpu].low) | THRM1_V | THRM1_TID);
	mtspr(SPRN_THRM2, THRM1_THRES(tau[cpu].high) | THRM1_V);
#endif
}

void TAUupdate(int cpu)
{
	unsigned thrm;

#ifdef DEBUG
	printk("TAUupdate ");
#endif

	if((thrm = mfspr(SPRN_THRM1)) & THRM1_TIV){ 
		if(thrm & THRM1_TIN){ 
			if (tau[cpu].low >= step_size){
				tau[cpu].low -= step_size;
				tau[cpu].high -= (step_size - window_expand);
			}
			tau[cpu].grew = 1;
#ifdef DEBUG
			printk("low threshold crossed ");
#endif
		}
	}
	if((thrm = mfspr(SPRN_THRM2)) & THRM1_TIV){ 
		if(thrm & THRM1_TIN){ 
			if (tau[cpu].high <= 127-step_size){
				tau[cpu].low += (step_size - window_expand);
				tau[cpu].high += step_size;
			}
			tau[cpu].grew = 1;
#ifdef DEBUG
			printk("high threshold crossed ");
#endif
		}
	}

#ifdef DEBUG
	printk("grew = %d\n", tau[cpu].grew);
#endif

#ifndef CONFIG_TAU_INT 
	set_thresholds(cpu);
#endif

}

#ifdef CONFIG_TAU_INT

void TAUException(struct pt_regs * regs)
{
	int cpu = smp_processor_id();

	irq_enter();
	tau[cpu].interrupts++;

	TAUupdate(cpu);

	irq_exit();
}
#endif 

static void tau_timeout(void * info)
{
	int cpu;
	unsigned long flags;
	int size;
	int shrink;

	
	local_irq_save(flags);
	cpu = smp_processor_id();

#ifndef CONFIG_TAU_INT
	TAUupdate(cpu);
#endif

	size = tau[cpu].high - tau[cpu].low;
	if (size > min_window && ! tau[cpu].grew) {
		
		shrink = (2 + size - min_window) / 4;
		if (shrink) {
			tau[cpu].low += shrink;
			tau[cpu].high -= shrink;
		} else { 
			tau[cpu].low += 1;
#if 1 
			if ((tau[cpu].high - tau[cpu].low) != min_window){
				printk(KERN_ERR "temp.c: line %d, logic error\n", __LINE__);
			}
#endif
		}
	}

	tau[cpu].grew = 0;

	set_thresholds(cpu);

	mtspr(SPRN_THRM3, THRM3_SITV(500*60) | THRM3_E);

	local_irq_restore(flags);
}

static void tau_timeout_smp(unsigned long unused)
{

	
	mod_timer(&tau_timer, jiffies + shrink_timer) ;
	on_each_cpu(tau_timeout, NULL, 0);
}


int tau_initialized = 0;

void __init TAU_init_smp(void * info)
{
	unsigned long cpu = smp_processor_id();

	tau[cpu].low = 5;
	tau[cpu].high = 120;

	set_thresholds(cpu);
}

int __init TAU_init(void)
{
	if (!cpu_has_feature(CPU_FTR_TAU)) {
		printk("Thermal assist unit not available\n");
		tau_initialized = 0;
		return 1;
	}


	
	init_timer(&tau_timer);
	tau_timer.function = tau_timeout_smp;
	tau_timer.expires = jiffies + shrink_timer;
	add_timer(&tau_timer);

	on_each_cpu(TAU_init_smp, NULL, 0);

	printk("Thermal assist unit ");
#ifdef CONFIG_TAU_INT
	printk("using interrupts, ");
#else
	printk("using timers, ");
#endif
	printk("shrink_timer: %d jiffies\n", shrink_timer);
	tau_initialized = 1;

	return 0;
}

__initcall(TAU_init);


u32 cpu_temp_both(unsigned long cpu)
{
	return ((tau[cpu].high << 16) | tau[cpu].low);
}

int cpu_temp(unsigned long cpu)
{
	return ((tau[cpu].high + tau[cpu].low) / 2);
}

int tau_interrupts(unsigned long cpu)
{
	return (tau[cpu].interrupts);
}
