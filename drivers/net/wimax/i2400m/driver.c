/*
 * Intel Wireless WiMAX Connection 2400m
 * Generic probe/disconnect, reset and message passing
 *
 *
 * Copyright (C) 2007-2008 Intel Corporation <linux-wimax@intel.com>
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 *
 * See i2400m.h for driver documentation. This contains helpers for
 * the driver model glue [_setup()/_release()], handling device resets
 * [_dev_reset_handle()], and the backends for the WiMAX stack ops
 * reset [_op_reset()] and message from user [_op_msg_from_user()].
 *
 * ROADMAP:
 *
 * i2400m_op_msg_from_user()
 *   i2400m_msg_to_dev()
 *   wimax_msg_to_user_send()
 *
 * i2400m_op_reset()
 *   i240m->bus_reset()
 *
 * i2400m_dev_reset_handle()
 *   __i2400m_dev_reset_handle()
 *     __i2400m_dev_stop()
 *     __i2400m_dev_start()
 *
 * i2400m_setup()
 *   i2400m->bus_setup()
 *   i2400m_bootrom_init()
 *   register_netdev()
 *   wimax_dev_add()
 *   i2400m_dev_start()
 *     __i2400m_dev_start()
 *       i2400m_dev_bootstrap()
 *       i2400m_tx_setup()
 *       i2400m->bus_dev_start()
 *       i2400m_firmware_check()
 *       i2400m_check_mac_addr()
 *
 * i2400m_release()
 *   i2400m_dev_stop()
 *     __i2400m_dev_stop()
 *       i2400m_dev_shutdown()
 *       i2400m->bus_dev_stop()
 *       i2400m_tx_release()
 *   i2400m->bus_release()
 *   wimax_dev_rm()
 *   unregister_netdev()
 */
#include "i2400m.h"
#include <linux/etherdevice.h>
#include <linux/wimax/i2400m.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/suspend.h>
#include <linux/slab.h>

#define D_SUBMODULE driver
#include "debug-levels.h"


static char i2400m_debug_params[128];
module_param_string(debug, i2400m_debug_params, sizeof(i2400m_debug_params),
		    0644);
MODULE_PARM_DESC(debug,
		 "String of space-separated NAME:VALUE pairs, where NAMEs "
		 "are the different debug submodules and VALUE are the "
		 "initial debug value to set.");

static char i2400m_barkers_params[128];
module_param_string(barkers, i2400m_barkers_params,
		    sizeof(i2400m_barkers_params), 0644);
MODULE_PARM_DESC(barkers,
		 "String of comma-separated 32-bit values; each is "
		 "recognized as the value the device sends as a reboot "
		 "signal; values are appended to a list--setting one value "
		 "as zero cleans the existing list and starts a new one.");

static
int i2400m_op_msg_from_user(struct wimax_dev *wimax_dev,
			    const char *pipe_name,
			    const void *msg_buf, size_t msg_len,
			    const struct genl_info *genl_info)
{
	int result;
	struct i2400m *i2400m = wimax_dev_to_i2400m(wimax_dev);
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *ack_skb;

	d_fnstart(4, dev, "(wimax_dev %p [i2400m %p] msg_buf %p "
		  "msg_len %zu genl_info %p)\n", wimax_dev, i2400m,
		  msg_buf, msg_len, genl_info);
	ack_skb = i2400m_msg_to_dev(i2400m, msg_buf, msg_len);
	result = PTR_ERR(ack_skb);
	if (IS_ERR(ack_skb))
		goto error_msg_to_dev;
	result = wimax_msg_send(&i2400m->wimax_dev, ack_skb);
error_msg_to_dev:
	d_fnend(4, dev, "(wimax_dev %p [i2400m %p] msg_buf %p msg_len %zu "
		"genl_info %p) = %d\n", wimax_dev, i2400m, msg_buf, msg_len,
		genl_info, result);
	return result;
}


struct i2400m_reset_ctx {
	struct completion completion;
	int result;
};


