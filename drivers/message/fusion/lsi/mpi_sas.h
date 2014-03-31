/*
 *  Copyright (c) 2004-2008 LSI Corporation.
 *
 *
 *           Name:  mpi_sas.h
 *          Title:  MPI Serial Attached SCSI structures and definitions
 *  Creation Date:  August 19, 2004
 *
 *    mpi_sas.h Version:  01.05.05
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  08-19-04  01.05.01  Original release.
 *  08-30-05  01.05.02  Added DeviceInfo bit for SEP.
 *                      Added PrimFlags and Primitive field to SAS IO Unit
 *                      Control request, and added a new operation code.
 *  03-27-06  01.05.03  Added Force Full Discovery, Transmit Port Select Signal,
 *                      and Remove Device operations to SAS IO Unit Control.
 *                      Added DevHandle field to SAS IO Unit Control request and
 *                      reply.
 *  10-11-06  01.05.04  Fixed the name of a define for Operation field of SAS IO
 *                      Unit Control request.
 *  01-15-08  01.05.05  Added support for MPI_SAS_OP_SET_IOC_PARAMETER,
 *                      including adding IOCParameter and IOCParameter value
 *                      fields to SAS IO Unit Control Request.
 *                      Added MPI_SAS_DEVICE_INFO_PRODUCT_SPECIFIC define.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI_SAS_H
#define MPI_SAS_H


#define MPI_SASSTATUS_SUCCESS                           (0x00)
#define MPI_SASSTATUS_UNKNOWN_ERROR                     (0x01)
#define MPI_SASSTATUS_INVALID_FRAME                     (0x02)
#define MPI_SASSTATUS_UTC_BAD_DEST                      (0x03)
#define MPI_SASSTATUS_UTC_BREAK_RECEIVED                (0x04)
#define MPI_SASSTATUS_UTC_CONNECT_RATE_NOT_SUPPORTED    (0x05)
#define MPI_SASSTATUS_UTC_PORT_LAYER_REQUEST            (0x06)
#define MPI_SASSTATUS_UTC_PROTOCOL_NOT_SUPPORTED        (0x07)
#define MPI_SASSTATUS_UTC_STP_RESOURCES_BUSY            (0x08)
#define MPI_SASSTATUS_UTC_WRONG_DESTINATION             (0x09)
#define MPI_SASSTATUS_SHORT_INFORMATION_UNIT            (0x0A)
#define MPI_SASSTATUS_LONG_INFORMATION_UNIT             (0x0B)
#define MPI_SASSTATUS_XFER_RDY_INCORRECT_WRITE_DATA     (0x0C)
#define MPI_SASSTATUS_XFER_RDY_REQUEST_OFFSET_ERROR     (0x0D)
#define MPI_SASSTATUS_XFER_RDY_NOT_EXPECTED             (0x0E)
#define MPI_SASSTATUS_DATA_INCORRECT_DATA_LENGTH        (0x0F)
#define MPI_SASSTATUS_DATA_TOO_MUCH_READ_DATA           (0x10)
#define MPI_SASSTATUS_DATA_OFFSET_ERROR                 (0x11)
#define MPI_SASSTATUS_SDSF_NAK_RECEIVED                 (0x12)
#define MPI_SASSTATUS_SDSF_CONNECTION_FAILED            (0x13)
#define MPI_SASSTATUS_INITIATOR_RESPONSE_TIMEOUT        (0x14)


#define MPI_SAS_DEVICE_INFO_PRODUCT_SPECIFIC    (0xF0000000)

#define MPI_SAS_DEVICE_INFO_SEP                 (0x00004000)
#define MPI_SAS_DEVICE_INFO_ATAPI_DEVICE        (0x00002000)
#define MPI_SAS_DEVICE_INFO_LSI_DEVICE          (0x00001000)
#define MPI_SAS_DEVICE_INFO_DIRECT_ATTACH       (0x00000800)
#define MPI_SAS_DEVICE_INFO_SSP_TARGET          (0x00000400)
#define MPI_SAS_DEVICE_INFO_STP_TARGET          (0x00000200)
#define MPI_SAS_DEVICE_INFO_SMP_TARGET          (0x00000100)
#define MPI_SAS_DEVICE_INFO_SATA_DEVICE         (0x00000080)
#define MPI_SAS_DEVICE_INFO_SSP_INITIATOR       (0x00000040)
#define MPI_SAS_DEVICE_INFO_STP_INITIATOR       (0x00000020)
#define MPI_SAS_DEVICE_INFO_SMP_INITIATOR       (0x00000010)
#define MPI_SAS_DEVICE_INFO_SATA_HOST           (0x00000008)

#define MPI_SAS_DEVICE_INFO_MASK_DEVICE_TYPE    (0x00000007)
#define MPI_SAS_DEVICE_INFO_NO_DEVICE           (0x00000000)
#define MPI_SAS_DEVICE_INFO_END_DEVICE          (0x00000001)
#define MPI_SAS_DEVICE_INFO_EDGE_EXPANDER       (0x00000002)
#define MPI_SAS_DEVICE_INFO_FANOUT_EXPANDER     (0x00000003)





typedef struct _MSG_SMP_PASSTHROUGH_REQUEST
{
    U8                      PassthroughFlags;   
    U8                      PhysicalPort;       
    U8                      ChainOffset;        
    U8                      Function;           
    U16                     RequestDataLength;  
    U8                      ConnectionRate;     
    U8                      MsgFlags;           
    U32                     MsgContext;         
    U32                     Reserved1;          
    U64                     SASAddress;         
    U32                     Reserved2;          
    U32                     Reserved3;          
    SGE_SIMPLE_UNION        SGL;                
} MSG_SMP_PASSTHROUGH_REQUEST, MPI_POINTER PTR_MSG_SMP_PASSTHROUGH_REQUEST,
  SmpPassthroughRequest_t, MPI_POINTER pSmpPassthroughRequest_t;

#define MPI_SMP_PT_REQ_PT_FLAGS_IMMEDIATE       (0x80)

#define MPI_SMP_PT_REQ_CONNECT_RATE_NEGOTIATED  (0x00)
#define MPI_SMP_PT_REQ_CONNECT_RATE_1_5         (0x08)
#define MPI_SMP_PT_REQ_CONNECT_RATE_3_0         (0x09)


typedef struct _MSG_SMP_PASSTHROUGH_REPLY
{
    U8                      PassthroughFlags;   
    U8                      PhysicalPort;       
    U8                      MsgLength;          
    U8                      Function;           
    U16                     ResponseDataLength; 
    U8                      Reserved1;          
    U8                      MsgFlags;           
    U32                     MsgContext;         
    U8                      Reserved2;          
    U8                      SASStatus;          
    U16                     IOCStatus;          
    U32                     IOCLogInfo;         
    U32                     Reserved3;          
    U8                      ResponseData[4];    
} MSG_SMP_PASSTHROUGH_REPLY, MPI_POINTER PTR_MSG_SMP_PASSTHROUGH_REPLY,
  SmpPassthroughReply_t, MPI_POINTER pSmpPassthroughReply_t;

#define MPI_SMP_PT_REPLY_PT_FLAGS_IMMEDIATE     (0x80)



typedef struct _MSG_SATA_PASSTHROUGH_REQUEST
{
    U8                      TargetID;           
    U8                      Bus;                
    U8                      ChainOffset;        
    U8                      Function;           
    U16                     PassthroughFlags;   
    U8                      ConnectionRate;     
    U8                      MsgFlags;           
    U32                     MsgContext;         
    U32                     Reserved1;          
    U32                     Reserved2;          
    U32                     Reserved3;          
    U32                     DataLength;         
    U8                      CommandFIS[20];     
    SGE_SIMPLE_UNION        SGL;                
} MSG_SATA_PASSTHROUGH_REQUEST, MPI_POINTER PTR_MSG_SATA_PASSTHROUGH_REQUEST,
  SataPassthroughRequest_t, MPI_POINTER pSataPassthroughRequest_t;

#define MPI_SATA_PT_REQ_PT_FLAGS_RESET_DEVICE   (0x0200)
#define MPI_SATA_PT_REQ_PT_FLAGS_EXECUTE_DIAG   (0x0100)
#define MPI_SATA_PT_REQ_PT_FLAGS_DMA_QUEUED     (0x0080)
#define MPI_SATA_PT_REQ_PT_FLAGS_PACKET_COMMAND (0x0040)
#define MPI_SATA_PT_REQ_PT_FLAGS_DMA            (0x0020)
#define MPI_SATA_PT_REQ_PT_FLAGS_PIO            (0x0010)
#define MPI_SATA_PT_REQ_PT_FLAGS_UNSPECIFIED_VU (0x0004)
#define MPI_SATA_PT_REQ_PT_FLAGS_WRITE          (0x0002)
#define MPI_SATA_PT_REQ_PT_FLAGS_READ           (0x0001)

#define MPI_SATA_PT_REQ_CONNECT_RATE_NEGOTIATED (0x00)
#define MPI_SATA_PT_REQ_CONNECT_RATE_1_5        (0x08)
#define MPI_SATA_PT_REQ_CONNECT_RATE_3_0        (0x09)


typedef struct _MSG_SATA_PASSTHROUGH_REPLY
{
    U8                      TargetID;           
    U8                      Bus;                
    U8                      MsgLength;          
    U8                      Function;           
    U16                     PassthroughFlags;   
    U8                      Reserved1;          
    U8                      MsgFlags;           
    U32                     MsgContext;         
    U8                      Reserved2;          
    U8                      SASStatus;          
    U16                     IOCStatus;          
    U32                     IOCLogInfo;         
    U8                      StatusFIS[20];      
    U32                     StatusControlRegisters; 
    U32                     TransferCount;      
} MSG_SATA_PASSTHROUGH_REPLY, MPI_POINTER PTR_MSG_SATA_PASSTHROUGH_REPLY,
  SataPassthroughReply_t, MPI_POINTER pSataPassthroughReply_t;





typedef struct _MSG_SAS_IOUNIT_CONTROL_REQUEST
{
    U8                      Operation;          
    U8                      Reserved1;          
    U8                      ChainOffset;        
    U8                      Function;           
    U16                     DevHandle;          
    U8                      IOCParameter;       
    U8                      MsgFlags;           
    U32                     MsgContext;         
    U8                      TargetID;           
    U8                      Bus;                
    U8                      PhyNum;             
    U8                      PrimFlags;          
    U32                     Primitive;          
    U64                     SASAddress;         
    U32                     IOCParameterValue;  
} MSG_SAS_IOUNIT_CONTROL_REQUEST, MPI_POINTER PTR_MSG_SAS_IOUNIT_CONTROL_REQUEST,
  SasIoUnitControlRequest_t, MPI_POINTER pSasIoUnitControlRequest_t;

#define MPI_SAS_OP_CLEAR_NOT_PRESENT            (0x01)
#define MPI_SAS_OP_CLEAR_ALL_PERSISTENT         (0x02)
#define MPI_SAS_OP_PHY_LINK_RESET               (0x06)
#define MPI_SAS_OP_PHY_HARD_RESET               (0x07)
#define MPI_SAS_OP_PHY_CLEAR_ERROR_LOG          (0x08)
#define MPI_SAS_OP_MAP_CURRENT                  (0x09)
#define MPI_SAS_OP_SEND_PRIMITIVE               (0x0A)
#define MPI_SAS_OP_FORCE_FULL_DISCOVERY         (0x0B)
#define MPI_SAS_OP_TRANSMIT_PORT_SELECT_SIGNAL  (0x0C)
#define MPI_SAS_OP_TRANSMIT_REMOVE_DEVICE       (0x0D)  
#define MPI_SAS_OP_REMOVE_DEVICE                (0x0D)
#define MPI_SAS_OP_SET_IOC_PARAMETER            (0x0E)
#define MPI_SAS_OP_PRODUCT_SPECIFIC_MIN         (0x80)

#define MPI_SAS_PRIMFLAGS_SINGLE                (0x08)
#define MPI_SAS_PRIMFLAGS_TRIPLE                (0x02)
#define MPI_SAS_PRIMFLAGS_REDUNDANT             (0x01)


typedef struct _MSG_SAS_IOUNIT_CONTROL_REPLY
{
    U8                      Operation;          
    U8                      Reserved1;          
    U8                      MsgLength;          
    U8                      Function;           
    U16                     DevHandle;          
    U8                      IOCParameter;       
    U8                      MsgFlags;           
    U32                     MsgContext;         
    U16                     Reserved4;          
    U16                     IOCStatus;          
    U32                     IOCLogInfo;         
} MSG_SAS_IOUNIT_CONTROL_REPLY, MPI_POINTER PTR_MSG_SAS_IOUNIT_CONTROL_REPLY,
  SasIoUnitControlReply_t, MPI_POINTER pSasIoUnitControlReply_t;

#endif


