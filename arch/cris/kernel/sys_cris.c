
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/smp.h>
#include <linux/sem.h>
#include <linux/msg.h>
#include <linux/shm.h>
#include <linux/stat.h>
#include <linux/mman.h>
#include <linux/file.h>
#include <linux/ipc.h>

#include <asm/uaccess.h>
#include <asm/segment.h>

asmlinkage long
sys_mmap2(unsigned long addr, unsigned long len, unsigned long prot,
          unsigned long flags, unsigned long fd, unsigned long pgoff)
{
	
        return sys_mmap_pgoff(addr, len, prot, flags, fd, pgoff);
}
