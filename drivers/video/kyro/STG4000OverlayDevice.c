/*
 *  linux/drivers/video/kyro/STG4000OverlayDevice.c
 *
 *  Copyright (C) 2000 Imagination Technologies Ltd
 *  Copyright (C) 2002 STMicroelectronics
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>

#include "STG4000Reg.h"
#include "STG4000Interface.h"


#define STG4000_NO_SCALING    0x800
#define STG4000_NO_DECIMATION 0xFFFFFFFF

#define STG4000_PRIM_NUM_PIX   5
#define STG4000_PRIM_ALIGN     4
#define STG4000_PRIM_ADDR_BITS 20

#define STG4000_PRIM_MIN_WIDTH  640
#define STG4000_PRIM_MAX_WIDTH  1600
#define STG4000_PRIM_MIN_HEIGHT 480
#define STG4000_PRIM_MAX_HEIGHT 1200

#define STG4000_OVRL_NUM_PIX   4
#define STG4000_OVRL_ALIGN     2
#define STG4000_OVRL_ADDR_BITS 20
#define STG4000_OVRL_NUM_MODES 5

#define STG4000_OVRL_MIN_WIDTH  0
#define STG4000_OVRL_MAX_WIDTH  720
#define STG4000_OVRL_MIN_HEIGHT 0
#define STG4000_OVRL_MAX_HEIGHT 576

static u32 adwDecim8[33] = {
	    0xffffffff, 0xfffeffff, 0xffdffbff, 0xfefefeff, 0xfdf7efbf,
	    0xfbdf7bdf, 0xf7bbddef, 0xeeeeeeef, 0xeeddbb77, 0xedb76db7,
	    0xdb6db6db, 0xdb5b5b5b, 0xdab5ad6b, 0xd5ab55ab, 0xd555aaab,
	    0xaaaaaaab, 0xaaaa5555, 0xaa952a55, 0xa94a5295, 0xa5252525,
	    0xa4924925, 0x92491249, 0x91224489, 0x91111111, 0x90884211,
	    0x88410821, 0x88102041, 0x81010101, 0x80800801, 0x80010001,
	    0x80000001, 0x00000001, 0x00000000
};

typedef struct _OVRL_SRC_DEST {
	
	u32 ulDstX1;
	u32 ulDstY1;
	u32 ulDstX2;
	u32 ulDstY2;

	
	u32 ulSrcX1;
	u32 ulSrcY1;
	u32 ulSrcX2;
	u32 ulSrcY2;

	
	s32 lDstX1;
	s32 lDstY1;
	s32 lDstX2;
	s32 lDstY2;
} OVRL_SRC_DEST;

static u32 ovlWidth, ovlHeight, ovlStride;
static int ovlLinear;

void ResetOverlayRegisters(volatile STG4000REG __iomem *pSTGReg)
{
	u32 tmp;

	
	tmp = STG_READ_REG(DACOverlayAddr);
	CLEAR_BITS_FRM_TO(0, 20);
	CLEAR_BIT(31);
	STG_WRITE_REG(DACOverlayAddr, tmp);

	
	tmp = STG_READ_REG(DACOverlayUAddr);
	CLEAR_BITS_FRM_TO(0, 20);
	STG_WRITE_REG(DACOverlayUAddr, tmp);

	
	tmp = STG_READ_REG(DACOverlayVAddr);
	CLEAR_BITS_FRM_TO(0, 20);
	STG_WRITE_REG(DACOverlayVAddr, tmp);

	
	tmp = STG_READ_REG(DACOverlaySize);
	CLEAR_BITS_FRM_TO(0, 10);
	CLEAR_BITS_FRM_TO(12, 31);
	STG_WRITE_REG(DACOverlaySize, tmp);

	
	tmp = STG4000_NO_DECIMATION;
	STG_WRITE_REG(DACOverlayVtDec, tmp);

	
	tmp = STG_READ_REG(DACPixelFormat);
	CLEAR_BITS_FRM_TO(4, 7);
	CLEAR_BITS_FRM_TO(16, 22);
	STG_WRITE_REG(DACPixelFormat, tmp);

	
	tmp = STG_READ_REG(DACVerticalScal);
	CLEAR_BITS_FRM_TO(0, 11);
	CLEAR_BITS_FRM_TO(16, 22);
	tmp |= STG4000_NO_SCALING;	
	STG_WRITE_REG(DACVerticalScal, tmp);

	
	tmp = STG_READ_REG(DACHorizontalScal);
	CLEAR_BITS_FRM_TO(0, 11);
	CLEAR_BITS_FRM_TO(16, 17);
	tmp |= STG4000_NO_SCALING;	
	STG_WRITE_REG(DACHorizontalScal, tmp);

	
	tmp = STG_READ_REG(DACBlendCtrl);
	CLEAR_BITS_FRM_TO(0, 30);
	tmp = (GRAPHICS_MODE << 28);
	STG_WRITE_REG(DACBlendCtrl, tmp);

}

int CreateOverlaySurface(volatile STG4000REG __iomem *pSTGReg,
			 u32 inWidth,
			 u32 inHeight,
			 int bLinear,
			 u32 ulOverlayOffset,
			 u32 * retStride, u32 * retUVStride)
{
	u32 tmp;
	u32 ulStride;

	if (inWidth > STG4000_OVRL_MAX_WIDTH ||
	    inHeight > STG4000_OVRL_MAX_HEIGHT) {
		return -EINVAL;
	}

	
	if (bLinear) {
		
		if ((inWidth & 0x7) == 0) {	
			ulStride = (inWidth / 8);
		} else {
			
			ulStride = ((inWidth + 8) / 8);
		}
	} else {
		
		if ((inWidth & 0xf) == 0) {	
			ulStride = (inWidth / 16);
		} else {
			
			ulStride = ((inWidth + 16) / 16);
		}
	}


	
	tmp = STG_READ_REG(DACOverlayAddr);
	CLEAR_BITS_FRM_TO(0, 20);
	if (bLinear) {
		CLEAR_BIT(31);	
	} else {
		tmp |= SET_BIT(31);	
	}

	
	tmp |= (ulOverlayOffset >> 4);
	STG_WRITE_REG(DACOverlayAddr, tmp);

	if (!bLinear) {
		u32 uvSize =
		    (inWidth & 0x1) ? (inWidth + 1 / 2) : (inWidth / 2);
		u32 uvStride;
		u32 ulOffset;
		
		if ((uvSize & 0xf) == 0) {	
			uvStride = (uvSize / 16);
		} else {
			
			uvStride = ((uvSize + 16) / 16);
		}

		ulOffset = ulOverlayOffset + (inHeight * (ulStride * 16));
		
		if ((ulOffset & 0x1f) != 0)
			ulOffset = (ulOffset + 32L) & 0xffffffE0L;

		tmp = STG_READ_REG(DACOverlayUAddr);
		CLEAR_BITS_FRM_TO(0, 20);
		tmp |= (ulOffset >> 4);
		STG_WRITE_REG(DACOverlayUAddr, tmp);

		ulOffset += (inHeight / 2) * (uvStride * 16);
		
		if ((ulOffset & 0x1f) != 0)
			ulOffset = (ulOffset + 32L) & 0xffffffE0L;

		tmp = STG_READ_REG(DACOverlayVAddr);
		CLEAR_BITS_FRM_TO(0, 20);
		tmp |= (ulOffset >> 4);
		STG_WRITE_REG(DACOverlayVAddr, tmp);

		*retUVStride = uvStride * 16;
	}


	tmp = STG_READ_REG(DACPixelFormat);
	
	CLEAR_BITS_FRM_TO(4, 9);
	STG_WRITE_REG(DACPixelFormat, tmp);

	ovlWidth = inWidth;
	ovlHeight = inHeight;
	ovlStride = ulStride;
	ovlLinear = bLinear;
	*retStride = ulStride << 4;	

	return 0;
}

int SetOverlayBlendMode(volatile STG4000REG __iomem *pSTGReg,
			OVRL_BLEND_MODE mode,
			u32 ulAlpha, u32 ulColorKey)
{
	u32 tmp;

	tmp = STG_READ_REG(DACBlendCtrl);
	CLEAR_BITS_FRM_TO(28, 30);
	tmp |= (mode << 28);

	switch (mode) {
	case COLOR_KEY:
		CLEAR_BITS_FRM_TO(0, 23);
		tmp |= (ulColorKey & 0x00FFFFFF);
		break;

	case GLOBAL_ALPHA:
		CLEAR_BITS_FRM_TO(24, 27);
		tmp |= ((ulAlpha & 0xF) << 24);
		break;

	case CK_PIXEL_ALPHA:
		CLEAR_BITS_FRM_TO(0, 23);
		tmp |= (ulColorKey & 0x00FFFFFF);
		break;

	case CK_GLOBAL_ALPHA:
		CLEAR_BITS_FRM_TO(0, 23);
		tmp |= (ulColorKey & 0x00FFFFFF);
		CLEAR_BITS_FRM_TO(24, 27);
		tmp |= ((ulAlpha & 0xF) << 24);
		break;

	case GRAPHICS_MODE:
	case PER_PIXEL_ALPHA:
		break;

	default:
		return -EINVAL;
	}

	STG_WRITE_REG(DACBlendCtrl, tmp);

	return 0;
}

void EnableOverlayPlane(volatile STG4000REG __iomem *pSTGReg)
{
	u32 tmp;
	
	tmp = STG_READ_REG(DACPixelFormat);
	tmp |= SET_BIT(7);
	STG_WRITE_REG(DACPixelFormat, tmp);

	
	tmp = STG_READ_REG(DACStreamCtrl);
	tmp |= SET_BIT(1);	
	STG_WRITE_REG(DACStreamCtrl, tmp);
}

static u32 Overlap(u32 ulBits, u32 ulPattern)
{
	u32 ulCount = 0;

	while (ulBits) {
		if (!(ulPattern & 1))
			ulCount++;
		ulBits--;
		ulPattern = ulPattern >> 1;
	}

	return ulCount;

}

int SetOverlayViewPort(volatile STG4000REG __iomem *pSTGReg,
		       u32 left, u32 top,
		       u32 right, u32 bottom)
{
	OVRL_SRC_DEST srcDest;

	u32 ulSrcTop, ulSrcBottom;
	u32 ulSrc, ulDest;
	u32 ulFxScale, ulFxOffset;
	u32 ulHeight, ulWidth;
	u32 ulPattern;
	u32 ulDecimate, ulDecimated;
	u32 ulApplied;
	u32 ulDacXScale, ulDacYScale;
	u32 ulScale;
	u32 ulLeft, ulRight;
	u32 ulSrcLeft, ulSrcRight;
	u32 ulScaleLeft, ulScaleRight;
	u32 ulhDecim;
	u32 ulsVal;
	u32 ulVertDecFactor;
	int bResult;
	u32 ulClipOff = 0;
	u32 ulBits = 0;
	u32 ulsAdd = 0;
	u32 tmp, ulStride;
	u32 ulExcessPixels, ulClip, ulExtraLines;


	srcDest.ulSrcX1 = 0;
	srcDest.ulSrcY1 = 0;
	srcDest.ulSrcX2 = ovlWidth - 1;
	srcDest.ulSrcY2 = ovlHeight - 1;

	srcDest.ulDstX1 = left;
	srcDest.ulDstY1 = top;
	srcDest.ulDstX2 = right;
	srcDest.ulDstY2 = bottom;

	srcDest.lDstX1 = srcDest.ulDstX1;
	srcDest.lDstY1 = srcDest.ulDstY1;
	srcDest.lDstX2 = srcDest.ulDstX2;
	srcDest.lDstY2 = srcDest.ulDstY2;

    

	
	ulSrcTop = srcDest.ulSrcY1;
	ulSrcBottom = srcDest.ulSrcY2;

	ulSrc = ulSrcBottom - ulSrcTop;
	ulDest = srcDest.lDstY2 - srcDest.lDstY1;	

	if (ulSrc <= 1)
		return -EINVAL;

	ulFxScale = (ulDest << 11) / ulSrc;	
	ulFxOffset = (srcDest.lDstY2 - srcDest.ulDstY2) << 11;

	ulSrcBottom = ulSrcBottom - (ulFxOffset / ulFxScale);
	ulSrc = ulSrcBottom - ulSrcTop;
	ulHeight = ulSrc;

	ulDest = srcDest.ulDstY2 - (srcDest.ulDstY1 - 1);
	ulPattern = adwDecim8[ulBits];

	
	if (ulSrc > ulDest) {
		ulDecimate = ulSrc - ulDest;
		ulBits = 0;
		ulApplied = ulSrc / 32;

		while (((ulBits * ulApplied) +
			Overlap((ulSrc % 32),
				adwDecim8[ulBits])) < ulDecimate)
			ulBits++;

		ulPattern = adwDecim8[ulBits];
		ulDecimated =
		    (ulBits * ulApplied) + Overlap((ulSrc % 32),
						   ulPattern);
		ulSrc = ulSrc - ulDecimated;	
	}

	if (ulBits && (ulBits != 32)) {
		ulVertDecFactor = (63 - ulBits) / (32 - ulBits);	
	} else {
		ulVertDecFactor = 1;
	}

	ulDacYScale = ((ulSrc - 1) * 2048) / (ulDest + 1);

	tmp = STG_READ_REG(DACOverlayVtDec);	
	CLEAR_BITS_FRM_TO(0, 31);
	tmp = ulPattern;
	STG_WRITE_REG(DACOverlayVtDec, tmp);

	

	ulSrc = srcDest.ulSrcX2 - srcDest.ulSrcX1;
	ulDest = srcDest.lDstX2 - srcDest.lDstX1;
#ifdef _OLDCODE
	ulLeft = srcDest.ulDstX1;
	ulRight = srcDest.ulDstX2;
#else
	if (srcDest.ulDstX1 > 2) {
		ulLeft = srcDest.ulDstX1 + 2;
		ulRight = srcDest.ulDstX2 + 1;
	} else {
		ulLeft = srcDest.ulDstX1;
		ulRight = srcDest.ulDstX2 + 1;
	}
#endif
	
	bResult = 1;

	do {
		if (ulDest == 0)
			return -EINVAL;

		
		ulFxScale = ((ulSrc - 1) << 11) / (ulDest);

		
		ulFxOffset = ulFxScale * ((srcDest.ulDstX1 - srcDest.lDstX1) + ulClipOff);
		ulFxOffset >>= 11;

		
		ulSrcLeft = srcDest.ulSrcX1 + ulFxOffset;

		
		ulFxOffset = ulFxScale * (srcDest.lDstX2 - srcDest.ulDstX2);
		ulFxOffset >>= 11;

		ulSrcRight = srcDest.ulSrcX2 - ulFxOffset;

		ulScaleLeft = ulSrcLeft;
		ulScaleRight = ulSrcRight;

		
		ulhDecim = 0;
		ulScale = (((ulSrcRight - ulSrcLeft) - 1) << (11 - ulhDecim)) / (ulRight - ulLeft + 2);

		while (ulScale > 0x800) {
			ulhDecim++;
			ulScale = (((ulSrcRight - ulSrcLeft) - 1) << (11 - ulhDecim)) / (ulRight - ulLeft + 2);
		}

		if (!ovlLinear) {
			ulSrcLeft &= ~0x1f;

			ulSrcRight = (ulSrcRight + 0x1f) & ~0x1f;
		} else {
			ulSrcLeft &= ~0x7;

			ulSrcRight = (ulSrcRight + 0x7) & ~0x7;
		}

		
		ulWidth = ulSrcRight - ulSrcLeft;

		ulsVal = ((ulWidth / 8) >> ulhDecim);

		if ((ulWidth != (ulsVal << ulhDecim) * 8))
			ulsAdd = 1;

		
		ulSrc = ulWidth >> ulhDecim;

		if (ulSrc <= 2)
			return -EINVAL;

		ulExcessPixels = ((((ulScaleLeft - ulSrcLeft)) << (11 - ulhDecim)) / ulScale);

		ulClip = (ulSrc << 11) / ulScale;
		ulClip -= (ulRight - ulLeft);
		ulClip += ulExcessPixels;

		if (ulClip)
			ulClip--;

		
	} while (!bResult);

	ulExtraLines = (1 << ulhDecim) * ulVertDecFactor;
	ulExtraLines += 64;
	ulHeight += ulExtraLines;

	ulDacXScale = ulScale;


	tmp = STG_READ_REG(DACVerticalScal);
	CLEAR_BITS_FRM_TO(0, 11);
	CLEAR_BITS_FRM_TO(16, 22);	

	ulStride = (ulWidth >> (ulhDecim + 3)) + ulsAdd;
	tmp |= ((ulStride << 16) | (ulDacYScale));	
	STG_WRITE_REG(DACVerticalScal, tmp);

	tmp = STG_READ_REG(DACOverlaySize);
	CLEAR_BITS_FRM_TO(0, 10);
	CLEAR_BITS_FRM_TO(12, 31);

	if (ovlLinear) {
		tmp |=
		    (ovlStride | ((ulHeight + 1) << 12) |
		     (((ulWidth / 8) - 1) << 23));
	} else {
		tmp |=
		    (ovlStride | ((ulHeight + 1) << 12) |
		     (((ulWidth / 32) - 1) << 23));
	}

	STG_WRITE_REG(DACOverlaySize, tmp);

	
	tmp = ((ulLeft << 16)) | (srcDest.ulDstY1);
	STG_WRITE_REG(DACVidWinStart, tmp);

	
	tmp = ((ulRight) << 16) | (srcDest.ulDstY2);
	STG_WRITE_REG(DACVidWinEnd, tmp);

	tmp = STG_READ_REG(DACPixelFormat);
	tmp = ((ulExcessPixels << 16) | tmp) & 0x7fffffff;
	STG_WRITE_REG(DACPixelFormat, tmp);

	tmp = STG_READ_REG(DACHorizontalScal);
	CLEAR_BITS_FRM_TO(0, 11);
	CLEAR_BITS_FRM_TO(16, 17);
	tmp |= ((ulhDecim << 16) | (ulDacXScale));
	STG_WRITE_REG(DACHorizontalScal, tmp);

	return 0;
}
