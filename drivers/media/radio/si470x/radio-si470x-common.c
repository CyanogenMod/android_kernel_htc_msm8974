/*
 *  drivers/media/radio/si470x/radio-si470x-common.c
 *
 *  Driver for radios with Silicon Labs Si470x FM Radio Receivers
 *
 *  Copyright (c) 2009 Tobias Lorenz <tobias.lorenz@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */




#include "radio-si470x.h"




static unsigned short space = 2;
module_param(space, ushort, 0444);
MODULE_PARM_DESC(space, "Spacing: 0=200kHz 1=100kHz *2=50kHz*");

static unsigned short band = 1;
module_param(band, ushort, 0444);
MODULE_PARM_DESC(band, "Band: 0=87.5..108MHz *1=76..108MHz* 2=76..90MHz");

static unsigned short de = 1;
module_param(de, ushort, 0444);
MODULE_PARM_DESC(de, "De-emphasis: 0=75us *1=50us*");

static unsigned int tune_timeout = 3000;
module_param(tune_timeout, uint, 0644);
MODULE_PARM_DESC(tune_timeout, "Tune timeout: *3000*");

static unsigned int seek_timeout = 5000;
module_param(seek_timeout, uint, 0644);
MODULE_PARM_DESC(seek_timeout, "Seek timeout: *5000*");




static int si470x_set_chan(struct si470x_device *radio, unsigned short chan)
{
	int retval;
	unsigned long timeout;
	bool timed_out = 0;

	
	radio->registers[CHANNEL] &= ~CHANNEL_CHAN;
	radio->registers[CHANNEL] |= CHANNEL_TUNE | chan;
	retval = si470x_set_register(radio, CHANNEL);
	if (retval < 0)
		goto done;

	
	if (radio->stci_enabled) {
		INIT_COMPLETION(radio->completion);

		
		retval = wait_for_completion_timeout(&radio->completion,
				msecs_to_jiffies(tune_timeout));
		if (!retval)
			timed_out = true;
	} else {
		
		timeout = jiffies + msecs_to_jiffies(tune_timeout);
		do {
			retval = si470x_get_register(radio, STATUSRSSI);
			if (retval < 0)
				goto stop;
			timed_out = time_after(jiffies, timeout);
		} while (((radio->registers[STATUSRSSI] & STATUSRSSI_STC) == 0)
				&& (!timed_out));
	}

	if ((radio->registers[STATUSRSSI] & STATUSRSSI_STC) == 0)
		dev_warn(&radio->videodev->dev, "tune does not complete\n");
	if (timed_out)
		dev_warn(&radio->videodev->dev,
			"tune timed out after %u ms\n", tune_timeout);

stop:
	
	radio->registers[CHANNEL] &= ~CHANNEL_TUNE;
	retval = si470x_set_register(radio, CHANNEL);

done:
	return retval;
}


static int si470x_get_freq(struct si470x_device *radio, unsigned int *freq)
{
	unsigned int spacing, band_bottom;
	unsigned short chan;
	int retval;

	
	switch ((radio->registers[SYSCONFIG2] & SYSCONFIG2_SPACE) >> 4) {
	
	case 0:
		spacing = 0.200 * FREQ_MUL; break;
	
	case 1:
		spacing = 0.100 * FREQ_MUL; break;
	
	default:
		spacing = 0.050 * FREQ_MUL; break;
	};

	
	switch ((radio->registers[SYSCONFIG2] & SYSCONFIG2_BAND) >> 6) {
	
	case 0:
		band_bottom = 87.5 * FREQ_MUL; break;
	
	default:
		band_bottom = 76   * FREQ_MUL; break;
	
	case 2:
		band_bottom = 76   * FREQ_MUL; break;
	};

	
	retval = si470x_get_register(radio, READCHAN);
	chan = radio->registers[READCHAN] & READCHAN_READCHAN;

	
	*freq = chan * spacing + band_bottom;

	return retval;
}


int si470x_set_freq(struct si470x_device *radio, unsigned int freq)
{
	unsigned int spacing, band_bottom;
	unsigned short chan;

	
	switch ((radio->registers[SYSCONFIG2] & SYSCONFIG2_SPACE) >> 4) {
	
	case 0:
		spacing = 0.200 * FREQ_MUL; break;
	
	case 1:
		spacing = 0.100 * FREQ_MUL; break;
	
	default:
		spacing = 0.050 * FREQ_MUL; break;
	};

	
	switch ((radio->registers[SYSCONFIG2] & SYSCONFIG2_BAND) >> 6) {
	
	case 0:
		band_bottom = 87.5 * FREQ_MUL; break;
	
	default:
		band_bottom = 76   * FREQ_MUL; break;
	
	case 2:
		band_bottom = 76   * FREQ_MUL; break;
	};

	
	chan = (freq - band_bottom) / spacing;

	return si470x_set_chan(radio, chan);
}


