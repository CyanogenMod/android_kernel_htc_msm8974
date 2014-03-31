/*===========================================================================
FILE:
   QMI.h

DESCRIPTION:
   Qualcomm QMI driver header
   
FUNCTIONS:
   Generic QMUX functions
      ParseQMUX
      FillQMUX
   
   Generic QMI functions
      GetTLV
      ValidQMIMessage
      GetQMIMessageID

   Get sizes of buffers needed by QMI requests
      QMUXHeaderSize
      QMICTLGetClientIDReqSize
      QMICTLReleaseClientIDReqSize
      QMICTLReadyReqSize
      QMIWDSSetEventReportReqSize
      QMIWDSGetPKGSRVCStatusReqSize
      QMIDMSGetMEIDReqSize

   Fill Buffers with QMI requests
      QMICTLGetClientIDReq
      QMICTLReleaseClientIDReq
      QMICTLReadyReq
      QMIWDSSetEventReportReq
      QMIWDSGetPKGSRVCStatusReq
      QMIDMSGetMEIDReq
      
   Parse data from QMI responses
      QMICTLGetClientIDResp
      QMICTLReleaseClientIDResp
      QMIWDSEventResp
      QMIDMSGetMEIDResp

Copyright (c) 2010, The Linux Foundation. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 and
only version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.

===========================================================================*/

#pragma once


#define QMICTL 0
#define QMIWDS 1
#define QMIDMS 2

#define u8        unsigned char
#define u16       unsigned short
#define u32       unsigned int
#define u64       unsigned long long

#define bool      u8
#define true      1
#define false     0

#define ENOMEM    12
#define EFAULT    14
#define EINVAL    22
#define ENOMSG    42
#define ENODATA   61

typedef struct sQMUX
{
   
   u8         mTF;

   
   u16        mLength;

   
   u8         mCtrlFlag;
   
   
   u8         mQMIService;
   
   
   u8         mQMIClientID;

}__attribute__((__packed__)) sQMUX;


int ParseQMUX(
   u16 *    pClientID,
   void *   pBuffer,
   u16      buffSize );

int FillQMUX(
   u16      clientID,
   void *   pBuffer,
   u16      buffSize );


u16 GetTLV(
   void *   pQMIMessage,
   u16      messageLen,
   u8       type,
   void *   pOutDataBuf,
   u16      bufferLen );

int ValidQMIMessage(
   void *   pQMIMessage,
   u16      messageLen );

int GetQMIMessageID(
   void *   pQMIMessage,
   u16      messageLen );


u16 QMUXHeaderSize( void );

u16 QMICTLGetClientIDReqSize( void );

u16 QMICTLReleaseClientIDReqSize( void );

u16 QMICTLReadyReqSize( void );

u16 QMIWDSSetEventReportReqSize( void );

u16 QMIWDSGetPKGSRVCStatusReqSize( void );

u16 QMIDMSGetMEIDReqSize( void );


int QMICTLGetClientIDReq(
   void *   pBuffer,
   u16      buffSize,
   u8       transactionID,
   u8       serviceType );

int QMICTLReleaseClientIDReq(
   void *   pBuffer,
   u16      buffSize,
   u8       transactionID,
   u16      clientID );

int QMICTLReadyReq(
   void *   pBuffer,
   u16      buffSize,
   u8       transactionID );

int QMIWDSSetEventReportReq(
   void *   pBuffer,
   u16      buffSize,
   u16      transactionID );

int QMIWDSGetPKGSRVCStatusReq(
   void *   pBuffer,
   u16      buffSize,
   u16      transactionID );

int QMIDMSGetMEIDReq(
   void *   pBuffer,
   u16      buffSize,
   u16      transactionID );


int QMICTLGetClientIDResp(
   void * pBuffer,
   u16    buffSize,
   u16 *  pClientID );

int QMICTLReleaseClientIDResp(
   void *   pBuffer,
   u16      buffSize );

int QMIWDSEventResp(
   void *   pBuffer,
   u16      buffSize,
   u32 *    pTXOk,
   u32 *    pRXOk,
   u32 *    pTXErr,
   u32 *    pRXErr,
   u32 *    pTXOfl,
   u32 *    pRXOfl,
   u64 *    pTXBytesOk,
   u64 *    pRXBytesOk,
   bool *   pbLinkState,
   bool *   pbReconfigure );

int QMIDMSGetMEIDResp(
   void *   pBuffer,
   u16      buffSize,
   char *   pMEID,
   int      meidSize );

