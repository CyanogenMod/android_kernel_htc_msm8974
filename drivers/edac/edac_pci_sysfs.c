/*
 * (C) 2005, 2006 Linux Networx (http://lnxi.com)
 * This file may be distributed under the terms of the
 * GNU General Public License.
 *
 * Written Doug Thompson <norsk5@xmission.com>
 *
 */
#include <linux/module.h>
#include <linux/edac.h>
#include <linux/slab.h>
#include <linux/ctype.h>

#include "edac_core.h"
#include "edac_module.h"

#ifdef CONFIG_PCI

#define EDAC_PCI_SYMLINK	"device"

static int check_pci_errors;		
static int edac_pci_panic_on_pe;	
static int edac_pci_log_pe = 1;		
static int edac_pci_log_npe = 1;	
static int edac_pci_poll_msec = 1000;	

static atomic_t pci_parity_count = ATOMIC_INIT(0);
static atomic_t pci_nonparity_count = ATOMIC_INIT(0);

static struct kobject *edac_pci_top_main_kobj;
static atomic_t edac_pci_sysfs_refcount = ATOMIC_INIT(0);

int edac_pci_get_check_errors(void)
{
	return check_pci_errors;
}

static int edac_pci_get_log_pe(void)
{
	return edac_pci_log_pe;
}

static int edac_pci_get_log_npe(void)
{
	return edac_pci_log_npe;
}

static int edac_pci_get_panic_on_pe(void)
{
	return edac_pci_panic_on_pe;
}

int edac_pci_get_poll_msec(void)
{
	return edac_pci_poll_msec;
}

static ssize_t instance_pe_count_show(struct edac_pci_ctl_info *pci, char *data)
{
	return sprintf(data, "%u\n", atomic_read(&pci->counters.pe_count));
}

static ssize_t instance_npe_count_show(struct edac_pci_ctl_info *pci,
				char *data)
{
	return sprintf(data, "%u\n", atomic_read(&pci->counters.npe_count));
}

#define to_instance(k) container_of(k, struct edac_pci_ctl_info, kobj)
#define to_instance_attr(a) container_of(a, struct instance_attribute, attr)

static void edac_pci_instance_release(struct kobject *kobj)
{
	struct edac_pci_ctl_info *pci;

	debugf0("%s()\n", __func__);

	
	pci = to_instance(kobj);

	
	kobject_put(edac_pci_top_main_kobj);

	kfree(pci);	
}

struct instance_attribute {
	struct attribute attr;
	ssize_t(*show) (struct edac_pci_ctl_info *, char *);
	ssize_t(*store) (struct edac_pci_ctl_info *, const char *, size_t);
};

static ssize_t edac_pci_instance_show(struct kobject *kobj,
				struct attribute *attr, char *buffer)
{
	struct edac_pci_ctl_info *pci = to_instance(kobj);
	struct instance_attribute *instance_attr = to_instance_attr(attr);

	if (instance_attr->show)
		return instance_attr->show(pci, buffer);
	return -EIO;
}

static ssize_t edac_pci_instance_store(struct kobject *kobj,
				struct attribute *attr,
				const char *buffer, size_t count)
{
	struct edac_pci_ctl_info *pci = to_instance(kobj);
	struct instance_attribute *instance_attr = to_instance_attr(attr);

	if (instance_attr->store)
		return instance_attr->store(pci, buffer, count);
	return -EIO;
}

static const struct sysfs_ops pci_instance_ops = {
	.show = edac_pci_instance_show,
	.store = edac_pci_instance_store
};

#define INSTANCE_ATTR(_name, _mode, _show, _store)	\
static struct instance_attribute attr_instance_##_name = {	\
	.attr	= {.name = __stringify(_name), .mode = _mode },	\
	.show	= _show,					\
	.store	= _store,					\
};

INSTANCE_ATTR(pe_count, S_IRUGO, instance_pe_count_show, NULL);
INSTANCE_ATTR(npe_count, S_IRUGO, instance_npe_count_show, NULL);

static struct instance_attribute *pci_instance_attr[] = {
	&attr_instance_pe_count,
	&attr_instance_npe_count,
	NULL
};

static struct kobj_type ktype_pci_instance = {
	.release = edac_pci_instance_release,
	.sysfs_ops = &pci_instance_ops,
	.default_attrs = (struct attribute **)pci_instance_attr,
};

