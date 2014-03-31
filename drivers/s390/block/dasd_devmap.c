
#define KMSG_COMPONENT "dasd"

#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <asm/debug.h>
#include <asm/uaccess.h>
#include <asm/ipl.h>

#define PRINTK_HEADER "dasd_devmap:"
#define DASD_BUS_ID_SIZE 20

#include "dasd_int.h"

struct kmem_cache *dasd_page_cache;
EXPORT_SYMBOL_GPL(dasd_page_cache);

struct dasd_devmap {
	struct list_head list;
	char bus_id[DASD_BUS_ID_SIZE];
        unsigned int devindex;
        unsigned short features;
	struct dasd_device *device;
};


int dasd_probeonly =  0;	
int dasd_autodetect = 0;	
int dasd_nopav = 0;		
EXPORT_SYMBOL_GPL(dasd_nopav);
int dasd_nofcx;			
EXPORT_SYMBOL_GPL(dasd_nofcx);

static char *dasd[256];
module_param_array(dasd, charp, NULL, 0);

static DEFINE_SPINLOCK(dasd_devmap_lock);

static struct list_head dasd_hashlists[256];
int dasd_max_devindex;

static struct dasd_devmap *dasd_add_busid(const char *, int);

static inline int
dasd_hash_busid(const char *bus_id)
{
	int hash, i;

	hash = 0;
	for (i = 0; (i < DASD_BUS_ID_SIZE) && *bus_id; i++, bus_id++)
		hash += *bus_id;
	return hash & 0xff;
}

#ifndef MODULE
static int __init
dasd_call_setup(char *str)
{
	static int count = 0;

	if (count < 256)
		dasd[count++] = str;
	return 1;
}

__setup ("dasd=", dasd_call_setup);
#endif	

#define	DASD_IPLDEV	"ipldev"

static int

dasd_busid(char **str, int *id0, int *id1, int *devno)
{
	int val, old_style;

	
	if (strncmp(DASD_IPLDEV, *str, strlen(DASD_IPLDEV)) == 0) {
		if (ipl_info.type != IPL_TYPE_CCW) {
			pr_err("The IPL device is not a CCW device\n");
			return -EINVAL;
		}
		*id0 = 0;
		*id1 = ipl_info.data.ccw.dev_id.ssid;
		*devno = ipl_info.data.ccw.dev_id.devno;
		*str += strlen(DASD_IPLDEV);

		return 0;
	}
	
	old_style = 0;
	if ((*str)[0] == '0' && (*str)[1] == 'x') {
		*str += 2;
		old_style = 1;
	}
	if (!isxdigit((*str)[0]))	
		return -EINVAL;
	val = simple_strtoul(*str, str, 16);
	if (old_style || (*str)[0] != '.') {
		*id0 = *id1 = 0;
		if (val < 0 || val > 0xffff)
			return -EINVAL;
		*devno = val;
		return 0;
	}
	
	if (val < 0 || val > 0xff)
		return -EINVAL;
	*id0 = val;
	(*str)++;
	if (!isxdigit((*str)[0]))	
		return -EINVAL;
	val = simple_strtoul(*str, str, 16);
	if (val < 0 || val > 0xff || (*str)++[0] != '.')
		return -EINVAL;
	*id1 = val;
	if (!isxdigit((*str)[0]))	
		return -EINVAL;
	val = simple_strtoul(*str, str, 16);
	if (val < 0 || val > 0xffff)
		return -EINVAL;
	*devno = val;
	return 0;
}

static int
dasd_feature_list(char *str, char **endp)
{
	int features, len, rc;

	rc = 0;
	if (*str != '(') {
		*endp = str;
		return DASD_FEATURE_DEFAULT;
	}
	str++;
	features = 0;

	while (1) {
		for (len = 0;
		     str[len] && str[len] != ':' && str[len] != ')'; len++);
		if (len == 2 && !strncmp(str, "ro", 2))
			features |= DASD_FEATURE_READONLY;
		else if (len == 4 && !strncmp(str, "diag", 4))
			features |= DASD_FEATURE_USEDIAG;
		else if (len == 3 && !strncmp(str, "raw", 3))
			features |= DASD_FEATURE_USERAW;
		else if (len == 6 && !strncmp(str, "erplog", 6))
			features |= DASD_FEATURE_ERPLOG;
		else if (len == 8 && !strncmp(str, "failfast", 8))
			features |= DASD_FEATURE_FAILFAST;
		else {
			pr_warning("%*s is not a supported device option\n",
				   len, str);
			rc = -EINVAL;
		}
		str += len;
		if (*str != ':')
			break;
		str++;
	}
	if (*str != ')') {
		pr_warning("A closing parenthesis ')' is missing in the "
			   "dasd= parameter\n");
		rc = -EINVAL;
	} else
		str++;
	*endp = str;
	if (rc != 0)
		return rc;
	return features;
}

