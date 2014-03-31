/*===========================================================================
FILE:
   QMIDevice.c

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

#include "QMIDevice.h"


extern int debug;

int QCSuspend( 
   struct usb_interface *     pIntf,
   pm_message_t               powerEvent );

#define IOCTL_QMI_GET_SERVICE_FILE 0x8BE0 + 1

#define IOCTL_QMI_GET_DEVICE_VIDPID 0x8BE0 + 2

#define IOCTL_QMI_GET_DEVICE_MEID 0x8BE0 + 3

#define CDC_GET_ENCAPSULATED_RESPONSE 0x01A1ll

#define CDC_CONNECTION_SPEED_CHANGE 0x08000000002AA1ll

struct file_operations UserspaceQMIFops = 
{
   .owner     = THIS_MODULE,
   .read      = UserspaceRead,
   .write     = UserspaceWrite,
   .ioctl     = UserspaceIOCTL,
   .open      = UserspaceOpen,
   .flush     = UserspaceClose,
};


bool IsDeviceValid( sQCUSBNet * pDev )
{
   if (pDev == NULL)
   {
      return false;
   }

   if (pDev->mbQMIValid == false)
   {
      return false;
   }
   
   return true;
} 

void PrintHex(
   void *      pBuffer,
   u16         bufSize )
{
   char * pPrintBuf;
   u16 pos;
   int status;
   
   pPrintBuf = kmalloc( bufSize * 3 + 1, GFP_ATOMIC );
   if (pPrintBuf == NULL)
   {
      DBG( "Unable to allocate buffer\n" );
      return;
   }
   memset( pPrintBuf, 0 , bufSize * 3 + 1 );
   
   for (pos = 0; pos < bufSize; pos++)
   {
      status = snprintf( (pPrintBuf + (pos * 3)), 
                         4, 
                         "%02X ", 
                         *(u8 *)(pBuffer + pos) );
      if (status != 3)
      {
         DBG( "snprintf error %d\n", status );
         return;
      }
   }
   
   DBG( "   : %s\n", pPrintBuf );

   kfree( pPrintBuf );
   pPrintBuf = NULL;
   return;   
}

void QSetDownReason(
   sQCUSBNet *    pDev,
   u8             reason )
{
   set_bit( reason, &pDev->mDownReason );
   
   netif_carrier_off( pDev->mpNetDev->net );
}

void QClearDownReason(
   sQCUSBNet *    pDev,
   u8             reason )
{
   clear_bit( reason, &pDev->mDownReason );
   
   if (pDev->mDownReason == 0)
   {
      netif_carrier_on( pDev->mpNetDev->net );
   }
}

bool QTestDownReason(
   sQCUSBNet *    pDev,
   u8             reason )
{
   return test_bit( reason, &pDev->mDownReason );
}


void ReadCallback( struct urb * pReadURB )
{
   int result;
   u16 clientID;
   sClientMemList * pClientMem;
   void * pData;
   void * pDataCopy;
   u16 dataSize;
   sQCUSBNet * pDev;
   unsigned long flags;
   u16 transactionID;

   if (pReadURB == NULL)
   {
      DBG( "bad read URB\n" );
      return;
   }
   
   pDev = pReadURB->context;
   if (IsDeviceValid( pDev ) == false)
   {
      DBG( "Invalid device!\n" );
      return;
   }   

   if (pReadURB->status != 0)
   {
      DBG( "Read status = %d\n", pReadURB->status );
      return;
   }
   DBG( "Read %d bytes\n", pReadURB->actual_length );
   
   pData = pReadURB->transfer_buffer;
   dataSize = pReadURB->actual_length;

   PrintHex( pData, dataSize );

   result = ParseQMUX( &clientID,
                       pData,
                       dataSize );
   if (result < 0)
   {
      DBG( "Read error parsing QMUX %d\n", result );
      return;
   }
   
   

   
   if (dataSize < result + 3)
   {
      DBG( "Data buffer too small to parse\n" );
      return;
   }
   
   
   if (clientID == QMICTL)
   {
      transactionID = *(u8*)(pData + result + 1);
   }
   else
   {
      transactionID = *(u16*)(pData + result + 1);
   }
   
   
   spin_lock_irqsave( &pDev->mQMIDev.mClientMemLock, flags );

   
   
   pClientMem = pDev->mQMIDev.mpClientMemList;
   while (pClientMem != NULL)
   {
      if (pClientMem->mClientID == clientID 
      ||  (pClientMem->mClientID | 0xff00) == clientID)
      {
         
         pDataCopy = kmalloc( dataSize, GFP_ATOMIC );
         memcpy( pDataCopy, pData, dataSize );

         if (AddToReadMemList( pDev,
                               pClientMem->mClientID,
                               transactionID,
                               pDataCopy,
                               dataSize ) == false)
         {
            DBG( "Error allocating pReadMemListEntry "
                 "read will be discarded\n" );
            kfree( pDataCopy );
            
            
            spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );
            return;
         }

         
         DBG( "Creating new readListEntry for client 0x%04X, TID %x\n",
              clientID,
              transactionID );

         
         NotifyAndPopNotifyList( pDev,
                                 pClientMem->mClientID,
                                 transactionID );

         
         if (clientID >> 8 != 0xff)
         {
            break;
         }
      }
      
      
      pClientMem = pClientMem->mpNext;
   }
   
   
   spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );
}

void IntCallback( struct urb * pIntURB )
{
   int status;
   int interval;
   
   sQCUSBNet * pDev = (sQCUSBNet *)pIntURB->context;
   if (IsDeviceValid( pDev ) == false)
   {
      DBG( "Invalid device!\n" );
      return;
   }

   
   if (pIntURB->status != 0)
   {
      DBG( "Int status = %d\n", pIntURB->status );
      
      
      if (pIntURB->status != -EOVERFLOW)
      {
         
         return;
      }
   }
   else
   {
      
      if ((pIntURB->actual_length == 8)
      &&  (*(u64*)pIntURB->transfer_buffer == CDC_GET_ENCAPSULATED_RESPONSE))
      {
         
         usb_fill_control_urb( pDev->mQMIDev.mpReadURB,
                               pDev->mpNetDev->udev,
                               usb_rcvctrlpipe( pDev->mpNetDev->udev, 0 ),
                               (unsigned char *)pDev->mQMIDev.mpReadSetupPacket,
                               pDev->mQMIDev.mpReadBuffer,
                               DEFAULT_READ_URB_LENGTH,
                               ReadCallback,
                               pDev );
         status = usb_submit_urb( pDev->mQMIDev.mpReadURB, GFP_ATOMIC );
         if (status != 0)
         {
            DBG( "Error submitting Read URB %d\n", status );
            return;
         }
      }
      
      else if ((pIntURB->actual_length == 16)
      &&       (*(u64*)pIntURB->transfer_buffer == CDC_CONNECTION_SPEED_CHANGE))
      {
         
         if ((*(u32*)(pIntURB->transfer_buffer + 8) == 0)
         ||  (*(u32*)(pIntURB->transfer_buffer + 12) == 0))
         {
            QSetDownReason( pDev, CDC_CONNECTION_SPEED );
            DBG( "traffic stopping due to CONNECTION_SPEED_CHANGE\n" );
         }
         else
         {
            QClearDownReason( pDev, CDC_CONNECTION_SPEED );
            DBG( "resuming traffic due to CONNECTION_SPEED_CHANGE\n" );
         }
      }
      else
      {
         DBG( "ignoring invalid interrupt in packet\n" );
         PrintHex( pIntURB->transfer_buffer, pIntURB->actual_length );
      }
   }

   interval = (pDev->mpNetDev->udev->speed == USB_SPEED_HIGH) ? 7 : 3;

   
   usb_fill_int_urb( pIntURB,
                     pIntURB->dev,
                     pIntURB->pipe,
                     pIntURB->transfer_buffer,
                     pIntURB->transfer_buffer_length,
                     pIntURB->complete,
                     pIntURB->context,
                     interval );
   status = usb_submit_urb( pIntURB, GFP_ATOMIC );
   if (status != 0)
   {
      DBG( "Error re-submitting Int URB %d\n", status );
   }   
   return;
}

int StartRead( sQCUSBNet * pDev )
{
   int interval;

   if (IsDeviceValid( pDev ) == false)
   {
      DBG( "Invalid device!\n" );
      return -ENXIO;
   }
   
   
   pDev->mQMIDev.mpReadURB = usb_alloc_urb( 0, GFP_KERNEL );
   if (pDev->mQMIDev.mpReadURB == NULL)
   {
      DBG( "Error allocating read urb\n" );
      return -ENOMEM;
   }
   
   pDev->mQMIDev.mpIntURB = usb_alloc_urb( 0, GFP_KERNEL );
   if (pDev->mQMIDev.mpIntURB == NULL)
   {
      DBG( "Error allocating int urb\n" );
      return -ENOMEM;
   }

   
   pDev->mQMIDev.mpReadBuffer = kmalloc( DEFAULT_READ_URB_LENGTH, GFP_KERNEL );
   if (pDev->mQMIDev.mpReadBuffer == NULL)
   {
      DBG( "Error allocating read buffer\n" );
      return -ENOMEM;
   }
   
   pDev->mQMIDev.mpIntBuffer = kmalloc( DEFAULT_READ_URB_LENGTH, GFP_KERNEL );
   if (pDev->mQMIDev.mpIntBuffer == NULL)
   {
      DBG( "Error allocating int buffer\n" );
      return -ENOMEM;
   }      
   
   pDev->mQMIDev.mpReadSetupPacket = kmalloc( sizeof( sURBSetupPacket ), 
                                              GFP_KERNEL );
   if (pDev->mQMIDev.mpReadSetupPacket == NULL)
   {
      DBG( "Error allocating setup packet buffer\n" );
      return -ENOMEM;
   }

   
   pDev->mQMIDev.mpReadSetupPacket->mRequestType = 0xA1;
   pDev->mQMIDev.mpReadSetupPacket->mRequestCode = 1;
   pDev->mQMIDev.mpReadSetupPacket->mValue = 0;
   pDev->mQMIDev.mpReadSetupPacket->mIndex = 0;
   pDev->mQMIDev.mpReadSetupPacket->mLength = DEFAULT_READ_URB_LENGTH;

   interval = (pDev->mpNetDev->udev->speed == USB_SPEED_HIGH) ? 7 : 3;
   
   
   usb_fill_int_urb( pDev->mQMIDev.mpIntURB,
                     pDev->mpNetDev->udev,
                     usb_rcvintpipe( pDev->mpNetDev->udev, 0x81 ),
                     pDev->mQMIDev.mpIntBuffer,
                     DEFAULT_READ_URB_LENGTH,
                     IntCallback,
                     pDev,
                     interval );
   return usb_submit_urb( pDev->mQMIDev.mpIntURB, GFP_KERNEL );
}

void KillRead( sQCUSBNet * pDev )
{
   
   if (pDev->mQMIDev.mpReadURB != NULL)
   {
      DBG( "Killng read URB\n" );
      usb_kill_urb( pDev->mQMIDev.mpReadURB );
   }

   if (pDev->mQMIDev.mpIntURB != NULL)
   {
      DBG( "Killng int URB\n" );
      usb_kill_urb( pDev->mQMIDev.mpIntURB );
   }

   
   kfree( pDev->mQMIDev.mpReadSetupPacket );
   pDev->mQMIDev.mpReadSetupPacket = NULL;
   kfree( pDev->mQMIDev.mpReadBuffer );
   pDev->mQMIDev.mpReadBuffer = NULL;
   kfree( pDev->mQMIDev.mpIntBuffer );
   pDev->mQMIDev.mpIntBuffer = NULL;
   
   
   usb_free_urb( pDev->mQMIDev.mpReadURB );
   pDev->mQMIDev.mpReadURB = NULL;
   usb_free_urb( pDev->mQMIDev.mpIntURB );
   pDev->mQMIDev.mpIntURB = NULL;
}


int ReadAsync(
   sQCUSBNet *    pDev,
   u16            clientID,
   u16            transactionID,
   void           (*pCallback)(sQCUSBNet*, u16, void *),
   void *         pData )
{
   sClientMemList * pClientMem;
   sReadMemList ** ppReadMemList;
   
   unsigned long flags;

   if (IsDeviceValid( pDev ) == false)
   {
      DBG( "Invalid device!\n" );
      return -ENXIO;
   }

   
   spin_lock_irqsave( &pDev->mQMIDev.mClientMemLock, flags );

   
   pClientMem = FindClientMem( pDev, clientID );
   if (pClientMem == NULL)
   {
      DBG( "Could not find matching client ID 0x%04X\n",
           clientID );
           
      
      spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );
      return -ENXIO;
   }
   
   ppReadMemList = &(pClientMem->mpList);
   
   
   while (*ppReadMemList != NULL)
   {
      
      if (transactionID == 0 
      ||  transactionID == (*ppReadMemList)->mTransactionID)
      {
         
         spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );

         
         pCallback( pDev, clientID, pData );
         
         return 0;
      }
      
      
      ppReadMemList = &(*ppReadMemList)->mpNext;
   }

   
   if (AddToNotifyList( pDev,
                        clientID,
                        transactionID, 
                        pCallback, 
                        pData ) == false)
   {
      DBG( "Unable to register for notification\n" );
   }

   
   spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );

   
   return 0;
}

void UpSem( 
   sQCUSBNet * pDev,
   u16         clientID,
   void *      pData )
{
   DBG( "0x%04X\n", clientID );
        
   up( (struct semaphore *)pData );
   return;
}

int ReadSync(
   sQCUSBNet *    pDev,
   void **        ppOutBuffer,
   u16            clientID,
   u16            transactionID )
{
   int result;
   sClientMemList * pClientMem;
   sNotifyList ** ppNotifyList, * pDelNotifyListEntry;
   struct semaphore readSem;
   void * pData;
   unsigned long flags;
   u16 dataSize;

   if (IsDeviceValid( pDev ) == false)
   {
      DBG( "Invalid device!\n" );
      return -ENXIO;
   }
   
   
   spin_lock_irqsave( &pDev->mQMIDev.mClientMemLock, flags );

   
   pClientMem = FindClientMem( pDev, clientID );
   if (pClientMem == NULL)
   {
      DBG( "Could not find matching client ID 0x%04X\n",
           clientID );
      
      
      spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );
      return -ENXIO;
   }
   
   
   
   while (PopFromReadMemList( pDev,
                              clientID,
                              transactionID,
                              &pData,
                              &dataSize ) == false)
   {
      
      sema_init( &readSem, 0 );

      
      if (AddToNotifyList( pDev, 
                           clientID, 
                           transactionID, 
                           UpSem, 
                           &readSem ) == false)
      {
         DBG( "unable to register for notification\n" );
         spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );
         return -EFAULT;
      }

      
      spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );

      
      result = down_interruptible( &readSem );
      if (result != 0)
      {
         DBG( "Interrupted %d\n", result );

         
         
         spin_lock_irqsave( &pDev->mQMIDev.mClientMemLock, flags );
         ppNotifyList = &(pClientMem->mpReadNotifyList);
         pDelNotifyListEntry = NULL;

         
         while (*ppNotifyList != NULL)
         {
            if ((*ppNotifyList)->mpData == &readSem)
            {
               pDelNotifyListEntry = *ppNotifyList;
               *ppNotifyList = (*ppNotifyList)->mpNext;
               kfree( pDelNotifyListEntry );
               break;
            }

            
            ppNotifyList = &(*ppNotifyList)->mpNext;
         }

         spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );
         return -EINTR;
      }
      
      
      if (IsDeviceValid( pDev ) == false)
      {
         DBG( "Invalid device!\n" );
         return -ENXIO;
      }
      
      
      spin_lock_irqsave( &pDev->mQMIDev.mClientMemLock, flags );
   }
   
   
   spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );

   
   *ppOutBuffer = pData;

   return dataSize;
}

void WriteSyncCallback( struct urb * pWriteURB )
{
   if (pWriteURB == NULL)
   {
      DBG( "null urb\n" );
      return;
   }

   DBG( "Write status/size %d/%d\n", 
        pWriteURB->status, 
        pWriteURB->actual_length );

   
   up( (struct semaphore * )pWriteURB->context );
   
   return;
}

/*===========================================================================
METHOD:
   WriteSync (Public Method)

DESCRIPTION:
   Start synchronous write

PARAMETERS:
   pDev                 [ I ] - Device specific memory
   pWriteBuffer         [ I ] - Data to be written
   writeBufferSize      [ I ] - Size of data to be written
   clientID             [ I ] - Client ID of requester

RETURN VALUE:
   int - write size (includes QMUX)
         negative errno for failure
===========================================================================*/
int WriteSync(
   sQCUSBNet *        pDev,
   char *             pWriteBuffer,
   int                writeBufferSize,
   u16                clientID )
{
   int result;
   struct semaphore writeSem;
   struct urb * pWriteURB;
   sURBSetupPacket writeSetup;
   unsigned long flags;

   if (IsDeviceValid( pDev ) == false)
   {
      DBG( "Invalid device!\n" );
      return -ENXIO;
   }

   pWriteURB = usb_alloc_urb( 0, GFP_KERNEL );
   if (pWriteURB == NULL)
   {
      DBG( "URB mem error\n" );
      return -ENOMEM;
   }

   
   result = FillQMUX( clientID, pWriteBuffer, writeBufferSize );
   if (result < 0)
   {
      usb_free_urb( pWriteURB );
      return result;
   }

   
   writeSetup.mRequestType = 0x21;
   writeSetup.mRequestCode = 0;
   writeSetup.mValue = 0;
   writeSetup.mIndex = 0;
   writeSetup.mLength = 0;
   writeSetup.mLength = writeBufferSize;

   
   usb_fill_control_urb( pWriteURB,
                         pDev->mpNetDev->udev,
                         usb_sndctrlpipe( pDev->mpNetDev->udev, 0 ),
                         (unsigned char *)&writeSetup,
                         (void*)pWriteBuffer,
                         writeBufferSize,
                         NULL,
                         pDev );

   DBG( "Actual Write:\n" );
   PrintHex( pWriteBuffer, writeBufferSize );

   sema_init( &writeSem, 0 );
   
   pWriteURB->complete = WriteSyncCallback;
   pWriteURB->context = &writeSem;
   
   
   result = usb_autopm_get_interface( pDev->mpIntf );
   if (result < 0)
   {
      DBG( "unable to resume interface: %d\n", result );
      
      
      if (result == -EPERM)
      {
#if (LINUX_VERSION_CODE < KERNEL_VERSION( 2,6,33 ))
         pDev->mpNetDev->udev->auto_pm = 0;
#endif
         QCSuspend( pDev->mpIntf, PMSG_SUSPEND );
      }

      return result;
   }

   
   spin_lock_irqsave( &pDev->mQMIDev.mClientMemLock, flags );

   if (AddToURBList( pDev, clientID, pWriteURB ) == false)
   {
      usb_free_urb( pWriteURB );

      
      spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );   
      usb_autopm_put_interface( pDev->mpIntf );
      return -EINVAL;
   }

   result = usb_submit_urb( pWriteURB, GFP_KERNEL );
   if (result < 0)
   {
      DBG( "submit URB error %d\n", result );
      
      
      if (PopFromURBList( pDev, clientID ) != pWriteURB)
      {
         
         DBG( "Didn't get write URB back\n" );
      }

      usb_free_urb( pWriteURB );

      
      spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );
      usb_autopm_put_interface( pDev->mpIntf );
      return result;
   }
   
   
   spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );   

   
   result = down_interruptible( &writeSem );

   
   if (IsDeviceValid( pDev ) == false)
   {
      DBG( "Invalid device!\n" );
      return -ENXIO;
   }

   
   usb_autopm_put_interface( pDev->mpIntf );

   
   spin_lock_irqsave( &pDev->mQMIDev.mClientMemLock, flags );

   
   if (PopFromURBList( pDev, clientID ) != pWriteURB)
   {
      
      DBG( "Didn't get write URB back\n" );
   
      
      spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );   
      return -EINVAL;
   }

   
   spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );   

   if (result == 0)
   {
      
      if (pWriteURB->status == 0)
      {
         // Return number of bytes that were supposed to have been written,
         
         result = writeBufferSize;
      }
      else
      {
         DBG( "bad status = %d\n", pWriteURB->status );
         
         
         result = pWriteURB->status;
      }
   }
   else
   {
      
      DBG( "Interrupted %d !!!\n", result );
      DBG( "Device may be in bad state and need reset !!!\n" );

      
      usb_kill_urb( pWriteURB );
   }

   usb_free_urb( pWriteURB );

   return result;
}