static int edac_pci_create_instance_kobj(struct edac_pci_ctl_info *pci, int idx)
{
	struct kobject *main_kobj;
	int err;

	debugf0("%s()\n", __func__);

	main_kobj = kobject_get(edac_pci_top_main_kobj);
	if (!main_kobj) {
		err = -ENODEV;
		goto error_out;
	}

	
	err = kobject_init_and_add(&pci->kobj, &ktype_pci_instance,
				   edac_pci_top_main_kobj, "pci%d", idx);
	if (err != 0) {
		debugf2("%s() failed to register instance pci%d\n",
			__func__, idx);
		kobject_put(edac_pci_top_main_kobj);
		goto error_out;
	}

	kobject_uevent(&pci->kobj, KOBJ_ADD);
	debugf1("%s() Register instance 'pci%d' kobject\n", __func__, idx);

	return 0;

	
error_out:
	return err;
}

static void edac_pci_unregister_sysfs_instance_kobj(
			struct edac_pci_ctl_info *pci)
{
	debugf0("%s()\n", __func__);

	kobject_put(&pci->kobj);
}

#define to_edacpci(k) container_of(k, struct edac_pci_ctl_info, kobj)
#define to_edacpci_attr(a) container_of(a, struct edac_pci_attr, attr)

static ssize_t edac_pci_int_show(void *ptr, char *buffer)
{
	int *value = ptr;
	return sprintf(buffer, "%d\n", *value);
}

static ssize_t edac_pci_int_store(void *ptr, const char *buffer, size_t count)
{
	int *value = ptr;

	if (isdigit(*buffer))
		*value = simple_strtoul(buffer, NULL, 0);

	return count;
}

struct edac_pci_dev_attribute {
	struct attribute attr;
	void *value;
	 ssize_t(*show) (void *, char *);
	 ssize_t(*store) (void *, const char *, size_t);
};

static ssize_t edac_pci_dev_show(struct kobject *kobj, struct attribute *attr,
				 char *buffer)
{
	struct edac_pci_dev_attribute *edac_pci_dev;
	edac_pci_dev = (struct edac_pci_dev_attribute *)attr;

	if (edac_pci_dev->show)
		return edac_pci_dev->show(edac_pci_dev->value, buffer);
	return -EIO;
}

static ssize_t edac_pci_dev_store(struct kobject *kobj,
				struct attribute *attr, const char *buffer,
				size_t count)
{
	struct edac_pci_dev_attribute *edac_pci_dev;
	edac_pci_dev = (struct edac_pci_dev_attribute *)attr;

	if (edac_pci_dev->show)
		return edac_pci_dev->store(edac_pci_dev->value, buffer, count);
	return -EIO;
}

static const struct sysfs_ops edac_pci_sysfs_ops = {
	.show = edac_pci_dev_show,
	.store = edac_pci_dev_store
};

#define EDAC_PCI_ATTR(_name,_mode,_show,_store)			\
static struct edac_pci_dev_attribute edac_pci_attr_##_name = {		\
	.attr = {.name = __stringify(_name), .mode = _mode },	\
	.value  = &_name,					\
	.show   = _show,					\
	.store  = _store,					\
};

#define EDAC_PCI_STRING_ATTR(_name,_data,_mode,_show,_store)	\
static struct edac_pci_dev_attribute edac_pci_attr_##_name = {		\
	.attr = {.name = __stringify(_name), .mode = _mode },	\
	.value  = _data,					\
	.show   = _show,					\
	.store  = _store,					\
};

EDAC_PCI_ATTR(check_pci_errors, S_IRUGO | S_IWUSR, edac_pci_int_show,
	edac_pci_int_store);
EDAC_PCI_ATTR(edac_pci_log_pe, S_IRUGO | S_IWUSR, edac_pci_int_show,
	edac_pci_int_store);
EDAC_PCI_ATTR(edac_pci_log_npe, S_IRUGO | S_IWUSR, edac_pci_int_show,
	edac_pci_int_store);
EDAC_PCI_ATTR(edac_pci_panic_on_pe, S_IRUGO | S_IWUSR, edac_pci_int_show,
	edac_pci_int_store);
EDAC_PCI_ATTR(pci_parity_count, S_IRUGO, edac_pci_int_show, NULL);
EDAC_PCI_ATTR(pci_nonparity_count, S_IRUGO, edac_pci_int_show, NULL);

static struct edac_pci_dev_attribute *edac_pci_attr[] = {
	&edac_pci_attr_check_pci_errors,
	&edac_pci_attr_edac_pci_log_pe,
	&edac_pci_attr_edac_pci_log_npe,
	&edac_pci_attr_edac_pci_panic_on_pe,
	&edac_pci_attr_pci_parity_count,
	&edac_pci_attr_pci_nonparity_count,
	NULL,
};

static void edac_pci_release_main_kobj(struct kobject *kobj)
{
	debugf0("%s() here to module_put(THIS_MODULE)\n", __func__);

	kfree(kobj);

	module_put(THIS_MODULE);
}

static struct kobj_type ktype_edac_pci_main_kobj = {
	.release = edac_pci_release_main_kobj,
	.sysfs_ops = &edac_pci_sysfs_ops,
	.default_attrs = (struct attribute **)edac_pci_attr,
};

