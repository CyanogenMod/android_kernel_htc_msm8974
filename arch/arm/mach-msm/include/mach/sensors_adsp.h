/* Copyright (c) 2012,  The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef SENSORS_ADSP_H
#define SENSORS_ADSP_H

#include <linux/types.h>

#define SNS_OCMEM_MAX_NUM_SEG_V01 16

#define SNS_OCMEM_MAX_VECTORS_SIZE_V01 512


#define SNS_OCMEM_CANCEL_REQ_V01 0x0000
#define SNS_OCMEM_CANCEL_RESP_V01 0x0000
#define SNS_OCMEM_VERSION_REQ_V01 0x0001
#define SNS_OCMEM_VERSION_RESP_V01 0x0001
#define SNS_OCMEM_PHYS_ADDR_REQ_V01 0x0002
#define SNS_OCMEM_PHYS_ADDR_RESP_V01 0x0002
#define SNS_OCMEM_HAS_CLIENT_IND_V01 0x0002
#define SNS_OCMEM_BW_VOTE_REQ_V01 0x0003
#define SNS_OCMEM_BW_VOTE_RESP_V01 0x0003
#define SNS_OCMEM_BW_VOTE_IND_V01 0x0003

enum {
	SNS_OCMEM_MODULE_KERNEL = 0,
	SNS_OCMEM_MODULE_ADSP
};

enum {
	SNS_OCMEM_MSG_TYPE_REQ = 0,  
	SNS_OCMEM_MSG_TYPE_RESP,     
	SNS_OCMEM_MSG_TYPE_IND       
};

struct sns_ocmem_hdr_s {
	int32_t  msg_id ;	
	uint16_t msg_size;	
	uint8_t  dst_module;	
	uint8_t  src_module;	
	uint8_t  msg_type;	
} __packed;

struct sns_ocmem_common_resp_s_v01 {
	
	uint8_t sns_result_t;
	uint8_t sns_err_t;
	
};

struct sns_mem_segment_s_v01 {

	uint64_t start_address; 
	uint32_t size; 
	uint16_t type; 
} __packed;

struct sns_ocmem_phys_addr_resp_msg_v01 {
	struct sns_ocmem_common_resp_s_v01 resp; 
	uint32_t segments_len; 
	
	struct sns_mem_segment_s_v01 segments[SNS_OCMEM_MAX_NUM_SEG_V01];
	uint8_t segments_valid; 
} __packed ;

struct sns_ocmem_has_client_ind_msg_v01 {
	uint16_t num_clients; 
} __packed;

struct sns_ocmem_bw_vote_req_msg_v01 {
	uint8_t is_map;		
	uint8_t vectors_valid;  
	uint32_t vectors_len;	
	uint8_t vectors[SNS_OCMEM_MAX_VECTORS_SIZE_V01]; 
} __packed;

struct sns_ocmem_bw_vote_resp_msg_v01 {
	struct sns_ocmem_common_resp_s_v01 resp;
};

struct sns_ocmem_bw_vote_ind_msg_v01 {
	uint8_t is_vote_on;
} __packed;

#endif 
