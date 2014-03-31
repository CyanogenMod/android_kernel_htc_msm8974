/*
 * Texas Instruments TLV320AIC26 low power audio CODEC
 * ALSA SoC CODEC driver
 *
 * Copyright (C) 2008 Secret Lab Technologies Ltd.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>

#include "tlv320aic26.h"

MODULE_DESCRIPTION("ASoC TLV320AIC26 codec driver");
MODULE_AUTHOR("Grant Likely <grant.likely@secretlab.ca>");
MODULE_LICENSE("GPL");

struct aic26 {
	struct spi_device *spi;
	struct snd_soc_codec codec;
	int master;
	int datfm;
	int mclk;

	
	int keyclick_amplitude;
	int keyclick_freq;
	int keyclick_len;
};

static unsigned int aic26_reg_read(struct snd_soc_codec *codec,
				   unsigned int reg)
{
	struct aic26 *aic26 = snd_soc_codec_get_drvdata(codec);
	u16 *cache = codec->reg_cache;
	u16 cmd, value;
	u8 buffer[2];
	int rc;

	if (reg >= AIC26_NUM_REGS) {
		WARN_ON_ONCE(1);
		return 0;
	}

	cmd = AIC26_READ_COMMAND_WORD(reg);
	buffer[0] = (cmd >> 8) & 0xff;
	buffer[1] = cmd & 0xff;
	rc = spi_write_then_read(aic26->spi, buffer, 2, buffer, 2);
	if (rc) {
		dev_err(&aic26->spi->dev, "AIC26 reg read error\n");
		return -EIO;
	}
	value = (buffer[0] << 8) | buffer[1];

	
	cache[reg] = value;
	return value;
}

static unsigned int aic26_reg_read_cache(struct snd_soc_codec *codec,
					 unsigned int reg)
{
	u16 *cache = codec->reg_cache;

	if (reg >= AIC26_NUM_REGS) {
		WARN_ON_ONCE(1);
		return 0;
	}

	return cache[reg];
}

static int aic26_reg_write(struct snd_soc_codec *codec, unsigned int reg,
			   unsigned int value)
{
	struct aic26 *aic26 = snd_soc_codec_get_drvdata(codec);
	u16 *cache = codec->reg_cache;
	u16 cmd;
	u8 buffer[4];
	int rc;

	if (reg >= AIC26_NUM_REGS) {
		WARN_ON_ONCE(1);
		return -EINVAL;
	}

	cmd = AIC26_WRITE_COMMAND_WORD(reg);
	buffer[0] = (cmd >> 8) & 0xff;
	buffer[1] = cmd & 0xff;
	buffer[2] = value >> 8;
	buffer[3] = value;
	rc = spi_write(aic26->spi, buffer, 4);
	if (rc) {
		dev_err(&aic26->spi->dev, "AIC26 reg read error\n");
		return -EIO;
	}

	
	cache[reg] = value;
	return 0;
}

static int aic26_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct aic26 *aic26 = snd_soc_codec_get_drvdata(codec);
	int fsref, divisor, wlen, pval, jval, dval, qval;
	u16 reg;

	dev_dbg(&aic26->spi->dev, "aic26_hw_params(substream=%p, params=%p)\n",
		substream, params);
	dev_dbg(&aic26->spi->dev, "rate=%i format=%i\n", params_rate(params),
		params_format(params));

	switch (params_rate(params)) {
	case 8000:  fsref = 48000; divisor = AIC26_DIV_6; break;
	case 11025: fsref = 44100; divisor = AIC26_DIV_4; break;
	case 12000: fsref = 48000; divisor = AIC26_DIV_4; break;
	case 16000: fsref = 48000; divisor = AIC26_DIV_3; break;
	case 22050: fsref = 44100; divisor = AIC26_DIV_2; break;
	case 24000: fsref = 48000; divisor = AIC26_DIV_2; break;
	case 32000: fsref = 48000; divisor = AIC26_DIV_1_5; break;
	case 44100: fsref = 44100; divisor = AIC26_DIV_1; break;
	case 48000: fsref = 48000; divisor = AIC26_DIV_1; break;
	default:
		dev_dbg(&aic26->spi->dev, "bad rate\n"); return -EINVAL;
	}

	
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:     wlen = AIC26_WLEN_16; break;
	case SNDRV_PCM_FORMAT_S16_BE: wlen = AIC26_WLEN_16; break;
	case SNDRV_PCM_FORMAT_S24_BE: wlen = AIC26_WLEN_24; break;
	case SNDRV_PCM_FORMAT_S32_BE: wlen = AIC26_WLEN_32; break;
	default:
		dev_dbg(&aic26->spi->dev, "bad format\n"); return -EINVAL;
	}

	pval = 1;
	
	jval = fsref / (aic26->mclk / 2048);
	
	dval = fsref - (jval * (aic26->mclk / 2048));
	dval = (10000 * dval) / (aic26->mclk / 2048);
	dev_dbg(&aic26->spi->dev, "Setting PLLM to %d.%04d\n", jval, dval);
	qval = 0;
	reg = 0x8000 | qval << 11 | pval << 8 | jval << 2;
	aic26_reg_write(codec, AIC26_REG_PLL_PROG1, reg);
	reg = dval << 2;
	aic26_reg_write(codec, AIC26_REG_PLL_PROG2, reg);

	
	reg = aic26_reg_read_cache(codec, AIC26_REG_AUDIO_CTRL3);
	reg &= ~0xf800;
	if (aic26->master)
		reg |= 0x0800;
	if (fsref == 48000)
		reg |= 0x2000;
	aic26_reg_write(codec, AIC26_REG_AUDIO_CTRL3, reg);

	
	reg = aic26_reg_read_cache(codec, AIC26_REG_AUDIO_CTRL1);
	reg &= ~0x0fff;
	reg |= wlen | aic26->datfm | (divisor << 3) | divisor;
	aic26_reg_write(codec, AIC26_REG_AUDIO_CTRL1, reg);

	return 0;
}

static int aic26_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct aic26 *aic26 = snd_soc_codec_get_drvdata(codec);
	u16 reg = aic26_reg_read_cache(codec, AIC26_REG_DAC_GAIN);

	dev_dbg(&aic26->spi->dev, "aic26_mute(dai=%p, mute=%i)\n",
		dai, mute);

	if (mute)
		reg |= 0x8080;
	else
		reg &= ~0x8080;
	aic26_reg_write(codec, AIC26_REG_DAC_GAIN, reg);

	return 0;
}

static int aic26_set_sysclk(struct snd_soc_dai *codec_dai,
			    int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct aic26 *aic26 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(&aic26->spi->dev, "aic26_set_sysclk(dai=%p, clk_id==%i,"
		" freq=%i, dir=%i)\n",
		codec_dai, clk_id, freq, dir);

	
	if ((freq < 2000000) || (freq > 50000000))
		return -EINVAL;

	aic26->mclk = freq;
	return 0;
}

static int aic26_set_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct aic26 *aic26 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(&aic26->spi->dev, "aic26_set_fmt(dai=%p, fmt==%i)\n",
		codec_dai, fmt);

	
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM: aic26->master = 1; break;
	case SND_SOC_DAIFMT_CBS_CFS: aic26->master = 0; break;
	default:
		dev_dbg(&aic26->spi->dev, "bad master\n"); return -EINVAL;
	}

	
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:     aic26->datfm = AIC26_DATFM_I2S; break;
	case SND_SOC_DAIFMT_DSP_A:   aic26->datfm = AIC26_DATFM_DSP; break;
	case SND_SOC_DAIFMT_RIGHT_J: aic26->datfm = AIC26_DATFM_RIGHTJ; break;
	case SND_SOC_DAIFMT_LEFT_J:  aic26->datfm = AIC26_DATFM_LEFTJ; break;
	default:
		dev_dbg(&aic26->spi->dev, "bad format\n"); return -EINVAL;
	}

	return 0;
}

#define AIC26_RATES	(SNDRV_PCM_RATE_8000  | SNDRV_PCM_RATE_11025 |\
			 SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
			 SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |\
			 SNDRV_PCM_RATE_48000)
#define AIC26_FORMATS	(SNDRV_PCM_FMTBIT_S8     | SNDRV_PCM_FMTBIT_S16_BE |\
			 SNDRV_PCM_FMTBIT_S24_BE | SNDRV_PCM_FMTBIT_S32_BE)

static const struct snd_soc_dai_ops aic26_dai_ops = {
	.hw_params	= aic26_hw_params,
	.digital_mute	= aic26_mute,
	.set_sysclk	= aic26_set_sysclk,
	.set_fmt	= aic26_set_fmt,
};

static struct snd_soc_dai_driver aic26_dai = {
	.name = "tlv320aic26-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = AIC26_RATES,
		.formats = AIC26_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = AIC26_RATES,
		.formats = AIC26_FORMATS,
	},
	.ops = &aic26_dai_ops,
};

static const char *aic26_capture_src_text[] = {"Mic", "Aux"};
static const struct soc_enum aic26_capture_src_enum =
	SOC_ENUM_SINGLE(AIC26_REG_AUDIO_CTRL1, 12, 2, aic26_capture_src_text);

static const struct snd_kcontrol_new aic26_snd_controls[] = {
	
	SOC_DOUBLE("PCM Playback Volume", AIC26_REG_DAC_GAIN, 8, 0, 0x7f, 1),
	SOC_DOUBLE("PCM Playback Switch", AIC26_REG_DAC_GAIN, 15, 7, 1, 1),
	SOC_SINGLE("PCM Capture Volume", AIC26_REG_ADC_GAIN, 8, 0x7f, 0),
	SOC_SINGLE("PCM Capture Mute", AIC26_REG_ADC_GAIN, 15, 1, 1),
	SOC_SINGLE("Keyclick activate", AIC26_REG_AUDIO_CTRL2, 15, 0x1, 0),
	SOC_SINGLE("Keyclick amplitude", AIC26_REG_AUDIO_CTRL2, 12, 0x7, 0),
	SOC_SINGLE("Keyclick frequency", AIC26_REG_AUDIO_CTRL2, 8, 0x7, 0),
	SOC_SINGLE("Keyclick period", AIC26_REG_AUDIO_CTRL2, 4, 0xf, 0),
	SOC_ENUM("Capture Source", aic26_capture_src_enum),
};


static ssize_t aic26_keyclick_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct aic26 *aic26 = dev_get_drvdata(dev);
	int val, amp, freq, len;

	val = aic26_reg_read_cache(&aic26->codec, AIC26_REG_AUDIO_CTRL2);
	amp = (val >> 12) & 0x7;
	freq = (125 << ((val >> 8) & 0x7)) >> 1;
	len = 2 * (1 + ((val >> 4) & 0xf));

	return sprintf(buf, "amp=%x freq=%iHz len=%iclks\n", amp, freq, len);
}

static ssize_t aic26_keyclick_set(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct aic26 *aic26 = dev_get_drvdata(dev);
	int val;

	val = aic26_reg_read_cache(&aic26->codec, AIC26_REG_AUDIO_CTRL2);
	val |= 0x8000;
	aic26_reg_write(&aic26->codec, AIC26_REG_AUDIO_CTRL2, val);

	return count;
}

static DEVICE_ATTR(keyclick, 0644, aic26_keyclick_show, aic26_keyclick_set);

static int aic26_probe(struct snd_soc_codec *codec)
{
	int ret, err, i, reg;

	dev_info(codec->dev, "Probing AIC26 SoC CODEC driver\n");

	
	aic26_reg_write(codec, AIC26_REG_RESET, 0xBB00);

	
	aic26_reg_write(codec, AIC26_REG_POWER_CTRL, 0);

	
	reg = aic26_reg_read(codec, AIC26_REG_AUDIO_CTRL3);
	reg &= ~0xf800;
	reg |= 0x0800; 
	aic26_reg_write(codec, AIC26_REG_AUDIO_CTRL3, reg);

	
	for (i = 0; i < codec->driver->reg_cache_size; i++)
		aic26_reg_read(codec, i);

	
	
	ret = device_create_file(codec->dev, &dev_attr_keyclick);
	if (ret)
		dev_info(codec->dev, "error creating sysfs files\n");

	
	dev_dbg(codec->dev, "Registering controls\n");
	err = snd_soc_add_codec_controls(codec, aic26_snd_controls,
			ARRAY_SIZE(aic26_snd_controls));
	WARN_ON(err < 0);

	return 0;
}

static struct snd_soc_codec_driver aic26_soc_codec_dev = {
	.probe = aic26_probe,
	.read = aic26_reg_read,
	.write = aic26_reg_write,
	.reg_cache_size = AIC26_NUM_REGS,
	.reg_word_size = sizeof(u16),
};

static int aic26_spi_probe(struct spi_device *spi)
{
	struct aic26 *aic26;
	int ret;

	dev_dbg(&spi->dev, "probing tlv320aic26 spi device\n");

	
	aic26 = devm_kzalloc(&spi->dev, sizeof *aic26, GFP_KERNEL);
	if (!aic26)
		return -ENOMEM;

	
	aic26->spi = spi;
	dev_set_drvdata(&spi->dev, aic26);
	aic26->master = 1;

	ret = snd_soc_register_codec(&spi->dev,
			&aic26_soc_codec_dev, &aic26_dai, 1);
	return ret;
}

static int aic26_spi_remove(struct spi_device *spi)
{
	snd_soc_unregister_codec(&spi->dev);
	return 0;
}

static struct spi_driver aic26_spi = {
	.driver = {
		.name = "tlv320aic26-codec",
		.owner = THIS_MODULE,
	},
	.probe = aic26_spi_probe,
	.remove = aic26_spi_remove,
};

static int __init aic26_init(void)
{
	return spi_register_driver(&aic26_spi);
}
module_init(aic26_init);

static void __exit aic26_exit(void)
{
	spi_unregister_driver(&aic26_spi);
}
module_exit(aic26_exit);
