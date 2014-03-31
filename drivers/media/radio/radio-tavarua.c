/* Copyright (c) 2009-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define DRIVER_AUTHOR "Qualcomm"
#define DRIVER_NAME "radio-tavarua"
#define DRIVER_CARD "Qualcomm FM Radio Transceiver"
#define DRIVER_DESC "I2C radio driver for Qualcomm FM Radio Transceiver "
#define DRIVER_VERSION "1.0.0"

#include <linux/version.h>
#include <linux/init.h>         
#include <linux/delay.h>        
#include <linux/uaccess.h>      
#include <linux/kfifo.h>        
#include <linux/param.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/videodev2.h>
#include <linux/mutex.h>
#include <media/v4l2-common.h>
#include <asm/unaligned.h>
#include <media/v4l2-ioctl.h>
#include <linux/unistd.h>
#include <asm/atomic.h>
#include <media/tavarua.h>
#include <linux/mfd/marimba.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
struct region_params_t {
	enum tavarua_region_t region;
	unsigned int band_high;
	unsigned int band_low;
	char emphasis;
	char rds_std;
	char spacing;
};

struct srch_params_t {
	unsigned short srch_pi;
	unsigned char srch_pty;
	unsigned int preset_num;
	int get_list;
};

struct tavarua_device {
	struct video_device *videodev;
	
	atomic_t users;
	
	struct marimba *marimba;
	struct device *dev;
	
	struct marimba_fm_platform_data *pdata;
	unsigned int chipID;
	
	struct kfifo data_buf[TAVARUA_BUF_MAX];
	
	struct srch_params_t srch_params;
	
	int pending_xfrs[TAVARUA_XFR_MAX];
	int xfr_bytes_left;
	int xfr_in_progress;
	
	enum tavarua_xfr_ctrl_t tx_mode;
	
	unsigned char sync_xfr_regs[XFR_REG_NUM];
	struct completion sync_xfr_start;
	struct completion shutdown_done;
	struct completion sync_req_done;
	int tune_req;
	
	unsigned char registers[RADIO_REGISTERS];
	
	struct region_params_t region_params;
	
	int lp_mode;
	int handle_irq;
	
	struct mutex lock;
	
	spinlock_t buf_lock[TAVARUA_BUF_MAX];
	
	struct workqueue_struct *wqueue;
	struct delayed_work work;
	
	wait_queue_head_t event_queue;
	
	wait_queue_head_t read_queue;
	
	int pty;
	
	int pi;
	
	int ps_repeatcount;
	int enable_optimized_srch_alg;
	unsigned char spur_table_size;
	struct fm_spur_data spur_data;
	atomic_t validate_channel;
	unsigned char is_station_valid;
};


static int radio_nr = -1;
module_param(radio_nr, int, 0);
MODULE_PARM_DESC(radio_nr, "Radio Nr");
static int wait_timeout = WAIT_TIMEOUT;
static u8 bahama_version;
static unsigned int rds_buf = 100;
module_param(rds_buf, uint, 0);
MODULE_PARM_DESC(rds_buf, "RDS buffer entries: *100*");
static struct tavarua_device *private_data;
static int tavarua_disable_interrupts(struct tavarua_device *radio);
static int tavarua_setup_interrupts(struct tavarua_device *radio,
					enum radio_state_t state);
static int tavarua_start(struct tavarua_device *radio,
			enum radio_state_t state);
static int tavarua_request_irq(struct tavarua_device *radio);
static void start_pending_xfr(struct tavarua_device *radio);
static int update_spur_table(struct tavarua_device *radio);
static int xfr_rdwr_data(struct tavarua_device *radio, int op, int size,
	unsigned long offset, unsigned char *buf);
static int compute_MPX_DCC(struct tavarua_device *radio, int *val);

static void read_int_stat(struct work_struct *work);

static int is_bahama(void)
{
	int id = 0;

	switch (id = adie_get_detected_connectivity_type()) {
	case BAHAMA_ID:
		FMDBG("It is Bahama\n");
		return 1;

	case MARIMBA_ID:
		FMDBG("It is Marimba\n");
		return 0;
	default:
		printk(KERN_ERR "%s: unexpected adie connectivity type: %d\n",
			__func__, id);
		return -ENODEV;
	}
}

static int set_fm_slave_id(struct tavarua_device *radio)
{
	int bahama_present = is_bahama();

	if (bahama_present == -ENODEV)
		return -ENODEV;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}
	if (bahama_present)
		radio->marimba->mod_id = SLAVE_ID_BAHAMA_FM;
	else
		radio->marimba->mod_id = MARIMBA_SLAVE_ID_FM;

	return 0;
}

static irqreturn_t tavarua_isr(int irq, void *dev_id)
{
	struct tavarua_device *radio = dev_id;
	

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}
	queue_delayed_work(radio->wqueue, &radio->work,
				msecs_to_jiffies(TAVARUA_DELAY));
	return IRQ_HANDLED;
}


static int tavarua_read_registers(struct tavarua_device *radio,
				unsigned char offset, int len)
{
	int retval = 0, i = 0;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	retval = set_fm_slave_id(radio);

	if (retval == -ENODEV)
		return retval;

	FMDBG_I2C("I2C Slave: %x, Read Offset(%x): Data [",
						radio->marimba->mod_id,
						offset);

	retval =  marimba_read(radio->marimba, offset,
				&radio->registers[offset], len);

	if (retval > 0) {
		for (i = 0; i < len; i++)
			FMDBG_I2C("%02x ", radio->registers[offset+i]);
		FMDBG_I2C(" ]\n");

	}
	return retval;
}

/**
  This function is called to write a byte over the I2C interface.
  The corresponding shadow copy is stored in internal register status.

  @param radio: structure pointer passed by client.
  @param offset: register offset.
  @param value: buffer to be written to the registers.

  @return => 0 if successful.
  @return < 0 if failure.
*/
static int tavarua_write_register(struct tavarua_device *radio,
			unsigned char offset, unsigned char value)
{
	int retval;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	retval = set_fm_slave_id(radio);

	if (retval == -ENODEV)
		return retval;

	FMDBG_I2C("I2C Slave: %x, Write Offset(%x): Data[",
						radio->marimba->mod_id,
						offset);
	retval = marimba_write(radio->marimba, offset, &value, 1);
	if (retval > 0) {
		if (offset < RADIO_REGISTERS) {
			radio->registers[offset] = value;
			FMDBG_I2C("%02x ", radio->registers[offset]);
		}
		FMDBG_I2C(" ]\n");
	}
	return retval;
}

/**
  This function is called to write a number of bytes over the I2C interface.
  The corresponding shadow copy is stored in internal register status.

  @param radio: structure pointer passed by client.
  @param offset: register offset.
  @param buf: buffer to be written to the registers.
  @param len: num of bytes.

  @return => 0 if successful.
  @return < 0 if failure.
*/
static int tavarua_write_registers(struct tavarua_device *radio,
			unsigned char offset, unsigned char *buf, int len)
{

	int i;
	int retval;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	retval = set_fm_slave_id(radio);

	if (retval == -ENODEV)
		return retval;

	FMDBG_I2C("I2C Slave: %x, Write Offset(%x): Data[",
						radio->marimba->mod_id,
						offset);
	retval = marimba_write(radio->marimba, offset, buf, len);
	if (retval > 0) { 
		for (i = 0; i < len; i++) {
			if ((offset+i) < RADIO_REGISTERS) {
				radio->registers[offset+i] = buf[i];
				FMDBG_I2C("%x ",  radio->registers[offset+i]);
			}
		}
		FMDBG_I2C(" ]\n");
	}
	return retval;
}

static int read_data_blocks(struct tavarua_device *radio, unsigned char offset)
{
	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	
	return tavarua_read_registers(radio, offset, RDS_BLOCK*4);
}

static void tavarua_rds_read(struct tavarua_device *radio)
{
	struct kfifo *rds_buf;
	unsigned char blocknum;
	unsigned char tmp[3];

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return;
	}

	rds_buf = &radio->data_buf[TAVARUA_BUF_RAW_RDS];

	if (read_data_blocks(radio, RAW_RDS) < 0)
		return;
	 
	for (blocknum = 0; blocknum < RDS_BLOCKS_NUM; blocknum++) {
		
		put_unaligned(cpu_to_le16(radio->registers[RAW_RDS +
			blocknum*RDS_BLOCK]), (unsigned short *) tmp);
		tmp[2] = blocknum;		
		tmp[2] |= blocknum << 3;	
		tmp[2] |= 0x40; 

		
		kfifo_in_locked(rds_buf, tmp, 3, &radio->buf_lock[TAVARUA_BUF_RAW_RDS]);
	}
	
	if (kfifo_len(rds_buf))
		wake_up_interruptible(&radio->read_queue);

}

static int request_read_xfr(struct tavarua_device *radio,
				enum tavarua_xfr_ctrl_t mode){


	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	tavarua_write_register(radio, XFRCTRL, mode);
	msleep(TAVARUA_DELAY);
	return 0;
}

static int copy_from_xfr(struct tavarua_device *radio,
		enum tavarua_buf_t buf_type, unsigned int n){

	struct kfifo *data_fifo;
	unsigned char *xfr_regs;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	data_fifo = &radio->data_buf[buf_type];
	xfr_regs = &radio->registers[XFRCTRL+1];

	kfifo_in_locked(data_fifo, xfr_regs, n, &radio->buf_lock[buf_type]);
	return 0;
}

/**
  This function sets the desired MODE in the XFRCTRL register and it also sets
  the CTRL field and data to write.
  This also writes all the XFRDATx registers with the desired input buffer.

  NOTE:

  The Data Transfer (XFR) registers are used to pass various data and
  configuration parameters between the Core and host processor.

  To write data to the Core, the host processor updates XFRDAT0 - XFRDAT15 with
  the appropriate mode bytes. The host processor must then set the desired MODE
  in the XFRCTRL register and set the CTRL field to write. The core will detect
  that the XFRCTRL register was written to and will read the XFR mode bytes.
  After reading all the mode bytes, the Core will set the TRANSFER interrupt
  status bit and interrupt the host if the TRANSFERCTRL interrupt control bit
  is set.

  @param radio: structure pointer passed by client.
  @param mode: XFR mode to write in XFRCTRL register.
  @param buf: buffer to be written to the registers.
  @param len: num of bytes.

  @return => 0 if successful.
  @return < 0 if failure.
*/
static int write_to_xfr(struct tavarua_device *radio, unsigned char mode,
			char *buf, int len)
{
	char buffer[len+1];

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	memcpy(buffer+1, buf, len);
	buffer[0] = ((1<<7) | mode);
	return tavarua_write_registers(radio, XFRCTRL, buffer, sizeof(buffer));
}

static int xfr_intf_own(struct tavarua_device *radio)
{

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	mutex_lock(&radio->lock);
	if (radio->xfr_in_progress) {
		radio->pending_xfrs[TAVARUA_XFR_SYNC] = 1;
		mutex_unlock(&radio->lock);
		if (!wait_for_completion_timeout(&radio->sync_xfr_start,
			msecs_to_jiffies(wait_timeout)))
			return -ETIME;
	} else {
		FMDBG("gained ownership of xfr\n");
		radio->xfr_in_progress = 1;
		mutex_unlock(&radio->lock);
	}
	return 0;
}

static int sync_read_xfr(struct tavarua_device *radio,
			enum tavarua_xfr_ctrl_t xfr_type, unsigned char *buf)
{
	int retval;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	retval = xfr_intf_own(radio);
	if (retval < 0)
		return retval;
	retval = tavarua_write_register(radio, XFRCTRL, xfr_type);

	if (retval >= 0) {
		if (!wait_for_completion_timeout(&radio->sync_req_done,
			msecs_to_jiffies(wait_timeout)) || (retval < 0)) {
			retval = -ETIME;
		} else {
			memcpy(buf, radio->sync_xfr_regs, XFR_REG_NUM);
		}
	}
	radio->xfr_in_progress = 0;
	start_pending_xfr(radio);
	FMDBG("%s: %d\n", __func__, retval);
	return retval;
}

/**
  This function is used to do synchronous XFR write operation.

  @param radio: structure pointer passed by client.
  @param xfr_type: XFR mode to write in XFRCTRL register.
  @param buf: buffer to be written to the core.

  @return => 0 if successful.
  @return < 0 if failure.
*/
static int sync_write_xfr(struct tavarua_device *radio,
		enum tavarua_xfr_ctrl_t xfr_type, unsigned char *buf)
{
	int retval;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (unlikely(buf == NULL)) {
		FMDERR("%s:buf is null", __func__);
		return -EINVAL;
	}

	retval = xfr_intf_own(radio);
	if (retval < 0)
		return retval;
	retval = write_to_xfr(radio, xfr_type, buf, XFR_REG_NUM);

	if (retval >= 0) {
		if (!wait_for_completion_timeout(&radio->sync_req_done,
			msecs_to_jiffies(wait_timeout)) || (retval < 0)) {
			FMDBG("Write xfr timeout");
		}
	}
	radio->xfr_in_progress = 0;
	start_pending_xfr(radio);
	FMDBG("%s: %d\n", __func__,  retval);
	return retval;
}


static void start_pending_xfr(struct tavarua_device *radio)
{
	int i;
	enum tavarua_xfr_t xfr;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return;
	}

	for (i = 0; i < TAVARUA_XFR_MAX; i++) {
		if (radio->pending_xfrs[i]) {
			radio->xfr_in_progress = 1;
			xfr = (enum tavarua_xfr_t)i;
			switch (xfr) {
			
			case TAVARUA_XFR_SYNC:
				complete(&radio->sync_xfr_start);
				break;
			
			case TAVARUA_XFR_SRCH_LIST:
				request_read_xfr(radio, RX_STATIONS_0);
				break;
			case TAVARUA_XFR_RT_RDS:
				request_read_xfr(radio, RDS_RT_0);
				break;
			case TAVARUA_XFR_PS_RDS:
				request_read_xfr(radio, RDS_PS_0);
				break;
			case TAVARUA_XFR_AF_LIST:
				request_read_xfr(radio, RDS_AF_0);
				break;
			default:
				FMDERR("%s: Unsupported XFR %d\n",
					 __func__, xfr);
			}
			radio->pending_xfrs[i] = 0;
			FMDBG("resurrect xfr %d\n", i);
			}
	}
	return;
}

static void tavarua_q_event(struct tavarua_device *radio,
				enum tavarua_evt_t event)
{

	struct kfifo *data_b;
	unsigned char evt = event;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return;
	}

	data_b = &radio->data_buf[TAVARUA_BUF_EVENTS];

	FMDBG("updating event_q with event %x\n", event);
	if (kfifo_in_locked(data_b, &evt, 1, &radio->buf_lock[TAVARUA_BUF_EVENTS]))
		wake_up_interruptible(&radio->event_queue);
}

static void tavarua_start_xfr(struct tavarua_device *radio,
		enum tavarua_xfr_t pending_id, enum tavarua_xfr_ctrl_t xfr_id)
{

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return;
	}

	if (radio->xfr_in_progress)
		radio->pending_xfrs[pending_id] = 1;
	else {
		radio->xfr_in_progress = 1;
		request_read_xfr(radio, xfr_id);
	}
}