static int edac_pci_main_kobj_setup(void)
{
	int err;
	struct bus_type *edac_subsys;

	debugf0("%s()\n", __func__);

	
	if (atomic_inc_return(&edac_pci_sysfs_refcount) != 1)
		return 0;

	edac_subsys = edac_get_sysfs_subsys();
	if (edac_subsys == NULL) {
		debugf1("%s() no edac_subsys\n", __func__);
		err = -ENODEV;
		goto decrement_count_fail;
	}

	if (!try_module_get(THIS_MODULE)) {
		debugf1("%s() try_module_get() failed\n", __func__);
		err = -ENODEV;
		goto mod_get_fail;
	}

	edac_pci_top_main_kobj = kzalloc(sizeof(struct kobject), GFP_KERNEL);
	if (!edac_pci_top_main_kobj) {
		debugf1("Failed to allocate\n");
		err = -ENOMEM;
		goto kzalloc_fail;
	}

	
	err = kobject_init_and_add(edac_pci_top_main_kobj,
				   &ktype_edac_pci_main_kobj,
				   &edac_subsys->dev_root->kobj, "pci");
	if (err) {
		debugf1("Failed to register '.../edac/pci'\n");
		goto kobject_init_and_add_fail;
	}

	kobject_uevent(edac_pci_top_main_kobj, KOBJ_ADD);
	debugf1("Registered '.../edac/pci' kobject\n");

	return 0;

	
kobject_init_and_add_fail:
	kfree(edac_pci_top_main_kobj);

kzalloc_fail:
	module_put(THIS_MODULE);

mod_get_fail:
	edac_put_sysfs_subsys();

decrement_count_fail:
	
	atomic_dec(&edac_pci_sysfs_refcount);

	return err;
}

static void edac_pci_main_kobj_teardown(void)
{
	debugf0("%s()\n", __func__);

	if (atomic_dec_return(&edac_pci_sysfs_refcount) == 0) {
		debugf0("%s() called kobject_put on main kobj\n",
			__func__);
		kobject_put(edac_pci_top_main_kobj);
	}
	edac_put_sysfs_subsys();
}

int edac_pci_create_sysfs(struct edac_pci_ctl_info *pci)
{
	int err;
	struct kobject *edac_kobj = &pci->kobj;

	debugf0("%s() idx=%d\n", __func__, pci->pci_idx);

	
	err = edac_pci_main_kobj_setup();
	if (err)
		return err;

	
	err = edac_pci_create_instance_kobj(pci, pci->pci_idx);
	if (err)
		goto unregister_cleanup;

	err = sysfs_create_link(edac_kobj, &pci->dev->kobj, EDAC_PCI_SYMLINK);
	if (err) {
		debugf0("%s() sysfs_create_link() returned err= %d\n",
			__func__, err);
		goto symlink_fail;
	}

	return 0;

	
symlink_fail:
	edac_pci_unregister_sysfs_instance_kobj(pci);

unregister_cleanup:
	edac_pci_main_kobj_teardown();

	return err;
}

void edac_pci_remove_sysfs(struct edac_pci_ctl_info *pci)
{
	debugf0("%s() index=%d\n", __func__, pci->pci_idx);

	
	sysfs_remove_link(&pci->kobj, EDAC_PCI_SYMLINK);

	
	edac_pci_unregister_sysfs_instance_kobj(pci);

	debugf0("%s() calling edac_pci_main_kobj_teardown()\n", __func__);
	edac_pci_main_kobj_teardown();
}

static u16 get_pci_parity_status(struct pci_dev *dev, int secondary)
{
	int where;
	u16 status;

	where = secondary ? PCI_SEC_STATUS : PCI_STATUS;
	pci_read_config_word(dev, where, &status);


	if (status == 0xFFFF) {
		u32 sanity;

		pci_read_config_dword(dev, 0, &sanity);

		if (sanity == 0xFFFFFFFF)
			return 0;
	}

	status &= PCI_STATUS_DETECTED_PARITY | PCI_STATUS_SIG_SYSTEM_ERROR |
		PCI_STATUS_PARITY;

	if (status)
		
		pci_write_config_word(dev, where, status);

	return status;
}


static void edac_pci_dev_parity_clear(struct pci_dev *dev)
{
	u8 header_type;

	get_pci_parity_status(dev, 0);

	
	pci_read_config_byte(dev, PCI_HEADER_TYPE, &header_type);

	if ((header_type & 0x7F) == PCI_HEADER_TYPE_BRIDGE)
		get_pci_parity_status(dev, 1);
}