static
int i2400m_op_reset(struct wimax_dev *wimax_dev)
{
	int result;
	struct i2400m *i2400m = wimax_dev_to_i2400m(wimax_dev);
	struct device *dev = i2400m_dev(i2400m);
	struct i2400m_reset_ctx ctx = {
		.completion = COMPLETION_INITIALIZER_ONSTACK(ctx.completion),
		.result = 0,
	};

	d_fnstart(4, dev, "(wimax_dev %p)\n", wimax_dev);
	mutex_lock(&i2400m->init_mutex);
	i2400m->reset_ctx = &ctx;
	mutex_unlock(&i2400m->init_mutex);
	result = i2400m_reset(i2400m, I2400M_RT_WARM);
	if (result < 0)
		goto out;
	result = wait_for_completion_timeout(&ctx.completion, 4*HZ);
	if (result == 0)
		result = -ETIMEDOUT;
	else if (result > 0)
		result = ctx.result;
	
	mutex_lock(&i2400m->init_mutex);
	i2400m->reset_ctx = NULL;
	mutex_unlock(&i2400m->init_mutex);
out:
	d_fnend(4, dev, "(wimax_dev %p) = %d\n", wimax_dev, result);
	return result;
}


static
int i2400m_check_mac_addr(struct i2400m *i2400m)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *skb;
	const struct i2400m_tlv_detailed_device_info *ddi;
	struct net_device *net_dev = i2400m->wimax_dev.net_dev;
	const unsigned char zeromac[ETH_ALEN] = { 0 };

	d_fnstart(3, dev, "(i2400m %p)\n", i2400m);
	skb = i2400m_get_device_info(i2400m);
	if (IS_ERR(skb)) {
		result = PTR_ERR(skb);
		dev_err(dev, "Cannot verify MAC address, error reading: %d\n",
			result);
		goto error;
	}
	
	ddi = (void *) skb->data;
	BUILD_BUG_ON(ETH_ALEN != sizeof(ddi->mac_address));
	d_printf(2, dev, "GET DEVICE INFO: mac addr %pM\n",
		 ddi->mac_address);
	if (!memcmp(net_dev->perm_addr, ddi->mac_address,
		   sizeof(ddi->mac_address)))
		goto ok;
	dev_warn(dev, "warning: device reports a different MAC address "
		 "to that of boot mode's\n");
	dev_warn(dev, "device reports     %pM\n", ddi->mac_address);
	dev_warn(dev, "boot mode reported %pM\n", net_dev->perm_addr);
	if (!memcmp(zeromac, ddi->mac_address, sizeof(zeromac)))
		dev_err(dev, "device reports an invalid MAC address, "
			"not updating\n");
	else {
		dev_warn(dev, "updating MAC address\n");
		net_dev->addr_len = ETH_ALEN;
		memcpy(net_dev->perm_addr, ddi->mac_address, ETH_ALEN);
		memcpy(net_dev->dev_addr, ddi->mac_address, ETH_ALEN);
	}
ok:
	result = 0;
	kfree_skb(skb);
error:
	d_fnend(3, dev, "(i2400m %p) = %d\n", i2400m, result);
	return result;
}


static
int __i2400m_dev_start(struct i2400m *i2400m, enum i2400m_bri flags)
{
	int result;
	struct wimax_dev *wimax_dev = &i2400m->wimax_dev;
	struct net_device *net_dev = wimax_dev->net_dev;
	struct device *dev = i2400m_dev(i2400m);
	int times = i2400m->bus_bm_retries;

	d_fnstart(3, dev, "(i2400m %p)\n", i2400m);
retry:
	result = i2400m_dev_bootstrap(i2400m, flags);
	if (result < 0) {
		dev_err(dev, "cannot bootstrap device: %d\n", result);
		goto error_bootstrap;
	}
	result = i2400m_tx_setup(i2400m);
	if (result < 0)
		goto error_tx_setup;
	result = i2400m_rx_setup(i2400m);
	if (result < 0)
		goto error_rx_setup;
	i2400m->work_queue = create_singlethread_workqueue(wimax_dev->name);
	if (i2400m->work_queue == NULL) {
		result = -ENOMEM;
		dev_err(dev, "cannot create workqueue\n");
		goto error_create_workqueue;
	}
	if (i2400m->bus_dev_start) {
		result = i2400m->bus_dev_start(i2400m);
		if (result < 0)
			goto error_bus_dev_start;
	}
	i2400m->ready = 1;
	wmb();		
	
	queue_work(i2400m->work_queue, &i2400m->rx_report_ws);
	result = i2400m_firmware_check(i2400m);	
	if (result < 0)
		goto error_fw_check;
	
	result = i2400m_check_mac_addr(i2400m);
	if (result < 0)
		goto error_check_mac_addr;
	result = i2400m_dev_initialize(i2400m);
	if (result < 0)
		goto error_dev_initialize;


	atomic_dec(&i2400m->error_recovery);

	d_fnend(3, dev, "(net_dev %p [i2400m %p]) = %d\n",
		net_dev, i2400m, result);
	return result;

error_dev_initialize:
error_check_mac_addr:
error_fw_check:
	i2400m->ready = 0;
	wmb();		
	flush_workqueue(i2400m->work_queue);
	if (i2400m->bus_dev_stop)
		i2400m->bus_dev_stop(i2400m);
error_bus_dev_start:
	destroy_workqueue(i2400m->work_queue);
error_create_workqueue:
	i2400m_rx_release(i2400m);
error_rx_setup:
	i2400m_tx_release(i2400m);
error_tx_setup:
error_bootstrap:
	if (result == -EL3RST && times-- > 0) {
		flags = I2400M_BRI_SOFT|I2400M_BRI_MAC_REINIT;
		goto retry;
	}
	d_fnend(3, dev, "(net_dev %p [i2400m %p]) = %d\n",
		net_dev, i2400m, result);
	return result;
}


