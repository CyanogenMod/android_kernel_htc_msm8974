/*
 * include/linux/mfd/wm831x/pdata.h -- Platform data for WM831x
 *
 * Copyright 2009 Wolfson Microelectronics PLC.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __MFD_WM831X_PDATA_H__
#define __MFD_WM831X_PDATA_H__

struct wm831x;
struct regulator_init_data;

struct wm831x_backlight_pdata {
	int isink;     
	int max_uA;    
};

struct wm831x_backup_pdata {
	int charger_enable;
	int no_constant_voltage;  
	int vlim;   
	int ilim;   
};

struct wm831x_battery_pdata {
	int enable;         
	int fast_enable;    
	int off_mask;       
	int trickle_ilim;   
	int vsel;           
	int eoc_iterm;      
	int fast_ilim;      
	int timeout;        
};

struct wm831x_buckv_pdata {
	int dvs_gpio;        
	int dvs_control_src; 
	int dvs_init_state;  
	int dvs_state_gpio;  
};

enum wm831x_status_src {
	WM831X_STATUS_PRESERVE = 0,  
	WM831X_STATUS_OTP = 1,
	WM831X_STATUS_POWER = 2,
	WM831X_STATUS_CHARGER = 3,
	WM831X_STATUS_MANUAL = 4,
};

struct wm831x_status_pdata {
	enum wm831x_status_src default_src;
	const char *name;
	const char *default_trigger;
};

struct wm831x_touch_pdata {
	int fivewire;          
	int isel;              
	int rpu;               
	int pressure;          
	unsigned int data_irq; 
	int data_irqf;         
	unsigned int pd_irq;   
	int pd_irqf;           
};

enum wm831x_watchdog_action {
	WM831X_WDOG_NONE = 0,
	WM831X_WDOG_INTERRUPT = 1,
	WM831X_WDOG_RESET = 2,
	WM831X_WDOG_WAKE = 3,
};

struct wm831x_watchdog_pdata {
	enum wm831x_watchdog_action primary, secondary;
	int update_gpio;
	unsigned int software:1;
};

#define WM831X_MAX_STATUS 2
#define WM831X_MAX_DCDC   4
#define WM831X_MAX_EPE    2
#define WM831X_MAX_LDO    11
#define WM831X_MAX_ISINK  2

#define WM831X_GPIO_CONFIGURE 0x10000
#define WM831X_GPIO_NUM 16

struct wm831x_pdata {
	
	int wm831x_num;

	
	int (*pre_init)(struct wm831x *wm831x);
	
	int (*post_init)(struct wm831x *wm831x);

	
	bool irq_cmos;

	
	bool disable_touch;

	
	bool soft_shutdown;

	int irq_base;
	int gpio_base;
	int gpio_defaults[WM831X_GPIO_NUM];
	struct wm831x_backlight_pdata *backlight;
	struct wm831x_backup_pdata *backup;
	struct wm831x_battery_pdata *battery;
	struct wm831x_touch_pdata *touch;
	struct wm831x_watchdog_pdata *watchdog;

	
	struct wm831x_status_pdata *status[WM831X_MAX_STATUS];
	
	struct regulator_init_data *dcdc[WM831X_MAX_DCDC];
	
	struct regulator_init_data *epe[WM831X_MAX_EPE];
	
	struct regulator_init_data *ldo[WM831X_MAX_LDO];
	
	struct regulator_init_data *isink[WM831X_MAX_ISINK];
};

#endif
