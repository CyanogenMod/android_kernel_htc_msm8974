
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

#define _COMPONENT          ACPI_UTILITIES
ACPI_MODULE_NAME("utxferror")

#ifdef ACPI_ASL_COMPILER
#include <stdio.h>
extern FILE *acpi_gbl_output_file;

#define ACPI_MSG_REDIRECT_BEGIN \
	FILE                            *output_file = acpi_gbl_output_file; \
	acpi_os_redirect_output (stderr);

#define ACPI_MSG_REDIRECT_END \
	acpi_os_redirect_output (output_file);

#else
#define ACPI_MSG_REDIRECT_BEGIN
#define ACPI_MSG_REDIRECT_END
#endif
#define ACPI_MSG_ERROR          "ACPI Error: "
#define ACPI_MSG_EXCEPTION      "ACPI Exception: "
#define ACPI_MSG_WARNING        "ACPI Warning: "
#define ACPI_MSG_INFO           "ACPI: "
#define ACPI_MSG_SUFFIX \
	acpi_os_printf (" (%8.8X/%s-%u)\n", ACPI_CA_VERSION, module_name, line_number)
void ACPI_INTERNAL_VAR_XFACE
acpi_error(const char *module_name, u32 line_number, const char *format, ...)
{
	va_list arg_list;

	ACPI_MSG_REDIRECT_BEGIN;
	acpi_os_printf(ACPI_MSG_ERROR);

	va_start(arg_list, format);
	acpi_os_vprintf(format, arg_list);
	ACPI_MSG_SUFFIX;
	va_end(arg_list);

	ACPI_MSG_REDIRECT_END;
}

ACPI_EXPORT_SYMBOL(acpi_error)

void ACPI_INTERNAL_VAR_XFACE
acpi_exception(const char *module_name,
	       u32 line_number, acpi_status status, const char *format, ...)
{
	va_list arg_list;

	ACPI_MSG_REDIRECT_BEGIN;
	acpi_os_printf(ACPI_MSG_EXCEPTION "%s, ",
		       acpi_format_exception(status));

	va_start(arg_list, format);
	acpi_os_vprintf(format, arg_list);
	ACPI_MSG_SUFFIX;
	va_end(arg_list);

	ACPI_MSG_REDIRECT_END;
}

ACPI_EXPORT_SYMBOL(acpi_exception)

void ACPI_INTERNAL_VAR_XFACE
acpi_warning(const char *module_name, u32 line_number, const char *format, ...)
{
	va_list arg_list;

	ACPI_MSG_REDIRECT_BEGIN;
	acpi_os_printf(ACPI_MSG_WARNING);

	va_start(arg_list, format);
	acpi_os_vprintf(format, arg_list);
	ACPI_MSG_SUFFIX;
	va_end(arg_list);

	ACPI_MSG_REDIRECT_END;
}

ACPI_EXPORT_SYMBOL(acpi_warning)

void ACPI_INTERNAL_VAR_XFACE
acpi_info(const char *module_name, u32 line_number, const char *format, ...)
{
	va_list arg_list;

	ACPI_MSG_REDIRECT_BEGIN;
	acpi_os_printf(ACPI_MSG_INFO);

	va_start(arg_list, format);
	acpi_os_vprintf(format, arg_list);
	acpi_os_printf("\n");
	va_end(arg_list);

	ACPI_MSG_REDIRECT_END;
}

ACPI_EXPORT_SYMBOL(acpi_info)

#if !defined (ACPI_NO_ERROR_MESSAGES) && !defined (ACPI_BIN_APP)
void ACPI_INTERNAL_VAR_XFACE
acpi_ut_predefined_warning(const char *module_name,
			   u32 line_number,
			   char *pathname,
			   u8 node_flags, const char *format, ...)
{
	va_list arg_list;

	if (node_flags & ANOBJ_EVALUATED) {
		return;
	}

	acpi_os_printf(ACPI_MSG_WARNING "For %s: ", pathname);

	va_start(arg_list, format);
	acpi_os_vprintf(format, arg_list);
	ACPI_MSG_SUFFIX;
	va_end(arg_list);
}


void ACPI_INTERNAL_VAR_XFACE
acpi_ut_predefined_info(const char *module_name,
			u32 line_number,
			char *pathname, u8 node_flags, const char *format, ...)
{
	va_list arg_list;

	if (node_flags & ANOBJ_EVALUATED) {
		return;
	}

	acpi_os_printf(ACPI_MSG_INFO "For %s: ", pathname);

	va_start(arg_list, format);
	acpi_os_vprintf(format, arg_list);
	ACPI_MSG_SUFFIX;
	va_end(arg_list);
}


void
acpi_ut_namespace_error(const char *module_name,
			u32 line_number,
			const char *internal_name, acpi_status lookup_status)
{
	acpi_status status;
	u32 bad_name;
	char *name = NULL;

	ACPI_MSG_REDIRECT_BEGIN;
	acpi_os_printf(ACPI_MSG_ERROR);

	if (lookup_status == AE_BAD_CHARACTER) {

		

		ACPI_MOVE_32_TO_32(&bad_name,
				   ACPI_CAST_PTR(u32, internal_name));
		acpi_os_printf("[0x%4.4X] (NON-ASCII)", bad_name);
	} else {
		

		status = acpi_ns_externalize_name(ACPI_UINT32_MAX,
						  internal_name, NULL, &name);

		

		if (ACPI_SUCCESS(status)) {
			acpi_os_printf("[%s]", name);
		} else {
			acpi_os_printf("[COULD NOT EXTERNALIZE NAME]");
		}

		if (name) {
			ACPI_FREE(name);
		}
	}

	acpi_os_printf(" Namespace lookup failure, %s",
		       acpi_format_exception(lookup_status));

	ACPI_MSG_SUFFIX;
	ACPI_MSG_REDIRECT_END;
}


void
acpi_ut_method_error(const char *module_name,
		     u32 line_number,
		     const char *message,
		     struct acpi_namespace_node *prefix_node,
		     const char *path, acpi_status method_status)
{
	acpi_status status;
	struct acpi_namespace_node *node = prefix_node;

	ACPI_MSG_REDIRECT_BEGIN;
	acpi_os_printf(ACPI_MSG_ERROR);

	if (path) {
		status =
		    acpi_ns_get_node(prefix_node, path, ACPI_NS_NO_UPSEARCH,
				     &node);
		if (ACPI_FAILURE(status)) {
			acpi_os_printf("[Could not get node by pathname]");
		}
	}

	acpi_ns_print_node_pathname(node, message);
	acpi_os_printf(", %s", acpi_format_exception(method_status));

	ACPI_MSG_SUFFIX;
	ACPI_MSG_REDIRECT_END;
}

#endif				
