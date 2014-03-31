/*
 * Cell Broadband Engine OProfile Support
 *
 * (C) Copyright IBM Corporation 2006
 *
 * Author: David Erb (djerb@us.ibm.com)
 * Modifications:
 *	   Carl Love <carll@us.ibm.com>
 *	   Maynard Johnson <maynardj@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/oprofile.h>
#include <linux/percpu.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <asm/cell-pmu.h>
#include <asm/cputable.h>
#include <asm/firmware.h>
#include <asm/io.h>
#include <asm/oprofile_impl.h>
#include <asm/processor.h>
#include <asm/prom.h>
#include <asm/ptrace.h>
#include <asm/reg.h>
#include <asm/rtas.h>
#include <asm/cell-regs.h>

#include "../platforms/cell/interrupt.h"
#include "cell/pr_util.h"

#define PPU_PROFILING            0
#define SPU_PROFILING_CYCLES     1
#define SPU_PROFILING_EVENTS     2

#define SPU_EVENT_NUM_START      4100
#define SPU_EVENT_NUM_STOP       4399
#define SPU_PROFILE_EVENT_ADDR          4363  
#define SPU_PROFILE_EVENT_ADDR_MASK_A   0x146 
#define SPU_PROFILE_EVENT_ADDR_MASK_B   0x186 

#define NUM_SPUS_PER_NODE    8
#define SPU_CYCLES_EVENT_NUM 2	

#define PPU_CYCLES_EVENT_NUM 1	
#define PPU_CYCLES_GRP_NUM   1	
#define CBE_COUNT_ALL_CYCLES 0x42800000 

#define NUM_THREADS 2         
#define NUM_DEBUG_BUS_WORDS 4
#define NUM_INPUT_BUS_WORDS 2

#define MAX_SPU_COUNT 0xFFFFFF	

#define NUM_INTERVAL_CYC  0xFFFFFFFF - 10

static unsigned int spu_cycle_reset;
static unsigned int profiling_mode;
static int spu_evnt_phys_spu_indx;

struct pmc_cntrl_data {
	unsigned long vcntr;
	unsigned long evnts;
	unsigned long masks;
	unsigned long enabled;
};

struct pm_signal {
	u16 cpu;		
	u16 sub_unit;		
	short int signal_group; 
	u8 bus_word;		
	u8 bit;			
};

enum {
	SUBFUNC_RESET = 1,
	SUBFUNC_ACTIVATE = 2,
	SUBFUNC_DEACTIVATE = 3,

	PASSTHRU_IGNORE = 0,
	PASSTHRU_ENABLE = 1,
	PASSTHRU_DISABLE = 2,
};

struct pm_cntrl {
	u16 enable;
	u16 stop_at_max;
	u16 trace_mode;
	u16 freeze;
	u16 count_mode;
	u16 spu_addr_trace;
	u8  trace_buf_ovflw;
};

static struct {
	u32 group_control;
	u32 debug_bus_control;
	struct pm_cntrl pm_cntrl;
	u32 pm07_cntrl[NR_PHYS_CTRS];
} pm_regs;

#define GET_SUB_UNIT(x) ((x & 0x0000f000) >> 12)
#define GET_BUS_WORD(x) ((x & 0x000000f0) >> 4)
#define GET_BUS_TYPE(x) ((x & 0x00000300) >> 8)
#define GET_POLARITY(x) ((x & 0x00000002) >> 1)
#define GET_COUNT_CYCLES(x) (x & 0x00000001)
#define GET_INPUT_CONTROL(x) ((x & 0x00000004) >> 2)

static DEFINE_PER_CPU(unsigned long[NR_PHYS_CTRS], pmc_values);
static unsigned long spu_pm_cnt[MAX_NUMNODES * NUM_SPUS_PER_NODE];
static struct pmc_cntrl_data pmc_cntrl[NUM_THREADS][NR_PHYS_CTRS];


static u32 hdw_thread;

static u32 virt_cntr_inter_mask;
static struct timer_list timer_virt_cntr;
static struct timer_list timer_spu_event_swap;

static struct pm_signal pm_signal[NR_PHYS_CTRS];
static int pm_rtas_token;    
static int spu_rtas_token;   

static u32 reset_value[NR_PHYS_CTRS];
static int num_counters;
static int oprofile_running;
static DEFINE_SPINLOCK(cntr_lock);

static u32 ctr_enabled;

static unsigned char input_bus[NUM_INPUT_BUS_WORDS];

static int
rtas_ibm_cbe_perftools(int subfunc, int passthru,
		       void *address, unsigned long length)
{
	u64 paddr = __pa(address);

	return rtas_call(pm_rtas_token, 5, 1, NULL, subfunc,
			 passthru, paddr >> 32, paddr & 0xffffffff, length);
}

static void pm_rtas_reset_signals(u32 node)
{
	int ret;
	struct pm_signal pm_signal_local;


	
	pm_signal_local.cpu = node;
	pm_signal_local.signal_group = 21;
	pm_signal_local.bus_word = 1;
	pm_signal_local.sub_unit = 0;
	pm_signal_local.bit = 0;

	ret = rtas_ibm_cbe_perftools(SUBFUNC_RESET, PASSTHRU_DISABLE,
				     &pm_signal_local,
				     sizeof(struct pm_signal));

	if (unlikely(ret))
		printk(KERN_WARNING "%s: rtas returned: %d\n",
		       __func__, ret);
}

static int pm_rtas_activate_signals(u32 node, u32 count)
{
	int ret;
	int i, j;
	struct pm_signal pm_signal_local[NR_PHYS_CTRS];

	i = 0;
	for (j = 0; j < count; j++) {
		if (pm_signal[j].signal_group != PPU_CYCLES_GRP_NUM) {

			
			pm_signal_local[i].cpu = node;
			pm_signal_local[i].signal_group
				= pm_signal[j].signal_group;
			pm_signal_local[i].bus_word = pm_signal[j].bus_word;
			pm_signal_local[i].sub_unit = pm_signal[j].sub_unit;
			pm_signal_local[i].bit = pm_signal[j].bit;
			i++;
		}
	}

	if (i != 0) {
		ret = rtas_ibm_cbe_perftools(SUBFUNC_ACTIVATE, PASSTHRU_ENABLE,
					     pm_signal_local,
					     i * sizeof(struct pm_signal));

		if (unlikely(ret)) {
			printk(KERN_WARNING "%s: rtas returned: %d\n",
			       __func__, ret);
			return -EIO;
		}
	}

	return 0;
}

static void set_pm_event(u32 ctr, int event, u32 unit_mask)
{
	struct pm_signal *p;
	u32 signal_bit;
	u32 bus_word, bus_type, count_cycles, polarity, input_control;
	int j, i;

	if (event == PPU_CYCLES_EVENT_NUM) {
		
		pm_regs.pm07_cntrl[ctr] = CBE_COUNT_ALL_CYCLES;
		p = &(pm_signal[ctr]);
		p->signal_group = PPU_CYCLES_GRP_NUM;
		p->bus_word = 1;
		p->sub_unit = 0;
		p->bit = 0;
		goto out;
	} else {
		pm_regs.pm07_cntrl[ctr] = 0;
	}

	bus_word = GET_BUS_WORD(unit_mask);
	bus_type = GET_BUS_TYPE(unit_mask);
	count_cycles = GET_COUNT_CYCLES(unit_mask);
	polarity = GET_POLARITY(unit_mask);
	input_control = GET_INPUT_CONTROL(unit_mask);
	signal_bit = (event % 100);

	p = &(pm_signal[ctr]);

	p->signal_group = event / 100;
	p->bus_word = bus_word;
	p->sub_unit = GET_SUB_UNIT(unit_mask);

	pm_regs.pm07_cntrl[ctr] = 0;
	pm_regs.pm07_cntrl[ctr] |= PM07_CTR_COUNT_CYCLES(count_cycles);
	pm_regs.pm07_cntrl[ctr] |= PM07_CTR_POLARITY(polarity);
	pm_regs.pm07_cntrl[ctr] |= PM07_CTR_INPUT_CONTROL(input_control);

	if (input_control == 0) {
		if (signal_bit > 31) {
			signal_bit -= 32;
			if (bus_word == 0x3)
				bus_word = 0x2;
			else if (bus_word == 0xc)
				bus_word = 0x8;
		}

		if ((bus_type == 0) && p->signal_group >= 60)
			bus_type = 2;
		if ((bus_type == 1) && p->signal_group >= 50)
			bus_type = 0;

		pm_regs.pm07_cntrl[ctr] |= PM07_CTR_INPUT_MUX(signal_bit);
	} else {
		pm_regs.pm07_cntrl[ctr] = 0;
		p->bit = signal_bit;
	}

	for (i = 0; i < NUM_DEBUG_BUS_WORDS; i++) {
		if (bus_word & (1 << i)) {
			pm_regs.debug_bus_control |=
				(bus_type << (30 - (2 * i)));

			for (j = 0; j < NUM_INPUT_BUS_WORDS; j++) {
				if (input_bus[j] == 0xff) {
					input_bus[j] = i;
					pm_regs.group_control |=
						(i << (30 - (2 * j)));

					break;
				}
			}
		}
	}
out:
	;
}

static void write_pm_cntrl(int cpu)
{

	u32 val = 0;
	if (pm_regs.pm_cntrl.enable == 1)
		val |= CBE_PM_ENABLE_PERF_MON;

	if (pm_regs.pm_cntrl.stop_at_max == 1)
		val |= CBE_PM_STOP_AT_MAX;

	if (pm_regs.pm_cntrl.trace_mode != 0)
		val |= CBE_PM_TRACE_MODE_SET(pm_regs.pm_cntrl.trace_mode);

	if (pm_regs.pm_cntrl.trace_buf_ovflw == 1)
		val |= CBE_PM_TRACE_BUF_OVFLW(pm_regs.pm_cntrl.trace_buf_ovflw);
	if (pm_regs.pm_cntrl.freeze == 1)
		val |= CBE_PM_FREEZE_ALL_CTRS;

	val |= CBE_PM_SPU_ADDR_TRACE_SET(pm_regs.pm_cntrl.spu_addr_trace);

	val |= CBE_PM_COUNT_MODE_SET(pm_regs.pm_cntrl.count_mode);
	cbe_write_pm(cpu, pm_control, val);
}

static inline void
set_count_mode(u32 kernel, u32 user)
{
	if (kernel) {
		if (user)
			pm_regs.pm_cntrl.count_mode = CBE_COUNT_ALL_MODES;
		else
			pm_regs.pm_cntrl.count_mode =
				CBE_COUNT_SUPERVISOR_MODE;
	} else {
		if (user)
			pm_regs.pm_cntrl.count_mode = CBE_COUNT_PROBLEM_MODE;
		else
			pm_regs.pm_cntrl.count_mode =
				CBE_COUNT_HYPERVISOR_MODE;
	}
}

static inline void enable_ctr(u32 cpu, u32 ctr, u32 *pm07_cntrl)
{

	pm07_cntrl[ctr] |= CBE_PM_CTR_ENABLE;
	cbe_write_pm07_control(cpu, ctr, pm07_cntrl[ctr]);
}

static void cell_virtual_cntr(unsigned long data)
{
	int i, prev_hdw_thread, next_hdw_thread;
	u32 cpu;
	unsigned long flags;


	spin_lock_irqsave(&cntr_lock, flags);

	prev_hdw_thread = hdw_thread;

	
	hdw_thread = 1 ^ hdw_thread;
	next_hdw_thread = hdw_thread;

	pm_regs.group_control = 0;
	pm_regs.debug_bus_control = 0;

	for (i = 0; i < NUM_INPUT_BUS_WORDS; i++)
		input_bus[i] = 0xff;

	for (i = 0; i < num_counters; i++)
		set_pm_event(i,
			pmc_cntrl[next_hdw_thread][i].evnts,
			pmc_cntrl[next_hdw_thread][i].masks);

	for_each_online_cpu(cpu) {
		if (cbe_get_hw_thread_id(cpu))
			continue;

		cbe_disable_pm(cpu);
		cbe_disable_pm_interrupts(cpu);
		for (i = 0; i < num_counters; i++) {
			per_cpu(pmc_values, cpu + prev_hdw_thread)[i]
				= cbe_read_ctr(cpu, i);

			if (per_cpu(pmc_values, cpu + next_hdw_thread)[i]
			    == 0xFFFFFFFF)
				cbe_write_ctr(cpu, i, 0xFFFFFFF0);
			else
				cbe_write_ctr(cpu, i,
					      per_cpu(pmc_values,
						      cpu +
						      next_hdw_thread)[i]);
		}

		for (i = 0; i < num_counters; i++) {
			if (pmc_cntrl[next_hdw_thread][i].enabled) {
				enable_ctr(cpu, i,
					   pm_regs.pm07_cntrl);
			} else {
				cbe_write_pm07_control(cpu, i, 0);
			}
		}

		
		cbe_enable_pm_interrupts(cpu, next_hdw_thread,
					 virt_cntr_inter_mask);
		cbe_enable_pm(cpu);
	}

	spin_unlock_irqrestore(&cntr_lock, flags);

	mod_timer(&timer_virt_cntr, jiffies + HZ / 10);
}

static void start_virt_cntrs(void)
{
	init_timer(&timer_virt_cntr);
	timer_virt_cntr.function = cell_virtual_cntr;
	timer_virt_cntr.data = 0UL;
	timer_virt_cntr.expires = jiffies + HZ / 10;
	add_timer(&timer_virt_cntr);
}

static int cell_reg_setup_spu_cycles(struct op_counter_config *ctr,
			struct op_system_config *sys, int num_ctrs)
{
	spu_cycle_reset = ctr[0].count;

	spu_rtas_token = rtas_token("ibm,cbe-spu-perftools");

	if (unlikely(spu_rtas_token == RTAS_UNKNOWN_SERVICE)) {
		printk(KERN_ERR
		       "%s: rtas token ibm,cbe-spu-perftools unknown\n",
		       __func__);
		return -EIO;
	}
	return 0;
}

static void spu_evnt_swap(unsigned long data)
{
	int node;
	int cur_phys_spu, nxt_phys_spu, cur_spu_evnt_phys_spu_indx;
	unsigned long flags;
	int cpu;
	int ret;
	u32 interrupt_mask;


	
	interrupt_mask = CBE_PM_CTR_OVERFLOW_INTR(0);

	hdw_thread = 0;

	spin_lock_irqsave(&cntr_lock, flags);

	cur_spu_evnt_phys_spu_indx = spu_evnt_phys_spu_indx;

	if (++(spu_evnt_phys_spu_indx) == NUM_SPUS_PER_NODE)
		spu_evnt_phys_spu_indx = 0;

	pm_signal[0].sub_unit = spu_evnt_phys_spu_indx;
	pm_signal[1].sub_unit = spu_evnt_phys_spu_indx;
	pm_signal[2].sub_unit = spu_evnt_phys_spu_indx;

	
	for_each_online_cpu(cpu) {
		if (cbe_get_hw_thread_id(cpu))
			continue;

		node = cbe_cpu_to_node(cpu);
		cur_phys_spu = (node * NUM_SPUS_PER_NODE)
			+ cur_spu_evnt_phys_spu_indx;
		nxt_phys_spu = (node * NUM_SPUS_PER_NODE)
			+ spu_evnt_phys_spu_indx;

		cbe_disable_pm(cpu);
		cbe_disable_pm_interrupts(cpu);

		spu_pm_cnt[cur_phys_spu]
			= cbe_read_ctr(cpu, 0);

		
		if (spu_pm_cnt[nxt_phys_spu] >= 0xFFFFFFFF)
			cbe_write_ctr(cpu, 0, 0xFFFFFFF0);
		 else
			 cbe_write_ctr(cpu, 0, spu_pm_cnt[nxt_phys_spu]);

		pm_rtas_reset_signals(cbe_cpu_to_node(cpu));

		ret = pm_rtas_activate_signals(cbe_cpu_to_node(cpu), 3);
		if (ret)
			printk(KERN_ERR "%s: pm_rtas_activate_signals failed, "
			       "SPU event swap\n", __func__);

		cbe_write_pm(cpu, trace_address, 0);

		enable_ctr(cpu, 0, pm_regs.pm07_cntrl);

		
		cbe_enable_pm_interrupts(cpu, hdw_thread,
					 interrupt_mask);
		cbe_enable_pm(cpu);
	}

	spin_unlock_irqrestore(&cntr_lock, flags);

	
	mod_timer(&timer_spu_event_swap, jiffies + HZ / 25);
}

static void start_spu_event_swap(void)
{
	init_timer(&timer_spu_event_swap);
	timer_spu_event_swap.function = spu_evnt_swap;
	timer_spu_event_swap.data = 0UL;
	timer_spu_event_swap.expires = jiffies + HZ / 25;
	add_timer(&timer_spu_event_swap);
}

static int cell_reg_setup_spu_events(struct op_counter_config *ctr,
			struct op_system_config *sys, int num_ctrs)
{
	int i;

	

	spu_evnt_phys_spu_indx = 0;
	pm_rtas_token = rtas_token("ibm,cbe-perftools");

	if (unlikely(pm_rtas_token == RTAS_UNKNOWN_SERVICE)) {
		printk(KERN_ERR
		       "%s: rtas token ibm,cbe-perftools unknown\n",
		       __func__);
		return -EIO;
	}

	/* setup the pm_control register settings,
	 * settings will be written per node by the
	 * cell_cpu_setup() function.
	 */
	pm_regs.pm_cntrl.trace_buf_ovflw = 1;

	pm_regs.pm_cntrl.trace_mode = 2;

	pm_regs.pm_cntrl.spu_addr_trace = 0x1;  

	pm_signal[1].signal_group = SPU_PROFILE_EVENT_ADDR / 100;
	pm_signal[1].bus_word = GET_BUS_WORD(SPU_PROFILE_EVENT_ADDR_MASK_A);
	pm_signal[1].bit = SPU_PROFILE_EVENT_ADDR % 100;
	pm_signal[1].sub_unit = spu_evnt_phys_spu_indx;

	pm_signal[2].signal_group = SPU_PROFILE_EVENT_ADDR / 100;
	pm_signal[2].bus_word = GET_BUS_WORD(SPU_PROFILE_EVENT_ADDR_MASK_B);
	pm_signal[2].bit = SPU_PROFILE_EVENT_ADDR % 100;
	pm_signal[2].sub_unit = spu_evnt_phys_spu_indx;

	num_counters = 1;  
	set_pm_event(0, ctr[0].event, ctr[0].unit_mask);

	reset_value[0] = 0xFFFFFFFF - ctr[0].count;

	
	ctr_enabled |= 1;

	
	for (i=0; i < MAX_NUMNODES * NUM_SPUS_PER_NODE; i++)
		spu_pm_cnt[i] = reset_value[0];

	return 0;
}

