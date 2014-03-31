/*
 * Copyright (c) 2010 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef	_BRCM_DEFS_H_
#define	_BRCM_DEFS_H_

#include <linux/types.h>

#define	SI_BUS			0
#define	PCI_BUS			1
#define	PCMCIA_BUS		2
#define SDIO_BUS		3
#define JTAG_BUS		4
#define USB_BUS			5
#define SPI_BUS			6

#define	OFF	0
#define	ON	1		
#define	AUTO	(-1)		

#define	PRIO_8021D_NONE		2
#define	PRIO_8021D_BK		1
#define	PRIO_8021D_BE		0
#define	PRIO_8021D_EE		3
#define	PRIO_8021D_CL		4
#define	PRIO_8021D_VI		5
#define	PRIO_8021D_VO		6
#define	PRIO_8021D_NC		7

#define	MAXPRIO			7
#define NUMPRIO			(MAXPRIO + 1)

#define WL_NUMRATES		16	

#define BRCM_CNTRY_BUF_SZ	4	

#define BRCM_SET_CHANNEL	30
#define BRCM_SET_SRL		32
#define BRCM_SET_LRL		34
#define BRCM_SET_BCNPRD		76

#define BRCM_GET_CURR_RATESET	114	
#define BRCM_GET_PHYLIST	180


#define WL_RADIO_SW_DISABLE		(1<<0)
#define WL_RADIO_HW_DISABLE		(1<<1)
#define WL_RADIO_COUNTRY_DISABLE	(1<<3)

#define WL_TXPWR_OVERRIDE	(1U<<31)

#define	BRCM_BAND_AUTO		0	
#define	BRCM_BAND_5G		1	
#define	BRCM_BAND_2G		2	
#define	BRCM_BAND_ALL		3	

#define PM_OFF	0
#define PM_MAX	1

#define LOG_ERROR_VAL		0x00000001
#define LOG_TRACE_VAL		0x00000002

#define PM_OFF	0
#define PM_MAX	1
#define PM_FAST 2


#define	SBCONFIGOFF		0xf00

#ifndef	PAD
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)
#endif

#endif				
