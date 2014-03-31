/*
 *  Copyright (c) 2000-2010 LSI Corporation.
 *
 *
 *           Name:  mpi2_raid.h
 *          Title:  MPI Integrated RAID messages and structures
 *  Creation Date:  April 26, 2007
 *
 *    mpi2_raid.h Version:  02.00.06
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  04-30-07  02.00.00  Corresponds to Fusion-MPT MPI Specification Rev A.
 *  08-31-07  02.00.01  Modifications to RAID Action request and reply,
 *                      including the Actions and ActionData.
 *  02-29-08  02.00.02  Added MPI2_RAID_ACTION_ADATA_DISABL_FULL_REBUILD.
 *  05-21-08  02.00.03  Added MPI2_RAID_VOL_CREATION_NUM_PHYSDISKS so that
 *                      the PhysDisk array in MPI2_RAID_VOLUME_CREATION_STRUCT
 *                      can be sized by the build environment.
 *  07-30-09  02.00.04  Added proper define for the Use Default Settings bit of
 *                      VolumeCreationFlags and marked the old one as obsolete.
 *  05-12-10  02.00.05  Added MPI2_RAID_VOL_FLAGS_OP_MDC define.
 *  08-24-10  02.00.06  Added MPI2_RAID_ACTION_COMPATIBILITY_CHECK along with
 *                      related structures and defines.
 *                      Added product-specific range to RAID Action values.

 *  --------------------------------------------------------------------------
 */

#ifndef MPI2_RAID_H
#define MPI2_RAID_H



#define MPI2_RAID_ACTION_ADATA_KEEP_LBA0            (0x00000000)
#define MPI2_RAID_ACTION_ADATA_ZERO_LBA0            (0x00000001)


#define MPI2_RAID_ACTION_ADATA_DISABL_FULL_REBUILD  (0x00000001)

typedef struct _MPI2_RAID_ACTION_RATE_DATA
{
    U8              RateToChange;               
    U8              RateOrMode;                 
    U16             DataScrubDuration;          
} MPI2_RAID_ACTION_RATE_DATA, MPI2_POINTER PTR_MPI2_RAID_ACTION_RATE_DATA,
  Mpi2RaidActionRateData_t, MPI2_POINTER pMpi2RaidActionRateData_t;

#define MPI2_RAID_ACTION_SET_RATE_RESYNC            (0x00)
#define MPI2_RAID_ACTION_SET_RATE_DATA_SCRUB        (0x01)
#define MPI2_RAID_ACTION_SET_RATE_POWERSAVE_MODE    (0x02)

typedef struct _MPI2_RAID_ACTION_START_RAID_FUNCTION
{
    U8              RAIDFunction;                       
    U8              Flags;                              
    U16             Reserved1;                          
} MPI2_RAID_ACTION_START_RAID_FUNCTION,
  MPI2_POINTER PTR_MPI2_RAID_ACTION_START_RAID_FUNCTION,
  Mpi2RaidActionStartRaidFunction_t,
  MPI2_POINTER pMpi2RaidActionStartRaidFunction_t;

#define MPI2_RAID_ACTION_START_BACKGROUND_INIT      (0x00)
#define MPI2_RAID_ACTION_START_ONLINE_CAP_EXPANSION (0x01)
#define MPI2_RAID_ACTION_START_CONSISTENCY_CHECK    (0x02)

#define MPI2_RAID_ACTION_START_NEW                  (0x00)
#define MPI2_RAID_ACTION_START_RESUME               (0x01)

typedef struct _MPI2_RAID_ACTION_STOP_RAID_FUNCTION
{
    U8              RAIDFunction;                       
    U8              Flags;                              
    U16             Reserved1;                          
} MPI2_RAID_ACTION_STOP_RAID_FUNCTION,
  MPI2_POINTER PTR_MPI2_RAID_ACTION_STOP_RAID_FUNCTION,
  Mpi2RaidActionStopRaidFunction_t,
  MPI2_POINTER pMpi2RaidActionStopRaidFunction_t;

#define MPI2_RAID_ACTION_STOP_BACKGROUND_INIT       (0x00)
#define MPI2_RAID_ACTION_STOP_ONLINE_CAP_EXPANSION  (0x01)
#define MPI2_RAID_ACTION_STOP_CONSISTENCY_CHECK     (0x02)

