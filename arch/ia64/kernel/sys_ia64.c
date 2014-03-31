/*
 * This file contains various system calls that have different calling
 * conventions on different platforms.
 *
 * Copyright (C) 1999-2000, 2002-2003, 2005 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/shm.h>
#include <linux/file.h>		
#include <linux/smp.h>
#include <linux/syscalls.h>
#include <linux/highuid.h>
#include <linux/hugetlb.h>

#include <asm/shmparam.h>
#include <asm/uaccess.h>

unsigned long
arch_get_unmapped_area (struct file *filp, unsigned long addr, unsigned long len,
			unsigned long pgoff, unsigned long flags)
{
	long map_shared = (flags & MAP_SHARED);
	unsigned long start_addr, align_mask = PAGE_SIZE - 1;
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;

	if (len > RGN_MAP_LIMIT)
		return -ENOMEM;

	
	if (flags & MAP_FIXED) {
		if (is_hugepage_only_range(mm, addr, len))
			return -EINVAL;
		return addr;
	}

#ifdef CONFIG_HUGETLB_PAGE
	if (REGION_NUMBER(addr) == RGN_HPAGE)
		addr = 0;
#endif
	if (!addr)
		addr = mm->free_area_cache;

	if (map_shared && (TASK_SIZE > 0xfffffffful))
		align_mask = SHMLBA - 1;

  full_search:
	start_addr = addr = (addr + align_mask) & ~align_mask;

	for (vma = find_vma(mm, addr); ; vma = vma->vm_next) {
		
		if (TASK_SIZE - len < addr || RGN_MAP_LIMIT - len < REGION_OFFSET(addr)) {
			if (start_addr != TASK_UNMAPPED_BASE) {
				
				addr = TASK_UNMAPPED_BASE;
				goto full_search;
			}
			return -ENOMEM;
		}
		if (!vma || addr + len <= vma->vm_start) {
			
			mm->free_area_cache = addr + len;
			return addr;
		}
		addr = (vma->vm_end + align_mask) & ~align_mask;
	}
}

asmlinkage long
ia64_getpriority (int which, int who)
{
	long prio;

	prio = sys_getpriority(which, who);
	if (prio >= 0) {
		force_successful_syscall_return();
		prio = 20 - prio;
	}
	return prio;
}

asmlinkage unsigned long
sys_getpagesize (void)
{
	return PAGE_SIZE;
}

asmlinkage unsigned long
ia64_brk (unsigned long brk)
{
	unsigned long retval = sys_brk(brk);
	force_successful_syscall_return();
	return retval;
}

asmlinkage long
sys_ia64_pipe (void)
{
	struct pt_regs *regs = task_pt_regs(current);
	int fd[2];
	int retval;

	retval = do_pipe_flags(fd, 0);
	if (retval)
		goto out;
	retval = fd[0];
	regs->r9 = fd[1];
  out:
	return retval;
}

int ia64_mmap_check(unsigned long addr, unsigned long len,
		unsigned long flags)
{
	unsigned long roff;

	roff = REGION_OFFSET(addr);
	if ((len > RGN_MAP_LIMIT) || (roff > (RGN_MAP_LIMIT - len)))
		return -EINVAL;
	return 0;
}

asmlinkage unsigned long
sys_mmap2 (unsigned long addr, unsigned long len, int prot, int flags, int fd, long pgoff)
{
	addr = sys_mmap_pgoff(addr, len, prot, flags, fd, pgoff);
	if (!IS_ERR((void *) addr))
		force_successful_syscall_return();
	return addr;
}

asmlinkage unsigned long
sys_mmap (unsigned long addr, unsigned long len, int prot, int flags, int fd, long off)
{
	if (offset_in_page(off) != 0)
		return -EINVAL;

	addr = sys_mmap_pgoff(addr, len, prot, flags, fd, off >> PAGE_SHIFT);
	if (!IS_ERR((void *) addr))
		force_successful_syscall_return();
	return addr;
}

asmlinkage unsigned long
ia64_mremap (unsigned long addr, unsigned long old_len, unsigned long new_len, unsigned long flags,
	     unsigned long new_addr)
{
	extern unsigned long do_mremap (unsigned long addr,
					unsigned long old_len,
					unsigned long new_len,
					unsigned long flags,
					unsigned long new_addr);

	down_write(&current->mm->mmap_sem);
	{
		addr = do_mremap(addr, old_len, new_len, flags, new_addr);
	}
	up_write(&current->mm->mmap_sem);

	if (IS_ERR((void *) addr))
		return addr;

	force_successful_syscall_return();
	return addr;
}

#ifndef CONFIG_PCI

asmlinkage long
sys_pciconfig_read (unsigned long bus, unsigned long dfn, unsigned long off, unsigned long len,
		    void *buf)
{
	return -ENOSYS;
}

asmlinkage long
sys_pciconfig_write (unsigned long bus, unsigned long dfn, unsigned long off, unsigned long len,
		     void *buf)
{
	return -ENOSYS;
}

#endif 
