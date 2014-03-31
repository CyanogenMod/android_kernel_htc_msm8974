/*
 * dm355evm_keys.c - support buttons and IR remote on DM355 EVM board
 *
 * Copyright (c) 2008 by David Brownell
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>

#include <linux/i2c/dm355evm_msp.h>
#include <linux/module.h>


struct dm355evm_keys {
	struct input_dev	*input;
	struct device		*dev;
	int			irq;
};

static const struct key_entry dm355evm_keys[] = {
	{ KE_KEY, 0x00d8, { KEY_OK } },		
	{ KE_KEY, 0x00b8, { KEY_UP } },		
	{ KE_KEY, 0x00e8, { KEY_DOWN } },	
	{ KE_KEY, 0x0078, { KEY_LEFT } },	
	{ KE_KEY, 0x00f0, { KEY_RIGHT } },	

	{ KE_KEY, 0x300c, { KEY_POWER } },	
	{ KE_KEY, 0x3000, { KEY_NUMERIC_0 } },
	{ KE_KEY, 0x3001, { KEY_NUMERIC_1 } },
	{ KE_KEY, 0x3002, { KEY_NUMERIC_2 } },
	{ KE_KEY, 0x3003, { KEY_NUMERIC_3 } },
	{ KE_KEY, 0x3004, { KEY_NUMERIC_4 } },
	{ KE_KEY, 0x3005, { KEY_NUMERIC_5 } },
	{ KE_KEY, 0x3006, { KEY_NUMERIC_6 } },
	{ KE_KEY, 0x3007, { KEY_NUMERIC_7 } },
	{ KE_KEY, 0x3008, { KEY_NUMERIC_8 } },
	{ KE_KEY, 0x3009, { KEY_NUMERIC_9 } },
	{ KE_KEY, 0x3022, { KEY_ENTER } },
	{ KE_KEY, 0x30ec, { KEY_MODE } },	
	{ KE_KEY, 0x300f, { KEY_SELECT } },	
	{ KE_KEY, 0x3020, { KEY_CHANNELUP } },	
	{ KE_KEY, 0x302e, { KEY_MENU } },	
	{ KE_KEY, 0x3011, { KEY_VOLUMEDOWN } },	
	{ KE_KEY, 0x300d, { KEY_MUTE } },	
	{ KE_KEY, 0x3010, { KEY_VOLUMEUP } },	
	{ KE_KEY, 0x301e, { KEY_SUBTITLE } },	
	{ KE_KEY, 0x3021, { KEY_CHANNELDOWN } },
	{ KE_KEY, 0x3022, { KEY_PREVIOUS } },
	{ KE_KEY, 0x3026, { KEY_SLEEP } },
	{ KE_KEY, 0x3172, { KEY_REWIND } },	
	{ KE_KEY, 0x3175, { KEY_PLAY } },
	{ KE_KEY, 0x3174, { KEY_FASTFORWARD } },
	{ KE_KEY, 0x3177, { KEY_RECORD } },
	{ KE_KEY, 0x3176, { KEY_STOP } },
	{ KE_KEY, 0x3169, { KEY_PAUSE } },
};

static irqreturn_t dm355evm_keys_irq(int irq, void *_keys)
{
	static u16 last_event;
	struct dm355evm_keys *keys = _keys;
	const struct key_entry *ke;
	unsigned int keycode;
	int status;
	u16 event;

	for (;;) {
		status = dm355evm_msp_read(DM355EVM_MSP_INPUT_HIGH);
		if (status < 0) {
			dev_dbg(keys->dev, "input high err %d\n",
					status);
			break;
		}
		event = status << 8;

		status = dm355evm_msp_read(DM355EVM_MSP_INPUT_LOW);
		if (status < 0) {
			dev_dbg(keys->dev, "input low err %d\n",
					status);
			break;
		}
		event |= status;
		if (event == 0xdead)
			break;

		if (event == last_event) {
			last_event = 0;
			continue;
		}
		last_event = event;

		
		event &= ~0x0800;

		
		ke = sparse_keymap_entry_from_scancode(keys->input, event);
		keycode = ke ? ke->keycode : KEY_UNKNOWN;
		dev_dbg(keys->dev,
			"input event 0x%04x--> keycode %d\n",
			event, keycode);

		
		input_report_key(keys->input, keycode, 1);
		input_sync(keys->input);
		input_report_key(keys->input, keycode, 0);
		input_sync(keys->input);
	}

	return IRQ_HANDLED;
}


static int __devinit dm355evm_keys_probe(struct platform_device *pdev)
{
	struct dm355evm_keys	*keys;
	struct input_dev	*input;
	int			status;

	
	keys = kzalloc(sizeof *keys, GFP_KERNEL);
	input = input_allocate_device();
	if (!keys || !input) {
		status = -ENOMEM;
		goto fail1;
	}

	keys->dev = &pdev->dev;
	keys->input = input;

	
	status = platform_get_irq(pdev, 0);
	if (status < 0)
		goto fail1;
	keys->irq = status;

	input_set_drvdata(input, keys);

	input->name = "DM355 EVM Controls";
	input->phys = "dm355evm/input0";
	input->dev.parent = &pdev->dev;

	input->id.bustype = BUS_I2C;
	input->id.product = 0x0355;
	input->id.version = dm355evm_msp_read(DM355EVM_MSP_FIRMREV);

	status = sparse_keymap_setup(input, dm355evm_keys, NULL);
	if (status)
		goto fail1;

	

	status = request_threaded_irq(keys->irq, NULL, dm355evm_keys_irq,
			IRQF_TRIGGER_FALLING, dev_name(&pdev->dev), keys);
	if (status < 0)
		goto fail2;

	
	status = input_register_device(input);
	if (status < 0)
		goto fail3;

	platform_set_drvdata(pdev, keys);

	return 0;

fail3:
	free_irq(keys->irq, keys);
fail2:
	sparse_keymap_free(input);
fail1:
	input_free_device(input);
	kfree(keys);
	dev_err(&pdev->dev, "can't register, err %d\n", status);

	return status;
}

static int __devexit dm355evm_keys_remove(struct platform_device *pdev)
{
	struct dm355evm_keys	*keys = platform_get_drvdata(pdev);

	free_irq(keys->irq, keys);
	sparse_keymap_free(keys->input);
	input_unregister_device(keys->input);
	kfree(keys);

	return 0;
}


static struct platform_driver dm355evm_keys_driver = {
	.probe		= dm355evm_keys_probe,
	.remove		= __devexit_p(dm355evm_keys_remove),
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "dm355evm_keys",
	},
};
module_platform_driver(dm355evm_keys_driver);

MODULE_LICENSE("GPL");
