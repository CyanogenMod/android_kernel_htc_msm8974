/******************************************************************************
 * x86_emulate.h
 *
 * Generic x86 (32-bit and 64-bit) instruction decoder and emulator.
 *
 * Copyright (c) 2005 Keir Fraser
 *
 * From: xen-unstable 10676:af9809f51f81a3c43f276f00c81a52ef558afda4
 */

#ifndef _ASM_X86_KVM_X86_EMULATE_H
#define _ASM_X86_KVM_X86_EMULATE_H

#include <asm/desc_defs.h>

struct x86_emulate_ctxt;
enum x86_intercept;
enum x86_intercept_stage;

struct x86_exception {
	u8 vector;
	bool error_code_valid;
	u16 error_code;
	bool nested_page_fault;
	u64 address; 
};

struct x86_instruction_info {
	u8  intercept;          
	u8  rep_prefix;         
	u8  modrm_mod;		
	u8  modrm_reg;          
	u8  modrm_rm;		
	u64 src_val;            
	u8  src_bytes;          
	u8  dst_bytes;          
	u8  ad_bytes;           
	u64 next_rip;           
};

#define X86EMUL_CONTINUE        0
#define X86EMUL_UNHANDLEABLE    1
#define X86EMUL_PROPAGATE_FAULT 2 
#define X86EMUL_RETRY_INSTR     3 
#define X86EMUL_CMPXCHG_FAILED  4 
#define X86EMUL_IO_NEEDED       5 
#define X86EMUL_INTERCEPTED     6 

struct x86_emulate_ops {
	int (*read_std)(struct x86_emulate_ctxt *ctxt,
			unsigned long addr, void *val,
			unsigned int bytes,
			struct x86_exception *fault);

	int (*write_std)(struct x86_emulate_ctxt *ctxt,
			 unsigned long addr, void *val, unsigned int bytes,
			 struct x86_exception *fault);
	int (*fetch)(struct x86_emulate_ctxt *ctxt,
		     unsigned long addr, void *val, unsigned int bytes,
		     struct x86_exception *fault);

	int (*read_emulated)(struct x86_emulate_ctxt *ctxt,
			     unsigned long addr, void *val, unsigned int bytes,
			     struct x86_exception *fault);

	int (*write_emulated)(struct x86_emulate_ctxt *ctxt,
			      unsigned long addr, const void *val,
			      unsigned int bytes,
			      struct x86_exception *fault);

	int (*cmpxchg_emulated)(struct x86_emulate_ctxt *ctxt,
				unsigned long addr,
				const void *old,
				const void *new,
				unsigned int bytes,
				struct x86_exception *fault);
	void (*invlpg)(struct x86_emulate_ctxt *ctxt, ulong addr);

	int (*pio_in_emulated)(struct x86_emulate_ctxt *ctxt,
			       int size, unsigned short port, void *val,
			       unsigned int count);

	int (*pio_out_emulated)(struct x86_emulate_ctxt *ctxt,
				int size, unsigned short port, const void *val,
				unsigned int count);

