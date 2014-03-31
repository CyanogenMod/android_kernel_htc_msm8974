/*
 * Fujifilm Finepix subdriver
 *
 * Copyright (C) 2008 Frank Zago
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define MODULE_NAME "finepix"

#include "gspca.h"

MODULE_AUTHOR("Frank Zago <frank@zago.net>");
MODULE_DESCRIPTION("Fujifilm FinePix USB V4L2 driver");
MODULE_LICENSE("GPL");

#define FPIX_TIMEOUT 250

#define FPIX_MAX_TRANSFER 0x2000

struct usb_fpix {
	struct gspca_dev gspca_dev;	

	struct work_struct work_struct;
	struct workqueue_struct *work_thread;
};

#define NEXT_FRAME_DELAY 35

static const struct v4l2_pix_format fpix_mode[1] = {
	{ 320, 240, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0}
};

static int command(struct gspca_dev *gspca_dev,
		int order)	
{
	static u8 order_values[2][12] = {
		{0xc6, 0, 0, 0, 0, 0, 0,    0, 0x20, 0, 0, 0},	
		{0xd3, 0, 0, 0, 0, 0, 0, 0x01,    0, 0, 0, 0},	
	};

	memcpy(gspca_dev->usb_buf, order_values[order], 12);
	return usb_control_msg(gspca_dev->dev,
			usb_sndctrlpipe(gspca_dev->dev, 0),
			USB_REQ_GET_STATUS,
			USB_DIR_OUT | USB_TYPE_CLASS |
			USB_RECIP_INTERFACE, 0, 0, gspca_dev->usb_buf,
			12, FPIX_TIMEOUT);
}

static void dostream(struct work_struct *work)
{
	struct usb_fpix *dev = container_of(work, struct usb_fpix, work_struct);
	struct gspca_dev *gspca_dev = &dev->gspca_dev;
	struct urb *urb = gspca_dev->urb[0];
	u8 *data = urb->transfer_buffer;
	int ret = 0;
	int len;

	
	mutex_lock(&gspca_dev->usb_lock);
	mutex_unlock(&gspca_dev->usb_lock);
	PDEBUG(D_STREAM, "dostream started");

	
again:
	while (gspca_dev->present && gspca_dev->streaming) {

		
		mutex_lock(&gspca_dev->usb_lock);
		ret = command(gspca_dev, 1);
		mutex_unlock(&gspca_dev->usb_lock);
		if (ret < 0)
			break;
		if (!gspca_dev->present || !gspca_dev->streaming)
			break;

		
		for (;;) {
			ret = usb_bulk_msg(gspca_dev->dev,
					urb->pipe,
					data,
					FPIX_MAX_TRANSFER,
					&len, FPIX_TIMEOUT);
			if (ret < 0) {
				goto again;
			}
			if (!gspca_dev->present || !gspca_dev->streaming)
				goto out;
			if (len < FPIX_MAX_TRANSFER ||
				(data[len - 2] == 0xff &&
					data[len - 1] == 0xd9)) {

				gspca_frame_add(gspca_dev, LAST_PACKET,
						data, len);
				break;
			}

			
			gspca_frame_add(gspca_dev,
					gspca_dev->last_packet_type
						== LAST_PACKET
					? FIRST_PACKET : INTER_PACKET,
					data, len);
		}

		msleep(NEXT_FRAME_DELAY);
	}

out:
	PDEBUG(D_STREAM, "dostream stopped");
}

static int sd_config(struct gspca_dev *gspca_dev,
		const struct usb_device_id *id)
{
	struct usb_fpix *dev = (struct usb_fpix *) gspca_dev;
	struct cam *cam = &gspca_dev->cam;

	cam->cam_mode = fpix_mode;
	cam->nmodes = 1;
	cam->bulk = 1;
	cam->bulk_size = FPIX_MAX_TRANSFER;

	INIT_WORK(&dev->work_struct, dostream);

	return 0;
}

static int sd_init(struct gspca_dev *gspca_dev)
{
	return 0;
}

static int sd_start(struct gspca_dev *gspca_dev)
{
	struct usb_fpix *dev = (struct usb_fpix *) gspca_dev;
	int ret, len;

	
	ret = command(gspca_dev, 0);
	if (ret < 0) {
		pr_err("init failed %d\n", ret);
		return ret;
	}

	ret = usb_bulk_msg(gspca_dev->dev,
			gspca_dev->urb[0]->pipe,
			gspca_dev->urb[0]->transfer_buffer,
			FPIX_MAX_TRANSFER, &len,
			FPIX_TIMEOUT);
	if (ret < 0) {
		pr_err("usb_bulk_msg failed %d\n", ret);
		return ret;
	}

	
	ret = command(gspca_dev, 1);
	if (ret < 0) {
		pr_err("frame request failed %d\n", ret);
		return ret;
	}

	
	usb_clear_halt(gspca_dev->dev, gspca_dev->urb[0]->pipe);

	
	dev->work_thread = create_singlethread_workqueue(MODULE_NAME);
	queue_work(dev->work_thread, &dev->work_struct);

	return 0;
}

static void sd_stop0(struct gspca_dev *gspca_dev)
{
	struct usb_fpix *dev = (struct usb_fpix *) gspca_dev;

	
	mutex_unlock(&gspca_dev->usb_lock);
	destroy_workqueue(dev->work_thread);
	mutex_lock(&gspca_dev->usb_lock);
	dev->work_thread = NULL;
}

static const struct usb_device_id device_table[] = {
	{USB_DEVICE(0x04cb, 0x0104)},
	{USB_DEVICE(0x04cb, 0x0109)},
	{USB_DEVICE(0x04cb, 0x010b)},
	{USB_DEVICE(0x04cb, 0x010f)},
	{USB_DEVICE(0x04cb, 0x0111)},
	{USB_DEVICE(0x04cb, 0x0113)},
	{USB_DEVICE(0x04cb, 0x0115)},
	{USB_DEVICE(0x04cb, 0x0117)},
	{USB_DEVICE(0x04cb, 0x0119)},
	{USB_DEVICE(0x04cb, 0x011b)},
	{USB_DEVICE(0x04cb, 0x011d)},
	{USB_DEVICE(0x04cb, 0x0121)},
	{USB_DEVICE(0x04cb, 0x0123)},
	{USB_DEVICE(0x04cb, 0x0125)},
	{USB_DEVICE(0x04cb, 0x0127)},
	{USB_DEVICE(0x04cb, 0x0129)},
	{USB_DEVICE(0x04cb, 0x012b)},
	{USB_DEVICE(0x04cb, 0x012d)},
	{USB_DEVICE(0x04cb, 0x012f)},
	{USB_DEVICE(0x04cb, 0x0131)},
	{USB_DEVICE(0x04cb, 0x013b)},
	{USB_DEVICE(0x04cb, 0x013d)},
	{USB_DEVICE(0x04cb, 0x013f)},
	{}
};

MODULE_DEVICE_TABLE(usb, device_table);

static const struct sd_desc sd_desc = {
	.name   = MODULE_NAME,
	.config = sd_config,
	.init   = sd_init,
	.start  = sd_start,
	.stop0  = sd_stop0,
};

static int sd_probe(struct usb_interface *intf,
		const struct usb_device_id *id)
{
	return gspca_dev_probe(intf, id,
			&sd_desc,
			sizeof(struct usb_fpix),
			THIS_MODULE);
}

static struct usb_driver sd_driver = {
	.name       = MODULE_NAME,
	.id_table   = device_table,
	.probe      = sd_probe,
	.disconnect = gspca_disconnect,
#ifdef CONFIG_PM
	.suspend = gspca_suspend,
	.resume  = gspca_resume,
#endif
};

module_usb_driver(sd_driver);
