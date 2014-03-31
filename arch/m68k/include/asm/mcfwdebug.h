
/*
 *	mcfdebug.h -- ColdFire Debug Module support.
 *
 * 	(C) Copyright 2001, Lineo Inc. (www.lineo.com) 
 */

#ifndef mcfdebug_h
#define mcfdebug_h

#define MCFDEBUG_CSR	0x0			
#define MCFDEBUG_BAAR	0x5			
#define MCFDEBUG_AATR	0x6			
#define MCFDEBUG_TDR	0x7			
#define MCFDEBUG_PBR	0x8			
#define MCFDEBUG_PBMR	0x9			
#define MCFDEBUG_ABHR	0xc			
#define MCFDEBUG_ABLR	0xd			
#define MCFDEBUG_DBR	0xe			
#define MCFDEBUG_DBMR	0xf			

#define MCFDEBUG_TDR_TRC_DISP	0x00000000	
#define MCFDEBUG_TDR_TRC_HALT	0x40000000	
#define MCFDEBUG_TDR_TRC_INTR	0x80000000	
#define MCFDEBUG_TDR_LXT1	0x00004000	
#define MCFDEBUG_TDR_LXT2	0x00008000	
#define MCFDEBUG_TDR_EBL1	0x00002000	
#define MCFDEBUG_TDR_EBL2	0x20000000	
#define MCFDEBUG_TDR_EDLW1	0x00001000	
#define MCFDEBUG_TDR_EDLW2	0x10000000
#define MCFDEBUG_TDR_EDWL1	0x00000800	
#define MCFDEBUG_TDR_EDWL2	0x08000000
#define MCFDEBUG_TDR_EDWU1	0x00000400	
#define MCFDEBUG_TDR_EDWU2	0x04000000
#define MCFDEBUG_TDR_EDLL1	0x00000200	
#define MCFDEBUG_TDR_EDLL2	0x02000000
#define MCFDEBUG_TDR_EDLM1	0x00000100	
#define MCFDEBUG_TDR_EDLM2	0x01000000
#define MCFDEBUG_TDR_EDUM1	0x00000080	
#define MCFDEBUG_TDR_EDUM2	0x00800000
#define MCFDEBUG_TDR_EDUU1	0x00000040	
#define MCFDEBUG_TDR_EDUU2	0x00400000
#define MCFDEBUG_TDR_DI1	0x00000020	
#define MCFDEBUG_TDR_DI2	0x00200000
#define MCFDEBUG_TDR_EAI1	0x00000010	
#define MCFDEBUG_TDR_EAI2	0x00100000
#define MCFDEBUG_TDR_EAR1	0x00000008	
#define MCFDEBUG_TDR_EAR2	0x00080000
#define MCFDEBUG_TDR_EAL1	0x00000004	
#define MCFDEBUG_TDR_EAL2	0x00040000
#define MCFDEBUG_TDR_EPC1	0x00000002	
#define MCFDEBUG_TDR_EPC2	0x00020000
#define MCFDEBUG_TDR_PCI1	0x00000001	
#define MCFDEBUG_TDR_PCI2	0x00010000

#define MCFDEBUG_AAR_RESET	0x00000005

#define MCFDEBUG_CSR_RESET	0x00100000
#define MCFDEBUG_CSR_PSTCLK	0x00020000	
#define MCFDEBUG_CSR_IPW	0x00010000	
#define MCFDEBUG_CSR_MAP	0x00008000	
#define MCFDEBUG_CSR_TRC	0x00004000	
#define MCFDEBUG_CSR_EMU	0x00002000	
#define MCFDEBUG_CSR_DDC_READ	0x00000800	
#define MCFDEBUG_CSR_DDC_WRITE	0x00001000
#define MCFDEBUG_CSR_UHE	0x00000400	
#define MCFDEBUG_CSR_BTB0	0x00000000	
#define MCFDEBUG_CSR_BTB2	0x00000100	
#define MCFDEBUG_CSR_BTB3	0x00000200	
#define MCFDEBUG_CSR_BTB4	0x00000300	
#define MCFDEBUG_CSR_NPL	0x00000040	
#define MCFDEBUG_CSR_SSM	0x00000010	

#define MCFDEBUG_BAAR_RESET	0x00000005


static inline void wdebug(int reg, unsigned long data) {
	unsigned short dbg_spc[6];
	unsigned short *dbg;

	
	dbg = (unsigned short *)((((unsigned long)dbg_spc) + 3) & 0xfffffffc);

	
	dbg[0] = 0x2c80 | (reg & 0xf);
	dbg[1] = (data >> 16) & 0xffff;
	dbg[2] = data & 0xffff;
	dbg[3] = 0;

	
#if 0
	
	asm(	"move.l	%0, %%a0\n\t"
		".word	0xfbd0\n\t"
		".word	0x0003\n\t"
	    :: "g" (dbg) : "a0");
#else
	
	asm(	"wdebug	(%0)" :: "a" (dbg));
#endif
}

#endif
