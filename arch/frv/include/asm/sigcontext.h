/* sigcontext.h: FRV signal context
 *
 * Copyright (C) 2003 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifndef _ASM_SIGCONTEXT_H
#define _ASM_SIGCONTEXT_H

#include <asm/registers.h>

struct sigcontext {
	struct user_context	sc_context;
	unsigned long		sc_oldmask; 	
} __attribute__((aligned(8)));

#endif
