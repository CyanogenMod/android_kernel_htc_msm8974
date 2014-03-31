/*
 *   Driver for Midiman Portman2x4 parallel port midi interface
 *
 *   Copyright (c) by Levent Guendogdu <levon@feature-it.com>
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
 *
 * ChangeLog
 * Jan 24 2007 Matthias Koenig <mkoenig@suse.de>
 *      - cleanup and rewrite
 * Sep 30 2004 Tobias Gehrig <tobias@gehrig.tk>
 *      - source code cleanup
 * Sep 03 2004 Tobias Gehrig <tobias@gehrig.tk>
 *      - fixed compilation problem with alsa 1.0.6a (removed MODULE_CLASSES,
 *        MODULE_PARM_SYNTAX and changed MODULE_DEVICES to
 *        MODULE_SUPPORTED_DEVICE)
 * Mar 24 2004 Tobias Gehrig <tobias@gehrig.tk>
 *      - added 2.6 kernel support
 * Mar 18 2004 Tobias Gehrig <tobias@gehrig.tk>
 *      - added parport_unregister_driver to the startup routine if the driver fails to detect a portman
 *      - added support for all 4 output ports in portman_putmidi
 * Mar 17 2004 Tobias Gehrig <tobias@gehrig.tk>
 *      - added checks for opened input device in interrupt handler
 * Feb 20 2004 Tobias Gehrig <tobias@gehrig.tk>
 *      - ported from alsa 0.5 to 1.0
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/parport.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/rawmidi.h>
#include <sound/control.h>

#define CARD_NAME "Portman 2x4"
#define DRIVER_NAME "portman"
#define PLATFORM_DRIVER "snd_portman2x4"

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

MODULE_AUTHOR("Levent Guendogdu, Tobias Gehrig, Matthias Koenig");
MODULE_DESCRIPTION("Midiman Portman2x4");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("{{Midiman,Portman2x4}}");

#define PORTMAN_NUM_INPUT_PORTS 2
#define PORTMAN_NUM_OUTPUT_PORTS 4

struct portman {
	spinlock_t reg_lock;
	struct snd_card *card;
	struct snd_rawmidi *rmidi;
	struct pardevice *pardev;
	int pardev_claimed;

	int open_count;
	int mode[PORTMAN_NUM_INPUT_PORTS];
	struct snd_rawmidi_substream *midi_input[PORTMAN_NUM_INPUT_PORTS];
};

static int portman_free(struct portman *pm)
{
	kfree(pm);
	return 0;
}

static int __devinit portman_create(struct snd_card *card, 
				    struct pardevice *pardev, 
				    struct portman **rchip)
{
	struct portman *pm;

	*rchip = NULL;

	pm = kzalloc(sizeof(struct portman), GFP_KERNEL);
	if (pm == NULL) 
		return -ENOMEM;

	
	spin_lock_init(&pm->reg_lock);
	pm->card = card;
	pm->pardev = pardev;

	*rchip = pm;

	return 0;
}


#define	PP_STAT_BSY   	0x80	
#define	PP_STAT_ACK   	0x40	
#define	PP_STAT_POUT  	0x20	
#define	PP_STAT_SEL   	0x10	
#define	PP_STAT_ERR   	0x08	

#define	PP_CMD_IEN  	0x10	
#define	PP_CMD_SELI 	0x08	
#define	PP_CMD_INIT 	0x04	
#define	PP_CMD_FEED 	0x02	
#define	PP_CMD_STB      0x01	

#define	INT_EN	 	PP_CMD_IEN	
#define	STROBE	        PP_CMD_STB	

/* The parallel port command register field (b1..b3) selects the 
 * various "registers" within the PC/P 2x4.  These are the internal
 * address of these "registers" that must be written to the parallel
 * port command register.
 */
