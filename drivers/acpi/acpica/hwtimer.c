

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

#define _COMPONENT          ACPI_HARDWARE
ACPI_MODULE_NAME("hwtimer")

#if (!ACPI_REDUCED_HARDWARE)	
acpi_status acpi_get_timer_resolution(u32 * resolution)
{
	ACPI_FUNCTION_TRACE(acpi_get_timer_resolution);

	if (!resolution) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	if ((acpi_gbl_FADT.flags & ACPI_FADT_32BIT_TIMER) == 0) {
		*resolution = 24;
	} else {
		*resolution = 32;
	}

	return_ACPI_STATUS(AE_OK);
}

ACPI_EXPORT_SYMBOL(acpi_get_timer_resolution)

acpi_status acpi_get_timer(u32 * ticks)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(acpi_get_timer);

	if (!ticks) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	status =
	    acpi_hw_read(ticks, &acpi_gbl_FADT.xpm_timer_block);

	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_get_timer)

acpi_status
acpi_get_timer_duration(u32 start_ticks, u32 end_ticks, u32 * time_elapsed)
{
	acpi_status status;
	u32 delta_ticks;
	u64 quotient;

	ACPI_FUNCTION_TRACE(acpi_get_timer_duration);

	if (!time_elapsed) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	if (start_ticks < end_ticks) {
		delta_ticks = end_ticks - start_ticks;
	} else if (start_ticks > end_ticks) {
		if ((acpi_gbl_FADT.flags & ACPI_FADT_32BIT_TIMER) == 0) {

			

			delta_ticks =
			    (((0x00FFFFFF - start_ticks) +
			      end_ticks) & 0x00FFFFFF);
		} else {
			

			delta_ticks = (0xFFFFFFFF - start_ticks) + end_ticks;
		}
	} else {		

		*time_elapsed = 0;
		return_ACPI_STATUS(AE_OK);
	}

	status = acpi_ut_short_divide(((u64) delta_ticks) * 1000000,
				      PM_TIMER_FREQUENCY, &quotient, NULL);

	*time_elapsed = (u32) quotient;
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_get_timer_duration)
#endif				
