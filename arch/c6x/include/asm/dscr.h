/*
 *  Copyright (C) 2011 Texas Instruments Incorporated
 *  Author: Mark Salter <msalter@redhat.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */
#ifndef _ASM_C6X_DSCR_H
#define _ASM_C6X_DSCR_H

enum dscr_devstate_t {
	DSCR_DEVSTATE_ENABLED,
	DSCR_DEVSTATE_DISABLED,
};

extern void dscr_set_devstate(int devid, enum dscr_devstate_t state);

extern void dscr_rmii_reset(int id, int assert);

extern void dscr_probe(void);

#endif 
