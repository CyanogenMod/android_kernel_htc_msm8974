/*---------------------------------------------------------------------------+
 |  load_store.c                                                             |
 |                                                                           |
 | This file contains most of the code to interpret the FPU instructions     |
 | which load and store from user memory.                                    |
 |                                                                           |
 | Copyright (C) 1992,1993,1994,1997                                         |
 |                       W. Metzenthen, 22 Parker St, Ormond, Vic 3163,      |
 |                       Australia.  E-mail   billm@suburbia.net             |
 |                                                                           |
 |                                                                           |
 +---------------------------------------------------------------------------*/


#include <asm/uaccess.h>

#include "fpu_system.h"
#include "exception.h"
#include "fpu_emu.h"
#include "status_w.h"
#include "control_w.h"

#define _NONE_ 0		
#define _REG0_ 1		
#define _PUSH_ 3		
#define _null_ 4		

#define pop_0()	{ FPU_settag0(TAG_Empty); top++; }

static u_char const type_table[32] = {
	_PUSH_, _PUSH_, _PUSH_, _PUSH_,
	_null_, _null_, _null_, _null_,
	_REG0_, _REG0_, _REG0_, _REG0_,
	_REG0_, _REG0_, _REG0_, _REG0_,
	_NONE_, _null_, _NONE_, _PUSH_,
	_NONE_, _PUSH_, _null_, _PUSH_,
	_NONE_, _null_, _NONE_, _REG0_,
	_NONE_, _REG0_, _NONE_, _REG0_
};

u_char const data_sizes_16[32] = {
	4, 4, 8, 2, 0, 0, 0, 0,
	4, 4, 8, 2, 4, 4, 8, 2,
	14, 0, 94, 10, 2, 10, 0, 8,
	14, 0, 94, 10, 2, 10, 2, 8
};

static u_char const data_sizes_32[32] = {
	4, 4, 8, 2, 0, 0, 0, 0,
	4, 4, 8, 2, 4, 4, 8, 2,
	28, 0, 108, 10, 2, 10, 0, 8,
	28, 0, 108, 10, 2, 10, 2, 8
};

