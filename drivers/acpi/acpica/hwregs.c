

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
#include "acevents.h"

#define _COMPONENT          ACPI_HARDWARE
ACPI_MODULE_NAME("hwregs")

#if (!ACPI_REDUCED_HARDWARE)
static acpi_status
acpi_hw_read_multiple(u32 *value,
		      struct acpi_generic_address *register_a,
		      struct acpi_generic_address *register_b);

static acpi_status
acpi_hw_write_multiple(u32 value,
		       struct acpi_generic_address *register_a,
		       struct acpi_generic_address *register_b);

#endif				


acpi_status
acpi_hw_validate_register(struct acpi_generic_address *reg,
			  u8 max_bit_width, u64 *address)
{

	

	if (!reg) {
		return (AE_BAD_PARAMETER);
	}

	ACPI_MOVE_64_TO_64(address, &reg->address);
	if (!(*address)) {
		return (AE_BAD_ADDRESS);
	}

	

	if ((reg->space_id != ACPI_ADR_SPACE_SYSTEM_MEMORY) &&
	    (reg->space_id != ACPI_ADR_SPACE_SYSTEM_IO)) {
		ACPI_ERROR((AE_INFO,
			    "Unsupported address space: 0x%X", reg->space_id));
		return (AE_SUPPORT);
	}

	

	if ((reg->bit_width != 8) &&
	    (reg->bit_width != 16) &&
	    (reg->bit_width != 32) && (reg->bit_width != max_bit_width)) {
		ACPI_ERROR((AE_INFO,
			    "Unsupported register bit width: 0x%X",
			    reg->bit_width));
		return (AE_SUPPORT);
	}

	

	if (reg->bit_offset != 0) {
		ACPI_WARNING((AE_INFO,
			      "Unsupported register bit offset: 0x%X",
			      reg->bit_offset));
	}

	return (AE_OK);
}


acpi_status acpi_hw_read(u32 *value, struct acpi_generic_address *reg)
{
	u64 address;
	u64 value64;
	acpi_status status;

	ACPI_FUNCTION_NAME(hw_read);

	

	status = acpi_hw_validate_register(reg, 32, &address);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	

	*value = 0;

	if (reg->space_id == ACPI_ADR_SPACE_SYSTEM_MEMORY) {
		status = acpi_os_read_memory((acpi_physical_address)
					     address, &value64, reg->bit_width);

		*value = (u32)value64;
	} else {		

		status = acpi_hw_read_port((acpi_io_address)
					   address, value, reg->bit_width);
	}

	ACPI_DEBUG_PRINT((ACPI_DB_IO,
			  "Read:  %8.8X width %2d from %8.8X%8.8X (%s)\n",
			  *value, reg->bit_width, ACPI_FORMAT_UINT64(address),
			  acpi_ut_get_region_name(reg->space_id)));

	return (status);
}

/******************************************************************************
 *
 * FUNCTION:    acpi_hw_write
 *
 * PARAMETERS:  Value               - Value to be written
 *              Reg                 - GAS register structure
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Write to either memory or IO space. This is a 32-bit max
 *              version of acpi_write, used internally since the overhead of
 *              64-bit values is not needed.
 *
 ******************************************************************************/

acpi_status acpi_hw_write(u32 value, struct acpi_generic_address *reg)
{
	u64 address;
	acpi_status status;

	ACPI_FUNCTION_NAME(hw_write);

	

	status = acpi_hw_validate_register(reg, 32, &address);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	if (reg->space_id == ACPI_ADR_SPACE_SYSTEM_MEMORY) {
		status = acpi_os_write_memory((acpi_physical_address)
					      address, (u64)value,
					      reg->bit_width);
	} else {		

		status = acpi_hw_write_port((acpi_io_address)
					    address, value, reg->bit_width);
	}

	ACPI_DEBUG_PRINT((ACPI_DB_IO,
			  "Wrote: %8.8X width %2d   to %8.8X%8.8X (%s)\n",
			  value, reg->bit_width, ACPI_FORMAT_UINT64(address),
			  acpi_ut_get_region_name(reg->space_id)));

	return (status);
}