int GetClientID( 
   sQCUSBNet *    pDev,
   u8             serviceType )
{
   u16 clientID;
   sClientMemList ** ppClientMem;
   int result;
   void * pWriteBuffer;
   u16 writeBufferSize;
   void * pReadBuffer;
   u16 readBufferSize;
   unsigned long flags;
   u8 transactionID;
   
   if (IsDeviceValid( pDev ) == false)
   {
      DBG( "Invalid device!\n" );
      return -ENXIO;
   }

   
   if (serviceType != 0)
   {
      writeBufferSize = QMICTLGetClientIDReqSize();
      pWriteBuffer = kmalloc( writeBufferSize, GFP_KERNEL );
      if (pWriteBuffer == NULL)
      {
         return -ENOMEM;
      }

      transactionID = atomic_add_return( 1, &pDev->mQMIDev.mQMICTLTransactionID );
      if (transactionID == 0)
      {
         atomic_add_return( 1, &pDev->mQMIDev.mQMICTLTransactionID );
      }
      result = QMICTLGetClientIDReq( pWriteBuffer, 
                                     writeBufferSize,
                                     transactionID,
                                     serviceType );
      if (result < 0)
      {
         kfree( pWriteBuffer );
         return result;
      }
      
      result = WriteSync( pDev,
                          pWriteBuffer,
                          writeBufferSize,
                          QMICTL );
      kfree( pWriteBuffer );

      if (result < 0)
      {
         return result;
      }

      result = ReadSync( pDev,
                         &pReadBuffer,
                         QMICTL,
                         transactionID );
      if (result < 0)
      {
         DBG( "bad read data %d\n", result );
         return result;
      }
      readBufferSize = result;

      result = QMICTLGetClientIDResp( pReadBuffer,
                                      readBufferSize,
                                      &clientID );
      kfree( pReadBuffer );

      if (result < 0)
      {
         return result;
      }
   }
   else
   {
      
      clientID = 0;
   }

   
   spin_lock_irqsave( &pDev->mQMIDev.mClientMemLock, flags );

   
   if (FindClientMem( pDev, clientID ) != NULL)
   {
      DBG( "Client memory already exists\n" );

      
      spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );
      return -ETOOMANYREFS;
   }

   
   ppClientMem = &pDev->mQMIDev.mpClientMemList;
   while (*ppClientMem != NULL)
   {
      ppClientMem = &(*ppClientMem)->mpNext;
   }
   
   
   *ppClientMem = kmalloc( sizeof( sClientMemList ), GFP_ATOMIC );
   if (*ppClientMem == NULL)
   {
      DBG( "Error allocating read list\n" );

      
      spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );
      return -ENOMEM;
   }
      
   (*ppClientMem)->mClientID = clientID;
   (*ppClientMem)->mpList = NULL;
   (*ppClientMem)->mpReadNotifyList = NULL;
   (*ppClientMem)->mpURBList = NULL;
   (*ppClientMem)->mpNext = NULL;


   
   spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );
   
   return clientID;
}

