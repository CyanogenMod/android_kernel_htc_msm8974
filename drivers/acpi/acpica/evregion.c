
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
#include "acnamesp.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_EVENTS
ACPI_MODULE_NAME("evregion")

static u8
acpi_ev_has_default_handler(struct acpi_namespace_node *node,
			    acpi_adr_space_type space_id);

static void acpi_ev_orphan_ec_reg_method(void);

static acpi_status
acpi_ev_reg_run(acpi_handle obj_handle,
		u32 level, void *context, void **return_value);

static acpi_status
acpi_ev_install_handler(acpi_handle obj_handle,
			u32 level, void *context, void **return_value);


#define ACPI_NUM_DEFAULT_SPACES     4

static u8 acpi_gbl_default_address_spaces[ACPI_NUM_DEFAULT_SPACES] = {
	ACPI_ADR_SPACE_SYSTEM_MEMORY,
	ACPI_ADR_SPACE_SYSTEM_IO,
	ACPI_ADR_SPACE_PCI_CONFIG,
	ACPI_ADR_SPACE_DATA_TABLE
};


acpi_status acpi_ev_install_region_handlers(void)
{
	acpi_status status;
	u32 i;

	ACPI_FUNCTION_TRACE(ev_install_region_handlers);

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	for (i = 0; i < ACPI_NUM_DEFAULT_SPACES; i++) {
		status = acpi_ev_install_space_handler(acpi_gbl_root_node,
						       acpi_gbl_default_address_spaces
						       [i],
						       ACPI_DEFAULT_HANDLER,
						       NULL, NULL);
		switch (status) {
		case AE_OK:
		case AE_SAME_HANDLER:
		case AE_ALREADY_EXISTS:

			

			status = AE_OK;
			break;

		default:

			goto unlock_and_exit;
		}
	}

      unlock_and_exit:
	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	return_ACPI_STATUS(status);
}


static u8
acpi_ev_has_default_handler(struct acpi_namespace_node *node,
			    acpi_adr_space_type space_id)
{
	union acpi_operand_object *obj_desc;
	union acpi_operand_object *handler_obj;

	

	obj_desc = acpi_ns_get_attached_object(node);
	if (obj_desc) {
		handler_obj = obj_desc->device.handler;

		

		while (handler_obj) {
			if (handler_obj->address_space.space_id == space_id) {
				if (handler_obj->address_space.handler_flags &
				    ACPI_ADDR_HANDLER_DEFAULT_INSTALLED) {
					return (TRUE);
				}
			}

			handler_obj = handler_obj->address_space.next;
		}
	}

	return (FALSE);
}


acpi_status acpi_ev_initialize_op_regions(void)
{
	acpi_status status;
	u32 i;

	ACPI_FUNCTION_TRACE(ev_initialize_op_regions);

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	

	for (i = 0; i < ACPI_NUM_DEFAULT_SPACES; i++) {
		if (acpi_ev_has_default_handler(acpi_gbl_root_node,
						acpi_gbl_default_address_spaces
						[i])) {
			status =
			    acpi_ev_execute_reg_methods(acpi_gbl_root_node,
							acpi_gbl_default_address_spaces
							[i]);
		}
	}

	acpi_gbl_reg_methods_executed = TRUE;

	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	return_ACPI_STATUS(status);
}