static void tavarua_handle_interrupts(struct tavarua_device *radio)
{
	int i;
	int retval, adj_channel_tune_req = 0;
	unsigned char xfr_status;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return;
	}

	if (!radio->handle_irq) {
		FMDBG("IRQ happend, but I wont handle it\n");
		return;
	}
	mutex_lock(&radio->lock);
	retval = tavarua_read_registers(radio, STATUS_REG1, STATUS_REG_NUM);
	if (retval < 0) {
		FMDERR("Fails to read status register and try once again");
		msleep(TAVARUA_DELAY);
		retval = tavarua_read_registers(radio, STATUS_REG1,
							STATUS_REG_NUM);
		if (retval < 0)
			FMDERR("Fails to read status register");
	}

	FMDBG("INTSTAT1 <%x>\n", radio->registers[STATUS_REG1]);
	FMDBG("INTSTAT2 <%x>\n", radio->registers[STATUS_REG2]);
	FMDBG("INTSTAT3 <%x>\n", radio->registers[STATUS_REG3]);

	if (radio->registers[STATUS_REG1] & READY) {
		complete(&radio->sync_req_done);
		tavarua_q_event(radio, TAVARUA_EVT_RADIO_READY);
	}

	
	if (radio->registers[STATUS_REG1] & TUNE) {
		if (radio->tune_req) {
			complete(&radio->sync_req_done);
			radio->tune_req = 0;
		}

		adj_channel_tune_req = atomic_read(&radio->validate_channel);
		if (adj_channel_tune_req) {
			complete(&radio->sync_req_done);
			FMDBG("Tune event for adjacent channel\n");
		} else {
			tavarua_q_event(radio, TAVARUA_EVT_TUNE_SUCC);
			FMDBG("Queueing Tune event\n");
		}

		if (radio->srch_params.get_list) {
			tavarua_start_xfr(radio, TAVARUA_XFR_SRCH_LIST,
							RX_STATIONS_0);
		}
		radio->srch_params.get_list = 0;
		radio->xfr_in_progress = 0;
		radio->xfr_bytes_left = 0;
		for (i = 0; i < TAVARUA_BUF_MAX; i++) {
			if (i >= TAVARUA_BUF_RT_RDS)
				kfifo_reset(&radio->data_buf[i]);
		}
		for (i = 0; i < TAVARUA_XFR_MAX; i++) {
			if (i >= TAVARUA_XFR_RT_RDS)
				radio->pending_xfrs[i] = 0;
		}
		retval = tavarua_read_registers(radio, TUNECTRL, 1);
		
		if (retval > -1) {
			
			if (!(radio->registers[TUNECTRL] & SIGSTATE))
				tavarua_q_event(radio, TAVARUA_EVT_BELOW_TH);
			else
				tavarua_q_event(radio, TAVARUA_EVT_ABOVE_TH);
			
			if ((radio->registers[TUNECTRL] & MOSTSTATE))
				tavarua_q_event(radio, TAVARUA_EVT_STEREO);
			else
				tavarua_q_event(radio, TAVARUA_EVT_MONO);
			
			if ((radio->registers[TUNECTRL] & RDSSYNC))
				tavarua_q_event(radio, TAVARUA_EVT_RDS_AVAIL);
			else
				tavarua_q_event(radio,
						TAVARUA_EVT_RDS_NOT_AVAIL);
		}

	} else {
		if (radio->tune_req) {
			FMDERR("Tune INT is pending\n");
			mutex_unlock(&radio->lock);
			return;
		}
	}
	
	if (radio->registers[STATUS_REG1] & SEARCH)
		tavarua_q_event(radio, TAVARUA_EVT_SEEK_COMPLETE);

	
	if (radio->registers[STATUS_REG1] & SCANNEXT)
		tavarua_q_event(radio, TAVARUA_EVT_SCAN_NEXT);

	
	if (radio->registers[STATUS_REG1] & SIGNAL) {
		retval = tavarua_read_registers(radio, TUNECTRL, 1);
		if (retval > -1) {
			if (!(radio->registers[TUNECTRL] & SIGSTATE))
				tavarua_q_event(radio, TAVARUA_EVT_BELOW_TH);
			else
				tavarua_q_event(radio, TAVARUA_EVT_ABOVE_TH);
		}
	}

	
	if (radio->registers[STATUS_REG1] & SYNC) {
		retval = tavarua_read_registers(radio, TUNECTRL, 1);
		if (retval > -1) {
			if ((radio->registers[TUNECTRL] & RDSSYNC))
				tavarua_q_event(radio, TAVARUA_EVT_RDS_AVAIL);
			else
				tavarua_q_event(radio,
						TAVARUA_EVT_RDS_NOT_AVAIL);
		}
	}

	
	if (radio->registers[STATUS_REG1] & AUDIO) {
		retval = tavarua_read_registers(radio, AUDIOIND, 1);
		if (retval > -1) {
			if ((radio->registers[AUDIOIND] & 0x01))
				tavarua_q_event(radio, TAVARUA_EVT_STEREO);
			else
				tavarua_q_event(radio, TAVARUA_EVT_MONO);
		}
	}

	

	
	if (radio->registers[STATUS_REG2] & RDSDAT) {
		FMDBG("Raw RDS Available\n");
		tavarua_rds_read(radio);
		tavarua_q_event(radio, TAVARUA_EVT_NEW_RAW_RDS);
	}

	
	if (radio->registers[STATUS_REG2] & RDSPS) {
		FMDBG("New PS RDS\n");
		tavarua_start_xfr(radio, TAVARUA_XFR_PS_RDS, RDS_PS_0);
	}

	
	if (radio->registers[STATUS_REG2] & RDSRT) {
		FMDBG("New RT RDS\n");
		tavarua_start_xfr(radio, TAVARUA_XFR_RT_RDS, RDS_RT_0);
	}

	
	if (radio->registers[STATUS_REG2] & RDSAF) {
		FMDBG("New AF RDS\n");
		tavarua_start_xfr(radio, TAVARUA_XFR_AF_LIST, RDS_AF_0);
	}
	
	if (radio->registers[STATUS_REG2] & TXRDSDAT) {
		FMDBG("New TXRDSDAT\n");
		tavarua_q_event(radio, TAVARUA_EVT_TXRDSDAT);
	}

	
	if (radio->registers[STATUS_REG2] & TXRDSDONE) {
		FMDBG("New TXRDSDAT\n");
		tavarua_q_event(radio, TAVARUA_EVT_TXRDSDONE);
	}
	

	
	if (radio->registers[STATUS_REG3] & TRANSFER) {
		FMDBG("XFR Interrupt\n");
		tavarua_read_registers(radio, XFRCTRL, XFR_REG_NUM+1);
		FMDBG("XFRCTRL IS: %x\n", radio->registers[XFRCTRL]);
		xfr_status = radio->registers[XFRCTRL];
		switch (xfr_status) {
		case RDS_PS_0:
			FMDBG("PS Header\n");
			copy_from_xfr(radio, TAVARUA_BUF_PS_RDS, 5);
			radio->xfr_bytes_left = (radio->registers[XFRCTRL+1] &
								0x0F) * 8;
			FMDBG("PS RDS Length: %d\n", radio->xfr_bytes_left);
			if ((radio->xfr_bytes_left > 0) &&
			    (radio->xfr_bytes_left < 97))
				request_read_xfr(radio,	RDS_PS_1);
			else
				radio->xfr_in_progress = 0;
			break;
		case RDS_PS_1:
		case RDS_PS_2:
		case RDS_PS_3:
		case RDS_PS_4:
		case RDS_PS_5:
		case RDS_PS_6:
			FMDBG("PS Data\n");
			copy_from_xfr(radio, TAVARUA_BUF_PS_RDS, XFR_REG_NUM);
			radio->xfr_bytes_left -= XFR_REG_NUM;
			if (radio->xfr_bytes_left > 0) {
				if ((xfr_status + 1) > RDS_PS_6)
					request_read_xfr(radio,	RDS_PS_6);
				else
					request_read_xfr(radio,	xfr_status+1);
			} else {
				radio->xfr_in_progress = 0;
				tavarua_q_event(radio, TAVARUA_EVT_NEW_PS_RDS);
			}
			break;
		case RDS_RT_0:
			FMDBG("RT Header\n");
			copy_from_xfr(radio, TAVARUA_BUF_RT_RDS, 5);
			radio->xfr_bytes_left = radio->registers[XFRCTRL+1]
									& 0x7F;
			FMDBG("RT RDS Length: %d\n", radio->xfr_bytes_left);
			
			if ((radio->xfr_bytes_left > 0)
					 && (radio->xfr_bytes_left < 65))
				request_read_xfr(radio, RDS_RT_1);
			break;
		case RDS_RT_1:
		case RDS_RT_2:
		case RDS_RT_3:
		case RDS_RT_4:
			FMDBG("xfr interrupt RT data\n");
			copy_from_xfr(radio, TAVARUA_BUF_RT_RDS, XFR_REG_NUM);
			radio->xfr_bytes_left -= XFR_REG_NUM;
			if (radio->xfr_bytes_left > 0)
				request_read_xfr(radio,	xfr_status+1);
			else {
				radio->xfr_in_progress = 0;
				tavarua_q_event(radio, TAVARUA_EVT_NEW_RT_RDS);
			}
			break;
		case RDS_AF_0:
			copy_from_xfr(radio, TAVARUA_BUF_AF_LIST,
						XFR_REG_NUM);
			radio->xfr_bytes_left = radio->registers[XFRCTRL+5]-11;
			if (radio->xfr_bytes_left > 0)
				request_read_xfr(radio,	RDS_AF_1);
			else
				radio->xfr_in_progress = 0;
			break;
		case RDS_AF_1:
			copy_from_xfr(radio, TAVARUA_BUF_AF_LIST,
						radio->xfr_bytes_left);
			tavarua_q_event(radio, TAVARUA_EVT_NEW_AF_LIST);
			radio->xfr_in_progress = 0;
			break;
		case RX_CONFIG:
		case RADIO_CONFIG:
		case RDS_CONFIG:
			memcpy(radio->sync_xfr_regs,
				&radio->registers[XFRCTRL+1], XFR_REG_NUM);
			complete(&radio->sync_req_done);
			break;
		case RX_STATIONS_0:
			FMDBG("Search list has %d stations\n",
						radio->registers[XFRCTRL+1]);
			radio->xfr_bytes_left = radio->registers[XFRCTRL+1]*2;
			if (!radio->registers[XFRCTRL+1]) {
				copy_from_xfr(radio, TAVARUA_BUF_SRCH_LIST,
									1);
				tavarua_q_event(radio,
						TAVARUA_EVT_NEW_SRCH_LIST);
				radio->xfr_in_progress = 0;
			} else if (radio->xfr_bytes_left > 14) {
				copy_from_xfr(radio, TAVARUA_BUF_SRCH_LIST,
							RX_STATIONS0_LEN);
				request_read_xfr(radio,	RX_STATIONS_1);
			} else if (radio->xfr_bytes_left) {
				FMDBG("In else RX_STATIONS_0\n");
				copy_from_xfr(radio, TAVARUA_BUF_SRCH_LIST,
						radio->xfr_bytes_left+1);
				tavarua_q_event(radio,
						TAVARUA_EVT_NEW_SRCH_LIST);
				radio->xfr_in_progress = 0;
			}
			break;
		case RX_STATIONS_1:
			FMDBG("In RX_STATIONS_1");
			copy_from_xfr(radio, TAVARUA_BUF_SRCH_LIST,
						radio->xfr_bytes_left);
			tavarua_q_event(radio, TAVARUA_EVT_NEW_SRCH_LIST);
			radio->xfr_in_progress = 0;
			break;
		case PHY_TXGAIN:
			FMDBG("read PHY_TXGAIN is successful");
			complete(&radio->sync_req_done);
			break;
		case (XFR_EXT | 0x80):
			FMDBG("Set tone generator successful\n");
			complete(&radio->sync_req_done);
			break;
		case (0x80 | RX_CONFIG):
		case (0x80 | RADIO_CONFIG):
		case (0x80 | RDS_CONFIG):
		case (0x80 | INT_CTRL):
			complete(&radio->sync_req_done);
			break;
		case (0x80 | RDS_RT_0):
			FMDBG("RT Header Sent\n");
			complete(&radio->sync_req_done);
			break;
		case (0x80 | RDS_RT_1):
		case (0x80 | RDS_RT_2):
		case (0x80 | RDS_RT_3):
		case (0x80 | RDS_RT_4):
			FMDBG("xfr interrupt RT data Sent\n");
			complete(&radio->sync_req_done);
			break;
		
		case (0x80 | RDS_PS_0):
			FMDBG("PS Header Sent\n");
			complete(&radio->sync_req_done);
			break;
		case (0x80 | RDS_PS_1):
		case (0x80 | RDS_PS_2):
		case (0x80 | RDS_PS_3):
		case (0x80 | RDS_PS_4):
		case (0x80 | RDS_PS_5):
		case (0x80 | RDS_PS_6):
			FMDBG("xfr interrupt PS data Sent\n");
			complete(&radio->sync_req_done);
			break;
		case (0x80 | PHY_TXGAIN):
			FMDBG("write PHY_TXGAIN is successful");
			complete(&radio->sync_req_done);
			break;
		case (XFR_POKE_COMPLETE | LSH_DATA(ONE_BYTE, 1)):
		case (XFR_POKE_COMPLETE | LSH_DATA(TWO_BYTE, 1)):
		case (XFR_POKE_COMPLETE | LSH_DATA(THREE_BYTE, 1)):
		case (XFR_POKE_COMPLETE | LSH_DATA(FOUR_BYTE, 1)):
		case (XFR_POKE_COMPLETE | LSH_DATA(FIVE_BYTE, 1)):
		case (XFR_POKE_COMPLETE | LSH_DATA(SIX_BYTE, 1)):
		case (XFR_POKE_COMPLETE | LSH_DATA(SEVEN_BYTE, 1)):
		case (XFR_POKE_COMPLETE | LSH_DATA(EIGHT_BYTE, 1)):
		case (XFR_POKE_COMPLETE | LSH_DATA(NINE_BYTE, 1)):
		case (XFR_POKE_COMPLETE | LSH_DATA(TEN_BYTE, 1)):
		case (XFR_POKE_COMPLETE | LSH_DATA(ELEVEN_BYTE, 1)):
		case (XFR_POKE_COMPLETE | LSH_DATA(TWELVE_BYTE, 1)):
		case (XFR_POKE_COMPLETE | LSH_DATA(THIRTEEN_BYTE, 1)):
		case (XFR_PEEK_COMPLETE | LSH_DATA(ONE_BYTE, 1)):
		case (XFR_PEEK_COMPLETE | LSH_DATA(TWO_BYTE, 1)):
		case (XFR_PEEK_COMPLETE | LSH_DATA(THREE_BYTE, 1)):
		case (XFR_PEEK_COMPLETE | LSH_DATA(FOUR_BYTE, 1)):
		case (XFR_PEEK_COMPLETE | LSH_DATA(FIVE_BYTE, 1)):
		case (XFR_PEEK_COMPLETE | LSH_DATA(SIX_BYTE, 1)):
		case (XFR_PEEK_COMPLETE | LSH_DATA(SEVEN_BYTE, 1)):
		case (XFR_PEEK_COMPLETE | LSH_DATA(EIGHT_BYTE, 1)):
		case (XFR_PEEK_COMPLETE | LSH_DATA(NINE_BYTE, 1)):
		case (XFR_PEEK_COMPLETE | LSH_DATA(TEN_BYTE, 1)):
		case (XFR_PEEK_COMPLETE | LSH_DATA(ELEVEN_BYTE, 1)):
		case (XFR_PEEK_COMPLETE | LSH_DATA(TWELVE_BYTE, 1)):
		case (XFR_PEEK_COMPLETE | LSH_DATA(THIRTEEN_BYTE, 1)):
			FMDBG("XFR interrupt for PEEK/POKE complete\n");
			complete(&radio->sync_req_done);
			break;
		default:
			FMDERR("UNKNOWN XFR = %d\n", xfr_status);
		}
		if (!radio->xfr_in_progress)
			start_pending_xfr(radio);

	}

	
	if (radio->registers[STATUS_REG3] & ERROR) {
#ifdef FM_DEBUG
		unsigned char xfr_buf[XFR_REG_NUM];
		int retval = sync_read_xfr(radio, ERROR_CODE, xfr_buf);
		FMDBG("retval of ERROR_CODE read : %d\n", retval);
#endif
		FMDERR("ERROR STATE\n");
	}

	mutex_unlock(&radio->lock);
	FMDBG("Work is done\n");

}

static void read_int_stat(struct work_struct *work)
{
	struct tavarua_device *radio;

	if (unlikely(work == NULL)) {
		FMDERR("%s:work is null", __func__);
		return;
	}

	radio = container_of(work, struct tavarua_device, work.work);

	tavarua_handle_interrupts(radio);
}

static void fm_shutdown(struct work_struct *work)
{
	struct tavarua_device *radio;

	if (unlikely(work == NULL)) {
		FMDERR("%s:work is null", __func__);
		return;
	}

	radio = container_of(work, struct tavarua_device, work.work);

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return;
	}

	FMDERR("%s: Releasing the FM I2S GPIO\n", __func__);
	if (radio->pdata->config_i2s_gpio != NULL)
		radio->pdata->config_i2s_gpio(FM_I2S_OFF);
	FMDERR("%s: Shutting down FM SOC\n", __func__);
	radio->pdata->fm_shutdown(radio->pdata);
	complete(&radio->shutdown_done);
}