#define	RXDATA0		(0 << 1)	
#define	RXDATA1		(1 << 1)	
#define	GEN_CTL		(2 << 1)	
#define	SYNC_CTL 	(3 << 1)	
#define	TXDATA0		(4 << 1)	
#define	TXDATA1		(5 << 1)	
#define	TXDATA2		(6 << 1)	
#define	TXDATA3		(7 << 1)	

#define	ESTB		PP_STAT_POUT	
#define	INT_REQ         PP_STAT_ACK	
#define	BUSY            PP_STAT_ERR	

#define	RXAVAIL		PP_STAT_SEL	
#define	SYNC_STAT	PP_STAT_SEL	
#define	TXEMPTY		PP_STAT_SEL	

#define	RXDATA		PP_STAT_BSY	
#define	SYNC_DATA       PP_STAT_BSY	
					
#define	DATA_ECHO       PP_STAT_BSY	
#define	A0_ECHO         PP_STAT_BSY	
#define	A1_ECHO         PP_STAT_BSY	
#define	A2_ECHO         PP_STAT_BSY	

#define PORTMAN2X4_MODE_INPUT_TRIGGERED	 0x01

static inline void portman_write_command(struct portman *pm, u8 value)
{
	parport_write_control(pm->pardev->port, value);
}

static inline u8 portman_read_command(struct portman *pm)
{
	return parport_read_control(pm->pardev->port);
}

static inline u8 portman_read_status(struct portman *pm)
{
	return parport_read_status(pm->pardev->port);
}

static inline u8 portman_read_data(struct portman *pm)
{
	return parport_read_data(pm->pardev->port);
}

static inline void portman_write_data(struct portman *pm, u8 value)
{
	parport_write_data(pm->pardev->port, value);
}

static void portman_write_midi(struct portman *pm, 
			       int port, u8 mididata)
{
	int command = ((port + 4) << 1);

	command |= INT_EN;


	do {
		portman_write_command(pm, command);


		portman_write_data(pm, mididata);
		
	} while ((portman_read_status(pm) & TXEMPTY) != TXEMPTY);


	portman_write_command(pm, command | STROBE);


	while ((portman_read_status(pm) & ESTB) == 0)
		cpu_relax();

	
	portman_write_command(pm, command);

	while ((portman_read_status(pm) & ESTB) == ESTB)
		cpu_relax();


	while ((portman_read_status(pm) & BUSY) == BUSY)
		cpu_relax();

	
}


static int portman_read_midi(struct portman *pm, int port)
{
	unsigned char midi_data = 0;
	unsigned char cmdout;	

	
	portman_write_data(pm, 0);	

	
	cmdout = (port << 1) | INT_EN;	
	portman_write_command(pm, cmdout);

	while ((portman_read_status(pm) & ESTB) == ESTB)
		cpu_relax();	

	if ((portman_read_status(pm) & RXAVAIL) == 0)
		return -1;	

	
	portman_write_command(pm, cmdout | STROBE);	

	while ((portman_read_status(pm) & ESTB) == 0)
		cpu_relax(); 

	
	midi_data = (portman_read_status(pm) & 128);
	portman_write_data(pm, 1);	

	
	portman_write_data(pm, 0);	
	midi_data |= (portman_read_status(pm) >> 1) & 64;
	portman_write_data(pm, 1);	

	
	portman_write_data(pm, 0);	
	midi_data |= (portman_read_status(pm) >> 2) & 32;
	portman_write_data(pm, 1);	

	
	portman_write_data(pm, 0);	
	midi_data |= (portman_read_status(pm) >> 3) & 16;
	portman_write_data(pm, 1);	

	
	portman_write_data(pm, 0);	
	midi_data |= (portman_read_status(pm) >> 4) & 8;
	portman_write_data(pm, 1);	

	
	portman_write_data(pm, 0);	
	midi_data |= (portman_read_status(pm) >> 5) & 4;
	portman_write_data(pm, 1);	

	
	portman_write_data(pm, 0);	
	midi_data |= (portman_read_status(pm) >> 6) & 2;
	portman_write_data(pm, 1);	

	
	portman_write_data(pm, 0);	
	midi_data |= (portman_read_status(pm) >> 7) & 1;
	portman_write_data(pm, 1);	
	portman_write_data(pm, 0);	


	
	portman_write_command(pm, cmdout);	

	
	while ((portman_read_status(pm) & ESTB) == ESTB)
		cpu_relax();

	return (midi_data & 255);	
}

