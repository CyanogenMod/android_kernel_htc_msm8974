/*
 * opp2xxx.h - macros for old-style OMAP2xxx "OPP" definitions
 *
 * Copyright (C) 2005-2009 Texas Instruments, Inc.
 * Copyright (C) 2004-2009 Nokia Corporation
 *
 * Richard Woodruff <r-woodruff2@ti.com>
 *
 * The OMAP2 processor can be run at several discrete 'PRCM configurations'.
 * These configurations are characterized by voltage and speed for clocks.
 * The device is only validated for certain combinations. One way to express
 * these combinations is via the 'ratio's' which the clocks operate with
 * respect to each other. These ratio sets are for a given voltage/DPLL
 * setting. All configurations can be described by a DPLL setting and a ratio
 * There are 3 ratio sets for the 2430 and X ratio sets for 2420.
 *
 * 2430 differs from 2420 in that there are no more phase synchronizers used.
 * They both have a slightly different clock domain setup. 2420(iva1,dsp) vs
 * 2430 (iva2.1, NOdsp, mdm)
 *
 * XXX Missing voltage data.
 *
 * THe format described in this file is deprecated.  Once a reasonable
 * OPP API exists, the data in this file should be converted to use it.
 *
 * This is technically part of the OMAP2xxx clock code.
 */

#ifndef __ARCH_ARM_MACH_OMAP2_OPP2XXX_H
#define __ARCH_ARM_MACH_OMAP2_OPP2XXX_H

struct prcm_config {
	unsigned long xtal_speed;	
	unsigned long dpll_speed;	
	unsigned long mpu_speed;	
	unsigned long cm_clksel_mpu;	
	unsigned long cm_clksel_dsp;	
	unsigned long cm_clksel_gfx;	
	unsigned long cm_clksel1_core;	
	unsigned long cm_clksel1_pll;	
	unsigned long cm_clksel2_pll;	
	unsigned long cm_clksel_mdm;	
	unsigned long base_sdrc_rfr;	
	unsigned short flags;
};


#define RX_CLKSEL_DSS1			(0x10 << 8)
#define RX_CLKSEL_DSS2			(0x0 << 13)
#define RX_CLKSEL_SSI			(0x5 << 20)


#define R1_CLKSEL_L3			(4 << 0)
#define R1_CLKSEL_L4			(2 << 5)
#define R1_CLKSEL_USB			(4 << 25)
#define R1_CM_CLKSEL1_CORE_VAL		(R1_CLKSEL_USB | RX_CLKSEL_SSI | \
					 RX_CLKSEL_DSS2 | RX_CLKSEL_DSS1 | \
					 R1_CLKSEL_L4 | R1_CLKSEL_L3)
#define R1_CLKSEL_MPU			(2 << 0)
#define R1_CM_CLKSEL_MPU_VAL		R1_CLKSEL_MPU
#define R1_CLKSEL_DSP			(2 << 0)
#define R1_CLKSEL_DSP_IF		(2 << 5)
#define R1_CM_CLKSEL_DSP_VAL		(R1_CLKSEL_DSP | R1_CLKSEL_DSP_IF)
#define R1_CLKSEL_GFX			(2 << 0)
#define R1_CM_CLKSEL_GFX_VAL		R1_CLKSEL_GFX
#define R1_CLKSEL_MDM			(4 << 0)
#define R1_CM_CLKSEL_MDM_VAL		R1_CLKSEL_MDM

#define R2_CLKSEL_L3			(6 << 0)
#define R2_CLKSEL_L4			(2 << 5)
#define R2_CLKSEL_USB			(2 << 25)
#define R2_CM_CLKSEL1_CORE_VAL		(R2_CLKSEL_USB | RX_CLKSEL_SSI | \
					 RX_CLKSEL_DSS2 | RX_CLKSEL_DSS1 | \
					 R2_CLKSEL_L4 | R2_CLKSEL_L3)
#define R2_CLKSEL_MPU			(2 << 0)
#define R2_CM_CLKSEL_MPU_VAL		R2_CLKSEL_MPU
#define R2_CLKSEL_DSP			(2 << 0)
#define R2_CLKSEL_DSP_IF		(3 << 5)
#define R2_CM_CLKSEL_DSP_VAL		(R2_CLKSEL_DSP | R2_CLKSEL_DSP_IF)
#define R2_CLKSEL_GFX			(2 << 0)
#define R2_CM_CLKSEL_GFX_VAL		R2_CLKSEL_GFX
#define R2_CLKSEL_MDM			(6 << 0)
#define R2_CM_CLKSEL_MDM_VAL		R2_CLKSEL_MDM

