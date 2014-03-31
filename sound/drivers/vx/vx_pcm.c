/*
 * Driver for Digigram VX soundcards
 *
 * PCM part
 *
 * Copyright (c) 2002,2003 by Takashi Iwai <tiwai@suse.de>
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
 *
 *
 * STRATEGY
 *  for playback, we send series of "chunks", which size is equal with the
 *  IBL size, typically 126 samples.  at each end of chunk, the end-of-buffer
 *  interrupt is notified, and the interrupt handler will feed the next chunk.
 *
 *  the current position is calculated from the sample count RMH.
 *  pipe->transferred is the counter of data which has been already transferred.
 *  if this counter reaches to the period size, snd_pcm_period_elapsed() will
 *  be issued.
 *
 *  for capture, the situation is much easier.
 *  to get a low latency response, we'll check the capture streams at each
 *  interrupt (capture stream has no EOB notification).  if the pending
 *  data is accumulated to the period size, snd_pcm_period_elapsed() is
 *  called and the pointer is updated.
 *
 *  the current point of read buffer is kept in pipe->hw_ptr.  note that
 *  this is in bytes.
 *
 *
 * TODO
 *  - linked trigger for full-duplex mode.
 *  - scheduled action on the stream.
 */

#include <linux/slab.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/asoundef.h>
#include <sound/pcm.h>
#include <sound/vx_core.h>
#include "vx_cmd.h"


static void vx_pcm_read_per_bytes(struct vx_core *chip, struct snd_pcm_runtime *runtime,
				  struct vx_pipe *pipe)
{
	int offset = pipe->hw_ptr;
	unsigned char *buf = (unsigned char *)(runtime->dma_area + offset);
	*buf++ = vx_inb(chip, RXH);
	if (++offset >= pipe->buffer_bytes) {
		offset = 0;
		buf = (unsigned char *)runtime->dma_area;
	}
	*buf++ = vx_inb(chip, RXM);
	if (++offset >= pipe->buffer_bytes) {
		offset = 0;
		buf = (unsigned char *)runtime->dma_area;
	}
	*buf++ = vx_inb(chip, RXL);
	if (++offset >= pipe->buffer_bytes) {
		offset = 0;
		buf = (unsigned char *)runtime->dma_area;
	}
	pipe->hw_ptr = offset;
}

static void vx_set_pcx_time(struct vx_core *chip, pcx_time_t *pc_time,
			    unsigned int *dsp_time)
{
	dsp_time[0] = (unsigned int)((*pc_time) >> 24) & PCX_TIME_HI_MASK;
	dsp_time[1] = (unsigned int)(*pc_time) &  MASK_DSP_WORD;
}

static int vx_set_differed_time(struct vx_core *chip, struct vx_rmh *rmh,
				struct vx_pipe *pipe)
{
	
	if (! (pipe->differed_type & DC_DIFFERED_DELAY))
		return 0;
		
	
	rmh->Cmd[0] |= DSP_DIFFERED_COMMAND_MASK;

	
	vx_set_pcx_time(chip, &pipe->pcx_time, &rmh->Cmd[1]);

	
	if (pipe->differed_type & DC_NOTIFY_DELAY)
		rmh->Cmd[1] |= NOTIFY_MASK_TIME_HIGH ;

	
	if (pipe->differed_type & DC_MULTIPLE_DELAY)
		rmh->Cmd[1] |= MULTIPLE_MASK_TIME_HIGH;

	
	if (pipe->differed_type & DC_STREAM_TIME_DELAY)
		rmh->Cmd[1] |= STREAM_MASK_TIME_HIGH;
		
	rmh->LgCmd += 2;
	return 2;
}

static int vx_set_stream_format(struct vx_core *chip, struct vx_pipe *pipe,
				unsigned int data)
{
	struct vx_rmh rmh;

	vx_init_rmh(&rmh, pipe->is_capture ?
		    CMD_FORMAT_STREAM_IN : CMD_FORMAT_STREAM_OUT);
	rmh.Cmd[0] |= pipe->number << FIELD_SIZE;

        
	vx_set_differed_time(chip, &rmh, pipe);

	rmh.Cmd[rmh.LgCmd] = (data & 0xFFFFFF00) >> 8;
	rmh.Cmd[rmh.LgCmd + 1] = (data & 0xFF) << 16 ;
	rmh.LgCmd += 2;
    
	return vx_send_msg(chip, &rmh);
}


