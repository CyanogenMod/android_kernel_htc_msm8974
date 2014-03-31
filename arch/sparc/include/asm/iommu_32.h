/* iommu.h: Definitions for the sun4m IOMMU.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 */
#ifndef _SPARC_IOMMU_H
#define _SPARC_IOMMU_H

#include <asm/page.h>
#include <asm/bitext.h>


struct iommu_regs {
	
	volatile unsigned long control;    
	volatile unsigned long base;       
	volatile unsigned long _unused1[3];
	volatile unsigned long tlbflush;   
	volatile unsigned long pageflush;  
	volatile unsigned long _unused2[1017];
	
	volatile unsigned long afsr;       
	volatile unsigned long afar;       
	volatile unsigned long _unused3[2];
	volatile unsigned long sbuscfg0;   
	volatile unsigned long sbuscfg1;
	volatile unsigned long sbuscfg2;
	volatile unsigned long sbuscfg3;
	volatile unsigned long mfsr;       
	volatile unsigned long mfar;       
	volatile unsigned long _unused4[1014];
	
	volatile unsigned long mid;        
};

#define IOMMU_CTRL_IMPL     0xf0000000 
#define IOMMU_CTRL_VERS     0x0f000000 
#define IOMMU_CTRL_RNGE     0x0000001c 
#define IOMMU_RNGE_16MB     0x00000000 
#define IOMMU_RNGE_32MB     0x00000004 
#define IOMMU_RNGE_64MB     0x00000008 
#define IOMMU_RNGE_128MB    0x0000000c 
#define IOMMU_RNGE_256MB    0x00000010 
#define IOMMU_RNGE_512MB    0x00000014 
#define IOMMU_RNGE_1GB      0x00000018 
#define IOMMU_RNGE_2GB      0x0000001c 
#define IOMMU_CTRL_ENAB     0x00000001 

#define IOMMU_AFSR_ERR      0x80000000 
#define IOMMU_AFSR_LE       0x40000000 
#define IOMMU_AFSR_TO       0x20000000 
#define IOMMU_AFSR_BE       0x10000000 
#define IOMMU_AFSR_SIZE     0x0e000000 
#define IOMMU_AFSR_S        0x01000000 
#define IOMMU_AFSR_RESV     0x00f00000 
#define IOMMU_AFSR_ME       0x00080000 
#define IOMMU_AFSR_RD       0x00040000 
#define IOMMU_AFSR_FAV      0x00020000 

#define IOMMU_SBCFG_SAB30   0x00010000 
#define IOMMU_SBCFG_BA16    0x00000004 
#define IOMMU_SBCFG_BA8     0x00000002 
#define IOMMU_SBCFG_BYPASS  0x00000001 

#define IOMMU_MFSR_ERR      0x80000000 
#define IOMMU_MFSR_S        0x01000000 
#define IOMMU_MFSR_CPU      0x00800000 
#define IOMMU_MFSR_ME       0x00080000 
#define IOMMU_MFSR_PERR     0x00006000 
#define IOMMU_MFSR_BM       0x00001000 
#define IOMMU_MFSR_C        0x00000800 
#define IOMMU_MFSR_RTYP     0x000000f0 

#define IOMMU_MID_SBAE      0x001f0000 
#define IOMMU_MID_SE        0x00100000 
#define IOMMU_MID_SB3       0x00080000 
#define IOMMU_MID_SB2       0x00040000 
#define IOMMU_MID_SB1       0x00020000 
#define IOMMU_MID_SB0       0x00010000 
#define IOMMU_MID_MID       0x0000000f 

#define IOPTE_PAGE          0x07ffff00 
#define IOPTE_CACHE         0x00000080 
#define IOPTE_WRITE         0x00000004 
#define IOPTE_VALID         0x00000002 
#define IOPTE_WAZ           0x00000001 

struct iommu_struct {
	struct iommu_regs *regs;
	iopte_t *page_table;
	
	unsigned long start; 
	unsigned long end;   

	struct bit_map usemap;
};

static inline void iommu_invalidate(struct iommu_regs *regs)
{
	regs->tlbflush = 0;
}

static inline void iommu_invalidate_page(struct iommu_regs *regs, unsigned long ba)
{
	regs->pageflush = (ba & PAGE_MASK);
}

#endif 