acpi_status
acpi_ev_execute_reg_method(union acpi_operand_object *region_obj, u32 function)
{
	struct acpi_evaluate_info *info;
	union acpi_operand_object *args[3];
	union acpi_operand_object *region_obj2;
	acpi_status status;

	ACPI_FUNCTION_TRACE(ev_execute_reg_method);

	region_obj2 = acpi_ns_get_secondary_object(region_obj);
	if (!region_obj2) {
		return_ACPI_STATUS(AE_NOT_EXIST);
	}

	if (region_obj2->extra.method_REG == NULL) {
		return_ACPI_STATUS(AE_OK);
	}

	

	info = ACPI_ALLOCATE_ZEROED(sizeof(struct acpi_evaluate_info));
	if (!info) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	info->prefix_node = region_obj2->extra.method_REG;
	info->pathname = NULL;
	info->parameters = args;
	info->flags = ACPI_IGNORE_RETURN_VALUE;

	args[0] =
	    acpi_ut_create_integer_object((u64) region_obj->region.space_id);
	if (!args[0]) {
		status = AE_NO_MEMORY;
		goto cleanup1;
	}

	args[1] = acpi_ut_create_integer_object((u64) function);
	if (!args[1]) {
		status = AE_NO_MEMORY;
		goto cleanup2;
	}

	args[2] = NULL;		

	

	ACPI_DEBUG_EXEC(acpi_ut_display_init_pathname
			(ACPI_TYPE_METHOD, info->prefix_node, NULL));

	status = acpi_ns_evaluate(info);
	acpi_ut_remove_reference(args[1]);

      cleanup2:
	acpi_ut_remove_reference(args[0]);

      cleanup1:
	ACPI_FREE(info);
	return_ACPI_STATUS(status);
}


acpi_status
acpi_ev_address_space_dispatch(union acpi_operand_object *region_obj,
			       union acpi_operand_object *field_obj,
			       u32 function,
			       u32 region_offset, u32 bit_width, u64 *value)
{
	acpi_status status;
	acpi_adr_space_handler handler;
	acpi_adr_space_setup region_setup;
	union acpi_operand_object *handler_desc;
	union acpi_operand_object *region_obj2;
	void *region_context = NULL;
	struct acpi_connection_info *context;

	ACPI_FUNCTION_TRACE(ev_address_space_dispatch);

	region_obj2 = acpi_ns_get_secondary_object(region_obj);
	if (!region_obj2) {
		return_ACPI_STATUS(AE_NOT_EXIST);
	}

	

	handler_desc = region_obj->region.handler;
	if (!handler_desc) {
		ACPI_ERROR((AE_INFO,
			    "No handler for Region [%4.4s] (%p) [%s]",
			    acpi_ut_get_node_name(region_obj->region.node),
			    region_obj,
			    acpi_ut_get_region_name(region_obj->region.
						    space_id)));

		return_ACPI_STATUS(AE_NOT_EXIST);
	}

	context = handler_desc->address_space.context;

	if (!(region_obj->region.flags & AOPOBJ_SETUP_COMPLETE)) {

		

		region_setup = handler_desc->address_space.setup;
		if (!region_setup) {

			

			ACPI_ERROR((AE_INFO,
				    "No init routine for region(%p) [%s]",
				    region_obj,
				    acpi_ut_get_region_name(region_obj->region.
							    space_id)));
			return_ACPI_STATUS(AE_NOT_EXIST);
		}

		acpi_ex_exit_interpreter();

		status = region_setup(region_obj, ACPI_REGION_ACTIVATE,
				      context, &region_context);

		

		acpi_ex_enter_interpreter();

		

		if (ACPI_FAILURE(status)) {
			ACPI_EXCEPTION((AE_INFO, status,
					"During region initialization: [%s]",
					acpi_ut_get_region_name(region_obj->
								region.
								space_id)));
			return_ACPI_STATUS(status);
		}

		

		if (!(region_obj->region.flags & AOPOBJ_SETUP_COMPLETE)) {
			region_obj->region.flags |= AOPOBJ_SETUP_COMPLETE;

			if (region_obj2->extra.region_context) {

				

				ACPI_FREE(region_context);
			} else {
				region_obj2->extra.region_context =
				    region_context;
			}
		}
	}

	

	handler = handler_desc->address_space.handler;

	ACPI_DEBUG_PRINT((ACPI_DB_OPREGION,
			  "Handler %p (@%p) Address %8.8X%8.8X [%s]\n",
			  &region_obj->region.handler->address_space, handler,
			  ACPI_FORMAT_NATIVE_UINT(region_obj->region.address +
						  region_offset),
			  acpi_ut_get_region_name(region_obj->region.
						  space_id)));

	if (((region_obj->region.space_id == ACPI_ADR_SPACE_GSBUS) ||
	     (region_obj->region.space_id == ACPI_ADR_SPACE_GPIO)) &&
	    context && field_obj) {

		

		context->connection = field_obj->field.resource_buffer;
		context->length = field_obj->field.resource_length;
		context->access_length = field_obj->field.access_length;
	}

	if (!(handler_desc->address_space.handler_flags &
	      ACPI_ADDR_HANDLER_DEFAULT_INSTALLED)) {
		acpi_ex_exit_interpreter();
	}

	

	status = handler(function,
			 (region_obj->region.address + region_offset),
			 bit_width, value, context,
			 region_obj2->extra.region_context);

	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status, "Returned by Handler for [%s]",
				acpi_ut_get_region_name(region_obj->region.
							space_id)));
	}

	if (!(handler_desc->address_space.handler_flags &
	      ACPI_ADDR_HANDLER_DEFAULT_INSTALLED)) {
		acpi_ex_enter_interpreter();
	}

	return_ACPI_STATUS(status);
}


