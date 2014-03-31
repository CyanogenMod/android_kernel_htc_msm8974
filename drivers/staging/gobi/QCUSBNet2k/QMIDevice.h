/*===========================================================================
FILE:
   QMIDevice.h

DESCRIPTION:
   Functions related to the QMI interface device
   
FUNCTIONS:
   Generic functions
      IsDeviceValid
      PrintHex
      QSetDownReason
      QClearDownReason
      QTestDownReason

   Driver level asynchronous read functions
      ReadCallback
      IntCallback
      StartRead
      KillRead

   Internal read/write functions
      ReadAsync
      UpSem
      ReadSync
      WriteSyncCallback
      WriteSync

   Internal memory management functions
      GetClientID
      ReleaseClientID
      FindClientMem
      AddToReadMemList
      PopFromReadMemList
      AddToNotifyList
      NotifyAndPopNotifyList
      AddToURBList
      PopFromURBList

   Userspace wrappers
      UserspaceOpen
      UserspaceIOCTL
      UserspaceClose
      UserspaceRead
      UserspaceWrite

   Initializer and destructor
      RegisterQMIDevice
      DeregisterQMIDevice

   Driver level client management
      QMIReady
      QMIWDSCallback
      SetupQMIWDSCallback
      QMIDMSGetMEID

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

#include "Structs.h"
#include "QMI.h"


bool IsDeviceValid( sQCUSBNet * pDev );

void PrintHex(
   void *         pBuffer,
   u16            bufSize );

void QSetDownReason(
   sQCUSBNet *    pDev,
   u8             reason );

void QClearDownReason(
   sQCUSBNet *    pDev,
   u8             reason );

bool QTestDownReason(
   sQCUSBNet *    pDev,
   u8             reason );


void ReadCallback( struct urb * pReadURB );

void IntCallback( struct urb * pIntURB );

int StartRead( sQCUSBNet * pDev );

void KillRead( sQCUSBNet * pDev );


int ReadAsync(
   sQCUSBNet *    pDev,
   u16            clientID,
   u16            transactionID,
   void           (*pCallback)(sQCUSBNet *, u16, void *),
   void *         pData );

void UpSem( 
   sQCUSBNet *    pDev,
   u16            clientID,
   void *         pData );

int ReadSync(
   sQCUSBNet *    pDev,
   void **        ppOutBuffer,
   u16            clientID,
   u16            transactionID );

void WriteSyncCallback( struct urb * pWriteURB );

int WriteSync(
   sQCUSBNet *    pDev,
   char *         pInWriteBuffer,
   int            size,
   u16            clientID );


int GetClientID( 
   sQCUSBNet *      pDev,
   u8               serviceType );

void ReleaseClientID(
   sQCUSBNet *      pDev,
   u16              clientID );

sClientMemList * FindClientMem(
   sQCUSBNet *      pDev,
   u16              clientID );

bool AddToReadMemList( 
   sQCUSBNet *      pDev,
   u16              clientID,
   u16              transactionID,
   void *           pData,
   u16              dataSize );

bool PopFromReadMemList( 
   sQCUSBNet *      pDev,
   u16              clientID,
   u16              transactionID,
   void **          ppData,
   u16 *            pDataSize );

bool AddToNotifyList( 
   sQCUSBNet *      pDev,
   u16              clientID,
   u16              transactionID,
   void             (* pNotifyFunct)(sQCUSBNet *, u16, void *),
   void *           pData );

bool NotifyAndPopNotifyList( 
   sQCUSBNet *      pDev,
   u16              clientID,
   u16              transactionID );

bool AddToURBList( 
   sQCUSBNet *      pDev,
   u16              clientID,
   struct urb *     pURB );

struct urb * PopFromURBList( 
   sQCUSBNet *      pDev,
   u16              clientID );


int UserspaceOpen( 
   struct inode *   pInode, 
   struct file *    pFilp );

int UserspaceIOCTL( 
   struct inode *    pUnusedInode, 
   struct file *     pFilp,
   unsigned int      cmd, 
   unsigned long     arg );

int UserspaceClose( 
   struct file *       pFilp,
   fl_owner_t          unusedFileTable );

ssize_t UserspaceRead( 
   struct file *        pFilp,
   char __user *        pBuf, 
   size_t               size,
   loff_t *             pUnusedFpos );

ssize_t UserspaceWrite(
   struct file *        pFilp, 
   const char __user *  pBuf, 
   size_t               size,
   loff_t *             pUnusedFpos );


int RegisterQMIDevice( sQCUSBNet * pDev );

void DeregisterQMIDevice( sQCUSBNet * pDev );


bool QMIReady(
   sQCUSBNet *    pDev,
   u16            timeout );

void QMIWDSCallback(
   sQCUSBNet *    pDev,
   u16            clientID,
   void *         pData );

int SetupQMIWDSCallback( sQCUSBNet * pDev );

int QMIDMSGetMEID( sQCUSBNet * pDev );



