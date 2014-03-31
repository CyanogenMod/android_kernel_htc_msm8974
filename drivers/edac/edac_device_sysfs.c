/*
 * file for managing the edac_device subsystem of devices for EDAC
 *
 * (C) 2007 SoftwareBitMaker 
 *
 * This file may be distributed under the terms of the
 * GNU General Public License.
 *
 * Written Doug Thompson <norsk5@xmission.com>
 *
 */

#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/edac.h>

#include "edac_core.h"
#include "edac_module.h"

#define EDAC_DEVICE_SYMLINK	"device"

#define to_edacdev(k) container_of(k, struct edac_device_ctl_info, kobj)
#define to_edacdev_attr(a) container_of(a, struct edacdev_attribute, attr)



static ssize_t edac_device_ctl_log_ue_show(struct edac_device_ctl_info
					*ctl_info, char *data)
{
	return sprintf(data, "%u\n", ctl_info->log_ue);
}

static ssize_t edac_device_ctl_log_ue_store(struct edac_device_ctl_info
					*ctl_info, const char *data,
					size_t count)
{
	
	ctl_info->log_ue = (simple_strtoul(data, NULL, 0) != 0);

	return count;
}

static ssize_t edac_device_ctl_log_ce_show(struct edac_device_ctl_info
					*ctl_info, char *data)
{
	return sprintf(data, "%u\n", ctl_info->log_ce);
}

static ssize_t edac_device_ctl_log_ce_store(struct edac_device_ctl_info
					*ctl_info, const char *data,
					size_t count)
{
	
	ctl_info->log_ce = (simple_strtoul(data, NULL, 0) != 0);

	return count;
}

static ssize_t edac_device_ctl_panic_on_ue_show(struct edac_device_ctl_info
						*ctl_info, char *data)
{
	return sprintf(data, "%u\n", ctl_info->panic_on_ue);
}

static ssize_t edac_device_ctl_panic_on_ue_store(struct edac_device_ctl_info
						 *ctl_info, const char *data,
						 size_t count)
{
	
	ctl_info->panic_on_ue = (simple_strtoul(data, NULL, 0) != 0);

	return count;
}

static ssize_t edac_device_ctl_poll_msec_show(struct edac_device_ctl_info
					*ctl_info, char *data)
{
	return sprintf(data, "%u\n", ctl_info->poll_msec);
}

static ssize_t edac_device_ctl_poll_msec_store(struct edac_device_ctl_info
					*ctl_info, const char *data,
					size_t count)
{
	unsigned long value;

	value = simple_strtoul(data, NULL, 0);
	edac_device_reset_delay_period(ctl_info, value);

	return count;
}

struct ctl_info_attribute {
	struct attribute attr;
	ssize_t(*show) (struct edac_device_ctl_info *, char *);
	ssize_t(*store) (struct edac_device_ctl_info *, const char *, size_t);
};

#define to_ctl_info(k) container_of(k, struct edac_device_ctl_info, kobj)
#define to_ctl_info_attr(a) container_of(a,struct ctl_info_attribute,attr)

static ssize_t edac_dev_ctl_info_show(struct kobject *kobj,
				struct attribute *attr, char *buffer)
{
	struct edac_device_ctl_info *edac_dev = to_ctl_info(kobj);
	struct ctl_info_attribute *ctl_info_attr = to_ctl_info_attr(attr);

	if (ctl_info_attr->show)
		return ctl_info_attr->show(edac_dev, buffer);
	return -EIO;
}

static ssize_t edac_dev_ctl_info_store(struct kobject *kobj,
				struct attribute *attr,
				const char *buffer, size_t count)
{
	struct edac_device_ctl_info *edac_dev = to_ctl_info(kobj);
	struct ctl_info_attribute *ctl_info_attr = to_ctl_info_attr(attr);

	if (ctl_info_attr->store)
		return ctl_info_attr->store(edac_dev, buffer, count);
	return -EIO;
}

static const struct sysfs_ops device_ctl_info_ops = {
	.show = edac_dev_ctl_info_show,
	.store = edac_dev_ctl_info_store
};

