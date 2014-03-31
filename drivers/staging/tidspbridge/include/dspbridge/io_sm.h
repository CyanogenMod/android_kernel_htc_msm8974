/*
 * io_sm.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * IO dispatcher for a shared memory channel driver.
 * Also, includes macros to simulate shm via port io calls.
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

#ifndef IOSM_
#define IOSM_

#include <dspbridge/_chnl_sm.h>
#include <dspbridge/host_os.h>

#include <dspbridge/io.h>
#include <dspbridge/mbx_sh.h>	

#define DEH_BASE        MBX_DEH_BASE
#define DEH_LIMIT       MBX_DEH_LIMIT

#define IO_INPUT            0
#define IO_OUTPUT           1
#define IO_SERVICE          2

#ifdef CONFIG_TIDSPBRIDGE_DVFS
extern s32 dsp_max_opps;
extern u32 vdd1_dsp_freq[6][4];
#endif

extern void io_cancel_chnl(struct io_mgr *hio_mgr, u32 chnl);

extern void io_dpc(unsigned long ref_data);

int io_mbox_msg(struct notifier_block *self, unsigned long len, void *msg);

extern void io_request_chnl(struct io_mgr *io_manager,
			    struct chnl_object *pchnl,
			    u8 io_mode, u16 *mbx_val);

extern void iosm_schedule(struct io_mgr *io_manager);

extern int io_sh_msetting(struct io_mgr *hio_mgr, u8 desc, void *pargs);


extern u32 io_buf_size(struct io_mgr *hio_mgr);

#ifdef CONFIG_TIDSPBRIDGE_BACKTRACE
extern int print_dsp_trace_buffer(struct bridge_dev_context
					 *hbridge_context);

int dump_dsp_stack(struct bridge_dev_context *bridge_context);

void dump_dl_modules(struct bridge_dev_context *bridge_context);

void print_dsp_debug_trace(struct io_mgr *hio_mgr);
#endif

#endif 
