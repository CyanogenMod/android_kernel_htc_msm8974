#ifndef LINUX_MMC_SDHCI_PCI_DATA_H
#define LINUX_MMC_SDHCI_PCI_DATA_H

struct pci_dev;

struct sdhci_pci_data {
	struct pci_dev	*pdev;
	int		slotno;
	int		rst_n_gpio; 
	int		cd_gpio;    
	int		(*setup)(struct sdhci_pci_data *data);
	void		(*cleanup)(struct sdhci_pci_data *data);
};

extern struct sdhci_pci_data *(*sdhci_pci_get_data)(struct pci_dev *pdev,
				int slotno);

#endif