static int si470x_set_seek(struct si470x_device *radio,
		unsigned int wrap_around, unsigned int seek_upward)
{
	int retval = 0;
	unsigned long timeout;
	bool timed_out = 0;

	
	radio->registers[POWERCFG] |= POWERCFG_SEEK;
	if (wrap_around == 1)
		radio->registers[POWERCFG] &= ~POWERCFG_SKMODE;
	else
		radio->registers[POWERCFG] |= POWERCFG_SKMODE;
	if (seek_upward == 1)
		radio->registers[POWERCFG] |= POWERCFG_SEEKUP;
	else
		radio->registers[POWERCFG] &= ~POWERCFG_SEEKUP;
	retval = si470x_set_register(radio, POWERCFG);
	if (retval < 0)
		goto done;

	
	if (radio->stci_enabled) {
		INIT_COMPLETION(radio->completion);

		
		retval = wait_for_completion_timeout(&radio->completion,
				msecs_to_jiffies(seek_timeout));
		if (!retval)
			timed_out = true;
	} else {
		
		timeout = jiffies + msecs_to_jiffies(seek_timeout);
		do {
			retval = si470x_get_register(radio, STATUSRSSI);
			if (retval < 0)
				goto stop;
			timed_out = time_after(jiffies, timeout);
		} while (((radio->registers[STATUSRSSI] & STATUSRSSI_STC) == 0)
				&& (!timed_out));
	}

	if ((radio->registers[STATUSRSSI] & STATUSRSSI_STC) == 0)
		dev_warn(&radio->videodev->dev, "seek does not complete\n");
	if (radio->registers[STATUSRSSI] & STATUSRSSI_SF)
		dev_warn(&radio->videodev->dev,
			"seek failed / band limit reached\n");
	if (timed_out)
		dev_warn(&radio->videodev->dev,
			"seek timed out after %u ms\n", seek_timeout);

stop:
	
	radio->registers[POWERCFG] &= ~POWERCFG_SEEK;
	retval = si470x_set_register(radio, POWERCFG);

done:
	
	if ((retval == 0) && timed_out)
		retval = -EAGAIN;

	return retval;
}


int si470x_start(struct si470x_device *radio)
{
	int retval;

	
	radio->registers[POWERCFG] =
		POWERCFG_DMUTE | POWERCFG_ENABLE | POWERCFG_RDSM;
	retval = si470x_set_register(radio, POWERCFG);
	if (retval < 0)
		goto done;

	
	radio->registers[SYSCONFIG1] =
		(de << 11) & SYSCONFIG1_DE;		
	retval = si470x_set_register(radio, SYSCONFIG1);
	if (retval < 0)
		goto done;

	
	radio->registers[SYSCONFIG2] =
		(0x3f  << 8) |				
		((band  << 6) & SYSCONFIG2_BAND)  |	
		((space << 4) & SYSCONFIG2_SPACE) |	
		15;					
	retval = si470x_set_register(radio, SYSCONFIG2);
	if (retval < 0)
		goto done;

	
	retval = si470x_set_chan(radio,
		radio->registers[CHANNEL] & CHANNEL_CHAN);

done:
	return retval;
}


int si470x_stop(struct si470x_device *radio)
{
	int retval;

	
	radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_RDS;
	retval = si470x_set_register(radio, SYSCONFIG1);
	if (retval < 0)
		goto done;

	
	radio->registers[POWERCFG] &= ~POWERCFG_DMUTE;
	
	radio->registers[POWERCFG] |= POWERCFG_ENABLE |	POWERCFG_DISABLE;
	retval = si470x_set_register(radio, POWERCFG);

done:
	return retval;
}


static int si470x_rds_on(struct si470x_device *radio)
{
	int retval;

	
	radio->registers[SYSCONFIG1] |= SYSCONFIG1_RDS;
	retval = si470x_set_register(radio, SYSCONFIG1);
	if (retval < 0)
		radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_RDS;

	return retval;
}




static ssize_t si470x_fops_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;
	unsigned int block_count = 0;

	
	mutex_lock(&radio->lock);
	if ((radio->registers[SYSCONFIG1] & SYSCONFIG1_RDS) == 0)
		si470x_rds_on(radio);

	
	while (radio->wr_index == radio->rd_index) {
		if (file->f_flags & O_NONBLOCK) {
			retval = -EWOULDBLOCK;
			goto done;
		}
		if (wait_event_interruptible(radio->read_queue,
			radio->wr_index != radio->rd_index) < 0) {
			retval = -EINTR;
			goto done;
		}
	}

	
	count /= 3;

	
	while (block_count < count) {
		if (radio->rd_index == radio->wr_index)
			break;

		
		if (copy_to_user(buf, &radio->buffer[radio->rd_index], 3))
			
			break;

		
		radio->rd_index += 3;
		if (radio->rd_index >= radio->buf_size)
			radio->rd_index = 0;

		
		block_count++;
		buf += 3;
		retval += 3;
	}

