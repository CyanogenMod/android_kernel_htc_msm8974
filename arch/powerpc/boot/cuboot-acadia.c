/*
 * Old U-boot compatibility for Acadia
 *
 * Author: Josh Boyer <jwboyer@linux.vnet.ibm.com>
 *
 * Copyright 2008 IBM Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "ops.h"
#include "io.h"
#include "dcr.h"
#include "stdio.h"
#include "4xx.h"
#include "44x.h"
#include "cuboot.h"

#define TARGET_4xx
#include "ppcboot.h"

static bd_t bd;

#define CPR_PERD0_SPIDV_MASK   0x000F0000     

#define PLLC_SRC_MASK	       0x20000000     

#define PLLD_FBDV_MASK	       0x1F000000     
#define PLLD_FWDVA_MASK        0x000F0000     
#define PLLD_FWDVB_MASK        0x00000700     

#define PRIMAD_CPUDV_MASK      0x0F000000     
#define PRIMAD_PLBDV_MASK      0x000F0000     
#define PRIMAD_OPBDV_MASK      0x00000F00     
#define PRIMAD_EBCDV_MASK      0x0000000F     

#define PERD0_PWMDV_MASK       0xFF000000     
#define PERD0_SPIDV_MASK       0x000F0000     
#define PERD0_U0DV_MASK        0x0000FF00     
#define PERD0_U1DV_MASK        0x000000FF     

static void get_clocks(void)
{
	unsigned long sysclk, cpr_plld, cpr_pllc, cpr_primad, plloutb, i;
	unsigned long pllFwdDiv, pllFwdDivB, pllFbkDiv, pllPlbDiv, pllExtBusDiv;
	unsigned long pllOpbDiv, freqEBC, freqUART, freqOPB;
	unsigned long div;		
	unsigned long umin;		
	unsigned short diff;		
	unsigned long udiv;		
	unsigned short idiff;		
	unsigned short ibdiv;		
	unsigned long est;		
	unsigned long baud;
	void *np;

	
	sysclk = (in_8((unsigned char *)0x80000000) == 0xc) ? 66666666 : 33333000;

	cpr_plld = CPR0_READ(DCRN_CPR0_PLLD);
	cpr_pllc = CPR0_READ(DCRN_CPR0_PLLC);

	pllFwdDiv = ((cpr_plld & PLLD_FWDVA_MASK) >> 16);

	pllFwdDivB = ((cpr_plld & PLLD_FWDVB_MASK) >> 8);
	if (pllFwdDivB == 0)
		pllFwdDivB = 8;

	pllFbkDiv = ((cpr_plld & PLLD_FBDV_MASK) >> 24);
	if (pllFbkDiv == 0)
		pllFbkDiv = 256;

	cpr_primad = CPR0_READ(DCRN_CPR0_PRIMAD);

	pllPlbDiv = ((cpr_primad & PRIMAD_PLBDV_MASK) >> 16);
	if (pllPlbDiv == 0)
		pllPlbDiv = 16;

	pllExtBusDiv = (cpr_primad & PRIMAD_EBCDV_MASK);
	if (pllExtBusDiv == 0)
		pllExtBusDiv = 16;

	pllOpbDiv = ((cpr_primad & PRIMAD_OPBDV_MASK) >> 8);
	if (pllOpbDiv == 0)
		pllOpbDiv = 16;

	freqOPB = (sysclk *pllFbkDiv) /pllOpbDiv;

	freqEBC = (sysclk * pllFbkDiv) / pllExtBusDiv;

	plloutb = ((sysclk * ((cpr_pllc & PLLC_SRC_MASK) ?
					   pllFwdDivB : pllFwdDiv) *
		    pllFbkDiv) / pllFwdDivB);

	np = find_node_by_alias("serial0");
	if (getprop(np, "current-speed", &baud, sizeof(baud)) != sizeof(baud))
		fatal("no current-speed property\n\r");

	udiv = 256;			
	div = plloutb / (16 * baud); 
	umin = (plloutb / freqOPB) << 1;	
	diff = 256;			

	for (i = 256; i > umin; i--) {
		ibdiv = div / i;
		est = i * ibdiv;
		idiff = (est > div) ? (est-div) : (div-est);
		if (idiff == 0) {
			udiv = i;
			break;      
		} else if (idiff < diff) {
			udiv = i;       
			diff = idiff;   
		}
	}
	freqUART = plloutb / udiv;

	dt_fixup_cpu_clocks(bd.bi_procfreq, bd.bi_intfreq, bd.bi_plb_busfreq);
	dt_fixup_clock("/plb/ebc", freqEBC);
	dt_fixup_clock("/plb/opb", freqOPB);
	dt_fixup_clock("/plb/opb/serial@ef600300", freqUART);
	dt_fixup_clock("/plb/opb/serial@ef600400", freqUART);
}

static void acadia_fixups(void)
{
	dt_fixup_memory(bd.bi_memstart, bd.bi_memsize);
	get_clocks();
	dt_fixup_mac_address_by_alias("ethernet0", bd.bi_enetaddr);
}
	
void platform_init(unsigned long r3, unsigned long r4, unsigned long r5,
		unsigned long r6, unsigned long r7)
{
	CUBOOT_INIT();
	platform_ops.fixups = acadia_fixups;
	platform_ops.exit = ibm40x_dbcr_reset;
	fdt_init(_dtb_start);
	serial_console_init();
}
