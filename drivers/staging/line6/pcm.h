/*
 * Line6 Linux USB driver - 0.9.1beta
 *
 * Copyright (C) 2004-2010 Markus Grabner (grabner@icg.tugraz.at)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation, version 2.
 *
 */


#ifndef PCM_H
#define PCM_H

#include <sound/pcm.h>

#include "driver.h"
#include "usbdefs.h"

#define LINE6_ISO_BUFFERS	2

#define LINE6_ISO_PACKETS	1

#define LINE6_ISO_INTERVAL	1

#ifdef CONFIG_LINE6_USB_IMPULSE_RESPONSE
#define LINE6_IMPULSE_DEFAULT_PERIOD 100
#endif

#define get_substream(line6pcm, stream)	\
		(line6pcm->pcm->streams[stream].substream)

enum {
	
	LINE6_INDEX_PCM_ALSA_PLAYBACK_BUFFER,
	LINE6_INDEX_PCM_ALSA_PLAYBACK_STREAM,
	LINE6_INDEX_PCM_ALSA_CAPTURE_BUFFER,
	LINE6_INDEX_PCM_ALSA_CAPTURE_STREAM,
	LINE6_INDEX_PCM_MONITOR_PLAYBACK_BUFFER,
	LINE6_INDEX_PCM_MONITOR_PLAYBACK_STREAM,
	LINE6_INDEX_PCM_MONITOR_CAPTURE_BUFFER,
	LINE6_INDEX_PCM_MONITOR_CAPTURE_STREAM,
#ifdef CONFIG_LINE6_USB_IMPULSE_RESPONSE
	LINE6_INDEX_PCM_IMPULSE_PLAYBACK_BUFFER,
	LINE6_INDEX_PCM_IMPULSE_PLAYBACK_STREAM,
	LINE6_INDEX_PCM_IMPULSE_CAPTURE_BUFFER,
	LINE6_INDEX_PCM_IMPULSE_CAPTURE_STREAM,
#endif
	LINE6_INDEX_PAUSE_PLAYBACK,
	LINE6_INDEX_PREPARED,

	
	LINE6_BIT(PCM_ALSA_PLAYBACK_BUFFER),
	LINE6_BIT(PCM_ALSA_PLAYBACK_STREAM),
	LINE6_BIT(PCM_ALSA_CAPTURE_BUFFER),
	LINE6_BIT(PCM_ALSA_CAPTURE_STREAM),
	LINE6_BIT(PCM_MONITOR_PLAYBACK_BUFFER),
	LINE6_BIT(PCM_MONITOR_PLAYBACK_STREAM),
	LINE6_BIT(PCM_MONITOR_CAPTURE_BUFFER),
	LINE6_BIT(PCM_MONITOR_CAPTURE_STREAM),
#ifdef CONFIG_LINE6_USB_IMPULSE_RESPONSE
	LINE6_BIT(PCM_IMPULSE_PLAYBACK_BUFFER),
	LINE6_BIT(PCM_IMPULSE_PLAYBACK_STREAM),
	LINE6_BIT(PCM_IMPULSE_CAPTURE_BUFFER),
	LINE6_BIT(PCM_IMPULSE_CAPTURE_STREAM),
#endif
	LINE6_BIT(PAUSE_PLAYBACK),
	LINE6_BIT(PREPARED),

	
	LINE6_BITS_PCM_ALSA_BUFFER =
	    LINE6_BIT_PCM_ALSA_PLAYBACK_BUFFER |
	    LINE6_BIT_PCM_ALSA_CAPTURE_BUFFER,

	LINE6_BITS_PCM_ALSA_STREAM =
	    LINE6_BIT_PCM_ALSA_PLAYBACK_STREAM |
	    LINE6_BIT_PCM_ALSA_CAPTURE_STREAM,