	bool (*get_segment)(struct x86_emulate_ctxt *ctxt, u16 *selector,
			    struct desc_struct *desc, u32 *base3, int seg);
	void (*set_segment)(struct x86_emulate_ctxt *ctxt, u16 selector,
			    struct desc_struct *desc, u32 base3, int seg);
	unsigned long (*get_cached_segment_base)(struct x86_emulate_ctxt *ctxt,
						 int seg);
	void (*get_gdt)(struct x86_emulate_ctxt *ctxt, struct desc_ptr *dt);
	void (*get_idt)(struct x86_emulate_ctxt *ctxt, struct desc_ptr *dt);
	void (*set_gdt)(struct x86_emulate_ctxt *ctxt, struct desc_ptr *dt);
	void (*set_idt)(struct x86_emulate_ctxt *ctxt, struct desc_ptr *dt);
	ulong (*get_cr)(struct x86_emulate_ctxt *ctxt, int cr);
	int (*set_cr)(struct x86_emulate_ctxt *ctxt, int cr, ulong val);
	void (*set_rflags)(struct x86_emulate_ctxt *ctxt, ulong val);
	int (*cpl)(struct x86_emulate_ctxt *ctxt);
	int (*get_dr)(struct x86_emulate_ctxt *ctxt, int dr, ulong *dest);
	int (*set_dr)(struct x86_emulate_ctxt *ctxt, int dr, ulong value);
	int (*set_msr)(struct x86_emulate_ctxt *ctxt, u32 msr_index, u64 data);
	int (*get_msr)(struct x86_emulate_ctxt *ctxt, u32 msr_index, u64 *pdata);
	int (*read_pmc)(struct x86_emulate_ctxt *ctxt, u32 pmc, u64 *pdata);
	void (*halt)(struct x86_emulate_ctxt *ctxt);
	void (*wbinvd)(struct x86_emulate_ctxt *ctxt);
	int (*fix_hypercall)(struct x86_emulate_ctxt *ctxt);
	void (*get_fpu)(struct x86_emulate_ctxt *ctxt); 
	void (*put_fpu)(struct x86_emulate_ctxt *ctxt); 
	int (*intercept)(struct x86_emulate_ctxt *ctxt,
			 struct x86_instruction_info *info,
			 enum x86_intercept_stage stage);

	bool (*get_cpuid)(struct x86_emulate_ctxt *ctxt,
			 u32 *eax, u32 *ebx, u32 *ecx, u32 *edx);
};

typedef u32 __attribute__((vector_size(16))) sse128_t;

struct operand {
	enum { OP_REG, OP_MEM, OP_IMM, OP_XMM, OP_NONE } type;
	unsigned int bytes;
	union {
		unsigned long orig_val;
		u64 orig_val64;
	};
	union {
		unsigned long *reg;
		struct segmented_address {
			ulong ea;
			unsigned seg;
		} mem;
		unsigned xmm;
	} addr;
	union {
		unsigned long val;
		u64 val64;
		char valptr[sizeof(unsigned long) + 2];
		sse128_t vec_val;
	};
};

struct fetch_cache {
	u8 data[15];
	unsigned long start;
	unsigned long end;
};

struct read_cache {
	u8 data[1024];
	unsigned long pos;
	unsigned long end;
};

struct x86_emulate_ctxt {
	struct x86_emulate_ops *ops;

	
	unsigned long eflags;
	unsigned long eip; 
	
	int mode;

	
	int interruptibility;

	bool guest_mode; 
	bool perm_ok; 
	bool only_vendor_specific_insn;

	bool have_exception;
	struct x86_exception exception;

	
	u8 twobyte;
	u8 b;
	u8 intercept;
	u8 lock_prefix;
	u8 rep_prefix;
	u8 op_bytes;
	u8 ad_bytes;
	u8 rex_prefix;
	struct operand src;
	struct operand src2;
	struct operand dst;
	bool has_seg_override;
	u8 seg_override;
	u64 d;
	int (*execute)(struct x86_emulate_ctxt *ctxt);
	int (*check_perm)(struct x86_emulate_ctxt *ctxt);
	
	u8 modrm;
	u8 modrm_mod;
	u8 modrm_reg;
	u8 modrm_rm;
	u8 modrm_seg;
	bool rip_relative;
	unsigned long _eip;
	
	unsigned long regs[NR_VCPU_REGS];
	struct operand memop;
	struct operand *memopp;
	struct fetch_cache fetch;
	struct read_cache io_read;
	struct read_cache mem_read;
};

#define REPE_PREFIX	0xf3
#define REPNE_PREFIX	0xf2

#define X86EMUL_MODE_REAL     0	
#define X86EMUL_MODE_VM86     1	
#define X86EMUL_MODE_PROT16   2	
#define X86EMUL_MODE_PROT32   4	
#define X86EMUL_MODE_PROT64   8	

