/*
 * include/asm-m68k/dma.h
 *
 * Copyright 1995 (C) David S. Miller (davem@caip.rutgers.edu)
 *
 * Hacked to fit Sun3x needs by Thomas Bogendoerfer
 */

#ifndef __M68K_DVMA_H
#define __M68K_DVMA_H


#define DVMA_PAGE_SHIFT	13
#define DVMA_PAGE_SIZE	(1UL << DVMA_PAGE_SHIFT)
#define DVMA_PAGE_MASK	(~(DVMA_PAGE_SIZE-1))
#define DVMA_PAGE_ALIGN(addr)	ALIGN(addr, DVMA_PAGE_SIZE)

extern void dvma_init(void);
extern int dvma_map_iommu(unsigned long kaddr, unsigned long baddr,
			  int len);

#define dvma_malloc(x) dvma_malloc_align(x, 0)
#define dvma_map(x, y) dvma_map_align(x, y, 0)
#define dvma_map_vme(x, y) (dvma_map(x, y) & 0xfffff)
#define dvma_map_align_vme(x, y, z) (dvma_map_align (x, y, z) & 0xfffff)
extern unsigned long dvma_map_align(unsigned long kaddr, int len,
			    int align);
extern void *dvma_malloc_align(unsigned long len, unsigned long align);

extern void dvma_unmap(void *baddr);
extern void dvma_free(void *vaddr);


#ifdef CONFIG_SUN3

#define DVMA_PMEG_START 10
#define DVMA_PMEG_END 16
#define DVMA_START 0xf00000
#define DVMA_END 0xfe0000
#define DVMA_SIZE (DVMA_END-DVMA_START)
#define IOMMU_TOTAL_ENTRIES 128
#define IOMMU_ENTRIES 120

#define DVMA_REGION_SIZE 0x10000
#define DVMA_ALIGN(addr) (((addr)+DVMA_REGION_SIZE-1) & \
                         ~(DVMA_REGION_SIZE-1))

#define dvma_vtop(x) ((unsigned long)(x) & 0xffffff)
#define dvma_ptov(x) ((unsigned long)(x) | 0xf000000)
#define dvma_vtovme(x) ((unsigned long)(x) & 0x00fffff)
#define dvma_vmetov(x) ((unsigned long)(x) | 0xff00000)
#define dvma_vtob(x) dvma_vtop(x)
#define dvma_btov(x) dvma_ptov(x)

static inline int dvma_map_cpu(unsigned long kaddr, unsigned long vaddr,
			       int len)
{
	return 0;
}

#else 


#define DVMA_START 0x0
#define DVMA_END 0xf00000
#define DVMA_SIZE (DVMA_END-DVMA_START)
#define IOMMU_TOTAL_ENTRIES	   2048
#define IOMMU_ENTRIES              (IOMMU_TOTAL_ENTRIES - 0x80)

#define dvma_vtob(x) ((unsigned long)(x) & 0x00ffffff)
#define dvma_btov(x) ((unsigned long)(x) | 0xff000000)

extern int dvma_map_cpu(unsigned long kaddr, unsigned long vaddr, int len);




struct sparc_dma_registers {
  __volatile__ unsigned long cond_reg;	
  __volatile__ unsigned long st_addr;	
  __volatile__ unsigned long  cnt;	
  __volatile__ unsigned long dma_test;	
};

enum dvma_rev {
	dvmarev0,
	dvmaesc1,
	dvmarev1,
	dvmarev2,
	dvmarev3,
	dvmarevplus,
	dvmahme
};

#define DMA_HASCOUNT(rev)  ((rev)==dvmaesc1)

struct Linux_SBus_DMA {
	struct Linux_SBus_DMA *next;
	struct linux_sbus_device *SBus_dev;
	struct sparc_dma_registers *regs;

	
	int node;                
	int running;             
	int allocated;           

	
	unsigned long addr;      
	int nbytes;              
	int realbytes;           

	
	enum dvma_rev revision;
};

extern struct Linux_SBus_DMA *dma_chain;

#define DMA_ISBROKEN(dma)    ((dma)->revision == dvmarev1)
#define DMA_ISESC1(dma)      ((dma)->revision == dvmaesc1)

#define DMA_DEVICE_ID    0xf0000000        
#define DMA_VERS0        0x00000000        
#define DMA_ESCV1        0x40000000        
#define DMA_VERS1        0x80000000        
#define DMA_VERS2        0xa0000000        
#define DMA_VERHME       0xb0000000        
#define DMA_VERSPLUS     0x90000000        

