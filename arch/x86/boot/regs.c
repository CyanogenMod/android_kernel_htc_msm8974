/* -----------------------------------------------------------------------
 *
 *   Copyright 2009 Intel Corporation; author H. Peter Anvin
 *
 *   This file is part of the Linux kernel, and is made available under
 *   the terms of the GNU General Public License version 2 or (at your
 *   option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */


#include "boot.h"

void initregs(struct biosregs *reg)
{
	memset(reg, 0, sizeof *reg);
	reg->eflags |= X86_EFLAGS_CF;
	reg->ds = ds();
	reg->es = ds();
	reg->fs = fs();
	reg->gs = gs();
}
