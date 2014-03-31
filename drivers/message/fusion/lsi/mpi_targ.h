/*
 *  Copyright (c) 2000-2008 LSI Corporation.
 *
 *
 *           Name:  mpi_targ.h
 *          Title:  MPI Target mode messages and structures
 *  Creation Date:  June 22, 2000
 *
 *    mpi_targ.h Version:  01.05.06
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  05-08-00  00.10.01  Original release for 0.10 spec dated 4/26/2000.
 *  06-06-00  01.00.01  Update version number for 1.0 release.
 *  06-22-00  01.00.02  Added _MSG_TARGET_CMD_BUFFER_POST_REPLY structure.
 *                      Corrected DECSRIPTOR typo to DESCRIPTOR.
 *  11-02-00  01.01.01  Original release for post 1.0 work
 *                      Modified target mode to use IoIndex instead of
 *                      HostIndex and IocIndex. Added Alias.
 *  01-09-01  01.01.02  Added defines for TARGET_ASSIST_FLAGS_REPOST_CMD_BUFFER
 *                      and TARGET_STATUS_SEND_FLAGS_REPOST_CMD_BUFFER.
 *  02-20-01  01.01.03  Started using MPI_POINTER.
 *                      Added structures for MPI_TARGET_SCSI_SPI_CMD_BUFFER and
 *                      MPI_TARGET_FCP_CMD_BUFFER.
 *  03-27-01  01.01.04  Added structure offset comments.
 *  08-08-01  01.02.01  Original release for v1.2 work.
 *  09-28-01  01.02.02  Added structure for MPI_TARGET_SCSI_SPI_STATUS_IU.
 *                      Added PriorityReason field to some replies and
 *                      defined more PriorityReason codes.
 *                      Added some defines for to support previous version
 *                      of MPI.
 *  10-04-01  01.02.03  Added PriorityReason to MSG_TARGET_ERROR_REPLY.
 *  11-01-01  01.02.04  Added define for TARGET_STATUS_SEND_FLAGS_HIGH_PRIORITY.
 *  03-14-02  01.02.05  Modified MPI_TARGET_FCP_RSP_BUFFER to get the proper
 *                      byte ordering.
 *  05-31-02  01.02.06  Modified TARGET_MODE_REPLY_ALIAS_MASK to only include
 *                      one bit.
 *                      Added AliasIndex field to MPI_TARGET_FCP_CMD_BUFFER.
 *  09-16-02  01.02.07  Added flags for confirmed completion.
 *                      Added PRIORITY_REASON_TARGET_BUSY.
 *  11-15-02  01.02.08  Added AliasID field to MPI_TARGET_SCSI_SPI_CMD_BUFFER.
 *  04-01-03  01.02.09  Added OptionalOxid field to MPI_TARGET_FCP_CMD_BUFFER.
 *  05-11-04  01.03.01  Original release for MPI v1.3.
 *  08-19-04  01.05.01  Added new request message structures for
 *                      MSG_TARGET_CMD_BUF_POST_BASE_REQUEST,
 *                      MSG_TARGET_CMD_BUF_POST_LIST_REQUEST, and
 *                      MSG_TARGET_ASSIST_EXT_REQUEST.
 *                      Added new structures for SAS SSP Command buffer, SSP
 *                      Task buffer, and SSP Status IU.
 *  10-05-04  01.05.02  MSG_TARGET_CMD_BUFFER_POST_BASE_LIST_REPLY added.
 *  02-22-05  01.05.03  Changed a comment.
 *  03-11-05  01.05.04  Removed TargetAssistExtended Request.
 *  06-24-05  01.05.05  Added TargetAssistExtended structures and defines.
 *  03-27-06  01.05.06  Added a comment.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI_TARG_H
#define MPI_TARG_H



typedef struct _CMD_BUFFER_DESCRIPTOR
{
    U16                     IoIndex;                    
    U16                     Reserved;                   
    union                                               
    {
        U32                 PhysicalAddress32;
        U64                 PhysicalAddress64;
    } u;
} CMD_BUFFER_DESCRIPTOR, MPI_POINTER PTR_CMD_BUFFER_DESCRIPTOR,
  CmdBufferDescriptor_t, MPI_POINTER pCmdBufferDescriptor_t;



typedef struct _MSG_TARGET_CMD_BUFFER_POST_REQUEST
{
    U8                      BufferPostFlags;            
    U8                      BufferCount;                
    U8                      ChainOffset;                
    U8                      Function;                   
    U8                      BufferLength;               
    U8                      Reserved;                   
    U8                      Reserved1;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    CMD_BUFFER_DESCRIPTOR   Buffer[1];                  
} MSG_TARGET_CMD_BUFFER_POST_REQUEST, MPI_POINTER PTR_MSG_TARGET_CMD_BUFFER_POST_REQUEST,
  TargetCmdBufferPostRequest_t, MPI_POINTER pTargetCmdBufferPostRequest_t;

#define CMD_BUFFER_POST_FLAGS_PORT_MASK         (0x01)
#define CMD_BUFFER_POST_FLAGS_ADDR_MODE_MASK    (0x80)
#define CMD_BUFFER_POST_FLAGS_ADDR_MODE_32      (0)
#define CMD_BUFFER_POST_FLAGS_ADDR_MODE_64      (1)
#define CMD_BUFFER_POST_FLAGS_64_BIT_ADDR       (0x80)

#define CMD_BUFFER_POST_IO_INDEX_MASK           (0x00003FFF)
#define CMD_BUFFER_POST_IO_INDEX_MASK_0100      (0x000003FF) 


typedef struct _MSG_TARGET_CMD_BUFFER_POST_REPLY
{
    U8                      BufferPostFlags;            
    U8                      BufferCount;                
    U8                      MsgLength;                  
    U8                      Function;                   
    U8                      BufferLength;               
    U8                      Reserved;                   
    U8                      Reserved1;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U16                     Reserved2;                  
    U16                     IOCStatus;                  
    U32                     IOCLogInfo;                 
} MSG_TARGET_CMD_BUFFER_POST_REPLY, MPI_POINTER PTR_MSG_TARGET_CMD_BUFFER_POST_REPLY,
  TargetCmdBufferPostReply_t, MPI_POINTER pTargetCmdBufferPostReply_t;

typedef struct _MSG_PRIORITY_CMD_RECEIVED_REPLY
{
    U16                     Reserved;                   
    U8                      MsgLength;                  
    U8                      Function;                   
    U16                     Reserved1;                  
    U8                      Reserved2;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U8                      PriorityReason;             
    U8                      Reserved3;                  
    U16                     IOCStatus;                  
    U32                     IOCLogInfo;                 
    U32                     ReplyWord;                  
} MSG_PRIORITY_CMD_RECEIVED_REPLY, MPI_POINTER PTR_MSG_PRIORITY_CMD_RECEIVED_REPLY,
  PriorityCommandReceivedReply_t, MPI_POINTER pPriorityCommandReceivedReply_t;


typedef struct _MSG_TARGET_CMD_BUFFER_POST_ERROR_REPLY
{
    U16                     Reserved;                   
    U8                      MsgLength;                  
    U8                      Function;                   
    U16                     Reserved1;                  
    U8                      Reserved2;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U8                      PriorityReason;             
    U8                      Reserved3;                  
    U16                     IOCStatus;                  
    U32                     IOCLogInfo;                 
    U32                     ReplyWord;                  
} MSG_TARGET_CMD_BUFFER_POST_ERROR_REPLY,
  MPI_POINTER PTR_MSG_TARGET_CMD_BUFFER_POST_ERROR_REPLY,
  TargetCmdBufferPostErrorReply_t, MPI_POINTER pTargetCmdBufferPostErrorReply_t;

#define PRIORITY_REASON_NO_DISCONNECT           (0x00)
#define PRIORITY_REASON_SCSI_TASK_MANAGEMENT    (0x01)
#define PRIORITY_REASON_CMD_PARITY_ERR          (0x02)
#define PRIORITY_REASON_MSG_OUT_PARITY_ERR      (0x03)
#define PRIORITY_REASON_LQ_CRC_ERR              (0x04)
#define PRIORITY_REASON_CMD_CRC_ERR             (0x05)
#define PRIORITY_REASON_PROTOCOL_ERR            (0x06)
#define PRIORITY_REASON_DATA_OUT_PARITY_ERR     (0x07)
#define PRIORITY_REASON_DATA_OUT_CRC_ERR        (0x08)
#define PRIORITY_REASON_TARGET_BUSY             (0x09)
#define PRIORITY_REASON_UNKNOWN                 (0xFF)



typedef struct _MSG_TARGET_CMD_BUF_POST_BASE_REQUEST
{
    U8                      BufferPostFlags;            
    U8                      PortNumber;                 
    U8                      ChainOffset;                
    U8                      Function;                   
    U16                     TotalCmdBuffers;            
    U8                      Reserved;                   
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U32                     Reserved1;                  
    U16                     CmdBufferLength;            
    U16                     NextCmdBufferOffset;        
    U32                     BaseAddressLow;             
    U32                     BaseAddressHigh;            
} MSG_TARGET_CMD_BUF_POST_BASE_REQUEST,
  MPI_POINTER PTR__MSG_TARGET_CMD_BUF_POST_BASE_REQUEST,
  TargetCmdBufferPostBaseRequest_t,
  MPI_POINTER pTargetCmdBufferPostBaseRequest_t;

#define CMD_BUFFER_POST_BASE_FLAGS_AUTO_POST_ALL    (0x01)


typedef struct _MSG_TARGET_CMD_BUFFER_POST_BASE_LIST_REPLY
{
    U16                     Reserved;                   
    U8                      MsgLength;                  
    U8                      Function;                   
    U16                     Reserved1;                  
    U8                      Reserved2;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U16                     Reserved3;                  
    U16                     IOCStatus;                  
    U32                     IOCLogInfo;                 
} MSG_TARGET_CMD_BUFFER_POST_BASE_LIST_REPLY,
  MPI_POINTER PTR_MSG_TARGET_CMD_BUFFER_POST_BASE_LIST_REPLY,
  TargetCmdBufferPostBaseListReply_t,
  MPI_POINTER pTargetCmdBufferPostBaseListReply_t;



typedef struct _MSG_TARGET_CMD_BUF_POST_LIST_REQUEST
{
    U8                      Reserved;                   
    U8                      PortNumber;                 
    U8                      ChainOffset;                
    U8                      Function;                   
    U16                     CmdBufferCount;             
    U8                      Reserved1;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U32                     Reserved2;                  
    U16                     IoIndex[2];                 
} MSG_TARGET_CMD_BUF_POST_LIST_REQUEST,
  MPI_POINTER PTR_MSG_TARGET_CMD_BUF_POST_LIST_REQUEST,
  TargetCmdBufferPostListRequest_t,
  MPI_POINTER pTargetCmdBufferPostListRequest_t;



typedef struct _MPI_TARGET_FCP_CMD_BUFFER
{
    U8      FcpLun[8];                                  
    U8      FcpCntl[4];                                 
    U8      FcpCdb[16];                                 
    U32     FcpDl;                                      
    U8      AliasIndex;                                 
    U8      Reserved1;                                  
    U16     OptionalOxid;                               
} MPI_TARGET_FCP_CMD_BUFFER, MPI_POINTER PTR_MPI_TARGET_FCP_CMD_BUFFER,
  MpiTargetFcpCmdBuffer, MPI_POINTER pMpiTargetFcpCmdBuffer;


typedef struct _MPI_TARGET_SCSI_SPI_CMD_BUFFER
{
    
    U8      L_QType;                                    
    U8      Reserved;                                   
    U16     Tag;                                        
    U8      LogicalUnitNumber[8];                       
    U32     DataLength;                                 
    
    U8      ReservedFirstByteOfCommandIU;               
    U8      TaskAttribute;                              
    U8      TaskManagementFlags;                        
    U8      AdditionalCDBLength;                        
    U8      CDB[16];                                    
    
    U8      AliasID;                                    
    U8      Reserved1;                                  
    U16     Reserved2;                                  
} MPI_TARGET_SCSI_SPI_CMD_BUFFER,
  MPI_POINTER PTR_MPI_TARGET_SCSI_SPI_CMD_BUFFER,
  MpiTargetScsiSpiCmdBuffer, MPI_POINTER pMpiTargetScsiSpiCmdBuffer;


typedef struct _MPI_TARGET_SSP_CMD_BUFFER
{
    U8      FrameType;                                  
    U8      Reserved1;                                  
    U16     Reserved2;                                  
    U16     InitiatorTag;                               
    U16     DevHandle;                                  
    
    U8      LogicalUnitNumber[8];                       
    U8      Reserved3;                                  
    U8      TaskAttribute;            
    U8      Reserved4;                                  
    U8      AdditionalCDBLength;      
    U8      CDB[16];                                    
    
} MPI_TARGET_SSP_CMD_BUFFER, MPI_POINTER PTR_MPI_TARGET_SSP_CMD_BUFFER,
  MpiTargetSspCmdBuffer, MPI_POINTER pMpiTargetSspCmdBuffer;

typedef struct _MPI_TARGET_SSP_TASK_BUFFER
{
    U8      FrameType;                                  
    U8      Reserved1;                                  
    U16     Reserved2;                                  
    U16     InitiatorTag;                               
    U16     DevHandle;                                  
    
    U8      LogicalUnitNumber[8];                       
    U8      Reserved3;                                  
    U8      Reserved4;                                  
    U8      TaskManagementFunction;                     
    U8      Reserved5;                                  
    U16     ManagedTaskTag;                             
    U16     Reserved6;                                  
    U32     Reserved7;                                  
    U32     Reserved8;                                  
    U32     Reserved9;                                  
} MPI_TARGET_SSP_TASK_BUFFER, MPI_POINTER PTR_MPI_TARGET_SSP_TASK_BUFFER,
  MpiTargetSspTaskBuffer, MPI_POINTER pMpiTargetSspTaskBuffer;



typedef struct _MSG_TARGET_ASSIST_REQUEST
{
    U8                      StatusCode;                 
    U8                      TargetAssistFlags;          
    U8                      ChainOffset;                
    U8                      Function;                   
    U16                     QueueTag;                   
    U8                      Reserved;                   
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U32                     ReplyWord;                  
    U8                      LUN[8];                     
    U32                     RelativeOffset;             
    U32                     DataLength;                 
    SGE_IO_UNION            SGL[1];                     
} MSG_TARGET_ASSIST_REQUEST, MPI_POINTER PTR_MSG_TARGET_ASSIST_REQUEST,
  TargetAssistRequest_t, MPI_POINTER pTargetAssistRequest_t;

#define TARGET_ASSIST_FLAGS_DATA_DIRECTION          (0x01)
#define TARGET_ASSIST_FLAGS_AUTO_STATUS             (0x02)
#define TARGET_ASSIST_FLAGS_HIGH_PRIORITY           (0x04)
#define TARGET_ASSIST_FLAGS_CONFIRMED               (0x08)
#define TARGET_ASSIST_FLAGS_REPOST_CMD_BUFFER       (0x80)

typedef struct _MSG_TARGET_ERROR_REPLY
{
    U16                     Reserved;                   
    U8                      MsgLength;                  
    U8                      Function;                   
    U16                     Reserved1;                  
    U8                      Reserved2;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U8                      PriorityReason;             
    U8                      Reserved3;                  
    U16                     IOCStatus;                  
    U32                     IOCLogInfo;                 
    U32                     ReplyWord;                  
    U32                     TransferCount;              
} MSG_TARGET_ERROR_REPLY, MPI_POINTER PTR_MSG_TARGET_ERROR_REPLY,
  TargetErrorReply_t, MPI_POINTER pTargetErrorReply_t;



typedef struct _MSG_TARGET_ASSIST_EXT_REQUEST
{
    U8                      StatusCode;                     
    U8                      TargetAssistFlags;              
    U8                      ChainOffset;                    
    U8                      Function;                       
    U16                     QueueTag;                       
    U8                      Reserved1;                      
    U8                      MsgFlags;                       
    U32                     MsgContext;                     
    U32                     ReplyWord;                      
    U8                      LUN[8];                         
    U32                     RelativeOffset;                 
    U32                     Reserved2;                      
    U32                     Reserved3;                      
    U32                     PrimaryReferenceTag;            
    U16                     PrimaryApplicationTag;          
    U16                     PrimaryApplicationTagMask;      
    U32                     Reserved4;                      
    U32                     DataLength;                     
    U32                     BidirectionalDataLength;        
    U32                     SecondaryReferenceTag;          
    U16                     SecondaryApplicationTag;        
    U16                     Reserved5;                      
    U16                     EEDPFlags;                      
    U16                     ApplicationTagTranslationMask;  
    U32                     EEDPBlockSize;                  
    U8                      SGLOffset0;                     
    U8                      SGLOffset1;                     
    U8                      SGLOffset2;                     
    U8                      SGLOffset3;                     
    U32                     Reserved6;                      
    SGE_IO_UNION            SGL[1];                         
} MSG_TARGET_ASSIST_EXT_REQUEST, MPI_POINTER PTR_MSG_TARGET_ASSIST_EXT_REQUEST,
  TargetAssistExtRequest_t, MPI_POINTER pTargetAssistExtRequest_t;


#define TARGET_ASSIST_EXT_MSGFLAGS_BIDIRECTIONAL        (0x20)
#define TARGET_ASSIST_EXT_MSGFLAGS_MULTICAST            (0x10)
#define TARGET_ASSIST_EXT_MSGFLAGS_SGL_OFFSET_CHAINS    (0x08)

#define TARGET_ASSIST_EXT_EEDP_MASK_OP          (0x0007)
#define TARGET_ASSIST_EXT_EEDP_NOOP_OP          (0x0000)
#define TARGET_ASSIST_EXT_EEDP_CHK_OP           (0x0001)
#define TARGET_ASSIST_EXT_EEDP_STRIP_OP         (0x0002)
#define TARGET_ASSIST_EXT_EEDP_CHKRM_OP         (0x0003)
#define TARGET_ASSIST_EXT_EEDP_INSERT_OP        (0x0004)
#define TARGET_ASSIST_EXT_EEDP_REPLACE_OP       (0x0006)
#define TARGET_ASSIST_EXT_EEDP_CHKREGEN_OP      (0x0007)

#define TARGET_ASSIST_EXT_EEDP_PASS_REF_TAG     (0x0008)

#define TARGET_ASSIST_EXT_EEDP_T10_CHK_MASK     (0x0700)
#define TARGET_ASSIST_EXT_EEDP_T10_CHK_GUARD    (0x0100)
#define TARGET_ASSIST_EXT_EEDP_T10_CHK_APPTAG   (0x0200)
#define TARGET_ASSIST_EXT_EEDP_T10_CHK_REFTAG   (0x0400)
#define TARGET_ASSIST_EXT_EEDP_T10_CHK_SHIFT    (8)

#define TARGET_ASSIST_EXT_EEDP_INC_SEC_APPTAG   (0x1000)
#define TARGET_ASSIST_EXT_EEDP_INC_PRI_APPTAG   (0x2000)
#define TARGET_ASSIST_EXT_EEDP_INC_SEC_REFTAG   (0x4000)
#define TARGET_ASSIST_EXT_EEDP_INC_PRI_REFTAG   (0x8000)



typedef struct _MSG_TARGET_STATUS_SEND_REQUEST
{
    U8                      StatusCode;                 
    U8                      StatusFlags;                
    U8                      ChainOffset;                
    U8                      Function;                   
    U16                     QueueTag;                   
    U8                      Reserved;                   
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U32                     ReplyWord;                  
    U8                      LUN[8];                     
    SGE_SIMPLE_UNION        StatusDataSGE;              
} MSG_TARGET_STATUS_SEND_REQUEST, MPI_POINTER PTR_MSG_TARGET_STATUS_SEND_REQUEST,
  TargetStatusSendRequest_t, MPI_POINTER pTargetStatusSendRequest_t;

#define TARGET_STATUS_SEND_FLAGS_AUTO_GOOD_STATUS   (0x01)
#define TARGET_STATUS_SEND_FLAGS_HIGH_PRIORITY      (0x04)
#define TARGET_STATUS_SEND_FLAGS_CONFIRMED          (0x08)
#define TARGET_STATUS_SEND_FLAGS_REPOST_CMD_BUFFER  (0x80)

typedef struct _MPI_TARGET_FCP_RSP_BUFFER
{
    U8      Reserved0[8];                               
    U8      Reserved1[2];                               
    U8      FcpFlags;                                   
    U8      FcpStatus;                                  
    U32     FcpResid;                                   
    U32     FcpSenseLength;                             
    U32     FcpResponseLength;                          
    U8      FcpResponseData[8];                         
    U8      FcpSenseData[32];      
} MPI_TARGET_FCP_RSP_BUFFER, MPI_POINTER PTR_MPI_TARGET_FCP_RSP_BUFFER,
  MpiTargetFcpRspBuffer, MPI_POINTER pMpiTargetFcpRspBuffer;

typedef struct _MPI_TARGET_SCSI_SPI_STATUS_IU
{
    U8      Reserved0;                                  
    U8      Reserved1;                                  
    U8      Valid;                                      
    U8      Status;                                     
    U32     SenseDataListLength;                        
    U32     PktFailuresListLength;                      
    U8      SenseData[52];  
} MPI_TARGET_SCSI_SPI_STATUS_IU, MPI_POINTER PTR_MPI_TARGET_SCSI_SPI_STATUS_IU,
  TargetScsiSpiStatusIU_t, MPI_POINTER pTargetScsiSpiStatusIU_t;

typedef struct _MPI_TARGET_SSP_RSP_IU
{
    U32     Reserved0[6];  
    
    U32     Reserved1;                                  
    U32     Reserved2;                                  
    U16     Reserved3;                                  
    U8      DataPres;                 
    U8      Status;                                     
    U32     Reserved4;                                  
    U32     SenseDataLength;                            
    U32     ResponseDataLength;                         
    U8      ResponseSenseData[4];                       
} MPI_TARGET_SSP_RSP_IU, MPI_POINTER PTR_MPI_TARGET_SSP_RSP_IU,
  MpiTargetSspRspIu_t, MPI_POINTER pMpiTargetSspRspIu_t;



typedef struct _MSG_TARGET_MODE_ABORT_REQUEST
{
    U8                      AbortType;                  
    U8                      Reserved;                   
    U8                      ChainOffset;                
    U8                      Function;                   
    U16                     Reserved1;                  
    U8                      Reserved2;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U32                     ReplyWord;                  
    U32                     MsgContextToAbort;          
} MSG_TARGET_MODE_ABORT, MPI_POINTER PTR_MSG_TARGET_MODE_ABORT,
  TargetModeAbort_t, MPI_POINTER pTargetModeAbort_t;

#define TARGET_MODE_ABORT_TYPE_ALL_CMD_BUFFERS      (0x00)
#define TARGET_MODE_ABORT_TYPE_ALL_IO               (0x01)
#define TARGET_MODE_ABORT_TYPE_EXACT_IO             (0x02)
#define TARGET_MODE_ABORT_TYPE_EXACT_IO_REQUEST     (0x03)


typedef struct _MSG_TARGET_MODE_ABORT_REPLY
{
    U16                     Reserved;                   
    U8                      MsgLength;                  
    U8                      Function;                   
    U16                     Reserved1;                  
    U8                      Reserved2;                  
    U8                      MsgFlags;                   
    U32                     MsgContext;                 
    U16                     Reserved3;                  
    U16                     IOCStatus;                  
    U32                     IOCLogInfo;                 
    U32                     AbortCount;                 
} MSG_TARGET_MODE_ABORT_REPLY, MPI_POINTER PTR_MSG_TARGET_MODE_ABORT_REPLY,
  TargetModeAbortReply_t, MPI_POINTER pTargetModeAbortReply_t;



#define TARGET_MODE_REPLY_IO_INDEX_MASK         (0x00003FFF)
#define TARGET_MODE_REPLY_IO_INDEX_SHIFT        (0)
#define TARGET_MODE_REPLY_INITIATOR_INDEX_MASK  (0x03FFC000)
#define TARGET_MODE_REPLY_INITIATOR_INDEX_SHIFT (14)
#define TARGET_MODE_REPLY_ALIAS_MASK            (0x04000000)
#define TARGET_MODE_REPLY_ALIAS_SHIFT           (26)
#define TARGET_MODE_REPLY_PORT_MASK             (0x10000000)
#define TARGET_MODE_REPLY_PORT_SHIFT            (28)


#define GET_IO_INDEX(x)     (((x) & TARGET_MODE_REPLY_IO_INDEX_MASK)           \
                                    >> TARGET_MODE_REPLY_IO_INDEX_SHIFT)

#define SET_IO_INDEX(t, i)                                                     \
            ((t) = ((t) & ~TARGET_MODE_REPLY_IO_INDEX_MASK) |                  \
                              (((i) << TARGET_MODE_REPLY_IO_INDEX_SHIFT) &     \
                                             TARGET_MODE_REPLY_IO_INDEX_MASK))

#define GET_INITIATOR_INDEX(x) (((x) & TARGET_MODE_REPLY_INITIATOR_INDEX_MASK) \
                                   >> TARGET_MODE_REPLY_INITIATOR_INDEX_SHIFT)

#define SET_INITIATOR_INDEX(t, ii)                                             \
        ((t) = ((t) & ~TARGET_MODE_REPLY_INITIATOR_INDEX_MASK) |               \
                        (((ii) << TARGET_MODE_REPLY_INITIATOR_INDEX_SHIFT) &   \
                                      TARGET_MODE_REPLY_INITIATOR_INDEX_MASK))

#define GET_ALIAS(x) (((x) & TARGET_MODE_REPLY_ALIAS_MASK)                     \
                                               >> TARGET_MODE_REPLY_ALIAS_SHIFT)

#define SET_ALIAS(t, a)  ((t) = ((t) & ~TARGET_MODE_REPLY_ALIAS_MASK) |        \
                                    (((a) << TARGET_MODE_REPLY_ALIAS_SHIFT) &  \
                                                 TARGET_MODE_REPLY_ALIAS_MASK))

#define GET_PORT(x) (((x) & TARGET_MODE_REPLY_PORT_MASK)                       \
                                               >> TARGET_MODE_REPLY_PORT_SHIFT)

#define SET_PORT(t, p)  ((t) = ((t) & ~TARGET_MODE_REPLY_PORT_MASK) |          \
                                    (((p) << TARGET_MODE_REPLY_PORT_SHIFT) &   \
                                                  TARGET_MODE_REPLY_PORT_MASK))

#define TARGET_MODE_REPLY_0100_MASK_HOST_INDEX       (0x000003FF)
#define TARGET_MODE_REPLY_0100_SHIFT_HOST_INDEX      (0)
#define TARGET_MODE_REPLY_0100_MASK_IOC_INDEX        (0x001FF800)
#define TARGET_MODE_REPLY_0100_SHIFT_IOC_INDEX       (11)
#define TARGET_MODE_REPLY_0100_PORT_MASK             (0x00400000)
#define TARGET_MODE_REPLY_0100_PORT_SHIFT            (22)
#define TARGET_MODE_REPLY_0100_MASK_INITIATOR_INDEX  (0x1F800000)
#define TARGET_MODE_REPLY_0100_SHIFT_INITIATOR_INDEX (23)

#define GET_HOST_INDEX_0100(x) (((x) & TARGET_MODE_REPLY_0100_MASK_HOST_INDEX) \
                                  >> TARGET_MODE_REPLY_0100_SHIFT_HOST_INDEX)

#define SET_HOST_INDEX_0100(t, hi)                                             \
            ((t) = ((t) & ~TARGET_MODE_REPLY_0100_MASK_HOST_INDEX) |           \
                         (((hi) << TARGET_MODE_REPLY_0100_SHIFT_HOST_INDEX) &  \
                                      TARGET_MODE_REPLY_0100_MASK_HOST_INDEX))

#define GET_IOC_INDEX_0100(x)   (((x) & TARGET_MODE_REPLY_0100_MASK_IOC_INDEX) \
                                  >> TARGET_MODE_REPLY_0100_SHIFT_IOC_INDEX)

#define SET_IOC_INDEX_0100(t, ii)                                              \
            ((t) = ((t) & ~TARGET_MODE_REPLY_0100_MASK_IOC_INDEX) |            \
                        (((ii) << TARGET_MODE_REPLY_0100_SHIFT_IOC_INDEX) &    \
                                     TARGET_MODE_REPLY_0100_MASK_IOC_INDEX))

#define GET_INITIATOR_INDEX_0100(x)                                            \
            (((x) & TARGET_MODE_REPLY_0100_MASK_INITIATOR_INDEX)               \
                              >> TARGET_MODE_REPLY_0100_SHIFT_INITIATOR_INDEX)

#define SET_INITIATOR_INDEX_0100(t, ii)                                        \
        ((t) = ((t) & ~TARGET_MODE_REPLY_0100_MASK_INITIATOR_INDEX) |          \
                   (((ii) << TARGET_MODE_REPLY_0100_SHIFT_INITIATOR_INDEX) &   \
                                TARGET_MODE_REPLY_0100_MASK_INITIATOR_INDEX))


#endif

