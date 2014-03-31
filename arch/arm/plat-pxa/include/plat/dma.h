#ifndef __PLAT_DMA_H
#define __PLAT_DMA_H

#define DMAC_REG(x)	(*((volatile u32 *)(DMAC_REGS_VIRT + (x))))

#define DCSR(n)		DMAC_REG((n) << 2)
#define DALGN		DMAC_REG(0x00a0)  
#define DINT		DMAC_REG(0x00f0)  
#define DDADR(n)	DMAC_REG(0x0200 + ((n) << 4))
#define DSADR(n)	DMAC_REG(0x0204 + ((n) << 4))
#define DTADR(n)	DMAC_REG(0x0208 + ((n) << 4))
#define DCMD(n)		DMAC_REG(0x020c + ((n) << 4))
#define DRCMR(n)	DMAC_REG((((n) < 64) ? 0x0100 : 0x1100) + \
				 (((n) & 0x3f) << 2))

#define DCSR_RUN	(1 << 31)	
#define DCSR_NODESC	(1 << 30)	
#define DCSR_STOPIRQEN	(1 << 29)	
#define DCSR_REQPEND	(1 << 8)	
#define DCSR_STOPSTATE	(1 << 3)	
#define DCSR_ENDINTR	(1 << 2)	
#define DCSR_STARTINTR	(1 << 1)	
#define DCSR_BUSERR	(1 << 0)	

#define DCSR_EORIRQEN	(1 << 28)       
#define DCSR_EORJMPEN	(1 << 27)       
#define DCSR_EORSTOPEN	(1 << 26)       
#define DCSR_SETCMPST	(1 << 25)       
#define DCSR_CLRCMPST	(1 << 24)       
#define DCSR_CMPST	(1 << 10)       
#define DCSR_EORINTR	(1 << 9)        

#define DRCMR_MAPVLD	(1 << 7)	
#define DRCMR_CHLNUM	0x1f		

#define DDADR_DESCADDR	0xfffffff0	
#define DDADR_STOP	(1 << 0)	

#define DCMD_INCSRCADDR	(1 << 31)	
#define DCMD_INCTRGADDR	(1 << 30)	
#define DCMD_FLOWSRC	(1 << 29)	
#define DCMD_FLOWTRG	(1 << 28)	
#define DCMD_STARTIRQEN	(1 << 22)	
#define DCMD_ENDIRQEN	(1 << 21)	
#define DCMD_ENDIAN	(1 << 18)	
#define DCMD_BURST8	(1 << 16)	
#define DCMD_BURST16	(2 << 16)	
#define DCMD_BURST32	(3 << 16)	
#define DCMD_WIDTH1	(1 << 14)	
#define DCMD_WIDTH2	(2 << 14)	
#define DCMD_WIDTH4	(3 << 14)	
#define DCMD_LENGTH	0x01fff		


typedef struct pxa_dma_desc {
	volatile u32 ddadr;	
	volatile u32 dsadr;	
	volatile u32 dtadr;	
	volatile u32 dcmd;	
} pxa_dma_desc;

typedef enum {
	DMA_PRIO_HIGH = 0,
	DMA_PRIO_MEDIUM = 1,
	DMA_PRIO_LOW = 2
} pxa_dma_prio;


int __init pxa_init_dma(int irq, int num_ch);

int pxa_request_dma (char *name,
			 pxa_dma_prio prio,
			 void (*irq_handler)(int, void *),
			 void *data);

void pxa_free_dma (int dma_ch);

#endif 
