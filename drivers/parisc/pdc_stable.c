/* 
 *    Interfaces to retrieve and set PDC Stable options (firmware)
 *
 *    Copyright (C) 2005-2006 Thibaut VARENE <varenet@parisc-linux.org>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License, version 2, as
 *    published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *    DEV NOTE: the PDC Procedures reference states that:
 *    "A minimum of 96 bytes of Stable Storage is required. Providing more than
 *    96 bytes of Stable Storage is optional [...]. Failure to provide the
 *    optional locations from 96 to 192 results in the loss of certain
 *    functionality during boot."
 *
 *    Since locations between 96 and 192 are the various paths, most (if not
 *    all) PA-RISC machines should have them. Anyway, for safety reasons, the
 *    following code can deal with just 96 bytes of Stable Storage, and all
 *    sizes between 96 and 192 bytes (provided they are multiple of struct
 *    device_path size, eg: 128, 160 and 192) to provide full information.
 *    One last word: there's one path we can always count on: the primary path.
 *    Anything above 224 bytes is used for 'osdep2' OS-dependent storage area.
 *
 *    The first OS-dependent area should always be available. Obviously, this is
 *    not true for the other one. Also bear in mind that reading/writing from/to
 *    osdep2 is much more expensive than from/to osdep1.
 *    NOTE: We do not handle the 2 bytes OS-dep area at 0x5D, nor the first
 *    2 bytes of storage available right after OSID. That's a total of 4 bytes
 *    sacrificed: -ETOOLAZY :P
 *
 *    The current policy wrt file permissions is:
 *	- write: root only
 *	- read: (reading triggers PDC calls) ? root only : everyone
 *    The rationale is that PDC calls could hog (DoS) the machine.
 *
 *	TODO:
 *	- timer/fastsize write calls
 */

#undef PDCS_DEBUG
#ifdef PDCS_DEBUG
#define DPRINTK(fmt, args...)	printk(KERN_DEBUG fmt, ## args)
#else
#define DPRINTK(fmt, args...)
#endif

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/capability.h>
#include <linux/ctype.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/spinlock.h>

#include <asm/pdc.h>
#include <asm/page.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>

#define PDCS_VERSION	"0.30"
#define PDCS_PREFIX	"PDC Stable Storage"

#define PDCS_ADDR_PPRI	0x00
#define PDCS_ADDR_OSID	0x40
#define PDCS_ADDR_OSD1	0x48
#define PDCS_ADDR_DIAG	0x58
#define PDCS_ADDR_FSIZ	0x5C
#define PDCS_ADDR_PCON	0x60
#define PDCS_ADDR_PALT	0x80
#define PDCS_ADDR_PKBD	0xA0
#define PDCS_ADDR_OSD2	0xE0

MODULE_AUTHOR("Thibaut VARENE <varenet@parisc-linux.org>");
MODULE_DESCRIPTION("sysfs interface to HP PDC Stable Storage data");
MODULE_LICENSE("GPL");
MODULE_VERSION(PDCS_VERSION);

static unsigned long pdcs_size __read_mostly;

static u16 pdcs_osid __read_mostly;

struct pdcspath_entry {
	rwlock_t rw_lock;		
	short ready;			
	unsigned long addr;		
	char *name;			
	struct device_path devpath;	
	struct device *dev;		
	struct kobject kobj;
};

struct pdcspath_attribute {
	struct attribute attr;
	ssize_t (*show)(struct pdcspath_entry *entry, char *buf);
	ssize_t (*store)(struct pdcspath_entry *entry, const char *buf, size_t count);
};

#define PDCSPATH_ENTRY(_addr, _name) \
struct pdcspath_entry pdcspath_entry_##_name = { \
	.ready = 0, \
	.addr = _addr, \
	.name = __stringify(_name), \
};

#define PDCS_ATTR(_name, _mode, _show, _store) \
struct kobj_attribute pdcs_attr_##_name = { \
	.attr = {.name = __stringify(_name), .mode = _mode}, \
	.show = _show, \
	.store = _store, \
};

#define PATHS_ATTR(_name, _mode, _show, _store) \
struct pdcspath_attribute paths_attr_##_name = { \
	.attr = {.name = __stringify(_name), .mode = _mode}, \
	.show = _show, \
	.store = _store, \
};

