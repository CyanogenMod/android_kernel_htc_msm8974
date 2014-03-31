
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
#include "acnamesp.h"

#ifdef ACPI_ASL_COMPILER
#include "amlcode.h"
#endif

#define _COMPONENT          ACPI_NAMESPACE
ACPI_MODULE_NAME("nssearch")

static acpi_status
acpi_ns_search_parent_tree(u32 target_name,
			   struct acpi_namespace_node *node,
			   acpi_object_type type,
			   struct acpi_namespace_node **return_node);


acpi_status
acpi_ns_search_one_scope(u32 target_name,
			 struct acpi_namespace_node *parent_node,
			 acpi_object_type type,
			 struct acpi_namespace_node **return_node)
{
	struct acpi_namespace_node *node;

	ACPI_FUNCTION_TRACE(ns_search_one_scope);

#ifdef ACPI_DEBUG_OUTPUT
	if (ACPI_LV_NAMES & acpi_dbg_level) {
		char *scope_name;

		scope_name = acpi_ns_get_external_pathname(parent_node);
		if (scope_name) {
			ACPI_DEBUG_PRINT((ACPI_DB_NAMES,
					  "Searching %s (%p) For [%4.4s] (%s)\n",
					  scope_name, parent_node,
					  ACPI_CAST_PTR(char, &target_name),
					  acpi_ut_get_type_name(type)));

			ACPI_FREE(scope_name);
		}
	}
#endif

	node = parent_node->child;
	while (node) {

		

		if (node->name.integer == target_name) {

			

			if (acpi_ns_get_type(node) ==
			    ACPI_TYPE_LOCAL_METHOD_ALIAS) {
				node =
				    ACPI_CAST_PTR(struct acpi_namespace_node,
						  node->object);
			}

			

			ACPI_DEBUG_PRINT((ACPI_DB_NAMES,
					  "Name [%4.4s] (%s) %p found in scope [%4.4s] %p\n",
					  ACPI_CAST_PTR(char, &target_name),
					  acpi_ut_get_type_name(node->type),
					  node,
					  acpi_ut_get_node_name(parent_node),
					  parent_node));

			*return_node = node;
			return_ACPI_STATUS(AE_OK);
		}

		

		node = node->peer;
	}

	

	ACPI_DEBUG_PRINT((ACPI_DB_NAMES,
			  "Name [%4.4s] (%s) not found in search in scope [%4.4s] "
			  "%p first child %p\n",
			  ACPI_CAST_PTR(char, &target_name),
			  acpi_ut_get_type_name(type),
			  acpi_ut_get_node_name(parent_node), parent_node,
			  parent_node->child));

	return_ACPI_STATUS(AE_NOT_FOUND);
}


static acpi_status
acpi_ns_search_parent_tree(u32 target_name,
			   struct acpi_namespace_node *node,
			   acpi_object_type type,
			   struct acpi_namespace_node **return_node)
{
	acpi_status status;
	struct acpi_namespace_node *parent_node;

	ACPI_FUNCTION_TRACE(ns_search_parent_tree);

	parent_node = node->parent;

	if (!parent_node) {
		ACPI_DEBUG_PRINT((ACPI_DB_NAMES, "[%4.4s] has no parent\n",
				  ACPI_CAST_PTR(char, &target_name)));
		return_ACPI_STATUS(AE_NOT_FOUND);
	}

	if (acpi_ns_local(type)) {
		ACPI_DEBUG_PRINT((ACPI_DB_NAMES,
				  "[%4.4s] type [%s] must be local to this scope (no parent search)\n",
				  ACPI_CAST_PTR(char, &target_name),
				  acpi_ut_get_type_name(type)));
		return_ACPI_STATUS(AE_NOT_FOUND);
	}

	

	ACPI_DEBUG_PRINT((ACPI_DB_NAMES,
			  "Searching parent [%4.4s] for [%4.4s]\n",
			  acpi_ut_get_node_name(parent_node),
			  ACPI_CAST_PTR(char, &target_name)));

	

	while (parent_node) {
		status =
		    acpi_ns_search_one_scope(target_name, parent_node,
					     ACPI_TYPE_ANY, return_node);
		if (ACPI_SUCCESS(status)) {
			return_ACPI_STATUS(status);
		}

		

		parent_node = parent_node->parent;
	}

	

	return_ACPI_STATUS(AE_NOT_FOUND);
}


acpi_status
acpi_ns_search_and_enter(u32 target_name,
			 struct acpi_walk_state *walk_state,
			 struct acpi_namespace_node *node,
			 acpi_interpreter_mode interpreter_mode,
			 acpi_object_type type,
			 u32 flags, struct acpi_namespace_node **return_node)
{
	acpi_status status;
	struct acpi_namespace_node *new_node;

	ACPI_FUNCTION_TRACE(ns_search_and_enter);

	

	if (!node || !target_name || !return_node) {
		ACPI_ERROR((AE_INFO,
			    "Null parameter: Node %p Name 0x%X ReturnNode %p",
			    node, target_name, return_node));
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	if (!acpi_ut_valid_acpi_name(target_name)) {
		target_name =
		    acpi_ut_repair_name(ACPI_CAST_PTR(char, &target_name));

		

		if (!acpi_gbl_enable_interpreter_slack) {
			ACPI_WARNING((AE_INFO,
				      "Found bad character(s) in name, repaired: [%4.4s]\n",
				      ACPI_CAST_PTR(char, &target_name)));
		} else {
			ACPI_DEBUG_PRINT((ACPI_DB_INFO,
					  "Found bad character(s) in name, repaired: [%4.4s]\n",
					  ACPI_CAST_PTR(char, &target_name)));
		}
	}

	

	*return_node = ACPI_ENTRY_NOT_FOUND;
	status = acpi_ns_search_one_scope(target_name, node, type, return_node);
	if (status != AE_NOT_FOUND) {
		if ((status == AE_OK) && (flags & ACPI_NS_ERROR_IF_FOUND)) {
			status = AE_ALREADY_EXISTS;
		}

		

		return_ACPI_STATUS(status);
	}

	if ((interpreter_mode != ACPI_IMODE_LOAD_PASS1) &&
	    (flags & ACPI_NS_SEARCH_PARENT)) {
		status =
		    acpi_ns_search_parent_tree(target_name, node, type,
					       return_node);
		if (ACPI_SUCCESS(status)) {
			return_ACPI_STATUS(status);
		}
	}

	

	if (interpreter_mode == ACPI_IMODE_EXECUTE) {
		ACPI_DEBUG_PRINT((ACPI_DB_NAMES,
				  "%4.4s Not found in %p [Not adding]\n",
				  ACPI_CAST_PTR(char, &target_name), node));

		return_ACPI_STATUS(AE_NOT_FOUND);
	}

	

	new_node = acpi_ns_create_node(target_name);
	if (!new_node) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}
#ifdef ACPI_ASL_COMPILER

	

	if (flags & ACPI_NS_EXTERNAL) {
		new_node->flags |= ANOBJ_IS_EXTERNAL;
	}
#endif

	if (flags & ACPI_NS_TEMPORARY) {
		new_node->flags |= ANOBJ_TEMPORARY;
	}

	

	acpi_ns_install_node(walk_state, node, new_node, type);
	*return_node = new_node;
	return_ACPI_STATUS(AE_OK);
}