static char *
dasd_parse_keyword( char *parsestring ) {

	char *nextcomma, *residual_str;
	int length;

	nextcomma = strchr(parsestring,',');
	if (nextcomma) {
		length = nextcomma - parsestring;
		residual_str = nextcomma + 1;
	} else {
		length = strlen(parsestring);
		residual_str = parsestring + length;
        }
	if (strncmp("autodetect", parsestring, length) == 0) {
		dasd_autodetect = 1;
		pr_info("The autodetection mode has been activated\n");
                return residual_str;
        }
	if (strncmp("probeonly", parsestring, length) == 0) {
		dasd_probeonly = 1;
		pr_info("The probeonly mode has been activated\n");
                return residual_str;
        }
	if (strncmp("nopav", parsestring, length) == 0) {
		if (MACHINE_IS_VM)
			pr_info("'nopav' is not supported on z/VM\n");
		else {
			dasd_nopav = 1;
			pr_info("PAV support has be deactivated\n");
		}
		return residual_str;
	}
	if (strncmp("nofcx", parsestring, length) == 0) {
		dasd_nofcx = 1;
		pr_info("High Performance FICON support has been "
			"deactivated\n");
		return residual_str;
	}
	if (strncmp("fixedbuffers", parsestring, length) == 0) {
		if (dasd_page_cache)
			return residual_str;
		dasd_page_cache =
			kmem_cache_create("dasd_page_cache", PAGE_SIZE,
					  PAGE_SIZE, SLAB_CACHE_DMA,
					  NULL);
		if (!dasd_page_cache)
			DBF_EVENT(DBF_WARNING, "%s", "Failed to create slab, "
				"fixed buffer mode disabled.");
		else
			DBF_EVENT(DBF_INFO, "%s",
				 "turning on fixed buffer mode");
                return residual_str;
        }
	return ERR_PTR(-EINVAL);
}

static char *
dasd_parse_range( char *parsestring ) {

	struct dasd_devmap *devmap;
	int from, from_id0, from_id1;
	int to, to_id0, to_id1;
	int features, rc;
	char bus_id[DASD_BUS_ID_SIZE+1], *str;

	str = parsestring;
	rc = dasd_busid(&str, &from_id0, &from_id1, &from);
	if (rc == 0) {
		to = from;
		to_id0 = from_id0;
		to_id1 = from_id1;
		if (*str == '-') {
			str++;
			rc = dasd_busid(&str, &to_id0, &to_id1, &to);
		}
	}
	if (rc == 0 &&
	    (from_id0 != to_id0 || from_id1 != to_id1 || from > to))
		rc = -EINVAL;
	if (rc) {
		pr_err("%s is not a valid device range\n", parsestring);
		return ERR_PTR(rc);
	}
	features = dasd_feature_list(str, &str);
	if (features < 0)
		return ERR_PTR(-EINVAL);
	
	features |= DASD_FEATURE_INITIAL_ONLINE;
	while (from <= to) {
		sprintf(bus_id, "%01x.%01x.%04x",
			from_id0, from_id1, from++);
		devmap = dasd_add_busid(bus_id, features);
		if (IS_ERR(devmap))
			return (char *)devmap;
	}
	if (*str == ',')
		return str + 1;
	if (*str == '\0')
		return str;
	pr_warning("The dasd= parameter value %s has an invalid ending\n",
		   str);
	return ERR_PTR(-EINVAL);
}

static char *
dasd_parse_next_element( char *parsestring ) {
	char * residual_str;
	residual_str = dasd_parse_keyword(parsestring);
	if (!IS_ERR(residual_str))
		return residual_str;
	residual_str = dasd_parse_range(parsestring);
	return residual_str;
}

int
dasd_parse(void)
{
	int rc, i;
	char *parsestring;

	rc = 0;
	for (i = 0; i < 256; i++) {
		if (dasd[i] == NULL)
			break;
		parsestring = dasd[i];
		
		while (*parsestring) {
			parsestring = dasd_parse_next_element(parsestring);
			if(IS_ERR(parsestring)) {
				rc = PTR_ERR(parsestring);
				break;
			}
		}
		if (rc) {
			DBF_EVENT(DBF_ALERT, "%s", "invalid range found");
			break;
		}
	}
	return rc;
}