#define to_pdcspath_attribute(_attr) container_of(_attr, struct pdcspath_attribute, attr)
#define to_pdcspath_entry(obj)  container_of(obj, struct pdcspath_entry, kobj)

static int
pdcspath_fetch(struct pdcspath_entry *entry)
{
	struct device_path *devpath;

	if (!entry)
		return -EINVAL;

	devpath = &entry->devpath;
	
	DPRINTK("%s: fetch: 0x%p, 0x%p, addr: 0x%lx\n", __func__,
			entry, devpath, entry->addr);

	
	if (pdc_stable_read(entry->addr, devpath, sizeof(*devpath)) != PDC_OK)
		return -EIO;
		
	entry->dev = hwpath_to_device((struct hardware_path *)devpath);

	entry->ready = 1;
	
	DPRINTK("%s: device: 0x%p\n", __func__, entry->dev);
	
	return 0;
}

static void
pdcspath_store(struct pdcspath_entry *entry)
{
	struct device_path *devpath;

	BUG_ON(!entry);

	devpath = &entry->devpath;
	
	if (!entry->ready) {
		
		BUG_ON(!entry->dev);
		device_to_hwpath(entry->dev, (struct hardware_path *)devpath);
	}
	
	
	DPRINTK("%s: store: 0x%p, 0x%p, addr: 0x%lx\n", __func__,
			entry, devpath, entry->addr);

	
	if (pdc_stable_write(entry->addr, devpath, sizeof(*devpath)) != PDC_OK) {
		printk(KERN_ERR "%s: an error occurred when writing to PDC.\n"
				"It is likely that the Stable Storage data has been corrupted.\n"
				"Please check it carefully upon next reboot.\n", __func__);
		WARN_ON(1);
	}
		
	
	entry->ready = 2;
	
	DPRINTK("%s: device: 0x%p\n", __func__, entry->dev);
}

static ssize_t
pdcspath_hwpath_read(struct pdcspath_entry *entry, char *buf)
{
	char *out = buf;
	struct device_path *devpath;
	short i;

	if (!entry || !buf)
		return -EINVAL;

	read_lock(&entry->rw_lock);
	devpath = &entry->devpath;
	i = entry->ready;
	read_unlock(&entry->rw_lock);

	if (!i)	
		return -ENODATA;
	
	for (i = 0; i < 6; i++) {
		if (devpath->bc[i] >= 128)
			continue;
		out += sprintf(out, "%u/", (unsigned char)devpath->bc[i]);
	}
	out += sprintf(out, "%u\n", (unsigned char)devpath->mod);
	
	return out - buf;
}

static ssize_t
pdcspath_hwpath_write(struct pdcspath_entry *entry, const char *buf, size_t count)
{
	struct hardware_path hwpath;
	unsigned short i;
	char in[count+1], *temp;
	struct device *dev;
	int ret;

	if (!entry || !buf || !count)
		return -EINVAL;

	
	memset(in, 0, count+1);
	strncpy(in, buf, count);
	
	
	memset(&hwpath, 0xff, sizeof(hwpath));
	
	
	if (!(temp = strrchr(in, '/')))
		return -EINVAL;
			
	hwpath.mod = simple_strtoul(temp+1, NULL, 10);
	in[temp-in] = '\0';	
	DPRINTK("%s: mod: %d\n", __func__, hwpath.mod);
	
	for (i=5; ((temp = strrchr(in, '/'))) && (temp-in > 0) && (likely(i)); i--) {
		hwpath.bc[i] = simple_strtoul(temp+1, NULL, 10);
		in[temp-in] = '\0';
		DPRINTK("%s: bc[%d]: %d\n", __func__, i, hwpath.bc[i]);
	}
	
			
	hwpath.bc[i] = simple_strtoul(in, NULL, 10);
	DPRINTK("%s: bc[%d]: %d\n", __func__, i, hwpath.bc[i]);
	
	
	if (!(dev = hwpath_to_device((struct hardware_path *)&hwpath))) {
		printk(KERN_WARNING "%s: attempt to set invalid \"%s\" "
			"hardware path: %s\n", __func__, entry->name, buf);
		return -EINVAL;
	}
	
	
	write_lock(&entry->rw_lock);
	entry->ready = 0;
	entry->dev = dev;
	
	
	pdcspath_store(entry);
	
	
	sysfs_remove_link(&entry->kobj, "device");
	ret = sysfs_create_link(&entry->kobj, &entry->dev->kobj, "device");
	WARN_ON(ret);

	write_unlock(&entry->rw_lock);
	
	printk(KERN_INFO PDCS_PREFIX ": changed \"%s\" path to \"%s\"\n",
		entry->name, buf);
	
	return count;
}

