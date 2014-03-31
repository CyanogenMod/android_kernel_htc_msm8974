/*
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2001-2008 Silicon Graphics, Inc.  All rights reserved.
 */

#ifndef _ASM_IA64_SN_MSPEC_H
#define _ASM_IA64_SN_MSPEC_H

#define FETCHOP_VAR_SIZE 64 

#define FETCHOP_LOAD		0
#define FETCHOP_INCREMENT	8
#define FETCHOP_DECREMENT	16
#define FETCHOP_CLEAR		24

#define FETCHOP_STORE		0
#define FETCHOP_AND		24
#define FETCHOP_OR		32

#define FETCHOP_CLEAR_CACHE	56

#define FETCHOP_LOAD_OP(addr, op) ( \
         *(volatile long *)((char*) (addr) + (op)))

#define FETCHOP_STORE_OP(addr, op, x) ( \
         *(volatile long *)((char*) (addr) + (op)) = (long) (x))

#ifdef __KERNEL__

struct amo {
        u64 variable;
        u64 unused[7];
};


#endif 

#endif 