static int tavarua_request_irq(struct tavarua_device *radio)
{
	int retval;
	int irq;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	irq = radio->pdata->irq;

  
	retval = request_any_context_irq(irq, tavarua_isr,
				IRQ_TYPE_EDGE_FALLING, "fm interrupt", radio);
	if (retval < 0) {
		FMDERR("Couldn't acquire FM gpio %d\n", irq);
		return retval;
	} else {
		FMDBG("FM GPIO %d registered\n", irq);
	}
	retval = enable_irq_wake(irq);
	if (retval < 0) {
		FMDERR("Could not enable FM interrupt\n ");
		free_irq(irq , radio);
	}
	return retval;
}

static int tavarua_disable_irq(struct tavarua_device *radio)
{
	int irq;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	irq = radio->pdata->irq;
	disable_irq_wake(irq);
	free_irq(irq, radio);
	cancel_delayed_work_sync(&radio->work);
	flush_workqueue(radio->wqueue);
	return 0;
}

static int optimized_search_algorithm(struct tavarua_device *radio,
				int region)
{
	unsigned char adie_type_bahma;
	int retval = 0;
	unsigned int rdsMask = 0;
	unsigned char value = 0;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	adie_type_bahma = is_bahama();

	switch (region) {
	case TAVARUA_REGION_US:
	FMDBG("%s: The region selected from APP is"
		" : TAVARUA_REGION_US", __func__);
	break;
	case TAVARUA_REGION_EU:
	FMDBG("%s: The region selected from APP is : "
		"TAVARUA_REGION_EU", __func__);
	break;
	case TAVARUA_REGION_JAPAN:
	FMDBG("%s: The region selected from APP is"
		" : TAVARUA_REGION_JAPAN", __func__);
	break;
	case TAVARUA_REGION_JAPAN_WIDE:
	FMDBG("%s: The region selected from APP is"
		" : TAVARUA_REGION_JAPAN_WIDE", __func__);
	break;
	case TAVARUA_REGION_OTHER:
	FMDBG("%s: The region selected from APP is"
		" : TAVARUA_REGION_OTHER", __func__);
	break;
	default:
		pr_err("%s: Should not reach here.", __func__);
		break;

	}

	
	switch (region) {
	case TAVARUA_REGION_US:
	case TAVARUA_REGION_JAPAN:
	case TAVARUA_REGION_OTHER:
		if (adie_type_bahma) {
			FMDBG("Adie type : Bahama\n");
			FMDBG("%s: Enabling the 200KHz enforcer for"
				" Region : %d", __func__, region);
			
			retval = tavarua_read_registers(radio,
				ADVCTRL, 1);
			if (retval >= 0) {
				rdsMask = radio->registers[ADVCTRL];
				SET_REG_FIELD(rdsMask, ENF_SRCH200khz,
					SRCH200KHZ_OFFSET, SRCH_MASK);
				retval = tavarua_write_register(radio,
					ADVCTRL, rdsMask);
			} else
				return retval;
		} 
		break;
	case TAVARUA_REGION_EU:
	case TAVARUA_REGION_JAPAN_WIDE:
		if (adie_type_bahma) {
			FMDBG("Adie type : Bahama\n");
			FMDBG("%s: Disabling the 200KHz enforcer for"
				" Region : %d", __func__, region);
			retval = tavarua_read_registers(radio,
				ADVCTRL, 1);
			if (retval >= 0) {
				rdsMask = radio->registers[ADVCTRL];
				SET_REG_FIELD(rdsMask, NO_SRCH200khz,
					SRCH200KHZ_OFFSET, SRCH_MASK);
				retval = tavarua_write_register(radio,
					ADVCTRL, rdsMask);
			} else
				return retval;
		} 
		break;
	default:
		FMDBG("%s: Defaulting in case of Enabling/Disabling"
			"the 200KHz Enforcer", __func__);
		break;
	}

	
	switch (region) {
	case TAVARUA_REGION_US:
		if ((adie_type_bahma) && (bahama_version == 0x09)) {
			FMDBG("Adie type : Bahama\n");
			value = FM_CH_SPACE_100KHZ;
		} else if ((adie_type_bahma) && (bahama_version == 0x0a)) {
			FMDBG("Adie type : Bahama B1\n");
			value = FM_CH_SPACE_200KHZ;
		} else {
			FMDBG("Adie type : Marimba\n");
			value = FM_CH_SPACE_200KHZ;
		}
		break;
	case TAVARUA_REGION_JAPAN:
	case TAVARUA_REGION_OTHER:
		if ((adie_type_bahma) && (bahama_version == 0x09)) {
			FMDBG("Adie type : Bahama\n");
			FMDBG("%s: Configuring the channel-spacing as 50KHz"
				"for the Region : %d", __func__, region);
			value = FM_CH_SPACE_50KHZ;
		} else if ((adie_type_bahma) && (bahama_version == 0x0a)) {
			FMDBG("Adie type : Bahama B1\n");
			value = FM_CH_SPACE_100KHZ;
		} else {
			FMDBG("Adie type : Marimba\n");
			value = FM_CH_SPACE_100KHZ;
		}
		break;
	case TAVARUA_REGION_EU:
	case TAVARUA_REGION_JAPAN_WIDE:
		value = FM_CH_SPACE_50KHZ;
		break;
	default:
		FMDBG("%s: Defualting in case of Channel-Spacing", __func__);
		break;
	}

	SET_REG_FIELD(radio->registers[RDCTRL], value,
		RDCTRL_CHSPACE_OFFSET, RDCTRL_CHSPACE_MASK);

	return retval;
}

static int tavarua_search(struct tavarua_device *radio, int on, int dir)
{
	enum search_t srch;

	FMDBG("In tavarua_search\n");

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	srch = radio->registers[SRCHCTRL] & SRCH_MODE;

	if (on) {
		radio->registers[SRCHRDS1] = 0x00;
		radio->registers[SRCHRDS2] = 0x00;
		
		switch (srch) {
		case SCAN_FOR_STRONG:
		case SCAN_FOR_WEAK:
			radio->srch_params.get_list = 1;
			radio->registers[SRCHRDS2] =
					radio->srch_params.preset_num;
			break;
		case RDS_SEEK_PTY:
		case RDS_SCAN_PTY:
			radio->registers[SRCHRDS2] =
					radio->srch_params.srch_pty;
			break;
		case RDS_SEEK_PI:
			radio->registers[SRCHRDS1] =
				(radio->srch_params.srch_pi & 0xFF00) >> 8;
			radio->registers[SRCHRDS2] =
				(radio->srch_params.srch_pi & 0x00FF);
			break;
		default:
			break;
		}
		radio->registers[SRCHCTRL] |= SRCH_ON;
	} else {
		radio->registers[SRCHCTRL] &= ~SRCH_ON;
		radio->srch_params.get_list = 0;
	}
	radio->registers[SRCHCTRL] = (dir << 3) |
				(radio->registers[SRCHCTRL] & 0xF7);

	FMDBG("SRCHCTRL <%x>\n", radio->registers[SRCHCTRL]);
	FMDBG("Search Started\n");
	return tavarua_write_registers(radio, SRCHRDS1,
				&radio->registers[SRCHRDS1], 3);
}

static int tavarua_set_region(struct tavarua_device *radio,
				int req_region)
{
	int retval = 0;
	unsigned int rdsMask = 0;
	unsigned char xfr_buf[XFR_REG_NUM];
	unsigned char value;
	unsigned int spacing = 0.100 * FREQ_MUL;
	unsigned int band_low, band_high;
	unsigned int low_band_limit = 76.0 * FREQ_MUL;
	enum tavarua_region_t region = req_region;
	unsigned char adie_type_bahma;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	adie_type_bahma = is_bahama();

	
	if (region == TAVARUA_REGION_JAPAN)
		SET_REG_FIELD(radio->registers[RDCTRL], 1,
			RDCTRL_BAND_OFFSET, RDCTRL_BAND_MASK);
	else
		SET_REG_FIELD(radio->registers[RDCTRL], 0,
			RDCTRL_BAND_OFFSET, RDCTRL_BAND_MASK);

	
	SET_REG_FIELD(radio->registers[RDCTRL], radio->region_params.emphasis,
		RDCTRL_DEEMPHASIS_OFFSET, RDCTRL_DEEMPHASIS_MASK);

	
	SET_REG_FIELD(radio->registers[RDSCTRL], radio->region_params.rds_std,
		RDSCTRL_STANDARD_OFFSET, RDSCTRL_STANDARD_MASK);

	FMDBG("RDSCTRLL %x\n", radio->registers[RDSCTRL]);
	retval = tavarua_write_register(radio, RDSCTRL,
					radio->registers[RDSCTRL]);
	if (retval < 0) {
		FMDERR("Failed to set RDS/RBDS standard\n");
		return retval;
	}

	
	retval = sync_read_xfr(radio, RADIO_CONFIG, xfr_buf);
	if (retval < 0) {
		FMDERR("failed to get RADIO_CONFIG\n");
		return retval;
	}

	band_low = (radio->region_params.band_low -
				low_band_limit) / spacing;
	band_high = (radio->region_params.band_high -
				low_band_limit) / spacing;

	xfr_buf[0] = RSH_DATA(band_low, 8);
	xfr_buf[1] = GET_ABS_VAL(band_low);
	xfr_buf[2] = RSH_DATA(band_high, 8);
	xfr_buf[3] = GET_ABS_VAL(band_high);
	xfr_buf[4] = 0; 
	retval = sync_write_xfr(radio, RADIO_CONFIG, xfr_buf);
	if (retval < 0) {
		FMDERR("Could not set regional settings\n");
		return retval;
	}
	radio->region_params.region = region;

	
	if (radio->enable_optimized_srch_alg) {
		FMDBG("Optimized Srch Algorithm!!!");
		optimized_search_algorithm(radio, region);
	} else {
		FMDBG("Native Srch Algorithm!!!");
		
		switch (region) {
		case TAVARUA_REGION_US:
			if (adie_type_bahma) {
				FMDBG("Adie type : Bahama\n");
				
				retval = tavarua_read_registers(radio,
					ADVCTRL, 1);
				if (retval >= 0) {
					rdsMask = radio->registers[ADVCTRL];
					SET_REG_FIELD(rdsMask, ENF_SRCH200khz,
						SRCH200KHZ_OFFSET, SRCH_MASK);
					retval = tavarua_write_register(radio,
						ADVCTRL, rdsMask);
				} else
					return retval;
			} 
			break;
		case TAVARUA_REGION_EU:
		case TAVARUA_REGION_JAPAN:
		case TAVARUA_REGION_JAPAN_WIDE:
		default:
			if (adie_type_bahma) {
				FMDBG("Adie type : Bahama\n");
				retval = tavarua_read_registers(radio,
					ADVCTRL, 1);
				if (retval >= 0) {
					rdsMask = radio->registers[ADVCTRL];
					SET_REG_FIELD(rdsMask, NO_SRCH200khz,
						SRCH200KHZ_OFFSET, SRCH_MASK);
					retval = tavarua_write_register(radio,
						ADVCTRL, rdsMask);
				} else
					return retval;
			} 
			break;
		}

		
		if (region == TAVARUA_REGION_US) {
			if (adie_type_bahma) {
				FMDBG("Adie type : Bahama\n");
				value = FM_CH_SPACE_100KHZ;
			} else {
				FMDBG("Adie type : Marimba\n");
				value = FM_CH_SPACE_200KHZ;
			}
		} else {
			value = radio->region_params.spacing;
		}
		SET_REG_FIELD(radio->registers[RDCTRL], value,
			RDCTRL_CHSPACE_OFFSET, RDCTRL_CHSPACE_MASK);

	}

	
	FMDBG("RDCTRL: %x\n", radio->registers[RDCTRL]);
	retval = tavarua_write_register(radio, RDCTRL,
					radio->registers[RDCTRL]);
	if (retval < 0) {
		FMDERR("Could not set region in rdctrl\n");
		return retval;
	}

	return retval;
}

static int tavarua_get_freq(struct tavarua_device *radio,
				struct v4l2_frequency *freq)
{
	int retval;
	unsigned short chan;
	unsigned int band_bottom;
	unsigned int spacing;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (unlikely(freq == NULL)) {
		FMDERR("%s:freq is null", __func__);
		return -EINVAL;
	}

	band_bottom = radio->region_params.band_low;
	spacing  = 0.100 * FREQ_MUL;
	
	retval = tavarua_read_registers(radio, FREQ, 2);
	chan = radio->registers[FREQ];

	
	freq->frequency = spacing * chan + band_bottom;
	if (radio->registers[TUNECTRL] & ADD_OFFSET)
		freq->frequency += 800;
	return retval;
}

static int tavarua_set_freq(struct tavarua_device *radio, unsigned int freq)
{

	unsigned int band_bottom;
	unsigned char chan;
	unsigned char cmd[] = {0x00, 0x00};
	unsigned int spacing;
	int retval;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	band_bottom = radio->region_params.band_low;
	spacing  = 0.100 * FREQ_MUL;
	if ((freq % 1600) == 800) {
		cmd[1] = ADD_OFFSET;
		freq -= 800;
	}
	
	chan = (freq - band_bottom) / spacing;

	cmd[0] = chan;
	cmd[1] |= TUNE_STATION;
	radio->tune_req = 1;
	retval = tavarua_write_registers(radio, FREQ, cmd, 2);
	if (retval < 0)
		radio->tune_req = 0;
	return retval;

}


static ssize_t tavarua_fops_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	struct tavarua_device *radio = video_get_drvdata(video_devdata(file));
	struct kfifo *rds_buf;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (unlikely(buf == NULL)) {
		FMDERR("%s:buf is null", __func__);
		return -EINVAL;
	}

	rds_buf = &radio->data_buf[TAVARUA_BUF_RAW_RDS];
	
	while (!kfifo_len(rds_buf)) {
		if (file->f_flags & O_NONBLOCK)
			return -EWOULDBLOCK;
		if (wait_event_interruptible(radio->read_queue,
			kfifo_len(rds_buf)) < 0)
			return -EINTR;
	}

	
	count /= BYTES_PER_BLOCK;


	
	if (!access_ok(VERIFY_WRITE, buf, count*BYTES_PER_BLOCK))
		return -EFAULT;

	
	return kfifo_out_locked(rds_buf, buf, count*BYTES_PER_BLOCK,
				&radio->buf_lock[TAVARUA_BUF_RAW_RDS]);
}

/**
  This function is called when a process, which already opened the dev file,
  attempts to write to it.

  In case of tavarua driver, it is called to write RDS data to host.

  @param file: file descriptor.
	@param buf: The buffer which has data to write.
	@param count: The length of the buffer.
	@param ppos: Our offset in the file.

  @return The number of bytes written from the buffer.
*/
static ssize_t tavarua_fops_write(struct file *file, const char __user *data,
			size_t count, loff_t *ppos)
{
	struct tavarua_device *radio = video_get_drvdata(video_devdata(file));
	int retval = 0;
	int bytes_to_copy;
	int bytes_copied = 0;
	int bytes_left;
	int chunk_index = 0;
	unsigned char tx_data[XFR_REG_NUM];

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (unlikely(data == NULL)) {
		FMDERR("%s:data is null", __func__);
		return -EINVAL;
	}

	
	switch (radio->tx_mode) {
	case TAVARUA_TX_RT:
		bytes_left = min((int)count, MAX_RT_LENGTH);
		tx_data[1] = 0;
		break;
	case TAVARUA_TX_PS:
		bytes_left = min((int)count, MAX_PS_LENGTH);
		tx_data[4] = 0;
		break;
	default:
		FMDERR("%s: Unknown TX mode\n", __func__);
		return -1;
	}
	retval = sync_write_xfr(radio, radio->tx_mode, tx_data);
	if (retval < 0)
		return retval;

	
	while (bytes_left) {
		chunk_index++;
		bytes_to_copy = min(bytes_left, XFR_REG_NUM);
		if (copy_from_user(tx_data, data + bytes_copied, bytes_to_copy))
			return -EFAULT;
		retval = sync_write_xfr(radio, radio->tx_mode +
						chunk_index, tx_data);
		if (retval < 0)
			return retval;

		bytes_copied += bytes_to_copy;
		bytes_left -= bytes_to_copy;
	}

	
	switch (radio->tx_mode) {
	case TAVARUA_TX_RT:
		FMDBG("Writing RT header\n");
		tx_data[0] = bytes_copied;
		tx_data[1] = TX_ON | 0x03; 
		tx_data[2] = 0x12; 
		tx_data[3] = 0x34; 
		break;
	case TAVARUA_TX_PS:
		FMDBG("Writing PS header\n");
		tx_data[0] = chunk_index;
		tx_data[1] = 0x03; 
		tx_data[2] = 0x12; 
		tx_data[3] = 0x34; 
		tx_data[4] = TX_ON | 0x01;
		break;
	default:
		FMDERR("%s: Unknown TX mode\n", __func__);
		return -1;
	}
	retval = sync_write_xfr(radio, radio->tx_mode, tx_data);
	if (retval < 0)
		return retval;
	FMDBG("done writing: %d\n", retval);
	return bytes_copied;
}

