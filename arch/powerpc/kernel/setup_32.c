
#include <linux/module.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/initrd.h>
#include <linux/tty.h>
#include <linux/bootmem.h>
#include <linux/seq_file.h>
#include <linux/root_dev.h>
#include <linux/cpu.h>
#include <linux/console.h>
#include <linux/memblock.h>

#include <asm/io.h>
#include <asm/prom.h>
#include <asm/processor.h>
#include <asm/pgtable.h>
#include <asm/setup.h>
#include <asm/smp.h>
#include <asm/elf.h>
#include <asm/cputable.h>
#include <asm/bootx.h>
#include <asm/btext.h>
#include <asm/machdep.h>
#include <asm/uaccess.h>
#include <asm/pmac_feature.h>
#include <asm/sections.h>
#include <asm/nvram.h>
#include <asm/xmon.h>
#include <asm/time.h>
#include <asm/serial.h>
#include <asm/udbg.h>
#include <asm/mmu_context.h>

#include "setup.h"

#define DBG(fmt...)

extern void bootx_init(unsigned long r4, unsigned long phys);

int boot_cpuid = -1;
EXPORT_SYMBOL_GPL(boot_cpuid);
int boot_cpuid_phys;
EXPORT_SYMBOL_GPL(boot_cpuid_phys);

int smp_hw_index[NR_CPUS];

unsigned long ISA_DMA_THRESHOLD;
unsigned int DMA_MODE_READ;
unsigned int DMA_MODE_WRITE;

#ifdef CONFIG_VGA_CONSOLE
unsigned long vgacon_remap_base;
EXPORT_SYMBOL(vgacon_remap_base);
#endif

int dcache_bsize;
int icache_bsize;
int ucache_bsize;

notrace unsigned long __init early_init(unsigned long dt_ptr)
{
	unsigned long offset = reloc_offset();
	struct cpu_spec *spec;

	memset_io((void __iomem *)PTRRELOC(&__bss_start), 0,
			__bss_stop - __bss_start);

	spec = identify_cpu(offset, mfspr(SPRN_PVR));

	do_feature_fixups(spec->cpu_features,
			  PTRRELOC(&__start___ftr_fixup),
			  PTRRELOC(&__stop___ftr_fixup));

	do_feature_fixups(spec->mmu_features,
			  PTRRELOC(&__start___mmu_ftr_fixup),
			  PTRRELOC(&__stop___mmu_ftr_fixup));

	do_lwsync_fixups(spec->cpu_features,
			 PTRRELOC(&__start___lwsync_fixup),
			 PTRRELOC(&__stop___lwsync_fixup));

	do_final_fixups();

	return KERNELBASE + offset;
}


notrace void __init machine_init(u64 dt_ptr)
{
	lockdep_init();

	
	udbg_early_init();

	
	early_init_devtree(__va(dt_ptr));

	early_init_mmu();

	probe_machine();

	setup_kdump_trampoline();

#ifdef CONFIG_6xx
	if (cpu_has_feature(CPU_FTR_CAN_DOZE) ||
	    cpu_has_feature(CPU_FTR_CAN_NAP))
		ppc_md.power_save = ppc6xx_idle;
#endif

#ifdef CONFIG_E500
	if (cpu_has_feature(CPU_FTR_CAN_DOZE) ||
	    cpu_has_feature(CPU_FTR_CAN_NAP))
		ppc_md.power_save = e500_idle;
#endif
	if (ppc_md.progress)
		ppc_md.progress("id mach(): done", 0x200);
}

#ifdef CONFIG_BOOKE_WDT
extern u32 booke_wdt_enabled;
extern u32 booke_wdt_period;

notrace int __init early_parse_wdt(char *p)
{
	if (p && strncmp(p, "0", 1) != 0)
	       booke_wdt_enabled = 1;

	return 0;
}
early_param("wdt", early_parse_wdt);

int __init early_parse_wdt_period (char *p)
{
	if (p)
		booke_wdt_period = simple_strtoul(p, NULL, 0);

	return 0;
}
early_param("wdt_period", early_parse_wdt_period);
#endif	

int __init ppc_setup_l2cr(char *str)
{
	if (cpu_has_feature(CPU_FTR_L2CR)) {
		unsigned long val = simple_strtoul(str, NULL, 0);
		printk(KERN_INFO "l2cr set to %lx\n", val);
		_set_L2CR(0);		
		_set_L2CR(val);		
	}
	return 1;
}
__setup("l2cr=", ppc_setup_l2cr);

