/*
 * rms_sh.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * DSP/BIOS Bridge Resource Manager Server shared definitions (used on both
 * GPP and DSP sides).
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

#ifndef RMS_SH_
#define RMS_SH_

#include <dspbridge/rmstypes.h>

#define RMS_CODE                0	
#define RMS_DATA                1	

#define RMS_COMMANDBUFSIZE     256	

#define RMS_EXIT                0x80000000	
#define RMS_EXITACK             0x40000000	
#define RMS_BUFDESC             0x20000000	
#define RMS_KILLTASK            0x10000000	

struct rms_command {
	rms_word fxn;		
	rms_word arg1;		
	rms_word arg2;		
	rms_word data;		
};

struct rms_strm_def {
	rms_word bufsize;	
	rms_word nbufs;		
	rms_word segid;		
	rms_word align;		
	rms_word timeout;	
	char name[1];	
};

struct rms_msg_args {
	rms_word max_msgs;	
	rms_word segid;		
	rms_word notify_type;	
	rms_word arg_length;	
	rms_word arg_data;	
};

struct rms_more_task_args {
	rms_word priority;	
	rms_word stack_size;	
	rms_word sysstack_size;	
	rms_word stack_seg;	
	rms_word heap_addr;	
	rms_word heap_size;	
	rms_word misc;		
	
	rms_word num_input_streams;
};

#endif 
