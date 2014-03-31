/*
 * Firmware Assisted dump header file.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright 2011 IBM Corporation
 * Author: Mahesh Salgaonkar <mahesh@linux.vnet.ibm.com>
 */

#ifndef __PPC64_FA_DUMP_H__
#define __PPC64_FA_DUMP_H__

#ifdef CONFIG_FA_DUMP

#define RMA_START	0x0
#define RMA_END		(ppc64_rma_size)

#define MIN_BOOT_MEM	(((RMA_END < (0x1UL << 28)) ? (0x1UL << 28) : RMA_END) \
			+ (0x1UL << 26))

#define memblock_num_regions(memblock_type)	(memblock.memblock_type.cnt)

#ifndef ELF_CORE_EFLAGS
#define ELF_CORE_EFLAGS 0
#endif

#define FADUMP_CPU_STATE_DATA	0x0001
#define FADUMP_HPTE_REGION	0x0002
#define FADUMP_REAL_MODE_REGION	0x0011

#define FADUMP_REQUEST_FLAG	0x00000001

#define FADUMP_REGISTER		1
#define FADUMP_UNREGISTER	2
#define FADUMP_INVALIDATE	3

#define FADUMP_ERROR_FLAG	0x2000

#define FADUMP_CPU_ID_MASK	((1UL << 32) - 1)

#define CPU_UNKNOWN		(~((u32)0))

#define SKIP_TO_NEXT_CPU(reg_entry)			\
({							\
	while (reg_entry->reg_id != REG_ID("CPUEND"))	\
		reg_entry++;				\
	reg_entry++;					\
})

struct fadump_section {
	u32	request_flag;
	u16	source_data_type;
	u16	error_flags;
	u64	source_address;
	u64	source_len;
	u64	bytes_dumped;
	u64	destination_address;
};

struct fadump_section_header {
	u32	dump_format_version;
	u16	dump_num_sections;
	u16	dump_status_flag;
	u32	offset_first_dump_section;

	
	u32	dd_block_size;
	u64	dd_block_offset;
	u64	dd_num_blocks;
	u32	dd_offset_disk_path;

	
	u32	max_time_auto;
};

struct fadump_mem_struct {
	struct fadump_section_header	header;

	
	struct fadump_section		cpu_state_data;
	struct fadump_section		hpte_region;
	struct fadump_section		rmr_region;
};

struct fw_dump {
	unsigned long	cpu_state_data_size;
	unsigned long	hpte_region_size;
	unsigned long	boot_memory_size;
	unsigned long	reserve_dump_area_start;
	unsigned long	reserve_dump_area_size;
	
	unsigned long	reserve_bootvar;

	unsigned long	fadumphdr_addr;
	unsigned long	cpu_notes_buf;
	unsigned long	cpu_notes_buf_size;

	int		ibm_configure_kernel_dump;

	unsigned long	fadump_enabled:1;
	unsigned long	fadump_supported:1;
	unsigned long	dump_active:1;
	unsigned long	dump_registered:1;
};

static inline u64 str_to_u64(const char *str)
{
	u64 val = 0;
	int i;

	for (i = 0; i < sizeof(val); i++)
		val = (*str) ? (val << 8) | *str++ : val << 8;
	return val;
}
#define STR_TO_HEX(x)	str_to_u64(x)
#define REG_ID(x)	str_to_u64(x)

#define FADUMP_CRASH_INFO_MAGIC		STR_TO_HEX("FADMPINF")
#define REGSAVE_AREA_MAGIC		STR_TO_HEX("REGSAVE")


struct fadump_reg_save_area_header {
	u64		magic_number;
	u32		version;
	u32		num_cpu_offset;
};

struct fadump_reg_entry {
	u64		reg_id;
	u64		reg_value;
};

struct fadump_crash_info_header {
	u64		magic_number;
	u64		elfcorehdr_addr;
	u32		crashing_cpu;
	struct pt_regs	regs;
	struct cpumask	cpu_online_mask;
};

#define INIT_CRASHMEM_RANGES	(INIT_MEMBLOCK_REGIONS + 2)

struct fad_crash_memory_ranges {
	unsigned long long	base;
	unsigned long long	size;
};

extern int early_init_dt_scan_fw_dump(unsigned long node,
		const char *uname, int depth, void *data);
extern int fadump_reserve_mem(void);
extern int setup_fadump(void);
extern int is_fadump_active(void);
extern void crash_fadump(struct pt_regs *, const char *);
extern void fadump_cleanup(void);

extern void vmcore_cleanup(void);
#else	
static inline int is_fadump_active(void) { return 0; }
static inline void crash_fadump(struct pt_regs *regs, const char *str) { }
#endif
#endif
