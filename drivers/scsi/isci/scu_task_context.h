/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SCU_TASK_CONTEXT_H_
#define _SCU_TASK_CONTEXT_H_



typedef enum {
	SCU_TASK_TYPE_IOREAD,           
	SCU_TASK_TYPE_IOWRITE,          
	SCU_TASK_TYPE_SMP_REQUEST,      
	SCU_TASK_TYPE_RESPONSE,         
	SCU_TASK_TYPE_RAW_FRAME,        
	SCU_TASK_TYPE_PRIMITIVE         
} scu_ssp_task_type;

typedef enum {
	SCU_TASK_TYPE_DMA_IN,           
	SCU_TASK_TYPE_FPDMAQ_READ,      
	SCU_TASK_TYPE_PACKET_DMA_IN,    
	SCU_TASK_TYPE_SATA_RAW_FRAME,   
	RESERVED_4,
	RESERVED_5,
	RESERVED_6,
	RESERVED_7,
	SCU_TASK_TYPE_DMA_OUT,          
	SCU_TASK_TYPE_FPDMAQ_WRITE,     
	SCU_TASK_TYPE_PACKET_DMA_OUT    
} scu_sata_task_type;


#define SCU_TASK_CONTEXT_TYPE  0
#define SCU_RNC_CONTEXT_TYPE   1

#define SCU_TASK_CONTEXT_INVALID          0
#define SCU_TASK_CONTEXT_VALID            1

#define SCU_COMMAND_CODE_INITIATOR_NEW_TASK   0
#define SCU_COMMAND_CODE_ACTIVE_TASK          1
#define SCU_COMMAND_CODE_PRIMITIVE_SEQ_TASK   2
#define SCU_COMMAND_CODE_TARGET_RAW_FRAMES    3

#define SCU_TASK_PRIORITY_NORMAL          0

#define SCU_TASK_PRIORITY_HEAD_OF_Q       1

#define SCU_TASK_PRIORITY_HIGH            2

#define SCU_TASK_PRIORITY_RESERVED        3

#define SCU_TASK_INITIATOR_MODE           1
#define SCU_TASK_TARGET_MODE              0

#define SCU_TASK_REGULAR                  0
#define SCU_TASK_ABORTED                  1

#define SCU_SATA_WRITE_DATA_DIRECTION     0
#define SCU_SATA_READ_DATA_DIRECTION      1

#define SCU_CONTEXT_COMMAND_REQUEST_TYPE_SHIFT           21
#define SCU_CONTEXT_COMMAND_REQUEST_TYPE_MASK            0x00E00000
#define scu_get_command_request_type(x)	\
	((x) & SCU_CONTEXT_COMMAND_REQUEST_TYPE_MASK)

#define SCU_CONTEXT_COMMAND_REQUEST_SUBTYPE_SHIFT        18
#define SCU_CONTEXT_COMMAND_REQUEST_SUBTYPE_MASK         0x001C0000
#define scu_get_command_request_subtype(x) \
	((x) & SCU_CONTEXT_COMMAND_REQUEST_SUBTYPE_MASK)

#define SCU_CONTEXT_COMMAND_REQUEST_FULLTYPE_MASK	 \
	(\
		SCU_CONTEXT_COMMAND_REQUEST_TYPE_MASK		  \
		| SCU_CONTEXT_COMMAND_REQUEST_SUBTYPE_MASK	    \
	)
#define scu_get_command_request_full_type(x) \
	((x) & SCU_CONTEXT_COMMAND_REQUEST_FULLTYPE_MASK)

#define SCU_CONTEXT_COMMAND_PROTOCOL_ENGINE_GROUP_SHIFT  16
#define SCU_CONTEXT_COMMAND_PROTOCOL_ENGINE_GROUP_MASK   0x00010000
#define scu_get_command_protocl_engine_group(x)	\
	((x) & SCU_CONTEXT_COMMAND_PROTOCOL_ENGINE_GROUP_MASK)

#define SCU_CONTEXT_COMMAND_LOGICAL_PORT_SHIFT           12
#define SCU_CONTEXT_COMMAND_LOGICAL_PORT_MASK            0x00007000
#define scu_get_command_reqeust_logical_port(x)	\
	((x) & SCU_CONTEXT_COMMAND_LOGICAL_PORT_MASK)


#define MAKE_SCU_CONTEXT_COMMAND_TYPE(type) \
	((u32)(type) << SCU_CONTEXT_COMMAND_REQUEST_TYPE_SHIFT)

