/*
 * Copyright (C) by Paul Barton-Davis 1998-1999
 *
 * This file is distributed under the GNU GENERAL PUBLIC LICENSE (GPL)
 * Version 2 (June 1991). See the "COPYING" file distributed with this
 * software for more info.  
 */


#include <asm/io.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <sound/core.h>
#include <sound/snd_wavefront.h>

static inline int 
wf_mpu_status (snd_wavefront_midi_t *midi)

{
	return inb (midi->mpu_status_port);
}

static inline int 
input_avail (snd_wavefront_midi_t *midi)

{
	return !(wf_mpu_status(midi) & INPUT_AVAIL);
}

static inline int
output_ready (snd_wavefront_midi_t *midi)

{
	return !(wf_mpu_status(midi) & OUTPUT_READY);
}

static inline int 
read_data (snd_wavefront_midi_t *midi)

{
	return inb (midi->mpu_data_port);
}

static inline void 
write_data (snd_wavefront_midi_t *midi, unsigned char byte)

{
	outb (byte, midi->mpu_data_port);
}

static snd_wavefront_midi_t *
get_wavefront_midi (struct snd_rawmidi_substream *substream)

{
	struct snd_card *card;
	snd_wavefront_card_t *acard;

	if (substream == NULL || substream->rmidi == NULL) 
	        return NULL;

	card = substream->rmidi->card;

	if (card == NULL) 
	        return NULL;

	if (card->private_data == NULL) 
 	        return NULL;

	acard = card->private_data;

	return &acard->wavefront.midi;
}

static void snd_wavefront_midi_output_write(snd_wavefront_card_t *card)
{
	snd_wavefront_midi_t *midi = &card->wavefront.midi;
	snd_wavefront_mpu_id  mpu;
	unsigned long flags;
	unsigned char midi_byte;
	int max = 256, mask = 1;
	int timeout;


	if (midi->substream_output[midi->output_mpu] == NULL) {
		goto __second;
	}

	while (max > 0) {

		

		for (timeout = 30000; timeout > 0; timeout--) {
			if (output_ready (midi))
				break;
		}
	
		spin_lock_irqsave (&midi->virtual, flags);
		if ((midi->mode[midi->output_mpu] & MPU401_MODE_OUTPUT) == 0) {
			spin_unlock_irqrestore (&midi->virtual, flags);
			goto __second;
		}
		if (output_ready (midi)) {
			if (snd_rawmidi_transmit(midi->substream_output[midi->output_mpu], &midi_byte, 1) == 1) {
				if (!midi->isvirtual ||
					(midi_byte != WF_INTERNAL_SWITCH &&
					 midi_byte != WF_EXTERNAL_SWITCH))
					write_data(midi, midi_byte);
				max--;
			} else {
				if (midi->istimer) {
					if (--midi->istimer <= 0)
						del_timer(&midi->timer);
				}
				midi->mode[midi->output_mpu] &= ~MPU401_MODE_OUTPUT_TRIGGER;
				spin_unlock_irqrestore (&midi->virtual, flags);
				goto __second;
			}
		} else {
			spin_unlock_irqrestore (&midi->virtual, flags);
			return;
		}
		spin_unlock_irqrestore (&midi->virtual, flags);
	}

      __second:

	if (midi->substream_output[!midi->output_mpu] == NULL) {
		return;
	}

	while (max > 0) {

		

		for (timeout = 30000; timeout > 0; timeout--) {
			if (output_ready (midi))
				break;
		}
	
		spin_lock_irqsave (&midi->virtual, flags);
		if (!midi->isvirtual)
			mask = 0;
		mpu = midi->output_mpu ^ mask;
		mask = 0;	
		if ((midi->mode[mpu] & MPU401_MODE_OUTPUT) == 0) {
			spin_unlock_irqrestore (&midi->virtual, flags);
			return;
		}
		if (snd_rawmidi_transmit_empty(midi->substream_output[mpu]))
			goto __timer;
		if (output_ready (midi)) {
			if (mpu != midi->output_mpu) {
				write_data(midi, mpu == internal_mpu ?
							WF_INTERNAL_SWITCH :
							WF_EXTERNAL_SWITCH);
				midi->output_mpu = mpu;
			} else if (snd_rawmidi_transmit(midi->substream_output[mpu], &midi_byte, 1) == 1) {
				if (!midi->isvirtual ||
					(midi_byte != WF_INTERNAL_SWITCH &&
					 midi_byte != WF_EXTERNAL_SWITCH))
					write_data(midi, midi_byte);
				max--;
			} else {
			      __timer:
				if (midi->istimer) {
					if (--midi->istimer <= 0)
						del_timer(&midi->timer);
				}
				midi->mode[mpu] &= ~MPU401_MODE_OUTPUT_TRIGGER;
				spin_unlock_irqrestore (&midi->virtual, flags);
				return;
			}
		} else {
			spin_unlock_irqrestore (&midi->virtual, flags);
			return;
		}
		spin_unlock_irqrestore (&midi->virtual, flags);
	}
}

