/*****************************************************************************
 *
 *     Author: Xilinx, Inc.
 *
 *     This program is free software; you can redistribute it and/or modify it
 *     under the terms of the GNU General Public License as published by the
 *     Free Software Foundation; either version 2 of the License, or (at your
 *     option) any later version.
 *
 *     XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS"
 *     AS A COURTESY TO YOU, SOLELY FOR USE IN DEVELOPING PROGRAMS AND
 *     SOLUTIONS FOR XILINX DEVICES.  BY PROVIDING THIS DESIGN, CODE,
 *     OR INFORMATION AS ONE POSSIBLE IMPLEMENTATION OF THIS FEATURE,
 *     APPLICATION OR STANDARD, XILINX IS MAKING NO REPRESENTATION
 *     THAT THIS IMPLEMENTATION IS FREE FROM ANY CLAIMS OF INFRINGEMENT,
 *     AND YOU ARE RESPONSIBLE FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE
 *     FOR YOUR IMPLEMENTATION.  XILINX EXPRESSLY DISCLAIMS ANY
 *     WARRANTY WHATSOEVER WITH RESPECT TO THE ADEQUACY OF THE
 *     IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OR
 *     REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE FROM CLAIMS OF
 *     INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *     FOR A PARTICULAR PURPOSE.
 *
 *     (c) Copyright 2007-2008 Xilinx Inc.
 *     All rights reserved.
 *
 *     You should have received a copy of the GNU General Public License along
 *     with this program; if not, write to the Free Software Foundation, Inc.,
 *     675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *****************************************************************************/

#include "fifo_icap.h"

#define XHI_GIER_OFFSET	0x1C  
#define XHI_IPISR_OFFSET 0x20  
#define XHI_IPIER_OFFSET 0x28  
#define XHI_WF_OFFSET 0x100 
#define XHI_RF_OFFSET 0x104 
#define XHI_SZ_OFFSET 0x108 
#define XHI_CR_OFFSET 0x10C 
#define XHI_SR_OFFSET 0x110 
#define XHI_WFV_OFFSET 0x114 
#define XHI_RFO_OFFSET 0x118 


#define XHI_GIER_GIE_MASK 0x80000000 

#define XHI_IPIXR_RFULL_MASK 0x00000008 
#define XHI_IPIXR_WEMPTY_MASK 0x00000004 
#define XHI_IPIXR_RDP_MASK 0x00000002 
#define XHI_IPIXR_WRP_MASK 0x00000001 
#define XHI_IPIXR_ALL_MASK 0x0000000F 

#define XHI_CR_SW_RESET_MASK 0x00000008 
#define XHI_CR_FIFO_CLR_MASK 0x00000004 
#define XHI_CR_READ_MASK 0x00000002 
#define XHI_CR_WRITE_MASK 0x00000001 


#define XHI_WFO_MAX_VACANCY 1024 
#define XHI_RFO_MAX_OCCUPANCY 256 
#define XHI_MAX_READ_TRANSACTION_WORDS 0xFFF


/**
 * fifo_icap_fifo_write - Write data to the write FIFO.
 * @drvdata: a pointer to the drvdata.
 * @data: the 32-bit value to be written to the FIFO.
 *
 * This function will silently fail if the fifo is full.
 **/
static inline void fifo_icap_fifo_write(struct hwicap_drvdata *drvdata,
		u32 data)
{
	dev_dbg(drvdata->dev, "fifo_write: %x\n", data);
	out_be32(drvdata->base_address + XHI_WF_OFFSET, data);
}

static inline u32 fifo_icap_fifo_read(struct hwicap_drvdata *drvdata)
{
	u32 data = in_be32(drvdata->base_address + XHI_RF_OFFSET);
	dev_dbg(drvdata->dev, "fifo_read: %x\n", data);
	return data;
}

static inline void fifo_icap_set_read_size(struct hwicap_drvdata *drvdata,
		u32 data)
{
	out_be32(drvdata->base_address + XHI_SZ_OFFSET, data);
}

static inline void fifo_icap_start_config(struct hwicap_drvdata *drvdata)
{
	out_be32(drvdata->base_address + XHI_CR_OFFSET, XHI_CR_WRITE_MASK);
	dev_dbg(drvdata->dev, "configuration started\n");
}

static inline void fifo_icap_start_readback(struct hwicap_drvdata *drvdata)
{
	out_be32(drvdata->base_address + XHI_CR_OFFSET, XHI_CR_READ_MASK);
	dev_dbg(drvdata->dev, "readback started\n");
}

u32 fifo_icap_get_status(struct hwicap_drvdata *drvdata)
{
	u32 status = in_be32(drvdata->base_address + XHI_SR_OFFSET);
	dev_dbg(drvdata->dev, "Getting status = %x\n", status);
	return status;
}

