/*
 * Data structures and definitions for the CAM system.
 *
 * Copyright (c) 1997 Justin T. Gibbs.
 * Copyright (c) 2000 Adaptec Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL").
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: //depot/aic7xxx/linux/drivers/scsi/aic7xxx/cam.h#15 $
 */

#ifndef _AIC7XXX_CAM_H
#define _AIC7XXX_CAM_H 1

#include <linux/types.h>

#define	CAM_BUS_WILDCARD ((u_int)~0)
#define	CAM_TARGET_WILDCARD ((u_int)~0)
#define	CAM_LUN_WILDCARD ((u_int)~0)

typedef enum {
	CAM_REQ_INPROG,		
	CAM_REQ_CMP,		
	CAM_REQ_ABORTED,	
	CAM_UA_ABORT,		
	CAM_REQ_CMP_ERR,	
	CAM_BUSY,		
	CAM_REQ_INVALID,	
	CAM_PATH_INVALID,	
	CAM_SEL_TIMEOUT,	
	CAM_CMD_TIMEOUT,	
	CAM_SCSI_STATUS_ERROR,	
	CAM_SCSI_BUS_RESET,	
	CAM_UNCOR_PARITY,	
	CAM_AUTOSENSE_FAIL,	
	CAM_NO_HBA,		
	CAM_DATA_RUN_ERR,	
	CAM_UNEXP_BUSFREE,	
	CAM_SEQUENCE_FAIL,	
	CAM_CCB_LEN_ERR,	
	CAM_PROVIDE_FAIL,	
	CAM_BDR_SENT,		
	CAM_REQ_TERMIO,		
	CAM_UNREC_HBA_ERROR,	
	CAM_REQ_TOO_BIG,	
	CAM_UA_TERMIO,		
	CAM_MSG_REJECT_REC,	
	CAM_DEV_NOT_THERE,	
	CAM_RESRC_UNAVAIL,	
	CAM_REQUEUE_REQ,
	CAM_DEV_QFRZN		= 0x40,

	CAM_STATUS_MASK		= 0x3F
} cam_status;

typedef enum {
	AC_GETDEV_CHANGED	= 0x800,
	AC_INQ_CHANGED		= 0x400,
	AC_TRANSFER_NEG		= 0x200,
	AC_LOST_DEVICE		= 0x100,
	AC_FOUND_DEVICE		= 0x080,
	AC_PATH_DEREGISTERED	= 0x040,
	AC_PATH_REGISTERED	= 0x020,
	AC_SENT_BDR		= 0x010,
	AC_SCSI_AEN		= 0x008,
	AC_UNSOL_RESEL		= 0x002,
	AC_BUS_RESET		= 0x001 
} ac_code;

typedef enum {
	CAM_DIR_IN		= DMA_FROM_DEVICE,
	CAM_DIR_OUT		= DMA_TO_DEVICE,
	CAM_DIR_NONE		= DMA_NONE,
} ccb_flags;

#endif 