void
acpi_ev_detach_region(union acpi_operand_object *region_obj,
		      u8 acpi_ns_is_locked)
{
	union acpi_operand_object *handler_obj;
	union acpi_operand_object *obj_desc;
	union acpi_operand_object **last_obj_ptr;
	acpi_adr_space_setup region_setup;
	void **region_context;
	union acpi_operand_object *region_obj2;
	acpi_status status;

	ACPI_FUNCTION_TRACE(ev_detach_region);

	region_obj2 = acpi_ns_get_secondary_object(region_obj);
	if (!region_obj2) {
		return_VOID;
	}
	region_context = &region_obj2->extra.region_context;

	

	handler_obj = region_obj->region.handler;
	if (!handler_obj) {

		

		return_VOID;
	}

	

	obj_desc = handler_obj->address_space.region_list;
	last_obj_ptr = &handler_obj->address_space.region_list;

	while (obj_desc) {

		

		if (obj_desc == region_obj) {
			ACPI_DEBUG_PRINT((ACPI_DB_OPREGION,
					  "Removing Region %p from address handler %p\n",
					  region_obj, handler_obj));

			

			*last_obj_ptr = obj_desc->region.next;
			obj_desc->region.next = NULL;	

			if (acpi_ns_is_locked) {
				status =
				    acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
				if (ACPI_FAILURE(status)) {
					return_VOID;
				}
			}

			

			status =
			    acpi_ev_execute_reg_method(region_obj,
						       ACPI_REG_DISCONNECT);
			if (ACPI_FAILURE(status)) {
				ACPI_EXCEPTION((AE_INFO, status,
						"from region _REG, [%s]",
						acpi_ut_get_region_name
						(region_obj->region.space_id)));
			}

			if (acpi_ns_is_locked) {
				status =
				    acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
				if (ACPI_FAILURE(status)) {
					return_VOID;
				}
			}

			if (region_obj->region.flags & AOPOBJ_SETUP_COMPLETE) {
				region_setup = handler_obj->address_space.setup;
				status =
				    region_setup(region_obj,
						 ACPI_REGION_DEACTIVATE,
						 handler_obj->address_space.
						 context, region_context);

				

				if (ACPI_FAILURE(status)) {
					ACPI_EXCEPTION((AE_INFO, status,
							"from region handler - deactivate, [%s]",
							acpi_ut_get_region_name
							(region_obj->region.
							 space_id)));
				}

				region_obj->region.flags &=
				    ~(AOPOBJ_SETUP_COMPLETE);
			}

			region_obj->region.handler = NULL;
			acpi_ut_remove_reference(handler_obj);

			return_VOID;
		}

		

		last_obj_ptr = &obj_desc->region.next;
		obj_desc = obj_desc->region.next;
	}

	

	ACPI_DEBUG_PRINT((ACPI_DB_OPREGION,
			  "Cannot remove region %p from address handler %p\n",
			  region_obj, handler_obj));

	return_VOID;
}


