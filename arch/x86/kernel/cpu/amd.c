#include <linux/export.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/elf.h>
#include <linux/mm.h>

#include <linux/io.h>
#include <linux/sched.h>
#include <asm/processor.h>
#include <asm/apic.h>
#include <asm/cpu.h>
#include <asm/pci-direct.h>

#ifdef CONFIG_X86_64
# include <asm/numa_64.h>
# include <asm/mmconfig.h>
# include <asm/cacheflush.h>
#endif

#include "cpu.h"

#ifdef CONFIG_X86_32

extern void vide(void);
__asm__(".align 4\nvide: ret");

static void __cpuinit init_amd_k5(struct cpuinfo_x86 *c)
{
#define CBAR		(0xfffc) 
#define CBAR_ENB	(0x80000000)
#define CBAR_KEY	(0X000000CB)
	if (c->x86_model == 9 || c->x86_model == 10) {
		if (inl(CBAR) & CBAR_ENB)
			outl(0 | CBAR_KEY, CBAR);
	}
}


static void __cpuinit init_amd_k6(struct cpuinfo_x86 *c)
{
	u32 l, h;
	int mbytes = num_physpages >> (20-PAGE_SHIFT);

	if (c->x86_model < 6) {
		
		if (c->x86_model == 0) {
			clear_cpu_cap(c, X86_FEATURE_APIC);
			set_cpu_cap(c, X86_FEATURE_PGE);
		}
		return;
	}

	if (c->x86_model == 6 && c->x86_mask == 1) {
		const int K6_BUG_LOOP = 1000000;
		int n;
		void (*f_vide)(void);
		unsigned long d, d2;

		printk(KERN_INFO "AMD K6 stepping B detected - ");


		n = K6_BUG_LOOP;
		f_vide = vide;
		rdtscl(d);
		while (n--)
			f_vide();
		rdtscl(d2);
		d = d2-d;

		if (d > 20*K6_BUG_LOOP)
			printk(KERN_CONT
				"system stability may be impaired when more than 32 MB are used.\n");
		else
			printk(KERN_CONT "probably OK (after B9730xxxx).\n");
	}

	
	if (c->x86_model < 8 ||
	   (c->x86_model == 8 && c->x86_mask < 8)) {
		
		if (mbytes > 508)
			mbytes = 508;

		rdmsr(MSR_K6_WHCR, l, h);
		if ((l&0x0000FFFF) == 0) {
			unsigned long flags;
			l = (1<<0)|((mbytes/4)<<1);
			local_irq_save(flags);
			wbinvd();
			wrmsr(MSR_K6_WHCR, l, h);
			local_irq_restore(flags);
			printk(KERN_INFO "Enabling old style K6 write allocation for %d Mb\n",
				mbytes);
		}
		return;
	}

	if ((c->x86_model == 8 && c->x86_mask > 7) ||
	     c->x86_model == 9 || c->x86_model == 13) {
		

		if (mbytes > 4092)
			mbytes = 4092;

		rdmsr(MSR_K6_WHCR, l, h);
		if ((l&0xFFFF0000) == 0) {
			unsigned long flags;
			l = ((mbytes>>2)<<22)|(1<<16);
			local_irq_save(flags);
			wbinvd();
			wrmsr(MSR_K6_WHCR, l, h);
			local_irq_restore(flags);
			printk(KERN_INFO "Enabling new style K6 write allocation for %d Mb\n",
				mbytes);
		}

		return;
	}

	if (c->x86_model == 10) {
		
		
		return;
	}
}

static void __cpuinit amd_k7_smp_check(struct cpuinfo_x86 *c)
{
	
	if (!c->cpu_index)
		return;

	
	if ((c->x86_model == 6) && ((c->x86_mask == 0) ||
	    (c->x86_mask == 1)))
		goto valid_k7;

	
	if ((c->x86_model == 7) && (c->x86_mask == 0))
		goto valid_k7;

	if (((c->x86_model == 6) && (c->x86_mask >= 2)) ||
	    ((c->x86_model == 7) && (c->x86_mask >= 1)) ||
	     (c->x86_model > 7))
		if (cpu_has_mp)
			goto valid_k7;

	

	WARN_ONCE(1, "WARNING: This combination of AMD"
		" processors is not suitable for SMP.\n");
	if (!test_taint(TAINT_UNSAFE_SMP))
		add_taint(TAINT_UNSAFE_SMP);

valid_k7:
	;
}