static int tavarua_fops_open(struct file *file)
{
	struct tavarua_device *radio = video_get_drvdata(video_devdata(file));
	struct marimba config = { .mod_id =  SLAVE_ID_BAHAMA};
	int retval = -ENODEV;
	unsigned char value;
	
	int i = 0;
	char fm_ctl0_part1[] = { 0xCA, 0xCE, 0xD6 };
	char fm_ctl1[] = { 0x03 };
	char fm_ctl0_part2[] = { 0xB6, 0xB7 };
	char buffer[] = {0x00, 0x48, 0x8A, 0x8E, 0x97, 0xB7};
	int bahama_present = -ENODEV;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&radio->work, read_int_stat);
	if (!atomic_dec_and_test(&radio->users)) {
		pr_err("%s: Device already in use."
			"Try again later", __func__);
		atomic_inc(&radio->users);
		return -EBUSY;
	}

	
	retval = radio->pdata->fm_setup(radio->pdata);
	if (retval) {
		printk(KERN_ERR "%s: failed config gpio & pmic\n", __func__);
		goto open_err_setup;
	}
	if (radio->pdata->config_i2s_gpio != NULL) {
		retval = radio->pdata->config_i2s_gpio(FM_I2S_ON);
		if (retval) {
			printk(KERN_ERR "%s: failed config gpio\n", __func__);
			goto config_i2s_err;
		}
	}
	
	retval = tavarua_request_irq(radio);
	if (retval < 0) {
		printk(KERN_ERR "%s: failed to request irq\n", __func__);
		goto open_err_req_irq;
	}
	
	FMDBG("initializing SoC\n");

	bahama_present = is_bahama();

	if (bahama_present == -ENODEV)
		return -ENODEV;

	marimba_set_fm_status(radio->marimba, true);
	if (bahama_present)
		radio->marimba->mod_id = SLAVE_ID_BAHAMA;
	else
		radio->marimba->mod_id = MARIMBA_SLAVE_ID_MARIMBA;

	value = FM_ENABLE;
	retval = marimba_write_bit_mask(&config,
			MARIMBA_XO_BUFF_CNTRL, &value, 1, value);
	if (retval < 0) {
		printk(KERN_ERR "%s:XO_BUFF_CNTRL write failed\n",
					__func__);
		goto open_err_all;
	}


	
	if (bahama_present)	{

		radio->marimba->mod_id = SLAVE_ID_BAHAMA;
		
		retval = marimba_read_bit_mask(&config,
				0x00,  &bahama_version, 1, 0x1F);
		if (retval < 0) {
			printk(KERN_ERR "%s: version read failed",
				__func__);
			goto open_err_all;
		}

		
		if ((bahama_version == 0x09) || (bahama_version == 0x0a)) {

			value = 0x06;
			
			retval = marimba_write_bit_mask(&config,
			BAHAMA_LDO_DREG_CTL0, &value, 1, value);
			if (retval < 0) {
				printk(KERN_ERR "%s:0xF0 write failed\n",
					__func__);
				goto open_err_all;
			}
			value = 0x86;
			retval = marimba_write_bit_mask(&config,
				BAHAMA_LDO_AREG_CTL0, &value, 1, value);
			if (retval < 0) {
				printk(KERN_ERR "%s:0xF4 write failed\n",
					__func__);
				goto open_err_all;
			}
		}

		
		retval = tavarua_write_register(radio, BAHAMA_FM_MODE_REG,
					BAHAMA_FM_MODE_NORMAL);
		if (retval < 0) {
			printk(KERN_ERR "failed to set the FM mode: %d\n",
					retval);
			goto open_err_all;
		}
		
		for (i = 0; i < 3; i++)  {
			retval = tavarua_write_register(radio,
					BAHAMA_FM_CTL0_REG, fm_ctl0_part1[i]);
			if (retval < 0) {
				printk(KERN_ERR "FM_CTL0:set-1 failure: %d\n",
							retval);
				goto open_err_all;
			}
		}
		
		for (i = 0; i < 1; i++)  {
			retval = tavarua_write_register(radio,
					BAHAMA_FM_CTL1_REG, fm_ctl1[i]);
			if (retval < 0) {
				printk(KERN_ERR "FM_CTL1 write failure: %d\n",
							retval);
				goto open_err_all;
			}
		}
		
		for (i = 0; i < 2; i++)  {
			retval = tavarua_write_register(radio,
					BAHAMA_FM_CTL0_REG, fm_ctl0_part2[i]);
			if (retval < 0) {
				printk(KERN_ERR "FM_CTL0:set-2 failure: %d\n",
					retval);
			goto open_err_all;
			}
		}
	} else {
		retval = tavarua_write_registers(radio, LEAKAGE_CNTRL,
						buffer, 6);
		if (retval < 0) {
			printk(KERN_ERR "%s: failed to bring up FM Core\n",
						__func__);
			goto open_err_all;
		}
	}
	
	init_completion(&radio->sync_req_done);
	if (!wait_for_completion_timeout(&radio->sync_req_done,
		msecs_to_jiffies(wait_timeout))) {
		retval = -1;
		FMDERR("Timeout waiting for initialization\n");
	}

	
	retval = tavarua_write_register(radio, XFRCTRL, CHIPID);
	if (retval < 0)
		goto open_err_all;
	msleep(TAVARUA_DELAY);
	tavarua_read_registers(radio, XFRCTRL, XFR_REG_NUM+1);
	if (radio->registers[XFRCTRL] != CHIPID)
		goto open_err_all;

	radio->chipID = (radio->registers[XFRCTRL+2] << 24) |
			(radio->registers[XFRCTRL+5] << 16) |
			(radio->registers[XFRCTRL+6] << 8)  |
			(radio->registers[XFRCTRL+7]);

	printk(KERN_WARNING DRIVER_NAME ": Chip ID %x\n", radio->chipID);
	if (radio->chipID == MARIMBA_A0) {
		printk(KERN_WARNING DRIVER_NAME ": Unsupported hardware: %x\n",
						radio->chipID);
		retval = -1;
		goto open_err_all;
	}

	radio->handle_irq = 0;
	radio->marimba->mod_id = SLAVE_ID_BAHAMA;
	return 0;


open_err_all:
    
	value = 0x00;
	marimba_write_bit_mask(&config, MARIMBA_XO_BUFF_CNTRL,
							&value, 1, value);
	tavarua_disable_irq(radio);
open_err_req_irq:
	if (radio->pdata->config_i2s_gpio != NULL)
		radio->pdata->config_i2s_gpio(FM_I2S_OFF);
config_i2s_err:
	radio->pdata->fm_shutdown(radio->pdata);
open_err_setup:
	marimba_set_fm_status(radio->marimba, false);
	radio->handle_irq = 1;
	atomic_inc(&radio->users);
	return retval;
}

static int tavarua_fops_release(struct file *file)
{
	int retval;
	struct tavarua_device *radio = video_get_drvdata(video_devdata(file));
	struct marimba config = { .mod_id =  SLAVE_ID_BAHAMA};
	unsigned char value;
	int i = 0;
	
	char fm_ctl0_part1[] = { 0xB7 };
	char fm_ctl1[] = { 0x03 };
	char fm_ctl0_part2[] = { 0x9F, 0x48, 0x02 };
	int bahama_present = -ENODEV;
	
	char buffer[] = {0x18, 0xB7, 0x48};
	bool bt_status = false;
	int index;
	char internal_vreg_ctl[2][2] = {
		{ 0x04, 0x84 },
		{ 0x00, 0x80 }
	};

	if (!radio) {
		pr_err("%s: Radio device not available...", __func__);
		return -ENODEV;
	}

	FMDBG("In %s", __func__);

	FMDBG("%s, Disabling the IRQs\n", __func__);
	
	retval = tavarua_disable_irq(radio);
	if (retval < 0) {
		printk(KERN_ERR "%s: failed to disable irq\n", __func__);
		return retval;
	}

	
	retval = tavarua_write_register(radio, RDCTRL, 0x00);
	if (retval < 0) {
		printk(KERN_ERR "%s: failed to disable FM\n", __func__);
		return retval;
	}

	init_completion(&radio->shutdown_done);

	bahama_present = is_bahama();

	if (bahama_present == -ENODEV)
		return -ENODEV;

	INIT_DELAYED_WORK(&radio->work, fm_shutdown);

	if (bahama_present)	{
		
		for (i = 0; i < 1; i++) {
			retval = tavarua_write_register(radio,
					BAHAMA_FM_CTL0_REG, fm_ctl0_part1[i]);
			if (retval < 0) {
				printk(KERN_ERR "FM_CTL0:Set-1 failure: %d\n",
						retval);
				break;
			}
		}
		
		for (i = 0; i < 1; i++)  {
			retval = tavarua_write_register(radio,
					BAHAMA_FM_CTL1_REG, fm_ctl1[i]);
			if (retval < 0) {
				printk(KERN_ERR "FM_CTL1 failure: %d\n",
						retval);
				break;
			}
		}
		
		for (i = 0; i < 3; i++)   {
			retval = tavarua_write_register(radio,
					BAHAMA_FM_CTL0_REG, fm_ctl0_part2[i]);
			if (retval < 0) {
				printk(KERN_ERR "FM_CTL0:Set-2 failure: %d\n",
						retval);
			break;
			}
		}
	}	else	{

		retval = tavarua_write_registers(radio, FM_CTL0,
				buffer, sizeof(buffer)/sizeof(buffer[0]));
		if (retval < 0) {
			printk(KERN_ERR "%s: failed to bring down the  FM Core\n",
							__func__);
			return retval;
		}
	}
	radio->marimba->mod_id = SLAVE_ID_BAHAMA;
	bt_status = marimba_get_bt_status(radio->marimba);
	
	index = bt_status ?  1 : 0;
	
	if (bahama_present
		&& (bahama_version == 0x09 || bahama_version == 0x0a))   {
		radio->marimba->mod_id = SLAVE_ID_BAHAMA;
		
		retval = marimba_write_bit_mask(&config,
			BAHAMA_LDO_DREG_CTL0, &internal_vreg_ctl[bt_status][0],
			 1, internal_vreg_ctl[index][0]);
		if (retval < 0) {
			printk(KERN_ERR "%s:0xF0 write failed\n", __func__);
			goto exit;
		}
		
		retval = marimba_write_bit_mask(&config,
			BAHAMA_LDO_AREG_CTL0, &internal_vreg_ctl[bt_status][1],
			1, internal_vreg_ctl[index][1]);
		if (retval < 0) {
			printk(KERN_ERR "%s:0xF4 write failed\n", __func__);
			goto exit;
		}
	} else    {
		
		radio->marimba->mod_id = MARIMBA_SLAVE_ID_MARIMBA;
	}

	value = 0x00;
	retval = marimba_write_bit_mask(&config, MARIMBA_XO_BUFF_CNTRL,
							&value, 1, FM_ENABLE);
	if (retval < 0) {
		printk(KERN_ERR "%s:XO_BUFF_CNTRL write failed\n", __func__);
		goto exit;
	}
exit:
	FMDBG("%s, Calling fm_shutdown\n", __func__);
	queue_delayed_work(radio->wqueue, &radio->work,
				msecs_to_jiffies(TAVARUA_DELAY/2));
	
	marimba_set_fm_status(radio->marimba, false);
	wait_for_completion(&radio->shutdown_done);
	radio->handle_irq = 1;
	radio->lp_mode = 1;
	radio->spur_table_size = 0;
	atomic_inc(&radio->users);
	radio->marimba->mod_id = SLAVE_ID_BAHAMA;
	flush_workqueue(radio->wqueue);
	return retval;
}

static const struct v4l2_file_operations tavarua_fops = {
	.owner = THIS_MODULE,
	.read = tavarua_fops_read,
	.write = tavarua_fops_write,
	.ioctl = video_ioctl2,
	.open  = tavarua_fops_open,
	.release = tavarua_fops_release,
};


static struct v4l2_queryctrl tavarua_v4l2_queryctrl[] = {
	{
		.id	       = V4L2_CID_AUDIO_VOLUME,
		.type	       = V4L2_CTRL_TYPE_INTEGER,
		.name	       = "Volume",
		.minimum       = 0,
		.maximum       = 15,
		.step	       = 1,
		.default_value = 15,
	},
	{
		.id	       = V4L2_CID_AUDIO_BALANCE,
		.flags	       = V4L2_CTRL_FLAG_DISABLED,
	},
	{
		.id	       = V4L2_CID_AUDIO_BASS,
		.flags	       = V4L2_CTRL_FLAG_DISABLED,
	},
	{
		.id	       = V4L2_CID_AUDIO_TREBLE,
		.flags	       = V4L2_CTRL_FLAG_DISABLED,
	},
	{
		.id	       = V4L2_CID_AUDIO_MUTE,
		.type	       = V4L2_CTRL_TYPE_BOOLEAN,
		.name	       = "Mute",
		.minimum       = 0,
		.maximum       = 1,
		.step	       = 1,
		.default_value = 1,
	},
	{
		.id	       = V4L2_CID_AUDIO_LOUDNESS,
		.flags	       = V4L2_CTRL_FLAG_DISABLED,
	},
	{
		.id	       = V4L2_CID_PRIVATE_TAVARUA_SRCHMODE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name	       = "Search mode",
		.minimum       = 0,
		.maximum       = 7,
		.step	       = 1,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_SCANDWELL,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Search dwell time",
		.minimum       = 0,
		.maximum       = 7,
		.step          = 1,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_SRCHON,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "Search on/off",
		.minimum       = 0,
		.maximum       = 1,
		.step          = 1,
		.default_value = 1,

	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_STATE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "radio 0ff/rx/tx/reset",
		.minimum       = 0,
		.maximum       = 3,
		.step          = 1,
		.default_value = 1,

	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_REGION,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "radio standard",
		.minimum       = 0,
		.maximum       = 2,
		.step          = 1,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Signal Threshold",
		.minimum       = 0x80,
		.maximum       = 0x7F,
		.step          = 1,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_SRCH_PTY,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Search PTY",
		.minimum       = 0,
		.maximum       = 31,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_SRCH_PI,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Search PI",
		.minimum       = 0,
		.maximum       = 0xFF,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_SRCH_CNT,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Preset num",
		.minimum       = 0,
		.maximum       = 12,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_EMPHASIS,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "Emphasis",
		.minimum       = 0,
		.maximum       = 1,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_RDS_STD,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "RDS standard",
		.minimum       = 0,
		.maximum       = 1,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_SPACING,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Channel spacing",
		.minimum       = 0,
		.maximum       = 2,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_RDSON,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "RDS on/off",
		.minimum       = 0,
		.maximum       = 1,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_MASK,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "RDS group mask",
		.minimum       = 0,
		.maximum       = 0xFFFFFFFF,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_PROC,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "RDS processing",
		.minimum       = 0,
		.maximum       = 0xFF,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_RDSD_BUF,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "RDS data groups to buffer",
		.minimum       = 1,
		.maximum       = 21,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_PSALL,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "pass all ps strings",
		.minimum       = 0,
		.maximum       = 1,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_LP_MODE,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "Low power mode",
		.minimum       = 0,
		.maximum       = 1,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_ANTENNA,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "headset/internal",
		.minimum       = 0,
		.maximum       = 1,
		.default_value = 0,
	},
	
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_TX_SETPSREPEATCOUNT,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Set PS REPEATCOUNT",
		.minimum       = 0,
		.maximum       = 15,
	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_PS_NAME,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "Stop PS NAME",
		.minimum       = 0,
		.maximum       = 1,
	},
	{
		.id            = V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_RT,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "Stop RT",
		.minimum       = 0,
		.maximum       = 1,
	},
	{	.id	       = V4L2_CID_PRIVATE_SET_NOTCH_FILTER,
		.type	       = V4L2_CTRL_TYPE_INTEGER,
		.name	       = "Notch filter",
		.minimum       = 0,
		.maximum       = 2,
	},

};

