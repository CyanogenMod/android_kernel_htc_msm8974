/*
 * Miscellaneous procedures for dealing with the PowerMac hardware.
 * Contains support for the backlight.
 *
 *   Copyright (C) 2000 Benjamin Herrenschmidt
 *   Copyright (C) 2006 Michael Hanselmann <linux-kernel@hansmi.ch>
 *
 */

#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/adb.h>
#include <linux/pmu.h>
#include <linux/atomic.h>
#include <linux/export.h>
#include <asm/prom.h>
#include <asm/backlight.h>

#define OLD_BACKLIGHT_MAX 15

static void pmac_backlight_key_worker(struct work_struct *work);
static void pmac_backlight_set_legacy_worker(struct work_struct *work);

static DECLARE_WORK(pmac_backlight_key_work, pmac_backlight_key_worker);
static DECLARE_WORK(pmac_backlight_set_legacy_work, pmac_backlight_set_legacy_worker);

static int pmac_backlight_key_queued;
static int pmac_backlight_set_legacy_queued;

static atomic_t kernel_backlight_disabled = ATOMIC_INIT(0);

DEFINE_MUTEX(pmac_backlight_mutex);

struct backlight_device *pmac_backlight;

int pmac_has_backlight_type(const char *type)
{
	struct device_node* bk_node = of_find_node_by_name(NULL, "backlight");

	if (bk_node) {
		const char *prop = of_get_property(bk_node,
				"backlight-control", NULL);
		if (prop && strncmp(prop, type, strlen(type)) == 0) {
			of_node_put(bk_node);
			return 1;
		}
		of_node_put(bk_node);
	}

	return 0;
}

int pmac_backlight_curve_lookup(struct fb_info *info, int value)
{
	int level = (FB_BACKLIGHT_LEVELS - 1);

	if (info && info->bl_dev) {
		int i, max = 0;

		
		for (i = 0; i < FB_BACKLIGHT_LEVELS; i++)
			max = max((int)info->bl_curve[i], max);

		
		for (i = 0; i < FB_BACKLIGHT_LEVELS; i++) {
			int diff = abs(info->bl_curve[i] - value);
			if (diff < max) {
				max = diff;
				level = i;
			}
		}

	}

	return level;
}

static void pmac_backlight_key_worker(struct work_struct *work)
{
	if (atomic_read(&kernel_backlight_disabled))
		return;

	mutex_lock(&pmac_backlight_mutex);
	if (pmac_backlight) {
		struct backlight_properties *props;
		int brightness;

		props = &pmac_backlight->props;

		brightness = props->brightness +
			((pmac_backlight_key_queued?-1:1) *
			 (props->max_brightness / 15));

		if (brightness < 0)
			brightness = 0;
		else if (brightness > props->max_brightness)
			brightness = props->max_brightness;

		props->brightness = brightness;
		backlight_update_status(pmac_backlight);
	}
	mutex_unlock(&pmac_backlight_mutex);
}

void pmac_backlight_key(int direction)
{
	if (atomic_read(&kernel_backlight_disabled))
		return;

	pmac_backlight_key_queued = direction;
	schedule_work(&pmac_backlight_key_work);
}

static int __pmac_backlight_set_legacy_brightness(int brightness)
{
	int error = -ENXIO;

	mutex_lock(&pmac_backlight_mutex);
	if (pmac_backlight) {
		struct backlight_properties *props;

		props = &pmac_backlight->props;
		props->brightness = brightness *
			(props->max_brightness + 1) /
			(OLD_BACKLIGHT_MAX + 1);

		if (props->brightness > props->max_brightness)
			props->brightness = props->max_brightness;
		else if (props->brightness < 0)
			props->brightness = 0;

		backlight_update_status(pmac_backlight);

		error = 0;
	}
	mutex_unlock(&pmac_backlight_mutex);

	return error;
}

static void pmac_backlight_set_legacy_worker(struct work_struct *work)
{
	if (atomic_read(&kernel_backlight_disabled))
		return;

	__pmac_backlight_set_legacy_brightness(pmac_backlight_set_legacy_queued);
}

void pmac_backlight_set_legacy_brightness_pmu(int brightness) {
	if (atomic_read(&kernel_backlight_disabled))
		return;

	pmac_backlight_set_legacy_queued = brightness;
	schedule_work(&pmac_backlight_set_legacy_work);
}

int pmac_backlight_set_legacy_brightness(int brightness)
{
	return __pmac_backlight_set_legacy_brightness(brightness);
}

int pmac_backlight_get_legacy_brightness()
{
	int result = -ENXIO;

	mutex_lock(&pmac_backlight_mutex);
	if (pmac_backlight) {
		struct backlight_properties *props;

		props = &pmac_backlight->props;

		result = props->brightness *
			(OLD_BACKLIGHT_MAX + 1) /
			(props->max_brightness + 1);
	}
	mutex_unlock(&pmac_backlight_mutex);

	return result;
}

void pmac_backlight_disable()
{
	atomic_inc(&kernel_backlight_disabled);
}

void pmac_backlight_enable()
{
	atomic_dec(&kernel_backlight_disabled);
}

EXPORT_SYMBOL_GPL(pmac_backlight);
EXPORT_SYMBOL_GPL(pmac_backlight_mutex);
EXPORT_SYMBOL_GPL(pmac_has_backlight_type);