static int cell_reg_setup_ppu(struct op_counter_config *ctr,
			struct op_system_config *sys, int num_ctrs)
{
	
	int i, j, cpu;

	num_counters = num_ctrs;

	if (unlikely(num_ctrs > NR_PHYS_CTRS)) {
		printk(KERN_ERR
		       "%s: Oprofile, number of specified events " \
		       "exceeds number of physical counters\n",
		       __func__);
		return -EIO;
	}

	set_count_mode(sys->enable_kernel, sys->enable_user);

	
	for (i = 0; i < num_ctrs; ++i) {

		pmc_cntrl[0][i].evnts = ctr[i].event;
		pmc_cntrl[0][i].masks = ctr[i].unit_mask;
		pmc_cntrl[0][i].enabled = ctr[i].enabled;
		pmc_cntrl[0][i].vcntr = i;

		for_each_possible_cpu(j)
			per_cpu(pmc_values, j)[i] = 0;
	}

	for (i = 0; i < num_ctrs; ++i) {
		if ((ctr[i].event >= 2100) && (ctr[i].event <= 2111))
			pmc_cntrl[1][i].evnts = ctr[i].event + 19;
		else if (ctr[i].event == 2203)
			pmc_cntrl[1][i].evnts = ctr[i].event;
		else if ((ctr[i].event >= 2200) && (ctr[i].event <= 2215))
			pmc_cntrl[1][i].evnts = ctr[i].event + 16;
		else
			pmc_cntrl[1][i].evnts = ctr[i].event;

		pmc_cntrl[1][i].masks = ctr[i].unit_mask;
		pmc_cntrl[1][i].enabled = ctr[i].enabled;
		pmc_cntrl[1][i].vcntr = i;
	}

