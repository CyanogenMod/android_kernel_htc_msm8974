#ifndef __OMAP_NOKIA_DSI_PANEL_H
#define __OMAP_NOKIA_DSI_PANEL_H

struct omap_dss_device;

struct nokia_dsi_panel_data {
	const char *name;

	int reset_gpio;

	bool use_ext_te;
	int ext_te_gpio;

	unsigned esd_interval;
	unsigned ulps_timeout;

	bool use_dsi_backlight;
};

#endif 