int __init ppc_setup_l3cr(char *str)
{
	if (cpu_has_feature(CPU_FTR_L3CR)) {
		unsigned long val = simple_strtoul(str, NULL, 0);
		printk(KERN_INFO "l3cr set to %lx\n", val);
		_set_L3CR(val);		
	}
	return 1;
}
__setup("l3cr=", ppc_setup_l3cr);

#ifdef CONFIG_GENERIC_NVRAM

unsigned char nvram_read_byte(int addr)
{
	if (ppc_md.nvram_read_val)
		return ppc_md.nvram_read_val(addr);
	return 0xff;
}
EXPORT_SYMBOL(nvram_read_byte);

void nvram_write_byte(unsigned char val, int addr)
{
	if (ppc_md.nvram_write_val)
		ppc_md.nvram_write_val(addr, val);
}
EXPORT_SYMBOL(nvram_write_byte);

ssize_t nvram_get_size(void)
{
	if (ppc_md.nvram_size)
		return ppc_md.nvram_size();
	return -1;
}
EXPORT_SYMBOL(nvram_get_size);

void nvram_sync(void)
{
	if (ppc_md.nvram_sync)
		ppc_md.nvram_sync();
}
EXPORT_SYMBOL(nvram_sync);

#endif 

int __init ppc_init(void)
{
	
	if (ppc_md.progress)
		ppc_md.progress("             ", 0xffff);

	
	if (ppc_md.init != NULL) {
		ppc_md.init();
	}
	return 0;
}

arch_initcall(ppc_init);

static void __init irqstack_early_init(void)
{
	unsigned int i;

	for_each_possible_cpu(i) {
		softirq_ctx[i] = (struct thread_info *)
			__va(memblock_alloc(THREAD_SIZE, THREAD_SIZE));
		hardirq_ctx[i] = (struct thread_info *)
			__va(memblock_alloc(THREAD_SIZE, THREAD_SIZE));
	}
}

#if defined(CONFIG_BOOKE) || defined(CONFIG_40x)
static void __init exc_lvl_early_init(void)
{
	unsigned int i, hw_cpu;

	for_each_possible_cpu(i) {
		hw_cpu = get_hard_smp_processor_id(i);
		critirq_ctx[hw_cpu] = (struct thread_info *)
			__va(memblock_alloc(THREAD_SIZE, THREAD_SIZE));
#ifdef CONFIG_BOOKE
		dbgirq_ctx[hw_cpu] = (struct thread_info *)
			__va(memblock_alloc(THREAD_SIZE, THREAD_SIZE));
		mcheckirq_ctx[hw_cpu] = (struct thread_info *)
			__va(memblock_alloc(THREAD_SIZE, THREAD_SIZE));
#endif
	}
}
#else
#define exc_lvl_early_init()
#endif

void __init setup_arch(char **cmdline_p)
{
	*cmdline_p = cmd_line;

	
	loops_per_jiffy = 500000000 / HZ;

	unflatten_device_tree();
	check_for_initrd();

	if (ppc_md.init_early)
		ppc_md.init_early();

	find_legacy_serial_ports();

	smp_setup_cpu_maps();

	
	register_early_udbg_console();

	xmon_setup();

	dcache_bsize = cur_cpu_spec->dcache_bsize;
	icache_bsize = cur_cpu_spec->icache_bsize;
	ucache_bsize = 0;
	if (cpu_has_feature(CPU_FTR_UNIFIED_ID_CACHE))
		ucache_bsize = icache_bsize = dcache_bsize;

	
	panic_timeout = 180;

	if (ppc_md.panic)
		setup_panic();

	init_mm.start_code = (unsigned long)_stext;
	init_mm.end_code = (unsigned long) _etext;
	init_mm.end_data = (unsigned long) _edata;
	init_mm.brk = klimit;

	exc_lvl_early_init();

	irqstack_early_init();

	
	do_init_bootmem();
	if ( ppc_md.progress ) ppc_md.progress("setup_arch: bootmem", 0x3eab);

#ifdef CONFIG_DUMMY_CONSOLE
	conswitchp = &dummy_con;
#endif

	if (ppc_md.setup_arch)
		ppc_md.setup_arch();
	if ( ppc_md.progress ) ppc_md.progress("arch: exit", 0x3eab);

	paging_init();

	
	mmu_context_init();

}
