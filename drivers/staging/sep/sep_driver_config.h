/*
 *
 *  sep_driver_config.h - Security Processor Driver configuration
 *
 *  Copyright(c) 2009-2011 Intel Corporation. All rights reserved.
 *  Contributions(c) 2009-2011 Discretix. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  this program; if not, write to the Free Software Foundation, Inc., 59
 *  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *  CONTACTS:
 *
 *  Mark Allyn		mark.a.allyn@intel.com
 *  Jayant Mangalampalli jayant.mangalampalli@intel.com
 *
 *  CHANGES:
 *
 *  2010.06.26	Upgrade to Medfield
 *  2011.02.22  Enable kernel crypto
 *
 */

#ifndef __SEP_DRIVER_CONFIG_H__
#define __SEP_DRIVER_CONFIG_H__



#define SEP_DRIVER_POLLING_MODE                         0

#define SEP_DRIVER_RECONFIG_MESSAGE_AREA                0

#define SEP_DRIVER_ARM_DEBUG_MODE                       0

#define SEP_START_MSG_TOKEN				0x02558808

#define SEP_DRIVER_IN_FLAG                              0

#define SEP_DRIVER_OUT_FLAG                             1

#define SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP             31

#define SEP_DRIVER_MIN_DATA_SIZE_PER_TABLE		16

#define SEP_DRIVER_OWN_LOCK_FLAG                        1

#define SEP_DRIVER_DISOWN_LOCK_FLAG                     0

#define SEP_REQUEST_DAEMON_MAPPED 1
#define SEP_REQUEST_DAEMON_UNMAPPED 0


#define SEP_DEV_NAME "sep_sec_driver"
#define SEP_DEV_SINGLETON "sep_sec_singleton_driver"
#define SEP_DEV_DAEMON "sep_req_daemon_driver"


#define SEP_DRIVER_MIN_MESSAGE_SIZE_IN_BYTES			(5*sizeof(u32))

#define SEP_DRIVER_MAX_MESSAGE_SIZE_IN_BYTES			(8 * 1024)

#define SEP_DRIVER_MESSAGE_SHARED_AREA_SIZE_IN_BYTES		(8 * 1024)

#define SEP_DRIVER_STATIC_AREA_SIZE_IN_BYTES			(4 * 1024)

#define SEP_DRIVER_DATA_POOL_SHARED_AREA_SIZE_IN_BYTES		(16 * 1024)

#define SYNCHRONIC_DMA_TABLES_AREA_SIZE_BYTES	(1024 * 29)

#define SEP_DRIVER_FLOW_DMA_TABLES_AREA_SIZE_IN_BYTES		(1024 * 4)

#define SEP_DRIVER_SYSTEM_DATA_MEMORY_SIZE_IN_BYTES		(1024 * 3)

#define SEP_DRIVER_PRINTF_OFFSET_IN_BYTES			(5888)

#define SEP_DRIVER_TIME_MEMORY_SIZE_IN_BYTES			8

#define SEP_DRIVER_SYSTEM_RAR_MEMORY_SIZE_IN_BYTES		8

#define SEP_DRIVER_MMMAP_AREA_SIZE				(1024 * 28)


#define SEP_DRIVER_MESSAGE_AREA_OFFSET_IN_BYTES			0

#define SEP_DRIVER_STATIC_AREA_OFFSET_IN_BYTES \
	(SEP_DRIVER_MESSAGE_SHARED_AREA_SIZE_IN_BYTES)

#define SEP_DRIVER_DATA_POOL_AREA_OFFSET_IN_BYTES \
	(SEP_DRIVER_STATIC_AREA_OFFSET_IN_BYTES + \
	SEP_DRIVER_STATIC_AREA_SIZE_IN_BYTES)

#define SYNCHRONIC_DMA_TABLES_AREA_OFFSET_BYTES \
	(SEP_DRIVER_DATA_POOL_AREA_OFFSET_IN_BYTES + \
	SEP_DRIVER_DATA_POOL_SHARED_AREA_SIZE_IN_BYTES)

#define SEP_DRIVER_SYSTEM_DATA_MEMORY_OFFSET_IN_BYTES \
	(SYNCHRONIC_DMA_TABLES_AREA_OFFSET_BYTES + \
	SYNCHRONIC_DMA_TABLES_AREA_SIZE_BYTES)

#define SEP_DRIVER_SYSTEM_TIME_MEMORY_OFFSET_IN_BYTES \
	(SEP_DRIVER_SYSTEM_DATA_MEMORY_OFFSET_IN_BYTES)

#define SEP_DRIVER_SYSTEM_RAR_MEMORY_OFFSET_IN_BYTES \
	(SEP_DRIVER_SYSTEM_TIME_MEMORY_OFFSET_IN_BYTES + \
	SEP_DRIVER_TIME_MEMORY_SIZE_IN_BYTES)

#define SEP_CALLER_ID_OFFSET_BYTES \
	(SEP_DRIVER_SYSTEM_RAR_MEMORY_OFFSET_IN_BYTES + \
	SEP_DRIVER_SYSTEM_RAR_MEMORY_SIZE_IN_BYTES)

#define SEP_DRIVER_SYSTEM_DCB_MEMORY_OFFSET_IN_BYTES \
	(SEP_DRIVER_SYSTEM_DATA_MEMORY_OFFSET_IN_BYTES + \
	0x400)

#define SEP_DRIVER_SYSTEM_EXT_CACHE_ADDR_OFFSET_IN_BYTES \
	SEP_DRIVER_SYSTEM_RAR_MEMORY_OFFSET_IN_BYTES

#define SEP_DRIVER_DATA_POOL_ALLOCATION_OFFSET_IN_BYTES \
	(SEP_CALLER_ID_OFFSET_BYTES + \
	SEP_CALLER_ID_HASH_SIZE_IN_BYTES)

#define SEP_TIME_VAL_TOKEN                                    0x12345678

#define FAKE_RAR_SIZE (1024*1024) 

#define SEP_CALLER_ID_HASH_SIZE_IN_BYTES                      32

#define SEP_CALLER_ID_HASH_SIZE_IN_WORDS                8

#define SEP_CALLER_ID_TABLE_NUM_ENTRIES                       20

#define SEP_MAX_NUM_SYNC_DMA_OPS			16

#define SEP_RAR_VAL_TOKEN                                     0xABABABAB

#define SEP_ALREADY_INITIALIZED_ERR                           12

#define SEP_TRANSACTION_STARTED_LOCK_BIT                      0

#define SEP_WORKING_LOCK_BIT                                  1

#define SEP_STATIC_POOL_VAL_TOKEN                             0xABBAABBA

#define SEP_DATA_POOL_POINTERS_VAL_TOKEN                      0xEDDEEDDE

#define SEP_EXT_CACHE_ADDR_VAL_TOKEN                          0xBABABABA

#define WAIT_TIME 10

#define SUSPEND_DELAY 10

#define SCU_DELAY_MAX 50

#define SCU_DELAY_ITERATION 10



#define SEP_FASTCALL_WRITE_DONE_OFFSET		0

#define SEP_LEGACY_MMAP_DONE_OFFSET		1

#define SEP_LEGACY_SENDMSG_DONE_OFFSET		2

#define SEP_LEGACY_POLL_DONE_OFFSET		3

#define SEP_LEGACY_ENDTRANSACTION_DONE_OFFSET	4

#define SEP_DOUBLEBUF_USERS_LIMIT		3

#define SEP_FC_MAGIC				0xFFAACCAA

#define SEP_ENABLE_RUNTIME_PM

#endif 
