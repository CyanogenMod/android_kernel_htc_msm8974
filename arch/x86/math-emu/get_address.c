/*---------------------------------------------------------------------------+
 |  get_address.c                                                            |
 |                                                                           |
 | Get the effective address from an FPU instruction.                        |
 |                                                                           |
 | Copyright (C) 1992,1993,1994,1997                                         |
 |                       W. Metzenthen, 22 Parker St, Ormond, Vic 3163,      |
 |                       Australia.  E-mail   billm@suburbia.net             |
 |                                                                           |
 |                                                                           |
 +---------------------------------------------------------------------------*/


#include <linux/stddef.h>

#include <asm/uaccess.h>
#include <asm/desc.h>

#include "fpu_system.h"
#include "exception.h"
#include "fpu_emu.h"

#define FPU_WRITE_BIT 0x10

static int reg_offset[] = {
	offsetof(struct pt_regs, ax),
	offsetof(struct pt_regs, cx),
	offsetof(struct pt_regs, dx),
	offsetof(struct pt_regs, bx),
	offsetof(struct pt_regs, sp),
	offsetof(struct pt_regs, bp),
	offsetof(struct pt_regs, si),
	offsetof(struct pt_regs, di)
};

#define REG_(x) (*(long *)(reg_offset[(x)] + (u_char *)FPU_info->regs))

static int reg_offset_vm86[] = {
	offsetof(struct pt_regs, cs),
	offsetof(struct kernel_vm86_regs, ds),
	offsetof(struct kernel_vm86_regs, es),
	offsetof(struct kernel_vm86_regs, fs),
	offsetof(struct kernel_vm86_regs, gs),
	offsetof(struct pt_regs, ss),
	offsetof(struct kernel_vm86_regs, ds)
};

#define VM86_REG_(x) (*(unsigned short *) \
		(reg_offset_vm86[((unsigned)x)] + (u_char *)FPU_info->regs))

static int reg_offset_pm[] = {
	offsetof(struct pt_regs, cs),
	offsetof(struct pt_regs, ds),
	offsetof(struct pt_regs, es),
	offsetof(struct pt_regs, fs),
	offsetof(struct pt_regs, ds),	
	offsetof(struct pt_regs, ss),
	offsetof(struct pt_regs, ds)
};

#define PM_REG_(x) (*(unsigned short *) \
		(reg_offset_pm[((unsigned)x)] + (u_char *)FPU_info->regs))

static int sib(int mod, unsigned long *fpu_eip)
{
	u_char ss, index, base;
	long offset;

	RE_ENTRANT_CHECK_OFF;
	FPU_code_access_ok(1);
	FPU_get_user(base, (u_char __user *) (*fpu_eip));	
	RE_ENTRANT_CHECK_ON;
	(*fpu_eip)++;
	ss = base >> 6;
	index = (base >> 3) & 7;
	base &= 7;

	if ((mod == 0) && (base == 5))
		offset = 0;	
	else
		offset = REG_(base);

	if (index == 4) {
		
		
		if (ss)
			EXCEPTION(EX_Invalid);
	} else {
		offset += (REG_(index)) << ss;
	}

	if (mod == 1) {
		
		long displacement;
		RE_ENTRANT_CHECK_OFF;
		FPU_code_access_ok(1);
		FPU_get_user(displacement, (signed char __user *)(*fpu_eip));
		offset += displacement;
		RE_ENTRANT_CHECK_ON;
		(*fpu_eip)++;
	} else if (mod == 2 || base == 5) {	
		
		long displacement;
		RE_ENTRANT_CHECK_OFF;
		FPU_code_access_ok(4);
		FPU_get_user(displacement, (long __user *)(*fpu_eip));
		offset += displacement;
		RE_ENTRANT_CHECK_ON;
		(*fpu_eip) += 4;
	}

	return offset;
}

static unsigned long vm86_segment(u_char segment, struct address *addr)
{
	segment--;
#ifdef PARANOID
	if (segment > PREFIX_SS_) {
		EXCEPTION(EX_INTERNAL | 0x130);
		math_abort(FPU_info, SIGSEGV);
	}
#endif 
	addr->selector = VM86_REG_(segment);
	return (unsigned long)VM86_REG_(segment) << 4;
}

static long pm_address(u_char FPU_modrm, u_char segment,
		       struct address *addr, long offset)
{
	struct desc_struct descriptor;
	unsigned long base_address, limit, address, seg_top;

	segment--;

#ifdef PARANOID
	
	if (segment > PREFIX_SS_) {
		EXCEPTION(EX_INTERNAL | 0x132);
		math_abort(FPU_info, SIGSEGV);
	}
#endif 

	switch (segment) {
	case PREFIX_GS_ - 1:
		
		addr->selector = get_user_gs(FPU_info->regs);
		break;
	default:
		addr->selector = PM_REG_(segment);
	}

	descriptor = LDT_DESCRIPTOR(PM_REG_(segment));
	base_address = SEG_BASE_ADDR(descriptor);
	address = base_address + offset;
	limit = base_address
	    + (SEG_LIMIT(descriptor) + 1) * SEG_GRANULARITY(descriptor) - 1;
	if (limit < base_address)
		limit = 0xffffffff;

	if (SEG_EXPAND_DOWN(descriptor)) {
		if (SEG_G_BIT(descriptor))
			seg_top = 0xffffffff;
		else {
			seg_top = base_address + (1 << 20);
			if (seg_top < base_address)
				seg_top = 0xffffffff;
		}
		access_limit =
		    (address <= limit) || (address >= seg_top) ? 0 :
		    ((seg_top - address) >= 255 ? 255 : seg_top - address);
	} else {
		access_limit =
		    (address > limit) || (address < base_address) ? 0 :
		    ((limit - address) >= 254 ? 255 : limit - address + 1);
	}
	if (SEG_EXECUTE_ONLY(descriptor) ||
	    (!SEG_WRITE_PERM(descriptor) && (FPU_modrm & FPU_WRITE_BIT))) {
		access_limit = 0;
	}
	return address;
}


