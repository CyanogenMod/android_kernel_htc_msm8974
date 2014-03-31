/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/fs.h>
#include <linux/sched.h>
#include "ipa_i.h"

struct ipa_intf {
	char name[IPA_RESOURCE_NAME_MAX];
	struct list_head link;
	u32 num_tx_props;
	u32 num_rx_props;
	struct ipa_ioc_tx_intf_prop *tx;
	struct ipa_ioc_rx_intf_prop *rx;
};

struct ipa_push_msg {
	struct ipa_msg_meta meta;
	ipa_msg_free_fn callback;
	void *buff;
	struct list_head link;
};

struct ipa_pull_msg {
	struct ipa_msg_meta meta;
	ipa_msg_pull_fn callback;
	struct list_head link;
};

int ipa_register_intf(const char *name, const struct ipa_tx_intf *tx,
		       const struct ipa_rx_intf *rx)
{
	struct ipa_intf *intf;
	u32 len;

	if (name == NULL || (tx == NULL && rx == NULL)) {
		IPAERR("invalid params name=%p tx=%p rx=%p\n", name, tx, rx);
		return -EINVAL;
	}

	len = sizeof(struct ipa_intf);
	intf = kzalloc(len, GFP_KERNEL);
	if (intf == NULL) {
		IPAERR("fail to alloc 0x%x bytes\n", len);
		return -ENOMEM;
	}

	strlcpy(intf->name, name, IPA_RESOURCE_NAME_MAX);

	if (tx) {
		intf->num_tx_props = tx->num_props;
		len = tx->num_props * sizeof(struct ipa_ioc_tx_intf_prop);
		intf->tx = kzalloc(len, GFP_KERNEL);
		if (intf->tx == NULL) {
			IPAERR("fail to alloc 0x%x bytes\n", len);
			kfree(intf);
			return -ENOMEM;
		}
		memcpy(intf->tx, tx->prop, len);
	}

	if (rx) {
		intf->num_rx_props = rx->num_props;
		len = rx->num_props * sizeof(struct ipa_ioc_rx_intf_prop);
		intf->rx = kzalloc(len, GFP_KERNEL);
		if (intf->rx == NULL) {
			IPAERR("fail to alloc 0x%x bytes\n", len);
			kfree(intf->tx);
			kfree(intf);
			return -ENOMEM;
		}
		memcpy(intf->rx, rx->prop, len);
	}

	mutex_lock(&ipa_ctx->lock);
	list_add_tail(&intf->link, &ipa_ctx->intf_list);
	mutex_unlock(&ipa_ctx->lock);

	return 0;
}
EXPORT_SYMBOL(ipa_register_intf);

int ipa_deregister_intf(const char *name)
{
	struct ipa_intf *entry;
	struct ipa_intf *next;
	int result = -EINVAL;

	if (name == NULL) {
		IPAERR("invalid param name=%p\n", name);
		return result;
	}

	mutex_lock(&ipa_ctx->lock);
	list_for_each_entry_safe(entry, next, &ipa_ctx->intf_list, link) {
		if (!strncmp(entry->name, name, IPA_RESOURCE_NAME_MAX)) {
			list_del(&entry->link);
			kfree(entry->rx);
			kfree(entry->tx);
			kfree(entry);
			result = 0;
			break;
		}
	}
	mutex_unlock(&ipa_ctx->lock);
	return result;
}
EXPORT_SYMBOL(ipa_deregister_intf);

int ipa_query_intf(struct ipa_ioc_query_intf *lookup)
{
	struct ipa_intf *entry;
	int result = -EINVAL;

	if (lookup == NULL) {
		IPAERR("invalid param lookup=%p\n", lookup);
		return result;
	}

	mutex_lock(&ipa_ctx->lock);
	list_for_each_entry(entry, &ipa_ctx->intf_list, link) {
		if (!strncmp(entry->name, lookup->name,
					IPA_RESOURCE_NAME_MAX)) {
			lookup->num_tx_props = entry->num_tx_props;
			lookup->num_rx_props = entry->num_rx_props;
			result = 0;
			break;
		}
	}
	mutex_unlock(&ipa_ctx->lock);
	return result;
}