static int snd_wavefront_midi_input_open(struct snd_rawmidi_substream *substream)
{
	unsigned long flags;
	snd_wavefront_midi_t *midi;
	snd_wavefront_mpu_id mpu;

	if (snd_BUG_ON(!substream || !substream->rmidi))
		return -ENXIO;
	if (snd_BUG_ON(!substream->rmidi->private_data))
		return -ENXIO;

	mpu = *((snd_wavefront_mpu_id *) substream->rmidi->private_data);

	if ((midi = get_wavefront_midi (substream)) == NULL)
	        return -EIO;

	spin_lock_irqsave (&midi->open, flags);
	midi->mode[mpu] |= MPU401_MODE_INPUT;
	midi->substream_input[mpu] = substream;
	spin_unlock_irqrestore (&midi->open, flags);

	return 0;
}

static int snd_wavefront_midi_output_open(struct snd_rawmidi_substream *substream)
{
	unsigned long flags;
	snd_wavefront_midi_t *midi;
	snd_wavefront_mpu_id mpu;

	if (snd_BUG_ON(!substream || !substream->rmidi))
		return -ENXIO;
	if (snd_BUG_ON(!substream->rmidi->private_data))
		return -ENXIO;

	mpu = *((snd_wavefront_mpu_id *) substream->rmidi->private_data);

	if ((midi = get_wavefront_midi (substream)) == NULL)
	        return -EIO;

	spin_lock_irqsave (&midi->open, flags);
	midi->mode[mpu] |= MPU401_MODE_OUTPUT;
	midi->substream_output[mpu] = substream;
	spin_unlock_irqrestore (&midi->open, flags);

	return 0;
}

static int snd_wavefront_midi_input_close(struct snd_rawmidi_substream *substream)
{
	unsigned long flags;
	snd_wavefront_midi_t *midi;
	snd_wavefront_mpu_id mpu;

	if (snd_BUG_ON(!substream || !substream->rmidi))
		return -ENXIO;
	if (snd_BUG_ON(!substream->rmidi->private_data))
		return -ENXIO;

	mpu = *((snd_wavefront_mpu_id *) substream->rmidi->private_data);

	if ((midi = get_wavefront_midi (substream)) == NULL)
	        return -EIO;

	spin_lock_irqsave (&midi->open, flags);
	midi->mode[mpu] &= ~MPU401_MODE_INPUT;
	spin_unlock_irqrestore (&midi->open, flags);

	return 0;
}

