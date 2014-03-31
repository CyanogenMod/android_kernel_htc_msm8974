/*
 * MobiCore Driver API.
 *
 * The MobiCore (MC) Driver API provides access functions to the MobiCore
 * runtime environment and the contained Trustlets.
 *
 * <-- Copyright Giesecke & Devrient GmbH 2009 - 2012 -->
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *	products derived from this software without specific prior
 *	written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _MOBICORE_DRIVER_API_H_
#define _MOBICORE_DRIVER_API_H_

#define __MC_CLIENT_LIB_API

#include "mcuuid.h"

enum mc_result {
	
	MC_DRV_OK			= 0,
	
	MC_DRV_NO_NOTIFICATION		= 1,
	
	MC_DRV_ERR_NOTIFICATION		= 2,
	
	MC_DRV_ERR_NOT_IMPLEMENTED	= 3,
	
	MC_DRV_ERR_OUT_OF_RESOURCES	= 4,
	
	MC_DRV_ERR_INIT			= 5,
	
	MC_DRV_ERR_UNKNOWN		= 6,
	
	MC_DRV_ERR_UNKNOWN_DEVICE	= 7,
	
	MC_DRV_ERR_UNKNOWN_SESSION	= 8,
	
	MC_DRV_ERR_INVALID_OPERATION	= 9,
	
	MC_DRV_ERR_INVALID_RESPONSE	= 10,
	
	MC_DRV_ERR_TIMEOUT		= 11,
	
	MC_DRV_ERR_NO_FREE_MEMORY	= 12,
	
	MC_DRV_ERR_FREE_MEMORY_FAILED	= 13,
	
	MC_DRV_ERR_SESSION_PENDING	= 14,
	
	MC_DRV_ERR_DAEMON_UNREACHABLE	= 15,
	
	MC_DRV_ERR_INVALID_DEVICE_FILE	= 16,
	
	MC_DRV_ERR_INVALID_PARAMETER	= 17,
	
	MC_DRV_ERR_KERNEL_MODULE	= 18,
	
	MC_DRV_ERR_BULK_MAPPING		= 19,
	
	MC_DRV_ERR_BULK_UNMAPPING	= 20,
	
	MC_DRV_INFO_NOTIFICATION	= 21,
	
	MC_DRV_ERR_NQ_FAILED		= 22
};

enum mc_driver_ctrl {
	
	MC_CTRL_GET_VERSION		= 1
};

struct mc_session_handle {
	uint32_t session_id;		
	uint32_t device_id;		
};

struct mc_bulk_map {
	void *secure_virt_addr;
	uint32_t secure_virt_len;	
};

#define MC_DEVICE_ID_DEFAULT	0
#define MC_INFINITE_TIMEOUT	((int32_t)(-1))
#define MC_NO_TIMEOUT		0
#define MC_MAX_TCI_LEN		0x100000

__MC_CLIENT_LIB_API enum mc_result mc_open_device(uint32_t device_id);

__MC_CLIENT_LIB_API enum mc_result mc_close_device(uint32_t device_id);

__MC_CLIENT_LIB_API enum mc_result mc_open_session(
	struct mc_session_handle *session, const struct mc_uuid_t *uuid,
	uint8_t *tci, uint32_t tci_len);

__MC_CLIENT_LIB_API enum mc_result mc_close_session(
	struct mc_session_handle *session);

__MC_CLIENT_LIB_API enum mc_result mc_notify(struct mc_session_handle *session);

__MC_CLIENT_LIB_API enum mc_result mc_wait_notification(
	struct mc_session_handle *session, int32_t timeout);

__MC_CLIENT_LIB_API enum mc_result mc_malloc_wsm(
	uint32_t  device_id,
	uint32_t  align,
	uint32_t  len,
	uint8_t   **wsm,
	uint32_t  wsm_flags
);

__MC_CLIENT_LIB_API enum mc_result mc_free_wsm(uint32_t device_id,
					       uint8_t *wsm);

__MC_CLIENT_LIB_API enum mc_result mc_map(
	struct mc_session_handle *session, void	*buf, uint32_t len,
	struct mc_bulk_map *map_info);

__MC_CLIENT_LIB_API enum mc_result mc_unmap(
	struct mc_session_handle *session, void *buf,
	struct mc_bulk_map *map_info);

__MC_CLIENT_LIB_API enum mc_result mc_driver_ctrl(
	enum mc_driver_ctrl param, uint8_t *data, uint32_t len);

__MC_CLIENT_LIB_API enum mc_result mc_manage(
	uint32_t device_id, uint8_t *data, uint32_t len);

__MC_CLIENT_LIB_API enum mc_result mc_get_session_error_code(
	struct mc_session_handle *session, int32_t *last_error);

#endif 
