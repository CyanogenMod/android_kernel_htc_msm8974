#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/sem.h>
#include <linux/msg.h>
#include <linux/shm.h>
#include <linux/stat.h>
#include <linux/syscalls.h>
#include <linux/mman.h>
#include <linux/file.h>
#include <linux/utsname.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/ipc.h>
#include <asm/syscalls.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>
#include <asm/cacheflush.h>
#include <asm/cachectl.h>

asmlinkage int old_mmap(unsigned long addr, unsigned long len,
	unsigned long prot, unsigned long flags,
	int fd, unsigned long off)
{
	if (off & ~PAGE_MASK)
		return -EINVAL;
	return sys_mmap_pgoff(addr, len, prot, flags, fd, off>>PAGE_SHIFT);
}

asmlinkage long sys_mmap2(unsigned long addr, unsigned long len,
	unsigned long prot, unsigned long flags,
	unsigned long fd, unsigned long pgoff)
{
	if (pgoff & ((1 << (PAGE_SHIFT - 12)) - 1))
		return -EINVAL;

	pgoff >>= PAGE_SHIFT - 12;

	return sys_mmap_pgoff(addr, len, prot, flags, fd, pgoff);
}

asmlinkage int sys_cacheflush(unsigned long addr, unsigned long len, int op)
{
	struct vm_area_struct *vma;

	if ((op <= 0) || (op > (CACHEFLUSH_D_PURGE|CACHEFLUSH_I)))
		return -EINVAL;

	if (addr + len < addr)
		return -EFAULT;

	down_read(&current->mm->mmap_sem);
	vma = find_vma (current->mm, addr);
	if (vma == NULL || addr < vma->vm_start || addr + len > vma->vm_end) {
		up_read(&current->mm->mmap_sem);
		return -EFAULT;
	}

	switch (op & CACHEFLUSH_D_PURGE) {
		case CACHEFLUSH_D_INVAL:
			__flush_invalidate_region((void *)addr, len);
			break;
		case CACHEFLUSH_D_WB:
			__flush_wback_region((void *)addr, len);
			break;
		case CACHEFLUSH_D_PURGE:
			__flush_purge_region((void *)addr, len);
			break;
	}

	if (op & CACHEFLUSH_I)
		flush_icache_range(addr, addr+len);

	up_read(&current->mm->mmap_sem);
	return 0;
}
