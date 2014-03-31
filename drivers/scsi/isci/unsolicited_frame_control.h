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

#ifndef _SCIC_SDS_UNSOLICITED_FRAME_CONTROL_H_
#define _SCIC_SDS_UNSOLICITED_FRAME_CONTROL_H_

#include "isci.h"

#define SCU_UNSOLICITED_FRAME_HEADER_DATA_DWORDS 15

struct scu_unsolicited_frame_header {
	u32 iit_exists:1;

	u32 protocol_type:3;

	u32 is_address_frame:1;

	u32 connection_rate:4;

	u32 reserved:23;

	u32 data[SCU_UNSOLICITED_FRAME_HEADER_DATA_DWORDS];

};



enum unsolicited_frame_state {
	UNSOLICITED_FRAME_EMPTY,

	UNSOLICITED_FRAME_IN_USE,

	UNSOLICITED_FRAME_RELEASED,

	UNSOLICITED_FRAME_MAX_STATES
};

struct sci_unsolicited_frame {
	enum unsolicited_frame_state state;

	struct scu_unsolicited_frame_header *header;

	void *buffer;

};

struct sci_uf_header_array {
	struct scu_unsolicited_frame_header *array;

	dma_addr_t physical_address;

};

struct sci_uf_buffer_array {
	struct sci_unsolicited_frame array[SCU_MAX_UNSOLICITED_FRAMES];

	dma_addr_t physical_address;
};

struct sci_uf_address_table_array {
	u64 *array;

	dma_addr_t physical_address;

};

struct sci_unsolicited_frame_control {
	u32 get;

	struct sci_uf_header_array headers;

	struct sci_uf_buffer_array buffers;

	struct sci_uf_address_table_array address_table;

};

struct isci_host;

int sci_unsolicited_frame_control_construct(struct isci_host *ihost);

enum sci_status sci_unsolicited_frame_control_get_header(
	struct sci_unsolicited_frame_control *uf_control,
	u32 frame_index,
	void **frame_header);

enum sci_status sci_unsolicited_frame_control_get_buffer(
	struct sci_unsolicited_frame_control *uf_control,
	u32 frame_index,
	void **frame_buffer);

bool sci_unsolicited_frame_control_release_frame(
	struct sci_unsolicited_frame_control *uf_control,
	u32 frame_index);

#endif 
