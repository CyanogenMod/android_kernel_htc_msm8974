
#include <linux/linkage.h>
#include <linux/sys.h>
#include <linux/cache.h>
#include <generated/user_constants.h>

#define __NO_STUBS


#define sys_iopl sys_ni_syscall
#define sys_ioperm sys_ni_syscall

#define sys_vm86old sys_ni_syscall
#define sys_vm86 sys_ni_syscall

#define old_mmap sys_old_mmap

#define ptregs_fork sys_fork
#define ptregs_execve sys_execve
#define ptregs_iopl sys_iopl
#define ptregs_vm86old sys_vm86old
#define ptregs_clone sys_clone
#define ptregs_vm86 sys_vm86
#define ptregs_sigaltstack sys_sigaltstack
#define ptregs_vfork sys_vfork

#define __SYSCALL_I386(nr, sym, compat) extern asmlinkage void sym(void) ;
#include <asm/syscalls_32.h>

#undef __SYSCALL_I386
#define __SYSCALL_I386(nr, sym, compat) [ nr ] = sym,

typedef void (*sys_call_ptr_t)(void);

extern void sys_ni_syscall(void);

const sys_call_ptr_t sys_call_table[] __cacheline_aligned = {
	[0 ... __NR_syscall_max] = &sys_ni_syscall,
#include <asm/syscalls_32.h>
};

int syscall_table_size = sizeof(sys_call_table);