static int tavarua_vidioc_querycap(struct file *file, void *priv,
		struct v4l2_capability *capability)
{
	struct tavarua_device *radio = video_get_drvdata(video_devdata(file));

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (unlikely(capability == NULL)) {
		FMDERR("%s:capability is null", __func__);
		return -EINVAL;
	}

	strlcpy(capability->driver, DRIVER_NAME, sizeof(capability->driver));
	strlcpy(capability->card, DRIVER_CARD, sizeof(capability->card));
	sprintf(capability->bus_info, "I2C");
	capability->capabilities = V4L2_CAP_TUNER | V4L2_CAP_RADIO;

	capability->version = radio->chipID;

	return 0;
}

static int tavarua_vidioc_queryctrl(struct file *file, void *priv,
		struct v4l2_queryctrl *qc)
{
	unsigned char i;
	int retval = -EINVAL;

	if (unlikely(qc == NULL)) {
		FMDERR("%s:qc is null", __func__);
		return -EINVAL;
	}


	for (i = 0; i < ARRAY_SIZE(tavarua_v4l2_queryctrl); i++) {
		if (qc->id && qc->id == tavarua_v4l2_queryctrl[i].id) {
			memcpy(qc, &(tavarua_v4l2_queryctrl[i]), sizeof(*qc));
			retval = 0;
			break;
		}
	}
	if (retval < 0)
		printk(KERN_WARNING DRIVER_NAME
			": query conv4ltrol failed with %d\n", retval);

	return retval;
}

static int update_spur_table(struct tavarua_device *radio)
{
	unsigned char xfr_buf[XFR_REG_NUM];
	unsigned char size = 0, tbl_size = 0;
	int index = 0, offset = 0, addr = 0x0, val = 0;
	int retval = 0, temp = 0, cnt = 0, j = 0;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	memset(xfr_buf, 0x0, XFR_REG_NUM);

	
	retval = xfr_rdwr_data(radio, XFR_READ, 1, SPUR_TABLE_ADDR, &tbl_size);
	if (retval < 0) {
		FMDERR("%s: Failed to read SPUR table size\n", __func__);
		return retval;
	}

	
	val = addr = (SPUR_TABLE_START_ADDR + (tbl_size * 3));

	
	temp = radio->spur_table_size;

	
	size = (radio->spur_table_size + tbl_size);
	retval = xfr_rdwr_data(radio, XFR_WRITE, 1, SPUR_TABLE_ADDR, &size);
	if (retval < 0) {
		FMDERR("%s: Failed to configure SPUR table size\n", __func__);
		return retval;
	}

	
	for (cnt = 0; cnt < (temp / 4); cnt++) {
		offset  = 0;
		for (j = 0; j < 4; j++) {
			xfr_buf[offset++] = GET_FREQ(COMPUTE_SPUR(
				radio->spur_data.freq[index]), 1);
			xfr_buf[offset++] = GET_FREQ(COMPUTE_SPUR(
				radio->spur_data.freq[index]), 0);
			xfr_buf[offset++] =
				radio->spur_data.rmssi[index];
			index++;
		}
		retval = xfr_rdwr_data(radio, XFR_WRITE, (SPUR_DATA_SIZE * 4),
			addr, xfr_buf);
		if (retval < 0) {
			FMDERR("%s: Failed to program SPUR frequencies\n",
				__func__);
			return retval;
		}
		addr += (SPUR_DATA_SIZE * 4);
	}

	
	temp = radio->spur_table_size;
	temp = (temp % 4);
	if (temp > 0) {
		offset = 0;
		for (j = 0; j < temp; j++) {
			xfr_buf[offset++] = GET_FREQ(COMPUTE_SPUR(
				radio->spur_data.freq[index]), 1);
			xfr_buf[offset++] = GET_FREQ(COMPUTE_SPUR(
				radio->spur_data.freq[index]), 0);
			xfr_buf[offset++] =
				radio->spur_data.rmssi[index];
			index++;
		}
		size   = (temp * SPUR_DATA_SIZE);
		retval = xfr_rdwr_data(radio, XFR_WRITE, size, addr, xfr_buf);
		if (retval < 0) {
			FMDERR("%s: Failed to program SPUR frequencies\n",
				__func__);
			return retval;
		}
	}

	return retval;
}

static int xfr_rdwr_data(struct tavarua_device *radio, int op, int size,
	unsigned long offset, unsigned char *buf) {

	unsigned char xfr_buf[XFR_REG_NUM + 1];
	int retval = 0, temp = 0;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (unlikely(buf == NULL)) {
		FMDERR("%s:buf is null", __func__);
		return -EINVAL;
	}
	
	memset(xfr_buf, 0x0, XFR_REG_NUM);

	
	temp = size;

	
	xfr_buf[XFR_MODE_OFFSET]     = LSH_DATA(size, 1);
	xfr_buf[XFR_ADDR_MSB_OFFSET] = GET_FREQ(offset, 1);
	xfr_buf[XFR_ADDR_LSB_OFFSET] = GET_FREQ(offset, 0);
	if (op == XFR_READ) {
		if (size > XFR_REG_NUM) {
			FMDERR("%s: Cant read more than 16 bytes\n", __func__);
			return -EINVAL;
		}
		xfr_buf[XFR_MODE_OFFSET] |= (XFR_PEEK_MODE);
		size = 3;
	} else if (op == XFR_WRITE) {
		if (size > (XFR_REG_NUM - 2)) {
			FMDERR("%s: Cant write more than 14 bytes\n", __func__);
			return -EINVAL;
		}
		xfr_buf[XFR_MODE_OFFSET] |= (XFR_POKE_MODE);
		memcpy(&xfr_buf[XFR_DATA_OFFSET], buf, size);
		size += 3;
	}

	
	init_completion(&radio->sync_req_done);
	retval = tavarua_write_registers(radio, XFRCTRL, xfr_buf, size);
	if (retval < 0) {
		FMDERR("%s: Failed to perform XFR operation\n", __func__);
		return retval;
	}

	
	if (!wait_for_completion_timeout(&radio->sync_req_done,
		msecs_to_jiffies(WAIT_TIMEOUT))) {
		FMDERR("Timeout: No XFR interrupt");
		return -ETIMEDOUT;
	}

	size = temp;
	if (op == XFR_READ) {
		retval = tavarua_read_registers(radio, XFRDAT0, size);
		if (retval < 0) {
			FMDERR("%s: Failed to read the XFR data\n", __func__);
			return retval;
		}
		if (buf != NULL)
			memcpy(buf, &radio->registers[XFRDAT0], size);
		else {
			FMDERR("%s: No buffer to copy XFR data\n", __func__);
			return -EINVAL;
		}
	}

	return retval;
}

static int peek_MPX_DCC(struct tavarua_device *radio)
{
	int retval = 0;
	unsigned char xfr_buf[XFR_REG_NUM];
	int MPX_DCC[] = { 0 };
	int DCC = 0;
	int ct = 0;
	unsigned char size = 0;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}


	
	size = 0x01;
	xfr_buf[0] = (XFR_POKE_MODE | (size << 1));
	xfr_buf[1] = MPX_DCC_BYPASS_POKE_MSB;
	xfr_buf[2] = MPX_DCC_BYPASS_POKE_LSB;
	xfr_buf[3] = 0x01;

	retval = tavarua_write_registers(radio, XFRCTRL, xfr_buf, 4);
	if (retval < 0) {
		FMDBG("Failed to write\n");
		return retval;
	}
	
	msleep(TAVARUA_DELAY*15);

	for (ct = 0; ct < 5; ct++)
		xfr_buf[ct] = 0;

	
	size = 0x03;
	xfr_buf[0] = (XFR_PEEK_MODE | (size << 1));
	xfr_buf[1] = MPX_DCC_PEEK_MSB_REG1;
	xfr_buf[2] = MPX_DCC_PEEK_LSB_REG1;
	retval = tavarua_write_registers(radio, XFRCTRL, xfr_buf, 3);
	if (retval < 0) {
		FMDBG("Failed to write\n");
		return retval;
	}
	
	msleep(TAVARUA_DELAY*10);
	retval = tavarua_read_registers(radio, XFRDAT0, 3);
	if (retval < 0) {
		printk(KERN_INFO "INT_DET: Read failure\n");
		return retval;
	}
	MPX_DCC[0] = (int)radio->registers[XFRDAT0];
	MPX_DCC[1] = (int)radio->registers[XFRDAT1];
	MPX_DCC[2] = (int)radio->registers[XFRDAT2];


	DCC = ((int)MPX_DCC[2] << 16) | ((int)MPX_DCC[1] << 8) |
		((int)MPX_DCC[0]);

	if (DCC & 0x00080000) {
		FMDBG(KERN_INFO "bit-19 is '1'\n");
		DCC |= 0xFFF00000;
	}


	
	size = 0x01;
	xfr_buf[0] = (XFR_POKE_MODE | (size << 1));
	xfr_buf[1] = MPX_DCC_BYPASS_POKE_MSB;
	xfr_buf[2] = MPX_DCC_BYPASS_POKE_LSB;
	xfr_buf[3] = 0x00;

	retval = tavarua_write_registers(radio, XFRCTRL, xfr_buf, 4);
	if (retval < 0) {
		FMDBG("Failed to write\n");
		return retval;
	}
	
	msleep(TAVARUA_DELAY*10);

	return DCC;
}
static int tavarua_vidioc_g_ctrl(struct file *file, void *priv,
		struct v4l2_control *ctrl)
{
	struct tavarua_device *radio = video_get_drvdata(video_devdata(file));
	int retval = 0;
	int cnt = 0;
	unsigned char xfr_buf[XFR_REG_NUM];
	signed char cRmssiThreshold;
	signed char ioc;
	unsigned char size = 0;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (unlikely(ctrl == NULL)) {
		FMDERR("%s:ctrl is null", __func__);
		return -EINVAL;
	}

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_VOLUME:
		break;
	case V4L2_CID_AUDIO_MUTE:
		ctrl->value = radio->registers[IOCTRL] & 0x03 ;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCHMODE:
		ctrl->value = radio->registers[SRCHCTRL] & SRCH_MODE;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SCANDWELL:
		ctrl->value = (radio->registers[SRCHCTRL] & SCAN_DWELL) >> 4;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCHON:
		ctrl->value = (radio->registers[SRCHCTRL] & SRCH_ON) >> 7 ;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_STATE:
		ctrl->value = (radio->registers[RDCTRL] & 0x03);
		break;
	case V4L2_CID_PRIVATE_TAVARUA_IOVERC:
		retval = tavarua_read_registers(radio, IOVERC, 1);
		if (retval < 0)
			return retval;
		ioc = radio->registers[IOVERC];
		ctrl->value = ioc;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_INTDET:
		size = 0x1;
		xfr_buf[0] = (XFR_PEEK_MODE | (size << 1));
		xfr_buf[1] = INTDET_PEEK_MSB;
		xfr_buf[2] = INTDET_PEEK_LSB;
		retval = tavarua_write_registers(radio, XFRCTRL, xfr_buf, 3);
		if (retval < 0) {
			FMDBG("Failed to write\n");
			return retval;
		}
		FMDBG("INT_DET:Sync write success\n");
		
		msleep(TAVARUA_DELAY*10);
		
		retval = tavarua_read_registers(radio, XFRDAT0, 3);
		if (retval < 0) {
			FMDBG("INT_DET: Read failure\n");
			return retval;
		}
		ctrl->value = radio->registers[XFRDAT0];
		break;
	case V4L2_CID_PRIVATE_TAVARUA_MPX_DCC:
		ctrl->value = peek_MPX_DCC(radio);
		break;
	case V4L2_CID_PRIVATE_TAVARUA_REGION:
		ctrl->value = radio->region_params.region;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH:
		retval = sync_read_xfr(radio, RX_CONFIG, xfr_buf);
		if (retval < 0) {
			FMDBG("[G IOCTL=V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH]\n");
			FMDBG("sync_read_xfr error: [retval=%d]\n", retval);
			break;
		}
		
		cRmssiThreshold = (signed char)xfr_buf[0];
		ctrl->value  = cRmssiThreshold;
		FMDBG("cRmssiThreshold: %d\n", cRmssiThreshold);
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCH_PTY:
		ctrl->value = radio->srch_params.srch_pty;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCH_PI:
		ctrl->value = radio->srch_params.srch_pi;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCH_CNT:
		ctrl->value = radio->srch_params.preset_num;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_EMPHASIS:
		ctrl->value = radio->region_params.emphasis;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDS_STD:
		ctrl->value = radio->region_params.rds_std;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SPACING:
		ctrl->value = radio->region_params.spacing;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDSON:
		ctrl->value = radio->registers[RDSCTRL] & RDS_ON;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_MASK:
		retval = sync_read_xfr(radio, RDS_CONFIG, xfr_buf);
		if (retval > -1)
			ctrl->value =   (xfr_buf[8] << 24) |
					(xfr_buf[9] << 16) |
					(xfr_buf[10] << 8) |
					 xfr_buf[11];
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_PROC:
		retval = tavarua_read_registers(radio, ADVCTRL, 1);
		if (retval > -1)
			ctrl->value = radio->registers[ADVCTRL];
		msleep(TAVARUA_DELAY*5);
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RSSI_DELTA:
		retval = sync_read_xfr(radio, RX_CONFIG, xfr_buf);
		if (retval < 0) {
			FMDERR("V4L2_CID_PRIVATE_TAVARUA_RSSI_DELTA]\n");
			FMDERR("sync_read_xfr [retval=%d]\n", retval);
			break;
		}
		ctrl->value = (unsigned char)xfr_buf[4];
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDSD_BUF:
		retval = sync_read_xfr(radio, RDS_CONFIG, xfr_buf);
		if (retval > -1)
			ctrl->value = xfr_buf[1];
		break;
	case V4L2_CID_PRIVATE_TAVARUA_PSALL:
		retval = sync_read_xfr(radio, RDS_CONFIG, xfr_buf);
		if (retval > -1)
			ctrl->value = xfr_buf[12] & RDS_CONFIG_PSALL;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_LP_MODE:
		ctrl->value = radio->lp_mode;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_ANTENNA:
		ctrl->value = GET_REG_FIELD(radio->registers[IOCTRL],
			IOC_ANTENNA_OFFSET, IOC_ANTENNA_MASK);
		break;
	case V4L2_CID_PRIVATE_INTF_LOW_THRESHOLD:
		size = 0x04;
		xfr_buf[0] = (XFR_PEEK_MODE | (size << 1));
		xfr_buf[1] = ON_CHANNEL_TH_MSB;
		xfr_buf[2] = ON_CHANNEL_TH_LSB;
		retval = tavarua_write_registers(radio, XFRCTRL, xfr_buf, 3);
		if (retval < 0) {
			pr_err("%s: Failed to write\n", __func__);
			return retval;
		}
		
		msleep(TAVARUA_DELAY*10);
		retval = tavarua_read_registers(radio, XFRDAT0, 4);
		if (retval < 0) {
			pr_err("%s: On Ch. DET: Read failure\n", __func__);
			return retval;
		}
		for (cnt = 0; cnt < 4; cnt++)
			FMDBG("On-Channel data set is : 0x%x\t",
				(int)radio->registers[XFRDAT0+cnt]);

		ctrl->value =	LSH_DATA(radio->registers[XFRDAT0],   24) |
				LSH_DATA(radio->registers[XFRDAT0+1], 16) |
				LSH_DATA(radio->registers[XFRDAT0+2],  8) |
				(radio->registers[XFRDAT0+3]);
		FMDBG("The On Channel Threshold value is : 0x%x", ctrl->value);
		break;
	case V4L2_CID_PRIVATE_INTF_HIGH_THRESHOLD:
		size = 0x04;
		xfr_buf[0] = (XFR_PEEK_MODE | (size << 1));
		xfr_buf[1] = OFF_CHANNEL_TH_MSB;
		xfr_buf[2] = OFF_CHANNEL_TH_LSB;
		retval = tavarua_write_registers(radio, XFRCTRL, xfr_buf, 3);
		if (retval < 0) {
			pr_err("%s: Failed to write\n", __func__);
			return retval;
		}
		
