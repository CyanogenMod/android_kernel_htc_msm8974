/*
 * 440SPe's XOR engines support header file
 *
 * 2006-2009 (C) DENX Software Engineering.
 *
 * Author: Yuri Tikhonov <yur@emcraft.com>
 *
 * This file is licensed under the term of  the GNU General Public License
 * version 2. The program licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#ifndef _PPC440SPE_XOR_H
#define _PPC440SPE_XOR_H

#include <linux/types.h>

#define XOR_ENGINES_NUM		1

#define XOR_MAX_OPS		16

#define XOR_CBCR_LNK_BIT        (1<<31) 
#define XOR_CBCR_TGT_BIT        (1<<30) 
#define XOR_CBCR_CBCE_BIT       (1<<29) 
#define XOR_CBCR_RNZE_BIT       (1<<28) 
#define XOR_CBCR_XNOR_BIT       (1<<15) 
#define XOR_CDCR_OAC_MSK        (0x7F)  

#define XOR_SR_XCP_BIT		(1<<31)	
#define XOR_SR_ICB_BIT		(1<<17)	
#define XOR_SR_IC_BIT		(1<<16)	
#define XOR_SR_IPE_BIT		(1<<15)	
#define XOR_SR_RNZ_BIT		(1<<2)	
#define XOR_SR_CBC_BIT		(1<<1)	
#define XOR_SR_CBLC_BIT		(1<<0)	

#define XOR_CRSR_XASR_BIT	(1<<31)	
#define XOR_CRSR_XAE_BIT	(1<<30)	
#define XOR_CRSR_RCBE_BIT	(1<<29)	
#define XOR_CRSR_PAUS_BIT	(1<<28)	
#define XOR_CRSR_64BA_BIT	(1<<27) 
#define XOR_CRSR_CLP_BIT	(1<<25)	

#define XOR_IE_ICBIE_BIT	(1<<17)	
#define XOR_IE_ICIE_BIT		(1<<16)	
#define XOR_IE_RPTIE_BIT	(1<<14)	
#define XOR_IE_CBCIE_BIT	(1<<1)	
#define XOR_IE_CBLCI_BIT	(1<<0)	

struct xor_cb {
	u32	cbc;		
	u32	cbbc;		
	u32	cbs;		
	u8	pad0[4];	
	u32	cbtah;		
	u32	cbtal;		
	u32	cblah;		
	u32	cblal;		
	struct {
		u32 h;
		u32 l;
	} __attribute__ ((packed)) ops[16];
} __attribute__ ((packed));

struct xor_regs {
	u32	op_ar[16][2];	
	u8	pad0[352];	
	u32	cbcr;		
	u32	cbbcr;		
	u32	cbsr;		
	u8	pad1[4];	
	u32	cbtahr;		
	u32	cbtalr;		
	u32	cblahr;		
	u32	cblalr;		
	u32	crsr;		
	u32	crrr;		
	u32	ccbahr;		
	u32	ccbalr;		
	u32	plbr;		
	u32	ier;		
	u32	pecr;		
	u32	sr;		
	u32	revidr;		
};

#endif 