static struct dasd_devmap *
dasd_add_busid(const char *bus_id, int features)
{
	struct dasd_devmap *devmap, *new, *tmp;
	int hash;

	new = (struct dasd_devmap *)
		kzalloc(sizeof(struct dasd_devmap), GFP_KERNEL);
	if (!new)
		return ERR_PTR(-ENOMEM);
	spin_lock(&dasd_devmap_lock);
	devmap = NULL;
	hash = dasd_hash_busid(bus_id);
	list_for_each_entry(tmp, &dasd_hashlists[hash], list)
		if (strncmp(tmp->bus_id, bus_id, DASD_BUS_ID_SIZE) == 0) {
			devmap = tmp;
			break;
		}
	if (!devmap) {
		
		new->devindex = dasd_max_devindex++;
		strncpy(new->bus_id, bus_id, DASD_BUS_ID_SIZE);
		new->features = features;
		new->device = NULL;
		list_add(&new->list, &dasd_hashlists[hash]);
		devmap = new;
		new = NULL;
	}
	spin_unlock(&dasd_devmap_lock);
	kfree(new);
	return devmap;
}

static struct dasd_devmap *
dasd_find_busid(const char *bus_id)
{
	struct dasd_devmap *devmap, *tmp;
	int hash;

	spin_lock(&dasd_devmap_lock);
	devmap = ERR_PTR(-ENODEV);
	hash = dasd_hash_busid(bus_id);
	list_for_each_entry(tmp, &dasd_hashlists[hash], list) {
		if (strncmp(tmp->bus_id, bus_id, DASD_BUS_ID_SIZE) == 0) {
			devmap = tmp;
			break;
		}
	}
	spin_unlock(&dasd_devmap_lock);
	return devmap;
}

int
dasd_busid_known(const char *bus_id)
{
	return IS_ERR(dasd_find_busid(bus_id)) ? -ENOENT : 0;
}

static void
dasd_forget_ranges(void)
{
	struct dasd_devmap *devmap, *n;
	int i;

	spin_lock(&dasd_devmap_lock);
	for (i = 0; i < 256; i++) {
		list_for_each_entry_safe(devmap, n, &dasd_hashlists[i], list) {
			BUG_ON(devmap->device != NULL);
			list_del(&devmap->list);
			kfree(devmap);
		}
	}
	spin_unlock(&dasd_devmap_lock);
}

struct dasd_device *
dasd_device_from_devindex(int devindex)
{
	struct dasd_devmap *devmap, *tmp;
	struct dasd_device *device;
	int i;

	spin_lock(&dasd_devmap_lock);
	devmap = NULL;
	for (i = 0; (i < 256) && !devmap; i++)
		list_for_each_entry(tmp, &dasd_hashlists[i], list)
			if (tmp->devindex == devindex) {
				
				devmap = tmp;
				break;
			}
	if (devmap && devmap->device) {
		device = devmap->device;
		dasd_get_device(device);
	} else
		device = ERR_PTR(-ENODEV);
	spin_unlock(&dasd_devmap_lock);
	return device;
}

static struct dasd_devmap *
dasd_devmap_from_cdev(struct ccw_device *cdev)
{
	struct dasd_devmap *devmap;

	devmap = dasd_find_busid(dev_name(&cdev->dev));
	if (IS_ERR(devmap))
		devmap = dasd_add_busid(dev_name(&cdev->dev),
					DASD_FEATURE_DEFAULT);
	return devmap;
}

struct dasd_device *
dasd_create_device(struct ccw_device *cdev)
{
	struct dasd_devmap *devmap;
	struct dasd_device *device;
	unsigned long flags;
	int rc;

	devmap = dasd_devmap_from_cdev(cdev);
	if (IS_ERR(devmap))
		return (void *) devmap;

	device = dasd_alloc_device();
	if (IS_ERR(device))
		return device;
	atomic_set(&device->ref_count, 3);

	spin_lock(&dasd_devmap_lock);
	if (!devmap->device) {
		devmap->device = device;
		device->devindex = devmap->devindex;
		device->features = devmap->features;
		get_device(&cdev->dev);
		device->cdev = cdev;
		rc = 0;
	} else
		
		rc = -EBUSY;
	spin_unlock(&dasd_devmap_lock);

	if (rc) {
		dasd_free_device(device);
		return ERR_PTR(rc);
	}

	spin_lock_irqsave(get_ccwdev_lock(cdev), flags);
	dev_set_drvdata(&cdev->dev, device);
	spin_unlock_irqrestore(get_ccwdev_lock(cdev), flags);

	return device;
}

static DECLARE_WAIT_QUEUE_HEAD(dasd_delete_wq);