static int vx_set_format(struct vx_core *chip, struct vx_pipe *pipe,
			 struct snd_pcm_runtime *runtime)
{
	unsigned int header = HEADER_FMT_BASE;

	if (runtime->channels == 1)
		header |= HEADER_FMT_MONO;
	if (snd_pcm_format_little_endian(runtime->format))
		header |= HEADER_FMT_INTEL;
	if (runtime->rate < 32000 && runtime->rate > 11025)
		header |= HEADER_FMT_UPTO32;
	else if (runtime->rate <= 11025)
		header |= HEADER_FMT_UPTO11;

	switch (snd_pcm_format_physical_width(runtime->format)) {
	
	case 16: header |= HEADER_FMT_16BITS; break;
	case 24: header |= HEADER_FMT_24BITS; break;
	default : 
		snd_BUG();
		return -EINVAL;
        };

	return vx_set_stream_format(chip, pipe, header);
}

static int vx_set_ibl(struct vx_core *chip, struct vx_ibl_info *info)
{
	int err;
	struct vx_rmh rmh;

	vx_init_rmh(&rmh, CMD_IBL);
	rmh.Cmd[0] |= info->size & 0x03ffff;
	err = vx_send_msg(chip, &rmh);
	if (err < 0)
		return err;
	info->size = rmh.Stat[0];
	info->max_size = rmh.Stat[1];
	info->min_size = rmh.Stat[2];
	info->granularity = rmh.Stat[3];
	snd_printdd(KERN_DEBUG "vx_set_ibl: size = %d, max = %d, min = %d, gran = %d\n",
		   info->size, info->max_size, info->min_size, info->granularity);
	return 0;
}


static int vx_get_pipe_state(struct vx_core *chip, struct vx_pipe *pipe, int *state)
{
	int err;
	struct vx_rmh rmh;

	vx_init_rmh(&rmh, CMD_PIPE_STATE);
	vx_set_pipe_cmd_params(&rmh, pipe->is_capture, pipe->number, 0);
	err = vx_send_msg_nolock(chip, &rmh);  
	if (! err)
		*state = (rmh.Stat[0] & (1 << pipe->number)) ? 1 : 0;
	return err;
}


static int vx_query_hbuffer_size(struct vx_core *chip, struct vx_pipe *pipe)
{
	int result;
	struct vx_rmh rmh;

	vx_init_rmh(&rmh, CMD_SIZE_HBUFFER);
	vx_set_pipe_cmd_params(&rmh, pipe->is_capture, pipe->number, 0);
	if (pipe->is_capture)
		rmh.Cmd[0] |= 0x00000001;
	result = vx_send_msg(chip, &rmh);
	if (! result)
		result = rmh.Stat[0] & 0xffff;
	return result;
}


static int vx_pipe_can_start(struct vx_core *chip, struct vx_pipe *pipe)
{
	int err;
	struct vx_rmh rmh;
        
	vx_init_rmh(&rmh, CMD_CAN_START_PIPE);
	vx_set_pipe_cmd_params(&rmh, pipe->is_capture, pipe->number, 0);
	rmh.Cmd[0] |= 1;

	err = vx_send_msg_nolock(chip, &rmh);  
	if (! err) {
		if (rmh.Stat[0])
			err = 1;
	}
	return err;
}

static int vx_conf_pipe(struct vx_core *chip, struct vx_pipe *pipe)
{
	struct vx_rmh rmh;

	vx_init_rmh(&rmh, CMD_CONF_PIPE);
	if (pipe->is_capture)
		rmh.Cmd[0] |= COMMAND_RECORD_MASK;
	rmh.Cmd[1] = 1 << pipe->number;
	return vx_send_msg_nolock(chip, &rmh); 
}

static int vx_send_irqa(struct vx_core *chip)
{
	struct vx_rmh rmh;

	vx_init_rmh(&rmh, CMD_SEND_IRQA);
	return vx_send_msg_nolock(chip, &rmh);  
}


#define MAX_WAIT_FOR_DSP        250
#define CAN_START_DELAY         2	
#define WAIT_STATE_DELAY        2	

