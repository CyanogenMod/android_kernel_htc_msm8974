/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2002-2006 Silicon Graphics, Inc.  All Rights Reserved.
 */
#ifndef _ASM_IA64_SN_RW_MMR_H
#define _ASM_IA64_SN_RW_MMR_H




extern long pio_phys_read_mmr(volatile long *mmr); 
extern void pio_phys_write_mmr(volatile long *mmr, long val);
extern void pio_atomic_phys_write_mmrs(volatile long *mmr1, long val1, volatile long *mmr2, long val2); 

#endif 
