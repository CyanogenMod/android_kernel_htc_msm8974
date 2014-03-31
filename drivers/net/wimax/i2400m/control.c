/*
 * Intel Wireless WiMAX Connection 2400m
 * Miscellaneous control functions for managing the device
 *
 *
 * Copyright (C) 2007-2008 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Intel Corporation <linux-wimax@intel.com>
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *  - Initial implementation
 *
 * This is a collection of functions used to control the device (plus
 * a few helpers).
 *
 * There are utilities for handling TLV buffers, hooks on the device's
 * reports to act on device changes of state [i2400m_report_hook()],
 * on acks to commands [i2400m_msg_ack_hook()], a helper for sending
 * commands to the device and blocking until a reply arrives
 * [i2400m_msg_to_dev()], a few high level commands for manipulating
 * the device state, powersving mode and configuration plus the
 * routines to setup the device once communication is stablished with
 * it [i2400m_dev_initialize()].
 *
 * ROADMAP
 *
 * i2400m_dev_initialize()       Called by i2400m_dev_start()
 *   i2400m_set_init_config()
 *   i2400m_cmd_get_state()
 * i2400m_dev_shutdown()        Called by i2400m_dev_stop()
 *   i2400m_reset()
 *
 * i2400m_{cmd,get,set}_*()
 *   i2400m_msg_to_dev()
 *   i2400m_msg_check_status()
 *
 * i2400m_report_hook()         Called on reception of an event
 *   i2400m_report_state_hook()
 *     i2400m_tlv_buffer_walk()
 *     i2400m_tlv_match()
 *     i2400m_report_tlv_system_state()
 *     i2400m_report_tlv_rf_switches_status()
 *     i2400m_report_tlv_media_status()
 *   i2400m_cmd_enter_powersave()
 *
 * i2400m_msg_ack_hook()        Called on reception of a reply to a
 *                              command, get or set
 */

#include <stdarg.h>
#include "i2400m.h"
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/wimax/i2400m.h>
#include <linux/export.h>
#include <linux/moduleparam.h>


#define D_SUBMODULE control
#include "debug-levels.h"

static int i2400m_idle_mode_disabled;
module_param_named(idle_mode_disabled, i2400m_idle_mode_disabled, int, 0644);
MODULE_PARM_DESC(idle_mode_disabled,
		 "If true, the device will not enable idle mode negotiation "
		 "with the base station (when connected) to save power.");

static int i2400m_power_save_disabled;
module_param_named(power_save_disabled, i2400m_power_save_disabled, int, 0644);
MODULE_PARM_DESC(power_save_disabled,
		 "If true, the driver will not tell the device to enter "
		 "power saving mode when it reports it is ready for it. "
		 "False by default (so the device is told to do power "
		 "saving).");

static int i2400m_passive_mode;	
module_param_named(passive_mode, i2400m_passive_mode, int, 0644);
MODULE_PARM_DESC(passive_mode,
		 "If true, the driver will not do any device setup "
		 "and leave it up to user space, who must be properly "
		 "setup.");


static
ssize_t i2400m_tlv_match(const struct i2400m_tlv_hdr *tlv,
		     enum i2400m_tlv tlv_type, ssize_t tlv_size)
{
	if (le16_to_cpu(tlv->type) != tlv_type)	
		return -1;
	if (tlv_size != -1
	    && le16_to_cpu(tlv->length) + sizeof(*tlv) != tlv_size) {
		size_t size = le16_to_cpu(tlv->length) + sizeof(*tlv);
		printk(KERN_WARNING "W: tlv type 0x%x mismatched because of "
		       "size (got %zu vs %zu expected)\n",
		       tlv_type, size, tlv_size);
		return size;
	}
	return 0;
}


static
const struct i2400m_tlv_hdr *i2400m_tlv_buffer_walk(
	struct i2400m *i2400m,
	const void *tlv_buf, size_t buf_size,
	const struct i2400m_tlv_hdr *tlv_pos)
{
	struct device *dev = i2400m_dev(i2400m);
	const struct i2400m_tlv_hdr *tlv_top = tlv_buf + buf_size;
	size_t offset, length, avail_size;
	unsigned type;

	if (tlv_pos == NULL)	
		tlv_pos = tlv_buf;
	else			
		tlv_pos = (void *) tlv_pos
			+ le16_to_cpu(tlv_pos->length) + sizeof(*tlv_pos);
	if (tlv_pos == tlv_top) {	
		tlv_pos = NULL;
		goto error_beyond_end;
	}
	if (tlv_pos > tlv_top) {
		tlv_pos = NULL;
		WARN_ON(1);
		goto error_beyond_end;
	}
	offset = (void *) tlv_pos - (void *) tlv_buf;
	avail_size = buf_size - offset;
	if (avail_size < sizeof(*tlv_pos)) {
		dev_err(dev, "HW BUG? tlv_buf %p [%zu bytes], tlv @%zu: "
			"short header\n", tlv_buf, buf_size, offset);
		goto error_short_header;
	}
	type = le16_to_cpu(tlv_pos->type);
	length = le16_to_cpu(tlv_pos->length);
	if (avail_size < sizeof(*tlv_pos) + length) {
		dev_err(dev, "HW BUG? tlv_buf %p [%zu bytes], "
			"tlv type 0x%04x @%zu: "
			"short data (%zu bytes vs %zu needed)\n",
			tlv_buf, buf_size, type, offset, avail_size,
			sizeof(*tlv_pos) + length);
		goto error_short_header;
	}
error_short_header:
error_beyond_end:
	return tlv_pos;
}


