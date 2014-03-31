
#ifndef DHF_H
#define DHF_H

/**************************************************************************************************************
*
* FILE   :	DHF.H
*
* DATE	:	$Date: 2004/07/19 08:16:14 $   $Revision: 1.2 $
* Original	:	2004/05/17 07:33:13    Revision: 1.25      Tag: hcf7_t20040602_01
* Original	:	2004/05/11 06:03:14    Revision: 1.24      Tag: hcf7_t7_20040513_01
* Original	:	2004/04/15 09:24:42    Revision: 1.22      Tag: hcf7_t7_20040415_01
* Original	:	2004/04/09 14:35:52    Revision: 1.21      Tag: t7_20040413_01
* Original	:	2004/04/01 15:32:55    Revision: 1.18      Tag: t7_20040401_01
* Original	:	2004/03/10 15:39:28    Revision: 1.15      Tag: t20040310_01
* Original	:	2004/03/04 11:03:38    Revision: 1.13      Tag: t20040304_01
* Original	:	2004/02/25 14:14:37    Revision: 1.11      Tag: t20040302_03
* Original	:	2004/02/24 13:00:28    Revision: 1.10      Tag: t20040224_01
* Original	:	2004/02/19 10:57:28    Revision: 1.8      Tag: t20040219_01
*
* AUTHOR :	John Meertens
*			Nico Valster
*
* SPECIFICATION: .........
*
* DESC   :	structure definitions and function prototypes for unit DHF.
*
*			Customizable via HCFCFG.H, which is included indirectly via HCF.H
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
* COPYRIGHT (C) 1994 - 1995	by AT&T.				All Rights Reserved
* COPYRIGHT (C) 1999 - 2000 by Lucent Technologies.	All Rights Reserved
* COPYRIGHT (C) 2001 - 2004	by Agere Systems Inc.	All Rights Reserved
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


#ifdef _WIN32_WCE
#include <windef.h>
#endif

#include "hcf.h"   		 	

#ifdef DHF_UIL
#define GET_INFO(pp)  uil_get_info((LTVP)pp)
#define PUT_INFO(pp)  uil_put_info((LTVP)pp)
#else
#define GET_INFO(pp)  hcf_get_info(ifbp, (LTVP)pp)
#define PUT_INFO(pp)  hcf_put_info(ifbp, (LTVP)pp)
#endif


#define CODEMASK				0x0000FFFFL    	


#define DHF_ERR_INCOMP_FW		0x40	


typedef struct {
	LTVP 	ltvp;
	hcf_16	len;
} LTV_INFO_STRUCT , *LTV_INFO_STRUCT_PTR;



typedef struct {
	hcf_32	code;      	
	hcf_32	addr;      	
	hcf_32	len;       	
} plugrecord;


#define MAX_DEBUGSTRINGS 		1024
#define MAX_DEBUGSTRING_LEN 	  82

typedef struct {
	hcf_32	id;
	char 	str[MAX_DEBUGSTRING_LEN];
} stringrecord;


#define MAX_DEBUGEXPORTS 		2048
#define MAX_DEBUGEXPORT_LEN 	  12

typedef struct {
	hcf_32	id;
	char 	str[MAX_DEBUGEXPORT_LEN];
} exportrecord;

#define FWSTRINGS_FUNCTION		0
#define FWEXPORTS_FUNCTION		1

typedef struct {
	char					signature[14+1+1];	
	CFG_PROG_STRCT FAR *codep;				
	hcf_32           	 	execution;    		
	void FAR *place_holder_1;
	void FAR  		     	*place_holder_2;
	CFG_RANGE20_STRCT FAR  	*compat;      		
	CFG_IDENTITY_STRCT FAR 	*identity;    		
	void FAR				*p[2];				
} memimage;




EXTERN_C int dhf_download_fw(void *ifbp, memimage *fw);	
EXTERN_C int dhf_download_binary(memimage *fw);



EXTERN_C hcf_16 *find_record_in_pda(hcf_16 *pdap, hcf_16 code);

#endif  

