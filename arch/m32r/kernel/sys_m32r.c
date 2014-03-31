
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/smp.h>
#include <linux/sem.h>
#include <linux/msg.h>
#include <linux/shm.h>
#include <linux/stat.h>
#include <linux/syscalls.h>
#include <linux/mman.h>
#include <linux/file.h>
#include <linux/utsname.h>
#include <linux/ipc.h>

#include <asm/uaccess.h>
#include <asm/cachectl.h>
#include <asm/cacheflush.h>
#include <asm/syscall.h>
#include <asm/unistd.h>

asmlinkage int sys_tas(int __user *addr)
{
	int oldval;

	if (!access_ok(VERIFY_WRITE, addr, sizeof (int)))
		return -EFAULT;

	__asm__ __volatile__ (
		DCACHE_CLEAR("%0", "r4", "%1")
		"	.fillinsn\n"
		"1:\n"
		"	lock	%0, @%1	    ->	unlock	%2, @%1\n"
		"2:\n"
		".section .fixup,\"ax\"\n"
		"	.balign 4\n"
		"3:	ldi	%0, #%3\n"
		"	seth	r14, #high(2b)\n"
		"	or3	r14, r14, #low(2b)\n"
		"	jmp	r14\n"
		".previous\n"
		".section __ex_table,\"a\"\n"
		"	.balign 4\n"
		"	.long 1b,3b\n"
		".previous\n"
		: "=&r" (oldval)
		: "r" (addr), "r" (1), "i"(-EFAULT)
		: "r14", "memory"
#ifdef CONFIG_CHIP_M32700_TS1
		  , "r4"
#endif 
	);

	return oldval;
}

asmlinkage int sys_cacheflush(void *addr, int bytes, int cache)
{
	
	_flush_cache_all();
	return 0;
}

asmlinkage int sys_cachectl(char *addr, int nbytes, int op)
{
	
	return -ENOSYS;
}

int kernel_execve(const char *filename,
		  const char *const argv[],
		  const char *const envp[])
{
	register long __scno __asm__ ("r7") = __NR_execve;
	register long __arg3 __asm__ ("r2") = (long)(envp);
	register long __arg2 __asm__ ("r1") = (long)(argv);
	register long __res __asm__ ("r0") = (long)(filename);
	__asm__ __volatile__ (
		"trap #" SYSCALL_VECTOR "|| nop"
		: "=r" (__res)
		: "r" (__scno), "0" (__res), "r" (__arg2),
			"r" (__arg3)
		: "memory");
	return __res;
}