static int portman_data_avail(struct portman *pm, int channel)
{
	int command = INT_EN;
	switch (channel) {
	case 0:
		command |= RXDATA0;
		break;
	case 1:
		command |= RXDATA1;
		break;
	}
	
	portman_write_command(pm, command);
	
	if ((portman_read_status(pm) & RXAVAIL) == RXAVAIL)
		return 1;	

	
	return 0;
}


static void portman_flush_input(struct portman *pm, unsigned char port)
{
	
	unsigned int i = 0;
	unsigned char command = 0;

	switch (port) {
	case 0:
		command = RXDATA0;
		break;
	case 1:
		command = RXDATA1;
		break;
	default:
		snd_printk(KERN_WARNING
			   "portman_flush_input() Won't flush port %i\n",
			   port);
		return;
	}

	
	portman_write_command(pm, command);

	
	portman_write_command(pm, command | STROBE);

	
	while ((portman_read_status(pm) & ESTB) == 0)
		cpu_relax();

	
	portman_write_data(pm, 0);

	
	for (i = 0; i < 250; i++) {
		portman_write_data(pm, 1);
		portman_write_data(pm, 0);
	}

	
	portman_write_command(pm, command | INT_EN);

	
	while ((portman_read_status(pm) & ESTB) == ESTB)
		cpu_relax();
}

static int portman_probe(struct parport *p)
{
	
	parport_write_data(p, 0);

	
	parport_write_control(p, 0);

	
	
	parport_write_control(p, RXDATA0);	

	
	
	if ((parport_read_status(p) & ESTB) == ESTB)
		return 1;	

	
	
	parport_write_control(p, RXDATA0 + STROBE);	

	
	if ((parport_read_status(p) & ESTB) != ESTB)
		return 1;	

	
	parport_write_control(p, 0);	

	
	parport_write_control(p, TXDATA0);	

	
	if ((parport_read_status(p) & TXEMPTY) == 0)
		return 2;

	
	return 0;
}

static int portman_device_init(struct portman *pm)
{
	portman_flush_input(pm, 0);
	portman_flush_input(pm, 1);

	return 0;
}

static int snd_portman_midi_open(struct snd_rawmidi_substream *substream)
{
	return 0;
}

static int snd_portman_midi_close(struct snd_rawmidi_substream *substream)
{
	return 0;
}

static void snd_portman_midi_input_trigger(struct snd_rawmidi_substream *substream,
					   int up)
{
	struct portman *pm = substream->rmidi->private_data;
	unsigned long flags;

	spin_lock_irqsave(&pm->reg_lock, flags);
	if (up)
		pm->mode[substream->number] |= PORTMAN2X4_MODE_INPUT_TRIGGERED;
	else
		pm->mode[substream->number] &= ~PORTMAN2X4_MODE_INPUT_TRIGGERED;
	spin_unlock_irqrestore(&pm->reg_lock, flags);
}

static void snd_portman_midi_output_trigger(struct snd_rawmidi_substream *substream,
					    int up)
{
	struct portman *pm = substream->rmidi->private_data;
	unsigned long flags;
	unsigned char byte;

	spin_lock_irqsave(&pm->reg_lock, flags);
	if (up) {
		while ((snd_rawmidi_transmit(substream, &byte, 1) == 1))
			portman_write_midi(pm, substream->number, byte);
	}
	spin_unlock_irqrestore(&pm->reg_lock, flags);
}

