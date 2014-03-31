/*
 * _tiomap_pwr.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Definitions and types for the DSP wake/sleep routines.
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

#ifndef _TIOMAP_PWR_
#define _TIOMAP_PWR_

#ifdef CONFIG_PM
extern s32 dsp_test_sleepstate;
#endif

extern struct mailbox_context mboxsetting;

extern int wake_dsp(struct bridge_dev_context *dev_context,
							void *pargs);

extern int sleep_dsp(struct bridge_dev_context *dev_context,
			    u32 dw_cmd, void *pargs);
extern void interrupt_dsp(struct bridge_dev_context *dev_context,
							u16 mb_val);

extern int dsp_peripheral_clk_ctrl(struct bridge_dev_context
					*dev_context, void *pargs);
int handle_hibernation_from_dsp(struct bridge_dev_context *dev_context);
int post_scale_dsp(struct bridge_dev_context *dev_context,
							void *pargs);
int pre_scale_dsp(struct bridge_dev_context *dev_context,
							void *pargs);
int handle_constraints_set(struct bridge_dev_context *dev_context,
				  void *pargs);

void dsp_clk_wakeup_event_ctrl(u32 clock_id, bool enable);

#endif 
