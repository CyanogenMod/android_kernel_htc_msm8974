/*
 * max98095.c -- MAX98095 ALSA SoC Audio driver
 *
 * Copyright 2011 Maxim Integrated Products
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/slab.h>
#include <asm/div64.h>
#include <sound/max98095.h>
#include "max98095.h"

enum max98095_type {
	MAX98095,
};

struct max98095_cdata {
	unsigned int rate;
	unsigned int fmt;
	int eq_sel;
	int bq_sel;
};

struct max98095_priv {
	enum max98095_type devtype;
	struct max98095_pdata *pdata;
	unsigned int sysclk;
	struct max98095_cdata dai[3];
	const char **eq_texts;
	const char **bq_texts;
	struct soc_enum eq_enum;
	struct soc_enum bq_enum;
	int eq_textcnt;
	int bq_textcnt;
	u8 lin_state;
	unsigned int mic1pre;
	unsigned int mic2pre;
};

static const u8 max98095_reg_def[M98095_REG_CNT] = {
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x30, 
	0xF0, 
	0x00, 
	0x00, 
	0x3F, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
};

static struct {
	int readable;
	int writable;
} max98095_access[M98095_REG_CNT] = {
	{ 0x00, 0x00 }, 
	{ 0xFF, 0x00 }, 
	{ 0xFF, 0x00 }, 
	{ 0xFF, 0x00 }, 
	{ 0xFF, 0x00 }, 
	{ 0xFF, 0x00 }, 
	{ 0xFF, 0x00 }, 
	{ 0xFF, 0x00 }, 
	{ 0xFF, 0x00 }, 
	{ 0xFF, 0x00 }, 
	{ 0xFF, 0x00 }, 
	{ 0xFF, 0x00 }, 
	{ 0xFF, 0x00 }, 
	{ 0xFF, 0x00 }, 
	{ 0xFF, 0x00 }, 
	{ 0xFF, 0x9F }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0x77 }, 
	{ 0xFF, 0x77 }, 
	{ 0xFF, 0x77 }, 
	{ 0xFF, 0x77 }, 
	{ 0xFF, 0x77 }, 
	{ 0xFF, 0x77 }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0x7F }, 
	{ 0xFF, 0x31 }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xF7 }, 
	{ 0xFF, 0x2F }, 
	{ 0xFF, 0xEF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xF7 }, 
	{ 0xFF, 0x2F }, 
	{ 0xFF, 0xCF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xF7 }, 
	{ 0xFF, 0x2F }, 
	{ 0xFF, 0xCF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0x77 }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0x0F }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0x3F }, 
	{ 0xFF, 0x3F }, 
	{ 0xFF, 0x3F }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0x7F }, 
	{ 0xFF, 0x7F }, 
	{ 0xFF, 0x0F }, 
	{ 0xFF, 0x3F }, 
	{ 0xFF, 0x3F }, 
	{ 0xFF, 0x3F }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xBF }, 
	{ 0xFF, 0x1F }, 
	{ 0xFF, 0xBF }, 
	{ 0xFF, 0x1F }, 
	{ 0xFF, 0xBF }, 
	{ 0xFF, 0x3F }, 
	{ 0xFF, 0x3F }, 
	{ 0xFF, 0x7F }, 
	{ 0xFF, 0x7F }, 
	{ 0xFF, 0x47 }, 
	{ 0xFF, 0x9F }, 
	{ 0xFF, 0x9F }, 
	{ 0xFF, 0x9F }, 
	{ 0xFF, 0x9F }, 
	{ 0xFF, 0x9F }, 
	{ 0xFF, 0xBF }, 
	{ 0xFF, 0xBF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0x7F }, 
	{ 0xFF, 0xF7 }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0x1F }, 
	{ 0xFF, 0xF7 }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0x1F }, 
	{ 0xFF, 0xF7 }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0x1F }, 
	{ 0xFF, 0xF7 }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0x1F }, 
	{ 0xFF, 0xF7 }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0x1F }, 
	{ 0xFF, 0xF7 }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0x1F }, 
	{ 0xFF, 0x7F }, 
	{ 0xFF, 0x0F }, 
	{ 0xFF, 0xD8 }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xEF }, 
	{ 0xFF, 0xFE }, 
	{ 0xFF, 0xFE }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0x3F }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0x3F }, 
	{ 0xFF, 0x8F }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0x3F }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0xFF }, 
	{ 0xFF, 0x0F }, 
	{ 0xFF, 0x3F }, 
	{ 0xFF, 0x8C }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0x00, 0x00 }, 
	{ 0xFF, 0x00 }, 
};

static int max98095_readable(struct snd_soc_codec *codec, unsigned int reg)
{
	if (reg >= M98095_REG_CNT)
		return 0;
	return max98095_access[reg].readable != 0;
}

static int max98095_volatile(struct snd_soc_codec *codec, unsigned int reg)
{
	if (reg > M98095_REG_MAX_CACHED)
		return 1;

	switch (reg) {
	case M98095_000_HOST_DATA:
	case M98095_001_HOST_INT_STS:
	case M98095_002_HOST_RSP_STS:
	case M98095_003_HOST_CMD_STS:
	case M98095_004_CODEC_STS:
	case M98095_005_DAI1_ALC_STS:
	case M98095_006_DAI2_ALC_STS:
	case M98095_007_JACK_AUTO_STS:
	case M98095_008_JACK_MANUAL_STS:
	case M98095_009_JACK_VBAT_STS:
	case M98095_00A_ACC_ADC_STS:
	case M98095_00B_MIC_NG_AGC_STS:
	case M98095_00C_SPK_L_VOLT_STS:
	case M98095_00D_SPK_R_VOLT_STS:
	case M98095_00E_TEMP_SENSOR_STS:
		return 1;
	}

	return 0;
}

static int max98095_hw_write(struct snd_soc_codec *codec, unsigned int reg,
			     unsigned int value)
{
	int ret;

	codec->cache_bypass = 1;
	ret = snd_soc_write(codec, reg, value);
	codec->cache_bypass = 0;

	return ret ? -EIO : 0;
}

static void m98095_eq_band(struct snd_soc_codec *codec, unsigned int dai,
		    unsigned int band, u16 *coefs)
{
	unsigned int eq_reg;
	unsigned int i;

	BUG_ON(band > 4);
	BUG_ON(dai > 1);

	
	eq_reg = dai ? M98095_142_DAI2_EQ_BASE : M98095_110_DAI1_EQ_BASE;

	
	eq_reg += band * (M98095_COEFS_PER_BAND << 1);

	
	for (i = 0; i < M98095_COEFS_PER_BAND; i++) {
		max98095_hw_write(codec, eq_reg++, M98095_BYTE1(coefs[i]));
		max98095_hw_write(codec, eq_reg++, M98095_BYTE0(coefs[i]));
	}
}

static void m98095_biquad_band(struct snd_soc_codec *codec, unsigned int dai,
		    unsigned int band, u16 *coefs)
{
	unsigned int bq_reg;
	unsigned int i;

	BUG_ON(band > 1);
	BUG_ON(dai > 1);

	
	bq_reg = dai ? M98095_17E_DAI2_BQ_BASE : M98095_174_DAI1_BQ_BASE;

	
	bq_reg += band * (M98095_COEFS_PER_BAND << 1);

	
	for (i = 0; i < M98095_COEFS_PER_BAND; i++) {
		max98095_hw_write(codec, bq_reg++, M98095_BYTE1(coefs[i]));
		max98095_hw_write(codec, bq_reg++, M98095_BYTE0(coefs[i]));
	}
}

static const char * const max98095_fltr_mode[] = { "Voice", "Music" };
static const struct soc_enum max98095_dai1_filter_mode_enum[] = {
	SOC_ENUM_SINGLE(M98095_02E_DAI1_FILTERS, 7, 2, max98095_fltr_mode),
};
static const struct soc_enum max98095_dai2_filter_mode_enum[] = {
	SOC_ENUM_SINGLE(M98095_038_DAI2_FILTERS, 7, 2, max98095_fltr_mode),
};

static const char * const max98095_extmic_text[] = { "None", "MIC1", "MIC2" };

static const struct soc_enum max98095_extmic_enum =
	SOC_ENUM_SINGLE(M98095_087_CFG_MIC, 0, 3, max98095_extmic_text);

static const struct snd_kcontrol_new max98095_extmic_mux =
	SOC_DAPM_ENUM("External MIC Mux", max98095_extmic_enum);

static const char * const max98095_linein_text[] = { "INA", "INB" };

static const struct soc_enum max98095_linein_enum =
	SOC_ENUM_SINGLE(M98095_086_CFG_LINE, 6, 2, max98095_linein_text);

static const struct snd_kcontrol_new max98095_linein_mux =
	SOC_DAPM_ENUM("Linein Input Mux", max98095_linein_enum);

static const char * const max98095_line_mode_text[] = {
	"Stereo", "Differential"};

static const struct soc_enum max98095_linein_mode_enum =
	SOC_ENUM_SINGLE(M98095_086_CFG_LINE, 7, 2, max98095_line_mode_text);

static const struct soc_enum max98095_lineout_mode_enum =
	SOC_ENUM_SINGLE(M98095_086_CFG_LINE, 4, 2, max98095_line_mode_text);

static const char * const max98095_dai_fltr[] = {
	"Off", "Elliptical-HPF-16k", "Butterworth-HPF-16k",
	"Elliptical-HPF-8k", "Butterworth-HPF-8k", "Butterworth-HPF-Fs/240"};
static const struct soc_enum max98095_dai1_dac_filter_enum[] = {
	SOC_ENUM_SINGLE(M98095_02E_DAI1_FILTERS, 0, 6, max98095_dai_fltr),
};
static const struct soc_enum max98095_dai2_dac_filter_enum[] = {
	SOC_ENUM_SINGLE(M98095_038_DAI2_FILTERS, 0, 6, max98095_dai_fltr),
};
static const struct soc_enum max98095_dai3_dac_filter_enum[] = {
	SOC_ENUM_SINGLE(M98095_042_DAI3_FILTERS, 0, 6, max98095_dai_fltr),
};

static int max98095_mic1pre_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];

	max98095->mic1pre = sel;
	snd_soc_update_bits(codec, M98095_05F_LVL_MIC1, M98095_MICPRE_MASK,
		(1+sel)<<M98095_MICPRE_SHIFT);

	return 0;
}

static int max98095_mic1pre_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = max98095->mic1pre;
	return 0;
}

static int max98095_mic2pre_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];

	max98095->mic2pre = sel;
	snd_soc_update_bits(codec, M98095_060_LVL_MIC2, M98095_MICPRE_MASK,
		(1+sel)<<M98095_MICPRE_SHIFT);

	return 0;
}

static int max98095_mic2pre_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = max98095->mic2pre;
	return 0;
}

static const unsigned int max98095_micboost_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0, 1, TLV_DB_SCALE_ITEM(0, 2000, 0),
	2, 2, TLV_DB_SCALE_ITEM(3000, 0, 0),
};

static const DECLARE_TLV_DB_SCALE(max98095_mic_tlv, 0, 100, 0);
static const DECLARE_TLV_DB_SCALE(max98095_adc_tlv, -1200, 100, 0);
static const DECLARE_TLV_DB_SCALE(max98095_adcboost_tlv, 0, 600, 0);

static const unsigned int max98095_hp_tlv[] = {
	TLV_DB_RANGE_HEAD(5),
	0, 6, TLV_DB_SCALE_ITEM(-6700, 400, 0),
	7, 14, TLV_DB_SCALE_ITEM(-4000, 300, 0),
	15, 21, TLV_DB_SCALE_ITEM(-1700, 200, 0),
	22, 27, TLV_DB_SCALE_ITEM(-400, 100, 0),
	28, 31, TLV_DB_SCALE_ITEM(150, 50, 0),
};

static const unsigned int max98095_spk_tlv[] = {
	TLV_DB_RANGE_HEAD(4),
	0, 10, TLV_DB_SCALE_ITEM(-5900, 400, 0),
	11, 18, TLV_DB_SCALE_ITEM(-1700, 200, 0),
	19, 27, TLV_DB_SCALE_ITEM(-200, 100, 0),
	28, 39, TLV_DB_SCALE_ITEM(650, 50, 0),
};

static const unsigned int max98095_rcv_lout_tlv[] = {
	TLV_DB_RANGE_HEAD(5),
	0, 6, TLV_DB_SCALE_ITEM(-6200, 400, 0),
	7, 14, TLV_DB_SCALE_ITEM(-3500, 300, 0),
	15, 21, TLV_DB_SCALE_ITEM(-1200, 200, 0),
	22, 27, TLV_DB_SCALE_ITEM(100, 100, 0),
	28, 31, TLV_DB_SCALE_ITEM(650, 50, 0),
};

static const unsigned int max98095_lin_tlv[] = {
	TLV_DB_RANGE_HEAD(3),
	0, 2, TLV_DB_SCALE_ITEM(-600, 300, 0),
	3, 3, TLV_DB_SCALE_ITEM(300, 1100, 0),
	4, 5, TLV_DB_SCALE_ITEM(1400, 600, 0),
};

static const struct snd_kcontrol_new max98095_snd_controls[] = {

	SOC_DOUBLE_R_TLV("Headphone Volume", M98095_064_LVL_HP_L,
		M98095_065_LVL_HP_R, 0, 31, 0, max98095_hp_tlv),

	SOC_DOUBLE_R_TLV("Speaker Volume", M98095_067_LVL_SPK_L,
		M98095_068_LVL_SPK_R, 0, 39, 0, max98095_spk_tlv),

	SOC_SINGLE_TLV("Receiver Volume", M98095_066_LVL_RCV,
		0, 31, 0, max98095_rcv_lout_tlv),

	SOC_DOUBLE_R_TLV("Lineout Volume", M98095_062_LVL_LINEOUT1,
		M98095_063_LVL_LINEOUT2, 0, 31, 0, max98095_rcv_lout_tlv),

	SOC_DOUBLE_R("Headphone Switch", M98095_064_LVL_HP_L,
		M98095_065_LVL_HP_R, 7, 1, 1),

	SOC_DOUBLE_R("Speaker Switch", M98095_067_LVL_SPK_L,
		M98095_068_LVL_SPK_R, 7, 1, 1),

	SOC_SINGLE("Receiver Switch", M98095_066_LVL_RCV, 7, 1, 1),

	SOC_DOUBLE_R("Lineout Switch", M98095_062_LVL_LINEOUT1,
		M98095_063_LVL_LINEOUT2, 7, 1, 1),

	SOC_SINGLE_TLV("MIC1 Volume", M98095_05F_LVL_MIC1, 0, 20, 1,
		max98095_mic_tlv),

	SOC_SINGLE_TLV("MIC2 Volume", M98095_060_LVL_MIC2, 0, 20, 1,
		max98095_mic_tlv),

	SOC_SINGLE_EXT_TLV("MIC1 Boost Volume",
			M98095_05F_LVL_MIC1, 5, 2, 0,
			max98095_mic1pre_get, max98095_mic1pre_set,
			max98095_micboost_tlv),
	SOC_SINGLE_EXT_TLV("MIC2 Boost Volume",
			M98095_060_LVL_MIC2, 5, 2, 0,
			max98095_mic2pre_get, max98095_mic2pre_set,
			max98095_micboost_tlv),

	SOC_SINGLE_TLV("Linein Volume", M98095_061_LVL_LINEIN, 0, 5, 1,
		max98095_lin_tlv),

	SOC_SINGLE_TLV("ADCL Volume", M98095_05D_LVL_ADC_L, 0, 15, 1,
		max98095_adc_tlv),
	SOC_SINGLE_TLV("ADCR Volume", M98095_05E_LVL_ADC_R, 0, 15, 1,
		max98095_adc_tlv),

	SOC_SINGLE_TLV("ADCL Boost Volume", M98095_05D_LVL_ADC_L, 4, 3, 0,
		max98095_adcboost_tlv),
	SOC_SINGLE_TLV("ADCR Boost Volume", M98095_05E_LVL_ADC_R, 4, 3, 0,
		max98095_adcboost_tlv),

	SOC_SINGLE("EQ1 Switch", M98095_088_CFG_LEVEL, 0, 1, 0),
	SOC_SINGLE("EQ2 Switch", M98095_088_CFG_LEVEL, 1, 1, 0),

	SOC_SINGLE("Biquad1 Switch", M98095_088_CFG_LEVEL, 2, 1, 0),
	SOC_SINGLE("Biquad2 Switch", M98095_088_CFG_LEVEL, 3, 1, 0),

	SOC_ENUM("DAI1 Filter Mode", max98095_dai1_filter_mode_enum),
	SOC_ENUM("DAI2 Filter Mode", max98095_dai2_filter_mode_enum),
	SOC_ENUM("DAI1 DAC Filter", max98095_dai1_dac_filter_enum),
	SOC_ENUM("DAI2 DAC Filter", max98095_dai2_dac_filter_enum),
	SOC_ENUM("DAI3 DAC Filter", max98095_dai3_dac_filter_enum),

	SOC_ENUM("Linein Mode", max98095_linein_mode_enum),
	SOC_ENUM("Lineout Mode", max98095_lineout_mode_enum),
};

static const struct snd_kcontrol_new max98095_left_speaker_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC1 Switch", M98095_050_MIX_SPK_LEFT, 0, 1, 0),
	SOC_DAPM_SINGLE("Right DAC1 Switch", M98095_050_MIX_SPK_LEFT, 6, 1, 0),
	SOC_DAPM_SINGLE("Mono DAC2 Switch", M98095_050_MIX_SPK_LEFT, 3, 1, 0),
	SOC_DAPM_SINGLE("Mono DAC3 Switch", M98095_050_MIX_SPK_LEFT, 3, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M98095_050_MIX_SPK_LEFT, 4, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M98095_050_MIX_SPK_LEFT, 5, 1, 0),
	SOC_DAPM_SINGLE("IN1 Switch", M98095_050_MIX_SPK_LEFT, 1, 1, 0),
	SOC_DAPM_SINGLE("IN2 Switch", M98095_050_MIX_SPK_LEFT, 2, 1, 0),
};

static const struct snd_kcontrol_new max98095_right_speaker_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC1 Switch", M98095_051_MIX_SPK_RIGHT, 6, 1, 0),
	SOC_DAPM_SINGLE("Right DAC1 Switch", M98095_051_MIX_SPK_RIGHT, 0, 1, 0),
	SOC_DAPM_SINGLE("Mono DAC2 Switch", M98095_051_MIX_SPK_RIGHT, 3, 1, 0),
	SOC_DAPM_SINGLE("Mono DAC3 Switch", M98095_051_MIX_SPK_RIGHT, 3, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M98095_051_MIX_SPK_RIGHT, 5, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M98095_051_MIX_SPK_RIGHT, 4, 1, 0),
	SOC_DAPM_SINGLE("IN1 Switch", M98095_051_MIX_SPK_RIGHT, 1, 1, 0),
	SOC_DAPM_SINGLE("IN2 Switch", M98095_051_MIX_SPK_RIGHT, 2, 1, 0),
};

static const struct snd_kcontrol_new max98095_left_hp_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC1 Switch", M98095_04C_MIX_HP_LEFT, 0, 1, 0),
	SOC_DAPM_SINGLE("Right DAC1 Switch", M98095_04C_MIX_HP_LEFT, 5, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M98095_04C_MIX_HP_LEFT, 3, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M98095_04C_MIX_HP_LEFT, 4, 1, 0),
	SOC_DAPM_SINGLE("IN1 Switch", M98095_04C_MIX_HP_LEFT, 1, 1, 0),
	SOC_DAPM_SINGLE("IN2 Switch", M98095_04C_MIX_HP_LEFT, 2, 1, 0),
};

static const struct snd_kcontrol_new max98095_right_hp_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC1 Switch", M98095_04D_MIX_HP_RIGHT, 5, 1, 0),
	SOC_DAPM_SINGLE("Right DAC1 Switch", M98095_04D_MIX_HP_RIGHT, 0, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M98095_04D_MIX_HP_RIGHT, 3, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M98095_04D_MIX_HP_RIGHT, 4, 1, 0),
	SOC_DAPM_SINGLE("IN1 Switch", M98095_04D_MIX_HP_RIGHT, 1, 1, 0),
	SOC_DAPM_SINGLE("IN2 Switch", M98095_04D_MIX_HP_RIGHT, 2, 1, 0),
};

static const struct snd_kcontrol_new max98095_mono_rcv_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC1 Switch", M98095_04F_MIX_RCV, 0, 1, 0),
	SOC_DAPM_SINGLE("Right DAC1 Switch", M98095_04F_MIX_RCV, 5, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M98095_04F_MIX_RCV, 3, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M98095_04F_MIX_RCV, 4, 1, 0),
	SOC_DAPM_SINGLE("IN1 Switch", M98095_04F_MIX_RCV, 1, 1, 0),
	SOC_DAPM_SINGLE("IN2 Switch", M98095_04F_MIX_RCV, 2, 1, 0),
};

static const struct snd_kcontrol_new max98095_left_lineout_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC1 Switch", M98095_053_MIX_LINEOUT1, 5, 1, 0),
	SOC_DAPM_SINGLE("Right DAC1 Switch", M98095_053_MIX_LINEOUT1, 0, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M98095_053_MIX_LINEOUT1, 3, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M98095_053_MIX_LINEOUT1, 4, 1, 0),
	SOC_DAPM_SINGLE("IN1 Switch", M98095_053_MIX_LINEOUT1, 1, 1, 0),
	SOC_DAPM_SINGLE("IN2 Switch", M98095_053_MIX_LINEOUT1, 2, 1, 0),
};

static const struct snd_kcontrol_new max98095_right_lineout_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC1 Switch", M98095_054_MIX_LINEOUT2, 0, 1, 0),
	SOC_DAPM_SINGLE("Right DAC1 Switch", M98095_054_MIX_LINEOUT2, 5, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M98095_054_MIX_LINEOUT2, 3, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M98095_054_MIX_LINEOUT2, 4, 1, 0),
	SOC_DAPM_SINGLE("IN1 Switch", M98095_054_MIX_LINEOUT2, 1, 1, 0),
	SOC_DAPM_SINGLE("IN2 Switch", M98095_054_MIX_LINEOUT2, 2, 1, 0),
};

static const struct snd_kcontrol_new max98095_left_ADC_mixer_controls[] = {
	SOC_DAPM_SINGLE("MIC1 Switch", M98095_04A_MIX_ADC_LEFT, 7, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M98095_04A_MIX_ADC_LEFT, 6, 1, 0),
	SOC_DAPM_SINGLE("IN1 Switch", M98095_04A_MIX_ADC_LEFT, 3, 1, 0),
	SOC_DAPM_SINGLE("IN2 Switch", M98095_04A_MIX_ADC_LEFT, 2, 1, 0),
};

static const struct snd_kcontrol_new max98095_right_ADC_mixer_controls[] = {
	SOC_DAPM_SINGLE("MIC1 Switch", M98095_04B_MIX_ADC_RIGHT, 7, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M98095_04B_MIX_ADC_RIGHT, 6, 1, 0),
	SOC_DAPM_SINGLE("IN1 Switch", M98095_04B_MIX_ADC_RIGHT, 3, 1, 0),
	SOC_DAPM_SINGLE("IN2 Switch", M98095_04B_MIX_ADC_RIGHT, 2, 1, 0),
};

static int max98095_mic_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (w->reg == M98095_05F_LVL_MIC1) {
			snd_soc_update_bits(codec, w->reg, M98095_MICPRE_MASK,
				(1+max98095->mic1pre)<<M98095_MICPRE_SHIFT);
		} else {
			snd_soc_update_bits(codec, w->reg, M98095_MICPRE_MASK,
				(1+max98095->mic2pre)<<M98095_MICPRE_SHIFT);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, w->reg, M98095_MICPRE_MASK, 0);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int max98095_line_pga(struct snd_soc_dapm_widget *w,
			     int event, u8 channel)
{
	struct snd_soc_codec *codec = w->codec;
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);
	u8 *state;

	BUG_ON(!((channel == 1) || (channel == 2)));

	state = &max98095->lin_state;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		*state |= channel;
		snd_soc_update_bits(codec, w->reg,
			(1 << w->shift), (1 << w->shift));
		break;
	case SND_SOC_DAPM_POST_PMD:
		*state &= ~channel;
		if (*state == 0) {
			snd_soc_update_bits(codec, w->reg,
				(1 << w->shift), 0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int max98095_pga_in1_event(struct snd_soc_dapm_widget *w,
				   struct snd_kcontrol *k, int event)
{
	return max98095_line_pga(w, event, 1);
}

static int max98095_pga_in2_event(struct snd_soc_dapm_widget *w,
				   struct snd_kcontrol *k, int event)
{
	return max98095_line_pga(w, event, 2);
}

static int max98095_lineout_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, w->reg,
			(1 << (w->shift+2)), (1 << (w->shift+2)));
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, w->reg,
			(1 << (w->shift+2)), 0);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct snd_soc_dapm_widget max98095_dapm_widgets[] = {

	SND_SOC_DAPM_ADC("ADCL", "HiFi Capture", M98095_090_PWR_EN_IN, 0, 0),
	SND_SOC_DAPM_ADC("ADCR", "HiFi Capture", M98095_090_PWR_EN_IN, 1, 0),

	SND_SOC_DAPM_DAC("DACL1", "HiFi Playback",
		M98095_091_PWR_EN_OUT, 0, 0),
	SND_SOC_DAPM_DAC("DACR1", "HiFi Playback",
		M98095_091_PWR_EN_OUT, 1, 0),
	SND_SOC_DAPM_DAC("DACM2", "Aux Playback",
		M98095_091_PWR_EN_OUT, 2, 0),
	SND_SOC_DAPM_DAC("DACM3", "Voice Playback",
		M98095_091_PWR_EN_OUT, 2, 0),

	SND_SOC_DAPM_PGA("HP Left Out", M98095_091_PWR_EN_OUT,
		6, 0, NULL, 0),
	SND_SOC_DAPM_PGA("HP Right Out", M98095_091_PWR_EN_OUT,
		7, 0, NULL, 0),

	SND_SOC_DAPM_PGA("SPK Left Out", M98095_091_PWR_EN_OUT,
		4, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPK Right Out", M98095_091_PWR_EN_OUT,
		5, 0, NULL, 0),

	SND_SOC_DAPM_PGA("RCV Mono Out", M98095_091_PWR_EN_OUT,
		3, 0, NULL, 0),

	SND_SOC_DAPM_PGA_E("LINE Left Out", M98095_092_PWR_EN_OUT,
		0, 0, NULL, 0, max98095_lineout_event, SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_E("LINE Right Out", M98095_092_PWR_EN_OUT,
		1, 0, NULL, 0, max98095_lineout_event, SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_MUX("External MIC", SND_SOC_NOPM, 0, 0,
		&max98095_extmic_mux),

	SND_SOC_DAPM_MUX("Linein Mux", SND_SOC_NOPM, 0, 0,
		&max98095_linein_mux),

	SND_SOC_DAPM_MIXER("Left Headphone Mixer", SND_SOC_NOPM, 0, 0,
		&max98095_left_hp_mixer_controls[0],
		ARRAY_SIZE(max98095_left_hp_mixer_controls)),

	SND_SOC_DAPM_MIXER("Right Headphone Mixer", SND_SOC_NOPM, 0, 0,
		&max98095_right_hp_mixer_controls[0],
		ARRAY_SIZE(max98095_right_hp_mixer_controls)),

	SND_SOC_DAPM_MIXER("Left Speaker Mixer", SND_SOC_NOPM, 0, 0,
		&max98095_left_speaker_mixer_controls[0],
		ARRAY_SIZE(max98095_left_speaker_mixer_controls)),

	SND_SOC_DAPM_MIXER("Right Speaker Mixer", SND_SOC_NOPM, 0, 0,
		&max98095_right_speaker_mixer_controls[0],
		ARRAY_SIZE(max98095_right_speaker_mixer_controls)),

	SND_SOC_DAPM_MIXER("Receiver Mixer", SND_SOC_NOPM, 0, 0,
	  &max98095_mono_rcv_mixer_controls[0],
		ARRAY_SIZE(max98095_mono_rcv_mixer_controls)),

	SND_SOC_DAPM_MIXER("Left Lineout Mixer", SND_SOC_NOPM, 0, 0,
		&max98095_left_lineout_mixer_controls[0],
		ARRAY_SIZE(max98095_left_lineout_mixer_controls)),

	SND_SOC_DAPM_MIXER("Right Lineout Mixer", SND_SOC_NOPM, 0, 0,
		&max98095_right_lineout_mixer_controls[0],
		ARRAY_SIZE(max98095_right_lineout_mixer_controls)),

	SND_SOC_DAPM_MIXER("Left ADC Mixer", SND_SOC_NOPM, 0, 0,
		&max98095_left_ADC_mixer_controls[0],
		ARRAY_SIZE(max98095_left_ADC_mixer_controls)),

	SND_SOC_DAPM_MIXER("Right ADC Mixer", SND_SOC_NOPM, 0, 0,
		&max98095_right_ADC_mixer_controls[0],
		ARRAY_SIZE(max98095_right_ADC_mixer_controls)),

	SND_SOC_DAPM_PGA_E("MIC1 Input", M98095_05F_LVL_MIC1,
		5, 0, NULL, 0, max98095_mic_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("MIC2 Input", M98095_060_LVL_MIC2,
		5, 0, NULL, 0, max98095_mic_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("IN1 Input", M98095_090_PWR_EN_IN,
		7, 0, NULL, 0, max98095_pga_in1_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("IN2 Input", M98095_090_PWR_EN_IN,
		7, 0, NULL, 0, max98095_pga_in2_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MICBIAS("MICBIAS1", M98095_090_PWR_EN_IN, 2, 0),
	SND_SOC_DAPM_MICBIAS("MICBIAS2", M98095_090_PWR_EN_IN, 3, 0),

	SND_SOC_DAPM_OUTPUT("HPL"),
	SND_SOC_DAPM_OUTPUT("HPR"),
	SND_SOC_DAPM_OUTPUT("SPKL"),
	SND_SOC_DAPM_OUTPUT("SPKR"),
	SND_SOC_DAPM_OUTPUT("RCV"),
	SND_SOC_DAPM_OUTPUT("OUT1"),
	SND_SOC_DAPM_OUTPUT("OUT2"),
	SND_SOC_DAPM_OUTPUT("OUT3"),
	SND_SOC_DAPM_OUTPUT("OUT4"),

	SND_SOC_DAPM_INPUT("MIC1"),
	SND_SOC_DAPM_INPUT("MIC2"),
	SND_SOC_DAPM_INPUT("INA1"),
	SND_SOC_DAPM_INPUT("INA2"),
	SND_SOC_DAPM_INPUT("INB1"),
	SND_SOC_DAPM_INPUT("INB2"),
};

static const struct snd_soc_dapm_route max98095_audio_map[] = {
	
	{"Left Headphone Mixer", "Left DAC1 Switch", "DACL1"},
	{"Left Headphone Mixer", "Right DAC1 Switch", "DACR1"},
	{"Left Headphone Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Left Headphone Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Left Headphone Mixer", "IN1 Switch", "IN1 Input"},
	{"Left Headphone Mixer", "IN2 Switch", "IN2 Input"},

	
	{"Right Headphone Mixer", "Left DAC1 Switch", "DACL1"},
	{"Right Headphone Mixer", "Right DAC1 Switch", "DACR1"},
	{"Right Headphone Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Right Headphone Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Right Headphone Mixer", "IN1 Switch", "IN1 Input"},
	{"Right Headphone Mixer", "IN2 Switch", "IN2 Input"},

	
	{"Left Speaker Mixer", "Left DAC1 Switch", "DACL1"},
	{"Left Speaker Mixer", "Right DAC1 Switch", "DACR1"},
	{"Left Speaker Mixer", "Mono DAC2 Switch", "DACM2"},
	{"Left Speaker Mixer", "Mono DAC3 Switch", "DACM3"},
	{"Left Speaker Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Left Speaker Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Left Speaker Mixer", "IN1 Switch", "IN1 Input"},
	{"Left Speaker Mixer", "IN2 Switch", "IN2 Input"},

	
	{"Right Speaker Mixer", "Left DAC1 Switch", "DACL1"},
	{"Right Speaker Mixer", "Right DAC1 Switch", "DACR1"},
	{"Right Speaker Mixer", "Mono DAC2 Switch", "DACM2"},
	{"Right Speaker Mixer", "Mono DAC3 Switch", "DACM3"},
	{"Right Speaker Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Right Speaker Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Right Speaker Mixer", "IN1 Switch", "IN1 Input"},
	{"Right Speaker Mixer", "IN2 Switch", "IN2 Input"},

	
	{"Receiver Mixer", "Left DAC1 Switch", "DACL1"},
	{"Receiver Mixer", "Right DAC1 Switch", "DACR1"},
	{"Receiver Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Receiver Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Receiver Mixer", "IN1 Switch", "IN1 Input"},
	{"Receiver Mixer", "IN2 Switch", "IN2 Input"},

	
	{"Left Lineout Mixer", "Left DAC1 Switch", "DACL1"},
	{"Left Lineout Mixer", "Right DAC1 Switch", "DACR1"},
	{"Left Lineout Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Left Lineout Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Left Lineout Mixer", "IN1 Switch", "IN1 Input"},
	{"Left Lineout Mixer", "IN2 Switch", "IN2 Input"},

	
	{"Right Lineout Mixer", "Left DAC1 Switch", "DACL1"},
	{"Right Lineout Mixer", "Right DAC1 Switch", "DACR1"},
	{"Right Lineout Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Right Lineout Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Right Lineout Mixer", "IN1 Switch", "IN1 Input"},
	{"Right Lineout Mixer", "IN2 Switch", "IN2 Input"},

	{"HP Left Out", NULL, "Left Headphone Mixer"},
	{"HP Right Out", NULL, "Right Headphone Mixer"},
	{"SPK Left Out", NULL, "Left Speaker Mixer"},
	{"SPK Right Out", NULL, "Right Speaker Mixer"},
	{"RCV Mono Out", NULL, "Receiver Mixer"},
	{"LINE Left Out", NULL, "Left Lineout Mixer"},
	{"LINE Right Out", NULL, "Right Lineout Mixer"},

	{"HPL", NULL, "HP Left Out"},
	{"HPR", NULL, "HP Right Out"},
	{"SPKL", NULL, "SPK Left Out"},
	{"SPKR", NULL, "SPK Right Out"},
	{"RCV", NULL, "RCV Mono Out"},
	{"OUT1", NULL, "LINE Left Out"},
	{"OUT2", NULL, "LINE Right Out"},
	{"OUT3", NULL, "LINE Left Out"},
	{"OUT4", NULL, "LINE Right Out"},

	
	{"Left ADC Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Left ADC Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Left ADC Mixer", "IN1 Switch", "IN1 Input"},
	{"Left ADC Mixer", "IN2 Switch", "IN2 Input"},

	
	{"Right ADC Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Right ADC Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Right ADC Mixer", "IN1 Switch", "IN1 Input"},
	{"Right ADC Mixer", "IN2 Switch", "IN2 Input"},

	
	{"ADCL", NULL, "Left ADC Mixer"},
	{"ADCR", NULL, "Right ADC Mixer"},

	{"IN1 Input", NULL, "INA1"},
	{"IN2 Input", NULL, "INA2"},

	{"MIC1 Input", NULL, "MIC1"},
	{"MIC2 Input", NULL, "MIC2"},
};

static int max98095_add_widgets(struct snd_soc_codec *codec)
{
	snd_soc_add_codec_controls(codec, max98095_snd_controls,
			     ARRAY_SIZE(max98095_snd_controls));

	return 0;
}

static const struct {
	u32 rate;
	u8  sr;
} rate_table[] = {
	{8000,  0x01},
	{11025, 0x02},
	{16000, 0x03},
	{22050, 0x04},
	{24000, 0x05},
	{32000, 0x06},
	{44100, 0x07},
	{48000, 0x08},
	{88200, 0x09},
	{96000, 0x0A},
};

static int rate_value(int rate, u8 *value)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rate_table); i++) {
		if (rate_table[i].rate >= rate) {
			*value = rate_table[i].sr;
			return 0;
		}
	}
	*value = rate_table[0].sr;
	return -EINVAL;
}

static int max98095_dai1_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);
	struct max98095_cdata *cdata;
	unsigned long long ni;
	unsigned int rate;
	u8 regval;

	cdata = &max98095->dai[0];

	rate = params_rate(params);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		snd_soc_update_bits(codec, M98095_02A_DAI1_FORMAT,
			M98095_DAI_WS, 0);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		snd_soc_update_bits(codec, M98095_02A_DAI1_FORMAT,
			M98095_DAI_WS, M98095_DAI_WS);
		break;
	default:
		return -EINVAL;
	}

	if (rate_value(rate, &regval))
		return -EINVAL;

	snd_soc_update_bits(codec, M98095_027_DAI1_CLKMODE,
		M98095_CLKMODE_MASK, regval);
	cdata->rate = rate;

	
	if (snd_soc_read(codec, M98095_02A_DAI1_FORMAT) & M98095_DAI_MAS) {
		if (max98095->sysclk == 0) {
			dev_err(codec->dev, "Invalid system clock frequency\n");
			return -EINVAL;
		}
		ni = 65536ULL * (rate < 50000 ? 96ULL : 48ULL)
				* (unsigned long long int)rate;
		do_div(ni, (unsigned long long int)max98095->sysclk);
		snd_soc_write(codec, M98095_028_DAI1_CLKCFG_HI,
			(ni >> 8) & 0x7F);
		snd_soc_write(codec, M98095_029_DAI1_CLKCFG_LO,
			ni & 0xFF);
	}

	
	if (rate < 50000)
		snd_soc_update_bits(codec, M98095_02E_DAI1_FILTERS,
			M98095_DAI_DHF, 0);
	else
		snd_soc_update_bits(codec, M98095_02E_DAI1_FILTERS,
			M98095_DAI_DHF, M98095_DAI_DHF);

	return 0;
}

static int max98095_dai2_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);
	struct max98095_cdata *cdata;
	unsigned long long ni;
	unsigned int rate;
	u8 regval;

	cdata = &max98095->dai[1];

	rate = params_rate(params);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		snd_soc_update_bits(codec, M98095_034_DAI2_FORMAT,
			M98095_DAI_WS, 0);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		snd_soc_update_bits(codec, M98095_034_DAI2_FORMAT,
			M98095_DAI_WS, M98095_DAI_WS);
		break;
	default:
		return -EINVAL;
	}

	if (rate_value(rate, &regval))
		return -EINVAL;

	snd_soc_update_bits(codec, M98095_031_DAI2_CLKMODE,
		M98095_CLKMODE_MASK, regval);
	cdata->rate = rate;

	
	if (snd_soc_read(codec, M98095_034_DAI2_FORMAT) & M98095_DAI_MAS) {
		if (max98095->sysclk == 0) {
			dev_err(codec->dev, "Invalid system clock frequency\n");
			return -EINVAL;
		}
		ni = 65536ULL * (rate < 50000 ? 96ULL : 48ULL)
				* (unsigned long long int)rate;
		do_div(ni, (unsigned long long int)max98095->sysclk);
		snd_soc_write(codec, M98095_032_DAI2_CLKCFG_HI,
			(ni >> 8) & 0x7F);
		snd_soc_write(codec, M98095_033_DAI2_CLKCFG_LO,
			ni & 0xFF);
	}

	
	if (rate < 50000)
		snd_soc_update_bits(codec, M98095_038_DAI2_FILTERS,
			M98095_DAI_DHF, 0);
	else
		snd_soc_update_bits(codec, M98095_038_DAI2_FILTERS,
			M98095_DAI_DHF, M98095_DAI_DHF);

	return 0;
}

static int max98095_dai3_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);
	struct max98095_cdata *cdata;
	unsigned long long ni;
	unsigned int rate;
	u8 regval;

	cdata = &max98095->dai[2];

	rate = params_rate(params);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		snd_soc_update_bits(codec, M98095_03E_DAI3_FORMAT,
			M98095_DAI_WS, 0);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		snd_soc_update_bits(codec, M98095_03E_DAI3_FORMAT,
			M98095_DAI_WS, M98095_DAI_WS);
		break;
	default:
		return -EINVAL;
	}

	if (rate_value(rate, &regval))
		return -EINVAL;

	snd_soc_update_bits(codec, M98095_03B_DAI3_CLKMODE,
		M98095_CLKMODE_MASK, regval);
	cdata->rate = rate;

	
	if (snd_soc_read(codec, M98095_03E_DAI3_FORMAT) & M98095_DAI_MAS) {
		if (max98095->sysclk == 0) {
			dev_err(codec->dev, "Invalid system clock frequency\n");
			return -EINVAL;
		}
		ni = 65536ULL * (rate < 50000 ? 96ULL : 48ULL)
				* (unsigned long long int)rate;
		do_div(ni, (unsigned long long int)max98095->sysclk);
		snd_soc_write(codec, M98095_03C_DAI3_CLKCFG_HI,
			(ni >> 8) & 0x7F);
		snd_soc_write(codec, M98095_03D_DAI3_CLKCFG_LO,
			ni & 0xFF);
	}

	
	if (rate < 50000)
		snd_soc_update_bits(codec, M98095_042_DAI3_FILTERS,
			M98095_DAI_DHF, 0);
	else
		snd_soc_update_bits(codec, M98095_042_DAI3_FILTERS,
			M98095_DAI_DHF, M98095_DAI_DHF);

	return 0;
}

static int max98095_dai_set_sysclk(struct snd_soc_dai *dai,
				   int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);

	
	if (freq == max98095->sysclk)
		return 0;

	if ((freq >= 10000000) && (freq < 20000000)) {
		snd_soc_write(codec, M98095_026_SYS_CLK, 0x10);
	} else if ((freq >= 20000000) && (freq < 40000000)) {
		snd_soc_write(codec, M98095_026_SYS_CLK, 0x20);
	} else if ((freq >= 40000000) && (freq < 60000000)) {
		snd_soc_write(codec, M98095_026_SYS_CLK, 0x30);
	} else {
		dev_err(codec->dev, "Invalid master clock frequency\n");
		return -EINVAL;
	}

	dev_dbg(dai->dev, "Clock source is %d at %uHz\n", clk_id, freq);

	max98095->sysclk = freq;
	return 0;
}

static int max98095_dai1_set_fmt(struct snd_soc_dai *codec_dai,
				 unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);
	struct max98095_cdata *cdata;
	u8 regval = 0;

	cdata = &max98095->dai[0];

	if (fmt != cdata->fmt) {
		cdata->fmt = fmt;

		switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBS_CFS:
			
			snd_soc_write(codec, M98095_028_DAI1_CLKCFG_HI,
				0x80);
			snd_soc_write(codec, M98095_029_DAI1_CLKCFG_LO,
				0x00);
			break;
		case SND_SOC_DAIFMT_CBM_CFM:
			
			regval |= M98095_DAI_MAS;
			break;
		case SND_SOC_DAIFMT_CBS_CFM:
		case SND_SOC_DAIFMT_CBM_CFS:
		default:
			dev_err(codec->dev, "Clock mode unsupported");
			return -EINVAL;
		}

		switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			regval |= M98095_DAI_DLY;
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			break;
		default:
			return -EINVAL;
		}

		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			break;
		case SND_SOC_DAIFMT_NB_IF:
			regval |= M98095_DAI_WCI;
			break;
		case SND_SOC_DAIFMT_IB_NF:
			regval |= M98095_DAI_BCI;
			break;
		case SND_SOC_DAIFMT_IB_IF:
			regval |= M98095_DAI_BCI|M98095_DAI_WCI;
			break;
		default:
			return -EINVAL;
		}

		snd_soc_update_bits(codec, M98095_02A_DAI1_FORMAT,
			M98095_DAI_MAS | M98095_DAI_DLY | M98095_DAI_BCI |
			M98095_DAI_WCI, regval);

		snd_soc_write(codec, M98095_02B_DAI1_CLOCK, M98095_DAI_BSEL64);
	}

	return 0;
}

static int max98095_dai2_set_fmt(struct snd_soc_dai *codec_dai,
				 unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);
	struct max98095_cdata *cdata;
	u8 regval = 0;

	cdata = &max98095->dai[1];

	if (fmt != cdata->fmt) {
		cdata->fmt = fmt;

		switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBS_CFS:
			
			snd_soc_write(codec, M98095_032_DAI2_CLKCFG_HI,
				0x80);
			snd_soc_write(codec, M98095_033_DAI2_CLKCFG_LO,
				0x00);
			break;
		case SND_SOC_DAIFMT_CBM_CFM:
			
			regval |= M98095_DAI_MAS;
			break;
		case SND_SOC_DAIFMT_CBS_CFM:
		case SND_SOC_DAIFMT_CBM_CFS:
		default:
			dev_err(codec->dev, "Clock mode unsupported");
			return -EINVAL;
		}

		switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			regval |= M98095_DAI_DLY;
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			break;
		default:
			return -EINVAL;
		}

		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			break;
		case SND_SOC_DAIFMT_NB_IF:
			regval |= M98095_DAI_WCI;
			break;
		case SND_SOC_DAIFMT_IB_NF:
			regval |= M98095_DAI_BCI;
			break;
		case SND_SOC_DAIFMT_IB_IF:
			regval |= M98095_DAI_BCI|M98095_DAI_WCI;
			break;
		default:
			return -EINVAL;
		}

		snd_soc_update_bits(codec, M98095_034_DAI2_FORMAT,
			M98095_DAI_MAS | M98095_DAI_DLY | M98095_DAI_BCI |
			M98095_DAI_WCI, regval);

		snd_soc_write(codec, M98095_035_DAI2_CLOCK,
			M98095_DAI_BSEL64);
	}

	return 0;
}

static int max98095_dai3_set_fmt(struct snd_soc_dai *codec_dai,
				 unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);
	struct max98095_cdata *cdata;
	u8 regval = 0;

	cdata = &max98095->dai[2];

	if (fmt != cdata->fmt) {
		cdata->fmt = fmt;

		switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBS_CFS:
			
			snd_soc_write(codec, M98095_03C_DAI3_CLKCFG_HI,
				0x80);
			snd_soc_write(codec, M98095_03D_DAI3_CLKCFG_LO,
				0x00);
			break;
		case SND_SOC_DAIFMT_CBM_CFM:
			
			regval |= M98095_DAI_MAS;
			break;
		case SND_SOC_DAIFMT_CBS_CFM:
		case SND_SOC_DAIFMT_CBM_CFS:
		default:
			dev_err(codec->dev, "Clock mode unsupported");
			return -EINVAL;
		}

		switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			regval |= M98095_DAI_DLY;
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			break;
		default:
			return -EINVAL;
		}

		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			break;
		case SND_SOC_DAIFMT_NB_IF:
			regval |= M98095_DAI_WCI;
			break;
		case SND_SOC_DAIFMT_IB_NF:
			regval |= M98095_DAI_BCI;
			break;
		case SND_SOC_DAIFMT_IB_IF:
			regval |= M98095_DAI_BCI|M98095_DAI_WCI;
			break;
		default:
			return -EINVAL;
		}

		snd_soc_update_bits(codec, M98095_03E_DAI3_FORMAT,
			M98095_DAI_MAS | M98095_DAI_DLY | M98095_DAI_BCI |
			M98095_DAI_WCI, regval);

		snd_soc_write(codec, M98095_03F_DAI3_CLOCK,
			M98095_DAI_BSEL64);
	}

	return 0;
}

static int max98095_set_bias_level(struct snd_soc_codec *codec,
				   enum snd_soc_bias_level level)
{
	int ret;

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		break;

	case SND_SOC_BIAS_STANDBY:
		if (codec->dapm.bias_level == SND_SOC_BIAS_OFF) {
			ret = snd_soc_cache_sync(codec);

			if (ret != 0) {
				dev_err(codec->dev, "Failed to sync cache: %d\n", ret);
				return ret;
			}
		}

		snd_soc_update_bits(codec, M98095_090_PWR_EN_IN,
				M98095_MBEN, M98095_MBEN);
		break;

	case SND_SOC_BIAS_OFF:
		snd_soc_update_bits(codec, M98095_090_PWR_EN_IN,
				M98095_MBEN, 0);
		codec->cache_sync = 1;
		break;
	}
	codec->dapm.bias_level = level;
	return 0;
}

#define MAX98095_RATES SNDRV_PCM_RATE_8000_96000
#define MAX98095_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE)

static const struct snd_soc_dai_ops max98095_dai1_ops = {
	.set_sysclk = max98095_dai_set_sysclk,
	.set_fmt = max98095_dai1_set_fmt,
	.hw_params = max98095_dai1_hw_params,
};

static const struct snd_soc_dai_ops max98095_dai2_ops = {
	.set_sysclk = max98095_dai_set_sysclk,
	.set_fmt = max98095_dai2_set_fmt,
	.hw_params = max98095_dai2_hw_params,
};

static const struct snd_soc_dai_ops max98095_dai3_ops = {
	.set_sysclk = max98095_dai_set_sysclk,
	.set_fmt = max98095_dai3_set_fmt,
	.hw_params = max98095_dai3_hw_params,
};

static struct snd_soc_dai_driver max98095_dai[] = {
{
	.name = "HiFi",
	.playback = {
		.stream_name = "HiFi Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = MAX98095_RATES,
		.formats = MAX98095_FORMATS,
	},
	.capture = {
		.stream_name = "HiFi Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = MAX98095_RATES,
		.formats = MAX98095_FORMATS,
	},
	 .ops = &max98095_dai1_ops,
},
{
	.name = "Aux",
	.playback = {
		.stream_name = "Aux Playback",
		.channels_min = 1,
		.channels_max = 1,
		.rates = MAX98095_RATES,
		.formats = MAX98095_FORMATS,
	},
	.ops = &max98095_dai2_ops,
},
{
	.name = "Voice",
	.playback = {
		.stream_name = "Voice Playback",
		.channels_min = 1,
		.channels_max = 1,
		.rates = MAX98095_RATES,
		.formats = MAX98095_FORMATS,
	},
	.ops = &max98095_dai3_ops,
}

};

static int max98095_get_eq_channel(const char *name)
{
	if (strcmp(name, "EQ1 Mode") == 0)
		return 0;
	if (strcmp(name, "EQ2 Mode") == 0)
		return 1;
	return -EINVAL;
}

static int max98095_put_eq_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);
	struct max98095_pdata *pdata = max98095->pdata;
	int channel = max98095_get_eq_channel(kcontrol->id.name);
	struct max98095_cdata *cdata;
	int sel = ucontrol->value.integer.value[0];
	struct max98095_eq_cfg *coef_set;
	int fs, best, best_val, i;
	int regmask, regsave;

	BUG_ON(channel > 1);

	if (!pdata || !max98095->eq_textcnt)
		return 0;

	if (sel >= pdata->eq_cfgcnt)
		return -EINVAL;

	cdata = &max98095->dai[channel];
	cdata->eq_sel = sel;
	fs = cdata->rate;

	
	best = 0;
	best_val = INT_MAX;
	for (i = 0; i < pdata->eq_cfgcnt; i++) {
		if (strcmp(pdata->eq_cfg[i].name, max98095->eq_texts[sel]) == 0 &&
			abs(pdata->eq_cfg[i].rate - fs) < best_val) {
			best = i;
			best_val = abs(pdata->eq_cfg[i].rate - fs);
		}
	}

	dev_dbg(codec->dev, "Selected %s/%dHz for %dHz sample rate\n",
		pdata->eq_cfg[best].name,
		pdata->eq_cfg[best].rate, fs);

	coef_set = &pdata->eq_cfg[best];

	regmask = (channel == 0) ? M98095_EQ1EN : M98095_EQ2EN;

	
	regsave = snd_soc_read(codec, M98095_088_CFG_LEVEL);
	snd_soc_update_bits(codec, M98095_088_CFG_LEVEL, regmask, 0);

	mutex_lock(&codec->mutex);
	snd_soc_update_bits(codec, M98095_00F_HOST_CFG, M98095_SEG, M98095_SEG);
	m98095_eq_band(codec, channel, 0, coef_set->band1);
	m98095_eq_band(codec, channel, 1, coef_set->band2);
	m98095_eq_band(codec, channel, 2, coef_set->band3);
	m98095_eq_band(codec, channel, 3, coef_set->band4);
	m98095_eq_band(codec, channel, 4, coef_set->band5);
	snd_soc_update_bits(codec, M98095_00F_HOST_CFG, M98095_SEG, 0);
	mutex_unlock(&codec->mutex);

	
	snd_soc_update_bits(codec, M98095_088_CFG_LEVEL, regmask, regsave);
	return 0;
}

static int max98095_get_eq_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);
	int channel = max98095_get_eq_channel(kcontrol->id.name);
	struct max98095_cdata *cdata;

	cdata = &max98095->dai[channel];
	ucontrol->value.enumerated.item[0] = cdata->eq_sel;

	return 0;
}

static void max98095_handle_eq_pdata(struct snd_soc_codec *codec)
{
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);
	struct max98095_pdata *pdata = max98095->pdata;
	struct max98095_eq_cfg *cfg;
	unsigned int cfgcnt;
	int i, j;
	const char **t;
	int ret;

	struct snd_kcontrol_new controls[] = {
		SOC_ENUM_EXT("EQ1 Mode",
			max98095->eq_enum,
			max98095_get_eq_enum,
			max98095_put_eq_enum),
		SOC_ENUM_EXT("EQ2 Mode",
			max98095->eq_enum,
			max98095_get_eq_enum,
			max98095_put_eq_enum),
	};

	cfg = pdata->eq_cfg;
	cfgcnt = pdata->eq_cfgcnt;

	max98095->eq_textcnt = 0;
	max98095->eq_texts = NULL;
	for (i = 0; i < cfgcnt; i++) {
		for (j = 0; j < max98095->eq_textcnt; j++) {
			if (strcmp(cfg[i].name, max98095->eq_texts[j]) == 0)
				break;
		}

		if (j != max98095->eq_textcnt)
			continue;

		
		t = krealloc(max98095->eq_texts,
			     sizeof(char *) * (max98095->eq_textcnt + 1),
			     GFP_KERNEL);
		if (t == NULL)
			continue;

		
		t[max98095->eq_textcnt] = cfg[i].name;
		max98095->eq_textcnt++;
		max98095->eq_texts = t;
	}

	
	max98095->eq_enum.texts = max98095->eq_texts;
	max98095->eq_enum.max = max98095->eq_textcnt;

	ret = snd_soc_add_codec_controls(codec, controls, ARRAY_SIZE(controls));
	if (ret != 0)
		dev_err(codec->dev, "Failed to add EQ control: %d\n", ret);
}

static const char *bq_mode_name[] = {"Biquad1 Mode", "Biquad2 Mode"};

static int max98095_get_bq_channel(struct snd_soc_codec *codec,
				   const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bq_mode_name); i++)
		if (strcmp(name, bq_mode_name[i]) == 0)
			return i;

	
	dev_err(codec->dev, "Bad biquad channel name '%s'\n", name);
	return -EINVAL;
}

static int max98095_put_bq_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);
	struct max98095_pdata *pdata = max98095->pdata;
	int channel = max98095_get_bq_channel(codec, kcontrol->id.name);
	struct max98095_cdata *cdata;
	int sel = ucontrol->value.integer.value[0];
	struct max98095_biquad_cfg *coef_set;
	int fs, best, best_val, i;
	int regmask, regsave;

	if (channel < 0)
		return channel;

	if (!pdata || !max98095->bq_textcnt)
		return 0;

	if (sel >= pdata->bq_cfgcnt)
		return -EINVAL;

	cdata = &max98095->dai[channel];
	cdata->bq_sel = sel;
	fs = cdata->rate;

	
	best = 0;
	best_val = INT_MAX;
	for (i = 0; i < pdata->bq_cfgcnt; i++) {
		if (strcmp(pdata->bq_cfg[i].name, max98095->bq_texts[sel]) == 0 &&
			abs(pdata->bq_cfg[i].rate - fs) < best_val) {
			best = i;
			best_val = abs(pdata->bq_cfg[i].rate - fs);
		}
	}

	dev_dbg(codec->dev, "Selected %s/%dHz for %dHz sample rate\n",
		pdata->bq_cfg[best].name,
		pdata->bq_cfg[best].rate, fs);

	coef_set = &pdata->bq_cfg[best];

	regmask = (channel == 0) ? M98095_BQ1EN : M98095_BQ2EN;

	
	regsave = snd_soc_read(codec, M98095_088_CFG_LEVEL);
	snd_soc_update_bits(codec, M98095_088_CFG_LEVEL, regmask, 0);

	mutex_lock(&codec->mutex);
	snd_soc_update_bits(codec, M98095_00F_HOST_CFG, M98095_SEG, M98095_SEG);
	m98095_biquad_band(codec, channel, 0, coef_set->band1);
	m98095_biquad_band(codec, channel, 1, coef_set->band2);
	snd_soc_update_bits(codec, M98095_00F_HOST_CFG, M98095_SEG, 0);
	mutex_unlock(&codec->mutex);

	
	snd_soc_update_bits(codec, M98095_088_CFG_LEVEL, regmask, regsave);
	return 0;
}

static int max98095_get_bq_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);
	int channel = max98095_get_bq_channel(codec, kcontrol->id.name);
	struct max98095_cdata *cdata;

	if (channel < 0)
		return channel;

	cdata = &max98095->dai[channel];
	ucontrol->value.enumerated.item[0] = cdata->bq_sel;

	return 0;
}

static void max98095_handle_bq_pdata(struct snd_soc_codec *codec)
{
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);
	struct max98095_pdata *pdata = max98095->pdata;
	struct max98095_biquad_cfg *cfg;
	unsigned int cfgcnt;
	int i, j;
	const char **t;
	int ret;

	struct snd_kcontrol_new controls[] = {
		SOC_ENUM_EXT((char *)bq_mode_name[0],
			max98095->bq_enum,
			max98095_get_bq_enum,
			max98095_put_bq_enum),
		SOC_ENUM_EXT((char *)bq_mode_name[1],
			max98095->bq_enum,
			max98095_get_bq_enum,
			max98095_put_bq_enum),
	};
	BUILD_BUG_ON(ARRAY_SIZE(controls) != ARRAY_SIZE(bq_mode_name));

	cfg = pdata->bq_cfg;
	cfgcnt = pdata->bq_cfgcnt;

	max98095->bq_textcnt = 0;
	max98095->bq_texts = NULL;
	for (i = 0; i < cfgcnt; i++) {
		for (j = 0; j < max98095->bq_textcnt; j++) {
			if (strcmp(cfg[i].name, max98095->bq_texts[j]) == 0)
				break;
		}

		if (j != max98095->bq_textcnt)
			continue;

		
		t = krealloc(max98095->bq_texts,
			     sizeof(char *) * (max98095->bq_textcnt + 1),
			     GFP_KERNEL);
		if (t == NULL)
			continue;

		
		t[max98095->bq_textcnt] = cfg[i].name;
		max98095->bq_textcnt++;
		max98095->bq_texts = t;
	}

	
	max98095->bq_enum.texts = max98095->bq_texts;
	max98095->bq_enum.max = max98095->bq_textcnt;

	ret = snd_soc_add_codec_controls(codec, controls, ARRAY_SIZE(controls));
	if (ret != 0)
		dev_err(codec->dev, "Failed to add Biquad control: %d\n", ret);
}

static void max98095_handle_pdata(struct snd_soc_codec *codec)
{
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);
	struct max98095_pdata *pdata = max98095->pdata;
	u8 regval = 0;

	if (!pdata) {
		dev_dbg(codec->dev, "No platform data\n");
		return;
	}

	
	if (pdata->digmic_left_mode)
		regval |= M98095_DIGMIC_L;

	if (pdata->digmic_right_mode)
		regval |= M98095_DIGMIC_R;

	snd_soc_write(codec, M98095_087_CFG_MIC, regval);

	
	if (pdata->eq_cfgcnt)
		max98095_handle_eq_pdata(codec);

	
	if (pdata->bq_cfgcnt)
		max98095_handle_bq_pdata(codec);
}

#ifdef CONFIG_PM
static int max98095_suspend(struct snd_soc_codec *codec)
{
	max98095_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int max98095_resume(struct snd_soc_codec *codec)
{
	max98095_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}
#else
#define max98095_suspend NULL
#define max98095_resume NULL
#endif

static int max98095_reset(struct snd_soc_codec *codec)
{
	int i, ret;

	ret = snd_soc_write(codec, M98095_00F_HOST_CFG, 0);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to reset DSP: %d\n", ret);
		return ret;
	}

	ret = snd_soc_write(codec, M98095_097_PWR_SYS, 0);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to reset codec: %d\n", ret);
		return ret;
	}

	for (i = M98095_010_HOST_INT_CFG; i < M98095_REG_MAX_CACHED; i++) {
		ret = snd_soc_write(codec, i, max98095_reg_def[i]);
		if (ret < 0) {
			dev_err(codec->dev, "Failed to reset: %d\n", ret);
			return ret;
		}
	}

	return ret;
}

static int max98095_probe(struct snd_soc_codec *codec)
{
	struct max98095_priv *max98095 = snd_soc_codec_get_drvdata(codec);
	struct max98095_cdata *cdata;
	int ret = 0;

	ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_I2C);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	
	max98095_reset(codec);

	

	max98095->sysclk = (unsigned)-1;
	max98095->eq_textcnt = 0;
	max98095->bq_textcnt = 0;

	cdata = &max98095->dai[0];
	cdata->rate = (unsigned)-1;
	cdata->fmt  = (unsigned)-1;
	cdata->eq_sel = 0;
	cdata->bq_sel = 0;

	cdata = &max98095->dai[1];
	cdata->rate = (unsigned)-1;
	cdata->fmt  = (unsigned)-1;
	cdata->eq_sel = 0;
	cdata->bq_sel = 0;

	cdata = &max98095->dai[2];
	cdata->rate = (unsigned)-1;
	cdata->fmt  = (unsigned)-1;
	cdata->eq_sel = 0;
	cdata->bq_sel = 0;

	max98095->lin_state = 0;
	max98095->mic1pre = 0;
	max98095->mic2pre = 0;

	ret = snd_soc_read(codec, M98095_0FF_REV_ID);
	if (ret < 0) {
		dev_err(codec->dev, "Failure reading hardware revision: %d\n",
			ret);
		goto err_access;
	}
	dev_info(codec->dev, "Hardware revision: %c\n", ret - 0x40 + 'A');

	snd_soc_write(codec, M98095_097_PWR_SYS, M98095_PWRSV);

	
	max98095_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	snd_soc_write(codec, M98095_048_MIX_DAC_LR,
		M98095_DAI1L_TO_DACL|M98095_DAI1R_TO_DACR);

	snd_soc_write(codec, M98095_049_MIX_DAC_M,
		M98095_DAI2M_TO_DACM|M98095_DAI3M_TO_DACM);

	snd_soc_write(codec, M98095_092_PWR_EN_OUT, M98095_SPK_SPREADSPECTRUM);
	snd_soc_write(codec, M98095_045_CFG_DSP, M98095_DSPNORMAL);
	snd_soc_write(codec, M98095_04E_CFG_HP, M98095_HPNORMAL);

	snd_soc_write(codec, M98095_02C_DAI1_IOCFG,
		M98095_S1NORMAL|M98095_SDATA);

	snd_soc_write(codec, M98095_036_DAI2_IOCFG,
		M98095_S2NORMAL|M98095_SDATA);

	snd_soc_write(codec, M98095_040_DAI3_IOCFG,
		M98095_S3NORMAL|M98095_SDATA);

	max98095_handle_pdata(codec);

	
	snd_soc_update_bits(codec, M98095_097_PWR_SYS, M98095_SHDNRUN,
		M98095_SHDNRUN);

	max98095_add_widgets(codec);

err_access:
	return ret;
}

static int max98095_remove(struct snd_soc_codec *codec)
{
	max98095_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_max98095 = {
	.probe   = max98095_probe,
	.remove  = max98095_remove,
	.suspend = max98095_suspend,
	.resume  = max98095_resume,
	.set_bias_level = max98095_set_bias_level,
	.reg_cache_size = ARRAY_SIZE(max98095_reg_def),
	.reg_word_size = sizeof(u8),
	.reg_cache_default = max98095_reg_def,
	.readable_register = max98095_readable,
	.volatile_register = max98095_volatile,
	.dapm_widgets	  = max98095_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(max98095_dapm_widgets),
	.dapm_routes     = max98095_audio_map,
	.num_dapm_routes = ARRAY_SIZE(max98095_audio_map),
};

static int max98095_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct max98095_priv *max98095;
	int ret;

	max98095 = devm_kzalloc(&i2c->dev, sizeof(struct max98095_priv),
				GFP_KERNEL);
	if (max98095 == NULL)
		return -ENOMEM;

	max98095->devtype = id->driver_data;
	i2c_set_clientdata(i2c, max98095);
	max98095->pdata = i2c->dev.platform_data;

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_max98095,
				     max98095_dai, ARRAY_SIZE(max98095_dai));
	return ret;
}

static int __devexit max98095_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static const struct i2c_device_id max98095_i2c_id[] = {
	{ "max98095", MAX98095 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max98095_i2c_id);

static struct i2c_driver max98095_i2c_driver = {
	.driver = {
		.name = "max98095",
		.owner = THIS_MODULE,
	},
	.probe  = max98095_i2c_probe,
	.remove = __devexit_p(max98095_i2c_remove),
	.id_table = max98095_i2c_id,
};

static int __init max98095_init(void)
{
	int ret;

	ret = i2c_add_driver(&max98095_i2c_driver);
	if (ret)
		pr_err("Failed to register max98095 I2C driver: %d\n", ret);

	return ret;
}
module_init(max98095_init);

static void __exit max98095_exit(void)
{
	i2c_del_driver(&max98095_i2c_driver);
}
module_exit(max98095_exit);

MODULE_DESCRIPTION("ALSA SoC MAX98095 driver");
MODULE_AUTHOR("Peter Hsiang");
MODULE_LICENSE("GPL");
