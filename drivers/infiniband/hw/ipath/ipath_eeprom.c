/*
 * Copyright (c) 2006, 2007, 2008 QLogic Corporation. All rights reserved.
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

#include "ipath_kernel.h"


#define IPATH_EEPROM_DEV_V1 0xA0
#define IPATH_EEPROM_DEV_V2 0xA2
#define IPATH_TEMP_DEV 0x98
#define IPATH_BAD_DEV (IPATH_EEPROM_DEV_V2+2)
#define IPATH_NO_DEV (0xFF)

static struct i2c_chain_desc {
	u8 probe_dev;	
	u8 eeprom_dev;	
	u8 temp_dev;	
} i2c_chains[] = {
	{ IPATH_BAD_DEV, IPATH_NO_DEV, IPATH_NO_DEV }, 
	{ IPATH_EEPROM_DEV_V1, IPATH_EEPROM_DEV_V1, IPATH_TEMP_DEV}, 
	{ IPATH_EEPROM_DEV_V2, IPATH_EEPROM_DEV_V2, IPATH_TEMP_DEV}, 
	{ IPATH_NO_DEV }
};

enum i2c_type {
	i2c_line_scl = 0,
	i2c_line_sda
};

enum i2c_state {
	i2c_line_low = 0,
	i2c_line_high
};

#define READ_CMD 1
#define WRITE_CMD 0

static int i2c_gpio_set(struct ipath_devdata *dd,
			enum i2c_type line,
			enum i2c_state new_line_state)
{
	u64 out_mask, dir_mask, *gpioval;
	unsigned long flags = 0;

	gpioval = &dd->ipath_gpio_out;

	if (line == i2c_line_scl) {
		dir_mask = dd->ipath_gpio_scl;
		out_mask = (1UL << dd->ipath_gpio_scl_num);
	} else {
		dir_mask = dd->ipath_gpio_sda;
		out_mask = (1UL << dd->ipath_gpio_sda_num);
	}

	spin_lock_irqsave(&dd->ipath_gpio_lock, flags);
	if (new_line_state == i2c_line_high) {
		
		dd->ipath_extctrl &= ~dir_mask;
	} else {
		
		dd->ipath_extctrl |= dir_mask;
	}
	ipath_write_kreg(dd, dd->ipath_kregs->kr_extctrl, dd->ipath_extctrl);

	
	if (new_line_state == i2c_line_high)
		*gpioval |= out_mask;
	else
		*gpioval &= ~out_mask;

	ipath_write_kreg(dd, dd->ipath_kregs->kr_gpio_out, *gpioval);
	spin_unlock_irqrestore(&dd->ipath_gpio_lock, flags);

	return 0;
}

static int i2c_gpio_get(struct ipath_devdata *dd,
			enum i2c_type line,
			enum i2c_state *curr_statep)
{
	u64 read_val, mask;
	int ret;
	unsigned long flags = 0;

	
	if (curr_statep == NULL) {
		ret = 1;
		goto bail;
	}

	
	if (line == i2c_line_scl)
		mask = dd->ipath_gpio_scl;
	else
		mask = dd->ipath_gpio_sda;

	spin_lock_irqsave(&dd->ipath_gpio_lock, flags);
	dd->ipath_extctrl &= ~mask;
	ipath_write_kreg(dd, dd->ipath_kregs->kr_extctrl, dd->ipath_extctrl);
	read_val = ipath_read_kreg64(dd, dd->ipath_kregs->kr_extstatus);
	spin_unlock_irqrestore(&dd->ipath_gpio_lock, flags);

	if (read_val & mask)
		*curr_statep = i2c_line_high;
	else
		*curr_statep = i2c_line_low;

	ret = 0;

bail:
	return ret;
}

static void i2c_wait_for_writes(struct ipath_devdata *dd)
{
	(void)ipath_read_kreg32(dd, dd->ipath_kregs->kr_scratch);
	rmb();
}

static void scl_out(struct ipath_devdata *dd, u8 bit)
{
	udelay(1);
	i2c_gpio_set(dd, i2c_line_scl, bit ? i2c_line_high : i2c_line_low);

	i2c_wait_for_writes(dd);
}

static void sda_out(struct ipath_devdata *dd, u8 bit)
{
	i2c_gpio_set(dd, i2c_line_sda, bit ? i2c_line_high : i2c_line_low);

	i2c_wait_for_writes(dd);
}

static u8 sda_in(struct ipath_devdata *dd, int wait)
{
	enum i2c_state bit;

	if (i2c_gpio_get(dd, i2c_line_sda, &bit))
		ipath_dbg("get bit failed!\n");

	if (wait)
		i2c_wait_for_writes(dd);

	return bit == i2c_line_high ? 1U : 0;
}

static int i2c_ackrcv(struct ipath_devdata *dd)
{
	u8 ack_received;

	
	
	ack_received = sda_in(dd, 1);
	scl_out(dd, i2c_line_high);
	ack_received = sda_in(dd, 1) == 0;
	scl_out(dd, i2c_line_low);
	return ack_received;
}

static int rd_byte(struct ipath_devdata *dd)
{
	int bit_cntr, data;

	data = 0;

	for (bit_cntr = 7; bit_cntr >= 0; --bit_cntr) {
		data <<= 1;
		scl_out(dd, i2c_line_high);
		data |= sda_in(dd, 0);
		scl_out(dd, i2c_line_low);
	}
	return data;
}

static int wr_byte(struct ipath_devdata *dd, u8 data)
{
	int bit_cntr;
	u8 bit;

	for (bit_cntr = 7; bit_cntr >= 0; bit_cntr--) {
		bit = (data >> bit_cntr) & 1;
		sda_out(dd, bit);
		scl_out(dd, i2c_line_high);
		scl_out(dd, i2c_line_low);
	}
	return (!i2c_ackrcv(dd)) ? 1 : 0;
}

static void send_ack(struct ipath_devdata *dd)
{
	sda_out(dd, i2c_line_low);
	scl_out(dd, i2c_line_high);
	scl_out(dd, i2c_line_low);
	sda_out(dd, i2c_line_high);
}

static int i2c_startcmd(struct ipath_devdata *dd, u8 offset_dir)
{
	int res;

	
	sda_out(dd, i2c_line_high);
	scl_out(dd, i2c_line_high);
	sda_out(dd, i2c_line_low);
	scl_out(dd, i2c_line_low);

	
	res = wr_byte(dd, offset_dir);

	if (res)
		ipath_cdbg(VERBOSE, "No ack to complete start\n");

	return res;
}

static void stop_cmd(struct ipath_devdata *dd)
{
	scl_out(dd, i2c_line_low);
	sda_out(dd, i2c_line_low);
	scl_out(dd, i2c_line_high);
	sda_out(dd, i2c_line_high);
	udelay(2);
}


static int eeprom_reset(struct ipath_devdata *dd)
{
	int clock_cycles_left = 9;
	u64 *gpioval = &dd->ipath_gpio_out;
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&dd->ipath_gpio_lock, flags);
	
	dd->ipath_extctrl = ipath_read_kreg64(dd, dd->ipath_kregs->kr_extctrl);
	*gpioval = ipath_read_kreg64(dd, dd->ipath_kregs->kr_gpio_out);
	spin_unlock_irqrestore(&dd->ipath_gpio_lock, flags);

	ipath_cdbg(VERBOSE, "Resetting i2c eeprom; initial gpioout reg "
		   "is %llx\n", (unsigned long long) *gpioval);

	scl_out(dd, i2c_line_low);
	sda_out(dd, i2c_line_high);

	
	while (clock_cycles_left--) {
		scl_out(dd, i2c_line_high);

		
		if (sda_in(dd, 0)) {
			sda_out(dd, i2c_line_low);
			scl_out(dd, i2c_line_low);
			
			scl_out(dd, i2c_line_high);
			sda_out(dd, i2c_line_high);
			ret = 0;
			goto bail;
		}

		scl_out(dd, i2c_line_low);
	}

	ret = 1;

bail:
	return ret;
}

static int i2c_probe(struct ipath_devdata *dd, int devaddr)
{
	int ret = 0;

	ret = eeprom_reset(dd);
	if (ret) {
		ipath_dev_err(dd, "Failed reset probing device 0x%02X\n",
			      devaddr);
		return ret;
	}
	ret = i2c_startcmd(dd, devaddr | READ_CMD);
	if (ret)
		ipath_cdbg(VERBOSE, "Failed startcmd for device 0x%02X\n",
			   devaddr);
	else {
		int data;
		data = rd_byte(dd);
		stop_cmd(dd);
		ipath_cdbg(VERBOSE, "Response from device 0x%02X\n", devaddr);
	}
	return ret;
}

static struct i2c_chain_desc *ipath_i2c_type(struct ipath_devdata *dd)
{
	int idx;

	
	idx = dd->ipath_i2c_chain_type - 1;
	if (idx >= 0 && idx < (ARRAY_SIZE(i2c_chains) - 1))
		goto done;

	idx = 0;
	while (i2c_chains[idx].probe_dev != IPATH_NO_DEV) {
		
		if (!i2c_probe(dd, i2c_chains[idx].probe_dev))
			break;
		++idx;
	}

	if (idx == 0)
		eeprom_reset(dd);

	if (i2c_chains[idx].probe_dev == IPATH_NO_DEV)
		idx = -1;
	else
		dd->ipath_i2c_chain_type = idx + 1;
done:
	return (idx >= 0) ? i2c_chains + idx : NULL;
}

static int ipath_eeprom_internal_read(struct ipath_devdata *dd,
					u8 eeprom_offset, void *buffer, int len)
{
	int ret;
	struct i2c_chain_desc *icd;
	u8 *bp = buffer;

	ret = 1;
	icd = ipath_i2c_type(dd);
	if (!icd)
		goto bail;

	if (icd->eeprom_dev == IPATH_NO_DEV) {
		
		ipath_cdbg(VERBOSE, "Start command only address\n");
		eeprom_offset = (eeprom_offset << 1) | READ_CMD;
		ret = i2c_startcmd(dd, eeprom_offset);
	} else {
		
		ipath_cdbg(VERBOSE, "Start command uses devaddr\n");
		if (i2c_startcmd(dd, icd->eeprom_dev | WRITE_CMD)) {
			ipath_dbg("Failed EEPROM startcmd\n");
			stop_cmd(dd);
			ret = 1;
			goto bail;
		}
		ret = wr_byte(dd, eeprom_offset);
		stop_cmd(dd);
		if (ret) {
			ipath_dev_err(dd, "Failed to write EEPROM address\n");
			ret = 1;
			goto bail;
		}
		ret = i2c_startcmd(dd, icd->eeprom_dev | READ_CMD);
	}
	if (ret) {
		ipath_dbg("Failed startcmd for dev %02X\n", icd->eeprom_dev);
		stop_cmd(dd);
		ret = 1;
		goto bail;
	}

	while (len-- > 0) {
		
		*bp++ = rd_byte(dd);
		
		if (len)
			send_ack(dd);
	}

	stop_cmd(dd);

	ret = 0;

bail:
	return ret;
}

static int ipath_eeprom_internal_write(struct ipath_devdata *dd, u8 eeprom_offset,
				       const void *buffer, int len)
{
	int sub_len;
	const u8 *bp = buffer;
	int max_wait_time, i;
	int ret;
	struct i2c_chain_desc *icd;

	ret = 1;
	icd = ipath_i2c_type(dd);
	if (!icd)
		goto bail;

	while (len > 0) {
		if (icd->eeprom_dev == IPATH_NO_DEV) {
			if (i2c_startcmd(dd,
					 (eeprom_offset << 1) | WRITE_CMD)) {
				ipath_dbg("Failed to start cmd offset %u\n",
					eeprom_offset);
				goto failed_write;
			}
		} else {
			
			if (i2c_startcmd(dd, icd->eeprom_dev | WRITE_CMD)) {
				ipath_dbg("Failed EEPROM startcmd\n");
				goto failed_write;
			}
			ret = wr_byte(dd, eeprom_offset);
			if (ret) {
				ipath_dev_err(dd, "Failed to write EEPROM "
					      "address\n");
				goto failed_write;
			}
		}

		sub_len = min(len, 4);
		eeprom_offset += sub_len;
		len -= sub_len;

		for (i = 0; i < sub_len; i++) {
			if (wr_byte(dd, *bp++)) {
				ipath_dbg("no ack after byte %u/%u (%u "
					  "total remain)\n", i, sub_len,
					  len + sub_len - i);
				goto failed_write;
			}
		}

		stop_cmd(dd);

		max_wait_time = 100;
		while (i2c_startcmd(dd, icd->eeprom_dev | READ_CMD)) {
			stop_cmd(dd);
			if (!--max_wait_time) {
				ipath_dbg("Did not get successful read to "
					  "complete write\n");
				goto failed_write;
			}
		}
		
		rd_byte(dd);
		stop_cmd(dd);
	}

	ret = 0;
	goto bail;

failed_write:
	stop_cmd(dd);
	ret = 1;

bail:
	return ret;
}

int ipath_eeprom_read(struct ipath_devdata *dd, u8 eeprom_offset,
			void *buff, int len)
{
	int ret;

	ret = mutex_lock_interruptible(&dd->ipath_eep_lock);
	if (!ret) {
		ret = ipath_eeprom_internal_read(dd, eeprom_offset, buff, len);
		mutex_unlock(&dd->ipath_eep_lock);
	}

	return ret;
}

int ipath_eeprom_write(struct ipath_devdata *dd, u8 eeprom_offset,
			const void *buff, int len)
{
	int ret;

	ret = mutex_lock_interruptible(&dd->ipath_eep_lock);
	if (!ret) {
		ret = ipath_eeprom_internal_write(dd, eeprom_offset, buff, len);
		mutex_unlock(&dd->ipath_eep_lock);
	}

	return ret;
}

static u8 flash_csum(struct ipath_flash *ifp, int adjust)
{
	u8 *ip = (u8 *) ifp;
	u8 csum = 0, len;

	len = ifp->if_length;
	if (len > sizeof(struct ipath_flash))
		len = sizeof(struct ipath_flash);
	while (len--)
		csum += *ip++;
	csum -= ifp->if_csum;
	csum = ~csum;
	if (adjust)
		ifp->if_csum = csum;

	return csum;
}

void ipath_get_eeprom_info(struct ipath_devdata *dd)
{
	void *buf;
	struct ipath_flash *ifp;
	__be64 guid;
	int len, eep_stat;
	u8 csum, *bguid;
	int t = dd->ipath_unit;
	struct ipath_devdata *dd0 = ipath_lookup(0);

	if (t && dd0->ipath_nguid > 1 && t <= dd0->ipath_nguid) {
		u8 oguid;
		dd->ipath_guid = dd0->ipath_guid;
		bguid = (u8 *) & dd->ipath_guid;

		oguid = bguid[7];
		bguid[7] += t;
		if (oguid > bguid[7]) {
			if (bguid[6] == 0xff) {
				if (bguid[5] == 0xff) {
					ipath_dev_err(
						dd,
						"Can't set %s GUID from "
						"base, wraps to OUI!\n",
						ipath_get_unit_name(t));
					dd->ipath_guid = 0;
					goto bail;
				}
				bguid[5]++;
			}
			bguid[6]++;
		}
		dd->ipath_nguid = 1;

		ipath_dbg("nguid %u, so adding %u to device 0 guid, "
			  "for %llx\n",
			  dd0->ipath_nguid, t,
			  (unsigned long long) be64_to_cpu(dd->ipath_guid));
		goto bail;
	}

	/*
	 * read full flash, not just currently used part, since it may have
	 * been written with a newer definition
	 * */
	len = sizeof(struct ipath_flash);
	buf = vmalloc(len);
	if (!buf) {
		ipath_dev_err(dd, "Couldn't allocate memory to read %u "
			      "bytes from eeprom for GUID\n", len);
		goto bail;
	}

	mutex_lock(&dd->ipath_eep_lock);
	eep_stat = ipath_eeprom_internal_read(dd, 0, buf, len);
	mutex_unlock(&dd->ipath_eep_lock);

	if (eep_stat) {
		ipath_dev_err(dd, "Failed reading GUID from eeprom\n");
		goto done;
	}
	ifp = (struct ipath_flash *)buf;

	csum = flash_csum(ifp, 0);
	if (csum != ifp->if_csum) {
		dev_info(&dd->pcidev->dev, "Bad I2C flash checksum: "
			 "0x%x, not 0x%x\n", csum, ifp->if_csum);
		goto done;
	}
	if (*(__be64 *) ifp->if_guid == cpu_to_be64(0) ||
	    *(__be64 *) ifp->if_guid == ~cpu_to_be64(0)) {
		ipath_dev_err(dd, "Invalid GUID %llx from flash; "
			      "ignoring\n",
			      *(unsigned long long *) ifp->if_guid);
		
		goto done;
	}

	
	if (*(u64 *) ifp->if_guid == 0x100007511000000ULL)
		dev_info(&dd->pcidev->dev, "Warning, GUID %llx is "
			 "default, probably not correct!\n",
			 *(unsigned long long *) ifp->if_guid);

	bguid = ifp->if_guid;
	if (!bguid[0] && !bguid[1] && !bguid[2]) {
		bguid[1] = bguid[3];
		bguid[2] = bguid[4];
		bguid[3] = bguid[4] = 0;
		guid = *(__be64 *) ifp->if_guid;
		ipath_cdbg(VERBOSE, "Old GUID format in flash, top 3 zero, "
			   "shifting 2 octets\n");
	} else
		guid = *(__be64 *) ifp->if_guid;
	dd->ipath_guid = guid;
	dd->ipath_nguid = ifp->if_numguid;
	if ((ifp->if_fversion > 1) && ifp->if_sprefix[0]
		&& ((u8 *)ifp->if_sprefix)[0] != 0xFF) {
		char *snp = dd->ipath_serial;
		memcpy(snp, ifp->if_sprefix, sizeof ifp->if_sprefix);
		snp[sizeof ifp->if_sprefix] = '\0';
		len = strlen(snp);
		snp += len;
		len = (sizeof dd->ipath_serial) - len;
		if (len > sizeof ifp->if_serial) {
			len = sizeof ifp->if_serial;
		}
		memcpy(snp, ifp->if_serial, len);
	} else
		memcpy(dd->ipath_serial, ifp->if_serial,
		       sizeof ifp->if_serial);
	if (!strstr(ifp->if_comment, "Tested successfully"))
		ipath_dev_err(dd, "Board SN %s did not pass functional "
			"test: %s\n", dd->ipath_serial,
			ifp->if_comment);

	ipath_cdbg(VERBOSE, "Initted GUID to %llx from eeprom\n",
		   (unsigned long long) be64_to_cpu(dd->ipath_guid));

	memcpy(&dd->ipath_eep_st_errs, &ifp->if_errcntp, IPATH_EEP_LOG_CNT);
	atomic_set(&dd->ipath_active_time, 0);
	dd->ipath_eep_hrs = ifp->if_powerhour[0] | (ifp->if_powerhour[1] << 8);