void __user *FPU_get_address(u_char FPU_modrm, unsigned long *fpu_eip,
			     struct address *addr, fpu_addr_modes addr_modes)
{
	u_char mod;
	unsigned rm = FPU_modrm & 7;
	long *cpu_reg_ptr;
	int address = 0;	

	if (!addr_modes.default_mode && (FPU_modrm & FPU_WRITE_BIT)
	    && (addr_modes.override.segment == PREFIX_CS_)) {
		math_abort(FPU_info, SIGSEGV);
	}

	addr->selector = FPU_DS;	

	mod = (FPU_modrm >> 6) & 3;

	if (rm == 4 && mod != 3) {
		address = sib(mod, fpu_eip);
	} else {
		cpu_reg_ptr = &REG_(rm);
		switch (mod) {
		case 0:
			if (rm == 5) {
				
				RE_ENTRANT_CHECK_OFF;
				FPU_code_access_ok(4);
				FPU_get_user(address,
					     (unsigned long __user
					      *)(*fpu_eip));
				(*fpu_eip) += 4;
				RE_ENTRANT_CHECK_ON;
				addr->offset = address;
				return (void __user *)address;
			} else {
				address = *cpu_reg_ptr;	
				addr->offset = address;
				return (void __user *)address;
			}
		case 1:
			
			RE_ENTRANT_CHECK_OFF;
			FPU_code_access_ok(1);
			FPU_get_user(address, (signed char __user *)(*fpu_eip));
			RE_ENTRANT_CHECK_ON;
			(*fpu_eip)++;
			break;
		case 2:
			
			RE_ENTRANT_CHECK_OFF;
			FPU_code_access_ok(4);
			FPU_get_user(address, (long __user *)(*fpu_eip));
			(*fpu_eip) += 4;
			RE_ENTRANT_CHECK_ON;
			break;
		case 3:
			
			EXCEPTION(EX_Invalid);
		}
		address += *cpu_reg_ptr;
	}

	addr->offset = address;

	switch (addr_modes.default_mode) {
	case 0:
		break;
	case VM86:
		address += vm86_segment(addr_modes.override.segment, addr);
		break;
	case PM16:
	case SEG32:
		address = pm_address(FPU_modrm, addr_modes.override.segment,
				     addr, address);
		break;
	default:
		EXCEPTION(EX_INTERNAL | 0x133);
	}

	return (void __user *)address;
}

void __user *FPU_get_address_16(u_char FPU_modrm, unsigned long *fpu_eip,
				struct address *addr, fpu_addr_modes addr_modes)
{
	u_char mod;
	unsigned rm = FPU_modrm & 7;
	int address = 0;	

	if (!addr_modes.default_mode && (FPU_modrm & FPU_WRITE_BIT)
	    && (addr_modes.override.segment == PREFIX_CS_)) {
		math_abort(FPU_info, SIGSEGV);
	}

	addr->selector = FPU_DS;	

	mod = (FPU_modrm >> 6) & 3;

	switch (mod) {
	case 0:
		if (rm == 6) {
			
			RE_ENTRANT_CHECK_OFF;
			FPU_code_access_ok(2);
			FPU_get_user(address,
				     (unsigned short __user *)(*fpu_eip));
			(*fpu_eip) += 2;
			RE_ENTRANT_CHECK_ON;
			goto add_segment;
		}
		break;
	case 1:
		
		RE_ENTRANT_CHECK_OFF;
		FPU_code_access_ok(1);
		FPU_get_user(address, (signed char __user *)(*fpu_eip));
		RE_ENTRANT_CHECK_ON;
		(*fpu_eip)++;
		break;
	case 2:
		
		RE_ENTRANT_CHECK_OFF;
		FPU_code_access_ok(2);
		FPU_get_user(address, (unsigned short __user *)(*fpu_eip));
		(*fpu_eip) += 2;
		RE_ENTRANT_CHECK_ON;
		break;
	case 3:
		
		EXCEPTION(EX_Invalid);
		break;
	}
	switch (rm) {
	case 0:
		address += FPU_info->regs->bx + FPU_info->regs->si;
		break;
	case 1:
		address += FPU_info->regs->bx + FPU_info->regs->di;
		break;
	case 2:
		address += FPU_info->regs->bp + FPU_info->regs->si;
		if (addr_modes.override.segment == PREFIX_DEFAULT)
			addr_modes.override.segment = PREFIX_SS_;
		break;
	case 3:
		address += FPU_info->regs->bp + FPU_info->regs->di;
		if (addr_modes.override.segment == PREFIX_DEFAULT)
			addr_modes.override.segment = PREFIX_SS_;
		break;
	case 4:
		address += FPU_info->regs->si;
		break;
	case 5:
		address += FPU_info->regs->di;
		break;
	case 6:
		address += FPU_info->regs->bp;
		if (addr_modes.override.segment == PREFIX_DEFAULT)
			addr_modes.override.segment = PREFIX_SS_;
		break;
	case 7:
		address += FPU_info->regs->bx;
		break;
	}

      add_segment:
	address &= 0xffff;

	addr->offset = address;

	switch (addr_modes.default_mode) {
	case 0:
		break;
	case VM86:
		address += vm86_segment(addr_modes.override.segment, addr);
		break;
	case PM16:
	case SEG32:
		address = pm_address(FPU_modrm, addr_modes.override.segment,
				     addr, address);
		break;
	default:
		EXCEPTION(EX_INTERNAL | 0x131);
	}

	return (void __user *)address;
}