acpi_status
acpi_ev_attach_region(union acpi_operand_object *handler_obj,
		      union acpi_operand_object *region_obj,
		      u8 acpi_ns_is_locked)
{

	ACPI_FUNCTION_TRACE(ev_attach_region);

	ACPI_DEBUG_PRINT((ACPI_DB_OPREGION,
			  "Adding Region [%4.4s] %p to address handler %p [%s]\n",
			  acpi_ut_get_node_name(region_obj->region.node),
			  region_obj, handler_obj,
			  acpi_ut_get_region_name(region_obj->region.
						  space_id)));

	

	region_obj->region.next = handler_obj->address_space.region_list;
	handler_obj->address_space.region_list = region_obj;

	

	if (region_obj->region.handler) {
		return_ACPI_STATUS(AE_ALREADY_EXISTS);
	}

	region_obj->region.handler = handler_obj;
	acpi_ut_add_reference(handler_obj);

	return_ACPI_STATUS(AE_OK);
}


static acpi_status
acpi_ev_install_handler(acpi_handle obj_handle,
			u32 level, void *context, void **return_value)
{
	union acpi_operand_object *handler_obj;
	union acpi_operand_object *next_handler_obj;
	union acpi_operand_object *obj_desc;
	struct acpi_namespace_node *node;
	acpi_status status;

	ACPI_FUNCTION_NAME(ev_install_handler);

	handler_obj = (union acpi_operand_object *)context;

	

	if (!handler_obj) {
		return (AE_OK);
	}

	

	node = acpi_ns_validate_handle(obj_handle);
	if (!node) {
		return (AE_BAD_PARAMETER);
	}

	if ((node->type != ACPI_TYPE_DEVICE) &&
	    (node->type != ACPI_TYPE_REGION) && (node != acpi_gbl_root_node)) {
		return (AE_OK);
	}

	

	obj_desc = acpi_ns_get_attached_object(node);
	if (!obj_desc) {

		

		return (AE_OK);
	}

	

	if (obj_desc->common.type == ACPI_TYPE_DEVICE) {

		

		next_handler_obj = obj_desc->device.handler;
		while (next_handler_obj) {

			

			if (next_handler_obj->address_space.space_id ==
			    handler_obj->address_space.space_id) {
				ACPI_DEBUG_PRINT((ACPI_DB_OPREGION,
						  "Found handler for region [%s] in device %p(%p) "
						  "handler %p\n",
						  acpi_ut_get_region_name
						  (handler_obj->address_space.
						   space_id), obj_desc,
						  next_handler_obj,
						  handler_obj));

				return (AE_CTRL_DEPTH);
			}

			

			next_handler_obj = next_handler_obj->address_space.next;
		}

		return (AE_OK);
	}

	

	if (obj_desc->region.space_id != handler_obj->address_space.space_id) {

		

		return (AE_OK);
	}

	acpi_ev_detach_region(obj_desc, FALSE);

	

	status = acpi_ev_attach_region(handler_obj, obj_desc, FALSE);
	return (status);
}


