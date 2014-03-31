#ifndef _ASM_POWERPC_DMA_H
#define _ASM_POWERPC_DMA_H
#ifdef __KERNEL__

/*
 * Defines for using and allocating dma channels.
 * Written by Hennus Bergman, 1992.
 * High DMA channel support & info by Hannu Savolainen
 * and John Boyd, Nov. 1992.
 * Changes for ppc sound by Christoph Nadig
 */


#include <asm/io.h>
#include <linux/spinlock.h>

#ifndef MAX_DMA_CHANNELS
#define MAX_DMA_CHANNELS	8
#endif

#define MAX_DMA_ADDRESS		(~0UL)

#ifdef HAVE_REALLY_SLOW_DMA_CONTROLLER
#define dma_outb	outb_p
#else
#define dma_outb	outb
#endif

#define dma_inb		inb


#define IO_DMA1_BASE	0x00	
#define IO_DMA2_BASE	0xC0	

#define DMA1_CMD_REG		0x08	
#define DMA1_STAT_REG		0x08	
#define DMA1_REQ_REG		0x09	
#define DMA1_MASK_REG		0x0A	
#define DMA1_MODE_REG		0x0B	
#define DMA1_CLEAR_FF_REG	0x0C	
#define DMA1_TEMP_REG		0x0D	
#define DMA1_RESET_REG		0x0D	
#define DMA1_CLR_MASK_REG	0x0E	
#define DMA1_MASK_ALL_REG	0x0F	

#define DMA2_CMD_REG		0xD0	
#define DMA2_STAT_REG		0xD0	
#define DMA2_REQ_REG		0xD2	
#define DMA2_MASK_REG		0xD4	
#define DMA2_MODE_REG		0xD6	
#define DMA2_CLEAR_FF_REG	0xD8	
#define DMA2_TEMP_REG		0xDA	
#define DMA2_RESET_REG		0xDA	
#define DMA2_CLR_MASK_REG	0xDC	
#define DMA2_MASK_ALL_REG	0xDE	

#define DMA_ADDR_0		0x00	
#define DMA_ADDR_1		0x02
#define DMA_ADDR_2		0x04
#define DMA_ADDR_3		0x06
#define DMA_ADDR_4		0xC0
#define DMA_ADDR_5		0xC4
#define DMA_ADDR_6		0xC8
#define DMA_ADDR_7		0xCC

#define DMA_CNT_0		0x01	
#define DMA_CNT_1		0x03
#define DMA_CNT_2		0x05
#define DMA_CNT_3		0x07
#define DMA_CNT_4		0xC2
#define DMA_CNT_5		0xC6
#define DMA_CNT_6		0xCA
#define DMA_CNT_7		0xCE

#define DMA_LO_PAGE_0		0x87	
#define DMA_LO_PAGE_1		0x83
#define DMA_LO_PAGE_2		0x81
#define DMA_LO_PAGE_3		0x82
#define DMA_LO_PAGE_5		0x8B
#define DMA_LO_PAGE_6		0x89
#define DMA_LO_PAGE_7		0x8A

#define DMA_HI_PAGE_0		0x487	
#define DMA_HI_PAGE_1		0x483
#define DMA_HI_PAGE_2		0x481
#define DMA_HI_PAGE_3		0x482
#define DMA_HI_PAGE_5		0x48B
#define DMA_HI_PAGE_6		0x489
#define DMA_HI_PAGE_7		0x48A

#define DMA1_EXT_REG		0x40B
#define DMA2_EXT_REG		0x4D6

#ifndef __powerpc64__
    
    extern unsigned int DMA_MODE_WRITE;
    extern unsigned int DMA_MODE_READ;
    extern unsigned long ISA_DMA_THRESHOLD;
#else
    #define DMA_MODE_READ	0x44	
    #define DMA_MODE_WRITE	0x48	
#endif

#define DMA_MODE_CASCADE	0xC0	

#define DMA_AUTOINIT		0x10

extern spinlock_t dma_spin_lock;

static __inline__ unsigned long claim_dma_lock(void)
{
	unsigned long flags;
	spin_lock_irqsave(&dma_spin_lock, flags);
	return flags;
}

static __inline__ void release_dma_lock(unsigned long flags)
{
	spin_unlock_irqrestore(&dma_spin_lock, flags);
}

static __inline__ void enable_dma(unsigned int dmanr)
{
	unsigned char ucDmaCmd = 0x00;

	if (dmanr != 4) {
		dma_outb(0, DMA2_MASK_REG);	
		dma_outb(ucDmaCmd, DMA2_CMD_REG);	
	}
	if (dmanr <= 3) {
		dma_outb(dmanr, DMA1_MASK_REG);
		dma_outb(ucDmaCmd, DMA1_CMD_REG);	
	} else {
		dma_outb(dmanr & 3, DMA2_MASK_REG);
	}
}