#define CTL_INFO_ATTR(_name,_mode,_show,_store)        \
static struct ctl_info_attribute attr_ctl_info_##_name = {      \
	.attr = {.name = __stringify(_name), .mode = _mode },   \
	.show   = _show,                                        \
	.store  = _store,                                       \
};

CTL_INFO_ATTR(log_ue, S_IRUGO | S_IWUSR,
	edac_device_ctl_log_ue_show, edac_device_ctl_log_ue_store);
CTL_INFO_ATTR(log_ce, S_IRUGO | S_IWUSR,
	edac_device_ctl_log_ce_show, edac_device_ctl_log_ce_store);
CTL_INFO_ATTR(panic_on_ue, S_IRUGO | S_IWUSR,
	edac_device_ctl_panic_on_ue_show,
	edac_device_ctl_panic_on_ue_store);
CTL_INFO_ATTR(poll_msec, S_IRUGO | S_IWUSR,
	edac_device_ctl_poll_msec_show, edac_device_ctl_poll_msec_store);

static struct ctl_info_attribute *device_ctrl_attr[] = {
	&attr_ctl_info_panic_on_ue,
	&attr_ctl_info_log_ue,
	&attr_ctl_info_log_ce,
	&attr_ctl_info_poll_msec,
	NULL,
};

static void edac_device_ctrl_master_release(struct kobject *kobj)
{
	struct edac_device_ctl_info *edac_dev = to_edacdev(kobj);

	debugf4("%s() control index=%d\n", __func__, edac_dev->dev_idx);

	
	module_put(edac_dev->owner);

	kfree(edac_dev);
}

static struct kobj_type ktype_device_ctrl = {
	.release = edac_device_ctrl_master_release,
	.sysfs_ops = &device_ctl_info_ops,
	.default_attrs = (struct attribute **)device_ctrl_attr,
};

int edac_device_register_sysfs_main_kobj(struct edac_device_ctl_info *edac_dev)
{
	struct bus_type *edac_subsys;
	int err;

	debugf1("%s()\n", __func__);

	
	edac_subsys = edac_get_sysfs_subsys();
	if (edac_subsys == NULL) {
		debugf1("%s() no edac_subsys error\n", __func__);
		err = -ENODEV;
		goto err_out;
	}

	
	edac_dev->edac_subsys = edac_subsys;

	
	memset(&edac_dev->kobj, 0, sizeof(struct kobject));

	edac_dev->owner = THIS_MODULE;

	if (!try_module_get(edac_dev->owner)) {
		err = -ENODEV;
		goto err_mod_get;
	}

	
	err = kobject_init_and_add(&edac_dev->kobj, &ktype_device_ctrl,
				   &edac_subsys->dev_root->kobj,
				   "%s", edac_dev->name);
	if (err) {
		debugf1("%s()Failed to register '.../edac/%s'\n",
			__func__, edac_dev->name);
		goto err_kobj_reg;
	}
	kobject_uevent(&edac_dev->kobj, KOBJ_ADD);


	debugf4("%s() Registered '.../edac/%s' kobject\n",
		__func__, edac_dev->name);

	return 0;

	
err_kobj_reg:
	module_put(edac_dev->owner);

err_mod_get:
	edac_put_sysfs_subsys();

err_out:
	return err;
}

void edac_device_unregister_sysfs_main_kobj(struct edac_device_ctl_info *dev)
{
	debugf0("%s()\n", __func__);
	debugf4("%s() name of kobject is: %s\n",
		__func__, kobject_name(&dev->kobj));

	kobject_put(&dev->kobj);
	edac_put_sysfs_subsys();
}


static ssize_t instance_ue_count_show(struct edac_device_instance *instance,
				char *data)
{
	return sprintf(data, "%u\n", instance->counters.ue_count);
}

static ssize_t instance_ce_count_show(struct edac_device_instance *instance,
				char *data)
{
	return sprintf(data, "%u\n", instance->counters.ce_count);
}

#define to_instance(k) container_of(k, struct edac_device_instance, kobj)
#define to_instance_attr(a) container_of(a,struct instance_attribute,attr)

