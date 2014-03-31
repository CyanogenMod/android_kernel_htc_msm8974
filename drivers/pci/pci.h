#ifndef DRIVERS_PCI_H
#define DRIVERS_PCI_H

#include <linux/workqueue.h>

#define PCI_CFG_SPACE_SIZE	256
#define PCI_CFG_SPACE_EXP_SIZE	4096


extern int pci_uevent(struct device *dev, struct kobj_uevent_env *env);
extern int pci_create_sysfs_dev_files(struct pci_dev *pdev);
extern void pci_remove_sysfs_dev_files(struct pci_dev *pdev);
#if !defined(CONFIG_DMI) && !defined(CONFIG_ACPI)
static inline void pci_create_firmware_label_files(struct pci_dev *pdev)
{ return; }
static inline void pci_remove_firmware_label_files(struct pci_dev *pdev)
{ return; }
#else
extern void pci_create_firmware_label_files(struct pci_dev *pdev);
extern void pci_remove_firmware_label_files(struct pci_dev *pdev);
#endif
extern void pci_cleanup_rom(struct pci_dev *dev);
#ifdef HAVE_PCI_MMAP
enum pci_mmap_api {
	PCI_MMAP_SYSFS,	
	PCI_MMAP_PROCFS	
};
extern int pci_mmap_fits(struct pci_dev *pdev, int resno,
			 struct vm_area_struct *vmai,
			 enum pci_mmap_api mmap_api);
#endif
int pci_probe_reset_function(struct pci_dev *dev);

struct pci_platform_pm_ops {
	bool (*is_manageable)(struct pci_dev *dev);
	int (*set_state)(struct pci_dev *dev, pci_power_t state);
	pci_power_t (*choose_state)(struct pci_dev *dev);
	bool (*can_wakeup)(struct pci_dev *dev);
	int (*sleep_wake)(struct pci_dev *dev, bool enable);
	int (*run_wake)(struct pci_dev *dev, bool enable);
};

extern int pci_set_platform_pm(struct pci_platform_pm_ops *ops);
extern void pci_update_current_state(struct pci_dev *dev, pci_power_t state);
extern void pci_disable_enabled_device(struct pci_dev *dev);
extern int pci_finish_runtime_suspend(struct pci_dev *dev);
extern int __pci_pme_wakeup(struct pci_dev *dev, void *ign);
extern void pci_pm_init(struct pci_dev *dev);
extern void platform_pci_wakeup_init(struct pci_dev *dev);
extern void pci_allocate_cap_save_buffers(struct pci_dev *dev);
void pci_free_cap_save_buffers(struct pci_dev *dev);

static inline void pci_wakeup_event(struct pci_dev *dev)
{
	
	pm_wakeup_event(&dev->dev, 100);
}

static inline bool pci_is_bridge(struct pci_dev *pci_dev)
{
	return !!(pci_dev->subordinate);
}

extern int pci_user_read_config_byte(struct pci_dev *dev, int where, u8 *val);
extern int pci_user_read_config_word(struct pci_dev *dev, int where, u16 *val);
extern int pci_user_read_config_dword(struct pci_dev *dev, int where, u32 *val);
extern int pci_user_write_config_byte(struct pci_dev *dev, int where, u8 val);
extern int pci_user_write_config_word(struct pci_dev *dev, int where, u16 val);
extern int pci_user_write_config_dword(struct pci_dev *dev, int where, u32 val);

struct pci_vpd_ops {
	ssize_t (*read)(struct pci_dev *dev, loff_t pos, size_t count, void *buf);
	ssize_t (*write)(struct pci_dev *dev, loff_t pos, size_t count, const void *buf);
	void (*release)(struct pci_dev *dev);
};

struct pci_vpd {
	unsigned int len;
	const struct pci_vpd_ops *ops;
	struct bin_attribute *attr; 
};

extern int pci_vpd_pci22_init(struct pci_dev *dev);
static inline void pci_vpd_release(struct pci_dev *dev)
{
	if (dev->vpd)
		dev->vpd->ops->release(dev);
}

#ifdef CONFIG_PROC_FS
extern int pci_proc_attach_device(struct pci_dev *dev);
extern int pci_proc_detach_device(struct pci_dev *dev);
extern int pci_proc_detach_bus(struct pci_bus *bus);
#else
static inline int pci_proc_attach_device(struct pci_dev *dev) { return 0; }
static inline int pci_proc_detach_device(struct pci_dev *dev) { return 0; }
static inline int pci_proc_detach_bus(struct pci_bus *bus) { return 0; }
#endif

extern unsigned int pci_do_scan_bus(struct pci_bus *bus);

#ifdef HAVE_PCI_LEGACY
extern void pci_create_legacy_files(struct pci_bus *bus);
extern void pci_remove_legacy_files(struct pci_bus *bus);
#else
static inline void pci_create_legacy_files(struct pci_bus *bus) { return; }
static inline void pci_remove_legacy_files(struct pci_bus *bus) { return; }
#endif

extern struct rw_semaphore pci_bus_sem;

extern raw_spinlock_t pci_lock;

extern unsigned int pci_pm_d3_delay;

#ifdef CONFIG_PCI_MSI
void pci_no_msi(void);
extern void pci_msi_init_pci_dev(struct pci_dev *dev);
#else
static inline void pci_no_msi(void) { }
static inline void pci_msi_init_pci_dev(struct pci_dev *dev) { }
#endif

