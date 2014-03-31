/*
 * i2sbus driver -- pcm routines
 *
 * Copyright 2006 Johannes Berg <johannes@sipsolutions.net>
 *
 * GPL v2, can be found in COPYING.
 */

#include <asm/io.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <asm/macio.h>
#include <linux/pci.h>
#include <linux/module.h>
#include "../soundbus.h"
#include "i2sbus.h"

static inline void get_pcm_info(struct i2sbus_dev *i2sdev, int in,
				struct pcm_info **pi, struct pcm_info **other)
{
	if (in) {
		if (pi)
			*pi = &i2sdev->in;
		if (other)
			*other = &i2sdev->out;
	} else {
		if (pi)
			*pi = &i2sdev->out;
		if (other)
			*other = &i2sdev->in;
	}
}

static int clock_and_divisors(int mclk, int sclk, int rate, int *out)
{
	
	if (mclk % sclk)
		return -1;
	
	if (i2s_sf_sclkdiv(mclk / sclk, out))
		return -1;

	if (I2S_CLOCK_SPEED_18MHz % (rate * mclk) == 0) {
		if (!i2s_sf_mclkdiv(I2S_CLOCK_SPEED_18MHz / (rate * mclk), out)) {
			*out |= I2S_SF_CLOCK_SOURCE_18MHz;
			return 0;
		}
	}
	if (I2S_CLOCK_SPEED_45MHz % (rate * mclk) == 0) {
		if (!i2s_sf_mclkdiv(I2S_CLOCK_SPEED_45MHz / (rate * mclk), out)) {
			*out |= I2S_SF_CLOCK_SOURCE_45MHz;
			return 0;
		}
	}
	if (I2S_CLOCK_SPEED_49MHz % (rate * mclk) == 0) {
		if (!i2s_sf_mclkdiv(I2S_CLOCK_SPEED_49MHz / (rate * mclk), out)) {
			*out |= I2S_SF_CLOCK_SOURCE_49MHz;
			return 0;
		}
	}
	return -1;
}

#define CHECK_RATE(rate)						\
	do { if (rates & SNDRV_PCM_RATE_ ##rate) {			\
		int dummy;						\
		if (clock_and_divisors(sysclock_factor,			\
				       bus_factor, rate, &dummy))	\
			rates &= ~SNDRV_PCM_RATE_ ##rate;		\
	} } while (0)