static ssize_t
pdcspath_layer_read(struct pdcspath_entry *entry, char *buf)
{
	char *out = buf;
	struct device_path *devpath;
	short i;

	if (!entry || !buf)
		return -EINVAL;
	
	read_lock(&entry->rw_lock);
	devpath = &entry->devpath;
	i = entry->ready;
	read_unlock(&entry->rw_lock);

	if (!i)	
		return -ENODATA;
	
	for (i = 0; i < 6 && devpath->layers[i]; i++)
		out += sprintf(out, "%u ", devpath->layers[i]);

	out += sprintf(out, "\n");
	
	return out - buf;
}

static ssize_t
pdcspath_layer_write(struct pdcspath_entry *entry, const char *buf, size_t count)
{
	unsigned int layers[6]; 
	unsigned short i;
	char in[count+1], *temp;

	if (!entry || !buf || !count)
		return -EINVAL;

	
	memset(in, 0, count+1);
	strncpy(in, buf, count);
	
	
	memset(&layers, 0, sizeof(layers));
	
	
	if (unlikely(!isdigit(*in)))
		return -EINVAL;
	layers[0] = simple_strtoul(in, NULL, 10);
	DPRINTK("%s: layer[0]: %d\n", __func__, layers[0]);
	
	temp = in;
	for (i=1; ((temp = strchr(temp, '.'))) && (likely(i<6)); i++) {
		if (unlikely(!isdigit(*(++temp))))
			return -EINVAL;
		layers[i] = simple_strtoul(temp, NULL, 10);
		DPRINTK("%s: layer[%d]: %d\n", __func__, i, layers[i]);
	}
		
	
	write_lock(&entry->rw_lock);
	
	memcpy(&entry->devpath.layers, &layers, sizeof(layers));
	
	
	pdcspath_store(entry);
	write_unlock(&entry->rw_lock);
	
	printk(KERN_INFO PDCS_PREFIX ": changed \"%s\" layers to \"%s\"\n",
		entry->name, buf);
	
	return count;
}

static ssize_t
pdcspath_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct pdcspath_entry *entry = to_pdcspath_entry(kobj);
	struct pdcspath_attribute *pdcs_attr = to_pdcspath_attribute(attr);
	ssize_t ret = 0;

	if (pdcs_attr->show)
		ret = pdcs_attr->show(entry, buf);

	return ret;
}

static ssize_t
pdcspath_attr_store(struct kobject *kobj, struct attribute *attr,
			const char *buf, size_t count)
{
	struct pdcspath_entry *entry = to_pdcspath_entry(kobj);
	struct pdcspath_attribute *pdcs_attr = to_pdcspath_attribute(attr);
	ssize_t ret = 0;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (pdcs_attr->store)
		ret = pdcs_attr->store(entry, buf, count);

	return ret;
}

static const struct sysfs_ops pdcspath_attr_ops = {
	.show = pdcspath_attr_show,
	.store = pdcspath_attr_store,
};

static PATHS_ATTR(hwpath, 0644, pdcspath_hwpath_read, pdcspath_hwpath_write);
static PATHS_ATTR(layer, 0644, pdcspath_layer_read, pdcspath_layer_write);

static struct attribute *paths_subsys_attrs[] = {
	&paths_attr_hwpath.attr,
	&paths_attr_layer.attr,
	NULL,
};

static struct kobj_type ktype_pdcspath = {
	.sysfs_ops = &pdcspath_attr_ops,
	.default_attrs = paths_subsys_attrs,
};

