/*
 * SH-Mobile High-Definition Multimedia Interface (HDMI) driver
 * for SLISHDMI13T and SLIPHDMIT IP cores
 *
 * Copyright (C) 2010, Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include <video/sh_mobile_hdmi.h>
#include <video/sh_mobile_lcdc.h>

#include "sh_mobile_lcdcfb.h"

#define HDMI_SYSTEM_CTRL			0x00 
#define HDMI_L_R_DATA_SWAP_CTRL_RPKT		0x01 
#define HDMI_20_BIT_N_FOR_AUDIO_RPKT_15_8	0x02 
#define HDMI_20_BIT_N_FOR_AUDIO_RPKT_7_0	0x03 
#define HDMI_SPDIF_AUDIO_SAMP_FREQ_CTS		0x04 
#define HDMI_INTERNAL_CTS_15_8			0x05 
#define HDMI_INTERNAL_CTS_7_0			0x06 
#define HDMI_EXTERNAL_CTS_19_16			0x07 
#define HDMI_EXTERNAL_CTS_15_8			0x08 
#define HDMI_EXTERNAL_CTS_7_0			0x09 
#define HDMI_AUDIO_SETTING_1			0x0A 
#define HDMI_AUDIO_SETTING_2			0x0B 
#define HDMI_I2S_AUDIO_SET			0x0C 
#define HDMI_DSD_AUDIO_SET			0x0D 
#define HDMI_DEBUG_MONITOR_1			0x0E 
#define HDMI_DEBUG_MONITOR_2			0x0F 
#define HDMI_I2S_INPUT_PIN_SWAP			0x10 
#define HDMI_AUDIO_STATUS_BITS_SETTING_1	0x11 
#define HDMI_AUDIO_STATUS_BITS_SETTING_2	0x12 
#define HDMI_CATEGORY_CODE			0x13 
#define HDMI_SOURCE_NUM_AUDIO_WORD_LEN		0x14 
#define HDMI_AUDIO_VIDEO_SETTING_1		0x15 
#define HDMI_VIDEO_SETTING_1			0x16 
#define HDMI_DEEP_COLOR_MODES			0x17 

#define HDMI_COLOR_SPACE_CONVERSION_PARAMETERS	0x18

#define HDMI_EXTERNAL_VIDEO_PARAM_SETTINGS	0x30 
#define HDMI_EXTERNAL_H_TOTAL_7_0		0x31 
#define HDMI_EXTERNAL_H_TOTAL_11_8		0x32 
#define HDMI_EXTERNAL_H_BLANK_7_0		0x33 
#define HDMI_EXTERNAL_H_BLANK_9_8		0x34 
#define HDMI_EXTERNAL_H_DELAY_7_0		0x35 
#define HDMI_EXTERNAL_H_DELAY_9_8		0x36 
#define HDMI_EXTERNAL_H_DURATION_7_0		0x37 
#define HDMI_EXTERNAL_H_DURATION_9_8		0x38 
#define HDMI_EXTERNAL_V_TOTAL_7_0		0x39 
#define HDMI_EXTERNAL_V_TOTAL_9_8		0x3A 
#define HDMI_AUDIO_VIDEO_SETTING_2		0x3B 
#define HDMI_EXTERNAL_V_BLANK			0x3D 
#define HDMI_EXTERNAL_V_DELAY			0x3E 
#define HDMI_EXTERNAL_V_DURATION		0x3F 
#define HDMI_CTRL_PKT_MANUAL_SEND_CONTROL	0x40 
#define HDMI_CTRL_PKT_AUTO_SEND			0x41 
#define HDMI_AUTO_CHECKSUM_OPTION		0x42 
#define HDMI_VIDEO_SETTING_2			0x45 
#define HDMI_OUTPUT_OPTION			0x46 
#define HDMI_SLIPHDMIT_PARAM_OPTION		0x51 
#define HDMI_HSYNC_PMENT_AT_EMB_7_0		0x52 
#define HDMI_HSYNC_PMENT_AT_EMB_15_8		0x53 
#define HDMI_VSYNC_PMENT_AT_EMB_7_0		0x54 
#define HDMI_VSYNC_PMENT_AT_EMB_14_8		0x55 
#define HDMI_SLIPHDMIT_PARAM_SETTINGS_1		0x56 
#define HDMI_SLIPHDMIT_PARAM_SETTINGS_2		0x57 
#define HDMI_SLIPHDMIT_PARAM_SETTINGS_3		0x58 
#define HDMI_SLIPHDMIT_PARAM_SETTINGS_5		0x59 
#define HDMI_SLIPHDMIT_PARAM_SETTINGS_6		0x5A 
#define HDMI_SLIPHDMIT_PARAM_SETTINGS_7		0x5B 
#define HDMI_SLIPHDMIT_PARAM_SETTINGS_8		0x5C 
#define HDMI_SLIPHDMIT_PARAM_SETTINGS_9		0x5D 
#define HDMI_SLIPHDMIT_PARAM_SETTINGS_10	0x5E 
#define HDMI_CTRL_PKT_BUF_INDEX			0x5F 
#define HDMI_CTRL_PKT_BUF_ACCESS_HB0		0x60 
#define HDMI_CTRL_PKT_BUF_ACCESS_HB1		0x61 
#define HDMI_CTRL_PKT_BUF_ACCESS_HB2		0x62 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB0		0x63 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB1		0x64 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB2		0x65 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB3		0x66 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB4		0x67 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB5		0x68 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB6		0x69 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB7		0x6A 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB8		0x6B 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB9		0x6C 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB10		0x6D 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB11		0x6E 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB12		0x6F 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB13		0x70 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB14		0x71 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB15		0x72 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB16		0x73 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB17		0x74 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB18		0x75 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB19		0x76 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB20		0x77 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB21		0x78 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB22		0x79 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB23		0x7A 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB24		0x7B 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB25		0x7C 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB26		0x7D 
#define HDMI_CTRL_PKT_BUF_ACCESS_PB27		0x7E 
#define HDMI_EDID_KSV_FIFO_ACCESS_WINDOW	0x80 
#define HDMI_DDC_BUS_ACCESS_FREQ_CTRL_7_0	0x81 
#define HDMI_DDC_BUS_ACCESS_FREQ_CTRL_15_8	0x82 
#define HDMI_INTERRUPT_MASK_1			0x92 
#define HDMI_INTERRUPT_MASK_2			0x93 
#define HDMI_INTERRUPT_STATUS_1			0x94 
#define HDMI_INTERRUPT_STATUS_2			0x95 
#define HDMI_INTERRUPT_MASK_3			0x96 
#define HDMI_INTERRUPT_MASK_4			0x97 
#define HDMI_INTERRUPT_STATUS_3			0x98 
#define HDMI_INTERRUPT_STATUS_4			0x99 
#define HDMI_SOFTWARE_HDCP_CONTROL_1		0x9A 
#define HDMI_FRAME_COUNTER			0x9C 
#define HDMI_FRAME_COUNTER_FOR_RI_CHECK		0x9D 
#define HDMI_HDCP_CONTROL			0xAF 
#define HDMI_RI_FRAME_COUNT_REGISTER		0xB2 
#define HDMI_DDC_BUS_CONTROL			0xB7 
#define HDMI_HDCP_STATUS			0xB8 
#define HDMI_SHA0				0xB9 
#define HDMI_SHA1				0xBA 
#define HDMI_SHA2				0xBB 
#define HDMI_SHA3				0xBC 
#define HDMI_SHA4				0xBD 
#define HDMI_BCAPS_READ				0xBE 
#define HDMI_AKSV_BKSV_7_0_MONITOR		0xBF 
#define HDMI_AKSV_BKSV_15_8_MONITOR		0xC0 
#define HDMI_AKSV_BKSV_23_16_MONITOR		0xC1 
#define HDMI_AKSV_BKSV_31_24_MONITOR		0xC2 
#define HDMI_AKSV_BKSV_39_32_MONITOR		0xC3 
#define HDMI_EDID_SEGMENT_POINTER		0xC4 
#define HDMI_EDID_WORD_ADDRESS			0xC5 
#define HDMI_EDID_DATA_FIFO_ADDRESS		0xC6 
#define HDMI_NUM_OF_HDMI_DEVICES		0xC7 
#define HDMI_HDCP_ERROR_CODE			0xC8 
#define HDMI_100MS_TIMER_SET			0xC9 
#define HDMI_5SEC_TIMER_SET			0xCA 
#define HDMI_RI_READ_COUNT			0xCB 
#define HDMI_AN_SEED				0xCC 
#define HDMI_MAX_NUM_OF_RCIVRS_ALLOWED		0xCD 
#define HDMI_HDCP_MEMORY_ACCESS_CONTROL_1	0xCE 
#define HDMI_HDCP_MEMORY_ACCESS_CONTROL_2	0xCF 
#define HDMI_HDCP_CONTROL_2			0xD0 
#define HDMI_HDCP_KEY_MEMORY_CONTROL		0xD2 
#define HDMI_COLOR_SPACE_CONV_CONFIG_1		0xD3 
#define HDMI_VIDEO_SETTING_3			0xD4 
#define HDMI_RI_7_0				0xD5 
#define HDMI_RI_15_8				0xD6 
#define HDMI_PJ					0xD7 
#define HDMI_SHA_RD				0xD8 
#define HDMI_RI_7_0_SAVED			0xD9 
#define HDMI_RI_15_8_SAVED			0xDA 
#define HDMI_PJ_SAVED				0xDB 
#define HDMI_NUM_OF_DEVICES			0xDC 
#define HDMI_HOT_PLUG_MSENS_STATUS		0xDF 
#define HDMI_BCAPS_WRITE			0xE0 
#define HDMI_BSTAT_7_0				0xE1 
#define HDMI_BSTAT_15_8				0xE2 
#define HDMI_BKSV_7_0				0xE3 
#define HDMI_BKSV_15_8				0xE4 
#define HDMI_BKSV_23_16				0xE5 
#define HDMI_BKSV_31_24				0xE6 
#define HDMI_BKSV_39_32				0xE7 
#define HDMI_AN_7_0				0xE8 
#define HDMI_AN_15_8				0xE9 
#define HDMI_AN_23_16				0xEA 
#define HDMI_AN_31_24				0xEB 
#define HDMI_AN_39_32				0xEC 
#define HDMI_AN_47_40				0xED 
#define HDMI_AN_55_48				0xEE 
#define HDMI_AN_63_56				0xEF 
#define HDMI_PRODUCT_ID				0xF0 
#define HDMI_REVISION_ID			0xF1 
#define HDMI_TEST_MODE				0xFE 

enum hotplug_state {
	HDMI_HOTPLUG_DISCONNECTED,
	HDMI_HOTPLUG_CONNECTED,
	HDMI_HOTPLUG_EDID_DONE,
};

struct sh_hdmi {
	struct sh_mobile_lcdc_entity entity;

	void __iomem *base;
	enum hotplug_state hp_state;	
	u8 preprogrammed_vic;		
	u8 edid_block_addr;
	u8 edid_segment_nr;
	u8 edid_blocks;
	struct clk *hdmi_clk;
	struct device *dev;
	struct delayed_work edid_work;
	struct fb_videomode mode;
	struct fb_monspecs monspec;
};

#define entity_to_sh_hdmi(e)	container_of(e, struct sh_hdmi, entity)

static void hdmi_write(struct sh_hdmi *hdmi, u8 data, u8 reg)
{
	iowrite8(data, hdmi->base + reg);
}

static u8 hdmi_read(struct sh_hdmi *hdmi, u8 reg)
{
	return ioread8(hdmi->base + reg);
}

static unsigned int sh_hdmi_snd_read(struct snd_soc_codec *codec,
				     unsigned int reg)
{
	struct sh_hdmi *hdmi = snd_soc_codec_get_drvdata(codec);

	return hdmi_read(hdmi, reg);
}

static int sh_hdmi_snd_write(struct snd_soc_codec *codec,
			     unsigned int reg,
			     unsigned int value)
{
	struct sh_hdmi *hdmi = snd_soc_codec_get_drvdata(codec);

	hdmi_write(hdmi, value, reg);
	return 0;
}

static struct snd_soc_dai_driver sh_hdmi_dai = {
	.name = "sh_mobile_hdmi-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100  |
			 SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200  |
			 SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 |
			 SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	},
};

static int sh_hdmi_snd_probe(struct snd_soc_codec *codec)
{
	dev_info(codec->dev, "SH Mobile HDMI Audio Codec");

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_sh_hdmi = {
	.probe		= sh_hdmi_snd_probe,
	.read		= sh_hdmi_snd_read,
	.write		= sh_hdmi_snd_write,
};


static void sh_hdmi_external_video_param(struct sh_hdmi *hdmi)
{
	struct fb_videomode *mode = &hdmi->mode;
	u16 htotal, hblank, hdelay, vtotal, vblank, vdelay, voffset;
	u8 sync = 0;

	htotal = mode->xres + mode->right_margin + mode->left_margin
	       + mode->hsync_len;
	hdelay = mode->hsync_len + mode->left_margin;
	hblank = mode->right_margin + hdelay;

	vtotal = mode->yres + mode->upper_margin + mode->lower_margin
	       + mode->vsync_len;
	vdelay = mode->vsync_len + mode->upper_margin;
	vblank = mode->lower_margin + vdelay;
	voffset = min(mode->upper_margin / 2, 6U);

	if (mode->sync & FB_SYNC_HOR_HIGH_ACT)
		sync |= 4;
	if (mode->sync & FB_SYNC_VERT_HIGH_ACT)
		sync |= 8;

	dev_dbg(hdmi->dev, "H: %u, %u, %u, %u; V: %u, %u, %u, %u; sync 0x%x\n",
		htotal, hblank, hdelay, mode->hsync_len,
		vtotal, vblank, vdelay, mode->vsync_len, sync);

	hdmi_write(hdmi, sync | (voffset << 4), HDMI_EXTERNAL_VIDEO_PARAM_SETTINGS);

	hdmi_write(hdmi, htotal, HDMI_EXTERNAL_H_TOTAL_7_0);
	hdmi_write(hdmi, htotal >> 8, HDMI_EXTERNAL_H_TOTAL_11_8);

	hdmi_write(hdmi, hblank, HDMI_EXTERNAL_H_BLANK_7_0);
	hdmi_write(hdmi, hblank >> 8, HDMI_EXTERNAL_H_BLANK_9_8);

	hdmi_write(hdmi, hdelay, HDMI_EXTERNAL_H_DELAY_7_0);
	hdmi_write(hdmi, hdelay >> 8, HDMI_EXTERNAL_H_DELAY_9_8);

	hdmi_write(hdmi, mode->hsync_len, HDMI_EXTERNAL_H_DURATION_7_0);
	hdmi_write(hdmi, mode->hsync_len >> 8, HDMI_EXTERNAL_H_DURATION_9_8);

	hdmi_write(hdmi, vtotal, HDMI_EXTERNAL_V_TOTAL_7_0);
	hdmi_write(hdmi, vtotal >> 8, HDMI_EXTERNAL_V_TOTAL_9_8);

	hdmi_write(hdmi, vblank, HDMI_EXTERNAL_V_BLANK);

	hdmi_write(hdmi, vdelay, HDMI_EXTERNAL_V_DELAY);

	hdmi_write(hdmi, mode->vsync_len, HDMI_EXTERNAL_V_DURATION);

	
	if (!hdmi->preprogrammed_vic)
		hdmi_write(hdmi, sync | 1 | (voffset << 4),
			   HDMI_EXTERNAL_VIDEO_PARAM_SETTINGS);
}

static void sh_hdmi_video_config(struct sh_hdmi *hdmi)
{
	hdmi_write(hdmi, 0x20, HDMI_AUDIO_VIDEO_SETTING_1);

	hdmi_write(hdmi, 0x34, HDMI_VIDEO_SETTING_1);

	hdmi_write(hdmi, 0x20, HDMI_DEEP_COLOR_MODES);
}

static void sh_hdmi_audio_config(struct sh_hdmi *hdmi)
{
	u8 data;
	struct sh_mobile_hdmi_info *pdata = hdmi->dev->platform_data;

	hdmi_write(hdmi, 0x00, HDMI_L_R_DATA_SWAP_CTRL_RPKT);
	
	hdmi_write(hdmi, 0x18, HDMI_20_BIT_N_FOR_AUDIO_RPKT_15_8);
	
	hdmi_write(hdmi, 0x00, HDMI_20_BIT_N_FOR_AUDIO_RPKT_7_0);

	
	hdmi_write(hdmi, 0x20, HDMI_SPDIF_AUDIO_SAMP_FREQ_CTS);

	switch (pdata->flags & HDMI_SND_SRC_MASK) {
	default:
		
	case HDMI_SND_SRC_I2S:
		data = 0x0 << 3;
		break;
	case HDMI_SND_SRC_SPDIF:
		data = 0x1 << 3;
		break;
	case HDMI_SND_SRC_DSD:
		data = 0x2 << 3;
		break;
	case HDMI_SND_SRC_HBR:
		data = 0x3 << 3;
		break;
	}
	hdmi_write(hdmi, data, HDMI_AUDIO_SETTING_1);

	
	hdmi_write(hdmi, 0x40, HDMI_AUDIO_SETTING_2);

	hdmi_write(hdmi, 0x04, HDMI_I2S_AUDIO_SET);

	
	hdmi_write(hdmi, 0x00, HDMI_DSD_AUDIO_SET);

	
	hdmi_write(hdmi, 0x00, HDMI_I2S_INPUT_PIN_SWAP);

	hdmi_write(hdmi, 0x00, HDMI_AUDIO_STATUS_BITS_SETTING_1);

	/*
	 * [7] set value for channel status
	 * [6] set value for channel status
	 * [5] set copyright bit for channel status
	 * [4:2] set additional information for channel status
	 * [1:0] set clock accuracy for channel status
	 */
	hdmi_write(hdmi, 0x00, HDMI_AUDIO_STATUS_BITS_SETTING_2);

	
	hdmi_write(hdmi, 0x00, HDMI_CATEGORY_CODE);

	hdmi_write(hdmi, 0x00, HDMI_SOURCE_NUM_AUDIO_WORD_LEN);

	
	hdmi_write(hdmi, 0x20, HDMI_AUDIO_VIDEO_SETTING_1);
}