int ipa_query_intf_tx_props(struct ipa_ioc_query_intf_tx_props *tx)
{
	struct ipa_intf *entry;
	int result = -EINVAL;

	if (tx == NULL) {
		IPAERR("invalid param tx=%p\n", tx);
		return result;
	}

	mutex_lock(&ipa_ctx->lock);
	list_for_each_entry(entry, &ipa_ctx->intf_list, link) {
		if (!strncmp(entry->name, tx->name, IPA_RESOURCE_NAME_MAX)) {
			memcpy(tx->tx, entry->tx, entry->num_tx_props *
			       sizeof(struct ipa_ioc_tx_intf_prop));
			result = 0;
			break;
		}
	}
	mutex_unlock(&ipa_ctx->lock);
	return result;
}

int ipa_query_intf_rx_props(struct ipa_ioc_query_intf_rx_props *rx)
{
	struct ipa_intf *entry;
	int result = -EINVAL;

	if (rx == NULL) {
		IPAERR("invalid param rx=%p\n", rx);
		return result;
	}

	mutex_lock(&ipa_ctx->lock);
	list_for_each_entry(entry, &ipa_ctx->intf_list, link) {
		if (!strncmp(entry->name, rx->name, IPA_RESOURCE_NAME_MAX)) {
			memcpy(rx->rx, entry->rx, entry->num_rx_props *
					sizeof(struct ipa_ioc_rx_intf_prop));
			result = 0;
			break;
		}
	}
	mutex_unlock(&ipa_ctx->lock);
	return result;
}

int ipa_send_msg(struct ipa_msg_meta *meta, void *buff,
		  ipa_msg_free_fn callback)
{
	struct ipa_push_msg *msg;

	if (meta == NULL || (buff == NULL && callback != NULL) ||
	    (buff != NULL && callback == NULL)) {
		IPAERR("invalid param meta=%p buff=%p, callback=%p\n",
		       meta, buff, callback);
		return -EINVAL;
	}

	if (meta->msg_type >= IPA_EVENT_MAX) {
		IPAERR("unsupported message type %d\n", meta->msg_type);
		return -EINVAL;
	}

	msg = kzalloc(sizeof(struct ipa_push_msg), GFP_KERNEL);
	if (msg == NULL) {
		IPAERR("fail to alloc ipa_msg container\n");
		return -ENOMEM;
	}

	msg->meta = *meta;
	msg->buff = buff;
	msg->callback = callback;

	mutex_lock(&ipa_ctx->msg_lock);
	list_add_tail(&msg->link, &ipa_ctx->msg_list);
	mutex_unlock(&ipa_ctx->msg_lock);
	IPA_STATS_INC_CNT(ipa_ctx->stats.msg_w[meta->msg_type]);

	wake_up(&ipa_ctx->msg_waitq);

	return 0;
}
EXPORT_SYMBOL(ipa_send_msg);

int ipa_register_pull_msg(struct ipa_msg_meta *meta, ipa_msg_pull_fn callback)
{
	struct ipa_pull_msg *msg;

	if (meta == NULL || callback == NULL) {
		IPAERR("invalid param meta=%p callback=%p\n", meta, callback);
		return -EINVAL;
	}

	msg = kzalloc(sizeof(struct ipa_pull_msg), GFP_KERNEL);
	if (msg == NULL) {
		IPAERR("fail to alloc ipa_msg container\n");
		return -ENOMEM;
	}

	msg->meta = *meta;
	msg->callback = callback;

	mutex_lock(&ipa_ctx->msg_lock);
	list_add_tail(&msg->link, &ipa_ctx->pull_msg_list);
	mutex_unlock(&ipa_ctx->msg_lock);

	return 0;
}
EXPORT_SYMBOL(ipa_register_pull_msg);

