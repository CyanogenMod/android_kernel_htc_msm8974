/*
 * Linux WiMAX
 * Initialization, addition and removal of wimax devices
 *
 *
 * Copyright (C) 2005-2006 Intel Corporation <linux-wimax@intel.com>
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
 * This implements:
 *
 *   - basic life cycle of 'struct wimax_dev' [wimax_dev_*()]; on
 *     addition/registration initialize all subfields and allocate
 *     generic netlink resources for user space communication. On
 *     removal/unregistration, undo all that.
 *
 *   - device state machine [wimax_state_change()] and support to send
 *     reports to user space when the state changes
 *     [wimax_gnl_re_state_change*()].
 *
 * See include/net/wimax.h for rationales and design.
 *
 * ROADMAP
 *
 * [__]wimax_state_change()     Called by drivers to update device's state
 *   wimax_gnl_re_state_change_alloc()
 *   wimax_gnl_re_state_change_send()
 *
 * wimax_dev_init()	        Init a device
 * wimax_dev_add()              Register
 *   wimax_rfkill_add()
 *   wimax_gnl_add()            Register all the generic netlink resources.
 *   wimax_id_table_add()
 * wimax_dev_rm()               Unregister
 *   wimax_id_table_rm()
 *   wimax_gnl_rm()
 *   wimax_rfkill_rm()
 */
#include <linux/device.h>
#include <linux/gfp.h>
#include <net/genetlink.h>
#include <linux/netdevice.h>
#include <linux/wimax.h>
#include <linux/module.h>
#include "wimax-internal.h"


#define D_SUBMODULE stack
#include "debug-levels.h"

static char wimax_debug_params[128];
module_param_string(debug, wimax_debug_params, sizeof(wimax_debug_params),
		    0644);
MODULE_PARM_DESC(debug,
		 "String of space-separated NAME:VALUE pairs, where NAMEs "
		 "are the different debug submodules and VALUE are the "
		 "initial debug value to set.");



static
struct sk_buff *wimax_gnl_re_state_change_alloc(
	struct wimax_dev *wimax_dev,
	enum wimax_st new_state, enum wimax_st old_state,
	void **header)
{
	int result;
	struct device *dev = wimax_dev_to_dev(wimax_dev);
	void *data;
	struct sk_buff *report_skb;

	d_fnstart(3, dev, "(wimax_dev %p new_state %u old_state %u)\n",
		  wimax_dev, new_state, old_state);
	result = -ENOMEM;
	report_skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (report_skb == NULL) {
		dev_err(dev, "RE_STCH: can't create message\n");
		goto error_new;
	}
	data = genlmsg_put(report_skb, 0, wimax_gnl_mcg.id, &wimax_gnl_family,
			   0, WIMAX_GNL_RE_STATE_CHANGE);
	if (data == NULL) {
		dev_err(dev, "RE_STCH: can't put data into message\n");
		goto error_put;
	}
	*header = data;

	result = nla_put_u8(report_skb, WIMAX_GNL_STCH_STATE_OLD, old_state);
	if (result < 0) {
		dev_err(dev, "RE_STCH: Error adding OLD attr: %d\n", result);
		goto error_put;
	}
	result = nla_put_u8(report_skb, WIMAX_GNL_STCH_STATE_NEW, new_state);
	if (result < 0) {
		dev_err(dev, "RE_STCH: Error adding NEW attr: %d\n", result);
		goto error_put;
	}
	result = nla_put_u32(report_skb, WIMAX_GNL_STCH_IFIDX,
			     wimax_dev->net_dev->ifindex);
	if (result < 0) {
		dev_err(dev, "RE_STCH: Error adding IFINDEX attribute\n");
		goto error_put;
	}
	d_fnend(3, dev, "(wimax_dev %p new_state %u old_state %u) = %p\n",
		wimax_dev, new_state, old_state, report_skb);
	return report_skb;

error_put:
	nlmsg_free(report_skb);
error_new:
	d_fnend(3, dev, "(wimax_dev %p new_state %u old_state %u) = %d\n",
		wimax_dev, new_state, old_state, result);
	return ERR_PTR(result);
}