static
int i2400m_dev_start(struct i2400m *i2400m, enum i2400m_bri bm_flags)
{
	int result = 0;
	mutex_lock(&i2400m->init_mutex);	
	if (i2400m->updown == 0) {
		result = __i2400m_dev_start(i2400m, bm_flags);
		if (result >= 0) {
			i2400m->updown = 1;
			i2400m->alive = 1;
			wmb();
		}
	}
	mutex_unlock(&i2400m->init_mutex);
	return result;
}


static
void __i2400m_dev_stop(struct i2400m *i2400m)
{
	struct wimax_dev *wimax_dev = &i2400m->wimax_dev;
	struct device *dev = i2400m_dev(i2400m);

	d_fnstart(3, dev, "(i2400m %p)\n", i2400m);
	wimax_state_change(wimax_dev, __WIMAX_ST_QUIESCING);
	i2400m_msg_to_dev_cancel_wait(i2400m, -EL3RST);
	complete(&i2400m->msg_completion);
	i2400m_net_wake_stop(i2400m);
	i2400m_dev_shutdown(i2400m);
	i2400m->ready = 0;	
	wmb();		
	flush_workqueue(i2400m->work_queue);

	if (i2400m->bus_dev_stop)
		i2400m->bus_dev_stop(i2400m);
	destroy_workqueue(i2400m->work_queue);
	i2400m_rx_release(i2400m);
	i2400m_tx_release(i2400m);
	wimax_state_change(wimax_dev, WIMAX_ST_DOWN);
	d_fnend(3, dev, "(i2400m %p) = 0\n", i2400m);
}


static
void i2400m_dev_stop(struct i2400m *i2400m)
{
	mutex_lock(&i2400m->init_mutex);
	if (i2400m->updown) {
		__i2400m_dev_stop(i2400m);
		i2400m->updown = 0;
		i2400m->alive = 0;
		wmb();	
	}
	mutex_unlock(&i2400m->init_mutex);
}


static
int i2400m_pm_notifier(struct notifier_block *notifier,
		       unsigned long pm_event,
		       void *unused)
{
	struct i2400m *i2400m =
		container_of(notifier, struct i2400m, pm_notifier);
	struct device *dev = i2400m_dev(i2400m);

	d_fnstart(3, dev, "(i2400m %p pm_event %lx)\n", i2400m, pm_event);
	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		i2400m_fw_cache(i2400m);
		break;
	case PM_POST_RESTORE:
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
		i2400m_fw_uncache(i2400m);
		break;

	case PM_RESTORE_PREPARE:
	default:
		break;
	}
	d_fnend(3, dev, "(i2400m %p pm_event %lx) = void\n", i2400m, pm_event);
	return NOTIFY_DONE;
}


int i2400m_pre_reset(struct i2400m *i2400m)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);

	d_fnstart(3, dev, "(i2400m %p)\n", i2400m);
	d_printf(1, dev, "pre-reset shut down\n");

	result = 0;
	mutex_lock(&i2400m->init_mutex);
	if (i2400m->updown) {
		netif_tx_disable(i2400m->wimax_dev.net_dev);
		__i2400m_dev_stop(i2400m);
		result = 0;
	}
	mutex_unlock(&i2400m->init_mutex);
	if (i2400m->bus_release)
		i2400m->bus_release(i2400m);
	d_fnend(3, dev, "(i2400m %p) = %d\n", i2400m, result);
	return result;
}
EXPORT_SYMBOL_GPL(i2400m_pre_reset);