static int vx_toggle_pipe(struct vx_core *chip, struct vx_pipe *pipe, int state)
{
	int err, i, cur_state;

	
	if (vx_get_pipe_state(chip, pipe, &cur_state) < 0)
		return -EBADFD;
	if (state == cur_state)
		return 0;

	if (state) {
		for (i = 0 ; i < MAX_WAIT_FOR_DSP; i++) {
			err = vx_pipe_can_start(chip, pipe);
			if (err > 0)
				break;
			mdelay(1);
		}
	}
    
	if ((err = vx_conf_pipe(chip, pipe)) < 0)
		return err;

	if ((err = vx_send_irqa(chip)) < 0)
		return err;
    
	for (i = 0; i < MAX_WAIT_FOR_DSP; i++) {
		err = vx_get_pipe_state(chip, pipe, &cur_state);
		if (err < 0 || cur_state == state)
			break;
		err = -EIO;
		mdelay(1);
	}
	return err < 0 ? -EIO : 0;
}

    
static int vx_stop_pipe(struct vx_core *chip, struct vx_pipe *pipe)
{
	struct vx_rmh rmh;
	vx_init_rmh(&rmh, CMD_STOP_PIPE);
	vx_set_pipe_cmd_params(&rmh, pipe->is_capture, pipe->number, 0);
	return vx_send_msg_nolock(chip, &rmh);  
}


static int vx_alloc_pipe(struct vx_core *chip, int capture,
			 int audioid, int num_audio,
			 struct vx_pipe **pipep)
{
	int err;
	struct vx_pipe *pipe;
	struct vx_rmh rmh;
	int data_mode;

	*pipep = NULL;
	vx_init_rmh(&rmh, CMD_RES_PIPE);
	vx_set_pipe_cmd_params(&rmh, capture, audioid, num_audio);
#if 0	
	if (underrun_skip_sound)
		rmh.Cmd[0] |= BIT_SKIP_SOUND;
#endif	
	data_mode = (chip->uer_bits & IEC958_AES0_NONAUDIO) != 0;
	if (! capture && data_mode)
		rmh.Cmd[0] |= BIT_DATA_MODE;
	err = vx_send_msg(chip, &rmh);
	if (err < 0)
		return err;

	
	pipe = kzalloc(sizeof(*pipe), GFP_KERNEL);
	if (! pipe) {
		
		vx_init_rmh(&rmh, CMD_FREE_PIPE);
		vx_set_pipe_cmd_params(&rmh, capture, audioid, 0);
		vx_send_msg(chip, &rmh);
		return -ENOMEM;
	}

	
	pipe->number = audioid;
	pipe->is_capture = capture;
	pipe->channels = num_audio;
	pipe->differed_type = 0;
	pipe->pcx_time = 0;
	pipe->data_mode = data_mode;
	*pipep = pipe;

	return 0;
}


static int vx_free_pipe(struct vx_core *chip, struct vx_pipe *pipe)
{
	struct vx_rmh rmh;

	vx_init_rmh(&rmh, CMD_FREE_PIPE);
	vx_set_pipe_cmd_params(&rmh, pipe->is_capture, pipe->number, 0);
	vx_send_msg(chip, &rmh);

	kfree(pipe);
	return 0;
}


static int vx_start_stream(struct vx_core *chip, struct vx_pipe *pipe)
{
	struct vx_rmh rmh;

	vx_init_rmh(&rmh, CMD_START_ONE_STREAM);
	vx_set_stream_cmd_params(&rmh, pipe->is_capture, pipe->number);
	vx_set_differed_time(chip, &rmh, pipe);
	return vx_send_msg_nolock(chip, &rmh);  
}


static int vx_stop_stream(struct vx_core *chip, struct vx_pipe *pipe)
{
	struct vx_rmh rmh;

	vx_init_rmh(&rmh, CMD_STOP_STREAM);
	vx_set_stream_cmd_params(&rmh, pipe->is_capture, pipe->number);
	return vx_send_msg_nolock(chip, &rmh);  
}



static struct snd_pcm_hardware vx_pcm_playback_hw = {
	.info =			(SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_MMAP_VALID 
				 ),
	.formats =		(
				 SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_3LE),
	.rates =		SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000,
	.rate_min =		5000,
	.rate_max =		48000,
	.channels_min =		1,
	.channels_max =		2,
	.buffer_bytes_max =	(128*1024),
	.period_bytes_min =	126,
	.period_bytes_max =	(128*1024),
	.periods_min =		2,
	.periods_max =		VX_MAX_PERIODS,
	.fifo_size =		126,
};


static void vx_pcm_delayed_start(unsigned long arg);

