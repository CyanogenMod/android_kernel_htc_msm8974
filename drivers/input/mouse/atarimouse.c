/*
 *  Atari mouse driver for Linux/m68k
 *
 *  Copyright (c) 2005 Michael Schmitz
 *
 *  Based on:
 *  Amiga mouse driver for Linux/m68k
 *
 *  Copyright (c) 2000-2002 Vojtech Pavlik
 *
 */


/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>

#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/uaccess.h>
#include <asm/atarihw.h>
#include <asm/atarikb.h>
#include <asm/atariints.h>

MODULE_AUTHOR("Michael Schmitz <schmitz@biophys.uni-duesseldorf.de>");
MODULE_DESCRIPTION("Atari mouse driver");
MODULE_LICENSE("GPL");

static int mouse_threshold[2] = {2, 2};
module_param_array(mouse_threshold, int, NULL, 0);

#ifdef FIXED_ATARI_JOYSTICK
extern int atari_mouse_buttons;
#endif

static struct input_dev *atamouse_dev;

static void atamouse_interrupt(char *buf)
{
	int buttons, dx, dy;

	buttons = (buf[0] & 1) | ((buf[0] & 2) << 1);
#ifdef FIXED_ATARI_JOYSTICK
	buttons |= atari_mouse_buttons & 2;
	atari_mouse_buttons = buttons;
#endif

	
	dx = buf[1];
	dy = buf[2];

	input_report_rel(atamouse_dev, REL_X, dx);
	input_report_rel(atamouse_dev, REL_Y, dy);

	input_report_key(atamouse_dev, BTN_LEFT,   buttons & 0x4);
	input_report_key(atamouse_dev, BTN_MIDDLE, buttons & 0x2);
	input_report_key(atamouse_dev, BTN_RIGHT,  buttons & 0x1);

	input_sync(atamouse_dev);

	return;
}

static int atamouse_open(struct input_dev *dev)
{
#ifdef FIXED_ATARI_JOYSTICK
	atari_mouse_buttons = 0;
#endif
	ikbd_mouse_y0_top();
	ikbd_mouse_thresh(mouse_threshold[0], mouse_threshold[1]);
	ikbd_mouse_rel_pos();
	atari_input_mouse_interrupt_hook = atamouse_interrupt;

	return 0;
}

static void atamouse_close(struct input_dev *dev)
{
	ikbd_mouse_disable();
	atari_input_mouse_interrupt_hook = NULL;
}

static int __init atamouse_init(void)
{
	int error;

	if (!MACH_IS_ATARI || !ATARIHW_PRESENT(ST_MFP))
		return -ENODEV;

	error = atari_keyb_init();
	if (error)
		return error;

	atamouse_dev = input_allocate_device();
	if (!atamouse_dev)
		return -ENOMEM;

	atamouse_dev->name = "Atari mouse";
	atamouse_dev->phys = "atamouse/input0";
	atamouse_dev->id.bustype = BUS_HOST;
	atamouse_dev->id.vendor = 0x0001;
	atamouse_dev->id.product = 0x0002;
	atamouse_dev->id.version = 0x0100;

	atamouse_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);
	atamouse_dev->relbit[0] = BIT_MASK(REL_X) | BIT_MASK(REL_Y);
	atamouse_dev->keybit[BIT_WORD(BTN_LEFT)] = BIT_MASK(BTN_LEFT) |
		BIT_MASK(BTN_MIDDLE) | BIT_MASK(BTN_RIGHT);

	atamouse_dev->open = atamouse_open;
	atamouse_dev->close = atamouse_close;

	error = input_register_device(atamouse_dev);
	if (error) {
		input_free_device(atamouse_dev);
		return error;
	}

	return 0;
}

static void __exit atamouse_exit(void)
{
	input_unregister_device(atamouse_dev);
}

module_init(atamouse_init);
module_exit(atamouse_exit);
