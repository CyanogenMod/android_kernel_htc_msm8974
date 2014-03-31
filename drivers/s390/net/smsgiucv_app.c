/*
 * Deliver z/VM CP special messages (SMSG) as uevents.
 *
 * The driver registers for z/VM CP special messages with the
 * "APP" prefix. Incoming messages are delivered to user space
 * as uevents.
 *
 * Copyright IBM Corp. 2010
 * Author(s): Hendrik Brueckner <brueckner@linux.vnet.ibm.com>
 *
 */
#define KMSG_COMPONENT		"smsgiucv_app"
#define pr_fmt(fmt)		KMSG_COMPONENT ": " fmt

#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <net/iucv/iucv.h>
#include "smsgiucv.h"

#define SMSG_PREFIX		"APP"

#define ENV_SENDER_STR		"SMSG_SENDER="
#define ENV_SENDER_LEN		(strlen(ENV_SENDER_STR) + 8 + 1)
#define ENV_PREFIX_STR		"SMSG_ID="
#define ENV_PREFIX_LEN		(strlen(ENV_PREFIX_STR) + \
				 strlen(SMSG_PREFIX) + 1)
#define ENV_TEXT_STR		"SMSG_TEXT="
#define ENV_TEXT_LEN(msg)	(strlen(ENV_TEXT_STR) + strlen((msg)) + 1)

static char *sender;
module_param(sender, charp, 0400);
MODULE_PARM_DESC(sender, "z/VM user ID from which CP SMSGs are accepted");

static struct device *smsg_app_dev;

struct smsg_app_event {
	struct list_head list;
	char *buf;
	char *envp[4];
};

static LIST_HEAD(smsg_event_queue);
static DEFINE_SPINLOCK(smsg_event_queue_lock);

static void smsg_app_event_free(struct smsg_app_event *ev)
{
	kfree(ev->buf);
	kfree(ev);
}

static struct smsg_app_event *smsg_app_event_alloc(const char *from,
						   const char *msg)
{
	struct smsg_app_event *ev;

	ev = kzalloc(sizeof(*ev), GFP_ATOMIC);
	if (!ev)
		return NULL;

	ev->buf = kzalloc(ENV_SENDER_LEN + ENV_PREFIX_LEN +
			  ENV_TEXT_LEN(msg), GFP_ATOMIC);
	if (!ev->buf) {
		kfree(ev);
		return NULL;
	}

	
	ev->envp[0] = ev->buf;
	ev->envp[1] = ev->envp[0] + ENV_SENDER_LEN;
	ev->envp[2] = ev->envp[1] + ENV_PREFIX_LEN;
	ev->envp[3] = NULL;

	
	snprintf(ev->envp[0], ENV_SENDER_LEN, ENV_SENDER_STR "%s", from);
	snprintf(ev->envp[1], ENV_PREFIX_LEN, ENV_PREFIX_STR "%s", SMSG_PREFIX);
	snprintf(ev->envp[2], ENV_TEXT_LEN(msg), ENV_TEXT_STR "%s", msg);

	return ev;
}

static void smsg_event_work_fn(struct work_struct *work)
{
	LIST_HEAD(event_queue);
	struct smsg_app_event *p, *n;
	struct device *dev;

	dev = get_device(smsg_app_dev);
	if (!dev)
		return;

	spin_lock_bh(&smsg_event_queue_lock);
	list_splice_init(&smsg_event_queue, &event_queue);
	spin_unlock_bh(&smsg_event_queue_lock);

	list_for_each_entry_safe(p, n, &event_queue, list) {
		list_del(&p->list);
		kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, p->envp);
		smsg_app_event_free(p);
	}

	put_device(dev);
}
static DECLARE_WORK(smsg_event_work, smsg_event_work_fn);

static void smsg_app_callback(const char *from, char *msg)
{
	struct smsg_app_event *se;

	if (sender && strlen(sender) > 0 && strcmp(from, sender) != 0)
		return;

	
	msg += strlen(SMSG_PREFIX);
	while (*msg && isspace(*msg))
		msg++;
	if (*msg == '\0')
		return;

	
	se = smsg_app_event_alloc(from, msg);
	if (!se)
		return;

	
	spin_lock(&smsg_event_queue_lock);
	list_add_tail(&se->list, &smsg_event_queue);
	spin_unlock(&smsg_event_queue_lock);

	schedule_work(&smsg_event_work);
	return;
}

static int __init smsgiucv_app_init(void)
{
	struct device_driver *smsgiucv_drv;
	int rc;

	if (!MACHINE_IS_VM)
		return -ENODEV;

	smsg_app_dev = kzalloc(sizeof(*smsg_app_dev), GFP_KERNEL);
	if (!smsg_app_dev)
		return -ENOMEM;

	smsgiucv_drv = driver_find(SMSGIUCV_DRV_NAME, &iucv_bus);
	if (!smsgiucv_drv) {
		kfree(smsg_app_dev);
		return -ENODEV;
	}

	rc = dev_set_name(smsg_app_dev, KMSG_COMPONENT);
	if (rc) {
		kfree(smsg_app_dev);
		goto fail;
	}
	smsg_app_dev->bus = &iucv_bus;
	smsg_app_dev->parent = iucv_root;
	smsg_app_dev->release = (void (*)(struct device *)) kfree;
	smsg_app_dev->driver = smsgiucv_drv;
	rc = device_register(smsg_app_dev);
	if (rc) {
		put_device(smsg_app_dev);
		goto fail;
	}

	
	if (sender) {
		int len = strlen(sender);
		while (len--)
			sender[len] = toupper(sender[len]);
	}

	
	rc = smsg_register_callback(SMSG_PREFIX, smsg_app_callback);
	if (rc) {
		device_unregister(smsg_app_dev);
		goto fail;
	}

	rc = 0;
fail:
	return rc;
}
module_init(smsgiucv_app_init);

static void __exit smsgiucv_app_exit(void)
{
	
	smsg_unregister_callback(SMSG_PREFIX, smsg_app_callback);

	
	cancel_work_sync(&smsg_event_work);
	smsg_event_work_fn(&smsg_event_work);

	device_unregister(smsg_app_dev);
}
module_exit(smsgiucv_app_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Deliver z/VM CP SMSG as uevents");
MODULE_AUTHOR("Hendrik Brueckner <brueckner@linux.vnet.ibm.com>");
