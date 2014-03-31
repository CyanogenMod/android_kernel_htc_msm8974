/*
 * max98088.c -- MAX98088 ALSA SoC Audio driver
 *
 * Copyright 2010 Maxim Integrated Products
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
#include <sound/max98088.h>
#include "max98088.h"

enum max98088_type {
       MAX98088,
       MAX98089,
};

struct max98088_cdata {
       unsigned int rate;
       unsigned int fmt;
       int eq_sel;
};

struct max98088_priv {
       enum max98088_type devtype;
       struct max98088_pdata *pdata;
       unsigned int sysclk;
       struct max98088_cdata dai[2];
       int eq_textcnt;
       const char **eq_texts;
       struct soc_enum eq_enum;
       u8 ina_state;
       u8 inb_state;
       unsigned int ex_mode;
       unsigned int digmic;
       unsigned int mic1pre;
       unsigned int mic2pre;
       unsigned int extmic_mode;
};

static const u8 max98088_reg[M98088_REG_CNT] = {
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
       0xF0, 
       0x00, 

       0x0F, 
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
       0x70, 
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
       int vol;
} max98088_access[M98088_REG_CNT] = {
       { 0xFF, 0xFF, 1 }, 
       { 0xFF, 0x00, 1 }, 
       { 0xFF, 0x00, 1 }, 
       { 0x1F, 0x1F, 1 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 

       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 

       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 

       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 

       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 

       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 

       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 

       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 

       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 

       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 

       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 

       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 

       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0xFF, 0xFF, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 

       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 

       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 

       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0x00, 0x00, 0 }, 
       { 0xFF, 0x00, 1 }, 
};

static int max98088_volatile_register(struct snd_soc_codec *codec, unsigned int reg)
{
       return max98088_access[reg].vol;
}


static void m98088_eq_band(struct snd_soc_codec *codec, unsigned int dai,
                   unsigned int band, u16 *coefs)
{
       unsigned int eq_reg;
       unsigned int i;

       BUG_ON(band > 4);
       BUG_ON(dai > 1);

       
       eq_reg = dai ? M98088_REG_84_DAI2_EQ_BASE : M98088_REG_52_DAI1_EQ_BASE;

       
       eq_reg += band * (M98088_COEFS_PER_BAND << 1);

       
       for (i = 0; i < M98088_COEFS_PER_BAND; i++) {
               snd_soc_write(codec, eq_reg++, M98088_BYTE1(coefs[i]));
               snd_soc_write(codec, eq_reg++, M98088_BYTE0(coefs[i]));
       }
}

static const char *max98088_exmode_texts[] = {
       "Off", "100Hz", "400Hz", "600Hz", "800Hz", "1000Hz", "200-400Hz",
       "400-600Hz", "400-800Hz",
};

static const unsigned int max98088_exmode_values[] = {
       0x00, 0x43, 0x10, 0x20, 0x30, 0x40, 0x11, 0x22, 0x32
};

static const struct soc_enum max98088_exmode_enum =
       SOC_VALUE_ENUM_SINGLE(M98088_REG_41_SPKDHP, 0, 127,
                             ARRAY_SIZE(max98088_exmode_texts),
                             max98088_exmode_texts,
                             max98088_exmode_values);

static const char *max98088_ex_thresh[] = { 
       "0.6", "1.2", "1.8", "2.4", "3.0", "3.6", "4.2", "4.8"};
static const struct soc_enum max98088_ex_thresh_enum[] = {
       SOC_ENUM_SINGLE(M98088_REG_42_SPKDHP_THRESH, 0, 8,
               max98088_ex_thresh),
};

static const char *max98088_fltr_mode[] = {"Voice", "Music" };
static const struct soc_enum max98088_filter_mode_enum[] = {
       SOC_ENUM_SINGLE(M98088_REG_18_DAI1_FILTERS, 7, 2, max98088_fltr_mode),
};

static const char *max98088_extmic_text[] = { "None", "MIC1", "MIC2" };

static const struct soc_enum max98088_extmic_enum =
       SOC_ENUM_SINGLE(M98088_REG_48_CFG_MIC, 0, 3, max98088_extmic_text);

static const struct snd_kcontrol_new max98088_extmic_mux =
       SOC_DAPM_ENUM("External MIC Mux", max98088_extmic_enum);

static const char *max98088_dai1_fltr[] = {
       "Off", "fc=258/fs=16k", "fc=500/fs=16k",
       "fc=258/fs=8k", "fc=500/fs=8k", "fc=200"};
static const struct soc_enum max98088_dai1_dac_filter_enum[] = {
       SOC_ENUM_SINGLE(M98088_REG_18_DAI1_FILTERS, 0, 6, max98088_dai1_fltr),
};
static const struct soc_enum max98088_dai1_adc_filter_enum[] = {
       SOC_ENUM_SINGLE(M98088_REG_18_DAI1_FILTERS, 4, 6, max98088_dai1_fltr),
};

static int max98088_mic1pre_set(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
       struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
       struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);
       unsigned int sel = ucontrol->value.integer.value[0];

       max98088->mic1pre = sel;
       snd_soc_update_bits(codec, M98088_REG_35_LVL_MIC1, M98088_MICPRE_MASK,
               (1+sel)<<M98088_MICPRE_SHIFT);

       return 0;
}

static int max98088_mic1pre_get(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
       struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
       struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);

       ucontrol->value.integer.value[0] = max98088->mic1pre;
       return 0;
}

static int max98088_mic2pre_set(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
       struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
       struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);
       unsigned int sel = ucontrol->value.integer.value[0];

       max98088->mic2pre = sel;
       snd_soc_update_bits(codec, M98088_REG_36_LVL_MIC2, M98088_MICPRE_MASK,
               (1+sel)<<M98088_MICPRE_SHIFT);

       return 0;
}

static int max98088_mic2pre_get(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
       struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
       struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);

       ucontrol->value.integer.value[0] = max98088->mic2pre;
       return 0;
}

static const unsigned int max98088_micboost_tlv[] = {
       TLV_DB_RANGE_HEAD(2),
       0, 1, TLV_DB_SCALE_ITEM(0, 2000, 0),
       2, 2, TLV_DB_SCALE_ITEM(3000, 0, 0),
};

static const struct snd_kcontrol_new max98088_snd_controls[] = {

       SOC_DOUBLE_R("Headphone Volume", M98088_REG_39_LVL_HP_L,
               M98088_REG_3A_LVL_HP_R, 0, 31, 0),
       SOC_DOUBLE_R("Speaker Volume", M98088_REG_3D_LVL_SPK_L,
               M98088_REG_3E_LVL_SPK_R, 0, 31, 0),
       SOC_DOUBLE_R("Receiver Volume", M98088_REG_3B_LVL_REC_L,
               M98088_REG_3C_LVL_REC_R, 0, 31, 0),

       SOC_DOUBLE_R("Headphone Switch", M98088_REG_39_LVL_HP_L,
               M98088_REG_3A_LVL_HP_R, 7, 1, 1),
       SOC_DOUBLE_R("Speaker Switch", M98088_REG_3D_LVL_SPK_L,
               M98088_REG_3E_LVL_SPK_R, 7, 1, 1),
       SOC_DOUBLE_R("Receiver Switch", M98088_REG_3B_LVL_REC_L,
               M98088_REG_3C_LVL_REC_R, 7, 1, 1),

       SOC_SINGLE("MIC1 Volume", M98088_REG_35_LVL_MIC1, 0, 31, 1),
       SOC_SINGLE("MIC2 Volume", M98088_REG_36_LVL_MIC2, 0, 31, 1),

       SOC_SINGLE_EXT_TLV("MIC1 Boost Volume",
                       M98088_REG_35_LVL_MIC1, 5, 2, 0,
                       max98088_mic1pre_get, max98088_mic1pre_set,
                       max98088_micboost_tlv),
       SOC_SINGLE_EXT_TLV("MIC2 Boost Volume",
                       M98088_REG_36_LVL_MIC2, 5, 2, 0,
                       max98088_mic2pre_get, max98088_mic2pre_set,
                       max98088_micboost_tlv),

       SOC_SINGLE("INA Volume", M98088_REG_37_LVL_INA, 0, 7, 1),
       SOC_SINGLE("INB Volume", M98088_REG_38_LVL_INB, 0, 7, 1),

       SOC_SINGLE("ADCL Volume", M98088_REG_33_LVL_ADC_L, 0, 15, 0),
       SOC_SINGLE("ADCR Volume", M98088_REG_34_LVL_ADC_R, 0, 15, 0),

       SOC_SINGLE("ADCL Boost Volume", M98088_REG_33_LVL_ADC_L, 4, 3, 0),
       SOC_SINGLE("ADCR Boost Volume", M98088_REG_34_LVL_ADC_R, 4, 3, 0),

       SOC_SINGLE("EQ1 Switch", M98088_REG_49_CFG_LEVEL, 0, 1, 0),
       SOC_SINGLE("EQ2 Switch", M98088_REG_49_CFG_LEVEL, 1, 1, 0),

       SOC_ENUM("EX Limiter Mode", max98088_exmode_enum),
       SOC_ENUM("EX Limiter Threshold", max98088_ex_thresh_enum),

       SOC_ENUM("DAI1 Filter Mode", max98088_filter_mode_enum),
       SOC_ENUM("DAI1 DAC Filter", max98088_dai1_dac_filter_enum),
       SOC_ENUM("DAI1 ADC Filter", max98088_dai1_adc_filter_enum),
       SOC_SINGLE("DAI2 DC Block Switch", M98088_REG_20_DAI2_FILTERS,
               0, 1, 0),

       SOC_SINGLE("ALC Switch", M98088_REG_43_SPKALC_COMP, 7, 1, 0),
       SOC_SINGLE("ALC Threshold", M98088_REG_43_SPKALC_COMP, 0, 7, 0),
       SOC_SINGLE("ALC Multiband", M98088_REG_43_SPKALC_COMP, 3, 1, 0),
       SOC_SINGLE("ALC Release Time", M98088_REG_43_SPKALC_COMP, 4, 7, 0),

       SOC_SINGLE("PWR Limiter Threshold", M98088_REG_44_PWRLMT_CFG,
               4, 15, 0),
       SOC_SINGLE("PWR Limiter Weight", M98088_REG_44_PWRLMT_CFG, 0, 7, 0),
       SOC_SINGLE("PWR Limiter Time1", M98088_REG_45_PWRLMT_TIME, 0, 15, 0),
       SOC_SINGLE("PWR Limiter Time2", M98088_REG_45_PWRLMT_TIME, 4, 15, 0),

       SOC_SINGLE("THD Limiter Threshold", M98088_REG_46_THDLMT_CFG, 4, 15, 0),
       SOC_SINGLE("THD Limiter Time", M98088_REG_46_THDLMT_CFG, 0, 7, 0),
};

static const struct snd_kcontrol_new max98088_left_speaker_mixer_controls[] = {
       SOC_DAPM_SINGLE("Left DAC1 Switch", M98088_REG_2B_MIX_SPK_LEFT, 0, 1, 0),
       SOC_DAPM_SINGLE("Right DAC1 Switch", M98088_REG_2B_MIX_SPK_LEFT, 7, 1, 0),
       SOC_DAPM_SINGLE("Left DAC2 Switch", M98088_REG_2B_MIX_SPK_LEFT, 0, 1, 0),
       SOC_DAPM_SINGLE("Right DAC2 Switch", M98088_REG_2B_MIX_SPK_LEFT, 7, 1, 0),
       SOC_DAPM_SINGLE("MIC1 Switch", M98088_REG_2B_MIX_SPK_LEFT, 5, 1, 0),
       SOC_DAPM_SINGLE("MIC2 Switch", M98088_REG_2B_MIX_SPK_LEFT, 6, 1, 0),
       SOC_DAPM_SINGLE("INA1 Switch", M98088_REG_2B_MIX_SPK_LEFT, 1, 1, 0),
       SOC_DAPM_SINGLE("INA2 Switch", M98088_REG_2B_MIX_SPK_LEFT, 2, 1, 0),
       SOC_DAPM_SINGLE("INB1 Switch", M98088_REG_2B_MIX_SPK_LEFT, 3, 1, 0),
       SOC_DAPM_SINGLE("INB2 Switch", M98088_REG_2B_MIX_SPK_LEFT, 4, 1, 0),
};

static const struct snd_kcontrol_new max98088_right_speaker_mixer_controls[] = {
       SOC_DAPM_SINGLE("Left DAC1 Switch", M98088_REG_2C_MIX_SPK_RIGHT, 7, 1, 0),
       SOC_DAPM_SINGLE("Right DAC1 Switch", M98088_REG_2C_MIX_SPK_RIGHT, 0, 1, 0),
       SOC_DAPM_SINGLE("Left DAC2 Switch", M98088_REG_2C_MIX_SPK_RIGHT, 7, 1, 0),
       SOC_DAPM_SINGLE("Right DAC2 Switch", M98088_REG_2C_MIX_SPK_RIGHT, 0, 1, 0),
       SOC_DAPM_SINGLE("MIC1 Switch", M98088_REG_2C_MIX_SPK_RIGHT, 5, 1, 0),
       SOC_DAPM_SINGLE("MIC2 Switch", M98088_REG_2C_MIX_SPK_RIGHT, 6, 1, 0),
       SOC_DAPM_SINGLE("INA1 Switch", M98088_REG_2C_MIX_SPK_RIGHT, 1, 1, 0),
       SOC_DAPM_SINGLE("INA2 Switch", M98088_REG_2C_MIX_SPK_RIGHT, 2, 1, 0),
       SOC_DAPM_SINGLE("INB1 Switch", M98088_REG_2C_MIX_SPK_RIGHT, 3, 1, 0),
       SOC_DAPM_SINGLE("INB2 Switch", M98088_REG_2C_MIX_SPK_RIGHT, 4, 1, 0),
};

static const struct snd_kcontrol_new max98088_left_hp_mixer_controls[] = {
       SOC_DAPM_SINGLE("Left DAC1 Switch", M98088_REG_25_MIX_HP_LEFT, 0, 1, 0),
       SOC_DAPM_SINGLE("Right DAC1 Switch", M98088_REG_25_MIX_HP_LEFT, 7, 1, 0),
       SOC_DAPM_SINGLE("Left DAC2 Switch", M98088_REG_25_MIX_HP_LEFT, 0, 1, 0),
       SOC_DAPM_SINGLE("Right DAC2 Switch", M98088_REG_25_MIX_HP_LEFT, 7, 1, 0),
       SOC_DAPM_SINGLE("MIC1 Switch", M98088_REG_25_MIX_HP_LEFT, 5, 1, 0),
       SOC_DAPM_SINGLE("MIC2 Switch", M98088_REG_25_MIX_HP_LEFT, 6, 1, 0),
       SOC_DAPM_SINGLE("INA1 Switch", M98088_REG_25_MIX_HP_LEFT, 1, 1, 0),
       SOC_DAPM_SINGLE("INA2 Switch", M98088_REG_25_MIX_HP_LEFT, 2, 1, 0),
       SOC_DAPM_SINGLE("INB1 Switch", M98088_REG_25_MIX_HP_LEFT, 3, 1, 0),
       SOC_DAPM_SINGLE("INB2 Switch", M98088_REG_25_MIX_HP_LEFT, 4, 1, 0),
};

static const struct snd_kcontrol_new max98088_right_hp_mixer_controls[] = {
       SOC_DAPM_SINGLE("Left DAC1 Switch", M98088_REG_26_MIX_HP_RIGHT, 7, 1, 0),
       SOC_DAPM_SINGLE("Right DAC1 Switch", M98088_REG_26_MIX_HP_RIGHT, 0, 1, 0),
       SOC_DAPM_SINGLE("Left DAC2 Switch", M98088_REG_26_MIX_HP_RIGHT, 7, 1, 0),
       SOC_DAPM_SINGLE("Right DAC2 Switch", M98088_REG_26_MIX_HP_RIGHT, 0, 1, 0),
       SOC_DAPM_SINGLE("MIC1 Switch", M98088_REG_26_MIX_HP_RIGHT, 5, 1, 0),
       SOC_DAPM_SINGLE("MIC2 Switch", M98088_REG_26_MIX_HP_RIGHT, 6, 1, 0),
       SOC_DAPM_SINGLE("INA1 Switch", M98088_REG_26_MIX_HP_RIGHT, 1, 1, 0),
       SOC_DAPM_SINGLE("INA2 Switch", M98088_REG_26_MIX_HP_RIGHT, 2, 1, 0),
       SOC_DAPM_SINGLE("INB1 Switch", M98088_REG_26_MIX_HP_RIGHT, 3, 1, 0),
       SOC_DAPM_SINGLE("INB2 Switch", M98088_REG_26_MIX_HP_RIGHT, 4, 1, 0),
};

static const struct snd_kcontrol_new max98088_left_rec_mixer_controls[] = {
       SOC_DAPM_SINGLE("Left DAC1 Switch", M98088_REG_28_MIX_REC_LEFT, 0, 1, 0),
       SOC_DAPM_SINGLE("Right DAC1 Switch", M98088_REG_28_MIX_REC_LEFT, 7, 1, 0),
       SOC_DAPM_SINGLE("Left DAC2 Switch", M98088_REG_28_MIX_REC_LEFT, 0, 1, 0),
       SOC_DAPM_SINGLE("Right DAC2 Switch", M98088_REG_28_MIX_REC_LEFT, 7, 1, 0),
       SOC_DAPM_SINGLE("MIC1 Switch", M98088_REG_28_MIX_REC_LEFT, 5, 1, 0),
       SOC_DAPM_SINGLE("MIC2 Switch", M98088_REG_28_MIX_REC_LEFT, 6, 1, 0),
       SOC_DAPM_SINGLE("INA1 Switch", M98088_REG_28_MIX_REC_LEFT, 1, 1, 0),
       SOC_DAPM_SINGLE("INA2 Switch", M98088_REG_28_MIX_REC_LEFT, 2, 1, 0),
       SOC_DAPM_SINGLE("INB1 Switch", M98088_REG_28_MIX_REC_LEFT, 3, 1, 0),
       SOC_DAPM_SINGLE("INB2 Switch", M98088_REG_28_MIX_REC_LEFT, 4, 1, 0),
};

static const struct snd_kcontrol_new max98088_right_rec_mixer_controls[] = {
       SOC_DAPM_SINGLE("Left DAC1 Switch", M98088_REG_29_MIX_REC_RIGHT, 7, 1, 0),
       SOC_DAPM_SINGLE("Right DAC1 Switch", M98088_REG_29_MIX_REC_RIGHT, 0, 1, 0),
       SOC_DAPM_SINGLE("Left DAC2 Switch", M98088_REG_29_MIX_REC_RIGHT, 7, 1, 0),
       SOC_DAPM_SINGLE("Right DAC2 Switch", M98088_REG_29_MIX_REC_RIGHT, 0, 1, 0),
       SOC_DAPM_SINGLE("MIC1 Switch", M98088_REG_29_MIX_REC_RIGHT, 5, 1, 0),
       SOC_DAPM_SINGLE("MIC2 Switch", M98088_REG_29_MIX_REC_RIGHT, 6, 1, 0),
       SOC_DAPM_SINGLE("INA1 Switch", M98088_REG_29_MIX_REC_RIGHT, 1, 1, 0),
       SOC_DAPM_SINGLE("INA2 Switch", M98088_REG_29_MIX_REC_RIGHT, 2, 1, 0),
       SOC_DAPM_SINGLE("INB1 Switch", M98088_REG_29_MIX_REC_RIGHT, 3, 1, 0),
       SOC_DAPM_SINGLE("INB2 Switch", M98088_REG_29_MIX_REC_RIGHT, 4, 1, 0),
};

static const struct snd_kcontrol_new max98088_left_ADC_mixer_controls[] = {
       SOC_DAPM_SINGLE("MIC1 Switch", M98088_REG_23_MIX_ADC_LEFT, 7, 1, 0),
       SOC_DAPM_SINGLE("MIC2 Switch", M98088_REG_23_MIX_ADC_LEFT, 6, 1, 0),
       SOC_DAPM_SINGLE("INA1 Switch", M98088_REG_23_MIX_ADC_LEFT, 3, 1, 0),
       SOC_DAPM_SINGLE("INA2 Switch", M98088_REG_23_MIX_ADC_LEFT, 2, 1, 0),
       SOC_DAPM_SINGLE("INB1 Switch", M98088_REG_23_MIX_ADC_LEFT, 1, 1, 0),
       SOC_DAPM_SINGLE("INB2 Switch", M98088_REG_23_MIX_ADC_LEFT, 0, 1, 0),
};

static const struct snd_kcontrol_new max98088_right_ADC_mixer_controls[] = {
       SOC_DAPM_SINGLE("MIC1 Switch", M98088_REG_24_MIX_ADC_RIGHT, 7, 1, 0),
       SOC_DAPM_SINGLE("MIC2 Switch", M98088_REG_24_MIX_ADC_RIGHT, 6, 1, 0),
       SOC_DAPM_SINGLE("INA1 Switch", M98088_REG_24_MIX_ADC_RIGHT, 3, 1, 0),
       SOC_DAPM_SINGLE("INA2 Switch", M98088_REG_24_MIX_ADC_RIGHT, 2, 1, 0),
       SOC_DAPM_SINGLE("INB1 Switch", M98088_REG_24_MIX_ADC_RIGHT, 1, 1, 0),
       SOC_DAPM_SINGLE("INB2 Switch", M98088_REG_24_MIX_ADC_RIGHT, 0, 1, 0),
};

static int max98088_mic_event(struct snd_soc_dapm_widget *w,
                            struct snd_kcontrol *kcontrol, int event)
{
       struct snd_soc_codec *codec = w->codec;
       struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);

       switch (event) {
       case SND_SOC_DAPM_POST_PMU:
               if (w->reg == M98088_REG_35_LVL_MIC1) {
                       snd_soc_update_bits(codec, w->reg, M98088_MICPRE_MASK,
                               (1+max98088->mic1pre)<<M98088_MICPRE_SHIFT);
               } else {
                       snd_soc_update_bits(codec, w->reg, M98088_MICPRE_MASK,
                               (1+max98088->mic2pre)<<M98088_MICPRE_SHIFT);
               }
               break;
       case SND_SOC_DAPM_POST_PMD:
               snd_soc_update_bits(codec, w->reg, M98088_MICPRE_MASK, 0);
               break;
       default:
               return -EINVAL;
       }

       return 0;
}

static int max98088_line_pga(struct snd_soc_dapm_widget *w,
                            int event, int line, u8 channel)
{
       struct snd_soc_codec *codec = w->codec;
       struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);
       u8 *state;

       BUG_ON(!((channel == 1) || (channel == 2)));

       switch (line) {
       case LINE_INA:
               state = &max98088->ina_state;
               break;
       case LINE_INB:
               state = &max98088->inb_state;
               break;
       default:
               return -EINVAL;
       }

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

static int max98088_pga_ina1_event(struct snd_soc_dapm_widget *w,
                                  struct snd_kcontrol *k, int event)
{
       return max98088_line_pga(w, event, LINE_INA, 1);
}

static int max98088_pga_ina2_event(struct snd_soc_dapm_widget *w,
                                  struct snd_kcontrol *k, int event)
{
       return max98088_line_pga(w, event, LINE_INA, 2);
}

static int max98088_pga_inb1_event(struct snd_soc_dapm_widget *w,
                                  struct snd_kcontrol *k, int event)
{
       return max98088_line_pga(w, event, LINE_INB, 1);
}

static int max98088_pga_inb2_event(struct snd_soc_dapm_widget *w,
                                  struct snd_kcontrol *k, int event)
{
       return max98088_line_pga(w, event, LINE_INB, 2);
}

static const struct snd_soc_dapm_widget max98088_dapm_widgets[] = {

       SND_SOC_DAPM_ADC("ADCL", "HiFi Capture", M98088_REG_4C_PWR_EN_IN, 1, 0),
       SND_SOC_DAPM_ADC("ADCR", "HiFi Capture", M98088_REG_4C_PWR_EN_IN, 0, 0),

       SND_SOC_DAPM_DAC("DACL1", "HiFi Playback",
               M98088_REG_4D_PWR_EN_OUT, 1, 0),
       SND_SOC_DAPM_DAC("DACR1", "HiFi Playback",
               M98088_REG_4D_PWR_EN_OUT, 0, 0),
       SND_SOC_DAPM_DAC("DACL2", "Aux Playback",
               M98088_REG_4D_PWR_EN_OUT, 1, 0),
       SND_SOC_DAPM_DAC("DACR2", "Aux Playback",
               M98088_REG_4D_PWR_EN_OUT, 0, 0),

       SND_SOC_DAPM_PGA("HP Left Out", M98088_REG_4D_PWR_EN_OUT,
               7, 0, NULL, 0),
       SND_SOC_DAPM_PGA("HP Right Out", M98088_REG_4D_PWR_EN_OUT,
               6, 0, NULL, 0),

       SND_SOC_DAPM_PGA("SPK Left Out", M98088_REG_4D_PWR_EN_OUT,
               5, 0, NULL, 0),
       SND_SOC_DAPM_PGA("SPK Right Out", M98088_REG_4D_PWR_EN_OUT,
               4, 0, NULL, 0),

       SND_SOC_DAPM_PGA("REC Left Out", M98088_REG_4D_PWR_EN_OUT,
               3, 0, NULL, 0),
       SND_SOC_DAPM_PGA("REC Right Out", M98088_REG_4D_PWR_EN_OUT,
               2, 0, NULL, 0),

       SND_SOC_DAPM_MUX("External MIC", SND_SOC_NOPM, 0, 0,
               &max98088_extmic_mux),

       SND_SOC_DAPM_MIXER("Left HP Mixer", SND_SOC_NOPM, 0, 0,
               &max98088_left_hp_mixer_controls[0],
               ARRAY_SIZE(max98088_left_hp_mixer_controls)),

       SND_SOC_DAPM_MIXER("Right HP Mixer", SND_SOC_NOPM, 0, 0,
               &max98088_right_hp_mixer_controls[0],
               ARRAY_SIZE(max98088_right_hp_mixer_controls)),

       SND_SOC_DAPM_MIXER("Left SPK Mixer", SND_SOC_NOPM, 0, 0,
               &max98088_left_speaker_mixer_controls[0],
               ARRAY_SIZE(max98088_left_speaker_mixer_controls)),

       SND_SOC_DAPM_MIXER("Right SPK Mixer", SND_SOC_NOPM, 0, 0,
               &max98088_right_speaker_mixer_controls[0],
               ARRAY_SIZE(max98088_right_speaker_mixer_controls)),

       SND_SOC_DAPM_MIXER("Left REC Mixer", SND_SOC_NOPM, 0, 0,
         &max98088_left_rec_mixer_controls[0],
               ARRAY_SIZE(max98088_left_rec_mixer_controls)),

       SND_SOC_DAPM_MIXER("Right REC Mixer", SND_SOC_NOPM, 0, 0,
         &max98088_right_rec_mixer_controls[0],
               ARRAY_SIZE(max98088_right_rec_mixer_controls)),

       SND_SOC_DAPM_MIXER("Left ADC Mixer", SND_SOC_NOPM, 0, 0,
               &max98088_left_ADC_mixer_controls[0],
               ARRAY_SIZE(max98088_left_ADC_mixer_controls)),

       SND_SOC_DAPM_MIXER("Right ADC Mixer", SND_SOC_NOPM, 0, 0,
               &max98088_right_ADC_mixer_controls[0],
               ARRAY_SIZE(max98088_right_ADC_mixer_controls)),

       SND_SOC_DAPM_PGA_E("MIC1 Input", M98088_REG_35_LVL_MIC1,
               5, 0, NULL, 0, max98088_mic_event,
               SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

       SND_SOC_DAPM_PGA_E("MIC2 Input", M98088_REG_36_LVL_MIC2,
               5, 0, NULL, 0, max98088_mic_event,
               SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

       SND_SOC_DAPM_PGA_E("INA1 Input", M98088_REG_4C_PWR_EN_IN,
               7, 0, NULL, 0, max98088_pga_ina1_event,
               SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

       SND_SOC_DAPM_PGA_E("INA2 Input", M98088_REG_4C_PWR_EN_IN,
               7, 0, NULL, 0, max98088_pga_ina2_event,
               SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

       SND_SOC_DAPM_PGA_E("INB1 Input", M98088_REG_4C_PWR_EN_IN,
               6, 0, NULL, 0, max98088_pga_inb1_event,
               SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

       SND_SOC_DAPM_PGA_E("INB2 Input", M98088_REG_4C_PWR_EN_IN,
               6, 0, NULL, 0, max98088_pga_inb2_event,
               SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

       SND_SOC_DAPM_MICBIAS("MICBIAS", M98088_REG_4C_PWR_EN_IN, 3, 0),

       SND_SOC_DAPM_OUTPUT("HPL"),
       SND_SOC_DAPM_OUTPUT("HPR"),
       SND_SOC_DAPM_OUTPUT("SPKL"),
       SND_SOC_DAPM_OUTPUT("SPKR"),
       SND_SOC_DAPM_OUTPUT("RECL"),
       SND_SOC_DAPM_OUTPUT("RECR"),

       SND_SOC_DAPM_INPUT("MIC1"),
       SND_SOC_DAPM_INPUT("MIC2"),
       SND_SOC_DAPM_INPUT("INA1"),
       SND_SOC_DAPM_INPUT("INA2"),
       SND_SOC_DAPM_INPUT("INB1"),
       SND_SOC_DAPM_INPUT("INB2"),
};

static const struct snd_soc_dapm_route max98088_audio_map[] = {
       
       {"Left HP Mixer", "Left DAC1 Switch", "DACL1"},
       {"Left HP Mixer", "Left DAC2 Switch", "DACL2"},
       {"Left HP Mixer", "Right DAC1 Switch", "DACR1"},
       {"Left HP Mixer", "Right DAC2 Switch", "DACR2"},
       {"Left HP Mixer", "MIC1 Switch", "MIC1 Input"},
       {"Left HP Mixer", "MIC2 Switch", "MIC2 Input"},
       {"Left HP Mixer", "INA1 Switch", "INA1 Input"},
       {"Left HP Mixer", "INA2 Switch", "INA2 Input"},
       {"Left HP Mixer", "INB1 Switch", "INB1 Input"},
       {"Left HP Mixer", "INB2 Switch", "INB2 Input"},

       
       {"Right HP Mixer", "Left DAC1 Switch", "DACL1"},
       {"Right HP Mixer", "Left DAC2 Switch", "DACL2"  },
       {"Right HP Mixer", "Right DAC1 Switch", "DACR1"},
       {"Right HP Mixer", "Right DAC2 Switch", "DACR2"},
       {"Right HP Mixer", "MIC1 Switch", "MIC1 Input"},
       {"Right HP Mixer", "MIC2 Switch", "MIC2 Input"},
       {"Right HP Mixer", "INA1 Switch", "INA1 Input"},
       {"Right HP Mixer", "INA2 Switch", "INA2 Input"},
       {"Right HP Mixer", "INB1 Switch", "INB1 Input"},
       {"Right HP Mixer", "INB2 Switch", "INB2 Input"},

       
       {"Left SPK Mixer", "Left DAC1 Switch", "DACL1"},
       {"Left SPK Mixer", "Left DAC2 Switch", "DACL2"},
       {"Left SPK Mixer", "Right DAC1 Switch", "DACR1"},
       {"Left SPK Mixer", "Right DAC2 Switch", "DACR2"},
       {"Left SPK Mixer", "MIC1 Switch", "MIC1 Input"},
       {"Left SPK Mixer", "MIC2 Switch", "MIC2 Input"},
       {"Left SPK Mixer", "INA1 Switch", "INA1 Input"},
       {"Left SPK Mixer", "INA2 Switch", "INA2 Input"},
       {"Left SPK Mixer", "INB1 Switch", "INB1 Input"},
       {"Left SPK Mixer", "INB2 Switch", "INB2 Input"},

       
       {"Right SPK Mixer", "Left DAC1 Switch", "DACL1"},
       {"Right SPK Mixer", "Left DAC2 Switch", "DACL2"},
       {"Right SPK Mixer", "Right DAC1 Switch", "DACR1"},
       {"Right SPK Mixer", "Right DAC2 Switch", "DACR2"},
       {"Right SPK Mixer", "MIC1 Switch", "MIC1 Input"},
       {"Right SPK Mixer", "MIC2 Switch", "MIC2 Input"},
       {"Right SPK Mixer", "INA1 Switch", "INA1 Input"},
       {"Right SPK Mixer", "INA2 Switch", "INA2 Input"},
       {"Right SPK Mixer", "INB1 Switch", "INB1 Input"},
       {"Right SPK Mixer", "INB2 Switch", "INB2 Input"},

       
       {"Left REC Mixer", "Left DAC1 Switch", "DACL1"},
       {"Left REC Mixer", "Left DAC2 Switch", "DACL2"},
       {"Left REC Mixer", "Right DAC1 Switch", "DACR1"},
       {"Left REC Mixer", "Right DAC2 Switch", "DACR2"},
       {"Left REC Mixer", "MIC1 Switch", "MIC1 Input"},
       {"Left REC Mixer", "MIC2 Switch", "MIC2 Input"},
       {"Left REC Mixer", "INA1 Switch", "INA1 Input"},
       {"Left REC Mixer", "INA2 Switch", "INA2 Input"},
       {"Left REC Mixer", "INB1 Switch", "INB1 Input"},
       {"Left REC Mixer", "INB2 Switch", "INB2 Input"},

       
       {"Right REC Mixer", "Left DAC1 Switch", "DACL1"},
       {"Right REC Mixer", "Left DAC2 Switch", "DACL2"},
       {"Right REC Mixer", "Right DAC1 Switch", "DACR1"},
       {"Right REC Mixer", "Right DAC2 Switch", "DACR2"},
       {"Right REC Mixer", "MIC1 Switch", "MIC1 Input"},
       {"Right REC Mixer", "MIC2 Switch", "MIC2 Input"},
       {"Right REC Mixer", "INA1 Switch", "INA1 Input"},
       {"Right REC Mixer", "INA2 Switch", "INA2 Input"},
       {"Right REC Mixer", "INB1 Switch", "INB1 Input"},
       {"Right REC Mixer", "INB2 Switch", "INB2 Input"},

       {"HP Left Out", NULL, "Left HP Mixer"},
       {"HP Right Out", NULL, "Right HP Mixer"},
       {"SPK Left Out", NULL, "Left SPK Mixer"},
       {"SPK Right Out", NULL, "Right SPK Mixer"},
       {"REC Left Out", NULL, "Left REC Mixer"},
       {"REC Right Out", NULL, "Right REC Mixer"},

       {"HPL", NULL, "HP Left Out"},
       {"HPR", NULL, "HP Right Out"},
       {"SPKL", NULL, "SPK Left Out"},
       {"SPKR", NULL, "SPK Right Out"},
       {"RECL", NULL, "REC Left Out"},
       {"RECR", NULL, "REC Right Out"},

       
       {"Left ADC Mixer", "MIC1 Switch", "MIC1 Input"},
       {"Left ADC Mixer", "MIC2 Switch", "MIC2 Input"},
       {"Left ADC Mixer", "INA1 Switch", "INA1 Input"},
       {"Left ADC Mixer", "INA2 Switch", "INA2 Input"},
       {"Left ADC Mixer", "INB1 Switch", "INB1 Input"},
       {"Left ADC Mixer", "INB2 Switch", "INB2 Input"},

       
       {"Right ADC Mixer", "MIC1 Switch", "MIC1 Input"},
       {"Right ADC Mixer", "MIC2 Switch", "MIC2 Input"},
       {"Right ADC Mixer", "INA1 Switch", "INA1 Input"},
       {"Right ADC Mixer", "INA2 Switch", "INA2 Input"},
       {"Right ADC Mixer", "INB1 Switch", "INB1 Input"},
       {"Right ADC Mixer", "INB2 Switch", "INB2 Input"},

       
       {"ADCL", NULL, "Left ADC Mixer"},
       {"ADCR", NULL, "Right ADC Mixer"},
       {"INA1 Input", NULL, "INA1"},
       {"INA2 Input", NULL, "INA2"},
       {"INB1 Input", NULL, "INB1"},
       {"INB2 Input", NULL, "INB2"},
       {"MIC1 Input", NULL, "MIC1"},
       {"MIC2 Input", NULL, "MIC2"},
};

static const struct {
       u32 rate;
       u8  sr;
} rate_table[] = {
       {8000,  0x10},
       {11025, 0x20},
       {16000, 0x30},
       {22050, 0x40},
       {24000, 0x50},
       {32000, 0x60},
       {44100, 0x70},
       {48000, 0x80},
       {88200, 0x90},
       {96000, 0xA0},
};

static inline int rate_value(int rate, u8 *value)
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

static int max98088_dai1_hw_params(struct snd_pcm_substream *substream,
                                  struct snd_pcm_hw_params *params,
                                  struct snd_soc_dai *dai)
{
       struct snd_soc_codec *codec = dai->codec;
       struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);
       struct max98088_cdata *cdata;
       unsigned long long ni;
       unsigned int rate;
       u8 regval;

       cdata = &max98088->dai[0];

       rate = params_rate(params);

       switch (params_format(params)) {
       case SNDRV_PCM_FORMAT_S16_LE:
               snd_soc_update_bits(codec, M98088_REG_14_DAI1_FORMAT,
                       M98088_DAI_WS, 0);
               break;
       case SNDRV_PCM_FORMAT_S24_LE:
               snd_soc_update_bits(codec, M98088_REG_14_DAI1_FORMAT,
                       M98088_DAI_WS, M98088_DAI_WS);
               break;
       default:
               return -EINVAL;
       }

       snd_soc_update_bits(codec, M98088_REG_51_PWR_SYS, M98088_SHDNRUN, 0);

       if (rate_value(rate, &regval))
               return -EINVAL;

       snd_soc_update_bits(codec, M98088_REG_11_DAI1_CLKMODE,
               M98088_CLKMODE_MASK, regval);
       cdata->rate = rate;

       
       if (snd_soc_read(codec, M98088_REG_14_DAI1_FORMAT)
               & M98088_DAI_MAS) {
               if (max98088->sysclk == 0) {
                       dev_err(codec->dev, "Invalid system clock frequency\n");
                       return -EINVAL;
               }
               ni = 65536ULL * (rate < 50000 ? 96ULL : 48ULL)
                               * (unsigned long long int)rate;
               do_div(ni, (unsigned long long int)max98088->sysclk);
               snd_soc_write(codec, M98088_REG_12_DAI1_CLKCFG_HI,
                       (ni >> 8) & 0x7F);
               snd_soc_write(codec, M98088_REG_13_DAI1_CLKCFG_LO,
                       ni & 0xFF);
       }

       
       if (rate < 50000)
               snd_soc_update_bits(codec, M98088_REG_18_DAI1_FILTERS,
                       M98088_DAI_DHF, 0);
       else
               snd_soc_update_bits(codec, M98088_REG_18_DAI1_FILTERS,
                       M98088_DAI_DHF, M98088_DAI_DHF);

       snd_soc_update_bits(codec, M98088_REG_51_PWR_SYS, M98088_SHDNRUN,
               M98088_SHDNRUN);

       return 0;
}

static int max98088_dai2_hw_params(struct snd_pcm_substream *substream,
                                  struct snd_pcm_hw_params *params,
                                  struct snd_soc_dai *dai)
{
       struct snd_soc_codec *codec = dai->codec;
       struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);
       struct max98088_cdata *cdata;
       unsigned long long ni;
       unsigned int rate;
       u8 regval;

       cdata = &max98088->dai[1];

       rate = params_rate(params);

       switch (params_format(params)) {
       case SNDRV_PCM_FORMAT_S16_LE:
               snd_soc_update_bits(codec, M98088_REG_1C_DAI2_FORMAT,
                       M98088_DAI_WS, 0);
               break;
       case SNDRV_PCM_FORMAT_S24_LE:
               snd_soc_update_bits(codec, M98088_REG_1C_DAI2_FORMAT,
                       M98088_DAI_WS, M98088_DAI_WS);
               break;
       default:
               return -EINVAL;
       }

       snd_soc_update_bits(codec, M98088_REG_51_PWR_SYS, M98088_SHDNRUN, 0);

       if (rate_value(rate, &regval))
               return -EINVAL;

       snd_soc_update_bits(codec, M98088_REG_19_DAI2_CLKMODE,
               M98088_CLKMODE_MASK, regval);
       cdata->rate = rate;

       
       if (snd_soc_read(codec, M98088_REG_1C_DAI2_FORMAT)
               & M98088_DAI_MAS) {
               if (max98088->sysclk == 0) {
                       dev_err(codec->dev, "Invalid system clock frequency\n");
                       return -EINVAL;
               }
               ni = 65536ULL * (rate < 50000 ? 96ULL : 48ULL)
                               * (unsigned long long int)rate;
               do_div(ni, (unsigned long long int)max98088->sysclk);
               snd_soc_write(codec, M98088_REG_1A_DAI2_CLKCFG_HI,
                       (ni >> 8) & 0x7F);
               snd_soc_write(codec, M98088_REG_1B_DAI2_CLKCFG_LO,
                       ni & 0xFF);
       }

       
       if (rate < 50000)
               snd_soc_update_bits(codec, M98088_REG_20_DAI2_FILTERS,
                       M98088_DAI_DHF, 0);
       else
               snd_soc_update_bits(codec, M98088_REG_20_DAI2_FILTERS,
                       M98088_DAI_DHF, M98088_DAI_DHF);

       snd_soc_update_bits(codec, M98088_REG_51_PWR_SYS, M98088_SHDNRUN,
               M98088_SHDNRUN);

       return 0;
}

static int max98088_dai_set_sysclk(struct snd_soc_dai *dai,
                                  int clk_id, unsigned int freq, int dir)
{
       struct snd_soc_codec *codec = dai->codec;
       struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);

       
       if (freq == max98088->sysclk)
               return 0;

       if ((freq >= 10000000) && (freq < 20000000)) {
               snd_soc_write(codec, M98088_REG_10_SYS_CLK, 0x10);
       } else if ((freq >= 20000000) && (freq < 30000000)) {
               snd_soc_write(codec, M98088_REG_10_SYS_CLK, 0x20);
       } else {
               dev_err(codec->dev, "Invalid master clock frequency\n");
               return -EINVAL;
       }

       if (snd_soc_read(codec, M98088_REG_51_PWR_SYS)  & M98088_SHDNRUN) {
               snd_soc_update_bits(codec, M98088_REG_51_PWR_SYS,
                       M98088_SHDNRUN, 0);
               snd_soc_update_bits(codec, M98088_REG_51_PWR_SYS,
                       M98088_SHDNRUN, M98088_SHDNRUN);
       }

       dev_dbg(dai->dev, "Clock source is %d at %uHz\n", clk_id, freq);

       max98088->sysclk = freq;
       return 0;
}

static int max98088_dai1_set_fmt(struct snd_soc_dai *codec_dai,
                                unsigned int fmt)
{
       struct snd_soc_codec *codec = codec_dai->codec;
       struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);
       struct max98088_cdata *cdata;
       u8 reg15val;
       u8 reg14val = 0;

       cdata = &max98088->dai[0];

       if (fmt != cdata->fmt) {
               cdata->fmt = fmt;

               switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
               case SND_SOC_DAIFMT_CBS_CFS:
                       
                       snd_soc_write(codec, M98088_REG_12_DAI1_CLKCFG_HI,
                               0x80);
                       snd_soc_write(codec, M98088_REG_13_DAI1_CLKCFG_LO,
                               0x00);
                       break;
               case SND_SOC_DAIFMT_CBM_CFM:
                       
                       reg14val |= M98088_DAI_MAS;
                       break;
               case SND_SOC_DAIFMT_CBS_CFM:
               case SND_SOC_DAIFMT_CBM_CFS:
               default:
                       dev_err(codec->dev, "Clock mode unsupported");
                       return -EINVAL;
               }

               switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
               case SND_SOC_DAIFMT_I2S:
                       reg14val |= M98088_DAI_DLY;
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
                       reg14val |= M98088_DAI_WCI;
                       break;
               case SND_SOC_DAIFMT_IB_NF:
                       reg14val |= M98088_DAI_BCI;
                       break;
               case SND_SOC_DAIFMT_IB_IF:
                       reg14val |= M98088_DAI_BCI|M98088_DAI_WCI;
                       break;
               default:
                       return -EINVAL;
               }

               snd_soc_update_bits(codec, M98088_REG_14_DAI1_FORMAT,
                       M98088_DAI_MAS | M98088_DAI_DLY | M98088_DAI_BCI |
                       M98088_DAI_WCI, reg14val);

               reg15val = M98088_DAI_BSEL64;
               if (max98088->digmic)
                       reg15val |= M98088_DAI_OSR64;
               snd_soc_write(codec, M98088_REG_15_DAI1_CLOCK, reg15val);
       }

       return 0;
}

static int max98088_dai2_set_fmt(struct snd_soc_dai *codec_dai,
                                unsigned int fmt)
{
       struct snd_soc_codec *codec = codec_dai->codec;
       struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);
       struct max98088_cdata *cdata;
       u8 reg1Cval = 0;

       cdata = &max98088->dai[1];

       if (fmt != cdata->fmt) {
               cdata->fmt = fmt;

               switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
               case SND_SOC_DAIFMT_CBS_CFS:
                       
                       snd_soc_write(codec, M98088_REG_1A_DAI2_CLKCFG_HI,
                               0x80);
                       snd_soc_write(codec, M98088_REG_1B_DAI2_CLKCFG_LO,
                               0x00);
                       break;
               case SND_SOC_DAIFMT_CBM_CFM:
                       
                       reg1Cval |= M98088_DAI_MAS;
                       break;
               case SND_SOC_DAIFMT_CBS_CFM:
               case SND_SOC_DAIFMT_CBM_CFS:
               default:
                       dev_err(codec->dev, "Clock mode unsupported");
                       return -EINVAL;
               }

               switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
               case SND_SOC_DAIFMT_I2S:
                       reg1Cval |= M98088_DAI_DLY;
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
                       reg1Cval |= M98088_DAI_WCI;
                       break;
               case SND_SOC_DAIFMT_IB_NF:
                       reg1Cval |= M98088_DAI_BCI;
                       break;
               case SND_SOC_DAIFMT_IB_IF:
                       reg1Cval |= M98088_DAI_BCI|M98088_DAI_WCI;
                       break;
               default:
                       return -EINVAL;
               }

               snd_soc_update_bits(codec, M98088_REG_1C_DAI2_FORMAT,
                       M98088_DAI_MAS | M98088_DAI_DLY | M98088_DAI_BCI |
                       M98088_DAI_WCI, reg1Cval);

               snd_soc_write(codec, M98088_REG_1D_DAI2_CLOCK,
                       M98088_DAI_BSEL64);
       }

       return 0;
}

static int max98088_dai1_digital_mute(struct snd_soc_dai *codec_dai, int mute)
{
       struct snd_soc_codec *codec = codec_dai->codec;
       int reg;

       if (mute)
               reg = M98088_DAI_MUTE;
       else
               reg = 0;

       snd_soc_update_bits(codec, M98088_REG_2F_LVL_DAI1_PLAY,
                           M98088_DAI_MUTE_MASK, reg);
       return 0;
}

static int max98088_dai2_digital_mute(struct snd_soc_dai *codec_dai, int mute)
{
       struct snd_soc_codec *codec = codec_dai->codec;
       int reg;

       if (mute)
               reg = M98088_DAI_MUTE;
       else
               reg = 0;

       snd_soc_update_bits(codec, M98088_REG_31_LVL_DAI2_PLAY,
                           M98088_DAI_MUTE_MASK, reg);
       return 0;
}

static void max98088_sync_cache(struct snd_soc_codec *codec)
{
       u16 *reg_cache = codec->reg_cache;
       int i;

       if (!codec->cache_sync)
               return;

       codec->cache_only = 0;

       for (i = 1; i < codec->driver->reg_cache_size; i++) {
               if (!max98088_access[i].writable)
                       continue;

               if (reg_cache[i] == max98088_reg[i])
                       continue;

               snd_soc_write(codec, i, reg_cache[i]);
       }

       codec->cache_sync = 0;
}

static int max98088_set_bias_level(struct snd_soc_codec *codec,
                                  enum snd_soc_bias_level level)
{
       switch (level) {
       case SND_SOC_BIAS_ON:
               break;

       case SND_SOC_BIAS_PREPARE:
               break;

       case SND_SOC_BIAS_STANDBY:
               if (codec->dapm.bias_level == SND_SOC_BIAS_OFF)
                       max98088_sync_cache(codec);

               snd_soc_update_bits(codec, M98088_REG_4C_PWR_EN_IN,
                               M98088_MBEN, M98088_MBEN);
               break;

       case SND_SOC_BIAS_OFF:
               snd_soc_update_bits(codec, M98088_REG_4C_PWR_EN_IN,
                               M98088_MBEN, 0);
               codec->cache_sync = 1;
               break;
       }
       codec->dapm.bias_level = level;
       return 0;
}

#define MAX98088_RATES SNDRV_PCM_RATE_8000_96000
#define MAX98088_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE)

static const struct snd_soc_dai_ops max98088_dai1_ops = {
       .set_sysclk = max98088_dai_set_sysclk,
       .set_fmt = max98088_dai1_set_fmt,
       .hw_params = max98088_dai1_hw_params,
       .digital_mute = max98088_dai1_digital_mute,
};

static const struct snd_soc_dai_ops max98088_dai2_ops = {
       .set_sysclk = max98088_dai_set_sysclk,
       .set_fmt = max98088_dai2_set_fmt,
       .hw_params = max98088_dai2_hw_params,
       .digital_mute = max98088_dai2_digital_mute,
};

static struct snd_soc_dai_driver max98088_dai[] = {
{
       .name = "HiFi",
       .playback = {
               .stream_name = "HiFi Playback",
               .channels_min = 1,
               .channels_max = 2,
               .rates = MAX98088_RATES,
               .formats = MAX98088_FORMATS,
       },
       .capture = {
               .stream_name = "HiFi Capture",
               .channels_min = 1,
               .channels_max = 2,
               .rates = MAX98088_RATES,
               .formats = MAX98088_FORMATS,
       },
        .ops = &max98088_dai1_ops,
},
{
       .name = "Aux",
       .playback = {
               .stream_name = "Aux Playback",
               .channels_min = 1,
               .channels_max = 2,
               .rates = MAX98088_RATES,
               .formats = MAX98088_FORMATS,
       },
       .ops = &max98088_dai2_ops,
}
};

static const char *eq_mode_name[] = {"EQ1 Mode", "EQ2 Mode"};

static int max98088_get_channel(struct snd_soc_codec *codec, const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(eq_mode_name); i++)
		if (strcmp(name, eq_mode_name[i]) == 0)
			return i;

	
	dev_err(codec->dev, "Bad EQ channel name '%s'\n", name);
	return -EINVAL;
}

static void max98088_setup_eq1(struct snd_soc_codec *codec)
{
       struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);
       struct max98088_pdata *pdata = max98088->pdata;
       struct max98088_eq_cfg *coef_set;
       int best, best_val, save, i, sel, fs;
       struct max98088_cdata *cdata;

       cdata = &max98088->dai[0];

       if (!pdata || !max98088->eq_textcnt)
               return;

       
       fs = cdata->rate;
       sel = cdata->eq_sel;

       best = 0;
       best_val = INT_MAX;
       for (i = 0; i < pdata->eq_cfgcnt; i++) {
               if (strcmp(pdata->eq_cfg[i].name, max98088->eq_texts[sel]) == 0 &&
                   abs(pdata->eq_cfg[i].rate - fs) < best_val) {
                       best = i;
                       best_val = abs(pdata->eq_cfg[i].rate - fs);
               }
       }

       dev_dbg(codec->dev, "Selected %s/%dHz for %dHz sample rate\n",
               pdata->eq_cfg[best].name,
               pdata->eq_cfg[best].rate, fs);

       
       save = snd_soc_read(codec, M98088_REG_49_CFG_LEVEL);
       snd_soc_update_bits(codec, M98088_REG_49_CFG_LEVEL, M98088_EQ1EN, 0);

       coef_set = &pdata->eq_cfg[sel];

       m98088_eq_band(codec, 0, 0, coef_set->band1);
       m98088_eq_band(codec, 0, 1, coef_set->band2);
       m98088_eq_band(codec, 0, 2, coef_set->band3);
       m98088_eq_band(codec, 0, 3, coef_set->band4);
       m98088_eq_band(codec, 0, 4, coef_set->band5);

       
       snd_soc_update_bits(codec, M98088_REG_49_CFG_LEVEL, M98088_EQ1EN, save);
}

static void max98088_setup_eq2(struct snd_soc_codec *codec)
{
       struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);
       struct max98088_pdata *pdata = max98088->pdata;
       struct max98088_eq_cfg *coef_set;
       int best, best_val, save, i, sel, fs;
       struct max98088_cdata *cdata;

       cdata = &max98088->dai[1];

       if (!pdata || !max98088->eq_textcnt)
               return;

       
       fs = cdata->rate;

       sel = cdata->eq_sel;
       best = 0;
       best_val = INT_MAX;
       for (i = 0; i < pdata->eq_cfgcnt; i++) {
               if (strcmp(pdata->eq_cfg[i].name, max98088->eq_texts[sel]) == 0 &&
                   abs(pdata->eq_cfg[i].rate - fs) < best_val) {
                       best = i;
                       best_val = abs(pdata->eq_cfg[i].rate - fs);
               }
       }

       dev_dbg(codec->dev, "Selected %s/%dHz for %dHz sample rate\n",
               pdata->eq_cfg[best].name,
               pdata->eq_cfg[best].rate, fs);

       
       save = snd_soc_read(codec, M98088_REG_49_CFG_LEVEL);
       snd_soc_update_bits(codec, M98088_REG_49_CFG_LEVEL, M98088_EQ2EN, 0);

       coef_set = &pdata->eq_cfg[sel];

       m98088_eq_band(codec, 1, 0, coef_set->band1);
       m98088_eq_band(codec, 1, 1, coef_set->band2);
       m98088_eq_band(codec, 1, 2, coef_set->band3);
       m98088_eq_band(codec, 1, 3, coef_set->band4);
       m98088_eq_band(codec, 1, 4, coef_set->band5);

       
       snd_soc_update_bits(codec, M98088_REG_49_CFG_LEVEL, M98088_EQ2EN,
               save);
}

static int max98088_put_eq_enum(struct snd_kcontrol *kcontrol,
                                struct snd_ctl_elem_value *ucontrol)
{
       struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
       struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);
       struct max98088_pdata *pdata = max98088->pdata;
       int channel = max98088_get_channel(codec, kcontrol->id.name);
       struct max98088_cdata *cdata;
       int sel = ucontrol->value.integer.value[0];

       if (channel < 0)
	       return channel;

       cdata = &max98088->dai[channel];

       if (sel >= pdata->eq_cfgcnt)
               return -EINVAL;

       cdata->eq_sel = sel;

       switch (channel) {
       case 0:
               max98088_setup_eq1(codec);
               break;
       case 1:
               max98088_setup_eq2(codec);
               break;
       }

       return 0;
}

static int max98088_get_eq_enum(struct snd_kcontrol *kcontrol,
                                struct snd_ctl_elem_value *ucontrol)
{
       struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
       struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);
       int channel = max98088_get_channel(codec, kcontrol->id.name);
       struct max98088_cdata *cdata;

       if (channel < 0)
	       return channel;

       cdata = &max98088->dai[channel];
       ucontrol->value.enumerated.item[0] = cdata->eq_sel;
       return 0;
}

static void max98088_handle_eq_pdata(struct snd_soc_codec *codec)
{
       struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);
       struct max98088_pdata *pdata = max98088->pdata;
       struct max98088_eq_cfg *cfg;
       unsigned int cfgcnt;
       int i, j;
       const char **t;
       int ret;
       struct snd_kcontrol_new controls[] = {
               SOC_ENUM_EXT((char *)eq_mode_name[0],
                       max98088->eq_enum,
                       max98088_get_eq_enum,
                       max98088_put_eq_enum),
               SOC_ENUM_EXT((char *)eq_mode_name[1],
                       max98088->eq_enum,
                       max98088_get_eq_enum,
                       max98088_put_eq_enum),
       };
       BUILD_BUG_ON(ARRAY_SIZE(controls) != ARRAY_SIZE(eq_mode_name));

       cfg = pdata->eq_cfg;
       cfgcnt = pdata->eq_cfgcnt;

       max98088->eq_textcnt = 0;
       max98088->eq_texts = NULL;
       for (i = 0; i < cfgcnt; i++) {
               for (j = 0; j < max98088->eq_textcnt; j++) {
                       if (strcmp(cfg[i].name, max98088->eq_texts[j]) == 0)
                               break;
               }

               if (j != max98088->eq_textcnt)
                       continue;

               
               t = krealloc(max98088->eq_texts,
                            sizeof(char *) * (max98088->eq_textcnt + 1),
                            GFP_KERNEL);
               if (t == NULL)
                       continue;

               
               t[max98088->eq_textcnt] = cfg[i].name;
               max98088->eq_textcnt++;
               max98088->eq_texts = t;
       }

       
       max98088->eq_enum.texts = max98088->eq_texts;
       max98088->eq_enum.max = max98088->eq_textcnt;

       ret = snd_soc_add_codec_controls(codec, controls, ARRAY_SIZE(controls));
       if (ret != 0)
               dev_err(codec->dev, "Failed to add EQ control: %d\n", ret);
}

static void max98088_handle_pdata(struct snd_soc_codec *codec)
{
       struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);
       struct max98088_pdata *pdata = max98088->pdata;
       u8 regval = 0;

       if (!pdata) {
               dev_dbg(codec->dev, "No platform data\n");
               return;
       }

       
       if (pdata->digmic_left_mode)
               regval |= M98088_DIGMIC_L;

       if (pdata->digmic_right_mode)
               regval |= M98088_DIGMIC_R;

       max98088->digmic = (regval ? 1 : 0);

       snd_soc_write(codec, M98088_REG_48_CFG_MIC, regval);

       
       regval = ((pdata->receiver_mode) ? M98088_REC_LINEMODE : 0);
       snd_soc_update_bits(codec, M98088_REG_2A_MIC_REC_CNTL,
               M98088_REC_LINEMODE_MASK, regval);

       
       if (pdata->eq_cfgcnt)
               max98088_handle_eq_pdata(codec);
}

#ifdef CONFIG_PM
static int max98088_suspend(struct snd_soc_codec *codec)
{
       max98088_set_bias_level(codec, SND_SOC_BIAS_OFF);

       return 0;
}

static int max98088_resume(struct snd_soc_codec *codec)
{
       max98088_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

       return 0;
}
#else
#define max98088_suspend NULL
#define max98088_resume NULL
#endif

static int max98088_probe(struct snd_soc_codec *codec)
{
       struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);
       struct max98088_cdata *cdata;
       int ret = 0;

       codec->cache_sync = 1;

       ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_I2C);
       if (ret != 0) {
               dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
               return ret;
       }

       

       max98088->sysclk = (unsigned)-1;
       max98088->eq_textcnt = 0;

       cdata = &max98088->dai[0];
       cdata->rate = (unsigned)-1;
       cdata->fmt  = (unsigned)-1;
       cdata->eq_sel = 0;

       cdata = &max98088->dai[1];
       cdata->rate = (unsigned)-1;
       cdata->fmt  = (unsigned)-1;
       cdata->eq_sel = 0;

       max98088->ina_state = 0;
       max98088->inb_state = 0;
       max98088->ex_mode = 0;
       max98088->digmic = 0;
       max98088->mic1pre = 0;
       max98088->mic2pre = 0;

       ret = snd_soc_read(codec, M98088_REG_FF_REV_ID);
       if (ret < 0) {
               dev_err(codec->dev, "Failed to read device revision: %d\n",
                       ret);
               goto err_access;
       }
       dev_info(codec->dev, "revision %c\n", ret + 'A');

       snd_soc_write(codec, M98088_REG_51_PWR_SYS, M98088_PWRSV);

       
       max98088_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

       snd_soc_write(codec, M98088_REG_0F_IRQ_ENABLE, 0x00);

       snd_soc_write(codec, M98088_REG_22_MIX_DAC,
               M98088_DAI1L_TO_DACL|M98088_DAI2L_TO_DACL|
               M98088_DAI1R_TO_DACR|M98088_DAI2R_TO_DACR);

       snd_soc_write(codec, M98088_REG_4E_BIAS_CNTL, 0xF0);
       snd_soc_write(codec, M98088_REG_50_DAC_BIAS2, 0x0F);

       snd_soc_write(codec, M98088_REG_16_DAI1_IOCFG,
               M98088_S1NORMAL|M98088_SDATA);

       snd_soc_write(codec, M98088_REG_1E_DAI2_IOCFG,
               M98088_S2NORMAL|M98088_SDATA);

       max98088_handle_pdata(codec);

       snd_soc_add_codec_controls(codec, max98088_snd_controls,
                            ARRAY_SIZE(max98088_snd_controls));

err_access:
       return ret;
}

static int max98088_remove(struct snd_soc_codec *codec)
{
       struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);

       max98088_set_bias_level(codec, SND_SOC_BIAS_OFF);
       kfree(max98088->eq_texts);

       return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_max98088 = {
       .probe   = max98088_probe,
       .remove  = max98088_remove,
       .suspend = max98088_suspend,
       .resume  = max98088_resume,
       .set_bias_level = max98088_set_bias_level,
       .reg_cache_size = ARRAY_SIZE(max98088_reg),
       .reg_word_size = sizeof(u8),
       .reg_cache_default = max98088_reg,
       .volatile_register = max98088_volatile_register,
	.dapm_widgets = max98088_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(max98088_dapm_widgets),
	.dapm_routes = max98088_audio_map,
	.num_dapm_routes = ARRAY_SIZE(max98088_audio_map),
};

static int max98088_i2c_probe(struct i2c_client *i2c,
                            const struct i2c_device_id *id)
{
       struct max98088_priv *max98088;
       int ret;

       max98088 = devm_kzalloc(&i2c->dev, sizeof(struct max98088_priv),
			       GFP_KERNEL);
       if (max98088 == NULL)
               return -ENOMEM;

       max98088->devtype = id->driver_data;

       i2c_set_clientdata(i2c, max98088);
       max98088->pdata = i2c->dev.platform_data;

       ret = snd_soc_register_codec(&i2c->dev,
                       &soc_codec_dev_max98088, &max98088_dai[0], 2);
       return ret;
}

static int __devexit max98088_i2c_remove(struct i2c_client *client)
{
       snd_soc_unregister_codec(&client->dev);
       return 0;
}

static const struct i2c_device_id max98088_i2c_id[] = {
       { "max98088", MAX98088 },
       { "max98089", MAX98089 },
       { }
};
MODULE_DEVICE_TABLE(i2c, max98088_i2c_id);

static struct i2c_driver max98088_i2c_driver = {
       .driver = {
               .name = "max98088",
               .owner = THIS_MODULE,
       },
       .probe  = max98088_i2c_probe,
       .remove = __devexit_p(max98088_i2c_remove),
       .id_table = max98088_i2c_id,
};

static int __init max98088_init(void)
{
       int ret;

       ret = i2c_add_driver(&max98088_i2c_driver);
       if (ret)
               pr_err("Failed to register max98088 I2C driver: %d\n", ret);

       return ret;
}
module_init(max98088_init);

static void __exit max98088_exit(void)
{
       i2c_del_driver(&max98088_i2c_driver);
}
module_exit(max98088_exit);

MODULE_DESCRIPTION("ALSA SoC MAX98088 driver");
MODULE_AUTHOR("Peter Hsiang, Jesse Marroquin");
MODULE_LICENSE("GPL");
