/*
 * HID driver for Nintendo Wiimote extension devices
 * Copyright (c) 2011 David Herrmann
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include "hid-wiimote.h"

struct wiimote_ext {
	struct wiimote_data *wdata;
	struct work_struct worker;
	struct input_dev *input;
	struct input_dev *mp_input;

	atomic_t opened;
	atomic_t mp_opened;
	bool plugged;
	bool mp_plugged;
	bool motionp;
	__u8 ext_type;
};

enum wiiext_type {
	WIIEXT_NONE,		
	WIIEXT_CLASSIC,		
	WIIEXT_NUNCHUCK,	
};

enum wiiext_keys {
	WIIEXT_KEY_C,
	WIIEXT_KEY_Z,
	WIIEXT_KEY_A,
	WIIEXT_KEY_B,
	WIIEXT_KEY_X,
	WIIEXT_KEY_Y,
	WIIEXT_KEY_ZL,
	WIIEXT_KEY_ZR,
	WIIEXT_KEY_PLUS,
	WIIEXT_KEY_MINUS,
	WIIEXT_KEY_HOME,
	WIIEXT_KEY_LEFT,
	WIIEXT_KEY_RIGHT,
	WIIEXT_KEY_UP,
	WIIEXT_KEY_DOWN,
	WIIEXT_KEY_LT,
	WIIEXT_KEY_RT,
	WIIEXT_KEY_COUNT
};

static __u16 wiiext_keymap[] = {
	BTN_C,		
	BTN_Z,		
	BTN_A,		
	BTN_B,		
	BTN_X,		
	BTN_Y,		
	BTN_TL2,	
	BTN_TR2,	
	KEY_NEXT,	
	KEY_PREVIOUS,	
	BTN_MODE,	
	KEY_LEFT,	
	KEY_RIGHT,	
	KEY_UP,		
	KEY_DOWN,	
	BTN_TL,		
	BTN_TR,		
};

static void ext_disable(struct wiimote_ext *ext)
{
	unsigned long flags;
	__u8 wmem = 0x55;

	if (!wiimote_cmd_acquire(ext->wdata)) {
		wiimote_cmd_write(ext->wdata, 0xa400f0, &wmem, sizeof(wmem));
		wiimote_cmd_release(ext->wdata);
	}

	spin_lock_irqsave(&ext->wdata->state.lock, flags);
	ext->motionp = false;
	ext->ext_type = WIIEXT_NONE;
	wiiproto_req_drm(ext->wdata, WIIPROTO_REQ_NULL);
	spin_unlock_irqrestore(&ext->wdata->state.lock, flags);
}

static bool motionp_read(struct wiimote_ext *ext)
{
	__u8 rmem[2], wmem;
	ssize_t ret;
	bool avail = false;

	if (!atomic_read(&ext->mp_opened))
		return false;

	if (wiimote_cmd_acquire(ext->wdata))
		return false;

	
	wmem = 0x55;
	ret = wiimote_cmd_write(ext->wdata, 0xa600f0, &wmem, sizeof(wmem));
	if (ret)
		goto error;

	
	ret = wiimote_cmd_read(ext->wdata, 0xa600fe, rmem, 2);
	if (ret == 2 || rmem[1] == 0x5)
		avail = true;

error:
	wiimote_cmd_release(ext->wdata);
	return avail;
}

static __u8 ext_read(struct wiimote_ext *ext)
{
	ssize_t ret;
	__u8 rmem[2], wmem;
	__u8 type = WIIEXT_NONE;

	if (!ext->plugged || !atomic_read(&ext->opened))
		return WIIEXT_NONE;

	if (wiimote_cmd_acquire(ext->wdata))
		return WIIEXT_NONE;

	
	wmem = 0x55;
	ret = wiimote_cmd_write(ext->wdata, 0xa400f0, &wmem, sizeof(wmem));
	if (!ret) {
		
		wmem = 0x0;
		wiimote_cmd_write(ext->wdata, 0xa400fb, &wmem, sizeof(wmem));
	}

	
	ret = wiimote_cmd_read(ext->wdata, 0xa400fe, rmem, 2);
	if (ret == 2) {
		if (rmem[0] == 0 && rmem[1] == 0)
			type = WIIEXT_NUNCHUCK;
		else if (rmem[0] == 0x01 && rmem[1] == 0x01)
			type = WIIEXT_CLASSIC;
	}

	wiimote_cmd_release(ext->wdata);

	return type;
}

static void ext_enable(struct wiimote_ext *ext, bool motionp, __u8 ext_type)
{
	unsigned long flags;
	__u8 wmem;
	int ret;

	if (motionp) {
		if (wiimote_cmd_acquire(ext->wdata))
			return;

		if (ext_type == WIIEXT_CLASSIC)
			wmem = 0x07;
		else if (ext_type == WIIEXT_NUNCHUCK)
			wmem = 0x05;
		else
			wmem = 0x04;

		ret = wiimote_cmd_write(ext->wdata, 0xa600fe, &wmem, sizeof(wmem));
		wiimote_cmd_release(ext->wdata);
		if (ret)
			return;
	}

	spin_lock_irqsave(&ext->wdata->state.lock, flags);
	ext->motionp = motionp;
	ext->ext_type = ext_type;
	wiiproto_req_drm(ext->wdata, WIIPROTO_REQ_NULL);
	spin_unlock_irqrestore(&ext->wdata->state.lock, flags);
}

static void wiiext_worker(struct work_struct *work)
{
	struct wiimote_ext *ext = container_of(work, struct wiimote_ext,
									worker);
	bool motionp;
	__u8 ext_type;

	ext_disable(ext);
	motionp = motionp_read(ext);
	ext_type = ext_read(ext);
	ext_enable(ext, motionp, ext_type);
}

static void wiiext_schedule(struct wiimote_ext *ext)
{
	queue_work(system_nrt_wq, &ext->worker);
}

void wiiext_event(struct wiimote_data *wdata, bool plugged)
{
	if (!wdata->ext)
		return;

	if (wdata->ext->plugged == plugged)
		return;

	wdata->ext->plugged = plugged;

	if (!plugged)
		wdata->ext->mp_plugged = false;

}

bool wiiext_active(struct wiimote_data *wdata)
{
	if (!wdata->ext)
		return false;

	return wdata->ext->motionp || wdata->ext->ext_type;
}

static void handler_motionp(struct wiimote_ext *ext, const __u8 *payload)
{
	__s32 x, y, z;
	bool plugged;


	x = payload[0];
	y = payload[1];
	z = payload[2];

	x |= (((__u16)payload[3]) << 6) & 0xff00;
	y |= (((__u16)payload[4]) << 6) & 0xff00;
	z |= (((__u16)payload[5]) << 6) & 0xff00;

	x -= 8192;
	y -= 8192;
	z -= 8192;

	if (!(payload[3] & 0x02))
		x *= 18;
	else
		x *= 9;
	if (!(payload[4] & 0x02))
		y *= 18;
	else
		y *= 9;
	if (!(payload[3] & 0x01))
		z *= 18;
	else
		z *= 9;

	input_report_abs(ext->mp_input, ABS_RX, x);
	input_report_abs(ext->mp_input, ABS_RY, y);
	input_report_abs(ext->mp_input, ABS_RZ, z);
	input_sync(ext->mp_input);

	plugged = payload[5] & 0x01;
	if (plugged != ext->mp_plugged)
		ext->mp_plugged = plugged;
}

static void handler_nunchuck(struct wiimote_ext *ext, const __u8 *payload)
{
	__s16 x, y, z, bx, by;


	bx = payload[0];
	by = payload[1];
	bx -= 128;
	by -= 128;

	x = payload[2] << 2;
	y = payload[3] << 2;
	z = payload[4] << 2;

	if (ext->motionp) {
		x |= (payload[5] >> 3) & 0x02;
		y |= (payload[5] >> 4) & 0x02;
		z &= ~0x4;
		z |= (payload[5] >> 5) & 0x06;
	} else {
		x |= (payload[5] >> 2) & 0x03;
		y |= (payload[5] >> 4) & 0x03;
		z |= (payload[5] >> 6) & 0x03;
	}

	x -= 0x200;
	y -= 0x200;
	z -= 0x200;

	input_report_abs(ext->input, ABS_HAT0X, bx);
	input_report_abs(ext->input, ABS_HAT0Y, by);

	input_report_abs(ext->input, ABS_RX, x);
	input_report_abs(ext->input, ABS_RY, y);
	input_report_abs(ext->input, ABS_RZ, z);

	if (ext->motionp) {
		input_report_key(ext->input,
			wiiext_keymap[WIIEXT_KEY_Z], !!(payload[5] & 0x04));
		input_report_key(ext->input,
			wiiext_keymap[WIIEXT_KEY_C], !!(payload[5] & 0x08));
	} else {
		input_report_key(ext->input,
			wiiext_keymap[WIIEXT_KEY_Z], !!(payload[5] & 0x01));
		input_report_key(ext->input,
			wiiext_keymap[WIIEXT_KEY_C], !!(payload[5] & 0x02));
	}

	input_sync(ext->input);
}

static void handler_classic(struct wiimote_ext *ext, const __u8 *payload)
{
	__s8 rx, ry, lx, ly, lt, rt;


	if (ext->motionp) {
		lx = payload[0] & 0x3e;
		ly = payload[0] & 0x3e;
	} else {
		lx = payload[0] & 0x3f;
		ly = payload[0] & 0x3f;
	}

	rx = (payload[0] >> 3) & 0x14;
	rx |= (payload[1] >> 5) & 0x06;
	rx |= (payload[2] >> 7) & 0x01;
	ry = payload[2] & 0x1f;

	rt = payload[3] & 0x1f;
	lt = (payload[2] >> 2) & 0x18;
	lt |= (payload[3] >> 5) & 0x07;

	rx <<= 1;
	ry <<= 1;
	rt <<= 1;
	lt <<= 1;

	input_report_abs(ext->input, ABS_HAT1X, lx - 0x20);
	input_report_abs(ext->input, ABS_HAT1Y, ly - 0x20);
	input_report_abs(ext->input, ABS_HAT2X, rx - 0x20);
	input_report_abs(ext->input, ABS_HAT2Y, ry - 0x20);
	input_report_abs(ext->input, ABS_HAT3X, rt - 0x20);
	input_report_abs(ext->input, ABS_HAT3Y, lt - 0x20);

	input_report_key(ext->input, wiiext_keymap[WIIEXT_KEY_RIGHT],
							!!(payload[4] & 0x80));
	input_report_key(ext->input, wiiext_keymap[WIIEXT_KEY_DOWN],
							!!(payload[4] & 0x40));
	input_report_key(ext->input, wiiext_keymap[WIIEXT_KEY_LT],
							!!(payload[4] & 0x20));
	input_report_key(ext->input, wiiext_keymap[WIIEXT_KEY_MINUS],
							!!(payload[4] & 0x10));
	input_report_key(ext->input, wiiext_keymap[WIIEXT_KEY_HOME],
							!!(payload[4] & 0x08));
	input_report_key(ext->input, wiiext_keymap[WIIEXT_KEY_PLUS],
							!!(payload[4] & 0x04));
	input_report_key(ext->input, wiiext_keymap[WIIEXT_KEY_RT],
							!!(payload[4] & 0x02));
	input_report_key(ext->input, wiiext_keymap[WIIEXT_KEY_ZL],
							!!(payload[5] & 0x80));
	input_report_key(ext->input, wiiext_keymap[WIIEXT_KEY_B],
							!!(payload[5] & 0x40));
	input_report_key(ext->input, wiiext_keymap[WIIEXT_KEY_Y],
							!!(payload[5] & 0x20));
	input_report_key(ext->input, wiiext_keymap[WIIEXT_KEY_A],
							!!(payload[5] & 0x10));
	input_report_key(ext->input, wiiext_keymap[WIIEXT_KEY_X],
							!!(payload[5] & 0x08));
	input_report_key(ext->input, wiiext_keymap[WIIEXT_KEY_ZR],
							!!(payload[5] & 0x04));

	if (ext->motionp) {
		input_report_key(ext->input, wiiext_keymap[WIIEXT_KEY_UP],
							!!(payload[0] & 0x01));
		input_report_key(ext->input, wiiext_keymap[WIIEXT_KEY_LEFT],
							!!(payload[1] & 0x01));
	} else {
		input_report_key(ext->input, wiiext_keymap[WIIEXT_KEY_UP],
							!!(payload[5] & 0x01));
		input_report_key(ext->input, wiiext_keymap[WIIEXT_KEY_LEFT],
							!!(payload[5] & 0x02));
	}

	input_sync(ext->input);
}

void wiiext_handle(struct wiimote_data *wdata, const __u8 *payload)
{
	struct wiimote_ext *ext = wdata->ext;

	if (!ext)
		return;

	if (ext->motionp && (payload[5] & 0x02)) {
		handler_motionp(ext, payload);
	} else if (ext->ext_type == WIIEXT_NUNCHUCK) {
		handler_nunchuck(ext, payload);
	} else if (ext->ext_type == WIIEXT_CLASSIC) {
		handler_classic(ext, payload);
	}
}

static ssize_t wiiext_show(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	struct wiimote_data *wdata = dev_to_wii(dev);
	__u8 type = WIIEXT_NONE;
	bool motionp = false;
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	if (wdata->ext) {
		motionp = wdata->ext->motionp;
		type = wdata->ext->ext_type;
	}
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	if (type == WIIEXT_NUNCHUCK) {
		if (motionp)
			return sprintf(buf, "motionp+nunchuck\n");
		else
			return sprintf(buf, "nunchuck\n");
	} else if (type == WIIEXT_CLASSIC) {
		if (motionp)
			return sprintf(buf, "motionp+classic\n");
		else
			return sprintf(buf, "classic\n");
	} else {
		if (motionp)
			return sprintf(buf, "motionp\n");
		else
			return sprintf(buf, "none\n");
	}
}

static DEVICE_ATTR(extension, S_IRUGO, wiiext_show, NULL);

static int wiiext_input_open(struct input_dev *dev)
{
	struct wiimote_ext *ext = input_get_drvdata(dev);
	int ret;

	ret = hid_hw_open(ext->wdata->hdev);
	if (ret)
		return ret;

	atomic_inc(&ext->opened);
	wiiext_schedule(ext);

	return 0;
}

static void wiiext_input_close(struct input_dev *dev)
{
	struct wiimote_ext *ext = input_get_drvdata(dev);

	atomic_dec(&ext->opened);
	wiiext_schedule(ext);
	hid_hw_close(ext->wdata->hdev);
}

static int wiiext_mp_open(struct input_dev *dev)
{
	struct wiimote_ext *ext = input_get_drvdata(dev);
	int ret;

	ret = hid_hw_open(ext->wdata->hdev);
	if (ret)
		return ret;

	atomic_inc(&ext->mp_opened);
	wiiext_schedule(ext);

	return 0;
}

static void wiiext_mp_close(struct input_dev *dev)
{
	struct wiimote_ext *ext = input_get_drvdata(dev);

	atomic_dec(&ext->mp_opened);
	wiiext_schedule(ext);
	hid_hw_close(ext->wdata->hdev);
}

int wiiext_init(struct wiimote_data *wdata)
{
	struct wiimote_ext *ext;
	unsigned long flags;
	int ret, i;

	ext = kzalloc(sizeof(*ext), GFP_KERNEL);
	if (!ext)
		return -ENOMEM;

	ext->wdata = wdata;
	INIT_WORK(&ext->worker, wiiext_worker);

	ext->input = input_allocate_device();
	if (!ext->input) {
		ret = -ENOMEM;
		goto err_input;
	}

	input_set_drvdata(ext->input, ext);
	ext->input->open = wiiext_input_open;
	ext->input->close = wiiext_input_close;
	ext->input->dev.parent = &wdata->hdev->dev;
	ext->input->id.bustype = wdata->hdev->bus;
	ext->input->id.vendor = wdata->hdev->vendor;
	ext->input->id.product = wdata->hdev->product;
	ext->input->id.version = wdata->hdev->version;
	ext->input->name = WIIMOTE_NAME " Extension";

	set_bit(EV_KEY, ext->input->evbit);
	for (i = 0; i < WIIEXT_KEY_COUNT; ++i)
		set_bit(wiiext_keymap[i], ext->input->keybit);

	set_bit(EV_ABS, ext->input->evbit);
	set_bit(ABS_HAT0X, ext->input->absbit);
	set_bit(ABS_HAT0Y, ext->input->absbit);
	set_bit(ABS_HAT1X, ext->input->absbit);
	set_bit(ABS_HAT1Y, ext->input->absbit);
	set_bit(ABS_HAT2X, ext->input->absbit);
	set_bit(ABS_HAT2Y, ext->input->absbit);
	set_bit(ABS_HAT3X, ext->input->absbit);
	set_bit(ABS_HAT3Y, ext->input->absbit);
	input_set_abs_params(ext->input, ABS_HAT0X, -120, 120, 2, 4);
	input_set_abs_params(ext->input, ABS_HAT0Y, -120, 120, 2, 4);
	input_set_abs_params(ext->input, ABS_HAT1X, -30, 30, 1, 1);
	input_set_abs_params(ext->input, ABS_HAT1Y, -30, 30, 1, 1);
	input_set_abs_params(ext->input, ABS_HAT2X, -30, 30, 1, 1);
	input_set_abs_params(ext->input, ABS_HAT2Y, -30, 30, 1, 1);
	input_set_abs_params(ext->input, ABS_HAT3X, -30, 30, 1, 1);
	input_set_abs_params(ext->input, ABS_HAT3Y, -30, 30, 1, 1);
	set_bit(ABS_RX, ext->input->absbit);
	set_bit(ABS_RY, ext->input->absbit);
	set_bit(ABS_RZ, ext->input->absbit);
	input_set_abs_params(ext->input, ABS_RX, -500, 500, 2, 4);
	input_set_abs_params(ext->input, ABS_RY, -500, 500, 2, 4);
	input_set_abs_params(ext->input, ABS_RZ, -500, 500, 2, 4);

	ret = input_register_device(ext->input);
	if (ret) {
		input_free_device(ext->input);
		goto err_input;
	}

	ext->mp_input = input_allocate_device();
	if (!ext->mp_input) {
		ret = -ENOMEM;
		goto err_mp;
	}

	input_set_drvdata(ext->mp_input, ext);
	ext->mp_input->open = wiiext_mp_open;
	ext->mp_input->close = wiiext_mp_close;
	ext->mp_input->dev.parent = &wdata->hdev->dev;
	ext->mp_input->id.bustype = wdata->hdev->bus;
	ext->mp_input->id.vendor = wdata->hdev->vendor;
	ext->mp_input->id.product = wdata->hdev->product;
	ext->mp_input->id.version = wdata->hdev->version;
	ext->mp_input->name = WIIMOTE_NAME " Motion+";

	set_bit(EV_ABS, ext->mp_input->evbit);
	set_bit(ABS_RX, ext->mp_input->absbit);
	set_bit(ABS_RY, ext->mp_input->absbit);
	set_bit(ABS_RZ, ext->mp_input->absbit);
	input_set_abs_params(ext->mp_input, ABS_RX, -160000, 160000, 4, 8);
	input_set_abs_params(ext->mp_input, ABS_RY, -160000, 160000, 4, 8);
	input_set_abs_params(ext->mp_input, ABS_RZ, -160000, 160000, 4, 8);

	ret = input_register_device(ext->mp_input);
	if (ret) {
		input_free_device(ext->mp_input);
		goto err_mp;
	}

	ret = device_create_file(&wdata->hdev->dev, &dev_attr_extension);
	if (ret)
		goto err_dev;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->ext = ext;
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	return 0;

err_dev:
	input_unregister_device(ext->mp_input);
err_mp:
	input_unregister_device(ext->input);
err_input:
	kfree(ext);
	return ret;
}

void wiiext_deinit(struct wiimote_data *wdata)
{
	struct wiimote_ext *ext = wdata->ext;
	unsigned long flags;

	if (!ext)
		return;


	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->ext = NULL;
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	device_remove_file(&wdata->hdev->dev, &dev_attr_extension);
	input_unregister_device(ext->mp_input);
	input_unregister_device(ext->input);

	cancel_work_sync(&ext->worker);
	kfree(ext);
}