#define X86EMUL_MODE_PROT     (X86EMUL_MODE_PROT16|X86EMUL_MODE_PROT32| \
			       X86EMUL_MODE_PROT64)

#define X86EMUL_CPUID_VENDOR_AuthenticAMD_ebx 0x68747541
#define X86EMUL_CPUID_VENDOR_AuthenticAMD_ecx 0x444d4163
#define X86EMUL_CPUID_VENDOR_AuthenticAMD_edx 0x69746e65

#define X86EMUL_CPUID_VENDOR_AMDisbetterI_ebx 0x69444d41
#define X86EMUL_CPUID_VENDOR_AMDisbetterI_ecx 0x21726574
#define X86EMUL_CPUID_VENDOR_AMDisbetterI_edx 0x74656273

#define X86EMUL_CPUID_VENDOR_GenuineIntel_ebx 0x756e6547
#define X86EMUL_CPUID_VENDOR_GenuineIntel_ecx 0x6c65746e
#define X86EMUL_CPUID_VENDOR_GenuineIntel_edx 0x49656e69

enum x86_intercept_stage {
	X86_ICTP_NONE = 0,   
	X86_ICPT_PRE_EXCEPT,
	X86_ICPT_POST_EXCEPT,
	X86_ICPT_POST_MEMACCESS,
};

enum x86_intercept {
	x86_intercept_none,
	x86_intercept_cr_read,
	x86_intercept_cr_write,
	x86_intercept_clts,
	x86_intercept_lmsw,
	x86_intercept_smsw,
	x86_intercept_dr_read,
	x86_intercept_dr_write,
	x86_intercept_lidt,
	x86_intercept_sidt,
	x86_intercept_lgdt,
	x86_intercept_sgdt,
	x86_intercept_lldt,
	x86_intercept_sldt,
	x86_intercept_ltr,
	x86_intercept_str,
	x86_intercept_rdtsc,
	x86_intercept_rdpmc,
	x86_intercept_pushf,
	x86_intercept_popf,
	x86_intercept_cpuid,
	x86_intercept_rsm,
	x86_intercept_iret,
	x86_intercept_intn,
	x86_intercept_invd,
	x86_intercept_pause,
	x86_intercept_hlt,
	x86_intercept_invlpg,
	x86_intercept_invlpga,
	x86_intercept_vmrun,
	x86_intercept_vmload,
	x86_intercept_vmsave,
	x86_intercept_vmmcall,
	x86_intercept_stgi,
	x86_intercept_clgi,
	x86_intercept_skinit,
	x86_intercept_rdtscp,
	x86_intercept_icebp,
	x86_intercept_wbinvd,
	x86_intercept_monitor,
	x86_intercept_mwait,
	x86_intercept_rdmsr,
	x86_intercept_wrmsr,
	x86_intercept_in,
	x86_intercept_ins,
	x86_intercept_out,
	x86_intercept_outs,

	nr_x86_intercepts
};

#if defined(CONFIG_X86_32)
#define X86EMUL_MODE_HOST X86EMUL_MODE_PROT32
#elif defined(CONFIG_X86_64)
#define X86EMUL_MODE_HOST X86EMUL_MODE_PROT64
#endif

int x86_decode_insn(struct x86_emulate_ctxt *ctxt, void *insn, int insn_len);
bool x86_page_table_writing_insn(struct x86_emulate_ctxt *ctxt);
#define EMULATION_FAILED -1
#define EMULATION_OK 0
#define EMULATION_RESTART 1
#define EMULATION_INTERCEPTED 2
int x86_emulate_insn(struct x86_emulate_ctxt *ctxt);
int emulator_task_switch(struct x86_emulate_ctxt *ctxt,
			 u16 tss_selector, int idt_index, int reason,
			 bool has_error_code, u32 error_code);
int emulate_int_real(struct x86_emulate_ctxt *ctxt, int irq);
#endif 