int i2400m_post_reset(struct i2400m *i2400m)
{
	int result = 0;
	struct device *dev = i2400m_dev(i2400m);

	d_fnstart(3, dev, "(i2400m %p)\n", i2400m);
	d_printf(1, dev, "post-reset start\n");
	if (i2400m->bus_setup) {
		result = i2400m->bus_setup(i2400m);
		if (result < 0) {
			dev_err(dev, "bus-specific setup failed: %d\n",
				result);
			goto error_bus_setup;
		}
	}
	mutex_lock(&i2400m->init_mutex);
	if (i2400m->updown) {
		result = __i2400m_dev_start(
			i2400m, I2400M_BRI_SOFT | I2400M_BRI_MAC_REINIT);
		if (result < 0)
			goto error_dev_start;
	}
	mutex_unlock(&i2400m->init_mutex);
	d_fnend(3, dev, "(i2400m %p) = %d\n", i2400m, result);
	return result;

error_dev_start:
	if (i2400m->bus_release)
		i2400m->bus_release(i2400m);
	i2400m->updown = 0;
	wmb();		
	mutex_unlock(&i2400m->init_mutex);
error_bus_setup:
	d_fnend(3, dev, "(i2400m %p) = %d\n", i2400m, result);
	return result;
}
EXPORT_SYMBOL_GPL(i2400m_post_reset);


static
void __i2400m_dev_reset_handle(struct work_struct *ws)
{
	struct i2400m *i2400m = container_of(ws, struct i2400m, reset_ws);
	const char *reason = i2400m->reset_reason;
	struct device *dev = i2400m_dev(i2400m);
	struct i2400m_reset_ctx *ctx = i2400m->reset_ctx;
	int result;

	d_fnstart(3, dev, "(ws %p i2400m %p reason %s)\n", ws, i2400m, reason);

	i2400m->boot_mode = 1;
	wmb();		

	result = 0;
	if (mutex_trylock(&i2400m->init_mutex) == 0) {
		dev_err(dev, "device rebooted somewhere else?\n");
		i2400m_msg_to_dev_cancel_wait(i2400m, -EL3RST);
		complete(&i2400m->msg_completion);
		goto out;
	}

	dev_err(dev, "%s: reinitializing driver\n", reason);
	rmb();
	if (i2400m->updown) {
		__i2400m_dev_stop(i2400m);
		i2400m->updown = 0;
		wmb();		
	}

	if (i2400m->alive) {
		result = __i2400m_dev_start(i2400m,
				    I2400M_BRI_SOFT | I2400M_BRI_MAC_REINIT);
		if (result < 0) {
			dev_err(dev, "%s: cannot start the device: %d\n",
				reason, result);
			result = -EUCLEAN;
			if (atomic_read(&i2400m->bus_reset_retries)
					>= I2400M_BUS_RESET_RETRIES) {
				result = -ENODEV;
				dev_err(dev, "tried too many times to "
					"reset the device, giving up\n");
			}
		}
	}

	if (i2400m->reset_ctx) {
		ctx->result = result;
		complete(&ctx->completion);
	}
	mutex_unlock(&i2400m->init_mutex);
	if (result == -EUCLEAN) {
		i2400m->boot_mode = 0;
		wmb();

		atomic_inc(&i2400m->bus_reset_retries);
		
		result = i2400m_reset(i2400m, I2400M_RT_BUS);
		if (result >= 0)
			result = -ENODEV;
	} else {
		rmb();
		if (i2400m->alive) {
			i2400m->updown = 1;
			wmb();
			atomic_set(&i2400m->bus_reset_retries, 0);
		}
	}
out:
	d_fnend(3, dev, "(ws %p i2400m %p reason %s) = void\n",
		ws, i2400m, reason);
}


int i2400m_dev_reset_handle(struct i2400m *i2400m, const char *reason)
{
	i2400m->reset_reason = reason;
	return schedule_work(&i2400m->reset_ws);
}
EXPORT_SYMBOL_GPL(i2400m_dev_reset_handle);


static
void __i2400m_error_recovery(struct work_struct *ws)
{
	struct i2400m *i2400m = container_of(ws, struct i2400m, recovery_ws);

	i2400m_reset(i2400m, I2400M_RT_BUS);
}

void i2400m_error_recovery(struct i2400m *i2400m)
{
	if (atomic_add_return(1, &i2400m->error_recovery) == 1)
		schedule_work(&i2400m->recovery_ws);
	else
		atomic_dec(&i2400m->error_recovery);
}
EXPORT_SYMBOL_GPL(i2400m_error_recovery);