	for (i = 0; i < NUM_INPUT_BUS_WORDS; i++)
		input_bus[i] = 0xff;

	for (i = 0; i < num_counters; ++i) {
		
		if (pmc_cntrl[0][i].enabled) {
			
			reset_value[i] = 0xFFFFFFFF - ctr[i].count;
			set_pm_event(i,
				     pmc_cntrl[0][i].evnts,
				     pmc_cntrl[0][i].masks);

			
			ctr_enabled |= (1 << i);
		}
	}

	
	for_each_online_cpu(cpu)
		for (i = 0; i < num_counters; ++i) {
			per_cpu(pmc_values, cpu)[i] = reset_value[i];
		}

	return 0;
}


static int cell_reg_setup(struct op_counter_config *ctr,
			struct op_system_config *sys, int num_ctrs)
{
	int ret=0;
	spu_cycle_reset = 0;

	pm_regs.group_control = 0;
	pm_regs.debug_bus_control = 0;
	pm_regs.pm_cntrl.stop_at_max = 1;
	pm_regs.pm_cntrl.trace_mode = 0;
	pm_regs.pm_cntrl.freeze = 1;
	pm_regs.pm_cntrl.trace_buf_ovflw = 0;
	pm_regs.pm_cntrl.spu_addr_trace = 0;

