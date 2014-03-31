
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
#include <linux/module.h>

#define _COMPONENT          ACPI_HARDWARE
ACPI_MODULE_NAME("hwxfsleep")

static acpi_status
acpi_hw_sleep_dispatch(u8 sleep_state, u8 flags, u32 function_id);

#define ACPI_SLEEP_FUNCTION_ID         0
#define ACPI_WAKE_PREP_FUNCTION_ID     1
#define ACPI_WAKE_FUNCTION_ID          2


static struct acpi_sleep_functions acpi_sleep_dispatch[] = {
	{ACPI_HW_OPTIONAL_FUNCTION(acpi_hw_legacy_sleep),
	 acpi_hw_extended_sleep},
	{ACPI_HW_OPTIONAL_FUNCTION(acpi_hw_legacy_wake_prep),
	 acpi_hw_extended_wake_prep},
	{ACPI_HW_OPTIONAL_FUNCTION(acpi_hw_legacy_wake), acpi_hw_extended_wake}
};


#if (!ACPI_REDUCED_HARDWARE)

acpi_status acpi_set_firmware_waking_vector(u32 physical_address)
{
	ACPI_FUNCTION_TRACE(acpi_set_firmware_waking_vector);



	

	acpi_gbl_FACS->firmware_waking_vector = physical_address;

	

	if ((acpi_gbl_FACS->length > 32) && (acpi_gbl_FACS->version >= 1)) {
		acpi_gbl_FACS->xfirmware_waking_vector = 0;
	}

	return_ACPI_STATUS(AE_OK);
}

ACPI_EXPORT_SYMBOL(acpi_set_firmware_waking_vector)

#if ACPI_MACHINE_WIDTH == 64
acpi_status acpi_set_firmware_waking_vector64(u64 physical_address)
{
	ACPI_FUNCTION_TRACE(acpi_set_firmware_waking_vector64);


	

	if ((acpi_gbl_FACS->length <= 32) || (acpi_gbl_FACS->version < 1)) {
		return_ACPI_STATUS(AE_NOT_EXIST);
	}

	

	acpi_gbl_FACS->firmware_waking_vector = 0;
	acpi_gbl_FACS->xfirmware_waking_vector = physical_address;
	return_ACPI_STATUS(AE_OK);
}

ACPI_EXPORT_SYMBOL(acpi_set_firmware_waking_vector64)
#endif

acpi_status asmlinkage acpi_enter_sleep_state_s4bios(void)
{
	u32 in_value;
	acpi_status status;

	ACPI_FUNCTION_TRACE(acpi_enter_sleep_state_s4bios);

	

	status =
	    acpi_write_bit_register(ACPI_BITREG_WAKE_STATUS, ACPI_CLEAR_STATUS);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	status = acpi_hw_clear_acpi_status();
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	status = acpi_hw_disable_all_gpes();
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}
	acpi_gbl_system_awake_and_running = FALSE;

	status = acpi_hw_enable_all_wakeup_gpes();
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	ACPI_FLUSH_CPU_CACHE();

	status = acpi_hw_write_port(acpi_gbl_FADT.smi_command,
				    (u32)acpi_gbl_FADT.S4bios_request, 8);

	do {
		acpi_os_stall(1000);
		status =
		    acpi_read_bit_register(ACPI_BITREG_WAKE_STATUS, &in_value);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}
	} while (!in_value);

	return_ACPI_STATUS(AE_OK);
}

ACPI_EXPORT_SYMBOL(acpi_enter_sleep_state_s4bios)
#endif				
static acpi_status
acpi_hw_sleep_dispatch(u8 sleep_state, u8 flags, u32 function_id)
{
	acpi_status status;
	struct acpi_sleep_functions *sleep_functions =
	    &acpi_sleep_dispatch[function_id];

#if (!ACPI_REDUCED_HARDWARE)

	if (acpi_gbl_reduced_hardware || acpi_gbl_FADT.sleep_control.address) {
		status = sleep_functions->extended_function(sleep_state, flags);
	} else {
		

		status = sleep_functions->legacy_function(sleep_state, flags);
	}

	return (status);

#else
	status = sleep_functions->extended_function(sleep_state, flags);
	return (status);

#endif				
}


acpi_status acpi_enter_sleep_state_prep(u8 sleep_state)
{
	acpi_status status;
	struct acpi_object_list arg_list;
	union acpi_object arg;
	u32 sst_value;

	ACPI_FUNCTION_TRACE(acpi_enter_sleep_state_prep);

	status = acpi_get_sleep_type_data(sleep_state,
					  &acpi_gbl_sleep_type_a,
					  &acpi_gbl_sleep_type_b);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	

	arg_list.count = 1;
	arg_list.pointer = &arg;
	arg.type = ACPI_TYPE_INTEGER;
	arg.integer.value = sleep_state;

	status =
	    acpi_evaluate_object(NULL, METHOD_PATHNAME__PTS, &arg_list, NULL);
	if (ACPI_FAILURE(status) && status != AE_NOT_FOUND) {
		return_ACPI_STATUS(status);
	}

	

	switch (sleep_state) {
	case ACPI_STATE_S0:
		sst_value = ACPI_SST_WORKING;
		break;

	case ACPI_STATE_S1:
	case ACPI_STATE_S2:
	case ACPI_STATE_S3:
		sst_value = ACPI_SST_SLEEPING;
		break;

	case ACPI_STATE_S4:
		sst_value = ACPI_SST_SLEEP_CONTEXT;
		break;

	default:
		sst_value = ACPI_SST_INDICATOR_OFF;	
		break;
	}

	acpi_hw_execute_sleep_method(METHOD_PATHNAME__SST, sst_value);
	return_ACPI_STATUS(AE_OK);
}

ACPI_EXPORT_SYMBOL(acpi_enter_sleep_state_prep)

acpi_status asmlinkage acpi_enter_sleep_state(u8 sleep_state, u8 flags)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(acpi_enter_sleep_state);

	if ((acpi_gbl_sleep_type_a > ACPI_SLEEP_TYPE_MAX) ||
	    (acpi_gbl_sleep_type_b > ACPI_SLEEP_TYPE_MAX)) {
		ACPI_ERROR((AE_INFO, "Sleep values out of range: A=0x%X B=0x%X",
			    acpi_gbl_sleep_type_a, acpi_gbl_sleep_type_b));
		return_ACPI_STATUS(AE_AML_OPERAND_VALUE);
	}

	status =
	    acpi_hw_sleep_dispatch(sleep_state, flags, ACPI_SLEEP_FUNCTION_ID);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_enter_sleep_state)

acpi_status acpi_leave_sleep_state_prep(u8 sleep_state, u8 flags)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(acpi_leave_sleep_state_prep);

	status =
	    acpi_hw_sleep_dispatch(sleep_state, flags,
				   ACPI_WAKE_PREP_FUNCTION_ID);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_leave_sleep_state_prep)

acpi_status acpi_leave_sleep_state(u8 sleep_state)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(acpi_leave_sleep_state);


	status = acpi_hw_sleep_dispatch(sleep_state, 0, ACPI_WAKE_FUNCTION_ID);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_leave_sleep_state)
