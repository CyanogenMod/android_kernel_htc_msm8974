/*
 * Driver for the VINO (Video In No Out) system found in SGI Indys.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * Copyright (C) 1999 Ulf Karlsson <ulfc@bun.falkenberg.se>
 * Copyright (C) 2003 Ladislav Michl <ladis@linux-mips.org>
 */

#ifndef _VINO_H_
#define _VINO_H_

#define VINO_BASE	0x00080000	
#define VINO_PAGE_SIZE	4096

struct sgi_vino_channel {
	u32 _pad_alpha;
	volatile u32 alpha;

#define VINO_CLIP_X(x)		((x) & 0x3ff)		
#define VINO_CLIP_ODD(x)	(((x) & 0x1ff) << 10)	
#define VINO_CLIP_EVEN(x)	(((x) & 0x1ff) << 19)	
	u32 _pad_clip_start;
	volatile u32 clip_start;
	u32 _pad_clip_end;
	volatile u32 clip_end;

#define VINO_FRAMERT_FULL	0xfff
#define VINO_FRAMERT_PAL	(1<<0)			
#define VINO_FRAMERT_RT(x)	(((x) & 0xfff) << 1)	
	u32 _pad_frame_rate;
	volatile u32 frame_rate;

	u32 _pad_field_counter;
	volatile u32 field_counter;
	u32 _pad_line_size;
	volatile u32 line_size;
	u32 _pad_line_count;
	volatile u32 line_count;
	u32 _pad_page_index;
	volatile u32 page_index;
	u32 _pad_next_4_desc;
	volatile u32 next_4_desc;
	u32 _pad_start_desc_tbl;
	volatile u32 start_desc_tbl;

#define VINO_DESC_JUMP		(1<<30)
#define VINO_DESC_STOP		(1<<31)
#define VINO_DESC_VALID		(1<<32)
	u32 _pad_desc_0;
	volatile u32 desc_0;
	u32 _pad_desc_1;
	volatile u32 desc_1;
	u32 _pad_desc_2;
	volatile u32 desc_2;
	u32 _pad_Bdesc_3;
	volatile u32 desc_3;

	u32 _pad_fifo_thres;
	volatile u32 fifo_thres;
	u32 _pad_fifo_read;
	volatile u32 fifo_read;
	u32 _pad_fifo_write;
	volatile u32 fifo_write;
};

struct sgi_vino {
#define VINO_CHIP_ID		0xb
#define VINO_REV_NUM(x)		((x) & 0x0f)
#define VINO_ID_VALUE(x)	(((x) & 0xf0) >> 4)
	u32 _pad_rev_id;
	volatile u32 rev_id;

#define VINO_CTRL_LITTLE_ENDIAN		(1<<0)
#define VINO_CTRL_A_EOF_INT		(1<<1)	
#define VINO_CTRL_A_FIFO_INT		(1<<2)	
#define VINO_CTRL_A_EOD_INT		(1<<3)	
#define VINO_CTRL_A_INT			(VINO_CTRL_A_EOF_INT | \
					 VINO_CTRL_A_FIFO_INT | \
					 VINO_CTRL_A_EOD_INT)
#define VINO_CTRL_B_EOF_INT		(1<<4)	
#define VINO_CTRL_B_FIFO_INT		(1<<5)	
#define VINO_CTRL_B_EOD_INT		(1<<6)	
#define VINO_CTRL_B_INT			(VINO_CTRL_B_EOF_INT | \
					 VINO_CTRL_B_FIFO_INT | \
					 VINO_CTRL_B_EOD_INT)
#define VINO_CTRL_A_DMA_ENBL		(1<<7)
#define VINO_CTRL_A_INTERLEAVE_ENBL	(1<<8)
#define VINO_CTRL_A_SYNC_ENBL		(1<<9)
#define VINO_CTRL_A_SELECT		(1<<10)	
#define VINO_CTRL_A_RGB			(1<<11)	
#define VINO_CTRL_A_LUMA_ONLY		(1<<12)
#define VINO_CTRL_A_DEC_ENBL		(1<<13)	
#define VINO_CTRL_A_DEC_SCALE_MASK	0x1c000	
#define VINO_CTRL_A_DEC_SCALE_SHIFT	(14)
#define VINO_CTRL_A_DEC_HOR_ONLY	(1<<17)	
#define VINO_CTRL_A_DITHER		(1<<18)	
#define VINO_CTRL_B_DMA_ENBL		(1<<19)
#define VINO_CTRL_B_INTERLEAVE_ENBL	(1<<20)
#define VINO_CTRL_B_SYNC_ENBL		(1<<21)
#define VINO_CTRL_B_SELECT		(1<<22)	
#define VINO_CTRL_B_RGB			(1<<23)	
#define VINO_CTRL_B_LUMA_ONLY		(1<<24)
#define VINO_CTRL_B_DEC_ENBL		(1<<25)	
#define VINO_CTRL_B_DEC_SCALE_MASK	0x1c000000	
#define VINO_CTRL_B_DEC_SCALE_SHIFT	(26)
#define VINO_CTRL_B_DEC_HOR_ONLY	(1<<29)	
#define VINO_CTRL_B_DITHER		(1<<30)	
	u32 _pad_control;
	volatile u32 control;

#define VINO_INTSTAT_A_EOF		(1<<0)	
#define VINO_INTSTAT_A_FIFO		(1<<1)	
#define VINO_INTSTAT_A_EOD		(1<<2)	
#define VINO_INTSTAT_A			(VINO_INTSTAT_A_EOF | \
					 VINO_INTSTAT_A_FIFO | \
					 VINO_INTSTAT_A_EOD)
#define VINO_INTSTAT_B_EOF		(1<<3)	
#define VINO_INTSTAT_B_FIFO		(1<<4)	
#define VINO_INTSTAT_B_EOD		(1<<5)	
#define VINO_INTSTAT_B			(VINO_INTSTAT_B_EOF | \
					 VINO_INTSTAT_B_FIFO | \
					 VINO_INTSTAT_B_EOD)
	u32 _pad_intr_status;
	volatile u32 intr_status;

	u32 _pad_i2c_control;
	volatile u32 i2c_control;
	u32 _pad_i2c_data;
	volatile u32 i2c_data;

	struct sgi_vino_channel a;
	struct sgi_vino_channel b;
};

#endif