#if (!ACPI_REDUCED_HARDWARE)

acpi_status acpi_hw_clear_acpi_status(void)
{
	acpi_status status;
	acpi_cpu_flags lock_flags = 0;

	ACPI_FUNCTION_TRACE(hw_clear_acpi_status);

	ACPI_DEBUG_PRINT((ACPI_DB_IO, "About to write %04X to %8.8X%8.8X\n",
			  ACPI_BITMASK_ALL_FIXED_STATUS,
			  ACPI_FORMAT_UINT64(acpi_gbl_xpm1a_status.address)));

	lock_flags = acpi_os_acquire_lock(acpi_gbl_hardware_lock);

	

	status = acpi_hw_register_write(ACPI_REGISTER_PM1_STATUS,
					ACPI_BITMASK_ALL_FIXED_STATUS);

	acpi_os_release_lock(acpi_gbl_hardware_lock, lock_flags);

	if (ACPI_FAILURE(status))
		goto exit;

	

	status = acpi_ev_walk_gpe_list(acpi_hw_clear_gpe_block, NULL);

exit:
	return_ACPI_STATUS(status);
}


struct acpi_bit_register_info *acpi_hw_get_bit_register_info(u32 register_id)
{
	ACPI_FUNCTION_ENTRY();

	if (register_id > ACPI_BITREG_MAX) {
		ACPI_ERROR((AE_INFO, "Invalid BitRegister ID: 0x%X",
			    register_id));
		return (NULL);
	}

	return (&acpi_gbl_bit_register_info[register_id]);
}

/******************************************************************************
 *
 * FUNCTION:    acpi_hw_write_pm1_control
 *
 * PARAMETERS:  pm1a_control        - Value to be written to PM1A control
 *              pm1b_control        - Value to be written to PM1B control
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Write the PM1 A/B control registers. These registers are
 *              different than than the PM1 A/B status and enable registers
 *              in that different values can be written to the A/B registers.
 *              Most notably, the SLP_TYP bits can be different, as per the
 *              values returned from the _Sx predefined methods.
 *
 ******************************************************************************/

acpi_status acpi_hw_write_pm1_control(u32 pm1a_control, u32 pm1b_control)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(hw_write_pm1_control);

	status =
	    acpi_hw_write(pm1a_control, &acpi_gbl_FADT.xpm1a_control_block);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	if (acpi_gbl_FADT.xpm1b_control_block.address) {
		status =
		    acpi_hw_write(pm1b_control,
				  &acpi_gbl_FADT.xpm1b_control_block);
	}
	return_ACPI_STATUS(status);
}

acpi_status
acpi_hw_register_read(u32 register_id, u32 * return_value)
{
	u32 value = 0;
	acpi_status status;

	ACPI_FUNCTION_TRACE(hw_register_read);

	switch (register_id) {
	case ACPI_REGISTER_PM1_STATUS:	

		status = acpi_hw_read_multiple(&value,
					       &acpi_gbl_xpm1a_status,
					       &acpi_gbl_xpm1b_status);
		break;

	case ACPI_REGISTER_PM1_ENABLE:	

		status = acpi_hw_read_multiple(&value,
					       &acpi_gbl_xpm1a_enable,
					       &acpi_gbl_xpm1b_enable);
		break;

	case ACPI_REGISTER_PM1_CONTROL:	

		status = acpi_hw_read_multiple(&value,
					       &acpi_gbl_FADT.
					       xpm1a_control_block,
					       &acpi_gbl_FADT.
					       xpm1b_control_block);

		value &= ~ACPI_PM1_CONTROL_WRITEONLY_BITS;
		break;

	case ACPI_REGISTER_PM2_CONTROL:	

		status =
		    acpi_hw_read(&value, &acpi_gbl_FADT.xpm2_control_block);
		break;

	case ACPI_REGISTER_PM_TIMER:	

		status = acpi_hw_read(&value, &acpi_gbl_FADT.xpm_timer_block);
		break;

	case ACPI_REGISTER_SMI_COMMAND_BLOCK:	

		status =
		    acpi_hw_read_port(acpi_gbl_FADT.smi_command, &value, 8);
		break;

	default:
		ACPI_ERROR((AE_INFO, "Unknown Register ID: 0x%X", register_id));
		status = AE_BAD_PARAMETER;
		break;
	}

	if (ACPI_SUCCESS(status)) {
		*return_value = value;
	}

	return_ACPI_STATUS(status);
}