	pm_rtas_token = rtas_token("ibm,cbe-perftools");

	if (unlikely(pm_rtas_token == RTAS_UNKNOWN_SERVICE)) {
		printk(KERN_ERR
		       "%s: rtas token ibm,cbe-perftools unknown\n",
		       __func__);
		return -EIO;
	}

	if (ctr[0].event == SPU_CYCLES_EVENT_NUM) {
		profiling_mode = SPU_PROFILING_CYCLES;
		ret = cell_reg_setup_spu_cycles(ctr, sys, num_ctrs);
	} else if ((ctr[0].event >= SPU_EVENT_NUM_START) &&
		   (ctr[0].event <= SPU_EVENT_NUM_STOP)) {
		profiling_mode = SPU_PROFILING_EVENTS;
		spu_cycle_reset = ctr[0].count;

		cell_reg_setup_spu_events(ctr, sys, num_ctrs);
	} else {
		profiling_mode = PPU_PROFILING;
		ret = cell_reg_setup_ppu(ctr, sys, num_ctrs);
	}

	return ret;
}



static int cell_cpu_setup(struct op_counter_config *cntr)
{
	u32 cpu = smp_processor_id();
	u32 num_enabled = 0;
	int i;
	int ret;

	if (profiling_mode == SPU_PROFILING_CYCLES)
		return 0;

	if (cbe_get_hw_thread_id(cpu))
		return 0;

	
	cbe_disable_pm(cpu);
	cbe_disable_pm_interrupts(cpu);

	cbe_write_pm(cpu, pm_start_stop, 0);
	cbe_write_pm(cpu, group_control, pm_regs.group_control);
	cbe_write_pm(cpu, debug_bus_control, pm_regs.debug_bus_control);
	write_pm_cntrl(cpu);

	for (i = 0; i < num_counters; ++i) {
		if (ctr_enabled & (1 << i)) {
			pm_signal[num_enabled].cpu = cbe_cpu_to_node(cpu);
			num_enabled++;
		}
	}

	if (profiling_mode == SPU_PROFILING_EVENTS) {
		ret = pm_rtas_activate_signals(cbe_cpu_to_node(cpu),
					       num_enabled+2);
		cbe_write_pm(cpu, pm_interval, NUM_INTERVAL_CYC);
		return ret;
	} else
		return pm_rtas_activate_signals(cbe_cpu_to_node(cpu),
						num_enabled);
}

