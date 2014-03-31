/*
 * Copyright (c) 2006, 2007, 2008, 2009 QLogic Corporation. All rights reserved.
 * Copyright (c) 2003, 2004, 2005, 2006 PathScale, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>

#include "qib.h"

/*
 * QLogic_IB "Two Wire Serial Interface" driver.
 * Originally written for a not-quite-i2c serial eeprom, which is
 * still used on some supported boards. Later boards have added a
 * variety of other uses, most board-specific, so the bit-boffing
 * part has been split off to this file, while the other parts
 * have been moved to chip-specific files.
 *
 * We have also dropped all pretense of fully generic (e.g. pretend
 * we don't know whether '1' is the higher voltage) interface, as
 * the restrictions of the generic i2c interface (e.g. no access from
 * driver itself) make it unsuitable for this use.
 */

#define READ_CMD 1
#define WRITE_CMD 0

static void i2c_wait_for_writes(struct qib_devdata *dd)
{
	dd->f_gpio_mod(dd, 0, 0, 0);
	rmb(); 
}

#define SCL_WAIT_USEC 1000

#define TWSI_BUF_WAIT_USEC 60

static void scl_out(struct qib_devdata *dd, u8 bit)
{
	u32 mask;

	udelay(1);

	mask = 1UL << dd->gpio_scl_num;

	
	dd->f_gpio_mod(dd, 0, bit ? 0 : mask, mask);

	if (!bit)
		udelay(2);
	else {
		int rise_usec;
		for (rise_usec = SCL_WAIT_USEC; rise_usec > 0; rise_usec -= 2) {
			if (mask & dd->f_gpio_mod(dd, 0, 0, 0))
				break;
			udelay(2);
		}
		if (rise_usec <= 0)
			qib_dev_err(dd, "SCL interface stuck low > %d uSec\n",
				    SCL_WAIT_USEC);
	}
	i2c_wait_for_writes(dd);
}

static void sda_out(struct qib_devdata *dd, u8 bit)
{
	u32 mask;

	mask = 1UL << dd->gpio_sda_num;

	
	dd->f_gpio_mod(dd, 0, bit ? 0 : mask, mask);

	i2c_wait_for_writes(dd);
	udelay(2);
}

static u8 sda_in(struct qib_devdata *dd, int wait)
{
	int bnum;
	u32 read_val, mask;

	bnum = dd->gpio_sda_num;
	mask = (1UL << bnum);
	
	dd->f_gpio_mod(dd, 0, 0, mask);
	read_val = dd->f_gpio_mod(dd, 0, 0, 0);
	if (wait)
		i2c_wait_for_writes(dd);
	return (read_val & mask) >> bnum;
}

static int i2c_ackrcv(struct qib_devdata *dd)
{
	u8 ack_received;

	
	
	ack_received = sda_in(dd, 1);
	scl_out(dd, 1);
	ack_received = sda_in(dd, 1) == 0;
	scl_out(dd, 0);
	return ack_received;
}

static void stop_cmd(struct qib_devdata *dd);

static int rd_byte(struct qib_devdata *dd, int last)
{
	int bit_cntr, data;

	data = 0;

	for (bit_cntr = 7; bit_cntr >= 0; --bit_cntr) {
		data <<= 1;
		scl_out(dd, 1);
		data |= sda_in(dd, 0);
		scl_out(dd, 0);
	}
	if (last) {
		scl_out(dd, 1);
		stop_cmd(dd);
	} else {
		sda_out(dd, 0);
		scl_out(dd, 1);
		scl_out(dd, 0);
		sda_out(dd, 1);
	}
	return data;
}

static int wr_byte(struct qib_devdata *dd, u8 data)
{
	int bit_cntr;
	u8 bit;

	for (bit_cntr = 7; bit_cntr >= 0; bit_cntr--) {
		bit = (data >> bit_cntr) & 1;
		sda_out(dd, bit);
		scl_out(dd, 1);
		scl_out(dd, 0);
	}
	return (!i2c_ackrcv(dd)) ? 1 : 0;
}

static void start_seq(struct qib_devdata *dd)
{
	sda_out(dd, 1);
	scl_out(dd, 1);
	sda_out(dd, 0);
	udelay(1);
	scl_out(dd, 0);
}

static void stop_seq(struct qib_devdata *dd)
{
	scl_out(dd, 0);
	sda_out(dd, 0);
	scl_out(dd, 1);
	sda_out(dd, 1);
}

static void stop_cmd(struct qib_devdata *dd)
{
	stop_seq(dd);
	udelay(TWSI_BUF_WAIT_USEC);
}


