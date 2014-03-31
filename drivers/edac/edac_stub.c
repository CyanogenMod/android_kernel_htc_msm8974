/*
 * common EDAC components that must be in kernel
 *
 * Author: Dave Jiang <djiang@mvista.com>
 *
 * 2007 (c) MontaVista Software, Inc.
 * 2010 (c) Advanced Micro Devices Inc.
 *	    Borislav Petkov <borislav.petkov@amd.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */
#include <linux/module.h>
#include <linux/edac.h>
#include <linux/atomic.h>
#include <linux/device.h>
#include <asm/edac.h>

int edac_op_state = EDAC_OPSTATE_INVAL;
EXPORT_SYMBOL_GPL(edac_op_state);

atomic_t edac_handlers = ATOMIC_INIT(0);
EXPORT_SYMBOL_GPL(edac_handlers);

int edac_err_assert = 0;
EXPORT_SYMBOL_GPL(edac_err_assert);

static atomic_t edac_subsys_valid = ATOMIC_INIT(0);

int edac_handler_set(void)
{
	if (edac_op_state == EDAC_OPSTATE_POLL)
		return 0;

	return atomic_read(&edac_handlers);
}
EXPORT_SYMBOL_GPL(edac_handler_set);

void edac_atomic_assert_error(void)
{
	edac_err_assert++;
}
EXPORT_SYMBOL_GPL(edac_atomic_assert_error);

struct bus_type edac_subsys = {
	.name = "edac",
	.dev_name = "edac",
};
EXPORT_SYMBOL_GPL(edac_subsys);

struct bus_type *edac_get_sysfs_subsys(void)
{
	int err = 0;

	if (atomic_read(&edac_subsys_valid))
		goto out;

	
	err = subsys_system_register(&edac_subsys, NULL);
	if (err) {
		printk(KERN_ERR "Error registering toplevel EDAC sysfs dir\n");
		return NULL;
	}

out:
	atomic_inc(&edac_subsys_valid);
	return &edac_subsys;
}
EXPORT_SYMBOL_GPL(edac_get_sysfs_subsys);

void edac_put_sysfs_subsys(void)
{
	
	if (atomic_dec_and_test(&edac_subsys_valid))
		bus_unregister(&edac_subsys);
}
EXPORT_SYMBOL_GPL(edac_put_sysfs_subsys);