void
dasd_delete_device(struct dasd_device *device)
{
	struct ccw_device *cdev;
	struct dasd_devmap *devmap;
	unsigned long flags;

	
	devmap = dasd_find_busid(dev_name(&device->cdev->dev));
	BUG_ON(IS_ERR(devmap));
	spin_lock(&dasd_devmap_lock);
	if (devmap->device != device) {
		spin_unlock(&dasd_devmap_lock);
		dasd_put_device(device);
		return;
	}
	devmap->device = NULL;
	spin_unlock(&dasd_devmap_lock);

	
	spin_lock_irqsave(get_ccwdev_lock(device->cdev), flags);
	dev_set_drvdata(&device->cdev->dev, NULL);
	spin_unlock_irqrestore(get_ccwdev_lock(device->cdev), flags);

	atomic_sub(3, &device->ref_count);

	
	wait_event(dasd_delete_wq, atomic_read(&device->ref_count) == 0);

	
	cdev = device->cdev;
	device->cdev = NULL;

	
	put_device(&cdev->dev);

	
	dasd_free_device(device);
}

void
dasd_put_device_wake(struct dasd_device *device)
{
	wake_up(&dasd_delete_wq);
}
EXPORT_SYMBOL_GPL(dasd_put_device_wake);

struct dasd_device *
dasd_device_from_cdev_locked(struct ccw_device *cdev)
{
	struct dasd_device *device = dev_get_drvdata(&cdev->dev);

	if (!device)
		return ERR_PTR(-ENODEV);
	dasd_get_device(device);
	return device;
}

struct dasd_device *
dasd_device_from_cdev(struct ccw_device *cdev)
{
	struct dasd_device *device;
	unsigned long flags;

	spin_lock_irqsave(get_ccwdev_lock(cdev), flags);
	device = dasd_device_from_cdev_locked(cdev);
	spin_unlock_irqrestore(get_ccwdev_lock(cdev), flags);
	return device;
}

void dasd_add_link_to_gendisk(struct gendisk *gdp, struct dasd_device *device)
{
	struct dasd_devmap *devmap;

	devmap = dasd_find_busid(dev_name(&device->cdev->dev));
	if (IS_ERR(devmap))
		return;
	spin_lock(&dasd_devmap_lock);
	gdp->private_data = devmap;
	spin_unlock(&dasd_devmap_lock);
}

struct dasd_device *dasd_device_from_gendisk(struct gendisk *gdp)
{
	struct dasd_device *device;
	struct dasd_devmap *devmap;

	if (!gdp->private_data)
		return NULL;
	device = NULL;
	spin_lock(&dasd_devmap_lock);
	devmap = gdp->private_data;
	if (devmap && devmap->device) {
		device = devmap->device;
		dasd_get_device(device);
	}
	spin_unlock(&dasd_devmap_lock);
	return device;
}


static ssize_t dasd_ff_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct dasd_devmap *devmap;
	int ff_flag;

	devmap = dasd_find_busid(dev_name(dev));
	if (!IS_ERR(devmap))
		ff_flag = (devmap->features & DASD_FEATURE_FAILFAST) != 0;
	else
		ff_flag = (DASD_FEATURE_DEFAULT & DASD_FEATURE_FAILFAST) != 0;
	return snprintf(buf, PAGE_SIZE, ff_flag ? "1\n" : "0\n");
}

static ssize_t dasd_ff_store(struct device *dev, struct device_attribute *attr,
	      const char *buf, size_t count)
{
	struct dasd_devmap *devmap;
	int val;
	char *endp;

	devmap = dasd_devmap_from_cdev(to_ccwdev(dev));
	if (IS_ERR(devmap))
		return PTR_ERR(devmap);

	val = simple_strtoul(buf, &endp, 0);
	if (((endp + 1) < (buf + count)) || (val > 1))
		return -EINVAL;

	spin_lock(&dasd_devmap_lock);
	if (val)
		devmap->features |= DASD_FEATURE_FAILFAST;
	else
		devmap->features &= ~DASD_FEATURE_FAILFAST;
	if (devmap->device)
		devmap->device->features = devmap->features;
	spin_unlock(&dasd_devmap_lock);
	return count;
}

static DEVICE_ATTR(failfast, 0644, dasd_ff_show, dasd_ff_store);

static ssize_t
dasd_ro_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dasd_devmap *devmap;
	int ro_flag;

	devmap = dasd_find_busid(dev_name(dev));
	if (!IS_ERR(devmap))
		ro_flag = (devmap->features & DASD_FEATURE_READONLY) != 0;
	else
		ro_flag = (DASD_FEATURE_DEFAULT & DASD_FEATURE_READONLY) != 0;
	return snprintf(buf, PAGE_SIZE, ro_flag ? "1\n" : "0\n");
}

