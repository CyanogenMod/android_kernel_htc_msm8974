/*
 *  linux/sound/arm/aaci.c - ARM PrimeCell AACI PL041 driver
 *
 *  Copyright (C) 2003 Deep Blue Solutions, Ltd, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef AACI_H
#define AACI_H

#define AACI_CSCH1	0x000
#define AACI_CSCH2	0x014
#define AACI_CSCH3	0x028
#define AACI_CSCH4	0x03c

#define AACI_RXCR	0x000	
#define AACI_TXCR	0x004	
#define AACI_SR		0x008	
#define AACI_ISR	0x00c	
#define AACI_IE 	0x010	

#define AACI_SL1RX	0x050
#define AACI_SL1TX	0x054
#define AACI_SL2RX	0x058
#define AACI_SL2TX	0x05c
#define AACI_SL12RX	0x060
#define AACI_SL12TX	0x064
#define AACI_SLFR	0x068	
#define AACI_SLISTAT	0x06c	
#define AACI_SLIEN	0x070	
#define AACI_INTCLR	0x074	
#define AACI_MAINCR	0x078	
#define AACI_RESET	0x07c	
#define AACI_SYNC	0x080	
#define AACI_ALLINTS	0x084	
#define AACI_MAINFR	0x088	
#define AACI_DR1	0x090	/* data read/written fifo 1 */
#define AACI_DR2	0x0b0	/* data read/written fifo 2 */
#define AACI_DR3	0x0d0	/* data read/written fifo 3 */
#define AACI_DR4	0x0f0	/* data read/written fifo 4 */

#define CR_FEN		(1 << 16)	
#define CR_COMPACT	(1 << 15)	
#define CR_SZ16		(0 << 13)	
#define CR_SZ18		(1 << 13)	
#define CR_SZ20		(2 << 13)	
#define CR_SZ12		(3 << 13)	
#define CR_SL12		(1 << 12)
#define CR_SL11		(1 << 11)
#define CR_SL10		(1 << 10)
#define CR_SL9		(1 << 9)
#define CR_SL8		(1 << 8)
#define CR_SL7		(1 << 7)
#define CR_SL6		(1 << 6)
#define CR_SL5		(1 << 5)
#define CR_SL4		(1 << 4)
#define CR_SL3		(1 << 3)
#define CR_SL2		(1 << 2)
#define CR_SL1		(1 << 1)
#define CR_EN		(1 << 0)	

#define SR_RXTOFE	(1 << 11)	
#define SR_TXTO		(1 << 10)	
#define SR_TXU		(1 << 9)	
#define SR_RXO		(1 << 8)	
#define SR_TXB		(1 << 7)	
#define SR_RXB		(1 << 6)	
#define SR_TXFF		(1 << 5)	
#define SR_RXFF		(1 << 4)	
#define SR_TXHE		(1 << 3)	
#define SR_RXHF		(1 << 2)	
#define SR_TXFE		(1 << 1)	
#define SR_RXFE		(1 << 0)	

#define ISR_RXTOFEINTR	(1 << 6)	
#define ISR_URINTR	(1 << 5)	
#define ISR_ORINTR	(1 << 4)	
#define ISR_RXINTR	(1 << 3)	
#define ISR_TXINTR	(1 << 2)	
#define ISR_RXTOINTR	(1 << 1)	
#define ISR_TXCINTR	(1 << 0)	

#define IE_RXTOIE	(1 << 6)
#define IE_URIE		(1 << 5)
#define IE_ORIE		(1 << 4)
#define IE_RXIE		(1 << 3)
#define IE_TXIE		(1 << 2)
#define IE_RXTIE	(1 << 1)
#define IE_TXCIE	(1 << 0)

#define ISR_RXTOFE	(1 << 6)	
#define ISR_UR		(1 << 5)	
#define ISR_OR		(1 << 4)	
#define ISR_RX		(1 << 3)	
#define ISR_TX		(1 << 2)	
#define ISR_RXTO	(1 << 1)	
#define ISR_TXC		(1 << 0)	

#define IE_RXTOFE	(1 << 6)	
#define IE_UR		(1 << 5)	
#define IE_OR		(1 << 4)	
#define IE_RX		(1 << 3)	
#define IE_TX		(1 << 2)	
#define IE_RXTO		(1 << 1)	
#define IE_TXC		(1 << 0)	

#define SLFR_RWIS	(1 << 13)	
#define SLFR_RGPIOINTR	(1 << 12)	
#define SLFR_12TXE	(1 << 11)	
#define SLFR_12RXV	(1 << 10)	
#define SLFR_2TXE	(1 << 9)	
#define SLFR_2RXV	(1 << 8)	
#define SLFR_1TXE	(1 << 7)	
#define SLFR_1RXV	(1 << 6)	
#define SLFR_12TXB	(1 << 5)	
#define SLFR_12RXB	(1 << 4)	
#define SLFR_2TXB	(1 << 3)	
#define SLFR_2RXB	(1 << 2)	
#define SLFR_1TXB	(1 << 1)	
#define SLFR_1RXB	(1 << 0)	

#define ICLR_RXTOFEC4	(1 << 12)
#define ICLR_RXTOFEC3	(1 << 11)
#define ICLR_RXTOFEC2	(1 << 10)
#define ICLR_RXTOFEC1	(1 << 9)
#define ICLR_TXUEC4	(1 << 8)
#define ICLR_TXUEC3	(1 << 7)
#define ICLR_TXUEC2	(1 << 6)
#define ICLR_TXUEC1	(1 << 5)
#define ICLR_RXOEC4	(1 << 4)
#define ICLR_RXOEC3	(1 << 3)
#define ICLR_RXOEC2	(1 << 2)
#define ICLR_RXOEC1	(1 << 1)
#define ICLR_WISC	(1 << 0)

#define MAINCR_SCRA(x)	((x) << 10)	
#define MAINCR_DMAEN	(1 << 9)	
#define MAINCR_SL12TXEN	(1 << 8)	
#define MAINCR_SL12RXEN	(1 << 7)	
#define MAINCR_SL2TXEN	(1 << 6)	
#define MAINCR_SL2RXEN	(1 << 5)	
#define MAINCR_SL1TXEN	(1 << 4)	
#define MAINCR_SL1RXEN	(1 << 3)	
#define MAINCR_LPM	(1 << 2)	
#define MAINCR_LOOPBK	(1 << 1)	
#define MAINCR_IE	(1 << 0)	

#define RESET_NRST	(1 << 0)

#define SYNC_FORCE	(1 << 0)

#define MAINFR_TXB	(1 << 1)	
#define MAINFR_RXB	(1 << 0)	



struct aaci_runtime {
	void			__iomem *base;
	void			__iomem *fifo;
	spinlock_t		lock;

	struct ac97_pcm		*pcm;
	int			pcm_open;

	u32			cr;
	struct snd_pcm_substream	*substream;

	unsigned int		period;	

	void			*start;
	void			*end;
	void			*ptr;
	int			bytes;
	unsigned int		fifo_bytes;
};

struct aaci {
	struct amba_device	*dev;
	struct snd_card		*card;
	void			__iomem *base;
	unsigned int		fifo_depth;
	unsigned int		users;
	struct mutex		irq_lock;

	
	struct mutex		ac97_sem;
	struct snd_ac97_bus	*ac97_bus;
	struct snd_ac97		*ac97;

	u32			maincr;

	struct aaci_runtime	playback;
	struct aaci_runtime	capture;

	struct snd_pcm		*pcm;
};

#define ACSTREAM_FRONT		0
#define ACSTREAM_SURROUND	1
#define ACSTREAM_LFE		2

#endif
