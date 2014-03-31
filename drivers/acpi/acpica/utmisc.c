
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

#include <linux/module.h>

#include <acpi/acpi.h>
#include "accommon.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_UTILITIES
ACPI_MODULE_NAME("utmisc")

const char *acpi_ut_validate_exception(acpi_status status)
{
	u32 sub_status;
	const char *exception = NULL;

	ACPI_FUNCTION_ENTRY();

	sub_status = (status & ~AE_CODE_MASK);

	switch (status & AE_CODE_MASK) {
	case AE_CODE_ENVIRONMENTAL:

		if (sub_status <= AE_CODE_ENV_MAX) {
			exception = acpi_gbl_exception_names_env[sub_status];
		}
		break;

	case AE_CODE_PROGRAMMER:

		if (sub_status <= AE_CODE_PGM_MAX) {
			exception = acpi_gbl_exception_names_pgm[sub_status];
		}
		break;

	case AE_CODE_ACPI_TABLES:

		if (sub_status <= AE_CODE_TBL_MAX) {
			exception = acpi_gbl_exception_names_tbl[sub_status];
		}
		break;

	case AE_CODE_AML:

		if (sub_status <= AE_CODE_AML_MAX) {
			exception = acpi_gbl_exception_names_aml[sub_status];
		}
		break;

	case AE_CODE_CONTROL:

		if (sub_status <= AE_CODE_CTRL_MAX) {
			exception = acpi_gbl_exception_names_ctrl[sub_status];
		}
		break;

	default:
		break;
	}

	return (ACPI_CAST_PTR(const char, exception));
}


u8 acpi_ut_is_pci_root_bridge(char *id)
{

	if (!(ACPI_STRCMP(id,
			  PCI_ROOT_HID_STRING)) ||
	    !(ACPI_STRCMP(id, PCI_EXPRESS_ROOT_HID_STRING))) {
		return (TRUE);
	}

	return (FALSE);
}


u8 acpi_ut_is_aml_table(struct acpi_table_header *table)
{

	

	if (ACPI_COMPARE_NAME(table->signature, ACPI_SIG_DSDT) ||
	    ACPI_COMPARE_NAME(table->signature, ACPI_SIG_PSDT) ||
	    ACPI_COMPARE_NAME(table->signature, ACPI_SIG_SSDT)) {
		return (TRUE);
	}

	return (FALSE);
}


acpi_status acpi_ut_allocate_owner_id(acpi_owner_id * owner_id)
{
	u32 i;
	u32 j;
	u32 k;
	acpi_status status;

	ACPI_FUNCTION_TRACE(ut_allocate_owner_id);

	

	if (*owner_id) {
		ACPI_ERROR((AE_INFO, "Owner ID [0x%2.2X] already exists",
			    *owner_id));
		return_ACPI_STATUS(AE_ALREADY_EXISTS);
	}

	

	status = acpi_ut_acquire_mutex(ACPI_MTX_CACHES);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	for (i = 0, j = acpi_gbl_last_owner_id_index;
	     i < (ACPI_NUM_OWNERID_MASKS + 1); i++, j++) {
		if (j >= ACPI_NUM_OWNERID_MASKS) {
			j = 0;	
		}

		for (k = acpi_gbl_next_owner_id_offset; k < 32; k++) {
			if (acpi_gbl_owner_id_mask[j] == ACPI_UINT32_MAX) {

				

				break;
			}

			if (!(acpi_gbl_owner_id_mask[j] & (1 << k))) {
				acpi_gbl_owner_id_mask[j] |= (1 << k);

				acpi_gbl_last_owner_id_index = (u8) j;
				acpi_gbl_next_owner_id_offset = (u8) (k + 1);

				*owner_id =
				    (acpi_owner_id) ((k + 1) + ACPI_MUL_32(j));

				ACPI_DEBUG_PRINT((ACPI_DB_VALUES,
						  "Allocated OwnerId: %2.2X\n",
						  (unsigned int)*owner_id));
				goto exit;
			}
		}

		acpi_gbl_next_owner_id_offset = 0;
	}

	status = AE_OWNER_ID_LIMIT;
	ACPI_ERROR((AE_INFO,
		    "Could not allocate new OwnerId (255 max), AE_OWNER_ID_LIMIT"));

      exit:
	(void)acpi_ut_release_mutex(ACPI_MTX_CACHES);
	return_ACPI_STATUS(status);
}


