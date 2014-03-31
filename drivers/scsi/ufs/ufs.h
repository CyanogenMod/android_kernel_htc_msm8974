/*
 * Universal Flash Storage Host controller driver
 *
 * This code is based on drivers/scsi/ufs/ufs.h
 * Copyright (C) 2011-2013 Samsung India Software Operations
 *
 * Authors:
 *	Santosh Yaraganavi <santosh.sy@samsung.com>
 *	Vinayak Holikatti <h.vinayak@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * See the COPYING file in the top-level directory or visit
 * <http://www.gnu.org/licenses/gpl-2.0.html>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program is provided "AS IS" and "WITH ALL FAULTS" and
 * without warranty of any kind. You are solely responsible for
 * determining the appropriateness of using and distributing
 * the program and assume all risks associated with your exercise
 * of rights with respect to the program, including but not limited
 * to infringement of third party rights, the risks and costs of
 * program errors, damage to or loss of data, programs or equipment,
 * and unavailability or interruption of operations. Under no
 * circumstances will the contributor of this Program be liable for
 * any damages of any kind arising from your use or distribution of
 * this program.
 */

#ifndef _UFS_H
#define _UFS_H

#define MAX_CDB_SIZE	16

#define UPIU_HEADER_DWORD(byte3, byte2, byte1, byte0)\
			((byte3 << 24) | (byte2 << 16) |\
			 (byte1 << 8) | (byte0))


enum {
	UFS_ABORT_TASK		= 0x01,
	UFS_ABORT_TASK_SET	= 0x02,
	UFS_CLEAR_TASK_SET	= 0x04,
	UFS_LOGICAL_RESET	= 0x08,
	UFS_QUERY_TASK		= 0x80,
	UFS_QUERY_TASK_SET	= 0x81,
};

enum {
	UPIU_TRANSACTION_NOP_OUT	= 0x00,
	UPIU_TRANSACTION_COMMAND	= 0x01,
	UPIU_TRANSACTION_DATA_OUT	= 0x02,
	UPIU_TRANSACTION_TASK_REQ	= 0x04,
	UPIU_TRANSACTION_QUERY_REQ	= 0x26,
};

enum {
	UPIU_TRANSACTION_NOP_IN		= 0x20,
	UPIU_TRANSACTION_RESPONSE	= 0x21,
	UPIU_TRANSACTION_DATA_IN	= 0x22,
	UPIU_TRANSACTION_TASK_RSP	= 0x24,
	UPIU_TRANSACTION_READY_XFER	= 0x31,
	UPIU_TRANSACTION_QUERY_RSP	= 0x36,
};

enum {
	UPIU_CMD_FLAGS_NONE	= 0x00,
	UPIU_CMD_FLAGS_WRITE	= 0x20,
	UPIU_CMD_FLAGS_READ	= 0x40,
};

enum {
	UPIU_TASK_ATTR_SIMPLE	= 0x00,
	UPIU_TASK_ATTR_ORDERED	= 0x01,
	UPIU_TASK_ATTR_HEADQ	= 0x02,
	UPIU_TASK_ATTR_ACA	= 0x03,
};

enum {
	UPIU_QUERY_OPCODE_NOP		= 0x0,
	UPIU_QUERY_OPCODE_READ_DESC	= 0x1,
	UPIU_QUERY_OPCODE_WRITE_DESC	= 0x2,
	UPIU_QUERY_OPCODE_READ_ATTR	= 0x3,
	UPIU_QUERY_OPCODE_WRITE_ATTR	= 0x4,
	UPIU_QUERY_OPCODE_READ_FLAG	= 0x5,
	UPIU_QUERY_OPCODE_SET_FLAG	= 0x6,
	UPIU_QUERY_OPCODE_CLEAR_FLAG	= 0x7,
	UPIU_QUERY_OPCODE_TOGGLE_FLAG	= 0x8,
};

enum {
	UPIU_COMMAND_SET_TYPE_SCSI	= 0x0,
	UPIU_COMMAND_SET_TYPE_UFS	= 0x1,
	UPIU_COMMAND_SET_TYPE_QUERY	= 0x2,
};

enum {
	MASK_SCSI_STATUS	= 0xFF,
	MASK_TASK_RESPONSE	= 0xFF00,
	MASK_RSP_UPIU_RESULT	= 0xFFFF,
};

enum {
	UPIU_TASK_MANAGEMENT_FUNC_COMPL		= 0x00,
	UPIU_TASK_MANAGEMENT_FUNC_NOT_SUPPORTED = 0x04,
	UPIU_TASK_MANAGEMENT_FUNC_SUCCEEDED	= 0x08,
	UPIU_TASK_MANAGEMENT_FUNC_FAILED	= 0x05,
	UPIU_INCORRECT_LOGICAL_UNIT_NO		= 0x09,
};
struct utp_upiu_header {
	u32 dword_0;
	u32 dword_1;
	u32 dword_2;
};

struct utp_upiu_cmd {
	struct utp_upiu_header header;
	u32 exp_data_transfer_len;
	u8 cdb[MAX_CDB_SIZE];
};

struct utp_upiu_rsp {
	struct utp_upiu_header header;
	u32 residual_transfer_count;
	u32 reserved[4];
	u16 sense_data_len;
	u8 sense_data[18];
};

struct utp_upiu_task_req {
	struct utp_upiu_header header;
	u32 input_param1;
	u32 input_param2;
	u32 input_param3;
	u32 reserved[2];
};

struct utp_upiu_task_rsp {
	struct utp_upiu_header header;
	u32 output_param1;
	u32 output_param2;
	u32 reserved[3];
};

#endif 