static int i2sbus_pcm_open(struct i2sbus_dev *i2sdev, int in)
{
	struct pcm_info *pi, *other;
	struct soundbus_dev *sdev;
	int masks_inited = 0, err;
	struct codec_info_item *cii, *rev;
	struct snd_pcm_hardware *hw;
	u64 formats = 0;
	unsigned int rates = 0;
	struct transfer_info v;
	int result = 0;
	int bus_factor = 0, sysclock_factor = 0;
	int found_this;

	mutex_lock(&i2sdev->lock);

	get_pcm_info(i2sdev, in, &pi, &other);

	hw = &pi->substream->runtime->hw;
	sdev = &i2sdev->sound;

	if (pi->active) {
		
		result = -EBUSY;
		goto out_unlock;
	}

	
	list_for_each_entry(cii, &sdev->codec_list, list) {
		struct transfer_info *ti = cii->codec->transfers;
		bus_factor = cii->codec->bus_factor;
		sysclock_factor = cii->codec->sysclock_factor;
		while (ti->formats && ti->rates) {
			v = *ti;
			if (ti->transfer_in == in
			    && cii->codec->usable(cii, ti, &v)) {
				if (masks_inited) {
					formats &= v.formats;
					rates &= v.rates;
				} else {
					formats = v.formats;
					rates = v.rates;
					masks_inited = 1;
				}
			}
			ti++;
		}
	}
	if (!masks_inited || !bus_factor || !sysclock_factor) {
		result = -ENODEV;
		goto out_unlock;
	}
	
	hw->info = SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID |
		   SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_RESUME |
		   SNDRV_PCM_INFO_JOINT_DUPLEX;

	CHECK_RATE(5512);
	CHECK_RATE(8000);
	CHECK_RATE(11025);
	CHECK_RATE(16000);
	CHECK_RATE(22050);
	CHECK_RATE(32000);
	CHECK_RATE(44100);
	CHECK_RATE(48000);
	CHECK_RATE(64000);
	CHECK_RATE(88200);
	CHECK_RATE(96000);
	CHECK_RATE(176400);
	CHECK_RATE(192000);
	hw->rates = rates;

	if (formats & SNDRV_PCM_FMTBIT_S24_BE)
		formats |= SNDRV_PCM_FMTBIT_S32_BE;
	if (formats & SNDRV_PCM_FMTBIT_U24_BE)
		formats |= SNDRV_PCM_FMTBIT_U32_BE;
	hw->formats = formats & (SNDRV_PCM_FMTBIT_S16_BE |
				 SNDRV_PCM_FMTBIT_U16_BE |
				 SNDRV_PCM_FMTBIT_S32_BE |
				 SNDRV_PCM_FMTBIT_U32_BE);

	hw->rate_min = 5512;
	hw->rate_max = 192000;
	if (other->active) {
		
		hw->formats &= (1ULL << i2sdev->format);
		
		hw->rate_min = i2sdev->rate;
		hw->rate_max = i2sdev->rate;
	}

	hw->channels_min = 2;
	hw->channels_max = 2;
	
	hw->buffer_bytes_max = 131072;
	hw->period_bytes_min = 256;
	hw->period_bytes_max = 16384;
	hw->periods_min = 3;
	hw->periods_max = MAX_DBDMA_COMMANDS;
	err = snd_pcm_hw_constraint_integer(pi->substream->runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (err < 0) {
		result = err;
		goto out_unlock;
	}
	list_for_each_entry(cii, &sdev->codec_list, list) {
		if (cii->codec->open) {
			err = cii->codec->open(cii, pi->substream);
			if (err) {
				result = err;
				
				found_this = 0;
				list_for_each_entry_reverse(rev,
				    &sdev->codec_list, list) {
					if (found_this && rev->codec->close) {
						rev->codec->close(rev,
								pi->substream);
					}
					if (rev == cii)
						found_this = 1;
				}
				goto out_unlock;
			}
		}
	}

 out_unlock:
	mutex_unlock(&i2sdev->lock);
	return result;
}

#undef CHECK_RATE

static int i2sbus_pcm_close(struct i2sbus_dev *i2sdev, int in)
{
	struct codec_info_item *cii;
	struct pcm_info *pi;
	int err = 0, tmp;

	mutex_lock(&i2sdev->lock);

	get_pcm_info(i2sdev, in, &pi, NULL);

	list_for_each_entry(cii, &i2sdev->sound.codec_list, list) {
		if (cii->codec->close) {
			tmp = cii->codec->close(cii, pi->substream);
			if (tmp)
				err = tmp;
		}
	}

	pi->substream = NULL;
	pi->active = 0;
	mutex_unlock(&i2sdev->lock);
	return err;
}

static void i2sbus_wait_for_stop(struct i2sbus_dev *i2sdev,
				 struct pcm_info *pi)
{
	unsigned long flags;
	struct completion done;
	long timeout;

	spin_lock_irqsave(&i2sdev->low_lock, flags);
	if (pi->dbdma_ring.stopping) {
		init_completion(&done);
		pi->stop_completion = &done;
		spin_unlock_irqrestore(&i2sdev->low_lock, flags);
		timeout = wait_for_completion_timeout(&done, HZ);
		spin_lock_irqsave(&i2sdev->low_lock, flags);
		pi->stop_completion = NULL;
		if (timeout == 0) {
			
			printk(KERN_ERR "i2sbus_wait_for_stop: timed out\n");
			
			out_le32(&pi->dbdma->control, (RUN | PAUSE | 1) << 16);
			pi->dbdma_ring.stopping = 0;
			timeout = 10;
			while (in_le32(&pi->dbdma->status) & ACTIVE) {
				if (--timeout <= 0)
					break;
				udelay(1);
			}
		}
	}
	spin_unlock_irqrestore(&i2sdev->low_lock, flags);
}

#ifdef CONFIG_PM
void i2sbus_wait_for_stop_both(struct i2sbus_dev *i2sdev)
{
	struct pcm_info *pi;

	get_pcm_info(i2sdev, 0, &pi, NULL);
	i2sbus_wait_for_stop(i2sdev, pi);
	get_pcm_info(i2sdev, 1, &pi, NULL);
	i2sbus_wait_for_stop(i2sdev, pi);
}
#endif

static int i2sbus_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params)
{
	return snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(params));
}

