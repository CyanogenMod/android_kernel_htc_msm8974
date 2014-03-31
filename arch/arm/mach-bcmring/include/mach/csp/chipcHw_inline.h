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

#ifndef CHIPC_INLINE_H
#define CHIPC_INLINE_H


#include <csp/errno.h>
#include <csp/reg.h>
#include <mach/csp/chipcHw_reg.h>
#include <mach/csp/chipcHw_def.h>

typedef enum {
	chipcHw_OPTYPE_BYPASS,	
	chipcHw_OPTYPE_OUTPUT	
} chipcHw_OPTYPE_e;

static inline void chipcHw_setClock(chipcHw_CLOCK_e clock,
				    chipcHw_OPTYPE_e type, int mode);

static inline uint32_t chipcHw_getChipId(void)
{
	return pChipcHw->ChipId;
}

static inline void chipcHw_enableSpreadSpectrum(void)
{
	if ((pChipcHw->
	     PLLPreDivider & chipcHw_REG_PLL_PREDIVIDER_NDIV_MODE_MASK) !=
	    chipcHw_REG_PLL_PREDIVIDER_NDIV_MODE_INTEGER) {
		ddrcReg_PHY_ADDR_CTL_REGP->ssCfg =
		    (0xFFFF << ddrcReg_PHY_ADDR_SS_CFG_NDIV_AMPLITUDE_SHIFT) |
		    (ddrcReg_PHY_ADDR_SS_CFG_MIN_CYCLE_PER_TICK <<
		     ddrcReg_PHY_ADDR_SS_CFG_CYCLE_PER_TICK_SHIFT);
		ddrcReg_PHY_ADDR_CTL_REGP->ssCtl |=
		    ddrcReg_PHY_ADDR_SS_CTRL_ENABLE;
	}
}

static inline void chipcHw_disableSpreadSpectrum(void)
{
	ddrcReg_PHY_ADDR_CTL_REGP->ssCtl &= ~ddrcReg_PHY_ADDR_SS_CTRL_ENABLE;
}

static inline uint32_t chipcHw_getChipProductId(void)
{
	return (pChipcHw->
		 ChipId & chipcHw_REG_CHIPID_BASE_MASK) >>
		chipcHw_REG_CHIPID_BASE_SHIFT;
}

static inline chipcHw_REV_NUMBER_e chipcHw_getChipRevisionNumber(void)
{
	return pChipcHw->ChipId & chipcHw_REG_CHIPID_REV_MASK;
}

static inline void chipcHw_busInterfaceClockEnable(uint32_t mask)
{
	reg32_modify_or(&pChipcHw->BusIntfClock, mask);
}

static inline void chipcHw_busInterfaceClockDisable(uint32_t mask)
{
	reg32_modify_and(&pChipcHw->BusIntfClock, ~mask);
}

static inline uint32_t chipcHw_getBusInterfaceClockStatus(void)
{
	return pChipcHw->BusIntfClock;
}

static inline void chipcHw_audioChannelEnable(uint32_t mask)
{
	reg32_modify_or(&pChipcHw->AudioEnable, mask);
}

static inline void chipcHw_audioChannelDisable(uint32_t mask)
{
	reg32_modify_and(&pChipcHw->AudioEnable, ~mask);
}

static inline void chipcHw_softReset(uint64_t mask)
{
	chipcHw_softResetEnable(mask);
	chipcHw_softResetDisable(mask);
}

