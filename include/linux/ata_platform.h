#ifndef __LINUX_ATA_PLATFORM_H
#define __LINUX_ATA_PLATFORM_H

struct pata_platform_info {
	unsigned int ioport_shift;
	unsigned int irq_flags;
};

extern int __devinit __pata_platform_probe(struct device *dev,
					   struct resource *io_res,
					   struct resource *ctl_res,
					   struct resource *irq_res,
					   unsigned int ioport_shift,
					   int __pio_mask);

extern int __devexit __pata_platform_remove(struct device *dev);

struct mv_sata_platform_data {
	int	n_ports; 
};

#endif 
