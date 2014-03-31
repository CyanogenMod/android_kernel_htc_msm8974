
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

#define _COMPONENT          ACPI_UTILITIES
ACPI_MODULE_NAME("utosi")

static struct acpi_interface_info acpi_default_supported_interfaces[] = {
	

	{"Windows 2000", NULL, 0, ACPI_OSI_WIN_2000},	
	{"Windows 2001", NULL, 0, ACPI_OSI_WIN_XP},	
	{"Windows 2001 SP1", NULL, 0, ACPI_OSI_WIN_XP_SP1},	
	{"Windows 2001.1", NULL, 0, ACPI_OSI_WINSRV_2003},	
	{"Windows 2001 SP2", NULL, 0, ACPI_OSI_WIN_XP_SP2},	
	{"Windows 2001.1 SP1", NULL, 0, ACPI_OSI_WINSRV_2003_SP1},	
	{"Windows 2006", NULL, 0, ACPI_OSI_WIN_VISTA},	
	{"Windows 2006.1", NULL, 0, ACPI_OSI_WINSRV_2008},	
	{"Windows 2006 SP1", NULL, 0, ACPI_OSI_WIN_VISTA_SP1},	
	{"Windows 2006 SP2", NULL, 0, ACPI_OSI_WIN_VISTA_SP2},	
	{"Windows 2009", NULL, 0, ACPI_OSI_WIN_7},	

	

	{"Extended Address Space Descriptor", NULL, 0, 0}

};


acpi_status acpi_ut_initialize_interfaces(void)
{
	u32 i;

	(void)acpi_os_acquire_mutex(acpi_gbl_osi_mutex, ACPI_WAIT_FOREVER);
	acpi_gbl_supported_interfaces = acpi_default_supported_interfaces;

	

	for (i = 0;
	     i < (ACPI_ARRAY_LENGTH(acpi_default_supported_interfaces) - 1);
	     i++) {
		acpi_default_supported_interfaces[i].next =
		    &acpi_default_supported_interfaces[(acpi_size) i + 1];
	}

	acpi_os_release_mutex(acpi_gbl_osi_mutex);
	return (AE_OK);
}


void acpi_ut_interface_terminate(void)
{
	struct acpi_interface_info *next_interface;

	(void)acpi_os_acquire_mutex(acpi_gbl_osi_mutex, ACPI_WAIT_FOREVER);
	next_interface = acpi_gbl_supported_interfaces;

	while (next_interface) {
		acpi_gbl_supported_interfaces = next_interface->next;

		

		if (next_interface->flags & ACPI_OSI_DYNAMIC) {
			ACPI_FREE(next_interface->name);
			ACPI_FREE(next_interface);
		}

		next_interface = acpi_gbl_supported_interfaces;
	}

	acpi_os_release_mutex(acpi_gbl_osi_mutex);
}


acpi_status acpi_ut_install_interface(acpi_string interface_name)
{
	struct acpi_interface_info *interface_info;

	

	interface_info =
	    ACPI_ALLOCATE_ZEROED(sizeof(struct acpi_interface_info));
	if (!interface_info) {
		return (AE_NO_MEMORY);
	}

	interface_info->name =
	    ACPI_ALLOCATE_ZEROED(ACPI_STRLEN(interface_name) + 1);
	if (!interface_info->name) {
		ACPI_FREE(interface_info);
		return (AE_NO_MEMORY);
	}

	

	ACPI_STRCPY(interface_info->name, interface_name);
	interface_info->flags = ACPI_OSI_DYNAMIC;
	interface_info->next = acpi_gbl_supported_interfaces;

	acpi_gbl_supported_interfaces = interface_info;
	return (AE_OK);
}


acpi_status acpi_ut_remove_interface(acpi_string interface_name)
{
	struct acpi_interface_info *previous_interface;
	struct acpi_interface_info *next_interface;

	previous_interface = next_interface = acpi_gbl_supported_interfaces;
	while (next_interface) {
		if (!ACPI_STRCMP(interface_name, next_interface->name)) {

			

			if (next_interface->flags & ACPI_OSI_DYNAMIC) {

				

				if (previous_interface == next_interface) {
					acpi_gbl_supported_interfaces =
					    next_interface->next;
				} else {
					previous_interface->next =
					    next_interface->next;
				}

				ACPI_FREE(next_interface->name);
				ACPI_FREE(next_interface);
			} else {
				if (next_interface->flags & ACPI_OSI_INVALID) {
					return (AE_NOT_EXIST);
				}

				next_interface->flags |= ACPI_OSI_INVALID;
			}

			return (AE_OK);
		}

		previous_interface = next_interface;
		next_interface = next_interface->next;
	}

	

	return (AE_NOT_EXIST);
}


struct acpi_interface_info *acpi_ut_get_interface(acpi_string interface_name)
{
	struct acpi_interface_info *next_interface;

	next_interface = acpi_gbl_supported_interfaces;
	while (next_interface) {
		if (!ACPI_STRCMP(interface_name, next_interface->name)) {
			return (next_interface);
		}

		next_interface = next_interface->next;
	}

	return (NULL);
}


acpi_status acpi_ut_osi_implementation(struct acpi_walk_state * walk_state)
{
	union acpi_operand_object *string_desc;
	union acpi_operand_object *return_desc;
	struct acpi_interface_info *interface_info;
	acpi_interface_handler interface_handler;
	u32 return_value;

	ACPI_FUNCTION_TRACE(ut_osi_implementation);

	

	string_desc = walk_state->arguments[0].object;
	if (!string_desc || (string_desc->common.type != ACPI_TYPE_STRING)) {
		return_ACPI_STATUS(AE_TYPE);
	}

	

	return_desc = acpi_ut_create_internal_object(ACPI_TYPE_INTEGER);
	if (!return_desc) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	

	return_value = 0;
	(void)acpi_os_acquire_mutex(acpi_gbl_osi_mutex, ACPI_WAIT_FOREVER);

	

	interface_info = acpi_ut_get_interface(string_desc->string.pointer);
	if (interface_info && !(interface_info->flags & ACPI_OSI_INVALID)) {
		if (interface_info->value > acpi_gbl_osi_data) {
			acpi_gbl_osi_data = interface_info->value;
		}

		return_value = ACPI_UINT32_MAX;
	}

	acpi_os_release_mutex(acpi_gbl_osi_mutex);

	interface_handler = acpi_gbl_interface_handler;
	if (interface_handler) {
		return_value =
		    interface_handler(string_desc->string.pointer,
				      return_value);
	}

	ACPI_DEBUG_PRINT_RAW((ACPI_DB_INFO,
			      "ACPI: BIOS _OSI(\"%s\") is %ssupported\n",
			      string_desc->string.pointer,
			      return_value == 0 ? "not " : ""));

	

	return_desc->integer.value = return_value;
	walk_state->return_desc = return_desc;
	return_ACPI_STATUS(AE_OK);
}
