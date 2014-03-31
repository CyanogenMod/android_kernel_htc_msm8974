
#include <linux/linkage.h>
#include <linux/sys.h>
#include <linux/cache.h>
#include <generated/user_constants.h>

#define __NO_STUBS


#define stub_iopl sys_ni_syscall
#define sys_ioperm sys_ni_syscall


#define sys_mmap old_mmap

#define stub_clone sys_clone
#define stub_fork sys_fork
#define stub_vfork sys_vfork
#define stub_execve sys_execve
#define stub_rt_sigsuspend sys_rt_sigsuspend
#define stub_sigaltstack sys_sigaltstack
#define stub_rt_sigreturn sys_rt_sigreturn

#define __SYSCALL_COMMON(nr, sym, compat) __SYSCALL_64(nr, sym, compat)
#define __SYSCALL_X32(nr, sym, compat) 

#define __SYSCALL_64(nr, sym, compat) extern asmlinkage void sym(void) ;
#include <asm/syscalls_64.h>

#undef __SYSCALL_64
#define __SYSCALL_64(nr, sym, compat) [ nr ] = sym,

typedef void (*sys_call_ptr_t)(void);

extern void sys_ni_syscall(void);

const sys_call_ptr_t sys_call_table[] __cacheline_aligned = {
	[0 ... __NR_syscall_max] = &sys_ni_syscall,
#include <asm/syscalls_64.h>
};

int syscall_table_size = sizeof(sys_call_table);