#define ENTRIES	 303
#define MAXLFSR	 0xFFFFFF

static int initial_lfsr[] = {
 8221349, 12579195, 5379618, 10097839, 7512963, 7519310, 3955098, 10753424,
 15507573, 7458917, 285419, 2641121, 9780088, 3915503, 6668768, 1548716,
 4885000, 8774424, 9650099, 2044357, 2304411, 9326253, 10332526, 4421547,
 3440748, 10179459, 13332843, 10375561, 1313462, 8375100, 5198480, 6071392,
 9341783, 1526887, 3985002, 1439429, 13923762, 7010104, 11969769, 4547026,
 2040072, 4025602, 3437678, 7939992, 11444177, 4496094, 9803157, 10745556,
 3671780, 4257846, 5662259, 13196905, 3237343, 12077182, 16222879, 7587769,
 14706824, 2184640, 12591135, 10420257, 7406075, 3648978, 11042541, 15906893,
 11914928, 4732944, 10695697, 12928164, 11980531, 4430912, 11939291, 2917017,
 6119256, 4172004, 9373765, 8410071, 14788383, 5047459, 5474428, 1737756,
 15967514, 13351758, 6691285, 8034329, 2856544, 14394753, 11310160, 12149558,
 7487528, 7542781, 15668898, 12525138, 12790975, 3707933, 9106617, 1965401,
 16219109, 12801644, 2443203, 4909502, 8762329, 3120803, 6360315, 9309720,
 15164599, 10844842, 4456529, 6667610, 14924259, 884312, 6234963, 3326042,
 15973422, 13919464, 5272099, 6414643, 3909029, 2764324, 5237926, 4774955,
 10445906, 4955302, 5203726, 10798229, 11443419, 2303395, 333836, 9646934,
 3464726, 4159182, 568492, 995747, 10318756, 13299332, 4836017, 8237783,
 3878992, 2581665, 11394667, 5672745, 14412947, 3159169, 9094251, 16467278,
 8671392, 15230076, 4843545, 7009238, 15504095, 1494895, 9627886, 14485051,
 8304291, 252817, 12421642, 16085736, 4774072, 2456177, 4160695, 15409741,
 4902868, 5793091, 13162925, 16039714, 782255, 11347835, 14884586, 366972,
 16308990, 11913488, 13390465, 2958444, 10340278, 1177858, 1319431, 10426302,
 2868597, 126119, 5784857, 5245324, 10903900, 16436004, 3389013, 1742384,
 14674502, 10279218, 8536112, 10364279, 6877778, 14051163, 1025130, 6072469,
 1988305, 8354440, 8216060, 16342977, 13112639, 3976679, 5913576, 8816697,
 6879995, 14043764, 3339515, 9364420, 15808858, 12261651, 2141560, 5636398,
 10345425, 10414756, 781725, 6155650, 4746914, 5078683, 7469001, 6799140,
 10156444, 9667150, 10116470, 4133858, 2121972, 1124204, 1003577, 1611214,
 14304602, 16221850, 13878465, 13577744, 3629235, 8772583, 10881308, 2410386,
 7300044, 5378855, 9301235, 12755149, 4977682, 8083074, 10327581, 6395087,
 9155434, 15501696, 7514362, 14520507, 15808945, 3244584, 4741962, 9658130,
 14336147, 8654727, 7969093, 15759799, 14029445, 5038459, 9894848, 8659300,
 13699287, 8834306, 10712885, 14753895, 10410465, 3373251, 309501, 9561475,
 5526688, 14647426, 14209836, 5339224, 207299, 14069911, 8722990, 2290950,
 3258216, 12505185, 6007317, 9218111, 14661019, 10537428, 11731949, 9027003,
 6641507, 9490160, 200241, 9720425, 16277895, 10816638, 1554761, 10431375,
 7467528, 6790302, 3429078, 14633753, 14428997, 11463204, 3576212, 2003426,
 6123687, 820520, 9992513, 15784513, 5778891, 6428165, 8388607
};