static __inline__ void disable_dma(unsigned int dmanr)
{
	if (dmanr <= 3)
		dma_outb(dmanr | 4, DMA1_MASK_REG);
	else
		dma_outb((dmanr & 3) | 4, DMA2_MASK_REG);
}

static __inline__ void clear_dma_ff(unsigned int dmanr)
{
	if (dmanr <= 3)
		dma_outb(0, DMA1_CLEAR_FF_REG);
	else
		dma_outb(0, DMA2_CLEAR_FF_REG);
}

static __inline__ void set_dma_mode(unsigned int dmanr, char mode)
{
	if (dmanr <= 3)
		dma_outb(mode | dmanr, DMA1_MODE_REG);
	else
		dma_outb(mode | (dmanr & 3), DMA2_MODE_REG);
}

static __inline__ void set_dma_page(unsigned int dmanr, int pagenr)
{
	switch (dmanr) {
	case 0:
		dma_outb(pagenr, DMA_LO_PAGE_0);
		dma_outb(pagenr >> 8, DMA_HI_PAGE_0);
		break;
	case 1:
		dma_outb(pagenr, DMA_LO_PAGE_1);
		dma_outb(pagenr >> 8, DMA_HI_PAGE_1);
		break;
	case 2:
		dma_outb(pagenr, DMA_LO_PAGE_2);
		dma_outb(pagenr >> 8, DMA_HI_PAGE_2);
		break;
	case 3:
		dma_outb(pagenr, DMA_LO_PAGE_3);
		dma_outb(pagenr >> 8, DMA_HI_PAGE_3);
		break;
	case 5:
		dma_outb(pagenr & 0xfe, DMA_LO_PAGE_5);
		dma_outb(pagenr >> 8, DMA_HI_PAGE_5);
		break;
	case 6:
		dma_outb(pagenr & 0xfe, DMA_LO_PAGE_6);
		dma_outb(pagenr >> 8, DMA_HI_PAGE_6);
		break;
	case 7:
		dma_outb(pagenr & 0xfe, DMA_LO_PAGE_7);
		dma_outb(pagenr >> 8, DMA_HI_PAGE_7);
		break;
	}
}

static __inline__ void set_dma_addr(unsigned int dmanr, unsigned int phys)
{
	if (dmanr <= 3) {
		dma_outb(phys & 0xff,
			 ((dmanr & 3) << 1) + IO_DMA1_BASE);
		dma_outb((phys >> 8) & 0xff,
			 ((dmanr & 3) << 1) + IO_DMA1_BASE);
	} else {
		dma_outb((phys >> 1) & 0xff,
			 ((dmanr & 3) << 2) + IO_DMA2_BASE);
		dma_outb((phys >> 9) & 0xff,
			 ((dmanr & 3) << 2) + IO_DMA2_BASE);
	}
	set_dma_page(dmanr, phys >> 16);
}


static __inline__ void set_dma_count(unsigned int dmanr, unsigned int count)
{
	count--;
	if (dmanr <= 3) {
		dma_outb(count & 0xff,
			 ((dmanr & 3) << 1) + 1 + IO_DMA1_BASE);
		dma_outb((count >> 8) & 0xff,
			 ((dmanr & 3) << 1) + 1 + IO_DMA1_BASE);
	} else {
		dma_outb((count >> 1) & 0xff,
			 ((dmanr & 3) << 2) + 2 + IO_DMA2_BASE);
		dma_outb((count >> 9) & 0xff,
			 ((dmanr & 3) << 2) + 2 + IO_DMA2_BASE);
	}
}


static __inline__ int get_dma_residue(unsigned int dmanr)
{
	unsigned int io_port = (dmanr <= 3)
	    ? ((dmanr & 3) << 1) + 1 + IO_DMA1_BASE
	    : ((dmanr & 3) << 2) + 2 + IO_DMA2_BASE;

	
	unsigned short count;

	count = 1 + dma_inb(io_port);
	count += dma_inb(io_port) << 8;

	return (dmanr <= 3) ? count : (count << 1);
}


extern int request_dma(unsigned int dmanr, const char *device_id);
extern void free_dma(unsigned int dmanr);

#ifdef CONFIG_PCI
extern int isa_dma_bridge_buggy;
#else
#define isa_dma_bridge_buggy	(0)
#endif

#endif 
#endif	