void ReleaseClientID(
   sQCUSBNet *    pDev,
   u16            clientID )
{
   int result;
   sClientMemList ** ppDelClientMem;
   sClientMemList * pNextClientMem;
   struct urb * pDelURB;
   void * pDelData;
   u16 dataSize;
   void * pWriteBuffer;
   u16 writeBufferSize;
   void * pReadBuffer;
   u16 readBufferSize;
   unsigned long flags;
   u8 transactionID;

   
   if (IsDeviceValid( pDev ) == false)
   {
      DBG( "invalid device\n" );
      return;
   }
   
   DBG( "releasing 0x%04X\n", clientID );

   
   if (clientID != QMICTL)
   {
      
      
      
      writeBufferSize = QMICTLReleaseClientIDReqSize();
      pWriteBuffer = kmalloc( writeBufferSize, GFP_KERNEL );
      if (pWriteBuffer == NULL)
      {
         DBG( "memory error\n" );
      }
      else
      {
         transactionID = atomic_add_return( 1, &pDev->mQMIDev.mQMICTLTransactionID );
         if (transactionID == 0)
         {
            transactionID = atomic_add_return( 1, &pDev->mQMIDev.mQMICTLTransactionID );
         }
         result = QMICTLReleaseClientIDReq( pWriteBuffer, 
                                            writeBufferSize,
                                            transactionID,
                                            clientID );
         if (result < 0)
         {
            kfree( pWriteBuffer );
            DBG( "error %d filling req buffer\n", result );
         }
         else
         {
            result = WriteSync( pDev,
                                pWriteBuffer,
                                writeBufferSize,
                                QMICTL );
            kfree( pWriteBuffer );

            if (result < 0)
            {
               DBG( "bad write status %d\n", result );
            }
            else
            {
               result = ReadSync( pDev,
                                  &pReadBuffer,
                                  QMICTL,
                                  transactionID );
               if (result < 0)
               {
                  DBG( "bad read status %d\n", result );
               }
               else
               {
                  readBufferSize = result;

                  result = QMICTLReleaseClientIDResp( pReadBuffer,
                                                      readBufferSize );
                  kfree( pReadBuffer );

                  if (result < 0)
                  {
                     DBG( "error %d parsing response\n", result );
                  }
               }
            }
         }
      }
   }

   
   
   
   spin_lock_irqsave( &pDev->mQMIDev.mClientMemLock, flags );

   
   ppDelClientMem = &pDev->mQMIDev.mpClientMemList;
   while (*ppDelClientMem != NULL)
   {
      if ((*ppDelClientMem)->mClientID == clientID)
      {
         pNextClientMem = (*ppDelClientMem)->mpNext;

         
         while (NotifyAndPopNotifyList( pDev,
                                        clientID,
                                        0 ) == true );         

         
         pDelURB = PopFromURBList( pDev, clientID );
         while (pDelURB != NULL)
         {
            usb_kill_urb( pDelURB );
            usb_free_urb( pDelURB );
            pDelURB = PopFromURBList( pDev, clientID );
         }

         
         while (PopFromReadMemList( pDev, 
                                    clientID,
                                    0,
                                    &pDelData,
                                    &dataSize ) == true )
         {
            kfree( pDelData );
         }

         
         kfree( *ppDelClientMem );

         
         *ppDelClientMem = pNextClientMem;
      }
      else
      {
         
         ppDelClientMem = &(*ppDelClientMem)->mpNext;
      }
   }
   
   
   spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );

   return;
}

