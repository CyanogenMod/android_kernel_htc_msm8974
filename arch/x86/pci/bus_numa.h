#ifndef __BUS_NUMA_H
#define __BUS_NUMA_H
#define RES_NUM 16
struct pci_root_info {
	char name[12];
	unsigned int res_num;
	struct resource res[RES_NUM];
	int bus_min;
	int bus_max;
	int node;
	int link;
};

#define PCI_ROOT_NR 4
extern int pci_root_num;
extern struct pci_root_info pci_root_info[PCI_ROOT_NR];

extern void update_res(struct pci_root_info *info, resource_size_t start,
		      resource_size_t end, unsigned long flags, int merge);
#endif