static void sh_hdmi_phy_config(struct sh_hdmi *hdmi)
{
	if (hdmi->mode.pixclock < 10000) {
		
		hdmi_write(hdmi, 0x1d, HDMI_SLIPHDMIT_PARAM_SETTINGS_1);
		hdmi_write(hdmi, 0x00, HDMI_SLIPHDMIT_PARAM_SETTINGS_2);
		hdmi_write(hdmi, 0x00, HDMI_SLIPHDMIT_PARAM_SETTINGS_3);
		hdmi_write(hdmi, 0x4c, HDMI_SLIPHDMIT_PARAM_SETTINGS_5);
		hdmi_write(hdmi, 0x1e, HDMI_SLIPHDMIT_PARAM_SETTINGS_6);
		hdmi_write(hdmi, 0x48, HDMI_SLIPHDMIT_PARAM_SETTINGS_7);
		hdmi_write(hdmi, 0x0e, HDMI_SLIPHDMIT_PARAM_SETTINGS_8);
		hdmi_write(hdmi, 0x25, HDMI_SLIPHDMIT_PARAM_SETTINGS_9);
		hdmi_write(hdmi, 0x04, HDMI_SLIPHDMIT_PARAM_SETTINGS_10);
	} else if (hdmi->mode.pixclock < 30000) {
		
		hdmi_write(hdmi, 0x0f, HDMI_SLIPHDMIT_PARAM_SETTINGS_1);
		
		hdmi_write(hdmi, 0x00, HDMI_SLIPHDMIT_PARAM_SETTINGS_2);
		hdmi_write(hdmi, 0x00, HDMI_SLIPHDMIT_PARAM_SETTINGS_3);
		
		hdmi_write(hdmi, 0x44, HDMI_SLIPHDMIT_PARAM_SETTINGS_5);
		hdmi_write(hdmi, 0x32, HDMI_SLIPHDMIT_PARAM_SETTINGS_6);
		
		hdmi_write(hdmi, 0x4A, HDMI_SLIPHDMIT_PARAM_SETTINGS_7);
		hdmi_write(hdmi, 0x00, HDMI_SLIPHDMIT_PARAM_SETTINGS_8);
		
		hdmi_write(hdmi, 0x25, HDMI_SLIPHDMIT_PARAM_SETTINGS_9);
		hdmi_write(hdmi, 0x04, HDMI_SLIPHDMIT_PARAM_SETTINGS_10);
	} else {
		
		hdmi_write(hdmi, 0x19, HDMI_SLIPHDMIT_PARAM_SETTINGS_1);
		hdmi_write(hdmi, 0x00, HDMI_SLIPHDMIT_PARAM_SETTINGS_2);
		hdmi_write(hdmi, 0x00, HDMI_SLIPHDMIT_PARAM_SETTINGS_3);
		hdmi_write(hdmi, 0x44, HDMI_SLIPHDMIT_PARAM_SETTINGS_5);
		hdmi_write(hdmi, 0x32, HDMI_SLIPHDMIT_PARAM_SETTINGS_6);
		hdmi_write(hdmi, 0x48, HDMI_SLIPHDMIT_PARAM_SETTINGS_7);
		hdmi_write(hdmi, 0x0F, HDMI_SLIPHDMIT_PARAM_SETTINGS_8);
		hdmi_write(hdmi, 0x20, HDMI_SLIPHDMIT_PARAM_SETTINGS_9);
		hdmi_write(hdmi, 0x04, HDMI_SLIPHDMIT_PARAM_SETTINGS_10);
	}
}

