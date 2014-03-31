
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
ACPI_MODULE_NAME("tbfadt")

static ACPI_INLINE void
acpi_tb_init_generic_address(struct acpi_generic_address *generic_address,
			     u8 space_id, u8 byte_width, u64 address);

static void acpi_tb_convert_fadt(void);

static void acpi_tb_validate_fadt(void);

static void acpi_tb_setup_fadt_registers(void);


typedef struct acpi_fadt_info {
	char *name;
	u16 address64;
	u16 address32;
	u16 length;
	u8 default_length;
	u8 type;

} acpi_fadt_info;

#define ACPI_FADT_OPTIONAL          0
#define ACPI_FADT_REQUIRED          1
#define ACPI_FADT_SEPARATE_LENGTH   2

static struct acpi_fadt_info fadt_info_table[] = {
	{"Pm1aEventBlock",
	 ACPI_FADT_OFFSET(xpm1a_event_block),
	 ACPI_FADT_OFFSET(pm1a_event_block),
	 ACPI_FADT_OFFSET(pm1_event_length),
	 ACPI_PM1_REGISTER_WIDTH * 2,	
	 ACPI_FADT_REQUIRED},

	{"Pm1bEventBlock",
	 ACPI_FADT_OFFSET(xpm1b_event_block),
	 ACPI_FADT_OFFSET(pm1b_event_block),
	 ACPI_FADT_OFFSET(pm1_event_length),
	 ACPI_PM1_REGISTER_WIDTH * 2,	
	 ACPI_FADT_OPTIONAL},

	{"Pm1aControlBlock",
	 ACPI_FADT_OFFSET(xpm1a_control_block),
	 ACPI_FADT_OFFSET(pm1a_control_block),
	 ACPI_FADT_OFFSET(pm1_control_length),
	 ACPI_PM1_REGISTER_WIDTH,
	 ACPI_FADT_REQUIRED},

	{"Pm1bControlBlock",
	 ACPI_FADT_OFFSET(xpm1b_control_block),
	 ACPI_FADT_OFFSET(pm1b_control_block),
	 ACPI_FADT_OFFSET(pm1_control_length),
	 ACPI_PM1_REGISTER_WIDTH,
	 ACPI_FADT_OPTIONAL},

	{"Pm2ControlBlock",
	 ACPI_FADT_OFFSET(xpm2_control_block),
	 ACPI_FADT_OFFSET(pm2_control_block),
	 ACPI_FADT_OFFSET(pm2_control_length),
	 ACPI_PM2_REGISTER_WIDTH,
	 ACPI_FADT_SEPARATE_LENGTH},

	{"PmTimerBlock",
	 ACPI_FADT_OFFSET(xpm_timer_block),
	 ACPI_FADT_OFFSET(pm_timer_block),
	 ACPI_FADT_OFFSET(pm_timer_length),
	 ACPI_PM_TIMER_WIDTH,
	 ACPI_FADT_REQUIRED},

	{"Gpe0Block",
	 ACPI_FADT_OFFSET(xgpe0_block),
	 ACPI_FADT_OFFSET(gpe0_block),
	 ACPI_FADT_OFFSET(gpe0_block_length),
	 0,
	 ACPI_FADT_SEPARATE_LENGTH},

	{"Gpe1Block",
	 ACPI_FADT_OFFSET(xgpe1_block),
	 ACPI_FADT_OFFSET(gpe1_block),
	 ACPI_FADT_OFFSET(gpe1_block_length),
	 0,
	 ACPI_FADT_SEPARATE_LENGTH}
};

#define ACPI_FADT_INFO_ENTRIES \
			(sizeof (fadt_info_table) / sizeof (struct acpi_fadt_info))


typedef struct acpi_fadt_pm_info {
	struct acpi_generic_address *target;
	u16 source;
	u8 register_num;

} acpi_fadt_pm_info;

static struct acpi_fadt_pm_info fadt_pm_info_table[] = {
	{&acpi_gbl_xpm1a_status,
	 ACPI_FADT_OFFSET(xpm1a_event_block),
	 0},

	{&acpi_gbl_xpm1a_enable,
	 ACPI_FADT_OFFSET(xpm1a_event_block),
	 1},

	{&acpi_gbl_xpm1b_status,
	 ACPI_FADT_OFFSET(xpm1b_event_block),
	 0},

	{&acpi_gbl_xpm1b_enable,
	 ACPI_FADT_OFFSET(xpm1b_event_block),
	 1}
};

#define ACPI_FADT_PM_INFO_ENTRIES \
			(sizeof (fadt_pm_info_table) / sizeof (struct acpi_fadt_pm_info))


