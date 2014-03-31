/*
*******************************************************************************
**        O.S   : Linux
**   FILE NAME  : arcmsr.h
**        BY    : Nick Cheng
**   Description: SCSI RAID Device Driver for
**                ARECA RAID Host adapter
*******************************************************************************
** Copyright (C) 2002 - 2005, Areca Technology Corporation All rights reserved.
**
**     Web site: www.areca.com.tw
**       E-mail: support@areca.com.tw
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License version 2 as
** published by the Free Software Foundation.
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*******************************************************************************
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION)HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
**(INCLUDING NEGLIGENCE OR OTHERWISE)ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************
*/
#include <linux/interrupt.h>
struct device_attribute;
#define ARCMSR_MAX_OUTSTANDING_CMD						256
#ifdef CONFIG_XEN
	#define ARCMSR_MAX_FREECCB_NUM	160
#else
	#define ARCMSR_MAX_FREECCB_NUM	320
#endif
#define ARCMSR_DRIVER_VERSION		     "Driver Version 1.20.00.15 2010/08/05"
#define ARCMSR_SCSI_INITIATOR_ID						255
#define ARCMSR_MAX_XFER_SECTORS							512
#define ARCMSR_MAX_XFER_SECTORS_B						4096
#define ARCMSR_MAX_XFER_SECTORS_C						304
#define ARCMSR_MAX_TARGETID							17
#define ARCMSR_MAX_TARGETLUN							8
#define ARCMSR_MAX_CMD_PERLUN		                 ARCMSR_MAX_OUTSTANDING_CMD
#define ARCMSR_MAX_QBUFFER							4096
#define ARCMSR_DEFAULT_SG_ENTRIES						38
#define ARCMSR_MAX_HBB_POSTQUEUE						264
#define ARCMSR_MAX_XFER_LEN							0x26000 
#define ARCMSR_CDB_SG_PAGE_LENGTH						256 
#ifndef PCI_DEVICE_ID_ARECA_1880
#define PCI_DEVICE_ID_ARECA_1880 0x1880
 #endif
#define ARC_SUCCESS                                                       0
#define ARC_FAILURE                                                       1
#define dma_addr_hi32(addr)               (uint32_t) ((addr>>16)>>16)
#define dma_addr_lo32(addr)               (uint32_t) (addr & 0xffffffff)
struct CMD_MESSAGE
{
      uint32_t HeaderLength;
      uint8_t  Signature[8];
      uint32_t Timeout;
      uint32_t ControlCode;
      uint32_t ReturnCode;
      uint32_t Length;
};
struct CMD_MESSAGE_FIELD
{
    struct CMD_MESSAGE			cmdmessage;
    uint8_t				messagedatabuffer[1032];
};
#define ARCMSR_MESSAGE_FAIL			0x0001
#define ARECA_SATA_RAID				0x90000000
#define FUNCTION_READ_RQBUFFER			0x0801
#define FUNCTION_WRITE_WQBUFFER			0x0802
#define FUNCTION_CLEAR_RQBUFFER			0x0803
#define FUNCTION_CLEAR_WQBUFFER			0x0804
#define FUNCTION_CLEAR_ALLQBUFFER		0x0805
#define FUNCTION_RETURN_CODE_3F			0x0806
#define FUNCTION_SAY_HELLO			0x0807
#define FUNCTION_SAY_GOODBYE			0x0808
#define FUNCTION_FLUSH_ADAPTER_CACHE		0x0809
#define FUNCTION_GET_FIRMWARE_STATUS			0x080A
#define FUNCTION_HARDWARE_RESET			0x080B
#define ARCMSR_MESSAGE_READ_RQBUFFER       \
	ARECA_SATA_RAID | FUNCTION_READ_RQBUFFER
#define ARCMSR_MESSAGE_WRITE_WQBUFFER      \
	ARECA_SATA_RAID | FUNCTION_WRITE_WQBUFFER
