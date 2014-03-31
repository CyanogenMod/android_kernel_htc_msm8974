
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

#define _COMPONENT          ACPI_EVENTS
ACPI_MODULE_NAME("evrgnini")

static u8 acpi_ev_is_pci_root_bridge(struct acpi_namespace_node *node);


acpi_status
acpi_ev_system_memory_region_setup(acpi_handle handle,
				   u32 function,
				   void *handler_context, void **region_context)
{
	union acpi_operand_object *region_desc =
	    (union acpi_operand_object *)handle;
	struct acpi_mem_space_context *local_region_context;

	ACPI_FUNCTION_TRACE(ev_system_memory_region_setup);

	if (function == ACPI_REGION_DEACTIVATE) {
		if (*region_context) {
			local_region_context =
			    (struct acpi_mem_space_context *)*region_context;

			

			if (local_region_context->mapped_length) {
				acpi_os_unmap_memory(local_region_context->
						     mapped_logical_address,
						     local_region_context->
						     mapped_length);
			}
			ACPI_FREE(local_region_context);
			*region_context = NULL;
		}
		return_ACPI_STATUS(AE_OK);
	}

	

	local_region_context =
	    ACPI_ALLOCATE_ZEROED(sizeof(struct acpi_mem_space_context));
	if (!(local_region_context)) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	

	local_region_context->length = region_desc->region.length;
	local_region_context->address = region_desc->region.address;

	*region_context = local_region_context;
	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_ev_io_space_region_setup(acpi_handle handle,
			      u32 function,
			      void *handler_context, void **region_context)
{
	ACPI_FUNCTION_TRACE(ev_io_space_region_setup);

	if (function == ACPI_REGION_DEACTIVATE) {
		*region_context = NULL;
	} else {
		*region_context = handler_context;
	}

	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_ev_pci_config_region_setup(acpi_handle handle,
				u32 function,
				void *handler_context, void **region_context)
{
	acpi_status status = AE_OK;
	u64 pci_value;
	struct acpi_pci_id *pci_id = *region_context;
	union acpi_operand_object *handler_obj;
	struct acpi_namespace_node *parent_node;
	struct acpi_namespace_node *pci_root_node;
	struct acpi_namespace_node *pci_device_node;
	union acpi_operand_object *region_obj =
	    (union acpi_operand_object *)handle;

	ACPI_FUNCTION_TRACE(ev_pci_config_region_setup);

	handler_obj = region_obj->region.handler;
	if (!handler_obj) {
		ACPI_DEBUG_PRINT((ACPI_DB_OPREGION,
				  "Attempting to init a region %p, with no handler\n",
				  region_obj));
		return_ACPI_STATUS(AE_NOT_EXIST);
	}

	*region_context = NULL;
	if (function == ACPI_REGION_DEACTIVATE) {
		if (pci_id) {
			ACPI_FREE(pci_id);
		}
		return_ACPI_STATUS(status);
	}

	parent_node = region_obj->region.node->parent;


	if (handler_obj->address_space.node == acpi_gbl_root_node) {

		

		pci_root_node = parent_node;
		while (pci_root_node != acpi_gbl_root_node) {

			

			if (acpi_ev_is_pci_root_bridge(pci_root_node)) {

				

				status =
				    acpi_install_address_space_handler((acpi_handle) pci_root_node, ACPI_ADR_SPACE_PCI_CONFIG, ACPI_DEFAULT_HANDLER, NULL, NULL);
				if (ACPI_FAILURE(status)) {
					if (status == AE_SAME_HANDLER) {
						status = AE_OK;
					} else {
						ACPI_EXCEPTION((AE_INFO, status,
								"Could not install PciConfig handler "
								"for Root Bridge %4.4s",
								acpi_ut_get_node_name
								(pci_root_node)));
					}
				}
				break;
			}

			pci_root_node = pci_root_node->parent;
		}

		
	} else {
		pci_root_node = handler_obj->address_space.node;
	}

	if (region_obj->region.flags & AOPOBJ_SETUP_COMPLETE) {
		return_ACPI_STATUS(AE_OK);
	}

	

	pci_id = ACPI_ALLOCATE_ZEROED(sizeof(struct acpi_pci_id));
	if (!pci_id) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	pci_device_node = region_obj->region.node;
	while (pci_device_node && (pci_device_node->type != ACPI_TYPE_DEVICE)) {
		pci_device_node = pci_device_node->parent;
	}

	if (!pci_device_node) {
		ACPI_FREE(pci_id);
		return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
	}

	status = acpi_ut_evaluate_numeric_object(METHOD_NAME__ADR,
						 pci_device_node, &pci_value);

	if (ACPI_SUCCESS(status)) {
		pci_id->device = ACPI_HIWORD(ACPI_LODWORD(pci_value));
		pci_id->function = ACPI_LOWORD(ACPI_LODWORD(pci_value));
	}

	

	status = acpi_ut_evaluate_numeric_object(METHOD_NAME__SEG,
						 pci_root_node, &pci_value);
	if (ACPI_SUCCESS(status)) {
		pci_id->segment = ACPI_LOWORD(pci_value);
	}

	

	status = acpi_ut_evaluate_numeric_object(METHOD_NAME__BBN,
						 pci_root_node, &pci_value);
	if (ACPI_SUCCESS(status)) {
		pci_id->bus = ACPI_LOWORD(pci_value);
	}

	

	status =
	    acpi_hw_derive_pci_id(pci_id, pci_root_node,
				  region_obj->region.node);
	if (ACPI_FAILURE(status)) {
		ACPI_FREE(pci_id);
		return_ACPI_STATUS(status);
	}

	*region_context = pci_id;
	return_ACPI_STATUS(AE_OK);
}


static u8 acpi_ev_is_pci_root_bridge(struct acpi_namespace_node *node)
{
	acpi_status status;
	struct acpica_device_id *hid;
	struct acpica_device_id_list *cid;
	u32 i;
	u8 match;

	

	status = acpi_ut_execute_HID(node, &hid);
	if (ACPI_FAILURE(status)) {
		return (FALSE);
	}

	match = acpi_ut_is_pci_root_bridge(hid->string);
	ACPI_FREE(hid);

	if (match) {
		return (TRUE);
	}

	

	status = acpi_ut_execute_CID(node, &cid);
	if (ACPI_FAILURE(status)) {
		return (FALSE);
	}

	

	for (i = 0; i < cid->count; i++) {
		if (acpi_ut_is_pci_root_bridge(cid->ids[i].string)) {
			ACPI_FREE(cid);
			return (TRUE);
		}
	}

	ACPI_FREE(cid);
	return (FALSE);
}


acpi_status
acpi_ev_pci_bar_region_setup(acpi_handle handle,
			     u32 function,
			     void *handler_context, void **region_context)
{
	ACPI_FUNCTION_TRACE(ev_pci_bar_region_setup);

	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_ev_cmos_region_setup(acpi_handle handle,
			  u32 function,
			  void *handler_context, void **region_context)
{
	ACPI_FUNCTION_TRACE(ev_cmos_region_setup);

	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_ev_default_region_setup(acpi_handle handle,
			     u32 function,
			     void *handler_context, void **region_context)
{
	ACPI_FUNCTION_TRACE(ev_default_region_setup);

	if (function == ACPI_REGION_DEACTIVATE) {
		*region_context = NULL;
	} else {
		*region_context = handler_context;
	}

	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_ev_initialize_region(union acpi_operand_object *region_obj,
			  u8 acpi_ns_locked)
{
	union acpi_operand_object *handler_obj;
	union acpi_operand_object *obj_desc;
	acpi_adr_space_type space_id;
	struct acpi_namespace_node *node;
	acpi_status status;
	struct acpi_namespace_node *method_node;
	acpi_name *reg_name_ptr = (acpi_name *) METHOD_NAME__REG;
	union acpi_operand_object *region_obj2;

	ACPI_FUNCTION_TRACE_U32(ev_initialize_region, acpi_ns_locked);

	if (!region_obj) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	if (region_obj->common.flags & AOPOBJ_OBJECT_INITIALIZED) {
		return_ACPI_STATUS(AE_OK);
	}

	region_obj2 = acpi_ns_get_secondary_object(region_obj);
	if (!region_obj2) {
		return_ACPI_STATUS(AE_NOT_EXIST);
	}

	node = region_obj->region.node->parent;
	space_id = region_obj->region.space_id;

	

	region_obj->region.handler = NULL;
	region_obj2->extra.method_REG = NULL;
	region_obj->common.flags &= ~(AOPOBJ_SETUP_COMPLETE);
	region_obj->common.flags |= AOPOBJ_OBJECT_INITIALIZED;

	

	status =
	    acpi_ns_search_one_scope(*reg_name_ptr, node, ACPI_TYPE_METHOD,
				     &method_node);
	if (ACPI_SUCCESS(status)) {
		region_obj2->extra.method_REG = method_node;
	}

	while (node) {

		

		handler_obj = NULL;
		obj_desc = acpi_ns_get_attached_object(node);
		if (obj_desc) {

			

			switch (node->type) {
			case ACPI_TYPE_DEVICE:

				handler_obj = obj_desc->device.handler;
				break;

			case ACPI_TYPE_PROCESSOR:

				handler_obj = obj_desc->processor.handler;
				break;

			case ACPI_TYPE_THERMAL:

				handler_obj = obj_desc->thermal_zone.handler;
				break;

			case ACPI_TYPE_METHOD:
				if (obj_desc->method.
				    info_flags & ACPI_METHOD_MODULE_LEVEL) {
					handler_obj =
					    obj_desc->method.dispatch.handler;
				}
				break;

			default:
				
				break;
			}

			while (handler_obj) {

				

				if (handler_obj->address_space.space_id ==
				    space_id) {

					

					ACPI_DEBUG_PRINT((ACPI_DB_OPREGION,
							  "Found handler %p for region %p in obj %p\n",
							  handler_obj,
							  region_obj,
							  obj_desc));

					status =
					    acpi_ev_attach_region(handler_obj,
								  region_obj,
								  acpi_ns_locked);

					if (acpi_ns_locked) {
						status =
						    acpi_ut_release_mutex
						    (ACPI_MTX_NAMESPACE);
						if (ACPI_FAILURE(status)) {
							return_ACPI_STATUS
							    (status);
						}
					}

					status =
					    acpi_ev_execute_reg_method
					    (region_obj, ACPI_REG_CONNECT);

					if (acpi_ns_locked) {
						status =
						    acpi_ut_acquire_mutex
						    (ACPI_MTX_NAMESPACE);
						if (ACPI_FAILURE(status)) {
							return_ACPI_STATUS
							    (status);
						}
					}

					return_ACPI_STATUS(AE_OK);
				}

				

				handler_obj = handler_obj->address_space.next;
			}
		}

		

		node = node->parent;
	}

	

	ACPI_DEBUG_PRINT((ACPI_DB_OPREGION,
			  "No handler for RegionType %s(%X) (RegionObj %p)\n",
			  acpi_ut_get_region_name(space_id), space_id,
			  region_obj));

	return_ACPI_STATUS(AE_NOT_EXIST);
}
