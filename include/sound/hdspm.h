#ifndef __SOUND_HDSPM_H
#define __SOUND_HDSPM_H
/*
 *   Copyright (C) 2003 Winfried Ritsch (IEM)
 *   based on hdsp.h from Thomas Charbonnel (thomas@undata.org)
 *
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define HDSPM_MAX_CHANNELS      64

enum hdspm_io_type {
	MADI,
	MADIface,
	AIO,
	AES32,
	RayDAT
};

enum hdspm_speed {
	ss,
	ds,
	qs
};


struct hdspm_peak_rms {
	uint32_t input_peaks[64];
	uint32_t playback_peaks[64];
	uint32_t output_peaks[64];

	uint64_t input_rms[64];
	uint64_t playback_rms[64];
	uint64_t output_rms[64];

	uint8_t speed; 
	int status2;
};

#define SNDRV_HDSPM_IOCTL_GET_PEAK_RMS \
	_IOR('H', 0x42, struct hdspm_peak_rms)


struct hdspm_config {
	unsigned char pref_sync_ref;
	unsigned char wordclock_sync_check;
	unsigned char madi_sync_check;
	unsigned int system_sample_rate;
	unsigned int autosync_sample_rate;
	unsigned char system_clock_mode;
	unsigned char clock_source;
	unsigned char autosync_ref;
	unsigned char line_out;
	unsigned int passthru;
	unsigned int analog_out;
};

#define SNDRV_HDSPM_IOCTL_GET_CONFIG \
	_IOR('H', 0x41, struct hdspm_config)


enum hdspm_ltc_format {
	format_invalid,
	fps_24,
	fps_25,
	fps_2997,
	fps_30
};

enum hdspm_ltc_frame {
	frame_invalid,
	drop_frame,
	full_frame
};

enum hdspm_ltc_input_format {
	ntsc,
	pal,
	no_video
};

struct hdspm_ltc {
	unsigned int ltc;

	enum hdspm_ltc_format format;
	enum hdspm_ltc_frame frame;
	enum hdspm_ltc_input_format input_format;
};

#define SNDRV_HDSPM_IOCTL_GET_LTC _IOR('H', 0x46, struct hdspm_mixer_ioctl)


enum hdspm_sync {
	hdspm_sync_no_lock = 0,
	hdspm_sync_lock = 1,
	hdspm_sync_sync = 2
};

enum hdspm_madi_input {
	hdspm_input_optical = 0,
	hdspm_input_coax = 1
};

enum hdspm_madi_channel_format {
	hdspm_format_ch_64 = 0,
	hdspm_format_ch_56 = 1
};

enum hdspm_madi_frame_format {
	hdspm_frame_48 = 0,
	hdspm_frame_96 = 1
};

enum hdspm_syncsource {
	syncsource_wc = 0,
	syncsource_madi = 1,
	syncsource_tco = 2,
	syncsource_sync = 3,
	syncsource_none = 4
};

struct hdspm_status {
	uint8_t card_type; 
	enum hdspm_syncsource autosync_source;

	uint64_t card_clock;
	uint32_t master_period;

	union {
		struct {
			uint8_t sync_wc; 
			uint8_t sync_madi; 
			uint8_t sync_tco; 
			uint8_t sync_in; 
			uint8_t madi_input; 
			uint8_t channel_format; 
			uint8_t frame_format; 
		} madi;
	} card_specific;
};

#define SNDRV_HDSPM_IOCTL_GET_STATUS \
	_IOR('H', 0x47, struct hdspm_status)


#define HDSPM_ADDON_TCO 1

struct hdspm_version {
	uint8_t card_type; 
	char cardname[20];
	unsigned int serial;
	unsigned short firmware_rev;
	int addons;
};

#define SNDRV_HDSPM_IOCTL_GET_VERSION _IOR('H', 0x48, struct hdspm_version)




#define HDSPM_MIXER_CHANNELS HDSPM_MAX_CHANNELS

struct hdspm_channelfader {
	unsigned int in[HDSPM_MIXER_CHANNELS];
	unsigned int pb[HDSPM_MIXER_CHANNELS];
};

struct hdspm_mixer {
	struct hdspm_channelfader ch[HDSPM_MIXER_CHANNELS];
};

struct hdspm_mixer_ioctl {
	struct hdspm_mixer *mixer;
};

#define SNDRV_HDSPM_IOCTL_GET_MIXER _IOR('H', 0x44, struct hdspm_mixer_ioctl)

typedef struct hdspm_peak_rms hdspm_peak_rms_t;
typedef struct hdspm_config_info hdspm_config_info_t;
typedef struct hdspm_version hdspm_version_t;
typedef struct hdspm_channelfader snd_hdspm_channelfader_t;
typedef struct hdspm_mixer hdspm_mixer_t;


#endif