static inline int i2sbus_hw_free(struct snd_pcm_substream *substream, int in)
{
	struct i2sbus_dev *i2sdev = snd_pcm_substream_chip(substream);
	struct pcm_info *pi;

	get_pcm_info(i2sdev, in, &pi, NULL);
	if (pi->dbdma_ring.stopping)
		i2sbus_wait_for_stop(i2sdev, pi);
	snd_pcm_lib_free_pages(substream);
	return 0;
}

static int i2sbus_playback_hw_free(struct snd_pcm_substream *substream)
{
	return i2sbus_hw_free(substream, 0);
}

static int i2sbus_record_hw_free(struct snd_pcm_substream *substream)
{
	return i2sbus_hw_free(substream, 1);
}

static int i2sbus_pcm_prepare(struct i2sbus_dev *i2sdev, int in)
{
	struct snd_pcm_runtime *runtime;
	struct dbdma_cmd *command;
	int i, periodsize, nperiods;
	dma_addr_t offset;
	struct bus_info bi;
	struct codec_info_item *cii;
	int sfr = 0;		
	int dws = 0;		
	int input_16bit;
	struct pcm_info *pi, *other;
	int cnt;
	int result = 0;
	unsigned int cmd, stopaddr;

	mutex_lock(&i2sdev->lock);

	get_pcm_info(i2sdev, in, &pi, &other);

	if (pi->dbdma_ring.running) {
		result = -EBUSY;
		goto out_unlock;
	}
	if (pi->dbdma_ring.stopping)
		i2sbus_wait_for_stop(i2sdev, pi);

	if (!pi->substream || !pi->substream->runtime) {
		result = -EINVAL;
		goto out_unlock;
	}

	runtime = pi->substream->runtime;
	pi->active = 1;
	if (other->active &&
	    ((i2sdev->format != runtime->format)
	     || (i2sdev->rate != runtime->rate))) {
		result = -EINVAL;
		goto out_unlock;
	}

	i2sdev->format = runtime->format;
	i2sdev->rate = runtime->rate;

	periodsize = snd_pcm_lib_period_bytes(pi->substream);
	nperiods = pi->substream->runtime->periods;
	pi->current_period = 0;

	
	command = pi->dbdma_ring.cmds;
	memset(command, 0, (nperiods + 2) * sizeof(struct dbdma_cmd));

	
	offset = runtime->dma_addr;
	cmd = (in? INPUT_MORE: OUTPUT_MORE) | BR_IFSET | INTR_ALWAYS;
	stopaddr = pi->dbdma_ring.bus_cmd_start +
		(nperiods + 1) * sizeof(struct dbdma_cmd);
	for (i = 0; i < nperiods; i++, command++, offset += periodsize) {
		command->command = cpu_to_le16(cmd);
		command->cmd_dep = cpu_to_le32(stopaddr);
		command->phy_addr = cpu_to_le32(offset);
		command->req_count = cpu_to_le16(periodsize);
	}

	
	command->command = cpu_to_le16(DBDMA_NOP | BR_ALWAYS);
	command->cmd_dep = cpu_to_le32(pi->dbdma_ring.bus_cmd_start);
	command++;

	
	command->command = cpu_to_le16(DBDMA_STOP);

	
	switch (runtime->format) {
	
	case SNDRV_PCM_FORMAT_S16_BE:
	case SNDRV_PCM_FORMAT_U16_BE:
		bi.bus_factor = 0;
		list_for_each_entry(cii, &i2sdev->sound.codec_list, list) {
			bi.bus_factor = cii->codec->bus_factor;
			break;
		}
		if (!bi.bus_factor) {
			result = -ENODEV;
			goto out_unlock;
		}
		input_16bit = 1;
		break;
	case SNDRV_PCM_FORMAT_S32_BE:
	case SNDRV_PCM_FORMAT_U32_BE:
		bi.bus_factor = 64;
		input_16bit = 0;
		break;
	default:
		result = -EINVAL;
		goto out_unlock;
	}
	
	list_for_each_entry(cii, &i2sdev->sound.codec_list, list) {
		bi.sysclock_factor = cii->codec->sysclock_factor;
		break;
	}

	if (clock_and_divisors(bi.sysclock_factor,
			       bi.bus_factor,
			       runtime->rate,
			       &sfr) < 0) {
		result = -EINVAL;
		goto out_unlock;
	}
	switch (bi.bus_factor) {
	case 32:
		sfr |= I2S_SF_SERIAL_FORMAT_I2S_32X;
		break;
	case 64:
		sfr |= I2S_SF_SERIAL_FORMAT_I2S_64X;
		break;
	}
	
	sfr |= I2S_SF_SCLK_MASTER;

	list_for_each_entry(cii, &i2sdev->sound.codec_list, list) {
		int err = 0;
		if (cii->codec->prepare)
			err = cii->codec->prepare(cii, &bi, pi->substream);
		if (err) {
			result = err;
			goto out_unlock;
		}
	}
	
	if (input_16bit)
		dws =	(2 << I2S_DWS_NUM_CHANNELS_IN_SHIFT) |
			(2 << I2S_DWS_NUM_CHANNELS_OUT_SHIFT) |
			I2S_DWS_DATA_IN_16BIT | I2S_DWS_DATA_OUT_16BIT;
	else
		dws =	(2 << I2S_DWS_NUM_CHANNELS_IN_SHIFT) |
			(2 << I2S_DWS_NUM_CHANNELS_OUT_SHIFT) |
			I2S_DWS_DATA_IN_24BIT | I2S_DWS_DATA_OUT_24BIT;

	
	
	if (in_le32(&i2sdev->intfregs->serial_format) == sfr
	 && in_le32(&i2sdev->intfregs->data_word_sizes) == dws)
		goto out_unlock;

	list_for_each_entry(cii, &i2sdev->sound.codec_list, list)
		if (cii->codec->switch_clock)
			cii->codec->switch_clock(cii, CLOCK_SWITCH_PREPARE_SLAVE);

	i2sbus_control_enable(i2sdev->control, i2sdev);
	i2sbus_control_cell(i2sdev->control, i2sdev, 1);

	out_le32(&i2sdev->intfregs->intr_ctl, I2S_PENDING_CLOCKS_STOPPED);

	i2sbus_control_clock(i2sdev->control, i2sdev, 0);

	msleep(1);

	
	cnt = 100;
	while (cnt-- &&
	    !(in_le32(&i2sdev->intfregs->intr_ctl) & I2S_PENDING_CLOCKS_STOPPED)) {
		msleep(5);
	}
	out_le32(&i2sdev->intfregs->intr_ctl, I2S_PENDING_CLOCKS_STOPPED);

	
	out_le32(&i2sdev->intfregs->serial_format, sfr);
	out_le32(&i2sdev->intfregs->data_word_sizes, dws);

        i2sbus_control_enable(i2sdev->control, i2sdev);
        i2sbus_control_cell(i2sdev->control, i2sdev, 1);
        i2sbus_control_clock(i2sdev->control, i2sdev, 1);
	msleep(1);

	list_for_each_entry(cii, &i2sdev->sound.codec_list, list)
		if (cii->codec->switch_clock)
			cii->codec->switch_clock(cii, CLOCK_SWITCH_SLAVE);

 out_unlock:
	mutex_unlock(&i2sdev->lock);
	return result;
}

