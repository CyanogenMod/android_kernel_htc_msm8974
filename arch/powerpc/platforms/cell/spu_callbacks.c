
#undef DEBUG

#include <linux/kallsyms.h>
#include <linux/export.h>
#include <linux/syscalls.h>

#include <asm/spu.h>
#include <asm/syscalls.h>
#include <asm/unistd.h>


static void *spu_syscall_table[] = {
#define SYSCALL(func)		sys_ni_syscall,
#define COMPAT_SYS(func)	sys_ni_syscall,
#define PPC_SYS(func)		sys_ni_syscall,
#define OLDSYS(func)		sys_ni_syscall,
#define SYS32ONLY(func)		sys_ni_syscall,
#define SYSX(f, f3264, f32)	sys_ni_syscall,

#define SYSCALL_SPU(func)	sys_##func,
#define COMPAT_SYS_SPU(func)	sys_##func,
#define PPC_SYS_SPU(func)	ppc_##func,
#define SYSX_SPU(f, f3264, f32)	f,

#include <asm/systbl.h>
};

long spu_sys_callback(struct spu_syscall_block *s)
{
	long (*syscall)(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6);

	if (s->nr_ret >= ARRAY_SIZE(spu_syscall_table)) {
		pr_debug("%s: invalid syscall #%lld", __func__, s->nr_ret);
		return -ENOSYS;
	}

	syscall = spu_syscall_table[s->nr_ret];

#ifdef DEBUG
	print_symbol(KERN_DEBUG "SPU-syscall %s:", (unsigned long)syscall);
	printk("syscall%ld(%lx, %lx, %lx, %lx, %lx, %lx)\n",
			s->nr_ret,
			s->parm[0], s->parm[1], s->parm[2],
			s->parm[3], s->parm[4], s->parm[5]);
#endif

	return syscall(s->parm[0], s->parm[1], s->parm[2],
		       s->parm[3], s->parm[4], s->parm[5]);
}
EXPORT_SYMBOL_GPL(spu_sys_callback);
