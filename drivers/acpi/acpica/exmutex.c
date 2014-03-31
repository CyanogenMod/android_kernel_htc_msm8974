

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
#include "acevents.h"

#define _COMPONENT          ACPI_EXECUTER
ACPI_MODULE_NAME("exmutex")

static void
acpi_ex_link_mutex(union acpi_operand_object *obj_desc,
		   struct acpi_thread_state *thread);


void acpi_ex_unlink_mutex(union acpi_operand_object *obj_desc)
{
	struct acpi_thread_state *thread = obj_desc->mutex.owner_thread;

	if (!thread) {
		return;
	}

	

	if (obj_desc->mutex.next) {
		(obj_desc->mutex.next)->mutex.prev = obj_desc->mutex.prev;
	}

	if (obj_desc->mutex.prev) {
		(obj_desc->mutex.prev)->mutex.next = obj_desc->mutex.next;

		(obj_desc->mutex.prev)->mutex.original_sync_level =
		    obj_desc->mutex.original_sync_level;
	} else {
		thread->acquired_mutex_list = obj_desc->mutex.next;
	}
}


static void
acpi_ex_link_mutex(union acpi_operand_object *obj_desc,
		   struct acpi_thread_state *thread)
{
	union acpi_operand_object *list_head;

	list_head = thread->acquired_mutex_list;

	

	obj_desc->mutex.prev = NULL;
	obj_desc->mutex.next = list_head;

	

	if (list_head) {
		list_head->mutex.prev = obj_desc;
	}

	

	thread->acquired_mutex_list = obj_desc;
}


acpi_status
acpi_ex_acquire_mutex_object(u16 timeout,
			     union acpi_operand_object *obj_desc,
			     acpi_thread_id thread_id)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE_PTR(ex_acquire_mutex_object, obj_desc);

	if (!obj_desc) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	

	if (obj_desc->mutex.thread_id == thread_id) {
		obj_desc->mutex.acquisition_depth++;
		return_ACPI_STATUS(AE_OK);
	}

	

	if (obj_desc == acpi_gbl_global_lock_mutex) {
		status = acpi_ev_acquire_global_lock(timeout);
	} else {
		status = acpi_ex_system_wait_mutex(obj_desc->mutex.os_mutex,
						   timeout);
	}

	if (ACPI_FAILURE(status)) {

		

		return_ACPI_STATUS(status);
	}

	

	obj_desc->mutex.thread_id = thread_id;
	obj_desc->mutex.acquisition_depth = 1;
	obj_desc->mutex.original_sync_level = 0;
	obj_desc->mutex.owner_thread = NULL;	

	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_ex_acquire_mutex(union acpi_operand_object *time_desc,
		      union acpi_operand_object *obj_desc,
		      struct acpi_walk_state *walk_state)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE_PTR(ex_acquire_mutex, obj_desc);

	if (!obj_desc) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	

	if (!walk_state->thread) {
		ACPI_ERROR((AE_INFO,
			    "Cannot acquire Mutex [%4.4s], null thread info",
			    acpi_ut_get_node_name(obj_desc->mutex.node)));
		return_ACPI_STATUS(AE_AML_INTERNAL);
	}

	if (walk_state->thread->current_sync_level > obj_desc->mutex.sync_level) {
		ACPI_ERROR((AE_INFO,
			    "Cannot acquire Mutex [%4.4s], current SyncLevel is too large (%u)",
			    acpi_ut_get_node_name(obj_desc->mutex.node),
			    walk_state->thread->current_sync_level));
		return_ACPI_STATUS(AE_AML_MUTEX_ORDER);
	}

	status = acpi_ex_acquire_mutex_object((u16) time_desc->integer.value,
					      obj_desc,
					      walk_state->thread->thread_id);
	if (ACPI_SUCCESS(status) && obj_desc->mutex.acquisition_depth == 1) {

		

		obj_desc->mutex.owner_thread = walk_state->thread;
		obj_desc->mutex.original_sync_level =
		    walk_state->thread->current_sync_level;
		walk_state->thread->current_sync_level =
		    obj_desc->mutex.sync_level;

		

		acpi_ex_link_mutex(obj_desc, walk_state->thread);
	}

	return_ACPI_STATUS(status);
}