void acpi_ut_release_owner_id(acpi_owner_id * owner_id_ptr)
{
	acpi_owner_id owner_id = *owner_id_ptr;
	acpi_status status;
	u32 index;
	u32 bit;

	ACPI_FUNCTION_TRACE_U32(ut_release_owner_id, owner_id);

	

	*owner_id_ptr = 0;

	

	if (owner_id == 0) {
		ACPI_ERROR((AE_INFO, "Invalid OwnerId: 0x%2.2X", owner_id));
		return_VOID;
	}

	

	status = acpi_ut_acquire_mutex(ACPI_MTX_CACHES);
	if (ACPI_FAILURE(status)) {
		return_VOID;
	}

	

	owner_id--;

	

	index = ACPI_DIV_32(owner_id);
	bit = 1 << ACPI_MOD_32(owner_id);

	

	if (acpi_gbl_owner_id_mask[index] & bit) {
		acpi_gbl_owner_id_mask[index] ^= bit;
	} else {
		ACPI_ERROR((AE_INFO,
			    "Release of non-allocated OwnerId: 0x%2.2X",
			    owner_id + 1));
	}

	(void)acpi_ut_release_mutex(ACPI_MTX_CACHES);
	return_VOID;
}


void acpi_ut_strupr(char *src_string)
{
	char *string;

	ACPI_FUNCTION_ENTRY();

	if (!src_string) {
		return;
	}

	

	for (string = src_string; *string; string++) {
		*string = (char)ACPI_TOUPPER(*string);
	}

	return;
}


void acpi_ut_print_string(char *string, u8 max_length)
{
	u32 i;

	if (!string) {
		acpi_os_printf("<\"NULL STRING PTR\">");
		return;
	}

	acpi_os_printf("\"");
	for (i = 0; string[i] && (i < max_length); i++) {

		

		switch (string[i]) {
		case 0x07:
			acpi_os_printf("\\a");	
			break;

		case 0x08:
			acpi_os_printf("\\b");	
			break;

		case 0x0C:
			acpi_os_printf("\\f");	
			break;

		case 0x0A:
			acpi_os_printf("\\n");	
			break;

		case 0x0D:
			acpi_os_printf("\\r");	
			break;

		case 0x09:
			acpi_os_printf("\\t");	
			break;

		case 0x0B:
			acpi_os_printf("\\v");	
			break;

		case '\'':	
		case '\"':	
		case '\\':	
			acpi_os_printf("\\%c", (int)string[i]);
			break;

		default:

			

			if (ACPI_IS_PRINT(string[i])) {
				

				acpi_os_printf("%c", (int)string[i]);
			} else {
				

				acpi_os_printf("\\x%2.2X", (s32) string[i]);
			}
			break;
		}
	}
	acpi_os_printf("\"");

	if (i == max_length && string[i]) {
		acpi_os_printf("...");
	}
}


u32 acpi_ut_dword_byte_swap(u32 value)
{
	union {
		u32 value;
		u8 bytes[4];
	} out;
	union {
		u32 value;
		u8 bytes[4];
	} in;

	ACPI_FUNCTION_ENTRY();

	in.value = value;

	out.bytes[0] = in.bytes[3];
	out.bytes[1] = in.bytes[2];
	out.bytes[2] = in.bytes[1];
	out.bytes[3] = in.bytes[0];

	return (out.value);
}


void acpi_ut_set_integer_width(u8 revision)
{

	if (revision < 2) {

		

		acpi_gbl_integer_bit_width = 32;
		acpi_gbl_integer_nybble_width = 8;
		acpi_gbl_integer_byte_width = 4;
	} else {
		

		acpi_gbl_integer_bit_width = 64;
		acpi_gbl_integer_nybble_width = 16;
		acpi_gbl_integer_byte_width = 8;
	}
}

