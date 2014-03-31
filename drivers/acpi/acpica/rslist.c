
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
#include "acresrc.h"

#define _COMPONENT          ACPI_RESOURCES
ACPI_MODULE_NAME("rslist")

acpi_status
acpi_rs_convert_aml_to_resources(u8 * aml,
				 u32 length,
				 u32 offset, u8 resource_index, void **context)
{
	struct acpi_resource **resource_ptr =
	    ACPI_CAST_INDIRECT_PTR(struct acpi_resource, context);
	struct acpi_resource *resource;
	union aml_resource *aml_resource;
	struct acpi_rsconvert_info *conversion_table;
	acpi_status status;

	ACPI_FUNCTION_TRACE(rs_convert_aml_to_resources);

	resource = *resource_ptr;
	if (ACPI_IS_MISALIGNED(resource)) {
		ACPI_WARNING((AE_INFO,
			      "Misaligned resource pointer %p", resource));
	}

	

	aml_resource = ACPI_CAST_PTR(union aml_resource, aml);
	if (acpi_ut_get_resource_type(aml) == ACPI_RESOURCE_NAME_SERIAL_BUS) {
		if (aml_resource->common_serial_bus.type >
		    AML_RESOURCE_MAX_SERIALBUSTYPE) {
			conversion_table = NULL;
		} else {
			

			conversion_table =
			    acpi_gbl_convert_resource_serial_bus_dispatch
			    [aml_resource->common_serial_bus.type];
		}
	} else {
		conversion_table =
		    acpi_gbl_get_resource_dispatch[resource_index];
	}

	if (!conversion_table) {
		ACPI_ERROR((AE_INFO,
			    "Invalid/unsupported resource descriptor: Type 0x%2.2X",
			    resource_index));
		return (AE_AML_INVALID_RESOURCE_TYPE);
	}

	

	status =
	    acpi_rs_convert_aml_to_resource(resource, aml_resource,
					    conversion_table);
	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status,
				"Could not convert AML resource (Type 0x%X)",
				*aml));
		return_ACPI_STATUS(status);
	}

	ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
			  "Type %.2X, AmlLength %.2X InternalLength %.2X\n",
			  acpi_ut_get_resource_type(aml), length,
			  resource->length));

	

	*resource_ptr = ACPI_NEXT_RESOURCE(resource);
	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_rs_convert_resources_to_aml(struct acpi_resource *resource,
				 acpi_size aml_size_needed, u8 * output_buffer)
{
	u8 *aml = output_buffer;
	u8 *end_aml = output_buffer + aml_size_needed;
	struct acpi_rsconvert_info *conversion_table;
	acpi_status status;

	ACPI_FUNCTION_TRACE(rs_convert_resources_to_aml);

	

	while (aml < end_aml) {

		

		if (resource->type > ACPI_RESOURCE_TYPE_MAX) {
			ACPI_ERROR((AE_INFO,
				    "Invalid descriptor type (0x%X) in resource list",
				    resource->type));
			return_ACPI_STATUS(AE_BAD_DATA);
		}

		

		if (resource->type == ACPI_RESOURCE_TYPE_SERIAL_BUS) {
			if (resource->data.common_serial_bus.type >
			    AML_RESOURCE_MAX_SERIALBUSTYPE) {
				conversion_table = NULL;
			} else {
				

				conversion_table =
				    acpi_gbl_convert_resource_serial_bus_dispatch
				    [resource->data.common_serial_bus.type];
			}
		} else {
			conversion_table =
			    acpi_gbl_set_resource_dispatch[resource->type];
		}

		if (!conversion_table) {
			ACPI_ERROR((AE_INFO,
				    "Invalid/unsupported resource descriptor: Type 0x%2.2X",
				    resource->type));
			return (AE_AML_INVALID_RESOURCE_TYPE);
		}

		status = acpi_rs_convert_resource_to_aml(resource,
						         ACPI_CAST_PTR(union
								       aml_resource,
								       aml),
							 conversion_table);
		if (ACPI_FAILURE(status)) {
			ACPI_EXCEPTION((AE_INFO, status,
					"Could not convert resource (type 0x%X) to AML",
					resource->type));
			return_ACPI_STATUS(status);
		}

		

		status =
		    acpi_ut_validate_resource(ACPI_CAST_PTR
					      (union aml_resource, aml), NULL);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

		

		if (resource->type == ACPI_RESOURCE_TYPE_END_TAG) {

			

			return_ACPI_STATUS(AE_OK);
		}

		aml += acpi_ut_get_descriptor_length(aml);

		

		resource = ACPI_NEXT_RESOURCE(resource);
	}

	

	return_ACPI_STATUS(AE_AML_NO_RESOURCE_END_TAG);
}
