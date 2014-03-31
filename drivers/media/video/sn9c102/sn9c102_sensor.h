/***************************************************************************
 * API for image sensors connected to the SN9C1xx PC Camera Controllers    *
 *                                                                         *
 * Copyright (C) 2004-2007 by Luca Risolia <luca.risolia@studio.unibo.it>  *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 2 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this program; if not, write to the Free Software             *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.               *
 ***************************************************************************/

#ifndef _SN9C102_SENSOR_H_
#define _SN9C102_SENSOR_H_

#include <linux/usb.h>
#include <linux/videodev2.h>
#include <linux/device.h>
#include <linux/stddef.h>
#include <linux/errno.h>
#include <asm/types.h>

struct sn9c102_device;
struct sn9c102_sensor;




enum sn9c102_bridge {
	BRIDGE_SN9C101 = 0x01,
	BRIDGE_SN9C102 = 0x02,
	BRIDGE_SN9C103 = 0x04,
	BRIDGE_SN9C105 = 0x08,
	BRIDGE_SN9C120 = 0x10,
};

enum sn9c102_bridge sn9c102_get_bridge(struct sn9c102_device* cam);

struct sn9c102_sensor* sn9c102_get_sensor(struct sn9c102_device* cam);

extern struct sn9c102_device*
sn9c102_match_id(struct sn9c102_device* cam, const struct usb_device_id *id);

extern void
sn9c102_attach_sensor(struct sn9c102_device* cam,
		      const struct sn9c102_sensor* sensor);

/*
   Read/write routines: they always return -1 on error, 0 or the read value
   otherwise. NOTE that a real read operation is not supported by the SN9C1XX
   chip for some of its registers. To work around this problem, a pseudo-read
   call is provided instead: it returns the last successfully written value
   on the register (0 if it has never been written), the usual -1 on error.
*/

extern int sn9c102_i2c_try_read(struct sn9c102_device*,
				const struct sn9c102_sensor*, u8 address);

extern int sn9c102_i2c_try_raw_write(struct sn9c102_device* cam,
				     const struct sn9c102_sensor* sensor, u8 n,
				     u8 data0, u8 data1, u8 data2, u8 data3,
				     u8 data4, u8 data5);
extern int sn9c102_i2c_try_raw_read(struct sn9c102_device* cam,
				    const struct sn9c102_sensor* sensor,
				    u8 data0, u8 data1, u8 n, u8 buffer[]);

extern int sn9c102_i2c_write(struct sn9c102_device*, u8 address, u8 value);
extern int sn9c102_i2c_read(struct sn9c102_device*, u8 address);

extern int sn9c102_read_reg(struct sn9c102_device*, u16 index);
extern int sn9c102_pread_reg(struct sn9c102_device*, u16 index);
extern int sn9c102_write_reg(struct sn9c102_device*, u8 value, u16 index);
extern int sn9c102_write_regs(struct sn9c102_device*, const u8 valreg[][2],
			      int count);
#define sn9c102_write_const_regs(sn9c102_device, data...)                     \
	({ static const u8 _valreg[][2] = {data};                             \
	sn9c102_write_regs(sn9c102_device, _valreg, ARRAY_SIZE(_valreg)); })


enum sn9c102_i2c_sysfs_ops {
	SN9C102_I2C_READ = 0x01,
	SN9C102_I2C_WRITE = 0x02,
};

enum sn9c102_i2c_frequency { 
	SN9C102_I2C_100KHZ = 0x01,
	SN9C102_I2C_400KHZ = 0x02,
};

enum sn9c102_i2c_interface {
	SN9C102_I2C_2WIRES,
	SN9C102_I2C_3WIRES,
};

#define SN9C102_MAX_CTRLS (V4L2_CID_LASTP1-V4L2_CID_BASE+10)

struct sn9c102_sensor {
	char name[32], 
	     maintainer[64]; 

	enum sn9c102_bridge supported_bridge; 

	
	enum sn9c102_i2c_sysfs_ops sysfs_ops;

	enum sn9c102_i2c_frequency frequency;
	enum sn9c102_i2c_interface interface;

	u8 i2c_slave_id; 


	int (*init)(struct sn9c102_device* cam);

	struct v4l2_queryctrl qctrl[SN9C102_MAX_CTRLS];

	int (*get_ctrl)(struct sn9c102_device* cam, struct v4l2_control* ctrl);
	int (*set_ctrl)(struct sn9c102_device* cam,
			const struct v4l2_control* ctrl);

	struct v4l2_cropcap cropcap;

	int (*set_crop)(struct sn9c102_device* cam,
			const struct v4l2_rect* rect);

	struct v4l2_pix_format pix_format;

	int (*set_pix_format)(struct sn9c102_device* cam,
			      const struct v4l2_pix_format* pix);

	struct v4l2_queryctrl _qctrl[SN9C102_MAX_CTRLS];
	struct v4l2_rect _rect;
};


#define SN9C102_V4L2_CID_DAC_MAGNITUDE (V4L2_CID_PRIVATE_BASE + 0)
#define SN9C102_V4L2_CID_GREEN_BALANCE (V4L2_CID_PRIVATE_BASE + 1)
#define SN9C102_V4L2_CID_RESET_LEVEL (V4L2_CID_PRIVATE_BASE + 2)
#define SN9C102_V4L2_CID_PIXEL_BIAS_VOLTAGE (V4L2_CID_PRIVATE_BASE + 3)
#define SN9C102_V4L2_CID_GAMMA (V4L2_CID_PRIVATE_BASE + 4)
#define SN9C102_V4L2_CID_BAND_FILTER (V4L2_CID_PRIVATE_BASE + 5)
#define SN9C102_V4L2_CID_BRIGHT_LEVEL (V4L2_CID_PRIVATE_BASE + 6)

#endif 
