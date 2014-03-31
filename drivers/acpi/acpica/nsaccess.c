
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
#include "amlcode.h"
#include "acnamesp.h"
#include "acdispat.h"

#define _COMPONENT          ACPI_NAMESPACE
ACPI_MODULE_NAME("nsaccess")

acpi_status acpi_ns_root_initialize(void)
{
	acpi_status status;
	const struct acpi_predefined_names *init_val = NULL;
	struct acpi_namespace_node *new_node;
	union acpi_operand_object *obj_desc;
	acpi_string val = NULL;

	ACPI_FUNCTION_TRACE(ns_root_initialize);

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	if (acpi_gbl_root_node) {
		status = AE_OK;
		goto unlock_and_exit;
	}

	acpi_gbl_root_node = &acpi_gbl_root_node_struct;

	

	ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			  "Entering predefined entries into namespace\n"));

	for (init_val = acpi_gbl_pre_defined_names; init_val->name; init_val++) {

		

		if (!ACPI_STRCMP(init_val->name, "_OSI")
		    && !acpi_gbl_create_osi_method) {
			continue;
		}

		status = acpi_ns_lookup(NULL, init_val->name, init_val->type,
					ACPI_IMODE_LOAD_PASS2,
					ACPI_NS_NO_UPSEARCH, NULL, &new_node);

		if (ACPI_FAILURE(status) || (!new_node)) {	
			ACPI_EXCEPTION((AE_INFO, status,
					"Could not create predefined name %s",
					init_val->name));
		}

		if (init_val->val) {
			status = acpi_os_predefined_override(init_val, &val);
			if (ACPI_FAILURE(status)) {
				ACPI_ERROR((AE_INFO,
					    "Could not override predefined %s",
					    init_val->name));
			}

			if (!val) {
				val = init_val->val;
			}

			obj_desc =
			    acpi_ut_create_internal_object(init_val->type);
			if (!obj_desc) {
				status = AE_NO_MEMORY;
				goto unlock_and_exit;
			}

			switch (init_val->type) {
			case ACPI_TYPE_METHOD:
				obj_desc->method.param_count =
				    (u8) ACPI_TO_INTEGER(val);
				obj_desc->common.flags |= AOPOBJ_DATA_VALID;

#if defined (ACPI_ASL_COMPILER)

				

				new_node->value = obj_desc->method.param_count;
#else
				

				obj_desc->method.info_flags =
				    ACPI_METHOD_INTERNAL_ONLY;
				obj_desc->method.dispatch.implementation =
				    acpi_ut_osi_implementation;
#endif
				break;

			case ACPI_TYPE_INTEGER:

				obj_desc->integer.value = ACPI_TO_INTEGER(val);
				break;

			case ACPI_TYPE_STRING:

				

				obj_desc->string.length =
				    (u32) ACPI_STRLEN(val);
				obj_desc->string.pointer = val;
				obj_desc->common.flags |= AOPOBJ_STATIC_POINTER;
				break;

			case ACPI_TYPE_MUTEX:

				obj_desc->mutex.node = new_node;
				obj_desc->mutex.sync_level =
				    (u8) (ACPI_TO_INTEGER(val) - 1);

				

				status =
				    acpi_os_create_mutex(&obj_desc->mutex.
							 os_mutex);
				if (ACPI_FAILURE(status)) {
					acpi_ut_remove_reference(obj_desc);
					goto unlock_and_exit;
				}

				

				if (ACPI_STRCMP(init_val->name, "_GL_") == 0) {
					acpi_gbl_global_lock_mutex = obj_desc;

					

					status =
					    acpi_os_create_semaphore(1, 0,
								     &acpi_gbl_global_lock_semaphore);
					if (ACPI_FAILURE(status)) {
						acpi_ut_remove_reference
						    (obj_desc);
						goto unlock_and_exit;
					}
				}
				break;

			default:

				ACPI_ERROR((AE_INFO,
					    "Unsupported initial type value 0x%X",
					    init_val->type));
				acpi_ut_remove_reference(obj_desc);
				obj_desc = NULL;
				continue;
			}

			

			status = acpi_ns_attach_object(new_node, obj_desc,
						       obj_desc->common.type);

			

			acpi_ut_remove_reference(obj_desc);
		}
	}

      unlock_and_exit:
	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);

	

	if (ACPI_SUCCESS(status)) {
		status = acpi_ns_get_node(NULL, "\\_GPE", ACPI_NS_NO_UPSEARCH,
					  &acpi_gbl_fadt_gpe_device);
	}

	return_ACPI_STATUS(status);
}


