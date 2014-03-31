/*
 * Definitions used by low-level trap handlers
 *
 * Copyright (C) 2008-2009 Michal Simek <monstr@monstr.eu>
 * Copyright (C) 2007-2009 PetaLogix
 * Copyright (C) 2007 John Williams <john.williams@petalogix.com>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file COPYING in the main directory of this
 * archive for more details.
 */

#ifndef _ASM_MICROBLAZE_ENTRY_H
#define _ASM_MICROBLAZE_ENTRY_H

#include <asm/percpu.h>
#include <asm/ptrace.h>


#define PER_CPU(var) var

# ifndef __ASSEMBLY__
DECLARE_PER_CPU(unsigned int, KSP); 
DECLARE_PER_CPU(unsigned int, KM); 
DECLARE_PER_CPU(unsigned int, ENTRY_SP); 
DECLARE_PER_CPU(unsigned int, R11_SAVE); 
DECLARE_PER_CPU(unsigned int, CURRENT_SAVE); 
# endif 

#endif 
