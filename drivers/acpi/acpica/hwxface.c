

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

#include <linux/export.h>
#include <acpi/acpi.h>
#include "accommon.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_HARDWARE
ACPI_MODULE_NAME("hwxface")

acpi_status acpi_reset(void)
{
	struct acpi_generic_address *reset_reg;
	acpi_status status;

	ACPI_FUNCTION_TRACE(acpi_reset);

	reset_reg = &acpi_gbl_FADT.reset_register;

	

	if (!(acpi_gbl_FADT.flags & ACPI_FADT_RESET_REGISTER) ||
	    !reset_reg->address) {
		return_ACPI_STATUS(AE_NOT_EXIST);
	}

	if (reset_reg->space_id == ACPI_ADR_SPACE_SYSTEM_IO) {
		status =
		    acpi_os_write_port((acpi_io_address) reset_reg->address,
				       acpi_gbl_FADT.reset_value, 8);
	} else {
		

		status = acpi_hw_write(acpi_gbl_FADT.reset_value, reset_reg);
	}

	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_reset)

acpi_status acpi_read(u64 *return_value, struct acpi_generic_address *reg)
{
	u32 value;
	u32 width;
	u64 address;
	acpi_status status;

	ACPI_FUNCTION_NAME(acpi_read);

	if (!return_value) {
		return (AE_BAD_PARAMETER);
	}

	

	status = acpi_hw_validate_register(reg, 64, &address);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	

	*return_value = 0;
	value = 0;

	if (reg->space_id == ACPI_ADR_SPACE_SYSTEM_MEMORY) {
		status = acpi_os_read_memory((acpi_physical_address)
					     address, return_value,
					     reg->bit_width);
		if (ACPI_FAILURE(status)) {
			return (status);
		}
	} else {		

		width = reg->bit_width;
		if (width == 64) {
			width = 32;	
		}

		status = acpi_hw_read_port((acpi_io_address)
					   address, &value, width);
		if (ACPI_FAILURE(status)) {
			return (status);
		}
		*return_value = value;

		if (reg->bit_width == 64) {

			

			status = acpi_hw_read_port((acpi_io_address)
						   (address + 4), &value, 32);
			if (ACPI_FAILURE(status)) {
				return (status);
			}
			*return_value |= ((u64)value << 32);
		}
	}

	ACPI_DEBUG_PRINT((ACPI_DB_IO,
			  "Read:  %8.8X%8.8X width %2d from %8.8X%8.8X (%s)\n",
			  ACPI_FORMAT_UINT64(*return_value), reg->bit_width,
			  ACPI_FORMAT_UINT64(address),
			  acpi_ut_get_region_name(reg->space_id)));

	return (status);
}

ACPI_EXPORT_SYMBOL(acpi_read)

/******************************************************************************
 *
 * FUNCTION:    acpi_write
 *
 * PARAMETERS:  Value               - Value to be written
 *              Reg                 - GAS register structure
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Write to either memory or IO space.
 *
 ******************************************************************************/
acpi_status acpi_write(u64 value, struct acpi_generic_address *reg)
{
	u32 width;
	u64 address;
	acpi_status status;

	ACPI_FUNCTION_NAME(acpi_write);

	

	status = acpi_hw_validate_register(reg, 64, &address);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	if (reg->space_id == ACPI_ADR_SPACE_SYSTEM_MEMORY) {
		status = acpi_os_write_memory((acpi_physical_address)
					      address, value, reg->bit_width);
		if (ACPI_FAILURE(status)) {
			return (status);
		}
	} else {		

		width = reg->bit_width;
		if (width == 64) {
			width = 32;	
		}

		status = acpi_hw_write_port((acpi_io_address)
					    address, ACPI_LODWORD(value),
					    width);
		if (ACPI_FAILURE(status)) {
			return (status);
		}

		if (reg->bit_width == 64) {
			status = acpi_hw_write_port((acpi_io_address)
						    (address + 4),
						    ACPI_HIDWORD(value), 32);
			if (ACPI_FAILURE(status)) {
				return (status);
			}
		}
	}

	ACPI_DEBUG_PRINT((ACPI_DB_IO,
			  "Wrote: %8.8X%8.8X width %2d   to %8.8X%8.8X (%s)\n",
			  ACPI_FORMAT_UINT64(value), reg->bit_width,
			  ACPI_FORMAT_UINT64(address),
			  acpi_ut_get_region_name(reg->space_id)));

	return (status);
}

ACPI_EXPORT_SYMBOL(acpi_write)

#if (!ACPI_REDUCED_HARDWARE)
acpi_status acpi_read_bit_register(u32 register_id, u32 *return_value)
{
	struct acpi_bit_register_info *bit_reg_info;
	u32 register_value;
	u32 value;
	acpi_status status;

	ACPI_FUNCTION_TRACE_U32(acpi_read_bit_register, register_id);

	

	bit_reg_info = acpi_hw_get_bit_register_info(register_id);
	if (!bit_reg_info) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	

	status = acpi_hw_register_read(bit_reg_info->parent_register,
				       &register_value);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	

	value = ((register_value & bit_reg_info->access_bit_mask)
		 >> bit_reg_info->bit_position);

	ACPI_DEBUG_PRINT((ACPI_DB_IO,
			  "BitReg %X, ParentReg %X, Actual %8.8X, ReturnValue %8.8X\n",
			  register_id, bit_reg_info->parent_register,
			  register_value, value));

	*return_value = value;
	return_ACPI_STATUS(AE_OK);
}