static void sh_hdmi_avi_infoframe_setup(struct sh_hdmi *hdmi)
{
	u8 vic;

	
	hdmi_write(hdmi, 0x06, HDMI_CTRL_PKT_BUF_INDEX);

	
	hdmi_write(hdmi, 0x82, HDMI_CTRL_PKT_BUF_ACCESS_HB0);

	
	hdmi_write(hdmi, 0x02, HDMI_CTRL_PKT_BUF_ACCESS_HB1);

	
	hdmi_write(hdmi, 0x0D, HDMI_CTRL_PKT_BUF_ACCESS_HB2);

	
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB0);

	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB1);

	hdmi_write(hdmi, 0x28, HDMI_CTRL_PKT_BUF_ACCESS_PB2);

	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB3);

	if (hdmi->preprogrammed_vic)
		vic = hdmi->preprogrammed_vic;
	else
		vic = 4;
	hdmi_write(hdmi, vic, HDMI_CTRL_PKT_BUF_ACCESS_PB4);

	
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB5);

	
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB6);

	
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB7);

	
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB8);

	
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB9);

	
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB10);

	
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB11);

	
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB12);

	
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB13);
}

static void sh_hdmi_audio_infoframe_setup(struct sh_hdmi *hdmi)
{
	
	hdmi_write(hdmi, 0x08, HDMI_CTRL_PKT_BUF_INDEX);

	
	hdmi_write(hdmi, 0x84, HDMI_CTRL_PKT_BUF_ACCESS_HB0);

	
	hdmi_write(hdmi, 0x01, HDMI_CTRL_PKT_BUF_ACCESS_HB1);

	
	hdmi_write(hdmi, 0x0A, HDMI_CTRL_PKT_BUF_ACCESS_HB2);

	
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB0);

	
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB1);

	
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB2);

	
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB3);

	
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB4);

	
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB5);

	
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB6);
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB7);
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB8);
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB9);
	hdmi_write(hdmi, 0x00, HDMI_CTRL_PKT_BUF_ACCESS_PB10);
}

