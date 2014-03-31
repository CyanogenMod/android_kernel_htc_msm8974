/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef __SSM_H_
#define __SSM_H_

#define MAX_APP_NAME_SIZE	32
#define ENC_MODE_MAX_SIZE	200

enum tz_response {
	RESULT_SUCCESS = 0,
	RESULT_FAILURE  = 0xFFFFFFFF,
};

enum tz_commands {
	ENC_MODE,
	GET_ENC_MODE,
	KEY_EXCHANGE = 11,
};

enum ssm_ipc_req {
	SSM_IPC_MIN = 0x0000AAAB,
	SSM_ATOM_MODE_UPDATE,
	SSM_MTOA_MODE_UPDATE_STATUS = SSM_IPC_MIN + 4,
	SSM_INVALID_REQ,
};

enum oem_req {
	SSM_READY,
	SSM_MODE_INFO_READY,
	SSM_SET_MODE,
	SSM_GET_MODE_STATUS,
	SSM_INVALID,
};

enum modem_mode_status {
	SUCCESS,
	RETRY,
	FAILED = -1,
};

__packed struct tzapp_mode_enc_req {
	uint32_t tzapp_ssm_cmd;
	uint8_t  mode_info[4];
};

__packed struct tzapp_mode_enc_rsp {
	uint32_t tzapp_ssm_cmd;
	uint8_t enc_mode_info[ENC_MODE_MAX_SIZE];
	uint32_t enc_mode_len;
	long status;
};

__packed struct tzapp_get_mode_info_req {
	uint32_t tzapp_ssm_cmd;
};

__packed struct tzapp_get_mode_info_rsp {
	uint32_t tzapp_ssm_cmd;
	uint8_t  enc_mode_info[ENC_MODE_MAX_SIZE];
	uint32_t enc_mode_len;
	long status;
};

struct ssm_common_msg {
	enum ssm_ipc_req ipc_req;
	int err_code;

};

#endif