ACPI_EXPORT_SYMBOL(acpi_read_bit_register)

acpi_status acpi_write_bit_register(u32 register_id, u32 value)
{
	struct acpi_bit_register_info *bit_reg_info;
	acpi_cpu_flags lock_flags;
	u32 register_value;
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE_U32(acpi_write_bit_register, register_id);

	

	bit_reg_info = acpi_hw_get_bit_register_info(register_id);
	if (!bit_reg_info) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	lock_flags = acpi_os_acquire_lock(acpi_gbl_hardware_lock);

	if (bit_reg_info->parent_register != ACPI_REGISTER_PM1_STATUS) {
		status = acpi_hw_register_read(bit_reg_info->parent_register,
					       &register_value);
		if (ACPI_FAILURE(status)) {
			goto unlock_and_exit;
		}

		ACPI_REGISTER_INSERT_VALUE(register_value,
					   bit_reg_info->bit_position,
					   bit_reg_info->access_bit_mask,
					   value);

		status = acpi_hw_register_write(bit_reg_info->parent_register,
						register_value);
	} else {
		/*
		 * 2) Case for PM1 Status
		 *
		 * The Status register is different from the rest. Clear an event
		 * by writing 1, writing 0 has no effect. So, the only relevant
		 * information is the single bit we're interested in, all others
		 * should be written as 0 so they will be left unchanged.
		 */
		register_value = ACPI_REGISTER_PREPARE_BITS(value,
							    bit_reg_info->
							    bit_position,
							    bit_reg_info->
							    access_bit_mask);

		

		if (register_value) {
			status =
			    acpi_hw_register_write(ACPI_REGISTER_PM1_STATUS,
						   register_value);
		}
	}

	ACPI_DEBUG_PRINT((ACPI_DB_IO,
			  "BitReg %X, ParentReg %X, Value %8.8X, Actual %8.8X\n",
			  register_id, bit_reg_info->parent_register, value,
			  register_value));

unlock_and_exit:

	acpi_os_release_lock(acpi_gbl_hardware_lock, lock_flags);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_write_bit_register)
#endif				
acpi_status
acpi_get_sleep_type_data(u8 sleep_state, u8 *sleep_type_a, u8 *sleep_type_b)
{
	acpi_status status = AE_OK;
	struct acpi_evaluate_info *info;

	ACPI_FUNCTION_TRACE(acpi_get_sleep_type_data);

	

	if ((sleep_state > ACPI_S_STATES_MAX) || !sleep_type_a || !sleep_type_b) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	

	info = ACPI_ALLOCATE_ZEROED(sizeof(struct acpi_evaluate_info));
	if (!info) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	info->pathname =
	    ACPI_CAST_PTR(char, acpi_gbl_sleep_state_names[sleep_state]);

	

	status = acpi_ns_evaluate(info);
	if (ACPI_FAILURE(status)) {
		ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
				  "%s while evaluating SleepState [%s]\n",
				  acpi_format_exception(status),
				  info->pathname));

		goto cleanup;
	}

	

	if (!info->return_object) {
		ACPI_ERROR((AE_INFO, "No Sleep State object returned from [%s]",
			    info->pathname));
		status = AE_NOT_EXIST;
	}

	

	else if (info->return_object->common.type != ACPI_TYPE_PACKAGE) {
		ACPI_ERROR((AE_INFO,
			    "Sleep State return object is not a Package"));
		status = AE_AML_OPERAND_TYPE;
	}

	else if (info->return_object->package.count < 2) {
		ACPI_ERROR((AE_INFO,
			    "Sleep State return package does not have at least two elements"));
		status = AE_AML_NO_OPERAND;
	}

	

	else if (((info->return_object->package.elements[0])->common.type
		  != ACPI_TYPE_INTEGER) ||
		 ((info->return_object->package.elements[1])->common.type
		  != ACPI_TYPE_INTEGER)) {
		ACPI_ERROR((AE_INFO,
			    "Sleep State return package elements are not both Integers "
			    "(%s, %s)",
			    acpi_ut_get_object_type_name(info->return_object->
							 package.elements[0]),
			    acpi_ut_get_object_type_name(info->return_object->
							 package.elements[1])));
		status = AE_AML_OPERAND_TYPE;
	} else {
		

		*sleep_type_a = (u8)
		    (info->return_object->package.elements[0])->integer.value;
		*sleep_type_b = (u8)
		    (info->return_object->package.elements[1])->integer.value;
	}

	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status,
				"While evaluating SleepState [%s], bad Sleep object %p type %s",
				info->pathname, info->return_object,
				acpi_ut_get_object_type_name(info->
							     return_object)));
	}

	acpi_ut_remove_reference(info->return_object);

      cleanup:
	ACPI_FREE(info);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_get_sleep_type_data)