#define ARCMSR_MESSAGE_CLEAR_RQBUFFER      \
	ARECA_SATA_RAID | FUNCTION_CLEAR_RQBUFFER
#define ARCMSR_MESSAGE_CLEAR_WQBUFFER      \
	ARECA_SATA_RAID | FUNCTION_CLEAR_WQBUFFER
#define ARCMSR_MESSAGE_CLEAR_ALLQBUFFER    \
	ARECA_SATA_RAID | FUNCTION_CLEAR_ALLQBUFFER
#define ARCMSR_MESSAGE_RETURN_CODE_3F      \
	ARECA_SATA_RAID | FUNCTION_RETURN_CODE_3F
#define ARCMSR_MESSAGE_SAY_HELLO           \
	ARECA_SATA_RAID | FUNCTION_SAY_HELLO
#define ARCMSR_MESSAGE_SAY_GOODBYE         \
	ARECA_SATA_RAID | FUNCTION_SAY_GOODBYE
#define ARCMSR_MESSAGE_FLUSH_ADAPTER_CACHE \
	ARECA_SATA_RAID | FUNCTION_FLUSH_ADAPTER_CACHE
#define ARCMSR_MESSAGE_RETURNCODE_OK		0x00000001
#define ARCMSR_MESSAGE_RETURNCODE_ERROR		0x00000006
#define ARCMSR_MESSAGE_RETURNCODE_3F		0x0000003F
#define ARCMSR_MESSAGE_RETURNCODE_BUS_HANG_ON	0x00000088
#define IS_DMA64			(sizeof(dma_addr_t) == 8)
#define IS_SG64_ADDR                0x01000000 
struct  SG32ENTRY
{
	__le32					length;
	__le32					address;
}__attribute__ ((packed));
struct  SG64ENTRY
{
	__le32					length;
	__le32					address;
	__le32					addresshigh;
}__attribute__ ((packed));
struct QBUFFER
{
	uint32_t      data_len;
	uint8_t       data[124];
};
struct FIRMWARE_INFO
{
	uint32_t      signature;		
	uint32_t      request_len;		
	uint32_t      numbers_queue;		
	uint32_t      sdram_size;               
	uint32_t      ide_channels;		
	char          vendor[40];		
	char          model[8];			
	char          firmware_ver[16];     	
	char          device_map[16];		
	uint32_t		cfgVersion;               	
	uint8_t		cfgSerial[16];           	
	uint32_t		cfgPicStatus;            		
};
#define ARCMSR_SIGNATURE_GET_CONFIG		      0x87974060
#define ARCMSR_SIGNATURE_SET_CONFIG		      0x87974063
#define ARCMSR_INBOUND_MESG0_NOP		      0x00000000
#define ARCMSR_INBOUND_MESG0_GET_CONFIG		      0x00000001
#define ARCMSR_INBOUND_MESG0_SET_CONFIG               0x00000002
#define ARCMSR_INBOUND_MESG0_ABORT_CMD                0x00000003
#define ARCMSR_INBOUND_MESG0_STOP_BGRB                0x00000004
#define ARCMSR_INBOUND_MESG0_FLUSH_CACHE              0x00000005
#define ARCMSR_INBOUND_MESG0_START_BGRB               0x00000006
#define ARCMSR_INBOUND_MESG0_CHK331PENDING            0x00000007
#define ARCMSR_INBOUND_MESG0_SYNC_TIMER               0x00000008
#define ARCMSR_INBOUND_DRIVER_DATA_WRITE_OK           0x00000001
#define ARCMSR_INBOUND_DRIVER_DATA_READ_OK            0x00000002
#define ARCMSR_OUTBOUND_IOP331_DATA_WRITE_OK          0x00000001
#define ARCMSR_OUTBOUND_IOP331_DATA_READ_OK           0x00000002
#define ARCMSR_CCBPOST_FLAG_SGL_BSIZE                 0x80000000
#define ARCMSR_CCBPOST_FLAG_IAM_BIOS                  0x40000000
#define ARCMSR_CCBREPLY_FLAG_IAM_BIOS                 0x40000000
#define ARCMSR_CCBREPLY_FLAG_ERROR_MODE0              0x10000000
#define ARCMSR_CCBREPLY_FLAG_ERROR_MODE1              0x00000001
#define ARCMSR_OUTBOUND_MESG1_FIRMWARE_OK             0x80000000
#define ARCMSR_ARC1680_BUS_RESET				0x00000003
#define ARCMSR_ARC1880_RESET_ADAPTER				0x00000024
#define ARCMSR_ARC1880_DiagWrite_ENABLE			0x00000080

