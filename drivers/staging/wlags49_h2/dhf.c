
/**************************************************************************************************************
*
* FILE   :	DHF.C
*
* DATE	:	$Date: 2004/07/19 08:16:14 $   $Revision: 1.2 $
* Original	:	2004/05/28 14:05:34    Revision: 1.36      Tag: hcf7_t20040602_01
* Original	:	2004/05/11 06:22:57    Revision: 1.32      Tag: hcf7_t7_20040513_01
* Original	:	2004/04/15 09:24:42    Revision: 1.28      Tag: hcf7_t7_20040415_01
* Original	:	2004/04/08 15:18:16    Revision: 1.27      Tag: t7_20040413_01
* Original	:	2004/04/01 15:32:55    Revision: 1.25      Tag: t7_20040401_01
* Original	:	2004/03/10 15:39:28    Revision: 1.21      Tag: t20040310_01
* Original	:	2004/03/04 11:03:37    Revision: 1.19      Tag: t20040304_01
* Original	:	2004/03/02 09:27:11    Revision: 1.17      Tag: t20040302_03
* Original	:	2004/02/24 13:00:28    Revision: 1.15      Tag: t20040224_01
* Original	:	2004/02/19 10:57:28    Revision: 1.14      Tag: t20040219_01
* Original	:	2003/11/27 09:00:09    Revision: 1.3      Tag: t20021216_01
*
* AUTHOR :	John Meertens
*			Nico Valster
*
* SPECIFICATION: ........
*
* DESC   :	generic functions to handle the download of NIC firmware
*			Local Support Routines for above procedures
*
*			Customizable via HCFCFG.H, which is included by HCF.H
*
*
*	DHF is (intended to be) platform-independent.
*	DHF is a module that provides a number of routines to download firmware
*	images (the names primary, station, access point, secondary and tertiary
*	are used or have been used) to volatile or nonvolatile memory
*	in WaveLAN/IEEE NICs. To achieve this DHF makes use of the WaveLAN/IEEE
*	WCI as implemented by the HCF-module.
*
*	Download to non-volatile memory is used to update a WaveLAN/IEEE NIC to new
*	firmware. Normally this will be an upgrade to newer firmware, although
*	downgrading to older firmware is possible too.
*
* Note: relative to Asserts, the following can be observed:
*	Since the IFB is not known inside the routine, the macro HCFASSERT is replaced with MMDASSERT.
*	Also the line number reported in the assert is raised by FILE_NAME_OFFSET (10000) to discriminate the
*	DHF Asserts from HCF and MMD asserts.
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

#include "hcf.h"
#include "hcfdef.h"
#include "dhf.h"
#include "mmd.h"

#undef	FILE_NAME_OFFSET
#define FILE_NAME_OFFSET MMD_FILE_NAME_OFFSET

char signature[14] = "FUPU7D37dhfwci";


#define DL_SIZE 2000

CFG_SUP_RANGE_STRCT 	mfi_sup        	= { LOF(CFG_SUP_RANGE_STRCT), CFG_NIC_MFI_SUP_RANGE };
CFG_SUP_RANGE_STRCT 	cfi_sup        	= { LOF(CFG_SUP_RANGE_STRCT), CFG_NIC_CFI_SUP_RANGE };



LTV_INFO_STRUCT ltv_info[] = {
	{ (LTVP)&mfi_sup,			LOF(CFG_SUP_RANGE_STRCT) } ,
	{ (LTVP)&cfi_sup,			LOF(CFG_SUP_RANGE_STRCT) } ,
	{ (LTVP) NULL, 				0 }
};


static int				check_comp_fw(memimage *fw);


int
check_comp_fw(memimage *fw)
{
CFG_RANGE20_STRCT  		*p;
int   					rc = HCF_SUCCESS;
CFG_RANGE_SPEC_STRCT *i;

	switch (fw->identity->typ) {
	case CFG_FW_IDENTITY:				
	case COMP_ID_FW_AP_FAKE:			
		break;
	default:
		MMDASSERT(DO_ASSERT, fw->identity->typ) 	
		rc = DHF_ERR_INCOMP_FW;
		return rc; 
	}
	p = fw->compat;
	i = NULL;
	while (p->len && i == NULL) {					
		if (p->typ  == CFG_MFI_ACT_RANGES_STA) {
			i = mmd_check_comp((void *)p, &mfi_sup);
		}
		p++;
	}
	MMDASSERT(i, 0)	
	if (i) {
		p = fw->compat;
		i = NULL;
		while (p->len && i == NULL) {			
			if (p->typ  == CFG_CFI_ACT_RANGES_STA) {
				 i = mmd_check_comp((void *)p, &cfi_sup);
			}
			p++;
		}
		MMDASSERT(i, 0)	
	}
	if (i == NULL) {
		rc = DHF_ERR_INCOMP_FW;
	}
	return rc;
} 








int
dhf_download_binary(memimage *fw)
{
int 			rc = HCF_SUCCESS;
CFG_PROG_STRCT 	*p;
int				i;

	
	for (i = 0; i < sizeof(signature) && fw->signature[i] == signature[i]; i++)
		; 
	if (i != sizeof(signature) 		||
		 fw->signature[i] != 0x01   	||
		 
		 fw->signature[i+1] != ( 'L'))
		rc = DHF_ERR_INCOMP_FW;
	else {					
		fw->codep    = (CFG_PROG_STRCT FAR*)((char *)fw->codep + (hcf_32)fw);
		fw->identity = (CFG_IDENTITY_STRCT FAR*)((char *)fw->identity + (hcf_32)fw);
		fw->compat   = (CFG_RANGE20_STRCT FAR*)((char *)fw->compat + (hcf_32)fw);
		for (i = 0; fw->p[i]; i++)
			fw->p[i] = ((char *)fw->p[i] + (hcf_32)fw);
		p = fw->codep;
		while (p->len) {
			p->host_addr = (char *)p->host_addr + (hcf_32)fw;
			p++;
		}
	}
	return rc;
}   


int
dhf_download_fw(void *ifbp, memimage *fw)
{
int 				rc = HCF_SUCCESS;
LTV_INFO_STRUCT_PTR pp = ltv_info;
CFG_PROG_STRCT 		*p = fw->codep;
LTVP 				ltvp;
int					i;

	MMDASSERT(fw != NULL, 0)
	
	for (i = 0; i < sizeof(signature) && fw->signature[i] == signature[i]; i++)
		; 
	if (i != sizeof(signature) 		||
		 fw->signature[i] != 0x01		||
		 
		 (fw->signature[i+1] != 'C' && fw->signature[i+1] != ( 'L')))
		 rc = DHF_ERR_INCOMP_FW;

	while ((rc == HCF_SUCCESS) && ((ltvp = pp->ltvp) != NULL)) {
		ltvp->len = pp++->len;	
		rc = GET_INFO(ltvp);
		MMDASSERT(rc == HCF_SUCCESS, rc)
		MMDASSERT(rc == HCF_SUCCESS, ltvp->typ)
		MMDASSERT(rc == HCF_SUCCESS, ltvp->len)
	}
	if (rc == HCF_SUCCESS)
		rc = check_comp_fw(fw);
	if (rc == HCF_SUCCESS) {
		while (rc == HCF_SUCCESS && p->len) {
			rc = PUT_INFO(p);
			p++;
		}
	}
	MMDASSERT(rc == HCF_SUCCESS, rc)
	return rc;
}   


