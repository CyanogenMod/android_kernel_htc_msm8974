/*
 * saa7185 - Philips SAA7185B video encoder driver version 0.0.3
 *
 * Copyright (C) 1998 Dave Perks <dperks@ibm.net>
 *
 * Slight changes for video timing and attachment output by
 * Wolfgang Scherr <scherr@net4you.net>
 *
 * Changes by Ronald Bultje <rbultje@ronald.bitfreak.net>
 *    - moved over to linux>=2.4.x i2c protocol (1/1/2003)
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>
#include <linux/i2c.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>

MODULE_DESCRIPTION("Philips SAA7185 video encoder driver");
MODULE_AUTHOR("Dave Perks");
MODULE_LICENSE("GPL");

static int debug;
module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "Debug level (0-1)");



struct saa7185 {
	struct v4l2_subdev sd;
	unsigned char reg[128];

	v4l2_std_id norm;
};

static inline struct saa7185 *to_saa7185(struct v4l2_subdev *sd)
{
	return container_of(sd, struct saa7185, sd);
}


static inline int saa7185_read(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return i2c_smbus_read_byte(client);
}

static int saa7185_write(struct v4l2_subdev *sd, u8 reg, u8 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct saa7185 *encoder = to_saa7185(sd);

	v4l2_dbg(1, debug, sd, "%02x set to %02x\n", reg, value);
	encoder->reg[reg] = value;
	return i2c_smbus_write_byte_data(client, reg, value);
}

static int saa7185_write_block(struct v4l2_subdev *sd,
		const u8 *data, unsigned int len)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct saa7185 *encoder = to_saa7185(sd);
	int ret = -1;
	u8 reg;

	if (i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		
		u8 block_data[32];
		int block_len;

		while (len >= 2) {
			block_len = 0;
			block_data[block_len++] = reg = data[0];
			do {
				block_data[block_len++] =
				    encoder->reg[reg++] = data[1];
				len -= 2;
				data += 2;
			} while (len >= 2 && data[0] == reg && block_len < 32);
			ret = i2c_master_send(client, block_data, block_len);
			if (ret < 0)
				break;
		}
	} else {
		
		while (len >= 2) {
			reg = *data++;
			ret = saa7185_write(sd, reg, *data++);
			if (ret < 0)
				break;
			len -= 2;
		}
	}

	return ret;
}


static const unsigned char init_common[] = {
	0x3a, 0x0f,		

	0x42, 0x6b,		
	0x43, 0x00,		
	0x44, 0x00,		
	0x45, 0x22,		
	0x46, 0xac,		
	0x47, 0x0e,		
	0x48, 0x03,		
	0x49, 0x1d,		
	0x4a, 0xac,		
	0x4b, 0xf0,		
	0x4c, 0xc8,		
	0x4d, 0xb9,		
	0x4e, 0xd4,		
	0x4f, 0x38,		
	0x50, 0x47,		
	0x51, 0xc1,		
	0x52, 0xe3,		
	0x53, 0x54,		
	0x54, 0xa3,		
	0x55, 0x54,		
	0x56, 0xf2,		
	0x57, 0x90,		
	0x58, 0x00,		
	0x59, 0x00,		

	0x5a, 0x00,		
	0x5b, 0x76,		
	0x5c, 0xa5,		
	0x5d, 0x3c,		
	0x5e, 0x3a,		
	0x5f, 0x3a,		
	0x60, 0x00,		

	

	0x67, 0x00,		
	0x68, 0x00,		
	0x69, 0x00,		
	0x6a, 0x00,		

	0x6b, 0x91,		
	0x6c, 0x20,		
	0x6d, 0x00,		

	0x6e, 0x0e,		
	0x6f, 0x00,		
	0x70, 0x20,		

	

	0x71, 0x15,		
	0x72, 0x90,		
	0x73, 0x61,		
	0x74, 0x00,		
	0x75, 0x00,		
	0x76, 0x00,		
	0x77, 0x15,		
	0x78, 0x90,		
	0x79, 0x61,		

	

	0x7a, 0x70,		

	

	0x7b, 0x16,		
	0x7c, 0x35,		
	0x7d, 0x20,		
};

static const unsigned char init_pal[] = {
	0x61, 0x1e,		
	0x62, 0xc8,		
	0x63, 0xcb,		
	0x64, 0x8a,		
	0x65, 0x09,		
	0x66, 0x2a,		
};

static const unsigned char init_ntsc[] = {
	0x61, 0x1d,		
	0x62, 0xe6,		
	0x63, 0x1f,		
	0x64, 0x7c,		
	0x65, 0xf0,		
	0x66, 0x21,		
};


static int saa7185_init(struct v4l2_subdev *sd, u32 val)
{
	struct saa7185 *encoder = to_saa7185(sd);

	saa7185_write_block(sd, init_common, sizeof(init_common));
	if (encoder->norm & V4L2_STD_NTSC)
		saa7185_write_block(sd, init_ntsc, sizeof(init_ntsc));
	else
		saa7185_write_block(sd, init_pal, sizeof(init_pal));
	return 0;
}

static int saa7185_s_std_output(struct v4l2_subdev *sd, v4l2_std_id std)
{
	struct saa7185 *encoder = to_saa7185(sd);

	if (std & V4L2_STD_NTSC)
		saa7185_write_block(sd, init_ntsc, sizeof(init_ntsc));
	else if (std & V4L2_STD_PAL)
		saa7185_write_block(sd, init_pal, sizeof(init_pal));
	else
		return -EINVAL;
	encoder->norm = std;
	return 0;
}

static int saa7185_s_routing(struct v4l2_subdev *sd,
			     u32 input, u32 output, u32 config)
{
	struct saa7185 *encoder = to_saa7185(sd);


	switch (input) {
	case 0:
		
		saa7185_write(sd, 0x3a, 0x0f);
		
		saa7185_write(sd, 0x61, (encoder->reg[0x61] & 0xf7) | 0x08);
		saa7185_write(sd, 0x6e, 0x01);
		break;

	case 1:
		
		saa7185_write(sd, 0x3a, 0x0f);
		
		saa7185_write(sd, 0x61, (encoder->reg[0x61] & 0xf7) | 0x00);
		
		saa7185_write(sd, 0x6e, 0x00);
		break;

	case 2:
		
		saa7185_write(sd, 0x3a, 0x8f);
		
		saa7185_write(sd, 0x61, (encoder->reg[0x61] & 0xf7) | 0x08);
		
		saa7185_write(sd, 0x6e, 0x01);
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static int saa7185_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_SAA7185, 0);
}


static const struct v4l2_subdev_core_ops saa7185_core_ops = {
	.g_chip_ident = saa7185_g_chip_ident,
	.init = saa7185_init,
};

static const struct v4l2_subdev_video_ops saa7185_video_ops = {
	.s_std_output = saa7185_s_std_output,
	.s_routing = saa7185_s_routing,
};

static const struct v4l2_subdev_ops saa7185_ops = {
	.core = &saa7185_core_ops,
	.video = &saa7185_video_ops,
};



static int saa7185_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int i;
	struct saa7185 *encoder;
	struct v4l2_subdev *sd;

	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	v4l_info(client, "chip found @ 0x%x (%s)\n",
			client->addr << 1, client->adapter->name);

	encoder = kzalloc(sizeof(struct saa7185), GFP_KERNEL);
	if (encoder == NULL)
		return -ENOMEM;
	encoder->norm = V4L2_STD_NTSC;
	sd = &encoder->sd;
	v4l2_i2c_subdev_init(sd, client, &saa7185_ops);

	i = saa7185_write_block(sd, init_common, sizeof(init_common));
	if (i >= 0)
		i = saa7185_write_block(sd, init_ntsc, sizeof(init_ntsc));
	if (i < 0)
		v4l2_dbg(1, debug, sd, "init error %d\n", i);
	else
		v4l2_dbg(1, debug, sd, "revision 0x%x\n",
				saa7185_read(sd) >> 5);
	return 0;
}

static int saa7185_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct saa7185 *encoder = to_saa7185(sd);

	v4l2_device_unregister_subdev(sd);
	
	saa7185_write(sd, 0x61, (encoder->reg[0x61]) | 0x40);
	kfree(encoder);
	return 0;
}


static const struct i2c_device_id saa7185_id[] = {
	{ "saa7185", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, saa7185_id);

static struct i2c_driver saa7185_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "saa7185",
	},
	.probe		= saa7185_probe,
	.remove		= saa7185_remove,
	.id_table	= saa7185_id,
};

module_i2c_driver(saa7185_driver);
