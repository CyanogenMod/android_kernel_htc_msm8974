/*
    comedi/drivers/plx9052.h
    Definitions for the PLX-9052 PCI interface chip

    Copyright (C) 2002 MEV Ltd. <http://www.mev.co.uk/>

    COMEDI - Linux Control and Measurement Device Interface
    Copyright (C) 2000 David A. Schleef <ds@schleef.org>

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

#ifndef _PLX9052_H_
#define _PLX9052_H_

#define PLX9052_INTCSR	0x4C	
#define PLX9052_INTCSR_LI1ENAB_MASK		0x0001
#define PLX9052_INTCSR_LI1ENAB_DISABLED		0x0000
#define PLX9052_INTCSR_LI1ENAB_ENABLED		0x0001
#define PLX9052_INTCSR_LI1POL_MASK		0x0002
#define PLX9052_INTCSR_LI1POL_LOW		0x0000
#define PLX9052_INTCSR_LI1POL_HIGH		0x0002
#define PLX9052_INTCSR_LI1STAT_MASK		0x0004
#define PLX9052_INTCSR_LI1STAT_INACTIVE		0x0000
#define PLX9052_INTCSR_LI1STAT_ACTIVE		0x0004
#define PLX9052_INTCSR_LI2ENAB_MASK		0x0008
#define PLX9052_INTCSR_LI2ENAB_DISABLED		0x0000
#define PLX9052_INTCSR_LI2ENAB_ENABLED		0x0008
#define PLX9052_INTCSR_LI2POL_MASK		0x0010
#define PLX9052_INTCSR_LI2POL_LOW		0x0000
#define PLX9052_INTCSR_LI2POL_HIGH		0x0010
#define PLX9052_INTCSR_LI2STAT_MASK		0x0020
#define PLX9052_INTCSR_LI2STAT_INACTIVE		0x0000
#define PLX9052_INTCSR_LI2STAT_ACTIVE		0x0020
#define PLX9052_INTCSR_PCIENAB_MASK		0x0040
#define PLX9052_INTCSR_PCIENAB_DISABLED		0x0000
#define PLX9052_INTCSR_PCIENAB_ENABLED		0x0040
#define PLX9052_INTCSR_SOFTINT_MASK		0x0080
#define PLX9052_INTCSR_SOFTINT_UNASSERTED	0x0000
#define PLX9052_INTCSR_SOFTINT_ASSERTED		0x0080
#define PLX9052_INTCSR_LI1SEL_MASK		0x0100
#define PLX9052_INTCSR_LI1SEL_LEVEL		0x0000
#define PLX9052_INTCSR_LI1SEL_EDGE		0x0100
#define PLX9052_INTCSR_LI2SEL_MASK		0x0200
#define PLX9052_INTCSR_LI2SEL_LEVEL		0x0000
#define PLX9052_INTCSR_LI2SEL_EDGE		0x0200
#define PLX9052_INTCSR_LI1CLRINT_MASK		0x0400
#define PLX9052_INTCSR_LI1CLRINT_UNASSERTED	0x0000
#define PLX9052_INTCSR_LI1CLRINT_ASSERTED	0x0400
#define PLX9052_INTCSR_LI2CLRINT_MASK		0x0800
#define PLX9052_INTCSR_LI2CLRINT_UNASSERTED	0x0000
#define PLX9052_INTCSR_LI2CLRINT_ASSERTED	0x0800
#define PLX9052_INTCSR_ISAMODE_MASK		0x1000
#define PLX9052_INTCSR_ISAMODE_DISABLED		0x0000
#define PLX9052_INTCSR_ISAMODE_ENABLED		0x1000

#endif 