#ifdef CONFIG_PM
void i2sbus_pcm_prepare_both(struct i2sbus_dev *i2sdev)
{
	i2sbus_pcm_prepare(i2sdev, 0);
	i2sbus_pcm_prepare(i2sdev, 1);
}
#endif

static int i2sbus_pcm_trigger(struct i2sbus_dev *i2sdev, int in, int cmd)
{
	struct codec_info_item *cii;
	struct pcm_info *pi;
	int result = 0;
	unsigned long flags;

	spin_lock_irqsave(&i2sdev->low_lock, flags);

	get_pcm_info(i2sdev, in, &pi, NULL);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		if (pi->dbdma_ring.running) {
			result = -EALREADY;
			goto out_unlock;
		}
		list_for_each_entry(cii, &i2sdev->sound.codec_list, list)
			if (cii->codec->start)
				cii->codec->start(cii, pi->substream);
		pi->dbdma_ring.running = 1;

		if (pi->dbdma_ring.stopping) {
			
			out_le32(&pi->dbdma->control, 1 << 16);
			if (in_le32(&pi->dbdma->status) & ACTIVE) {
				
				udelay(10);
				if (in_le32(&pi->dbdma->status) & ACTIVE) {
					pi->dbdma_ring.stopping = 0;
					goto out_unlock; 
				}
			}
		}

		
		out_le32(&pi->dbdma->control, (RUN | PAUSE | 1) << 16);

		
		out_le32(&pi->dbdma->br_sel, (1 << 16) | 1);

		
		out_le32(&pi->dbdma->cmdptr, pi->dbdma_ring.bus_cmd_start);

		
		pi->current_period = 0;
		pi->frame_count = in_le32(&i2sdev->intfregs->frame_count);

		
		out_le32(&pi->dbdma->control, (RUN << 16) | RUN);

		
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (!pi->dbdma_ring.running) {
			result = -EALREADY;
			goto out_unlock;
		}
		pi->dbdma_ring.running = 0;

		
		out_le32(&pi->dbdma->control, (1 << 16) | 1);
		pi->dbdma_ring.stopping = 1;

		list_for_each_entry(cii, &i2sdev->sound.codec_list, list)
			if (cii->codec->stop)
				cii->codec->stop(cii, pi->substream);
		break;
	default:
		result = -EINVAL;
		goto out_unlock;
	}

 out_unlock:
	spin_unlock_irqrestore(&i2sdev->low_lock, flags);
	return result;
}