void pci_realloc_get_opt(char *);

static inline int pci_no_d1d2(struct pci_dev *dev)
{
	unsigned int parent_dstates = 0;

	if (dev->bus->self)
		parent_dstates = dev->bus->self->no_d1d2;
	return (dev->no_d1d2 || parent_dstates);

}
extern struct device_attribute pci_dev_attrs[];
extern struct device_attribute pcibus_dev_attrs[];
#ifdef CONFIG_HOTPLUG
extern struct bus_attribute pci_bus_attrs[];
#else
#define pci_bus_attrs	NULL
#endif


static inline const struct pci_device_id *
pci_match_one_device(const struct pci_device_id *id, const struct pci_dev *dev)
{
	if ((id->vendor == PCI_ANY_ID || id->vendor == dev->vendor) &&
	    (id->device == PCI_ANY_ID || id->device == dev->device) &&
	    (id->subvendor == PCI_ANY_ID || id->subvendor == dev->subsystem_vendor) &&
	    (id->subdevice == PCI_ANY_ID || id->subdevice == dev->subsystem_device) &&
	    !((id->class ^ dev->class) & id->class_mask))
		return id;
	return NULL;
}

#define to_pci_slot(s) container_of(s, struct pci_slot, kobj)

extern struct kset *pci_slots_kset;

struct pci_slot_attribute {
	struct attribute attr;
	ssize_t (*show)(struct pci_slot *, char *);
	ssize_t (*store)(struct pci_slot *, const char *, size_t);
};
#define to_pci_slot_attr(s) container_of(s, struct pci_slot_attribute, attr)

enum pci_bar_type {
	pci_bar_unknown,	
	pci_bar_io,		
	pci_bar_mem32,		
	pci_bar_mem64,		
};

bool pci_bus_read_dev_vendor_id(struct pci_bus *bus, int devfn, u32 *pl,
				int crs_timeout);
extern int pci_setup_device(struct pci_dev *dev);
extern int __pci_read_base(struct pci_dev *dev, enum pci_bar_type type,
				struct resource *res, unsigned int reg);
extern int pci_resource_bar(struct pci_dev *dev, int resno,
			    enum pci_bar_type *type);
extern int pci_bus_add_child(struct pci_bus *bus);
extern void pci_enable_ari(struct pci_dev *dev);
static inline int pci_ari_enabled(struct pci_bus *bus)
{
	return bus->self && bus->self->ari_enabled;
}

void pci_reassigndev_resource_alignment(struct pci_dev *dev);
extern void pci_disable_bridge_window(struct pci_dev *dev);

struct pci_sriov {
	int pos;		
	int nres;		
	u32 cap;		
	u16 ctrl;		
	u16 total;		
	u16 initial;		
	u16 nr_virtfn;		
	u16 offset;		
	u16 stride;		
	u32 pgsz;		
	u8 link;		
	struct pci_dev *dev;	
	struct pci_dev *self;	
	struct mutex lock;	
	struct work_struct mtask; 
	u8 __iomem *mstate;	
};

#ifdef CONFIG_PCI_ATS
extern void pci_restore_ats_state(struct pci_dev *dev);
#else
static inline void pci_restore_ats_state(struct pci_dev *dev)
{
}
#endif 

#ifdef CONFIG_PCI_IOV
extern int pci_iov_init(struct pci_dev *dev);
extern void pci_iov_release(struct pci_dev *dev);
extern int pci_iov_resource_bar(struct pci_dev *dev, int resno,
				enum pci_bar_type *type);
extern resource_size_t pci_sriov_resource_alignment(struct pci_dev *dev,
						    int resno);
extern void pci_restore_iov_state(struct pci_dev *dev);
extern int pci_iov_bus_range(struct pci_bus *bus);

#else
static inline int pci_iov_init(struct pci_dev *dev)
{
	return -ENODEV;
}
static inline void pci_iov_release(struct pci_dev *dev)

{
}
static inline int pci_iov_resource_bar(struct pci_dev *dev, int resno,
				       enum pci_bar_type *type)
{
	return 0;
}
static inline void pci_restore_iov_state(struct pci_dev *dev)
{
}
static inline int pci_iov_bus_range(struct pci_bus *bus)
{
	return 0;
}

#endif 

extern unsigned long pci_cardbus_resource_alignment(struct resource *);

static inline resource_size_t pci_resource_alignment(struct pci_dev *dev,
					 struct resource *res)
{
#ifdef CONFIG_PCI_IOV
	int resno = res - dev->resource;

	if (resno >= PCI_IOV_RESOURCES && resno <= PCI_IOV_RESOURCE_END)
		return pci_sriov_resource_alignment(dev, resno);
#endif
	if (dev->class >> 8  == PCI_CLASS_BRIDGE_CARDBUS)
		return pci_cardbus_resource_alignment(res);
	return resource_alignment(res);
}

extern void pci_enable_acs(struct pci_dev *dev);

struct pci_dev_reset_methods {
	u16 vendor;
	u16 device;
	int (*reset)(struct pci_dev *dev, int probe);
};

#ifdef CONFIG_PCI_QUIRKS
extern int pci_dev_specific_reset(struct pci_dev *dev, int probe);
#else
static inline int pci_dev_specific_reset(struct pci_dev *dev, int probe)
{
	return -ENOTTY;
}
#endif

#endif 