static void __cpuinit init_amd_k7(struct cpuinfo_x86 *c)
{
	u32 l, h;

	if (c->x86_model >= 6 && c->x86_model <= 10) {
		if (!cpu_has(c, X86_FEATURE_XMM)) {
			printk(KERN_INFO "Enabling disabled K7/SSE Support.\n");
			rdmsr(MSR_K7_HWCR, l, h);
			l &= ~0x00008000;
			wrmsr(MSR_K7_HWCR, l, h);
			set_cpu_cap(c, X86_FEATURE_XMM);
		}
	}

	if ((c->x86_model == 8 && c->x86_mask >= 1) || (c->x86_model > 8)) {
		rdmsr(MSR_K7_CLK_CTL, l, h);
		if ((l & 0xfff00000) != 0x20000000) {
			printk(KERN_INFO
			    "CPU: CLK_CTL MSR was %x. Reprogramming to %x\n",
					l, ((l & 0x000fffff)|0x20000000));
			wrmsr(MSR_K7_CLK_CTL, (l & 0x000fffff)|0x20000000, h);
		}
	}

	set_cpu_cap(c, X86_FEATURE_K7);

	amd_k7_smp_check(c);
}
#endif

#ifdef CONFIG_NUMA
static int __cpuinit nearby_node(int apicid)
{
	int i, node;

	for (i = apicid - 1; i >= 0; i--) {
		node = __apicid_to_node[i];
		if (node != NUMA_NO_NODE && node_online(node))
			return node;
	}
	for (i = apicid + 1; i < MAX_LOCAL_APIC; i++) {
		node = __apicid_to_node[i];
		if (node != NUMA_NO_NODE && node_online(node))
			return node;
	}
	return first_node(node_online_map); 
}
#endif

#ifdef CONFIG_X86_HT
static void __cpuinit amd_get_topology(struct cpuinfo_x86 *c)
{
	u32 nodes, cores_per_cu = 1;
	u8 node_id;
	int cpu = smp_processor_id();

	
	if (cpu_has(c, X86_FEATURE_TOPOEXT)) {
		u32 eax, ebx, ecx, edx;

		cpuid(0x8000001e, &eax, &ebx, &ecx, &edx);
		nodes = ((ecx >> 8) & 7) + 1;
		node_id = ecx & 7;

		
		smp_num_siblings = ((ebx >> 8) & 3) + 1;
		c->compute_unit_id = ebx & 0xff;
		cores_per_cu += ((ebx >> 8) & 3);
	} else if (cpu_has(c, X86_FEATURE_NODEID_MSR)) {
		u64 value;

		rdmsrl(MSR_FAM10H_NODE_ID, value);
		nodes = ((value >> 3) & 7) + 1;
		node_id = value & 7;
	} else
		return;

	
	if (nodes > 1) {
		u32 cores_per_node;
		u32 cus_per_node;

		set_cpu_cap(c, X86_FEATURE_AMD_DCM);
		cores_per_node = c->x86_max_cores / nodes;
		cus_per_node = cores_per_node / cores_per_cu;

		
		per_cpu(cpu_llc_id, cpu) = node_id;

		
		c->cpu_core_id %= cores_per_node;
		c->compute_unit_id %= cus_per_node;
	}
}
#endif

static void __cpuinit amd_detect_cmp(struct cpuinfo_x86 *c)
{
#ifdef CONFIG_X86_HT
	unsigned bits;
	int cpu = smp_processor_id();

	bits = c->x86_coreid_bits;
	
	c->cpu_core_id = c->initial_apicid & ((1 << bits)-1);
	
	c->phys_proc_id = c->initial_apicid >> bits;
	
	per_cpu(cpu_llc_id, cpu) = c->phys_proc_id;
	amd_get_topology(c);
#endif
}