static ACPI_INLINE void
acpi_tb_init_generic_address(struct acpi_generic_address *generic_address,
			     u8 space_id, u8 byte_width, u64 address)
{

	ACPI_MOVE_64_TO_64(&generic_address->address, &address);

	

	generic_address->space_id = space_id;
	generic_address->bit_width = (u8)ACPI_MUL_8(byte_width);
	generic_address->bit_offset = 0;
	generic_address->access_width = 0;	
}


void acpi_tb_parse_fadt(u32 table_index)
{
	u32 length;
	struct acpi_table_header *table;

	length = acpi_gbl_root_table_list.tables[table_index].length;

	table =
	    acpi_os_map_memory(acpi_gbl_root_table_list.tables[table_index].
			       address, length);
	if (!table) {
		return;
	}

	(void)acpi_tb_verify_checksum(table, length);

	

	acpi_tb_create_local_fadt(table, length);

	

	acpi_os_unmap_memory(table, length);

	

	acpi_tb_install_table((acpi_physical_address) acpi_gbl_FADT.Xdsdt,
			      ACPI_SIG_DSDT, ACPI_TABLE_INDEX_DSDT);

	

	if (!acpi_gbl_reduced_hardware) {
		acpi_tb_install_table((acpi_physical_address) acpi_gbl_FADT.
				      Xfacs, ACPI_SIG_FACS,
				      ACPI_TABLE_INDEX_FACS);
	}
}


void acpi_tb_create_local_fadt(struct acpi_table_header *table, u32 length)
{
	if (length > sizeof(struct acpi_table_fadt)) {
		ACPI_WARNING((AE_INFO,
			      "FADT (revision %u) is longer than ACPI 5.0 version, "
			      "truncating length %u to %u",
			      table->revision, length,
			      (u32)sizeof(struct acpi_table_fadt)));
	}

	

	ACPI_MEMSET(&acpi_gbl_FADT, 0, sizeof(struct acpi_table_fadt));

	

	ACPI_MEMCPY(&acpi_gbl_FADT, table,
		    ACPI_MIN(length, sizeof(struct acpi_table_fadt)));

	

	acpi_gbl_reduced_hardware = FALSE;
	if (acpi_gbl_FADT.flags & ACPI_FADT_HW_REDUCED) {
		acpi_gbl_reduced_hardware = TRUE;
	}

	

	acpi_tb_convert_fadt();

	

	acpi_tb_validate_fadt();

	

	acpi_tb_setup_fadt_registers();
}


static void acpi_tb_convert_fadt(void)
{
	struct acpi_generic_address *address64;
	u32 address32;
	u32 i;

	if (!acpi_gbl_FADT.Xfacs) {
		acpi_gbl_FADT.Xfacs = (u64) acpi_gbl_FADT.facs;
	} else if (acpi_gbl_FADT.facs &&
		   (acpi_gbl_FADT.Xfacs != (u64) acpi_gbl_FADT.facs)) {
		ACPI_WARNING((AE_INFO,
		    "32/64 FACS address mismatch in FADT - two FACS tables!"));
	}

	if (!acpi_gbl_FADT.Xdsdt) {
		acpi_gbl_FADT.Xdsdt = (u64) acpi_gbl_FADT.dsdt;
	} else if (acpi_gbl_FADT.dsdt &&
		   (acpi_gbl_FADT.Xdsdt != (u64) acpi_gbl_FADT.dsdt)) {
		ACPI_WARNING((AE_INFO,
		    "32/64 DSDT address mismatch in FADT - two DSDT tables!"));
	}

	if (acpi_gbl_FADT.header.length <= ACPI_FADT_V2_SIZE) {
		acpi_gbl_FADT.preferred_profile = 0;
		acpi_gbl_FADT.pstate_control = 0;
		acpi_gbl_FADT.cst_control = 0;
		acpi_gbl_FADT.boot_flags = 0;
	}

	

	acpi_gbl_FADT.header.length = sizeof(struct acpi_table_fadt);

	for (i = 0; i < ACPI_FADT_INFO_ENTRIES; i++) {
		address32 = *ACPI_ADD_PTR(u32,
					  &acpi_gbl_FADT,
					  fadt_info_table[i].address32);

		address64 = ACPI_ADD_PTR(struct acpi_generic_address,
					 &acpi_gbl_FADT,
					 fadt_info_table[i].address64);

		if (address64->address && address32 &&
		    (address64->address != (u64) address32)) {
			ACPI_ERROR((AE_INFO,
				    "32/64X address mismatch in %s: 0x%8.8X/0x%8.8X%8.8X, using 32",
				    fadt_info_table[i].name, address32,
				    ACPI_FORMAT_UINT64(address64->address)));
		}

		

		if (address32) {
			acpi_tb_init_generic_address(address64,
						     ACPI_ADR_SPACE_SYSTEM_IO,
						     *ACPI_ADD_PTR(u8,
								   &acpi_gbl_FADT,
								   fadt_info_table
								   [i].length),
						     (u64) address32);
		}
	}
}