sClientMemList * FindClientMem( 
   sQCUSBNet *      pDev,
   u16              clientID )
{
   sClientMemList * pClientMem;
   
   if (IsDeviceValid( pDev ) == false)
   {
      DBG( "Invalid device\n" );
      return NULL;
   }
   
#ifdef CONFIG_SMP
   
   if (spin_is_locked( &pDev->mQMIDev.mClientMemLock ) == 0)
   {
      DBG( "unlocked\n" );
      BUG();
   }
#endif
   
   pClientMem = pDev->mQMIDev.mpClientMemList;
   while (pClientMem != NULL)
   {
      if (pClientMem->mClientID == clientID)
      {
         
         
         return pClientMem;
      }
      
      pClientMem = pClientMem->mpNext;
   }

   DBG( "Could not find client mem 0x%04X\n", clientID );
   return NULL;
}

bool AddToReadMemList( 
   sQCUSBNet *      pDev,
   u16              clientID,
   u16              transactionID,
   void *           pData,
   u16              dataSize )
{
   sClientMemList * pClientMem;
   sReadMemList ** ppThisReadMemList;

#ifdef CONFIG_SMP
   
   if (spin_is_locked( &pDev->mQMIDev.mClientMemLock ) == 0)
   {
      DBG( "unlocked\n" );
      BUG();
   }
#endif

   
   pClientMem = FindClientMem( pDev, clientID );
   if (pClientMem == NULL)
   {
      DBG( "Could not find this client's memory 0x%04X\n",
           clientID );

      return false;
   }

   
   ppThisReadMemList = &pClientMem->mpList;
   while (*ppThisReadMemList != NULL)
   {
      ppThisReadMemList = &(*ppThisReadMemList)->mpNext;
   }
   
   *ppThisReadMemList = kmalloc( sizeof( sReadMemList ), GFP_ATOMIC );
   if (*ppThisReadMemList == NULL)
   {
      DBG( "Mem error\n" );

      return false;
   }   
   
   (*ppThisReadMemList)->mpNext = NULL;
   (*ppThisReadMemList)->mpData = pData;
   (*ppThisReadMemList)->mDataSize = dataSize;
   (*ppThisReadMemList)->mTransactionID = transactionID;
   
   return true;
}

bool PopFromReadMemList( 
   sQCUSBNet *      pDev,
   u16              clientID,
   u16              transactionID,
   void **          ppData,
   u16 *            pDataSize )
{
   sClientMemList * pClientMem;
   sReadMemList * pDelReadMemList, ** ppReadMemList;

#ifdef CONFIG_SMP
   
   if (spin_is_locked( &pDev->mQMIDev.mClientMemLock ) == 0)
   {
      DBG( "unlocked\n" );
      BUG();
   }
#endif

   
   pClientMem = FindClientMem( pDev, clientID );
   if (pClientMem == NULL)
   {
      DBG( "Could not find this client's memory 0x%04X\n",
           clientID );

      return false;
   }
   
   ppReadMemList = &(pClientMem->mpList);
   pDelReadMemList = NULL;
   
   
   while (*ppReadMemList != NULL)
   {
      
      if (transactionID == 0
      ||  transactionID == (*ppReadMemList)->mTransactionID )
      {
         pDelReadMemList = *ppReadMemList;
         break;
      }
      
      DBG( "skipping 0x%04X data TID = %x\n", clientID, (*ppReadMemList)->mTransactionID );
      
      
      ppReadMemList = &(*ppReadMemList)->mpNext;
   }
   
   if (pDelReadMemList != NULL)
   {
      *ppReadMemList = (*ppReadMemList)->mpNext;
      
      
      *ppData = pDelReadMemList->mpData;
      *pDataSize = pDelReadMemList->mDataSize;
      
      
      kfree( pDelReadMemList );
      
      return true;
   }
   else
   {
      DBG( "No read memory to pop, Client 0x%04X, TID = %x\n", 
           clientID, 
           transactionID );
      return false;
   }
}

