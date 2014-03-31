/* MN10300 FPU management
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#include <asm/fpu.h>

asmlinkage
void unexpected_fpu_exception(struct pt_regs *regs, enum exception_code code)
{
	panic("An FPU exception was received, but there's no FPU enabled.");
}

int dump_fpu(struct pt_regs *regs, elf_fpregset_t *fpreg)
{
	return 0; 
}
