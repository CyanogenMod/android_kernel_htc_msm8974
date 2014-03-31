/*
 * This header file describes this specific Xtensa processor's TIE extensions
 * that extend basic Xtensa core functionality.  It is customized to this
 * Xtensa processor configuration.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1999-2008 Tensilica Inc.
 */

#ifndef _XTENSA_CORE_TIE_H
#define _XTENSA_CORE_TIE_H

#define XCHAL_CP_NUM			2	
#define XCHAL_CP_MAX			7	
#define XCHAL_CP_MASK			0x41	
#define XCHAL_CP_PORT_MASK		0x00	

#define XCHAL_CP0_NAME			"FPU"
#define XCHAL_CP0_IDENT			FPU
#define XCHAL_CP0_SA_SIZE		72	
#define XCHAL_CP0_SA_ALIGN		4	
#define XCHAL_CP_ID_FPU			0	
#define XCHAL_CP6_NAME			"XAD"
#define XCHAL_CP6_IDENT			XAD
#define XCHAL_CP6_SA_SIZE		576	
#define XCHAL_CP6_SA_ALIGN		16	
#define XCHAL_CP_ID_XAD			6	

#define XCHAL_CP1_SA_SIZE		0
#define XCHAL_CP1_SA_ALIGN		1
#define XCHAL_CP2_SA_SIZE		0
#define XCHAL_CP2_SA_ALIGN		1
#define XCHAL_CP3_SA_SIZE		0
#define XCHAL_CP3_SA_ALIGN		1
#define XCHAL_CP4_SA_SIZE		0
#define XCHAL_CP4_SA_ALIGN		1
#define XCHAL_CP5_SA_SIZE		0
#define XCHAL_CP5_SA_ALIGN		1
#define XCHAL_CP7_SA_SIZE		0
#define XCHAL_CP7_SA_ALIGN		1

#define XCHAL_NCP_SA_SIZE		4
#define XCHAL_NCP_SA_ALIGN		4

#define XCHAL_TOTAL_SA_SIZE		672	
#define XCHAL_TOTAL_SA_ALIGN		16	


#define XCHAL_NCP_SA_NUM	1
#define XCHAL_NCP_SA_LIST(s)	\
 XCHAL_SA_REG(s,0,0,0,1,             br, 4, 4, 4,0x0204,  sr,4  , 16,0,0,0)

#define XCHAL_CP0_SA_NUM	18
#define XCHAL_CP0_SA_LIST(s)	\
 XCHAL_SA_REG(s,0,0,1,0,            fcr, 4, 4, 4,0x03E8,  ur,232, 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,1,0,            fsr, 4, 4, 4,0x03E9,  ur,233, 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,             f0, 4, 4, 4,0x0030,   f,0  , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,             f1, 4, 4, 4,0x0031,   f,1  , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,             f2, 4, 4, 4,0x0032,   f,2  , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,             f3, 4, 4, 4,0x0033,   f,3  , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,             f4, 4, 4, 4,0x0034,   f,4  , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,             f5, 4, 4, 4,0x0035,   f,5  , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,             f6, 4, 4, 4,0x0036,   f,6  , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,             f7, 4, 4, 4,0x0037,   f,7  , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,             f8, 4, 4, 4,0x0038,   f,8  , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,             f9, 4, 4, 4,0x0039,   f,9  , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,            f10, 4, 4, 4,0x003A,   f,10 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,            f11, 4, 4, 4,0x003B,   f,11 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,            f12, 4, 4, 4,0x003C,   f,12 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,            f13, 4, 4, 4,0x003D,   f,13 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,            f14, 4, 4, 4,0x003E,   f,14 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,            f15, 4, 4, 4,0x003F,   f,15 , 32,0,0,0)

#define XCHAL_CP1_SA_NUM	0
#define XCHAL_CP1_SA_LIST(s)	

#define XCHAL_CP2_SA_NUM	0
#define XCHAL_CP2_SA_LIST(s)	