#define ARCMSR_DRV2IOP_DOORBELL                       0x00020400
#define ARCMSR_DRV2IOP_DOORBELL_MASK                  0x00020404
#define ARCMSR_IOP2DRV_DOORBELL                       0x00020408
#define ARCMSR_IOP2DRV_DOORBELL_MASK                  0x0002040C
#define ARCMSR_IOP2DRV_DATA_WRITE_OK                  0x00000001
#define ARCMSR_IOP2DRV_DATA_READ_OK                   0x00000002
#define ARCMSR_IOP2DRV_CDB_DONE                       0x00000004
#define ARCMSR_IOP2DRV_MESSAGE_CMD_DONE               0x00000008

#define ARCMSR_DOORBELL_HANDLE_INT		      0x0000000F
#define ARCMSR_DOORBELL_INT_CLEAR_PATTERN   	      0xFF00FFF0
#define ARCMSR_MESSAGE_INT_CLEAR_PATTERN	      0xFF00FFF7
#define ARCMSR_MESSAGE_GET_CONFIG		      0x00010008
#define ARCMSR_MESSAGE_SET_CONFIG		      0x00020008
#define ARCMSR_MESSAGE_ABORT_CMD		      0x00030008
#define ARCMSR_MESSAGE_STOP_BGRB		      0x00040008
#define ARCMSR_MESSAGE_FLUSH_CACHE                    0x00050008
#define ARCMSR_MESSAGE_START_BGRB		      0x00060008
#define ARCMSR_MESSAGE_START_DRIVER_MODE	      0x000E0008
#define ARCMSR_MESSAGE_SET_POST_WINDOW		      0x000F0008
#define ARCMSR_MESSAGE_ACTIVE_EOI_MODE		    0x00100008
#define ARCMSR_MESSAGE_FIRMWARE_OK		      0x80000000
#define ARCMSR_DRV2IOP_DATA_WRITE_OK                  0x00000001
#define ARCMSR_DRV2IOP_DATA_READ_OK                   0x00000002
#define ARCMSR_DRV2IOP_CDB_POSTED                     0x00000004
#define ARCMSR_DRV2IOP_MESSAGE_CMD_POSTED             0x00000008
#define ARCMSR_DRV2IOP_END_OF_INTERRUPT		0x00000010

#define ARCMSR_MESSAGE_WBUFFER			      0x0000fe00
#define ARCMSR_MESSAGE_RBUFFER			      0x0000ff00
#define ARCMSR_MESSAGE_RWBUFFER			      0x0000fa00
#define ARCMSR_HBC_ISR_THROTTLING_LEVEL		12
#define ARCMSR_HBC_ISR_MAX_DONE_QUEUE		20
#define ARCMSR_HBCMU_UTILITY_A_ISR_MASK		0x00000001 
#define ARCMSR_HBCMU_OUTBOUND_DOORBELL_ISR_MASK	0x00000004 
#define ARCMSR_HBCMU_OUTBOUND_POSTQUEUE_ISR_MASK	0x00000008 
#define ARCMSR_HBCMU_ALL_INTMASKENABLE		0x0000000D 
#define ARCMSR_HBCMU_UTILITY_A_ISR			0x00000001
#define ARCMSR_HBCMU_OUTBOUND_DOORBELL_ISR		0x00000004
#define ARCMSR_HBCMU_OUTBOUND_POSTQUEUE_ISR	0x00000008
#define ARCMSR_HBCMU_SAS_ALL_INT			0x00000010
	
