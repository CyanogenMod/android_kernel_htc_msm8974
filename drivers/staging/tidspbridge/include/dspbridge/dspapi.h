/*
 * dspapi.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Includes the wrapper functions called directly by the
 * DeviceIOControl interface.
 *
 * Notes:
 *   Bridge services exported to Bridge driver are initialized by the DSPAPI on
 *   behalf of the Bridge driver. Bridge driver must not call module Init/Exit
 *   functions.
 *
 *   To ensure Bridge driver binary compatibility across different platforms,
 *   for the same processor, a Bridge driver must restrict its usage of system
 *   services to those exported by the DSPAPI library.
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

#ifndef DSPAPI_
#define DSPAPI_

#include <dspbridge/dspapi-ioctl.h>

#define BRD_API_MAJOR_VERSION   (u32)8	
#define BRD_API_MINOR_VERSION   (u32)0

extern int api_call_dev_ioctl(unsigned int cmd,
				      union trapped_args *args,
				      u32 *result, void *pr_ctxt);

extern bool api_init(void);

extern int api_init_complete2(void);

extern void api_exit(void);

extern u32 mgrwrap_enum_node_info(union trapped_args *args, void *pr_ctxt);
extern u32 mgrwrap_enum_proc_info(union trapped_args *args, void *pr_ctxt);
extern u32 mgrwrap_register_object(union trapped_args *args, void *pr_ctxt);
extern u32 mgrwrap_unregister_object(union trapped_args *args, void *pr_ctxt);
extern u32 mgrwrap_wait_for_bridge_events(union trapped_args *args,
					  void *pr_ctxt);

extern u32 mgrwrap_get_process_resources_info(union trapped_args *args,
					      void *pr_ctxt);

extern u32 procwrap_attach(union trapped_args *args, void *pr_ctxt);
extern u32 procwrap_ctrl(union trapped_args *args, void *pr_ctxt);
extern u32 procwrap_detach(union trapped_args *args, void *pr_ctxt);
extern u32 procwrap_enum_node_info(union trapped_args *args, void *pr_ctxt);
extern u32 procwrap_enum_resources(union trapped_args *args, void *pr_ctxt);
extern u32 procwrap_get_state(union trapped_args *args, void *pr_ctxt);
extern u32 procwrap_get_trace(union trapped_args *args, void *pr_ctxt);
extern u32 procwrap_load(union trapped_args *args, void *pr_ctxt);
extern u32 procwrap_register_notify(union trapped_args *args, void *pr_ctxt);
extern u32 procwrap_start(union trapped_args *args, void *pr_ctxt);
extern u32 procwrap_reserve_memory(union trapped_args *args, void *pr_ctxt);
extern u32 procwrap_un_reserve_memory(union trapped_args *args, void *pr_ctxt);
extern u32 procwrap_map(union trapped_args *args, void *pr_ctxt);
extern u32 procwrap_un_map(union trapped_args *args, void *pr_ctxt);
extern u32 procwrap_flush_memory(union trapped_args *args, void *pr_ctxt);
extern u32 procwrap_stop(union trapped_args *args, void *pr_ctxt);
extern u32 procwrap_invalidate_memory(union trapped_args *args, void *pr_ctxt);
extern u32 procwrap_begin_dma(union trapped_args *args, void *pr_ctxt);
extern u32 procwrap_end_dma(union trapped_args *args, void *pr_ctxt);

extern u32 nodewrap_allocate(union trapped_args *args, void *pr_ctxt);
extern u32 nodewrap_alloc_msg_buf(union trapped_args *args, void *pr_ctxt);
extern u32 nodewrap_change_priority(union trapped_args *args, void *pr_ctxt);
extern u32 nodewrap_connect(union trapped_args *args, void *pr_ctxt);
extern u32 nodewrap_create(union trapped_args *args, void *pr_ctxt);
extern u32 nodewrap_delete(union trapped_args *args, void *pr_ctxt);
extern u32 nodewrap_free_msg_buf(union trapped_args *args, void *pr_ctxt);
extern u32 nodewrap_get_attr(union trapped_args *args, void *pr_ctxt);
extern u32 nodewrap_get_message(union trapped_args *args, void *pr_ctxt);
extern u32 nodewrap_pause(union trapped_args *args, void *pr_ctxt);
extern u32 nodewrap_put_message(union trapped_args *args, void *pr_ctxt);
extern u32 nodewrap_register_notify(union trapped_args *args, void *pr_ctxt);
extern u32 nodewrap_run(union trapped_args *args, void *pr_ctxt);
extern u32 nodewrap_terminate(union trapped_args *args, void *pr_ctxt);
extern u32 nodewrap_get_uuid_props(union trapped_args *args, void *pr_ctxt);

extern u32 strmwrap_allocate_buffer(union trapped_args *args, void *pr_ctxt);
extern u32 strmwrap_close(union trapped_args *args, void *pr_ctxt);
extern u32 strmwrap_free_buffer(union trapped_args *args, void *pr_ctxt);
extern u32 strmwrap_get_event_handle(union trapped_args *args, void *pr_ctxt);
extern u32 strmwrap_get_info(union trapped_args *args, void *pr_ctxt);
extern u32 strmwrap_idle(union trapped_args *args, void *pr_ctxt);
extern u32 strmwrap_issue(union trapped_args *args, void *pr_ctxt);
extern u32 strmwrap_open(union trapped_args *args, void *pr_ctxt);
extern u32 strmwrap_reclaim(union trapped_args *args, void *pr_ctxt);
extern u32 strmwrap_register_notify(union trapped_args *args, void *pr_ctxt);
extern u32 strmwrap_select(union trapped_args *args, void *pr_ctxt);

extern u32 cmmwrap_calloc_buf(union trapped_args *args, void *pr_ctxt);
extern u32 cmmwrap_free_buf(union trapped_args *args, void *pr_ctxt);
extern u32 cmmwrap_get_handle(union trapped_args *args, void *pr_ctxt);
extern u32 cmmwrap_get_info(union trapped_args *args, void *pr_ctxt);

#endif 