acpi_status
acpi_ns_lookup(union acpi_generic_state *scope_info,
	       char *pathname,
	       acpi_object_type type,
	       acpi_interpreter_mode interpreter_mode,
	       u32 flags,
	       struct acpi_walk_state *walk_state,
	       struct acpi_namespace_node **return_node)
{
	acpi_status status;
	char *path = pathname;
	struct acpi_namespace_node *prefix_node;
	struct acpi_namespace_node *current_node = NULL;
	struct acpi_namespace_node *this_node = NULL;
	u32 num_segments;
	u32 num_carats;
	acpi_name simple_name;
	acpi_object_type type_to_check_for;
	acpi_object_type this_search_type;
	u32 search_parent_flag = ACPI_NS_SEARCH_PARENT;
	u32 local_flags;

	ACPI_FUNCTION_TRACE(ns_lookup);

	if (!return_node) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	local_flags = flags & ~(ACPI_NS_ERROR_IF_FOUND | ACPI_NS_SEARCH_PARENT);
	*return_node = ACPI_ENTRY_NOT_FOUND;
	acpi_gbl_ns_lookup_count++;

	if (!acpi_gbl_root_node) {
		return_ACPI_STATUS(AE_NO_NAMESPACE);
	}

	

	if ((!scope_info) || (!scope_info->scope.node)) {
		ACPI_DEBUG_PRINT((ACPI_DB_NAMES,
				  "Null scope prefix, using root node (%p)\n",
				  acpi_gbl_root_node));

		prefix_node = acpi_gbl_root_node;
	} else {
		prefix_node = scope_info->scope.node;
		if (ACPI_GET_DESCRIPTOR_TYPE(prefix_node) !=
		    ACPI_DESC_TYPE_NAMED) {
			ACPI_ERROR((AE_INFO, "%p is not a namespace node [%s]",
				    prefix_node,
				    acpi_ut_get_descriptor_name(prefix_node)));
			return_ACPI_STATUS(AE_AML_INTERNAL);
		}

		if (!(flags & ACPI_NS_PREFIX_IS_SCOPE)) {
			while (!acpi_ns_opens_scope(prefix_node->type) &&
			       prefix_node->type != ACPI_TYPE_ANY) {
				prefix_node = prefix_node->parent;
			}
		}
	}

	

	type_to_check_for = type;

	if (!pathname) {

		

		num_segments = 0;
		this_node = acpi_gbl_root_node;
		path = "";

		ACPI_DEBUG_PRINT((ACPI_DB_NAMES,
				  "Null Pathname (Zero segments), Flags=%X\n",
				  flags));
	} else {
		if (*path == (u8) AML_ROOT_PREFIX) {

			

			this_node = acpi_gbl_root_node;
			search_parent_flag = ACPI_NS_NO_UPSEARCH;

			

			path++;

			ACPI_DEBUG_PRINT((ACPI_DB_NAMES,
					  "Path is absolute from root [%p]\n",
					  this_node));
		} else {
			

			ACPI_DEBUG_PRINT((ACPI_DB_NAMES,
					  "Searching relative to prefix scope [%4.4s] (%p)\n",
					  acpi_ut_get_node_name(prefix_node),
					  prefix_node));

			this_node = prefix_node;
			num_carats = 0;
			while (*path == (u8) AML_PARENT_PREFIX) {

				

				search_parent_flag = ACPI_NS_NO_UPSEARCH;

				path++;

				

				num_carats++;
				this_node = this_node->parent;
				if (!this_node) {

					

					ACPI_ERROR((AE_INFO,
						    "ACPI path has too many parent prefixes (^) "
						    "- reached beyond root node"));
					return_ACPI_STATUS(AE_NOT_FOUND);
				}
			}

			if (search_parent_flag == ACPI_NS_NO_UPSEARCH) {
				ACPI_DEBUG_PRINT((ACPI_DB_NAMES,
						  "Search scope is [%4.4s], path has %u carat(s)\n",
						  acpi_ut_get_node_name
						  (this_node), num_carats));
			}
		}

		switch (*path) {
		case 0:
			num_segments = 0;
			type = this_node->type;

			ACPI_DEBUG_PRINT((ACPI_DB_NAMES,
					  "Prefix-only Pathname (Zero name segments), Flags=%X\n",
					  flags));
			break;

		case AML_DUAL_NAME_PREFIX:

			

			search_parent_flag = ACPI_NS_NO_UPSEARCH;

			

			num_segments = 2;
			path++;

			ACPI_DEBUG_PRINT((ACPI_DB_NAMES,
					  "Dual Pathname (2 segments, Flags=%X)\n",
					  flags));
			break;

		case AML_MULTI_NAME_PREFIX_OP:

			

			search_parent_flag = ACPI_NS_NO_UPSEARCH;

			

			path++;
			num_segments = (u32) (u8) * path;
			path++;

			ACPI_DEBUG_PRINT((ACPI_DB_NAMES,
					  "Multi Pathname (%u Segments, Flags=%X)\n",
					  num_segments, flags));
			break;

		default:
			num_segments = 1;

			ACPI_DEBUG_PRINT((ACPI_DB_NAMES,
					  "Simple Pathname (1 segment, Flags=%X)\n",
					  flags));
			break;
		}

		ACPI_DEBUG_EXEC(acpi_ns_print_pathname(num_segments, path));
	}

