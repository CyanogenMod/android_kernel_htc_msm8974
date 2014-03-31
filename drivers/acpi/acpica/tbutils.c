
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
#include "actables.h"

#define _COMPONENT          ACPI_TABLES
ACPI_MODULE_NAME("tbutils")

static void acpi_tb_fix_string(char *string, acpi_size length);

static void
acpi_tb_cleanup_table_header(struct acpi_table_header *out_header,
			     struct acpi_table_header *header);

static acpi_physical_address
acpi_tb_get_root_table_entry(u8 *table_entry, u32 table_entry_size);


static acpi_status
acpi_tb_check_xsdt(acpi_physical_address address)
{
	struct acpi_table_header *table;
	u32 length;
	u64 xsdt_entry_address;
	u8 *table_entry;
	u32 table_count;
	int i;

	table = acpi_os_map_memory(address, sizeof(struct acpi_table_header));
	if (!table)
		return AE_NO_MEMORY;

	length = table->length;
	acpi_os_unmap_memory(table, sizeof(struct acpi_table_header));
	if (length < sizeof(struct acpi_table_header))
		return AE_INVALID_TABLE_LENGTH;

	table = acpi_os_map_memory(address, length);
	if (!table)
		return AE_NO_MEMORY;

	
	table_count =
		(u32) ((table->length -
		sizeof(struct acpi_table_header)) / sizeof(u64));
	table_entry =
		ACPI_CAST_PTR(u8, table) + sizeof(struct acpi_table_header);
	for (i = 0; i < table_count; i++) {
		ACPI_MOVE_64_TO_64(&xsdt_entry_address, table_entry);
		if (!xsdt_entry_address) {
			
			break;
		}
		table_entry += sizeof(u64);
	}
	acpi_os_unmap_memory(table, length);

	if (i < table_count)
		return AE_NULL_ENTRY;
	else
		return AE_OK;
}

#if (!ACPI_REDUCED_HARDWARE)

acpi_status acpi_tb_initialize_facs(void)
{
	acpi_status status;

	

	if (acpi_gbl_reduced_hardware) {
		acpi_gbl_FACS = NULL;
		return (AE_OK);
	}

	status = acpi_get_table_by_index(ACPI_TABLE_INDEX_FACS,
					 ACPI_CAST_INDIRECT_PTR(struct
								acpi_table_header,
								&acpi_gbl_FACS));
	return status;
}
#endif				


u8 acpi_tb_tables_loaded(void)
{

	if (acpi_gbl_root_table_list.current_table_count >= 3) {
		return (TRUE);
	}

	return (FALSE);
}


static void acpi_tb_fix_string(char *string, acpi_size length)
{

	while (length && *string) {
		if (!ACPI_IS_PRINT(*string)) {
			*string = '?';
		}
		string++;
		length--;
	}
}


static void
acpi_tb_cleanup_table_header(struct acpi_table_header *out_header,
			     struct acpi_table_header *header)
{

	ACPI_MEMCPY(out_header, header, sizeof(struct acpi_table_header));

	acpi_tb_fix_string(out_header->signature, ACPI_NAME_SIZE);
	acpi_tb_fix_string(out_header->oem_id, ACPI_OEM_ID_SIZE);
	acpi_tb_fix_string(out_header->oem_table_id, ACPI_OEM_TABLE_ID_SIZE);
	acpi_tb_fix_string(out_header->asl_compiler_id, ACPI_NAME_SIZE);
}


void
acpi_tb_print_table_header(acpi_physical_address address,
			   struct acpi_table_header *header)
{
	struct acpi_table_header local_header;

	if (ACPI_COMPARE_NAME(header->signature, ACPI_SIG_FACS)) {

		

		ACPI_INFO((AE_INFO, "%4.4s %p %05X",
			   header->signature, ACPI_CAST_PTR(void, address),
			   header->length));
	} else if (ACPI_COMPARE_NAME(header->signature, ACPI_SIG_RSDP)) {

		

		ACPI_MEMCPY(local_header.oem_id,
			    ACPI_CAST_PTR(struct acpi_table_rsdp,
					  header)->oem_id, ACPI_OEM_ID_SIZE);
		acpi_tb_fix_string(local_header.oem_id, ACPI_OEM_ID_SIZE);

		ACPI_INFO((AE_INFO, "RSDP %p %05X (v%.2d %6.6s)",
			   ACPI_CAST_PTR (void, address),
			   (ACPI_CAST_PTR(struct acpi_table_rsdp, header)->
			    revision >
			    0) ? ACPI_CAST_PTR(struct acpi_table_rsdp,
					       header)->length : 20,
			   ACPI_CAST_PTR(struct acpi_table_rsdp,
					 header)->revision,
			   local_header.oem_id));
	} else {
		

		acpi_tb_cleanup_table_header(&local_header, header);

		ACPI_INFO((AE_INFO,
			   "%4.4s %p %05X (v%.2d %6.6s %8.8s %08X %4.4s %08X)",
			   local_header.signature, ACPI_CAST_PTR(void, address),
			   local_header.length, local_header.revision,
			   local_header.oem_id, local_header.oem_table_id,
			   local_header.oem_revision,
			   local_header.asl_compiler_id,
			   local_header.asl_compiler_revision));

	}
}


