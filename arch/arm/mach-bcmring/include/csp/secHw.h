/*****************************************************************************
* Copyright 2004 - 2008 Broadcom Corporation.  All rights reserved.
*
* Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2, available at
* http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a
* license other than the GPL, without Broadcom's express prior written
* consent.
*****************************************************************************/

#ifndef SECHW_H
#define SECHW_H

typedef void (*secHw_FUNC_t) (void);

typedef enum {
	secHw_MODE_SECURE = 0x0,	
	secHw_MODE_NONSECURE = 0x1	
} secHw_MODE;

void secHw_RunSecure(secHw_FUNC_t	
    );

void secHw_SetMode(secHw_MODE	
    );

void secHw_GetMode(secHw_MODE *);

#endif 