#define ARCMSR_HBCMU_DRV2IOP_DATA_WRITE_OK			0x00000002
#define ARCMSR_HBCMU_DRV2IOP_DATA_READ_OK			0x00000004
	
#define ARCMSR_HBCMU_DRV2IOP_MESSAGE_CMD_DONE		0x00000008
	
#define ARCMSR_HBCMU_DRV2IOP_POSTQUEUE_THROTTLING		0x00000010
#define ARCMSR_HBCMU_IOP2DRV_DATA_WRITE_OK			0x00000002
	
#define ARCMSR_HBCMU_IOP2DRV_DATA_WRITE_DOORBELL_CLEAR	0x00000002
#define ARCMSR_HBCMU_IOP2DRV_DATA_READ_OK			0x00000004
	
#define ARCMSR_HBCMU_IOP2DRV_DATA_READ_DOORBELL_CLEAR	0x00000004
	
#define ARCMSR_HBCMU_IOP2DRV_MESSAGE_CMD_DONE		0x00000008
	
#define ARCMSR_HBCMU_IOP2DRV_MESSAGE_CMD_DONE_DOORBELL_CLEAR	0x00000008
	
#define ARCMSR_HBCMU_MESSAGE_FIRMWARE_OK			0x80000000
struct ARCMSR_CDB
{
	uint8_t							Bus;
	uint8_t							TargetID;
	uint8_t							LUN;
	uint8_t							Function;
	uint8_t							CdbLength;
	uint8_t							sgcount;
	uint8_t							Flags;
#define ARCMSR_CDB_FLAG_SGL_BSIZE          0x01
#define ARCMSR_CDB_FLAG_BIOS               0x02
#define ARCMSR_CDB_FLAG_WRITE              0x04
#define ARCMSR_CDB_FLAG_SIMPLEQ            0x00
#define ARCMSR_CDB_FLAG_HEADQ              0x08
#define ARCMSR_CDB_FLAG_ORDEREDQ           0x10

	uint8_t							msgPages;
	uint32_t						Context;
	uint32_t						DataLength;
	uint8_t							Cdb[16];
	uint8_t							DeviceStatus;
#define ARCMSR_DEV_CHECK_CONDITION	    0x02
#define ARCMSR_DEV_SELECT_TIMEOUT	    0xF0
#define ARCMSR_DEV_ABORTED		    0xF1
#define ARCMSR_DEV_INIT_FAIL		    0xF2

	uint8_t							SenseData[15];
	union
	{
		struct SG32ENTRY                sg32entry[1];
		struct SG64ENTRY                sg64entry[1];
	} u;
};
struct MessageUnit_A
{
	uint32_t	resrved0[4];			
	uint32_t	inbound_msgaddr0;		
	uint32_t	inbound_msgaddr1;		
	uint32_t	outbound_msgaddr0;		
	uint32_t	outbound_msgaddr1;		
	uint32_t	inbound_doorbell;		
	uint32_t	inbound_intstatus;		
	uint32_t	inbound_intmask;		
	uint32_t	outbound_doorbell;		
	uint32_t	outbound_intstatus;		
	uint32_t	outbound_intmask;		
	uint32_t	reserved1[2];			
	uint32_t	inbound_queueport;		
	uint32_t	outbound_queueport;     	
	uint32_t	reserved2[2];			
	uint32_t	reserved3[492];			
	uint32_t	reserved4[128];			
	uint32_t	message_rwbuffer[256];		
	uint32_t	message_wbuffer[32];		
	uint32_t	reserved5[32];			
	uint32_t	message_rbuffer[32];		
	uint32_t	reserved6[32];			
};

