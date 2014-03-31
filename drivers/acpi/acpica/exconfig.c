
/*
 * Copyright (C) 2000 - 2012, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include <acpi/acpi.h>
#include "accommon.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "actables.h"
#include "acdispat.h"
#include "acevents.h"

#define _COMPONENT          ACPI_EXECUTER
ACPI_MODULE_NAME("exconfig")

static acpi_status
acpi_ex_add_table(u32 table_index,
		  struct acpi_namespace_node *parent_node,
		  union acpi_operand_object **ddb_handle);

static acpi_status
acpi_ex_region_read(union acpi_operand_object *obj_desc,
		    u32 length, u8 *buffer);


static acpi_status
acpi_ex_add_table(u32 table_index,
		  struct acpi_namespace_node *parent_node,
		  union acpi_operand_object **ddb_handle)
{
	union acpi_operand_object *obj_desc;
	acpi_status status;
	acpi_owner_id owner_id;

	ACPI_FUNCTION_TRACE(ex_add_table);

	

	obj_desc = acpi_ut_create_internal_object(ACPI_TYPE_LOCAL_REFERENCE);
	if (!obj_desc) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	

	obj_desc->common.flags |= AOPOBJ_DATA_VALID;
	obj_desc->reference.class = ACPI_REFCLASS_TABLE;
	*ddb_handle = obj_desc;

	

	obj_desc->reference.value = table_index;

	

	status = acpi_ns_load_table(table_index, parent_node);
	if (ACPI_FAILURE(status)) {
		acpi_ut_remove_reference(obj_desc);
		*ddb_handle = NULL;
		return_ACPI_STATUS(status);
	}

	

	acpi_ex_exit_interpreter();
	acpi_ns_exec_module_code_list();
	acpi_ex_enter_interpreter();

	

	status = acpi_tb_get_owner_id(table_index, &owner_id);
	if (ACPI_SUCCESS(status)) {
		acpi_ev_update_gpes(owner_id);
	}

	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_ex_load_table_op(struct acpi_walk_state *walk_state,
		      union acpi_operand_object **return_desc)
{
	acpi_status status;
	union acpi_operand_object **operand = &walk_state->operands[0];
	struct acpi_namespace_node *parent_node;
	struct acpi_namespace_node *start_node;
	struct acpi_namespace_node *parameter_node = NULL;
	union acpi_operand_object *ddb_handle;
	struct acpi_table_header *table;
	u32 table_index;

	ACPI_FUNCTION_TRACE(ex_load_table_op);

	

	if ((operand[0]->string.length > ACPI_NAME_SIZE) ||
	    (operand[1]->string.length > ACPI_OEM_ID_SIZE) ||
	    (operand[2]->string.length > ACPI_OEM_TABLE_ID_SIZE)) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	

	status = acpi_tb_find_table(operand[0]->string.pointer,
				    operand[1]->string.pointer,
				    operand[2]->string.pointer, &table_index);
	if (ACPI_FAILURE(status)) {
		if (status != AE_NOT_FOUND) {
			return_ACPI_STATUS(status);
		}

		

		ddb_handle = acpi_ut_create_integer_object((u64) 0);
		if (!ddb_handle) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		*return_desc = ddb_handle;
		return_ACPI_STATUS(AE_OK);
	}

	

	start_node = walk_state->scope_info->scope.node;
	parent_node = acpi_gbl_root_node;

	

	if (operand[3]->string.length > 0) {
		status =
		    acpi_ns_get_node(start_node, operand[3]->string.pointer,
				     ACPI_NS_SEARCH_PARENT, &parent_node);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}
	}

	

	if (operand[4]->string.length > 0) {
		if ((operand[4]->string.pointer[0] != '\\') &&
		    (operand[4]->string.pointer[0] != '^')) {
			start_node = parent_node;
		}

		

		status =
		    acpi_ns_get_node(start_node, operand[4]->string.pointer,
				     ACPI_NS_SEARCH_PARENT, &parameter_node);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}
	}

	

	status = acpi_ex_add_table(table_index, parent_node, &ddb_handle);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	

	if (parameter_node) {

		

		status = acpi_ex_store(operand[5],
				       ACPI_CAST_PTR(union acpi_operand_object,
						     parameter_node),
				       walk_state);
		if (ACPI_FAILURE(status)) {
			(void)acpi_ex_unload_table(ddb_handle);

			acpi_ut_remove_reference(ddb_handle);
			return_ACPI_STATUS(status);
		}
	}

	status = acpi_get_table_by_index(table_index, &table);
	if (ACPI_SUCCESS(status)) {
		ACPI_INFO((AE_INFO, "Dynamic OEM Table Load:"));
		acpi_tb_print_table_header(0, table);
	}

	

	if (acpi_gbl_table_handler) {
		(void)acpi_gbl_table_handler(ACPI_TABLE_EVENT_LOAD, table,
					     acpi_gbl_table_handler_context);
	}

	*return_desc = ddb_handle;
	return_ACPI_STATUS(status);
}


static acpi_status
acpi_ex_region_read(union acpi_operand_object *obj_desc, u32 length, u8 *buffer)
{
	acpi_status status;
	u64 value;
	u32 region_offset = 0;
	u32 i;

	

	for (i = 0; i < length; i++) {
		status =
		    acpi_ev_address_space_dispatch(obj_desc, NULL, ACPI_READ,
						   region_offset, 8, &value);
		if (ACPI_FAILURE(status)) {
			return status;
		}

		*buffer = (u8)value;
		buffer++;
		region_offset++;
	}

	return AE_OK;
}


acpi_status
acpi_ex_load_op(union acpi_operand_object *obj_desc,
		union acpi_operand_object *target,
		struct acpi_walk_state *walk_state)
{
	union acpi_operand_object *ddb_handle;
	struct acpi_table_header *table;
	struct acpi_table_desc table_desc;
	u32 table_index;
	acpi_status status;
	u32 length;

	ACPI_FUNCTION_TRACE(ex_load_op);

	ACPI_MEMSET(&table_desc, 0, sizeof(struct acpi_table_desc));

	

	switch (obj_desc->common.type) {
	case ACPI_TYPE_REGION:

		ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
				  "Load table from Region %p\n", obj_desc));

		

		if (obj_desc->region.space_id != ACPI_ADR_SPACE_SYSTEM_MEMORY) {
			return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
		}

		if (!(obj_desc->common.flags & AOPOBJ_DATA_VALID)) {
			status = acpi_ds_get_region_arguments(obj_desc);
			if (ACPI_FAILURE(status)) {
				return_ACPI_STATUS(status);
			}
		}

		

		table = ACPI_ALLOCATE(sizeof(struct acpi_table_header));
		if (!table) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		status =
		    acpi_ex_region_read(obj_desc,
					sizeof(struct acpi_table_header),
					ACPI_CAST_PTR(u8, table));
		length = table->length;
		ACPI_FREE(table);

		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

		

		if (length < sizeof(struct acpi_table_header)) {
			return_ACPI_STATUS(AE_INVALID_TABLE_LENGTH);
		}


		

		table_desc.pointer = ACPI_ALLOCATE(length);
		if (!table_desc.pointer) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		

		status = acpi_ex_region_read(obj_desc, length,
					     ACPI_CAST_PTR(u8,
							   table_desc.pointer));
		if (ACPI_FAILURE(status)) {
			ACPI_FREE(table_desc.pointer);
			return_ACPI_STATUS(status);
		}

		table_desc.address = obj_desc->region.address;
		break;

	case ACPI_TYPE_BUFFER:	

		ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
				  "Load table from Buffer or Field %p\n",
				  obj_desc));

		

		if (obj_desc->buffer.length < sizeof(struct acpi_table_header)) {
			return_ACPI_STATUS(AE_INVALID_TABLE_LENGTH);
		}

		

		table =
		    ACPI_CAST_PTR(struct acpi_table_header,
				  obj_desc->buffer.pointer);
		length = table->length;

		

		if (length > obj_desc->buffer.length) {
			return_ACPI_STATUS(AE_AML_BUFFER_LIMIT);
		}
		if (length < sizeof(struct acpi_table_header)) {
			return_ACPI_STATUS(AE_INVALID_TABLE_LENGTH);
		}

		table_desc.pointer = ACPI_ALLOCATE(length);
		if (!table_desc.pointer) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		ACPI_MEMCPY(table_desc.pointer, table, length);
		table_desc.address = ACPI_TO_INTEGER(table_desc.pointer);
		break;

	default:
		return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
	}

	

	status = acpi_tb_verify_checksum(table_desc.pointer, length);
	if (ACPI_FAILURE(status)) {
		ACPI_FREE(table_desc.pointer);
		return_ACPI_STATUS(status);
	}

	

	table_desc.length = length;
	table_desc.flags = ACPI_TABLE_ORIGIN_ALLOCATED;

	

	status = acpi_tb_add_table(&table_desc, &table_index);
	if (ACPI_FAILURE(status)) {

		

		acpi_tb_delete_table(&table_desc);
		return_ACPI_STATUS(status);
	}

	status =
	    acpi_ex_add_table(table_index, acpi_gbl_root_node, &ddb_handle);
	if (ACPI_FAILURE(status)) {

		

		return_ACPI_STATUS(status);
	}

	

	status = acpi_ex_store(ddb_handle, target, walk_state);
	if (ACPI_FAILURE(status)) {
		(void)acpi_ex_unload_table(ddb_handle);

		

		acpi_ut_remove_reference(ddb_handle);
		return_ACPI_STATUS(status);
	}

	ACPI_INFO((AE_INFO, "Dynamic OEM Table Load:"));
	acpi_tb_print_table_header(0, table_desc.pointer);

	

	acpi_ut_remove_reference(ddb_handle);

	

	if (acpi_gbl_table_handler) {
		(void)acpi_gbl_table_handler(ACPI_TABLE_EVENT_LOAD,
					     table_desc.pointer,
					     acpi_gbl_table_handler_context);
	}

	return_ACPI_STATUS(status);
}


acpi_status acpi_ex_unload_table(union acpi_operand_object *ddb_handle)
{
	acpi_status status = AE_OK;
	union acpi_operand_object *table_desc = ddb_handle;
	u32 table_index;
	struct acpi_table_header *table;

	ACPI_FUNCTION_TRACE(ex_unload_table);

	if ((!ddb_handle) ||
	    (ACPI_GET_DESCRIPTOR_TYPE(ddb_handle) != ACPI_DESC_TYPE_OPERAND) ||
	    (ddb_handle->common.type != ACPI_TYPE_LOCAL_REFERENCE) ||
	    (!(ddb_handle->common.flags & AOPOBJ_DATA_VALID))) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	

	table_index = table_desc->reference.value;

	

	if (!acpi_tb_is_table_loaded(table_index)) {
		return_ACPI_STATUS(AE_NOT_EXIST);
	}

	

	if (acpi_gbl_table_handler) {
		status = acpi_get_table_by_index(table_index, &table);
		if (ACPI_SUCCESS(status)) {
			(void)acpi_gbl_table_handler(ACPI_TABLE_EVENT_UNLOAD,
						     table,
						     acpi_gbl_table_handler_context);
		}
	}

	

	status = acpi_tb_delete_namespace_by_owner(table_index);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	(void)acpi_tb_release_owner_id(table_index);
	acpi_tb_set_table_loaded_flag(table_index, FALSE);

	ddb_handle->common.flags &= ~AOPOBJ_DATA_VALID;
	return_ACPI_STATUS(AE_OK);
}