#define SCU_CONTEXT_COMMAND_REQUEST_TYPE_POST_TC    MAKE_SCU_CONTEXT_COMMAND_TYPE(0)
#define SCU_CONTEXT_COMMAND_REQUEST_TYPE_DUMP_TC    MAKE_SCU_CONTEXT_COMMAND_TYPE(1)
#define SCU_CONTEXT_COMMAND_REQUEST_TYPE_POST_RNC   MAKE_SCU_CONTEXT_COMMAND_TYPE(2)
#define SCU_CONTEXT_COMMAND_REQUEST_TYPE_DUMP_RNC   MAKE_SCU_CONTEXT_COMMAND_TYPE(3)
#define SCU_CONTEXT_COMMAND_REQUEST_TYPE_OTHER_RNC  MAKE_SCU_CONTEXT_COMMAND_TYPE(6)

#define MAKE_SCU_CONTEXT_COMMAND_REQUEST(type, command)	\
	((type) | ((command) << SCU_CONTEXT_COMMAND_REQUEST_SUBTYPE_SHIFT))

#define SCU_CONTEXT_COMMAND_REQUST_POST_TC \
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_POST_TC, 0))

#define SCU_CONTEXT_COMMAND_REQUEST_POST_TC_ABORT \
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_POST_TC, 1))

#define SCU_CONTEXT_COMMAND_REQUST_DUMP_TC \
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_DUMP_TC, 0))

#define SCU_CONTEXT_COMMAND_POST_RNC_32	\
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_POST_RNC, 0))

#define SCU_CONTEXT_COMMAND_POST_RNC_96	\
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_POST_RNC, 1))

#define SCU_CONTEXT_COMMAND_POST_RNC_INVALIDATE	\
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_POST_RNC, 2))

#define SCU_CONTEXT_COMMAND_DUMP_RNC_32	\
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_DUMP_RNC, 0))

#define SCU_CONTEXT_COMMAND_DUMP_RNC_96	\
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_DUMP_RNC, 1))

#define SCU_CONTEXT_COMMAND_POST_RNC_SUSPEND_TX	\
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_OTHER_RNC, 0))

#define SCU_CONTEXT_COMMAND_POST_RNC_SUSPEND_TX_RX \
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_OTHER_RNC, 1))

#define SCU_CONTEXT_COMMAND_POST_RNC_RESUME \
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_OTHER_RNC, 2))

#define SCU_CONTEXT_IT_NEXUS_LOSS_TIMER_ENABLE \
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_OTHER_RNC, 3))

#define SCU_CONTEXT_IT_NEXUS_LOSS_TIMER_DISABLE	\
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_OTHER_RNC, 4))

#define SCU_TASK_CONTEXT_PROTOCOL_SMP    0x00
#define SCU_TASK_CONTEXT_PROTOCOL_SSP    0x01
#define SCU_TASK_CONTEXT_PROTOCOL_STP    0x02
#define SCU_TASK_CONTEXT_PROTOCOL_NONE   0x07

struct ssp_task_context {
	
	u32 reserved00:24;
	u32 frame_type:8;

	
	u32 reserved01;

	
	u32 fill_bytes:2;
	u32 reserved02:6;
	u32 changing_data_pointer:1;
	u32 retransmit:1;
	u32 retry_data_frame:1;
	u32 tlr_control:2;
	u32 reserved03:19;

	
	u32 uiRsvd4;

	
	u32 target_port_transfer_tag:16;
	u32 tag:16;

	
	u32 data_offset;
};

struct stp_task_context {
	
	u32 fis_type:8;
	u32 pm_port:4;
	u32 reserved0:3;
	u32 control:1;
	u32 command:8;
	u32 features:8;

	
	u32 reserved1;

	
	u32 reserved2;

	
	u32 reserved3;

	
	u32 ncq_tag:5;
	u32 reserved4:27;

	
	u32 data_offset; 
};

struct smp_task_context {
	
	u32 response_length:8;
	u32 function_result:8;
	u32 function:8;
	u32 frame_type:8;

	
	u32 smp_response_ufi:12;
	u32 reserved1:20;

	
	u32 reserved2;

	
	u32 reserved3;

	
	u32 reserved4;

	
	u32 reserved5;
};

struct primitive_task_context {
	
	u32 control; 

	
	u32 sequence;

	
	u32 reserved0;

	
	u32 reserved1;

	
	u32 reserved2;

	
	u32 reserved3;
};

union protocol_context {
	struct ssp_task_context ssp;
	struct stp_task_context stp;
	struct smp_task_context smp;
	struct primitive_task_context primitive;
	u32 words[6];
};

struct scu_sgl_element {
	u32 address_upper;

	u32 address_lower;

	u32 length:24;

	u32 address_modifier:8;

};

#define SCU_SGL_ELEMENT_PAIR_A   0
#define SCU_SGL_ELEMENT_PAIR_B   1

struct scu_sgl_element_pair {
	
	struct scu_sgl_element A;

	
	struct scu_sgl_element B;

	
	u32 next_pair_upper;

