/*
 * opp2420_data.c - old-style "OPP" table for OMAP2420
 *
 * Copyright (C) 2005-2009 Texas Instruments, Inc.
 * Copyright (C) 2004-2009 Nokia Corporation
 *
 * Richard Woodruff <r-woodruff2@ti.com>
 *
 * The OMAP2 processor can be run at several discrete 'PRCM configurations'.
 * These configurations are characterized by voltage and speed for clocks.
 * The device is only validated for certain combinations. One way to express
 * these combinations is via the 'ratios' which the clocks operate with
 * respect to each other. These ratio sets are for a given voltage/DPLL
 * setting. All configurations can be described by a DPLL setting and a ratio.
 *
 * XXX Missing voltage data.
 * XXX Missing 19.2MHz sys_clk rate sets (needed for N800/N810)
 *
 * THe format described in this file is deprecated.  Once a reasonable
 * OPP API exists, the data in this file should be converted to use it.
 *
 * This is technically part of the OMAP2xxx clock code.
 *
 * Considerable work is still needed to fully support dynamic frequency
 * changes on OMAP2xxx-series chips.  Readers interested in such a
 * project are encouraged to review the Maemo Diablo RX-34 and RX-44
 * kernel source at:
 *     http://repository.maemo.org/pool/diablo/free/k/kernel-source-diablo/
 */

#include <plat/hardware.h>

#include "opp2xxx.h"
#include "sdrc.h"
#include "clock.h"

const struct prcm_config omap2420_rate_table[] = {
	
	{S12M, S660M, S330M, RI_CM_CLKSEL_MPU_VAL,		
		RI_CM_CLKSEL_DSP_VAL, RI_CM_CLKSEL_GFX_VAL,
		RI_CM_CLKSEL1_CORE_VAL, MI_CM_CLKSEL1_PLL_12_VAL,
		MX_CLKSEL2_PLL_2x_VAL, 0, SDRC_RFR_CTRL_165MHz,
		RATE_IN_242X},

	
	{S12M, S600M, S300M, RII_CM_CLKSEL_MPU_VAL,		
		RII_CM_CLKSEL_DSP_VAL, RII_CM_CLKSEL_GFX_VAL,
		RII_CM_CLKSEL1_CORE_VAL, MII_CM_CLKSEL1_PLL_12_VAL,
		MX_CLKSEL2_PLL_2x_VAL, 0, SDRC_RFR_CTRL_100MHz,
		RATE_IN_242X},

	{S13M, S600M, S300M, RII_CM_CLKSEL_MPU_VAL,		
		RII_CM_CLKSEL_DSP_VAL, RII_CM_CLKSEL_GFX_VAL,
		RII_CM_CLKSEL1_CORE_VAL, MII_CM_CLKSEL1_PLL_13_VAL,
		MX_CLKSEL2_PLL_2x_VAL, 0, SDRC_RFR_CTRL_100MHz,
		RATE_IN_242X},

	
	{S12M, S532M, S266M, RIII_CM_CLKSEL_MPU_VAL,		
		RIII_CM_CLKSEL_DSP_VAL, RIII_CM_CLKSEL_GFX_VAL,
		RIII_CM_CLKSEL1_CORE_VAL, MIII_CM_CLKSEL1_PLL_12_VAL,
		MX_CLKSEL2_PLL_2x_VAL, 0, SDRC_RFR_CTRL_133MHz,
		RATE_IN_242X},

	{S13M, S532M, S266M, RIII_CM_CLKSEL_MPU_VAL,		
		RIII_CM_CLKSEL_DSP_VAL, RIII_CM_CLKSEL_GFX_VAL,
		RIII_CM_CLKSEL1_CORE_VAL, MIII_CM_CLKSEL1_PLL_13_VAL,
		MX_CLKSEL2_PLL_2x_VAL, 0, SDRC_RFR_CTRL_133MHz,
		RATE_IN_242X},

	
	{S12M, S300M, S150M, RII_CM_CLKSEL_MPU_VAL,		
		RII_CM_CLKSEL_DSP_VAL, RII_CM_CLKSEL_GFX_VAL,
		RII_CM_CLKSEL1_CORE_VAL, MII_CM_CLKSEL1_PLL_12_VAL,
		MX_CLKSEL2_PLL_2x_VAL, 0, SDRC_RFR_CTRL_100MHz,
		RATE_IN_242X},

	{S13M, S300M, S150M, RII_CM_CLKSEL_MPU_VAL,		
		RII_CM_CLKSEL_DSP_VAL, RII_CM_CLKSEL_GFX_VAL,
		RII_CM_CLKSEL1_CORE_VAL, MII_CM_CLKSEL1_PLL_13_VAL,
		MX_CLKSEL2_PLL_2x_VAL, 0, SDRC_RFR_CTRL_100MHz,
		RATE_IN_242X},

	
	{S12M, S266M, S133M, RIII_CM_CLKSEL_MPU_VAL,		
		RIII_CM_CLKSEL_DSP_VAL, RIII_CM_CLKSEL_GFX_VAL,
		RIII_CM_CLKSEL1_CORE_VAL, MIII_CM_CLKSEL1_PLL_12_VAL,
		MX_CLKSEL2_PLL_2x_VAL, 0, SDRC_RFR_CTRL_133MHz,
		RATE_IN_242X},

	{S13M, S266M, S133M, RIII_CM_CLKSEL_MPU_VAL,		
		RIII_CM_CLKSEL_DSP_VAL, RIII_CM_CLKSEL_GFX_VAL,
		RIII_CM_CLKSEL1_CORE_VAL, MIII_CM_CLKSEL1_PLL_13_VAL,
		MX_CLKSEL2_PLL_2x_VAL, 0, SDRC_RFR_CTRL_133MHz,
		RATE_IN_242X},

	
	{S12M, S12M, S12M, RVII_CM_CLKSEL_MPU_VAL,		
		RVII_CM_CLKSEL_DSP_VAL, RVII_CM_CLKSEL_GFX_VAL,
		RVII_CM_CLKSEL1_CORE_VAL, MVII_CM_CLKSEL1_PLL_12_VAL,
		MX_CLKSEL2_PLL_2x_VAL, 0, SDRC_RFR_CTRL_BYPASS,
		RATE_IN_242X},

	
	{S13M, S13M, S13M, RVII_CM_CLKSEL_MPU_VAL,		
		RVII_CM_CLKSEL_DSP_VAL, RVII_CM_CLKSEL_GFX_VAL,
		RVII_CM_CLKSEL1_CORE_VAL, MVII_CM_CLKSEL1_PLL_13_VAL,
		MX_CLKSEL2_PLL_2x_VAL, 0, SDRC_RFR_CTRL_BYPASS,
		RATE_IN_242X},

	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