done:
	vfree(buf);

bail:;
}


int ipath_update_eeprom_log(struct ipath_devdata *dd)
{
	void *buf;
	struct ipath_flash *ifp;
	int len, hi_water;
	uint32_t new_time, new_hrs;
	u8 csum;
	int ret, idx;
	unsigned long flags;

	
	ret = 0;
	for (idx = 0; idx < IPATH_EEP_LOG_CNT; ++idx) {
		if (dd->ipath_eep_st_new_errs[idx]) {
			ret = 1;
			break;
		}
	}
	new_time = atomic_read(&dd->ipath_active_time);

	if (ret == 0 && new_time < 3600)
		return 0;

	/*
	 * The quick-check above determined that there is something worthy
	 * of logging, so get current contents and do a more detailed idea.
	 * read full flash, not just currently used part, since it may have
	 * been written with a newer definition
	 */
	len = sizeof(struct ipath_flash);
	buf = vmalloc(len);
	ret = 1;
	if (!buf) {
		ipath_dev_err(dd, "Couldn't allocate memory to read %u "
				"bytes from eeprom for logging\n", len);
		goto bail;
	}

	ret = mutex_lock_interruptible(&dd->ipath_eep_lock);
	if (ret) {
		ipath_dev_err(dd, "Unable to acquire EEPROM for logging\n");
		goto free_bail;
	}
	ret = ipath_eeprom_internal_read(dd, 0, buf, len);
	if (ret) {
		mutex_unlock(&dd->ipath_eep_lock);
		ipath_dev_err(dd, "Unable read EEPROM for logging\n");
		goto free_bail;
	}
	ifp = (struct ipath_flash *)buf;

	csum = flash_csum(ifp, 0);
	if (csum != ifp->if_csum) {
		mutex_unlock(&dd->ipath_eep_lock);
		ipath_dev_err(dd, "EEPROM cks err (0x%02X, S/B 0x%02X)\n",
				csum, ifp->if_csum);
		ret = 1;
		goto free_bail;
	}
	hi_water = 0;
	spin_lock_irqsave(&dd->ipath_eep_st_lock, flags);
	for (idx = 0; idx < IPATH_EEP_LOG_CNT; ++idx) {
		int new_val = dd->ipath_eep_st_new_errs[idx];
		if (new_val) {
			new_val += ifp->if_errcntp[idx];
			if (new_val > 0xFF)
				new_val = 0xFF;
			if (ifp->if_errcntp[idx] != new_val) {
				ifp->if_errcntp[idx] = new_val;
				hi_water = offsetof(struct ipath_flash,
						if_errcntp) + idx;
			}
			dd->ipath_eep_st_errs[idx] = new_val;
			dd->ipath_eep_st_new_errs[idx] = 0;
		}
	}
	if (new_time >= 3600) {
		new_hrs = new_time / 3600;
		atomic_sub((new_hrs * 3600), &dd->ipath_active_time);
		new_hrs += dd->ipath_eep_hrs;
		if (new_hrs > 0xFFFF)
			new_hrs = 0xFFFF;
		dd->ipath_eep_hrs = new_hrs;
		if ((new_hrs & 0xFF) != ifp->if_powerhour[0]) {
			ifp->if_powerhour[0] = new_hrs & 0xFF;
			hi_water = offsetof(struct ipath_flash, if_powerhour);
		}
		if ((new_hrs >> 8) != ifp->if_powerhour[1]) {
			ifp->if_powerhour[1] = new_hrs >> 8;
			hi_water = offsetof(struct ipath_flash, if_powerhour)
					+ 1;
		}
	}
	spin_unlock_irqrestore(&dd->ipath_eep_st_lock, flags);
	if (hi_water) {
		
		csum = flash_csum(ifp, 1);
		ret = ipath_eeprom_internal_write(dd, 0, buf, hi_water + 1);
	}
	mutex_unlock(&dd->ipath_eep_lock);
	if (ret)
		ipath_dev_err(dd, "Failed updating EEPROM\n");

free_bail:
	vfree(buf);
bail:
	return ret;

}


