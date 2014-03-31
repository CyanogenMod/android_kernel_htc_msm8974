/*
 * wm9712.c  --  Codec driver for Wolfson WM9712 AC97 Codecs.
 *
 * Copyright 2003, 2004, 2005, 2006, 2007 Wolfson Microelectronics PLC.
 * Author: Liam Girdwood <lrg@slimlogic.co.uk>
 * Parts Copyright : Ian Molton <spyro@f2s.com>
 *                   Andrew Zabolotny <zap@homelink.ru>
 *                   Russell King <rmk@arm.linux.org.uk>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/wm97xx.h>

#define TS_NAME			"wm97xx"
#define WM9712_VERSION		"1.00"
#define DEFAULT_PRESSURE	0xb0c0


static int rpu = 8;
module_param(rpu, int, 0);
MODULE_PARM_DESC(rpu, "Set internal pull up resitor for pen detect.");

static int pil;
module_param(pil, int, 0);
MODULE_PARM_DESC(pil, "Set current used for pressure measurement.");

static int pressure = DEFAULT_PRESSURE & 0xfff;
module_param(pressure, int, 0);
MODULE_PARM_DESC(pressure, "Set threshold for pressure measurement.");

static int delay = 3;
module_param(delay, int, 0);
MODULE_PARM_DESC(delay, "Set adc sample delay.");

static int five_wire;
module_param(five_wire, int, 0);
MODULE_PARM_DESC(five_wire, "Set to '1' to use 5-wire touchscreen.");

static int mask;
module_param(mask, int, 0);
MODULE_PARM_DESC(mask, "Set adc mask function.");

static int coord;
module_param(coord, int, 0);
MODULE_PARM_DESC(coord, "Polling coordinate mode");

static const int delay_table[] = {
	21,    
	42,    
	84,    
	167,   
	333,   
	667,   
	1000,  
	1333,  
	2000,  
	2667,  
	3333,  
	4000,  
	4667,  
	5333,  
	6000,  
	0      
};

static inline void poll_delay(int d)
{
	udelay(3 * AC97_LINK_FRAME + delay_table[d]);
}

static void wm9712_phy_init(struct wm97xx *wm)
{
	u16 dig1 = 0;
	u16 dig2 = WM97XX_RPR | WM9712_RPU(1);

	
	if (rpu) {
		dig2 &= 0xffc0;
		dig2 |= WM9712_RPU(rpu);
		dev_dbg(wm->dev, "setting pen detect pull-up to %d Ohms",
			64000 / rpu);
	}

	
	if (five_wire) {
		dig2 |= WM9712_45W;
		dev_dbg(wm->dev, "setting 5-wire touchscreen mode.");

		if (pil) {
			dev_warn(wm->dev, "pressure measurement is not "
				 "supported in 5-wire mode\n");
			pil = 0;
		}
	}

	
	if (pil == 2) {
		dig2 |= WM9712_PIL;
		dev_dbg(wm->dev,
			"setting pressure measurement current to 400uA.");
	} else if (pil)
		dev_dbg(wm->dev,
			"setting pressure measurement current to 200uA.");
	if (!pil)
		pressure = 0;

	
	if (delay < 0 || delay > 15) {
		dev_dbg(wm->dev, "supplied delay out of range.");
		delay = 4;
	}
	dig1 &= 0xff0f;
	dig1 |= WM97XX_DELAY(delay);
	dev_dbg(wm->dev, "setting adc sample delay to %d u Secs.",
		delay_table[delay]);

	
	dig2 |= ((mask & 0x3) << 6);
	if (mask) {
		u16 reg;
		
		reg = wm97xx_reg_read(wm, AC97_MISC_AFE);
		wm97xx_reg_write(wm, AC97_MISC_AFE, reg | WM97XX_GPIO_4);
		reg = wm97xx_reg_read(wm, AC97_GPIO_CFG);
		wm97xx_reg_write(wm, AC97_GPIO_CFG, reg | WM97XX_GPIO_4);
	}

	
	if (coord)
		dig2 |= WM9712_WAIT;

	wm97xx_reg_write(wm, AC97_WM97XX_DIGITISER1, dig1);
	wm97xx_reg_write(wm, AC97_WM97XX_DIGITISER2, dig2);
}

static void wm9712_dig_enable(struct wm97xx *wm, int enable)
{
	u16 dig2 = wm->dig[2];

	if (enable) {
		wm97xx_reg_write(wm, AC97_WM97XX_DIGITISER2,
				 dig2 | WM97XX_PRP_DET_DIG);
		wm97xx_reg_read(wm, AC97_WM97XX_DIGITISER_RD); 
	} else
		wm97xx_reg_write(wm, AC97_WM97XX_DIGITISER2,
				 dig2 & ~WM97XX_PRP_DET_DIG);
}

static void wm9712_aux_prepare(struct wm97xx *wm)
{
	memcpy(wm->dig_save, wm->dig, sizeof(wm->dig));
	wm97xx_reg_write(wm, AC97_WM97XX_DIGITISER1, 0);
	wm97xx_reg_write(wm, AC97_WM97XX_DIGITISER2, WM97XX_PRP_DET_DIG);
}

static void wm9712_dig_restore(struct wm97xx *wm)
{
	wm97xx_reg_write(wm, AC97_WM97XX_DIGITISER1, wm->dig_save[1]);
	wm97xx_reg_write(wm, AC97_WM97XX_DIGITISER2, wm->dig_save[2]);
}

static inline int is_pden(struct wm97xx *wm)
{
	return wm->dig[2] & WM9712_PDEN;
}

static int wm9712_poll_sample(struct wm97xx *wm, int adcsel, int *sample)
{
	int timeout = 5 * delay;
	bool wants_pen = adcsel & WM97XX_PEN_DOWN;

	if (wants_pen && !wm->pen_probably_down) {
		u16 data = wm97xx_reg_read(wm, AC97_WM97XX_DIGITISER_RD);
		if (!(data & WM97XX_PEN_DOWN))
			return RC_PENUP;
		wm->pen_probably_down = 1;
	}

	
	if (wm->mach_ops && wm->mach_ops->pre_sample)
		wm->mach_ops->pre_sample(adcsel);
	wm97xx_reg_write(wm, AC97_WM97XX_DIGITISER1, (adcsel & WM97XX_ADCSEL_MASK)
				| WM97XX_POLL | WM97XX_DELAY(delay));

	
	poll_delay(delay);

	
	while ((wm97xx_reg_read(wm, AC97_WM97XX_DIGITISER1) & WM97XX_POLL)
	       && timeout) {
		udelay(AC97_LINK_FRAME);
		timeout--;
	}

	if (timeout <= 0) {
		
		if (is_pden(wm))
			wm->pen_probably_down = 0;
		else
			dev_dbg(wm->dev, "adc sample timeout");
		return RC_PENUP;
	}

	*sample = wm97xx_reg_read(wm, AC97_WM97XX_DIGITISER_RD);
	if (wm->mach_ops && wm->mach_ops->post_sample)
		wm->mach_ops->post_sample(adcsel);

	
	if ((*sample ^ adcsel) & WM97XX_ADCSEL_MASK) {
		dev_dbg(wm->dev, "adc wrong sample, wanted %x got %x",
			adcsel & WM97XX_ADCSEL_MASK,
			*sample & WM97XX_ADCSEL_MASK);
		return RC_PENUP;
	}

	if (wants_pen && !(*sample & WM97XX_PEN_DOWN)) {
		wm->pen_probably_down = 0;
		return RC_PENUP;
	}

	return RC_VALID;
}

static int wm9712_poll_coord(struct wm97xx *wm, struct wm97xx_data *data)
{
	int timeout = 5 * delay;

	if (!wm->pen_probably_down) {
		u16 data_rd = wm97xx_reg_read(wm, AC97_WM97XX_DIGITISER_RD);
		if (!(data_rd & WM97XX_PEN_DOWN))
			return RC_PENUP;
		wm->pen_probably_down = 1;
	}

	
	if (wm->mach_ops && wm->mach_ops->pre_sample)
		wm->mach_ops->pre_sample(WM97XX_ADCSEL_X | WM97XX_ADCSEL_Y);

	wm97xx_reg_write(wm, AC97_WM97XX_DIGITISER1,
		WM97XX_COO | WM97XX_POLL | WM97XX_DELAY(delay));

	
	poll_delay(delay);
	data->x = wm97xx_reg_read(wm, AC97_WM97XX_DIGITISER_RD);
	
	while ((wm97xx_reg_read(wm, AC97_WM97XX_DIGITISER1) & WM97XX_POLL)
	       && timeout) {
		udelay(AC97_LINK_FRAME);
		timeout--;
	}

	if (timeout <= 0) {
		
		if (is_pden(wm))
			wm->pen_probably_down = 0;
		else
			dev_dbg(wm->dev, "adc sample timeout");
		return RC_PENUP;
	}

	
	data->y = wm97xx_reg_read(wm, AC97_WM97XX_DIGITISER_RD);
	if (pil)
		data->p = wm97xx_reg_read(wm, AC97_WM97XX_DIGITISER_RD);
	else
		data->p = DEFAULT_PRESSURE;

	if (wm->mach_ops && wm->mach_ops->post_sample)
		wm->mach_ops->post_sample(WM97XX_ADCSEL_X | WM97XX_ADCSEL_Y);

	
	if (!(data->x & WM97XX_ADCSEL_X) || !(data->y & WM97XX_ADCSEL_Y))
		goto err;
	if (pil && !(data->p & WM97XX_ADCSEL_PRES))
		goto err;

	if (!(data->x & WM97XX_PEN_DOWN) || !(data->y & WM97XX_PEN_DOWN)) {
		wm->pen_probably_down = 0;
		return RC_PENUP;
	}
	return RC_VALID;
err:
	return 0;
}

static int wm9712_poll_touch(struct wm97xx *wm, struct wm97xx_data *data)
{
	int rc;

	if (coord) {
		rc = wm9712_poll_coord(wm, data);
		if (rc != RC_VALID)
			return rc;
	} else {
		rc = wm9712_poll_sample(wm, WM97XX_ADCSEL_X | WM97XX_PEN_DOWN,
					&data->x);
		if (rc != RC_VALID)
			return rc;

		rc = wm9712_poll_sample(wm, WM97XX_ADCSEL_Y | WM97XX_PEN_DOWN,
					&data->y);
		if (rc != RC_VALID)
			return rc;

		if (pil && !five_wire) {
			rc = wm9712_poll_sample(wm, WM97XX_ADCSEL_PRES | WM97XX_PEN_DOWN,
						&data->p);
			if (rc != RC_VALID)
				return rc;
		} else
			data->p = DEFAULT_PRESSURE;
	}
	return RC_VALID;
}

static int wm9712_acc_enable(struct wm97xx *wm, int enable)
{
	u16 dig1, dig2;
	int ret = 0;

	dig1 = wm->dig[1];
	dig2 = wm->dig[2];

	if (enable) {
		
		if (wm->mach_ops->acc_startup) {
			ret = wm->mach_ops->acc_startup(wm);
			if (ret < 0)
				return ret;
		}
		dig1 &= ~(WM97XX_CM_RATE_MASK | WM97XX_ADCSEL_MASK |
			WM97XX_DELAY_MASK | WM97XX_SLT_MASK);
		dig1 |= WM97XX_CTC | WM97XX_COO | WM97XX_SLEN |
			WM97XX_DELAY(delay) |
			WM97XX_SLT(wm->acc_slot) |
			WM97XX_RATE(wm->acc_rate);
		if (pil)
			dig1 |= WM97XX_ADCSEL_PRES;
		dig2 |= WM9712_PDEN;
	} else {
		dig1 &= ~(WM97XX_CTC | WM97XX_COO | WM97XX_SLEN);
		dig2 &= ~WM9712_PDEN;
		if (wm->mach_ops->acc_shutdown)
			wm->mach_ops->acc_shutdown(wm);
	}

	wm97xx_reg_write(wm, AC97_WM97XX_DIGITISER1, dig1);
	wm97xx_reg_write(wm, AC97_WM97XX_DIGITISER2, dig2);

	return 0;
}

struct wm97xx_codec_drv wm9712_codec = {
	.id = WM9712_ID2,
	.name = "wm9712",
	.poll_sample = wm9712_poll_sample,
	.poll_touch = wm9712_poll_touch,
	.acc_enable = wm9712_acc_enable,
	.phy_init = wm9712_phy_init,
	.dig_enable = wm9712_dig_enable,
	.dig_restore = wm9712_dig_restore,
	.aux_prepare = wm9712_aux_prepare,
};
EXPORT_SYMBOL_GPL(wm9712_codec);

MODULE_AUTHOR("Liam Girdwood <lrg@slimlogic.co.uk>");
MODULE_DESCRIPTION("WM9712 Touch Screen Driver");
MODULE_LICENSE("GPL");
