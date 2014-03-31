/*
 * This header file describes this specific Xtensa processor's TIE extensions
 * that extend basic Xtensa core functionality.  It is customized to this
 * Xtensa processor configuration.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1999-2007 Tensilica Inc.
 */

#ifndef _XTENSA_CORE_TIE_H
#define _XTENSA_CORE_TIE_H

#define XCHAL_CP_NUM			1	
#define XCHAL_CP_MAX			8	
#define XCHAL_CP_MASK			0x80	
#define XCHAL_CP_PORT_MASK		0x80	

#define XCHAL_CP7_NAME			"XTIOP"
#define XCHAL_CP7_IDENT			XTIOP
#define XCHAL_CP7_SA_SIZE		0	
#define XCHAL_CP7_SA_ALIGN		1	
#define XCHAL_CP_ID_XTIOP		7	

#define XCHAL_CP0_SA_SIZE		0
#define XCHAL_CP0_SA_ALIGN		1
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
#define XCHAL_CP6_SA_SIZE		0
#define XCHAL_CP6_SA_ALIGN		1

#define XCHAL_NCP_SA_SIZE		32
#define XCHAL_NCP_SA_ALIGN		4

#define XCHAL_TOTAL_SA_SIZE		32	
#define XCHAL_TOTAL_SA_ALIGN		4	


#define XCHAL_NCP_SA_NUM	8
#define XCHAL_NCP_SA_LIST(s)	\
 XCHAL_SA_REG(s,1,0,0,1,          acclo, 4, 4, 4,0x0210,  sr,16 , 32,0,0,0) \
 XCHAL_SA_REG(s,1,0,0,1,          acchi, 4, 4, 4,0x0211,  sr,17 ,  8,0,0,0) \
 XCHAL_SA_REG(s,0,0,0,1,             m0, 4, 4, 4,0x0220,  sr,32 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,0,1,             m1, 4, 4, 4,0x0221,  sr,33 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,0,1,             m2, 4, 4, 4,0x0222,  sr,34 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,0,1,             m3, 4, 4, 4,0x0223,  sr,35 , 32,0,0,0) \
 XCHAL_SA_REG(s,0,0,0,1,      scompare1, 4, 4, 4,0x020C,  sr,12 , 32,0,0,0) \
 XCHAL_SA_REG(s,1,2,1,1,      threadptr, 4, 4, 4,0x03E7,  ur,231, 32,0,0,0)

#define XCHAL_CP0_SA_NUM	0
#define XCHAL_CP0_SA_LIST(s)	

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

#define XCHAL_CP6_SA_NUM	0
#define XCHAL_CP6_SA_LIST(s)	

#define XCHAL_CP7_SA_NUM	0
#define XCHAL_CP7_SA_LIST(s)	

#define XCHAL_OP0_FORMAT_LENGTHS	3,3,3,3,3,3,3,3,2,2,2,2,2,2,3,3

#endif 