acpi_status acpi_tb_verify_checksum(struct acpi_table_header *table, u32 length)
{
	u8 checksum;

	

	checksum = acpi_tb_checksum(ACPI_CAST_PTR(u8, table), length);

	

	if (checksum) {
		ACPI_WARNING((AE_INFO,
			      "Incorrect checksum in table [%4.4s] - 0x%2.2X, should be 0x%2.2X",
			      table->signature, table->checksum,
			      (u8) (table->checksum - checksum)));

#if (ACPI_CHECKSUM_ABORT)

		return (AE_BAD_CHECKSUM);
#endif
	}

	return (AE_OK);
}


u8 acpi_tb_checksum(u8 *buffer, u32 length)
{
	u8 sum = 0;
	u8 *end = buffer + length;

	while (buffer < end) {
		sum = (u8) (sum + *(buffer++));
	}

	return sum;
}


void acpi_tb_check_dsdt_header(void)
{

	

	if (acpi_gbl_original_dsdt_header.length != acpi_gbl_DSDT->length ||
	    acpi_gbl_original_dsdt_header.checksum != acpi_gbl_DSDT->checksum) {
		ACPI_ERROR((AE_INFO,
			    "The DSDT has been corrupted or replaced - old, new headers below"));
		acpi_tb_print_table_header(0, &acpi_gbl_original_dsdt_header);
		acpi_tb_print_table_header(0, acpi_gbl_DSDT);

		ACPI_ERROR((AE_INFO,
			    "Please send DMI info to linux-acpi@vger.kernel.org\n"
			    "If system does not work as expected, please boot with acpi=copy_dsdt"));

		

		acpi_gbl_original_dsdt_header.length = acpi_gbl_DSDT->length;
		acpi_gbl_original_dsdt_header.checksum =
		    acpi_gbl_DSDT->checksum;
	}
}


struct acpi_table_header *acpi_tb_copy_dsdt(u32 table_index)
{
	struct acpi_table_header *new_table;
	struct acpi_table_desc *table_desc;

	table_desc = &acpi_gbl_root_table_list.tables[table_index];

	new_table = ACPI_ALLOCATE(table_desc->length);
	if (!new_table) {
		ACPI_ERROR((AE_INFO, "Could not copy DSDT of length 0x%X",
			    table_desc->length));
		return (NULL);
	}

	ACPI_MEMCPY(new_table, table_desc->pointer, table_desc->length);
	acpi_tb_delete_table(table_desc);
	table_desc->pointer = new_table;
	table_desc->flags = ACPI_TABLE_ORIGIN_ALLOCATED;

	ACPI_INFO((AE_INFO,
		   "Forced DSDT copy: length 0x%05X copied locally, original unmapped",
		   new_table->length));

	return (new_table);
}


void
acpi_tb_install_table(acpi_physical_address address,
		      char *signature, u32 table_index)
{
	struct acpi_table_header *table;
	struct acpi_table_header *final_table;
	struct acpi_table_desc *table_desc;

	if (!address) {
		ACPI_ERROR((AE_INFO,
			    "Null physical address for ACPI table [%s]",
			    signature));
		return;
	}

	

	table = acpi_os_map_memory(address, sizeof(struct acpi_table_header));
	if (!table) {
		ACPI_ERROR((AE_INFO,
			    "Could not map memory for table [%s] at %p",
			    signature, ACPI_CAST_PTR(void, address)));
		return;
	}

	

	if (signature && !ACPI_COMPARE_NAME(table->signature, signature)) {
		ACPI_ERROR((AE_INFO,
			    "Invalid signature 0x%X for ACPI table, expected [%s]",
			    *ACPI_CAST_PTR(u32, table->signature), signature));
		goto unmap_and_exit;
	}

	table_desc = &acpi_gbl_root_table_list.tables[table_index];

	table_desc->address = address;
	table_desc->pointer = NULL;
	table_desc->length = table->length;
	table_desc->flags = ACPI_TABLE_ORIGIN_MAPPED;
	ACPI_MOVE_32_TO_32(table_desc->signature.ascii, table->signature);

	final_table = acpi_tb_table_override(table, table_desc);
	if (!final_table) {
		final_table = table;	
	}

	acpi_tb_print_table_header(table_desc->address, final_table);

	

	if (table_index == ACPI_TABLE_INDEX_DSDT) {
		acpi_ut_set_integer_width(final_table->revision);
	}

	if (final_table != table) {
		acpi_tb_delete_table(table_desc);
	}

      unmap_and_exit:

	

	acpi_os_unmap_memory(table, sizeof(struct acpi_table_header));
}


