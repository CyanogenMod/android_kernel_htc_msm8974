
/*
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * Copyright (c) 2009-2010, The Linux Foundation.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _DRM_P_H_
#define _DRM_P_H_

#ifdef __KERNEL__
#ifdef __alpha__
#include <asm/current.h>
#endif				
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/file.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/jiffies.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#if defined(__alpha__) || defined(__powerpc__)
#include <asm/pgtable.h>	
#endif
#include <asm/io.h>
#include <asm/mman.h>
#include <asm/uaccess.h>
#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif
#if defined(CONFIG_AGP) || defined(CONFIG_AGP_MODULE)
#include <linux/types.h>
#include <linux/agp_backend.h>
#endif
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <asm/pgalloc.h>
#include "drm.h"

#include <linux/idr.h>

#define __OS_HAS_AGP (defined(CONFIG_AGP) || (defined(CONFIG_AGP_MODULE) && defined(MODULE)))
#define __OS_HAS_MTRR (defined(CONFIG_MTRR))

struct module;

struct drm_file;
struct drm_device;

#include "drm_os_linux.h"
#include "drm_hashtab.h"
#include "drm_mm.h"

#define DRM_UT_CORE 		0x01
#define DRM_UT_DRIVER		0x02
#define DRM_UT_KMS		0x04
#define DRM_UT_PRIME		0x08

extern __printf(4, 5)
void drm_ut_debug_printk(unsigned int request_level,
			 const char *prefix,
			 const char *function_name,
			 const char *format, ...);
extern __printf(2, 3)
int drm_err(const char *func, const char *format, ...);


#define DRIVER_USE_AGP     0x1
#define DRIVER_REQUIRE_AGP 0x2
#define DRIVER_USE_MTRR    0x4
#define DRIVER_PCI_DMA     0x8
#define DRIVER_SG          0x10
#define DRIVER_HAVE_DMA    0x20
#define DRIVER_HAVE_IRQ    0x40
#define DRIVER_IRQ_SHARED  0x80
#define DRIVER_IRQ_VBL     0x100
#define DRIVER_DMA_QUEUE   0x200
#define DRIVER_FB_DMA      0x400
#define DRIVER_IRQ_VBL2    0x800
#define DRIVER_GEM         0x1000
#define DRIVER_MODESET     0x2000
#define DRIVER_PRIME       0x4000

#define DRIVER_BUS_PCI 0x1
#define DRIVER_BUS_PLATFORM 0x2
#define DRIVER_BUS_USB 0x3


#define DRM_DEBUG_CODE 2	  

#define DRM_MAGIC_HASH_ORDER  4  
#define DRM_KERNEL_CONTEXT    0	 
#define DRM_RESERVED_CONTEXTS 1	 
#define DRM_LOOPING_LIMIT     5000000
#define DRM_TIME_SLICE	      (HZ/20)  
#define DRM_LOCK_SLICE	      1	

#define DRM_FLAG_DEBUG	  0x01

#define DRM_MAX_CTXBITMAP (PAGE_SIZE * 8)
#define DRM_MAP_HASH_OFFSET 0x10000000



#define DRM_ERROR(fmt, ...)				\
	drm_err(__func__, fmt, ##__VA_ARGS__)

#define DRM_INFO(fmt, ...)				\
	printk(KERN_INFO "[" DRM_NAME "] " fmt, ##__VA_ARGS__)

#if DRM_DEBUG_CODE
#define DRM_DEBUG(fmt, args...)						\
	do {								\
		drm_ut_debug_printk(DRM_UT_CORE, DRM_NAME, 		\
					__func__, fmt, ##args);		\
	} while (0)

#define DRM_DEBUG_DRIVER(fmt, args...)					\
	do {								\
		drm_ut_debug_printk(DRM_UT_DRIVER, DRM_NAME,		\
					__func__, fmt, ##args);		\
	} while (0)
#define DRM_DEBUG_KMS(fmt, args...)				\
	do {								\
		drm_ut_debug_printk(DRM_UT_KMS, DRM_NAME, 		\
					 __func__, fmt, ##args);	\
	} while (0)
#define DRM_DEBUG_PRIME(fmt, args...)					\
	do {								\
		drm_ut_debug_printk(DRM_UT_PRIME, DRM_NAME,		\
					__func__, fmt, ##args);		\
	} while (0)
#define DRM_LOG(fmt, args...)						\
	do {								\
		drm_ut_debug_printk(DRM_UT_CORE, NULL,			\
					NULL, fmt, ##args);		\
	} while (0)
#define DRM_LOG_KMS(fmt, args...)					\
	do {								\
		drm_ut_debug_printk(DRM_UT_KMS, NULL,			\
					NULL, fmt, ##args);		\
	} while (0)
#define DRM_LOG_MODE(fmt, args...)					\
	do {								\
		drm_ut_debug_printk(DRM_UT_MODE, NULL,			\
					NULL, fmt, ##args);		\
	} while (0)
#define DRM_LOG_DRIVER(fmt, args...)					\
	do {								\
		drm_ut_debug_printk(DRM_UT_DRIVER, NULL,		\
					NULL, fmt, ##args);		\
	} while (0)
#else
#define DRM_DEBUG_DRIVER(fmt, args...) do { } while (0)
#define DRM_DEBUG_KMS(fmt, args...)	do { } while (0)
#define DRM_DEBUG_PRIME(fmt, args...)	do { } while (0)
#define DRM_DEBUG(fmt, arg...)		 do { } while (0)
#define DRM_LOG(fmt, arg...)		do { } while (0)
#define DRM_LOG_KMS(fmt, args...) do { } while (0)
#define DRM_LOG_MODE(fmt, arg...) do { } while (0)
#define DRM_LOG_DRIVER(fmt, arg...) do { } while (0)

#endif



#define DRM_ARRAY_SIZE(x) ARRAY_SIZE(x)

#define DRM_LEFTCOUNT(x) (((x)->rp + (x)->count - (x)->wp) % ((x)->count + 1))
#define DRM_BUFCOUNT(x) ((x)->count - DRM_LEFTCOUNT(x))

#define DRM_IF_VERSION(maj, min) (maj << 16 | min)

#define LOCK_TEST_WITH_RETURN( dev, _file_priv )				\
do {										\
	if (!_DRM_LOCK_IS_HELD(_file_priv->master->lock.hw_lock->lock) ||	\
	    _file_priv->master->lock.file_priv != _file_priv)	{		\
		DRM_ERROR( "%s called without lock held, held  %d owner %p %p\n",\
			   __func__, _DRM_LOCK_IS_HELD(_file_priv->master->lock.hw_lock->lock),\
			   _file_priv->master->lock.file_priv, _file_priv);	\
		return -EINVAL;							\
	}									\
} while (0)

typedef int drm_ioctl_t(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

typedef int drm_ioctl_compat_t(struct file *filp, unsigned int cmd,
			       unsigned long arg);

#define DRM_IOCTL_NR(n)                _IOC_NR(n)
#define DRM_MAJOR       226

#define DRM_AUTH	0x1
#define	DRM_MASTER	0x2
#define DRM_ROOT_ONLY	0x4
#define DRM_CONTROL_ALLOW 0x8
#define DRM_UNLOCKED	0x10

struct drm_ioctl_desc {
	unsigned int cmd;
	int flags;
	drm_ioctl_t *func;
	unsigned int cmd_drv;
};


#define DRM_IOCTL_DEF_DRV(ioctl, _func, _flags)			\
	[DRM_IOCTL_NR(DRM_##ioctl)] = {.cmd = DRM_##ioctl, .func = _func, .flags = _flags, .cmd_drv = DRM_IOCTL_##ioctl}

struct drm_magic_entry {
	struct list_head head;
	struct drm_hash_item hash_item;
	struct drm_file *priv;
};

struct drm_vma_entry {
	struct list_head head;
	struct vm_area_struct *vma;
	pid_t pid;
};

struct drm_buf {
	int idx;		       
	int total;		       
	int order;		       
	int used;		       
	unsigned long offset;	       
	void *address;		       
	unsigned long bus_address;     
	struct drm_buf *next;	       
	__volatile__ int waiting;      
	__volatile__ int pending;      
	wait_queue_head_t dma_wait;    
	struct drm_file *file_priv;    
	int context;		       
	int while_locked;	       
	enum {
		DRM_LIST_NONE = 0,
		DRM_LIST_FREE = 1,
		DRM_LIST_WAIT = 2,
		DRM_LIST_PEND = 3,
		DRM_LIST_PRIO = 4,
		DRM_LIST_RECLAIM = 5
	} list;			       

	int dev_priv_size;		 
	void *dev_private;		 
};

struct drm_waitlist {
	int count;			
	struct drm_buf **bufs;		
	struct drm_buf **rp;			
	struct drm_buf **wp;			
	struct drm_buf **end;		
	spinlock_t read_lock;
	spinlock_t write_lock;
};

struct drm_freelist {
	int initialized;	       
	atomic_t count;		       
	struct drm_buf *next;	       

	wait_queue_head_t waiting;     
	int low_mark;		       
	int high_mark;		       
	atomic_t wfh;		       
	spinlock_t lock;
};

typedef struct drm_dma_handle {
	dma_addr_t busaddr;
	void *vaddr;
	size_t size;
} drm_dma_handle_t;

struct drm_buf_entry {
	int buf_size;			
	int buf_count;			
	struct drm_buf *buflist;		
	int seg_count;
	int page_order;
	struct drm_dma_handle **seglist;

	struct drm_freelist freelist;
};

struct drm_pending_event {
	struct drm_event *event;
	struct list_head link;
	struct drm_file *file_priv;
	pid_t pid; 
	void (*destroy)(struct drm_pending_event *event);
};

struct drm_prime_file_private {
	struct list_head head;
	struct mutex lock;
};

struct drm_file {
	int authenticated;
	pid_t pid;
	uid_t uid;
	drm_magic_t magic;
	unsigned long ioctl_count;
	struct list_head lhead;
	struct drm_minor *minor;
	unsigned long lock_count;

	
	struct idr object_idr;
	
	spinlock_t table_lock;

	struct file *filp;
	void *driver_priv;

	int is_master; 
	struct drm_master *master; 
	struct list_head fbs;

	wait_queue_head_t event_wait;
	struct list_head event_list;
	int event_space;

	struct drm_prime_file_private prime;
};

struct drm_queue {
	atomic_t use_count;		
	atomic_t finalization;		
	atomic_t block_count;		
	atomic_t block_read;		
	wait_queue_head_t read_queue;	
	atomic_t block_write;		
	wait_queue_head_t write_queue;	
	atomic_t total_queued;		
	atomic_t total_flushed;		
	atomic_t total_locks;		
	enum drm_ctx_flags flags;	
	struct drm_waitlist waitlist;	
	wait_queue_head_t flush_queue;	
};

struct drm_lock_data {
	struct drm_hw_lock *hw_lock;	
	
	struct drm_file *file_priv;
	wait_queue_head_t lock_queue;	
	unsigned long lock_time;	
	spinlock_t spinlock;
	uint32_t kernel_waiters;
	uint32_t user_waiters;
	int idle_has_lock;
};

struct drm_device_dma {

	struct drm_buf_entry bufs[DRM_MAX_ORDER + 1];	
	int buf_count;			
	struct drm_buf **buflist;		
	int seg_count;
	int page_count;			
	unsigned long *pagelist;	
	unsigned long byte_count;
	enum {
		_DRM_DMA_USE_AGP = 0x01,
		_DRM_DMA_USE_SG = 0x02,
		_DRM_DMA_USE_FB = 0x04,
		_DRM_DMA_USE_PCI_RO = 0x08
	} flags;

};

struct drm_agp_mem {
	unsigned long handle;		
	DRM_AGP_MEM *memory;
	unsigned long bound;		
	int pages;
	struct list_head head;
};

struct drm_agp_head {
	DRM_AGP_KERN agp_info;		
	struct list_head memory;
	unsigned long mode;		
	struct agp_bridge_data *bridge;
	int enabled;			
	int acquired;			
	unsigned long base;
	int agp_mtrr;
	int cant_use_aperture;
	unsigned long page_mask;
};

struct drm_sg_mem {
	unsigned long handle;
	void *virtual;
	int pages;
	struct page **pagelist;
	dma_addr_t *busaddr;
};

struct drm_sigdata {
	int context;
	struct drm_hw_lock *lock;
};


struct drm_local_map {
	resource_size_t offset;	 
	unsigned long size;	 
	enum drm_map_type type;	 
	enum drm_map_flags flags;	 
	void *handle;		 
				 
	int mtrr;		 
};

typedef struct drm_local_map drm_local_map_t;

struct drm_map_list {
	struct list_head head;		
	struct drm_hash_item hash;
	struct drm_local_map *map;	
	uint64_t user_token;
	struct drm_master *master;
	struct drm_mm_node *file_offset_node;	
};

struct drm_ctx_list {
	struct list_head head;		
	drm_context_t handle;		
	struct drm_file *tag;		
};

#define DRM_ATI_GART_MAIN 1
#define DRM_ATI_GART_FB   2

#define DRM_ATI_GART_PCI 1
#define DRM_ATI_GART_PCIE 2
#define DRM_ATI_GART_IGP 3

struct drm_ati_pcigart_info {
	int gart_table_location;
	int gart_reg_if;
	void *addr;
	dma_addr_t bus_addr;
	dma_addr_t table_mask;
	struct drm_dma_handle *table_handle;
	struct drm_local_map mapping;
	int table_size;
};

struct drm_gem_mm {
	struct drm_mm offset_manager;	
	struct drm_open_hash offset_hash; 
};

struct drm_gem_object {
	
	struct kref refcount;

	
	atomic_t handle_count; 

	
	struct drm_device *dev;

	
	struct file *filp;

	
	struct drm_map_list map_list;

	size_t size;

	int name;

	uint32_t read_domains;
	uint32_t write_domain;

	uint32_t pending_read_domains;
	uint32_t pending_write_domain;

	void *driver_private;

	
	struct dma_buf *export_dma_buf;

	
	struct dma_buf_attachment *import_attach;
};

#include "drm_crtc.h"

struct drm_master {

	struct kref refcount; 

	struct list_head head; 
	struct drm_minor *minor; 

	char *unique;			
	int unique_len;			
	int unique_size;		

	int blocked;			

	
	
	struct drm_open_hash magiclist;
	struct list_head magicfree;
	

	struct drm_lock_data lock;	

	void *driver_priv; 
};

#define DRM_VBLANKTIME_RBSIZE 2

#define DRM_CALLED_FROM_VBLIRQ 1
#define DRM_VBLANKTIME_SCANOUTPOS_METHOD (1 << 0)
#define DRM_VBLANKTIME_INVBL             (1 << 1)

#define DRM_SCANOUTPOS_VALID        (1 << 0)
#define DRM_SCANOUTPOS_INVBL        (1 << 1)
#define DRM_SCANOUTPOS_ACCURATE     (1 << 2)

struct drm_bus {
	int bus_type;
	int (*get_irq)(struct drm_device *dev);
	const char *(*get_name)(struct drm_device *dev);
	int (*set_busid)(struct drm_device *dev, struct drm_master *master);
	int (*set_unique)(struct drm_device *dev, struct drm_master *master,
			  struct drm_unique *unique);
	int (*irq_by_busid)(struct drm_device *dev, struct drm_irq_busid *p);
	
	int (*agp_init)(struct drm_device *dev);

};

struct drm_driver {
	int (*load) (struct drm_device *, unsigned long flags);
	int (*firstopen) (struct drm_device *);
	int (*open) (struct drm_device *, struct drm_file *);
	void (*preclose) (struct drm_device *, struct drm_file *file_priv);
	void (*postclose) (struct drm_device *, struct drm_file *);
	void (*lastclose) (struct drm_device *);
	int (*unload) (struct drm_device *);
	int (*suspend) (struct drm_device *, pm_message_t state);
	int (*resume) (struct drm_device *);
	int (*dma_ioctl) (struct drm_device *dev, void *data, struct drm_file *file_priv);
	int (*dma_quiescent) (struct drm_device *);
	int (*context_dtor) (struct drm_device *dev, int context);

	u32 (*get_vblank_counter) (struct drm_device *dev, int crtc);

	int (*enable_vblank) (struct drm_device *dev, int crtc);

	void (*disable_vblank) (struct drm_device *dev, int crtc);

	int (*device_is_agp) (struct drm_device *dev);

	int (*get_scanout_position) (struct drm_device *dev, int crtc,
				     int *vpos, int *hpos);

	int (*get_vblank_timestamp) (struct drm_device *dev, int crtc,
				     int *max_error,
				     struct timeval *vblank_time,
				     unsigned flags);

	

	irqreturn_t(*irq_handler) (DRM_IRQ_ARGS);
	void (*irq_preinstall) (struct drm_device *dev);
	int (*irq_postinstall) (struct drm_device *dev);
	void (*irq_uninstall) (struct drm_device *dev);
	void (*reclaim_buffers) (struct drm_device *dev,
				 struct drm_file * file_priv);
	void (*reclaim_buffers_locked) (struct drm_device *dev,
					struct drm_file *file_priv);
	void (*reclaim_buffers_idlelocked) (struct drm_device *dev,
					    struct drm_file *file_priv);
	void (*set_version) (struct drm_device *dev,
			     struct drm_set_version *sv);

	
	int (*master_create)(struct drm_device *dev, struct drm_master *master);
	void (*master_destroy)(struct drm_device *dev, struct drm_master *master);

	int (*master_set)(struct drm_device *dev, struct drm_file *file_priv,
			  bool from_open);
	void (*master_drop)(struct drm_device *dev, struct drm_file *file_priv,
			    bool from_release);

	int (*debugfs_init)(struct drm_minor *minor);
	void (*debugfs_cleanup)(struct drm_minor *minor);

	int (*gem_init_object) (struct drm_gem_object *obj);
	void (*gem_free_object) (struct drm_gem_object *obj);
	int (*gem_open_object) (struct drm_gem_object *, struct drm_file *);
	void (*gem_close_object) (struct drm_gem_object *, struct drm_file *);

	
	
	int (*prime_handle_to_fd)(struct drm_device *dev, struct drm_file *file_priv,
				uint32_t handle, uint32_t flags, int *prime_fd);
	
	int (*prime_fd_to_handle)(struct drm_device *dev, struct drm_file *file_priv,
				int prime_fd, uint32_t *handle);
	
	struct dma_buf * (*gem_prime_export)(struct drm_device *dev,
				struct drm_gem_object *obj, int flags);
	
	struct drm_gem_object * (*gem_prime_import)(struct drm_device *dev,
				struct dma_buf *dma_buf);

	
	void (*vgaarb_irq)(struct drm_device *dev, bool state);

	
	int (*dumb_create)(struct drm_file *file_priv,
			   struct drm_device *dev,
			   struct drm_mode_create_dumb *args);
	int (*dumb_map_offset)(struct drm_file *file_priv,
			       struct drm_device *dev, uint32_t handle,
			       uint64_t *offset);
	int (*dumb_destroy)(struct drm_file *file_priv,
			    struct drm_device *dev,
			    uint32_t handle);

	
	struct vm_operations_struct *gem_vm_ops;

	int major;
	int minor;
	int patchlevel;
	char *name;
	char *desc;
	char *date;

	u32 driver_features;
	int dev_priv_size;
	struct drm_ioctl_desc *ioctls;
	int num_ioctls;
	const struct file_operations *fops;
	union {
		struct pci_driver *pci;
		struct platform_device *platform_device;
		struct usb_driver *usb;
	} kdriver;
	struct drm_bus *bus;

	
	struct list_head device_list;
};

#define DRM_MINOR_UNASSIGNED 0
#define DRM_MINOR_LEGACY 1
#define DRM_MINOR_CONTROL 2
#define DRM_MINOR_RENDER 3


struct drm_debugfs_list {
	const char *name; 
	int (*show)(struct seq_file*, void*); 
	u32 driver_features; 
};

struct drm_debugfs_node {
	struct list_head list;
	struct drm_minor *minor;
	struct drm_debugfs_list *debugfs_ent;
	struct dentry *dent;
};

struct drm_info_list {
	const char *name; 
	int (*show)(struct seq_file*, void*); 
	u32 driver_features; 
	void *data;
};

struct drm_info_node {
	struct list_head list;
	struct drm_minor *minor;
	struct drm_info_list *info_ent;
	struct dentry *dent;
};

struct drm_minor {
	int index;			
	int type;                       
	dev_t device;			
	struct device kdev;		
	struct drm_device *dev;

	struct proc_dir_entry *proc_root;  
	struct drm_info_node proc_nodes;
	struct dentry *debugfs_root;

	struct list_head debugfs_list;
	struct mutex debugfs_lock; 

	struct drm_master *master; 
	struct list_head master_list;
	struct drm_mode_group mode_group;
};

struct drm_cmdline_mode {
	bool specified;
	bool refresh_specified;
	bool bpp_specified;
	int xres, yres;
	int bpp;
	int refresh;
	bool rb;
	bool interlace;
	bool cvt;
	bool margins;
	enum drm_connector_force force;
};


struct drm_pending_vblank_event {
	struct drm_pending_event base;
	int pipe;
	struct drm_event_vblank event;
};

struct drm_device {
	struct list_head driver_item;	
	char *devname;			
	int if_version;			

	
	
	spinlock_t count_lock;		
	struct mutex struct_mutex;	
	

	
	
	int open_count;			
	atomic_t ioctl_count;		
	atomic_t vma_count;		
	int buf_use;			
	atomic_t buf_alloc;		
	

	
	
	unsigned long counters;
	enum drm_stat_type types[15];
	atomic_t counts[15];
	

	struct list_head filelist;

	
	
	struct list_head maplist;	
	int map_count;			
	struct drm_open_hash map_hash;	

	
	
	struct list_head ctxlist;	
	int ctx_count;			
	struct mutex ctxlist_mutex;	

	struct idr ctx_idr;

	struct list_head vmalist;	

	

	
	
	int queue_count;		
	int queue_reserved;		  
	int queue_slots;		
	struct drm_queue **queuelist;	
	struct drm_device_dma *dma;		
	

	
	
	int irq_enabled;		
	__volatile__ long context_flag;	
	__volatile__ long interrupt_flag; 
	__volatile__ long dma_flag;	
	wait_queue_head_t context_wait;	
	int last_checked;		
	int last_context;		
	unsigned long last_switch;	
	

	struct work_struct work;
	
	

	int vblank_disable_allowed;

	wait_queue_head_t *vbl_queue;   
	atomic_t *_vblank_count;        
	struct timeval *_vblank_time;   
	spinlock_t vblank_time_lock;    
	spinlock_t vbl_lock;
	atomic_t *vblank_refcount;      
	u32 *last_vblank;               
					
	int *vblank_enabled;            
	int *vblank_inmodeset;          
	u32 *last_vblank_wait;		
	struct timer_list vblank_disable_timer;

	u32 max_vblank_count;           

	struct list_head vblank_event_list;
	spinlock_t event_lock;

	
	cycles_t ctx_start;
	cycles_t lck_start;

	struct fasync_struct *buf_async;
	wait_queue_head_t buf_readers;	
	wait_queue_head_t buf_writers;	

	struct drm_agp_head *agp;	

	struct device *dev;             
	struct pci_dev *pdev;		
	int pci_vendor;			
	int pci_device;			
#ifdef __alpha__
	struct pci_controller *hose;
#endif

	struct platform_device *platformdev; 
	struct usb_device *usbdev;

	struct drm_sg_mem *sg;	
	unsigned int num_crtcs;                  
	void *dev_private;		
	void *mm_private;
	struct address_space *dev_mapping;
	struct drm_sigdata sigdata;	   
	sigset_t sigmask;

	struct drm_driver *driver;
	struct drm_local_map *agp_buffer_map;
	unsigned int agp_buffer_token;
	struct drm_minor *control;		
	struct drm_minor *primary;		

        struct drm_mode_config mode_config;	

	
	
	spinlock_t object_name_lock;
	struct idr object_name_idr;
	
	int switch_power_state;

	atomic_t unplugged; 
};

#define DRM_SWITCH_POWER_ON 0
#define DRM_SWITCH_POWER_OFF 1
#define DRM_SWITCH_POWER_CHANGING 2

static __inline__ int drm_core_check_feature(struct drm_device *dev,
					     int feature)
{
	return ((dev->driver->driver_features & feature) ? 1 : 0);
}

static inline int drm_dev_to_irq(struct drm_device *dev)
{
	return dev->driver->bus->get_irq(dev);
}


#if __OS_HAS_AGP
static inline int drm_core_has_AGP(struct drm_device *dev)
{
	return drm_core_check_feature(dev, DRIVER_USE_AGP);
}
#else
#define drm_core_has_AGP(dev) (0)
#endif

#if __OS_HAS_MTRR
static inline int drm_core_has_MTRR(struct drm_device *dev)
{
	return drm_core_check_feature(dev, DRIVER_USE_MTRR);
}

#define DRM_MTRR_WC		MTRR_TYPE_WRCOMB

static inline int drm_mtrr_add(unsigned long offset, unsigned long size,
			       unsigned int flags)
{
	return mtrr_add(offset, size, flags, 1);
}

static inline int drm_mtrr_del(int handle, unsigned long offset,
			       unsigned long size, unsigned int flags)
{
	return mtrr_del(handle, offset, size);
}

#else
#define drm_core_has_MTRR(dev) (0)

#define DRM_MTRR_WC		0

static inline int drm_mtrr_add(unsigned long offset, unsigned long size,
			       unsigned int flags)
{
	return 0;
}

static inline int drm_mtrr_del(int handle, unsigned long offset,
			       unsigned long size, unsigned int flags)
{
	return 0;
}
#endif

static inline void drm_device_set_unplugged(struct drm_device *dev)
{
	smp_wmb();
	atomic_set(&dev->unplugged, 1);
}

static inline int drm_device_is_unplugged(struct drm_device *dev)
{
	int ret = atomic_read(&dev->unplugged);
	smp_rmb();
	return ret;
}


				
extern long drm_ioctl(struct file *filp,
		      unsigned int cmd, unsigned long arg);
extern long drm_compat_ioctl(struct file *filp,
			     unsigned int cmd, unsigned long arg);
extern int drm_lastclose(struct drm_device *dev);

				
extern struct mutex drm_global_mutex;
extern int drm_open(struct inode *inode, struct file *filp);
extern int drm_stub_open(struct inode *inode, struct file *filp);
extern int drm_fasync(int fd, struct file *filp, int on);
extern ssize_t drm_read(struct file *filp, char __user *buffer,
			size_t count, loff_t *offset);
extern int drm_release(struct inode *inode, struct file *filp);

				
extern int drm_mmap(struct file *filp, struct vm_area_struct *vma);
extern int drm_mmap_locked(struct file *filp, struct vm_area_struct *vma);
extern void drm_vm_open_locked(struct vm_area_struct *vma);
extern void drm_vm_close_locked(struct vm_area_struct *vma);
extern unsigned int drm_poll(struct file *filp, struct poll_table_struct *wait);

				
#include "drm_memory.h"
extern void drm_free_agp(DRM_AGP_MEM * handle, int pages);
extern int drm_bind_agp(DRM_AGP_MEM * handle, unsigned int start);
extern DRM_AGP_MEM *drm_agp_bind_pages(struct drm_device *dev,
				       struct page **pages,
				       unsigned long num_pages,
				       uint32_t gtt_offset,
				       uint32_t type);
extern int drm_unbind_agp(DRM_AGP_MEM * handle);

				
extern int drm_irq_by_busid(struct drm_device *dev, void *data,
			    struct drm_file *file_priv);
extern int drm_getunique(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
extern int drm_setunique(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
extern int drm_getmap(struct drm_device *dev, void *data,
		      struct drm_file *file_priv);
extern int drm_getclient(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
extern int drm_getstats(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
extern int drm_getcap(struct drm_device *dev, void *data,
		      struct drm_file *file_priv);
extern int drm_setversion(struct drm_device *dev, void *data,
			  struct drm_file *file_priv);
extern int drm_noop(struct drm_device *dev, void *data,
		    struct drm_file *file_priv);

				
extern int drm_resctx(struct drm_device *dev, void *data,
		      struct drm_file *file_priv);
extern int drm_addctx(struct drm_device *dev, void *data,
		      struct drm_file *file_priv);
extern int drm_modctx(struct drm_device *dev, void *data,
		      struct drm_file *file_priv);
extern int drm_getctx(struct drm_device *dev, void *data,
		      struct drm_file *file_priv);
extern int drm_switchctx(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
extern int drm_newctx(struct drm_device *dev, void *data,
		      struct drm_file *file_priv);
extern int drm_rmctx(struct drm_device *dev, void *data,
		     struct drm_file *file_priv);

extern int drm_ctxbitmap_init(struct drm_device *dev);
extern void drm_ctxbitmap_cleanup(struct drm_device *dev);
extern void drm_ctxbitmap_free(struct drm_device *dev, int ctx_handle);

extern int drm_setsareactx(struct drm_device *dev, void *data,
			   struct drm_file *file_priv);
extern int drm_getsareactx(struct drm_device *dev, void *data,
			   struct drm_file *file_priv);

				
extern int drm_getmagic(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
extern int drm_authmagic(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
extern int drm_remove_magic(struct drm_master *master, drm_magic_t magic);

void drm_clflush_pages(struct page *pages[], unsigned long num_pages);

				
extern int drm_lock(struct drm_device *dev, void *data,
		    struct drm_file *file_priv);
extern int drm_unlock(struct drm_device *dev, void *data,
		      struct drm_file *file_priv);
extern int drm_lock_free(struct drm_lock_data *lock_data, unsigned int context);
extern void drm_idlelock_take(struct drm_lock_data *lock_data);
extern void drm_idlelock_release(struct drm_lock_data *lock_data);


extern int drm_i_have_hw_lock(struct drm_device *dev, struct drm_file *file_priv);

				
extern int drm_addbufs_agp(struct drm_device *dev, struct drm_buf_desc * request);
extern int drm_addbufs_pci(struct drm_device *dev, struct drm_buf_desc * request);
extern int drm_addmap(struct drm_device *dev, resource_size_t offset,
		      unsigned int size, enum drm_map_type type,
		      enum drm_map_flags flags, struct drm_local_map **map_ptr);
extern int drm_addmap_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *file_priv);
extern int drm_rmmap(struct drm_device *dev, struct drm_local_map *map);
extern int drm_rmmap_locked(struct drm_device *dev, struct drm_local_map *map);
extern int drm_rmmap_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv);
extern int drm_addbufs(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
extern int drm_infobufs(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
extern int drm_markbufs(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
extern int drm_freebufs(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
extern int drm_mapbufs(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
extern int drm_order(unsigned long size);

				
extern int drm_dma_setup(struct drm_device *dev);
extern void drm_dma_takedown(struct drm_device *dev);
extern void drm_free_buffer(struct drm_device *dev, struct drm_buf * buf);
extern void drm_core_reclaim_buffers(struct drm_device *dev,
				     struct drm_file *filp);

				
extern int drm_control(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
extern int drm_irq_install(struct drm_device *dev);
extern int drm_irq_uninstall(struct drm_device *dev);

extern int drm_vblank_init(struct drm_device *dev, int num_crtcs);
extern int drm_wait_vblank(struct drm_device *dev, void *data,
			   struct drm_file *filp);
extern int drm_vblank_wait(struct drm_device *dev, unsigned int *vbl_seq);
extern u32 drm_vblank_count(struct drm_device *dev, int crtc);
extern u32 drm_vblank_count_and_time(struct drm_device *dev, int crtc,
				     struct timeval *vblanktime);
extern bool drm_handle_vblank(struct drm_device *dev, int crtc);
extern int drm_vblank_get(struct drm_device *dev, int crtc);
extern void drm_vblank_put(struct drm_device *dev, int crtc);
extern void drm_vblank_off(struct drm_device *dev, int crtc);
extern void drm_vblank_cleanup(struct drm_device *dev);
extern u32 drm_get_last_vbltimestamp(struct drm_device *dev, int crtc,
				     struct timeval *tvblank, unsigned flags);
extern int drm_calc_vbltimestamp_from_scanoutpos(struct drm_device *dev,
						 int crtc, int *max_error,
						 struct timeval *vblank_time,
						 unsigned flags,
						 struct drm_crtc *refcrtc);
extern void drm_calc_timestamping_constants(struct drm_crtc *crtc);

extern bool
drm_mode_parse_command_line_for_connector(const char *mode_option,
					  struct drm_connector *connector,
					  struct drm_cmdline_mode *mode);

extern struct drm_display_mode *
drm_mode_create_from_cmdline_mode(struct drm_device *dev,
				  struct drm_cmdline_mode *cmd);

extern void drm_vblank_pre_modeset(struct drm_device *dev, int crtc);
extern void drm_vblank_post_modeset(struct drm_device *dev, int crtc);
extern int drm_modeset_ctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv);

				
extern struct drm_agp_head *drm_agp_init(struct drm_device *dev);
extern int drm_agp_acquire(struct drm_device *dev);
extern int drm_agp_acquire_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
extern int drm_agp_release(struct drm_device *dev);
extern int drm_agp_release_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
extern int drm_agp_enable(struct drm_device *dev, struct drm_agp_mode mode);
extern int drm_agp_enable_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
extern int drm_agp_info(struct drm_device *dev, struct drm_agp_info *info);
extern int drm_agp_info_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
extern int drm_agp_alloc(struct drm_device *dev, struct drm_agp_buffer *request);
extern int drm_agp_alloc_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
extern int drm_agp_free(struct drm_device *dev, struct drm_agp_buffer *request);
extern int drm_agp_free_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
extern int drm_agp_unbind(struct drm_device *dev, struct drm_agp_binding *request);
extern int drm_agp_unbind_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv);
extern int drm_agp_bind(struct drm_device *dev, struct drm_agp_binding *request);
extern int drm_agp_bind_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

				
extern int drm_setmaster_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv);
extern int drm_dropmaster_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
struct drm_master *drm_master_create(struct drm_minor *minor);
extern struct drm_master *drm_master_get(struct drm_master *master);
extern void drm_master_put(struct drm_master **master);

extern void drm_put_dev(struct drm_device *dev);
extern int drm_put_minor(struct drm_minor **minor);
extern void drm_unplug_dev(struct drm_device *dev);
extern unsigned int drm_debug;

extern unsigned int drm_vblank_offdelay;
extern unsigned int drm_timestamp_precision;

extern struct class *drm_class;
extern struct proc_dir_entry *drm_proc_root;
extern struct dentry *drm_debugfs_root;

extern struct idr drm_minors_idr;

extern struct drm_local_map *drm_getsarea(struct drm_device *dev);

				
extern int drm_proc_init(struct drm_minor *minor, int minor_id,
			 struct proc_dir_entry *root);
extern int drm_proc_cleanup(struct drm_minor *minor, struct proc_dir_entry *root);

				
#if defined(CONFIG_DEBUG_FS)
extern int drm_debugfs_init(struct drm_minor *minor, int minor_id,
			    struct dentry *root);
extern int drm_debugfs_create_files(struct drm_info_list *files, int count,
				    struct dentry *root, struct drm_minor *minor);
extern int drm_debugfs_remove_files(struct drm_info_list *files, int count,
                                    struct drm_minor *minor);
extern int drm_debugfs_cleanup(struct drm_minor *minor);
#endif

				
extern int drm_name_info(struct seq_file *m, void *data);
extern int drm_vm_info(struct seq_file *m, void *data);
extern int drm_queues_info(struct seq_file *m, void *data);
extern int drm_bufs_info(struct seq_file *m, void *data);
extern int drm_vblank_info(struct seq_file *m, void *data);
extern int drm_clients_info(struct seq_file *m, void* data);
extern int drm_gem_name_info(struct seq_file *m, void *data);


extern int drm_gem_prime_handle_to_fd(struct drm_device *dev,
		struct drm_file *file_priv, uint32_t handle, uint32_t flags,
		int *prime_fd);
extern int drm_gem_prime_fd_to_handle(struct drm_device *dev,
		struct drm_file *file_priv, int prime_fd, uint32_t *handle);

extern int drm_prime_handle_to_fd_ioctl(struct drm_device *dev, void *data,
					struct drm_file *file_priv);
extern int drm_prime_fd_to_handle_ioctl(struct drm_device *dev, void *data,
					struct drm_file *file_priv);

extern struct sg_table *drm_prime_pages_to_sg(struct page **pages, int nr_pages);
extern void drm_prime_gem_destroy(struct drm_gem_object *obj, struct sg_table *sg);


void drm_prime_init_file_private(struct drm_prime_file_private *prime_fpriv);
void drm_prime_destroy_file_private(struct drm_prime_file_private *prime_fpriv);
int drm_prime_add_imported_buf_handle(struct drm_prime_file_private *prime_fpriv, struct dma_buf *dma_buf, uint32_t handle);
int drm_prime_lookup_imported_buf_handle(struct drm_prime_file_private *prime_fpriv, struct dma_buf *dma_buf, uint32_t *handle);
void drm_prime_remove_imported_buf_handle(struct drm_prime_file_private *prime_fpriv, struct dma_buf *dma_buf);

int drm_prime_add_dma_buf(struct drm_device *dev, struct drm_gem_object *obj);
int drm_prime_lookup_obj(struct drm_device *dev, struct dma_buf *buf,
			 struct drm_gem_object **obj);

#if DRM_DEBUG_CODE
extern int drm_vma_info(struct seq_file *m, void *data);
#endif

				
extern void drm_sg_cleanup(struct drm_sg_mem * entry);
extern int drm_sg_alloc_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
extern int drm_sg_alloc(struct drm_device *dev, struct drm_scatter_gather * request);
extern int drm_sg_free(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

			       
extern int drm_ati_pcigart_init(struct drm_device *dev,
				struct drm_ati_pcigart_info * gart_info);
extern int drm_ati_pcigart_cleanup(struct drm_device *dev,
				   struct drm_ati_pcigart_info * gart_info);

extern drm_dma_handle_t *drm_pci_alloc(struct drm_device *dev, size_t size,
				       size_t align);
extern void __drm_pci_free(struct drm_device *dev, drm_dma_handle_t * dmah);
extern void drm_pci_free(struct drm_device *dev, drm_dma_handle_t * dmah);

			       
struct drm_sysfs_class;
extern struct class *drm_sysfs_create(struct module *owner, char *name);
extern void drm_sysfs_destroy(void);
extern int drm_sysfs_device_add(struct drm_minor *minor);
extern void drm_sysfs_hotplug_event(struct drm_device *dev);
extern void drm_sysfs_device_remove(struct drm_minor *minor);
extern char *drm_get_connector_status_name(enum drm_connector_status status);
extern int drm_sysfs_connector_add(struct drm_connector *connector);
extern void drm_sysfs_connector_remove(struct drm_connector *connector);

int drm_gem_init(struct drm_device *dev);
void drm_gem_destroy(struct drm_device *dev);
void drm_gem_object_release(struct drm_gem_object *obj);
void drm_gem_object_free(struct kref *kref);
struct drm_gem_object *drm_gem_object_alloc(struct drm_device *dev,
					    size_t size);
int drm_gem_object_init(struct drm_device *dev,
			struct drm_gem_object *obj, size_t size);
int drm_gem_private_object_init(struct drm_device *dev,
			struct drm_gem_object *obj, size_t size);
void drm_gem_object_handle_free(struct drm_gem_object *obj);
void drm_gem_vm_open(struct vm_area_struct *vma);
void drm_gem_vm_close(struct vm_area_struct *vma);
int drm_gem_mmap(struct file *filp, struct vm_area_struct *vma);

#include "drm_global.h"

static inline void
drm_gem_object_reference(struct drm_gem_object *obj)
{
	kref_get(&obj->refcount);
}

static inline void
drm_gem_object_unreference(struct drm_gem_object *obj)
{
	if (obj != NULL)
		kref_put(&obj->refcount, drm_gem_object_free);
}

static inline void
drm_gem_object_unreference_unlocked(struct drm_gem_object *obj)
{
	if (obj != NULL) {
		struct drm_device *dev = obj->dev;
		mutex_lock(&dev->struct_mutex);
		kref_put(&obj->refcount, drm_gem_object_free);
		mutex_unlock(&dev->struct_mutex);
	}
}

int drm_gem_handle_create(struct drm_file *file_priv,
			  struct drm_gem_object *obj,
			  u32 *handlep);
int drm_gem_handle_delete(struct drm_file *filp, u32 handle);

static inline void
drm_gem_object_handle_reference(struct drm_gem_object *obj)
{
	drm_gem_object_reference(obj);
	atomic_inc(&obj->handle_count);
}

static inline void
drm_gem_object_handle_unreference(struct drm_gem_object *obj)
{
	if (obj == NULL)
		return;

	if (atomic_read(&obj->handle_count) == 0)
		return;
	if (atomic_dec_and_test(&obj->handle_count))
		drm_gem_object_handle_free(obj);
	drm_gem_object_unreference(obj);
}

static inline void
drm_gem_object_handle_unreference_unlocked(struct drm_gem_object *obj)
{
	if (obj == NULL)
		return;

	if (atomic_read(&obj->handle_count) == 0)
		return;


	if (atomic_dec_and_test(&obj->handle_count))
		drm_gem_object_handle_free(obj);
	drm_gem_object_unreference_unlocked(obj);
}

void drm_gem_free_mmap_offset(struct drm_gem_object *obj);
int drm_gem_create_mmap_offset(struct drm_gem_object *obj);

struct drm_gem_object *drm_gem_object_lookup(struct drm_device *dev,
					     struct drm_file *filp,
					     u32 handle);
int drm_gem_close_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int drm_gem_flink_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int drm_gem_open_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
void drm_gem_open(struct drm_device *dev, struct drm_file *file_private);
void drm_gem_release(struct drm_device *dev, struct drm_file *file_private);

extern void drm_core_ioremap(struct drm_local_map *map, struct drm_device *dev);
extern void drm_core_ioremap_wc(struct drm_local_map *map, struct drm_device *dev);
extern void drm_core_ioremapfree(struct drm_local_map *map, struct drm_device *dev);

static __inline__ struct drm_local_map *drm_core_findmap(struct drm_device *dev,
							 unsigned int token)
{
	struct drm_map_list *_entry;
	list_for_each_entry(_entry, &dev->maplist, head)
	    if (_entry->user_token == token)
		return _entry->map;
	return NULL;
}

static __inline__ void drm_core_dropmap(struct drm_local_map *map)
{
}

#include "drm_mem_util.h"

extern int drm_fill_in_dev(struct drm_device *dev,
			   const struct pci_device_id *ent,
			   struct drm_driver *driver);
int drm_get_minor(struct drm_device *dev, struct drm_minor **minor, int type);

static __inline__ int drm_pci_device_is_agp(struct drm_device *dev)
{
	if (dev->driver->device_is_agp != NULL) {
		int err = (*dev->driver->device_is_agp) (dev);

		if (err != 2) {
			return err;
		}
	}

	return pci_find_capability(dev->pdev, PCI_CAP_ID_AGP);
}

extern int drm_pci_init(struct drm_driver *driver, struct pci_driver *pdriver);
extern void drm_pci_exit(struct drm_driver *driver, struct pci_driver *pdriver);
extern int drm_get_pci_dev(struct pci_dev *pdev,
			   const struct pci_device_id *ent,
			   struct drm_driver *driver);


extern int drm_platform_init(struct drm_driver *driver, struct platform_device *platform_device);
extern void drm_platform_exit(struct drm_driver *driver, struct platform_device *platform_device);

extern int drm_get_platform_dev(struct platform_device *pdev,
				struct drm_driver *driver);

static __inline__ bool drm_can_sleep(void)
{
	if (in_atomic() || in_dbg_master() || irqs_disabled())
		return false;
	return true;
}

#endif				
#endif
