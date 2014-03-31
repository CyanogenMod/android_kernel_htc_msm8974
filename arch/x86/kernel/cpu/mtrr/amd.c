#include <linux/init.h>
#include <linux/mm.h>
#include <asm/mtrr.h>
#include <asm/msr.h>

#include "mtrr.h"

static void
amd_get_mtrr(unsigned int reg, unsigned long *base,
	     unsigned long *size, mtrr_type *type)
{
	unsigned long low, high;

	rdmsr(MSR_K6_UWCCR, low, high);
	
	if (reg == 1)
		low = high;
	
	*base = (low & 0xFFFE0000) >> PAGE_SHIFT;
	*type = 0;
	if (low & 1)
		*type = MTRR_TYPE_UNCACHABLE;
	if (low & 2)
		*type = MTRR_TYPE_WRCOMB;
	if (!(low & 3)) {
		*size = 0;
		return;
	}
	low = (~low) & 0x1FFFC;
	*size = (low + 4) << (15 - PAGE_SHIFT);
}

static void
amd_set_mtrr(unsigned int reg, unsigned long base, unsigned long size, mtrr_type type)
{
	u32 regs[2];

	rdmsr(MSR_K6_UWCCR, regs[0], regs[1]);
	if (size == 0) {
		regs[reg] = 0;
	} else {
		regs[reg] = (-size >> (15 - PAGE_SHIFT) & 0x0001FFFC)
		    | (base << PAGE_SHIFT) | (type + 1);
	}

	wbinvd();
	wrmsr(MSR_K6_UWCCR, regs[0], regs[1]);
}

static int
amd_validate_add_page(unsigned long base, unsigned long size, unsigned int type)
{
	if (type > MTRR_TYPE_WRCOMB || size < (1 << (17 - PAGE_SHIFT))
	    || (size & ~(size - 1)) - size || (base & (size - 1)))
		return -EINVAL;
	return 0;
}

static const struct mtrr_ops amd_mtrr_ops = {
	.vendor            = X86_VENDOR_AMD,
	.set               = amd_set_mtrr,
	.get               = amd_get_mtrr,
	.get_free_region   = generic_get_free_region,
	.validate_add_page = amd_validate_add_page,
	.have_wrcomb       = positive_have_wrcomb,
};

int __init amd_init_mtrr(void)
{
	set_mtrr_ops(&amd_mtrr_ops);
	return 0;
}
