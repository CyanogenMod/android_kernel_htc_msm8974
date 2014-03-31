/*
 * comparator.h  --  Comparator Aux ADC for Wolfson WM8350 PMIC
 *
 * Copyright 2007 Wolfson Microelectronics PLC
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#ifndef __LINUX_MFD_WM8350_COMPARATOR_H_
#define __LINUX_MFD_WM8350_COMPARATOR_H_


#define WM8350_DIGITISER_CONTROL_1              0x90
#define WM8350_DIGITISER_CONTROL_2              0x91
#define WM8350_AUX1_READBACK                    0x98
#define WM8350_AUX2_READBACK                    0x99
#define WM8350_AUX3_READBACK                    0x9A
#define WM8350_AUX4_READBACK                    0x9B
#define WM8350_CHIP_TEMP_READBACK               0x9F
#define WM8350_GENERIC_COMPARATOR_CONTROL       0xA3
#define WM8350_GENERIC_COMPARATOR_1             0xA4
#define WM8350_GENERIC_COMPARATOR_2             0xA5
#define WM8350_GENERIC_COMPARATOR_3             0xA6
#define WM8350_GENERIC_COMPARATOR_4             0xA7

#define WM8350_AUXADC_CTC                       0x4000
#define WM8350_AUXADC_POLL                      0x2000
#define WM8350_AUXADC_HIB_MODE                  0x1000
#define WM8350_AUXADC_SEL8                      0x0080
#define WM8350_AUXADC_SEL7                      0x0040
#define WM8350_AUXADC_SEL6                      0x0020
#define WM8350_AUXADC_SEL5                      0x0010
#define WM8350_AUXADC_SEL4                      0x0008
#define WM8350_AUXADC_SEL3                      0x0004
#define WM8350_AUXADC_SEL2                      0x0002
#define WM8350_AUXADC_SEL1                      0x0001

#define WM8350_AUXADC_MASKMODE_MASK             0x3000
#define WM8350_AUXADC_CRATE_MASK                0x0700
#define WM8350_AUXADC_CAL                       0x0004
#define WM8350_AUX_RBMODE                       0x0002
#define WM8350_AUXADC_WAIT                      0x0001

#define WM8350_AUXADC_SCALE1_MASK               0x6000
#define WM8350_AUXADC_REF1                      0x1000
#define WM8350_AUXADC_DATA1_MASK                0x0FFF

#define WM8350_AUXADC_SCALE2_MASK               0x6000
#define WM8350_AUXADC_REF2                      0x1000
#define WM8350_AUXADC_DATA2_MASK                0x0FFF

#define WM8350_AUXADC_SCALE3_MASK               0x6000
#define WM8350_AUXADC_REF3                      0x1000
#define WM8350_AUXADC_DATA3_MASK                0x0FFF

#define WM8350_AUXADC_SCALE4_MASK               0x6000
#define WM8350_AUXADC_REF4                      0x1000
#define WM8350_AUXADC_DATA4_MASK                0x0FFF

#define WM8350_AUXADC_DATA_USB_MASK             0x0FFF

#define WM8350_AUXADC_DATA_LINE_MASK            0x0FFF

#define WM8350_AUXADC_DATA_BATT_MASK            0x0FFF

#define WM8350_AUXADC_DATA_CHIPTEMP_MASK        0x0FFF

#define WM8350_DCMP4_ENA                        0x0008
#define WM8350_DCMP3_ENA                        0x0004
#define WM8350_DCMP2_ENA                        0x0002
#define WM8350_DCMP1_ENA                        0x0001

#define WM8350_DCMP1_SRCSEL_MASK                0xE000
#define WM8350_DCMP1_GT                         0x1000
#define WM8350_DCMP1_THR_MASK                   0x0FFF

#define WM8350_DCMP2_SRCSEL_MASK                0xE000
#define WM8350_DCMP2_GT                         0x1000
#define WM8350_DCMP2_THR_MASK                   0x0FFF

#define WM8350_DCMP3_SRCSEL_MASK                0xE000
#define WM8350_DCMP3_GT                         0x1000
#define WM8350_DCMP3_THR_MASK                   0x0FFF

#define WM8350_DCMP4_SRCSEL_MASK                0xE000
#define WM8350_DCMP4_GT                         0x1000
#define WM8350_DCMP4_THR_MASK                   0x0FFF

#define WM8350_IRQ_AUXADC_DATARDY		16
#define WM8350_IRQ_AUXADC_DCOMP4		17
#define WM8350_IRQ_AUXADC_DCOMP3		18
#define WM8350_IRQ_AUXADC_DCOMP2		19
#define WM8350_IRQ_AUXADC_DCOMP1		20
#define WM8350_IRQ_SYS_HYST_COMP_FAIL		21
#define WM8350_IRQ_SYS_CHIP_GT115		22
#define WM8350_IRQ_SYS_CHIP_GT140		23

#define WM8350_AUX_COEFF			1319

#define WM8350_AUXADC_AUX1			0
#define WM8350_AUXADC_AUX2			1
#define WM8350_AUXADC_AUX3			2
#define WM8350_AUXADC_AUX4			3
#define WM8350_AUXADC_USB			4
#define WM8350_AUXADC_LINE			5
#define WM8350_AUXADC_BATT			6
#define WM8350_AUXADC_TEMP			7

struct wm8350;

int wm8350_read_auxadc(struct wm8350 *wm8350, int channel, int scale,
		       int vref);

#endif
