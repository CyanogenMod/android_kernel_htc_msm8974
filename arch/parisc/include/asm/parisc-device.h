#ifndef _ASM_PARISC_PARISC_DEVICE_H_
#define _ASM_PARISC_PARISC_DEVICE_H_

#include <linux/device.h>

struct parisc_device {
	struct resource hpa;		
	struct parisc_device_id id;
	struct parisc_driver *driver;	
	char		name[80];	
	int		irq;
	int		aux_irq;	

	char		hw_path;        
	unsigned int	num_addrs;	
	unsigned long	*addr;          
 
#ifdef CONFIG_64BIT
	
	unsigned long	pcell_loc;	
	unsigned long	mod_index;	

	
	unsigned long	mod_info;	
	unsigned long	pmod_loc;	
#endif
	u64		dma_mask;	
	struct device 	dev;
};

struct parisc_driver {
	struct parisc_driver *next;
	char *name; 
	const struct parisc_device_id *id_table;
	int (*probe) (struct parisc_device *dev); 
	int (*remove) (struct parisc_device *dev);
	struct device_driver drv;
};


#define to_parisc_device(d)	container_of(d, struct parisc_device, dev)
#define to_parisc_driver(d)	container_of(d, struct parisc_driver, drv)
#define parisc_parent(d)	to_parisc_device(d->dev.parent)

static inline const char *parisc_pathname(struct parisc_device *d)
{
	return dev_name(&d->dev);
}

static inline void
parisc_set_drvdata(struct parisc_device *d, void *p)
{
	dev_set_drvdata(&d->dev, p);
}

static inline void *
parisc_get_drvdata(struct parisc_device *d)
{
	return dev_get_drvdata(&d->dev);
}

extern struct bus_type parisc_bus_type;

#endif 
