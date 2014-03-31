/*
 * TXx9 SoC AC Link Controller
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __TXX9ACLC_H
#define __TXX9ACLC_H

#include <linux/interrupt.h>
#include <asm/txx9/dmac.h>

#define ACCTLEN			0x00	
#define ACCTLDIS		0x04	
#define   ACCTL_ENLINK		0x00000001	
#define   ACCTL_AUDODMA		0x00000100	
#define   ACCTL_AUDIDMA		0x00001000	
#define   ACCTL_AUDOEHLT	0x00010000	
#define   ACCTL_AUDIEHLT	0x00100000	
#define ACREGACC		0x08	
#define   ACREGACC_DAT_SHIFT	0	
#define   ACREGACC_REG_SHIFT	16	
#define   ACREGACC_CODECID_SHIFT	24	
#define   ACREGACC_READ		0x80000000	
#define   ACREGACC_WRITE	0x00000000	
#define ACINTSTS		0x10	
#define ACINTMSTS		0x14	
#define ACINTEN			0x18	
#define ACINTDIS		0x1c	
#define   ACINT_CODECRDY(n)	(0x00000001 << (n))	
#define   ACINT_REGACCRDY	0x00000010	
#define   ACINT_AUDOERR		0x00000100	
#define   ACINT_AUDIERR		0x00001000	
#define ACDMASTS		0x80	
#define   ACDMA_AUDO		0x00000001	
#define   ACDMA_AUDI		0x00000010	
#define ACAUDODAT		0xa0	
#define ACAUDIDAT		0xb0	
#define ACREVID			0xfc	

struct txx9aclc_dmadata {
	struct resource *dma_res;
	struct txx9dmac_slave dma_slave;
	struct dma_chan *dma_chan;
	struct tasklet_struct tasklet;
	spinlock_t dma_lock;
	int stream; 
	struct snd_pcm_substream *substream;
	unsigned long pos;
	dma_addr_t dma_addr;
	unsigned long buffer_bytes;
	unsigned long period_bytes;
	unsigned long frag_bytes;
	int frags;
	int frag_count;
	int dmacount;
};

struct txx9aclc_plat_drvdata {
	void __iomem *base;
	u64 physbase;
};

static inline struct txx9aclc_plat_drvdata *txx9aclc_get_plat_drvdata(
	struct snd_soc_dai *dai)
{
	return dev_get_drvdata(dai->dev);
}

#endif 