static
int i2400m_bm_buf_alloc(struct i2400m *i2400m)
{
	int result;

	result = -ENOMEM;
	i2400m->bm_cmd_buf = kzalloc(I2400M_BM_CMD_BUF_SIZE, GFP_KERNEL);
	if (i2400m->bm_cmd_buf == NULL)
		goto error_bm_cmd_kzalloc;
	i2400m->bm_ack_buf = kzalloc(I2400M_BM_ACK_BUF_SIZE, GFP_KERNEL);
	if (i2400m->bm_ack_buf == NULL)
		goto error_bm_ack_buf_kzalloc;
	return 0;

error_bm_ack_buf_kzalloc:
	kfree(i2400m->bm_cmd_buf);
error_bm_cmd_kzalloc:
	return result;
}


static
void i2400m_bm_buf_free(struct i2400m *i2400m)
{
	kfree(i2400m->bm_ack_buf);
	kfree(i2400m->bm_cmd_buf);
}


void i2400m_init(struct i2400m *i2400m)
{
	wimax_dev_init(&i2400m->wimax_dev);

	i2400m->boot_mode = 1;
	i2400m->rx_reorder = 1;
	init_waitqueue_head(&i2400m->state_wq);

	spin_lock_init(&i2400m->tx_lock);
	i2400m->tx_pl_min = UINT_MAX;
	i2400m->tx_size_min = UINT_MAX;

	spin_lock_init(&i2400m->rx_lock);
	i2400m->rx_pl_min = UINT_MAX;
	i2400m->rx_size_min = UINT_MAX;
	INIT_LIST_HEAD(&i2400m->rx_reports);
	INIT_WORK(&i2400m->rx_report_ws, i2400m_report_hook_work);

	mutex_init(&i2400m->msg_mutex);
	init_completion(&i2400m->msg_completion);

	mutex_init(&i2400m->init_mutex);
	

	INIT_WORK(&i2400m->reset_ws, __i2400m_dev_reset_handle);
	INIT_WORK(&i2400m->recovery_ws, __i2400m_error_recovery);

	atomic_set(&i2400m->bus_reset_retries, 0);

	i2400m->alive = 0;

	atomic_set(&i2400m->error_recovery, 1);
}
EXPORT_SYMBOL_GPL(i2400m_init);


int i2400m_reset(struct i2400m *i2400m, enum i2400m_reset_type rt)
{
	struct net_device *net_dev = i2400m->wimax_dev.net_dev;

	if (net_dev->reg_state == NETREG_REGISTERED) {
		netif_tx_disable(net_dev);
		netif_carrier_off(net_dev);
	}
	return i2400m->bus_reset(i2400m, rt);
}
EXPORT_SYMBOL_GPL(i2400m_reset);


