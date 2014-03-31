/******************************************************************************
 *
 * Copyright(c) 2007 - 2010 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * Modifications for inclusion into the Linux staging tree are
 * Copyright(c) 2010 Larry Finger. All rights reserved.
 *
 * Contact information:
 * WLAN FAE <wlanfae@realtek.com>
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 ******************************************************************************/
#ifndef __RTL8712_LED_H
#define __RTL8712_LED_H

#include "osdep_service.h"
#include "drv_types.h"

enum LED_CTL_MODE {
	LED_CTL_POWER_ON = 1,
	LED_CTL_LINK = 2,
	LED_CTL_NO_LINK = 3,
	LED_CTL_TX = 4,
	LED_CTL_RX = 5,
	LED_CTL_SITE_SURVEY = 6,
	LED_CTL_POWER_OFF = 7,
	LED_CTL_START_TO_LINK = 8,
	LED_CTL_START_WPS = 9,
	LED_CTL_STOP_WPS = 10,
	LED_CTL_START_WPS_BOTTON = 11,
	LED_CTL_STOP_WPS_FAIL = 12,
	LED_CTL_STOP_WPS_FAIL_OVERLAP = 13,
};

#define IS_LED_WPS_BLINKING(_LED_871x)	\
	(((struct LED_871x *)_LED_871x)->CurrLedState == LED_BLINK_WPS \
	|| ((struct LED_871x *)_LED_871x)->CurrLedState == LED_BLINK_WPS_STOP \
	|| ((struct LED_871x *)_LED_871x)->bLedWPSBlinkInProgress)

#define IS_LED_BLINKING(_LED_871x)	\
		(((struct LED_871x *)_LED_871x)->bLedWPSBlinkInProgress \
		|| ((struct LED_871x *)_LED_871x)->bLedScanBlinkInProgress)

enum LED_PIN_871x {
	LED_PIN_GPIO0,
	LED_PIN_LED0,
	LED_PIN_LED1
};

enum LED_STRATEGY_871x {
	SW_LED_MODE0, 
	SW_LED_MODE1, 
	SW_LED_MODE2, 
	SW_LED_MODE3, 
	SW_LED_MODE4, 
	SW_LED_MODE5, 
	SW_LED_MODE6, 
	HW_LED, 
};

struct LED_871x {
	struct _adapter		*padapter;
	enum LED_PIN_871x	LedPin;	
	u32			CurrLedState; 
	u8			bLedOn; 
	u8			bSWLedCtrl;
	u8			bLedBlinkInProgress; 
	u8			bLedNoLinkBlinkInProgress;
	u8			bLedLinkBlinkInProgress;
	u8			bLedStartToLinkBlinkInProgress;
	u8			bLedScanBlinkInProgress;
	u8			bLedWPSBlinkInProgress;
	u32			BlinkTimes; 
	u32			BlinkingLedState; 

	struct timer_list	BlinkTimer; 
	_workitem		BlinkWorkItem; 
};

struct led_priv {
	
	struct LED_871x		SwLed0;
	struct LED_871x		SwLed1;
	enum LED_STRATEGY_871x	LedStrategy;
	u8			bRegUseLed;
	void (*LedControlHandler)(struct _adapter *padapter,
				  enum LED_CTL_MODE LedAction);
	
};

void r8712_InitSwLeds(struct _adapter *padapter);
void r8712_DeInitSwLeds(struct _adapter *padapter);
void LedControl871x(struct _adapter *padapter, enum LED_CTL_MODE LedAction);

#endif