#define RB_CLKSEL_L3			(1 << 0)
#define RB_CLKSEL_L4			(1 << 5)
#define RB_CLKSEL_USB			(1 << 25)
#define RB_CM_CLKSEL1_CORE_VAL		(RB_CLKSEL_USB | RX_CLKSEL_SSI | \
					 RX_CLKSEL_DSS2 | RX_CLKSEL_DSS1 | \
					 RB_CLKSEL_L4 | RB_CLKSEL_L3)
#define RB_CLKSEL_MPU			(1 << 0)
#define RB_CM_CLKSEL_MPU_VAL		RB_CLKSEL_MPU
#define RB_CLKSEL_DSP			(1 << 0)
#define RB_CLKSEL_DSP_IF		(1 << 5)
#define RB_CM_CLKSEL_DSP_VAL		(RB_CLKSEL_DSP | RB_CLKSEL_DSP_IF)
#define RB_CLKSEL_GFX			(1 << 0)
#define RB_CM_CLKSEL_GFX_VAL		RB_CLKSEL_GFX
#define RB_CLKSEL_MDM			(1 << 0)
#define RB_CM_CLKSEL_MDM_VAL		RB_CLKSEL_MDM

#define RXX_CLKSEL_VLYNQ		(0x12 << 15)
#define RXX_CLKSEL_SSI			(0x8 << 20)

#define RIII_CLKSEL_L3			(4 << 0)	
#define RIII_CLKSEL_L4			(2 << 5)	
#define RIII_CLKSEL_USB			(4 << 25)	
#define RIII_CM_CLKSEL1_CORE_VAL	(RIII_CLKSEL_USB | RXX_CLKSEL_SSI | \
					 RXX_CLKSEL_VLYNQ | RX_CLKSEL_DSS2 | \
					 RX_CLKSEL_DSS1 | RIII_CLKSEL_L4 | \
					 RIII_CLKSEL_L3)
#define RIII_CLKSEL_MPU			(2 << 0)	
#define RIII_CM_CLKSEL_MPU_VAL		RIII_CLKSEL_MPU
#define RIII_CLKSEL_DSP			(3 << 0)	
#define RIII_CLKSEL_DSP_IF		(2 << 5)	
#define RIII_SYNC_DSP			(1 << 7)	
#define RIII_CLKSEL_IVA			(6 << 8)	
#define RIII_SYNC_IVA			(1 << 13)	
#define RIII_CM_CLKSEL_DSP_VAL		(RIII_SYNC_IVA | RIII_CLKSEL_IVA | \
					 RIII_SYNC_DSP | RIII_CLKSEL_DSP_IF | \
					 RIII_CLKSEL_DSP)
#define RIII_CLKSEL_GFX			(2 << 0)	
#define RIII_CM_CLKSEL_GFX_VAL		RIII_CLKSEL_GFX

#define RII_CLKSEL_L3			(6 << 0)	
#define RII_CLKSEL_L4			(2 << 5)	
#define RII_CLKSEL_USB			(2 << 25)	
#define RII_CM_CLKSEL1_CORE_VAL		(RII_CLKSEL_USB | RXX_CLKSEL_SSI | \
					 RXX_CLKSEL_VLYNQ | RX_CLKSEL_DSS2 | \
					 RX_CLKSEL_DSS1 | RII_CLKSEL_L4 | \
					 RII_CLKSEL_L3)
#define RII_CLKSEL_MPU			(2 << 0)	
#define RII_CM_CLKSEL_MPU_VAL		RII_CLKSEL_MPU
#define RII_CLKSEL_DSP			(3 << 0)	
#define RII_CLKSEL_DSP_IF		(2 << 5)	
#define RII_SYNC_DSP			(0 << 7)	
#define RII_CLKSEL_IVA			(3 << 8)	
#define RII_SYNC_IVA			(0 << 13)	
#define RII_CM_CLKSEL_DSP_VAL		(RII_SYNC_IVA | RII_CLKSEL_IVA | \
					 RII_SYNC_DSP | RII_CLKSEL_DSP_IF | \
					 RII_CLKSEL_DSP)
#define RII_CLKSEL_GFX			(2 << 0)	
#define RII_CM_CLKSEL_GFX_VAL		RII_CLKSEL_GFX

