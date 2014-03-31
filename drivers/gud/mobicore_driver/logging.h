/*
 * MobiCore driver module.(interface to the secure world SWD)
 *
 * <-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 * <-- Copyright Trustonic Limited 2013 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MC_LOGGING_H_
#define _MC_LOGGING_H_

struct mc_trace_buf {
	uint32_t version; 
	uint32_t length; 
	uint32_t write_pos; 
	char  buff[1]; 
};

void mobicore_log_read(void);
long mobicore_log_setup(void);
void mobicore_log_free(void);

#endif 
