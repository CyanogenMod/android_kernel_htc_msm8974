/*
 * edac_module.c
 *
 * (C) 2007 www.softwarebitmaker.com
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 * Author: Doug Thompson <dougthompson@xmission.com>
 *
 */
#include <linux/edac.h>

#include "edac_core.h"
#include "edac_module.h"

#define EDAC_VERSION "Ver: 2.1.0"

#ifdef CONFIG_EDAC_DEBUG
int edac_debug_level = 2;
EXPORT_SYMBOL_GPL(edac_debug_level);
#endif

struct workqueue_struct *edac_workqueue;

char *edac_op_state_to_string(int opstate)
{
	if (opstate == OP_RUNNING_POLL)
		return "POLLED";
	else if (opstate == OP_RUNNING_INTERRUPT)
		return "INTERRUPT";
	else if (opstate == OP_RUNNING_POLL_INTR)
		return "POLL-INTR";
	else if (opstate == OP_ALLOC)
		return "ALLOC";
	else if (opstate == OP_OFFLINE)
		return "OFFLINE";

	return "UNKNOWN";
}

static int edac_workqueue_setup(void)
{
	edac_workqueue = create_singlethread_workqueue("edac-poller");
	if (edac_workqueue == NULL)
		return -ENODEV;
	else
		return 0;
}

static void edac_workqueue_teardown(void)
{
	if (edac_workqueue) {
		flush_workqueue(edac_workqueue);
		destroy_workqueue(edac_workqueue);
		edac_workqueue = NULL;
	}
}

static int __init edac_init(void)
{
	int err = 0;

	edac_printk(KERN_INFO, EDAC_MC, EDAC_VERSION "\n");

	edac_pci_clear_parity_errors();

	err = edac_sysfs_setup_mc_kset();
	if (err)
		goto error;

	
	err = edac_workqueue_setup();
	if (err) {
		edac_printk(KERN_ERR, EDAC_MC, "init WorkQueue failure\n");
		goto workq_fail;
	}

	return 0;

	
workq_fail:
	edac_sysfs_teardown_mc_kset();

error:
	return err;
}

static void __exit edac_exit(void)
{
	debugf0("%s()\n", __func__);

	
	edac_workqueue_teardown();
	edac_sysfs_teardown_mc_kset();
}

module_init(edac_init);
module_exit(edac_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Doug Thompson www.softwarebitmaker.com, et al");
MODULE_DESCRIPTION("Core library routines for EDAC reporting");


#ifdef CONFIG_EDAC_DEBUG
module_param(edac_debug_level, int, 0644);
MODULE_PARM_DESC(edac_debug_level, "Debug level");
#endif
