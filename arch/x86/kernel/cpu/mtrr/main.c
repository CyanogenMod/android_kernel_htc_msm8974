/*  Generic MTRR (Memory Type Range Register) driver.

    Copyright (C) 1997-2000  Richard Gooch
    Copyright (c) 2002	     Patrick Mochel

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    Richard Gooch may be reached by email at  rgooch@atnf.csiro.au
    The postal address is:
      Richard Gooch, c/o ATNF, P. O. Box 76, Epping, N.S.W., 2121, Australia.

    Source: "Pentium Pro Family Developer's Manual, Volume 3:
    Operating System Writer's Guide" (Intel document number 242692),
    section 11.11.7

    This was cleaned and made readable by Patrick Mochel <mochel@osdl.org>
    on 6-7 March 2002.
    Source: Intel Architecture Software Developers Manual, Volume 3:
    System Programming Guide; Section 9.11. (1997 edition - PPro).
*/

#define DEBUG

#include <linux/types.h> 

#include <linux/stop_machine.h>
#include <linux/kvm_para.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/init.h>
#include <linux/sort.h>
#include <linux/cpu.h>
#include <linux/pci.h>
#include <linux/smp.h>
#include <linux/syscore_ops.h>

#include <asm/processor.h>
#include <asm/e820.h>
#include <asm/mtrr.h>
#include <asm/msr.h>

#include "mtrr.h"

u32 num_var_ranges;

unsigned int mtrr_usage_table[MTRR_MAX_VAR_RANGES];
static DEFINE_MUTEX(mtrr_mutex);

u64 size_or_mask, size_and_mask;
static bool mtrr_aps_delayed_init;

static const struct mtrr_ops *mtrr_ops[X86_VENDOR_NUM];

const struct mtrr_ops *mtrr_if;

static void set_mtrr(unsigned int reg, unsigned long base,
		     unsigned long size, mtrr_type type);

void set_mtrr_ops(const struct mtrr_ops *ops)
{
	if (ops->vendor && ops->vendor < X86_VENDOR_NUM)
		mtrr_ops[ops->vendor] = ops;
}

static int have_wrcomb(void)
{
	struct pci_dev *dev;

	dev = pci_get_class(PCI_CLASS_BRIDGE_HOST << 8, NULL);
	if (dev != NULL) {
		if (dev->vendor == PCI_VENDOR_ID_SERVERWORKS &&
		    dev->device == PCI_DEVICE_ID_SERVERWORKS_LE &&
		    dev->revision <= 5) {
			pr_info("mtrr: Serverworks LE rev < 6 detected. Write-combining disabled.\n");
			pci_dev_put(dev);
			return 0;
		}
		if (dev->vendor == PCI_VENDOR_ID_INTEL &&
		    dev->device == PCI_DEVICE_ID_INTEL_82451NX) {
			pr_info("mtrr: Intel 450NX MMC detected. Write-combining disabled.\n");
			pci_dev_put(dev);
			return 0;
		}
		pci_dev_put(dev);
	}
	return mtrr_if->have_wrcomb ? mtrr_if->have_wrcomb() : 0;
}

static void __init set_num_var_ranges(void)
{
	unsigned long config = 0, dummy;

	if (use_intel())
		rdmsr(MSR_MTRRcap, config, dummy);
	else if (is_cpu(AMD))
		config = 2;
	else if (is_cpu(CYRIX) || is_cpu(CENTAUR))
		config = 8;

	num_var_ranges = config & 0xff;
}

static void __init init_table(void)
{
	int i, max;

	max = num_var_ranges;
	for (i = 0; i < max; i++)
		mtrr_usage_table[i] = 1;
}

struct set_mtrr_data {
	unsigned long	smp_base;
	unsigned long	smp_size;
	unsigned int	smp_reg;
	mtrr_type	smp_type;
};

