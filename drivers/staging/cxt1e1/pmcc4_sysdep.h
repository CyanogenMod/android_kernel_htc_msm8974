#ifndef _INC_PMCC4_SYSDEP_H_
#define _INC_PMCC4_SYSDEP_H_

/*-----------------------------------------------------------------------------
 * pmcc4_sysdep.h -
 *
 * Copyright (C) 2005  SBE, Inc.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */


#ifdef CONFIG_SBE_PMCC4_HDLC_V7_MODULE
#undef CONFIG_SBE_PMCC4_HDLC_V7
#define CONFIG_SBE_PMCC4_HDLC_V7  1
#endif

#ifdef CONFIG_SBE_PMCC4_NCOMM_MODULE
#undef CONFIG_SBE_PMCC4_NCOMM
#define CONFIG_SBE_PMCC4_NCOMM  1
#endif



#define FLUSH_PCI_READ()     rmb()
#define FLUSH_PCI_WRITE()    wmb()
#define FLUSH_MEM_READ()     rmb()
#define FLUSH_MEM_WRITE()    wmb()




void sd_recv_consume(void *token, size_t len, void *user);

void        sd_disable_xmit (void *user);
void        sd_enable_xmit (void *user);
int         sd_line_is_ok (void *user);
void        sd_line_is_up (void *user);
void        sd_line_is_down (void *user);
int         sd_queue_stopped (void *user);

#endif                          
