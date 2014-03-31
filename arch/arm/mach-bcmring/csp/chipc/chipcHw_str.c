/*****************************************************************************
* Copyright 2008 Broadcom Corporation.  All rights reserved.
*
* Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2, available at
* http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a
* license other than the GPL, without Broadcom's express prior written
* consent.
*****************************************************************************/


#include <mach/csp/chipcHw_inline.h>


static const char *gMuxStr[] = {
	"GPIO",			
	"KeyPad",		
	"I2C-Host",		
	"SPI",			
	"Uart",			
	"LED-Mtx-P",		
	"LED-Mtx-S",		
	"SDIO-0",		
	"SDIO-1",		
	"PCM",			
	"I2S",			
	"ETM",			
	"Debug",		
	"Misc",			
	"0xE",			
	"0xF",			
};


const char *chipcHw_getGpioPinFunctionStr(int pin)
{
	if ((pin < 0) || (pin >= chipcHw_GPIO_COUNT)) {
		return "";
	}

	return gMuxStr[chipcHw_getGpioPinFunction(pin)];
}