static struct snd_rawmidi_ops snd_portman_midi_output = {
	.open =		snd_portman_midi_open,
	.close =	snd_portman_midi_close,
	.trigger =	snd_portman_midi_output_trigger,
};

static struct snd_rawmidi_ops snd_portman_midi_input = {
	.open =		snd_portman_midi_open,
	.close =	snd_portman_midi_close,
	.trigger =	snd_portman_midi_input_trigger,
};

static int __devinit snd_portman_rawmidi_create(struct snd_card *card)
{
	struct portman *pm = card->private_data;
	struct snd_rawmidi *rmidi;
	struct snd_rawmidi_substream *substream;
	int err;
	
	err = snd_rawmidi_new(card, CARD_NAME, 0, 
			      PORTMAN_NUM_OUTPUT_PORTS, 
			      PORTMAN_NUM_INPUT_PORTS, 
			      &rmidi);
	if (err < 0) 
		return err;

	rmidi->private_data = pm;
	strcpy(rmidi->name, CARD_NAME);
	rmidi->info_flags = SNDRV_RAWMIDI_INFO_OUTPUT |
		            SNDRV_RAWMIDI_INFO_INPUT |
                            SNDRV_RAWMIDI_INFO_DUPLEX;

	pm->rmidi = rmidi;

	
	snd_rawmidi_set_ops(rmidi, SNDRV_RAWMIDI_STREAM_OUTPUT, 
			    &snd_portman_midi_output);
	snd_rawmidi_set_ops(rmidi, SNDRV_RAWMIDI_STREAM_INPUT, 
			    &snd_portman_midi_input);

	
	
	list_for_each_entry(substream,
			    &rmidi->streams[SNDRV_RAWMIDI_STREAM_OUTPUT].substreams,
			    list) {
		sprintf(substream->name,
			"Portman2x4 %d", substream->number+1);
	}
	
	list_for_each_entry(substream,
			    &rmidi->streams[SNDRV_RAWMIDI_STREAM_INPUT].substreams,
			    list) {
		pm->midi_input[substream->number] = substream;
		sprintf(substream->name,
			"Portman2x4 %d", substream->number+1);
	}

	return err;
}

static void snd_portman_interrupt(void *userdata)
{
	unsigned char midivalue = 0;
	struct portman *pm = ((struct snd_card*)userdata)->private_data;

	spin_lock(&pm->reg_lock);

	
	while ((portman_read_status(pm) & INT_REQ) == INT_REQ) {
		if (portman_data_avail(pm, 0)) {
			
			midivalue = portman_read_midi(pm, 0);
			
			if (pm->mode[0] & PORTMAN2X4_MODE_INPUT_TRIGGERED)
				snd_rawmidi_receive(pm->midi_input[0],
						    &midivalue, 1);

		}
		if (portman_data_avail(pm, 1)) {
			
			midivalue = portman_read_midi(pm, 1);
			
			if (pm->mode[1] & PORTMAN2X4_MODE_INPUT_TRIGGERED)
				snd_rawmidi_receive(pm->midi_input[1],
						    &midivalue, 1);
		}

	}

	spin_unlock(&pm->reg_lock);
}

static int __devinit snd_portman_probe_port(struct parport *p)
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

	res = portman_probe(p);

	parport_release(pardev);
	parport_unregister_device(pardev);

	return res ? -EIO : 0;
}

static void __devinit snd_portman_attach(struct parport *p)
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

static void snd_portman_detach(struct parport *p)
{
	
}

static struct parport_driver portman_parport_driver = {
	.name   = "portman2x4",
	.attach = snd_portman_attach,
	.detach = snd_portman_detach
};

static void snd_portman_card_private_free(struct snd_card *card)
{
	struct portman *pm = card->private_data;
	struct pardevice *pardev = pm->pardev;

	if (pardev) {
		if (pm->pardev_claimed)
			parport_release(pardev);
		parport_unregister_device(pardev);
	}

	portman_free(pm);
}

