/*
 *  Copyright (c) 2001-2008 LSI Corporation.
 *
 *
 *           Name:  mpi_raid.h
 *          Title:  MPI RAID message and structures
 *  Creation Date:  February 27, 2001
 *
 *    mpi_raid.h Version:  01.05.05
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  02-27-01  01.01.01  Original release for this file.
 *  03-27-01  01.01.02  Added structure offset comments.
 *  08-08-01  01.02.01  Original release for v1.2 work.
 *  09-28-01  01.02.02  Major rework for MPI v1.2 Integrated RAID changes.
 *  10-04-01  01.02.03  Added ActionData defines for
 *                      MPI_RAID_ACTION_DELETE_VOLUME action.
 *  11-01-01  01.02.04  Added define for MPI_RAID_ACTION_ADATA_DO_NOT_SYNC.
 *  03-14-02  01.02.05  Added define for MPI_RAID_ACTION_ADATA_LOW_LEVEL_INIT.
 *  05-07-02  01.02.06  Added define for MPI_RAID_ACTION_ACTIVATE_VOLUME,
 *                      MPI_RAID_ACTION_INACTIVATE_VOLUME, and
 *                      MPI_RAID_ACTION_ADATA_INACTIVATE_ALL.
 *  07-12-02  01.02.07  Added structures for Mailbox request and reply.
 *  11-15-02  01.02.08  Added missing MsgContext field to MSG_MAILBOX_REQUEST.
 *  04-01-03  01.02.09  New action data option flag for
 *                      MPI_RAID_ACTION_DELETE_VOLUME.
 *  05-11-04  01.03.01  Original release for MPI v1.3.
 *  08-19-04  01.05.01  Original release for MPI v1.5.
 *  01-15-05  01.05.02  Added defines for the two new RAID Actions for
 *                      _SET_RESYNC_RATE and _SET_DATA_SCRUB_RATE.
 *  02-28-07  01.05.03  Added new RAID Action, Device FW Update Mode, and
 *                      associated defines.
 *  08-07-07  01.05.04  Added Disable Full Rebuild bit to the ActionDataWord
 *                      for the RAID Action MPI_RAID_ACTION_DISABLE_VOLUME.
 *  01-15-08  01.05.05  Added define for MPI_RAID_ACTION_SET_VOLUME_NAME.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI_RAID_H
#define MPI_RAID_H





typedef struct _MSG_RAID_ACTION
{
    U8                      Action;             
    U8                      Reserved1;          
    U8                      ChainOffset;        
    U8                      Function;           
    U8                      VolumeID;           
    U8                      VolumeBus;          
    U8                      PhysDiskNum;        
    U8                      MsgFlags;           
    U32                     MsgContext;         
    U32                     Reserved2;          
    U32                     ActionDataWord;     
    SGE_SIMPLE_UNION        ActionDataSGE;      
} MSG_RAID_ACTION_REQUEST, MPI_POINTER PTR_MSG_RAID_ACTION_REQUEST,
  MpiRaidActionRequest_t , MPI_POINTER pMpiRaidActionRequest_t;



#define MPI_RAID_ACTION_STATUS                      (0x00)
#define MPI_RAID_ACTION_INDICATOR_STRUCT            (0x01)
#define MPI_RAID_ACTION_CREATE_VOLUME               (0x02)
#define MPI_RAID_ACTION_DELETE_VOLUME               (0x03)
#define MPI_RAID_ACTION_DISABLE_VOLUME              (0x04)
#define MPI_RAID_ACTION_ENABLE_VOLUME               (0x05)
#define MPI_RAID_ACTION_QUIESCE_PHYS_IO             (0x06)
#define MPI_RAID_ACTION_ENABLE_PHYS_IO              (0x07)
#define MPI_RAID_ACTION_CHANGE_VOLUME_SETTINGS      (0x08)
#define MPI_RAID_ACTION_PHYSDISK_OFFLINE            (0x0A)
#define MPI_RAID_ACTION_PHYSDISK_ONLINE             (0x0B)
#define MPI_RAID_ACTION_CHANGE_PHYSDISK_SETTINGS    (0x0C)
#define MPI_RAID_ACTION_CREATE_PHYSDISK             (0x0D)
#define MPI_RAID_ACTION_DELETE_PHYSDISK             (0x0E)
#define MPI_RAID_ACTION_FAIL_PHYSDISK               (0x0F)
#define MPI_RAID_ACTION_REPLACE_PHYSDISK            (0x10)
#define MPI_RAID_ACTION_ACTIVATE_VOLUME             (0x11)
#define MPI_RAID_ACTION_INACTIVATE_VOLUME           (0x12)
#define MPI_RAID_ACTION_SET_RESYNC_RATE             (0x13)
#define MPI_RAID_ACTION_SET_DATA_SCRUB_RATE         (0x14)
#define MPI_RAID_ACTION_DEVICE_FW_UPDATE_MODE       (0x15)
#define MPI_RAID_ACTION_SET_VOLUME_NAME             (0x16)

#define MPI_RAID_ACTION_ADATA_DO_NOT_SYNC           (0x00000001)
#define MPI_RAID_ACTION_ADATA_LOW_LEVEL_INIT        (0x00000002)

#define MPI_RAID_ACTION_ADATA_KEEP_PHYS_DISKS       (0x00000000)
#define MPI_RAID_ACTION_ADATA_DEL_PHYS_DISKS        (0x00000001)

#define MPI_RAID_ACTION_ADATA_KEEP_LBA0             (0x00000000)
#define MPI_RAID_ACTION_ADATA_ZERO_LBA0             (0x00000002)

#define MPI_RAID_ACTION_ADATA_DISABLE_FULL_REBUILD  (0x00000001)

#define MPI_RAID_ACTION_ADATA_INACTIVATE_ALL        (0x00000001)

#define MPI_RAID_ACTION_ADATA_RESYNC_RATE_MASK      (0x000000FF)

#define MPI_RAID_ACTION_ADATA_DATA_SCRUB_RATE_MASK  (0x000000FF)

#define MPI_RAID_ACTION_ADATA_ENABLE_FW_UPDATE          (0x00000001)
#define MPI_RAID_ACTION_ADATA_MASK_FW_UPDATE_TIMEOUT    (0x0000FF00)
#define MPI_RAID_ACTION_ADATA_SHIFT_FW_UPDATE_TIMEOUT   (8)



typedef struct _MSG_RAID_ACTION_REPLY
{
    U8                      Action;             
    U8                      Reserved;           
    U8                      MsgLength;          
    U8                      Function;           
    U8                      VolumeID;           
    U8                      VolumeBus;          
    U8                      PhysDiskNum;        
    U8                      MsgFlags;           
    U32                     MsgContext;         
    U16                     ActionStatus;       
    U16                     IOCStatus;          
    U32                     IOCLogInfo;         
    U32                     VolumeStatus;       
    U32                     ActionData;         
} MSG_RAID_ACTION_REPLY, MPI_POINTER PTR_MSG_RAID_ACTION_REPLY,
  MpiRaidActionReply_t, MPI_POINTER pMpiRaidActionReply_t;



#define MPI_RAID_ACTION_ASTATUS_SUCCESS             (0x0000)
#define MPI_RAID_ACTION_ASTATUS_INVALID_ACTION      (0x0001)
#define MPI_RAID_ACTION_ASTATUS_FAILURE             (0x0002)
#define MPI_RAID_ACTION_ASTATUS_IN_PROGRESS         (0x0003)



typedef struct _MPI_RAID_VOL_INDICATOR
{
    U64                     TotalBlocks;        
    U64                     BlocksRemaining;    
} MPI_RAID_VOL_INDICATOR, MPI_POINTER PTR_MPI_RAID_VOL_INDICATOR,
  MpiRaidVolIndicator_t, MPI_POINTER pMpiRaidVolIndicator_t;



typedef struct _MSG_SCSI_IO_RAID_PT_REQUEST
{
    U8                      PhysDiskNum;        
    U8                      Reserved1;          
    U8                      ChainOffset;        
    U8                      Function;           
    U8                      CDBLength;          
    U8                      SenseBufferLength;  
    U8                      Reserved2;          
    U8                      MsgFlags;           
    U32                     MsgContext;         
    U8                      LUN[8];             
    U32                     Control;            
    U8                      CDB[16];            
    U32                     DataLength;         
    U32                     SenseBufferLowAddr; 
    SGE_IO_UNION            SGL;                
} MSG_SCSI_IO_RAID_PT_REQUEST, MPI_POINTER PTR_MSG_SCSI_IO_RAID_PT_REQUEST,
  SCSIIORaidPassthroughRequest_t, MPI_POINTER pSCSIIORaidPassthroughRequest_t;



typedef struct _MSG_SCSI_IO_RAID_PT_REPLY
{
    U8                      PhysDiskNum;        
    U8                      Reserved1;          
    U8                      MsgLength;          
    U8                      Function;           
    U8                      CDBLength;          
    U8                      SenseBufferLength;  
    U8                      Reserved2;          
    U8                      MsgFlags;           
    U32                     MsgContext;         
    U8                      SCSIStatus;         
    U8                      SCSIState;          
    U16                     IOCStatus;          
    U32                     IOCLogInfo;         
    U32                     TransferCount;      
    U32                     SenseCount;         
    U32                     ResponseInfo;       
} MSG_SCSI_IO_RAID_PT_REPLY, MPI_POINTER PTR_MSG_SCSI_IO_RAID_PT_REPLY,
  SCSIIORaidPassthroughReply_t, MPI_POINTER pSCSIIORaidPassthroughReply_t;



typedef struct _MSG_MAILBOX_REQUEST
{
    U16                     Reserved1;
    U8                      ChainOffset;
    U8                      Function;
    U16                     Reserved2;
    U8                      Reserved3;
    U8                      MsgFlags;
    U32                     MsgContext;
    U8                      Command[10];
    U16                     Reserved4;
    SGE_IO_UNION            SGL;
} MSG_MAILBOX_REQUEST, MPI_POINTER PTR_MSG_MAILBOX_REQUEST,
  MailboxRequest_t, MPI_POINTER pMailboxRequest_t;


typedef struct _MSG_MAILBOX_REPLY
{
    U16                     Reserved1;          
    U8                      MsgLength;          
    U8                      Function;           
    U16                     Reserved2;          
    U8                      Reserved3;          
    U8                      MsgFlags;           
    U32                     MsgContext;         
    U16                     MailboxStatus;      
    U16                     IOCStatus;          
    U32                     IOCLogInfo;         
    U32                     Reserved4;          
} MSG_MAILBOX_REPLY, MPI_POINTER PTR_MSG_MAILBOX_REPLY,
  MailboxReply_t, MPI_POINTER pMailboxReply_t;

#endif