static int vx_pcm_playback_open(struct snd_pcm_substream *subs)
{
	struct snd_pcm_runtime *runtime = subs->runtime;
	struct vx_core *chip = snd_pcm_substream_chip(subs);
	struct vx_pipe *pipe = NULL;
	unsigned int audio;
	int err;

	if (chip->chip_status & VX_STAT_IS_STALE)
		return -EBUSY;

	audio = subs->pcm->device * 2;
	if (snd_BUG_ON(audio >= chip->audio_outs))
		return -EINVAL;
	
	
	pipe = chip->playback_pipes[audio];
	if (! pipe) {
		
		err = vx_alloc_pipe(chip, 0, audio, 2, &pipe); 
		if (err < 0)
			return err;
		chip->playback_pipes[audio] = pipe;
	}
	
	pipe->references++;

	pipe->substream = subs;
	tasklet_init(&pipe->start_tq, vx_pcm_delayed_start, (unsigned long)subs);
	chip->playback_pipes[audio] = pipe;

	runtime->hw = vx_pcm_playback_hw;
	runtime->hw.period_bytes_min = chip->ibl.size;
	runtime->private_data = pipe;

	 
	snd_pcm_hw_constraint_step(runtime, 0, SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 4);
	snd_pcm_hw_constraint_step(runtime, 0, SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 4);

	return 0;
}

static int vx_pcm_playback_close(struct snd_pcm_substream *subs)
{
	struct vx_core *chip = snd_pcm_substream_chip(subs);
	struct vx_pipe *pipe;

	if (! subs->runtime->private_data)
		return -EINVAL;

	pipe = subs->runtime->private_data;

	if (--pipe->references == 0) {
		chip->playback_pipes[pipe->number] = NULL;
		vx_free_pipe(chip, pipe);
	}

	return 0;

}


static int vx_notify_end_of_buffer(struct vx_core *chip, struct vx_pipe *pipe)
{
	int err;
	struct vx_rmh rmh;  

	
	vx_send_rih_nolock(chip, IRQ_PAUSE_START_CONNECT);
	vx_init_rmh(&rmh, CMD_NOTIFY_END_OF_BUFFER);
	vx_set_stream_cmd_params(&rmh, 0, pipe->number);
	err = vx_send_msg_nolock(chip, &rmh);
	if (err < 0)
		return err;
	
	vx_send_rih_nolock(chip, IRQ_PAUSE_START_CONNECT);
	return 0;
}

static int vx_pcm_playback_transfer_chunk(struct vx_core *chip,
					  struct snd_pcm_runtime *runtime,
					  struct vx_pipe *pipe, int size)
{
	int space, err = 0;

	space = vx_query_hbuffer_size(chip, pipe);
	if (space < 0) {
		
		vx_send_rih(chip, IRQ_CONNECT_STREAM_NEXT);
		snd_printd("error hbuffer\n");
		return space;
	}
	if (space < size) {
		vx_send_rih(chip, IRQ_CONNECT_STREAM_NEXT);
		snd_printd("no enough hbuffer space %d\n", space);
		return -EIO; 
	}
		
	spin_lock(&chip->lock); 
	vx_pseudo_dma_write(chip, runtime, pipe, size);
	err = vx_notify_end_of_buffer(chip, pipe);
	
	vx_send_rih_nolock(chip, IRQ_CONNECT_STREAM_NEXT);
	spin_unlock(&chip->lock);
	return err;
}

static int vx_update_pipe_position(struct vx_core *chip,
				   struct snd_pcm_runtime *runtime,
				   struct vx_pipe *pipe)
{
	struct vx_rmh rmh;
	int err, update;
	u64 count;

	vx_init_rmh(&rmh, CMD_STREAM_SAMPLE_COUNT);
	vx_set_pipe_cmd_params(&rmh, pipe->is_capture, pipe->number, 0);
	err = vx_send_msg(chip, &rmh);
	if (err < 0)
		return err;

	count = ((u64)(rmh.Stat[0] & 0xfffff) << 24) | (u64)rmh.Stat[1];
	update = (int)(count - pipe->cur_count);
	pipe->cur_count = count;
	pipe->position += update;
	if (pipe->position >= (int)runtime->buffer_size)
		pipe->position %= runtime->buffer_size;
	pipe->transferred += update;
	return 0;
}

static void vx_pcm_playback_transfer(struct vx_core *chip,
				     struct snd_pcm_substream *subs,
				     struct vx_pipe *pipe, int nchunks)
{
	int i, err;
	struct snd_pcm_runtime *runtime = subs->runtime;

