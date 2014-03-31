/**
 *
 * GSPCA sub driver for W996[78]CF JPEG USB Dual Mode Camera Chip.
 *
 * Copyright (C) 2009 Hans de Goede <hdegoede@redhat.com>
 *
 * This module is adapted from the in kernel v4l1 w9968cf driver:
 *
 * Copyright (C) 2002-2004 by Luca Risolia <luca.risolia@studio.unibo.it>
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
 *
 */


#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define W9968CF_I2C_BUS_DELAY    4 

#define Y_QUANTABLE (&sd->jpeg_hdr[JPEG_QT0_OFFSET])
#define UV_QUANTABLE (&sd->jpeg_hdr[JPEG_QT1_OFFSET])

static const struct v4l2_pix_format w9968cf_vga_mode[] = {
	{160, 120, V4L2_PIX_FMT_UYVY, V4L2_FIELD_NONE,
		.bytesperline = 160 * 2,
		.sizeimage = 160 * 120 * 2,
		.colorspace = V4L2_COLORSPACE_JPEG},
	{176, 144, V4L2_PIX_FMT_UYVY, V4L2_FIELD_NONE,
		.bytesperline = 176 * 2,
		.sizeimage = 176 * 144 * 2,
		.colorspace = V4L2_COLORSPACE_JPEG},
	{320, 240, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 320 * 2,
		.sizeimage = 320 * 240 * 2,
		.colorspace = V4L2_COLORSPACE_JPEG},
	{352, 288, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 352 * 2,
		.sizeimage = 352 * 288 * 2,
		.colorspace = V4L2_COLORSPACE_JPEG},
	{640, 480, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 640 * 2,
		.sizeimage = 640 * 480 * 2,
		.colorspace = V4L2_COLORSPACE_JPEG},
};

static void reg_w(struct sd *sd, u16 index, u16 value);

static void w9968cf_write_fsb(struct sd *sd, u16* data)
{
	struct usb_device *udev = sd->gspca_dev.dev;
	u16 value;
	int ret;

	if (sd->gspca_dev.usb_err < 0)
		return;

	value = *data++;
	memcpy(sd->gspca_dev.usb_buf, data, 6);

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0,
			      USB_TYPE_VENDOR | USB_DIR_OUT | USB_RECIP_DEVICE,
			      value, 0x06, sd->gspca_dev.usb_buf, 6, 500);
	if (ret < 0) {
		pr_err("Write FSB registers failed (%d)\n", ret);
		sd->gspca_dev.usb_err = ret;
	}
}

static void w9968cf_write_sb(struct sd *sd, u16 value)
{
	int ret;

	if (sd->gspca_dev.usb_err < 0)
		return;

	ret = usb_control_msg(sd->gspca_dev.dev,
		usb_sndctrlpipe(sd->gspca_dev.dev, 0),
		0,
		USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
		value, 0x01, NULL, 0, 500);

	udelay(W9968CF_I2C_BUS_DELAY);

	if (ret < 0) {
		pr_err("Write SB reg [01] %04x failed\n", value);
		sd->gspca_dev.usb_err = ret;
	}
}

static int w9968cf_read_sb(struct sd *sd)
{
	int ret;

	if (sd->gspca_dev.usb_err < 0)
		return -1;

	ret = usb_control_msg(sd->gspca_dev.dev,
			usb_rcvctrlpipe(sd->gspca_dev.dev, 0),
			1,
			USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			0, 0x01, sd->gspca_dev.usb_buf, 2, 500);
	if (ret >= 0) {
		ret = sd->gspca_dev.usb_buf[0] |
		      (sd->gspca_dev.usb_buf[1] << 8);
	} else {
		pr_err("Read SB reg [01] failed\n");
		sd->gspca_dev.usb_err = ret;
	}

	udelay(W9968CF_I2C_BUS_DELAY);

	return ret;
}

static void w9968cf_upload_quantizationtables(struct sd *sd)
{
	u16 a, b;
	int i, j;

	reg_w(sd, 0x39, 0x0010); 

	for (i = 0, j = 0; i < 32; i++, j += 2) {
		a = Y_QUANTABLE[j] | ((unsigned)(Y_QUANTABLE[j + 1]) << 8);
		b = UV_QUANTABLE[j] | ((unsigned)(UV_QUANTABLE[j + 1]) << 8);
		reg_w(sd, 0x40 + i, a);
		reg_w(sd, 0x60 + i, b);
	}
	reg_w(sd, 0x39, 0x0012); 
}