static void sh_hdmi_configure(struct sh_hdmi *hdmi)
{
	
	sh_hdmi_video_config(hdmi);

	
	sh_hdmi_audio_config(hdmi);

	
	sh_hdmi_phy_config(hdmi);

	
	sh_hdmi_avi_infoframe_setup(hdmi);

	
	sh_hdmi_audio_infoframe_setup(hdmi);

	hdmi_write(hdmi, 0x8E, HDMI_CTRL_PKT_AUTO_SEND);

	
	msleep(10);

	
	hdmi_write(hdmi, 0x4C, HDMI_SYSTEM_CTRL);

	udelay(10);

	hdmi_write(hdmi, 0x40, HDMI_SYSTEM_CTRL);
}

static unsigned long sh_hdmi_rate_error(struct sh_hdmi *hdmi,
		const struct fb_videomode *mode,
		unsigned long *hdmi_rate, unsigned long *parent_rate)
{
	unsigned long target = PICOS2KHZ(mode->pixclock) * 1000, rate_error;
	struct sh_mobile_hdmi_info *pdata = hdmi->dev->platform_data;

	*hdmi_rate = clk_round_rate(hdmi->hdmi_clk, target);
	if ((long)*hdmi_rate < 0)
		*hdmi_rate = clk_get_rate(hdmi->hdmi_clk);

	rate_error = (long)*hdmi_rate > 0 ? abs(*hdmi_rate - target) : ULONG_MAX;
	if (rate_error && pdata->clk_optimize_parent)
		rate_error = pdata->clk_optimize_parent(target, hdmi_rate, parent_rate);
	else if (clk_get_parent(hdmi->hdmi_clk))
		*parent_rate = clk_get_rate(clk_get_parent(hdmi->hdmi_clk));

	dev_dbg(hdmi->dev, "%u-%u-%u-%u x %u-%u-%u-%u\n",
		mode->left_margin, mode->xres,
		mode->right_margin, mode->hsync_len,
		mode->upper_margin, mode->yres,
		mode->lower_margin, mode->vsync_len);

	dev_dbg(hdmi->dev, "\t@%lu(+/-%lu)Hz, e=%lu / 1000, r=%uHz, p=%luHz\n", target,
		rate_error, rate_error ? 10000 / (10 * target / rate_error) : 0,
		mode->refresh, *parent_rate);

	return rate_error;
}

