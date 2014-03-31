/*
 *
 *  linux/arch/cris/kernel/setup.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *  Copyright (c) 2001  Axis Communications AB
 */


#include <linux/init.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <asm/pgtable.h>
#include <linux/seq_file.h>
#include <linux/screen_info.h>
#include <linux/utsname.h>
#include <linux/pfn.h>
#include <linux/cpu.h>
#include <asm/setup.h>
#include <arch/system.h>

struct screen_info screen_info;

extern int root_mountflags;
extern char _etext, _edata, _end;

char __initdata cris_command_line[COMMAND_LINE_SIZE] = { 0, };

extern const unsigned long text_start, edata; 
extern unsigned long dram_start, dram_end;

extern unsigned long romfs_start, romfs_length, romfs_in_flash; 

static struct cpu cpu_devices[NR_CPUS];

extern void show_etrax_copyright(void);		


void __init setup_arch(char **cmdline_p)
{
	extern void init_etrax_debug(void);
	unsigned long bootmap_size;
	unsigned long start_pfn, max_pfn;
	unsigned long memory_start;

	

	init_etrax_debug();

	

	high_memory = &dram_end;

	if(romfs_in_flash || !romfs_length) {
		memory_start = (unsigned long) &_end;
	} else {
		
		printk("ROM fs in RAM, size %lu bytes\n", romfs_length);
		memory_start = romfs_start + romfs_length;
	}

	

	init_mm.start_code = (unsigned long) &text_start;
	init_mm.end_code =   (unsigned long) &_etext;
	init_mm.end_data =   (unsigned long) &_edata;
	init_mm.brk =        (unsigned long) &_end;



        start_pfn = PFN_UP(memory_start);  
	max_pfn =   PFN_DOWN((unsigned long)high_memory); 


	max_low_pfn = max_pfn;
	min_low_pfn = PAGE_OFFSET >> PAGE_SHIFT;

	bootmap_size = init_bootmem_node(NODE_DATA(0), start_pfn,
					 min_low_pfn,
					 max_low_pfn);

	

	free_bootmem(PFN_PHYS(start_pfn), PFN_PHYS(max_pfn - start_pfn));


	reserve_bootmem(PFN_PHYS(start_pfn), bootmap_size, BOOTMEM_DEFAULT);

	

	paging_init();

	*cmdline_p = cris_command_line;

#ifdef CONFIG_ETRAX_CMDLINE
        if (!strcmp(cris_command_line, "")) {
		strlcpy(cris_command_line, CONFIG_ETRAX_CMDLINE, COMMAND_LINE_SIZE);
		cris_command_line[COMMAND_LINE_SIZE - 1] = '\0';
	}
#endif

	
	memcpy(boot_command_line, cris_command_line, COMMAND_LINE_SIZE);
	boot_command_line[COMMAND_LINE_SIZE - 1] = '\0';

	
	show_etrax_copyright();

	
	strcpy(init_utsname()->machine, cris_machine_name);
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < nr_cpu_ids ? (void *)(int)(*pos + 1) : NULL;
}

static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return c_start(m, pos);
}

static void c_stop(struct seq_file *m, void *v)
{
}

extern int show_cpuinfo(struct seq_file *m, void *v);

const struct seq_operations cpuinfo_op = {
	.start = c_start,
	.next  = c_next,
	.stop  = c_stop,
	.show  = show_cpuinfo,
};

static int __init topology_init(void)
{
	int i;

	for_each_possible_cpu(i) {
		 return register_cpu(&cpu_devices[i], i);
	}

	return 0;
}

subsys_initcall(topology_init);

