#ifndef _ASM_SPARC_DMA_H
#define _ASM_SPARC_DMA_H

#define MAX_DMA_CHANNELS 8
#define DMA_MODE_READ    1
#define DMA_MODE_WRITE   2
#define MAX_DMA_ADDRESS  (~0UL)

#define SIZE_16MB      (16*1024*1024)
#define SIZE_64K       (64*1024)

#define DMA_CSR		0x00UL		
#define DMA_ADDR	0x04UL		
#define DMA_COUNT	0x08UL		
#define DMA_TEST	0x0cUL		

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
#define DMA_SCSI_SBUS64  0x00008000        
#define DMA_CSR_DISAB    0x00010000        
#define DMA_SCSI_DISAB   0x00020000        
#define DMA_DSBL_WR_INV  0x00020000        
#define DMA_ADD_ENABLE   0x00040000        
#define DMA_E_BURSTS	 0x000c0000	   
#define DMA_E_BURST32	 0x00040000	   
#define DMA_E_BURST16	 0x00000000	   
#define DMA_BRST_SZ      0x000c0000        
#define DMA_BRST64       0x000c0000        
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
#define DMA_RESET_FAS366 0x08000000        

#define DMA_BURST1       0x01
#define DMA_BURST2       0x02
#define DMA_BURST4       0x04
#define DMA_BURST8       0x08
#define DMA_BURST16      0x10
#define DMA_BURST32      0x20
#define DMA_BURST64      0x40
#define DMA_BURSTBITS    0x7f


#ifdef CONFIG_PCI
extern int isa_dma_bridge_buggy;
#else
#define isa_dma_bridge_buggy 	(0)
#endif

#ifdef CONFIG_SPARC32

BTFIXUPDEF_CALL(char *, mmu_lockarea, char *, unsigned long)
BTFIXUPDEF_CALL(void,   mmu_unlockarea, char *, unsigned long)

#define mmu_lockarea(vaddr,len) BTFIXUP_CALL(mmu_lockarea)(vaddr,len)
#define mmu_unlockarea(vaddr,len) BTFIXUP_CALL(mmu_unlockarea)(vaddr,len)

struct page;
struct device;
struct scatterlist;

BTFIXUPDEF_CALL(__u32, mmu_get_scsi_one, struct device *, char *, unsigned long)
BTFIXUPDEF_CALL(void,  mmu_get_scsi_sgl, struct device *, struct scatterlist *, int)
BTFIXUPDEF_CALL(void,  mmu_release_scsi_one, struct device *, __u32, unsigned long)
BTFIXUPDEF_CALL(void,  mmu_release_scsi_sgl, struct device *, struct scatterlist *, int)

#define mmu_get_scsi_one(dev,vaddr,len) BTFIXUP_CALL(mmu_get_scsi_one)(dev,vaddr,len)
#define mmu_get_scsi_sgl(dev,sg,sz) BTFIXUP_CALL(mmu_get_scsi_sgl)(dev,sg,sz)
#define mmu_release_scsi_one(dev,vaddr,len) BTFIXUP_CALL(mmu_release_scsi_one)(dev,vaddr,len)
#define mmu_release_scsi_sgl(dev,sg,sz) BTFIXUP_CALL(mmu_release_scsi_sgl)(dev,sg,sz)

BTFIXUPDEF_CALL(int, mmu_map_dma_area, struct device *, dma_addr_t *, unsigned long, unsigned long, int len)
BTFIXUPDEF_CALL(void, mmu_unmap_dma_area, struct device *, unsigned long busa, int len)

#define mmu_map_dma_area(dev,pba,va,a,len) BTFIXUP_CALL(mmu_map_dma_area)(dev,pba,va,a,len)
#define mmu_unmap_dma_area(dev,ba,len) BTFIXUP_CALL(mmu_unmap_dma_area)(dev,ba,len)
#endif

#endif 
