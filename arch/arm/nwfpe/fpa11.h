/*
    NetWinder Floating Point Emulator
    (c) Rebel.com, 1998-1999
    
    Direct questions, comments to Scott Bambrough <scottb@netwinder.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __FPA11_H__
#define __FPA11_H__

#define GET_FPA11() ((FPA11 *)(&current_thread_info()->fpstate))

#define GET_USERREG() ((struct pt_regs *)(THREAD_START_SP + (unsigned long)current_thread_info()) - 1)

#include <linux/thread_info.h>

#include "fpsr.h"		
#include "milieu.h"

struct roundingData {
    int8 mode;
    int8 precision;
    signed char exception;
};

#include "softfloat.h"

#define		typeNone		0x00
#define		typeSingle		0x01
#define		typeDouble		0x02
#define		typeExtended		0x03

typedef union tagFPREG {
	float32 fSingle;
	float64 fDouble;
#ifdef CONFIG_FPE_NWFPE_XP
	floatx80 fExtended;
#else
	u32 padding[3];
#endif
} __attribute__ ((packed,aligned(4))) FPREG;

typedef struct tagFPA11 {
 FPREG fpreg[8];	
 FPSR fpsr;		
 FPCR fpcr;		
 unsigned char fType[8];	
 int initflag;		
} __attribute__ ((packed,aligned(4))) FPA11;

extern int8 SetRoundingMode(const unsigned int);
extern int8 SetRoundingPrecision(const unsigned int);
extern void nwfpe_init_fpa(union fp_state *fp);

extern unsigned int EmulateAll(unsigned int opcode);

extern unsigned int EmulateCPDT(const unsigned int opcode);
extern unsigned int EmulateCPDO(const unsigned int opcode);
extern unsigned int EmulateCPRT(const unsigned int opcode);

extern unsigned int PerformLDF(const unsigned int opcode);
extern unsigned int PerformSTF(const unsigned int opcode);
extern unsigned int PerformLFM(const unsigned int opcode);
extern unsigned int PerformSFM(const unsigned int opcode);


extern unsigned int SingleCPDO(struct roundingData *roundData,
			       const unsigned int opcode, FPREG * rFd);
extern unsigned int DoubleCPDO(struct roundingData *roundData,
			       const unsigned int opcode, FPREG * rFd);

extern unsigned int ExtendedCPDO(struct roundingData *roundData,
				 const unsigned int opcode, FPREG * rFd);

#endif
