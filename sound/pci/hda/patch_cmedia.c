/*
 * Universal Interface for Intel High Definition Audio Codec
 *
 * HD audio interface patch for C-Media CMI9880
 *
 * Copyright (c) 2004 Takashi Iwai <tiwai@suse.de>
 *
 *
 *  This driver is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This driver is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <sound/core.h>
#include "hda_codec.h"
#include "hda_local.h"
#define NUM_PINS	11


enum {
	CMI_MINIMAL,	
	CMI_MIN_FP,	
	CMI_FULL,	
	CMI_FULL_DIG,	
	CMI_ALLOUT,	
	CMI_AUTO,	
	CMI_MODELS
};

struct cmi_spec {
	int board_config;
	unsigned int no_line_in: 1;	
	unsigned int front_panel: 1;	

	
	struct hda_multi_out multiout;
	hda_nid_t dac_nids[AUTO_CFG_MAX_OUTS];	
	int num_dacs;

	
	const hda_nid_t *adc_nids;
	hda_nid_t dig_in_nid;

	
	const struct hda_input_mux *input_mux;
	unsigned int cur_mux[2];

	
	int num_channel_modes;
	const struct hda_channel_mode *channel_modes;

	struct hda_pcm pcm_rec[2];	

	
	hda_nid_t pin_nid[NUM_PINS];
	unsigned int def_conf[NUM_PINS];
	unsigned int pin_def_confs;

	
	struct hda_verb multi_init[9];	
};

static int cmi_mux_enum_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct cmi_spec *spec = codec->spec;
	return snd_hda_input_mux_info(spec->input_mux, uinfo);
}

static int cmi_mux_enum_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct cmi_spec *spec = codec->spec;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);

	ucontrol->value.enumerated.item[0] = spec->cur_mux[adc_idx];
	return 0;
}

static int cmi_mux_enum_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct cmi_spec *spec = codec->spec;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);

	return snd_hda_input_mux_put(codec, spec->input_mux, ucontrol,
				     spec->adc_nids[adc_idx], &spec->cur_mux[adc_idx]);
}


static const struct hda_verb cmi9880_ch2_init[] = {
	
	{ 0x0c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN },
	
	{ 0x0d, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	
	{ 0x0f, AC_VERB_SET_CONNECT_SEL, 0x00 },
	{}
};

static const struct hda_verb cmi9880_ch6_init[] = {
	
	{ 0x0c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	
	{ 0x0d, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	
	{ 0x0f, AC_VERB_SET_CONNECT_SEL, 0x00 },
	{}
};

static const struct hda_verb cmi9880_ch8_init[] = {
	
	{ 0x0c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	
	{ 0x0d, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	
	{ 0x0f, AC_VERB_SET_CONNECT_SEL, 0x03 },
	{}
};

static const struct hda_channel_mode cmi9880_channel_modes[3] = {
	{ 2, cmi9880_ch2_init },
	{ 6, cmi9880_ch6_init },
	{ 8, cmi9880_ch8_init },
};

static int cmi_ch_mode_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct cmi_spec *spec = codec->spec;
	return snd_hda_ch_mode_info(codec, uinfo, spec->channel_modes,
				    spec->num_channel_modes);
}

static int cmi_ch_mode_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct cmi_spec *spec = codec->spec;
	return snd_hda_ch_mode_get(codec, ucontrol, spec->channel_modes,
				   spec->num_channel_modes, spec->multiout.max_channels);
}

static int cmi_ch_mode_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct cmi_spec *spec = codec->spec;
	return snd_hda_ch_mode_put(codec, ucontrol, spec->channel_modes,
				   spec->num_channel_modes, &spec->multiout.max_channels);
}

static const struct snd_kcontrol_new cmi9880_basic_mixer[] = {
	
	HDA_CODEC_MUTE("PCM Playback Switch", 0x03, 0x0, HDA_OUTPUT), 
	HDA_CODEC_MUTE("Surround Playback Switch", 0x04, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE_MONO("Center Playback Switch", 0x05, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE_MONO("LFE Playback Switch", 0x05, 2, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Side Playback Switch", 0x06, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		
		.name = "Input Source",
		.count = 2,
		.info = cmi_mux_enum_info,
		.get = cmi_mux_enum_get,
		.put = cmi_mux_enum_put,
	},
	HDA_CODEC_VOLUME("Capture Volume", 0x08, 0, HDA_INPUT),
	HDA_CODEC_VOLUME_IDX("Capture Volume", 1, 0x09, 0, HDA_INPUT),
	HDA_CODEC_MUTE("Capture Switch", 0x08, 0, HDA_INPUT),
	HDA_CODEC_MUTE_IDX("Capture Switch", 1, 0x09, 0, HDA_INPUT),
	HDA_CODEC_VOLUME("Beep Playback Volume", 0x23, 0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Beep Playback Switch", 0x23, 0, HDA_OUTPUT),
	{ } 
};

static const struct snd_kcontrol_new cmi9880_ch_mode_mixer[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Channel Mode",
		.info = cmi_ch_mode_info,
		.get = cmi_ch_mode_get,
		.put = cmi_ch_mode_put,
	},
	{ } 
};

static const struct hda_input_mux cmi9880_basic_mux = {
	.num_items = 4,
	.items = {
		{ "Front Mic", 0x5 },
		{ "Rear Mic", 0x2 },
		{ "Line", 0x1 },
		{ "CD", 0x7 },
	}
};

static const struct hda_input_mux cmi9880_no_line_mux = {
	.num_items = 3,
	.items = {
		{ "Front Mic", 0x5 },
		{ "Rear Mic", 0x2 },
		{ "CD", 0x7 },
	}
};

static const hda_nid_t cmi9880_dac_nids[4] = {
	0x03, 0x04, 0x05, 0x06
};
static const hda_nid_t cmi9880_adc_nids[2] = {
	0x08, 0x09
};

#define CMI_DIG_OUT_NID	0x07
#define CMI_DIG_IN_NID	0x0a

static const struct hda_verb cmi9880_basic_init[] = {
	
	{ 0x0b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP },
	
	{ 0x0f, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP },
	
	{ 0x0f, AC_VERB_SET_CONNECT_SEL, 0x00 },
	
	{ 0x0e, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP },
	
	{ 0x1f, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP },
	{ 0x1f, AC_VERB_SET_CONNECT_SEL, 0x02 },
	
	{ 0x20, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP },
	{ 0x20, AC_VERB_SET_CONNECT_SEL, 0x01 },
	
	{ 0x0c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN },
	
	{ 0x0d, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	
	{ 0x10, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	
	{ 0x11, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN },
	
	{ 0x08, AC_VERB_SET_CONNECT_SEL, 0x05 },
	{ 0x09, AC_VERB_SET_CONNECT_SEL, 0x05 },
	{} 
};

static const struct hda_verb cmi9880_allout_init[] = {
	
	{ 0x0b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP },
	
	{ 0x0f, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP },
	
	{ 0x0f, AC_VERB_SET_CONNECT_SEL, 0x00 },
	
	{ 0x0e, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP },
	
	{ 0x1f, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP },
	{ 0x1f, AC_VERB_SET_CONNECT_SEL, 0x02 },
	
	{ 0x20, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP },
	{ 0x20, AC_VERB_SET_CONNECT_SEL, 0x01 },
	
	{ 0x0c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP },
	
	{ 0x0d, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	
	{ 0x10, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	
	{ 0x11, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN },
	
	{ 0x08, AC_VERB_SET_CONNECT_SEL, 0x05 },
	{ 0x09, AC_VERB_SET_CONNECT_SEL, 0x05 },
	{} 
};

static int cmi9880_build_controls(struct hda_codec *codec)
{
	struct cmi_spec *spec = codec->spec;
	struct snd_kcontrol *kctl;
	int i, err;

	err = snd_hda_add_new_ctls(codec, cmi9880_basic_mixer);
	if (err < 0)
		return err;
	if (spec->channel_modes) {
		err = snd_hda_add_new_ctls(codec, cmi9880_ch_mode_mixer);
		if (err < 0)
			return err;
	}
	if (spec->multiout.dig_out_nid) {
		err = snd_hda_create_spdif_out_ctls(codec,
						    spec->multiout.dig_out_nid,
						    spec->multiout.dig_out_nid);
		if (err < 0)
			return err;
		err = snd_hda_create_spdif_share_sw(codec,
						    &spec->multiout);
		if (err < 0)
			return err;
		spec->multiout.share_spdif = 1;
	}
	if (spec->dig_in_nid) {
		err = snd_hda_create_spdif_in_ctls(codec, spec->dig_in_nid);
		if (err < 0)
			return err;
	}

	
	kctl = snd_hda_find_mixer_ctl(codec, "Capture Source");
	for (i = 0; kctl && i < kctl->count; i++) {
		err = snd_hda_add_nid(codec, kctl, i, spec->adc_nids[i]);
		if (err < 0)
			return err;
	}
	return 0;
}

static int cmi9880_fill_multi_dac_nids(struct hda_codec *codec, const struct auto_pin_cfg *cfg)
{
	struct cmi_spec *spec = codec->spec;
	hda_nid_t nid;
	int assigned[4];
	int i, j;

	
	memset(spec->dac_nids, 0, sizeof(spec->dac_nids));
	memset(assigned, 0, sizeof(assigned));
	
	for (i = 0; i < cfg->line_outs; i++) {
		nid = cfg->line_out_pins[i];
		
		if (nid >= 0x0b && nid <= 0x0e) {
			spec->dac_nids[i] = (nid - 0x0b) + 0x03;
			assigned[nid - 0x0b] = 1;
		}
	}
	
	for (i = 0; i < cfg->line_outs; i++) {
		nid = cfg->line_out_pins[i];
		if (nid <= 0x0e)
			continue;
		
		for (j = 0; j < cfg->line_outs; j++) {
			if (! assigned[j]) {
				spec->dac_nids[i] = j + 0x03;
				assigned[j] = 1;
				break;
			}
		}
	}
	spec->num_dacs = cfg->line_outs;
	return 0;
}

static int cmi9880_fill_multi_init(struct hda_codec *codec, const struct auto_pin_cfg *cfg)
{
	struct cmi_spec *spec = codec->spec;
	hda_nid_t nid;
	int i, j, k;

	
	memset(spec->multi_init, 0, sizeof(spec->multi_init));
	for (j = 0, i = 0; i < cfg->line_outs; i++) {
		nid = cfg->line_out_pins[i];
		
		spec->multi_init[j].nid = nid;
		spec->multi_init[j].verb = AC_VERB_SET_PIN_WIDGET_CONTROL;
		spec->multi_init[j].param = PIN_OUT;
		j++;
		if (nid > 0x0e) {
			
			spec->multi_init[j].nid = nid;
			spec->multi_init[j].verb = AC_VERB_SET_CONNECT_SEL;
			spec->multi_init[j].param = 0;
			
			k = snd_hda_get_conn_index(codec, nid,
						   spec->dac_nids[i], 0);
			if (k >= 0)
				spec->multi_init[j].param = k;
			j++;
		}
	}
	return 0;
}

static int cmi9880_init(struct hda_codec *codec)
{
	struct cmi_spec *spec = codec->spec;
	if (spec->board_config == CMI_ALLOUT)
		snd_hda_sequence_write(codec, cmi9880_allout_init);
	else
		snd_hda_sequence_write(codec, cmi9880_basic_init);
	if (spec->board_config == CMI_AUTO)
		snd_hda_sequence_write(codec, spec->multi_init);
	return 0;
}

static int cmi9880_playback_pcm_open(struct hda_pcm_stream *hinfo,
				     struct hda_codec *codec,
				     struct snd_pcm_substream *substream)
{
	struct cmi_spec *spec = codec->spec;
	return snd_hda_multi_out_analog_open(codec, &spec->multiout, substream,
					     hinfo);
}

static int cmi9880_playback_pcm_prepare(struct hda_pcm_stream *hinfo,
					struct hda_codec *codec,
					unsigned int stream_tag,
					unsigned int format,
					struct snd_pcm_substream *substream)
{
	struct cmi_spec *spec = codec->spec;
	return snd_hda_multi_out_analog_prepare(codec, &spec->multiout, stream_tag,
						format, substream);
}

static int cmi9880_playback_pcm_cleanup(struct hda_pcm_stream *hinfo,
				       struct hda_codec *codec,
				       struct snd_pcm_substream *substream)
{
	struct cmi_spec *spec = codec->spec;
	return snd_hda_multi_out_analog_cleanup(codec, &spec->multiout);
}

static int cmi9880_dig_playback_pcm_open(struct hda_pcm_stream *hinfo,
					 struct hda_codec *codec,
					 struct snd_pcm_substream *substream)
{
	struct cmi_spec *spec = codec->spec;
	return snd_hda_multi_out_dig_open(codec, &spec->multiout);
}

static int cmi9880_dig_playback_pcm_close(struct hda_pcm_stream *hinfo,
					  struct hda_codec *codec,
					  struct snd_pcm_substream *substream)
{
	struct cmi_spec *spec = codec->spec;
	return snd_hda_multi_out_dig_close(codec, &spec->multiout);
}

static int cmi9880_dig_playback_pcm_prepare(struct hda_pcm_stream *hinfo,
					    struct hda_codec *codec,
					    unsigned int stream_tag,
					    unsigned int format,
					    struct snd_pcm_substream *substream)
{
	struct cmi_spec *spec = codec->spec;
	return snd_hda_multi_out_dig_prepare(codec, &spec->multiout, stream_tag,
					     format, substream);
}

static int cmi9880_capture_pcm_prepare(struct hda_pcm_stream *hinfo,
				      struct hda_codec *codec,
				      unsigned int stream_tag,
				      unsigned int format,
				      struct snd_pcm_substream *substream)
{
	struct cmi_spec *spec = codec->spec;

	snd_hda_codec_setup_stream(codec, spec->adc_nids[substream->number],
				   stream_tag, 0, format);
	return 0;
}

static int cmi9880_capture_pcm_cleanup(struct hda_pcm_stream *hinfo,
				      struct hda_codec *codec,
				      struct snd_pcm_substream *substream)
{
	struct cmi_spec *spec = codec->spec;

	snd_hda_codec_cleanup_stream(codec, spec->adc_nids[substream->number]);
	return 0;
}


static const struct hda_pcm_stream cmi9880_pcm_analog_playback = {
	.substreams = 1,
	.channels_min = 2,
	.channels_max = 8,
	.nid = 0x03, 
	.ops = {
		.open = cmi9880_playback_pcm_open,
		.prepare = cmi9880_playback_pcm_prepare,
		.cleanup = cmi9880_playback_pcm_cleanup
	},
};

static const struct hda_pcm_stream cmi9880_pcm_analog_capture = {
	.substreams = 2,
	.channels_min = 2,
	.channels_max = 2,
	.nid = 0x08, 
	.ops = {
		.prepare = cmi9880_capture_pcm_prepare,
		.cleanup = cmi9880_capture_pcm_cleanup
	},
};

static const struct hda_pcm_stream cmi9880_pcm_digital_playback = {
	.substreams = 1,
	.channels_min = 2,
	.channels_max = 2,
	
	.ops = {
		.open = cmi9880_dig_playback_pcm_open,
		.close = cmi9880_dig_playback_pcm_close,
		.prepare = cmi9880_dig_playback_pcm_prepare
	},
};

static const struct hda_pcm_stream cmi9880_pcm_digital_capture = {
	.substreams = 1,
	.channels_min = 2,
	.channels_max = 2,
	
};

static int cmi9880_build_pcms(struct hda_codec *codec)
{
	struct cmi_spec *spec = codec->spec;
	struct hda_pcm *info = spec->pcm_rec;

	codec->num_pcms = 1;
	codec->pcm_info = info;

	info->name = "CMI9880";
	info->stream[SNDRV_PCM_STREAM_PLAYBACK] = cmi9880_pcm_analog_playback;
	info->stream[SNDRV_PCM_STREAM_CAPTURE] = cmi9880_pcm_analog_capture;

	if (spec->multiout.dig_out_nid || spec->dig_in_nid) {
		codec->num_pcms++;
		info++;
		info->name = "CMI9880 Digital";
		info->pcm_type = HDA_PCM_TYPE_SPDIF;
		if (spec->multiout.dig_out_nid) {
			info->stream[SNDRV_PCM_STREAM_PLAYBACK] = cmi9880_pcm_digital_playback;
			info->stream[SNDRV_PCM_STREAM_PLAYBACK].nid = spec->multiout.dig_out_nid;
		}
		if (spec->dig_in_nid) {
			info->stream[SNDRV_PCM_STREAM_CAPTURE] = cmi9880_pcm_digital_capture;
			info->stream[SNDRV_PCM_STREAM_CAPTURE].nid = spec->dig_in_nid;
		}
	}

	return 0;
}

static void cmi9880_free(struct hda_codec *codec)
{
	kfree(codec->spec);
}


static const char * const cmi9880_models[CMI_MODELS] = {
	[CMI_MINIMAL]	= "minimal",
	[CMI_MIN_FP]	= "min_fp",
	[CMI_FULL]	= "full",
	[CMI_FULL_DIG]	= "full_dig",
	[CMI_ALLOUT]	= "allout",
	[CMI_AUTO]	= "auto",
};

static const struct snd_pci_quirk cmi9880_cfg_tbl[] = {
	SND_PCI_QUIRK(0x1043, 0x813d, "ASUS P5AD2", CMI_FULL_DIG),
	SND_PCI_QUIRK(0x1854, 0x002b, "LG LS75", CMI_MINIMAL),
	SND_PCI_QUIRK(0x1854, 0x0032, "LG", CMI_FULL_DIG),
	{} 
};

static const struct hda_codec_ops cmi9880_patch_ops = {
	.build_controls = cmi9880_build_controls,
	.build_pcms = cmi9880_build_pcms,
	.init = cmi9880_init,
	.free = cmi9880_free,
};

static int patch_cmi9880(struct hda_codec *codec)
{
	struct cmi_spec *spec;

	spec = kzalloc(sizeof(*spec), GFP_KERNEL);
	if (spec == NULL)
		return -ENOMEM;

	codec->spec = spec;
	spec->board_config = snd_hda_check_board_config(codec, CMI_MODELS,
							cmi9880_models,
							cmi9880_cfg_tbl);
	if (spec->board_config < 0) {
		snd_printdd(KERN_INFO "hda_codec: %s: BIOS auto-probing.\n",
			    codec->chip_name);
		spec->board_config = CMI_AUTO; 
	}

	
	memcpy(spec->dac_nids, cmi9880_dac_nids, sizeof(spec->dac_nids));
	spec->num_dacs = 4;

	switch (spec->board_config) {
	case CMI_MINIMAL:
	case CMI_MIN_FP:
		spec->channel_modes = cmi9880_channel_modes;
		if (spec->board_config == CMI_MINIMAL)
			spec->num_channel_modes = 2;
		else {
			spec->front_panel = 1;
			spec->num_channel_modes = 3;
		}
		spec->multiout.max_channels = cmi9880_channel_modes[0].channels;
		spec->input_mux = &cmi9880_basic_mux;
		break;
	case CMI_FULL:
	case CMI_FULL_DIG:
		spec->front_panel = 1;
		spec->multiout.max_channels = 8;
		spec->input_mux = &cmi9880_basic_mux;
		if (spec->board_config == CMI_FULL_DIG) {
			spec->multiout.dig_out_nid = CMI_DIG_OUT_NID;
			spec->dig_in_nid = CMI_DIG_IN_NID;
		}
		break;
	case CMI_ALLOUT:
		spec->front_panel = 1;
		spec->multiout.max_channels = 8;
		spec->no_line_in = 1;
		spec->input_mux = &cmi9880_no_line_mux;
		spec->multiout.dig_out_nid = CMI_DIG_OUT_NID;
		break;
	case CMI_AUTO:
		{
		unsigned int port_e, port_f, port_g, port_h;
		unsigned int port_spdifi, port_spdifo;
		struct auto_pin_cfg cfg;

		
		port_e = snd_hda_codec_get_pincfg(codec, 0x0f);
		port_f = snd_hda_codec_get_pincfg(codec, 0x10);
		spec->front_panel = 1;
		if (get_defcfg_connect(port_e) == AC_JACK_PORT_NONE ||
		    get_defcfg_connect(port_f) == AC_JACK_PORT_NONE) {
			port_g = snd_hda_codec_get_pincfg(codec, 0x1f);
			port_h = snd_hda_codec_get_pincfg(codec, 0x20);
			spec->channel_modes = cmi9880_channel_modes;
			
			if (get_defcfg_connect(port_g) == AC_JACK_PORT_NONE ||
			    get_defcfg_connect(port_h) == AC_JACK_PORT_NONE) {
				
				spec->board_config = CMI_MINIMAL;
				spec->front_panel = 0;
				spec->num_channel_modes = 2;
			} else {
				spec->board_config = CMI_MIN_FP;
				spec->num_channel_modes = 3;
			}
			spec->input_mux = &cmi9880_basic_mux;
			spec->multiout.max_channels = cmi9880_channel_modes[0].channels;
		} else {
			spec->input_mux = &cmi9880_basic_mux;
			port_spdifi = snd_hda_codec_get_pincfg(codec, 0x13);
			port_spdifo = snd_hda_codec_get_pincfg(codec, 0x12);
			if (get_defcfg_connect(port_spdifo) != AC_JACK_PORT_NONE)
				spec->multiout.dig_out_nid = CMI_DIG_OUT_NID;
			if (get_defcfg_connect(port_spdifi) != AC_JACK_PORT_NONE)
				spec->dig_in_nid = CMI_DIG_IN_NID;
			spec->multiout.max_channels = 8;
		}
		snd_hda_parse_pin_def_config(codec, &cfg, NULL);
		if (cfg.line_outs) {
			spec->multiout.max_channels = cfg.line_outs * 2;
			cmi9880_fill_multi_dac_nids(codec, &cfg);
			cmi9880_fill_multi_init(codec, &cfg);
		} else
			snd_printd("patch_cmedia: cannot detect association in defcfg\n");
		break;
		}
	}

	spec->multiout.num_dacs = spec->num_dacs;
	spec->multiout.dac_nids = spec->dac_nids;

	spec->adc_nids = cmi9880_adc_nids;

	codec->patch_ops = cmi9880_patch_ops;

	return 0;
}

static const struct hda_codec_preset snd_hda_preset_cmedia[] = {
	{ .id = 0x13f69880, .name = "CMI9880", .patch = patch_cmi9880 },
 	{ .id = 0x434d4980, .name = "CMI9880", .patch = patch_cmi9880 },
	{} 
};

MODULE_ALIAS("snd-hda-codec-id:13f69880");
MODULE_ALIAS("snd-hda-codec-id:434d4980");

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("C-Media HD-audio codec");

static struct hda_codec_preset_list cmedia_list = {
	.preset = snd_hda_preset_cmedia,
	.owner = THIS_MODULE,
};

static int __init patch_cmedia_init(void)
{
	return snd_hda_add_codec_preset(&cmedia_list);
}

static void __exit patch_cmedia_exit(void)
{
	snd_hda_delete_codec_preset(&cmedia_list);
}

module_init(patch_cmedia_init)
module_exit(patch_cmedia_exit)
