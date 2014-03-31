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

#ifndef __FPSR_H__
#define __FPSR_H__



typedef unsigned int FPSR;	
typedef unsigned int FPCR;	

#define MASK_SYSID		0xff000000
#define BIT_HARDWARE		0x80000000
#define FP_EMULATOR		0x01000000	
#define FP_ACCELERATOR		0x81000000	


#define MASK_TRAP_ENABLE	0x00ff0000
#define MASK_TRAP_ENABLE_STRICT	0x001f0000
#define BIT_IXE		0x00100000	
#define BIT_UFE		0x00080000	
#define BIT_OFE		0x00040000	
#define BIT_DZE		0x00020000	
#define BIT_IOE		0x00010000	


#define MASK_SYSTEM_CONTROL	0x0000ff00
#define MASK_TRAP_STRICT	0x00001f00

#define BIT_AC	0x00001000	
#define BIT_EP	0x00000800	
#define BIT_SO	0x00000400	
#define BIT_NE	0x00000200	
#define BIT_ND	0x00000100	


#define MASK_EXCEPTION_FLAGS		0x000000ff
#define MASK_EXCEPTION_FLAGS_STRICT	0x0000001f

#define BIT_IXC		0x00000010	
#define BIT_UFC		0x00000008	
#define BIT_OFC		0x00000004	
#define BIT_DZC		0x00000002	
#define BIT_IOC		0x00000001	


#define BIT_RU		0x80000000	
#define BIT_IE		0x10000000	
#define BIT_MO		0x08000000	
#define BIT_EO		0x04000000	
#define BIT_SB		0x00000800	
#define BIT_AB		0x00000400	
#define BIT_RE		0x00000200	
#define BIT_DA		0x00000100	

#define MASK_OP		0x00f08010	
#define MASK_PR		0x00080080	
#define MASK_S1		0x00070000	
#define MASK_S2		0x00000007	
#define MASK_DS		0x00007000	
#define MASK_RM		0x00000060	
#define MASK_ALU	0x9cfff2ff	
#define MASK_RESET	0x00000d00	
#define MASK_WFC	MASK_RESET
#define MASK_RFC	~MASK_RESET

#endif