static
const struct i2400m_tlv_hdr *i2400m_tlv_find(
	struct i2400m *i2400m,
	const struct i2400m_tlv_hdr *tlv_hdr, size_t size,
	enum i2400m_tlv tlv_type, ssize_t tlv_size)
{
	ssize_t match;
	struct device *dev = i2400m_dev(i2400m);
	const struct i2400m_tlv_hdr *tlv = NULL;
	while ((tlv = i2400m_tlv_buffer_walk(i2400m, tlv_hdr, size, tlv))) {
		match = i2400m_tlv_match(tlv, tlv_type, tlv_size);
		if (match == 0)		
			break;
		if (match > 0)
			dev_warn(dev, "TLV type 0x%04x found with size "
				 "mismatch (%zu vs %zu needed)\n",
				 tlv_type, match, tlv_size);
	}
	return tlv;
}


static const struct
{
	char *msg;
	int errno;
} ms_to_errno[I2400M_MS_MAX] = {
	[I2400M_MS_DONE_OK] = { "", 0 },
	[I2400M_MS_DONE_IN_PROGRESS] = { "", 0 },
	[I2400M_MS_INVALID_OP] = { "invalid opcode", -ENOSYS },
	[I2400M_MS_BAD_STATE] = { "invalid state", -EILSEQ },
	[I2400M_MS_ILLEGAL_VALUE] = { "illegal value", -EINVAL },
	[I2400M_MS_MISSING_PARAMS] = { "missing parameters", -ENOMSG },
	[I2400M_MS_VERSION_ERROR] = { "bad version", -EIO },
	[I2400M_MS_ACCESSIBILITY_ERROR] = { "accesibility error", -EIO },
	[I2400M_MS_BUSY] = { "busy", -EBUSY },
	[I2400M_MS_CORRUPTED_TLV] = { "corrupted TLV", -EILSEQ },
	[I2400M_MS_UNINITIALIZED] = { "not unitialized", -EILSEQ },
	[I2400M_MS_UNKNOWN_ERROR] = { "unknown error", -EIO },
	[I2400M_MS_PRODUCTION_ERROR] = { "production error", -EIO },
	[I2400M_MS_NO_RF] = { "no RF", -EIO },
	[I2400M_MS_NOT_READY_FOR_POWERSAVE] =
		{ "not ready for powersave", -EACCES },
	[I2400M_MS_THERMAL_CRITICAL] = { "thermal critical", -EL3HLT },
};


int i2400m_msg_check_status(const struct i2400m_l3l4_hdr *l3l4_hdr,
			    char *strbuf, size_t strbuf_size)
{
	int result;
	enum i2400m_ms status = le16_to_cpu(l3l4_hdr->status);
	const char *str;

	if (status == 0)
		return 0;
	if (status >= ARRAY_SIZE(ms_to_errno)) {
		str = "unknown status code";
		result = -EBADR;
	} else {
		str = ms_to_errno[status].msg;
		result = ms_to_errno[status].errno;
	}
	if (strbuf)
		snprintf(strbuf, strbuf_size, "%s (%d)", str, status);
	return result;
}


static
void i2400m_report_tlv_system_state(struct i2400m *i2400m,
				    const struct i2400m_tlv_system_state *ss)
{
	struct device *dev = i2400m_dev(i2400m);
	struct wimax_dev *wimax_dev = &i2400m->wimax_dev;
	enum i2400m_system_state i2400m_state = le32_to_cpu(ss->state);

	d_fnstart(3, dev, "(i2400m %p ss %p [%u])\n", i2400m, ss, i2400m_state);

	if (i2400m->state != i2400m_state) {
		i2400m->state = i2400m_state;
		wake_up_all(&i2400m->state_wq);
	}
	switch (i2400m_state) {
	case I2400M_SS_UNINITIALIZED:
	case I2400M_SS_INIT:
	case I2400M_SS_CONFIG:
	case I2400M_SS_PRODUCTION:
		wimax_state_change(wimax_dev, WIMAX_ST_UNINITIALIZED);
		break;

	case I2400M_SS_RF_OFF:
	case I2400M_SS_RF_SHUTDOWN:
		wimax_state_change(wimax_dev, WIMAX_ST_RADIO_OFF);
		break;

	case I2400M_SS_READY:
	case I2400M_SS_STANDBY:
	case I2400M_SS_SLEEPACTIVE:
		wimax_state_change(wimax_dev, WIMAX_ST_READY);
		break;

	case I2400M_SS_CONNECTING:
	case I2400M_SS_WIMAX_CONNECTED:
		wimax_state_change(wimax_dev, WIMAX_ST_READY);
		break;

	case I2400M_SS_SCAN:
	case I2400M_SS_OUT_OF_ZONE:
		wimax_state_change(wimax_dev, WIMAX_ST_SCANNING);
		break;

	case I2400M_SS_IDLE:
		d_printf(1, dev, "entering BS-negotiated idle mode\n");
	case I2400M_SS_DISCONNECTING:
	case I2400M_SS_DATA_PATH_CONNECTED:
		wimax_state_change(wimax_dev, WIMAX_ST_CONNECTED);
		break;

	default:
		
		dev_err(dev, "HW BUG? unknown state %u: shutting down\n",
			i2400m_state);
		i2400m_reset(i2400m, I2400M_RT_WARM);
		break;
	}
	d_fnend(3, dev, "(i2400m %p ss %p [%u]) = void\n",
		i2400m, ss, i2400m_state);
}