static int sh_hdmi_read_edid(struct sh_hdmi *hdmi, unsigned long *hdmi_rate,
			     unsigned long *parent_rate)
{
	struct sh_mobile_lcdc_chan *ch = hdmi->entity.lcdc;
	const struct fb_videomode *mode, *found = NULL;
	unsigned int f_width = 0, f_height = 0, f_refresh = 0;
	unsigned long found_rate_error = ULONG_MAX; 
	bool scanning = false, preferred_bad = false;
	bool use_edid_mode = false;
	u8 edid[128];
	char *forced;
	int i;

	
	dev_dbg(hdmi->dev, "Read back EDID code:");
	for (i = 0; i < 128; i++) {
		edid[i] = hdmi_read(hdmi, HDMI_EDID_KSV_FIFO_ACCESS_WINDOW);
#ifdef DEBUG
		if ((i % 16) == 0) {
			printk(KERN_CONT "\n");
			printk(KERN_DEBUG "%02X | %02X", i, edid[i]);
		} else {
			printk(KERN_CONT " %02X", edid[i]);
		}
#endif
	}
#ifdef DEBUG
	printk(KERN_CONT "\n");
#endif

	if (!hdmi->edid_blocks) {
		fb_edid_to_monspecs(edid, &hdmi->monspec);
		hdmi->edid_blocks = edid[126] + 1;

		dev_dbg(hdmi->dev, "%d main modes, %d extension blocks\n",
			hdmi->monspec.modedb_len, hdmi->edid_blocks - 1);
	} else {
		dev_dbg(hdmi->dev, "Extension %u detected, DTD start %u\n",
			edid[0], edid[2]);
		fb_edid_add_monspecs(edid, &hdmi->monspec);
	}

	if (hdmi->edid_blocks > hdmi->edid_segment_nr * 2 +
	    (hdmi->edid_block_addr >> 7) + 1) {
		
		if (hdmi->edid_block_addr) {
			hdmi->edid_block_addr = 0;
			hdmi->edid_segment_nr++;
		} else {
			hdmi->edid_block_addr = 0x80;
		}
		
		hdmi_write(hdmi, hdmi->edid_block_addr, HDMI_EDID_WORD_ADDRESS);
		
		hdmi_write(hdmi, 0xC6, HDMI_INTERRUPT_MASK_1);
		
		hdmi_write(hdmi, hdmi->edid_segment_nr, HDMI_EDID_SEGMENT_POINTER);
		return -EAGAIN;
	}

	
	dev_dbg(hdmi->dev, "%d main and extended modes\n", hdmi->monspec.modedb_len);

	fb_get_options("sh_mobile_lcdc", &forced);
	if (forced && *forced) {
		
		i = sscanf(forced, "%ux%u@%u",
			   &f_width, &f_height, &f_refresh);
		if (i < 2) {
			f_width = 0;
			f_height = 0;
		} else {
			
			scanning = true;
		}
		dev_dbg(hdmi->dev, "Forced mode %ux%u@%uHz\n",
			f_width, f_height, f_refresh);
	}

	
	for (i = 0, mode = hdmi->monspec.modedb;
	     i < hdmi->monspec.modedb_len && scanning;
	     i++, mode++) {
		unsigned long rate_error;

		if (!f_width && !f_height) {
			if ((mode->flag & FB_MODE_IS_FIRST) || preferred_bad)
				scanning = false;
			else
				continue;
		} else if (f_width != mode->xres || f_height != mode->yres) {
			
			continue;
		}

		rate_error = sh_hdmi_rate_error(hdmi, mode, hdmi_rate, parent_rate);

		if (scanning) {
			if (f_refresh == mode->refresh || (!f_refresh && !rate_error))
				scanning = false;
			else if (found && found_rate_error <= rate_error)
				continue;
		}

		
		if (ch && ch->notify &&
		    ch->notify(ch, SH_MOBILE_LCDC_EVENT_DISPLAY_MODE, mode,
			       NULL)) {
			scanning = true;
			preferred_bad = true;
			continue;
		}

		found = mode;
		found_rate_error = rate_error;
		use_edid_mode = true;
	}

	if (!found && hdmi->entity.def_mode.xres != 0) {
		found = &hdmi->entity.def_mode;
		found_rate_error = sh_hdmi_rate_error(hdmi, found, hdmi_rate,
						      parent_rate);
	}

	
	if (!found)
		return -ENXIO;

	if (found->xres == 640 && found->yres == 480 && found->refresh == 60)
		hdmi->preprogrammed_vic = 1;
	else if (found->xres == 720 && found->yres == 480 && found->refresh == 60)
		hdmi->preprogrammed_vic = 2;
	else if (found->xres == 720 && found->yres == 576 && found->refresh == 50)
		hdmi->preprogrammed_vic = 17;
	else if (found->xres == 1280 && found->yres == 720 && found->refresh == 60)
		hdmi->preprogrammed_vic = 4;
	else if (found->xres == 1920 && found->yres == 1080 && found->refresh == 24)
		hdmi->preprogrammed_vic = 32;
	else if (found->xres == 1920 && found->yres == 1080 && found->refresh == 50)
		hdmi->preprogrammed_vic = 31;
	else if (found->xres == 1920 && found->yres == 1080 && found->refresh == 60)
		hdmi->preprogrammed_vic = 16;
	else
		hdmi->preprogrammed_vic = 0;

	dev_dbg(hdmi->dev, "Using %s %s mode %ux%u@%uHz (%luHz), "
		"clock error %luHz\n", use_edid_mode ? "EDID" : "default",
		hdmi->preprogrammed_vic ? "VIC" : "external", found->xres,
		found->yres, found->refresh, PICOS2KHZ(found->pixclock) * 1000,
		found_rate_error);

	hdmi->mode = *found;
	sh_hdmi_external_video_param(hdmi);

	return 0;
}