static int snd_wavefront_midi_output_close(struct snd_rawmidi_substream *substream)
{
	unsigned long flags;
	snd_wavefront_midi_t *midi;
	snd_wavefront_mpu_id mpu;

	if (snd_BUG_ON(!substream || !substream->rmidi))
		return -ENXIO;
	if (snd_BUG_ON(!substream->rmidi->private_data))
		return -ENXIO;

	mpu = *((snd_wavefront_mpu_id *) substream->rmidi->private_data);

	if ((midi = get_wavefront_midi (substream)) == NULL)
	        return -EIO;

	spin_lock_irqsave (&midi->open, flags);
	midi->mode[mpu] &= ~MPU401_MODE_OUTPUT;
	spin_unlock_irqrestore (&midi->open, flags);
	return 0;
}

static void snd_wavefront_midi_input_trigger(struct snd_rawmidi_substream *substream, int up)
{
	unsigned long flags;
	snd_wavefront_midi_t *midi;
	snd_wavefront_mpu_id mpu;

	if (substream == NULL || substream->rmidi == NULL) 
	        return;

	if (substream->rmidi->private_data == NULL)
	        return;

	mpu = *((snd_wavefront_mpu_id *) substream->rmidi->private_data);

	if ((midi = get_wavefront_midi (substream)) == NULL) {
		return;
	}

	spin_lock_irqsave (&midi->virtual, flags);
	if (up) {
		midi->mode[mpu] |= MPU401_MODE_INPUT_TRIGGER;
	} else {
		midi->mode[mpu] &= ~MPU401_MODE_INPUT_TRIGGER;
	}
	spin_unlock_irqrestore (&midi->virtual, flags);
}

static void snd_wavefront_midi_output_timer(unsigned long data)
{
	snd_wavefront_card_t *card = (snd_wavefront_card_t *)data;
	snd_wavefront_midi_t *midi = &card->wavefront.midi;
	unsigned long flags;
	
	spin_lock_irqsave (&midi->virtual, flags);
	midi->timer.expires = 1 + jiffies;
	add_timer(&midi->timer);
	spin_unlock_irqrestore (&midi->virtual, flags);
	snd_wavefront_midi_output_write(card);
}

static void snd_wavefront_midi_output_trigger(struct snd_rawmidi_substream *substream, int up)
{
	unsigned long flags;
	snd_wavefront_midi_t *midi;
	snd_wavefront_mpu_id mpu;

	if (substream == NULL || substream->rmidi == NULL) 
	        return;

	if (substream->rmidi->private_data == NULL)
	        return;

	mpu = *((snd_wavefront_mpu_id *) substream->rmidi->private_data);

	if ((midi = get_wavefront_midi (substream)) == NULL) {
		return;
	}

	spin_lock_irqsave (&midi->virtual, flags);
	if (up) {
		if ((midi->mode[mpu] & MPU401_MODE_OUTPUT_TRIGGER) == 0) {
			if (!midi->istimer) {
				init_timer(&midi->timer);
				midi->timer.function = snd_wavefront_midi_output_timer;
				midi->timer.data = (unsigned long) substream->rmidi->card->private_data;
				midi->timer.expires = 1 + jiffies;
				add_timer(&midi->timer);
			}
			midi->istimer++;
			midi->mode[mpu] |= MPU401_MODE_OUTPUT_TRIGGER;
		}
	} else {
		midi->mode[mpu] &= ~MPU401_MODE_OUTPUT_TRIGGER;
	}
	spin_unlock_irqrestore (&midi->virtual, flags);

	if (up)
		snd_wavefront_midi_output_write((snd_wavefront_card_t *)substream->rmidi->card->private_data);
}

void
snd_wavefront_midi_interrupt (snd_wavefront_card_t *card)