static PDCSPATH_ENTRY(PDCS_ADDR_PPRI, primary);
static PDCSPATH_ENTRY(PDCS_ADDR_PCON, console);
static PDCSPATH_ENTRY(PDCS_ADDR_PALT, alternative);
static PDCSPATH_ENTRY(PDCS_ADDR_PKBD, keyboard);

static struct pdcspath_entry *pdcspath_entries[] = {
	&pdcspath_entry_primary,
	&pdcspath_entry_alternative,
	&pdcspath_entry_console,
	&pdcspath_entry_keyboard,
	NULL,
};



static ssize_t pdcs_size_read(struct kobject *kobj,
			      struct kobj_attribute *attr,
			      char *buf)
{
	char *out = buf;

	if (!buf)
		return -EINVAL;

	
	out += sprintf(out, "%ld\n", pdcs_size);

	return out - buf;
}

static ssize_t pdcs_auto_read(struct kobject *kobj,
			      struct kobj_attribute *attr,
			      char *buf, int knob)
{
	char *out = buf;
	struct pdcspath_entry *pathentry;

	if (!buf)
		return -EINVAL;

	
	pathentry = &pdcspath_entry_primary;

	read_lock(&pathentry->rw_lock);
	out += sprintf(out, "%s\n", (pathentry->devpath.flags & knob) ?
					"On" : "Off");
	read_unlock(&pathentry->rw_lock);

	return out - buf;
}

static ssize_t pdcs_autoboot_read(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	return pdcs_auto_read(kobj, attr, buf, PF_AUTOBOOT);
}

static ssize_t pdcs_autosearch_read(struct kobject *kobj,
				    struct kobj_attribute *attr, char *buf)
{
	return pdcs_auto_read(kobj, attr, buf, PF_AUTOSEARCH);
}

static ssize_t pdcs_timer_read(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	char *out = buf;
	struct pdcspath_entry *pathentry;

	if (!buf)
		return -EINVAL;

	
	pathentry = &pdcspath_entry_primary;

	
	read_lock(&pathentry->rw_lock);
	out += sprintf(out, "%u\n", (pathentry->devpath.flags & PF_TIMER) ?
				(1 << (pathentry->devpath.flags & PF_TIMER)) : 0);
	read_unlock(&pathentry->rw_lock);

	return out - buf;
}

static ssize_t pdcs_osid_read(struct kobject *kobj,
			      struct kobj_attribute *attr, char *buf)
{
	char *out = buf;

	if (!buf)
		return -EINVAL;

	out += sprintf(out, "%s dependent data (0x%.4x)\n",
		os_id_to_string(pdcs_osid), pdcs_osid);

	return out - buf;
}

static ssize_t pdcs_osdep1_read(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	char *out = buf;
	u32 result[4];

	if (!buf)
		return -EINVAL;

	if (pdc_stable_read(PDCS_ADDR_OSD1, &result, sizeof(result)) != PDC_OK)
		return -EIO;

	out += sprintf(out, "0x%.8x\n", result[0]);
	out += sprintf(out, "0x%.8x\n", result[1]);
	out += sprintf(out, "0x%.8x\n", result[2]);
	out += sprintf(out, "0x%.8x\n", result[3]);

	return out - buf;
}

static ssize_t pdcs_diagnostic_read(struct kobject *kobj,
				    struct kobj_attribute *attr, char *buf)
{
	char *out = buf;
	u32 result;

	if (!buf)
		return -EINVAL;

	
	if (pdc_stable_read(PDCS_ADDR_DIAG, &result, sizeof(result)) != PDC_OK)
		return -EIO;

	out += sprintf(out, "0x%.4x\n", (result >> 16));

	return out - buf;
}

static ssize_t pdcs_fastsize_read(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	char *out = buf;
	u32 result;

	if (!buf)
		return -EINVAL;

	
	if (pdc_stable_read(PDCS_ADDR_FSIZ, &result, sizeof(result)) != PDC_OK)
		return -EIO;

	if ((result & 0x0F) < 0x0E)
		out += sprintf(out, "%d kB", (1<<(result & 0x0F))*256);
	else
		out += sprintf(out, "All");
	out += sprintf(out, "\n");
	
	return out - buf;
}

