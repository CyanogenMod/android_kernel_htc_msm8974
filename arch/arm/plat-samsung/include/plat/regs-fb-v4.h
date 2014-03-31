/* arch/arm/plat-samsung/include/plat/regs-fb-v4.h
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *      http://armlinux.simtec.co.uk/
 *      Ben Dooks <ben@simtec.co.uk>
 *
 * S3C64XX - new-style framebuffer register definitions
 *
 * This is the register set for the new style framebuffer interface
 * found from the S3C2443 onwards and specifically the S3C64XX series
 * S3C6400 and S3C6410.
 *
 * The file contains the cpu specific items which change between whichever
 * architecture is selected. See <plat/regs-fb.h> for the core definitions
 * that are the same.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/


#include <plat/regs-fb.h>

#define S3C_FB_MAX_WIN (5)  
#define VIDCON1_FSTATUS_EVEN	(1 << 15)

#define VIDTCON0				(0x10)
#define VIDTCON1				(0x14)
#define VIDTCON2				(0x18)


#define WINCON(_win)				(0x20 + ((_win) * 4))


#define VIDOSD_BASE				(0x40)

#define VIDINTCON0				(0x130)


#define WINCONx_CSCWIDTH_MASK			(0x3 << 26)
#define WINCONx_CSCWIDTH_SHIFT			(26)
#define WINCONx_CSCWIDTH_WIDE			(0x0 << 26)
#define WINCONx_CSCWIDTH_NARROW			(0x3 << 26)

#define WINCONx_ENLOCAL				(1 << 22)
#define WINCONx_BUFSTATUS			(1 << 21)
#define WINCONx_BUFSEL				(1 << 20)
#define WINCONx_BUFAUTOEN			(1 << 19)
#define WINCONx_YCbCr				(1 << 13)

#define WINCON1_LOCALSEL_CAMIF			(1 << 23)

#define WINCON2_LOCALSEL_CAMIF			(1 << 23)
#define WINCON2_BLD_PIX				(1 << 6)

#define WINCON2_ALPHA_SEL			(1 << 1)
#define WINCON2_BPPMODE_MASK			(0xf << 2)
#define WINCON2_BPPMODE_SHIFT			(2)
#define WINCON2_BPPMODE_1BPP			(0x0 << 2)
#define WINCON2_BPPMODE_2BPP			(0x1 << 2)
#define WINCON2_BPPMODE_4BPP			(0x2 << 2)
#define WINCON2_BPPMODE_8BPP_1232		(0x4 << 2)
#define WINCON2_BPPMODE_16BPP_565		(0x5 << 2)
#define WINCON2_BPPMODE_16BPP_A1555		(0x6 << 2)
#define WINCON2_BPPMODE_16BPP_I1555		(0x7 << 2)
#define WINCON2_BPPMODE_18BPP_666		(0x8 << 2)
#define WINCON2_BPPMODE_18BPP_A1665		(0x9 << 2)
#define WINCON2_BPPMODE_19BPP_A1666		(0xa << 2)
#define WINCON2_BPPMODE_24BPP_888		(0xb << 2)
#define WINCON2_BPPMODE_24BPP_A1887		(0xc << 2)
#define WINCON2_BPPMODE_25BPP_A1888		(0xd << 2)
#define WINCON2_BPPMODE_28BPP_A4888		(0xd << 2)

#define WINCON3_BLD_PIX				(1 << 6)

#define WINCON3_ALPHA_SEL			(1 << 1)
#define WINCON3_BPPMODE_MASK			(0xf << 2)
#define WINCON3_BPPMODE_SHIFT			(2)
#define WINCON3_BPPMODE_1BPP			(0x0 << 2)
#define WINCON3_BPPMODE_2BPP			(0x1 << 2)
#define WINCON3_BPPMODE_4BPP			(0x2 << 2)
#define WINCON3_BPPMODE_16BPP_565		(0x5 << 2)
#define WINCON3_BPPMODE_16BPP_A1555		(0x6 << 2)
#define WINCON3_BPPMODE_16BPP_I1555		(0x7 << 2)
#define WINCON3_BPPMODE_18BPP_666		(0x8 << 2)
#define WINCON3_BPPMODE_18BPP_A1665		(0x9 << 2)
#define WINCON3_BPPMODE_19BPP_A1666		(0xa << 2)
#define WINCON3_BPPMODE_24BPP_888		(0xb << 2)
#define WINCON3_BPPMODE_24BPP_A1887		(0xc << 2)
#define WINCON3_BPPMODE_25BPP_A1888		(0xd << 2)
#define WINCON3_BPPMODE_28BPP_A4888		(0xd << 2)

#define VIDINTCON0_FIFIOSEL_WINDOW2		(0x10 << 5)
#define VIDINTCON0_FIFIOSEL_WINDOW3		(0x20 << 5)
#define VIDINTCON0_FIFIOSEL_WINDOW4		(0x40 << 5)

#define DITHMODE				(0x170)
#define WINxMAP(_win)				(0x180 + ((_win) * 4))


#define DITHMODE_R_POS_MASK			(0x3 << 5)
#define DITHMODE_R_POS_SHIFT			(5)
#define DITHMODE_R_POS_8BIT			(0x0 << 5)
#define DITHMODE_R_POS_6BIT			(0x1 << 5)
#define DITHMODE_R_POS_5BIT			(0x2 << 5)

#define DITHMODE_G_POS_MASK			(0x3 << 3)
#define DITHMODE_G_POS_SHIFT			(3)
#define DITHMODE_G_POS_8BIT			(0x0 << 3)
#define DITHMODE_G_POS_6BIT			(0x1 << 3)
#define DITHMODE_G_POS_5BIT			(0x2 << 3)

#define DITHMODE_B_POS_MASK			(0x3 << 1)
#define DITHMODE_B_POS_SHIFT			(1)
#define DITHMODE_B_POS_8BIT			(0x0 << 1)
#define DITHMODE_B_POS_6BIT			(0x1 << 1)
#define DITHMODE_B_POS_5BIT			(0x2 << 1)

#define DITHMODE_DITH_EN			(1 << 0)

#define WPALCON					(0x1A0)

#define WPALCON_W4PAL_16BPP_A555		(1 << 8)
#define WPALCON_W3PAL_16BPP_A555		(1 << 7)
#define WPALCON_W2PAL_16BPP_A555		(1 << 6)


