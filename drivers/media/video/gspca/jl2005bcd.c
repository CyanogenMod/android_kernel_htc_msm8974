/*
 * Jeilin JL2005B/C/D library
 *
 * Copyright (C) 2011 Theodore Kilgore <kilgota@auburn.edu>
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

#define MODULE_NAME "jl2005bcd"

#include <linux/workqueue.h>
#include <linux/slab.h>
#include "gspca.h"


MODULE_AUTHOR("Theodore Kilgore <kilgota@auburn.edu>");
MODULE_DESCRIPTION("JL2005B/C/D USB Camera Driver");
MODULE_LICENSE("GPL");

#define JL2005C_CMD_TIMEOUT 500
#define JL2005C_DATA_TIMEOUT 1000

#define JL2005C_MAX_TRANSFER 0x200
#define FRAME_HEADER_LEN 16


struct sd {
	struct gspca_dev gspca_dev;  
	unsigned char firmware_id[6];
	const struct v4l2_pix_format *cap_mode;
	
	struct work_struct work_struct;
	struct workqueue_struct *work_thread;
	u8 frame_brightness;
	int block_size;	
	int vga;	
};


static const struct v4l2_pix_format cif_mode[] = {
	{176, 144, V4L2_PIX_FMT_JL2005BCD, V4L2_FIELD_NONE,
		.bytesperline = 176,
		.sizeimage = 176 * 144,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0},
	{352, 288, V4L2_PIX_FMT_JL2005BCD, V4L2_FIELD_NONE,
		.bytesperline = 352,
		.sizeimage = 352 * 288,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0},
};

static const struct v4l2_pix_format vga_mode[] = {
	{320, 240, V4L2_PIX_FMT_JL2005BCD, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0},
	{640, 480, V4L2_PIX_FMT_JL2005BCD, V4L2_FIELD_NONE,
		.bytesperline = 640,
		.sizeimage = 640 * 480,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0},
};


static int jl2005c_write2(struct gspca_dev *gspca_dev, unsigned char *command)
{
	int retval;

	memcpy(gspca_dev->usb_buf, command, 2);
	retval = usb_bulk_msg(gspca_dev->dev,
			usb_sndbulkpipe(gspca_dev->dev, 3),
			gspca_dev->usb_buf, 2, NULL, 500);
	if (retval < 0)
		pr_err("command write [%02x] error %d\n",
		       gspca_dev->usb_buf[0], retval);
	return retval;
}

static int jl2005c_read1(struct gspca_dev *gspca_dev)
{
	int retval;

	retval = usb_bulk_msg(gspca_dev->dev,
				usb_rcvbulkpipe(gspca_dev->dev, 0x84),
				gspca_dev->usb_buf, 1, NULL, 500);
	if (retval < 0)
		pr_err("read command [0x%02x] error %d\n",
		       gspca_dev->usb_buf[0], retval);
	return retval;
}

static int jl2005c_read_reg(struct gspca_dev *gspca_dev, unsigned char reg)
{
	int retval;

	static u8 instruction[2] = {0x95, 0x00};
	
	instruction[1] = reg;
	
	retval = jl2005c_write2(gspca_dev, instruction);
	if (retval < 0)
		return retval;
	retval = jl2005c_read1(gspca_dev);

	return retval;
}

static int jl2005c_start_new_frame(struct gspca_dev *gspca_dev)
{
	int i;
	int retval;
	int frame_brightness = 0;

	static u8 instruction[2] = {0x7f, 0x01};

	retval = jl2005c_write2(gspca_dev, instruction);
	if (retval < 0)
		return retval;

	i = 0;
	while (i < 20 && !frame_brightness) {
		
		retval = jl2005c_read_reg(gspca_dev, 0x7e);
		if (retval < 0)
			return retval;
		frame_brightness = gspca_dev->usb_buf[0];
		retval = jl2005c_read_reg(gspca_dev, 0x7d);
		if (retval < 0)
			return retval;
		i++;
	}
	PDEBUG(D_FRAM, "frame_brightness is 0x%02x", gspca_dev->usb_buf[0]);
	return retval;
}

static int jl2005c_write_reg(struct gspca_dev *gspca_dev, unsigned char reg,
						    unsigned char value)
{
	int retval;
	u8 instruction[2];

	instruction[0] = reg;
	instruction[1] = value;

	retval = jl2005c_write2(gspca_dev, instruction);
	if (retval < 0)
			return retval;

	return retval;
}

static int jl2005c_get_firmware_id(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *)gspca_dev;
	int i = 0;
	int retval = -1;
	unsigned char regs_to_read[] = {0x57, 0x02, 0x03, 0x5d, 0x5e, 0x5f};

	PDEBUG(D_PROBE, "Running jl2005c_get_firmware_id");
	
	retval = jl2005c_read_reg(gspca_dev, regs_to_read[0]);
	PDEBUG(D_PROBE, "response is %02x", gspca_dev->usb_buf[0]);
	if (retval < 0)
		return retval;
	
	for (i = 0; i < 6; i++) {
		retval = jl2005c_read_reg(gspca_dev, regs_to_read[i]);
		if (retval < 0)
			return retval;
		sd->firmware_id[i] = gspca_dev->usb_buf[0];
	}
	PDEBUG(D_PROBE, "firmware ID is %02x%02x%02x%02x%02x%02x",
						sd->firmware_id[0],
						sd->firmware_id[1],
						sd->firmware_id[2],
						sd->firmware_id[3],
						sd->firmware_id[4],
						sd->firmware_id[5]);
	return 0;
}

static int jl2005c_stream_start_vga_lg
		    (struct gspca_dev *gspca_dev)
{
	int i;
	int retval = -1;
	static u8 instruction[][2] = {
		{0x05, 0x00},
		{0x7c, 0x00},
		{0x7d, 0x18},
		{0x02, 0x00},
		{0x01, 0x00},
		{0x04, 0x52},
	};

	for (i = 0; i < ARRAY_SIZE(instruction); i++) {
		msleep(60);
		retval = jl2005c_write2(gspca_dev, instruction[i]);
		if (retval < 0)
			return retval;
	}
	msleep(60);
	return retval;
}

static int jl2005c_stream_start_vga_small(struct gspca_dev *gspca_dev)
{
	int i;
	int retval = -1;
	static u8 instruction[][2] = {
		{0x06, 0x00},
		{0x7c, 0x00},
		{0x7d, 0x1a},
		{0x02, 0x00},
		{0x01, 0x00},
		{0x04, 0x52},
	};

	for (i = 0; i < ARRAY_SIZE(instruction); i++) {
		msleep(60);
		retval = jl2005c_write2(gspca_dev, instruction[i]);
		if (retval < 0)
			return retval;
	}
	msleep(60);
	return retval;
}

static int jl2005c_stream_start_cif_lg(struct gspca_dev *gspca_dev)
{
	int i;
	int retval = -1;
	static u8 instruction[][2] = {
		{0x05, 0x00},
		{0x7c, 0x00},
		{0x7d, 0x30},
		{0x02, 0x00},
		{0x01, 0x00},
		{0x04, 0x42},
	};

	for (i = 0; i < ARRAY_SIZE(instruction); i++) {
		msleep(60);
		retval = jl2005c_write2(gspca_dev, instruction[i]);
		if (retval < 0)
			return retval;
	}
	msleep(60);
	return retval;
}

static int jl2005c_stream_start_cif_small(struct gspca_dev *gspca_dev)
{
	int i;
	int retval = -1;
	static u8 instruction[][2] = {
		{0x06, 0x00},
		{0x7c, 0x00},
		{0x7d, 0x32},
		{0x02, 0x00},
		{0x01, 0x00},
		{0x04, 0x42},
	};

	for (i = 0; i < ARRAY_SIZE(instruction); i++) {
		msleep(60);
		retval = jl2005c_write2(gspca_dev, instruction[i]);
		if (retval < 0)
			return retval;
	}
	msleep(60);
	return retval;
}


static int jl2005c_stop(struct gspca_dev *gspca_dev)
{
	int retval;

	retval = jl2005c_write_reg(gspca_dev, 0x07, 0x00);
	return retval;
}

static void jl2005c_dostream(struct work_struct *work)
{
	struct sd *dev = container_of(work, struct sd, work_struct);
	struct gspca_dev *gspca_dev = &dev->gspca_dev;
	int bytes_left = 0; 
	int data_len;   
	int header_read = 0;
	unsigned char header_sig[2] = {0x4a, 0x4c};
	int act_len;
	int packet_type;
	int ret;
	u8 *buffer;

	buffer = kmalloc(JL2005C_MAX_TRANSFER, GFP_KERNEL | GFP_DMA);
	if (!buffer) {
		pr_err("Couldn't allocate USB buffer\n");
		goto quit_stream;
	}

	while (gspca_dev->present && gspca_dev->streaming) {
		
		if (!header_read) {
			mutex_lock(&gspca_dev->usb_lock);
			ret = jl2005c_start_new_frame(gspca_dev);
			mutex_unlock(&gspca_dev->usb_lock);
			if (ret < 0)
				goto quit_stream;
			ret = usb_bulk_msg(gspca_dev->dev,
				usb_rcvbulkpipe(gspca_dev->dev, 0x82),
				buffer, JL2005C_MAX_TRANSFER, &act_len,
				JL2005C_DATA_TIMEOUT);
			PDEBUG(D_PACK,
				"Got %d bytes out of %d for header",
					act_len, JL2005C_MAX_TRANSFER);
			if (ret < 0 || act_len < JL2005C_MAX_TRANSFER)
				goto quit_stream;
			
			if (memcmp(header_sig, buffer, 2) != 0) {
				pr_err("First block is not the first block\n");
				goto quit_stream;
			}
			bytes_left = buffer[0x07] * dev->block_size - act_len;
			PDEBUG(D_PACK, "bytes_left = 0x%x", bytes_left);
			
			packet_type = FIRST_PACKET;
			gspca_frame_add(gspca_dev, packet_type,
					buffer, act_len);
			header_read = 1;
		}
		while (bytes_left > 0 && gspca_dev->present) {
			data_len = bytes_left > JL2005C_MAX_TRANSFER ?
				JL2005C_MAX_TRANSFER : bytes_left;
			ret = usb_bulk_msg(gspca_dev->dev,
				usb_rcvbulkpipe(gspca_dev->dev, 0x82),
				buffer, data_len, &act_len,
				JL2005C_DATA_TIMEOUT);
			if (ret < 0 || act_len < data_len)
				goto quit_stream;
			PDEBUG(D_PACK,
				"Got %d bytes out of %d for frame",
						data_len, bytes_left);
			bytes_left -= data_len;
			if (bytes_left == 0) {
				packet_type = LAST_PACKET;
				header_read = 0;
			} else
				packet_type = INTER_PACKET;
			gspca_frame_add(gspca_dev, packet_type,
					buffer, data_len);
		}
	}
quit_stream:
	if (gspca_dev->present) {
		mutex_lock(&gspca_dev->usb_lock);
		jl2005c_stop(gspca_dev);
		mutex_unlock(&gspca_dev->usb_lock);
	}
	kfree(buffer);
}




static int sd_config(struct gspca_dev *gspca_dev,
			const struct usb_device_id *id)
{
	struct cam *cam;
	struct sd *sd = (struct sd *) gspca_dev;

	cam = &gspca_dev->cam;
	
	cam->bulk_size = 64;
	cam->bulk = 1;
	
	jl2005c_get_firmware_id(gspca_dev);
	if ((sd->firmware_id[0] & 0xf0) == 0x40) {
		cam->cam_mode	= cif_mode;
		cam->nmodes	= ARRAY_SIZE(cif_mode);
		sd->block_size	= 0x80;
	} else {
		cam->cam_mode	= vga_mode;
		cam->nmodes	= ARRAY_SIZE(vga_mode);
		sd->block_size	= 0x200;
	}

	INIT_WORK(&sd->work_struct, jl2005c_dostream);

	return 0;
}

static int sd_init(struct gspca_dev *gspca_dev)
{
	return 0;
}

static int sd_start(struct gspca_dev *gspca_dev)
{

	struct sd *sd = (struct sd *) gspca_dev;
	sd->cap_mode = gspca_dev->cam.cam_mode;

	switch (gspca_dev->width) {
	case 640:
		PDEBUG(D_STREAM, "Start streaming at vga resolution");
		jl2005c_stream_start_vga_lg(gspca_dev);
		break;
	case 320:
		PDEBUG(D_STREAM, "Start streaming at qvga resolution");
		jl2005c_stream_start_vga_small(gspca_dev);
		break;
	case 352:
		PDEBUG(D_STREAM, "Start streaming at cif resolution");
		jl2005c_stream_start_cif_lg(gspca_dev);
		break;
	case 176:
		PDEBUG(D_STREAM, "Start streaming at qcif resolution");
		jl2005c_stream_start_cif_small(gspca_dev);
		break;
	default:
		pr_err("Unknown resolution specified\n");
		return -1;
	}

	
	sd->work_thread = create_singlethread_workqueue(MODULE_NAME);
	queue_work(sd->work_thread, &sd->work_struct);

	return 0;
}

static void sd_stop0(struct gspca_dev *gspca_dev)
{
	struct sd *dev = (struct sd *) gspca_dev;

	
	mutex_unlock(&gspca_dev->usb_lock);
	
	destroy_workqueue(dev->work_thread);
	dev->work_thread = NULL;
	mutex_lock(&gspca_dev->usb_lock);
}



static const struct sd_desc sd_desc = {
	.name = MODULE_NAME,
	
	
	.config = sd_config,
	.init = sd_init,
	.start = sd_start,
	.stop0 = sd_stop0,
};

static const __devinitdata struct usb_device_id device_table[] = {
	{USB_DEVICE(0x0979, 0x0227)},
	{}
};
MODULE_DEVICE_TABLE(usb, device_table);

static int sd_probe(struct usb_interface *intf,
				const struct usb_device_id *id)
{
	return gspca_dev_probe(intf, id, &sd_desc, sizeof(struct sd),
				THIS_MODULE);
}

static struct usb_driver sd_driver = {
	.name = MODULE_NAME,
	.id_table = device_table,
	.probe = sd_probe,
	.disconnect = gspca_disconnect,
#ifdef CONFIG_PM
	.suspend = gspca_suspend,
	.resume = gspca_resume,
#endif
};

static int __init sd_mod_init(void)
{
	int ret;

	ret = usb_register(&sd_driver);
	if (ret < 0)
		return ret;
	return 0;
}
static void __exit sd_mod_exit(void)
{
	usb_deregister(&sd_driver);
}

module_init(sd_mod_init);
module_exit(sd_mod_exit);
