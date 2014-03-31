

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

#define _COMPONENT          ACPI_EXECUTER
ACPI_MODULE_NAME("exstorob")

acpi_status
acpi_ex_store_buffer_to_buffer(union acpi_operand_object *source_desc,
			       union acpi_operand_object *target_desc)
{
	u32 length;
	u8 *buffer;

	ACPI_FUNCTION_TRACE_PTR(ex_store_buffer_to_buffer, source_desc);

	

	if (source_desc == target_desc) {
		return_ACPI_STATUS(AE_OK);
	}

	

	buffer = ACPI_CAST_PTR(u8, source_desc->buffer.pointer);
	length = source_desc->buffer.length;

	if ((target_desc->buffer.length == 0) ||
	    (target_desc->common.flags & AOPOBJ_STATIC_POINTER)) {
		target_desc->buffer.pointer = ACPI_ALLOCATE(length);
		if (!target_desc->buffer.pointer) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		target_desc->buffer.length = length;
	}

	

	if (length <= target_desc->buffer.length) {

		

		ACPI_MEMSET(target_desc->buffer.pointer, 0,
			    target_desc->buffer.length);
		ACPI_MEMCPY(target_desc->buffer.pointer, buffer, length);

#ifdef ACPI_OBSOLETE_BEHAVIOR

		if (original_src_type == ACPI_TYPE_STRING) {

			

			target_desc->buffer.length = length;
		}
#endif
	} else {
		

		ACPI_MEMCPY(target_desc->buffer.pointer, buffer,
			    target_desc->buffer.length);

		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				  "Truncating source buffer from %X to %X\n",
				  length, target_desc->buffer.length));
	}

	

	target_desc->buffer.flags = source_desc->buffer.flags;
	target_desc->common.flags &= ~AOPOBJ_STATIC_POINTER;
	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_ex_store_string_to_string(union acpi_operand_object *source_desc,
			       union acpi_operand_object *target_desc)
{
	u32 length;
	u8 *buffer;

	ACPI_FUNCTION_TRACE_PTR(ex_store_string_to_string, source_desc);

	

	if (source_desc == target_desc) {
		return_ACPI_STATUS(AE_OK);
	}

	

	buffer = ACPI_CAST_PTR(u8, source_desc->string.pointer);
	length = source_desc->string.length;

	if ((length < target_desc->string.length) &&
	    (!(target_desc->common.flags & AOPOBJ_STATIC_POINTER))) {
		ACPI_MEMSET(target_desc->string.pointer, 0,
			    (acpi_size) target_desc->string.length + 1);
		ACPI_MEMCPY(target_desc->string.pointer, buffer, length);
	} else {
		if (target_desc->string.pointer &&
		    (!(target_desc->common.flags & AOPOBJ_STATIC_POINTER))) {

			

			ACPI_FREE(target_desc->string.pointer);
		}

		target_desc->string.pointer = ACPI_ALLOCATE_ZEROED((acpi_size)
								   length + 1);
		if (!target_desc->string.pointer) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		target_desc->common.flags &= ~AOPOBJ_STATIC_POINTER;
		ACPI_MEMCPY(target_desc->string.pointer, buffer, length);
	}

	

	target_desc->string.length = length;
	return_ACPI_STATUS(AE_OK);
}
