/*
 * wm8961.c  --  WM8961 ALSA SoC Audio driver
 *
 * Author: Mark Brown
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Currently unimplemented features:
 *  - ALC
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "wm8961.h"

#define WM8961_MAX_REGISTER                     0xFC

static u16 wm8961_reg_defaults[] = {
	0x009F,     
	0x009F,     
	0x0000,     
	0x0000,     
	0x0020,     
	0x0008,     
	0x0000,     
	0x000A,     
	0x01F4,     
	0x0000,     
	0x00FF,     
	0x00FF,     
	0x0000,     
	0x0000,     
	0x0040,     
	0x0000,     
	0x0000,     
	0x007B,     
	0x0000,     
	0x0032,     
	0x0000,     
	0x00C0,     
	0x00C0,     
	0x0120,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x005F,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0023,     
	0x0000,     
	0x0000,     
	0x0003,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0106,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x015E,     
	0x0010,     
	0x0010,     
	0x0000,     
	0x0001,     
	0x0003,     
	0x0000,     
	0x0060,     
	0x01FB,     
	0x0000,     
	0x0065,     
	0x005F,     
	0x0059,     
	0x006B,     
	0x0038,     
	0x000C,     
	0x000A,     
	0x006B,     
	0x0000,     
	0x0000,     
	0x0087,     
	0x0000,     
	0x005C,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0030,     
	0x0006,     
	0x0000,     
	0x0060,     
	0x0000,     
	0x003F,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0001,     
	0x0000,     
	0x0181,     
	0x0005,     
	0x0008,     
	0x0008,     
	0x0000,     
	0x013B,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0070,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0003,     
	0x0000,     
	0x0000,     
	0x0001,     
	0x0008,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0004,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0080,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0000,     
	0x0052,     
	0x0110,     
	0x0040,     
	0x0000,     
	0x0030,     
	0x0000,     
	0x0000,     
	0x0001,     
};

struct wm8961_priv {
	enum snd_soc_control_type control_type;
	int sysclk;
};

static int wm8961_volatile_register(struct snd_soc_codec *codec, unsigned int reg)
{
	switch (reg) {
	case WM8961_SOFTWARE_RESET:
	case WM8961_WRITE_SEQUENCER_7:
	case WM8961_DC_SERVO_1:
		return 1;

	default:
		return 0;
	}
}

static int wm8961_reset(struct snd_soc_codec *codec)
{
	return snd_soc_write(codec, WM8961_SOFTWARE_RESET, 0);
}

static int wm8961_hp_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 hp_reg = snd_soc_read(codec, WM8961_ANALOGUE_HP_0);
	u16 cp_reg = snd_soc_read(codec, WM8961_CHARGE_PUMP_1);
	u16 pwr_reg = snd_soc_read(codec, WM8961_PWR_MGMT_2);
	u16 dcs_reg = snd_soc_read(codec, WM8961_DC_SERVO_1);
	int timeout = 500;

	if (event & SND_SOC_DAPM_POST_PMU) {
		
		hp_reg &= ~(WM8961_HPR_RMV_SHORT | WM8961_HPL_RMV_SHORT);
		snd_soc_write(codec, WM8961_ANALOGUE_HP_0, hp_reg);

		
		cp_reg |= WM8961_CP_ENA;
		snd_soc_write(codec, WM8961_CHARGE_PUMP_1, cp_reg);
		mdelay(5);

		
		pwr_reg |= WM8961_LOUT1_PGA | WM8961_ROUT1_PGA;
		snd_soc_write(codec, WM8961_PWR_MGMT_2, pwr_reg);

		
		hp_reg |= WM8961_HPR_ENA | WM8961_HPL_ENA;
		snd_soc_write(codec, WM8961_ANALOGUE_HP_0, hp_reg);

		
		hp_reg |= WM8961_HPR_ENA_DLY | WM8961_HPL_ENA_DLY;
		snd_soc_write(codec, WM8961_ANALOGUE_HP_0, hp_reg);

		
		dcs_reg |=
			WM8961_DCS_ENA_CHAN_HPR | WM8961_DCS_TRIG_STARTUP_HPR |
			WM8961_DCS_ENA_CHAN_HPL | WM8961_DCS_TRIG_STARTUP_HPL;
		dev_dbg(codec->dev, "Enabling DC servo\n");

		snd_soc_write(codec, WM8961_DC_SERVO_1, dcs_reg);
		do {
			msleep(1);
			dcs_reg = snd_soc_read(codec, WM8961_DC_SERVO_1);
		} while (--timeout &&
			 dcs_reg & (WM8961_DCS_TRIG_STARTUP_HPR |
				WM8961_DCS_TRIG_STARTUP_HPL));
		if (dcs_reg & (WM8961_DCS_TRIG_STARTUP_HPR |
			       WM8961_DCS_TRIG_STARTUP_HPL))
			dev_err(codec->dev, "DC servo timed out\n");
		else
			dev_dbg(codec->dev, "DC servo startup complete\n");

		
		hp_reg |= WM8961_HPR_ENA_OUTP | WM8961_HPL_ENA_OUTP;
		snd_soc_write(codec, WM8961_ANALOGUE_HP_0, hp_reg);

		
		hp_reg |= WM8961_HPR_RMV_SHORT | WM8961_HPL_RMV_SHORT;
		snd_soc_write(codec, WM8961_ANALOGUE_HP_0, hp_reg);
	}

	if (event & SND_SOC_DAPM_PRE_PMD) {
		
		hp_reg &= ~(WM8961_HPR_RMV_SHORT | WM8961_HPL_RMV_SHORT);
		snd_soc_write(codec, WM8961_ANALOGUE_HP_0, hp_reg);

		
		hp_reg &= ~(WM8961_HPR_ENA_OUTP | WM8961_HPL_ENA_OUTP);
		snd_soc_write(codec, WM8961_ANALOGUE_HP_0, hp_reg);

		
		dcs_reg &= ~(WM8961_DCS_ENA_CHAN_HPR |
			     WM8961_DCS_ENA_CHAN_HPL);
		snd_soc_write(codec, WM8961_DC_SERVO_1, dcs_reg);

		
		hp_reg &= ~(WM8961_HPR_ENA_DLY | WM8961_HPR_ENA |
			    WM8961_HPL_ENA_DLY | WM8961_HPL_ENA);
		snd_soc_write(codec, WM8961_ANALOGUE_HP_0, hp_reg);

		
		pwr_reg &= ~(WM8961_LOUT1_PGA | WM8961_ROUT1_PGA);
		snd_soc_write(codec, WM8961_PWR_MGMT_2, pwr_reg);

		
		dev_dbg(codec->dev, "Disabling charge pump\n");
		snd_soc_write(codec, WM8961_CHARGE_PUMP_1,
			     cp_reg & ~WM8961_CP_ENA);
	}

	return 0;
}

static int wm8961_spk_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 pwr_reg = snd_soc_read(codec, WM8961_PWR_MGMT_2);
	u16 spk_reg = snd_soc_read(codec, WM8961_CLASS_D_CONTROL_1);

	if (event & SND_SOC_DAPM_POST_PMU) {
		
		pwr_reg |= WM8961_SPKL_PGA | WM8961_SPKR_PGA;
		snd_soc_write(codec, WM8961_PWR_MGMT_2, pwr_reg);

		
		spk_reg |= WM8961_SPKL_ENA | WM8961_SPKR_ENA;
		snd_soc_write(codec, WM8961_CLASS_D_CONTROL_1, spk_reg);
	}

	if (event & SND_SOC_DAPM_PRE_PMD) {
		
		spk_reg &= ~(WM8961_SPKL_ENA | WM8961_SPKR_ENA);
		snd_soc_write(codec, WM8961_CLASS_D_CONTROL_1, spk_reg);

		
		pwr_reg &= ~(WM8961_SPKL_PGA | WM8961_SPKR_PGA);
		snd_soc_write(codec, WM8961_PWR_MGMT_2, pwr_reg);
	}

	return 0;
}

static const char *adc_hpf_text[] = {
	"Hi-fi", "Voice 1", "Voice 2", "Voice 3",
};

static const struct soc_enum adc_hpf =
	SOC_ENUM_SINGLE(WM8961_ADC_DAC_CONTROL_2, 7, 4, adc_hpf_text);

static const char *dac_deemph_text[] = {
	"None", "32kHz", "44.1kHz", "48kHz",
};

static const struct soc_enum dac_deemph =
	SOC_ENUM_SINGLE(WM8961_ADC_DAC_CONTROL_1, 1, 4, dac_deemph_text);

static const DECLARE_TLV_DB_SCALE(out_tlv, -12100, 100, 1);
static const DECLARE_TLV_DB_SCALE(hp_sec_tlv, -700, 100, 0);
static const DECLARE_TLV_DB_SCALE(adc_tlv, -7200, 75, 1);
static const DECLARE_TLV_DB_SCALE(sidetone_tlv, -3600, 300, 0);
static unsigned int boost_tlv[] = {
	TLV_DB_RANGE_HEAD(4),
	0, 0, TLV_DB_SCALE_ITEM(0,  0, 0),
	1, 1, TLV_DB_SCALE_ITEM(13, 0, 0),
	2, 2, TLV_DB_SCALE_ITEM(20, 0, 0),
	3, 3, TLV_DB_SCALE_ITEM(29, 0, 0),
};
static const DECLARE_TLV_DB_SCALE(pga_tlv, -2325, 75, 0);

static const struct snd_kcontrol_new wm8961_snd_controls[] = {
SOC_DOUBLE_R_TLV("Headphone Volume", WM8961_LOUT1_VOLUME, WM8961_ROUT1_VOLUME,
		 0, 127, 0, out_tlv),
SOC_DOUBLE_TLV("Headphone Secondary Volume", WM8961_ANALOGUE_HP_2,
	       6, 3, 7, 0, hp_sec_tlv),
SOC_DOUBLE_R("Headphone ZC Switch", WM8961_LOUT1_VOLUME, WM8961_ROUT1_VOLUME,
	     7, 1, 0),

SOC_DOUBLE_R_TLV("Speaker Volume", WM8961_LOUT2_VOLUME, WM8961_ROUT2_VOLUME,
		 0, 127, 0, out_tlv),
SOC_DOUBLE_R("Speaker ZC Switch", WM8961_LOUT2_VOLUME, WM8961_ROUT2_VOLUME,
	   7, 1, 0),
SOC_SINGLE("Speaker AC Gain", WM8961_CLASS_D_CONTROL_2, 0, 7, 0),

SOC_SINGLE("DAC x128 OSR Switch", WM8961_ADC_DAC_CONTROL_2, 0, 1, 0),
SOC_ENUM("DAC Deemphasis", dac_deemph),
SOC_SINGLE("DAC Soft Mute Switch", WM8961_ADC_DAC_CONTROL_2, 3, 1, 0),

SOC_DOUBLE_R_TLV("Sidetone Volume", WM8961_DSP_SIDETONE_0,
		 WM8961_DSP_SIDETONE_1, 4, 12, 0, sidetone_tlv),

SOC_SINGLE("ADC High Pass Filter Switch", WM8961_ADC_DAC_CONTROL_1, 0, 1, 0),
SOC_ENUM("ADC High Pass Filter Mode", adc_hpf),

SOC_DOUBLE_R_TLV("Capture Volume",
		 WM8961_LEFT_ADC_VOLUME, WM8961_RIGHT_ADC_VOLUME,
		 1, 119, 0, adc_tlv),
SOC_DOUBLE_R_TLV("Capture Boost Volume",
		 WM8961_ADCL_SIGNAL_PATH, WM8961_ADCR_SIGNAL_PATH,
		 4, 3, 0, boost_tlv),
SOC_DOUBLE_R_TLV("Capture PGA Volume",
		 WM8961_LEFT_INPUT_VOLUME, WM8961_RIGHT_INPUT_VOLUME,
		 0, 62, 0, pga_tlv),
SOC_DOUBLE_R("Capture PGA ZC Switch",
	     WM8961_LEFT_INPUT_VOLUME, WM8961_RIGHT_INPUT_VOLUME,
	     6, 1, 1),
SOC_DOUBLE_R("Capture PGA Switch",
	     WM8961_LEFT_INPUT_VOLUME, WM8961_RIGHT_INPUT_VOLUME,
	     7, 1, 1),
};

static const char *sidetone_text[] = {
	"None", "Left", "Right"
};

static const struct soc_enum dacl_sidetone =
	SOC_ENUM_SINGLE(WM8961_DSP_SIDETONE_0, 2, 3, sidetone_text);

static const struct soc_enum dacr_sidetone =
	SOC_ENUM_SINGLE(WM8961_DSP_SIDETONE_1, 2, 3, sidetone_text);

static const struct snd_kcontrol_new dacl_mux =
	SOC_DAPM_ENUM("DACL Sidetone", dacl_sidetone);

static const struct snd_kcontrol_new dacr_mux =
	SOC_DAPM_ENUM("DACR Sidetone", dacr_sidetone);

static const struct snd_soc_dapm_widget wm8961_dapm_widgets[] = {
SND_SOC_DAPM_INPUT("LINPUT"),
SND_SOC_DAPM_INPUT("RINPUT"),

SND_SOC_DAPM_SUPPLY("CLK_DSP", WM8961_CLOCKING2, 4, 0, NULL, 0),

SND_SOC_DAPM_PGA("Left Input", WM8961_PWR_MGMT_1, 5, 0, NULL, 0),
SND_SOC_DAPM_PGA("Right Input", WM8961_PWR_MGMT_1, 4, 0, NULL, 0),

SND_SOC_DAPM_ADC("ADCL", "HiFi Capture", WM8961_PWR_MGMT_1, 3, 0),
SND_SOC_DAPM_ADC("ADCR", "HiFi Capture", WM8961_PWR_MGMT_1, 2, 0),

SND_SOC_DAPM_SUPPLY("MICBIAS", WM8961_PWR_MGMT_1, 1, 0, NULL, 0),

SND_SOC_DAPM_MUX("DACL Sidetone", SND_SOC_NOPM, 0, 0, &dacl_mux),
SND_SOC_DAPM_MUX("DACR Sidetone", SND_SOC_NOPM, 0, 0, &dacr_mux),

SND_SOC_DAPM_DAC("DACL", "HiFi Playback", WM8961_PWR_MGMT_2, 8, 0),
SND_SOC_DAPM_DAC("DACR", "HiFi Playback", WM8961_PWR_MGMT_2, 7, 0),

SND_SOC_DAPM_PGA_E("Headphone Output", SND_SOC_NOPM,
		   4, 0, NULL, 0, wm8961_hp_event,
		   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_PGA_E("Speaker Output", SND_SOC_NOPM,
		   4, 0, NULL, 0, wm8961_spk_event,
		   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

SND_SOC_DAPM_OUTPUT("HP_L"),
SND_SOC_DAPM_OUTPUT("HP_R"),
SND_SOC_DAPM_OUTPUT("SPK_LN"),
SND_SOC_DAPM_OUTPUT("SPK_LP"),
SND_SOC_DAPM_OUTPUT("SPK_RN"),
SND_SOC_DAPM_OUTPUT("SPK_RP"),
};


static const struct snd_soc_dapm_route audio_paths[] = {
	{ "DACL", NULL, "CLK_DSP" },
	{ "DACL", NULL, "DACL Sidetone" },
	{ "DACR", NULL, "CLK_DSP" },
	{ "DACR", NULL, "DACR Sidetone" },

	{ "DACL Sidetone", "Left", "ADCL" },
	{ "DACL Sidetone", "Right", "ADCR" },

	{ "DACR Sidetone", "Left", "ADCL" },
	{ "DACR Sidetone", "Right", "ADCR" },

	{ "HP_L", NULL, "Headphone Output" },
	{ "HP_R", NULL, "Headphone Output" },
	{ "Headphone Output", NULL, "DACL" },
	{ "Headphone Output", NULL, "DACR" },

	{ "SPK_LN", NULL, "Speaker Output" },
	{ "SPK_LP", NULL, "Speaker Output" },
	{ "SPK_RN", NULL, "Speaker Output" },
	{ "SPK_RP", NULL, "Speaker Output" },

	{ "Speaker Output", NULL, "DACL" },
	{ "Speaker Output", NULL, "DACR" },

	{ "ADCL", NULL, "Left Input" },
	{ "ADCL", NULL, "CLK_DSP" },
	{ "ADCR", NULL, "Right Input" },
	{ "ADCR", NULL, "CLK_DSP" },

	{ "Left Input", NULL, "LINPUT" },
	{ "Right Input", NULL, "RINPUT" },

};

static struct {
	int ratio;
	u16 val;
} wm8961_clk_sys_ratio[] = {
	{  64,  0 },
	{  128, 1 },
	{  192, 2 },
	{  256, 3 },
	{  384, 4 },
	{  512, 5 },
	{  768, 6 },
	{ 1024, 7 },
	{ 1408, 8 },
	{ 1536, 9 },
};

static struct {
	int rate;
	u16 val;
} wm8961_srate[] = {
	{ 48000, 0 },
	{ 44100, 0 },
	{ 32000, 1 },
	{ 22050, 2 },
	{ 24000, 2 },
	{ 16000, 3 },
	{ 11250, 4 },
	{ 12000, 4 },
	{  8000, 5 },
};

static int wm8961_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct wm8961_priv *wm8961 = snd_soc_codec_get_drvdata(codec);
	int i, best, target, fs;
	u16 reg;

	fs = params_rate(params);

	if (!wm8961->sysclk) {
		dev_err(codec->dev, "MCLK has not been specified\n");
		return -EINVAL;
	}

	
	best = 0;
	for (i = 0; i < ARRAY_SIZE(wm8961_srate); i++) {
		if (abs(wm8961_srate[i].rate - fs) <
		    abs(wm8961_srate[best].rate - fs))
			best = i;
	}
	reg = snd_soc_read(codec, WM8961_ADDITIONAL_CONTROL_3);
	reg &= ~WM8961_SAMPLE_RATE_MASK;
	reg |= wm8961_srate[best].val;
	snd_soc_write(codec, WM8961_ADDITIONAL_CONTROL_3, reg);
	dev_dbg(codec->dev, "Selected SRATE %dHz for %dHz\n",
		wm8961_srate[best].rate, fs);

	
	target = wm8961->sysclk / fs;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK && target < 64) {
		dev_err(codec->dev,
			"SYSCLK must be at least 64*fs for DAC\n");
		return -EINVAL;
	}
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE && target < 256) {
		dev_err(codec->dev,
			"SYSCLK must be at least 256*fs for ADC\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(wm8961_clk_sys_ratio); i++) {
		if (wm8961_clk_sys_ratio[i].ratio >= target)
			break;
	}
	if (i == ARRAY_SIZE(wm8961_clk_sys_ratio)) {
		dev_err(codec->dev, "Unable to generate CLK_SYS_RATE\n");
		return -EINVAL;
	}
	dev_dbg(codec->dev, "Selected CLK_SYS_RATE of %d for %d/%d=%d\n",
		wm8961_clk_sys_ratio[i].ratio, wm8961->sysclk, fs,
		wm8961->sysclk / fs);

	reg = snd_soc_read(codec, WM8961_CLOCKING_4);
	reg &= ~WM8961_CLK_SYS_RATE_MASK;
	reg |= wm8961_clk_sys_ratio[i].val << WM8961_CLK_SYS_RATE_SHIFT;
	snd_soc_write(codec, WM8961_CLOCKING_4, reg);

	reg = snd_soc_read(codec, WM8961_AUDIO_INTERFACE_0);
	reg &= ~WM8961_WL_MASK;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		reg |= 1 << WM8961_WL_SHIFT;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		reg |= 2 << WM8961_WL_SHIFT;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		reg |= 3 << WM8961_WL_SHIFT;
		break;
	default:
		return -EINVAL;
	}
	snd_soc_write(codec, WM8961_AUDIO_INTERFACE_0, reg);

	
	reg = snd_soc_read(codec, WM8961_ADC_DAC_CONTROL_2);
	if (fs <= 24000)
		reg |= WM8961_DACSLOPE;
	else
		reg &= ~WM8961_DACSLOPE;
	snd_soc_write(codec, WM8961_ADC_DAC_CONTROL_2, reg);

	return 0;
}

static int wm8961_set_sysclk(struct snd_soc_dai *dai, int clk_id,
			     unsigned int freq,
			     int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct wm8961_priv *wm8961 = snd_soc_codec_get_drvdata(codec);
	u16 reg = snd_soc_read(codec, WM8961_CLOCKING1);

	if (freq > 33000000) {
		dev_err(codec->dev, "MCLK must be <33MHz\n");
		return -EINVAL;
	}

	if (freq > 16500000) {
		dev_dbg(codec->dev, "Using MCLK/2 for %dHz MCLK\n", freq);
		reg |= WM8961_MCLKDIV;
		freq /= 2;
	} else {
		dev_dbg(codec->dev, "Using MCLK/1 for %dHz MCLK\n", freq);
		reg &= ~WM8961_MCLKDIV;
	}

	snd_soc_write(codec, WM8961_CLOCKING1, reg);

	wm8961->sysclk = freq;

	return 0;
}

static int wm8961_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 aif = snd_soc_read(codec, WM8961_AUDIO_INTERFACE_0);

	aif &= ~(WM8961_BCLKINV | WM8961_LRP |
		 WM8961_MS | WM8961_FORMAT_MASK);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		aif |= WM8961_MS;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_RIGHT_J:
		break;

	case SND_SOC_DAIFMT_LEFT_J:
		aif |= 1;
		break;

	case SND_SOC_DAIFMT_I2S:
		aif |= 2;
		break;

	case SND_SOC_DAIFMT_DSP_B:
		aif |= WM8961_LRP;
	case SND_SOC_DAIFMT_DSP_A:
		aif |= 3;
		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
		case SND_SOC_DAIFMT_IB_NF:
			break;
		default:
			return -EINVAL;
		}
		break;

	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_NB_IF:
		aif |= WM8961_LRP;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		aif |= WM8961_BCLKINV;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		aif |= WM8961_BCLKINV | WM8961_LRP;
		break;
	default:
		return -EINVAL;
	}

	return snd_soc_write(codec, WM8961_AUDIO_INTERFACE_0, aif);
}

static int wm8961_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 reg = snd_soc_read(codec, WM8961_ADDITIONAL_CONTROL_2);

	if (tristate)
		reg |= WM8961_TRIS;
	else
		reg &= ~WM8961_TRIS;

	return snd_soc_write(codec, WM8961_ADDITIONAL_CONTROL_2, reg);
}

static int wm8961_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 reg = snd_soc_read(codec, WM8961_ADC_DAC_CONTROL_1);

	if (mute)
		reg |= WM8961_DACMU;
	else
		reg &= ~WM8961_DACMU;

	msleep(17);

	return snd_soc_write(codec, WM8961_ADC_DAC_CONTROL_1, reg);
}

static int wm8961_set_clkdiv(struct snd_soc_dai *dai, int div_id, int div)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 reg;

	switch (div_id) {
	case WM8961_BCLK:
		reg = snd_soc_read(codec, WM8961_CLOCKING2);
		reg &= ~WM8961_BCLKDIV_MASK;
		reg |= div;
		snd_soc_write(codec, WM8961_CLOCKING2, reg);
		break;

	case WM8961_LRCLK:
		reg = snd_soc_read(codec, WM8961_AUDIO_INTERFACE_2);
		reg &= ~WM8961_LRCLK_RATE_MASK;
		reg |= div;
		snd_soc_write(codec, WM8961_AUDIO_INTERFACE_2, reg);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int wm8961_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	u16 reg;

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		if (codec->dapm.bias_level == SND_SOC_BIAS_STANDBY) {
			
			reg = snd_soc_read(codec, WM8961_ANTI_POP);
			reg |= WM8961_BUFIOEN | WM8961_BUFDCOPEN;
			snd_soc_write(codec, WM8961_ANTI_POP, reg);

			
			reg = snd_soc_read(codec, WM8961_PWR_MGMT_1);
			reg &= ~WM8961_VMIDSEL_MASK;
			reg |= (1 << WM8961_VMIDSEL_SHIFT) | WM8961_VREF;
			snd_soc_write(codec, WM8961_PWR_MGMT_1, reg);
		}
		break;

	case SND_SOC_BIAS_STANDBY:
		if (codec->dapm.bias_level == SND_SOC_BIAS_PREPARE) {
			
			reg = snd_soc_read(codec, WM8961_PWR_MGMT_1);
			reg &= ~WM8961_VREF;
			snd_soc_write(codec, WM8961_PWR_MGMT_1, reg);

			
			reg = snd_soc_read(codec, WM8961_ANTI_POP);
			reg &= ~(WM8961_BUFIOEN | WM8961_BUFDCOPEN);
			snd_soc_write(codec, WM8961_ANTI_POP, reg);

			
			reg = snd_soc_read(codec, WM8961_PWR_MGMT_1);
			reg &= ~WM8961_VMIDSEL_MASK;
			snd_soc_write(codec, WM8961_PWR_MGMT_1, reg);
		}
		break;

	case SND_SOC_BIAS_OFF:
		break;
	}

	codec->dapm.bias_level = level;

	return 0;
}


#define WM8961_RATES SNDRV_PCM_RATE_8000_48000

#define WM8961_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE)

static const struct snd_soc_dai_ops wm8961_dai_ops = {
	.hw_params = wm8961_hw_params,
	.set_sysclk = wm8961_set_sysclk,
	.set_fmt = wm8961_set_fmt,
	.digital_mute = wm8961_digital_mute,
	.set_tristate = wm8961_set_tristate,
	.set_clkdiv = wm8961_set_clkdiv,
};

static struct snd_soc_dai_driver wm8961_dai = {
	.name = "wm8961-hifi",
	.playback = {
		.stream_name = "HiFi Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM8961_RATES,
		.formats = WM8961_FORMATS,},
	.capture = {
		.stream_name = "HiFi Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM8961_RATES,
		.formats = WM8961_FORMATS,},
	.ops = &wm8961_dai_ops,
};

static int wm8961_probe(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret = 0;
	u16 reg;

	ret = snd_soc_codec_set_cache_io(codec, 8, 16, SND_SOC_I2C);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	reg = snd_soc_read(codec, WM8961_SOFTWARE_RESET);
	if (reg != 0x1801) {
		dev_err(codec->dev, "Device is not a WM8961: ID=0x%x\n", reg);
		return -EINVAL;
	}

	
	codec->cache_bypass = 1;
	reg = snd_soc_read(codec, WM8961_RIGHT_INPUT_VOLUME);
	codec->cache_bypass = 0;
	dev_info(codec->dev, "WM8961 family %d revision %c\n",
		 (reg & WM8961_DEVICE_ID_MASK) >> WM8961_DEVICE_ID_SHIFT,
		 ((reg & WM8961_CHIP_REV_MASK) >> WM8961_CHIP_REV_SHIFT)
		 + 'A');

	ret = wm8961_reset(codec);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to issue reset\n");
		return ret;
	}

	
	reg = snd_soc_read(codec, WM8961_CHARGE_PUMP_B);
	reg |= WM8961_CP_DYN_PWR_MASK;
	snd_soc_write(codec, WM8961_CHARGE_PUMP_B, reg);

	reg = snd_soc_read(codec, WM8961_ROUT1_VOLUME);
	snd_soc_write(codec, WM8961_ROUT1_VOLUME,
		     reg | WM8961_LO1ZC | WM8961_OUT1VU);
	snd_soc_write(codec, WM8961_LOUT1_VOLUME, reg | WM8961_LO1ZC);
	reg = snd_soc_read(codec, WM8961_ROUT2_VOLUME);
	snd_soc_write(codec, WM8961_ROUT2_VOLUME,
		     reg | WM8961_SPKRZC | WM8961_SPKVU);
	snd_soc_write(codec, WM8961_LOUT2_VOLUME, reg | WM8961_SPKLZC);

	reg = snd_soc_read(codec, WM8961_RIGHT_ADC_VOLUME);
	snd_soc_write(codec, WM8961_RIGHT_ADC_VOLUME, reg | WM8961_ADCVU);
	reg = snd_soc_read(codec, WM8961_RIGHT_INPUT_VOLUME);
	snd_soc_write(codec, WM8961_RIGHT_INPUT_VOLUME, reg | WM8961_IPVU);

	
	reg = snd_soc_read(codec, WM8961_ADC_DAC_CONTROL_2);
	reg |= WM8961_DACSMM;
	snd_soc_write(codec, WM8961_ADC_DAC_CONTROL_2, reg);

	reg = snd_soc_read(codec, WM8961_CLOCKING_3);
	reg &= ~WM8961_MANUAL_MODE;
	snd_soc_write(codec, WM8961_CLOCKING_3, reg);

	wm8961_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	snd_soc_add_codec_controls(codec, wm8961_snd_controls,
				ARRAY_SIZE(wm8961_snd_controls));
	snd_soc_dapm_new_controls(dapm, wm8961_dapm_widgets,
				  ARRAY_SIZE(wm8961_dapm_widgets));
	snd_soc_dapm_add_routes(dapm, audio_paths, ARRAY_SIZE(audio_paths));

	return 0;
}

static int wm8961_remove(struct snd_soc_codec *codec)
{
	wm8961_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

#ifdef CONFIG_PM
static int wm8961_suspend(struct snd_soc_codec *codec)
{
	wm8961_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int wm8961_resume(struct snd_soc_codec *codec)
{
	snd_soc_cache_sync(codec);

	wm8961_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}
#else
#define wm8961_suspend NULL
#define wm8961_resume NULL
#endif

static struct snd_soc_codec_driver soc_codec_dev_wm8961 = {
	.probe =	wm8961_probe,
	.remove =	wm8961_remove,
	.suspend =	wm8961_suspend,
	.resume =	wm8961_resume,
	.set_bias_level = wm8961_set_bias_level,
	.reg_cache_size = ARRAY_SIZE(wm8961_reg_defaults),
	.reg_word_size = sizeof(u16),
	.reg_cache_default = wm8961_reg_defaults,
	.volatile_register = wm8961_volatile_register,
};

static __devinit int wm8961_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct wm8961_priv *wm8961;
	int ret;

	wm8961 = devm_kzalloc(&i2c->dev, sizeof(struct wm8961_priv),
			      GFP_KERNEL);
	if (wm8961 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, wm8961);

	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_wm8961, &wm8961_dai, 1);

	return ret;
}

static __devexit int wm8961_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);

	return 0;
}

static const struct i2c_device_id wm8961_i2c_id[] = {
	{ "wm8961", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, wm8961_i2c_id);

static struct i2c_driver wm8961_i2c_driver = {
	.driver = {
		.name = "wm8961",
		.owner = THIS_MODULE,
	},
	.probe =    wm8961_i2c_probe,
	.remove =   __devexit_p(wm8961_i2c_remove),
	.id_table = wm8961_i2c_id,
};

static int __init wm8961_modinit(void)
{
	int ret = 0;
	ret = i2c_add_driver(&wm8961_i2c_driver);
	if (ret != 0) {
		printk(KERN_ERR "Failed to register wm8961 I2C driver: %d\n",
		       ret);
	}
	return ret;
}
module_init(wm8961_modinit);

static void __exit wm8961_exit(void)
{
	i2c_del_driver(&wm8961_i2c_driver);
}
module_exit(wm8961_exit);

MODULE_DESCRIPTION("ASoC WM8961 driver");
MODULE_AUTHOR("Mark Brown <broonie@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL");
