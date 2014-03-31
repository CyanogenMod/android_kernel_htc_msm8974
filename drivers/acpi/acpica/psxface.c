
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
#include "acparser.h"
#include "acdispat.h"
#include "acinterp.h"
#include "actables.h"

#define _COMPONENT          ACPI_PARSER
ACPI_MODULE_NAME("psxface")

static void acpi_ps_start_trace(struct acpi_evaluate_info *info);

static void acpi_ps_stop_trace(struct acpi_evaluate_info *info);

static void
acpi_ps_update_parameter_list(struct acpi_evaluate_info *info, u16 action);


acpi_status
acpi_debug_trace(char *name, u32 debug_level, u32 debug_layer, u32 flags)
{
	acpi_status status;

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	

	acpi_gbl_trace_method_name = *ACPI_CAST_PTR(u32, name);
	acpi_gbl_trace_flags = flags;

	if (debug_level) {
		acpi_gbl_trace_dbg_level = debug_level;
	}
	if (debug_layer) {
		acpi_gbl_trace_dbg_layer = debug_layer;
	}

	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	return (AE_OK);
}


static void acpi_ps_start_trace(struct acpi_evaluate_info *info)
{
	acpi_status status;

	ACPI_FUNCTION_ENTRY();

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return;
	}

	if ((!acpi_gbl_trace_method_name) ||
	    (acpi_gbl_trace_method_name != info->resolved_node->name.integer)) {
		goto exit;
	}

	acpi_gbl_original_dbg_level = acpi_dbg_level;
	acpi_gbl_original_dbg_layer = acpi_dbg_layer;

	acpi_dbg_level = 0x00FFFFFF;
	acpi_dbg_layer = ACPI_UINT32_MAX;

	if (acpi_gbl_trace_dbg_level) {
		acpi_dbg_level = acpi_gbl_trace_dbg_level;
	}
	if (acpi_gbl_trace_dbg_layer) {
		acpi_dbg_layer = acpi_gbl_trace_dbg_layer;
	}

      exit:
	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
}


static void acpi_ps_stop_trace(struct acpi_evaluate_info *info)
{
	acpi_status status;

	ACPI_FUNCTION_ENTRY();

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return;
	}

	if ((!acpi_gbl_trace_method_name) ||
	    (acpi_gbl_trace_method_name != info->resolved_node->name.integer)) {
		goto exit;
	}

	

	if (acpi_gbl_trace_flags & 1) {
		acpi_gbl_trace_method_name = 0;
		acpi_gbl_trace_dbg_level = 0;
		acpi_gbl_trace_dbg_layer = 0;
	}

	acpi_dbg_level = acpi_gbl_original_dbg_level;
	acpi_dbg_layer = acpi_gbl_original_dbg_layer;

      exit:
	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
}


acpi_status acpi_ps_execute_method(struct acpi_evaluate_info *info)
{
	acpi_status status;
	union acpi_parse_object *op;
	struct acpi_walk_state *walk_state;

	ACPI_FUNCTION_TRACE(ps_execute_method);

	

	acpi_tb_check_dsdt_header();

	

	if (!info || !info->resolved_node) {
		return_ACPI_STATUS(AE_NULL_ENTRY);
	}

	

	status =
	    acpi_ds_begin_method_execution(info->resolved_node, info->obj_desc,
					   NULL);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	acpi_ps_update_parameter_list(info, REF_INCREMENT);

	

	acpi_ps_start_trace(info);

	ACPI_DEBUG_PRINT((ACPI_DB_PARSE,
			  "**** Begin Method Parse/Execute [%4.4s] **** Node=%p Obj=%p\n",
			  info->resolved_node->name.ascii, info->resolved_node,
			  info->obj_desc));

	

	op = acpi_ps_create_scope_op();
	if (!op) {
		status = AE_NO_MEMORY;
		goto cleanup;
	}

	

	info->pass_number = ACPI_IMODE_EXECUTE;
	walk_state =
	    acpi_ds_create_walk_state(info->obj_desc->method.owner_id, NULL,
				      NULL, NULL);
	if (!walk_state) {
		status = AE_NO_MEMORY;
		goto cleanup;
	}

	status = acpi_ds_init_aml_walk(walk_state, op, info->resolved_node,
				       info->obj_desc->method.aml_start,
				       info->obj_desc->method.aml_length, info,
				       info->pass_number);
	if (ACPI_FAILURE(status)) {
		acpi_ds_delete_walk_state(walk_state);
		goto cleanup;
	}

	if (info->obj_desc->method.info_flags & ACPI_METHOD_MODULE_LEVEL) {
		walk_state->parse_flags |= ACPI_PARSE_MODULE_LEVEL;
	}

	

	if (info->obj_desc->method.info_flags & ACPI_METHOD_INTERNAL_ONLY) {
		status =
		    info->obj_desc->method.dispatch.implementation(walk_state);
		info->return_object = walk_state->return_desc;

		

		acpi_ds_scope_stack_clear(walk_state);
		acpi_ps_cleanup_scope(&walk_state->parser_state);
		acpi_ds_terminate_control_method(walk_state->method_desc,
						 walk_state);
		acpi_ds_delete_walk_state(walk_state);
		goto cleanup;
	}

	if (acpi_gbl_enable_interpreter_slack) {
		walk_state->implicit_return_obj =
		    acpi_ut_create_integer_object((u64) 0);
		if (!walk_state->implicit_return_obj) {
			status = AE_NO_MEMORY;
			acpi_ds_delete_walk_state(walk_state);
			goto cleanup;
		}
	}

	

	status = acpi_ps_parse_aml(walk_state);

	

      cleanup:
	acpi_ps_delete_parse_tree(op);

	

	acpi_ps_stop_trace(info);

	

	acpi_ps_update_parameter_list(info, REF_DECREMENT);

	

	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	if (info->return_object) {
		ACPI_DEBUG_PRINT((ACPI_DB_PARSE, "Method returned ObjDesc=%p\n",
				  info->return_object));
		ACPI_DUMP_STACK_ENTRY(info->return_object);

		status = AE_CTRL_RETURN_VALUE;
	}

	return_ACPI_STATUS(status);
}


static void
acpi_ps_update_parameter_list(struct acpi_evaluate_info *info, u16 action)
{
	u32 i;

	if (info->parameters) {

		

		for (i = 0; info->parameters[i]; i++) {

			

			(void)acpi_ut_update_object_reference(info->
							      parameters[i],
							      action);
		}
	}
}