static irqreturn_t sh_hdmi_hotplug(int irq, void *dev_id)
{
	struct sh_hdmi *hdmi = dev_id;
	u8 status1, status2, mask1, mask2;

	
	hdmi_write(hdmi, 0x2C, HDMI_SYSTEM_CTRL);

	
	udelay(10);

	
	hdmi_write(hdmi, 0x20, HDMI_SYSTEM_CTRL);

	status1 = hdmi_read(hdmi, HDMI_INTERRUPT_STATUS_1);
	status2 = hdmi_read(hdmi, HDMI_INTERRUPT_STATUS_2);

	mask1 = hdmi_read(hdmi, HDMI_INTERRUPT_MASK_1);
	mask2 = hdmi_read(hdmi, HDMI_INTERRUPT_MASK_2);

	
	hdmi_write(hdmi, 0xFF, HDMI_INTERRUPT_STATUS_1);
	hdmi_write(hdmi, 0xFF, HDMI_INTERRUPT_STATUS_2);

	if (printk_ratelimit())
		dev_dbg(hdmi->dev, "IRQ #%d: Status #1: 0x%x & 0x%x, #2: 0x%x & 0x%x\n",
			irq, status1, mask1, status2, mask2);

	if (!((status1 & mask1) | (status2 & mask2))) {
		return IRQ_NONE;
	} else if (status1 & 0xc0) {
		u8 msens;

		
		udelay(500);

		msens = hdmi_read(hdmi, HDMI_HOT_PLUG_MSENS_STATUS);
		dev_dbg(hdmi->dev, "MSENS 0x%x\n", msens);
		
		if ((msens & 0xC0) == 0xC0) {
			
			hdmi->edid_segment_nr = 0;
			hdmi->edid_block_addr = 0;
			hdmi->edid_blocks = 0;
			hdmi->hp_state = HDMI_HOTPLUG_CONNECTED;

			
			hdmi_write(hdmi, 0x00, HDMI_EDID_WORD_ADDRESS);
			
			hdmi_write(hdmi, 0xC6, HDMI_INTERRUPT_MASK_1);
			
			hdmi_write(hdmi, 0x00, HDMI_EDID_SEGMENT_POINTER);
		} else if (!(status1 & 0x80)) {
			
			if (hdmi->hp_state != HDMI_HOTPLUG_DISCONNECTED) {
				hdmi->hp_state = HDMI_HOTPLUG_DISCONNECTED;
				schedule_delayed_work(&hdmi->edid_work, 0);
			}
			
		}
	} else if (status1 & 2) {
		
		
		hdmi_write(hdmi, hdmi->edid_block_addr, HDMI_EDID_WORD_ADDRESS);
		
		hdmi_write(hdmi, hdmi->edid_segment_nr, HDMI_EDID_SEGMENT_POINTER);
	} else if (status1 & 4) {
		
		hdmi_write(hdmi, 0xC0, HDMI_INTERRUPT_MASK_1);
		schedule_delayed_work(&hdmi->edid_work, msecs_to_jiffies(10));
	}

	return IRQ_HANDLED;
}

