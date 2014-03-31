/*
 * Linux 2.6.32 and later Kernel module for VMware MVP Hypervisor Support
 *
 * Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#line 5


#ifndef _ARM_INLINE_H_
#define _ARM_INLINE_H_

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_PV
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

#include "arm_types.h"
#include "arm_defs.h"

#include "arm_gcc_inline.h"


static inline _Bool
ARM_InterruptsEnabled(void)
{
	return !(ARM_ReadCPSR() & ARM_PSR_I);
}

static inline MA
ARM_ReadTTBase0(void)
{
	MA ttbase;

	ARM_MRC_CP15(TTBASE0_POINTER, ttbase);

	return ttbase & ARM_CP15_TTBASE_MASK;
}

static inline uint32
ARM_ReadVFPSystemRegister(uint8 specReg)
{
	uint32 value = 0;


	switch (specReg) {
	case ARM_VFP_SYSTEM_REG_FPSID:
		ARM_MRC_CP10(VFP_FPSID, value);
		break;
	case ARM_VFP_SYSTEM_REG_MVFR0:
		ARM_MRC_CP10(VFP_MVFR0, value);
		break;
	case ARM_VFP_SYSTEM_REG_MVFR1:
		ARM_MRC_CP10(VFP_MVFR1, value);
		break;
	case ARM_VFP_SYSTEM_REG_FPEXC:
		ARM_MRC_CP10(VFP_FPEXC, value);
		break;
	case ARM_VFP_SYSTEM_REG_FPSCR:
		ARM_MRC_CP10(VFP_FPSCR, value);
		break;
	case ARM_VFP_SYSTEM_REG_FPINST:
		ARM_MRC_CP10(VFP_FPINST, value);
		break;
	case ARM_VFP_SYSTEM_REG_FPINST2:
		ARM_MRC_CP10(VFP_FPINST2, value);
		break;
	default:
		NOT_IMPLEMENTED_JIRA(1849);
		break;
	}

	return value;
}

/**
 * @brief Write to VFP/Adv.SIMD Extension System Register
 *
 * @param specReg which VFP/Adv. SIMD Extension System Register
 * @param value desired value to be written to the System Register
 */
static inline void
ARM_WriteVFPSystemRegister(uint8 specReg,
			   uint32 value)
{

	switch (specReg) {
	case ARM_VFP_SYSTEM_REG_FPEXC:
		ARM_MCR_CP10(VFP_FPEXC, value);
		break;
	case ARM_VFP_SYSTEM_REG_FPSCR:
		ARM_MCR_CP10(VFP_FPSCR, value);
		break;
	case ARM_VFP_SYSTEM_REG_FPINST:
		ARM_MCR_CP10(VFP_FPINST, value);
		break;
	case ARM_VFP_SYSTEM_REG_FPINST2:
		ARM_MCR_CP10(VFP_FPINST2, value);
		break;
	default:
		NOT_IMPLEMENTED_JIRA(1849);
		break;
	}
}

#endif 
