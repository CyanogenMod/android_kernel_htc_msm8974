
/************************************************************************************************************
*
* FILE	  : mmd.c
*
* DATE    : $Date: 2004/07/23 11:57:45 $   $Revision: 1.4 $
* Original: 2004/05/28 14:05:35    Revision: 1.32      Tag: hcf7_t20040602_01
* Original: 2004/05/13 15:31:45    Revision: 1.30      Tag: hcf7_t7_20040513_01
* Original: 2004/04/15 09:24:42    Revision: 1.25      Tag: hcf7_t7_20040415_01
* Original: 2004/04/08 15:18:17    Revision: 1.24      Tag: t7_20040413_01
* Original: 2004/04/01 15:32:55    Revision: 1.22      Tag: t7_20040401_01
* Original: 2004/03/10 15:39:28    Revision: 1.18      Tag: t20040310_01
* Original: 2004/03/03 14:10:12    Revision: 1.16      Tag: t20040304_01
* Original: 2004/03/02 09:27:12    Revision: 1.14      Tag: t20040302_03
* Original: 2004/02/24 13:00:29    Revision: 1.12      Tag: t20040224_01
* Original: 2004/01/30 09:59:33    Revision: 1.11      Tag: t20040219_01
*
* AUTHOR  : Nico Valster
*
* DESC    : Common routines for HCF, MSF, UIL as well as USF sources
*
* Note: relative to Asserts, the following can be observed:
*	Since the IFB is not known inside the routine, the macro HCFASSERT is replaced with MDDASSERT.
*	Also the line number reported in the assert is raised by FILE_NAME_OFFSET (20000) to discriminate the
*	MMD Asserts from HCF and DHF asserts.
*
***************************************************************************************************************
*
*
* SOFTWARE LICENSE
*
* This software is provided subject to the following terms and conditions,
* which you should read carefully before using the software.  Using this
* software indicates your acceptance of these terms and conditions.  If you do
* not agree with these terms and conditions, do not use the software.
*
* COPYRIGHT © 2001 - 2004	by Agere Systems Inc.	All Rights Reserved
* All rights reserved.
*
* Redistribution and use in source or binary forms, with or without
* modifications, are permitted provided that the following conditions are met:
*
* . Redistributions of source code must retain the above copyright notice, this
*    list of conditions and the following Disclaimer as comments in the code as
*    well as in the documentation and/or other materials provided with the
*    distribution.
*
* . Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following Disclaimer in the documentation
*    and/or other materials provided with the distribution.
*
* . Neither the name of Agere Systems Inc. nor the names of the contributors
*    may be used to endorse or promote products derived from this software
*    without specific prior written permission.
*
* Disclaimer
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
* INCLUDING, BUT NOT LIMITED TO, INFRINGEMENT AND THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  ANY
* USE, MODIFICATION OR DISTRIBUTION OF THIS SOFTWARE IS SOLELY AT THE USERS OWN
* RISK. IN NO EVENT SHALL AGERE SYSTEMS INC. OR CONTRIBUTORS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, INCLUDING, BUT NOT LIMITED TO, CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
* DAMAGE.
*
*
**************************************************************************************************************/

#include "hcf.h"				
#include "hcfdef.h"				
#include "mmd.h"				

#undef	FILE_NAME_OFFSET
#define FILE_NAME_OFFSET DHF_FILE_NAME_OFFSET