		msleep(TAVARUA_DELAY*10);
		retval = tavarua_read_registers(radio, XFRDAT0, 4);
		if (retval < 0) {
			pr_err("%s: Off Ch. DET: Read failure\n", __func__);
			return retval;
		}
		for (cnt = 0; cnt < 4; cnt++)
			FMDBG("Off-channel data set is : 0x%x\t",
				(int)radio->registers[XFRDAT0+cnt]);

		ctrl->value =	LSH_DATA(radio->registers[XFRDAT0],   24) |
				LSH_DATA(radio->registers[XFRDAT0+1], 16) |
				LSH_DATA(radio->registers[XFRDAT0+2],  8) |
				(radio->registers[XFRDAT0+3]);
		FMDBG("The Off Channel Threshold value is : 0x%x", ctrl->value);
		break;
	case V4L2_CID_PRIVATE_SINR_THRESHOLD:
	case V4L2_CID_PRIVATE_SINR_SAMPLES:
	case V4L2_CID_PRIVATE_IRIS_GET_SINR:
		retval = 0;
		break;
	case V4L2_CID_PRIVATE_VALID_CHANNEL:
		ctrl->value = radio->is_station_valid;
		break;
	default:
		retval = -EINVAL;
	}
	if (retval < 0)
		printk(KERN_WARNING DRIVER_NAME
		": get control failed with %d, id: %d\n", retval, ctrl->id);

	return retval;
}

static int tavarua_vidioc_s_ext_ctrls(struct file *file, void *priv,
			struct v4l2_ext_controls *ctrl)
{
	int retval = 0;
	int bytes_to_copy;
	int bytes_copied = 0;
	__u32 bytes_left = 0;
	int chunk_index = 0;
	char tx_data[XFR_REG_NUM];
	struct tavarua_device *radio = video_get_drvdata(video_devdata(file));
	char *data = NULL;
	int extra_name_byte = 0;
	int name_bytes = 0;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (unlikely(ctrl == NULL) ||
		unlikely(ctrl->controls == NULL) ||
		unlikely(ctrl->count <= 0)) {
		FMDERR("%s:ctrl is null", __func__);
		return -EINVAL;
	}

	switch ((ctrl->controls[0]).id)	{
	case V4L2_CID_RDS_TX_PS_NAME: {
		FMDBG("In V4L2_CID_RDS_TX_PS_NAME\n");
		

		chunk_index = 0;
		bytes_copied = 0;
		bytes_left = min((ctrl->controls[0]).size,
			(__u32)MAX_PS_LENGTH);
		data = (ctrl->controls[0]).string;

		
		while (bytes_left) {
			chunk_index++;
			FMDBG("chunk is %d", chunk_index);
			bytes_to_copy = min(bytes_left, (__u32)XFR_REG_NUM);
			
			memset(tx_data, 0, XFR_REG_NUM);
			if (copy_from_user(tx_data,
				data + bytes_copied, bytes_to_copy))
				return -EFAULT;
			retval = sync_write_xfr(radio,
				RDS_PS_0 + chunk_index, tx_data);
			if (retval < 0)	{
				FMDBG("sync_write_xfr:  %d", retval);
				return retval;
			}
			bytes_copied += bytes_to_copy;
			bytes_left -= bytes_to_copy;
		}
		memset(tx_data, 0, XFR_REG_NUM);
		
		FMDBG("Writing PS header\n");
		extra_name_byte = (bytes_copied%8) ? 1 : 0;
		name_bytes = (bytes_copied/8) + extra_name_byte;
		
		tx_data[0] = (name_bytes) & MASK_TXREPCOUNT;
		tx_data[1] = radio->pty & MASK_PTY; 
		tx_data[2] = ((radio->pi & MASK_PI_MSB) >> 8);
		tx_data[3] = radio->pi & MASK_PI_LSB;
		
		tx_data[4] = TX_ON |
		    (radio->ps_repeatcount & MASK_TXREPCOUNT);
		retval = sync_write_xfr(radio, RDS_PS_0, tx_data);
		if (retval < 0)	{
			FMDBG("sync_write_xfr returned %d", retval);
			return retval;
		}
	} break;
	case V4L2_CID_RDS_TX_RADIO_TEXT: {
		chunk_index = 0;
		bytes_copied = 0;
		FMDBG("In V4L2_CID_RDS_TX_RADIO_TEXT\n");
		
		FMDBG("Passed RT String : %s\n",
			(ctrl->controls[0]).string);
		bytes_left =
		    min((ctrl->controls[0]).size, (__u32)MAX_RT_LENGTH);
		data = (ctrl->controls[0]).string;
		
		while (bytes_left) {
			chunk_index++;
			bytes_to_copy = min(bytes_left, (__u32)XFR_REG_NUM);
			memset(tx_data, 0, XFR_REG_NUM);
			if (copy_from_user(tx_data,
				    data + bytes_copied, bytes_to_copy))
				return -EFAULT;
			retval = sync_write_xfr(radio,
				RDS_RT_0 + chunk_index, tx_data);
			if (retval < 0)
				return retval;
			bytes_copied += bytes_to_copy;
			bytes_left -= bytes_to_copy;
		}
		
		tx_data[0] = bytes_copied;
		
		tx_data[1] = TX_ON | ((radio->pty & MASK_PTY) >> 8);
		
		tx_data[2] = ((radio->pi & MASK_PI_MSB) >> 8);
		
		tx_data[3] = radio->pi & MASK_PI_LSB;
		retval = sync_write_xfr(radio, RDS_RT_0 , tx_data);
		if (retval < 0)
			return retval;
		FMDBG("done RT writing: %d\n", retval);
	} break;
	default:
	{
		FMDBG("Shouldn't reach here\n");
		retval = -1;
	}
	}
	return retval;
}

static int tavarua_vidioc_s_ctrl(struct file *file, void *priv,
		struct v4l2_control *ctrl)
{
	struct tavarua_device *radio = video_get_drvdata(video_devdata(file));
	int retval = 0, size = 0, cnt = 0;
	unsigned char value;
	unsigned char xfr_buf[XFR_REG_NUM];
	unsigned char tx_data[XFR_REG_NUM];
	unsigned char dis_buf[XFR_REG_NUM];
	unsigned int freq = 0, mpx_dcc = 0;
	unsigned long curr = 0, prev = 0;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (unlikely(ctrl == NULL)) {
		FMDERR("%s:ctrl is null", __func__);
		return -EINVAL;
	}

	memset(xfr_buf, 0x0, XFR_REG_NUM);

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_VOLUME:
		break;
	case V4L2_CID_AUDIO_MUTE:
		value = (radio->registers[IOCTRL] & ~IOC_HRD_MUTE) |
							(ctrl->value & 0x03);
		retval = tavarua_write_register(radio, IOCTRL, value);
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCHMODE:
		value = (radio->registers[SRCHCTRL] & ~SRCH_MODE) |
							ctrl->value;
		radio->registers[SRCHCTRL] = value;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SCANDWELL:
		value = (radio->registers[SRCHCTRL] & ~SCAN_DWELL) |
						(ctrl->value << 4);
		radio->registers[SRCHCTRL] = value;
		break;
	
	case V4L2_CID_PRIVATE_TAVARUA_SRCHON:
		FMDBG("starting search\n");
		tavarua_search(radio, ctrl->value, SRCH_DIR_UP);
		break;
	case V4L2_CID_PRIVATE_TAVARUA_STATE:
		
		radio->handle_irq = 1;
		if (((ctrl->value == FM_RECV) || (ctrl->value == FM_TRANS))
				    && !(radio->registers[RDCTRL] &
							ctrl->value)) {
			FMDBG("clearing flags\n");
			init_completion(&radio->sync_xfr_start);
			init_completion(&radio->sync_req_done);
			radio->xfr_in_progress = 0;
			radio->xfr_bytes_left = 0;
			FMDBG("turning on ..\n");
			retval = tavarua_start(radio, ctrl->value);
			if (retval >= 0) {
				
				value = (radio->registers[IOCTRL] |
				    IOC_SFT_MUTE | IOC_SIG_BLND);
				retval = tavarua_write_register(radio,
					IOCTRL, value);
				if (retval < 0)
					FMDBG("SMute and SBlending"
						"not enabled\n");
			}
		}
		
		else if ((ctrl->value == FM_OFF) && radio->registers[RDCTRL]) {
			radio->spur_table_size = 0;
			FMDBG("%s: turning off...\n", __func__);
			tavarua_write_register(radio, RDCTRL, ctrl->value);
			
			kfifo_reset(&radio->data_buf[TAVARUA_BUF_EVENTS]);
			flush_workqueue(radio->wqueue);
			tavarua_q_event(radio, TAVARUA_EVT_RADIO_DISABLED);

			FMDBG("%s, Disable All Interrupts\n", __func__);
			
			dis_buf[STATUS_REG1] = 0x00;
			dis_buf[STATUS_REG2] = 0x00;
			dis_buf[STATUS_REG3] = TRANSFER;
			retval = sync_write_xfr(radio, INT_CTRL, dis_buf);
			if (retval < 0) {
				pr_err("%s: failed to disable"
						"Interrupts\n", __func__);
				return retval;
			}
		}
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SET_AUDIO_PATH:
		FMDBG("Setting audio path ...\n");
		if (ctrl->value == FM_DIGITAL_PATH) {
			FMDBG("Digital audio path enabled ...\n");
			retval = tavarua_set_audio_path(
				TAVARUA_AUDIO_OUT_DIGITAL_ON,
				TAVARUA_AUDIO_OUT_ANALOG_OFF);
			if (retval < 0) {
				FMDERR("Error in tavarua_set_audio_path"
					" %d\n", retval);
			}
		} else if (ctrl->value == FM_ANALOG_PATH) {
			FMDBG("Analog audio path enabled ...\n");
			retval = tavarua_set_audio_path(
				TAVARUA_AUDIO_OUT_DIGITAL_OFF,
				TAVARUA_AUDIO_OUT_ANALOG_ON);
			if (retval < 0) {
				FMDERR("Error in tavarua_set_audio_path"
					" %d\n", retval);
			}
		}
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCH_ALGORITHM:
		radio->enable_optimized_srch_alg = ctrl->value;
		FMDBG("V4L2_CID_PRIVATE_TAVARUA_SRCH_ALGORITHM : %d",
			radio->enable_optimized_srch_alg);
		break;
	case V4L2_CID_PRIVATE_TAVARUA_REGION:
		retval = tavarua_set_region(radio, ctrl->value);
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH:
		retval = sync_read_xfr(radio, RX_CONFIG, xfr_buf);
		if (retval < 0)	{
			FMDERR("V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH]\n");
			FMDERR("sync_read_xfr [retval=%d]\n", retval);
			break;
		}
		
		xfr_buf[0] = (unsigned char)ctrl->value;
		xfr_buf[1] = (unsigned char)ctrl->value;
		retval = sync_write_xfr(radio, RX_CONFIG, xfr_buf);
		if (retval < 0) {
			FMDERR("V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH]\n");
			FMDERR("sync_write_xfr [retval=%d]\n", retval);
			break;
		}
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCH_PTY:
		radio->srch_params.srch_pty = ctrl->value;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCH_PI:
		radio->srch_params.srch_pi = ctrl->value;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SRCH_CNT:
		radio->srch_params.preset_num = ctrl->value;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_EMPHASIS:
		radio->region_params.emphasis = ctrl->value;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDS_STD:
		radio->region_params.rds_std = ctrl->value;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_SPACING:
		radio->region_params.spacing = ctrl->value;
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDSON:
		retval = 0;
		if (ctrl->value != (radio->registers[RDSCTRL] & RDS_ON)) {
			value = radio->registers[RDSCTRL] | ctrl->value;
			retval = tavarua_write_register(radio, RDSCTRL, value);
		}
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_MASK:
		retval = sync_read_xfr(radio, RDS_CONFIG, xfr_buf);
		if (retval < 0)
			break;
		xfr_buf[8] = (ctrl->value & 0xFF000000) >> 24;
		xfr_buf[9] = (ctrl->value & 0x00FF0000) >> 16;
		xfr_buf[10] = (ctrl->value & 0x0000FF00) >> 8;
		xfr_buf[11] = (ctrl->value & 0x000000FF);
		retval = sync_write_xfr(radio, RDS_CONFIG, xfr_buf);
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_PROC:
		value  = radio->registers[ADVCTRL] | ctrl->value;
		retval = tavarua_write_register(radio, ADVCTRL, value);
		break;
	case V4L2_CID_PRIVATE_TAVARUA_AF_JUMP:
		retval = tavarua_read_registers(radio, ADVCTRL, 1);
		SET_REG_FIELD(radio->registers[ADVCTRL], ctrl->value,
			RDSAF_OFFSET, RDSAF_MASK);
		msleep(TAVARUA_DELAY*5);
		retval = tavarua_write_register(radio,
			ADVCTRL, radio->registers[ADVCTRL]);
		msleep(TAVARUA_DELAY*5);
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RSSI_DELTA:
		retval = sync_read_xfr(radio, RX_CONFIG, xfr_buf);
		if (retval < 0) {
			FMDERR("V4L2_CID_PRIVATE_TAVARUA_RSSI_DELTA]\n");
			FMDERR("sync_read_xfr [retval=%d]\n", retval);
			break;
		}
		xfr_buf[4] = (unsigned char)ctrl->value;
		retval = sync_write_xfr(radio, RX_CONFIG, xfr_buf);
		if (retval < 0) {
			FMDERR("V4L2_CID_PRIVATE_TAVARUA_RSSI_DELTA]\n");
			FMDERR("sync_write_xfr [retval=%d]\n", retval);
			break;
		}
		break;
	case V4L2_CID_PRIVATE_TAVARUA_HLSI:
		retval = tavarua_read_registers(radio, RDCTRL, 1);
		SET_REG_FIELD(radio->registers[RDCTRL], ctrl->value,
			RDCTRL_HLSI_OFFSET, RDCTRL_HLSI_MASK);
		retval = tavarua_write_register(radio, RDCTRL,
			radio->registers[RDCTRL]);
		break;
	case V4L2_CID_PRIVATE_TAVARUA_RDSD_BUF:
		retval = sync_read_xfr(radio, RDS_CONFIG, xfr_buf);
		if (retval < 0)
			break;
		xfr_buf[1] = ctrl->value;
		retval = sync_write_xfr(radio, RDS_CONFIG, xfr_buf);
		break;
	case V4L2_CID_PRIVATE_TAVARUA_PSALL:
		retval = sync_read_xfr(radio, RDS_CONFIG, xfr_buf);
		value = ctrl->value & RDS_CONFIG_PSALL;
		if (retval < 0)
			break;
		xfr_buf[12] &= ~RDS_CONFIG_PSALL;
		xfr_buf[12] |= value;
		retval = sync_write_xfr(radio, RDS_CONFIG, xfr_buf);
		break;
	case V4L2_CID_PRIVATE_TAVARUA_LP_MODE:
		retval = 0;
		if (ctrl->value == radio->lp_mode)
			break;
		if (ctrl->value) {
			FMDBG("going into low power mode\n");
			retval = tavarua_disable_interrupts(radio);
		} else {
			FMDBG("going into normal power mode\n");
			tavarua_setup_interrupts(radio,
				(radio->registers[RDCTRL] & 0x03));
		}
		break;
	case V4L2_CID_PRIVATE_TAVARUA_ANTENNA:
		SET_REG_FIELD(radio->registers[IOCTRL], ctrl->value,
					IOC_ANTENNA_OFFSET, IOC_ANTENNA_MASK);
		break;
	case V4L2_CID_PRIVATE_INTF_LOW_THRESHOLD:
		size = 0x04;
		
		xfr_buf[0] = (XFR_POKE_MODE | (size << 1));
		xfr_buf[1] = ON_CHANNEL_TH_MSB;
		xfr_buf[2] = ON_CHANNEL_TH_LSB;
		
		xfr_buf[3] = (ctrl->value & 0xFF000000) >> 24;
		xfr_buf[4] = (ctrl->value & 0x00FF0000) >> 16;
		xfr_buf[5] = (ctrl->value & 0x0000FF00) >> 8;
		xfr_buf[6] = (ctrl->value & 0x000000FF);

		for (cnt = 3; cnt < 7; cnt++) {
			FMDBG("On-channel data to be poked is : %d",
				(int)xfr_buf[cnt]);
		}