static inline u32 fifo_icap_busy(struct hwicap_drvdata *drvdata)
{
	u32 status = in_be32(drvdata->base_address + XHI_SR_OFFSET);
	return (status & XHI_SR_DONE_MASK) ? 0 : 1;
}

static inline u32 fifo_icap_write_fifo_vacancy(
		struct hwicap_drvdata *drvdata)
{
	return in_be32(drvdata->base_address + XHI_WFV_OFFSET);
}

static inline u32 fifo_icap_read_fifo_occupancy(
		struct hwicap_drvdata *drvdata)
{
	return in_be32(drvdata->base_address + XHI_RFO_OFFSET);
}

/**
 * fifo_icap_set_configuration - Send configuration data to the ICAP.
 * @drvdata: a pointer to the drvdata.
 * @frame_buffer: a pointer to the data to be written to the
 *		ICAP device.
 * @num_words: the number of words (32 bit) to write to the ICAP
 *		device.

 * This function writes the given user data to the Write FIFO in
 * polled mode and starts the transfer of the data to
 * the ICAP device.
 **/
int fifo_icap_set_configuration(struct hwicap_drvdata *drvdata,
		u32 *frame_buffer, u32 num_words)
{

	u32 write_fifo_vacancy = 0;
	u32 retries = 0;
	u32 remaining_words;

	dev_dbg(drvdata->dev, "fifo_set_configuration\n");

	if (fifo_icap_busy(drvdata))
		return -EBUSY;

	remaining_words = num_words;

	while (remaining_words > 0) {
		while (write_fifo_vacancy == 0) {
			write_fifo_vacancy =
				fifo_icap_write_fifo_vacancy(drvdata);
			retries++;
			if (retries > XHI_MAX_RETRIES)
				return -EIO;
		}

		while ((write_fifo_vacancy != 0) &&
				(remaining_words > 0)) {
			fifo_icap_fifo_write(drvdata, *frame_buffer);

			remaining_words--;
			write_fifo_vacancy--;
			frame_buffer++;
		}
		
		fifo_icap_start_config(drvdata);
	}

	
	while (fifo_icap_busy(drvdata)) {
		retries++;
		if (retries > XHI_MAX_RETRIES)
			break;
	}

	dev_dbg(drvdata->dev, "done fifo_set_configuration\n");

	if (remaining_words != 0)
		return -EIO;

	return 0;
}

int fifo_icap_get_configuration(struct hwicap_drvdata *drvdata,
		u32 *frame_buffer, u32 num_words)
{

	u32 read_fifo_occupancy = 0;
	u32 retries = 0;
	u32 *data = frame_buffer;
	u32 remaining_words;
	u32 words_to_read;

	dev_dbg(drvdata->dev, "fifo_get_configuration\n");

	if (fifo_icap_busy(drvdata))
		return -EBUSY;

	remaining_words = num_words;

	while (remaining_words > 0) {
		words_to_read = remaining_words;
		if (words_to_read > XHI_MAX_READ_TRANSACTION_WORDS)
			words_to_read = XHI_MAX_READ_TRANSACTION_WORDS;

		remaining_words -= words_to_read;

		fifo_icap_set_read_size(drvdata, words_to_read);
		fifo_icap_start_readback(drvdata);

		while (words_to_read > 0) {
			
			while (read_fifo_occupancy == 0) {
				read_fifo_occupancy =
					fifo_icap_read_fifo_occupancy(drvdata);
				retries++;
				if (retries > XHI_MAX_RETRIES)
					return -EIO;
			}

			if (read_fifo_occupancy > words_to_read)
				read_fifo_occupancy = words_to_read;

			words_to_read -= read_fifo_occupancy;

			
			while (read_fifo_occupancy != 0) {
				*data++ = fifo_icap_fifo_read(drvdata);
				read_fifo_occupancy--;
			}
		}
	}

	dev_dbg(drvdata->dev, "done fifo_get_configuration\n");

	return 0;
}

void fifo_icap_reset(struct hwicap_drvdata *drvdata)
{
	u32 reg_data;
	reg_data = in_be32(drvdata->base_address + XHI_CR_OFFSET);

	out_be32(drvdata->base_address + XHI_CR_OFFSET,
				reg_data | XHI_CR_SW_RESET_MASK);

	out_be32(drvdata->base_address + XHI_CR_OFFSET,
				reg_data & (~XHI_CR_SW_RESET_MASK));

}

void fifo_icap_flush_fifo(struct hwicap_drvdata *drvdata)
{
	u32 reg_data;
	reg_data = in_be32(drvdata->base_address + XHI_CR_OFFSET);

	out_be32(drvdata->base_address + XHI_CR_OFFSET,
				reg_data | XHI_CR_FIFO_CLR_MASK);

	out_be32(drvdata->base_address + XHI_CR_OFFSET,
				reg_data & (~XHI_CR_FIFO_CLR_MASK));
}

