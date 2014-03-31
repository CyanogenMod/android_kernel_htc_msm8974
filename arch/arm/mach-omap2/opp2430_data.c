/*
 * opp2430_data.c - old-style "OPP" table for OMAP2430
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
 * 2430 differs from 2420 in that there are no more phase synchronizers used.
 * They both have a slightly different clock domain setup. 2420(iva1,dsp) vs
 * 2430 (iva2.1, NOdsp, mdm)
 *
 * XXX Missing voltage data.
 * XXX Missing 19.2MHz sys_clk rate sets.
 *
 * THe format described in this file is deprecated.  Once a reasonable
 * OPP API exists, the data in this file should be converted to use it.
 *
 * This is technically part of the OMAP2xxx clock code.
 */

#include <plat/hardware.h>

#include "opp2xxx.h"
#include "sdrc.h"
#include "clock.h"

const struct prcm_config omap2430_rate_table[] = {
	
	{S13M, S798M, S399M, R2_CM_CLKSEL_MPU_VAL,		
		R2_CM_CLKSEL_DSP_VAL, R2_CM_CLKSEL_GFX_VAL,
		R2_CM_CLKSEL1_CORE_VAL, M4_CM_CLKSEL1_PLL_13_VAL,
		MX_CLKSEL2_PLL_2x_VAL, R2_CM_CLKSEL_MDM_VAL,
		SDRC_RFR_CTRL_133MHz,
		RATE_IN_243X},

	
	{S13M, S658M, S329M, R1_CM_CLKSEL_MPU_VAL,		
		R1_CM_CLKSEL_DSP_VAL, R1_CM_CLKSEL_GFX_VAL,
		R1_CM_CLKSEL1_CORE_VAL, M2_CM_CLKSEL1_PLL_13_VAL,
		MX_CLKSEL2_PLL_2x_VAL, R1_CM_CLKSEL_MDM_VAL,
		SDRC_RFR_CTRL_165MHz,
		RATE_IN_243X},

	
	{S13M, S532M, S266M, R1_CM_CLKSEL_MPU_VAL,		
		R1_CM_CLKSEL_DSP_VAL, R1_CM_CLKSEL_GFX_VAL,
		R1_CM_CLKSEL1_CORE_VAL, M5A_CM_CLKSEL1_PLL_13_VAL,
		MX_CLKSEL2_PLL_2x_VAL, R1_CM_CLKSEL_MDM_VAL,
		SDRC_RFR_CTRL_133MHz,
		RATE_IN_243X},

	
	{S13M, S400M, S200M, R1_CM_CLKSEL_MPU_VAL,		
		R1_CM_CLKSEL_DSP_VAL, R1_CM_CLKSEL_GFX_VAL,
		R1_CM_CLKSEL1_CORE_VAL, M5B_CM_CLKSEL1_PLL_13_VAL,
		MX_CLKSEL2_PLL_2x_VAL, R1_CM_CLKSEL_MDM_VAL,
		SDRC_RFR_CTRL_100MHz,
		RATE_IN_243X},

	
	{S13M, S399M, S199M, R2_CM_CLKSEL_MPU_VAL,		
		R2_CM_CLKSEL_DSP_VAL, R2_CM_CLKSEL_GFX_VAL,
		R2_CM_CLKSEL1_CORE_VAL, M4_CM_CLKSEL1_PLL_13_VAL,
		MX_CLKSEL2_PLL_1x_VAL, R2_CM_CLKSEL_MDM_VAL,
		SDRC_RFR_CTRL_133MHz,
		RATE_IN_243X},

	
	{S13M, S329M, S164M, R1_CM_CLKSEL_MPU_VAL,		
		R1_CM_CLKSEL_DSP_VAL, R1_CM_CLKSEL_GFX_VAL,
		R1_CM_CLKSEL1_CORE_VAL, M2_CM_CLKSEL1_PLL_13_VAL,
		MX_CLKSEL2_PLL_1x_VAL, R1_CM_CLKSEL_MDM_VAL,
		SDRC_RFR_CTRL_165MHz,
		RATE_IN_243X},

	
	{S13M, S266M, S133M, R1_CM_CLKSEL_MPU_VAL,		
		R1_CM_CLKSEL_DSP_VAL, R1_CM_CLKSEL_GFX_VAL,
		R1_CM_CLKSEL1_CORE_VAL, M5A_CM_CLKSEL1_PLL_13_VAL,
		MX_CLKSEL2_PLL_1x_VAL, R1_CM_CLKSEL_MDM_VAL,
		SDRC_RFR_CTRL_133MHz,
		RATE_IN_243X},

	
	{S13M, S200M, S100M, R1_CM_CLKSEL_MPU_VAL,		
		R1_CM_CLKSEL_DSP_VAL, R1_CM_CLKSEL_GFX_VAL,
		R1_CM_CLKSEL1_CORE_VAL, M5B_CM_CLKSEL1_PLL_13_VAL,
		MX_CLKSEL2_PLL_1x_VAL, R1_CM_CLKSEL_MDM_VAL,
		SDRC_RFR_CTRL_100MHz,
		RATE_IN_243X},

	
	{S13M, S13M, S13M, RB_CM_CLKSEL_MPU_VAL,		
		RB_CM_CLKSEL_DSP_VAL, RB_CM_CLKSEL_GFX_VAL,
		RB_CM_CLKSEL1_CORE_VAL, MB_CM_CLKSEL1_PLL_13_VAL,
		MX_CLKSEL2_PLL_2x_VAL, RB_CM_CLKSEL_MDM_VAL,
		SDRC_RFR_CTRL_BYPASS,
		RATE_IN_243X},

	
	{S12M, S12M, S12M, RB_CM_CLKSEL_MPU_VAL,		
		RB_CM_CLKSEL_DSP_VAL, RB_CM_CLKSEL_GFX_VAL,
		RB_CM_CLKSEL1_CORE_VAL, MB_CM_CLKSEL1_PLL_12_VAL,
		MX_CLKSEL2_PLL_2x_VAL, RB_CM_CLKSEL_MDM_VAL,
		SDRC_RFR_CTRL_BYPASS,
		RATE_IN_243X},

	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
