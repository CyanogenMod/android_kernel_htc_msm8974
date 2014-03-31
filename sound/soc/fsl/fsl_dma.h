/*
 * mpc8610-pcm.h - ALSA PCM interface for the Freescale MPC8610 SoC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MPC8610_PCM_H
#define _MPC8610_PCM_H

struct ccsr_dma {
	u8 res0[0x100];
	struct ccsr_dma_channel {
		__be32 mr;      
		__be32 sr;      
		__be32 eclndar; 
		__be32 clndar;  
		__be32 satr;    
		__be32 sar;     
		__be32 datr;    
		__be32 dar;     
		__be32 bcr;     
		__be32 enlndar; 
		__be32 nlndar;  
		u8 res1[4];
		__be32 eclsdar; 
		__be32 clsdar;  
		__be32 enlsdar; 
		__be32 nlsdar;  
		__be32 ssr;     
		__be32 dsr;     
		u8 res2[0x38];
	} channel[4];
	__be32 dgsr;
};

#define CCSR_DMA_MR_BWC_DISABLED	0x0F000000
#define CCSR_DMA_MR_BWC_SHIFT   	24
#define CCSR_DMA_MR_BWC_MASK    	0x0F000000
#define CCSR_DMA_MR_BWC(x) \
	((ilog2(x) << CCSR_DMA_MR_BWC_SHIFT) & CCSR_DMA_MR_BWC_MASK)
#define CCSR_DMA_MR_EMP_EN      	0x00200000
#define CCSR_DMA_MR_EMS_EN      	0x00040000
#define CCSR_DMA_MR_DAHTS_MASK  	0x00030000
#define CCSR_DMA_MR_DAHTS_1     	0x00000000
#define CCSR_DMA_MR_DAHTS_2     	0x00010000
#define CCSR_DMA_MR_DAHTS_4     	0x00020000
#define CCSR_DMA_MR_DAHTS_8     	0x00030000
#define CCSR_DMA_MR_SAHTS_MASK  	0x0000C000
#define CCSR_DMA_MR_SAHTS_1     	0x00000000
#define CCSR_DMA_MR_SAHTS_2     	0x00004000
#define CCSR_DMA_MR_SAHTS_4     	0x00008000
#define CCSR_DMA_MR_SAHTS_8     	0x0000C000
#define CCSR_DMA_MR_DAHE		0x00002000
#define CCSR_DMA_MR_SAHE		0x00001000
#define CCSR_DMA_MR_SRW 		0x00000400
#define CCSR_DMA_MR_EOSIE       	0x00000200
#define CCSR_DMA_MR_EOLNIE      	0x00000100
#define CCSR_DMA_MR_EOLSIE      	0x00000080
#define CCSR_DMA_MR_EIE 		0x00000040
#define CCSR_DMA_MR_XFE 		0x00000020
#define CCSR_DMA_MR_CDSM_SWSM   	0x00000010
#define CCSR_DMA_MR_CA  		0x00000008
#define CCSR_DMA_MR_CTM 		0x00000004
#define CCSR_DMA_MR_CC  		0x00000002
#define CCSR_DMA_MR_CS  		0x00000001

#define CCSR_DMA_SR_TE  		0x00000080
#define CCSR_DMA_SR_CH  		0x00000020
#define CCSR_DMA_SR_PE  		0x00000010
#define CCSR_DMA_SR_EOLNI       	0x00000008
#define CCSR_DMA_SR_CB  		0x00000004
#define CCSR_DMA_SR_EOSI		0x00000002
#define CCSR_DMA_SR_EOLSI       	0x00000001

static inline u32 CCSR_DMA_ECLNDAR_ADDR(u64 x)
{
	return (x >> 32) & 0xf;
}

#define CCSR_DMA_CLNDAR_ADDR(x) ((x) & 0xFFFFFFFE)
#define CCSR_DMA_CLNDAR_EOSIE   	0x00000008

#define CCSR_DMA_ATR_PBATMU     	0x20000000
#define CCSR_DMA_ATR_TFLOWLVL_0 	0x00000000
#define CCSR_DMA_ATR_TFLOWLVL_1 	0x06000000
#define CCSR_DMA_ATR_TFLOWLVL_2 	0x08000000
#define CCSR_DMA_ATR_TFLOWLVL_3 	0x0C000000
#define CCSR_DMA_ATR_PCIORDER   	0x02000000
#define CCSR_DMA_ATR_SME		0x01000000
#define CCSR_DMA_ATR_NOSNOOP    	0x00040000
#define CCSR_DMA_ATR_SNOOP      	0x00050000
#define CCSR_DMA_ATR_ESAD_MASK  	0x0000000F

struct fsl_dma_list_descriptor {
	__be64 next;    	
	__be64 first_link;      
	__be32 source;  	
	__be32 dest;    	
	u8 res[8];      	
} __attribute__ ((aligned(32), packed));

struct fsl_dma_link_descriptor {
	__be32 source_attr;     
	__be32 source_addr;     
	__be32 dest_attr;       
	__be32 dest_addr;       
	__be64 next;    
	__be32 count;   
	u8 res[4];      
} __attribute__ ((aligned(32), packed));

#endif
