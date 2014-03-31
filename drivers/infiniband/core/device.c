/*
 * Copyright (c) 2004 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005 Sun Microsystems, Inc. All rights reserved.
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

#include <linux/module.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <rdma/rdma_netlink.h>

#include "core_priv.h"

MODULE_AUTHOR("Roland Dreier");
MODULE_DESCRIPTION("core kernel InfiniBand API");
MODULE_LICENSE("Dual BSD/GPL");

struct ib_client_data {
	struct list_head  list;
	struct ib_client *client;
	void *            data;
};

struct workqueue_struct *ib_wq;
EXPORT_SYMBOL_GPL(ib_wq);

static LIST_HEAD(device_list);
static LIST_HEAD(client_list);

static DEFINE_MUTEX(device_mutex);

static int ib_device_check_mandatory(struct ib_device *device)
{
#define IB_MANDATORY_FUNC(x) { offsetof(struct ib_device, x), #x }
	static const struct {
		size_t offset;
		char  *name;
	} mandatory_table[] = {
		IB_MANDATORY_FUNC(query_device),
		IB_MANDATORY_FUNC(query_port),
		IB_MANDATORY_FUNC(query_pkey),
		IB_MANDATORY_FUNC(query_gid),
		IB_MANDATORY_FUNC(alloc_pd),
		IB_MANDATORY_FUNC(dealloc_pd),
		IB_MANDATORY_FUNC(create_ah),
		IB_MANDATORY_FUNC(destroy_ah),
		IB_MANDATORY_FUNC(create_qp),
		IB_MANDATORY_FUNC(modify_qp),
		IB_MANDATORY_FUNC(destroy_qp),
		IB_MANDATORY_FUNC(post_send),
		IB_MANDATORY_FUNC(post_recv),
		IB_MANDATORY_FUNC(create_cq),
		IB_MANDATORY_FUNC(destroy_cq),
		IB_MANDATORY_FUNC(poll_cq),
		IB_MANDATORY_FUNC(req_notify_cq),
		IB_MANDATORY_FUNC(get_dma_mr),
		IB_MANDATORY_FUNC(dereg_mr)
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(mandatory_table); ++i) {
		if (!*(void **) ((void *) device + mandatory_table[i].offset)) {
			printk(KERN_WARNING "Device %s is missing mandatory function %s\n",
			       device->name, mandatory_table[i].name);
			return -EINVAL;
		}
	}

	return 0;
}

static struct ib_device *__ib_device_get_by_name(const char *name)
{
	struct ib_device *device;

	list_for_each_entry(device, &device_list, core_list)
		if (!strncmp(name, device->name, IB_DEVICE_NAME_MAX))
			return device;

	return NULL;
}


static int alloc_name(char *name)
{
	unsigned long *inuse;
	char buf[IB_DEVICE_NAME_MAX];
	struct ib_device *device;
	int i;

	inuse = (unsigned long *) get_zeroed_page(GFP_KERNEL);
	if (!inuse)
		return -ENOMEM;

	list_for_each_entry(device, &device_list, core_list) {
		if (!sscanf(device->name, name, &i))
			continue;
		if (i < 0 || i >= PAGE_SIZE * 8)
			continue;
		snprintf(buf, sizeof buf, name, i);
		if (!strncmp(buf, device->name, IB_DEVICE_NAME_MAX))
			set_bit(i, inuse);
	}

	i = find_first_zero_bit(inuse, PAGE_SIZE * 8);
	free_page((unsigned long) inuse);
	snprintf(buf, sizeof buf, name, i);

	if (__ib_device_get_by_name(buf))
		return -ENFILE;

	strlcpy(name, buf, IB_DEVICE_NAME_MAX);
	return 0;
}

static int start_port(struct ib_device *device)
{
	return (device->node_type == RDMA_NODE_IB_SWITCH) ? 0 : 1;
}


static int end_port(struct ib_device *device)
{
	return (device->node_type == RDMA_NODE_IB_SWITCH) ?
		0 : device->phys_port_cnt;
}

struct ib_device *ib_alloc_device(size_t size)
{
	BUG_ON(size < sizeof (struct ib_device));

	return kzalloc(size, GFP_KERNEL);
}
EXPORT_SYMBOL(ib_alloc_device);

void ib_dealloc_device(struct ib_device *device)
{
	if (device->reg_state == IB_DEV_UNINITIALIZED) {
		kfree(device);
		return;
	}

	BUG_ON(device->reg_state != IB_DEV_UNREGISTERED);

	kobject_put(&device->dev.kobj);
}
EXPORT_SYMBOL(ib_dealloc_device);

static int add_client_context(struct ib_device *device, struct ib_client *client)
{
	struct ib_client_data *context;
	unsigned long flags;

	context = kmalloc(sizeof *context, GFP_KERNEL);
	if (!context) {
		printk(KERN_WARNING "Couldn't allocate client context for %s/%s\n",
		       device->name, client->name);
		return -ENOMEM;
	}

	context->client = client;
	context->data   = NULL;

	spin_lock_irqsave(&device->client_data_lock, flags);
	list_add(&context->list, &device->client_data_list);
	spin_unlock_irqrestore(&device->client_data_lock, flags);

	return 0;
}

static int read_port_table_lengths(struct ib_device *device)
{
	struct ib_port_attr *tprops = NULL;
	int num_ports, ret = -ENOMEM;
	u8 port_index;

	tprops = kmalloc(sizeof *tprops, GFP_KERNEL);
	if (!tprops)
		goto out;

	num_ports = end_port(device) - start_port(device) + 1;

	device->pkey_tbl_len = kmalloc(sizeof *device->pkey_tbl_len * num_ports,
				       GFP_KERNEL);
	device->gid_tbl_len = kmalloc(sizeof *device->gid_tbl_len * num_ports,
				      GFP_KERNEL);
	if (!device->pkey_tbl_len || !device->gid_tbl_len)
		goto err;

	for (port_index = 0; port_index < num_ports; ++port_index) {
		ret = ib_query_port(device, port_index + start_port(device),
					tprops);
		if (ret)
			goto err;
		device->pkey_tbl_len[port_index] = tprops->pkey_tbl_len;
		device->gid_tbl_len[port_index]  = tprops->gid_tbl_len;
	}

	ret = 0;
	goto out;

err:
	kfree(device->gid_tbl_len);
	kfree(device->pkey_tbl_len);
out:
	kfree(tprops);
	return ret;
}

int ib_register_device(struct ib_device *device,
		       int (*port_callback)(struct ib_device *,
					    u8, struct kobject *))
{
	int ret;

	mutex_lock(&device_mutex);

	if (strchr(device->name, '%')) {
		ret = alloc_name(device->name);
		if (ret)
			goto out;
	}

	if (ib_device_check_mandatory(device)) {
		ret = -EINVAL;
		goto out;
	}

	INIT_LIST_HEAD(&device->event_handler_list);
	INIT_LIST_HEAD(&device->client_data_list);
	spin_lock_init(&device->event_handler_lock);
	spin_lock_init(&device->client_data_lock);

	ret = read_port_table_lengths(device);
	if (ret) {
		printk(KERN_WARNING "Couldn't create table lengths cache for device %s\n",
		       device->name);
		goto out;
	}

	ret = ib_device_register_sysfs(device, port_callback);
	if (ret) {
		printk(KERN_WARNING "Couldn't register device %s with driver model\n",
		       device->name);
		kfree(device->gid_tbl_len);
		kfree(device->pkey_tbl_len);
		goto out;
	}

	list_add_tail(&device->core_list, &device_list);

	device->reg_state = IB_DEV_REGISTERED;

	{
		struct ib_client *client;

		list_for_each_entry(client, &client_list, list)
			if (client->add && !add_client_context(device, client))
				client->add(device);
	}

 out:
	mutex_unlock(&device_mutex);
	return ret;
}
EXPORT_SYMBOL(ib_register_device);

void ib_unregister_device(struct ib_device *device)
{
	struct ib_client *client;
	struct ib_client_data *context, *tmp;
	unsigned long flags;

	mutex_lock(&device_mutex);

	list_for_each_entry_reverse(client, &client_list, list)
		if (client->remove)
			client->remove(device);

	list_del(&device->core_list);

	kfree(device->gid_tbl_len);
	kfree(device->pkey_tbl_len);

	mutex_unlock(&device_mutex);

	ib_device_unregister_sysfs(device);

	spin_lock_irqsave(&device->client_data_lock, flags);
	list_for_each_entry_safe(context, tmp, &device->client_data_list, list)
		kfree(context);
	spin_unlock_irqrestore(&device->client_data_lock, flags);

	device->reg_state = IB_DEV_UNREGISTERED;
}
EXPORT_SYMBOL(ib_unregister_device);

int ib_register_client(struct ib_client *client)
{
	struct ib_device *device;

	mutex_lock(&device_mutex);

	list_add_tail(&client->list, &client_list);
	list_for_each_entry(device, &device_list, core_list)
		if (client->add && !add_client_context(device, client))
			client->add(device);

	mutex_unlock(&device_mutex);

	return 0;
}
EXPORT_SYMBOL(ib_register_client);

void ib_unregister_client(struct ib_client *client)
{
	struct ib_client_data *context, *tmp;
	struct ib_device *device;
	unsigned long flags;

	mutex_lock(&device_mutex);

	list_for_each_entry(device, &device_list, core_list) {
		if (client->remove)
			client->remove(device);

		spin_lock_irqsave(&device->client_data_lock, flags);
		list_for_each_entry_safe(context, tmp, &device->client_data_list, list)
			if (context->client == client) {
				list_del(&context->list);
				kfree(context);
			}
		spin_unlock_irqrestore(&device->client_data_lock, flags);
	}
	list_del(&client->list);

	mutex_unlock(&device_mutex);
}
EXPORT_SYMBOL(ib_unregister_client);

void *ib_get_client_data(struct ib_device *device, struct ib_client *client)
{
	struct ib_client_data *context;
	void *ret = NULL;
	unsigned long flags;

	spin_lock_irqsave(&device->client_data_lock, flags);
	list_for_each_entry(context, &device->client_data_list, list)
		if (context->client == client) {
			ret = context->data;
			break;
		}
	spin_unlock_irqrestore(&device->client_data_lock, flags);

	return ret;
}
EXPORT_SYMBOL(ib_get_client_data);

void ib_set_client_data(struct ib_device *device, struct ib_client *client,
			void *data)
{
	struct ib_client_data *context;
	unsigned long flags;

	spin_lock_irqsave(&device->client_data_lock, flags);
	list_for_each_entry(context, &device->client_data_list, list)
		if (context->client == client) {
			context->data = data;
			goto out;
		}

	printk(KERN_WARNING "No client context found for %s/%s\n",
	       device->name, client->name);

out:
	spin_unlock_irqrestore(&device->client_data_lock, flags);
}
EXPORT_SYMBOL(ib_set_client_data);

int ib_register_event_handler  (struct ib_event_handler *event_handler)
{
	unsigned long flags;

	spin_lock_irqsave(&event_handler->device->event_handler_lock, flags);
	list_add_tail(&event_handler->list,
		      &event_handler->device->event_handler_list);
	spin_unlock_irqrestore(&event_handler->device->event_handler_lock, flags);

	return 0;
}
EXPORT_SYMBOL(ib_register_event_handler);

int ib_unregister_event_handler(struct ib_event_handler *event_handler)
{
	unsigned long flags;

	spin_lock_irqsave(&event_handler->device->event_handler_lock, flags);
	list_del(&event_handler->list);
	spin_unlock_irqrestore(&event_handler->device->event_handler_lock, flags);

	return 0;
}
EXPORT_SYMBOL(ib_unregister_event_handler);

void ib_dispatch_event(struct ib_event *event)
{
	unsigned long flags;
	struct ib_event_handler *handler;

	spin_lock_irqsave(&event->device->event_handler_lock, flags);

	list_for_each_entry(handler, &event->device->event_handler_list, list)
		handler->handler(handler, event);

	spin_unlock_irqrestore(&event->device->event_handler_lock, flags);
}
EXPORT_SYMBOL(ib_dispatch_event);

int ib_query_device(struct ib_device *device,
		    struct ib_device_attr *device_attr)
{
	return device->query_device(device, device_attr);
}
EXPORT_SYMBOL(ib_query_device);

int ib_query_port(struct ib_device *device,
		  u8 port_num,
		  struct ib_port_attr *port_attr)
{
	if (port_num < start_port(device) || port_num > end_port(device))
		return -EINVAL;

	return device->query_port(device, port_num, port_attr);
}
EXPORT_SYMBOL(ib_query_port);

int ib_query_gid(struct ib_device *device,
		 u8 port_num, int index, union ib_gid *gid)
{
	return device->query_gid(device, port_num, index, gid);
}
EXPORT_SYMBOL(ib_query_gid);

int ib_query_pkey(struct ib_device *device,
		  u8 port_num, u16 index, u16 *pkey)
{
	return device->query_pkey(device, port_num, index, pkey);
}
EXPORT_SYMBOL(ib_query_pkey);

int ib_modify_device(struct ib_device *device,
		     int device_modify_mask,
		     struct ib_device_modify *device_modify)
{
	if (!device->modify_device)
		return -ENOSYS;

	return device->modify_device(device, device_modify_mask,
				     device_modify);
}
EXPORT_SYMBOL(ib_modify_device);

int ib_modify_port(struct ib_device *device,
		   u8 port_num, int port_modify_mask,
		   struct ib_port_modify *port_modify)
{
	if (!device->modify_port)
		return -ENOSYS;

	if (port_num < start_port(device) || port_num > end_port(device))
		return -EINVAL;

	return device->modify_port(device, port_num, port_modify_mask,
				   port_modify);
}
EXPORT_SYMBOL(ib_modify_port);

int ib_find_gid(struct ib_device *device, union ib_gid *gid,
		u8 *port_num, u16 *index)
{
	union ib_gid tmp_gid;
	int ret, port, i;

	for (port = start_port(device); port <= end_port(device); ++port) {
		for (i = 0; i < device->gid_tbl_len[port - start_port(device)]; ++i) {
			ret = ib_query_gid(device, port, i, &tmp_gid);
			if (ret)
				return ret;
			if (!memcmp(&tmp_gid, gid, sizeof *gid)) {
				*port_num = port;
				if (index)
					*index = i;
				return 0;
			}
		}
	}

	return -ENOENT;
}
EXPORT_SYMBOL(ib_find_gid);

int ib_find_pkey(struct ib_device *device,
		 u8 port_num, u16 pkey, u16 *index)
{
	int ret, i;
	u16 tmp_pkey;

	for (i = 0; i < device->pkey_tbl_len[port_num - start_port(device)]; ++i) {
		ret = ib_query_pkey(device, port_num, i, &tmp_pkey);
		if (ret)
			return ret;

		if ((pkey & 0x7fff) == (tmp_pkey & 0x7fff)) {
			*index = i;
			return 0;
		}
	}

	return -ENOENT;
}
EXPORT_SYMBOL(ib_find_pkey);

static int __init ib_core_init(void)
{
	int ret;

	ib_wq = alloc_workqueue("infiniband", 0, 0);
	if (!ib_wq)
		return -ENOMEM;

	ret = ib_sysfs_setup();
	if (ret) {
		printk(KERN_WARNING "Couldn't create InfiniBand device class\n");
		goto err;
	}

	ret = ibnl_init();
	if (ret) {
		printk(KERN_WARNING "Couldn't init IB netlink interface\n");
		goto err_sysfs;
	}

	ret = ib_cache_setup();
	if (ret) {
		printk(KERN_WARNING "Couldn't set up InfiniBand P_Key/GID cache\n");
		goto err_nl;
	}

	return 0;

err_nl:
	ibnl_cleanup();

err_sysfs:
	ib_sysfs_cleanup();

err:
	destroy_workqueue(ib_wq);
	return ret;
}

static void __exit ib_core_cleanup(void)
{
	ib_cache_cleanup();
	ibnl_cleanup();
	ib_sysfs_cleanup();
	
	destroy_workqueue(ib_wq);
}

module_init(ib_core_init);
module_exit(ib_core_cleanup);