#define RI_CLKSEL_L3			(4 << 0)	
#define RI_CLKSEL_L4			(2 << 5)	
#define RI_CLKSEL_USB			(4 << 25)	
#define RI_CM_CLKSEL1_CORE_VAL		(RI_CLKSEL_USB |		\
					 RXX_CLKSEL_SSI | RXX_CLKSEL_VLYNQ | \
					 RX_CLKSEL_DSS2 | RX_CLKSEL_DSS1 | \
					 RI_CLKSEL_L4 | RI_CLKSEL_L3)
#define RI_CLKSEL_MPU			(2 << 0)	
#define RI_CM_CLKSEL_MPU_VAL		RI_CLKSEL_MPU
#define RI_CLKSEL_DSP			(3 << 0)	
#define RI_CLKSEL_DSP_IF		(2 << 5)	
#define RI_SYNC_DSP			(1 << 7)	
#define RI_CLKSEL_IVA			(4 << 8)	
#define RI_SYNC_IVA			(0 << 13)	
#define RI_CM_CLKSEL_DSP_VAL		(RI_SYNC_IVA | RI_CLKSEL_IVA |	\
					 RI_SYNC_DSP | RI_CLKSEL_DSP_IF | \
					 RI_CLKSEL_DSP)
#define RI_CLKSEL_GFX			(1 << 0)	
#define RI_CM_CLKSEL_GFX_VAL		RI_CLKSEL_GFX

#define RVII_CLKSEL_L3			(1 << 0)
#define RVII_CLKSEL_L4			(1 << 5)
#define RVII_CLKSEL_DSS1		(1 << 8)
#define RVII_CLKSEL_DSS2		(0 << 13)
#define RVII_CLKSEL_VLYNQ		(1 << 15)
#define RVII_CLKSEL_SSI			(1 << 20)
#define RVII_CLKSEL_USB			(1 << 25)

#define RVII_CM_CLKSEL1_CORE_VAL	(RVII_CLKSEL_USB | RVII_CLKSEL_SSI | \
					 RVII_CLKSEL_VLYNQ | \
					 RVII_CLKSEL_DSS2 | RVII_CLKSEL_DSS1 | \
					 RVII_CLKSEL_L4 | RVII_CLKSEL_L3)

#define RVII_CLKSEL_MPU			(1 << 0) 
#define RVII_CM_CLKSEL_MPU_VAL		RVII_CLKSEL_MPU

#define RVII_CLKSEL_DSP			(1 << 0)
#define RVII_CLKSEL_DSP_IF		(1 << 5)
#define RVII_SYNC_DSP			(0 << 7)
#define RVII_CLKSEL_IVA			(1 << 8)
#define RVII_SYNC_IVA			(0 << 13)
#define RVII_CM_CLKSEL_DSP_VAL		(RVII_SYNC_IVA | RVII_CLKSEL_IVA | \
					 RVII_SYNC_DSP | RVII_CLKSEL_DSP_IF | \
					 RVII_CLKSEL_DSP)

#define RVII_CLKSEL_GFX			(1 << 0)
#define RVII_CM_CLKSEL_GFX_VAL		RVII_CLKSEL_GFX


#define MX_48M_SRC			(0 << 3)
#define MX_54M_SRC			(0 << 5)
#define MX_APLLS_CLIKIN_12		(3 << 23)
#define MX_APLLS_CLIKIN_13		(2 << 23)
#define MX_APLLS_CLIKIN_19_2		(0 << 23)

#define M5A_DPLL_MULT_12		(133 << 12)
#define M5A_DPLL_DIV_12			(5 << 8)
#define M5A_CM_CLKSEL1_PLL_12_VAL	(MX_48M_SRC | MX_54M_SRC | \
					 M5A_DPLL_DIV_12 | M5A_DPLL_MULT_12 | \
					 MX_APLLS_CLIKIN_12)
#define M5A_DPLL_MULT_13		(61 << 12)
#define M5A_DPLL_DIV_13			(2 << 8)
#define M5A_CM_CLKSEL1_PLL_13_VAL	(MX_48M_SRC | MX_54M_SRC | \
					 M5A_DPLL_DIV_13 | M5A_DPLL_MULT_13 | \
					 MX_APLLS_CLIKIN_13)
#define M5A_DPLL_MULT_19		(55 << 12)
#define M5A_DPLL_DIV_19			(3 << 8)
#define M5A_CM_CLKSEL1_PLL_19_VAL	(MX_48M_SRC | MX_54M_SRC | \
					 M5A_DPLL_DIV_19 | M5A_DPLL_MULT_19 | \
					 MX_APLLS_CLIKIN_19_2)
