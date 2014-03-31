/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Derived from IRIX <sys/SN/gda.h>.
 *
 * Copyright (C) 1992 - 1997, 2000 Silicon Graphics, Inc.
 *
 * gda.h -- Contains the data structure for the global data area,
 * 	The GDA contains information communicated between the
 *	PROM, SYMMON, and the kernel.
 */
#ifndef _ASM_SN_GDA_H
#define _ASM_SN_GDA_H

#include <asm/sn/addrs.h>

#define GDA_MAGIC	0x58464552


#define GDA_VERSION	2	

#define G_MAGICOFF	0
#define G_VERSIONOFF	4
#define G_PROMOPOFF	6
#define G_MASTEROFF	8
#define G_VDSOFF	12
#define G_HKDNORMOFF	16
#define G_HKDUTLBOFF	24
#define G_HKDXUTLBOFF	32
#define G_PARTIDOFF	40
#define G_TABLEOFF	128

#ifndef __ASSEMBLY__

typedef struct gda {
	u32	g_magic;	
	u16	g_version;	
	u16	g_masterid;	
	u32	g_promop;	
	u32	g_vds;		
	void	**g_hooked_norm;
	void	**g_hooked_utlb;
	void	**g_hooked_xtlb;
	int	g_partid;	
	int	g_symmax;	
	void	*g_dbstab;	
	char	*g_nametab;	
	void	*g_ktext_repmask;
	char	g_padding[56];	
	nasid_t	g_nasidtable[MAX_COMPACT_NODES]; 
} gda_t;

#define GDA ((gda_t*) GDA_ADDR(get_nasid()))

#endif 
#define	PART_GDA_VERSION	2


#define PROMOP_MAGIC		0x0ead0000
#define PROMOP_MAGIC_MASK	0x0fff0000

#define PROMOP_BIST_SHIFT       11
#define PROMOP_BIST_MASK        (0x3 << 11)

#define PROMOP_REG		PI_ERR_STACK_ADDR_A

#define PROMOP_INVALID		(PROMOP_MAGIC | 0x00)
#define PROMOP_HALT             (PROMOP_MAGIC | 0x10)
#define PROMOP_POWERDOWN        (PROMOP_MAGIC | 0x20)
#define PROMOP_RESTART          (PROMOP_MAGIC | 0x30)
#define PROMOP_REBOOT           (PROMOP_MAGIC | 0x40)
#define PROMOP_IMODE            (PROMOP_MAGIC | 0x50)

#define PROMOP_CMD_MASK		0x00f0
#define PROMOP_OPTIONS_MASK	0xfff0

#define PROMOP_SKIP_DIAGS	0x0100		
#define PROMOP_SKIP_MEMINIT	0x0200		
#define PROMOP_SKIP_DEVINIT	0x0400		
#define PROMOP_BIST1		0x0800		
#define PROMOP_BIST2		0x1000		

#endif 
