
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

#define _COMPONENT          ACPI_UTILITIES
ACPI_MODULE_NAME("utids")

acpi_status
acpi_ut_execute_HID(struct acpi_namespace_node *device_node,
		    struct acpica_device_id **return_id)
{
	union acpi_operand_object *obj_desc;
	struct acpica_device_id *hid;
	u32 length;
	acpi_status status;

	ACPI_FUNCTION_TRACE(ut_execute_HID);

	status = acpi_ut_evaluate_object(device_node, METHOD_NAME__HID,
					 ACPI_BTYPE_INTEGER | ACPI_BTYPE_STRING,
					 &obj_desc);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	

	if (obj_desc->common.type == ACPI_TYPE_INTEGER) {
		length = ACPI_EISAID_STRING_SIZE;
	} else {
		length = obj_desc->string.length + 1;
	}

	

	hid =
	    ACPI_ALLOCATE_ZEROED(sizeof(struct acpica_device_id) +
				 (acpi_size) length);
	if (!hid) {
		status = AE_NO_MEMORY;
		goto cleanup;
	}

	

	hid->string = ACPI_ADD_PTR(char, hid, sizeof(struct acpica_device_id));

	

	if (obj_desc->common.type == ACPI_TYPE_INTEGER) {
		acpi_ex_eisa_id_to_string(hid->string, obj_desc->integer.value);
	} else {
		ACPI_STRCPY(hid->string, obj_desc->string.pointer);
	}

	hid->length = length;
	*return_id = hid;

cleanup:

	

	acpi_ut_remove_reference(obj_desc);
	return_ACPI_STATUS(status);
}


acpi_status
acpi_ut_execute_UID(struct acpi_namespace_node *device_node,
		    struct acpica_device_id **return_id)
{
	union acpi_operand_object *obj_desc;
	struct acpica_device_id *uid;
	u32 length;
	acpi_status status;

	ACPI_FUNCTION_TRACE(ut_execute_UID);

	status = acpi_ut_evaluate_object(device_node, METHOD_NAME__UID,
					 ACPI_BTYPE_INTEGER | ACPI_BTYPE_STRING,
					 &obj_desc);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	

	if (obj_desc->common.type == ACPI_TYPE_INTEGER) {
		length = ACPI_MAX64_DECIMAL_DIGITS + 1;
	} else {
		length = obj_desc->string.length + 1;
	}

	

	uid =
	    ACPI_ALLOCATE_ZEROED(sizeof(struct acpica_device_id) +
				 (acpi_size) length);
	if (!uid) {
		status = AE_NO_MEMORY;
		goto cleanup;
	}

	

	uid->string = ACPI_ADD_PTR(char, uid, sizeof(struct acpica_device_id));

	

	if (obj_desc->common.type == ACPI_TYPE_INTEGER) {
		acpi_ex_integer_to_string(uid->string, obj_desc->integer.value);
	} else {
		ACPI_STRCPY(uid->string, obj_desc->string.pointer);
	}

	uid->length = length;
	*return_id = uid;

cleanup:

	

	acpi_ut_remove_reference(obj_desc);
	return_ACPI_STATUS(status);
}


acpi_status
acpi_ut_execute_CID(struct acpi_namespace_node *device_node,
		    struct acpica_device_id_list **return_cid_list)
{
	union acpi_operand_object **cid_objects;
	union acpi_operand_object *obj_desc;
	struct acpica_device_id_list *cid_list;
	char *next_id_string;
	u32 string_area_size;
	u32 length;
	u32 cid_list_size;
	acpi_status status;
	u32 count;
	u32 i;

	ACPI_FUNCTION_TRACE(ut_execute_CID);

	

	status = acpi_ut_evaluate_object(device_node, METHOD_NAME__CID,
					 ACPI_BTYPE_INTEGER | ACPI_BTYPE_STRING
					 | ACPI_BTYPE_PACKAGE, &obj_desc);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	if (obj_desc->common.type == ACPI_TYPE_PACKAGE) {
		count = obj_desc->package.count;
		cid_objects = obj_desc->package.elements;
	} else {		

		count = 1;
		cid_objects = &obj_desc;
	}

	string_area_size = 0;
	for (i = 0; i < count; i++) {

		

		switch (cid_objects[i]->common.type) {
		case ACPI_TYPE_INTEGER:
			string_area_size += ACPI_EISAID_STRING_SIZE;
			break;

		case ACPI_TYPE_STRING:
			string_area_size += cid_objects[i]->string.length + 1;
			break;

		default:
			status = AE_TYPE;
			goto cleanup;
		}
	}

	cid_list_size = sizeof(struct acpica_device_id_list) +
	    ((count - 1) * sizeof(struct acpica_device_id)) + string_area_size;

	cid_list = ACPI_ALLOCATE_ZEROED(cid_list_size);
	if (!cid_list) {
		status = AE_NO_MEMORY;
		goto cleanup;
	}

	

	next_id_string = ACPI_CAST_PTR(char, cid_list->ids) +
	    ((acpi_size) count * sizeof(struct acpica_device_id));

	

	for (i = 0; i < count; i++) {
		if (cid_objects[i]->common.type == ACPI_TYPE_INTEGER) {

			

			acpi_ex_eisa_id_to_string(next_id_string,
						  cid_objects[i]->integer.
						  value);
			length = ACPI_EISAID_STRING_SIZE;
		} else {	

			

			ACPI_STRCPY(next_id_string,
				    cid_objects[i]->string.pointer);
			length = cid_objects[i]->string.length + 1;
		}

		cid_list->ids[i].string = next_id_string;
		cid_list->ids[i].length = length;
		next_id_string += length;
	}

	

	cid_list->count = count;
	cid_list->list_size = cid_list_size;
	*return_cid_list = cid_list;

cleanup:

	

	acpi_ut_remove_reference(obj_desc);
	return_ACPI_STATUS(status);
}
