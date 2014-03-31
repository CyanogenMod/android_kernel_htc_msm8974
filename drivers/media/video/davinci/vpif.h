/*
 * VPIF header file
 *
 * Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed .as is. WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef VPIF_H
#define VPIF_H

#include <linux/io.h>
#include <linux/videodev2.h>
#include <media/davinci/vpif_types.h>

#define VPIF_NUM_CHANNELS		(4)
#define VPIF_CAPTURE_NUM_CHANNELS	(2)
#define VPIF_DISPLAY_NUM_CHANNELS	(2)

extern void __iomem *vpif_base;
extern spinlock_t vpif_lock;

#define regr(reg)               readl((reg) + vpif_base)
#define regw(value, reg)        writel(value, (reg + vpif_base))

#define VPIF_PID			(0x0000)
#define VPIF_CH0_CTRL			(0x0004)
#define VPIF_CH1_CTRL			(0x0008)
#define VPIF_CH2_CTRL			(0x000C)
#define VPIF_CH3_CTRL			(0x0010)

#define VPIF_INTEN			(0x0020)
#define VPIF_INTEN_SET			(0x0024)
#define VPIF_INTEN_CLR			(0x0028)
#define VPIF_STATUS			(0x002C)
#define VPIF_STATUS_CLR			(0x0030)
#define VPIF_EMULATION_CTRL		(0x0034)
#define VPIF_REQ_SIZE			(0x0038)

#define VPIF_CH0_TOP_STRT_ADD_LUMA	(0x0040)
#define VPIF_CH0_BTM_STRT_ADD_LUMA	(0x0044)
#define VPIF_CH0_TOP_STRT_ADD_CHROMA	(0x0048)
#define VPIF_CH0_BTM_STRT_ADD_CHROMA	(0x004c)
#define VPIF_CH0_TOP_STRT_ADD_HANC	(0x0050)
#define VPIF_CH0_BTM_STRT_ADD_HANC	(0x0054)
#define VPIF_CH0_TOP_STRT_ADD_VANC	(0x0058)
#define VPIF_CH0_BTM_STRT_ADD_VANC	(0x005c)
#define VPIF_CH0_SP_CFG			(0x0060)
#define VPIF_CH0_IMG_ADD_OFST		(0x0064)
#define VPIF_CH0_HANC_ADD_OFST		(0x0068)
#define VPIF_CH0_H_CFG			(0x006c)
#define VPIF_CH0_V_CFG_00		(0x0070)
#define VPIF_CH0_V_CFG_01		(0x0074)
#define VPIF_CH0_V_CFG_02		(0x0078)
#define VPIF_CH0_V_CFG_03		(0x007c)

#define VPIF_CH1_TOP_STRT_ADD_LUMA	(0x0080)
#define VPIF_CH1_BTM_STRT_ADD_LUMA	(0x0084)
#define VPIF_CH1_TOP_STRT_ADD_CHROMA	(0x0088)
#define VPIF_CH1_BTM_STRT_ADD_CHROMA	(0x008c)
#define VPIF_CH1_TOP_STRT_ADD_HANC	(0x0090)
#define VPIF_CH1_BTM_STRT_ADD_HANC	(0x0094)
#define VPIF_CH1_TOP_STRT_ADD_VANC	(0x0098)
#define VPIF_CH1_BTM_STRT_ADD_VANC	(0x009c)
#define VPIF_CH1_SP_CFG			(0x00a0)
#define VPIF_CH1_IMG_ADD_OFST		(0x00a4)
#define VPIF_CH1_HANC_ADD_OFST		(0x00a8)
#define VPIF_CH1_H_CFG			(0x00ac)
#define VPIF_CH1_V_CFG_00		(0x00b0)
#define VPIF_CH1_V_CFG_01		(0x00b4)
#define VPIF_CH1_V_CFG_02		(0x00b8)
#define VPIF_CH1_V_CFG_03		(0x00bc)

#define VPIF_CH2_TOP_STRT_ADD_LUMA	(0x00c0)
#define VPIF_CH2_BTM_STRT_ADD_LUMA	(0x00c4)
#define VPIF_CH2_TOP_STRT_ADD_CHROMA	(0x00c8)
#define VPIF_CH2_BTM_STRT_ADD_CHROMA	(0x00cc)
#define VPIF_CH2_TOP_STRT_ADD_HANC	(0x00d0)
#define VPIF_CH2_BTM_STRT_ADD_HANC	(0x00d4)
#define VPIF_CH2_TOP_STRT_ADD_VANC	(0x00d8)
#define VPIF_CH2_BTM_STRT_ADD_VANC	(0x00dc)
#define VPIF_CH2_SP_CFG			(0x00e0)
#define VPIF_CH2_IMG_ADD_OFST		(0x00e4)
#define VPIF_CH2_HANC_ADD_OFST		(0x00e8)
#define VPIF_CH2_H_CFG			(0x00ec)
#define VPIF_CH2_V_CFG_00		(0x00f0)
#define VPIF_CH2_V_CFG_01		(0x00f4)
#define VPIF_CH2_V_CFG_02		(0x00f8)
#define VPIF_CH2_V_CFG_03		(0x00fc)
#define VPIF_CH2_HANC0_STRT		(0x0100)
#define VPIF_CH2_HANC0_SIZE		(0x0104)
#define VPIF_CH2_HANC1_STRT		(0x0108)
#define VPIF_CH2_HANC1_SIZE		(0x010c)
#define VPIF_CH2_VANC0_STRT		(0x0110)
#define VPIF_CH2_VANC0_SIZE		(0x0114)
#define VPIF_CH2_VANC1_STRT		(0x0118)
#define VPIF_CH2_VANC1_SIZE		(0x011c)

#define VPIF_CH3_TOP_STRT_ADD_LUMA	(0x0140)
#define VPIF_CH3_BTM_STRT_ADD_LUMA	(0x0144)
#define VPIF_CH3_TOP_STRT_ADD_CHROMA	(0x0148)
#define VPIF_CH3_BTM_STRT_ADD_CHROMA	(0x014c)
#define VPIF_CH3_TOP_STRT_ADD_HANC	(0x0150)
#define VPIF_CH3_BTM_STRT_ADD_HANC	(0x0154)
#define VPIF_CH3_TOP_STRT_ADD_VANC	(0x0158)
#define VPIF_CH3_BTM_STRT_ADD_VANC	(0x015c)
#define VPIF_CH3_SP_CFG			(0x0160)
#define VPIF_CH3_IMG_ADD_OFST		(0x0164)
#define VPIF_CH3_HANC_ADD_OFST		(0x0168)
#define VPIF_CH3_H_CFG			(0x016c)
#define VPIF_CH3_V_CFG_00		(0x0170)
#define VPIF_CH3_V_CFG_01		(0x0174)
#define VPIF_CH3_V_CFG_02		(0x0178)
#define VPIF_CH3_V_CFG_03		(0x017c)
#define VPIF_CH3_HANC0_STRT		(0x0180)
#define VPIF_CH3_HANC0_SIZE		(0x0184)
#define VPIF_CH3_HANC1_STRT		(0x0188)
#define VPIF_CH3_HANC1_SIZE		(0x018c)
#define VPIF_CH3_VANC0_STRT		(0x0190)
#define VPIF_CH3_VANC0_SIZE		(0x0194)
#define VPIF_CH3_VANC1_STRT		(0x0198)
#define VPIF_CH3_VANC1_SIZE		(0x019c)

#define VPIF_IODFT_CTRL			(0x01c0)

static inline void vpif_set_bit(u32 reg, u32 bit)
{
	regw((regr(reg)) | (0x01 << bit), reg);
}

static inline void vpif_clr_bit(u32 reg, u32 bit)
{
	regw(((regr(reg)) & ~(0x01 << bit)), reg);
}

#ifdef GENERATE_MASK
#undef GENERATE_MASK
#endif

#define GENERATE_MASK(bits, pos) \
		((((0xFFFFFFFF) << (32 - bits)) >> (32 - bits)) << pos)

#define VPIF_CH_DATA_MODE_BIT	(2)
#define VPIF_CH_YC_MUX_BIT	(3)
#define VPIF_CH_SDR_FMT_BIT	(4)
#define VPIF_CH_HANC_EN_BIT	(8)
#define VPIF_CH_VANC_EN_BIT	(9)

#define VPIF_CAPTURE_CH_NIP	(10)
#define VPIF_DISPLAY_CH_NIP	(11)

#define VPIF_DISPLAY_PIX_EN_BIT	(10)

#define VPIF_CH_INPUT_FIELD_FRAME_BIT	(12)

#define VPIF_CH_FID_POLARITY_BIT	(15)
#define VPIF_CH_V_VALID_POLARITY_BIT	(14)
#define VPIF_CH_H_VALID_POLARITY_BIT	(13)
#define VPIF_CH_DATA_WIDTH_BIT		(28)

#define VPIF_CH_CLK_EDGE_CTRL_BIT	(31)

#define VPIF_CH_EAVSAV_MASK	GENERATE_MASK(13, 0)
#define VPIF_CH_LEN_MASK	GENERATE_MASK(12, 0)
#define VPIF_CH_WIDTH_MASK	GENERATE_MASK(13, 0)
#define VPIF_CH_LEN_SHIFT	(16)

#define VPIF_REQ_SIZE_MASK	(0x1ff)

#define VPIF_INTEN_FRAME_CH0	(0x00000001)
#define VPIF_INTEN_FRAME_CH1	(0x00000002)
#define VPIF_INTEN_FRAME_CH2	(0x00000004)
#define VPIF_INTEN_FRAME_CH3	(0x00000008)


#define VPIF_CH0_CLK_EN		(0x00000002)
#define VPIF_CH0_EN		(0x00000001)
#define VPIF_CH1_CLK_EN		(0x00000002)
#define VPIF_CH1_EN		(0x00000001)
#define VPIF_CH2_CLK_EN		(0x00000002)
#define VPIF_CH2_EN		(0x00000001)
#define VPIF_CH3_CLK_EN		(0x00000002)
#define VPIF_CH3_EN		(0x00000001)
#define VPIF_CH_CLK_EN		(0x00000002)
#define VPIF_CH_EN		(0x00000001)

#define VPIF_INT_TOP	(0x00)
#define VPIF_INT_BOTTOM	(0x01)
#define VPIF_INT_BOTH	(0x02)

#define VPIF_CH0_INT_CTRL_SHIFT	(6)
#define VPIF_CH1_INT_CTRL_SHIFT	(6)
#define VPIF_CH2_INT_CTRL_SHIFT	(6)
#define VPIF_CH3_INT_CTRL_SHIFT	(6)
#define VPIF_CH_INT_CTRL_SHIFT	(6)

#define channel0_intr_assert()	(regw((regr(VPIF_CH0_CTRL)|\
	(VPIF_INT_BOTH << VPIF_CH0_INT_CTRL_SHIFT)), VPIF_CH0_CTRL))

#define channel1_intr_assert()	(regw((regr(VPIF_CH1_CTRL)|\
	(VPIF_INT_BOTH << VPIF_CH1_INT_CTRL_SHIFT)), VPIF_CH1_CTRL))

#define channel2_intr_assert() 	(regw((regr(VPIF_CH2_CTRL)|\
	(VPIF_INT_BOTH << VPIF_CH2_INT_CTRL_SHIFT)), VPIF_CH2_CTRL))

#define channel3_intr_assert() 	(regw((regr(VPIF_CH3_CTRL)|\
	(VPIF_INT_BOTH << VPIF_CH3_INT_CTRL_SHIFT)), VPIF_CH3_CTRL))

#define VPIF_CH_FID_MASK	(0x20)
#define VPIF_CH_FID_SHIFT	(5)

#define VPIF_NTSC_VBI_START_FIELD0	(1)
#define VPIF_NTSC_VBI_START_FIELD1	(263)
#define VPIF_PAL_VBI_START_FIELD0	(624)
#define VPIF_PAL_VBI_START_FIELD1	(311)

#define VPIF_NTSC_HBI_START_FIELD0	(1)
#define VPIF_NTSC_HBI_START_FIELD1	(263)
#define VPIF_PAL_HBI_START_FIELD0	(624)
#define VPIF_PAL_HBI_START_FIELD1	(311)

#define VPIF_NTSC_VBI_COUNT_FIELD0	(20)
#define VPIF_NTSC_VBI_COUNT_FIELD1	(19)
#define VPIF_PAL_VBI_COUNT_FIELD0	(24)
#define VPIF_PAL_VBI_COUNT_FIELD1	(25)

#define VPIF_NTSC_HBI_COUNT_FIELD0	(263)
#define VPIF_NTSC_HBI_COUNT_FIELD1	(262)
#define VPIF_PAL_HBI_COUNT_FIELD0	(312)
#define VPIF_PAL_HBI_COUNT_FIELD1	(313)

#define VPIF_NTSC_VBI_SAMPLES_PER_LINE	(720)
#define VPIF_PAL_VBI_SAMPLES_PER_LINE	(720)
#define VPIF_NTSC_HBI_SAMPLES_PER_LINE	(268)
#define VPIF_PAL_HBI_SAMPLES_PER_LINE	(280)

#define VPIF_CH_VANC_EN			(0x20)
#define VPIF_DMA_REQ_SIZE		(0x080)
#define VPIF_EMULATION_DISABLE		(0x01)

extern u8 irq_vpif_capture_channel[VPIF_NUM_CHANNELS];

static inline void enable_channel0(int enable)
{
	if (enable)
		regw((regr(VPIF_CH0_CTRL) | (VPIF_CH0_EN)), VPIF_CH0_CTRL);
	else
		regw((regr(VPIF_CH0_CTRL) & (~VPIF_CH0_EN)), VPIF_CH0_CTRL);
}

static inline void enable_channel1(int enable)
{
	if (enable)
		regw((regr(VPIF_CH1_CTRL) | (VPIF_CH1_EN)), VPIF_CH1_CTRL);
	else
		regw((regr(VPIF_CH1_CTRL) & (~VPIF_CH1_EN)), VPIF_CH1_CTRL);
}

static inline void channel0_intr_enable(int enable)
{
	unsigned long flags;

	spin_lock_irqsave(&vpif_lock, flags);

	if (enable) {
		regw((regr(VPIF_INTEN) | 0x10), VPIF_INTEN);
		regw((regr(VPIF_INTEN_SET) | 0x10), VPIF_INTEN_SET);

		regw((regr(VPIF_INTEN) | VPIF_INTEN_FRAME_CH0), VPIF_INTEN);
		regw((regr(VPIF_INTEN_SET) | VPIF_INTEN_FRAME_CH0),
							VPIF_INTEN_SET);
	} else {
		regw((regr(VPIF_INTEN) & (~VPIF_INTEN_FRAME_CH0)), VPIF_INTEN);
		regw((regr(VPIF_INTEN_SET) | VPIF_INTEN_FRAME_CH0),
							VPIF_INTEN_SET);
	}
	spin_unlock_irqrestore(&vpif_lock, flags);
}

static inline void channel1_intr_enable(int enable)
{
	unsigned long flags;

	spin_lock_irqsave(&vpif_lock, flags);

	if (enable) {
		regw((regr(VPIF_INTEN) | 0x10), VPIF_INTEN);
		regw((regr(VPIF_INTEN_SET) | 0x10), VPIF_INTEN_SET);

		regw((regr(VPIF_INTEN) | VPIF_INTEN_FRAME_CH1), VPIF_INTEN);
		regw((regr(VPIF_INTEN_SET) | VPIF_INTEN_FRAME_CH1),
							VPIF_INTEN_SET);
	} else {
		regw((regr(VPIF_INTEN) & (~VPIF_INTEN_FRAME_CH1)), VPIF_INTEN);
		regw((regr(VPIF_INTEN_SET) | VPIF_INTEN_FRAME_CH1),
							VPIF_INTEN_SET);
	}
	spin_unlock_irqrestore(&vpif_lock, flags);
}

static inline void ch0_set_videobuf_addr_yc_nmux(unsigned long top_strt_luma,
						 unsigned long btm_strt_luma,
						 unsigned long top_strt_chroma,
						 unsigned long btm_strt_chroma)
{
	regw(top_strt_luma, VPIF_CH0_TOP_STRT_ADD_LUMA);
	regw(btm_strt_luma, VPIF_CH0_BTM_STRT_ADD_LUMA);
	regw(top_strt_chroma, VPIF_CH1_TOP_STRT_ADD_CHROMA);
	regw(btm_strt_chroma, VPIF_CH1_BTM_STRT_ADD_CHROMA);
}

static inline void ch0_set_videobuf_addr(unsigned long top_strt_luma,
					 unsigned long btm_strt_luma,
					 unsigned long top_strt_chroma,
					 unsigned long btm_strt_chroma)
{
	regw(top_strt_luma, VPIF_CH0_TOP_STRT_ADD_LUMA);
	regw(btm_strt_luma, VPIF_CH0_BTM_STRT_ADD_LUMA);
	regw(top_strt_chroma, VPIF_CH0_TOP_STRT_ADD_CHROMA);
	regw(btm_strt_chroma, VPIF_CH0_BTM_STRT_ADD_CHROMA);
}

static inline void ch1_set_videobuf_addr(unsigned long top_strt_luma,
					 unsigned long btm_strt_luma,
					 unsigned long top_strt_chroma,
					 unsigned long btm_strt_chroma)
{

	regw(top_strt_luma, VPIF_CH1_TOP_STRT_ADD_LUMA);
	regw(btm_strt_luma, VPIF_CH1_BTM_STRT_ADD_LUMA);
	regw(top_strt_chroma, VPIF_CH1_TOP_STRT_ADD_CHROMA);
	regw(btm_strt_chroma, VPIF_CH1_BTM_STRT_ADD_CHROMA);
}

static inline void ch0_set_vbi_addr(unsigned long top_vbi,
	unsigned long btm_vbi, unsigned long a, unsigned long b)
{
	regw(top_vbi, VPIF_CH0_TOP_STRT_ADD_VANC);
	regw(btm_vbi, VPIF_CH0_BTM_STRT_ADD_VANC);
}

static inline void ch0_set_hbi_addr(unsigned long top_vbi,
	unsigned long btm_vbi, unsigned long a, unsigned long b)
{
	regw(top_vbi, VPIF_CH0_TOP_STRT_ADD_HANC);
	regw(btm_vbi, VPIF_CH0_BTM_STRT_ADD_HANC);
}

static inline void ch1_set_vbi_addr(unsigned long top_vbi,
	unsigned long btm_vbi, unsigned long a, unsigned long b)
{
	regw(top_vbi, VPIF_CH1_TOP_STRT_ADD_VANC);
	regw(btm_vbi, VPIF_CH1_BTM_STRT_ADD_VANC);
}

static inline void ch1_set_hbi_addr(unsigned long top_vbi,
	unsigned long btm_vbi, unsigned long a, unsigned long b)
{
	regw(top_vbi, VPIF_CH1_TOP_STRT_ADD_HANC);
	regw(btm_vbi, VPIF_CH1_BTM_STRT_ADD_HANC);
}

static inline void disable_raw_feature(u8 channel_id, u8 index)
{
	u32 ctrl_reg;
	if (0 == channel_id)
		ctrl_reg = VPIF_CH0_CTRL;
	else
		ctrl_reg = VPIF_CH1_CTRL;

	if (1 == index)
		vpif_clr_bit(ctrl_reg, VPIF_CH_VANC_EN_BIT);
	else
		vpif_clr_bit(ctrl_reg, VPIF_CH_HANC_EN_BIT);
}

static inline void enable_raw_feature(u8 channel_id, u8 index)
{
	u32 ctrl_reg;
	if (0 == channel_id)
		ctrl_reg = VPIF_CH0_CTRL;
	else
		ctrl_reg = VPIF_CH1_CTRL;

	if (1 == index)
		vpif_set_bit(ctrl_reg, VPIF_CH_VANC_EN_BIT);
	else
		vpif_set_bit(ctrl_reg, VPIF_CH_HANC_EN_BIT);
}

static inline void enable_channel2(int enable)
{
	if (enable) {
		regw((regr(VPIF_CH2_CTRL) | (VPIF_CH2_CLK_EN)), VPIF_CH2_CTRL);
		regw((regr(VPIF_CH2_CTRL) | (VPIF_CH2_EN)), VPIF_CH2_CTRL);
	} else {
		regw((regr(VPIF_CH2_CTRL) & (~VPIF_CH2_CLK_EN)), VPIF_CH2_CTRL);
		regw((regr(VPIF_CH2_CTRL) & (~VPIF_CH2_EN)), VPIF_CH2_CTRL);
	}
}

static inline void enable_channel3(int enable)
{
	if (enable) {
		regw((regr(VPIF_CH3_CTRL) | (VPIF_CH3_CLK_EN)), VPIF_CH3_CTRL);
		regw((regr(VPIF_CH3_CTRL) | (VPIF_CH3_EN)), VPIF_CH3_CTRL);
	} else {
		regw((regr(VPIF_CH3_CTRL) & (~VPIF_CH3_CLK_EN)), VPIF_CH3_CTRL);
		regw((regr(VPIF_CH3_CTRL) & (~VPIF_CH3_EN)), VPIF_CH3_CTRL);
	}
}

static inline void channel2_intr_enable(int enable)
{
	unsigned long flags;

	spin_lock_irqsave(&vpif_lock, flags);

	if (enable) {
		regw((regr(VPIF_INTEN) | 0x10), VPIF_INTEN);
		regw((regr(VPIF_INTEN_SET) | 0x10), VPIF_INTEN_SET);
		regw((regr(VPIF_INTEN) | VPIF_INTEN_FRAME_CH2), VPIF_INTEN);
		regw((regr(VPIF_INTEN_SET) | VPIF_INTEN_FRAME_CH2),
							VPIF_INTEN_SET);
	} else {
		regw((regr(VPIF_INTEN) & (~VPIF_INTEN_FRAME_CH2)), VPIF_INTEN);
		regw((regr(VPIF_INTEN_SET) | VPIF_INTEN_FRAME_CH2),
							VPIF_INTEN_SET);
	}
	spin_unlock_irqrestore(&vpif_lock, flags);
}

static inline void channel3_intr_enable(int enable)
{
	unsigned long flags;

	spin_lock_irqsave(&vpif_lock, flags);

	if (enable) {
		regw((regr(VPIF_INTEN) | 0x10), VPIF_INTEN);
		regw((regr(VPIF_INTEN_SET) | 0x10), VPIF_INTEN_SET);

		regw((regr(VPIF_INTEN) | VPIF_INTEN_FRAME_CH3), VPIF_INTEN);
		regw((regr(VPIF_INTEN_SET) | VPIF_INTEN_FRAME_CH3),
							VPIF_INTEN_SET);
	} else {
		regw((regr(VPIF_INTEN) & (~VPIF_INTEN_FRAME_CH3)), VPIF_INTEN);
		regw((regr(VPIF_INTEN_SET) | VPIF_INTEN_FRAME_CH3),
							VPIF_INTEN_SET);
	}
	spin_unlock_irqrestore(&vpif_lock, flags);
}

static inline void channel2_raw_enable(int enable, u8 index)
{
	u32 mask;

	if (1 == index)
		mask = VPIF_CH_VANC_EN_BIT;
	else
		mask = VPIF_CH_HANC_EN_BIT;

	if (enable)
		vpif_set_bit(VPIF_CH2_CTRL, mask);
	else
		vpif_clr_bit(VPIF_CH2_CTRL, mask);
}

static inline void channel3_raw_enable(int enable, u8 index)
{
	u32 mask;

	if (1 == index)
		mask = VPIF_CH_VANC_EN_BIT;
	else
		mask = VPIF_CH_HANC_EN_BIT;

	if (enable)
		vpif_set_bit(VPIF_CH3_CTRL, mask);
	else
		vpif_clr_bit(VPIF_CH3_CTRL, mask);
}

static inline void ch2_set_videobuf_addr_yc_nmux(unsigned long top_strt_luma,
						 unsigned long btm_strt_luma,
						 unsigned long top_strt_chroma,
						 unsigned long btm_strt_chroma)
{
	regw(top_strt_luma, VPIF_CH2_TOP_STRT_ADD_LUMA);
	regw(btm_strt_luma, VPIF_CH2_BTM_STRT_ADD_LUMA);
	regw(top_strt_chroma, VPIF_CH3_TOP_STRT_ADD_CHROMA);
	regw(btm_strt_chroma, VPIF_CH3_BTM_STRT_ADD_CHROMA);
}

static inline void ch2_set_videobuf_addr(unsigned long top_strt_luma,
					 unsigned long btm_strt_luma,
					 unsigned long top_strt_chroma,
					 unsigned long btm_strt_chroma)
{
	regw(top_strt_luma, VPIF_CH2_TOP_STRT_ADD_LUMA);
	regw(btm_strt_luma, VPIF_CH2_BTM_STRT_ADD_LUMA);
	regw(top_strt_chroma, VPIF_CH2_TOP_STRT_ADD_CHROMA);
	regw(btm_strt_chroma, VPIF_CH2_BTM_STRT_ADD_CHROMA);
}

static inline void ch3_set_videobuf_addr(unsigned long top_strt_luma,
					 unsigned long btm_strt_luma,
					 unsigned long top_strt_chroma,
					 unsigned long btm_strt_chroma)
{
	regw(top_strt_luma, VPIF_CH3_TOP_STRT_ADD_LUMA);
	regw(btm_strt_luma, VPIF_CH3_BTM_STRT_ADD_LUMA);
	regw(top_strt_chroma, VPIF_CH3_TOP_STRT_ADD_CHROMA);
	regw(btm_strt_chroma, VPIF_CH3_BTM_STRT_ADD_CHROMA);
}

static inline void ch2_set_vbi_addr(unsigned long top_strt_luma,
					 unsigned long btm_strt_luma,
					 unsigned long top_strt_chroma,
					 unsigned long btm_strt_chroma)
{
	regw(top_strt_luma, VPIF_CH2_TOP_STRT_ADD_VANC);
	regw(btm_strt_luma, VPIF_CH2_BTM_STRT_ADD_VANC);
}

static inline void ch3_set_vbi_addr(unsigned long top_strt_luma,
					 unsigned long btm_strt_luma,
					 unsigned long top_strt_chroma,
					 unsigned long btm_strt_chroma)
{
	regw(top_strt_luma, VPIF_CH3_TOP_STRT_ADD_VANC);
	regw(btm_strt_luma, VPIF_CH3_BTM_STRT_ADD_VANC);
}

#define VPIF_MAX_NAME	(30)

struct vpif_channel_config_params {
	char name[VPIF_MAX_NAME];	
	u16 width;			
	u16 height;			
	u8 frm_fmt;			
	u8 ycmux_mode;			
	u16 eav2sav;			
	u16 sav2eav;			
	u16 l1, l3, l5, l7, l9, l11;	
	u16 vsize;			
	u8 capture_format;		
	u8  vbi_supported;		
	u8 hd_sd;			
	v4l2_std_id stdid;		
	u32 dv_preset;			
};

extern const unsigned int vpif_ch_params_count;
extern const struct vpif_channel_config_params ch_params[];

struct vpif_video_params;
struct vpif_params;
struct vpif_vbi_params;

int vpif_set_video_params(struct vpif_params *vpifparams, u8 channel_id);
void vpif_set_vbi_display_params(struct vpif_vbi_params *vbiparams,
							u8 channel_id);
int vpif_channel_getfid(u8 channel_id);

enum data_size {
	_8BITS = 0,
	_10BITS,
	_12BITS,
};

struct vpif_vbi_params {
	__u32 hstart0;  
	__u32 vstart0;  
	__u32 hsize0;   
	__u32 vsize0;   
	__u32 hstart1;  
	__u32 vstart1;  
	__u32 hsize1;   
	__u32 vsize1;   
};

struct vpif_video_params {
	__u8 storage_mode;	
	unsigned long hpitch;
	v4l2_std_id stdid;
};

struct vpif_params {
	struct vpif_interface iface;
	struct vpif_video_params video_params;
	struct vpif_channel_config_params std_info;
	union param {
		struct vpif_vbi_params	vbi_params;
		enum data_size data_sz;
	} params;
};

#endif				

