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


#define MAX_PHASE_ADJUST_COUNT         0xFFFF	
#define MAX_PHASE_ALIGN_ATTEMPTS       10	

#define PLL_CLOCK                      1	
#define NON_PLL_CLOCK                  2	

static int chipcHw_divide(int num, int denom)
    __attribute__ ((section(".aramtext")));

chipcHw_freq chipcHw_getClockFrequency(chipcHw_CLOCK_e clock	
    ) {
	volatile uint32_t *pPLLReg = (uint32_t *) 0x0;
	volatile uint32_t *pClockCtrl = (uint32_t *) 0x0;
	volatile uint32_t *pDependentClock = (uint32_t *) 0x0;
	uint32_t vcoFreqPll1Hz = 0;	
	uint32_t vcoFreqPll2Hz = 0;	
	uint32_t dependentClockType = 0;
	uint32_t vcoHz = 0;

	
	if ((pChipcHw->PLLPreDivider & chipcHw_REG_PLL_PREDIVIDER_NDIV_MODE_MASK) != chipcHw_REG_PLL_PREDIVIDER_NDIV_MODE_INTEGER) {
		uint64_t adjustFreq = 0;

		vcoFreqPll1Hz = chipcHw_XTAL_FREQ_Hz *
		    chipcHw_divide(chipcHw_REG_PLL_PREDIVIDER_P1, chipcHw_REG_PLL_PREDIVIDER_P2) *
		    ((pChipcHw->PLLPreDivider & chipcHw_REG_PLL_PREDIVIDER_NDIV_MASK) >>
		     chipcHw_REG_PLL_PREDIVIDER_NDIV_SHIFT);

		
		adjustFreq = (uint64_t) chipcHw_XTAL_FREQ_Hz *
			(uint64_t) chipcHw_REG_PLL_DIVIDER_NDIV_f_SS *
			chipcHw_divide(chipcHw_REG_PLL_PREDIVIDER_P1, (chipcHw_REG_PLL_PREDIVIDER_P2 * (uint64_t) chipcHw_REG_PLL_DIVIDER_FRAC));
		vcoFreqPll1Hz += (uint32_t) adjustFreq;
	} else {
		vcoFreqPll1Hz = chipcHw_XTAL_FREQ_Hz *
		    chipcHw_divide(chipcHw_REG_PLL_PREDIVIDER_P1, chipcHw_REG_PLL_PREDIVIDER_P2) *
		    ((pChipcHw->PLLPreDivider & chipcHw_REG_PLL_PREDIVIDER_NDIV_MASK) >>
		     chipcHw_REG_PLL_PREDIVIDER_NDIV_SHIFT);
	}
	vcoFreqPll2Hz =
	    chipcHw_XTAL_FREQ_Hz *
		 chipcHw_divide(chipcHw_REG_PLL_PREDIVIDER_P1, chipcHw_REG_PLL_PREDIVIDER_P2) *
	    ((pChipcHw->PLLPreDivider2 & chipcHw_REG_PLL_PREDIVIDER_NDIV_MASK) >>
	     chipcHw_REG_PLL_PREDIVIDER_NDIV_SHIFT);

	switch (clock) {
	case chipcHw_CLOCK_DDR:
		pPLLReg = &pChipcHw->DDRClock;
		vcoHz = vcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_ARM:
		pPLLReg = &pChipcHw->ARMClock;
		vcoHz = vcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_ESW:
		pPLLReg = &pChipcHw->ESWClock;
		vcoHz = vcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_VPM:
		pPLLReg = &pChipcHw->VPMClock;
		vcoHz = vcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_ESW125:
		pPLLReg = &pChipcHw->ESW125Clock;
		vcoHz = vcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_UART:
		pPLLReg = &pChipcHw->UARTClock;
		vcoHz = vcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_SDIO0:
		pPLLReg = &pChipcHw->SDIO0Clock;
		vcoHz = vcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_SDIO1:
		pPLLReg = &pChipcHw->SDIO1Clock;
		vcoHz = vcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_SPI:
		pPLLReg = &pChipcHw->SPIClock;
		vcoHz = vcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_ETM:
		pPLLReg = &pChipcHw->ETMClock;
		vcoHz = vcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_USB:
		pPLLReg = &pChipcHw->USBClock;
		vcoHz = vcoFreqPll2Hz;
		break;
	case chipcHw_CLOCK_LCD:
		pPLLReg = &pChipcHw->LCDClock;
		vcoHz = vcoFreqPll2Hz;
		break;
	case chipcHw_CLOCK_APM:
		pPLLReg = &pChipcHw->APMClock;
		vcoHz = vcoFreqPll2Hz;
		break;
	case chipcHw_CLOCK_BUS:
		pClockCtrl = &pChipcHw->ACLKClock;
		pDependentClock = &pChipcHw->ARMClock;
		vcoHz = vcoFreqPll1Hz;
		dependentClockType = PLL_CLOCK;
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
		pDependentClock = &pChipcHw->ACLKClock;
		dependentClockType = NON_PLL_CLOCK;
		break;
	case chipcHw_CLOCK_APM100:
		pClockCtrl = &pChipcHw->APM100Clock;
		pDependentClock = &pChipcHw->APMClock;
		vcoHz = vcoFreqPll2Hz;
		dependentClockType = PLL_CLOCK;
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
		
		if (*pPLLReg & chipcHw_REG_PLL_CLOCK_BYPASS_SELECT) {
			
			return chipcHw_XTAL_FREQ_Hz;
		} else if (clock == chipcHw_CLOCK_DDR) {
			
			return chipcHw_divide (vcoHz, (((pChipcHw->PLLDivider & 0xFF000000) >> 24) ? ((pChipcHw->PLLDivider & 0xFF000000) >> 24) : 256));
		} else {
			
			if ((pPLLReg == &pChipcHw->LCDClock) && (chipcHw_getChipRevisionNumber() != chipcHw_REV_NUMBER_A0)) {
				vcoHz >>= 1;
			}
			
			return chipcHw_divide(vcoHz, ((*pPLLReg & chipcHw_REG_PLL_CLOCK_MDIV_MASK) ? (*pPLLReg & chipcHw_REG_PLL_CLOCK_MDIV_MASK) : 256));
		}
	} else if (pClockCtrl) {
		
		uint32_t div;
		uint32_t freq = 0;

		if (*pClockCtrl & chipcHw_REG_DIV_CLOCK_BYPASS_SELECT) {
			
			return chipcHw_XTAL_FREQ_Hz;
		} else if (pDependentClock) {
			
			switch (dependentClockType) {
			case PLL_CLOCK:
				if (*pDependentClock & chipcHw_REG_PLL_CLOCK_BYPASS_SELECT) {
					
					freq = chipcHw_XTAL_FREQ_Hz;
				} else {
					
					div = *pDependentClock & chipcHw_REG_PLL_CLOCK_MDIV_MASK;
					freq = div ? chipcHw_divide(vcoHz, div) : 0;
				}
				break;
			case NON_PLL_CLOCK:
				if (pDependentClock == (uint32_t *) &pChipcHw->ACLKClock) {
					freq = chipcHw_getClockFrequency (chipcHw_CLOCK_BUS);
				} else {
					if (*pDependentClock & chipcHw_REG_DIV_CLOCK_BYPASS_SELECT) {
						
						freq = chipcHw_XTAL_FREQ_Hz;
					} else {
						
						div = *pDependentClock & chipcHw_REG_DIV_CLOCK_DIV_MASK;
						freq = chipcHw_divide (chipcHw_XTAL_FREQ_Hz, (div ? div : 256));
					}
				}
				break;
			}
		} else {
			
			freq = chipcHw_XTAL_FREQ_Hz;
		}

		div = *pClockCtrl & chipcHw_REG_DIV_CLOCK_DIV_MASK;
		return chipcHw_divide(freq, (div ? div : 256));
	}
	return 0;
}