int amd_get_nb_id(int cpu)
{
	int id = 0;
#ifdef CONFIG_SMP
	id = per_cpu(cpu_llc_id, cpu);
#endif
	return id;
}
EXPORT_SYMBOL_GPL(amd_get_nb_id);

static void __cpuinit srat_detect_node(struct cpuinfo_x86 *c)
{
#ifdef CONFIG_NUMA
	int cpu = smp_processor_id();
	int node;
	unsigned apicid = c->apicid;

	node = numa_cpu_node(cpu);
	if (node == NUMA_NO_NODE)
		node = per_cpu(cpu_llc_id, cpu);

	if (x86_cpuinit.fixup_cpu_id)
		x86_cpuinit.fixup_cpu_id(c, node);

	if (!node_online(node)) {
		int ht_nodeid = c->initial_apicid;

		if (ht_nodeid >= 0 &&
		    __apicid_to_node[ht_nodeid] != NUMA_NO_NODE)
			node = __apicid_to_node[ht_nodeid];
		
		if (!node_online(node))
			node = nearby_node(apicid);
	}
	numa_set_node(cpu, node);
#endif
}

static void __cpuinit early_init_amd_mc(struct cpuinfo_x86 *c)
{
#ifdef CONFIG_X86_HT
	unsigned bits, ecx;

	
	if (c->extended_cpuid_level < 0x80000008)
		return;

	ecx = cpuid_ecx(0x80000008);

	c->x86_max_cores = (ecx & 0xff) + 1;

	
	bits = (ecx >> 12) & 0xF;

	
	if (bits == 0) {
		while ((1 << bits) < c->x86_max_cores)
			bits++;
	}

	c->x86_coreid_bits = bits;
#endif
}

static void __cpuinit bsp_init_amd(struct cpuinfo_x86 *c)
{
	if (cpu_has(c, X86_FEATURE_CONSTANT_TSC)) {

		if (c->x86 > 0x10 ||
		    (c->x86 == 0x10 && c->x86_model >= 0x2)) {
			u64 val;

			rdmsrl(MSR_K7_HWCR, val);
			if (!(val & BIT(24)))
				printk(KERN_WARNING FW_BUG "TSC doesn't count "
					"with P0 frequency!\n");
		}
	}

	if (c->x86 == 0x15) {
		unsigned long upperbit;
		u32 cpuid, assoc;

		cpuid	 = cpuid_edx(0x80000005);
		assoc	 = cpuid >> 16 & 0xff;
		upperbit = ((cpuid >> 24) << 10) / assoc;

		va_align.mask	  = (upperbit - 1) & PAGE_MASK;
		va_align.flags    = ALIGN_VA_32 | ALIGN_VA_64;
	}
}

static void __cpuinit early_init_amd(struct cpuinfo_x86 *c)
{
	early_init_amd_mc(c);

	if (c->x86_power & (1 << 8)) {
		set_cpu_cap(c, X86_FEATURE_CONSTANT_TSC);
		set_cpu_cap(c, X86_FEATURE_NONSTOP_TSC);
		if (!check_tsc_unstable())
			sched_clock_stable = 1;
	}

#ifdef CONFIG_X86_64
	set_cpu_cap(c, X86_FEATURE_SYSCALL32);
#else
	
	if (c->x86 == 5)
		if (c->x86_model == 13 || c->x86_model == 9 ||
		    (c->x86_model == 8 && c->x86_mask >= 8))
			set_cpu_cap(c, X86_FEATURE_K6_MTRR);
#endif
#if defined(CONFIG_X86_LOCAL_APIC) && defined(CONFIG_PCI)
	
	if (cpu_has_apic && c->x86 >= 0xf) {
		unsigned int val;
		val = read_pci_config(0, 24, 0, 0x68);
		if ((val & ((1 << 17) | (1 << 18))) == ((1 << 17) | (1 << 18)))
			set_cpu_cap(c, X86_FEATURE_EXTD_APICID);
	}
#endif
}

