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

#include "remote_node_table.h"
#include "remote_node_context.h"

static u32 sci_remote_node_table_get_group_index(
	struct sci_remote_node_table *remote_node_table,
	u32 group_table_index)
{
	u32 dword_index;
	u32 *group_table;
	u32 bit_index;

	group_table = remote_node_table->remote_node_groups[group_table_index];

	for (dword_index = 0; dword_index < remote_node_table->group_array_size; dword_index++) {
		if (group_table[dword_index] != 0) {
			for (bit_index = 0; bit_index < 32; bit_index++) {
				if ((group_table[dword_index] & (1 << bit_index)) != 0) {
					return (dword_index * 32) + bit_index;
				}
			}
		}
	}

	return SCIC_SDS_REMOTE_NODE_TABLE_INVALID_INDEX;
}

static void sci_remote_node_table_clear_group_index(
	struct sci_remote_node_table *remote_node_table,
	u32 group_table_index,
	u32 group_index)
{
	u32 dword_index;
	u32 bit_index;
	u32 *group_table;

	BUG_ON(group_table_index >= SCU_STP_REMOTE_NODE_COUNT);
	BUG_ON(group_index >= (u32)(remote_node_table->group_array_size * 32));

	dword_index = group_index / 32;
	bit_index   = group_index % 32;
	group_table = remote_node_table->remote_node_groups[group_table_index];

	group_table[dword_index] = group_table[dword_index] & ~(1 << bit_index);
}

static void sci_remote_node_table_set_group_index(
	struct sci_remote_node_table *remote_node_table,
	u32 group_table_index,
	u32 group_index)
{
	u32 dword_index;
	u32 bit_index;
	u32 *group_table;

	BUG_ON(group_table_index >= SCU_STP_REMOTE_NODE_COUNT);
	BUG_ON(group_index >= (u32)(remote_node_table->group_array_size * 32));

	dword_index = group_index / 32;
	bit_index   = group_index % 32;
	group_table = remote_node_table->remote_node_groups[group_table_index];

	group_table[dword_index] = group_table[dword_index] | (1 << bit_index);
}

static void sci_remote_node_table_set_node_index(
	struct sci_remote_node_table *remote_node_table,
	u32 remote_node_index)
{
	u32 dword_location;
	u32 dword_remainder;
	u32 slot_normalized;
	u32 slot_position;

	BUG_ON(
		(remote_node_table->available_nodes_array_size * SCIC_SDS_REMOTE_NODE_SETS_PER_DWORD)
		<= (remote_node_index / SCU_STP_REMOTE_NODE_COUNT)
		);

	dword_location  = remote_node_index / SCIC_SDS_REMOTE_NODES_PER_DWORD;
	dword_remainder = remote_node_index % SCIC_SDS_REMOTE_NODES_PER_DWORD;
	slot_normalized = (dword_remainder / SCU_STP_REMOTE_NODE_COUNT) * sizeof(u32);
	slot_position   = remote_node_index % SCU_STP_REMOTE_NODE_COUNT;

	remote_node_table->available_remote_nodes[dword_location] |=
		1 << (slot_normalized + slot_position);
}

static void sci_remote_node_table_clear_node_index(
	struct sci_remote_node_table *remote_node_table,
	u32 remote_node_index)
{
	u32 dword_location;
	u32 dword_remainder;
	u32 slot_position;
	u32 slot_normalized;

	BUG_ON(
		(remote_node_table->available_nodes_array_size * SCIC_SDS_REMOTE_NODE_SETS_PER_DWORD)
		<= (remote_node_index / SCU_STP_REMOTE_NODE_COUNT)
		);

	dword_location  = remote_node_index / SCIC_SDS_REMOTE_NODES_PER_DWORD;
	dword_remainder = remote_node_index % SCIC_SDS_REMOTE_NODES_PER_DWORD;
	slot_normalized = (dword_remainder / SCU_STP_REMOTE_NODE_COUNT) * sizeof(u32);
	slot_position   = remote_node_index % SCU_STP_REMOTE_NODE_COUNT;

	remote_node_table->available_remote_nodes[dword_location] &=
		~(1 << (slot_normalized + slot_position));
}

