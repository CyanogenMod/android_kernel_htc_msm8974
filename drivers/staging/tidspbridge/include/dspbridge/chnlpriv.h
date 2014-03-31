/*
 * chnlpriv.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Private channel header shared between DSPSYS, DSPAPI and
 * Bridge driver modules.
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

#ifndef CHNLPRIV_
#define CHNLPRIV_

#include <dspbridge/chnldefs.h>
#include <dspbridge/devdefs.h>
#include <dspbridge/sync.h>

#define CHNL_MAXCHANNELS    32	

#define CHNL_PCPY       0	

#define CHNL_STATEREADY		0	
#define CHNL_STATECANCEL	1	
#define CHNL_STATEEOS		2	

#define CHNL_IS_INPUT(mode)      (mode & CHNL_MODEFROMDSP)
#define CHNL_IS_OUTPUT(mode)     (!CHNL_IS_INPUT(mode))

#define CHNL_TYPESM         1	

struct chnl_info {
	struct chnl_mgr *chnl_mgr;	
	u32 cnhl_id;		
	void *event_obj;	
	
	struct sync_object *sync_event;
	s8 mode;		
	u8 state;		
	u32 bytes_tx;		
	u32 cio_cs;		
	u32 cio_reqs;		
	u32 process;		
};

struct chnl_mgrinfo {
	u8 type;		
	
	struct chnl_object *chnl_obj;
	u8 open_channels;	
	u8 max_channels;	
};

struct chnl_mgrattrs {
	
	u8 max_channels;
	u32 word_size;		
};

#endif 