bool AddToNotifyList( 
   sQCUSBNet *      pDev,
   u16              clientID,
   u16              transactionID,
   void             (* pNotifyFunct)(sQCUSBNet *, u16, void *),
   void *           pData )
{
   sClientMemList * pClientMem;
   sNotifyList ** ppThisNotifyList;

#ifdef CONFIG_SMP
   
   if (spin_is_locked( &pDev->mQMIDev.mClientMemLock ) == 0)
   {
      DBG( "unlocked\n" );
      BUG();
   }
#endif

   
   pClientMem = FindClientMem( pDev, clientID );
   if (pClientMem == NULL)
   {
      DBG( "Could not find this client's memory 0x%04X\n", clientID );
      return false;
   }

   
   ppThisNotifyList = &pClientMem->mpReadNotifyList;
   while (*ppThisNotifyList != NULL)
   {
      ppThisNotifyList = &(*ppThisNotifyList)->mpNext;
   }
   
   *ppThisNotifyList = kmalloc( sizeof( sNotifyList ), GFP_ATOMIC );
   if (*ppThisNotifyList == NULL)
   {
      DBG( "Mem error\n" );
      return false;
   }   
   
   (*ppThisNotifyList)->mpNext = NULL;
   (*ppThisNotifyList)->mpNotifyFunct = pNotifyFunct;
   (*ppThisNotifyList)->mpData = pData;
   (*ppThisNotifyList)->mTransactionID = transactionID;
   
   return true;
}

bool NotifyAndPopNotifyList( 
   sQCUSBNet *      pDev,
   u16              clientID,
   u16              transactionID )
{
   sClientMemList * pClientMem;
   sNotifyList * pDelNotifyList, ** ppNotifyList;

#ifdef CONFIG_SMP
   
   if (spin_is_locked( &pDev->mQMIDev.mClientMemLock ) == 0)
   {
      DBG( "unlocked\n" );
      BUG();
   }
#endif

   
   pClientMem = FindClientMem( pDev, clientID );
   if (pClientMem == NULL)
   {
      DBG( "Could not find this client's memory 0x%04X\n", clientID );
      return false;
   }

   ppNotifyList = &(pClientMem->mpReadNotifyList);
   pDelNotifyList = NULL;

   
   while (*ppNotifyList != NULL)
   {
      
      if (transactionID == 0
      ||  (*ppNotifyList)->mTransactionID == 0
      ||  transactionID == (*ppNotifyList)->mTransactionID)
      {
         pDelNotifyList = *ppNotifyList;
         break;
      }
      
      DBG( "skipping data TID = %x\n", (*ppNotifyList)->mTransactionID );
      
      
      ppNotifyList = &(*ppNotifyList)->mpNext;
   }
   
   if (pDelNotifyList != NULL)
   {
      
      *ppNotifyList = (*ppNotifyList)->mpNext;
      
      
      if (pDelNotifyList->mpNotifyFunct != NULL)
      {
         
         spin_unlock( &pDev->mQMIDev.mClientMemLock );
      
         pDelNotifyList->mpNotifyFunct( pDev,
                                        clientID,
                                        pDelNotifyList->mpData );
                                        
         
         spin_lock( &pDev->mQMIDev.mClientMemLock );
      }
      
      
      kfree( pDelNotifyList );

      return true;
   }
   else
   {
      DBG( "no one to notify for TID %x\n", transactionID );
      
      return false;
   }
}

bool AddToURBList( 
   sQCUSBNet *      pDev,
   u16              clientID,
   struct urb *     pURB )
{
   sClientMemList * pClientMem;
   sURBList ** ppThisURBList;

#ifdef CONFIG_SMP
   
   if (spin_is_locked( &pDev->mQMIDev.mClientMemLock ) == 0)
   {
      DBG( "unlocked\n" );
      BUG();
   }
#endif

   
   pClientMem = FindClientMem( pDev, clientID );
   if (pClientMem == NULL)
   {
      DBG( "Could not find this client's memory 0x%04X\n", clientID );
      return false;
   }

   
   ppThisURBList = &pClientMem->mpURBList;
   while (*ppThisURBList != NULL)
   {
      ppThisURBList = &(*ppThisURBList)->mpNext;
   }
   
   *ppThisURBList = kmalloc( sizeof( sURBList ), GFP_ATOMIC );
   if (*ppThisURBList == NULL)
   {
      DBG( "Mem error\n" );
      return false;
   }   
   
   (*ppThisURBList)->mpNext = NULL;
   (*ppThisURBList)->mpURB = pURB;
   
   return true;
}

struct urb * PopFromURBList( 
   sQCUSBNet *      pDev,
   u16              clientID )
{
   sClientMemList * pClientMem;
   sURBList * pDelURBList;
   struct urb * pURB;

#ifdef CONFIG_SMP
   
   if (spin_is_locked( &pDev->mQMIDev.mClientMemLock ) == 0)
   {
      DBG( "unlocked\n" );
      BUG();
   }
#endif

   
   pClientMem = FindClientMem( pDev, clientID );
   if (pClientMem == NULL)
   {
      DBG( "Could not find this client's memory 0x%04X\n", clientID );
      return NULL;
   }

   
   if (pClientMem->mpURBList != NULL)
   {
      pDelURBList = pClientMem->mpURBList;
      pClientMem->mpURBList = pClientMem->mpURBList->mpNext;
      
      
      pURB = pDelURBList->mpURB;
      
      
      kfree( pDelURBList );

      return pURB;
   }
   else
   {
      DBG( "No URB's to pop\n" );
      
      return NULL;
   }
}


int UserspaceOpen( 
   struct inode *         pInode, 
   struct file *          pFilp )
{
   sQMIFilpStorage * pFilpData;
   
   
   sQMIDev * pQMIDev = container_of( pInode->i_cdev,
                                     sQMIDev,
                                     mCdev );
   sQCUSBNet * pDev = container_of( pQMIDev,
                                    sQCUSBNet,
                                    mQMIDev );                                    

   if (IsDeviceValid( pDev ) == false)
   {
      DBG( "Invalid device\n" );
      return -ENXIO;
   }

   
   pFilp->private_data = kmalloc( sizeof( sQMIFilpStorage ), GFP_KERNEL );
   if (pFilp->private_data == NULL)
   {
      DBG( "Mem error\n" );
      return -ENOMEM;
   }
   
   pFilpData = (sQMIFilpStorage *)pFilp->private_data;
   pFilpData->mClientID = (u16)-1;
   pFilpData->mpDev = pDev;
   
   return 0;
}