static snd_pcm_uframes_t i2sbus_pcm_pointer(struct i2sbus_dev *i2sdev, int in)
{
	struct pcm_info *pi;
	u32 fc;

	get_pcm_info(i2sdev, in, &pi, NULL);

	fc = in_le32(&i2sdev->intfregs->frame_count);
	fc = fc - pi->frame_count;

	if (fc >= pi->substream->runtime->buffer_size)
		fc %= pi->substream->runtime->buffer_size;
	return fc;
}

static inline void handle_interrupt(struct i2sbus_dev *i2sdev, int in)
{
	struct pcm_info *pi;
	u32 fc, nframes;
	u32 status;
	int timeout, i;
	int dma_stopped = 0;
	struct snd_pcm_runtime *runtime;

	spin_lock(&i2sdev->low_lock);
	get_pcm_info(i2sdev, in, &pi, NULL);
	if (!pi->dbdma_ring.running && !pi->dbdma_ring.stopping)
		goto out_unlock;

	i = pi->current_period;
	runtime = pi->substream->runtime;
	while (pi->dbdma_ring.cmds[i].xfer_status) {
		if (le16_to_cpu(pi->dbdma_ring.cmds[i].xfer_status) & BT)
			dma_stopped = 1;
		pi->dbdma_ring.cmds[i].xfer_status = 0;

		if (++i >= runtime->periods) {
			i = 0;
			pi->frame_count += runtime->buffer_size;
		}
		pi->current_period = i;

		fc = in_le32(&i2sdev->intfregs->frame_count);
		nframes = i * runtime->period_size;
		if (fc < pi->frame_count + nframes)
			pi->frame_count = fc - nframes;
	}

	if (dma_stopped) {
		timeout = 1000;
		for (;;) {
			status = in_le32(&pi->dbdma->status);
			if (!(status & ACTIVE) && (!in || (status & 0x80)))
				break;
			if (--timeout <= 0) {
				printk(KERN_ERR "i2sbus: timed out "
				       "waiting for DMA to stop!\n");
				break;
			}
			udelay(1);
		}

		
		out_le32(&pi->dbdma->control, (RUN | PAUSE | 1) << 16);

		pi->dbdma_ring.stopping = 0;
		if (pi->stop_completion)
			complete(pi->stop_completion);
	}

	if (!pi->dbdma_ring.running)
		goto out_unlock;
	spin_unlock(&i2sdev->low_lock);
	
	snd_pcm_period_elapsed(pi->substream);
	return;

 out_unlock:
	spin_unlock(&i2sdev->low_lock);
}

