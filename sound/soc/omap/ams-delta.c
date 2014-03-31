/*
 * ams-delta.c  --  SoC audio for Amstrad E3 (Delta) videophone
 *
 * Copyright (C) 2009 Janusz Krzysztofik <jkrzyszt@tis.icnet.pl>
 *
 * Initially based on sound/soc/omap/osk5912.x
 * Copyright (C) 2008 Mistral Solutions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/gpio.h>
#include <linux/spinlock.h>
#include <linux/tty.h>
#include <linux/module.h>

#include <sound/soc.h>
#include <sound/jack.h>

#include <asm/mach-types.h>

#include <plat/board-ams-delta.h>
#include <plat/mcbsp.h>

#include "omap-mcbsp.h"
#include "omap-pcm.h"
#include "../codecs/cx20442.h"


static const struct snd_soc_dapm_widget ams_delta_dapm_widgets[] = {
	
	SND_SOC_DAPM_MIC("Mouthpiece", NULL),
	SND_SOC_DAPM_HP("Earpiece", NULL),
	
	SND_SOC_DAPM_MIC("Microphone", NULL),
	SND_SOC_DAPM_SPK("Speaker", NULL),
};

static const struct snd_soc_dapm_route ams_delta_audio_map[] = {
	{"TELIN", NULL, "Mouthpiece"},
	{"Earpiece", NULL, "TELOUT"},

	{"MIC", NULL, "Microphone"},
	{"Speaker", NULL, "SPKOUT"},
};


static const char *ams_delta_audio_mode[] =
	{"Mixed", "Handset", "Handsfree", "Speakerphone"};

#define AMS_DELTA_MOUTHPIECE	0
#define AMS_DELTA_EARPIECE	1
#define AMS_DELTA_MICROPHONE	2
#define AMS_DELTA_SPEAKER	3
#define AMS_DELTA_AGC		4

#define AMS_DELTA_MIXED		((1 << AMS_DELTA_EARPIECE) | \
						(1 << AMS_DELTA_MICROPHONE))
#define AMS_DELTA_HANDSET	((1 << AMS_DELTA_MOUTHPIECE) | \
						(1 << AMS_DELTA_EARPIECE))
#define AMS_DELTA_HANDSFREE	((1 << AMS_DELTA_MICROPHONE) | \
						(1 << AMS_DELTA_SPEAKER))
#define AMS_DELTA_SPEAKERPHONE	(AMS_DELTA_HANDSFREE | (1 << AMS_DELTA_AGC))

static const unsigned short ams_delta_audio_mode_pins[] = {
	AMS_DELTA_MIXED,
	AMS_DELTA_HANDSET,
	AMS_DELTA_HANDSFREE,
	AMS_DELTA_SPEAKERPHONE,
};

static unsigned short ams_delta_audio_agc;

static int ams_delta_set_audio_mode(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec =  snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct soc_enum *control = (struct soc_enum *)kcontrol->private_value;
	unsigned short pins;
	int pin, changed = 0;

	
	if (!codec->hw_write)
		return -EUNATCH;

	if (ucontrol->value.enumerated.item[0] >= control->max)
		return -EINVAL;

	mutex_lock(&codec->mutex);

	
	pins = ams_delta_audio_mode_pins[ucontrol->value.enumerated.item[0]];

	
	pin = !!(pins & (1 << AMS_DELTA_MOUTHPIECE));
	if (pin != snd_soc_dapm_get_pin_status(dapm, "Mouthpiece")) {
		changed = 1;
		if (pin)
			snd_soc_dapm_enable_pin(dapm, "Mouthpiece");
		else
			snd_soc_dapm_disable_pin(dapm, "Mouthpiece");
	}
	pin = !!(pins & (1 << AMS_DELTA_EARPIECE));
	if (pin != snd_soc_dapm_get_pin_status(dapm, "Earpiece")) {
		changed = 1;
		if (pin)
			snd_soc_dapm_enable_pin(dapm, "Earpiece");
		else
			snd_soc_dapm_disable_pin(dapm, "Earpiece");
	}
	pin = !!(pins & (1 << AMS_DELTA_MICROPHONE));
	if (pin != snd_soc_dapm_get_pin_status(dapm, "Microphone")) {
		changed = 1;
		if (pin)
			snd_soc_dapm_enable_pin(dapm, "Microphone");
		else
			snd_soc_dapm_disable_pin(dapm, "Microphone");
	}
	pin = !!(pins & (1 << AMS_DELTA_SPEAKER));
	if (pin != snd_soc_dapm_get_pin_status(dapm, "Speaker")) {
		changed = 1;
		if (pin)
			snd_soc_dapm_enable_pin(dapm, "Speaker");
		else
			snd_soc_dapm_disable_pin(dapm, "Speaker");
	}
	pin = !!(pins & (1 << AMS_DELTA_AGC));
	if (pin != ams_delta_audio_agc) {
		ams_delta_audio_agc = pin;
		changed = 1;
		if (pin)
			snd_soc_dapm_enable_pin(dapm, "AGCIN");
		else
			snd_soc_dapm_disable_pin(dapm, "AGCIN");
	}
	if (changed)
		snd_soc_dapm_sync(dapm);

	mutex_unlock(&codec->mutex);

	return changed;
}

static int ams_delta_get_audio_mode(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec =  snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	unsigned short pins, mode;

	pins = ((snd_soc_dapm_get_pin_status(dapm, "Mouthpiece") <<
							AMS_DELTA_MOUTHPIECE) |
			(snd_soc_dapm_get_pin_status(dapm, "Earpiece") <<
							AMS_DELTA_EARPIECE));
	if (pins)
		pins |= (snd_soc_dapm_get_pin_status(dapm, "Microphone") <<
							AMS_DELTA_MICROPHONE);
	else
		pins = ((snd_soc_dapm_get_pin_status(dapm, "Microphone") <<
							AMS_DELTA_MICROPHONE) |
			(snd_soc_dapm_get_pin_status(dapm, "Speaker") <<
							AMS_DELTA_SPEAKER) |
			(ams_delta_audio_agc << AMS_DELTA_AGC));

	for (mode = 0; mode < ARRAY_SIZE(ams_delta_audio_mode); mode++)
		if (pins == ams_delta_audio_mode_pins[mode])
			break;

	if (mode >= ARRAY_SIZE(ams_delta_audio_mode))
		return -EINVAL;

	ucontrol->value.enumerated.item[0] = mode;

	return 0;
}

static const struct soc_enum ams_delta_audio_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ams_delta_audio_mode),
						ams_delta_audio_mode),
};

static const struct snd_kcontrol_new ams_delta_audio_controls[] = {
	SOC_ENUM_EXT("Audio Mode", ams_delta_audio_enum[0],
			ams_delta_get_audio_mode, ams_delta_set_audio_mode),
};

static struct snd_soc_jack ams_delta_hook_switch;
static struct snd_soc_jack_gpio ams_delta_hook_switch_gpios[] = {
	{
		.gpio = 4,
		.name = "hook_switch",
		.report = SND_JACK_HEADSET,
		.invert = 1,
		.debounce_time = 150,
	}
};

static struct snd_soc_jack_pin ams_delta_hook_switch_pins[] = {
	
	{
		.pin = "Mouthpiece",
		.mask = SND_JACK_MICROPHONE,
	},
	{
		.pin = "Earpiece",
		.mask = SND_JACK_HEADPHONE,
	},
	
	{
		.pin = "Microphone",
		.mask = SND_JACK_MICROPHONE,
		.invert = 1,
	},
	{
		.pin = "Speaker",
		.mask = SND_JACK_HEADPHONE,
		.invert = 1,
	},
};



static struct timer_list cx81801_timer;
static bool cx81801_cmd_pending;
static bool ams_delta_muted;
static DEFINE_SPINLOCK(ams_delta_lock);

static void cx81801_timeout(unsigned long data)
{
	int muted;

	spin_lock(&ams_delta_lock);
	cx81801_cmd_pending = 0;
	muted = ams_delta_muted;
	spin_unlock(&ams_delta_lock);

	if (!muted)
		ams_delta_latch2_write(AMS_DELTA_LATCH2_MODEM_CODEC, 0);
}

static struct snd_soc_codec *cx20442_codec;

static int cx81801_open(struct tty_struct *tty)
{
	int ret;

	if (!cx20442_codec)
		return -ENODEV;

	tty->disc_data = cx20442_codec;

	ret = v253_ops.open(tty);

	if (ret < 0)
		tty->disc_data = NULL;

	return ret;
}

static void cx81801_close(struct tty_struct *tty)
{
	struct snd_soc_codec *codec = tty->disc_data;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	del_timer_sync(&cx81801_timer);

	
	INIT_LIST_HEAD(&ams_delta_hook_switch.pins);

	if (!codec)
		return;

	v253_ops.close(tty);

	
	snd_soc_dapm_disable_pin(dapm, "Mouthpiece");
	snd_soc_dapm_enable_pin(dapm, "Earpiece");
	snd_soc_dapm_enable_pin(dapm, "Microphone");
	snd_soc_dapm_disable_pin(dapm, "Speaker");
	snd_soc_dapm_disable_pin(dapm, "AGCIN");
	snd_soc_dapm_sync(dapm);
}

static int cx81801_hangup(struct tty_struct *tty)
{
	cx81801_close(tty);
	return 0;
}

static void cx81801_receive(struct tty_struct *tty,
				const unsigned char *cp, char *fp, int count)
{
	struct snd_soc_codec *codec = tty->disc_data;
	const unsigned char *c;
	int apply, ret;

	if (!codec)
		return;

	if (!codec->hw_write) {
		

		
		setup_timer(&cx81801_timer, cx81801_timeout, 0);

		v253_ops.receive_buf(tty, cp, fp, count);

		
		ret = snd_soc_jack_add_pins(&ams_delta_hook_switch,
					ARRAY_SIZE(ams_delta_hook_switch_pins),
					ams_delta_hook_switch_pins);
		if (ret)
			dev_warn(codec->dev,
				"Failed to link hook switch to DAPM pins, "
				"will continue with hook switch unlinked.\n");

		return;
	}

	v253_ops.receive_buf(tty, cp, fp, count);

	for (c = &cp[count - 1]; c >= cp; c--) {
		if (*c != '\r')
			continue;
		

		spin_lock_bh(&ams_delta_lock);
		mod_timer(&cx81801_timer, jiffies + msecs_to_jiffies(150));
		apply = !ams_delta_muted && !cx81801_cmd_pending;
		cx81801_cmd_pending = 1;
		spin_unlock_bh(&ams_delta_lock);

		if (apply)
			ams_delta_latch2_write(AMS_DELTA_LATCH2_MODEM_CODEC,
						AMS_DELTA_LATCH2_MODEM_CODEC);
		break;
	}
}

static void cx81801_wakeup(struct tty_struct *tty)
{
	v253_ops.write_wakeup(tty);
}

static struct tty_ldisc_ops cx81801_ops = {
	.magic = TTY_LDISC_MAGIC,
	.name = "cx81801",
	.owner = THIS_MODULE,
	.open = cx81801_open,
	.close = cx81801_close,
	.hangup = cx81801_hangup,
	.receive_buf = cx81801_receive,
	.write_wakeup = cx81801_wakeup,
};



static int ams_delta_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	
	return snd_soc_dai_set_fmt(rtd->cpu_dai,
				   SND_SOC_DAIFMT_DSP_A |
				   SND_SOC_DAIFMT_NB_NF |
				   SND_SOC_DAIFMT_CBM_CFM);
}

static struct snd_soc_ops ams_delta_ops = {
	.hw_params = ams_delta_hw_params,
};


static bool ams_delta_muted = 1;

static int ams_delta_digital_mute(struct snd_soc_dai *dai, int mute)
{
	int apply;

	if (ams_delta_muted == mute)
		return 0;

	spin_lock_bh(&ams_delta_lock);
	ams_delta_muted = mute;
	apply = !cx81801_cmd_pending;
	spin_unlock_bh(&ams_delta_lock);

	if (apply)
		ams_delta_latch2_write(AMS_DELTA_LATCH2_MODEM_CODEC,
				mute ? AMS_DELTA_LATCH2_MODEM_CODEC : 0);
	return 0;
}

static const struct snd_soc_dai_ops ams_delta_dai_ops = {
	.digital_mute = ams_delta_digital_mute,
};

static int ams_delta_startup(struct snd_pcm_substream *substream)
{
	return ams_delta_digital_mute(NULL, 0);
}

static void ams_delta_shutdown(struct snd_pcm_substream *substream)
{
	ams_delta_digital_mute(NULL, 1);
}



static int ams_delta_cx20442_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_card *card = rtd->card;
	int ret;
	

	
	cx20442_codec = codec;

	
	if (!codec_dai->driver->ops) {
		codec_dai->driver->ops = &ams_delta_dai_ops;
	} else {
		ams_delta_ops.startup = ams_delta_startup;
		ams_delta_ops.shutdown = ams_delta_shutdown;
	}

	ret = snd_soc_jack_new(rtd->codec, "hook_switch",
				SND_JACK_HEADSET, &ams_delta_hook_switch);
	if (ret)
		dev_warn(card->dev,
				"Failed to allocate resources for hook switch, "
				"will continue without one.\n");
	else {
		ret = snd_soc_jack_add_gpios(&ams_delta_hook_switch,
					ARRAY_SIZE(ams_delta_hook_switch_gpios),
					ams_delta_hook_switch_gpios);
		if (ret)
			dev_warn(card->dev,
				"Failed to set up hook switch GPIO line, "
				"will continue with hook switch inactive.\n");
	}

	
	ret = tty_register_ldisc(N_V253, &cx81801_ops);
	if (ret) {
		dev_warn(card->dev,
				"Failed to register line discipline, "
				"will continue without any controls.\n");
		return 0;
	}

	
	ret = snd_soc_dapm_new_controls(dapm, ams_delta_dapm_widgets,
					ARRAY_SIZE(ams_delta_dapm_widgets));
	if (ret) {
		dev_warn(card->dev,
				"Failed to register DAPM controls, "
				"will continue without any.\n");
		return 0;
	}

	ret = snd_soc_dapm_add_routes(dapm, ams_delta_audio_map,
					ARRAY_SIZE(ams_delta_audio_map));
	if (ret) {
		dev_warn(card->dev,
				"Failed to set up DAPM routes, "
				"will continue with codec default map.\n");
		return 0;
	}

	
	snd_soc_dapm_disable_pin(dapm, "Mouthpiece");
	snd_soc_dapm_enable_pin(dapm, "Earpiece");
	snd_soc_dapm_enable_pin(dapm, "Microphone");
	snd_soc_dapm_disable_pin(dapm, "Speaker");
	snd_soc_dapm_disable_pin(dapm, "AGCIN");
	snd_soc_dapm_disable_pin(dapm, "AGCOUT");

	
	ret = snd_soc_add_codec_controls(codec, ams_delta_audio_controls,
					ARRAY_SIZE(ams_delta_audio_controls));
	if (ret)
		dev_warn(card->dev,
				"Failed to register audio mode control, "
				"will continue without it.\n");

	return 0;
}

static struct snd_soc_dai_link ams_delta_dai_link = {
	.name = "CX20442",
	.stream_name = "CX20442",
	.cpu_dai_name = "omap-mcbsp.1",
	.codec_dai_name = "cx20442-voice",
	.init = ams_delta_cx20442_init,
	.platform_name = "omap-pcm-audio",
	.codec_name = "cx20442-codec",
	.ops = &ams_delta_ops,
};

static struct snd_soc_card ams_delta_audio_card = {
	.name = "AMS_DELTA",
	.owner = THIS_MODULE,
	.dai_link = &ams_delta_dai_link,
	.num_links = 1,
};

static struct platform_device *ams_delta_audio_platform_device;
static struct platform_device *cx20442_platform_device;

static int __init ams_delta_module_init(void)
{
	int ret;

	if (!(machine_is_ams_delta()))
		return -ENODEV;

	ams_delta_audio_platform_device =
			platform_device_alloc("soc-audio", -1);
	if (!ams_delta_audio_platform_device)
		return -ENOMEM;

	platform_set_drvdata(ams_delta_audio_platform_device,
				&ams_delta_audio_card);

	ret = platform_device_add(ams_delta_audio_platform_device);
	if (ret)
		goto err;

	cx20442_platform_device =
		platform_device_register_simple("cx20442-codec", -1, NULL, 0);
	return 0;
err:
	platform_device_put(ams_delta_audio_platform_device);
	return ret;
}
late_initcall(ams_delta_module_init);

static void __exit ams_delta_module_exit(void)
{
	if (tty_unregister_ldisc(N_V253) != 0)
		dev_warn(&ams_delta_audio_platform_device->dev,
			"failed to unregister V253 line discipline\n");

	snd_soc_jack_free_gpios(&ams_delta_hook_switch,
			ARRAY_SIZE(ams_delta_hook_switch_gpios),
			ams_delta_hook_switch_gpios);

	platform_device_unregister(cx20442_platform_device);
	platform_device_unregister(ams_delta_audio_platform_device);
}
module_exit(ams_delta_module_exit);

MODULE_AUTHOR("Janusz Krzysztofik <jkrzyszt@tis.icnet.pl>");
MODULE_DESCRIPTION("ALSA SoC driver for Amstrad E3 (Delta) videophone");
MODULE_LICENSE("GPL");
