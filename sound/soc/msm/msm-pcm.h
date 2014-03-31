/* sound/soc/msm/msm-pcm.h
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2008-2009, 2012 The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org.
 */

#ifndef _MSM_PCM_H
#define _MSM_PCM_H


#include <mach/qdsp5/qdsp5audppcmdi.h>
#include <mach/qdsp5/qdsp5audppmsg.h>
#include <mach/qdsp5/qdsp5audreccmdi.h>
#include <mach/qdsp5/qdsp5audrecmsg.h>
#include <mach/qdsp5/qdsp5audpreproccmdi.h>
#include <mach/qdsp5/qdsp5audpreprocmsg.h>
#include <mach/qdsp5/qdsp5audpp.h>
#include <../arch/arm/mach-msm/qdsp5/adsp.h>
#include <../arch/arm/mach-msm/qdsp5/audmgr.h>


#define FRAME_NUM               (8)
#define FRAME_SIZE              (2052 * 2)
#define MONO_DATA_SIZE          (2048)
#define STEREO_DATA_SIZE        (MONO_DATA_SIZE * 2)
#define CAPTURE_DMASZ           (FRAME_SIZE * FRAME_NUM)

#define BUFSZ			(960 * 5)
#define PLAYBACK_DMASZ 		(BUFSZ * 2)

#define MSM_PLAYBACK_DEFAULT_VOLUME 0 
#define MSM_PLAYBACK_DEFAULT_PAN 0

#define USE_FORMATS             SNDRV_PCM_FMTBIT_S16_LE
#define USE_CHANNELS_MIN        1
#define USE_CHANNELS_MAX        2
#define USE_RATE                \
			(SNDRV_PCM_RATE_8000_48000 | SNDRV_PCM_RATE_KNOT)
#define USE_RATE_MIN            8000
#define USE_RATE_MAX            48000
#define MAX_BUFFER_PLAYBACK_SIZE \
				(4800*4)
#define CAPTURE_SIZE		4096
#define MAX_BUFFER_CAPTURE_SIZE (4096*4)
#define MAX_PERIOD_SIZE         BUFSZ
#define USE_PERIODS_MAX         1024
#define USE_PERIODS_MIN		1


#define MAX_DB			(16)
#define MIN_DB			(-50)
#define PCMPLAYBACK_DECODERID   5

#define	BUF_INVALID_LEN		0xFFFFFFFF

extern int copy_count;
extern int intcnt;

struct msm_volume {
	bool update;
	int volume; 
	int pan;
};

struct buffer {
	void *data;
	unsigned size;
	unsigned used;
	unsigned addr;
};

struct buffer_rec {
	void *data;
	unsigned int size;
	unsigned int read;
	unsigned int addr;
};

struct audio_locks {
	struct mutex lock;
	struct mutex write_lock;
	struct mutex read_lock;
	spinlock_t read_dsp_lock;
	spinlock_t write_dsp_lock;
	spinlock_t mixer_lock;
	wait_queue_head_t read_wait;
	wait_queue_head_t write_wait;
	wait_queue_head_t eos_wait;
};

extern struct audio_locks the_locks;

struct msm_audio_event_callbacks {
	void (*playback)(void *);
	void (*capture)(void *);
};


struct msm_audio {
	struct buffer out[2];
	struct buffer_rec in[8];

	uint8_t out_head;
	uint8_t out_tail;
	uint8_t out_needed; 
	atomic_t out_bytes;

	
	uint32_t out_sample_rate;
	uint32_t out_channel_mode;
	uint32_t out_weight;
	uint32_t out_buffer_size;

	struct audmgr audmgr;
	struct snd_pcm_substream *playback_substream;
	struct snd_pcm_substream *capture_substream;

	
	char *data;
	dma_addr_t phys;

	unsigned int pcm_size;
	unsigned int pcm_count;
	unsigned int pcm_irq_pos;       
	unsigned int pcm_buf_pos;       

	struct msm_adsp_module *audpre;
	struct msm_adsp_module *audrec;

	
	uint32_t samp_rate;
	uint32_t channel_mode;
	uint32_t buffer_size; 
	uint32_t type; 
	uint32_t dsp_cnt;
	uint32_t in_head; 
	uint32_t in_tail; 
	uint32_t in_count; 

	unsigned short samp_rate_index;

	
	audpreproc_cmd_cfg_agc_params tx_agc_cfg;
	audpreproc_cmd_cfg_ns_params ns_cfg;
	audpreproc_cmd_cfg_iir_tuning_filter_params iir_cfg;

	struct  msm_audio_event_callbacks *ops;

	int dir;
	int opened;
	int enabled;
	int running;
	int stopped; 
	int eos_ack;
	int mmap_flag;
	int period;
};



extern int alsa_dsp_send_buffer(struct msm_audio *prtd,
			unsigned idx, unsigned len);
extern int audio_dsp_out_enable(struct msm_audio *prtd, int yes);
extern struct snd_soc_platform_driver msm_soc_platform;

int audrec_encoder_config(struct msm_audio *prtd);
extern void alsa_get_dsp_frames(struct msm_audio *prtd);
extern int alsa_rec_dsp_enable(struct msm_audio *prtd, int enable);
extern int alsa_audrec_disable(struct msm_audio *prtd);
extern int alsa_audio_configure(struct msm_audio *prtd);
extern int alsa_audio_disable(struct msm_audio *prtd);
extern int alsa_adsp_configure(struct msm_audio *prtd);
extern int alsa_buffer_read(struct msm_audio *prtd, void __user *buf,
					size_t count, loff_t *pos);
ssize_t alsa_send_buffer(struct msm_audio *prtd, const char __user *buf,
					size_t count, loff_t *pos);
int msm_audio_volume_update(unsigned id,
				int volume, int pan);
extern struct audio_locks the_locks;
extern struct msm_volume msm_vol_ctl;

#endif 