static void w9968cf_smbus_start(struct sd *sd)
{
	w9968cf_write_sb(sd, 0x0011); 
	w9968cf_write_sb(sd, 0x0010); 
}

static void w9968cf_smbus_stop(struct sd *sd)
{
	w9968cf_write_sb(sd, 0x0010); 
	w9968cf_write_sb(sd, 0x0011); 
	w9968cf_write_sb(sd, 0x0013); 
}

static void w9968cf_smbus_write_byte(struct sd *sd, u8 v)
{
	u8 bit;
	int sda;

	for (bit = 0 ; bit < 8 ; bit++) {
		sda = (v & 0x80) ? 2 : 0;
		v <<= 1;
		
		w9968cf_write_sb(sd, 0x10 | sda);
		
		w9968cf_write_sb(sd, 0x11 | sda);
		
		w9968cf_write_sb(sd, 0x10 | sda);
	}
}

static void w9968cf_smbus_read_byte(struct sd *sd, u8 *v)
{
	u8 bit;

	*v = 0;
	for (bit = 0 ; bit < 8 ; bit++) {
		*v <<= 1;
		
		w9968cf_write_sb(sd, 0x0013);
		*v |= (w9968cf_read_sb(sd) & 0x0008) ? 1 : 0;
		
		w9968cf_write_sb(sd, 0x0012);
	}
}

static void w9968cf_smbus_write_nack(struct sd *sd)
{
	w9968cf_write_sb(sd, 0x0013); 
	w9968cf_write_sb(sd, 0x0012); 
}

static void w9968cf_smbus_read_ack(struct sd *sd)
{
	int sda;

	
	w9968cf_write_sb(sd, 0x0012); 
	w9968cf_write_sb(sd, 0x0013); 
	sda = w9968cf_read_sb(sd);
	w9968cf_write_sb(sd, 0x0012); 
	if (sda >= 0 && (sda & 0x08)) {
		PDEBUG(D_USBI, "Did not receive i2c ACK");
		sd->gspca_dev.usb_err = -EIO;
	}
}

static void w9968cf_i2c_w(struct sd *sd, u8 reg, u8 value)
{
	u16* data = (u16 *)sd->gspca_dev.usb_buf;

	data[0] = 0x082f | ((sd->sensor_addr & 0x80) ? 0x1500 : 0x0);
	data[0] |= (sd->sensor_addr & 0x40) ? 0x4000 : 0x0;
	data[1] = 0x2082 | ((sd->sensor_addr & 0x40) ? 0x0005 : 0x0);
	data[1] |= (sd->sensor_addr & 0x20) ? 0x0150 : 0x0;
	data[1] |= (sd->sensor_addr & 0x10) ? 0x5400 : 0x0;
	data[2] = 0x8208 | ((sd->sensor_addr & 0x08) ? 0x0015 : 0x0);
	data[2] |= (sd->sensor_addr & 0x04) ? 0x0540 : 0x0;
	data[2] |= (sd->sensor_addr & 0x02) ? 0x5000 : 0x0;
	data[3] = 0x1d20 | ((sd->sensor_addr & 0x02) ? 0x0001 : 0x0);
	data[3] |= (sd->sensor_addr & 0x01) ? 0x0054 : 0x0;

	w9968cf_write_fsb(sd, data);

	data[0] = 0x8208 | ((reg & 0x80) ? 0x0015 : 0x0);
	data[0] |= (reg & 0x40) ? 0x0540 : 0x0;
	data[0] |= (reg & 0x20) ? 0x5000 : 0x0;
	data[1] = 0x0820 | ((reg & 0x20) ? 0x0001 : 0x0);
	data[1] |= (reg & 0x10) ? 0x0054 : 0x0;
	data[1] |= (reg & 0x08) ? 0x1500 : 0x0;
	data[1] |= (reg & 0x04) ? 0x4000 : 0x0;
	data[2] = 0x2082 | ((reg & 0x04) ? 0x0005 : 0x0);
	data[2] |= (reg & 0x02) ? 0x0150 : 0x0;
	data[2] |= (reg & 0x01) ? 0x5400 : 0x0;
	data[3] = 0x001d;

	w9968cf_write_fsb(sd, data);

	data[0] = 0x8208 | ((value & 0x80) ? 0x0015 : 0x0);
	data[0] |= (value & 0x40) ? 0x0540 : 0x0;
	data[0] |= (value & 0x20) ? 0x5000 : 0x0;
	data[1] = 0x0820 | ((value & 0x20) ? 0x0001 : 0x0);
	data[1] |= (value & 0x10) ? 0x0054 : 0x0;
	data[1] |= (value & 0x08) ? 0x1500 : 0x0;
	data[1] |= (value & 0x04) ? 0x4000 : 0x0;
	data[2] = 0x2082 | ((value & 0x04) ? 0x0005 : 0x0);
	data[2] |= (value & 0x02) ? 0x0150 : 0x0;
	data[2] |= (value & 0x01) ? 0x5400 : 0x0;
	data[3] = 0xfe1d;

	w9968cf_write_fsb(sd, data);

	PDEBUG(D_USBO, "i2c 0x%02x -> [0x%02x]", value, reg);
}