static
void i2400m_report_tlv_media_status(struct i2400m *i2400m,
				    const struct i2400m_tlv_media_status *ms)
{
	struct device *dev = i2400m_dev(i2400m);
	struct wimax_dev *wimax_dev = &i2400m->wimax_dev;
	struct net_device *net_dev = wimax_dev->net_dev;
	enum i2400m_media_status status = le32_to_cpu(ms->media_status);

	d_fnstart(3, dev, "(i2400m %p ms %p [%u])\n", i2400m, ms, status);

	switch (status) {
	case I2400M_MEDIA_STATUS_LINK_UP:
		netif_carrier_on(net_dev);
		break;
	case I2400M_MEDIA_STATUS_LINK_DOWN:
		netif_carrier_off(net_dev);
		break;
	case I2400M_MEDIA_STATUS_LINK_RENEW:
		netif_carrier_on(net_dev);
		break;
	default:
		dev_err(dev, "HW BUG? unknown media status %u\n",
			status);
	}
	d_fnend(3, dev, "(i2400m %p ms %p [%u]) = void\n",
		i2400m, ms, status);
}


static
void i2400m_report_state_parse_tlv(struct i2400m *i2400m,
				   const struct i2400m_tlv_hdr *tlv,
				   const char *tag)
{
	struct device *dev = i2400m_dev(i2400m);
	const struct i2400m_tlv_media_status *ms;
	const struct i2400m_tlv_system_state *ss;
	const struct i2400m_tlv_rf_switches_status *rfss;

	if (0 == i2400m_tlv_match(tlv, I2400M_TLV_SYSTEM_STATE, sizeof(*ss))) {
		ss = container_of(tlv, typeof(*ss), hdr);
		d_printf(2, dev, "%s: system state TLV "
			 "found (0x%04x), state 0x%08x\n",
			 tag, I2400M_TLV_SYSTEM_STATE,
			 le32_to_cpu(ss->state));
		i2400m_report_tlv_system_state(i2400m, ss);
	}
	if (0 == i2400m_tlv_match(tlv, I2400M_TLV_RF_STATUS, sizeof(*rfss))) {
		rfss = container_of(tlv, typeof(*rfss), hdr);
		d_printf(2, dev, "%s: RF status TLV "
			 "found (0x%04x), sw 0x%02x hw 0x%02x\n",
			 tag, I2400M_TLV_RF_STATUS,
			 le32_to_cpu(rfss->sw_rf_switch),
			 le32_to_cpu(rfss->hw_rf_switch));
		i2400m_report_tlv_rf_switches_status(i2400m, rfss);
	}
	if (0 == i2400m_tlv_match(tlv, I2400M_TLV_MEDIA_STATUS, sizeof(*ms))) {
		ms = container_of(tlv, typeof(*ms), hdr);
		d_printf(2, dev, "%s: Media Status TLV: %u\n",
			 tag, le32_to_cpu(ms->media_status));
		i2400m_report_tlv_media_status(i2400m, ms);
	}
}


static
void i2400m_report_state_hook(struct i2400m *i2400m,
			      const struct i2400m_l3l4_hdr *l3l4_hdr,
			      size_t size, const char *tag)
{
	struct device *dev = i2400m_dev(i2400m);
	const struct i2400m_tlv_hdr *tlv;
	size_t tlv_size = le16_to_cpu(l3l4_hdr->length);

	d_fnstart(4, dev, "(i2400m %p, l3l4_hdr %p, size %zu, %s)\n",
		  i2400m, l3l4_hdr, size, tag);
	tlv = NULL;

	while ((tlv = i2400m_tlv_buffer_walk(i2400m, &l3l4_hdr->pl,
					     tlv_size, tlv)))
		i2400m_report_state_parse_tlv(i2400m, tlv, tag);
	d_fnend(4, dev, "(i2400m %p, l3l4_hdr %p, size %zu, %s) = void\n",
		i2400m, l3l4_hdr, size, tag);
}