chipcHw_freq chipcHw_setClockFrequency(chipcHw_CLOCK_e clock,	
				       uint32_t freq	
    ) {
	volatile uint32_t *pPLLReg = (uint32_t *) 0x0;
	volatile uint32_t *pClockCtrl = (uint32_t *) 0x0;
	volatile uint32_t *pDependentClock = (uint32_t *) 0x0;
	uint32_t vcoFreqPll1Hz = 0;	
	uint32_t desVcoFreqPll1Hz = 0;	
	uint32_t vcoFreqPll2Hz = 0;	
	uint32_t dependentClockType = 0;
	uint32_t vcoHz = 0;
	uint32_t desVcoHz = 0;

	
	if ((pChipcHw->PLLPreDivider & chipcHw_REG_PLL_PREDIVIDER_NDIV_MODE_MASK) != chipcHw_REG_PLL_PREDIVIDER_NDIV_MODE_INTEGER) {
		uint64_t adjustFreq = 0;

		vcoFreqPll1Hz = chipcHw_XTAL_FREQ_Hz *
		    chipcHw_divide(chipcHw_REG_PLL_PREDIVIDER_P1, chipcHw_REG_PLL_PREDIVIDER_P2) *
		    ((pChipcHw->PLLPreDivider & chipcHw_REG_PLL_PREDIVIDER_NDIV_MASK) >>
		     chipcHw_REG_PLL_PREDIVIDER_NDIV_SHIFT);

		
		adjustFreq = (uint64_t) chipcHw_XTAL_FREQ_Hz *
			(uint64_t) chipcHw_REG_PLL_DIVIDER_NDIV_f_SS *
			chipcHw_divide(chipcHw_REG_PLL_PREDIVIDER_P1, (chipcHw_REG_PLL_PREDIVIDER_P2 * (uint64_t) chipcHw_REG_PLL_DIVIDER_FRAC));
		vcoFreqPll1Hz += (uint32_t) adjustFreq;

		
		desVcoFreqPll1Hz = chipcHw_XTAL_FREQ_Hz *
		    chipcHw_divide(chipcHw_REG_PLL_PREDIVIDER_P1, chipcHw_REG_PLL_PREDIVIDER_P2) *
		    (((pChipcHw->PLLPreDivider & chipcHw_REG_PLL_PREDIVIDER_NDIV_MASK) >>
		      chipcHw_REG_PLL_PREDIVIDER_NDIV_SHIFT) + 1);
	} else {
		vcoFreqPll1Hz = desVcoFreqPll1Hz = chipcHw_XTAL_FREQ_Hz *
		    chipcHw_divide(chipcHw_REG_PLL_PREDIVIDER_P1, chipcHw_REG_PLL_PREDIVIDER_P2) *
		    ((pChipcHw->PLLPreDivider & chipcHw_REG_PLL_PREDIVIDER_NDIV_MASK) >>
		     chipcHw_REG_PLL_PREDIVIDER_NDIV_SHIFT);
	}
	vcoFreqPll2Hz = chipcHw_XTAL_FREQ_Hz * chipcHw_divide(chipcHw_REG_PLL_PREDIVIDER_P1, chipcHw_REG_PLL_PREDIVIDER_P2) *
	    ((pChipcHw->PLLPreDivider2 & chipcHw_REG_PLL_PREDIVIDER_NDIV_MASK) >>
	     chipcHw_REG_PLL_PREDIVIDER_NDIV_SHIFT);

	switch (clock) {
	case chipcHw_CLOCK_DDR:
		
		{
			REG_LOCAL_IRQ_SAVE;
			
			pChipcHw->DDRClock = (pChipcHw->DDRClock & ~chipcHw_REG_PLL_CLOCK_TO_BUS_RATIO_MASK) | ((((freq / 2) / chipcHw_getClockFrequency(chipcHw_CLOCK_BUS)) - 1)
				<< chipcHw_REG_PLL_CLOCK_TO_BUS_RATIO_SHIFT);
			REG_LOCAL_IRQ_RESTORE;
		}
		pPLLReg = &pChipcHw->DDRClock;
		vcoHz = vcoFreqPll1Hz;
		desVcoHz = desVcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_ARM:
		pPLLReg = &pChipcHw->ARMClock;
		vcoHz = vcoFreqPll1Hz;
		desVcoHz = desVcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_ESW:
		pPLLReg = &pChipcHw->ESWClock;
		vcoHz = vcoFreqPll1Hz;
		desVcoHz = desVcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_VPM:
		
		{
			REG_LOCAL_IRQ_SAVE;
			pChipcHw->VPMClock = (pChipcHw->VPMClock & ~chipcHw_REG_PLL_CLOCK_TO_BUS_RATIO_MASK) | ((chipcHw_divide (freq, chipcHw_getClockFrequency(chipcHw_CLOCK_BUS)) - 1)
				<< chipcHw_REG_PLL_CLOCK_TO_BUS_RATIO_SHIFT);
			REG_LOCAL_IRQ_RESTORE;
		}
		pPLLReg = &pChipcHw->VPMClock;
		vcoHz = vcoFreqPll1Hz;
		desVcoHz = desVcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_ESW125:
		pPLLReg = &pChipcHw->ESW125Clock;
		vcoHz = vcoFreqPll1Hz;
		desVcoHz = desVcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_UART:
		pPLLReg = &pChipcHw->UARTClock;
		vcoHz = vcoFreqPll1Hz;
		desVcoHz = desVcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_SDIO0:
		pPLLReg = &pChipcHw->SDIO0Clock;
		vcoHz = vcoFreqPll1Hz;
		desVcoHz = desVcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_SDIO1:
		pPLLReg = &pChipcHw->SDIO1Clock;
		vcoHz = vcoFreqPll1Hz;
		desVcoHz = desVcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_SPI:
		pPLLReg = &pChipcHw->SPIClock;
		vcoHz = vcoFreqPll1Hz;
		desVcoHz = desVcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_ETM:
		pPLLReg = &pChipcHw->ETMClock;
		vcoHz = vcoFreqPll1Hz;
		desVcoHz = desVcoFreqPll1Hz;
		break;
	case chipcHw_CLOCK_USB:
		pPLLReg = &pChipcHw->USBClock;
		vcoHz = vcoFreqPll2Hz;
		desVcoHz = vcoFreqPll2Hz;
		break;
	case chipcHw_CLOCK_LCD:
		pPLLReg = &pChipcHw->LCDClock;
		vcoHz = vcoFreqPll2Hz;
		desVcoHz = vcoFreqPll2Hz;
		break;
	case chipcHw_CLOCK_APM:
		pPLLReg = &pChipcHw->APMClock;
		vcoHz = vcoFreqPll2Hz;
		desVcoHz = vcoFreqPll2Hz;
		break;
	case chipcHw_CLOCK_BUS:
		pClockCtrl = &pChipcHw->ACLKClock;
		pDependentClock = &pChipcHw->ARMClock;
		vcoHz = vcoFreqPll1Hz;
		desVcoHz = desVcoFreqPll1Hz;
		dependentClockType = PLL_CLOCK;
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
		pDependentClock = &pChipcHw->ACLKClock;
		dependentClockType = NON_PLL_CLOCK;
		break;
	case chipcHw_CLOCK_APM100:
		pClockCtrl = &pChipcHw->APM100Clock;
		pDependentClock = &pChipcHw->APMClock;
		vcoHz = vcoFreqPll2Hz;
		desVcoHz = vcoFreqPll2Hz;
		dependentClockType = PLL_CLOCK;
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
		
		reg32_modify_and(pPLLReg, ~chipcHw_REG_PLL_CLOCK_SOURCE_GPIO);
		reg32_modify_or(pPLLReg, chipcHw_REG_PLL_CLOCK_BYPASS_SELECT);
		
		if (pPLLReg == &pChipcHw->DDRClock) {
			
			reg32_write(&pChipcHw->PLLDivider, (pChipcHw->PLLDivider & 0x00FFFFFF) | ((chipcHw_REG_PLL_DIVIDER_MDIV (desVcoHz, freq)) << 24));
			
			freq = chipcHw_divide(vcoHz, (((pChipcHw->PLLDivider & 0xFF000000) >> 24) ? ((pChipcHw->PLLDivider & 0xFF000000) >> 24) : 256));
		} else {
			
			if ((pPLLReg == &pChipcHw->LCDClock) && (chipcHw_getChipRevisionNumber() != chipcHw_REV_NUMBER_A0)) {
				desVcoHz >>= 1;
				vcoHz >>= 1;
			}
			
			reg32_modify_and(pPLLReg, ~(chipcHw_REG_PLL_CLOCK_MDIV_MASK));
			reg32_modify_or(pPLLReg, chipcHw_REG_PLL_DIVIDER_MDIV(desVcoHz, freq));
			
			freq = chipcHw_divide(vcoHz, ((*(pPLLReg) & chipcHw_REG_PLL_CLOCK_MDIV_MASK) ? (*(pPLLReg) & chipcHw_REG_PLL_CLOCK_MDIV_MASK) : 256));
		}
		
		udelay(1);
		
		reg32_modify_and(pPLLReg, ~chipcHw_REG_PLL_CLOCK_BYPASS_SELECT);
		
		return freq;
	} else if (pClockCtrl) {
		uint32_t divider = 0;

		
		reg32_modify_and(pClockCtrl,
				 ~chipcHw_REG_DIV_CLOCK_BYPASS_SELECT);

		
		if (pDependentClock) {
			switch (dependentClockType) {
			case PLL_CLOCK:
				divider = chipcHw_divide(chipcHw_divide (desVcoHz, (*pDependentClock & chipcHw_REG_PLL_CLOCK_MDIV_MASK)), freq);
				break;
			case NON_PLL_CLOCK:
				{
					uint32_t sourceClock = 0;

					if (pDependentClock == (uint32_t *) &pChipcHw->ACLKClock) {
						sourceClock = chipcHw_getClockFrequency (chipcHw_CLOCK_BUS);
					} else {
						uint32_t div = *pDependentClock & chipcHw_REG_DIV_CLOCK_DIV_MASK;
						sourceClock = chipcHw_divide (chipcHw_XTAL_FREQ_Hz, ((div) ? div : 256));
					}
					divider = chipcHw_divide(sourceClock, freq);
				}
				break;
			}
		} else {
			divider = chipcHw_divide(chipcHw_XTAL_FREQ_Hz, freq);
		}

		if (divider) {
			REG_LOCAL_IRQ_SAVE;
			
			*pClockCtrl = (*pClockCtrl & (~chipcHw_REG_DIV_CLOCK_DIV_MASK)) | (((divider > 256) ? chipcHw_REG_DIV_CLOCK_DIV_256 : divider) & chipcHw_REG_DIV_CLOCK_DIV_MASK);
			REG_LOCAL_IRQ_RESTORE;
			return freq;
		}
	}

	return 0;
}

