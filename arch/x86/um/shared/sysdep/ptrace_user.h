#include <generated/user_constants.h>

#define PT_OFFSET(r) ((r) * sizeof(long))

#define PT_SYSCALL_NR(regs) ((regs)[HOST_ORIG_AX])
#define PT_SYSCALL_NR_OFFSET PT_OFFSET(HOST_ORIG_AX)

#define PT_SYSCALL_RET_OFFSET PT_OFFSET(HOST_AX)

#define REGS_IP_INDEX HOST_IP
#define REGS_SP_INDEX HOST_SP

#ifdef __i386__
#define FP_SIZE ((HOST_FPX_SIZE > HOST_FP_SIZE) ? HOST_FPX_SIZE : HOST_FP_SIZE)
#else
#define FP_SIZE HOST_FP_SIZE

#ifndef PTRACE_OLDSETOPTIONS
#define PTRACE_OLDSETOPTIONS 21
#endif

#endif