#define M5B_DPLL_MULT_12		(50 << 12)
#define M5B_DPLL_DIV_12			(2 << 8)
#define M5B_CM_CLKSEL1_PLL_12_VAL	(MX_48M_SRC | MX_54M_SRC | \
					 M5B_DPLL_DIV_12 | M5B_DPLL_MULT_12 | \
					 MX_APLLS_CLIKIN_12)
#define M5B_DPLL_MULT_13		(200 << 12)
#define M5B_DPLL_DIV_13			(12 << 8)

#define M5B_CM_CLKSEL1_PLL_13_VAL	(MX_48M_SRC | MX_54M_SRC | \
					 M5B_DPLL_DIV_13 | M5B_DPLL_MULT_13 | \
					 MX_APLLS_CLIKIN_13)
#define M5B_DPLL_MULT_19		(125 << 12)
#define M5B_DPLL_DIV_19			(31 << 8)
#define M5B_CM_CLKSEL1_PLL_19_VAL	(MX_48M_SRC | MX_54M_SRC | \
					 M5B_DPLL_DIV_19 | M5B_DPLL_MULT_19 | \
					 MX_APLLS_CLIKIN_19_2)
#define M4_DPLL_MULT_12			(133 << 12)
#define M4_DPLL_DIV_12			(3 << 8)
#define M4_CM_CLKSEL1_PLL_12_VAL	(MX_48M_SRC | MX_54M_SRC | \
					 M4_DPLL_DIV_12 | M4_DPLL_MULT_12 | \
					 MX_APLLS_CLIKIN_12)

#define M4_DPLL_MULT_13			(399 << 12)
#define M4_DPLL_DIV_13			(12 << 8)
#define M4_CM_CLKSEL1_PLL_13_VAL	(MX_48M_SRC | MX_54M_SRC | \
					 M4_DPLL_DIV_13 | M4_DPLL_MULT_13 | \
					 MX_APLLS_CLIKIN_13)

#define M4_DPLL_MULT_19			(145 << 12)
#define M4_DPLL_DIV_19			(6 << 8)
#define M4_CM_CLKSEL1_PLL_19_VAL	(MX_48M_SRC | MX_54M_SRC | \
					 M4_DPLL_DIV_19 | M4_DPLL_MULT_19 | \
					 MX_APLLS_CLIKIN_19_2)

#define M3_DPLL_MULT_12			(55 << 12)
#define M3_DPLL_DIV_12			(1 << 8)
#define M3_CM_CLKSEL1_PLL_12_VAL	(MX_48M_SRC | MX_54M_SRC | \
					 M3_DPLL_DIV_12 | M3_DPLL_MULT_12 | \
					 MX_APLLS_CLIKIN_12)
#define M3_DPLL_MULT_13			(76 << 12)
#define M3_DPLL_DIV_13			(2 << 8)
#define M3_CM_CLKSEL1_PLL_13_VAL	(MX_48M_SRC | MX_54M_SRC | \
					 M3_DPLL_DIV_13 | M3_DPLL_MULT_13 | \
					 MX_APLLS_CLIKIN_13)
#define M3_DPLL_MULT_19			(17 << 12)
#define M3_DPLL_DIV_19			(0 << 8)
#define M3_CM_CLKSEL1_PLL_19_VAL	(MX_48M_SRC | MX_54M_SRC | \
					 M3_DPLL_DIV_19 | M3_DPLL_MULT_19 | \
					 MX_APLLS_CLIKIN_19_2)

#define M2_DPLL_MULT_12		        (55 << 12)
#define M2_DPLL_DIV_12		        (1 << 8)
#define M2_CM_CLKSEL1_PLL_12_VAL	(MX_48M_SRC | MX_54M_SRC | \
					 M2_DPLL_DIV_12 | M2_DPLL_MULT_12 | \
					 MX_APLLS_CLIKIN_12)

#define M2_DPLL_MULT_13		        (76 << 12)
#define M2_DPLL_DIV_13		        (2 << 8)
#define M2_CM_CLKSEL1_PLL_13_VAL	(MX_48M_SRC | MX_54M_SRC | \
					 M2_DPLL_DIV_13 | M2_DPLL_MULT_13 | \
					 MX_APLLS_CLIKIN_13)

