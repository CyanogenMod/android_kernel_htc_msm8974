/*
 *  Copyright (c) 2001-2008 LSI Corporation.
 *
 *
 *           Name:  mpi_tool.h
 *          Title:  MPI Toolbox structures and definitions
 *  Creation Date:  July 30, 2001
 *
 *    mpi_tool.h Version:  01.05.03
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  08-08-01  01.02.01  Original release.
 *  08-29-01  01.02.02  Added DIAG_DATA_UPLOAD_HEADER and related defines.
 *  01-16-04  01.02.03  Added defines and structures for new tools
 *.                     MPI_TOOLBOX_ISTWI_READ_WRITE_TOOL and
 *                      MPI_TOOLBOX_FC_MANAGEMENT_TOOL.
 *  04-29-04  01.02.04  Added message structures for Diagnostic Buffer Post and
 *                      Diagnostic Release requests and replies.
 *  05-11-04  01.03.01  Original release for MPI v1.3.
 *  08-19-04  01.05.01  Original release for MPI v1.5.
 *  10-06-04  01.05.02  Added define for MPI_DIAG_BUF_TYPE_COUNT.
 *  02-09-05  01.05.03  Added frame size option to FC management tool.
 *                      Added Beacon tool to the Toolbox.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI_TOOL_H
#define MPI_TOOL_H

#define MPI_TOOLBOX_CLEAN_TOOL                      (0x00)
#define MPI_TOOLBOX_MEMORY_MOVE_TOOL                (0x01)
#define MPI_TOOLBOX_DIAG_DATA_UPLOAD_TOOL           (0x02)
#define MPI_TOOLBOX_ISTWI_READ_WRITE_TOOL           (0x03)
#define MPI_TOOLBOX_FC_MANAGEMENT_TOOL              (0x04)
#define MPI_TOOLBOX_BEACON_TOOL                     (0x05)



typedef struct _MSG_TOOLBOX_REPLY
{
    U8                      Tool;                       
    U8                      Reserved;                   
    U8                      MsgLength;                  
    U8                      Function;                   
    U16                     Reserved1;                  
    U8                      Reserved2;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U16                     Reserved3;                  
    U16                     IOCStatus;                  
    U32                     IOCLogInfo;                 
} MSG_TOOLBOX_REPLY, MPI_POINTER PTR_MSG_TOOLBOX_REPLY,
  ToolboxReply_t, MPI_POINTER pToolboxReply_t;



typedef struct _MSG_TOOLBOX_CLEAN_REQUEST
{
    U8                      Tool;                       
    U8                      Reserved;                   
    U8                      ChainOffset;                
    U8                      Function;                   
    U16                     Reserved1;                  
    U8                      Reserved2;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U32                     Flags;                      
} MSG_TOOLBOX_CLEAN_REQUEST, MPI_POINTER PTR_MSG_TOOLBOX_CLEAN_REQUEST,
  ToolboxCleanRequest_t, MPI_POINTER pToolboxCleanRequest_t;

#define MPI_TOOLBOX_CLEAN_NVSRAM                    (0x00000001)
#define MPI_TOOLBOX_CLEAN_SEEPROM                   (0x00000002)
#define MPI_TOOLBOX_CLEAN_FLASH                     (0x00000004)
#define MPI_TOOLBOX_CLEAN_BOOTLOADER                (0x04000000)
#define MPI_TOOLBOX_CLEAN_FW_BACKUP                 (0x08000000)
#define MPI_TOOLBOX_CLEAN_FW_CURRENT                (0x10000000)
#define MPI_TOOLBOX_CLEAN_OTHER_PERSIST_PAGES       (0x20000000)
#define MPI_TOOLBOX_CLEAN_PERSIST_MANUFACT_PAGES    (0x40000000)
#define MPI_TOOLBOX_CLEAN_BOOT_SERVICES             (0x80000000)



typedef struct _MSG_TOOLBOX_MEM_MOVE_REQUEST
{
    U8                      Tool;                       
    U8                      Reserved;                   
    U8                      ChainOffset;                
    U8                      Function;                   
    U16                     Reserved1;                  
    U8                      Reserved2;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    SGE_SIMPLE_UNION        SGL;                        
} MSG_TOOLBOX_MEM_MOVE_REQUEST, MPI_POINTER PTR_MSG_TOOLBOX_MEM_MOVE_REQUEST,
  ToolboxMemMoveRequest_t, MPI_POINTER pToolboxMemMoveRequest_t;



typedef struct _MSG_TOOLBOX_DIAG_DATA_UPLOAD_REQUEST
{
    U8                      Tool;                       
    U8                      Reserved;                   
    U8                      ChainOffset;                
    U8                      Function;                   
    U16                     Reserved1;                  
    U8                      Reserved2;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U32                     Flags;                      
    U32                     Reserved3;                  
    SGE_SIMPLE_UNION        SGL;                        
} MSG_TOOLBOX_DIAG_DATA_UPLOAD_REQUEST, MPI_POINTER PTR_MSG_TOOLBOX_DIAG_DATA_UPLOAD_REQUEST,
  ToolboxDiagDataUploadRequest_t, MPI_POINTER pToolboxDiagDataUploadRequest_t;

typedef struct _DIAG_DATA_UPLOAD_HEADER
{
    U32                     DiagDataLength;             
    U8                      FormatCode;                 
    U8                      Reserved;                   
    U16                     Reserved1;                  
} DIAG_DATA_UPLOAD_HEADER, MPI_POINTER PTR_DIAG_DATA_UPLOAD_HEADER,
  DiagDataUploadHeader_t, MPI_POINTER pDiagDataUploadHeader_t;

#define MPI_TB_DIAG_FORMAT_SCSI_PRINTF_1            (0x01)
#define MPI_TB_DIAG_FORMAT_SCSI_2                   (0x02)
#define MPI_TB_DIAG_FORMAT_SCSI_3                   (0x03)
#define MPI_TB_DIAG_FORMAT_FC_TRACE_1               (0x04)



typedef struct _MSG_TOOLBOX_ISTWI_READ_WRITE_REQUEST
{
    U8                      Tool;                       
    U8                      Reserved;                   
    U8                      ChainOffset;                
    U8                      Function;                   
    U16                     Reserved1;                  
    U8                      Reserved2;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U8                      Flags;                      
    U8                      BusNum;                     
    U16                     Reserved3;                  
    U8                      NumAddressBytes;            
    U8                      Reserved4;                  
    U16                     DataLength;                 
    U8                      DeviceAddr;                 
    U8                      Addr1;                      
    U8                      Addr2;                      
    U8                      Addr3;                      
    U32                     Reserved5;                  
    SGE_SIMPLE_UNION        SGL;                        
} MSG_TOOLBOX_ISTWI_READ_WRITE_REQUEST, MPI_POINTER PTR_MSG_TOOLBOX_ISTWI_READ_WRITE_REQUEST,
  ToolboxIstwiReadWriteRequest_t, MPI_POINTER pToolboxIstwiReadWriteRequest_t;

#define MPI_TB_ISTWI_FLAGS_WRITE                    (0x00)
#define MPI_TB_ISTWI_FLAGS_READ                     (0x01)



typedef struct _MPI_TB_FC_MANAGE_BUS_TID_AI
{
    U16                     Reserved;                   
    U8                      Bus;                        
    U8                      TargetId;                   
} MPI_TB_FC_MANAGE_BUS_TID_AI, MPI_POINTER PTR_MPI_TB_FC_MANAGE_BUS_TID_AI,
  MpiTbFcManageBusTidAi_t, MPI_POINTER pMpiTbFcManageBusTidAi_t;

typedef struct _MPI_TB_FC_MANAGE_PID_AI
{
    U32                     PortIdentifier;             
} MPI_TB_FC_MANAGE_PID_AI, MPI_POINTER PTR_MPI_TB_FC_MANAGE_PID_AI,
  MpiTbFcManagePidAi_t, MPI_POINTER pMpiTbFcManagePidAi_t;

typedef struct _MPI_TB_FC_MANAGE_FRAME_SIZE_AI
{
    U16                     FrameSize;                  
    U8                      PortNum;                    
    U8                      Reserved1;                  
} MPI_TB_FC_MANAGE_FRAME_SIZE_AI, MPI_POINTER PTR_MPI_TB_FC_MANAGE_FRAME_SIZE_AI,
  MpiTbFcManageFrameSizeAi_t, MPI_POINTER pMpiTbFcManageFrameSizeAi_t;

typedef union _MPI_TB_FC_MANAGE_AI_UNION
{
    MPI_TB_FC_MANAGE_BUS_TID_AI     BusTid;
    MPI_TB_FC_MANAGE_PID_AI         Port;
    MPI_TB_FC_MANAGE_FRAME_SIZE_AI  FrameSize;
} MPI_TB_FC_MANAGE_AI_UNION, MPI_POINTER PTR_MPI_TB_FC_MANAGE_AI_UNION,
  MpiTbFcManageAiUnion_t, MPI_POINTER pMpiTbFcManageAiUnion_t;

typedef struct _MSG_TOOLBOX_FC_MANAGE_REQUEST
{
    U8                          Tool;                   
    U8                          Reserved;               
    U8                          ChainOffset;            
    U8                          Function;               
    U16                         Reserved1;              
    U8                          Reserved2;              
    U8                          MsgFlags;               
    U32                         MsgContext;             
    U8                          Action;                 
    U8                          Reserved3;              
    U16                         Reserved4;              
    MPI_TB_FC_MANAGE_AI_UNION   ActionInfo;             
} MSG_TOOLBOX_FC_MANAGE_REQUEST, MPI_POINTER PTR_MSG_TOOLBOX_FC_MANAGE_REQUEST,
  ToolboxFcManageRequest_t, MPI_POINTER pToolboxFcManageRequest_t;

#define MPI_TB_FC_MANAGE_ACTION_DISC_ALL            (0x00)
#define MPI_TB_FC_MANAGE_ACTION_DISC_PID            (0x01)
#define MPI_TB_FC_MANAGE_ACTION_DISC_BUS_TID        (0x02)
#define MPI_TB_FC_MANAGE_ACTION_SET_MAX_FRAME_SIZE  (0x03)



typedef struct _MSG_TOOLBOX_BEACON_REQUEST
{
    U8                      Tool;                       
    U8                      Reserved;                   
    U8                      ChainOffset;                
    U8                      Function;                   
    U16                     Reserved1;                  
    U8                      Reserved2;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U8                      ConnectNum;                 
    U8                      PortNum;                    
    U8                      Reserved3;                  
    U8                      Flags;                      
} MSG_TOOLBOX_BEACON_REQUEST, MPI_POINTER PTR_MSG_TOOLBOX_BEACON_REQUEST,
  ToolboxBeaconRequest_t, MPI_POINTER pToolboxBeaconRequest_t;

#define MPI_TOOLBOX_FLAGS_BEACON_MODE_OFF       (0x00)
#define MPI_TOOLBOX_FLAGS_BEACON_MODE_ON        (0x01)



typedef struct _MSG_DIAG_BUFFER_POST_REQUEST
{
    U8                      TraceLevel;                 
    U8                      BufferType;                 
    U8                      ChainOffset;                
    U8                      Function;                   
    U16                     Reserved1;                  
    U8                      Reserved2;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U32                     ExtendedType;               
    U32                     BufferLength;               
    U32                     ProductSpecific[4];         
    U32                     Reserved3;                  
    U64                     BufferAddress;              
} MSG_DIAG_BUFFER_POST_REQUEST, MPI_POINTER PTR_MSG_DIAG_BUFFER_POST_REQUEST,
  DiagBufferPostRequest_t, MPI_POINTER pDiagBufferPostRequest_t;

#define MPI_DIAG_BUF_TYPE_TRACE                     (0x00)
#define MPI_DIAG_BUF_TYPE_SNAPSHOT                  (0x01)
#define MPI_DIAG_BUF_TYPE_EXTENDED                  (0x02)
#define MPI_DIAG_BUF_TYPE_COUNT                     (0x03)

#define MPI_DIAG_EXTENDED_QTAG                      (0x00000001)


typedef struct _MSG_DIAG_BUFFER_POST_REPLY
{
    U8                      Reserved1;                  
    U8                      BufferType;                 
    U8                      MsgLength;                  
    U8                      Function;                   
    U16                     Reserved2;                  
    U8                      Reserved3;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U16                     Reserved4;                  
    U16                     IOCStatus;                  
    U32                     IOCLogInfo;                 
    U32                     TransferLength;             
} MSG_DIAG_BUFFER_POST_REPLY, MPI_POINTER PTR_MSG_DIAG_BUFFER_POST_REPLY,
  DiagBufferPostReply_t, MPI_POINTER pDiagBufferPostReply_t;



typedef struct _MSG_DIAG_RELEASE_REQUEST
{
    U8                      Reserved1;                  
    U8                      BufferType;                 
    U8                      ChainOffset;                
    U8                      Function;                   
    U16                     Reserved2;                  
    U8                      Reserved3;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
} MSG_DIAG_RELEASE_REQUEST, MPI_POINTER PTR_MSG_DIAG_RELEASE_REQUEST,
  DiagReleaseRequest_t, MPI_POINTER pDiagReleaseRequest_t;


typedef struct _MSG_DIAG_RELEASE_REPLY
{
    U8                      Reserved1;                  
    U8                      BufferType;                 
    U8                      MsgLength;                  
    U8                      Function;                   
    U16                     Reserved2;                  
    U8                      Reserved3;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U16                     Reserved4;                  
    U16                     IOCStatus;                  
    U32                     IOCLogInfo;                 
} MSG_DIAG_RELEASE_REPLY, MPI_POINTER PTR_MSG_DIAG_RELEASE_REPLY,
  DiagReleaseReply_t, MPI_POINTER pDiagReleaseReply_t;


#endif


