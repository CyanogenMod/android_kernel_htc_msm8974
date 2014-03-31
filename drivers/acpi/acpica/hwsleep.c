
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
#include <linux/acpi.h>
#include "accommon.h"
#include <linux/module.h>

#define _COMPONENT          ACPI_HARDWARE
ACPI_MODULE_NAME("hwsleep")

#if (!ACPI_REDUCED_HARDWARE)	
acpi_status acpi_hw_legacy_sleep(u8 sleep_state, u8 flags)
{
	struct acpi_bit_register_info *sleep_type_reg_info;
	struct acpi_bit_register_info *sleep_enable_reg_info;
	u32 pm1a_control;
	u32 pm1b_control;
	u32 in_value;
	acpi_status status;

	ACPI_FUNCTION_TRACE(hw_legacy_sleep);

	sleep_type_reg_info =
	    acpi_hw_get_bit_register_info(ACPI_BITREG_SLEEP_TYPE);
	sleep_enable_reg_info =
	    acpi_hw_get_bit_register_info(ACPI_BITREG_SLEEP_ENABLE);

	

	status =
	    acpi_write_bit_register(ACPI_BITREG_WAKE_STATUS, ACPI_CLEAR_STATUS);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	

	status = acpi_hw_clear_acpi_status();
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	if (sleep_state != ACPI_STATE_S5) {
		status = acpi_write_bit_register(ACPI_BITREG_ARB_DISABLE, 1);
		if (ACPI_FAILURE(status) && (status != AE_BAD_ADDRESS)) {
			return_ACPI_STATUS(status);
		}
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

	

	if (flags & ACPI_EXECUTE_GTS) {
		acpi_hw_execute_sleep_method(METHOD_PATHNAME__GTS, sleep_state);
	}

	

	status = acpi_hw_register_read(ACPI_REGISTER_PM1_CONTROL,
				       &pm1a_control);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}
	ACPI_DEBUG_PRINT((ACPI_DB_INIT,
			  "Entering sleep state [S%u]\n", sleep_state));

	

	pm1a_control &= ~(sleep_type_reg_info->access_bit_mask |
			  sleep_enable_reg_info->access_bit_mask);
	pm1b_control = pm1a_control;

	

	pm1a_control |=
	    (acpi_gbl_sleep_type_a << sleep_type_reg_info->bit_position);
	pm1b_control |=
	    (acpi_gbl_sleep_type_b << sleep_type_reg_info->bit_position);


	

	status = acpi_hw_write_pm1_control(pm1a_control, pm1b_control);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	

	pm1a_control |= sleep_enable_reg_info->access_bit_mask;
	pm1b_control |= sleep_enable_reg_info->access_bit_mask;

	

	ACPI_FLUSH_CPU_CACHE();

	status = acpi_os_prepare_sleep(sleep_state, pm1a_control,
				       pm1b_control);
	if (ACPI_SKIP(status))
		return_ACPI_STATUS(AE_OK);
	if (ACPI_FAILURE(status))
		return_ACPI_STATUS(status);
	

	status = acpi_hw_write_pm1_control(pm1a_control, pm1b_control);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	if (sleep_state > ACPI_STATE_S3) {
		acpi_os_stall(10000000);

		status = acpi_hw_register_write(ACPI_REGISTER_PM1_CONTROL,
						sleep_enable_reg_info->
						access_bit_mask);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}
	}

	

	do {
		status =
		    acpi_read_bit_register(ACPI_BITREG_WAKE_STATUS, &in_value);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

	} while (!in_value);

	return_ACPI_STATUS(AE_OK);
}


acpi_status acpi_hw_legacy_wake_prep(u8 sleep_state, u8 flags)
{
	acpi_status status;
	struct acpi_bit_register_info *sleep_type_reg_info;
	struct acpi_bit_register_info *sleep_enable_reg_info;
	u32 pm1a_control;
	u32 pm1b_control;

	ACPI_FUNCTION_TRACE(hw_legacy_wake_prep);

	status = acpi_get_sleep_type_data(ACPI_STATE_S0,
					  &acpi_gbl_sleep_type_a,
					  &acpi_gbl_sleep_type_b);
	if (ACPI_SUCCESS(status)) {
		sleep_type_reg_info =
		    acpi_hw_get_bit_register_info(ACPI_BITREG_SLEEP_TYPE);
		sleep_enable_reg_info =
		    acpi_hw_get_bit_register_info(ACPI_BITREG_SLEEP_ENABLE);

		

		status = acpi_hw_register_read(ACPI_REGISTER_PM1_CONTROL,
					       &pm1a_control);
		if (ACPI_SUCCESS(status)) {

			

			pm1a_control &= ~(sleep_type_reg_info->access_bit_mask |
					  sleep_enable_reg_info->
					  access_bit_mask);
			pm1b_control = pm1a_control;

			

			pm1a_control |= (acpi_gbl_sleep_type_a <<
					 sleep_type_reg_info->bit_position);
			pm1b_control |= (acpi_gbl_sleep_type_b <<
					 sleep_type_reg_info->bit_position);

			

			(void)acpi_hw_write_pm1_control(pm1a_control,
							pm1b_control);
		}
	}

	

	if (flags & ACPI_EXECUTE_BFS) {
		acpi_hw_execute_sleep_method(METHOD_PATHNAME__BFS, sleep_state);
	}
	return_ACPI_STATUS(status);
}


acpi_status acpi_hw_legacy_wake(u8 sleep_state, u8 flags)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(hw_legacy_wake);

	

	acpi_gbl_sleep_type_a = ACPI_SLEEP_TYPE_INVALID;
	acpi_hw_execute_sleep_method(METHOD_PATHNAME__SST, ACPI_SST_WAKING);

	status = acpi_hw_disable_all_gpes();
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	status = acpi_hw_enable_all_runtime_gpes();
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	acpi_hw_execute_sleep_method(METHOD_PATHNAME__WAK, sleep_state);

	acpi_write_bit_register(ACPI_BITREG_WAKE_STATUS, 1);
	acpi_gbl_system_awake_and_running = TRUE;

	

	(void)
	    acpi_write_bit_register(acpi_gbl_fixed_event_info
				    [ACPI_EVENT_POWER_BUTTON].
				    enable_register_id, ACPI_ENABLE_EVENT);

	(void)
	    acpi_write_bit_register(acpi_gbl_fixed_event_info
				    [ACPI_EVENT_POWER_BUTTON].
				    status_register_id, ACPI_CLEAR_STATUS);

	status = acpi_write_bit_register(ACPI_BITREG_ARB_DISABLE, 0);
	if (ACPI_FAILURE(status) && (status != AE_BAD_ADDRESS)) {
		return_ACPI_STATUS(status);
	}

	acpi_hw_execute_sleep_method(METHOD_PATHNAME__SST, ACPI_SST_WORKING);
	return_ACPI_STATUS(status);
}

#endif				
