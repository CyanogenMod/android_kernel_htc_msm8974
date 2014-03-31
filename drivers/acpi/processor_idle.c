/*
 * processor_idle - idle state submodule to the ACPI processor driver
 *
 *  Copyright (C) 2001, 2002 Andy Grover <andrew.grover@intel.com>
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *  Copyright (C) 2004, 2005 Dominik Brodowski <linux@brodo.de>
 *  Copyright (C) 2004  Anil S Keshavamurthy <anil.s.keshavamurthy@intel.com>
 *  			- Added processor hotplug support
 *  Copyright (C) 2005  Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>
 *  			- Added support for C3 on SMP
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>	
#include <linux/pm_qos.h>
#include <linux/clockchips.h>
#include <linux/cpuidle.h>
#include <linux/irqflags.h>

#ifdef CONFIG_X86
#include <asm/apic.h>
#endif

#include <asm/io.h>
#include <asm/uaccess.h>

#include <acpi/acpi_bus.h>
#include <acpi/processor.h>
#include <asm/processor.h>

#define PREFIX "ACPI: "

#define ACPI_PROCESSOR_CLASS            "processor"
#define _COMPONENT              ACPI_PROCESSOR_COMPONENT
ACPI_MODULE_NAME("processor_idle");
#define PM_TIMER_TICK_NS		(1000000000ULL/PM_TIMER_FREQUENCY)
#define C2_OVERHEAD			1	
#define C3_OVERHEAD			1	
#define PM_TIMER_TICKS_TO_US(p)		(((p) * 1000)/(PM_TIMER_FREQUENCY/1000))

static unsigned int max_cstate __read_mostly = ACPI_PROCESSOR_MAX_POWER;
module_param(max_cstate, uint, 0000);
static unsigned int nocst __read_mostly;
module_param(nocst, uint, 0000);
static int bm_check_disable __read_mostly;
module_param(bm_check_disable, uint, 0000);

static unsigned int latency_factor __read_mostly = 2;
module_param(latency_factor, uint, 0644);

static int disabled_by_idle_boot_param(void)
{
	return boot_option_idle_override == IDLE_POLL ||
		boot_option_idle_override == IDLE_FORCE_MWAIT ||
		boot_option_idle_override == IDLE_HALT;
}

static int set_max_cstate(const struct dmi_system_id *id)
{
	if (max_cstate > ACPI_PROCESSOR_MAX_POWER)
		return 0;

	printk(KERN_NOTICE PREFIX "%s detected - limiting to C%ld max_cstate."
	       " Override with \"processor.max_cstate=%d\"\n", id->ident,
	       (long)id->driver_data, ACPI_PROCESSOR_MAX_POWER + 1);

	max_cstate = (long)id->driver_data;

	return 0;
}

static struct dmi_system_id __cpuinitdata processor_power_dmi_table[] = {
	{ set_max_cstate, "Clevo 5600D", {
	  DMI_MATCH(DMI_BIOS_VENDOR,"Phoenix Technologies LTD"),
	  DMI_MATCH(DMI_BIOS_VERSION,"SHE845M0.86C.0013.D.0302131307")},
	 (void *)2},
	{ set_max_cstate, "Pavilion zv5000", {
	  DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
	  DMI_MATCH(DMI_PRODUCT_NAME,"Pavilion zv5000 (DS502A#ABA)")},
	 (void *)1},
	{ set_max_cstate, "Asus L8400B", {
	  DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK Computer Inc."),
	  DMI_MATCH(DMI_PRODUCT_NAME,"L8400B series Notebook PC")},
	 (void *)1},
	{},
};


static void acpi_safe_halt(void)
{
	current_thread_info()->status &= ~TS_POLLING;
	smp_mb();
	if (!need_resched()) {
		safe_halt();
		local_irq_disable();
	}
	current_thread_info()->status |= TS_POLLING;
}

#ifdef ARCH_APICTIMER_STOPS_ON_C3

static void lapic_timer_check_state(int state, struct acpi_processor *pr,
				   struct acpi_processor_cx *cx)
{
	struct acpi_processor_power *pwr = &pr->power;
	u8 type = local_apic_timer_c2_ok ? ACPI_STATE_C3 : ACPI_STATE_C2;

	if (cpu_has(&cpu_data(pr->id), X86_FEATURE_ARAT))
		return;

	if (amd_e400_c1e_detected)
		type = ACPI_STATE_C1;

	if (pwr->timer_broadcast_on_state < state)
		return;

	if (cx->type >= type)
		pr->power.timer_broadcast_on_state = state;
}

static void __lapic_timer_propagate_broadcast(void *arg)
{
	struct acpi_processor *pr = (struct acpi_processor *) arg;
	unsigned long reason;

	reason = pr->power.timer_broadcast_on_state < INT_MAX ?
		CLOCK_EVT_NOTIFY_BROADCAST_ON : CLOCK_EVT_NOTIFY_BROADCAST_OFF;

	clockevents_notify(reason, &pr->id);
}

static void lapic_timer_propagate_broadcast(struct acpi_processor *pr)
{
	smp_call_function_single(pr->id, __lapic_timer_propagate_broadcast,
				 (void *)pr, 1);
}

static void lapic_timer_state_broadcast(struct acpi_processor *pr,
				       struct acpi_processor_cx *cx,
				       int broadcast)
{
	int state = cx - pr->power.states;

	if (state >= pr->power.timer_broadcast_on_state) {
		unsigned long reason;

		reason = broadcast ?  CLOCK_EVT_NOTIFY_BROADCAST_ENTER :
			CLOCK_EVT_NOTIFY_BROADCAST_EXIT;
		clockevents_notify(reason, &pr->id);
	}
}

#else

static void lapic_timer_check_state(int state, struct acpi_processor *pr,
				   struct acpi_processor_cx *cstate) { }
static void lapic_timer_propagate_broadcast(struct acpi_processor *pr) { }
static void lapic_timer_state_broadcast(struct acpi_processor *pr,
				       struct acpi_processor_cx *cx,
				       int broadcast)
{
}

#endif

static u32 saved_bm_rld;

static void acpi_idle_bm_rld_save(void)
{
	acpi_read_bit_register(ACPI_BITREG_BUS_MASTER_RLD, &saved_bm_rld);
}
static void acpi_idle_bm_rld_restore(void)
{
	u32 resumed_bm_rld;

	acpi_read_bit_register(ACPI_BITREG_BUS_MASTER_RLD, &resumed_bm_rld);

	if (resumed_bm_rld != saved_bm_rld)
		acpi_write_bit_register(ACPI_BITREG_BUS_MASTER_RLD, saved_bm_rld);
}

int acpi_processor_suspend(struct acpi_device * device, pm_message_t state)
{
	acpi_idle_bm_rld_save();
	return 0;
}

int acpi_processor_resume(struct acpi_device * device)
{
	acpi_idle_bm_rld_restore();
	return 0;
}

#if defined(CONFIG_X86)
static void tsc_check_state(int state)
{
	switch (boot_cpu_data.x86_vendor) {
	case X86_VENDOR_AMD:
	case X86_VENDOR_INTEL:
		if (boot_cpu_has(X86_FEATURE_NONSTOP_TSC))
			return;

		
	default:
		
		if (state > ACPI_STATE_C1)
			mark_tsc_unstable("TSC halts in idle");
	}
}
#else
static void tsc_check_state(int state) { return; }
#endif

static int acpi_processor_get_power_info_fadt(struct acpi_processor *pr)
{

	if (!pr)
		return -EINVAL;

	if (!pr->pblk)
		return -ENODEV;

	
	pr->power.states[ACPI_STATE_C2].type = ACPI_STATE_C2;
	pr->power.states[ACPI_STATE_C3].type = ACPI_STATE_C3;

#ifndef CONFIG_HOTPLUG_CPU
	if ((num_online_cpus() > 1) &&
	    !(acpi_gbl_FADT.flags & ACPI_FADT_C2_MP_SUPPORTED))
		return -ENODEV;
#endif

	
	pr->power.states[ACPI_STATE_C2].address = pr->pblk + 4;
	pr->power.states[ACPI_STATE_C3].address = pr->pblk + 5;

	
	pr->power.states[ACPI_STATE_C2].latency = acpi_gbl_FADT.C2latency;
	pr->power.states[ACPI_STATE_C3].latency = acpi_gbl_FADT.C3latency;

	if (acpi_gbl_FADT.C2latency > ACPI_PROCESSOR_MAX_C2_LATENCY) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			"C2 latency too large [%d]\n", acpi_gbl_FADT.C2latency));
		
		pr->power.states[ACPI_STATE_C2].address = 0;
	}

	if (acpi_gbl_FADT.C3latency > ACPI_PROCESSOR_MAX_C3_LATENCY) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			"C3 latency too large [%d]\n", acpi_gbl_FADT.C3latency));
		
		pr->power.states[ACPI_STATE_C3].address = 0;
	}

	ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			  "lvl2[0x%08x] lvl3[0x%08x]\n",
			  pr->power.states[ACPI_STATE_C2].address,
			  pr->power.states[ACPI_STATE_C3].address));

	return 0;
}

static int acpi_processor_get_power_info_default(struct acpi_processor *pr)
{
	if (!pr->power.states[ACPI_STATE_C1].valid) {
		
		
		pr->power.states[ACPI_STATE_C1].type = ACPI_STATE_C1;
		pr->power.states[ACPI_STATE_C1].valid = 1;
		pr->power.states[ACPI_STATE_C1].entry_method = ACPI_CSTATE_HALT;
	}
	
	pr->power.states[ACPI_STATE_C0].valid = 1;
	return 0;
}

static int acpi_processor_get_power_info_cst(struct acpi_processor *pr)
{
	acpi_status status = 0;
	u64 count;
	int current_count;
	int i;
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *cst;


	if (nocst)
		return -ENODEV;

	current_count = 0;

	status = acpi_evaluate_object(pr->handle, "_CST", NULL, &buffer);
	if (ACPI_FAILURE(status)) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "No _CST, giving up\n"));
		return -ENODEV;
	}

	cst = buffer.pointer;

	
	if (!cst || (cst->type != ACPI_TYPE_PACKAGE) || cst->package.count < 2) {
		printk(KERN_ERR PREFIX "not enough elements in _CST\n");
		status = -EFAULT;
		goto end;
	}

	count = cst->package.elements[0].integer.value;

	
	if (count < 1 || count != cst->package.count - 1) {
		printk(KERN_ERR PREFIX "count given by _CST is not valid\n");
		status = -EFAULT;
		goto end;
	}

	
	pr->flags.has_cst = 1;

	for (i = 1; i <= count; i++) {
		union acpi_object *element;
		union acpi_object *obj;
		struct acpi_power_register *reg;
		struct acpi_processor_cx cx;

		memset(&cx, 0, sizeof(cx));

		element = &(cst->package.elements[i]);
		if (element->type != ACPI_TYPE_PACKAGE)
			continue;

		if (element->package.count != 4)
			continue;

		obj = &(element->package.elements[0]);

		if (obj->type != ACPI_TYPE_BUFFER)
			continue;

		reg = (struct acpi_power_register *)obj->buffer.pointer;

		if (reg->space_id != ACPI_ADR_SPACE_SYSTEM_IO &&
		    (reg->space_id != ACPI_ADR_SPACE_FIXED_HARDWARE))
			continue;

		
		obj = &(element->package.elements[1]);
		if (obj->type != ACPI_TYPE_INTEGER)
			continue;

		cx.type = obj->integer.value;
		if (i == 1 && cx.type != ACPI_STATE_C1)
			current_count++;

		cx.address = reg->address;
		cx.index = current_count + 1;

		cx.entry_method = ACPI_CSTATE_SYSTEMIO;
		if (reg->space_id == ACPI_ADR_SPACE_FIXED_HARDWARE) {
			if (acpi_processor_ffh_cstate_probe
					(pr->id, &cx, reg) == 0) {
				cx.entry_method = ACPI_CSTATE_FFH;
			} else if (cx.type == ACPI_STATE_C1) {
				cx.entry_method = ACPI_CSTATE_HALT;
				snprintf(cx.desc, ACPI_CX_DESC_LEN, "ACPI HLT");
			} else {
				continue;
			}
			if (cx.type == ACPI_STATE_C1 &&
			    (boot_option_idle_override == IDLE_NOMWAIT)) {
				cx.entry_method = ACPI_CSTATE_HALT;
				snprintf(cx.desc, ACPI_CX_DESC_LEN, "ACPI HLT");
			}
		} else {
			snprintf(cx.desc, ACPI_CX_DESC_LEN, "ACPI IOPORT 0x%x",
				 cx.address);
		}

		if (cx.type == ACPI_STATE_C1) {
			cx.valid = 1;
		}

		obj = &(element->package.elements[2]);
		if (obj->type != ACPI_TYPE_INTEGER)
			continue;

		cx.latency = obj->integer.value;

		obj = &(element->package.elements[3]);
		if (obj->type != ACPI_TYPE_INTEGER)
			continue;

		cx.power = obj->integer.value;

		current_count++;
		memcpy(&(pr->power.states[current_count]), &cx, sizeof(cx));

		if (current_count >= (ACPI_PROCESSOR_MAX_POWER - 1)) {
			printk(KERN_WARNING
			       "Limiting number of power states to max (%d)\n",
			       ACPI_PROCESSOR_MAX_POWER);
			printk(KERN_WARNING
			       "Please increase ACPI_PROCESSOR_MAX_POWER if needed.\n");
			break;
		}
	}

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Found %d power states\n",
			  current_count));

	
	if (current_count < 2)
		status = -EFAULT;

      end:
	kfree(buffer.pointer);

	return status;
}

static void acpi_processor_power_verify_c3(struct acpi_processor *pr,
					   struct acpi_processor_cx *cx)
{
	static int bm_check_flag = -1;
	static int bm_control_flag = -1;


	if (!cx->address)
		return;

	else if (errata.piix4.fdma) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				  "C3 not supported on PIIX4 with Type-F DMA\n"));
		return;
	}

	
	if (bm_check_flag == -1) {
		
		acpi_processor_power_init_bm_check(&(pr->flags), pr->id);
		bm_check_flag = pr->flags.bm_check;
		bm_control_flag = pr->flags.bm_control;
	} else {
		pr->flags.bm_check = bm_check_flag;
		pr->flags.bm_control = bm_control_flag;
	}

	if (pr->flags.bm_check) {
		if (!pr->flags.bm_control) {
			if (pr->flags.has_cst != 1) {
				
				ACPI_DEBUG_PRINT((ACPI_DB_INFO,
					"C3 support requires BM control\n"));
				return;
			} else {
				
				ACPI_DEBUG_PRINT((ACPI_DB_INFO,
					"C3 support without BM control\n"));
			}
		}
	} else {
		if (!(acpi_gbl_FADT.flags & ACPI_FADT_WBINVD)) {
			ACPI_DEBUG_PRINT((ACPI_DB_INFO,
					  "Cache invalidation should work properly"
					  " for C3 to be enabled on SMP systems\n"));
			return;
		}
	}

	cx->valid = 1;

	cx->latency_ticks = cx->latency;
	acpi_write_bit_register(ACPI_BITREG_BUS_MASTER_RLD, 1);

	return;
}

static int acpi_processor_power_verify(struct acpi_processor *pr)
{
	unsigned int i;
	unsigned int working = 0;

	pr->power.timer_broadcast_on_state = INT_MAX;

	for (i = 1; i < ACPI_PROCESSOR_MAX_POWER && i <= max_cstate; i++) {
		struct acpi_processor_cx *cx = &pr->power.states[i];

		switch (cx->type) {
		case ACPI_STATE_C1:
			cx->valid = 1;
			break;

		case ACPI_STATE_C2:
			if (!cx->address)
				break;
			cx->valid = 1; 
			cx->latency_ticks = cx->latency; 
			break;

		case ACPI_STATE_C3:
			acpi_processor_power_verify_c3(pr, cx);
			break;
		}
		if (!cx->valid)
			continue;

		lapic_timer_check_state(i, pr, cx);
		tsc_check_state(cx->type);
		working++;
	}

	lapic_timer_propagate_broadcast(pr);

	return (working);
}

static int acpi_processor_get_power_info(struct acpi_processor *pr)
{
	unsigned int i;
	int result;



	
	memset(pr->power.states, 0, sizeof(pr->power.states));

	result = acpi_processor_get_power_info_cst(pr);
	if (result == -ENODEV)
		result = acpi_processor_get_power_info_fadt(pr);

	if (result)
		return result;

	acpi_processor_get_power_info_default(pr);

	pr->power.count = acpi_processor_power_verify(pr);

	for (i = 1; i < ACPI_PROCESSOR_MAX_POWER; i++) {
		if (pr->power.states[i].valid) {
			pr->power.count = i;
			if (pr->power.states[i].type >= ACPI_STATE_C2)
				pr->flags.power = 1;
		}
	}

	return 0;
}

static int acpi_idle_bm_check(void)
{
	u32 bm_status = 0;

	if (bm_check_disable)
		return 0;

	acpi_read_bit_register(ACPI_BITREG_BUS_MASTER_STATUS, &bm_status);
	if (bm_status)
		acpi_write_bit_register(ACPI_BITREG_BUS_MASTER_STATUS, 1);
	else if (errata.piix4.bmisx) {
		if ((inb_p(errata.piix4.bmisx + 0x02) & 0x01)
		    || (inb_p(errata.piix4.bmisx + 0x0A) & 0x01))
			bm_status = 1;
	}
	return bm_status;
}

static inline void acpi_idle_do_entry(struct acpi_processor_cx *cx)
{
	
	stop_critical_timings();
	if (cx->entry_method == ACPI_CSTATE_FFH) {
		
		acpi_processor_ffh_cstate_enter(cx);
	} else if (cx->entry_method == ACPI_CSTATE_HALT) {
		acpi_safe_halt();
	} else {
		
		inb(cx->address);
		inl(acpi_gbl_FADT.xpm_timer_block.address);
	}
	start_critical_timings();
}

static int acpi_idle_enter_c1(struct cpuidle_device *dev,
		struct cpuidle_driver *drv, int index)
{
	ktime_t  kt1, kt2;
	s64 idle_time;
	struct acpi_processor *pr;
	struct cpuidle_state_usage *state_usage = &dev->states_usage[index];
	struct acpi_processor_cx *cx = cpuidle_get_statedata(state_usage);

	pr = __this_cpu_read(processors);
	dev->last_residency = 0;

	if (unlikely(!pr))
		return -EINVAL;

	local_irq_disable();

	lapic_timer_state_broadcast(pr, cx, 1);
	kt1 = ktime_get_real();
	acpi_idle_do_entry(cx);
	kt2 = ktime_get_real();
	idle_time =  ktime_to_us(ktime_sub(kt2, kt1));

	
	dev->last_residency = (int)idle_time;

	local_irq_enable();
	cx->usage++;
	lapic_timer_state_broadcast(pr, cx, 0);

	return index;
}


static int acpi_idle_play_dead(struct cpuidle_device *dev, int index)
{
	struct cpuidle_state_usage *state_usage = &dev->states_usage[index];
	struct acpi_processor_cx *cx = cpuidle_get_statedata(state_usage);

	ACPI_FLUSH_CPU_CACHE();

	while (1) {

		if (cx->entry_method == ACPI_CSTATE_HALT)
			safe_halt();
		else if (cx->entry_method == ACPI_CSTATE_SYSTEMIO) {
			inb(cx->address);
			
			inl(acpi_gbl_FADT.xpm_timer_block.address);
		} else
			return -ENODEV;
	}

	
	return 0;
}

static int acpi_idle_enter_simple(struct cpuidle_device *dev,
		struct cpuidle_driver *drv, int index)
{
	struct acpi_processor *pr;
	struct cpuidle_state_usage *state_usage = &dev->states_usage[index];
	struct acpi_processor_cx *cx = cpuidle_get_statedata(state_usage);
	ktime_t  kt1, kt2;
	s64 idle_time_ns;
	s64 idle_time;

	pr = __this_cpu_read(processors);
	dev->last_residency = 0;

	if (unlikely(!pr))
		return -EINVAL;

	local_irq_disable();

	if (cx->entry_method != ACPI_CSTATE_FFH) {
		current_thread_info()->status &= ~TS_POLLING;
		smp_mb();

		if (unlikely(need_resched())) {
			current_thread_info()->status |= TS_POLLING;
			local_irq_enable();
			return -EINVAL;
		}
	}

	lapic_timer_state_broadcast(pr, cx, 1);

	if (cx->type == ACPI_STATE_C3)
		ACPI_FLUSH_CPU_CACHE();

	kt1 = ktime_get_real();
	
	sched_clock_idle_sleep_event();
	acpi_idle_do_entry(cx);
	kt2 = ktime_get_real();
	idle_time_ns = ktime_to_ns(ktime_sub(kt2, kt1));
	idle_time = idle_time_ns;
	do_div(idle_time, NSEC_PER_USEC);

	
	dev->last_residency = (int)idle_time;

	
	sched_clock_idle_wakeup_event(idle_time_ns);

	local_irq_enable();
	if (cx->entry_method != ACPI_CSTATE_FFH)
		current_thread_info()->status |= TS_POLLING;

	cx->usage++;

	lapic_timer_state_broadcast(pr, cx, 0);
	cx->time += idle_time;
	return index;
}

static int c3_cpu_count;
static DEFINE_RAW_SPINLOCK(c3_lock);

static int acpi_idle_enter_bm(struct cpuidle_device *dev,
		struct cpuidle_driver *drv, int index)
{
	struct acpi_processor *pr;
	struct cpuidle_state_usage *state_usage = &dev->states_usage[index];
	struct acpi_processor_cx *cx = cpuidle_get_statedata(state_usage);
	ktime_t  kt1, kt2;
	s64 idle_time_ns;
	s64 idle_time;


	pr = __this_cpu_read(processors);
	dev->last_residency = 0;

	if (unlikely(!pr))
		return -EINVAL;

	if (!cx->bm_sts_skip && acpi_idle_bm_check()) {
		if (drv->safe_state_index >= 0) {
			return drv->states[drv->safe_state_index].enter(dev,
						drv, drv->safe_state_index);
		} else {
			local_irq_disable();
			acpi_safe_halt();
			local_irq_enable();
			return -EINVAL;
		}
	}

	local_irq_disable();

	if (cx->entry_method != ACPI_CSTATE_FFH) {
		current_thread_info()->status &= ~TS_POLLING;
		smp_mb();

		if (unlikely(need_resched())) {
			current_thread_info()->status |= TS_POLLING;
			local_irq_enable();
			return -EINVAL;
		}
	}

	acpi_unlazy_tlb(smp_processor_id());

	
	sched_clock_idle_sleep_event();
	lapic_timer_state_broadcast(pr, cx, 1);

	kt1 = ktime_get_real();
	if (pr->flags.bm_check && pr->flags.bm_control) {
		raw_spin_lock(&c3_lock);
		c3_cpu_count++;
		
		if (c3_cpu_count == num_online_cpus())
			acpi_write_bit_register(ACPI_BITREG_ARB_DISABLE, 1);
		raw_spin_unlock(&c3_lock);
	} else if (!pr->flags.bm_check) {
		ACPI_FLUSH_CPU_CACHE();
	}

	acpi_idle_do_entry(cx);

	
	if (pr->flags.bm_check && pr->flags.bm_control) {
		raw_spin_lock(&c3_lock);
		acpi_write_bit_register(ACPI_BITREG_ARB_DISABLE, 0);
		c3_cpu_count--;
		raw_spin_unlock(&c3_lock);
	}
	kt2 = ktime_get_real();
	idle_time_ns = ktime_to_ns(ktime_sub(kt2, kt1));
	idle_time = idle_time_ns;
	do_div(idle_time, NSEC_PER_USEC);

	
	dev->last_residency = (int)idle_time;

	
	sched_clock_idle_wakeup_event(idle_time_ns);

	local_irq_enable();
	if (cx->entry_method != ACPI_CSTATE_FFH)
		current_thread_info()->status |= TS_POLLING;

	cx->usage++;

	lapic_timer_state_broadcast(pr, cx, 0);
	cx->time += idle_time;
	return index;
}

struct cpuidle_driver acpi_idle_driver = {
	.name =		"acpi_idle",
	.owner =	THIS_MODULE,
};

static int acpi_processor_setup_cpuidle_cx(struct acpi_processor *pr)
{
	int i, count = CPUIDLE_DRIVER_STATE_START;
	struct acpi_processor_cx *cx;
	struct cpuidle_state_usage *state_usage;
	struct cpuidle_device *dev = &pr->power.dev;

	if (!pr->flags.power_setup_done)
		return -EINVAL;

	if (pr->flags.power == 0) {
		return -EINVAL;
	}

	dev->cpu = pr->id;

	if (max_cstate == 0)
		max_cstate = 1;

	for (i = 1; i < ACPI_PROCESSOR_MAX_POWER && i <= max_cstate; i++) {
		cx = &pr->power.states[i];
		state_usage = &dev->states_usage[count];

		if (!cx->valid)
			continue;

#ifdef CONFIG_HOTPLUG_CPU
		if ((cx->type != ACPI_STATE_C1) && (num_online_cpus() > 1) &&
		    !pr->flags.has_cst &&
		    !(acpi_gbl_FADT.flags & ACPI_FADT_C2_MP_SUPPORTED))
			continue;
#endif

		cpuidle_set_statedata(state_usage, cx);

		count++;
		if (count == CPUIDLE_STATE_MAX)
			break;
	}

	dev->state_count = count;

	if (!count)
		return -EINVAL;

	return 0;
}

static int acpi_processor_setup_cpuidle_states(struct acpi_processor *pr)
{
	int i, count = CPUIDLE_DRIVER_STATE_START;
	struct acpi_processor_cx *cx;
	struct cpuidle_state *state;
	struct cpuidle_driver *drv = &acpi_idle_driver;

	if (!pr->flags.power_setup_done)
		return -EINVAL;

	if (pr->flags.power == 0)
		return -EINVAL;

	drv->safe_state_index = -1;
	for (i = 0; i < CPUIDLE_STATE_MAX; i++) {
		drv->states[i].name[0] = '\0';
		drv->states[i].desc[0] = '\0';
	}

	if (max_cstate == 0)
		max_cstate = 1;

	for (i = 1; i < ACPI_PROCESSOR_MAX_POWER && i <= max_cstate; i++) {
		cx = &pr->power.states[i];

		if (!cx->valid)
			continue;

#ifdef CONFIG_HOTPLUG_CPU
		if ((cx->type != ACPI_STATE_C1) && (num_online_cpus() > 1) &&
		    !pr->flags.has_cst &&
		    !(acpi_gbl_FADT.flags & ACPI_FADT_C2_MP_SUPPORTED))
			continue;
#endif

		state = &drv->states[count];
		snprintf(state->name, CPUIDLE_NAME_LEN, "C%d", i);
		strncpy(state->desc, cx->desc, CPUIDLE_DESC_LEN);
		state->exit_latency = cx->latency;
		state->target_residency = cx->latency * latency_factor;

		state->flags = 0;
		switch (cx->type) {
			case ACPI_STATE_C1:
			if (cx->entry_method == ACPI_CSTATE_FFH)
				state->flags |= CPUIDLE_FLAG_TIME_VALID;

			state->enter = acpi_idle_enter_c1;
			state->enter_dead = acpi_idle_play_dead;
			drv->safe_state_index = count;
			break;

			case ACPI_STATE_C2:
			state->flags |= CPUIDLE_FLAG_TIME_VALID;
			state->enter = acpi_idle_enter_simple;
			state->enter_dead = acpi_idle_play_dead;
			drv->safe_state_index = count;
			break;

			case ACPI_STATE_C3:
			state->flags |= CPUIDLE_FLAG_TIME_VALID;
			state->enter = pr->flags.bm_check ?
					acpi_idle_enter_bm :
					acpi_idle_enter_simple;
			break;
		}

		count++;
		if (count == CPUIDLE_STATE_MAX)
			break;
	}

	drv->state_count = count;

	if (!count)
		return -EINVAL;

	return 0;
}

int acpi_processor_hotplug(struct acpi_processor *pr)
{
	int ret = 0;

	if (disabled_by_idle_boot_param())
		return 0;

	if (!pr)
		return -EINVAL;

	if (nocst) {
		return -ENODEV;
	}

	if (!pr->flags.power_setup_done)
		return -ENODEV;

	cpuidle_pause_and_lock();
	cpuidle_disable_device(&pr->power.dev);
	acpi_processor_get_power_info(pr);
	if (pr->flags.power) {
		acpi_processor_setup_cpuidle_cx(pr);
		ret = cpuidle_enable_device(&pr->power.dev);
	}
	cpuidle_resume_and_unlock();

	return ret;
}

int acpi_processor_cst_has_changed(struct acpi_processor *pr)
{
	int cpu;
	struct acpi_processor *_pr;

	if (disabled_by_idle_boot_param())
		return 0;

	if (!pr)
		return -EINVAL;

	if (nocst)
		return -ENODEV;

	if (!pr->flags.power_setup_done)
		return -ENODEV;


	if (pr->id == 0 && cpuidle_get_driver() == &acpi_idle_driver) {

		cpuidle_pause_and_lock();
		
		get_online_cpus();

		
		for_each_online_cpu(cpu) {
			_pr = per_cpu(processors, cpu);
			if (!_pr || !_pr->flags.power_setup_done)
				continue;
			cpuidle_disable_device(&_pr->power.dev);
		}

		
		acpi_processor_setup_cpuidle_states(pr);

		
		for_each_online_cpu(cpu) {
			_pr = per_cpu(processors, cpu);
			if (!_pr || !_pr->flags.power_setup_done)
				continue;
			acpi_processor_get_power_info(_pr);
			if (_pr->flags.power) {
				acpi_processor_setup_cpuidle_cx(_pr);
				cpuidle_enable_device(&_pr->power.dev);
			}
		}
		put_online_cpus();
		cpuidle_resume_and_unlock();
	}

	return 0;
}

static int acpi_processor_registered;

int __cpuinit acpi_processor_power_init(struct acpi_processor *pr,
			      struct acpi_device *device)
{
	acpi_status status = 0;
	int retval;
	static int first_run;

	if (disabled_by_idle_boot_param())
		return 0;

	if (!first_run) {
		dmi_check_system(processor_power_dmi_table);
		max_cstate = acpi_processor_cstate_check(max_cstate);
		if (max_cstate < ACPI_C_STATES_MAX)
			printk(KERN_NOTICE
			       "ACPI: processor limited to max C-state %d\n",
			       max_cstate);
		first_run++;
	}

	if (!pr)
		return -EINVAL;

	if (acpi_gbl_FADT.cst_control && !nocst) {
		status =
		    acpi_os_write_port(acpi_gbl_FADT.smi_command, acpi_gbl_FADT.cst_control, 8);
		if (ACPI_FAILURE(status)) {
			ACPI_EXCEPTION((AE_INFO, status,
					"Notifying BIOS of _CST ability failed"));
		}
	}

	acpi_processor_get_power_info(pr);
	pr->flags.power_setup_done = 1;

	if (pr->flags.power) {
		
		if (!acpi_processor_registered) {
			acpi_processor_setup_cpuidle_states(pr);
			retval = cpuidle_register_driver(&acpi_idle_driver);
			if (retval)
				return retval;
			printk(KERN_DEBUG "ACPI: %s registered with cpuidle\n",
					acpi_idle_driver.name);
		}
		acpi_processor_setup_cpuidle_cx(pr);
		retval = cpuidle_register_device(&pr->power.dev);
		if (retval) {
			if (acpi_processor_registered == 0)
				cpuidle_unregister_driver(&acpi_idle_driver);
			return retval;
		}
		acpi_processor_registered++;
	}
	return 0;
}

int acpi_processor_power_exit(struct acpi_processor *pr,
			      struct acpi_device *device)
{
	if (disabled_by_idle_boot_param())
		return 0;

	if (pr->flags.power) {
		cpuidle_unregister_device(&pr->power.dev);
		acpi_processor_registered--;
		if (acpi_processor_registered == 0)
			cpuidle_unregister_driver(&acpi_idle_driver);
	}

	pr->flags.power_setup_done = 0;
	return 0;
}