static ssize_t
dasd_ro_store(struct device *dev, struct device_attribute *attr,
	      const char *buf, size_t count)
{
	struct dasd_devmap *devmap;
	struct dasd_device *device;
	int val;
	char *endp;

	devmap = dasd_devmap_from_cdev(to_ccwdev(dev));
	if (IS_ERR(devmap))
		return PTR_ERR(devmap);

	val = simple_strtoul(buf, &endp, 0);
	if (((endp + 1) < (buf + count)) || (val > 1))
		return -EINVAL;

	spin_lock(&dasd_devmap_lock);
	if (val)
		devmap->features |= DASD_FEATURE_READONLY;
	else
		devmap->features &= ~DASD_FEATURE_READONLY;
	device = devmap->device;
	if (device) {
		device->features = devmap->features;
		val = val || test_bit(DASD_FLAG_DEVICE_RO, &device->flags);
	}
	spin_unlock(&dasd_devmap_lock);
	if (device && device->block && device->block->gdp)
		set_disk_ro(device->block->gdp, val);
	return count;
}

static DEVICE_ATTR(readonly, 0644, dasd_ro_show, dasd_ro_store);
static ssize_t
dasd_erplog_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dasd_devmap *devmap;
	int erplog;

	devmap = dasd_find_busid(dev_name(dev));
	if (!IS_ERR(devmap))
		erplog = (devmap->features & DASD_FEATURE_ERPLOG) != 0;
	else
		erplog = (DASD_FEATURE_DEFAULT & DASD_FEATURE_ERPLOG) != 0;
	return snprintf(buf, PAGE_SIZE, erplog ? "1\n" : "0\n");
}

static ssize_t
dasd_erplog_store(struct device *dev, struct device_attribute *attr,
	      const char *buf, size_t count)
{
	struct dasd_devmap *devmap;
	int val;
	char *endp;

	devmap = dasd_devmap_from_cdev(to_ccwdev(dev));
	if (IS_ERR(devmap))
		return PTR_ERR(devmap);

	val = simple_strtoul(buf, &endp, 0);
	if (((endp + 1) < (buf + count)) || (val > 1))
		return -EINVAL;

	spin_lock(&dasd_devmap_lock);
	if (val)
		devmap->features |= DASD_FEATURE_ERPLOG;
	else
		devmap->features &= ~DASD_FEATURE_ERPLOG;
	if (devmap->device)
		devmap->device->features = devmap->features;
	spin_unlock(&dasd_devmap_lock);
	return count;
}

static DEVICE_ATTR(erplog, 0644, dasd_erplog_show, dasd_erplog_store);

static ssize_t
dasd_use_diag_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dasd_devmap *devmap;
	int use_diag;

	devmap = dasd_find_busid(dev_name(dev));
	if (!IS_ERR(devmap))
		use_diag = (devmap->features & DASD_FEATURE_USEDIAG) != 0;
	else
		use_diag = (DASD_FEATURE_DEFAULT & DASD_FEATURE_USEDIAG) != 0;
	return sprintf(buf, use_diag ? "1\n" : "0\n");
}

static ssize_t
dasd_use_diag_store(struct device *dev, struct device_attribute *attr,
		    const char *buf, size_t count)
{
	struct dasd_devmap *devmap;
	ssize_t rc;
	int val;
	char *endp;

	devmap = dasd_devmap_from_cdev(to_ccwdev(dev));
	if (IS_ERR(devmap))
		return PTR_ERR(devmap);

	val = simple_strtoul(buf, &endp, 0);
	if (((endp + 1) < (buf + count)) || (val > 1))
		return -EINVAL;

	spin_lock(&dasd_devmap_lock);
	
	rc = count;
	if (!devmap->device && !(devmap->features & DASD_FEATURE_USERAW)) {
		if (val)
			devmap->features |= DASD_FEATURE_USEDIAG;
		else
			devmap->features &= ~DASD_FEATURE_USEDIAG;
	} else
		rc = -EPERM;
	spin_unlock(&dasd_devmap_lock);
	return rc;
}

static DEVICE_ATTR(use_diag, 0644, dasd_use_diag_show, dasd_use_diag_store);

static ssize_t
dasd_use_raw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dasd_devmap *devmap;
	int use_raw;

	devmap = dasd_find_busid(dev_name(dev));
	if (!IS_ERR(devmap))
		use_raw = (devmap->features & DASD_FEATURE_USERAW) != 0;
	else
		use_raw = (DASD_FEATURE_DEFAULT & DASD_FEATURE_USERAW) != 0;
	return sprintf(buf, use_raw ? "1\n" : "0\n");
}