#define DMA_HNDL_INTR    0x00000001        
#define DMA_HNDL_ERROR   0x00000002        
#define DMA_FIFO_ISDRAIN 0x0000000c        
#define DMA_INT_ENAB     0x00000010        
#define DMA_FIFO_INV     0x00000020        
#define DMA_ACC_SZ_ERR   0x00000040        
#define DMA_FIFO_STDRAIN 0x00000040        
#define DMA_RST_SCSI     0x00000080        
#define DMA_RST_ENET     DMA_RST_SCSI      
#define DMA_ST_WRITE     0x00000100        
#define DMA_ENABLE       0x00000200        
#define DMA_PEND_READ    0x00000400        
#define DMA_ESC_BURST    0x00000800        
#define DMA_READ_AHEAD   0x00001800        
#define DMA_DSBL_RD_DRN  0x00001000        
#define DMA_BCNT_ENAB    0x00002000        
#define DMA_TERM_CNTR    0x00004000        
#define DMA_CSR_DISAB    0x00010000        
#define DMA_SCSI_DISAB   0x00020000        
#define DMA_DSBL_WR_INV  0x00020000        
#define DMA_ADD_ENABLE   0x00040000        
#define DMA_E_BURST8	 0x00040000	   
#define DMA_BRST_SZ      0x000c0000        
#define DMA_BRST64       0x00080000        
#define DMA_BRST32       0x00040000        
#define DMA_BRST16       0x00000000        
#define DMA_BRST0        0x00080000        
#define DMA_ADDR_DISAB   0x00100000        
#define DMA_2CLKS        0x00200000        
#define DMA_3CLKS        0x00400000        
#define DMA_EN_ENETAUI   DMA_3CLKS         
#define DMA_CNTR_DISAB   0x00800000        
#define DMA_AUTO_NADDR   0x01000000        
#define DMA_SCSI_ON      0x02000000        
#define DMA_PARITY_OFF   0x02000000        
#define DMA_LOADED_ADDR  0x04000000        
#define DMA_LOADED_NADDR 0x08000000        

#define DMA_BURST1       0x01
#define DMA_BURST2       0x02
#define DMA_BURST4       0x04
#define DMA_BURST8       0x08
#define DMA_BURST16      0x10
#define DMA_BURST32      0x20
#define DMA_BURST64      0x40
#define DMA_BURSTBITS    0x7f

#define DMA_MAXEND(addr) (0x01000000UL-(((unsigned long)(addr))&0x00ffffffUL))

#define DMA_ERROR_P(regs)  ((((regs)->cond_reg) & DMA_HNDL_ERROR))
#define DMA_IRQ_P(regs)    ((((regs)->cond_reg) & (DMA_HNDL_INTR | DMA_HNDL_ERROR)))
#define DMA_WRITE_P(regs)  ((((regs)->cond_reg) & DMA_ST_WRITE))
#define DMA_OFF(regs)      ((((regs)->cond_reg) &= (~DMA_ENABLE)))
#define DMA_INTSOFF(regs)  ((((regs)->cond_reg) &= (~DMA_INT_ENAB)))
#define DMA_INTSON(regs)   ((((regs)->cond_reg) |= (DMA_INT_ENAB)))
#define DMA_PUNTFIFO(regs) ((((regs)->cond_reg) |= DMA_FIFO_INV))
#define DMA_SETSTART(regs, addr)  ((((regs)->st_addr) = (char *) addr))
#define DMA_BEGINDMA_W(regs) \
        ((((regs)->cond_reg |= (DMA_ST_WRITE|DMA_ENABLE|DMA_INT_ENAB))))
#define DMA_BEGINDMA_R(regs) \
        ((((regs)->cond_reg |= ((DMA_ENABLE|DMA_INT_ENAB)&(~DMA_ST_WRITE)))))

#define DMA_IRQ_ENTRY(dma, dregs) do { \
        if(DMA_ISBROKEN(dma)) DMA_INTSOFF(dregs); \
   } while (0)

#define DMA_IRQ_EXIT(dma, dregs) do { \
	if(DMA_ISBROKEN(dma)) DMA_INTSON(dregs); \
   } while(0)

#define DMA_RESET(dma) do { \
	struct sparc_dma_registers *regs = dma->regs;                      \
	                            \
	sparc_dma_pause(regs, (DMA_FIFO_ISDRAIN));                         \
	                                              \
	regs->cond_reg |= (DMA_RST_SCSI);                      \
	__delay(400);                             \
	regs->cond_reg &= ~(DMA_RST_SCSI);                  \
	sparc_dma_enable_interrupts(regs);       \
	                           \
	if(dma->revision>dvmarev1) regs->cond_reg |= DMA_3CLKS;            \
	dma->running = 0;                                                  \
} while(0)


#endif 

#endif 
