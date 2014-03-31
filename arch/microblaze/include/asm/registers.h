/*
 * Copyright (C) 2008-2009 Michal Simek <monstr@monstr.eu>
 * Copyright (C) 2008-2009 PetaLogix
 * Copyright (C) 2006 Atmark Techno, Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#ifndef _ASM_MICROBLAZE_REGISTERS_H
#define _ASM_MICROBLAZE_REGISTERS_H

#define MSR_BE	(1<<0) 
#define MSR_IE	(1<<1) 
#define MSR_C	(1<<2) 
#define MSR_BIP	(1<<3) 
#define MSR_FSL	(1<<4) 
#define MSR_ICE	(1<<5) 
#define MSR_DZ	(1<<6) 
#define MSR_DCE	(1<<7) 
#define MSR_EE	(1<<8) 
#define MSR_EIP	(1<<9) 
#define MSR_CC	(1<<31)

#define FSR_IO		(1<<4) 
#define FSR_DZ		(1<<3) 
#define FSR_OF		(1<<2) 
#define FSR_UF		(1<<1) 
#define FSR_DO		(1<<0) 

# ifdef CONFIG_MMU
# define MSR_UM		(1<<11) 
# define MSR_UMS	(1<<12) 
# define MSR_VM		(1<<13) 
# define MSR_VMS	(1<<14) 

# define MSR_KERNEL	(MSR_EE | MSR_VM)
# define MSR_KERNEL_VMS	(MSR_EE | MSR_VMS)

# define	  ESR_DIZ	(1<<11) 
# define	  ESR_S		(1<<10) 

# endif 
#endif 
