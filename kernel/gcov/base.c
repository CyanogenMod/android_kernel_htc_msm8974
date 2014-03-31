/*
 *  This code maintains a list of active profiling data structures.
 *
 *    Copyright IBM Corp. 2009
 *    Author(s): Peter Oberparleiter <oberpar@linux.vnet.ibm.com>
 *
 *    Uses gcc-internal data definitions.
 *    Based on the gcov-kernel patch by:
 *		 Hubertus Franke <frankeh@us.ibm.com>
 *		 Nigel Hinds <nhinds@us.ibm.com>
 *		 Rajan Ravindran <rajancr@us.ibm.com>
 *		 Peter Oberparleiter <oberpar@linux.vnet.ibm.com>
 *		 Paul Larson
 */

#define pr_fmt(fmt)	"gcov: " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include "gcov.h"

static struct gcov_info *gcov_info_head;
static int gcov_events_enabled;
static DEFINE_MUTEX(gcov_lock);

void __gcov_init(struct gcov_info *info)
{
	static unsigned int gcov_version;

	mutex_lock(&gcov_lock);
	if (gcov_version == 0) {
		gcov_version = info->version;
		pr_info("version magic: 0x%x\n", gcov_version);
	}
	info->next = gcov_info_head;
	gcov_info_head = info;
	if (gcov_events_enabled)
		gcov_event(GCOV_ADD, info);
	mutex_unlock(&gcov_lock);
}
EXPORT_SYMBOL(__gcov_init);

void __gcov_flush(void)
{
	
}
EXPORT_SYMBOL(__gcov_flush);

void __gcov_merge_add(gcov_type *counters, unsigned int n_counters)
{
	
}
EXPORT_SYMBOL(__gcov_merge_add);

void __gcov_merge_single(gcov_type *counters, unsigned int n_counters)
{
	
}
EXPORT_SYMBOL(__gcov_merge_single);

void __gcov_merge_delta(gcov_type *counters, unsigned int n_counters)
{
	
}
EXPORT_SYMBOL(__gcov_merge_delta);

void gcov_enable_events(void)
{
	struct gcov_info *info;

	mutex_lock(&gcov_lock);
	gcov_events_enabled = 1;
	
	for (info = gcov_info_head; info; info = info->next)
		gcov_event(GCOV_ADD, info);
	mutex_unlock(&gcov_lock);
}

#ifdef CONFIG_MODULES
static inline int within(void *addr, void *start, unsigned long size)
{
	return ((addr >= start) && (addr < start + size));
}

static int gcov_module_notifier(struct notifier_block *nb, unsigned long event,
				void *data)
{
	struct module *mod = data;
	struct gcov_info *info;
	struct gcov_info *prev;

	if (event != MODULE_STATE_GOING)
		return NOTIFY_OK;
	mutex_lock(&gcov_lock);
	prev = NULL;
	
	for (info = gcov_info_head; info; info = info->next) {
		if (within(info, mod->module_core, mod->core_size)) {
			if (prev)
				prev->next = info->next;
			else
				gcov_info_head = info->next;
			if (gcov_events_enabled)
				gcov_event(GCOV_REMOVE, info);
		} else
			prev = info;
	}
	mutex_unlock(&gcov_lock);

	return NOTIFY_OK;
}

static struct notifier_block gcov_nb = {
	.notifier_call	= gcov_module_notifier,
};

static int __init gcov_init(void)
{
	return register_module_notifier(&gcov_nb);
}
device_initcall(gcov_init);
#endif 
