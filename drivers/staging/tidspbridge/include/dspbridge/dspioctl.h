/*
 * dspioctl.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Bridge driver BRD_IOCtl reserved command definitions.
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef DSPIOCTL_
#define DSPIOCTL_

#include <hw_defs.h>
#include <hw_mmu.h>

#define BRDIOCTL_RESERVEDBASE       0x8000

#define BRDIOCTL_CHNLREAD           (BRDIOCTL_RESERVEDBASE + 0x10)
#define BRDIOCTL_CHNLWRITE          (BRDIOCTL_RESERVEDBASE + 0x20)
#define BRDIOCTL_SETMMUCONFIG       (BRDIOCTL_RESERVEDBASE + 0x60)
#define BRDIOCTL_PWRCONTROL         (BRDIOCTL_RESERVEDBASE + 0x70)

#define BRDIOCTL_DEEPSLEEP          (BRDIOCTL_PWRCONTROL + 0x0)
#define BRDIOCTL_EMERGENCYSLEEP     (BRDIOCTL_PWRCONTROL + 0x1)
#define BRDIOCTL_WAKEUP             (BRDIOCTL_PWRCONTROL + 0x2)
#define BRDIOCTL_CLK_CTRL		    (BRDIOCTL_PWRCONTROL + 0x7)
#define BRDIOCTL_PWR_HIBERNATE	(BRDIOCTL_PWRCONTROL + 0x8)
#define BRDIOCTL_PRESCALE_NOTIFY (BRDIOCTL_PWRCONTROL + 0x9)
#define BRDIOCTL_POSTSCALE_NOTIFY (BRDIOCTL_PWRCONTROL + 0xA)
#define BRDIOCTL_CONSTRAINT_REQUEST (BRDIOCTL_PWRCONTROL + 0xB)

#define BRDIOCTL_NUMOFMMUTLB        32

struct bridge_ioctl_extproc {
	u32 dsp_va;		
	u32 gpp_pa;		
	
	u32 gpp_va;
	u32 size;		
	enum hw_endianism_t endianism;
	enum hw_mmu_mixed_size_t mixed_mode;
	enum hw_element_size_t elem_size;
};

#endif 
