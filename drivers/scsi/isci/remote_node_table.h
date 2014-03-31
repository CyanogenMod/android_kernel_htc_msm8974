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

#ifndef _SCIC_SDS_REMOTE_NODE_TABLE_H_
#define _SCIC_SDS_REMOTE_NODE_TABLE_H_

#include "isci.h"

#define SCIC_SDS_REMOTE_NODE_SETS_PER_BYTE 2

#define SCIC_SDS_REMOTE_NODE_SETS_PER_DWORD \
	(sizeof(u32) * SCIC_SDS_REMOTE_NODE_SETS_PER_BYTE)
#define SCIC_SDS_REMOTE_NODES_PER_BYTE	\
	(SCU_STP_REMOTE_NODE_COUNT * SCIC_SDS_REMOTE_NODE_SETS_PER_BYTE)

#define SCIC_SDS_REMOTE_NODES_PER_DWORD	\
	(sizeof(u32) * SCIC_SDS_REMOTE_NODES_PER_BYTE)

#define SCIC_SDS_REMOTE_NODES_BITS_PER_GROUP   4

#define SCIC_SDS_REMOTE_NODE_TABLE_INVALID_INDEX      (0xFFFFFFFF)
#define SCIC_SDS_REMOTE_NODE_TABLE_FULL_SLOT_VALUE    (0x07)
#define SCIC_SDS_REMOTE_NODE_TABLE_EMPTY_SLOT_VALUE   (0x00)

#define SCU_STP_REMOTE_NODE_COUNT        3

#define SCU_SSP_REMOTE_NODE_COUNT        1

#define SCU_SATA_REMOTE_NODE_COUNT       1

struct sci_remote_node_table {
	u16 available_nodes_array_size;

	u16 group_array_size;

	u32 available_remote_nodes[
		(SCI_MAX_REMOTE_DEVICES / SCIC_SDS_REMOTE_NODES_PER_DWORD)
		+ ((SCI_MAX_REMOTE_DEVICES % SCIC_SDS_REMOTE_NODES_PER_DWORD) != 0)];

	u32 remote_node_groups[
		SCU_STP_REMOTE_NODE_COUNT][
		(SCI_MAX_REMOTE_DEVICES / (32 * SCU_STP_REMOTE_NODE_COUNT))
		+ ((SCI_MAX_REMOTE_DEVICES % (32 * SCU_STP_REMOTE_NODE_COUNT)) != 0)];

};


void sci_remote_node_table_initialize(
	struct sci_remote_node_table *remote_node_table,
	u32 remote_node_entries);

u16 sci_remote_node_table_allocate_remote_node(
	struct sci_remote_node_table *remote_node_table,
	u32 remote_node_count);

void sci_remote_node_table_release_remote_node_index(
	struct sci_remote_node_table *remote_node_table,
	u32 remote_node_count,
	u16 remote_node_index);

#endif 