acpi_status
acpi_ev_install_space_handler(struct acpi_namespace_node * node,
			      acpi_adr_space_type space_id,
			      acpi_adr_space_handler handler,
			      acpi_adr_space_setup setup, void *context)
{
	union acpi_operand_object *obj_desc;
	union acpi_operand_object *handler_obj;
	acpi_status status;
	acpi_object_type type;
	u8 flags = 0;

	ACPI_FUNCTION_TRACE(ev_install_space_handler);

	if ((node->type != ACPI_TYPE_DEVICE) &&
	    (node->type != ACPI_TYPE_PROCESSOR) &&
	    (node->type != ACPI_TYPE_THERMAL) && (node != acpi_gbl_root_node)) {
		status = AE_BAD_PARAMETER;
		goto unlock_and_exit;
	}

	if (handler == ACPI_DEFAULT_HANDLER) {
		flags = ACPI_ADDR_HANDLER_DEFAULT_INSTALLED;

		switch (space_id) {
		case ACPI_ADR_SPACE_SYSTEM_MEMORY:
			handler = acpi_ex_system_memory_space_handler;
			setup = acpi_ev_system_memory_region_setup;
			break;

		case ACPI_ADR_SPACE_SYSTEM_IO:
			handler = acpi_ex_system_io_space_handler;
			setup = acpi_ev_io_space_region_setup;
			break;

		case ACPI_ADR_SPACE_PCI_CONFIG:
			handler = acpi_ex_pci_config_space_handler;
			setup = acpi_ev_pci_config_region_setup;
			break;

		case ACPI_ADR_SPACE_CMOS:
			handler = acpi_ex_cmos_space_handler;
			setup = acpi_ev_cmos_region_setup;
			break;

		case ACPI_ADR_SPACE_PCI_BAR_TARGET:
			handler = acpi_ex_pci_bar_space_handler;
			setup = acpi_ev_pci_bar_region_setup;
			break;

		case ACPI_ADR_SPACE_DATA_TABLE:
			handler = acpi_ex_data_table_space_handler;
			setup = NULL;
			break;

		default:
			status = AE_BAD_PARAMETER;
			goto unlock_and_exit;
		}
	}

	

	if (!setup) {
		setup = acpi_ev_default_region_setup;
	}

	

	obj_desc = acpi_ns_get_attached_object(node);
	if (obj_desc) {
		handler_obj = obj_desc->device.handler;

		

		while (handler_obj) {

			

			if (handler_obj->address_space.space_id == space_id) {
				if (handler_obj->address_space.handler ==
				    handler) {
					status = AE_SAME_HANDLER;
					goto unlock_and_exit;
				} else {
					

					status = AE_ALREADY_EXISTS;
				}
				goto unlock_and_exit;
			}

			

			handler_obj = handler_obj->address_space.next;
		}
	} else {
		ACPI_DEBUG_PRINT((ACPI_DB_OPREGION,
				  "Creating object on Device %p while installing handler\n",
				  node));

		

		if (node->type == ACPI_TYPE_ANY) {
			type = ACPI_TYPE_DEVICE;
		} else {
			type = node->type;
		}

		obj_desc = acpi_ut_create_internal_object(type);
		if (!obj_desc) {
			status = AE_NO_MEMORY;
			goto unlock_and_exit;
		}

		

		obj_desc->common.type = (u8) type;

		

		status = acpi_ns_attach_object(node, obj_desc, type);

		

		acpi_ut_remove_reference(obj_desc);

		if (ACPI_FAILURE(status)) {
			goto unlock_and_exit;
		}
	}

	ACPI_DEBUG_PRINT((ACPI_DB_OPREGION,
			  "Installing address handler for region %s(%X) on Device %4.4s %p(%p)\n",
			  acpi_ut_get_region_name(space_id), space_id,
			  acpi_ut_get_node_name(node), node, obj_desc));

	handler_obj =
	    acpi_ut_create_internal_object(ACPI_TYPE_LOCAL_ADDRESS_HANDLER);
	if (!handler_obj) {
		status = AE_NO_MEMORY;
		goto unlock_and_exit;
	}

	

	handler_obj->address_space.space_id = (u8) space_id;
	handler_obj->address_space.handler_flags = flags;
	handler_obj->address_space.region_list = NULL;
	handler_obj->address_space.node = node;
	handler_obj->address_space.handler = handler;
	handler_obj->address_space.context = context;
	handler_obj->address_space.setup = setup;

	

	handler_obj->address_space.next = obj_desc->device.handler;

	obj_desc->device.handler = handler_obj;

	status = acpi_ns_walk_namespace(ACPI_TYPE_ANY, node, ACPI_UINT32_MAX,
					ACPI_NS_WALK_UNLOCK,
					acpi_ev_install_handler, NULL,
					handler_obj, NULL);

      unlock_and_exit:
	return_ACPI_STATUS(status);
}