#define V2_16  (0x1 << 16)
#define V2_19  (0x1 << 19)
#define V2_22  (0x1 << 22)

static int calculate_lfsr(int n)
{
	int index;

	if ((n >> 16) == 0)
		index = 0;
	else if (((n - V2_16) >> 19) == 0)
		index = ((n - V2_16) >> 12) + 1;
	else if (((n - V2_16 - V2_19) >> 22) == 0)
		index = ((n - V2_16 - V2_19) >> 15 ) + 1 + 128;
	else if (((n - V2_16 - V2_19 - V2_22) >> 24) == 0)
		index = ((n - V2_16 - V2_19 - V2_22) >> 18 ) + 1 + 256;
	else
		index = ENTRIES-1;

	
	if ((index >= ENTRIES) || (index < 0))
		index = ENTRIES-1;

	return initial_lfsr[index];
}

static int pm_rtas_activate_spu_profiling(u32 node)
{
	int ret, i;
	struct pm_signal pm_signal_local[NUM_SPUS_PER_NODE];

	for (i = 0; i < ARRAY_SIZE(pm_signal_local); i++) {
		pm_signal_local[i].cpu = node;
		pm_signal_local[i].signal_group = 41;
		
		pm_signal_local[i].bus_word = 1 << i / 2;
		
		pm_signal_local[i].sub_unit = i;
		pm_signal_local[i].bit = 63;
	}

	ret = rtas_ibm_cbe_perftools(SUBFUNC_ACTIVATE,
				     PASSTHRU_ENABLE, pm_signal_local,
				     (ARRAY_SIZE(pm_signal_local)
				      * sizeof(struct pm_signal)));

	if (unlikely(ret)) {
		printk(KERN_WARNING "%s: rtas returned: %d\n",
		       __func__, ret);
		return -EIO;
	}

	return 0;
}

#ifdef CONFIG_CPU_FREQ
static int
oprof_cpufreq_notify(struct notifier_block *nb, unsigned long val, void *data)
{
	int ret = 0;
	struct cpufreq_freqs *frq = data;
	if ((val == CPUFREQ_PRECHANGE && frq->old < frq->new) ||
	    (val == CPUFREQ_POSTCHANGE && frq->old > frq->new) ||
	    (val == CPUFREQ_RESUMECHANGE || val == CPUFREQ_SUSPENDCHANGE))
		set_spu_profiling_frequency(frq->new, spu_cycle_reset);
	return ret;
}

static struct notifier_block cpu_freq_notifier_block = {
	.notifier_call	= oprof_cpufreq_notify
};
#endif

static void cell_global_stop_spu_cycles(void)
{
	int subfunc, rtn_value;
	unsigned int lfsr_value;
	int cpu;

	oprofile_running = 0;
	smp_wmb();

#ifdef CONFIG_CPU_FREQ
	cpufreq_unregister_notifier(&cpu_freq_notifier_block,
				    CPUFREQ_TRANSITION_NOTIFIER);
#endif

	for_each_online_cpu(cpu) {
		if (cbe_get_hw_thread_id(cpu))
			continue;

		subfunc = 3;	
		lfsr_value = 0x8f100000;

		rtn_value = rtas_call(spu_rtas_token, 3, 1, NULL,
				      subfunc, cbe_cpu_to_node(cpu),
				      lfsr_value);

		if (unlikely(rtn_value != 0)) {
			printk(KERN_ERR
			       "%s: rtas call ibm,cbe-spu-perftools " \
			       "failed, return = %d\n",
			       __func__, rtn_value);
		}

		
		pm_rtas_reset_signals(cbe_cpu_to_node(cpu));
	}

	stop_spu_profiling_cycles();
}

static void cell_global_stop_spu_events(void)
{
	int cpu;
	oprofile_running = 0;

	stop_spu_profiling_events();
	smp_wmb();

	for_each_online_cpu(cpu) {
		if (cbe_get_hw_thread_id(cpu))
			continue;

		cbe_sync_irq(cbe_cpu_to_node(cpu));
		
		cbe_disable_pm(cpu);
		cbe_write_pm07_control(cpu, 0, 0);

		
		pm_rtas_reset_signals(cbe_cpu_to_node(cpu));

		
		cbe_disable_pm_interrupts(cpu);
	}
	del_timer_sync(&timer_spu_event_swap);
}

static void cell_global_stop_ppu(void)
{
	int cpu;

	del_timer_sync(&timer_virt_cntr);
	oprofile_running = 0;
	smp_wmb();

	for_each_online_cpu(cpu) {
		if (cbe_get_hw_thread_id(cpu))
			continue;

		cbe_sync_irq(cbe_cpu_to_node(cpu));
		
		cbe_disable_pm(cpu);

		
		pm_rtas_reset_signals(cbe_cpu_to_node(cpu));

		
		cbe_disable_pm_interrupts(cpu);
	}
}

static void cell_global_stop(void)
{
	if (profiling_mode == PPU_PROFILING)
		cell_global_stop_ppu();
	else if (profiling_mode == SPU_PROFILING_EVENTS)
		cell_global_stop_spu_events();
	else
		cell_global_stop_spu_cycles();
}