static void edac_pci_dev_parity_test(struct pci_dev *dev)
{
	unsigned long flags;
	u16 status;
	u8 header_type;

	
	local_irq_save(flags);

	
	status = get_pci_parity_status(dev, 0);

	
	pci_read_config_byte(dev, PCI_HEADER_TYPE, &header_type);

	local_irq_restore(flags);

	debugf4("PCI STATUS= 0x%04x %s\n", status, dev_name(&dev->dev));

	if (status && !dev->broken_parity_status) {
		if (status & (PCI_STATUS_SIG_SYSTEM_ERROR)) {
			edac_printk(KERN_CRIT, EDAC_PCI,
				"Signaled System Error on %s\n",
				pci_name(dev));
			atomic_inc(&pci_nonparity_count);
		}

		if (status & (PCI_STATUS_PARITY)) {
			edac_printk(KERN_CRIT, EDAC_PCI,
				"Master Data Parity Error on %s\n",
				pci_name(dev));

			atomic_inc(&pci_parity_count);
		}

		if (status & (PCI_STATUS_DETECTED_PARITY)) {
			edac_printk(KERN_CRIT, EDAC_PCI,
				"Detected Parity Error on %s\n",
				pci_name(dev));

			atomic_inc(&pci_parity_count);
		}
	}


	debugf4("PCI HEADER TYPE= 0x%02x %s\n", header_type, dev_name(&dev->dev));

	if ((header_type & 0x7F) == PCI_HEADER_TYPE_BRIDGE) {
		
		status = get_pci_parity_status(dev, 1);

		debugf4("PCI SEC_STATUS= 0x%04x %s\n", status, dev_name(&dev->dev));

		if (status && !dev->broken_parity_status) {
			if (status & (PCI_STATUS_SIG_SYSTEM_ERROR)) {
				edac_printk(KERN_CRIT, EDAC_PCI, "Bridge "
					"Signaled System Error on %s\n",
					pci_name(dev));
				atomic_inc(&pci_nonparity_count);
			}

			if (status & (PCI_STATUS_PARITY)) {
				edac_printk(KERN_CRIT, EDAC_PCI, "Bridge "
					"Master Data Parity Error on "
					"%s\n", pci_name(dev));

				atomic_inc(&pci_parity_count);
			}

			if (status & (PCI_STATUS_DETECTED_PARITY)) {
				edac_printk(KERN_CRIT, EDAC_PCI, "Bridge "
					"Detected Parity Error on %s\n",
					pci_name(dev));

				atomic_inc(&pci_parity_count);
			}
		}
	}
}

typedef void (*pci_parity_check_fn_t) (struct pci_dev *dev);

static inline void edac_pci_dev_parity_iterator(pci_parity_check_fn_t fn)
{
	struct pci_dev *dev = NULL;

	while ((dev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, dev)) != NULL) {
		fn(dev);
	}
}

void edac_pci_do_parity_check(void)
{
	int before_count;

	debugf3("%s()\n", __func__);

	
	if (!check_pci_errors)
		return;

	before_count = atomic_read(&pci_parity_count);

	edac_pci_dev_parity_iterator(edac_pci_dev_parity_test);

	
	if (edac_pci_get_panic_on_pe()) {
		
		if (before_count != atomic_read(&pci_parity_count))
			panic("EDAC: PCI Parity Error");
	}
}

void edac_pci_clear_parity_errors(void)
{
	edac_pci_dev_parity_iterator(edac_pci_dev_parity_clear);
}

void edac_pci_handle_pe(struct edac_pci_ctl_info *pci, const char *msg)
{

	
	atomic_inc(&pci->counters.pe_count);

	if (edac_pci_get_log_pe())
		edac_pci_printk(pci, KERN_WARNING,
				"Parity Error ctl: %s %d: %s\n",
				pci->ctl_name, pci->pci_idx, msg);

	edac_pci_do_parity_check();
}
EXPORT_SYMBOL_GPL(edac_pci_handle_pe);


void edac_pci_handle_npe(struct edac_pci_ctl_info *pci, const char *msg)
{

	
	atomic_inc(&pci->counters.npe_count);

	if (edac_pci_get_log_npe())
		edac_pci_printk(pci, KERN_WARNING,
				"Non-Parity Error ctl: %s %d: %s\n",
				pci->ctl_name, pci->pci_idx, msg);

	edac_pci_do_parity_check();
}
EXPORT_SYMBOL_GPL(edac_pci_handle_npe);

module_param(check_pci_errors, int, 0644);
MODULE_PARM_DESC(check_pci_errors,
		 "Check for PCI bus parity errors: 0=off 1=on");
module_param(edac_pci_panic_on_pe, int, 0644);
MODULE_PARM_DESC(edac_pci_panic_on_pe,
		 "Panic on PCI Bus Parity error: 0=off 1=on");

#endif				
