
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
#include "acevents.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_EVENTS
ACPI_MODULE_NAME("evglock")
#if (!ACPI_REDUCED_HARDWARE)	
static u32 acpi_ev_global_lock_handler(void *context);


acpi_status acpi_ev_init_global_lock_handler(void)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(ev_init_global_lock_handler);

	

	if (acpi_gbl_reduced_hardware) {
		return_ACPI_STATUS(AE_OK);
	}

	

	status = acpi_install_fixed_event_handler(ACPI_EVENT_GLOBAL,
						  acpi_ev_global_lock_handler,
						  NULL);

	acpi_gbl_global_lock_present = FALSE;
	if (status == AE_NO_HARDWARE_RESPONSE) {
		ACPI_ERROR((AE_INFO,
			    "No response from Global Lock hardware, disabling lock"));

		return_ACPI_STATUS(AE_OK);
	}

	status = acpi_os_create_lock(&acpi_gbl_global_lock_pending_lock);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	acpi_gbl_global_lock_pending = FALSE;
	acpi_gbl_global_lock_present = TRUE;
	return_ACPI_STATUS(status);
}


acpi_status acpi_ev_remove_global_lock_handler(void)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(ev_remove_global_lock_handler);

	acpi_gbl_global_lock_present = FALSE;
	status = acpi_remove_fixed_event_handler(ACPI_EVENT_GLOBAL,
						 acpi_ev_global_lock_handler);

	return_ACPI_STATUS(status);
}


static u32 acpi_ev_global_lock_handler(void *context)
{
	acpi_status status;
	acpi_cpu_flags flags;

	flags = acpi_os_acquire_lock(acpi_gbl_global_lock_pending_lock);

	if (!acpi_gbl_global_lock_pending) {
		goto cleanup_and_exit;
	}

	status = acpi_os_signal_semaphore(acpi_gbl_global_lock_semaphore, 1);
	if (ACPI_FAILURE(status)) {
		ACPI_ERROR((AE_INFO, "Could not signal Global Lock semaphore"));
	}

	acpi_gbl_global_lock_pending = FALSE;

      cleanup_and_exit:

	acpi_os_release_lock(acpi_gbl_global_lock_pending_lock, flags);
	return (ACPI_INTERRUPT_HANDLED);
}


acpi_status acpi_ev_acquire_global_lock(u16 timeout)
{
	acpi_cpu_flags flags;
	acpi_status status;
	u8 acquired = FALSE;

	ACPI_FUNCTION_TRACE(ev_acquire_global_lock);

	status =
	    acpi_ex_system_wait_mutex(acpi_gbl_global_lock_mutex->mutex.
				      os_mutex, timeout);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	acpi_gbl_global_lock_handle++;
	if (acpi_gbl_global_lock_handle == 0) {
		acpi_gbl_global_lock_handle = 1;
	}

	if (!acpi_gbl_global_lock_present) {
		acpi_gbl_global_lock_acquired = TRUE;
		return_ACPI_STATUS(AE_OK);
	}

	flags = acpi_os_acquire_lock(acpi_gbl_global_lock_pending_lock);

	do {

		

		ACPI_ACQUIRE_GLOBAL_LOCK(acpi_gbl_FACS, acquired);
		if (acquired) {
			acpi_gbl_global_lock_acquired = TRUE;
			ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
					  "Acquired hardware Global Lock\n"));
			break;
		}

		acpi_gbl_global_lock_pending = TRUE;
		acpi_os_release_lock(acpi_gbl_global_lock_pending_lock, flags);

		ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
				  "Waiting for hardware Global Lock\n"));

		status =
		    acpi_ex_system_wait_semaphore
		    (acpi_gbl_global_lock_semaphore, ACPI_WAIT_FOREVER);

		flags = acpi_os_acquire_lock(acpi_gbl_global_lock_pending_lock);

	} while (ACPI_SUCCESS(status));

	acpi_gbl_global_lock_pending = FALSE;
	acpi_os_release_lock(acpi_gbl_global_lock_pending_lock, flags);

	return_ACPI_STATUS(status);
}


acpi_status acpi_ev_release_global_lock(void)
{
	u8 pending = FALSE;
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ev_release_global_lock);

	

	if (!acpi_gbl_global_lock_acquired) {
		ACPI_WARNING((AE_INFO,
			      "Cannot release the ACPI Global Lock, it has not been acquired"));
		return_ACPI_STATUS(AE_NOT_ACQUIRED);
	}

	if (acpi_gbl_global_lock_present) {

		

		ACPI_RELEASE_GLOBAL_LOCK(acpi_gbl_FACS, pending);

		if (pending) {
			status =
			    acpi_write_bit_register
			    (ACPI_BITREG_GLOBAL_LOCK_RELEASE,
			     ACPI_ENABLE_EVENT);
		}

		ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
				  "Released hardware Global Lock\n"));
	}

	acpi_gbl_global_lock_acquired = FALSE;

	

	acpi_os_release_mutex(acpi_gbl_global_lock_mutex->mutex.os_mutex);
	return_ACPI_STATUS(status);
}

#endif				