struct MessageUnit_B
{
	uint32_t	post_qbuffer[ARCMSR_MAX_HBB_POSTQUEUE];
	uint32_t	done_qbuffer[ARCMSR_MAX_HBB_POSTQUEUE];
	uint32_t	postq_index;
	uint32_t	doneq_index;
	uint32_t		__iomem *drv2iop_doorbell;
	uint32_t		__iomem *drv2iop_doorbell_mask;
	uint32_t		__iomem *iop2drv_doorbell;
	uint32_t		__iomem *iop2drv_doorbell_mask;
	uint32_t		__iomem *message_rwbuffer;
	uint32_t		__iomem *message_wbuffer;
	uint32_t		__iomem *message_rbuffer;
};
struct MessageUnit_C{
	uint32_t	message_unit_status;			
	uint32_t	slave_error_attribute;			
	uint32_t	slave_error_address;			
	uint32_t	posted_outbound_doorbell;		
	uint32_t	master_error_attribute;			
	uint32_t	master_error_address_low;		
	uint32_t	master_error_address_high;		
	uint32_t	hcb_size;				
	uint32_t	inbound_doorbell;			
	uint32_t	diagnostic_rw_data;			
	uint32_t	diagnostic_rw_address_low;		
	uint32_t	diagnostic_rw_address_high;		
	uint32_t	host_int_status;				
	uint32_t	host_int_mask;				
	uint32_t	dcr_data;				
	uint32_t	dcr_address;				
	uint32_t	inbound_queueport;			
	uint32_t	outbound_queueport;			
	uint32_t	hcb_pci_address_low;			
	uint32_t	hcb_pci_address_high;			
	uint32_t	iop_int_status;				
	uint32_t	iop_int_mask;				
	uint32_t	iop_inbound_queue_port;			
	uint32_t	iop_outbound_queue_port;		
	uint32_t	inbound_free_list_index;			
	uint32_t	inbound_post_list_index;			
	uint32_t	outbound_free_list_index;			
	uint32_t	outbound_post_list_index;			
	uint32_t	inbound_doorbell_clear;			
	uint32_t	i2o_message_unit_control;			
	uint32_t	last_used_message_source_address_low;	
	uint32_t	last_used_message_source_address_high;	
	uint32_t	pull_mode_data_byte_count[4];		
	uint32_t	message_dest_address_index;		
	uint32_t	done_queue_not_empty_int_counter_timer;	
	uint32_t	utility_A_int_counter_timer;		
	uint32_t	outbound_doorbell;			
	uint32_t	outbound_doorbell_clear;			
	uint32_t	message_source_address_index;		
	uint32_t	message_done_queue_index;		
	uint32_t	reserved0;				
	uint32_t	inbound_msgaddr0;			
	uint32_t	inbound_msgaddr1;			
	uint32_t	outbound_msgaddr0;			
	uint32_t	outbound_msgaddr1;			
	uint32_t	inbound_queueport_low;			
	uint32_t	inbound_queueport_high;			
	uint32_t	outbound_queueport_low;			
	uint32_t	outbound_queueport_high;		
	uint32_t	iop_inbound_queue_port_low;		
	uint32_t	iop_inbound_queue_port_high;		
	uint32_t	iop_outbound_queue_port_low;		
	uint32_t	iop_outbound_queue_port_high;		
	uint32_t	message_dest_queue_port_low;		
	uint32_t	message_dest_queue_port_high;		
	uint32_t	last_used_message_dest_address_low;	
	uint32_t	last_used_message_dest_address_high;	
	uint32_t	message_done_queue_base_address_low;	
	uint32_t	message_done_queue_base_address_high;	
	uint32_t	host_diagnostic;				
	uint32_t	write_sequence;				
	uint32_t	reserved1[34];				
	uint32_t	reserved2[1950];				
	uint32_t	message_wbuffer[32];			
	uint32_t	reserved3[32];				
	uint32_t	message_rbuffer[32];			
	uint32_t	reserved4[32];				
	uint32_t	msgcode_rwbuffer[256];			
};
struct AdapterControlBlock
{
	uint32_t  adapter_type;                
	#define ACB_ADAPTER_TYPE_A            0x00000001	
	#define ACB_ADAPTER_TYPE_B            0x00000002	
	#define ACB_ADAPTER_TYPE_C            0x00000004	
	#define ACB_ADAPTER_TYPE_D            0x00000008	
	struct pci_dev *		pdev;
	struct Scsi_Host *		host;
	unsigned long			vir2phy_offset;
	
