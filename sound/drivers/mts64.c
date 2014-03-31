/*     
 *   ALSA Driver for Ego Systems Inc. (ESI) Miditerminal 4140
 *   Copyright (c) 2006 by Matthias König <mk@phasorlab.de>
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
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/parport.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/rawmidi.h>
#include <sound/control.h>

#define CARD_NAME "Miditerminal 4140"
#define DRIVER_NAME "MTS64"
#define PLATFORM_DRIVER "snd_mts64"

static int index[SNDRV_CARDS]  = SNDRV_DEFAULT_IDX;
static char *id[SNDRV_CARDS]   = SNDRV_DEFAULT_STR;
static bool enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;

static struct platform_device *platform_devices[SNDRV_CARDS]; 
static int device_count;

module_param_array(index, int, NULL, S_IRUGO);
MODULE_PARM_DESC(index, "Index value for " CARD_NAME " soundcard.");
module_param_array(id, charp, NULL, S_IRUGO);
MODULE_PARM_DESC(id, "ID string for " CARD_NAME " soundcard.");
module_param_array(enable, bool, NULL, S_IRUGO);
MODULE_PARM_DESC(enable, "Enable " CARD_NAME " soundcard.");

MODULE_AUTHOR("Matthias Koenig <mk@phasorlab.de>");
MODULE_DESCRIPTION("ESI Miditerminal 4140");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("{{ESI,Miditerminal 4140}}");

#define MTS64_NUM_INPUT_PORTS 5
#define MTS64_NUM_OUTPUT_PORTS 4
#define MTS64_SMPTE_SUBSTREAM 4

struct mts64 {
	spinlock_t lock;
	struct snd_card *card;
	struct snd_rawmidi *rmidi;
	struct pardevice *pardev;
	int pardev_claimed;

	int open_count;
	int current_midi_output_port;
	int current_midi_input_port;
	u8 mode[MTS64_NUM_INPUT_PORTS];
	struct snd_rawmidi_substream *midi_input_substream[MTS64_NUM_INPUT_PORTS];
	int smpte_switch;
	u8 time[4]; 
	u8 fps;
};

static int snd_mts64_free(struct mts64 *mts)
{
	kfree(mts);
	return 0;
}

static int __devinit snd_mts64_create(struct snd_card *card, 
				      struct pardevice *pardev, 
				      struct mts64 **rchip)
{
	struct mts64 *mts;

	*rchip = NULL;

	mts = kzalloc(sizeof(struct mts64), GFP_KERNEL);
	if (mts == NULL) 
		return -ENOMEM;

	
	spin_lock_init(&mts->lock);
	mts->card = card;
	mts->pardev = pardev;
	mts->current_midi_output_port = -1;
	mts->current_midi_input_port = -1;

	*rchip = mts;

	return 0;
}


#define MTS64_STAT_BSY             0x80
#define MTS64_STAT_BIT_SET         0x20  
#define MTS64_STAT_PORT            0x10  

#define MTS64_CTL_READOUT          0x08  
#define MTS64_CTL_WRITE_CMD        0x06  
#define MTS64_CTL_WRITE_DATA       0x02  
#define MTS64_CTL_STROBE           0x01  

#define MTS64_CMD_RESET            0xfe
#define MTS64_CMD_PROBE            0x8f  
#define MTS64_CMD_SMPTE_SET_TIME   0xe8
#define MTS64_CMD_SMPTE_SET_FPS    0xee
#define MTS64_CMD_SMPTE_STOP       0xef
#define MTS64_CMD_SMPTE_FPS_24     0xe3
#define MTS64_CMD_SMPTE_FPS_25     0xe2
#define MTS64_CMD_SMPTE_FPS_2997   0xe4 
#define MTS64_CMD_SMPTE_FPS_30D    0xe1
#define MTS64_CMD_SMPTE_FPS_30     0xe0
#define MTS64_CMD_COM_OPEN         0xf8  
#define MTS64_CMD_COM_CLOSE1       0xff  
#define MTS64_CMD_COM_CLOSE2       0xf5

static void mts64_enable_readout(struct parport *p);
static void mts64_disable_readout(struct parport *p);
static int mts64_device_ready(struct parport *p);
static int mts64_device_init(struct parport *p);
static int mts64_device_open(struct mts64 *mts);
static int mts64_device_close(struct mts64 *mts);
static u8 mts64_map_midi_input(u8 c);
static int mts64_probe(struct parport *p);
static u16 mts64_read(struct parport *p);
static u8 mts64_read_char(struct parport *p);
static void mts64_smpte_start(struct parport *p,
			      u8 hours, u8 minutes,
			      u8 seconds, u8 frames,
			      u8 idx);
static void mts64_smpte_stop(struct parport *p);
static void mts64_write_command(struct parport *p, u8 c);
static void mts64_write_data(struct parport *p, u8 c);
static void mts64_write_midi(struct mts64 *mts, u8 c, int midiport);


static void mts64_enable_readout(struct parport *p)
{
	u8 c;

	c = parport_read_control(p);
	c |= MTS64_CTL_READOUT;
	parport_write_control(p, c); 
}

static void mts64_disable_readout(struct parport *p)
{
	u8 c;

	c = parport_read_control(p);
	c &= ~MTS64_CTL_READOUT;
	parport_write_control(p, c);
}

static int mts64_device_ready(struct parport *p)
{
	int i;
	u8 c;

	for (i = 0; i < 0xffff; ++i) {
		c = parport_read_status(p);
		c &= MTS64_STAT_BSY;
		if (c != 0) 
			return 1;
	} 

	return 0;
}

static int __devinit mts64_device_init(struct parport *p)
{
	int i;

	mts64_write_command(p, MTS64_CMD_RESET);

	for (i = 0; i < 64; ++i) {
		msleep(100);

		if (mts64_probe(p) == 0) {
			
			mts64_disable_readout(p);
			return 0;
		}
	}
	mts64_disable_readout(p);

	return -EIO;
}

static int mts64_device_open(struct mts64 *mts)
{
	int i;
	struct parport *p = mts->pardev->port;

	for (i = 0; i < 5; ++i)
		mts64_write_command(p, MTS64_CMD_COM_OPEN);

	return 0;
}

static int mts64_device_close(struct mts64 *mts)
{
	int i;
	struct parport *p = mts->pardev->port;

	for (i = 0; i < 5; ++i) {
		mts64_write_command(p, MTS64_CMD_COM_CLOSE1);
		mts64_write_command(p, MTS64_CMD_COM_CLOSE2);
	}

	return 0;
}

static u8 mts64_map_midi_input(u8 c)
{
	static u8 map[] = { 0, 1, 4, 2, 3 };

	return map[c];
}


static int __devinit mts64_probe(struct parport *p)
{
	u8 c;

	mts64_smpte_stop(p);
	mts64_write_command(p, MTS64_CMD_PROBE);

	msleep(50);
	
	c = mts64_read(p);

	c &= 0x00ff;
	if (c != MTS64_CMD_PROBE) 
		return -ENODEV;
	else 
		return 0;

}

static u16 mts64_read(struct parport *p)
{
	u8 data, status;

	mts64_device_ready(p);
	mts64_enable_readout(p);
	status = parport_read_status(p);
	data = mts64_read_char(p);
	mts64_disable_readout(p);

	return (status << 8) | data;
}

static u8 mts64_read_char(struct parport *p)
{
	u8 c = 0;
	u8 status;
	u8 i;

	for (i = 0; i < 8; ++i) {
		parport_write_data(p, i);
		c >>= 1;
		status = parport_read_status(p);
		if (status & MTS64_STAT_BIT_SET) 
			c |= 0x80;
	}
	
	return c;
}

static void mts64_smpte_start(struct parport *p,
			      u8 hours, u8 minutes,
			      u8 seconds, u8 frames,
			      u8 idx)
{
	static u8 fps[5] = { MTS64_CMD_SMPTE_FPS_24, 
			     MTS64_CMD_SMPTE_FPS_25,
			     MTS64_CMD_SMPTE_FPS_2997, 
			     MTS64_CMD_SMPTE_FPS_30D,
			     MTS64_CMD_SMPTE_FPS_30    };

	mts64_write_command(p, MTS64_CMD_SMPTE_SET_TIME);
	mts64_write_command(p, frames);
	mts64_write_command(p, seconds);
	mts64_write_command(p, minutes);
	mts64_write_command(p, hours);

	mts64_write_command(p, MTS64_CMD_SMPTE_SET_FPS);
	mts64_write_command(p, fps[idx]);
}

static void mts64_smpte_stop(struct parport *p)
{
	mts64_write_command(p, MTS64_CMD_SMPTE_STOP);
}

static void mts64_write_command(struct parport *p, u8 c)
{
	mts64_device_ready(p);

	parport_write_data(p, c);

	parport_write_control(p, MTS64_CTL_WRITE_CMD);
	parport_write_control(p, MTS64_CTL_WRITE_CMD | MTS64_CTL_STROBE);
	parport_write_control(p, MTS64_CTL_WRITE_CMD);
}

static void mts64_write_data(struct parport *p, u8 c)
{
	mts64_device_ready(p);

	parport_write_data(p, c);

	parport_write_control(p, MTS64_CTL_WRITE_DATA);
	parport_write_control(p, MTS64_CTL_WRITE_DATA | MTS64_CTL_STROBE);
	parport_write_control(p, MTS64_CTL_WRITE_DATA);
}

static void mts64_write_midi(struct mts64 *mts, u8 c,
			     int midiport)
{
	struct parport *p = mts->pardev->port;

	
	if (mts->current_midi_output_port != midiport)
		mts64_write_command(p, midiport);

	
	mts64_write_data(p, c);
}


#define snd_mts64_ctl_smpte_switch_info		snd_ctl_boolean_mono_info

static int snd_mts64_ctl_smpte_switch_get(struct snd_kcontrol* kctl,
					  struct snd_ctl_elem_value *uctl)
{
	struct mts64 *mts = snd_kcontrol_chip(kctl);

	spin_lock_irq(&mts->lock);
	uctl->value.integer.value[0] = mts->smpte_switch;
	spin_unlock_irq(&mts->lock);

	return 0;
}

static int snd_mts64_ctl_smpte_switch_put(struct snd_kcontrol* kctl,
					  struct snd_ctl_elem_value *uctl)
{
	struct mts64 *mts = snd_kcontrol_chip(kctl);
	int changed = 0;
	int val = !!uctl->value.integer.value[0];

	spin_lock_irq(&mts->lock);
	if (mts->smpte_switch == val)
		goto __out;

	changed = 1;
	mts->smpte_switch = val;
	if (mts->smpte_switch) {
		mts64_smpte_start(mts->pardev->port,
				  mts->time[0], mts->time[1],
				  mts->time[2], mts->time[3],
				  mts->fps);
	} else {
		mts64_smpte_stop(mts->pardev->port);
	}
__out:
	spin_unlock_irq(&mts->lock);
	return changed;
}

static struct snd_kcontrol_new mts64_ctl_smpte_switch __devinitdata = {
	.iface = SNDRV_CTL_ELEM_IFACE_RAWMIDI,
	.name  = "SMPTE Playback Switch",
	.index = 0,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.private_value = 0,
	.info = snd_mts64_ctl_smpte_switch_info,
	.get  = snd_mts64_ctl_smpte_switch_get,
	.put  = snd_mts64_ctl_smpte_switch_put
};

static int snd_mts64_ctl_smpte_time_h_info(struct snd_kcontrol *kctl,
					   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 23;
	return 0;
}

static int snd_mts64_ctl_smpte_time_f_info(struct snd_kcontrol *kctl,
					   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 99;
	return 0;
}

static int snd_mts64_ctl_smpte_time_info(struct snd_kcontrol *kctl,
					 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 59;
	return 0;
}

static int snd_mts64_ctl_smpte_time_get(struct snd_kcontrol *kctl,
					struct snd_ctl_elem_value *uctl)
{
	struct mts64 *mts = snd_kcontrol_chip(kctl);
	int idx = kctl->private_value;

	spin_lock_irq(&mts->lock);
	uctl->value.integer.value[0] = mts->time[idx];
	spin_unlock_irq(&mts->lock);

	return 0;
}

static int snd_mts64_ctl_smpte_time_put(struct snd_kcontrol *kctl,
					struct snd_ctl_elem_value *uctl)
{
	struct mts64 *mts = snd_kcontrol_chip(kctl);
	int idx = kctl->private_value;
	unsigned int time = uctl->value.integer.value[0] % 60;
	int changed = 0;

	spin_lock_irq(&mts->lock);
	if (mts->time[idx] != time) {
		changed = 1;
		mts->time[idx] = time;
	}
	spin_unlock_irq(&mts->lock);

	return changed;
}

static struct snd_kcontrol_new mts64_ctl_smpte_time_hours __devinitdata = {
	.iface = SNDRV_CTL_ELEM_IFACE_RAWMIDI,
	.name  = "SMPTE Time Hours",
	.index = 0,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.private_value = 0,
	.info = snd_mts64_ctl_smpte_time_h_info,
	.get  = snd_mts64_ctl_smpte_time_get,
	.put  = snd_mts64_ctl_smpte_time_put
};

static struct snd_kcontrol_new mts64_ctl_smpte_time_minutes __devinitdata = {
	.iface = SNDRV_CTL_ELEM_IFACE_RAWMIDI,
	.name  = "SMPTE Time Minutes",
	.index = 0,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.private_value = 1,
	.info = snd_mts64_ctl_smpte_time_info,
	.get  = snd_mts64_ctl_smpte_time_get,
	.put  = snd_mts64_ctl_smpte_time_put
};

static struct snd_kcontrol_new mts64_ctl_smpte_time_seconds __devinitdata = {
	.iface = SNDRV_CTL_ELEM_IFACE_RAWMIDI,
	.name  = "SMPTE Time Seconds",
	.index = 0,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.private_value = 2,
	.info = snd_mts64_ctl_smpte_time_info,
	.get  = snd_mts64_ctl_smpte_time_get,
	.put  = snd_mts64_ctl_smpte_time_put
};

static struct snd_kcontrol_new mts64_ctl_smpte_time_frames __devinitdata = {
	.iface = SNDRV_CTL_ELEM_IFACE_RAWMIDI,
	.name  = "SMPTE Time Frames",
	.index = 0,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.private_value = 3,
	.info = snd_mts64_ctl_smpte_time_f_info,
	.get  = snd_mts64_ctl_smpte_time_get,
	.put  = snd_mts64_ctl_smpte_time_put
};

static int snd_mts64_ctl_smpte_fps_info(struct snd_kcontrol *kctl,
					struct snd_ctl_elem_info *uinfo)
{
	static char *texts[5] = { "24",
				  "25",
				  "29.97",
				  "30D",
				  "30"    };

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 5;
	if (uinfo->value.enumerated.item > 4)
		uinfo->value.enumerated.item = 4;
	strcpy(uinfo->value.enumerated.name,
	       texts[uinfo->value.enumerated.item]);
	
	return 0;
}

static int snd_mts64_ctl_smpte_fps_get(struct snd_kcontrol *kctl,
				       struct snd_ctl_elem_value *uctl)
{
	struct mts64 *mts = snd_kcontrol_chip(kctl);

	spin_lock_irq(&mts->lock);
	uctl->value.enumerated.item[0] = mts->fps;
	spin_unlock_irq(&mts->lock);

	return 0;
}

static int snd_mts64_ctl_smpte_fps_put(struct snd_kcontrol *kctl,
				       struct snd_ctl_elem_value *uctl)
{
	struct mts64 *mts = snd_kcontrol_chip(kctl);
	int changed = 0;

	if (uctl->value.enumerated.item[0] >= 5)
		return -EINVAL;
	spin_lock_irq(&mts->lock);
	if (mts->fps != uctl->value.enumerated.item[0]) {
		changed = 1;
		mts->fps = uctl->value.enumerated.item[0];
	}
	spin_unlock_irq(&mts->lock);

	return changed;
}

static struct snd_kcontrol_new mts64_ctl_smpte_fps __devinitdata = {
	.iface = SNDRV_CTL_ELEM_IFACE_RAWMIDI,
	.name  = "SMPTE Fps",
	.index = 0,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.private_value = 0,
	.info  = snd_mts64_ctl_smpte_fps_info,
	.get   = snd_mts64_ctl_smpte_fps_get,
	.put   = snd_mts64_ctl_smpte_fps_put
};


static int __devinit snd_mts64_ctl_create(struct snd_card *card, 
					  struct mts64 *mts) 
{
	int err, i;
	static struct snd_kcontrol_new *control[] __devinitdata = {
		&mts64_ctl_smpte_switch,
		&mts64_ctl_smpte_time_hours,
		&mts64_ctl_smpte_time_minutes,
		&mts64_ctl_smpte_time_seconds,
		&mts64_ctl_smpte_time_frames,
		&mts64_ctl_smpte_fps,
	        NULL  };

	for (i = 0; control[i]; ++i) {
		err = snd_ctl_add(card, snd_ctl_new1(control[i], mts));
		if (err < 0) {
			snd_printd("Cannot create control: %s\n", 
				   control[i]->name);
			return err;
		}
	}

	return 0;
}

#define MTS64_MODE_INPUT_TRIGGERED 0x01

static int snd_mts64_rawmidi_open(struct snd_rawmidi_substream *substream)
{
	struct mts64 *mts = substream->rmidi->private_data;

	if (mts->open_count == 0) {
		mts64_device_open(mts);

		msleep(50);
	}
	++(mts->open_count);

	return 0;
}

static int snd_mts64_rawmidi_close(struct snd_rawmidi_substream *substream)
{
	struct mts64 *mts = substream->rmidi->private_data;
	unsigned long flags;

	--(mts->open_count);
	if (mts->open_count == 0) {
		spin_lock_irqsave(&mts->lock, flags);
		mts64_device_close(mts);
		spin_unlock_irqrestore(&mts->lock, flags);

		msleep(500);

	} else if (mts->open_count < 0)
		mts->open_count = 0;

	return 0;
}

static void snd_mts64_rawmidi_output_trigger(struct snd_rawmidi_substream *substream,
					     int up)
{
	struct mts64 *mts = substream->rmidi->private_data;
	u8 data;
	unsigned long flags;

	spin_lock_irqsave(&mts->lock, flags);
	while (snd_rawmidi_transmit_peek(substream, &data, 1) == 1) {
		mts64_write_midi(mts, data, substream->number+1);
		snd_rawmidi_transmit_ack(substream, 1);
	}
	spin_unlock_irqrestore(&mts->lock, flags);
}

static void snd_mts64_rawmidi_input_trigger(struct snd_rawmidi_substream *substream,
					    int up)
{
	struct mts64 *mts = substream->rmidi->private_data;
	unsigned long flags;

	spin_lock_irqsave(&mts->lock, flags);
	if (up)
		mts->mode[substream->number] |= MTS64_MODE_INPUT_TRIGGERED;
	else
 		mts->mode[substream->number] &= ~MTS64_MODE_INPUT_TRIGGERED;
	
	spin_unlock_irqrestore(&mts->lock, flags);
}

static struct snd_rawmidi_ops snd_mts64_rawmidi_output_ops = {
	.open    = snd_mts64_rawmidi_open,
	.close   = snd_mts64_rawmidi_close,
	.trigger = snd_mts64_rawmidi_output_trigger
};

static struct snd_rawmidi_ops snd_mts64_rawmidi_input_ops = {
	.open    = snd_mts64_rawmidi_open,
	.close   = snd_mts64_rawmidi_close,
	.trigger = snd_mts64_rawmidi_input_trigger
};

static int __devinit snd_mts64_rawmidi_create(struct snd_card *card)
{
	struct mts64 *mts = card->private_data;
	struct snd_rawmidi *rmidi;
	struct snd_rawmidi_substream *substream;
	struct list_head *list;
	int err;
	
	err = snd_rawmidi_new(card, CARD_NAME, 0, 
			      MTS64_NUM_OUTPUT_PORTS, 
			      MTS64_NUM_INPUT_PORTS, 
			      &rmidi);
	if (err < 0) 
		return err;

	rmidi->private_data = mts;
	strcpy(rmidi->name, CARD_NAME);
	rmidi->info_flags = SNDRV_RAWMIDI_INFO_OUTPUT |
		            SNDRV_RAWMIDI_INFO_INPUT |
                            SNDRV_RAWMIDI_INFO_DUPLEX;

	mts->rmidi = rmidi;

	
	snd_rawmidi_set_ops(rmidi, SNDRV_RAWMIDI_STREAM_OUTPUT, 
			    &snd_mts64_rawmidi_output_ops);
	snd_rawmidi_set_ops(rmidi, SNDRV_RAWMIDI_STREAM_INPUT, 
			    &snd_mts64_rawmidi_input_ops);

	
	
	list_for_each(list, 
		      &rmidi->streams[SNDRV_RAWMIDI_STREAM_OUTPUT].substreams) {
		substream = list_entry(list, struct snd_rawmidi_substream, list);
		sprintf(substream->name,
			"Miditerminal %d", substream->number+1);
	}
	
	list_for_each(list, 
		      &rmidi->streams[SNDRV_RAWMIDI_STREAM_INPUT].substreams) {
		substream = list_entry(list, struct snd_rawmidi_substream, list);
		mts->midi_input_substream[substream->number] = substream;
		switch(substream->number) {
		case MTS64_SMPTE_SUBSTREAM:
			strcpy(substream->name, "Miditerminal SMPTE");
			break;
		default:
			sprintf(substream->name,
				"Miditerminal %d", substream->number+1);
		}
	}

	
	err = snd_mts64_ctl_create(card, mts);

	return err;
}

static void snd_mts64_interrupt(void *private)
{
	struct mts64 *mts = ((struct snd_card*)private)->private_data;
	u16 ret;
	u8 status, data;
	struct snd_rawmidi_substream *substream;

	spin_lock(&mts->lock);
	ret = mts64_read(mts->pardev->port);
	data = ret & 0x00ff;
	status = ret >> 8;

	if (status & MTS64_STAT_PORT) {
		mts->current_midi_input_port = mts64_map_midi_input(data);
	} else {
		if (mts->current_midi_input_port == -1) 
			goto __out;
		substream = mts->midi_input_substream[mts->current_midi_input_port];
		if (mts->mode[substream->number] & MTS64_MODE_INPUT_TRIGGERED)
			snd_rawmidi_receive(substream, &data, 1);
	}
__out:
	spin_unlock(&mts->lock);
}

static int __devinit snd_mts64_probe_port(struct parport *p)
{
	struct pardevice *pardev;
	int res;

	pardev = parport_register_device(p, DRIVER_NAME,
					 NULL, NULL, NULL,
					 0, NULL);
	if (!pardev)
		return -EIO;
	
	if (parport_claim(pardev)) {
		parport_unregister_device(pardev);
		return -EIO;
	}

	res = mts64_probe(p);

	parport_release(pardev);
	parport_unregister_device(pardev);

	return res;
}

static void __devinit snd_mts64_attach(struct parport *p)
{
	struct platform_device *device;

	device = platform_device_alloc(PLATFORM_DRIVER, device_count);
	if (!device)
		return;

	
	platform_set_drvdata(device, p);

	if (platform_device_add(device) < 0) {
		platform_device_put(device);
		return;
	}

	if (!platform_get_drvdata(device)) {
		platform_device_unregister(device);
		return;
	}

	
	platform_devices[device_count] = device;
	device_count++;
}

static void snd_mts64_detach(struct parport *p)
{
	
}

static struct parport_driver mts64_parport_driver = {
	.name   = "mts64",
	.attach = snd_mts64_attach,
	.detach = snd_mts64_detach
};

static void snd_mts64_card_private_free(struct snd_card *card)
{
	struct mts64 *mts = card->private_data;
	struct pardevice *pardev = mts->pardev;

	if (pardev) {
		if (mts->pardev_claimed)
			parport_release(pardev);
		parport_unregister_device(pardev);
	}

	snd_mts64_free(mts);
}

static int __devinit snd_mts64_probe(struct platform_device *pdev)
{
	struct pardevice *pardev;
	struct parport *p;
	int dev = pdev->id;
	struct snd_card *card = NULL;
	struct mts64 *mts = NULL;
	int err;

	p = platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, NULL);

	if (dev >= SNDRV_CARDS)
		return -ENODEV;
	if (!enable[dev]) 
		return -ENOENT;
	if ((err = snd_mts64_probe_port(p)) < 0)
		return err;

	err = snd_card_create(index[dev], id[dev], THIS_MODULE, 0, &card);
	if (err < 0) {
		snd_printd("Cannot create card\n");
		return err;
	}
	strcpy(card->driver, DRIVER_NAME);
	strcpy(card->shortname, "ESI " CARD_NAME);
	sprintf(card->longname,  "%s at 0x%lx, irq %i", 
		card->shortname, p->base, p->irq);

	pardev = parport_register_device(p,                   
					 DRIVER_NAME,         
					 NULL,                
					 NULL,                
					 snd_mts64_interrupt, 
					 PARPORT_DEV_EXCL,    
					 (void *)card);       
	if (pardev == NULL) {
		snd_printd("Cannot register pardevice\n");
		err = -EIO;
		goto __err;
	}

	if ((err = snd_mts64_create(card, pardev, &mts)) < 0) {
		snd_printd("Cannot create main component\n");
		parport_unregister_device(pardev);
		goto __err;
	}
	card->private_data = mts;
	card->private_free = snd_mts64_card_private_free;
	
	if ((err = snd_mts64_rawmidi_create(card)) < 0) {
		snd_printd("Creating Rawmidi component failed\n");
		goto __err;
	}

	
	if (parport_claim(pardev)) {
		snd_printd("Cannot claim parport 0x%lx\n", pardev->port->base);
		err = -EIO;
		goto __err;
	}
	mts->pardev_claimed = 1;

	
	if ((err = mts64_device_init(p)) < 0)
		goto __err;

	platform_set_drvdata(pdev, card);

	snd_card_set_dev(card, &pdev->dev);

	
	if ((err = snd_card_register(card)) < 0) {
		snd_printd("Cannot register card\n");
		goto __err;
	}

	snd_printk(KERN_INFO "ESI Miditerminal 4140 on 0x%lx\n", p->base);
	return 0;

__err:
	snd_card_free(card);
	return err;
}

static int __devexit snd_mts64_remove(struct platform_device *pdev)
{
	struct snd_card *card = platform_get_drvdata(pdev);

	if (card)
		snd_card_free(card);

	return 0;
}


static struct platform_driver snd_mts64_driver = {
	.probe  = snd_mts64_probe,
	.remove = __devexit_p(snd_mts64_remove),
	.driver = {
		.name = PLATFORM_DRIVER
	}
};

static void snd_mts64_unregister_all(void)
{
	int i;

	for (i = 0; i < SNDRV_CARDS; ++i) {
		if (platform_devices[i]) {
			platform_device_unregister(platform_devices[i]);
			platform_devices[i] = NULL;
		}
	}		
	platform_driver_unregister(&snd_mts64_driver);
	parport_unregister_driver(&mts64_parport_driver);
}

static int __init snd_mts64_module_init(void)
{
	int err;

	if ((err = platform_driver_register(&snd_mts64_driver)) < 0)
		return err;

	if (parport_register_driver(&mts64_parport_driver) != 0) {
		platform_driver_unregister(&snd_mts64_driver);
		return -EIO;
	}

	if (device_count == 0) {
		snd_mts64_unregister_all();
		return -ENODEV;
	}

	return 0;
}

static void __exit snd_mts64_module_exit(void)
{
	snd_mts64_unregister_all();
}

module_init(snd_mts64_module_init);
module_exit(snd_mts64_module_exit);
