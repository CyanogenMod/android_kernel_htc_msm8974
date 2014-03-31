#ifndef _VME_BRIDGE_H_
#define _VME_BRIDGE_H_

#define VME_CRCSR_BUF_SIZE (508*1024)
struct vme_master_resource {
	struct list_head list;
	struct vme_bridge *parent;
	spinlock_t lock;
	int locked;
	int number;
	u32 address_attr;
	u32 cycle_attr;
	u32 width_attr;
	struct resource bus_resource;
	void __iomem *kern_base;
};

struct vme_slave_resource {
	struct list_head list;
	struct vme_bridge *parent;
	struct mutex mtx;
	int locked;
	int number;
	u32 address_attr;
	u32 cycle_attr;
};

struct vme_dma_pattern {
	u32 pattern;
	u32 type;
};

struct vme_dma_pci {
	dma_addr_t address;
};

struct vme_dma_vme {
	unsigned long long address;
	u32 aspace;
	u32 cycle;
	u32 dwidth;
};

struct vme_dma_list {
	struct list_head list;
	struct vme_dma_resource *parent;
	struct list_head entries;
	struct mutex mtx;
};

struct vme_dma_resource {
	struct list_head list;
	struct vme_bridge *parent;
	struct mutex mtx;
	int locked;
	int number;
	struct list_head pending;
	struct list_head running;
	u32 route_attr;
};

struct vme_lm_resource {
	struct list_head list;
	struct vme_bridge *parent;
	struct mutex mtx;
	int locked;
	int number;
	int monitors;
};

struct vme_bus_error {
	struct list_head list;
	unsigned long long address;
	u32 attributes;
};

struct vme_callback {
	void (*func)(int, int, void*);
	void *priv_data;
};

struct vme_irq {
	int count;
	struct vme_callback callback[255];
};

#define VMENAMSIZ 16

struct vme_bridge {
	char name[VMENAMSIZ];
	int num;
	struct list_head master_resources;
	struct list_head slave_resources;
	struct list_head dma_resources;
	struct list_head lm_resources;

	struct list_head vme_errors;	
	struct list_head devices;	

	
	struct device *parent;	
	void *driver_priv;	
	struct list_head bus_list; 

	
	struct vme_irq irq[7];
	
	struct mutex irq_mtx;

	
	int (*slave_get) (struct vme_slave_resource *, int *,
		unsigned long long *, unsigned long long *, dma_addr_t *,
		u32 *, u32 *);
	int (*slave_set) (struct vme_slave_resource *, int, unsigned long long,
		unsigned long long, dma_addr_t, u32, u32);

	
	int (*master_get) (struct vme_master_resource *, int *,
		unsigned long long *, unsigned long long *, u32 *, u32 *,
		u32 *);
	int (*master_set) (struct vme_master_resource *, int,
		unsigned long long, unsigned long long,  u32, u32, u32);
	ssize_t (*master_read) (struct vme_master_resource *, void *, size_t,
		loff_t);
	ssize_t (*master_write) (struct vme_master_resource *, void *, size_t,
		loff_t);
	unsigned int (*master_rmw) (struct vme_master_resource *, unsigned int,
		unsigned int, unsigned int, loff_t);

	
	int (*dma_list_add) (struct vme_dma_list *, struct vme_dma_attr *,
		struct vme_dma_attr *, size_t);
	int (*dma_list_exec) (struct vme_dma_list *);
	int (*dma_list_empty) (struct vme_dma_list *);

	
	void (*irq_set) (struct vme_bridge *, int, int, int);
	int (*irq_generate) (struct vme_bridge *, int, int);

	
	int (*lm_set) (struct vme_lm_resource *, unsigned long long, u32, u32);
	int (*lm_get) (struct vme_lm_resource *, unsigned long long *, u32 *,
		u32 *);
	int (*lm_attach) (struct vme_lm_resource *, int, void (*callback)(int));
	int (*lm_detach) (struct vme_lm_resource *, int);

	
	int (*slot_get) (struct vme_bridge *);

	
	void *(*alloc_consistent)(struct device *dev, size_t size,
		dma_addr_t *dma);
	void (*free_consistent)(struct device *dev, size_t size,
		void *vaddr, dma_addr_t dma);
};

void vme_irq_handler(struct vme_bridge *, int, int);

int vme_register_bridge(struct vme_bridge *);
void vme_unregister_bridge(struct vme_bridge *);

#endif 
