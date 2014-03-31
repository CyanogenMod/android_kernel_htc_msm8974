/*
 * drivers/input/keyboard/jornada680_kbd.c
 *
 * HP Jornada 620/660/680/690 scan keyboard platform driver
 *  Copyright (C) 2007  Kristoffer Ericson <Kristoffer.Ericson@gmail.com>
 *
 * Based on hp680_keyb.c
 *  Copyright (C) 2006 Paul Mundt
 *  Copyright (C) 2005 Andriy Skulysh
 * Split from drivers/input/keyboard/hp600_keyb.c
 *  Copyright (C) 2000 Yaegashi Takeshi (hp6xx kbd scan routine and translation table)
 *  Copyright (C) 2000 Niibe Yutaka (HP620 Keyb translation table)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <asm/delay.h>
#include <asm/io.h>

#define PCCR 0xa4000104
#define PDCR 0xa4000106
#define PECR 0xa4000108
#define PFCR 0xa400010a
#define PCDR 0xa4000124
#define PDDR 0xa4000126
#define PEDR 0xa4000128
#define PFDR 0xa400012a
#define PGDR 0xa400012c
#define PHDR 0xa400012e
#define PJDR 0xa4000130
#define PKDR 0xa4000132
#define PLDR 0xa4000134

static const unsigned short jornada_scancodes[] = {
	KEY_CAPSLOCK, KEY_MACRO, KEY_LEFTCTRL, 0, KEY_ESC, KEY_KP5, 0, 0,			
		KEY_F1, KEY_F2, KEY_F3, KEY_F8, KEY_F7, KEY_F6, KEY_F4, KEY_F5,				
	KEY_SLASH, KEY_APOSTROPHE, KEY_ENTER, 0, KEY_Z, 0, 0, 0,				
		KEY_X, KEY_C, KEY_V, KEY_DOT, KEY_COMMA, KEY_M, KEY_B, KEY_N,				
	KEY_KP2, KEY_KP6, KEY_KP3, 0, 0, 0, 0, 0,						
		KEY_F10, KEY_RO, KEY_F9, KEY_KP4, KEY_NUMLOCK, KEY_SCROLLLOCK, KEY_LEFTALT, KEY_HANJA,	
	KEY_KATAKANA, KEY_KP0, KEY_GRAVE, 0, KEY_FINANCE, 0, 0, 0,				
		KEY_KPMINUS, KEY_HIRAGANA, KEY_SPACE, KEY_KPDOT, KEY_VOLUMEUP, 249, 0, 0,		
	KEY_SEMICOLON, KEY_RIGHTBRACE, KEY_BACKSLASH, 0, KEY_A, 0, 0, 0,			
		KEY_S, KEY_D, KEY_F, KEY_L, KEY_K, KEY_J, KEY_G, KEY_H,					
	KEY_KP8, KEY_LEFTMETA, KEY_RIGHTSHIFT, 0, KEY_TAB, 0, 0, 0,				
		0, KEY_LEFTSHIFT, KEY_KP7, KEY_KP9, KEY_KP1, KEY_F11, KEY_KPPLUS, KEY_KPASTERISK,	
	KEY_P, KEY_LEFTBRACE, KEY_BACKSPACE, 0, KEY_Q, 0, 0, 0,					
		KEY_W, KEY_E, KEY_R, KEY_O, KEY_I, KEY_U, KEY_T, KEY_Y,					
	KEY_0, KEY_MINUS, KEY_EQUAL, 0, KEY_1, 0, 0, 0,						
		KEY_2, KEY_3, KEY_4, KEY_9, KEY_8, KEY_7, KEY_5, KEY_6,					
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0
};

#define JORNADA_SCAN_SIZE	18

struct jornadakbd {
	struct input_polled_dev *poll_dev;
	unsigned short keymap[ARRAY_SIZE(jornada_scancodes)];
	unsigned char length;
	unsigned char old_scan[JORNADA_SCAN_SIZE];
	unsigned char new_scan[JORNADA_SCAN_SIZE];
};

static void jornada_parse_kbd(struct jornadakbd *jornadakbd)
{
	struct input_dev *input_dev = jornadakbd->poll_dev->input;
	unsigned short *keymap = jornadakbd->keymap;
	unsigned int sync_me = 0;
	unsigned int i, j;

	for (i = 0; i < JORNADA_SCAN_SIZE; i++) {
		unsigned char new = jornadakbd->new_scan[i];
		unsigned char old = jornadakbd->old_scan[i];
		unsigned int xor = new ^ old;

		if (xor == 0)
			continue;

		for (j = 0; j < 8; j++) {
			unsigned int bit = 1 << j;
			if (xor & bit) {
				unsigned int scancode = (i << 3) + j;
				input_event(input_dev,
					    EV_MSC, MSC_SCAN, scancode);
				input_report_key(input_dev,
						 keymap[scancode],
						 !(new & bit));
				sync_me = 1;
			}
		}
	}

	if (sync_me)
	    input_sync(input_dev);
}

static void jornada_scan_keyb(unsigned char *s)
{
	int i;
	unsigned short ec_static, dc_static; 
	unsigned char matrix_switch[] = {
		0xfd, 0xff,   
		0xdf, 0xff,   
		0x7f, 0xff,   
		0xff, 0xfe,   
		0xff, 0xfd,   
		0xff, 0xf7,   
		0xff, 0xbf,   
		0xff, 0x7f,   
	}, *t = matrix_switch;
	unsigned short matrix_PDE[] = {
		0xcc04, 0xf0cf,  
		0xc40c, 0xf0cf,	 
		0x4c0c, 0xf0cf,  
		0xcc0c, 0xf0cd,  
		0xcc0c, 0xf0c7,	 
		0xcc0c, 0xf04f,  
		0xcc0c, 0xd0cf,	 
		0xcc0c, 0x70cf,	 
	}, *y = matrix_PDE;

	
	dc_static = (__raw_readw(PDCR) & (~0xcc0c));
	ec_static = (__raw_readw(PECR) & (~0xf0cf));

	for (i = 0; i < 8; i++) {
		
		__raw_writew((dc_static | *y++), PDCR);
		__raw_writew((ec_static | *y++), PECR);
		udelay(5);

		
		__raw_writeb(*t++, PDDR);
		__raw_writeb(*t++, PEDR);
		udelay(50);

		
		*s++ = __raw_readb(PCDR);
		*s++ = __raw_readb(PFDR);
	}
	
	__raw_writeb(0xff, PDDR);
	__raw_writeb(0xff, PEDR);

	
	__raw_writew((dc_static | (0x5555 & 0xcc0c)),PDCR);
	__raw_writew((ec_static | (0x5555 & 0xf0cf)),PECR);

	
	*s++ = __raw_readb(PGDR);
	*s++ = __raw_readb(PHDR);
}

static void jornadakbd680_poll(struct input_polled_dev *dev)
{
	struct jornadakbd *jornadakbd = dev->private;

	jornada_scan_keyb(jornadakbd->new_scan);
	jornada_parse_kbd(jornadakbd);
	memcpy(jornadakbd->old_scan, jornadakbd->new_scan, JORNADA_SCAN_SIZE);
}

static int __devinit jornada680kbd_probe(struct platform_device *pdev)
{
	struct jornadakbd *jornadakbd;
	struct input_polled_dev *poll_dev;
	struct input_dev *input_dev;
	int i, error;

	jornadakbd = kzalloc(sizeof(struct jornadakbd), GFP_KERNEL);
	if (!jornadakbd)
		return -ENOMEM;

	poll_dev = input_allocate_polled_device();
	if (!poll_dev) {
		error = -ENOMEM;
		goto failed;
	}

	platform_set_drvdata(pdev, jornadakbd);

	jornadakbd->poll_dev = poll_dev;

	memcpy(jornadakbd->keymap, jornada_scancodes,
		sizeof(jornadakbd->keymap));

	poll_dev->private = jornadakbd;
	poll_dev->poll = jornadakbd680_poll;
	poll_dev->poll_interval = 50; 

	input_dev = poll_dev->input;
	input_dev->evbit[0] = BIT(EV_KEY) | BIT(EV_REP);
	input_dev->name = "HP Jornada 680 keyboard";
	input_dev->phys = "jornadakbd/input0";
	input_dev->keycode = jornadakbd->keymap;
	input_dev->keycodesize = sizeof(unsigned short);
	input_dev->keycodemax = ARRAY_SIZE(jornada_scancodes);
	input_dev->dev.parent = &pdev->dev;
	input_dev->id.bustype = BUS_HOST;

	for (i = 0; i < 128; i++)
		if (jornadakbd->keymap[i])
			__set_bit(jornadakbd->keymap[i], input_dev->keybit);
	__clear_bit(KEY_RESERVED, input_dev->keybit);

	input_set_capability(input_dev, EV_MSC, MSC_SCAN);

	error = input_register_polled_device(jornadakbd->poll_dev);
	if (error)
		goto failed;

	return 0;

 failed:
	printk(KERN_ERR "Jornadakbd: failed to register driver, error: %d\n",
		error);
	platform_set_drvdata(pdev, NULL);
	input_free_polled_device(poll_dev);
	kfree(jornadakbd);
	return error;

}

static int __devexit jornada680kbd_remove(struct platform_device *pdev)
{
	struct jornadakbd *jornadakbd = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	input_unregister_polled_device(jornadakbd->poll_dev);
	input_free_polled_device(jornadakbd->poll_dev);
	kfree(jornadakbd);

	return 0;
}

static struct platform_driver jornada680kbd_driver = {
	.driver	= {
		.name	= "jornada680_kbd",
		.owner	= THIS_MODULE,
	},
	.probe	= jornada680kbd_probe,
	.remove	= __devexit_p(jornada680kbd_remove),
};
module_platform_driver(jornada680kbd_driver);

MODULE_AUTHOR("Kristoffer Ericson <kristoffer.ericson@gmail.com>");
MODULE_DESCRIPTION("HP Jornada 620/660/680/690 Keyboard Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:jornada680_kbd");
