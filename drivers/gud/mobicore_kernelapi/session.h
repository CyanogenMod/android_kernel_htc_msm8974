/*
 * <-- Copyright Giesecke & Devrient GmbH 2009 - 2012 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _MC_KAPI_SESSION_H_
#define _MC_KAPI_SESSION_H_

#include "common.h"

#include <linux/list.h>
#include "connection.h"


struct bulk_buffer_descriptor {
	void		*virt_addr;	
	uint32_t	len;		
	uint32_t	handle;

	
	void		*phys_addr_wsm_l2;

	
	struct list_head list;
};

struct bulk_buffer_descriptor *bulk_buffer_descriptor_create(
	void		*virt_addr,
	uint32_t	len,
	uint32_t	handle,
	void		*phys_addr_wsm_l2
);

enum session_state {
	SESSION_STATE_INITIAL,
	SESSION_STATE_OPEN,
	SESSION_STATE_TRUSTLET_DEAD
};

#define SESSION_ERR_NO	0		

struct session_information {
	enum session_state state;	
	int32_t		last_error;	
};


struct session {
	struct mc_instance		*instance;

	
	struct list_head		bulk_buffer_descriptors;

	
	struct session_information	session_info;

	uint32_t			session_id;
	struct connection		*notification_connection;

	
	struct list_head		list;
};

struct session *session_create(
	uint32_t		session_id,
	void			*instance,
	struct connection	*connection
);

void session_cleanup(struct session *session);

struct bulk_buffer_descriptor *session_add_bulk_buf(
	struct session *session, void *buf, uint32_t len);

bool session_remove_bulk_buf(struct session *session, void *buf);


uint32_t session_find_bulk_buf(struct session *session, void *virt_addr);

void session_set_error_info(struct session *session, int32_t err);

int32_t session_get_last_err(struct session *session);

#endif 