		retval = tavarua_write_registers(radio, XFRCTRL,
				xfr_buf, size+3);
		if (retval < 0) {
			pr_err("%s: Failed to write\n", __func__);
			return retval;
		}
		
		msleep(TAVARUA_DELAY*10);
		break;
	case V4L2_CID_PRIVATE_INTF_HIGH_THRESHOLD:
		size = 0x04;
		
		xfr_buf[0] = (XFR_POKE_MODE | (size << 1));
		xfr_buf[1] = OFF_CHANNEL_TH_MSB;
		xfr_buf[2] = OFF_CHANNEL_TH_LSB;
		
		xfr_buf[3] = (ctrl->value & 0xFF000000) >> 24;
		xfr_buf[4] = (ctrl->value & 0x00FF0000) >> 16;
		xfr_buf[5] = (ctrl->value & 0x0000FF00) >> 8;
		xfr_buf[6] = (ctrl->value & 0x000000FF);

		for (cnt = 3; cnt < 7; cnt++) {
			FMDBG("Off-channel data to be poked is : %d",
				(int)xfr_buf[cnt]);
		}

		retval = tavarua_write_registers(radio, XFRCTRL,
				xfr_buf, size+3);
		if (retval < 0) {
			pr_err("%s: Failed to write\n", __func__);
			return retval;
		}
		
		msleep(TAVARUA_DELAY*10);
		break;
	

	case V4L2_CID_RDS_TX_PTY: {
			radio->pty = ctrl->value;
		} break;
	case V4L2_CID_RDS_TX_PI: {
			radio->pi = ctrl->value;
		} break;
	case V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_PS_NAME: {
			FMDBG("In STOP_RDS_TX_PS_NAME\n");
			
			memset(tx_data, '0', XFR_REG_NUM);
			FMDBG("Writing PS header\n");
			retval = sync_write_xfr(radio, RDS_PS_0, tx_data);
			FMDBG("retval of PS Header write: %d", retval);

		} break;

	case V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_RT: {
			memset(tx_data, '0', XFR_REG_NUM);
			FMDBG("Writing RT header\n");
			retval = sync_write_xfr(radio, RDS_RT_0, tx_data);
			FMDBG("retval of Header write: %d", retval);

		} break;

	case V4L2_CID_PRIVATE_TAVARUA_TX_SETPSREPEATCOUNT: {
			radio->ps_repeatcount = ctrl->value;
		} break;
	case V4L2_CID_TUNE_POWER_LEVEL: {
		unsigned char tx_power_lvl_config[FM_TX_PWR_LVL_MAX+1] = {
			0x85, 
			0x95, 
			0x9D, 
			0xA5, 
			0xAD, 
			0xB5, 
			0xBD, 
			0xBF  
		};
		if (ctrl->value > FM_TX_PWR_LVL_MAX)
			ctrl->value = FM_TX_PWR_LVL_MAX;
		if (ctrl->value < FM_TX_PWR_LVL_0)
			ctrl->value = FM_TX_PWR_LVL_0;
		retval = sync_read_xfr(radio, PHY_TXGAIN, xfr_buf);
		FMDBG("return for PHY_TXGAIN is %d", retval);
		if (retval < 0) {
			FMDBG("read failed");
			break;
		}
		xfr_buf[2] = tx_power_lvl_config[ctrl->value];
		retval = sync_write_xfr(radio, PHY_TXGAIN, xfr_buf);
		FMDBG("return for write PHY_TXGAIN is %d", retval);
		if (retval < 0)
			FMDBG("write failed");
	} break;
	case V4L2_CID_PRIVATE_SOFT_MUTE:
		radio->registers[IOCTRL] &= ~(IOC_SFT_MUTE);
		if (ctrl->value)
			radio->registers[IOCTRL] |= IOC_SFT_MUTE;
		retval = tavarua_write_register(radio, IOCTRL,
					radio->registers[IOCTRL]);
		if (retval < 0)
			FMDERR("Failed to enable/disable SMute\n");
		break;
	case V4L2_CID_PRIVATE_RIVA_ACCS_ADDR:
	case V4L2_CID_PRIVATE_RIVA_ACCS_LEN:
	case V4L2_CID_PRIVATE_RIVA_PEEK:
	case V4L2_CID_PRIVATE_RIVA_POKE:
	case V4L2_CID_PRIVATE_SSBI_ACCS_ADDR:
	case V4L2_CID_PRIVATE_SSBI_PEEK:
	case V4L2_CID_PRIVATE_SSBI_POKE:
	case V4L2_CID_PRIVATE_RDS_GRP_COUNTERS:
	case V4L2_CID_PRIVATE_SET_NOTCH_FILTER:
	case V4L2_CID_PRIVATE_TAVARUA_DO_CALIBRATION:
	case V4L2_CID_PRIVATE_SINR_THRESHOLD:
	case V4L2_CID_PRIVATE_SINR_SAMPLES:
	case V4L2_CID_PRIVATE_SPUR_SELECTION:
		retval = 0;
		break;
	case V4L2_CID_PRIVATE_SPUR_FREQ:
		radio->spur_data.freq[radio->spur_table_size] =
			ctrl->value;
		break;
	case V4L2_CID_PRIVATE_SPUR_FREQ_RMSSI:
		radio->spur_data.rmssi[radio->spur_table_size++] =
			ctrl->value;
		break;
	case V4L2_CID_PRIVATE_UPDATE_SPUR_TABLE:
		retval = update_spur_table(radio);
		break;
	case V4L2_CID_PRIVATE_TX_TONE:
		retval = 0;
		memset(xfr_buf, 0, sizeof(xfr_buf));
		switch (ctrl->value) {
		case ONE_KHZ_LR_EQUA_0DBFS:
			xfr_buf[TONE_CHANNEL_EN_AND_SCALING_BYTE]
				= TONE_LEFT_RIGHT_CH_ENABLED;
			xfr_buf[TONE_LEFT_FREQ_BYTE] = 0x01;
			xfr_buf[TONE_RIGHT_FREQ_BYTE] = 0x01;
			break;
		case ONE_KHZ_LEFTONLY_EQUA_0DBFS:
			xfr_buf[TONE_CHANNEL_EN_AND_SCALING_BYTE]
				 = TONE_LEFT_CH_ENABLED;
			xfr_buf[TONE_LEFT_FREQ_BYTE] = 0x01;
			break;
		case ONE_KHZ_RIGHTONLY_EQUA_0DBFS:
			xfr_buf[TONE_CHANNEL_EN_AND_SCALING_BYTE]
				 = TONE_RIGHT_CH_ENABLED;
			xfr_buf[TONE_RIGHT_FREQ_BYTE] = 0x01;
			break;
		case ONE_KHZ_LR_EQUA_l8DBFS:
			xfr_buf[TONE_CHANNEL_EN_AND_SCALING_BYTE]
				 = (LSH_DATA(TONE_SCALE_IND_12,
						 TONE_SCALING_SHIFT)
					 | TONE_LEFT_RIGHT_CH_ENABLED);
			xfr_buf[TONE_LEFT_FREQ_BYTE] = 0x01;
			xfr_buf[TONE_RIGHT_FREQ_BYTE] = 0x01;
			break;
		case FIFTEEN_KHZ_LR_EQUA_l8DBFS:
			xfr_buf[TONE_CHANNEL_EN_AND_SCALING_BYTE]
				 = (LSH_DATA(TONE_SCALE_IND_12,
						 TONE_SCALING_SHIFT)
					 | TONE_LEFT_RIGHT_CH_ENABLED);
			xfr_buf[TONE_LEFT_FREQ_BYTE] = 0x0F;
			xfr_buf[TONE_RIGHT_FREQ_BYTE] = 0x0F;
			break;
		default:
			retval = -1;
			FMDERR("tone generator value not valid\n");
			break;
		}
		if (retval >= 0) {
			xfr_buf[TONE_GEN_CTRL_BYTE] = 0x01;
			retval = sync_write_xfr(radio, XFR_EXT, xfr_buf);
		}
		if (retval < 0)
			FMDERR("Tone generator failed\n");
		break;
	case V4L2_CID_PRIVATE_VALID_CHANNEL:
		
		atomic_set(&radio->validate_channel, 1);

		FMDBG("Going into low power mode\n");
		retval = tavarua_disable_interrupts(radio);

		if (ctrl->value == REGION_JAPAN_STANDARD_BAND_LOW)
			freq = (ctrl->value + ADJ_CHANNEL_KHZ);
		else
			freq = (ctrl->value - ADJ_CHANNEL_KHZ);
		INIT_COMPLETION(radio->sync_req_done);
		retval = tavarua_set_freq(radio, (freq * TUNE_MULT));
		if (retval < 0) {
			FMDERR("Failed to tune to adjacent station\n");
			goto error;
		}
		if (!wait_for_completion_timeout(&radio->sync_req_done,
			msecs_to_jiffies(wait_timeout))) {
			FMDERR("Timeout: No Tune response\n");
			retval = -ETIMEDOUT;
			goto error;
		}

		msleep(TAVARUA_DELAY * 10);

		
		retval = compute_MPX_DCC(radio, &mpx_dcc);
		if (retval < 0) {
			FMDERR("Failed to get MPX_DCC of adjacent station\n");
			goto error;
		}
		
		prev = abs(mpx_dcc);

		
		INIT_COMPLETION(radio->sync_req_done);
		retval = tavarua_set_freq(radio, (ctrl->value * TUNE_MULT));
		if (retval < 0) {
			FMDERR("Failed to tune to requested station\n");
			goto error;
		}
		if (!wait_for_completion_timeout(&radio->sync_req_done,
			msecs_to_jiffies(wait_timeout))) {
			FMDERR("Timeout: No Tune response\n");
			retval = -ETIMEDOUT;
			goto error;
		}

		msleep(TAVARUA_DELAY * 10);

		
		retval = compute_MPX_DCC(radio, &mpx_dcc);
		if (retval < 0) {
			FMDERR("Failed to get MPX_DCC of current station\n");
			goto error;
		}
		
		curr = abs(mpx_dcc);

		FMDBG("Going into normal power mode\n");
		tavarua_setup_interrupts(radio,
			(radio->registers[RDCTRL] & 0x03));

		FMDBG("Absolute MPX_DCC of current station  : %lu\n", curr);
		FMDBG("Absolute MPX_DCC of adjacent station : %lu\n", prev);

		if ((curr <= MPX_DCC_LIMIT) &&
			(prev > MPX_DCC_UPPER_LIMIT)) {
			FMDBG("%d KHz is A VALID STATION!\n", ctrl->value);
			radio->is_station_valid = VALID_CHANNEL;
		} else {
			FMDBG("%d KHz is NOT A VALID STATION!\n", ctrl->value);
			radio->is_station_valid = INVALID_CHANNEL;
		}
error:
		atomic_set(&radio->validate_channel, 0);
		break;
	default:
		retval = -EINVAL;
	}
	if (retval < 0)
		printk(KERN_WARNING DRIVER_NAME
		": set control failed with %d, id : %d\n", retval, ctrl->id);

	return retval;
}

static int compute_MPX_DCC(struct tavarua_device *radio, int *val)
{

	int DCC = 0, retval = 0;
	int MPX_DCC[3];
	unsigned char value;
	unsigned char xfr_buf[XFR_REG_NUM];

	
	value = CTRL_ON;
	retval = xfr_rdwr_data(radio, XFR_WRITE, 1, MPX_DCC_BYPASS_REG, &value);
	if (retval < 0) {
		FMDERR("%s: Failed to freeze MPX_DCC\n", __func__);
		return retval;
	}

	
	retval = xfr_rdwr_data(radio, XFR_READ, 3, MPX_DCC_DATA_REG, xfr_buf);
	if (retval < 0) {
		FMDERR("%s: Failed to read MPX_DCC\n", __func__);
		return retval;
	}
	MPX_DCC[0] = xfr_buf[0];
	MPX_DCC[1] = xfr_buf[1];
	MPX_DCC[2] = xfr_buf[2];
	DCC = (LSH_DATA(MPX_DCC[2], 16) | LSH_DATA(MPX_DCC[1], 8) | MPX_DCC[0]);

	
	if (DCC & 0x00080000)
		DCC |= 0xFFF00000;

	*val = DCC;

	
	value = CTRL_OFF;
	retval = xfr_rdwr_data(radio, XFR_WRITE, 1, 0x88C0, &value);
	if (retval < 0) {
		FMDERR("%s: Failed to un-freeze MPX_DCC\n", __func__);
		return retval;
	}

	return retval;
}

static int tavarua_vidioc_g_tuner(struct file *file, void *priv,
		struct v4l2_tuner *tuner)
{
	struct tavarua_device *radio = video_get_drvdata(video_devdata(file));
	int retval;
	unsigned char xfr_buf[XFR_REG_NUM];
	char rmssi = 0;
	unsigned char size = 0;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (unlikely(tuner == NULL)) {
		FMDERR("%s:tuner is null", __func__);
		return -EINVAL;
	}

	if (tuner->index > 0)
		return -EINVAL;

	
	retval = tavarua_read_registers(radio, IOCTRL, 1);
	if (retval < 0)
		return retval;
	
	size = 0x1;
	xfr_buf[0] = (XFR_PEEK_MODE | (size << 1));
	xfr_buf[1] = RMSSI_PEEK_MSB;
	xfr_buf[2] = RMSSI_PEEK_LSB;
	retval = tavarua_write_registers(radio, XFRCTRL, xfr_buf, 3);
	msleep(TAVARUA_DELAY*10);
	retval = tavarua_read_registers(radio, XFRDAT0, 3);
	rmssi = radio->registers[XFRDAT0];
	tuner->signal = rmssi;

	strcpy(tuner->name, "FM");
	tuner->type = V4L2_TUNER_RADIO;
	tuner->rangelow  =  radio->region_params.band_low;
	tuner->rangehigh =  radio->region_params.band_high;
	tuner->rxsubchans = V4L2_TUNER_SUB_MONO | V4L2_TUNER_SUB_STEREO;
	tuner->capability = V4L2_TUNER_CAP_LOW;

	
	if (radio->registers[IOCTRL] & IOC_MON_STR)
		tuner->audmode = V4L2_TUNER_MODE_STEREO;
	else
	  tuner->audmode = V4L2_TUNER_MODE_MONO;

	
	tuner->afc = 0;

	return 0;
}

static int tavarua_vidioc_s_tuner(struct file *file, void *priv,
		struct v4l2_tuner *tuner)
{
	struct tavarua_device *radio = video_get_drvdata(video_devdata(file));
	int retval;
	int audmode;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (unlikely(tuner == NULL)) {
		FMDERR("%s:tuner is null", __func__);
		return -EINVAL;
	}

	if (tuner->index > 0)
		return -EINVAL;

	FMDBG("%s: set low to %d\n", __func__, tuner->rangelow);
	radio->region_params.band_low = tuner->rangelow;
	radio->region_params.band_high = tuner->rangehigh;
	if (tuner->audmode == V4L2_TUNER_MODE_MONO)
		
		audmode = (radio->registers[IOCTRL] | IOC_MON_STR);
	 else
		
		audmode = (radio->registers[IOCTRL] & ~IOC_MON_STR);
	retval = tavarua_write_register(radio, IOCTRL, audmode);
	if (retval < 0)
		printk(KERN_WARNING DRIVER_NAME
			": set tuner failed with %d\n", retval);

	return retval;
}

static int tavarua_vidioc_g_frequency(struct file *file, void *priv,
		struct v4l2_frequency *freq)
{
	struct tavarua_device *radio = video_get_drvdata(video_devdata(file));

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (unlikely(freq == NULL)) {
		FMDERR("%s:freq is null", __func__);
		return -EINVAL;
	}

	freq->type = V4L2_TUNER_RADIO;
	return tavarua_get_freq(radio, freq);

}

static int tavarua_vidioc_s_frequency(struct file *file, void *priv,
					struct v4l2_frequency *freq)
{
	struct tavarua_device *radio = video_get_drvdata(video_devdata(file));
	int retval = -1;
	struct v4l2_frequency getFreq;

	FMDBG("%s\n", __func__);

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (unlikely(freq == NULL)) {
		FMDERR("%s:freq is null", __func__);
		return -EINVAL;
	}

	if (freq->type != V4L2_TUNER_RADIO)
		return -EINVAL;

	FMDBG("Calling tavarua_set_freq\n");

