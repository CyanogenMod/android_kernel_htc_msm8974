/*
 *  Name:       uamp_api.h
 *
 *  Description: Universal AMP API
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
 * $Id: uamp_api.h 294267 2011-11-04 23:41:52Z $
 *
 */
#ifndef UAMP_API_H
#define UAMP_API_H


#include "typedefs.h"



#define BT_API

typedef bool	BOOLEAN;
typedef uint8	UINT8;
typedef uint16	UINT16;


#define UAMP_ID_1   1
#define UAMP_ID_2   2
typedef UINT8 tUAMP_ID;

#define UAMP_EVT_RX_READY           0   
#define UAMP_EVT_CTLR_REMOVED       1   
#define UAMP_EVT_CTLR_READY         2   
typedef UINT8 tUAMP_EVT;


#define UAMP_CH_HCI_CMD            0   
#define UAMP_CH_HCI_EVT            1   
#define UAMP_CH_HCI_DATA           2   
typedef UINT8 tUAMP_CH;

typedef union {
    tUAMP_CH channel;       
} tUAMP_EVT_DATA;


typedef void (*tUAMP_CBACK)(tUAMP_ID amp_id, tUAMP_EVT amp_evt, tUAMP_EVT_DATA *p_amp_evt_data);

#ifdef __cplusplus
extern "C"
{
#endif

BT_API BOOLEAN UAMP_Init(tUAMP_CBACK p_cback);


BT_API BOOLEAN UAMP_Open(tUAMP_ID amp_id);

BT_API void UAMP_Close(tUAMP_ID amp_id);


/*****************************************************************************
**
** Function:    UAMP_Write
**
** Description: Send buffer to AMP device. Frees GKI buffer when done.
**
**
** Parameters:  app_id:     AMP identifer.
**              p_buf:      pointer to buffer to write
**              num_bytes:  number of bytes to write
**              channel:    UAMP_CH_HCI_ACL, or UAMP_CH_HCI_CMD
**
** Returns:     number of bytes written
**
******************************************************************************
*/
BT_API UINT16 UAMP_Write(tUAMP_ID amp_id, UINT8 *p_buf, UINT16 num_bytes, tUAMP_CH channel);

BT_API UINT16 UAMP_Read(tUAMP_ID amp_id, UINT8 *p_buf, UINT16 buf_size, tUAMP_CH channel);

#ifdef __cplusplus
}
#endif

#endif 
