#ifndef _ASM_X86_E820_H
#define _ASM_X86_E820_H
#define E820MAP	0x2d0		
#define E820MAX	128		


#ifdef __KERNEL__
#ifdef CONFIG_EFI
#include <linux/numa.h>
#define E820_X_MAX (E820MAX + 3 * MAX_NUMNODES)
#else	
#define E820_X_MAX E820MAX
#endif
#else	
#define E820_X_MAX E820MAX
#endif

#define E820NR	0x1e8		

#define E820_RAM	1
#define E820_RESERVED	2
#define E820_ACPI	3
#define E820_NVS	4
#define E820_UNUSABLE	5

#define E820_RESERVED_KERN        128

#ifndef __ASSEMBLY__
#include <linux/types.h>
struct e820entry {
	__u64 addr;	
	__u64 size;	
	__u32 type;	
} __attribute__((packed));

struct e820map {
	__u32 nr_map;
	struct e820entry map[E820_X_MAX];
};

#define ISA_START_ADDRESS	0xa0000
#define ISA_END_ADDRESS		0x100000

#define BIOS_BEGIN		0x000a0000
#define BIOS_END		0x00100000

#define BIOS_ROM_BASE		0xffe00000
#define BIOS_ROM_END		0xffffffff

#ifdef __KERNEL__
extern struct e820map e820;
extern struct e820map e820_saved;

extern unsigned long pci_mem_start;
extern int e820_any_mapped(u64 start, u64 end, unsigned type);
extern int e820_all_mapped(u64 start, u64 end, unsigned type);
extern void e820_add_region(u64 start, u64 size, int type);
extern void e820_print_map(char *who);
extern int
sanitize_e820_map(struct e820entry *biosmap, int max_nr_map, u32 *pnr_map);
extern u64 e820_update_range(u64 start, u64 size, unsigned old_type,
			       unsigned new_type);
extern u64 e820_remove_range(u64 start, u64 size, unsigned old_type,
			     int checktype);
extern void update_e820(void);
extern void e820_setup_gap(void);
extern int e820_search_gap(unsigned long *gapstart, unsigned long *gapsize,
			unsigned long start_addr, unsigned long long end_addr);
struct setup_data;
extern void parse_e820_ext(struct setup_data *data);

#if defined(CONFIG_X86_64) || \
	(defined(CONFIG_X86_32) && defined(CONFIG_HIBERNATION))
extern void e820_mark_nosave_regions(unsigned long limit_pfn);
#else
static inline void e820_mark_nosave_regions(unsigned long limit_pfn)
{
}
#endif

#ifdef CONFIG_MEMTEST
extern void early_memtest(unsigned long start, unsigned long end);
#else
static inline void early_memtest(unsigned long start, unsigned long end)
{
}
#endif

extern unsigned long e820_end_of_ram_pfn(void);
extern unsigned long e820_end_of_low_ram_pfn(void);
extern u64 early_reserve_e820(u64 sizet, u64 align);

void memblock_x86_fill(void);
void memblock_find_dma_reserve(void);

extern void finish_e820_parsing(void);
extern void e820_reserve_resources(void);
extern void e820_reserve_resources_late(void);
extern void setup_memory_map(void);
extern char *default_machine_specific_memory_setup(void);

static inline bool is_ISA_range(u64 s, u64 e)
{
	return s >= ISA_START_ADDRESS && e <= ISA_END_ADDRESS;
}

#endif 
#endif 

#ifdef __KERNEL__
#include <linux/ioport.h>

#define HIGH_MEMORY	(1024*1024)
#endif 

#endif 