void ipath_inc_eeprom_err(struct ipath_devdata *dd, u32 eidx, u32 incr)
{
	uint new_val;
	unsigned long flags;

	spin_lock_irqsave(&dd->ipath_eep_st_lock, flags);
	new_val = dd->ipath_eep_st_new_errs[eidx] + incr;
	if (new_val > 255)
		new_val = 255;
	dd->ipath_eep_st_new_errs[eidx] = new_val;
	spin_unlock_irqrestore(&dd->ipath_eep_st_lock, flags);
	return;
}

static int ipath_tempsense_internal_read(struct ipath_devdata *dd, u8 regnum)
{
	int ret;
	struct i2c_chain_desc *icd;

	ret = -ENOENT;

	icd = ipath_i2c_type(dd);
	if (!icd)
		goto bail;

	if (icd->temp_dev == IPATH_NO_DEV) {
		
		ret = -ENXIO;
		goto bail;
	}

	if (i2c_startcmd(dd, icd->temp_dev | WRITE_CMD)) {
		ipath_dbg("Failed tempsense startcmd\n");
		stop_cmd(dd);
		ret = -ENXIO;
		goto bail;
	}
	ret = wr_byte(dd, regnum);
	stop_cmd(dd);
	if (ret) {
		ipath_dev_err(dd, "Failed tempsense WR command %02X\n",
			      regnum);
		ret = -ENXIO;
		goto bail;
	}
	if (i2c_startcmd(dd, icd->temp_dev | READ_CMD)) {
		ipath_dbg("Failed tempsense RD startcmd\n");
		stop_cmd(dd);
		ret = -ENXIO;
		goto bail;
	}
	ret = rd_byte(dd);
	stop_cmd(dd);

bail:
	return ret;
}

