/*
 * arch/sh/kernel/cpu/init.c
 *
 * CPU init code
 *
 * Copyright (C) 2002 - 2009  Paul Mundt
 * Copyright (C) 2003  Richard Curnow
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/log2.h>
#include <asm/mmu_context.h>
#include <asm/processor.h>
#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <asm/cache.h>
#include <asm/elf.h>
#include <asm/io.h>
#include <asm/smp.h>
#include <asm/sh_bios.h>
#include <asm/setup.h>

#ifdef CONFIG_SH_FPU
#define cpu_has_fpu	1
#else
#define cpu_has_fpu	0
#endif

#ifdef CONFIG_SH_DSP
#define cpu_has_dsp	1
#else
#define cpu_has_dsp	0
#endif

#define onchip_setup(x)					\
static int x##_disabled __cpuinitdata = !cpu_has_##x;	\
							\
static int __cpuinit x##_setup(char *opts)			\
{							\
	x##_disabled = 1;				\
	return 1;					\
}							\
__setup("no" __stringify(x), x##_setup);

onchip_setup(fpu);
onchip_setup(dsp);

#ifdef CONFIG_SPECULATIVE_EXECUTION
#define CPUOPM		0xff2f0000
#define CPUOPM_RABD	(1 << 5)

static void __cpuinit speculative_execution_init(void)
{
	
	__raw_writel(__raw_readl(CPUOPM) & ~CPUOPM_RABD, CPUOPM);

	
	(void)__raw_readl(CPUOPM);
	ctrl_barrier();
}
#else
#define speculative_execution_init()	do { } while (0)
#endif

#ifdef CONFIG_CPU_SH4A
#define EXPMASK			0xff2f0004
#define EXPMASK_RTEDS		(1 << 0)
#define EXPMASK_BRDSSLP		(1 << 1)
#define EXPMASK_MMCAW		(1 << 4)

static void __cpuinit expmask_init(void)
{
	unsigned long expmask = __raw_readl(EXPMASK);

	expmask &= ~(EXPMASK_RTEDS | EXPMASK_BRDSSLP | EXPMASK_MMCAW);

	__raw_writel(expmask, EXPMASK);
	ctrl_barrier();
}
#else
#define expmask_init()	do { } while (0)
#endif

void __attribute__ ((weak)) l2_cache_init(void)
{
}

#ifdef CONFIG_SUPERH32
static void cache_init(void)
{
	unsigned long ccr, flags;

	jump_to_uncached();
	ccr = __raw_readl(CCR);

	if (ccr & CCR_CACHE_ENABLE) {
		unsigned long ways, waysize, addrstart;

		waysize = current_cpu_data.dcache.sets;

#ifdef CCR_CACHE_ORA
		if (ccr & CCR_CACHE_ORA)
			waysize >>= 1;
#endif

		waysize <<= current_cpu_data.dcache.entry_shift;

#ifdef CCR_CACHE_EMODE
		
		if (!(ccr & CCR_CACHE_EMODE))
			ways = 1;
		else
#endif
			ways = current_cpu_data.dcache.ways;

		addrstart = CACHE_OC_ADDRESS_ARRAY;
		do {
			unsigned long addr;

			for (addr = addrstart;
			     addr < addrstart + waysize;
			     addr += current_cpu_data.dcache.linesz)
				__raw_writel(0, addr);

			addrstart += current_cpu_data.dcache.way_incr;
		} while (--ways);
	}

	flags = CCR_CACHE_ENABLE | CCR_CACHE_INVALIDATE;

#ifdef CCR_CACHE_EMODE
	
	if (current_cpu_data.dcache.ways > 1)
		flags |= CCR_CACHE_EMODE;
	else
		flags &= ~CCR_CACHE_EMODE;
#endif

#if defined(CONFIG_CACHE_WRITETHROUGH)
	
	flags |= CCR_CACHE_WT;
#elif defined(CONFIG_CACHE_WRITEBACK)
	
	flags |= CCR_CACHE_CB;
#else
	
	flags &= ~CCR_CACHE_ENABLE;
#endif

	l2_cache_init();

	__raw_writel(flags, CCR);
	back_to_cached();
}
#else
#define cache_init()	do { } while (0)
#endif

#define CSHAPE(totalsize, linesize, assoc) \
	((totalsize & ~0xff) | (linesize << 4) | assoc)

#define CACHE_DESC_SHAPE(desc)	\
	CSHAPE((desc).way_size * (desc).ways, ilog2((desc).linesz), (desc).ways)

static void detect_cache_shape(void)
{
	l1d_cache_shape = CACHE_DESC_SHAPE(current_cpu_data.dcache);

	if (current_cpu_data.dcache.flags & SH_CACHE_COMBINED)
		l1i_cache_shape = l1d_cache_shape;
	else
		l1i_cache_shape = CACHE_DESC_SHAPE(current_cpu_data.icache);

	if (current_cpu_data.flags & CPU_HAS_L2_CACHE)
		l2_cache_shape = CACHE_DESC_SHAPE(current_cpu_data.scache);
	else
		l2_cache_shape = -1; 
}

static void __cpuinit fpu_init(void)
{
	
	if (fpu_disabled && (current_cpu_data.flags & CPU_HAS_FPU)) {
		printk("FPU Disabled\n");
		current_cpu_data.flags &= ~CPU_HAS_FPU;
	}

	disable_fpu();
	clear_used_math();
}

#ifdef CONFIG_SH_DSP
static void __cpuinit release_dsp(void)
{
	unsigned long sr;

	
	__asm__ __volatile__ (
		"stc\tsr, %0\n\t"
		"and\t%1, %0\n\t"
		"ldc\t%0, sr\n\t"
		: "=&r" (sr)
		: "r" (~SR_DSP)
	);
}

static void __cpuinit dsp_init(void)
{
	unsigned long sr;

	__asm__ __volatile__ (
		"stc\tsr, %0\n\t"
		"or\t%1, %0\n\t"
		"ldc\t%0, sr\n\t"
		"nop\n\t"
		"stc\tsr, %0\n\t"
		: "=&r" (sr)
		: "r" (SR_DSP)
	);

	
	if (sr & SR_DSP)
		current_cpu_data.flags |= CPU_HAS_DSP;

	
	if (dsp_disabled && (current_cpu_data.flags & CPU_HAS_DSP)) {
		printk("DSP Disabled\n");
		current_cpu_data.flags &= ~CPU_HAS_DSP;
	}

	
	release_dsp();
}
#else
static inline void __cpuinit dsp_init(void) { }
#endif 

asmlinkage void __cpuinit cpu_init(void)
{
	current_thread_info()->cpu = hard_smp_processor_id();

	
	cpu_probe();

	if (current_cpu_data.type == CPU_SH_NONE)
		panic("Unknown CPU");

	
	current_cpu_data.icache.entry_mask = current_cpu_data.icache.way_incr -
				      current_cpu_data.icache.linesz;

	current_cpu_data.icache.way_size = current_cpu_data.icache.sets *
				    current_cpu_data.icache.linesz;

	
	current_cpu_data.dcache.entry_mask = current_cpu_data.dcache.way_incr -
				      current_cpu_data.dcache.linesz;

	current_cpu_data.dcache.way_size = current_cpu_data.dcache.sets *
				    current_cpu_data.dcache.linesz;

	
	cache_init();

	if (raw_smp_processor_id() == 0) {
		shm_align_mask = max_t(unsigned long,
				       current_cpu_data.dcache.way_size - 1,
				       PAGE_SIZE - 1);

		
		detect_cache_shape();
	}

	fpu_init();
	dsp_init();

	current_cpu_data.asid_cache = NO_CONTEXT;

	current_cpu_data.phys_bits = __in_29bit_mode() ? 29 : 32;

	speculative_execution_init();
	expmask_init();

	
	if (raw_smp_processor_id() == 0) {
		
		sh_bios_vbr_init();

		per_cpu_trap_init();

		init_thread_xstate();
	}
}
