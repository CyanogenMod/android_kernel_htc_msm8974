/*
 *  linux/drivers/video/kyro/STG4000Reg.h
 *
 *  Copyright (C) 2002 STMicroelectronics
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#ifndef _STG4000REG_H
#define _STG4000REG_H

#define DWFILL unsigned long :32
#define WFILL unsigned short :16

#if defined(__KERNEL__)
#include <asm/page.h>
#include <asm/io.h>
#define STG_WRITE_REG(reg,data) (writel(data,&pSTGReg->reg))
#define STG_READ_REG(reg)      (readl(&pSTGReg->reg))
#else
#define STG_WRITE_REG(reg,data) (pSTGReg->reg = data)
#define STG_READ_REG(reg)      (pSTGReg->reg)
#endif 

#define SET_BIT(n) (1<<(n))
#define CLEAR_BIT(n) (tmp &= ~(1<<n))
#define CLEAR_BITS_FRM_TO(frm, to) \
{\
int i; \
    for(i = frm; i<= to; i++) \
	{ \
	    tmp &= ~(1<<i); \
	} \
}

#define CLEAR_BIT_2(n) (usTemp &= ~(1<<n))
#define CLEAR_BITS_FRM_TO_2(frm, to) \
{\
int i; \
    for(i = frm; i<= to; i++) \
	{ \
	    usTemp &= ~(1<<i); \
	} \
}

typedef enum _LUT_USES {
	NO_LUT = 0, RESERVED, GRAPHICS, OVERLAY
} LUT_USES;

typedef enum _PIXEL_FORMAT {
	_8BPP = 0, _15BPP, _16BPP, _24BPP, _32BPP
} PIXEL_FORMAT;

typedef enum _BLEND_MODE {
	GRAPHICS_MODE = 0, COLOR_KEY, PER_PIXEL_ALPHA, GLOBAL_ALPHA,
	CK_PIXEL_ALPHA, CK_GLOBAL_ALPHA
} OVRL_BLEND_MODE;

typedef enum _OVRL_PIX_FORMAT {
	UYVY, VYUY, YUYV, YVYU
} OVRL_PIX_FORMAT;

typedef struct {
	
	volatile u32 Thread0Enable;	
	volatile u32 Thread1Enable;	
	volatile u32 Thread0Recover;	
	volatile u32 Thread1Recover;	
	volatile u32 Thread0Step;	
	volatile u32 Thread1Step;	
	volatile u32 VideoInStatus;	
	volatile u32 Core2InSignStart;	
	volatile u32 Core1ResetVector;	
	volatile u32 Core1ROMOffset;	
	volatile u32 Core1ArbiterPriority;	
	volatile u32 VideoInControl;	
	volatile u32 VideoInReg0CtrlA;	
	volatile u32 VideoInReg0CtrlB;	
	volatile u32 VideoInReg1CtrlA;	
	volatile u32 VideoInReg1CtrlB;	
	volatile u32 Thread0Kicker;	
	volatile u32 Core2InputSign;	
	volatile u32 Thread0ProgCtr;	
	volatile u32 Thread1ProgCtr;	
	volatile u32 Thread1Kicker;	
	volatile u32 GPRegister1;	
	volatile u32 GPRegister2;	
	volatile u32 GPRegister3;	
	volatile u32 GPRegister4;	
	volatile u32 SerialIntA;	

	volatile u32 Fill0[6];	

	volatile u32 SoftwareReset;	
	volatile u32 SerialIntB;	

	volatile u32 Fill1[37];	

	volatile u32 ROMELQV;	
	volatile u32 WLWH;	
	volatile u32 ROMELWL;	

	volatile u32 dwFill_1;	

	volatile u32 IntStatus;	
	volatile u32 IntMask;	
	volatile u32 IntClear;	

	volatile u32 Fill2[6];	

	volatile u32 ROMGPIOA;	
	volatile u32 ROMGPIOB;	
	volatile u32 ROMGPIOC;	
	volatile u32 ROMGPIOD;	

	volatile u32 Fill3[2];	

	volatile u32 AGPIntID;	
	volatile u32 AGPIntClassCode;	
	volatile u32 AGPIntBIST;	
	volatile u32 AGPIntSSID;	
	volatile u32 AGPIntPMCSR;	
	volatile u32 VGAFrameBufBase;	
	volatile u32 VGANotify;	
	volatile u32 DACPLLMode;	
	volatile u32 Core1VideoClockDiv;	
	volatile u32 AGPIntStat;	

	volatile u32 Fill4[412];	

	volatile u32 TACtrlStreamBase;	
	volatile u32 TAObjDataBase;	
	volatile u32 TAPtrDataBase;	
	volatile u32 TARegionDataBase;	
	volatile u32 TATailPtrBase;	
	volatile u32 TAPtrRegionSize;	
	volatile u32 TAConfiguration;	
	volatile u32 TAObjDataStartAddr;	
	volatile u32 TAObjDataEndAddr;	
	volatile u32 TAXScreenClip;	
	volatile u32 TAYScreenClip;	
	volatile u32 TARHWClamp;	
	volatile u32 TARHWCompare;	
	volatile u32 TAStart;	
	volatile u32 TAObjReStart;	
	volatile u32 TAPtrReStart;	
	volatile u32 TAStatus1;	
	volatile u32 TAStatus2;	
	volatile u32 TAIntStatus;	
	volatile u32 TAIntMask;	

	volatile u32 Fill5[235];	

	volatile u32 TextureAddrThresh;	
	volatile u32 Core1Translation;	
	volatile u32 TextureAddrReMap;	
	volatile u32 RenderOutAGPRemap;	
	volatile u32 _3DRegionReadTrans;	
	volatile u32 _3DPtrReadTrans;	
	volatile u32 _3DParamReadTrans;	
	volatile u32 _3DRegionReadThresh;	
	volatile u32 _3DPtrReadThresh;	
	volatile u32 _3DParamReadThresh;	
	volatile u32 _3DRegionReadAGPRemap;	
	volatile u32 _3DPtrReadAGPRemap;	
	volatile u32 _3DParamReadAGPRemap;	
	volatile u32 ZBufferAGPRemap;	
	volatile u32 TAIndexAGPRemap;	
	volatile u32 TAVertexAGPRemap;	
	volatile u32 TAUVAddrTrans;	
	volatile u32 TATailPtrCacheTrans;	
	volatile u32 TAParamWriteTrans;	
	volatile u32 TAPtrWriteTrans;	
	volatile u32 TAParamWriteThresh;	
	volatile u32 TAPtrWriteThresh;	
	volatile u32 TATailPtrCacheAGPRe;	
	volatile u32 TAParamWriteAGPRe;	
	volatile u32 TAPtrWriteAGPRe;	
	volatile u32 SDRAMArbiterConf;	
	volatile u32 SDRAMConf0;	
	volatile u32 SDRAMConf1;	
	volatile u32 SDRAMConf2;	
	volatile u32 SDRAMRefresh;	
	volatile u32 SDRAMPowerStat;	

	volatile u32 Fill6[2];	

	volatile u32 RAMBistData;	
	volatile u32 RAMBistCtrl;	
	volatile u32 FIFOBistKey;	
	volatile u32 RAMBistResult;	
	volatile u32 FIFOBistResult;	


	volatile u32 Fill7[16];	

	volatile u32 SDRAMAddrSign;	
	volatile u32 SDRAMDataSign;	
	volatile u32 SDRAMSignConf;	

	
	volatile u32 dwFill_2;

	volatile u32 ISPSignature;	

	volatile u32 Fill8[454];	

	volatile u32 DACPrimAddress;	
	volatile u32 DACPrimSize;	
	volatile u32 DACCursorAddr;	
	volatile u32 DACCursorCtrl;	
	volatile u32 DACOverlayAddr;	
	volatile u32 DACOverlayUAddr;	
	volatile u32 DACOverlayVAddr;	
	volatile u32 DACOverlaySize;	
	volatile u32 DACOverlayVtDec;	

	volatile u32 Fill9[9];	

	volatile u32 DACVerticalScal;	
	volatile u32 DACPixelFormat;	
	volatile u32 DACHorizontalScal;	
	volatile u32 DACVidWinStart;	
	volatile u32 DACVidWinEnd;	
	volatile u32 DACBlendCtrl;	
	volatile u32 DACHorTim1;	
	volatile u32 DACHorTim2;	
	volatile u32 DACHorTim3;	
	volatile u32 DACVerTim1;	
	volatile u32 DACVerTim2;	
	volatile u32 DACVerTim3;	
	volatile u32 DACBorderColor;	
	volatile u32 DACSyncCtrl;	
	volatile u32 DACStreamCtrl;	
	volatile u32 DACLUTAddress;	
	volatile u32 DACLUTData;	
	volatile u32 DACBurstCtrl;	
	volatile u32 DACCrcTrigger;	
	volatile u32 DACCrcDone;	
	volatile u32 DACCrcResult1;	
	volatile u32 DACCrcResult2;	
	volatile u32 DACLinecount;	

	volatile u32 Fill10[151];	

	volatile u32 DigVidPortCtrl;	
	volatile u32 DigVidPortStat;	


	volatile u32 Fill11[1598];

	
	volatile u32 Fill_3;

} STG4000REG;

#endif 
