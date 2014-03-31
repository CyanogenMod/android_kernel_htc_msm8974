
#include <linux/linkage.h>
#include <linux/sys.h>
#include <linux/cache.h>
#include <asm/asm-offsets.h>

#define __SYSCALL_I386(nr, sym, compat) extern asmlinkage void sym(void) ;
#include <asm/syscalls_32.h>
#undef __SYSCALL_I386

#define __SYSCALL_I386(nr, sym, compat) [nr] = sym,

typedef asmlinkage void (*sys_call_ptr_t)(void);

extern asmlinkage void sys_ni_syscall(void);

const sys_call_ptr_t sys_call_table[__NR_syscall_max+1] = {
	[0 ... __NR_syscall_max] = &sys_ni_syscall,
#include <asm/syscalls_32.h>
};
