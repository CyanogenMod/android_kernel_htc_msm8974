

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
#include "acinterp.h"

#define _COMPONENT          ACPI_EXECUTER
ACPI_MODULE_NAME("exsystem")

acpi_status acpi_ex_system_wait_semaphore(acpi_semaphore semaphore, u16 timeout)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(ex_system_wait_semaphore);

	status = acpi_os_wait_semaphore(semaphore, 1, ACPI_DO_NOT_WAIT);
	if (ACPI_SUCCESS(status)) {
		return_ACPI_STATUS(status);
	}

	if (status == AE_TIME) {

		

		acpi_ex_relinquish_interpreter();

		status = acpi_os_wait_semaphore(semaphore, 1, timeout);

		ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
				  "*** Thread awake after blocking, %s\n",
				  acpi_format_exception(status)));

		

		acpi_ex_reacquire_interpreter();
	}

	return_ACPI_STATUS(status);
}


acpi_status acpi_ex_system_wait_mutex(acpi_mutex mutex, u16 timeout)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(ex_system_wait_mutex);

	status = acpi_os_acquire_mutex(mutex, ACPI_DO_NOT_WAIT);
	if (ACPI_SUCCESS(status)) {
		return_ACPI_STATUS(status);
	}

	if (status == AE_TIME) {

		

		acpi_ex_relinquish_interpreter();

		status = acpi_os_acquire_mutex(mutex, timeout);

		ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
				  "*** Thread awake after blocking, %s\n",
				  acpi_format_exception(status)));

		

		acpi_ex_reacquire_interpreter();
	}

	return_ACPI_STATUS(status);
}


acpi_status acpi_ex_system_do_stall(u32 how_long)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_ENTRY();

	if (how_long > 255) {	
		ACPI_ERROR((AE_INFO, "Time parameter is too large (%u)",
			    how_long));
		status = AE_AML_OPERAND_VALUE;
	} else {
		acpi_os_stall(how_long);
	}

	return (status);
}


acpi_status acpi_ex_system_do_sleep(u64 how_long)
{
	ACPI_FUNCTION_ENTRY();

	

	acpi_ex_relinquish_interpreter();

	if (how_long > ACPI_MAX_SLEEP) {
		how_long = ACPI_MAX_SLEEP;
	}

	acpi_os_sleep(how_long);

	

	acpi_ex_reacquire_interpreter();
	return (AE_OK);
}


acpi_status acpi_ex_system_signal_event(union acpi_operand_object * obj_desc)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ex_system_signal_event);

	if (obj_desc) {
		status =
		    acpi_os_signal_semaphore(obj_desc->event.os_semaphore, 1);
	}

	return_ACPI_STATUS(status);
}


acpi_status
acpi_ex_system_wait_event(union acpi_operand_object *time_desc,
			  union acpi_operand_object *obj_desc)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ex_system_wait_event);

	if (obj_desc) {
		status =
		    acpi_ex_system_wait_semaphore(obj_desc->event.os_semaphore,
						  (u16) time_desc->integer.
						  value);
	}

	return_ACPI_STATUS(status);
}


acpi_status acpi_ex_system_reset_event(union acpi_operand_object *obj_desc)
{
	acpi_status status = AE_OK;
	acpi_semaphore temp_semaphore;

	ACPI_FUNCTION_ENTRY();

	status =
	    acpi_os_create_semaphore(ACPI_NO_UNIT_LIMIT, 0, &temp_semaphore);
	if (ACPI_SUCCESS(status)) {
		(void)acpi_os_delete_semaphore(obj_desc->event.os_semaphore);
		obj_desc->event.os_semaphore = temp_semaphore;
	}

	return (status);
}