static ssize_t
dasd_use_raw_store(struct device *dev, struct device_attribute *attr,
		    const char *buf, size_t count)
{
	struct dasd_devmap *devmap;
	ssize_t rc;
	unsigned long val;

	devmap = dasd_devmap_from_cdev(to_ccwdev(dev));
	if (IS_ERR(devmap))
		return PTR_ERR(devmap);

	if ((strict_strtoul(buf, 10, &val) != 0) || val > 1)
		return -EINVAL;

	spin_lock(&dasd_devmap_lock);
	
	rc = count;
	if (!devmap->device && !(devmap->features & DASD_FEATURE_USEDIAG)) {
		if (val)
			devmap->features |= DASD_FEATURE_USERAW;
		else
			devmap->features &= ~DASD_FEATURE_USERAW;
	} else
		rc = -EPERM;
	spin_unlock(&dasd_devmap_lock);
	return rc;
}

static DEVICE_ATTR(raw_track_access, 0644, dasd_use_raw_show,
		   dasd_use_raw_store);

static ssize_t
dasd_discipline_show(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct dasd_device *device;
	ssize_t len;

	device = dasd_device_from_cdev(to_ccwdev(dev));
	if (IS_ERR(device))
		goto out;
	else if (!device->discipline) {
		dasd_put_device(device);
		goto out;
	} else {
		len = snprintf(buf, PAGE_SIZE, "%s\n",
			       device->discipline->name);
		dasd_put_device(device);
		return len;
	}
out:
	len = snprintf(buf, PAGE_SIZE, "none\n");
	return len;
}

static DEVICE_ATTR(discipline, 0444, dasd_discipline_show, NULL);

static ssize_t
dasd_device_status_show(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct dasd_device *device;
	ssize_t len;

	device = dasd_device_from_cdev(to_ccwdev(dev));
	if (!IS_ERR(device)) {
		switch (device->state) {
		case DASD_STATE_NEW:
			len = snprintf(buf, PAGE_SIZE, "new\n");
			break;
		case DASD_STATE_KNOWN:
			len = snprintf(buf, PAGE_SIZE, "detected\n");
			break;
		case DASD_STATE_BASIC:
			len = snprintf(buf, PAGE_SIZE, "basic\n");
			break;
		case DASD_STATE_UNFMT:
			len = snprintf(buf, PAGE_SIZE, "unformatted\n");
			break;
		case DASD_STATE_READY:
			len = snprintf(buf, PAGE_SIZE, "ready\n");
			break;
		case DASD_STATE_ONLINE:
			len = snprintf(buf, PAGE_SIZE, "online\n");
			break;
		default:
			len = snprintf(buf, PAGE_SIZE, "no stat\n");
			break;
		}
		dasd_put_device(device);
	} else
		len = snprintf(buf, PAGE_SIZE, "unknown\n");
	return len;
}

static DEVICE_ATTR(status, 0444, dasd_device_status_show, NULL);

static ssize_t dasd_alias_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct dasd_device *device;
	struct dasd_uid uid;

	device = dasd_device_from_cdev(to_ccwdev(dev));
	if (IS_ERR(device))
		return sprintf(buf, "0\n");

	if (device->discipline && device->discipline->get_uid &&
	    !device->discipline->get_uid(device, &uid)) {
		if (uid.type == UA_BASE_PAV_ALIAS ||
		    uid.type == UA_HYPER_PAV_ALIAS) {
			dasd_put_device(device);
			return sprintf(buf, "1\n");
		}
	}
	dasd_put_device(device);

	return sprintf(buf, "0\n");
}

static DEVICE_ATTR(alias, 0444, dasd_alias_show, NULL);

static ssize_t dasd_vendor_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dasd_device *device;
	struct dasd_uid uid;
	char *vendor;

	device = dasd_device_from_cdev(to_ccwdev(dev));
	vendor = "";
	if (IS_ERR(device))
		return snprintf(buf, PAGE_SIZE, "%s\n", vendor);

	if (device->discipline && device->discipline->get_uid &&
	    !device->discipline->get_uid(device, &uid))
			vendor = uid.vendor;

	dasd_put_device(device);

	return snprintf(buf, PAGE_SIZE, "%s\n", vendor);
}

static DEVICE_ATTR(vendor, 0444, dasd_vendor_show, NULL);

#define UID_STRLEN (  3 + 1 +  14 + 1 +\
		      4 + 1 +  2 + 1 +\
		      32 + 1)

