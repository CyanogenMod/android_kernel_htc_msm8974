
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

#include <linux/export.h>
#include <acpi/acpi.h>
#include "accommon.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_NAMESPACE
ACPI_MODULE_NAME("nsxfobj")

acpi_status acpi_get_id(acpi_handle handle, acpi_owner_id * ret_id)
{
	struct acpi_namespace_node *node;
	acpi_status status;

	

	if (!ret_id) {
		return (AE_BAD_PARAMETER);
	}

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	

	node = acpi_ns_validate_handle(handle);
	if (!node) {
		(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
		return (AE_BAD_PARAMETER);
	}

	*ret_id = node->owner_id;

	status = acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	return (status);
}

ACPI_EXPORT_SYMBOL(acpi_get_id)

acpi_status acpi_get_type(acpi_handle handle, acpi_object_type * ret_type)
{
	struct acpi_namespace_node *node;
	acpi_status status;

	

	if (!ret_type) {
		return (AE_BAD_PARAMETER);
	}

	if (handle == ACPI_ROOT_OBJECT) {
		*ret_type = ACPI_TYPE_ANY;
		return (AE_OK);
	}

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	

	node = acpi_ns_validate_handle(handle);
	if (!node) {
		(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
		return (AE_BAD_PARAMETER);
	}

	*ret_type = node->type;

	status = acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	return (status);
}

ACPI_EXPORT_SYMBOL(acpi_get_type)

acpi_status acpi_get_parent(acpi_handle handle, acpi_handle * ret_handle)
{
	struct acpi_namespace_node *node;
	struct acpi_namespace_node *parent_node;
	acpi_status status;

	if (!ret_handle) {
		return (AE_BAD_PARAMETER);
	}

	

	if (handle == ACPI_ROOT_OBJECT) {
		return (AE_NULL_ENTRY);
	}

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	

	node = acpi_ns_validate_handle(handle);
	if (!node) {
		status = AE_BAD_PARAMETER;
		goto unlock_and_exit;
	}

	

	parent_node = node->parent;
	*ret_handle = ACPI_CAST_PTR(acpi_handle, parent_node);

	

	if (!parent_node) {
		status = AE_NULL_ENTRY;
	}

      unlock_and_exit:

	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	return (status);
}

ACPI_EXPORT_SYMBOL(acpi_get_parent)

acpi_status
acpi_get_next_object(acpi_object_type type,
		     acpi_handle parent,
		     acpi_handle child, acpi_handle * ret_handle)
{
	acpi_status status;
	struct acpi_namespace_node *node;
	struct acpi_namespace_node *parent_node = NULL;
	struct acpi_namespace_node *child_node = NULL;

	

	if (type > ACPI_TYPE_EXTERNAL_MAX) {
		return (AE_BAD_PARAMETER);
	}

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	

	if (!child) {

		

		parent_node = acpi_ns_validate_handle(parent);
		if (!parent_node) {
			status = AE_BAD_PARAMETER;
			goto unlock_and_exit;
		}
	} else {
		
		

		child_node = acpi_ns_validate_handle(child);
		if (!child_node) {
			status = AE_BAD_PARAMETER;
			goto unlock_and_exit;
		}
	}

	

	node = acpi_ns_get_next_node_typed(type, parent_node, child_node);
	if (!node) {
		status = AE_NOT_FOUND;
		goto unlock_and_exit;
	}

	if (ret_handle) {
		*ret_handle = ACPI_CAST_PTR(acpi_handle, node);
	}

      unlock_and_exit:

	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	return (status);
}

ACPI_EXPORT_SYMBOL(acpi_get_next_object)
