/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2010 - 2012 Intel Corporation. All rights reserved.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2010 - 2012 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 *
 *****************************************************************************/
#ifndef __IWL_TESTMODE_H__
#define __IWL_TESTMODE_H__

#include <linux/types.h>


enum iwl_tm_cmd_t {
	IWL_TM_CMD_APP2DEV_UCODE		= 1,
	IWL_TM_CMD_APP2DEV_DIRECT_REG_READ32	= 2,
	IWL_TM_CMD_APP2DEV_DIRECT_REG_WRITE32	= 3,
	IWL_TM_CMD_APP2DEV_DIRECT_REG_WRITE8	= 4,
	IWL_TM_CMD_APP2DEV_GET_DEVICENAME	= 5,
	IWL_TM_CMD_APP2DEV_LOAD_INIT_FW		= 6,
	IWL_TM_CMD_APP2DEV_CFG_INIT_CALIB	= 7,
	IWL_TM_CMD_APP2DEV_LOAD_RUNTIME_FW	= 8,
	IWL_TM_CMD_APP2DEV_GET_EEPROM		= 9,
	IWL_TM_CMD_APP2DEV_FIXRATE_REQ		= 10,
	IWL_TM_CMD_APP2DEV_BEGIN_TRACE		= 11,
	IWL_TM_CMD_APP2DEV_END_TRACE		= 12,
	IWL_TM_CMD_APP2DEV_READ_TRACE		= 13,
	IWL_TM_CMD_DEV2APP_SYNC_RSP		= 14,
	IWL_TM_CMD_DEV2APP_UCODE_RX_PKT		= 15,
	IWL_TM_CMD_DEV2APP_EEPROM_RSP		= 16,
	IWL_TM_CMD_APP2DEV_OWNERSHIP		= 17,
	RESERVED_18				= 18,
	RESERVED_19				= 19,
	RESERVED_20				= 20,
	RESERVED_21				= 21,
	IWL_TM_CMD_APP2DEV_LOAD_WOWLAN_FW	= 22,
	IWL_TM_CMD_APP2DEV_GET_FW_VERSION	= 23,
	IWL_TM_CMD_APP2DEV_GET_DEVICE_ID	= 24,
	IWL_TM_CMD_APP2DEV_GET_FW_INFO		= 25,
	IWL_TM_CMD_APP2DEV_INDIRECT_BUFFER_READ = 26,
	IWL_TM_CMD_APP2DEV_INDIRECT_BUFFER_DUMP = 27,
	IWL_TM_CMD_APP2DEV_INDIRECT_BUFFER_WRITE = 28,
	IWL_TM_CMD_APP2DEV_NOTIFICATIONS	= 29,
	IWL_TM_CMD_MAX				= 30,
};

enum iwl_tm_attr_t {
	IWL_TM_ATTR_NOT_APPLICABLE		= 0,
	IWL_TM_ATTR_COMMAND			= 1,
	IWL_TM_ATTR_UCODE_CMD_ID		= 2,
	IWL_TM_ATTR_UCODE_CMD_DATA		= 3,
	IWL_TM_ATTR_REG_OFFSET			= 4,
	IWL_TM_ATTR_REG_VALUE8			= 5,
	IWL_TM_ATTR_REG_VALUE32			= 6,
	IWL_TM_ATTR_SYNC_RSP			= 7,
	IWL_TM_ATTR_UCODE_RX_PKT		= 8,
	IWL_TM_ATTR_EEPROM			= 9,
	IWL_TM_ATTR_TRACE_ADDR			= 10,
	IWL_TM_ATTR_TRACE_SIZE			= 11,
	IWL_TM_ATTR_TRACE_DUMP			= 12,
	IWL_TM_ATTR_FIXRATE			= 13,
	IWL_TM_ATTR_UCODE_OWNER			= 14,
	IWL_TM_ATTR_MEM_ADDR			= 15,
	IWL_TM_ATTR_BUFFER_SIZE			= 16,
	IWL_TM_ATTR_BUFFER_DUMP			= 17,
	IWL_TM_ATTR_FW_VERSION			= 18,
	IWL_TM_ATTR_DEVICE_ID			= 19,
	IWL_TM_ATTR_FW_TYPE			= 20,
	IWL_TM_ATTR_FW_INST_SIZE		= 21,
	IWL_TM_ATTR_FW_DATA_SIZE		= 22,
	IWL_TM_ATTR_UCODE_CMD_SKB		= 23,
	IWL_TM_ATTR_ENABLE_NOTIFICATION		= 24,
	IWL_TM_ATTR_MAX				= 25,
};

#define TRACE_BUFF_SIZE_MAX	0x200000
#define TRACE_BUFF_SIZE_MIN	0x20000
#define TRACE_BUFF_SIZE_DEF	TRACE_BUFF_SIZE_MIN
#define TRACE_BUFF_PADD		0x2000

#define DUMP_CHUNK_SIZE		(PAGE_SIZE - 1024)

#define SRAM_DATA_SEG_OFFSET   0x800000

#endif
