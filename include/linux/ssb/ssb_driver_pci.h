#ifndef LINUX_SSB_PCICORE_H_
#define LINUX_SSB_PCICORE_H_

#include <linux/types.h>

struct pci_dev;


#ifdef CONFIG_SSB_DRIVER_PCICORE

#define SSB_PCICORE_CTL			0x0000	
#define  SSB_PCICORE_CTL_RST_OE		0x00000001 
#define  SSB_PCICORE_CTL_RST		0x00000002 
#define  SSB_PCICORE_CTL_CLK_OE		0x00000004 
#define  SSB_PCICORE_CTL_CLK		0x00000008 
#define SSB_PCICORE_ARBCTL		0x0010	
#define  SSB_PCICORE_ARBCTL_INTERN	0x00000001 
#define  SSB_PCICORE_ARBCTL_EXTERN	0x00000002 
#define  SSB_PCICORE_ARBCTL_PARKID	0x00000006 
#define   SSB_PCICORE_ARBCTL_PARKID_LAST	0x00000000 
#define   SSB_PCICORE_ARBCTL_PARKID_4710	0x00000002 
#define   SSB_PCICORE_ARBCTL_PARKID_EXT0	0x00000004 
#define   SSB_PCICORE_ARBCTL_PARKID_EXT1	0x00000006 
#define SSB_PCICORE_ISTAT		0x0020	
#define  SSB_PCICORE_ISTAT_INTA		0x00000001 
#define  SSB_PCICORE_ISTAT_INTB		0x00000002 
#define  SSB_PCICORE_ISTAT_SERR		0x00000004 
#define  SSB_PCICORE_ISTAT_PERR		0x00000008 
#define  SSB_PCICORE_ISTAT_PME		0x00000010 
#define SSB_PCICORE_IMASK		0x0024	
#define  SSB_PCICORE_IMASK_INTA		0x00000001 
#define  SSB_PCICORE_IMASK_INTB		0x00000002 
#define  SSB_PCICORE_IMASK_SERR		0x00000004 
#define  SSB_PCICORE_IMASK_PERR		0x00000008 
#define  SSB_PCICORE_IMASK_PME		0x00000010 
#define SSB_PCICORE_MBOX		0x0028	
#define  SSB_PCICORE_MBOX_F0_0		0x00000100 
#define  SSB_PCICORE_MBOX_F0_1		0x00000200 
#define  SSB_PCICORE_MBOX_F1_0		0x00000400 
#define  SSB_PCICORE_MBOX_F1_1		0x00000800 
#define  SSB_PCICORE_MBOX_F2_0		0x00001000 
#define  SSB_PCICORE_MBOX_F2_1		0x00002000 
#define  SSB_PCICORE_MBOX_F3_0		0x00004000 
#define  SSB_PCICORE_MBOX_F3_1		0x00008000 
#define SSB_PCICORE_BCAST_ADDR		0x0050	
#define  SSB_PCICORE_BCAST_ADDR_MASK	0x000000FF
#define SSB_PCICORE_BCAST_DATA		0x0054	
#define SSB_PCICORE_GPIO_IN		0x0060	
#define SSB_PCICORE_GPIO_OUT		0x0064	
#define SSB_PCICORE_GPIO_ENABLE		0x0068	
#define SSB_PCICORE_GPIO_CTL		0x006C	
#define SSB_PCICORE_SBTOPCI0		0x0100	
#define  SSB_PCICORE_SBTOPCI0_MASK	0xFC000000
#define SSB_PCICORE_SBTOPCI1		0x0104	
#define  SSB_PCICORE_SBTOPCI1_MASK	0xFC000000
#define SSB_PCICORE_SBTOPCI2		0x0108	
#define  SSB_PCICORE_SBTOPCI2_MASK	0xC0000000
#define SSB_PCICORE_PCICFG0		0x0400	
#define SSB_PCICORE_PCICFG1		0x0500	
#define SSB_PCICORE_PCICFG2		0x0600	
#define SSB_PCICORE_PCICFG3		0x0700	
#define SSB_PCICORE_SPROM(wordoffset)	(0x0800 + ((wordoffset) * 2)) 

#define SSB_PCICORE_SBTOPCI_MEM		0x00000000
#define SSB_PCICORE_SBTOPCI_IO		0x00000001
#define SSB_PCICORE_SBTOPCI_CFG0	0x00000002
#define SSB_PCICORE_SBTOPCI_CFG1	0x00000003
#define SSB_PCICORE_SBTOPCI_PREF	0x00000004 
#define SSB_PCICORE_SBTOPCI_BURST	0x00000008 
#define SSB_PCICORE_SBTOPCI_MRM		0x00000020 
#define SSB_PCICORE_SBTOPCI_RC		0x00000030 
#define  SSB_PCICORE_SBTOPCI_RC_READ	0x00000000 
#define  SSB_PCICORE_SBTOPCI_RC_READL	0x00000010 
#define  SSB_PCICORE_SBTOPCI_RC_READM	0x00000020 


#define SSB_PCICORE_BFL_NOPCI		0x00000400 


struct ssb_pcicore {
	struct ssb_device *dev;
	u8 setup_done:1;
	u8 hostmode:1;
	u8 cardbusmode:1;
};

extern void ssb_pcicore_init(struct ssb_pcicore *pc);

extern int ssb_pcicore_dev_irqvecs_enable(struct ssb_pcicore *pc,
					  struct ssb_device *dev);

int ssb_pcicore_plat_dev_init(struct pci_dev *d);
int ssb_pcicore_pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin);


#else 


struct ssb_pcicore {
};

static inline
void ssb_pcicore_init(struct ssb_pcicore *pc)
{
}

static inline
int ssb_pcicore_dev_irqvecs_enable(struct ssb_pcicore *pc,
				   struct ssb_device *dev)
{
	return 0;
}

static inline
int ssb_pcicore_plat_dev_init(struct pci_dev *d)
{
	return -ENODEV;
}
static inline
int ssb_pcicore_pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	return -ENODEV;
}

#endif 
#endif 
