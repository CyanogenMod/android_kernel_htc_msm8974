/*
 * arch/sh/kernel/cpu/sh2a/opcode_helper.c
 *
 * Helper for the SH-2A 32-bit opcodes.
 *
 *  Copyright (C) 2007  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/kernel.h>

unsigned int instruction_size(unsigned int insn)
{
	
	switch ((insn & 0xf00f)) {
	case 0x0000:	
	case 0x0001:	
	case 0x3001:	
		return 4;
	}

	
	switch ((insn & 0xf08f)) {
	case 0x3009:	
		return 4;
	}

	return 2;
}
