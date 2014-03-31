
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
#include "amlcode.h"

#define _COMPONENT          ACPI_EXECUTER
ACPI_MODULE_NAME("exconvrt")

static u32
acpi_ex_convert_to_ascii(u64 integer, u16 base, u8 *string, u8 max_length);


acpi_status
acpi_ex_convert_to_integer(union acpi_operand_object *obj_desc,
			   union acpi_operand_object **result_desc, u32 flags)
{
	union acpi_operand_object *return_desc;
	u8 *pointer;
	u64 result;
	u32 i;
	u32 count;
	acpi_status status;

	ACPI_FUNCTION_TRACE_PTR(ex_convert_to_integer, obj_desc);

	switch (obj_desc->common.type) {
	case ACPI_TYPE_INTEGER:

		

		*result_desc = obj_desc;
		return_ACPI_STATUS(AE_OK);

	case ACPI_TYPE_BUFFER:
	case ACPI_TYPE_STRING:

		

		pointer = obj_desc->buffer.pointer;
		count = obj_desc->buffer.length;
		break;

	default:
		return_ACPI_STATUS(AE_TYPE);
	}

	result = 0;

	

	switch (obj_desc->common.type) {
	case ACPI_TYPE_STRING:

		status = acpi_ut_strtoul64((char *)pointer, flags, &result);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}
		break;

	case ACPI_TYPE_BUFFER:

		

		if (!count) {
			return_ACPI_STATUS(AE_AML_BUFFER_LIMIT);
		}

		

		if (count > acpi_gbl_integer_byte_width) {
			count = acpi_gbl_integer_byte_width;
		}

		for (i = 0; i < count; i++) {
			result |= (((u64) pointer[i]) << (i * 8));
		}
		break;

	default:

		
		break;
	}

	

	return_desc = acpi_ut_create_integer_object(result);
	if (!return_desc) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "Converted value: %8.8X%8.8X\n",
			  ACPI_FORMAT_UINT64(result)));

	

	acpi_ex_truncate_for32bit_table(return_desc);
	*result_desc = return_desc;
	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_ex_convert_to_buffer(union acpi_operand_object *obj_desc,
			  union acpi_operand_object **result_desc)
{
	union acpi_operand_object *return_desc;
	u8 *new_buf;

	ACPI_FUNCTION_TRACE_PTR(ex_convert_to_buffer, obj_desc);

	switch (obj_desc->common.type) {
	case ACPI_TYPE_BUFFER:

		

		*result_desc = obj_desc;
		return_ACPI_STATUS(AE_OK);

	case ACPI_TYPE_INTEGER:

		return_desc =
		    acpi_ut_create_buffer_object(acpi_gbl_integer_byte_width);
		if (!return_desc) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		

		new_buf = return_desc->buffer.pointer;
		ACPI_MEMCPY(new_buf,
			    &obj_desc->integer.value,
			    acpi_gbl_integer_byte_width);
		break;

	case ACPI_TYPE_STRING:

		return_desc = acpi_ut_create_buffer_object((acpi_size)
							   obj_desc->string.
							   length + 1);
		if (!return_desc) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		

		new_buf = return_desc->buffer.pointer;
		ACPI_STRNCPY((char *)new_buf, (char *)obj_desc->string.pointer,
			     obj_desc->string.length);
		break;

	default:
		return_ACPI_STATUS(AE_TYPE);
	}

	

	return_desc->common.flags |= AOPOBJ_DATA_VALID;
	*result_desc = return_desc;
	return_ACPI_STATUS(AE_OK);
}


static u32
acpi_ex_convert_to_ascii(u64 integer, u16 base, u8 *string, u8 data_width)
{
	u64 digit;
	u32 i;
	u32 j;
	u32 k = 0;
	u32 hex_length;
	u32 decimal_length;
	u32 remainder;
	u8 supress_zeros;

	ACPI_FUNCTION_ENTRY();

	switch (base) {
	case 10:

		

		switch (data_width) {
		case 1:
			decimal_length = ACPI_MAX8_DECIMAL_DIGITS;
			break;

		case 4:
			decimal_length = ACPI_MAX32_DECIMAL_DIGITS;
			break;

		case 8:
		default:
			decimal_length = ACPI_MAX64_DECIMAL_DIGITS;
			break;
		}

		supress_zeros = TRUE;	
		remainder = 0;

		for (i = decimal_length; i > 0; i--) {

			

			digit = integer;
			for (j = 0; j < i; j++) {
				(void)acpi_ut_short_divide(digit, 10, &digit,
							   &remainder);
			}

			

			if (remainder != 0) {
				supress_zeros = FALSE;
			}

			if (!supress_zeros) {
				string[k] = (u8) (ACPI_ASCII_ZERO + remainder);
				k++;
			}
		}
		break;

	case 16:

		

		hex_length = ACPI_MUL_2(data_width);
		for (i = 0, j = (hex_length - 1); i < hex_length; i++, j--) {

			

			string[k] =
			    (u8) acpi_ut_hex_to_ascii_char(integer,
							   ACPI_MUL_4(j));
			k++;
		}
		break;

	default:
		return (0);
	}

	if (!k) {
		string[0] = ACPI_ASCII_ZERO;
		k = 1;
	}

	string[k] = 0;
	return ((u32) k);
}


