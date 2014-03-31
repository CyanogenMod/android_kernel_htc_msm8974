/*
 * Setup the right wbflush routine for the different DECstations.
 *
 * Created with information from:
 *      DECstation 3100 Desktop Workstation Functional Specification
 *      DECstation 5000/200 KN02 System Module Functional Specification
 *      mipsel-linux-objdump --disassemble vmunix | grep "wbflush" :-)
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1998 Harald Koerfgen
 * Copyright (C) 2002 Maciej W. Rozycki
 */

#include <linux/init.h>

#include <asm/bootinfo.h>
#include <asm/wbflush.h>
#include <asm/barrier.h>

static void wbflush_kn01(void);
static void wbflush_kn210(void);
static void wbflush_mips(void);

void (*__wbflush) (void);

void __init wbflush_setup(void)
{
	switch (mips_machtype) {
	case MACH_DS23100:
	case MACH_DS5000_200:	
		__wbflush = wbflush_kn01;
		break;
	case MACH_DS5100:	
		__wbflush = wbflush_kn210;
		break;
	case MACH_DS5000_1XX:	
	case MACH_DS5000_XX:	
	case MACH_DS5000_2X0:	
	case MACH_DS5900:	
	default:
		__wbflush = wbflush_mips;
		break;
	}
}

static void wbflush_kn01(void)
{
    asm(".set\tpush\n\t"
	".set\tnoreorder\n\t"
	"1:\tbc0f\t1b\n\t"
	"nop\n\t"
	".set\tpop");
}

static void wbflush_kn210(void)
{
    asm(".set\tpush\n\t"
	".set\tnoreorder\n\t"
	"mfc0\t$2,$12\n\t"
	"lui\t$3,0x8000\n\t"
	"or\t$3,$2,$3\n\t"
	"mtc0\t$3,$12\n\t"
	"nop\n"
	"1:\tbc3f\t1b\n\t"
	"nop\n\t"
	"mtc0\t$2,$12\n\t"
	"nop\n\t"
	".set\tpop"
	: : : "$2", "$3");
}

static void wbflush_mips(void)
{
	__fast_iob();
}

#include <linux/module.h>

EXPORT_SYMBOL(__wbflush);