static void edac_device_ctrl_instance_release(struct kobject *kobj)
{
	struct edac_device_instance *instance;

	debugf1("%s()\n", __func__);

	instance = to_instance(kobj);
	kobject_put(&instance->ctl->kobj);
}

struct instance_attribute {
	struct attribute attr;
	ssize_t(*show) (struct edac_device_instance *, char *);
	ssize_t(*store) (struct edac_device_instance *, const char *, size_t);
};

static ssize_t edac_dev_instance_show(struct kobject *kobj,
				struct attribute *attr, char *buffer)
{
	struct edac_device_instance *instance = to_instance(kobj);
	struct instance_attribute *instance_attr = to_instance_attr(attr);

	if (instance_attr->show)
		return instance_attr->show(instance, buffer);
	return -EIO;
}

static ssize_t edac_dev_instance_store(struct kobject *kobj,
				struct attribute *attr,
				const char *buffer, size_t count)
{
	struct edac_device_instance *instance = to_instance(kobj);
	struct instance_attribute *instance_attr = to_instance_attr(attr);

	if (instance_attr->store)
		return instance_attr->store(instance, buffer, count);
	return -EIO;
}

static const struct sysfs_ops device_instance_ops = {
	.show = edac_dev_instance_show,
	.store = edac_dev_instance_store
};

#define INSTANCE_ATTR(_name,_mode,_show,_store)        \
static struct instance_attribute attr_instance_##_name = {      \
	.attr = {.name = __stringify(_name), .mode = _mode },   \
	.show   = _show,                                        \
	.store  = _store,                                       \
};

INSTANCE_ATTR(ce_count, S_IRUGO, instance_ce_count_show, NULL);
INSTANCE_ATTR(ue_count, S_IRUGO, instance_ue_count_show, NULL);

static struct instance_attribute *device_instance_attr[] = {
	&attr_instance_ce_count,
	&attr_instance_ue_count,
	NULL,
};

static struct kobj_type ktype_instance_ctrl = {
	.release = edac_device_ctrl_instance_release,
	.sysfs_ops = &device_instance_ops,
	.default_attrs = (struct attribute **)device_instance_attr,
};


#define to_block(k) container_of(k, struct edac_device_block, kobj)
#define to_block_attr(a) \
	container_of(a, struct edac_dev_sysfs_block_attribute, attr)

static ssize_t block_ue_count_show(struct kobject *kobj,
					struct attribute *attr, char *data)
{
	struct edac_device_block *block = to_block(kobj);

	return sprintf(data, "%u\n", block->counters.ue_count);
}

static ssize_t block_ce_count_show(struct kobject *kobj,
					struct attribute *attr, char *data)
{
	struct edac_device_block *block = to_block(kobj);

	return sprintf(data, "%u\n", block->counters.ce_count);
}

static void edac_device_ctrl_block_release(struct kobject *kobj)
{
	struct edac_device_block *block;

	debugf1("%s()\n", __func__);

	
	block = to_block(kobj);

	kobject_put(&block->instance->ctl->kobj);
}


static ssize_t edac_dev_block_show(struct kobject *kobj,
				struct attribute *attr, char *buffer)
{
	struct edac_dev_sysfs_block_attribute *block_attr =
						to_block_attr(attr);

	if (block_attr->show)
		return block_attr->show(kobj, attr, buffer);
	return -EIO;
}

static ssize_t edac_dev_block_store(struct kobject *kobj,
				struct attribute *attr,
				const char *buffer, size_t count)
{
	struct edac_dev_sysfs_block_attribute *block_attr;

	block_attr = to_block_attr(attr);

	if (block_attr->store)
		return block_attr->store(kobj, attr, buffer, count);
	return -EIO;
}

static const struct sysfs_ops device_block_ops = {
	.show = edac_dev_block_show,
	.store = edac_dev_block_store
};

#define BLOCK_ATTR(_name,_mode,_show,_store)        \
static struct edac_dev_sysfs_block_attribute attr_block_##_name = {	\
	.attr = {.name = __stringify(_name), .mode = _mode },   \
	.show   = _show,                                        \
	.store  = _store,                                       \
};

BLOCK_ATTR(ce_count, S_IRUGO, block_ce_count_show, NULL);
BLOCK_ATTR(ue_count, S_IRUGO, block_ue_count_show, NULL);

