/*
 * Copyright (C) 2009 Texas Instruments Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * vpss - video processing subsystem module header file.
 *
 * Include this header file if a driver needs to configure vpss system
 * module. It exports a set of library functions  for video drivers to
 * configure vpss system module functions such as clock enable/disable,
 * vpss interrupt mux to arm, and other common vpss system module
 * functions.
 */
#ifndef _VPSS_H
#define _VPSS_H

enum vpss_ccdc_source_sel {
	VPSS_CCDCIN,
	VPSS_HSSIIN,
	VPSS_PGLPBK,	
	VPSS_CCDCPG	
};

struct vpss_sync_pol {
	unsigned int ccdpg_hdpol:1;
	unsigned int ccdpg_vdpol:1;
};

struct vpss_pg_frame_size {
	short hlpfr;
	short pplen;
};

enum vpss_clock_sel {
	
	VPSS_CCDC_CLOCK,
	VPSS_IPIPE_CLOCK,
	VPSS_H3A_CLOCK,
	VPSS_CFALD_CLOCK,
	VPSS_VENC_CLOCK_SEL,
	VPSS_VPBE_CLOCK,
	
	VPSS_IPIPEIF_CLOCK,
	VPSS_RSZ_CLOCK,
	VPSS_BL_CLOCK,
	VPSS_PCLK_INTERNAL,
	VPSS_PSYNC_CLOCK_SEL,
	VPSS_LDC_CLOCK_SEL,
	VPSS_OSD_CLOCK_SEL,
	VPSS_FDIF_CLOCK,
	VPSS_LDC_CLOCK
};

int vpss_select_ccdc_source(enum vpss_ccdc_source_sel src_sel);
int vpss_enable_clock(enum vpss_clock_sel clock_sel, int en);
void dm365_vpss_set_sync_pol(struct vpss_sync_pol);
void dm365_vpss_set_pg_frame_size(struct vpss_pg_frame_size);

enum vpss_wbl_sel {
	VPSS_PCR_AEW_WBL_0 = 16,
	VPSS_PCR_AF_WBL_0,
	VPSS_PCR_RSZ4_WBL_0,
	VPSS_PCR_RSZ3_WBL_0,
	VPSS_PCR_RSZ2_WBL_0,
	VPSS_PCR_RSZ1_WBL_0,
	VPSS_PCR_PREV_WBL_0,
	VPSS_PCR_CCDC_WBL_O,
};
int vpss_clear_wbl_overflow(enum vpss_wbl_sel wbl_sel);
#endif