EXPORT_SYMBOL(chipcHw_setClockFrequency);

static int vpmPhaseAlignA0(void)
{
	uint32_t phaseControl;
	uint32_t phaseValue;
	uint32_t prevPhaseComp;
	int iter = 0;
	int adjustCount = 0;
	int count = 0;

	for (iter = 0; (iter < MAX_PHASE_ALIGN_ATTEMPTS) && (adjustCount < MAX_PHASE_ADJUST_COUNT); iter++) {
		phaseControl = (pChipcHw->VPMClock & chipcHw_REG_PLL_CLOCK_PHASE_CONTROL_MASK) >> chipcHw_REG_PLL_CLOCK_PHASE_CONTROL_SHIFT;
		phaseValue = 0;
		prevPhaseComp = 0;

		

		
		phaseValue = pChipcHw->VPMClock;
		do {
			
			prevPhaseComp = phaseValue & chipcHw_REG_PLL_CLOCK_PHASE_COMP;
			
			reg32_write(&pChipcHw->VPMClock, (pChipcHw->VPMClock & (~chipcHw_REG_PLL_CLOCK_PHASE_CONTROL_MASK)) | (phaseControl << chipcHw_REG_PLL_CLOCK_PHASE_CONTROL_SHIFT));
			
			udelay(1);
			/* Toggle the LOAD_CH after phase control is written. */
			pChipcHw->VPMClock ^= chipcHw_REG_PLL_CLOCK_PHASE_UPDATE_ENABLE;
			
			phaseValue = pChipcHw->VPMClock;

			if ((phaseValue & chipcHw_REG_PLL_CLOCK_PHASE_COMP) == 0x0) {
				phaseControl = (0x3F & (phaseControl - 1));
			} else {
				
				phaseControl = (0x3F & (phaseControl + 1));
			}
			
			adjustCount++;
		} while (((prevPhaseComp == (phaseValue & chipcHw_REG_PLL_CLOCK_PHASE_COMP)) ||	
			  ((phaseValue & chipcHw_REG_PLL_CLOCK_PHASE_COMP) != 0x0)) &&	
			 (adjustCount < MAX_PHASE_ADJUST_COUNT)	
		    );

		if (adjustCount >= MAX_PHASE_ADJUST_COUNT) {
			
			return -1;
		}

		

		for (count = 0; (count < 5) && ((phaseValue & chipcHw_REG_PLL_CLOCK_PHASE_COMP) == 0); count++) {
			phaseControl = (0x3F & (phaseControl + 1));
			reg32_write(&pChipcHw->VPMClock, (pChipcHw->VPMClock & (~chipcHw_REG_PLL_CLOCK_PHASE_CONTROL_MASK)) | (phaseControl << chipcHw_REG_PLL_CLOCK_PHASE_CONTROL_SHIFT));
			
			udelay(1);
			/* Toggle the LOAD_CH after phase control is written. */
			pChipcHw->VPMClock ^= chipcHw_REG_PLL_CLOCK_PHASE_UPDATE_ENABLE;
			phaseValue = pChipcHw->VPMClock;
			
			adjustCount++;
		}

		if (adjustCount >= MAX_PHASE_ADJUST_COUNT) {
			
			return -1;
		}

		if (count != 5) {
			
			continue;
		}

		

		for (count = 0; (count < 3) && ((phaseValue & chipcHw_REG_PLL_CLOCK_PHASE_COMP) == 0); count++) {
			phaseControl = (0x3F & (phaseControl - 1));
			reg32_write(&pChipcHw->VPMClock, (pChipcHw->VPMClock & (~chipcHw_REG_PLL_CLOCK_PHASE_CONTROL_MASK)) | (phaseControl << chipcHw_REG_PLL_CLOCK_PHASE_CONTROL_SHIFT));
			
			udelay(1);
			/* Toggle the LOAD_CH after phase control is written. */
			pChipcHw->VPMClock ^= chipcHw_REG_PLL_CLOCK_PHASE_UPDATE_ENABLE;
			phaseValue = pChipcHw->VPMClock;
			
			adjustCount++;
		}

		if (adjustCount >= MAX_PHASE_ADJUST_COUNT) {
			
			return -1;
		}

		if (count != 3) {
			
			continue;
		}

		

		for (count = 0; (count < 5); count++) {
			phaseControl = (0x3F & (phaseControl - 1));
			reg32_write(&pChipcHw->VPMClock, (pChipcHw->VPMClock & (~chipcHw_REG_PLL_CLOCK_PHASE_CONTROL_MASK)) | (phaseControl << chipcHw_REG_PLL_CLOCK_PHASE_CONTROL_SHIFT));
			
			udelay(1);
			/* Toggle the LOAD_CH after phase control is written. */
			pChipcHw->VPMClock ^= chipcHw_REG_PLL_CLOCK_PHASE_UPDATE_ENABLE;
			phaseValue = pChipcHw->VPMClock;
			
			adjustCount++;
		}

		if (adjustCount >= MAX_PHASE_ADJUST_COUNT) {
			
			return -1;
		}

		if ((phaseValue & chipcHw_REG_PLL_CLOCK_PHASE_COMP) == 0) {
			
			continue;
		}

		

		do {
			
			prevPhaseComp = phaseValue;
			
			reg32_write(&pChipcHw->VPMClock, (pChipcHw->VPMClock & (~chipcHw_REG_PLL_CLOCK_PHASE_CONTROL_MASK)) | (phaseControl << chipcHw_REG_PLL_CLOCK_PHASE_CONTROL_SHIFT));
			
			udelay(1);
			/* Toggle the LOAD_CH after phase control is written. */
			pChipcHw->VPMClock ^=
			    chipcHw_REG_PLL_CLOCK_PHASE_UPDATE_ENABLE;
			
			phaseValue = pChipcHw->VPMClock;

			if ((phaseValue & chipcHw_REG_PLL_CLOCK_PHASE_COMP) == 0x0) {
				phaseControl = (0x3F & (phaseControl - 1));
			} else {
				
				phaseControl = (0x3F & (phaseControl + 1));
			}

			
			adjustCount++;
		} while (((prevPhaseComp == (phaseValue & chipcHw_REG_PLL_CLOCK_PHASE_COMP)) || ((phaseValue & chipcHw_REG_PLL_CLOCK_PHASE_COMP) != 0x0)) && (adjustCount < MAX_PHASE_ADJUST_COUNT));

		if (adjustCount >= MAX_PHASE_ADJUST_COUNT) {
			
			return -1;
		} else {
			
			break;
		}
	}

	
	phaseControl = (((pChipcHw->VPMClock >> chipcHw_REG_PLL_CLOCK_PHASE_CONTROL_SHIFT) - 1) & 0x3F);
	{
		REG_LOCAL_IRQ_SAVE;

		pChipcHw->VPMClock = (pChipcHw->VPMClock & ~chipcHw_REG_PLL_CLOCK_PHASE_CONTROL_MASK) | (phaseControl << chipcHw_REG_PLL_CLOCK_PHASE_CONTROL_SHIFT);
		
		pChipcHw->VPMClock ^= chipcHw_REG_PLL_CLOCK_PHASE_UPDATE_ENABLE;

		REG_LOCAL_IRQ_RESTORE;
	}
	
	return (int)adjustCount;
}

