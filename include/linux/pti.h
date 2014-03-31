/*
 *  Copyright (C) Intel 2011
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * The PTI (Parallel Trace Interface) driver directs trace data routed from
 * various parts in the system out through the Intel Penwell PTI port and
 * out of the mobile device for analysis with a debugging tool
 * (Lauterbach, Fido). This is part of a solution for the MIPI P1149.7,
 * compact JTAG, standard.
 *
 * This header file will allow other parts of the OS to use the
 * interface to write out it's contents for debugging a mobile system.
 */

#ifndef PTI_H_
#define PTI_H_

#define PTI_LASTDWORD_DTS	0x30

struct pti_masterchannel {
	u8 master;
	u8 channel;
};

void pti_writedata(struct pti_masterchannel *mc, u8 *buf, int count);
struct pti_masterchannel *pti_request_masterchannel(u8 type,
						    const char *thread_name);
void pti_release_masterchannel(struct pti_masterchannel *mc);

#endif 