int qib_twsi_reset(struct qib_devdata *dd)
{
	int clock_cycles_left = 9;
	int was_high = 0;
	u32 pins, mask;

	mask = (1UL << dd->gpio_scl_num) | (1UL << dd->gpio_sda_num);

	dd->f_gpio_mod(dd, 0, 0, mask);

	while (clock_cycles_left--) {
		scl_out(dd, 0);
		scl_out(dd, 1);
		
		was_high |= sda_in(dd, 0);
	}

	if (was_high) {

		pins = dd->f_gpio_mod(dd, 0, 0, 0);
		if ((pins & mask) != mask)
			qib_dev_err(dd, "GPIO pins not at rest: %d\n",
				    pins & mask);
		
		udelay(1); 
		sda_out(dd, 0);
		udelay(1); 
		
		sda_out(dd, 1);
		udelay(TWSI_BUF_WAIT_USEC);
	}

	return !was_high;
}

#define QIB_TWSI_START 0x100
#define QIB_TWSI_STOP 0x200

static int qib_twsi_wr(struct qib_devdata *dd, int data, int flags)
{
	int ret = 1;
	if (flags & QIB_TWSI_START)
		start_seq(dd);

	ret = wr_byte(dd, data); 

	if (flags & QIB_TWSI_STOP)
		stop_cmd(dd);
	return ret;
}

#define QIB_TEMP_DEV 0x98

int qib_twsi_blk_rd(struct qib_devdata *dd, int dev, int addr,
		    void *buffer, int len)
{
	int ret;
	u8 *bp = buffer;

	ret = 1;

	if (dev == QIB_TWSI_NO_DEV) {
		
		addr = (addr << 1) | READ_CMD;
		ret = qib_twsi_wr(dd, addr, QIB_TWSI_START);
	} else {
		
		ret = qib_twsi_wr(dd, dev | WRITE_CMD, QIB_TWSI_START);
		if (ret) {
			stop_cmd(dd);
			ret = 1;
			goto bail;
		}
		ret = qib_twsi_wr(dd, addr, 0);
		udelay(TWSI_BUF_WAIT_USEC);

		if (ret) {
			qib_dev_err(dd,
				"Failed to write interface read addr %02X\n",
				addr);
			ret = 1;
			goto bail;
		}
		ret = qib_twsi_wr(dd, dev | READ_CMD, QIB_TWSI_START);
	}
	if (ret) {
		stop_cmd(dd);
		ret = 1;
		goto bail;
	}

	while (len-- > 0) {
		*bp++ = rd_byte(dd, !len);
	}

	ret = 0;

bail:
	return ret;
}

/*
 * qib_twsi_blk_wr
 * Formerly called qib_eeprom_internal_write, and only used for eeprom,
 * but now the general interface for data transfer to twsi devices.
 * One vestige of its former role is that it recognizes a device
 * QIB_TWSI_NO_DEV and does the correct operation for the legacy part,
 * which responded to all TWSI device codes, interpreting them as
 * address within device. On all other devices found on board handled by
 * this driver, the device is followed by a one-byte "address" which selects
 * the "register" or "offset" within the device to which data should
 * be written.
 */
int qib_twsi_blk_wr(struct qib_devdata *dd, int dev, int addr,
		    const void *buffer, int len)
{
	int sub_len;
	const u8 *bp = buffer;
	int max_wait_time, i;
	int ret;
	ret = 1;

	while (len > 0) {
		if (dev == QIB_TWSI_NO_DEV) {
			if (qib_twsi_wr(dd, (addr << 1) | WRITE_CMD,
					QIB_TWSI_START)) {
				goto failed_write;
			}
		} else {
			
			if (qib_twsi_wr(dd, dev | WRITE_CMD, QIB_TWSI_START))
				goto failed_write;
			ret = qib_twsi_wr(dd, addr, 0);
			if (ret) {
				qib_dev_err(dd, "Failed to write interface"
					    " write addr %02X\n", addr);
				goto failed_write;
			}
		}

		sub_len = min(len, 4);
		addr += sub_len;
		len -= sub_len;

		for (i = 0; i < sub_len; i++)
			if (qib_twsi_wr(dd, *bp++, 0))
				goto failed_write;

		stop_cmd(dd);

		max_wait_time = 100;
		while (qib_twsi_wr(dd, dev | READ_CMD, QIB_TWSI_START)) {
			stop_cmd(dd);
			if (!--max_wait_time)
				goto failed_write;
		}
		
		rd_byte(dd, 1);
	}

	ret = 0;
	goto bail;

failed_write:
	stop_cmd(dd);
	ret = 1;

bail:
	return ret;
}
