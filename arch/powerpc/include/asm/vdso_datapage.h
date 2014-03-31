#ifndef _VDSO_DATAPAGE_H
#define _VDSO_DATAPAGE_H
#ifdef __KERNEL__

/*
 * Copyright (C) 2002 Peter Bergner <bergner@vnet.ibm.com>, IBM
 * Copyright (C) 2005 Benjamin Herrenschmidy <benh@kernel.crashing.org>,
 * 		      IBM Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */



#define SYSTEMCFG_MAJOR 1
#define SYSTEMCFG_MINOR 1

#ifndef __ASSEMBLY__

#include <linux/unistd.h>
#include <linux/time.h>

#define SYSCALL_MAP_SIZE      ((__NR_syscalls + 31) / 32)


#ifdef CONFIG_PPC64

struct vdso_data {
	__u8  eye_catcher[16];		
	struct {			
		__u32 major;		
		__u32 minor;		
	} version;

	__u32 platform;			
	__u32 processor;		
	__u64 processorCount;		
	__u64 physicalMemorySize;	
	__u64 tb_orig_stamp;		
	__u64 tb_ticks_per_sec;		
	__u64 tb_to_xs;			
	__u64 stamp_xsec;		
	__u64 tb_update_count;		
	__u32 tz_minuteswest;		
	__u32 tz_dsttime;		
	__u32 dcache_size;		
	__u32 dcache_line_size;		
	__u32 icache_size;		
	__u32 icache_line_size;		

	__u32 dcache_block_size;		
	__u32 icache_block_size;		
	__u32 dcache_log_block_size;		
	__u32 icache_log_block_size;		
	__s32 wtom_clock_sec;			
	__s32 wtom_clock_nsec;
	struct timespec stamp_xtime;	
	__u32 stamp_sec_fraction;	
   	__u32 syscall_map_64[SYSCALL_MAP_SIZE]; 
   	__u32 syscall_map_32[SYSCALL_MAP_SIZE]; 
};

#else 

struct vdso_data {
	__u64 tb_orig_stamp;		
	__u64 tb_ticks_per_sec;		
	__u64 tb_to_xs;			
	__u64 stamp_xsec;		
	__u32 tb_update_count;		
	__u32 tz_minuteswest;		
	__u32 tz_dsttime;		
	__s32 wtom_clock_sec;			
	__s32 wtom_clock_nsec;
	struct timespec stamp_xtime;	
	__u32 stamp_sec_fraction;	
   	__u32 syscall_map_32[SYSCALL_MAP_SIZE]; 
	__u32 dcache_block_size;	
	__u32 icache_block_size;	
	__u32 dcache_log_block_size;	
	__u32 icache_log_block_size;	
};

#endif 

extern struct vdso_data *vdso_data;

#endif 

#endif 
#endif 