static ssize_t
dasd_uid_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dasd_device *device;
	struct dasd_uid uid;
	char uid_string[UID_STRLEN];
	char ua_string[3];

	device = dasd_device_from_cdev(to_ccwdev(dev));
	uid_string[0] = 0;
	if (IS_ERR(device))
		return snprintf(buf, PAGE_SIZE, "%s\n", uid_string);

	if (device->discipline && device->discipline->get_uid &&
	    !device->discipline->get_uid(device, &uid)) {
		switch (uid.type) {
		case UA_BASE_DEVICE:
			snprintf(ua_string, sizeof(ua_string), "%02x",
				 uid.real_unit_addr);
			break;
		case UA_BASE_PAV_ALIAS:
			snprintf(ua_string, sizeof(ua_string), "%02x",
				 uid.base_unit_addr);
			break;
		case UA_HYPER_PAV_ALIAS:
			snprintf(ua_string, sizeof(ua_string), "xx");
			break;
		default:
			
			snprintf(ua_string, sizeof(ua_string), "%02x",
				 uid.real_unit_addr);
			break;
		}

		if (strlen(uid.vduit) > 0)
			snprintf(uid_string, sizeof(uid_string),
				 "%s.%s.%04x.%s.%s",
				 uid.vendor, uid.serial, uid.ssid, ua_string,
				 uid.vduit);
		else
			snprintf(uid_string, sizeof(uid_string),
				 "%s.%s.%04x.%s",
				 uid.vendor, uid.serial, uid.ssid, ua_string);
	}
	dasd_put_device(device);

	return snprintf(buf, PAGE_SIZE, "%s\n", uid_string);
}
static DEVICE_ATTR(uid, 0444, dasd_uid_show, NULL);

static ssize_t
dasd_eer_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dasd_devmap *devmap;
	int eer_flag;

	devmap = dasd_find_busid(dev_name(dev));
	if (!IS_ERR(devmap) && devmap->device)
		eer_flag = dasd_eer_enabled(devmap->device);
	else
		eer_flag = 0;
	return snprintf(buf, PAGE_SIZE, eer_flag ? "1\n" : "0\n");
}

static ssize_t
dasd_eer_store(struct device *dev, struct device_attribute *attr,
	       const char *buf, size_t count)
{
	struct dasd_devmap *devmap;
	int val, rc;
	char *endp;

	devmap = dasd_devmap_from_cdev(to_ccwdev(dev));
	if (IS_ERR(devmap))
		return PTR_ERR(devmap);
	if (!devmap->device)
		return -ENODEV;

	val = simple_strtoul(buf, &endp, 0);
	if (((endp + 1) < (buf + count)) || (val > 1))
		return -EINVAL;

	if (val) {
		rc = dasd_eer_enable(devmap->device);
		if (rc)
			return rc;
	} else
		dasd_eer_disable(devmap->device);
	return count;
}

static DEVICE_ATTR(eer_enabled, 0644, dasd_eer_show, dasd_eer_store);

static ssize_t
dasd_expires_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dasd_device *device;
	int len;

	device = dasd_device_from_cdev(to_ccwdev(dev));
	if (IS_ERR(device))
		return -ENODEV;
	len = snprintf(buf, PAGE_SIZE, "%lu\n", device->default_expires);
	dasd_put_device(device);
	return len;
}

static ssize_t
dasd_expires_store(struct device *dev, struct device_attribute *attr,
	       const char *buf, size_t count)
{
	struct dasd_device *device;
	unsigned long val;

	device = dasd_device_from_cdev(to_ccwdev(dev));
	if (IS_ERR(device))
		return -ENODEV;

	if ((strict_strtoul(buf, 10, &val) != 0) ||
	    (val > DASD_EXPIRES_MAX) || val == 0) {
		dasd_put_device(device);
		return -EINVAL;
	}

	if (val)
		device->default_expires = val;

	dasd_put_device(device);
	return count;
}

static DEVICE_ATTR(expires, 0644, dasd_expires_show, dasd_expires_store);

static ssize_t dasd_reservation_policy_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct dasd_devmap *devmap;
	int rc = 0;

	devmap = dasd_find_busid(dev_name(dev));
	if (IS_ERR(devmap)) {
		rc = snprintf(buf, PAGE_SIZE, "ignore\n");
	} else {
		spin_lock(&dasd_devmap_lock);
		if (devmap->features & DASD_FEATURE_FAILONSLCK)
			rc = snprintf(buf, PAGE_SIZE, "fail\n");
		else
			rc = snprintf(buf, PAGE_SIZE, "ignore\n");
		spin_unlock(&dasd_devmap_lock);
	}
	return rc;
}

