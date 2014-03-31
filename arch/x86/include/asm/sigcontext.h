#ifndef _ASM_X86_SIGCONTEXT_H
#define _ASM_X86_SIGCONTEXT_H

#include <linux/compiler.h>
#include <linux/types.h>

#define FP_XSTATE_MAGIC1	0x46505853U
#define FP_XSTATE_MAGIC2	0x46505845U
#define FP_XSTATE_MAGIC2_SIZE	sizeof(FP_XSTATE_MAGIC2)

struct _fpx_sw_bytes {
	__u32 magic1;		
	__u32 extended_size;	
	__u64 xstate_bv;
	__u32 xstate_size;	
	__u32 padding[7];	
};

#ifdef __i386__
struct _fpreg {
	unsigned short significand[4];
	unsigned short exponent;
};

struct _fpxreg {
	unsigned short significand[4];
	unsigned short exponent;
	unsigned short padding[3];
};

struct _xmmreg {
	unsigned long element[4];
};

struct _fpstate {
	
	unsigned long	cw;
	unsigned long	sw;
	unsigned long	tag;
	unsigned long	ipoff;
	unsigned long	cssel;
	unsigned long	dataoff;
	unsigned long	datasel;
	struct _fpreg	_st[8];
	unsigned short	status;
	unsigned short	magic;		

	
	unsigned long	_fxsr_env[6];	
	unsigned long	mxcsr;
	unsigned long	reserved;
	struct _fpxreg	_fxsr_st[8];	
	struct _xmmreg	_xmm[8];
	unsigned long	padding1[44];

	union {
		unsigned long	padding2[12];
		struct _fpx_sw_bytes sw_reserved; 
	};
};

#define X86_FXSR_MAGIC		0x0000

#ifdef __KERNEL__
struct sigcontext {
	unsigned short gs, __gsh;
	unsigned short fs, __fsh;
	unsigned short es, __esh;
	unsigned short ds, __dsh;
	unsigned long di;
	unsigned long si;
	unsigned long bp;
	unsigned long sp;
	unsigned long bx;
	unsigned long dx;
	unsigned long cx;
	unsigned long ax;
	unsigned long trapno;
	unsigned long err;
	unsigned long ip;
	unsigned short cs, __csh;
	unsigned long flags;
	unsigned long sp_at_signal;
	unsigned short ss, __ssh;

	void __user *fpstate;		
	unsigned long oldmask;
	unsigned long cr2;
};
#else 
struct sigcontext {
	unsigned short gs, __gsh;
	unsigned short fs, __fsh;
	unsigned short es, __esh;
	unsigned short ds, __dsh;
	unsigned long edi;
	unsigned long esi;
	unsigned long ebp;
	unsigned long esp;
	unsigned long ebx;
	unsigned long edx;
	unsigned long ecx;
	unsigned long eax;
	unsigned long trapno;
	unsigned long err;
	unsigned long eip;
	unsigned short cs, __csh;
	unsigned long eflags;
	unsigned long esp_at_signal;
	unsigned short ss, __ssh;
	struct _fpstate __user *fpstate;
	unsigned long oldmask;
	unsigned long cr2;
};
#endif 

#else 

struct _fpstate {
	__u16	cwd;
	__u16	swd;
	__u16	twd;		
	__u16	fop;
	__u64	rip;
	__u64	rdp;
	__u32	mxcsr;
	__u32	mxcsr_mask;
	__u32	st_space[32];	
	__u32	xmm_space[64];	
	__u32	reserved2[12];
	union {
		__u32	reserved3[12];
		struct _fpx_sw_bytes sw_reserved; 
	};
};

#ifdef __KERNEL__
struct sigcontext {
	unsigned long r8;
	unsigned long r9;
	unsigned long r10;
	unsigned long r11;
	unsigned long r12;
	unsigned long r13;
	unsigned long r14;
	unsigned long r15;
	unsigned long di;
	unsigned long si;
	unsigned long bp;
	unsigned long bx;
	unsigned long dx;
	unsigned long ax;
	unsigned long cx;
	unsigned long sp;
	unsigned long ip;
	unsigned long flags;
	unsigned short cs;
	unsigned short gs;
	unsigned short fs;
	unsigned short __pad0;
	unsigned long err;
	unsigned long trapno;
	unsigned long oldmask;
	unsigned long cr2;

	void __user *fpstate;		
	unsigned long reserved1[8];
};
#else 
struct sigcontext {
	__u64 r8;
	__u64 r9;
	__u64 r10;
	__u64 r11;
	__u64 r12;
	__u64 r13;
	__u64 r14;
	__u64 r15;
	__u64 rdi;
	__u64 rsi;
	__u64 rbp;
	__u64 rbx;
	__u64 rdx;
	__u64 rax;
	__u64 rcx;
	__u64 rsp;
	__u64 rip;
	__u64 eflags;		
	__u16 cs;
	__u16 gs;
	__u16 fs;
	__u16 __pad0;
	__u64 err;
	__u64 trapno;
	__u64 oldmask;
	__u64 cr2;
	struct _fpstate __user *fpstate;	
#ifdef __ILP32__
	__u32 __fpstate_pad;
#endif
	__u64 reserved1[8];
};
#endif 

#endif 

struct _xsave_hdr {
	__u64 xstate_bv;
	__u64 reserved1[2];
	__u64 reserved2[5];
};

struct _ymmh_state {
	
	__u32 ymmh_space[64];
};

struct _xstate {
	struct _fpstate fpstate;
	struct _xsave_hdr xstate_hdr;
	struct _ymmh_state ymmh;
	
};

#endif 