static int sh_hdmi_display_on(struct sh_mobile_lcdc_entity *entity)
{
	struct sh_hdmi *hdmi = entity_to_sh_hdmi(entity);

	dev_dbg(hdmi->dev, "%s(%p): state %x\n", __func__, hdmi,
		hdmi->hp_state);

	if (hdmi->hp_state == HDMI_HOTPLUG_EDID_DONE) {
		
		hdmi_write(hdmi, 0x80, HDMI_SYSTEM_CTRL);
		dev_dbg(hdmi->dev, "HDMI running\n");
	}

	return hdmi->hp_state == HDMI_HOTPLUG_DISCONNECTED
		? SH_MOBILE_LCDC_DISPLAY_DISCONNECTED
		: SH_MOBILE_LCDC_DISPLAY_CONNECTED;
}

static void sh_hdmi_display_off(struct sh_mobile_lcdc_entity *entity)
{
	struct sh_hdmi *hdmi = entity_to_sh_hdmi(entity);

	dev_dbg(hdmi->dev, "%s(%p)\n", __func__, hdmi);
	
	hdmi_write(hdmi, 0x10, HDMI_SYSTEM_CTRL);
}

static const struct sh_mobile_lcdc_entity_ops sh_hdmi_ops = {
	.display_on = sh_hdmi_display_on,
	.display_off = sh_hdmi_display_off,
};

static long sh_hdmi_clk_configure(struct sh_hdmi *hdmi, unsigned long hdmi_rate,
				  unsigned long parent_rate)
{
	int ret;

	if (parent_rate && clk_get_parent(hdmi->hdmi_clk)) {
		ret = clk_set_rate(clk_get_parent(hdmi->hdmi_clk), parent_rate);
		if (ret < 0) {
			dev_warn(hdmi->dev, "Cannot set parent rate %ld: %d\n", parent_rate, ret);
			hdmi_rate = clk_round_rate(hdmi->hdmi_clk, hdmi_rate);
		} else {
			dev_dbg(hdmi->dev, "HDMI set parent frequency %lu\n", parent_rate);
		}
	}

	ret = clk_set_rate(hdmi->hdmi_clk, hdmi_rate);
	if (ret < 0) {
		dev_warn(hdmi->dev, "Cannot set rate %ld: %d\n", hdmi_rate, ret);
		hdmi_rate = 0;
	} else {
		dev_dbg(hdmi->dev, "HDMI set frequency %lu\n", hdmi_rate);
	}

	return hdmi_rate;
}

static void sh_hdmi_edid_work_fn(struct work_struct *work)
{
	struct sh_hdmi *hdmi = container_of(work, struct sh_hdmi, edid_work.work);
	struct sh_mobile_lcdc_chan *ch = hdmi->entity.lcdc;
	int ret;

	dev_dbg(hdmi->dev, "%s(%p): begin, hotplug status %d\n", __func__, hdmi,
		hdmi->hp_state);

	if (hdmi->hp_state == HDMI_HOTPLUG_CONNECTED) {
		unsigned long parent_rate = 0, hdmi_rate;

		ret = sh_hdmi_read_edid(hdmi, &hdmi_rate, &parent_rate);
		if (ret < 0)
			goto out;

		hdmi->hp_state = HDMI_HOTPLUG_EDID_DONE;

		
		ret = sh_hdmi_clk_configure(hdmi, hdmi_rate, parent_rate);
		if (ret < 0)
			goto out;

		msleep(10);
		sh_hdmi_configure(hdmi);
		
		msleep(10);

		if (ch && ch->notify)
			ch->notify(ch, SH_MOBILE_LCDC_EVENT_DISPLAY_CONNECT,
				   &hdmi->mode, &hdmi->monspec);
	} else {
		hdmi->monspec.modedb_len = 0;
		fb_destroy_modedb(hdmi->monspec.modedb);
		hdmi->monspec.modedb = NULL;

		if (ch && ch->notify)
			ch->notify(ch, SH_MOBILE_LCDC_EVENT_DISPLAY_DISCONNECT,
				   NULL, NULL);

		ret = 0;
	}

out:
	if (ret < 0 && ret != -EAGAIN)
		hdmi->hp_state = HDMI_HOTPLUG_DISCONNECTED;

	dev_dbg(hdmi->dev, "%s(%p): end\n", __func__, hdmi);
}