static void acpi_tb_validate_fadt(void)
{
	char *name;
	struct acpi_generic_address *address64;
	u8 length;
	u32 i;

	if (acpi_gbl_FADT.facs &&
	    (acpi_gbl_FADT.Xfacs != (u64) acpi_gbl_FADT.facs)) {
		ACPI_WARNING((AE_INFO,
			      "32/64X FACS address mismatch in FADT - "
			      "0x%8.8X/0x%8.8X%8.8X, using 32",
			      acpi_gbl_FADT.facs,
			      ACPI_FORMAT_UINT64(acpi_gbl_FADT.Xfacs)));

		acpi_gbl_FADT.Xfacs = (u64) acpi_gbl_FADT.facs;
	}

	if (acpi_gbl_FADT.dsdt &&
	    (acpi_gbl_FADT.Xdsdt != (u64) acpi_gbl_FADT.dsdt)) {
		ACPI_WARNING((AE_INFO,
			      "32/64X DSDT address mismatch in FADT - "
			      "0x%8.8X/0x%8.8X%8.8X, using 32",
			      acpi_gbl_FADT.dsdt,
			      ACPI_FORMAT_UINT64(acpi_gbl_FADT.Xdsdt)));

		acpi_gbl_FADT.Xdsdt = (u64) acpi_gbl_FADT.dsdt;
	}

	

	if (acpi_gbl_reduced_hardware) {
		return;
	}

	

	for (i = 0; i < ACPI_FADT_INFO_ENTRIES; i++) {
		address64 = ACPI_ADD_PTR(struct acpi_generic_address,
					 &acpi_gbl_FADT,
					 fadt_info_table[i].address64);
		length =
		    *ACPI_ADD_PTR(u8, &acpi_gbl_FADT,
				  fadt_info_table[i].length);
		name = fadt_info_table[i].name;

		if (address64->address &&
		    (address64->bit_width != ACPI_MUL_8(length))) {
			ACPI_WARNING((AE_INFO,
				      "32/64X length mismatch in %s: %u/%u",
				      name, ACPI_MUL_8(length),
				      address64->bit_width));
		}

		if (fadt_info_table[i].type & ACPI_FADT_REQUIRED) {
			if (!address64->address || !length) {
				ACPI_ERROR((AE_INFO,
					    "Required field %s has zero address and/or length:"
					    " 0x%8.8X%8.8X/0x%X",
					    name,
					    ACPI_FORMAT_UINT64(address64->
							       address),
					    length));
			}
		} else if (fadt_info_table[i].type & ACPI_FADT_SEPARATE_LENGTH) {
			if ((address64->address && !length) ||
			    (!address64->address && length)) {
				ACPI_WARNING((AE_INFO,
					      "Optional field %s has zero address or length: "
					      "0x%8.8X%8.8X/0x%X",
					      name,
					      ACPI_FORMAT_UINT64(address64->
								 address),
					      length));
			}
		}
	}
}


static void acpi_tb_setup_fadt_registers(void)
{
	struct acpi_generic_address *target64;
	struct acpi_generic_address *source64;
	u8 pm1_register_byte_width;
	u32 i;

	if (acpi_gbl_use_default_register_widths) {
		for (i = 0; i < ACPI_FADT_INFO_ENTRIES; i++) {
			target64 =
			    ACPI_ADD_PTR(struct acpi_generic_address,
					 &acpi_gbl_FADT,
					 fadt_info_table[i].address64);

			if ((target64->address) &&
			    (fadt_info_table[i].default_length > 0) &&
			    (fadt_info_table[i].default_length !=
			     target64->bit_width)) {
				ACPI_WARNING((AE_INFO,
					      "Invalid length for %s: %u, using default %u",
					      fadt_info_table[i].name,
					      target64->bit_width,
					      fadt_info_table[i].
					      default_length));

				

				target64->bit_width =
				    fadt_info_table[i].default_length;
			}
		}
	}

	pm1_register_byte_width = (u8)
	    ACPI_DIV_16(acpi_gbl_FADT.xpm1a_event_block.bit_width);


	for (i = 0; i < ACPI_FADT_PM_INFO_ENTRIES; i++) {
		source64 =
		    ACPI_ADD_PTR(struct acpi_generic_address, &acpi_gbl_FADT,
				 fadt_pm_info_table[i].source);

		if (source64->address) {
			acpi_tb_init_generic_address(fadt_pm_info_table[i].
						     target, source64->space_id,
						     pm1_register_byte_width,
						     source64->address +
						     (fadt_pm_info_table[i].
						      register_num *
						      pm1_register_byte_width));
		}
	}
}
