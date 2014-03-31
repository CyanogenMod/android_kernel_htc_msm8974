/******************************************************************************
 * arch-x86_32.h
 *
 * Guest OS interface to x86 Xen.
 *
 * Copyright (c) 2004, K A Fraser
 */

#ifndef _ASM_X86_XEN_INTERFACE_H
#define _ASM_X86_XEN_INTERFACE_H

#ifdef __XEN__
#define __DEFINE_GUEST_HANDLE(name, type) \
    typedef struct { type *p; } __guest_handle_ ## name
#else
#define __DEFINE_GUEST_HANDLE(name, type) \
    typedef type * __guest_handle_ ## name
#endif

#define DEFINE_GUEST_HANDLE_STRUCT(name) \
	__DEFINE_GUEST_HANDLE(name, struct name)
#define DEFINE_GUEST_HANDLE(name) __DEFINE_GUEST_HANDLE(name, name)
#define GUEST_HANDLE(name)        __guest_handle_ ## name

#ifdef __XEN__
#if defined(__i386__)
#define set_xen_guest_handle(hnd, val)			\
	do {						\
		if (sizeof(hnd) == 8)			\
			*(uint64_t *)&(hnd) = 0;	\
		(hnd).p = val;				\
	} while (0)
#elif defined(__x86_64__)
#define set_xen_guest_handle(hnd, val)	do { (hnd).p = val; } while (0)
#endif
#else
#if defined(__i386__)
#define set_xen_guest_handle(hnd, val)			\
	do {						\
		if (sizeof(hnd) == 8)			\
			*(uint64_t *)&(hnd) = 0;	\
		(hnd) = val;				\
	} while (0)
#elif defined(__x86_64__)
#define set_xen_guest_handle(hnd, val)	do { (hnd) = val; } while (0)
#endif
#endif

#ifndef __ASSEMBLY__
__DEFINE_GUEST_HANDLE(uchar, unsigned char);
__DEFINE_GUEST_HANDLE(uint,  unsigned int);
__DEFINE_GUEST_HANDLE(ulong, unsigned long);
DEFINE_GUEST_HANDLE(char);
DEFINE_GUEST_HANDLE(int);
DEFINE_GUEST_HANDLE(long);
DEFINE_GUEST_HANDLE(void);
DEFINE_GUEST_HANDLE(uint64_t);
DEFINE_GUEST_HANDLE(uint32_t);
#endif

#ifndef HYPERVISOR_VIRT_START
#define HYPERVISOR_VIRT_START mk_unsigned_long(__HYPERVISOR_VIRT_START)
#endif

#define MACH2PHYS_VIRT_START  mk_unsigned_long(__MACH2PHYS_VIRT_START)
#define MACH2PHYS_VIRT_END    mk_unsigned_long(__MACH2PHYS_VIRT_END)
#define MACH2PHYS_NR_ENTRIES  ((MACH2PHYS_VIRT_END-MACH2PHYS_VIRT_START)>>__MACH2PHYS_SHIFT)

#define MAX_VIRT_CPUS 32

#define FIRST_RESERVED_GDT_PAGE  14
#define FIRST_RESERVED_GDT_BYTE  (FIRST_RESERVED_GDT_PAGE * 4096)
#define FIRST_RESERVED_GDT_ENTRY (FIRST_RESERVED_GDT_BYTE / 8)

#define TI_GET_DPL(_ti)		((_ti)->flags & 3)
#define TI_GET_IF(_ti)		((_ti)->flags & 4)
#define TI_SET_DPL(_ti, _dpl)	((_ti)->flags |= (_dpl))
#define TI_SET_IF(_ti, _if)	((_ti)->flags |= ((!!(_if))<<2))

#ifndef __ASSEMBLY__
struct trap_info {
    uint8_t       vector;  
    uint8_t       flags;   
    uint16_t      cs;      
    unsigned long address; 
};
DEFINE_GUEST_HANDLE_STRUCT(trap_info);

struct arch_shared_info {
    unsigned long max_pfn;                  
    
    unsigned long pfn_to_mfn_frame_list_list;
    unsigned long nmi_reason;
};
#endif	

#ifdef CONFIG_X86_32
#include "interface_32.h"
#else
#include "interface_64.h"
#endif

#ifndef __ASSEMBLY__
struct vcpu_guest_context {
    
    struct { char x[512]; } fpu_ctxt;       
#define VGCF_I387_VALID (1<<0)
#define VGCF_HVM_GUEST  (1<<1)
#define VGCF_IN_KERNEL  (1<<2)
    unsigned long flags;                    
    struct cpu_user_regs user_regs;         
    struct trap_info trap_ctxt[256];        
    unsigned long ldt_base, ldt_ents;       
    unsigned long gdt_frames[16], gdt_ents; 
    unsigned long kernel_ss, kernel_sp;     
    
    unsigned long ctrlreg[8];               
    unsigned long debugreg[8];              
#ifdef __i386__
    unsigned long event_callback_cs;        
    unsigned long event_callback_eip;
    unsigned long failsafe_callback_cs;     
    unsigned long failsafe_callback_eip;
#else
    unsigned long event_callback_eip;
    unsigned long failsafe_callback_eip;
    unsigned long syscall_callback_eip;
#endif
    unsigned long vm_assist;                
#ifdef __x86_64__
    
    uint64_t      fs_base;
    uint64_t      gs_base_kernel;
    uint64_t      gs_base_user;
#endif
};
DEFINE_GUEST_HANDLE_STRUCT(vcpu_guest_context);
#endif	

#ifdef __ASSEMBLY__
#define XEN_EMULATE_PREFIX .byte 0x0f,0x0b,0x78,0x65,0x6e ;
#define XEN_CPUID          XEN_EMULATE_PREFIX cpuid
#else
#define XEN_EMULATE_PREFIX ".byte 0x0f,0x0b,0x78,0x65,0x6e ; "
#define XEN_CPUID          XEN_EMULATE_PREFIX "cpuid"
#endif

#endif 