int chipcHw_vpmPhaseAlign(void)
{

	if (chipcHw_getChipRevisionNumber() == chipcHw_REV_NUMBER_A0) {
		return vpmPhaseAlignA0();
	} else {
		uint32_t phaseControl = chipcHw_getVpmPhaseControl();
		uint32_t phaseValue = 0;
		int adjustCount = 0;

		
		pChipcHw->Spare1 &= ~chipcHw_REG_SPARE1_VPM_BUS_ACCESS_ENABLE;
		
		chipcHw_vpmHwPhaseAlignDisable();
		
		chipcHw_vpmSwPhaseAlignEnable();
		
		while (adjustCount < MAX_PHASE_ADJUST_COUNT) {
			phaseValue = chipcHw_getVpmHwPhaseAlignStatus();

			
			if (phaseValue > 0xF) {
				
				phaseControl++;
			} else if (phaseValue < 0xF) {
				
				phaseControl--;
			} else {
				
				pChipcHw->Spare1 |= chipcHw_REG_SPARE1_VPM_BUS_ACCESS_ENABLE;
				
				return adjustCount;
			}
			
			reg32_write(&pChipcHw->VPMClock, (pChipcHw->VPMClock & (~chipcHw_REG_PLL_CLOCK_PHASE_CONTROL_MASK)) | (phaseControl << chipcHw_REG_PLL_CLOCK_PHASE_CONTROL_SHIFT));
			
			udelay(1);
			/* Toggle the LOAD_CH after phase control is written. */
			pChipcHw->VPMClock ^= chipcHw_REG_PLL_CLOCK_PHASE_UPDATE_ENABLE;
			
			adjustCount++;
		}
	}

	
	pChipcHw->Spare1 &= ~chipcHw_REG_SPARE1_VPM_BUS_ACCESS_ENABLE;
	return -1;
}

static int chipcHw_divide(int num, int denom)
{
	int r;
	int t = 1;

	
	
	while ((denom & 0x40000000) == 0) {	
		denom = denom << 1;
		t = t << 1;
	}

	
	r = 0;

	do {
		
		if ((num - denom) >= 0) {
			
			num = num - denom;
			r = r + t;
		}
		
		denom = denom >> 1;
		t = t >> 1;
	} while (t != 0);

	return r;
}