static ssize_t pdcs_osdep2_read(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	char *out = buf;
	unsigned long size;
	unsigned short i;
	u32 result;

	if (unlikely(pdcs_size <= 224))
		return -ENODATA;

	size = pdcs_size - 224;

	if (!buf)
		return -EINVAL;

	for (i=0; i<size; i+=4) {
		if (unlikely(pdc_stable_read(PDCS_ADDR_OSD2 + i, &result,
					sizeof(result)) != PDC_OK))
			return -EIO;
		out += sprintf(out, "0x%.8x\n", result);
	}

	return out - buf;
}

static ssize_t pdcs_auto_write(struct kobject *kobj,
			       struct kobj_attribute *attr, const char *buf,
			       size_t count, int knob)
{
	struct pdcspath_entry *pathentry;
	unsigned char flags;
	char in[count+1], *temp;
	char c;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (!buf || !count)
		return -EINVAL;

	
	memset(in, 0, count+1);
	strncpy(in, buf, count);

	
	pathentry = &pdcspath_entry_primary;
	
	
	read_lock(&pathentry->rw_lock);
	flags = pathentry->devpath.flags;
	read_unlock(&pathentry->rw_lock);
	
	DPRINTK("%s: flags before: 0x%X\n", __func__, flags);

	temp = skip_spaces(in);

	c = *temp++ - '0';
	if ((c != 0) && (c != 1))
		goto parse_error;
	if (c == 0)
		flags &= ~knob;
	else
		flags |= knob;
	
	DPRINTK("%s: flags after: 0x%X\n", __func__, flags);
		
	
	write_lock(&pathentry->rw_lock);
	
	
	pathentry->devpath.flags = flags;
		
	
	pdcspath_store(pathentry);
	write_unlock(&pathentry->rw_lock);
	
	printk(KERN_INFO PDCS_PREFIX ": changed \"%s\" to \"%s\"\n",
		(knob & PF_AUTOBOOT) ? "autoboot" : "autosearch",
		(flags & knob) ? "On" : "Off");
	
	return count;

parse_error:
	printk(KERN_WARNING "%s: Parse error: expect \"n\" (n == 0 or 1)\n", __func__);
	return -EINVAL;
}

static ssize_t pdcs_autoboot_write(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   const char *buf, size_t count)
{
	return pdcs_auto_write(kobj, attr, buf, count, PF_AUTOBOOT);
}

static ssize_t pdcs_autosearch_write(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     const char *buf, size_t count)
{
	return pdcs_auto_write(kobj, attr, buf, count, PF_AUTOSEARCH);
}

static ssize_t pdcs_osdep1_write(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	u8 in[16];

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (!buf || !count)
		return -EINVAL;

	if (unlikely(pdcs_osid != OS_ID_LINUX))
		return -EPERM;

	if (count > 16)
		return -EMSGSIZE;

	
	memset(in, 0, 16);
	memcpy(in, buf, count);

	if (pdc_stable_write(PDCS_ADDR_OSD1, &in, sizeof(in)) != PDC_OK)
		return -EIO;

	return count;
}

static ssize_t pdcs_osdep2_write(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	unsigned long size;
	unsigned short i;
	u8 in[4];

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (!buf || !count)
		return -EINVAL;

	if (unlikely(pdcs_size <= 224))
		return -ENOSYS;

	if (unlikely(pdcs_osid != OS_ID_LINUX))
		return -EPERM;

	size = pdcs_size - 224;

	if (count > size)
		return -EMSGSIZE;

	

	for (i=0; i<count; i+=4) {
		memset(in, 0, 4);
		memcpy(in, buf+i, (count-i < 4) ? count-i : 4);
		if (unlikely(pdc_stable_write(PDCS_ADDR_OSD2 + i, &in,
					sizeof(in)) != PDC_OK))
			return -EIO;
	}

	return count;
}

static PDCS_ATTR(size, 0444, pdcs_size_read, NULL);
static PDCS_ATTR(autoboot, 0644, pdcs_autoboot_read, pdcs_autoboot_write);
static PDCS_ATTR(autosearch, 0644, pdcs_autosearch_read, pdcs_autosearch_write);
static PDCS_ATTR(timer, 0444, pdcs_timer_read, NULL);
static PDCS_ATTR(osid, 0444, pdcs_osid_read, NULL);
static PDCS_ATTR(osdep1, 0600, pdcs_osdep1_read, pdcs_osdep1_write);
static PDCS_ATTR(diagnostic, 0400, pdcs_diagnostic_read, NULL);
static PDCS_ATTR(fastsize, 0400, pdcs_fastsize_read, NULL);
static PDCS_ATTR(osdep2, 0600, pdcs_osdep2_read, pdcs_osdep2_write);

