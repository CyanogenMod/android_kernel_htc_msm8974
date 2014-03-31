/*****************************************************************************
* Copyright 2003 - 2008 Broadcom Corporation.  All rights reserved.
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



#include <csp/errno.h>
#include <csp/stdint.h>
#include <csp/module.h>

#include <mach/csp/chipcHw_def.h>
#include <mach/csp/chipcHw_inline.h>

#include <csp/reg.h>
#include <csp/delay.h>


void chipcHw_pll2Enable(uint32_t vcoFreqHz)
{
	uint32_t pllPreDivider2 = 0;

	{
		REG_LOCAL_IRQ_SAVE;
		pChipcHw->PLLConfig2 =
		    chipcHw_REG_PLL_CONFIG_D_RESET |
		    chipcHw_REG_PLL_CONFIG_A_RESET;

		pllPreDivider2 = chipcHw_REG_PLL_PREDIVIDER_POWER_DOWN |
		    chipcHw_REG_PLL_PREDIVIDER_NDIV_MODE_INTEGER |
		    (chipcHw_REG_PLL_PREDIVIDER_NDIV_i(vcoFreqHz) <<
		     chipcHw_REG_PLL_PREDIVIDER_NDIV_SHIFT) |
		    (chipcHw_REG_PLL_PREDIVIDER_P1 <<
		     chipcHw_REG_PLL_PREDIVIDER_P1_SHIFT) |
		    (chipcHw_REG_PLL_PREDIVIDER_P2 <<
		     chipcHw_REG_PLL_PREDIVIDER_P2_SHIFT);

		
		pChipcHw->PLLStatus |= chipcHw_REG_PLL_STATUS_CONTROL_ENABLE;

		
		pChipcHw->PLLPreDivider2 = pllPreDivider2;
		
		pChipcHw->PLLDivider2 = chipcHw_REG_PLL_DIVIDER_NDIV_f;

		
		pChipcHw->PLLControl12 = 0x38000700;
		pChipcHw->PLLControl22 = 0x00000015;

		
		if (vcoFreqHz > chipcHw_REG_PLL_CONFIG_VCO_SPLIT_FREQ) {
			pChipcHw->PLLConfig2 = chipcHw_REG_PLL_CONFIG_D_RESET |
			    chipcHw_REG_PLL_CONFIG_A_RESET |
			    chipcHw_REG_PLL_CONFIG_VCO_1601_3200 |
			    chipcHw_REG_PLL_CONFIG_POWER_DOWN;
		} else {
			pChipcHw->PLLConfig2 = chipcHw_REG_PLL_CONFIG_D_RESET |
			    chipcHw_REG_PLL_CONFIG_A_RESET |
			    chipcHw_REG_PLL_CONFIG_VCO_800_1600 |
			    chipcHw_REG_PLL_CONFIG_POWER_DOWN;
		}
		REG_LOCAL_IRQ_RESTORE;
	}

	
	udelay(1);

	{
		REG_LOCAL_IRQ_SAVE;
		
		pChipcHw->PLLConfig2 &=
		    ~(chipcHw_REG_PLL_CONFIG_A_RESET |
		      chipcHw_REG_PLL_CONFIG_POWER_DOWN);

		REG_LOCAL_IRQ_RESTORE;

	}

	
	while (!(pChipcHw->PLLStatus2 & chipcHw_REG_PLL_STATUS_LOCKED))
		;

	{
		REG_LOCAL_IRQ_SAVE;
		
		pChipcHw->PLLConfig2 &= ~chipcHw_REG_PLL_CONFIG_D_RESET;

		REG_LOCAL_IRQ_RESTORE;
	}
}

EXPORT_SYMBOL(chipcHw_pll2Enable);

void chipcHw_pll1Enable(uint32_t vcoFreqHz, chipcHw_SPREAD_SPECTRUM_e ssSupport)
{
	uint32_t pllPreDivider = 0;

	{
		REG_LOCAL_IRQ_SAVE;

		pChipcHw->PLLConfig =
		    chipcHw_REG_PLL_CONFIG_D_RESET |
		    chipcHw_REG_PLL_CONFIG_A_RESET;
		
		if (ssSupport == chipcHw_SPREAD_SPECTRUM_ALLOW) {
			pllPreDivider =
			    chipcHw_REG_PLL_PREDIVIDER_NDIV_MODE_MASH_1_8 |
			    ((chipcHw_REG_PLL_PREDIVIDER_NDIV_i(vcoFreqHz) -
			      1) << chipcHw_REG_PLL_PREDIVIDER_NDIV_SHIFT) |
			    (chipcHw_REG_PLL_PREDIVIDER_P1 <<
			     chipcHw_REG_PLL_PREDIVIDER_P1_SHIFT) |
			    (chipcHw_REG_PLL_PREDIVIDER_P2 <<
			     chipcHw_REG_PLL_PREDIVIDER_P2_SHIFT);
		} else {
			pllPreDivider = chipcHw_REG_PLL_PREDIVIDER_POWER_DOWN |
			    chipcHw_REG_PLL_PREDIVIDER_NDIV_MODE_INTEGER |
			    (chipcHw_REG_PLL_PREDIVIDER_NDIV_i(vcoFreqHz) <<
			     chipcHw_REG_PLL_PREDIVIDER_NDIV_SHIFT) |
			    (chipcHw_REG_PLL_PREDIVIDER_P1 <<
			     chipcHw_REG_PLL_PREDIVIDER_P1_SHIFT) |
			    (chipcHw_REG_PLL_PREDIVIDER_P2 <<
			     chipcHw_REG_PLL_PREDIVIDER_P2_SHIFT);
		}

		
		pChipcHw->PLLStatus |= chipcHw_REG_PLL_STATUS_CONTROL_ENABLE;

		
		pChipcHw->PLLPreDivider = pllPreDivider;
		
		if (ssSupport == chipcHw_SPREAD_SPECTRUM_ALLOW) {
			pChipcHw->PLLDivider = chipcHw_REG_PLL_DIVIDER_M1DIV |
			    chipcHw_REG_PLL_DIVIDER_NDIV_f_SS;
		} else {
			pChipcHw->PLLDivider = chipcHw_REG_PLL_DIVIDER_M1DIV |
			    chipcHw_REG_PLL_DIVIDER_NDIV_f;
		}

		
		if (vcoFreqHz > chipcHw_REG_PLL_CONFIG_VCO_SPLIT_FREQ) {
			pChipcHw->PLLConfig = chipcHw_REG_PLL_CONFIG_D_RESET |
			    chipcHw_REG_PLL_CONFIG_A_RESET |
			    chipcHw_REG_PLL_CONFIG_VCO_1601_3200 |
			    chipcHw_REG_PLL_CONFIG_POWER_DOWN;
		} else {
			pChipcHw->PLLConfig = chipcHw_REG_PLL_CONFIG_D_RESET |
			    chipcHw_REG_PLL_CONFIG_A_RESET |
			    chipcHw_REG_PLL_CONFIG_VCO_800_1600 |
			    chipcHw_REG_PLL_CONFIG_POWER_DOWN;
		}

		REG_LOCAL_IRQ_RESTORE;

		
		udelay(1);

		{
			REG_LOCAL_IRQ_SAVE;
			
			pChipcHw->PLLConfig &=
			    ~(chipcHw_REG_PLL_CONFIG_A_RESET |
			      chipcHw_REG_PLL_CONFIG_POWER_DOWN);
			REG_LOCAL_IRQ_RESTORE;
		}

		
		while (!(pChipcHw->PLLStatus & chipcHw_REG_PLL_STATUS_LOCKED)
		       || !(pChipcHw->
			    PLLStatus2 & chipcHw_REG_PLL_STATUS_LOCKED))
			;

		
		{
			REG_LOCAL_IRQ_SAVE;
			pChipcHw->PLLConfig &= ~chipcHw_REG_PLL_CONFIG_D_RESET;
			REG_LOCAL_IRQ_RESTORE;
		}
	}
}

EXPORT_SYMBOL(chipcHw_pll1Enable);


void chipcHw_Init(chipcHw_INIT_PARAM_t *initParam	
    ) {
#if !(defined(__KERNEL__) && !defined(STANDALONE))
	delay_init();
#endif

	
	if (!(chipcHw_getStickyBits() & chipcHw_REG_STICKY_CHIP_WARM_RESET)) {
		chipcHw_pll1Enable(initParam->pllVcoFreqHz,
				   initParam->ssSupport);
		chipcHw_pll2Enable(initParam->pll2VcoFreqHz);
	} else {
		
		chipcHw_clearStickyBits(chipcHw_REG_STICKY_CHIP_WARM_RESET);
	}
	
	chipcHw_clearStickyBits(chipcHw_REG_STICKY_CHIP_SOFT_RESET);

	
	pChipcHw->ACLKClock =
	    (pChipcHw->
	     ACLKClock & ~chipcHw_REG_ACLKClock_CLK_DIV_MASK) | (initParam->
								 armBusRatio &
								 chipcHw_REG_ACLKClock_CLK_DIV_MASK);

	
	
	
	

	chipcHw_setClockFrequency(chipcHw_CLOCK_ARM,
				  initParam->busClockFreqHz *
				  initParam->armBusRatio);
	chipcHw_setClockFrequency(chipcHw_CLOCK_BUS, initParam->busClockFreqHz);
	chipcHw_setClockFrequency(chipcHw_CLOCK_VPM,
				  initParam->busClockFreqHz *
				  initParam->vpmBusRatio);
	chipcHw_setClockFrequency(chipcHw_CLOCK_DDR,
				  initParam->busClockFreqHz *
				  initParam->ddrBusRatio);
	chipcHw_setClockFrequency(chipcHw_CLOCK_RTBUS,
				  initParam->busClockFreqHz / 2);
}