done:
	mutex_unlock(&radio->lock);
	return retval;
}


static unsigned int si470x_fops_poll(struct file *file,
		struct poll_table_struct *pts)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	

	mutex_lock(&radio->lock);
	if ((radio->registers[SYSCONFIG1] & SYSCONFIG1_RDS) == 0)
		si470x_rds_on(radio);
	mutex_unlock(&radio->lock);

	poll_wait(file, &radio->read_queue, pts);

	if (radio->rd_index != radio->wr_index)
		retval = POLLIN | POLLRDNORM;

	return retval;
}


static const struct v4l2_file_operations si470x_fops = {
	.owner			= THIS_MODULE,
	.read			= si470x_fops_read,
	.poll			= si470x_fops_poll,
	.unlocked_ioctl		= video_ioctl2,
	.open			= si470x_fops_open,
	.release		= si470x_fops_release,
};




static int si470x_vidioc_queryctrl(struct file *file, void *priv,
		struct v4l2_queryctrl *qc)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = -EINVAL;

	
	if (qc->id < V4L2_CID_BASE)
		goto done;

	
	switch (qc->id) {
	case V4L2_CID_AUDIO_VOLUME:
		return v4l2_ctrl_query_fill(qc, 0, 15, 1, 15);
	case V4L2_CID_AUDIO_MUTE:
		return v4l2_ctrl_query_fill(qc, 0, 1, 1, 1);
	}

	
	
	if ((retval == -EINVAL) && (qc->id < V4L2_CID_LASTP1)) {
		qc->flags = V4L2_CTRL_FLAG_DISABLED;
		retval = 0;
	}

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"query controls failed with %d\n", retval);
	return retval;
}


static int si470x_vidioc_g_ctrl(struct file *file, void *priv,
		struct v4l2_control *ctrl)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	mutex_lock(&radio->lock);
	
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_VOLUME:
		ctrl->value = radio->registers[SYSCONFIG2] &
				SYSCONFIG2_VOLUME;
		break;
	case V4L2_CID_AUDIO_MUTE:
		ctrl->value = ((radio->registers[POWERCFG] &
				POWERCFG_DMUTE) == 0) ? 1 : 0;
		break;
	default:
		retval = -EINVAL;
	}

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"get control failed with %d\n", retval);

	mutex_unlock(&radio->lock);
	return retval;
}


static int si470x_vidioc_s_ctrl(struct file *file, void *priv,
		struct v4l2_control *ctrl)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	mutex_lock(&radio->lock);
	
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_VOLUME:
		radio->registers[SYSCONFIG2] &= ~SYSCONFIG2_VOLUME;
		radio->registers[SYSCONFIG2] |= ctrl->value;
		retval = si470x_set_register(radio, SYSCONFIG2);
		break;
	case V4L2_CID_AUDIO_MUTE:
		if (ctrl->value == 1)
			radio->registers[POWERCFG] &= ~POWERCFG_DMUTE;
		else
			radio->registers[POWERCFG] |= POWERCFG_DMUTE;
		retval = si470x_set_register(radio, POWERCFG);
		break;
	default:
		retval = -EINVAL;
	}

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"set control failed with %d\n", retval);
	mutex_unlock(&radio->lock);
	return retval;
}


static int si470x_vidioc_g_audio(struct file *file, void *priv,
		struct v4l2_audio *audio)
{
	
	audio->index = 0;
	strcpy(audio->name, "Radio");
	audio->capability = V4L2_AUDCAP_STEREO;
	audio->mode = 0;

	return 0;
}