static int mtrr_rendezvous_handler(void *info)
{
	struct set_mtrr_data *data = info;

	if (data->smp_reg != ~0U) {
		mtrr_if->set(data->smp_reg, data->smp_base,
			     data->smp_size, data->smp_type);
	} else if (mtrr_aps_delayed_init || !cpu_online(smp_processor_id())) {
		mtrr_if->set_all();
	}
	return 0;
}

static inline int types_compatible(mtrr_type type1, mtrr_type type2)
{
	return type1 == MTRR_TYPE_UNCACHABLE ||
	       type2 == MTRR_TYPE_UNCACHABLE ||
	       (type1 == MTRR_TYPE_WRTHROUGH && type2 == MTRR_TYPE_WRBACK) ||
	       (type1 == MTRR_TYPE_WRBACK && type2 == MTRR_TYPE_WRTHROUGH);
}

static void
set_mtrr(unsigned int reg, unsigned long base, unsigned long size, mtrr_type type)
{
	struct set_mtrr_data data = { .smp_reg = reg,
				      .smp_base = base,
				      .smp_size = size,
				      .smp_type = type
				    };

	stop_machine(mtrr_rendezvous_handler, &data, cpu_online_mask);
}

static void set_mtrr_from_inactive_cpu(unsigned int reg, unsigned long base,
				      unsigned long size, mtrr_type type)
{
	struct set_mtrr_data data = { .smp_reg = reg,
				      .smp_base = base,
				      .smp_size = size,
				      .smp_type = type
				    };

	stop_machine_from_inactive_cpu(mtrr_rendezvous_handler, &data,
				       cpu_callout_mask);
}

int mtrr_add_page(unsigned long base, unsigned long size,
		  unsigned int type, bool increment)
{
	unsigned long lbase, lsize;
	int i, replace, error;
	mtrr_type ltype;

	if (!mtrr_if)
		return -ENXIO;

	error = mtrr_if->validate_add_page(base, size, type);
	if (error)
		return error;

	if (type >= MTRR_NUM_TYPES) {
		pr_warning("mtrr: type: %u invalid\n", type);
		return -EINVAL;
	}

	
	if ((type == MTRR_TYPE_WRCOMB) && !have_wrcomb()) {
		pr_warning("mtrr: your processor doesn't support write-combining\n");
		return -ENOSYS;
	}

	if (!size) {
		pr_warning("mtrr: zero sized request\n");
		return -EINVAL;
	}

	if (base & size_or_mask || size & size_or_mask) {
		pr_warning("mtrr: base or size exceeds the MTRR width\n");
		return -EINVAL;
	}

	error = -EINVAL;
	replace = -1;

	
	get_online_cpus();

	
	mutex_lock(&mtrr_mutex);
	for (i = 0; i < num_var_ranges; ++i) {
		mtrr_if->get(i, &lbase, &lsize, &ltype);
		if (!lsize || base > lbase + lsize - 1 ||
		    base + size - 1 < lbase)
			continue;
		if (base < lbase || base + size - 1 > lbase + lsize - 1) {
			if (base <= lbase &&
			    base + size - 1 >= lbase + lsize - 1) {
				
				if (type == ltype) {
					replace = replace == -1 ? i : -2;
					continue;
				} else if (types_compatible(type, ltype))
					continue;
			}
			pr_warning("mtrr: 0x%lx000,0x%lx000 overlaps existing"
				" 0x%lx000,0x%lx000\n", base, size, lbase,
				lsize);
			goto out;
		}
		
		if (ltype != type) {
			if (types_compatible(type, ltype))
				continue;
			pr_warning("mtrr: type mismatch for %lx000,%lx000 old: %s new: %s\n",
				base, size, mtrr_attrib_to_str(ltype),
				mtrr_attrib_to_str(type));
			goto out;
		}
		if (increment)
			++mtrr_usage_table[i];
		error = i;
		goto out;
	}
	
	i = mtrr_if->get_free_region(base, size, replace);
	if (i >= 0) {
		set_mtrr(i, base, size, type);
		if (likely(replace < 0)) {
			mtrr_usage_table[i] = 1;
		} else {
			mtrr_usage_table[i] = mtrr_usage_table[replace];
			if (increment)
				mtrr_usage_table[i]++;
			if (unlikely(replace != i)) {
				set_mtrr(replace, 0, 0, 0);
				mtrr_usage_table[replace] = 0;
			}
		}
	} else {
		pr_info("mtrr: no more MTRRs available\n");
	}
	error = i;
 out:
	mutex_unlock(&mtrr_mutex);
	put_online_cpus();
	return error;
}

