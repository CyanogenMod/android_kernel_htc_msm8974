/* io-unit.h: Definitions for the sun4d IO-UNIT.
 *
 * Copyright (C) 1997,1998 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 */
#ifndef _SPARC_IO_UNIT_H
#define _SPARC_IO_UNIT_H

#include <linux/spinlock.h>
#include <asm/page.h>
#include <asm/pgtable.h>

 
 
#define IOUNIT_DMA_BASE	    0xfc000000 
#define IOUNIT_DMA_SIZE	    0x04000000 
#define IOUNIT_DVMA_SIZE    0x00100000 

#define IOUPTE_PAGE          0xffffff00 
#define IOUPTE_CACHE         0x00000080 
#define IOUPTE_STREAM        0x00000040 
#define IOUPTE_INTRA	     0x00000008 
#define IOUPTE_WRITE         0x00000004 
#define IOUPTE_VALID         0x00000002 
#define IOUPTE_PARITY        0x00000001 

struct iounit_struct {
	unsigned long		bmap[(IOUNIT_DMA_SIZE >> (PAGE_SHIFT + 3)) / sizeof(unsigned long)];
	spinlock_t		lock;
	iopte_t			*page_table;
	unsigned long		rotor[3];
	unsigned long		limit[4];
};

#define IOUNIT_BMAP1_START	0x00000000
#define IOUNIT_BMAP1_END	(IOUNIT_DMA_SIZE >> (PAGE_SHIFT + 1))
#define IOUNIT_BMAP2_START	IOUNIT_BMAP1_END
#define IOUNIT_BMAP2_END	IOUNIT_BMAP2_START + (IOUNIT_DMA_SIZE >> (PAGE_SHIFT + 2))
#define IOUNIT_BMAPM_START	IOUNIT_BMAP2_END
#define IOUNIT_BMAPM_END	((IOUNIT_DMA_SIZE - IOUNIT_DVMA_SIZE) >> PAGE_SHIFT)

#endif 
