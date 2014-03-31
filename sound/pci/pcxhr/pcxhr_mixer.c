#define __NO_VERSION__
/*
 * Driver for Digigram pcxhr compatible soundcards
 *
 * mixer callbacks
 *
 * Copyright (c) 2004 by Digigram <alsa@digigram.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <sound/core.h>
#include "pcxhr.h"
#include "pcxhr_hwdep.h"
#include "pcxhr_core.h"
#include <sound/control.h>
#include <sound/tlv.h>
#include <sound/asoundef.h>
#include "pcxhr_mixer.h"
#include "pcxhr_mix22.h"

#define PCXHR_LINE_CAPTURE_LEVEL_MIN   0	
#define PCXHR_LINE_CAPTURE_LEVEL_MAX   255	
#define PCXHR_LINE_CAPTURE_ZERO_LEVEL  224	

#define PCXHR_LINE_PLAYBACK_LEVEL_MIN  0	
#define PCXHR_LINE_PLAYBACK_LEVEL_MAX  128	
#define PCXHR_LINE_PLAYBACK_ZERO_LEVEL 104	

static const DECLARE_TLV_DB_SCALE(db_scale_analog_capture, -11200, 50, 1550);
static const DECLARE_TLV_DB_SCALE(db_scale_analog_playback, -10400, 100, 2400);

static const DECLARE_TLV_DB_SCALE(db_scale_a_hr222_capture, -11150, 50, 1600);
static const DECLARE_TLV_DB_SCALE(db_scale_a_hr222_playback, -2550, 50, 2400);

static int pcxhr_update_analog_audio_level(struct snd_pcxhr *chip,
					   int is_capture, int channel)
{
	int err, vol;
	struct pcxhr_rmh rmh;

	pcxhr_init_rmh(&rmh, CMD_ACCESS_IO_WRITE);
	if (is_capture) {
		rmh.cmd[0] |= IO_NUM_REG_IN_ANA_LEVEL;
		rmh.cmd[2] = chip->analog_capture_volume[channel];
	} else {
		rmh.cmd[0] |= IO_NUM_REG_OUT_ANA_LEVEL;
		if (chip->analog_playback_active[channel])
			vol = chip->analog_playback_volume[channel];
		else
			vol = PCXHR_LINE_PLAYBACK_LEVEL_MIN;
		
		rmh.cmd[2] = PCXHR_LINE_PLAYBACK_LEVEL_MAX - vol;
	}
	rmh.cmd[1]  = 1 << ((2 * chip->chip_idx) + channel);	
	rmh.cmd_len = 3;
	err = pcxhr_send_msg(chip->mgr, &rmh);
	if (err < 0) {
		snd_printk(KERN_DEBUG "error update_analog_audio_level card(%d)"
			   " is_capture(%d) err(%x)\n",
			   chip->chip_idx, is_capture, err);
		return -EINVAL;
	}
	return 0;
}

static int pcxhr_analog_vol_info(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_info *uinfo)
{
	struct snd_pcxhr *chip = snd_kcontrol_chip(kcontrol);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
	if (kcontrol->private_value == 0) {	
	    if (chip->mgr->is_hr_stereo) {
		uinfo->value.integer.min =
			HR222_LINE_PLAYBACK_LEVEL_MIN;	
		uinfo->value.integer.max =
			HR222_LINE_PLAYBACK_LEVEL_MAX;	
	    } else {
		uinfo->value.integer.min =
			PCXHR_LINE_PLAYBACK_LEVEL_MIN;	
		uinfo->value.integer.max =
			PCXHR_LINE_PLAYBACK_LEVEL_MAX;	
	    }
	} else {				
	    if (chip->mgr->is_hr_stereo) {
		uinfo->value.integer.min =
			HR222_LINE_CAPTURE_LEVEL_MIN;	
		uinfo->value.integer.max =
			HR222_LINE_CAPTURE_LEVEL_MAX;	
	    } else {
		uinfo->value.integer.min =
			PCXHR_LINE_CAPTURE_LEVEL_MIN;	
		uinfo->value.integer.max =
			PCXHR_LINE_CAPTURE_LEVEL_MAX;	
	    }
	}
	return 0;
}

static int pcxhr_analog_vol_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcxhr *chip = snd_kcontrol_chip(kcontrol);
	mutex_lock(&chip->mgr->mixer_mutex);
	if (kcontrol->private_value == 0) {	
	  ucontrol->value.integer.value[0] = chip->analog_playback_volume[0];
	  ucontrol->value.integer.value[1] = chip->analog_playback_volume[1];
	} else {				
	  ucontrol->value.integer.value[0] = chip->analog_capture_volume[0];
	  ucontrol->value.integer.value[1] = chip->analog_capture_volume[1];
	}
	mutex_unlock(&chip->mgr->mixer_mutex);
	return 0;
}

static int pcxhr_analog_vol_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcxhr *chip = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	int is_capture, i;

	mutex_lock(&chip->mgr->mixer_mutex);
	is_capture = (kcontrol->private_value != 0);
	for (i = 0; i < 2; i++) {
		int  new_volume = ucontrol->value.integer.value[i];
		int *stored_volume = is_capture ?
			&chip->analog_capture_volume[i] :
			&chip->analog_playback_volume[i];
		if (is_capture) {
			if (chip->mgr->is_hr_stereo) {
				if (new_volume < HR222_LINE_CAPTURE_LEVEL_MIN ||
				    new_volume > HR222_LINE_CAPTURE_LEVEL_MAX)
					continue;
			} else {
				if (new_volume < PCXHR_LINE_CAPTURE_LEVEL_MIN ||
				    new_volume > PCXHR_LINE_CAPTURE_LEVEL_MAX)
					continue;
			}
		} else {
			if (chip->mgr->is_hr_stereo) {
				if (new_volume < HR222_LINE_PLAYBACK_LEVEL_MIN ||
				    new_volume > HR222_LINE_PLAYBACK_LEVEL_MAX)
					continue;
			} else {
				if (new_volume < PCXHR_LINE_PLAYBACK_LEVEL_MIN ||
				    new_volume > PCXHR_LINE_PLAYBACK_LEVEL_MAX)
					continue;
			}
		}
		if (*stored_volume != new_volume) {
			*stored_volume = new_volume;
			changed = 1;
			if (chip->mgr->is_hr_stereo)
				hr222_update_analog_audio_level(chip,
								is_capture, i);
			else
				pcxhr_update_analog_audio_level(chip,
								is_capture, i);
		}
	}
	mutex_unlock(&chip->mgr->mixer_mutex);
	return changed;
}

static struct snd_kcontrol_new pcxhr_control_analog_level = {
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.access =	(SNDRV_CTL_ELEM_ACCESS_READWRITE |
			 SNDRV_CTL_ELEM_ACCESS_TLV_READ),
	
	.info =		pcxhr_analog_vol_info,
	.get =		pcxhr_analog_vol_get,
	.put =		pcxhr_analog_vol_put,
	
};


#define pcxhr_sw_info		snd_ctl_boolean_stereo_info

static int pcxhr_audio_sw_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcxhr *chip = snd_kcontrol_chip(kcontrol);

	mutex_lock(&chip->mgr->mixer_mutex);
	ucontrol->value.integer.value[0] = chip->analog_playback_active[0];
	ucontrol->value.integer.value[1] = chip->analog_playback_active[1];
	mutex_unlock(&chip->mgr->mixer_mutex);
	return 0;
}

static int pcxhr_audio_sw_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcxhr *chip = snd_kcontrol_chip(kcontrol);
	int i, changed = 0;
	mutex_lock(&chip->mgr->mixer_mutex);
	for(i = 0; i < 2; i++) {
		if (chip->analog_playback_active[i] !=
		    ucontrol->value.integer.value[i]) {
			chip->analog_playback_active[i] =
				!!ucontrol->value.integer.value[i];
			changed = 1;
			
			if (chip->mgr->is_hr_stereo)
				hr222_update_analog_audio_level(chip, 0, i);
			else
				pcxhr_update_analog_audio_level(chip, 0, i);
		}
	}
	mutex_unlock(&chip->mgr->mixer_mutex);
	return changed;
}

static struct snd_kcontrol_new pcxhr_control_output_switch = {
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =		"Master Playback Switch",
	.info =		pcxhr_sw_info,		
	.get =		pcxhr_audio_sw_get,
	.put =		pcxhr_audio_sw_put
};


#define PCXHR_DIGITAL_LEVEL_MIN		0x000	
#define PCXHR_DIGITAL_LEVEL_MAX		0x1ff	
#define PCXHR_DIGITAL_ZERO_LEVEL	0x1b7	

static const DECLARE_TLV_DB_SCALE(db_scale_digital, -10975, 25, 1800);

#define MORE_THAN_ONE_STREAM_LEVEL	0x000001
#define VALID_STREAM_PAN_LEVEL_MASK	0x800000
#define VALID_STREAM_LEVEL_MASK		0x400000
#define VALID_STREAM_LEVEL_1_MASK	0x200000
#define VALID_STREAM_LEVEL_2_MASK	0x100000

static int pcxhr_update_playback_stream_level(struct snd_pcxhr* chip, int idx)
{
	int err;
	struct pcxhr_rmh rmh;
	struct pcxhr_pipe *pipe = &chip->playback_pipe;
	int left, right;

	if (chip->digital_playback_active[idx][0])
		left = chip->digital_playback_volume[idx][0];
	else
		left = PCXHR_DIGITAL_LEVEL_MIN;
	if (chip->digital_playback_active[idx][1])
		right = chip->digital_playback_volume[idx][1];
	else
		right = PCXHR_DIGITAL_LEVEL_MIN;

	pcxhr_init_rmh(&rmh, CMD_STREAM_OUT_LEVEL_ADJUST);
	
	pcxhr_set_pipe_cmd_params(&rmh, 0, pipe->first_audio, 0, 1<<idx);
	
	rmh.cmd[0] |= MORE_THAN_ONE_STREAM_LEVEL;
	rmh.cmd[2]  = VALID_STREAM_PAN_LEVEL_MASK | VALID_STREAM_LEVEL_1_MASK;
	rmh.cmd[2] |= (left << 10);
	rmh.cmd[3]  = VALID_STREAM_PAN_LEVEL_MASK | VALID_STREAM_LEVEL_2_MASK;
	rmh.cmd[3] |= right;
	rmh.cmd_len = 4;

	err = pcxhr_send_msg(chip->mgr, &rmh);
	if (err < 0) {
		snd_printk(KERN_DEBUG "error update_playback_stream_level "
			   "card(%d) err(%x)\n", chip->chip_idx, err);
		return -EINVAL;
	}
	return 0;
}

#define AUDIO_IO_HAS_MUTE_LEVEL		0x400000
#define AUDIO_IO_HAS_MUTE_MONITOR_1	0x200000
#define VALID_AUDIO_IO_DIGITAL_LEVEL	0x000001
#define VALID_AUDIO_IO_MONITOR_LEVEL	0x000002
#define VALID_AUDIO_IO_MUTE_LEVEL	0x000004
#define VALID_AUDIO_IO_MUTE_MONITOR_1	0x000008

static int pcxhr_update_audio_pipe_level(struct snd_pcxhr *chip,
					 int capture, int channel)
{
	int err;
	struct pcxhr_rmh rmh;
	struct pcxhr_pipe *pipe;

	if (capture)
		pipe = &chip->capture_pipe[0];
	else
		pipe = &chip->playback_pipe;

	pcxhr_init_rmh(&rmh, CMD_AUDIO_LEVEL_ADJUST);
	
	pcxhr_set_pipe_cmd_params(&rmh, capture, 0, 0,
				  1 << (channel + pipe->first_audio));
	if (capture) {
		rmh.cmd[0] |= VALID_AUDIO_IO_DIGITAL_LEVEL;
		rmh.cmd[2] = chip->digital_capture_volume[channel];
	} else {
		rmh.cmd[0] |=	VALID_AUDIO_IO_MONITOR_LEVEL |
				VALID_AUDIO_IO_MUTE_MONITOR_1;
		rmh.cmd[2] = chip->monitoring_volume[channel] << 10;
		if (chip->monitoring_active[channel] == 0)
			rmh.cmd[2] |= AUDIO_IO_HAS_MUTE_MONITOR_1;
	}
	rmh.cmd_len = 3;

	err = pcxhr_send_msg(chip->mgr, &rmh);
	if (err < 0) {
		snd_printk(KERN_DEBUG "error update_audio_level(%d) err=%x\n",
			   chip->chip_idx, err);
		return -EINVAL;
	}
	return 0;
}


static int pcxhr_digital_vol_info(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
	uinfo->value.integer.min = PCXHR_DIGITAL_LEVEL_MIN;   
	uinfo->value.integer.max = PCXHR_DIGITAL_LEVEL_MAX;   
	return 0;
}


static int pcxhr_pcm_vol_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcxhr *chip = snd_kcontrol_chip(kcontrol);
	int idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);	
	int *stored_volume;
	int is_capture = kcontrol->private_value;

	mutex_lock(&chip->mgr->mixer_mutex);
	if (is_capture)		
		stored_volume = chip->digital_capture_volume;
	else			
		stored_volume = chip->digital_playback_volume[idx];
	ucontrol->value.integer.value[0] = stored_volume[0];
	ucontrol->value.integer.value[1] = stored_volume[1];
	mutex_unlock(&chip->mgr->mixer_mutex);
	return 0;
}

static int pcxhr_pcm_vol_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcxhr *chip = snd_kcontrol_chip(kcontrol);
	int idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);	
	int changed = 0;
	int is_capture = kcontrol->private_value;
	int *stored_volume;
	int i;

	mutex_lock(&chip->mgr->mixer_mutex);
	if (is_capture)		
		stored_volume = chip->digital_capture_volume;
	else			
		stored_volume = chip->digital_playback_volume[idx];
	for (i = 0; i < 2; i++) {
		int vol = ucontrol->value.integer.value[i];
		if (vol < PCXHR_DIGITAL_LEVEL_MIN ||
		    vol > PCXHR_DIGITAL_LEVEL_MAX)
			continue;
		if (stored_volume[i] != vol) {
			stored_volume[i] = vol;
			changed = 1;
			if (is_capture)	
				pcxhr_update_audio_pipe_level(chip, 1, i);
		}
	}
	if (!is_capture && changed)	
		pcxhr_update_playback_stream_level(chip, idx);
	mutex_unlock(&chip->mgr->mixer_mutex);
	return changed;
}

static struct snd_kcontrol_new snd_pcxhr_pcm_vol =
{
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.access =	(SNDRV_CTL_ELEM_ACCESS_READWRITE |
			 SNDRV_CTL_ELEM_ACCESS_TLV_READ),
	
	
	.info =		pcxhr_digital_vol_info,		
	.get =		pcxhr_pcm_vol_get,
	.put =		pcxhr_pcm_vol_put,
	.tlv = { .p = db_scale_digital },
};


static int pcxhr_pcm_sw_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcxhr *chip = snd_kcontrol_chip(kcontrol);
	int idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id); 

	mutex_lock(&chip->mgr->mixer_mutex);
	ucontrol->value.integer.value[0] = chip->digital_playback_active[idx][0];
	ucontrol->value.integer.value[1] = chip->digital_playback_active[idx][1];
	mutex_unlock(&chip->mgr->mixer_mutex);
	return 0;
}

static int pcxhr_pcm_sw_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcxhr *chip = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	int idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id); 
	int i, j;

	mutex_lock(&chip->mgr->mixer_mutex);
	j = idx;
	for (i = 0; i < 2; i++) {
		if (chip->digital_playback_active[j][i] !=
		    ucontrol->value.integer.value[i]) {
			chip->digital_playback_active[j][i] =
				!!ucontrol->value.integer.value[i];
			changed = 1;
		}
	}
	if (changed)
		pcxhr_update_playback_stream_level(chip, idx);
	mutex_unlock(&chip->mgr->mixer_mutex);
	return changed;
}

static struct snd_kcontrol_new pcxhr_control_pcm_switch = {
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =		"PCM Playback Switch",
	.count =	PCXHR_PLAYBACK_STREAMS,
	.info =		pcxhr_sw_info,		
	.get =		pcxhr_pcm_sw_get,
	.put =		pcxhr_pcm_sw_put
};



static int pcxhr_monitor_vol_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcxhr *chip = snd_kcontrol_chip(kcontrol);
	mutex_lock(&chip->mgr->mixer_mutex);
	ucontrol->value.integer.value[0] = chip->monitoring_volume[0];
	ucontrol->value.integer.value[1] = chip->monitoring_volume[1];
	mutex_unlock(&chip->mgr->mixer_mutex);
	return 0;
}

static int pcxhr_monitor_vol_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcxhr *chip = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	int i;

	mutex_lock(&chip->mgr->mixer_mutex);
	for (i = 0; i < 2; i++) {
		if (chip->monitoring_volume[i] !=
		    ucontrol->value.integer.value[i]) {
			chip->monitoring_volume[i] =
				ucontrol->value.integer.value[i];
			if (chip->monitoring_active[i])
				
				
				pcxhr_update_audio_pipe_level(chip, 0, i);
			changed = 1;
		}
	}
	mutex_unlock(&chip->mgr->mixer_mutex);
	return changed;
}

static struct snd_kcontrol_new pcxhr_control_monitor_vol = {
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.access =	(SNDRV_CTL_ELEM_ACCESS_READWRITE |
			 SNDRV_CTL_ELEM_ACCESS_TLV_READ),
	.name =         "Monitoring Playback Volume",
	.info =		pcxhr_digital_vol_info,		
	.get =		pcxhr_monitor_vol_get,
	.put =		pcxhr_monitor_vol_put,
	.tlv = { .p = db_scale_digital },
};


static int pcxhr_monitor_sw_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcxhr *chip = snd_kcontrol_chip(kcontrol);
	mutex_lock(&chip->mgr->mixer_mutex);
	ucontrol->value.integer.value[0] = chip->monitoring_active[0];
	ucontrol->value.integer.value[1] = chip->monitoring_active[1];
	mutex_unlock(&chip->mgr->mixer_mutex);
	return 0;
}

static int pcxhr_monitor_sw_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcxhr *chip = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	int i;

	mutex_lock(&chip->mgr->mixer_mutex);
	for (i = 0; i < 2; i++) {
		if (chip->monitoring_active[i] !=
		    ucontrol->value.integer.value[i]) {
			chip->monitoring_active[i] =
				!!ucontrol->value.integer.value[i];
			changed |= (1<<i); 
		}
	}
	if (changed & 0x01)
		
		pcxhr_update_audio_pipe_level(chip, 0, 0);
	if (changed & 0x02)
		
		pcxhr_update_audio_pipe_level(chip, 0, 1);

	mutex_unlock(&chip->mgr->mixer_mutex);
	return (changed != 0);
}

static struct snd_kcontrol_new pcxhr_control_monitor_sw = {
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =         "Monitoring Playback Switch",
	.info =         pcxhr_sw_info,		
	.get =          pcxhr_monitor_sw_get,
	.put =          pcxhr_monitor_sw_put
};



#define PCXHR_SOURCE_AUDIO01_UER	0x000100
#define PCXHR_SOURCE_AUDIO01_SYNC	0x000200
#define PCXHR_SOURCE_AUDIO23_UER	0x000400
#define PCXHR_SOURCE_AUDIO45_UER	0x001000
#define PCXHR_SOURCE_AUDIO67_UER	0x040000

static int pcxhr_set_audio_source(struct snd_pcxhr* chip)
{
	struct pcxhr_rmh rmh;
	unsigned int mask, reg;
	unsigned int codec;
	int err, changed;

	switch (chip->chip_idx) {
	case 0 : mask = PCXHR_SOURCE_AUDIO01_UER; codec = CS8420_01_CS; break;
	case 1 : mask = PCXHR_SOURCE_AUDIO23_UER; codec = CS8420_23_CS; break;
	case 2 : mask = PCXHR_SOURCE_AUDIO45_UER; codec = CS8420_45_CS; break;
	case 3 : mask = PCXHR_SOURCE_AUDIO67_UER; codec = CS8420_67_CS; break;
	default: return -EINVAL;
	}
	if (chip->audio_capture_source != 0) {
		reg = mask;	
	} else {
		reg = 0;	
	}
	
	pcxhr_write_io_num_reg_cont(chip->mgr, mask, reg, &changed);
	
	if (changed) {
		pcxhr_init_rmh(&rmh, CMD_RESYNC_AUDIO_INPUTS);
		rmh.cmd[0] |= (1 << chip->chip_idx);
		err = pcxhr_send_msg(chip->mgr, &rmh);
		if (err)
			return err;
	}
	if (chip->mgr->board_aes_in_192k) {
		int i;
		unsigned int src_config = 0xC0;
		
		for (i = 0; (i < 4) && (i < chip->mgr->capture_chips); i++) {
			if (chip->mgr->chip[i]->audio_capture_source == 2)
				src_config |= (1 << (3 - i));
		}
		
		pcxhr_init_rmh(&rmh, CMD_ACCESS_IO_WRITE);
		rmh.cmd_len = 2;
		rmh.cmd[0] |= IO_NUM_REG_CONFIG_SRC;
		rmh.cmd[1] = src_config;
		err = pcxhr_send_msg(chip->mgr, &rmh);
	} else {
		int use_src = 0;
		if (chip->audio_capture_source == 2)
			use_src = 1;
		
		pcxhr_init_rmh(&rmh, CMD_ACCESS_IO_WRITE);
		rmh.cmd_len = 3;
		rmh.cmd[0] |= IO_NUM_UER_CHIP_REG;
		rmh.cmd[1] = codec;
		rmh.cmd[2] = ((CS8420_DATA_FLOW_CTL & CHIP_SIG_AND_MAP_SPI) |
			      (use_src ? 0x41 : 0x54));
		err = pcxhr_send_msg(chip->mgr, &rmh);
		if (err)
			return err;
		rmh.cmd[2] = ((CS8420_CLOCK_SRC_CTL & CHIP_SIG_AND_MAP_SPI) |
			      (use_src ? 0x41 : 0x49));
		err = pcxhr_send_msg(chip->mgr, &rmh);
	}
	return err;
}

static int pcxhr_audio_src_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	static const char *texts[5] = {
		"Line", "Digital", "Digi+SRC", "Mic", "Line+Mic"
	};
	int i;
	struct snd_pcxhr *chip = snd_kcontrol_chip(kcontrol);

	i = 2;			
	if (chip->mgr->board_has_aes1) {
		i = 3;		
		if (chip->mgr->board_has_mic)
			i = 5;	
	}
	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = i;
	if (uinfo->value.enumerated.item > (i-1))
		uinfo->value.enumerated.item = i-1;
	strcpy(uinfo->value.enumerated.name,
		texts[uinfo->value.enumerated.item]);
	return 0;
}

static int pcxhr_audio_src_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcxhr *chip = snd_kcontrol_chip(kcontrol);
	ucontrol->value.enumerated.item[0] = chip->audio_capture_source;
	return 0;
}

static int pcxhr_audio_src_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcxhr *chip = snd_kcontrol_chip(kcontrol);
	int ret = 0;
	int i = 2;		
	if (chip->mgr->board_has_aes1) {
		i = 3;		
		if (chip->mgr->board_has_mic)
			i = 5;	
	}
	if (ucontrol->value.enumerated.item[0] >= i)
		return -EINVAL;
	mutex_lock(&chip->mgr->mixer_mutex);
	if (chip->audio_capture_source != ucontrol->value.enumerated.item[0]) {
		chip->audio_capture_source = ucontrol->value.enumerated.item[0];
		if (chip->mgr->is_hr_stereo)
			hr222_set_audio_source(chip);
		else
			pcxhr_set_audio_source(chip);
		ret = 1;
	}
	mutex_unlock(&chip->mgr->mixer_mutex);
	return ret;
}

static struct snd_kcontrol_new pcxhr_control_audio_src = {
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =		"Capture Source",
	.info =		pcxhr_audio_src_info,
	.get =		pcxhr_audio_src_get,
	.put =		pcxhr_audio_src_put,
};



static int pcxhr_clock_type_info(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_info *uinfo)
{
	static const char *textsPCXHR[7] = {
		"Internal", "WordClock", "AES Sync",
		"AES 1", "AES 2", "AES 3", "AES 4"
	};
	static const char *textsHR22[3] = {
		"Internal", "AES Sync", "AES 1"
	};
	const char **texts;
	struct pcxhr_mgr *mgr = snd_kcontrol_chip(kcontrol);
	int clock_items = 2;	
	if (mgr->board_has_aes1) {
		clock_items += mgr->capture_chips;	
		if (!mgr->is_hr_stereo)
			clock_items += 1;		
	}
	if (mgr->is_hr_stereo) {
		texts = textsHR22;
		snd_BUG_ON(clock_items > (HR22_CLOCK_TYPE_MAX+1));
	} else {
		texts = textsPCXHR;
		snd_BUG_ON(clock_items > (PCXHR_CLOCK_TYPE_MAX+1));
	}
	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = clock_items;
	if (uinfo->value.enumerated.item >= clock_items)
		uinfo->value.enumerated.item = clock_items-1;
	strcpy(uinfo->value.enumerated.name,
		texts[uinfo->value.enumerated.item]);
	return 0;
}

static int pcxhr_clock_type_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct pcxhr_mgr *mgr = snd_kcontrol_chip(kcontrol);
	ucontrol->value.enumerated.item[0] = mgr->use_clock_type;
	return 0;
}

static int pcxhr_clock_type_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct pcxhr_mgr *mgr = snd_kcontrol_chip(kcontrol);
	int rate, ret = 0;
	unsigned int clock_items = 2; 
	if (mgr->board_has_aes1) {
		clock_items += mgr->capture_chips;	
		if (!mgr->is_hr_stereo)
			clock_items += 1;		
	}
	if (ucontrol->value.enumerated.item[0] >= clock_items)
		return -EINVAL;
	mutex_lock(&mgr->mixer_mutex);
	if (mgr->use_clock_type != ucontrol->value.enumerated.item[0]) {
		mutex_lock(&mgr->setup_mutex);
		mgr->use_clock_type = ucontrol->value.enumerated.item[0];
		rate = 0;
		if (mgr->use_clock_type != PCXHR_CLOCK_TYPE_INTERNAL) {
			pcxhr_get_external_clock(mgr, mgr->use_clock_type,
						 &rate);
		} else {
			rate = mgr->sample_rate;
			if (!rate)
				rate = 48000;
		}
		if (rate) {
			pcxhr_set_clock(mgr, rate);
			if (mgr->sample_rate)
				mgr->sample_rate = rate;
		}
		mutex_unlock(&mgr->setup_mutex);
		ret = 1; 
	}
	mutex_unlock(&mgr->mixer_mutex);
	return ret;
}

static struct snd_kcontrol_new pcxhr_control_clock_type = {
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =		"Clock Mode",
	.info =		pcxhr_clock_type_info,
	.get =		pcxhr_clock_type_get,
	.put =		pcxhr_clock_type_put,
};

static int pcxhr_clock_rate_info(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_info *uinfo)
{
	struct pcxhr_mgr *mgr = snd_kcontrol_chip(kcontrol);
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 3 + mgr->capture_chips;
	uinfo->value.integer.min = 0;		
	uinfo->value.integer.max = 192000;	
	return 0;
}

static int pcxhr_clock_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct pcxhr_mgr *mgr = snd_kcontrol_chip(kcontrol);
	int i, err, rate;

	mutex_lock(&mgr->mixer_mutex);
	for(i = 0; i < 3 + mgr->capture_chips; i++) {
		if (i == PCXHR_CLOCK_TYPE_INTERNAL)
			rate = mgr->sample_rate_real;
		else {
			err = pcxhr_get_external_clock(mgr, i, &rate);
			if (err)
				break;
		}
		ucontrol->value.integer.value[i] = rate;
	}
	mutex_unlock(&mgr->mixer_mutex);
	return 0;
}

static struct snd_kcontrol_new pcxhr_control_clock_rate = {
	.access =	SNDRV_CTL_ELEM_ACCESS_READ,
	.iface =	SNDRV_CTL_ELEM_IFACE_CARD,
	.name =		"Clock Rates",
	.info =		pcxhr_clock_rate_info,
	.get =		pcxhr_clock_rate_get,
};

static int pcxhr_iec958_info(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}

static int pcxhr_iec958_capture_byte(struct snd_pcxhr *chip,
				     int aes_idx, unsigned char *aes_bits)
{
	int i, err;
	unsigned char temp;
	struct pcxhr_rmh rmh;

	pcxhr_init_rmh(&rmh, CMD_ACCESS_IO_READ);
	rmh.cmd[0] |= IO_NUM_UER_CHIP_REG;
	switch (chip->chip_idx) {
	  
	case 0:	rmh.cmd[1] = CS8420_01_CS; break;
	case 1:	rmh.cmd[1] = CS8420_23_CS; break;
	case 2:	rmh.cmd[1] = CS8420_45_CS; break;
	case 3:	rmh.cmd[1] = CS8420_67_CS; break;
	default: return -EINVAL;
	}
	if (chip->mgr->board_aes_in_192k) {
		switch (aes_idx) {
		case 0:	rmh.cmd[2] = CS8416_CSB0; break;
		case 1:	rmh.cmd[2] = CS8416_CSB1; break;
		case 2:	rmh.cmd[2] = CS8416_CSB2; break;
		case 3:	rmh.cmd[2] = CS8416_CSB3; break;
		case 4:	rmh.cmd[2] = CS8416_CSB4; break;
		default: return -EINVAL;
		}
	} else {
		switch (aes_idx) {
		  
		case 0:	rmh.cmd[2] = CS8420_CSB0; break;
		case 1:	rmh.cmd[2] = CS8420_CSB1; break;
		case 2:	rmh.cmd[2] = CS8420_CSB2; break;
		case 3:	rmh.cmd[2] = CS8420_CSB3; break;
		case 4:	rmh.cmd[2] = CS8420_CSB4; break;
		default: return -EINVAL;
		}
	}
	
	rmh.cmd[1] &= 0x0fffff;
	
	rmh.cmd[2] &= CHIP_SIG_AND_MAP_SPI;
	rmh.cmd_len = 3;
	err = pcxhr_send_msg(chip->mgr, &rmh);
	if (err)
		return err;

	if (chip->mgr->board_aes_in_192k) {
		temp = (unsigned char)rmh.stat[1];
	} else {
		temp = 0;
		
		for (i = 0; i < 8; i++) {
			temp <<= 1;
			if (rmh.stat[1] & (1 << i))
				temp |= 1;
		}
	}
	snd_printdd("read iec958 AES %d byte %d = 0x%x\n",
		    chip->chip_idx, aes_idx, temp);
	*aes_bits = temp;
	return 0;
}

static int pcxhr_iec958_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcxhr *chip = snd_kcontrol_chip(kcontrol);
	unsigned char aes_bits;
	int i, err;

	mutex_lock(&chip->mgr->mixer_mutex);
	for(i = 0; i < 5; i++) {
		if (kcontrol->private_value == 0)	
			aes_bits = chip->aes_bits[i];
		else {				
			if (chip->mgr->is_hr_stereo)
				err = hr222_iec958_capture_byte(chip, i,
								&aes_bits);
			else
				err = pcxhr_iec958_capture_byte(chip, i,
								&aes_bits);
			if (err)
				break;
		}
		ucontrol->value.iec958.status[i] = aes_bits;
	}
	mutex_unlock(&chip->mgr->mixer_mutex);
        return 0;
}

static int pcxhr_iec958_mask_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	int i;
	for (i = 0; i < 5; i++)
		ucontrol->value.iec958.status[i] = 0xff;
        return 0;
}

static int pcxhr_iec958_update_byte(struct snd_pcxhr *chip,
				    int aes_idx, unsigned char aes_bits)
{
	int i, err, cmd;
	unsigned char new_bits = aes_bits;
	unsigned char old_bits = chip->aes_bits[aes_idx];
	struct pcxhr_rmh rmh;

	for (i = 0; i < 8; i++) {
		if ((old_bits & 0x01) != (new_bits & 0x01)) {
			cmd = chip->chip_idx & 0x03;      
			if (chip->chip_idx > 3)
				
				cmd |= 1 << 22;
			cmd |= ((aes_idx << 3) + i) << 2; 
			cmd |= (new_bits & 0x01) << 23;   
			pcxhr_init_rmh(&rmh, CMD_ACCESS_IO_WRITE);
			rmh.cmd[0] |= IO_NUM_REG_CUER;
			rmh.cmd[1] = cmd;
			rmh.cmd_len = 2;
			snd_printdd("write iec958 AES %d byte %d bit %d (cmd %x)\n",
				    chip->chip_idx, aes_idx, i, cmd);
			err = pcxhr_send_msg(chip->mgr, &rmh);
			if (err)
				return err;
		}
		old_bits >>= 1;
		new_bits >>= 1;
	}
	chip->aes_bits[aes_idx] = aes_bits;
	return 0;
}

static int pcxhr_iec958_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcxhr *chip = snd_kcontrol_chip(kcontrol);
	int i, changed = 0;

	
	mutex_lock(&chip->mgr->mixer_mutex);
	for (i = 0; i < 5; i++) {
		if (ucontrol->value.iec958.status[i] != chip->aes_bits[i]) {
			if (chip->mgr->is_hr_stereo)
				hr222_iec958_update_byte(chip, i,
					ucontrol->value.iec958.status[i]);
			else
				pcxhr_iec958_update_byte(chip, i,
					ucontrol->value.iec958.status[i]);
			changed = 1;
		}
	}
	mutex_unlock(&chip->mgr->mixer_mutex);
	return changed;
}

static struct snd_kcontrol_new pcxhr_control_playback_iec958_mask = {
	.access =	SNDRV_CTL_ELEM_ACCESS_READ,
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		SNDRV_CTL_NAME_IEC958("",PLAYBACK,MASK),
	.info =		pcxhr_iec958_info,
	.get =		pcxhr_iec958_mask_get
};
static struct snd_kcontrol_new pcxhr_control_playback_iec958 = {
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =         SNDRV_CTL_NAME_IEC958("",PLAYBACK,DEFAULT),
	.info =         pcxhr_iec958_info,
	.get =          pcxhr_iec958_get,
	.put =          pcxhr_iec958_put,
	.private_value = 0 
};

static struct snd_kcontrol_new pcxhr_control_capture_iec958_mask = {
	.access =	SNDRV_CTL_ELEM_ACCESS_READ,
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		SNDRV_CTL_NAME_IEC958("",CAPTURE,MASK),
	.info =		pcxhr_iec958_info,
	.get =		pcxhr_iec958_mask_get
};
static struct snd_kcontrol_new pcxhr_control_capture_iec958 = {
	.access =	SNDRV_CTL_ELEM_ACCESS_READ,
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =         SNDRV_CTL_NAME_IEC958("",CAPTURE,DEFAULT),
	.info =         pcxhr_iec958_info,
	.get =          pcxhr_iec958_get,
	.private_value = 1 
};

static void pcxhr_init_audio_levels(struct snd_pcxhr *chip)
{
	int i;

	for (i = 0; i < 2; i++) {
		if (chip->nb_streams_play) {
			int j;
			
			for (j = 0; j < PCXHR_PLAYBACK_STREAMS; j++) {
				chip->digital_playback_active[j][i] = 1;
				chip->digital_playback_volume[j][i] =
					PCXHR_DIGITAL_ZERO_LEVEL;
			}
			chip->aes_bits[0] = (IEC958_AES0_PROFESSIONAL |
					     IEC958_AES0_PRO_FS_48000);
#ifdef CONFIG_SND_DEBUG
			chip->analog_playback_active[i] = 1;
			if (chip->mgr->is_hr_stereo)
				chip->analog_playback_volume[i] =
					HR222_LINE_PLAYBACK_ZERO_LEVEL;
			else {
				chip->analog_playback_volume[i] =
					PCXHR_LINE_PLAYBACK_ZERO_LEVEL;
				pcxhr_update_analog_audio_level(chip, 0, i);
			}
#endif
			
			if (chip->mgr->is_hr_stereo)
				hr222_update_analog_audio_level(chip, 0, i);
		}
		if (chip->nb_streams_capt) {
			
			chip->digital_capture_volume[i] =
				PCXHR_DIGITAL_ZERO_LEVEL;
			chip->analog_capture_active = 1;
#ifdef CONFIG_SND_DEBUG
			if (chip->mgr->is_hr_stereo)
				chip->analog_capture_volume[i] =
					HR222_LINE_CAPTURE_ZERO_LEVEL;
			else {
				chip->analog_capture_volume[i] =
					PCXHR_LINE_CAPTURE_ZERO_LEVEL;
				pcxhr_update_analog_audio_level(chip, 1, i);
			}
#endif
			
			if (chip->mgr->is_hr_stereo)
				hr222_update_analog_audio_level(chip, 1, i);
		}
	}

	return;
}


int pcxhr_create_mixer(struct pcxhr_mgr *mgr)
{
	struct snd_pcxhr *chip;
	int err, i;

	mutex_init(&mgr->mixer_mutex); 

	for (i = 0; i < mgr->num_cards; i++) {
		struct snd_kcontrol_new temp;
		chip = mgr->chip[i];

		if (chip->nb_streams_play) {
			
			temp = pcxhr_control_analog_level;
			temp.name = "Master Playback Volume";
			temp.private_value = 0; 
			if (mgr->is_hr_stereo)
				temp.tlv.p = db_scale_a_hr222_playback;
			else
				temp.tlv.p = db_scale_analog_playback;
			err = snd_ctl_add(chip->card,
					  snd_ctl_new1(&temp, chip));
			if (err < 0)
				return err;

			
			err = snd_ctl_add(chip->card,
				snd_ctl_new1(&pcxhr_control_output_switch,
					     chip));
			if (err < 0)
				return err;

			temp = snd_pcxhr_pcm_vol;
			temp.name = "PCM Playback Volume";
			temp.count = PCXHR_PLAYBACK_STREAMS;
			temp.private_value = 0; 
			err = snd_ctl_add(chip->card,
					  snd_ctl_new1(&temp, chip));
			if (err < 0)
				return err;

			err = snd_ctl_add(chip->card,
				snd_ctl_new1(&pcxhr_control_pcm_switch, chip));
			if (err < 0)
				return err;

			
			err = snd_ctl_add(chip->card,
				snd_ctl_new1(&pcxhr_control_playback_iec958_mask,
					     chip));
			if (err < 0)
				return err;

			err = snd_ctl_add(chip->card,
				snd_ctl_new1(&pcxhr_control_playback_iec958,
					     chip));
			if (err < 0)
				return err;
		}
		if (chip->nb_streams_capt) {
			
			temp = pcxhr_control_analog_level;
			temp.name = "Line Capture Volume";
			temp.private_value = 1; 
			if (mgr->is_hr_stereo)
				temp.tlv.p = db_scale_a_hr222_capture;
			else
				temp.tlv.p = db_scale_analog_capture;

			err = snd_ctl_add(chip->card,
					  snd_ctl_new1(&temp, chip));
			if (err < 0)
				return err;

			temp = snd_pcxhr_pcm_vol;
			temp.name = "PCM Capture Volume";
			temp.count = 1;
			temp.private_value = 1; 

			err = snd_ctl_add(chip->card,
					  snd_ctl_new1(&temp, chip));
			if (err < 0)
				return err;

			
			err = snd_ctl_add(chip->card,
				snd_ctl_new1(&pcxhr_control_audio_src, chip));
			if (err < 0)
				return err;

			
			err = snd_ctl_add(chip->card,
				snd_ctl_new1(&pcxhr_control_capture_iec958_mask,
					     chip));
			if (err < 0)
				return err;

			err = snd_ctl_add(chip->card,
				snd_ctl_new1(&pcxhr_control_capture_iec958,
					     chip));
			if (err < 0)
				return err;

			if (mgr->is_hr_stereo) {
				err = hr222_add_mic_controls(chip);
				if (err < 0)
					return err;
			}
		}
		
		if (chip->nb_streams_capt > 0 && chip->nb_streams_play > 0) {
			
			err = snd_ctl_add(chip->card,
				snd_ctl_new1(&pcxhr_control_monitor_vol, chip));
			if (err < 0)
				return err;

			err = snd_ctl_add(chip->card,
				snd_ctl_new1(&pcxhr_control_monitor_sw, chip));
			if (err < 0)
				return err;
		}

		if (i == 0) {
			
			err = snd_ctl_add(chip->card,
				snd_ctl_new1(&pcxhr_control_clock_type, mgr));
			if (err < 0)
				return err;
			err = snd_ctl_add(chip->card,
				snd_ctl_new1(&pcxhr_control_clock_rate, mgr));
			if (err < 0)
				return err;
		}

		
		pcxhr_init_audio_levels(chip);
	}

	return 0;
}