irqreturn_t i2sbus_tx_intr(int irq, void *devid)
{
	handle_interrupt((struct i2sbus_dev *)devid, 0);
	return IRQ_HANDLED;
}

irqreturn_t i2sbus_rx_intr(int irq, void *devid)
{
	handle_interrupt((struct i2sbus_dev *)devid, 1);
	return IRQ_HANDLED;
}

static int i2sbus_playback_open(struct snd_pcm_substream *substream)
{
	struct i2sbus_dev *i2sdev = snd_pcm_substream_chip(substream);

	if (!i2sdev)
		return -EINVAL;
	i2sdev->out.substream = substream;
	return i2sbus_pcm_open(i2sdev, 0);
}

static int i2sbus_playback_close(struct snd_pcm_substream *substream)
{
	struct i2sbus_dev *i2sdev = snd_pcm_substream_chip(substream);
	int err;

	if (!i2sdev)
		return -EINVAL;
	if (i2sdev->out.substream != substream)
		return -EINVAL;
	err = i2sbus_pcm_close(i2sdev, 0);
	if (!err)
		i2sdev->out.substream = NULL;
	return err;
}

static int i2sbus_playback_prepare(struct snd_pcm_substream *substream)
{
	struct i2sbus_dev *i2sdev = snd_pcm_substream_chip(substream);

	if (!i2sdev)
		return -EINVAL;
	if (i2sdev->out.substream != substream)
		return -EINVAL;
	return i2sbus_pcm_prepare(i2sdev, 0);
}

static int i2sbus_playback_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct i2sbus_dev *i2sdev = snd_pcm_substream_chip(substream);

	if (!i2sdev)
		return -EINVAL;
	if (i2sdev->out.substream != substream)
		return -EINVAL;
	return i2sbus_pcm_trigger(i2sdev, 0, cmd);
}

static snd_pcm_uframes_t i2sbus_playback_pointer(struct snd_pcm_substream
						 *substream)
{
	struct i2sbus_dev *i2sdev = snd_pcm_substream_chip(substream);

	if (!i2sdev)
		return -EINVAL;
	if (i2sdev->out.substream != substream)
		return 0;
	return i2sbus_pcm_pointer(i2sdev, 0);
}

static struct snd_pcm_ops i2sbus_playback_ops = {
	.open =		i2sbus_playback_open,
	.close =	i2sbus_playback_close,
	.ioctl =	snd_pcm_lib_ioctl,
	.hw_params =	i2sbus_hw_params,
	.hw_free =	i2sbus_playback_hw_free,
	.prepare =	i2sbus_playback_prepare,
	.trigger =	i2sbus_playback_trigger,
	.pointer =	i2sbus_playback_pointer,
};

static int i2sbus_record_open(struct snd_pcm_substream *substream)
{
	struct i2sbus_dev *i2sdev = snd_pcm_substream_chip(substream);

	if (!i2sdev)
		return -EINVAL;
	i2sdev->in.substream = substream;
	return i2sbus_pcm_open(i2sdev, 1);
}

