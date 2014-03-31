/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MSM_IPC_ROUTER_SECURITY_H
#define _MSM_IPC_ROUTER_SECURITY_H

#include <linux/types.h>
#include <linux/socket.h>
#include <linux/errno.h>

#ifdef CONFIG_MSM_IPC_ROUTER_SECURITY
#include <linux/android_aid.h>

int check_permissions(void);

int msm_ipc_config_sec_rules(void *arg);

void *msm_ipc_get_security_rule(uint32_t service_id, uint32_t instance_id);

int msm_ipc_check_send_permissions(void *data);

int msm_ipc_router_security_init(void);

void wait_for_irsc_completion(void);

void signal_irsc_completion(void);

#else

static inline int check_permissions(void)
{
	return 1;
}

static inline int msm_ipc_config_sec_rules(void *arg)
{
	return -ENODEV;
}

static inline void *msm_ipc_get_security_rule(uint32_t service_id,
					      uint32_t instance_id)
{
	return NULL;
}

static inline int msm_ipc_check_send_permissions(void *data)
{
	return 1;
}

static inline int msm_ipc_router_security_init(void)
{
	return 0;
}

static inline void wait_for_irsc_completion(void) { }

static inline void signal_irsc_completion(void) { }

#endif
#endif
