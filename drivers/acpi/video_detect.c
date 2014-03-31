/*
 *  Copyright (C) 2008       SuSE Linux Products GmbH
 *                           Thomas Renninger <trenn@suse.de>
 *
 *  May be copied or modified under the terms of the GNU General Public License
 *
 * video_detect.c:
 * Provides acpi_is_video_device() for early scanning of ACPI devices in scan.c
 * There a Linux specific (Spec does not provide a HID for video devices) is
 * assigned
 *
 * After PCI devices are glued with ACPI devices
 * acpi_get_pci_dev() can be called to identify ACPI graphics
 * devices for which a real graphics card is plugged in
 *
 * Now acpi_video_get_capabilities() can be called to check which
 * capabilities the graphics cards plugged in support. The check for general
 * video capabilities will be triggered by the first caller of
 * acpi_video_get_capabilities(NULL); which will happen when the first
 * backlight switching supporting driver calls:
 * acpi_video_backlight_support();
 *
 * Depending on whether ACPI graphics extensions (cmp. ACPI spec Appendix B)
 * are available, video.ko should be used to handle the device.
 *
 * Otherwise vendor specific drivers like thinkpad_acpi, asus-laptop,
 * sony_acpi,... can take care about backlight brightness.
 *
 * If CONFIG_ACPI_VIDEO is neither set as "compiled in" (y) nor as a module (m)
 * this file will not be compiled, acpi_video_get_capabilities() and
 * acpi_video_backlight_support() will always return 0 and vendor specific
 * drivers always can handle backlight.
 *
 */

#include <linux/export.h>
#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/pci.h>

#define PREFIX "ACPI: "

ACPI_MODULE_NAME("video");
#define _COMPONENT		ACPI_VIDEO_COMPONENT

static long acpi_video_support;
static bool acpi_video_caps_checked;

static acpi_status
acpi_backlight_cap_match(acpi_handle handle, u32 level, void *context,
			  void **retyurn_value)
{
	long *cap = context;
	acpi_handle h_dummy;

	if (ACPI_SUCCESS(acpi_get_handle(handle, "_BCM", &h_dummy)) &&
	    ACPI_SUCCESS(acpi_get_handle(handle, "_BCL", &h_dummy))) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Found generic backlight "
				  "support\n"));
		*cap |= ACPI_VIDEO_BACKLIGHT;
		if (ACPI_FAILURE(acpi_get_handle(handle, "_BQC", &h_dummy)))
			printk(KERN_WARNING FW_BUG PREFIX "No _BQC method, "
				"cannot determine initial brightness\n");
		
		return AE_CTRL_TERMINATE;
	}
	return 0;
}

long acpi_is_video_device(struct acpi_device *device)
{
	acpi_handle h_dummy;
	long video_caps = 0;

	if (!device)
		return 0;

	
	if (ACPI_SUCCESS(acpi_get_handle(device->handle, "_DOD", &h_dummy)) ||
	    ACPI_SUCCESS(acpi_get_handle(device->handle, "_DOS", &h_dummy)))
		video_caps |= ACPI_VIDEO_OUTPUT_SWITCHING;

	
	if (ACPI_SUCCESS(acpi_get_handle(device->handle, "_ROM", &h_dummy)))
		video_caps |= ACPI_VIDEO_ROM_AVAILABLE;

	
	if (ACPI_SUCCESS(acpi_get_handle(device->handle, "_VPO", &h_dummy)) &&
	    ACPI_SUCCESS(acpi_get_handle(device->handle, "_GPD", &h_dummy)) &&
	    ACPI_SUCCESS(acpi_get_handle(device->handle, "_SPD", &h_dummy)))
		video_caps |= ACPI_VIDEO_DEVICE_POSTING;

	
	if (video_caps)
		acpi_walk_namespace(ACPI_TYPE_DEVICE, device->handle,
				    ACPI_UINT32_MAX, acpi_backlight_cap_match, NULL,
				    &video_caps, NULL);

	return video_caps;
}
EXPORT_SYMBOL(acpi_is_video_device);

static acpi_status
find_video(acpi_handle handle, u32 lvl, void *context, void **rv)
{
	long *cap = context;
	struct pci_dev *dev;
	struct acpi_device *acpi_dev;

	const struct acpi_device_id video_ids[] = {
		{ACPI_VIDEO_HID, 0},
		{"", 0},
	};
	if (acpi_bus_get_device(handle, &acpi_dev))
		return AE_OK;

	if (!acpi_match_device_ids(acpi_dev, video_ids)) {
		dev = acpi_get_pci_dev(handle);
		if (!dev)
			return AE_OK;
		pci_dev_put(dev);
		*cap |= acpi_is_video_device(acpi_dev);
	}
	return AE_OK;
}

long acpi_video_get_capabilities(acpi_handle graphics_handle)
{
	long caps = 0;
	struct acpi_device *tmp_dev;
	acpi_status status;

	if (acpi_video_caps_checked && graphics_handle == NULL)
		return acpi_video_support;

	if (!graphics_handle) {
		
		acpi_walk_namespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
				    ACPI_UINT32_MAX, find_video, NULL,
				    &caps, NULL);
		
		acpi_video_support |= caps;
		acpi_video_caps_checked = 1;
	} else {
		status = acpi_bus_get_device(graphics_handle, &tmp_dev);
		if (ACPI_FAILURE(status)) {
			ACPI_EXCEPTION((AE_INFO, status, "Invalid device"));
			return 0;
		}
		acpi_walk_namespace(ACPI_TYPE_DEVICE, graphics_handle,
				    ACPI_UINT32_MAX, find_video, NULL,
				    &caps, NULL);
	}
	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "We have 0x%lX video support %s %s\n",
			  graphics_handle ? caps : acpi_video_support,
			  graphics_handle ? "on device " : "in general",
			  graphics_handle ? acpi_device_bid(tmp_dev) : ""));
	return caps;
}
EXPORT_SYMBOL(acpi_video_get_capabilities);

int acpi_video_backlight_support(void)
{
	if (!acpi_video_caps_checked)
		acpi_video_get_capabilities(NULL);

	
	if (acpi_video_support & ACPI_VIDEO_BACKLIGHT_FORCE_VENDOR)
		return 0;
	else if (acpi_video_support & ACPI_VIDEO_BACKLIGHT_FORCE_VIDEO)
		return 1;

	
	if (acpi_video_support & ACPI_VIDEO_BACKLIGHT_DMI_VENDOR)
		return 0;
	else if (acpi_video_support & ACPI_VIDEO_BACKLIGHT_DMI_VIDEO)
		return 1;

	
	return acpi_video_support & ACPI_VIDEO_BACKLIGHT;
}
EXPORT_SYMBOL(acpi_video_backlight_support);

static int __init acpi_backlight(char *str)
{
	if (str == NULL || *str == '\0')
		return 1;
	else {
		if (!strcmp("vendor", str))
			acpi_video_support |=
				ACPI_VIDEO_BACKLIGHT_FORCE_VENDOR;
		if (!strcmp("video", str))
			acpi_video_support |=
				ACPI_VIDEO_BACKLIGHT_FORCE_VIDEO;
	}
	return 1;
}
__setup("acpi_backlight=", acpi_backlight);
