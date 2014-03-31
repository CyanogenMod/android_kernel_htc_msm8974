/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */


#ifndef __ARCH_ABI_H__

#if !defined __need_int_reg_t && !defined __DOXYGEN__
# define __ARCH_ABI_H__
# include <arch/chip.h>
#endif

#ifndef __INT_REG_BITS

#if defined __tilegx__
# define __INT_REG_BITS 64
#elif defined __tilepro__
# define __INT_REG_BITS 32
#elif !defined __need_int_reg_t
# include <arch/chip.h>
# define __INT_REG_BITS CHIP_WORD_SIZE()
#else
# error Unrecognized architecture with __need_int_reg_t
#endif

#if __INT_REG_BITS == 64

#ifndef __ASSEMBLER__
typedef unsigned long long __uint_reg_t;

typedef long long __int_reg_t;
#endif

#define __INT_REG_FMT "ll"

#else

#ifndef __ASSEMBLER__
typedef unsigned long __uint_reg_t;

typedef long __int_reg_t;
#endif

#define __INT_REG_FMT "l"

#endif
#endif 


#ifndef __need_int_reg_t


#ifndef __ASSEMBLER__
typedef __uint_reg_t uint_reg_t;

typedef __int_reg_t int_reg_t;
#endif

#define INT_REG_FMT __INT_REG_FMT

#define INT_REG_BITS __INT_REG_BITS



#define TREG_FP       52   
#define TREG_TP       53   
#define TREG_SP       54   
#define TREG_LR       55   

#define TREG_LAST_GPR 55


#define TREG_SN       56   
#define TREG_IDN0     57   
#define TREG_IDN1     58   
#define TREG_UDN0     59   
#define TREG_UDN1     60   
#define TREG_UDN2     61   
#define TREG_UDN3     62   


#define TREG_ZERO     63   


#define TREG_SYSCALL_NR      10

#define TREG_SYSCALL_NR_NAME r10


#define C_ABI_SAVE_AREA_SIZE (2 * (INT_REG_BITS / 8))

#define INFO_OP_CANNOT_BACKTRACE 2


#endif 

#undef __need_int_reg_t

#endif 
