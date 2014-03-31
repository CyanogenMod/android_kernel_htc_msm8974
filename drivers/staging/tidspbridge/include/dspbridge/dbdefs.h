/*
 * dbdefs.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Global definitions and constants for DSP/BIOS Bridge.
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

#ifndef DBDEFS_
#define DBDEFS_

#include <linux/types.h>

#include <dspbridge/rms_sh.h>	

#define PG_SIZE4K 4096
#define PG_MASK(pg_size) (~((pg_size)-1))
#define PG_ALIGN_LOW(addr, pg_size) ((addr) & PG_MASK(pg_size))
#define PG_ALIGN_HIGH(addr, pg_size) (((addr)+(pg_size)-1) & PG_MASK(pg_size))

#define DBAPI                       int

#define DSP_MAXNAMELEN              32

#define DSP_SIGNALEVENT             0x00000001

#define DSP_PROCESSORSTATECHANGE    0x00000001
#define DSP_PROCESSORATTACH         0x00000002
#define DSP_PROCESSORDETACH         0x00000004
#define DSP_PROCESSORRESTART        0x00000008

#define DSP_MMUFAULT                0x00000010
#define DSP_SYSERROR                0x00000020
#define DSP_EXCEPTIONABORT          0x00000300
#define DSP_PWRERROR                0x00000080
#define DSP_WDTOVERFLOW	0x00000040

#define IVA_MMUFAULT                0x00000040
#define DSP_NODESTATECHANGE         0x00000100
#define DSP_NODEMESSAGEREADY        0x00000200

#define DSP_STREAMDONE              0x00001000
#define DSP_STREAMIOCOMPLETION      0x00002000

#define DSP_HGPPNODE                0xFFFFFFFF

#define DSP_TONODE                  1
#define DSP_FROMNODE                2

#define DSP_NODE_MIN_PRIORITY       1
#define DSP_NODE_MAX_PRIORITY       15

#define DSP_RMSBUFDESC RMS_BUFDESC

#define DSP_UNIT    0
#define IVA_UNIT    1

#define DSPWORD       unsigned char
#define DSPWORDSIZE     sizeof(DSPWORD)

#define    MAX_PROFILES     16

#define DSPTYPE64	0x99

#define VALID_PROC_EVENT (DSP_PROCESSORSTATECHANGE | DSP_PROCESSORATTACH | \
	DSP_PROCESSORDETACH | DSP_PROCESSORRESTART | DSP_NODESTATECHANGE | \
	DSP_STREAMDONE | DSP_STREAMIOCOMPLETION | DSP_MMUFAULT | \
	DSP_SYSERROR | DSP_WDTOVERFLOW | DSP_PWRERROR)

static inline bool is_valid_proc_event(u32 x)
{
	return (x == 0 || (x & VALID_PROC_EVENT && !(x & ~VALID_PROC_EVENT)));
}

struct dsp_uuid {
	u32 data1;
	u16 data2;
	u16 data3;
	u8 data4;
	u8 data5;
	u8 data6[6];
};

enum dsp_dcdobjtype {
	DSP_DCDNODETYPE,
	DSP_DCDPROCESSORTYPE,
	DSP_DCDLIBRARYTYPE,
	DSP_DCDCREATELIBTYPE,
	DSP_DCDEXECUTELIBTYPE,
	DSP_DCDDELETELIBTYPE,
	
	DSP_DCDMAXOBJTYPE
};

enum dsp_procstate {
	PROC_STOPPED,
	PROC_LOADED,
	PROC_RUNNING,
	PROC_ERROR
};

enum node_type {
	NODE_DEVICE,
	NODE_TASK,
	NODE_DAISSOCKET,
	NODE_MESSAGE,
	NODE_GPP
};

enum node_state {
	NODE_ALLOCATED,
	NODE_CREATED,
	NODE_RUNNING,
	NODE_PAUSED,
	NODE_DONE,
	NODE_CREATING,
	NODE_STARTING,
	NODE_PAUSING,
	NODE_TERMINATING,
	NODE_DELETING,
};

enum dsp_streamstate {
	STREAM_IDLE,
	STREAM_READY,
	STREAM_PENDING,
	STREAM_DONE
};

enum dsp_connecttype {
	CONNECTTYPE_NODEOUTPUT,
	CONNECTTYPE_GPPOUTPUT,
	CONNECTTYPE_NODEINPUT,
	CONNECTTYPE_GPPINPUT
};

enum dsp_strmmode {
	STRMMODE_PROCCOPY,	
	STRMMODE_ZEROCOPY,	
	STRMMODE_LDMA,		
	STRMMODE_RDMA		
};

enum dsp_resourceinfotype {
	DSP_RESOURCE_DYNDARAM = 0,
	DSP_RESOURCE_DYNSARAM,
	DSP_RESOURCE_DYNEXTERNAL,
	DSP_RESOURCE_DYNSRAM,
	DSP_RESOURCE_PROCLOAD
};

enum dsp_memtype {
	DSP_DYNDARAM = 0,
	DSP_DYNSARAM,
	DSP_DYNEXTERNAL,
	DSP_DYNSRAM
};

enum dsp_flushtype {
	PROC_INVALIDATE_MEM = 0,
	PROC_WRITEBACK_MEM,
	PROC_WRITEBACK_INVALIDATE_MEM,
};

struct dsp_memstat {
	u32 size;
	u32 total_free_size;
	u32 len_max_free_block;
	u32 num_free_blocks;
	u32 num_alloc_blocks;
};

struct dsp_procloadstat {
	u32 curr_load;
	u32 predicted_load;
	u32 curr_dsp_freq;
	u32 predicted_freq;
};

struct dsp_strmattr {
	u32 seg_id;		
	u32 buf_size;		
	u32 num_bufs;		
	u32 buf_alignment;	
	u32 timeout;		
	enum dsp_strmmode strm_mode;	
	
	u32 dma_chnl_id;
	u32 dma_priority;	
};

struct dsp_cbdata {
	u32 cb_data;
	u8 node_data[1];
};

struct dsp_msg {
	u32 cmd;
	u32 arg1;
	u32 arg2;
};

struct dsp_resourcereqmts {
	u32 cb_struct;
	u32 static_data_size;
	u32 global_data_size;
	u32 program_mem_size;
	u32 wc_execution_time;
	u32 wc_period;
	u32 wc_deadline;
	u32 avg_exection_time;
	u32 minimum_period;
};

struct dsp_streamconnect {
	u32 cb_struct;
	enum dsp_connecttype connect_type;
	u32 this_node_stream_index;
	void *connected_node;
	struct dsp_uuid ui_connected_node_id;
	u32 connected_node_stream_index;
};

struct dsp_nodeprofs {
	u32 heap_size;
};

struct dsp_ndbprops {
	u32 cb_struct;
	struct dsp_uuid ui_node_id;
	char ac_name[DSP_MAXNAMELEN];
	enum node_type ntype;
	u32 cache_on_gpp;
	struct dsp_resourcereqmts dsp_resource_reqmts;
	s32 prio;
	u32 stack_size;
	u32 sys_stack_size;
	u32 stack_seg;
	u32 message_depth;
	u32 num_input_streams;
	u32 num_output_streams;
	u32 timeout;
	u32 count_profiles;	
	
	struct dsp_nodeprofs node_profiles[MAX_PROFILES];
	u32 stack_seg_name;	
};

struct dsp_nodeattrin {
	u32 cb_struct;
	s32 prio;
	u32 timeout;
	u32 profile_id;
	
	u32 heap_size;
	void *pgpp_virt_addr;	
};

struct dsp_nodeinfo {
	u32 cb_struct;
	struct dsp_ndbprops nb_node_database_props;
	u32 execution_priority;
	enum node_state ns_execution_state;
	void *device_owner;
	u32 number_streams;
	struct dsp_streamconnect sc_stream_connection[16];
	u32 node_env;
};

	
struct dsp_nodeattr {
	u32 cb_struct;
	struct dsp_nodeattrin in_node_attr_in;
	u32 node_attr_inputs;
	u32 node_attr_outputs;
	struct dsp_nodeinfo node_info;
};

struct dsp_notification {
	char *name;
	void *handle;
};

struct dsp_processorattrin {
	u32 cb_struct;
	u32 timeout;
};
struct dsp_processorinfo {
	u32 cb_struct;
	int processor_family;
	int processor_type;
	u32 clock_rate;
	u32 internal_mem_size;
	u32 external_mem_size;
	u32 processor_id;
	int ty_running_rtos;
	s32 node_min_priority;
	s32 node_max_priority;
};

struct dsp_errorinfo {
	u32 err_mask;
	u32 val1;
	u32 val2;
	u32 val3;
};

struct dsp_processorstate {
	u32 cb_struct;
	enum dsp_procstate proc_state;
};

struct dsp_resourceinfo {
	u32 cb_struct;
	enum dsp_resourceinfotype resource_type;
	union {
		u32 resource;
		struct dsp_memstat mem_stat;
		struct dsp_procloadstat proc_load_stat;
	} result;
};

struct dsp_streamattrin {
	u32 cb_struct;
	u32 timeout;
	u32 segment_id;
	u32 buf_alignment;
	u32 num_bufs;
	enum dsp_strmmode strm_mode;
	u32 dma_chnl_id;
	u32 dma_priority;
};

struct dsp_bufferattr {
	u32 cb_struct;
	u32 segment_id;
	u32 buf_alignment;
};

struct dsp_streaminfo {
	u32 cb_struct;
	u32 number_bufs_allowed;
	u32 number_bufs_in_stream;
	u32 number_bytes;
	void *sync_object_handle;
	enum dsp_streamstate ss_stream_state;
};



#define DSP_MAPVIRTUALADDR          0x00000000
#define DSP_MAPPHYSICALADDR         0x00000001

#define DSP_MAPBIGENDIAN            0x00000002
#define DSP_MAPLITTLEENDIAN         0x00000000

#define DSP_MAPMIXEDELEMSIZE        0x00000004

#define DSP_MAPELEMSIZE8            0x00000008
#define DSP_MAPELEMSIZE16           0x00000010
#define DSP_MAPELEMSIZE32           0x00000020
#define DSP_MAPELEMSIZE64           0x00000040

#define DSP_MAPVMALLOCADDR         0x00000080

#define DSP_MAPDONOTLOCK	   0x00000100

#define DSP_MAP_DIR_MASK		0x3FFF

#define GEM_CACHE_LINE_SIZE     128
#define GEM_L1P_PREFETCH_SIZE   128


#define DSPPROCTYPE_C64		6410
#define IVAPROCTYPE_ARM7	470

#define MAXREGPATHLENGTH	255

#endif 
