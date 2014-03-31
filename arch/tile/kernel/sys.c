/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 *
 * This file contains various random system calls that
 * have a non-standard calling sequence on the Linux/TILE
 * platform.
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/syscalls.h>
#include <linux/mman.h>
#include <linux/file.h>
#include <linux/mempolicy.h>
#include <linux/binfmts.h>
#include <linux/fs.h>
#include <linux/compat.h>
#include <linux/uaccess.h>
#include <linux/signal.h>
#include <asm/syscalls.h>
#include <asm/pgtable.h>
#include <asm/homecache.h>
#include <arch/chip.h>

SYSCALL_DEFINE0(flush_cache)
{
	homecache_evict(cpumask_of(smp_processor_id()));
	return 0;
}


#if !defined(__tilegx__) || defined(CONFIG_COMPAT)

ssize_t sys32_readahead(int fd, u32 offset_lo, u32 offset_hi, u32 count)
{
	return sys_readahead(fd, ((loff_t)offset_hi << 32) | offset_lo, count);
}

int sys32_fadvise64_64(int fd, u32 offset_lo, u32 offset_hi,
		       u32 len_lo, u32 len_hi, int advice)
{
	return sys_fadvise64_64(fd, ((loff_t)offset_hi << 32) | offset_lo,
				((loff_t)len_hi << 32) | len_lo, advice);
}

#endif 

SYSCALL_DEFINE6(mmap2, unsigned long, addr, unsigned long, len,
		unsigned long, prot, unsigned long, flags,
		unsigned long, fd, unsigned long, off_4k)
{
#define PAGE_ADJUST (PAGE_SHIFT - 12)
	if (off_4k & ((1 << PAGE_ADJUST) - 1))
		return -EINVAL;
	return sys_mmap_pgoff(addr, len, prot, flags, fd,
			      off_4k >> PAGE_ADJUST);
}

#ifdef __tilegx__
SYSCALL_DEFINE6(mmap, unsigned long, addr, unsigned long, len,
		unsigned long, prot, unsigned long, flags,
		unsigned long, fd, off_t, offset)
{
	if (offset & ((1 << PAGE_SHIFT) - 1))
		return -EINVAL;
	return sys_mmap_pgoff(addr, len, prot, flags, fd,
			      offset >> PAGE_SHIFT);
}
#endif


#undef __SYSCALL
#define __SYSCALL(nr, call) [nr] = (call),

#ifndef __tilegx__
#define sys_fadvise64_64 sys32_fadvise64_64
#define sys_readahead sys32_readahead
#endif

#define sys_execve _sys_execve
#define sys_sigaltstack _sys_sigaltstack
#define sys_rt_sigreturn _sys_rt_sigreturn
#define sys_clone _sys_clone
#ifndef __tilegx__
#define sys_cmpxchg_badaddr _sys_cmpxchg_badaddr
#endif

void *sys_call_table[__NR_syscalls] = {
	[0 ... __NR_syscalls-1] = sys_ni_syscall,
#include <asm/unistd.h>
};
