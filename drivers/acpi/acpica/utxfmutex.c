
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

#define _COMPONENT          ACPI_UTILITIES
ACPI_MODULE_NAME("utxfmutex")

static acpi_status
acpi_ut_get_mutex_object(acpi_handle handle,
			 acpi_string pathname,
			 union acpi_operand_object **ret_obj);


static acpi_status
acpi_ut_get_mutex_object(acpi_handle handle,
			 acpi_string pathname,
			 union acpi_operand_object **ret_obj)
{
	struct acpi_namespace_node *mutex_node;
	union acpi_operand_object *mutex_obj;
	acpi_status status;

	

	if (!ret_obj || (!handle && !pathname)) {
		return (AE_BAD_PARAMETER);
	}

	

	mutex_node = handle;
	if (pathname != NULL) {
		status = acpi_get_handle(handle, pathname,
					 ACPI_CAST_PTR(acpi_handle,
						       &mutex_node));
		if (ACPI_FAILURE(status)) {
			return (status);
		}
	}

	

	if (!mutex_node || (mutex_node->type != ACPI_TYPE_MUTEX)) {
		return (AE_TYPE);
	}

	

	mutex_obj = acpi_ns_get_attached_object(mutex_node);
	if (!mutex_obj) {
		return (AE_NULL_OBJECT);
	}

	*ret_obj = mutex_obj;
	return (AE_OK);
}


acpi_status
acpi_acquire_mutex(acpi_handle handle, acpi_string pathname, u16 timeout)
{
	acpi_status status;
	union acpi_operand_object *mutex_obj;

	

	status = acpi_ut_get_mutex_object(handle, pathname, &mutex_obj);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	

	status = acpi_os_acquire_mutex(mutex_obj->mutex.os_mutex, timeout);
	return (status);
}


acpi_status acpi_release_mutex(acpi_handle handle, acpi_string pathname)
{
	acpi_status status;
	union acpi_operand_object *mutex_obj;

	

	status = acpi_ut_get_mutex_object(handle, pathname, &mutex_obj);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	

	acpi_os_release_mutex(mutex_obj->mutex.os_mutex);
	return (AE_OK);
}
