/* NAND flash interface register definitions
 *
 * Copyright (C) 2008-2009 Panasonic Corporation
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef	_PROC_NAND_REGS_H_
#define	_PROC_NAND_REGS_H_

#define FCOMMAND_0		__SYSREG(0xd8f00000, u8) 
#define FCOMMAND_1		__SYSREG(0xd8f00001, u8) 
#define FCOMMAND_2		__SYSREG(0xd8f00002, u8) 
#define FCOMMAND_3		__SYSREG(0xd8f00003, u8) 

#define FCOMMAND2_0		__SYSREG(0xd8f00110, u8) 
#define FCOMMAND2_1		__SYSREG(0xd8f00111, u8) 
#define FCOMMAND2_2		__SYSREG(0xd8f00112, u8) 
#define FCOMMAND2_3		__SYSREG(0xd8f00113, u8) 

#define FCOMMAND_FIEN		0x80		
#define FCOMMAND_BW_8BIT	0x00		
#define FCOMMAND_BW_16BIT	0x40		
#define FCOMMAND_BLOCKSZ_SMALL	0x00		
#define FCOMMAND_BLOCKSZ_LARGE	0x20		
#define FCOMMAND_DMASTART	0x10		
#define FCOMMAND_RYBY		0x08		
#define FCOMMAND_RYBYINTMSK	0x04		
#define FCOMMAND_XFWP		0x02		
#define FCOMMAND_XFCE		0x01		
#define FCOMMAND_SEQKILL	0x10		
#define FCOMMAND_ANUM		0x07		
#define FCOMMAND_ANUM_NONE	0x00		
#define FCOMMAND_ANUM_1CYC	0x01		
#define FCOMMAND_ANUM_2CYC	0x02		
#define FCOMMAND_ANUM_3CYC	0x03		
#define FCOMMAND_ANUM_4CYC	0x04		
#define FCOMMAND_ANUM_5CYC	0x05		
#define FCOMMAND_FCMD_READ0	0x00		
#define FCOMMAND_FCMD_SEQIN	0x80		
#define FCOMMAND_FCMD_PAGEPROG	0x10		
#define FCOMMAND_FCMD_RESET	0xff		
#define FCOMMAND_FCMD_ERASE1	0x60		
#define FCOMMAND_FCMD_ERASE2	0xd0		
#define FCOMMAND_FCMD_STATUS	0x70		
#define FCOMMAND_FCMD_READID	0x90		
#define FCOMMAND_FCMD_READOOB	0x50		
#define FADD			__SYSREG(0xd8f00004, u32)
#define FADD2			__SYSREG(0xd8f00008, u32)
#define FJUDGE			__SYSREG(0xd8f0000c, u32)
#define FJUDGE_NOERR		0x0		
#define FJUDGE_1BITERR		0x1		
#define FJUDGE_PARITYERR	0x2		
#define FJUDGE_UNCORRECTABLE	0x3		
#define FJUDGE_ERRJDG_MSK	0x3		
#define FECC11			__SYSREG(0xd8f00010, u32)
#define FECC12			__SYSREG(0xd8f00014, u32)
#define FECC21			__SYSREG(0xd8f00018, u32)
#define FECC22			__SYSREG(0xd8f0001c, u32)
#define FECC31			__SYSREG(0xd8f00020, u32)
#define FECC32			__SYSREG(0xd8f00024, u32)
#define FECC41			__SYSREG(0xd8f00028, u32)
#define FECC42			__SYSREG(0xd8f0002c, u32)
#define FDATA			__SYSREG(0xd8f00030, u32)
#define FPWS			__SYSREG(0xd8f00100, u32)
#define FPWS_PWS1W_2CLK		0x00000000 
#define FPWS_PWS1W_3CLK		0x01000000 
#define FPWS_PWS1W_4CLK		0x02000000 
#define FPWS_PWS1W_5CLK		0x03000000 
#define FPWS_PWS1W_6CLK		0x04000000 
#define FPWS_PWS1W_7CLK		0x05000000 
#define FPWS_PWS1W_8CLK		0x06000000 
#define FPWS_PWS1R_3CLK		0x00010000 
#define FPWS_PWS1R_4CLK		0x00020000 
#define FPWS_PWS1R_5CLK		0x00030000 
#define FPWS_PWS1R_6CLK		0x00040000 
#define FPWS_PWS1R_7CLK		0x00050000 
#define FPWS_PWS1R_8CLK		0x00060000 
#define FPWS_PWS2W_2CLK		0x00000100 
#define FPWS_PWS2W_3CLK		0x00000200 
#define FPWS_PWS2W_4CLK		0x00000300 
#define FPWS_PWS2W_5CLK		0x00000400 
#define FPWS_PWS2W_6CLK		0x00000500 
#define FPWS_PWS2R_2CLK		0x00000001 
#define FPWS_PWS2R_3CLK		0x00000002 
#define FPWS_PWS2R_4CLK		0x00000003 
#define FPWS_PWS2R_5CLK		0x00000004 
#define FPWS_PWS2R_6CLK		0x00000005 
#define FCOMMAND2		__SYSREG(0xd8f00110, u32)
#define FNUM			__SYSREG(0xd8f00114, u32)
#define FSDATA_ADDR		0xd8f00400
#define FSDATA			__SYSREG(FSDATA_ADDR, u32)

#endif 