static void sci_remote_node_table_clear_group(
	struct sci_remote_node_table *remote_node_table,
	u32 group_index)
{
	u32 dword_location;
	u32 dword_remainder;
	u32 dword_value;

	BUG_ON(
		(remote_node_table->available_nodes_array_size * SCIC_SDS_REMOTE_NODE_SETS_PER_DWORD)
		<= (group_index / SCU_STP_REMOTE_NODE_COUNT)
		);

	dword_location  = group_index / SCIC_SDS_REMOTE_NODE_SETS_PER_DWORD;
	dword_remainder = group_index % SCIC_SDS_REMOTE_NODE_SETS_PER_DWORD;

	dword_value = remote_node_table->available_remote_nodes[dword_location];
	dword_value &= ~(SCIC_SDS_REMOTE_NODE_TABLE_FULL_SLOT_VALUE << (dword_remainder * 4));
	remote_node_table->available_remote_nodes[dword_location] = dword_value;
}

static void sci_remote_node_table_set_group(
	struct sci_remote_node_table *remote_node_table,
	u32 group_index)
{
	u32 dword_location;
	u32 dword_remainder;
	u32 dword_value;

	BUG_ON(
		(remote_node_table->available_nodes_array_size * SCIC_SDS_REMOTE_NODE_SETS_PER_DWORD)
		<= (group_index / SCU_STP_REMOTE_NODE_COUNT)
		);

	dword_location  = group_index / SCIC_SDS_REMOTE_NODE_SETS_PER_DWORD;
	dword_remainder = group_index % SCIC_SDS_REMOTE_NODE_SETS_PER_DWORD;

	dword_value = remote_node_table->available_remote_nodes[dword_location];
	dword_value |= (SCIC_SDS_REMOTE_NODE_TABLE_FULL_SLOT_VALUE << (dword_remainder * 4));
	remote_node_table->available_remote_nodes[dword_location] = dword_value;
}

static u8 sci_remote_node_table_get_group_value(
	struct sci_remote_node_table *remote_node_table,
	u32 group_index)
{
	u32 dword_location;
	u32 dword_remainder;
	u32 dword_value;

	dword_location  = group_index / SCIC_SDS_REMOTE_NODE_SETS_PER_DWORD;
	dword_remainder = group_index % SCIC_SDS_REMOTE_NODE_SETS_PER_DWORD;

	dword_value = remote_node_table->available_remote_nodes[dword_location];
	dword_value &= (SCIC_SDS_REMOTE_NODE_TABLE_FULL_SLOT_VALUE << (dword_remainder * 4));
	dword_value = dword_value >> (dword_remainder * 4);

	return (u8)dword_value;
}

void sci_remote_node_table_initialize(
	struct sci_remote_node_table *remote_node_table,
	u32 remote_node_entries)
{
	u32 index;

	memset(
		remote_node_table->available_remote_nodes,
		0x00,
		sizeof(remote_node_table->available_remote_nodes)
		);

	memset(
		remote_node_table->remote_node_groups,
		0x00,
		sizeof(remote_node_table->remote_node_groups)
		);

	
	remote_node_table->available_nodes_array_size = (u16)
							(remote_node_entries / SCIC_SDS_REMOTE_NODES_PER_DWORD)
							+ ((remote_node_entries % SCIC_SDS_REMOTE_NODES_PER_DWORD) != 0);


	
	for (index = 0; index < remote_node_entries; index++) {
		sci_remote_node_table_set_node_index(remote_node_table, index);
	}

	remote_node_table->group_array_size = (u16)
					      (remote_node_entries / (SCU_STP_REMOTE_NODE_COUNT * 32))
					      + ((remote_node_entries % (SCU_STP_REMOTE_NODE_COUNT * 32)) != 0);

	for (index = 0; index < (remote_node_entries / SCU_STP_REMOTE_NODE_COUNT); index++) {
		sci_remote_node_table_set_group_index(remote_node_table, 2, index);
	}

	
	if ((remote_node_entries % SCU_STP_REMOTE_NODE_COUNT) == 2) {
		sci_remote_node_table_set_group_index(remote_node_table, 1, index);
	} else if ((remote_node_entries % SCU_STP_REMOTE_NODE_COUNT) == 1) {
		sci_remote_node_table_set_group_index(remote_node_table, 0, index);
	}
}

static u16 sci_remote_node_table_allocate_single_remote_node(
	struct sci_remote_node_table *remote_node_table,
	u32 group_table_index)
{
	u8 index;
	u8 group_value;
	u32 group_index;
	u16 remote_node_index = SCIC_SDS_REMOTE_NODE_CONTEXT_INVALID_INDEX;

	group_index = sci_remote_node_table_get_group_index(
		remote_node_table, group_table_index);

	
	if (group_index != SCIC_SDS_REMOTE_NODE_TABLE_INVALID_INDEX) {
		group_value = sci_remote_node_table_get_group_value(
			remote_node_table, group_index);

		for (index = 0; index < SCU_STP_REMOTE_NODE_COUNT; index++) {
			if (((1 << index) & group_value) != 0) {
				
				remote_node_index = (u16)(group_index * SCU_STP_REMOTE_NODE_COUNT
							  + index);

				sci_remote_node_table_clear_group_index(
					remote_node_table, group_table_index, group_index
					);

				sci_remote_node_table_clear_node_index(
					remote_node_table, remote_node_index
					);

				if (group_table_index > 0) {
					sci_remote_node_table_set_group_index(
						remote_node_table, group_table_index - 1, group_index
						);
				}

				break;
			}
		}
	}

	return remote_node_index;
}