static int w9968cf_i2c_r(struct sd *sd, u8 reg)
{
	int ret = 0;
	u8 value;

	
	w9968cf_write_sb(sd, 0x0013); 

	w9968cf_smbus_start(sd);
	w9968cf_smbus_write_byte(sd, sd->sensor_addr);
	w9968cf_smbus_read_ack(sd);
	w9968cf_smbus_write_byte(sd, reg);
	w9968cf_smbus_read_ack(sd);
	w9968cf_smbus_stop(sd);
	w9968cf_smbus_start(sd);
	w9968cf_smbus_write_byte(sd, sd->sensor_addr + 1);
	w9968cf_smbus_read_ack(sd);
	w9968cf_smbus_read_byte(sd, &value);
	w9968cf_smbus_write_nack(sd);
	w9968cf_smbus_stop(sd);

	
	w9968cf_write_sb(sd, 0x0030);

	if (sd->gspca_dev.usb_err >= 0) {
		ret = value;
		PDEBUG(D_USBI, "i2c [0x%02X] -> 0x%02X", reg, value);
	} else
		PDEBUG(D_ERR, "i2c read [0x%02x] failed", reg);

	return ret;
}

static void w9968cf_configure(struct sd *sd)
{
	reg_w(sd, 0x00, 0xff00); 
	reg_w(sd, 0x00, 0xbf17); 
	reg_w(sd, 0x00, 0xbf10); 
	reg_w(sd, 0x01, 0x0010); 
	reg_w(sd, 0x01, 0x0000); 
	reg_w(sd, 0x01, 0x0010); 
	reg_w(sd, 0x01, 0x0030); 

	sd->stopped = 1;
}

static void w9968cf_init(struct sd *sd)
{
	unsigned long hw_bufsize = sd->sif ? (352 * 288 * 2) : (640 * 480 * 2),
		      y0 = 0x0000,
		      u0 = y0 + hw_bufsize / 2,
		      v0 = u0 + hw_bufsize / 4,
		      y1 = v0 + hw_bufsize / 4,
		      u1 = y1 + hw_bufsize / 2,
		      v1 = u1 + hw_bufsize / 4;

	reg_w(sd, 0x00, 0xff00); 
	reg_w(sd, 0x00, 0xbf10); 

	reg_w(sd, 0x03, 0x405d); 
	reg_w(sd, 0x04, 0x0030); 

	reg_w(sd, 0x20, y0 & 0xffff); 
	reg_w(sd, 0x21, y0 >> 16);    
	reg_w(sd, 0x24, u0 & 0xffff); 
	reg_w(sd, 0x25, u0 >> 16);    
	reg_w(sd, 0x28, v0 & 0xffff); 
	reg_w(sd, 0x29, v0 >> 16);    

	reg_w(sd, 0x22, y1 & 0xffff); 
	reg_w(sd, 0x23, y1 >> 16);    
	reg_w(sd, 0x26, u1 & 0xffff); 
	reg_w(sd, 0x27, u1 >> 16);    
	reg_w(sd, 0x2a, v1 & 0xffff); 
	reg_w(sd, 0x2b, v1 >> 16);    

	reg_w(sd, 0x32, y1 & 0xffff); 
	reg_w(sd, 0x33, y1 >> 16);    

	reg_w(sd, 0x34, y1 & 0xffff); 
	reg_w(sd, 0x35, y1 >> 16);    

	reg_w(sd, 0x36, 0x0000);
	reg_w(sd, 0x37, 0x0804);
	reg_w(sd, 0x38, 0x0000);
	reg_w(sd, 0x3f, 0x0000); 
}

