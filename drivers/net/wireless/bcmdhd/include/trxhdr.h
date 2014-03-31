/*
 * TRX image file header format.
 *
 * Copyright (C) 1999-2012, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: trxhdr.h 260898 2011-05-20 23:11:12Z $
 */

#ifndef _TRX_HDR_H
#define _TRX_HDR_H

#include <typedefs.h>

#define TRX_MAGIC	0x30524448	
#define TRX_VERSION	1		
#define TRX_MAX_LEN	0x3B0000	
#define TRX_NO_HEADER	1		
#define TRX_GZ_FILES	0x2     
#define TRX_EMBED_UCODE	0x8	
#define TRX_ROMSIM_IMAGE	0x10	
#define TRX_UNCOMP_IMAGE	0x20	
#define TRX_MAX_OFFSET	3		

struct trx_header {
	uint32 magic;		
	uint32 len;		
	uint32 crc32;		
	uint32 flag_version;	
	uint32 offsets[TRX_MAX_OFFSET];	
};

typedef struct trx_header TRXHDR, *PTRXHDR;

#endif 