static int cell_global_start_spu_cycles(struct op_counter_config *ctr)
{
	int subfunc;
	unsigned int lfsr_value;
	int cpu;
	int ret;
	int rtas_error;
	unsigned int cpu_khzfreq = 0;

#ifdef CONFIG_CPU_FREQ
	ret = cpufreq_register_notifier(&cpu_freq_notifier_block,
					CPUFREQ_TRANSITION_NOTIFIER);
	if (ret < 0)
		
		printk(KERN_ERR "CPU freq change registration failed: %d\n",
		       ret);

	else
		cpu_khzfreq = cpufreq_quick_get(smp_processor_id());
#endif

	set_spu_profiling_frequency(cpu_khzfreq, spu_cycle_reset);

	for_each_online_cpu(cpu) {
		if (cbe_get_hw_thread_id(cpu))
			continue;

		cbe_write_pm(cpu, pm_control, 0);

		if (spu_cycle_reset > MAX_SPU_COUNT)
			
			lfsr_value = calculate_lfsr(MAX_SPU_COUNT-1);
		else
			lfsr_value = calculate_lfsr(spu_cycle_reset);

		
		if (lfsr_value == 0)
			lfsr_value = calculate_lfsr(1);

		lfsr_value = lfsr_value << 8; 

		
		ret = pm_rtas_activate_spu_profiling(cbe_cpu_to_node(cpu));

		if (unlikely(ret)) {
			rtas_error = ret;
			goto out;
		}


		subfunc = 2;	

		
		ret = rtas_call(spu_rtas_token, 3, 1, NULL, subfunc,
				cbe_cpu_to_node(cpu), lfsr_value);

		if (unlikely(ret != 0)) {
			printk(KERN_ERR
			       "%s: rtas call ibm,cbe-spu-perftools failed, " \
			       "return = %d\n", __func__, ret);
			rtas_error = -EIO;
			goto out;
		}
	}

	rtas_error = start_spu_profiling_cycles(spu_cycle_reset);
	if (rtas_error)
		goto out_stop;

	oprofile_running = 1;
	return 0;

out_stop:
	cell_global_stop_spu_cycles();	
out:
	return rtas_error;
}

static int cell_global_start_spu_events(struct op_counter_config *ctr)
{
	int cpu;
	u32 interrupt_mask = 0;
	int rtn = 0;

	hdw_thread = 0;



	for_each_online_cpu(cpu) {
		if (cbe_get_hw_thread_id(cpu))
			continue;

		if (ctr_enabled & 1) {
			cbe_write_ctr(cpu, 0, reset_value[0]);
			enable_ctr(cpu, 0, pm_regs.pm07_cntrl);
			interrupt_mask |=
				CBE_PM_CTR_OVERFLOW_INTR(0);
		} else {
			
			cbe_write_pm07_control(cpu, 0, 0);
		}

		cbe_get_and_clear_pm_interrupts(cpu);
		cbe_enable_pm_interrupts(cpu, hdw_thread, interrupt_mask);
		cbe_enable_pm(cpu);

		
		cbe_write_pm(cpu, trace_address, 0);
	}

	start_spu_event_swap();
	start_spu_profiling_events();
	oprofile_running = 1;
	smp_wmb();

	return rtn;
}

static int cell_global_start_ppu(struct op_counter_config *ctr)
{
	u32 cpu, i;
	u32 interrupt_mask = 0;

	for_each_online_cpu(cpu) {
		if (cbe_get_hw_thread_id(cpu))
			continue;

		interrupt_mask = 0;

		for (i = 0; i < num_counters; ++i) {
			if (ctr_enabled & (1 << i)) {
				cbe_write_ctr(cpu, i, reset_value[i]);
				enable_ctr(cpu, i, pm_regs.pm07_cntrl);
				interrupt_mask |= CBE_PM_CTR_OVERFLOW_INTR(i);
			} else {
				
				cbe_write_pm07_control(cpu, i, 0);
			}
		}

		cbe_get_and_clear_pm_interrupts(cpu);
		cbe_enable_pm_interrupts(cpu, hdw_thread, interrupt_mask);
		cbe_enable_pm(cpu);
	}

	virt_cntr_inter_mask = interrupt_mask;
	oprofile_running = 1;
	smp_wmb();

	start_virt_cntrs();

	return 0;
}

static int cell_global_start(struct op_counter_config *ctr)
{
	if (profiling_mode == SPU_PROFILING_CYCLES)
		return cell_global_start_spu_cycles(ctr);
	else if (profiling_mode == SPU_PROFILING_EVENTS)
		return cell_global_start_spu_events(ctr);
	else
		return cell_global_start_ppu(ctr);
}


/* The SPU interrupt handler
 *
 * SPU event profiling works as follows:
 * The pm_signal[0] holds the one SPU event to be measured.  It is routed on
 * the debug bus using word 0 or 1.  The value of pm_signal[1] and
 * pm_signal[2] contain the necessary events to route the SPU program
 * counter for the selected SPU onto the debug bus using words 2 and 3.
 * The pm_interval register is setup to write the SPU PC value into the
 * trace buffer at the maximum rate possible.  The trace buffer is configured
 * to store the PCs, wrapping when it is full.  The performance counter is
 * initialized to the max hardware count minus the number of events, N, between
 * samples.  Once the N events have occurred, a HW counter overflow occurs
 * causing the generation of a HW counter interrupt which also stops the
 * writing of the SPU PC values to the trace buffer.  Hence the last PC
 * written to the trace buffer is the SPU PC that we want.  Unfortunately,
 * we have to read from the beginning of the trace buffer to get to the
 * last value written.  We just hope the PPU has nothing better to do then
 * service this interrupt. The PC for the specific SPU being profiled is
 * extracted from the trace buffer processed and stored.  The trace buffer
 * is cleared, interrupts are cleared, the counter is reset to max - N.
 * A kernel timer is used to periodically call the routine spu_evnt_swap()
 * to switch to the next physical SPU in the node to profile in round robbin
 * order.  This way data is collected for all SPUs on the node. It does mean
 * that we need to use a relatively small value of N to ensure enough samples
 * on each SPU are collected each SPU is being profiled 1/8 of the time.
 * It may also be necessary to use a longer sample collection period.
 */