int UserspaceIOCTL( 
   struct inode *    pUnusedInode, 
   struct file *     pFilp,
   unsigned int      cmd, 
   unsigned long     arg )
{
   int result;
   u32 devVIDPID;
   
   sQMIFilpStorage * pFilpData = (sQMIFilpStorage *)pFilp->private_data;

   if (pFilpData == NULL)
   {
      DBG( "Bad file data\n" );
      return -EBADF;
   }
   
   if (IsDeviceValid( pFilpData->mpDev ) == false)
   {
      DBG( "Invalid device! Updating f_ops\n" );
      pFilp->f_op = pFilp->f_dentry->d_inode->i_fop;
      return -ENXIO;
   }

   switch (cmd)
   {
      case IOCTL_QMI_GET_SERVICE_FILE:
      
         DBG( "Setting up QMI for service %lu\n", arg );
         if ((u8)arg == 0)
         {
            DBG( "Cannot use QMICTL from userspace\n" );
            return -EINVAL;
         }

         
         if (pFilpData->mClientID != (u16)-1)
         {
            DBG( "Close the current connection before opening a new one\n" );
            return -EBADR;
         }
         
         result = GetClientID( pFilpData->mpDev, (u8)arg );
         if (result < 0)
         {
            return result;
         }
         pFilpData->mClientID = result;

         return 0;
         break;


      case IOCTL_QMI_GET_DEVICE_VIDPID:
         if (arg == 0)
         {
            DBG( "Bad VIDPID buffer\n" );
            return -EINVAL;
         }
         
         
         if (pFilpData->mpDev->mpNetDev == 0)
         {
            DBG( "Bad mpNetDev\n" );
            return -ENOMEM;
         }
         if (pFilpData->mpDev->mpNetDev->udev == 0)
         {
            DBG( "Bad udev\n" );
            return -ENOMEM;
         }

         devVIDPID = ((le16_to_cpu( pFilpData->mpDev->mpNetDev->udev->descriptor.idVendor ) << 16)
                     + le16_to_cpu( pFilpData->mpDev->mpNetDev->udev->descriptor.idProduct ) );

         result = copy_to_user( (unsigned int *)arg, &devVIDPID, 4 );
         if (result != 0)
         {
            DBG( "Copy to userspace failure\n" );
         }

         return result;
                 
         break;

      case IOCTL_QMI_GET_DEVICE_MEID:
         if (arg == 0)
         {
            DBG( "Bad MEID buffer\n" );
            return -EINVAL;
         }
         
         result = copy_to_user( (unsigned int *)arg, &pFilpData->mpDev->mMEID[0], 14 );
         if (result != 0)
         {
            DBG( "copy to userspace failure\n" );
         }

         return result;
                 
         break;
         
      default:
         return -EBADRQC;       
   }
}

int UserspaceClose(
   struct file *       pFilp,
   fl_owner_t          unusedFileTable )
{
   sQMIFilpStorage * pFilpData = (sQMIFilpStorage *)pFilp->private_data;
   struct list_head * pTasks;
   struct task_struct * pEachTask;
   struct fdtable * pFDT;
   int count = 0;
   int used = 0;
   unsigned long flags;

   if (pFilpData == NULL)
   {
      DBG( "bad file data\n" );
      return -EBADF;
   }

   
   if (atomic_read( &pFilp->f_count ) != 1)
   {
      
      
      list_for_each( pTasks, &current->group_leader->tasks )
      {
         pEachTask = container_of( pTasks, struct task_struct, tasks );
         if (pEachTask == NULL || pEachTask->files == NULL)
         {
            
            continue;
         }
         spin_lock_irqsave( &pEachTask->files->file_lock, flags );
         pFDT = files_fdtable( pEachTask->files );
         for (count = 0; count < pFDT->max_fds; count++)
         {
            
            
            
            if (pFDT->fd[count] == pFilp)
            {
               used++;
               break;
            }
         }
         spin_unlock_irqrestore( &pEachTask->files->file_lock, flags );
      }
      
      if (used > 0)
      {
         DBG( "not closing, as this FD is open by %d other process\n", used );
         return 0;
      }
   }

   if (IsDeviceValid( pFilpData->mpDev ) == false)
   {
      DBG( "Invalid device! Updating f_ops\n" );
      pFilp->f_op = pFilp->f_dentry->d_inode->i_fop;
      return -ENXIO;
   }
   
   DBG( "0x%04X\n", pFilpData->mClientID );
   
   
   
   
   pFilp->private_data = NULL;

   if (pFilpData->mClientID != (u16)-1)
   {
      ReleaseClientID( pFilpData->mpDev,
                       pFilpData->mClientID );
   }
      
   kfree( pFilpData );
   return 0;
}

ssize_t UserspaceRead( 
   struct file *          pFilp,
   char __user *          pBuf, 
   size_t                 size,
   loff_t *               pUnusedFpos )
{
   int result;
   void * pReadData = NULL;
   void * pSmallReadData;
   sQMIFilpStorage * pFilpData = (sQMIFilpStorage *)pFilp->private_data;

   if (pFilpData == NULL)
   {
      DBG( "Bad file data\n" );
      return -EBADF;
   }

   if (IsDeviceValid( pFilpData->mpDev ) == false)
   {
      DBG( "Invalid device! Updating f_ops\n" );
      pFilp->f_op = pFilp->f_dentry->d_inode->i_fop;
      return -ENXIO;
   }
   
   if (pFilpData->mClientID == (u16)-1)
   {
      DBG( "Client ID must be set before reading 0x%04X\n",
           pFilpData->mClientID );
      return -EBADR;
   }
   
   
   result = ReadSync( pFilpData->mpDev,
                      &pReadData,
                      pFilpData->mClientID,
                      0 );
   if (result <= 0)
   {
      return result;
   }
   
   
   result -= QMUXHeaderSize();
   pSmallReadData = pReadData + QMUXHeaderSize();

   if (result > size)
   {
      DBG( "Read data is too large for amount user has requested\n" );
      kfree( pReadData );
      return -EOVERFLOW;
   }

   if (copy_to_user( pBuf, pSmallReadData, result ) != 0)
   {
      DBG( "Error copying read data to user\n" );
      result = -EFAULT;
   }
   
   
   kfree( pReadData );
   
   return result;
}

ssize_t UserspaceWrite (
   struct file *        pFilp, 
   const char __user *  pBuf, 
   size_t               size,
   loff_t *             pUnusedFpos )
{
   int status;
   void * pWriteBuffer;
   sQMIFilpStorage * pFilpData = (sQMIFilpStorage *)pFilp->private_data;

   if (pFilpData == NULL)
   {
      DBG( "Bad file data\n" );
      return -EBADF;
   }

   if (IsDeviceValid( pFilpData->mpDev ) == false)
   {
      DBG( "Invalid device! Updating f_ops\n" );
      pFilp->f_op = pFilp->f_dentry->d_inode->i_fop;
      return -ENXIO;
   }

   if (pFilpData->mClientID == (u16)-1)
   {
      DBG( "Client ID must be set before writing 0x%04X\n",
           pFilpData->mClientID );
      return -EBADR;
   }
   
   
   pWriteBuffer = kmalloc( size + QMUXHeaderSize(), GFP_KERNEL );
   if (pWriteBuffer == NULL)
   {
      return -ENOMEM;
   }
   status = copy_from_user( pWriteBuffer + QMUXHeaderSize(), pBuf, size );
   if (status != 0)
   {
      DBG( "Unable to copy data from userspace %d\n", status );
      kfree( pWriteBuffer );
      return status;
   }

   status = WriteSync( pFilpData->mpDev,
                       pWriteBuffer, 
                       size + QMUXHeaderSize(),
                       pFilpData->mClientID );

   kfree( pWriteBuffer );
   
   
   if (status == size + QMUXHeaderSize())
   {
      return size;
   }
   else
   {
      return status;
   }
}