#define XCHAL_CP3_SA_NUM	0
#define XCHAL_CP3_SA_LIST(s)	

#define XCHAL_CP4_SA_NUM	0
#define XCHAL_CP4_SA_LIST(s)	

#define XCHAL_CP5_SA_NUM	0
#define XCHAL_CP5_SA_LIST(s)	

#define XCHAL_CP6_SA_NUM	46
#define XCHAL_CP6_SA_LIST(s)	\
 XCHAL_SA_REG(s,0,0,1,0,         ldcbhi,16, 4, 4,0x0300,  ur,0  , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,1,0,         ldcblo, 4, 4, 4,0x0301,  ur,1  , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,1,0,         stcbhi, 4, 4, 4,0x0302,  ur,2  , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,1,0,         stcblo, 4, 4, 4,0x0303,  ur,3  , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,1,0,       ldbrbase, 4, 4, 4,0x0308,  ur,8  , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,1,0,        ldbroff, 4, 4, 4,0x0309,  ur,9  , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,1,0,        ldbrinc, 4, 4, 4,0x030A,  ur,10 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,1,0,       stbrbase, 4, 4, 4,0x030B,  ur,11 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,1,0,        stbroff, 4, 4, 4,0x030C,  ur,12 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,1,0,        stbrinc, 4, 4, 4,0x030D,  ur,13 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,1,0,       scratch0, 4, 4, 4,0x0318,  ur,24 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,1,0,       scratch1, 4, 4, 4,0x0319,  ur,25 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,1,0,       scratch2, 4, 4, 4,0x031A,  ur,26 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,1,0,       scratch3, 4, 4, 4,0x031B,  ur,27 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wra0,16,16,16,0x1010, wra,0  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wra1,16,16,16,0x1011, wra,1  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wra2,16,16,16,0x1012, wra,2  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wra3,16,16,16,0x1013, wra,3  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wra4,16,16,16,0x1014, wra,4  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wra5,16,16,16,0x1015, wra,5  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wra6,16,16,16,0x1016, wra,6  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wra7,16,16,16,0x1017, wra,7  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wra8,16,16,16,0x1018, wra,8  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wra9,16,16,16,0x1019, wra,9  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,          wra10,16,16,16,0x101A, wra,10 ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,          wra11,16,16,16,0x101B, wra,11 ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,          wra12,16,16,16,0x101C, wra,12 ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,          wra13,16,16,16,0x101D, wra,13 ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,          wra14,16,16,16,0x101E, wra,14 ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,          wra15,16,16,16,0x101F, wra,15 ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wrb0,16,16,16,0x1020, wrb,0  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wrb1,16,16,16,0x1021, wrb,1  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wrb2,16,16,16,0x1022, wrb,2  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wrb3,16,16,16,0x1023, wrb,3  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wrb4,16,16,16,0x1024, wrb,4  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wrb5,16,16,16,0x1025, wrb,5  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wrb6,16,16,16,0x1026, wrb,6  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wrb7,16,16,16,0x1027, wrb,7  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wrb8,16,16,16,0x1028, wrb,8  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,           wrb9,16,16,16,0x1029, wrb,9  ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,          wrb10,16,16,16,0x102A, wrb,10 ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,          wrb11,16,16,16,0x102B, wrb,11 ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,          wrb12,16,16,16,0x102C, wrb,12 ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,          wrb13,16,16,16,0x102D, wrb,13 ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,          wrb14,16,16,16,0x102E, wrb,14 ,128,0,0,0) \
 XCHAL_SA_REG(s,0,0,2,0,          wrb15,16,16,16,0x102F, wrb,15 ,128,0,0,0)

#define XCHAL_CP7_SA_NUM	0
#define XCHAL_CP7_SA_LIST(s)	

#define XCHAL_OP0_FORMAT_LENGTHS	3,3,3,3,3,3,3,3,2,2,2,2,2,2,8,8

#endif 

