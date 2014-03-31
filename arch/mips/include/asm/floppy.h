/*
 * Architecture specific parts of the Floppy driver
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995 - 2000 Ralf Baechle
 */
#ifndef _ASM_FLOPPY_H
#define _ASM_FLOPPY_H

#include <linux/dma-mapping.h>

static inline void fd_cacheflush(char * addr, long size)
{
	dma_cache_sync(NULL, addr, size, DMA_BIDIRECTIONAL);
}

#define MAX_BUFFER_SECTORS 24


#define FLOPPY0_TYPE 		fd_drive_type(0)
#define FLOPPY1_TYPE		fd_drive_type(1)

#define FDC1			fd_getfdaddr1()

#define N_FDC 1			
#define N_DRIVE 8

#define CROSS_64KB(a, s) ((unsigned long)(a)/K_64 != ((unsigned long)(a) + (s) - 1) / K_64)

#define EXTRA_FLOPPY_PARAMS

#include <floppy.h>

#endif 