	if (! pipe->prepared || (chip->chip_status & VX_STAT_IS_STALE))
		return;
	for (i = 0; i < nchunks; i++) {
		if ((err = vx_pcm_playback_transfer_chunk(chip, runtime, pipe,
							  chip->ibl.size)) < 0)
			return;
	}
}

static void vx_pcm_playback_update(struct vx_core *chip,
				   struct snd_pcm_substream *subs,
				   struct vx_pipe *pipe)
{
	int err;
	struct snd_pcm_runtime *runtime = subs->runtime;

	if (pipe->running && ! (chip->chip_status & VX_STAT_IS_STALE)) {
		if ((err = vx_update_pipe_position(chip, runtime, pipe)) < 0)
			return;
		if (pipe->transferred >= (int)runtime->period_size) {
			pipe->transferred %= runtime->period_size;
			snd_pcm_period_elapsed(subs);
		}
	}
}

static void vx_pcm_delayed_start(unsigned long arg)
{
	struct snd_pcm_substream *subs = (struct snd_pcm_substream *)arg;
	struct vx_core *chip = subs->pcm->private_data;
	struct vx_pipe *pipe = subs->runtime->private_data;
	int err;

	

	if ((err = vx_start_stream(chip, pipe)) < 0) {
		snd_printk(KERN_ERR "vx: cannot start stream\n");
		return;
	}
	if ((err = vx_toggle_pipe(chip, pipe, 1)) < 0) {
		snd_printk(KERN_ERR "vx: cannot start pipe\n");
		return;
	}
	
}

