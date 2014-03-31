/*
 * Linux/PA-RISC Project (http://www.parisc-linux.org/)
 *
 * Floating-point emulation code
 *  Copyright (C) 2001 Hewlett-Packard (Paul Bame) <bame@debian.org>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 *  linux/arch/math-emu/driver.c.c
 *
 *	decodes and dispatches unimplemented FPU instructions
 *
 *  Copyright (C) 1999, 2000  Philipp Rumpf <prumpf@tux.org>
 *  Copyright (C) 2001	      Hewlett-Packard <bame@debian.org>
 */

#include <linux/sched.h>
#include "float.h"
#include "math-emu.h"


#define fptpos 31
#define fpr1pos 10
#define extru(r,pos,len) (((r) >> (31-(pos))) & (( 1 << (len)) - 1))

#define FPUDEBUG 0

struct exc_reg {
	unsigned int exception : 6;
	unsigned int ei : 26;
};

#define FP0CE_UID(i) (((i) >> 6) & 3)
#define FP0CE_CLASS(i) (((i) >> 9) & 3)
#define FP0CE_SUBOP(i) (((i) >> 13) & 7)
#define FP0CE_SUBOP1(i) (((i) >> 15) & 7) 
#define FP0C_FORMAT(i) (((i) >> 11) & 3)
#define FP0E_FORMAT(i) (((i) >> 11) & 1)

#define FPPM_SUBOP(i) (((i) >> 9) & 0x1f)

#define FP2E_SUBOP(i)  (((i) >> 5) & 1)
#define FP2E_FORMAT(i) (((i) >> 11) & 1)

#define FPx6_FORMAT(i) ((i) & 0x1f)

#define FPSW_FLAGS(w) ((w) >> 27)
#define FPSW_ENABLE(w) ((w) & 0x1f)
#define FPSW_V (1<<4)
#define FPSW_Z (1<<3)
#define FPSW_O (1<<2)
#define FPSW_U (1<<1)
#define FPSW_I (1<<0)

int
handle_fpe(struct pt_regs *regs)
{
	extern void printbinary(unsigned long x, int nbits);
	struct siginfo si;
	unsigned int orig_sw, sw;
	int signalcode;
	__u64 frcopy[36];

	memcpy(frcopy, regs->fr, sizeof regs->fr);
	frcopy[32] = 0;

	memcpy(&orig_sw, frcopy, sizeof(orig_sw));

	if (FPUDEBUG) {
		printk(KERN_DEBUG "FP VZOUICxxxxCQCQCQCQCQCRMxxTDVZOUI ->\n   ");
		printbinary(orig_sw, 32);
		printk(KERN_DEBUG "\n");
	}

	signalcode = decode_fpu(frcopy, 0x666);

	
	memcpy(&sw, frcopy, sizeof(sw));
	if (FPUDEBUG) {
		printk(KERN_DEBUG "VZOUICxxxxCQCQCQCQCQCRMxxTDVZOUI decode_fpu returns %d|0x%x\n",
			signalcode >> 24, signalcode & 0xffffff);
		printbinary(sw, 32);
		printk(KERN_DEBUG "\n");
	}

	memcpy(regs->fr, frcopy, sizeof regs->fr);
	if (signalcode != 0) {
	    si.si_signo = signalcode >> 24;
	    si.si_errno = 0;
	    si.si_code = signalcode & 0xffffff;
	    si.si_addr = (void __user *) regs->iaoq[0];
	    force_sig_info(si.si_signo, &si, current);
	    return -1;
	}

	return signalcode ? -1 : 0;
}
