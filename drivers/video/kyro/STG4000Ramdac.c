/*
 *  linux/drivers/video/kyro/STG4000Ramdac.c
 *
 *  Copyright (C) 2002 STMicroelectronics
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <video/kyro.h>

#include "STG4000Reg.h"
#include "STG4000Interface.h"

static u32 STG_PIXEL_BUS_WIDTH = 128;	
static u32 REF_CLOCK = 14318;

int InitialiseRamdac(volatile STG4000REG __iomem * pSTGReg,
		     u32 displayDepth,
		     u32 displayWidth,
		     u32 displayHeight,
		     s32 HSyncPolarity,
		     s32 VSyncPolarity, u32 * pixelClock)
{
	u32 tmp = 0;
	u32 F = 0, R = 0, P = 0;
	u32 stride = 0;
	u32 ulPdiv = 0;
	u32 physicalPixelDepth = 0;
	
	tmp = STG_READ_REG(SoftwareReset);

	if (tmp & 0x1) {
		CLEAR_BIT(1);
		STG_WRITE_REG(SoftwareReset, tmp);
	}

	
	tmp = STG_READ_REG(DACPixelFormat);
	CLEAR_BITS_FRM_TO(0, 2);

	
	CLEAR_BITS_FRM_TO(8, 9);

	switch (displayDepth) {
	case 16:
		{
			physicalPixelDepth = 16;
			tmp |= _16BPP;
			break;
		}
	case 32:
		{
			
			physicalPixelDepth = 32;
			tmp |= _32BPP;
			break;
		}
	default:
		return -EINVAL;
	}

	STG_WRITE_REG(DACPixelFormat, tmp);

	
	ulPdiv = STG_PIXEL_BUS_WIDTH / physicalPixelDepth;

	
	stride = displayWidth;

	
	tmp = STG_READ_REG(DACPrimSize);
	CLEAR_BITS_FRM_TO(0, 10);
	CLEAR_BITS_FRM_TO(12, 31);
	tmp |=
	    ((((displayHeight - 1) << 12) | (((displayWidth / ulPdiv) -
					      1) << 23))
	     | (stride / ulPdiv));
	STG_WRITE_REG(DACPrimSize, tmp);


	
	*pixelClock = ProgramClock(REF_CLOCK, *pixelClock, &F, &R, &P);

	
	tmp = STG_READ_REG(DACPLLMode);
	CLEAR_BITS_FRM_TO(0, 15);
	
	tmp |= ((P) | ((F - 2) << 2) | ((R - 2) << 11));
	STG_WRITE_REG(DACPLLMode, tmp);

	
	tmp = STG_READ_REG(DACPrimAddress);
	CLEAR_BITS_FRM_TO(0, 20);
	CLEAR_BITS_FRM_TO(20, 31);
	STG_WRITE_REG(DACPrimAddress, tmp);

	
	tmp = STG_READ_REG(DACCursorCtrl);
	tmp &= ~SET_BIT(31);
	STG_WRITE_REG(DACCursorCtrl, tmp);

	tmp = STG_READ_REG(DACCursorAddr);
	CLEAR_BITS_FRM_TO(0, 20);
	STG_WRITE_REG(DACCursorAddr, tmp);

	
	tmp = STG_READ_REG(DACVidWinStart);
	CLEAR_BITS_FRM_TO(0, 10);
	CLEAR_BITS_FRM_TO(16, 26);
	STG_WRITE_REG(DACVidWinStart, tmp);

	tmp = STG_READ_REG(DACVidWinEnd);
	CLEAR_BITS_FRM_TO(0, 10);
	CLEAR_BITS_FRM_TO(16, 26);
	STG_WRITE_REG(DACVidWinEnd, tmp);

	
	tmp = STG_READ_REG(DACBorderColor);
	CLEAR_BITS_FRM_TO(0, 23);
	STG_WRITE_REG(DACBorderColor, tmp);

	
	STG_WRITE_REG(DACBurstCtrl, 0x0404);

	
	tmp = STG_READ_REG(DACCrcTrigger);
	CLEAR_BIT(0);
	STG_WRITE_REG(DACCrcTrigger, tmp);

	
	tmp = STG_READ_REG(DigVidPortCtrl);
	CLEAR_BIT(8);
	CLEAR_BITS_FRM_TO(16, 27);
	CLEAR_BITS_FRM_TO(1, 3);
	CLEAR_BITS_FRM_TO(10, 11);
	STG_WRITE_REG(DigVidPortCtrl, tmp);

	return 0;
}

void DisableRamdacOutput(volatile STG4000REG __iomem * pSTGReg)
{
	u32 tmp;

	
	tmp = (STG_READ_REG(DACStreamCtrl)) & ~SET_BIT(0);
	STG_WRITE_REG(DACStreamCtrl, tmp);
}

void EnableRamdacOutput(volatile STG4000REG __iomem * pSTGReg)
{
	u32 tmp;

	
	tmp = (STG_READ_REG(DACStreamCtrl)) | SET_BIT(0);
	STG_WRITE_REG(DACStreamCtrl, tmp);
}
