/*
 *  cx18 ADEC firmware functions
 *
 *  Copyright (C) 2007  Hans Verkuil <hverkuil@xs4all.nl>
 *  Copyright (C) 2008  Andy Walls <awalls@md.metrocast.net>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include "cx18-driver.h"
#include "cx18-io.h"
#include <linux/firmware.h>

#define CX18_AUDIO_ENABLE    0xc72014
#define CX18_AI1_MUX_MASK    0x30
#define CX18_AI1_MUX_I2S1    0x00
#define CX18_AI1_MUX_I2S2    0x10
#define CX18_AI1_MUX_843_I2S 0x20
#define CX18_AI1_MUX_INVALID 0x30

#define FWFILE "v4l-cx23418-dig.fw"

static int cx18_av_verifyfw(struct cx18 *cx, const struct firmware *fw)
{
	struct v4l2_subdev *sd = &cx->av_state.sd;
	int ret = 0;
	const u8 *data;
	u32 size;
	int addr;
	u32 expected, dl_control;

	
	dl_control = cx18_av_read4(cx, CXADEC_DL_CTL);
	do {
		dl_control &= 0x00ffffff;
		dl_control |= 0x0f000000;
		cx18_av_write4_noretry(cx, CXADEC_DL_CTL, dl_control);
		dl_control = cx18_av_read4(cx, CXADEC_DL_CTL);
	} while ((dl_control & 0xff000000) != 0x0f000000);

	
	while (dl_control & 0x3fff)
		dl_control = cx18_av_read4(cx, CXADEC_DL_CTL);

	data = fw->data;
	size = fw->size;
	for (addr = 0; addr < size; addr++) {
		dl_control &= 0xffff3fff; 
		expected = 0x0f000000 | ((u32)data[addr] << 16) | addr;
		if (expected != dl_control) {
			CX18_ERR_DEV(sd, "verification of %s firmware load "
				     "failed: expected %#010x got %#010x\n",
				     FWFILE, expected, dl_control);
			ret = -EIO;
			break;
		}
		dl_control = cx18_av_read4(cx, CXADEC_DL_CTL);
	}
	if (ret == 0)
		CX18_INFO_DEV(sd, "verified load of %s firmware (%d bytes)\n",
			      FWFILE, size);
	return ret;
}

int cx18_av_loadfw(struct cx18 *cx)
{
	struct v4l2_subdev *sd = &cx->av_state.sd;
	const struct firmware *fw = NULL;
	u32 size;
	u32 u, v;
	const u8 *ptr;
	int i;
	int retries1 = 0;

	if (request_firmware(&fw, FWFILE, &cx->pci_dev->dev) != 0) {
		CX18_ERR_DEV(sd, "unable to open firmware %s\n", FWFILE);
		return -EINVAL;
	}

	while (retries1 < 5) {
		cx18_av_write4_expect(cx, CXADEC_CHIP_CTRL, 0x00010000,
					  0x00008430, 0xffffffff); 
		cx18_av_write_expect(cx, CXADEC_STD_DET_CTL, 0xf6, 0xf6, 0xff);

		
		cx18_av_write4_expect(cx, 0x8100, 0x00010000,
					  0x00008430, 0xffffffff); 

		
		cx18_av_write4_noretry(cx, CXADEC_DL_CTL, 0x0F000000);

		ptr = fw->data;
		size = fw->size;

		for (i = 0; i < size; i++) {
			u32 dl_control = 0x0F000000 | i | ((u32)ptr[i] << 16);
			u32 value = 0;
			int retries2;
			int unrec_err = 0;

			for (retries2 = 0; retries2 < CX18_MAX_MMIO_WR_RETRIES;
			     retries2++) {
				cx18_av_write4_noretry(cx, CXADEC_DL_CTL,
						       dl_control);
				udelay(10);
				value = cx18_av_read4(cx, CXADEC_DL_CTL);
				if (value == dl_control)
					break;
				if ((value & 0x3F00) != (dl_control & 0x3F00)) {
					unrec_err = 1;
					break;
				}
			}
			if (unrec_err || retries2 >= CX18_MAX_MMIO_WR_RETRIES)
				break;
		}
		if (i == size)
			break;
		retries1++;
	}
	if (retries1 >= 5) {
		CX18_ERR_DEV(sd, "unable to load firmware %s\n", FWFILE);
		release_firmware(fw);
		return -EIO;
	}

	cx18_av_write4_expect(cx, CXADEC_DL_CTL,
				0x03000000 | fw->size, 0x03000000, 0x13000000);

	CX18_INFO_DEV(sd, "loaded %s firmware (%d bytes)\n", FWFILE, size);

	if (cx18_av_verifyfw(cx, fw) == 0)
		cx18_av_write4_expect(cx, CXADEC_DL_CTL,
				0x13000000 | fw->size, 0x13000000, 0x13000000);

	
	cx18_av_and_or4(cx, CXADEC_PIN_CTRL1, ~0, 0x78000);

	
	
	
	cx18_av_write4(cx, CXADEC_I2S_IN_CTL, 0x000000A0);

	
	
	
	cx18_av_write4(cx, CXADEC_I2S_OUT_CTL, 0x000001A0);

	cx18_av_write4(cx, CXADEC_PIN_CFG3, 0x5600B687);

	cx18_av_write4_expect(cx, CXADEC_STD_DET_CTL, 0x000000F6, 0x000000F6,
								  0x3F00FFFF);
	

	
	cx18_av_write4(cx, 0x09CC, 1);

	v = cx18_read_reg(cx, CX18_AUDIO_ENABLE);
	
	if (v & 0x800)
		cx18_write_reg_expect(cx, v & 0xFFFFFBFF, CX18_AUDIO_ENABLE,
				      0, 0x400);

	
	v = cx18_read_reg(cx, CX18_AUDIO_ENABLE);
	u = v & CX18_AI1_MUX_MASK;
	v &= ~CX18_AI1_MUX_MASK;
	if (u == CX18_AI1_MUX_843_I2S || u == CX18_AI1_MUX_INVALID) {
		
		v |= CX18_AI1_MUX_I2S1;
		cx18_write_reg_expect(cx, v | 0xb00, CX18_AUDIO_ENABLE,
				      v, CX18_AI1_MUX_MASK);
		
		v = (v & ~CX18_AI1_MUX_MASK) | CX18_AI1_MUX_843_I2S;
	} else {
		
		v |= CX18_AI1_MUX_843_I2S;
		cx18_write_reg_expect(cx, v | 0xb00, CX18_AUDIO_ENABLE,
				      v, CX18_AI1_MUX_MASK);
		
		v = (v & ~CX18_AI1_MUX_MASK) | u;
	}
	cx18_write_reg_expect(cx, v | 0xb00, CX18_AUDIO_ENABLE,
			      v, CX18_AI1_MUX_MASK);

	
	v = cx18_av_read4(cx, CXADEC_STD_DET_CTL);
	v |= 0xFF;   
	v |= 0x400;  
	v |= 0x14000000;
	cx18_av_write4_expect(cx, CXADEC_STD_DET_CTL, v, v, 0x3F00FFFF);

	release_firmware(fw);
	return 0;
}