#define M2_DPLL_MULT_19		        (17 << 12)
#define M2_DPLL_DIV_19		        (0 << 8)
#define M2_CM_CLKSEL1_PLL_19_VAL	(MX_48M_SRC | MX_54M_SRC | \
					 M2_DPLL_DIV_19 | M2_DPLL_MULT_19 | \
					 MX_APLLS_CLIKIN_19_2)

#define MB_DPLL_MULT			(1 << 12)
#define MB_DPLL_DIV			(0 << 8)
#define MB_CM_CLKSEL1_PLL_12_VAL	(MX_48M_SRC | MX_54M_SRC | \
					 MB_DPLL_DIV | MB_DPLL_MULT | \
					 MX_APLLS_CLIKIN_12)

#define MB_CM_CLKSEL1_PLL_13_VAL	(MX_48M_SRC | MX_54M_SRC | \
					 MB_DPLL_DIV | MB_DPLL_MULT | \
					 MX_APLLS_CLIKIN_13)

#define MB_CM_CLKSEL1_PLL_19_VAL	(MX_48M_SRC | MX_54M_SRC | \
					 MB_DPLL_DIV | MB_DPLL_MULT | \
					 MX_APLLS_CLIKIN_19)


#define MI_DPLL_MULT_12			(55 << 12)
#define MI_DPLL_DIV_12			(1 << 8)
#define MI_CM_CLKSEL1_PLL_12_VAL	(MX_48M_SRC | MX_54M_SRC | \
					 MI_DPLL_DIV_12 | MI_DPLL_MULT_12 | \
					 MX_APLLS_CLIKIN_12)

#define MII_DPLL_MULT_12		(50 << 12)
#define MII_DPLL_DIV_12			(1 << 8)
#define MII_CM_CLKSEL1_PLL_12_VAL	(MX_48M_SRC | MX_54M_SRC |	\
					 MII_DPLL_DIV_12 | MII_DPLL_MULT_12 | \
					 MX_APLLS_CLIKIN_12)
#define MII_DPLL_MULT_13		(300 << 12)
#define MII_DPLL_DIV_13			(12 << 8)
#define MII_CM_CLKSEL1_PLL_13_VAL	(MX_48M_SRC | MX_54M_SRC |	\
					 MII_DPLL_DIV_13 | MII_DPLL_MULT_13 | \
					 MX_APLLS_CLIKIN_13)

#define MIII_DPLL_MULT_12		(133 << 12)
#define MIII_DPLL_DIV_12		(5 << 8)
#define MIII_CM_CLKSEL1_PLL_12_VAL	(MX_48M_SRC | MX_54M_SRC |	\
					 MIII_DPLL_DIV_12 | \
					 MIII_DPLL_MULT_12 | MX_APLLS_CLIKIN_12)
#define MIII_DPLL_MULT_13		(266 << 12)
#define MIII_DPLL_DIV_13		(12 << 8)
#define MIII_CM_CLKSEL1_PLL_13_VAL	(MX_48M_SRC | MX_54M_SRC |	\
					 MIII_DPLL_DIV_13 | \
					 MIII_DPLL_MULT_13 | MX_APLLS_CLIKIN_13)

#define MVII_CM_CLKSEL1_PLL_12_VAL	MB_CM_CLKSEL1_PLL_12_VAL
#define MVII_CM_CLKSEL1_PLL_13_VAL	MB_CM_CLKSEL1_PLL_13_VAL

#define MX_CLKSEL2_PLL_2x_VAL		(2 << 0)
#define MX_CLKSEL2_PLL_1x_VAL		(1 << 0)

#define S12M	12000000
#define S13M	13000000
#define S19M	19200000
#define S26M	26000000
#define S100M	100000000
#define S133M	133000000
#define S150M	150000000
#define S164M	164000000
#define S165M	165000000
#define S199M	199000000
#define S200M	200000000
#define S266M	266000000
#define S300M	300000000
#define S329M	329000000
#define S330M	330000000
#define S399M	399000000
#define S400M	400000000
#define S532M	532000000
#define S600M	600000000
#define S658M	658000000
#define S660M	660000000
#define S798M	798000000


extern const struct prcm_config omap2420_rate_table[];

#ifdef CONFIG_SOC_OMAP2430
extern const struct prcm_config omap2430_rate_table[];
#else
#define omap2430_rate_table	NULL
#endif
extern const struct prcm_config *rate_table;
extern const struct prcm_config *curr_prcm_set;

#endif