static void __cpuinit init_amd(struct cpuinfo_x86 *c)
{
	u32 dummy;

#ifdef CONFIG_SMP
	unsigned long long value;

	if (c->x86 == 0xf) {
		rdmsrl(MSR_K7_HWCR, value);
		value |= 1 << 6;
		wrmsrl(MSR_K7_HWCR, value);
	}
#endif

	early_init_amd(c);

	clear_cpu_cap(c, 0*32+31);

#ifdef CONFIG_X86_64
	
	if (c->x86 == 0xf) {
		u32 level;

		level = cpuid_eax(1);
		if ((level >= 0x0f48 && level < 0x0f50) || level >= 0x0f58)
			set_cpu_cap(c, X86_FEATURE_REP_GOOD);

		if (c->x86_model < 0x14 && cpu_has(c, X86_FEATURE_LAHF_LM)) {
			u64 val;

			clear_cpu_cap(c, X86_FEATURE_LAHF_LM);
			if (!rdmsrl_amd_safe(0xc001100d, &val)) {
				val &= ~(1ULL << 32);
				wrmsrl_amd_safe(0xc001100d, val);
			}
		}

	}
	if (c->x86 >= 0x10)
		set_cpu_cap(c, X86_FEATURE_REP_GOOD);

	
	c->apicid = hard_smp_processor_id();
#else


	switch (c->x86) {
	case 4:
		init_amd_k5(c);
		break;
	case 5:
		init_amd_k6(c);
		break;
	case 6: 
		init_amd_k7(c);
		break;
	}

	
	if (c->x86 < 6)
		clear_cpu_cap(c, X86_FEATURE_MCE);
#endif

	
	if (c->x86 >= 6)
		set_cpu_cap(c, X86_FEATURE_FXSAVE_LEAK);

	if (!c->x86_model_id[0]) {
		switch (c->x86) {
		case 0xf:
			strcpy(c->x86_model_id, "Hammer");
			break;
		}
	}

	
	if ((c->x86 == 0x15) &&
	    (c->x86_model >= 0x10) && (c->x86_model <= 0x1f) &&
	    !cpu_has(c, X86_FEATURE_TOPOEXT)) {
		u64 val;

		if (!rdmsrl_amd_safe(0xc0011005, &val)) {
			val |= 1ULL << 54;
			wrmsrl_amd_safe(0xc0011005, val);
			rdmsrl(0xc0011005, val);
			if (val & (1ULL << 54)) {
				set_cpu_cap(c, X86_FEATURE_TOPOEXT);
				printk(KERN_INFO FW_INFO "CPU: Re-enabling "
				  "disabled Topology Extensions Support\n");
			}
		}
	}

	cpu_detect_cache_sizes(c);

	
	if (c->extended_cpuid_level >= 0x80000008) {
		amd_detect_cmp(c);
		srat_detect_node(c);
	}

#ifdef CONFIG_X86_32
	detect_ht(c);
#endif

	if (c->extended_cpuid_level >= 0x80000006) {
		if (cpuid_edx(0x80000006) & 0xf000)
			num_cache_leaves = 4;
		else
			num_cache_leaves = 3;
	}

	if (c->x86 >= 0xf)
		set_cpu_cap(c, X86_FEATURE_K8);

	if (cpu_has_xmm2) {
		
		set_cpu_cap(c, X86_FEATURE_MFENCE_RDTSC);
	}

#ifdef CONFIG_X86_64
	if (c->x86 == 0x10) {
		
		if (c == &boot_cpu_data)
			check_enable_amd_mmconf_dmi();

		fam10h_check_enable_mmcfg();
	}

	if (c == &boot_cpu_data && c->x86 >= 0xf) {
		unsigned long long tseg;

		if (!rdmsrl_safe(MSR_K8_TSEG_ADDR, &tseg)) {
			printk(KERN_DEBUG "tseg: %010llx\n", tseg);
			if ((tseg>>PMD_SHIFT) <
				(max_low_pfn_mapped>>(PMD_SHIFT-PAGE_SHIFT)) ||
				((tseg>>PMD_SHIFT) <
				(max_pfn_mapped>>(PMD_SHIFT-PAGE_SHIFT)) &&
				(tseg>>PMD_SHIFT) >= (1ULL<<(32 - PMD_SHIFT))))
				set_memory_4k((unsigned long)__va(tseg), 1);
		}
	}
#endif

	if (c->x86 > 0x11)
		set_cpu_cap(c, X86_FEATURE_ARAT);

	if (c->x86 == 0x10) {
		u64 mask;
		int err;

		err = rdmsrl_safe(MSR_AMD64_MCx_MASK(4), &mask);
		if (err == 0) {
			mask |= (1 << 10);
			checking_wrmsrl(MSR_AMD64_MCx_MASK(4), mask);
		}
	}

	rdmsr_safe(MSR_AMD64_PATCH_LEVEL, &c->microcode, &dummy);
}

