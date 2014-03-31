/*
 * ds17287rtc.h - register definitions for the ds1728[57] RTC / CMOS RAM
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * (C) 2003 Guido Guenther <agx@sigxcpu.org>
 */
#ifndef __LINUX_DS17287RTC_H
#define __LINUX_DS17287RTC_H

#include <linux/rtc.h>			
#include <linux/mc146818rtc.h>

#define DS_REGA_DV2	0x40		
#define DS_REGA_DV1	0x20		
#define DS_REGA_DV0	0x10		

#define DS_B1_MODEL	0x40		
#define DS_B1_SN1 	0x41		
#define DS_B1_SN2 	0x42		
#define DS_B1_SN3 	0x43		
#define DS_B1_SN4 	0x44		
#define DS_B1_SN5 	0x45		
#define DS_B1_SN6 	0x46		
#define DS_B1_CRC 	0x47		
#define DS_B1_CENTURY 	0x48		
#define DS_B1_DALARM 	0x49		
#define DS_B1_XCTRL4A	0x4a		
#define DS_B1_XCTRL4B	0x4b		
#define DS_B1_RTCADDR2 	0x4e		
#define DS_B1_RTCADDR3 	0x4f		
#define DS_B1_RAMLSB	0x50		
#define DS_B1_RAMMSB	0x51		
#define DS_B1_RAMDPORT	0x53		

#define DS_XCTRL4A_VRT2	0x80 		
#define DS_XCTRL4A_INCR	0x40		
#define DS_XCTRL4A_BME	0x20		
#define DS_XCTRL4A_PAB	0x08		
#define DS_XCTRL4A_RF	0x04		
#define DS_XCTRL4A_WF	0x02		
#define DS_XCTRL4A_KF	0x01		

#define DS_XCTRL4A_IFS	(DS_XCTRL4A_RF|DS_XCTRL4A_WF|DS_XCTRL4A_KF)

#define DS_XCTRL4B_ABE	0x80 		
#define DS_XCTRL4B_E32K	0x40		
#define DS_XCTRL4B_CS	0x20		
#define DS_XCTRL4B_RCE	0x10		
#define DS_XCTRL4B_PRS	0x08		
#define DS_XCTRL4B_RIE	0x04		
#define DS_XCTRL4B_WFE	0x02		
#define DS_XCTRL4B_KFE	0x01		

#define DS_XCTRL4B_IFES	(DS_XCTRL4B_RIE|DS_XCTRL4B_WFE|DS_XCTRL4B_KFE)

#endif 
