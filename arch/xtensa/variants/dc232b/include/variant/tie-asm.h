/*
 * This header file contains assembly-language definitions (assembly
 * macros, etc.) for this specific Xtensa processor's TIE extensions
 * and options.  It is customized to this Xtensa processor configuration.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1999-2007 Tensilica Inc.
 */

#ifndef _XTENSA_CORE_TIE_ASM_H
#define _XTENSA_CORE_TIE_ASM_H

#define XTHAL_SAS_TIE	0x0001	
#define XTHAL_SAS_OPT	0x0002	
#define XTHAL_SAS_NOCC	0x0004	
#define XTHAL_SAS_CC	0x0008	
#define XTHAL_SAS_CALR	0x0010	
#define XTHAL_SAS_CALE	0x0020	
#define XTHAL_SAS_GLOB	0x0040	
#define XTHAL_SAS_ALL	0xFFFF	



	.macro xchal_ncp_store  ptr at1 at2 at3 at4  continue=0 ofs=-1 select=XTHAL_SAS_ALL
	xchal_sa_start	\continue, \ofs
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_CC | XTHAL_SAS_CALR) & ~\select
	xchal_sa_align	\ptr, 0, 1024-8, 4, 4
	rsr	\at1, ACCLO		
	rsr	\at2, ACCHI
	s32i	\at1, \ptr, .Lxchal_ofs_ + 0
	s32i	\at2, \ptr, .Lxchal_ofs_ + 4
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 8
	.endif
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~\select
	xchal_sa_align	\ptr, 0, 1024-16, 4, 4
	rsr	\at1, M0		
	rsr	\at2, M1
	s32i	\at1, \ptr, .Lxchal_ofs_ + 0
	s32i	\at2, \ptr, .Lxchal_ofs_ + 4
	rsr	\at1, M2
	rsr	\at2, M3
	s32i	\at1, \ptr, .Lxchal_ofs_ + 8
	s32i	\at2, \ptr, .Lxchal_ofs_ + 12
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 16
	.endif
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~\select
	xchal_sa_align	\ptr, 0, 1024-4, 4, 4
	rsr	\at1, SCOMPARE1		
	s32i	\at1, \ptr, .Lxchal_ofs_ + 0
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 4
	.endif
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_CC | XTHAL_SAS_GLOB) & ~\select
	xchal_sa_align	\ptr, 0, 1024-4, 4, 4
	rur	\at1, THREADPTR		
	s32i	\at1, \ptr, .Lxchal_ofs_ + 0
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 4
	.endif
	.endm	

	.macro xchal_ncp_load  ptr at1 at2 at3 at4  continue=0 ofs=-1 select=XTHAL_SAS_ALL
	xchal_sa_start	\continue, \ofs
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_CC | XTHAL_SAS_CALR) & ~\select
	xchal_sa_align	\ptr, 0, 1024-8, 4, 4
	l32i	\at1, \ptr, .Lxchal_ofs_ + 0
	l32i	\at2, \ptr, .Lxchal_ofs_ + 4
	wsr	\at1, ACCLO		
	wsr	\at2, ACCHI
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 8
	.endif
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~\select
	xchal_sa_align	\ptr, 0, 1024-16, 4, 4
	l32i	\at1, \ptr, .Lxchal_ofs_ + 0
	l32i	\at2, \ptr, .Lxchal_ofs_ + 4
	wsr	\at1, M0		
	wsr	\at2, M1
	l32i	\at1, \ptr, .Lxchal_ofs_ + 8
	l32i	\at2, \ptr, .Lxchal_ofs_ + 12
	wsr	\at1, M2
	wsr	\at2, M3
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 16
	.endif
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~\select
	xchal_sa_align	\ptr, 0, 1024-4, 4, 4
	l32i	\at1, \ptr, .Lxchal_ofs_ + 0
	wsr	\at1, SCOMPARE1		
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 4
	.endif
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_CC | XTHAL_SAS_GLOB) & ~\select
	xchal_sa_align	\ptr, 0, 1024-4, 4, 4
	l32i	\at1, \ptr, .Lxchal_ofs_ + 0
	wur	\at1, THREADPTR		
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 4
	.endif
	.endm	



#define XCHAL_NCP_NUM_ATMPS	2


#define XCHAL_SA_NUM_ATMPS	2

#endif 

