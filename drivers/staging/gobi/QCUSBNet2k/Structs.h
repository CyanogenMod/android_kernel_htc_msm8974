/*===========================================================================
FILE:
   Structs.h

DESCRIPTION:
   Declaration of structures used by the Qualcomm Linux USB Network driver
   
FUNCTIONS:
   none

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

#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/usb.h>
#include <linux/version.h>
#include <linux/cdev.h>
#include <linux/kthread.h>

#if (LINUX_VERSION_CODE <= KERNEL_VERSION( 2,6,24 ))
   #include "usbnet.h"
#else
   #include <linux/usb/usbnet.h>
#endif

#if (LINUX_VERSION_CODE > KERNEL_VERSION( 2,6,25 ))
   #include <linux/fdtable.h>
#else
   #include <linux/file.h>
#endif

#define DBG( format, arg... ) \
   if (debug == 1)\
   { \
      printk( KERN_INFO "QCUSBNet2k::%s " format, __FUNCTION__, ## arg ); \
   } \


struct sQCUSBNet;

typedef struct sReadMemList
{
   
   void *                     mpData;
   
   
   u16                        mTransactionID;

   
   u16                        mDataSize;

   
   struct sReadMemList *      mpNext;

} sReadMemList;

typedef struct sNotifyList
{
   
   void                  (* mpNotifyFunct)(struct sQCUSBNet *, u16, void *);
   
   
   u16                   mTransactionID;

   
   void *                mpData;
   
   
   struct sNotifyList *  mpNext;

} sNotifyList;

typedef struct sURBList
{
   
   struct urb *       mpURB;

   
   struct sURBList *  mpNext;

} sURBList;

typedef struct sClientMemList
{
   
   u16                          mClientID;

   
   
   sReadMemList *               mpList;
   
   
   sNotifyList *                mpReadNotifyList;

   
   sURBList *                   mpURBList;
   
   
   struct sClientMemList *      mpNext;

} sClientMemList;

typedef struct sURBSetupPacket
{
   
   u8    mRequestType;

   
   u8    mRequestCode;

   
   u16   mValue;

   
   u16   mIndex;

   
   u16   mLength;

} sURBSetupPacket;

#define DEFAULT_READ_URB_LENGTH 0x1000


typedef struct sAutoPM
{
   
   struct task_struct *       mpThread;

   
   struct semaphore           mThreadDoWork;

   
   bool                       mbExit;

   
   sURBList *                 mpURBList;

   
   spinlock_t                 mURBListLock;
   
   
   struct urb *               mpActiveURB;

   
   spinlock_t                 mActiveURBLock;
   
   
   struct usb_interface *     mpIntf;

} sAutoPM;


typedef struct sQMIDev
{
   
   dev_t                      mDevNum;

   
   struct class *             mpDevClass;

   
   struct cdev                mCdev;

   
   struct urb *               mpReadURB;

   
   sURBSetupPacket *          mpReadSetupPacket;

   
   void *                     mpReadBuffer;
   
   
   
   struct urb *               mpIntURB;

   
   void *                     mpIntBuffer;
   
   
   sClientMemList *           mpClientMemList;
   
   
   spinlock_t                 mClientMemLock;

   
   atomic_t                   mQMICTLTransactionID;

} sQMIDev;

typedef struct sQCUSBNet
{
   
   struct usbnet *        mpNetDev;

   
   struct usb_interface * mpIntf;
   
   
   int                  (* mpUSBNetOpen)(struct net_device *);
   int                  (* mpUSBNetStop)(struct net_device *);
   
   
   
   unsigned long          mDownReason;
#define NO_NDIS_CONNECTION    0
#define CDC_CONNECTION_SPEED  1
#define DRIVER_SUSPENDED      2
#define NET_IFACE_STOPPED     3

   
   bool                   mbQMIValid;

   
   sQMIDev                mQMIDev;

   
   char                   mMEID[14];
   
   
   sAutoPM                mAutoPM;

} sQCUSBNet;

typedef struct sQMIFilpStorage
{
   
   u16                  mClientID;
   
   
   sQCUSBNet *          mpDev;

} sQMIFilpStorage;