static struct attribute *pdcs_subsys_attrs[] = {
	&pdcs_attr_size.attr,
	&pdcs_attr_autoboot.attr,
	&pdcs_attr_autosearch.attr,
	&pdcs_attr_timer.attr,
	&pdcs_attr_osid.attr,
	&pdcs_attr_osdep1.attr,
	&pdcs_attr_diagnostic.attr,
	&pdcs_attr_fastsize.attr,
	&pdcs_attr_osdep2.attr,
	NULL,
};

static struct attribute_group pdcs_attr_group = {
	.attrs = pdcs_subsys_attrs,
};

static struct kobject *stable_kobj;
static struct kset *paths_kset;

static inline int __init
pdcs_register_pathentries(void)
{
	unsigned short i;
	struct pdcspath_entry *entry;
	int err;
	
	
	for (i = 0; (entry = pdcspath_entries[i]); i++)
		rwlock_init(&entry->rw_lock);

	for (i = 0; (entry = pdcspath_entries[i]); i++) {
		write_lock(&entry->rw_lock);
		err = pdcspath_fetch(entry);
		write_unlock(&entry->rw_lock);

		if (err < 0)
			continue;

		entry->kobj.kset = paths_kset;
		err = kobject_init_and_add(&entry->kobj, &ktype_pdcspath, NULL,
					   "%s", entry->name);
		if (err)
			return err;

		
		write_lock(&entry->rw_lock);
		entry->ready = 2;
		
		
		if (entry->dev) {
			err = sysfs_create_link(&entry->kobj, &entry->dev->kobj, "device");
			WARN_ON(err);
		}

		write_unlock(&entry->rw_lock);
		kobject_uevent(&entry->kobj, KOBJ_ADD);
	}
	
	return 0;
}

static inline void
pdcs_unregister_pathentries(void)
{
	unsigned short i;
	struct pdcspath_entry *entry;
	
	for (i = 0; (entry = pdcspath_entries[i]); i++) {
		read_lock(&entry->rw_lock);
		if (entry->ready >= 2)
			kobject_put(&entry->kobj);
		read_unlock(&entry->rw_lock);
	}
}

static int __init
pdc_stable_init(void)
{
	int rc = 0, error = 0;
	u32 result;

	
	if (pdc_stable_get_size(&pdcs_size) != PDC_OK) 
		return -ENODEV;

	
	if (pdcs_size < 96)
		return -ENODATA;

	printk(KERN_INFO PDCS_PREFIX " facility v%s\n", PDCS_VERSION);

	
	if (pdc_stable_read(PDCS_ADDR_OSID, &result, sizeof(result)) != PDC_OK)
		return -EIO;

	
	pdcs_osid = (u16)(result >> 16);

	
	stable_kobj = kobject_create_and_add("stable", firmware_kobj);
	if (!stable_kobj) {
		rc = -ENOMEM;
		goto fail_firmreg;
	}

	
	error = sysfs_create_group(stable_kobj, &pdcs_attr_group);

	
	paths_kset = kset_create_and_add("paths", NULL, stable_kobj);
	if (!paths_kset) {
		rc = -ENOMEM;
		goto fail_ksetreg;
	}

	
	if ((rc = pdcs_register_pathentries()))
		goto fail_pdcsreg;

	return rc;
	
fail_pdcsreg:
	pdcs_unregister_pathentries();
	kset_unregister(paths_kset);
	
fail_ksetreg:
	kobject_put(stable_kobj);
	
fail_firmreg:
	printk(KERN_INFO PDCS_PREFIX " bailing out\n");
	return rc;
}

static void __exit
pdc_stable_exit(void)
{
	pdcs_unregister_pathentries();
	kset_unregister(paths_kset);
	kobject_put(stable_kobj);
}


module_init(pdc_stable_init);
module_exit(pdc_stable_exit);
