
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
#include "actables.h"

#define _COMPONENT          ACPI_EVENTS
ACPI_MODULE_NAME("evxfevnt")

#if (!ACPI_REDUCED_HARDWARE)	

acpi_status acpi_enable(void)
{
	acpi_status status;
	int retry;

	ACPI_FUNCTION_TRACE(acpi_enable);

	

	if (!acpi_tb_tables_loaded()) {
		return_ACPI_STATUS(AE_NO_ACPI_TABLES);
	}

	

	if (acpi_hw_get_mode() == ACPI_SYS_MODE_ACPI) {
		ACPI_DEBUG_PRINT((ACPI_DB_INIT,
				  "System is already in ACPI mode\n"));
		return_ACPI_STATUS(AE_OK);
	}

	

	status = acpi_hw_set_mode(ACPI_SYS_MODE_ACPI);
	if (ACPI_FAILURE(status)) {
		ACPI_ERROR((AE_INFO,
			    "Could not transition to ACPI mode"));
		return_ACPI_STATUS(status);
	}

	

	for (retry = 0; retry < 30000; ++retry) {
		if (acpi_hw_get_mode() == ACPI_SYS_MODE_ACPI) {
			if (retry != 0)
				ACPI_WARNING((AE_INFO,
				"Platform took > %d00 usec to enter ACPI mode", retry));
			return_ACPI_STATUS(AE_OK);
		}
		acpi_os_stall(100);	
	}

	ACPI_ERROR((AE_INFO, "Hardware did not enter ACPI mode"));
	return_ACPI_STATUS(AE_NO_HARDWARE_RESPONSE);
}

ACPI_EXPORT_SYMBOL(acpi_enable)

acpi_status acpi_disable(void)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(acpi_disable);

	if (acpi_hw_get_mode() == ACPI_SYS_MODE_LEGACY) {
		ACPI_DEBUG_PRINT((ACPI_DB_INIT,
				  "System is already in legacy (non-ACPI) mode\n"));
	} else {
		

		status = acpi_hw_set_mode(ACPI_SYS_MODE_LEGACY);

		if (ACPI_FAILURE(status)) {
			ACPI_ERROR((AE_INFO,
				    "Could not exit ACPI mode to legacy mode"));
			return_ACPI_STATUS(status);
		}

		ACPI_DEBUG_PRINT((ACPI_DB_INIT, "ACPI mode disabled\n"));
	}

	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_disable)

acpi_status acpi_enable_event(u32 event, u32 flags)
{
	acpi_status status = AE_OK;
	u32 value;

	ACPI_FUNCTION_TRACE(acpi_enable_event);

	

	if (event > ACPI_EVENT_MAX) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	status =
	    acpi_write_bit_register(acpi_gbl_fixed_event_info[event].
				    enable_register_id, ACPI_ENABLE_EVENT);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	

	status =
	    acpi_read_bit_register(acpi_gbl_fixed_event_info[event].
				   enable_register_id, &value);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	if (value != 1) {
		ACPI_ERROR((AE_INFO,
			    "Could not enable %s event",
			    acpi_ut_get_event_name(event)));
		return_ACPI_STATUS(AE_NO_HARDWARE_RESPONSE);
	}

	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_enable_event)

acpi_status acpi_disable_event(u32 event, u32 flags)
{
	acpi_status status = AE_OK;
	u32 value;

	ACPI_FUNCTION_TRACE(acpi_disable_event);

	

	if (event > ACPI_EVENT_MAX) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	status =
	    acpi_write_bit_register(acpi_gbl_fixed_event_info[event].
				    enable_register_id, ACPI_DISABLE_EVENT);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	status =
	    acpi_read_bit_register(acpi_gbl_fixed_event_info[event].
				   enable_register_id, &value);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	if (value != 0) {
		ACPI_ERROR((AE_INFO,
			    "Could not disable %s events",
			    acpi_ut_get_event_name(event)));
		return_ACPI_STATUS(AE_NO_HARDWARE_RESPONSE);
	}

	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_disable_event)

acpi_status acpi_clear_event(u32 event)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(acpi_clear_event);

	

	if (event > ACPI_EVENT_MAX) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	status =
	    acpi_write_bit_register(acpi_gbl_fixed_event_info[event].
				    status_register_id, ACPI_CLEAR_STATUS);

	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_clear_event)

acpi_status acpi_get_event_status(u32 event, acpi_event_status * event_status)
{
	acpi_status status = AE_OK;
	u32 value;

	ACPI_FUNCTION_TRACE(acpi_get_event_status);

	if (!event_status) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	

	if (event > ACPI_EVENT_MAX) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	

	status =
	    acpi_read_bit_register(acpi_gbl_fixed_event_info[event].
			      enable_register_id, &value);
	if (ACPI_FAILURE(status))
		return_ACPI_STATUS(status);

	*event_status = value;

	status =
	    acpi_read_bit_register(acpi_gbl_fixed_event_info[event].
			      status_register_id, &value);
	if (ACPI_FAILURE(status))
		return_ACPI_STATUS(status);

	if (value)
		*event_status |= ACPI_EVENT_FLAG_SET;

	if (acpi_gbl_fixed_event_handlers[event].handler)
		*event_status |= ACPI_EVENT_FLAG_HANDLE;

	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_get_event_status)
#endif				