static int i2sbus_record_close(struct snd_pcm_substream *substream)
{
	struct i2sbus_dev *i2sdev = snd_pcm_substream_chip(substream);
	int err;

	if (!i2sdev)
		return -EINVAL;
	if (i2sdev->in.substream != substream)
		return -EINVAL;
	err = i2sbus_pcm_close(i2sdev, 1);
	if (!err)
		i2sdev->in.substream = NULL;
	return err;
}

static int i2sbus_record_prepare(struct snd_pcm_substream *substream)
{
	struct i2sbus_dev *i2sdev = snd_pcm_substream_chip(substream);

	if (!i2sdev)
		return -EINVAL;
	if (i2sdev->in.substream != substream)
		return -EINVAL;
	return i2sbus_pcm_prepare(i2sdev, 1);
}

static int i2sbus_record_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct i2sbus_dev *i2sdev = snd_pcm_substream_chip(substream);

	if (!i2sdev)
		return -EINVAL;
	if (i2sdev->in.substream != substream)
		return -EINVAL;
	return i2sbus_pcm_trigger(i2sdev, 1, cmd);
}

static snd_pcm_uframes_t i2sbus_record_pointer(struct snd_pcm_substream
					       *substream)
{
	struct i2sbus_dev *i2sdev = snd_pcm_substream_chip(substream);

	if (!i2sdev)
		return -EINVAL;
	if (i2sdev->in.substream != substream)
		return 0;
	return i2sbus_pcm_pointer(i2sdev, 1);
}

static struct snd_pcm_ops i2sbus_record_ops = {
	.open =		i2sbus_record_open,
	.close =	i2sbus_record_close,
	.ioctl =	snd_pcm_lib_ioctl,
	.hw_params =	i2sbus_hw_params,
	.hw_free =	i2sbus_record_hw_free,
	.prepare =	i2sbus_record_prepare,
	.trigger =	i2sbus_record_trigger,
	.pointer =	i2sbus_record_pointer,
};

static void i2sbus_private_free(struct snd_pcm *pcm)
{
	struct i2sbus_dev *i2sdev = snd_pcm_chip(pcm);
	struct codec_info_item *p, *tmp;

	i2sdev->sound.pcm = NULL;
	i2sdev->out.created = 0;
	i2sdev->in.created = 0;
	list_for_each_entry_safe(p, tmp, &i2sdev->sound.codec_list, list) {
		printk(KERN_ERR "i2sbus: a codec didn't unregister!\n");
		list_del(&p->list);
		module_put(p->codec->owner);
		kfree(p);
	}
	soundbus_dev_put(&i2sdev->sound);
	module_put(THIS_MODULE);
}

int
i2sbus_attach_codec(struct soundbus_dev *dev, struct snd_card *card,
		    struct codec_info *ci, void *data)
{
	int err, in = 0, out = 0;
	struct transfer_info *tmp;
	struct i2sbus_dev *i2sdev = soundbus_dev_to_i2sbus_dev(dev);
	struct codec_info_item *cii;

	if (!dev->pcmname || dev->pcmid == -1) {
		printk(KERN_ERR "i2sbus: pcm name and id must be set!\n");
		return -EINVAL;
	}

	list_for_each_entry(cii, &dev->codec_list, list) {
		if (cii->codec_data == data)
			return -EALREADY;
	}

	if (!ci->transfers || !ci->transfers->formats
	    || !ci->transfers->rates || !ci->usable)
		return -EINVAL;

	if (ci->bus_factor != 32 && ci->bus_factor != 64)
		return -EINVAL;

	list_for_each_entry(cii, &dev->codec_list, list) {
		if (cii->codec->sysclock_factor != ci->sysclock_factor) {
			printk(KERN_DEBUG
			       "cannot yet handle multiple different sysclocks!\n");
			return -EINVAL;
		}
		if (cii->codec->bus_factor != ci->bus_factor) {
			printk(KERN_DEBUG
			       "cannot yet handle multiple different bus clocks!\n");
			return -EINVAL;
		}
	}

	tmp = ci->transfers;
	while (tmp->formats && tmp->rates) {
		if (tmp->transfer_in)
			in = 1;
		else
			out = 1;
		tmp++;
	}

