
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
#include "acdebug.h"

#define _COMPONENT          ACPI_UTILITIES
ACPI_MODULE_NAME("utalloc")

acpi_status acpi_ut_create_caches(void)
{
	acpi_status status;

	

	status =
	    acpi_os_create_cache("Acpi-Namespace",
				 sizeof(struct acpi_namespace_node),
				 ACPI_MAX_NAMESPACE_CACHE_DEPTH,
				 &acpi_gbl_namespace_cache);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	status =
	    acpi_os_create_cache("Acpi-State", sizeof(union acpi_generic_state),
				 ACPI_MAX_STATE_CACHE_DEPTH,
				 &acpi_gbl_state_cache);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	status =
	    acpi_os_create_cache("Acpi-Parse",
				 sizeof(struct acpi_parse_obj_common),
				 ACPI_MAX_PARSE_CACHE_DEPTH,
				 &acpi_gbl_ps_node_cache);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	status =
	    acpi_os_create_cache("Acpi-ParseExt",
				 sizeof(struct acpi_parse_obj_named),
				 ACPI_MAX_EXTPARSE_CACHE_DEPTH,
				 &acpi_gbl_ps_node_ext_cache);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	status =
	    acpi_os_create_cache("Acpi-Operand",
				 sizeof(union acpi_operand_object),
				 ACPI_MAX_OBJECT_CACHE_DEPTH,
				 &acpi_gbl_operand_cache);
	if (ACPI_FAILURE(status)) {
		return (status);
	}
#ifdef ACPI_DBG_TRACK_ALLOCATIONS

	

	status = acpi_ut_create_list("Acpi-Global", 0, &acpi_gbl_global_list);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	status =
	    acpi_ut_create_list("Acpi-Namespace",
				sizeof(struct acpi_namespace_node),
				&acpi_gbl_ns_node_list);
	if (ACPI_FAILURE(status)) {
		return (status);
	}
#endif

	return (AE_OK);
}


acpi_status acpi_ut_delete_caches(void)
{
#ifdef ACPI_DBG_TRACK_ALLOCATIONS
	char buffer[7];

	if (acpi_gbl_display_final_mem_stats) {
		ACPI_STRCPY(buffer, "MEMORY");
		(void)acpi_db_display_statistics(buffer);
	}
#endif

	(void)acpi_os_delete_cache(acpi_gbl_namespace_cache);
	acpi_gbl_namespace_cache = NULL;

	(void)acpi_os_delete_cache(acpi_gbl_state_cache);
	acpi_gbl_state_cache = NULL;

	(void)acpi_os_delete_cache(acpi_gbl_operand_cache);
	acpi_gbl_operand_cache = NULL;

	(void)acpi_os_delete_cache(acpi_gbl_ps_node_cache);
	acpi_gbl_ps_node_cache = NULL;

	(void)acpi_os_delete_cache(acpi_gbl_ps_node_ext_cache);
	acpi_gbl_ps_node_ext_cache = NULL;

#ifdef ACPI_DBG_TRACK_ALLOCATIONS

	

	acpi_ut_dump_allocations(ACPI_UINT32_MAX, NULL);

	

	ACPI_FREE(acpi_gbl_global_list);
	acpi_gbl_global_list = NULL;

	ACPI_FREE(acpi_gbl_ns_node_list);
	acpi_gbl_ns_node_list = NULL;
#endif

	return (AE_OK);
}


acpi_status acpi_ut_validate_buffer(struct acpi_buffer * buffer)
{

	

	if (!buffer) {
		return (AE_BAD_PARAMETER);
	}

	

	if ((buffer->length == ACPI_NO_BUFFER) ||
	    (buffer->length == ACPI_ALLOCATE_BUFFER) ||
	    (buffer->length == ACPI_ALLOCATE_LOCAL_BUFFER)) {
		return (AE_OK);
	}

	

	if (!buffer->pointer) {
		return (AE_BAD_PARAMETER);
	}

	return (AE_OK);
}


acpi_status
acpi_ut_initialize_buffer(struct acpi_buffer * buffer,
			  acpi_size required_length)
{
	acpi_size input_buffer_length;

	

	if (!buffer || !required_length) {
		return (AE_BAD_PARAMETER);
	}

	input_buffer_length = buffer->length;
	buffer->length = required_length;

	switch (input_buffer_length) {
	case ACPI_NO_BUFFER:

		

		return (AE_BUFFER_OVERFLOW);

	case ACPI_ALLOCATE_BUFFER:

		

		buffer->pointer = acpi_os_allocate(required_length);
		break;

	case ACPI_ALLOCATE_LOCAL_BUFFER:

		

		buffer->pointer = ACPI_ALLOCATE(required_length);
		break;

	default:

		

		if (input_buffer_length < required_length) {
			return (AE_BUFFER_OVERFLOW);
		}
		break;
	}

	

	if (!buffer->pointer) {
		return (AE_NO_MEMORY);
	}

	

	ACPI_MEMSET(buffer->pointer, 0, required_length);
	return (AE_OK);
}

#ifdef NOT_USED_BY_LINUX

void *acpi_ut_allocate(acpi_size size,
		       u32 component, const char *module, u32 line)
{
	void *allocation;

	ACPI_FUNCTION_TRACE_U32(ut_allocate, size);

	

	if (!size) {
		ACPI_WARNING((module, line,
			      "Attempt to allocate zero bytes, allocating 1 byte"));
		size = 1;
	}

	allocation = acpi_os_allocate(size);
	if (!allocation) {

		

		ACPI_WARNING((module, line,
			      "Could not allocate size %u", (u32) size));

		return_PTR(NULL);
	}

	return_PTR(allocation);
}


void *acpi_ut_allocate_zeroed(acpi_size size,
			      u32 component, const char *module, u32 line)
{
	void *allocation;

	ACPI_FUNCTION_ENTRY();

	allocation = acpi_ut_allocate(size, component, module, line);
	if (allocation) {

		

		ACPI_MEMSET(allocation, 0, size);
	}

	return (allocation);
}
#endif