static int __init sh_hdmi_probe(struct platform_device *pdev)
{
	struct sh_mobile_hdmi_info *pdata = pdev->dev.platform_data;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	int irq = platform_get_irq(pdev, 0), ret;
	struct sh_hdmi *hdmi;
	long rate;

	if (!res || !pdata || irq < 0)
		return -ENODEV;

	hdmi = kzalloc(sizeof(*hdmi), GFP_KERNEL);
	if (!hdmi) {
		dev_err(&pdev->dev, "Cannot allocate device data\n");
		return -ENOMEM;
	}

	hdmi->dev = &pdev->dev;
	hdmi->entity.owner = THIS_MODULE;
	hdmi->entity.ops = &sh_hdmi_ops;

	hdmi->hdmi_clk = clk_get(&pdev->dev, "ick");
	if (IS_ERR(hdmi->hdmi_clk)) {
		ret = PTR_ERR(hdmi->hdmi_clk);
		dev_err(&pdev->dev, "Unable to get clock: %d\n", ret);
		goto egetclk;
	}

	
	rate = clk_round_rate(hdmi->hdmi_clk, PICOS2KHZ(37037));
	if (rate > 0)
		rate = sh_hdmi_clk_configure(hdmi, rate, 0);

	if (rate < 0) {
		ret = rate;
		goto erate;
	}

	ret = clk_enable(hdmi->hdmi_clk);
	if (ret < 0) {
		dev_err(hdmi->dev, "Cannot enable clock: %d\n", ret);
		goto erate;
	}

	dev_dbg(&pdev->dev, "Enabled HDMI clock at %luHz\n", rate);

	if (!request_mem_region(res->start, resource_size(res), dev_name(&pdev->dev))) {
		dev_err(&pdev->dev, "HDMI register region already claimed\n");
		ret = -EBUSY;
		goto ereqreg;
	}

	hdmi->base = ioremap(res->start, resource_size(res));
	if (!hdmi->base) {
		dev_err(&pdev->dev, "HDMI register region already claimed\n");
		ret = -ENOMEM;
		goto emap;
	}

	platform_set_drvdata(pdev, &hdmi->entity);

	INIT_DELAYED_WORK(&hdmi->edid_work, sh_hdmi_edid_work_fn);

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	
	dev_info(&pdev->dev, "Detected HDMI controller 0x%x:0x%x\n",
		 hdmi_read(hdmi, HDMI_PRODUCT_ID), hdmi_read(hdmi, HDMI_REVISION_ID));

	ret = request_irq(irq, sh_hdmi_hotplug, 0,
			  dev_name(&pdev->dev), hdmi);
	if (ret < 0) {
		dev_err(&pdev->dev, "Unable to request irq: %d\n", ret);
		goto ereqirq;
	}

	ret = snd_soc_register_codec(&pdev->dev,
			&soc_codec_dev_sh_hdmi, &sh_hdmi_dai, 1);
	if (ret < 0) {
		dev_err(&pdev->dev, "codec registration failed\n");
		goto ecodec;
	}

	return 0;

ecodec:
	free_irq(irq, hdmi);
ereqirq:
	pm_runtime_put(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	iounmap(hdmi->base);
emap:
	release_mem_region(res->start, resource_size(res));
ereqreg:
	clk_disable(hdmi->hdmi_clk);
erate:
	clk_put(hdmi->hdmi_clk);
egetclk:
	kfree(hdmi);

	return ret;
}

static int __exit sh_hdmi_remove(struct platform_device *pdev)
{
	struct sh_hdmi *hdmi = entity_to_sh_hdmi(platform_get_drvdata(pdev));
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	int irq = platform_get_irq(pdev, 0);

	snd_soc_unregister_codec(&pdev->dev);

	
	free_irq(irq, hdmi);
	
	cancel_delayed_work_sync(&hdmi->edid_work);
	pm_runtime_put(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	clk_disable(hdmi->hdmi_clk);
	clk_put(hdmi->hdmi_clk);
	iounmap(hdmi->base);
	release_mem_region(res->start, resource_size(res));
	kfree(hdmi);

	return 0;
}

static struct platform_driver sh_hdmi_driver = {
	.remove		= __exit_p(sh_hdmi_remove),
	.driver = {
		.name	= "sh-mobile-hdmi",
	},
};

static int __init sh_hdmi_init(void)
{
	return platform_driver_probe(&sh_hdmi_driver, sh_hdmi_probe);
}
module_init(sh_hdmi_init);

static void __exit sh_hdmi_exit(void)
{
	platform_driver_unregister(&sh_hdmi_driver);
}
module_exit(sh_hdmi_exit);

MODULE_AUTHOR("Guennadi Liakhovetski <g.liakhovetski@gmx.de>");
MODULE_DESCRIPTION("SuperH / ARM-shmobile HDMI driver");
MODULE_LICENSE("GPL v2");
