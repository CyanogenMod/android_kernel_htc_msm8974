/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2004-2012 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com                                                  *
 *                                                                 *
 * This program is free software; you can redistribute it and/or   *
 * modify it under the terms of version 2 of the GNU General       *
 * Public License as published by the Free Software Foundation.    *
 * This program is distributed in the hope that it will be useful. *
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND          *
 * WARRANTIES, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,  *
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE      *
 * DISCLAIMED, EXCEPT TO THE EXTENT THAT SUCH DISCLAIMERS ARE HELD *
 * TO BE LEGALLY INVALID.  See the GNU General Public License for  *
 * more details, a copy of which can be found in the file COPYING  *
 * included with this package.                                     *
 *******************************************************************/

#include <asm/byteorder.h>

struct lpfc_hba;
#define LPFC_FCP_CDB_LEN 16

#define list_remove_head(list, entry, type, member)		\
	do {							\
	entry = NULL;						\
	if (!list_empty(list)) {				\
		entry = list_entry((list)->next, type, member);	\
		list_del_init(&entry->member);			\
	}							\
	} while(0)

#define list_get_first(list, type, member)			\
	(list_empty(list)) ? NULL :				\
	list_entry((list)->next, type, member)

struct lpfc_rport_data {
	struct lpfc_nodelist *pnode;	
};

struct fcp_rsp {
	uint32_t rspRsvd1;	
	uint32_t rspRsvd2;	

	uint8_t rspStatus0;	
	uint8_t rspStatus1;	
	uint8_t rspStatus2;	
#define RSP_LEN_VALID  0x01	
#define SNS_LEN_VALID  0x02	
#define RESID_OVER     0x04	
#define RESID_UNDER    0x08	
	uint8_t rspStatus3;	

	uint32_t rspResId;	
	
	uint32_t rspSnsLen;	
	
	uint32_t rspRspLen;	
	

	uint8_t rspInfo0;	
	uint8_t rspInfo1;	
	uint8_t rspInfo2;	
	uint8_t rspInfo3;	

#define RSP_NO_FAILURE       0x00
#define RSP_DATA_BURST_ERR   0x01
#define RSP_CMD_FIELD_ERR    0x02
#define RSP_RO_MISMATCH_ERR  0x03
#define RSP_TM_NOT_SUPPORTED 0x04	
#define RSP_TM_NOT_COMPLETED 0x05	

	uint32_t rspInfoRsvd;	

	uint8_t rspSnsInfo[128];
#define SNS_ILLEGAL_REQ 0x05	
#define SNSCOD_BADCMD 0x20	
};

struct fcp_cmnd {
	struct scsi_lun  fcp_lun;

	uint8_t fcpCntl0;	
	uint8_t fcpCntl1;	
#define  SIMPLE_Q        0x00
#define  HEAD_OF_Q       0x01
#define  ORDERED_Q       0x02
#define  ACA_Q           0x04
#define  UNTAGGED        0x05
	uint8_t fcpCntl2;	
#define  FCP_ABORT_TASK_SET  0x02	
#define  FCP_CLEAR_TASK_SET  0x04	
#define  FCP_BUS_RESET       0x08	
#define  FCP_LUN_RESET       0x10	
#define  FCP_TARGET_RESET    0x20	
#define  FCP_CLEAR_ACA       0x40	
#define  FCP_TERMINATE_TASK  0x80	
	uint8_t fcpCntl3;
#define  WRITE_DATA      0x01	
#define  READ_DATA       0x02	

	uint8_t fcpCdb[LPFC_FCP_CDB_LEN]; 
	uint32_t fcpDl;		

};

struct lpfc_scsicmd_bkt {
	uint32_t cmd_count;
};

struct lpfc_scsi_buf {
	struct list_head list;
	struct scsi_cmnd *pCmd;
	struct lpfc_rport_data *rdata;

	uint32_t timeout;

	uint16_t exch_busy;     
	uint16_t status;	
	uint32_t result;	

	uint32_t   seg_cnt;	
	uint32_t prot_seg_cnt;  

	dma_addr_t nonsg_phys;	

	void *data;
	dma_addr_t dma_handle;

	struct fcp_cmnd *fcp_cmnd;
	struct fcp_rsp *fcp_rsp;
	struct ulp_bde64 *fcp_bpl;

	dma_addr_t dma_phys_bpl;

	struct lpfc_iocbq cur_iocbq;
	wait_queue_head_t *waitq;
	unsigned long start_time;

#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	
	void *prot_data_segment;
	uint32_t prot_data;
	uint32_t prot_data_type;
#define	LPFC_INJERR_REFTAG	1
#define	LPFC_INJERR_APPTAG	2
#define	LPFC_INJERR_GUARD	3
#endif
};

#define LPFC_SCSI_DMA_EXT_SIZE 264
#define LPFC_BPL_SIZE          1024
#define MDAC_DIRECT_CMD                  0x22