	u32 next_pair_lower;

};

struct transport_snapshot {
	
	u32 xfer_rdy_write_data_length;

	
	u32 data_offset;

	
	u32 data_transfer_size:24;
	u32 reserved_50_0:8;

	
	u32 next_initiator_write_data_offset;

	
	u32 next_initiator_write_data_xfer_size:24;
	u32 reserved_58_0:8;
};

struct scu_task_context {
	
	u32 priority:2;

	u32 initiator_request:1;

	u32 connection_rate:4;

	u32 protocol_engine_index:3;

	u32 logical_port_index:3;

	u32 protocol_type:3;

	u32 task_index:12;

	u32 reserved_00_0:1;

	u32 abort:1;

	u32 valid:1;

	u32 context_type:1;

	
	u32 remote_node_index:12;

	u32 mirrored_node_index:12;

	u32 sata_direction:1;

	u32 command_code:2;

	u32 suspend_node:1;

	u32 task_type:4;

	
	u32 link_layer_control:8; 

	u32 ssp_tlr_enable:1;

	u32 dma_ssp_target_good_response:1;

	u32 do_not_dma_ssp_good_response:1;

	u32 strict_ordering:1;

	u32 control_frame:1;

	u32 tl_control_reserved:3;

	u32 timeout_enable:1;

	u32 pts_control_reserved:7;

	u32 block_guard_enable:1;

	u32 sdma_control_reserved:7;

	
	u32 address_modifier:16;

	u32 mirrored_protocol_engine:3;  

	u32 mirrored_logical_port:4;  

	u32 reserved_0C_0:8;

	u32 mirror_request_enable:1;  

	
	u32 ssp_command_iu_length:8;

	u32 xfer_ready_tlr_enable:1;

	u32 reserved_10_0:7;

	u32 ssp_max_burst_size:16;

	
	u32 transfer_length_bytes:24; 

	u32 reserved_14_0:8;

	
	union protocol_context type;

	
	u32 command_iu_upper;

	u32 command_iu_lower;

	
	u32 response_iu_upper;

	u32 response_iu_lower;

	
	u32 task_phase:8;

	u32 task_status:8;

	u32 previous_extended_tag:4;

	u32 stp_retry_count:2;

	u32 reserved_40_1:2;

	u32 ssp_tlr_threshold:4;

	u32 reserved_40_2:4;

	
	u32 write_data_length; 

	
	struct transport_snapshot snapshot; 

	
	u32 blk_prot_en:1;
	u32 blk_sz:2;
	u32 blk_prot_func:2;
	u32 reserved_5C_0:9;
	u32 active_sgl_element:2;  
	u32 sgl_exhausted:1;  
	u32 payload_data_transfer_error:4;  
	u32 frame_buffer_offset:11; 

	
	struct scu_sgl_element_pair sgl_pair_ab;
	
	struct scu_sgl_element_pair sgl_pair_cd;

	
	struct scu_sgl_element_pair sgl_snapshot_ac;

	
	u32 active_sgl_element_pair; 

	
	u32 reserved_C4_CC[3];

	
	u32 interm_crc_val:16;
	u32 init_crc_seed:16;

	
	u32 app_tag_verify:16;
	u32 app_tag_gen:16;

	
	u32 ref_tag_seed_verify;

	
	u32 UD_bytes_immed_val:13;
	u32 reserved_DC_0:3;
	u32 DIF_bytes_immed_val:4;
	u32 reserved_DC_1:12;

	
	u32 bgc_blk_sz:13;
	u32 reserved_E0_0:3;
	u32 app_tag_gen_mask:16;

	
	union {
		u16 bgctl;
		struct {
			u16 crc_verify:1;
			u16 app_tag_chk:1;
			u16 ref_tag_chk:1;
			u16 op:2;
			u16 legacy:1;
			u16 invert_crc_seed:1;
			u16 ref_tag_gen:1;
			u16 fixed_ref_tag:1;
			u16 invert_crc:1;
			u16 app_ref_f_detect:1;
			u16 uninit_dif_check_err:1;
			u16 uninit_dif_bypass:1;
			u16 app_f_detect:1;
			u16 reserved_0:2;
		} bgctl_f;
	};

	u16 app_tag_verify_mask;

	
	u32 blk_guard_err:8;
	u32 reserved_E8_0:24;

	
	u32 ref_tag_seed_gen;

	
	u32 intermediate_crc_valid_snapshot:16;
	u32 reserved_F0_0:16;

	
	u32 reference_tag_seed_for_verify_function_snapshot;

	
	u32 snapshot_of_reserved_dword_DC_of_tc;

	
	u32 reference_tag_seed_for_generate_function_snapshot;

} __packed;

#endif 
