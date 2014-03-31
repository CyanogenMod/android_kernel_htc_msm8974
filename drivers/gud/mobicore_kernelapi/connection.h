/*
 * Connection data.
 *
 * <-- Copyright Giesecke & Devrient GmbH 2009 - 2012 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _MC_KAPI_CONNECTION_H_
#define _MC_KAPI_CONNECTION_H_

#include <linux/semaphore.h>
#include <linux/mutex.h>

#include <stddef.h>
#include <stdbool.h>

struct connection {
	
	struct sock		*socket_descriptor;
	
	uint32_t		sequence_magic;

	struct nlmsghdr		*data_msg;
	
	uint32_t		data_len;
	
	void			*data_start;
	struct sk_buff		*skb;

	
	struct mutex		data_lock;
	
	struct semaphore	data_available_sem;

	
	pid_t			self_pid;
	
	pid_t			peer_pid;

	
	struct list_head	list;
};

struct connection *connection_new(void);
struct connection *connection_create(int socket_descriptor, pid_t dest);
void connection_cleanup(struct connection *conn);
bool connection_connect(struct connection *conn, pid_t dest);
size_t connection_read_datablock(struct connection *conn, void *buffer,
					uint32_t len);
size_t connection_read_data(struct connection *conn, void *buffer,
				   uint32_t len, int32_t timeout);
size_t connection_write_data(struct connection *conn, void *buffer,
				    uint32_t len);
int connection_process(struct connection *conn, struct sk_buff *skb);

#endif 