#ifdef ACPI_DEBUG_OUTPUT

void
acpi_ut_display_init_pathname(u8 type,
			      struct acpi_namespace_node *obj_handle,
			      char *path)
{
	acpi_status status;
	struct acpi_buffer buffer;

	ACPI_FUNCTION_ENTRY();

	

	if (!(acpi_dbg_level & ACPI_LV_INIT_NAMES)) {
		return;
	}

	

	buffer.length = ACPI_ALLOCATE_LOCAL_BUFFER;
	status = acpi_ns_handle_to_pathname(obj_handle, &buffer);
	if (ACPI_FAILURE(status)) {
		return;
	}

	

	switch (type) {
	case ACPI_TYPE_METHOD:
		acpi_os_printf("Executing  ");
		break;

	default:
		acpi_os_printf("Initializing ");
		break;
	}

	

	acpi_os_printf("%-12s %s",
		       acpi_ut_get_type_name(type), (char *)buffer.pointer);

	

	if (path) {
		acpi_os_printf(".%s", path);
	}
	acpi_os_printf("\n");

	ACPI_FREE(buffer.pointer);
}
#endif


u8 acpi_ut_valid_acpi_char(char character, u32 position)
{

	if (!((character >= 'A' && character <= 'Z') ||
	      (character >= '0' && character <= '9') || (character == '_'))) {

		

		if (character == '!' && position == 3) {
			return (TRUE);
		}

		return (FALSE);
	}

	return (TRUE);
}


u8 acpi_ut_valid_acpi_name(u32 name)
{
	u32 i;

	ACPI_FUNCTION_ENTRY();

	for (i = 0; i < ACPI_NAME_SIZE; i++) {
		if (!acpi_ut_valid_acpi_char
		    ((ACPI_CAST_PTR(char, &name))[i], i)) {
			return (FALSE);
		}
	}

	return (TRUE);
}


acpi_name acpi_ut_repair_name(char *name)
{
       u32 i;
	char new_name[ACPI_NAME_SIZE];

	for (i = 0; i < ACPI_NAME_SIZE; i++) {
		new_name[i] = name[i];

		if (!acpi_ut_valid_acpi_char(name[i], i)) {
			new_name[i] = '*';
		}
	}

	return (*(u32 *) new_name);
}


acpi_status acpi_ut_strtoul64(char *string, u32 base, u64 * ret_integer)
{
	u32 this_digit = 0;
	u64 return_value = 0;
	u64 quotient;
	u64 dividend;
	u32 to_integer_op = (base == ACPI_ANY_BASE);
	u32 mode32 = (acpi_gbl_integer_byte_width == 4);
	u8 valid_digits = 0;
	u8 sign_of0x = 0;
	u8 term = 0;

	ACPI_FUNCTION_TRACE_STR(ut_stroul64, string);

	switch (base) {
	case ACPI_ANY_BASE:
	case 16:
		break;

	default:
		
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	if (!string) {
		goto error_exit;
	}

	

	while ((*string) && (ACPI_IS_SPACE(*string) || *string == '\t')) {
		string++;
	}

	if (to_integer_op) {
		if ((*string == '0') && (ACPI_TOLOWER(*(string + 1)) == 'x')) {
			sign_of0x = 1;
			base = 16;

			
			string += 2;
		} else {
			base = 10;
		}
	}

	

	if (!(*string) || ACPI_IS_SPACE(*string) || *string == '\t') {
		if (to_integer_op) {
			goto error_exit;
		} else {
			goto all_done;
		}
	}

	dividend = (mode32) ? ACPI_UINT32_MAX : ACPI_UINT64_MAX;

	

	while (*string) {
		if (ACPI_IS_DIGIT(*string)) {

			

			this_digit = ((u8) * string) - '0';
		} else if (base == 10) {

			

			term = 1;
		} else {
			this_digit = (u8) ACPI_TOUPPER(*string);
			if (ACPI_IS_XDIGIT((char)this_digit)) {

				

				this_digit = this_digit - 'A' + 10;
			} else {
				term = 1;
			}
		}

		if (term) {
			if (to_integer_op) {
				goto error_exit;
			} else {
				break;
			}
		} else if ((valid_digits == 0) && (this_digit == 0)
			   && !sign_of0x) {

			
			string++;
			continue;
		}

		valid_digits++;

		if (sign_of0x && ((valid_digits > 16)
				  || ((valid_digits > 8) && mode32))) {
			goto error_exit;
		}

		

		(void)acpi_ut_short_divide((dividend - (u64) this_digit),
					   base, &quotient, NULL);

		if (return_value > quotient) {
			if (to_integer_op) {
				goto error_exit;
			} else {
				break;
			}
		}

		return_value *= base;
		return_value += this_digit;
		string++;
	}

	

      all_done:

	ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "Converted value: %8.8X%8.8X\n",
			  ACPI_FORMAT_UINT64(return_value)));

	*ret_integer = return_value;
	return_ACPI_STATUS(AE_OK);

      error_exit:
	

	if (base == 10) {
		return_ACPI_STATUS(AE_BAD_DECIMAL_CONSTANT);
	} else {
		return_ACPI_STATUS(AE_BAD_HEX_CONSTANT);
	}
}