acpi_status acpi_hw_register_write(u32 register_id, u32 value)
{
	acpi_status status;
	u32 read_value;

	ACPI_FUNCTION_TRACE(hw_register_write);

	switch (register_id) {
	case ACPI_REGISTER_PM1_STATUS:	
		value &= ~ACPI_PM1_STATUS_PRESERVED_BITS;

		status = acpi_hw_write_multiple(value,
						&acpi_gbl_xpm1a_status,
						&acpi_gbl_xpm1b_status);
		break;

	case ACPI_REGISTER_PM1_ENABLE:	

		status = acpi_hw_write_multiple(value,
						&acpi_gbl_xpm1a_enable,
						&acpi_gbl_xpm1b_enable);
		break;

	case ACPI_REGISTER_PM1_CONTROL:	

		status = acpi_hw_read_multiple(&read_value,
					       &acpi_gbl_FADT.
					       xpm1a_control_block,
					       &acpi_gbl_FADT.
					       xpm1b_control_block);
		if (ACPI_FAILURE(status)) {
			goto exit;
		}

		

		ACPI_INSERT_BITS(value, ACPI_PM1_CONTROL_PRESERVED_BITS,
				 read_value);

		

		status = acpi_hw_write_multiple(value,
						&acpi_gbl_FADT.
						xpm1a_control_block,
						&acpi_gbl_FADT.
						xpm1b_control_block);
		break;

	case ACPI_REGISTER_PM2_CONTROL:	

		status =
		    acpi_hw_read(&read_value,
				 &acpi_gbl_FADT.xpm2_control_block);
		if (ACPI_FAILURE(status)) {
			goto exit;
		}

		

		ACPI_INSERT_BITS(value, ACPI_PM2_CONTROL_PRESERVED_BITS,
				 read_value);

		status =
		    acpi_hw_write(value, &acpi_gbl_FADT.xpm2_control_block);
		break;

	case ACPI_REGISTER_PM_TIMER:	

		status = acpi_hw_write(value, &acpi_gbl_FADT.xpm_timer_block);
		break;

	case ACPI_REGISTER_SMI_COMMAND_BLOCK:	

		

		status =
		    acpi_hw_write_port(acpi_gbl_FADT.smi_command, value, 8);
		break;

	default:
		ACPI_ERROR((AE_INFO, "Unknown Register ID: 0x%X", register_id));
		status = AE_BAD_PARAMETER;
		break;
	}

      exit:
	return_ACPI_STATUS(status);
}


static acpi_status
acpi_hw_read_multiple(u32 *value,
		      struct acpi_generic_address *register_a,
		      struct acpi_generic_address *register_b)
{
	u32 value_a = 0;
	u32 value_b = 0;
	acpi_status status;

	

	status = acpi_hw_read(&value_a, register_a);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	

	if (register_b->address) {
		status = acpi_hw_read(&value_b, register_b);
		if (ACPI_FAILURE(status)) {
			return (status);
		}
	}

	*value = (value_a | value_b);
	return (AE_OK);
}


static acpi_status
acpi_hw_write_multiple(u32 value,
		       struct acpi_generic_address *register_a,
		       struct acpi_generic_address *register_b)
{
	acpi_status status;

	

	status = acpi_hw_write(value, register_a);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	if (register_b->address) {
		status = acpi_hw_write(value, register_b);
	}

	return (status);
}

#endif				