void i2400m_report_hook(struct i2400m *i2400m,
			const struct i2400m_l3l4_hdr *l3l4_hdr, size_t size)
{
	struct device *dev = i2400m_dev(i2400m);
	unsigned msg_type;

	d_fnstart(3, dev, "(i2400m %p l3l4_hdr %p size %zu)\n",
		  i2400m, l3l4_hdr, size);
	msg_type = le16_to_cpu(l3l4_hdr->type);
	switch (msg_type) {
	case I2400M_MT_REPORT_STATE:	
		i2400m_report_state_hook(i2400m,
					 l3l4_hdr, size, "REPORT STATE");
		break;
	case I2400M_MT_REPORT_POWERSAVE_READY:	
		if (l3l4_hdr->status == cpu_to_le16(I2400M_MS_DONE_OK)) {
			if (i2400m_power_save_disabled)
				d_printf(1, dev, "ready for powersave, "
					 "not requesting (disabled by module "
					 "parameter)\n");
			else {
				d_printf(1, dev, "ready for powersave, "
					 "requesting\n");
				i2400m_cmd_enter_powersave(i2400m);
			}
		}
		break;
	}
	d_fnend(3, dev, "(i2400m %p l3l4_hdr %p size %zu) = void\n",
		i2400m, l3l4_hdr, size);
}


static void i2400m_msg_ack_hook(struct i2400m *i2400m,
				 const struct i2400m_l3l4_hdr *l3l4_hdr,
				 size_t size)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	unsigned ack_type, ack_status;
	char strerr[32];

	ack_type = le16_to_cpu(l3l4_hdr->type);
	ack_status = le16_to_cpu(l3l4_hdr->status);
	switch (ack_type) {
	case I2400M_MT_CMD_ENTER_POWERSAVE:
		if (0) {
			result = i2400m_msg_check_status(
				l3l4_hdr, strerr, sizeof(strerr));
			if (result >= 0)
				d_printf(1, dev, "ready for power save: %zd\n",
					 size);
		}
		break;
	}
}


int i2400m_msg_size_check(struct i2400m *i2400m,
			  const struct i2400m_l3l4_hdr *l3l4_hdr,
			  size_t msg_size)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	size_t expected_size;
	d_fnstart(4, dev, "(i2400m %p l3l4_hdr %p msg_size %zu)\n",
		  i2400m, l3l4_hdr, msg_size);
	if (msg_size < sizeof(*l3l4_hdr)) {
		dev_err(dev, "bad size for message header "
			"(expected at least %zu, got %zu)\n",
			(size_t) sizeof(*l3l4_hdr), msg_size);
		result = -EIO;
		goto error_hdr_size;
	}
	expected_size = le16_to_cpu(l3l4_hdr->length) + sizeof(*l3l4_hdr);
	if (msg_size < expected_size) {
		dev_err(dev, "bad size for message code 0x%04x (expected %zu, "
			"got %zu)\n", le16_to_cpu(l3l4_hdr->type),
			expected_size, msg_size);
		result = -EIO;
	} else
		result = 0;
error_hdr_size:
	d_fnend(4, dev,
		"(i2400m %p l3l4_hdr %p msg_size %zu) = %d\n",
		i2400m, l3l4_hdr, msg_size, result);
	return result;
}



void i2400m_msg_to_dev_cancel_wait(struct i2400m *i2400m, int code)
{
	struct sk_buff *ack_skb;
	unsigned long flags;

	spin_lock_irqsave(&i2400m->rx_lock, flags);
	ack_skb = i2400m->ack_skb;
	if (ack_skb && !IS_ERR(ack_skb))
		kfree_skb(ack_skb);
	i2400m->ack_skb = ERR_PTR(code);
	spin_unlock_irqrestore(&i2400m->rx_lock, flags);
}