int ipa_deregister_pull_msg(struct ipa_msg_meta *meta)
{
	struct ipa_pull_msg *entry;
	struct ipa_pull_msg *next;
	int result = -EINVAL;

	if (meta == NULL) {
		IPAERR("invalid param name=%p\n", meta);
		return result;
	}

	mutex_lock(&ipa_ctx->msg_lock);
	list_for_each_entry_safe(entry, next, &ipa_ctx->pull_msg_list, link) {
		if (entry->meta.msg_len == meta->msg_len &&
		    entry->meta.msg_type == meta->msg_type) {
			list_del(&entry->link);
			kfree(entry);
			result = 0;
			break;
		}
	}
	mutex_unlock(&ipa_ctx->msg_lock);
	return result;
}
EXPORT_SYMBOL(ipa_deregister_pull_msg);

ssize_t ipa_read(struct file *filp, char __user *buf, size_t count,
		  loff_t *f_pos)
{
	char __user *start;
	struct ipa_push_msg *msg = NULL;
	int ret;
	DEFINE_WAIT(wait);
	int locked;

	start = buf;

	while (1) {
		prepare_to_wait(&ipa_ctx->msg_waitq, &wait, TASK_INTERRUPTIBLE);

		mutex_lock(&ipa_ctx->msg_lock);
		locked = 1;
		if (!list_empty(&ipa_ctx->msg_list)) {
			msg = list_first_entry(&ipa_ctx->msg_list,
					struct ipa_push_msg, link);
			list_del(&msg->link);
		}

		IPADBG("msg=%p\n", msg);

		if (msg) {
			locked = 0;
			mutex_unlock(&ipa_ctx->msg_lock);
			if (copy_to_user(buf, &msg->meta,
					  sizeof(struct ipa_msg_meta))) {
				ret = -EFAULT;
				break;
			}
			buf += sizeof(struct ipa_msg_meta);
			count -= sizeof(struct ipa_msg_meta);
			if (msg->buff) {
				if (copy_to_user(buf, msg->buff,
						  msg->meta.msg_len)) {
					ret = -EFAULT;
					break;
				}
				buf += msg->meta.msg_len;
				count -= msg->meta.msg_len;
				msg->callback(msg->buff, msg->meta.msg_len,
					       msg->meta.msg_type);
			}
			IPA_STATS_INC_CNT(
				ipa_ctx->stats.msg_r[msg->meta.msg_type]);
			kfree(msg);
		}

		ret = -EAGAIN;
		if (filp->f_flags & O_NONBLOCK)
			break;

		ret = -EINTR;
		if (signal_pending(current))
			break;

		if (start != buf)
			break;

		locked = 0;
		mutex_unlock(&ipa_ctx->msg_lock);
		schedule();
	}

	finish_wait(&ipa_ctx->msg_waitq, &wait);
	if (start != buf && ret != -EFAULT)
		ret = buf - start;

	if (locked)
		mutex_unlock(&ipa_ctx->msg_lock);

	return ret;
}

int ipa_pull_msg(struct ipa_msg_meta *meta, char *buff, size_t count)
{
	struct ipa_pull_msg *entry;
	int result = -EINVAL;

	if (meta == NULL || buff == NULL || !count) {
		IPAERR("invalid param name=%p buff=%p count=%zu\n",
				meta, buff, count);
		return result;
	}

	mutex_lock(&ipa_ctx->msg_lock);
	list_for_each_entry(entry, &ipa_ctx->pull_msg_list, link) {
		if (entry->meta.msg_len == meta->msg_len &&
		    entry->meta.msg_type == meta->msg_type) {
			result = entry->callback(buff, count, meta->msg_type);
			break;
		}
	}
	mutex_unlock(&ipa_ctx->msg_lock);
	return result;
}
