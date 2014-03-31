
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
#include "acdispat.h"
#include "acnamesp.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_DISPATCHER
ACPI_MODULE_NAME("dsmthdat")

static void
acpi_ds_method_data_delete_value(u8 type,
				 u32 index, struct acpi_walk_state *walk_state);

static acpi_status
acpi_ds_method_data_set_value(u8 type,
			      u32 index,
			      union acpi_operand_object *object,
			      struct acpi_walk_state *walk_state);

#ifdef ACPI_OBSOLETE_FUNCTIONS
acpi_object_type
acpi_ds_method_data_get_type(u16 opcode,
			     u32 index, struct acpi_walk_state *walk_state);
#endif


void acpi_ds_method_data_init(struct acpi_walk_state *walk_state)
{
	u32 i;

	ACPI_FUNCTION_TRACE(ds_method_data_init);

	

	for (i = 0; i < ACPI_METHOD_NUM_ARGS; i++) {
		ACPI_MOVE_32_TO_32(&walk_state->arguments[i].name,
				   NAMEOF_ARG_NTE);
		walk_state->arguments[i].name.integer |= (i << 24);
		walk_state->arguments[i].descriptor_type = ACPI_DESC_TYPE_NAMED;
		walk_state->arguments[i].type = ACPI_TYPE_ANY;
		walk_state->arguments[i].flags = ANOBJ_METHOD_ARG;
	}

	

	for (i = 0; i < ACPI_METHOD_NUM_LOCALS; i++) {
		ACPI_MOVE_32_TO_32(&walk_state->local_variables[i].name,
				   NAMEOF_LOCAL_NTE);

		walk_state->local_variables[i].name.integer |= (i << 24);
		walk_state->local_variables[i].descriptor_type =
		    ACPI_DESC_TYPE_NAMED;
		walk_state->local_variables[i].type = ACPI_TYPE_ANY;
		walk_state->local_variables[i].flags = ANOBJ_METHOD_LOCAL;
	}

	return_VOID;
}


void acpi_ds_method_data_delete_all(struct acpi_walk_state *walk_state)
{
	u32 index;

	ACPI_FUNCTION_TRACE(ds_method_data_delete_all);

	

	for (index = 0; index < ACPI_METHOD_NUM_LOCALS; index++) {
		if (walk_state->local_variables[index].object) {
			ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "Deleting Local%u=%p\n",
					  index,
					  walk_state->local_variables[index].
					  object));

			

			acpi_ns_detach_object(&walk_state->
					      local_variables[index]);
		}
	}

	

	for (index = 0; index < ACPI_METHOD_NUM_ARGS; index++) {
		if (walk_state->arguments[index].object) {
			ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "Deleting Arg%u=%p\n",
					  index,
					  walk_state->arguments[index].object));

			

			acpi_ns_detach_object(&walk_state->arguments[index]);
		}
	}

	return_VOID;
}


acpi_status
acpi_ds_method_data_init_args(union acpi_operand_object **params,
			      u32 max_param_count,
			      struct acpi_walk_state *walk_state)
{
	acpi_status status;
	u32 index = 0;

	ACPI_FUNCTION_TRACE_PTR(ds_method_data_init_args, params);

	if (!params) {
		ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
				  "No param list passed to method\n"));
		return_ACPI_STATUS(AE_OK);
	}

	

	while ((index < ACPI_METHOD_NUM_ARGS) &&
	       (index < max_param_count) && params[index]) {
		status = acpi_ds_method_data_set_value(ACPI_REFCLASS_ARG, index,
						       params[index],
						       walk_state);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

		index++;
	}

	ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "%u args passed to method\n", index));
	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_ds_method_data_get_node(u8 type,
			     u32 index,
			     struct acpi_walk_state *walk_state,
			     struct acpi_namespace_node **node)
{
	ACPI_FUNCTION_TRACE(ds_method_data_get_node);

	switch (type) {
	case ACPI_REFCLASS_LOCAL:

		if (index > ACPI_METHOD_MAX_LOCAL) {
			ACPI_ERROR((AE_INFO,
				    "Local index %u is invalid (max %u)",
				    index, ACPI_METHOD_MAX_LOCAL));
			return_ACPI_STATUS(AE_AML_INVALID_INDEX);
		}

		

		*node = &walk_state->local_variables[index];
		break;

	case ACPI_REFCLASS_ARG:

		if (index > ACPI_METHOD_MAX_ARG) {
			ACPI_ERROR((AE_INFO,
				    "Arg index %u is invalid (max %u)",
				    index, ACPI_METHOD_MAX_ARG));
			return_ACPI_STATUS(AE_AML_INVALID_INDEX);
		}

		

		*node = &walk_state->arguments[index];
		break;

	default:
		ACPI_ERROR((AE_INFO, "Type %u is invalid", type));
		return_ACPI_STATUS(AE_TYPE);
	}

	return_ACPI_STATUS(AE_OK);
}


static acpi_status
acpi_ds_method_data_set_value(u8 type,
			      u32 index,
			      union acpi_operand_object *object,
			      struct acpi_walk_state *walk_state)
{
	acpi_status status;
	struct acpi_namespace_node *node;

	ACPI_FUNCTION_TRACE(ds_method_data_set_value);

	ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
			  "NewObj %p Type %2.2X, Refs=%u [%s]\n", object,
			  type, object->common.reference_count,
			  acpi_ut_get_type_name(object->common.type)));

	

	status = acpi_ds_method_data_get_node(type, index, walk_state, &node);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	acpi_ut_add_reference(object);

	

	node->object = object;
	return_ACPI_STATUS(status);
}


