/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1999, 2001 Ralf Baechle
 * Copyright (C) 1999 Silicon Graphics, Inc.
 * Copyright (C) 2001 MIPS Technologies, Inc.
 */
#ifndef __ASM_SGIDEFS_H
#define __ASM_SGIDEFS_H

#ifndef __linux__
#error Use a Linux compiler or give up.
#endif

#define _MIPS_ISA_MIPS1		1
#define _MIPS_ISA_MIPS2		2
#define _MIPS_ISA_MIPS3		3
#define _MIPS_ISA_MIPS4		4
#define _MIPS_ISA_MIPS5		5
#define _MIPS_ISA_MIPS32	6
#define _MIPS_ISA_MIPS64	7

#define _MIPS_SIM_ABI32		1
#define _MIPS_SIM_NABI32	2
#define _MIPS_SIM_ABI64		3

#endif 
