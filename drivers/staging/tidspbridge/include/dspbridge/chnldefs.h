/*
 * chnldefs.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * System-wide channel objects and constants.
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

#ifndef CHNLDEFS_
#define CHNLDEFS_

#define CHNL_PICKFREE       (~0UL)	

#define CHNL_MODETODSP		0	
#define CHNL_MODEFROMDSP	1	

#define CHNL_IOCINFINITE     0xffffffff	
#define CHNL_IOCNOWAIT       0x0	

#define CHNL_IOCSTATCOMPLETE 0x0000	
#define CHNL_IOCSTATCANCEL   0x0002	
#define CHNL_IOCSTATTIMEOUT  0x0008	
#define CHNL_IOCSTATEOS      0x8000	

#define CHNL_IS_IO_COMPLETE(ioc)  (!(ioc.status & ~CHNL_IOCSTATEOS))
#define CHNL_IS_IO_CANCELLED(ioc) (ioc.status & CHNL_IOCSTATCANCEL)
#define CHNL_IS_TIMED_OUT(ioc)    (ioc.status & CHNL_IOCSTATTIMEOUT)

struct chnl_attr {
	u32 uio_reqs;		
	void *event_obj;	
	char *str_event_name;	
	void *reserved1;	
	u32 reserved2;		

};

struct chnl_ioc {
	void *buf;		
	u32 byte_size;		
	u32 buf_size;		
	u32 status;		
	u32 arg;		
};

#endif 
