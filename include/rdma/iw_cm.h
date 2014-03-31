/*
 * Copyright (c) 2005 Network Appliance, Inc. All rights reserved.
 * Copyright (c) 2005 Open Grid Computing, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef IW_CM_H
#define IW_CM_H

#include <linux/in.h>
#include <rdma/ib_cm.h>

struct iw_cm_id;

enum iw_cm_event_type {
	IW_CM_EVENT_CONNECT_REQUEST = 1, 
	IW_CM_EVENT_CONNECT_REPLY,	 
	IW_CM_EVENT_ESTABLISHED,	 
	IW_CM_EVENT_DISCONNECT,		 
	IW_CM_EVENT_CLOSE		 
};

struct iw_cm_event {
	enum iw_cm_event_type event;
	int			 status;
	struct sockaddr_in local_addr;
	struct sockaddr_in remote_addr;
	void *private_data;
	void *provider_data;
	u8 private_data_len;
	u8 ord;
	u8 ird;
};

typedef int (*iw_cm_handler)(struct iw_cm_id *cm_id,
			     struct iw_cm_event *event);

typedef int (*iw_event_handler)(struct iw_cm_id *cm_id,
				 struct iw_cm_event *event);

struct iw_cm_id {
	iw_cm_handler		cm_handler;      
	void		        *context;	 
	struct ib_device	*device;
	struct sockaddr_in      local_addr;
	struct sockaddr_in	remote_addr;
	void			*provider_data;	 
	iw_event_handler        event_handler;   
	
	void (*add_ref)(struct iw_cm_id *);
	void (*rem_ref)(struct iw_cm_id *);
};

struct iw_cm_conn_param {
	const void *private_data;
	u16 private_data_len;
	u32 ord;
	u32 ird;
	u32 qpn;
};

struct iw_cm_verbs {
	void		(*add_ref)(struct ib_qp *qp);

	void		(*rem_ref)(struct ib_qp *qp);

	struct ib_qp *	(*get_qp)(struct ib_device *device,
				  int qpn);

	int		(*connect)(struct iw_cm_id *cm_id,
				   struct iw_cm_conn_param *conn_param);

	int		(*accept)(struct iw_cm_id *cm_id,
				  struct iw_cm_conn_param *conn_param);

	int		(*reject)(struct iw_cm_id *cm_id,
				  const void *pdata, u8 pdata_len);

	int		(*create_listen)(struct iw_cm_id *cm_id,
					 int backlog);

	int		(*destroy_listen)(struct iw_cm_id *cm_id);
};

struct iw_cm_id *iw_create_cm_id(struct ib_device *device,
				 iw_cm_handler cm_handler, void *context);

void iw_destroy_cm_id(struct iw_cm_id *cm_id);

void iw_cm_unbind_qp(struct iw_cm_id *cm_id, struct ib_qp *qp);

struct ib_qp *iw_cm_get_qp(struct ib_device *device, int qpn);

int iw_cm_listen(struct iw_cm_id *cm_id, int backlog);

int iw_cm_accept(struct iw_cm_id *cm_id, struct iw_cm_conn_param *iw_param);

int iw_cm_reject(struct iw_cm_id *cm_id, const void *private_data,
		 u8 private_data_len);

int iw_cm_connect(struct iw_cm_id *cm_id, struct iw_cm_conn_param *iw_param);

int iw_cm_disconnect(struct iw_cm_id *cm_id, int abrupt);

int iw_cm_init_qp_attr(struct iw_cm_id *cm_id, struct ib_qp_attr *qp_attr,
		       int *qp_attr_mask);

#endif 
