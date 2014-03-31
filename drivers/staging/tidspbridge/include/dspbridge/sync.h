/*
 * sync.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Provide synchronization services.
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

#ifndef _SYNC_H
#define _SYNC_H

#include <dspbridge/dbdefs.h>
#include <dspbridge/host_os.h>


#define SYNC_INFINITE  0xffffffff

struct sync_object{
	struct completion comp;
	struct completion *multi_comp;
};


static inline void sync_init_event(struct sync_object *event)
{
	init_completion(&event->comp);
	event->multi_comp = NULL;
}


static inline void sync_reset_event(struct sync_object *event)
{
	INIT_COMPLETION(event->comp);
	event->multi_comp = NULL;
}


void sync_set_event(struct sync_object *event);


static inline int sync_wait_on_event(struct sync_object *event,
							unsigned timeout)
{
	int res;

	res = wait_for_completion_interruptible_timeout(&event->comp,
						msecs_to_jiffies(timeout));
	if (!res)
		res = -ETIME;
	else if (res > 0)
		res = 0;

	return res;
}


int sync_wait_on_multiple_events(struct sync_object **events,
				     unsigned count, unsigned timeout,
				     unsigned *index);

#endif 