int FPU_load_store(u_char type, fpu_addr_modes addr_modes,
		   void __user * data_address)
{
	FPU_REG loaded_data;
	FPU_REG *st0_ptr;
	u_char st0_tag = TAG_Empty;	
	u_char loaded_tag;

	st0_ptr = NULL;		

	if (addr_modes.default_mode & PROTECTED) {
		if (addr_modes.default_mode == SEG32) {
			if (access_limit < data_sizes_32[type])
				math_abort(FPU_info, SIGSEGV);
		} else if (addr_modes.default_mode == PM16) {
			if (access_limit < data_sizes_16[type])
				math_abort(FPU_info, SIGSEGV);
		}
#ifdef PARANOID
		else
			EXCEPTION(EX_INTERNAL | 0x140);
#endif 
	}

	switch (type_table[type]) {
	case _NONE_:
		break;
	case _REG0_:
		st0_ptr = &st(0);	
		st0_tag = FPU_gettag0();
		break;
	case _PUSH_:
		{
			if (FPU_gettagi(-1) != TAG_Empty) {
				FPU_stack_overflow();
				return 0;
			}
			top--;
			st0_ptr = &st(0);
		}
		break;
	case _null_:
		FPU_illegal();
		return 0;
#ifdef PARANOID
	default:
		EXCEPTION(EX_INTERNAL | 0x141);
		return 0;
#endif 
	}

	switch (type) {
	case 000:		
		clear_C1();
		loaded_tag =
		    FPU_load_single((float __user *)data_address, &loaded_data);
		if ((loaded_tag == TAG_Special)
		    && isNaN(&loaded_data)
		    && (real_1op_NaN(&loaded_data) < 0)) {
			top++;
			break;
		}
		FPU_copy_to_reg0(&loaded_data, loaded_tag);
		break;
	case 001:		
		clear_C1();
		loaded_tag =
		    FPU_load_int32((long __user *)data_address, &loaded_data);
		FPU_copy_to_reg0(&loaded_data, loaded_tag);
		break;
	case 002:		
		clear_C1();
		loaded_tag =
		    FPU_load_double((double __user *)data_address,
				    &loaded_data);
		if ((loaded_tag == TAG_Special)
		    && isNaN(&loaded_data)
		    && (real_1op_NaN(&loaded_data) < 0)) {
			top++;
			break;
		}
		FPU_copy_to_reg0(&loaded_data, loaded_tag);
		break;
	case 003:		
		clear_C1();
		loaded_tag =
		    FPU_load_int16((short __user *)data_address, &loaded_data);
		FPU_copy_to_reg0(&loaded_data, loaded_tag);
		break;
	case 010:		
		clear_C1();
		FPU_store_single(st0_ptr, st0_tag,
				 (float __user *)data_address);
		break;
	case 011:		
		clear_C1();
		FPU_store_int32(st0_ptr, st0_tag, (long __user *)data_address);
		break;
	case 012:		
		clear_C1();
		FPU_store_double(st0_ptr, st0_tag,
				 (double __user *)data_address);
		break;
	case 013:		
		clear_C1();
		FPU_store_int16(st0_ptr, st0_tag, (short __user *)data_address);
		break;
	case 014:		
		clear_C1();
		if (FPU_store_single
		    (st0_ptr, st0_tag, (float __user *)data_address))
			pop_0();	
		break;
	case 015:		
		clear_C1();
		if (FPU_store_int32
		    (st0_ptr, st0_tag, (long __user *)data_address))
			pop_0();	
		break;
	case 016:		
		clear_C1();
		if (FPU_store_double
		    (st0_ptr, st0_tag, (double __user *)data_address))
			pop_0();	
		break;
	case 017:		
		clear_C1();
		if (FPU_store_int16
		    (st0_ptr, st0_tag, (short __user *)data_address))
			pop_0();	
		break;
	case 020:		
		fldenv(addr_modes, (u_char __user *) data_address);
		return 1;
	case 022:		
		frstor(addr_modes, (u_char __user *) data_address);
		return 1;
	case 023:		
		clear_C1();
		loaded_tag = FPU_load_bcd((u_char __user *) data_address);
		FPU_settag0(loaded_tag);
		break;
	case 024:		
		RE_ENTRANT_CHECK_OFF;
		FPU_access_ok(VERIFY_READ, data_address, 2);
		FPU_get_user(control_word,
			     (unsigned short __user *)data_address);
		RE_ENTRANT_CHECK_ON;
		if (partial_status & ~control_word & CW_Exceptions)
			partial_status |= (SW_Summary | SW_Backward);
		else
			partial_status &= ~(SW_Summary | SW_Backward);
#ifdef PECULIAR_486
		control_word |= 0x40;	
#endif 
		return 1;
	case 025:		
		clear_C1();
		loaded_tag =
		    FPU_load_extended((long double __user *)data_address, 0);
		FPU_settag0(loaded_tag);
		break;
	case 027:		
		clear_C1();
		loaded_tag = FPU_load_int64((long long __user *)data_address);
		if (loaded_tag == TAG_Error)
			return 0;
		FPU_settag0(loaded_tag);
		break;
	case 030:		
		fstenv(addr_modes, (u_char __user *) data_address);
		return 1;
	case 032:		
		fsave(addr_modes, (u_char __user *) data_address);
		return 1;
	case 033:		
		clear_C1();
		if (FPU_store_bcd
		    (st0_ptr, st0_tag, (u_char __user *) data_address))
			pop_0();	
		break;
	case 034:		
		RE_ENTRANT_CHECK_OFF;
		FPU_access_ok(VERIFY_WRITE, data_address, 2);
		FPU_put_user(control_word,
			     (unsigned short __user *)data_address);
		RE_ENTRANT_CHECK_ON;
		return 1;
	case 035:		
		clear_C1();
		if (FPU_store_extended
		    (st0_ptr, st0_tag, (long double __user *)data_address))
			pop_0();	
		break;
	case 036:		
		RE_ENTRANT_CHECK_OFF;
		FPU_access_ok(VERIFY_WRITE, data_address, 2);
		FPU_put_user(status_word(),
			     (unsigned short __user *)data_address);
		RE_ENTRANT_CHECK_ON;
		return 1;
	case 037:		
		clear_C1();
		if (FPU_store_int64
		    (st0_ptr, st0_tag, (long long __user *)data_address))
			pop_0();	
		break;
	}
	return 0;
}