static
int wimax_gnl_re_state_change_send(
	struct wimax_dev *wimax_dev, struct sk_buff *report_skb,
	void *header)
{
	int result = 0;
	struct device *dev = wimax_dev_to_dev(wimax_dev);
	d_fnstart(3, dev, "(wimax_dev %p report_skb %p)\n",
		  wimax_dev, report_skb);
	if (report_skb == NULL) {
		result = -ENOMEM;
		goto out;
	}
	genlmsg_end(report_skb, header);
	genlmsg_multicast(report_skb, 0, wimax_gnl_mcg.id, GFP_KERNEL);
out:
	d_fnend(3, dev, "(wimax_dev %p report_skb %p) = %d\n",
		wimax_dev, report_skb, result);
	return result;
}


static
void __check_new_state(enum wimax_st old_state, enum wimax_st new_state,
		       unsigned allowed_states_bm)
{
	if (WARN_ON(((1 << new_state) & allowed_states_bm) == 0)) {
		printk(KERN_ERR "SW BUG! Forbidden state change %u -> %u\n",
			old_state, new_state);
	}
}


void __wimax_state_change(struct wimax_dev *wimax_dev, enum wimax_st new_state)
{
	struct device *dev = wimax_dev_to_dev(wimax_dev);
	enum wimax_st old_state = wimax_dev->state;
	struct sk_buff *stch_skb;
	void *header;

	d_fnstart(3, dev, "(wimax_dev %p new_state %u [old %u])\n",
		  wimax_dev, new_state, old_state);

	if (WARN_ON(new_state >= __WIMAX_ST_INVALID)) {
		dev_err(dev, "SW BUG: requesting invalid state %u\n",
			new_state);
		goto out;
	}
	if (old_state == new_state)
		goto out;
	header = NULL;	
	stch_skb = wimax_gnl_re_state_change_alloc(
		wimax_dev, new_state, old_state, &header);

	
	switch (old_state) {
	case __WIMAX_ST_NULL:
		__check_new_state(old_state, new_state,
				  1 << WIMAX_ST_DOWN);
		break;
	case WIMAX_ST_DOWN:
		__check_new_state(old_state, new_state,
				  1 << __WIMAX_ST_QUIESCING
				  | 1 << WIMAX_ST_UNINITIALIZED
				  | 1 << WIMAX_ST_RADIO_OFF);
		break;
	case __WIMAX_ST_QUIESCING:
		__check_new_state(old_state, new_state, 1 << WIMAX_ST_DOWN);
		break;
	case WIMAX_ST_UNINITIALIZED:
		__check_new_state(old_state, new_state,
				  1 << __WIMAX_ST_QUIESCING
				  | 1 << WIMAX_ST_RADIO_OFF);
		break;
	case WIMAX_ST_RADIO_OFF:
		__check_new_state(old_state, new_state,
				  1 << __WIMAX_ST_QUIESCING
				  | 1 << WIMAX_ST_READY);
		break;
	case WIMAX_ST_READY:
		__check_new_state(old_state, new_state,
				  1 << __WIMAX_ST_QUIESCING
				  | 1 << WIMAX_ST_RADIO_OFF
				  | 1 << WIMAX_ST_SCANNING
				  | 1 << WIMAX_ST_CONNECTING
				  | 1 << WIMAX_ST_CONNECTED);
		break;
	case WIMAX_ST_SCANNING:
		__check_new_state(old_state, new_state,
				  1 << __WIMAX_ST_QUIESCING
				  | 1 << WIMAX_ST_RADIO_OFF
				  | 1 << WIMAX_ST_READY
				  | 1 << WIMAX_ST_CONNECTING
				  | 1 << WIMAX_ST_CONNECTED);
		break;
	case WIMAX_ST_CONNECTING:
		__check_new_state(old_state, new_state,
				  1 << __WIMAX_ST_QUIESCING
				  | 1 << WIMAX_ST_RADIO_OFF
				  | 1 << WIMAX_ST_READY
				  | 1 << WIMAX_ST_SCANNING
				  | 1 << WIMAX_ST_CONNECTED);
		break;
	case WIMAX_ST_CONNECTED:
		__check_new_state(old_state, new_state,
				  1 << __WIMAX_ST_QUIESCING
				  | 1 << WIMAX_ST_RADIO_OFF
				  | 1 << WIMAX_ST_READY);
		netif_tx_disable(wimax_dev->net_dev);
		netif_carrier_off(wimax_dev->net_dev);
		break;
	case __WIMAX_ST_INVALID:
	default:
		dev_err(dev, "SW BUG: wimax_dev %p is in unknown state %u\n",
			wimax_dev, wimax_dev->state);
		WARN_ON(1);
		goto out;
	}

	
	switch (new_state) {
	case __WIMAX_ST_NULL:
		dev_err(dev, "SW BUG: wimax_dev %p entering NULL state "
			"from %u\n", wimax_dev, wimax_dev->state);
		WARN_ON(1);		
		break;
	case WIMAX_ST_DOWN:
		break;
	case __WIMAX_ST_QUIESCING:
		break;
	case WIMAX_ST_UNINITIALIZED:
		break;
	case WIMAX_ST_RADIO_OFF:
		break;
	case WIMAX_ST_READY:
		break;
	case WIMAX_ST_SCANNING:
		break;
	case WIMAX_ST_CONNECTING:
		break;
	case WIMAX_ST_CONNECTED:
		netif_carrier_on(wimax_dev->net_dev);
		netif_wake_queue(wimax_dev->net_dev);
		break;
	case __WIMAX_ST_INVALID:
	default:
		BUG();
	}
	__wimax_state_set(wimax_dev, new_state);
	if (!IS_ERR(stch_skb))
		wimax_gnl_re_state_change_send(wimax_dev, stch_skb, header);
out:
	d_fnend(3, dev, "(wimax_dev %p new_state %u [old %u]) = void\n",
		wimax_dev, new_state, old_state);
}