static int vx_pcm_trigger(struct snd_pcm_substream *subs, int cmd)
{
	struct vx_core *chip = snd_pcm_substream_chip(subs);
	struct vx_pipe *pipe = subs->runtime->private_data;
	int err;

	if (chip->chip_status & VX_STAT_IS_STALE)
		return -EBUSY;
		
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		if (! pipe->is_capture)
			vx_pcm_playback_transfer(chip, subs, pipe, 2);
 
		tasklet_schedule(&pipe->start_tq);
		chip->pcm_running++;
		pipe->running = 1;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		vx_toggle_pipe(chip, pipe, 0);
		vx_stop_pipe(chip, pipe);
		vx_stop_stream(chip, pipe);
		chip->pcm_running--;
		pipe->running = 0;
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if ((err = vx_toggle_pipe(chip, pipe, 0)) < 0)
			return err;
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if ((err = vx_toggle_pipe(chip, pipe, 1)) < 0)
			return err;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static snd_pcm_uframes_t vx_pcm_playback_pointer(struct snd_pcm_substream *subs)
{
	struct snd_pcm_runtime *runtime = subs->runtime;
	struct vx_pipe *pipe = runtime->private_data;
	return pipe->position;
}

static int vx_pcm_hw_params(struct snd_pcm_substream *subs,
				     struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_alloc_vmalloc_32_buffer
					(subs, params_buffer_bytes(hw_params));
}

static int vx_pcm_hw_free(struct snd_pcm_substream *subs)
{
	return snd_pcm_lib_free_vmalloc_buffer(subs);
}

static int vx_pcm_prepare(struct snd_pcm_substream *subs)
{
	struct vx_core *chip = snd_pcm_substream_chip(subs);
	struct snd_pcm_runtime *runtime = subs->runtime;
	struct vx_pipe *pipe = runtime->private_data;
	int err, data_mode;
	

	if (chip->chip_status & VX_STAT_IS_STALE)
		return -EBUSY;

	data_mode = (chip->uer_bits & IEC958_AES0_NONAUDIO) != 0;
	if (data_mode != pipe->data_mode && ! pipe->is_capture) {
		
		
		struct vx_rmh rmh;
		snd_printdd(KERN_DEBUG "reopen the pipe with data_mode = %d\n", data_mode);
		vx_init_rmh(&rmh, CMD_FREE_PIPE);
		vx_set_pipe_cmd_params(&rmh, 0, pipe->number, 0);
		if ((err = vx_send_msg(chip, &rmh)) < 0)
			return err;
		vx_init_rmh(&rmh, CMD_RES_PIPE);
		vx_set_pipe_cmd_params(&rmh, 0, pipe->number, pipe->channels);
		if (data_mode)
			rmh.Cmd[0] |= BIT_DATA_MODE;
		if ((err = vx_send_msg(chip, &rmh)) < 0)
			return err;
		pipe->data_mode = data_mode;
	}

	if (chip->pcm_running && chip->freq != runtime->rate) {
		snd_printk(KERN_ERR "vx: cannot set different clock %d "
			   "from the current %d\n", runtime->rate, chip->freq);
		return -EINVAL;
	}
	vx_set_clock(chip, runtime->rate);

	if ((err = vx_set_format(chip, pipe, runtime)) < 0)
		return err;

	if (vx_is_pcmcia(chip)) {
		pipe->align = 2; 
	} else {
		pipe->align = 4; 
	}

	pipe->buffer_bytes = frames_to_bytes(runtime, runtime->buffer_size);
	pipe->period_bytes = frames_to_bytes(runtime, runtime->period_size);
	pipe->hw_ptr = 0;

	
	vx_update_pipe_position(chip, runtime, pipe);
	
	pipe->transferred = 0;
	pipe->position = 0;

	pipe->prepared = 1;

	return 0;
}


static struct snd_pcm_ops vx_pcm_playback_ops = {
	.open =		vx_pcm_playback_open,
	.close =	vx_pcm_playback_close,
	.ioctl =	snd_pcm_lib_ioctl,
	.hw_params =	vx_pcm_hw_params,
	.hw_free =	vx_pcm_hw_free,
	.prepare =	vx_pcm_prepare,
	.trigger =	vx_pcm_trigger,
	.pointer =	vx_pcm_playback_pointer,
	.page =		snd_pcm_lib_get_vmalloc_page,
	.mmap =		snd_pcm_lib_mmap_vmalloc,
};



static struct snd_pcm_hardware vx_pcm_capture_hw = {
	.info =			(SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_MMAP_VALID 
				 ),
	.formats =		(
				 SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_3LE),
	.rates =		SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000,
	.rate_min =		5000,
	.rate_max =		48000,
	.channels_min =		1,
	.channels_max =		2,
	.buffer_bytes_max =	(128*1024),
	.period_bytes_min =	126,
	.period_bytes_max =	(128*1024),
	.periods_min =		2,
	.periods_max =		VX_MAX_PERIODS,
	.fifo_size =		126,
};


static int vx_pcm_capture_open(struct snd_pcm_substream *subs)
{
	struct snd_pcm_runtime *runtime = subs->runtime;
	struct vx_core *chip = snd_pcm_substream_chip(subs);
	struct vx_pipe *pipe;
	struct vx_pipe *pipe_out_monitoring = NULL;
	unsigned int audio;
	int err;

	if (chip->chip_status & VX_STAT_IS_STALE)
		return -EBUSY;

	audio = subs->pcm->device * 2;
	if (snd_BUG_ON(audio >= chip->audio_ins))
		return -EINVAL;
	err = vx_alloc_pipe(chip, 1, audio, 2, &pipe);
	if (err < 0)
		return err;
	pipe->substream = subs;
	tasklet_init(&pipe->start_tq, vx_pcm_delayed_start, (unsigned long)subs);
	chip->capture_pipes[audio] = pipe;

	
	if (chip->audio_monitor_active[audio]) {
		pipe_out_monitoring = chip->playback_pipes[audio];
		if (! pipe_out_monitoring) {
			
			err = vx_alloc_pipe(chip, 0, audio, 2, &pipe_out_monitoring);
			if (err < 0)
				return err;
			chip->playback_pipes[audio] = pipe_out_monitoring;
		}
		pipe_out_monitoring->references++;
		vx_set_monitor_level(chip, audio, chip->audio_monitor[audio],
				     chip->audio_monitor_active[audio]);
		
		vx_set_monitor_level(chip, audio+1, chip->audio_monitor[audio+1],
				     chip->audio_monitor_active[audio+1]); 
	}

	pipe->monitoring_pipe = pipe_out_monitoring; 

	runtime->hw = vx_pcm_capture_hw;
	runtime->hw.period_bytes_min = chip->ibl.size;
	runtime->private_data = pipe;

	 
	snd_pcm_hw_constraint_step(runtime, 0, SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 4);
	snd_pcm_hw_constraint_step(runtime, 0, SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 4);

	return 0;
}

static int vx_pcm_capture_close(struct snd_pcm_substream *subs)
{
	struct vx_core *chip = snd_pcm_substream_chip(subs);
	struct vx_pipe *pipe;
	struct vx_pipe *pipe_out_monitoring;
	
	if (! subs->runtime->private_data)
		return -EINVAL;
	pipe = subs->runtime->private_data;
	chip->capture_pipes[pipe->number] = NULL;

	pipe_out_monitoring = pipe->monitoring_pipe;

	if (pipe_out_monitoring) {
		if (--pipe_out_monitoring->references == 0) {
			vx_free_pipe(chip, pipe_out_monitoring);
			chip->playback_pipes[pipe->number] = NULL;
			pipe->monitoring_pipe = NULL;
		}
	}
	
	vx_free_pipe(chip, pipe);
	return 0;
}



#define DMA_READ_ALIGN	6	

static void vx_pcm_capture_update(struct vx_core *chip, struct snd_pcm_substream *subs,
				  struct vx_pipe *pipe)
{
	int size, space, count;
	struct snd_pcm_runtime *runtime = subs->runtime;

	if (! pipe->prepared || (chip->chip_status & VX_STAT_IS_STALE))
		return;

	size = runtime->buffer_size - snd_pcm_capture_avail(runtime);
	if (! size)
		return;
	size = frames_to_bytes(runtime, size);
	space = vx_query_hbuffer_size(chip, pipe);
	if (space < 0)
		goto _error;
	if (size > space)
		size = space;
	size = (size / 3) * 3; 
	if (size < DMA_READ_ALIGN)
		goto _error;

	
	count = size - DMA_READ_ALIGN;
	while (count > 0) {
		if ((pipe->hw_ptr % pipe->align) == 0)
			break;
		if (vx_wait_for_rx_full(chip) < 0)
			goto _error;
		vx_pcm_read_per_bytes(chip, runtime, pipe);
		count -= 3;
	}
	if (count > 0) {
		
		int align = pipe->align * 3;
		space = (count / align) * align;
		vx_pseudo_dma_read(chip, runtime, pipe, space);
		count -= space;
	}
	
	while (count > 0) {
		if (vx_wait_for_rx_full(chip) < 0)
			goto _error;
		vx_pcm_read_per_bytes(chip, runtime, pipe);
		count -= 3;
	}
	
	vx_send_rih_nolock(chip, IRQ_CONNECT_STREAM_NEXT);
	
	count = DMA_READ_ALIGN;
	while (count > 0) {
		vx_pcm_read_per_bytes(chip, runtime, pipe);
		count -= 3;
	}
	
	pipe->transferred += size;
	if (pipe->transferred >= pipe->period_bytes) {
		pipe->transferred %= pipe->period_bytes;
		snd_pcm_period_elapsed(subs);
	}
	return;

 _error:
	
	vx_send_rih_nolock(chip, IRQ_CONNECT_STREAM_NEXT);
	return;
}

static snd_pcm_uframes_t vx_pcm_capture_pointer(struct snd_pcm_substream *subs)
{
	struct snd_pcm_runtime *runtime = subs->runtime;
	struct vx_pipe *pipe = runtime->private_data;
	return bytes_to_frames(runtime, pipe->hw_ptr);
}

static struct snd_pcm_ops vx_pcm_capture_ops = {
	.open =		vx_pcm_capture_open,
	.close =	vx_pcm_capture_close,
	.ioctl =	snd_pcm_lib_ioctl,
	.hw_params =	vx_pcm_hw_params,
	.hw_free =	vx_pcm_hw_free,
	.prepare =	vx_pcm_prepare,
	.trigger =	vx_pcm_trigger,
	.pointer =	vx_pcm_capture_pointer,
	.page =		snd_pcm_lib_get_vmalloc_page,
	.mmap =		snd_pcm_lib_mmap_vmalloc,
};


void vx_pcm_update_intr(struct vx_core *chip, unsigned int events)
{
	unsigned int i;
	struct vx_pipe *pipe;

#define EVENT_MASK	(END_OF_BUFFER_EVENTS_PENDING|ASYNC_EVENTS_PENDING)

	if (events & EVENT_MASK) {
		vx_init_rmh(&chip->irq_rmh, CMD_ASYNC);
		if (events & ASYNC_EVENTS_PENDING)
			chip->irq_rmh.Cmd[0] |= 0x00000001;	
		if (events & END_OF_BUFFER_EVENTS_PENDING)
			chip->irq_rmh.Cmd[0] |= 0x00000002;	

		if (vx_send_msg(chip, &chip->irq_rmh) < 0) {
			snd_printdd(KERN_ERR "msg send error!!\n");
			return;
		}

		i = 1;
		while (i < chip->irq_rmh.LgStat) {
			int p, buf, capture, eob;
			p = chip->irq_rmh.Stat[i] & MASK_FIRST_FIELD;
			capture = (chip->irq_rmh.Stat[i] & 0x400000) ? 1 : 0;
			eob = (chip->irq_rmh.Stat[i] & 0x800000) ? 1 : 0;
			i++;
			if (events & ASYNC_EVENTS_PENDING)
				i++;
			buf = 1; 
			if (events & END_OF_BUFFER_EVENTS_PENDING) {
				if (eob)
					buf = chip->irq_rmh.Stat[i];
				i++;
			}
			if (capture)
				continue;
			if (snd_BUG_ON(p < 0 || p >= chip->audio_outs))
				continue;
			pipe = chip->playback_pipes[p];
			if (pipe && pipe->substream) {
				vx_pcm_playback_update(chip, pipe->substream, pipe);
				vx_pcm_playback_transfer(chip, pipe->substream, pipe, buf);
			}
		}
	}

	
	for (i = 0; i < chip->audio_ins; i++) {
		pipe = chip->capture_pipes[i];
		if (pipe && pipe->substream)
			vx_pcm_capture_update(chip, pipe->substream, pipe);
	}
}


static int vx_init_audio_io(struct vx_core *chip)
{
	struct vx_rmh rmh;
	int preferred;

	vx_init_rmh(&rmh, CMD_SUPPORTED);
	if (vx_send_msg(chip, &rmh) < 0) {
		snd_printk(KERN_ERR "vx: cannot get the supported audio data\n");
		return -ENXIO;
	}

	chip->audio_outs = rmh.Stat[0] & MASK_FIRST_FIELD;
	chip->audio_ins = (rmh.Stat[0] >> (FIELD_SIZE*2)) & MASK_FIRST_FIELD;
	chip->audio_info = rmh.Stat[1];

	
	chip->playback_pipes = kcalloc(chip->audio_outs, sizeof(struct vx_pipe *), GFP_KERNEL);
	if (!chip->playback_pipes)
		return -ENOMEM;
	chip->capture_pipes = kcalloc(chip->audio_ins, sizeof(struct vx_pipe *), GFP_KERNEL);
	if (!chip->capture_pipes) {
		kfree(chip->playback_pipes);
		return -ENOMEM;
	}

	preferred = chip->ibl.size;
	chip->ibl.size = 0;
	vx_set_ibl(chip, &chip->ibl); 
	if (preferred > 0) {
		chip->ibl.size = ((preferred + chip->ibl.granularity - 1) /
				  chip->ibl.granularity) * chip->ibl.granularity;
		if (chip->ibl.size > chip->ibl.max_size)
			chip->ibl.size = chip->ibl.max_size;
	} else
		chip->ibl.size = chip->ibl.min_size; 
	vx_set_ibl(chip, &chip->ibl);

	return 0;
}


static void snd_vx_pcm_free(struct snd_pcm *pcm)
{
	struct vx_core *chip = pcm->private_data;
	chip->pcm[pcm->device] = NULL;
	kfree(chip->playback_pipes);
	chip->playback_pipes = NULL;
	kfree(chip->capture_pipes);
	chip->capture_pipes = NULL;
}

int snd_vx_pcm_new(struct vx_core *chip)
{
	struct snd_pcm *pcm;
	unsigned int i;
	int err;

	if ((err = vx_init_audio_io(chip)) < 0)
		return err;

	for (i = 0; i < chip->hw->num_codecs; i++) {
		unsigned int outs, ins;
		outs = chip->audio_outs > i * 2 ? 1 : 0;
		ins = chip->audio_ins > i * 2 ? 1 : 0;
		if (! outs && ! ins)
			break;
		err = snd_pcm_new(chip->card, "VX PCM", i,
				  outs, ins, &pcm);
		if (err < 0)
			return err;
		if (outs)
			snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &vx_pcm_playback_ops);
		if (ins)
			snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &vx_pcm_capture_ops);

		pcm->private_data = chip;
		pcm->private_free = snd_vx_pcm_free;
		pcm->info_flags = 0;
		strcpy(pcm->name, chip->card->shortname);
		chip->pcm[i] = pcm;
	}

	return 0;
}