static u16 sci_remote_node_table_allocate_triple_remote_node(
	struct sci_remote_node_table *remote_node_table,
	u32 group_table_index)
{
	u32 group_index;
	u16 remote_node_index = SCIC_SDS_REMOTE_NODE_CONTEXT_INVALID_INDEX;

	group_index = sci_remote_node_table_get_group_index(
		remote_node_table, group_table_index);

	if (group_index != SCIC_SDS_REMOTE_NODE_TABLE_INVALID_INDEX) {
		remote_node_index = (u16)group_index * SCU_STP_REMOTE_NODE_COUNT;

		sci_remote_node_table_clear_group_index(
			remote_node_table, group_table_index, group_index
			);

		sci_remote_node_table_clear_group(
			remote_node_table, group_index
			);
	}

	return remote_node_index;
}

u16 sci_remote_node_table_allocate_remote_node(
	struct sci_remote_node_table *remote_node_table,
	u32 remote_node_count)
{
	u16 remote_node_index = SCIC_SDS_REMOTE_NODE_CONTEXT_INVALID_INDEX;

	if (remote_node_count == SCU_SSP_REMOTE_NODE_COUNT) {
		remote_node_index =
			sci_remote_node_table_allocate_single_remote_node(
				remote_node_table, 0);

		if (remote_node_index == SCIC_SDS_REMOTE_NODE_CONTEXT_INVALID_INDEX) {
			remote_node_index =
				sci_remote_node_table_allocate_single_remote_node(
					remote_node_table, 1);
		}

		if (remote_node_index == SCIC_SDS_REMOTE_NODE_CONTEXT_INVALID_INDEX) {
			remote_node_index =
				sci_remote_node_table_allocate_single_remote_node(
					remote_node_table, 2);
		}
	} else if (remote_node_count == SCU_STP_REMOTE_NODE_COUNT) {
		remote_node_index =
			sci_remote_node_table_allocate_triple_remote_node(
				remote_node_table, 2);
	}

	return remote_node_index;
}

static void sci_remote_node_table_release_single_remote_node(
	struct sci_remote_node_table *remote_node_table,
	u16 remote_node_index)
{
	u32 group_index;
	u8 group_value;

	group_index = remote_node_index / SCU_STP_REMOTE_NODE_COUNT;

	group_value = sci_remote_node_table_get_group_value(remote_node_table, group_index);

	BUG_ON(group_value == SCIC_SDS_REMOTE_NODE_TABLE_FULL_SLOT_VALUE);

	if (group_value == 0x00) {
		sci_remote_node_table_set_group_index(remote_node_table, 0, group_index);
	} else if ((group_value & (group_value - 1)) == 0) {
		sci_remote_node_table_clear_group_index(remote_node_table, 0, group_index);
		sci_remote_node_table_set_group_index(remote_node_table, 1, group_index);
	} else {
		sci_remote_node_table_clear_group_index(remote_node_table, 1, group_index);
		sci_remote_node_table_set_group_index(remote_node_table, 2, group_index);
	}

	sci_remote_node_table_set_node_index(remote_node_table, remote_node_index);
}

static void sci_remote_node_table_release_triple_remote_node(
	struct sci_remote_node_table *remote_node_table,
	u16 remote_node_index)
{
	u32 group_index;

	group_index = remote_node_index / SCU_STP_REMOTE_NODE_COUNT;

	sci_remote_node_table_set_group_index(
		remote_node_table, 2, group_index
		);

	sci_remote_node_table_set_group(remote_node_table, group_index);
}

void sci_remote_node_table_release_remote_node_index(
	struct sci_remote_node_table *remote_node_table,
	u32 remote_node_count,
	u16 remote_node_index)
{
	if (remote_node_count == SCU_SSP_REMOTE_NODE_COUNT) {
		sci_remote_node_table_release_single_remote_node(
			remote_node_table, remote_node_index);
	} else if (remote_node_count == SCU_STP_REMOTE_NODE_COUNT) {
		sci_remote_node_table_release_triple_remote_node(
			remote_node_table, remote_node_index);
	}
}