struct sk_buff *i2400m_msg_to_dev(struct i2400m *i2400m,
				  const void *buf, size_t buf_len)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	const struct i2400m_l3l4_hdr *msg_l3l4_hdr;
	struct sk_buff *ack_skb;
	const struct i2400m_l3l4_hdr *ack_l3l4_hdr;
	size_t ack_len;
	int ack_timeout;
	unsigned msg_type;
	unsigned long flags;

	d_fnstart(3, dev, "(i2400m %p buf %p len %zu)\n",
		  i2400m, buf, buf_len);

	rmb();		
	if (i2400m->boot_mode)
		return ERR_PTR(-EL3RST);

	msg_l3l4_hdr = buf;
	
	result = i2400m_msg_size_check(i2400m, msg_l3l4_hdr, buf_len);
	if (result < 0)
		goto error_bad_msg;
	msg_type = le16_to_cpu(msg_l3l4_hdr->type);
	d_printf(1, dev, "CMD/GET/SET 0x%04x %zu bytes\n",
		 msg_type, buf_len);
	d_dump(2, dev, buf, buf_len);

	mutex_lock(&i2400m->msg_mutex);
	spin_lock_irqsave(&i2400m->rx_lock, flags);
	i2400m->ack_skb = ERR_PTR(-EINPROGRESS);
	spin_unlock_irqrestore(&i2400m->rx_lock, flags);
	init_completion(&i2400m->msg_completion);
	result = i2400m_tx(i2400m, buf, buf_len, I2400M_PT_CTRL);
	if (result < 0) {
		dev_err(dev, "can't send message 0x%04x: %d\n",
			le16_to_cpu(msg_l3l4_hdr->type), result);
		goto error_tx;
	}

	switch (msg_type) {
	case I2400M_MT_GET_TLS_OPERATION_RESULT:
	case I2400M_MT_CMD_SEND_EAP_RESPONSE:
		ack_timeout = 5 * HZ;
		break;
	default:
		ack_timeout = HZ;
	}

	if (unlikely(i2400m->trace_msg_from_user))
		wimax_msg(&i2400m->wimax_dev, "echo", buf, buf_len, GFP_KERNEL);
	result = wait_for_completion_interruptible_timeout(
		&i2400m->msg_completion, ack_timeout);
	if (result == 0) {
		dev_err(dev, "timeout waiting for reply to message 0x%04x\n",
			msg_type);
		result = -ETIMEDOUT;
		i2400m_msg_to_dev_cancel_wait(i2400m, result);
		goto error_wait_for_completion;
	} else if (result < 0) {
		dev_err(dev, "error waiting for reply to message 0x%04x: %d\n",
			msg_type, result);
		i2400m_msg_to_dev_cancel_wait(i2400m, result);
		goto error_wait_for_completion;
	}

	spin_lock_irqsave(&i2400m->rx_lock, flags);
	ack_skb = i2400m->ack_skb;
	if (IS_ERR(ack_skb))
		result = PTR_ERR(ack_skb);
	else
		result = 0;
	i2400m->ack_skb = NULL;
	spin_unlock_irqrestore(&i2400m->rx_lock, flags);
	if (result < 0)
		goto error_ack_status;
	ack_l3l4_hdr = wimax_msg_data_len(ack_skb, &ack_len);

	
	if (unlikely(i2400m->trace_msg_from_user))
		wimax_msg(&i2400m->wimax_dev, "echo",
			  ack_l3l4_hdr, ack_len, GFP_KERNEL);
	result = i2400m_msg_size_check(i2400m, ack_l3l4_hdr, ack_len);
	if (result < 0) {
		dev_err(dev, "HW BUG? reply to message 0x%04x: %d\n",
			msg_type, result);
		goto error_bad_ack_len;
	}
	if (msg_type != le16_to_cpu(ack_l3l4_hdr->type)) {
		dev_err(dev, "HW BUG? bad reply 0x%04x to message 0x%04x\n",
			le16_to_cpu(ack_l3l4_hdr->type), msg_type);
		result = -EIO;
		goto error_bad_ack_type;
	}
	i2400m_msg_ack_hook(i2400m, ack_l3l4_hdr, ack_len);
	mutex_unlock(&i2400m->msg_mutex);
	d_fnend(3, dev, "(i2400m %p buf %p len %zu) = %p\n",
		i2400m, buf, buf_len, ack_skb);
	return ack_skb;

error_bad_ack_type:
error_bad_ack_len:
	kfree_skb(ack_skb);
error_ack_status:
error_wait_for_completion:
error_tx:
	mutex_unlock(&i2400m->msg_mutex);
error_bad_msg:
	d_fnend(3, dev, "(i2400m %p buf %p len %zu) = %d\n",
		i2400m, buf, buf_len, result);
	return ERR_PTR(result);
}


enum {
	I2400M_WAKEUP_ENABLED  = 0x01,
	I2400M_WAKEUP_DISABLED = 0x02,
	I2400M_TLV_TYPE_WAKEUP_MODE = 144,
};

struct i2400m_cmd_enter_power_save {
	struct i2400m_l3l4_hdr hdr;
	struct i2400m_tlv_hdr tlv;
	__le32 val;
} __packed;


int i2400m_cmd_enter_powersave(struct i2400m *i2400m)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *ack_skb;
	struct i2400m_cmd_enter_power_save *cmd;
	char strerr[32];

	result = -ENOMEM;
	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd == NULL)
		goto error_alloc;
	cmd->hdr.type = cpu_to_le16(I2400M_MT_CMD_ENTER_POWERSAVE);
	cmd->hdr.length = cpu_to_le16(sizeof(*cmd) - sizeof(cmd->hdr));
	cmd->hdr.version = cpu_to_le16(I2400M_L3L4_VERSION);
	cmd->tlv.type = cpu_to_le16(I2400M_TLV_TYPE_WAKEUP_MODE);
	cmd->tlv.length = cpu_to_le16(sizeof(cmd->val));
	cmd->val = cpu_to_le32(I2400M_WAKEUP_ENABLED);

	ack_skb = i2400m_msg_to_dev(i2400m, cmd, sizeof(*cmd));
	result = PTR_ERR(ack_skb);
	if (IS_ERR(ack_skb)) {
		dev_err(dev, "Failed to issue 'Enter power save' command: %d\n",
			result);
		goto error_msg_to_dev;
	}
	result = i2400m_msg_check_status(wimax_msg_data(ack_skb),
					 strerr, sizeof(strerr));
	if (result == -EACCES)
		d_printf(1, dev, "Cannot enter power save mode\n");
	else if (result < 0)
		dev_err(dev, "'Enter power save' (0x%04x) command failed: "
			"%d - %s\n", I2400M_MT_CMD_ENTER_POWERSAVE,
			result, strerr);
	else
		d_printf(1, dev, "device ready to power save\n");
	kfree_skb(ack_skb);