acpi_status acpi_ex_release_mutex_object(union acpi_operand_object *obj_desc)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ex_release_mutex_object);

	if (obj_desc->mutex.acquisition_depth == 0) {
		return (AE_NOT_ACQUIRED);
	}

	

	obj_desc->mutex.acquisition_depth--;
	if (obj_desc->mutex.acquisition_depth != 0) {

		

		return_ACPI_STATUS(AE_OK);
	}

	if (obj_desc->mutex.owner_thread) {

		

		acpi_ex_unlink_mutex(obj_desc);
		obj_desc->mutex.owner_thread = NULL;
	}

	

	if (obj_desc == acpi_gbl_global_lock_mutex) {
		status = acpi_ev_release_global_lock();
	} else {
		acpi_os_release_mutex(obj_desc->mutex.os_mutex);
	}

	

	obj_desc->mutex.thread_id = 0;
	return_ACPI_STATUS(status);
}


acpi_status
acpi_ex_release_mutex(union acpi_operand_object *obj_desc,
		      struct acpi_walk_state *walk_state)
{
	acpi_status status = AE_OK;
	u8 previous_sync_level;
	struct acpi_thread_state *owner_thread;

	ACPI_FUNCTION_TRACE(ex_release_mutex);

	if (!obj_desc) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	owner_thread = obj_desc->mutex.owner_thread;

	

	if (!owner_thread) {
		ACPI_ERROR((AE_INFO,
			    "Cannot release Mutex [%4.4s], not acquired",
			    acpi_ut_get_node_name(obj_desc->mutex.node)));
		return_ACPI_STATUS(AE_AML_MUTEX_NOT_ACQUIRED);
	}

	
	if (!walk_state->thread) {
		ACPI_ERROR((AE_INFO,
			    "Cannot release Mutex [%4.4s], null thread info",
			    acpi_ut_get_node_name(obj_desc->mutex.node)));
		return_ACPI_STATUS(AE_AML_INTERNAL);
	}

	if ((owner_thread->thread_id != walk_state->thread->thread_id) &&
	    (obj_desc != acpi_gbl_global_lock_mutex)) {
		ACPI_ERROR((AE_INFO,
			    "Thread %u cannot release Mutex [%4.4s] acquired by thread %u",
			    (u32)walk_state->thread->thread_id,
			    acpi_ut_get_node_name(obj_desc->mutex.node),
			    (u32)owner_thread->thread_id));
		return_ACPI_STATUS(AE_AML_NOT_OWNER);
	}

	if (obj_desc->mutex.sync_level != owner_thread->current_sync_level) {
		ACPI_ERROR((AE_INFO,
			    "Cannot release Mutex [%4.4s], SyncLevel mismatch: mutex %u current %u",
			    acpi_ut_get_node_name(obj_desc->mutex.node),
			    obj_desc->mutex.sync_level,
			    walk_state->thread->current_sync_level));
		return_ACPI_STATUS(AE_AML_MUTEX_ORDER);
	}

	previous_sync_level =
	    owner_thread->acquired_mutex_list->mutex.original_sync_level;

	status = acpi_ex_release_mutex_object(obj_desc);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	if (obj_desc->mutex.acquisition_depth == 0) {

		

		owner_thread->current_sync_level = previous_sync_level;
	}

	return_ACPI_STATUS(status);
}


void acpi_ex_release_all_mutexes(struct acpi_thread_state *thread)
{
	union acpi_operand_object *next = thread->acquired_mutex_list;
	union acpi_operand_object *obj_desc;

	ACPI_FUNCTION_ENTRY();

	

	while (next) {
		obj_desc = next;
		next = obj_desc->mutex.next;

		obj_desc->mutex.prev = NULL;
		obj_desc->mutex.next = NULL;
		obj_desc->mutex.acquisition_depth = 0;

		

		if (obj_desc == acpi_gbl_global_lock_mutex) {

			

			(void)acpi_ev_release_global_lock();
		} else {
			acpi_os_release_mutex(obj_desc->mutex.os_mutex);
		}

		

		obj_desc->mutex.owner_thread = NULL;
		obj_desc->mutex.thread_id = 0;

		

		thread->current_sync_level =
		    obj_desc->mutex.original_sync_level;
	}
}
