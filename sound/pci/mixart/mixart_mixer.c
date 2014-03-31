/*
 * Driver for Digigram miXart soundcards
 *
 * mixer callbacks
 *
 * Copyright (c) 2003 by Digigram <alsa@digigram.com>
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
#include "mixart.h"
#include "mixart_core.h"
#include "mixart_hwdep.h"
#include <sound/control.h>
#include <sound/tlv.h>
#include "mixart_mixer.h"

static u32 mixart_analog_level[256] = {
	0xc2c00000,		
	0xc2bf0000,		
	0xc2be0000,		
	0xc2bd0000,		
	0xc2bc0000,		
	0xc2bb0000,		
	0xc2ba0000,		
	0xc2b90000,		
	0xc2b80000,		
	0xc2b70000,		
	0xc2b60000,		
	0xc2b50000,		
	0xc2b40000,		
	0xc2b30000,		
	0xc2b20000,		
	0xc2b10000,		
	0xc2b00000,		
	0xc2af0000,		
	0xc2ae0000,		
	0xc2ad0000,		
	0xc2ac0000,		
	0xc2ab0000,		
	0xc2aa0000,		
	0xc2a90000,		
	0xc2a80000,		
	0xc2a70000,		
	0xc2a60000,		
	0xc2a50000,		
	0xc2a40000,		
	0xc2a30000,		
	0xc2a20000,		
	0xc2a10000,		
	0xc2a00000,		
	0xc29f0000,		
	0xc29e0000,		
	0xc29d0000,		
	0xc29c0000,		
	0xc29b0000,		
	0xc29a0000,		
	0xc2990000,		
	0xc2980000,		
	0xc2970000,		
	0xc2960000,		
	0xc2950000,		
	0xc2940000,		
	0xc2930000,		
	0xc2920000,		
	0xc2910000,		
	0xc2900000,		
	0xc28f0000,		
	0xc28e0000,		
	0xc28d0000,		
	0xc28c0000,		
	0xc28b0000,		
	0xc28a0000,		
	0xc2890000,		
	0xc2880000,		
	0xc2870000,		
	0xc2860000,		
	0xc2850000,		
	0xc2840000,		
	0xc2830000,		
	0xc2820000,		
	0xc2810000,		
	0xc2800000,		
	0xc27e0000,		
	0xc27c0000,		
	0xc27a0000,		
	0xc2780000,		
	0xc2760000,		
	0xc2740000,		
	0xc2720000,		
	0xc2700000,		
	0xc26e0000,		
	0xc26c0000,		
	0xc26a0000,		
	0xc2680000,		
	0xc2660000,		
	0xc2640000,		
	0xc2620000,		
	0xc2600000,		
	0xc25e0000,		
	0xc25c0000,		
	0xc25a0000,		
	0xc2580000,		
	0xc2560000,		
	0xc2540000,		
	0xc2520000,		
	0xc2500000,		
	0xc24e0000,		
	0xc24c0000,		
	0xc24a0000,		
	0xc2480000,		
	0xc2460000,		
	0xc2440000,		
	0xc2420000,		
	0xc2400000,		
	0xc23e0000,		
	0xc23c0000,		
	0xc23a0000,		
	0xc2380000,		
	0xc2360000,		
	0xc2340000,		
	0xc2320000,		
	0xc2300000,		
	0xc22e0000,		
	0xc22c0000,		
	0xc22a0000,		
	0xc2280000,		
	0xc2260000,		
	0xc2240000,		
	0xc2220000,		
	0xc2200000,		
	0xc21e0000,		
	0xc21c0000,		
	0xc21a0000,		
	0xc2180000,		
	0xc2160000,		
	0xc2140000,		
	0xc2120000,		
	0xc2100000,		
	0xc20e0000,		
	0xc20c0000,		
	0xc20a0000,		
	0xc2080000,		
	0xc2060000,		
	0xc2040000,		
	0xc2020000,		
	0xc2000000,		
	0xc1fc0000,		
	0xc1f80000,		
	0xc1f40000,		
	0xc1f00000,		
	0xc1ec0000,		
	0xc1e80000,		
	0xc1e40000,		
	0xc1e00000,		
	0xc1dc0000,		
	0xc1d80000,		
	0xc1d40000,		
	0xc1d00000,		
	0xc1cc0000,		
	0xc1c80000,		
	0xc1c40000,		
	0xc1c00000,		
	0xc1bc0000,		
	0xc1b80000,		
	0xc1b40000,		
	0xc1b00000,		
	0xc1ac0000,		
	0xc1a80000,		
	0xc1a40000,		
	0xc1a00000,		
	0xc19c0000,		
	0xc1980000,		
	0xc1940000,		
	0xc1900000,		
	0xc18c0000,		
	0xc1880000,		
	0xc1840000,		
	0xc1800000,		
	0xc1780000,		
	0xc1700000,		
	0xc1680000,		
	0xc1600000,		
	0xc1580000,		
	0xc1500000,		
	0xc1480000,		
	0xc1400000,		
	0xc1380000,		
	0xc1300000,		
	0xc1280000,		
	0xc1200000,		
	0xc1180000,		
	0xc1100000,		
	0xc1080000,		
	0xc1000000,		
	0xc0f00000,		
	0xc0e00000,		
	0xc0d00000,		
	0xc0c00000,		
	0xc0b00000,		
	0xc0a00000,		
	0xc0900000,		
	0xc0800000,		
	0xc0600000,		
	0xc0400000,		
	0xc0200000,		
	0xc0000000,		
	0xbfc00000,		
	0xbf800000,		
	0xbf000000,		
	0x00000000,		
	0x3f000000,		
	0x3f800000,		
	0x3fc00000,		
	0x40000000,		
	0x40200000,		
	0x40400000,		
	0x40600000,		
	0x40800000,		
	0x40900000,		
	0x40a00000,		
	0x40b00000,		
	0x40c00000,		
	0x40d00000,		
	0x40e00000,		
	0x40f00000,		
	0x41000000,		
	0x41080000,		
	0x41100000,		
	0x41180000,		
	0x41200000,		
	0x41280000,		
	0x41300000,		
	0x41380000,		
	0x41400000,		
	0x41480000,		
	0x41500000,		
	0x41580000,		
	0x41600000,		
	0x41680000,		
	0x41700000,		
	0x41780000,		
	0x41800000,		
	0x41840000,		
	0x41880000,		
	0x418c0000,		
	0x41900000,		
	0x41940000,		
	0x41980000,		
	0x419c0000,		
	0x41a00000,		
	0x41a40000,		
	0x41a80000,		
	0x41ac0000,		
	0x41b00000,		
	0x41b40000,		
	0x41b80000,		
	0x41bc0000,		
	0x41c00000,		
	0x41c40000,		
	0x41c80000,		
	0x41cc0000,		
	0x41d00000,		
	0x41d40000,		
	0x41d80000,		
	0x41dc0000,		
	0x41e00000,		
	0x41e40000,		
	0x41e80000,		
	0x41ec0000,		
	0x41f00000,		
	0x41f40000,		
	0x41f80000,		
	0x41fc0000,		
};

#define MIXART_ANALOG_CAPTURE_LEVEL_MIN   0      
#define MIXART_ANALOG_CAPTURE_LEVEL_MAX   255    
#define MIXART_ANALOG_CAPTURE_ZERO_LEVEL  176    

#define MIXART_ANALOG_PLAYBACK_LEVEL_MIN  0      
#define MIXART_ANALOG_PLAYBACK_LEVEL_MAX  192    
#define MIXART_ANALOG_PLAYBACK_ZERO_LEVEL 189    

static int mixart_update_analog_audio_level(struct snd_mixart* chip, int is_capture)
{
	int i, err;
	struct mixart_msg request;
	struct mixart_io_level io_level;
	struct mixart_return_uid resp;

	memset(&io_level, 0, sizeof(io_level));
	io_level.channel = -1; 

	for(i=0; i<2; i++) {
		if(is_capture) {
			io_level.level[i].analog_level = mixart_analog_level[chip->analog_capture_volume[i]];
		} else {
			if(chip->analog_playback_active[i])
				io_level.level[i].analog_level = mixart_analog_level[chip->analog_playback_volume[i]];
			else
				io_level.level[i].analog_level = mixart_analog_level[MIXART_ANALOG_PLAYBACK_LEVEL_MIN];
		}
	}

	if(is_capture)	request.uid = chip->uid_in_analog_physio;
	else		request.uid = chip->uid_out_analog_physio;
	request.message_id = MSG_PHYSICALIO_SET_LEVEL;
	request.data = &io_level;
	request.size = sizeof(io_level);

	err = snd_mixart_send_msg(chip->mgr, &request, sizeof(resp), &resp);
	if((err<0) || (resp.error_code)) {
		snd_printk(KERN_DEBUG "error MSG_PHYSICALIO_SET_LEVEL card(%d) is_capture(%d) error_code(%x)\n", chip->chip_idx, is_capture, resp.error_code);
		return -EINVAL;
	}
	return 0;
}

static int mixart_analog_vol_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
	if(kcontrol->private_value == 0) {	
		uinfo->value.integer.min = MIXART_ANALOG_PLAYBACK_LEVEL_MIN;  
		uinfo->value.integer.max = MIXART_ANALOG_PLAYBACK_LEVEL_MAX;  
	} else {				
		uinfo->value.integer.min = MIXART_ANALOG_CAPTURE_LEVEL_MIN;   
		uinfo->value.integer.max = MIXART_ANALOG_CAPTURE_LEVEL_MAX;   
	}
	return 0;
}

static int mixart_analog_vol_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_mixart *chip = snd_kcontrol_chip(kcontrol);
	mutex_lock(&chip->mgr->mixer_mutex);
	if(kcontrol->private_value == 0) {	
		ucontrol->value.integer.value[0] = chip->analog_playback_volume[0];
		ucontrol->value.integer.value[1] = chip->analog_playback_volume[1];
	} else {				
		ucontrol->value.integer.value[0] = chip->analog_capture_volume[0];
		ucontrol->value.integer.value[1] = chip->analog_capture_volume[1];
	}
	mutex_unlock(&chip->mgr->mixer_mutex);
	return 0;
}

static int mixart_analog_vol_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_mixart *chip = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	int is_capture, i;

	mutex_lock(&chip->mgr->mixer_mutex);
	is_capture = (kcontrol->private_value != 0);
	for (i = 0; i < 2; i++) {
		int new_volume = ucontrol->value.integer.value[i];
		int *stored_volume = is_capture ?
			&chip->analog_capture_volume[i] :
			&chip->analog_playback_volume[i];
		if (is_capture) {
			if (new_volume < MIXART_ANALOG_CAPTURE_LEVEL_MIN ||
			    new_volume > MIXART_ANALOG_CAPTURE_LEVEL_MAX)
				continue;
		} else {
			if (new_volume < MIXART_ANALOG_PLAYBACK_LEVEL_MIN ||
			    new_volume > MIXART_ANALOG_PLAYBACK_LEVEL_MAX)
				continue;
		}
		if (*stored_volume != new_volume) {
			*stored_volume = new_volume;
			changed = 1;
		}
	}
	if (changed)
		mixart_update_analog_audio_level(chip, is_capture);
	mutex_unlock(&chip->mgr->mixer_mutex);
	return changed;
}

static const DECLARE_TLV_DB_SCALE(db_scale_analog, -9600, 50, 0);

static struct snd_kcontrol_new mixart_control_analog_level = {
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = (SNDRV_CTL_ELEM_ACCESS_READWRITE |
		   SNDRV_CTL_ELEM_ACCESS_TLV_READ),
	
	.info =		mixart_analog_vol_info,
	.get =		mixart_analog_vol_get,
	.put =		mixart_analog_vol_put,
	.tlv = { .p = db_scale_analog },
};

#define mixart_sw_info		snd_ctl_boolean_stereo_info

static int mixart_audio_sw_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_mixart *chip = snd_kcontrol_chip(kcontrol);

	mutex_lock(&chip->mgr->mixer_mutex);
	ucontrol->value.integer.value[0] = chip->analog_playback_active[0];
	ucontrol->value.integer.value[1] = chip->analog_playback_active[1];
	mutex_unlock(&chip->mgr->mixer_mutex);
	return 0;
}

static int mixart_audio_sw_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_mixart *chip = snd_kcontrol_chip(kcontrol);
	int i, changed = 0;
	mutex_lock(&chip->mgr->mixer_mutex);
	for (i = 0; i < 2; i++) {
		if (chip->analog_playback_active[i] !=
		    ucontrol->value.integer.value[i]) {
			chip->analog_playback_active[i] =
				!!ucontrol->value.integer.value[i];
			changed = 1;
		}
	}
	if (changed) 
		mixart_update_analog_audio_level(chip, 0);
	mutex_unlock(&chip->mgr->mixer_mutex);
	return changed;
}

static struct snd_kcontrol_new mixart_control_output_switch = {
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =         "Master Playback Switch",
	.info =         mixart_sw_info,		
	.get =          mixart_audio_sw_get,
	.put =          mixart_audio_sw_put
};

static u32 mixart_digital_level[256] = {
	0x00000000,		
	0x366e1c7a,		
	0x367c3860,		
	0x36859525,		
	0x368d7f74,		
	0x3695e1d4,		
	0x369ec362,		
	0x36a82ba8,		
	0x36b222a0,		
	0x36bcb0c1,		
	0x36c7defd,		
	0x36d3b6d3,		
	0x36e0424e,		
	0x36ed8c14,		
	0x36fb9f6c,		
	0x37054423,		
	0x370d29a5,		
	0x371586f0,		
	0x371e631b,		
	0x3727c5ac,		
	0x3731b69a,		
	0x373c3e53,		
	0x374765c8,		
	0x3753366f,		
	0x375fba4f,		
	0x376cfc07,		
	0x377b06d5,		
	0x3784f352,		
	0x378cd40b,		
	0x37952c42,		
	0x379e030e,		
	0x37a75fef,		
	0x37b14ad5,		
	0x37bbcc2c,		
	0x37c6ecdd,		
	0x37d2b65a,		
	0x37df32a3,		
	0x37ec6c50,		
	0x37fa6e9b,		
	0x3804a2b3,		
	0x380c7ea4,		
	0x3814d1cc,		
	0x381da33c,		
	0x3826fa6f,		
	0x3830df51,		
	0x383b5a49,		
	0x3846743b,		
	0x38523692,		
	0x385eab48,		
	0x386bdcf1,		
	0x3879d6bc,		
	0x38845244,		
	0x388c2971,		
	0x3894778d,		
	0x389d43a4,		
	0x38a6952c,		
	0x38b0740f,		
	0x38bae8ac,		
	0x38c5fbe2,		
	0x38d1b717,		
	0x38de2440,		
	0x38eb4de8,		
	0x38f93f3a,		
	0x39040206,		
	0x390bd472,		
	0x39141d84,		
	0x391ce445,		
	0x39263027,		
	0x3930090d,		
	0x393a7753,		
	0x394583d2,		
	0x395137ea,		
	0x395d9d8a,		
	0x396abf37,		
	0x3978a814,		
	0x3983b1f8,		
	0x398b7fa6,		
	0x3993c3b2,		
	0x399c8521,		
	0x39a5cb5f,		
	0x39af9e4d,		
	0x39ba063f,		
	0x39c50c0b,		
	0x39d0b90a,		
	0x39dd1726,		
	0x39ea30db,		
	0x39f81149,		
	0x3a03621b,		
	0x3a0b2b0d,		
	0x3a136a16,		
	0x3a1c2636,		
	0x3a2566d5,		
	0x3a2f33cd,		
	0x3a399570,		
	0x3a44948c,		
	0x3a503a77,		
	0x3a5c9112,		
	0x3a69a2d7,		
	0x3a777ada,		
	0x3a83126f,		
	0x3a8ad6a8,		
	0x3a9310b1,		
	0x3a9bc784,		
	0x3aa50287,		
	0x3aaec98e,		
	0x3ab924e5,		
	0x3ac41d56,		
	0x3acfbc31,		
	0x3adc0b51,		
	0x3ae91528,		
	0x3af6e4c6,		
	0x3b02c2f2,		
	0x3b0a8276,		
	0x3b12b782,		
	0x3b1b690d,		
	0x3b249e76,		
	0x3b2e5f8f,		
	0x3b38b49f,		
	0x3b43a669,		
	0x3b4f3e37,		
	0x3b5b85e0,		
	0x3b6887cf,		
	0x3b764f0e,		
	0x3b8273a6,		
	0x3b8a2e77,		
	0x3b925e89,		
	0x3b9b0ace,		
	0x3ba43aa2,		
	0x3badf5d1,		
	0x3bb8449c,		
	0x3bc32fc3,		
	0x3bcec08a,		
	0x3bdb00c0,		
	0x3be7facc,		
	0x3bf5b9b0,		
	0x3c02248a,		
	0x3c09daac,		
	0x3c1205c6,		
	0x3c1aacc8,		
	0x3c23d70a,		
	0x3c2d8c52,		
	0x3c37d4dd,		
	0x3c42b965,		
	0x3c4e4329,		
	0x3c5a7bf1,		
	0x3c676e1e,		
	0x3c7524ac,		
	0x3c81d59f,		
	0x3c898712,		
	0x3c91ad39,		
	0x3c9a4efc,		
	0x3ca373af,		
	0x3cad2314,		
	0x3cb76563,		
	0x3cc24350,		
	0x3ccdc614,		
	0x3cd9f773,		
	0x3ce6e1c6,		
	0x3cf49003,		
	0x3d0186e2,		
	0x3d0933ac,		
	0x3d1154e1,		
	0x3d19f169,		
	0x3d231090,		
	0x3d2cba15,		
	0x3d36f62b,		
	0x3d41cd81,		
	0x3d4d494a,		
	0x3d597345,		
	0x3d6655c3,		
	0x3d73fbb4,		
	0x3d813856,		
	0x3d88e078,		
	0x3d90fcbf,		
	0x3d99940e,		
	0x3da2adad,		
	0x3dac5156,		
	0x3db68738,		
	0x3dc157fb,		
	0x3dcccccd,		
	0x3dd8ef67,		
	0x3de5ca15,		
	0x3df367bf,		
	0x3e00e9f9,		
	0x3e088d77,		
	0x3e10a4d3,		
	0x3e1936ec,		
	0x3e224b06,		
	0x3e2be8d7,		
	0x3e361887,		
	0x3e40e2bb,		
	0x3e4c509b,		
	0x3e586bd9,		
	0x3e653ebb,		
	0x3e72d424,		
	0x3e809bcc,		
	0x3e883aa8,		
	0x3e904d1c,		
	0x3e98da02,		
	0x3ea1e89b,		
	0x3eab8097,		
	0x3eb5aa1a,		
	0x3ec06dc3,		
	0x3ecbd4b4,		
	0x3ed7e89b,		
	0x3ee4b3b6,		
	0x3ef240e2,		
	0x3f004dce,		
	0x3f07e80b,		
	0x3f0ff59a,		
	0x3f187d50,		
	0x3f21866c,		
	0x3f2b1896,		
	0x3f353bef,		
	0x3f3ff911,		
	0x3f4b5918,		
	0x3f5765ac,		
	0x3f642905,		
	0x3f71adf9,		
	0x3f800000,		
	0x3f8795a0,		
	0x3f8f9e4d,		
	0x3f9820d7,		
	0x3fa12478,		
	0x3faab0d5,		
	0x3fb4ce08,		
	0x3fbf84a6,		
	0x3fcaddc8,		
	0x3fd6e30d,		
	0x3fe39ea9,		
	0x3ff11b6a,		
	0x3fff64c1,		
	0x40074368,		
	0x400f4735,		
	0x4017c496,		
	0x4020c2bf,		
	0x402a4952,		
	0x40346063,		
	0x403f1082,		
	0x404a62c2,		
	0x405660bd,		
	0x406314a0,		
	0x40708933,		
	0x407ec9e1,		
	0x4086f161,		
	0x408ef052,		
	0x4097688d,		
	0x40a06142,		
	0x40a9e20e,		
	0x40b3f300,		
	0x40be9ca5,		
	0x40c9e807,		
	0x40d5debc,		
	0x40e28aeb,		
	0x40eff755,		
	0x40fe2f5e,		
};

#define MIXART_DIGITAL_LEVEL_MIN   0      
#define MIXART_DIGITAL_LEVEL_MAX   255    
#define MIXART_DIGITAL_ZERO_LEVEL  219    


int mixart_update_playback_stream_level(struct snd_mixart* chip, int is_aes, int idx)
{
	int err, i;
	int volume[2];
	struct mixart_msg request;
	struct mixart_set_out_stream_level_req set_level;
	u32 status;
	struct mixart_pipe *pipe;

	memset(&set_level, 0, sizeof(set_level));
	set_level.nb_of_stream = 1;
	set_level.stream_level.desc.stream_idx = idx;

	if(is_aes) {
		pipe = &chip->pipe_out_dig;	
		idx += MIXART_PLAYBACK_STREAMS;
	} else {
		pipe = &chip->pipe_out_ana;	
	}

	
	if(pipe->status == PIPE_UNDEFINED)
		return 0;

	set_level.stream_level.desc.uid_pipe = pipe->group_uid;

	for(i=0; i<2; i++) {
		if(chip->digital_playback_active[idx][i])
			volume[i] = chip->digital_playback_volume[idx][i];
		else
			volume[i] = MIXART_DIGITAL_LEVEL_MIN;
	}

	set_level.stream_level.out_level.valid_mask1 = MIXART_OUT_STREAM_SET_LEVEL_LEFT_AUDIO1 | MIXART_OUT_STREAM_SET_LEVEL_RIGHT_AUDIO2;
	set_level.stream_level.out_level.left_to_out1_level = mixart_digital_level[volume[0]];
	set_level.stream_level.out_level.right_to_out2_level = mixart_digital_level[volume[1]];

	request.message_id = MSG_STREAM_SET_OUT_STREAM_LEVEL;
	request.uid = (struct mixart_uid){0,0};
	request.data = &set_level;
	request.size = sizeof(set_level);

	err = snd_mixart_send_msg(chip->mgr, &request, sizeof(status), &status);
	if((err<0) || status) {
		snd_printk(KERN_DEBUG "error MSG_STREAM_SET_OUT_STREAM_LEVEL card(%d) status(%x)\n", chip->chip_idx, status);
		return -EINVAL;
	}
	return 0;
}

int mixart_update_capture_stream_level(struct snd_mixart* chip, int is_aes)
{
	int err, i, idx;
	struct mixart_pipe *pipe;
	struct mixart_msg request;
	struct mixart_set_in_audio_level_req set_level;
	u32 status;

	if(is_aes) {
		idx = 1;
		pipe = &chip->pipe_in_dig;
	} else {
		idx = 0;
		pipe = &chip->pipe_in_ana;
	}

	
	if(pipe->status == PIPE_UNDEFINED)
		return 0;

	memset(&set_level, 0, sizeof(set_level));
	set_level.audio_count = 2;
	set_level.level[0].connector = pipe->uid_left_connector;
	set_level.level[1].connector = pipe->uid_right_connector;

	for(i=0; i<2; i++) {
		set_level.level[i].valid_mask1 = MIXART_AUDIO_LEVEL_DIGITAL_MASK;
		set_level.level[i].digital_level = mixart_digital_level[chip->digital_capture_volume[idx][i]];
	}

	request.message_id = MSG_STREAM_SET_IN_AUDIO_LEVEL;
	request.uid = (struct mixart_uid){0,0};
	request.data = &set_level;
	request.size = sizeof(set_level);

	err = snd_mixart_send_msg(chip->mgr, &request, sizeof(status), &status);
	if((err<0) || status) {
		snd_printk(KERN_DEBUG "error MSG_STREAM_SET_IN_AUDIO_LEVEL card(%d) status(%x)\n", chip->chip_idx, status);
		return -EINVAL;
	}
	return 0;
}


static int mixart_digital_vol_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
	uinfo->value.integer.min = MIXART_DIGITAL_LEVEL_MIN;   
	uinfo->value.integer.max = MIXART_DIGITAL_LEVEL_MAX;   
	return 0;
}

#define MIXART_VOL_REC_MASK	1
#define MIXART_VOL_AES_MASK	2

static int mixart_pcm_vol_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_mixart *chip = snd_kcontrol_chip(kcontrol);
	int idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id); 
	int *stored_volume;
	int is_capture = kcontrol->private_value & MIXART_VOL_REC_MASK;
	int is_aes = kcontrol->private_value & MIXART_VOL_AES_MASK;
	mutex_lock(&chip->mgr->mixer_mutex);
	if(is_capture) {
		if(is_aes)	stored_volume = chip->digital_capture_volume[1];	
		else		stored_volume = chip->digital_capture_volume[0];	
	} else {
		snd_BUG_ON(idx >= MIXART_PLAYBACK_STREAMS);
		if(is_aes)	stored_volume = chip->digital_playback_volume[MIXART_PLAYBACK_STREAMS + idx]; 
		else		stored_volume = chip->digital_playback_volume[idx];	
	}
	ucontrol->value.integer.value[0] = stored_volume[0];
	ucontrol->value.integer.value[1] = stored_volume[1];
	mutex_unlock(&chip->mgr->mixer_mutex);
	return 0;
}

static int mixart_pcm_vol_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_mixart *chip = snd_kcontrol_chip(kcontrol);
	int idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id); 
	int changed = 0;
	int is_capture = kcontrol->private_value & MIXART_VOL_REC_MASK;
	int is_aes = kcontrol->private_value & MIXART_VOL_AES_MASK;
	int* stored_volume;
	int i;
	mutex_lock(&chip->mgr->mixer_mutex);
	if (is_capture) {
		if (is_aes)	
			stored_volume = chip->digital_capture_volume[1];
		else		
			stored_volume = chip->digital_capture_volume[0];
	} else {
		snd_BUG_ON(idx >= MIXART_PLAYBACK_STREAMS);
		if (is_aes)	
			stored_volume = chip->digital_playback_volume[MIXART_PLAYBACK_STREAMS + idx];
		else		
			stored_volume = chip->digital_playback_volume[idx];
	}
	for (i = 0; i < 2; i++) {
		int vol = ucontrol->value.integer.value[i];
		if (vol < MIXART_DIGITAL_LEVEL_MIN ||
		    vol > MIXART_DIGITAL_LEVEL_MAX)
			continue;
		if (stored_volume[i] != vol) {
			stored_volume[i] = vol;
			changed = 1;
		}
	}
	if (changed) {
		if (is_capture)
			mixart_update_capture_stream_level(chip, is_aes);
		else
			mixart_update_playback_stream_level(chip, is_aes, idx);
	}
	mutex_unlock(&chip->mgr->mixer_mutex);
	return changed;
}

static const DECLARE_TLV_DB_SCALE(db_scale_digital, -10950, 50, 0);

static struct snd_kcontrol_new snd_mixart_pcm_vol =
{
	.iface =        SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = (SNDRV_CTL_ELEM_ACCESS_READWRITE |
		   SNDRV_CTL_ELEM_ACCESS_TLV_READ),
	
	
	.info =         mixart_digital_vol_info,		
	.get =          mixart_pcm_vol_get,
	.put =          mixart_pcm_vol_put,
	.tlv = { .p = db_scale_digital },
};


static int mixart_pcm_sw_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_mixart *chip = snd_kcontrol_chip(kcontrol);
	int idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id); 
	snd_BUG_ON(idx >= MIXART_PLAYBACK_STREAMS);
	mutex_lock(&chip->mgr->mixer_mutex);
	if(kcontrol->private_value & MIXART_VOL_AES_MASK)	
		idx += MIXART_PLAYBACK_STREAMS;
	ucontrol->value.integer.value[0] = chip->digital_playback_active[idx][0];
	ucontrol->value.integer.value[1] = chip->digital_playback_active[idx][1];
	mutex_unlock(&chip->mgr->mixer_mutex);
	return 0;
}

static int mixart_pcm_sw_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_mixart *chip = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	int is_aes = kcontrol->private_value & MIXART_VOL_AES_MASK;
	int idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id); 
	int i, j;
	snd_BUG_ON(idx >= MIXART_PLAYBACK_STREAMS);
	mutex_lock(&chip->mgr->mixer_mutex);
	j = idx;
	if (is_aes)
		j += MIXART_PLAYBACK_STREAMS;
	for (i = 0; i < 2; i++) {
		if (chip->digital_playback_active[j][i] !=
		    ucontrol->value.integer.value[i]) {
			chip->digital_playback_active[j][i] =
				!!ucontrol->value.integer.value[i];
			changed = 1;
		}
	}
	if (changed)
		mixart_update_playback_stream_level(chip, is_aes, idx);
	mutex_unlock(&chip->mgr->mixer_mutex);
	return changed;
}

static struct snd_kcontrol_new mixart_control_pcm_switch = {
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	
	.count =        MIXART_PLAYBACK_STREAMS,
	.info =         mixart_sw_info,		
	.get =          mixart_pcm_sw_get,
	.put =          mixart_pcm_sw_put
};

static int mixart_update_monitoring(struct snd_mixart* chip, int channel)
{
	int err;
	struct mixart_msg request;
	struct mixart_set_out_audio_level audio_level;
	u32 resp;

	if(chip->pipe_out_ana.status == PIPE_UNDEFINED)
		return -EINVAL; 

	if(!channel)	request.uid = chip->pipe_out_ana.uid_left_connector;
	else		request.uid = chip->pipe_out_ana.uid_right_connector;
	request.message_id = MSG_CONNECTOR_SET_OUT_AUDIO_LEVEL;
	request.data = &audio_level;
	request.size = sizeof(audio_level);

	memset(&audio_level, 0, sizeof(audio_level));
	audio_level.valid_mask1 = MIXART_AUDIO_LEVEL_MONITOR_MASK | MIXART_AUDIO_LEVEL_MUTE_M1_MASK;
	audio_level.monitor_level = mixart_digital_level[chip->monitoring_volume[channel!=0]];
	audio_level.monitor_mute1 = !chip->monitoring_active[channel!=0];

	err = snd_mixart_send_msg(chip->mgr, &request, sizeof(resp), &resp);
	if((err<0) || resp) {
		snd_printk(KERN_DEBUG "error MSG_CONNECTOR_SET_OUT_AUDIO_LEVEL card(%d) resp(%x)\n", chip->chip_idx, resp);
		return -EINVAL;
	}
	return 0;
}


static int mixart_monitor_vol_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_mixart *chip = snd_kcontrol_chip(kcontrol);
	mutex_lock(&chip->mgr->mixer_mutex);
	ucontrol->value.integer.value[0] = chip->monitoring_volume[0];
	ucontrol->value.integer.value[1] = chip->monitoring_volume[1];
	mutex_unlock(&chip->mgr->mixer_mutex);
	return 0;
}

static int mixart_monitor_vol_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_mixart *chip = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	int i;
	mutex_lock(&chip->mgr->mixer_mutex);
	for (i = 0; i < 2; i++) {
		if (chip->monitoring_volume[i] !=
		    ucontrol->value.integer.value[i]) {
			chip->monitoring_volume[i] =
				!!ucontrol->value.integer.value[i];
			mixart_update_monitoring(chip, i);
			changed = 1;
		}
	}
	mutex_unlock(&chip->mgr->mixer_mutex);
	return changed;
}

static struct snd_kcontrol_new mixart_control_monitor_vol = {
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = (SNDRV_CTL_ELEM_ACCESS_READWRITE |
		   SNDRV_CTL_ELEM_ACCESS_TLV_READ),
	.name =         "Monitoring Volume",
	.info =		mixart_digital_vol_info,		
	.get =		mixart_monitor_vol_get,
	.put =		mixart_monitor_vol_put,
	.tlv = { .p = db_scale_digital },
};


static int mixart_monitor_sw_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_mixart *chip = snd_kcontrol_chip(kcontrol);
	mutex_lock(&chip->mgr->mixer_mutex);
	ucontrol->value.integer.value[0] = chip->monitoring_active[0];
	ucontrol->value.integer.value[1] = chip->monitoring_active[1];
	mutex_unlock(&chip->mgr->mixer_mutex);
	return 0;
}

static int mixart_monitor_sw_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_mixart *chip = snd_kcontrol_chip(kcontrol);
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
	if (changed) {
		
		int allocate = chip->monitoring_active[0] ||
			chip->monitoring_active[1];
		if (allocate) {
			
			snd_mixart_add_ref_pipe(chip, MIXART_PCM_ANALOG, 0, 1);
			
			snd_mixart_add_ref_pipe(chip, MIXART_PCM_ANALOG, 1, 1);
		}
		if (changed & 0x01)
			mixart_update_monitoring(chip, 0);
		if (changed & 0x02)
			mixart_update_monitoring(chip, 1);
		if (!allocate) {
			
			snd_mixart_kill_ref_pipe(chip->mgr,
						 &chip->pipe_in_ana, 1);
			
			snd_mixart_kill_ref_pipe(chip->mgr,
						 &chip->pipe_out_ana, 1);
		}
	}

	mutex_unlock(&chip->mgr->mixer_mutex);
	return (changed != 0);
}

static struct snd_kcontrol_new mixart_control_monitor_sw = {
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =         "Monitoring Switch",
	.info =         mixart_sw_info,		
	.get =          mixart_monitor_sw_get,
	.put =          mixart_monitor_sw_put
};


static void mixart_reset_audio_levels(struct snd_mixart *chip)
{
	
	mixart_update_analog_audio_level(chip, 0);
	
	if(chip->chip_idx < 2) {
		mixart_update_analog_audio_level(chip, 1);
	}
	return;
}


int snd_mixart_create_mixer(struct mixart_mgr *mgr)
{
	struct snd_mixart *chip;
	int err, i;

	mutex_init(&mgr->mixer_mutex); 

	for(i=0; i<mgr->num_cards; i++) {
		struct snd_kcontrol_new temp;
		chip = mgr->chip[i];

		
		temp = mixart_control_analog_level;
		temp.name = "Master Playback Volume";
		temp.private_value = 0; 
		if ((err = snd_ctl_add(chip->card, snd_ctl_new1(&temp, chip))) < 0)
			return err;
		
		if ((err = snd_ctl_add(chip->card, snd_ctl_new1(&mixart_control_output_switch, chip))) < 0)
			return err;

		
		if(i<2) {
			temp = mixart_control_analog_level;
			temp.name = "Master Capture Volume";
			temp.private_value = 1; 
			if ((err = snd_ctl_add(chip->card, snd_ctl_new1(&temp, chip))) < 0)
				return err;
		}

		temp = snd_mixart_pcm_vol;
		temp.name = "PCM Playback Volume";
		temp.count = MIXART_PLAYBACK_STREAMS;
		temp.private_value = 0; 
		if ((err = snd_ctl_add(chip->card, snd_ctl_new1(&temp, chip))) < 0)
			return err;

		temp.name = "PCM Capture Volume";
		temp.count = 1;
		temp.private_value = MIXART_VOL_REC_MASK; 
		if ((err = snd_ctl_add(chip->card, snd_ctl_new1(&temp, chip))) < 0)
			return err;

		if(mgr->board_type == MIXART_DAUGHTER_TYPE_AES) {
			temp.name = "AES Playback Volume";
			temp.count = MIXART_PLAYBACK_STREAMS;
			temp.private_value = MIXART_VOL_AES_MASK; 
			if ((err = snd_ctl_add(chip->card, snd_ctl_new1(&temp, chip))) < 0)
				return err;

			temp.name = "AES Capture Volume";
			temp.count = 0;
			temp.private_value = MIXART_VOL_REC_MASK | MIXART_VOL_AES_MASK; 
			if ((err = snd_ctl_add(chip->card, snd_ctl_new1(&temp, chip))) < 0)
				return err;
		}
		temp = mixart_control_pcm_switch;
		temp.name = "PCM Playback Switch";
		temp.private_value = 0; 
		if ((err = snd_ctl_add(chip->card, snd_ctl_new1(&temp, chip))) < 0)
			return err;

		if(mgr->board_type == MIXART_DAUGHTER_TYPE_AES) {
			temp.name = "AES Playback Switch";
			temp.private_value = MIXART_VOL_AES_MASK; 
			if ((err = snd_ctl_add(chip->card, snd_ctl_new1(&temp, chip))) < 0)
				return err;
		}

		
		if ((err = snd_ctl_add(chip->card, snd_ctl_new1(&mixart_control_monitor_vol, chip))) < 0)
			return err;
		if ((err = snd_ctl_add(chip->card, snd_ctl_new1(&mixart_control_monitor_sw, chip))) < 0)
			return err;

		
		mixart_reset_audio_levels(chip);
	}
	return 0;
}
