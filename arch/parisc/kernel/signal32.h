/* 
 *    Copyright (C) 2001 Matthew Wilcox <willy at parisc-linux.org>
 *    Copyright (C) 2003 Carlos O'Donell <carlos at parisc-linux.org>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _PARISC64_KERNEL_SIGNAL32_H
#define _PARISC64_KERNEL_SIGNAL32_H

#include <linux/compat.h>

typedef compat_uptr_t compat_sighandler_t;

typedef struct compat_sigaltstack {
        compat_uptr_t ss_sp;
        compat_int_t ss_flags;
        compat_size_t ss_size;
} compat_stack_t;


struct compat_sigaction {
        compat_sighandler_t sa_handler;
        compat_uint_t sa_flags;
        compat_sigset_t sa_mask;               
};

struct compat_ucontext {
        compat_uint_t uc_flags;
        compat_uptr_t uc_link;
        compat_stack_t uc_stack;        
        
        compat_uint_t pad[1];
        struct compat_sigcontext uc_mcontext;
        compat_sigset_t uc_sigmask;     
};


struct k_sigaction32 {
	struct compat_sigaction sa;
};

typedef struct compat_siginfo {
        int si_signo;
        int si_errno;
        int si_code;

        union {
                int _pad[((128/sizeof(int)) - 3)];

                
                struct {
                        unsigned int _pid;      
                        unsigned int _uid;      
                } _kill;

                
                struct {
                        compat_timer_t _tid;            
                        int _overrun;           
                        char _pad[sizeof(unsigned int) - sizeof(int)];
                        compat_sigval_t _sigval;        
                        int _sys_private;       
                } _timer;

                
                struct {
                        unsigned int _pid;      
                        unsigned int _uid;      
                        compat_sigval_t _sigval;
                } _rt;

                
                struct {
                        unsigned int _pid;      
                        unsigned int _uid;      
                        int _status;            
                        compat_clock_t _utime;
                        compat_clock_t _stime;
                } _sigchld;

                
                struct {
                        unsigned int _addr;     
                } _sigfault;

                
                struct {
                        int _band;      
                        int _fd;
                } _sigpoll;
        } _sifields;
} compat_siginfo_t;

int copy_siginfo_to_user32 (compat_siginfo_t __user *to, siginfo_t *from);
int copy_siginfo_from_user32 (siginfo_t *to, compat_siginfo_t __user *from);

struct compat_regfile {
        compat_int_t rf_gr[32];
        compat_int_t rf_iasq[2];
        compat_int_t rf_iaoq[2];
        compat_int_t rf_sar;
};

#define COMPAT_SIGRETURN_TRAMP 4
#define COMPAT_SIGRESTARTBLOCK_TRAMP 5
#define COMPAT_TRAMP_SIZE (COMPAT_SIGRETURN_TRAMP + \
				COMPAT_SIGRESTARTBLOCK_TRAMP)

struct compat_rt_sigframe {
        compat_uint_t tramp[COMPAT_TRAMP_SIZE];
        compat_siginfo_t info;
        struct compat_ucontext uc;
        
        struct compat_regfile regs;
};

#define SIGFRAME32              64
#define FUNCTIONCALLFRAME32     48
#define PARISC_RT_SIGFRAME_SIZE32 (((sizeof(struct compat_rt_sigframe) + FUNCTIONCALLFRAME32) + SIGFRAME32) & -SIGFRAME32)

void sigset_32to64(sigset_t *s64, compat_sigset_t *s32);
void sigset_64to32(compat_sigset_t *s32, sigset_t *s64);
int do_sigaltstack32 (const compat_stack_t __user *uss32, 
		compat_stack_t __user *uoss32, unsigned long sp);
long restore_sigcontext32(struct compat_sigcontext __user *sc, 
		struct compat_regfile __user *rf,
		struct pt_regs *regs);
long setup_sigcontext32(struct compat_sigcontext __user *sc, 
		struct compat_regfile __user *rf,
		struct pt_regs *regs, int in_syscall);

#endif