{
	unsigned long flags;
	snd_wavefront_midi_t *midi;
	static struct snd_rawmidi_substream *substream = NULL;
	static int mpu = external_mpu; 
	int max = 128;
	unsigned char byte;

	midi = &card->wavefront.midi;

	if (!input_avail (midi)) { 
		snd_wavefront_midi_output_write(card);
		return;
	}

	spin_lock_irqsave (&midi->virtual, flags);
	while (--max) {

		if (input_avail (midi)) {
			byte = read_data (midi);

			if (midi->isvirtual) {				
				if (byte == WF_EXTERNAL_SWITCH) {
					substream = midi->substream_input[external_mpu];
					mpu = external_mpu;
				} else if (byte == WF_INTERNAL_SWITCH) { 
					substream = midi->substream_output[internal_mpu];
					mpu = internal_mpu;
				} 
			} else {
				substream = midi->substream_input[internal_mpu];
				mpu = internal_mpu;
			}

			if (substream == NULL) {
				continue;
			}

			if (midi->mode[mpu] & MPU401_MODE_INPUT_TRIGGER) {
				snd_rawmidi_receive(substream, &byte, 1);
			}
		} else {
			break;
		}
	} 
	spin_unlock_irqrestore (&midi->virtual, flags);

	snd_wavefront_midi_output_write(card);
}

void
snd_wavefront_midi_enable_virtual (snd_wavefront_card_t *card)

{
	unsigned long flags;

	spin_lock_irqsave (&card->wavefront.midi.virtual, flags);
	card->wavefront.midi.isvirtual = 1;
	card->wavefront.midi.output_mpu = internal_mpu;
	card->wavefront.midi.input_mpu = internal_mpu;
	spin_unlock_irqrestore (&card->wavefront.midi.virtual, flags);
}

void
snd_wavefront_midi_disable_virtual (snd_wavefront_card_t *card)

{
	unsigned long flags;

	spin_lock_irqsave (&card->wavefront.midi.virtual, flags);
	
	
	card->wavefront.midi.isvirtual = 0;
	spin_unlock_irqrestore (&card->wavefront.midi.virtual, flags);
}

int __devinit
snd_wavefront_midi_start (snd_wavefront_card_t *card)

{
	int ok, i;
	unsigned char rbuf[4], wbuf[4];
	snd_wavefront_t *dev;
	snd_wavefront_midi_t *midi;

	dev = &card->wavefront;
	midi = &dev->midi;


	

	for (i = 0; i < 30000 && !output_ready (midi); i++);

	if (!output_ready (midi)) {
		snd_printk ("MIDI interface not ready for command\n");
		return -1;
	}


	dev->interrupts_are_midi = 1;
	
	outb (UART_MODE_ON, midi->mpu_command_port);

	for (ok = 0, i = 50000; i > 0 && !ok; i--) {
		if (input_avail (midi)) {
			if (read_data (midi) == MPU_ACK) {
				ok = 1;
				break;
			}
		}
	}

	if (!ok) {
		snd_printk ("cannot set UART mode for MIDI interface");
		dev->interrupts_are_midi = 0;
		return -1;
	}

	
    
	if (snd_wavefront_cmd (dev, WFC_MISYNTH_ON, rbuf, wbuf)) {
		snd_printk ("can't enable MIDI-IN-2-synth routing.\n");
		
	}


	if (snd_wavefront_cmd (dev, WFC_VMIDI_OFF, rbuf, wbuf)) { 
		snd_printk ("virtual MIDI mode not disabled\n");
		return 0; 
	}

	snd_wavefront_midi_enable_virtual (card);

	if (snd_wavefront_cmd (dev, WFC_VMIDI_ON, rbuf, wbuf)) {
		snd_printk ("cannot enable virtual MIDI mode.\n");
		snd_wavefront_midi_disable_virtual (card);
	} 
	return 0;
}

struct snd_rawmidi_ops snd_wavefront_midi_output =
{
	.open =		snd_wavefront_midi_output_open,
	.close =	snd_wavefront_midi_output_close,
	.trigger =	snd_wavefront_midi_output_trigger,
};

struct snd_rawmidi_ops snd_wavefront_midi_input =
{
	.open =		snd_wavefront_midi_input_open,
	.close =	snd_wavefront_midi_input_close,
	.trigger =	snd_wavefront_midi_input_trigger,
};

