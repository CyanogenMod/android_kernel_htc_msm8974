
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

#define _COMPONENT          ACPI_NAMESPACE
ACPI_MODULE_NAME("nswalk")

struct acpi_namespace_node *acpi_ns_get_next_node(struct acpi_namespace_node
						  *parent_node,
						  struct acpi_namespace_node
						  *child_node)
{
	ACPI_FUNCTION_ENTRY();

	if (!child_node) {

		

		return parent_node->child;
	}

	

	return child_node->peer;
}


struct acpi_namespace_node *acpi_ns_get_next_node_typed(acpi_object_type type,
							struct
							acpi_namespace_node
							*parent_node,
							struct
							acpi_namespace_node
							*child_node)
{
	struct acpi_namespace_node *next_node = NULL;

	ACPI_FUNCTION_ENTRY();

	next_node = acpi_ns_get_next_node(parent_node, child_node);


	

	if (type == ACPI_TYPE_ANY) {

		

		return (next_node);
	}

	

	while (next_node) {

		

		if (next_node->type == type) {
			return (next_node);
		}

		

		next_node = next_node->peer;
	}

	

	return (NULL);
}


acpi_status
acpi_ns_walk_namespace(acpi_object_type type,
		       acpi_handle start_node,
		       u32 max_depth,
		       u32 flags,
		       acpi_walk_callback pre_order_visit,
		       acpi_walk_callback post_order_visit,
		       void *context, void **return_value)
{
	acpi_status status;
	acpi_status mutex_status;
	struct acpi_namespace_node *child_node;
	struct acpi_namespace_node *parent_node;
	acpi_object_type child_type;
	u32 level;
	u8 node_previously_visited = FALSE;

	ACPI_FUNCTION_TRACE(ns_walk_namespace);

	

	if (start_node == ACPI_ROOT_OBJECT) {
		start_node = acpi_gbl_root_node;
	}

	

	parent_node = start_node;
	child_node = acpi_ns_get_next_node(parent_node, NULL);
	child_type = ACPI_TYPE_ANY;
	level = 1;

	while (level > 0 && child_node) {
		status = AE_OK;

		

		if (type != ACPI_TYPE_ANY) {
			child_type = child_node->type;
		}

		if ((child_node->flags & ANOBJ_TEMPORARY) &&
		    !(flags & ACPI_NS_WALK_TEMP_NODES)) {
			status = AE_CTRL_DEPTH;
		}

		

		else if (child_type == type) {
			if (flags & ACPI_NS_WALK_UNLOCK) {
				mutex_status =
				    acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
				if (ACPI_FAILURE(mutex_status)) {
					return_ACPI_STATUS(mutex_status);
				}
			}

			if (!node_previously_visited) {
				if (pre_order_visit) {
					status =
					    pre_order_visit(child_node, level,
							    context,
							    return_value);
				}
			} else {
				if (post_order_visit) {
					status =
					    post_order_visit(child_node, level,
							     context,
							     return_value);
				}
			}

			if (flags & ACPI_NS_WALK_UNLOCK) {
				mutex_status =
				    acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
				if (ACPI_FAILURE(mutex_status)) {
					return_ACPI_STATUS(mutex_status);
				}
			}

			switch (status) {
			case AE_OK:
			case AE_CTRL_DEPTH:

				
				break;

			case AE_CTRL_TERMINATE:

				

				return_ACPI_STATUS(AE_OK);

			default:

				

				return_ACPI_STATUS(status);
			}
		}

		if (!node_previously_visited &&
		    (level < max_depth) && (status != AE_CTRL_DEPTH)) {
			if (child_node->child) {

				

				level++;
				parent_node = child_node;
				child_node =
				    acpi_ns_get_next_node(parent_node, NULL);
				continue;
			}
		}

		

		if (!node_previously_visited) {
			node_previously_visited = TRUE;
			continue;
		}

		

		child_node = acpi_ns_get_next_node(parent_node, child_node);
		if (child_node) {
			node_previously_visited = FALSE;
		}

		

		else {
			level--;
			child_node = parent_node;
			parent_node = parent_node->parent;

			node_previously_visited = TRUE;
		}
	}

	

	return_ACPI_STATUS(AE_OK);
}
