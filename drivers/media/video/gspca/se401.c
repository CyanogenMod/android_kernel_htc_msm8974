/*
 * GSPCA Endpoints (formerly known as AOX) se401 USB Camera sub Driver
 *
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 *
 * Based on the v4l1 se401 driver which is:
 *
 * Copyright (c) 2000 Jeroen B. Vreeken (pe1rxq@amsat.org)
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define MODULE_NAME "se401"

#define BULK_SIZE 4096
#define PACKET_SIZE 1024
#define READ_REQ_SIZE 64
#define MAX_MODES ((READ_REQ_SIZE - 6) / 4)
#define SE401_QUANT_FACT 8

#include <linux/input.h>
#include <linux/slab.h>
#include "gspca.h"
#include "se401.h"

MODULE_AUTHOR("Hans de Goede <hdegoede@redhat.com>");
MODULE_DESCRIPTION("Endpoints se401");
MODULE_LICENSE("GPL");

enum e_ctrl {
	BRIGHTNESS,
	GAIN,
	EXPOSURE,
	FREQ,
	NCTRL	
};

enum {
	EXPO_CHANGED,
	EXPO_DROP_FRAME,
	EXPO_NO_CHANGE,
};

struct sd {
	struct gspca_dev gspca_dev;	
	struct gspca_ctrl ctrls[NCTRL];
	struct v4l2_pix_format fmts[MAX_MODES];
	int pixels_read;
	int packet_read;
	u8 packet[PACKET_SIZE];
	u8 restart_stream;
	u8 button_state;
	u8 resetlevel;
	u8 resetlevel_frame_count;
	int resetlevel_adjust_dir;
	int expo_change_state;
};

static void setbrightness(struct gspca_dev *gspca_dev);
static void setgain(struct gspca_dev *gspca_dev);
static void setexposure(struct gspca_dev *gspca_dev);

static const struct ctrl sd_ctrls[NCTRL] = {
[BRIGHTNESS] = {
		{
			.id      = V4L2_CID_BRIGHTNESS,
			.type    = V4L2_CTRL_TYPE_INTEGER,
			.name    = "Brightness",
			.minimum = 0,
			.maximum = 255,
			.step    = 1,
			.default_value = 15,
		},
		.set_control = setbrightness
	},
[GAIN] = {
		{
			.id      = V4L2_CID_GAIN,
			.type    = V4L2_CTRL_TYPE_INTEGER,
			.name    = "Gain",
			.minimum = 0,
			.maximum = 50, 
			.step    = 1,
			.default_value = 25,
		},
		.set_control = setgain
	},
[EXPOSURE] = {
		{
			.id = V4L2_CID_EXPOSURE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Exposure",
			.minimum = 0,
			.maximum = 32767,
			.step = 1,
			.default_value = 15000,
		},
		.set_control = setexposure
	},
[FREQ] = {
		{
			.id	 = V4L2_CID_POWER_LINE_FREQUENCY,
			.type    = V4L2_CTRL_TYPE_MENU,
			.name    = "Light frequency filter",
			.minimum = 0,
			.maximum = 2,
			.step    = 1,
			.default_value = 0,
		},
		.set_control = setexposure
	},
};

static void se401_write_req(struct gspca_dev *gspca_dev, u16 req, u16 value,
			    int silent)
{
	int err;

	if (gspca_dev->usb_err < 0)
		return;

	err = usb_control_msg(gspca_dev->dev,
			      usb_sndctrlpipe(gspca_dev->dev, 0), req,
			      USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			      value, 0, NULL, 0, 1000);
	if (err < 0) {
		if (!silent)
			pr_err("write req failed req %#04x val %#04x error %d\n",
			       req, value, err);
		gspca_dev->usb_err = err;
	}
}

static void se401_read_req(struct gspca_dev *gspca_dev, u16 req, int silent)
{
	int err;

	if (gspca_dev->usb_err < 0)
		return;

	if (USB_BUF_SZ < READ_REQ_SIZE) {
		pr_err("USB_BUF_SZ too small!!\n");
		gspca_dev->usb_err = -ENOBUFS;
		return;
	}

	err = usb_control_msg(gspca_dev->dev,
			      usb_rcvctrlpipe(gspca_dev->dev, 0), req,
			      USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			      0, 0, gspca_dev->usb_buf, READ_REQ_SIZE, 1000);
	if (err < 0) {
		if (!silent)
			pr_err("read req failed req %#04x error %d\n",
			       req, err);
		gspca_dev->usb_err = err;
	}
}

static void se401_set_feature(struct gspca_dev *gspca_dev,
			      u16 selector, u16 param)
{
	int err;

	if (gspca_dev->usb_err < 0)
		return;

	err = usb_control_msg(gspca_dev->dev,
			      usb_sndctrlpipe(gspca_dev->dev, 0),
			      SE401_REQ_SET_EXT_FEATURE,
			      USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			      param, selector, NULL, 0, 1000);
	if (err < 0) {
		pr_err("set feature failed sel %#04x param %#04x error %d\n",
		       selector, param, err);
		gspca_dev->usb_err = err;
	}
}

static int se401_get_feature(struct gspca_dev *gspca_dev, u16 selector)
{
	int err;

	if (gspca_dev->usb_err < 0)
		return gspca_dev->usb_err;

	if (USB_BUF_SZ < 2) {
		pr_err("USB_BUF_SZ too small!!\n");
		gspca_dev->usb_err = -ENOBUFS;
		return gspca_dev->usb_err;
	}

	err = usb_control_msg(gspca_dev->dev,
			      usb_rcvctrlpipe(gspca_dev->dev, 0),
			      SE401_REQ_GET_EXT_FEATURE,
			      USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			      0, selector, gspca_dev->usb_buf, 2, 1000);
	if (err < 0) {
		pr_err("get feature failed sel %#04x error %d\n",
		       selector, err);
		gspca_dev->usb_err = err;
		return err;
	}
	return gspca_dev->usb_buf[0] | (gspca_dev->usb_buf[1] << 8);
}

static void setbrightness(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (gspca_dev->ctrl_dis & (1 << BRIGHTNESS))
		return;

	
	se401_write_req(gspca_dev, SE401_REQ_SET_BRT,
			sd->ctrls[BRIGHTNESS].val, 0);
}

static void setgain(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	u16 gain = 63 - sd->ctrls[GAIN].val;

	
	se401_set_feature(gspca_dev, HV7131_REG_ARCG, gain);
	
	se401_set_feature(gspca_dev, HV7131_REG_AGCG, gain);
	
	se401_set_feature(gspca_dev, HV7131_REG_ABCG, gain);
}

static void setexposure(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int integration = sd->ctrls[EXPOSURE].val << 6;
	u8 expose_h, expose_m, expose_l;

	sd->expo_change_state = EXPO_CHANGED;

	if (sd->ctrls[FREQ].val == V4L2_CID_POWER_LINE_FREQUENCY_50HZ)
		integration = integration - integration % 106667;
	if (sd->ctrls[FREQ].val == V4L2_CID_POWER_LINE_FREQUENCY_60HZ)
		integration = integration - integration % 88889;

	expose_h = (integration >> 16);
	expose_m = (integration >> 8);
	expose_l = integration;

	
	se401_set_feature(gspca_dev, HV7131_REG_TITL, expose_l);
	
	se401_set_feature(gspca_dev, HV7131_REG_TITM, expose_m);
	
	se401_set_feature(gspca_dev, HV7131_REG_TITU, expose_h);
}

static int sd_config(struct gspca_dev *gspca_dev,
			const struct usb_device_id *id)
{
	struct sd *sd = (struct sd *)gspca_dev;
	struct cam *cam = &gspca_dev->cam;
	u8 *cd = gspca_dev->usb_buf;
	int i, j, n;
	int widths[MAX_MODES], heights[MAX_MODES];

	
	se401_read_req(gspca_dev, SE401_REQ_GET_CAMERA_DESCRIPTOR, 1);
	if (gspca_dev->usb_err) {
		usb_reset_device(gspca_dev->dev);
		gspca_dev->usb_err = 0;
		se401_read_req(gspca_dev, SE401_REQ_GET_CAMERA_DESCRIPTOR, 0);
	}

	
	se401_write_req(gspca_dev, SE401_REQ_LED_CONTROL, 0, 0);
	if (gspca_dev->usb_err)
		return gspca_dev->usb_err;

	if (cd[1] != 0x41) {
		pr_err("Wrong descriptor type\n");
		return -ENODEV;
	}

	if (!(cd[2] & SE401_FORMAT_BAYER)) {
		pr_err("Bayer format not supported!\n");
		return -ENODEV;
	}

	if (cd[3])
		pr_info("ExtraFeatures: %d\n", cd[3]);

	n = cd[4] | (cd[5] << 8);
	if (n > MAX_MODES) {
		pr_err("Too many frame sizes\n");
		return -ENODEV;
	}

	for (i = 0; i < n ; i++) {
		widths[i] = cd[6 + i * 4 + 0] | (cd[6 + i * 4 + 1] << 8);
		heights[i] = cd[6 + i * 4 + 2] | (cd[6 + i * 4 + 3] << 8);
	}

	for (i = 0; i < n ; i++) {
		sd->fmts[i].width = widths[i];
		sd->fmts[i].height = heights[i];
		sd->fmts[i].field = V4L2_FIELD_NONE;
		sd->fmts[i].colorspace = V4L2_COLORSPACE_SRGB;
		sd->fmts[i].priv = 1;

		
		for (j = 0; j < n; j++) {
			if (widths[j] / 2 == widths[i] &&
			    heights[j] / 2 == heights[i]) {
				sd->fmts[i].priv = 2;
				break;
			}
		}
		for (j = 0; j < n; j++) {
			if (widths[j] / 4 == widths[i] &&
			    heights[j] / 4 == heights[i]) {
				sd->fmts[i].priv = 4;
				break;
			}
		}

		if (sd->fmts[i].priv == 1) {
			
			sd->fmts[i].pixelformat = V4L2_PIX_FMT_SBGGR8;
			sd->fmts[i].bytesperline = widths[i];
			sd->fmts[i].sizeimage = widths[i] * heights[i];
			pr_info("Frame size: %dx%d bayer\n",
				widths[i], heights[i]);
		} else {
			
			sd->fmts[i].pixelformat = V4L2_PIX_FMT_SE401;
			sd->fmts[i].bytesperline = 0;
			sd->fmts[i].sizeimage = widths[i] * heights[i] * 3;
			pr_info("Frame size: %dx%d 1/%dth janggu\n",
				widths[i], heights[i],
				sd->fmts[i].priv * sd->fmts[i].priv);
		}
	}

	cam->cam_mode = sd->fmts;
	cam->nmodes = n;
	cam->bulk = 1;
	cam->bulk_size = BULK_SIZE;
	cam->bulk_nurbs = 4;
	cam->ctrls = sd->ctrls;
	sd->resetlevel = 0x2d; 

	
	se401_read_req(gspca_dev, SE401_REQ_GET_BRT, 1);
	if (gspca_dev->usb_err) {
		gspca_dev->ctrl_dis = (1 << BRIGHTNESS);
		gspca_dev->usb_err = 0;
	}

	return 0;
}

static int sd_init(struct gspca_dev *gspca_dev)
{
	return 0;
}

static int sd_isoc_init(struct gspca_dev *gspca_dev)
{
	gspca_dev->alt = 1;	

	return gspca_dev->usb_err;
}

static int sd_start(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *)gspca_dev;
	int mult = gspca_dev->cam.cam_mode[gspca_dev->curr_mode].priv;
	int mode = 0;

	se401_write_req(gspca_dev, SE401_REQ_CAMERA_POWER, 1, 1);
	if (gspca_dev->usb_err) {
		usb_reset_device(gspca_dev->dev);
		gspca_dev->usb_err = 0;
		se401_write_req(gspca_dev, SE401_REQ_CAMERA_POWER, 1, 0);
	}
	se401_write_req(gspca_dev, SE401_REQ_LED_CONTROL, 1, 0);

	se401_set_feature(gspca_dev, HV7131_REG_MODE_B, 0x05);

	
	se401_write_req(gspca_dev, SE401_REQ_SET_WIDTH,
			gspca_dev->width * mult, 0);
	se401_write_req(gspca_dev, SE401_REQ_SET_HEIGHT,
			gspca_dev->height * mult, 0);

	switch (mult) {
	case 1: 
		mode = 0x03; break;
	case 2: 
		mode = SE401_QUANT_FACT << 4; break;
	case 4: 
		mode = (SE401_QUANT_FACT << 4) | 0x02; break;
	}
	se401_set_feature(gspca_dev, SE401_OPERATINGMODE, mode);

	setbrightness(gspca_dev);
	setgain(gspca_dev);
	setexposure(gspca_dev);
	se401_set_feature(gspca_dev, HV7131_REG_ARLV, sd->resetlevel);

	sd->packet_read = 0;
	sd->pixels_read = 0;
	sd->restart_stream = 0;
	sd->resetlevel_frame_count = 0;
	sd->resetlevel_adjust_dir = 0;
	sd->expo_change_state = EXPO_NO_CHANGE;

	se401_write_req(gspca_dev, SE401_REQ_START_CONTINUOUS_CAPTURE, 0, 0);

	return gspca_dev->usb_err;
}

static void sd_stopN(struct gspca_dev *gspca_dev)
{
	se401_write_req(gspca_dev, SE401_REQ_STOP_CONTINUOUS_CAPTURE, 0, 0);
	se401_write_req(gspca_dev, SE401_REQ_LED_CONTROL, 0, 0);
	se401_write_req(gspca_dev, SE401_REQ_CAMERA_POWER, 0, 0);
}

static void sd_dq_callback(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *)gspca_dev;
	unsigned int ahrc, alrc;
	int oldreset, adjust_dir;

	
	if (sd->restart_stream) {
		sd_stopN(gspca_dev);
		sd_start(gspca_dev);
		sd->restart_stream = 0;
	}

	sd->resetlevel_frame_count++;
	if (sd->resetlevel_frame_count < 20)
		return;

	se401_get_feature(gspca_dev, HV7131_REG_HIREFNOH);
	se401_get_feature(gspca_dev, HV7131_REG_HIREFNOL);
	se401_get_feature(gspca_dev, HV7131_REG_LOREFNOH);
	se401_get_feature(gspca_dev, HV7131_REG_LOREFNOL);
	ahrc = 256*se401_get_feature(gspca_dev, HV7131_REG_HIREFNOH) +
	    se401_get_feature(gspca_dev, HV7131_REG_HIREFNOL);
	alrc = 256*se401_get_feature(gspca_dev, HV7131_REG_LOREFNOH) +
	    se401_get_feature(gspca_dev, HV7131_REG_LOREFNOL);

	
	oldreset = sd->resetlevel;
	if (alrc > 10) {
		while (alrc >= 10 && sd->resetlevel < 63) {
			sd->resetlevel++;
			alrc /= 2;
		}
	} else if (ahrc > 20) {
		while (ahrc >= 20 && sd->resetlevel > 0) {
			sd->resetlevel--;
			ahrc /= 2;
		}
	}
	
	if (sd->resetlevel > oldreset)
		adjust_dir = 1;
	else
		adjust_dir = -1;
	if (sd->resetlevel_adjust_dir &&
	    sd->resetlevel_adjust_dir != adjust_dir)
		sd->resetlevel = oldreset + (sd->resetlevel - oldreset) / 2;

	if (sd->resetlevel != oldreset) {
		sd->resetlevel_adjust_dir = adjust_dir;
		se401_set_feature(gspca_dev, HV7131_REG_ARLV, sd->resetlevel);
	}

	sd->resetlevel_frame_count = 0;
}

static void sd_complete_frame(struct gspca_dev *gspca_dev, u8 *data, int len)
{
	struct sd *sd = (struct sd *)gspca_dev;

	switch (sd->expo_change_state) {
	case EXPO_CHANGED:
		sd->expo_change_state = EXPO_DROP_FRAME;
		break;
	case EXPO_DROP_FRAME:
		gspca_dev->last_packet_type = DISCARD_PACKET;
		sd->expo_change_state = EXPO_NO_CHANGE;
		break;
	case EXPO_NO_CHANGE:
		break;
	}
	gspca_frame_add(gspca_dev, LAST_PACKET, data, len);
}

static void sd_pkt_scan_janggu(struct gspca_dev *gspca_dev, u8 *data, int len)
{
	struct sd *sd = (struct sd *)gspca_dev;
	int imagesize = gspca_dev->width * gspca_dev->height;
	int i, plen, bits, pixels, info, count;

	if (sd->restart_stream)
		return;

	
	if (gspca_dev->last_packet_type == LAST_PACKET && len == 1024) {
		gspca_dev->last_packet_type = DISCARD_PACKET;
		return;
	}

	i = 0;
	while (i < len) {
		
		if (sd->packet_read < 4) {
			count = 4 - sd->packet_read;
			if (count > len - i)
				count = len - i;
			memcpy(&sd->packet[sd->packet_read], &data[i], count);
			sd->packet_read += count;
			i += count;
			if (sd->packet_read < 4)
				break;
		}
		bits   = sd->packet[3] + (sd->packet[2] << 8);
		pixels = sd->packet[1] + ((sd->packet[0] & 0x3f) << 8);
		info   = (sd->packet[0] & 0xc0) >> 6;
		plen   = ((bits + 47) >> 4) << 1;
		
		if (plen > 1024) {
			pr_err("invalid packet len %d restarting stream\n",
			       plen);
			goto error;
		}
		if (info == 3) {
			pr_err("unknown frame info value restarting stream\n");
			goto error;
		}

		
		count = plen - sd->packet_read;
		if (count > len - i)
			count = len - i;
		memcpy(&sd->packet[sd->packet_read], &data[i], count);
		sd->packet_read += count;
		i += count;
		if (sd->packet_read < plen)
			break;

		sd->pixels_read += pixels;
		sd->packet_read = 0;

		switch (info) {
		case 0: 
			gspca_frame_add(gspca_dev, INTER_PACKET, sd->packet,
					plen);
			break;
		case 1: 
			if (sd->pixels_read != imagesize) {
				pr_err("frame size %d expected %d\n",
				       sd->pixels_read, imagesize);
				goto error;
			}
			sd_complete_frame(gspca_dev, sd->packet, plen);
			return; 
		case 2: 
			gspca_frame_add(gspca_dev, FIRST_PACKET, sd->packet,
					plen);
			sd->pixels_read = pixels;
			break;
		}
	}
	return;

error:
	sd->restart_stream = 1;
	gspca_frame_add(gspca_dev, FIRST_PACKET, NULL, 0);
	gspca_frame_add(gspca_dev, LAST_PACKET, NULL, 0);
}

static void sd_pkt_scan_bayer(struct gspca_dev *gspca_dev, u8 *data, int len)
{
	struct cam *cam = &gspca_dev->cam;
	int imagesize = cam->cam_mode[gspca_dev->curr_mode].sizeimage;

	if (gspca_dev->image_len == 0) {
		gspca_frame_add(gspca_dev, FIRST_PACKET, data, len);
		return;
	}

	if (gspca_dev->image_len + len >= imagesize) {
		sd_complete_frame(gspca_dev, data, len);
		return;
	}

	gspca_frame_add(gspca_dev, INTER_PACKET, data, len);
}

static void sd_pkt_scan(struct gspca_dev *gspca_dev, u8 *data, int len)
{
	int mult = gspca_dev->cam.cam_mode[gspca_dev->curr_mode].priv;

	if (len == 0)
		return;

	if (mult == 1) 
		sd_pkt_scan_bayer(gspca_dev, data, len);
	else
		sd_pkt_scan_janggu(gspca_dev, data, len);
}

static int sd_querymenu(struct gspca_dev *gspca_dev,
			struct v4l2_querymenu *menu)
{
	switch (menu->id) {
	case V4L2_CID_POWER_LINE_FREQUENCY:
		switch (menu->index) {
		case V4L2_CID_POWER_LINE_FREQUENCY_DISABLED:
			strcpy((char *) menu->name, "NoFliker");
			return 0;
		case V4L2_CID_POWER_LINE_FREQUENCY_50HZ:
			strcpy((char *) menu->name, "50 Hz");
			return 0;
		case V4L2_CID_POWER_LINE_FREQUENCY_60HZ:
			strcpy((char *) menu->name, "60 Hz");
			return 0;
		}
		break;
	}
	return -EINVAL;
}

#if defined(CONFIG_INPUT) || defined(CONFIG_INPUT_MODULE)
static int sd_int_pkt_scan(struct gspca_dev *gspca_dev, u8 *data, int len)
{
	struct sd *sd = (struct sd *)gspca_dev;
	u8 state;

	if (len != 2)
		return -EINVAL;

	switch (data[0]) {
	case 0:
	case 1:
		state = data[0];
		break;
	default:
		return -EINVAL;
	}
	if (sd->button_state != state) {
		input_report_key(gspca_dev->input_dev, KEY_CAMERA, state);
		input_sync(gspca_dev->input_dev);
		sd->button_state = state;
	}

	return 0;
}
#endif

static const struct sd_desc sd_desc = {
	.name = MODULE_NAME,
	.ctrls = sd_ctrls,
	.nctrls = ARRAY_SIZE(sd_ctrls),
	.config = sd_config,
	.init = sd_init,
	.isoc_init = sd_isoc_init,
	.start = sd_start,
	.stopN = sd_stopN,
	.dq_callback = sd_dq_callback,
	.pkt_scan = sd_pkt_scan,
	.querymenu = sd_querymenu,
#if defined(CONFIG_INPUT) || defined(CONFIG_INPUT_MODULE)
	.int_pkt_scan = sd_int_pkt_scan,
#endif
};

static const struct usb_device_id device_table[] = {
	{USB_DEVICE(0x03e8, 0x0004)}, 
	{USB_DEVICE(0x0471, 0x030b)}, 
	{USB_DEVICE(0x047d, 0x5001)}, 
	{USB_DEVICE(0x047d, 0x5002)}, 
	{USB_DEVICE(0x047d, 0x5003)}, 
	{}
};
MODULE_DEVICE_TABLE(usb, device_table);

static int sd_probe(struct usb_interface *intf,
			const struct usb_device_id *id)
{
	return gspca_dev_probe(intf, id, &sd_desc, sizeof(struct sd),
				THIS_MODULE);
}

static int sd_pre_reset(struct usb_interface *intf)
{
	return 0;
}

static int sd_post_reset(struct usb_interface *intf)
{
	return 0;
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
	.pre_reset = sd_pre_reset,
	.post_reset = sd_post_reset,
};

module_usb_driver(sd_driver);
