/*
 * zr36057.h - zr36057 register offsets
 *
 * Copyright (C) 1998 Dave Perks <dperks@ibm.net>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _ZR36057_H_
#define _ZR36057_H_



#define ZR36057_VFEHCR          0x000	
#define ZR36057_VFEHCR_HSPol            (1<<30)
#define ZR36057_VFEHCR_HStart           10
#define ZR36057_VFEHCR_HEnd		0
#define ZR36057_VFEHCR_Hmask		0x3ff

#define ZR36057_VFEVCR          0x004	
#define ZR36057_VFEVCR_VSPol            (1<<30)
#define ZR36057_VFEVCR_VStart           10
#define ZR36057_VFEVCR_VEnd		0
#define ZR36057_VFEVCR_Vmask		0x3ff

#define ZR36057_VFESPFR         0x008	
#define ZR36057_VFESPFR_ExtFl           (1<<26)
#define ZR36057_VFESPFR_TopField        (1<<25)
#define ZR36057_VFESPFR_VCLKPol         (1<<24)
#define ZR36057_VFESPFR_HFilter         21
#define ZR36057_VFESPFR_HorDcm          14
#define ZR36057_VFESPFR_VerDcm          8
#define ZR36057_VFESPFR_DispMode        6
#define ZR36057_VFESPFR_YUV422          (0<<3)
#define ZR36057_VFESPFR_RGB888          (1<<3)
#define ZR36057_VFESPFR_RGB565          (2<<3)
#define ZR36057_VFESPFR_RGB555          (3<<3)
#define ZR36057_VFESPFR_ErrDif          (1<<2)
#define ZR36057_VFESPFR_Pack24          (1<<1)
#define ZR36057_VFESPFR_LittleEndian    (1<<0)

#define ZR36057_VDTR            0x00c	

#define ZR36057_VDBR            0x010	

#define ZR36057_VSSFGR          0x014	
#define ZR36057_VSSFGR_DispStride       16
#define ZR36057_VSSFGR_VidOvf           (1<<8)
#define ZR36057_VSSFGR_SnapShot         (1<<1)
#define ZR36057_VSSFGR_FrameGrab        (1<<0)

#define ZR36057_VDCR            0x018	
#define ZR36057_VDCR_VidEn              (1<<31)
#define ZR36057_VDCR_MinPix             24
#define ZR36057_VDCR_Triton             (1<<24)
#define ZR36057_VDCR_VidWinHt           12
#define ZR36057_VDCR_VidWinWid          0

#define ZR36057_MMTR            0x01c	

#define ZR36057_MMBR            0x020	

#define ZR36057_OCR             0x024	
#define ZR36057_OCR_OvlEnable           (1 << 15)
#define ZR36057_OCR_MaskStride          0

#define ZR36057_SPGPPCR         0x028	
#define ZR36057_SPGPPCR_SoftReset	(1<<24)

#define ZR36057_GPPGCR1         0x02c	

#define ZR36057_MCSAR           0x030	

#define ZR36057_MCTCR           0x034	
#define ZR36057_MCTCR_CodTime           (1 << 30)
#define ZR36057_MCTCR_CEmpty            (1 << 29)
#define ZR36057_MCTCR_CFlush            (1 << 28)
#define ZR36057_MCTCR_CodGuestID	20
#define ZR36057_MCTCR_CodGuestReg	16

#define ZR36057_MCMPR           0x038	

#define ZR36057_ISR             0x03c	
#define ZR36057_ISR_GIRQ1               (1<<30)
#define ZR36057_ISR_GIRQ0               (1<<29)
#define ZR36057_ISR_CodRepIRQ           (1<<28)
#define ZR36057_ISR_JPEGRepIRQ          (1<<27)

#define ZR36057_ICR             0x040	
#define ZR36057_ICR_GIRQ1               (1<<30)
#define ZR36057_ICR_GIRQ0               (1<<29)
#define ZR36057_ICR_CodRepIRQ           (1<<28)
#define ZR36057_ICR_JPEGRepIRQ          (1<<27)
#define ZR36057_ICR_IntPinEn            (1<<24)

#define ZR36057_I2CBR           0x044	
#define ZR36057_I2CBR_SDA       	(1<<1)
#define ZR36057_I2CBR_SCL       	(1<<0)

#define ZR36057_JMC             0x100	
#define ZR36057_JMC_JPG                 (1 << 31)
#define ZR36057_JMC_JPGExpMode          (0 << 29)
#define ZR36057_JMC_JPGCmpMode          (1 << 29)
#define ZR36057_JMC_MJPGExpMode         (2 << 29)
#define ZR36057_JMC_MJPGCmpMode         (3 << 29)
#define ZR36057_JMC_RTBUSY_FB           (1 << 6)
#define ZR36057_JMC_Go_en               (1 << 5)
#define ZR36057_JMC_SyncMstr            (1 << 4)
#define ZR36057_JMC_Fld_per_buff        (1 << 3)
#define ZR36057_JMC_VFIFO_FB            (1 << 2)
#define ZR36057_JMC_CFIFO_FB            (1 << 1)
#define ZR36057_JMC_Stll_LitEndian      (1 << 0)

#define ZR36057_JPC             0x104	
#define ZR36057_JPC_P_Reset             (1 << 7)
#define ZR36057_JPC_CodTrnsEn           (1 << 5)
#define ZR36057_JPC_Active              (1 << 0)

#define ZR36057_VSP             0x108	
#define ZR36057_VSP_VsyncSize           16
#define ZR36057_VSP_FrmTot              0

#define ZR36057_HSP             0x10c	
#define ZR36057_HSP_HsyncStart          16
#define ZR36057_HSP_LineTot             0

#define ZR36057_FHAP            0x110	
#define ZR36057_FHAP_NAX                16
#define ZR36057_FHAP_PAX                0

#define ZR36057_FVAP            0x114	
#define ZR36057_FVAP_NAY                16
#define ZR36057_FVAP_PAY                0

#define ZR36057_FPP             0x118	
#define ZR36057_FPP_Odd_Even            (1 << 0)

#define ZR36057_JCBA            0x11c	

#define ZR36057_JCFT            0x120	

#define ZR36057_JCGI            0x124	
#define ZR36057_JCGI_JPEGuestID         4
#define ZR36057_JCGI_JPEGuestReg        0

#define ZR36057_GCR2            0x12c	

#define ZR36057_POR             0x200	
#define ZR36057_POR_POPen               (1<<25)
#define ZR36057_POR_POTime              (1<<24)
#define ZR36057_POR_PODir               (1<<23)

#define ZR36057_STR             0x300	

#endif