	LINE6_BITS_PCM_MONITOR =
	    LINE6_BIT_PCM_MONITOR_PLAYBACK_BUFFER |
	    LINE6_BIT_PCM_MONITOR_PLAYBACK_STREAM |
	    LINE6_BIT_PCM_MONITOR_CAPTURE_BUFFER |
	    LINE6_BIT_PCM_MONITOR_CAPTURE_STREAM,

#ifdef CONFIG_LINE6_USB_IMPULSE_RESPONSE
	LINE6_BITS_PCM_IMPULSE =
	    LINE6_BIT_PCM_IMPULSE_PLAYBACK_BUFFER |
	    LINE6_BIT_PCM_IMPULSE_PLAYBACK_STREAM |
	    LINE6_BIT_PCM_IMPULSE_CAPTURE_BUFFER |
	    LINE6_BIT_PCM_IMPULSE_CAPTURE_STREAM,
#endif

	
	LINE6_BITS_PLAYBACK_BUFFER =
#ifdef CONFIG_LINE6_USB_IMPULSE_RESPONSE
	    LINE6_BIT_PCM_IMPULSE_PLAYBACK_BUFFER |
#endif
	    LINE6_BIT_PCM_ALSA_PLAYBACK_BUFFER |
	    LINE6_BIT_PCM_MONITOR_PLAYBACK_BUFFER ,

	LINE6_BITS_PLAYBACK_STREAM =
#ifdef CONFIG_LINE6_USB_IMPULSE_RESPONSE
	    LINE6_BIT_PCM_IMPULSE_PLAYBACK_STREAM |
#endif
	    LINE6_BIT_PCM_ALSA_PLAYBACK_STREAM |
	    LINE6_BIT_PCM_MONITOR_PLAYBACK_STREAM ,

	LINE6_BITS_CAPTURE_BUFFER =
#ifdef CONFIG_LINE6_USB_IMPULSE_RESPONSE
	    LINE6_BIT_PCM_IMPULSE_CAPTURE_BUFFER |
#endif
	    LINE6_BIT_PCM_ALSA_CAPTURE_BUFFER |
	    LINE6_BIT_PCM_MONITOR_CAPTURE_BUFFER ,

	LINE6_BITS_CAPTURE_STREAM =
#ifdef CONFIG_LINE6_USB_IMPULSE_RESPONSE
	    LINE6_BIT_PCM_IMPULSE_CAPTURE_STREAM |
#endif
	    LINE6_BIT_PCM_ALSA_CAPTURE_STREAM |
	    LINE6_BIT_PCM_MONITOR_CAPTURE_STREAM,
	
	LINE6_BITS_STREAM =
	    LINE6_BITS_PLAYBACK_STREAM |
	    LINE6_BITS_CAPTURE_STREAM
};

struct line6_pcm_properties {
	struct snd_pcm_hardware snd_line6_playback_hw, snd_line6_capture_hw;
	struct snd_pcm_hw_constraint_ratdens snd_line6_rates;
	int bytes_per_frame;
};

struct snd_line6_pcm {
	struct usb_line6 *line6;

	struct line6_pcm_properties *properties;

	struct snd_pcm *pcm;

	struct urb *urb_audio_out[LINE6_ISO_BUFFERS];

	struct urb *urb_audio_in[LINE6_ISO_BUFFERS];

	unsigned char *buffer_out;

	unsigned char *buffer_in;

	unsigned char *prev_fbuf;

	int prev_fsize;

	snd_pcm_uframes_t pos_out;

	unsigned bytes_out;

	unsigned count_out;

	unsigned period_out;

	snd_pcm_uframes_t pos_out_done;

	unsigned bytes_in;

	unsigned count_in;

	unsigned period_in;

	snd_pcm_uframes_t pos_in_done;

	unsigned long active_urb_out;

	int max_packet_size;

	int ep_audio_read;

	int ep_audio_write;

	unsigned long active_urb_in;

	unsigned long unlink_urb_out;

	unsigned long unlink_urb_in;

	spinlock_t lock_audio_out;

	spinlock_t lock_audio_in;

	spinlock_t lock_trigger;

	int volume_playback[2];

	int volume_monitor;

#ifdef CONFIG_LINE6_USB_IMPULSE_RESPONSE
	int impulse_volume;

	int impulse_period;

	int impulse_count;
#endif

	unsigned long flags;

	int last_frame_in, last_frame_out;
};

extern int line6_init_pcm(struct usb_line6 *line6,
			  struct line6_pcm_properties *properties);
extern int snd_line6_trigger(struct snd_pcm_substream *substream, int cmd);
extern int snd_line6_prepare(struct snd_pcm_substream *substream);
extern void line6_pcm_disconnect(struct snd_line6_pcm *line6pcm);
extern int line6_pcm_acquire(struct snd_line6_pcm *line6pcm, int channels);
extern int line6_pcm_release(struct snd_line6_pcm *line6pcm, int channels);

#endif
