/*
 * (C) 2002 - 2003 Dominik Brodowski <linux@brodo.de>
 *
 *  Licensed under the terms of the GNU GPL License version 2.
 *
 *  Library for common functions for Intel SpeedStep v.1 and v.2 support
 *
 *  BIG FAT DISCLAIMER: Work in progress code. Possibly *dangerous*
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/cpufreq.h>

#include <asm/msr.h>
#include <asm/tsc.h>
#include "speedstep-lib.h"

#define PFX "speedstep-lib: "

#ifdef CONFIG_X86_SPEEDSTEP_RELAXED_CAP_CHECK
static int relaxed_check;
#else
#define relaxed_check 0
#endif


static unsigned int pentium3_get_frequency(enum speedstep_processor processor)
{
	
	struct {
		unsigned int ratio;	
		u8 bitmap;		
	} msr_decode_mult[] = {
		{ 30, 0x01 },
		{ 35, 0x05 },
		{ 40, 0x02 },
		{ 45, 0x06 },
		{ 50, 0x00 },
		{ 55, 0x04 },
		{ 60, 0x0b },
		{ 65, 0x0f },
		{ 70, 0x09 },
		{ 75, 0x0d },
		{ 80, 0x0a },
		{ 85, 0x26 },
		{ 90, 0x20 },
		{ 100, 0x2b },
		{ 0, 0xff }	
	};

	
	struct {
		unsigned int value;	
		u8 bitmap;		
	} msr_decode_fsb[] = {
		{  66, 0x0 },
		{ 100, 0x2 },
		{ 133, 0x1 },
		{   0, 0xff}
	};

	u32 msr_lo, msr_tmp;
	int i = 0, j = 0;

	
	rdmsr(MSR_IA32_EBL_CR_POWERON, msr_lo, msr_tmp);
	pr_debug("P3 - MSR_IA32_EBL_CR_POWERON: 0x%x 0x%x\n", msr_lo, msr_tmp);
	msr_tmp = msr_lo;

	
	msr_tmp &= 0x00c0000;
	msr_tmp >>= 18;
	while (msr_tmp != msr_decode_fsb[i].bitmap) {
		if (msr_decode_fsb[i].bitmap == 0xff)
			return 0;
		i++;
	}

	
	if (processor == SPEEDSTEP_CPU_PIII_C_EARLY) {
		pr_debug("workaround for early PIIIs\n");
		msr_lo &= 0x03c00000;
	} else
		msr_lo &= 0x0bc00000;
	msr_lo >>= 22;
	while (msr_lo != msr_decode_mult[j].bitmap) {
		if (msr_decode_mult[j].bitmap == 0xff)
			return 0;
		j++;
	}

	pr_debug("speed is %u\n",
		(msr_decode_mult[j].ratio * msr_decode_fsb[i].value * 100));

	return msr_decode_mult[j].ratio * msr_decode_fsb[i].value * 100;
}


static unsigned int pentiumM_get_frequency(void)
{
	u32 msr_lo, msr_tmp;

	rdmsr(MSR_IA32_EBL_CR_POWERON, msr_lo, msr_tmp);
	pr_debug("PM - MSR_IA32_EBL_CR_POWERON: 0x%x 0x%x\n", msr_lo, msr_tmp);

	
	if (msr_lo & 0x00040000) {
		printk(KERN_DEBUG PFX "PM - invalid FSB: 0x%x 0x%x\n",
				msr_lo, msr_tmp);
		return 0;
	}

	msr_tmp = (msr_lo >> 22) & 0x1f;
	pr_debug("bits 22-26 are 0x%x, speed is %u\n",
			msr_tmp, (msr_tmp * 100 * 1000));

	return msr_tmp * 100 * 1000;
}

static unsigned int pentium_core_get_frequency(void)
{
	u32 fsb = 0;
	u32 msr_lo, msr_tmp;
	int ret;

	rdmsr(MSR_FSB_FREQ, msr_lo, msr_tmp);
	
	switch (msr_lo & 0x07) {
	case 5:
		fsb = 100000;
		break;
	case 1:
		fsb = 133333;
		break;
	case 3:
		fsb = 166667;
		break;
	case 2:
		fsb = 200000;
		break;
	case 0:
		fsb = 266667;
		break;
	case 4:
		fsb = 333333;
		break;
	default:
		printk(KERN_ERR "PCORE - MSR_FSB_FREQ undefined value");
	}

	rdmsr(MSR_IA32_EBL_CR_POWERON, msr_lo, msr_tmp);
	pr_debug("PCORE - MSR_IA32_EBL_CR_POWERON: 0x%x 0x%x\n",
			msr_lo, msr_tmp);

	msr_tmp = (msr_lo >> 22) & 0x1f;
	pr_debug("bits 22-26 are 0x%x, speed is %u\n",
			msr_tmp, (msr_tmp * fsb));

	ret = (msr_tmp * fsb);
	return ret;
}


static unsigned int pentium4_get_frequency(void)
{
	struct cpuinfo_x86 *c = &boot_cpu_data;
	u32 msr_lo, msr_hi, mult;
	unsigned int fsb = 0;
	unsigned int ret;
	u8 fsb_code;

	if (c->x86_model < 2)
		return cpu_khz;

	rdmsr(0x2c, msr_lo, msr_hi);

	pr_debug("P4 - MSR_EBC_FREQUENCY_ID: 0x%x 0x%x\n", msr_lo, msr_hi);

	fsb_code = (msr_lo >> 16) & 0x7;
	switch (fsb_code) {
	case 0:
		fsb = 100 * 1000;
		break;
	case 1:
		fsb = 13333 * 10;
		break;
	case 2:
		fsb = 200 * 1000;
		break;
	}

	if (!fsb)
		printk(KERN_DEBUG PFX "couldn't detect FSB speed. "
				"Please send an e-mail to <linux@brodo.de>\n");

	
	mult = msr_lo >> 24;

	pr_debug("P4 - FSB %u kHz; Multiplier %u; Speed %u kHz\n",
			fsb, mult, (fsb * mult));

	ret = (fsb * mult);
	return ret;
}


unsigned int speedstep_get_frequency(enum speedstep_processor processor)
{
	switch (processor) {
	case SPEEDSTEP_CPU_PCORE:
		return pentium_core_get_frequency();
	case SPEEDSTEP_CPU_PM:
		return pentiumM_get_frequency();
	case SPEEDSTEP_CPU_P4D:
	case SPEEDSTEP_CPU_P4M:
		return pentium4_get_frequency();
	case SPEEDSTEP_CPU_PIII_T:
	case SPEEDSTEP_CPU_PIII_C:
	case SPEEDSTEP_CPU_PIII_C_EARLY:
		return pentium3_get_frequency(processor);
	default:
		return 0;
	};
	return 0;
}
EXPORT_SYMBOL_GPL(speedstep_get_frequency);



unsigned int speedstep_detect_processor(void)
{
	struct cpuinfo_x86 *c = &cpu_data(0);
	u32 ebx, msr_lo, msr_hi;

	pr_debug("x86: %x, model: %x\n", c->x86, c->x86_model);

	if ((c->x86_vendor != X86_VENDOR_INTEL) ||
	    ((c->x86 != 6) && (c->x86 != 0xF)))
		return 0;

	if (c->x86 == 0xF) {
		if (c->x86_model != 2)
			return 0;

		ebx = cpuid_ebx(0x00000001);
		ebx &= 0x000000FF;

		pr_debug("ebx value is %x, x86_mask is %x\n", ebx, c->x86_mask);

		switch (c->x86_mask) {
		case 4:
			if ((ebx == 0x0e) || (ebx == 0x0f))
				return SPEEDSTEP_CPU_P4M;
			break;
		case 7:
			if (ebx == 0x0e)
				return SPEEDSTEP_CPU_P4M;
			break;
		case 9:
			if ((ebx == 0x0e) ||
				(strstr(c->x86_model_id,
				    "Mobile Intel(R) Pentium(R) 4") != NULL))
				return SPEEDSTEP_CPU_P4M;
			break;
		default:
			break;
		}
		return 0;
	}

	switch (c->x86_model) {
	case 0x0B: 
		ebx = cpuid_ebx(0x00000001);
		pr_debug("ebx is %x\n", ebx);

		ebx &= 0x000000FF;

		if (ebx != 0x06)
			return 0;

		return SPEEDSTEP_CPU_PIII_T;

	case 0x08: 

		rdmsr(MSR_IA32_EBL_CR_POWERON, msr_lo, msr_hi);
		pr_debug("Coppermine: MSR_IA32_EBL_CR_POWERON is 0x%x, 0x%x\n",
				msr_lo, msr_hi);
		msr_lo &= 0x00c0000;
		if (msr_lo != 0x0080000)
			return 0;

		rdmsr(MSR_IA32_PLATFORM_ID, msr_lo, msr_hi);
		pr_debug("Coppermine: MSR_IA32_PLATFORM ID is 0x%x, 0x%x\n",
				msr_lo, msr_hi);
		if ((msr_hi & (1<<18)) &&
		    (relaxed_check ? 1 : (msr_hi & (3<<24)))) {
			if (c->x86_mask == 0x01) {
				pr_debug("early PIII version\n");
				return SPEEDSTEP_CPU_PIII_C_EARLY;
			} else
				return SPEEDSTEP_CPU_PIII_C;
		}

	default:
		return 0;
	}
}
EXPORT_SYMBOL_GPL(speedstep_detect_processor);



unsigned int speedstep_get_freqs(enum speedstep_processor processor,
				  unsigned int *low_speed,
				  unsigned int *high_speed,
				  unsigned int *transition_latency,
				  void (*set_state) (unsigned int state))
{
	unsigned int prev_speed;
	unsigned int ret = 0;
	unsigned long flags;
	struct timeval tv1, tv2;

	if ((!processor) || (!low_speed) || (!high_speed) || (!set_state))
		return -EINVAL;

	pr_debug("trying to determine both speeds\n");

	
	prev_speed = speedstep_get_frequency(processor);
	if (!prev_speed)
		return -EIO;

	pr_debug("previous speed is %u\n", prev_speed);

	local_irq_save(flags);

	
	set_state(SPEEDSTEP_LOW);
	*low_speed = speedstep_get_frequency(processor);
	if (!*low_speed) {
		ret = -EIO;
		goto out;
	}

	pr_debug("low speed is %u\n", *low_speed);

	
	if (transition_latency)
		do_gettimeofday(&tv1);

	
	set_state(SPEEDSTEP_HIGH);

	
	if (transition_latency)
		do_gettimeofday(&tv2);

	*high_speed = speedstep_get_frequency(processor);
	if (!*high_speed) {
		ret = -EIO;
		goto out;
	}

	pr_debug("high speed is %u\n", *high_speed);

	if (*low_speed == *high_speed) {
		ret = -ENODEV;
		goto out;
	}

	
	if (*high_speed != prev_speed)
		set_state(SPEEDSTEP_LOW);

	if (transition_latency) {
		*transition_latency = (tv2.tv_sec - tv1.tv_sec) * USEC_PER_SEC +
			tv2.tv_usec - tv1.tv_usec;
		pr_debug("transition latency is %u uSec\n", *transition_latency);

		
		*transition_latency *= 1200;

		if (*transition_latency > 10000000 ||
		    *transition_latency < 50000) {
			printk(KERN_WARNING PFX "frequency transition "
					"measured seems out of range (%u "
					"nSec), falling back to a safe one of"
					"%u nSec.\n",
					*transition_latency, 500000);
			*transition_latency = 500000;
		}
	}

out:
	local_irq_restore(flags);
	return ret;
}
EXPORT_SYMBOL_GPL(speedstep_get_freqs);

#ifdef CONFIG_X86_SPEEDSTEP_RELAXED_CAP_CHECK
module_param(relaxed_check, int, 0444);
MODULE_PARM_DESC(relaxed_check,
		"Don't do all checks for speedstep capability.");
#endif

MODULE_AUTHOR("Dominik Brodowski <linux@brodo.de>");
MODULE_DESCRIPTION("Library for Intel SpeedStep 1 or 2 cpufreq drivers.");
MODULE_LICENSE("GPL");
