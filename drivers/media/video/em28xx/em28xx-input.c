/*
  handle em28xx IR remotes via linux kernel input layer.

   Copyright (C) 2005 Ludovico Cavedon <cavedon@sssup.it>
		      Markus Rechberger <mrechberger@gmail.com>
		      Mauro Carvalho Chehab <mchehab@infradead.org>
		      Sascha Sommer <saschasommer@freenet.de>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/usb.h>
#include <linux/slab.h>

#include "em28xx.h"

#define EM28XX_SNAPSHOT_KEY KEY_CAMERA
#define EM28XX_SBUTTON_QUERY_INTERVAL 500
#define EM28XX_R0C_USBSUSP_SNAPSHOT 0x20

static unsigned int ir_debug;
module_param(ir_debug, int, 0644);
MODULE_PARM_DESC(ir_debug, "enable debug messages [IR]");

#define MODULE_NAME "em28xx"

#define i2cdprintk(fmt, arg...) \
	if (ir_debug) { \
		printk(KERN_DEBUG "%s/ir: " fmt, ir->name , ## arg); \
	}

#define dprintk(fmt, arg...) \
	if (ir_debug) { \
		printk(KERN_DEBUG "%s/ir: " fmt, ir->name , ## arg); \
	}


struct em28xx_ir_poll_result {
	unsigned int toggle_bit:1;
	unsigned int read_count:7;
	u8 rc_address;
	u8 rc_data[4]; 
};

struct em28xx_IR {
	struct em28xx *dev;
	struct rc_dev *rc;
	char name[32];
	char phys[32];

	
	int polling;
	struct delayed_work work;
	unsigned int full_code:1;
	unsigned int last_readcount;

	int  (*get_key)(struct em28xx_IR *, struct em28xx_ir_poll_result *);
};


int em28xx_get_key_terratec(struct IR_i2c *ir, u32 *ir_key, u32 *ir_raw)
{
	unsigned char b;

	
	if (1 != i2c_master_recv(ir->c, &b, 1)) {
		i2cdprintk("read error\n");
		return -EIO;
	}


	i2cdprintk("key %02x\n", b);

	if (b == 0xff)
		return 0;

	if (b == 0xfe)
		
		return 1;

	*ir_key = b;
	*ir_raw = b;
	return 1;
}

int em28xx_get_key_em_haup(struct IR_i2c *ir, u32 *ir_key, u32 *ir_raw)
{
	unsigned char buf[2];
	u16 code;
	int size;

	
	size = i2c_master_recv(ir->c, buf, sizeof(buf));

	if (size != 2)
		return -EIO;

	
	if (buf[1] == 0xff)
		return 0;

	ir->old = buf[1];

	code =
		 ((buf[0] & 0x01) ? 0x0020 : 0) | 
		 ((buf[0] & 0x02) ? 0x0010 : 0) | 
		 ((buf[0] & 0x04) ? 0x0008 : 0) | 
		 ((buf[0] & 0x08) ? 0x0004 : 0) | 
		 ((buf[0] & 0x10) ? 0x0002 : 0) | 
		 ((buf[0] & 0x20) ? 0x0001 : 0) | 
		 ((buf[1] & 0x08) ? 0x1000 : 0) | 
		 ((buf[1] & 0x10) ? 0x0800 : 0) | 
		 ((buf[1] & 0x20) ? 0x0400 : 0) | 
		 ((buf[1] & 0x40) ? 0x0200 : 0) | 
		 ((buf[1] & 0x80) ? 0x0100 : 0);  

	i2cdprintk("ir hauppauge (em2840): code=0x%02x (rcv=0x%02x%02x)\n",
			code, buf[1], buf[0]);

	
	*ir_key = code;
	*ir_raw = code;
	return 1;
}

int em28xx_get_key_pinnacle_usb_grey(struct IR_i2c *ir, u32 *ir_key,
				     u32 *ir_raw)
{
	unsigned char buf[3];

	

	if (3 != i2c_master_recv(ir->c, buf, 3)) {
		i2cdprintk("read error\n");
		return -EIO;
	}

	i2cdprintk("key %02x\n", buf[2]&0x3f);
	if (buf[0] != 0x00)
		return 0;

	*ir_key = buf[2]&0x3f;
	*ir_raw = buf[2]&0x3f;

	return 1;
}

int em28xx_get_key_winfast_usbii_deluxe(struct IR_i2c *ir, u32 *ir_key, u32 *ir_raw)
{
	unsigned char subaddr, keydetect, key;

	struct i2c_msg msg[] = { { .addr = ir->c->addr, .flags = 0, .buf = &subaddr, .len = 1},

				{ .addr = ir->c->addr, .flags = I2C_M_RD, .buf = &keydetect, .len = 1} };

	subaddr = 0x10;
	if (2 != i2c_transfer(ir->c->adapter, msg, 2)) {
		i2cdprintk("read error\n");
		return -EIO;
	}
	if (keydetect == 0x00)
		return 0;

	subaddr = 0x00;
	msg[1].buf = &key;
	if (2 != i2c_transfer(ir->c->adapter, msg, 2)) {
		i2cdprintk("read error\n");
	return -EIO;
	}
	if (key == 0x00)
		return 0;

	*ir_key = key;
	*ir_raw = key;
	return 1;
}


static int default_polling_getkey(struct em28xx_IR *ir,
				  struct em28xx_ir_poll_result *poll_result)
{
	struct em28xx *dev = ir->dev;
	int rc;
	u8 msg[3] = { 0, 0, 0 };

	rc = dev->em28xx_read_reg_req_len(dev, 0, EM28XX_R45_IR,
					  msg, sizeof(msg));
	if (rc < 0)
		return rc;

	
	poll_result->toggle_bit = (msg[0] >> 7);

	
	poll_result->read_count = (msg[0] & 0x7f);

	
	poll_result->rc_address = msg[1];

	
	poll_result->rc_data[0] = msg[2];

	return 0;
}

static int em2874_polling_getkey(struct em28xx_IR *ir,
				 struct em28xx_ir_poll_result *poll_result)
{
	struct em28xx *dev = ir->dev;
	int rc;
	u8 msg[5] = { 0, 0, 0, 0, 0 };

	rc = dev->em28xx_read_reg_req_len(dev, 0, EM2874_R51_IR,
					  msg, sizeof(msg));
	if (rc < 0)
		return rc;

	
	poll_result->toggle_bit = (msg[0] >> 7);

	
	poll_result->read_count = (msg[0] & 0x7f);

	
	poll_result->rc_address = msg[1];

	
	poll_result->rc_data[0] = msg[2];
	poll_result->rc_data[1] = msg[3];
	poll_result->rc_data[2] = msg[4];

	return 0;
}


static void em28xx_ir_handle_key(struct em28xx_IR *ir)
{
	int result;
	struct em28xx_ir_poll_result poll_result;

	
	result = ir->get_key(ir, &poll_result);
	if (unlikely(result < 0)) {
		dprintk("ir->get_key() failed %d\n", result);
		return;
	}

	if (unlikely(poll_result.read_count != ir->last_readcount)) {
		dprintk("%s: toggle: %d, count: %d, key 0x%02x%02x\n", __func__,
			poll_result.toggle_bit, poll_result.read_count,
			poll_result.rc_address, poll_result.rc_data[0]);
		if (ir->full_code)
			rc_keydown(ir->rc,
				   poll_result.rc_address << 8 |
				   poll_result.rc_data[0],
				   poll_result.toggle_bit);
		else
			rc_keydown(ir->rc,
				   poll_result.rc_data[0],
				   poll_result.toggle_bit);

		if (ir->dev->chip_id == CHIP_ID_EM2874 ||
		    ir->dev->chip_id == CHIP_ID_EM2884)
			ir->last_readcount = 0;
		else
			ir->last_readcount = poll_result.read_count;
	}
}

static void em28xx_ir_work(struct work_struct *work)
{
	struct em28xx_IR *ir = container_of(work, struct em28xx_IR, work.work);

	em28xx_ir_handle_key(ir);
	schedule_delayed_work(&ir->work, msecs_to_jiffies(ir->polling));
}

static int em28xx_ir_start(struct rc_dev *rc)
{
	struct em28xx_IR *ir = rc->priv;

	INIT_DELAYED_WORK(&ir->work, em28xx_ir_work);
	schedule_delayed_work(&ir->work, 0);

	return 0;
}

static void em28xx_ir_stop(struct rc_dev *rc)
{
	struct em28xx_IR *ir = rc->priv;

	cancel_delayed_work_sync(&ir->work);
}

int em28xx_ir_change_protocol(struct rc_dev *rc_dev, u64 rc_type)
{
	int rc = 0;
	struct em28xx_IR *ir = rc_dev->priv;
	struct em28xx *dev = ir->dev;
	u8 ir_config = EM2874_IR_RC5;

	

	if (rc_type == RC_TYPE_RC5) {
		dev->board.xclk |= EM28XX_XCLK_IR_RC5_MODE;
		ir->full_code = 1;
	} else if (rc_type == RC_TYPE_NEC) {
		dev->board.xclk &= ~EM28XX_XCLK_IR_RC5_MODE;
		ir_config = EM2874_IR_NEC;
		ir->full_code = 1;
	} else if (rc_type != RC_TYPE_UNKNOWN)
		rc = -EINVAL;

	em28xx_write_reg_bits(dev, EM28XX_R0F_XCLK, dev->board.xclk,
			      EM28XX_XCLK_IR_RC5_MODE);

	
	switch (dev->chip_id) {
	case CHIP_ID_EM2860:
	case CHIP_ID_EM2883:
		ir->get_key = default_polling_getkey;
		break;
	case CHIP_ID_EM2884:
	case CHIP_ID_EM2874:
	case CHIP_ID_EM28174:
		ir->get_key = em2874_polling_getkey;
		em28xx_write_regs(dev, EM2874_R50_IR_CONFIG, &ir_config, 1);
		break;
	default:
		printk("Unrecognized em28xx chip id 0x%02x: IR not supported\n",
			dev->chip_id);
		rc = -EINVAL;
	}

	return rc;
}

int em28xx_ir_init(struct em28xx *dev)
{
	struct em28xx_IR *ir;
	struct rc_dev *rc;
	int err = -ENOMEM;

	if (dev->board.ir_codes == NULL) {
		
		return 0;
	}

	ir = kzalloc(sizeof(*ir), GFP_KERNEL);
	rc = rc_allocate_device();
	if (!ir || !rc)
		goto err_out_free;

	
	ir->dev = dev;
	dev->ir = ir;
	ir->rc = rc;

	rc->allowed_protos = RC_TYPE_RC5 | RC_TYPE_NEC;
	rc->priv = ir;
	rc->change_protocol = em28xx_ir_change_protocol;
	rc->open = em28xx_ir_start;
	rc->close = em28xx_ir_stop;

	
	err = em28xx_ir_change_protocol(rc, RC_TYPE_UNKNOWN);
	if (err)
		goto err_out_free;

	
	ir->polling = 100; 

	
	snprintf(ir->name, sizeof(ir->name), "em28xx IR (%s)",
						dev->name);

	usb_make_path(dev->udev, ir->phys, sizeof(ir->phys));
	strlcat(ir->phys, "/input0", sizeof(ir->phys));

	rc->input_name = ir->name;
	rc->input_phys = ir->phys;
	rc->input_id.bustype = BUS_USB;
	rc->input_id.version = 1;
	rc->input_id.vendor = le16_to_cpu(dev->udev->descriptor.idVendor);
	rc->input_id.product = le16_to_cpu(dev->udev->descriptor.idProduct);
	rc->dev.parent = &dev->udev->dev;
	rc->map_name = dev->board.ir_codes;
	rc->driver_name = MODULE_NAME;

	
	err = rc_register_device(rc);
	if (err)
		goto err_out_stop;

	return 0;

 err_out_stop:
	dev->ir = NULL;
 err_out_free:
	rc_free_device(rc);
	kfree(ir);
	return err;
}

int em28xx_ir_fini(struct em28xx *dev)
{
	struct em28xx_IR *ir = dev->ir;

	
	if (!ir)
		return 0;

	if (ir->rc)
		rc_unregister_device(ir->rc);

	
	kfree(ir);
	dev->ir = NULL;
	return 0;
}


static void em28xx_query_sbutton(struct work_struct *work)
{
	
	struct em28xx *dev =
		container_of(work, struct em28xx, sbutton_query_work.work);
	int ret;

	ret = em28xx_read_reg(dev, EM28XX_R0C_USBSUSP);

	if (ret & EM28XX_R0C_USBSUSP_SNAPSHOT) {
		u8 cleared;
		
		cleared = ((u8) ret) & ~EM28XX_R0C_USBSUSP_SNAPSHOT;
		em28xx_write_regs(dev, EM28XX_R0C_USBSUSP, &cleared, 1);

		
		input_report_key(dev->sbutton_input_dev, EM28XX_SNAPSHOT_KEY,
				 1);
		
		input_report_key(dev->sbutton_input_dev, EM28XX_SNAPSHOT_KEY,
				 0);
	}

	
	schedule_delayed_work(&dev->sbutton_query_work,
			      msecs_to_jiffies(EM28XX_SBUTTON_QUERY_INTERVAL));
}

void em28xx_register_snapshot_button(struct em28xx *dev)
{
	struct input_dev *input_dev;
	int err;

	em28xx_info("Registering snapshot button...\n");
	input_dev = input_allocate_device();
	if (!input_dev) {
		em28xx_errdev("input_allocate_device failed\n");
		return;
	}

	usb_make_path(dev->udev, dev->snapshot_button_path,
		      sizeof(dev->snapshot_button_path));
	strlcat(dev->snapshot_button_path, "/sbutton",
		sizeof(dev->snapshot_button_path));
	INIT_DELAYED_WORK(&dev->sbutton_query_work, em28xx_query_sbutton);

	input_dev->name = "em28xx snapshot button";
	input_dev->phys = dev->snapshot_button_path;
	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
	set_bit(EM28XX_SNAPSHOT_KEY, input_dev->keybit);
	input_dev->keycodesize = 0;
	input_dev->keycodemax = 0;
	input_dev->id.bustype = BUS_USB;
	input_dev->id.vendor = le16_to_cpu(dev->udev->descriptor.idVendor);
	input_dev->id.product = le16_to_cpu(dev->udev->descriptor.idProduct);
	input_dev->id.version = 1;
	input_dev->dev.parent = &dev->udev->dev;

	err = input_register_device(input_dev);
	if (err) {
		em28xx_errdev("input_register_device failed\n");
		input_free_device(input_dev);
		return;
	}

	dev->sbutton_input_dev = input_dev;
	schedule_delayed_work(&dev->sbutton_query_work,
			      msecs_to_jiffies(EM28XX_SBUTTON_QUERY_INTERVAL));
	return;

}

void em28xx_deregister_snapshot_button(struct em28xx *dev)
{
	if (dev->sbutton_input_dev != NULL) {
		em28xx_info("Deregistering snapshot button\n");
		cancel_delayed_work_sync(&dev->sbutton_query_work);
		input_unregister_device(dev->sbutton_input_dev);
		dev->sbutton_input_dev = NULL;
	}
	return;
}