int RegisterQMIDevice( sQCUSBNet * pDev )
{
   int result;
   int QCQMIIndex = 0;
   dev_t devno; 
   char * pDevName;
   
   pDev->mbQMIValid = true;

   
   
   result = GetClientID( pDev, QMICTL );
   if (result != 0)
   {
      pDev->mbQMIValid = false;
      return result;
   }
   atomic_set( &pDev->mQMIDev.mQMICTLTransactionID, 1 );

   
   result = StartRead( pDev );
   if (result != 0)
   {
      pDev->mbQMIValid = false;
      return result;
   }
   
   
   
   if (QMIReady( pDev, 30000 ) == false)
   {
      DBG( "Device unresponsive to QMI\n" );
      return -ETIMEDOUT;
   }

   
   result = SetupQMIWDSCallback( pDev );
   if (result != 0)
   {
      pDev->mbQMIValid = false;
      return result;
   }

   
   result = QMIDMSGetMEID( pDev );
   if (result != 0)
   {
      pDev->mbQMIValid = false;
      return result;
   }

   
   result = alloc_chrdev_region( &devno, 0, 1, "qcqmi" );
   if (result < 0)
   {
      return result;
   }

   
   cdev_init( &pDev->mQMIDev.mCdev, &UserspaceQMIFops );
   pDev->mQMIDev.mCdev.owner = THIS_MODULE;
   pDev->mQMIDev.mCdev.ops = &UserspaceQMIFops;

   result = cdev_add( &pDev->mQMIDev.mCdev, devno, 1 );
   if (result != 0)
   {
      DBG( "error adding cdev\n" );
      return result;
   }

   
   pDevName = strstr( pDev->mpNetDev->net->name, "usb" );
   if (pDevName == NULL)
   {
      DBG( "Bad net name: %s\n", pDev->mpNetDev->net->name );
      return -ENXIO;
   }
   pDevName += strlen("usb");
   QCQMIIndex = simple_strtoul( pDevName, NULL, 10 );
   if(QCQMIIndex < 0 )
   {
      DBG( "Bad minor number\n" );
      return -ENXIO;
   }

   
   printk( KERN_INFO "creating qcqmi%d\n",
           QCQMIIndex );
#if (LINUX_VERSION_CODE >= KERNEL_VERSION( 2,6,27 ))
   
   
   device_create( pDev->mQMIDev.mpDevClass,
                  NULL, 
                  devno,
                  NULL,
                  "qcqmi%d", 
                  QCQMIIndex );
#else
   device_create( pDev->mQMIDev.mpDevClass,
                  NULL, 
                  devno,
                  "qcqmi%d", 
                  QCQMIIndex );
#endif
   
   pDev->mQMIDev.mDevNum = devno;
   
   
   return 0;
}

void DeregisterQMIDevice( sQCUSBNet * pDev )
{
   struct inode * pOpenInode;
   struct list_head * pInodeList;
   struct list_head * pTasks;
   struct task_struct * pEachTask;
   struct fdtable * pFDT;
   struct file * pFilp;
   unsigned long flags;
   int count = 0;

   
   if (IsDeviceValid( pDev ) == false)
   {
      DBG( "wrong device\n" );
      return;
   }

   
   while (pDev->mQMIDev.mpClientMemList != NULL)
   {
      DBG( "release 0x%04X\n", pDev->mQMIDev.mpClientMemList->mClientID );
   
      ReleaseClientID( pDev,
                       pDev->mQMIDev.mpClientMemList->mClientID );
      
      
   }

   
   KillRead( pDev );

   pDev->mbQMIValid = false;

   
   
   
   list_for_each( pInodeList, &pDev->mQMIDev.mCdev.list )
   {
      
      pOpenInode = container_of( pInodeList, struct inode, i_devices );
      if (pOpenInode != NULL && (IS_ERR( pOpenInode ) == false))
      {
         

         
         
         list_for_each( pTasks, &current->group_leader->tasks )
         {
            pEachTask = container_of( pTasks, struct task_struct, tasks );
            if (pEachTask == NULL || pEachTask->files == NULL)
            {
               
               continue;
            }
            
            
            spin_lock_irqsave( &pEachTask->files->file_lock, flags );
            pFDT = files_fdtable( pEachTask->files );
            for (count = 0; count < pFDT->max_fds; count++)
            {
               pFilp = pFDT->fd[count];
               if (pFilp != NULL &&  pFilp->f_dentry != NULL )
               {
                  if (pFilp->f_dentry->d_inode == pOpenInode)
                  {
                     
                     rcu_assign_pointer( pFDT->fd[count], NULL );                     
                     spin_unlock_irqrestore( &pEachTask->files->file_lock, flags );
                     
                     DBG( "forcing close of open file handle\n" );
                     filp_close( pFilp, pEachTask->files );

                     spin_lock_irqsave( &pEachTask->files->file_lock, flags );
                  }
               }
            }
            spin_unlock_irqrestore( &pEachTask->files->file_lock, flags );
         }
      }
   }

   
   if (IS_ERR(pDev->mQMIDev.mpDevClass) == false)
   {
      device_destroy( pDev->mQMIDev.mpDevClass, 
                      pDev->mQMIDev.mDevNum );   
   }
   cdev_del( &pDev->mQMIDev.mCdev );
   
   unregister_chrdev_region( pDev->mQMIDev.mDevNum, 1 );

   return;
}


bool QMIReady(
   sQCUSBNet *    pDev,
   u16            timeout )
{
   int result;
   void * pWriteBuffer;
   u16 writeBufferSize;
   void * pReadBuffer;
   u16 readBufferSize;
   struct semaphore readSem;
   u16 curTime;
   unsigned long flags;
   u8 transactionID;
   
   if (IsDeviceValid( pDev ) == false)
   {
      DBG( "Invalid device\n" );
      return -EFAULT;
   }

   writeBufferSize = QMICTLReadyReqSize();
   pWriteBuffer = kmalloc( writeBufferSize, GFP_KERNEL );
   if (pWriteBuffer == NULL)
   {
      return -ENOMEM;
   }

   
   
   

   
   for (curTime = 0; curTime < timeout; curTime += 100)
   {
      
      sema_init( &readSem, 0 );
   
      transactionID = atomic_add_return( 1, &pDev->mQMIDev.mQMICTLTransactionID );
      if (transactionID == 0)
      {
         transactionID = atomic_add_return( 1, &pDev->mQMIDev.mQMICTLTransactionID );
      }
      result = ReadAsync( pDev, QMICTL, transactionID, UpSem, &readSem );
      if (result != 0)
      {
         return false;
      }

      
      result = QMICTLReadyReq( pWriteBuffer, 
                               writeBufferSize,
                               transactionID );
      if (result < 0)
      {
         kfree( pWriteBuffer );
         return false;
      }

      
      WriteSync( pDev,
                 pWriteBuffer,
                 writeBufferSize,
                 QMICTL );

      msleep( 100 );
      if (down_trylock( &readSem ) == 0)
      {
         
         spin_lock_irqsave( &pDev->mQMIDev.mClientMemLock, flags );

         
         if (PopFromReadMemList( pDev,
                                 QMICTL,
                                 transactionID,
                                 &pReadBuffer,
                                 &readBufferSize ) == true)
         {
            

            
            spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );
         
            
            kfree( pReadBuffer );

            break;
         }
      }
      else
      {
         
         spin_lock_irqsave( &pDev->mQMIDev.mClientMemLock, flags );
         
         
         NotifyAndPopNotifyList( pDev, QMICTL, transactionID );
         
         
         spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags );
      }
   }

   kfree( pWriteBuffer );

   
   if (curTime >= timeout)
   {
      return false;
   }
   
   DBG( "QMI Ready after %u milliseconds\n", curTime );
   
   
   msleep( 5000 );

   
   return true;
}

