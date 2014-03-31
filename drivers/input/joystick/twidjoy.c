/*
 *  Copyright (c) 2001 Arndt Schoenewald
 *  Copyright (c) 2000-2001 Vojtech Pavlik
 *  Copyright (c) 2000 Mark Fletcher
 *
 *  Sponsored by Quelltext AG (http://www.quelltext-ag.de), Dortmund, Germany
 */


/*
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/serio.h>
#include <linux/init.h>

#define DRIVER_DESC	"Handykey Twiddler keyboard as a joystick driver"

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");


#define TWIDJOY_MAX_LENGTH 5

static struct twidjoy_button_spec {
	int bitshift;
	int bitmask;
	int buttons[3];
}
twidjoy_buttons[] = {
	{  0, 3, { BTN_A,      BTN_B,     BTN_C    } },
	{  2, 3, { BTN_X,      BTN_Y,     BTN_Z    } },
	{  4, 3, { BTN_TL,     BTN_TR,    BTN_TR2  } },
	{  6, 3, { BTN_SELECT, BTN_START, BTN_MODE } },
	{  8, 1, { BTN_BASE5                       } },
	{  9, 1, { BTN_BASE                        } },
	{ 10, 1, { BTN_BASE3                       } },
	{ 11, 1, { BTN_BASE4                       } },
	{ 12, 1, { BTN_BASE2                       } },
	{ 13, 1, { BTN_BASE6                       } },
	{ 0,  0, { 0                               } }
};


struct twidjoy {
	struct input_dev *dev;
	int idx;
	unsigned char data[TWIDJOY_MAX_LENGTH];
	char phys[32];
};


static void twidjoy_process_packet(struct twidjoy *twidjoy)
{
	struct input_dev *dev = twidjoy->dev;
	unsigned char *data = twidjoy->data;
	struct twidjoy_button_spec *bp;
	int button_bits, abs_x, abs_y;

	button_bits = ((data[1] & 0x7f) << 7) | (data[0] & 0x7f);

	for (bp = twidjoy_buttons; bp->bitmask; bp++) {
		int value = (button_bits & (bp->bitmask << bp->bitshift)) >> bp->bitshift;
		int i;

		for (i = 0; i < bp->bitmask; i++)
			input_report_key(dev, bp->buttons[i], i+1 == value);
	}

	abs_x = ((data[4] & 0x07) << 5) | ((data[3] & 0x7C) >> 2);
	if (data[4] & 0x08) abs_x -= 256;

	abs_y = ((data[3] & 0x01) << 7) | ((data[2] & 0x7F) >> 0);
	if (data[3] & 0x02) abs_y -= 256;

	input_report_abs(dev, ABS_X, -abs_x);
	input_report_abs(dev, ABS_Y, +abs_y);

	input_sync(dev);
}


static irqreturn_t twidjoy_interrupt(struct serio *serio, unsigned char data, unsigned int flags)
{
	struct twidjoy *twidjoy = serio_get_drvdata(serio);


	if ((data & 0x80) == 0)
		twidjoy->idx = 0;	
	else if (twidjoy->idx == 0)
		return IRQ_HANDLED;	

	if (twidjoy->idx < TWIDJOY_MAX_LENGTH)
		twidjoy->data[twidjoy->idx++] = data;

	if (twidjoy->idx == TWIDJOY_MAX_LENGTH) {
		twidjoy_process_packet(twidjoy);
		twidjoy->idx = 0;
	}

	return IRQ_HANDLED;
}


static void twidjoy_disconnect(struct serio *serio)
{
	struct twidjoy *twidjoy = serio_get_drvdata(serio);

	serio_close(serio);
	serio_set_drvdata(serio, NULL);
	input_unregister_device(twidjoy->dev);
	kfree(twidjoy);
}


static int twidjoy_connect(struct serio *serio, struct serio_driver *drv)
{
	struct twidjoy_button_spec *bp;
	struct twidjoy *twidjoy;
	struct input_dev *input_dev;
	int err = -ENOMEM;
	int i;

	twidjoy = kzalloc(sizeof(struct twidjoy), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!twidjoy || !input_dev)
		goto fail1;

	twidjoy->dev = input_dev;
	snprintf(twidjoy->phys, sizeof(twidjoy->phys), "%s/input0", serio->phys);

	input_dev->name = "Handykey Twiddler";
	input_dev->phys = twidjoy->phys;
	input_dev->id.bustype = BUS_RS232;
	input_dev->id.vendor = SERIO_TWIDJOY;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;
	input_dev->dev.parent = &serio->dev;

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_set_abs_params(input_dev, ABS_X, -50, 50, 4, 4);
	input_set_abs_params(input_dev, ABS_Y, -50, 50, 4, 4);

	for (bp = twidjoy_buttons; bp->bitmask; bp++)
		for (i = 0; i < bp->bitmask; i++)
			set_bit(bp->buttons[i], input_dev->keybit);

	serio_set_drvdata(serio, twidjoy);

	err = serio_open(serio, drv);
	if (err)
		goto fail2;

	err = input_register_device(twidjoy->dev);
	if (err)
		goto fail3;

	return 0;

 fail3:	serio_close(serio);
 fail2:	serio_set_drvdata(serio, NULL);
 fail1:	input_free_device(input_dev);
	kfree(twidjoy);
	return err;
}


static struct serio_device_id twidjoy_serio_ids[] = {
	{
		.type	= SERIO_RS232,
		.proto	= SERIO_TWIDJOY,
		.id	= SERIO_ANY,
		.extra	= SERIO_ANY,
	},
	{ 0 }
};

MODULE_DEVICE_TABLE(serio, twidjoy_serio_ids);

static struct serio_driver twidjoy_drv = {
	.driver		= {
		.name	= "twidjoy",
	},
	.description	= DRIVER_DESC,
	.id_table	= twidjoy_serio_ids,
	.interrupt	= twidjoy_interrupt,
	.connect	= twidjoy_connect,
	.disconnect	= twidjoy_disconnect,
};


static int __init twidjoy_init(void)
{
	return serio_register_driver(&twidjoy_drv);
}

static void __exit twidjoy_exit(void)
{
	serio_unregister_driver(&twidjoy_drv);
}

module_init(twidjoy_init);
module_exit(twidjoy_exit);