acpi_status
acpi_ds_method_data_get_value(u8 type,
			      u32 index,
			      struct acpi_walk_state *walk_state,
			      union acpi_operand_object **dest_desc)
{
	acpi_status status;
	struct acpi_namespace_node *node;
	union acpi_operand_object *object;

	ACPI_FUNCTION_TRACE(ds_method_data_get_value);

	

	if (!dest_desc) {
		ACPI_ERROR((AE_INFO, "Null object descriptor pointer"));
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	

	status = acpi_ds_method_data_get_node(type, index, walk_state, &node);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	

	object = node->object;

	

	if (!object) {

		

		if (acpi_gbl_enable_interpreter_slack) {
			object = acpi_ut_create_integer_object((u64) 0);
			if (!object) {
				return_ACPI_STATUS(AE_NO_MEMORY);
			}

			node->object = object;
		}

		

		else
			switch (type) {
			case ACPI_REFCLASS_ARG:

				ACPI_ERROR((AE_INFO,
					    "Uninitialized Arg[%u] at node %p",
					    index, node));

				return_ACPI_STATUS(AE_AML_UNINITIALIZED_ARG);

			case ACPI_REFCLASS_LOCAL:

				return_ACPI_STATUS(AE_AML_UNINITIALIZED_LOCAL);

			default:

				ACPI_ERROR((AE_INFO,
					    "Not a Arg/Local opcode: 0x%X",
					    type));
				return_ACPI_STATUS(AE_AML_INTERNAL);
			}
	}

	*dest_desc = object;
	acpi_ut_add_reference(object);

	return_ACPI_STATUS(AE_OK);
}


static void
acpi_ds_method_data_delete_value(u8 type,
				 u32 index, struct acpi_walk_state *walk_state)
{
	acpi_status status;
	struct acpi_namespace_node *node;
	union acpi_operand_object *object;

	ACPI_FUNCTION_TRACE(ds_method_data_delete_value);

	

	status = acpi_ds_method_data_get_node(type, index, walk_state, &node);
	if (ACPI_FAILURE(status)) {
		return_VOID;
	}

	

	object = acpi_ns_get_attached_object(node);

	node->object = NULL;

	if ((object) &&
	    (ACPI_GET_DESCRIPTOR_TYPE(object) == ACPI_DESC_TYPE_OPERAND)) {
		acpi_ut_remove_reference(object);
	}

	return_VOID;
}


acpi_status
acpi_ds_store_object_to_local(u8 type,
			      u32 index,
			      union acpi_operand_object *obj_desc,
			      struct acpi_walk_state *walk_state)
{
	acpi_status status;
	struct acpi_namespace_node *node;
	union acpi_operand_object *current_obj_desc;
	union acpi_operand_object *new_obj_desc;

	ACPI_FUNCTION_TRACE(ds_store_object_to_local);
	ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "Type=%2.2X Index=%u Obj=%p\n",
			  type, index, obj_desc));

	

	if (!obj_desc) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	

	status = acpi_ds_method_data_get_node(type, index, walk_state, &node);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	current_obj_desc = acpi_ns_get_attached_object(node);
	if (current_obj_desc == obj_desc) {
		ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "Obj=%p already installed!\n",
				  obj_desc));
		return_ACPI_STATUS(status);
	}

	new_obj_desc = obj_desc;
	if (obj_desc->common.reference_count > 1) {
		status =
		    acpi_ut_copy_iobject_to_iobject(obj_desc, &new_obj_desc,
						    walk_state);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}
	}

	if (current_obj_desc) {
		if (type == ACPI_REFCLASS_ARG) {
			if ((ACPI_GET_DESCRIPTOR_TYPE(current_obj_desc) ==
			     ACPI_DESC_TYPE_OPERAND)
			    && (current_obj_desc->common.type ==
				ACPI_TYPE_LOCAL_REFERENCE)
			    && (current_obj_desc->reference.class ==
				ACPI_REFCLASS_REFOF)) {
				ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
						  "Arg (%p) is an ObjRef(Node), storing in node %p\n",
						  new_obj_desc,
						  current_obj_desc));

				status =
				    acpi_ex_store_object_to_node(new_obj_desc,
								 current_obj_desc->
								 reference.
								 object,
								 walk_state,
								 ACPI_NO_IMPLICIT_CONVERSION);

				

				if (new_obj_desc != obj_desc) {
					acpi_ut_remove_reference(new_obj_desc);
				}
				return_ACPI_STATUS(status);
			}
		}

		

		acpi_ds_method_data_delete_value(type, index, walk_state);
	}

	status =
	    acpi_ds_method_data_set_value(type, index, new_obj_desc,
					  walk_state);

	

	if (new_obj_desc != obj_desc) {
		acpi_ut_remove_reference(new_obj_desc);
	}

	return_ACPI_STATUS(status);
}

#ifdef ACPI_OBSOLETE_FUNCTIONS

acpi_object_type
acpi_ds_method_data_get_type(u16 opcode,
			     u32 index, struct acpi_walk_state *walk_state)
{
	acpi_status status;
	struct acpi_namespace_node *node;
	union acpi_operand_object *object;

	ACPI_FUNCTION_TRACE(ds_method_data_get_type);

	

	status = acpi_ds_method_data_get_node(opcode, index, walk_state, &node);
	if (ACPI_FAILURE(status)) {
		return_VALUE((ACPI_TYPE_NOT_FOUND));
	}

	

	object = acpi_ns_get_attached_object(node);
	if (!object) {

		

		return_VALUE(ACPI_TYPE_ANY);
	}

	

	return_VALUE(object->type);
}
#endif