void wimax_state_change(struct wimax_dev *wimax_dev, enum wimax_st new_state)
{
	mutex_lock(&wimax_dev->mutex);
	if (wimax_dev->state > __WIMAX_ST_NULL)
		__wimax_state_change(wimax_dev, new_state);
	mutex_unlock(&wimax_dev->mutex);
}
EXPORT_SYMBOL_GPL(wimax_state_change);


enum wimax_st wimax_state_get(struct wimax_dev *wimax_dev)
{
	enum wimax_st state;
	mutex_lock(&wimax_dev->mutex);
	state = wimax_dev->state;
	mutex_unlock(&wimax_dev->mutex);
	return state;
}
EXPORT_SYMBOL_GPL(wimax_state_get);


void wimax_dev_init(struct wimax_dev *wimax_dev)
{
	INIT_LIST_HEAD(&wimax_dev->id_table_node);
	__wimax_state_set(wimax_dev, __WIMAX_ST_NULL);
	mutex_init(&wimax_dev->mutex);
	mutex_init(&wimax_dev->mutex_reset);
}
EXPORT_SYMBOL_GPL(wimax_dev_init);

extern struct genl_ops
	wimax_gnl_msg_from_user,
	wimax_gnl_reset,
	wimax_gnl_rfkill,
	wimax_gnl_state_get;

static
struct genl_ops *wimax_gnl_ops[] = {
	&wimax_gnl_msg_from_user,
	&wimax_gnl_reset,
	&wimax_gnl_rfkill,
	&wimax_gnl_state_get,
};


static
size_t wimax_addr_scnprint(char *addr_str, size_t addr_str_size,
			   unsigned char *addr, size_t addr_len)
{
	unsigned cnt, total;
	for (total = cnt = 0; cnt < addr_len; cnt++)
		total += scnprintf(addr_str + total, addr_str_size - total,
				   "%02x%c", addr[cnt],
				   cnt == addr_len - 1 ? '\0' : ':');
	return total;
}


int wimax_dev_add(struct wimax_dev *wimax_dev, struct net_device *net_dev)
{
	int result;
	struct device *dev = net_dev->dev.parent;
	char addr_str[32];

	d_fnstart(3, dev, "(wimax_dev %p net_dev %p)\n", wimax_dev, net_dev);

	wimax_dev->net_dev = net_dev;
	result = wimax_rfkill_add(wimax_dev);
	if (result < 0)
		goto error_rfkill_add;

	
	mutex_lock(&wimax_dev->mutex);
	wimax_id_table_add(wimax_dev);
	result = wimax_debugfs_add(wimax_dev);
	if (result < 0) {
		dev_err(dev, "cannot initialize debugfs: %d\n",
			result);
		goto error_debugfs_add;
	}

	__wimax_state_set(wimax_dev, WIMAX_ST_DOWN);
	mutex_unlock(&wimax_dev->mutex);

	wimax_addr_scnprint(addr_str, sizeof(addr_str),
			    net_dev->dev_addr, net_dev->addr_len);
	dev_err(dev, "WiMAX interface %s (%s) ready\n",
		net_dev->name, addr_str);
	d_fnend(3, dev, "(wimax_dev %p net_dev %p) = 0\n", wimax_dev, net_dev);
	return 0;

error_debugfs_add:
	wimax_id_table_rm(wimax_dev);
	mutex_unlock(&wimax_dev->mutex);
	wimax_rfkill_rm(wimax_dev);
error_rfkill_add:
	d_fnend(3, dev, "(wimax_dev %p net_dev %p) = %d\n",
		wimax_dev, net_dev, result);
	return result;
}
EXPORT_SYMBOL_GPL(wimax_dev_add);