static void cell_handle_interrupt_spu(struct pt_regs *regs,
				      struct op_counter_config *ctr)
{
	u32 cpu, cpu_tmp;
	u64 trace_entry;
	u32 interrupt_mask;
	u64 trace_buffer[2];
	u64 last_trace_buffer;
	u32 sample;
	u32 trace_addr;
	unsigned long sample_array_lock_flags;
	int spu_num;
	unsigned long flags;

	cpu = smp_processor_id();
	spin_lock_irqsave(&cntr_lock, flags);

	cpu_tmp = cpu;
	cbe_disable_pm(cpu);

	interrupt_mask = cbe_get_and_clear_pm_interrupts(cpu);

	sample = 0xABCDEF;
	trace_entry = 0xfedcba;
	last_trace_buffer = 0xdeadbeaf;

	if ((oprofile_running == 1) && (interrupt_mask != 0)) {
		
		cbe_write_pm(cpu, pm_interval, 0);

		
		if ((interrupt_mask & CBE_PM_CTR_OVERFLOW_INTR(0))
		    && ctr[0].enabled)

			cbe_write_ctr(cpu, 0, reset_value[0]);

		trace_addr = cbe_read_pm(cpu, trace_address);

		while (!(trace_addr & CBE_PM_TRACE_BUF_EMPTY)) {

			cbe_read_trace_buffer(cpu, trace_buffer);
			trace_addr = cbe_read_pm(cpu, trace_address);
		}


		trace_entry = trace_buffer[0]
			& 0x00000000FFFF0000;

		sample = trace_entry >> 14;
		last_trace_buffer = trace_buffer[0];

		spu_num = spu_evnt_phys_spu_indx
			+ (cbe_cpu_to_node(cpu) * NUM_SPUS_PER_NODE);

		spin_lock_irqsave(&oprof_spu_smpl_arry_lck,
				  sample_array_lock_flags);
		spu_sync_buffer(spu_num, &sample, 1);
		spin_unlock_irqrestore(&oprof_spu_smpl_arry_lck,
				       sample_array_lock_flags);

		smp_wmb();    /* insure spu event buffer updates are written
			       * don't want events intermingled... */

		cbe_write_pm(cpu, pm_interval, NUM_INTERVAL_CYC);
		cbe_enable_pm_interrupts(cpu, hdw_thread,
					 virt_cntr_inter_mask);

		
		cbe_write_pm(cpu, trace_address, 0);
		cbe_write_pm(cpu, pm_interval, NUM_INTERVAL_CYC);

		write_pm_cntrl(cpu);
		cbe_enable_pm(cpu);
	}
	spin_unlock_irqrestore(&cntr_lock, flags);
}

static void cell_handle_interrupt_ppu(struct pt_regs *regs,
				      struct op_counter_config *ctr)
{
	u32 cpu;
	u64 pc;
	int is_kernel;
	unsigned long flags = 0;
	u32 interrupt_mask;
	int i;

	cpu = smp_processor_id();

	spin_lock_irqsave(&cntr_lock, flags);


	cbe_disable_pm(cpu);

	interrupt_mask = cbe_get_and_clear_pm_interrupts(cpu);


	if ((oprofile_running == 1) && (interrupt_mask != 0)) {
		pc = regs->nip;
		is_kernel = is_kernel_addr(pc);

		for (i = 0; i < num_counters; ++i) {
			if ((interrupt_mask & CBE_PM_CTR_OVERFLOW_INTR(i))
			    && ctr[i].enabled) {
				oprofile_add_ext_sample(pc, regs, i, is_kernel);
				cbe_write_ctr(cpu, i, reset_value[i]);
			}
		}

		cbe_enable_pm_interrupts(cpu, hdw_thread,
					 virt_cntr_inter_mask);

		cbe_enable_pm(cpu);
	}
	spin_unlock_irqrestore(&cntr_lock, flags);
}

static void cell_handle_interrupt(struct pt_regs *regs,
				  struct op_counter_config *ctr)
{
	if (profiling_mode == PPU_PROFILING)
		cell_handle_interrupt_ppu(regs, ctr);
	else
		cell_handle_interrupt_spu(regs, ctr);
}

static int cell_sync_start(void)
{
	if ((profiling_mode == SPU_PROFILING_CYCLES) ||
	    (profiling_mode == SPU_PROFILING_EVENTS))
		return spu_sync_start();
	else
		return DO_GENERIC_SYNC;
}

static int cell_sync_stop(void)
{
	if ((profiling_mode == SPU_PROFILING_CYCLES) ||
	    (profiling_mode == SPU_PROFILING_EVENTS))
		return spu_sync_stop();
	else
		return 1;
}

struct op_powerpc_model op_model_cell = {
	.reg_setup = cell_reg_setup,
	.cpu_setup = cell_cpu_setup,
	.global_start = cell_global_start,
	.global_stop = cell_global_stop,
	.sync_start = cell_sync_start,
	.sync_stop = cell_sync_stop,
	.handle_interrupt = cell_handle_interrupt,
};