#ifdef CONFIG_X86_32
static unsigned int __cpuinit amd_size_cache(struct cpuinfo_x86 *c,
							unsigned int size)
{
	
	if ((c->x86 == 6)) {
		
		if (c->x86_model == 3 && c->x86_mask == 0)
			size = 64;
		
		if (c->x86_model == 4 &&
			(c->x86_mask == 0 || c->x86_mask == 1))
			size = 256;
	}
	return size;
}
#endif

static const struct cpu_dev __cpuinitconst amd_cpu_dev = {
	.c_vendor	= "AMD",
	.c_ident	= { "AuthenticAMD" },
#ifdef CONFIG_X86_32
	.c_models = {
		{ .vendor = X86_VENDOR_AMD, .family = 4, .model_names =
		  {
			  [3] = "486 DX/2",
			  [7] = "486 DX/2-WB",
			  [8] = "486 DX/4",
			  [9] = "486 DX/4-WB",
			  [14] = "Am5x86-WT",
			  [15] = "Am5x86-WB"
		  }
		},
	},
	.c_size_cache	= amd_size_cache,
#endif
	.c_early_init   = early_init_amd,
	.c_bsp_init	= bsp_init_amd,
	.c_init		= init_amd,
	.c_x86_vendor	= X86_VENDOR_AMD,
};

cpu_dev_register(amd_cpu_dev);


const int amd_erratum_400[] =
	AMD_OSVW_ERRATUM(1, AMD_MODEL_RANGE(0xf, 0x41, 0x2, 0xff, 0xf),
			    AMD_MODEL_RANGE(0x10, 0x2, 0x1, 0xff, 0xf));
EXPORT_SYMBOL_GPL(amd_erratum_400);

const int amd_erratum_383[] =
	AMD_OSVW_ERRATUM(3, AMD_MODEL_RANGE(0x10, 0, 0, 0xff, 0xf));
EXPORT_SYMBOL_GPL(amd_erratum_383);

bool cpu_has_amd_erratum(const int *erratum)
{
	struct cpuinfo_x86 *cpu = __this_cpu_ptr(&cpu_info);
	int osvw_id = *erratum++;
	u32 range;
	u32 ms;

	if (cpu->x86 == 0)
		cpu = &boot_cpu_data;

	if (cpu->x86_vendor != X86_VENDOR_AMD)
		return false;

	if (osvw_id >= 0 && osvw_id < 65536 &&
	    cpu_has(cpu, X86_FEATURE_OSVW)) {
		u64 osvw_len;

		rdmsrl(MSR_AMD64_OSVW_ID_LENGTH, osvw_len);
		if (osvw_id < osvw_len) {
			u64 osvw_bits;

			rdmsrl(MSR_AMD64_OSVW_STATUS + (osvw_id >> 6),
			    osvw_bits);
			return osvw_bits & (1ULL << (osvw_id & 0x3f));
		}
	}

	
	ms = (cpu->x86_model << 4) | cpu->x86_mask;
	while ((range = *erratum++))
		if ((cpu->x86 == AMD_MODEL_RANGE_FAMILY(range)) &&
		    (ms >= AMD_MODEL_RANGE_START(range)) &&
		    (ms <= AMD_MODEL_RANGE_END(range)))
			return true;

	return false;
}

EXPORT_SYMBOL_GPL(cpu_has_amd_erratum);