static ssize_t dasd_reservation_policy_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	struct dasd_devmap *devmap;
	int rc;

	devmap = dasd_devmap_from_cdev(to_ccwdev(dev));
	if (IS_ERR(devmap))
		return PTR_ERR(devmap);
	rc = 0;
	spin_lock(&dasd_devmap_lock);
	if (sysfs_streq("ignore", buf))
		devmap->features &= ~DASD_FEATURE_FAILONSLCK;
	else if (sysfs_streq("fail", buf))
		devmap->features |= DASD_FEATURE_FAILONSLCK;
	else
		rc = -EINVAL;
	if (devmap->device)
		devmap->device->features = devmap->features;
	spin_unlock(&dasd_devmap_lock);
	if (rc)
		return rc;
	else
		return count;
}

static DEVICE_ATTR(reservation_policy, 0644,
		   dasd_reservation_policy_show, dasd_reservation_policy_store);

static ssize_t dasd_reservation_state_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct dasd_device *device;
	int rc = 0;

	device = dasd_device_from_cdev(to_ccwdev(dev));
	if (IS_ERR(device))
		return snprintf(buf, PAGE_SIZE, "none\n");

	if (test_bit(DASD_FLAG_IS_RESERVED, &device->flags))
		rc = snprintf(buf, PAGE_SIZE, "reserved\n");
	else if (test_bit(DASD_FLAG_LOCK_STOLEN, &device->flags))
		rc = snprintf(buf, PAGE_SIZE, "lost\n");
	else
		rc = snprintf(buf, PAGE_SIZE, "none\n");
	dasd_put_device(device);
	return rc;
}

static ssize_t dasd_reservation_state_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct dasd_device *device;
	int rc = 0;

	device = dasd_device_from_cdev(to_ccwdev(dev));
	if (IS_ERR(device))
		return -ENODEV;
	if (sysfs_streq("reset", buf))
		clear_bit(DASD_FLAG_LOCK_STOLEN, &device->flags);
	else
		rc = -EINVAL;
	dasd_put_device(device);

	if (rc)
		return rc;
	else
		return count;
}

static DEVICE_ATTR(last_known_reservation_state, 0644,
		   dasd_reservation_state_show, dasd_reservation_state_store);

static struct attribute * dasd_attrs[] = {
	&dev_attr_readonly.attr,
	&dev_attr_discipline.attr,
	&dev_attr_status.attr,
	&dev_attr_alias.attr,
	&dev_attr_vendor.attr,
	&dev_attr_uid.attr,
	&dev_attr_use_diag.attr,
	&dev_attr_raw_track_access.attr,
	&dev_attr_eer_enabled.attr,
	&dev_attr_erplog.attr,
	&dev_attr_failfast.attr,
	&dev_attr_expires.attr,
	&dev_attr_reservation_policy.attr,
	&dev_attr_last_known_reservation_state.attr,
	NULL,
};

static struct attribute_group dasd_attr_group = {
	.attrs = dasd_attrs,
};

int
dasd_get_feature(struct ccw_device *cdev, int feature)
{
	struct dasd_devmap *devmap;

	devmap = dasd_find_busid(dev_name(&cdev->dev));
	if (IS_ERR(devmap))
		return PTR_ERR(devmap);

	return ((devmap->features & feature) != 0);
}

int
dasd_set_feature(struct ccw_device *cdev, int feature, int flag)
{
	struct dasd_devmap *devmap;

	devmap = dasd_find_busid(dev_name(&cdev->dev));
	if (IS_ERR(devmap))
		return PTR_ERR(devmap);

	spin_lock(&dasd_devmap_lock);
	if (flag)
		devmap->features |= feature;
	else
		devmap->features &= ~feature;
	if (devmap->device)
		devmap->device->features = devmap->features;
	spin_unlock(&dasd_devmap_lock);
	return 0;
}


int
dasd_add_sysfs_files(struct ccw_device *cdev)
{
	return sysfs_create_group(&cdev->dev.kobj, &dasd_attr_group);
}

void
dasd_remove_sysfs_files(struct ccw_device *cdev)
{
	sysfs_remove_group(&cdev->dev.kobj, &dasd_attr_group);
}


int
dasd_devmap_init(void)
{
	int i;

	
	dasd_max_devindex = 0;
	for (i = 0; i < 256; i++)
		INIT_LIST_HEAD(&dasd_hashlists[i]);
	return 0;
}

void
dasd_devmap_exit(void)
{
	dasd_forget_ranges();
}