error_msg_to_dev:
	kfree(cmd);
error_alloc:
	return result;
}
EXPORT_SYMBOL_GPL(i2400m_cmd_enter_powersave);


enum {
	I2400M_TLV_DETAILED_DEVICE_INFO = 140
};

struct sk_buff *i2400m_get_device_info(struct i2400m *i2400m)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *ack_skb;
	struct i2400m_l3l4_hdr *cmd;
	const struct i2400m_l3l4_hdr *ack;
	size_t ack_len;
	const struct i2400m_tlv_hdr *tlv;
	const struct i2400m_tlv_detailed_device_info *ddi;
	char strerr[32];

	ack_skb = ERR_PTR(-ENOMEM);
	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd == NULL)
		goto error_alloc;
	cmd->type = cpu_to_le16(I2400M_MT_GET_DEVICE_INFO);
	cmd->length = 0;
	cmd->version = cpu_to_le16(I2400M_L3L4_VERSION);

	ack_skb = i2400m_msg_to_dev(i2400m, cmd, sizeof(*cmd));
	if (IS_ERR(ack_skb)) {
		dev_err(dev, "Failed to issue 'get device info' command: %ld\n",
			PTR_ERR(ack_skb));
		goto error_msg_to_dev;
	}
	ack = wimax_msg_data_len(ack_skb, &ack_len);
	result = i2400m_msg_check_status(ack, strerr, sizeof(strerr));
	if (result < 0) {
		dev_err(dev, "'get device info' (0x%04x) command failed: "
			"%d - %s\n", I2400M_MT_GET_DEVICE_INFO, result,
			strerr);
		goto error_cmd_failed;
	}
	tlv = i2400m_tlv_find(i2400m, ack->pl, ack_len - sizeof(*ack),
			      I2400M_TLV_DETAILED_DEVICE_INFO, sizeof(*ddi));
	if (tlv == NULL) {
		dev_err(dev, "GET DEVICE INFO: "
			"detailed device info TLV not found (0x%04x)\n",
			I2400M_TLV_DETAILED_DEVICE_INFO);
		result = -EIO;
		goto error_no_tlv;
	}
	skb_pull(ack_skb, (void *) tlv - (void *) ack_skb->data);
error_msg_to_dev:
	kfree(cmd);
error_alloc:
	return ack_skb;

error_no_tlv:
error_cmd_failed:
	kfree_skb(ack_skb);
	kfree(cmd);
	return ERR_PTR(result);
}


enum {
	I2400M_HDIv_MAJOR = 9,
	I2400M_HDIv_MINOR = 1,
	I2400M_HDIv_MINOR_2 = 2,
};


int i2400m_firmware_check(struct i2400m *i2400m)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *ack_skb;
	struct i2400m_l3l4_hdr *cmd;
	const struct i2400m_l3l4_hdr *ack;
	size_t ack_len;
	const struct i2400m_tlv_hdr *tlv;
	const struct i2400m_tlv_l4_message_versions *l4mv;
	char strerr[32];
	unsigned major, minor, branch;

	result = -ENOMEM;
	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd == NULL)
		goto error_alloc;
	cmd->type = cpu_to_le16(I2400M_MT_GET_LM_VERSION);
	cmd->length = 0;
	cmd->version = cpu_to_le16(I2400M_L3L4_VERSION);

	ack_skb = i2400m_msg_to_dev(i2400m, cmd, sizeof(*cmd));
	if (IS_ERR(ack_skb)) {
		result = PTR_ERR(ack_skb);
		dev_err(dev, "Failed to issue 'get lm version' command: %-d\n",
			result);
		goto error_msg_to_dev;
	}
	ack = wimax_msg_data_len(ack_skb, &ack_len);
	result = i2400m_msg_check_status(ack, strerr, sizeof(strerr));
	if (result < 0) {
		dev_err(dev, "'get lm version' (0x%04x) command failed: "
			"%d - %s\n", I2400M_MT_GET_LM_VERSION, result,
			strerr);
		goto error_cmd_failed;
	}
	tlv = i2400m_tlv_find(i2400m, ack->pl, ack_len - sizeof(*ack),
			      I2400M_TLV_L4_MESSAGE_VERSIONS, sizeof(*l4mv));
	if (tlv == NULL) {
		dev_err(dev, "get lm version: TLV not found (0x%04x)\n",
			I2400M_TLV_L4_MESSAGE_VERSIONS);
		result = -EIO;
		goto error_no_tlv;
	}
	l4mv = container_of(tlv, typeof(*l4mv), hdr);
	major = le16_to_cpu(l4mv->major);
	minor = le16_to_cpu(l4mv->minor);
	branch = le16_to_cpu(l4mv->branch);
	result = -EINVAL;
	if (major != I2400M_HDIv_MAJOR) {
		dev_err(dev, "unsupported major fw version "
			"%u.%u.%u\n", major, minor, branch);
		goto error_bad_major;
	}
	result = 0;
	if (minor < I2400M_HDIv_MINOR_2 && minor > I2400M_HDIv_MINOR)
		dev_warn(dev, "untested minor fw version %u.%u.%u\n",
			 major, minor, branch);
	
	i2400m->fw_version = major << 16 | minor;
	dev_info(dev, "firmware interface version %u.%u.%u\n",
		 major, minor, branch);
