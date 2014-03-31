
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
ACPI_MODULE_NAME("utaddress")

acpi_status
acpi_ut_add_address_range(acpi_adr_space_type space_id,
			  acpi_physical_address address,
			  u32 length, struct acpi_namespace_node *region_node)
{
	struct acpi_address_range *range_info;
	acpi_status status;

	ACPI_FUNCTION_TRACE(ut_add_address_range);

	if ((space_id != ACPI_ADR_SPACE_SYSTEM_MEMORY) &&
	    (space_id != ACPI_ADR_SPACE_SYSTEM_IO)) {
		return_ACPI_STATUS(AE_OK);
	}

	

	range_info = ACPI_ALLOCATE(sizeof(struct acpi_address_range));
	if (!range_info) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	range_info->start_address = address;
	range_info->end_address = (address + length - 1);
	range_info->region_node = region_node;

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		ACPI_FREE(range_info);
		return_ACPI_STATUS(status);
	}

	range_info->next = acpi_gbl_address_range_list[space_id];
	acpi_gbl_address_range_list[space_id] = range_info;

	ACPI_DEBUG_PRINT((ACPI_DB_NAMES,
			  "\nAdded [%4.4s] address range: 0x%p-0x%p\n",
			  acpi_ut_get_node_name(range_info->region_node),
			  ACPI_CAST_PTR(void, address),
			  ACPI_CAST_PTR(void, range_info->end_address)));

	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	return_ACPI_STATUS(AE_OK);
}


void
acpi_ut_remove_address_range(acpi_adr_space_type space_id,
			     struct acpi_namespace_node *region_node)
{
	struct acpi_address_range *range_info;
	struct acpi_address_range *prev;

	ACPI_FUNCTION_TRACE(ut_remove_address_range);

	if ((space_id != ACPI_ADR_SPACE_SYSTEM_MEMORY) &&
	    (space_id != ACPI_ADR_SPACE_SYSTEM_IO)) {
		return_VOID;
	}

	

	range_info = prev = acpi_gbl_address_range_list[space_id];
	while (range_info) {
		if (range_info->region_node == region_node) {
			if (range_info == prev) {	
				acpi_gbl_address_range_list[space_id] =
				    range_info->next;
			} else {
				prev->next = range_info->next;
			}

			ACPI_DEBUG_PRINT((ACPI_DB_NAMES,
					  "\nRemoved [%4.4s] address range: 0x%p-0x%p\n",
					  acpi_ut_get_node_name(range_info->
								region_node),
					  ACPI_CAST_PTR(void,
							range_info->
							start_address),
					  ACPI_CAST_PTR(void,
							range_info->
							end_address)));

			ACPI_FREE(range_info);
			return_VOID;
		}

		prev = range_info;
		range_info = range_info->next;
	}

	return_VOID;
}


u32
acpi_ut_check_address_range(acpi_adr_space_type space_id,
			    acpi_physical_address address, u32 length, u8 warn)
{
	struct acpi_address_range *range_info;
	acpi_physical_address end_address;
	char *pathname;
	u32 overlap_count = 0;

	ACPI_FUNCTION_TRACE(ut_check_address_range);

	if ((space_id != ACPI_ADR_SPACE_SYSTEM_MEMORY) &&
	    (space_id != ACPI_ADR_SPACE_SYSTEM_IO)) {
		return_UINT32(0);
	}

	range_info = acpi_gbl_address_range_list[space_id];
	end_address = address + length - 1;

	

	while (range_info) {
		if ((address <= range_info->end_address) &&
		    (end_address >= range_info->start_address)) {

			

			overlap_count++;
			if (warn) {	
				pathname =
				    acpi_ns_get_external_pathname(range_info->
								  region_node);

				ACPI_WARNING((AE_INFO,
					      "0x%p-0x%p %s conflicts with Region %s %d",
					      ACPI_CAST_PTR(void, address),
					      ACPI_CAST_PTR(void, end_address),
					      acpi_ut_get_region_name(space_id),
					      pathname, overlap_count));
				ACPI_FREE(pathname);
			}
		}

		range_info = range_info->next;
	}

	return_UINT32(overlap_count);
}


void acpi_ut_delete_address_lists(void)
{
	struct acpi_address_range *next;
	struct acpi_address_range *range_info;
	int i;

	

	for (i = 0; i < ACPI_ADDRESS_RANGE_MAX; i++) {
		next = acpi_gbl_address_range_list[i];

		while (next) {
			range_info = next;
			next = range_info->next;
			ACPI_FREE(range_info);
		}

		acpi_gbl_address_range_list[i] = NULL;
	}
}