static acpi_physical_address
acpi_tb_get_root_table_entry(u8 *table_entry, u32 table_entry_size)
{
	u64 address64;

	if (table_entry_size == sizeof(u32)) {
		return ((acpi_physical_address)
			(*ACPI_CAST_PTR(u32, table_entry)));
	} else {
		ACPI_MOVE_64_TO_64(&address64, table_entry);

#if ACPI_MACHINE_WIDTH == 32
		if (address64 > ACPI_UINT32_MAX) {

			

			ACPI_WARNING((AE_INFO,
				      "64-bit Physical Address in XSDT is too large (0x%8.8X%8.8X),"
				      " truncating",
				      ACPI_FORMAT_UINT64(address64)));
		}
#endif
		return ((acpi_physical_address) (address64));
	}
}


acpi_status __init
acpi_tb_parse_root_table(acpi_physical_address rsdp_address)
{
	struct acpi_table_rsdp *rsdp;
	u32 table_entry_size;
	u32 i;
	u32 table_count;
	struct acpi_table_header *table;
	acpi_physical_address address;
	acpi_physical_address uninitialized_var(rsdt_address);
	u32 length;
	u8 *table_entry;
	acpi_status status;

	ACPI_FUNCTION_TRACE(tb_parse_root_table);

	rsdp = acpi_os_map_memory(rsdp_address, sizeof(struct acpi_table_rsdp));
	if (!rsdp) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	acpi_tb_print_table_header(rsdp_address,
				   ACPI_CAST_PTR(struct acpi_table_header,
						 rsdp));

	

	if (rsdp->revision > 1 && rsdp->xsdt_physical_address
			&& !acpi_rsdt_forced) {
		address = (acpi_physical_address) rsdp->xsdt_physical_address;
		table_entry_size = sizeof(u64);
		rsdt_address = (acpi_physical_address)
					rsdp->rsdt_physical_address;
	} else {
		

		address = (acpi_physical_address) rsdp->rsdt_physical_address;
		table_entry_size = sizeof(u32);
	}

	acpi_os_unmap_memory(rsdp, sizeof(struct acpi_table_rsdp));

	if (table_entry_size == sizeof(u64)) {
		if (acpi_tb_check_xsdt(address) == AE_NULL_ENTRY) {
			
			address = rsdt_address;
			table_entry_size = sizeof(u32);
			ACPI_WARNING((AE_INFO, "BIOS XSDT has NULL entry, "
					"using RSDT"));
		}
	}
	

	table = acpi_os_map_memory(address, sizeof(struct acpi_table_header));
	if (!table) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	acpi_tb_print_table_header(address, table);

	

	length = table->length;
	acpi_os_unmap_memory(table, sizeof(struct acpi_table_header));

	if (length < sizeof(struct acpi_table_header)) {
		ACPI_ERROR((AE_INFO, "Invalid length 0x%X in RSDT/XSDT",
			    length));
		return_ACPI_STATUS(AE_INVALID_TABLE_LENGTH);
	}

	table = acpi_os_map_memory(address, length);
	if (!table) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	

	status = acpi_tb_verify_checksum(table, length);
	if (ACPI_FAILURE(status)) {
		acpi_os_unmap_memory(table, length);
		return_ACPI_STATUS(status);
	}

	

	table_count = (u32)((table->length - sizeof(struct acpi_table_header)) /
			    table_entry_size);
	table_entry =
	    ACPI_CAST_PTR(u8, table) + sizeof(struct acpi_table_header);
	acpi_gbl_root_table_list.current_table_count = 2;

	for (i = 0; i < table_count; i++) {
		if (acpi_gbl_root_table_list.current_table_count >=
		    acpi_gbl_root_table_list.max_table_count) {

			

			status = acpi_tb_resize_root_table_list();
			if (ACPI_FAILURE(status)) {
				ACPI_WARNING((AE_INFO,
					      "Truncating %u table entries!",
					      (unsigned) (table_count -
					       (acpi_gbl_root_table_list.
							  current_table_count -
							  2))));
				break;
			}
		}

		

		acpi_gbl_root_table_list.tables[acpi_gbl_root_table_list.
						current_table_count].address =
		    acpi_tb_get_root_table_entry(table_entry, table_entry_size);

		table_entry += table_entry_size;
		acpi_gbl_root_table_list.current_table_count++;
	}

	acpi_os_unmap_memory(table, length);

	for (i = 2; i < acpi_gbl_root_table_list.current_table_count; i++) {
		acpi_tb_install_table(acpi_gbl_root_table_list.tables[i].
				      address, NULL, i);

		

		if (ACPI_COMPARE_NAME
		    (&acpi_gbl_root_table_list.tables[i].signature,
		     ACPI_SIG_FADT)) {
			acpi_tb_parse_fadt(i);
		}
	}

	return_ACPI_STATUS(AE_OK);
}