static void w9968cf_set_crop_window(struct sd *sd)
{
	int start_cropx, start_cropy,  x, y, fw, fh, cw, ch,
	    max_width, max_height;

	if (sd->sif) {
		max_width  = 352;
		max_height = 288;
	} else {
		max_width  = 640;
		max_height = 480;
	}

	if (sd->sensor == SEN_OV7620) {
		if (sd->ctrls[FREQ].val == 1) {
			start_cropx = 277;
			start_cropy = 37;
		} else {
			start_cropx = 105;
			start_cropy = 37;
		}
	} else {
		start_cropx = 320;
		start_cropy = 35;
	}

	
	#define SC(x) ((x) << 10)

	
	fw = SC(sd->gspca_dev.width) / max_width;
	fh = SC(sd->gspca_dev.height) / max_height;

	cw = (fw >= fh) ? max_width : SC(sd->gspca_dev.width) / fh;
	ch = (fw >= fh) ? SC(sd->gspca_dev.height) / fw : max_height;

	sd->sensor_width = max_width;
	sd->sensor_height = max_height;

	x = (max_width - cw) / 2;
	y = (max_height - ch) / 2;

	reg_w(sd, 0x10, start_cropx + x);
	reg_w(sd, 0x11, start_cropy + y);
	reg_w(sd, 0x12, start_cropx + x + cw);
	reg_w(sd, 0x13, start_cropy + y + ch);
}

static void w9968cf_mode_init_regs(struct sd *sd)
{
	int val, vs_polarity, hs_polarity;

	w9968cf_set_crop_window(sd);

	reg_w(sd, 0x14, sd->gspca_dev.width);
	reg_w(sd, 0x15, sd->gspca_dev.height);

	
	reg_w(sd, 0x30, sd->gspca_dev.width);
	reg_w(sd, 0x31, sd->gspca_dev.height);

	
	if (w9968cf_vga_mode[sd->gspca_dev.curr_mode].pixelformat ==
	    V4L2_PIX_FMT_JPEG) {
		reg_w(sd, 0x2c, sd->gspca_dev.width / 2);
		reg_w(sd, 0x2d, sd->gspca_dev.width / 4);
	} else
		reg_w(sd, 0x2c, sd->gspca_dev.width);

	reg_w(sd, 0x00, 0xbf17); 
	reg_w(sd, 0x00, 0xbf10); 

	
	val = sd->gspca_dev.width * sd->gspca_dev.height;
	reg_w(sd, 0x3d, val & 0xffff); 
	reg_w(sd, 0x3e, val >> 16);    

	if (w9968cf_vga_mode[sd->gspca_dev.curr_mode].pixelformat ==
	    V4L2_PIX_FMT_JPEG) {
		
		jpeg_define(sd->jpeg_hdr, sd->gspca_dev.height,
			    sd->gspca_dev.width, 0x22); 
		jpeg_set_qual(sd->jpeg_hdr, sd->quality);
		w9968cf_upload_quantizationtables(sd);
	}

	
	if (sd->sensor == SEN_OV7620) {
		
		vs_polarity = 1;
		hs_polarity = 1;
	} else {
		vs_polarity = 1;
		hs_polarity = 0;
	}

	val = (vs_polarity << 12) | (hs_polarity << 11);

	if (w9968cf_vga_mode[sd->gspca_dev.curr_mode].pixelformat ==
	    V4L2_PIX_FMT_JPEG) {
		
		val |= 0x0003; 
	} else
		val |= 0x0080; 

	
	
	

	val |= 0x8000; 

	reg_w(sd, 0x16, val);

	sd->gspca_dev.empty_packet = 0;
}

static void w9968cf_stop0(struct sd *sd)
{
	reg_w(sd, 0x39, 0x0000); 
	reg_w(sd, 0x16, 0x0000); 
}

static void w9968cf_pkt_scan(struct gspca_dev *gspca_dev,
			u8 *data,			
			int len)			
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (w9968cf_vga_mode[gspca_dev->curr_mode].pixelformat ==
	    V4L2_PIX_FMT_JPEG) {
		if (len >= 2 &&
		    data[0] == 0xff &&
		    data[1] == 0xd8) {
			gspca_frame_add(gspca_dev, LAST_PACKET,
					NULL, 0);
			gspca_frame_add(gspca_dev, FIRST_PACKET,
					sd->jpeg_hdr, JPEG_HDR_SZ);
			len -= 2;
			data += 2;
		}
	} else {
		
		if (gspca_dev->empty_packet) {
			gspca_frame_add(gspca_dev, LAST_PACKET,
						NULL, 0);
			gspca_frame_add(gspca_dev, FIRST_PACKET,
					NULL, 0);
			gspca_dev->empty_packet = 0;
		}
	}
	gspca_frame_add(gspca_dev, INTER_PACKET, data, len);
}
