/*
 *  Copyright (c) 2000-2008 LSI Corporation.
 *
 *
 *           Name:  mpi_ioc.h
 *          Title:  MPI IOC, Port, Event, FW Download, and FW Upload messages
 *  Creation Date:  August 11, 2000
 *
 *    mpi_ioc.h Version:  01.05.16
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  05-08-00  00.10.01  Original release for 0.10 spec dated 4/26/2000.
 *  05-24-00  00.10.02  Added _MSG_IOC_INIT_REPLY structure.
 *  06-06-00  01.00.01  Added CurReplyFrameSize field to _MSG_IOC_FACTS_REPLY.
 *  06-12-00  01.00.02  Added _MSG_PORT_ENABLE_REPLY structure.
 *                      Added _MSG_EVENT_ACK_REPLY structure.
 *                      Added _MSG_FW_DOWNLOAD_REPLY structure.
 *                      Added _MSG_TOOLBOX_REPLY structure.
 *  06-30-00  01.00.03  Added MaxLanBuckets to _PORT_FACT_REPLY structure.
 *  07-27-00  01.00.04  Added _EVENT_DATA structure definitions for _SCSI,
 *                      _LINK_STATUS, _LOOP_STATE and _LOGOUT.
 *  08-11-00  01.00.05  Switched positions of MsgLength and Function fields in
 *                      _MSG_EVENT_ACK_REPLY structure to match specification.
 *  11-02-00  01.01.01  Original release for post 1.0 work.
 *                      Added a value for Manufacturer to WhoInit.
 *  12-04-00  01.01.02  Modified IOCFacts reply, added FWUpload messages, and
 *                      removed toolbox message.
 *  01-09-01  01.01.03  Added event enabled and disabled defines.
 *                      Added structures for FwHeader and DataHeader.
 *                      Added ImageType to FwUpload reply.
 *  02-20-01  01.01.04  Started using MPI_POINTER.
 *  02-27-01  01.01.05  Added event for RAID status change and its event data.
 *                      Added IocNumber field to MSG_IOC_FACTS_REPLY.
 *  03-27-01  01.01.06  Added defines for ProductId field of MPI_FW_HEADER.
 *                      Added structure offset comments.
 *  04-09-01  01.01.07  Added structure EVENT_DATA_EVENT_CHANGE.
 *  08-08-01  01.02.01  Original release for v1.2 work.
 *                      New format for FWVersion and ProductId in
 *                      MSG_IOC_FACTS_REPLY and MPI_FW_HEADER.
 *  08-31-01  01.02.02  Addded event MPI_EVENT_SCSI_DEVICE_STATUS_CHANGE and
 *                      related structure and defines.
 *                      Added event MPI_EVENT_ON_BUS_TIMER_EXPIRED.
 *                      Added MPI_IOCINIT_FLAGS_DISCARD_FW_IMAGE.
 *                      Replaced a reserved field in MSG_IOC_FACTS_REPLY with
 *                      IOCExceptions and changed DataImageSize to reserved.
 *                      Added MPI_FW_DOWNLOAD_ITYPE_NVSTORE_DATA and
 *                      MPI_FW_UPLOAD_ITYPE_NVDATA.
 *  09-28-01  01.02.03  Modified Event Data for Integrated RAID.
 *  11-01-01  01.02.04  Added defines for MPI_EXT_IMAGE_HEADER ImageType field.
 *  03-14-02  01.02.05  Added HeaderVersion field to MSG_IOC_FACTS_REPLY.
 *  05-31-02  01.02.06  Added define for
 *                      MPI_IOCFACTS_EXCEPT_RAID_CONFIG_INVALID.
 *                      Added AliasIndex to EVENT_DATA_LOGOUT structure.
 *  04-01-03  01.02.07  Added defines for MPI_FW_HEADER_SIGNATURE_.
 *  06-26-03  01.02.08  Added new values to the product family defines.
 *  04-29-04  01.02.09  Added IOCCapabilities field to MSG_IOC_FACTS_REPLY and
 *                      added related defines.
 *  05-11-04  01.03.01  Original release for MPI v1.3.
 *  08-19-04  01.05.01  Added four new fields to MSG_IOC_INIT.
 *                      Added three new fields to MSG_IOC_FACTS_REPLY.
 *                      Defined four new bits for the IOCCapabilities field of
 *                      the IOCFacts reply.
 *                      Added two new PortTypes for the PortFacts reply.
 *                      Added six new events along with their EventData
 *                      structures.
 *                      Added a new MsgFlag to the FwDownload request to
 *                      indicate last segment.
 *                      Defined a new image type of boot loader.
 *                      Added FW family codes for SAS product families.
 *  10-05-04  01.05.02  Added ReplyFifoHostSignalingAddr field to
 *                      MSG_IOC_FACTS_REPLY.
 *  12-07-04  01.05.03  Added more defines for SAS Discovery Error event.
 *  12-09-04  01.05.04  Added Unsupported device to SAS Device event.
 *  01-15-05  01.05.05  Added event data for SAS SES Event.
 *  02-09-05  01.05.06  Added MPI_FW_UPLOAD_ITYPE_FW_BACKUP define.
 *  02-22-05  01.05.07  Added Host Page Buffer Persistent flag to IOC Facts
 *                      Reply and IOC Init Request.
 *  03-11-05  01.05.08  Added family code for 1068E family.
 *                      Removed IOCFacts Reply EEDP Capability bit.
 *  06-24-05  01.05.09  Added 5 new IOCFacts Reply IOCCapabilities bits.
 *                      Added Max SATA Targets to SAS Discovery Error event.
 *  08-30-05  01.05.10  Added 4 new events and their event data structures.
 *                      Added new ReasonCode value for SAS Device Status Change
 *                      event.
 *                      Added new family code for FC949E.
 *  03-27-06  01.05.11  Added MPI_IOCFACTS_CAPABILITY_TLR.
 *                      Added additional Reason Codes and more event data fields
 *                      to EVENT_DATA_SAS_DEVICE_STATUS_CHANGE.
 *                      Added EVENT_DATA_SAS_BROADCAST_PRIMITIVE structure and
 *                      new event.
 *                      Added MPI_EVENT_SAS_SMP_ERROR and event data structure.
 *                      Added MPI_EVENT_SAS_INIT_DEVICE_STATUS_CHANGE and event
 *                      data structure.
 *                      Added MPI_EVENT_SAS_INIT_TABLE_OVERFLOW and event
 *                      data structure.
 *                      Added MPI_EXT_IMAGE_TYPE_INITIALIZATION.
 *  10-11-06  01.05.12  Added MPI_IOCFACTS_EXCEPT_METADATA_UNSUPPORTED.
 *                      Added MaxInitiators field to PortFacts reply.
 *                      Added SAS Device Status Change ReasonCode for
 *                      asynchronous notificaiton.
 *                      Added MPI_EVENT_SAS_EXPANDER_STATUS_CHANGE and event
 *                      data structure.
 *                      Added new ImageType values for FWDownload and FWUpload
 *                      requests.
 *  02-28-07  01.05.13  Added MPI_EVENT_PRIMITIVE_ASYNCHRONOUS_EVENT for SAS
 *                      Broadcast Event Data (replacing _RESERVED2).
 *                      For Discovery Error Event Data DiscoveryStatus field,
 *                      replaced _MULTPL_PATHS with _UNSUPPORTED_DEVICE and
 *                      added _MULTI_PORT_DOMAIN.
 *  05-24-07  01.05.14  Added Common Boot Block type to FWDownload Request.
 *                      Added Common Boot Block type to FWUpload Request.
 *  08-07-07  01.05.15  Added MPI_EVENT_SAS_INIT_RC_REMOVED define.
 *                      Added MPI_EVENT_IR2_RC_DUAL_PORT_ADDED and
 *                      MPI_EVENT_IR2_RC_DUAL_PORT_REMOVED for IR2 event data.
 *                      Added SASAddress field to SAS Initiator Device Table
 *                      Overflow event data structure.
 *  03-28-08  01.05.16  Added two new ReasonCode values to SAS Device Status
 *                      Change Event data to indicate completion of internally
 *                      generated task management.
 *                      Added MPI_EVENT_DSCVRY_ERR_DS_SATA_INIT_FAILURE define.
 *                      Added MPI_EVENT_SAS_INIT_RC_INACCESSIBLE define.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI_IOC_H
#define MPI_IOC_H




typedef struct _MSG_IOC_INIT
{
    U8                      WhoInit;                    
    U8                      Reserved;                   
    U8                      ChainOffset;                
    U8                      Function;                   
    U8                      Flags;                      
    U8                      MaxDevices;                 
    U8                      MaxBuses;                   
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U16                     ReplyFrameSize;             
    U8                      Reserved1[2];               
    U32                     HostMfaHighAddr;            
    U32                     SenseBufferHighAddr;        
    U32                     ReplyFifoHostSignalingAddr; 
    SGE_SIMPLE_UNION        HostPageBufferSGE;          
    U16                     MsgVersion;                 
    U16                     HeaderVersion;              
} MSG_IOC_INIT, MPI_POINTER PTR_MSG_IOC_INIT,
  IOCInit_t, MPI_POINTER pIOCInit_t;

#define MPI_WHOINIT_NO_ONE                              (0x00)
#define MPI_WHOINIT_SYSTEM_BIOS                         (0x01)
#define MPI_WHOINIT_ROM_BIOS                            (0x02)
#define MPI_WHOINIT_PCI_PEER                            (0x03)
#define MPI_WHOINIT_HOST_DRIVER                         (0x04)
#define MPI_WHOINIT_MANUFACTURER                        (0x05)

#define MPI_IOCINIT_FLAGS_HOST_PAGE_BUFFER_PERSISTENT   (0x04)
#define MPI_IOCINIT_FLAGS_REPLY_FIFO_HOST_SIGNAL        (0x02)
#define MPI_IOCINIT_FLAGS_DISCARD_FW_IMAGE              (0x01)

#define MPI_IOCINIT_MSGVERSION_MAJOR_MASK               (0xFF00)
#define MPI_IOCINIT_MSGVERSION_MAJOR_SHIFT              (8)
#define MPI_IOCINIT_MSGVERSION_MINOR_MASK               (0x00FF)
#define MPI_IOCINIT_MSGVERSION_MINOR_SHIFT              (0)

#define MPI_IOCINIT_HEADERVERSION_UNIT_MASK             (0xFF00)
#define MPI_IOCINIT_HEADERVERSION_UNIT_SHIFT            (8)
#define MPI_IOCINIT_HEADERVERSION_DEV_MASK              (0x00FF)
#define MPI_IOCINIT_HEADERVERSION_DEV_SHIFT             (0)


typedef struct _MSG_IOC_INIT_REPLY
{
    U8                      WhoInit;                    
    U8                      Reserved;                   
    U8                      MsgLength;                  
    U8                      Function;                   
    U8                      Flags;                      
    U8                      MaxDevices;                 
    U8                      MaxBuses;                   
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U16                     Reserved2;                  
    U16                     IOCStatus;                  
    U32                     IOCLogInfo;                 
} MSG_IOC_INIT_REPLY, MPI_POINTER PTR_MSG_IOC_INIT_REPLY,
  IOCInitReply_t, MPI_POINTER pIOCInitReply_t;




typedef struct _MSG_IOC_FACTS
{
    U8                      Reserved[2];                
    U8                      ChainOffset;                
    U8                      Function;                   
    U8                      Reserved1[3];               
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
} MSG_IOC_FACTS, MPI_POINTER PTR_IOC_FACTS,
  IOCFacts_t, MPI_POINTER pIOCFacts_t;

typedef struct _MPI_FW_VERSION_STRUCT
{
    U8                      Dev;                        
    U8                      Unit;                       
    U8                      Minor;                      
    U8                      Major;                      
} MPI_FW_VERSION_STRUCT;

typedef union _MPI_FW_VERSION
{
    MPI_FW_VERSION_STRUCT   Struct;
    U32                     Word;
} MPI_FW_VERSION;

typedef struct _MSG_IOC_FACTS_REPLY
{
    U16                     MsgVersion;                 
    U8                      MsgLength;                  
    U8                      Function;                   
    U16                     HeaderVersion;              
    U8                      IOCNumber;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U16                     IOCExceptions;              
    U16                     IOCStatus;                  
    U32                     IOCLogInfo;                 
    U8                      MaxChainDepth;              
    U8                      WhoInit;                    
    U8                      BlockSize;                  
    U8                      Flags;                      
    U16                     ReplyQueueDepth;            
    U16                     RequestFrameSize;           
    U16                     Reserved_0101_FWVersion;     
    U16                     ProductID;                  
    U32                     CurrentHostMfaHighAddr;     
    U16                     GlobalCredits;              
    U8                      NumberOfPorts;              
    U8                      EventState;                 
    U32                     CurrentSenseBufferHighAddr; 
    U16                     CurReplyFrameSize;          
    U8                      MaxDevices;                 
    U8                      MaxBuses;                   
    U32                     FWImageSize;                
    U32                     IOCCapabilities;            
    MPI_FW_VERSION          FWVersion;                  
    U16                     HighPriorityQueueDepth;     
    U16                     Reserved2;                  
    SGE_SIMPLE_UNION        HostPageBufferSGE;          
    U32                     ReplyFifoHostSignalingAddr; 
} MSG_IOC_FACTS_REPLY, MPI_POINTER PTR_MSG_IOC_FACTS_REPLY,
  IOCFactsReply_t, MPI_POINTER pIOCFactsReply_t;

#define MPI_IOCFACTS_MSGVERSION_MAJOR_MASK              (0xFF00)
#define MPI_IOCFACTS_MSGVERSION_MAJOR_SHIFT             (8)
#define MPI_IOCFACTS_MSGVERSION_MINOR_MASK              (0x00FF)
#define MPI_IOCFACTS_MSGVERSION_MINOR_SHIFT             (0)

#define MPI_IOCFACTS_HDRVERSION_UNIT_MASK               (0xFF00)
#define MPI_IOCFACTS_HDRVERSION_UNIT_SHIFT              (8)
#define MPI_IOCFACTS_HDRVERSION_DEV_MASK                (0x00FF)
#define MPI_IOCFACTS_HDRVERSION_DEV_SHIFT               (0)

#define MPI_IOCFACTS_EXCEPT_CONFIG_CHECKSUM_FAIL        (0x0001)
#define MPI_IOCFACTS_EXCEPT_RAID_CONFIG_INVALID         (0x0002)
#define MPI_IOCFACTS_EXCEPT_FW_CHECKSUM_FAIL            (0x0004)
#define MPI_IOCFACTS_EXCEPT_PERSISTENT_TABLE_FULL       (0x0008)
#define MPI_IOCFACTS_EXCEPT_METADATA_UNSUPPORTED        (0x0010)

#define MPI_IOCFACTS_FLAGS_FW_DOWNLOAD_BOOT             (0x01)
#define MPI_IOCFACTS_FLAGS_REPLY_FIFO_HOST_SIGNAL       (0x02)
#define MPI_IOCFACTS_FLAGS_HOST_PAGE_BUFFER_PERSISTENT  (0x04)

#define MPI_IOCFACTS_EVENTSTATE_DISABLED                (0x00)
#define MPI_IOCFACTS_EVENTSTATE_ENABLED                 (0x01)

#define MPI_IOCFACTS_CAPABILITY_HIGH_PRI_Q              (0x00000001)
#define MPI_IOCFACTS_CAPABILITY_REPLY_HOST_SIGNAL       (0x00000002)
#define MPI_IOCFACTS_CAPABILITY_QUEUE_FULL_HANDLING     (0x00000004)
#define MPI_IOCFACTS_CAPABILITY_DIAG_TRACE_BUFFER       (0x00000008)
#define MPI_IOCFACTS_CAPABILITY_SNAPSHOT_BUFFER         (0x00000010)
#define MPI_IOCFACTS_CAPABILITY_EXTENDED_BUFFER         (0x00000020)
#define MPI_IOCFACTS_CAPABILITY_EEDP                    (0x00000040)
#define MPI_IOCFACTS_CAPABILITY_BIDIRECTIONAL           (0x00000080)
#define MPI_IOCFACTS_CAPABILITY_MULTICAST               (0x00000100)
#define MPI_IOCFACTS_CAPABILITY_SCSIIO32                (0x00000200)
#define MPI_IOCFACTS_CAPABILITY_NO_SCSIIO16             (0x00000400)
#define MPI_IOCFACTS_CAPABILITY_TLR                     (0x00000800)




typedef struct _MSG_PORT_FACTS
{
     U8                     Reserved[2];                
     U8                     ChainOffset;                
     U8                     Function;                   
     U8                     Reserved1[2];               
     U8                     PortNumber;                 
     U8                     MsgFlags;                   
     U32                    MsgContext;                 
} MSG_PORT_FACTS, MPI_POINTER PTR_MSG_PORT_FACTS,
  PortFacts_t, MPI_POINTER pPortFacts_t;

typedef struct _MSG_PORT_FACTS_REPLY
{
     U16                    Reserved;                   
     U8                     MsgLength;                  
     U8                     Function;                   
     U16                    Reserved1;                  
     U8                     PortNumber;                 
     U8                     MsgFlags;                   
     U32                    MsgContext;                 
     U16                    Reserved2;                  
     U16                    IOCStatus;                  
     U32                    IOCLogInfo;                 
     U8                     Reserved3;                  
     U8                     PortType;                   
     U16                    MaxDevices;                 
     U16                    PortSCSIID;                 
     U16                    ProtocolFlags;              
     U16                    MaxPostedCmdBuffers;        
     U16                    MaxPersistentIDs;           
     U16                    MaxLanBuckets;              
     U8                     MaxInitiators;              
     U8                     Reserved4;                  
     U32                    Reserved5;                  
} MSG_PORT_FACTS_REPLY, MPI_POINTER PTR_MSG_PORT_FACTS_REPLY,
  PortFactsReply_t, MPI_POINTER pPortFactsReply_t;



#define MPI_PORTFACTS_PORTTYPE_INACTIVE         (0x00)
#define MPI_PORTFACTS_PORTTYPE_SCSI             (0x01)
#define MPI_PORTFACTS_PORTTYPE_FC               (0x10)
#define MPI_PORTFACTS_PORTTYPE_ISCSI            (0x20)
#define MPI_PORTFACTS_PORTTYPE_SAS              (0x30)


#define MPI_PORTFACTS_PROTOCOL_LOGBUSADDR       (0x01)
#define MPI_PORTFACTS_PROTOCOL_LAN              (0x02)
#define MPI_PORTFACTS_PROTOCOL_TARGET           (0x04)
#define MPI_PORTFACTS_PROTOCOL_INITIATOR        (0x08)



typedef struct _MSG_PORT_ENABLE
{
    U8                      Reserved[2];                
    U8                      ChainOffset;                
    U8                      Function;                   
    U8                      Reserved1[2];               
    U8                      PortNumber;                 
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
} MSG_PORT_ENABLE, MPI_POINTER PTR_MSG_PORT_ENABLE,
  PortEnable_t, MPI_POINTER pPortEnable_t;

typedef struct _MSG_PORT_ENABLE_REPLY
{
    U8                      Reserved[2];                
    U8                      MsgLength;                  
    U8                      Function;                   
    U8                      Reserved1[2];               
    U8                      PortNumber;                 
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U16                     Reserved2;                  
    U16                     IOCStatus;                  
    U32                     IOCLogInfo;                 
} MSG_PORT_ENABLE_REPLY, MPI_POINTER PTR_MSG_PORT_ENABLE_REPLY,
  PortEnableReply_t, MPI_POINTER pPortEnableReply_t;




typedef struct _MSG_EVENT_NOTIFY
{
    U8                      Switch;                     
    U8                      Reserved;                   
    U8                      ChainOffset;                
    U8                      Function;                   
    U8                      Reserved1[3];               
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
} MSG_EVENT_NOTIFY, MPI_POINTER PTR_MSG_EVENT_NOTIFY,
  EventNotification_t, MPI_POINTER pEventNotification_t;


typedef struct _MSG_EVENT_NOTIFY_REPLY
{
     U16                    EventDataLength;            
     U8                     MsgLength;                  
     U8                     Function;                   
     U8                     Reserved1[2];               
     U8                     AckRequired;                
     U8                     MsgFlags;                   
     U32                    MsgContext;                 
     U8                     Reserved2[2];               
     U16                    IOCStatus;                  
     U32                    IOCLogInfo;                 
     U32                    Event;                      
     U32                    EventContext;               
     U32                    Data[1];                    
} MSG_EVENT_NOTIFY_REPLY, MPI_POINTER PTR_MSG_EVENT_NOTIFY_REPLY,
  EventNotificationReply_t, MPI_POINTER pEventNotificationReply_t;


typedef struct _MSG_EVENT_ACK
{
    U8                      Reserved[2];                
    U8                      ChainOffset;                
    U8                      Function;                   
    U8                      Reserved1[3];               
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U32                     Event;                      
    U32                     EventContext;               
} MSG_EVENT_ACK, MPI_POINTER PTR_MSG_EVENT_ACK,
  EventAck_t, MPI_POINTER pEventAck_t;

typedef struct _MSG_EVENT_ACK_REPLY
{
    U8                      Reserved[2];                
    U8                      MsgLength;                  
    U8                      Function;                   
    U8                      Reserved1[3];               
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U16                     Reserved2;                  
    U16                     IOCStatus;                  
    U32                     IOCLogInfo;                 
} MSG_EVENT_ACK_REPLY, MPI_POINTER PTR_MSG_EVENT_ACK_REPLY,
  EventAckReply_t, MPI_POINTER pEventAckReply_t;


#define MPI_EVENT_NOTIFICATION_SWITCH_OFF   (0x00)
#define MPI_EVENT_NOTIFICATION_SWITCH_ON    (0x01)


#define MPI_EVENT_NONE                          (0x00000000)
#define MPI_EVENT_LOG_DATA                      (0x00000001)
#define MPI_EVENT_STATE_CHANGE                  (0x00000002)
#define MPI_EVENT_UNIT_ATTENTION                (0x00000003)
#define MPI_EVENT_IOC_BUS_RESET                 (0x00000004)
#define MPI_EVENT_EXT_BUS_RESET                 (0x00000005)
#define MPI_EVENT_RESCAN                        (0x00000006)
#define MPI_EVENT_LINK_STATUS_CHANGE            (0x00000007)
#define MPI_EVENT_LOOP_STATE_CHANGE             (0x00000008)
#define MPI_EVENT_LOGOUT                        (0x00000009)
#define MPI_EVENT_EVENT_CHANGE                  (0x0000000A)
#define MPI_EVENT_INTEGRATED_RAID               (0x0000000B)
#define MPI_EVENT_SCSI_DEVICE_STATUS_CHANGE     (0x0000000C)
#define MPI_EVENT_ON_BUS_TIMER_EXPIRED          (0x0000000D)
#define MPI_EVENT_QUEUE_FULL                    (0x0000000E)
#define MPI_EVENT_SAS_DEVICE_STATUS_CHANGE      (0x0000000F)
#define MPI_EVENT_SAS_SES                       (0x00000010)
#define MPI_EVENT_PERSISTENT_TABLE_FULL         (0x00000011)
#define MPI_EVENT_SAS_PHY_LINK_STATUS           (0x00000012)
#define MPI_EVENT_SAS_DISCOVERY_ERROR           (0x00000013)
#define MPI_EVENT_IR_RESYNC_UPDATE              (0x00000014)
#define MPI_EVENT_IR2                           (0x00000015)
#define MPI_EVENT_SAS_DISCOVERY                 (0x00000016)
#define MPI_EVENT_SAS_BROADCAST_PRIMITIVE       (0x00000017)
#define MPI_EVENT_SAS_INIT_DEVICE_STATUS_CHANGE (0x00000018)
#define MPI_EVENT_SAS_INIT_TABLE_OVERFLOW       (0x00000019)
#define MPI_EVENT_SAS_SMP_ERROR                 (0x0000001A)
#define MPI_EVENT_SAS_EXPANDER_STATUS_CHANGE    (0x0000001B)
#define MPI_EVENT_LOG_ENTRY_ADDED               (0x00000021)


#define MPI_EVENT_NOTIFICATION_ACK_NOT_REQUIRED (0x00)
#define MPI_EVENT_NOTIFICATION_ACK_REQUIRED     (0x01)


typedef struct _EVENT_DATA_EVENT_CHANGE
{
    U8                      EventState;                 
    U8                      Reserved;                   
    U16                     Reserved1;                  
} EVENT_DATA_EVENT_CHANGE, MPI_POINTER PTR_EVENT_DATA_EVENT_CHANGE,
  EventDataEventChange_t, MPI_POINTER pEventDataEventChange_t;


#define MPI_EVENT_DATA_LOG_ENTRY_DATA_LENGTH    (0x1C)
typedef struct _EVENT_DATA_LOG_ENTRY
{
    U32         TimeStamp;                          
    U32         Reserved1;                          
    U16         LogSequence;                        
    U16         LogEntryQualifier;                  
    U8          LogData[MPI_EVENT_DATA_LOG_ENTRY_DATA_LENGTH]; 
} EVENT_DATA_LOG_ENTRY, MPI_POINTER PTR_EVENT_DATA_LOG_ENTRY,
  MpiEventDataLogEntry_t, MPI_POINTER pMpiEventDataLogEntry_t;

typedef struct _EVENT_DATA_LOG_ENTRY_ADDED
{
    U16                     LogSequence;            
    U16                     Reserved1;              
    U32                     Reserved2;              
    EVENT_DATA_LOG_ENTRY    LogEntry;               
} EVENT_DATA_LOG_ENTRY_ADDED, MPI_POINTER PTR_EVENT_DATA_LOG_ENTRY_ADDED,
  MpiEventDataLogEntryAdded_t, MPI_POINTER pMpiEventDataLogEntryAdded_t;


typedef struct _EVENT_DATA_SCSI
{
    U8                      TargetID;                   
    U8                      BusPort;                    
    U16                     Reserved;                   
} EVENT_DATA_SCSI, MPI_POINTER PTR_EVENT_DATA_SCSI,
  EventDataScsi_t, MPI_POINTER pEventDataScsi_t;


typedef struct _EVENT_DATA_SCSI_DEVICE_STATUS_CHANGE
{
    U8                      TargetID;                   
    U8                      Bus;                        
    U8                      ReasonCode;                 
    U8                      LUN;                        
    U8                      ASC;                        
    U8                      ASCQ;                       
    U16                     Reserved;                   
} EVENT_DATA_SCSI_DEVICE_STATUS_CHANGE,
  MPI_POINTER PTR_EVENT_DATA_SCSI_DEVICE_STATUS_CHANGE,
  MpiEventDataScsiDeviceStatusChange_t,
  MPI_POINTER pMpiEventDataScsiDeviceStatusChange_t;

#define MPI_EVENT_SCSI_DEV_STAT_RC_ADDED                (0x03)
#define MPI_EVENT_SCSI_DEV_STAT_RC_NOT_RESPONDING       (0x04)
#define MPI_EVENT_SCSI_DEV_STAT_RC_SMART_DATA           (0x05)


typedef struct _EVENT_DATA_SAS_DEVICE_STATUS_CHANGE
{
    U8                      TargetID;                   
    U8                      Bus;                        
    U8                      ReasonCode;                 
    U8                      Reserved;                   
    U8                      ASC;                        
    U8                      ASCQ;                       
    U16                     DevHandle;                  
    U32                     DeviceInfo;                 
    U16                     ParentDevHandle;            
    U8                      PhyNum;                     
    U8                      Reserved1;                  
    U64                     SASAddress;                 
    U8                      LUN[8];                     
    U16                     TaskTag;                    
    U16                     Reserved2;                  
} EVENT_DATA_SAS_DEVICE_STATUS_CHANGE,
  MPI_POINTER PTR_EVENT_DATA_SAS_DEVICE_STATUS_CHANGE,
  MpiEventDataSasDeviceStatusChange_t,
  MPI_POINTER pMpiEventDataSasDeviceStatusChange_t;

#define MPI_EVENT_SAS_DEV_STAT_RC_ADDED                     (0x03)
#define MPI_EVENT_SAS_DEV_STAT_RC_NOT_RESPONDING            (0x04)
#define MPI_EVENT_SAS_DEV_STAT_RC_SMART_DATA                (0x05)
#define MPI_EVENT_SAS_DEV_STAT_RC_NO_PERSIST_ADDED          (0x06)
#define MPI_EVENT_SAS_DEV_STAT_RC_UNSUPPORTED               (0x07)
#define MPI_EVENT_SAS_DEV_STAT_RC_INTERNAL_DEVICE_RESET     (0x08)
#define MPI_EVENT_SAS_DEV_STAT_RC_TASK_ABORT_INTERNAL       (0x09)
#define MPI_EVENT_SAS_DEV_STAT_RC_ABORT_TASK_SET_INTERNAL   (0x0A)
#define MPI_EVENT_SAS_DEV_STAT_RC_CLEAR_TASK_SET_INTERNAL   (0x0B)
#define MPI_EVENT_SAS_DEV_STAT_RC_QUERY_TASK_INTERNAL       (0x0C)
#define MPI_EVENT_SAS_DEV_STAT_RC_ASYNC_NOTIFICATION        (0x0D)
#define MPI_EVENT_SAS_DEV_STAT_RC_CMPL_INTERNAL_DEV_RESET   (0x0E)
#define MPI_EVENT_SAS_DEV_STAT_RC_CMPL_TASK_ABORT_INTERNAL  (0x0F)



typedef struct _EVENT_DATA_QUEUE_FULL
{
    U8                      TargetID;                   
    U8                      Bus;                        
    U16                     CurrentDepth;               
} EVENT_DATA_QUEUE_FULL, MPI_POINTER PTR_EVENT_DATA_QUEUE_FULL,
  EventDataQueueFull_t, MPI_POINTER pEventDataQueueFull_t;


typedef struct _EVENT_DATA_RAID
{
    U8                      VolumeID;                   
    U8                      VolumeBus;                  
    U8                      ReasonCode;                 
    U8                      PhysDiskNum;                
    U8                      ASC;                        
    U8                      ASCQ;                       
    U16                     Reserved;                   
    U32                     SettingsStatus;             
} EVENT_DATA_RAID, MPI_POINTER PTR_EVENT_DATA_RAID,
  MpiEventDataRaid_t, MPI_POINTER pMpiEventDataRaid_t;

#define MPI_EVENT_RAID_RC_VOLUME_CREATED                (0x00)
#define MPI_EVENT_RAID_RC_VOLUME_DELETED                (0x01)
#define MPI_EVENT_RAID_RC_VOLUME_SETTINGS_CHANGED       (0x02)
#define MPI_EVENT_RAID_RC_VOLUME_STATUS_CHANGED         (0x03)
#define MPI_EVENT_RAID_RC_VOLUME_PHYSDISK_CHANGED       (0x04)
#define MPI_EVENT_RAID_RC_PHYSDISK_CREATED              (0x05)
#define MPI_EVENT_RAID_RC_PHYSDISK_DELETED              (0x06)
#define MPI_EVENT_RAID_RC_PHYSDISK_SETTINGS_CHANGED     (0x07)
#define MPI_EVENT_RAID_RC_PHYSDISK_STATUS_CHANGED       (0x08)
#define MPI_EVENT_RAID_RC_DOMAIN_VAL_NEEDED             (0x09)
#define MPI_EVENT_RAID_RC_SMART_DATA                    (0x0A)
#define MPI_EVENT_RAID_RC_REPLACE_ACTION_STARTED        (0x0B)



typedef struct _MPI_EVENT_DATA_IR_RESYNC_UPDATE
{
    U8                      VolumeID;                   
    U8                      VolumeBus;                  
    U8                      ResyncComplete;             
    U8                      Reserved1;                  
    U32                     Reserved2;                  
} MPI_EVENT_DATA_IR_RESYNC_UPDATE,
  MPI_POINTER PTR_MPI_EVENT_DATA_IR_RESYNC_UPDATE,
  MpiEventDataIrResyncUpdate_t, MPI_POINTER pMpiEventDataIrResyncUpdate_t;


typedef struct _IR2_STATE_CHANGED
{
    U16                 PreviousState;  
    U16                 NewState;       
} IR2_STATE_CHANGED, MPI_POINTER PTR_IR2_STATE_CHANGED;

typedef struct _IR2_PD_INFO
{
    U16                 DeviceHandle;           
    U8                  TruncEnclosureHandle;   
    U8                  TruncatedSlot;          
} IR2_PD_INFO, MPI_POINTER PTR_IR2_PD_INFO;

typedef union _MPI_IR2_RC_EVENT_DATA
{
    IR2_STATE_CHANGED   StateChanged;
    U32                 Lba;
    IR2_PD_INFO         PdInfo;
} MPI_IR2_RC_EVENT_DATA, MPI_POINTER PTR_MPI_IR2_RC_EVENT_DATA;

typedef struct _MPI_EVENT_DATA_IR2
{
    U8                      TargetID;             
    U8                      Bus;                  
    U8                      ReasonCode;           
    U8                      PhysDiskNum;          
    MPI_IR2_RC_EVENT_DATA   IR2EventData;         
} MPI_EVENT_DATA_IR2, MPI_POINTER PTR_MPI_EVENT_DATA_IR2,
  MpiEventDataIR2_t, MPI_POINTER pMpiEventDataIR2_t;

#define MPI_EVENT_IR2_RC_LD_STATE_CHANGED           (0x01)
#define MPI_EVENT_IR2_RC_PD_STATE_CHANGED           (0x02)
#define MPI_EVENT_IR2_RC_BAD_BLOCK_TABLE_FULL       (0x03)
#define MPI_EVENT_IR2_RC_PD_INSERTED                (0x04)
#define MPI_EVENT_IR2_RC_PD_REMOVED                 (0x05)
#define MPI_EVENT_IR2_RC_FOREIGN_CFG_DETECTED       (0x06)
#define MPI_EVENT_IR2_RC_REBUILD_MEDIUM_ERROR       (0x07)
#define MPI_EVENT_IR2_RC_DUAL_PORT_ADDED            (0x08)
#define MPI_EVENT_IR2_RC_DUAL_PORT_REMOVED          (0x09)

#define MPI_LD_STATE_OPTIMAL                        (0x00)
#define MPI_LD_STATE_DEGRADED                       (0x01)
#define MPI_LD_STATE_FAILED                         (0x02)
#define MPI_LD_STATE_MISSING                        (0x03)
#define MPI_LD_STATE_OFFLINE                        (0x04)

#define MPI_PD_STATE_ONLINE                         (0x00)
#define MPI_PD_STATE_MISSING                        (0x01)
#define MPI_PD_STATE_NOT_COMPATIBLE                 (0x02)
#define MPI_PD_STATE_FAILED                         (0x03)
#define MPI_PD_STATE_INITIALIZING                   (0x04)
#define MPI_PD_STATE_OFFLINE_AT_HOST_REQUEST        (0x05)
#define MPI_PD_STATE_FAILED_AT_HOST_REQUEST         (0x06)
#define MPI_PD_STATE_OFFLINE_FOR_ANOTHER_REASON     (0xFF)


typedef struct _EVENT_DATA_LINK_STATUS
{
    U8                      State;                      
    U8                      Reserved;                   
    U16                     Reserved1;                  
    U8                      Reserved2;                  
    U8                      Port;                       
    U16                     Reserved3;                  
} EVENT_DATA_LINK_STATUS, MPI_POINTER PTR_EVENT_DATA_LINK_STATUS,
  EventDataLinkStatus_t, MPI_POINTER pEventDataLinkStatus_t;

#define MPI_EVENT_LINK_STATUS_FAILURE       (0x00000000)
#define MPI_EVENT_LINK_STATUS_ACTIVE        (0x00000001)


typedef struct _EVENT_DATA_LOOP_STATE
{
    U8                      Character4;                 
    U8                      Character3;                 
    U8                      Type;                       
    U8                      Reserved;                   
    U8                      Reserved1;                  
    U8                      Port;                       
    U16                     Reserved2;                  
} EVENT_DATA_LOOP_STATE, MPI_POINTER PTR_EVENT_DATA_LOOP_STATE,
  EventDataLoopState_t, MPI_POINTER pEventDataLoopState_t;

#define MPI_EVENT_LOOP_STATE_CHANGE_LIP     (0x0001)
#define MPI_EVENT_LOOP_STATE_CHANGE_LPE     (0x0002)
#define MPI_EVENT_LOOP_STATE_CHANGE_LPB     (0x0003)


typedef struct _EVENT_DATA_LOGOUT
{
    U32                     NPortID;                    
    U8                      AliasIndex;                 
    U8                      Port;                       
    U16                     Reserved1;                  
} EVENT_DATA_LOGOUT, MPI_POINTER PTR_EVENT_DATA_LOGOUT,
  EventDataLogout_t, MPI_POINTER pEventDataLogout_t;

#define MPI_EVENT_LOGOUT_ALL_ALIASES        (0xFF)


typedef struct _EVENT_DATA_SAS_SES
{
    U8                      PhyNum;                     
    U8                      Port;                       
    U8                      PortWidth;                  
    U8                      Reserved1;                  
} EVENT_DATA_SAS_SES, MPI_POINTER PTR_EVENT_DATA_SAS_SES,
  MpiEventDataSasSes_t, MPI_POINTER pMpiEventDataSasSes_t;


typedef struct _EVENT_DATA_SAS_BROADCAST_PRIMITIVE
{
    U8                      PhyNum;                     
    U8                      Port;                       
    U8                      PortWidth;                  
    U8                      Primitive;                  
} EVENT_DATA_SAS_BROADCAST_PRIMITIVE,
  MPI_POINTER PTR_EVENT_DATA_SAS_BROADCAST_PRIMITIVE,
  MpiEventDataSasBroadcastPrimitive_t,
  MPI_POINTER pMpiEventDataSasBroadcastPrimitive_t;

#define MPI_EVENT_PRIMITIVE_CHANGE              (0x01)
#define MPI_EVENT_PRIMITIVE_EXPANDER            (0x03)
#define MPI_EVENT_PRIMITIVE_ASYNCHRONOUS_EVENT  (0x04)
#define MPI_EVENT_PRIMITIVE_RESERVED3           (0x05)
#define MPI_EVENT_PRIMITIVE_RESERVED4           (0x06)
#define MPI_EVENT_PRIMITIVE_CHANGE0_RESERVED    (0x07)
#define MPI_EVENT_PRIMITIVE_CHANGE1_RESERVED    (0x08)


typedef struct _EVENT_DATA_SAS_PHY_LINK_STATUS
{
    U8                      PhyNum;                     
    U8                      LinkRates;                  
    U16                     DevHandle;                  
    U64                     SASAddress;                 
} EVENT_DATA_SAS_PHY_LINK_STATUS, MPI_POINTER PTR_EVENT_DATA_SAS_PHY_LINK_STATUS,
  MpiEventDataSasPhyLinkStatus_t, MPI_POINTER pMpiEventDataSasPhyLinkStatus_t;

#define MPI_EVENT_SAS_PLS_LR_CURRENT_MASK                   (0xF0)
#define MPI_EVENT_SAS_PLS_LR_CURRENT_SHIFT                  (4)
#define MPI_EVENT_SAS_PLS_LR_PREVIOUS_MASK                  (0x0F)
#define MPI_EVENT_SAS_PLS_LR_PREVIOUS_SHIFT                 (0)
#define MPI_EVENT_SAS_PLS_LR_RATE_UNKNOWN                   (0x00)
#define MPI_EVENT_SAS_PLS_LR_RATE_PHY_DISABLED              (0x01)
#define MPI_EVENT_SAS_PLS_LR_RATE_FAILED_SPEED_NEGOTIATION  (0x02)
#define MPI_EVENT_SAS_PLS_LR_RATE_SATA_OOB_COMPLETE         (0x03)
#define MPI_EVENT_SAS_PLS_LR_RATE_1_5                       (0x08)
#define MPI_EVENT_SAS_PLS_LR_RATE_3_0                       (0x09)
#define MPI_EVENT_SAS_PLS_LR_RATE_6_0                       (0x0A)


typedef struct _EVENT_DATA_SAS_DISCOVERY
{
    U32                     DiscoveryStatus;            
    U32                     Reserved1;                  
} EVENT_DATA_SAS_DISCOVERY, MPI_POINTER PTR_EVENT_DATA_SAS_DISCOVERY,
  EventDataSasDiscovery_t, MPI_POINTER pEventDataSasDiscovery_t;

#define MPI_EVENT_SAS_DSCVRY_COMPLETE                       (0x00000000)
#define MPI_EVENT_SAS_DSCVRY_IN_PROGRESS                    (0x00000001)
#define MPI_EVENT_SAS_DSCVRY_PHY_BITS_MASK                  (0xFFFF0000)
#define MPI_EVENT_SAS_DSCVRY_PHY_BITS_SHIFT                 (16)


typedef struct _EVENT_DATA_DISCOVERY_ERROR
{
    U32                     DiscoveryStatus;            
    U8                      Port;                       
    U8                      Reserved1;                  
    U16                     Reserved2;                  
} EVENT_DATA_DISCOVERY_ERROR, MPI_POINTER PTR_EVENT_DATA_DISCOVERY_ERROR,
  EventDataDiscoveryError_t, MPI_POINTER pEventDataDiscoveryError_t;

#define MPI_EVENT_DSCVRY_ERR_DS_LOOP_DETECTED               (0x00000001)
#define MPI_EVENT_DSCVRY_ERR_DS_UNADDRESSABLE_DEVICE        (0x00000002)
#define MPI_EVENT_DSCVRY_ERR_DS_MULTIPLE_PORTS              (0x00000004)
#define MPI_EVENT_DSCVRY_ERR_DS_EXPANDER_ERR                (0x00000008)
#define MPI_EVENT_DSCVRY_ERR_DS_SMP_TIMEOUT                 (0x00000010)
#define MPI_EVENT_DSCVRY_ERR_DS_OUT_ROUTE_ENTRIES           (0x00000020)
#define MPI_EVENT_DSCVRY_ERR_DS_INDEX_NOT_EXIST             (0x00000040)
#define MPI_EVENT_DSCVRY_ERR_DS_SMP_FUNCTION_FAILED         (0x00000080)
#define MPI_EVENT_DSCVRY_ERR_DS_SMP_CRC_ERROR               (0x00000100)
#define MPI_EVENT_DSCVRY_ERR_DS_MULTPL_SUBTRACTIVE          (0x00000200)
#define MPI_EVENT_DSCVRY_ERR_DS_TABLE_TO_TABLE              (0x00000400)
#define MPI_EVENT_DSCVRY_ERR_DS_UNSUPPORTED_DEVICE          (0x00000800)
#define MPI_EVENT_DSCVRY_ERR_DS_MAX_SATA_TARGETS            (0x00001000)
#define MPI_EVENT_DSCVRY_ERR_DS_MULTI_PORT_DOMAIN           (0x00002000)
#define MPI_EVENT_DSCVRY_ERR_DS_SATA_INIT_FAILURE           (0x00004000)


typedef struct _EVENT_DATA_SAS_SMP_ERROR
{
    U8                      Status;                     
    U8                      Port;                       
    U8                      SMPFunctionResult;          
    U8                      Reserved1;                  
    U64                     SASAddress;                 
} EVENT_DATA_SAS_SMP_ERROR, MPI_POINTER PTR_EVENT_DATA_SAS_SMP_ERROR,
  MpiEventDataSasSmpError_t, MPI_POINTER pMpiEventDataSasSmpError_t;

#define MPI_EVENT_SAS_SMP_FUNCTION_RESULT_VALID         (0x00)
#define MPI_EVENT_SAS_SMP_CRC_ERROR                     (0x01)
#define MPI_EVENT_SAS_SMP_TIMEOUT                       (0x02)
#define MPI_EVENT_SAS_SMP_NO_DESTINATION                (0x03)
#define MPI_EVENT_SAS_SMP_BAD_DESTINATION               (0x04)


typedef struct _EVENT_DATA_SAS_INIT_DEV_STATUS_CHANGE
{
    U8                      ReasonCode;                 
    U8                      Port;                       
    U16                     DevHandle;                  
    U64                     SASAddress;                 
} EVENT_DATA_SAS_INIT_DEV_STATUS_CHANGE,
  MPI_POINTER PTR_EVENT_DATA_SAS_INIT_DEV_STATUS_CHANGE,
  MpiEventDataSasInitDevStatusChange_t,
  MPI_POINTER pMpiEventDataSasInitDevStatusChange_t;

#define MPI_EVENT_SAS_INIT_RC_ADDED                 (0x01)
#define MPI_EVENT_SAS_INIT_RC_REMOVED               (0x02)
#define MPI_EVENT_SAS_INIT_RC_INACCESSIBLE          (0x03)


typedef struct _EVENT_DATA_SAS_INIT_TABLE_OVERFLOW
{
    U8                      MaxInit;                    
    U8                      CurrentInit;                
    U16                     Reserved1;                  
    U64                     SASAddress;                 
} EVENT_DATA_SAS_INIT_TABLE_OVERFLOW,
  MPI_POINTER PTR_EVENT_DATA_SAS_INIT_TABLE_OVERFLOW,
  MpiEventDataSasInitTableOverflow_t,
  MPI_POINTER pMpiEventDataSasInitTableOverflow_t;


typedef struct _EVENT_DATA_SAS_EXPANDER_STATUS_CHANGE
{
    U8                      ReasonCode;             
    U8                      Reserved1;              
    U16                     Reserved2;              
    U8                      PhysicalPort;           
    U8                      Reserved3;              
    U16                     EnclosureHandle;        
    U64                     SASAddress;             
    U32                     DiscoveryStatus;        
    U16                     DevHandle;              
    U16                     ParentDevHandle;        
    U16                     ExpanderChangeCount;    
    U16                     ExpanderRouteIndexes;   
    U8                      NumPhys;                
    U8                      SASLevel;               
    U8                      Flags;                  
    U8                      Reserved4;              
} EVENT_DATA_SAS_EXPANDER_STATUS_CHANGE,
  MPI_POINTER PTR_EVENT_DATA_SAS_EXPANDER_STATUS_CHANGE,
  MpiEventDataSasExpanderStatusChange_t,
  MPI_POINTER pMpiEventDataSasExpanderStatusChange_t;

#define MPI_EVENT_SAS_EXP_RC_ADDED                      (0x00)
#define MPI_EVENT_SAS_EXP_RC_NOT_RESPONDING             (0x01)

#define MPI_EVENT_SAS_EXP_DS_LOOP_DETECTED              (0x00000001)
#define MPI_EVENT_SAS_EXP_DS_UNADDRESSABLE_DEVICE       (0x00000002)
#define MPI_EVENT_SAS_EXP_DS_MULTIPLE_PORTS             (0x00000004)
#define MPI_EVENT_SAS_EXP_DS_EXPANDER_ERR               (0x00000008)
#define MPI_EVENT_SAS_EXP_DS_SMP_TIMEOUT                (0x00000010)
#define MPI_EVENT_SAS_EXP_DS_OUT_ROUTE_ENTRIES          (0x00000020)
#define MPI_EVENT_SAS_EXP_DS_INDEX_NOT_EXIST            (0x00000040)
#define MPI_EVENT_SAS_EXP_DS_SMP_FUNCTION_FAILED        (0x00000080)
#define MPI_EVENT_SAS_EXP_DS_SMP_CRC_ERROR              (0x00000100)
#define MPI_EVENT_SAS_EXP_DS_SUBTRACTIVE_LINK           (0x00000200)
#define MPI_EVENT_SAS_EXP_DS_TABLE_LINK                 (0x00000400)
#define MPI_EVENT_SAS_EXP_DS_UNSUPPORTED_DEVICE         (0x00000800)

#define MPI_EVENT_SAS_EXP_FLAGS_ROUTE_TABLE_CONFIG      (0x02)
#define MPI_EVENT_SAS_EXP_FLAGS_CONFIG_IN_PROGRESS      (0x01)





typedef struct _MSG_FW_DOWNLOAD
{
    U8                      ImageType;                  
    U8                      Reserved;                   
    U8                      ChainOffset;                
    U8                      Function;                   
    U8                      Reserved1[3];               
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    SGE_MPI_UNION           SGL;                        
} MSG_FW_DOWNLOAD, MPI_POINTER PTR_MSG_FW_DOWNLOAD,
  FWDownload_t, MPI_POINTER pFWDownload_t;

#define MPI_FW_DOWNLOAD_MSGFLGS_LAST_SEGMENT    (0x01)

#define MPI_FW_DOWNLOAD_ITYPE_RESERVED          (0x00)
#define MPI_FW_DOWNLOAD_ITYPE_FW                (0x01)
#define MPI_FW_DOWNLOAD_ITYPE_BIOS              (0x02)
#define MPI_FW_DOWNLOAD_ITYPE_NVDATA            (0x03)
#define MPI_FW_DOWNLOAD_ITYPE_BOOTLOADER        (0x04)
#define MPI_FW_DOWNLOAD_ITYPE_MANUFACTURING     (0x06)
#define MPI_FW_DOWNLOAD_ITYPE_CONFIG_1          (0x07)
#define MPI_FW_DOWNLOAD_ITYPE_CONFIG_2          (0x08)
#define MPI_FW_DOWNLOAD_ITYPE_MEGARAID          (0x09)
#define MPI_FW_DOWNLOAD_ITYPE_COMMON_BOOT_BLOCK (0x0B)


typedef struct _FWDownloadTCSGE
{
    U8                      Reserved;                   
    U8                      ContextSize;                
    U8                      DetailsLength;              
    U8                      Flags;                      
    U32                     Reserved_0100_Checksum;      
    U32                     ImageOffset;                
    U32                     ImageSize;                  
} FW_DOWNLOAD_TCSGE, MPI_POINTER PTR_FW_DOWNLOAD_TCSGE,
  FWDownloadTCSGE_t, MPI_POINTER pFWDownloadTCSGE_t;

typedef struct _MSG_FW_DOWNLOAD_REPLY
{
    U8                      ImageType;                  
    U8                      Reserved;                   
    U8                      MsgLength;                  
    U8                      Function;                   
    U8                      Reserved1[3];               
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U16                     Reserved2;                  
    U16                     IOCStatus;                  
    U32                     IOCLogInfo;                 
} MSG_FW_DOWNLOAD_REPLY, MPI_POINTER PTR_MSG_FW_DOWNLOAD_REPLY,
  FWDownloadReply_t, MPI_POINTER pFWDownloadReply_t;



typedef struct _MSG_FW_UPLOAD
{
    U8                      ImageType;                  
    U8                      Reserved;                   
    U8                      ChainOffset;                
    U8                      Function;                   
    U8                      Reserved1[3];               
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    SGE_MPI_UNION           SGL;                        
} MSG_FW_UPLOAD, MPI_POINTER PTR_MSG_FW_UPLOAD,
  FWUpload_t, MPI_POINTER pFWUpload_t;

#define MPI_FW_UPLOAD_ITYPE_FW_IOC_MEM          (0x00)
#define MPI_FW_UPLOAD_ITYPE_FW_FLASH            (0x01)
#define MPI_FW_UPLOAD_ITYPE_BIOS_FLASH          (0x02)
#define MPI_FW_UPLOAD_ITYPE_NVDATA              (0x03)
#define MPI_FW_UPLOAD_ITYPE_BOOTLOADER          (0x04)
#define MPI_FW_UPLOAD_ITYPE_FW_BACKUP           (0x05)
#define MPI_FW_UPLOAD_ITYPE_MANUFACTURING       (0x06)
#define MPI_FW_UPLOAD_ITYPE_CONFIG_1            (0x07)
#define MPI_FW_UPLOAD_ITYPE_CONFIG_2            (0x08)
#define MPI_FW_UPLOAD_ITYPE_MEGARAID            (0x09)
#define MPI_FW_UPLOAD_ITYPE_COMPLETE            (0x0A)
#define MPI_FW_UPLOAD_ITYPE_COMMON_BOOT_BLOCK   (0x0B)

typedef struct _FWUploadTCSGE
{
    U8                      Reserved;                   
    U8                      ContextSize;                
    U8                      DetailsLength;              
    U8                      Flags;                      
    U32                     Reserved1;                  
    U32                     ImageOffset;                
    U32                     ImageSize;                  
} FW_UPLOAD_TCSGE, MPI_POINTER PTR_FW_UPLOAD_TCSGE,
  FWUploadTCSGE_t, MPI_POINTER pFWUploadTCSGE_t;

typedef struct _MSG_FW_UPLOAD_REPLY
{
    U8                      ImageType;                  
    U8                      Reserved;                   
    U8                      MsgLength;                  
    U8                      Function;                   
    U8                      Reserved1[3];               
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U16                     Reserved2;                  
    U16                     IOCStatus;                  
    U32                     IOCLogInfo;                 
    U32                     ActualImageSize;            
} MSG_FW_UPLOAD_REPLY, MPI_POINTER PTR_MSG_FW_UPLOAD_REPLY,
  FWUploadReply_t, MPI_POINTER pFWUploadReply_t;


typedef struct _MPI_FW_HEADER
{
    U32                     ArmBranchInstruction0;      
    U32                     Signature0;                 
    U32                     Signature1;                 
    U32                     Signature2;                 
    U32                     ArmBranchInstruction1;      
    U32                     ArmBranchInstruction2;      
    U32                     Reserved;                   
    U32                     Checksum;                   
    U16                     VendorId;                   
    U16                     ProductId;                  
    MPI_FW_VERSION          FWVersion;                  
    U32                     SeqCodeVersion;             
    U32                     ImageSize;                  
    U32                     NextImageHeaderOffset;      
    U32                     LoadStartAddress;           
    U32                     IopResetVectorValue;        
    U32                     IopResetRegAddr;            
    U32                     VersionNameWhat;            
    U8                      VersionName[32];            
    U32                     VendorNameWhat;             
    U8                      VendorName[32];             
} MPI_FW_HEADER, MPI_POINTER PTR_MPI_FW_HEADER,
  MpiFwHeader_t, MPI_POINTER pMpiFwHeader_t;

#define MPI_FW_HEADER_WHAT_SIGNATURE        (0x29232840)

#define MPI_FW_HEADER_PID_TYPE_MASK             (0xF000)
#define MPI_FW_HEADER_PID_TYPE_SCSI             (0x0000)
#define MPI_FW_HEADER_PID_TYPE_FC               (0x1000)
#define MPI_FW_HEADER_PID_TYPE_SAS              (0x2000)

#define MPI_FW_HEADER_SIGNATURE_0               (0x5AEAA55A)
#define MPI_FW_HEADER_SIGNATURE_1               (0xA55AEAA5)
#define MPI_FW_HEADER_SIGNATURE_2               (0x5AA55AEA)

#define MPI_FW_HEADER_PID_PROD_MASK                     (0x0F00)
#define MPI_FW_HEADER_PID_PROD_INITIATOR_SCSI           (0x0100)
#define MPI_FW_HEADER_PID_PROD_TARGET_INITIATOR_SCSI    (0x0200)
#define MPI_FW_HEADER_PID_PROD_TARGET_SCSI              (0x0300)
#define MPI_FW_HEADER_PID_PROD_IM_SCSI                  (0x0400)
#define MPI_FW_HEADER_PID_PROD_IS_SCSI                  (0x0500)
#define MPI_FW_HEADER_PID_PROD_CTX_SCSI                 (0x0600)
#define MPI_FW_HEADER_PID_PROD_IR_SCSI                  (0x0700)

#define MPI_FW_HEADER_PID_FAMILY_MASK           (0x00FF)
#define MPI_FW_HEADER_PID_FAMILY_1030A0_SCSI    (0x0001)
#define MPI_FW_HEADER_PID_FAMILY_1030B0_SCSI    (0x0002)
#define MPI_FW_HEADER_PID_FAMILY_1030B1_SCSI    (0x0003)
#define MPI_FW_HEADER_PID_FAMILY_1030C0_SCSI    (0x0004)
#define MPI_FW_HEADER_PID_FAMILY_1020A0_SCSI    (0x0005)
#define MPI_FW_HEADER_PID_FAMILY_1020B0_SCSI    (0x0006)
#define MPI_FW_HEADER_PID_FAMILY_1020B1_SCSI    (0x0007)
#define MPI_FW_HEADER_PID_FAMILY_1020C0_SCSI    (0x0008)
#define MPI_FW_HEADER_PID_FAMILY_1035A0_SCSI    (0x0009)
#define MPI_FW_HEADER_PID_FAMILY_1035B0_SCSI    (0x000A)
#define MPI_FW_HEADER_PID_FAMILY_1030TA0_SCSI   (0x000B)
#define MPI_FW_HEADER_PID_FAMILY_1020TA0_SCSI   (0x000C)
#define MPI_FW_HEADER_PID_FAMILY_909_FC         (0x0000)
#define MPI_FW_HEADER_PID_FAMILY_919_FC         (0x0001) 
#define MPI_FW_HEADER_PID_FAMILY_919X_FC        (0x0002) 
#define MPI_FW_HEADER_PID_FAMILY_919XL_FC       (0x0003) 
#define MPI_FW_HEADER_PID_FAMILY_939X_FC        (0x0004) 
#define MPI_FW_HEADER_PID_FAMILY_959_FC         (0x0005)
#define MPI_FW_HEADER_PID_FAMILY_949E_FC        (0x0006)
#define MPI_FW_HEADER_PID_FAMILY_1064_SAS       (0x0001)
#define MPI_FW_HEADER_PID_FAMILY_1068_SAS       (0x0002)
#define MPI_FW_HEADER_PID_FAMILY_1078_SAS       (0x0003)
#define MPI_FW_HEADER_PID_FAMILY_106xE_SAS      (0x0004) 

typedef struct _MPI_EXT_IMAGE_HEADER
{
    U8                      ImageType;                  
    U8                      Reserved;                   
    U16                     Reserved1;                  
    U32                     Checksum;                   
    U32                     ImageSize;                  
    U32                     NextImageHeaderOffset;      
    U32                     LoadStartAddress;           
    U32                     Reserved2;                  
} MPI_EXT_IMAGE_HEADER, MPI_POINTER PTR_MPI_EXT_IMAGE_HEADER,
  MpiExtImageHeader_t, MPI_POINTER pMpiExtImageHeader_t;

#define MPI_EXT_IMAGE_TYPE_UNSPECIFIED          (0x00)
#define MPI_EXT_IMAGE_TYPE_FW                   (0x01)
#define MPI_EXT_IMAGE_TYPE_NVDATA               (0x03)
#define MPI_EXT_IMAGE_TYPE_BOOTLOADER           (0x04)
#define MPI_EXT_IMAGE_TYPE_INITIALIZATION       (0x05)

#endif