	cii = kzalloc(sizeof(struct codec_info_item), GFP_KERNEL);
	if (!cii) {
		printk(KERN_DEBUG "i2sbus: failed to allocate cii\n");
		return -ENOMEM;
	}

	
	cii->sdev = soundbus_dev_get(dev);
	cii->codec = ci;
	cii->codec_data = data;

	if (!cii->sdev) {
		printk(KERN_DEBUG
		       "i2sbus: failed to get soundbus dev reference\n");
		err = -ENODEV;
		goto out_free_cii;
	}

	if (!try_module_get(THIS_MODULE)) {
		printk(KERN_DEBUG "i2sbus: failed to get module reference!\n");
		err = -EBUSY;
		goto out_put_sdev;
	}

	if (!try_module_get(ci->owner)) {
		printk(KERN_DEBUG
		       "i2sbus: failed to get module reference to codec owner!\n");
		err = -EBUSY;
		goto out_put_this_module;
	}

	if (!dev->pcm) {
		err = snd_pcm_new(card, dev->pcmname, dev->pcmid, 0, 0,
				  &dev->pcm);
		if (err) {
			printk(KERN_DEBUG "i2sbus: failed to create pcm\n");
			goto out_put_ci_module;
		}
		dev->pcm->dev = &dev->ofdev.dev;
	}

	out = in = 1;

	if (!i2sdev->out.created && out) {
		if (dev->pcm->card != card) {
			
			printk(KERN_ERR
			       "Can't attach same bus to different cards!\n");
			err = -EINVAL;
			goto out_put_ci_module;
		}
		err = snd_pcm_new_stream(dev->pcm, SNDRV_PCM_STREAM_PLAYBACK, 1);
		if (err)
			goto out_put_ci_module;
		snd_pcm_set_ops(dev->pcm, SNDRV_PCM_STREAM_PLAYBACK,
				&i2sbus_playback_ops);
		i2sdev->out.created = 1;
	}

	if (!i2sdev->in.created && in) {
		if (dev->pcm->card != card) {
			printk(KERN_ERR
			       "Can't attach same bus to different cards!\n");
			err = -EINVAL;
			goto out_put_ci_module;
		}
		err = snd_pcm_new_stream(dev->pcm, SNDRV_PCM_STREAM_CAPTURE, 1);
		if (err)
			goto out_put_ci_module;
		snd_pcm_set_ops(dev->pcm, SNDRV_PCM_STREAM_CAPTURE,
				&i2sbus_record_ops);
		i2sdev->in.created = 1;
	}

	err = snd_device_register(card, dev->pcm);
	if (err) {
		printk(KERN_ERR "i2sbus: error registering new pcm\n");
		goto out_put_ci_module;
	}
	
	list_add(&cii->list, &dev->codec_list);

	dev->pcm->private_data = i2sdev;
	dev->pcm->private_free = i2sbus_private_free;

	
	snd_pcm_lib_preallocate_pages_for_all(
		dev->pcm, SNDRV_DMA_TYPE_DEV,
		snd_dma_pci_data(macio_get_pci_dev(i2sdev->macio)),
		64 * 1024, 64 * 1024);

	return 0;
 out_put_ci_module:
	module_put(ci->owner);
 out_put_this_module:
	module_put(THIS_MODULE);
 out_put_sdev:
	soundbus_dev_put(dev);
 out_free_cii:
	kfree(cii);
	return err;
}

void i2sbus_detach_codec(struct soundbus_dev *dev, void *data)
{
	struct codec_info_item *cii = NULL, *i;

	list_for_each_entry(i, &dev->codec_list, list) {
		if (i->codec_data == data) {
			cii = i;
			break;
		}
	}
	if (cii) {
		list_del(&cii->list);
		module_put(cii->codec->owner);
		kfree(cii);
	}
	
	if (list_empty(&dev->codec_list) && dev->pcm) {
		
		snd_device_free(dev->pcm->card, dev->pcm);
	}
}