acpi_status
acpi_ut_create_update_state_and_push(union acpi_operand_object *object,
				     u16 action,
				     union acpi_generic_state **state_list)
{
	union acpi_generic_state *state;

	ACPI_FUNCTION_ENTRY();

	

	if (!object) {
		return (AE_OK);
	}

	state = acpi_ut_create_update_state(object, action);
	if (!state) {
		return (AE_NO_MEMORY);
	}

	acpi_ut_push_generic_state(state_list, state);
	return (AE_OK);
}


acpi_status
acpi_ut_walk_package_tree(union acpi_operand_object * source_object,
			  void *target_object,
			  acpi_pkg_callback walk_callback, void *context)
{
	acpi_status status = AE_OK;
	union acpi_generic_state *state_list = NULL;
	union acpi_generic_state *state;
	u32 this_index;
	union acpi_operand_object *this_source_obj;

	ACPI_FUNCTION_TRACE(ut_walk_package_tree);

	state = acpi_ut_create_pkg_state(source_object, target_object, 0);
	if (!state) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	while (state) {

		

		this_index = state->pkg.index;
		this_source_obj = (union acpi_operand_object *)
		    state->pkg.source_object->package.elements[this_index];

		if ((!this_source_obj) ||
		    (ACPI_GET_DESCRIPTOR_TYPE(this_source_obj) !=
		     ACPI_DESC_TYPE_OPERAND)
		    || (this_source_obj->common.type != ACPI_TYPE_PACKAGE)) {
			status =
			    walk_callback(ACPI_COPY_TYPE_SIMPLE,
					  this_source_obj, state, context);
			if (ACPI_FAILURE(status)) {
				return_ACPI_STATUS(status);
			}

			state->pkg.index++;
			while (state->pkg.index >=
			       state->pkg.source_object->package.count) {
				acpi_ut_delete_generic_state(state);
				state = acpi_ut_pop_generic_state(&state_list);

				

				if (!state) {
					return_ACPI_STATUS(AE_OK);
				}

				state->pkg.index++;
			}
		} else {
			

			status =
			    walk_callback(ACPI_COPY_TYPE_PACKAGE,
					  this_source_obj, state, context);
			if (ACPI_FAILURE(status)) {
				return_ACPI_STATUS(status);
			}

			acpi_ut_push_generic_state(&state_list, state);
			state = acpi_ut_create_pkg_state(this_source_obj,
							 state->pkg.
							 this_target_obj, 0);
			if (!state) {

				

				while (state_list) {
					state =
					    acpi_ut_pop_generic_state
					    (&state_list);
					acpi_ut_delete_generic_state(state);
				}
				return_ACPI_STATUS(AE_NO_MEMORY);
			}
		}
	}

	

	return_ACPI_STATUS(AE_AML_INTERNAL);
}