static int __devinit snd_portman_probe(struct platform_device *pdev)
{
	struct pardevice *pardev;
	struct parport *p;
	int dev = pdev->id;
	struct snd_card *card = NULL;
	struct portman *pm = NULL;
	int err;

	p = platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, NULL);

	if (dev >= SNDRV_CARDS)
		return -ENODEV;
	if (!enable[dev]) 
		return -ENOENT;

	if ((err = snd_portman_probe_port(p)) < 0)
		return err;

	err = snd_card_create(index[dev], id[dev], THIS_MODULE, 0, &card);
	if (err < 0) {
		snd_printd("Cannot create card\n");
		return err;
	}
	strcpy(card->driver, DRIVER_NAME);
	strcpy(card->shortname, CARD_NAME);
	sprintf(card->longname,  "%s at 0x%lx, irq %i", 
		card->shortname, p->base, p->irq);

	pardev = parport_register_device(p,                     
					 DRIVER_NAME,           
					 NULL,                  
					 NULL,                  
					 snd_portman_interrupt, 
					 PARPORT_DEV_EXCL,      
					 (void *)card);         
	if (pardev == NULL) {
		snd_printd("Cannot register pardevice\n");
		err = -EIO;
		goto __err;
	}

	if ((err = portman_create(card, pardev, &pm)) < 0) {
		snd_printd("Cannot create main component\n");
		parport_unregister_device(pardev);
		goto __err;
	}
	card->private_data = pm;
	card->private_free = snd_portman_card_private_free;
	
	if ((err = snd_portman_rawmidi_create(card)) < 0) {
		snd_printd("Creating Rawmidi component failed\n");
		goto __err;
	}

	
	if (parport_claim(pardev)) {
		snd_printd("Cannot claim parport 0x%lx\n", pardev->port->base);
		err = -EIO;
		goto __err;
	}
	pm->pardev_claimed = 1;

	
	if ((err = portman_device_init(pm)) < 0)
		goto __err;

	platform_set_drvdata(pdev, card);

	snd_card_set_dev(card, &pdev->dev);

	
	if ((err = snd_card_register(card)) < 0) {
		snd_printd("Cannot register card\n");
		goto __err;
	}

	snd_printk(KERN_INFO "Portman 2x4 on 0x%lx\n", p->base);
	return 0;

__err:
	snd_card_free(card);
	return err;
}

static int __devexit snd_portman_remove(struct platform_device *pdev)
{
	struct snd_card *card = platform_get_drvdata(pdev);

	if (card)
		snd_card_free(card);

	return 0;
}


static struct platform_driver snd_portman_driver = {
	.probe  = snd_portman_probe,
	.remove = __devexit_p(snd_portman_remove),
	.driver = {
		.name = PLATFORM_DRIVER
	}
};

static void snd_portman_unregister_all(void)
{
	int i;

	for (i = 0; i < SNDRV_CARDS; ++i) {
		if (platform_devices[i]) {
			platform_device_unregister(platform_devices[i]);
			platform_devices[i] = NULL;
		}
	}		
	platform_driver_unregister(&snd_portman_driver);
	parport_unregister_driver(&portman_parport_driver);
}

static int __init snd_portman_module_init(void)
{
	int err;

	if ((err = platform_driver_register(&snd_portman_driver)) < 0)
		return err;

	if (parport_register_driver(&portman_parport_driver) != 0) {
		platform_driver_unregister(&snd_portman_driver);
		return -EIO;
	}

	if (device_count == 0) {
		snd_portman_unregister_all();
		return -ENODEV;
	}

	return 0;
}

static void __exit snd_portman_module_exit(void)
{
	snd_portman_unregister_all();
}

module_init(snd_portman_module_init);
module_exit(snd_portman_module_exit);