static int mtrr_check(unsigned long base, unsigned long size)
{
	if ((base & (PAGE_SIZE - 1)) || (size & (PAGE_SIZE - 1))) {
		pr_warning("mtrr: size and base must be multiples of 4 kiB\n");
		pr_debug("mtrr: size: 0x%lx  base: 0x%lx\n", size, base);
		dump_stack();
		return -1;
	}
	return 0;
}

int mtrr_add(unsigned long base, unsigned long size, unsigned int type,
	     bool increment)
{
	if (mtrr_check(base, size))
		return -EINVAL;
	return mtrr_add_page(base >> PAGE_SHIFT, size >> PAGE_SHIFT, type,
			     increment);
}
EXPORT_SYMBOL(mtrr_add);

int mtrr_del_page(int reg, unsigned long base, unsigned long size)
{
	int i, max;
	mtrr_type ltype;
	unsigned long lbase, lsize;
	int error = -EINVAL;

	if (!mtrr_if)
		return -ENXIO;

	max = num_var_ranges;
	
	get_online_cpus();
	mutex_lock(&mtrr_mutex);
	if (reg < 0) {
		
		for (i = 0; i < max; ++i) {
			mtrr_if->get(i, &lbase, &lsize, &ltype);
			if (lbase == base && lsize == size) {
				reg = i;
				break;
			}
		}
		if (reg < 0) {
			pr_debug("mtrr: no MTRR for %lx000,%lx000 found\n",
				 base, size);
			goto out;
		}
	}
	if (reg >= max) {
		pr_warning("mtrr: register: %d too big\n", reg);
		goto out;
	}
	mtrr_if->get(reg, &lbase, &lsize, &ltype);
	if (lsize < 1) {
		pr_warning("mtrr: MTRR %d not used\n", reg);
		goto out;
	}
	if (mtrr_usage_table[reg] < 1) {
		pr_warning("mtrr: reg: %d has count=0\n", reg);
		goto out;
	}
	if (--mtrr_usage_table[reg] < 1)
		set_mtrr(reg, 0, 0, 0);
	error = reg;
 out:
	mutex_unlock(&mtrr_mutex);
	put_online_cpus();
	return error;
}

int mtrr_del(int reg, unsigned long base, unsigned long size)
{
	if (mtrr_check(base, size))
		return -EINVAL;
	return mtrr_del_page(reg, base >> PAGE_SHIFT, size >> PAGE_SHIFT);
}
EXPORT_SYMBOL(mtrr_del);

static void __init init_ifs(void)
{
#ifndef CONFIG_X86_64
	amd_init_mtrr();
	cyrix_init_mtrr();
	centaur_init_mtrr();
#endif
}

struct mtrr_value {
	mtrr_type	ltype;
	unsigned long	lbase;
	unsigned long	lsize;
};

static struct mtrr_value mtrr_value[MTRR_MAX_VAR_RANGES];

static int mtrr_save(void)
{
	int i;

	for (i = 0; i < num_var_ranges; i++) {
		mtrr_if->get(i, &mtrr_value[i].lbase,
				&mtrr_value[i].lsize,
				&mtrr_value[i].ltype);
	}
	return 0;
}

static void mtrr_restore(void)
{
	int i;

	for (i = 0; i < num_var_ranges; i++) {
		if (mtrr_value[i].lsize) {
			set_mtrr(i, mtrr_value[i].lbase,
				    mtrr_value[i].lsize,
				    mtrr_value[i].ltype);
		}
	}
}