static struct edac_dev_sysfs_block_attribute *device_block_attr[] = {
	&attr_block_ce_count,
	&attr_block_ue_count,
	NULL,
};

static struct kobj_type ktype_block_ctrl = {
	.release = edac_device_ctrl_block_release,
	.sysfs_ops = &device_block_ops,
	.default_attrs = (struct attribute **)device_block_attr,
};


static int edac_device_create_block(struct edac_device_ctl_info *edac_dev,
				struct edac_device_instance *instance,
				struct edac_device_block *block)
{
	int i;
	int err;
	struct edac_dev_sysfs_block_attribute *sysfs_attrib;
	struct kobject *main_kobj;

	debugf4("%s() Instance '%s' inst_p=%p  block '%s'  block_p=%p\n",
		__func__, instance->name, instance, block->name, block);
	debugf4("%s() block kobj=%p  block kobj->parent=%p\n",
		__func__, &block->kobj, &block->kobj.parent);

	
	memset(&block->kobj, 0, sizeof(struct kobject));

	main_kobj = kobject_get(&edac_dev->kobj);
	if (!main_kobj) {
		err = -ENODEV;
		goto err_out;
	}

	
	err = kobject_init_and_add(&block->kobj, &ktype_block_ctrl,
				   &instance->kobj,
				   "%s", block->name);
	if (err) {
		debugf1("%s() Failed to register instance '%s'\n",
			__func__, block->name);
		kobject_put(main_kobj);
		err = -ENODEV;
		goto err_out;
	}

	sysfs_attrib = block->block_attributes;
	if (sysfs_attrib && block->nr_attribs) {
		for (i = 0; i < block->nr_attribs; i++, sysfs_attrib++) {

			debugf4("%s() creating block attrib='%s' "
				"attrib->%p to kobj=%p\n",
				__func__,
				sysfs_attrib->attr.name,
				sysfs_attrib, &block->kobj);

			
			err = sysfs_create_file(&block->kobj,
				&sysfs_attrib->attr);
			if (err)
				goto err_on_attrib;
		}
	}
	kobject_uevent(&block->kobj, KOBJ_ADD);

	return 0;

	
err_on_attrib:
	kobject_put(&block->kobj);

err_out:
	return err;
}

static void edac_device_delete_block(struct edac_device_ctl_info *edac_dev,
				struct edac_device_block *block)
{
	struct edac_dev_sysfs_block_attribute *sysfs_attrib;
	int i;

	sysfs_attrib = block->block_attributes;
	if (sysfs_attrib && block->nr_attribs) {
		for (i = 0; i < block->nr_attribs; i++, sysfs_attrib++) {

			
			sysfs_remove_file(&block->kobj,
				(struct attribute *) sysfs_attrib);
		}
	}

	kobject_put(&block->kobj);
}


static int edac_device_create_instance(struct edac_device_ctl_info *edac_dev,
				int idx)
{
	int i, j;
	int err;
	struct edac_device_instance *instance;
	struct kobject *main_kobj;

	instance = &edac_dev->instances[idx];

	
	memset(&instance->kobj, 0, sizeof(struct kobject));

	instance->ctl = edac_dev;

	main_kobj = kobject_get(&edac_dev->kobj);
	if (!main_kobj) {
		err = -ENODEV;
		goto err_out;
	}

	
	err = kobject_init_and_add(&instance->kobj, &ktype_instance_ctrl,
				   &edac_dev->kobj, "%s", instance->name);
	if (err != 0) {
		debugf2("%s() Failed to register instance '%s'\n",
			__func__, instance->name);
		kobject_put(main_kobj);
		goto err_out;
	}

	debugf4("%s() now register '%d' blocks for instance %d\n",
		__func__, instance->nr_blocks, idx);

	
	for (i = 0; i < instance->nr_blocks; i++) {
		err = edac_device_create_block(edac_dev, instance,
						&instance->blocks[i]);
		if (err) {
			
			for (j = 0; j < i; j++)
				edac_device_delete_block(edac_dev,
							&instance->blocks[j]);
			goto err_release_instance_kobj;
		}
	}
	kobject_uevent(&instance->kobj, KOBJ_ADD);

	debugf4("%s() Registered instance %d '%s' kobject\n",
		__func__, idx, instance->name);

	return 0;

	
err_release_instance_kobj:
	kobject_put(&instance->kobj);

err_out:
	return err;
}

