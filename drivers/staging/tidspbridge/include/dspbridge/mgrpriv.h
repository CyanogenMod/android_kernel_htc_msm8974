/*
 * mgrpriv.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Global MGR constants and types, shared by PROC, MGR, and DSP API.
 *
 * Copyright (C) 2008 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef MGRPRIV_
#define MGRPRIV_

#define MGR_MAXTLBENTRIES  32

struct mgr_object;

struct mgr_tlbentry {
	u32 dsp_virt;	
	u32 gpp_phys;	
};

struct mgr_processorextinfo {
	struct dsp_processorinfo ty_basic;	
	
	struct mgr_tlbentry ty_tlb[MGR_MAXTLBENTRIES];
};

#endif 