static struct syscore_ops mtrr_syscore_ops = {
	.suspend	= mtrr_save,
	.resume		= mtrr_restore,
};

int __initdata changed_by_mtrr_cleanup;

void __init mtrr_bp_init(void)
{
	u32 phys_addr;

	init_ifs();

	phys_addr = 32;

	if (cpu_has_mtrr) {
		mtrr_if = &generic_mtrr_ops;
		size_or_mask = 0xff000000;			
		size_and_mask = 0x00f00000;
		phys_addr = 36;

		if (cpuid_eax(0x80000000) >= 0x80000008) {
			phys_addr = cpuid_eax(0x80000008) & 0xff;
			
			if (boot_cpu_data.x86_vendor == X86_VENDOR_INTEL &&
			    boot_cpu_data.x86 == 0xF &&
			    boot_cpu_data.x86_model == 0x3 &&
			    (boot_cpu_data.x86_mask == 0x3 ||
			     boot_cpu_data.x86_mask == 0x4))
				phys_addr = 36;

			size_or_mask = ~((1ULL << (phys_addr - PAGE_SHIFT)) - 1);
			size_and_mask = ~size_or_mask & 0xfffff00000ULL;
		} else if (boot_cpu_data.x86_vendor == X86_VENDOR_CENTAUR &&
			   boot_cpu_data.x86 == 6) {
			size_or_mask = 0xfff00000;		
			size_and_mask = 0;
			phys_addr = 32;
		}
	} else {
		switch (boot_cpu_data.x86_vendor) {
		case X86_VENDOR_AMD:
			if (cpu_has_k6_mtrr) {
				
				mtrr_if = mtrr_ops[X86_VENDOR_AMD];
				size_or_mask = 0xfff00000;	
				size_and_mask = 0;
			}
			break;
		case X86_VENDOR_CENTAUR:
			if (cpu_has_centaur_mcr) {
				mtrr_if = mtrr_ops[X86_VENDOR_CENTAUR];
				size_or_mask = 0xfff00000;	
				size_and_mask = 0;
			}
			break;
		case X86_VENDOR_CYRIX:
			if (cpu_has_cyrix_arr) {
				mtrr_if = mtrr_ops[X86_VENDOR_CYRIX];
				size_or_mask = 0xfff00000;	
				size_and_mask = 0;
			}
			break;
		default:
			break;
		}
	}

	if (mtrr_if) {
		set_num_var_ranges();
		init_table();
		if (use_intel()) {
			get_mtrr_state();

			if (mtrr_cleanup(phys_addr)) {
				changed_by_mtrr_cleanup = 1;
				mtrr_if->set_all();
			}
		}
	}
}

void mtrr_ap_init(void)
{
	if (!use_intel() || mtrr_aps_delayed_init)
		return;
	set_mtrr_from_inactive_cpu(~0U, 0, 0, 0);
}

void mtrr_save_state(void)
{
	smp_call_function_single(0, mtrr_save_fixed_ranges, NULL, 1);
}

void set_mtrr_aps_delayed_init(void)
{
	if (!use_intel())
		return;

	mtrr_aps_delayed_init = true;
}

void mtrr_aps_init(void)
{
	if (!use_intel())
		return;

	if (!mtrr_aps_delayed_init)
		return;

	set_mtrr(~0U, 0, 0, 0);
	mtrr_aps_delayed_init = false;
}

void mtrr_bp_restore(void)
{
	if (!use_intel())
		return;

	mtrr_if->set_all();
}

static int __init mtrr_init_finialize(void)
{
	if (!mtrr_if)
		return 0;

	if (use_intel()) {
		if (!changed_by_mtrr_cleanup)
			mtrr_state_warn();
		return 0;
	}

	register_syscore_ops(&mtrr_syscore_ops);

	return 0;
}
subsys_initcall(mtrr_init_finialize);
