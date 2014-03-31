/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#ifndef _ASM_TILE_BACKTRACE_H
#define _ASM_TILE_BACKTRACE_H

#include <linux/types.h>

typedef bool (*BacktraceMemoryReader)(void *result,
				      unsigned long address,
				      unsigned int size,
				      void *extra);

typedef struct {
	
	unsigned long pc;

	
	unsigned long sp;

	
	unsigned long fp;

	
	unsigned long initial_frame_caller_pc;

	
	BacktraceMemoryReader read_memory_func;

	
	void *read_memory_func_extra;

} BacktraceIterator;


typedef enum {

	
	PC_LOC_UNKNOWN,

	
	PC_LOC_IN_LR,

	
	PC_LOC_ON_STACK

} CallerPCLocation;


typedef enum {

	
	SP_LOC_UNKNOWN,

	
	SP_LOC_IN_R52,

	SP_LOC_OFFSET

} CallerSPLocation;


enum {
	ONE_BUNDLE_AGO_FLAG = 1,

	PC_IN_LR_FLAG = 2,

	NUM_INFO_OP_FLAGS = 2,

	
	MAX_INFO_OPS_PER_BUNDLE = 2
};


enum {
	

	CALLER_UNKNOWN_BASE = 2,

	CALLER_SP_IN_R52_BASE = 4,

	CALLER_SP_OFFSET_BASE = 8,
};


typedef struct {

	
	CallerPCLocation pc_location : 8;

	
	CallerSPLocation sp_location : 8;

	uint16_t sp_offset;

	
	bool at_terminating_bundle;


	bool sp_clobber_follows;

	int16_t next_info_operand;

	
	bool is_next_info_operand_adjacent;

} CallerLocation;

extern void backtrace_init(BacktraceIterator *state,
                          BacktraceMemoryReader read_memory_func,
                          void *read_memory_func_extra,
                          unsigned long pc, unsigned long lr,
                          unsigned long sp, unsigned long r52);


extern bool backtrace_next(BacktraceIterator *state);

#endif 
