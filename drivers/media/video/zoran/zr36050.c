/*
 * Zoran ZR36050 basic configuration functions
 *
 * Copyright (C) 2001 Wolfgang Scherr <scherr@net4you.at>
 *
 * $Id: zr36050.c,v 1.1.2.11 2003/08/03 14:54:53 rbultje Exp $
 *
 * ------------------------------------------------------------------------
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
 *
 * ------------------------------------------------------------------------
 */

#define ZR050_VERSION "v0.7.1"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <linux/types.h>
#include <linux/wait.h>

#include <asm/io.h>

#include "zr36050.h"

#include "videocodec.h"

#define MAX_CODECS 20

static int zr36050_codecs;

static int debug;
module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "Debug level (0-4)");

#define dprintk(num, format, args...) \
	do { \
		if (debug >= num) \
			printk(format, ##args); \
	} while (0)


static u8
zr36050_read (struct zr36050 *ptr,
	      u16             reg)
{
	u8 value = 0;

	
	if (ptr->codec->master_data->readreg)
		value = (ptr->codec->master_data->readreg(ptr->codec,
							  reg)) & 0xFF;
	else
		dprintk(1,
			KERN_ERR "%s: invalid I/O setup, nothing read!\n",
			ptr->name);

	dprintk(4, "%s: reading from 0x%04x: %02x\n", ptr->name, reg,
		value);

	return value;
}

static void
zr36050_write (struct zr36050 *ptr,
	       u16             reg,
	       u8              value)
{
	dprintk(4, "%s: writing 0x%02x to 0x%04x\n", ptr->name, value,
		reg);

	
	if (ptr->codec->master_data->writereg)
		ptr->codec->master_data->writereg(ptr->codec, reg, value);
	else
		dprintk(1,
			KERN_ERR
			"%s: invalid I/O setup, nothing written!\n",
			ptr->name);
}


static u8
zr36050_read_status1 (struct zr36050 *ptr)
{
	ptr->status1 = zr36050_read(ptr, ZR050_STATUS_1);

	zr36050_read(ptr, 0);
	return ptr->status1;
}


static u16
zr36050_read_scalefactor (struct zr36050 *ptr)
{
	ptr->scalefact = (zr36050_read(ptr, ZR050_SF_HI) << 8) |
			 (zr36050_read(ptr, ZR050_SF_LO) & 0xFF);

	
	zr36050_read(ptr, 0);
	return ptr->scalefact;
}


static void
zr36050_wait_end (struct zr36050 *ptr)
{
	int i = 0;

	while (!(zr36050_read_status1(ptr) & 0x4)) {
		udelay(1);
		if (i++ > 200000) {	
			dprintk(1,
				"%s: timeout at wait_end (last status: 0x%02x)\n",
				ptr->name, ptr->status1);
			break;
		}
	}
}


static int
zr36050_basic_test (struct zr36050 *ptr)
{
	zr36050_write(ptr, ZR050_SOF_IDX, 0x00);
	zr36050_write(ptr, ZR050_SOF_IDX + 1, 0x00);
	if ((zr36050_read(ptr, ZR050_SOF_IDX) |
	     zr36050_read(ptr, ZR050_SOF_IDX + 1)) != 0x0000) {
		dprintk(1,
			KERN_ERR
			"%s: attach failed, can't connect to jpeg processor!\n",
			ptr->name);
		return -ENXIO;
	}
	zr36050_write(ptr, ZR050_SOF_IDX, 0xff);
	zr36050_write(ptr, ZR050_SOF_IDX + 1, 0xc0);
	if (((zr36050_read(ptr, ZR050_SOF_IDX) << 8) |
	     zr36050_read(ptr, ZR050_SOF_IDX + 1)) != 0xffc0) {
		dprintk(1,
			KERN_ERR
			"%s: attach failed, can't connect to jpeg processor!\n",
			ptr->name);
		return -ENXIO;
	}

	zr36050_wait_end(ptr);
	if ((ptr->status1 & 0x4) == 0) {
		dprintk(1,
			KERN_ERR
			"%s: attach failed, jpeg processor failed (end flag)!\n",
			ptr->name);
		return -EBUSY;
	}

	return 0;		
}


static int
zr36050_pushit (struct zr36050 *ptr,
		u16             startreg,
		u16             len,
		const char     *data)
{
	int i = 0;

	dprintk(4, "%s: write data block to 0x%04x (len=%d)\n", ptr->name,
		startreg, len);
	while (i < len) {
		zr36050_write(ptr, startreg++, data[i++]);
	}

	return i;
}


static const char zr36050_dqt[0x86] = {
	0xff, 0xdb,		
	0x00, 0x84,		
	0x00,			
	0x10, 0x0b, 0x0c, 0x0e, 0x0c, 0x0a, 0x10, 0x0e,
	0x0d, 0x0e, 0x12, 0x11, 0x10, 0x13, 0x18, 0x28,
	0x1a, 0x18, 0x16, 0x16, 0x18, 0x31, 0x23, 0x25,
	0x1d, 0x28, 0x3a, 0x33, 0x3d, 0x3c, 0x39, 0x33,
	0x38, 0x37, 0x40, 0x48, 0x5c, 0x4e, 0x40, 0x44,
	0x57, 0x45, 0x37, 0x38, 0x50, 0x6d, 0x51, 0x57,
	0x5f, 0x62, 0x67, 0x68, 0x67, 0x3e, 0x4d, 0x71,
	0x79, 0x70, 0x64, 0x78, 0x5c, 0x65, 0x67, 0x63,
	0x01,			
	0x11, 0x12, 0x12, 0x18, 0x15, 0x18, 0x2f, 0x1a,
	0x1a, 0x2f, 0x63, 0x42, 0x38, 0x42, 0x63, 0x63,
	0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
	0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
	0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
	0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
	0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
	0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63
};

static const char zr36050_dht[0x1a4] = {
	0xff, 0xc4,		
	0x01, 0xa2,		
	0x00,			
	0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
	0x01,			
	0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
	0x10,			
	0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03,
	0x05, 0x05, 0x04, 0x04, 0x00, 0x00,
	0x01, 0x7D, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11,
	0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61,
	0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1,
	0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0, 0x24,
	0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17,
	0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x34,
	0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44,
	0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56,
	0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66,
	0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
	0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
	0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
	0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8,
	0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9,
	0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8,
	0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9,
	0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
	0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
	0xF8, 0xF9, 0xFA,
	0x11,			
	0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04,
	0x07, 0x05, 0x04, 0x04, 0x00, 0x01,
	0x02, 0x77, 0x00, 0x01, 0x02, 0x03, 0x11, 0x04,
	0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
	0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
	0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0, 0x15, 0x62,
	0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25,
	0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26, 0x27, 0x28, 0x29, 0x2A,
	0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44,
	0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56,
	0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66,
	0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
	0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
	0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
	0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8,
	0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
	0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8,
	0xD9, 0xDA, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
	0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
	0xF9, 0xFA
};

#define NO_OF_COMPONENTS          0x3	
#define BASELINE_PRECISION        0x8	
static const char zr36050_tq[8] = { 0, 1, 1, 0, 0, 0, 0, 0 };	
static const char zr36050_td[8] = { 0, 1, 1, 0, 0, 0, 0, 0 };	
static const char zr36050_ta[8] = { 0, 1, 1, 0, 0, 0, 0, 0 };	

static const char zr36050_decimation_h[8] = { 2, 1, 1, 0, 0, 0, 0, 0 };
static const char zr36050_decimation_v[8] = { 1, 1, 1, 0, 0, 0, 0, 0 };




static int
zr36050_set_sof (struct zr36050 *ptr)
{
	char sof_data[34];	
	int i;

	dprintk(3, "%s: write SOF (%dx%d, %d components)\n", ptr->name,
		ptr->width, ptr->height, NO_OF_COMPONENTS);
	sof_data[0] = 0xff;
	sof_data[1] = 0xc0;
	sof_data[2] = 0x00;
	sof_data[3] = (3 * NO_OF_COMPONENTS) + 8;
	sof_data[4] = BASELINE_PRECISION;	
	sof_data[5] = (ptr->height) >> 8;
	sof_data[6] = (ptr->height) & 0xff;
	sof_data[7] = (ptr->width) >> 8;
	sof_data[8] = (ptr->width) & 0xff;
	sof_data[9] = NO_OF_COMPONENTS;
	for (i = 0; i < NO_OF_COMPONENTS; i++) {
		sof_data[10 + (i * 3)] = i;	
		sof_data[11 + (i * 3)] = (ptr->h_samp_ratio[i] << 4) | (ptr->v_samp_ratio[i]);	
		sof_data[12 + (i * 3)] = zr36050_tq[i];	
	}
	return zr36050_pushit(ptr, ZR050_SOF_IDX,
			      (3 * NO_OF_COMPONENTS) + 10, sof_data);
}



static int
zr36050_set_sos (struct zr36050 *ptr)
{
	char sos_data[16];	
	int i;

	dprintk(3, "%s: write SOS\n", ptr->name);
	sos_data[0] = 0xff;
	sos_data[1] = 0xda;
	sos_data[2] = 0x00;
	sos_data[3] = 2 + 1 + (2 * NO_OF_COMPONENTS) + 3;
	sos_data[4] = NO_OF_COMPONENTS;
	for (i = 0; i < NO_OF_COMPONENTS; i++) {
		sos_data[5 + (i * 2)] = i;	
		sos_data[6 + (i * 2)] = (zr36050_td[i] << 4) | zr36050_ta[i];	
	}
	sos_data[2 + 1 + (2 * NO_OF_COMPONENTS) + 2] = 00;	
	sos_data[2 + 1 + (2 * NO_OF_COMPONENTS) + 3] = 0x3F;
	sos_data[2 + 1 + (2 * NO_OF_COMPONENTS) + 4] = 00;
	return zr36050_pushit(ptr, ZR050_SOS1_IDX,
			      4 + 1 + (2 * NO_OF_COMPONENTS) + 3,
			      sos_data);
}



static int
zr36050_set_dri (struct zr36050 *ptr)
{
	char dri_data[6];	

	dprintk(3, "%s: write DRI\n", ptr->name);
	dri_data[0] = 0xff;
	dri_data[1] = 0xdd;
	dri_data[2] = 0x00;
	dri_data[3] = 0x04;
	dri_data[4] = ptr->dri >> 8;
	dri_data[5] = ptr->dri & 0xff;
	return zr36050_pushit(ptr, ZR050_DRI_IDX, 6, dri_data);
}

static void
zr36050_init (struct zr36050 *ptr)
{
	int sum = 0;
	long bitcnt, tmp;

	if (ptr->mode == CODEC_DO_COMPRESSION) {
		dprintk(2, "%s: COMPRESSION SETUP\n", ptr->name);

		
		zr36050_write(ptr, ZR050_HARDWARE, ZR050_HW_MSTR);

		
		zr36050_write(ptr, ZR050_MODE,
			      ZR050_MO_COMP | ZR050_MO_TLM);
		zr36050_write(ptr, ZR050_OPTIONS, 0);

		
		zr36050_write(ptr, ZR050_INT_REQ_0, 0);
		zr36050_write(ptr, ZR050_INT_REQ_1, 3);	

		
		
		zr36050_write(ptr, ZR050_SF_HI, ptr->scalefact >> 8);
		zr36050_write(ptr, ZR050_SF_LO, ptr->scalefact & 0xff);

		zr36050_write(ptr, ZR050_AF_HI, 0xff);
		zr36050_write(ptr, ZR050_AF_M, 0xff);
		zr36050_write(ptr, ZR050_AF_LO, 0xff);

		
		sum += zr36050_set_sof(ptr);
		sum += zr36050_set_sos(ptr);
		sum += zr36050_set_dri(ptr);

		dprintk(3, "%s: write DQT, DHT, APP\n", ptr->name);
		sum += zr36050_pushit(ptr, ZR050_DQT_IDX,
				      sizeof(zr36050_dqt), zr36050_dqt);
		sum += zr36050_pushit(ptr, ZR050_DHT_IDX,
				      sizeof(zr36050_dht), zr36050_dht);
		zr36050_write(ptr, ZR050_APP_IDX, 0xff);
		zr36050_write(ptr, ZR050_APP_IDX + 1, 0xe0 + ptr->app.appn);
		zr36050_write(ptr, ZR050_APP_IDX + 2, 0x00);
		zr36050_write(ptr, ZR050_APP_IDX + 3, ptr->app.len + 2);
		sum += zr36050_pushit(ptr, ZR050_APP_IDX + 4, 60,
				      ptr->app.data) + 4;
		zr36050_write(ptr, ZR050_COM_IDX, 0xff);
		zr36050_write(ptr, ZR050_COM_IDX + 1, 0xfe);
		zr36050_write(ptr, ZR050_COM_IDX + 2, 0x00);
		zr36050_write(ptr, ZR050_COM_IDX + 3, ptr->com.len + 2);
		sum += zr36050_pushit(ptr, ZR050_COM_IDX + 4, 60,
				      ptr->com.data) + 4;

		
		zr36050_write(ptr, ZR050_MARKERS_EN, ZR050_ME_DHTI);

		zr36050_write(ptr, ZR050_GO, 1);	
		zr36050_wait_end(ptr);
		dprintk(2, "%s: Status after table preload: 0x%02x\n",
			ptr->name, ptr->status1);

		if ((ptr->status1 & 0x4) == 0) {
			dprintk(1, KERN_ERR "%s: init aborted!\n",
				ptr->name);
			return;	
		}

		

		
		sum = ptr->real_code_vol - sum;
		bitcnt = sum << 3;	

		tmp = bitcnt >> 16;
		dprintk(3,
			"%s: code: csize=%d, tot=%d, bit=%ld, highbits=%ld\n",
			ptr->name, sum, ptr->real_code_vol, bitcnt, tmp);
		zr36050_write(ptr, ZR050_TCV_NET_HI, tmp >> 8);
		zr36050_write(ptr, ZR050_TCV_NET_MH, tmp & 0xff);
		tmp = bitcnt & 0xffff;
		zr36050_write(ptr, ZR050_TCV_NET_ML, tmp >> 8);
		zr36050_write(ptr, ZR050_TCV_NET_LO, tmp & 0xff);

		bitcnt -= bitcnt >> 7;	
		bitcnt -= ((bitcnt * 5) >> 6);	

		tmp = bitcnt >> 16;
		dprintk(3, "%s: code: nettobit=%ld, highnettobits=%ld\n",
			ptr->name, bitcnt, tmp);
		zr36050_write(ptr, ZR050_TCV_DATA_HI, tmp >> 8);
		zr36050_write(ptr, ZR050_TCV_DATA_MH, tmp & 0xff);
		tmp = bitcnt & 0xffff;
		zr36050_write(ptr, ZR050_TCV_DATA_ML, tmp >> 8);
		zr36050_write(ptr, ZR050_TCV_DATA_LO, tmp & 0xff);

		
		zr36050_write(ptr, ZR050_MODE,
			      ZR050_MO_COMP | ZR050_MO_PASS2 |
			      (ptr->bitrate_ctrl ? ZR050_MO_BRC : 0));

		
		zr36050_write(ptr, ZR050_MARKERS_EN,
			      ZR050_ME_DQT | ZR050_ME_DHT |
			      ((ptr->app.len > 0) ? ZR050_ME_APP : 0) |
			      ((ptr->com.len > 0) ? ZR050_ME_COM : 0));
	} else {
		dprintk(2, "%s: EXPANSION SETUP\n", ptr->name);

		
		zr36050_write(ptr, ZR050_HARDWARE,
			      ZR050_HW_MSTR | ZR050_HW_CFIS_2_CLK);

		
		zr36050_write(ptr, ZR050_MODE, ZR050_MO_TLM);

		
		zr36050_write(ptr, ZR050_INT_REQ_0, 0);
		zr36050_write(ptr, ZR050_INT_REQ_1, 3);	

		dprintk(3, "%s: write DHT\n", ptr->name);
		zr36050_pushit(ptr, ZR050_DHT_IDX, sizeof(zr36050_dht),
			       zr36050_dht);

		
		zr36050_write(ptr, ZR050_MARKERS_EN, ZR050_ME_DHTI);

		zr36050_write(ptr, ZR050_GO, 1);	
		zr36050_wait_end(ptr);
		dprintk(2, "%s: Status after table preload: 0x%02x\n",
			ptr->name, ptr->status1);

		if ((ptr->status1 & 0x4) == 0) {
			dprintk(1, KERN_ERR "%s: init aborted!\n",
				ptr->name);
			return;	
		}

		
		zr36050_write(ptr, ZR050_MODE, 0);
		zr36050_write(ptr, ZR050_MARKERS_EN, 0);
	}

	
	zr36050_read(ptr, 0);
}


static int
zr36050_set_mode (struct videocodec *codec,
		  int                mode)
{
	struct zr36050 *ptr = (struct zr36050 *) codec->data;

	dprintk(2, "%s: set_mode %d call\n", ptr->name, mode);

	if ((mode != CODEC_DO_EXPANSION) && (mode != CODEC_DO_COMPRESSION))
		return -EINVAL;

	ptr->mode = mode;
	zr36050_init(ptr);

	return 0;
}

static int
zr36050_set_video (struct videocodec   *codec,
		   struct tvnorm       *norm,
		   struct vfe_settings *cap,
		   struct vfe_polarity *pol)
{
	struct zr36050 *ptr = (struct zr36050 *) codec->data;
	int size;

	dprintk(2, "%s: set_video %d.%d, %d/%d-%dx%d (0x%x) q%d call\n",
		ptr->name, norm->HStart, norm->VStart,
		cap->x, cap->y, cap->width, cap->height,
		cap->decimation, cap->quality);
	ptr->width = cap->width / (cap->decimation & 0xff);
	ptr->height = cap->height / ((cap->decimation >> 8) & 0xff);

	
	size = ptr->width * ptr->height;
	size *= 16; 
	
	size = size * cap->quality / 200;

	
	if (size < 8192)
		size = 8192;
	
	if (size > ptr->total_code_vol * 7)
		size = ptr->total_code_vol * 7;

	ptr->real_code_vol = size >> 3; 

	zr36050_write(ptr, ZR050_MBCV, ptr->max_block_vol);

	return 0;
}

static int
zr36050_control (struct videocodec *codec,
		 int                type,
		 int                size,
		 void              *data)
{
	struct zr36050 *ptr = (struct zr36050 *) codec->data;
	int *ival = (int *) data;

	dprintk(2, "%s: control %d call with %d byte\n", ptr->name, type,
		size);

	switch (type) {
	case CODEC_G_STATUS:	
		if (size != sizeof(int))
			return -EFAULT;
		zr36050_read_status1(ptr);
		*ival = ptr->status1;
		break;

	case CODEC_G_CODEC_MODE:
		if (size != sizeof(int))
			return -EFAULT;
		*ival = CODEC_MODE_BJPG;
		break;

	case CODEC_S_CODEC_MODE:
		if (size != sizeof(int))
			return -EFAULT;
		if (*ival != CODEC_MODE_BJPG)
			return -EINVAL;
		
		return 0;

	case CODEC_G_VFE:
	case CODEC_S_VFE:
		
		return 0;

	case CODEC_S_MMAP:
		
		return -ENXIO;

	case CODEC_G_JPEG_TDS_BYTE:	
		if (size != sizeof(int))
			return -EFAULT;
		*ival = ptr->total_code_vol;
		break;

	case CODEC_S_JPEG_TDS_BYTE:	
		if (size != sizeof(int))
			return -EFAULT;
		ptr->total_code_vol = *ival;
		ptr->real_code_vol = (ptr->total_code_vol * 6) >> 3;
		break;

	case CODEC_G_JPEG_SCALE:	
		if (size != sizeof(int))
			return -EFAULT;
		*ival = zr36050_read_scalefactor(ptr);
		break;

	case CODEC_S_JPEG_SCALE:	
		if (size != sizeof(int))
			return -EFAULT;
		ptr->scalefact = *ival;
		break;

	case CODEC_G_JPEG_APP_DATA: {	
		struct jpeg_app_marker *app = data;

		if (size != sizeof(struct jpeg_app_marker))
			return -EFAULT;

		*app = ptr->app;
		break;
	}

	case CODEC_S_JPEG_APP_DATA: {	 
		struct jpeg_app_marker *app = data;

		if (size != sizeof(struct jpeg_app_marker))
			return -EFAULT;

		ptr->app = *app;
		break;
	}

	case CODEC_G_JPEG_COM_DATA: {	
		struct jpeg_com_marker *com = data;

		if (size != sizeof(struct jpeg_com_marker))
			return -EFAULT;

		*com = ptr->com;
		break;
	}

	case CODEC_S_JPEG_COM_DATA: {	
		struct jpeg_com_marker *com = data;

		if (size != sizeof(struct jpeg_com_marker))
			return -EFAULT;

		ptr->com = *com;
		break;
	}

	default:
		return -EINVAL;
	}

	return size;
}


static int
zr36050_unset (struct videocodec *codec)
{
	struct zr36050 *ptr = codec->data;

	if (ptr) {
		

		dprintk(1, "%s: finished codec #%d\n", ptr->name,
			ptr->num);
		kfree(ptr);
		codec->data = NULL;

		zr36050_codecs--;
		return 0;
	}

	return -EFAULT;
}


static int
zr36050_setup (struct videocodec *codec)
{
	struct zr36050 *ptr;
	int res;

	dprintk(2, "zr36050: initializing MJPEG subsystem #%d.\n",
		zr36050_codecs);

	if (zr36050_codecs == MAX_CODECS) {
		dprintk(1,
			KERN_ERR "zr36050: Can't attach more codecs!\n");
		return -ENOSPC;
	}
	
	codec->data = ptr = kzalloc(sizeof(struct zr36050), GFP_KERNEL);
	if (NULL == ptr) {
		dprintk(1, KERN_ERR "zr36050: Can't get enough memory!\n");
		return -ENOMEM;
	}

	snprintf(ptr->name, sizeof(ptr->name), "zr36050[%d]",
		 zr36050_codecs);
	ptr->num = zr36050_codecs++;
	ptr->codec = codec;

	
	res = zr36050_basic_test(ptr);
	if (res < 0) {
		zr36050_unset(codec);
		return res;
	}
	
	memcpy(ptr->h_samp_ratio, zr36050_decimation_h, 8);
	memcpy(ptr->v_samp_ratio, zr36050_decimation_v, 8);

	ptr->bitrate_ctrl = 0;	
	ptr->mode = CODEC_DO_COMPRESSION;
	ptr->width = 384;
	ptr->height = 288;
	ptr->total_code_vol = 16000;
	ptr->max_block_vol = 240;
	ptr->scalefact = 0x100;
	ptr->dri = 1;

	
	ptr->app.appn = 0;
	ptr->app.len = 0;
	ptr->com.len = 0;

	zr36050_init(ptr);

	dprintk(1, KERN_INFO "%s: codec attached and running\n",
		ptr->name);

	return 0;
}

static const struct videocodec zr36050_codec = {
	.owner = THIS_MODULE,
	.name = "zr36050",
	.magic = 0L,		
	.flags =
	    CODEC_FLAG_JPEG | CODEC_FLAG_HARDWARE | CODEC_FLAG_ENCODER |
	    CODEC_FLAG_DECODER,
	.type = CODEC_TYPE_ZR36050,
	.setup = zr36050_setup,	
	.unset = zr36050_unset,
	.set_mode = zr36050_set_mode,
	.set_video = zr36050_set_video,
	.control = zr36050_control,
	
};


static int __init
zr36050_init_module (void)
{
	
	zr36050_codecs = 0;
	return videocodec_register(&zr36050_codec);
}

static void __exit
zr36050_cleanup_module (void)
{
	if (zr36050_codecs) {
		dprintk(1,
			"zr36050: something's wrong - %d codecs left somehow.\n",
			zr36050_codecs);
	}
	videocodec_unregister(&zr36050_codec);
}

module_init(zr36050_init_module);
module_exit(zr36050_cleanup_module);

MODULE_AUTHOR("Wolfgang Scherr <scherr@net4you.at>");
MODULE_DESCRIPTION("Driver module for ZR36050 jpeg processors "
		   ZR050_VERSION);
MODULE_LICENSE("GPL");