static int si470x_vidioc_g_tuner(struct file *file, void *priv,
		struct v4l2_tuner *tuner)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	mutex_lock(&radio->lock);
	
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	if (tuner->index != 0) {
		retval = -EINVAL;
		goto done;
	}

	retval = si470x_get_register(radio, STATUSRSSI);
	if (retval < 0)
		goto done;

	
	strcpy(tuner->name, "FM");
	tuner->type = V4L2_TUNER_RADIO;
	tuner->capability = V4L2_TUNER_CAP_LOW | V4L2_TUNER_CAP_STEREO |
			    V4L2_TUNER_CAP_RDS | V4L2_TUNER_CAP_RDS_BLOCK_IO;

	
	switch ((radio->registers[SYSCONFIG2] & SYSCONFIG2_BAND) >> 6) {
	
	default:
		tuner->rangelow  =  87.5 * FREQ_MUL;
		tuner->rangehigh = 108   * FREQ_MUL;
		break;
	
	case 1:
		tuner->rangelow  =  76   * FREQ_MUL;
		tuner->rangehigh = 108   * FREQ_MUL;
		break;
	
	case 2:
		tuner->rangelow  =  76   * FREQ_MUL;
		tuner->rangehigh =  90   * FREQ_MUL;
		break;
	};

	
	if ((radio->registers[STATUSRSSI] & STATUSRSSI_ST) == 0)
		tuner->rxsubchans = V4L2_TUNER_SUB_MONO;
	else
		tuner->rxsubchans = V4L2_TUNER_SUB_MONO | V4L2_TUNER_SUB_STEREO;
	tuner->rxsubchans |= V4L2_TUNER_SUB_RDS;

	
	if ((radio->registers[POWERCFG] & POWERCFG_MONO) == 0)
		tuner->audmode = V4L2_TUNER_MODE_STEREO;
	else
		tuner->audmode = V4L2_TUNER_MODE_MONO;

	
	
	tuner->signal = (radio->registers[STATUSRSSI] & STATUSRSSI_RSSI);
	
	tuner->signal = (tuner->signal * 873) + (8 * tuner->signal / 10);

	
	
	tuner->afc = (radio->registers[STATUSRSSI] & STATUSRSSI_AFCRL) ? 1 : 0;

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"get tuner failed with %d\n", retval);
	mutex_unlock(&radio->lock);
	return retval;
}


static int si470x_vidioc_s_tuner(struct file *file, void *priv,
		struct v4l2_tuner *tuner)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	mutex_lock(&radio->lock);
	
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	if (tuner->index != 0)
		goto done;

	
	switch (tuner->audmode) {
	case V4L2_TUNER_MODE_MONO:
		radio->registers[POWERCFG] |= POWERCFG_MONO;  
		break;
	case V4L2_TUNER_MODE_STEREO:
		radio->registers[POWERCFG] &= ~POWERCFG_MONO; 
		break;
	default:
		goto done;
	}

	retval = si470x_set_register(radio, POWERCFG);

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"set tuner failed with %d\n", retval);
	mutex_unlock(&radio->lock);
	return retval;
}


static int si470x_vidioc_g_frequency(struct file *file, void *priv,
		struct v4l2_frequency *freq)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	
	mutex_lock(&radio->lock);
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	if (freq->tuner != 0) {
		retval = -EINVAL;
		goto done;
	}

	freq->type = V4L2_TUNER_RADIO;
	retval = si470x_get_freq(radio, &freq->frequency);

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"get frequency failed with %d\n", retval);
	mutex_unlock(&radio->lock);
	return retval;
}


static int si470x_vidioc_s_frequency(struct file *file, void *priv,
		struct v4l2_frequency *freq)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	mutex_lock(&radio->lock);
	
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	if (freq->tuner != 0) {
		retval = -EINVAL;
		goto done;
	}

	retval = si470x_set_freq(radio, freq->frequency);

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"set frequency failed with %d\n", retval);
	mutex_unlock(&radio->lock);
	return retval;
}


static int si470x_vidioc_s_hw_freq_seek(struct file *file, void *priv,
		struct v4l2_hw_freq_seek *seek)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	mutex_lock(&radio->lock);
	
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	if (seek->tuner != 0) {
		retval = -EINVAL;
		goto done;
	}

	retval = si470x_set_seek(radio, seek->wrap_around, seek->seek_upward);

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"set hardware frequency seek failed with %d\n", retval);
	mutex_unlock(&radio->lock);
	return retval;
}


static const struct v4l2_ioctl_ops si470x_ioctl_ops = {
	.vidioc_querycap	= si470x_vidioc_querycap,
	.vidioc_queryctrl	= si470x_vidioc_queryctrl,
	.vidioc_g_ctrl		= si470x_vidioc_g_ctrl,
	.vidioc_s_ctrl		= si470x_vidioc_s_ctrl,
	.vidioc_g_audio		= si470x_vidioc_g_audio,
	.vidioc_g_tuner		= si470x_vidioc_g_tuner,
	.vidioc_s_tuner		= si470x_vidioc_s_tuner,
	.vidioc_g_frequency	= si470x_vidioc_g_frequency,
	.vidioc_s_frequency	= si470x_vidioc_s_frequency,
	.vidioc_s_hw_freq_seek	= si470x_vidioc_s_hw_freq_seek,
};


struct video_device si470x_viddev_template = {
	.fops			= &si470x_fops,
	.name			= DRIVER_NAME,
	.release		= video_device_release,
	.ioctl_ops		= &si470x_ioctl_ops,
};
