/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef __Q6CORE_H__
#define __Q6CORE_H__
#include <mach/qdsp6v2/apr.h>
#include <mach/ocmem.h>


#define AVCS_CMD_GET_LOW_POWER_SEGMENTS_INFO              0x00012903

struct avcs_cmd_get_low_power_segments_info {
	struct apr_hdr hdr;
} __packed;


#define AVCS_CMDRSP_GET_LOW_POWER_SEGMENTS_INFO           0x00012904

#define AVCS_CMD_ADSP_EVENT_GET_STATE		0x0001290C
#define AVCS_CMDRSP_ADSP_EVENT_GET_STATE	0x0001290D


#define READ_ONLY_SEGMENT      1
#define READ_WRITE_SEGMENT     2
#define AUDIO_SEGMENT          1
#define OS_SEGMENT             2

struct avcs_mem_segment_t {
	uint16_t              type;
	uint16_t              category;
	uint32_t              size;
	uint32_t              start_address_lsw;
	uint32_t              start_address_msw;
};

struct avcs_cmd_rsp_get_low_power_segments_info_t {
	uint32_t              num_segments;

	uint32_t              bandwidth;
	struct avcs_mem_segment_t mem_segment[OCMEM_MAX_CHUNKS];
};


int core_get_low_power_segments(
			struct avcs_cmd_rsp_get_low_power_segments_info_t **);
bool q6core_is_adsp_ready(void);

#define ADSP_CMD_SET_DOLBY_MANUFACTURER_ID 0x00012918

struct adsp_dolby_manufacturer_id {
	struct apr_hdr hdr;
	int manufacturer_id;
};

uint32_t core_set_dolby_manufacturer_id(int manufacturer_id);

#endif 