#define MPI2_RAID_ACTION_STOP_ABORT                 (0x00)
#define MPI2_RAID_ACTION_STOP_PAUSE                 (0x01)

typedef struct _MPI2_RAID_ACTION_HOT_SPARE
{
    U8              HotSparePool;               
    U8              Reserved1;                  
    U16             DevHandle;                  
} MPI2_RAID_ACTION_HOT_SPARE, MPI2_POINTER PTR_MPI2_RAID_ACTION_HOT_SPARE,
  Mpi2RaidActionHotSpare_t, MPI2_POINTER pMpi2RaidActionHotSpare_t;

typedef struct _MPI2_RAID_ACTION_FW_UPDATE_MODE
{
    U8              Flags;                              
    U8              DeviceFirmwareUpdateModeTimeout;    
    U16             Reserved1;                          
} MPI2_RAID_ACTION_FW_UPDATE_MODE,
  MPI2_POINTER PTR_MPI2_RAID_ACTION_FW_UPDATE_MODE,
  Mpi2RaidActionFwUpdateMode_t, MPI2_POINTER pMpi2RaidActionFwUpdateMode_t;

#define MPI2_RAID_ACTION_ADATA_DISABLE_FW_UPDATE        (0x00)
#define MPI2_RAID_ACTION_ADATA_ENABLE_FW_UPDATE         (0x01)

typedef union _MPI2_RAID_ACTION_DATA
{
    U32                                     Word;
    MPI2_RAID_ACTION_RATE_DATA              Rates;
    MPI2_RAID_ACTION_START_RAID_FUNCTION    StartRaidFunction;
    MPI2_RAID_ACTION_STOP_RAID_FUNCTION     StopRaidFunction;
    MPI2_RAID_ACTION_HOT_SPARE              HotSpare;
    MPI2_RAID_ACTION_FW_UPDATE_MODE         FwUpdateMode;
} MPI2_RAID_ACTION_DATA, MPI2_POINTER PTR_MPI2_RAID_ACTION_DATA,
  Mpi2RaidActionData_t, MPI2_POINTER pMpi2RaidActionData_t;


typedef struct _MPI2_RAID_ACTION_REQUEST
{
    U8                      Action;                         
    U8                      Reserved1;                      
    U8                      ChainOffset;                    
    U8                      Function;                       
    U16                     VolDevHandle;                   
    U8                      PhysDiskNum;                    
    U8                      MsgFlags;                       
    U8                      VP_ID;                          
    U8                      VF_ID;                          
    U16                     Reserved2;                      
    U32                     Reserved3;                      
    MPI2_RAID_ACTION_DATA   ActionDataWord;                 
    MPI2_SGE_SIMPLE_UNION   ActionDataSGE;                  
} MPI2_RAID_ACTION_REQUEST, MPI2_POINTER PTR_MPI2_RAID_ACTION_REQUEST,
  Mpi2RaidActionRequest_t, MPI2_POINTER pMpi2RaidActionRequest_t;


#define MPI2_RAID_ACTION_INDICATOR_STRUCT           (0x01)
#define MPI2_RAID_ACTION_CREATE_VOLUME              (0x02)
#define MPI2_RAID_ACTION_DELETE_VOLUME              (0x03)
#define MPI2_RAID_ACTION_DISABLE_ALL_VOLUMES        (0x04)
#define MPI2_RAID_ACTION_ENABLE_ALL_VOLUMES         (0x05)
#define MPI2_RAID_ACTION_PHYSDISK_OFFLINE           (0x0A)
#define MPI2_RAID_ACTION_PHYSDISK_ONLINE            (0x0B)
#define MPI2_RAID_ACTION_FAIL_PHYSDISK              (0x0F)
#define MPI2_RAID_ACTION_ACTIVATE_VOLUME            (0x11)
#define MPI2_RAID_ACTION_DEVICE_FW_UPDATE_MODE      (0x15)
#define MPI2_RAID_ACTION_CHANGE_VOL_WRITE_CACHE     (0x17)
#define MPI2_RAID_ACTION_SET_VOLUME_NAME            (0x18)
#define MPI2_RAID_ACTION_SET_RAID_FUNCTION_RATE     (0x19)
#define MPI2_RAID_ACTION_ENABLE_FAILED_VOLUME       (0x1C)
#define MPI2_RAID_ACTION_CREATE_HOT_SPARE           (0x1D)
#define MPI2_RAID_ACTION_DELETE_HOT_SPARE           (0x1E)
#define MPI2_RAID_ACTION_SYSTEM_SHUTDOWN_INITIATED  (0x20)
#define MPI2_RAID_ACTION_START_RAID_FUNCTION        (0x21)
#define MPI2_RAID_ACTION_STOP_RAID_FUNCTION         (0x22)
#define MPI2_RAID_ACTION_COMPATIBILITY_CHECK        (0x23)
#define MPI2_RAID_ACTION_MIN_PRODUCT_SPECIFIC       (0x80)
#define MPI2_RAID_ACTION_MAX_PRODUCT_SPECIFIC       (0xFF)


