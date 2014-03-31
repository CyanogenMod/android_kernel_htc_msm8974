/*****************************************************************************
* Copyright 2001 - 2009 Broadcom Corporation.  All rights reserved.
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
#include <mach/csp/chipcHw_def.h>

#define CLK_TYPE_PRIMARY         1	
#define CLK_TYPE_PLL1            2	
#define CLK_TYPE_PLL2            4	
#define CLK_TYPE_PROGRAMMABLE    8	
#define CLK_TYPE_BYPASSABLE      16	

#define CLK_MODE_XTAL            1	

struct clk {
	const char *name;	
	unsigned int type;	
	unsigned int mode;	
	volatile int use_bypass;	
	chipcHw_CLOCK_e csp_id;	
	unsigned long rate_hz;	
	unsigned int use_cnt;	
	struct clk *parent;	
};
