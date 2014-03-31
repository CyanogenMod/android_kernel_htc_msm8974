/* ppc.h -- Header file for PowerPC opcode table
   Copyright 1994, 1995, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support

This file is part of GDB, GAS, and the GNU binutils.

GDB, GAS, and the GNU binutils are free software; you can redistribute
them and/or modify them under the terms of the GNU General Public
License as published by the Free Software Foundation; either version
1, or (at your option) any later version.

GDB, GAS, and the GNU binutils are distributed in the hope that they
will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this file; see the file COPYING.  If not, write to the Free
Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef PPC_H
#define PPC_H


struct powerpc_opcode
{
  
  const char *name;

  unsigned long opcode;

  unsigned long mask;

  unsigned long flags;

  unsigned char operands[8];
};

extern const struct powerpc_opcode powerpc_opcodes[];
extern const int powerpc_num_opcodes;


#define PPC_OPCODE_PPC			 1

#define PPC_OPCODE_POWER		 2

#define PPC_OPCODE_POWER2		 4

#define PPC_OPCODE_32			 8

#define PPC_OPCODE_64		      0x10

#define PPC_OPCODE_601		      0x20

#define PPC_OPCODE_COMMON	      0x40

#define PPC_OPCODE_ANY		      0x80

#define PPC_OPCODE_64_BRIDGE	     0x100

#define PPC_OPCODE_ALTIVEC	     0x200

#define PPC_OPCODE_403		     0x400

#define PPC_OPCODE_BOOKE	     0x800

#define PPC_OPCODE_BOOKE64	    0x1000

#define PPC_OPCODE_440		    0x2000

#define PPC_OPCODE_POWER4	    0x4000

#define PPC_OPCODE_NOPOWER4	    0x8000

#define PPC_OPCODE_CLASSIC	   0x10000

#define PPC_OPCODE_SPE		   0x20000

#define PPC_OPCODE_ISEL		   0x40000

#define PPC_OPCODE_EFS		   0x80000

#define PPC_OPCODE_BRLOCK	  0x100000

#define PPC_OPCODE_PMR		  0x200000

#define PPC_OPCODE_CACHELCK	  0x400000

#define PPC_OPCODE_RFMCI	  0x800000

#define PPC_OPCODE_POWER5	 0x1000000

#define PPC_OPCODE_E300          0x2000000

#define PPC_OPCODE_POWER6	 0x4000000

#define PPC_OPCODE_CELL		 0x8000000

#define PPC_OP(i) (((i) >> 26) & 0x3f)


struct powerpc_operand
{
  
  int bits;

  
  int shift;

  unsigned long (*insert)
    (unsigned long instruction, long op, int dialect, const char **errmsg);

  long (*extract) (unsigned long instruction, int dialect, int *invalid);

  
  unsigned long flags;
};


extern const struct powerpc_operand powerpc_operands[];


#define PPC_OPERAND_SIGNED (01)

#define PPC_OPERAND_SIGNOPT (02)

#define PPC_OPERAND_FAKE (04)

#define PPC_OPERAND_PARENS (010)

#define PPC_OPERAND_CR (020)

#define PPC_OPERAND_GPR (040)

#define PPC_OPERAND_GPR_0 (0100)

#define PPC_OPERAND_FPR (0200)

#define PPC_OPERAND_RELATIVE (0400)

#define PPC_OPERAND_ABSOLUTE (01000)

#define PPC_OPERAND_OPTIONAL (02000)

#define PPC_OPERAND_NEXT (04000)

#define PPC_OPERAND_NEGATIVE (010000)

#define PPC_OPERAND_VR (020000)

#define PPC_OPERAND_DS (040000)

#define PPC_OPERAND_DQ (0100000)


struct powerpc_macro
{
  
  const char *name;

  
  unsigned int operands;

  unsigned long flags;

  const char *format;
};

extern const struct powerpc_macro powerpc_macros[];
extern const int powerpc_num_macros;

#endif 