#ifndef MPI2_RAID_VOL_CREATION_NUM_PHYSDISKS
#define MPI2_RAID_VOL_CREATION_NUM_PHYSDISKS        (1)
#endif

typedef struct _MPI2_RAID_VOLUME_PHYSDISK
{
    U8                      RAIDSetNum;                     
    U8                      PhysDiskMap;                    
    U16                     PhysDiskDevHandle;              
} MPI2_RAID_VOLUME_PHYSDISK, MPI2_POINTER PTR_MPI2_RAID_VOLUME_PHYSDISK,
  Mpi2RaidVolumePhysDisk_t, MPI2_POINTER pMpi2RaidVolumePhysDisk_t;

#define MPI2_RAIDACTION_PHYSDISK_PRIMARY            (0x01)
#define MPI2_RAIDACTION_PHYSDISK_SECONDARY          (0x02)

typedef struct _MPI2_RAID_VOLUME_CREATION_STRUCT
{
    U8                          NumPhysDisks;               
    U8                          VolumeType;                 
    U16                         Reserved1;                  
    U32                         VolumeCreationFlags;        
    U32                         VolumeSettings;             
    U8                          Reserved2;                  
    U8                          ResyncRate;                 
    U16                         DataScrubDuration;          
    U64                         VolumeMaxLBA;               
    U32                         StripeSize;                 
    U8                          Name[16];                   
    MPI2_RAID_VOLUME_PHYSDISK   PhysDisk[MPI2_RAID_VOL_CREATION_NUM_PHYSDISKS];
} MPI2_RAID_VOLUME_CREATION_STRUCT,
  MPI2_POINTER PTR_MPI2_RAID_VOLUME_CREATION_STRUCT,
  Mpi2RaidVolumeCreationStruct_t, MPI2_POINTER pMpi2RaidVolumeCreationStruct_t;


#define MPI2_RAID_VOL_CREATION_DEFAULT_SETTINGS     (0x80000000)
#define MPI2_RAID_VOL_CREATION_BACKGROUND_INIT      (0x00000004)
#define MPI2_RAID_VOL_CREATION_LOW_LEVEL_INIT       (0x00000002)
#define MPI2_RAID_VOL_CREATION_MIGRATE_DATA         (0x00000001)
#define MPI2_RAID_VOL_CREATION_USE_DEFAULT_SETTINGS (0x80)



typedef struct _MPI2_RAID_ONLINE_CAPACITY_EXPANSION
{
    U32                     Flags;                          
    U16                     DevHandle0;                     
    U16                     Reserved1;                      
    U16                     DevHandle1;                     
    U16                     Reserved2;                      
} MPI2_RAID_ONLINE_CAPACITY_EXPANSION,
  MPI2_POINTER PTR_MPI2_RAID_ONLINE_CAPACITY_EXPANSION,
  Mpi2RaidOnlineCapacityExpansion_t,
  MPI2_POINTER pMpi2RaidOnlineCapacityExpansion_t;


typedef struct _MPI2_RAID_COMPATIBILITY_INPUT_STRUCT {
	U16                     SourceDevHandle;               
	U16                     CandidateDevHandle;             
	U32                     Flags;                          
	U32                     Reserved1;                      
	U32                     Reserved2;                      
} MPI2_RAID_COMPATIBILITY_INPUT_STRUCT,
MPI2_POINTER PTR_MPI2_RAID_COMPATIBILITY_INPUT_STRUCT,
Mpi2RaidCompatibilityInputStruct_t,
MPI2_POINTER pMpi2RaidCompatibilityInputStruct_t;

#define MPI2_RAID_COMPAT_SOURCE_IS_VOLUME_FLAG      (0x00000002)
#define MPI2_RAID_COMPAT_REPORT_SOURCE_INFO_FLAG    (0x00000001)