int i2400m_setup(struct i2400m *i2400m, enum i2400m_bri bm_flags)
{
	int result = -ENODEV;
	struct device *dev = i2400m_dev(i2400m);
	struct wimax_dev *wimax_dev = &i2400m->wimax_dev;
	struct net_device *net_dev = i2400m->wimax_dev.net_dev;

	d_fnstart(3, dev, "(i2400m %p)\n", i2400m);

	snprintf(wimax_dev->name, sizeof(wimax_dev->name),
		 "i2400m-%s:%s", dev->bus->name, dev_name(dev));

	result = i2400m_bm_buf_alloc(i2400m);
	if (result < 0) {
		dev_err(dev, "cannot allocate bootmode scratch buffers\n");
		goto error_bm_buf_alloc;
	}

	if (i2400m->bus_setup) {
		result = i2400m->bus_setup(i2400m);
		if (result < 0) {
			dev_err(dev, "bus-specific setup failed: %d\n",
				result);
			goto error_bus_setup;
		}
	}

	result = i2400m_bootrom_init(i2400m, bm_flags);
	if (result < 0) {
		dev_err(dev, "read mac addr: bootrom init "
			"failed: %d\n", result);
		goto error_bootrom_init;
	}
	result = i2400m_read_mac_addr(i2400m);
	if (result < 0)
		goto error_read_mac_addr;
	random_ether_addr(i2400m->src_mac_addr);

	i2400m->pm_notifier.notifier_call = i2400m_pm_notifier;
	register_pm_notifier(&i2400m->pm_notifier);

	result = register_netdev(net_dev);	
	if (result < 0) {
		dev_err(dev, "cannot register i2400m network device: %d\n",
			result);
		goto error_register_netdev;
	}
	netif_carrier_off(net_dev);

	i2400m->wimax_dev.op_msg_from_user = i2400m_op_msg_from_user;
	i2400m->wimax_dev.op_rfkill_sw_toggle = i2400m_op_rfkill_sw_toggle;
	i2400m->wimax_dev.op_reset = i2400m_op_reset;

	result = wimax_dev_add(&i2400m->wimax_dev, net_dev);
	if (result < 0)
		goto error_wimax_dev_add;

	
	result = sysfs_create_group(&net_dev->dev.kobj, &i2400m_dev_attr_group);
	if (result < 0) {
		dev_err(dev, "cannot setup i2400m's sysfs: %d\n", result);
		goto error_sysfs_setup;
	}

	result = i2400m_debugfs_add(i2400m);
	if (result < 0) {
		dev_err(dev, "cannot setup i2400m's debugfs: %d\n", result);
		goto error_debugfs_setup;
	}

	result = i2400m_dev_start(i2400m, bm_flags);
	if (result < 0)
		goto error_dev_start;
	d_fnend(3, dev, "(i2400m %p) = %d\n", i2400m, result);
	return result;

error_dev_start:
	i2400m_debugfs_rm(i2400m);
error_debugfs_setup:
	sysfs_remove_group(&i2400m->wimax_dev.net_dev->dev.kobj,
			   &i2400m_dev_attr_group);
error_sysfs_setup:
	wimax_dev_rm(&i2400m->wimax_dev);
error_wimax_dev_add:
	unregister_netdev(net_dev);
error_register_netdev:
	unregister_pm_notifier(&i2400m->pm_notifier);
error_read_mac_addr:
error_bootrom_init:
	if (i2400m->bus_release)
		i2400m->bus_release(i2400m);
error_bus_setup:
	i2400m_bm_buf_free(i2400m);
error_bm_buf_alloc:
	d_fnend(3, dev, "(i2400m %p) = %d\n", i2400m, result);
	return result;
}
EXPORT_SYMBOL_GPL(i2400m_setup);


void i2400m_release(struct i2400m *i2400m)
{
	struct device *dev = i2400m_dev(i2400m);

	d_fnstart(3, dev, "(i2400m %p)\n", i2400m);
	netif_stop_queue(i2400m->wimax_dev.net_dev);

	i2400m_dev_stop(i2400m);

	cancel_work_sync(&i2400m->reset_ws);
	cancel_work_sync(&i2400m->recovery_ws);

	i2400m_debugfs_rm(i2400m);
	sysfs_remove_group(&i2400m->wimax_dev.net_dev->dev.kobj,
			   &i2400m_dev_attr_group);
	wimax_dev_rm(&i2400m->wimax_dev);
	unregister_netdev(i2400m->wimax_dev.net_dev);
	unregister_pm_notifier(&i2400m->pm_notifier);
	if (i2400m->bus_release)
		i2400m->bus_release(i2400m);
	i2400m_bm_buf_free(i2400m);
	d_fnend(3, dev, "(i2400m %p) = void\n", i2400m);
}
EXPORT_SYMBOL_GPL(i2400m_release);


struct d_level D_LEVEL[] = {
	D_SUBMODULE_DEFINE(control),
	D_SUBMODULE_DEFINE(driver),
	D_SUBMODULE_DEFINE(debugfs),
	D_SUBMODULE_DEFINE(fw),
	D_SUBMODULE_DEFINE(netdev),
	D_SUBMODULE_DEFINE(rfkill),
	D_SUBMODULE_DEFINE(rx),
	D_SUBMODULE_DEFINE(sysfs),
	D_SUBMODULE_DEFINE(tx),
};
size_t D_LEVEL_SIZE = ARRAY_SIZE(D_LEVEL);


static
int __init i2400m_driver_init(void)
{
	d_parse_params(D_LEVEL, D_LEVEL_SIZE, i2400m_debug_params,
		       "i2400m.debug");
	return i2400m_barker_db_init(i2400m_barkers_params);
}
module_init(i2400m_driver_init);

static
void __exit i2400m_driver_exit(void)
{
	i2400m_barker_db_exit();
}
module_exit(i2400m_driver_exit);

MODULE_AUTHOR("Intel Corporation <linux-wimax@intel.com>");
MODULE_DESCRIPTION("Intel 2400M WiMAX networking bus-generic driver");
MODULE_LICENSE("GPL");
