/*
 * utils.h: Utilities for SPU-side of the context switch operation.
 *
 * (C) Copyright IBM 2005
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _SPU_CONTEXT_UTILS_H_
#define _SPU_CONTEXT_UTILS_H_

typedef union {
	unsigned long long ull;
	unsigned int ui[2];
} addr64;

typedef union {
	unsigned int slot[4];
	vector unsigned int v;
} spu_reg128v;

struct dma_list_elem {
	unsigned int size;
	unsigned int ea_low;
};

struct dma_list_elem dma_list[15] __attribute__ ((aligned(8)));

extern spu_reg128v regs_spill[NR_SPU_SPILL_REGS];

static struct spu_lscsa *dummy = (struct spu_lscsa *)0;
#define LSCSA_BYTE_OFFSET(_field)  \
	((char *)(&(dummy->_field)) - (char *)(&(dummy->gprs[0].slot[0])))
#define LSCSA_QW_OFFSET(_field)  (LSCSA_BYTE_OFFSET(_field) >> 4)

static inline void set_event_mask(void)
{
	unsigned int event_mask = 0;

	spu_writech(SPU_WrEventMask, event_mask);
}

static inline void set_tag_mask(void)
{
	unsigned int tag_mask = 1;

	spu_writech(MFC_WrTagMask, tag_mask);
}

static inline void build_dma_list(addr64 lscsa_ea)
{
	unsigned int ea_low;
	int i;

	ea_low = lscsa_ea.ui[1];
	ea_low += LSCSA_BYTE_OFFSET(ls[16384]);

	for (i = 0; i < 15; i++, ea_low += 16384) {
		dma_list[i].size = 16384;
		dma_list[i].ea_low = ea_low;
	}
}

static inline void enqueue_putllc(addr64 lscsa_ea)
{
	unsigned int ls = 0;
	unsigned int size = 128;
	unsigned int tag_id = 0;
	unsigned int cmd = 0xB4;	

	spu_writech(MFC_LSA, ls);
	spu_writech(MFC_EAH, lscsa_ea.ui[0]);
	spu_writech(MFC_EAL, lscsa_ea.ui[1]);
	spu_writech(MFC_Size, size);
	spu_writech(MFC_TagID, tag_id);
	spu_writech(MFC_Cmd, cmd);
}

static inline void set_tag_update(void)
{
	unsigned int update_any = 1;

	spu_writech(MFC_WrTagUpdate, update_any);
}

static inline void read_tag_status(void)
{
	spu_readch(MFC_RdTagStat);
}

static inline void read_llar_status(void)
{
	spu_readch(MFC_RdAtomicStat);
}

#endif				
