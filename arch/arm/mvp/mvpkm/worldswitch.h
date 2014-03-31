/*
 * Linux 2.6.32 and later Kernel module for VMware MVP Hypervisor Support
 *
 * Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#line 5

#ifndef _WORLDSWITCH_H
#define _WORLDSWITCH_H

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

#define MAX_REGISTER_SAVE_SIZE   464

#ifndef __ASSEMBLER__
typedef struct {
	uint32 kSPSR_svc;
	uint32 kr13_svc;
	uint32 kr14_svc;
	uint32 mSPSR_svc;
	uint32 mR13_svc;
	uint32 mR14_svc;

	uint32 kSPSR_abt;
	uint32 kr13_abt;
	uint32 kr14_abt;
	uint32 mSPSR_abt;
	uint32 mR13_abt;
	uint32 mR14_abt;

	uint32 kSPSR_und;
	uint32 kr13_und;
	uint32 kr14_und;
	uint32 mSPSR_und;
	uint32 mR13_und;
	uint32 mR14_und;

	uint32 kSPSR_irq;
	uint32 kr13_irq;
	uint32 kr14_irq;
	uint32 mSPSR_irq;
	uint32 mR13_irq;
	uint32 mR14_irq;

	uint32 kSPSR_fiq;
	uint32 kr8_fiq;
	uint32 kr9_fiq;
	uint32 kr10_fiq;
	uint32 kr11_fiq;
	uint32 kr12_fiq;
	uint32 kr13_fiq;
	uint32 kr14_fiq;
	uint32 mSPSR_fiq;
	uint32 mR8_fiq;
	uint32 mR9_fiq;
	uint32 mR10_fiq;
	uint32 mR11_fiq;
	uint32 mR12_fiq;
	uint32 mR13_fiq;
	uint32 mR14_fiq;
} BankedRegisterSave;

typedef struct {
	uint32 mCPSR;
	uint32 mR1;
	uint32 mR4;
	uint32 mR5;
	uint32 mR6;
	uint32 mR7;
	uint32 mR8;
	uint32 mR9;
	uint32 mR10;
	uint32 mR11;
	uint32 mSP;
	uint32 mLR;   
} MonitorRegisterSave;

typedef struct {
	uint32 kR2;   
	uint32 kR4;
	uint32 kR5;
	uint32 kR6;
	uint32 kR7;
	uint32 kR8;
	uint32 kR9;
	uint32 kR10;
	uint32 kR11;
	uint32 kR13;
	uint32 kR14;  

	BankedRegisterSave bankedRegs;

	uint32 kCtrlReg;
	uint32 kTTBR0;
	uint32 kDACR;
	uint32 kASID;
	uint32 kTIDUserRW;
	uint32 kTIDUserRO;
	uint32 kTIDPrivRW;
	uint32 kCSSELR;
	uint32 kPMNCIntEn;
	uint32 kPMNCCCCNT;
	uint32 kPMNCOvFlag;
	uint32 kOpEnabled;
	uint32 mCtrlReg;
	uint32 mTTBR0;
	uint32 mASID;
	uint32 mTIDUserRW;
	uint32 mTIDUserRO;
	uint32 mTIDPrivRW;
	uint32 mCSSELR;

	MonitorRegisterSave monRegs;
} RegisterSaveLPV;

typedef struct {
	uint32 mHTTBR;

	uint32 kR3;
	uint32 kR4;
	uint32 kR5;
	uint32 kR6;
	uint32 kR7;
	uint32 kR8;
	uint32 kR9;
	uint32 kR10;
	uint32 kR11;
	uint32 kR12;
	uint32 kCPSR;
	uint32 kRet;

	BankedRegisterSave bankedRegs;

	uint32 kCSSELR;
	uint32 kCtrlReg;
	uint32 kTTBR0[2];
	uint32 kTTBR1[2];
	uint32 kTTBRC;
	uint32 kDACR;
	uint32 kDFSR;
	uint32 kIFSR;
	uint32 kAuxDFSR;
	uint32 kAuxIFSR;
	uint32 kDFAR;
	uint32 kIFAR;
	uint32 kPAR[2];
	uint32 kPRRR;
	uint32 kNMRR;
	uint32 kASID;
	uint32 kTIDUserRW;
	uint32 kTIDUserRO;
	uint32 kTIDPrivRW;
	uint32 mCSSELR;
	uint32 mCtrlReg;
	uint32 mTTBR0[2];
	uint32 mTTBR1[2];
	uint32 mTTBRC;
	uint32 mDACR;
	uint32 mDFSR;
	uint32 mIFSR;
	uint32 mAuxDFSR;
	uint32 mAuxIFSR;
	uint32 mDFAR;
	uint32 mIFAR;
	uint32 mPAR[2];
	uint32 mPRRR;
	uint32 mNMRR;
	uint32 mASID;
	uint32 mTIDUserRW;
	uint32 mTIDUserRO;
	uint32 mTIDPrivRW;

	uint32 mHCR;
	uint32 mHDCR;
	uint32 mHCPTR;
	uint32 mHSTR;
	uint32 mVTTBR[2];
	uint32 mVTCR;

	MonitorRegisterSave monRegs;
} RegisterSaveVE;

typedef union {
	unsigned char reserve_space[MAX_REGISTER_SAVE_SIZE];
	RegisterSaveLPV lpv;
	RegisterSaveVE ve;
} RegisterSave;

MY_ASSERTS(REGSAVE,
	ASSERT_ON_COMPILE(sizeof(RegisterSave) == MAX_REGISTER_SAVE_SIZE);
)

typedef struct VFPSave {
	uint32 fpexc, fpscr, fpinst, fpinst2, cpacr, fpexc_;

	uint64 fpregs[32];

} VFPSave __attribute__((aligned(8)));

typedef struct WorldSwitchPage WorldSwitchPage;
typedef void (*SwitchToMonitor)(RegisterSave *regSave);
typedef void (*SwitchToUser)(RegisterSave *regSaveEnd);

#include "atomic.h"
#include "monva_common.h"
#include "mksck_shared.h"

struct WorldSwitchPage {
	uint32          mvpkmVersion; 

	HKVA            wspHKVA;     
				     
	ARM_L1D         wspKVAL1D;   

	SwitchToMonitor switchToMonitor;
					
	SwitchToUser    switchToUser;   

	MonVA           monVA;          
					
	union {
		ARM_L2D monAttribL2D;	
					
		ARM_MemAttrNormal memAttr; 
					   
	};

	MonitorType     monType;        
					
	_Bool           allowInts;      
					
					
					
					
					

	struct {
		uint64       switchedAt64;  
		uint32       switchedAtTSC; 
					    
		uint32       tscToRate64Mult; 
					    
		uint32       tscToRate64Shift;	
						
	};

	struct {
		AtmUInt32    hostActions;   
					    
		Mksck_VmId   guestId;       
	};

	struct {			    
					    
		uint32       critSecCount;  
					    
					    
		_Bool        isPageMapped[MKSCK_MAX_SHARES];
		
		_Bool        guestPageMapped;
		
		uint32       isOpened;	
					
	};

#define WSP_PARAMS_SIZE 512
	uint8           params_[WSP_PARAMS_SIZE];
	

	RegisterSave    regSave;
	

	VFPSave         hostVFP;
	
	VFPSave         monVFP;

__attribute__((aligned(ARM_L2PT_COARSE_SIZE)))
	ARM_L2D wspDoubleMap[ARM_L2PT_COARSE_ENTRIES];
	
	uint8 secondHalfPadding[ARM_L2PT_COARSE_SIZE];
};

MY_ASSERTS(WSP,
	ASSERT_ON_COMPILE(offsetof(struct WorldSwitchPage, wspDoubleMap) %
			  ARM_L2PT_COARSE_SIZE == 0);
)

extern void SaveVFP(VFPSave *);
extern void LoadVFP(VFPSave *);

#define SWITCH_VFP_TO_MONITOR do {	\
	SaveVFP(&wsp->hostVFP);		\
	LoadVFP(&wsp->monVFP);		\
} while (0)

#define SWITCH_VFP_TO_HOST do {		\
	SaveVFP(&wsp->monVFP);		\
	LoadVFP(&wsp->hostVFP);		\
} while (0)

#endif 

#define OFFSETOF_KR3_REGSAVE_VE_WSP 616

#endif 