acpi_status
acpi_ex_convert_to_string(union acpi_operand_object * obj_desc,
			  union acpi_operand_object ** result_desc, u32 type)
{
	union acpi_operand_object *return_desc;
	u8 *new_buf;
	u32 i;
	u32 string_length = 0;
	u16 base = 16;
	u8 separator = ',';

	ACPI_FUNCTION_TRACE_PTR(ex_convert_to_string, obj_desc);

	switch (obj_desc->common.type) {
	case ACPI_TYPE_STRING:

		

		*result_desc = obj_desc;
		return_ACPI_STATUS(AE_OK);

	case ACPI_TYPE_INTEGER:

		switch (type) {
		case ACPI_EXPLICIT_CONVERT_DECIMAL:

			

			string_length = ACPI_MAX_DECIMAL_DIGITS;
			base = 10;
			break;

		default:

			

			string_length = ACPI_MUL_2(acpi_gbl_integer_byte_width);
			break;
		}

		return_desc =
		    acpi_ut_create_string_object((acpi_size) string_length);
		if (!return_desc) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		new_buf = return_desc->buffer.pointer;

		

		string_length =
		    acpi_ex_convert_to_ascii(obj_desc->integer.value, base,
					     new_buf,
					     acpi_gbl_integer_byte_width);

		

		return_desc->string.length = string_length;
		new_buf[string_length] = 0;
		break;

	case ACPI_TYPE_BUFFER:

		

		switch (type) {
		case ACPI_EXPLICIT_CONVERT_DECIMAL:	
			base = 10;

			for (i = 0; i < obj_desc->buffer.length; i++) {
				if (obj_desc->buffer.pointer[i] >= 100) {
					string_length += 4;
				} else if (obj_desc->buffer.pointer[i] >= 10) {
					string_length += 3;
				} else {
					string_length += 2;
				}
			}
			break;

		case ACPI_IMPLICIT_CONVERT_HEX:
			separator = ' ';
			string_length = (obj_desc->buffer.length * 3);
			break;

		case ACPI_EXPLICIT_CONVERT_HEX:	
			string_length = (obj_desc->buffer.length * 3);
			break;

		default:
			return_ACPI_STATUS(AE_BAD_PARAMETER);
		}

		if (string_length) {
			string_length--;
		}

		return_desc = acpi_ut_create_string_object((acpi_size)
							   string_length);
		if (!return_desc) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		new_buf = return_desc->buffer.pointer;

		for (i = 0; i < obj_desc->buffer.length; i++) {
			new_buf += acpi_ex_convert_to_ascii((u64) obj_desc->
							    buffer.pointer[i],
							    base, new_buf, 1);
			*new_buf++ = separator;	
		}

		if (obj_desc->buffer.length) {
			new_buf--;
		}
		*new_buf = 0;
		break;

	default:
		return_ACPI_STATUS(AE_TYPE);
	}

	*result_desc = return_desc;
	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_ex_convert_to_target_type(acpi_object_type destination_type,
			       union acpi_operand_object *source_desc,
			       union acpi_operand_object **result_desc,
			       struct acpi_walk_state *walk_state)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ex_convert_to_target_type);

	

	*result_desc = source_desc;

	switch (GET_CURRENT_ARG_TYPE(walk_state->op_info->runtime_args)) {
	case ARGI_SIMPLE_TARGET:
	case ARGI_FIXED_TARGET:
	case ARGI_INTEGER_REF:	

		switch (destination_type) {
		case ACPI_TYPE_LOCAL_REGION_FIELD:
			break;

		default:
			

			if (destination_type != source_desc->common.type) {
				ACPI_DEBUG_PRINT((ACPI_DB_INFO,
						  "Explicit operator, will store (%s) over existing type (%s)\n",
						  acpi_ut_get_object_type_name
						  (source_desc),
						  acpi_ut_get_type_name
						  (destination_type)));
				status = AE_TYPE;
			}
		}
		break;

	case ARGI_TARGETREF:

		switch (destination_type) {
		case ACPI_TYPE_INTEGER:
		case ACPI_TYPE_BUFFER_FIELD:
		case ACPI_TYPE_LOCAL_BANK_FIELD:
		case ACPI_TYPE_LOCAL_INDEX_FIELD:
			status =
			    acpi_ex_convert_to_integer(source_desc, result_desc,
						       16);
			break;

		case ACPI_TYPE_STRING:
			status =
			    acpi_ex_convert_to_string(source_desc, result_desc,
						      ACPI_IMPLICIT_CONVERT_HEX);
			break;

		case ACPI_TYPE_BUFFER:
			status =
			    acpi_ex_convert_to_buffer(source_desc, result_desc);
			break;

		default:
			ACPI_ERROR((AE_INFO,
				    "Bad destination type during conversion: 0x%X",
				    destination_type));
			status = AE_AML_INTERNAL;
			break;
		}
		break;

	case ARGI_REFERENCE:
		break;

	default:
		ACPI_ERROR((AE_INFO,
			    "Unknown Target type ID 0x%X AmlOpcode 0x%X DestType %s",
			    GET_CURRENT_ARG_TYPE(walk_state->op_info->
						 runtime_args),
			    walk_state->opcode,
			    acpi_ut_get_type_name(destination_type)));
		status = AE_AML_INTERNAL;
	}

	if (status == AE_TYPE) {
		status = AE_OK;
	}

	return_ACPI_STATUS(status);
}