acpi_status
acpi_ev_execute_reg_methods(struct acpi_namespace_node *node,
			    acpi_adr_space_type space_id)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(ev_execute_reg_methods);

	status = acpi_ns_walk_namespace(ACPI_TYPE_ANY, node, ACPI_UINT32_MAX,
					ACPI_NS_WALK_UNLOCK, acpi_ev_reg_run,
					NULL, &space_id, NULL);

	

	if (space_id == ACPI_ADR_SPACE_EC) {
		acpi_ev_orphan_ec_reg_method();
	}

	return_ACPI_STATUS(status);
}


static acpi_status
acpi_ev_reg_run(acpi_handle obj_handle,
		u32 level, void *context, void **return_value)
{
	union acpi_operand_object *obj_desc;
	struct acpi_namespace_node *node;
	acpi_adr_space_type space_id;
	acpi_status status;

	space_id = *ACPI_CAST_PTR(acpi_adr_space_type, context);

	

	node = acpi_ns_validate_handle(obj_handle);
	if (!node) {
		return (AE_BAD_PARAMETER);
	}

	if ((node->type != ACPI_TYPE_REGION) && (node != acpi_gbl_root_node)) {
		return (AE_OK);
	}

	

	obj_desc = acpi_ns_get_attached_object(node);
	if (!obj_desc) {

		

		return (AE_OK);
	}

	

	if (obj_desc->region.space_id != space_id) {

		

		return (AE_OK);
	}

	status = acpi_ev_execute_reg_method(obj_desc, ACPI_REG_CONNECT);
	return (status);
}


static void acpi_ev_orphan_ec_reg_method(void)
{
	struct acpi_table_ecdt *table;
	acpi_status status;
	struct acpi_object_list args;
	union acpi_object objects[2];
	struct acpi_namespace_node *ec_device_node;
	struct acpi_namespace_node *reg_method;
	struct acpi_namespace_node *next_node;

	ACPI_FUNCTION_TRACE(ev_orphan_ec_reg_method);

	

	status = acpi_get_table(ACPI_SIG_ECDT, 0,
				ACPI_CAST_INDIRECT_PTR(struct acpi_table_header,
						       &table));
	if (ACPI_FAILURE(status)) {
		return_VOID;
	}

	

	if (!(*table->id)) {
		return_VOID;
	}

	

	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);

	

	status = acpi_get_handle(NULL,
				 ACPI_CAST_PTR(char, table->id),
				 ACPI_CAST_PTR(acpi_handle, &ec_device_node));
	if (ACPI_FAILURE(status)) {
		goto exit;
	}

	

	status = acpi_get_handle(ec_device_node,
				 METHOD_NAME__REG, ACPI_CAST_PTR(acpi_handle,
								 &reg_method));
	if (ACPI_FAILURE(status)) {
		goto exit;
	}

	next_node = acpi_ns_get_next_node(ec_device_node, NULL);
	while (next_node) {
		if ((next_node->type == ACPI_TYPE_REGION) &&
		    (next_node->object) &&
		    (next_node->object->region.space_id == ACPI_ADR_SPACE_EC)) {
			goto exit;	
		}
		next_node = acpi_ns_get_next_node(ec_device_node, next_node);
	}

	

	args.count = 2;
	args.pointer = objects;
	objects[0].type = ACPI_TYPE_INTEGER;
	objects[0].integer.value = ACPI_ADR_SPACE_EC;
	objects[1].type = ACPI_TYPE_INTEGER;
	objects[1].integer.value = ACPI_REG_CONNECT;

	status = acpi_evaluate_object(reg_method, NULL, &args, NULL);

      exit:
	

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	return_VOID;
}