CFG_RANGE_SPEC_STRCT*
mmd_check_comp( CFG_RANGES_STRCT *actp, CFG_SUP_RANGE_STRCT *supp )
{

CFG_RANGE_SPEC_BYTE_STRCT  *actq = (CFG_RANGE_SPEC_BYTE_STRCT*)actp->var_rec;
CFG_RANGE_SPEC_BYTE_STRCT  *supq = (CFG_RANGE_SPEC_BYTE_STRCT*)&(supp->variant);
hcf_16	i;
int		act_endian;					
int		sup_endian;					

	act_endian = actp->role == COMP_ROLE_ACT;	
	sup_endian = supp->bottom < 0x0100;		 	

#if HCF_ASSERT
	MMDASSERT( supp->len == 6, 																supp->len )
	MMDASSERT( actp->len >= 6 && actp->len%3 == 0, 											actp->len )

	if ( act_endian ) {							
		MMDASSERT( actp->role == COMP_ROLE_ACT,												actp->role )
		MMDASSERT( 1 <= actp->id && actp->id <= 99,  										actp->id )
	} else {									
		MMDASSERT( actp->role == CNV_END_SHORT(COMP_ROLE_ACT),	 							actp->role )
		MMDASSERT( 1 <= CNV_END_SHORT(actp->id) && CNV_END_SHORT(actp->id) <= 99,				actp->id )
	}
	if ( sup_endian ) {							
		MMDASSERT( supp->role == COMP_ROLE_SUPL, 											supp->role )
		MMDASSERT( 1 <= supp->id      && supp->id <= 99,  									supp->id )
		MMDASSERT( 1 <= supp->variant && supp->variant <= 99,  								supp->variant )
		MMDASSERT( 1 <= supp->bottom  && supp->bottom <= 99, 								supp->bottom )
		MMDASSERT( 1 <= supp->top     && supp->top <= 99, 		 							supp->top )
		MMDASSERT( supp->bottom <= supp->top,							supp->bottom << 8 | supp->top )
	} else {									
		MMDASSERT( supp->role == CNV_END_SHORT(COMP_ROLE_SUPL), 								supp->role )
		MMDASSERT( 1 <= CNV_END_SHORT(supp->id) && CNV_END_SHORT(supp->id) <= 99,				supp->id )
		MMDASSERT( 1 <= CNV_END_SHORT(supp->variant) && CNV_END_SHORT(supp->variant) <= 99,		supp->variant )
		MMDASSERT( 1 <= CNV_END_SHORT(supp->bottom)  && CNV_END_SHORT(supp->bottom) <=99,		supp->bottom )
		MMDASSERT( 1 <= CNV_END_SHORT(supp->top)     && CNV_END_SHORT(supp->top) <=99,  		supp->top )
		MMDASSERT( CNV_END_SHORT(supp->bottom) <= CNV_END_SHORT(supp->top),	supp->bottom << 8 |	supp->top )
	}
#endif 

#if HCF_BIG_ENDIAN == 0
	act_endian = !act_endian;																		
	sup_endian = !sup_endian;																		
#endif 

	for ( i = actp->len ; i > 3; actq++, i -= 3 ) {													
		MMDASSERT( actq->variant[act_endian] <= 99, i<<8 | actq->variant[act_endian] )
		MMDASSERT( actq->bottom[act_endian] <= 99 , i<<8 | actq->bottom[act_endian] )
		MMDASSERT( actq->top[act_endian] <= 99	  , i<<8 | actq->top[act_endian] )
		MMDASSERT( actq->bottom[act_endian] <= actq->top[act_endian], i<<8 | actq->bottom[act_endian] )
		if ( actq->variant[act_endian] == supq->variant[sup_endian] &&
			 actq->bottom[act_endian]  <= supq->top[sup_endian] &&
			 actq->top[act_endian]     >= supq->bottom[sup_endian]
		   ) break;
	}
	if ( i <= 3 || supp->len != 6  ) {
	   actq = NULL;																					
	}
#if HCF_ASSERT
	if ( actq == NULL ) {
		for ( i = 0; i <= supp->len; i += 2 ) {
			MMDASSERT( DO_ASSERT, MERGE_2( ((hcf_16*)supp)[i], ((hcf_16*)supp)[i+1] ) );
		}
		for ( i = 0; i <= actp->len; i += 2 ) {
			MMDASSERT( DO_ASSERT, MERGE_2( ((hcf_16*)actp)[i], ((hcf_16*)actp)[i+1] ) );
		}
	}
#endif 
	return (CFG_RANGE_SPEC_STRCT*)actq;
} 