error_bad_major:
error_no_tlv:
error_cmd_failed:
	kfree_skb(ack_skb);
error_msg_to_dev:
	kfree(cmd);
error_alloc:
	return result;
}


int i2400m_cmd_exit_idle(struct i2400m *i2400m)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *ack_skb;
	struct i2400m_l3l4_hdr *cmd;
	char strerr[32];

	result = -ENOMEM;
	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd == NULL)
		goto error_alloc;
	cmd->type = cpu_to_le16(I2400M_MT_CMD_EXIT_IDLE);
	cmd->length = 0;
	cmd->version = cpu_to_le16(I2400M_L3L4_VERSION);

	ack_skb = i2400m_msg_to_dev(i2400m, cmd, sizeof(*cmd));
	result = PTR_ERR(ack_skb);
	if (IS_ERR(ack_skb)) {
		dev_err(dev, "Failed to issue 'exit idle' command: %d\n",
			result);
		goto error_msg_to_dev;
	}
	result = i2400m_msg_check_status(wimax_msg_data(ack_skb),
					 strerr, sizeof(strerr));
	kfree_skb(ack_skb);
error_msg_to_dev:
	kfree(cmd);
error_alloc:
	return result;

}


static int i2400m_cmd_get_state(struct i2400m *i2400m)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *ack_skb;
	struct i2400m_l3l4_hdr *cmd;
	const struct i2400m_l3l4_hdr *ack;
	size_t ack_len;
	char strerr[32];

	result = -ENOMEM;
	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd == NULL)
		goto error_alloc;
	cmd->type = cpu_to_le16(I2400M_MT_GET_STATE);
	cmd->length = 0;
	cmd->version = cpu_to_le16(I2400M_L3L4_VERSION);

	ack_skb = i2400m_msg_to_dev(i2400m, cmd, sizeof(*cmd));
	if (IS_ERR(ack_skb)) {
		dev_err(dev, "Failed to issue 'get state' command: %ld\n",
			PTR_ERR(ack_skb));
		result = PTR_ERR(ack_skb);
		goto error_msg_to_dev;
	}
	ack = wimax_msg_data_len(ack_skb, &ack_len);
	result = i2400m_msg_check_status(ack, strerr, sizeof(strerr));
	if (result < 0) {
		dev_err(dev, "'get state' (0x%04x) command failed: "
			"%d - %s\n", I2400M_MT_GET_STATE, result, strerr);
		goto error_cmd_failed;
	}
	i2400m_report_state_hook(i2400m, ack, ack_len - sizeof(*ack),
				 "GET STATE");
	result = 0;
	kfree_skb(ack_skb);
error_cmd_failed:
error_msg_to_dev:
	kfree(cmd);
error_alloc:
	return result;
}

static int i2400m_set_init_config(struct i2400m *i2400m,
				  const struct i2400m_tlv_hdr **arg,
				  size_t args)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *ack_skb;
	struct i2400m_l3l4_hdr *cmd;
	char strerr[32];
	unsigned argc, argsize, tlv_size;
	const struct i2400m_tlv_hdr *tlv_hdr;
	void *buf, *itr;

	d_fnstart(3, dev, "(i2400m %p arg %p args %zu)\n", i2400m, arg, args);
	result = 0;
	if (args == 0)
		goto none;
	argsize = 0;
	for (argc = 0; argc < args; argc++) {
		tlv_hdr = arg[argc];
		argsize += sizeof(*tlv_hdr) + le16_to_cpu(tlv_hdr->length);
	}
	WARN_ON(argc >= 9);	

	
	result = -ENOMEM;
	buf = kzalloc(sizeof(*cmd) + argsize, GFP_KERNEL);
	if (buf == NULL)
		goto error_alloc;
	cmd = buf;
	cmd->type = cpu_to_le16(I2400M_MT_SET_INIT_CONFIG);
	cmd->length = cpu_to_le16(argsize);
	cmd->version = cpu_to_le16(I2400M_L3L4_VERSION);

	
	itr = buf + sizeof(*cmd);
	for (argc = 0; argc < args; argc++) {
		tlv_hdr = arg[argc];
		tlv_size = sizeof(*tlv_hdr) + le16_to_cpu(tlv_hdr->length);
		memcpy(itr, tlv_hdr, tlv_size);
		itr += tlv_size;
	}

	
	ack_skb = i2400m_msg_to_dev(i2400m, buf, sizeof(*cmd) + argsize);
	result = PTR_ERR(ack_skb);
	if (IS_ERR(ack_skb)) {
		dev_err(dev, "Failed to issue 'init config' command: %d\n",
			result);

		goto error_msg_to_dev;
	}
	result = i2400m_msg_check_status(wimax_msg_data(ack_skb),
					 strerr, sizeof(strerr));
	if (result < 0)
		dev_err(dev, "'init config' (0x%04x) command failed: %d - %s\n",
			I2400M_MT_SET_INIT_CONFIG, result, strerr);
	kfree_skb(ack_skb);
