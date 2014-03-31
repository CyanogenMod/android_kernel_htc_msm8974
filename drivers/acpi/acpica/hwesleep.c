
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

#define _COMPONENT          ACPI_HARDWARE
ACPI_MODULE_NAME("hwesleep")

void acpi_hw_execute_sleep_method(char *method_pathname, u32 integer_argument)
{
	struct acpi_object_list arg_list;
	union acpi_object arg;
	acpi_status status;

	ACPI_FUNCTION_TRACE(hw_execute_sleep_method);

	

	arg_list.count = 1;
	arg_list.pointer = &arg;
	arg.type = ACPI_TYPE_INTEGER;
	arg.integer.value = (u64)integer_argument;

	status = acpi_evaluate_object(NULL, method_pathname, &arg_list, NULL);
	if (ACPI_FAILURE(status) && status != AE_NOT_FOUND) {
		ACPI_EXCEPTION((AE_INFO, status, "While executing method %s",
				method_pathname));
	}

	return_VOID;
}


acpi_status acpi_hw_extended_sleep(u8 sleep_state, u8 flags)
{
	acpi_status status;
	u8 sleep_type_value;
	u64 sleep_status;

	ACPI_FUNCTION_TRACE(hw_extended_sleep);

	

	if (!acpi_gbl_FADT.sleep_control.address ||
	    !acpi_gbl_FADT.sleep_status.address) {
		return_ACPI_STATUS(AE_NOT_EXIST);
	}

	

	status = acpi_write(ACPI_X_WAKE_STATUS, &acpi_gbl_FADT.sleep_status);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	acpi_gbl_system_awake_and_running = FALSE;

	

	if (flags & ACPI_EXECUTE_GTS) {
		acpi_hw_execute_sleep_method(METHOD_PATHNAME__GTS, sleep_state);
	}

	

	ACPI_FLUSH_CPU_CACHE();

	ACPI_DEBUG_PRINT((ACPI_DB_INIT,
			  "Entering sleep state [S%u]\n", sleep_state));

	sleep_type_value =
	    ((acpi_gbl_sleep_type_a << ACPI_X_SLEEP_TYPE_POSITION) &
	     ACPI_X_SLEEP_TYPE_MASK);

	status = acpi_write((sleep_type_value | ACPI_X_SLEEP_ENABLE),
			    &acpi_gbl_FADT.sleep_control);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	

	do {
		status = acpi_read(&sleep_status, &acpi_gbl_FADT.sleep_status);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

	} while (!(((u8)sleep_status) & ACPI_X_WAKE_STATUS));

	return_ACPI_STATUS(AE_OK);
}


acpi_status acpi_hw_extended_wake_prep(u8 sleep_state, u8 flags)
{
	acpi_status status;
	u8 sleep_type_value;

	ACPI_FUNCTION_TRACE(hw_extended_wake_prep);

	status = acpi_get_sleep_type_data(ACPI_STATE_S0,
					  &acpi_gbl_sleep_type_a,
					  &acpi_gbl_sleep_type_b);
	if (ACPI_SUCCESS(status)) {
		sleep_type_value =
		    ((acpi_gbl_sleep_type_a << ACPI_X_SLEEP_TYPE_POSITION) &
		     ACPI_X_SLEEP_TYPE_MASK);

		(void)acpi_write((sleep_type_value | ACPI_X_SLEEP_ENABLE),
				 &acpi_gbl_FADT.sleep_control);
	}

	

	if (flags & ACPI_EXECUTE_BFS) {
		acpi_hw_execute_sleep_method(METHOD_PATHNAME__BFS, sleep_state);
	}
	return_ACPI_STATUS(AE_OK);
}


acpi_status acpi_hw_extended_wake(u8 sleep_state, u8 flags)
{
	ACPI_FUNCTION_TRACE(hw_extended_wake);

	

	acpi_gbl_sleep_type_a = ACPI_SLEEP_TYPE_INVALID;

	

	acpi_hw_execute_sleep_method(METHOD_PATHNAME__SST, ACPI_SST_WAKING);
	acpi_hw_execute_sleep_method(METHOD_PATHNAME__WAK, sleep_state);

	(void)acpi_write(ACPI_X_WAKE_STATUS, &acpi_gbl_FADT.sleep_status);
	acpi_gbl_system_awake_and_running = TRUE;

	acpi_hw_execute_sleep_method(METHOD_PATHNAME__SST, ACPI_SST_WORKING);
	return_ACPI_STATUS(AE_OK);
}
