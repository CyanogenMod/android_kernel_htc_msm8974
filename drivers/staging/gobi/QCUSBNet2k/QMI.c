/*===========================================================================
FILE:
   QMI.c

DESCRIPTION:
   Qualcomm QMI driver code
   
FUNCTIONS:
   Generic QMUX functions
      ParseQMUX
      FillQMUX
   
   Generic QMI functions
      GetTLV
      ValidQMIMessage
      GetQMIMessageID

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

#include "QMI.h"



u16 QMUXHeaderSize( void )
{
   return sizeof( sQMUX );
}

u16 QMICTLGetClientIDReqSize( void )
{
   return sizeof( sQMUX ) + 10;
}

u16 QMICTLReleaseClientIDReqSize( void )
{
   return sizeof( sQMUX ) + 11;
}

u16 QMICTLReadyReqSize( void )
{
   return sizeof( sQMUX ) + 6;
}

u16 QMIWDSSetEventReportReqSize( void )
{
   return sizeof( sQMUX ) + 15;
}

u16 QMIWDSGetPKGSRVCStatusReqSize( void )
{
   return sizeof( sQMUX ) + 7;
}

u16 QMIDMSGetMEIDReqSize( void )
{
   return sizeof( sQMUX ) + 7;
}


int ParseQMUX(
   u16 *    pClientID,
   void *   pBuffer,
   u16      buffSize )
{
   sQMUX * pQMUXHeader;
   
   if (pBuffer == 0 || buffSize < 12)
   {
      return -ENOMEM;
   }

   
   pQMUXHeader = (sQMUX *)pBuffer;

   if (pQMUXHeader->mTF != 1
   ||  pQMUXHeader->mLength != buffSize - 1
   ||  pQMUXHeader->mCtrlFlag != 0x80 )
   {
      return -EINVAL;
   }

   
   *pClientID = (pQMUXHeader->mQMIClientID << 8) 
              + pQMUXHeader->mQMIService;
   
   return sizeof( sQMUX );
}

int FillQMUX(
   u16      clientID,
   void *   pBuffer,
   u16      buffSize )
{
   sQMUX * pQMUXHeader;

   if (pBuffer == 0 ||  buffSize < sizeof( sQMUX ))
   {
      return -ENOMEM;
   }

   
   pQMUXHeader = (sQMUX *)pBuffer;

   pQMUXHeader->mTF = 1;
   pQMUXHeader->mLength = buffSize - 1;
   pQMUXHeader->mCtrlFlag = 0;

   
   pQMUXHeader->mQMIService = clientID & 0xff;
   pQMUXHeader->mQMIClientID = clientID >> 8;

   return 0;
}


u16 GetTLV(
   void *   pQMIMessage,
   u16      messageLen,
   u8       type,
   void *   pOutDataBuf,
   u16      bufferLen )
{
   u16 pos;
   u16 tlvSize = 0;
   u16 cpyCount;
   
   if (pQMIMessage == 0 || pOutDataBuf == 0)
   {
      return -ENOMEM;
   }   
   
   for (pos = 4; 
        pos + 3 < messageLen; 
        pos += tlvSize + 3)
   {
      tlvSize = *(u16 *)(pQMIMessage + pos + 1);
      if (*(u8 *)(pQMIMessage + pos) == type)
      {
         if (bufferLen < tlvSize)
         {
            return -ENOMEM;
         }
        
         
         for (cpyCount = 0; cpyCount < tlvSize; cpyCount++)
         {
            *((char*)(pOutDataBuf + cpyCount)) = *((char*)(pQMIMessage + pos + 3 + cpyCount));
         }
         
         return tlvSize;
      }
   }
   
   return -ENOMSG;
}

int ValidQMIMessage(
   void *   pQMIMessage,
   u16      messageLen )
{
   char mandTLV[4];

   if (GetTLV( pQMIMessage, messageLen, 2, &mandTLV[0], 4 ) == 4)
   {
      
      if (*(u16 *)&mandTLV[0] != 0)
      {
         return *(u16 *)&mandTLV[2];
      }
      else
      {
         return 0;
      }
   }
   else
   {
      return -ENOMSG;
   }
}      

int GetQMIMessageID(
   void *   pQMIMessage,
   u16      messageLen )
{
   if (messageLen < 2)
   {
      return -ENODATA;
   }
   else
   {
      return *(u16 *)pQMIMessage;
   }
}


int QMICTLGetClientIDReq(
   void *   pBuffer,
   u16      buffSize,
   u8       transactionID,
   u8       serviceType )
{
   if (pBuffer == 0 || buffSize < QMICTLGetClientIDReqSize() )
   {
      return -ENOMEM;
   }

   
   
   *(u8 *)(pBuffer + sizeof( sQMUX ))= 0x00;
   
   *(u8 *)(pBuffer + sizeof( sQMUX ) + 1) = transactionID;
   
   *(u16 *)(pBuffer + sizeof( sQMUX ) + 2) = 0x0022;
   
   *(u16 *)(pBuffer + sizeof( sQMUX ) + 4) = 0x0004;
      
      *(u8 *)(pBuffer + sizeof( sQMUX ) + 6)  = 0x01;
      
      *(u16 *)(pBuffer + sizeof( sQMUX ) + 7) = 0x0001;
      
      *(u8 *)(pBuffer + sizeof( sQMUX ) + 9)  = serviceType;

  
  return sizeof( sQMUX ) + 10;
}

int QMICTLReleaseClientIDReq(
   void *   pBuffer,
   u16      buffSize,
   u8       transactionID,
   u16      clientID )
{
   if (pBuffer == 0 || buffSize < QMICTLReleaseClientIDReqSize() )
   {
      return -ENOMEM;
   }

   
   
   *(u8 *)(pBuffer + sizeof( sQMUX ))  = 0x00;
   
   *(u8 *)(pBuffer + sizeof( sQMUX ) + 1 ) = transactionID;
   
   *(u16 *)(pBuffer + sizeof( sQMUX ) + 2) = 0x0023;
   
   *(u16 *)(pBuffer + sizeof( sQMUX ) + 4) = 0x0005;
      
      *(u8 *)(pBuffer + sizeof( sQMUX ) + 6)  = 0x01;
      
      *(u16 *)(pBuffer + sizeof( sQMUX ) + 7) = 0x0002;
      
      *(u16 *)(pBuffer + sizeof( sQMUX ) + 9)  = clientID;
      
  
  return sizeof( sQMUX ) + 11;
}

int QMICTLReadyReq(
   void *   pBuffer,
   u16      buffSize,
   u8       transactionID )
{
   if (pBuffer == 0 || buffSize < QMICTLReadyReqSize() )
   {
      return -ENOMEM;
   }

   
   
   *(u8 *)(pBuffer + sizeof( sQMUX ))  = 0x00;
   
   *(u8 *)(pBuffer + sizeof( sQMUX ) + 1) = transactionID;
   
   *(u16 *)(pBuffer + sizeof( sQMUX ) + 2) = 0x0021;
   
   *(u16 *)(pBuffer + sizeof( sQMUX ) + 4) = 0x0000;

  
  return sizeof( sQMUX ) + 6;
}

int QMIWDSSetEventReportReq(
   void *   pBuffer,
   u16      buffSize,
   u16      transactionID )
{
   if (pBuffer == 0 || buffSize < QMIWDSSetEventReportReqSize() )
   {
      return -ENOMEM;
   }

   
   
   *(u8 *)(pBuffer + sizeof( sQMUX ))  = 0x00;
   
   *(u16 *)(pBuffer + sizeof( sQMUX ) + 1) = transactionID;
   
   *(u16 *)(pBuffer + sizeof( sQMUX ) + 3) = 0x0001;
   
   *(u16 *)(pBuffer + sizeof( sQMUX ) + 5) = 0x0008;
      
      *(u8 *)(pBuffer + sizeof( sQMUX ) + 7)  = 0x11;
      
      *(u16 *)(pBuffer + sizeof( sQMUX ) + 8) = 0x0005;
      
      *(u8 *)(pBuffer + sizeof( sQMUX ) + 10)  = 0x01;
      
      *(u32 *)(pBuffer + sizeof( sQMUX ) + 11)  = 0x000000ff;

  
  return sizeof( sQMUX ) + 15;
}

int QMIWDSGetPKGSRVCStatusReq(
   void *   pBuffer,
   u16      buffSize,
   u16      transactionID )
{
   if (pBuffer == 0 || buffSize < QMIWDSGetPKGSRVCStatusReqSize() )
   {
      return -ENOMEM;
   }

   
   
   *(u8 *)(pBuffer + sizeof( sQMUX ))  = 0x00;
   
   *(u16 *)(pBuffer + sizeof( sQMUX ) + 1) = transactionID;
   
   *(u16 *)(pBuffer + sizeof( sQMUX ) + 3) = 0x0022;
   
   *(u16 *)(pBuffer + sizeof( sQMUX ) + 5) = 0x0000;

  
  return sizeof( sQMUX ) + 7;
}

int QMIDMSGetMEIDReq(
   void *   pBuffer,
   u16      buffSize,
   u16      transactionID )
{
   if (pBuffer == 0 || buffSize < QMIDMSGetMEIDReqSize() )
   {
      return -ENOMEM;
   }

   
   
   *(u8 *)(pBuffer + sizeof( sQMUX ))  = 0x00;
   
   *(u16 *)(pBuffer + sizeof( sQMUX ) + 1) = transactionID;
   
   *(u16 *)(pBuffer + sizeof( sQMUX ) + 3) = 0x0025;
   
   *(u16 *)(pBuffer + sizeof( sQMUX ) + 5) = 0x0000;

  
  return sizeof( sQMUX ) + 7;
}


int QMICTLGetClientIDResp(
   void * pBuffer,
   u16    buffSize,
   u16 *  pClientID )
{
   int result;
   
   
   
   u8 offset = sizeof( sQMUX ) + 2;

   if (pBuffer == 0 || buffSize < offset )
   {
      return -ENOMEM;
   }

   pBuffer = pBuffer + offset;
   buffSize -= offset;

   result = GetQMIMessageID( pBuffer, buffSize );
   if (result != 0x22)
   {
      return -EFAULT;
   }

   result = ValidQMIMessage( pBuffer, buffSize );
   if (result != 0)
   {
      return -EFAULT;
   }

   result = GetTLV( pBuffer, buffSize, 0x01, pClientID, 2 );
   if (result != 2)
   {
      return -EFAULT;
   }

   return 0;
}

int QMICTLReleaseClientIDResp(
   void *   pBuffer,
   u16      buffSize )
{
   int result;
   
   
   
   u8 offset = sizeof( sQMUX ) + 2;

   if (pBuffer == 0 || buffSize < offset )
   {
      return -ENOMEM;
   }

   pBuffer = pBuffer + offset;
   buffSize -= offset;

   result = GetQMIMessageID( pBuffer, buffSize );
   if (result != 0x23)
   {
      return -EFAULT;
   }

   result = ValidQMIMessage( pBuffer, buffSize );
   if (result != 0)
   {
      return -EFAULT;
   }

   return 0;
}

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
   bool *   pbReconfigure )
{
   int result;
   u8 pktStatusRead[2];

   
   u8 offset = sizeof( sQMUX ) + 3;

   if (pBuffer == 0 
   || buffSize < offset
   || pTXOk == 0
   || pRXOk == 0
   || pTXErr == 0
   || pRXErr == 0
   || pTXOfl == 0
   || pRXOfl == 0
   || pTXBytesOk == 0
   || pRXBytesOk == 0
   || pbLinkState == 0
   || pbReconfigure == 0 )
   {
      return -ENOMEM;
   }

   pBuffer = pBuffer + offset;
   buffSize -= offset;

   

   result = GetQMIMessageID( pBuffer, buffSize );
   
   if (result == 0x01)
   {
      
      GetTLV( pBuffer, buffSize, 0x10, (void*)pTXOk, 4 );
      GetTLV( pBuffer, buffSize, 0x11, (void*)pRXOk, 4 );
      GetTLV( pBuffer, buffSize, 0x12, (void*)pTXErr, 4 );
      GetTLV( pBuffer, buffSize, 0x13, (void*)pRXErr, 4 );
      GetTLV( pBuffer, buffSize, 0x14, (void*)pTXOfl, 4 );
      GetTLV( pBuffer, buffSize, 0x15, (void*)pRXOfl, 4 );
      GetTLV( pBuffer, buffSize, 0x19, (void*)pTXBytesOk, 8 );
      GetTLV( pBuffer, buffSize, 0x1A, (void*)pRXBytesOk, 8 );
   }
   
   else if (result == 0x22)
   {
      result = GetTLV( pBuffer, buffSize, 0x01, &pktStatusRead[0], 2 );
      
      if (result >= 1)
      {
         if (pktStatusRead[0] == 0x02)
         {
            *pbLinkState = true;
         }
         else
         {
            *pbLinkState = false;
         }
      }
      if (result == 2)
      {
         if (pktStatusRead[1] == 0x01)
         {
            *pbReconfigure = true;
         }
         else
         {
            *pbReconfigure = false;
         }
      }
      
      if (result < 0)
      {
         return result;
      }
   }
   else
   {
      return -EFAULT;
   }

   return 0;
}

int QMIDMSGetMEIDResp(
   void *   pBuffer,
   u16      buffSize,
   char *   pMEID,
   int      meidSize )
{
   int result;

   
   u8 offset = sizeof( sQMUX ) + 3;

   if (pBuffer == 0 || buffSize < offset || meidSize < 14 )
   {
      return -ENOMEM;
   }

   pBuffer = pBuffer + offset;
   buffSize -= offset;

   result = GetQMIMessageID( pBuffer, buffSize );
   if (result != 0x25)
   {
      return -EFAULT;
   }

   result = ValidQMIMessage( pBuffer, buffSize );
   if (result != 0)
   {
      return -EFAULT;
   }

   result = GetTLV( pBuffer, buffSize, 0x12, (void*)pMEID, 14 );
   if (result != 14)
   {
      return -EFAULT;
   }

   return 0;
}