error_msg_to_dev:
	kfree(buf);
error_alloc:
none:
	d_fnend(3, dev, "(i2400m %p arg %p args %zu) = %d\n",
		i2400m, arg, args, result);
	return result;

}

int i2400m_set_idle_timeout(struct i2400m *i2400m, unsigned msecs)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *ack_skb;
	struct {
		struct i2400m_l3l4_hdr hdr;
		struct i2400m_tlv_config_idle_timeout cit;
	} *cmd;
	const struct i2400m_l3l4_hdr *ack;
	size_t ack_len;
	char strerr[32];

	result = -ENOSYS;
	if (i2400m_le_v1_3(i2400m))
		goto error_alloc;
	result = -ENOMEM;
	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd == NULL)
		goto error_alloc;
	cmd->hdr.type = cpu_to_le16(I2400M_MT_GET_STATE);
	cmd->hdr.length = cpu_to_le16(sizeof(*cmd) - sizeof(cmd->hdr));
	cmd->hdr.version = cpu_to_le16(I2400M_L3L4_VERSION);

	cmd->cit.hdr.type =
		cpu_to_le16(I2400M_TLV_CONFIG_IDLE_TIMEOUT);
	cmd->cit.hdr.length = cpu_to_le16(sizeof(cmd->cit.timeout));
	cmd->cit.timeout = cpu_to_le32(msecs);

	ack_skb = i2400m_msg_to_dev(i2400m, cmd, sizeof(*cmd));
	if (IS_ERR(ack_skb)) {
		dev_err(dev, "Failed to issue 'set idle timeout' command: "
			"%ld\n", PTR_ERR(ack_skb));
		result = PTR_ERR(ack_skb);
		goto error_msg_to_dev;
	}
	ack = wimax_msg_data_len(ack_skb, &ack_len);
	result = i2400m_msg_check_status(ack, strerr, sizeof(strerr));
	if (result < 0) {
		dev_err(dev, "'set idle timeout' (0x%04x) command failed: "
			"%d - %s\n", I2400M_MT_GET_STATE, result, strerr);
		goto error_cmd_failed;
	}
	result = 0;
	kfree_skb(ack_skb);
error_cmd_failed:
error_msg_to_dev:
	kfree(cmd);
error_alloc:
	return result;
}


int i2400m_dev_initialize(struct i2400m *i2400m)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct i2400m_tlv_config_idle_parameters idle_params;
	struct i2400m_tlv_config_idle_timeout idle_timeout;
	struct i2400m_tlv_config_d2h_data_format df;
	struct i2400m_tlv_config_dl_host_reorder dlhr;
	const struct i2400m_tlv_hdr *args[9];
	unsigned argc = 0;

	d_fnstart(3, dev, "(i2400m %p)\n", i2400m);
	if (i2400m_passive_mode)
		goto out_passive;
	
	if (i2400m_idle_mode_disabled) {
		if (i2400m_le_v1_3(i2400m)) {
			idle_params.hdr.type =
				cpu_to_le16(I2400M_TLV_CONFIG_IDLE_PARAMETERS);
			idle_params.hdr.length = cpu_to_le16(
				sizeof(idle_params) - sizeof(idle_params.hdr));
			idle_params.idle_timeout = 0;
			idle_params.idle_paging_interval = 0;
			args[argc++] = &idle_params.hdr;
		} else {
			idle_timeout.hdr.type =
				cpu_to_le16(I2400M_TLV_CONFIG_IDLE_TIMEOUT);
			idle_timeout.hdr.length = cpu_to_le16(
				sizeof(idle_timeout) - sizeof(idle_timeout.hdr));
			idle_timeout.timeout = 0;
			args[argc++] = &idle_timeout.hdr;
		}
	}
	if (i2400m_ge_v1_4(i2400m)) {
		
		df.hdr.type =
			cpu_to_le16(I2400M_TLV_CONFIG_D2H_DATA_FORMAT);
		df.hdr.length = cpu_to_le16(
			sizeof(df) - sizeof(df.hdr));
		df.format = 1;
		args[argc++] = &df.hdr;

		if (i2400m->rx_reorder) {
			dlhr.hdr.type =
				cpu_to_le16(I2400M_TLV_CONFIG_DL_HOST_REORDER);
			dlhr.hdr.length = cpu_to_le16(
				sizeof(dlhr) - sizeof(dlhr.hdr));
			dlhr.reorder = 1;
			args[argc++] = &dlhr.hdr;
		}
	}
	result = i2400m_set_init_config(i2400m, args, argc);
	if (result < 0)
		goto error;
out_passive:
	result = i2400m_cmd_get_state(i2400m);
error:
	if (result < 0)
		dev_err(dev, "failed to initialize the device: %d\n", result);
	d_fnend(3, dev, "(i2400m %p) = %d\n", i2400m, result);
	return result;
}


void i2400m_dev_shutdown(struct i2400m *i2400m)
{
	struct device *dev = i2400m_dev(i2400m);

	d_fnstart(3, dev, "(i2400m %p)\n", i2400m);
	d_fnend(3, dev, "(i2400m %p) = void\n", i2400m);
}