	uint32_t			outbound_int_enable;
	uint32_t			cdb_phyaddr_hi32;
	uint32_t			reg_mu_acc_handle0;
	spinlock_t                      			eh_lock;
	spinlock_t                      			ccblist_lock;
	union {
		struct MessageUnit_A __iomem *pmuA;
		struct MessageUnit_B 	*pmuB;
		struct MessageUnit_C __iomem *pmuC;
	};
	
	void __iomem *mem_base0;
	void __iomem *mem_base1;
	uint32_t			acb_flags;
	u16			dev_id;
	uint8_t                   		adapter_index;
	#define ACB_F_SCSISTOPADAPTER         	0x0001
	#define ACB_F_MSG_STOP_BGRB     	0x0002
	
	#define ACB_F_MSG_START_BGRB          	0x0004
	
	#define ACB_F_IOPDATA_OVERFLOW        	0x0008
	
	#define ACB_F_MESSAGE_WQBUFFER_CLEARED	0x0010
	
	#define ACB_F_MESSAGE_RQBUFFER_CLEARED  0x0020
	
	#define ACB_F_MESSAGE_WQBUFFER_READED   0x0040
	#define ACB_F_BUS_RESET               	0x0080
	#define ACB_F_BUS_HANG_ON		0x0800

	#define ACB_F_IOP_INITED              	0x0100
	
	#define ACB_F_ABORT				0x0200
	#define ACB_F_FIRMWARE_TRAP           		0x0400
	struct CommandControlBlock *			pccb_pool[ARCMSR_MAX_FREECCB_NUM];
	
	struct list_head		ccb_free_list;
	

	atomic_t			ccboutstandingcount;

	void *				dma_coherent;
	
	dma_addr_t			dma_coherent_handle;
	
	dma_addr_t				dma_coherent_handle_hbb_mu;
	unsigned int				uncache_size;
	uint8_t				rqbuffer[ARCMSR_MAX_QBUFFER];
	
	int32_t				rqbuf_firstindex;
	
	int32_t				rqbuf_lastindex;
	
	uint8_t				wqbuffer[ARCMSR_MAX_QBUFFER];
	
	int32_t				wqbuf_firstindex;
	
	int32_t				wqbuf_lastindex;
	
	uint8_t				devstate[ARCMSR_MAX_TARGETID][ARCMSR_MAX_TARGETLUN];
	
#define ARECA_RAID_GONE               0x55
#define ARECA_RAID_GOOD               0xaa
	uint32_t			num_resets;
	uint32_t			num_aborts;
	uint32_t			signature;
	uint32_t			firm_request_len;
	uint32_t			firm_numbers_queue;
	uint32_t			firm_sdram_size;
	uint32_t			firm_hd_channels;
	uint32_t                           	firm_cfg_version;	
	char			firm_model[12];
	char			firm_version[20];
	char			device_map[20];			
	struct work_struct 		arcmsr_do_message_isr_bh;
	struct timer_list		eternal_timer;
	unsigned short		fw_flag;
				#define	FW_NORMAL	0x0000
				#define	FW_BOG		0x0001
				#define	FW_DEADLOCK	0x0010
	atomic_t 			rq_map_token;
	atomic_t			ante_token_value;
};
struct CommandControlBlock{
	