typedef struct _MPI2_RAID_VOL_INDICATOR
{
    U64                     TotalBlocks;                    
    U64                     BlocksRemaining;                
    U32                     Flags;                          
} MPI2_RAID_VOL_INDICATOR, MPI2_POINTER PTR_MPI2_RAID_VOL_INDICATOR,
  Mpi2RaidVolIndicator_t, MPI2_POINTER pMpi2RaidVolIndicator_t;

#define MPI2_RAID_VOL_FLAGS_OP_MASK                 (0x0000000F)
#define MPI2_RAID_VOL_FLAGS_OP_BACKGROUND_INIT      (0x00000000)
#define MPI2_RAID_VOL_FLAGS_OP_ONLINE_CAP_EXPANSION (0x00000001)
#define MPI2_RAID_VOL_FLAGS_OP_CONSISTENCY_CHECK    (0x00000002)
#define MPI2_RAID_VOL_FLAGS_OP_RESYNC               (0x00000003)
#define MPI2_RAID_VOL_FLAGS_OP_MDC                  (0x00000004)


typedef struct _MPI2_RAID_COMPATIBILITY_RESULT_STRUCT {
	U8                      State;                          
	U8                      Reserved1;                      
	U16                     Reserved2;                      
	U32                     GenericAttributes;              
	U32                     OEMSpecificAttributes;          
	U32                     Reserved3;                      
	U32                     Reserved4;                      
} MPI2_RAID_COMPATIBILITY_RESULT_STRUCT,
MPI2_POINTER PTR_MPI2_RAID_COMPATIBILITY_RESULT_STRUCT,
Mpi2RaidCompatibilityResultStruct_t,
MPI2_POINTER pMpi2RaidCompatibilityResultStruct_t;

#define MPI2_RAID_COMPAT_STATE_COMPATIBLE           (0x00)
#define MPI2_RAID_COMPAT_STATE_NOT_COMPATIBLE       (0x01)

#define MPI2_RAID_COMPAT_GENATTRIB_4K_SECTOR            (0x00000010)

#define MPI2_RAID_COMPAT_GENATTRIB_MEDIA_MASK           (0x0000000C)
#define MPI2_RAID_COMPAT_GENATTRIB_SOLID_STATE_DRIVE    (0x00000008)
#define MPI2_RAID_COMPAT_GENATTRIB_HARD_DISK_DRIVE      (0x00000004)

#define MPI2_RAID_COMPAT_GENATTRIB_PROTOCOL_MASK        (0x00000003)
#define MPI2_RAID_COMPAT_GENATTRIB_SAS_PROTOCOL         (0x00000002)
#define MPI2_RAID_COMPAT_GENATTRIB_SATA_PROTOCOL        (0x00000001)

typedef union _MPI2_RAID_ACTION_REPLY_DATA
{
	U32                                     Word[5];
	MPI2_RAID_VOL_INDICATOR                 RaidVolumeIndicator;
	U16                                     VolDevHandle;
	U8                                      VolumeState;
	U8                                      PhysDiskNum;
	MPI2_RAID_COMPATIBILITY_RESULT_STRUCT   RaidCompatibilityResult;
} MPI2_RAID_ACTION_REPLY_DATA, MPI2_POINTER PTR_MPI2_RAID_ACTION_REPLY_DATA,
  Mpi2RaidActionReplyData_t, MPI2_POINTER pMpi2RaidActionReplyData_t;



typedef struct _MPI2_RAID_ACTION_REPLY
{
    U8                          Action;                     
    U8                          Reserved1;                  
    U8                          MsgLength;                  
    U8                          Function;                   
    U16                         VolDevHandle;               
    U8                          PhysDiskNum;                
    U8                          MsgFlags;                   
    U8                          VP_ID;                      
    U8                          VF_ID;                      
    U16                         Reserved2;                  
    U16                         Reserved3;                  
    U16                         IOCStatus;                  
    U32                         IOCLogInfo;                 
    MPI2_RAID_ACTION_REPLY_DATA ActionData;                 
} MPI2_RAID_ACTION_REPLY, MPI2_POINTER PTR_MPI2_RAID_ACTION_REPLY,
  Mpi2RaidActionReply_t, MPI2_POINTER pMpi2RaidActionReply_t;


#endif