	INIT_COMPLETION(radio->sync_req_done);
	retval = tavarua_set_freq(radio, freq->frequency);
	if (retval < 0) {
		printk(KERN_WARNING DRIVER_NAME
			": set frequency failed with %d\n", retval);
	} else {
		if (!wait_for_completion_timeout(&radio->sync_req_done,
			msecs_to_jiffies(wait_timeout))) {
			FMDERR("Timeout: No Tune response");
			retval = tavarua_get_freq(radio, &getFreq);
			radio->tune_req = 0;
			if (retval > 0) {
				if (getFreq.frequency == freq->frequency) {
					
					tavarua_q_event(radio,
						TAVARUA_EVT_TUNE_SUCC);
					return 0;
				} else {
					return -EIO;
				}
			}
		}
	}
	radio->tune_req = 0;
	return retval;
}

static int tavarua_vidioc_dqbuf(struct file *file, void *priv,
				struct v4l2_buffer *buffer)
{

	struct tavarua_device  *radio = video_get_drvdata(video_devdata(file));
	enum tavarua_buf_t buf_type = -1;
	unsigned char buf_fifo[STD_BUF_SIZE] = {0};
	struct kfifo *data_fifo = NULL;
	unsigned char *buf = NULL;
	unsigned int len = 0, retval = -1;

	if ((radio == NULL) || (buffer == NULL)) {
		FMDERR("radio/buffer is NULL\n");
		return -ENXIO;
	}
	buf_type = buffer->index;
	buf = (unsigned char *)buffer->m.userptr;
	len = buffer->length;
	FMDBG("%s: requesting buffer %d\n", __func__, buf_type);

	if ((buf_type < TAVARUA_BUF_MAX) && (buf_type >= 0)) {
		data_fifo = &radio->data_buf[buf_type];
		if (buf_type == TAVARUA_BUF_EVENTS) {
			if (wait_event_interruptible(radio->event_queue,
				kfifo_len(data_fifo)) < 0) {
				return -EINTR;
			}
		}
	} else {
		FMDERR("invalid buffer type\n");
		return -EINVAL;
	}
	if (len <= STD_BUF_SIZE) {
		buffer->bytesused = kfifo_out_locked(data_fifo, &buf_fifo[0],
					len, &radio->buf_lock[buf_type]);
	} else {
		FMDERR("kfifo_out_locked can not use len more than 128\n");
		return -EINVAL;
	}
	retval = copy_to_user(buf, &buf_fifo[0], buffer->bytesused);
	if (retval > 0) {
		FMDERR("Failed to copy %d bytes of data\n", retval);
		return -EAGAIN;
	}

	return retval;
}

static int tavarua_vidioc_g_fmt_type_private(struct file *file, void *priv,
						struct v4l2_format *f)
{
	return 0;

}

static int tavarua_vidioc_s_hw_freq_seek(struct file *file, void *priv,
					struct v4l2_hw_freq_seek *seek)
{
	struct tavarua_device  *radio = video_get_drvdata(video_devdata(file));
	int dir;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (unlikely(seek == NULL)) {
		FMDERR("%s:seek is null", __func__);
		return -EINVAL;
	}

	if (seek->seek_upward)
		dir = SRCH_DIR_UP;
	else
		dir = SRCH_DIR_DOWN;
	FMDBG("starting search\n");
	return tavarua_search(radio, CTRL_ON, dir);
}

static const struct v4l2_ioctl_ops tavarua_ioctl_ops = {
	.vidioc_querycap              = tavarua_vidioc_querycap,
	.vidioc_queryctrl             = tavarua_vidioc_queryctrl,
	.vidioc_g_ctrl                = tavarua_vidioc_g_ctrl,
	.vidioc_s_ctrl                = tavarua_vidioc_s_ctrl,
	.vidioc_g_tuner               = tavarua_vidioc_g_tuner,
	.vidioc_s_tuner               = tavarua_vidioc_s_tuner,
	.vidioc_g_frequency           = tavarua_vidioc_g_frequency,
	.vidioc_s_frequency           = tavarua_vidioc_s_frequency,
	.vidioc_s_hw_freq_seek        = tavarua_vidioc_s_hw_freq_seek,
	.vidioc_dqbuf                 = tavarua_vidioc_dqbuf,
	.vidioc_g_fmt_type_private    = tavarua_vidioc_g_fmt_type_private,
	.vidioc_s_ext_ctrls           = tavarua_vidioc_s_ext_ctrls,
};

static struct video_device tavarua_viddev_template = {
	.fops                   = &tavarua_fops,
	.ioctl_ops              = &tavarua_ioctl_ops,
	.name                   = DRIVER_NAME,
	.release                = video_device_release,
};

static int tavarua_setup_interrupts(struct tavarua_device *radio,
					enum radio_state_t state)
{
	int retval;
	unsigned char int_ctrl[XFR_REG_NUM];

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (!radio->lp_mode)
		return 0;

	int_ctrl[STATUS_REG1] = READY | TUNE | SEARCH | SCANNEXT |
				SIGNAL | INTF | SYNC | AUDIO;
	if (state == FM_RECV)
		int_ctrl[STATUS_REG2] =  RDSDAT | RDSRT | RDSPS | RDSAF;
	else
		int_ctrl[STATUS_REG2] = TXRDSDAT | TXRDSDONE;

	int_ctrl[STATUS_REG3] = TRANSFER | ERROR;

	
    if (radio->chipID == MARIMBA_2_1 || radio->chipID == BAHAMA_1_0
		|| radio->chipID == BAHAMA_2_0 || radio->chipID == BAHAMA_2_1) {
		FMDBG("Setting interrupts\n");
		retval =  sync_write_xfr(radio, INT_CTRL, int_ctrl);
	
	} else {
		retval = tavarua_write_register(radio,
					STATUS_REG1, int_ctrl[STATUS_REG1]);
		if (retval < 0)
			return retval;

		retval = tavarua_write_register(radio,
					STATUS_REG2, int_ctrl[STATUS_REG2]);
		if (retval < 0)
			return retval;

		retval = tavarua_write_register(radio,
					STATUS_REG3, int_ctrl[STATUS_REG3]);
		if (retval < 0)
			return retval;
	}

	radio->lp_mode = 0;
	if ((radio->chipID != MARIMBA_2_1) && (radio->chipID != BAHAMA_1_0)
		&& (radio->chipID != BAHAMA_2_0)
		&& (radio->chipID != BAHAMA_2_1))
		tavarua_handle_interrupts(radio);

	return retval;

}

static int tavarua_disable_interrupts(struct tavarua_device *radio)
{
	unsigned char lpm_buf[XFR_REG_NUM];
	int retval;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (radio->lp_mode)
		return 0;
	FMDBG("%s\n", __func__);
	lpm_buf[STATUS_REG1] = TUNE | SEARCH | SCANNEXT;
	lpm_buf[STATUS_REG2] = 0x00;
	lpm_buf[STATUS_REG3] = TRANSFER;
	
	wait_timeout = 100;
	if (radio->chipID == MARIMBA_2_1 || radio->chipID == BAHAMA_1_0
		|| radio->chipID == BAHAMA_2_0 || radio->chipID == BAHAMA_2_1)
		retval = sync_write_xfr(radio, INT_CTRL, lpm_buf);
	
	else
		retval = tavarua_write_registers(radio, STATUS_REG1, lpm_buf,
							ARRAY_SIZE(lpm_buf));

	if (retval > -1 || retval == -ETIME) {
		radio->lp_mode = 1;
		
		retval = 0;
	}
	wait_timeout = WAIT_TIMEOUT;
	return retval;

}

static int tavarua_start(struct tavarua_device *radio,
				enum radio_state_t state)
{

	int retval;
	FMDBG("%s <%d>\n", __func__, state);

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	
	radio->region_params.region = TAVARUA_REGION_US;

	
	retval = tavarua_write_register(radio, RDCTRL, state);
	if (retval < 0)
		return retval;
	
	msleep(RADIO_INIT_TIME);
	
	tavarua_setup_interrupts(radio, state);
	
	radio->region_params.band_low = US_LOW_BAND * FREQ_MUL;
	radio->region_params.band_high = US_HIGH_BAND * FREQ_MUL;

	return 0;
}

static int tavarua_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct tavarua_device *radio = platform_get_drvdata(pdev);
	int retval;
	int users = 0;
	printk(KERN_INFO DRIVER_NAME "%s: radio suspend\n\n", __func__);

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (radio) {
		users = atomic_read(&radio->users);
		if (!users) {
			retval = tavarua_disable_interrupts(radio);
			if (retval < 0) {
				printk(KERN_INFO DRIVER_NAME
					"tavarua_suspend error %d\n", retval);
				return -EIO;
			}
		}
	}
	return 0;
}

static int tavarua_resume(struct platform_device *pdev)
{

	struct tavarua_device *radio = platform_get_drvdata(pdev);
	int retval;
	int users = 0;
	printk(KERN_INFO DRIVER_NAME "%s: radio resume\n\n", __func__);

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (radio) {
		users = atomic_read(&radio->users);

		if (!users) {
			retval = tavarua_setup_interrupts(radio,
			(radio->registers[RDCTRL] & 0x03));
			if (retval < 0) {
				FMDERR("Fails to write RDCTRL");
				msleep(TAVARUA_DELAY);
				retval = tavarua_setup_interrupts(radio,
				(radio->registers[RDCTRL] & 0x03));
				if (retval < 0) {
					FMDERR("Error in tavarua_resume %d\n",
								retval);
					return -EIO;
				}
			}
		}
	}
	return 0;
}

int tavarua_set_audio_path(int digital_on, int analog_on)
{
	struct tavarua_device *radio = private_data;
	int rx_on = radio->registers[RDCTRL] & FM_RECV;
	int retval = 0;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	
	FMDBG("%s: digital: %d analog: %d\n", __func__, digital_on, analog_on);
	if ((radio->pdata != NULL) && (radio->pdata->config_i2s_gpio != NULL)) {
		if (digital_on) {
			retval = radio->pdata->config_i2s_gpio(FM_I2S_ON);
			if (retval) {
				pr_err("%s: config_i2s_gpio failed\n",
								__func__);
			}
		} else {
			retval = radio->pdata->config_i2s_gpio(FM_I2S_OFF);
			if (retval) {
				pr_err("%s: config_i2s_gpio failed\n",
								__func__);
			}
		}
	}

	SET_REG_FIELD(radio->registers[AUDIOCTRL],
		((rx_on && analog_on) ? 1 : 0),
		AUDIORX_ANALOG_OFFSET,
		AUDIORX_ANALOG_MASK);
	SET_REG_FIELD(radio->registers[AUDIOCTRL],
		((rx_on && digital_on) ? 1 : 0),
		AUDIORX_DIGITAL_OFFSET,
		AUDIORX_DIGITAL_MASK);
	SET_REG_FIELD(radio->registers[AUDIOCTRL],
		(rx_on ? 0 : 1),
		AUDIOTX_OFFSET,
		AUDIOTX_MASK);

	if (!radio->pdata->is_fm_soc_i2s_master) {
		FMDBG("FM SoC is I2S Slave\n");
		SET_REG_FIELD(radio->registers[AUDIOCTRL],
		(0),
		I2SCTRL_OFFSET,
		I2SCTRL_MASK);
	} else {
		FMDBG("FM SoC is I2S Master\n");
		SET_REG_FIELD(radio->registers[AUDIOCTRL],
		(1),
		I2SCTRL_OFFSET,
		I2SCTRL_MASK);
	}
	FMDBG("%s: %x\n", __func__, radio->registers[AUDIOCTRL]);
	return tavarua_write_register(radio, AUDIOCTRL,
					radio->registers[AUDIOCTRL]);

}

static int  __init tavarua_probe(struct platform_device *pdev)
{

	struct marimba_fm_platform_data *tavarua_pdata;
	struct tavarua_device *radio;
	int retval = 0;
	int i = 0, j = 0;
	FMDBG("%s: probe called\n", __func__);

	if (unlikely(pdev == NULL)) {
		FMDERR("%s:pdev is null", __func__);
		return -EINVAL;
	}

	
	radio = kzalloc(sizeof(struct tavarua_device), GFP_KERNEL);
	if (!radio) {
		retval = -ENOMEM;
	goto err_initial;
	}

	radio->marimba = platform_get_drvdata(pdev);
	tavarua_pdata = pdev->dev.platform_data;
	radio->pdata = tavarua_pdata;
	radio->dev = &pdev->dev;
	radio->wqueue = NULL;
	platform_set_drvdata(pdev, radio);

	
	radio->videodev = video_device_alloc();
	if (!radio->videodev)
		goto err_radio;

	
	memcpy(radio->videodev, &tavarua_viddev_template,
	  sizeof(tavarua_viddev_template));

	
	for (i = 0; i < TAVARUA_BUF_MAX; i++) {
		int kfifo_alloc_rc=0;
		spin_lock_init(&radio->buf_lock[i]);

		if (i == TAVARUA_BUF_RAW_RDS)
			kfifo_alloc_rc = kfifo_alloc(&radio->data_buf[i],
				rds_buf*3, GFP_KERNEL);
		else if (i == TAVARUA_BUF_RT_RDS)
			kfifo_alloc_rc = kfifo_alloc(&radio->data_buf[i],
				STD_BUF_SIZE * 2, GFP_KERNEL);
		else
			kfifo_alloc_rc = kfifo_alloc(&radio->data_buf[i],
				STD_BUF_SIZE, GFP_KERNEL);

		if (kfifo_alloc_rc!=0) {
			printk(KERN_ERR "%s: failed allocating buffers %d\n",
				__func__, kfifo_alloc_rc);
		        retval = -ENOMEM;
		        goto err_all;
		}
	}
	
	atomic_set(&radio->users, 1);
	radio->xfr_in_progress = 0;
	radio->xfr_bytes_left = 0;
	for (j = 0; j < TAVARUA_XFR_MAX; j++)
		radio->pending_xfrs[j] = 0;

	
	radio->tx_mode = TAVARUA_TX_RT;
		
	radio->pty = 0;
	radio->pi = 0;
	radio->ps_repeatcount = 0;
		
	radio->srch_params.srch_pty = 0;
	radio->srch_params.srch_pi = 0;
	radio->srch_params.preset_num = 0;
	radio->srch_params.get_list = 0;
	
	radio->lp_mode = 1;
	radio->handle_irq = 1;
	
	mutex_init(&radio->lock);
	
	init_completion(&radio->sync_xfr_start);
	init_completion(&radio->sync_req_done);
	radio->tune_req = 0;
	
	init_waitqueue_head(&radio->event_queue);
	
	init_waitqueue_head(&radio->read_queue);

	video_set_drvdata(radio->videodev, radio);
	radio->wqueue  = create_singlethread_workqueue("kfmradio");
	if (!radio->wqueue) {
	        retval = -ENOMEM;
		goto err_all;
        }

	
	retval = video_register_device(radio->videodev, VFL_TYPE_RADIO, radio_nr);
	if (retval != 0) {
		printk(KERN_WARNING DRIVER_NAME
				": Could not register video device\n");
		goto err_all;
	}
	private_data = radio;
	return 0;

err_all:
	video_device_release(radio->videodev);
	if (radio->wqueue)
		destroy_workqueue(radio->wqueue);
	for (i--; i >= 0; i--) {
		kfifo_free(&radio->data_buf[i]);
        }
err_radio:
	kfree(radio);
err_initial:
	return retval;
}

static int __devexit tavarua_remove(struct platform_device *pdev)
{
	int i;
	struct tavarua_device *radio = platform_get_drvdata(pdev);

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	
	tavarua_disable_irq(radio);

	destroy_workqueue(radio->wqueue);

	video_unregister_device(radio->videodev);

	
	for (i = 0; i < TAVARUA_BUF_MAX; i++)
		kfifo_free(&radio->data_buf[i]);

	
	kfree(radio);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver tavarua_driver = {
	.driver = {
		.owner  = THIS_MODULE,
		.name   = "marimba_fm",
	},
	.remove = __devexit_p(tavarua_remove),
	.suspend = tavarua_suspend,
	.resume = tavarua_resume,
}; 



static int __init radio_module_init(void)
{
	printk(KERN_INFO DRIVER_DESC ", Version " DRIVER_VERSION "\n");
	return platform_driver_probe(&tavarua_driver, tavarua_probe);
}

static void __exit radio_module_exit(void)
{
  platform_driver_unregister(&tavarua_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);

module_init(radio_module_init);
module_exit(radio_module_exit);

