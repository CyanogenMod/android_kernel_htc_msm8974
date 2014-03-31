#ifndef LINUX_PCI_ATS_H
#define LINUX_PCI_ATS_H

#include <linux/pci.h>

struct pci_ats {
	int pos;        
	int stu;        
	int qdep;       
	int ref_cnt;    
	unsigned int is_enabled:1;      
};

#ifdef CONFIG_PCI_ATS

extern int pci_enable_ats(struct pci_dev *dev, int ps);
extern void pci_disable_ats(struct pci_dev *dev);
extern int pci_ats_queue_depth(struct pci_dev *dev);

static inline int pci_ats_enabled(struct pci_dev *dev)
{
	return dev->ats && dev->ats->is_enabled;
}

#else 

static inline int pci_enable_ats(struct pci_dev *dev, int ps)
{
	return -ENODEV;
}

static inline void pci_disable_ats(struct pci_dev *dev)
{
}

static inline int pci_ats_queue_depth(struct pci_dev *dev)
{
	return -ENODEV;
}

static inline int pci_ats_enabled(struct pci_dev *dev)
{
	return 0;
}

#endif 

#ifdef CONFIG_PCI_PRI

extern int  pci_enable_pri(struct pci_dev *pdev, u32 reqs);
extern void pci_disable_pri(struct pci_dev *pdev);
extern bool pci_pri_enabled(struct pci_dev *pdev);
extern int  pci_reset_pri(struct pci_dev *pdev);
extern bool pci_pri_stopped(struct pci_dev *pdev);
extern int  pci_pri_status(struct pci_dev *pdev);

#else 

static inline int pci_enable_pri(struct pci_dev *pdev, u32 reqs)
{
	return -ENODEV;
}

static inline void pci_disable_pri(struct pci_dev *pdev)
{
}

static inline bool pci_pri_enabled(struct pci_dev *pdev)
{
	return false;
}

static inline int pci_reset_pri(struct pci_dev *pdev)
{
	return -ENODEV;
}

static inline bool pci_pri_stopped(struct pci_dev *pdev)
{
	return true;
}

static inline int pci_pri_status(struct pci_dev *pdev)
{
	return -ENODEV;
}
#endif 

#ifdef CONFIG_PCI_PASID

extern int pci_enable_pasid(struct pci_dev *pdev, int features);
extern void pci_disable_pasid(struct pci_dev *pdev);
extern int pci_pasid_features(struct pci_dev *pdev);
extern int pci_max_pasids(struct pci_dev *pdev);

#else  

static inline int pci_enable_pasid(struct pci_dev *pdev, int features)
{
	return -EINVAL;
}

static inline void pci_disable_pasid(struct pci_dev *pdev)
{
}

static inline int pci_pasid_features(struct pci_dev *pdev)
{
	return -EINVAL;
}

static inline int pci_max_pasids(struct pci_dev *pdev)
{
	return -EINVAL;
}

#endif 


#endif 