#define VALID_TS_RD_REG_MASK 0xBF

int ipath_tempsense_read(struct ipath_devdata *dd, u8 regnum)
{
	int ret;

	if (regnum > 7)
		return -EINVAL;

	
	if (!((1 << regnum) & VALID_TS_RD_REG_MASK))
		return 0;

	ret = mutex_lock_interruptible(&dd->ipath_eep_lock);
	if (!ret) {
		ret = ipath_tempsense_internal_read(dd, regnum);
		mutex_unlock(&dd->ipath_eep_lock);
	}

	return ret;
}

static int ipath_tempsense_internal_write(struct ipath_devdata *dd,
					  u8 regnum, u8 data)
{
	int ret = -ENOENT;
	struct i2c_chain_desc *icd;

	icd = ipath_i2c_type(dd);
	if (!icd)
		goto bail;

	if (icd->temp_dev == IPATH_NO_DEV) {
		
		ret = -ENXIO;
		goto bail;
	}
	if (i2c_startcmd(dd, icd->temp_dev | WRITE_CMD)) {
		ipath_dbg("Failed tempsense startcmd\n");
		stop_cmd(dd);
		ret = -ENXIO;
		goto bail;
	}
	ret = wr_byte(dd, regnum);
	if (ret) {
		stop_cmd(dd);
		ipath_dev_err(dd, "Failed to write tempsense command %02X\n",
			      regnum);
		ret = -ENXIO;
		goto bail;
	}
	ret = wr_byte(dd, data);
	stop_cmd(dd);
	ret = i2c_startcmd(dd, icd->temp_dev | READ_CMD);
	if (ret) {
		ipath_dev_err(dd, "Failed tempsense data wrt to %02X\n",
			      regnum);
		ret = -ENXIO;
	}

bail:
	return ret;
}

#define VALID_TS_WR_REG_MASK ((1 << 9) | (1 << 0xB) | (1 << 0xD))

int ipath_tempsense_write(struct ipath_devdata *dd, u8 regnum, u8 data)
{
	int ret;

	if (regnum > 15 || !((1 << regnum) & VALID_TS_WR_REG_MASK))
		return -EINVAL;

	ret = mutex_lock_interruptible(&dd->ipath_eep_lock);
	if (!ret) {
		ret = ipath_tempsense_internal_write(dd, regnum, data);
		mutex_unlock(&dd->ipath_eep_lock);
	}

	return ret;
}
