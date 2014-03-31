/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2004-2008 Silicon Graphics, Inc.  All Rights Reserved.
 */


#include <linux/module.h>
#include <linux/device.h>
#include "xp.h"


struct device_driver xp_dbg_name = {
	.name = "xp"
};

struct device xp_dbg_subname = {
	.init_name = "",		
	.driver = &xp_dbg_name
};

struct device *xp = &xp_dbg_subname;

short xp_max_npartitions;
EXPORT_SYMBOL_GPL(xp_max_npartitions);

short xp_partition_id;
EXPORT_SYMBOL_GPL(xp_partition_id);

u8 xp_region_size;
EXPORT_SYMBOL_GPL(xp_region_size);

unsigned long (*xp_pa) (void *addr);
EXPORT_SYMBOL_GPL(xp_pa);

unsigned long (*xp_socket_pa) (unsigned long gpa);
EXPORT_SYMBOL_GPL(xp_socket_pa);

enum xp_retval (*xp_remote_memcpy) (unsigned long dst_gpa,
				    const unsigned long src_gpa, size_t len);
EXPORT_SYMBOL_GPL(xp_remote_memcpy);

int (*xp_cpu_to_nasid) (int cpuid);
EXPORT_SYMBOL_GPL(xp_cpu_to_nasid);

enum xp_retval (*xp_expand_memprotect) (unsigned long phys_addr,
					unsigned long size);
EXPORT_SYMBOL_GPL(xp_expand_memprotect);
enum xp_retval (*xp_restrict_memprotect) (unsigned long phys_addr,
					  unsigned long size);
EXPORT_SYMBOL_GPL(xp_restrict_memprotect);

struct xpc_registration xpc_registrations[XPC_MAX_NCHANNELS];
EXPORT_SYMBOL_GPL(xpc_registrations);

static enum xp_retval
xpc_notloaded(void)
{
	return xpNotLoaded;
}

struct xpc_interface xpc_interface = {
	(void (*)(int))xpc_notloaded,
	(void (*)(int))xpc_notloaded,
	(enum xp_retval(*)(short, int, u32, void *, u16))xpc_notloaded,
	(enum xp_retval(*)(short, int, u32, void *, u16, xpc_notify_func,
			   void *))xpc_notloaded,
	(void (*)(short, int, void *))xpc_notloaded,
	(enum xp_retval(*)(short, void *))xpc_notloaded
};
EXPORT_SYMBOL_GPL(xpc_interface);

void
xpc_set_interface(void (*connect) (int),
		  void (*disconnect) (int),
		  enum xp_retval (*send) (short, int, u32, void *, u16),
		  enum xp_retval (*send_notify) (short, int, u32, void *, u16,
						  xpc_notify_func, void *),
		  void (*received) (short, int, void *),
		  enum xp_retval (*partid_to_nasids) (short, void *))
{
	xpc_interface.connect = connect;
	xpc_interface.disconnect = disconnect;
	xpc_interface.send = send;
	xpc_interface.send_notify = send_notify;
	xpc_interface.received = received;
	xpc_interface.partid_to_nasids = partid_to_nasids;
}
EXPORT_SYMBOL_GPL(xpc_set_interface);

void
xpc_clear_interface(void)
{
	xpc_interface.connect = (void (*)(int))xpc_notloaded;
	xpc_interface.disconnect = (void (*)(int))xpc_notloaded;
	xpc_interface.send = (enum xp_retval(*)(short, int, u32, void *, u16))
	    xpc_notloaded;
	xpc_interface.send_notify = (enum xp_retval(*)(short, int, u32, void *,
						       u16, xpc_notify_func,
						       void *))xpc_notloaded;
	xpc_interface.received = (void (*)(short, int, void *))
	    xpc_notloaded;
	xpc_interface.partid_to_nasids = (enum xp_retval(*)(short, void *))
	    xpc_notloaded;
}
EXPORT_SYMBOL_GPL(xpc_clear_interface);

enum xp_retval
xpc_connect(int ch_number, xpc_channel_func func, void *key, u16 payload_size,
	    u16 nentries, u32 assigned_limit, u32 idle_limit)
{
	struct xpc_registration *registration;

	DBUG_ON(ch_number < 0 || ch_number >= XPC_MAX_NCHANNELS);
	DBUG_ON(payload_size == 0 || nentries == 0);
	DBUG_ON(func == NULL);
	DBUG_ON(assigned_limit == 0 || idle_limit > assigned_limit);

	if (XPC_MSG_SIZE(payload_size) > XPC_MSG_MAX_SIZE)
		return xpPayloadTooBig;

	registration = &xpc_registrations[ch_number];

	if (mutex_lock_interruptible(&registration->mutex) != 0)
		return xpInterrupted;

	
	if (registration->func != NULL) {
		mutex_unlock(&registration->mutex);
		return xpAlreadyRegistered;
	}

	
	registration->entry_size = XPC_MSG_SIZE(payload_size);
	registration->nentries = nentries;
	registration->assigned_limit = assigned_limit;
	registration->idle_limit = idle_limit;
	registration->key = key;
	registration->func = func;

	mutex_unlock(&registration->mutex);

	xpc_interface.connect(ch_number);

	return xpSuccess;
}
EXPORT_SYMBOL_GPL(xpc_connect);

void
xpc_disconnect(int ch_number)
{
	struct xpc_registration *registration;

	DBUG_ON(ch_number < 0 || ch_number >= XPC_MAX_NCHANNELS);

	registration = &xpc_registrations[ch_number];

	mutex_lock(&registration->mutex);

	
	if (registration->func == NULL) {
		mutex_unlock(&registration->mutex);
		return;
	}

	
	registration->func = NULL;
	registration->key = NULL;
	registration->nentries = 0;
	registration->entry_size = 0;
	registration->assigned_limit = 0;
	registration->idle_limit = 0;

	xpc_interface.disconnect(ch_number);

	mutex_unlock(&registration->mutex);

	return;
}
EXPORT_SYMBOL_GPL(xpc_disconnect);

int __init
xp_init(void)
{
	enum xp_retval ret;
	int ch_number;

	
	for (ch_number = 0; ch_number < XPC_MAX_NCHANNELS; ch_number++)
		mutex_init(&xpc_registrations[ch_number].mutex);

	if (is_shub())
		ret = xp_init_sn2();
	else if (is_uv())
		ret = xp_init_uv();
	else
		ret = 0;

	if (ret != xpSuccess)
		return ret;

	return 0;
}

module_init(xp_init);

void __exit
xp_exit(void)
{
	if (is_shub())
		xp_exit_sn2();
	else if (is_uv())
		xp_exit_uv();
}

module_exit(xp_exit);

MODULE_AUTHOR("Silicon Graphics, Inc.");
MODULE_DESCRIPTION("Cross Partition (XP) base");
MODULE_LICENSE("GPL");