void wimax_dev_rm(struct wimax_dev *wimax_dev)
{
	d_fnstart(3, NULL, "(wimax_dev %p)\n", wimax_dev);

	mutex_lock(&wimax_dev->mutex);
	__wimax_state_change(wimax_dev, __WIMAX_ST_QUIESCING);
	wimax_debugfs_rm(wimax_dev);
	wimax_id_table_rm(wimax_dev);
	__wimax_state_change(wimax_dev, WIMAX_ST_DOWN);
	mutex_unlock(&wimax_dev->mutex);
	wimax_rfkill_rm(wimax_dev);
	d_fnend(3, NULL, "(wimax_dev %p) = void\n", wimax_dev);
}
EXPORT_SYMBOL_GPL(wimax_dev_rm);


struct d_level D_LEVEL[] = {
	D_SUBMODULE_DEFINE(debugfs),
	D_SUBMODULE_DEFINE(id_table),
	D_SUBMODULE_DEFINE(op_msg),
	D_SUBMODULE_DEFINE(op_reset),
	D_SUBMODULE_DEFINE(op_rfkill),
	D_SUBMODULE_DEFINE(op_state_get),
	D_SUBMODULE_DEFINE(stack),
};
size_t D_LEVEL_SIZE = ARRAY_SIZE(D_LEVEL);


struct genl_family wimax_gnl_family = {
	.id = GENL_ID_GENERATE,
	.name = "WiMAX",
	.version = WIMAX_GNL_VERSION,
	.hdrsize = 0,
	.maxattr = WIMAX_GNL_ATTR_MAX,
};

struct genl_multicast_group wimax_gnl_mcg = {
	.name = "msg",
};



static
int __init wimax_subsys_init(void)
{
	int result, cnt;

	d_fnstart(4, NULL, "()\n");
	d_parse_params(D_LEVEL, D_LEVEL_SIZE, wimax_debug_params,
		       "wimax.debug");

	snprintf(wimax_gnl_family.name, sizeof(wimax_gnl_family.name),
		 "WiMAX");
	result = genl_register_family(&wimax_gnl_family);
	if (unlikely(result < 0)) {
		printk(KERN_ERR "cannot register generic netlink family: %d\n",
		       result);
		goto error_register_family;
	}

	for (cnt = 0; cnt < ARRAY_SIZE(wimax_gnl_ops); cnt++) {
		result = genl_register_ops(&wimax_gnl_family,
					   wimax_gnl_ops[cnt]);
		d_printf(4, NULL, "registering generic netlink op code "
			 "%u: %d\n", wimax_gnl_ops[cnt]->cmd, result);
		if (unlikely(result < 0)) {
			printk(KERN_ERR "cannot register generic netlink op "
			       "code %u: %d\n",
			       wimax_gnl_ops[cnt]->cmd, result);
			goto error_register_ops;
		}
	}

	result = genl_register_mc_group(&wimax_gnl_family, &wimax_gnl_mcg);
	if (result < 0)
		goto error_mc_group;
	d_fnend(4, NULL, "() = 0\n");
	return 0;

error_mc_group:
error_register_ops:
	for (cnt--; cnt >= 0; cnt--)
		genl_unregister_ops(&wimax_gnl_family,
				    wimax_gnl_ops[cnt]);
	genl_unregister_family(&wimax_gnl_family);
error_register_family:
	d_fnend(4, NULL, "() = %d\n", result);
	return result;

}
module_init(wimax_subsys_init);


static
void __exit wimax_subsys_exit(void)
{
	int cnt;
	wimax_id_table_release();
	genl_unregister_mc_group(&wimax_gnl_family, &wimax_gnl_mcg);
	for (cnt = ARRAY_SIZE(wimax_gnl_ops) - 1; cnt >= 0; cnt--)
		genl_unregister_ops(&wimax_gnl_family,
				    wimax_gnl_ops[cnt]);
	genl_unregister_family(&wimax_gnl_family);
}
module_exit(wimax_subsys_exit);

MODULE_AUTHOR("Intel Corporation <linux-wimax@intel.com>");
MODULE_DESCRIPTION("Linux WiMAX stack");
MODULE_LICENSE("GPL");