	this_search_type = ACPI_TYPE_ANY;
	current_node = this_node;
	while (num_segments && current_node) {
		num_segments--;
		if (!num_segments) {

			

			this_search_type = type;

			if ((search_parent_flag != ACPI_NS_NO_UPSEARCH) &&
			    (flags & ACPI_NS_SEARCH_PARENT)) {
				local_flags |= ACPI_NS_SEARCH_PARENT;
			}

			

			if (flags & ACPI_NS_ERROR_IF_FOUND) {
				local_flags |= ACPI_NS_ERROR_IF_FOUND;
			}
		}

		

		ACPI_MOVE_32_TO_32(&simple_name, path);

		

		status =
		    acpi_ns_search_and_enter(simple_name, walk_state,
					     current_node, interpreter_mode,
					     this_search_type, local_flags,
					     &this_node);
		if (ACPI_FAILURE(status)) {
			if (status == AE_NOT_FOUND) {

				

				ACPI_DEBUG_PRINT((ACPI_DB_NAMES,
						  "Name [%4.4s] not found in scope [%4.4s] %p\n",
						  (char *)&simple_name,
						  (char *)&current_node->name,
						  current_node));
			}

			*return_node = this_node;
			return_ACPI_STATUS(status);
		}

		

		if (num_segments > 0) {
			if (this_node->type == ACPI_TYPE_LOCAL_ALIAS) {
				if (!this_node->object) {
					return_ACPI_STATUS(AE_NOT_EXIST);
				}

				if (acpi_ns_opens_scope
				    (((struct acpi_namespace_node *)
				      this_node->object)->type)) {
					this_node =
					    (struct acpi_namespace_node *)
					    this_node->object;
				}
			}
		}

		

		else {
			if ((type_to_check_for != ACPI_TYPE_ANY) &&
			    (type_to_check_for != ACPI_TYPE_LOCAL_ALIAS) &&
			    (type_to_check_for != ACPI_TYPE_LOCAL_METHOD_ALIAS)
			    && (type_to_check_for != ACPI_TYPE_LOCAL_SCOPE)
			    && (this_node->type != ACPI_TYPE_ANY)
			    && (this_node->type != type_to_check_for)) {

				

				ACPI_WARNING((AE_INFO,
					      "NsLookup: Type mismatch on %4.4s (%s), searching for (%s)",
					      ACPI_CAST_PTR(char, &simple_name),
					      acpi_ut_get_type_name(this_node->
								    type),
					      acpi_ut_get_type_name
					      (type_to_check_for)));
			}

			if (type == ACPI_TYPE_ANY) {
				type = this_node->type;
			}
		}

		

		path += ACPI_NAME_SIZE;
		current_node = this_node;
	}

	

	if (!(flags & ACPI_NS_DONT_OPEN_SCOPE) && (walk_state)) {
		if (acpi_ns_opens_scope(type)) {
			status =
			    acpi_ds_scope_stack_push(this_node, type,
						     walk_state);
			if (ACPI_FAILURE(status)) {
				return_ACPI_STATUS(status);
			}
		}
	}

	*return_node = this_node;
	return_ACPI_STATUS(AE_OK);
}