static inline void chipcHw_softResetDisable(uint64_t mask)
{
	uint32_t ctrl1 = (uint32_t) mask;
	uint32_t ctrl2 = (uint32_t) (mask >> 32);

	
	REG_LOCAL_IRQ_SAVE;
	pChipcHw->SoftReset1 ^= ctrl1;
	pChipcHw->SoftReset2 ^= (ctrl2 & (~chipcHw_REG_SOFT_RESET_UNHOLD_MASK));
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_softResetEnable(uint64_t mask)
{
	uint32_t ctrl1 = (uint32_t) mask;
	uint32_t ctrl2 = (uint32_t) (mask >> 32);
	uint32_t unhold = 0;

	REG_LOCAL_IRQ_SAVE;
	pChipcHw->SoftReset1 |= ctrl1;
	
	pChipcHw->SoftReset2 |= (ctrl2 & (~chipcHw_REG_SOFT_RESET_UNHOLD_MASK));

	
	if (ctrl2 & chipcHw_REG_SOFT_RESET_VPM_GLOBAL_UNHOLD) {
		unhold = chipcHw_REG_SOFT_RESET_VPM_GLOBAL_HOLD;
	}

	if (ctrl2 & chipcHw_REG_SOFT_RESET_VPM_UNHOLD) {
		unhold |= chipcHw_REG_SOFT_RESET_VPM_HOLD;
	}

	if (ctrl2 & chipcHw_REG_SOFT_RESET_ARM_UNHOLD) {
		unhold |= chipcHw_REG_SOFT_RESET_ARM_HOLD;
	}

	if (unhold) {
		
		pChipcHw->SoftReset1 &= ~unhold;
	}
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_miscControl(uint32_t mask)
{
	reg32_write(&pChipcHw->MiscCtrl, mask);
}

static inline void chipcHw_miscControlDisable(uint32_t mask)
{
	reg32_modify_and(&pChipcHw->MiscCtrl, ~mask);
}

static inline void chipcHw_miscControlEnable(uint32_t mask)
{
	reg32_modify_or(&pChipcHw->MiscCtrl, mask);
}

static inline void chipcHw_setOTPOption(uint64_t mask)
{
	uint32_t ctrl1 = (uint32_t) mask;
	uint32_t ctrl2 = (uint32_t) (mask >> 32);

	reg32_modify_or(&pChipcHw->SoftOTP1, ctrl1);
	reg32_modify_or(&pChipcHw->SoftOTP2, ctrl2);
}

static inline uint32_t chipcHw_getStickyBits(void)
{
	return pChipcHw->Sticky;
}

static inline void chipcHw_setStickyBits(uint32_t mask)
{
	uint32_t bits = 0;

	REG_LOCAL_IRQ_SAVE;
	if (mask & chipcHw_REG_STICKY_POR_BROM) {
		bits |= chipcHw_REG_STICKY_POR_BROM;
	} else {
		uint32_t sticky;
		sticky = pChipcHw->Sticky;

		if ((mask & chipcHw_REG_STICKY_BOOT_DONE)
		    && (sticky & chipcHw_REG_STICKY_BOOT_DONE) == 0) {
			bits |= chipcHw_REG_STICKY_BOOT_DONE;
		}
		if ((mask & chipcHw_REG_STICKY_GENERAL_1)
		    && (sticky & chipcHw_REG_STICKY_GENERAL_1) == 0) {
			bits |= chipcHw_REG_STICKY_GENERAL_1;
		}
		if ((mask & chipcHw_REG_STICKY_GENERAL_2)
		    && (sticky & chipcHw_REG_STICKY_GENERAL_2) == 0) {
			bits |= chipcHw_REG_STICKY_GENERAL_2;
		}
		if ((mask & chipcHw_REG_STICKY_GENERAL_3)
		    && (sticky & chipcHw_REG_STICKY_GENERAL_3) == 0) {
			bits |= chipcHw_REG_STICKY_GENERAL_3;
		}
		if ((mask & chipcHw_REG_STICKY_GENERAL_4)
		    && (sticky & chipcHw_REG_STICKY_GENERAL_4) == 0) {
			bits |= chipcHw_REG_STICKY_GENERAL_4;
		}
		if ((mask & chipcHw_REG_STICKY_GENERAL_5)
		    && (sticky & chipcHw_REG_STICKY_GENERAL_5) == 0) {
			bits |= chipcHw_REG_STICKY_GENERAL_5;
		}
	}
	pChipcHw->Sticky = bits;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_clearStickyBits(uint32_t mask)
{
	uint32_t bits = 0;

	REG_LOCAL_IRQ_SAVE;
	if (mask &
	    (chipcHw_REG_STICKY_BOOT_DONE | chipcHw_REG_STICKY_GENERAL_1 |
	     chipcHw_REG_STICKY_GENERAL_2 | chipcHw_REG_STICKY_GENERAL_3 |
	     chipcHw_REG_STICKY_GENERAL_4 | chipcHw_REG_STICKY_GENERAL_5)) {
		uint32_t sticky = pChipcHw->Sticky;

		if ((mask & chipcHw_REG_STICKY_BOOT_DONE)
		    && (sticky & chipcHw_REG_STICKY_BOOT_DONE)) {
			bits = chipcHw_REG_STICKY_BOOT_DONE;
			mask &= ~chipcHw_REG_STICKY_BOOT_DONE;
		}
		if ((mask & chipcHw_REG_STICKY_GENERAL_1)
		    && (sticky & chipcHw_REG_STICKY_GENERAL_1)) {
			bits |= chipcHw_REG_STICKY_GENERAL_1;
			mask &= ~chipcHw_REG_STICKY_GENERAL_1;
		}
		if ((mask & chipcHw_REG_STICKY_GENERAL_2)
		    && (sticky & chipcHw_REG_STICKY_GENERAL_2)) {
			bits |= chipcHw_REG_STICKY_GENERAL_2;
			mask &= ~chipcHw_REG_STICKY_GENERAL_2;
		}
		if ((mask & chipcHw_REG_STICKY_GENERAL_3)
		    && (sticky & chipcHw_REG_STICKY_GENERAL_3)) {
			bits |= chipcHw_REG_STICKY_GENERAL_3;
			mask &= ~chipcHw_REG_STICKY_GENERAL_3;
		}
		if ((mask & chipcHw_REG_STICKY_GENERAL_4)
		    && (sticky & chipcHw_REG_STICKY_GENERAL_4)) {
			bits |= chipcHw_REG_STICKY_GENERAL_4;
			mask &= ~chipcHw_REG_STICKY_GENERAL_4;
		}
		if ((mask & chipcHw_REG_STICKY_GENERAL_5)
		    && (sticky & chipcHw_REG_STICKY_GENERAL_5)) {
			bits |= chipcHw_REG_STICKY_GENERAL_5;
			mask &= ~chipcHw_REG_STICKY_GENERAL_5;
		}
	}
	pChipcHw->Sticky = bits | mask;
	REG_LOCAL_IRQ_RESTORE;
}

static inline uint32_t chipcHw_getSoftStraps(void)
{
	return pChipcHw->SoftStraps;
}

static inline void chipcHw_setSoftStraps(uint32_t strapOptions)
{
	reg32_write(&pChipcHw->SoftStraps, strapOptions);
}

static inline uint32_t chipcHw_getPinStraps(void)
{
	return pChipcHw->PinStraps;
}

static inline uint32_t chipcHw_getValidStraps(void)
{
	uint32_t softStraps;

	softStraps = chipcHw_getSoftStraps();

	return softStraps;
}

static inline void chipcHw_initValidStraps(void)
{
	uint32_t softStraps;

	REG_LOCAL_IRQ_SAVE;
	softStraps = chipcHw_getSoftStraps();

	if ((softStraps & chipcHw_STRAPS_SOFT_OVERRIDE) == 0) {
		
		chipcHw_setSoftStraps(chipcHw_getPinStraps());
	}
	REG_LOCAL_IRQ_RESTORE;
}

static inline chipcHw_BOOT_DEVICE_e chipcHw_getBootDevice(void)
{
	return chipcHw_getValidStraps() & chipcHw_STRAPS_BOOT_DEVICE_MASK;
}

static inline chipcHw_BOOT_MODE_e chipcHw_getBootMode(void)
{
	return chipcHw_getValidStraps() & chipcHw_STRAPS_BOOT_MODE_MASK;
}

static inline chipcHw_NAND_PAGESIZE_e chipcHw_getNandPageSize(void)
{
	return chipcHw_getValidStraps() & chipcHw_STRAPS_NAND_PAGESIZE_MASK;
}

static inline int chipcHw_getNandExtraCycle(void)
{
	if (chipcHw_getValidStraps() & chipcHw_STRAPS_NAND_EXTRA_CYCLE) {
		return 1;
	} else {
		return 0;
	}
}

static inline void chipcHw_activatePifInterface(void)
{
	reg32_write(&pChipcHw->LcdPifMode, chipcHw_REG_PIF_PIN_ENABLE);
}

static inline void chipcHw_activateLcdInterface(void)
{
	reg32_write(&pChipcHw->LcdPifMode, chipcHw_REG_LCD_PIN_ENABLE);
}

static inline void chipcHw_deactivatePifLcdInterface(void)
{
	reg32_write(&pChipcHw->LcdPifMode, 0);
}

static inline void chipcHw_selectGE2(void)
{
	reg32_modify_and(&pChipcHw->MiscCtrl, ~chipcHw_REG_MISC_CTRL_GE_SEL);
}

static inline void chipcHw_selectGE3(void)
{
	reg32_modify_or(&pChipcHw->MiscCtrl, chipcHw_REG_MISC_CTRL_GE_SEL);
}

static inline chipcHw_GPIO_FUNCTION_e chipcHw_getGpioPinFunction(int pin)
{
	return (*((uint32_t *) chipcHw_REG_GPIO_MUX(pin)) &
		(chipcHw_REG_GPIO_MUX_MASK <<
		 chipcHw_REG_GPIO_MUX_POSITION(pin))) >>
	    chipcHw_REG_GPIO_MUX_POSITION(pin);
}

static inline void chipcHw_setGpioPinFunction(int pin,
					      chipcHw_GPIO_FUNCTION_e func)
{
	REG_LOCAL_IRQ_SAVE;
	*((uint32_t *) chipcHw_REG_GPIO_MUX(pin)) &=
	    ~(chipcHw_REG_GPIO_MUX_MASK << chipcHw_REG_GPIO_MUX_POSITION(pin));
	*((uint32_t *) chipcHw_REG_GPIO_MUX(pin)) |=
	    func << chipcHw_REG_GPIO_MUX_POSITION(pin);
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_setPinSlewRate(uint32_t pin,
					  chipcHw_PIN_SLEW_RATE_e slewRate)
{
	REG_LOCAL_IRQ_SAVE;
	*((uint32_t *) chipcHw_REG_SLEW_RATE(pin)) &=
	    ~(chipcHw_REG_SLEW_RATE_MASK <<
	      chipcHw_REG_SLEW_RATE_POSITION(pin));
	*((uint32_t *) chipcHw_REG_SLEW_RATE(pin)) |=
	    (uint32_t) slewRate << chipcHw_REG_SLEW_RATE_POSITION(pin);
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_setPinOutputCurrent(uint32_t pin,
					       chipcHw_PIN_CURRENT_STRENGTH_e
					       curr)
{
	REG_LOCAL_IRQ_SAVE;
	*((uint32_t *) chipcHw_REG_CURRENT(pin)) &=
	    ~(chipcHw_REG_CURRENT_MASK << chipcHw_REG_CURRENT_POSITION(pin));
	*((uint32_t *) chipcHw_REG_CURRENT(pin)) |=
	    (uint32_t) curr << chipcHw_REG_CURRENT_POSITION(pin);
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_setPinPullup(uint32_t pin, chipcHw_PIN_PULL_e pullup)
{
	REG_LOCAL_IRQ_SAVE;
	*((uint32_t *) chipcHw_REG_PULLUP(pin)) &=
	    ~(chipcHw_REG_PULLUP_MASK << chipcHw_REG_PULLUP_POSITION(pin));
	*((uint32_t *) chipcHw_REG_PULLUP(pin)) |=
	    (uint32_t) pullup << chipcHw_REG_PULLUP_POSITION(pin);
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_setPinInputType(uint32_t pin,
					   chipcHw_PIN_INPUTTYPE_e inputType)
{
	REG_LOCAL_IRQ_SAVE;
	*((uint32_t *) chipcHw_REG_INPUTTYPE(pin)) &=
	    ~(chipcHw_REG_INPUTTYPE_MASK <<
	      chipcHw_REG_INPUTTYPE_POSITION(pin));
	*((uint32_t *) chipcHw_REG_INPUTTYPE(pin)) |=
	    (uint32_t) inputType << chipcHw_REG_INPUTTYPE_POSITION(pin);
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_powerUpUsbPhy(void)
{
	reg32_modify_and(&pChipcHw->MiscCtrl,
			 chipcHw_REG_MISC_CTRL_USB_POWERON);
}

static inline void chipcHw_powerDownUsbPhy(void)
{
	reg32_modify_or(&pChipcHw->MiscCtrl,
			chipcHw_REG_MISC_CTRL_USB_POWEROFF);
}

static inline void chipcHw_setUsbHost(void)
{
	reg32_modify_or(&pChipcHw->MiscCtrl,
			chipcHw_REG_MISC_CTRL_USB_MODE_HOST);
}

static inline void chipcHw_setUsbDevice(void)
{
	reg32_modify_and(&pChipcHw->MiscCtrl,
			 chipcHw_REG_MISC_CTRL_USB_MODE_DEVICE);
}

static inline void chipcHw_setClock(chipcHw_CLOCK_e clock,
				    chipcHw_OPTYPE_e type, int mode)
{
	volatile uint32_t *pPLLReg = (uint32_t *) 0x0;
	volatile uint32_t *pClockCtrl = (uint32_t *) 0x0;

	switch (clock) {
	case chipcHw_CLOCK_DDR:
		pPLLReg = &pChipcHw->DDRClock;
		break;
	case chipcHw_CLOCK_ARM:
		pPLLReg = &pChipcHw->ARMClock;
		break;
	case chipcHw_CLOCK_ESW:
		pPLLReg = &pChipcHw->ESWClock;
		break;
	case chipcHw_CLOCK_VPM:
		pPLLReg = &pChipcHw->VPMClock;
		break;
	case chipcHw_CLOCK_ESW125:
		pPLLReg = &pChipcHw->ESW125Clock;
		break;
	case chipcHw_CLOCK_UART:
		pPLLReg = &pChipcHw->UARTClock;
		break;
	case chipcHw_CLOCK_SDIO0:
		pPLLReg = &pChipcHw->SDIO0Clock;
		break;
	case chipcHw_CLOCK_SDIO1:
		pPLLReg = &pChipcHw->SDIO1Clock;
		break;
	case chipcHw_CLOCK_SPI:
		pPLLReg = &pChipcHw->SPIClock;
		break;
	case chipcHw_CLOCK_ETM:
		pPLLReg = &pChipcHw->ETMClock;
		break;
	case chipcHw_CLOCK_USB:
		pPLLReg = &pChipcHw->USBClock;
		if (type == chipcHw_OPTYPE_OUTPUT) {
			if (mode) {
				reg32_modify_and(pPLLReg,
						 ~chipcHw_REG_PLL_CLOCK_POWER_DOWN);
			} else {
				reg32_modify_or(pPLLReg,
						chipcHw_REG_PLL_CLOCK_POWER_DOWN);
			}
		}
		break;
	case chipcHw_CLOCK_LCD:
		pPLLReg = &pChipcHw->LCDClock;
		if (type == chipcHw_OPTYPE_OUTPUT) {
			if (mode) {
				reg32_modify_and(pPLLReg,
						 ~chipcHw_REG_PLL_CLOCK_POWER_DOWN);
			} else {
				reg32_modify_or(pPLLReg,
						chipcHw_REG_PLL_CLOCK_POWER_DOWN);
			}
		}
		break;
	case chipcHw_CLOCK_APM:
		pPLLReg = &pChipcHw->APMClock;
		if (type == chipcHw_OPTYPE_OUTPUT) {
			if (mode) {
				reg32_modify_and(pPLLReg,
						 ~chipcHw_REG_PLL_CLOCK_POWER_DOWN);
			} else {
				reg32_modify_or(pPLLReg,
						chipcHw_REG_PLL_CLOCK_POWER_DOWN);
			}
		}
		break;
	case chipcHw_CLOCK_BUS:
		pClockCtrl = &pChipcHw->ACLKClock;
		break;
	case chipcHw_CLOCK_OTP:
		pClockCtrl = &pChipcHw->OTPClock;
		break;
	case chipcHw_CLOCK_I2C:
		pClockCtrl = &pChipcHw->I2CClock;
		break;
	case chipcHw_CLOCK_I2S0:
		pClockCtrl = &pChipcHw->I2S0Clock;
		break;
	case chipcHw_CLOCK_RTBUS:
		pClockCtrl = &pChipcHw->RTBUSClock;
		break;
	case chipcHw_CLOCK_APM100:
		pClockCtrl = &pChipcHw->APM100Clock;
		break;
	case chipcHw_CLOCK_TSC:
		pClockCtrl = &pChipcHw->TSCClock;
		break;
	case chipcHw_CLOCK_LED:
		pClockCtrl = &pChipcHw->LEDClock;
		break;
	case chipcHw_CLOCK_I2S1:
		pClockCtrl = &pChipcHw->I2S1Clock;
		break;
	}

	if (pPLLReg) {
		switch (type) {
		case chipcHw_OPTYPE_OUTPUT:
			
			if (mode) {
				if (clock == chipcHw_CLOCK_DDR) {
					
					reg32_modify_and(pPLLReg,
							 ~chipcHw_REG_PLL_CLOCK_OUTPUT_ENABLE);
				} else {
					reg32_modify_or(pPLLReg,
							chipcHw_REG_PLL_CLOCK_OUTPUT_ENABLE);
				}
			} else {
				if (clock == chipcHw_CLOCK_DDR) {
					
					reg32_modify_or(pPLLReg,
							chipcHw_REG_PLL_CLOCK_OUTPUT_ENABLE);
				} else {
					reg32_modify_and(pPLLReg,
							 ~chipcHw_REG_PLL_CLOCK_OUTPUT_ENABLE);
				}
			}
			break;
		case chipcHw_OPTYPE_BYPASS:
			
			if (mode) {
				reg32_modify_or(pPLLReg,
						chipcHw_REG_PLL_CLOCK_BYPASS_SELECT);
			} else {
				reg32_modify_and(pPLLReg,
						 ~chipcHw_REG_PLL_CLOCK_BYPASS_SELECT);
			}
			break;
		}
	} else if (pClockCtrl) {
		switch (type) {
		case chipcHw_OPTYPE_OUTPUT:
			if (mode) {
				reg32_modify_or(pClockCtrl,
						chipcHw_REG_DIV_CLOCK_OUTPUT_ENABLE);
			} else {
				reg32_modify_and(pClockCtrl,
						 ~chipcHw_REG_DIV_CLOCK_OUTPUT_ENABLE);
			}
			break;
		case chipcHw_OPTYPE_BYPASS:
			if (mode) {
				reg32_modify_or(pClockCtrl,
						chipcHw_REG_DIV_CLOCK_BYPASS_SELECT);
			} else {
				reg32_modify_and(pClockCtrl,
						 ~chipcHw_REG_DIV_CLOCK_BYPASS_SELECT);
			}
			break;
		}
	}
}

static inline void chipcHw_setClockDisable(chipcHw_CLOCK_e clock)
{

	
	chipcHw_setClock(clock, chipcHw_OPTYPE_OUTPUT, 0);
}

static inline void chipcHw_setClockEnable(chipcHw_CLOCK_e clock)
{

	
	chipcHw_setClock(clock, chipcHw_OPTYPE_OUTPUT, 1);
}

static inline void chipcHw_bypassClockEnable(chipcHw_CLOCK_e clock)
{
	
	chipcHw_setClock(clock, chipcHw_OPTYPE_BYPASS, 1);
}

static inline void chipcHw_bypassClockDisable(chipcHw_CLOCK_e clock)
{
	
	chipcHw_setClock(clock, chipcHw_OPTYPE_BYPASS, 0);

}

static inline int chipcHw_isSoftwareStrapsEnable(void)
{
	return pChipcHw->SoftStraps & 0x00000001;
}

static inline void chipcHw_softwareStrapsEnable(void)
{
	reg32_modify_or(&pChipcHw->SoftStraps, 0x00000001);
}

static inline void chipcHw_softwareStrapsDisable(void)
{
	reg32_modify_and(&pChipcHw->SoftStraps, (~0x00000001));
}

static inline void chipcHw_pllTestEnable(void)
{
	reg32_modify_or(&pChipcHw->PLLConfig,
			chipcHw_REG_PLL_CONFIG_TEST_ENABLE);
}

static inline void chipcHw_pll2TestEnable(void)
{
	reg32_modify_or(&pChipcHw->PLLConfig2,
			chipcHw_REG_PLL_CONFIG_TEST_ENABLE);
}

static inline void chipcHw_pllTestDisable(void)
{
	reg32_modify_and(&pChipcHw->PLLConfig,
			 ~chipcHw_REG_PLL_CONFIG_TEST_ENABLE);
}

static inline void chipcHw_pll2TestDisable(void)
{
	reg32_modify_and(&pChipcHw->PLLConfig2,
			 ~chipcHw_REG_PLL_CONFIG_TEST_ENABLE);
}

static inline int chipcHw_isPllTestEnable(void)
{
	return pChipcHw->PLLConfig & chipcHw_REG_PLL_CONFIG_TEST_ENABLE;
}

static inline int chipcHw_isPll2TestEnable(void)
{
	return pChipcHw->PLLConfig2 & chipcHw_REG_PLL_CONFIG_TEST_ENABLE;
}

static inline void chipcHw_pllTestSelect(uint32_t val)
{
	REG_LOCAL_IRQ_SAVE;
	pChipcHw->PLLConfig &= ~chipcHw_REG_PLL_CONFIG_TEST_SELECT_MASK;
	pChipcHw->PLLConfig |=
	    (val) << chipcHw_REG_PLL_CONFIG_TEST_SELECT_SHIFT;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_pll2TestSelect(uint32_t val)
{

	REG_LOCAL_IRQ_SAVE;
	pChipcHw->PLLConfig2 &= ~chipcHw_REG_PLL_CONFIG_TEST_SELECT_MASK;
	pChipcHw->PLLConfig2 |=
	    (val) << chipcHw_REG_PLL_CONFIG_TEST_SELECT_SHIFT;
	REG_LOCAL_IRQ_RESTORE;
}

static inline uint8_t chipcHw_getPllTestSelected(void)
{
	return (uint8_t) ((pChipcHw->
			   PLLConfig & chipcHw_REG_PLL_CONFIG_TEST_SELECT_MASK)
			  >> chipcHw_REG_PLL_CONFIG_TEST_SELECT_SHIFT);
}

static inline uint8_t chipcHw_getPll2TestSelected(void)
{
	return (uint8_t) ((pChipcHw->
			   PLLConfig2 & chipcHw_REG_PLL_CONFIG_TEST_SELECT_MASK)
			  >> chipcHw_REG_PLL_CONFIG_TEST_SELECT_SHIFT);
}

static inline void chipcHw_pll1Disable(void)
{
	REG_LOCAL_IRQ_SAVE;
	pChipcHw->PLLConfig |= chipcHw_REG_PLL_CONFIG_POWER_DOWN;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_pll2Disable(void)
{
	REG_LOCAL_IRQ_SAVE;
	pChipcHw->PLLConfig2 |= chipcHw_REG_PLL_CONFIG_POWER_DOWN;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_ddrPhaseAlignInterruptEnable(void)
{
	REG_LOCAL_IRQ_SAVE;
	pChipcHw->Spare1 |= chipcHw_REG_SPARE1_DDR_PHASE_INTR_ENABLE;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_ddrPhaseAlignInterruptDisable(void)
{
	REG_LOCAL_IRQ_SAVE;
	pChipcHw->Spare1 &= ~chipcHw_REG_SPARE1_DDR_PHASE_INTR_ENABLE;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void
chipcHw_vpmPhaseAlignInterruptMode(chipcHw_VPM_HW_PHASE_INTR_e mode)
{
	REG_LOCAL_IRQ_SAVE;
	if (mode == chipcHw_VPM_HW_PHASE_INTR_DISABLE) {
		pChipcHw->Spare1 &= ~chipcHw_REG_SPARE1_VPM_PHASE_INTR_ENABLE;
	} else {
		pChipcHw->Spare1 |= chipcHw_REG_SPARE1_VPM_PHASE_INTR_ENABLE;
	}
	pChipcHw->VPMPhaseCtrl2 =
	    (pChipcHw->
	     VPMPhaseCtrl2 & ~(chipcHw_REG_VPM_INTR_SELECT_MASK <<
			       chipcHw_REG_VPM_INTR_SELECT_SHIFT)) | mode;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_ddrSwPhaseAlignEnable(void)
{
	REG_LOCAL_IRQ_SAVE;
	pChipcHw->DDRPhaseCtrl1 |= chipcHw_REG_DDR_SW_PHASE_CTRL_ENABLE;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_ddrSwPhaseAlignDisable(void)
{
	REG_LOCAL_IRQ_SAVE;
	pChipcHw->DDRPhaseCtrl1 &= ~chipcHw_REG_DDR_SW_PHASE_CTRL_ENABLE;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_ddrHwPhaseAlignEnable(void)
{
	REG_LOCAL_IRQ_SAVE;
	pChipcHw->DDRPhaseCtrl1 |= chipcHw_REG_DDR_HW_PHASE_CTRL_ENABLE;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_ddrHwPhaseAlignDisable(void)
{
	REG_LOCAL_IRQ_SAVE;
	pChipcHw->DDRPhaseCtrl1 &= ~chipcHw_REG_DDR_HW_PHASE_CTRL_ENABLE;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_vpmSwPhaseAlignEnable(void)
{
	REG_LOCAL_IRQ_SAVE;
	pChipcHw->VPMPhaseCtrl1 |= chipcHw_REG_VPM_SW_PHASE_CTRL_ENABLE;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_vpmSwPhaseAlignDisable(void)
{
	REG_LOCAL_IRQ_SAVE;
	pChipcHw->VPMPhaseCtrl1 &= ~chipcHw_REG_VPM_SW_PHASE_CTRL_ENABLE;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_vpmHwPhaseAlignEnable(void)
{
	REG_LOCAL_IRQ_SAVE;
	pChipcHw->VPMPhaseCtrl1 |= chipcHw_REG_VPM_HW_PHASE_CTRL_ENABLE;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_vpmHwPhaseAlignDisable(void)
{
	REG_LOCAL_IRQ_SAVE;
	pChipcHw->VPMPhaseCtrl1 &= ~chipcHw_REG_VPM_HW_PHASE_CTRL_ENABLE;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void
chipcHw_setDdrHwPhaseAlignMargin(chipcHw_DDR_HW_PHASE_MARGIN_e margin)
{
	uint32_t ge = 0;
	uint32_t le = 0;

	switch (margin) {
	case chipcHw_DDR_HW_PHASE_MARGIN_STRICT:
		ge = 0x0F;
		le = 0x0F;
		break;
	case chipcHw_DDR_HW_PHASE_MARGIN_MEDIUM:
		ge = 0x03;
		le = 0x3F;
		break;
	case chipcHw_DDR_HW_PHASE_MARGIN_WIDE:
		ge = 0x01;
		le = 0x7F;
		break;
	}

	{
		REG_LOCAL_IRQ_SAVE;

		pChipcHw->DDRPhaseCtrl1 &=
		    ~((chipcHw_REG_DDR_PHASE_VALUE_GE_MASK <<
		       chipcHw_REG_DDR_PHASE_VALUE_GE_SHIFT)
		      || (chipcHw_REG_DDR_PHASE_VALUE_LE_MASK <<
			  chipcHw_REG_DDR_PHASE_VALUE_LE_SHIFT));

		pChipcHw->DDRPhaseCtrl1 |=
		    ((ge << chipcHw_REG_DDR_PHASE_VALUE_GE_SHIFT)
		     || (le << chipcHw_REG_DDR_PHASE_VALUE_LE_SHIFT));

		REG_LOCAL_IRQ_RESTORE;
	}
}

static inline void
chipcHw_setVpmHwPhaseAlignMargin(chipcHw_VPM_HW_PHASE_MARGIN_e margin)
{
	uint32_t ge = 0;
	uint32_t le = 0;

	switch (margin) {
	case chipcHw_VPM_HW_PHASE_MARGIN_STRICT:
		ge = 0x0F;
		le = 0x0F;
		break;
	case chipcHw_VPM_HW_PHASE_MARGIN_MEDIUM:
		ge = 0x03;
		le = 0x3F;
		break;
	case chipcHw_VPM_HW_PHASE_MARGIN_WIDE:
		ge = 0x01;
		le = 0x7F;
		break;
	}

	{
		REG_LOCAL_IRQ_SAVE;

		pChipcHw->VPMPhaseCtrl1 &=
		    ~((chipcHw_REG_VPM_PHASE_VALUE_GE_MASK <<
		       chipcHw_REG_VPM_PHASE_VALUE_GE_SHIFT)
		      || (chipcHw_REG_VPM_PHASE_VALUE_LE_MASK <<
			  chipcHw_REG_VPM_PHASE_VALUE_LE_SHIFT));

		pChipcHw->VPMPhaseCtrl1 |=
		    ((ge << chipcHw_REG_VPM_PHASE_VALUE_GE_SHIFT)
		     || (le << chipcHw_REG_VPM_PHASE_VALUE_LE_SHIFT));

		REG_LOCAL_IRQ_RESTORE;
	}
}

static inline uint32_t chipcHw_isDdrHwPhaseAligned(void)
{
	return (pChipcHw->
		PhaseAlignStatus & chipcHw_REG_DDR_PHASE_ALIGNED) ? 1 : 0;
}

static inline uint32_t chipcHw_isVpmHwPhaseAligned(void)
{
	return (pChipcHw->
		PhaseAlignStatus & chipcHw_REG_VPM_PHASE_ALIGNED) ? 1 : 0;
}

static inline uint32_t chipcHw_getDdrHwPhaseAlignStatus(void)
{
	return (pChipcHw->
		PhaseAlignStatus & chipcHw_REG_DDR_PHASE_STATUS_MASK) >>
	    chipcHw_REG_DDR_PHASE_STATUS_SHIFT;
}

static inline uint32_t chipcHw_getVpmHwPhaseAlignStatus(void)
{
	return (pChipcHw->
		PhaseAlignStatus & chipcHw_REG_VPM_PHASE_STATUS_MASK) >>
	    chipcHw_REG_VPM_PHASE_STATUS_SHIFT;
}

static inline uint32_t chipcHw_getDdrPhaseControl(void)
{
	return (pChipcHw->
		PhaseAlignStatus & chipcHw_REG_DDR_PHASE_CTRL_MASK) >>
	    chipcHw_REG_DDR_PHASE_CTRL_SHIFT;
}

static inline uint32_t chipcHw_getVpmPhaseControl(void)
{
	return (pChipcHw->
		PhaseAlignStatus & chipcHw_REG_VPM_PHASE_CTRL_MASK) >>
	    chipcHw_REG_VPM_PHASE_CTRL_SHIFT;
}

static inline void chipcHw_ddrHwPhaseAlignTimeout(uint32_t busCycle)
{
	REG_LOCAL_IRQ_SAVE;
	pChipcHw->DDRPhaseCtrl2 &=
	    ~(chipcHw_REG_DDR_PHASE_TIMEOUT_COUNT_MASK <<
	      chipcHw_REG_DDR_PHASE_TIMEOUT_COUNT_SHIFT);
	pChipcHw->DDRPhaseCtrl2 |=
	    (busCycle & chipcHw_REG_DDR_PHASE_TIMEOUT_COUNT_MASK) <<
	    chipcHw_REG_DDR_PHASE_TIMEOUT_COUNT_SHIFT;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_vpmHwPhaseAlignTimeout(uint32_t busCycle)
{
	REG_LOCAL_IRQ_SAVE;
	pChipcHw->VPMPhaseCtrl2 &=
	    ~(chipcHw_REG_VPM_PHASE_TIMEOUT_COUNT_MASK <<
	      chipcHw_REG_VPM_PHASE_TIMEOUT_COUNT_SHIFT);
	pChipcHw->VPMPhaseCtrl2 |=
	    (busCycle & chipcHw_REG_VPM_PHASE_TIMEOUT_COUNT_MASK) <<
	    chipcHw_REG_VPM_PHASE_TIMEOUT_COUNT_SHIFT;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_ddrHwPhaseAlignTimeoutInterruptClear(void)
{
	REG_LOCAL_IRQ_SAVE;
	
	pChipcHw->DDRPhaseCtrl2 |= chipcHw_REG_DDR_INTR_SERVICED;
	pChipcHw->DDRPhaseCtrl2 &= ~chipcHw_REG_DDR_INTR_SERVICED;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_vpmHwPhaseAlignTimeoutInterruptClear(void)
{
	REG_LOCAL_IRQ_SAVE;
	
	pChipcHw->VPMPhaseCtrl2 |= chipcHw_REG_VPM_INTR_SERVICED;
	pChipcHw->VPMPhaseCtrl2 &= ~chipcHw_REG_VPM_INTR_SERVICED;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_ddrHwPhaseAlignTimeoutInterruptEnable(void)
{
	REG_LOCAL_IRQ_SAVE;
	chipcHw_ddrHwPhaseAlignTimeoutInterruptClear();	
	
	pChipcHw->DDRPhaseCtrl2 |= chipcHw_REG_DDR_TIMEOUT_INTR_ENABLE;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_vpmHwPhaseAlignTimeoutInterruptEnable(void)
{
	REG_LOCAL_IRQ_SAVE;
	chipcHw_vpmHwPhaseAlignTimeoutInterruptClear();	
	
	pChipcHw->VPMPhaseCtrl2 |= chipcHw_REG_VPM_TIMEOUT_INTR_ENABLE;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_ddrHwPhaseAlignTimeoutInterruptDisable(void)
{
	REG_LOCAL_IRQ_SAVE;
	pChipcHw->DDRPhaseCtrl2 &= ~chipcHw_REG_DDR_TIMEOUT_INTR_ENABLE;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void chipcHw_vpmHwPhaseAlignTimeoutInterruptDisable(void)
{
	REG_LOCAL_IRQ_SAVE;
	pChipcHw->VPMPhaseCtrl2 &= ~chipcHw_REG_VPM_TIMEOUT_INTR_ENABLE;
	REG_LOCAL_IRQ_RESTORE;
}

#endif 