void QMIWDSCallback(
   sQCUSBNet *    pDev,
   u16            clientID,
   void *         pData )
{
   bool bRet;
   int result;
   void * pReadBuffer;
   u16 readBufferSize;

#if (LINUX_VERSION_CODE < KERNEL_VERSION( 2,6,31 ))
   struct net_device_stats * pStats = &(pDev->mpNetDev->stats);
#else
   struct net_device_stats * pStats = &(pDev->mpNetDev->net->stats);
#endif

   u32 TXOk = (u32)-1;
   u32 RXOk = (u32)-1;
   u32 TXErr = (u32)-1;
   u32 RXErr = (u32)-1;
   u32 TXOfl = (u32)-1;
   u32 RXOfl = (u32)-1;
   u64 TXBytesOk = (u64)-1;
   u64 RXBytesOk = (u64)-1;
   bool bLinkState;
   bool bReconfigure;
   unsigned long flags;
   
   if (IsDeviceValid( pDev ) == false)
   {
      DBG( "Invalid device\n" );
      return;
   }

   
   spin_lock_irqsave( &pDev->mQMIDev.mClientMemLock, flags );
   
   bRet = PopFromReadMemList( pDev,
                              clientID,
                              0,
                              &pReadBuffer,
                              &readBufferSize );
   
   
   spin_unlock_irqrestore( &pDev->mQMIDev.mClientMemLock, flags ); 
   
   if (bRet == false)
   {
      DBG( "WDS callback failed to get data\n" );
      return;
   }
   
   
   bLinkState = ! QTestDownReason( pDev, NO_NDIS_CONNECTION );
   bReconfigure = false;

   result = QMIWDSEventResp( pReadBuffer,
                             readBufferSize,
                             &TXOk,
                             &RXOk,
                             &TXErr,
                             &RXErr,
                             &TXOfl,
                             &RXOfl,
                             &TXBytesOk,
                             &RXBytesOk,
                             &bLinkState,
                             &bReconfigure );
   if (result < 0)
   {
      DBG( "bad WDS packet\n" );
   }
   else
   {

      
      if (TXOfl != (u32)-1)
      {
         pStats->tx_fifo_errors = TXOfl;
      }
      
      if (RXOfl != (u32)-1)
      {
         pStats->rx_fifo_errors = RXOfl;
      }

      if (TXErr != (u32)-1)
      {
         pStats->tx_errors = TXErr;
      }
      
      if (RXErr != (u32)-1)
      {
         pStats->rx_errors = RXErr;
      }

      if (TXOk != (u32)-1)
      {
         pStats->tx_packets = TXOk + pStats->tx_errors;
      }
      
      if (RXOk != (u32)-1)
      {
         pStats->rx_packets = RXOk + pStats->rx_errors;
      }

      if (TXBytesOk != (u64)-1)
      {
         pStats->tx_bytes = TXBytesOk;
      }
      
      if (RXBytesOk != (u64)-1)
      {
         pStats->rx_bytes = RXBytesOk;
      }

      if (bReconfigure == true)
      {
         DBG( "Net device link reset\n" );
         QSetDownReason( pDev, NO_NDIS_CONNECTION );
         QClearDownReason( pDev, NO_NDIS_CONNECTION );
      }
      else 
      {
         if (bLinkState == true)
         {
            DBG( "Net device link is connected\n" );
            QClearDownReason( pDev, NO_NDIS_CONNECTION );
         }
         else
         {
            DBG( "Net device link is disconnected\n" );
            QSetDownReason( pDev, NO_NDIS_CONNECTION );
         }
      }
   }

   kfree( pReadBuffer );

   
   result = ReadAsync( pDev,
                       clientID,
                       0,
                       QMIWDSCallback,
                       pData );
   if (result != 0)
   {
      DBG( "unable to setup next async read\n" );
   }

   return;
}

int SetupQMIWDSCallback( sQCUSBNet * pDev )
{
   int result;
   void * pWriteBuffer;
   u16 writeBufferSize;
   u16 WDSClientID;

   if (IsDeviceValid( pDev ) == false)
   {
      DBG( "Invalid device\n" );
      return -EFAULT;
   }
   
   result = GetClientID( pDev, QMIWDS );
   if (result < 0)
   {
      return result;
   }
   WDSClientID = result;

   
   writeBufferSize = QMIWDSSetEventReportReqSize();
   pWriteBuffer = kmalloc( writeBufferSize, GFP_KERNEL );
   if (pWriteBuffer == NULL)
   {
      return -ENOMEM;
   }
   
   result = QMIWDSSetEventReportReq( pWriteBuffer, 
                                     writeBufferSize,
                                     1 );
   if (result < 0)
   {
      kfree( pWriteBuffer );
      return result;
   }

   result = WriteSync( pDev,
                       pWriteBuffer,
                       writeBufferSize,
                       WDSClientID );
   kfree( pWriteBuffer );

   if (result < 0)
   {
      return result;
   }

   
   writeBufferSize = QMIWDSGetPKGSRVCStatusReqSize();
   pWriteBuffer = kmalloc( writeBufferSize, GFP_KERNEL );
   if (pWriteBuffer == NULL)
   {
      return -ENOMEM;
   }

   result = QMIWDSGetPKGSRVCStatusReq( pWriteBuffer, 
                                       writeBufferSize,
                                       2 );
   if (result < 0)
   {
      kfree( pWriteBuffer );
      return result;
   }
   
   result = WriteSync( pDev,
                       pWriteBuffer,
                       writeBufferSize,
                       WDSClientID );
   kfree( pWriteBuffer );

   if (result < 0)
   {
      return result;
   }

   
   result = ReadAsync( pDev,
                       WDSClientID,
                       0,
                       QMIWDSCallback,
                       NULL );
   if (result != 0)
   {
      DBG( "unable to setup async read\n" );
      return result;
   }

   
   
   result = usb_control_msg( pDev->mpNetDev->udev,
                             usb_sndctrlpipe( pDev->mpNetDev->udev, 0 ),
                             0x22,
                             0x21,
                             1, 
                             0,
                             NULL,
                             0,
                             100 );
   if (result < 0)
   {
      DBG( "Bad SetControlLineState status %d\n", result );
      return result;
   }

   return 0;
}

int QMIDMSGetMEID( sQCUSBNet * pDev )
{
   int result;
   void * pWriteBuffer;
   u16 writeBufferSize;
   void * pReadBuffer;
   u16 readBufferSize;
   u16 DMSClientID;

   if (IsDeviceValid( pDev ) == false)
   {
      DBG( "Invalid device\n" );
      return -EFAULT;
   }

   result = GetClientID( pDev, QMIDMS );
   if (result < 0)
   {
      return result;
   }
   DMSClientID = result;

   
   writeBufferSize = QMIDMSGetMEIDReqSize();
   pWriteBuffer = kmalloc( writeBufferSize, GFP_KERNEL );
   if (pWriteBuffer == NULL)
   {
      return -ENOMEM;
   }

   result = QMIDMSGetMEIDReq( pWriteBuffer, 
                              writeBufferSize,
                              1 );
   if (result < 0)
   {
      kfree( pWriteBuffer );
      return result;
   }

   result = WriteSync( pDev,
                       pWriteBuffer,
                       writeBufferSize,
                       DMSClientID );
   kfree( pWriteBuffer );

   if (result < 0)
   {
      return result;
   }

   
   result = ReadSync( pDev,
                      &pReadBuffer,
                      DMSClientID,
                      1 );
   if (result < 0)
   {
      return result;
   }
   readBufferSize = result;

   result = QMIDMSGetMEIDResp( pReadBuffer,
                               readBufferSize,
                               &pDev->mMEID[0],
                               14 );
   kfree( pReadBuffer );

   if (result < 0)
   {
      DBG( "bad get MEID resp\n" );
      
      
      
      memset( &pDev->mMEID[0], '0', 14 );
   }

   ReleaseClientID( pDev, DMSClientID );

   
   return 0;
}