	struct list_head		list;				
	struct scsi_cmnd		*pcmd;				
	struct AdapterControlBlock	*acb;				
	uint32_t			cdb_phyaddr_pattern;		
	uint32_t			arc_cdb_size;			
	uint16_t			ccb_flags;			
	#define			CCB_FLAG_READ			0x0000
	#define			CCB_FLAG_WRITE		0x0001
	#define			CCB_FLAG_ERROR		0x0002
	#define			CCB_FLAG_FLUSHCACHE		0x0004
	#define			CCB_FLAG_MASTER_ABORTED	0x0008	
	uint16_t                        	startdone;			
	#define			ARCMSR_CCB_DONE   	        	0x0000
	#define			ARCMSR_CCB_START		0x55AA
	#define			ARCMSR_CCB_ABORTED		0xAA55
	#define			ARCMSR_CCB_ILLEGAL		0xFFFF
	#if BITS_PER_LONG == 64
	
		uint32_t                        	reserved[5];		
	#else
	
		uint32_t                        	reserved;		
	#endif
	
	struct ARCMSR_CDB		arcmsr_cdb;
};
struct SENSE_DATA
{
	uint8_t				ErrorCode:7;
#define SCSI_SENSE_CURRENT_ERRORS	0x70
#define SCSI_SENSE_DEFERRED_ERRORS	0x71
	uint8_t				Valid:1;
	uint8_t				SegmentNumber;
	uint8_t				SenseKey:4;
	uint8_t				Reserved:1;
	uint8_t				IncorrectLength:1;
	uint8_t				EndOfMedia:1;
	uint8_t				FileMark:1;
	uint8_t				Information[4];
	uint8_t				AdditionalSenseLength;
	uint8_t				CommandSpecificInformation[4];
	uint8_t				AdditionalSenseCode;
	uint8_t				AdditionalSenseCodeQualifier;
	uint8_t				FieldReplaceableUnitCode;
	uint8_t				SenseKeySpecific[3];
};
#define     ARCMSR_MU_OUTBOUND_INTERRUPT_STATUS_REG                 0x30
#define     ARCMSR_MU_OUTBOUND_PCI_INT                              0x10
#define     ARCMSR_MU_OUTBOUND_POSTQUEUE_INT                        0x08
#define     ARCMSR_MU_OUTBOUND_DOORBELL_INT                         0x04
#define     ARCMSR_MU_OUTBOUND_MESSAGE1_INT                         0x02
#define     ARCMSR_MU_OUTBOUND_MESSAGE0_INT                         0x01
#define     ARCMSR_MU_OUTBOUND_HANDLE_INT                 \
                    (ARCMSR_MU_OUTBOUND_MESSAGE0_INT      \
                     |ARCMSR_MU_OUTBOUND_MESSAGE1_INT     \
                     |ARCMSR_MU_OUTBOUND_DOORBELL_INT     \
                     |ARCMSR_MU_OUTBOUND_POSTQUEUE_INT    \
                     |ARCMSR_MU_OUTBOUND_PCI_INT)
#define     ARCMSR_MU_OUTBOUND_INTERRUPT_MASK_REG                   0x34
#define     ARCMSR_MU_OUTBOUND_PCI_INTMASKENABLE                    0x10
#define     ARCMSR_MU_OUTBOUND_POSTQUEUE_INTMASKENABLE              0x08
#define     ARCMSR_MU_OUTBOUND_DOORBELL_INTMASKENABLE               0x04
#define     ARCMSR_MU_OUTBOUND_MESSAGE1_INTMASKENABLE               0x02
#define     ARCMSR_MU_OUTBOUND_MESSAGE0_INTMASKENABLE               0x01
#define     ARCMSR_MU_OUTBOUND_ALL_INTMASKENABLE                    0x1F

extern void arcmsr_post_ioctldata2iop(struct AdapterControlBlock *);
extern void arcmsr_iop_message_read(struct AdapterControlBlock *);
extern struct QBUFFER __iomem *arcmsr_get_iop_rqbuffer(struct AdapterControlBlock *);
extern struct device_attribute *arcmsr_host_attrs[];
extern int arcmsr_alloc_sysfs_attr(struct AdapterControlBlock *);
void arcmsr_free_sysfs_attr(struct AdapterControlBlock *acb);