static void edac_device_delete_instance(struct edac_device_ctl_info *edac_dev,
					int idx)
{
	struct edac_device_instance *instance;
	int i;

	instance = &edac_dev->instances[idx];

	
	for (i = 0; i < instance->nr_blocks; i++)
		edac_device_delete_block(edac_dev, &instance->blocks[i]);

	kobject_put(&instance->kobj);
}

static int edac_device_create_instances(struct edac_device_ctl_info *edac_dev)
{
	int i, j;
	int err;

	debugf0("%s()\n", __func__);

	
	for (i = 0; i < edac_dev->nr_instances; i++) {
		err = edac_device_create_instance(edac_dev, i);
		if (err) {
			
			for (j = 0; j < i; j++)
				edac_device_delete_instance(edac_dev, j);
			return err;
		}
	}

	return 0;
}

static void edac_device_delete_instances(struct edac_device_ctl_info *edac_dev)
{
	int i;

	
	for (i = 0; i < edac_dev->nr_instances; i++)
		edac_device_delete_instance(edac_dev, i);
}


static int edac_device_add_main_sysfs_attributes(
			struct edac_device_ctl_info *edac_dev)
{
	struct edac_dev_sysfs_attribute *sysfs_attrib;
	int err = 0;

	sysfs_attrib = edac_dev->sysfs_attributes;
	if (sysfs_attrib) {
		while (sysfs_attrib->attr.name != NULL) {
			err = sysfs_create_file(&edac_dev->kobj,
				(struct attribute*) sysfs_attrib);
			if (err)
				goto err_out;

			sysfs_attrib++;
		}
	}

err_out:
	return err;
}

static void edac_device_remove_main_sysfs_attributes(
			struct edac_device_ctl_info *edac_dev)
{
	struct edac_dev_sysfs_attribute *sysfs_attrib;

	sysfs_attrib = edac_dev->sysfs_attributes;
	if (sysfs_attrib) {
		while (sysfs_attrib->attr.name != NULL) {
			sysfs_remove_file(&edac_dev->kobj,
					(struct attribute *) sysfs_attrib);
			sysfs_attrib++;
		}
	}
}

int edac_device_create_sysfs(struct edac_device_ctl_info *edac_dev)
{
	int err;
	struct kobject *edac_kobj = &edac_dev->kobj;

	debugf0("%s() idx=%d\n", __func__, edac_dev->dev_idx);

	
	err = edac_device_add_main_sysfs_attributes(edac_dev);
	if (err) {
		debugf0("%s() failed to add sysfs attribs\n", __func__);
		goto err_out;
	}

	err = sysfs_create_link(edac_kobj,
				&edac_dev->dev->kobj, EDAC_DEVICE_SYMLINK);
	if (err) {
		debugf0("%s() sysfs_create_link() returned err= %d\n",
			__func__, err);
		goto err_remove_main_attribs;
	}

	err = edac_device_create_instances(edac_dev);
	if (err) {
		debugf0("%s() edac_device_create_instances() "
			"returned err= %d\n", __func__, err);
		goto err_remove_link;
	}


	debugf4("%s() create-instances done, idx=%d\n",
		__func__, edac_dev->dev_idx);

	return 0;

	
err_remove_link:
	
	sysfs_remove_link(&edac_dev->kobj, EDAC_DEVICE_SYMLINK);

err_remove_main_attribs:
	edac_device_remove_main_sysfs_attributes(edac_dev);

err_out:
	return err;
}

void edac_device_remove_sysfs(struct edac_device_ctl_info *edac_dev)
{
	debugf0("%s()\n", __func__);

	
	edac_device_remove_main_sysfs_attributes(edac_dev);

	
	sysfs_remove_link(&edac_dev->kobj, EDAC_DEVICE_SYMLINK);

	
	edac_device_delete_instances(edac_dev);
}
